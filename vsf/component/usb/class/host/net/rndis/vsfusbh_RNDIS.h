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

#ifndef __VSFUSBH_RNDIS_H_INCLUDED__
#define __VSFUSBH_RNDIS_H_INCLUDED__

extern const struct vsfusbh_class_drv_t vsfusbh_rndis_drv;

struct vsfusbh_rndis_cb_t
{
	void *param;
	void (*on_connect)(void *param, struct vsfip_netif_t *netif);
	void (*on_disconnect)(void *param, struct vsfip_netif_t *netif);
};
extern struct vsfusbh_rndis_cb_t vsfusbh_rndis_cb;

#endif // __VSFUSBH_RNDIS_H_INCLUDED__
