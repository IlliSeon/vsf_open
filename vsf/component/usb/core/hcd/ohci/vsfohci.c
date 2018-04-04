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
#include "vsfohci_priv.h"

#define OHCI_ISO_DELAY			2

#define CC_TO_ERROR(cc) (cc == 0 ? VSFERR_NONE : -cc)

static struct td_t *td_alloc(struct vsfohci_t *ohci)
{
	struct td_t *td = ohci->td_pool;
	for (uint32_t i = 0; i < TD_MAX_NUM; i++, td++)
	{
		if (!td->busy)
		{
			memset(td, 0, sizeof(struct td_t));
			td->busy = 1;
			return td;
		}
	}
	return NULL;
}

static void td_free(struct td_t *td)
{
	td->busy = 0;
}

static int8_t ed_balance(struct vsfohci_t *ohci, uint8_t interval, uint8_t load)
{
	int8_t i, j, branch = -1;

	// iso periods can be huge; iso tds specify frame numbers
	if (interval > NUM_INTS)
		interval = NUM_INTS;

	// search for the least loaded schedule branch of that period
	// that has enough bandwidth left unreserved.
	for (i = 0; i < interval; i++)
	{
		if (branch < 0 || ohci->load[branch] < ohci->load[i])
		{
			for (j = i; j < NUM_INTS; j += interval)
			{
				// usb 1.1 says 90% of one frame
				if (ohci->load[j] + load > 900)
					break;
			}
			if (j < NUM_INTS)
				continue;
			branch = i;
		}
	}
	return branch;
}

static void periodic_link(struct vsfohci_t *ohci, struct ed_t *ed)
{
	for (int8_t i = ed->branch; i < NUM_INTS; i += ed->interval)
	{
		struct ed_t **prev = (struct ed_t **)&ohci->hcca->int_table[i];
		struct ed_t *here = *prev;

		// sorting each branch by period (slow before fast)
		while (here && ed != here)
		{
			if (ed->interval > here->interval)
				break;
			prev = (struct ed_t **)&here->hwNextED;
			here = *prev;
		}
		if (ed != here)
		{
			ed->hwNextED = (uint32_t)here;
			*prev = ed;
		}
		ohci->load[i] += ed->load;
	}
}

static void periodic_unlink(struct vsfohci_t *ohci, struct ed_t *ed)
{
	for (int8_t i = ed->branch; i < NUM_INTS; i += ed->interval)
	{
		struct ed_t **prev = (struct ed_t **)&ohci->hcca->int_table[i];

		while (*prev && (*prev != ed))
			prev = (struct ed_t **)&((*prev)->hwNextED);
		if (*prev)
			*prev = (struct ed_t *)ed->hwNextED;
		ohci->load[i] -= ed->load;
	}
}

// link an ed into one of the HC chains
static vsf_err_t ed_schedule(struct vsfohci_t *ohci, struct urb_priv_t *urb_priv)
{
	struct ohci_regs_t *regs = ohci->regs;
	struct ed_t *ed = urb_priv->ed;
	ed->hwNextED = 0;

	switch (ed->type)
	{
	case PIPE_CONTROL:
		if (ohci->ed_controltail == NULL)
			regs->ed_controlhead = (uint32_t)ed;
		else
			ohci->ed_controltail->hwNextED = (uint32_t)ed;
		ed->prev = ohci->ed_controltail;
		if (!ohci->ed_controltail && !ohci->ed_rm_list)
		{
			ohci->hc_control |= OHCI_CTRL_CLE;
			regs->ed_controlcurrent = 0;
			regs->control = ohci->hc_control;
		}
		ohci->ed_controltail = ed;
		break;
	case PIPE_BULK:
		if (ohci->ed_bulktail == NULL)
			regs->ed_bulkhead = (uint32_t)ed;
		else
			ohci->ed_bulktail->hwNextED = (uint32_t)ed;
		ed->prev = ohci->ed_bulktail;
		if (!ohci->ed_bulktail && !ohci->ed_rm_list)
		{
			ohci->hc_control |= OHCI_CTRL_BLE;
			regs->ed_bulkcurrent = 0;
			regs->control = ohci->hc_control;
		}
		ohci->ed_bulktail = ed;
		break;
	case PIPE_INTERRUPT:
	case PIPE_ISOCHRONOUS:
		if ((ed->interval == 0) || (ed->interval > 32))
			ed->interval = 32;
		ed->branch = ed_balance(ohci, ed->interval, ed->load);
		if (ed->branch < 0)
			return VSFERR_NOT_ENOUGH_RESOURCES;
		periodic_link(ohci, ed);
		break;
	}
	urb_priv->state |= URB_PRIV_EDLINK;
	return VSFERR_NONE;
}

