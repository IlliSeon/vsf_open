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
#include "vsf.h"

struct vsfusbh_libusb_t
{
	struct vsflist_t devlist;
	struct
	{
		void *param;
		void (*on_event)(void *param, struct vsfusbh_libusb_dev_t *dev, enum vsfusbh_libusb_evt_t evt);
	} cb;
#ifdef VSFCFG_THREAD_SAFTY
	uint8_t origlevel;
#endif
	struct vsfusbh_libusb_dev_t *curdev;
} static vsfusbh_libusb;

static void *vsfusbh_libusb_probe(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, struct vsfusbh_ifs_t *ifs,
		const struct vsfusbh_device_id_t *id)
{
	struct vsfusbh_libusb_dev_t *ldev;

	ldev = vsf_bufmgr_malloc(sizeof(struct vsfusbh_libusb_dev_t));
	if (ldev == NULL)
		return NULL;
	memset(ldev, 0, sizeof(struct vsfusbh_libusb_dev_t));

	ldev->usbh = usbh;
	ldev->dev = dev;
	ldev->vid = dev->device_desc->idVendor;
	ldev->pid = dev->device_desc->idProduct;
	ldev->urb = vsfusbh_alloc_urb(ldev->usbh);
	if (!ldev->urb)
	{
		vsf_bufmgr_free(ldev);
		return NULL;
	}
	ldev->urb->hcddev = &dev->hcddev;
	vsflist_append(&vsfusbh_libusb.devlist, &ldev->list);
	if (vsfusbh_libusb.cb.on_event != NULL)
		vsfusbh_libusb.cb.on_event(vsfusbh_libusb.cb.param, ldev, VSFUSBH_LIBUSB_EVT_ON_ARRIVED);
	return ldev;
}

static void vsfusbh_libusb_free(struct vsfusbh_libusb_dev_t *ldev)
{
	vsflist_remove(&vsfusbh_libusb.devlist.next, &ldev->list);
	if (ldev->urb)
		vsfusbh_free_urb(ldev->usbh, &ldev->urb);
	vsf_bufmgr_free(ldev);
}

static void vsfusbh_libusb_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct vsfusbh_libusb_dev_t *ldev = (struct vsfusbh_libusb_dev_t *)priv;
	if (ldev)
	{
		if (vsfusbh_libusb.cb.on_event != NULL)
			vsfusbh_libusb.cb.on_event(vsfusbh_libusb.cb.param, ldev, VSFUSBH_LIBUSB_EVT_ON_LEFT);
		if (ldev->opened || ldev->refed)
			ldev->removed = true;
		else
			vsfusbh_libusb_free(ldev);
	}
}

static const struct vsfusbh_device_id_t vsfusbh_libusb_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_DEV_LO,
		.bcdDevice_lo = 0,
		.bDeviceClass = 1,
	},
	{0},
};

void vsfusbh_libusb_set_evthandler(void *param,
	void (*on_event)(void *param, struct vsfusbh_libusb_dev_t *dev, enum vsfusbh_libusb_evt_t evt))
{
	uint8_t origlevel = vsfsm_sched_lock();

	vsfusbh_libusb.cb.param = param;
	vsfusbh_libusb.cb.on_event = on_event;

	vsfsm_sched_unlock(origlevel);
}

uint32_t vsfusbh_libusb_enum_begin(void)
{
	vsfusbh_libusb.origlevel = vsfsm_sched_lock();
	vsfusbh_libusb.curdev = NULL;
	return vsflist_get_length(vsfusbh_libusb.devlist.next);
}

struct vsfusbh_libusb_dev_t *vsfusbh_libusb_enum_next(void)
{
	if (!vsfusbh_libusb.curdev)
		vsfusbh_libusb.curdev = vsflist_get_container(
			vsfusbh_libusb.devlist.next, struct vsfusbh_libusb_dev_t, list);
	else
		vsfusbh_libusb.curdev = vsflist_get_container(
			vsfusbh_libusb.curdev->list.next, struct vsfusbh_libusb_dev_t, list);
	return vsfusbh_libusb.curdev;
}

void vsfusbh_libusb_enum_end(void)
{
	vsfsm_sched_unlock(vsfusbh_libusb.origlevel);
}

// vsfusbh_libusb_ref can ONLY be called between
// 	vsfusbh_libusb_enum_begin and vsfusbh_libusb_enum_end
void vsfusbh_libusb_ref(struct vsfusbh_libusb_dev_t *ldev)
{
	ldev->refed = true;
}

void vsfusbh_libusb_deref(struct vsfusbh_libusb_dev_t *ldev)
{
	ldev->refed = false;
	if (!ldev->opened && ldev->removed)
		vsfusbh_libusb_free(ldev);
}

void vsfusbh_libusb_close(struct vsfusbh_libusb_dev_t *ldev)
{
	uint8_t origlevel = vsfsm_sched_lock();

	ldev->opened = false;
	if (!ldev->refed && ldev->removed)
		vsfusbh_libusb_free(ldev);

	vsfsm_sched_unlock(origlevel);
}

vsf_err_t vsfusbh_libusb_open(struct vsfusbh_libusb_dev_t *ldev)
{
	uint8_t origlevel = vsfsm_sched_lock();
	vsf_err_t err = VSFERR_NONE;

	if (ldev->opened)
		err = VSFERR_FAIL;
	else
		ldev->opened = true;

	vsfsm_sched_unlock(origlevel);
	return err;
}

const struct vsfusbh_class_drv_t vsfusbh_libusb_drv =
{
	.name = "libusb",
	.id_table = vsfusbh_libusb_id_table,
	.probe = vsfusbh_libusb_probe,
	.disconnect = vsfusbh_libusb_disconnect,
};
