/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * XiaoZhi xmini-c3 board — WiFi + SSD1306 OLED (128x64, I2C).
 *
 * Overrides the weak claw_lcd_* stubs so AI status messages
 * ("Thinking...", "Tool call...", progress) appear on the OLED.
 */

#include "claw_board.h"
#include "drivers/display/espressif/ssd1306_oled.h"
#include "claw/tools/claw_tools.h"

#include <string.h>

/* I2C pin assignment (same bus as Audio Codec on xmini-c3) */
#define OLED_SDA_PIN  3
#define OLED_SCL_PIN  4

/*
 * OLED layout (128x64, 8 rows of 8px each):
 *   Row 0: "rt-claw v0.1.0"
 *   Row 1: (blank)
 *   Row 2-5: status / AI response (4 lines)
 *   Row 6: progress bar
 *   Row 7: free heap info
 */

#define STATUS_ROW_START 2
#define STATUS_ROWS      4
#define PROGRESS_ROW     6

static int s_oled_ready;

void board_early_init(void)
{
    wifi_board_early_init();

    if (ssd1306_init(OLED_SDA_PIN, OLED_SCL_PIN) == 0) {
        s_oled_ready = 1;
        ssd1306_write_line(0, "  rt-claw v0.1.0");
        ssd1306_write_line(1, "  xmini-c3 OLED");
    }
}

const shell_cmd_t *board_platform_commands(int *count)
{
    return wifi_board_platform_commands(count);
}

/* ---- Override weak claw_lcd_* stubs ---- */

int claw_lcd_init(void)
{
    return s_oled_ready ? 0 : -1;
}

int claw_lcd_available(void)
{
    return s_oled_ready;
}

void claw_lcd_status(const char *msg)
{
    if (!s_oled_ready || !msg) {
        return;
    }

    /* Clear status rows */
    for (int r = STATUS_ROW_START; r < STATUS_ROW_START + STATUS_ROWS; r++) {
        ssd1306_write_line(r, "");
    }

    /* Word-wrap msg across status rows (16 chars per line) */
    int len = (int)strlen(msg);
    int chars_per_line = SSD1306_WIDTH / 8;

    for (int r = 0; r < STATUS_ROWS && len > 0; r++) {
        char line[17];
        int n = len > chars_per_line ? chars_per_line : len;
        memcpy(line, msg, n);
        line[n] = '\0';
        ssd1306_write_line(STATUS_ROW_START + r, line);
        msg += n;
        len -= n;
    }
}

void claw_lcd_progress(int percent)
{
    if (!s_oled_ready) {
        return;
    }
    ssd1306_progress_bar(PROGRESS_ROW, percent);
}
