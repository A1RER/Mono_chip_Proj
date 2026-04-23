/*
 * LCD1602.h - LCD1602 驱动 (HC6800-ES V2.0 普中51)
 * 引脚: D0-D7=P0, RS=P2^6, RW=P2^5, EN=P2^7
 */
#ifndef __LCD1602_H__
#define __LCD1602_H__

#include <REG52.H>

/* 控制引脚 */
sbit LCD_RS = P2^6;
sbit LCD_RW = P2^5;
sbit LCD_EN = P2^7;

void LCD_Init(void);
void LCD_Clear(void);
void LCD_ShowChar(unsigned char Line, unsigned char Col, char Char);
void LCD_ShowString(unsigned char Line, unsigned char Col, char *Str);
void LCD_ShowNum(unsigned char Line, unsigned char Col, unsigned int Num, unsigned char Len);

#endif
