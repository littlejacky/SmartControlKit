#include "led_control.h"
#include "button_input.h"
#include "motor_driver.h" // 后续你可以扩展
#include <zephyr/kernel.h>

void my_button_event(button_index_t idx, bool pressed)
{
    static led_mode_t led_mode = LED_MODE_RED;
    static int motor_pwm_mode = 0;

    if (!pressed) return; // 只处理按下事件，松开可做长按等

    switch(idx) {
    case BUTTON_SW0:
        // 切换 LED1 不同 PWM 模式
        led_mode = (led_mode+1)%LED_MODE_NUM;
        led_control_set_mode(led_mode);
        printk("LED模式切换到: %d\n", led_mode);
        break;
    case BUTTON_SW1:
        motor_pwm_mode = (motor_pwm_mode+1)%MOTOR_VIB_MODE_NUM;
        motor_driver_set_mode(motor_pwm_mode);     // 例: 不同PWM
        printk("马达PWM模式切换到: %d\n", motor_pwm_mode);
        break;
    case BUTTON_SW2:
        printk("BUTTON_SW2 pressed\n");
        break;
    case BUTTON_SW3:
        printk("BUTTON_SW3 pressed\n");
        break;
    default: break;
    }
}


#define LED_THREAD_STACK_SIZE 512
#define MOTOR_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIORITY 5
#define MOTOR_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(led_stack, LED_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(motor_stack, MOTOR_THREAD_STACK_SIZE);
struct k_thread led_thread_data;
struct k_thread motor_thread_data;

void led_thread_fn(void *a, void *b, void *c) {
    while (1) {
        led_control_periodic();
        k_msleep(15);
    }
}

void motor_thread_fn(void *a, void *b, void *c) {
    while (1) {
        motor_driver_periodic();
    }
}

void main(void)
{
    led_control_init();
    motor_driver_init();
    button_input_init(my_button_event);

    k_thread_create(&led_thread_data, led_stack, LED_THREAD_STACK_SIZE,
                    led_thread_fn, NULL, NULL, NULL,
                    LED_THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&motor_thread_data, motor_stack, MOTOR_THREAD_STACK_SIZE,
                    motor_thread_fn, NULL, NULL, NULL,
                    MOTOR_THREAD_PRIORITY, 0, K_NO_WAIT);

    while (1) {
        k_msleep(1000); // 主线程空转，可做看门狗等
    }
}