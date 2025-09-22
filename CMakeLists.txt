# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mvp)

target_sources(app PRIVATE 
#    src/main.c
#    src/led_control.c
#    src/button_input.c
#    src/motor_driver.c

    test/mpu6050.c
)

# 头文件路径
zephyr_include_directories(include)