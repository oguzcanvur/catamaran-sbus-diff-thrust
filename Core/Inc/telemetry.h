/**
 * @file    telemetry.h
 * @brief   ST-LINK sanal COM portu (LPUART1, PA2 TX / PA3 RX) üzerinden
 *          canlı durum çıktısı ve PC'den komut alımı (115200 8N1).
 *
 * Komutlar (satır sonu \n):
 *   T <sol_us> <sag_us>   Motor nötr testi: ESC'lere doğrudan bu değerleri yaz.
 *                         TEST_TIMEOUT_MS içinde yenilenmezse test modu kapanır.
 *   X                     Test modundan hemen çık.
 *   N <sol_us> <sag_us>   Motor nötr noktalarını uygula ve flash'a kalıcı kaydet.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"
#include "sbus.h"
#include "catamaran_mixer.h"

void telemetry_init(UART_HandleTypeDef *huart);

/* Ana döngüden her turda çağrılır; TELEMETRY_PERIOD_MS'de bir satır gönderir (bloklamaz). */
void telemetry_task(uint32_t now_ms, const sbus_data_t *rc, const motor_cmd_t *out,
                    const char *state, uint16_t neutral_l, uint16_t neutral_r);

/* Test modu aktifse true döner ve istenen µs değerlerini *out'a yazar. */
bool telemetry_get_test(uint32_t now_ms, motor_cmd_t *out);

/* PC yeni nötr değerleri gönderdiyse true döner (bir kez). Flash yazımı ana döngüde yapılır. */
bool telemetry_take_neutral_request(uint16_t *left_us, uint16_t *right_us);

/* HAL UART geri çağırmalarından yönlendirilir (main.c) */
void telemetry_on_rx_event(uint16_t size);
void telemetry_on_error(void);

#endif /* TELEMETRY_H */
