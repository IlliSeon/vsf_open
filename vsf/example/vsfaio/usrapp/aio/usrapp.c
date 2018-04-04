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

#include "fakefat32_fs.h"
#include "busybox.h"

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
		uint8_t ConfigDescriptor[106];
		uint8_t StringLangID[4];
		uint8_t StringVendor[20];
		uint8_t StringProduct[14];
		uint8_t StringFunc_CDC[14];
		uint8_t StringFunc_MSC[14];
		struct vsfusbd_desc_filter_t StdDesc[9];
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
		106, 0,		// wTotalLength
		0x03,		// bNumInterfaces: 3 interfaces
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

		// IDA for MSC
		USB_DT_INTERFACE_ASSOCIATION_SIZE,
		USB_DT_INTERFACE_ASSOCIATION,
		2,			// bFirstInterface
		1,			// bInterfaceCount
		0x08,		// bFunctionClass
		0x06,		// bFunctionSubClass
		0x50,		// bFunctionProtocol
		0x05,		// iFunction

		USB_DT_INTERFACE_SIZE,
		USB_DT_INTERFACE,
		0x02,		// bInterfaceNumber: Number of Interface
		0x00,		// bAlternateSetting: Alternate setting
		0x02,		// bNumEndpoints
		0x08,		// bInterfaceClass
		0x06,		// bInterfaceSubClass
		0x50,		// nInterfaceProtocol
		0x05,		// iInterface:

		USB_DT_ENDPOINT_SIZE,
		USB_DT_ENDPOINT,
		0x83,		// bEndpointAddress: (IN3)
		0x02,		// bmAttributes: Bulk
#if defined(VSFUSBD_CFG_HIGHSPEED)
		0x00, 0x02,	// wMaxPacketSize:
#elif defined(VSFUSBD_CFG_FULLSPEED)
		0x40, 0x00,	// wMaxPacketSize:
#endif
		0x00,		// bInterval:

		USB_DT_ENDPOINT_SIZE,
		USB_DT_ENDPOINT,
		0x03,		// bEndpointAddress: (OUT3)
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
	.usbd.StringFunc_MSC =
	{
		14,
		USB_DT_STRING,
		'V', 0, 'S', 0, 'F', 0, 'M', 0, 'S', 0, 'C', 0,
	},
	.usbd.StdDesc =
	{
		VSFUSBD_DESC_DEVICE(0, usrapp_param.usbd.DeviceDescriptor, sizeof(usrapp_param.usbd.DeviceDescriptor)),
		VSFUSBD_DESC_CONFIG(0, 0, usrapp_param.usbd.ConfigDescriptor, sizeof(usrapp_param.usbd.ConfigDescriptor)),
		VSFUSBD_DESC_STRING(0, 0, usrapp_param.usbd.StringLangID, sizeof(usrapp_param.usbd.StringLangID)),
		VSFUSBD_DESC_STRING(0x0409, 1, usrapp_param.usbd.StringVendor, sizeof(usrapp_param.usbd.StringVendor)),
		VSFUSBD_DESC_STRING(0x0409, 2, usrapp_param.usbd.StringProduct, sizeof(usrapp_param.usbd.StringProduct)),
		VSFUSBD_DESC_STRING(0x0409, 4, usrapp_param.usbd.StringFunc_CDC, sizeof(usrapp_param.usbd.StringFunc_CDC)),
		VSFUSBD_DESC_STRING(0x0409, 5, usrapp_param.usbd.StringFunc_MSC, sizeof(usrapp_param.usbd.StringFunc_MSC)),
		VSFUSBD_DESC_NULL,
	},
};

vsf_err_t usrapp_init_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
struct usrapp_t usrapp =
{
	.hcd_param.index						= VSFHAL_HCD_INDEX,
	.hcd_param.int_priority					= 0xFF,
	.usbh.hcd								= &vsfohci_drv,
	.usbh.hcd.param							= &usrapp.hcd_param,

