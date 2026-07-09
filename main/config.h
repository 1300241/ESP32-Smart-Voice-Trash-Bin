/*
 * config.h
 * 全局硬件引脚定义和系统参数配置
 * 可通过 idf.py menuconfig → "智能分类垃圾桶配置" 菜单调整
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ───── GPIO 引脚定义 ───── */

// 按键 (INPUT_PULLUP, 按下低电平)
#define BTN1_PIN            CONFIG_BTN1_PIN
#define BTN2_PIN            CONFIG_BTN2_PIN
#define BTN3_PIN            CONFIG_BTN3_PIN
#define BTN4_PIN            CONFIG_BTN4_PIN

// 超声波传感器 HC-SR04 (TRIG, ECHO)
#define ULTRASONIC1_TRIG    CONFIG_ULTRASONIC1_TRIG
#define ULTRASONIC1_ECHO    CONFIG_ULTRASONIC1_ECHO
#define ULTRASONIC2_TRIG    CONFIG_ULTRASONIC2_TRIG
#define ULTRASONIC2_ECHO    CONFIG_ULTRASONIC2_ECHO
#define ULTRASONIC3_TRIG    CONFIG_ULTRASONIC3_TRIG
#define ULTRASONIC3_ECHO    CONFIG_ULTRASONIC3_ECHO
#define ULTRASONIC4_TRIG    CONFIG_ULTRASONIC4_TRIG
#define ULTRASONIC4_ECHO    CONFIG_ULTRASONIC4_ECHO

// 语音模块 UART
#define VOICE_UART_NUM      CONFIG_VOICE_UART_NUM
#define VOICE_UART_TX       CONFIG_VOICE_UART_TX
#define VOICE_UART_RX       CONFIG_VOICE_UART_RX
#define VOICE_UART_BAUD     CONFIG_VOICE_UART_BAUD

// I2C (PCA9685 + PCF8574 + SSD1306 共享总线)
#define I2C_PORT            CONFIG_I2C_PORT
#define I2C_SDA             CONFIG_I2C_SDA
#define I2C_SCL             CONFIG_I2C_SCL
#define I2C_FREQ_HZ         CONFIG_I2C_FREQ_HZ

/* ───── I2C 设备地址 ───── */
#define PCA9685_ADDR        0x40    // PCA9685 默认地址
#define PCF8574_ADDR        0x20    // PCF8574 地址
#define SSD1306_ADDR        0x3C    // SSD1306 OLED 地址

/* ───── PCA9685 舵机通道 ───── */
#define SERVO1_CHANNEL      CONFIG_SERVO1_CHANNEL
#define SERVO2_CHANNEL      CONFIG_SERVO2_CHANNEL
#define SERVO3_CHANNEL      CONFIG_SERVO3_CHANNEL
#define SERVO4_CHANNEL      CONFIG_SERVO4_CHANNEL

/* ───── 超声波最大距离 (cm) ───── */
#define ULTRASONIC_MAX_DIST_CM   200

/* ───── 垃圾桶数量 ───── */
#define BIN_COUNT           4

/* ───── 舵机角度 ───── */
#define SERVO_OPEN_ANGLE    CONFIG_SERVO_OPEN_ANGLE
#define SERVO_CLOSE_ANGLE   CONFIG_SERVO_CLOSE_ANGLE

/* ───── 自动关闭延时 (秒) ───── */
#define AUTO_CLOSE_DELAY_S  CONFIG_AUTO_CLOSE_DELAY_S

/* ───── 超声波满桶阈值 (cm, 小于此值判定为满) ───── */
#define BIN_FULL_DIST_CM    CONFIG_BIN_FULL_DIST_CM

/* ───── OLED 刷新间隔 (ms) ───── */
#define DISPLAY_REFRESH_MS  CONFIG_DISPLAY_REFRESH_MS

/* ───── 按键消抖时间 (ms) ───── */
#define BUTTON_DEBOUNCE_MS  30

/* ───── 垃圾桶标签名称 ───── */
#define BIN_LABEL_0         "可回收垃圾"
#define BIN_LABEL_1         "厨余垃圾"
#define BIN_LABEL_2         "有害垃圾"
#define BIN_LABEL_3         "其他垃圾"

/* ───── 语音/蓝牙指令码 ───── */
#define CMD_OPEN_PREFIX     'O'
#define CMD_CLOSE_PREFIX    'C'
#define CMD_RECYCLABLE      "100"
#define CMD_KITCHEN         "101"
#define CMD_HAZARDOUS       "102"
#define CMD_OTHER           "103"

#ifdef __cplusplus
}
#endif
