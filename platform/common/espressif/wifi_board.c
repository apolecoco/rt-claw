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
#include "esp_wifi.h"

/* ---- WiFi shell commands ---- */

static void cmd_wifi_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: /wifi_set <SSID> <PASSWORD>\n");
        return;
    }
    wifi_manager_set_credentials(argv[1], argv[2]);
    printf("WiFi credentials saved. Reconnecting...\n");
    esp_wifi_disconnect();
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

static const shell_cmd_t s_wifi_commands[] = {
    SHELL_CMD("/wifi_set",    cmd_wifi_set,    "Save WiFi credentials"),
    SHELL_CMD("/wifi_status", cmd_wifi_status, "Show WiFi connection"),
    SHELL_CMD("/wifi_scan",   cmd_wifi_scan,   "Scan nearby APs"),
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
        } else {
            CLAW_LOGW("board", "WiFi timeout, continuing offline");
        }
    } else {
        CLAW_LOGW("board", "no WiFi credentials, use /wifi_set");
    }
}

const shell_cmd_t *wifi_board_platform_commands(int *count)
{
    *count = SHELL_CMD_COUNT(s_wifi_commands);
    return s_wifi_commands;
}
