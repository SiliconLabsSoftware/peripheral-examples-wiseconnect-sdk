/***************************************************************************/ /**
 * @file i2c_leader_interrupt.c
 * @brief Si91x - I2C Transmission using I2C Interrupts
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_si91x_peripheral_i2c.h"
#include "sl_si91x_clock_manager.h"
#include "i2c_leader_interrupt.h"
#include "rsi_debug.h"
#include "rsi_rom_egpio.h"
#include "rsi_rom_clks.h"
#include "rsi_rom_ulpss_clk.h"

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define SIZE_BUFFERS              2    // Size of buffer
#define FOLLOWER_I2C_ADDR         0x50 // LM75 Temperature sensor I2C address
#define FIFO_THRESHOLD            0x0  // FIFO threshold
#define ZERO_FLAG                 0    // Zero flag, No argument
#define LAST_DATA_COUNT           0    // Last read-write count
#define DATA_COUNT                1    // Last second data count for verification
#define BIT_SET                   1    // Set bit
#define STOP_BIT                  9    // Bit to send stop command
#define RW_MASK_BIT               8    // Bit to mask read and write
#define MAX_7BIT_ADDRESS          127  // Maximum 7-bit address

#define I2C_USED                  ULP_I2C
#define I2C_BUFFER_SIZE           1024  // Size of data buffer
#define INITIAL_VALUE             0     // Initial value of buffer
#define BUFFER_OFFSET             0x1   // Buffer offset

/*******************************************************************************
 ******************************  Data Types  ***********************************
 ******************************************************************************/
// Structure to hold the pin configuration
typedef const struct {
  uint8_t port;    // GPIO port
  uint8_t pin;     // GPIO pin
  uint8_t mode;    // GPIO mode
  uint8_t pad_sel; // GPIO pad selection
} I2C_PIN_;

// Enum for different transmission scenarios
typedef enum {
  I2C_SEND_DATA,              // Send mode
  I2C_RECEIVE_DATA,           // Receive mode
  I2C_TRANSMISSION_COMPLETED, // Transmission completed mode
} i2c_action_enum_t;

/*******************************************************************************
 *************************** LOCAL VARIABLES   *******************************
 ******************************************************************************/
static I2C_PIN scl =
{ RTE_I2C2_SCL_PORT, RTE_I2C2_SCL_PIN, RTE_I2C2_SCL_MUX, 0 };
static I2C_PIN sda =
{ RTE_I2C2_SDA_PORT, RTE_I2C2_SDA_PIN, RTE_I2C2_SDA_MUX, 0 };
volatile uint8_t i2c_send_complete = 0;
volatile uint8_t i2c_receive_complete = 0;

static uint32_t write_number = 0;
static uint32_t write_count = 0;
static uint32_t read_number = 0;
static uint32_t read_count = 0;
static uint8_t *write_data;
static uint8_t *read_data;
sl_i2c_init_params_t config;

static i2c_action_enum_t current_mode = I2C_SEND_DATA;
volatile uint8_t i2c_read_buffer[I2C_BUFFER_SIZE];
static uint8_t i2c_write_buffer[I2C_BUFFER_SIZE];

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static void pin_configurations(void);
static void i2c_send_data(const uint8_t *data,
                          uint32_t data_length,
                          uint16_t follower_address);
static void i2c_receive_data(uint8_t *data,
                             uint32_t data_length,
                             uint16_t follower_address);
static void i2c_clock_init(I2C_TypeDef *i2c, sl_i2c_init_params_t *config);
static void handle_leader_transmit_irq(void);
static void handle_leader_receive_irq(void);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
/*******************************************************************************
 * I2C example initialization function
 ******************************************************************************/
