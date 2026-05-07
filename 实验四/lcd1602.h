/*
 * lcd1602.h  --  LCD1602 字符液晶驱动 (8 位并行模式)
 * 引脚: D0-D7 = P0, RS=P2^6, RW=P2^5, EN=P2^7
 *
 * 注意: P0 口接 LCD 时, Proteus 中需要为 P0 加上拉电阻 (10k 排阻).
 *       LCD1602 字模无 ℃, 用 'C' 代替 (满足实验要求里允许的折中).
 *       实际开发板 P0 已有上拉, 直接连接即可.
 */

#ifndef __LCD1602_H__
#define __LCD1602_H__

#include "config.h"

void LCD_Init(void);
void LCD_Clear(void);
void LCD_ShowChar  (u8 line, u8 col, char ch);
void LCD_ShowString(u8 line, u8 col, char *str);
void LCD_ShowNum   (u8 line, u8 col, u16 num, u8 len);   /* 补前导零 */

#endif
