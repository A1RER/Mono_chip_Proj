/*
 * DS1302.h - DS1302 RTC 驱动 (HC6800-ES V2.0)
 * 引脚: CLK=P3^6, DAT=P3^4, RST=P3^5
 */
#ifndef __DS1302_H__
#define __DS1302_H__

void DS1302_Init(void);
void DS1302_SetTime(unsigned char hour, unsigned char min, unsigned char sec);
void DS1302_GetTime(unsigned char *hour, unsigned char *min, unsigned char *sec);

/* 调试: 返回 Burst Read 的原始 BCD 字节, buf 需 >=3 字节, 顺序 秒/分/时 */
void DS1302_GetRawBCD(unsigned char *buf);

#endif
