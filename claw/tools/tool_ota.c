/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * OTA tools — AI can check, trigger, and manage firmware updates.
 */

#include "claw/tools/claw_tools.h"
#include "claw_ota.h"
#include "claw/services/ota/ota_service.h"

#include <stdio.h>

#ifdef CONFIG_RTCLAW_OTA_ENABLE

static int tool_ota_check(const cJSON *params, cJSON *result)
{
    (void)params;

    claw_ota_info_t info;
    int ret = ota_check_update(&info);

    if (ret == 1) {
        cJSON_AddStringToObject(result, "status", "update_available");
        cJSON_AddStringToObject(result, "current_version",
                                claw_ota_running_version());
        cJSON_AddStringToObject(result, "new_version", info.version);
        cJSON_AddStringToObject(result, "url", info.url);
        cJSON_AddNumberToObject(result, "size_bytes", info.size);
    } else if (ret == 0) {
        cJSON_AddStringToObject(result, "status", "up_to_date");
        cJSON_AddStringToObject(result, "version",
                                claw_ota_running_version());
    } else {
        cJSON_AddStringToObject(result, "status", "error");
        cJSON_AddStringToObject(result, "error",
                                "version check failed");
        return CLAW_ERROR;
    }
    return CLAW_OK;
}

static int tool_ota_update(const cJSON *params, cJSON *result)
{
    cJSON *url_j = cJSON_GetObjectItem(params, "url");

    if (url_j && cJSON_IsString(url_j) &&
        url_j->valuestring[0] != '\0') {
        /* Direct URL provided */
        if (ota_trigger_update(url_j->valuestring) == CLAW_OK) {
            cJSON_AddStringToObject(result, "status", "started");
            cJSON_AddStringToObject(result, "message",
                "OTA update started. Device will reboot "
                "automatically when done.");
        } else {
            cJSON_AddStringToObject(result, "status", "error");
            cJSON_AddStringToObject(result, "error",
                                    "failed to start OTA update");
            return CLAW_ERROR;
        }
        return CLAW_OK;
    }

    /* No URL: check server first */
    claw_ota_info_t info;
    int ret = ota_check_update(&info);

    if (ret == 1) {
        if (ota_trigger_update(info.url) == CLAW_OK) {
            cJSON_AddStringToObject(result, "status", "started");
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Updating from %s to %s. "
                     "Device will reboot when done.",
                     claw_ota_running_version(), info.version);
            cJSON_AddStringToObject(result, "message", msg);
        } else {
            cJSON_AddStringToObject(result, "status", "error");
            cJSON_AddStringToObject(result, "error",
                                    "failed to start OTA update");
            return CLAW_ERROR;
        }
    } else if (ret == 0) {
        cJSON_AddStringToObject(result, "status", "up_to_date");
        cJSON_AddStringToObject(result, "version",
                                claw_ota_running_version());
    } else {
        cJSON_AddStringToObject(result, "status", "error");
        cJSON_AddStringToObject(result, "error",
                                "version check failed");
        return CLAW_ERROR;
    }
    return CLAW_OK;
}

static int tool_ota_version(const cJSON *params, cJSON *result)
{
    (void)params;

    cJSON_AddStringToObject(result, "status", "ok");
    cJSON_AddStringToObject(result, "version",
                            claw_ota_running_version());
    return CLAW_OK;
}

static int tool_ota_rollback(const cJSON *params, cJSON *result)
{
    (void)params;

    cJSON_AddStringToObject(result, "status", "ok");
    cJSON_AddStringToObject(result, "message",
                            "Rolling back firmware and rebooting...");

    /* Let the response be sent before reboot */
    claw_thread_delay_ms(1000);
    claw_ota_rollback();

    /* Not reached if rollback reboots */
    return CLAW_OK;
}

static const char schema_empty[] =
    "{\"type\":\"object\",\"properties\":{}}";

static const char schema_ota_update[] =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"url\":{\"type\":\"string\","
    "\"description\":\"Firmware URL. If omitted, checks the "
    "configured OTA server for the latest version.\"}},"
    "\"required\":[]}";

void claw_tools_register_ota(void)
{
    claw_tool_register("ota_check",
        "Check if a firmware update is available. Returns the "
        "current version and new version info if an update exists.",
        schema_empty, tool_ota_check,
        0, CLAW_TOOL_LOCAL_ONLY);

    claw_tool_register("ota_update",
        "Trigger an OTA firmware update. Optionally provide a "
        "direct firmware URL. The device will download, flash, "
        "and reboot automatically. Use with caution.",
        schema_ota_update, tool_ota_update,
        0, CLAW_TOOL_LOCAL_ONLY);

    claw_tool_register("ota_version",
        "Get the currently running firmware version.",
        schema_empty, tool_ota_version,
        0, CLAW_TOOL_LOCAL_ONLY);

    claw_tool_register("ota_rollback",
        "Roll back to the previous firmware version and reboot. "
        "Use this if the current firmware has issues after an OTA "
        "update. WARNING: device will reboot immediately.",
        schema_empty, tool_ota_rollback,
        0, CLAW_TOOL_LOCAL_ONLY);
}

#endif /* CONFIG_RTCLAW_OTA_ENABLE */
