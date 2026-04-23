/*
 * LCD1602.c - LCD1602 驱动实现
 * 晶振: 11.0592 MHz
 */
#include "LCD1602.h"

/* 内部延时 (约 1ms/次, 11.0592MHz) */
static void LCD_Delay(unsigned int ms)
{
    unsigned char i, j;
    while (ms--)
    {
        i = 2; j = 239;
        do { while (--j); } while (--i);
    }
}

/* 写命令 */
static void LCD_WriteCmd(unsigned char Cmd)
{
    LCD_RS = 0;
    LCD_RW = 0;
    P0 = Cmd;
    LCD_EN = 1;
    LCD_Delay(1);
    LCD_EN = 0;
    LCD_Delay(1);
}

/* 写数据 */
static void LCD_WriteData(unsigned char Data)
{
    LCD_RS = 1;
    LCD_RW = 0;
    P0 = Data;
    LCD_EN = 1;
    LCD_Delay(1);
    LCD_EN = 0;
    LCD_Delay(1);
}

/* 设置光标 (Line: 1或2, Col: 1~16) */
static void LCD_SetCursor(unsigned char Line, unsigned char Col)
{
    if (Line == 1)
        LCD_WriteCmd(0x80 | (Col - 1));
    else
        LCD_WriteCmd(0xC0 | (Col - 1));
}

/* 初始化 LCD1602 */
void LCD_Init(void)
{
    LCD_Delay(50);
    LCD_WriteCmd(0x38);  /* 8位数据, 2行, 5x8字体 */
    LCD_Delay(5);
    LCD_WriteCmd(0x38);
    LCD_Delay(5);
    LCD_WriteCmd(0x38);
    LCD_Delay(5);
    LCD_WriteCmd(0x08);  /* 关显示 */
    LCD_WriteCmd(0x01);  /* 清屏 */
    LCD_Delay(5);
    LCD_WriteCmd(0x06);  /* 光标自右移, 画面不移动 */
    LCD_WriteCmd(0x0C);  /* 开显示, 关光标 */
}

/* 清屏 */
void LCD_Clear(void)
{
    LCD_WriteCmd(0x01);
    LCD_Delay(5);
}

/* 在 (Line, Col) 显示单个字符 */
void LCD_ShowChar(unsigned char Line, unsigned char Col, char Char)
{
    LCD_SetCursor(Line, Col);
    LCD_WriteData(Char);
}

/* 在 (Line, Col) 显示字符串 */
void LCD_ShowString(unsigned char Line, unsigned char Col, char *Str)
{
    LCD_SetCursor(Line, Col);
    while (*Str)
        LCD_WriteData(*Str++);
}

/* 在 (Line, Col) 显示整数, Len 为显示位数 (补前导零) */
void LCD_ShowNum(unsigned char Line, unsigned char Col, unsigned int Num, unsigned char Len)
{
    unsigned char i;
    char buf[6];
    unsigned int temp = Num;

    /* 从低位到高位提取数字 */
    for (i = Len; i > 0; i--)
    {
        buf[i - 1] = '0' + (temp % 10);
        temp /= 10;
    }
    buf[Len] = '\0';
    LCD_ShowString(Line, Col, buf);
}
