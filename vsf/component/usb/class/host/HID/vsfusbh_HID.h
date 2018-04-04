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

#ifndef __VSFUSBH_HID_H_INCLUDED__
#define __VSFUSBH_HID_H_INCLUDED__

struct vsfusbh_hid_t
{
	struct vsfsm_t sm;
	struct vsfsm_pt_t pt;

	struct vsfusbh_t *usbh;
	struct vsfusbh_device_t *dev;

	struct vsfhcd_urb_t *inurb;
	struct vsfhcd_urb_t *outurb;

	struct vsfusbh_ifs_t *ifs;
	uint8_t bInterfaceNumber;
	uint8_t ep_in;
	uint8_t ep_out;
	uint16_t hid_desc_len;
	struct vsfhid_dev_t hid_dev;
};

#ifndef VSFCFG_EXCLUDE_USBH_HID
extern const struct vsfusbh_class_drv_t vsfusbh_hid_drv;
#endif

#endif
