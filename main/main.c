/*
 * main.c
 * ESP32 智能语音分类垃圾桶 — 主入口
 *
 * 任务架构:
 *   - SensorTask:  红外(PFC8574) + 超声波(HC-SR04) 采集
 *   - ButtonTask:  四路按键扫描 (GPIO ISR)
 *   - VoiceTask:   天问 ASRPRO 语音模块 (UART)
 *   - BTHostTask:  蓝牙 SPP 通信
 *   - ServoTask:   PCA9685 舵机控制 + 自动关闭计时
 *   - DisplayTask: SSD1306 OLED 状态刷新
 *
 * 模块间通过 Event Bus (Queue + Event Group) 解耦通信
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "config.h"
#include "event_bus.h"
#include "pca9685.h"
#include "pcf8574.h"
#include "hc_sr04.h"
#include "ssd1306.h"
#include "button.h"
#include "bin_manager.h"
#include "voice_uart.h"
#include "bluetooth_spp.h"
#include "display.h"

static const char *TAG = "main";

/* ─── 任务栈大小和优先级 ─── */
#define SENSOR_TASK_STACK       3072
#define SENSOR_TASK_PRIO        5

#define BUTTON_TASK_STACK       2048
#define BUTTON_TASK_PRIO        4

#define VOICE_TASK_STACK        3072
#define VOICE_TASK_PRIO         4

#define BT_TASK_STACK           4096
#define BT_TASK_PRIO            3

#define SERVO_TASK_STACK        2560
#define SERVO_TASK_PRIO         6

#define DISPLAY_TASK_STACK      3072
#define DISPLAY_TASK_PRIO       2


/*
 * 硬件初始化: I2C 总线 + 所有外设
 */
static esp_err_t hardware_init(void)
{
    esp_err_t ret;

    /* ─── 1. 初始化 I2C 主线 ─── */
    ret = pca9685_i2c_init(I2C_PORT, I2C_SDA, I2C_SCL, I2C_FREQ_HZ);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C 总线初始化失败");
        return ret;
    }
    ESP_LOGI(TAG, "I2C 总线已初始化 (SDA=%d, SCL=%d, FREQ=%d Hz)", I2C_SDA, I2C_SCL, I2C_FREQ_HZ);

    /* ─── 2. PCA9685 舵机驱动板 ─── */
    ret = pca9685_init(PCA9685_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 初始化失败");
        return ret;
    }
    ESP_LOGI(TAG, "PCA9685 已初始化 (addr=0x%02X)", PCA9685_ADDR);

    /* ─── 3. PCF8574 IO 扩展 (红外传感器) ─── */
    ret = pcf8574_init(PCF8574_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCF8574 初始化失败");
        return ret;
    }
    ESP_LOGI(TAG, "PCF8574 已初始化 (addr=0x%02X)", PCF8574_ADDR);

    /* ─── 4. 超声波传感器 HC-SR04 ─── */
    ret = hc_sr04_init_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "超声波传感器初始化失败");
        return ret;
    }
    ESP_LOGI(TAG, "4 路超声波传感器已初始化");

    /* ─── 5. SSD1306 OLED ─── */
    ret = ssd1306_init(SSD1306_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 OLED 初始化失败");
        return ret;
    }
    ESP_LOGI(TAG, "SSD1306 OLED 已初始化 (addr=0x%02X)", SSD1306_ADDR);

    /* ─── 6. 按键初始化 ─── */
    uint8_t btn_pins[BIN_COUNT] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN };
    ret = button_init(btn_pins, BIN_COUNT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败");
        return ret;
    }
    ESP_LOGI(TAG, "4 路按键已初始化");

    return ESP_OK;
}


void app_main(void)
{
    esp_err_t ret;

    /* ─── 0. NVS 初始化 (蓝牙协议栈依赖) ─── */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ─── 1. 硬件初始化 ─── */
    ESP_ERROR_CHECK(hardware_init());

    /* ─── 2. 事件总线初始化 ─── */
    ESP_ERROR_CHECK(event_bus_init());
    ESP_LOGI(TAG, "事件总线已初始化");

    /* ─── 3. 垃圾桶管理器初始化 ─── */
    ESP_ERROR_CHECK(bin_manager_init());
    ESP_LOGI(TAG, "垃圾桶管理器已初始化");

    /* ─── 4. 创建 FreeRTOS 任务 ─── */
    xTaskCreate(sensor_task,       "sensor",  SENSOR_TASK_STACK,  NULL, SENSOR_TASK_PRIO,  NULL);
    xTaskCreate(button_task,       "button",  BUTTON_TASK_STACK,  NULL, BUTTON_TASK_PRIO,  NULL);
    xTaskCreate(voice_uart_task,   "voice",   VOICE_TASK_STACK,   NULL, VOICE_TASK_PRIO,   NULL);
    xTaskCreate(bluetooth_spp_task,"bt_spp",  BT_TASK_STACK,      NULL, BT_TASK_PRIO,      NULL);
    xTaskCreate(servo_task,        "servo",   SERVO_TASK_STACK,   NULL, SERVO_TASK_PRIO,   NULL);
    xTaskCreate(display_task,      "display", DISPLAY_TASK_STACK, NULL, DISPLAY_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "  智能语音分类垃圾桶 启动完成");
    ESP_LOGI(TAG, "  唤醒词: 聪明蛋");
    ESP_LOGI(TAG, "  蓝牙设备名: ESP32_SmartBin");
    ESP_LOGI(TAG, "==========================================");

    /* ─── 5. 主任务进入空闲 ─── */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
