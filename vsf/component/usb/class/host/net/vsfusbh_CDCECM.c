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

struct vsfusbh_ecm_cb_t vsfusbh_ecm_cb;

struct vsfusbh_ecm_t
{
	struct vsfusbh_t *usbh;
	struct vsfusbh_device_t *dev;
	struct vsfusbh_ifs_t *ifs;

	struct vsfhcd_urb_t *ctrl_urb;
	struct vsfhcd_urb_t *evt_urb;
	struct vsfhcd_urb_t *in_urb;
	struct vsfhcd_urb_t *out_urb;

	uint8_t comm_iface;
	uint8_t data_iface;
	uint8_t iMAC;

	struct vsfip_netif_t netif;
	struct vsfip_netdrv_t netdrv;
	bool netif_inited;
	struct vsfip_buffer_t *tx_buffer;
	struct vsfip_buffer_t *rx_buffer;

	uint8_t evt[16];
	uint16_t max_segment_size;

	struct vsfsm_t sm;
	struct vsfsm_pt_t pt;

	struct vsfsm_t in_sm;
	struct vsfsm_t netdrv_out_sm;
};

static void vsfusbh_ecm_free_urb(struct vsfusbh_t *usbh, struct vsfusbh_ecm_t *ecm)
{
	if (ecm->ctrl_urb != NULL)
		vsfusbh_free_urb(usbh, &ecm->ctrl_urb);
	if (ecm->evt_urb != NULL)
		vsfusbh_free_urb(usbh, &ecm->evt_urb);
	if (ecm->in_urb != NULL)
		vsfusbh_free_urb(usbh, &ecm->in_urb);
	if (ecm->out_urb != NULL)
		vsfusbh_free_urb(usbh, &ecm->out_urb);
}

int hex_to_bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return -1;
}

static struct vsfsm_state_t *
vsfusbh_ecm_in_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbh_ecm_t *ecm = (struct vsfusbh_ecm_t *)sm->user_data;
	struct vsfusbh_t *usbh = ecm->usbh;
	struct vsfhcd_urb_t *urb = ecm->in_urb;
	vsf_err_t err;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		if (!ecm->rx_buffer)
			ecm->rx_buffer = vsfip_buffer_get(ecm->max_segment_size);

		if (ecm->rx_buffer != NULL)
		{
			urb->transfer_buffer = ecm->rx_buffer->buffer;
			urb->transfer_length = ecm->max_segment_size;
			err = vsfusbh_submit_urb(usbh, urb);
			if (err != VSFERR_NONE)
			{
				// TODO
			}
		}
		break;
	case VSFSM_EVT_URB_COMPLETE:
		if (urb->status == URB_OK)
		{
			vsfdbg_printf("cdc_ecm_input: ");
			vsfdbg_printb(urb->transfer_buffer, urb->actual_length, true);

			ecm->rx_buffer->netif = &ecm->netif;
			vsfip_eth_input(ecm->rx_buffer);
			ecm->rx_buffer = vsfip_buffer_get(ecm->max_segment_size);
		}
		else
		{
			// TODO
		}

		if (ecm->rx_buffer != NULL)
		{
			urb->transfer_buffer = ecm->rx_buffer->buffer;
			urb->transfer_length = ecm->max_segment_size;
			err = vsfusbh_submit_urb(usbh, urb);
			if (err != VSFERR_NONE)
			{
				// TODO
			}
		}
		break;
	default:
		break;
	}
	return NULL;
}

static void vsfusbh_ecm_on_event(struct vsfusbh_ecm_t *ecm)
{
	switch (ecm->evt[1])
	{
	case 0x00:			// NETWORK_CONNECTION
		if (ecm->netif_inited && (ecm->evt[2] == 0))
		{
			struct vsfsm_pt_t pt;

			vsfdbg_prints("cdc_ecm_event: NETWORK_CONNECTION Disconnected" VSFCFG_DEBUG_LINEEND);

			// disconnect netif, for ecm netif_remove is non-block
			pt.state = 0;
			pt.sm = 0;
			pt.user_data = &ecm->netif;
			vsfip_netif_remove(&pt, 0, &ecm->netif);
			ecm->netif_inited = false;
			if (vsfusbh_ecm_cb.on_disconnect != NULL)
				vsfusbh_ecm_cb.on_disconnect(vsfusbh_ecm_cb.param, &ecm->netif);
		}
		else if (!ecm->netif_inited && (ecm->evt[2] != 0))
		{
			struct vsfsm_pt_t pt;

			vsfdbg_prints("cdc_ecm_event: NETWORK_CONNECTION Connected" VSFCFG_DEBUG_LINEEND);

			// connect netif, for ecm netif_add is non-block
			pt.state = 0;
			pt.sm = 0;
			pt.user_data = &ecm->netif;
			if (!vsfip_netif_add(&pt, 0, &ecm->netif))
			{
				ecm->netif_inited = true;
				if (vsfusbh_ecm_cb.on_connect != NULL)
					vsfusbh_ecm_cb.on_connect(vsfusbh_ecm_cb.param, &ecm->netif);
			}
		}
		break;
	case 0x2A:			// CONNECTION_SPEED_CHANGE
//		vsfdbg_prints("cdc_ecm_event: CONNECTION_SPEED_CHANGE" VSFCFG_DEBUG_LINEEND);
		break;
	default:
		vsfdbg_printf("cdc_ecm_event: unknown(%d)" VSFCFG_DEBUG_LINEEND, ecm->evt[0]);
		break;
	}
}