	.mal.fakefat32.sector_size				= 512,
	.mal.fakefat32.sector_number			= 0x00001000,
	.mal.fakefat32.sectors_per_cluster		= 8,
	.mal.fakefat32.volume_id				= 0x0CA93E47,
	.mal.fakefat32.disk_id					= 0x12345678,
	.mal.fakefat32.root[0].memfile.file.name= "ROOT",
	.mal.fakefat32.root[0].memfile.d.child	= (struct vsfile_memfile_t *)fakefat32_root_dir,

	.mal.mal.drv							= &fakefat32_mal_drv,
	.mal.mal.param							= &usrapp.mal.fakefat32,
	.mal.pbuffer[0]							= usrapp.mal.buffer[0],
	.mal.pbuffer[1]							= usrapp.mal.buffer[1],
	.mal.scsistream.mbuf.count				= dimof(usrapp.mal.buffer),
	.mal.scsistream.mbuf.size				= sizeof(usrapp.mal.buffer[0]),
	.mal.scsistream.mbuf.buffer_list		= usrapp.mal.pbuffer,

	.mal.mal2scsi.malstream.mal				= &usrapp.mal.mal,
	.mal.mal2scsi.cparam.block_size			= 512,
	.mal.mal2scsi.cparam.removable			= false,
	.mal.mal2scsi.cparam.vendor				= "Simon   ",
	.mal.mal2scsi.cparam.product			= "VSFDriver       ",
	.mal.mal2scsi.cparam.revision			= "1.00",
	.mal.mal2scsi.cparam.type				= SCSI_PDT_DIRECT_ACCESS_BLOCK,

	.mal.lun[0].op							= (struct vsfscsi_lun_op_t *)&vsf_mal2scsi_op,
	// lun->stream MUST be scsistream for mal2scsi
	.mal.lun[0].stream						= (struct vsf_stream_t *)&usrapp.mal.scsistream,
	.mal.lun[0].param						= &usrapp.mal.mal2scsi,
	.mal.scsi_dev.max_lun					= 0,
	.mal.scsi_dev.lun						= usrapp.mal.lun,

	.usbd.cdc.param.CDC.ep_notify			= 1,
	.usbd.cdc.param.CDC.ep_out				= 2,
	.usbd.cdc.param.CDC.ep_in				= 2,
	.usbd.cdc.param.CDC.stream_tx			= (struct vsf_stream_t *)&usrapp.usbd.cdc.stream_tx,
	.usbd.cdc.param.CDC.stream_rx			= (struct vsf_stream_t *)&usrapp.usbd.cdc.stream_rx,
	.usbd.cdc.param.line_coding.bitrate		= 115200,
	.usbd.cdc.param.line_coding.stopbittype	= 0,
	.usbd.cdc.param.line_coding.paritytype	= 0,
	.usbd.cdc.param.line_coding.datatype	= 8,
	.usbd.cdc.stream_tx.stream.op			= &fifostream_op,
	.usbd.cdc.stream_tx.mem.buffer.buffer	= (uint8_t *)&usrapp.usbd.cdc.txbuff,
	.usbd.cdc.stream_tx.mem.buffer.size		= sizeof(usrapp.usbd.cdc.txbuff),
	.usbd.cdc.stream_rx.stream.op			= &fifostream_op,
	.usbd.cdc.stream_rx.mem.buffer.buffer	= (uint8_t *)&usrapp.usbd.cdc.rxbuff,
	.usbd.cdc.stream_rx.mem.buffer.size		= sizeof(usrapp.usbd.cdc.rxbuff),
	.usbd.msc.param.ep_in					= 3,
	.usbd.msc.param.ep_out					= 3,
	.usbd.msc.param.scsi_dev				= &usrapp.mal.scsi_dev,
	.usbd.ifaces[0].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMControl_class,
	.usbd.ifaces[0].protocol_param			= &usrapp.usbd.cdc.param,
	.usbd.ifaces[1].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMData_class,
	.usbd.ifaces[1].protocol_param			= &usrapp.usbd.cdc.param,
	.usbd.ifaces[2].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_MSCBOT_class,
	.usbd.ifaces[2].protocol_param			= &usrapp.usbd.msc.param,
	.usbd.config[0].num_of_ifaces			= dimof(usrapp.usbd.ifaces),
	.usbd.config[0].iface					= usrapp.usbd.ifaces,
	.usbd.device.num_of_configuration		= dimof(usrapp.usbd.config),
	.usbd.device.config						= usrapp.usbd.config,
	.usbd.device.desc_filter				= (struct vsfusbd_desc_filter_t *)usrapp_param.usbd.StdDesc,
	.usbd.device.device_class_iface			= 0,
	.usbd.device.drv						= (struct vsfhal_usbd_t *)&vsfhal_usbd,
	.usbd.device.int_priority				= 0xFF,