void i2c_leader_interrupt_init(void)
{
  // Filling the structure with default values.
  config.clhr = SL_I2C_FAST_PLUS_BUS_SPEED;   // Update this value to choose desired I2C bus speed.
  config.mode = SL_I2C_LEADER_MODE;           // Update this value to change between Leader and Follower mode
  // For aborting, I2C instance should be enabled.
  sl_si91x_i2c_enable(I2C_USED);
  // It aborts if any existing activity is there.
  sl_si91x_i2c_abort_transfer(I2C_USED);
  sl_si91x_i2c_disable(I2C_USED);
  // Initializing I2C clock
  i2c_clock_init(I2C_USED, &config);
  NVIC_SetPriority(I2C2_IRQn, 15);
  // Passing the structure and i2c instance for the initialization.
  sl_si91x_i2c_init(I2C_USED, &config);
  // Pin is configured here.
  pin_configurations();
  // Generating a buffer with values that needs to be sent.
  for (uint32_t loop = INITIAL_VALUE; loop < I2C_BUFFER_SIZE; loop++) {
    i2c_write_buffer[loop] = (uint8_t)(loop + BUFFER_OFFSET);
  }
}

/*******************************************************************************
 * Function to provide 1 ms Delay
 *******************************************************************************/
void Delay(uint32_t idelay)
{
  for (uint32_t x = 0; x < 4600 * idelay; x++) // 1.002ms delay
  {
    __NOP();
  }
}

/*******************************************************************************
 * Function will run continuously in while loop
 ******************************************************************************/
void i2c_leader_interrupt_process_action(void)
{
  // In switch case, according to the current mode, the transmission is
  // executed.
  // First leader sends data to follower, then leader receives same data from follower, using I2C transfer API.
  switch (current_mode) {
    case I2C_SEND_DATA:
      sl_si91x_i2c_set_follower_address(I2C_USED, FOLLOWER_I2C_ADDR, 0);
      i2c_send_data(i2c_write_buffer, I2C_BUFFER_SIZE, FOLLOWER_I2C_ADDR);
      while (!i2c_send_complete) {
      }
      // It waits till i2c_send_complete is true in IRQ handler.
      DEBUGOUT("Data is transferred to Follower successfully \n");
      current_mode = I2C_RECEIVE_DATA;
      break;
    case I2C_RECEIVE_DATA:
      // Validation for executing the API only once.
      i2c_receive_data((uint8_t *)i2c_read_buffer,
                       I2C_BUFFER_SIZE,
                       FOLLOWER_I2C_ADDR);
      Delay(10);
      while (!i2c_receive_complete) {
      }
      // It waits till i2c_receive_complete is true in IRQ handler.
      current_mode = I2C_TRANSMISSION_COMPLETED;
      break;
    case I2C_TRANSMISSION_COMPLETED:
    // I2C will be Idle in this mode
    default:
      break;
  }
}

/*******************************************************************************
 * Function to send the data using I2C.
 * Here the FIFO threshold, direction and interrupts are configured.
 *
 * @param[in] data (uint8_t) Constant pointer to the data which needs to be transferred.
 * @param[in] data_length (uint32_t) Length of the data that needs to be received.
 * @return none
 ******************************************************************************/
static void i2c_send_data(const uint8_t *data,
                          uint32_t data_length,
                          uint16_t follower_address)
{
  bool is_10bit_addr = false;
  // Disables the interrupts.
  sl_si91x_i2c_disable_interrupts(I2C_USED, ZERO_FLAG);
  // Updates the variables which are required for trasmission.
  write_data = (uint8_t *)data;
  write_count = 0;
  write_number = data_length;
  // Disables the I2C peripheral.
  sl_si91x_i2c_disable(I2C_USED);
  // Checking is address is 7-bit or 10bit
  if (follower_address > MAX_7BIT_ADDRESS) {
    is_10bit_addr = true;
  }
  // Setting the follower address recevied in parameter structure.
  sl_si91x_i2c_set_follower_address(I2C_USED, follower_address, is_10bit_addr);
  // Configures the FIFO threshold.
  sl_si91x_i2c_set_tx_threshold(I2C_USED, FIFO_THRESHOLD);
  // Enables the I2C peripheral.
  sl_si91x_i2c_enable(I2C_USED);
  // Sets the direction to write.
  // Configures the transmit empty interrupt.
  sl_si91x_i2c_set_interrupts(I2C_USED, SL_I2C_EVENT_TRANSMIT_EMPTY);
  // Enables the interrupt.
  sl_si91x_i2c_enable_interrupts(I2C_USED, ZERO_FLAG);
}

