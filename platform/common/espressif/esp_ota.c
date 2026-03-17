/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * OTA platform implementation for ESP-IDF (ESP32-C3, S3, …).
 * Uses esp_ota_ops for A/B partition switching.
 */

#include "osal/claw_os.h"
#include "claw_ota.h"

#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include <string.h>

#ifdef CONFIG_RTCLAW_OTA_ENABLE

#define TAG "esp_ota"
#define OTA_BUF_SIZE 4096

int claw_ota_platform_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        CLAW_LOGE(TAG, "cannot determine running partition");
        return CLAW_ERROR;
    }

    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            CLAW_LOGI(TAG, "first boot after OTA, pending verification");
        }
    }

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) {
        CLAW_LOGW(TAG, "no OTA update partition available");
        return CLAW_ERROR;
    }

    CLAW_LOGI(TAG, "running: %s, update target: %s",
              running->label, update->label);
    return CLAW_OK;
}

int claw_ota_do_update(const char *url, claw_ota_progress_cb progress)
{
    if (!url || url[0] == '\0') {
        return CLAW_ERROR;
    }

    char *buf = claw_malloc(OTA_BUF_SIZE);
    if (!buf) {
        CLAW_LOGE(TAG, "no memory for OTA buffer");
        return CLAW_ERROR;
    }

    esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        claw_free(buf);
        return CLAW_ERROR;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        CLAW_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        claw_free(buf);
        return CLAW_ERROR;
    }

    int content_length = esp_http_client_fetch_headers(client);
    uint32_t total_size = (content_length > 0) ? (uint32_t)content_length : 0;

    CLAW_LOGI(TAG, "firmware size: %lu bytes",
              (unsigned long)total_size);

    const esp_partition_t *update_part =
        esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        CLAW_LOGE(TAG, "no update partition");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        claw_free(buf);
        return CLAW_ERROR;
    }

    esp_ota_handle_t ota_handle = 0;
    err = esp_ota_begin(update_part, OTA_WITH_SEQUENTIAL_WRITES,
                        &ota_handle);
    if (err != ESP_OK) {
        CLAW_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        claw_free(buf);
        return CLAW_ERROR;
    }

    uint32_t received = 0;
    int ret = CLAW_OK;

    while (1) {
        int read_len = esp_http_client_read(client, buf, OTA_BUF_SIZE);
        if (read_len < 0) {
            CLAW_LOGE(TAG, "HTTP read error");
            ret = CLAW_ERROR;
            break;
        }
        if (read_len == 0) {
            if (esp_http_client_is_complete_data_received(client)) {
                break;
            }
            /* Server closed prematurely */
            if (total_size > 0 && received < total_size) {
                CLAW_LOGE(TAG, "incomplete download: %lu / %lu",
                          (unsigned long)received,
                          (unsigned long)total_size);
                ret = CLAW_ERROR;
            }
            break;
        }

        err = esp_ota_write(ota_handle, buf, (size_t)read_len);
        if (err != ESP_OK) {
            CLAW_LOGE(TAG, "esp_ota_write failed: %s",
                      esp_err_to_name(err));
            ret = CLAW_ERROR;
            break;
        }

        received += (uint32_t)read_len;
        if (progress) {
            progress(received, total_size);
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    claw_free(buf);

    if (ret != CLAW_OK) {
        esp_ota_abort(ota_handle);
        return CLAW_ERROR;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        CLAW_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return CLAW_ERROR;
    }

    err = esp_ota_set_boot_partition(update_part);
    if (err != ESP_OK) {
        CLAW_LOGE(TAG, "set boot partition failed: %s",
                  esp_err_to_name(err));
        return CLAW_ERROR;
    }

    CLAW_LOGI(TAG, "OTA succeeded, new boot partition: %s",
              update_part->label);
    return CLAW_OK;
}

int claw_ota_rollback(void)
{
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (err != ESP_OK) {
        CLAW_LOGE(TAG, "rollback failed: %s", esp_err_to_name(err));
        return CLAW_ERROR;
    }
    /* Does not return — device reboots */
    return CLAW_OK;
}

int claw_ota_mark_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err != ESP_OK) {
                CLAW_LOGE(TAG, "mark valid failed: %s",
                          esp_err_to_name(err));
                return CLAW_ERROR;
            }
            CLAW_LOGI(TAG, "firmware marked as valid");
        }
    }
    return CLAW_OK;
}

const char *claw_ota_running_version(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    return desc ? desc->version : "unknown";
}

#endif /* CONFIG_RTCLAW_OTA_ENABLE */
