#include "stm32f10x.h"
#include "LED.h"
#include "myUsart1.h"

/* Reply text goes back over the same Bluetooth link, so the phone sees the
 * result of every command. */
static void Reply(char *msg)
{
	USART3_SendString(msg);
}

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	USART3_Init(9600);   /* HC-05 factory baud rate */
	LED_Init();

	Reply("READY\r\n");

	while (1)
	{
		u8 cmd;

		if (!RxReady)
		{
			continue;
		}

		cmd = RxData;
		RxReady = 0;

		switch (cmd)
		{
		case '1':
			LED_Open();
			Reply("LED:ON\r\n");
			break;
		case '0':
			LED_Close();
			Reply("LED:OFF\r\n");
			break;
		default:
			/* Echo proves the link works both ways. */
			Reply("ECHO:");
			USART3_SendByte(cmd);
			Reply("\r\n");
			break;
		}
	}
}
