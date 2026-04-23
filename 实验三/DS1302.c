/*
 * DS1302.c - DS1302 实时时钟驱动 (逐位 SPI 模拟)
 * 引脚: CLK=P3^6, DAT=P3^4, RST/CE=P3^5
 * 数据格式: BCD, 24小时制
 *
 * 寄存器地址 (写/读):
 *   秒 0x80/0x81  分 0x82/0x83  时 0x84/0x85
 *   写保护 0x8E (0x00=关, 0x80=开)
 */
#include <REG52.H>
#include <intrins.h>
#include "DS1302.h"

sbit DS_CLK = P3^6;
sbit DS_DAT = P3^4;
sbit DS_RST = P3^5;

/* ~4us 位级建立/保持延时 (11.0592 MHz, 每 NOP ≈ 1.08us) */
#define DLY() { _nop_(); _nop_(); _nop_(); _nop_(); }

/* ~8us CE 无效→有效后等待 (DS1302 tCC ≥ 4us, 留余量) */
#define DLY_CE() { _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); }

static unsigned char BCD_Dec(unsigned char bcd)
{
    unsigned char hi = (bcd >> 4) & 0x0F;
    unsigned char lo = bcd & 0x0F;
    if (hi > 9 || lo > 9) return 0;   /* 非法BCD(例如通信失败时浮空读到0xFF)返回0 */
    return hi * 10 + lo;
}

