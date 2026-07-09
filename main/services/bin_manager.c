/*
 * bin_manager.c
 * 垃圾桶业务逻辑管理器 实现
 *
 * 核心逻辑:
 *   1. 接收打开/关闭指令 → 控制 PCA9685 舵机
 *   2. 打开时启动 SoftwareTimer → 到期自动关闭
 *   3. 红外传感器触发 → 自动打开
 *   4. 超声波测距 → 判断是否满桶
 *   5. 状态变更 → 广播到 Event Group
 */

#include "bin_manager.h"
#include "event_bus.h"
#include "pca9685.h"
#include "pcf8574.h"
#include "hc_sr04.h"
#include "button.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "bin_mgr";

/* ─── 垃圾桶实例数组 ─── */
static struct {
    uint8_t         id;
    bool            is_open;
    bool            is_full;
    uint16_t        distance_cm;
    TimerHandle_t   auto_close_timer;
    uint32_t        close_countdown;  /* 剩余秒数 */
} g_bins[BIN_COUNT];

/* ─── 舵机通道映射 (从 config.h) ─── */
static const uint8_t g_servo_channels[BIN_COUNT] = {
    SERVO1_CHANNEL,
    SERVO2_CHANNEL,
    SERVO3_CHANNEL,
    SERVO4_CHANNEL,
};

/* ─── 状态标志位映射 (从 bin index → event group bits) ─── */
static const EventBits_t g_opened_bits[BIN_COUNT] = {
    EVT_BIN0_OPENED, EVT_BIN1_OPENED, EVT_BIN2_OPENED, EVT_BIN3_OPENED,
};
static const EventBits_t g_closed_bits[BIN_COUNT] = {
    EVT_BIN0_CLOSED, EVT_BIN1_CLOSED, EVT_BIN2_CLOSED, EVT_BIN3_CLOSED,
};
static const EventBits_t g_full_bits[BIN_COUNT] = {
    EVT_BIN0_FULL, EVT_BIN1_FULL, EVT_BIN2_FULL, EVT_BIN3_FULL,
};

/* ─── 前向声明 ─── */
static void auto_close_timer_cb(TimerHandle_t timer);
static void open_bin(uint8_t index, uint8_t source);
static void close_bin(uint8_t index);

/*
 * 自动关闭定时器回调
 * 由 FreeRTOS 定时器服务任务调用
 */
static void auto_close_timer_cb(TimerHandle_t timer)
{
    /* 通过 timer ID 获取 bin index */
    uint8_t index = (uint8_t)(uint32_t)pvTimerGetTimerID(timer);

    if (g_bins[index].is_open) {
        ESP_LOGI(TAG, "[桶%d] 自动关闭 (定时器到期)", index);
        close_bin(index);
    }
}

/*
 * 打开垃圾桶
 * @param source  触发来源: 0=语音 1=蓝牙 2=按键 3=红外
 */
static void open_bin(uint8_t index, uint8_t source)
{
    if (index >= BIN_COUNT) return;

    const char *src_label[] = {"语音", "蓝牙", "按键", "红外"};

    if (g_bins[index].is_open) {
        ESP_LOGD(TAG, "[桶%d] 已打开, 重置自动关闭计时", index);
        /* 重置定时器 */
        if (g_bins[index].auto_close_timer != NULL) {
            xTimerReset(g_bins[index].auto_close_timer, 0);
            g_bins[index].close_countdown = AUTO_CLOSE_DELAY_S;
        }
        return;
    }

    ESP_LOGI(TAG, "[桶%d] 打开 (来源: %s)", index,
             (source < 4) ? src_label[source] : "未知");

    /* 舵机打开 */
    pca9685_set_servo_angle(g_servo_channels[index], SERVO_OPEN_ANGLE);

    g_bins[index].is_open = true;
    g_bins[index].close_countdown = AUTO_CLOSE_DELAY_S;

    /* 启动自动关闭定时器 */
    if (g_bins[index].auto_close_timer != NULL) {
        xTimerReset(g_bins[index].auto_close_timer, 0);
    }

    /* 广播状态 */
    event_bus_set_status(g_opened_bits[index]);
    event_bus_clear_status(g_closed_bits[index]);
}

/*
 * 关闭垃圾桶
 */
static void close_bin(uint8_t index)
{
    if (index >= BIN_COUNT) return;
    if (!g_bins[index].is_open) return;

    ESP_LOGI(TAG, "[桶%d] 关闭", index);

    /* 舵机关闭 */
    pca9685_set_servo_angle(g_servo_channels[index], SERVO_CLOSE_ANGLE);

    g_bins[index].is_open = false;
    g_bins[index].close_countdown = 0;

    /* 停止自动关闭定时器 */
    if (g_bins[index].auto_close_timer != NULL) {
        xTimerStop(g_bins[index].auto_close_timer, 0);
    }

    /* 广播状态 */
    event_bus_set_status(g_closed_bits[index]);
    event_bus_clear_status(g_opened_bits[index]);
}

/* ─── 公共 API ─── */

