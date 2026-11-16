#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "BlueTooth.h"
#include "Face_Config.h"

/* 表情轮换：0~6 对应 Face_Config 中的 7 种表情 */
#define FACE_COUNT 7
#define FACE_DELAY_MS 1000

int main(void)
{
	OLED_Init();//OLED初始化

	while (1)
	{
		for (Face_Mode = 0; Face_Mode < FACE_COUNT; Face_Mode++)
		{
			Face_Config();
			Delay_ms(FACE_DELAY_MS);
		}
	}
}
