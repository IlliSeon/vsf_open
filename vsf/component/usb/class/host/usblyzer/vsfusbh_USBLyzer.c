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
#include <stdarg.h>
#include <stdio.h>

struct usblyzer_urbcb_t
{
	struct vsfhcd_urb_t *urb;
	struct vsfsm_t sm;
	int8_t ep;
	uint8_t epsize;
	uint16_t totalsize;
	uint16_t curpos;
#define USBLYZER_EP_IN					1
#define USBLYZER_EP_OUT					0
	uint8_t epdir : 1;
	uint8_t eptype : 3;
	uint8_t urb_submitted : 1;
	uint8_t needzlp : 1;
	uint8_t ep_inited : 1;
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
	uint8_t transact_finished : 1;
#endif
};

struct usblyzer_t
{
	struct
	{
		void *param;
		void (*on_event)(void*, enum vsfhal_usbd_evt_t, uint32_t, uint8_t*, uint32_t);
		bool (*output_isbusy)(void *param);
	} cb;

	struct vsfusbh_t *usbh;
	struct vsfusbh_device_t *dev;

	struct
	{
		const struct vsfhal_usbd_t *drv;
		int32_t int_priority;
		struct usb_ctrlrequest_t request;
		enum
		{
			USB_IDLE = 0, USB_SETUP, URB_SUBMITTED, URB_COMPLETED, USB_STATUS,
		} request_state;
		struct vsfsm_t sm;
		uint8_t dev_address;
		uint8_t host_address;
		uint8_t ep0size;
		uint8_t *config_desc;
	} usbd;
	union
	{
		struct
		{
			struct usblyzer_urbcb_t in[16];
			struct usblyzer_urbcb_t out[16];
		};
		struct usblyzer_urbcb_t all[16 + 16];
	} urbcb;
	struct usblyzer_urbcb_t *pending_urbcb;
	bool resetting;
} static usblyzer;

#define USBLYZER_EVT_TIMEOUT			(VSFSM_EVT_USER_LOCAL + 0x1)
#define USBLYZER_EVT_RESET				(VSFSM_EVT_USER_LOCAL + 0x2)
#define USBLYZER_EVT_SETUP				(VSFSM_EVT_USER_LOCAL + 0x3)
#define USBLYZER_EVT_EPINOUT			(VSFSM_EVT_USER_LOCAL + 0x10)
#define USBLYZER_EVT_EPNAK				(VSFSM_EVT_USER_LOCAL + 0x20)

#define USBLYZER_EVT_EP_MASK			0x00F
#define USBLYZER_EVT_DIR_MASK			0x100
#define USBLYZER_EVT_DIR_IN				0x100
#define USBLYZER_EVT_DIR_OUT			0x000
#define USBLYZER_EVT_EVT_MASK			~0xF
#define USBLYZER_EVT_EPIN				(USBLYZER_EVT_EPINOUT | USBLYZER_EVT_DIR_IN)
#define USBLYZER_EVT_EPOUT				(USBLYZER_EVT_EPINOUT | USBLYZER_EVT_DIR_OUT)
#define USBLYZER_EVT_INEP(ep)			(USBLYZER_EVT_EPIN + (ep))
#define USBLYZER_EVT_OUTEP(ep)			(USBLYZER_EVT_EPOUT + (ep))
#define USBLYZER_EVT_NAKEP(ep)			(USBLYZER_EVT_EPNAK + (ep))

static vsf_err_t usblyzer_usbd_on_RESET(void *p)
{
	struct usblyzer_t *usblyzer = p;
	struct vsfsm_t *sm = &usblyzer->usbd.sm;
	return vsfsm_post_evt_pending(sm, USBLYZER_EVT_RESET);
}

static vsf_err_t usblyzer_usbd_on_SETUP(void *p)
{
	struct usblyzer_t *usblyzer = p;
	struct vsfsm_t *sm = &usblyzer->usbd.sm;
	return vsfsm_post_evt_pending(sm, USBLYZER_EVT_SETUP);
}

static vsf_err_t usblyzer_usbd_on_IN(void *p, uint8_t ep)
{
	struct usblyzer_t *usblyzer = p;
	struct vsfsm_t *sm = &usblyzer->usbd.sm;
	return vsfsm_post_evt_pending(sm, USBLYZER_EVT_INEP(ep));
}

