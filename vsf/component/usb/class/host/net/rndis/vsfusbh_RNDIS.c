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
#include "../../../common/CDC/vsfusb_RNDIS.h"

struct vsfusbh_rndis_cb_t vsfusbh_rndis_cb;

struct vsfusbh_rndis_t
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

	struct vsfip_netif_t netif;
	struct vsfip_netdrv_t netdrv;
	bool connected;
	struct vsfip_buffer_t *tx_buffer;
	struct vsfip_buffer_t *rx_buffer;

	uint32_t curxid;
	uint32_t curmsg;
	uint8_t control_buffer[1025];
	uint8_t cmdretry;
	uint16_t max_segment_size;

	struct vsfsm_t sm;
	struct vsfsm_pt_t pt;
	struct vsfsm_pt_t cmdpt;

	struct vsfsm_t in_sm;
	struct vsfsm_t netdrv_out_sm;
};

static void vsfusbh_rndis_free_urb(struct vsfusbh_t *usbh, struct vsfusbh_rndis_t *rndis)
{
	if (rndis->ctrl_urb != NULL)
		vsfusbh_free_urb(usbh, &rndis->ctrl_urb);
	if (rndis->evt_urb != NULL)
		vsfusbh_free_urb(usbh, &rndis->evt_urb);
	if (rndis->in_urb != NULL)
		vsfusbh_free_urb(usbh, &rndis->in_urb);
	if (rndis->out_urb != NULL)
		vsfusbh_free_urb(usbh, &rndis->out_urb);
}

