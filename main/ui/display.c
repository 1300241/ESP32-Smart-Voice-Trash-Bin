/*
 * display.c
 * OLED 显示任务 实现
 *
 * 布局 (128x64):
 *   第 0 行 (y=0):  可回收垃圾  打开/关闭  [满]
 *   第 1 行 (y=16): 厨余垃圾    打开/关闭  [满]
 *   第 2 行 (y=32): 有害垃圾    打开/关闭  [满]
 *   第 3 行 (y=48): 其他垃圾    打开/关闭  [满]
 *
 * 使用 u8g2 的 wqy12 字体 (12x12 像素, 支持中文)
 */

#include "display.h"
#include "ssd1306.h"
#include "bin_manager.h"
#include "event_bus.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "display";

/* 垃圾桶标签 */
static const char *g_labels[BIN_COUNT] = {
    BIN_LABEL_0, BIN_LABEL_1, BIN_LABEL_2, BIN_LABEL_3,
};

/* 状态变更标志位 (所有需要刷新的事件) */
#define DISPLAY_EVENT_BITS  ( \
    EVT_BIN0_OPENED | EVT_BIN0_CLOSED | EVT_BIN0_FULL | \
    EVT_BIN1_OPENED | EVT_BIN1_CLOSED | EVT_BIN1_FULL | \
    EVT_BIN2_OPENED | EVT_BIN2_CLOSED | EVT_BIN2_FULL | \
    EVT_BIN3_OPENED | EVT_BIN3_CLOSED | EVT_BIN3_FULL   \
)


void display_task(void *arg)
{
    ESP_LOGI(TAG, "显示任务启动 (刷新间隔=%dms)", DISPLAY_REFRESH_MS);

    u8g2_t *u8g2 = ssd1306_get_u8g2();

    while (1) {
        bin_status_t status[BIN_COUNT];
        bin_manager_get_all_status(status);

        /* ─── 绘制 framebuffer ─── */
        u8g2_ClearBuffer(u8g2);
        u8g2_SetFont(u8g2, u8g2_font_wqy12_t_gb2312a);
        u8g2_SetFontPosTop(u8g2);

        for (int i = 0; i < BIN_COUNT; i++) {
            char line[32];

            /* 标签 */
            const char *label = g_labels[i];

            /* 状态 */
            const char *state_str = status[i].is_open ? "打开" : "关闭";

            /* 满桶标志 */
            const char *full_str = status[i].is_full ? " 满" : "";

            snprintf(line, sizeof(line), "%s %s%s", label, state_str, full_str);

            u8g2_SetCursor(u8g2, 0, 16 * i);
            u8g2_print(u8g2, line);
        }

        /* 发送到屏幕 */
        u8g2_SendBuffer(u8g2);

        /* 等待下次刷新 (用 Event Group 等待, 状态变更时立即刷新) */
        event_bus_wait_status(DISPLAY_EVENT_BITS,
                              pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
    }
}
