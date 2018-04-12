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

#define USART_BUF_SIZE	16

static void uart_on_tx(void *p)
{
	uint8_t buf[USART_BUF_SIZE];
	struct vsf_buffer_t buffer;
	struct usart_stream_t *stream = p;
	uint16_t hw_bufsize;

	if (!stream->stream_tx)
		return;

	hw_bufsize = vsfhal_usart_tx_get_free_size(stream->index);
	buffer.size = min(hw_bufsize, dimof(buf));
	if (buffer.size)
	{
		buffer.buffer = buf;
		buffer.size = stream_read(stream->stream_tx, &buffer);
		if (buffer.size)
		{
			vsfhal_usart_tx_bytes(stream->index, buf, buffer.size);
		}
	}
}

static void uart_on_rx(void *p)
{
	uint8_t buf[USART_BUF_SIZE];
	struct vsf_buffer_t buffer;
	struct usart_stream_t *stream = p;

	if (!stream->stream_rx)
		return;

	buffer.size = vsfhal_usart_rx_get_data_size(stream->index);
	if (buffer.size)
	{
		buffer.buffer = buf;
		buffer.size = min(buffer.size, dimof(buf));
		buffer.size = vsfhal_usart_rx_bytes(stream->index, buf, buffer.size);
		stream_write(stream->stream_rx, &buffer);	
	}
}

vsf_err_t usart_stream_init(struct usart_stream_t *usart_stream)
{
	if (!usart_stream->stream_tx && !usart_stream->stream_rx)
		return VSFERR_FAIL;

	if (usart_stream->stream_tx)
	{
		stream_init(usart_stream->stream_tx);
		usart_stream->stream_tx->callback_rx.param = usart_stream;
		usart_stream->stream_tx->callback_rx.on_inout = uart_on_tx;
		usart_stream->stream_tx->callback_rx.on_connect = NULL;
		usart_stream->stream_tx->callback_rx.on_disconnect = NULL;
		stream_connect_rx(usart_stream->stream_tx);
	}
	if (usart_stream->stream_rx)
	{
		stream_init(usart_stream->stream_rx);
		usart_stream->stream_rx->callback_tx.param = usart_stream;
		usart_stream->stream_rx->callback_tx.on_inout = uart_on_rx;
		usart_stream->stream_rx->callback_tx.on_connect = NULL;
		usart_stream->stream_rx->callback_tx.on_disconnect = NULL;
		stream_connect_tx(usart_stream->stream_rx);
	}

	vsfhal_usart_init(usart_stream->index);
	vsfhal_usart_config_cb(usart_stream->index, usart_stream->int_priority,
			usart_stream, uart_on_tx, uart_on_rx);
	vsfhal_usart_config(usart_stream->index, usart_stream->baudrate,
			usart_stream->mode);

	return VSFERR_NONE;
}

vsf_err_t usart_stream_fini(struct usart_stream_t *usart_stream)
{
	vsfhal_usart_config_cb(usart_stream->index, 0, NULL, NULL, NULL);
	vsfhal_usart_fini(usart_stream->index);
	return VSFERR_NONE;
}