static vsf_err_t usblyzer_usbd_on_OUT(void *p, uint8_t ep)
{
	struct usblyzer_t *usblyzer = p;
	struct vsfsm_t *sm = &usblyzer->usbd.sm;
	return vsfsm_post_evt_pending(sm, USBLYZER_EVT_OUTEP(ep));
}

#ifdef VSFHAL_CFG_USBD_ONNAK_EN
static vsf_err_t usblyzer_usbd_on_NAK(void *p, uint8_t ep)
{
	struct usblyzer_t *usblyzer = p;
	struct vsfsm_t *sm = &usblyzer->usbd.sm;
	return vsfsm_post_evt_pending(sm, USBLYZER_EVT_NAKEP(ep));
}
#endif

static vsf_err_t usblyzer_usbd_ep_recv(struct usblyzer_urbcb_t *urbcb)
{
	const struct vsfhal_usbd_t *drv = usblyzer.usbd.drv;
	uint16_t cursize = drv->ep.get_OUT_count(urbcb->ep);
	uint16_t remain = urbcb->totalsize - urbcb->curpos;
	uint16_t epsize = drv->ep.get_OUT_epsize(urbcb->ep);

	if (remain > 0)
	{
		uint8_t *data = (uint8_t *)urbcb->urb->transfer_buffer + urbcb->curpos;

		drv->ep.read_OUT_buffer(urbcb->ep, data, cursize);
		if (usblyzer.cb.on_event != NULL)
			usblyzer.cb.on_event(usblyzer.cb.param, VSFHAL_USBD_ON_OUT, urbcb->ep, data, cursize);
		urbcb->curpos += cursize;
		remain = urbcb->totalsize - urbcb->curpos;
	}
	else
	{
		if (usblyzer.cb.on_event != NULL)
			usblyzer.cb.on_event(usblyzer.cb.param, VSFHAL_USBD_ON_OUT, urbcb->ep, NULL, 0);
		return VSFERR_NONE;
	}
	if ((remain > 0) && (cursize >= epsize))
	{
		drv->ep.enable_OUT(urbcb->ep);
		return VSFERR_NOT_READY;
	}
	return VSFERR_NONE;
}

static vsf_err_t usblyzer_usbd_ep_send(struct usblyzer_urbcb_t *urbcb)
{
	const struct vsfhal_usbd_t *drv = usblyzer.usbd.drv;
	uint16_t epsize = drv->ep.get_IN_epsize(urbcb->ep);
	uint16_t remain = urbcb->totalsize - urbcb->curpos;
	uint16_t cursize = min(epsize, remain);

	if (cursize)
	{
		uint8_t *data = (uint8_t *)urbcb->urb->transfer_buffer + urbcb->curpos;

		if (usblyzer.cb.on_event != NULL)
			usblyzer.cb.on_event(usblyzer.cb.param, VSFHAL_USBD_ON_IN, urbcb->ep, data, cursize);
		drv->ep.write_IN_buffer(urbcb->ep, data, cursize);
		drv->ep.set_IN_count(urbcb->ep, cursize);

		urbcb->curpos += cursize;
		remain = urbcb->totalsize - urbcb->curpos;
		if (!remain && (cursize < epsize))
			urbcb->needzlp = false;
		return VSFERR_NOT_READY;
	}
	else if (urbcb->needzlp)
	{
		if (usblyzer.cb.on_event != NULL)
			usblyzer.cb.on_event(usblyzer.cb.param, VSFHAL_USBD_ON_IN, urbcb->ep, NULL, 0);
		urbcb->needzlp = false;
		drv->ep.set_IN_count(urbcb->ep, 0);
		return VSFERR_NOT_READY;
	}
	return VSFERR_NONE;
}

