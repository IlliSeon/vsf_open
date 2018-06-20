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

#ifndef __VSFDBG_H_INCLUDED__
#define __VSFDBG_H_INCLUDED__

#include <stdarg.h>

#ifndef VSFCFG_DEBUG_LINEEND
#define VSFCFG_DEBUG_LINEEND		"\r\n"
#endif

#ifdef VSFCFG_DEBUG
void vsfdbg_init(struct vsf_stream_t *stream);
void vsfdbg_fini(void);
uint32_t vsfdbg_printr(const char *buff, uint32_t size);
uint32_t vsfdbg_printf_arg(const char *format, va_list *arg);
uint32_t vsfdbg_printf(const char *format, ...);
uint32_t vsfdbg_prints(char *str);
uint32_t vsfdbg_printb(void *buffer, uint32_t len, uint32_t data_size,
		uint32_t data_per_line, bool addr, bool newline);
#else
#define vsfdbg_init(...)
#define vsfdbg_fini(...)
#define vsfdbg_printr(...)
#define vsfdbg_printf_arg(...)
#define vsfdbg_printf(...)
#define vsfdbg_prints(...)
#define vsfdbg_printb(...)
#endif

#endif // __VSFDBG_H_INCLUDED__
