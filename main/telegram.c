#include "telegram.h"
#include "secrets.h"
#include "app_config.h"
#include "servo.h"
#include "schedule.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "telegram";

// Tempo do long-polling no servidor do Telegram (segundos).
#define LONGPOLL_S    25

static const char *API = "https://api.telegram.org/bot" TELEGRAM_BOT_TOKEN;

static int64_t s_offset = 0;   // proximo update a buscar (update_id + 1)

// ---- buffer de resposta HTTP que cresce conforme chega -------------
typedef struct {
    char *buf;
    int   len;
    int   cap;
} http_resp_t;

static esp_err_t http_evt(esp_http_client_event_t *e)
{
    if (e->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }
    http_resp_t *r = (http_resp_t *)e->user_data;
    if (!r) {
        return ESP_OK;
    }
    if (r->len + e->data_len + 1 > r->cap) {
        int newcap = r->cap ? r->cap * 2 : 1024;
        while (newcap < r->len + e->data_len + 1) newcap *= 2;
        char *nb = realloc(r->buf, newcap);
        if (!nb) {
            return ESP_FAIL;
        }
        r->buf = nb;
        r->cap = newcap;
    }
    memcpy(r->buf + r->len, e->data, e->data_len);
    r->len += e->data_len;
    r->buf[r->len] = '\0';
    return ESP_OK;
}

// ---- envia uma mensagem de texto para um chat -----------------------
static void telegram_send(int64_t chat_id, const char *text)
{
    char url[160];
    snprintf(url, sizeof(url), "%s/sendMessage", API);

    // Monta o corpo JSON (cJSON escapa o texto corretamente).
    // chat_id vai como string: o Telegram aceita e evita perda de precisao.
    char id_str[24];
    snprintf(id_str, sizeof(id_str), "%lld", (long long)chat_id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "chat_id", id_str);
    cJSON_AddStringToObject(root, "text", text);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) {
        return;
    }

    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_post_field(cli, body, strlen(body));
    esp_err_t err = esp_http_client_perform(cli);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sendMessage falhou: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(cli);
    free(body);
}

// ---- interpretacao dos comandos ------------------------------------
static void send_help(int64_t chat_id)
{
    char msg[512];
    int n = snprintf(msg, sizeof(msg),
        "Alimentador de Gato\n\n"
        "/alimentar [seg] - serve racao agora\n"
        "/agendar HH:MM seg - cadastra horario\n"
        "/horarios - lista os horarios\n"
        "/remover N - apaga o horario N\n"
        "/status - hora do aparelho e infos\n");
#if TELEGRAM_ALLOWED_CHAT_ID == 0
    snprintf(msg + n, sizeof(msg) - n,
        "\nSeu chat_id e: %lld\n"
        "Coloque-o em secrets.h (TELEGRAM_ALLOWED_CHAT_ID) e regrave o "
        "firmware para que so voce comande o alimentador.",
        (long long)chat_id);
#else
    (void)n;
#endif
    telegram_send(chat_id, msg);
}