static void ed_deschedule(struct vsfohci_t *ohci, struct urb_priv_t *urb_priv)
{
	struct ohci_regs_t *regs = ohci->regs;
	struct ed_t *ed = urb_priv->ed;
	ed->hwINFO |= ED_SKIP;
	urb_priv->state |= URB_PRIV_EDSKIP;

	switch (ed->type)
	{
	case PIPE_CONTROL:
		if (ed->prev == NULL)
		{
			if (!ed->hwNextED)
			{
				ohci->hc_control &= ~OHCI_CTRL_CLE;
				regs->control = ohci->hc_control;
			}
			else
				regs->ed_controlhead = ed->hwNextED;
		}
		else
			ed->prev->hwNextED = ed->hwNextED;
		if (ohci->ed_controltail == ed)
			ohci->ed_controltail = ed->prev;
		else
			((struct ed_t *)(ed->hwNextED))->prev = ed->prev;
		break;
	case PIPE_BULK:
		if (ed->prev == NULL)
		{
			if (!ed->hwNextED)
			{
				ohci->hc_control &= ~OHCI_CTRL_BLE;
				regs->control = ohci->hc_control;
			}
			else
				regs->ed_bulkhead = ed->hwNextED;
		}
		else
			ed->prev->hwNextED = ed->hwNextED;
		if (ohci->ed_bulktail == ed)
			ohci->ed_bulktail = ed->prev;
		else
			((struct ed_t *)(ed->hwNextED))->prev = ed->prev;
		break;
	case PIPE_INTERRUPT:
	case PIPE_ISOCHRONOUS:
		periodic_unlink(ohci, ed);
		break;
	}
	urb_priv->state &= ~URB_PRIV_EDLINK;
}

static vsf_err_t ed_init(struct vsfohci_t *ohci,
		struct urb_priv_t *urb_priv, struct vsfhcd_urb_t *urb)
{
	struct ed_t *ed = urb_priv->ed;
	struct td_t *td = NULL;
	uint32_t pipe;

	if (ed == NULL)
		return VSFERR_FAIL;
	memset(ed, 0, sizeof(struct ed_t));
	pipe = urb->pipe;

	/* dummy td; end of td list for ed */
	td = td_alloc(ohci);
	if (!td)
		return VSFERR_NOT_ENOUGH_RESOURCES;
	td->urb_priv = urb_priv;
	ed->dummy_td = td;
	ed->hwTailP = (uint32_t)td;
	ed->hwHeadP = ed->hwTailP;
	if (usb_gettoggle(urb->hcddev, usb_pipeendpoint(pipe), usb_pipeout(pipe)))
		ed->hwHeadP |= ED_C;
	ed->type = usb_pipetype(pipe);
	ed->hwINFO = usb_pipedevice(pipe)
			| usb_pipeendpoint(pipe) << 7
			| (usb_pipeisoc(pipe) ? ED_ISO : 0)
			| (usb_pipeslow(pipe) ? ED_LOWSPEED : 0)
			| urb->packet_size << 16;

	if (ed->type != PIPE_CONTROL)
	{
		ed->hwINFO |= usb_pipeout(pipe) ? ED_OUT : ED_IN;

		if (ed->type != PIPE_BULK)
		{
			uint32_t interval = urb->interval;
			ed->interval = usb_pipeisoc(pipe) ? interval : min(interval, 32);
			ed->load = 1;
		}
	}
	return VSFERR_NONE;
}

static void ed_fini(struct urb_priv_t *urb_priv)
{
	struct ed_t *ed = urb_priv->ed;
	for (uint32_t i = 0; i < urb_priv->length; i++)
	{
		if (urb_priv->td[i] != NULL)
		{
			td_free(urb_priv->td[i]);
			urb_priv->td[i] = NULL;
		}
	}
	urb_priv->length = 0;
	urb_priv->state &= ~URB_PRIV_TDALLOC;

	td_free(ed->dummy_td);
	ed->dummy_td = NULL;
}

static void td_fill(uint32_t info, void *data, uint16_t len, uint16_t index,
		struct urb_priv_t *urb_priv)
{
	struct td_t *td, *td_pt;

	if (index > urb_priv->length)
		return;

	td_pt = urb_priv->td[index];

	// fill the old dummy TD
	td = urb_priv->td[index] = urb_priv->ed->dummy_td;
	urb_priv->ed->dummy_td = td_pt;

	td_pt->urb_priv = urb_priv;

	td->next_dl_td = NULL;
	td->index = index;

	td->hwINFO = info;
	td->hwCBP = (uint32_t)((!data || !len) ? 0 : data);
	td->hwBE = (uint32_t)((!data || !len) ? 0 : (uint32_t)data + len - 1);
	td->hwNextTD = (uint32_t)td_pt;

	urb_priv->ed->hwTailP = td->hwNextTD;
}

