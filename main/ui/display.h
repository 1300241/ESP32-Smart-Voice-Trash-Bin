/*
 * display.h
 * OLED 显示任务
 *
 * 定时刷新 SSD1306 128x64, 显示 4 个垃圾桶状态:
 *   "标签名  [打开/关闭]  [满]"
 *
 * 使用 u8g2 中文 (wqy12) 字体
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OLED 显示任务入口 (FreeRTOS Task)
 * 500ms 间隔刷新屏幕
 */
void display_task(void *arg);

#ifdef __cplusplus
}
#endif