static struct vsfsm_state_t *
vsfusbh_rndis_in_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)sm->user_data;
	struct vsfusbh_t *usbh = rndis->usbh;
	struct vsfhcd_urb_t *urb = rndis->in_urb;
	vsf_err_t err;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		if (!rndis->rx_buffer)
			rndis->rx_buffer = vsfip_buffer_get(rndis->max_segment_size);

		if (rndis->rx_buffer != NULL)
		{
			urb->transfer_buffer = rndis->rx_buffer->buffer;
			urb->transfer_length = rndis->max_segment_size;
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
			vsfdbg_printf("cdc_rndis_input: ");
			vsfdbg_printb(urb->transfer_buffer, urb->actual_length, 1, 16, true, true);

			rndis->rx_buffer->buf.size = urb->actual_length;
			if (rndis->connected)
			{
				uint32_t msglen = rndis->rx_buffer->buf.size;

//				while (msglen)
				{
					struct rndis_data_packet_t *data =
						(struct rndis_data_packet_t *)rndis->rx_buffer->buf.buffer;

					data->DataLength = LE_TO_SYS_U32(data->DataLength);
					data->DataOffset = LE_TO_SYS_U32(data->DataOffset);
					data->head.MessageType.value = LE_TO_SYS_U32(data->head.MessageType.value);
					data->head.MessageLength = LE_TO_SYS_U32(data->head.MessageLength);

					if ((data->head.MessageType.msg == RNDIS_PACKET_MSG) &&
						(data->DataOffset + data->DataLength + 8 == data->head.MessageLength) &&
						(data->head.MessageLength == msglen))
					{
						vsfip_buffer_set_netif(rndis->rx_buffer, &rndis->netif);
						vsfip_eth_input(rndis->rx_buffer);
						rndis->rx_buffer = vsfip_buffer_get(rndis->max_segment_size);
					}
				}
			}
		}
		else
		{
			// TODO
		}

		if (rndis->rx_buffer != NULL)
		{
			urb->transfer_buffer = rndis->rx_buffer->buffer;
			urb->transfer_length = rndis->max_segment_size;
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

static void vsfusbh_rndis_prepare_request(struct vsfusbh_rndis_t *rndis,
	struct rndis_requesthead_t *request, enum rndis_ctrlmsg_t msg, uint32_t msglen)
{
	request->head.MessageType.value = SYS_TO_LE_U32(msg);
	rndis->curmsg = msg;
	request->head.MessageLength = msglen;
	if ((msg != RNDIS_HALT_MSG) && (msg != RNDIS_RESET_MSG))
	{
		request->RequestId = ++rndis->curxid;
		if (!request->RequestId)
			request->RequestId = ++rndis->curxid;
	}
}

static void vsfusbh_rndis_prepare_query(struct vsfusbh_rndis_t *rndis,
	struct rndis_query_msg_t *query, uint32_t oid, uint32_t inlen)
{
	vsfusbh_rndis_prepare_request(rndis, &query->request, RNDIS_QUERY_MSG,
		sizeof(*query) + inlen);
	query->Oid.value = SYS_TO_LE_U32(oid);
	query->InformationBufferLength = SYS_TO_LE_U32(inlen);
	query->InformationBufferOffset = SYS_TO_LE_U32(sizeof(*query) - 8);
}

static vsf_err_t vsfusbh_rndis_command(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)pt->user_data;
	struct rndis_msghead_t *head = (struct rndis_msghead_t *)rndis->control_buffer;
	union rndis_msg_t *msg = (union rndis_msg_t *)rndis->control_buffer;
	struct vsfhcd_urb_t *ctrlurb = rndis->ctrl_urb;
	struct vsfhcd_urb_t *evturb = rndis->evt_urb;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	if (vsfsm_crit_enter(&rndis->dev->ep0_crit, pt->sm))
		vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

	ctrlurb->pipe = usb_sndctrlpipe(ctrlurb->hcddev, 0);
	ctrlurb->transfer_buffer = (uint8_t *)head;
	ctrlurb->transfer_length = LE_TO_SYS_U32(head->MessageLength);
	vsfdbg_printf("cdc_rndis_command: ");
	vsfdbg_printb(ctrlurb->transfer_buffer, ctrlurb->transfer_length, 1, 16, true, true);
	err = vsfusbh_control_msg(rndis->usbh, ctrlurb, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			USB_CDCREQ_SEND_ENCAPSULATED_COMMAND, 0, rndis->comm_iface);
	if (err) goto ret_error_crit;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	if (ctrlurb->status != URB_OK) goto ret_failure_crit;

	if (evturb != NULL)
	{
		evturb->transfer_buffer = rndis->control_buffer;
		evturb->transfer_length = 8;	// cdc_notification
		err = vsfusbh_submit_urb(rndis->usbh, evturb);
		if (err) goto ret_error_crit;
		vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
		if (evturb->status != URB_OK) goto ret_failure_crit;
	}

	for (rndis->cmdretry = 0; rndis->cmdretry < 10; rndis->cmdretry++)
	{
		ctrlurb->pipe = usb_rcvctrlpipe(ctrlurb->hcddev, 0);
		ctrlurb->transfer_length = sizeof(rndis->control_buffer);
		err = vsfusbh_control_msg(rndis->usbh, ctrlurb, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				USB_CDCREQ_GET_ENCAPSULATED_RESPONSE, 0, rndis->comm_iface);
		if (err) goto ret_error_crit;
		vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
		if (ctrlurb->status != URB_OK) goto ret_failure_crit;

		if (ctrlurb->actual_length >= sizeof(msg->head))
		{
			uint32_t rsp = LE_TO_SYS_U32(msg->head.MessageType.value);
			if ((rndis->curmsg | RNDIS_COMPLETION) == rsp)
			{
				if (rndis->curxid == msg->reply.request.RequestId)
				{
					if ((rsp == RNDIS_RESET_CMPLT) || (msg->reply.status.value == SYS_TO_LE_U32(NDIS_STATUS_SUCCESS)))
						break;
					goto ret_failure_crit;
				}
			}
			else switch (rsp)
			{
			case RNDIS_INDICATE_STATUS_MSG:
				switch (LE_TO_SYS_U32(msg->reply.status.value))
				{
				case NDIS_STATUS_MEDIA_CONNECT:
					break;
				case NDIS_STATUS_MEDIA_DISCONNECT:
					break;
				}
				break;
			case RNDIS_KEEPALIVE_MSG:
				msg->keepalive_cmplt.reply.request.head.MessageType.value = SYS_TO_LE_U32(RNDIS_KEEPALIVE_CMPLT);
				msg->keepalive_cmplt.reply.request.head.MessageLength = SYS_TO_LE_U32(sizeof(msg->keepalive_cmplt));
				msg->keepalive_cmplt.reply.status.value = SYS_TO_LE_U32(NDIS_STATUS_SUCCESS);
				ctrlurb->pipe = usb_sndctrlpipe(ctrlurb->hcddev, 0);
				ctrlurb->transfer_length = LE_TO_SYS_U32(msg->head.MessageLength);
				err = vsfusbh_control_msg(rndis->usbh, ctrlurb, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
						USB_CDCREQ_SEND_ENCAPSULATED_COMMAND, 0, rndis->comm_iface);
				if (err) goto ret_error_crit;
				vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
				if (ctrlurb->status != URB_OK) goto ret_failure_crit;
				break;
			}
		}
	}

	vsfsm_crit_leave(&rndis->dev->ep0_crit);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;

ret_error_crit:
	vsfsm_crit_leave(&rndis->dev->ep0_crit);
	goto ret_error;
ret_failure_crit:
	vsfsm_crit_leave(&rndis->dev->ep0_crit);
	err = VSFERR_FAIL;
ret_error:
	return err;
}

