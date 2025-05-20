
/***************************************************************************/ /**
 * @file app.c
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "app.h"

#include "rsi_rom_egpio.h"
#include "rsi_rom_clks.h"
#include "rsi_egpio.h"
#include "rsi_ct.h"
#include "rsi_debug.h"

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define CONFIG_TIMER_INPUT_GPIO_PORT  RTE_SCT_IN_0_PORT
#define CONFIG_TIMER_INPUT_GPIO_PIN   RTE_SCT_IN_0_PIN
#define CONFIG_TIMER_INPUT_GPIO_PAD   RTE_SCT_IN_0_PAD
#define CONFIG_TIMER_0_BASE_ADD       CT0
#define CONFIG_TIMER_IRQHandler       IRQ034_Handler
#define FALLING_EDGE_EVENT            0x05
#define EDGE_CAPTURE_BUFFER_SIZE      2
#define TOP_COUNTER_VALUE             0xFFFFFFFF

/*******************************************************************************
 **********************  Local variables   *************************************
 ******************************************************************************/
static volatile uint16_t flag;
static volatile uint16_t capture_value;
static volatile uint8_t data_ready;

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
void CONFIG_TIMER_IRQHandler(void)
{
  uint32_t flag = RSI_CT_GetInterruptStatus(CONFIG_TIMER_0_BASE_ADD);
  RSI_CT_InterruptClear(CONFIG_TIMER_0_BASE_ADD, flag);
  if (flag == RSI_CT_EVENT_INTR_0_l) {
    capture_value = CONFIG_TIMER_0_BASE_ADD->CT_CAPTURE_REG;
    data_ready = 1;
  }
}

static void RSI_EGPIO_CLK_init(void)
{
  M4CLK->CLK_ENABLE_SET_REG3_b.EGPIO_CLK_ENABLE_b = 1;
  M4CLK->CLK_ENABLE_SET_REG2_b.EGPIO_PCLK_ENABLE_b = 1;
}

static void gpio_init(void)
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

static void config_timer_init(void)
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

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  gpio_init();
  config_timer_init();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  if (data_ready) {
    // clear flag here
    flag = 0;
    DEBUGOUT("capture value %d\n", capture_value);
  }
}