	.vsfip.telnetd.telnetd.port						= 23,
	.vsfip.telnetd.telnetd.session_num				= dimof(usrapp.vsfip.telnetd.sessions),
	.vsfip.telnetd.sessions[0].stream_tx			= (struct vsf_stream_t *)&usrapp.vsfip.telnetd.stream_tx,
	.vsfip.telnetd.sessions[0].stream_rx			= (struct vsf_stream_t *)&usrapp.vsfip.telnetd.stream_rx,
	.vsfip.telnetd.stream_tx.stream.op				= &fifostream_op,
	.vsfip.telnetd.stream_tx.mem.buffer.buffer		= (uint8_t *)&usrapp.vsfip.telnetd.txbuff,
	.vsfip.telnetd.stream_tx.mem.buffer.size		= sizeof(usrapp.vsfip.telnetd.txbuff),
	.vsfip.telnetd.stream_rx.stream.op				= &fifostream_op,
	.vsfip.telnetd.stream_rx.mem.buffer.buffer		= (uint8_t *)&usrapp.vsfip.telnetd.rxbuff,
	.vsfip.telnetd.stream_rx.mem.buffer.size		= sizeof(usrapp.vsfip.telnetd.rxbuff),

//	.shell.echo								= true,
//	.shell.stream_tx						= (struct vsf_stream_t *)&usrapp.usbd.cdc.stream_tx,
//	.shell.stream_rx						= (struct vsf_stream_t *)&usrapp.usbd.cdc.stream_rx,
	.shell.echo								= false,
	.shell.stream_tx						= (struct vsf_stream_t *)&usrapp.vsfip.telnetd.stream_tx,
	.shell.stream_rx						= (struct vsf_stream_t *)&usrapp.vsfip.telnetd.stream_rx,

	.fs.fat_mal.realmal						= &usrapp.mal.mal,
	.fs.fat_mal.mal.drv						= &vsfmim_drv,
	// refer to fakefat32, fat_mal should start from mbr(skip the hidden sectors)
	// hidden sectors in fakefat32 is 64, so fat_mal start from 64 * 512 = 32K
	.fs.fat_mal.addr						= 0x8000,
	.fs.fat.malfs.malstream.mal				= &usrapp.fs.fat_mal.mal,

	.pt.user_data							= &usrapp,
	.pt.thread								= usrapp_init_thread,
};

// vsfip buffer manager
static struct vsfip_buffer_t* usrapp_vsfip_get_buffer(uint32_t size)
{
	return VSFPOOL_ALLOC(&usrapp.vsfip.buffer_pool, struct vsfip_buffer_t);
}

static void usrapp_vsfip_release_buffer(struct vsfip_buffer_t *buffer)
{
	VSFPOOL_FREE(&usrapp.vsfip.buffer_pool, buffer);
}

static struct vsfip_socket_t* usrapp_vsfip_get_socket(void)
{
	return VSFPOOL_ALLOC(&usrapp.vsfip.socket_pool, struct vsfip_socket_t);
}

