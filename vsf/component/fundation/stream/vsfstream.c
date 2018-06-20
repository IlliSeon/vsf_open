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

uint32_t vsfstream_read(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	uint32_t count = stream->op->read(stream, buffer);

	if (stream->tx_ready && (stream->callback_tx.on_inout != NULL) && count)
	{
		stream->callback_tx.on_inout(stream->callback_tx.param);
	}
	return count;
}

uint32_t vsfstream_write(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	uint32_t count = stream->op->write(stream, buffer);

	if (count < buffer->size)
	{
		stream->overflow = true;
	}
	if (stream->rx_ready && (stream->callback_rx.on_inout != NULL) && count)
	{
		stream->callback_rx.on_inout(stream->callback_rx.param);
	}
	return count;
}

uint32_t vsfstream_get_data_size(struct vsf_stream_t *stream)
{
	return stream->op->get_data_length(stream);
}

uint32_t vsfstream_get_free_size(struct vsf_stream_t *stream)
{
	return stream->op->get_avail_length(stream);
}

uint32_t vsfstream_get_wbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	return stream->op->get_wbuf(stream, ptr);
}

uint32_t vsfstream_get_rbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	return stream->op->get_rbuf(stream, ptr);
}

void vsfstream_connect_rx(struct vsf_stream_t *stream)
{
	if (!stream->rx_ready)
	{
		if (stream->callback_tx.on_connect != NULL)
		{
			stream->callback_tx.on_connect(stream->callback_tx.param);
		}
		if ((stream->tx_ready) && (stream->callback_rx.on_connect != NULL))
		{
			stream->callback_rx.on_connect(stream->callback_rx.param);
		}
		stream->rx_ready = true;
	}
}

void vsfstream_connect_tx(struct vsf_stream_t *stream)
{
	if (!stream->tx_ready)
	{
		if (stream->callback_rx.on_connect != NULL)
		{
			stream->callback_rx.on_connect(stream->callback_rx.param);
		}
		if ((stream->rx_ready) && (stream->callback_tx.on_connect != NULL))
		{
			stream->callback_tx.on_connect(stream->callback_tx.param);
		}
		stream->tx_ready = true;
	}
}

void vsfstream_disconnect_rx(struct vsf_stream_t *stream)
{
	if (stream->rx_ready && (stream->callback_tx.on_disconnect != NULL))
	{
		stream->callback_tx.on_disconnect(stream->callback_tx.param);
	}
	stream->rx_ready = false;
}

void vsfstream_disconnect_tx(struct vsf_stream_t *stream)
{
	if (stream->tx_ready && (stream->callback_rx.on_disconnect != NULL))
	{
		stream->callback_rx.on_disconnect(stream->callback_rx.param);
	}
	stream->tx_ready = false;
}

vsf_err_t vsfstream_init(struct vsf_stream_t *stream)
{
	stream->overflow = false;
	stream->tx_ready = false;
	stream->rx_ready = false;
	if (stream->op->init != NULL)
	{
		stream->op->init(stream);
	}
	return VSFERR_NONE;
}

vsf_err_t vsfstream_fini(struct vsf_stream_t *stream)
{
	if (stream->tx_ready)
	{
		vsfstream_disconnect_tx(stream);
	}
	if (stream->rx_ready)
	{
		vsfstream_disconnect_rx(stream);
	}
	if (stream->op->fini != NULL)
	{
		stream->op->fini(stream);
	}
	return VSFERR_NONE;
}

// fifo stream
static void vsf_fifostream_init(struct vsf_stream_t *stream)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	vsf_fifo_init(&fifostream->mem);
}

static uint32_t vsf_fifostream_get_data_length(struct vsf_stream_t *stream)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	return vsf_fifo_get_data_length(&fifostream->mem);
}

static uint32_t vsf_fifostream_get_avail_length(struct vsf_stream_t *stream)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	return vsf_fifo_get_avail_length(&fifostream->mem);
}

static uint32_t vsf_fifostream_get_wbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	return vsf_fifo_get_wbuf(&fifostream->mem, ptr);
}

static uint32_t vsf_fifostream_get_rbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	return vsf_fifo_get_rbuf(&fifostream->mem, ptr);
}

