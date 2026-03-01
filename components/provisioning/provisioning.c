#include "provisioning.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PROVISIONING";
static provisioning_done_callback_t s_callback = NULL;
static httpd_handle_t s_server = NULL;
static esp_netif_t *s_ap_netif = NULL;

typedef struct {
    provisioning_done_callback_t callback;
    provisioning_data_t data;
} prov_stop_args_t;

static const char *SETUP_HTML =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><meta charset=\"UTF-8\"><title>SmartVault Setup</title>\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 20px}"
    "input{width:100%;padding:8px;margin:8px 0;box-sizing:border-box}"
    "button{width:100%;padding:10px;background:#333;color:#fff;border:none;cursor:pointer}</style>\n"
    "</head>\n"
    "<body>\n"
    "<h2>SmartVault Setup</h2>\n"
    "<form method=\"POST\" action=\"/provision\">\n"
    "<label>WiFi SSID<input name=\"ssid\" required></label>\n"
    "<label>WiFi Password<input name=\"password\" type=\"password\"></label>\n"
    "<label>Provisioning Token<input name=\"token\" required></label>\n"
    "<label>API URL (optional)<input name=\"api_url\" placeholder=\"http://...\"></label>\n"
    "<button type=\"submit\">Provision</button>\n"
    "</form>\n"
    "</body></html>\n";

static int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void url_decode(const char *src, char *dst, size_t dst_len)
{
    size_t si = 0;
    size_t di = 0;

    if (dst_len == 0) {
        return;
    }

    while (src[si] != '\0' && di < dst_len - 1) {
        if (src[si] == '+') {
            dst[di++] = ' ';
            si++;
        } else if (src[si] == '%' && src[si + 1] != '\0' && src[si + 2] != '\0') {
            int hi = hex_char_to_int(src[si + 1]);
            int lo = hex_char_to_int(src[si + 2]);
            if (hi >= 0 && lo >= 0) {
                dst[di++] = (char)((hi << 4) | lo);
                si += 3;
            } else {
                dst[di++] = src[si++];
            }
        } else {
            dst[di++] = src[si++];
        }
    }

    dst[di] = '\0';
}

static void parse_field(const char *body, const char *key, char *out, size_t out_len)
{
    size_t key_len = strlen(key);
    const char *cursor = body;

    if (out_len == 0) {
        return;
    }

    out[0] = '\0';

    while (*cursor != '\0') {
        const char *amp = strchr(cursor, '&');
        size_t pair_len = amp ? (size_t)(amp - cursor) : strlen(cursor);

        if (pair_len > key_len + 1 && strncmp(cursor, key, key_len) == 0 && cursor[key_len] == '=') {
            const char *value_start = cursor + key_len + 1;
            size_t value_len = pair_len - key_len - 1;
            char encoded[512];

            if (value_len >= sizeof(encoded)) {
                value_len = sizeof(encoded) - 1;
            }

            memcpy(encoded, value_start, value_len);
            encoded[value_len] = '\0';
            url_decode(encoded, out, out_len);
            return;
        }

        if (amp == NULL) {
            break;
        }
        cursor = amp + 1;
    }
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, SETUP_HTML, HTTPD_RESP_USE_STRLEN);
}

static void provisioning_stop_task(void *arg)
{
    prov_stop_args_t *args = (prov_stop_args_t *)arg;
    provisioning_stop();
    if (args->callback != NULL) {
        args->callback(&args->data);
    }
    free(args);
    vTaskDelete(NULL);
}

static esp_err_t provision_post_handler(httpd_req_t *req)
{
    char body[512];
    int total_read = 0;
    provisioning_data_t data = {0};

    if (req->content_len <= 0 || req->content_len >= (int)sizeof(body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Invalid request body", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    while (total_read < req->content_len) {
        int ret = httpd_req_recv(req, body + total_read, req->content_len - total_read);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive provisioning body");
            return ESP_FAIL;
        }
        total_read += ret;
    }
    body[total_read] = '\0';

    parse_field(body, "ssid", data.ssid, sizeof(data.ssid));
    parse_field(body, "password", data.password, sizeof(data.password));
    parse_field(body, "token", data.token, sizeof(data.token));
    parse_field(body, "api_url", data.api_url, sizeof(data.api_url));

    if (data.ssid[0] == '\0' || data.token[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Missing required fields: ssid and token", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h2>Provisioning complete. You may close this page.</h2>", HTTPD_RESP_USE_STRLEN);

    if (s_callback != NULL) {
        prov_stop_args_t *args = malloc(sizeof(prov_stop_args_t));
        if (args != NULL) {
            args->callback = s_callback;
            args->data = data;
            BaseType_t task_ok = xTaskCreate(provisioning_stop_task, "prov_stop", 8192, args,
                                             tskIDLE_PRIORITY + 1, NULL);
            if (task_ok != pdPASS) {
                ESP_LOGE(TAG, "Failed to schedule provisioning stop task");
                free(args);
            }
        } else {
            ESP_LOGE(TAG, "Failed to allocate provisioning stop args");
        }
    }

    return ESP_OK;
}

esp_err_t provisioning_start(provisioning_done_callback_t callback)
{
    uint8_t mac[6] = {0};
    char ap_ssid[32] = {0};
    wifi_config_t ap_config = {0};
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();

    s_callback = callback;

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t event_err = esp_event_loop_create_default();
    if (event_err != ESP_OK && event_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(event_err);
    }

    esp_err_t wifi_init_err = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (wifi_init_err != ESP_OK && wifi_init_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(wifi_init_err);
    }

    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create AP netif");
            return ESP_FAIL;
        }
    }

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(ap_ssid, sizeof(ap_ssid), "SmartVault-%02X%02X%02X", mac[3], mac[4], mac[5]);

    strncpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = (uint8_t)strlen(ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 4;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    server_config.server_port = 80;
    esp_err_t server_err = httpd_start(&s_server, &server_config);
    if (server_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(server_err));
        provisioning_stop();
        return server_err;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t provision_uri = {
        .uri = "/provision",
        .method = HTTP_POST,
        .handler = provision_post_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &root_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &provision_uri));

    ESP_LOGI(TAG, "SoftAP started: %s", ap_ssid);
    ESP_LOGI(TAG, "Captive portal ready at 192.168.4.1");
    return ESP_OK;
}

void provisioning_stop(void)
{
    if (s_server != NULL) {
        esp_err_t err = httpd_stop(s_server);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "httpd_stop failed: %s", esp_err_to_name(err));
        }
        s_server = NULL;
    }

    esp_err_t wifi_stop_err = esp_wifi_stop();
    if (wifi_stop_err != ESP_OK && wifi_stop_err != ESP_ERR_WIFI_NOT_INIT && wifi_stop_err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(wifi_stop_err));
    }

    esp_err_t wifi_deinit_err = esp_wifi_deinit();
    if (wifi_deinit_err != ESP_OK && wifi_deinit_err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "esp_wifi_deinit failed: %s", esp_err_to_name(wifi_deinit_err));
    }

    if (s_ap_netif != NULL) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }

    s_callback = NULL;
    ESP_LOGI(TAG, "Provisioning stopped");
}
