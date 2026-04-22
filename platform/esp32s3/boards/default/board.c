/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * ESP32-S3 default board — WiFi + PSRAM real hardware.
 */

#include "platform/board.h"
#include "drivers/display/espressif/st7789_lcd.h"
#include "claw/services/tools/tools.h"

#ifdef CLAW_PLATFORM_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

void board_early_init(void)
{
    wifi_board_early_init();
    claw_lcd_init();
}

const shell_cmd_t *board_platform_commands(int *count)
{
    return wifi_board_platform_commands(count);
}
