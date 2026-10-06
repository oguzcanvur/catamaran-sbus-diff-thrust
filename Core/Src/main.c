/**
 * @file    main.c
 * @brief   STM32G431RB - Çift yönlü katamaran tekne kontrolü.
 *
 *  S.BUS (R9DS)  -> PA10 USART1_RX (100k, 8E2, RX tersleme aktif, DMA + Idle)
 *  Sol ESC       -> PA0  TIM2_CH1  (50 Hz, 1 µs çözünürlük)
 *  Sağ ESC       -> PA1  TIM2_CH2
 *  LD2 (PA5)     -> Yanık: link OK | Yanıp söner: failsafe / sinyal yok | Hızlı: arming
 */
#include "main.h"
#include "boat_config.h"
#include "sbus.h"
#include "catamaran_mixer.h"
#include "motor_pwm.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;
TIM_HandleTypeDef  htim2;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    /* Önce PWM'i nötrde başlat (ESC'ler hiçbir zaman tanımsız sinyal görmesin) */
    motor_pwm_init(&htim2);
    sbus_init(&huart1);

    const uint32_t boot_ms = HAL_GetTick();
    sbus_data_t    rc;
    uint32_t       led_ms = 0;

    while (1)
    {
        const uint32_t now = HAL_GetTick();
        const bool     arming = (now - boot_ms) < ESC_ARM_TIME_MS;
        const bool     fresh  = sbus_get_frame(&rc);
        const bool     link   = sbus_link_ok(now);

        if (arming || !link) {
            /* Arming süresi, sinyal kaybı (timeout) veya alıcı failsafe -> DUR */
            motor_pwm_neutral();
        } else if (fresh) {
            motor_cmd_t cmd = catamaran_mixer(rc.us[SBUS_CH_THROTTLE],
                                              rc.us[SBUS_CH_STEERING]);
            motor_pwm_set(&cmd);
        }

        /* Durum LED'i */
        if (arming) {
            if (now - led_ms >= 50u)  { led_ms = now; HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); }
        } else if (!link) {
            if (now - led_ms >= 250u) { led_ms = now; HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); }
        } else {
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
        }
    }
}

/**
 * HSI 16 MHz -> PLL (M=4, N=85, R=2) -> SYSCLK 170 MHz, Boost modu, Flash 4 WS.
 * APB1 = APB2 = 170 MHz (TIM2 ve USART1 saati 170 MHz).
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = RCC_PLLM_DIV4;
    osc.PLL.PLLN            = 85;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = RCC_PLLQ_DIV2;
    osc.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    g.Pin   = LD2_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &g);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/**
 * S.BUS: 100000 baud, 8 veri + Even parity (=> HAL'de 9 bit kelime), 2 stop.
 * RX pin seviyesi donanımsal terslenir -> harici inverter gerekmez.
 */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 100000;
    huart1.Init.WordLength             = UART_WORDLENGTH_9B;   /* 8 veri + parity */
    huart1.Init.StopBits               = UART_STOPBITS_2;
    huart1.Init.Parity                 = UART_PARITY_EVEN;
    huart1.Init.Mode                   = UART_MODE_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_RXINVERT_INIT;
    huart1.AdvancedInit.RxPinLevelInvert = UART_ADVFEATURE_RXINV_ENABLE;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();

    HAL_UARTEx_DisableFifoMode(&huart1);
}

/**
 * TIM2: 170 MHz / (169+1) = 1 MHz (1 µs tick), ARR = 19999 -> 20 ms (50 Hz).
 */
static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef  clk = {0};
    TIM_MasterConfigTypeDef master = {0};
    TIM_OC_InitTypeDef      oc = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 170 - 1;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 20000 - 1;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    clk.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &clk) != HAL_OK) Error_Handler();

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    master.MasterOutputTrigger = TIM_TRGO_RESET;
    master.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &master) != HAL_OK) Error_Handler();

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = PWM_NEUTRAL_US;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

    /* PA0 / PA1 -> AF1 (TIM2_CH1 / TIM2_CH2) */
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &g);
}

void Error_Handler(void)
{
    __disable_irq();
    /* Zamanlayıcı çalışıyorsa motorları nötre çek */
    TIM2->CCR1 = PWM_NEUTRAL_US;
    TIM2->CCR2 = PWM_NEUTRAL_US;
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
