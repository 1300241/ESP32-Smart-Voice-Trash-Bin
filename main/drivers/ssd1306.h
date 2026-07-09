/*
 * ssd1306.h
 * SSD1306 128x64 OLED 显示屏 (I2C)
 * 基于 u8g2 图形库, 支持中文显示
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 SSD1306 + u8g2
 * @param i2c_addr  I2C 地址 (通常 0x3C)
 */
esp_err_t ssd1306_init(uint8_t i2c_addr);

/* 获取 u8g2 句柄 (供 display 模块直接绘图) */
u8g2_t *ssd1306_get_u8g2(void);

/* 清屏 */
void ssd1306_clear(void);

/* 发送 framebuffer 到屏幕 */
void ssd1306_flush(void);

#ifdef __cplusplus
}
#endif
