#include "Delay.h"
#include "stm32f10x.h" // Device header

/* 舵机编号与引脚对应关系 */
#define SERVO_FL   0		// PA0  左前腿
#define SERVO_FR   1		// PA1  右前腿
#define SERVO_BL   2		// PA2  左后腿
#define SERVO_BR   3		// PA3  右后腿
#define SERVO_TAIL 4		// PA6  尾巴

#define LEG_SWING  30		// 腿部摆动幅度，可按实际机械结构调整

/**
  * @brief  PWM 初始化：PA0/PA1/PA2/PA3 输出 TIM2_CH1~CH4，PA6 输出 TIM3_CH1
  * @note   定时器时钟 72MHz，PSC = 72-1，ARR = 20000-1，PWM 周期 20ms（50Hz）
  */
static void PWM_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_InternalClockConfig(TIM2);
    TIM_InternalClockConfig(TIM3);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;		//ARR
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;		//PSC
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;		//CCR
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC3Init(TIM2, &TIM_OCInitStructure);
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

/**
  * @brief  设置比较值，转换成 0.5ms~2.5ms 的高电平时间
  */
static uint16_t Servo_AngleToCompare(float Angle)
{
    return (uint16_t)(Angle / 180 * 2000 + 500);
}

/**
  * @brief  设置指定舵机角度
  * @param  id    舵机编号（SERVO_FL ~ SERVO_TAIL）
  * @param  Angle 角度 0~180
  */
static void Servo_SetAngle(uint8_t id, float Angle)
{
    uint16_t Compare = Servo_AngleToCompare(Angle);

    switch (id)
    {
        case SERVO_FL:		TIM_SetCompare1(TIM2, Compare);	break;
        case SERVO_FR:		TIM_SetCompare2(TIM2, Compare);	break;
        case SERVO_BL:		TIM_SetCompare3(TIM2, Compare);	break;
        case SERVO_BR:		TIM_SetCompare4(TIM2, Compare);	break;
        case SERVO_TAIL:	TIM_SetCompare1(TIM3, Compare);	break;
        default: break;
    }
}

/**
  * @brief  所有舵机同时设置到同一角度（上电回中位用）
  */
static void Servo_SetAllAngle(float Angle)
{
    uint16_t Compare = Servo_AngleToCompare(Angle);

    TIM_SetCompare1(TIM2, Compare);
    TIM_SetCompare2(TIM2, Compare);
    TIM_SetCompare3(TIM2, Compare);
    TIM_SetCompare4(TIM2, Compare);
    TIM_SetCompare1(TIM3, Compare);
}

int main(void)
{
    PWM_Init();

    /* 上电先回中位，避免舵机抖动 */
    Servo_SetAllAngle(90);
    Delay_ms(500);

    while (1)
    {
        /* 前进：对角腿交替摆动，尾巴左右摇摆 */
        Servo_SetAngle(SERVO_FL, 90 + LEG_SWING);
        Servo_SetAngle(SERVO_BR, 90 + LEG_SWING);
        Servo_SetAngle(SERVO_FR, 90 - LEG_SWING);
        Servo_SetAngle(SERVO_BL, 90 - LEG_SWING);
        Servo_SetAngle(SERVO_TAIL, 90 - LEG_SWING);
        Delay_ms(500);

        Servo_SetAngle(SERVO_FL, 90 - LEG_SWING);
        Servo_SetAngle(SERVO_BR, 90 - LEG_SWING);
        Servo_SetAngle(SERVO_FR, 90 + LEG_SWING);
        Servo_SetAngle(SERVO_BL, 90 + LEG_SWING);
        Servo_SetAngle(SERVO_TAIL, 90 + LEG_SWING);
        Delay_ms(500);
    }
}