static void handle_command(int64_t chat_id, const char *text)
{
    // Extrai o comando (1o token), tirando a "/" e um eventual "@nomedobot".
    char cmd[32] = { 0 };
    const char *p = text;
    while (*p == ' ') p++;
    if (*p == '/') p++;
    int i = 0;
    while (*p && *p != ' ' && *p != '@' && i < (int)sizeof(cmd) - 1) {
        cmd[i++] = (char)tolower((unsigned char)*p++);
    }
    cmd[i] = '\0';
    while (*p && *p != ' ') p++;   // descarta o resto do token (ex.: @bot)
    while (*p == ' ') p++;
    const char *args = p;          // argumentos (texto apos o comando)

    if (!strcmp(cmd, "start") || !strcmp(cmd, "ajuda") || !strcmp(cmd, "help")) {
        send_help(chat_id);

    } else if (!strcmp(cmd, "alimentar") || !strcmp(cmd, "feed")) {
        int secs = DEFAULT_FEED_SECONDS;
        if (*args) {
            int v = atoi(args);
            if (v > 0) secs = v;
        }
        if (secs > MAX_FEED_SECONDS) secs = MAX_FEED_SECONDS;
        bool ok = servo_dispense(secs);
        char msg[96];
        if (ok) {
            snprintf(msg, sizeof(msg), "Servindo racao por %d s. Bom apetite!", secs);
        } else {
            snprintf(msg, sizeof(msg), "Ja estou servindo agora, aguarde terminar.");
        }
        telegram_send(chat_id, msg);

    } else if (!strcmp(cmd, "agendar")) {
        int h, m, s;
        if (sscanf(args, "%d:%d %d", &h, &m, &s) == 3 &&
            schedule_add(h, m, s)) {
            char msg[96];
            snprintf(msg, sizeof(msg),
                     "Horario cadastrado: %02d:%02d servindo %d s.", h, m, s);
            telegram_send(chat_id, msg);
        } else {
            telegram_send(chat_id,
                "Nao consegui cadastrar. Use: /agendar HH:MM SEGUNDOS\n"
                "Ex.: /agendar 08:00 3\n"
                "(verifique a hora e se a agenda nao esta cheia)");
        }

    } else if (!strcmp(cmd, "horarios") || !strcmp(cmd, "agenda")) {
        char list[512];
        schedule_format(list, sizeof(list));
        telegram_send(chat_id, list);

    } else if (!strcmp(cmd, "remover")) {
        int idx = atoi(args);   // mostrado para o usuario como base 1
        if (idx >= 1 && schedule_remove(idx - 1)) {
            telegram_send(chat_id, "Horario removido.");
        } else {
            telegram_send(chat_id, "Indice invalido. Veja /horarios.");
        }

    } else if (!strcmp(cmd, "status")) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char ts[32];
        strftime(ts, sizeof(ts), "%d/%m/%Y %H:%M:%S", &tm);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "Hora do aparelho: %s\n"
                 "Horarios cadastrados: %d\n"
                 "Porcao padrao: %d s",
                 ts, schedule_count(), DEFAULT_FEED_SECONDS);
        telegram_send(chat_id, msg);

    } else {
        telegram_send(chat_id, "Comando desconhecido. Veja /ajuda.");
    }
}

// ---- uma rodada de getUpdates (long-polling) -----------------------
static void poll_once(void)
{
    char url[256];
    snprintf(url, sizeof(url),
             "%s/getUpdates?timeout=%d&limit=1&offset=%lld",
             API, LONGPOLL_S, (long long)s_offset);

    http_resp_t resp = { 0 };
    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler     = http_evt,
        .user_data         = &resp,
        .timeout_ms        = (LONGPOLL_S + 10) * 1000,
        .buffer_size_tx    = 1024,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(cli);
    esp_http_client_cleanup(cli);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "getUpdates falhou: %s", esp_err_to_name(err));
        free(resp.buf);
        vTaskDelay(pdMS_TO_TICKS(3000));   // evita martelar em caso de erro
        return;
    }
    if (!resp.buf) {
        return;
    }

    cJSON *root = cJSON_Parse(resp.buf);
    free(resp.buf);
    if (!root) {
        return;
    }

    cJSON *ok = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (cJSON_IsTrue(ok) && cJSON_IsArray(result)) {
        cJSON *upd;
        cJSON_ArrayForEach(upd, result) {
            cJSON *uid = cJSON_GetObjectItem(upd, "update_id");
            if (cJSON_IsNumber(uid)) {
                s_offset = (int64_t)uid->valuedouble + 1;
            }
            cJSON *msg = cJSON_GetObjectItem(upd, "message");
            if (!msg) continue;
            cJSON *chat = cJSON_GetObjectItem(msg, "chat");
            cJSON *txt  = cJSON_GetObjectItem(msg, "text");
            if (!chat || !cJSON_IsString(txt)) continue;
            cJSON *cid = cJSON_GetObjectItem(chat, "id");
            if (!cJSON_IsNumber(cid)) continue;
            int64_t chat_id = (int64_t)cid->valuedouble;

            ESP_LOGI(TAG, "msg de chat %lld: %s",
                     (long long)chat_id, txt->valuestring);

            // Autorizacao: se TELEGRAM_ALLOWED_CHAT_ID != 0, so esse chat passa.
#if TELEGRAM_ALLOWED_CHAT_ID != 0
            if (chat_id != (int64_t)TELEGRAM_ALLOWED_CHAT_ID) {
                telegram_send(chat_id, "Nao autorizado.");
                continue;
            }
#endif
            handle_command(chat_id, txt->valuestring);
        }
    }
    cJSON_Delete(root);
}

static void telegram_task(void *arg)
{
    ESP_LOGI(TAG, "bot ativo, aguardando comandos...");
    while (1) {
        poll_once();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void telegram_start(void)
{
    xTaskCreate(telegram_task, "telegram", 10240, NULL, 5, NULL);
}
