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
#include "usrapp.h"

struct usrapp_t usrapp =
{
	.usbh.hcd								= &vsfohci_drv,
	.hcd_param.index						= VSFHAL_HCD_INDEX,
	.hcd_param.int_priority					= 0xFF,
	.usbh.hcd								= &vsfohci_drv,
	.usbh.hcd.param							= &usrapp.hcd_param,

	.debug.stream_tx.stream.op				= &vsf_fifostream_op,
	.debug.stream_tx.mem.buffer.buffer		= (uint8_t *)&usrapp.debug.txbuff,
	.debug.stream_tx.mem.buffer.size		= sizeof(usrapp.debug.txbuff),
	.debug.stream_rx.stream.op				= &vsf_fifostream_op,
	.debug.stream_rx.mem.buffer.buffer		= (uint8_t *)&usrapp.debug.rxbuff,
	.debug.stream_rx.mem.buffer.size		= sizeof(usrapp.debug.rxbuff),

	.debug.uart_stream.index				= DEBUG_UART_INDEX,
	.debug.uart_stream.mode					= VSFHAL_USART_STOPBITS_1 | VSFHAL_USART_PARITY_NONE,
	.debug.uart_stream.int_priority			= 0xFF,
	.debug.uart_stream.baudrate				= 3 * 1000 * 1000,
	.debug.uart_stream.stream_tx			= &usrapp.debug.stream_tx.stream,
	.debug.uart_stream.stream_rx			= &usrapp.debug.stream_rx.stream,
};

void usrapp_initial_init(struct usrapp_t *app){}

static bool usrapp_usblyzer_output_isbusy(void *param)
{
	struct usrapp_t *app = (struct usrapp_t *)param;
	return vsfstream_get_data_size(app->debug.uart_stream.stream_tx) > 0;
}

static void usrapp_usblyzer_on_event(void *param, enum vsfhal_usbd_evt_t evt,
		uint32_t value, uint8_t *buff, uint32_t size)
{
	switch (evt)
	{
	case VSFHAL_USBD_ON_RESET:
		vsfdbg_prints("Reset" VSFCFG_DEBUG_LINEEND);
		break;
	case VSFHAL_USBD_ON_SETUP:
		vsfdbg_prints("SETUP: ");
		vsfdbg_printb(buff, size, 1, 16, true, true);
		break;
	case VSFHAL_USBD_ON_IN:
		vsfdbg_printf("IN%d(%d): ", value, size);
		if (buff != NULL)
			vsfdbg_printb(buff, size, 1, 16, true, false);
		vsfdbg_prints(VSFCFG_DEBUG_LINEEND);
		break;
	case VSFHAL_USBD_ON_OUT:
		vsfdbg_printf("OUT%d(%d): ", value, size);
		if (buff != NULL)
			vsfdbg_printb(buff, size, 1, 16, true, false);
		vsfdbg_prints(VSFCFG_DEBUG_LINEEND);
		break;
	case VSFHAL_USBD_ON_STALL:
		vsfdbg_printf("IN%d: STALL" VSFCFG_DEBUG_LINEEND, value);
		break;
	}
}

void usrapp_srt_init(struct usrapp_t *app)
{
	vsf_usart_stream_init(&app->debug.uart_stream);
	vsfdbg_init((struct vsf_stream_t *)&usrapp.debug.stream_tx);

	vsfusbh_init(&usrapp.usbh);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_hub_drv);

	usblyzer_init(&vsfhal_usbd, 0xFF, app, usrapp_usblyzer_on_event,
			usrapp_usblyzer_output_isbusy);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_usblyzer_drv);
}

#if defined(APPCFG_USR_POLL)
void usrapp_poll(struct usrapp_t *app){}
#endif

