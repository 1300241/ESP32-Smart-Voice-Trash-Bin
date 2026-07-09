/*
 * event_bus.h
 * 事件总线 — FreeRTOS Queue + Event Group
 *
 * 所有任务间通信通过此模块解耦:
 *   - Queue:  传输开关指令 (哪个桶 + 开关)
 *   - Event Group: 广播垃圾桶状态变更 (UI 订阅)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 事件类型 ─── */

/* 开关垃圾桶事件 (通过队列传递) */
typedef struct {
    uint8_t  bin_index;   /* 垃圾桶编号 0-3 */
    bool     open;         /* true=开, false=关 */
    uint8_t  source;       /* 来源: 0=语音 1=蓝牙 2=按键 3=红外 */
} bin_cmd_t;

/* 垃圾桶状态变更标志位 (通过 Event Group 广播) */
#define EVT_BIN0_OPENED     BIT0
#define EVT_BIN0_CLOSED     BIT1
#define EVT_BIN0_FULL       BIT2
#define EVT_BIN1_OPENED     BIT3
#define EVT_BIN1_CLOSED     BIT4
#define EVT_BIN1_FULL       BIT5
#define EVT_BIN2_OPENED     BIT6
#define EVT_BIN2_CLOSED     BIT7
#define EVT_BIN2_FULL       BIT8
#define EVT_BIN3_OPENED     BIT9
#define EVT_BIN3_CLOSED     BIT10
#define EVT_BIN3_FULL       BIT11

/* ─── 垃圾桶状态结构 ─── */
typedef struct {
    uint8_t  id;
    bool     is_open;
    bool     is_full;
    uint16_t distance_cm;
    uint32_t auto_close_remaining_s;   /* 自动关闭剩余秒数 */
} bin_status_t;

/* ─── API ─── */

/* 初始化事件总线 */
esp_err_t event_bus_init(void);

/* 发送开/关垃圾桶指令
 * @param cmd  指令结构体
 * @param timeout_ticks  队列满时等待超时
 */
esp_err_t event_bus_send_cmd(const bin_cmd_t *cmd, TickType_t timeout_ticks);

/* 接收开/关垃圾桶指令 (阻塞式)
 * @param cmd            输出
 * @param timeout_ticks  等待超时
 * @return ESP_OK 成功, ESP_ERR_TIMEOUT 超时
 */
esp_err_t event_bus_recv_cmd(bin_cmd_t *cmd, TickType_t timeout_ticks);

/* 设置垃圾桶状态标志位 (广播)
 * @param bits  要设置的标志位 (EVT_xxx 组合)
 */
esp_err_t event_bus_set_status(EventBits_t bits);

/* 清除垃圾桶状态标志位
 * @param bits  要清除的标志位
 */
esp_err_t event_bus_clear_status(EventBits_t bits);

/* 等待状态变更 (OR 模式)
 * @param bits_to_wait  等待的标志位
 * @param timeout_ticks  超时
 * @return 实际被设置的标志位
 */
EventBits_t event_bus_wait_status(EventBits_t bits_to_wait, TickType_t timeout_ticks);

/* 获取当前所有事件组位 */
EventBits_t event_bus_get_status(void);

/* 获取事件组句柄 (供 bin_manager 内部使用) */
EventGroupHandle_t event_bus_get_event_group(void);

#ifdef __cplusplus
}
#endif
