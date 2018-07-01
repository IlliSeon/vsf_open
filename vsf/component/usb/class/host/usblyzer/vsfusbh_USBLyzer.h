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

extern const struct vsfusbh_class_drv_t vsfusbh_usblyzer_drv;

void usblyzer_init(const struct vsfhal_usbd_t *drv, int32_t int_priority);

struct usblyzer_plugin_op_t
{
	void (*parse_config)(uint8_t *data, uint16_t len);
	void (*on_SETUP)(struct usb_ctrlrequest_t *request, int16_t urb_status, uint8_t *data, uint16_t len);
	void (*on_IN)(uint8_t ep, int16_t urb_status, uint8_t *data, uint16_t len);
	void (*on_OUT)(uint8_t ep, int16_t urb_status, uint8_t *data, uint16_t len);
};

struct usblyzer_plugin_t
{
	const struct usblyzer_plugin_op_t *op;
	struct usblyzer_plugin_t *next;
};

extern struct usblyzer_plugin_t usblyzer_plugin_stdreq;
extern struct usblyzer_plugin_t usblyzer_plugin_hid;
extern struct usblyzer_plugin_t usblyzer_plugin_msc;
void usblyzer_register_plugin(struct usblyzer_plugin_t *plugin);

#endif // __VSFUSBH_USBLYZER_H_INCLUDED__
