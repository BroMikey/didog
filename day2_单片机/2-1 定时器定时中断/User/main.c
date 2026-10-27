#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Timer.h"

uint16_t Num = 0;
uint16_t SOS_Count = 0;
uint8_t SOS_Step = 0;

int main(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA的时钟

	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);						//将PA0引脚初始化为推挽输出

	//设置PA0初始电平为高电平（LED熄灭）
	GPIO_SetBits(GPIOA, GPIO_Pin_0);

	/*定时器初始化*/
	Timer_Init();

	while (1)
	{

	}
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		Num ++;

		/*SOS闪灯控制：... --- ...  三短三长三短，单位时间为200ms*/
		if (SOS_Step < 3)			//三个短闪（S）
		{
			if (Num % 2 == 1)		//奇数周期点亮，偶数周期熄灭，周期200ms
			{
				GPIO_ResetBits(GPIOA, GPIO_Pin_0);	//LED亮
			}
			else
			{
				GPIO_SetBits(GPIOA, GPIO_Pin_0);	//LED灭
				SOS_Count ++;						//完成一个短闪计数
			}
		}
		else if (SOS_Step < 6)		//三个长闪（O）
		{
			if (Num % 6 != 0)		//点亮3个周期（600ms）
			{
				GPIO_ResetBits(GPIOA, GPIO_Pin_0);	//LED亮
			}
			else
			{
				GPIO_SetBits(GPIOA, GPIO_Pin_0);	//LED灭
			}
		}
		else						//三个短闪（S）
		{
			if (Num % 2 == 1)		//奇数周期点亮，偶数周期熄灭，周期200ms
			{
				GPIO_ResetBits(GPIOA, GPIO_Pin_0);	//LED亮
			}
			else
			{
				GPIO_SetBits(GPIOA, GPIO_Pin_0);	//LED灭
			}
		}

		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