static uint32_t
vsf_fifostream_write(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	return vsf_fifo_push(&fifostream->mem, buffer->size, buffer->buffer);
}

static uint32_t
vsf_fifostream_read(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	struct vsf_fifostream_t *fifostream = (struct vsf_fifostream_t *)stream;
	return vsf_fifo_pop(&fifostream->mem, buffer->size, buffer->buffer);
}

// multibuf stream
static void vsf_mbufstream_init(struct vsf_stream_t *stream)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;

	mbufstream->mem.rpos = mbufstream->mem.wpos = 0;
	vsf_multibuf_init(&mbufstream->mem.multibuf);
}

static uint32_t vsf_mbufstream_get_data_length(struct vsf_stream_t *stream)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;

	return  (mbufstream->mem.multibuf.length * mbufstream->mem.multibuf.size)
				+ mbufstream->mem.wpos - mbufstream->mem.rpos;
}

static uint32_t vsf_mbufstream_get_avail_length(struct vsf_stream_t *stream)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;

	return (mbufstream->mem.multibuf.count * mbufstream->mem.multibuf.size) -
								vsf_mbufstream_get_data_length(stream);
}

static uint32_t
vsf_mbufstream_get_wbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;
	uint8_t *buf = vsf_multibuf_get_empty(&mbufstream->mem.multibuf);
	uint32_t avail_len = VSFSTREAM_GET_FREE_SIZE(&mbufstream->mem.multibuf);
	uint32_t avail_buf = mbufstream->mem.multibuf.size - mbufstream->mem.wpos;

	if (ptr)
		*ptr = &buf[mbufstream->mem.wpos];
	return min(avail_len, avail_buf);
}

static uint32_t
vsf_mbufstream_get_rbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;
	uint8_t *buf = vsf_multibuf_get_payload(&mbufstream->mem.multibuf);
	uint32_t avail_len = VSFSTREAM_GET_DATA_SIZE(&mbufstream->mem.multibuf);
	uint32_t avail_buf = mbufstream->mem.multibuf.size - mbufstream->mem.rpos;

	if (ptr)
		*ptr = &buf[mbufstream->mem.rpos];
	return min(avail_len, avail_buf);
}

static uint32_t
vsf_mbufstream_write(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;
	uint8_t *buf = vsf_multibuf_get_empty(&mbufstream->mem.multibuf);
	uint32_t wsize = 0, cur_size, remain_size = buffer->size;

	while ((buf != NULL) && (remain_size > 0))
	{
		cur_size = mbufstream->mem.multibuf.size - mbufstream->mem.wpos;
		cur_size = min(cur_size, remain_size);
		if (buffer->buffer)
			memcpy(&buf[mbufstream->mem.wpos], &buffer->buffer[wsize], cur_size);
		wsize += cur_size;
		remain_size -= cur_size;

		mbufstream->mem.wpos += cur_size;
		if (mbufstream->mem.wpos >= mbufstream->mem.multibuf.size)
		{
			vsf_multibuf_push(&mbufstream->mem.multibuf);
			buf = vsf_multibuf_get_empty(&mbufstream->mem.multibuf);
			mbufstream->mem.wpos = 0;
		}
	}
	return wsize;
}

static uint32_t
vsf_mbufstream_read(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	struct vsf_mbufstream_t *mbufstream = (struct vsf_mbufstream_t *)stream;
	uint8_t *buf = vsf_multibuf_get_payload(&mbufstream->mem.multibuf);
	uint32_t rsize = 0, cur_size, remain_size = buffer->size;

	while ((buf != NULL) && (remain_size > 0))
	{
		cur_size = mbufstream->mem.multibuf.size - mbufstream->mem.rpos;
		cur_size = min(cur_size, remain_size);
		if (buffer->buffer)
			memcpy(&buffer->buffer[rsize], &buf[mbufstream->mem.rpos], cur_size);
		rsize += cur_size;
		remain_size -= cur_size;

		mbufstream->mem.rpos += cur_size;
		if (mbufstream->mem.rpos >= mbufstream->mem.multibuf.size)
		{
			vsf_multibuf_pop(&mbufstream->mem.multibuf);
			buf = vsf_multibuf_get_payload(&mbufstream->mem.multibuf);
			mbufstream->mem.rpos = 0;
		}
	}
	return rsize;
}

