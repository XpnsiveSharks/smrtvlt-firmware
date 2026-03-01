#pragma once

#include "esp_err.h"

#define NFC_UID_MAX_LEN 11  // 10 hex chars + null terminator

typedef void (*nfc_card_callback_t)(const char *uid);

esp_err_t nfc_init(void);
esp_err_t nfc_start_listener(nfc_card_callback_t callback);
void nfc_stop_listener(void);
const char *nfc_get_last_uid(void);