static vsf_err_t usblyzer_usbh_prepare_urb(struct usblyzer_urbcb_t *urbcb,
			uint16_t bufsize)
{
	bool isin = urbcb->epdir == USBLYZER_EP_IN;

	if (urbcb->urb_submitted)
		return VSFERR_FAIL;
	if (!urbcb->urb)
		urbcb->urb = vsfusbh_alloc_urb(usblyzer.usbh);
	if (!urbcb->urb)
	{
		vsfdbg_prints("Fail to allocate urb" VSFCFG_DEBUG_LINEEND);
		return VSFERR_FAIL;
	}
	vsfusbh_free_urb_buffer(urbcb->urb);
	if (bufsize)
	{
		if (!vsfusbh_alloc_urb_buffer(urbcb->urb, bufsize))
		{
			vsfdbg_prints("Fail to allocate transfer buffer" VSFCFG_DEBUG_LINEEND);
			return VSFERR_FAIL;
		}
	}

	urbcb->urb->hcddev = &usblyzer.dev->hcddev;
	urbcb->urb->timeout = 200;
	urbcb->urb->notifier_sm = &urbcb->sm;
	urbcb->urb->transfer_length = bufsize;

	switch (urbcb->eptype)
	{
	case USB_EP_TYPE_CONTROL:
		urbcb->urb->pipe = isin ? usb_rcvctrlpipe(urbcb->urb->hcddev, 0) :
									usb_sndctrlpipe(urbcb->urb->hcddev, 0);
		break;
	case USB_EP_TYPE_INTERRUPT:
		urbcb->urb->pipe = isin ? usb_rcvintpipe(urbcb->urb->hcddev, urbcb->ep) :
									usb_sndintpipe(urbcb->urb->hcddev, urbcb->ep);
		break;
	case USB_EP_TYPE_BULK:
		urbcb->urb->pipe = isin ? usb_rcvbulkpipe(urbcb->urb->hcddev, urbcb->ep) :
									usb_sndbulkpipe(urbcb->urb->hcddev, urbcb->ep);
		break;
	case USB_EP_TYPE_ISO:
		urbcb->urb->pipe = isin ? usb_rcvisocpipe(urbcb->urb->hcddev, urbcb->ep) :
									usb_sndisocpipe(urbcb->urb->hcddev, urbcb->ep);
		break;
	}
	return VSFERR_NONE;
}

static bool usblyzer_usbh_is_periodic(struct usblyzer_urbcb_t *urbcb)
{
	return (urbcb->eptype == USB_EP_TYPE_INTERRUPT) ||
			(urbcb->eptype == USB_EP_TYPE_ISO);
}

static vsf_err_t usblyzer_usbh_submit_urb(struct usblyzer_urbcb_t *urbcb)
{
	vsf_err_t err = VSFERR_NONE;
	if (!urbcb->urb_submitted)
	{
		err = !usblyzer_usbh_is_periodic(urbcb) || !urbcb->ep_inited ?
				vsfusbh_submit_urb(usblyzer.usbh, urbcb->urb) :
				vsfusbh_relink_urb(usblyzer.usbh, urbcb->urb);
		if (err)
			vsfdbg_prints("Fail to submit urb" VSFCFG_DEBUG_LINEEND);
	}
	return err;
}

static void usblyzer_usbd_setup_patch(struct usb_ctrlrequest_t *request)
{
	uint8_t type = request->bRequestType & USB_TYPE_MASK;
	uint8_t recip = request->bRequestType & USB_RECIP_MASK;

	if (USB_TYPE_STANDARD == type)
	{
		if (USB_RECIP_DEVICE == recip)
		{
			switch (request->bRequest)
			{
			case USB_REQ_SET_ADDRESS:
				usblyzer.usbd.dev_address = request->wValue;
				request->wValue = usblyzer.usbd.host_address;
				break;
			}
		}
	}
}

