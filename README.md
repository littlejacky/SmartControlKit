# SmartControlKit

SmartControlKit 是一个基于嵌入式系统的硬件控制项目，适用于如按钮输入、LED 控制、电机驱动等场景。该项目采用 C 语言开发，适配 nRF54L15DK 等开发板，适合学习和二次开发。

## 主要功能
- 按钮输入管理（button_input）
- LED 灯控制（led_control）
- 电机驱动（motor_driver）

## 目录结构
```
include/           # 头文件
src/               # 源代码
boards/            # 板级配置
build/             # 构建输出
CMakeLists.txt     # CMake 构建脚本
prj.conf           # 项目配置
```

## 快速开始
1. 克隆仓库：
   ```bash
   git clone https://github.com/littlejacky/SmartControlKit.git
   cd SmartControlKit
   ```
2. 配置开发环境（建议使用 nRF Connect SDK 和 Zephyr）。
3. 构建项目：
   ```bash
   west build -b nrf54l15dk_nrf54l15_cpuapp
   ```
4. 烧录固件：
   ```bash
   west flash
   ```

## 依赖
- nRF Connect SDK
- Zephyr RTOS
- CMake

## 贡献
欢迎 issue 和 PR！

## 许可证
MIT License
