/***************************************************************************//**
 * @file app.c
 * @brief Main application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 ********************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided \'as-is\', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *******************************************************************************
 * # Experimental Quality
 * This code has been minimally tested to ensure that it builds and is suitable
 * as a demonstration for evaluation purposes only. This code will be maintained
 * at the sole discretion of Silicon Labs.
 ******************************************************************************/
#include "rsi_rom_egpio.h"
#include "rsi_rom_clks.h"
#include "rsi_ct.h"
#include "rsi_pll.h"
#include "rsi_debug.h"
#include "clock_update.h"

#define CONFIG_TIMER_OUTPUT_GPIO_PIN          RTE_SCT_OUT_0_PIN
#define CONFIG_TIMER_OUTPUT_GPIO_PORT         RTE_SCT_OUT_0_PORT
#define CONFIG_TIMER_OUTPUT_GPIO_MUX          RTE_SCT_OUT_0_MUX
#define CONFIG_TIMER_0_BASE_ADD               CT0
#define CONFIG_TIMER_IRQHandler               IRQ034_Handler

#define PLL_REF_CLK_VAL_XTAL                  40000000UL
#define INTF_PLL_FREQ                         160000000UL
#define PWM_FREQUENCY                         1000
#define INITIAL_DUTY_CYCLE                    50
#define CONFIG_TIMER_FREQUENCY                RSI_CLK_GetBaseClock(M4_CT)
#define TOP_COUNTER_VALUE                     CONFIG_TIMER_FREQUENCY \
  / PWM_FREQUENCY
#define CONFIG_TIMER_COUNTER_16BIT_MODE       0

#define OUTPUT_COMPARE_VALUE                  ((100 - INITIAL_DUTY_CYCLE) \
                                               * TOP_COUNTER_VALUE) / 100

static void sl_config_timer_gpio_init(void);
static void sl_config_timer_init(void);

/***************************************************************************/ /**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  sl_config_timer_gpio_init();
  sl_config_timer_init();
}

/***************************************************************************/ /**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}

static void sl_config_timer_gpio_init(void)
{
  if ((CONFIG_TIMER_OUTPUT_GPIO_PIN > 24)
      && (CONFIG_TIMER_OUTPUT_GPIO_PIN < 31)) {
    RSI_EGPIO_HostPadsGpioModeEnable(RTE_SCT_OUT_0_PIN);
  }

  RSI_EGPIO_PadReceiverEnable(RTE_SCT_OUT_0_PIN);

  RSI_EGPIO_SetDir(EGPIO,
                   CONFIG_TIMER_OUTPUT_GPIO_PORT,
                   CONFIG_TIMER_OUTPUT_GPIO_PIN,
                   EGPIO_CONFIG_DIR_OUTPUT);

  RSI_EGPIO_SetPinMux(EGPIO,
                      CONFIG_TIMER_OUTPUT_GPIO_PORT,
                      CONFIG_TIMER_OUTPUT_GPIO_PIN,
                      CONFIG_TIMER_OUTPUT_GPIO_MUX);
}

static void sl_config_timer_init(void)
{
  uint32_t ct_config = 0;
  uint32_t ct_pwm_config = 0;

  RSI_CLK_CtClkConfig(M4CLK, SCT_CLOCK_SOURCE, SCT_CLOCK_DIV_FACT,
                      ENABLE_STATIC_CLK);

  RSI_CLK_SetIntfPllFreq(M4CLK, INTF_PLL_FREQ, PLL_REF_CLK_VAL_XTAL);

  ct_config = PERIODIC_ENCOUNTER_0 | COUNTER0_UP;

  RSI_CT_SetControl(CONFIG_TIMER_0_BASE_ADD, ct_config);

  ct_pwm_config = OUTPUT_OCU_0 | MAKE_OUTPUT_0_HIGH_SEL_0
                  | MAKE_OUTPUT_0_LOW_SEL_0;
  RSI_CT_OCUConfigSet(CONFIG_TIMER_0_BASE_ADD, ct_pwm_config);

  RSI_CT_SetCount(CONFIG_TIMER_0_BASE_ADD, 0);

  RSI_CT_SetMatchCount(CONFIG_TIMER_0_BASE_ADD,
                       TOP_COUNTER_VALUE,
                       CONFIG_TIMER_COUNTER_16BIT_MODE,
                       COUNTER_0);

  CONFIG_TIMER_0_BASE_ADD->CT_OCU_COMPARE_REG_b.OCU_COMPARE_0_REG =
    OUTPUT_COMPARE_VALUE;

  RSI_CT_StartSoftwareTrig(CONFIG_TIMER_0_BASE_ADD, COUNTER_0);
}
