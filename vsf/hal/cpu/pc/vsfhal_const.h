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

#ifndef __VSFHAL_CONST_H_INCLUDED__
#define __VSFHAL_CONST_H_INCLUDED__

#define VSFHAL_DUMMY_PORT					0xFF

#define VSFHAL_SLEEP_WFI					0

#define VSFHAL_GPIO_INPUT					0
#define VSFHAL_GPIO_OUTPP					1
#define VSFHAL_GPIO_OUTOD					2
#define VSFHAL_GPIO_PULLUP					3
#define VSFHAL_GPIO_PULLDOWN				4

#define vsfhal_usart_t						uint8_t
#define vsfhal_i2c_t						uint8_t

#define vsf_gint_t							bool
void vsf_enter_critical(void);
void vsf_leave_critical(void);
void vsf_set_gint(bool enter);
bool vsf_get_gint(void);

extern const struct vsf_stream_op_t stdout_stream_op;
extern const struct vsf_stream_op_t stdin_stream_op;

#endif	// __VSFHAL_CONST_H_INCLUDED__

