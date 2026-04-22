/*
 * Copyright (c) 2026, apolecoco <apolecoco@163.com>
 * SPDX-License-Identifier: MIT
 *
 * ST7789 LCD driver — 240x240 RGB565 SPI display.
 */

#include "drivers/display/espressif/st7789_lcd.h"

#ifdef CLAW_PLATFORM_ESP_IDF

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include <string.h>

#define TAG "st7789"

/* Pin mapping for your ESP32-S3 board */
#define TFT_SCLK   21
#define TFT_MOSI   47
#define TFT_RST    45
#define TFT_DC     40
#define TFT_CS     41
#define TFT_BL     42

#define LCD_HOST   SPI2_HOST
#define LCD_H_RES  ST7789_WIDTH
#define LCD_V_RES  ST7789_HEIGHT
#define LCD_BPP    16

/* Keep first version conservative */
#define LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)
#define LCD_SPI_MODE        0

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static int s_initialized;

static int st7789_init_panel(void)
{
    esp_err_t err;

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = TFT_SCLK,
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };

    err = spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init: %s", esp_err_to_name(err));
        return -1;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = TFT_DC,
        .cs_gpio_num = TFT_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = LCD_SPI_MODE,
        .trans_queue_depth = 10,
    };

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_panel_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel IO init: %s", esp_err_to_name(err));
        return -1;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = LCD_BPP,
    };

    err = esp_lcd_new_panel_st7789(s_panel_io, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel init: %s", esp_err_to_name(err));
        return -1;
    }

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_disp_on_off(s_panel, true);

    /* Common orientation fixups for square ST7789 panels.
     * If display is mirrored/rotated wrong, tweak these later.
     */
    esp_lcd_panel_mirror(s_panel, true, false);
    esp_lcd_panel_swap_xy(s_panel, false);

    s_initialized = 1;
    ESP_LOGI(TAG, "240x240 ST7789 ready (SPI)");
    return 0;
}

int st7789_backlight(bool on)
{
    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << TFT_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&io_cfg) != ESP_OK) {
        return -1;
    }

    /* Most modules are active-high. If screen stays dark, try 0/1 reversed. */
    if (gpio_set_level(TFT_BL, on ? 1 : 0) != ESP_OK) {
        return -1;
    }

    return 0;
}

int st7789_init(void)
{
    if (s_initialized) {
        return 0;
    }

    if (st7789_backlight(false) != 0) {
        ESP_LOGE(TAG, "backlight GPIO init failed");
        return -1;
    }

    if (st7789_init_panel() != 0) {
        return -1;
    }

    if (st7789_backlight(true) != 0) {
        ESP_LOGE(TAG, "backlight on failed");
        return -1;
    }

    return 0;
}

static uint16_t st7789_fix_color565(uint16_t color)
{
    uint16_t r5 = (color >> 11) & 0x1F;
    uint16_t g6 = (color >> 5)  & 0x3F;
    uint16_t b5 =  color        & 0x1F;

    /* Based on observed mapping:
     * sent red   -> shown blue
     * sent green -> shown red
     * sent blue  -> shown green
     *
     * So to display correct RGB, send:
     * R' = B
     * G' = R
     * B' = G
     *
     * Need approximate bit-width conversion:
     * 5-bit -> 6-bit : expand
     * 6-bit -> 5-bit : compress
     */
    uint16_t out_r5 = b5;
    uint16_t out_g6 = (r5 << 1) | (r5 >> 4);
    uint16_t out_b5 = g6 >> 1;

    return (out_r5 << 11) | (out_g6 << 5) | out_b5;
}

int st7789_fill_color(uint16_t color)
{
    static uint16_t line_buf[LCD_H_RES * 20];
    int i, y;

    if (!s_initialized) {
        return -1;
    }

    for (i = 0; i < (int)(sizeof(line_buf) / sizeof(line_buf[0])); i++) {
        line_buf[i] = st7789_fix_color565(color);
    }

    for (y = 0; y < LCD_V_RES; y += 20) {
        int y2 = y + 20;
        if (y2 > LCD_V_RES) {
            y2 = LCD_V_RES;
        }

        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y2, line_buf);
    }

    return 0;
}

int st7789_present_fb(const uint16_t *fb, int width, int height)
{
    static uint16_t line_buf[ST7789_WIDTH * 20];
    int x, y;

    if (!s_initialized || !fb) {
        return -1;
    }

    if (width != ST7789_WIDTH || height != ST7789_HEIGHT) {
        return -1;
    }

    for (y = 0; y < height; y += 20) {
        int h = 20;
        if (y + h > height) {
            h = height - y;
        }

        for (int row = 0; row < h; row++) {
            for (x = 0; x < width; x++) {
                uint16_t c = fb[(y + row) * width + x];
                line_buf[row * width + x] = st7789_fix_color565(c);
            }
        }

        esp_lcd_panel_draw_bitmap(s_panel, 0, y, width, y + h, line_buf);
    }

    return 0;
}

#else /* non-ESP-IDF */

int st7789_init(void) { return -1; }
int st7789_fill_color(uint16_t color) { (void)color; return -1; }
int st7789_backlight(bool on) { (void)on; return -1; }

#endif

#include "claw/core/driver.h"

static claw_err_t st7789_drv_probe(struct claw_driver *drv)
{
    (void)drv;
    /* Actual init requires pin config, deferred to platform board_init */
    return CLAW_OK;
}

static const struct claw_driver_ops st7789_drv_ops = {
    .probe  = st7789_drv_probe,
    .remove = NULL,
};

static struct claw_driver st7789_drv = {
    .name  = "st7789_lcd",
    .ops   = &st7789_drv_ops,
    .state = CLAW_DRV_REGISTERED,
};

CLAW_DRIVER_REGISTER(st7789_lcd, &st7789_drv);