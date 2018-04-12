/***************************************************************************
 *   Copyright (C) 2009 - 2010 by Simon Qian <SimonQian@SimonQian.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef __VSFHAL_CONST_H_INCLUDED__
#define __VSFHAL_CONST_H_INCLUDED__

#include "gd32f1x0.h"

// common
#define VSFHAL_DUMMY_PORT					0xFF

// core
#define VSFHAL_SLEEP_WFI					(0x1ul << 0)
#define VSFHAL_SLEEP_PWRDOWN				(0x1ul << 1)

// GPIO
#define VSFHAL_GPIO_INPUT					0
#define VSFHAL_GPIO_OUTPP					0x01
#define VSFHAL_GPIO_OUTOD					0x05
#define VSFHAL_GPIO_PULLUP					0x08
#define VSFHAL_GPIO_PULLDOWN				0x10
// gd32f1x0 private
#define gd32f1x0_GPIO_AF					0x02
#define gd32f1x0_GPIO_AN					0x03

// PWM
#define VSFHAL_PWM_ENABLE					0x01
#define VSFHAL_PWM_POLARITY_HIGH			0x02
#define VSFHAL_PWM_POLARITY_LOW				0x00

// USBD
#define VSFHAL_HAS_USBD

#define vsfhal_usart_t						uint8_t
#define vsfhal_i2c_t						uint8_t

#if 0
#define gd32f1x0_GPIO_OD				0x04

#define gd32f1x0_EINT_ONFALL			0x01
#define gd32f1x0_EINT_ONRISE			0x02
#define gd32f1x0_EINT_ONLEVEL			0x80
#define gd32f1x0_EINT_ONLOW			(gd32f1x0_EINT_ONLEVEL | 0x00)
#define gd32f1x0_EINT_ONHIGH			(gd32f1x0_EINT_ONLEVEL | 0x10)

#define gd32f1x0_SDIO_RESP_NONE		0x00
#define gd32f1x0_SDIO_RESP_SHORT		0x40
#define gd32f1x0_SDIO_RESP_LONG		0xC0
#endif

#endif	// __VSFHAL_CONST_H_INCLUDED__
