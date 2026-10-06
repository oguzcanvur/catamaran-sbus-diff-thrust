/**
 * @file    motor_pwm.c
 * @brief   ESC PWM çıkışı. Zamanlayıcı 1 µs çözünürlükte olduğu için CCR = darbe genişliği (µs).
 */
#include "motor_pwm.h"
#include "boat_config.h"

static TIM_HandleTypeDef *s_htim;

static uint16_t safe_us(uint16_t us)
{
    if (us < PWM_MIN_US) return PWM_MIN_US;
    if (us > PWM_MAX_US) return PWM_MAX_US;
    return us;
}

void motor_pwm_init(TIM_HandleTypeDef *htim)
{
    s_htim = htim;
    motor_pwm_neutral();
    HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_2);
}

void motor_pwm_set(const motor_cmd_t *cmd)
{
    /* Preload aktif: yeni değer bir sonraki periyot başında uygulanır (glitch yok) */
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, safe_us(cmd->left_us));
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_2, safe_us(cmd->right_us));
}

void motor_pwm_neutral(void)
{
    motor_cmd_t n = catamaran_mixer_neutral();
    motor_pwm_set(&n);
}
