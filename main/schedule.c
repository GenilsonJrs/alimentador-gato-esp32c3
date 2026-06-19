#include "schedule.h"
#include "app_config.h"
#include "servo.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "sched";

#define NVS_NS    "feeder"
#define NVS_KEY   "sched"

static feed_schedule_t s_list[MAX_SCHEDULES];
static int             s_count;

// ---- persistencia --------------------------------------------------
static void schedule_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open falhou ao salvar");
        return;
    }
    nvs_set_blob(h, NVS_KEY, s_list, sizeof(feed_schedule_t) * s_count);
    nvs_commit(h);
    nvs_close(h);
}

static void schedule_load(void)
{
    s_count = 0;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return;   // primeira execucao: agenda vazia
    }
    size_t size = sizeof(s_list);
    if (nvs_get_blob(h, NVS_KEY, s_list, &size) == ESP_OK) {
        s_count = size / sizeof(feed_schedule_t);
    }
    nvs_close(h);
    ESP_LOGI(TAG, "%d horario(s) carregado(s) da NVS", s_count);
}

// ---- API -----------------------------------------------------------
int schedule_count(void) { return s_count; }

bool schedule_add(int hour, int minute, int seconds)
{
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || seconds <= 0) {
        return false;
    }
    if (s_count >= MAX_SCHEDULES) {
        return false;
    }
    if (seconds > MAX_FEED_SECONDS) seconds = MAX_FEED_SECONDS;

    s_list[s_count].hour    = (uint8_t)hour;
    s_list[s_count].minute  = (uint8_t)minute;
    s_list[s_count].seconds = (uint16_t)seconds;
    s_list[s_count].enabled = 1;
    s_count++;
    schedule_save();
    return true;
}

bool schedule_remove(int idx)
{
    if (idx < 0 || idx >= s_count) {
        return false;
    }
    for (int i = idx; i < s_count - 1; i++) {
        s_list[i] = s_list[i + 1];
    }
    s_count--;
    schedule_save();
    return true;
}

int schedule_format(char *out, size_t out_size)
{
    if (s_count == 0) {
        snprintf(out, out_size, "Nenhum horario cadastrado.");
        return 0;
    }
    size_t pos = 0;
    for (int i = 0; i < s_count && pos < out_size; i++) {
        pos += snprintf(out + pos, out_size - pos, "%d) %02d:%02d - %ds\n",
                        i + 1, s_list[i].hour, s_list[i].minute, s_list[i].seconds);
    }
    return s_count;
}

// ---- tarefa que dispara nos horarios ------------------------------
static void schedule_task(void *arg)
{
    int last_key = -1;   // hora*60+min ja avaliado (evita disparo duplicado)

    while (1) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);

        // So age com hora valida (apos sincronizar via SNTP).
        if (tm.tm_year > (2021 - 1900)) {
            int key = tm.tm_hour * 60 + tm.tm_min;

            if (last_key == -1) {
                // Primeira leitura: apenas "arma" o relogio, sem disparar
                // (evita alimentar no boot se religar dentro do minuto agendado).
                last_key = key;
            } else if (key != last_key) {
                last_key = key;
                for (int i = 0; i < s_count; i++) {
                    if (s_list[i].enabled &&
                        s_list[i].hour == tm.tm_hour &&
                        s_list[i].minute == tm.tm_min) {
                        ESP_LOGI(TAG, "horario %02d:%02d -> alimentar %ds",
                                 tm.tm_hour, tm.tm_min, s_list[i].seconds);
                        servo_dispense(s_list[i].seconds);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void schedule_init(void)
{
    schedule_load();
    xTaskCreate(schedule_task, "sched", 4096, NULL, 5, NULL);
}
