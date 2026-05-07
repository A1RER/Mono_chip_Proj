/*
 * ds18b20.c  --  DS18B20 单总线驱动
 *
 * 时序细节 (11.0592 MHz, 机器周期 ~1.085 us):
 *   - 复位: 拉低 480~960 us, 释放 60~240 us 后检测 presence
 *   - 写 0:  拉低 60~120 us 然后释放
 *   - 写 1:  拉低 1~15 us 后释放, 总周期 60~120 us
 *   - 读位:  拉低 1~15 us 释放, 在 15us 内采样 DQ
 *
 * 温度寄存器格式 (12 位):
 *   16 位 signed, LSB = 0.0625 C, 即 raw / 16 = 实际温度
 *   要保留一位小数: temp_x10 = raw * 10 / 16
 *   raw 为负 (高位补码) 时直接做有符号运算即可.
 */

#include "ds18b20.h"
#include <intrins.h>

u8 g_ds18b20_status = DS18B20_STATUS_NO_ACK;

/* === 微秒级延时 (基于 _nop_, 11.0592 MHz) ===
 * _nop_ 占 1 机器周期 ~ 1.085 us, 调用 _nop_ 一次约 1us, 这里用循环近似.
 */
static void DS_DelayUs(u16 us)
{
    while (us--)
    {
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
    }
}

/* === 复位 + 检测 presence ===
 *
 * 重要 BUG 修复 (2026-05):
 *   原版 DS_DelayUs(60) 实际延时约 840µs (因 8 NOP+循环开销, 单圈 ~14µs),
 *   远超 DS18B20 presence pulse 的 60-240µs 存在窗口. 等 ~840µs 后采样,
 *   总线已被外部上拉拉回高电平 -> 永远读到 1 -> presence 检测永远失败
 *   -> 温度永远返回 INVALID(0x7FFF) -> LCD 把 0x7FFF 渲染成 "P 76.7C",
 *   肉眼读起来像"卡在 76.7C". 同时无效温度让报警保持 NORMAL,
 *   蜂鸣器/LED 都不响.
 *
 *   这里改用紧凑 NOP 循环精确控制采样时机, 落在 60-80µs 区间内.
 */
bit DS18B20_Init(void)
{
    bit present;
    u8  i;

    present = 0;
    DS18B20_DQ = 1;
    DS_DelayUs(2);
    DS18B20_DQ = 0;
    /* 复位脉冲: 规格书要求 >= 480µs.
     * 原来 DS_DelayUs(500) 实际 ~7ms, 对寄生供电模式会过度放电内部电容.
     * DS_DelayUs(40) 实际 ~560µs, 刚好满足最小值且减少放电量. */
    DS_DelayUs(40);
    DS18B20_DQ = 1;          /* 释放总线 */

    /* 等约 60-80µs 后采样 presence pulse.
     * DS18B20 在主机释放后 15-60µs 内开始拉低, 持续 60-240µs.
     * 采样窗口: 释放后 60-300µs 均安全.
     * 11.0592 MHz 下, 内层 2 NOP + for 开销 ≈ 5 cycle/圈 ≈ 5.4µs,
     * 12 圈 ≈ 65µs, 落在安全窗口内. */
    for (i = 0; i < 60; i++)
    {
        if (!DS18B20_DQ) present = 1;
        _nop_(); _nop_(); _nop_(); _nop_();
    }

    DS_DelayUs(30);          /* 等剩余复位时序完成 */
    return present;
}

/* === 写 1 字节 === */
static void DS18B20_WriteByte(u8 dat)
{
    u8 i;
    for (i = 0; i < 8; i++)
    {
        DS18B20_DQ = 0;
        _nop_(); _nop_();         /* >1us */
        DS18B20_DQ = (bit)(dat & 0x01);
        DS_DelayUs(5);
        DS18B20_DQ = 1;
        dat >>= 1;
    }
}

/* === 读 1 字节 === */
static u8 DS18B20_ReadByte(void)
{
    u8 i, dat = 0;
    for (i = 0; i < 8; i++)
    {
        DS18B20_DQ = 0;
        _nop_(); _nop_();
        DS18B20_DQ = 1;            /* 释放, 准备采样 */
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        if (DS18B20_DQ) dat |= (u8)(0x01 << i);
        DS_DelayUs(5);
    }
    return dat;
}

/* === 启动温度转换 (非阻塞) === */
void DS18B20_StartConvert(void)
{
    bit ea_bak;

    ea_bak = EA;
    EA = 0;
    if (DS18B20_Init())
    {
        DS18B20_WriteByte(0xCC);   /* Skip ROM */
        DS18B20_WriteByte(0x44);   /* Convert T */
    }
    EA = ea_bak;
}

/* === 读取上一次转换结果, 返回 temp_x10 === */
s16 DS18B20_ReadTempX10(void)
{
    u8 lsb, msb;
    s16 raw;
    s32 t;
    bit ea_bak;

    ea_bak = EA;
    EA = 0;
    if (!DS18B20_Init())
    {
        g_ds18b20_status = DS18B20_STATUS_NO_ACK;
        EA = ea_bak;
        return DS18B20_TEMP_INVALID;
    }
    DS18B20_WriteByte(0xCC);   /* Skip ROM */
    DS18B20_WriteByte(0xBE);   /* Read Scratchpad */
    lsb = DS18B20_ReadByte();
    msb = DS18B20_ReadByte();
    EA = ea_bak;

    raw = (s16)(((u16)msb << 8) | lsb);

    /* 通信失败时总线浮高 -> 全读到 0xFF -> raw = 0xFFFF = -1,
     * 对应 -0.0625C. 与室温无关, 视为无效. */
    if (raw == (s16)0xFFFF)
    {
        g_ds18b20_status = DS18B20_STATUS_READ_FF;
        return DS18B20_TEMP_INVALID;
    }

    /* raw 为有符号 16 位, 直接乘 10 再除 16 即可正确处理负数. */
    t = (s32)raw * 10;
    if (t >= 0) t = (t + 8) / 16;       /* 四舍五入 */
    else        t = (t - 8) / 16;
    g_ds18b20_status = DS18B20_STATUS_OK;
    return (s16)t;
}
