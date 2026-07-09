/*
 * hc_sr04.h
 * HC-SR04 超声波测距传感器
 * 使用 RMT 外设精确捕获 ECHO 高电平脉宽
 *
 * 支持 4 路传感器, 分时触发采集
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 超声波传感器描述 */
typedef struct {
    uint8_t  index;       /* 0-3 */
    uint8_t  trig_pin;    /* TRIG GPIO */
    uint8_t  echo_pin;    /* ECHO GPIO */
} hc_sr04_t;

/* 初始化全部 4 路超声波传感器
 * 引脚从 config.h 宏读取
 */
esp_err_t hc_sr04_init_all(void);

/* 读取指定传感器的距离
 * @param index   传感器编号 0-3
 * @param dist_cm 输出, 距离单位 cm, 超时返回 0
 * @return ESP_OK 成功
 */
esp_err_t hc_sr04_get_distance(uint8_t index, uint16_t *dist_cm);

/* 读取全部 4 路距离
 * @param dist 输出数组, 长度至少 4
 */
esp_err_t hc_sr04_get_all(uint16_t dist[4]);

#ifdef __cplusplus
}
#endif