#if OHCI_ENABLE_ISO
static void iso_td_fill(uint32_t info, void *data, uint16_t len, uint16_t index,
		struct urb_priv_t *urb_priv)
{
	struct td_t *td, *td_pt;
	uint32_t bufferStart;

	if (index > urb_priv->length)
		return;

	td_pt = urb_priv->td[index];

	// fill the old dummy TD
	td = urb_priv->td[index] = (struct td_t *)\
			((uint32_t)urb_priv->ed->hwTailP & 0xfffffff0);
	td->next_dl_td = NULL;
	td->index = index;
	td->urb_priv = urb_priv;

	bufferStart = (uint32_t)data + urb_priv->iso_frame_desc[index].offset;
	len = urb_priv->iso_frame_desc[index].length;

	td->hwINFO = info;
	td->hwCBP = (uint32_t)((!bufferStart || !len) ? 0 : bufferStart) & 0xfffff000;
	td->hwBE = (uint32_t)((!bufferStart || !len) ? 0 : (uint32_t)bufferStart + len - 1);
	td->hwNextTD = (uint32_t)td_pt;

	td->hwPSW[0] = ((uint32_t)data + urb_priv->iso_frame_desc[index].offset) & 0x0FFF | 0xE000;

	td_pt->hwNextTD = 0;
	urb_priv->ed->hwTailP = td->hwNextTD;
}
#endif // OHCI_ENABLE_ISO

static void td_submit_urb(struct vsfohci_t *ohci, struct vsfhcd_urb_t *urb)
{
	uint32_t data_len, info, cnt = 0, n;
	void *data;
	struct urb_priv_t *urb_priv = (struct urb_priv_t *)urb->priv;
	struct ohci_regs_t *regs = ohci->regs;
	uint8_t isout = usb_pipeout(urb->pipe);

	data = urb->transfer_buffer;
	data_len = urb->transfer_length;
	urb_priv->td_cnt = 0;

	switch (usb_pipetype(urb->pipe))
	{
	case PIPE_CONTROL:
		info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
		td_fill(info, (void *)&urb->setup_packet, 8, cnt++, urb_priv);
		if (data_len > 0)
		{
			info = isout ? TD_CC | TD_R | TD_DP_OUT | TD_T_DATA1 :
					TD_CC | TD_R | TD_DP_IN | TD_T_DATA1;
			td_fill(info, data, data_len, cnt++, urb_priv);
		}
		info = (isout || data_len == 0) ? (TD_CC | TD_DP_IN | TD_T_DATA1) :
					(TD_CC | TD_DP_OUT | TD_T_DATA1);
		td_fill(info, NULL, 0, cnt++, urb_priv);
		regs->cmdstatus = OHCI_CLF;
		break;
	case PIPE_INTERRUPT:
	case PIPE_BULK:
		info = isout ? (TD_T_TOGGLE | TD_CC | TD_DP_OUT) :
					(TD_T_TOGGLE | TD_CC | TD_DP_IN);
		while (data_len)
		{
			n = min(data_len, 4096);
			data_len -= n;

			if (!data_len && !(urb->transfer_flags & URB_SHORT_NOT_OK))
				info |= isout ? 0 : TD_R;
			td_fill(info, data, n, cnt++, urb_priv);
			data = (void *)((uint32_t)data + n);
		}
		if ((urb->transfer_flags & URB_ZERO_PACKET) && (cnt < urb_priv->length))
			td_fill(info, 0, 0, cnt++, urb_priv);
		if (usb_pipetype(urb->pipe) == PIPE_BULK)
			regs->cmdstatus = OHCI_BLF;
		break;
#if OHCI_ENABLE_ISO
	case PIPE_ISOCHRONOUS:
		for (cnt = urb_priv->td_cnt; cnt < urb_priv->number_of_packets; cnt++)
		{
			iso_td_fill(TD_CC | TD_ISO | ((urb->start_frame + cnt) & 0xffff),
					data, data_len, cnt, urb_priv);
		}
		break;
#endif // OHCI_ENABLE_ISO
	}
	urb_priv->state |= URB_PRIV_TDLINK;
}

static void update_done_list(struct vsfohci_t *ohci)
{
	uint8_t cc;
	struct td_t *td = NULL, *td_next = NULL;

	td_next = (struct td_t *)(ohci->hcca->done_head & 0xfffffff0);
	ohci->hcca->done_head = 0;

	while (td_next)
	{
		td = td_next;
		td_next = (struct td_t *)(td->hwNextTD & 0xfffffff0);

		td->hwINFO |= TD_DEL;
		cc = TD_CC_GET(td->hwINFO);

		// cc get error and ed halted
		if ((cc != TD_CC_NOERROR) && (td->urb_priv->ed->hwHeadP & ED_H))
		{
			uint8_t i;
			struct urb_priv_t *urb_priv = td->urb_priv;
			struct ed_t *ed = urb_priv->ed;

			ed->hwINFO |= ED_SKIP;
			urb_priv->state |= URB_PRIV_EDSKIP;
			ed->hwHeadP &= ~ED_H;

			for(i = 0; i < urb_priv->length; i++)
			{
				if (urb_priv->td[i] == td)
				{
					urb_priv->td_cnt += urb_priv->length - 1 - i;
				}
			}
		}

		// add to done list
		td->next_dl_td = ohci->dl_start;
		ohci->dl_start = td;
	}
}

