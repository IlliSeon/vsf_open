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

struct vsfusbh_msc_global_t vsfusbh_msc;

static vsf_err_t vsfusbh_msc_init_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfusbh_msc_t *msc = (struct vsfusbh_msc_t *)pt->user_data;
	struct vsfhcd_urb_t *urb = msc->urb;

	vsfsm_pt_begin(pt);

	msc->max_lun = 0;

	do
	{
		if (vsfsm_crit_enter(&msc->dev->ep0_crit, pt->sm))
			vsfsm_pt_wfe(pt, VSFSM_EVT_EP0_CRIT);

		urb->pipe = usb_rcvctrlpipe(urb->hcddev, 0);
		urb->transfer_buffer = &msc->max_lun;
		urb->transfer_length = 1;
		if (vsfusbh_control_msg(msc->usbh, urb,
				USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				USB_MSCBOTREQ_GET_MAX_LUN, 0, msc->iface))
			urb->status = URB_FAIL;
		else
			vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);

		vsfsm_crit_leave(&msc->dev->ep0_crit);
		if (urb->status != URB_OK)
			return VSFERR_FAIL;
	} while (!msc->max_lun);

	if (vsfusbh_msc.on_new != NULL)
		msc->scsi_dev = vsfusbh_msc.on_new(msc->max_lun,
				(struct vsfscsi_lun_op_t *)&vsfusbh_msc_scsi_op, msc);
	vsfsm_pt_end(pt);

	return VSFERR_NONE;
}

static struct vsfsm_state_t *vsfusbh_msc_evt_handler_init(struct vsfsm_t *sm,
		vsfsm_evt_t evt)
{
	struct vsfusbh_msc_t *msc = (struct vsfusbh_msc_t *)sm->user_data;

	switch (evt)
	{
	case VSFSM_EVT_INIT:
		msc->pt.thread = vsfusbh_msc_init_thread;
		msc->pt.user_data = msc;
		msc->pt.sm = sm;
		msc->pt.state = 0;
	case VSFSM_EVT_URB_COMPLETE:
	case VSFSM_EVT_DELAY_DONE:
		if (msc->pt.thread(&msc->pt, evt) < 0)
			vsfusbh_remove_interface(msc->usbh, msc->dev, msc->ifs);
		break;
	default:
		break;
	}
	return NULL;
}

static void *vsfusbh_msc_probe(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, struct vsfusbh_ifs_t *ifs,
		const struct vsfusbh_device_id_t *id)
{
	struct usb_interface_desc_t *ifs_desc = ifs->alt[ifs->cur_alt].ifs_desc;
	struct usb_endpoint_desc_t *ep_desc = ifs->alt[ifs->cur_alt].ep_desc;
	struct vsfusbh_msc_t *msc;
	uint8_t ep;
	int i;

	if (ifs_desc->bNumEndpoints != 2)
		return NULL;

	msc = vsf_bufmgr_malloc(sizeof(struct vsfusbh_msc_t));
	if (msc == NULL)
		return NULL;
	memset(msc, 0, sizeof(struct vsfusbh_msc_t));

	msc->urb = vsfusbh_alloc_urb(usbh);
	if (msc->urb == NULL)
	{
		vsf_bufmgr_free(msc);
		return NULL;
	}
	msc->urb->notifier_sm = &msc->sm;

	msc->iface = ifs_desc->bInterfaceNumber;
	for (i = 0; i < 2; i++)
	{
		ep = ep_desc->bEndpointAddress;
		if (ep & 0x80)
			msc->ep_in = ep & 0x7F;
		else
			msc->ep_out = ep & 0x7F;
		ep_desc = (struct usb_endpoint_desc_t *)((uint32_t)ep_desc + ep_desc->bLength);
	}

	msc->executing = 0;
	msc->CBW.dCBWSignature = SYS_TO_LE_U32(USBMSC_CBW_SIGNATURE);

	msc->usbh = usbh;
	msc->dev = dev;
	msc->ifs = ifs;
	msc->urb->hcddev = &dev->hcddev;
	msc->sm.init_state.evt_handler = vsfusbh_msc_evt_handler_init;
	msc->sm.user_data = msc;
	vsfsm_init(&msc->sm);

	return msc;
}

static void vsfusbh_msc_on_inout(void *p);
static void vsfusbh_msc_disconnect(struct vsfusbh_t *usbh,
		struct vsfusbh_device_t *dev, void *priv)
{
	struct vsfusbh_msc_t *msc = (struct vsfusbh_msc_t *)priv;
	struct vsfscsi_lun_t *lun;

