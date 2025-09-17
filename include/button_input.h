#include <stdbool.h>

#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

typedef enum {
    BUTTON_SW0 = 0,
    BUTTON_SW1,
    BUTTON_SW2,
    BUTTON_SW3,
    BUTTON_NUM
} button_index_t;

// 生产级: 每个按钮都传入index
typedef void (*button_event_cb_t)(button_index_t, bool pressed);
// bool pressed: true为按下，false为松开（可做长按逻辑）

int button_input_init(button_event_cb_t cb);

#endif