static void finish_unlinks(struct vsfohci_t *ohci)
{
	struct ohci_regs_t *regs = ohci->regs;
	uint8_t frame = ohci->hcca->frame_no & 0x01;
	struct ed_t *ed, **last, *del_edlist = NULL;
	uint32_t cmd = 0, ctrl = 0;
	uint32_t deleted = 0;

	for(last = &ohci->ed_rm_list, ed = *last; ed != NULL; ed = *last)
	{
		if (ed->rm_frame == frame)
		{
			struct urb_priv_t *urb_priv = ed->dummy_td->urb_priv;
			struct vsfhcd_urb_t *urb = container_of(urb_priv,
					struct vsfhcd_urb_t, priv);

			usb_settoggle(urb->hcddev, usb_pipeendpoint(urb->pipe),
						usb_pipeout(urb->pipe), (ed->hwHeadP & ED_C) >> 1);
			if (urb_priv->state & URB_PRIV_WAIT_DELETE)
			{
				if (ohci->ed_del_list != NULL)
				{
					last = &ed->ed_next;
					continue;
				}
				*last = ed->ed_next;
				ed->ed_next = del_edlist;
				del_edlist = ed;
				ohci->ed_del_list = ed;
				deleted++;
			}
			else if (urb_priv->state & URB_PRIV_WAIT_COMPLETE)
			{
				*last = ed->ed_next;
				ed_fini(urb_priv);
				urb_priv->state &= ~(URB_PRIV_EDSKIP | URB_PRIV_WAIT_COMPLETE);

				vsfsm_post_evt_pending(urb->notifier_sm, VSFSM_EVT_URB_COMPLETE);
			}
		}
		else
		{
			last = &ed->ed_next;
		}
	}

	if (deleted)
	{
		ohci->ed_del_list = del_edlist;
		vsfsm_post_evt_pending(&ohci->urb_free_sm, VSFSM_EVT_USER_LOCAL);
	}

	if (!ohci->ed_rm_list)
	{
		if (ohci->ed_controltail)
		{
			cmd |= OHCI_CLF;
			if (!(ohci->hc_control & OHCI_CTRL_CLE))
			{
				ctrl |= OHCI_CTRL_CLE;
				regs->ed_controlcurrent = 0;
			}
		}
		if (ohci->ed_bulktail)
		{
			cmd |= OHCI_BLF;
			if (!(ohci->hc_control & OHCI_CTRL_BLE))
			{
				ctrl |= OHCI_CTRL_BLE;
				regs->ed_bulkcurrent = 0;
			}
		}
		if (ctrl)
		{
			ohci->hc_control |= ctrl;
			regs->control = ohci->hc_control;
		}
		if (cmd)
			regs->cmdstatus = cmd;
	}
}

static vsf_err_t td_done(struct vsfhcd_urb_t *urb, struct td_t *td)
{
	struct urb_priv_t *urb_priv = (struct urb_priv_t *)urb->priv;
	int32_t cc = 0, err = VSFERR_NONE;

#if OHCI_ENABLE_ISO
	if (td->hwINFO & TD_ISO)
	{
		uint16_t tdPSW = td->hwPSW[0] & 0xffff;
		uint32_t dlen;

		if (td->hwINFO & TD_CC)
			return VSFERR_FAIL;

		cc = (tdPSW >> 12) & 0xf;
		if (usb_pipeout(urb->pipe))
			dlen = urb_priv->iso_frame_desc[td->index].length;
		else
		{
			if (cc == TD_DATAUNDERRUN)
				cc = TD_CC_NOERROR;
			dlen = tdPSW & 0x3ff;
		}
		urb->actual_length += dlen;
		urb_priv->iso_frame_desc[td->index].actual_length = dlen;
		urb_priv->iso_frame_desc[td->index].status = CC_TO_ERROR(cc);
	}
	else
#endif // OHCI_ENABLE_ISO
	{
		cc = TD_CC_GET(td->hwINFO);
		if ((cc == TD_DATAUNDERRUN) && !(urb->transfer_flags & URB_SHORT_NOT_OK))
			cc = TD_CC_NOERROR;
		// clear toggle carry if stalled, endpoint will be reset later
		if (cc == TD_CC_STALL)
			urb_priv->ed->hwHeadP &= ~ED_C;
		if ((cc != TD_CC_NOERROR) && (cc < 0x0e))
			err = CC_TO_ERROR(cc);

		if (((usb_pipetype(urb->pipe) != PIPE_CONTROL) || (td->index != 0)) &&
				(td->hwBE != 0))
		{
			if (td->hwCBP == 0)
				urb->actual_length = td->hwBE - (uint32_t)urb->transfer_buffer + 1;
			else
				urb->actual_length = td->hwCBP - (uint32_t)urb->transfer_buffer;
		}
	}

	return err;
}

