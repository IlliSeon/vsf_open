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
		uint8_t ConfigDescriptor[40];
		uint8_t StringLangID[4];
		uint8_t StringVendor[20];
		uint8_t StringProduct[14];
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
		0x01,		// bNumInterfaces: 1 interfaces
		0x01,		// bConfigurationValue: Configuration value
		0x00,		// iConfiguration: Index of string descriptor describing the configuration
		0x80,		// bmAttributes: bus powered
		0x64,		// MaxPower

		// IDA for MSC
		USB_DT_INTERFACE_ASSOCIATION_SIZE,
		USB_DT_INTERFACE_ASSOCIATION,
		0,			// bFirstInterface
		1,			// bInterfaceCount
		0x08,		// bFunctionClass
		0x06,		// bFunctionSubClass
		0x50,		// bFunctionProtocol
		0x05,		// iFunction

		USB_DT_INTERFACE_SIZE,
		USB_DT_INTERFACE,
		0x01,		// bInterfaceNumber: Number of Interface
		0x00,		// bAlternateSetting: Alternate setting
		0x02,		// bNumEndpoints
		0x08,		// bInterfaceClass
		0x06,		// bInterfaceSubClass
		0x50,		// nInterfaceProtocol
		0x04,		// iInterface:

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
		'V', 0, 'S', 0, 'F', 0, 'M', 0, 'S', 0, 'C', 0,
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
		VSFUSBD_DESC_STRING(0x0409, 4, usrapp_param.usbd.StringFunc_MSC, sizeof(usrapp_param.usbd.StringFunc_MSC)),
		VSFUSBD_DESC_NULL,
	},
};

static vsf_err_t usrapp_write_gpio(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	vsfhal_gpio_init(buff[0]);
	vsfhal_gpio_config(buff[0], buff[1], VSFHAL_GPIO_OUTPP);
	if (buff[2])
		vsfhal_gpio_set(buff[0], 1 << buff[1]);
	else
		vsfhal_gpio_clear(buff[0], 1 << buff[1]);
	return VSFERR_NONE;
}

struct usrapp_t usrapp =
{
	.fakefat32.root_dir						=
	{
		{
			.memfile.file.name				= "VSFMSC",
			.memfile.file.attr				= VSFILE_ATTR_VOLUMID,
		},
		{
			.memfile.file.name				= "gpio",
			.memfile.file.size				= 3,
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE,
			.cb.write						= usrapp_write_gpio,
		},
		{
			.memfile.file.name = NULL,
		},
	},

	.mal.fakefat32.sector_size				= 512,
	.mal.fakefat32.sector_number			= 0x00001000,
	.mal.fakefat32.sectors_per_cluster		= 8,
	.mal.fakefat32.volume_id				= 0x0CA93E47,
	.mal.fakefat32.disk_id					= 0x12345678,
	.mal.fakefat32.root[0].memfile.file.name= "ROOT",
	.mal.fakefat32.root[0].memfile.d.child	= (struct vsfile_memfile_t *)usrapp.fakefat32.root_dir,

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

	.usbd.msc.param.ep_in					= 3,
	.usbd.msc.param.ep_out					= 3,
	.usbd.msc.param.scsi_dev				= &usrapp.mal.scsi_dev,
	.usbd.ifaces[0].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_MSCBOT_class,
	.usbd.ifaces[0].protocol_param			= &usrapp.usbd.msc.param,
	.usbd.config[0].num_of_ifaces			= dimof(usrapp.usbd.ifaces),
	.usbd.config[0].iface					= usrapp.usbd.ifaces,
	.usbd.device.num_of_configuration		= dimof(usrapp.usbd.config),
	.usbd.device.config						= usrapp.usbd.config,
	.usbd.device.desc_filter				= (struct vsfusbd_desc_filter_t *)usrapp_param.usbd.StdDesc,
	.usbd.device.device_class_iface			= 0,
	.usbd.device.drv						= (struct vsfhal_usbd_t *)&vsfhal_usbd,
	.usbd.device.int_priority				= 0xFF,
};

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
	vsfscsi_init(&app->mal.scsi_dev);

	if (app_hwcfg.usbd.pullup.port != VSFHAL_DUMMY_PORT)
	{
		vsfhal_gpio_init(app_hwcfg.usbd.pullup.port);
		vsfhal_gpio_clear(app_hwcfg.usbd.pullup.port, 1 << app_hwcfg.usbd.pullup.pin);
		vsfhal_gpio_config(app_hwcfg.usbd.pullup.port, app_hwcfg.usbd.pullup.pin, VSFHAL_GPIO_OUTPP);
	}
	app->usbd.device.drv->disconnect();

	vsftimer_create_cb(200, 1, usrapp_usbd_conn, app);
}

void usrapp_initial_init(struct usrapp_t *app){}
