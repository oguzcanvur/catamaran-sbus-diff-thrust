/**
 * @file    sbus.h
 * @brief   S.BUS alıcı sürücüsü (USART1 + DMA + Idle Line, donanımsal RX tersleme).
 */
#ifndef SBUS_H
#define SBUS_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"

#define SBUS_FRAME_LEN      25u
#define SBUS_NUM_CHANNELS   16u

typedef struct {
    uint16_t raw[SBUS_NUM_CHANNELS];   /* 11-bit ham değerler           */
    uint16_t us[SBUS_NUM_CHANNELS];    /* 1000..2000 µs karşılıkları    */
    bool     frame_lost;
    bool     failsafe;
    uint32_t last_rx_tick;             /* Son geçerli paketin HAL_GetTick */
    uint32_t frame_count;
    uint32_t error_count;
} sbus_data_t;

void sbus_init(UART_HandleTypeDef *huart);

/* Yeni paket geldiyse true döner ve *out'a kopyalar (kesme-güvenli). */
bool sbus_get_frame(sbus_data_t *out);

/* Bağlantı sağlıklı mı? (timeout + failsafe bayrağı) */
bool sbus_link_ok(uint32_t now_ms);

/* HAL UART geri çağırmalarından yönlendirilir (main.c) */
void sbus_on_rx_event(uint16_t size);
void sbus_on_error(void);

#endif /* SBUS_H */