	if (msc == NULL)
		return;

	if (msc->scsi_dev != NULL)
	{
		int i;
		msc->scsi_dev->transact.err = VSFERR_FAIL;
		lun = msc->scsi_dev->lun;
		for (i = 0; i < msc->scsi_dev->max_lun; i++)
		{
			if ((lun[i].stream != NULL) &&
				(lun[i].stream->callback_rx.on_inout == vsfusbh_msc_on_inout))
			{
				if (lun[i].stream->rx_ready)
					STREAM_DISCONNECT_RX(lun->stream);
				else if (lun[i].stream->tx_ready)
					STREAM_DISCONNECT_TX(lun->stream);
			}
			lun[i].param = NULL;
		}
		if (vsfusbh_msc.on_delete)
			vsfusbh_msc.on_delete(msc->scsi_dev);
	}

	if (msc->urb != NULL)
		vsfusbh_free_urb(usbh, &msc->urb);
	vsfsm_fini(&msc->sm);
	vsf_bufmgr_free(msc);
}

static const struct vsfusbh_device_id_t vsfusbh_msc_id_table[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
			USB_DEVICE_ID_MATCH_INT_SUBCLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL,
		.bInterfaceClass = USB_CLASS_MASS_STORAGE,
		.bInterfaceSubClass = 6,		// SCSI transport
		.bInterfaceProtocol = 0x50,		// Bulk Only Transport: BBB Protocol
	},
	{0},
};

static void vsfusbh_msc_on_inout(void *p)
{
	struct vsfusbh_msc_t *msc = (struct vsfusbh_msc_t *)p;
	if (msc->inout_notifier)
		vsfsm_post_evt_pending(msc->inout_notifier, VSFSM_EVT_USER);
}

static vsf_err_t vsfusbh_msc_exe_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfscsi_lun_t *lun = (struct vsfscsi_lun_t *)pt->user_data;
	struct vsfusbh_msc_t *msc = (struct vsfusbh_msc_t *)lun->param;
	struct vsfhcd_urb_t *urb = msc->urb;
	bool send;
	uint32_t tmp32, remain;
	uint16_t epsize;

