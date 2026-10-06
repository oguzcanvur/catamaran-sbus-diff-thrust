/**
 * @file    telemetry.c
 * @brief   Kesme tabanlı (bloklamayan) durum satırı gönderimi ve PC komut alımı.
 */
#include "telemetry.h"
#include "boat_config.h"
#include <stdio.h>

static UART_HandleTypeDef *s_huart;
static char                s_line[256];
static uint32_t            s_last_ms;

/* ---- Komut alımı ---- */
static uint8_t  s_rx_buf[32];
static char     s_cmd[32];
static uint8_t  s_cmd_len;

static volatile bool     s_test_on;
static volatile uint32_t s_test_tick;
static volatile uint16_t s_test_l = PWM_NEUTRAL_US;
static volatile uint16_t s_test_r = PWM_NEUTRAL_US;

static volatile bool     s_neutral_req;
static volatile uint16_t s_neutral_l;
static volatile uint16_t s_neutral_r;

static void rx_start(void)
{
    HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buf, sizeof(s_rx_buf));
}

static bool parse_uint(const char **p, uint16_t *v)
{
    const char *s = *p;
    while (*s == ' ') s++;
    if (*s < '0' || *s > '9') return false;
    uint32_t n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10u + (uint32_t)(*s - '0');
        if (n > 65535u) return false;
        s++;
    }
    *v = (uint16_t)n;
    *p = s;
    return true;
}

static uint16_t clamp_test(uint16_t us)
{
    if (us < TEST_MIN_US) return TEST_MIN_US;
    if (us > TEST_MAX_US) return TEST_MAX_US;
    return us;
}

static void process_cmd(void)
{
    s_cmd[s_cmd_len] = '\0';
    const char *p = s_cmd + 1;
    uint16_t l, r;

    if (s_cmd[0] == 'T' && parse_uint(&p, &l) && parse_uint(&p, &r)) {
        s_test_l    = clamp_test(l);
        s_test_r    = clamp_test(r);
        s_test_tick = HAL_GetTick();
        s_test_on   = true;
    } else if (s_cmd[0] == 'N' && parse_uint(&p, &l) && parse_uint(&p, &r)) {
        s_neutral_l   = l;
        s_neutral_r   = r;
        s_neutral_req = true;
    } else if (s_cmd[0] == 'X') {
        s_test_on = false;
    }
}

void telemetry_on_rx_event(uint16_t size)
{
    for (uint16_t i = 0; i < size && i < sizeof(s_rx_buf); i++) {
        char c = (char)s_rx_buf[i];
        if (c == '\n' || c == '\r') {
            if (s_cmd_len > 0u) process_cmd();
            s_cmd_len = 0;
        } else if (s_cmd_len < sizeof(s_cmd) - 1u) {
            s_cmd[s_cmd_len++] = c;
        } else {
            s_cmd_len = 0;   /* taşma: satırı at */
        }
    }
    rx_start();
}

void telemetry_on_error(void)
{
    HAL_UART_AbortReceive(s_huart);
    s_cmd_len = 0;
    rx_start();
}

bool telemetry_get_test(uint32_t now_ms, motor_cmd_t *out)
{
    __disable_irq();
    bool     on   = s_test_on;
    uint32_t tick = s_test_tick;
    uint16_t l    = s_test_l;
    uint16_t r    = s_test_r;
    __enable_irq();

    if (!on) return false;
    if (now_ms - tick >= TEST_TIMEOUT_MS) {
        s_test_on = false;   /* PC sustu -> güvenli tarafa geç */
        return false;
    }
    out->left_us  = l;
    out->right_us = r;
    return true;
}

bool telemetry_take_neutral_request(uint16_t *left_us, uint16_t *right_us)
{
    if (!s_neutral_req) return false;
    __disable_irq();
    *left_us      = s_neutral_l;
    *right_us     = s_neutral_r;
    s_neutral_req = false;
    __enable_irq();
    return true;
}

/* ---- Durum çıktısı ---- */

void telemetry_init(UART_HandleTypeDef *huart)
{
    s_huart   = huart;
    s_last_ms = 0;
    static const char banner[] =
        "\r\n=== Katamaran S.BUS kontrol - telemetri (115200 8N1) ===\r\n";
    HAL_UART_Transmit(s_huart, (const uint8_t *)banner, sizeof(banner) - 1, 50);
    rx_start();
}

void telemetry_task(uint32_t now_ms, const sbus_data_t *rc, const motor_cmd_t *out,
                    const char *state, uint16_t neutral_l, uint16_t neutral_r)
{
    if (now_ms - s_last_ms < TELEMETRY_PERIOD_MS) return;
    if (s_huart->gState != HAL_UART_STATE_READY) return;   /* önceki satır hâlâ gidiyor */
    s_last_ms = now_ms;

    int n = snprintf(s_line, sizeof(s_line),
        "[%-6s] CH1(dumen)=%4u CH2(gaz)=%4u CH3=%4u CH4=%4u | SOL=%4u SAG=%4u"
        " | FS=%u LOST=%u | paket=%lu hata=%lu | RAW1=%4u RAW2=%4u | NL=%4u NR=%4u | SW=%4u\r\n",
        state,
        rc->us[SBUS_CH_STEERING], rc->us[SBUS_CH_THROTTLE], rc->us[2], rc->us[3],
        out->left_us, out->right_us,
        rc->failsafe ? 1u : 0u, rc->frame_lost ? 1u : 0u,
        (unsigned long)rc->frame_count, (unsigned long)rc->error_count,
        rc->raw[0], rc->raw[1], neutral_l, neutral_r, rc->us[ARM_SWITCH_CH]);

    if (n > 0) {
        if (n >= (int)sizeof(s_line)) n = sizeof(s_line) - 1;
        HAL_UART_Transmit_IT(s_huart, (const uint8_t *)s_line, (uint16_t)n);
    }
}
