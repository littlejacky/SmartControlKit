#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>
#include <string.h>

#define I2C_NODE    DT_ALIAS(i2c0)
#define MPU_ADDR    0x68
#define ACCEL_SCALE 16384.0f
#define SAMPLING_INTERVAL_MS 20

// ===== 戒指优化的双击参数 =====
#define TAP_SPIKE_TH        0.30f   // 突变阈值（稍微放宽）
#define TAP_PEAK_ABS_TH     1.20f   // 绝对值阈值（稍微放宽）
#define TAP_MIN_DURATION_MS 25      // 最小冲击持续时间
#define TAP_COOLDOWN_MS     180     // 单次tap冷却
#define DOUBLE_TAP_MIN_MS   100     // 双击最小间隔
#define DOUBLE_TAP_MAX_MS   500     // 双击最大间隔
#define DOUBLE_TAP_COOLDOWN 1000    // 双击事件冷却

// ===== 戒指专用静止判定参数 =====
#define STATIC_WIN          6       // 平衡响应速度和稳定性
#define STATIC_VAR_TH       0.040f  // 放宽方差阈值（考虑手指微动）
#define STATIC_DIFF_TH      0.15f   // 放宽差值阈值
#define GRAVITY_TOLERANCE   0.18f   // 放宽重力偏差容忍度
#define POSTURE_STABLE_TH   0.25f   // 放宽姿态稳定阈值

// ===== 方向性检测参数 =====
#define AXIS_DOMINANCE_MIN   0.8f   // 主轴最小强度
#define AXIS_DOMINANCE_RATIO 1.4f   // 主轴优势比例（降低要求）
#define TAP_CONSISTENCY_RATIO 2.5f  // 双击一致性比例（略放宽）

// ===== 滑动检测参数 =====
#define SMOOTH_WIN          3       // 平滑窗口大小
#define CALIBRATION_SAMPLES 50      // 自校准样本数

// ===== 常量定义 =====
#define STAT_WIN_INIT_MIN   999.0f
#define STAT_WIN_INIT_MAX  -999.0f
#define GRAVITY_NOMINAL     1.0f

typedef struct { 
    float buff[STATIC_WIN]; 
    int head, len; 
} FwStaticWin;

typedef struct {
    float buff[SMOOTH_WIN];
    int head, len;
} SmoothWin;

// ===== 平滑窗口操作 =====
static void smooth_push(SmoothWin *w, float v) {
    w->buff[w->head] = v;
    w->head = (w->head + 1) % SMOOTH_WIN;
    if (w->len < SMOOTH_WIN) w->len++;
}

static float smooth_avg(SmoothWin *w) {
    if (w->len == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < w->len; i++) {
        sum += w->buff[i];
    }
    return sum / w->len;
}

// ===== 自校准结构 =====
typedef struct {
    float gravity_ref;
    int sample_count;
    float sum;
    bool calibrated;
} CalibrationState;

// ===== 改进的窗口操作 =====
static void win_push(FwStaticWin *w, float v) {
    w->buff[w->head] = v; 
    w->head = (w->head + 1) % STATIC_WIN;
    if (w->len < STATIC_WIN) w->len++;
}

static void win_stat(FwStaticWin *w, float *mean, float *var, float *minv, float *maxv) {
    int i, l = w->len; 
    *mean = 0; *var = 0; 
    *maxv = STAT_WIN_INIT_MAX; 
    *minv = STAT_WIN_INIT_MIN;
    
    if (l == 0) return;
    
    for (i = 0; i < l; i++) {
        float v = w->buff[(w->head - l + i + STATIC_WIN) % STATIC_WIN];
        *mean += v; 
        if (v > *maxv) *maxv = v; 
        if (v < *minv) *minv = v;
    }
    *mean /= l;
    
    for (i = 0; i < l; i++) {
        float v = w->buff[(w->head - l + i + STATIC_WIN) % STATIC_WIN];
        *var += (v - *mean) * (v - *mean);
    }
    *var /= l;
}

