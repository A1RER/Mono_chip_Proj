/*
 * alarm.c  --  报警驱动
 *
 * 蜂鸣器和 LED 驱动均为低电平有效 (BUZZER_ACTIVE_LOW / LED_ACTIVE_LOW = 1).
 * 翻转节拍来自 timer.c: g_alarm_high_tick (100ms) / g_alarm_low_tick (300ms).
 */

#include "alarm.h"
#include "timer.h"
#include "ds18b20.h"
#include <intrins.h>

#if BUZZER_ACTIVE_LOW
#define BUZZER_ON()    (BUZZER = 0)
#define BUZZER_OFF()   (BUZZER = 1)
#define BUZZER_TGL()   (BUZZER = !BUZZER)
#else
#define BUZZER_ON()    (BUZZER = 1)
#define BUZZER_OFF()   (BUZZER = 0)
#define BUZZER_TGL()   (BUZZER = !BUZZER)
#endif

#if BUZZER_PASSIVE
static void Buzzer_DelayToneHalfCycle(void)
{
    u8 i;

    for (i = 0; i < 28; i++)
    {
        _nop_(); _nop_(); _nop_(); _nop_();
    }
}

static void Buzzer_ToneMs(u8 ms)
{
    u16 cycles;

    cycles = (u16)ms * 2;      /* about 2 cycles/ms at roughly 2 kHz */
    while (cycles--)
    {
        BUZZER_ON();
        Buzzer_DelayToneHalfCycle();
        BUZZER_OFF();
        Buzzer_DelayToneHalfCycle();
    }
    BUZZER_OFF();
}
#endif

#if LED_ACTIVE_LOW
#define LED_HI_ON()    (LED_HIGH = 0)
#define LED_HI_OFF()   (LED_HIGH = 1)
#define LED_HI_TGL()   (LED_HIGH = !LED_HIGH)
#define LED_LO_ON()    (LED_LOW  = 0)
#define LED_LO_OFF()   (LED_LOW  = 1)
#define LED_LO_TGL()   (LED_LOW  = !LED_LOW)
#else
#define LED_HI_ON()    (LED_HIGH = 1)
#define LED_HI_OFF()   (LED_HIGH = 0)
#define LED_HI_TGL()   (LED_HIGH = !LED_HIGH)
#define LED_LO_ON()    (LED_LOW  = 1)
#define LED_LO_OFF()   (LED_LOW  = 0)
#define LED_LO_TGL()   (LED_LOW  = !LED_LOW)
#endif

void Alarm_Init(void)
{
    BUZZER_OFF();
    LED_HI_OFF();
    LED_LO_OFF();
    g_alarm_state = ALARM_NORMAL;
}

void Alarm_UpdateState(s16 cur_x10, s16 low_x10, s16 high_x10)
{
    u8 new_state;

    /* 采集停止时, 报警状态保持当前显示, 但不重新评估
     * (保留实验要求"停止后显示停止前的最后一次温度信息"的精神). */
    if (!g_collect_enable) return;

    if      (cur_x10 == DS18B20_TEMP_INVALID) new_state = ALARM_NORMAL;
    else if (cur_x10 > high_x10)              new_state = ALARM_HIGH;
    else if (cur_x10 < low_x10)               new_state = ALARM_LOW;
    else                                      new_state = ALARM_NORMAL;

    if (new_state != g_alarm_state)
    {
        g_alarm_state = new_state;
        BUZZER_OFF();
        LED_HI_OFF();
        LED_LO_OFF();
        g_lcd_update_flag = 1;
    }
}

void Alarm_Task(void)
{
    if (g_alarm_state == ALARM_HIGH)
    {
        if (g_alarm_high_tick)
        {
            g_alarm_high_tick = 0;
#if BUZZER_PASSIVE
            Buzzer_ToneMs(40);
#else
            BUZZER_TGL();
#endif
            LED_HI_TGL();
            LED_LO_OFF();
        }
    }
    else if (g_alarm_state == ALARM_LOW)
    {
        if (g_alarm_low_tick)
        {
            g_alarm_low_tick = 0;
#if BUZZER_PASSIVE
            Buzzer_ToneMs(80);
#else
            BUZZER_TGL();
#endif
            LED_LO_TGL();
            LED_HI_OFF();
        }
    }
    else
    {
        BUZZER_OFF();
        LED_HI_OFF();
        LED_LO_OFF();
    }
}