static vsf_err_t vsfusbh_ecm_init_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfusbh_ecm_t *ecm = (struct vsfusbh_ecm_t *)pt->user_data;
	struct vsfusbh_t *usbh = ecm->usbh;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	if (vsfsm_crit_enter(&ecm->dev->ep0_crit, pt->sm))
		vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

	if (!vsfusbh_alloc_urb_buffer(ecm->ctrl_urb, 2 + 12 * 2)) goto ret_failure;
	err = vsfusbh_get_descriptor(usbh, ecm->ctrl_urb, USB_DT_STRING, ecm->iMAC);
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	if ((ecm->ctrl_urb->status != URB_OK) ||
			(ecm->ctrl_urb->actual_length != ecm->ctrl_urb->transfer_length))
		goto ret_failure;

	{
		char *str = (char *)ecm->ctrl_urb->transfer_buffer + 2;
		for (int i = 0; i < 6; i++, str += 4)
			ecm->netif.macaddr.addr.s_addr_buf[i] = (hex_to_bin(str[0]) << 4) | (hex_to_bin(str[2]) << 0);
	}
	vsfusbh_free_urb_buffer(ecm->ctrl_urb);
	ecm->netif.macaddr.size = 6;
	vsfdbg_printf("cdc_ecm: MAC is %02X:%02X:%02X:%02X:%02X:%02X" VSFCFG_DEBUG_LINEEND,
		ecm->netif.macaddr.addr.s_addr_buf[0], ecm->netif.macaddr.addr.s_addr_buf[1],
		ecm->netif.macaddr.addr.s_addr_buf[2], ecm->netif.macaddr.addr.s_addr_buf[3],
		ecm->netif.macaddr.addr.s_addr_buf[4], ecm->netif.macaddr.addr.s_addr_buf[5]);

	err = vsfusbh_set_interface(usbh, ecm->ctrl_urb, ecm->data_iface, 0);
	if (err) goto ret_error;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	if (ecm->ctrl_urb->status != URB_OK) goto ret_failure;

	err = vsfusbh_set_interface(usbh, ecm->ctrl_urb, ecm->data_iface, 1);
	if (err) goto ret_error;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	if (ecm->ctrl_urb->status != URB_OK) goto ret_failure;

	vsfsm_crit_leave(&ecm->dev->ep0_crit);

	ecm->in_sm.init_state.evt_handler = vsfusbh_ecm_in_evt_handler;
	ecm->in_sm.user_data = ecm;
	vsfsm_init(&ecm->in_sm);

	if (ecm->evt_urb != NULL)
	{
		ecm->evt_urb->transfer_buffer = ecm->evt;
		ecm->evt_urb->transfer_length = sizeof(ecm->evt);
		err = vsfusbh_submit_urb(usbh, ecm->evt_urb);
		if (err) goto ret_error;
		vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
		if (ecm->evt_urb->status != URB_OK) goto ret_failure;
		vsfusbh_ecm_on_event(ecm);

		while (1)
		{
			err = vsfusbh_relink_urb(usbh, ecm->evt_urb);
			if (err) goto ret_error;
			vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
			if (ecm->evt_urb->status != URB_OK) goto ret_failure;
			vsfusbh_ecm_on_event(ecm);
		}
	}

	vsfsm_pt_end(pt);
	return VSFERR_NONE;

ret_failure:
	err = VSFERR_FAIL;
ret_error:
	vsfusbh_remove_interface(ecm->usbh, ecm->dev, ecm->ifs);
	return err;
}