// ===== MPU 初始化（增加错误处理）=====
static int mpu6050_init(const struct device *i2c_dev) {
    int ret;
    
    k_sleep(K_MSEC(100));
    
    uint8_t reset[] = {0x6B, 0x80};
    ret = i2c_write(i2c_dev, reset, 2, MPU_ADDR);
    if (ret != 0) {
        printk("MPU6050 reset failed: %d\n", ret);
        return ret;
    }
    k_sleep(K_MSEC(100));
    
    uint8_t wakeup[] = {0x6B, 0x01};
    ret = i2c_write(i2c_dev, wakeup, 2, MPU_ADDR);
    if (ret != 0) {
        printk("MPU6050 wakeup failed: %d\n", ret);
        return ret;
    }
    k_sleep(K_MSEC(10));
    
    uint8_t sample_rate[] = {0x19, 0x07};
    ret = i2c_write(i2c_dev, sample_rate, 2, MPU_ADDR);
    if (ret != 0) {
        printk("Sample rate config failed: %d\n", ret);
        return ret;
    }
    
    uint8_t gyro_config[] = {0x1B, 0x00};
    ret = i2c_write(i2c_dev, gyro_config, 2, MPU_ADDR);
    if (ret != 0) {
        printk("Gyro config failed: %d\n", ret);
        return ret;
    }
    
    uint8_t accel_config[] = {0x1C, 0x00};
    ret = i2c_write(i2c_dev, accel_config, 2, MPU_ADDR);
    if (ret != 0) {
        printk("Accel config failed: %d\n", ret);
        return ret;
    }
    
    printk("MPU6050 initialized successfully\n");
    return 0;
}

static float calc_mag(int16_t ax, int16_t ay, int16_t az) {
    float x = (float)ax / ACCEL_SCALE;
    float y = (float)ay / ACCEL_SCALE; 
    float z = (float)az / ACCEL_SCALE;
    return sqrtf(x*x + y*y + z*z);
}

// ===== 改进的方向性检测 =====
static bool is_intentional_tap_direction(int16_t ax, int16_t ay, int16_t az) {
    float x = fabsf((float)ax / ACCEL_SCALE);
    float y = fabsf((float)ay / ACCEL_SCALE);
    float z = fabsf((float)az / ACCEL_SCALE);
    
    // 找出最大的轴向
    float max_axis = fmaxf(fmaxf(x, y), z);
    float sum_other = x + y + z - max_axis;
    
    // 改进的方向性检测逻辑
    bool strong_enough = max_axis > AXIS_DOMINANCE_MIN;
    bool dominant = max_axis > AXIS_DOMINANCE_RATIO * fmaxf(sum_other, 0.01f);
    
    return strong_enough && dominant;
}

// ===== 自校准功能 =====
static void update_calibration(CalibrationState *cal, float acc_g, bool is_static) {
    if (!is_static) return;
    
    if (cal->sample_count < CALIBRATION_SAMPLES) {
        cal->sum += acc_g;
        cal->sample_count++;
        
        if (cal->sample_count == CALIBRATION_SAMPLES) {
            float new_ref = cal->sum / CALIBRATION_SAMPLES;
            // 只有在合理范围内才更新参考值
            if (fabsf(new_ref - GRAVITY_NOMINAL) < 0.3f) {
                cal->gravity_ref = new_ref;
                cal->calibrated = true;
                printk("Gravity calibrated to %.3f\n", cal->gravity_ref);
            } else {
                printk("Calibration rejected: %.3f too far from nominal\n", new_ref);
                cal->sample_count = 0;
                cal->sum = 0.0f;
            }
        }
    }
}

static float get_gravity_reference(CalibrationState *cal) {
    return cal->calibrated ? cal->gravity_ref : GRAVITY_NOMINAL;
}

