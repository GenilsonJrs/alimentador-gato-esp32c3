#include "wifi.h"
#include "secrets.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "wifi";

#define CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_eg;

static const char *authmode_str(wifi_auth_mode_t m)
{
    switch (m) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    case WIFI_AUTH_OWE:             return "OWE";
    default:                        return "?";
    }
}

// Faz um scan e loga RSSI/canal/seguranca dos APs cujo SSID comeca com WIFI_SSID.
// Serve para diagnostico (sinal fraco? WPA3?). Executa com o Wi-Fi ja iniciado.
static void scan_diagnostico(void)
{
    wifi_scan_config_t sc = { .show_hidden = true };
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return;
    }
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n == 0) {
        ESP_LOGW(TAG, "scan nao achou nenhum AP");
        return;
    }
    if (n > 20) n = 20;
    wifi_ap_record_t recs[20];
    esp_wifi_scan_get_ap_records(&n, recs);

    size_t pref = strlen(WIFI_SSID);
    bool achou = false;
    for (int i = 0; i < n; i++) {
        if (strncmp((char *)recs[i].ssid, WIFI_SSID, pref) == 0) {
            achou = true;
            ESP_LOGI(TAG, "AP \"%s\"  rssi=%d dBm  ch=%d  seg=%s  bssid=%02x:%02x:%02x:%02x:%02x:%02x",
                     recs[i].ssid, recs[i].rssi, recs[i].primary,
                     authmode_str(recs[i].authmode),
                     recs[i].bssid[0], recs[i].bssid[1], recs[i].bssid[2],
                     recs[i].bssid[3], recs[i].bssid[4], recs[i].bssid[5]);
        }
    }
    if (!achou) {
        ESP_LOGW(TAG, "scan: nenhum AP comecando com \"%s\" (de %d encontrados)",
                 WIFI_SSID, n);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        xEventGroupClearBits(s_wifi_eg, CONNECTED_BIT);
        ESP_LOGW(TAG, "desconectado (reason=%d, rssi=%d); reconectando...",
                 d->reason, d->rssi);
        esp_wifi_connect();   // reconecta indefinidamente (ex.: roteador reiniciou)
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "conectado, IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_eg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &on_wifi_event, NULL));

    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password) - 1);
    // Rede e WPA/WPA2 (sem WPA3): config minima, sem SAE (que atrapalhava aqui).
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Power save desligado: evita perda de beacons / quedas com alguns roteadores.
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Diagnostico de RF antes de conectar (loga sinal e seguranca da rede).
    scan_diagnostico();

    ESP_LOGI(TAG, "conectando em \"%s\"...", WIFI_SSID);
    esp_wifi_connect();
    xEventGroupWaitBits(s_wifi_eg, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
