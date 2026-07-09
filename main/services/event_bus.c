/*
 * event_bus.c
 * 事件总线 实现
 */

#include "event_bus.h"
#include "esp_log.h"

static const char *TAG = "event_bus";

/* 指令队列 (最多缓存 10 条指令) */
static QueueHandle_t g_cmd_queue = NULL;

/* 状态事件组 */
static EventGroupHandle_t g_status_evt = NULL;

#define CMD_QUEUE_LENGTH    10

esp_err_t event_bus_init(void)
{
    /* 创建指令队列 */
    g_cmd_queue = xQueueCreate(CMD_QUEUE_LENGTH, sizeof(bin_cmd_t));
    if (g_cmd_queue == NULL) {
        ESP_LOGE(TAG, "创建指令队列失败");
        return ESP_ERR_NO_MEM;
    }

    /* 创建状态事件组 */
    g_status_evt = xEventGroupCreate();
    if (g_status_evt == NULL) {
        ESP_LOGE(TAG, "创建状态事件组失败");
        vQueueDelete(g_cmd_queue);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "事件总线初始化完成 (队列=%d, 事件组)", CMD_QUEUE_LENGTH);
    return ESP_OK;
}

esp_err_t event_bus_send_cmd(const bin_cmd_t *cmd, TickType_t timeout_ticks)
{
    if (g_cmd_queue == NULL) return ESP_ERR_INVALID_STATE;

    if (xQueueSend(g_cmd_queue, cmd, timeout_ticks) != pdTRUE) {
        ESP_LOGW(TAG, "指令队列已满, 丢弃 bin=%d cmd=%d", cmd->bin_index, cmd->open);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t event_bus_recv_cmd(bin_cmd_t *cmd, TickType_t timeout_ticks)
{
    if (g_cmd_queue == NULL) return ESP_ERR_INVALID_STATE;

    if (xQueueReceive(g_cmd_queue, cmd, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t event_bus_set_status(EventBits_t bits)
{
    if (g_status_evt == NULL) return ESP_ERR_INVALID_STATE;

    xEventGroupSetBits(g_status_evt, bits);
    return ESP_OK;
}

esp_err_t event_bus_clear_status(EventBits_t bits)
{
    if (g_status_evt == NULL) return ESP_ERR_INVALID_STATE;

    xEventGroupClearBits(g_status_evt, bits);
    return ESP_OK;
}

EventBits_t event_bus_wait_status(EventBits_t bits_to_wait, TickType_t timeout_ticks)
{
    if (g_status_evt == NULL) return 0;

    return xEventGroupWaitBits(
        g_status_evt,
        bits_to_wait,
        pdTRUE,    // 等待后清除
        pdFALSE,   // OR 模式 (任意一位)
        timeout_ticks
    );
}

EventBits_t event_bus_get_status(void)
{
    if (g_status_evt == NULL) return 0;
    return xEventGroupGetBits(g_status_evt);
}

EventGroupHandle_t event_bus_get_event_group(void)
{
    return g_status_evt;
}
