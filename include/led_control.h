#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_RED,
    LED_MODE_BLUE,
    LED_MODE_PURPLE,
    LED_MODE_BREATH,
    LED_MODE_FLASH,
    LED_MODE_USER,
    LED_MODE_USER_BREATH,
    LED_MODE_NUM
} led_mode_t;

// 初始化LED硬件
int led_control_init(void);

// 设置混色（占空比百分比），可灵活调用
void led_control_set_color(uint8_t red_percent, uint8_t blue_percent);

// 设置当前展示模式（预设的呼吸、危险等）
int led_control_set_mode(led_mode_t mode);

// 必须周期调用，一般main里刷新（可放定时器任务/线程里）
int led_control_periodic(void);

#endif