static void usrapp_vsfip_release_socket(struct vsfip_socket_t *socket)
{
	VSFPOOL_FREE(&usrapp.vsfip.socket_pool, socket);
}

static struct vsfip_tcppcb_t* usrapp_vsfip_get_tcppcb(void)
{
	return VSFPOOL_ALLOC(&usrapp.vsfip.tcppcb_pool, struct vsfip_tcppcb_t);
}

static void usrapp_vsfip_release_tcppcb(struct vsfip_tcppcb_t *tcppcb)
{
	VSFPOOL_FREE(&usrapp.vsfip.tcppcb_pool, tcppcb);
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

// vsfile_memop
struct vsfile_vfsfile_t* usrapp_vsfile_alloc_vfs(void)
{
	return VSFPOOL_ALLOC(&usrapp.fs.vfsfile_pool, struct vsfile_vfsfile_t);
}

static void usrapp_vsfile_free_vfs(struct vsfile_vfsfile_t *vfsfile)
{
	VSFPOOL_FREE(&usrapp.fs.vfsfile_pool, vfsfile);
}

static const struct vsfile_memop_t usrapp_vsfile_memop =
{
	.alloc_vfs = usrapp_vsfile_alloc_vfs,
	.free_vfs = usrapp_vsfile_free_vfs,
};

void usrapp_net_on_connect(void *param, struct vsfip_netif_t *netif)
{
	struct usrapp_t *app = (struct usrapp_t *)param;
	vsfip_dhcpc_start(netif, &app->vsfip.dhcpc);
}

static void usrapp_on_input(uint16_t generic_usage, struct vsfhid_event_t *event)
{
	if (event != NULL)
	{
		vsfdbg_printf("hid(%d): page=%d, id=%d, pre=%d, cur=%d" VSFCFG_DEBUG_LINEEND,
						generic_usage, event->usage_page, event->usage_id,
						event->pre_value, event->cur_value);
	}
}

vsf_err_t usrapp_init_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct usrapp_t *app = (struct usrapp_t *)pt->user_data;
	struct vsfsm_pt_t *caller_pt = &app->caller_pt;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	{
		struct vsfip_buffer_t *buffer = &app->vsfip.buffer_pool.buffer[0];
		for (int i = 0; i < APPCFG_VSFIP_BUFFER_NUM; i++)
		{
			buffer->buffer = app->vsfip.buffer_mem[i];
			buffer++;
		}
	}
	VSFPOOL_INIT(&app->vsfip.buffer_pool, struct vsfip_buffer_t, APPCFG_VSFIP_BUFFER_NUM);
	VSFPOOL_INIT(&app->vsfip.socket_pool, struct vsfip_socket_t, APPCFG_VSFIP_SOCKET_NUM);
	VSFPOOL_INIT(&app->vsfip.tcppcb_pool, struct vsfip_tcppcb_t, APPCFG_VSFIP_TCPPCB_NUM);
	vsfip_init((struct vsfip_mem_op_t *)&usrapp_vsfip_mem_op);

	STREAM_INIT(&app->vsfip.telnetd.stream_rx);
	STREAM_INIT(&app->vsfip.telnetd.stream_tx);
	vsfip_telnetd_start(&app->vsfip.telnetd.telnetd);

	vsfip_dnsc_init();

	vsfhid.report = usrapp_on_input;

	vsfusbh_init(&usrapp.usbh);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_hub_drv);
	vsfusbh_ecm_cb.param = app;
	vsfusbh_ecm_cb.on_connect = usrapp_net_on_connect;
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_ecm_drv);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_hid_drv);

	STREAM_INIT(&app->usbd.cdc.stream_rx);
	STREAM_INIT(&app->usbd.cdc.stream_tx);
	vsfdbg_init((struct vsf_stream_t *)&app->usbd.cdc.stream_tx);

	vsfscsi_init(&app->mal.scsi_dev);
	vsfshell_init(&app->shell);

	VSFPOOL_INIT(&app->fs.vfsfile_pool, struct vsfile_vfsfile_t, APPCFG_VSFILE_NUM);
	vsfile_init((struct vsfile_memop_t *)&usrapp_vsfile_memop);

	caller_pt->sm = pt->sm;
	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_addfile(caller_pt, evt, NULL, "msc_root", VSFILE_ATTR_DIRECTORY);
	if (err) return err;

	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_getfile(caller_pt, evt, NULL, "/msc_root", &app->fs.file);
	if (err) return err;

	caller_pt->state = 0;
	caller_pt->user_data = &app->mal.fakefat32;
	vsfsm_pt_entry(pt);
	err = vsfile_mount(caller_pt, evt, (struct vsfile_fsop_t *)&fakefat32_fs_op, app->fs.file);
	if (err) return err;

	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_close(caller_pt, evt, app->fs.file);
	if (err) return err;

	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_addfile(caller_pt, evt, NULL, "msc_mal_root", VSFILE_ATTR_DIRECTORY);
	if (err) return err;

	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_getfile(caller_pt, evt, NULL, "/msc_mal_root", &app->fs.file);
	if (err) return err;

	usrapp.fs.fat_mal.size =
		usrapp.mal.fakefat32.sector_number * usrapp.mal.fakefat32.sector_size;
	caller_pt->state = 0;
	caller_pt->user_data = &app->fs.fat_mal.mal;
	vsfsm_pt_entry(pt);
	err = vsfmal_init(caller_pt, evt);
	if (err) return err;

	caller_pt->state = 0;
	caller_pt->user_data = &app->fs.fat;
	vsfsm_pt_entry(pt);
	err = vsfile_mount(caller_pt, evt, (struct vsfile_fsop_t *)&vsffat_op, app->fs.file);
	if (err) return err;

	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_close(caller_pt, evt, app->fs.file);
	if (err) return err;

