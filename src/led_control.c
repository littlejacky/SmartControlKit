#include "led_control.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <math.h>        // for sin/pow

#define M_PI 3.14159265358979323846

// 呼吸灯参数
#define BREATH_PERIOD_MS    2000
#define BREATH_UPDATE_MS    10
#define BREATH_STEPS        (BREATH_PERIOD_MS / BREATH_UPDATE_MS)

static struct pwm_dt_spec led_red = PWM_DT_SPEC_GET(DT_ALIAS(ledred));
static struct pwm_dt_spec led_blue = PWM_DT_SPEC_GET(DT_ALIAS(ledblue));

static uint8_t current_red = 0;
static uint8_t current_blue = 0;
static led_mode_t current_mode = LED_MODE_RED;

int led_control_init(void)
{
    if (device_is_ready(led_red.dev) && device_is_ready(led_blue.dev)) {
        printk("LED Red PWM period: %u ns\n", led_red.period);
        printk("LED Blue PWM period: %u ns\n", led_blue.period);
    }
    led_control_set_color(0, 0);
    return 0;
}

void led_control_set_color(uint8_t red_percent, uint8_t blue_percent)
{
    if (red_percent > 100) red_percent = 100;
    if (blue_percent > 100) blue_percent = 100;
    current_red = red_percent;
    current_blue = blue_percent;
    pwm_set_dt(&led_red, led_red.period, (red_percent * led_red.period) / 100);
    pwm_set_dt(&led_blue, led_blue.period, (blue_percent * led_blue.period) / 100);
}

int led_control_set_mode(led_mode_t mode)
{
    current_mode = mode;
    return 0;
}

// 呼吸灯丝滑算法：正弦+伽马校正
static uint8_t sine_breathe(float phase)
{
    // phase: 0~2π, 输出0~100的“视觉均匀”亮度
    float raw = (sin(phase - M_PI/2) + 1.0f) / 2.0f; // sine, M_PI/2初始灭灯，0~1
    float gamma = 2.0f; // 亮度均匀视觉修正
    raw = powf(raw, gamma); // gamma校正
    uint8_t pct = (uint8_t)(raw * 100.0f);
    return pct;
}

int led_control_periodic(void)
{
    static int breath_cnt = 0;      // 呼吸步进
    static int blink = 0;           // 闪烁计数
    switch (current_mode) {
    case LED_MODE_BREATH: {
        breath_cnt = (breath_cnt + 1) % BREATH_STEPS;
        float phase = ((float)breath_cnt / (float)BREATH_STEPS) * 2.0f * M_PI; // 0~2π
        uint8_t blue = sine_breathe(phase);           // 蓝色亮度
        uint8_t red  = 100 - blue;                    // 红色反向
        led_control_set_color(red, blue);
        k_msleep(BREATH_UPDATE_MS);                   // 丝滑刷新，每步10ms
        }
        break;
    case LED_MODE_USER_BREATH: {
        breath_cnt = (breath_cnt + 1) % BREATH_STEPS;
        float phase = ((float)breath_cnt / (float)BREATH_STEPS) * 2.0f * M_PI;
 
        float raw = (sin(phase - M_PI/2) + 1.0f) / 2.0f;   // 0~1
        float gamma = 2.0f;
        raw = powf(raw, gamma);
 
        // 指定最小亮度
        float min_level = 0.2f;              // 最小为20%
        uint8_t red = (uint8_t)( (min_level + (1.0f - min_level)*raw) * 100 );
        uint8_t blue = (uint8_t)(red * 0.3f);   // 粉色（红主蓝辅）
 
        led_control_set_color(red, blue);
        k_msleep(BREATH_UPDATE_MS);
        }
        break;

    case LED_MODE_RED:
        led_control_set_color(100, 0); break;
    case LED_MODE_BLUE:
        led_control_set_color(0, 100); break;
    case LED_MODE_PURPLE:
        led_control_set_color(50, 50); break;
    case LED_MODE_FLASH:
        if (blink++ % 2)
            led_control_set_color(100, 0);
        else
            led_control_set_color(0, 100);
        k_msleep(250);   // 每250ms闪烁
        break;
    case LED_MODE_USER:
        led_control_set_color(80, 60); break;
    default:
        led_control_set_color(0, 0); break;
    }
    return 0;
}