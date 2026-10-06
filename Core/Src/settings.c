/**
 * @file    settings.c
 * @brief   Flash'ta kalıcı ayar kaydı (STM32G431: tek bank, 2 KB sayfa, 64-bit programlama).
 *
 * Kayıt düzeni (16 bayt = 2 double-word):
 *   [0] magic (32) | left_neutral (16) | right_neutral (16)
 *   [1] checksum (32) | ~magic (32)
 */
#include "settings.h"
#include "boat_config.h"
#include "motor_trim.h"
#include "stm32g4xx_hal.h"

#define SETTINGS_PAGE       63u
#define SETTINGS_ADDR       (FLASH_BASE + SETTINGS_PAGE * FLASH_PAGE_SIZE)   /* 0x0801F800 */
#define SETTINGS_MAGIC      0x4B544D31u                                      /* "KTM1" */

static uint32_t checksum(uint32_t magic, uint16_t l, uint16_t r)
{
    uint32_t c = 0x811C9DC5u;
    c = (c ^ magic) * 0x01000193u;
    c = (c ^ l)     * 0x01000193u;
    c = (c ^ r)     * 0x01000193u;
    return c;
}

bool settings_valid(const settings_t *s)
{
    return s->left_neutral_us  >= TEST_MIN_US && s->left_neutral_us  <= TEST_MAX_US
        && s->right_neutral_us >= TEST_MIN_US && s->right_neutral_us <= TEST_MAX_US;
}

bool settings_load(settings_t *s)
{
    const volatile uint32_t *w = (const volatile uint32_t *)SETTINGS_ADDR;
    uint32_t magic = w[0];
    uint16_t l     = (uint16_t)(w[1] & 0xFFFFu);
    uint16_t r     = (uint16_t)(w[1] >> 16);

    if (magic == SETTINGS_MAGIC && w[3] == ~SETTINGS_MAGIC && w[2] == checksum(magic, l, r)) {
        settings_t t = { l, r };
        if (settings_valid(&t)) {
            *s = t;
            return true;
        }
    }
    s->left_neutral_us  = LEFT_NEUTRAL_US;
    s->right_neutral_us = RIGHT_NEUTRAL_US;
    return false;
}

bool settings_save(const settings_t *s)
{
    if (!settings_valid(s)) return false;

    uint64_t dw0 = (uint64_t)SETTINGS_MAGIC
                 | ((uint64_t)s->left_neutral_us  << 32)
                 | ((uint64_t)s->right_neutral_us << 48);
    uint64_t dw1 = (uint64_t)checksum(SETTINGS_MAGIC, s->left_neutral_us, s->right_neutral_us)
                 | ((uint64_t)(~SETTINGS_MAGIC) << 32);

    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = FLASH_BANK_1;
    erase.Page      = SETTINGS_PAGE;
    erase.NbPages   = 1;
    uint32_t page_err = 0;

    bool ok = false;
    if (HAL_FLASH_Unlock() == HAL_OK) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        if (HAL_FLASHEx_Erase(&erase, &page_err) == HAL_OK
         && HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, SETTINGS_ADDR,     dw0) == HAL_OK
         && HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, SETTINGS_ADDR + 8, dw1) == HAL_OK) {
            ok = true;
        }
        HAL_FLASH_Lock();
    }
    if (!ok) return false;

    settings_t chk;
    return settings_load(&chk)
        && chk.left_neutral_us  == s->left_neutral_us
        && chk.right_neutral_us == s->right_neutral_us;
}
