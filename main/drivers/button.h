/*
 * button.h
 * 按键驱动 (GPIO + 消抖)
 *
 * 使用 GPIO 中断 + FreeRTOS Task Notification 实现消抖,
 * 替代原 Arduino 的 delay(10) 阻塞式消抖
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 最多 4 个按键 */
#define BUTTON_MAX_COUNT    4

/* 初始化按键
 * @param pins   GPIO 引脚数组
 * @param count  按键数量
 */
esp_err_t button_init(const uint8_t *pins, uint8_t count);

/* 注册按键接收任务 (用于消抖通知)
 * 按键 ISR 会在 GPIO 变化时通知此任务
 */
void button_register_task(TaskHandle_t task);

/* 等待按键按下 (阻塞式, 内部消抖)
 * @param timeout_ticks  超时 (FreeRTOS ticks)
 * @return 按键编号 1-4, 超时返回 0
 */
uint8_t button_wait_press(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
