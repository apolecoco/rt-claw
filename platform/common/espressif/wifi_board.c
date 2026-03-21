/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * WiFi board helpers — shared by all Espressif WiFi boards
 * (devkit, xiaozhi-xmini, etc.). Provides WiFi init and shell commands.
 */

#include "claw_board.h"
#include "drivers/net/espressif/wifi_manager.h"
#include "osal/claw_os.h"

#include <stdio.h>

#ifdef CONFIG_RTCLAW_WIFI_PROVISION_ENABLE
#include "wifi_provision.h"
#endif

/* ---- WiFi shell commands ---- */

static void cmd_wifi_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: /wifi_set <SSID> <PASSWORD>\n");
        return;
    }
    if (wifi_manager_set_credentials(argv[1], argv[2]) != ESP_OK) {
        printf("Failed to save WiFi credentials.\n");
        return;
    }
    printf("WiFi credentials saved. Reconnecting...\n");
    wifi_manager_reconnect(argv[1], argv[2]);
}

static void cmd_wifi_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("WiFi: %s\n",
           wifi_manager_is_connected() ? "connected" : "disconnected");
    printf("IP:   %s\n", wifi_manager_get_ip());
}

static void cmd_wifi_scan(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    wifi_manager_scan_and_print();
}

#ifdef CONFIG_RTCLAW_WIFI_PROVISION_ENABLE
static void cmd_wifi_portal(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (wifi_provision_is_active()) {
        printf("Provisioning already active at http://192.168.4.1\n");
        return;
    }
    printf("Starting WiFi provisioning portal...\n");
    if (wifi_provision_start() != ESP_OK) {
        printf("Failed to start provisioning portal.\n");
    }
}
#endif

static const shell_cmd_t s_wifi_commands[] = {
    SHELL_CMD("/wifi_set",    cmd_wifi_set,    "Save WiFi credentials"),
    SHELL_CMD("/wifi_status", cmd_wifi_status, "Show WiFi connection"),
    SHELL_CMD("/wifi_scan",   cmd_wifi_scan,   "Scan nearby APs"),
#ifdef CONFIG_RTCLAW_WIFI_PROVISION_ENABLE
    SHELL_CMD("/wifi_portal", cmd_wifi_portal, "Start WiFi config portal"),
#endif
};

/* ---- WiFi board interface ---- */

void wifi_board_early_init(void)
{
    wifi_manager_init();
    esp_err_t wifi_err = wifi_manager_start();

    if (wifi_err == ESP_OK) {
        CLAW_LOGI("board", "waiting for WiFi (30s timeout)...");
        if (wifi_manager_wait_connected(30000) == ESP_OK) {
            CLAW_LOGI("board", "WiFi connected: %s",
                      wifi_manager_get_ip());
            return;
        }
        CLAW_LOGW("board", "WiFi timeout, continuing offline");
    } else {
        CLAW_LOGW("board", "no WiFi credentials");
    }

#ifdef CONFIG_RTCLAW_WIFI_PROVISION_ENABLE
    CLAW_LOGI("board", "starting WiFi provisioning portal...");
    if (wifi_provision_start() != ESP_OK) {
        CLAW_LOGE("board", "provisioning portal failed to start");
    }
#else
    if (wifi_err != ESP_OK) {
        CLAW_LOGW("board", "use /wifi_set to configure WiFi");
    }
#endif
}

const shell_cmd_t *wifi_board_platform_commands(int *count)
{
    *count = SHELL_CMD_COUNT(s_wifi_commands);
    return s_wifi_commands;
}
