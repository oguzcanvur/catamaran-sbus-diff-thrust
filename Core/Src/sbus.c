/**
 * @file    sbus.c
 * @brief   S.BUS çözücü.
 *
 * Paket yapısı (25 bayt, 100000 baud, 8E2, terslenmiş):
 *   [0]      Header 0x0F
 *   [1..22]  16 kanal x 11 bit (LSB önce)
 *   [23]     Bayraklar: bit0 CH17, bit1 CH18, bit2 frame lost, bit3 failsafe
 *   [24]     Footer 0x00 (S.BUS2: 0x04/0x14/0x24/0x34)
 *
 * Alım: HAL_UARTEx_ReceiveToIdle_DMA. Paketler arasında ≥3 ms boşluk olduğu için
 * her IDLE olayı paketi doğal olarak hizalar; 25 bayttan farklı uzunluk atılır.
 */
#include "sbus.h"
#include "boat_config.h"
#include <string.h>

static UART_HandleTypeDef *s_huart;
static uint8_t             s_rx_buf[SBUS_FRAME_LEN];

static volatile sbus_data_t s_data;
static volatile bool        s_new_frame;
static volatile bool        s_has_frame;

static void sbus_start_rx(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_buf, SBUS_FRAME_LEN) == HAL_OK) {
        /* Yarım-transfer kesmesi gereksiz */
        __HAL_DMA_DISABLE_IT(s_huart->hdmarx, DMA_IT_HT);
    }
}

static uint16_t sbus_raw_to_us(uint16_t raw)
{
    int32_t v = (int32_t)raw;
    if (v < SBUS_RAW_MIN) v = SBUS_RAW_MIN;
    if (v > SBUS_RAW_MAX) v = SBUS_RAW_MAX;

    int32_t us;
    if (v >= SBUS_RAW_CENTER) {
        us = PWM_NEUTRAL_US + ((v - SBUS_RAW_CENTER) * 500 + (SBUS_RAW_MAX - SBUS_RAW_CENTER) / 2)
                              / (SBUS_RAW_MAX - SBUS_RAW_CENTER);
    } else {
        us = PWM_NEUTRAL_US - ((SBUS_RAW_CENTER - v) * 500 + (SBUS_RAW_CENTER - SBUS_RAW_MIN) / 2)
                              / (SBUS_RAW_CENTER - SBUS_RAW_MIN);
    }
    return (uint16_t)us;
}

static bool sbus_decode(const uint8_t *f)
{
    if (f[0] != 0x0F) return false;
    uint8_t footer = f[24];
    if (!(footer == 0x00 || (footer & 0x0F) == 0x04)) return false;

    /* 22 bayttan 16 x 11-bit kanal açma */
    uint32_t bitbuf = 0;
    uint8_t  bits   = 0;
    uint8_t  idx    = 1;
    for (uint8_t ch = 0; ch < SBUS_NUM_CHANNELS; ch++) {
        while (bits < 11u) {
            bitbuf |= (uint32_t)f[idx++] << bits;
            bits   += 8u;
        }
        uint16_t raw = (uint16_t)(bitbuf & 0x07FFu);
        bitbuf >>= 11;
        bits    -= 11u;

        s_data.raw[ch] = raw;
        s_data.us[ch]  = sbus_raw_to_us(raw);
    }

    s_data.frame_lost = (f[23] & 0x04u) != 0u;
    s_data.failsafe   = (f[23] & 0x08u) != 0u;
    return true;
}

void sbus_init(UART_HandleTypeDef *huart)
{
    s_huart     = huart;
    s_new_frame = false;
    s_has_frame = false;
    memset((void *)&s_data, 0, sizeof(s_data));
    for (uint8_t i = 0; i < SBUS_NUM_CHANNELS; i++) s_data.us[i] = PWM_NEUTRAL_US;
    sbus_start_rx();
}

bool sbus_get_frame(sbus_data_t *out)
{
    if (!s_new_frame) return false;
    __disable_irq();
    memcpy(out, (const void *)&s_data, sizeof(*out));
    s_new_frame = false;
    __enable_irq();
    return true;
}

bool sbus_link_ok(uint32_t now_ms)
{
    if (!s_has_frame) return false;
    if (s_data.failsafe) return false;
    return (now_ms - s_data.last_rx_tick) < SBUS_TIMEOUT_MS;
}

/* ---------------- HAL geri çağırmaları ---------------- */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != s_huart) return;

    if (Size == SBUS_FRAME_LEN && sbus_decode(s_rx_buf)) {
        s_data.last_rx_tick = HAL_GetTick();
        s_data.frame_count++;
        s_has_frame = true;
        s_new_frame = true;
    } else {
        s_data.error_count++;
    }
    sbus_start_rx();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_huart) return;
    /* Parity/Frame/Noise/Overrun: alımı sıfırla ve yeniden başlat */
    s_data.error_count++;
    HAL_UART_AbortReceive(huart);
    sbus_start_rx();
}
