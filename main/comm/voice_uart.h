/*
 * voice_uart.h
 * 天问 ASRPRO 语音模块通信 (UART)
 *
 * 通信协议:
 *   ESP32 → ASRPRO:  "O100"~"O103" (打开), "C100"~"C103" (关闭)
 *   ASRPRO → ESP32:  "100"~"103" (语音识别结果)
 *
 * 硬件: UART2, TX=GPIO17, RX=GPIO16, 115200-8-N-1
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化语音模块 UART */
esp_err_t voice_uart_init(void);

/* 发送指令给语音模块 (播放对应语音)
 * @param cmd_code  指令码: "100"/"101"/"102"/"103"
 * @param is_open   true=打开(O), false=关闭(C)
 */
esp_err_t voice_uart_send_cmd(const char *cmd_code, bool is_open);

/* 语音模块接收任务 (FreeRTOS Task)
 * 监听 UART, 解析识别结果并发送到事件总线
 */
void voice_uart_task(void *arg);

#ifdef __cplusplus
}
#endif
