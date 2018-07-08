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

enum ipheth_carrier_t
{
	IPHETH_CARRIER_ON = 0x04,
};

struct vsfusbh_ipheth_t
{
	struct vsfsm_pt_t initpt;
	struct vsfsm_t initsm;
	struct vsfsm_sem_t carrier_work_sem;
	struct vsftimer_t *timer;

	struct vsfhcd_urb_t *ctrlurb;
	struct vsfhcd_urb_t *inurb;
	struct vsfsm_t insm;
	struct vsfhcd_urb_t *outurb;
	struct vsfsm_t outsm;

	struct vsfusbh_device_t *dev;
	struct vsfusbh_t *usbh;
	uint8_t ifnum;
	uint8_t alt;

	uint8_t ctrlbuff[6];		// for mac address

	enum ipheth_carrier_t carrier;
	struct vsfip_netif_t netif;
	struct vsfip_netdrv_t netdrv;
	struct vsfip_buffer_t *tx_buffer;
	struct vsfip_buffer_t *rx_buffer;
	unsigned connected : 1;
};
struct vsfusbh_ipheth_cb_t vsfusbh_ipheth_cb;

#define IPHETH_MTU						1516
#define IPHETH_REQ_GETMAC				0x00
#define IPHETH_REQ_UNKNOWN02			0x02
#define IPHETH_REQ_UNKNOWN03			0x03
#define IPHETH_REQ_CARRIER_CHECK		0x45

static vsf_err_t vsfusbh_ipheth_netdrv_init(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
static vsf_err_t vsfusbh_ipheth_netdrv_fini(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
static void vsfusbh_ipheth_netdrv_free(struct vsfip_netif_t *netif);
static bool vsfip_ipheth_netdrv_available(struct vsfip_netif_t *netif,
	const struct vsfip_ipaddr_t *dest_addr);
static struct vsfip_netdrv_op_t vsfusbh_ipheth_netdrv_op =
{
	vsfusbh_ipheth_netdrv_init, vsfusbh_ipheth_netdrv_fini, vsfusbh_ipheth_netdrv_free,
	vsfip_eth_header, vsfip_ipheth_netdrv_available
};

static struct vsfsm_state_t *
vsfusbh_ipheth_in_evthandler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)sm->user_data;
	struct vsfhcd_urb_t *inurb = ipheth->inurb;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		inurb->notifier_sm = sm;
		inurb->transfer_length = IPHETH_MTU;
	re_submit:
		if (!ipheth->rx_buffer)
			ipheth->rx_buffer = vsfip_buffer_get(IPHETH_MTU + 2);
		if (ipheth->rx_buffer)
		{
			// ipheth->rx_buffer->buffer is 32-bit aligned
			// ipheth->rx_buffer->buffer + 2 is 16-bit aligned
			// after remove the first 16-bit header, eth packet will be 32-bit aligned
			// NOTE: hcd MUST accept 16-bit aligned transfer buffer
			inurb->transfer_buffer = ipheth->rx_buffer->buffer + 2;
			vsfusbh_submit_urb(ipheth->usbh, inurb);
		}
		break;
	case VSFSM_EVT_URB_COMPLETE:
		if ((inurb->status == URB_OK) && (inurb->actual_length > 2))
		{
			vsfdbg_prints("ipheth_input:" VSFCFG_DEBUG_LINEEND);
			vsfdbg_printb(inurb->transfer_buffer, inurb->actual_length, 1, 16, true, true);

			if (inurb->actual_length == 4)
			{
				uint8_t *buffer = (uint8_t *)inurb->transfer_buffer;
				struct vsfsm_pt_t pt =
				{
					.state = 0,
					.sm = 0,
				};

				if (!ipheth->connected && buffer[2])
				{
					vsfdbg_prints("ipheth: Connected" VSFCFG_DEBUG_LINEEND);
					vsfip_netif_add(&pt, 0, &ipheth->netif);
					if (vsfusbh_ipheth_cb.on_connect != NULL)
						vsfusbh_ipheth_cb.on_connect(vsfusbh_ipheth_cb.param, &ipheth->netif);
				}
				else if (ipheth->connected && !buffer[2])
				{
					vsfdbg_prints("ipheth: Disonnected" VSFCFG_DEBUG_LINEEND);
					if (vsfusbh_ipheth_cb.on_disconnect != NULL)
						vsfusbh_ipheth_cb.on_disconnect(vsfusbh_ipheth_cb.param, &ipheth->netif);
					vsfip_netif_remove(&pt, 0, &ipheth->netif);
				}
				vsfsm_sem_post(&ipheth->carrier_work_sem);
			}
			else if (ipheth->connected)
			{
				ipheth->rx_buffer->buf.buffer += 4;
				ipheth->rx_buffer->buf.size = inurb->actual_length - 2;
				vsfip_buffer_set_netif(ipheth->rx_buffer, &ipheth->netif);
				vsfip_eth_input(ipheth->rx_buffer);
				ipheth->rx_buffer = NULL;
			}
		}
		goto re_submit;
	}
	return NULL;
}

