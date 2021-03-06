/**************************************************************************/
/*!
    @file     usbd_host.c
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

#include "common/common.h"

#if MODE_HOST_SUPPORTED

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "tusb.h"
#include "usbh_hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define ENUM_QUEUE_DEPTH  5

// TODO fix/compress number of class driver
static host_class_driver_t const usbh_class_drivers[TUSB_CLASS_MAPPED_INDEX_END] =
{
#if HOST_CLASS_HID
    [TUSB_CLASS_HID] = {
        .init         = hidh_init,
        .open_subtask = hidh_open_subtask,
        .isr          = hidh_isr,
        .close        = hidh_close
    },
#endif

#if TUSB_CFG_HOST_CDC
    [TUSB_CLASS_CDC] = {
        .init         = cdch_init,
        .open_subtask = cdch_open_subtask,
        .isr          = cdch_isr,
        .close        = cdch_close
    },
#endif

#if TUSB_CFG_HOST_MSC
    [TUSB_CLASS_MSC] = {
        .init         = msch_init,
        .open_subtask = msch_open_subtask,
        .isr          = msch_isr,
        .close        = msch_close
    }
#endif

#if TUSB_CFG_HOST_HUB
    [TUSB_CLASS_HUB] = {
        .init         = hub_init,
        .open_subtask = hub_open_subtask,
        .isr          = hub_isr,
        .close        = hub_close
    }
#endif

#if TUSB_CFG_HOST_CUSTOM_CLASS
    [TUSB_CLASS_MAPPED_INDEX_END-1] = {
        .init         = cush_init,
        .open_subtask = cush_open_subtask,
        .isr          = cush_isr,
        .close        = cush_close
    }
#endif
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1] TUSB_CFG_ATTR_USBRAM; // including zero-address

//------------- Enumeration Task Data -------------//
OSAL_TASK_DEF(enum_task_def, "tinyusb host", usbh_enumeration_task, 150, TUSB_CFG_OS_TASK_PRIO);
OSAL_QUEUE_DEF(enum_queue_def, ENUM_QUEUE_DEPTH, uint32_t);
osal_queue_handle_t enum_queue_hdl;
STATIC_ uint8_t enum_data_buffer[TUSB_CFG_HOST_ENUM_BUFFER_SIZE] TUSB_CFG_ATTR_USBRAM;

//------------- Reporter Task Data -------------//

//------------- Helper Function Prototypes -------------//
static inline uint8_t get_new_address(void) ATTR_ALWAYS_INLINE;
static inline uint8_t get_configure_number_for_device(tusb_descriptor_device_t* dev_desc) ATTR_ALWAYS_INLINE;

static inline uint8_t std_class_code_to_index(uint8_t std_class_code) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t std_class_code_to_index(uint8_t std_class_code)
{
  return  (std_class_code <= TUSB_CLASS_AUDIO_VIDEO          ) ? std_class_code                   :
          (std_class_code == TUSB_CLASS_DIAGNOSTIC           ) ? TUSB_CLASS_MAPPED_INDEX_START     :
          (std_class_code == TUSB_CLASS_WIRELESS_CONTROLLER  ) ? TUSB_CLASS_MAPPED_INDEX_START + 1 :
          (std_class_code == TUSB_CLASS_MISC                 ) ? TUSB_CLASS_MAPPED_INDEX_START + 2 :
          (std_class_code == TUSB_CLASS_APPLICATION_SPECIFIC ) ? TUSB_CLASS_MAPPED_INDEX_START + 3 :
          (std_class_code == TUSB_CLASS_VENDOR_SPECIFIC      ) ? TUSB_CLASS_MAPPED_INDEX_START + 4 : 0;
}

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
tusb_device_state_t tusbh_device_get_state (uint8_t const dev_addr)
{
  ASSERT_INT_WITHIN(1, TUSB_CFG_HOST_DEVICE_MAX, dev_addr, TUSB_DEVICE_STATE_INVALID_PARAMETER);
  return usbh_devices[dev_addr].state;
}

uint32_t tusbh_device_get_mounted_class_flag(uint8_t dev_addr)
{
  return tusbh_device_is_configured(dev_addr) ? usbh_devices[dev_addr].flag_supported_class : 0;
}

//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+
tusb_error_t usbh_init(void)
{
  memclr_(usbh_devices, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  ASSERT_STATUS( hcd_init() );

  //------------- Enumeration & Reporter Task init -------------//
  ASSERT_STATUS( osal_task_create(&enum_task_def) );
  enum_queue_hdl = osal_queue_create(&enum_queue_def);
  ASSERT_PTR(enum_queue_hdl, TUSB_ERROR_OSAL_QUEUE_FAILED);

  //------------- Semaphore, Mutex for Control Pipe -------------//
  for(uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX+1; i++) // including address zero
  {
    usbh_device_info_t * const p_device = &usbh_devices[i];

    p_device->control.sem_hdl = osal_semaphore_create( OSAL_SEM_REF(p_device->control.semaphore) );
    ASSERT_PTR(p_device->control.sem_hdl, TUSB_ERROR_OSAL_SEMAPHORE_FAILED);

    p_device->control.mutex_hdl = osal_mutex_create ( OSAL_SEM_REF(p_device->control.mutex) );
    ASSERT_PTR(p_device->control.mutex_hdl, TUSB_ERROR_OSAL_MUTEX_FAILED);
  }

  //------------- class init -------------//
  for (uint8_t class_index = 1; class_index < TUSB_CLASS_MAPPED_INDEX_END; class_index++)
  {
    if (usbh_class_drivers[class_index].init)
    {
      usbh_class_drivers[class_index].init();
    }
  }

  return TUSB_ERROR_NONE;
}

//------------- USBH control transfer -------------//
// function called within a task, requesting os blocking services, subtask input parameter must be static/global variables or constant
tusb_error_t usbh_control_xfer_subtask(uint8_t dev_addr, uint8_t bmRequestType, uint8_t bRequest,
                                       uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data )
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  osal_mutex_wait(usbh_devices[dev_addr].control.mutex_hdl, OSAL_TIMEOUT_NORMAL, &error);
  SUBTASK_ASSERT_STATUS_WITH_HANDLER(error, osal_mutex_release(usbh_devices[dev_addr].control.mutex_hdl));

  usbh_devices[dev_addr].control.request = (tusb_control_request_t) {
                                                  {.bmRequestType = bmRequestType},
                                                  .bRequest      = bRequest,
                                                  .wValue        = wValue,
                                                  .wIndex        = wIndex,
                                                  .wLength       = wLength
                                           };

#ifndef _TEST_
  usbh_devices[dev_addr].control.pipe_status = 0;
#else
  usbh_devices[dev_addr].control.pipe_status = TUSB_EVENT_XFER_COMPLETE; // in Test project, mark as complete to imm pass
#endif
  /*SUBTASK_ASSERT_STATUS*/ (void) ( hcd_pipe_control_xfer(dev_addr, &usbh_devices[dev_addr].control.request, data) );

  osal_semaphore_wait(usbh_devices[dev_addr].control.sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static

  osal_mutex_release(usbh_devices[dev_addr].control.mutex_hdl);

  // TODO make handler for this function general purpose
  SUBTASK_ASSERT_WITH_HANDLER(TUSB_ERROR_NONE == error &&
                              TUSB_EVENT_XFER_COMPLETE == usbh_devices[dev_addr].control.pipe_status,
                              tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  OSAL_SUBTASK_END
}

tusb_error_t usbh_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size) ATTR_ALWAYS_INLINE;
tusb_error_t usbh_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  osal_semaphore_reset( usbh_devices[dev_addr].control.sem_hdl );
  osal_mutex_reset( usbh_devices[dev_addr].control.mutex_hdl );

  ASSERT_STATUS( hcd_pipe_control_open(dev_addr, max_packet_size) );

  return TUSB_ERROR_NONE;
}

