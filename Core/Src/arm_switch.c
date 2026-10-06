/**
 * @file    arm_switch.c
 * @brief   Arm anahtarı durum makinesi (histerezisli eşik + güvenlik kilitleri).
 */
#include "arm_switch.h"
#include "boat_config.h"
#include "catamaran_mixer.h"

static bool     s_armed;
static bool     s_seen_off;      /* son disarm'dan beri anahtar KAPALI görüldü mü */
static bool     s_sw_on;         /* histerezisli anahtar konumu                    */
static bool     s_link_lost;
static uint32_t s_lost_since;

static void disarm_hard(void)
{
    s_armed    = false;
    s_seen_off = false;          /* tekrar arm için anahtar kapatılıp açılmalı */
}

arm_state_t arm_update(uint32_t now_ms, bool link_ok, const sbus_data_t *rc)
{
    /* Uzun sinyal kaybı -> disarm */
    if (!link_ok) {
        if (!s_link_lost) { s_link_lost = true; s_lost_since = now_ms; }
        if (now_ms - s_lost_since >= ARM_LINK_LOSS_DISARM_MS) disarm_hard();
        return s_armed ? ARM_ARMED : ARM_DISARMED;
    }
    s_link_lost = false;

    /* Anahtar konumu (histerezis: arada kalan değerler son durumu korur) */
    uint16_t sw = rc->us[ARM_SWITCH_CH];
#if ARM_SWITCH_REVERSE
    sw = (uint16_t)(2u * PWM_NEUTRAL_US - sw);
#endif
    if (sw >= ARM_SWITCH_ON_US)       s_sw_on = true;
    else if (sw <= ARM_SWITCH_OFF_US) s_sw_on = false;

    if (!s_sw_on) {
        s_armed    = false;
        s_seen_off = true;
        return ARM_DISARMED;
    }
    if (s_armed) return ARM_ARMED;

    const bool sticks_centered =
        mixer_apply_deadband(rc->us[SBUS_CH_THROTTLE]) == PWM_NEUTRAL_US &&
        mixer_apply_deadband(rc->us[SBUS_CH_STEERING]) == PWM_NEUTRAL_US;

    if (s_seen_off && sticks_centered) {
        s_armed = true;
        return ARM_ARMED;
    }
    return ARM_WAITING;
}
