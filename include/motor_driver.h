#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H


typedef enum {
    MOTOR_VIB_OFF = 0,
	MOTOR_VIB_HEARTBEAT,
	MOTOR_VIB_TAP,
	MOTOR_VIB_LONG,
    MOTOR_VIB_MODE_NUM
} motor_vib_mode_t;

int motor_driver_init(void);
int motor_driver_set_mode(motor_vib_mode_t mode); // 选择振动模式
int motor_driver_periodic(void); // 可用于周期性处理

#endif