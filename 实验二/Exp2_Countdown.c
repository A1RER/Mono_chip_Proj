/*
 * 实验2: 2位数码管循环倒计时 59 → 58 → ... → 0 → 59 → ...
 * ----------------------------------------------------------
 * 硬件连接:
 *   P0      - 数码管段选 (共阴极)
 *   P2      - 数码管位选 (高电平有效)
 *             使用第6位(P2.6=0x40)显示十位, 第7位(P2.7=0x80)显示个位
 *
 * 计时原理:
 *   定时器T0 模式1, 24MHz晶振, 每1ms产生一次中断
 *   中断内累计1000次 = 1秒, 每秒递减 Seconds
 *   显示刷新也在中断内完成 (每1ms切换一位, 2ms刷新一圈, 500Hz)
 */

#include <REGX52.H>

/* ---------- 共阴数码管段码: 0-9 ---------- */
unsigned char code SegCode[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

/* 2位显示: 十位=digit6(P2.6低有效=0xBF), 个位=digit7(P2.7低有效=0x7F) */
unsigned char code DigSel2[] = {0xBF, 0x7F};

volatile unsigned char Seconds = 59;    /* 当前秒数           */
unsigned char DispBuf[2];               /* [0]=十位, [1]=个位 */
unsigned char DigIdx  = 0;              /* 当前扫描位索引     */
unsigned int  MsCount = 0;              /* 毫秒累计           */

#define T0_RELOAD_H 0xF8
#define T0_RELOAD_L 0x30

/* ---------- 定时器T0初始化 ---------- */
void Timer0Init(void)
{
    TMOD &= 0xF0;       /* 清除T0配置位 */
    TMOD |= 0x01;       /* T0 模式1 (16位不自动重载) */
    TH0   = T0_RELOAD_H;/* 1ms@24MHz: 65536-2000 = 0xF830 */
    TL0   = T0_RELOAD_L;
    ET0   = 1;          /* 开T0中断   */
    EA    = 1;          /* 开总中断   */
    TR0   = 1;          /* 启动定时器 */
}

/* ---------- 定时器T0中断服务函数 (每1ms触发) ---------- */
void Timer0_ISR(void) interrupt 1
{
    TH0 = T0_RELOAD_H;  /* 手动重载 */
    TL0 = T0_RELOAD_L;

    /* --- 动态扫描: 每次中断切换一位 --- */
    P2 = 0xFF;                          /* 先消隐             */
    P0 = SegCode[DispBuf[DigIdx]];      /* 送段码             */
    P2 = DigSel2[DigIdx];               /* 选通位             */
    DigIdx ^= 1;                        /* 0↔1 切换           */

    /* --- 秒计数 --- */
    MsCount++;
    if (MsCount >= 1000)
    {
        MsCount = 0;
        if (Seconds == 0)
            Seconds = 59;
        else
            Seconds--;
        DispBuf[0] = Seconds / 10;      /* 更新显示缓冲 */
        DispBuf[1] = Seconds % 10;
    }
}

void main(void)
{
    /* 初始化显示缓冲 → "59" */
    DispBuf[0] = Seconds / 10;      /* 5 */
    DispBuf[1] = Seconds % 10;      /* 9 */

    Timer0Init();

    while (1);      /* 所有逻辑均在中断中完成 */
}
