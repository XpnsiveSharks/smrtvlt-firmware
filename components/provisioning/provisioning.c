#include "provisioning.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PROVISIONING";

static httpd_handle_t s_server = NULL;
static provisioning_done_callback_t s_done_callback = NULL;
static esp_netif_t *s_ap_netif = NULL;

static const char *PORTAL_HTML =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SmartVault Setup</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:20px;background:#f5f5f5;}"
    "h2{color:#333;text-align:center;}label{display:block;margin-top:12px;font-weight:bold;color:#555;}"
    "input{width:100%;padding:10px;margin-top:4px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box;font-size:14px;}"
    "button{width:100%;padding:12px;margin-top:20px;background:#e74c3c;color:#fff;border:none;border-radius:6px;font-size:16px;cursor:pointer;}"
    "button:hover{background:#c0392b;}.hint{font-size:12px;color:#888;margin-top:2px;}"
    "</style></head><body>"
    "<h2>&#128274; SmartVault Setup</h2>"
    "<form method='POST' action='/provision'>"
    "<label>WiFi Network (SSID)</label>"
    "<input type='text' name='ssid' required placeholder='Your WiFi name'/>"
    "<label>WiFi Password</label>"
    "<input type='password' name='password' required placeholder='Your WiFi password'/>"
    "<label>Provisioning Token</label>"
    "<input type='text' name='token' required placeholder='e.g. 123456AB' maxlength='8'/>"
    "<p class='hint'>8-character token from the SmartVault app (6 digits + 2 letters)</p>"
    "<label>API URL <span style='font-weight:normal;color:#888;'>(optional)</span></label>"
    "<input type='text' name='api_url' placeholder='Leave blank to use default'/>"
    "<button type='submit'>Connect &amp; Activate</button>"
    "</form></body></html>";

static const char *SUCCESS_HTML =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>SmartVault Setup</title>"
    "<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:20px;text-align:center;}"
    "h2{color:#27ae60;}.msg{color:#555;margin-top:12px;}</style></head><body>"
    "<h2>&#10003; Setup Complete!</h2>"
    "<p class='msg'>SmartVault is connecting to your network.<br>You can close this page.</p>"
    "</body></html>";

static const char *ERROR_HTML =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>SmartVault Setup</title>"
    "<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:20px;text-align:center;}"
    "h2{color:#e74c3c;}.msg{color:#555;margin-top:12px;}"
    "a{color:#e74c3c;}</style></head><body>"
    "<h2>&#10007; Invalid Token</h2>"
    "<p class='msg'>The provisioning token is invalid.<br>Please check the token in your SmartVault app.</p>"
    "<a href='/'>Try again</a></body></html>";

bool provisioning_validate_token(const char *token) {
    if (!token || strlen(token) != 8) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char) token[i])) {
            return false;
        }
    }

    for (int i = 6; i < 8; i++) {
        if (!isupper((unsigned char) token[i])) {
            return false;
        }
    }

    return true;
}

static void url_decode(char *dst, const char *src, size_t dst_len) {
    size_t i = 0;
    size_t j = 0;

    while (src[i] && j < dst_len - 1) {
        if (src[i] == '%' && isxdigit((unsigned char) src[i + 1]) && isxdigit((unsigned char) src[i + 2])) {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            dst[j++] = (char) strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }

    dst[j] = '\0';
}

static void parse_field(const char *body, const char *key, char *out, size_t out_len) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char *start = strstr(body, search);
    if (!start) {
        out[0] = '\0';
        return;
    }

    start += strlen(search);
    const char *end = strchr(start, '&');
    size_t len = end ? (size_t) (end - start) : strlen(start);
    if (len >= out_len) {
        len = out_len - 1;
    }

    char raw[256] = {0};
    memcpy(raw, start, len);
    raw[len] = '\0';
    url_decode(out, raw, out_len);
}

static esp_err_t get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req) {
    char body[512] = {0};
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    body[ret] = '\0';

    provisioning_data_t data = {0};
    parse_field(body, "ssid", data.ssid, sizeof(data.ssid));
    parse_field(body, "password", data.password, sizeof(data.password));
    parse_field(body, "token", data.token, sizeof(data.token));
    parse_field(body, "api_url", data.api_url, sizeof(data.api_url));

    ESP_LOGI(TAG, "Received: ssid=%s token=%s api_url=%s", data.ssid, data.token, data.api_url);

    if (!provisioning_validate_token(data.token)) {
        ESP_LOGW(TAG, "Invalid token: %s", data.token);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, ERROR_HTML, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, SUCCESS_HTML, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Provisioning data valid. Firing callback...");
    if (s_done_callback) {
        s_done_callback(&data);
    }

    return ESP_OK;
}

static esp_err_t redirect_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void start_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t post_uri = {
        .uri = "/provision",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t redir = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = redirect_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(s_server, &get_uri);
    httpd_register_uri_handler(s_server, &post_uri);
    httpd_register_uri_handler(s_server, &redir);

    ESP_LOGI(TAG, "HTTP server started at 192.168.4.1");
}

esp_err_t provisioning_start(provisioning_done_callback_t callback) {
    s_done_callback = callback;

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "smrtvlt-%02X%02X%02X", mac[3], mac[4], mac[5]);

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = 0,
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };
    strncpy((char *) ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started: SSID=%s", ssid);

    start_http_server();
    return ESP_OK;
}

void provisioning_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_ap_netif) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }

    ESP_LOGI(TAG, "Provisioning stopped");
}