again:
	send = msc->CBW.bmCBWFlags > 0;
	epsize = send ? msc->dev->ep_mps_out[msc->ep_out] :
						msc->dev->ep_mps_in[msc->ep_in];
	vsfsm_pt_begin(pt);

	if (send)
	{
		lun->stream->callback_rx.param = msc;
		lun->stream->callback_rx.on_inout = vsfusbh_msc_on_inout;
		STREAM_CONNECT_RX(lun->stream);
	}
	else
	{
		lun->stream->callback_rx.param = msc;
		lun->stream->callback_rx.on_inout = vsfusbh_msc_on_inout;
		STREAM_CONNECT_TX(lun->stream);
	}

	urb->pipe = usb_sndbulkpipe(urb->hcddev, msc->ep_in);
	urb->transfer_buffer = &msc->CBW;
	urb->transfer_length = sizeof(msc->CBW);
	if (vsfusbh_submit_urb(msc->usbh, urb))
		goto fail_disconnect;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
	if (urb->status != URB_OK)
		goto fail_disconnect;

	urb->pipe = send ? usb_sndbulkpipe(urb->hcddev, msc->ep_out) :
						usb_rcvbulkpipe(urb->hcddev, msc->ep_in);
	lun->dev->transact.data_size = 0;
	while (lun->dev->transact.data_size < msc->CBW.dCBWDataTransferLength)
	{
		if (send)
		{
			while (1)
			{
				msc->cur_size = STREAM_GET_RBUF(lun->stream, &msc->cur_ptr);
				remain = msc->CBW.dCBWDataTransferLength -
								lun->dev->transact.data_size;
				tmp32 = min(epsize, remain);
				if (msc->cur_size >= tmp32)
					break;
				msc->inout_notifier = pt->sm;
				vsfsm_pt_wfe(pt, VSFSM_EVT_USER);
				msc->inout_notifier = NULL;
			}

			if (msc->cur_size < remain)
				msc->cur_size &= epsize - 1;
			urb->transfer_buffer = msc->cur_ptr;
			urb->transfer_length = msc->cur_size;
			if (vsfusbh_submit_urb(msc->usbh, urb))
				goto fail_disconnect;
			vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
			if (urb->status != URB_OK)
				goto fail_disconnect;

			{
				struct vsf_buffer_t buffer =
				{
					.buffer = NULL,
					.size = msc->cur_size,
				};
				STREAM_READ(lun->stream, &buffer);
			}
		}
		else
		{
			while (1)
			{
				msc->cur_size = STREAM_GET_WBUF(lun->stream, &msc->cur_ptr);
				remain = msc->CBW.dCBWDataTransferLength -
								lun->dev->transact.data_size;
				tmp32 = min(epsize, remain);
				if (msc->cur_size >= tmp32)
					break;
				msc->inout_notifier = pt->sm;
				vsfsm_pt_wfe(pt, VSFSM_EVT_USER);
				msc->inout_notifier = NULL;
			}

			if (msc->cur_size < remain)
				msc->cur_size &= epsize - 1;
			urb->transfer_buffer = &msc->cur_ptr;
			urb->transfer_length = msc->cur_size;
			if (vsfusbh_submit_urb(msc->usbh, urb))
				goto fail_disconnect;
			vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);
			if (urb->status != URB_OK)
				goto fail_disconnect;

			{
				struct vsf_buffer_t buffer =
				{
					.buffer = NULL,
					.size = msc->cur_size,
				};
				STREAM_WRITE(lun->stream, &buffer);
			}
		}
		lun->dev->transact.data_size += msc->cur_size;
		if (msc->cur_size < epsize)
			break;
	}
	if (send)
		STREAM_DISCONNECT_RX(lun->stream);
	else
		STREAM_DISCONNECT_TX(lun->stream);

	urb->pipe = usb_rcvbulkpipe(urb->hcddev, msc->ep_out);
	urb->transfer_buffer = &msc->CSW;
	urb->transfer_length = sizeof(msc->CSW);
	if (vsfusbh_submit_urb(msc->usbh, urb))
		goto fail;
	vsfsm_pt_wfe(pt, VSFSM_EVT_URB_COMPLETE);

	if ((urb->status != URB_OK) ||
		(msc->CSW.dCSWSignature != SYS_TO_LE_U32(USBMSC_CSW_SIGNATURE)) ||
		(msc->CSW.dCSWTag != msc->CBW.dCBWTag))
		goto fail;

	if (--msc->executing)
	{
		pt->state = 0;
		lun->dev->transact.data_size = 0;
		goto again;
	}
	vsfsm_pt_end(pt);

	return VSFERR_NONE;
fail_disconnect:
	lun->dev->transact.err = VSFERR_FAIL;
	if (send)
		STREAM_DISCONNECT_RX(lun->stream);
	else
		STREAM_DISCONNECT_TX(lun->stream);
	return VSFERR_FAIL;
fail:
	lun->dev->transact.err = VSFERR_FAIL;
	return VSFERR_FAIL;
}

static vsf_err_t vsfusbh_msc_execute(struct vsfscsi_lun_t *lun, uint8_t *CDB,
									uint8_t CDB_size, uint32_t size)
{
	struct vsfusbh_msc_t *msc = (struct vsfusbh_msc_t *)lun->param;
	if (!msc || (msc->executing > 1))
		return VSFERR_NOT_AVAILABLE;

	msc->CBW.bCBWLUN = lun - msc->scsi_dev->lun;
	if (msc->CBW.bCBWLUN >= msc->scsi_dev->max_lun)
		return VSFERR_NOT_AVAILABLE;

	msc->CBW.dCBWTag++;
	msc->CBW.dCBWDataTransferLength = size & ~(1UL << 31);
	msc->CBW.bmCBWFlags = size >> 31 ? 0x80 : 0x00;
	msc->CBW.bCBWCBLength = CDB_size;
	memcpy(msc->CBW.CBWCB, CDB, msc->CBW.bCBWCBLength);
	if (++msc->executing == 1)
	{
		lun->dev->transact.data_size = 0;
		lun->dev->transact.err = VSFERR_NONE;
		lun->dev->transact.lun = lun;
		msc->pt.thread = vsfusbh_msc_exe_thread;
		msc->pt.user_data = lun;
		vsfsm_pt_init(&msc->sm, &msc->pt);
	}
	return VSFERR_NONE;
}

const struct vsfusbh_class_drv_t vsfusbh_msc_drv =
{
	.name = "msc",
	.id_table = vsfusbh_msc_id_table,
	.probe = vsfusbh_msc_probe,
	.disconnect = vsfusbh_msc_disconnect,
};

const struct vsfscsi_lun_op_t vsfusbh_msc_scsi_op =
{
	.execute = vsfusbh_msc_execute,
};