static void usblyzer_usbd_setup_process(struct usblyzer_urbcb_t *urbcb)
{
	const struct vsfhal_usbd_t *drv = usblyzer.usbd.drv;
	struct usb_ctrlrequest_t *request = &usblyzer.usbd.request;
	uint8_t type = request->bRequestType & USB_TYPE_MASK;
	uint8_t recip = request->bRequestType & USB_RECIP_MASK;
	uint8_t *data = urbcb->urb->transfer_buffer;
	uint16_t len = urbcb->urb->actual_length;

	if (USB_TYPE_STANDARD == type)
	{
		if (USB_RECIP_DEVICE == recip)
		{
			switch (request->bRequest)
			{
			case USB_REQ_SET_ADDRESS:
				usblyzer.dev->hcddev.devnum = usblyzer.usbd.host_address;
				drv->set_address(usblyzer.usbd.dev_address);
				break;
			case USB_REQ_GET_DESCRIPTOR:
				switch ((request->wValue >> 8) & 0xFF)
				{
				case USB_DT_DEVICE:
					usblyzer.urbcb.in[0].epsize = usblyzer.usbd.ep0size;
					usblyzer.urbcb.out[0].epsize = usblyzer.usbd.ep0size;
					usblyzer.dev->ep_mps_in[0] = usblyzer.usbd.ep0size;
					usblyzer.dev->ep_mps_out[0] = usblyzer.usbd.ep0size;
					break;
				case USB_DT_CONFIG:
					if (GET_LE_U16(&data[2]) == len)
					{
						if (usblyzer.usbd.config_desc)
							vsf_bufmgr_free(usblyzer.usbd.config_desc);
						usblyzer.usbd.config_desc = vsf_bufmgr_malloc(len);
						if (!usblyzer.usbd.config_desc)
							break;
						memcpy(usblyzer.usbd.config_desc, data, len);
					}
					break;
				}
				break;
			case USB_REQ_SET_CONFIGURATION:
				if (usblyzer.usbd.config_desc &&
					(request->wValue == usblyzer.usbd.config_desc[5]))
				{
					uint16_t pos = USB_DT_CONFIG_SIZE;
					enum vsfhal_usbd_eptype_t eptype;
					uint16_t epsize, epaddr, epindex, epattr;
					struct usblyzer_urbcb_t *urbcb;

					len = GET_LE_U16(&usblyzer.usbd.config_desc[2]);
					data = usblyzer.usbd.config_desc;
					while (len > pos)
					{
						switch (data[pos + 1])
						{
						case USB_DT_ENDPOINT:
							epaddr = data[pos + 2];
							epattr = data[pos + 3];
							epsize = GET_LE_U16(&data[pos + 4]);
							epindex = epaddr & 0x0F;
							switch (epattr & 0x03)
							{
							case 0x00: eptype = USB_EP_TYPE_CONTROL; break;
							case 0x01: eptype = USB_EP_TYPE_ISO; break;
							case 0x02: eptype = USB_EP_TYPE_BULK; break;
							case 0x03: eptype = USB_EP_TYPE_INTERRUPT; break;
							}

							urbcb = (epaddr & 0x80) ?
										&usblyzer.urbcb.in[epindex] :
										&usblyzer.urbcb.out[epindex];
							if (urbcb->epsize)
								// already initialized
								break;

							urbcb->epsize = epsize;
							urbcb->eptype = eptype;
							if (epaddr & 0x80)
							{
								drv->ep.set_IN_epsize(epindex, epsize);
								usblyzer.dev->ep_mps_in[epindex] = epsize;
#ifndef VSFHAL_CFG_USBD_ONNAK_EN
								if (!usblyzer_usbh_prepare_urb(urbcb, epsize))
								{
									vsfsm_init(&urbcb->sm);
									urbcb->ep_inited = true;
								}
#endif
							}
							else
							{
								drv->ep.set_OUT_epsize(epindex, epsize);
								drv->ep.enable_OUT(epindex);
								usblyzer.dev->ep_mps_out[epindex] = epsize;
							}
							drv->ep.set_type(epindex, eptype);
							break;
						}
						pos += data[pos];
					}
				}
				break;
			}
		}
		else if (USB_RECIP_ENDPOINT == recip)
		{
			uint8_t epnum = request->wIndex & 0x7F;
			uint8_t epdir = request->wIndex * 0x80;

			switch (request->bRequest)
			{
			case USB_REQ_CLEAR_FEATURE:
				if (epdir)
				{
					drv->ep.reset_IN_toggle(epnum);
					drv->ep.clear_IN_stall(epnum);
#ifndef VSFHAL_CFG_USBD_ONNAK_EN
					usblyzer_usbh_submit_urb(&usblyzer.urbcb.in[epnum]);
#endif
				}
				else
				{
					drv->ep.reset_OUT_toggle(epnum);
					drv->ep.clear_OUT_stall(epnum);
					drv->ep.enable_OUT(epnum);
				}
				break;
			}
		}
	}
}

