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
#include <stdio.h>

#ifdef VSFCFG_DEBUG

static uint8_t vsfdbg_buf[VSFCFG_DEBUG_BUFLEN];
static struct vsf_stream_t *vsfdbg_stream = NULL;

uint32_t vsfdbg_printr(const char *buff, uint32_t size)
{
	uint32_t ret = 0;

	if (vsfdbg_stream != NULL)
	{
		struct vsf_buffer_t buffer = {(uint8_t *)buff, size};
		uint8_t origlevel = vsfsm_sched_lock();
		ret = vsfstream_write(vsfdbg_stream, &buffer);
		vsfsm_sched_unlock(origlevel);
	}
	return ret;
}

uint32_t vsfdbg_prints(char *str)
{
	return vsfdbg_printr(str, strlen(str));
}

#define VSFDBG_LINEBUF_SIZE			128
uint32_t vsfdbg_printb(void *buffer, uint32_t len, uint32_t data_size,
		uint32_t data_per_line, bool addr, bool newline)
{
	uint8_t *pbuf = (uint8_t *)buffer;

	if (!data_size || (data_size > 4))
		data_size = 1;
	if (!data_per_line)
		data_per_line = len;

	if ((vsfdbg_stream != NULL) && (len > 0))
	{
		const char map[16] = "0123456789ABCDEF";
		char hex[1 + 3 * VSFDBG_LINEBUF_SIZE], *ptr = hex;

		for (uint32_t i = 0; i < len; i++)
		{
			if (!(i % data_per_line) && addr)
			{
				if (ptr != hex)
				{
					*ptr++ = '\0';
					vsfdbg_printf("%s" VSFCFG_DEBUG_LINEEND, hex);
					ptr = hex;
				}
				vsfdbg_printf("%08X: ", i);
			}

			for (uint32_t j = 0; j < data_size; j++, pbuf++)
			{
				*ptr++ = map[(*pbuf >> 4) & 0x0F];
				*ptr++ = map[(*pbuf >> 0) & 0x0F];
			}
			*ptr++ = ' ';
			if (((ptr - hex) >= 9 * VSFDBG_LINEBUF_SIZE) || (i >= (len - 1)))
			{
				*ptr++ = '\0';
				vsfdbg_prints(hex);
				ptr = hex;
			}
    	}
		if (newline)
	    	vsfdbg_prints(VSFCFG_DEBUG_LINEEND);
	}
	return 0;
}

uint32_t vsfdbg_printf_arg(const char *format, va_list *arg)
{
	uint8_t origlevel = vsfsm_sched_lock();
	uint32_t size = vsnprintf((char *)vsfdbg_buf, VSFCFG_DEBUG_BUFLEN, format, *arg);
	struct vsf_buffer_t buffer = {vsfdbg_buf, size};
	size = vsfstream_write(vsfdbg_stream, &buffer);
	vsfsm_sched_unlock(origlevel);

	return size;
}

uint32_t vsfdbg_printf(const char *format, ...)
{
	uint32_t ret = 0;
	if (vsfdbg_stream != NULL)
	{
		va_list ap;

		va_start(ap, format);
		ret = vsfdbg_printf_arg(format, &ap);
		va_end(ap);
	}
	return ret;
}

void vsfdbg_init(struct vsf_stream_t *stream)
{
	vsfdbg_stream = stream;
	if (vsfdbg_stream)
		vsfstream_connect_tx(vsfdbg_stream);
}

void vsfdbg_fini(void)
{
	if (vsfdbg_stream)
		vsfstream_disconnect_tx(vsfdbg_stream);
	vsfdbg_stream = NULL;
}

#endif
