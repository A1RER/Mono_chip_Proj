/*
 * uart.c  --  串口驱动
 *
 * 9600 bps @ 11.0592 MHz: Timer1 mode2 重装值 0xFD.
 * 注意: Timer1 不能被本工程其他模块占用; Timer0 已被 timer.c 占用做系统节拍.
 */

#include "uart.h"

volatile u8 g_uart_buf[UART_BUF_LEN];
volatile u8 g_uart_len = 0;
static volatile u8 s_uart_idx = 0;

void UART_Init(void)
{
    PCON  &= 0x7F;                  /* SMOD=0 */
    SCON   = 0x50;                  /* 模式 1, 8 位 UART, 允许接收 */
    TMOD   = (TMOD & 0x0F) | 0x20;  /* Timer1 模式 2 (8 位自动重装), 保留 Timer0 设置 */
    T2CON  = 0x34;                  /* Timer2 baud generator for UART RX/TX */
    RCAP2H = 0xFF;                  /* 9600 bps @ 12 MHz */
    RCAP2L = 0xD9;
    TH2    = 0xFF;
    TL2    = 0xD9;
    ES     = 1;                     /* 允许串口中断 */
    EA     = 1;
}

/* 轮询发送时必须关 ES, 否则 TI 置位会先进 UART_ISR 被清掉,
 * 主循环 while(!TI) 永远等不到 → System_Init 末尾的 UART_SendString 会卡死,
 * 表现为 LCD 停在初始 "TIME 12:00:00 TEM 0.0 RUN" 不动. */
void UART_SendChar(char c)
{
    ES = 0;
    SBUF = (u8)c;
    while (!TI);
    TI = 0;
    ES = 1;
}

void UART_SendString(char *s)
{
    while (*s) UART_SendChar(*s++);
}

void UART_SendUInt(u16 v, u8 width)
{
    char buf[6];
    u8 i;
    for (i = width; i > 0; i--)
    {
        buf[i - 1] = (char)('0' + (v % 10));
        v /= 10;
    }
    buf[width] = '\0';
    UART_SendString(buf);
}

/* 把 temp_x10 输出成 "25.6" / "-3.5" / "26.0" */
void UART_SendTempX10(s16 v)
{
    u16 abs_v;
    u16 ip, fp;
    if (v < 0) { UART_SendChar('-'); abs_v = (u16)(-v); }
    else                            { abs_v = (u16)v;  }
    ip = abs_v / 10;
    fp = abs_v % 10;
    UART_SendUInt(ip, (ip >= 100) ? 3 : (ip >= 10 ? 2 : 1));
    UART_SendChar('.');
    UART_SendChar((char)('0' + fp));
}

void UART_FlushBuffer(void)
{
    ES = 0;
    s_uart_idx       = 0;
    g_uart_len       = 0;
    g_uart_cmd_ready = 0;
    ES = 1;
}

/* === 串口中断 === */
void UART_ISR(void) interrupt 4
{
    u8 c;
    if (RI)
    {
        RI = 0;
        c = SBUF;

        /* 命令已就绪但主循环还没处理: 丢弃新数据, 避免覆盖 */
        if (g_uart_cmd_ready) return;

        if (c == '\r' || c == '\n')
        {
            if (s_uart_idx > 0)
            {
                g_uart_buf[s_uart_idx] = '\0';
                g_uart_len             = s_uart_idx;
                g_uart_cmd_ready       = 1;
                s_uart_idx             = 0;
            }
        }
        else if (s_uart_idx < UART_BUF_LEN - 1)
        {
            g_uart_buf[s_uart_idx++] = c;
        }
        else
        {
            /* 溢出: 复位缓冲, 避免死锁 */
            s_uart_idx = 0;
        }
    }
    /* TX 走轮询 (UART_SendChar 会临时关 ES), 这里不处理 TI */
}