static void start_ed_unlink(struct vsfohci_t *ohci, struct urb_priv_t *urb_priv)
{
	struct ohci_regs_t *regs = ohci->regs;
	struct ed_t *ed = urb_priv->ed;

	if (ed->hwINFO & ED_DEQUEUE)
		return;

	ed->hwINFO |= ED_DEQUEUE;
	ed_deschedule(ohci, urb_priv);

	ed->ed_next = ohci->ed_rm_list;
	ohci->ed_rm_list = ed;
	ed->prev = NULL;

	regs->intrstatus = OHCI_INTR_SF;
	regs->intrenable = OHCI_INTR_SF;

	ed->rm_frame = (ohci->hcca->frame_no + 1) & 0x1;
}

static void takeback_td(struct vsfohci_t *ohci, struct td_t *td)
{
	vsf_err_t err;
	struct urb_priv_t *urb_priv = td->urb_priv;
	struct vsfhcd_urb_t *urb = container_of(urb_priv, struct vsfhcd_urb_t, priv);

	err = td_done(urb, td);
	urb_priv->td_cnt++;

	if (urb_priv->td_cnt >= urb_priv->length)
	{
		urb_priv->state &= ~URB_PRIV_TDLINK;
		urb->status = err;

		if (urb_priv->state & URB_PRIV_WAIT_DELETE)
		{
			start_ed_unlink(ohci, urb_priv);
		}
		else
		{
			if ((usb_pipetype(urb->pipe) == PIPE_BULK) ||
					(usb_pipetype(urb->pipe) == PIPE_CONTROL))
			{
				// pend complete event after ed unlink
				urb_priv->state |= URB_PRIV_WAIT_COMPLETE;
				start_ed_unlink(ohci, urb_priv);
			}
			else
			{
				vsfsm_post_evt_pending(urb->notifier_sm, VSFSM_EVT_URB_COMPLETE);
			}
		}
	}
}

static void ohci_work(struct vsfohci_t *ohci)
{
	struct td_t *td;
	while (ohci->dl_start)
	{
		td = ohci->dl_start;
		ohci->dl_start = td->next_dl_td;
		takeback_td(ohci, td);
	}

	if (ohci->ed_rm_list)
		finish_unlinks(ohci);
}

static void vsfohci_interrupt(void *param)
{
	struct vsfohci_t *ohci = (struct vsfohci_t *)param;
	struct ohci_regs_t *regs = ohci->regs;
	uint32_t intrstatus = regs->intrstatus & regs->intrenable;

	if (intrstatus & OHCI_INTR_UE)
	{
		ohci->disabled = 1;
		regs->intrdisable = OHCI_INTR_MIE;
		regs->control = 0;
		regs->cmdstatus = OHCI_HCR;
		ohci->hc_control = OHCI_USB_RESET;
		return;
	}

	if (intrstatus & OHCI_INTR_RHSC)
	{
		regs->intrstatus = OHCI_INTR_RHSC | OHCI_INTR_RD;
		regs->intrdisable = OHCI_INTR_RHSC;
	}
	else if (intrstatus & OHCI_INTR_RD)
	{
		regs->intrstatus = OHCI_INTR_RD;
		regs->intrdisable = OHCI_INTR_RD;
	}

	if (intrstatus & OHCI_INTR_WDH)
		update_done_list(ohci);

	ohci_work(ohci);

	if ((intrstatus & OHCI_INTR_SF) && !ohci->ed_rm_list)
		regs->intrdisable = OHCI_INTR_SF;

	regs->intrstatus = intrstatus;
	regs->intrenable = OHCI_INTR_MIE;
}

static struct vsfohci_t * vsfohci_init_get_resource(struct vsfusbh_t *usbh)
{
	struct vsfohci_t *ohci = vsf_bufmgr_malloc(sizeof(struct vsfohci_t));
	if (ohci == NULL)
		return NULL;
	memset(ohci, 0, sizeof(struct vsfohci_t));

	ohci->hcca = vsf_bufmgr_malloc_aligned(sizeof(struct ohci_hcca_t), 256);
	if (ohci->hcca == NULL)
		goto err_failed_alloc_hcca;
	memset(ohci->hcca, 0, sizeof(struct ohci_hcca_t));

