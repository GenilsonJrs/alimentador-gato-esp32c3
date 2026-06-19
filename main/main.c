// =====================================================================
//  Alimentador de Gato - ESP32-C3 + servo SG90
//  Acionamento manual e por horario, via bot do Telegram (Wi-Fi).
// =====================================================================
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "time_sync.h"
#include "servo.h"
#include "schedule.h"
#include "telegram.h"

static const char *TAG = "app";

void app_main(void)
{
    // NVS: usada pelo Wi-Fi e para guardar os horarios.
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    servo_init();                 // comporta comeca fechada
    wifi_init_sta();              // bloqueia ate conectar
    time_sync_init();             // relogio via SNTP (fuso de Brasilia)
    time_sync_wait(15);
    schedule_init();              // carrega agenda + tarefa de disparo
    telegram_start();             // bot ouvindo comandos

    ESP_LOGI(TAG, "alimentador pronto.");
}
