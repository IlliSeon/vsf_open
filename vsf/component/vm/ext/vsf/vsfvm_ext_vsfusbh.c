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
#ifdef VSFVM_VM
#include "vsfvm.h"
#endif
#ifdef VSFVM_COMPILER
#include "vsfvm_compiler.h"
#endif
#include "std/vsfvm_ext_std.h"
#include "vsfvm_ext_vsfusbh.h"

#ifdef VSFVM_VM
static struct vsfsm_state_t *
vsfvm_ext_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfvm_thread_t *thread = (struct vsfvm_thread_t *)sm->user_data;
	switch (evt)
	{
	case VSFSM_EVT_URB_COMPLETE:
		vsfvm_thread_ready(thread);
		break;
	}
	return NULL;
}

struct vsfvm_ext_usbhdev_t
{
	struct vsfusbh_libusb_dev_t *dev;
};

static enum vsfvm_ret_t vsfvm_ext_usbhdev_find(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *vid = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *pid = vsfvm_get_func_argu_ref(thread, 1);
	uint32_t devnum = vsfusbh_libusb_enum_begin();
	struct vsfusbh_libusb_dev_t *dev = NULL;

	while (devnum-- > 0)
	{
		dev = vsfusbh_libusb_enum_next();
		if ((dev->vid == vid->uval16) && (dev->pid == pid->uval16))
		{
			if (vsfvm_instance_alloc(result, sizeof(struct vsfvm_ext_usbhdev_t), &vsfvm_ext_usbhdev))
				return VSFVM_RET_ERROR;
			else if (!vsfusbh_libusb_open(dev))
			{
				struct vsfvm_ext_usbhdev_t *usbhdev =
					(struct vsfvm_ext_usbhdev_t *)result->inst->buffer.buffer;
				usbhdev->dev = dev;
				break;
			}
		}
	}
	vsfusbh_libusb_enum_end();
	if (!dev)
	{
		result->type = VSFVM_VAR_TYPE_INSTANCE;
		result->inst = NULL;
	}
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_usbhdev_transfer(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *usbhdev = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *ep = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfvm_var_t *trans_type = vsfvm_get_func_argu_ref(thread, 2);
	struct vsfvm_var_t *length;
	struct vsfvm_var_t *buffer;
	uint8_t argc = thread->func.argc;
	struct vsfusbh_libusb_dev_t *dev;
	struct vsfhcd_device_t *hcddev;
	struct vsfhcd_urb_t *urb;
	uint8_t epdir, epnum;

	if ((argc < 3) || !usbhdev->inst)
		return VSFVM_RET_ERROR;
	epdir = ep->uval8 & USB_DIR_IN;
	epnum = ep->uval8 &~USB_DIR_IN;
	if ((epdir != USB_DIR_IN) && (epdir != USB_DIR_OUT))
		return VSFVM_RET_ERROR;

	dev = ((struct vsfvm_ext_usbhdev_t *)usbhdev->inst->buffer.buffer)->dev;
	if (dev->removed)
	{
	ret_fail:
		vsfvm_instance_deref(thread, result);
		if (epdir)
		{
			result->type = VSFVM_VAR_TYPE_INSTANCE;
			result->inst = NULL;
		}
		else
		{
			result->type = VSFVM_VAR_TYPE_VALUE;
			result->value = 0;
		}
		vsfusbh_libusb_close(dev);
		return VSFVM_RET_FINISHED;
	}
	hcddev = &dev->dev->hcddev;
	urb = dev->urb;

	if (!thread->sm.init_state.evt_handler)
	{
		urb->notifier_sm = &thread->sm;
		if (!epnum)
		{
			if (((epdir == USB_DIR_IN) && (argc != 8)) ||
				((epdir == USB_DIR_OUT) && (argc != 9)))
			{
				goto ret_fail;
			}
			length = vsfvm_get_func_argu_ref(thread, 7);
			buffer = vsfvm_get_func_argu_ref(thread, 8);
		}
		else
		{
			if (((epdir == USB_DIR_IN) && (argc != 4)) ||
				((epdir == USB_DIR_OUT) && (argc != 5)))
			{
				goto ret_fail;
			}
			length = vsfvm_get_func_argu_ref(thread, 3);
			buffer = vsfvm_get_func_argu_ref(thread, 4);
		}

		if (epdir == USB_DIR_IN)
		{
			if (vsfvm_thread_stack_push(thread, 0, VSFVM_VAR_TYPE_VALUE, 1))
				goto ret_fail;
			buffer = vsfvm_thread_stack_get(thread, 0);
			if (vsfvm_instance_alloc(buffer, length->uval32, &vsfvm_ext_buffer))
			{
			pop_ret_fail:
				vsfvm_thread_stack_pop(thread, 1);
				goto ret_fail;
			}
		}
		else if (epdir == USB_DIR_OUT)
		{
			if (vsfvm_thread_stack_push(thread, buffer->value, buffer->type, 1))
				goto ret_fail;
		}

		switch (trans_type->value)
		{
		case USB_ENDPOINT_XFER_CONTROL:
			if (epnum != 0) goto ret_fail;
			urb->pipe = epdir ? usb_rcvctrlpipe(hcddev, 0) : usb_sndctrlpipe(hcddev, 0);
			break;
		case USB_ENDPOINT_XFER_ISOC:
			if (epnum == 0) goto ret_fail;
			urb->pipe = epdir ? usb_rcvisocpipe(hcddev, epnum) : usb_sndisocpipe(hcddev, epnum);
			break;
		case USB_ENDPOINT_XFER_BULK:
			if (epnum == 0) goto ret_fail;
			urb->pipe = epdir ? usb_rcvbulkpipe(hcddev, epnum) : usb_sndbulkpipe(hcddev, epnum);
			break;
		case USB_ENDPOINT_XFER_INT:
			if (epnum == 0) goto ret_fail;
			urb->pipe = epdir ? usb_rcvintpipe(hcddev, epnum) : usb_sndintpipe(hcddev, epnum);
			break;
		}

		if (!epnum)
		{
			// control transfer
			struct vsfvm_var_t *type = vsfvm_get_func_argu_ref(thread, 3);
			struct vsfvm_var_t *request = vsfvm_get_func_argu_ref(thread, 4);
			struct vsfvm_var_t *value = vsfvm_get_func_argu_ref(thread, 5);
			struct vsfvm_var_t *index = vsfvm_get_func_argu_ref(thread, 6);

			urb->setup_packet.bRequestType = type->uval8;
			urb->setup_packet.bRequest = request->uval8;
			urb->setup_packet.wValue = value->uval16;
			urb->setup_packet.wIndex = index->uval16;
			urb->setup_packet.wLength = length->uval16;
		}

		if (!buffer->inst) return VSFVM_RET_ERROR;
		urb->transfer_buffer = buffer->inst->buffer.buffer;
		urb->transfer_length = length->uval16;

		thread->sm.init_state.evt_handler = vsfvm_ext_evt_handler;
		thread->sm.user_data = thread;
		vsfsm_init(&thread->sm);
		if (vsfusbh_submit_urb(dev->usbh, urb))
			goto pop_ret_fail;
		return VSFVM_RET_PEND;
	}
	else
	{
		vsfsm_fini(&thread->sm);
		thread->sm.init_state.evt_handler = NULL;

		buffer = vsfvm_thread_stack_pop(thread, 1);
		vsfvm_instance_deref(thread, result);
		if (epdir)
		{
			buffer->inst->buffer.size = urb->actual_length;
			*result = *buffer;
		}
		else
		{
			result->type = VSFVM_VAR_TYPE_VALUE;
			result->value = urb->actual_length;
		}
		
		return VSFVM_RET_FINISHED;
	}
}

static void vsfvm_ext_usbhdev_destroy(struct vsfvm_var_t *var)
{
	struct vsfusbh_libusb_dev_t *dev;
	if (!var->inst) return;
	dev = ((struct vsfvm_ext_usbhdev_t *)var->inst->buffer.buffer)->dev;
	if (dev != NULL)
		vsfusbh_libusb_close(dev);
}
#endif

const struct vsfvm_class_t vsfvm_ext_usbhdev =
{
#ifdef VSFVM_COMPILER
	.name = "usbhdev",
#endif
#ifdef VSFVM_VM
	.type = VSFVM_CLASS_USER,
	.op.destroy = vsfvm_ext_usbhdev_destroy,
#endif
};

#ifdef VSFVM_COMPILER
extern const struct vsfvm_ext_op_t vsfvm_ext_vsfusbh;
static const struct vsfvmc_lexer_sym_t vsfvm_ext_sym[] =
{
	VSFVM_LEXERSYM_CONST("USBH_EPOUT", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev, USB_DIR_OUT),
	VSFVM_LEXERSYM_CONST("USBH_EPIN", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev, USB_DIR_IN),
	VSFVM_LEXERSYM_CONST("USBH_TRANS_CTRL", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev, USB_ENDPOINT_XFER_CONTROL),
	VSFVM_LEXERSYM_CONST("USBH_TRANS_BULK", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev, USB_ENDPOINT_XFER_BULK),
	VSFVM_LEXERSYM_CONST("USBH_TRANS_INT", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev, USB_ENDPOINT_XFER_INT),
	VSFVM_LEXERSYM_CONST("USBH_TRANS_ISO", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev, USB_ENDPOINT_XFER_ISOC),
	VSFVM_LEXERSYM_CLASS("usbhdev", &vsfvm_ext_vsfusbh, &vsfvm_ext_usbhdev),
	VSFVM_LEXERSYM_EXTFUNC("usbhdev_find", &vsfvm_ext_vsfusbh, NULL, &vsfvm_ext_usbhdev, 2, 0),
	VSFVM_LEXERSYM_EXTFUNC("usbhdev_transfer", &vsfvm_ext_vsfusbh, &vsfvm_ext_buffer, &vsfvm_ext_usbhdev, -1, 1),
};
#endif

#ifdef VSFVM_VM
static const struct vsfvm_extfunc_t vsfvm_ext_func[] =
{
	VSFVM_EXTFUNC("usbhdev_find", vsfvm_ext_usbhdev_find, 2),
	VSFVM_EXTFUNC("usbhdev_transfer", vsfvm_ext_usbhdev_transfer, -1),
};
#endif

const struct vsfvm_ext_op_t vsfvm_ext_vsfusbh =
{
#ifdef VSFVM_COMPILER
	.name = "vsfhal",
	.sym = vsfvm_ext_sym,
	.sym_num = dimof(vsfvm_ext_sym),
#endif
#ifdef VSFVM_VM
	.init = NULL,
	.fini = NULL,
	.func = (struct vsfvm_extfunc_t *)vsfvm_ext_func,
#endif
	.func_num = dimof(vsfvm_ext_func),
};
