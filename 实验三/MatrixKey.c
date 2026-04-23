/*
 * MatrixKey.c - 4x4 矩阵按键驱动 (逐行扫描法)
 * 行 P1[3:0] 依次拉低; 列 P1[7:4] 读取是否被拉低
 * 返回: 1~16, 无键返回 0
 *
 * 键盘物理布局:
 *        P1.4  P1.5  P1.6  P1.7
 * P1.0:   1     2     3     4
 * P1.1:   5     6     7     8
 * P1.2:   9    10    11    12
 * P1.3:  13    14    15    16
 */
#include <REG52.H>

/* 内部消抖延时 (~10ms) */
static void Key_Delay10ms(void)
{
    unsigned char i, j;
    i = 2; j = 239;
    do
    {
        while (--j);
    } while (--i);
    i = 2; j = 239;
    do
    {
        while (--j);
    } while (--i);
    i = 2; j = 239;
    do
    {
        while (--j);
    } while (--i);
    i = 2; j = 239;
    do
    {
        while (--j);
    } while (--i);
    i = 2; j = 239;
    do
    {
        while (--j);
    } while (--i);
}

/* 行扫描模式: 行0低=0xFE, 行1低=0xFD, 行2低=0xFB, 行3低=0xF7 */
static unsigned char row_patterns[4] = {0xFE, 0xFD, 0xFB, 0xF7};

unsigned char MatrixKey_GetKey(void)
{
    unsigned char row, col_val;
    unsigned char col = 0;

    for (row = 0; row < 4; row++)
    {
        P1 = row_patterns[row];  /* 拉低对应行, 高四位为列输入 */

        col_val = P1 & 0xF0;
        if (col_val != 0xF0)     /* 有列被拉低 = 有键按下 */
        {
            Key_Delay10ms();     /* 消抖 */
            col_val = P1 & 0xF0;
            if (col_val != 0xF0)
            {
                /* 找被拉低的列 */
                if      (!(col_val & 0x10)) col = 0;  /* P1.4 */
                else if (!(col_val & 0x20)) col = 1;  /* P1.5 */
                else if (!(col_val & 0x40)) col = 2;  /* P1.6 */
                else if (!(col_val & 0x80)) col = 3;  /* P1.7 */
                else continue;

                /* 等待按键释放 */
                while ((P1 & 0xF0) != 0xF0);
                Key_Delay10ms();

                return (unsigned char)(row * 4 + col + 1);  /* 1~16 */
            }
        }
    }
    return 0;
}
