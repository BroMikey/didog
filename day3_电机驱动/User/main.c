#include "Delay.h"
#include "Servo.h"
#include "stm32f10x.h" // Device header


float Angle = 90;

int main(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    Servo_Init();
    while (1)
    {
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        Servo_SetAngle(Angle);
        Delay_ms(5000);
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        Servo_SetAngle(Angle + 30);
        Delay_ms(5000);

        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);
        Servo_SetAngle(Angle + 60);
        Delay_ms(5000);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
        Servo_SetAngle(Angle + 90);
        Delay_ms(5000);

        GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)0);
        Servo_SetAngle(Angle);
        Delay_ms(5000);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)1);
        Servo_SetAngle(Angle + 30);
        Delay_ms(5000);
    }
}