static vsf_err_t vsfusbh_rndis_init_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)pt->user_data;
	union rndis_msg_t *msg = (union rndis_msg_t *)rndis->control_buffer;
	uint8_t *buf;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	rndis->cmdpt.user_data = rndis;
	rndis->cmdpt.thread = vsfusbh_rndis_command;

	vsfusbh_rndis_prepare_request(rndis, &msg->init_msg.request,
			RNDIS_INITIALIZE_MSG, sizeof(msg->init_msg));
	msg->init_msg.MajorVersion = SYS_TO_LE_U32(1);
	msg->init_msg.MinorVersion = SYS_TO_LE_U32(0);
	msg->init_msg.MaxTransferSize = SYS_TO_LE_U32(sizeof(struct rndis_data_packet_t) + VSFIP_CFG_MTU);
	vsfsm_pt_wfpt_errcode(pt, &rndis->cmdpt, __err,
	{
		err = __err;
		goto ret_error;
	});
	rndis->max_segment_size = LE_TO_SYS_U32(msg->init_cmplt.MaxTransferSize);
	rndis->max_segment_size = min(rndis->max_segment_size, sizeof(struct rndis_data_packet_t) + VSFIP_CFG_MTU);

	vsfusbh_rndis_prepare_query(rndis, &msg->query_msg, OID_802_3_PERMANENT_ADDRESS, VSFIP_ETH_ADDRLEN);
	vsfsm_pt_wfpt_errcode(pt, &rndis->cmdpt, __err,
	{
		err = __err;
		goto ret_error;
	});

	rndis->netif.macaddr.size = VSFIP_ETH_ADDRLEN;
	buf = (uint8_t *)msg + msg->query_cmplt.InformationBufferOffset;
	memcpy(rndis->netif.macaddr.addr.s_addr_buf, buf, VSFIP_ETH_ADDRLEN);
	vsfdbg_printf("cdc_rndis: MAC is %02X:%02X:%02X:%02X:%02X:%02X" VSFCFG_DEBUG_LINEEND,
		rndis->netif.macaddr.addr.s_addr_buf[0], rndis->netif.macaddr.addr.s_addr_buf[1],
		rndis->netif.macaddr.addr.s_addr_buf[2], rndis->netif.macaddr.addr.s_addr_buf[3],
		rndis->netif.macaddr.addr.s_addr_buf[4], rndis->netif.macaddr.addr.s_addr_buf[5]);

	vsfusbh_rndis_prepare_request(rndis, &msg->set_msg.request,
			RNDIS_SET_MSG, sizeof(msg->set_msg) + 4);
	msg->set_msg.Oid.value = SYS_TO_LE_U32(OID_GEN_CURRENT_PACKET_FILTER);
	msg->set_msg.InformationBufferLength = SYS_TO_LE_U32(4);
	msg->set_msg.InformationBufferOffset = SYS_TO_LE_U32(sizeof(msg->set_msg) - 8);
	buf = msg->buf + sizeof(msg->set_msg);
	*(uint32_t *)buf = SYS_TO_LE_U32(NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST);
	vsfsm_pt_wfpt_errcode(pt, &rndis->cmdpt, __err,
	{
		err = __err;
		goto ret_error;
	});

	{
		struct vsfsm_pt_t pt =
		{
			.state = 0,
			.sm = 0,
		};
		if (!vsfip_netif_add(&pt, 0, &rndis->netif) && (vsfusbh_rndis_cb.on_connect != NULL))
			vsfusbh_rndis_cb.on_connect(vsfusbh_rndis_cb.param, &rndis->netif);
	}

	rndis->in_sm.init_state.evt_handler = vsfusbh_rndis_in_evt_handler;
	rndis->in_sm.user_data = rndis;
	vsfsm_init(&rndis->in_sm);

	vsfsm_pt_end(pt);
	return VSFERR_NONE;

ret_error:
	vsfusbh_remove_interface(rndis->usbh, rndis->dev, rndis->ifs);
	return err;
}

