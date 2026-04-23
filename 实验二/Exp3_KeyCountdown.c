/*
 * 实验3: 带独立按键控制的2位数码管倒计时
 * ----------------------------------------
 * 硬件连接:
 *   P0      - 数码管段选 (共阴极)
 *   P2      - 数码管位选 (高电平有效)
 *             十位=digit6(P2.6=0x40), 个位=digit7(P2.7=0x80)
 *   K1=P3^1 - 开始计时   (按下低电平, 松开高电平)
 *   K2=P3^0 - 暂停/继续  (按下低电平)
 *   K3=P3^2 - 计时清零   (接 INT0, 下降沿外部中断)
 *
 * 功能说明:
 *   上电: 显示 59, 停止状态
 *   K1  : 从 59 开始倒计时
 *   K2  : 按一次暂停; 再按一次从停止时刻继续
 *   K3  : 计时清零, 显示 00, 停止
 *
 * 中断资源:
 *   定时器T0 (中断1) - 1ms定时, 负责显示刷新/秒计数/按键扫描
 *   外部INT0  (中断0) - 下降沿触发, 负责K3清零
 */

#include <REGX52.H>

sbit K1 = P3^1;     /* 开始   */
sbit K2 = P3^0;     /* 暂停/继续 */
/* K3 = P3^2 (INT0) 通过外部中断处理, 无需 sbit 声明 */

/* ---------- 共阴数码管段码: 0-9 ---------- */
unsigned char code SegCode[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

/* 2位显示: 74HC138位选, 十位=digit7(0x04), 个位=digit8(0x00) */
unsigned char code DigSel2[] = {0x04, 0x00};

/* ---------- 状态定义 ---------- */
#define ST_STOP  0      /* 停止 */
#define ST_RUN   1      /* 运行 */
#define ST_PAUSE 2      /* 暂停 */

volatile unsigned char Seconds = 59;       /* 当前秒数     */
volatile unsigned char State   = ST_STOP;  /* 当前状态     */
unsigned char DispBuf[2];                  /* 显示缓冲     */
unsigned char DigIdx  = 0;                 /* 扫描位索引   */
unsigned int  MsCount = 0;                 /* 毫秒累计     */
unsigned char KeyScanTick = 0;             /* 按键扫描分频 */

#define T0_RELOAD_H 0xFC
#define T0_RELOAD_L 0x18

static void HandleK1Press(void)
{
    Seconds    = 59;
    MsCount    = 0;
    DispBuf[0] = 5;
    DispBuf[1] = 9;
    State      = ST_RUN;
}

static void HandleK2Press(void)
{
    if (State == ST_RUN)
        State = ST_PAUSE;
    else if (State == ST_PAUSE)
        State = ST_RUN;
}

/* 每5ms扫描一次K1/K2, 在中断内完成按键消抖和按下事件触发 */
static void KeyScanInISR(void)
{
    static unsigned char k1Cnt = 0, k2Cnt = 0;
    static unsigned char k1Latched = 0, k2Latched = 0;

    if (K1 == 0)
    {
        if (k1Cnt < 4) k1Cnt++;
        if (k1Cnt >= 4 && k1Latched == 0)
        {
            k1Latched = 1;
            HandleK1Press();
        }
    }
    else
    {
        k1Cnt = 0;
        k1Latched = 0;
    }

    if (K2 == 0)
    {
        if (k2Cnt < 4) k2Cnt++;
        if (k2Cnt >= 4 && k2Latched == 0)
        {
            k2Latched = 1;
            HandleK2Press();
        }
    }
    else
    {
        k2Cnt = 0;
        k2Latched = 0;
    }
}

/* ---------- 定时器T0初始化 ---------- */
void Timer0Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;       /* T0 模式1 */
    TH0   = T0_RELOAD_H;/* 1ms@24MHz */
    TL0   = T0_RELOAD_L;
    ET0   = 1;
    EA    = 1;
    TR0   = 1;
}

/* ---------- 外部中断INT0初始化 (K3) ---------- */
void INT0Init(void)
{
    IT0 = 1;            /* 下降沿触发 */
    EX0 = 1;            /* 使能INT0   */
}

/* ========================================
 * 定时器T0 中断服务函数 (interrupt 1, 每1ms)
 * ======================================== */
void Timer0_ISR(void) interrupt 1
{
    TH0 = T0_RELOAD_H;  /* 手动重载 */
    TL0 = T0_RELOAD_L;

    /* --- 动态扫描 --- */
    P0 = 0x00;
    P2 = DigSel2[DigIdx];
    P0 = SegCode[DispBuf[DigIdx]];
    DigIdx ^= 1;

    /* --- 按键扫描(5ms一次) --- */
    KeyScanTick++;
    if (KeyScanTick >= 5)
    {
        KeyScanTick = 0;
        KeyScanInISR();
    }

    /* --- 秒计数 (仅 ST_RUN 状态) --- */
    if (State == ST_RUN)
    {
        MsCount++;
        if (MsCount >= 1000)
        {
            MsCount = 0;
            if (Seconds == 0)
                Seconds = 59;
            else
                Seconds--;
            DispBuf[0] = Seconds / 10;
            DispBuf[1] = Seconds % 10;
        }
    }
}

/* ========================================
 * 外部INT0 中断服务函数 (interrupt 0)
 * K3: 计时清零, 显示 00, 停止
 * ======================================== */
void INT0_ISR(void) interrupt 0
{
    State      = ST_STOP;
    Seconds    = 0;
    MsCount    = 0;
    DispBuf[0] = 0;
    DispBuf[1] = 0;
}

/* ========================================
 * 主函数
 * ======================================== */
void main(void)
{
    /* 初始显示 "59" */
    DispBuf[0] = Seconds / 10;  /* 5 */
    DispBuf[1] = Seconds % 10;  /* 9 */

    Timer0Init();
    INT0Init();

    while (1);
}