/*******************************************************************************
 * Function to receive the data using I2C.
 * Here the FIFO threshold, direction and interrupts are configured.
 *
 * @param[in] data (uint8_t) Constant pointer to the data which needs to be transferred.
 * @param[in] data_length (uint32_t) Length of the data that needs to be received.
 * @return none
 ******************************************************************************/
static void i2c_receive_data(uint8_t *data,
                             uint32_t data_length,
                             uint16_t follower_address)
{
  bool is_10bit_addr = false;
  // Disables the interrupts.
  sl_si91x_i2c_disable_interrupts(I2C_USED, ZERO_FLAG);
  // Updates the variables which are required for trasmission.
  read_data = (uint8_t *)data;
  read_count = 0;
  read_number = data_length;
  // Disables the I2C peripheral.
  sl_si91x_i2c_disable(I2C_USED);
  // Configures the FIFO threshold.
  // Checking is address is 7-bit or 10bit
  if (follower_address > MAX_7BIT_ADDRESS) {
    is_10bit_addr = true;
  }
  // Setting the follower address recevied in parameter structure.
  sl_si91x_i2c_set_follower_address(I2C_USED, follower_address, is_10bit_addr);
  // Configures the FIFO threshold.
  sl_si91x_i2c_set_rx_threshold(I2C_USED, FIFO_THRESHOLD);
  // Enables the I2C peripheral.
  sl_si91x_i2c_enable(I2C_USED);
  // Sets the direction to read.
  sl_si91x_i2c_control_direction(I2C_USED, SL_I2C_READ_MASK);
  // Configures the receive full interrupt.
  sl_si91x_i2c_set_interrupts(I2C_USED, SL_I2C_EVENT_RECEIVE_FULL);
  // Enables the interrupt.
  sl_si91x_i2c_enable_interrupts(I2C_USED, ZERO_FLAG);
}

/*******************************************************************************
 * To configure the clock and power up the peripheral according to the
 * I2C instance.
 *
 * @param none
 * @return none
 ******************************************************************************/
static void i2c_clock_init(I2C_TypeDef *i2c, sl_i2c_init_params_t *config)
{
  if ((uint32_t)i2c == I2C0_BASE) {
#if defined(SLI_SI917) || defined(SLI_SI915)
    // Powering up the peripheral.
    RSI_PS_M4ssPeriPowerUp(M4SS_PWRGATE_ULP_EFUSE_PERI);
#else
    // Powering up the peripheral.
    RSI_PS_M4ssPeriPowerUp(M4SS_PWRGATE_ULP_PERI1);
#endif
    // Initialize the I2C clock.
    RSI_CLK_I2CClkConfig(M4CLK, true, I2C1_INSTAN);
  } else if ((uint32_t)i2c == I2C1_BASE) {
#if defined(SLI_SI917) || defined(SLI_SI915)
    // Powering up the peripheral.
    RSI_PS_M4ssPeriPowerUp(M4SS_PWRGATE_ULP_EFUSE_PERI);
#else
    // Powering up the peripheral.
    RSI_PS_M4ssPeriPowerUp(M4SS_PWRGATE_ULP_PERI3);
#endif
    // Initialize the I2C clock.
    RSI_CLK_I2CClkConfig(M4CLK, true, I2C2_INSTAN);
  } else if ((uint32_t)i2c == I2C2_BASE) {
    // Powering up the peripheral.
    RSI_PS_UlpssPeriPowerUp(ULPSS_PWRGATE_ULP_I2C);
    // Enabling I2C clock.
    RSI_ULPSS_PeripheralEnable(ULPCLK, ULP_I2C_CLK, ENABLE_STATIC_CLK);
  }

  if ((config->clhr == SL_I2C_FAST_PLUS_BUS_SPEED)
      || (config->clhr == SL_I2C_HIGH_BUS_SPEED)) {
    if ((uint32_t)i2c == I2C2_BASE) {
      // Changing ULP Pro clock to SoC CLK for ULP I2C instance (I2C2) to run in FastPlus and HP modes
      RSI_ULPSS_ClockConfig(M4CLK, ENABLE, 0, 0);
      RSI_ULPSS_UlpProcClkConfig(ULPCLK, ULP_PROC_SOC_CLK, 0, 0);
    }
  }
  // Read the current M4 Core clock
  if (((uint32_t)i2c == I2C2_BASE)
      && ((config->clhr == SL_I2C_STANDARD_BUS_SPEED)
          || (config->clhr == SL_I2C_FAST_BUS_SPEED))) {
    config->freq = system_clocks.ulpss_ref_clk;
  } else {
    sl_si91x_clock_manager_m4_get_core_clk_src_freq(&(config->freq));
  }
}