static inline tusb_error_t usbh_pipe_control_close(uint8_t dev_addr) ATTR_ALWAYS_INLINE;
static inline tusb_error_t usbh_pipe_control_close(uint8_t dev_addr)
{
  ASSERT_STATUS( hcd_pipe_control_close(dev_addr) );

  return TUSB_ERROR_NONE;
}

tusb_interface_status_t usbh_pipe_status_get(pipe_handle_t pipe_hdl)
{
  return TUSB_INTERFACE_STATUS_BUSY;
}

//--------------------------------------------------------------------+
// USBH-HCD ISR/Callback API
//--------------------------------------------------------------------+
// interrupt caused by a TD (with IOC=1) in pipe of class class_code
void usbh_xfer_isr(pipe_handle_t pipe_hdl, uint8_t class_code, tusb_event_t event, uint32_t xferred_bytes)
{
  uint8_t class_index = std_class_code_to_index(class_code);
  if (TUSB_XFER_CONTROL == pipe_hdl.xfer_type)
  {
    usbh_devices[ pipe_hdl.dev_addr ].control.pipe_status = event;
    osal_semaphore_post( usbh_devices[ pipe_hdl.dev_addr ].control.sem_hdl );
  }else if (usbh_class_drivers[class_index].isr)
  {
    usbh_class_drivers[class_index].isr(pipe_hdl, event, xferred_bytes);
  }else
  {
    ASSERT(false, (void) 0); // something wrong, no one claims the isr's source
  }
}

