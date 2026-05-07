/*
 * main.c  --  实验五: 基于 DS18B20 的温度传感器 (普中 51 / Proteus)
 *
 * === 功能总览 ===
 *   - DS18B20 12 位温度采集, 1 位小数, 支持负温度
 *   - LCD1602 五页面 K1 手动切换:
 *       页1 实时温度+时间 / 页2 上下限显示 / 页3 设置上限 /
 *       页4 设置下限     / 页5 采集状态+上传频率
 *   - AT24C08 每秒保存 1 条温度记录, 上下限/上传频率断电保存 (魔术字校验)
 *   - 蜂鸣器+LED 报警: 高温 100ms 频闪, 低温 300ms 频闪 (非阻塞)
 *   - 串口 9600 中断接收命令:
 *       start_tem / stop_tem / T=X / TEM_L=XX.X / TEM_H=XX.X / TIME=hh:mm:ss
 *   - 按键 (详见 key.h):
 *       K3 (P3^2) 短按 = 页面循环切换 (1->2->3->4->5->1)
 *       K4 (P3^3) 短按 = 当前参数 +0.5C (仅页3/页4 生效)
 *       K4 (P3^3) 长按 = 当前参数 -0.5C, 800ms 触发, 200ms 重复
 *     说明: 板上 K1=RXD/K2=TXD 与 UART 复用, 故采用 K3+K4 两键方案,
 *           老师建议的 "K3 减号键" 用 K4 长按代替.
 *
 * === 主循环节拍 ===
 *   1ms : 系统计数 (Timer0)
 *   50ms: 按键扫描
 *   1s  : 时间递增 / 温度读取 / EEPROM 保存 / LCD 刷新 / 串口上传调度
 *
 * === 时间 ===
 *   单片机内部用 Timer0 维护软件时钟 (1Hz 节拍 -> Time_Update 进位).
 *   开机默认 12:00:00. 不读取电脑系统时间, 仅通过串口命令 TIME=hh:mm:ss 校准.
 *
 * === 实验要求第 7 题 (选做): 温度曲线绘制 -- 未实现 ===
 *   原因: 本工程使用 LCD1602 字符型液晶, 仅 16x2 个 5x8 字符位,
 *   既无逐像素绘图能力, 也无足够分辨率显示连续曲线趋势.
 *   如需实现, 建议改用 SSD1306 OLED 12864 + 自建像素帧缓冲, 用最近 N
 *   个采样点描点连线. 因属选做项, 当前版本不实现, 主循环里也不保留
 *   任何曲线/Graph/Plot 相关数据结构与代码.
 */

#include "config.h"
#include "timer.h"
#include "lcd1602.h"
#include "ds18b20.h"
#include "i2c.h"
#include "at24c08.h"
#include "uart.h"
#include "command.h"
#include "key.h"
#include "alarm.h"

/* DS18B20_TEMP_INVALID = 0x7FFF, 直接交给 LCD_ShowTempX10 会渲染成
 * "P 76.7C" (因 hundreds 位 32 -> '0'+32 = 'P'), 必须在显示前拦截. */

/* === 全局变量定义 === */
bit  g_collect_enable   = 1;
bit  g_one_second_flag  = 0;
bit  g_lcd_update_flag  = 1;
bit  g_uart_cmd_ready   = 0;

s16  g_temp_x10         = 0;
s16  g_temp_high_x10    = DEF_TEMP_HIGH_X10;
s16  g_temp_low_x10     = DEF_TEMP_LOW_X10;

/* 默认 12:00:00, 不读电脑系统时间, 仅 Timer0 自走 + 串口 TIME=hh:mm:ss 校准 */
u8   g_hour             = 12;
u8   g_min              = 0;
u8   g_sec              = 0;

u8   g_upload_interval  = DEF_UPLOAD_INTERVAL;
u8   g_alarm_state      = ALARM_NORMAL;
u8   g_lcd_page         = LCD_PAGE_TIME_TEMP;

/* EEPROM 写入位置 (循环 0..200) */
u8   g_ee_write_idx     = 0;

/* 串口上传计数 (累计到 g_upload_interval 时上传) */
static u8 s_upload_cnt  = 0;

/* === 时间递增 === */
static void Time_Update(void)
{
    g_sec++;
    if (g_sec >= 60) { g_sec = 0; g_min++;  }
    if (g_min >= 60) { g_min = 0; g_hour++; }
    if (g_hour >= 24){ g_hour = 0; }
}

/* === LCD 显示辅助: 把 temp_x10 渲染成定宽 7 字符 "[sign][HHH.HC]"
 * 例: 25.6 -> "  25.6C", -3.5 -> "-  3.5C", 125.6 -> "+125.6C", 0.0 -> "   0.0C"
 * DS18B20 范围 -55..+125, 故百位最多 1, 但函数对 0..999 整数都安全.
 */
