#include "button_input.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define BUTTON_SW0_NODE DT_ALIAS(sw0)
#define BUTTON_SW1_NODE DT_ALIAS(sw1)
#define BUTTON_SW2_NODE DT_ALIAS(sw2)
#define BUTTON_SW3_NODE DT_ALIAS(sw3)

static const struct gpio_dt_spec buttons[BUTTON_NUM] = {
    GPIO_DT_SPEC_GET(BUTTON_SW0_NODE, gpios),
    GPIO_DT_SPEC_GET(BUTTON_SW1_NODE, gpios),
    GPIO_DT_SPEC_GET(BUTTON_SW2_NODE, gpios),
    GPIO_DT_SPEC_GET(BUTTON_SW3_NODE, gpios),
};

static struct gpio_callback btn_cb_data[BUTTON_NUM];
static button_event_cb_t g_btn_cb = NULL;

// 生产级支持所有SWx的callback
static void button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    for (int i = 0; i < BUTTON_NUM; ++i) {
        if ((pins & BIT(buttons[i].pin)) && g_btn_cb) {
            bool pressed = !gpio_pin_get_dt(&buttons[i]);     // Active low
            g_btn_cb(i, pressed);
        }
    }
}

int button_input_init(button_event_cb_t cb)
{
    g_btn_cb = cb;
    for(int i = 0; i < BUTTON_NUM; ++i) {
        if (!device_is_ready(buttons[i].port)) {
            printk("Button %d device not ready!\n", i);
        }
        gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_BOTH); // 按下松开都响应

        gpio_init_callback(&btn_cb_data[i], button_handler, BIT(buttons[i].pin));
        gpio_add_callback(buttons[i].port, &btn_cb_data[i]);
    }
    return 0;
}