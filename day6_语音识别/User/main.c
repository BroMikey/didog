#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LED.h"
#include "myUsart1.h"

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	USART1_Init(9600);  //初始化串口1
	LED_Init(); //LED灯初始化

	while(1)
	{
		if(RxData == 0x00) LED_Close();   //关灯
		if(RxData == 0x01) LED_Open();    //开灯
	}
}
