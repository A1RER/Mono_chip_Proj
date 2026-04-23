#include <REGX52.H>
#include <INTRINS.H>

sbit Buzzer = P1^5;  // 无源蜂鸣器，P1.5引脚

// LED接P2，低电平亮（共阳极接法）

void Delay500ms()		//@12.000MHz
{
	unsigned char i, j, k;

	_nop_();
	i = 4;
	j = 205;
	k = 187;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

// 延时500us，用于产生约1kHz方波驱动无源蜂鸣器
void Delay500us()		//@12.000MHz
{
	unsigned char i;

	_nop_();
	i = 247;
	while (--i);
}

void main()
{
	unsigned char i;

	// 8盏LED同时亮灭10次
	for (i = 0; i < 10; i++)
	{
		P2 = 0x00;       // 全部点亮
		Delay500ms();
		P2 = 0xFF;       // 全部熄灭
		Delay500ms();
	}

	// 蜂鸣器长鸣报警（无限循环，持续输出1kHz方波）
	while (1)
	{
		Buzzer = !Buzzer;
		Delay500us();
	}
}
