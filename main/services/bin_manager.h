/*
 * bin_manager.h
 * 垃圾桶业务逻辑管理器
 *
 * 职责:
 *   - 管理 4 个垃圾桶的状态机
 *   - 自动关闭计时 (FreeRTOS Software Timer)
 *   - 舵机控制 (通过 PCA9685)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 垃圾桶状态 ─── */
typedef enum {
    BIN_STATE_CLOSED = 0,     /* 关闭 */
    BIN_STATE_OPENED,         /* 打开中 */
} bin_state_t;

/* ─── API ─── */

/* 初始化垃圾桶管理器 (创建所有软件定时器) */
esp_err_t bin_manager_init(void);

/* 获取指定垃圾桶的状态快照 */
void bin_manager_get_status(uint8_t index, bin_status_t *status);

/* 获取全部 4 个垃圾桶的状态 */
void bin_manager_get_all_status(bin_status_t status[BIN_COUNT]);

/*
 * 舵机控制任务入口 (FreeRTOS Task)
 * 从事件总线接收指令, 控制舵机 + 管理自动关闭计时
 */
void servo_task(void *arg);

/*
 * 传感器采集任务入口 (FreeRTOS Task)
 * 定时读取红外 + 超声波, 触发自动开盖
 */
void sensor_task(void *arg);

/*
 * 按键扫描任务入口 (FreeRTOS Task)
 * 等待按键事件, 发送开盖指令
 */
void button_task(void *arg);

#ifdef __cplusplus
}
#endif
