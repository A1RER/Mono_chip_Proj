/*
 * MatrixKey.h - 4x4 矩阵按键驱动 (HC6800-ES V2.0)
 * 引脚: 行 = P1^0~P1^3 (输出), 列 = P1^4~P1^7 (输入)
 * 键值: 1~16 (按行排列, 无键返回 0)
 */
#ifndef __MATRIXKEY_H__
#define __MATRIXKEY_H__

unsigned char MatrixKey_GetKey(void);

#endif
