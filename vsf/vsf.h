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

#ifndef __VSF_H_INCLUDED__
#define __VSF_H_INCLUDED__

#include "app_cfg.h"
#include "vsf_type.h"

#include "compiler.h"
#include "vsf_cfg.h"
#include "vsfhal.h"

// fundation and tools
#include "component/fundation/buffer/vsfbuffer.h"
#include "component/fundation/stream/vsfstream.h"
#include "component/stream/usart_stream/vsf_usart_stream.h"

#include "component/tool/dynpool/vsf_dynpool.h"
#include "component/tool/dynarr/vsf_dynarr.h"
#include "component/tool/dynstack/vsf_dynstack.h"

// framework
#include "framework/vsfsm/vsfsm.h"
#include "framework/vsftimer/vsftimer.h"

// components
#include "component/mal/vsfmal.h"
#include "component/mal/vsfscsi.h"
#include "component/mal/drivers/embflash/embflash.h"
#include "component/file/vsfile.h"
#include "component/file/fs/malfs/vsf_malfs.h"
#include "component/file/fs/malfs/fat/vsffat.h"
#include "component/file/fs/fakefs/fakefat32/fakefat32.h"
#include "component/debug/vsfdbg.h"

#include "component/input/hid/vsfhid.h"

#include "component/shell/vsfshell.h"

#include "component/tcpip/vsfip.h"
#include "component/tcpip/netif/eth/vsfip_eth.h"
#include "component/tcpip/proto/dhcp/vsfip_dhcpc.h"
#include "component/tcpip/proto/dhcp/vsfip_dhcpd.h"
#include "component/tcpip/proto/dns/vsfip_dnsc.h"
#include "component/tcpip/proto/http/vsfip_httpc.h"
#include "component/tcpip/proto/http/vsfip_httpd.h"
#include "component/tcpip/proto/telnet/vsfip_telnetd.h"

#include "component/usb/core/vsfusbd.h"
#include "component/usb/class/device/CDC/vsfusbd_CDC.h"
#include "component/usb/class/device/CDC/vsfusbd_CDCACM.h"
#include "component/usb/class/device/CDC/vsfusbd_RNDIS.h"
#include "component/usb/class/device/HID/vsfusbd_HID.h"
#include "component/usb/class/device/MSC/vsfusbd_MSC_BOT.h"

#include "component/usb/core/vsfusbh.h"
#include "component/usb/core/hcd/vsfhcd.h"
#include "component/usb/core/hcd/ohci/vsfohci.h"
#include "component/usb/core/dwc_otg/vsfdwcotg.h"
#include "component/usb/class/host/HUB/vsfusbh_HUB.h"
#include "component/usb/class/host/HID/vsfusbh_HID.h"
#include "component/usb/class/host/MSC/vsfusbh_MSC.h"
#include "component/usb/class/host/net/ecm/vsfusbh_CDCECM.h"
#include "component/usb/class/host/net/ipheth/vsfusbh_ipheth.h"
#include "component/usb/class/host/net/rndis/vsfusbh_RNDIS.h"
#include "component/usb/class/host/libusb/vsfusbh_libusb.h"
#include "component/usb/class/host/usblyzer/vsfusbh_USBLyzer.h"

#endif		// __VSF_H_INCLUDED__
