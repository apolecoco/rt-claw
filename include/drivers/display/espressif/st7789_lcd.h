/*
 * Copyright (c) 2026, apolecoco <apolecoco@163.com>
 * SPDX-License-Identifier: MIT
 *
 * ST7789 LCD driver — lightweight 240x240 RGB565 SPI display.
 * Uses ESP-IDF esp_lcd + SPI. No LVGL dependency.
 */

#ifndef CLAW_DRIVERS_DISPLAY_ESPRESSIF_ST7789_LCD_H
#define CLAW_DRIVERS_DISPLAY_ESPRESSIF_ST7789_LCD_H

#include <stdint.h>
#include <stdbool.h>

#define ST7789_WIDTH   240
#define ST7789_HEIGHT  240

int st7789_init(void);
int st7789_fill_color(uint16_t color);
int st7789_backlight(bool on);
int st7789_present_fb(const uint16_t *fb, int width, int height);

#endif /* CLAW_DRIVERS_DISPLAY_ESPRESSIF_ST7789_LCD_H */