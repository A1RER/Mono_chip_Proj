/*
 * at24c08.h  --  AT24C08 EEPROM 驱动 (8K bit = 1024 Byte)
 *
 * AT24C08 寻址特殊性: 8K bit 容量 = 4 个 256 Byte 块,
 *   器件地址  : 1010 P1 P0 R/W, 其中 P1 P0 为页选位 (块号 0..3)
 *   字地址(字节地址低 8 位) : 通过 I2C SendByte 发送
 *   高 2 位地址通过 P1 P0 编码进 device address 的 byte
 *
 * 地址布局 (本工程自定义):
 *   0x000 ~ 0x00F : 参数区
 *      0x000      : 魔术字 0x5A (用来判断是否已写入过参数)
 *      0x001~0x002: 高温阈值 x10 (low, high), s16
 *      0x003~0x004: 低温阈值 x10 (low, high), s16
 *      0x005      : upload_interval
 *      0x006      : write_idx 低字节 (0..203)
 *      0x007      : reserved
 *   0x010 ~ 0x3FF : 温度记录区, 5 字节/条, 共 (1024-16)/5 = 201 条
 *      [hour, min, sec, temp_x10_low, temp_x10_high]
 *
 * 注: 实验要求"1 秒保存 1 次"是教学场景, 实际工程中 AT24C08 写入寿命
 *     约 1,000,000 次, 不应这样用.
 */

#ifndef __AT24C08_H__
#define __AT24C08_H__

#include "config.h"

/* 参数区 */
#define EE_ADDR_MAGIC       0x000
#define EE_MAGIC_VALUE      0x5A
#define EE_ADDR_HIGH        0x001
#define EE_ADDR_LOW         0x003
#define EE_ADDR_UPLOAD      0x005
#define EE_ADDR_WRITEIDX    0x006

/* 记录区 */
#define EE_REC_BASE         0x010
#define EE_REC_SIZE         5
#define EE_REC_MAX          201

void AT24C08_WriteByte(u16 addr, u8  dat);
u8   AT24C08_ReadByte (u16 addr);
void AT24C08_WritePage(u16 addr, u8 *buf, u8 len);   /* len <= 16, 不跨 16 字节边界 */
void AT24C08_ReadBuf  (u16 addr, u8 *buf, u8 len);

/* 高层封装 */
void AT24C08_SaveParams (s16 high_x10, s16 low_x10, u8 upload, u8 write_idx);
bit  AT24C08_LoadParams (s16 *high_x10, s16 *low_x10, u8 *upload, u8 *write_idx);
void AT24C08_SaveRecord (u8 *write_idx, u8 hour, u8 minute, u8 sec, s16 temp_x10);

#endif