static struct vsfsm_state_t *
usblyzer_usbh_devurb_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct usblyzer_urbcb_t *urbcb = (struct usblyzer_urbcb_t *)sm->user_data;
	const struct vsfhal_usbd_t *drv = usblyzer.usbd.drv;
	struct vsfhcd_urb_t *urb = urbcb->urb;
	vsf_err_t err;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		err = usblyzer_usbh_submit_urb(urbcb);
		if (!err)
			urbcb->urb_submitted = true;
		break;
	case VSFSM_EVT_URB_COMPLETE:
		if (usblyzer.cb.output_isbusy != NULL)
		{
			if (usblyzer.cb.output_isbusy(usblyzer.cb.param))
			{
				vsftimer_create(sm, 1, 1, VSFSM_EVT_URB_COMPLETE);
				break;
			}
		}
#ifdef VSF_USBLYZER_URB_DELAY
		vsftimer_create(sm, VSF_USBLYZER_URB_DELAY, 1, VSFSM_EVT_TIMER);
		break;
#endif
	case VSFSM_EVT_TIMER:
		urbcb->urb_submitted = false;
		if (!urbcb->ep)
		{
			struct usb_ctrlrequest_t *request = &usblyzer.usbd.request;
			bool isin = (request->bRequestType & USB_DIR_MASK) == USB_DIR_IN;

			usblyzer.usbd.request_state++;
			if (urb->status == URB_OK)
			{
				urbcb->curpos = 0;
				if (isin)
				{
					uint16_t epsize = drv->ep.get_IN_epsize(urbcb->ep);
					urbcb->totalsize = urb->actual_length;
					urbcb->needzlp = urbcb->totalsize > epsize;
				}
				else
				{
					usblyzer.usbd.request_state++;
					urbcb->totalsize = 0;
					urbcb->needzlp = true;
				}
				usblyzer_usbd_ep_send(urbcb);
			}
			else
			{
				// urb failed, stall ep0
				if (usblyzer.cb.on_event != NULL)
					usblyzer.cb.on_event(usblyzer.cb.param, VSFHAL_USBD_ON_STALL, 0, NULL, 0);
				drv->ep.set_IN_stall(0);
				drv->ep.set_OUT_stall(0);
				usblyzer.usbd.request_state = USB_IDLE;
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
				urbcb->transact_finished = true;
#endif
			}
		}
		else
		{
			if (urbcb->epdir == USBLYZER_EP_IN)
			{
				if (urb->status != URB_OK)
				{
					if (usblyzer.cb.on_event != NULL)
						usblyzer.cb.on_event(usblyzer.cb.param, VSFHAL_USBD_ON_STALL, urbcb->ep, NULL, 0);
					drv->ep.set_IN_stall(urbcb->ep);
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
					urbcb->transact_finished = true;
#endif
				}
				else
				{
					urbcb->curpos = 0;
					urbcb->totalsize = urb->actual_length;
					urbcb->needzlp = !urbcb->totalsize;
					usblyzer_usbd_ep_send(urbcb);
				}
			}
			else
			{
				if (urb->status == URB_OK)
					drv->ep.enable_OUT(urbcb->ep);
				else
				{
					vsfdbg_printf("OUT%d: STALL" VSFCFG_DEBUG_LINEEND, urbcb->ep);
					drv->ep.set_OUT_stall(urbcb->ep);
				}
			}
		}
		break;
	}
	return NULL;
}

static void usblyzer_usbd_on_event(void *param, enum vsfhal_usbd_evt_t evt, uint32_t value)
{
	switch (evt)
	{
	case VSFHAL_USBD_ON_RESET:	usblyzer_usbd_on_RESET(param); break;
	case VSFHAL_USBD_ON_SETUP:	usblyzer_usbd_on_SETUP(param); break;
	case VSFHAL_USBD_ON_IN:		usblyzer_usbd_on_IN(param, (uint8_t)value); break;
	case VSFHAL_USBD_ON_OUT:	usblyzer_usbd_on_OUT(param, (uint8_t)value); break;
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
	case VSFHAL_USBD_ON_NAK:	usblyzer_usbd_on_NAK(param, (uint8_t)value); break;
#endif
	}
}

