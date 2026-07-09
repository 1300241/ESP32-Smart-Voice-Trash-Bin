/*
 * bluetooth_spp.h
 * 蓝牙 SPP (Serial Port Profile) 通信
 *
 * 模拟蓝牙串口, 接收手机 APP 发送的垃圾分类指令
 * 协议: 与语音模块相同, 接收 "100"~"103" 字符串
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 蓝牙设备名称 */
#define BT_DEVICE_NAME      "ESP32_SmartBin"

/* 蓝牙 SPP 任务入口 (FreeRTOS Task)
 * 初始化蓝牙协议栈, 等待客户端连接并接收数据
 */
void bluetooth_spp_task(void *arg);

#ifdef __cplusplus
}
#endif