static void vsfusbh_ipheth_timer(void *param)
{
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)param;
	vsfsm_sem_post(&ipheth->carrier_work_sem);
}

static vsf_err_t vsfusbh_ipheth_init(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)pt->user_data;
	struct vsfhcd_urb_t *ctrlurb = ipheth->ctrlurb;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	ctrlurb->notifier_sm = pt->sm;
	ctrlurb->transfer_buffer = ipheth->ctrlbuff;
	vsfsm_sem_init(&ipheth->carrier_work_sem, 0, VSFSM_EVT_USER);
	ipheth->timer = vsftimer_create_cb(5 * 1000, -1, vsfusbh_ipheth_timer, ipheth);

	while (ipheth->timer != NULL)
	{
		if (vsfsm_sem_pend(&ipheth->carrier_work_sem, pt->sm))
			vsfsm_pt_wfe(pt, VSFSM_EVT_USER);

		if (vsfsm_crit_enter(&ipheth->dev->ep0_crit, pt->sm))
			vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

		ctrlurb->transfer_length = 1;
		ctrlurb->pipe = usb_rcvctrlpipe(ctrlurb->hcddev, 0);
		err = vsfusbh_control_msg(ipheth->usbh, ctrlurb, USB_DIR_IN | USB_TYPE_VENDOR,
			IPHETH_REQ_CARRIER_CHECK, 0, ipheth->ifnum);
		if (err != VSFERR_NONE) goto fail;
		vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
		if ((ctrlurb->status != URB_OK) || (ctrlurb->actual_length != 1)) goto fail;

		vsfsm_crit_leave(&ipheth->dev->ep0_crit);

		if (ipheth->carrier != ipheth->ctrlbuff[0])
		{
			ipheth->carrier = (enum ipheth_carrier_t)ipheth->ctrlbuff[0];
			if (ipheth->carrier == IPHETH_CARRIER_ON)
			{
				vsfdbg_prints("ipheth: carrier on" VSFCFG_DEBUG_LINEEND);

				if (vsfsm_crit_enter(&ipheth->dev->ep0_crit, pt->sm))
				vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

				ctrlurb->transfer_length = 4;
				ctrlurb->pipe = usb_rcvctrlpipe(ctrlurb->hcddev, 0);

				err = vsfusbh_control_msg(ipheth->usbh, ctrlurb,
					USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
					IPHETH_REQ_UNKNOWN03, 0, ipheth->ifnum);
				if (err != VSFERR_NONE) goto fail;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if ((ctrlurb->status != URB_OK) || (ctrlurb->actual_length != 4)) goto fail;
				vsfdbg_prints("ipheth: response to unknown request 0x03:" VSFCFG_DEBUG_LINEEND);
				vsfdbg_printb(ctrlurb->transfer_buffer, ctrlurb->actual_length, 1, 16, true, true);

				err = vsfusbh_control_msg(ipheth->usbh, ctrlurb,
					USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
					IPHETH_REQ_UNKNOWN02, 0, ipheth->ifnum);
				if (err != VSFERR_NONE) goto fail;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if ((ctrlurb->status != URB_OK) || (ctrlurb->actual_length != 4)) goto fail;
				vsfdbg_prints("ipheth: response to unknown request 0x02:" VSFCFG_DEBUG_LINEEND);
				vsfdbg_printb(ctrlurb->transfer_buffer, ctrlurb->actual_length, 1, 16, true, true);

				ctrlurb->transfer_length = 0;
				err = vsfusbh_set_interface(ipheth->usbh, ctrlurb, ipheth->ifnum, ipheth->alt);
				if (err != VSFERR_NONE) goto fail;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if (ctrlurb->status != URB_OK) goto fail;

				err = vsfusbh_control_msg(ipheth->usbh, ctrlurb,
					USB_DIR_OUT | USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, 0,
					USB_DIR_IN | usb_pipeendpoint(ipheth->inurb->pipe));
				if (err != VSFERR_NONE) goto fail;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if (ctrlurb->status != URB_OK) goto fail;

				err = vsfusbh_control_msg(ipheth->usbh, ctrlurb,
					USB_DIR_OUT | USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, 0,
					USB_DIR_OUT | usb_pipeendpoint(ipheth->outurb->pipe));
				if (err != VSFERR_NONE) goto fail;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if (ctrlurb->status != URB_OK) goto fail;

				if (!ipheth->netif.macaddr.size)
				{
					ctrlurb->transfer_length = VSFIP_ETH_ADDRLEN;
					ctrlurb->pipe = usb_rcvctrlpipe(ctrlurb->hcddev, 0);
					err = vsfusbh_control_msg(ipheth->usbh, ctrlurb, USB_DIR_IN | USB_TYPE_VENDOR,
							IPHETH_REQ_GETMAC, 0, ipheth->ifnum);
					if (err != VSFERR_NONE) goto fail;
					vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
					if ((ctrlurb->status != URB_OK) || (ctrlurb->actual_length != VSFIP_ETH_ADDRLEN)) goto fail;

					ipheth->netif.macaddr.size = VSFIP_ETH_ADDRLEN;
					memcpy(ipheth->netif.macaddr.addr.s_addr_buf, ctrlurb->transfer_buffer,
							ctrlurb->actual_length);
					vsfdbg_printf("ipheth: MAC is %02X:%02X:%02X:%02X:%02X:%02X" VSFCFG_DEBUG_LINEEND,
						ipheth->netif.macaddr.addr.s_addr_buf[0], ipheth->netif.macaddr.addr.s_addr_buf[1],
						ipheth->netif.macaddr.addr.s_addr_buf[2], ipheth->netif.macaddr.addr.s_addr_buf[3],
						ipheth->netif.macaddr.addr.s_addr_buf[4], ipheth->netif.macaddr.addr.s_addr_buf[5]);
				}

				vsfsm_crit_leave(&ipheth->dev->ep0_crit);

				if (!ipheth->insm.init_state.evt_handler)
				{
					ipheth->insm.init_state.evt_handler = vsfusbh_ipheth_in_evthandler;
					ipheth->insm.user_data = ipheth;
					vsfsm_init(&ipheth->insm);
				}
			}
			else
			{
				vsfdbg_printf("ipheth: carrier off %d" VSFCFG_DEBUG_LINEEND,
								ipheth->carrier);

/*				if (vsfsm_crit_enter(&ipheth->dev->ep0_crit, pt->sm))
					vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

				ctrlurb->transfer_length = 0;
				err = vsfusbh_set_interface(ipheth->usbh, ctrlurb, ipheth->ifnum, 0);
				if (err != VSFERR_NONE) goto fail;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if (ctrlurb->status != URB_OK) goto fail;

				vsfsm_crit_leave(&ipheth->dev->ep0_crit);
*/			}
		}
	}

	vsfsm_pt_end(pt);
	return VSFERR_NONE;
