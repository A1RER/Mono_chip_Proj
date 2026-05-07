/*
 * command.c  --  串口命令解析
 *
 * 在主循环中, 当 g_uart_cmd_ready==1 时调用 Command_Parse().
 * 解析完成后必须 UART_FlushBuffer() 清空缓冲, 否则后续命令收不到.
 *
 * 解析后涉及参数变化时, 同步修改 g_*, 并由 LCD_Update() 在下一秒刷新.
 * 为了让按键 / 串口修改阈值后立即看见, 也置 g_lcd_update_flag = 1.
 */

#include "command.h"
#include "uart.h"
#include "at24c08.h"

extern u8 g_ee_write_idx;       /* main.c 中维护 */

/* 局部字符串比较, 不引入 string.h */
static bit str_eq(volatile u8 *s, char *cmp)
{
    while (*cmp)
    {
        if (*s != (u8)*cmp) return 0;
        s++; cmp++;
    }
    return (*s == 0);   /* s 必须以 '\0' 结束 */
}

static bit str_starts(volatile u8 *s, char *prefix, u8 *out_offset)
{
    u8 i = 0;
    while (prefix[i])
    {
        if (s[i] != (u8)prefix[i]) return 0;
        i++;
    }
    *out_offset = i;
    return 1;
}

/* 把 "XX.X" 形式解析成 x10 整数. 允许前导符号. 失败返回 0. */
static bit parse_temp_x10(volatile u8 *s, s16 *out)
{
    s16 sign = 1;
    s16 ip = 0;
    s16 fp = 0;
    u8 has_int = 0, has_frac = 0;

    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    while (*s >= '0' && *s <= '9')
    {
        ip = ip * 10 + (*s - '0');
        if (ip > 999) return 0;
        has_int = 1;
        s++;
    }
    if (*s == '.')
    {
        s++;
        if (*s >= '0' && *s <= '9') { fp = *s - '0'; has_frac = 1; s++; }
        /* 多余的小数位丢弃但不报错 */
        while (*s >= '0' && *s <= '9') s++;
    }
    if (!has_int) return 0;
    if (*s != 0) return 0;
    (void)has_frac;
    *out = (s16)(sign * (ip * 10 + fp));
    return 1;
}

/* 解析 "hh:mm:ss" */
static bit parse_time(volatile u8 *s, u8 *h, u8 *m, u8 *sec)
{
    u8 a, b, c, d, e, f;
    if (s[2] != ':' || s[5] != ':' || s[8] != 0) return 0;
    a = s[0]; b = s[1]; c = s[3]; d = s[4]; e = s[6]; f = s[7];
    if (a < '0' || a > '9' || b < '0' || b > '9') return 0;
    if (c < '0' || c > '9' || d < '0' || d > '9') return 0;
    if (e < '0' || e > '9' || f < '0' || f > '9') return 0;
    *h   = (u8)((a - '0') * 10 + (b - '0'));
    *m   = (u8)((c - '0') * 10 + (d - '0'));
    *sec = (u8)((e - '0') * 10 + (f - '0'));
    if (*h >= 24 || *m >= 60 || *sec >= 60) return 0;
    return 1;
}

/* === 主解析入口 === */
void Command_Parse(void)
{
    volatile u8 *s = g_uart_buf;
    u8 off;
    s16 v;
    u8 h, m, sec;

    if (str_eq(s, "start_tem"))
    {
        g_collect_enable = 1;
        g_lcd_update_flag = 1;
        UART_SendString("OK:START\r\n");
    }
    else if (str_eq(s, "stop_tem"))
    {
        g_collect_enable = 0;
        g_lcd_update_flag = 1;
        UART_SendString("OK:STOP\r\n");
    }
    else if (str_starts(s, "T=", &off))
    {
        /* 实验要求: 1 < X < 9, 即 2..8.
         * 默认值 1 与之冲突 (默认 1 是 1Hz 上传), 命令仅接受 2..8;
         * 想恢复默认 1Hz 上传, 可用 stop_tem -> start_tem 或重启. */
        if (s[off] >= '2' && s[off] <= '8' && s[off + 1] == 0)
        {
            g_upload_interval = s[off] - '0';
            g_lcd_update_flag = 1;
            UART_SendString("OK:T=");
            UART_SendChar((char)('0' + g_upload_interval));
            UART_SendString("\r\n");
        }
        else
        {
            UART_SendString("ERROR:VALUE\r\n");
        }
    }
    else if (str_starts(s, "TEM_L=", &off))
    {
        if (parse_temp_x10(s + off, &v) && v >= LIMIT_MIN_X10 && v < g_temp_high_x10)
        {
            g_temp_low_x10    = v;
            g_lcd_update_flag = 1;
            AT24C08_SaveParams(g_temp_high_x10, g_temp_low_x10,
                               g_upload_interval, g_ee_write_idx);
            UART_SendString("OK:TEM_L=");
            UART_SendTempX10(g_temp_low_x10);
            UART_SendString("C\r\n");
        }
        else
        {
            UART_SendString("ERROR:VALUE\r\n");
        }
    }
    else if (str_starts(s, "TEM_H=", &off))
    {
        if (parse_temp_x10(s + off, &v) && v <= LIMIT_MAX_X10 && v > g_temp_low_x10)
        {
            g_temp_high_x10   = v;
            g_lcd_update_flag = 1;
            AT24C08_SaveParams(g_temp_high_x10, g_temp_low_x10,
                               g_upload_interval, g_ee_write_idx);
            UART_SendString("OK:TEM_H=");
            UART_SendTempX10(g_temp_high_x10);
            UART_SendString("C\r\n");
        }
        else
        {
            UART_SendString("ERROR:VALUE\r\n");
        }
    }
    else if (str_starts(s, "TIME=", &off))
    {
        if (parse_time(s + off, &h, &m, &sec))
        {
            EA = 0;
            g_hour = h; g_min = m; g_sec = sec;
            EA = 1;
            g_lcd_update_flag = 1;
            UART_SendString("OK:TIME=");
            UART_SendUInt(h,   2); UART_SendChar(':');
            UART_SendUInt(m,   2); UART_SendChar(':');
            UART_SendUInt(sec, 2);
            UART_SendString("\r\n");
        }
        else
        {
            UART_SendString("ERROR:VALUE\r\n");
        }
    }
    else
    {
        UART_SendString("ERROR:CMD\r\n");
    }

    UART_FlushBuffer();
}
