/*
 * key.c  --  按键扫描 (2 键设计: KEY1=板上K3 翻页, KEY2=板上K4 调阈值)
 *
 * 用 50ms 节拍轮询 (timer.c 每 50ms 置 g_50ms_tick).
 * 每次 Key_Scan() 调用代表一个 50ms tick.
 *
 * 设计:
 *   - K3 (KEY1) 短按 -> 5 页循环切换. 边沿触发, 持续按住不重复.
 *   - K4 (KEY2) 短按 -> 当前页是 SET_HIGH/SET_LOW 时, 阈值 +0.5C.
 *   - K4 (KEY2) 长按 -> 持续 >=800ms 触发, 之后每 200ms 重复, -0.5C.
 *
 * 长按判定: 持续按下 16 个 tick (=800ms), 之后每 4 个 tick (=200ms) 重复.
 * 这样在 SET 页可以"长按快速降温".
 */

#include "key.h"
#include "at24c08.h"

extern u8 g_ee_write_idx;

#define DEBOUNCE_TICKS      2       /* 2 * 50ms = 100ms */
#define LONG_PRESS_TICKS    16      /* 16 * 50ms = 800ms */
#define REPEAT_TICKS        4       /* 4 * 50ms = 200ms */

static u8  s_k1_cnt   = 0;
static bit s_k1_done  = 0;          /* 防止持续按住重复触发 */

static u8  s_k2_cnt   = 0;
static u8  s_k2_repeat = 0;

/* 把当前正在编辑的阈值调整 step_x10 (单位 0.1C).
 * 仅在 SET_HIGH / SET_LOW 页面有效. 其他页面静默忽略. */
static void adjust_threshold(s16 step_x10)
{
    s16 *target;

    if      (g_lcd_page == LCD_PAGE_SET_HIGH) target = &g_temp_high_x10;
    else if (g_lcd_page == LCD_PAGE_SET_LOW)  target = &g_temp_low_x10;
    else return;

    *target += step_x10;

    /* 防止上下限交叉, 至少留 0.5C 间距 */
    if (g_temp_high_x10 <= g_temp_low_x10)
    {
        if (g_lcd_page == LCD_PAGE_SET_HIGH)
            g_temp_high_x10 = g_temp_low_x10 + 5;
        else
            g_temp_low_x10  = g_temp_high_x10 - 5;
    }
    if (g_temp_high_x10 > LIMIT_MAX_X10) g_temp_high_x10 = LIMIT_MAX_X10;
    if (g_temp_low_x10  < LIMIT_MIN_X10) g_temp_low_x10  = LIMIT_MIN_X10;

    g_lcd_update_flag = 1;
    AT24C08_SaveParams(g_temp_high_x10, g_temp_low_x10,
                       g_upload_interval, g_ee_write_idx);
}

/* K3 短按: 5 页循环切换 */
static void on_key1_short(void)
{
    g_lcd_page = (u8)((g_lcd_page + 1) % LCD_PAGE_COUNT);
    g_lcd_update_flag = 1;
}

void Key_Scan(void)
{
    /* === KEY1 / 板上 K3: 翻页, 仅短按 === */
    if (KEY1 == 0)
    {
        if (s_k1_cnt < 0xFF) s_k1_cnt++;
        if (s_k1_cnt == DEBOUNCE_TICKS && !s_k1_done)
        {
            on_key1_short();
            s_k1_done = 1;
        }
    }
    else
    {
        s_k1_cnt = 0;
        s_k1_done = 0;
    }

    /* === KEY2 / 板上 K4: 短按 +0.5, 长按 -0.5 (重复) ===
     * 短按判定要等到松开瞬间才知道, 所以放在 release 分支.
     */
    if (KEY2 == 0)
    {
        if (s_k2_cnt < 0xFF) s_k2_cnt++;
        if (s_k2_cnt == LONG_PRESS_TICKS)
        {
            adjust_threshold(-5);    /* 进入长按: 触发一次 -0.5 */
            s_k2_repeat = 0;
        }
        else if (s_k2_cnt > LONG_PRESS_TICKS)
        {
            s_k2_repeat++;
            if (s_k2_repeat >= REPEAT_TICKS)
            {
                s_k2_repeat = 0;
                adjust_threshold(-5);
            }
        }
    }
    else
    {
        if (s_k2_cnt >= DEBOUNCE_TICKS && s_k2_cnt < LONG_PRESS_TICKS)
        {
            adjust_threshold(+5);    /* 短按释放: +0.5 */
        }
        s_k2_cnt = 0;
        s_k2_repeat = 0;
    }
}
