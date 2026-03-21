/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * WiFi provisioning — SoftAP + captive portal + web config page.
 * Shared across all Espressif SoCs (ESP32-C3, ESP32-S3, etc.).
 */

#ifndef CLAW_PLATFORM_ESPRESSIF_WIFI_PROVISION_H
#define CLAW_PLATFORM_ESPRESSIF_WIFI_PROVISION_H

#include "esp_err.h"
#include <stdbool.h>

/* Start AP hotspot + DNS captive portal + HTTP config server. */
esp_err_t wifi_provision_start(void);

/* Stop provisioning (AP + DNS + HTTP). */
esp_err_t wifi_provision_stop(void);

/* Check if provisioning is currently active. */
bool wifi_provision_is_active(void);

#endif /* CLAW_PLATFORM_ESPRESSIF_WIFI_PROVISION_H */
