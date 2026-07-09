/*
 * pca9685.h
 * PCA9685 16通道 12-bit PWM 舵机驱动板
 * I2C 接口, 默认地址 0x40
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* I2C 初始化 (由 main 调用, 与其它 I2C 设备共享总线) */
esp_err_t pca9685_i2c_init(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint32_t freq_hz);

/* 初始化 PCA9685
 * - 设置振荡器频率 27MHz
 * - 设置 PWM 频率 50Hz (舵机标准)
 */
esp_err_t pca9685_init(uint8_t i2c_addr);

/* 设置指定通道的 PWM 脉宽
 * @param channel  通道号 0-15
 * @param off      关断计数值 (0-4095)
 */
esp_err_t pca9685_set_pwm(uint8_t channel, uint16_t off);

/* 设置舵机角度 (0-180°)
 * @param channel  PCA9685 通道号
 * @param angle    目标角度
 */
esp_err_t pca9685_set_servo_angle(uint8_t channel, uint8_t angle);

/* 将通道复位 (脉宽 = 0) */
esp_err_t pca9685_reset_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif
