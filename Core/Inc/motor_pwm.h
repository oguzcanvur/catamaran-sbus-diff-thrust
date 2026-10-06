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
void motor_pwm_set(const motor_cmd_t *cmd);
void motor_pwm_neutral(void);

#endif /* MOTOR_PWM_H */
