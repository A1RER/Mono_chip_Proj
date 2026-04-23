# 单片机实验 — STC89C52

基于 STC89C52（8051 兼容）的三个课程实验，平台为普中 HC6800-ES V2.0。

---

## 实验一　LED 流水灯 / 蜂鸣器

**硬件:** STC89C52，12 MHz 晶振，Keil MDK 工程

| 源文件 | 功能 |
|---|---|
| `main.c` | P2 口 8 路 LED 单向流水灯，每步 500 ms |
| `main_bidirection.c` | P2 口 LED 双向流水灯 |
| `main_buzzer_alarm.c` | P1.5 无源蜂鸣器报警 + LED 联动 |

---

## 实验二　数码管 + 矩阵键盘 + LCD1602

**硬件:** STC89C52，Keil MDK 工程
- P0 — 数码管段选（共阴）；P2 — 位选 / 74HC138 译码
- LCD1602：D0-D7=P0，RS/RW/EN=P2

| 源文件 | 功能 |
|---|---|
| `Exp1_Date.c` | 8 位数码管动态扫描，循环显示日期 / 学号 / 生日，约 2 s 切换一次 |
| `Exp2_Countdown.c` | 2 位数码管循环倒计时 59→0，Timer0 1 ms 中断计时（24 MHz 晶振） |
| `Exp3_KeyCountdown.c` | 矩阵键盘控制倒计时的启停与预置 |
| `main.c` | 矩阵键盘扫描，按键值实时显示在 LCD1602 第 2 行 |

---

## 实验三　DS1302 + LCD1602 + 串口 RTC 同步

**硬件:** HC6800-ES V2.0（STC89C52），11.0592 MHz 晶振，EIDE 工程（`project260423/`）

**引脚分配**

| 外设 | 引脚 |
|---|---|
| LCD1602 D0-D7 | P0 |
| LCD1602 RS/RW/EN | P2.6 / P2.5 / P2.7 |
| 矩阵键盘 行/列 | P1.0-P1.3 / P1.4-P1.7 |
| 独立按键 K1/K2/K3 | P3.1 / P3.0 / P3.2 |
| DS1302 CLK/DAT/RST | P3.6 / P3.4 / P3.5 |
| 串口 RXD/TXD | P3.0 / P3.1（9600 bps） |

> K3 循环切换五个任务，K1/K2 在任务二中用于加减调节。

### 五个任务（K3 循环切换）

| 任务 | 说明 |
|---|---|
| 任务 1 | LCD 静态显示学号后三位与姓名首字母 |
| 任务 2 | 矩阵键盘实时显示键值；K1/K2 短按 ±1、长按 ±2，范围 1-16 |
| 任务 3 | Timer0 50 ms 中断驱动的 24 小时软件时钟（初始 23:59:55） |
| 任务 4 | DS1302 硬件 RTC（掉电保持，初始 23:59:55），LCD 显示 BCD 原值用于调试 |
| 任务 5 | PC 端通过串口每隔 N 秒发送 `HH:MM:SS\n` 校准 DS1302，LCD 实时显示 |

### 串口时间同步工具

```bash
pip install pyserial
python 实验三/uart_sync.py              # 默认 COM7，每 10 s 同步一次
python 实验三/uart_sync.py COM5         # 指定串口
python 实验三/uart_sync.py COM5 1       # 指定串口 + 同步间隔（秒）
```

> 运行前确认 LCD 已显示 `Serial+DS1302`（任务五），否则按 K3 切换。  
> 脚本会强制 `dtr=False`，避免 CH340 在打开串口时触发单片机复位。

---

## 目录结构

```
Mono_chip_proj/
├── 实验一/
│   ├── main.c                  # LED 单向流水灯
│   ├── main_bidirection.c      # 双向流水灯
│   ├── main_buzzer_alarm.c     # 蜂鸣器报警
│   └── Project.uvproj          # Keil 工程文件
├── 实验二/
│   ├── Exp1_Date.c             # 数码管显示日期/学号/生日
│   ├── Exp2_Countdown.c        # 数码管倒计时
│   ├── Exp3_KeyCountdown.c     # 按键控制倒计时
│   ├── main.c                  # 矩阵键盘 → LCD
│   ├── LCD1602.c / .h
│   ├── MatrixKey.c / .h
│   ├── Delay.c / .h
│   └── project.uvproj          # Keil 工程文件
└── 实验三/
    ├── main.c                  # 五任务主程序
    ├── DS1302.c / .h           # RTC 驱动
    ├── LCD1602.c / .h          # LCD 驱动
    ├── MatrixKey.c / .h        # 矩阵键盘驱动
    ├── uart_sync.py            # PC 端串口时间同步脚本
    ├── uart_sync.cpp           # （同功能 C++ 版本，备用）
    └── project260423/          # EIDE 工程（VS Code + EIDE 插件）
```

## 开发环境

- **Keil MDK**（实验一、二）或 **EIDE 插件 for VS Code**（实验三）
- 编译器：Keil C51
- 烧录：STC-ISP
- Python 3.x + pyserial（实验三任务五串口同步）
