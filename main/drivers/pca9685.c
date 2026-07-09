/*
 * pca9685.c
 * PCA9685 16通道 12-bit PWM 舵机驱动板 实现
 */

#include "pca9685.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "pca9685";

static i2c_port_t    g_i2c_port;
static uint8_t       g_i2c_addr;
static bool          g_initialized = false;

/* PCA9685 寄存器 */
#define PCA9685_MODE1       0x00
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06
#define PCA9685_ALL_LED_ON_L  0xFA
#define PCA9685_ALL_LED_OFF_L 0xFC

#define PCA9685_OSC_CLOCK   27000000  // 内部振荡器 27MHz
#define PCA9685_PWM_RESOLUTION  4096  // 12-bit

/* I2C 写单字节寄存器 */
static esp_err_t i2c_write8(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(g_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* I2C 读单字节寄存器 */
static esp_err_t i2c_read8(uint8_t reg, uint8_t *val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(g_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* ─── 公共 API ─── */

esp_err_t pca9685_i2c_init(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint32_t freq_hz)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = freq_hz,
    };
    esp_err_t ret = i2c_param_config(port, &conf);
    if (ret != ESP_OK) return ret;

    ret = i2c_driver_install(port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) return ret;

    g_i2c_port = port;
    return ESP_OK;
}

esp_err_t pca9685_init(uint8_t i2c_addr)
{
    g_i2c_addr = i2c_addr;

    /* 复位 PCA9685 */
    esp_err_t ret = i2c_write8(PCA9685_MODE1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "无法与 PCA9685 通信 (addr=0x%02X)", i2c_addr);
        return ret;
    }

    /* 设置 PWM 频率为 50Hz (舵机标准)
     * prescale = round(osc_clock / (4096 * freq)) - 1
     *          = round(27,000,000 / (4096 * 50)) - 1
     *          = round(131.8) - 1 ≈ 131
     */
    float prescale_val = roundf((float)PCA9685_OSC_CLOCK /
                                (PCA9685_PWM_RESOLUTION * 50.0f)) - 1;
    uint8_t prescale = (uint8_t)(prescale_val < 3 ? 3 : prescale_val);

    // 进入 sleep 模式以设置 prescale
    uint8_t mode1;
    ret = i2c_read8(PCA9685_MODE1, &mode1);
    if (ret != ESP_OK) return ret;

    ret = i2c_write8(PCA9685_MODE1, (mode1 & 0x7F) | 0x10); // sleep
    if (ret != ESP_OK) return ret;

    ret = i2c_write8(PCA9685_PRESCALE, prescale);
    if (ret != ESP_OK) return ret;

    ret = i2c_write8(PCA9685_MODE1, mode1); // 退出 sleep
    if (ret != ESP_OK) return ret;

    // 等待振荡器稳定
    vTaskDelay(pdMS_TO_TICKS(5));

    ret = i2c_write8(PCA9685_MODE1, mode1 | 0x20); // auto-increment
    if (ret != ESP_OK) return ret;

    g_initialized = true;
    ESP_LOGI(TAG, "PCA9685 初始化完成 (prescale=%d, PWM=50Hz)", prescale);
    return ESP_OK;
}

esp_err_t pca9685_set_pwm(uint8_t channel, uint16_t off)
{
    if (!g_initialized) return ESP_ERR_INVALID_STATE;
    if (channel > 15)   return ESP_ERR_INVALID_ARG;

    uint8_t reg = PCA9685_LED0_ON_L + 4 * channel;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, 0x00, true);     // ON low
    i2c_master_write_byte(cmd, 0x00, true);     // ON high
    i2c_master_write_byte(cmd, off & 0xFF, true);      // OFF low
    i2c_master_write_byte(cmd, (off >> 8) & 0x0F, true); // OFF high
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(g_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t pca9685_set_servo_angle(uint8_t channel, uint8_t angle)
{
    if (angle > 180) angle = 180;

    // 50Hz PWM, 舵机标准脉宽: 0.5ms (0°) ~ 2.5ms (180°)
    // 0°   = 0.5ms  → off = 4096 * 0.5  / 20 ≈ 102
    // 180° = 2.5ms  → off = 4096 * 2.5  / 20 ≈ 512
    // 线性插值
    uint16_t off = (uint16_t)(102 + (angle / 180.0f) * 410.0f);

    return pca9685_set_pwm(channel, off);
}

esp_err_t pca9685_reset_channel(uint8_t channel)
{
    return pca9685_set_pwm(channel, 0);
}
