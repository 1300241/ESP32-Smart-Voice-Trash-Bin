/*
 * bluetooth_spp.c
 * 蓝牙 SPP 通信 实现
 *
 * 基于 ESP-IDF Bluedroid 协议栈, 实现 SPP Server
 * 手机 APP 连接后发送 "100"~"103" 控制垃圾桶开关
 */

#include "bluetooth_spp.h"
#include "event_bus.h"
#include "config.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "esp_gap_bt_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "bt_spp";

/* SPP 接收缓冲区 */
#define BT_RX_BUF_SIZE  256

/* 接收数据队列 (SPP callback → 任务) */
static QueueHandle_t g_bt_rx_queue = NULL;

/* SPP 连接句柄 */
static uint32_t g_spp_handle = 0;
static bool     g_connected = false;

/* 指令码查找表 */
static const char *g_cmd_codes[BIN_COUNT] = {
    CMD_RECYCLABLE, CMD_KITCHEN, CMD_HAZARDOUS, CMD_OTHER,
};

/*
 * GAP (Generic Access Profile) 回调
 */
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "蓝牙认证成功: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "蓝牙认证失败: status=%d", param->auth_cmpl.stat);
        }
        break;

    case ESP_BT_GAP_PIN_REQ_EVT:
        /* 无 PIN 码, 直接拒绝 (使用默认安全级别) */
        ESP_LOGI(TAG, "蓝牙 PIN 请求, 自动取消");
        esp_bt_pin_reply(param->pin_req.bda, false, 0, NULL);
        break;

    default:
        break;
    }
}

/*
 * SPP (Serial Port Profile) 回调
 */
static void bt_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(TAG, "SPP 协议栈初始化完成");
        /* 启动 SPP 服务器 */
        esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, BT_DEVICE_NAME);
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        /* 有客户端连接 */
        g_spp_handle = param->srv_open.handle;
        g_connected = true;
        ESP_LOGI(TAG, "蓝牙客户端已连接 (handle=%lu)", g_spp_handle);
        break;

    case ESP_SPP_CLOSE_EVT:
        /* 连接断开 */
        g_spp_handle = 0;
        g_connected = false;
        ESP_LOGI(TAG, "蓝牙客户端已断开");
        break;

    case ESP_SPP_DATA_IND_EVT:
        /* 收到数据 — 原样推送字符串到队列 */
        if (param->data_ind.len > 0 && param->data_ind.len < BT_RX_BUF_SIZE) {
            char *rx_data = malloc(param->data_ind.len + 1);
            if (rx_data) {
                memcpy(rx_data, param->data_ind.data, param->data_ind.len);
                rx_data[param->data_ind.len] = '\0';
                /* 去除尾部 \r\n */
                for (int i = param->data_ind.len - 1; i >= 0; i--) {
                    if (rx_data[i] == '\r' || rx_data[i] == '\n') {
                        rx_data[i] = '\0';
                    } else {
                        break;
                    }
                }
                xQueueSend(g_bt_rx_queue, &rx_data, 0);
            }
        }
        break;

    case ESP_SPP_WRITE_EVT:
        ESP_LOGD(TAG, "SPP 写入完成: cong=%d", param->write.cong);
        break;

    default:
        break;
    }
}


void bluetooth_spp_task(void *arg)
{
    ESP_LOGI(TAG, "蓝牙 SPP 任务启动");

    /* 创建数据接收队列 (队列元素为 char* 指针) */
    g_bt_rx_queue = xQueueCreate(10, sizeof(char *));
    if (g_bt_rx_queue == NULL) {
        ESP_LOGE(TAG, "无法创建蓝牙接收队列");
        vTaskDelete(NULL);
        return;
    }

    /* ─── 初始化蓝牙控制器 ─── */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "蓝牙控制器初始化失败: %s", esp_err_to_name(ret));
        goto fail;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "蓝牙控制器使能失败");
        goto fail;
    }

    /* ─── 初始化 Bluedroid ─── */
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid 初始化失败");
        goto fail;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid 使能失败");
        goto fail;
    }

    /* ─── 注册回调 ─── */
    ret = esp_bt_gap_register_callback(bt_gap_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP 回调注册失败");
        goto fail;
    }

    ret = esp_spp_register_callback(bt_spp_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPP 回调注册失败");
        goto fail;
    }

    /* ─── 初始化 SPP ─── */
    ret = esp_spp_init(ESP_SPP_MODE_CB);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPP 初始化失败");
        goto fail;
    }

    /* ─── 设置设备名称 ─── */
    esp_bt_dev_set_device_name(BT_DEVICE_NAME);

    /* ─── 设置为可发现模式 ─── */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    const uint8_t *mac = esp_bt_dev_get_address();
    ESP_LOGI(TAG, "蓝牙初始化完成");
    ESP_LOGI(TAG, "  设备名: %s", BT_DEVICE_NAME);
    ESP_LOGI(TAG, "  MAC:    %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "  等待客户端连接...");

    /* ─── 主循环: 接收蓝牙数据 ─── */
    while (1) {
        char *rx_data = NULL;

        if (xQueueReceive(g_bt_rx_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (rx_data) {
                ESP_LOGI(TAG, "蓝牙收到: \"%s\"", rx_data);

                /* 匹配指令码 */
                for (int i = 0; i < BIN_COUNT; i++) {
                    if (strcmp(rx_data, g_cmd_codes[i]) == 0) {
                        bin_cmd_t cmd = {
                            .bin_index = i,
                            .open = true,
                            .source = 1,  // 蓝牙
                        };
                        event_bus_send_cmd(&cmd, 0);
                        break;
                    }
                }
                free(rx_data);
            }
        }
    }

fail:
    ESP_LOGE(TAG, "蓝牙初始化失败, 任务退出");
    vTaskDelete(NULL);
}
