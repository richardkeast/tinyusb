/**************************************************************************/
/*!
    @file     hal.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** 
 *  \defgroup Group_HAL Hardware Abtract Layer
 *  \brief Hardware dependent layer
 *
 *  @{
 */

#ifndef _TUSB_HAL_H_
#define _TUSB_HAL_H_

#include "tusb_option.h"
#include "common/primitive_types.h"
#include "common/errors.h"
#include "common/compiler/compiler.h"

#if MCU == 0
  #error MCU is not defined or supported yet
#elif MCU == MCU_LPC11UXX
  #include "hal_lpc11uxx.h"
#elif MCU == MCU_LPC13UXX
  #include "hal_lpc13uxx.h"
#elif MCU == MCU_LPC43XX
  #include "hal_lpc43xx.h"
#elif MCU == MCU_LPC175X_6X
  #include "hal_lpc175x_6x.h"
#else
  #error MCU is not defined or supported yet
#endif

#ifdef __cplusplus
extern "C" {
#endif

// callback from tusb.h
extern void tusb_isr(uint8_t controller_id);

/// USB hardware init
tusb_error_t hal_init(void);

/// Enable USB Interrupt
static inline void hal_interrupt_enable(uint8_t controller_id) ATTR_ALWAYS_INLINE;
/// Disable USB Interrupt
static inline void hal_interrupt_disable(uint8_t controller_id) ATTR_ALWAYS_INLINE;

static inline bool hal_debugger_is_attached(void) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline bool hal_debugger_is_attached(void)
{
#if !defined(_TEST_) && !(MCU==MCU_LPC11UXX)
  return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == CoreDebug_DHCSR_C_DEBUGEN_Msk;
#else
  return false;
#endif
}

static inline void hal_debugger_breakpoint(void) ATTR_ALWAYS_INLINE;
static inline void hal_debugger_breakpoint(void)
{
#ifndef _TEST_
  if (hal_debugger_is_attached()) /* if there is debugger connected */
  {
    __asm("BKPT #0\n");
  }
#endif
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HAL_H_ */

/** @} */