// netdrv
static struct vsfsm_state_t*
vsfusbh_ecm_netdrv_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbh_ecm_t *ecm = (struct vsfusbh_ecm_t *)sm->user_data;
	struct vsfq_node_t *node;
	struct vsfip_buffer_t *tmpbuf;
	struct vsfhcd_urb_t *urb = ecm->out_urb;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
	pend_loop:
		if (vsfsm_sem_pend(&ecm->netif.output_sem, sm))
		{
			break;
		}
	case VSFSM_EVT_USER_LOCAL:
		node = vsfq_dequeue(&ecm->netif.outq);
		tmpbuf = container_of(node, struct vsfip_buffer_t, netif_node);

		ecm->tx_buffer = tmpbuf;
		urb->transfer_buffer = tmpbuf->buf.buffer;
		urb->transfer_length = tmpbuf->buf.size;
		vsfdbg_printf("cdc_ecm_output: ");
		vsfdbg_printb(urb->transfer_buffer, urb->transfer_length, true);
		if (!vsfusbh_submit_urb(ecm->usbh, urb))
			break;
	case VSFSM_EVT_URB_COMPLETE:
		vsfip_buffer_release(ecm->tx_buffer);
		goto pend_loop;
	}
	return NULL;
}

static vsf_err_t vsfusbh_ecm_netdrv_init(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfip_netif_t *netif = (struct vsfip_netif_t *)pt->user_data;
	struct vsfusbh_ecm_t *ecm = (struct vsfusbh_ecm_t *)netif->drv->param;

	netif->mac_broadcast.size = netif->macaddr.size;
	memset(netif->mac_broadcast.addr.s_addr_buf, 0xFF, netif->macaddr.size);
	netif->mtu = ecm->max_segment_size - VSFIP_ETH_HEADSIZE;
	netif->drv->netif_header_size = VSFIP_ETH_HEADSIZE;
	netif->drv->hwtype = VSFIP_ETH_HWTYPE;

	// start pt to receive output_sem
	ecm->netdrv_out_sm.init_state.evt_handler = vsfusbh_ecm_netdrv_evt_handler;
	ecm->netdrv_out_sm.user_data = ecm;
	return vsfsm_init(&ecm->netdrv_out_sm);
}

static vsf_err_t vsfusbh_ecm_netdrv_fini(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	((struct vsfusbh_ecm_t *)pt->user_data)->netif_inited = false;
	return VSFERR_NONE;
}

static struct vsfip_netdrv_op_t vsfusbh_ecm_netdrv_op =
{
	vsfusbh_ecm_netdrv_init, vsfusbh_ecm_netdrv_fini, vsfip_eth_header
};