void usbh_device_plugged_isr(uint8_t hostid)
{
  osal_queue_send(enum_queue_hdl,
                  &(usbh_enumerate_t){ .core_id = hostid} );
}

void usbh_device_unplugged_isr(uint8_t hostid)
{
  //------------- find the device address that is unplugged -------------//
  uint8_t dev_addr = 0;
  while ( dev_addr <= TUSB_CFG_HOST_DEVICE_MAX &&
          !(usbh_devices[dev_addr].core_id  == hostid &&
            usbh_devices[dev_addr].hub_addr == 0 &&
            usbh_devices[dev_addr].hub_port == 0 &&
            usbh_devices[dev_addr].state    != TUSB_DEVICE_STATE_UNPLUG ) )
  {
    dev_addr++;
  }

  if (dev_addr > TUSB_CFG_HOST_DEVICE_MAX) // unplug unmounted device
    return;

  if (dev_addr > 0) // device can still be unplugged when not set new address
  {
    // if device unplugged is not a hub TODO handle hub unplugged
    for (uint8_t class_index = 1; class_index < TUSB_CLASS_MAPPED_INDEX_END; class_index++)
    {
      if ((usbh_devices[dev_addr].flag_supported_class & BIT_(class_index)) &&
          usbh_class_drivers[class_index].close)
      {
        usbh_class_drivers[class_index].close(dev_addr);
      }
    }
  }

  usbh_pipe_control_close(dev_addr);

  // set to REMOVING to allow HCD to clean up its cached data for this device
  // HCD must set this device's state to TUSB_DEVICE_STATE_UNPLUG when done
  usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_REMOVING;
  usbh_devices[dev_addr].flag_supported_class = 0;
}

//--------------------------------------------------------------------+
// ENUMERATION TASK
//--------------------------------------------------------------------+
static tusb_error_t enumeration_body_subtask(void);

// To enable the TASK_ASSERT style (quick return on false condition) in a real RTOS, a task must act as a wrapper
// and is used mainly to call subtasks. Within a subtask return statement can be called freely, the task with
// forever loop cannot have any return at all.
OSAL_TASK_FUNCTION(usbh_enumeration_task) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  enumeration_body_subtask();

  OSAL_TASK_LOOP_END
}

