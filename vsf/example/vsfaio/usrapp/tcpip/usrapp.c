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
#include "usrapp.h"

struct app_hwcfg_t
{
	struct
	{
		struct vsfhal_gpio_pin_t pullup;
	} usbd;
} static const app_hwcfg =
{
	.usbd.pullup.port = USB_PULLUP_PORT,
	.usbd.pullup.pin = USB_PULLUP_PIN,
};

struct usrapp_param_t
{
	struct
	{
		uint8_t DeviceDescriptor[18];
		uint8_t ConfigDescriptor[75];
		uint8_t StringLangID[4];
		uint8_t StringVendor[20];
		uint8_t StringProduct[14];
		uint8_t StringFunc_CDC[14];
		uint8_t StringFunc_MSC[14];
		struct vsfusbd_desc_filter_t StdDesc[7];
	} usbd;
} static const usrapp_param =
{
	.usbd.DeviceDescriptor =
	{
		USB_DT_DEVICE_SIZE,
		USB_DT_DEVICE,
		0x00, 0x02,	// bcdUSB
		0xEF,		// device class: IAD
		0x02,		// device sub class
		0x01,		// device protocol
		64,			// max packet size
		(GENERATE_HEX(APPCFG_USBD_VID) >> 0) & 0xFF,
		(GENERATE_HEX(APPCFG_USBD_VID) >> 8) & 0xFF,
					// vendor
		(GENERATE_HEX(APPCFG_USBD_PID) >> 0) & 0xFF,
		(GENERATE_HEX(APPCFG_USBD_PID) >> 8) & 0xFF,
					// product
		0x00, 0x02,	// bcdDevice
		1,			// manu facturer
		2,			// product
		0,			// serial number
		1,			// number of configuration
	},
	.usbd.ConfigDescriptor =
	{
		USB_DT_CONFIG_SIZE,
		USB_DT_CONFIG,
		(sizeof(usrapp_param.usbd.ConfigDescriptor) >> 0) & 0xFF,
		(sizeof(usrapp_param.usbd.ConfigDescriptor) >> 8) & 0xFF,
					// wTotalLength
		0x02,		// bNumInterfaces: 3 interfaces
		0x01,		// bConfigurationValue: Configuration value
		0x00,		// iConfiguration: Index of string descriptor describing the configuration
		0x80,		// bmAttributes: bus powered
		0x64,		// MaxPower

		// IDA for CDC
		USB_DT_INTERFACE_ASSOCIATION_SIZE,
		USB_DT_INTERFACE_ASSOCIATION,
		0,			// bFirstInterface
		2,			// bInterfaceCount
		0x02,		// bFunctionClass
		0x02,		// bFunctionSubClass
		0x01,		// bFunctionProtocol
		0x04,		// iFunction

		USB_DT_INTERFACE_SIZE,
		USB_DT_INTERFACE,
		0x00,		// bInterfaceNumber: Number of Interface
		0x00,		// bAlternateSetting: Alternate setting
		0x01,		// bNumEndpoints
		0x02,		// bInterfaceClass:
		0x02,		// bInterfaceSubClass:
		0x01,		// nInterfaceProtocol:
		0x04,		// iInterface:

		// Header Functional Descriptor
		0x05,		// bLength: Endpoint Descriptor size
		0x24,		// bDescriptorType: CS_INTERFACE
		0x00,		// bDescriptorSubtype: Header Func Desc
		0x10,		// bcdCDC: spec release number
		0x01,

		// Call Managment Functional Descriptor
		0x05,		// bFunctionLength
		0x24,		// bDescriptorType: CS_INTERFACE
		0x01,		// bDescriptorSubtype: Call Management Func Desc
		0x00,		// bmCapabilities: D0+D1
		0x01,		// bDataInterface: 1

		// ACM Functional Descriptor
		0x04,		// bFunctionLength
		0x24,		// bDescriptorType: CS_INTERFACE
		0x02,		// bDescriptorSubtype: Abstract Control Management desc
		0x02,		// bmCapabilities

		// Union Functional Descriptor
		0x05,		// bFunctionLength
		0x24,		// bDescriptorType: CS_INTERFACE
		0x06,		// bDescriptorSubtype: Union func desc
		0,			// bMasterInterface: Communication class interface
		1,			// bSlaveInterface0: Data Class Interface

		USB_DT_ENDPOINT_SIZE,
		USB_DT_ENDPOINT,
		0x81,		// bEndpointAddress: (IN1)
		0x03,		// bmAttributes: Interrupt
		8, 0x00,	// wMaxPacketSize:
#if defined(VSFUSBD_CFG_HIGHSPEED)
		0x10,		// bInterval:
#elif defined(VSFUSBD_CFG_FULLSPEED)
		0xFF,		// bInterval:
#endif

		USB_DT_INTERFACE_SIZE,
		USB_DT_INTERFACE,
		0x01,		// bInterfaceNumber: Number of Interface
		0x00,		// bAlternateSetting: Alternate setting
		0x02,		// bNumEndpoints
		0x0A,		// bInterfaceClass
		0x00,		// bInterfaceSubClass
		0x00,		// nInterfaceProtocol
		0x04,		// iInterface:

		USB_DT_ENDPOINT_SIZE,
		USB_DT_ENDPOINT,
		0x82,		// bEndpointAddress: (IN2)
		0x02,		// bmAttributes: Bulk
#if defined(VSFUSBD_CFG_HIGHSPEED)
		0x00, 0x02,	// wMaxPacketSize:
#elif defined(VSFUSBD_CFG_FULLSPEED)
		0x40, 0x00,	// wMaxPacketSize:
#endif
		0x00,		// bInterval:

		USB_DT_ENDPOINT_SIZE,
		USB_DT_ENDPOINT,
		0x02,		// bEndpointAddress: (OUT2)
		0x02,		// bmAttributes: Bulk
#if defined(VSFUSBD_CFG_HIGHSPEED)
		0x00, 0x02,	// wMaxPacketSize:
#elif defined(VSFUSBD_CFG_FULLSPEED)
		0x40, 0x00,	// wMaxPacketSize:
#endif
		0x00,		// bInterval:
	},
	.usbd.StringLangID =
	{
		4,
		USB_DT_STRING,
		0x09,
		0x04,
	},
	.usbd.StringVendor =
	{
		20,
		USB_DT_STRING,
		'S', 0, 'i', 0, 'm', 0, 'o', 0, 'n', 0, 'Q', 0, 'i', 0, 'a', 0,
		'n', 0,
	},
	.usbd.StringProduct =
	{
		14,
		USB_DT_STRING,
		'V', 0, 'S', 0, 'F', 0, 'A', 0, 'I', 0, 'O', 0,
	},
	.usbd.StringFunc_CDC =
	{
		14,
		USB_DT_STRING,
		'V', 0, 'S', 0, 'F', 0, 'C', 0, 'D', 0, 'C', 0,
	},
	.usbd.StdDesc =
	{
		VSFUSBD_DESC_DEVICE(0, usrapp_param.usbd.DeviceDescriptor, sizeof(usrapp_param.usbd.DeviceDescriptor)),
		VSFUSBD_DESC_CONFIG(0, 0, usrapp_param.usbd.ConfigDescriptor, sizeof(usrapp_param.usbd.ConfigDescriptor)),
		VSFUSBD_DESC_STRING(0, 0, usrapp_param.usbd.StringLangID, sizeof(usrapp_param.usbd.StringLangID)),
		VSFUSBD_DESC_STRING(0x0409, 1, usrapp_param.usbd.StringVendor, sizeof(usrapp_param.usbd.StringVendor)),
		VSFUSBD_DESC_STRING(0x0409, 2, usrapp_param.usbd.StringProduct, sizeof(usrapp_param.usbd.StringProduct)),
		VSFUSBD_DESC_STRING(0x0409, 4, usrapp_param.usbd.StringFunc_CDC, sizeof(usrapp_param.usbd.StringFunc_CDC)),
		VSFUSBD_DESC_NULL,
	},
};

