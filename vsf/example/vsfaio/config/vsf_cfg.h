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

#define VSFCFG_MAX_SRT_PRIO			0xFF

#define VSFCFG_DEBUG
#define VSFCFG_DEBUG_BUFLEN			256

#define VSFCFG_THREAD_SAFTY

// usbd
//#define VSFUSBD_CFG_MPS					192
#define VSFUSBD_CFG_HIGHSPEED
//#define VSFUSBD_CFG_FULLSPEED
//#define VSFUSBD_CFG_LOWSPEED
#define VSFUSBD_CFG_EPMAXNO				8

// vsfip
#define VSFIP_CFG_MTU					1500
#define VSFIP_CFG_TCP_RX_WINDOW			4500
#define VSFIP_CFG_TCP_TX_WINDOW			3000
#define VSFIP_CFG_NETIF_HEADLEN			64

// usblyzer
#define VSF_USBLYZER_URB_DELAY			30
