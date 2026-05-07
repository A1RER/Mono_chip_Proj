/*
 * timer.c  --  Timer0 1ms 系统节拍
 *
 * 11.0592 MHz: 机器周期 = 12/Fosc = 1.0851 us
 * 1ms 需要约 921 个机器周期
 * 重装值 = 65536 - 921 = 64615 = 0xFC67
 *
 * 注意: 中断里只做计数和置位, 不做耗时工作.
 */

#include "timer.h"

volatile bit g_50ms_tick       = 0;
volatile bit g_alarm_high_tick = 0;
volatile bit g_alarm_low_tick  = 0;
volatile u16 g_ms_counter      = 0;

static volatile u16 s_50ms_cnt    = 0;
static volatile u16 s_alarm_h_cnt = 0;
static volatile u16 s_alarm_l_cnt = 0;

void Timer0_Init(void)
{
    TMOD &= 0xF0;       /* 清 Timer0 控制位, 保留 Timer1 设置给串口 */
    TMOD |= 0x01;       /* Timer0 模式 1 (16 位定时器) */
    TH0   = 0xFC;
    TL0   = 0x67;
    ET0   = 1;          /* 允许 Timer0 中断 */
    TR0   = 1;          /* 启动 Timer0 */
    EA    = 1;          /* 开总中断 */
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xFC;
    TL0 = 0x67;

    g_ms_counter++;
    if (g_ms_counter >= 1000)
    {
        g_ms_counter     = 0;
        g_one_second_flag = 1;
    }

    if (++s_50ms_cnt >= 50)    { s_50ms_cnt    = 0; g_50ms_tick       = 1; }
    if (++s_alarm_h_cnt >= 100){ s_alarm_h_cnt = 0; g_alarm_high_tick = 1; }
    if (++s_alarm_l_cnt >= 300){ s_alarm_l_cnt = 0; g_alarm_low_tick  = 1; }
}