struct usrapp_t usrapp =
{
	.hcd_param.index						= VSFHAL_HCD_INDEX,
	.hcd_param.int_priority					= 0xFF,
	.usbh.hcd								= &vsfohci_drv,
	.usbh.hcd.param							= &usrapp.hcd_param,

	.usbd.cdc.param.CDC.ep_notify			= 1,
	.usbd.cdc.param.CDC.ep_out				= 2,
	.usbd.cdc.param.CDC.ep_in				= 2,
	.usbd.cdc.param.CDC.stream_tx			= (struct vsf_stream_t *)&usrapp.usbd.cdc.stream_tx,
	.usbd.cdc.param.CDC.stream_rx			= (struct vsf_stream_t *)&usrapp.usbd.cdc.stream_rx,
	.usbd.cdc.param.line_coding.bitrate		= 115200,
	.usbd.cdc.param.line_coding.stopbittype	= 0,
	.usbd.cdc.param.line_coding.paritytype	= 0,
	.usbd.cdc.param.line_coding.datatype	= 8,
	.usbd.cdc.stream_tx.stream.op			= &vsf_fifostream_op,
	.usbd.cdc.stream_tx.mem.buffer.buffer	= (uint8_t *)&usrapp.usbd.cdc.txbuff,
	.usbd.cdc.stream_tx.mem.buffer.size		= sizeof(usrapp.usbd.cdc.txbuff),
	.usbd.cdc.stream_rx.stream.op			= &vsf_fifostream_op,
	.usbd.cdc.stream_rx.mem.buffer.buffer	= (uint8_t *)&usrapp.usbd.cdc.rxbuff,
	.usbd.cdc.stream_rx.mem.buffer.size		= sizeof(usrapp.usbd.cdc.rxbuff),
	.usbd.ifaces[0].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMControl_class,
	.usbd.ifaces[0].protocol_param			= &usrapp.usbd.cdc.param,
	.usbd.ifaces[1].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMData_class,
	.usbd.ifaces[1].protocol_param			= &usrapp.usbd.cdc.param,
	.usbd.config[0].num_of_ifaces			= dimof(usrapp.usbd.ifaces),
	.usbd.config[0].iface					= usrapp.usbd.ifaces,
	.usbd.device.num_of_configuration		= dimof(usrapp.usbd.config),
	.usbd.device.config						= usrapp.usbd.config,
	.usbd.device.desc_filter				= (struct vsfusbd_desc_filter_t *)usrapp_param.usbd.StdDesc,
	.usbd.device.device_class_iface			= 0,
	.usbd.device.drv						= (struct vsfhal_usbd_t *)&vsfhal_usbd,
	.usbd.device.int_priority				= 0xFF,
};

