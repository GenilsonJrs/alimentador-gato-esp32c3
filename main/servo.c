#include "servo.h"
#include "app_config.h"

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "servo";

// Resolucao do PWM. 14 bits -> 16384 passos no periodo de 20 ms,
// dando ~1,2 us por passo (mais que suficiente para o SG90).
#define LEDC_RES_BITS   LEDC_TIMER_14_BIT
#define LEDC_MAX_DUTY   ((1 << 14) - 1)
#define PERIOD_US       (1000000 / SERVO_FREQ_HZ)   // 20000 us a 50 Hz

static SemaphoreHandle_t s_lock;

// Converte largura de pulso (us) -> valor de duty do LEDC.
static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    return (uint32_t)(((uint64_t)pulse_us * (LEDC_MAX_DUTY + 1)) / PERIOD_US);
}

// Converte angulo (0..180) -> largura de pulso (us).
static uint32_t angle_to_pulse(int deg)
{
    if (deg < 0)   deg = 0;
    if (deg > 180) deg = 180;
    return SERVO_MIN_PULSE_US +
           ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * deg) / 180;
}

static void servo_write_duty(uint32_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void servo_set_angle(int deg)
{
    servo_write_duty(pulse_to_duty(angle_to_pulse(deg)));
}

void servo_init(void)
{
    s_lock = xSemaphoreCreateMutex();

    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = LEDC_RES_BITS,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num   = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    // Garante a comporta fechada ao ligar.
    servo_set_angle(SERVO_CLOSED_DEG);
    vTaskDelay(pdMS_TO_TICKS(500));
#if SERVO_RELAX_WHEN_IDLE
    servo_write_duty(0);
#endif
    ESP_LOGI(TAG, "servo pronto no GPIO %d (fechado=%d, aberto=%d)",
             SERVO_GPIO, SERVO_CLOSED_DEG, SERVO_OPEN_DEG);
}

bool servo_dispense(int seconds)
{
    if (seconds <= 0)                seconds = 1;
    if (seconds > MAX_FEED_SECONDS)  seconds = MAX_FEED_SECONDS;

    // Nao bloqueia: se ja estiver dispensando, recusa.
    if (xSemaphoreTake(s_lock, 0) != pdTRUE) {
        ESP_LOGW(TAG, "ja esta dispensando; comando ignorado");
        return false;
    }

    ESP_LOGI(TAG, "abrindo comporta por %d s", seconds);
    servo_set_angle(SERVO_OPEN_DEG);
    vTaskDelay(pdMS_TO_TICKS(seconds * 1000));

    servo_set_angle(SERVO_CLOSED_DEG);
    vTaskDelay(pdMS_TO_TICKS(500));   // tempo para o servo chegar na posicao
#if SERVO_RELAX_WHEN_IDLE
    servo_write_duty(0);              // corta o PWM para nao zumbir parado
#endif
    ESP_LOGI(TAG, "comporta fechada");

    xSemaphoreGive(s_lock);
    return true;
}
