#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"

int main(void)
{
  //X是横坐标，Y是纵坐标
	OLED_Init();//OLED初始化

    // OLED_ShowString(1, 3, "HelloWorld!", 6);
	OLED_ShowChar(0, 1, 'A',6);
	OLED_ShowString(0, 9, "HelloWorld!",6);
	OLED_ShowNum(0, 15, 12345, 5, 6);
	OLED_ShowSignedNum(0, 21, -66, 2, 6);
	OLED_ShowHexNum(0, 27, 0xAA55, 4, 6);
	OLED_ShowBinNum(0, 33, 0xAA55, 16, 6);

	OLED_Update();

}
