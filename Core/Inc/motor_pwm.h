/**
 * @file    motor_pwm.h
 * @brief   TIM2 CH1 (PA0, sol) / CH2 (PA1, sağ) 50 Hz ESC çıkışları.
 */
#ifndef MOTOR_PWM_H
#define MOTOR_PWM_H

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "catamaran_mixer.h"

void motor_pwm_init(TIM_HandleTypeDef *htim);

/* Mikser çıktısı (1500 = nötr). Her motorun ölçülmüş nötrüne kaydırılır. */
void motor_pwm_set(const motor_cmd_t *cmd);

/* Kaydırma uygulamadan doğrudan µs yazar (nötr testi için). */
void motor_pwm_set_raw(const motor_cmd_t *cmd);

/* Her iki motoru kendi nötr noktasına çeker. */
void motor_pwm_neutral(void);

/* Motor nötr noktaları (µs) - açılışta settings'ten, PC'den "N" komutuyla güncellenir. */
void motor_pwm_set_neutrals(uint16_t left_us, uint16_t right_us);
void motor_pwm_get_neutrals(uint16_t *left_us, uint16_t *right_us);

/* ESC'lere giden gerçek darbe genişlikleri (µs). */
motor_cmd_t motor_pwm_get_output(void);

#endif /* MOTOR_PWM_H */