fail:
	vsfsm_crit_leave(&ipheth->dev->ep0_crit);
	return VSFERR_FAIL;
}

static void *vsfusbh_ipheth_probe(struct vsfusbh_t *usbh,
			struct vsfusbh_device_t *dev, struct vsfusbh_ifs_t *ifs,
			const struct vsfusbh_device_id_t *id)
{
	struct usb_interface_desc_t *ifs_desc;
	struct usb_endpoint_desc_t *ep_desc;
	struct vsfusbh_ipheth_t *ipheth;
	uint8_t ep;
	int alt;

	// only support the last configuration
	if (dev->cur_config < dev->device_desc->bNumConfigurations - 1) return NULL;
	// include requested alt and has 2 endpoints
	alt = 2;
	ifs_desc = ifs->alt[alt].ifs_desc;
	ep_desc = ifs->alt[alt].ep_desc;

	ipheth = vsf_bufmgr_malloc(sizeof(*ipheth));
	if (!ipheth) return ipheth;
	memset(ipheth, 0, sizeof(*ipheth));

	ipheth->ctrlurb = vsfusbh_alloc_urb(usbh);
	if (ipheth->ctrlurb == NULL) goto err_free_ipheth;
	ipheth->inurb = vsfusbh_alloc_urb(usbh);
	if (ipheth->inurb == NULL) goto err_free_ctrlurb;
	ipheth->outurb = vsfusbh_alloc_urb(usbh);
	if (ipheth->outurb == NULL) goto err_free_inurb;

	ipheth->ctrlurb->hcddev = &dev->hcddev;
	ipheth->inurb->hcddev = &dev->hcddev;
	ipheth->outurb->hcddev = &dev->hcddev;
	for (int i = 0; i < 2; i++)
	{
		ep = ep_desc->bEndpointAddress;
		if (ep & 0x80)
			ipheth->inurb->pipe = usb_rcvbulkpipe(&dev->hcddev, ep & 0x7F);
		else
			ipheth->outurb->pipe = usb_sndbulkpipe(&dev->hcddev, ep & 0x7F);
		ep_desc = (struct usb_endpoint_desc_t *)((uint32_t)ep_desc + ep_desc->bLength);
	}

	ipheth->netdrv.op = &vsfusbh_ipheth_netdrv_op;
	ipheth->netdrv.param = ipheth;
	ipheth->netif.drv = &ipheth->netdrv;

	ipheth->dev = dev;
	ipheth->usbh = usbh;
	ipheth->dev = dev;
	ipheth->ifnum = ifs_desc->bInterfaceNumber;
	ipheth->alt = ifs_desc->bAlternateSetting;
	ipheth->initpt.user_data = ipheth;
	ipheth->initpt.thread = vsfusbh_ipheth_init;
	vsfsm_pt_init(&ipheth->initsm, &ipheth->initpt);
	return ipheth;

err_free_inurb:
	vsfusbh_free_urb(usbh, &ipheth->inurb);
err_free_ctrlurb:
	vsfusbh_free_urb(usbh, &ipheth->ctrlurb);
err_free_ipheth:
	vsf_bufmgr_free(ipheth);
	return NULL;
}