// non-open functions in hub module
bool vsfusbh_hub_dev_is_reset(struct vsfusbh_device_t *dev);
vsf_err_t vsfusbh_hub_reset_dev(struct vsfusbh_device_t *dev);
static struct vsfsm_state_t *
usblyzer_usbd_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct usblyzer_t *usblyzer = (struct usblyzer_t *)sm->user_data;
	const struct vsfhal_usbd_t *drv = usblyzer->usbd.drv;
	struct usb_ctrlrequest_t *request = &usblyzer->usbd.request;
	bool isin = (request->bRequestType & USB_DIR_MASK) == USB_DIR_IN;
	struct usblyzer_urbcb_t *urbcb;
	uint8_t ep;

	switch (evt)
	{
	case VSFSM_EVT_ENTER:
	case VSFSM_EVT_EXIT:
	case VSFSM_EVT_FINI:
		break;
	case VSFSM_EVT_INIT:
		usblyzer->urbcb.in[0].epsize = usblyzer->urbcb.out[0].epsize = 64;
		usblyzer->dev->ep_mps_in[0] = usblyzer->dev->ep_mps_out[0] = 64;

		drv->callback->param = usblyzer;
		drv->callback->on_event = usblyzer_usbd_on_event;
		drv->init(usblyzer->usbd.int_priority);
		drv->connect();
		break;
	case USBLYZER_EVT_TIMEOUT:
		if (!vsfusbh_hub_dev_is_reset(usblyzer->dev))
		{
			usblyzer->resetting = false;
			if (usblyzer->pending_urbcb)
			{
				vsfsm_init(&usblyzer->pending_urbcb->sm);
				usblyzer->usbd.request_state++;
				usblyzer->pending_urbcb = NULL;
			}
		}
		else
			vsftimer_create(sm, 10, 1, USBLYZER_EVT_TIMEOUT);
		break;
	case USBLYZER_EVT_RESET:
		if (!usblyzer->resetting)
		{
			vsfusbh_hub_reset_dev(usblyzer->dev);
			usblyzer->dev->hcddev.devnum = 0;
			usblyzer->resetting = true;
			usblyzer->usbd.request_state = USB_IDLE;
			for (int i = 0; i < dimof(usblyzer->urbcb.all); i++)
			{
				if (usblyzer->urbcb.all[i].ep != 0)
					usblyzer->urbcb.all[i].epsize = 0;
				usblyzer->urbcb.all[i].urb_submitted = false;
				usblyzer->urbcb.all[i].ep_inited = false;
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
				usblyzer->urbcb.all[i].transact_finished = true;
#endif
				if (usblyzer->urbcb.all[i].urb != NULL)
					vsfusbh_free_urb(usblyzer->usbh, &usblyzer->urbcb.all[i].urb);
			}

			drv->init(usblyzer->usbd.int_priority);
			// config ep0
			drv->prepare_buffer();
			drv->ep.set_IN_epsize(0, usblyzer->usbd.ep0size);
			drv->ep.set_OUT_epsize(0, usblyzer->usbd.ep0size);
			drv->ep.set_type(0, USB_EP_TYPE_CONTROL);
			drv->set_address(0);

			if (usblyzer->cb.on_event != NULL)
				usblyzer->cb.on_event(usblyzer->cb.param, VSFHAL_USBD_ON_RESET, 0, NULL, 0);
			drv->reset();
			vsftimer_create(sm, 10, 1, USBLYZER_EVT_TIMEOUT);
		}
		break;
	case USBLYZER_EVT_SETUP:
		if (usblyzer->resetting && usblyzer->pending_urbcb)
		{
			vsfdbg_prints("Setup while resetting" VSFCFG_DEBUG_LINEEND);
			break;
		}
		if (usblyzer->usbd.request_state)
		{
			vsfdbg_prints("Setup sequence error" VSFCFG_DEBUG_LINEEND);
			break;
		}
		if (!drv->get_setup((uint8_t *)&usblyzer->usbd.request))
		{
			isin = (request->bRequestType & USB_DIR_MASK) == USB_DIR_IN;
			urbcb = isin ? &usblyzer->urbcb.in[0] : &usblyzer->urbcb.out[0];

			if (usblyzer->cb.on_event != NULL)
				usblyzer->cb.on_event(usblyzer->cb.param, VSFHAL_USBD_ON_SETUP, 0,
					(uint8_t *)&usblyzer->usbd.request, sizeof(usblyzer->usbd.request));

			usblyzer_usbd_setup_patch(request);
			if (!usblyzer_usbh_prepare_urb(urbcb, request->wLength))
			{
				urbcb->urb->setup_packet = *request;
				usblyzer->usbd.request_state++;
				if (isin)
				{
					if (usblyzer->resetting)
						usblyzer->pending_urbcb = urbcb;
					else
					{
						vsfsm_init(&urbcb->sm);
						usblyzer->usbd.request_state++;
					}
				}
				else
				{
					urbcb->curpos = 0;
					urbcb->totalsize = request->wLength;
					if (!urbcb->totalsize)
					{
						if (usblyzer->resetting)
							usblyzer->pending_urbcb = urbcb;
						else
						{
							vsfsm_init(&urbcb->sm);
							usblyzer->usbd.request_state++;
						}
					}
				}
			}
			else
			{
				vsfdbg_prints("Fail to prepare urb" VSFCFG_DEBUG_LINEEND);
			}
		}
		else
		{
			vsfdbg_prints("Fail to read setup" VSFCFG_DEBUG_LINEEND);
		}
		break;
	default:
		ep = evt & USBLYZER_EVT_EP_MASK;
		evt &= USBLYZER_EVT_EVT_MASK;
		if (usblyzer->resetting &&
			(ep || !usblyzer->pending_urbcb || (evt != USBLYZER_EVT_EPOUT)))
		{
			vsfdbg_prints("Transaction while resetting" VSFCFG_DEBUG_LINEEND);
			break;
		}
		else
		{
			switch (evt)
			{
			case USBLYZER_EVT_EPIN:
				urbcb = &usblyzer->urbcb.in[ep & 0x0F];
				if (!ep)
				{
					switch (usblyzer->usbd.request_state)
					{
					case URB_COMPLETED:
						// sending reply
						if (!isin)
						{
							vsfdbg_prints("Setup sequence error" VSFCFG_DEBUG_LINEEND);
							break;
						}
						if (!usblyzer_usbd_ep_send(urbcb))
						{
							usblyzer->usbd.request_state++;
							drv->ep.enable_OUT(0);
						}
						break;
					case USB_STATUS:
						// status sent
						if (isin)
						{
							vsfdbg_prints("Setup sequence error" VSFCFG_DEBUG_LINEEND);
							break;
						}
						urbcb = &usblyzer->urbcb.out[0];
						usblyzer_usbd_setup_process(urbcb);
						usblyzer->usbd.request_state = USB_IDLE;
						break;
					default:
						break;
					}
				}
				else
				{
					if (!usblyzer_usbd_ep_send(urbcb))
					{
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
						urbcb->transact_finished = true;
#else
						// data sent, submit next urb
						usblyzer_usbh_submit_urb(urbcb);
#endif
					}
				}
				break;
			case USBLYZER_EVT_EPOUT:
				urbcb = &usblyzer->urbcb.out[ep & 0x0F];
				if (!ep)
				{
					switch (usblyzer->usbd.request_state)
					{
					case USB_SETUP:
						// receiving data
						if (isin)
						{
							vsfdbg_prints("Setup sequence error" VSFCFG_DEBUG_LINEEND);
							break;
						}
						if (!usblyzer_usbd_ep_recv(urbcb))
						{
							if (usblyzer->resetting)
								usblyzer->pending_urbcb = urbcb;
							else
							{
								vsfsm_init(&urbcb->sm);
								usblyzer->usbd.request_state++;
							}
						}
						break;
					case USB_STATUS:
						// status received
						if (!isin)
						{
							vsfdbg_prints("Setup sequence error" VSFCFG_DEBUG_LINEEND);
							break;
						}
						urbcb->totalsize = urbcb->curpos = 0;
						usblyzer_usbd_ep_recv(urbcb);
						urbcb = &usblyzer->urbcb.in[0];
						usblyzer_usbd_setup_process(urbcb);
						usblyzer->usbd.request_state = USB_IDLE;
						break;
					default:
						break;
					}
				}
				else
				{
					// data received, submit urb
					urbcb->curpos = 0;
					urbcb->totalsize = drv->ep.get_OUT_count(ep);
					if (!usblyzer_usbh_prepare_urb(urbcb, urbcb->totalsize))
					{
						if (urbcb->totalsize)
							usblyzer_usbd_ep_recv(urbcb);
						else
							urbcb->urb->transfer_flags |= URB_ZERO_PACKET;

						vsfsm_init(&urbcb->sm);
						urbcb->ep_inited = true;
					}
					else
					{
						vsfdbg_prints("Fail to prepare urb" VSFCFG_DEBUG_LINEEND);
					}
				}
				break;
#ifdef VSFHAL_CFG_USBD_ONNAK_EN
			case USBLYZER_EVT_EPNAK:
				urbcb = &usblyzer->urbcb.in[ep & 0x0F];
				if (urbcb->ep_inited)
				{
					if (urbcb->transact_finished)
					{
						urbcb->transact_finished = false;
						usblyzer_usbh_submit_urb(urbcb);
					}
				}
				else if (!usblyzer_usbh_prepare_urb(urbcb, urbcb->epsize))
				{
					urbcb->transact_finished = false;
					vsfsm_init(&urbcb->sm);
					urbcb->ep_inited = true;
				}
				break;
#endif
			}
		}
	}
	return NULL;
}