#if 0
	{
		static uint8_t buf[2048];
		static uint32_t rsize;

		caller_pt->state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_getfile(caller_pt, evt, NULL, "/msc_mal_root/Driver/Windows/VSFCDC.inf", &app->fs.file);
		if (err) return err;

		caller_pt->state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_read(caller_pt, evt, app->fs.file, 0, app->fs.file->size, buf, &rsize);
		if (err) return err;

		caller_pt->state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_close(caller_pt, evt, app->fs.file);
		if (err) return err;
	}
#endif

	caller_pt->state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_getfile(caller_pt, evt, NULL, "/", &app->fs.file);
	if (err) return err;

	vsf_busybox_init(&app->shell, app->fs.file);

	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static void usrapp_usbd_conn(void *p)
{
	struct usrapp_t *app = (struct usrapp_t *)p;

	vsfusbd_device_init(&app->usbd.device);
	app->usbd.device.drv->connect();
	if (app_hwcfg.usbd.pullup.port != VSFHAL_DUMMY_PORT)
		vsfhal_gpio_set(app_hwcfg.usbd.pullup.port, 1 << app_hwcfg.usbd.pullup.pin);
}

void usrapp_srt_init(struct usrapp_t *app)
{
	if (app_hwcfg.usbd.pullup.port != VSFHAL_DUMMY_PORT)
	{
		vsfhal_gpio_init(app_hwcfg.usbd.pullup.port);
		vsfhal_gpio_clear(app_hwcfg.usbd.pullup.port, 1 << app_hwcfg.usbd.pullup.pin);
		vsfhal_gpio_config(app_hwcfg.usbd.pullup.port, app_hwcfg.usbd.pullup.pin, VSFHAL_GPIO_OUTPP);
	}
	app->usbd.device.drv->disconnect();

	vsftimer_create_cb(200, 1, usrapp_usbd_conn, app);
	vsfsm_pt_init(&usrapp.sm, &usrapp.pt);
}

void usrapp_initial_init(struct usrapp_t *app){}