static void vsfusbh_ipheth_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct vsfusbh_ipheth_t *ipheth = priv;
	struct vsfsm_pt_t pt =
	{
		.state = 0,
		.sm = 0,
	};

	if (ipheth == NULL) return;

	if (ipheth->timer != NULL)
	{
		vsftimer_free(ipheth->timer);
		ipheth->timer = NULL;
	}
	vsfusbh_free_urb(usbh, &ipheth->ctrlurb);
	vsfusbh_free_urb(usbh, &ipheth->inurb);
	vsfusbh_free_urb(usbh, &ipheth->outurb);

	vsfsm_fini(&ipheth->initsm);
	vsfsm_fini(&ipheth->insm);
	vsfsm_fini(&ipheth->outsm);

	// DO NOT free ipheth here, ipheth will be free in vsfusbh_ipheth_netdrv_free
	ipheth->netif.tofree = true;
	if (ipheth->connected)
	{
		if (vsfusbh_ipheth_cb.on_disconnect != NULL)
			vsfusbh_ipheth_cb.on_disconnect(vsfusbh_ipheth_cb.param, &ipheth->netif);
		vsfip_netif_remove(&pt, 0, &ipheth->netif);
	}
	else if (!ipheth->netif.ref)
	{
		vsfusbh_ipheth_netdrv_free(&ipheth->netif);
	}
}

