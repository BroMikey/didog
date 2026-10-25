#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Timer.h"

uint16_t Num = 0;

int main(void)
{

	Timer_Init();

	
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{

		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