static void LCD_ShowTempX10(u8 line, u8 col, s16 v)
{
    u16 abs_v;
    u16 ip;
    u8  fp, hundreds, tens, ones;

    if (v < 0) { LCD_ShowChar(line, col++, '-'); abs_v = (u16)(-v); }
    else       { LCD_ShowChar(line, col++, ' '); abs_v = (u16)v;    }

    ip       = abs_v / 10;
    fp       = (u8)(abs_v % 10);
    hundreds = (u8)(ip / 100);
    tens     = (u8)((ip / 10) % 10);
    ones     = (u8)(ip % 10);

    LCD_ShowChar(line, col++, (hundreds > 0) ? (char)('0' + hundreds) : ' ');
    LCD_ShowChar(line, col++, (hundreds > 0 || tens > 0) ? (char)('0' + tens) : ' ');
    LCD_ShowChar(line, col++, (char)('0' + ones));
    LCD_ShowChar(line, col++, '.');
    LCD_ShowChar(line, col++, (char)('0' + fp));
    LCD_ShowChar(line, col++, 'C');
}

/* === LCD 五页面渲染 (按 K1 手动切换, 不再自动轮播) ===
 *
 * 共 16 列 x 2 行. LCD_ShowTempX10 固定输出 7 字符 "[sign][HHH.HC]".
 *
 * 页1 LCD_PAGE_TIME_TEMP:
 *   行1: "TIME HH:MM:SS   "
 *   行2: "TEMP [ HHH.HC] RUN" / "...STOP"
 * 页2 LCD_PAGE_LIMIT:
 *   行1: "H:[ HHH.HC]      "
 *   行2: "L:[ HHH.HC]      "
 * 页3 LCD_PAGE_SET_HIGH:
 *   行1: "SET HIGH        "
 *   行2: "H:[ HHH.HC]      "
 * 页4 LCD_PAGE_SET_LOW:
 *   行1: "SET LOW         "
 *   行2: "L:[ HHH.HC]      "
 * 页5 LCD_PAGE_STATE:
 *   行1: "STATE RUN       " / "STATE STOP      "
 *   行2: "T=Xs            "
 */
static void LCD_DrawPage(void)
{
    LCD_Clear();
    if (g_lcd_page == LCD_PAGE_TIME_TEMP)
    {
        LCD_ShowString(1, 1, "TIME ");
        LCD_ShowNum   (1, 6,  g_hour, 2);
        LCD_ShowChar  (1, 8,  ':');
        LCD_ShowNum   (1, 9,  g_min,  2);
        LCD_ShowChar  (1, 11, ':');
        LCD_ShowNum   (1, 12, g_sec,  2);

        LCD_ShowString(2, 1, "TEMP");
        if (g_temp_x10 == DS18B20_TEMP_INVALID)
        {
            if (g_ds18b20_status == DS18B20_STATUS_NO_ACK)
                LCD_ShowString(2, 5, " ERR1 ");
            else if (g_ds18b20_status == DS18B20_STATUS_READ_FF)
                LCD_ShowString(2, 5, " ERR2 ");
            else
                LCD_ShowString(2, 5, "  ERR! ");
        }
        else
            LCD_ShowTempX10(2, 5, g_temp_x10);      /* 5..11 */
        LCD_ShowString(2, 12, g_collect_enable ? "  RUN" : " STOP");
    }
    else if (g_lcd_page == LCD_PAGE_LIMIT)
    {
        LCD_ShowString(1, 1, "H:");
        LCD_ShowTempX10(1, 3, g_temp_high_x10);     /* 3..9 */
        LCD_ShowString(2, 1, "L:");
        LCD_ShowTempX10(2, 3, g_temp_low_x10);      /* 3..9 */
    }
    else if (g_lcd_page == LCD_PAGE_SET_HIGH)
    {
        LCD_ShowString(1, 1, "SET HIGH        ");
        LCD_ShowString(2, 1, "H:");
        LCD_ShowTempX10(2, 3, g_temp_high_x10);
    }
    else if (g_lcd_page == LCD_PAGE_SET_LOW)
    {
        LCD_ShowString(1, 1, "SET LOW         ");
        LCD_ShowString(2, 1, "L:");
        LCD_ShowTempX10(2, 3, g_temp_low_x10);
    }
    else /* LCD_PAGE_STATE */
    {
        LCD_ShowString(1, 1, g_collect_enable ? "STATE RUN       "
                                              : "STATE STOP      ");
        LCD_ShowString(2, 1, "T=");
        LCD_ShowChar  (2, 3, (char)('0' + g_upload_interval));
        LCD_ShowChar  (2, 4, 's');
    }
}