// vsfip buffer manager
static struct vsfip_buffer_t* usrapp_vsfip_get_buffer(uint32_t size)
{
	struct vsfip_buffer_t *buffer = vsf_bufmgr_malloc(sizeof(*buffer) + size);
	if (buffer != NULL)
		buffer->buffer = (uint8_t *)&buffer[1];
	return buffer;
}

static void usrapp_vsfip_release_buffer(struct vsfip_buffer_t *buffer)
{
	vsf_bufmgr_free(buffer);
}

static struct vsfip_socket_t* usrapp_vsfip_get_socket(void)
{
	return (struct vsfip_socket_t *)vsf_bufmgr_malloc(sizeof(struct vsfip_socket_t));
}

static void usrapp_vsfip_release_socket(struct vsfip_socket_t *socket)
{
	vsf_bufmgr_free(socket);
}

static struct vsfip_tcppcb_t* usrapp_vsfip_get_tcppcb(void)
{
	return (struct vsfip_tcppcb_t *)vsf_bufmgr_malloc(sizeof(struct vsfip_tcppcb_t));
}

static void usrapp_vsfip_release_tcppcb(struct vsfip_tcppcb_t *tcppcb)
{
	vsf_bufmgr_free(tcppcb);
}

const struct vsfip_mem_op_t usrapp_vsfip_mem_op =
{
	.get_buffer		= usrapp_vsfip_get_buffer,
	.release_buffer	= usrapp_vsfip_release_buffer,
	.get_socket		= usrapp_vsfip_get_socket,
	.release_socket	= usrapp_vsfip_release_socket,
	.get_tcppcb		= usrapp_vsfip_get_tcppcb,
	.release_tcppcb	= usrapp_vsfip_release_tcppcb,
};

