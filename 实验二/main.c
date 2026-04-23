#include <REGX52.H>
#include "Delay.h"
#include "LCD1602.h"
#include "MartixKey.h"

unsigned char KeyNum;

void main()
{
	LCD_Init();
	LCD_ShowString(1,1,"MartixKey:");
	while(1)
	{
		KeyNum=MartixKey();
		if(KeyNum!=0)
		{
			LCD_ShowNum(2,1,KeyNum,2);
		}
	}
}