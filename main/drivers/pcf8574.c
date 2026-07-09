/*
 * pcf8574.c
 * PCF8574 8-bit I2C IO 扩展芯片 实现
 * 用于连接 4 路红外反射传感器 (P0-P3)
 */

#include "pcf8574.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "config.h"

static const char *TAG = "pcf8574";

static i2c_port_t g_port;
static uint8_t    g_addr;
static bool       g_initialized = false;

/* 底层 I2C 读一个字节 (不检查 g_initialized, 供 init 调用) */
static esp_err_t raw_read(uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(g_port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t pcf8574_init(uint8_t i2c_addr)
{
    g_addr = i2c_addr;
    g_port = I2C_PORT;

    /* 发送一次读操作检测设备是否存在 */
    uint8_t dummy;
    esp_err_t ret = raw_read(&dummy);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "无法与 PCF8574 通信 (addr=0x%02X)", i2c_addr);
        return ret;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "PCF8574 初始化完成 (addr=0x%02X)", i2c_addr);
    return ESP_OK;
}

esp_err_t pcf8574_read(uint8_t *data)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return raw_read(data);
}

esp_err_t pcf8574_read_pin(uint8_t pin, bool *level)
{
    if (pin > 7) return ESP_ERR_INVALID_ARG;

    uint8_t data;
    esp_err_t ret = pcf8574_read(&data);
    if (ret != ESP_OK) return ret;

    *level = (data >> pin) & 0x01;
    return ESP_OK;
}