static void *vsfusbh_usblyzer_probe(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, struct vsfusbh_ifs_t *ifs,
		const struct vsfusbh_device_id_t *id)
{
	if (!usblyzer.dev)
	{
		usblyzer.usbd.host_address = dev->hcddev.devnum;
		usblyzer.usbd.ep0size = dev->device_desc->bMaxPacketSize0;
		usblyzer.usbh = usbh;
		usblyzer.dev = dev;
		usblyzer.usbd.sm.init_state.evt_handler = usblyzer_usbd_evt_handler;
		usblyzer.usbd.sm.user_data = &usblyzer;
		vsfsm_init(&usblyzer.usbd.sm);
		return &usblyzer;
	}
	return NULL;
}

static void vsfusbh_usblyzer_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct usblyzer_t *usblyzer = priv;

	if (usblyzer->dev == dev)
	{
		const struct vsfhal_usbd_t *drv = usblyzer->usbd.drv;

		for (int i = 0; i < dimof(usblyzer->urbcb.all); i++)
		{
			if (usblyzer->urbcb.all[i].urb != NULL)
			{
				vsfusbh_free_urb(usbh, &usblyzer->urbcb.all[i].urb);
				vsfsm_fini(&usblyzer->urbcb.all[i].sm);
			}
		}
		if (usblyzer->usbd.config_desc != NULL)
		{
			vsf_bufmgr_free(usblyzer->usbd.config_desc);
			usblyzer->usbd.config_desc = NULL;
		}
		drv->disconnect();
		drv->fini();
		vsfsm_fini(&usblyzer->usbd.sm);
		usblyzer->dev = NULL;
	}
}

