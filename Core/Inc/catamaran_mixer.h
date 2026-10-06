/**
 * @file    catamaran_mixer.h
 * @brief   Arcade (tek kol) diferansiyel itiş mikseri.
 */
#ifndef CATAMARAN_MIXER_H
#define CATAMARAN_MIXER_H

#include <stdint.h>

typedef struct {
    uint16_t left_us;
    uint16_t right_us;
} motor_cmd_t;

/* [DEADBAND_LOW_US, DEADBAND_HIGH_US] aralığını 1500'e sabitler, 1000..2000'e kırpar */
uint16_t mixer_apply_deadband(uint16_t us);

/* throttle_us: Kanal 2, steering_us: Kanal 1 (her ikisi 1000..2000 µs) */
motor_cmd_t catamaran_mixer(uint16_t throttle_us, uint16_t steering_us);

/* Güvenli durum: her iki motor nötr */
motor_cmd_t catamaran_mixer_neutral(void);

#endif /* CATAMARAN_MIXER_H */
