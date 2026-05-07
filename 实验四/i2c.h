/*
 * i2c.h  --  软件模拟 I2C (用于 AT24C08 EEPROM)
 *
 * 引脚: SCL=P2^1, SDA=P2^0  (均需外部上拉 4.7k 至 VCC)
 * 时序: 标准模式 100kHz, 实际配合 11.0592 MHz 主频, 用 _nop_ 占空保证 SCL 周期
 */

#ifndef __I2C_H__
#define __I2C_H__

#include "config.h"

void I2C_Start(void);
void I2C_Stop (void);
void I2C_SendByte (u8 dat);
u8   I2C_RecvByte (void);
void I2C_SendAck  (bit ack);     /* 0=ACK, 1=NACK */
bit  I2C_WaitAck  (void);        /* 0=ACK 收到 */

#endif