// netdrv
static struct vsfsm_state_t*
vsfusbh_rndis_netdrv_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)sm->user_data;
	struct vsfq_node_t *node;
	struct vsfip_buffer_t *tmpbuf;
	struct vsfhcd_urb_t *urb = rndis->out_urb;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
	pend_loop:
		if (vsfsm_sem_pend(&rndis->netif.output_sem, sm))
		{
			break;
		}
	case VSFSM_EVT_USER_LOCAL:
		node = vsfq_dequeue(&rndis->netif.outq);
		tmpbuf = container_of(node, struct vsfip_buffer_t, netif_node);

		rndis->tx_buffer = tmpbuf;
		urb->transfer_buffer = tmpbuf->buf.buffer;
		urb->transfer_length = tmpbuf->buf.size;
		vsfdbg_printf("cdc_rndis_output: ");
		vsfdbg_printb(urb->transfer_buffer, urb->transfer_length, 1, 16, true, true);
		if (!vsfusbh_submit_urb(rndis->usbh, urb))
			break;
	case VSFSM_EVT_URB_COMPLETE:
		vsfip_buffer_release(rndis->tx_buffer);
		goto pend_loop;
	}
	return NULL;
}

static vsf_err_t vsfusbh_rndis_netdrv_init(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfip_netif_t *netif = (struct vsfip_netif_t *)pt->user_data;
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)netif->drv->param;

	netif->mac_broadcast.size = netif->macaddr.size;
	memset(netif->mac_broadcast.addr.s_addr_buf, 0xFF, netif->macaddr.size);
	netif->mtu = rndis->max_segment_size - VSFIP_ETH_HEADSIZE - sizeof(struct rndis_data_packet_t);
	netif->drv->netif_header_size = VSFIP_ETH_HEADSIZE + sizeof(struct rndis_data_packet_t);
	netif->drv->hwtype = VSFIP_ETH_HWTYPE;
	rndis->connected = true;

	// start pt to receive output_sem
	rndis->netdrv_out_sm.init_state.evt_handler = vsfusbh_rndis_netdrv_evt_handler;
	rndis->netdrv_out_sm.user_data = rndis;
	return vsfsm_init(&rndis->netdrv_out_sm);
}

static vsf_err_t vsfusbh_rndis_netdrv_fini(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfip_netif_t *netif = (struct vsfip_netif_t *)pt->user_data;
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)netif->drv->param;
	rndis->connected = false;
	return VSFERR_NONE;
}

static vsf_err_t vsfip_rndis_netdrv_header(struct vsfip_buffer_t *buf,
		enum vsfip_netif_proto_t proto, const struct vsfip_macaddr_t *dest_addr)
{
	struct rndis_data_packet_t *data;
	uint32_t origlen;
	vsf_err_t err = vsfip_eth_header(buf, proto, dest_addr);
	if (!err) return err;

	if (buf->buf.buffer - buf->buffer < sizeof(struct rndis_data_packet_t))
		return VSFERR_FAIL;

	origlen = buf->buf.size;
	buf->buf.buffer -= sizeof(*data);
	buf->buf.size += sizeof(*data);

	data = (struct rndis_data_packet_t *)buf->buf.buffer;
	memset(data, 0, sizeof(*data));
	data->head.MessageType.value = SYS_TO_LE_U32(RNDIS_PACKET_MSG);
	data->head.MessageLength = SYS_TO_LE_U32(buf->buf.size);
	data->DataOffset = SYS_TO_LE_U32(sizeof(data) - 8);
	data->DataLength = origlen;
	return VSFERR_NONE;
}

static void vsfusbh_rndis_netdrv_free(struct vsfip_netif_t *netif)
{
	vsfdbg_printf("cdc_rndis: netif %04X freed\r\n", netif);
	vsf_bufmgr_free(netif);
}

static struct vsfip_netdrv_op_t vsfusbh_rndis_netdrv_op =
{
	vsfusbh_rndis_netdrv_init, vsfusbh_rndis_netdrv_fini, vsfusbh_rndis_netdrv_free,
	vsfip_rndis_netdrv_header, vsfip_eth_available
};

