
void Delay(unsigned int xms)
{
	unsigned char i, j;
	while(xms--)
	{
		i = 4;
		j = 239;
		do
		{
			while (--j);
		} while (--i);
	}
}

