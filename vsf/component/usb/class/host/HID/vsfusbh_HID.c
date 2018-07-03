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

PACKED_HEAD struct PACKED_MID usb_hid_class_descriptor_t
{
	uint8_t bDescriptorType;
	uint16_t wDescriptorLength;
}; PACKED_TAIL

PACKED_HEAD struct PACKED_MID usb_hid_descriptor_t
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdHID;
	uint8_t bCountryCode;
	uint8_t bNumDescriptors;

	struct usb_hid_class_descriptor_t desc[1];
}; PACKED_TAIL

static vsf_err_t vsfusbh_hid_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	vsf_err_t err;
	struct vsfusbh_hid_t *hid = (struct vsfusbh_hid_t *)pt->user_data;
	struct vsfhcd_urb_t *inurb = hid->inurb;

	vsfsm_pt_begin(pt);

	inurb->notifier_sm = &hid->sm;
	inurb->transfer_length = hid->hid_desc_len;
	if (hid->hid_desc_len > 1024) return VSFERR_NOT_SUPPORT;
	vsfusbh_alloc_urb_buffer(inurb, hid->hid_desc_len);
	if (inurb->transfer_buffer == NULL) return VSFERR_FAIL;

	if (vsfsm_crit_enter(&hid->dev->ep0_crit, pt->sm))
		vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

	err = vsfusbh_get_class_descriptor(hid->usbh, inurb, hid->bInterfaceNumber,
				USB_DT_REPORT, 0);
	if (err != VSFERR_NONE) goto inurb_fail_crit;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	vsfsm_crit_leave(&hid->dev->ep0_crit);
	if (inurb->status != URB_OK) goto inurb_fail;
	if (inurb->actual_length != inurb->transfer_length) goto inurb_fail;

	err = vsfhid_parse_report(&hid->hid_dev, inurb->transfer_buffer,
				inurb->transfer_length);
	if (err != VSFERR_NONE) goto inurb_fail;

	vsfusbh_free_urb_buffer(inurb);

	// submit urb
	inurb->interval = 4;
	inurb->pipe = usb_rcvintpipe(&hid->dev->hcddev, hid->ep_in);
	inurb->transfer_length = max(vsfhid_get_max_input_size(&hid->hid_dev),
				hid->dev->ep_mps_in[hid->ep_in]);
	inurb->transfer_buffer = vsf_bufmgr_malloc(inurb->transfer_length);
	if (!inurb->transfer_buffer) return VSFERR_FAIL;
	err = vsfusbh_submit_urb(hid->usbh, inurb);
	if (err != VSFERR_NONE) goto inurb_fail;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	if (inurb->status != URB_OK) goto inurb_fail;

	// poll
	while(1)
	{
		if (inurb->status == URB_OK)
			vsfhid_process_input(&hid->hid_dev, inurb->transfer_buffer,
					inurb->actual_length);

		err = vsfusbh_relink_urb(hid->usbh, inurb);
		if (err != VSFERR_NONE) goto inurb_fail;
		vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	}

	vsfsm_pt_end(pt);
	return VSFERR_NONE;

inurb_fail_crit:
	vsfsm_crit_leave(&hid->dev->ep0_crit);
inurb_fail:
	vsfusbh_free_urb_buffer(inurb);
	return VSFERR_FAIL;
}

static struct vsfsm_state_t *vsfusbh_hid_evt_handler_init(struct vsfsm_t *sm,
		vsfsm_evt_t evt)
{
	vsf_err_t err;
	struct vsfusbh_hid_t *hid = (struct vsfusbh_hid_t *)sm->user_data;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		hid->pt.thread = vsfusbh_hid_thread;
		hid->pt.user_data = hid;
		hid->pt.sm = sm;
		hid->pt.state = 0;
	case VSFSM_EVT_URB_COMPLETE:
	case VSFSM_EVT_EP0_CRIT:
	case VSFSM_EVT_DELAY_DONE:
		err = hid->pt.thread(&hid->pt, evt);
		if (err < 0)
		{
			vsfusbh_remove_interface(hid->usbh, hid->dev, hid->ifs);
		}
		break;
	default:
		break;
	}
	return NULL;
}

static void *vsfusbh_hid_probe(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, struct vsfusbh_ifs_t *ifs,
		const struct vsfusbh_device_id_t *id)
{
	struct vsfusbh_ifs_alt_t *alt = &ifs->alt[ifs->cur_alt];
	struct usb_interface_desc_t *ifs_desc = alt->ifs_desc;
	struct usb_endpoint_desc_t *ep_desc = alt->ep_desc;
	struct vsfusbh_hid_t *hid;
	struct usb_hid_descriptor_t *hid_desc;
	uint8_t epaddr;

	if (vsfusbh_get_extra_descriptor((uint8_t *)ifs_desc,
			alt->desc_size, USB_DT_HID, (void **)&hid_desc) != VSFERR_NONE)
	{
		return NULL;
	}

	hid = vsf_bufmgr_malloc(sizeof(struct vsfusbh_hid_t));
	if (hid == NULL)
		return NULL;
	memset(hid, 0, sizeof(struct vsfusbh_hid_t));

	hid->inurb = vsfusbh_alloc_urb(usbh);
	if (hid->inurb == NULL)
		goto fail_inurb;
	hid->outurb = vsfusbh_alloc_urb(usbh);
	if (hid->outurb == NULL)
		goto fail_outurb;

	hid->inurb->hcddev = &dev->hcddev;
	hid->outurb->hcddev = &dev->hcddev;
	for (int i = 0; i < ifs_desc->bNumEndpoints; i++)
	{
		epaddr = ep_desc->bEndpointAddress;
		if (epaddr & USB_ENDPOINT_DIR_MASK)
			hid->ep_in = epaddr & 0x7F;
		ep_desc = (struct usb_endpoint_desc_t *)((uint32_t)ep_desc + ep_desc->bLength);
	}

	hid->usbh = usbh;
	hid->dev = dev;
	hid->ifs = ifs;
	hid->bInterfaceNumber = ifs_desc->bInterfaceNumber;
	hid->hid_desc_len = hid_desc->desc[0].wDescriptorLength;

	hid->sm.init_state.evt_handler = vsfusbh_hid_evt_handler_init;
	hid->sm.user_data = hid;
	vsfsm_init(&hid->sm);

	return hid;
fail_outurb:
	vsfusbh_free_urb(usbh, &hid->inurb);
fail_inurb:
	vsf_bufmgr_free(hid);
	return NULL;
}

static void vsfusbh_hid_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct vsfusbh_hid_t *hid = priv;
	if (hid == NULL)
		return;

	if (hid->inurb != NULL)
		vsfusbh_free_urb(usbh, &hid->inurb);
	if (hid->outurb != NULL)
		vsfusbh_free_urb(usbh, &hid->inurb);

	vsfsm_fini(&hid->sm);

	vsfhid_free_dev(&hid->hid_dev);

	vsf_bufmgr_free(hid);
}

static const struct vsfusbh_device_id_t vsfusbh_hid_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS,
		.bInterfaceClass = USB_CLASS_HID,
	},
	{0},
};

const struct vsfusbh_class_drv_t vsfusbh_hid_drv =
{
	.name = "hid",
	.id_table = vsfusbh_hid_id_table,
	.probe = vsfusbh_hid_probe,
	.disconnect = vsfusbh_hid_disconnect,
};