esp_err_t bin_manager_init(void)
{
    memset(g_bins, 0, sizeof(g_bins));

    for (uint8_t i = 0; i < BIN_COUNT; i++) {
        g_bins[i].id = i;
        g_bins[i].is_open = false;

        /* 创建自动关闭定时器 (单次触发) */
        g_bins[i].auto_close_timer = xTimerCreate(
            "auto_close",
            pdMS_TO_TICKS(AUTO_CLOSE_DELAY_S * 1000),
            pdFALSE,  // 单次触发
            (void *)(uint32_t)i,
            auto_close_timer_cb
        );

        if (g_bins[i].auto_close_timer == NULL) {
            ESP_LOGE(TAG, "创建定时器失败 [桶%d]", i);
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "垃圾桶管理器初始化完成 (自动关闭=%ds)", AUTO_CLOSE_DELAY_S);
    return ESP_OK;
}

void bin_manager_get_status(uint8_t index, bin_status_t *status)
{
    if (index >= BIN_COUNT || status == NULL) return;

    status->id = g_bins[index].id;
    status->is_open = g_bins[index].is_open;
    status->is_full = g_bins[index].is_full;
    status->distance_cm = g_bins[index].distance_cm;
    status->auto_close_remaining_s = g_bins[index].close_countdown;
}

void bin_manager_get_all_status(bin_status_t status[BIN_COUNT])
{
    for (uint8_t i = 0; i < BIN_COUNT; i++) {
        bin_manager_get_status(i, &status[i]);
    }
}

/* ──────────────────────────────────────────
 * FreeRTOS 任务实现
 * ────────────────────────────────────────── */

/*
 * 舵机控制任务
 * 从事件总线接收指令, 控制舵机 + 管理自动关闭计时
 */
void servo_task(void *arg)
{
    ESP_LOGI(TAG, "舵机控制任务启动");

    /* 初始化所有舵机到关闭位置 */
    for (int i = 0; i < BIN_COUNT; i++) {
        pca9685_set_servo_angle(g_servo_channels[i], SERVO_CLOSE_ANGLE);
    }

    /* 倒计时计时器 (1 秒间隔更新) */
    TickType_t last_tick = xTaskGetTickCount();

    while (1) {
        bin_cmd_t cmd;
        esp_err_t ret;

        /* 非阻塞接收指令 (100ms 超时以支持倒计时更新) */
        ret = event_bus_recv_cmd(&cmd, pdMS_TO_TICKS(100));

        if (ret == ESP_OK) {
            if (cmd.open) {
                open_bin(cmd.bin_index, cmd.source);
            } else {
                close_bin(cmd.bin_index);
            }
        }

        /* 每秒更新倒计时 */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_tick) >= pdMS_TO_TICKS(1000)) {
            last_tick = now;
            for (int i = 0; i < BIN_COUNT; i++) {
                if (g_bins[i].is_open && g_bins[i].close_countdown > 0) {
                    g_bins[i].close_countdown--;
                }
            }
        }
    }
}

/*
 * 传感器采集任务
 * 定时读取红外 (PCF8574) + 超声波 (HC-SR04)
 * 红外检测到低电平 → 自动开盖
 * 超声波 < 满桶阈值 → 标记为满
 *
 * 传感器读取分频: 红外每 500ms, 超声波每 5s (10 次红外 = 1 次超声波)
 */
void sensor_task(void *arg)
{
    ESP_LOGI(TAG, "传感器采集任务启动");

    uint8_t cycle_count = 0;

    while (1) {
        /* ─── 读取红外传感器 (每轮) ─── */
        uint8_t pcf_data;
        if (pcf8574_read(&pcf_data) == ESP_OK) {
            for (int i = 0; i < BIN_COUNT; i++) {
                bool level = (pcf_data >> i) & 0x01;
                if (!level) {
                    /* 红外检测到物体 (低电平有效) — 自动打开 */
                    bin_cmd_t cmd = {
                        .bin_index = i,
                        .open = true,
                        .source = 3,  // 红外
                    };
                    event_bus_send_cmd(&cmd, 0);
                }
            }
        }

        /* ─── 读取超声波 (每 10 轮, 约 5s) ─── */
        cycle_count++;
        if (cycle_count >= 10) {
            cycle_count = 0;

            uint16_t distances[4];
            if (hc_sr04_get_all(distances) == ESP_OK) {
                for (int i = 0; i < BIN_COUNT; i++) {
                    g_bins[i].distance_cm = distances[i];

                    /* 判断满桶 (距离 < 阈值 且 距离 > 0, 排除传感器故障) */
                    bool was_full = g_bins[i].is_full;
                    bool now_full = (distances[i] > 0 && distances[i] < BIN_FULL_DIST_CM);

                    if (now_full != was_full) {
                        g_bins[i].is_full = now_full;
                        if (now_full) {
                            event_bus_set_status(g_full_bits[i]);
                            ESP_LOGW(TAG, "[桶%d] 已满! (距离=%d cm)", i, distances[i]);
                        } else {
                            event_bus_clear_status(g_full_bits[i]);
                        }
                    }
                }
            }
        }

        /* 500ms 采集周期 */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*
 * 按键扫描任务
 * 等待按键按下, 发送开盖指令
 */
void button_task(void *arg)
{
    ESP_LOGI(TAG, "按键扫描任务启动");

    /* 注册本任务以接收按键 ISR 通知 */
    button_register_task(xTaskGetCurrentTaskHandle());

    while (1) {
        /* 阻塞等待按键按下, 1s 超时 */
        uint8_t btn = button_wait_press(pdMS_TO_TICKS(1000));

        if (btn >= 1 && btn <= BIN_COUNT) {
            bin_cmd_t cmd = {
                .bin_index = btn - 1,
                .open = true,
                .source = 2,  // 按键
            };
            event_bus_send_cmd(&cmd, 0);
        }
    }
}
