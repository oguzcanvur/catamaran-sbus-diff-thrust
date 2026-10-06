/**
 * @file    settings.h
 * @brief   Kalıcı ayarlar: flash'ın son 2 KB sayfasında (0x0801F800) saklanır.
 *          Geçerli kayıt yoksa motor_trim.h varsayılanları kullanılır.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t left_neutral_us;
    uint16_t right_neutral_us;
} settings_t;

/* Flash'tan okur; kayıt yok/bozuksa varsayılanları döner. true = flash'tan okundu */
bool settings_load(settings_t *s);

/* Flash sayfasını silip yazar. true = başarılı ve doğrulandı */
bool settings_save(const settings_t *s);

/* Değerler kabul edilebilir aralıkta mı? */
bool settings_valid(const settings_t *s);

#endif /* SETTINGS_H */