static void *vsfusbh_rndis_probe(struct vsfusbh_t *usbh,
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
	struct vsfusbh_rndis_t *rndis;
	uint8_t epaddr, eptype;
	uint8_t parsed_size;

	rndis = vsf_bufmgr_malloc(sizeof(struct vsfusbh_rndis_t));
	if (rndis == NULL)
		return NULL;
	memset(rndis, 0, sizeof(struct vsfusbh_rndis_t));

	rndis->netdrv.op = &vsfusbh_rndis_netdrv_op;
	rndis->netdrv.param = rndis;
	rndis->netif.drv = &rndis->netdrv;

	rndis->ctrl_urb = vsfusbh_alloc_urb(usbh);
	rndis->in_urb = vsfusbh_alloc_urb(usbh);
	rndis->out_urb = vsfusbh_alloc_urb(usbh);
	if (!rndis->ctrl_urb || !rndis->in_urb || !rndis->out_urb)
		goto free_all;
	rndis->ctrl_urb->hcddev = &dev->hcddev;
	rndis->ctrl_urb->notifier_sm = &rndis->sm;
	rndis->in_urb->hcddev = &dev->hcddev;
	rndis->in_urb->notifier_sm = &rndis->in_sm;
	rndis->out_urb->hcddev = &dev->hcddev;
	rndis->out_urb->notifier_sm = &rndis->netdrv_out_sm;
	rndis->out_urb->transfer_flags = URB_ZERO_PACKET;

	rndis->comm_iface = ifs_desc->bInterfaceNumber;
	if (ifs_desc->bNumEndpoints == 1)
	{
		epaddr = ep_desc->bEndpointAddress;
		eptype = ep_desc->bmAttributes;
		if ((USB_ENDPOINT_XFER_INT == eptype) && (epaddr & USB_ENDPOINT_DIR_MASK))
		{
			rndis->evt_urb = vsfusbh_alloc_urb(usbh);
			if (!rndis->evt_urb) goto free_all;
			rndis->evt_urb->hcddev = &dev->hcddev;
			rndis->evt_urb->notifier_sm = &rndis->sm;
			rndis->evt_urb->pipe = usb_rcvintpipe(&dev->hcddev, epaddr & 0xF);
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
					if (union_desc->bControlInterface != rndis->comm_iface)
						goto free_all;
					rndis->data_iface = union_desc->bSubordinateInterface[0];
				}
				break;
			}
		}
		parsed_size += desc->bLength;
		desc = (struct usb_class_interface_descriptor_t *)((uint32_t)desc + desc->bLength);
	}

	data_ifs = &dev->config.ifs[rndis->data_iface];
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
					rndis->in_urb->pipe = usb_rcvbulkpipe(&dev->hcddev, epaddr & 0xF);
				else
					rndis->out_urb->pipe = usb_sndbulkpipe(&dev->hcddev, epaddr & 0xF);

				data_ep_desc = (struct usb_endpoint_desc_t *)\
							((uint32_t)data_ep_desc + data_ep_desc->bLength);
			}
			break;
		}
	}

	rndis->usbh = usbh;
	rndis->dev = dev;
	rndis->ifs = ifs;

	rndis->pt.thread = vsfusbh_rndis_init_thread;
	rndis->pt.user_data = rndis;
	vsfsm_pt_init(&rndis->sm, &rndis->pt);

	return rndis;
free_all:
	vsfusbh_rndis_free_urb(usbh, rndis);
	vsf_bufmgr_free(rndis);
	return NULL;
}

static void vsfusbh_rndis_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct vsfusbh_rndis_t *rndis = (struct vsfusbh_rndis_t *)priv;

	vsfusbh_rndis_free_urb(usbh, rndis);
	vsfsm_fini(&rndis->in_sm);
	vsfsm_fini(&rndis->sm);
	vsfsm_fini(&rndis->netdrv_out_sm);

	// DO NOT free rndis here, rndis will be free in vsfusbh_rndis_netdrv_free
	rndis->netif.tofree = true;
	if (rndis->connected)
	{
		struct vsfsm_pt_t pt =
		{
			.state = 0,
			.sm = 0,
			.user_data = &rndis->netif,
		};

		if (vsfusbh_rndis_cb.on_disconnect != NULL)
			vsfusbh_rndis_cb.on_disconnect(vsfusbh_rndis_cb.param, &rndis->netif);
		vsfip_netif_remove(&pt, 0, &rndis->netif);
	}
	else if (!rndis->netif.ref)
	{
		vsfusbh_rndis_netdrv_free(&rndis->netif);
	}
}

static const struct vsfusbh_device_id_t vsfusbh_rndis_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
			USB_DEVICE_ID_MATCH_INT_SUBCLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL,
		.bInterfaceClass = USB_CLASS_WIRELESS_CONTROLLER,
		.bInterfaceSubClass = 1,
		.bInterfaceProtocol = 3,
	},
	{0},
};

const struct vsfusbh_class_drv_t vsfusbh_rndis_drv =
{
	.name = "cdc_rndis",
	.id_table = vsfusbh_rndis_id_table,
	.probe = vsfusbh_rndis_probe,
	.disconnect = vsfusbh_rndis_disconnect,
};

