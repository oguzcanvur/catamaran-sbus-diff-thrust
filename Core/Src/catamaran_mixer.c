/**
 * @file    catamaran_mixer.c
 * @brief   Arcade diferansiyel itiş: sol = gaz + dümen, sağ = gaz - dümen.
 */
#include "catamaran_mixer.h"
#include "boat_config.h"

#define CLAMP(x, lo, hi)  (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

uint16_t mixer_apply_deadband(uint16_t us)
{
    if (us >= DEADBAND_LOW_US && us <= DEADBAND_HIGH_US) {
        return PWM_NEUTRAL_US;
    }
    return (uint16_t)CLAMP(us, PWM_MIN_US, PWM_MAX_US);
}

static uint16_t reverse_us(uint16_t us)
{
    return (uint16_t)(2u * PWM_NEUTRAL_US - us);
}

motor_cmd_t catamaran_mixer(uint16_t throttle_us, uint16_t steering_us)
{
    throttle_us = mixer_apply_deadband(throttle_us);
    steering_us = mixer_apply_deadband(steering_us);

#if THROTTLE_REVERSE
    throttle_us = reverse_us(throttle_us);
#endif
#if STEERING_REVERSE
    steering_us = reverse_us(steering_us);
#endif

    int16_t throttle_delta = (int16_t)throttle_us - (int16_t)PWM_NEUTRAL_US;            /* [-500, +500] */
    int16_t steering_delta = (int16_t)(((int16_t)steering_us - (int16_t)PWM_NEUTRAL_US)
                                       * TURN_SENSITIVITY);                               /* [-425, +425] */

    int32_t left_cmd  = (int32_t)PWM_NEUTRAL_US + throttle_delta + steering_delta;
    int32_t right_cmd = (int32_t)PWM_NEUTRAL_US + throttle_delta - steering_delta;

    motor_cmd_t out;
    out.left_us  = (uint16_t)CLAMP(left_cmd,  (int32_t)PWM_MIN_US, (int32_t)PWM_MAX_US);
    out.right_us = (uint16_t)CLAMP(right_cmd, (int32_t)PWM_MIN_US, (int32_t)PWM_MAX_US);

#if LEFT_MOTOR_REVERSE
    out.left_us = reverse_us(out.left_us);
#endif
#if RIGHT_MOTOR_REVERSE
    out.right_us = reverse_us(out.right_us);
#endif
    (void)reverse_us;
    return out;
}

motor_cmd_t catamaran_mixer_neutral(void)
{
    motor_cmd_t out = { PWM_NEUTRAL_US, PWM_NEUTRAL_US };
    return out;
}