/*******************************************************************************
 * Function to set the Pin configuration for I2C.
 * It configures the SDA and SCL pins.
 *
 * @param none
 * @return none
 ******************************************************************************/
static void pin_configurations(void)
{
  // SCL
  RSI_EGPIO_UlpPadReceiverEnable(scl.pin);
  RSI_EGPIO_SetPinMux(EGPIO1, scl.port, scl.pin, scl.mode);
  // SDA
  RSI_EGPIO_UlpPadReceiverEnable(sda.pin);
  RSI_EGPIO_SetPinMux(EGPIO1, sda.port, sda.pin, sda.mode);
}

/*******************************************************************************
 * Function to handle the transmit IRQ.
 * Transmit empty interrupt is monitored and byte by byte data is transmitted.
 * If the data transmission is completed, it clears and disables the interrupt.
 *
 * @param none
 * @return none
 ******************************************************************************/
static void handle_leader_transmit_irq(void)
{
  if (write_number > LAST_DATA_COUNT) {
    if (write_number == DATA_COUNT) {
      I2C_USED->IC_DATA_CMD = (uint32_t)write_data[write_count]
                              | (BIT_SET << STOP_BIT);
    } else {
      sl_si91x_i2c_tx(I2C_USED, write_data[write_count]);
    }
    write_count++;
    write_number--;
  } else {
    sl_si91x_i2c_clear_interrupts(I2C_USED, SL_I2C_EVENT_TRANSMIT_EMPTY);
    sl_si91x_i2c_disable_interrupts(I2C_USED, ZERO_FLAG);
    i2c_send_complete = 1;
  }
}

/*******************************************************************************
 * Function to handle the receive IRQ.
 * Receive full interrupt is monitored and byte by byte data is received.
 * If the data receive is completed, it clears and disables the interrupt.
 *
 * @param none
 * @return none
 ******************************************************************************/
static void handle_leader_receive_irq(void)
{
  if (read_number > LAST_DATA_COUNT) {
    read_data[read_count] = I2C_USED->IC_DATA_CMD_b.DAT;
    read_count++;
    read_number--;
    if (read_number == DATA_COUNT) {
      // If the last byte is there to receive, and in leader mode, it needs to send
      // the stop byte.
      I2C_USED->IC_DATA_CMD = (BIT_SET << RW_MASK_BIT) | (BIT_SET << STOP_BIT);
    }
    if (read_number > DATA_COUNT) {
      I2C_USED->IC_DATA_CMD = (BIT_SET << RW_MASK_BIT);
    }
  }
  if (read_number == LAST_DATA_COUNT) {
    sl_si91x_i2c_clear_interrupts(I2C_USED, SL_I2C_EVENT_RECEIVE_FULL);
    sl_si91x_i2c_disable_interrupts(I2C_USED, ZERO_FLAG);
    i2c_receive_complete = 1;
  }
}

/*******************************************************************************
 * IRQ handler for I2C2 (I2C_USED).
 ******************************************************************************/
void I2C2_IRQHandler(void)
{
  uint32_t status = 0;
  status = I2C_USED->IC_INTR_STAT;
  if (status & SL_I2C_EVENT_TRANSMIT_EMPTY) {
    handle_leader_transmit_irq();
  }
  if (status & SL_I2C_EVENT_RECEIVE_FULL) {
    handle_leader_receive_irq();
  }
}
