/*
 * 实验1: 数码管动态显示 - 日期 / 学号 / 生日
 * -----------------------------------------------
 * 硬件连接:
 *   P0      - 数码管段选 (共阴极, a=P0.0 ... dp=P0.7)
 *   P2      - 数码管位选 (低电平有效, P2.0=最左位 ... P2.7=最右位)
 *
 * 显示内容每隔约2秒轮流切换:
 *   1. 今日日期: 2026-04-09 → 20260409
 *   2. 学号    (请修改 StudentID 数组)
 *   3. 生日    (请修改 Birthday  数组)
 *
 * 修改说明: 字母用0代替, 不足8位在前面补零, 超过8位只取前8位
 */

#include <REGX52.H>
#include "Delay.h"

/* ---------- 共阴数码管段码: 0-9 ---------- */
unsigned char code SegCode[] = {
    0x3F,   /* 0 */
    0x06,   /* 1 */
    0x5B,   /* 2 */
    0x4F,   /* 3 */
    0x66,   /* 4 */
    0x6D,   /* 5 */
    0x7D,   /* 6 */
    0x07,   /* 7 */
    0x7F,   /* 8 */
    0x6F    /* 9 */
};

/* ---------- 74HC138位选码: P2.2=A, P2.3=B, P2.4=C ---------- */
unsigned char code DigSel[] = {
    0x1C, 0x18, 0x14, 0x10, 0x0C, 0x08, 0x04, 0x00
};

/* ========== 请在此处修改为自己的学号和生日(原始字符串) ========== */
char DateRaw[]      = "20260409";  /* 今日日期 */
char StudentIDRaw[] = "24210607";  /* 字母会被转成0 */
char BirthdayRaw[]  = "20060722";  /* yyyymmdd */

unsigned char Date[8];
unsigned char StudentID[8];
unsigned char Birthday[8];
/* ============================================================ */

unsigned char DispBuf[8];   /* 当前显示缓冲区 */

/*
 * NormalizeTo8Digits:
 *   规则: 字母->0, 不足8位左侧补0, 超过8位只取前8位
 *   其他字符(如'-')会被忽略
 */
void NormalizeTo8Digits(char *src, unsigned char *dst)
{
    unsigned char tmp[8];
    unsigned char i, n = 0;
    char c;

    while ((c = *src++) != '\0' && n < 8)
    {
        if (c >= '0' && c <= '9')
            tmp[n++] = c - '0';
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            tmp[n++] = 0;
    }

    if (n < 8)
    {
        for (i = 0; i < 8 - n; i++) dst[i] = 0;
        for (i = 0; i < n; i++)     dst[8 - n + i] = tmp[i];
    }
    else
    {
        for (i = 0; i < 8; i++) dst[i] = tmp[i];
    }
}

void InitContentData(void)
{
    NormalizeTo8Digits(DateRaw, Date);
    NormalizeTo8Digits(StudentIDRaw, StudentID);
    NormalizeTo8Digits(BirthdayRaw, Birthday);
}

/*
 * DisplayFrame: 完整扫描8位数码管一帧 (约8ms)
 *   每位亮1ms, 刷新率 = 1000/8 = 125Hz
 */
void DisplayFrame(void)
{
    unsigned char i;
    unsigned char num;
    for (i = 0; i < 8; i++)
    {
        num = DispBuf[i];
        if (num > 9) num = 0;
        P0 = 0x00;                      /* 消隐: 清段码     */
        P2 = DigSel[i];                 /* 74HC138选通第i位 */
        P0 = SegCode[num];              /* 输出段码         */
        Delay(1);                       /* 亮 1ms           */
    }
    P0 = 0x00;                          /* 末尾消隐         */
}

/* 将第 idx 组数据加载进显示缓冲 (0=日期, 1=学号, 2=生日) */
void LoadContent(unsigned char idx)
{
    unsigned char i;
    unsigned char *p;
    if      (idx == 0) p = Date;
    else if (idx == 1) p = StudentID;
    else               p = Birthday;
    for (i = 0; i < 8; i++) DispBuf[i] = p[i];
}

void main(void)
{
    unsigned int  frameCount = 0;
    unsigned char contentIdx = 0;

    InitContentData();
    LoadContent(0);     /* 上电先显示日期 */

    while (1)
    {
        DisplayFrame();     /* ~8ms/帧 */
        frameCount++;

        /* 250帧 × 8ms ≈ 2秒, 切换显示内容 */
        if (frameCount >= 250)
        {
            frameCount = 0;
            contentIdx = (contentIdx + 1) % 3;
            LoadContent(contentIdx);
        }
    }
}