	ohci->td_pool = vsf_bufmgr_malloc_aligned(
						sizeof(struct td_t) * TD_MAX_NUM, 32);
	if (ohci->td_pool == NULL)
		goto err_failed_alloc_ohci_td;
	memset(ohci->td_pool, 0, sizeof(struct td_t) * TD_MAX_NUM);

	return ohci;

err_failed_alloc_ohci_td:
	vsf_bufmgr_free(ohci->hcca);
err_failed_alloc_hcca:
	vsf_bufmgr_free(ohci);
	return NULL;
}

// return ms for PowerOn to PowerGood
static uint32_t vsfohci_init_hc_start(struct vsfohci_t *ohci)
{
	struct ohci_regs_t *regs = ohci->regs;

	ohci->disabled = 1;
	regs->ed_controlhead = 0;
	regs->ed_bulkhead = 0;
	regs->hcca = (uint32_t)ohci->hcca;
	ohci->hc_control = OHCI_CONTROL_INIT | OHCI_USB_OPER;
	ohci->disabled = 0;

	regs->control = ohci->hc_control;
	regs->fminterval = 0x2edf | (((0x2edf - 210) * 6 / 7) << 16);
	regs->periodicstart = (0x2edf * 9) / 10;
	regs->lsthresh = 0x628;
	regs->intrstatus = regs->intrenable =
			OHCI_INTR_MIE | OHCI_INTR_UE | OHCI_INTR_WDH | OHCI_INTR_SO;

	regs->roothub.a = regs->roothub.a & ~(RH_A_PSM | RH_A_OCPM);
	regs->roothub.status = RH_HS_LPSC;
	return (regs->roothub.a >> 23) & 0x1fe;
}

static struct vsfsm_state_t *
urb_free_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfohci_t *ohci = sm->user_data;
	struct vsfhcd_urb_t *urb;
	struct urb_priv_t *urb_priv;
	struct ed_t *ed;

	switch (evt)
	{
	case VSFSM_EVT_USER_LOCAL:
		while (ohci->ed_del_list != NULL)
		{
			ed = ohci->ed_del_list;
			ohci->ed_del_list = ed->ed_next;

			urb_priv = ed->dummy_td->urb_priv;
			urb = container_of(urb_priv, struct vsfhcd_urb_t, priv);
			if ((urb->transfer_buffer != NULL) &&
				(urb->transfer_flags & URB_BUFFER_DYNALLOC))
			{
				vsf_bufmgr_free(urb->transfer_buffer);
			}
			ed_fini(urb_priv);
			vsf_bufmgr_free(urb);
		}
		break;
	}
	return NULL;
}

static vsf_err_t vsfohci_init_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfusbh_t *usbh = (struct vsfusbh_t *)pt->user_data;
	struct vsfohci_t *ohci = (struct vsfohci_t *)usbh->hcd.priv;
	struct vsfohci_hcd_param_t *hcd_param = usbh->hcd.param;

	vsfsm_pt_begin(pt);

	usbh->hcd.rh_speed = USB_SPEED_FULL;
	ohci = usbh->hcd.priv = vsfohci_init_get_resource(usbh);
	if (!ohci)
		return VSFERR_FAIL;

	vsfhal_hcd_init(hcd_param->index, hcd_param->int_priority, vsfohci_interrupt, ohci);

	ohci->regs = (struct ohci_regs_t *)vsfhal_hcd_regbase(hcd_param->index);
	ohci->regs->intrdisable = OHCI_INTR_MIE;
	ohci->regs->control = 0;
	ohci->regs->cmdstatus = OHCI_HCR;
	// max 10 us delay
	while (ohci->regs->cmdstatus & OHCI_HCR)
		vsfsm_pt_delay(pt, 1);

	ohci->hc_control = OHCI_USB_RESET;

	vsfsm_pt_delay(pt, 100);
	vsfsm_pt_delay(pt, vsfohci_init_hc_start(ohci));

	ohci->urb_free_sm.init_state.evt_handler = urb_free_evt_handler;
	ohci->urb_free_sm.user_data = ohci;
	vsfsm_init(&ohci->urb_free_sm);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsfohci_fini(struct vsfhcd_t *hcd)
{
	return VSFERR_NONE;
}

static vsf_err_t vsfohci_suspend(struct vsfhcd_t *hcd)
{
	return VSFERR_NONE;
}

static vsf_err_t vsfohci_resume(struct vsfhcd_t *hcd)
{
	return VSFERR_NONE;
}

static struct vsfhcd_urb_t *vsfohci_alloc_urb(struct vsfhcd_t *hcd)
{
	uint32_t size;
	struct vsfhcd_urb_t *urb = NULL;
	struct urb_priv_t *urb_priv;
	struct ed_t *ed;

