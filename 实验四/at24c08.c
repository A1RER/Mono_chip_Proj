/*
 * at24c08.c  --  AT24C08 EEPROM
 *
 * 关键点:
 *   1. AT24C08 的 11 位字地址中, 高 2 位与器件地址 (1010xx) 中的 P1P0 复合.
 *      对于 12 位地址 addr (0..0x3FF):
 *          dev_addr = 0xA0 | ((addr >> 8) << 1) & 0x06
 *          word_addr = addr & 0xFF
 *   2. 页大小 16 字节, 跨页写需要切割.
 *   3. 写入后必须等待 ~5ms 才能再次访问 (写周期), 这里用 ACK polling 简化为 5ms 延时.
 */

#include "at24c08.h"
#include "i2c.h"
#include <intrins.h>

/* AT24C08 写周期 ~5ms, 用软件延时 (主程序 1Hz 节拍内, 此 5ms 延时可接受) */
static void EE_DelayMs(u16 ms)
{
    u8 i, j;
    while (ms--) { i = 2; j = 239; do { while (--j); } while (--i); }
}

#define EE_DEV_ADDR(addr)   (u8)(0xA0 | (((addr) >> 7) & 0x06))
#define EE_WORD_ADDR(addr)  (u8)((addr) & 0xFF)

void AT24C08_WriteByte(u16 addr, u8 dat)
{
    I2C_Start();
    I2C_SendByte(EE_DEV_ADDR(addr));     I2C_WaitAck();
    I2C_SendByte(EE_WORD_ADDR(addr));    I2C_WaitAck();
    I2C_SendByte(dat);                   I2C_WaitAck();
    I2C_Stop();
    EE_DelayMs(5);
}

u8 AT24C08_ReadByte(u16 addr)
{
    u8 dat;
    I2C_Start();
    I2C_SendByte(EE_DEV_ADDR(addr));     I2C_WaitAck();
    I2C_SendByte(EE_WORD_ADDR(addr));    I2C_WaitAck();
    I2C_Start();                         /* 重启 */
    I2C_SendByte(EE_DEV_ADDR(addr) | 1); I2C_WaitAck();
    dat = I2C_RecvByte();
    I2C_SendAck(1);                      /* NACK 表示读完 */
    I2C_Stop();
    return dat;
}

/* 页写: AT24C08 页大小 16 字节. 调用方需保证 (addr & 0x0F) + len <= 16. */
void AT24C08_WritePage(u16 addr, u8 *buf, u8 len)
{
    u8 i;
    if (len == 0) return;
    I2C_Start();
    I2C_SendByte(EE_DEV_ADDR(addr));     I2C_WaitAck();
    I2C_SendByte(EE_WORD_ADDR(addr));    I2C_WaitAck();
    for (i = 0; i < len; i++)
    {
        I2C_SendByte(buf[i]);            I2C_WaitAck();
    }
    I2C_Stop();
    EE_DelayMs(5);
}

void AT24C08_ReadBuf(u16 addr, u8 *buf, u8 len)
{
    u8 i;
    if (len == 0) return;
    I2C_Start();
    I2C_SendByte(EE_DEV_ADDR(addr));     I2C_WaitAck();
    I2C_SendByte(EE_WORD_ADDR(addr));    I2C_WaitAck();
    I2C_Start();
    I2C_SendByte(EE_DEV_ADDR(addr) | 1); I2C_WaitAck();
    for (i = 0; i < len - 1; i++)
    {
        buf[i] = I2C_RecvByte();
        I2C_SendAck(0);                  /* ACK, 继续读 */
    }
    buf[len - 1] = I2C_RecvByte();
    I2C_SendAck(1);                      /* 最后一个字节 NACK */
    I2C_Stop();
}

/* === 参数保存/读取 === */
void AT24C08_SaveParams(s16 high_x10, s16 low_x10, u8 upload, u8 write_idx)
{
    u8 buf[8];
    buf[0] = EE_MAGIC_VALUE;
    buf[1] = (u8)(high_x10 & 0xFF);
    buf[2] = (u8)((high_x10 >> 8) & 0xFF);
    buf[3] = (u8)(low_x10  & 0xFF);
    buf[4] = (u8)((low_x10  >> 8) & 0xFF);
    buf[5] = upload;
    buf[6] = write_idx;
    buf[7] = 0;
    /* 8 字节落在 0x000~0x007, 不跨 16 字节边界, 一次页写完成 */
    AT24C08_WritePage(0x000, buf, 8);
}

bit AT24C08_LoadParams(s16 *high_x10, s16 *low_x10, u8 *upload, u8 *write_idx)
{
    u8 buf[8];
    AT24C08_ReadBuf(0x000, buf, 8);
    if (buf[0] != EE_MAGIC_VALUE) return 0;          /* 第一次烧片, 没有有效数据 */
    *high_x10  = (s16)((u16)buf[1] | ((u16)buf[2] << 8));
    *low_x10   = (s16)((u16)buf[3] | ((u16)buf[4] << 8));
    *upload    = buf[5];
    *write_idx = buf[6];
    return 1;
}

/* === 温度记录: 5 字节/条, 循环存储 === */
void AT24C08_SaveRecord(u8 *write_idx, u8 hour, u8 minute, u8 sec, s16 temp_x10)
{
    u16 addr;
    u8  buf[5];

    if (*write_idx >= EE_REC_MAX) *write_idx = 0;

    addr = EE_REC_BASE + (u16)(*write_idx) * EE_REC_SIZE;
    buf[0] = hour;
    buf[1] = minute;
    buf[2] = sec;
    buf[3] = (u8)(temp_x10 & 0xFF);
    buf[4] = (u8)((temp_x10 >> 8) & 0xFF);

    /* 5 字节可能跨 16 字节页边界, 安全做法是逐字节写; 简化按页判断 */
    if (((addr & 0x0F) + EE_REC_SIZE) <= 16)
    {
        AT24C08_WritePage(addr, buf, EE_REC_SIZE);
    }
    else
    {
        u8 first = 16 - (u8)(addr & 0x0F);
        AT24C08_WritePage(addr,         buf,        first);
        AT24C08_WritePage(addr + first, buf+first, (u8)(EE_REC_SIZE - first));
    }

    (*write_idx)++;
    if (*write_idx >= EE_REC_MAX) *write_idx = 0;
}
