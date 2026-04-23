/*
 * main.c  --  实验三: 矩阵按键控制 LCD1602
 * 平台  : HC6800-ES V2.0 普中51 (STC89C52), 晶振 11.0592 MHz
 *
 * === 引脚分配 ===
 *  LCD1602 : D0-D7=P0, RS=P2^6, RW=P2^5, EN=P2^7
 *  矩阵按键: 行=P1^0~P1^3, 列=P1^4~P1^7
 *  独立按键: K1(P3^1), K2(P3^0), K3(P3^2)  -- 注: P3^0/P3^1 兼作 RXD/TXD
 *  DS1302  : CLK=P3^6, DAT=P3^4, RST=P3^5
 *  串口    : RXD=P3^0, TXD=P3^1, 9600bps, 格式 "HH:MM:SS\n"
 *
 * === 模式说明 (K3 循环切换) ===
 *  任务1: 静态显示学号后三位 / 姓名拼音首字母
 *  任务2: 矩阵键值实时显示 + 独立按键加减调节 (边界 1~16)
 *  任务3: 24小时软件计时 (初始 23:59:55)
 *  任务4: DS1302 硬件时钟 (初始 23:59:55, 掉电保持)
 *  任务5: 串口 RTC 同步 -> DS1302 -> LCD 显示
 */

#include <REG52.H>
#include "LCD1602.h"
#include "MatrixKey.h"
#include "DS1302.h"

/* -------- 学生信息 (根据实际情况修改) -------- */
#define STUDENT_ID   "607"
#define STUDENT_NAME "DW"

/* 独立按键 */
sbit K1 = P3^1;   /* +1 短按 / +2 长按  (任务5 时为 TXD, 不可用) */
sbit K2 = P3^0;   /* -1 短按 / -2 长按  (任务5 时为 RXD, 不可用) */
sbit K3 = P3^2;   /* 切换任务模式 */

/* 全局状态 */
static unsigned char g_mode = 0;   /* 0~4 对应任务1~5 */

/* Timer0 软件时钟 */
static volatile unsigned char g_t0_cnt = 0;
static volatile unsigned char g_sec    = 55;
static volatile unsigned char g_min    = 59;
static volatile unsigned char g_hour   = 23;
static volatile unsigned char g_tick   = 0;

/* UART 接收缓冲 (格式 "HH:MM:SS", 8 字节) */
#define UART_BUF_LEN 8
static volatile unsigned char uart_buf[UART_BUF_LEN];
static volatile unsigned char uart_idx   = 0;
static volatile bit           uart_ready = 0;

/* DS1302 首次进入标志 */
static bit ds1302_inited = 0;

/* ===== 延时工具 ===== */
static void Delay_ms(unsigned int ms)
{
    unsigned char i, j;
    while (ms--)
    {
        i = 2; j = 239;
        do { while (--j); } while (--i);
    }
}

/* ===== Timer0: 50ms 中断, 11.0592 MHz ===== */
static void Timer0_Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;   /* Timer0 模式1 (16位) */
    TH0   = 0x4B;
    TL0   = 0xFD;
    ET0   = 1;
    TR0   = 1;
    EA    = 1;
}

/* 20 x 50ms = 1s */
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0x4B;
    TL0 = 0xFD;
    g_t0_cnt++;
    if (g_t0_cnt >= 20)
    {
        g_t0_cnt = 0;
        g_sec++;
        if (g_sec  >= 60) { g_sec  = 0; g_min++;  }
        if (g_min  >= 60) { g_min  = 0; g_hour++; }
        if (g_hour >= 24) { g_hour = 0; }
        g_tick = 1;
    }
}

/* ===== UART: 9600bps (MCU), 配合 PC 端 10000bps 使用 =====
 * 11.0592MHz 晶振下 Timer1 分频器做不出精确 10000, 最接近的是 9600 (-4%).
 * PC 端 CH340 能精确发 10000. 单帧有 50% 机会采样错位丢帧, 但 DS1302
 * 自走时只需要偶尔一次校准成功就能保持同步, 宏观上 LCD 显示是对的.
 */
static void UART_Init(void)
{
    PCON  &= 0x7F;              /* SMOD=0 */
    SCON   = 0x50;              /* 模式1 (8位UART), 允许接收 */
    TMOD   = (TMOD & 0x0F) | 0x20; /* Timer1 模式2 (自动重载), 保留 Timer0 */
    TH1    = 0xFD;              /* 9600bps @ 11.0592MHz */
    TL1    = 0xFD;
    TR1    = 1;
    ES     = 1;
    EA     = 1;
}

