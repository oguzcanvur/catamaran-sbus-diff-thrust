/**
 * @file    boat_config.h
 * @brief   Katamaran kontrol yazılımı - tüm ayarlanabilir parametreler.
 */
#ifndef BOAT_CONFIG_H
#define BOAT_CONFIG_H

/* ---------------- PWM (ESC) ---------------- */
#define PWM_MIN_US              1000u   /* Tam geri   */
#define PWM_NEUTRAL_US          1500u   /* Nötr/Dur   */
#define PWM_MAX_US              2000u   /* Tam ileri  */

/* ---------------- Mikser ---------------- */
#define DEADBAND_LOW_US         1465u
#define DEADBAND_HIGH_US        1535u
#define TURN_SENSITIVITY        0.85f

/* Kanal yönü ters ise 1 yapın (kumandada reverse yerine) */
#define THROTTLE_REVERSE        0
#define STEERING_REVERSE        0
/* Motor tekneye ters monte edildiyse 1 yapın */
#define LEFT_MOTOR_REVERSE      0
#define RIGHT_MOTOR_REVERSE     0

/* ---------------- S.BUS ---------------- */
#define SBUS_CH_STEERING        0u      /* Kanal 1 - Aileron  (sağ kol yatay) */
#define SBUS_CH_THROTTLE        1u      /* Kanal 2 - Elevator (sağ kol dikey) */

/* Ham S.BUS değer -> µs dönüşümü (R9DS için gerekirse kalibre edin) */
#define SBUS_RAW_MIN            172
#define SBUS_RAW_CENTER         992
#define SBUS_RAW_MAX            1811

/* ---------------- Güvenlik ---------------- */
#define SBUS_TIMEOUT_MS         100u    /* Bu süre geçerli paket yoksa -> nötr */
#define ESC_ARM_TIME_MS         3000u   /* Açılışta ESC'lere sabit nötr süresi */

#endif /* BOAT_CONFIG_H */