static const struct vsfusbh_device_id_t vsfusbh_ipheth_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_INT_CLASS |
			USB_DEVICE_ID_MATCH_INT_SUBCLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL,
		.idVendor = 0x05AC,
		.bInterfaceClass = 0xFF,
		.bInterfaceSubClass = 0xFD,
		.bInterfaceProtocol = 0x01,
	},
	{0},
};

const struct vsfusbh_class_drv_t vsfusbh_ipheth_drv =
{
	.name = "ipheth",
	.id_table = vsfusbh_ipheth_id_table,
	.probe = vsfusbh_ipheth_probe,
	.disconnect = vsfusbh_ipheth_disconnect,
};

// netdrv
static struct vsfsm_state_t*
vsfusbh_ipheth_netdrv_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)sm->user_data;
	struct vsfhcd_urb_t *outurb = ipheth->outurb;
	struct vsfip_netif_t *netif = &ipheth->netif;
	struct vsfip_buffer_t *tmpbuf;
	struct vsfq_node_t *node;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		outurb->notifier_sm = sm;
	pend_loop:
		if (vsfsm_sem_pend(&netif->output_sem, sm))
		{
			break;
		}
	case VSFSM_EVT_USER_LOCAL:
		node = vsfq_dequeue(&netif->outq);
		tmpbuf = container_of(node, struct vsfip_buffer_t, netif_node);

		if (ipheth->connected)
		{
			ipheth->tx_buffer = tmpbuf;
			outurb->transfer_buffer = tmpbuf->buf.buffer - 2;
			outurb->transfer_length = tmpbuf->buf.size + 2;
			*(uint16_t *)outurb->transfer_buffer = 0;

			vsfdbg_prints("ipheth_output:" VSFCFG_DEBUG_LINEEND);
			vsfdbg_printb(outurb->transfer_buffer, outurb->transfer_length, 1, 16, true, true);

			if (!vsfusbh_submit_urb(ipheth->usbh, outurb))
				break;
		}
	case VSFSM_EVT_URB_COMPLETE:
		vsfip_buffer_release(ipheth->tx_buffer);
		goto pend_loop;
	}
	return NULL;
}

static vsf_err_t vsfusbh_ipheth_netdrv_init(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfip_netif_t *netif = (struct vsfip_netif_t *)pt->user_data;
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)netif->drv->param;

	ipheth->connected = true;
	netif->mac_broadcast.size = netif->macaddr.size;
	memset(netif->mac_broadcast.addr.s_addr_buf, 0xFF, netif->mac_broadcast.size);
	netif->mtu = IPHETH_MTU - VSFIP_ETH_HEADSIZE - 2;
	netif->drv->netif_header_size = VSFIP_ETH_HEADSIZE;
	netif->drv->hwtype = VSFIP_ETH_HWTYPE;

	// start pt to receive output_sem
	ipheth->outsm.init_state.evt_handler = vsfusbh_ipheth_netdrv_evt_handler;
	ipheth->outsm.user_data = ipheth;
	return vsfsm_init(&ipheth->outsm);
}

static vsf_err_t vsfusbh_ipheth_netdrv_fini(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfip_netif_t *netif = (struct vsfip_netif_t *)pt->user_data;
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)netif->drv->param;
	ipheth->connected = false;
	return VSFERR_NONE;
}

static bool vsfip_ipheth_netdrv_available(struct vsfip_netif_t *netif,
	const struct vsfip_ipaddr_t *dest_addr)
{
	struct vsfusbh_ipheth_t *ipheth = (struct vsfusbh_ipheth_t *)netif->drv->param;
	return ipheth->carrier == IPHETH_CARRIER_ON ? vsfip_eth_available(netif, dest_addr) : false;
}

static void vsfusbh_ipheth_netdrv_free(struct vsfip_netif_t *netif)
{
	vsfdbg_printf("ipeth: netif %04X freed\r\n", netif);
	vsf_bufmgr_free(netif);
}
