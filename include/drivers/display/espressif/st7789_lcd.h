#pragma once

#include <stdint.h>
#include <stdbool.h>

int st7789_lcd_init(void);
int st7789_lcd_fill(uint16_t rgb565);
int st7789_lcd_draw_bitmap(int x1, int y1, int x2, int y2, const void *buf);
int st7789_lcd_set_rotation(int rotation);
int st7789_lcd_backlight(bool on);
int st7789_lcd_width(void);
int st7789_lcd_height(void);