// buffer stream
static void vsf_bufstream_init(struct vsf_stream_t *stream)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	bufstream->mem.pos = 0;
}

static uint32_t vsf_bufstream_get_data_length(struct vsf_stream_t *stream)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	return !bufstream->mem.read ? bufstream->mem.pos :
						bufstream->mem.buffer.size - bufstream->mem.pos;
}

static uint32_t vsf_bufstream_get_avail_length(struct vsf_stream_t *stream)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	return bufstream->mem.read ? bufstream->mem.pos :
						bufstream->mem.buffer.size - bufstream->mem.pos;
}

static uint32_t
vsf_bufstream_get_wbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	uint8_t *p = NULL;
	uint32_t size = 0;

	if (!bufstream->mem.read)
	{
		p = bufstream->mem.buffer.buffer + bufstream->mem.pos;
		size = vsf_bufstream_get_avail_length(stream);
	}

	if (ptr)
		*ptr = p;
	return size;
}

static uint32_t
vsf_bufstream_get_rbuf(struct vsf_stream_t *stream, uint8_t **ptr)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	uint8_t *p = NULL;
	uint32_t size = 0;

	if (bufstream->mem.read)
	{
		p = bufstream->mem.buffer.buffer + bufstream->mem.pos;
		size = vsf_bufstream_get_data_length(stream);
	}

	if (ptr)
		*ptr = p;
	return size;
}

static uint32_t
vsf_bufstream_write(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	uint32_t wsize = 0;

	if (bufstream->mem.read)
	{
		bufstream->mem.buffer = *buffer;
		bufstream->mem.pos = 0;
		wsize = buffer->size;
	}
	else
	{
		uint32_t avail_len = vsf_bufstream_get_avail_length(stream);
		wsize = min(avail_len, buffer->size);
		if (buffer->buffer && wsize)
		{
			memcpy(bufstream->mem.buffer.buffer + bufstream->mem.pos,
					buffer->buffer, wsize);
			bufstream->mem.pos += wsize;
		}
	}
	return wsize;
}

static uint32_t
vsf_bufstream_read(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	struct vsf_bufstream_t *bufstream = (struct vsf_bufstream_t *)stream;
	uint32_t rsize = 0;

	if (bufstream->mem.read)
	{
		uint32_t data_len = vsf_bufstream_get_data_length(stream);
		rsize = min(data_len, buffer->size);
		if (buffer->buffer && rsize)
		{
			memcpy(buffer->buffer,
					bufstream->mem.buffer.buffer + bufstream->mem.pos, rsize);
			bufstream->mem.pos += rsize;
		}
	}
	else
	{
		bufstream->mem.buffer = *buffer;
		bufstream->mem.pos = 0;
		// just to notify the rx end
		rsize = 1;
	}
	return rsize;
}

const struct vsf_stream_op_t vsf_fifostream_op =
{
	.init = vsf_fifostream_init,
	.fini = vsf_fifostream_init,
	.write = vsf_fifostream_write,
	.read = vsf_fifostream_read,
	.get_data_length = vsf_fifostream_get_data_length,
	.get_avail_length = vsf_fifostream_get_avail_length,
	.get_wbuf = vsf_fifostream_get_wbuf,
	.get_rbuf = vsf_fifostream_get_rbuf,
};

const struct vsf_stream_op_t vsf_mbufstream_op =
{
	.init = vsf_mbufstream_init,
	.fini = vsf_mbufstream_init,
	.write = vsf_mbufstream_write,
	.read = vsf_mbufstream_read,
	.get_data_length = vsf_mbufstream_get_data_length,
	.get_avail_length = vsf_mbufstream_get_avail_length,
	.get_wbuf = vsf_mbufstream_get_wbuf,
	.get_rbuf = vsf_mbufstream_get_rbuf,
};

const struct vsf_stream_op_t vsf_bufstream_op =
{
	.init = vsf_bufstream_init,
	.fini = vsf_bufstream_init,
	.write = vsf_bufstream_write,
	.read = vsf_bufstream_read,
	.get_data_length = vsf_bufstream_get_data_length,
	.get_avail_length = vsf_bufstream_get_avail_length,
	.get_wbuf = vsf_bufstream_get_wbuf,
	.get_rbuf = vsf_bufstream_get_rbuf,
};