	size = sizeof(struct ed_t) + sizeof(struct vsfhcd_urb_t) + sizeof(struct urb_priv_t);
	ed = vsf_bufmgr_malloc_aligned(size, 16);
	if (ed)
	{
		memset(ed, 0, size);
		urb = (struct vsfhcd_urb_t *)(ed + 1);
		urb_priv = (struct urb_priv_t *)urb->priv;
		urb_priv->ed = ed;
	}
	return urb;
}

static void vsfohci_free_urb(struct vsfhcd_t *hcd, struct vsfhcd_urb_t *urb)
{
	struct vsfohci_t *ohci = (struct vsfohci_t *)hcd->priv;
	struct urb_priv_t *urb_priv = (struct urb_priv_t *)urb->priv;

	if (urb == NULL)
		return;

	if (urb_priv->state)
	{
		urb_priv->state &= ~URB_PRIV_WAIT_COMPLETE;
		urb_priv->state |= URB_PRIV_WAIT_DELETE;

		if ((urb_priv->state & (URB_PRIV_EDLINK | URB_PRIV_TDALLOC)) ==
				(URB_PRIV_EDLINK | URB_PRIV_TDALLOC))
		{
			start_ed_unlink(ohci, urb_priv);
		}
		else
		{
			// ERROR
		}
	}
	else
	{
		if ((urb->transfer_buffer != NULL) &&
			(urb->transfer_flags & URB_BUFFER_DYNALLOC))
		{
			vsf_bufmgr_free(urb->transfer_buffer);
		}
		vsf_bufmgr_free(urb);
	}
}

static vsf_err_t vsfohci_submit_urb(struct vsfhcd_t *hcd, struct vsfhcd_urb_t *urb)
{
	uint32_t size = 0, datablock;
	struct ed_t *ed;
	struct vsfohci_t *ohci = (struct vsfohci_t *)hcd->priv;
	struct urb_priv_t *urb_priv = (struct urb_priv_t *)urb->priv;

	if (ohci->disabled)
		return VSFERR_FAIL;

	ed_init(ohci, urb_priv, urb);
	datablock = (urb->transfer_length + 4095) / 4096;
	switch (usb_pipetype(urb->pipe))
	{
	case PIPE_CONTROL:
		size = 2;
	case PIPE_BULK:
	case PIPE_INTERRUPT:
		size += datablock;
		if (!size || ((urb->transfer_flags & URB_ZERO_PACKET) &&
						!(urb->transfer_length % urb->packet_size)))
			size++;
		break;
#if OHCI_ENABLE_ISO
	case PIPE_ISOCHRONOUS:
		size = urb_priv->number_of_packets;
		if (size == 0)
			return VSFERR_FAIL;
		for (uint32_t i = 0; i < size; i++)
		{
			urb_priv->iso_frame_desc[i].actual_length = 0;
			urb_priv->iso_frame_desc[i].status = VSFERR_FAIL;
		}
		break;
#endif // OHCI_ENABLE_ISO
	}

	if (size > TD_MAX_NUM_EACH_URB)
		return VSFERR_FAIL;

	ed = urb_priv->ed;
	memset(urb_priv, 0, sizeof(struct urb_priv_t));
	urb_priv->ed = ed;
	urb_priv->length = size;

	for (uint32_t i = 0; i < size; i++)
	{
		urb_priv->td[i] = td_alloc(ohci);
		if (NULL == urb_priv->td[i])
		{
			ed_fini(urb_priv);
			return VSFERR_FAIL;
		}
	}
	urb_priv->state |= URB_PRIV_TDALLOC;

#if OHCI_ENABLE_ISO
	if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS)
	{
		urb->start_frame = (ohci->hcca->frame_no + OHCI_ISO_DELAY) & 0xffff;
	}
#endif // OHCI_ENABLE_ISO

	urb->actual_length = 0;
	urb->status = URB_PENDING;

	ed_schedule(ohci, urb_priv);
	td_submit_urb(ohci, urb);
	return VSFERR_NONE;
}

// use for int/iso urb
static vsf_err_t vsfohci_relink_urb(struct vsfhcd_t *hcd, struct vsfhcd_urb_t *urb)
{
	struct vsfohci_t *ohci = (struct vsfohci_t *)hcd->priv;
	struct urb_priv_t *urb_priv = (struct urb_priv_t *)urb->priv;

	switch (usb_pipetype(urb->pipe))
	{
	case PIPE_INTERRUPT:
		urb->actual_length = 0;
		if (urb_priv->state == (URB_PRIV_EDLINK | URB_PRIV_TDALLOC))
		{
			urb->status = URB_PENDING;
			td_submit_urb(ohci, urb);
			return VSFERR_NONE;
		}
		break;
#if OHCI_ENABLE_ISO
	case PIPE_ISOCHRONOUS:
		urb->actual_length = 0;
		if (urb_priv->state == (URB_PRIV_EDLINK | URB_PRIV_TDALLOC))
		{
			uint32_t i;
			// NOTE: iso transfer interval fixed to 1
			urb->start_frame = (ohci->hcca->frame_no + 1) & 0xffff;
			for (i = 0; i < urb_priv->number_of_packets; i++)
				urb_priv->iso_frame_desc[i].actual_length = 0;
			td_submit_urb(ohci, urb);
			return VSFERR_NONE;
		}
#endif
	default:
		break;
	}
	return VSFERR_FAIL;
}