static void *vsfusbh_ecm_probe(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, struct vsfusbh_ifs_t *ifs,
		const struct vsfusbh_device_id_t *id)
{
	struct vsfusbh_ifs_alt_t *alt = &ifs->alt[ifs->cur_alt];
	struct usb_interface_desc_t *ifs_desc = alt->ifs_desc;
	struct usb_endpoint_desc_t *ep_desc = alt->ep_desc;
	struct vsfusbh_ifs_t *data_ifs;
	struct usb_interface_desc_t *data_ifs_desc;
	struct usb_endpoint_desc_t *data_ep_desc;
	struct usb_class_interface_descriptor_t *desc;
	struct vsfusbh_ecm_t *ecm;
	uint8_t epaddr, eptype;
	uint8_t parsed_size;

	ecm = vsf_bufmgr_malloc(sizeof(struct vsfusbh_ecm_t));
	if (ecm == NULL)
		return NULL;
	memset(ecm, 0, sizeof(struct vsfusbh_ecm_t));

	ecm->netdrv.op = &vsfusbh_ecm_netdrv_op;
	ecm->netdrv.param = ecm;
	ecm->netif.drv = &ecm->netdrv;

	ecm->ctrl_urb = vsfusbh_alloc_urb(usbh);
	ecm->in_urb = vsfusbh_alloc_urb(usbh);
	ecm->out_urb = vsfusbh_alloc_urb(usbh);
	if (!ecm->ctrl_urb || !ecm->in_urb || !ecm->out_urb)
		goto free_all;
	ecm->ctrl_urb->hcddev = &dev->hcddev;
	ecm->ctrl_urb->notifier_sm = &ecm->sm;
	ecm->ctrl_urb->pipe = usb_rcvctrlpipe(&dev->hcddev, 0);
	ecm->in_urb->hcddev = &dev->hcddev;
	ecm->in_urb->notifier_sm = &ecm->in_sm;
	ecm->out_urb->hcddev = &dev->hcddev;
	ecm->out_urb->notifier_sm = &ecm->netdrv_out_sm;
	ecm->out_urb->transfer_flags = URB_ZERO_PACKET;

	ecm->comm_iface = ifs_desc->bInterfaceNumber;
	if (ifs_desc->bNumEndpoints == 1)
	{
		epaddr = ep_desc->bEndpointAddress;
		eptype = ep_desc->bmAttributes;
		if ((USB_ENDPOINT_XFER_INT == eptype) && (epaddr & USB_ENDPOINT_DIR_MASK))
		{
			ecm->evt_urb = vsfusbh_alloc_urb(usbh);
			if (!ecm->evt_urb) goto free_all;
			ecm->evt_urb->hcddev = &dev->hcddev;
			ecm->evt_urb->notifier_sm = &ecm->sm;
			ecm->evt_urb->pipe = usb_rcvintpipe(&dev->hcddev, epaddr & 0xF);
		}
	}

	desc = (struct usb_class_interface_descriptor_t *)ifs_desc;
	for (parsed_size = 0; parsed_size < alt->desc_size;)
	{
		if (desc->bDescriptorType == USB_DT_CS_INTERFACE)
		{
			switch (desc->bDescriptorSubType)
			{
			case 0x06:		// Union Functional Descriptor
				{
					struct usb_cdc_union_descriptor_t *union_desc =
						(struct usb_cdc_union_descriptor_t *)desc;
					if (union_desc->bControlInterface != ecm->comm_iface)
						goto free_all;
					ecm->data_iface = union_desc->bSubordinateInterface[0];
				}
				break;
			case 0x0F:		// Ethernet Networking Functional Descriptor
				{
					struct usb_cdc_ecm_descriptor_t *ecm_desc =
						(struct usb_cdc_ecm_descriptor_t *)desc;
					ecm->iMAC = ecm_desc->iMACAddress;
					ecm->max_segment_size = GET_LE_U16(ecm_desc->wMaxSegmentSize);
					if (ecm->max_segment_size > (VSFIP_CFG_MTU + VSFIP_CFG_NETIF_HEADLEN))
						goto free_all;
				}
				break;
			}
		}
		parsed_size += desc->bLength;
		desc = (struct usb_class_interface_descriptor_t *)((uint32_t)desc + desc->bLength);
	}

	data_ifs = &dev->config.ifs[ecm->data_iface];
	alt = data_ifs->alt;
	for (int i = 0; i < data_ifs->num_of_alt; i++, alt++)
	{
		data_ifs_desc = alt->ifs_desc;
		data_ep_desc = alt->ep_desc;
		if ((data_ifs_desc->bDescriptorType == USB_DT_INTERFACE) &&
			(data_ifs_desc->bInterfaceClass == USB_CLASS_CDC_DATA) &&
			(data_ifs_desc->bInterfaceSubClass == 0) &&
			(data_ifs_desc->bInterfaceProtocol == 0))
		{
			if (data_ifs_desc->bNumEndpoints == 0)
				continue;

			for (int j = 0; j < data_ifs_desc->bNumEndpoints; j++)
			{
				epaddr = data_ep_desc->bEndpointAddress;
				eptype = data_ep_desc->bmAttributes;
				if (eptype != USB_ENDPOINT_XFER_BULK)
					goto free_all;
				if (epaddr & USB_ENDPOINT_DIR_MASK)
					ecm->in_urb->pipe = usb_rcvbulkpipe(&dev->hcddev, epaddr & 0xF);
				else
					ecm->out_urb->pipe = usb_sndbulkpipe(&dev->hcddev, epaddr & 0xF);

				data_ep_desc = (struct usb_endpoint_desc_t *)\
							((uint32_t)data_ep_desc + data_ep_desc->bLength);
			}
			break;
		}
	}

	ecm->usbh = usbh;
	ecm->dev = dev;
	ecm->ifs = ifs;

	ecm->pt.thread = vsfusbh_ecm_init_thread;
	ecm->pt.user_data = ecm;
	vsfsm_pt_init(&ecm->sm, &ecm->pt);

	return ecm;
free_all:
	vsfusbh_ecm_free_urb(usbh, ecm);
	vsf_bufmgr_free(ecm);
	return NULL;
}

static void vsfusbh_ecm_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct vsfusbh_ecm_t *ecm = (struct vsfusbh_ecm_t *)priv;

	vsfusbh_ecm_free_urb(usbh, ecm);
	vsfsm_fini(&ecm->in_sm);
	vsfsm_fini(&ecm->sm);
	vsfsm_fini(&ecm->netdrv_out_sm);
	vsf_bufmgr_free(ecm);
}

static const struct vsfusbh_device_id_t vsfusbh_ecm_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
			USB_DEVICE_ID_MATCH_INT_SUBCLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL,
		.bInterfaceClass = USB_CLASS_COMM,
		.bInterfaceSubClass = 6,		// ECM
		.bInterfaceProtocol = 0x00,
	},
	{0},
};

const struct vsfusbh_class_drv_t vsfusbh_ecm_drv =
{
	.name = "cdc_ecm",
	.id_table = vsfusbh_ecm_id_table,
	.probe = vsfusbh_ecm_probe,
	.disconnect = vsfusbh_ecm_disconnect,
};