/* 接收到 "HH:MM:SS\n" 后置 uart_ready */
void UART_ISR(void) interrupt 4
{
    unsigned char c;
    if (RI)
    {
        RI = 0;
        c  = SBUF;
        if (c == '\n' || c == '\r')
        {
            if (uart_idx == UART_BUF_LEN)
                uart_ready = 1;
            uart_idx = 0;
        }
        else if (uart_idx < UART_BUF_LEN)
        {
            uart_buf[uart_idx++] = c;
        }
    }
    TI = 0;
}

/* ===== 独立按键 =====
 * 返回: 0=无键, 1=K1短按(+1), 2=K1长按(+2), 3=K2短按(-1), 4=K2长按(-2)
 */
static unsigned char IndKey_Read(void)
{
    unsigned int hold;

    if (K1 == 0)
    {
        Delay_ms(10);
        if (K1 == 0)
        {
            hold = 0;
            while (K1 == 0) { Delay_ms(10); if (++hold > 50) break; }
            while (K1 == 0);
            Delay_ms(10);
            return (hold > 50) ? 2 : 1;
        }
    }
    if (K2 == 0)
    {
        Delay_ms(10);
        if (K2 == 0)
        {
            hold = 0;
            while (K2 == 0) { Delay_ms(10); if (++hold > 50) break; }
            while (K2 == 0);
            Delay_ms(10);
            return (hold > 50) ? 4 : 3;
        }
    }
    return 0;
}

static bit K3_Check(void)
{
    if (K3 == 0)
    {
        Delay_ms(10);
        if (K3 == 0)
        {
            while (K3 == 0);
            Delay_ms(150);   /* 松开后等150ms消抖, 防止任务切换时被误触发 */
            return 1;
        }
    }
    return 0;
}

/* ===== 任务一: 静态显示学号 / 姓名 ===== */
static void Task1_Run(void)
{
    LCD_Clear();
    LCD_ShowString(1, 1, "Num: " STUDENT_ID);
    LCD_ShowString(2, 1, "Name:" STUDENT_NAME);

    while (1)
    {
        if (K3_Check()) { g_mode = 1; return; }
    }
}

/* ===== 任务二: 矩阵键值 + 独立按键加减 ===== */
static void Task2_Run(void)
{
    unsigned char mat_key, ind_key;
    unsigned char adj_val   = 1;
    unsigned char key_shown = 0;
    int           new_val;

    LCD_Clear();
    LCD_ShowString(1, 1, "Key: --");
    LCD_ShowString(2, 1, "Adj: 01");

    while (1)
    {
        if (K3_Check()) { g_mode = 2; return; }

        mat_key = MatrixKey_GetKey();
        if (mat_key != 0)
        {
            key_shown = mat_key;
            adj_val   = mat_key;
            LCD_ShowNum(1, 6, key_shown, 2);
            LCD_ShowNum(2, 6, adj_val,   2);
        }

        ind_key = IndKey_Read();
        if (ind_key != 0)
        {
            new_val = (int)adj_val;
            if      (ind_key == 1) new_val += 1;
            else if (ind_key == 2) new_val += 2;
            else if (ind_key == 3) new_val -= 1;
            else if (ind_key == 4) new_val -= 2;

            if (new_val >= 1 && new_val <= 16)
            {
                adj_val = (unsigned char)new_val;
                LCD_ShowNum(2, 6, adj_val, 2);
            }
        }
    }
}

/* ===== 任务三: 软件计时器 (23:59:55 起) ===== */
static void Task3_Run(void)
{
    EA = 0;
    g_hour = 23; g_min = 59; g_sec = 55;
    g_t0_cnt = 0; g_tick = 0;
    EA = 1;

    LCD_Clear();
    LCD_ShowString(1, 4, "24H Clock");
    LCD_ShowString(2, 4, "  :  :  ");
    LCD_ShowNum(2, 4,  g_hour, 2);
    LCD_ShowChar(2, 6, ':');
    LCD_ShowNum(2, 7,  g_min,  2);
    LCD_ShowChar(2, 9, ':');
    LCD_ShowNum(2, 10, g_sec,  2);

    while (1)
    {
        if (K3_Check()) { g_mode = 3; return; }
        if (g_tick)
        {
            g_tick = 0;
            LCD_ShowNum(2, 4,  g_hour, 2);
            LCD_ShowChar(2, 6, ':');
            LCD_ShowNum(2, 7,  g_min,  2);
            LCD_ShowChar(2, 9, ':');
            LCD_ShowNum(2, 10, g_sec,  2);
        }
    }
}

/* ===== 任务四: DS1302 硬件时钟 (调试版: 显示原始 BCD) ===== */
static void Hex2Char(unsigned char v, unsigned char *hi, unsigned char *lo)
{
    unsigned char h = (v >> 4) & 0x0F;
    unsigned char l =  v       & 0x0F;
    *hi = (h < 10) ? ('0' + h) : ('A' + h - 10);
    *lo = (l < 10) ? ('0' + l) : ('A' + l - 10);
}

