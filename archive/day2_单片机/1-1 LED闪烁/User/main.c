#include "stm32f10x.h"                  // Device header
#include "Delay.h"

void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 开启GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    // 2. 配置PC13为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

int main(void)
{
    // 初始化GPIO
    GPIO_Config();

    while (1)
    {
        // 图片中的代码逻辑：复位(点亮LED)
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        Delay_ms(1000); // 延时1秒

        // 置位(熄灭LED)
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        Delay_ms(1000); // 延时1秒
    }
}
