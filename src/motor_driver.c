#include <zephyr/logging/log.h>
#include "motor_driver.h"

LOG_MODULE_REGISTER(motor_driver, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "motor_driver.h"

#define PWM_VIB DT_ALIAS(motor0)

static const struct pwm_dt_spec pwm_vibrator = PWM_DT_SPEC_GET(PWM_VIB);
static motor_vib_mode_t current_mode = MOTOR_VIB_HEARTBEAT;

int motor_driver_init(void) {
	// 可扩展初始化
    return 0;
}

int motor_driver_set_mode(motor_vib_mode_t mode) {
	current_mode = mode;
}

// 心跳模式
static void vibrate_heartbeat(const struct pwm_dt_spec *pwm) {
	// 使用设备树中的周期，只设置占空比
	pwm_set_dt(pwm, pwm->period, (pwm->period * 95) / 100);  // 95%占空比
	k_msleep(60);
	pwm_set_dt(pwm, pwm->period, 0);  // 关闭
	k_msleep(80);
	pwm_set_dt(pwm, pwm->period, (pwm->period * 70) / 100);  // 70%占空比
	k_msleep(40);
	pwm_set_dt(pwm, pwm->period, 0);
	k_msleep(820);
}

// 轻拍模式
static void vibrate_tap(const struct pwm_dt_spec *pwm) {
	pwm_set_dt(pwm, pwm->period, (pwm->period * 90) / 100);  // 90%占空比
	k_msleep(50);
	pwm_set_dt(pwm, pwm->period, 0);
	k_msleep(200);
}

// 长震模式
static void vibrate_long(const struct pwm_dt_spec *pwm) {
	pwm_set_dt(pwm, pwm->period, (pwm->period * 90) / 100);
	k_msleep(400);
	pwm_set_dt(pwm, pwm->period, 0);
	k_msleep(600);
}

// 关闭模式
static void vibrate_stop(const struct pwm_dt_spec *pwm) {
	pwm_set_dt(pwm, pwm->period, 0);
}

int motor_driver_periodic(void) {
	if (!device_is_ready(pwm_vibrator.dev)) {
		LOG_ERR("PWM vibrator device not ready!");
		k_msleep(1000);
		return;
	}
	switch (current_mode) {
		case MOTOR_VIB_HEARTBEAT:
			vibrate_heartbeat(&pwm_vibrator);
			break;
		case MOTOR_VIB_TAP:
			vibrate_tap(&pwm_vibrator);
			break;
		case MOTOR_VIB_LONG:
			vibrate_long(&pwm_vibrator);
			break;
		default:
            vibrate_stop(&pwm_vibrator);
			break;
	}
    return 0;
}