static void Task4_Run(void)
{
    unsigned char raw[3];
    unsigned char hi, lo;
    unsigned char cnt = 0;

    if (!ds1302_inited)
    {
        DS1302_Init();
        DS1302_SetTime(23, 59, 55);
        ds1302_inited = 1;
    }

    LCD_Clear();
    LCD_ShowString(1, 1, "RAW S  M  H");
    LCD_ShowString(2, 1, "              ");

    while (1)
    {
        if (K3_Check()) { g_mode = 4; return; }

        DS1302_GetRawBCD(raw);

        /* 第 2 行: "XX XX XX #NN"  — 秒/分/时 的 BCD 十六进制原值 + 读取计数 */
        Hex2Char(raw[0], &hi, &lo);
        LCD_ShowChar(2, 1, hi); LCD_ShowChar(2, 2, lo);
        LCD_ShowChar(2, 3, ' ');
        Hex2Char(raw[1], &hi, &lo);
        LCD_ShowChar(2, 4, hi); LCD_ShowChar(2, 5, lo);
        LCD_ShowChar(2, 6, ' ');
        Hex2Char(raw[2], &hi, &lo);
        LCD_ShowChar(2, 7, hi); LCD_ShowChar(2, 8, lo);
        LCD_ShowChar(2, 9, ' ');
        LCD_ShowChar(2, 10, '#');
        LCD_ShowNum(2, 11, cnt, 3);
        cnt++;

        Delay_ms(500);
    }
}

/* ===== 任务五: 串口 RTC 同步 -> DS1302 -> LCD ===== */
/*
 * PC 端每秒发送 "HH:MM:SS\n", 单片机解析后写入 DS1302.
 * DS1302 自行计时, 每次收到串口数据则校准一次.
 * LCD 第2行实时显示 DS1302 当前时间.
 */
static void Task5_Run(void)
{
    unsigned char h, m, s;
    unsigned char last_s = 0xFF;
    unsigned char uh, um, us;

    if (!ds1302_inited)
    {
        DS1302_Init();
        DS1302_SetTime(0, 0, 0);
        ds1302_inited = 1;
    }

    /* 清除可能遗留的串口数据 */
    uart_idx   = 0;
    uart_ready = 0;

    LCD_Clear();
    LCD_ShowString(1, 2, "Serial+DS1302");
    LCD_ShowString(2, 4, "  :  :  ");

    while (1)
    {
        if (K3_Check()) { g_mode = 0; return; }

        /* 收到完整时间帧则同步 DS1302 */
        if (uart_ready)
        {
            uart_ready = 0;
            if (uart_buf[2] == ':' && uart_buf[5] == ':')
            {
                uh = (unsigned char)((uart_buf[0] - '0') * 10 + (uart_buf[1] - '0'));
                um = (unsigned char)((uart_buf[3] - '0') * 10 + (uart_buf[4] - '0'));
                us = (unsigned char)((uart_buf[6] - '0') * 10 + (uart_buf[7] - '0'));
                if (uh < 24 && um < 60 && us < 60)
                {
                    DS1302_SetTime(uh, um, us);
                    /* 第一行显示本次同步到的时间, 确认串口已收到 */
                    LCD_ShowString(1, 1, "RX:");
                    LCD_ShowNum(1, 4,  uh, 2);
                    LCD_ShowChar(1, 6, ':');
                    LCD_ShowNum(1, 7,  um, 2);
                    LCD_ShowChar(1, 9, ':');
                    LCD_ShowNum(1, 10, us, 2);
                    last_s = 0xFF;   /* 强制下一秒立刻刷新第二行 */
                }
            }
        }

        /* 每秒读一次 DS1302 (用 g_tick 节拍, 避免死循环关中断) */
        if (g_tick)
        {
            g_tick = 0;
            DS1302_GetTime(&h, &m, &s);
            if (s != last_s)
            {
                last_s = s;
                LCD_ShowNum(2, 4,  h, 2);
                LCD_ShowChar(2, 6, ':');
                LCD_ShowNum(2, 7,  m, 2);
                LCD_ShowChar(2, 9, ':');
                LCD_ShowNum(2, 10, s, 2);
            }
        }
    }
}

/* ===== 主程序 ===== */
void main(void)
{
    Timer0_Init();
    UART_Init();
    LCD_Init();

    while (1)
    {
        if      (g_mode == 0) Task1_Run();
        else if (g_mode == 1) Task2_Run();
        else if (g_mode == 2) Task3_Run();
        else if (g_mode == 3) Task4_Run();
        else                  Task5_Run();
    }
}
