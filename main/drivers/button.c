/*
 * button.c
 * 按键驱动 — GPIO 中断 + FreeRTOS Task Notification 消抖
 */

#include "button.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "config.h"

static const char *TAG = "button";

static uint8_t  g_btn_pins[BUTTON_MAX_COUNT];
static uint8_t  g_btn_count = 0;
static TaskHandle_t g_target_task = NULL;

/* GPIO ISR — 发送通知给目标任务 */
static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t btn_idx = (uint32_t)arg;
    if (g_target_task != NULL) {
        /* 发送按键编号 (1-based) 作为通知值 */
        xTaskNotifyFromISR(g_target_task, btn_idx + 1, eSetValueWithOverwrite, NULL);
    }
}

esp_err_t button_init(const uint8_t *pins, uint8_t count)
{
    if (count > BUTTON_MAX_COUNT) count = BUTTON_MAX_COUNT;

    for (uint8_t i = 0; i < count; i++) {
        g_btn_pins[i] = pins[i];

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pins[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,   // 下降沿触发 (按下)
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        /* 注册 ISR */
        gpio_install_isr_service(0);
        gpio_isr_handler_add(pins[i], button_isr_handler, (void *)(uint32_t)i);
    }

    g_btn_count = count;
    ESP_LOGI(TAG, "%d 路按键初始化完成 (消抖=%dms)", count, BUTTON_DEBOUNCE_MS);
    return ESP_OK;
}

void button_register_task(TaskHandle_t task)
{
    g_target_task = task;
}

uint8_t button_wait_press(TickType_t timeout_ticks)
{
    uint32_t notified_value = 0;

    /* 等待任务通知 (来自 ISR) */
    if (xTaskNotifyWait(0x00, 0x0F, &notified_value, timeout_ticks) == pdTRUE) {
        uint8_t btn = (uint8_t)notified_value;

        /* 消抖: 等待 30ms 后确认电平 */
        vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));

        if (btn >= 1 && btn <= g_btn_count) {
            /* 再次读取引脚确认是否真的按下 (低电平 = 按下) */
            if (gpio_get_level(g_btn_pins[btn - 1]) == 0) {
                /* 等待按键释放 (松开), 防止重复触发 */
                while (gpio_get_level(g_btn_pins[btn - 1]) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                ESP_LOGD(TAG, "按键 %d 按下", btn);
                return btn;
            }
        }
    }
    return 0;  // 超时或无有效按键
}
