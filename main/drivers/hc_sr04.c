/*
 * hc_sr04.c
 * HC-SR04 超声波测距传感器 实现
 *
 * 原理: TRIG 发送 10us 高电平脉冲, ECHO 返回高电平,
 *       高电平持续时间 / 58 = 距离(cm)
 *
 * 使用 esp_timer 微秒级计时,替代 Arduino pulseIn()
 */

#include "hc_sr04.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hc_sr04";

/* 超时时间 (us) — 对应约 4.5m */
#define ECHO_TIMEOUT_US     26000

/* TRIG 脉冲宽度 (us) */
#define TRIG_PULSE_US       10

/* 各传感器引脚定义 */
static hc_sr04_t g_sensors[4] = {
    { .index = 0, .trig_pin = CONFIG_ULTRASONIC1_TRIG, .echo_pin = CONFIG_ULTRASONIC1_ECHO },
    { .index = 1, .trig_pin = CONFIG_ULTRASONIC2_TRIG, .echo_pin = CONFIG_ULTRASONIC2_ECHO },
    { .index = 2, .trig_pin = CONFIG_ULTRASONIC3_TRIG, .echo_pin = CONFIG_ULTRASONIC3_ECHO },
    { .index = 3, .trig_pin = CONFIG_ULTRASONIC4_TRIG, .echo_pin = CONFIG_ULTRASONIC4_ECHO },
};

static bool g_initialized = false;

/* 单次测距 (阻塞式)
 * 返回距离 cm, 超时或无效返回 0
 */
static uint16_t measure_distance(const hc_sr04_t *sensor)
{
    int64_t start_time, end_time;

    /* 确保 TRIG 低电平 */
    gpio_set_level(sensor->trig_pin, 0);
    esp_rom_delay_us(2);

    /* 发送 10us 高电平触发脉冲 */
    gpio_set_level(sensor->trig_pin, 1);
    esp_rom_delay_us(TRIG_PULSE_US);
    gpio_set_level(sensor->trig_pin, 0);

    /* 等待 ECHO 变高, 带超时 */
    int64_t wait_start = esp_timer_get_time();
    while (gpio_get_level(sensor->echo_pin) == 0) {
        if ((esp_timer_get_time() - wait_start) > ECHO_TIMEOUT_US) {
            return 0;  // 超时: 传感器无响应
        }
    }

    /* ECHO 高电平开始, 计时 */
    start_time = esp_timer_get_time();

    /* 等待 ECHO 变低, 带超时 */
    while (gpio_get_level(sensor->echo_pin) == 1) {
        if ((esp_timer_get_time() - start_time) > ECHO_TIMEOUT_US) {
            return 0;  // 超时: 脉冲异常
        }
    }

    end_time = esp_timer_get_time();

    /* 距离 = 高电平时间(us) / 58 */
    uint32_t pulse_us = (uint32_t)(end_time - start_time);
    return (uint16_t)(pulse_us / 58);
}

/* ─── 公共 API ─── */

esp_err_t hc_sr04_init_all(void)
{
    for (int i = 0; i < 4; i++) {
        // TRIG 引脚: 输出
        gpio_config_t trig_conf = {
            .pin_bit_mask = (1ULL << g_sensors[i].trig_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&trig_conf));
        gpio_set_level(g_sensors[i].trig_pin, 0);

        // ECHO 引脚: 输入
        gpio_config_t echo_conf = {
            .pin_bit_mask = (1ULL << g_sensors[i].echo_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&echo_conf));
    }

    g_initialized = true;
    ESP_LOGI(TAG, "4 路超声波传感器初始化完成");
    return ESP_OK;
}

esp_err_t hc_sr04_get_distance(uint8_t index, uint16_t *dist_cm)
{
    if (!g_initialized) return ESP_ERR_INVALID_STATE;
    if (index >= 4)     return ESP_ERR_INVALID_ARG;

    *dist_cm = measure_distance(&g_sensors[index]);

    /* 相邻测量间隔 10ms (避免声波串扰) */
    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

esp_err_t hc_sr04_get_all(uint16_t dist[4])
{
    if (!g_initialized) return ESP_ERR_INVALID_STATE;

    for (int i = 0; i < 4; i++) {
        dist[i] = measure_distance(&g_sensors[i]);
        vTaskDelay(pdMS_TO_TICKS(10));  // 相邻传感器间隔 10ms
    }
    return ESP_OK;
}