tusb_error_t enumeration_body_subtask(void)
{
  tusb_error_t error;
  usbh_enumerate_t enum_entry;

  // for OSAL_NONE local variable won't retain value after blocking service sem_wait/queue_recv
  static uint8_t new_addr;
  static uint8_t configure_selected = 1; // TODO move
  static uint8_t *p_desc = NULL; // TODO move

  OSAL_SUBTASK_BEGIN

  osal_queue_receive(enum_queue_hdl, &enum_entry, OSAL_TIMEOUT_WAIT_FOREVER, &error);

  SUBTASK_ASSERT( hcd_port_connect_status(enum_entry.core_id) ); // ensure device is still plugged

  usbh_devices[0].core_id  = enum_entry.core_id; // TODO refractor integrate to device_pool
  usbh_devices[0].hub_addr = enum_entry.hub_addr;
  usbh_devices[0].hub_port = enum_entry.hub_port;

  hcd_port_reset( usbh_devices[0].core_id ); // port must be reset to have correct speed operation
  usbh_devices[0].speed    = hcd_port_speed_get( usbh_devices[0].core_id );

  SUBTASK_ASSERT_STATUS( usbh_pipe_control_open(0, 8) );
  usbh_devices[0].state = TUSB_DEVICE_STATE_ADDRESSED;

#ifndef _TEST_
  // TODO hack delay 20 ms for slow device (use retry on the 1st xfer instead later)
  osal_task_delay(50);
#endif

  //------------- Get first 8 bytes of device descriptor to get Control Endpoint Size -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
      usbh_control_xfer_subtask( 0, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_DEVICE),
                                 TUSB_REQUEST_GET_DESCRIPTOR, (TUSB_DESC_TYPE_DEVICE << 8), 0,
                                 8, enum_data_buffer ),
      error
  );

  SUBTASK_ASSERT_STATUS(error); // TODO some slow device is observed to fail the very fist controler xfer, can try more times

  hcd_port_reset( usbh_devices[0].core_id ); // reset port after 8 byte descriptor

  //------------- Set new address -------------//
  new_addr = get_new_address();
  SUBTASK_ASSERT(new_addr <= TUSB_CFG_HOST_DEVICE_MAX); // TODO notify application we reach max devices

  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( 0, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_DEVICE),
                               TUSB_REQUEST_SET_ADDRESS, new_addr, 0,
                               0, NULL ),
    error
  );
  SUBTASK_ASSERT_STATUS(error);

  //------------- update port info & close control pipe of addr0 -------------//
  usbh_devices[new_addr].core_id  = usbh_devices[0].core_id;
  usbh_devices[new_addr].hub_addr = usbh_devices[0].hub_addr;
  usbh_devices[new_addr].hub_port = usbh_devices[0].hub_port;
  usbh_devices[new_addr].speed    = usbh_devices[0].speed;
  usbh_devices[new_addr].state    = TUSB_DEVICE_STATE_ADDRESSED;

  usbh_pipe_control_close(0);
  usbh_devices[0].state = TUSB_DEVICE_STATE_UNPLUG;

  // open control pipe for new address
  SUBTASK_ASSERT_STATUS ( usbh_pipe_control_open(new_addr, ((tusb_descriptor_device_t*) enum_data_buffer)->bMaxPacketSize0 ) );

  //------------- Get full device descriptor -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
      usbh_control_xfer_subtask( new_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_DEVICE),
                                 TUSB_REQUEST_GET_DESCRIPTOR, (TUSB_DESC_TYPE_DEVICE << 8), 0,
                                 18, enum_data_buffer ),
      error
  );
  SUBTASK_ASSERT_STATUS(error);

  // update device info  TODO alignment issue
  usbh_devices[new_addr].vendor_id       = ((tusb_descriptor_device_t*) enum_data_buffer)->idVendor;
  usbh_devices[new_addr].product_id      = ((tusb_descriptor_device_t*) enum_data_buffer)->idProduct;
  usbh_devices[new_addr].configure_count = ((tusb_descriptor_device_t*) enum_data_buffer)->bNumConfigurations;

  configure_selected = get_configure_number_for_device((tusb_descriptor_device_t*) enum_data_buffer);
  SUBTASK_ASSERT(configure_selected <= usbh_devices[new_addr].configure_count); // TODO notify application when invalid configuration

  //------------- Get 9 bytes of configuration descriptor -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
      usbh_control_xfer_subtask( new_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_DEVICE),
                                 TUSB_REQUEST_GET_DESCRIPTOR, (TUSB_DESC_TYPE_CONFIGURATION << 8) | (configure_selected - 1), 0,
                                 9, enum_data_buffer ),
      error
  );
  SUBTASK_ASSERT_STATUS(error);
  SUBTASK_ASSERT_WITH_HANDLER( TUSB_CFG_HOST_ENUM_BUFFER_SIZE > ((tusb_descriptor_configuration_t*)enum_data_buffer)->wTotalLength,
                            tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_CONFIG_DESC_TOO_LONG, NULL) );

  //------------- Get full configuration descriptor -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
      usbh_control_xfer_subtask( new_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_DEVICE),
                                 TUSB_REQUEST_GET_DESCRIPTOR, (TUSB_DESC_TYPE_CONFIGURATION << 8) | (configure_selected - 1), 0,
                                 ((tusb_descriptor_configuration_t*) enum_data_buffer)->wTotalLength, enum_data_buffer ),
      error
  );
  SUBTASK_ASSERT_STATUS(error);

  // update configuration info
  usbh_devices[new_addr].interface_count = ((tusb_descriptor_configuration_t*) enum_data_buffer)->bNumInterfaces;

  //------------- Set Configure -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( new_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_DEVICE),
                               TUSB_REQUEST_SET_CONFIGURATION, configure_selected, 0,
                               0, NULL ),
    error
  );
  SUBTASK_ASSERT_STATUS(error);

  usbh_devices[new_addr].state = TUSB_DEVICE_STATE_CONFIGURED;

  //------------- TODO Get String Descriptors -------------//

  //------------- parse configuration & install drivers -------------//
  p_desc = enum_data_buffer + sizeof(tusb_descriptor_configuration_t);

  // parse each interfaces
  while( p_desc < enum_data_buffer + ((tusb_descriptor_configuration_t*)enum_data_buffer)->wTotalLength )
  {
    // skip until we see interface descriptor
    if ( TUSB_DESC_TYPE_INTERFACE != p_desc[DESCRIPTOR_OFFSET_TYPE] )
    {
      p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip the descriptor, increase by the descriptor's length
    }else
    {
      static uint8_t class_index; // has to be static as it is used to call class's open_subtask

      class_index = std_class_code_to_index( ((tusb_descriptor_interface_t*) p_desc)->bInterfaceClass );
      SUBTASK_ASSERT( class_index != 0 ); // class_index == 0 means corrupted data, abort enumeration

      if (usbh_class_drivers[class_index].open_subtask)      // supported class
      {
        static uint16_t length;
        length = 0;

        OSAL_SUBTASK_INVOKED_AND_WAIT ( // parameters in task/sub_task must be static storage (static or global)
            usbh_class_drivers[class_index].open_subtask( new_addr, (tusb_descriptor_interface_t*) p_desc, &length ),
            error
        );

        if (error == TUSB_ERROR_NONE)
        {
          SUBTASK_ASSERT( length >= sizeof(tusb_descriptor_interface_t) );
          usbh_devices[new_addr].flag_supported_class |= BIT_(class_index);
          p_desc += length;
        }else  // Interface open failed, for example a subclass is not supported
        {
          p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip this interface, the rest will be skipped by the above loop
        }
      } else // unsupported class (not enable or yet implemented)
      {
        p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip this interface, the rest will be skipped by the above loop
      }
    }
  }

  tusbh_device_mount_succeed_cb(new_addr);

  OSAL_SUBTASK_END
}

//--------------------------------------------------------------------+
// REPORTER TASK & ITS DATA
//--------------------------------------------------------------------+





//--------------------------------------------------------------------+
// INTERNAL HELPER
//--------------------------------------------------------------------+
static inline uint8_t get_new_address(void)
{
  uint8_t addr;
  for (addr=1; addr <= TUSB_CFG_HOST_DEVICE_MAX; addr++)
  {
    if (usbh_devices[addr].state == TUSB_DEVICE_STATE_UNPLUG)
      break;
  }
  return addr;
}

static inline uint8_t get_configure_number_for_device(tusb_descriptor_device_t* dev_desc)
{
  uint8_t config_num = 1;

  // invoke callback to ask user which configuration to select
  if (tusbh_device_attached_cb)
  {
    config_num = min8_of(1, tusbh_device_attached_cb(dev_desc) );
  }

  return config_num;
}

#endif
