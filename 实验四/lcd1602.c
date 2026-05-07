/*
 * lcd1602.c  --  LCD1602 驱动 (8 位模式), 11.0592 MHz
 */

#include "lcd1602.h"

/* 内部短延时 (约 1ms/次) */
static void LCD_DelayMs(u16 ms)
{
    u8 i, j;
    while (ms--)
    {
        i = 2; j = 239;
        do { while (--j); } while (--i);
    }
}

static void LCD_WriteCmd(u8 cmd)
{
    LCD_RS = 0;
    LCD_RW = 0;
    LCD_DATA_PORT = cmd;
    LCD_EN = 1;
    LCD_DelayMs(1);
    LCD_EN = 0;
    LCD_DelayMs(1);
}

static void LCD_WriteData(u8 dat)
{
    LCD_RS = 1;
    LCD_RW = 0;
    LCD_DATA_PORT = dat;
    LCD_EN = 1;
    LCD_DelayMs(1);
    LCD_EN = 0;
    LCD_DelayMs(1);
}

static void LCD_SetCursor(u8 line, u8 col)
{
    if (line == 1) LCD_WriteCmd(0x80 | (col - 1));
    else           LCD_WriteCmd(0xC0 | (col - 1));
}

void LCD_Init(void)
{
    LCD_DelayMs(50);
    LCD_WriteCmd(0x38);   /* 8bit, 2line, 5x8 */
    LCD_DelayMs(5);
    LCD_WriteCmd(0x38);
    LCD_DelayMs(5);
    LCD_WriteCmd(0x38);
    LCD_DelayMs(5);
    LCD_WriteCmd(0x08);   /* 关显示 */
    LCD_WriteCmd(0x01);   /* 清屏 */
    LCD_DelayMs(5);
    LCD_WriteCmd(0x06);   /* 光标右移, 画面不动 */
    LCD_WriteCmd(0x0C);   /* 开显示, 关光标 */
}

void LCD_Clear(void)
{
    LCD_WriteCmd(0x01);
    LCD_DelayMs(5);
}

void LCD_ShowChar(u8 line, u8 col, char ch)
{
    LCD_SetCursor(line, col);
    LCD_WriteData((u8)ch);
}

void LCD_ShowString(u8 line, u8 col, char *str)
{
    LCD_SetCursor(line, col);
    while (*str) LCD_WriteData((u8)(*str++));
}

void LCD_ShowNum(u8 line, u8 col, u16 num, u8 len)
{
    char buf[6];
    u8 i;
    u16 t = num;
    for (i = len; i > 0; i--)
    {
        buf[i - 1] = (char)('0' + (t % 10));
        t /= 10;
    }
    buf[len] = '\0';
    LCD_ShowString(line, col, buf);
}
