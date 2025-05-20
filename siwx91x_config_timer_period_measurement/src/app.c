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
#include "rsi_egpio.h"
#include "rsi_debug.h"
#include "rsi_ct.h"
#include "clock_update.h"

#define SL_SI91X_REQUIRES_INTF_PLL

#define CONFIG_TIMER_INPUT_GPIO_PORT  RTE_SCT_IN_0_PORT
#define CONFIG_TIMER_INPUT_GPIO_PIN   RTE_SCT_IN_0_PIN
#define CONFIG_TIMER_INPUT_GPIO_PAD   RTE_SCT_IN_0_PAD
#define CONFIG_TIMER_0_BASE_ADD       CT0
#define CONFIG_TIMER_IRQHandler       IRQ034_Handler
#define FALLING_EDGE_EVENT            0x05

#define EDGE_CAPTURE_BUFFER_SIZE      2
#define TOP_COUNTER_VALUE             0xFFFFFFFF // Top value for the timer/counter
#define CONFIG_TIMER_FREQ             RSI_CLK_GetBaseClock(M4_CT)

static volatile uint32_t edge_capture_buffer[EDGE_CAPTURE_BUFFER_SIZE];
static volatile uint8_t edge_capture_index = 0;
static volatile bool measurement_ready = false;
static volatile bool counter_1_overflow = false;
static volatile uint32_t period_measurement_us = 0;

static void sl_gpio_init(void);
static void sl_config_timer_init(void);
static void RSI_EGPIO_CLK_init(void);
static uint32_t calculate_period(void);

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
    period_measurement_us = calculate_period();
  }
}

static void sl_gpio_init(void)
{
  RSI_EGPIO_CLK_init();

  if ((CONFIG_TIMER_INPUT_GPIO_PIN > 24)
      && (CONFIG_TIMER_INPUT_GPIO_PIN < 31)) {
    RSI_EGPIO_HostPadsGpioModeEnable(RTE_SCT_IN_0_PIN);
  }

  RSI_EGPIO_PadReceiverEnable(RTE_SCT_IN_0_PIN);

  RSI_EGPIO_SetDir(EGPIO, CONFIG_TIMER_INPUT_GPIO_PORT,
                   CONFIG_TIMER_INPUT_GPIO_PIN, EGPIO_CONFIG_DIR_INPUT);

  RSI_EGPIO_SetPinMux(EGPIO, CONFIG_TIMER_INPUT_GPIO_PORT,
                      CONFIG_TIMER_INPUT_GPIO_PIN, RTE_SCT_IN_0_MUX);

  DEBUGOUT("Successfully set pin mode for GPIO_25\r\n");
}

static void sl_config_timer_init(void)
{
  uint32_t ct_config_value = 0;
  ct_config_value = 0;
  uint32_t interrupt_flags = 0;
  interrupt_flags = 0;

  RSI_CLK_CtClkConfig(M4CLK, CT_SOCPLLCLK, SCT_CLOCK_DIV_FACT,
                      ENABLE_STATIC_CLK);

  ct_config_value = COUNTER32_BITMODE | PERIODIC_ENCOUNTER_0 | COUNTER0_UP;
  interrupt_flags = RSI_CT_EVENT_INTR_0_l | RSI_CT_EVENT_COUNTER_0_IS_PEAK_l
                    | RSI_CT_EVENT_COUNTER_1_IS_PEAK_l;

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

  RSI_CT_InterruptEventSelect(CONFIG_TIMER_0_BASE_ADD, FALLING_EDGE_EVENT);
  DEBUGOUT("Successfully selected interrupt action event for Config Timer\r\n");

  RSI_CT_CaptureEventSelect(CONFIG_TIMER_0_BASE_ADD, FALLING_EDGE_EVENT);
  DEBUGOUT("Successfully selected capture action event for Config Timer\r\n");

  RSI_CT_StartSoftwareTrig(CONFIG_TIMER_0_BASE_ADD, COUNTER_0);
  DEBUGOUT("Successfully started Config Timer\r\n");
}

static uint32_t calculate_period(void)
{
  uint32_t counts_between_edges = 0;

  if (edge_capture_buffer[1] < edge_capture_buffer[0]) {
    counts_between_edges = TOP_COUNTER_VALUE - edge_capture_buffer[0] + 1
                           + edge_capture_buffer[1];
  } else {
    counts_between_edges = edge_capture_buffer[1] - edge_capture_buffer[0];
  }
  measurement_ready = false;

  return (counts_between_edges / (CONFIG_TIMER_FREQ / 1000000)); // Period in micro seconds
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
  if (flag & RSI_CT_EVENT_COUNTER_1_IS_PEAK_l) {
    counter_1_overflow = 1;
  }

  if (flag & RSI_CT_EVENT_COUNTER_0_IS_PEAK_l) {
    if (counter_1_overflow) {
      CONFIG_TIMER_0_BASE_ADD->CT_COUNTER_REG = 0;
      counter_1_overflow = 0;
    }
  }

  if (flag == RSI_CT_EVENT_INTR_0_l) {
    edge_capture_buffer[edge_capture_index++] =
      CONFIG_TIMER_0_BASE_ADD->CT_CAPTURE_REG;
    if (edge_capture_index >= EDGE_CAPTURE_BUFFER_SIZE) {
      edge_capture_index = 0;
      measurement_ready = true;
    }
  }
}
