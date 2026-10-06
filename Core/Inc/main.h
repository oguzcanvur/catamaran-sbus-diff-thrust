/**
 * @file    main.h
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* NUCLEO-G431RB kullanıcı LED'i (LD2) */
#define LD2_Pin         GPIO_PIN_5
#define LD2_GPIO_Port   GPIOA

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern TIM_HandleTypeDef  htim2;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
