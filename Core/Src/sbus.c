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
#include "rc_calibration.h"
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

typedef struct {
    int32_t min, center, max;
} sbus_cal_t;

/* CH1/CH2 ölçülmüş kalibrasyonla, diğer kanallar standart S.BUS aralığıyla çevrilir */
static const sbus_cal_t s_cal_default = { SBUS_RAW_MIN, SBUS_RAW_CENTER, SBUS_RAW_MAX };
static const sbus_cal_t s_cal_ch1     = { CAL_CH1_MIN,  CAL_CH1_CENTER,  CAL_CH1_MAX  };
static const sbus_cal_t s_cal_ch2     = { CAL_CH2_MIN,  CAL_CH2_CENTER,  CAL_CH2_MAX  };

static const sbus_cal_t *sbus_cal_for(uint8_t ch)
{
    if (ch == 0u) return &s_cal_ch1;
    if (ch == 1u) return &s_cal_ch2;
    return &s_cal_default;
}

/* Parçalı doğrusal: min -> 1000, center -> 1500, max -> 2000 µs (yuvarlamalı) */
static uint16_t sbus_raw_to_us(uint16_t raw, const sbus_cal_t *c)
{
    int32_t v = (int32_t)raw;
    if (v < c->min) v = c->min;
    if (v > c->max) v = c->max;

    int32_t us;
    if (v >= c->center) {
        int32_t span = c->max - c->center;
        us = (span > 0) ? PWM_NEUTRAL_US + ((v - c->center) * 500 + span / 2) / span
                        : PWM_NEUTRAL_US;
    } else {
        int32_t span = c->center - c->min;
        us = (span > 0) ? PWM_NEUTRAL_US - ((c->center - v) * 500 + span / 2) / span
                        : PWM_NEUTRAL_US;
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
        s_data.us[ch]  = sbus_raw_to_us(raw, sbus_cal_for(ch));
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

/* ---------------- HAL geri çağırmalarından (main.c) ---------------- */

void sbus_on_rx_event(uint16_t Size)
{
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

void sbus_on_error(void)
{
    /* Parity/Frame/Noise/Overrun: alımı sıfırla ve yeniden başlat */
    s_data.error_count++;
    HAL_UART_AbortReceive(s_huart);
    sbus_start_rx();
}
