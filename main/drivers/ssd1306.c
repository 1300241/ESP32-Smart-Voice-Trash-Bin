/*
 * ssd1306.c
 * SSD1306 128x64 OLED 实现
 *
 * 使用 u8g2 库 (olikraus/u8g2), 通过 ESP-IDF 原生 I2C 驱动
 * 适配 HAL: u8x8_byte_hw_i2c callback
 */

#include "ssd1306.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "config.h"
#include <string.h>

static const char *TAG = "ssd1306";

static u8g2_t g_u8g2;
static uint8_t g_i2c_addr;

/*
 * u8g2 ESP-IDF I2C 硬件字节回调
 * 这是 u8g2 与 ESP-IDF I2C 驱动之间的桥梁
 */
static uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    static uint8_t buffer[32];
    static uint8_t buf_idx;

    switch (msg) {
    case U8X8_MSG_BYTE_SEND: {
        /* arg_ptr = 数据数组, arg_int = 字节数 */
        uint8_t *data = (uint8_t *)arg_ptr;

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (g_i2c_addr << 1) | I2C_MASTER_WRITE, true);
        for (int i = 0; i < arg_int; i++) {
            i2c_master_write_byte(cmd, data[i], true);
        }
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        return (ret == ESP_OK) ? 1 : 0;

        break;
    }

    case U8X8_MSG_BYTE_INIT: {
        /* u8g2 初始化 HAL */
        break;
    }

    case U8X8_MSG_BYTE_SET_DC:
        /* 硬件 I2C 不需要 DC 线 */
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        break;

    default:
        return 0;
    }
    return 1;
}

/*
 * u8g2 GPIO/Delay 回调 (ESP-IDF I2C 不需要, 给空实现)
 */
static uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        /* SSD1306 复位引脚不需要, 软件 I2C 需要但硬件 I2C 不需要 */
        break;
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    case U8X8_MSG_DELAY_10MICRO:
        vTaskDelay(1);  // 约 1ms, 作为 10us 的近似
        break;
    case U8X8_MSG_DELAY_100NANO:
        break;  // 忽略 (硬件 I2C 自带时序)
    default:
        break;
    }
    return 1;
}

/* ─── 公共 API ─── */

esp_err_t ssd1306_init(uint8_t i2c_addr)
{
    g_i2c_addr = i2c_addr;

    /* 用 I2C 地址初始化 u8g2
     * U8G2_R0 = 正常方向, 无硬件旋转
     * u8x8_byte_hw_i2c = 硬件 I2C 通信
     * u8x8_gpio_and_delay = 延时回调
     */
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &g_u8g2,
        U8G2_R0,
        u8x8_byte_hw_i2c,
        u8x8_gpio_and_delay
    );

    /* 设置 I2C 地址 (u8g2 默认用 0x3C*2=0x78, 匹配我们的 0x3C) */
    // u8g2_SetI2CAddress 已处理 x2 问题
    u8g2_SetI2CAddress(&g_u8g2, g_i2c_addr);

    // 初始化
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0);

    // 设置字体 (默认使用中文兼容字体)
    u8g2_SetFont(&g_u8g2, u8g2_font_wqy12_t_gb2312a);
    u8g2_SetFontPosTop(&g_u8g2);

    // 启用 UTF-8 打印
    u8g2_EnableUTF8Print(&g_u8g2);

    // 清屏
    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SendBuffer(&g_u8g2);

    ESP_LOGI(TAG, "SSD1306 初始化完成 (128x64, I2C addr=0x%02X)", i2c_addr);
    return ESP_OK;
}

u8g2_t *ssd1306_get_u8g2(void)
{
    return &g_u8g2;
}

void ssd1306_clear(void)
{
    u8g2_ClearBuffer(&g_u8g2);
}

void ssd1306_flush(void)
{
    u8g2_SendBuffer(&g_u8g2);
}
