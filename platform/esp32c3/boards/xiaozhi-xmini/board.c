/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * XiaoZhi xmini-c3 board — WiFi + SSD1306 OLED + ES8311 Audio.
 *
 * I2C bus is shared between OLED (0x3C) and ES8311 codec (0x18).
 * Board manages the bus handle and passes it to both drivers.
 */

#include "claw_board.h"
#include "drivers/display/espressif/ssd1306_oled.h"
#include "drivers/audio/espressif/es8311_audio.h"
#include "claw/tools/claw_tools.h"

#ifdef CLAW_PLATFORM_ESP_IDF
#include "driver/i2c_master.h"
#include "esp_log.h"
#endif

#include <string.h>

#define TAG "board"

/* I2C pin assignment (shared by OLED + ES8311) */
#define I2C_SDA_PIN   3
#define I2C_SCL_PIN   4

/* Power amplifier enable */
#define PA_PIN        11

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
static int s_audio_ready;

void board_early_init(void)
{
    wifi_board_early_init();

#ifdef CLAW_PLATFORM_ESP_IDF
    /* Create shared I2C bus */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s",
                 esp_err_to_name(err));
        return;
    }

    /* Initialize OLED on shared bus */
    if (ssd1306_init_on_bus(i2c_bus) == 0) {
        s_oled_ready = 1;
        ssd1306_write_line(0, "  rt-claw v0.1.0");
        ssd1306_write_line(1, "  xmini-c3");
    }

    /* Initialize ES8311 audio codec on shared bus */
    if (es8311_audio_init(i2c_bus, PA_PIN) == 0) {
        s_audio_ready = 1;
        /* Boot beep: short 1kHz tone */
        es8311_audio_beep(1000, 100, 50);
    }
#endif
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

    for (int r = STATUS_ROW_START;
         r < STATUS_ROW_START + STATUS_ROWS; r++) {
        ssd1306_write_line(r, "");
    }

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
