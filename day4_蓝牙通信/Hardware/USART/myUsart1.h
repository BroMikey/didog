#ifndef __myUsart1_H
#define __myUsart1_H
#include "sys.h"

/* Last byte received from the Bluetooth module (USART3). */
extern volatile u8 RxData;
/* Set by the ISR when a byte arrives, cleared by the consumer. */
extern volatile u8 RxReady;

void USART3_Init(uint32_t bound);
void USART3_SendByte(u8 byte);
void USART3_SendString(char *str);

#endif
