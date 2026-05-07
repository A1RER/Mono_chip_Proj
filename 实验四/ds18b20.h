/*
 * ds18b20.h  --  DS18B20 单总线温度传感器
 *
 * 时序: 单总线协议, DQ 上拉电阻 4.7k.
 * 转换分辨率: 12 位, 1 LSB = 0.0625 C, 转换时间 <= 750ms.
 *
 * 接口设计采用"非阻塞读取":
 *   - DS18B20_StartConvert() 仅发送 Convert T 命令后立即返回
 *   - 主循环在下一秒到来时调用 DS18B20_ReadTempX10() 读取转换结果
 *   - 这样不会因 750ms 阻塞导致按键/串口响应迟钝
 */

#ifndef __DS18B20_H__
#define __DS18B20_H__

#include "config.h"

#define DS18B20_STATUS_OK       0
#define DS18B20_STATUS_NO_ACK   1
#define DS18B20_STATUS_READ_FF  2

extern u8 g_ds18b20_status;

#define DS18B20_TEMP_INVALID    ((s16)0x7FFF)   /* 通信失败标志 */

bit  DS18B20_Init(void);            /* 复位 + 检测 presence, 返回 1=存在, 0=未应答 */
void DS18B20_StartConvert(void);    /* 启动一次温度转换 (非阻塞) */
s16  DS18B20_ReadTempX10(void);     /* 读取上次转换结果, 返回温度 x10 */

#endif
