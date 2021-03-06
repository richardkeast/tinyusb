/**************************************************************************/
/*!
    @file     mouse_app.c
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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "mouse_app.h"

#if TUSB_CFG_OS != TUSB_OS_NONE
#include "app_os_prio.h"
#endif


#if TUSB_CFG_HOST_HID_MOUSE

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_MOUSE_REPORT_DEPTH   4

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(mouse_task_def, "mouse app", mouse_app_task, 128, MOUSE_APP_TASK_PRIO);
OSAL_QUEUE_DEF(queue_mouse_def, QUEUE_MOUSE_REPORT_DEPTH, tusb_mouse_report_t);

static osal_queue_handle_t queue_mouse_hdl;
static tusb_mouse_report_t usb_mouse_report TUSB_CFG_ATTR_USBRAM;

static inline void process_mouse_report(tusb_mouse_report_t const * p_report);

//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
void tusbh_hid_mouse_mounted_cb(uint8_t dev_addr)
{
  // application set-up

  printf("a mouse device is mounted\n");

  osal_queue_flush(queue_mouse_hdl);
  tusbh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report); // first report
}

void tusbh_hid_mouse_unmounted_isr(uint8_t dev_addr)
{
  // application tear-down
}

void tusbh_hid_mouse_isr(uint8_t dev_addr, tusb_event_t event)
{
  switch(event)
  {
    case TUSB_EVENT_XFER_COMPLETE:
      osal_queue_send(queue_mouse_hdl, &usb_mouse_report);
      tusbh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report);
    break;

    case TUSB_EVENT_XFER_ERROR:
      tusbh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report); // ignore & continue
    break;

    default :
    break;
  }
}

//--------------------------------------------------------------------+
// APPLICATION
// NOTICE: MOUSE REPORT IS NOT CORRECT UNTIL A DECENT HID PARSER IS
// IMPLEMENTED, MEANWHILE IT CAN MISS DISPLAY BUTTONS OR X,Y etc
//--------------------------------------------------------------------+
void mouse_app_init(void)
{
  memclr_(&usb_mouse_report, sizeof(tusb_mouse_report_t));

  ASSERT( TUSB_ERROR_NONE == osal_task_create(&mouse_task_def), (void) 0 );

  queue_mouse_hdl = osal_queue_create(&queue_mouse_def);
  ASSERT_PTR( queue_mouse_hdl, (void) 0 );
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( mouse_app_task ) (void* p_task_para)
{
  tusb_error_t error;
  tusb_mouse_report_t mouse_report;

  OSAL_TASK_LOOP_BEGIN

  osal_queue_receive(queue_mouse_hdl, &mouse_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  process_mouse_report(&mouse_report);

  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
static inline void process_mouse_report(tusb_mouse_report_t const * p_report)
{
  static tusb_mouse_report_t prev_report = { 0 };

  //------------- button state  -------------//
  uint8_t button_changed_mask = p_report->buttons ^ prev_report.buttons;
  if ( button_changed_mask & p_report->buttons)
  {
     // example only display button pressed, ignore hold & dragging etc
    printf(" %c%c%c ",
       p_report->buttons & HID_MOUSEBUTTON_LEFT   ? 'L' : '-',
       p_report->buttons & HID_MOUSEBUTTON_MIDDLE ? 'M' : '-',
       p_report->buttons & HID_MOUSEBUTTON_RIGHT  ? 'R' : '-');
  }

  //------------- movement (disabled for clearer demo) -------------//
//  if ( p_report->x != 0 || p_report->y != 0 )
//  {
//    printf(" (%d, %d) ", p_report->x, p_report->y);
//  }

}


#endif
