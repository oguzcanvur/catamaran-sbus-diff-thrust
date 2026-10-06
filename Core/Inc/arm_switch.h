/**
 * @file    arm_switch.h
 * @brief   Kumanda arm anahtarı (varsayılan CH10) ve güvenlik kilitleri.
 *
 * Arm olma şartları:
 *   1) Anahtar ARM konumunda,
 *   2) Anahtar açılıştan / son disarm'dan sonra en az bir kez KAPALI görülmüş,
 *   3) Sağ kol ortada (gaz ve dümen ölü bölgede).
 * Anahtar kapanınca veya sinyal ARM_LINK_LOSS_DISARM_MS'den uzun kesilince disarm olur.
 */
#ifndef ARM_SWITCH_H
#define ARM_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include "sbus.h"

typedef enum {
    ARM_DISARMED = 0,   /* anahtar kapalı -> motorlar kilitli               */
    ARM_WAITING,        /* anahtar açık ama kilit şartı sağlanmadı          */
    ARM_ARMED           /* sürüş aktif                                       */
} arm_state_t;

/* Ana döngüden her turda çağrılır. rc: son geçerli S.BUS paketi. */
arm_state_t arm_update(uint32_t now_ms, bool link_ok, const sbus_data_t *rc);

#endif /* ARM_SWITCH_H */
