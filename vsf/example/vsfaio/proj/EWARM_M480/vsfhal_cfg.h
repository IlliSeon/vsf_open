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
#ifndef __VSFHAL_CFG_H__
#define __VSFHAL_CFG_H__

#ifndef CORE_CLKEN
#	define CORE_CLKEN						M480_CLK_HXT
#endif
#ifndef CORE_HCLKSRC
#	define CORE_HCLKSRC						M480_HCLKSRC_PLLFOUT
#endif
#ifndef CORE_PCLKSRC
#	define CORE_PCLKSRC						M480_PCLKSRC_HCLK
#endif
#ifndef CORE_PLLSRC
#	define CORE_PLLSRC						M480_PLLSRC_HXT
#endif
#ifndef OSC0_FREQ_HZ
#	define OSC0_FREQ_HZ						(12 * 1000 * 1000)
#endif
#ifndef OSC32_FREQ_HZ
#	define OSC32_FREQ_HZ					0
#endif
#ifndef CORE_PLL_FREQ_HZ
#	define CORE_PLL_FREQ_HZ					(384 * 1000 * 1000) 
#endif
#ifndef CPU_FREQ_HZ
#	define CPU_FREQ_HZ						(192 * 1000 * 1000)
#endif
#ifndef HCLK_FREQ_HZ
#	define HCLK_FREQ_HZ						(192 * 1000 * 1000)
#endif
#ifndef PCLK_FREQ_HZ
#	define PCLK_FREQ_HZ						(192 * 1000 * 1000)
#endif
#ifndef CORE_VECTOR_TABLE
#	define CORE_VECTOR_TABLE				FLASH_LOAD_OFFSET
#endif

#define VSFHAL_CFG_USBD_ONNAK_EN

#endif // __VSFHAL_CFG_H__
