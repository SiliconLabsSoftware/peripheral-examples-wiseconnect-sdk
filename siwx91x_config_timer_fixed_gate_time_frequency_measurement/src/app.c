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
#include "clock_update.h"
#include "rsi_egpio.h"
#include "rsi_debug.h"
#include "rsi_ct.h"

#define SL_SI91X_REQUIRES_INTF_PLL

#define SL_GPIO_INTERRUPT_ENABLE          1
#define SL_GPIO_INTERRUPT_DISABLE         0

#define GPIO_INTERRUPT_PRIORITY0          52
#define INTERRUPT_CLR                     0x07

#define INTERRUPT_CHANNEL                 0
#define MAX_GPIO_PORT_PIN                 16
#define INPUT_EGPIO_PORT                  RTE_GPIO_25_PORT
#define INPUT_EGPIO_PIN                   RTE_GPIO_25_PIN
#define CONFIG_TIMER_INPUT_GPIO_PAD       RTE_SCT_IN_0_PAD
#define CONFIG_TIMER_0_BASE_ADD           CT0
#define CONFIG_TIMER_IRQHandler           IRQ034_Handler
#define GPIO_PIN25_IRQHandler             IRQ052_Handler
#define RISING_EDGE_EVENT                 0x01

#define GATE_TIME_INTERVAL_SEC            10 // Should be changed depending on different cases
#define TOP_COUNTER_VALUE                 (RSI_CLK_GetBaseClock(M4_CT) \
                                           * GATE_TIME_INTERVAL_SEC)

static volatile uint32_t edge_counts = 0;
static volatile bool first_starting_edge = false;
static volatile bool measurement_ready = false;
static volatile uint32_t estimated_frequency = 0;

static void sl_gpio_init(void);
static void sl_config_timer_init(void);
static void RSI_EGPIO_CLK_init(void);

/***************************************************************************/ /**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  sl_gpio_init();
  sl_config_timer_init();
}

/***************************************************************************/ /**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  if (measurement_ready) {
    estimated_frequency = edge_counts / GATE_TIME_INTERVAL_SEC;
    measurement_ready = false;
    first_starting_edge = true;
  }
}

static void sl_gpio_init(void)
{
  RSI_EGPIO_CLK_init();

  if ((INPUT_EGPIO_PIN > 24)
      && (INPUT_EGPIO_PIN < 31)) {
    RSI_EGPIO_HostPadsGpioModeEnable(INPUT_EGPIO_PIN);
  }

  RSI_EGPIO_PadReceiverEnable(INPUT_EGPIO_PIN);

  RSI_EGPIO_SetDir(EGPIO, INPUT_EGPIO_PORT,
                   INPUT_EGPIO_PIN, EGPIO_CONFIG_DIR_INPUT);

  NVIC_EnableIRQ(EGPIO_PIN_0_IRQn);

  NVIC_SetPriority(EGPIO_PIN_0_IRQn, GPIO_INTERRUPT_PRIORITY0);

  RSI_EGPIO_PinIntSel(EGPIO,
                      INTERRUPT_CHANNEL,
                      (INPUT_EGPIO_PIN
                       > MAX_GPIO_PORT_PIN) ? (INPUT_EGPIO_PIN
                                               / MAX_GPIO_PORT_PIN) : INPUT_EGPIO_PORT,
                      INPUT_EGPIO_PIN);

  RSI_EGPIO_SetIntRiseEdgeEnable(EGPIO, INTERRUPT_CHANNEL);

  RSI_EGPIO_IntUnMask(EGPIO, INTERRUPT_CHANNEL);

  DEBUGOUT("Successfully set pin mode for GPIO_25\r\n");
}

static void sl_config_timer_init(void)
{
  uint32_t ct_config_value = 0;
  uint32_t interrupt_flags = 0;

  RSI_CLK_CtClkConfig(M4CLK, CT_SOCPLLCLK, SCT_CLOCK_DIV_FACT,
                      ENABLE_STATIC_CLK);

  ct_config_value = COUNTER32_BITMODE | PERIODIC_ENCOUNTER_0 | COUNTER0_UP;
  interrupt_flags = RSI_CT_EVENT_COUNTER_1_IS_PEAK_l;

  RSI_CT_SetControl(CONFIG_TIMER_0_BASE_ADD, ct_config_value);
  DEBUGOUT("Successfully set configuration for Config Timer\r\n");

  RSI_CT_PeripheralReset(CONFIG_TIMER_0_BASE_ADD, (boolean_t)COUNTER_0);
  RSI_CT_SetCount(CONFIG_TIMER_0_BASE_ADD, 0);
  DEBUGOUT("Successfully set CT Initial Count\n");

  CONFIG_TIMER_0_BASE_ADD->CT_MATCH_REG = TOP_COUNTER_VALUE;
  DEBUGOUT("Successfully set CT Match Count\n");

  RSI_CT_InterruptDisable(CONFIG_TIMER_0_BASE_ADD, interrupt_flags);
  RSI_CT_InterruptEnable(CONFIG_TIMER_0_BASE_ADD, interrupt_flags);
  NVIC_EnableIRQ(CT_IRQn);
  DEBUGOUT("Successfully enabled interrupt for Config Timer\r\n");

  RSI_CT_StartSoftwareTrig(CONFIG_TIMER_0_BASE_ADD, COUNTER_0);
  DEBUGOUT("Successfully started Config Timer\r\n");
  first_starting_edge = true;
}

static void RSI_EGPIO_CLK_init(void)
{
  M4CLK->CLK_ENABLE_SET_REG3_b.EGPIO_CLK_ENABLE_b = 1;
  M4CLK->CLK_ENABLE_SET_REG2_b.EGPIO_PCLK_ENABLE_b = 1;
}

void CONFIG_TIMER_IRQHandler(void)
{
  uint32_t flag = RSI_CT_GetInterruptStatus(CONFIG_TIMER_0_BASE_ADD);
  RSI_CT_InterruptClear(CONFIG_TIMER_0_BASE_ADD, flag);

  if (flag & RSI_CT_EVENT_COUNTER_0_IS_PEAK_l) {
    measurement_ready = true;
  }
}

void GPIO_PIN25_IRQHandler(void)
{
  RSI_EGPIO_IntClr(EGPIO, INTERRUPT_CHANNEL, INTERRUPT_CLR);

  if (first_starting_edge) {
    first_starting_edge = false;
    edge_counts = 1; // First starting edge, therefore edge_counts starts at 1
    RSI_CT_SetCount(CONFIG_TIMER_0_BASE_ADD, 1);
    return;
  }

  edge_counts++;
}
