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

#ifndef __VSFUSBH_USBLYZER_H_INCLUDED__
#define __VSFUSBH_USBLYZER_H_INCLUDED__

// VSFHAL_USBD_ON_STALL is not included in vsfhal.h
#define VSFHAL_USBD_ON_STALL			VSFHAL_USBD_ON_USER

extern const struct vsfusbh_class_drv_t vsfusbh_usblyzer_drv;
void usblyzer_init(const struct vsfhal_usbd_t *drv, int32_t int_priority,
		void *param,
		void (*on_event)(void*, enum vsfhal_usbd_evt_t, uint32_t, uint8_t*, uint32_t));

#endif // __VSFUSBH_USBLYZER_H_INCLUDED__
