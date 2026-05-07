# 单片机课程实验 - STC89C52

本仓库整理了基于 STC89C52/8051 兼容单片机的课程实验代码，主要平台为普中 HC6800-ES V2.0，使用 Keil C51、EIDE、STC-ISP 和串口工具完成开发与验证。

## 实验目录

| 目录 | 内容 |
|---|---|
| `实验一/` | LED 流水灯、蜂鸣器报警 |
| `实验二/` | 数码管、矩阵键盘、LCD1602 |
| `实验三/` | DS1302 RTC、LCD1602、串口时间同步 |
| `实验四/` | DS18B20 温度采集、LCD1602、AT24C08、串口命令、报警 |

## 实验四：基于 DS18B20 的温度传感器

实验四是当前整理后的完整工程，源码位于 `实验四/`，EIDE 工程位于 `实验四/NewProject1/`。

### 功能

- DS18B20 温度采集，支持一位小数和负温度。
- LCD1602 分页显示时间、温度、上下限、采集状态和上传间隔。
- K3 翻页，K4 调节阈值，短按加 `0.5C`，长按减 `0.5C`。
- AT24C08 保存参数和采样记录。
- 串口周期上传采集数据。
- 串口命令控制采集、上传频率、温度上下限和时间。
- 蜂鸣器报警，LED 模块 `P2.2/P2.3` 分别指示高温/低温报警。

### 主要引脚

| 外设 | 引脚 |
|---|---|
| LCD1602 数据口 | `P0` |
| LCD1602 RS/RW/EN | `P2.6 / P2.5 / P2.7` |
| DS18B20 DQ | `P3.7`，需要 `4.7k` 上拉到 VCC |
| AT24C08 SCL/SDA | `P2.1 / P2.0` |
| K3/K4 | `P3.2 / P3.3` |
| 蜂鸣器 | `P1.5` |
| 高温/低温报警 LED | `P2.2 / P2.3` |
| 串口 RXD/TXD | `P3.0 / P3.1`，9600 bps |

### 串口上传格式

```text
TIME=12:30:05,TEM=25.6C,TEM_L=20.0C,TEM_H=30.0C,STATE=RUN
```

### 串口命令

| 命令 | 说明 |
|---|---|
| `start_tem` | 开始采集并恢复周期上传 |
| `stop_tem` | 停止采集并停止周期上传 |
| `T=X` | 设置上传间隔，`X` 为 `2..8` |
| `TEM_L=XX.X` | 设置低温阈值 |
| `TEM_H=XX.X` | 设置高温阈值 |
| `TIME=hh:mm:ss` | 校准软件时钟 |

返回示例：

```text
OK:START
OK:STOP
OK:T=3
OK:TEM_L=18.5C
OK:TEM_H=32.0C
OK:TIME=12:30:00
ERROR:CMD
ERROR:VALUE
```

### Python 串口工具

命令行版：

```powershell
cd D:\Mono_chip_proj\实验四\NewProject1
python serial_test.py --port COM7
```

图形版：

```powershell
cd D:\Mono_chip_proj\实验四\NewProject1
python serial_gui.py --port COM7
```

依赖：

```powershell
pip install pyserial
```

## 开发环境

- Keil C51
- VS Code + EIDE
- STC-ISP
- Python 3.x + pyserial

## 说明

编译输出、HEX 文件、压缩包和 IDE 临时文件已通过 `.gitignore` 排除。仓库只保留源码、工程配置、说明文档和辅助测试脚本。
