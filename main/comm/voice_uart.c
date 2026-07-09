/*
 * voice_uart.c
 * 天问 ASRPRO 语音模块 UART 通信 实现
 *
 * 协议:
 *   发送: <prefix><code>\n   (如 "O100\n", "C103\n")
 *   接收: "<code>\r\n"       (如 "100\r\n")
 *
 * ESP32 UART2: TX=17, RX=16, Baud=115200
 */

#include "voice_uart.h"
#include "event_bus.h"
#include "config.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "voice_uart";

/* UART 接收缓冲区 */
#define VOICE_UART_BUF_SIZE    128

/* 4 个垃圾桶对应的指令码 */
static const char *g_cmd_codes[BIN_COUNT] = {
    CMD_RECYCLABLE,   // "100"
    CMD_KITCHEN,      // "101"
    CMD_HAZARDOUS,    // "102"
    CMD_OTHER,        // "103"
};

esp_err_t voice_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = VOICE_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t ret = uart_driver_install(VOICE_UART_NUM,
                                         VOICE_UART_BUF_SIZE * 2,
                                         0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 驱动安装失败");
        return ret;
    }

    ret = uart_param_config(VOICE_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 参数配置失败");
        return ret;
    }

    ret = uart_set_pin(VOICE_UART_NUM, VOICE_UART_TX, VOICE_UART_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 引脚配置失败");
        return ret;
    }

    ESP_LOGI(TAG, "语音模块 UART 初始化完成 (UART%d, TX=%d, RX=%d, %d baud)",
             VOICE_UART_NUM, VOICE_UART_TX, VOICE_UART_RX, VOICE_UART_BAUD);
    return ESP_OK;
}

esp_err_t voice_uart_send_cmd(const char *cmd_code, bool is_open)
{
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%c%s\n",
                       is_open ? CMD_OPEN_PREFIX : CMD_CLOSE_PREFIX,
                       cmd_code);

    int sent = uart_write_bytes(VOICE_UART_NUM, buf, len);
    if (sent < 0) {
        ESP_LOGE(TAG, "发送语音指令失败");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "发送语音指令: %s", buf);
    return ESP_OK;
}

/*
 * 语音模块接收任务
 * 轮询 UART, 按行解析, 匹配指令码
 */
void voice_uart_task(void *arg)
{
    ESP_LOGI(TAG, "语音模块接收任务启动");

    /* 初始化 UART */
    ESP_ERROR_CHECK(voice_uart_init());

    char line_buf[32];
    uint8_t line_idx = 0;

    while (1) {
        uint8_t ch;
        int len = uart_read_bytes(VOICE_UART_NUM, &ch, 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            /* 按行处理: \n 或 \r 表示行结束 */
            if (ch == '\n' || ch == '\r') {
                if (line_idx > 0) {
                    line_buf[line_idx] = '\0';

                    /* 去除尾部空白 */
                    while (line_idx > 0 && isspace((unsigned char)line_buf[line_idx - 1])) {
                        line_buf[--line_idx] = '\0';
                    }

                    ESP_LOGD(TAG, "收到语音指令: \"%s\"", line_buf);

                    /* 匹配指令码 */
                    for (int i = 0; i < BIN_COUNT; i++) {
                        if (strcmp(line_buf, g_cmd_codes[i]) == 0) {
                            ESP_LOGI(TAG, "语音识别: [桶%d] %s", i, line_buf);

                            bin_cmd_t cmd = {
                                .bin_index = i,
                                .open = true,
                                .source = 0,  // 语音
                            };
                            event_bus_send_cmd(&cmd, 0);

                            /* 通知语音模块播放对应语音 (ASRPRO 自己会播放回复) */
                            break;
                        }
                    }

                    line_idx = 0;
                }
            } else if (line_idx < sizeof(line_buf) - 1) {
                line_buf[line_idx++] = ch;
            }
        }
    }
}