static int vsfohci_rh_control(struct vsfhcd_t *hcd, struct vsfhcd_urb_t *urb)
{
	uint16_t typeReq, wValue, wIndex, wLength;
	struct vsfohci_t *ohci = (struct vsfohci_t *)hcd->priv;
	struct ohci_regs_t *regs = ohci->regs;
	struct usb_ctrlrequest_t *cmd = &urb->setup_packet;
	uint32_t datadw[4], temp;
	uint8_t *data = (uint8_t*)datadw;
	uint8_t len = 0;

	typeReq = (cmd->bRequestType << 8) | cmd->bRequest;
	wValue = cmd->wValue;
	wIndex = cmd->wIndex;
	wLength = cmd->wLength;

	switch (typeReq)
	{
	case GetHubStatus: datadw[0] = RD_RH_STAT; len = 4; break;
	case GetPortStatus: datadw[0] = RD_RH_PORTSTAT; len = 4; break;
	case SetPortFeature:
		switch (wValue)
		{
		case RH_PORT_SUSPEND: WR_RH_PORTSTAT(RH_PS_PSS); break;
		case RH_PORT_RESET:	/* BUG IN HUP CODE *********/
			if (RD_RH_PORTSTAT & RH_PS_CCS)
				WR_RH_PORTSTAT(RH_PS_PRS);
			break;
		case RH_PORT_POWER: WR_RH_PORTSTAT(RH_PS_PPS); break;
		case RH_PORT_ENABLE:	/* BUG IN HUP CODE *********/
			if (RD_RH_PORTSTAT & RH_PS_CCS)
				WR_RH_PORTSTAT(RH_PS_PES);
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		switch (wValue)
		{
		case RH_PORT_ENABLE: WR_RH_PORTSTAT(RH_PS_CCS); break;
		case RH_PORT_SUSPEND: WR_RH_PORTSTAT(RH_PS_POCI); break;
		case RH_PORT_POWER: WR_RH_PORTSTAT(RH_PS_LSDA); break;
		case RH_C_PORT_CONNECTION: WR_RH_PORTSTAT(RH_PS_CSC); break;
		case RH_C_PORT_ENABLE: WR_RH_PORTSTAT(RH_PS_PESC); break;
		case RH_C_PORT_SUSPEND: WR_RH_PORTSTAT(RH_PS_PSSC); break;
		case RH_C_PORT_OVER_CURRENT: WR_RH_PORTSTAT(RH_PS_OCIC); break;
		case RH_C_PORT_RESET: WR_RH_PORTSTAT(RH_PS_PRSC); break;
		default:
			goto error;
		}
		break;
	case GetHubDescriptor:
		temp = regs->roothub.a;
		data[0] = 9;				// min length;
		data[1] = 0x29;
		data[2] = temp & RH_A_NDP;
		data[3] = 0;
		if (temp & RH_A_PSM)		/* per-port power switching? */
			data[3] |= 0x1;
		if (temp & RH_A_NOCP)		/* no over current reporting? */
			data[3] |= 0x10;
		else if (temp & RH_A_OCPM)	/* per-port over current reporting? */
			data[3] |= 0x8;
		datadw[1] = 0;
		data[5] = (temp & RH_A_POTPGT) >> 24;
		temp = regs->roothub.b;
		data[7] = temp & RH_B_DR;
		if (data[2] < 7)
		{
			data[8] = 0xff;
		}
		else
		{
			data[0] += 2;
			data[8] = (temp & RH_B_DR) >> 8;
			data[10] = data[9] = 0xff;
		}
		len = min(data[0], wLength);
		break;
	default:
		goto error;
	}
	if (len)
	{
		if (urb->transfer_length < len)
			len = urb->transfer_length;
		urb->actual_length = len;
		memcpy(urb->transfer_buffer, data, len);
	}
	return len;

error:
	urb->status = URB_FAIL;
	return VSFERR_FAIL;
}

const struct vsfhcd_drv_t vsfohci_drv =
{
	.init_thread = vsfohci_init_thread,
	.fini = vsfohci_fini,
	.suspend = vsfohci_suspend,
	.resume = vsfohci_resume,
	.alloc_urb = vsfohci_alloc_urb,
	.free_urb = vsfohci_free_urb,
	.submit_urb = vsfohci_submit_urb,
	.relink_urb = vsfohci_relink_urb,
	.rh_control = vsfohci_rh_control,
};

