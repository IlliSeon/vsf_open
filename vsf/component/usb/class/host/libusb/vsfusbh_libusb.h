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

#ifndef __VSFUSBH_LIBUSB_H_INCLUDED__
#define __VSFUSBH_LIBUSB_H_INCLUDED__

struct vsfusbh_libusb_dev_t
{
	struct vsfusbh_t *usbh;
	struct vsfusbh_device_t *dev;
	uint16_t vid, pid;
	struct vsfhcd_urb_t *urb;
	unsigned opened : 1;
	unsigned removed : 1;
	unsigned refed : 1;
	struct vsflist_t list;
	struct vsflist_t usrlist;
};

enum vsfusbh_libusb_evt_t
{
	VSFUSBH_LIBUSB_EVT_ON_ARRIVED,
	VSFUSBH_LIBUSB_EVT_ON_LEFT,
};

extern const struct vsfusbh_class_drv_t vsfusbh_libusb_drv;

void vsfusbh_libusb_set_evthandler(void *param,
	void (*on_event)(void *param, struct vsfusbh_libusb_dev_t *dev, enum vsfusbh_libusb_evt_t evt));

uint32_t vsfusbh_libusb_enum_begin(void);
struct vsfusbh_libusb_dev_t *vsfusbh_libusb_enum_next(void);
void vsfusbh_libusb_enum_end(void);

void vsfusbh_libusb_ref(struct vsfusbh_libusb_dev_t *libusb);
void vsfusbh_libusb_deref(struct vsfusbh_libusb_dev_t *libusb);

vsf_err_t vsfusbh_libusb_open(struct vsfusbh_libusb_dev_t *libusb);
void vsfusbh_libusb_close(struct vsfusbh_libusb_dev_t *libusb);

#endif