// ===== 改进的姿态稳定性检测 =====
static bool is_posture_stable(FwStaticWin *win, float gravity_ref) {
    float mean, var, minv, maxv;
    win_stat(win, &mean, &var, &minv, &maxv);
    
    if (win->len < STATIC_WIN) return false;
    
    // 使用自校准的重力参考值
    bool low_variance = (var < STATIC_VAR_TH);
    bool small_range = (maxv - minv < STATIC_DIFF_TH);
    bool near_gravity = (fabsf(mean - gravity_ref) < POSTURE_STABLE_TH);
    
    return low_variance && small_range && near_gravity;
}

// ===== 双击一致性检测 =====
static bool taps_are_consistent(float mag1, float mag2) {
    if (mag1 <= 0 || mag2 <= 0) return false;
    
    float ratio = mag1 > mag2 ? mag1 / mag2 : mag2 / mag1;
    return ratio < TAP_CONSISTENCY_RATIO;
}

// ===== 改进的双击检测器 =====
typedef struct {
    int64_t last_tap_ts;
    int64_t last_double_ts;
    int tap_ready;
    int tap_cd;
    float first_tap_magnitude;
    int64_t tap_start_ts;
    bool tap_in_progress;
    SmoothWin smooth_win;        // 平滑窗口
    float last_smooth_acc;       // 上一次的平滑值
} DoubleTapState;

static int detect_double_tap_ring(int16_t ax, int16_t ay, int16_t az, 
                                  float acc_g, 
                                  FwStaticWin *stat_win, 
                                  DoubleTapState *st,
                                  CalibrationState *cal)
{
    int64_t now = k_uptime_get();
    float gravity_ref = get_gravity_reference(cal);
    
    // 平滑处理
    smooth_push(&st->smooth_win, acc_g);
    float smooth_acc = smooth_avg(&st->smooth_win);
    
    // 更新冷却计数器
    if (st->tap_cd > 0) {
        st->tap_cd -= SAMPLING_INTERVAL_MS;
        if (st->tap_cd < 0) st->tap_cd = 0;
    }
    
    // 检查基本条件
    bool posture_stable = is_posture_stable(stat_win, gravity_ref);
    bool near_gravity = fabsf(smooth_acc - gravity_ref) < GRAVITY_TOLERANCE;
    bool good_direction = is_intentional_tap_direction(ax, ay, az);
    
    // 更新自校准
    update_calibration(cal, smooth_acc, posture_stable && near_gravity);
    
    // 基本环境检查 - 放宽条件
    if (!posture_stable && !near_gravity) {
        // 环境不稳定，但不立即重置状态，给一定容忍度
        if (!st->tap_in_progress) {
            return 0;
        }
    }
    
    // 使用平滑后的突变检测
    float acc_spike = smooth_acc - st->last_smooth_acc;
    
    // 检测敲击开始
    if (!st->tap_in_progress && acc_spike > TAP_SPIKE_TH && smooth_acc > TAP_PEAK_ABS_TH && st->tap_cd == 0) {
        if (good_direction || near_gravity) { // 降低方向性要求
            st->tap_in_progress = true;
            st->tap_start_ts = now;
            printk("Tap start detected (smooth_acc: %.2f, spike: %.2f)\n", smooth_acc, acc_spike);
        }
    }
    
    // 检测敲击结束并确认
    if (st->tap_in_progress) {
        int64_t tap_duration = now - st->tap_start_ts;
        
        // 敲击持续时间足够长且现在回落
        if (tap_duration >= TAP_MIN_DURATION_MS && acc_spike < -TAP_SPIKE_TH * 0.4f) {
            st->tap_in_progress = false;
            st->tap_cd = TAP_COOLDOWN_MS;
            
            printk("Tap end detected (duration: %lldms)\n", tap_duration);
            
            // 确认这是一次有效敲击
            if (!st->tap_ready) {
                // 第一次敲击
                st->last_tap_ts = now;
                st->first_tap_magnitude = smooth_acc;
                st->tap_ready = 1;
                printk("First tap confirmed (mag: %.2f)\n", smooth_acc);
            } else {
                // 第二次敲击
                int64_t dt = now - st->last_tap_ts;
                if (dt >= DOUBLE_TAP_MIN_MS && dt <= DOUBLE_TAP_MAX_MS) {
                    // 检查双击一致性
                    if (taps_are_consistent(st->first_tap_magnitude, smooth_acc)) {
                        // 检查双击冷却
                        if (now - st->last_double_ts > DOUBLE_TAP_COOLDOWN) {
                            st->last_double_ts = now;
                            st->tap_ready = 0;
                            printk("Double tap confirmed! (dt: %lldms, mag1: %.2f, mag2: %.2f)\n", 
                                   dt, st->first_tap_magnitude, smooth_acc);
                            return 2; // 双击事件
                        } else {
                            printk("Double tap in cooldown period\n");
                        }
                    } else {
                        printk("Inconsistent tap magnitudes: %.2f vs %.2f (ratio: %.2f)\n", 
                               st->first_tap_magnitude, smooth_acc, 
                               st->first_tap_magnitude / smooth_acc);
                    }
                } else {
                    printk("Double tap timing out of range: %lldms\n", dt);
                }
                // 重置为新的第一次敲击
                st->last_tap_ts = now;
                st->first_tap_magnitude = smooth_acc;
                printk("Reset to new first tap\n");
            }
        }
        // 敲击超时
        else if (tap_duration > TAP_MIN_DURATION_MS * 4) {
            st->tap_in_progress = false;
            printk("Tap timeout after %lldms\n", tap_duration);
        }
    }
    
    // 双击超时重置
    if (st->tap_ready && (now - st->last_tap_ts > DOUBLE_TAP_MAX_MS)) {
        st->tap_ready = 0;
        printk("Double tap timeout, reset\n");
    }
    
    st->last_smooth_acc = smooth_acc;
    return 0;
}

