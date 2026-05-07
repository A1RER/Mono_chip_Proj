/*
 * i2c.c  --  软件 I2C
 *
 * 注意: 11.0592MHz 下 _nop_() ~ 1us, 普通 I2C 100kHz (10us 周期) 完全够用.
 *       AT24C08 也支持 400kHz, 但保守使用 100kHz 以保证仿真稳定.
 */

#include "i2c.h"
#include <intrins.h>

#define I2C_DELAY()  do { _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); } while(0)

void I2C_Start(void)
{
    I2C_SDA = 1;
    I2C_SCL = 1;
    I2C_DELAY();
    I2C_SDA = 0;        /* SCL 高时 SDA 由高变低 = START */
    I2C_DELAY();
    I2C_SCL = 0;
}

void I2C_Stop(void)
{
    I2C_SDA = 0;
    I2C_SCL = 1;
    I2C_DELAY();
    I2C_SDA = 1;        /* SCL 高时 SDA 由低变高 = STOP */
    I2C_DELAY();
}

/* 发送 1 字节, 高位先发 */
void I2C_SendByte(u8 dat)
{
    u8 i;
    for (i = 0; i < 8; i++)
    {
        I2C_SDA = (bit)((dat & 0x80) ? 1 : 0);
        dat <<= 1;
        I2C_SCL = 1;
        I2C_DELAY();
        I2C_SCL = 0;
        I2C_DELAY();
    }
}

/* 等待从机 ACK, 返回 0=收到 ACK, 1=NACK */
bit I2C_WaitAck(void)
{
    bit ack;
    I2C_SDA = 1;        /* 释放 SDA, 让从机拉低 */
    I2C_DELAY();
    I2C_SCL = 1;
    I2C_DELAY();
    ack = I2C_SDA;
    I2C_SCL = 0;
    I2C_DELAY();
    return ack;
}

/* 主机发 ACK/NACK 给从机 */
void I2C_SendAck(bit nack)
{
    I2C_SDA = nack;
    I2C_SCL = 1;
    I2C_DELAY();
    I2C_SCL = 0;
    I2C_SDA = 1;
    I2C_DELAY();
}

/* 接收 1 字节 */
u8 I2C_RecvByte(void)
{
    u8 i, dat = 0;
    I2C_SDA = 1;        /* 释放 SDA */
    for (i = 0; i < 8; i++)
    {
        I2C_SCL = 1;
        I2C_DELAY();
        dat <<= 1;
        if (I2C_SDA) dat |= 0x01;
        I2C_SCL = 0;
        I2C_DELAY();
    }
    return dat;
}
