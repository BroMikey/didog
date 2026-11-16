#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LED.h"
#include "myUsart1.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	USART3_Init(115200);  //
	LED_Init(); //

	while(1)
	{
		if(RxData == 0x00) LED_Close();   //
		if(RxData == 0x01) LED_Open();    //
	}
}
