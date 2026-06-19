#include "time_sync.h"
#include "app_config.h"

#include <time.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"
#include "esp_log.h"

static const char *TAG = "time";

void time_sync_init(void)
{
    setenv("TZ", TIMEZONE, 1);
    tzset();

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "a.st1.ntp.br");   // servidor publico do Brasil
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP iniciado, fuso=%s", TIMEZONE);
}

bool time_sync_wait(int timeout_s)
{
    // Antes de sincronizar, o relogio comeca em 1970. Consideramos valido
    // quando o ano passa de 2021.
    for (int i = 0; i < timeout_s * 2; i++) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        if (tm.tm_year > (2021 - 1900)) {
            char buf[32];
            strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &tm);
            ESP_LOGI(TAG, "hora sincronizada: %s", buf);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGW(TAG, "nao sincronizou a tempo; agenda pode falhar ate sincronizar");
    return false;
}