// ===== 主函数 =====
void main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        printk("I2C device not ready\n");
        return;
    }
    
    if (mpu6050_init(i2c_dev) != 0) {
        printk("MPU6050 initialization failed\n");
        return;
    }
    
    static FwStaticWin static_win;
    memset(&static_win, 0, sizeof(static_win));
    
    DoubleTapState st = {0};
    CalibrationState cal = {GRAVITY_NOMINAL, 0, 0.0f, false};
    
    printk("Ring double-tap detector started...\n");
    printk("Parameters: spike_th=%.2f, peak_th=%.2f, var_th=%.3f\n", 
           TAP_SPIKE_TH, TAP_PEAK_ABS_TH, STATIC_VAR_TH);
    printk("Calibration will start automatically...\n");
    
    int error_count = 0;
    int debug_counter = 0;
    
    while (1) {
        uint8_t reg_accel = 0x3B;
        uint8_t accel_data[6];
        
        int ret = i2c_write_read(i2c_dev, MPU_ADDR, &reg_accel, 1, accel_data, 6);
        if (ret == 0) {
            error_count = 0; // 重置错误计数
            
            int16_t ax = (int16_t)((accel_data[0] << 8) | accel_data[1]);
            int16_t ay = (int16_t)((accel_data[2] << 8) | accel_data[3]);
            int16_t az = (int16_t)((accel_data[4] << 8) | accel_data[5]);
            
            float acc_g = calc_mag(ax, ay, az);
            win_push(&static_win, acc_g);
            
            int evt = detect_double_tap_ring(ax, ay, az, acc_g, &static_win, &st, &cal);
            if (evt == 2) {
                printk(">>> 戒指双击事件触发! <<<\n");
                // 这里可以添加你的双击响应代码
            }
            
            // 定期输出状态信息
            debug_counter++;
            if (debug_counter >= 250) { // 每5秒输出一次
                float gravity_ref = get_gravity_reference(&cal);
                printk("Status: gravity_ref=%.3f, calibrated=%s, acc_g=%.3f\n", 
                       gravity_ref, cal.calibrated ? "YES" : "NO", acc_g);
                debug_counter = 0;
            }
        } else {
            error_count++;
            if (error_count > 10) {
                printk("I2C communication errors, reinitializing...\n");
                mpu6050_init(i2c_dev);
                error_count = 0;
            }
        }
        
        k_sleep(K_MSEC(SAMPLING_INTERVAL_MS));
    }
}