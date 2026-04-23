#include <REGX52.H>
#include <INTRINS.H>

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

void main()
{
	unsigned char i;
	unsigned char LED;

	while(1)
	{
		// 正向流水：D1->D2->...->D8
		LED = 0xFE;
		for(i = 0; i < 8; i++)
		{
			P2 = LED;
			Delay500ms();
			LED = _crol_(LED, 1);
		}

		// 反向流水：D8->D7->...->D1
		LED = 0x7F;
		for(i = 0; i < 8; i++)
		{
			P2 = LED;
			Delay500ms();
			LED = _cror_(LED, 1);
		}
	}
}
