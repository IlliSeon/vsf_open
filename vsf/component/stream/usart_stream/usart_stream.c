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
	struct usart_stream_t *stream = (struct usart_stream_t *)p;
	uint8_t buf[USART_BUF_SIZE];
	struct vsf_buffer_t buffer;
	uint16_t hw_bufsize;

	if (!stream->stream_tx)
		return;

	hw_bufsize = vsfhal_usart_tx_get_free_size(stream->index);
	buffer.buffer = buf;
	buffer.size = min(hw_bufsize, dimof(buf));
	if (buffer.size)
	{
		buffer.size = stream_read(stream->stream_tx, &buffer);
		if (buffer.size)
		{
			vsfhal_usart_tx_bytes(stream->index, buf, buffer.size);
		}
		else if (!vsfhal_usart_tx_get_data_size(stream->index))
		{
			stream->txing = false;
		}
	}
}

static void uart_on_rx(void *p)
{
	struct usart_stream_t *stream = (struct usart_stream_t *)p;
	uint8_t buf[USART_BUF_SIZE];
	struct vsf_buffer_t buffer;
	uint16_t hw_bufsize;
	uint32_t stream_free_size;

	if (!stream->stream_rx)
		return;

	hw_bufsize = vsfhal_usart_rx_get_data_size(stream->index);
	while (hw_bufsize)
	{
		buffer.size = min(hw_bufsize, dimof(buf));
		if (buffer.size)
		{
			stream_free_size = stream_get_free_size(stream->stream_rx);
			buffer.size = min(buffer.size, stream_free_size);
			if (buffer.size)
			{
				hw_bufsize -= buffer.size;
				buffer.size = vsfhal_usart_rx_bytes(stream->index, buf, buffer.size);
				buffer.buffer = buf;
				stream_write(stream->stream_rx, &buffer);
			}
			else
			{
				stream->rx_pend = true;
				vsfhal_usart_rx_disable(stream->index);
				break;
			}
		}
	}
}

static void uart_on_stream_in(void *p)
{
	struct usart_stream_t *stream = (struct usart_stream_t *)p;
	if (!stream->txing)
	{
		stream->txing = true;
		uart_on_tx(stream);
	}
}

static void uart_on_stream_out(void *p)
{
	struct usart_stream_t *stream = (struct usart_stream_t *)p;
	if (stream->rx_pend)
	{
		stream->rx_pend = false;
		vsfhal_usart_rx_enable(stream->index);
	}
}

vsf_err_t usart_stream_config(struct usart_stream_t *usart_stream)
{
	return vsfhal_usart_config(usart_stream->index, usart_stream->baudrate,
			usart_stream->mode);
}

vsf_err_t usart_stream_init(struct usart_stream_t *usart_stream)
{
	if (!usart_stream->stream_tx && !usart_stream->stream_rx)
		return VSFERR_FAIL;

	usart_stream->txing = false;
	usart_stream->rx_pend = false;
	if (usart_stream->stream_tx)
	{
		stream_init(usart_stream->stream_tx);
		usart_stream->stream_tx->callback_rx.param = usart_stream;
		usart_stream->stream_tx->callback_rx.on_inout = uart_on_stream_in;
		usart_stream->stream_tx->callback_rx.on_connect = NULL;
		usart_stream->stream_tx->callback_rx.on_disconnect = NULL;
		stream_connect_rx(usart_stream->stream_tx);
	}
	if (usart_stream->stream_rx)
	{
		stream_init(usart_stream->stream_rx);
		usart_stream->stream_rx->callback_tx.param = usart_stream;
		usart_stream->stream_rx->callback_tx.on_inout = uart_on_stream_out;
		usart_stream->stream_rx->callback_tx.on_connect = NULL;
		usart_stream->stream_rx->callback_tx.on_disconnect = NULL;
		stream_connect_tx(usart_stream->stream_rx);
	}

	vsfhal_usart_init(usart_stream->index);
	vsfhal_usart_config_cb(usart_stream->index, usart_stream->int_priority,
			usart_stream, uart_on_tx, uart_on_rx);
	return usart_stream_config(usart_stream);
}

vsf_err_t usart_stream_fini(struct usart_stream_t *usart_stream)
{
	vsfhal_usart_config_cb(usart_stream->index, 0, NULL, NULL, NULL);
	vsfhal_usart_fini(usart_stream->index);
	return VSFERR_NONE;
}

