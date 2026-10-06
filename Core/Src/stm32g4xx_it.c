/**
 * @file    stm32g4xx_it.c
 * @brief   Kesme servis rutinleri.
 */
#include "main.h"
#include "stm32g4xx_it.h"

/* Ölümcül hatalarda motorları doğrudan register ile nötre çek */
static void fault_stop_motors(void)
{
    TIM2->CCR1 = 1500u;
    TIM2->CCR2 = 1500u;
}

void NMI_Handler(void)        { fault_stop_motors(); while (1) { } }
void HardFault_Handler(void)  { fault_stop_motors(); while (1) { } }
void MemManage_Handler(void)  { fault_stop_motors(); while (1) { } }
void BusFault_Handler(void)   { fault_stop_motors(); while (1) { } }
void UsageFault_Handler(void) { fault_stop_motors(); while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void LPUART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&hlpuart1);
}
