#include "myUsart1.h"
#include "sys.h"

u8 RxData=0x00;

void USART1_Init(uint32_t bound)
{
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);//??GPIOA??
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	USART_InitTypeDef UASRT_InitStructure;
	UASRT_InitStructure.USART_BaudRate=9600;
	UASRT_InitStructure.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
	UASRT_InitStructure.USART_Mode=USART_Mode_Rx|USART_Mode_Tx;
	UASRT_InitStructure.USART_Parity=USART_Parity_No;
	UASRT_InitStructure.USART_StopBits=USART_StopBits_1;
	UASRT_InitStructure.USART_WordLength=USART_WordLength_8b;
	USART_Init(USART1,&UASRT_InitStructure);
	
	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel=USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=1;
	NVIC_Init(&NVIC_InitStructure);
	USART_Cmd(USART1,ENABLE);	
}

void USART1_IRQHandler(void)
{
	if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)
	{
			USART_ClearFlag(USART1, USART_FLAG_RXNE);
			RxData = USART_ReceiveData(USART1);  //接收到的数据存到 RxData
	}
}
