/**************************************************************************/
/*!
    @file     board_ngx4330.c
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

#include "../board.h"

#if BOARD == BOARD_NGX4330

#define BOARD_MAX_LEDS  2
const static struct {
  uint8_t port;
  uint8_t pin;
}leds[BOARD_MAX_LEDS] = { {1, 11}, {1,12} };

void board_init(void)
{
  SystemInit();
  CGU_Init();
	SysTick_Config( CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE)/CFG_TICKS_PER_SECOND );	/* 1 ms Timer */

	//------------- USB Bus power HOST ONLY-------------//
	scu_pinmux(0x1, 7, MD_PUP | MD_EZI, FUNC4);	// P1_7 USB0_PWR_EN, USB0 VBus function Xplorer

	scu_pinmux(0x2, 6, MD_PUP | MD_EZI, FUNC4); // P2_6 is configured as GPIO5[6] for USB1_PWR_EN
	GPIO_SetDir   (5, BIT_(6), 1);              // GPIO5[6] is output
	GPIO_SetValue (5, BIT_(6));                 // GPIO5[6] output high

	// Leds Init
	for (uint8_t i=0; i<BOARD_MAX_LEDS; i++)
	{
	  scu_pinmux(leds[i].port, leds[i].pin, MD_PUP|MD_EZI|MD_ZI, FUNC0);
	  GPIO_SetDir(leds[i].port, BIT_(leds[i].pin), 1); // output
	}

#if CFG_UART_ENABLE
	//------------- UART init -------------//
	UART_CFG_Type UARTConfigStruct;

	scu_pinmux(0x6 ,4, MD_PDN|MD_EZI, FUNC2); 	// UART0_TXD
	scu_pinmux(0x6 ,5, MD_PDN|MD_EZI, FUNC2); 	// UART0_RXD

	UART_ConfigStructInit(&UARTConfigStruct);                   // default: baud = 9600, 8 bit data, 1 stop bit, no parity
	UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;             // Re-configure baudrate

	UART_Init((LPC_USARTn_Type*) LPC_USART0, &UARTConfigStruct); // Initialize UART port
	UART_TxCmd((LPC_USARTn_Type*) LPC_USART0, ENABLE);           // Enable UART
#endif

}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  for (uint32_t i=0; i<BOARD_MAX_LEDS; i++)
  {
    if ( on_mask & BIT_(i))
    {
      GPIO_SetValue(leds[i].port, BIT_(leds[i].pin));
    }else if ( off_mask & BIT_(i)) // on_mask take precedence over off_mask
    {
      GPIO_ClearValue(leds[i].port, BIT_(leds[i].pin));
    }
  }
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
uint32_t board_uart_send(uint8_t *p_buffer, uint32_t length)
{
  return UART_Send((LPC_USARTn_Type*) LPC_USART0, p_buffer, length, BLOCKING);
}

uint32_t board_uart_recv(uint8_t *p_buffer, uint32_t length)
{
  return UART_Receive((LPC_USARTn_Type*) LPC_USART0, p_buffer, length, BLOCKING);
}

#endif
