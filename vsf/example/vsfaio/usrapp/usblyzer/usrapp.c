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
	.debug.uart_stream.baudrate				= 115200,
	.debug.uart_stream.stream_tx			= &usrapp.debug.stream_tx.stream,
	.debug.uart_stream.stream_rx			= &usrapp.debug.stream_rx.stream,
};

void usrapp_initial_init(struct usrapp_t *app){}

void usrapp_srt_init(struct usrapp_t *app)
{
	vsf_usart_stream_init(&app->debug.uart_stream);
	vsfdbg_init((struct vsf_stream_t *)&usrapp.debug.stream_tx);

	vsfusbh_init(&usrapp.usbh);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_hub_drv);

	usblyzer_init(&vsfhal_usbd, 0xFF);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_usblyzer_drv);
}

#if defined(APPCFG_USR_POLL)
void usrapp_poll(struct usrapp_t *app){}
#endif