static void usrapp_usbd_conn(void *p)
{
	struct usrapp_t *app = (struct usrapp_t *)p;

	vsfusbd_device_init(&app->usbd.device);
	vsfusbd_connect(&app->usbd.device);
	if (app_hwcfg.usbd.pullup.port != VSFHAL_DUMMY_PORT)
		vsfhal_gpio_set(app_hwcfg.usbd.pullup.port, 1 << app_hwcfg.usbd.pullup.pin);
}

void usrapp_net_on_dhcpc_notify(void *param)
{
	struct usrapp_net_t *net = (struct usrapp_net_t *)param;
	struct vsfip_dhcpc_t *dhcpc = &net->dhcpc;

	if (dhcpc->ready)
		net->ipaddr = net->dhcpc.ipaddr;
	else
	{
		dhcpc->ipaddr.size = 0;
		vsfip_dhcpc_start(net->netif, &net->dhcpc);
	}
}

static void usrapp_net_on_connect(void *param, struct vsfip_netif_t *netif)
{
	struct usrapp_net_t *net = (struct usrapp_net_t *)param;
	if (!net->netif)
	{
		net->netif = netif;
		if (net->ipaddr.size)
			net->dhcpc.ipaddr = net->ipaddr;
		vsfsm_notifier_set_cb(&net->dhcpc.notifier, usrapp_net_on_dhcpc_notify, net);
		vsfip_dhcpc_start(net->netif, &net->dhcpc);
	}
}

static void usrapp_net_on_disconnect(void *param, struct vsfip_netif_t *netif)
{
	struct usrapp_net_t *net = (struct usrapp_net_t *)param;
	if (net->netif == netif)
	{
		vsfip_dhcpc_stop(&net->dhcpc);
		net->netif = NULL;
	}
}

void usrapp_srt_init(struct usrapp_t *app)
{
	if (app_hwcfg.usbd.pullup.port != VSFHAL_DUMMY_PORT)
	{
		vsfhal_gpio_init(app_hwcfg.usbd.pullup.port);
		vsfhal_gpio_clear(app_hwcfg.usbd.pullup.port, 1 << app_hwcfg.usbd.pullup.pin);
		vsfhal_gpio_config(app_hwcfg.usbd.pullup.port, app_hwcfg.usbd.pullup.pin, VSFHAL_GPIO_OUTPP);
	}
	vsfusbd_disconnect(&app->usbd.device);

	vsftimer_create_cb(200, 1, usrapp_usbd_conn, app);

	vsfusbh_init(&usrapp.usbh);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_hub_drv);
	vsfusbh_ipheth_cb.param = &usrapp.net.ipheth;
	vsfusbh_ipheth_cb.on_connect = usrapp_net_on_connect;
	vsfusbh_ipheth_cb.on_disconnect = usrapp_net_on_disconnect;
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_ipheth_drv);
	vsfusbh_ecm_cb.param = &usrapp.net.ecm;
	vsfusbh_ecm_cb.on_connect = usrapp_net_on_connect;
	vsfusbh_ecm_cb.on_disconnect = usrapp_net_on_disconnect;
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_ecm_drv);

	VSFSTREAM_INIT(&app->usbd.cdc.stream_rx);
	VSFSTREAM_INIT(&app->usbd.cdc.stream_tx);
	vsfdbg_init((struct vsf_stream_t *)&app->usbd.cdc.stream_tx);

	vsfip_init((struct vsfip_mem_op_t *)&usrapp_vsfip_mem_op);
}

void usrapp_initial_init(struct usrapp_t *app){}
void usrapp_nrt_init(struct usrapp_t *app){}