static const struct vsfusbh_device_id_t vsfusbh_usblyzer_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_DEV_LO,
		.bcdDevice_lo = 0,
		.bDeviceClass = 1,
	},
	{0},
};

const struct vsfusbh_class_drv_t vsfusbh_usblyzer_drv =
{
	.name = "usblyzer",
	.id_table = vsfusbh_usblyzer_id_table,
	.probe = vsfusbh_usblyzer_probe,
	.disconnect = vsfusbh_usblyzer_disconnect,
};

void usblyzer_init(const struct vsfhal_usbd_t *drv, int32_t int_priority, void *param,
		void (*on_event)(void*, enum vsfhal_usbd_evt_t, uint32_t, uint8_t*, uint32_t),
		bool (*output_isbusy)(void *param))
{
	struct usblyzer_urbcb_t *urbcb;

	usblyzer.cb.param = param;
	usblyzer.cb.on_event = on_event;
	usblyzer.cb.output_isbusy = output_isbusy;
	usblyzer.usbd.drv = drv;
	usblyzer.usbd.int_priority = int_priority;

	for (int i = 0; i < dimof(usblyzer.urbcb.in); i++)
	{
		urbcb = &usblyzer.urbcb.in[i];
		urbcb->ep = i;
		urbcb->epdir = USBLYZER_EP_IN;
		urbcb->sm.init_state.evt_handler = usblyzer_usbh_devurb_evt_handler;
		urbcb->sm.user_data = urbcb;

		urbcb = &usblyzer.urbcb.out[i];
		urbcb->ep = i;
		urbcb->epdir = USBLYZER_EP_OUT;
		urbcb->sm.init_state.evt_handler = usblyzer_usbh_devurb_evt_handler;
		urbcb->sm.user_data = urbcb;
	}
	usblyzer.urbcb.in[0].eptype = usblyzer.urbcb.out[0].eptype =
					USB_EP_TYPE_CONTROL;
}