/* === 串口上传一帧: TIME=12:30:05,TEM=25.6C,TEM_L=20.0C,TEM_H=30.0C,STATE=RUN === */
static void UART_UploadFrame(void)
{
    UART_SendString("TIME=");
    UART_SendUInt(g_hour, 2); UART_SendChar(':');
    UART_SendUInt(g_min,  2); UART_SendChar(':');
    UART_SendUInt(g_sec,  2);

    UART_SendString(",TEM=");
    if (g_temp_x10 == DS18B20_TEMP_INVALID) UART_SendString("ERR");
    else UART_SendTempX10(g_temp_x10);
    UART_SendChar('C');

    UART_SendString(",TEM_L="); UART_SendTempX10(g_temp_low_x10);  UART_SendChar('C');
    UART_SendString(",TEM_H="); UART_SendTempX10(g_temp_high_x10); UART_SendChar('C');
    UART_SendString(",STATE=");
    UART_SendString(g_collect_enable ? "RUN" : "STOP");
    UART_SendString("\r\n");
}

/* === 系统初始化: 加载 EEPROM 参数, 启动 Timer0 / UART / LCD / DS18B20 === */
static void System_Init(void)
{
    s16 ee_high, ee_low;
    u8  ee_upload, ee_idx;

    /* 引脚初值 */
    BUZZER  = (BUZZER_ACTIVE_LOW ? 1 : 0);
    LED_HIGH = (LED_ACTIVE_LOW ? 1 : 0);
    LED_LOW  = (LED_ACTIVE_LOW ? 1 : 0);
    I2C_SCL = 1;
    I2C_SDA = 1;

    Timer0_Init();
    UART_Init();
    LCD_Init();
    Alarm_Init();

    /* 从 EEPROM 读取参数 (若魔术字未写过, 用默认值并写回) */
    if (AT24C08_LoadParams(&ee_high, &ee_low, &ee_upload, &ee_idx))
    {
        if (ee_high > ee_low &&
            ee_high <= LIMIT_MAX_X10 && ee_low >= LIMIT_MIN_X10)
        {
            g_temp_high_x10 = ee_high;
            g_temp_low_x10  = ee_low;
        }
        if (ee_upload >= 1 && ee_upload <= 8) g_upload_interval = ee_upload;
        if (ee_idx < EE_REC_MAX)              g_ee_write_idx    = ee_idx;
    }
    else
    {
        AT24C08_SaveParams(g_temp_high_x10, g_temp_low_x10,
                           g_upload_interval, g_ee_write_idx);
    }

    /* 启动一次 DS18B20 转换, 让第一秒就能读到结果 */
    DS18B20_StartConvert();

    LCD_DrawPage();
    UART_SendString("DS18B20 SYS BOOT\r\n");
}

/* === 主程序 === */
void main(void)
{
    System_Init();

    while (1)
    {
        /* --- 50ms 节拍: 按键扫描 --- */
        if (g_50ms_tick)
        {
            g_50ms_tick = 0;
            Key_Scan();
        }

        /* --- 串口命令处理 --- */
        if (g_uart_cmd_ready)
        {
            Command_Parse();
        }

        /* --- 1s 节拍 --- */
        if (g_one_second_flag)
        {
            g_one_second_flag = 0;

            Time_Update();

            if (g_collect_enable)
            {
                /* 上次的转换结果已有 ~1s, 一定大于 750ms, 可以读了 */
                g_temp_x10 = DS18B20_ReadTempX10();
                /* 启动下一次转换, 下一秒读 */
                DS18B20_StartConvert();

                /* 教学场景: 每秒保存到 EEPROM. 实际工程中 EEPROM 写入寿命
                 * 约 1,000,000 次, 1Hz 持续写入会 11 天耗尽寿命. */
                AT24C08_SaveRecord(&g_ee_write_idx,
                                   g_hour, g_min, g_sec, g_temp_x10);
                /* write_idx 同步保存到参数区 (避免掉电后从 0 重新覆盖) */
                AT24C08_SaveParams(g_temp_high_x10, g_temp_low_x10,
                                   g_upload_interval, g_ee_write_idx);
            }

            Alarm_UpdateState(g_temp_x10, g_temp_low_x10, g_temp_high_x10);

            /* 时间每秒变, 强制刷新当前页 (页面切换由 K1 手动触发, 不自动轮播) */
            g_lcd_update_flag = 1;
            LCD_DrawPage();

            /* 串口上传调度: stop_tem 后停止周期上传, start_tem 后恢复 */
            if (g_collect_enable)
            {
                s_upload_cnt++;
                if (s_upload_cnt >= g_upload_interval)
                {
                    s_upload_cnt = 0;
                    UART_UploadFrame();
                }
            }
            else
            {
                s_upload_cnt = 0;
            }
        }
        else if (g_lcd_update_flag)
        {
            /* 按键 / 串口命令引发的即时刷新 (不等下一秒) */
            g_lcd_update_flag = 0;
            LCD_DrawPage();
        }

        /* --- 报警节拍 (内部消费 100/300ms tick) --- */
        Alarm_Task();
    }
}
