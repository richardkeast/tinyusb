/**************************************************************************/
/*!
    @file     errors.h
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \file
 *  \brief Error Header
 *
 *  \note TBD
 */

/** \ingroup Group_Common
 *  \defgroup Group_Error Error Codes
 *  @{
 */

#ifndef _TUSB_ERRORS_H_
#define _TUSB_ERRORS_H_

#include "tusb_option.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define ERROR_ENUM(x) x,
#define ERROR_STRING(x) #x,

#define ERROR_TABLE(ENTRY) \
    ENTRY(TUSB_ERROR_NONE                            )\
    ENTRY(TUSB_ERROR_INVALID_PARA                    )\
    ENTRY(TUSB_ERROR_DEVICE_NOT_READY                )\
    ENTRY(TUSB_ERROR_INTERFACE_IS_BUSY               )\
    ENTRY(TUSB_ERROR_HCD_FAILED                      )\
    ENTRY(TUSB_ERROR_HCD_OPEN_PIPE_FAILED            )\
    ENTRY(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND   )\
    ENTRY(TUSB_ERROR_USBH_MOUNT_CONFIG_DESC_TOO_LONG )\
    ENTRY(TUSB_ERROR_OSAL_TIMEOUT                    )\
    ENTRY(TUSB_ERROR_OSAL_WAITING                    ) /* only used by OSAL_NONE in the subtask */ \
    ENTRY(TUSB_ERROR_OSAL_TASK_FAILED                )\
    ENTRY(TUSB_ERROR_OSAL_TASK_CREATE_FAILED         )\
    ENTRY(TUSB_ERROR_OSAL_QUEUE_FAILED               )\
    ENTRY(TUSB_ERROR_OSAL_SEMAPHORE_FAILED           )\
    ENTRY(TUSB_ERROR_OSAL_MUTEX_FAILED               )\
    ENTRY(TUSB_ERROR_EHCI_NOT_ENOUGH_QTD             )\
    ENTRY(TUSB_ERROR_USBD_DESCRIPTOR_STRING          )\
    ENTRY(TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE       )\
    ENTRY(TUSB_ERROR_HIDH_NOT_SUPPORTED_PROTOCOL     )\
    ENTRY(TUSB_ERROR_HIDH_NOT_SUPPORTED_SUBCLASS     )\
    ENTRY(TUSB_ERROR_CDCH_DESCRIPTOR_CORRUPTED       )\
    ENTRY(TUSB_ERROR_CDCH_UNSUPPORTED_SUBCLASS       )\
    ENTRY(TUSB_ERROR_CDCH_UNSUPPORTED_PROTOCOL       )\
    ENTRY(TUSB_ERROR_CDCH_DEVICE_NOT_MOUNTED         )\
    ENTRY(TUSB_ERROR_NOT_SUPPORTED_YET               )\
    ENTRY(TUSB_ERROR_FAILED                          )\


/** \enum tusb_error_t
 *  \brief Error Code returned
 */
typedef enum {
  ERROR_TABLE(ERROR_ENUM)
  TUSB_ERROR_COUNT
}tusb_error_t;

#if TUSB_CFG_DEBUG == 3
/// Enum to String for debugging purposes. Only available if \ref TUSB_CFG_DEBUG > 0
extern char const* const TUSB_ErrorStr[];
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_ERRORS_H_ */

 /**  @} */
