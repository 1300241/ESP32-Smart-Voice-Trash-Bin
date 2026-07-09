/*
 * pcf8574.h
 * PCF8574 / PCF8574A 8-bit I2C IO 扩展芯片
 * 用于读取 4 路红外反射传感器
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 PCF8574
 * @param i2c_addr  I2C 地址 (通常 0x20-0x27)
 */
esp_err_t pcf8574_init(uint8_t i2c_addr);

/* 读取 8 个 IO 引脚状态
 * @param data  输出, bit0=P0, bit1=P1, ...
 * @return      读取到的引脚状态位掩码
 */
esp_err_t pcf8574_read(uint8_t *data);

/* 读取单个引脚
 * @param pin  引脚号 0-7
 * @param level 输出, true=高电平, false=低电平
 */
esp_err_t pcf8574_read_pin(uint8_t pin, bool *level);

#ifdef __cplusplus
}
#endif
