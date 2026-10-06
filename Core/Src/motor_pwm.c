/**
 * @file    motor_pwm.c
 * @brief   ESC PWM çıkışı. Zamanlayıcı 1 µs çözünürlükte olduğu için CCR = darbe genişliği (µs).
 */
#include "motor_pwm.h"
#include "boat_config.h"

static TIM_HandleTypeDef *s_htim;
static motor_cmd_t        s_out = { PWM_NEUTRAL_US, PWM_NEUTRAL_US };
static volatile int32_t   s_neutral_l = PWM_NEUTRAL_US;
static volatile int32_t   s_neutral_r = PWM_NEUTRAL_US;

static uint16_t safe_us(int32_t us)
{
    if (us < (int32_t)PWM_MIN_US) return PWM_MIN_US;
    if (us > (int32_t)PWM_MAX_US) return PWM_MAX_US;
    return (uint16_t)us;
}

void motor_pwm_init(TIM_HandleTypeDef *htim)
{
    s_htim = htim;
    motor_pwm_neutral();
    HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_2);
}

void motor_pwm_set_raw(const motor_cmd_t *cmd)
{
    s_out.left_us  = safe_us(cmd->left_us);
    s_out.right_us = safe_us(cmd->right_us);
    /* Preload aktif: yeni değer bir sonraki periyot başında uygulanır (glitch yok) */
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, s_out.left_us);
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_2, s_out.right_us);
}

void motor_pwm_set(const motor_cmd_t *cmd)
{
    motor_cmd_t t;
    t.left_us  = safe_us((int32_t)cmd->left_us  + (s_neutral_l - (int32_t)PWM_NEUTRAL_US));
    t.right_us = safe_us((int32_t)cmd->right_us + (s_neutral_r - (int32_t)PWM_NEUTRAL_US));
    motor_pwm_set_raw(&t);
}

void motor_pwm_neutral(void)
{
    motor_cmd_t n = catamaran_mixer_neutral();
    motor_pwm_set(&n);
}

void motor_pwm_set_neutrals(uint16_t left_us, uint16_t right_us)
{
    s_neutral_l = left_us;
    s_neutral_r = right_us;
}

void motor_pwm_get_neutrals(uint16_t *left_us, uint16_t *right_us)
{
    *left_us  = (uint16_t)s_neutral_l;
    *right_us = (uint16_t)s_neutral_r;
}

motor_cmd_t motor_pwm_get_output(void)
{
    return s_out;
}
