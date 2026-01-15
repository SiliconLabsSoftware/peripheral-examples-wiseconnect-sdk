/***************************************************************************/ /**
 * @file i2c_leader_interrupt.h
 * @brief I2C Leader examples functions
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
#ifndef I2C_LEADER_INTERRUPT_H_
#define I2C_LEADER_INTERRUPT_H_

// -----------------------------------------------------------------------------
// Prototypes

/***************************************************************************/ /**
 * It is an initialization function, it initializes the clock, pin configuration
 * init_para for I2C communication.
 *
 * @param none
 * @return none
 ******************************************************************************/
void i2c_leader_interrupt_init(void);

/***************************************************************************/ /**
 * The state machine code for send and receive is implemented here.
 * This function is called in while loop.
 *
 * @param none
 * @return none
 ******************************************************************************/
void i2c_leader_interrupt_process_action(void);

#endif /* I2C_LEADER_INTERRUPT_H_ */
