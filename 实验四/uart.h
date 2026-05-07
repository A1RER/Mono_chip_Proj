/*
 * uart.h  --  串口 (9600 bps, 11.0592 MHz, 中断接收)
 *
 * 接收: 中断驱动, 收到 '\r' 或 '\n' 视为命令结束, 置 g_uart_cmd_ready.
 * 发送: 简单查询发送, 不开启发送中断.
 *
 * 缓冲区长度 32 字节, 足够容纳 "TIME=12:30:00\r\n" / "TEM_H=125.0\r\n" 等命令.
 */

#ifndef __UART_H__
#define __UART_H__

#include "config.h"

#define UART_BUF_LEN   32

extern volatile u8  g_uart_buf[UART_BUF_LEN];
extern volatile u8  g_uart_len;       /* 命令就绪时, 表示有效字节数 (不含 CR/LF) */

void UART_Init        (void);
void UART_SendChar    (char c);
void UART_SendString  (char *s);
void UART_SendUInt    (u16 v, u8 width);                /* 补 0 输出 */
void UART_SendTempX10 (s16 v);                          /* "25.6" / "-3.5" / "26.0" */
void UART_FlushBuffer (void);                           /* 主循环命令处理完后调用 */

#endif