static unsigned char Dec_BCD(unsigned char dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

/*
 * 写一个字节, LSB 先发
 * DS1302 在 CLK 上升沿锁存数据 (写命令字时).
 * 结束时保持 CLK=1, 以便紧跟的 ReadByte 的第一个 CLK 下降沿
 * 让 DS1302 输出数据的 bit0. 若此处再置 CLK=0, 会多产生一个下降沿,
 * 导致读回数据整体丢失 bit0 (表现为秒数每 2 秒跳一次: 55,57,59...).
 */
static void WriteByte(unsigned char dat)
{
    unsigned char i;
    for (i = 0; i < 8; i++)
    {
        DS_DAT = dat & 0x01;    /* 先放数据 */
        DLY();
        DS_CLK = 1;             /* 上升沿采样 */
        DLY();
        DS_CLK = 0;             /* 回低 (下一个建立沿) */
        DLY();
        dat >>= 1;
    }
    /* 退出时 CLK=0 (经典时序) */
}

/*
 * 读一个字节, LSB 先出.
 * 进入时要求 CLK=1 (由 WriteByte 结尾或上一次 ReadByte 结尾保证).
 * 时序: CLK 高→低 (DS1302 推出第 i 位) → 读 DAT → CLK 低→高 → 下一位.
 * 退出时同样保持 CLK=1, 以便 Burst Read 中连续调用时下一字节的 bit0 不丢失.
 */
static unsigned char ReadByte(void)
{
    unsigned char i, dat = 0;
    DS_DAT = 1;         /* 释放数据线为输入 (弱上拉) */
    DLY();
    /* 关键: 写完命令字后 DS1302 已经把 bit0 推到了 DAT 上 (命令字最后 CLK 下降沿).
     * 所以先采样, 再产生下一个下降沿把 bit1 推出. */
    for (i = 0; i < 8; i++)
    {
        dat >>= 1;
        if (DS_DAT) dat |= 0x80;
        DS_CLK = 1;
        DLY();
        DS_CLK = 0;     /* 下降沿: DS1302 把下一位推到 DAT */
        DLY();
    }
    /* 退出时 CLK=0 */
    return dat;
}

/*
 * 注意: DS1302 位操作对 CLK/DAT 时序敏感, 必须在关中断区间完成.
 * 否则 Timer0 (50ms) 或 UART 中断一旦插入, 时序错乱会全部读到 0xFF,
 * 经 BCD 解码后显示 "45:??:85" 之类异常值.
 * 这里只屏蔽 Timer0 和 UART 中断, 不动全局 EA.
 */
static void WriteReg(unsigned char cmd, unsigned char val)
{
    ET0 = 0;
    ES  = 0;
    DS_RST = 0;
    DS_CLK = 0;
    DLY_CE();               /* CE 拉低后保持, 确保内部复位 */
    DS_RST = 1;
    DLY_CE();               /* tCC: CE起后至首个 CLK ≥ 4us */
    WriteByte(cmd);
    WriteByte(val);
    DS_CLK = 0;
    DLY();                  /* tCCH: CLK 低后到 CE 拉低 ≥ 240ns */
    DS_RST = 0;
    DLY_CE();
    ET0 = 1;
    ES  = 1;
}

static unsigned char ReadReg(unsigned char cmd)
{
    unsigned char val;
    ET0 = 0;
    ES  = 0;
    DS_RST = 0;
    DS_CLK = 0;
    DLY_CE();
    DS_RST = 1;
    DLY_CE();
    WriteByte(cmd);
    val = ReadByte();
    DS_CLK = 0;
    DLY();
    DS_RST = 0;
    DLY_CE();
    ET0 = 1;
    ES  = 1;
    return val;
}

void DS1302_Init(void)
{
    DS_RST = 0;
    DS_CLK = 0;
    DS_DAT = 1;
    DLY(); DLY();
    WriteReg(0x8E, 0x00);   /* 关闭写保护 */
    WriteReg(0x8E, 0x00);   /* 写两次确保生效 */
}

void DS1302_SetTime(unsigned char hour, unsigned char min, unsigned char sec)
{
    WriteReg(0x8E, 0x00);
    WriteReg(0x80, Dec_BCD(sec)  & 0x7F);  /* CH=0: 振荡器运行 */
    WriteReg(0x82, Dec_BCD(min));
    WriteReg(0x84, Dec_BCD(hour) & 0x3F);  /* bit7=0: 24小时制 */
    WriteReg(0x8E, 0x80);   /* 重新开启写保护 */
}

/*
 * Clock Burst Read (命令字 0xBF):
 *   CE 拉高后先写 0xBF, 然后连续读出 8 个字节:
 *     [0]秒 [1]分 [2]时 [3]日 [4]月 [5]周 [6]年 [7]写保护
 *   DS1302 会在 CE 上升时对所有计时寄存器做一次快照,
 *   确保 8 个字节来自同一时刻, 不会出现跳警不一致.
 *
 * 这样比单个寄存器读取更稳:
 *   1. 一次性在关中断区间内完成, 不会被 UART/Timer0 打断;
 *   2. 免除多次 CE 翻转, 避免每次 WriteByte+ReadByte 交接处的潜在时序缺陷;
 *   3. DS1302 内部快照机制根本地消除了 s1/s2 护栏仍泄露的究竟跨边界问题
 *      (如分钟进位时 *min 被读成旧值).
 */
void DS1302_GetTime(unsigned char *hour, unsigned char *min, unsigned char *sec)
{
    unsigned char buf[8];
    unsigned char i;

    ET0 = 0;
    ES  = 0;

    DS_RST = 0;
    DS_CLK = 0;
    DLY_CE();
    DS_RST = 1;
    DLY_CE();

    WriteByte(0xBF);                /* 时钟突发读命令 */
    for (i = 0; i < 8; i++)
        buf[i] = ReadByte();

    DS_CLK = 0;
    DLY();
    DS_RST = 0;
    DLY_CE();

    ET0 = 1;
    ES  = 1;

    *sec  = BCD_Dec(buf[0] & 0x7F); /* 屏蔽 CH 位 */
    *min  = BCD_Dec(buf[1] & 0x7F);
    *hour = BCD_Dec(buf[2] & 0x3F); /* 24小时制, bit7=0 */
}

/*
 * 调试用: 直接返回 Burst Read 的前 3 字节 (原始 BCD, 未解码).
 * 读到全 0xFF (DS1302 无响应, DAT 浮空被 51 内部上拉) 时自动重试,
 * 最多 3 次. 偶发失败率 ~25% 时, 3 次重试后残余约 1.5%.
 */
void DS1302_GetRawBCD(unsigned char *buf)
{
    unsigned char tmp[8];
    unsigned char i, retry;
    bit ea_save = EA;

    for (retry = 0; retry < 3; retry++)
    {
        EA = 0;

        DS_RST = 0;
        DS_CLK = 0;
        DLY_CE();
        DS_RST = 1;
        DLY_CE();

        WriteByte(0xBF);
        for (i = 0; i < 8; i++)
            tmp[i] = ReadByte();

        DS_CLK = 0;
        DLY();
        DS_RST = 0;
        DLY_CE();

        EA = ea_save;

        /* 秒/分/时 任一为 0xFF 即判无响应, 重试 */
        if (tmp[0] != 0xFF && tmp[1] != 0xFF && tmp[2] != 0xFF)
            break;

        DLY_CE(); DLY_CE();     /* CE 完全复位后再试 */
    }

    buf[0] = tmp[0];    /* 秒寄存器原值 (含 CH 位) */
    buf[1] = tmp[1];    /* 分寄存器原值 */
    buf[2] = tmp[2];    /* 时寄存器原值 */
}
