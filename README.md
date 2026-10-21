# 计算器使用说明

## 功能与操作

本工程适用于 Comsen 招新 STM32F103C8T6 计算器板，使用 LCD1602 字符屏和 TTP229 触摸按键。
支持四则运算、小数、括号、乘方、平方根、三角函数、对数、DEG/RAD 角度模式、ANS 和复数模式。
USB 虚拟串口默认不启动；一元一次方程求解接口未接入按键界面。

输入算式后按 `=` 求值，按 `AC` 清空。SHIFT 的用法是先按下并松开，再按目标键；每次使用第二功能都需要重新按 SHIFT。

| 操作示例 | 按键顺序（从 AC 开始） | 预期结果 |
| --- | --- | --- |
| 四则运算 | `1 → + → 2 → × → 3 → =` | `7` |
| 括号 | `( → 1 → + → 2 → ) → × → 3 → =` | `9` |
| 乘方 | `2 → SHIFT → × → 3 → =` | `8` |
| 平方根 | `SHIFT → ÷ → 9 → ) → =` | `3` |
| 常用对数 | `SHIFT → 5 → 1 → 0 → 0 → ) → =` | `2` |
| 正弦（DEG 模式） | `SHIFT → 1 → 3 → 0 → ) → =` | 约 `0.5` |

函数键会自动输入左括号，计算前需要补右括号。屏幕上的 `s/c/t/l/n/q` 分别表示 sin/cos/tan/log/ln/sqrt。
模式选择和结果格式切换沿用实验室例程；复数相关输入需先切换到 CMPLX 模式。

## 编译与烧录

工程配置位于 `example/software.ioc`，可用 STM32CubeMX 查看。工程采用 CMake 构建，不是 Keil 工程。
需要 Arm GNU GCC、CMake（3.22 或更高）和 Ninja；工具链安装在工程外，不随源码提供。

已经将这些工具加入 PATH 时，在本目录执行：

```sh
cmake -S example -B example/build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build example/build/Release
```

Windows 也可使用 `build-windows.ps1`。该脚本默认查找同级 `Comsen-tools/`，目录结构如下：

```text
Comsen-tools/
  gcc/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/
  cmake/cmake-3.31.8-windows-x86_64/bin/
  ninja/ninja.exe
```

指定工具所在目录后执行：

```powershell
.\build-windows.ps1 -Configuration Release -ToolsRoot 'D:\Comsen-tools'
```

CMake 命令的编译产物位于 `example/build/Release/`；Windows 脚本的产物位于 `example/build/WindowsRelease/`。
其中 `software.hex` 或 `software.bin` 可用对应烧录软件写入开发板；BIN 的内部 Flash 起始地址为 `0x08000000`。
编译脚本不自动烧录。

## 使用限制

- 算式最多 63 个 ASCII 字节，输入输出使用单精度浮点数，约 6～7 位有效数字。
- 必须显式输入乘号，例如 `2*(3)` 和 `2*i`；不支持省略乘号。
- 不支持阶乘、反三角、积分和通用方程求解；三角函数、对数只接受实数参数。
- 本次整理仅核对源码及依赖完整性，未重新编译或进行实物验证。实际演示应使用本工程编译的固件，并单独确认按键、显示及各项功能。
- 触摸板背面也可能感应到手部接近。测试时用非导电支撑物垫高，上电时保持手远离正反面的触摸区域。

## 代码来源

基于 [Comsen 实验室例程](https://github.com/limolin234/Comsen_recruitment_hardware)。
计算核心 `example/Core/Src/calculator_engine.c` 由 Codex 辅助编写，构建文件由 Codex 辅助调整。
显示、触摸驱动、滤波、触摸模型和应用界面沿用实验室提供的五个静态库，保存在 `example/lib/`；编译需要这些文件。
底层初始化、HAL、CMSIS、FreeRTOS 和 USB 组件保留原有来源及许可证声明。
