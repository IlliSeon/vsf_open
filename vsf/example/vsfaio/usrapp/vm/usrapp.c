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
#include "vsfvm_objdump.h"

struct usrapp_t usrapp;
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
		uint8_t ConfigDescriptor[139];
		uint8_t ReportDescriptor[113];
		uint8_t StringLangID[4];
		uint8_t StringVendor[20];
		uint8_t StringProduct[14];
		uint8_t StringFunc_CDC[14];
		uint8_t StringFunc_MSC[14];
		uint8_t StringFunc_HID[14];
		struct vsfusbd_desc_filter_t StdDesc[9];
		struct vsfusbd_desc_filter_t HIDDesc[2];
	} usbd;

	struct
	{
		char *py;
	} ide;
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
		139, 0,		// wTotalLength
		0x04,		// bNumInterfaces: 4 interfaces
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

		// IDA for HID
		USB_DT_INTERFACE_ASSOCIATION_SIZE,
		USB_DT_INTERFACE_ASSOCIATION,
		3,			// bFirstInterface
		1,			// bInterfaceCount
		0x03,		// bFunctionClass
		0x01,		// bFunctionSubClass
		0x01,		// bFunctionProtocol
		0x06,		// iFunction

		USB_DT_INTERFACE_SIZE,
		USB_DT_INTERFACE,
		0x03,		// bInterfaceNumber: Number of Interface
		0x00,		// bAlternateSetting: Alternate setting
		0x01,		// bNumEndpoints
		0x03,		// bInterfaceClass: HID 
		0x01,		// bInterfaceSubClass: boot
		0x01,		// nInterfaceProtocol: keyboard
		0x06,		// iInterface

		0x09,		// bLength: HID Descriptor size
		USB_HID_DT_HID,		// bDescriptorType: HID
		0x11,		// bcdHID: HID Class Spec release number
		0x01,
		0x00,		// bCountryCode: Hardware target country
		0x01,		// bNumDescriptors: Number of HID class descriptors to follow
		0x22,		// bDescriptorType
		113, 0,		// wItemLength: Total length of Report descriptor

		USB_DT_ENDPOINT_SIZE,
		USB_DT_ENDPOINT,
		0x84,		// bEndpointAddress: (IN4)
		0x03,		// bmAttributes: Interrupt
		0x0d,		// wMaxPacketSize:
		0x00,
		0x0a,
	},
	.usbd.ReportDescriptor =
	{
		0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
		0x09, 0x06,			// USAGE (Keyboard)
		0xa1, 0x01,			// COLLECTION (Application)
		0x85, 0x01, 		//	Report ID (1)
		0x05, 0x07,			//	USAGE_PAGE (Keyboard)
		0x19, 0xe0,			//	USAGE_MINIMUM (Keyboard LeftControl)
		0x29, 0xe7,			//	USAGE_MAXIMUM (Keyboard Right GUI)
		0x15, 0x00,			//	LOGICAL_MINIMUM (0)
		0x25, 0x01,			//	LOGICAL_MAXIMUM (1)
		0x75, 0x01,			//	REPORT_SIZE (1)
		0x95, 0x08,			//	REPORT_COUNT (8)
		0x81, 0x02,			//	INPUT (Data,Var,Abs)
		0x95, 0x01,			//	REPORT_COUNT (1)
		0x75, 0x08,			//	REPORT_SIZE (8)
		0x81, 0x03,			//	INPUT (Cnst,Var,Abs) 
		0x95, 0x06,			//	REPORT_COUNT (6)
		0x75, 0x08,			//	REPORT_SIZE (8)
		0x15, 0x00,			//	LOGICAL_MINIMUM (0)
		0x25, 0xFF,			//	LOGICAL_MAXIMUM (255)
		0x05, 0x07,			//	USAGE_PAGE (Keyboard)
		0x19, 0x00,			//	USAGE_MINIMUM (Reserved (no event indicated))
		0x29, 0x65,			//	USAGE_MAXIMUM (Keyboard Application)
		0x81, 0x00,			//	INPUT (Data,Ary,Abs)
		0xc0,				// END_COLLECTION

		0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
		0x09, 0x02,			// USAGE (Mouse)
		0xa1, 0x01,			// COLLECTION (Application)
		0x85, 0x02,			//	Report id (2)
		0x09, 0x01,			//	USAGE (Pointer)
		0xa1, 0x00,			//	COLLECTION (Physical)
		0x05, 0x09,			//		USAGE_PAGE (Button)
		0x19, 0x01,			//		USAGE_MINIMUM (Button 1)
		0x29, 0x05,			//		USAGE_MAXIMUM (Button 5)
		0x15, 0x00,			//		LOGICAL_MINIMUM (0)
		0x25, 0x01,			//		LOGICAL_MAXIMUM (1)
		0x95, 0x05,			//		REPORT_COUNT (5)
		0x75, 0x01,			//		REPORT_SIZE (1)
		0x81, 0x02,			//		INPUT (Data,Var,Abs)
		0x95, 0x01,			//		REPORT_COUNT (1)
		0x75, 0x03,			//		REPORT_SIZE (3)
		0x81, 0x03,			//		INPUT (Cnst,Var,Abs)

		0x05, 0x01,			//		USAGE_PAGE (Generic Desktop)
		0x09, 0x30,			//		USAGE (X)
		0x09, 0x31,			//		USAGE (Y)
		0x16, 0x00, 0xf8,	//		LOGICAL_MINIMUM (-32769)
		0x26, 0xff, 0x07,	//		LOGICAL_MAXIMUM (32767)
		0x75, 0x10,			//		REPORT_SIZE (16)
		0x95, 0x02,			//		REPORT_COUNT (2)
		0x81, 0x06,			//		INPUT (Data,Var,Rel)
	
 		0x09, 0x38,			//		USAGE (Wheel)
		0x15, 0x81,			//		LOGICAL_MINIMUM (-127)
		0x25, 0x7f,			//		LOGICAL_MAXIMUM (127)
		0x75, 0x08,			//		REPORT_SIZE (8)
		0x95, 0x01,			//		REPORT_COUNT (1)
		0x81, 0x06,			//		INPUT (Data,Var,Rel)
		0xc0,				//	END_COLLECTION
		0xc0,				// END_COLLECTION
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
		VSFUSBD_DESC_STRING(0x0409, 6, usrapp_param.usbd.StringFunc_HID, sizeof(usrapp_param.usbd.StringFunc_HID)),
		VSFUSBD_DESC_NULL,
	},
	.usbd.HIDDesc = 
	{
		VSFUSBD_DESC_HID_REPORT(usrapp_param.usbd.ReportDescriptor, sizeof(usrapp_param.usbd.ReportDescriptor)),
		VSFUSBD_DESC_NULL,
	},

	.ide.py = "\
#!/usr/bin/python\n\
import os\n\
from Tkinter import *\n\
\n\
rootpath = './'\n\
\n\
srcpath = rootpath + 'source.txt'\n\
compilepath = rootpath + 'compile'\n\
runpath = rootpath + 'run'\n\
stoppath = rootpath + 'stop'\n\
\n\
def writeCtrlFile(path):\n\
	file = open(path, 'rb+')\n\
	config = bytearray(1)\n\
	config[0] = 0xFF\n\
	file.write(config)\n\
	file.close()\n\
\n\
def compile():\n\
	file = open(srcpath, 'rb+')\n\
	file.write(txtSrc.get(0.0, 'end'))\n\
	config = bytearray(1)\n\
	config[0] = 0\n\
	file.write(config)\n\
	file.close()\n\
	writeCtrlFile(compilepath)\n\
\n\
def run():\n\
	writeCtrlFile(runpath)\n\
\n\
def stop():\n\
	writeCtrlFile(stoppath)\n\
\n\
root = Tk()\n\
root.title('VSFVM IDE')\n\
\n\
frame = Frame(root)\n\
frame.pack()\n\
\n\
btnCompile = Button(frame, text='Compile', state='normal', command=compile)\n\
btnCompile.pack(side=LEFT)\n\
btnRun = Button(frame, text='Run', state='normal', command=run)\n\
btnRun.pack(side=LEFT)\n\
btnStop = Button(frame, text='Stop', state='normal', command=stop)\n\
btnStop.pack(side=LEFT)\n\
\n\
txtSrc = Text()\n\
txtSrc.pack(side=BOTTOM)\n\
\n\
root.mainloop()\n",
};

static vsf_err_t usrapp_write_source(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	uint8_t *ptr = usrapp.vsfvm.source;

	if ((offset + pagesize) > sizeof(usrapp.vsfvm.source))
		return VSFERR_FAIL;

	memcpy(ptr + offset, buff, pagesize);
	return VSFERR_NONE;
}

static vsf_err_t usrapp_read_source(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	uint8_t *ptr = usrapp.vsfvm.source;

	if ((offset + pagesize) > sizeof(usrapp.vsfvm.source))
		return VSFERR_FAIL;

	memcpy(buff, ptr + offset, pagesize);
	return VSFERR_NONE;
}

static vsf_err_t usrapp_write_bytecode(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	uint8_t *ptr = (uint8_t *)usrapp.vsfvm.token;

	if ((offset + pagesize) > sizeof(usrapp.vsfvm.token))
		return VSFERR_FAIL;

	memcpy(ptr + offset, buff, pagesize);
	return VSFERR_NONE;
}

static vsf_err_t usrapp_read_bytecode(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	uint8_t *ptr = (uint8_t *)usrapp.vsfvm.token;

	if ((offset + pagesize) > sizeof(usrapp.vsfvm.token))
		return VSFERR_FAIL;

	memcpy(buff, ptr + offset, pagesize);
	return VSFERR_NONE;
}

#define USRAPP_EVT_VM_COMPILE			(VSFSM_EVT_USER + 0)
#define USRAPP_EVT_VM_RUN				(VSFSM_EVT_USER + 1)
#define USRAPP_EVT_VM_STOP				(VSFSM_EVT_USER + 2)

static vsf_err_t usrapp_write_compile(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	vsfsm_post_evt_pending(&usrapp.vsfvm.sm, USRAPP_EVT_VM_COMPILE);
	return VSFERR_NONE;
}

static vsf_err_t usrapp_write_run(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	vsfsm_post_evt_pending(&usrapp.vsfvm.sm, USRAPP_EVT_VM_RUN);
	return VSFERR_NONE;
}

static vsf_err_t usrapp_write_stop(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		struct fakefat32_file_t *file, uint64_t offset, uint8_t *buff,
		uint32_t pagesize)
{
	vsfsm_post_evt_pending(&usrapp.vsfvm.sm, USRAPP_EVT_VM_STOP);
	return VSFERR_NONE;
}

static const char *vsfvmc_errcode_str[] =
{
	TO_STR(VSFVMC_ERRCODE_NONE),

	// common error
	TO_STR(VSFVMC_BUG),
	TO_STR(VSFVMC_NOT_ENOUGH_RESOURCES),
	TO_STR(VSFVMC_FATAL_ERROR),
	TO_STR(VSFVMC_NOT_SUPPORT),

	// lexer error
	TO_STR(VSFVMC_LEXER_NOT_SUPPORT),
	TO_STR(VSFVMC_LEXER_INVALID_OP),
	TO_STR(VSFVMC_LEXER_INVALID_STRING),
	TO_STR(VSFVMC_LEXER_INVALID_ESCAPE),
	TO_STR(VSFVMC_LEXER_SYMBOL_TOO_LONG),

	// parser error
	TO_STR(VSFVMC_PARSER_UNEXPECTED_TOKEN),
	TO_STR(VSFVMC_PARSER_ALREADY_DEFINED),
	TO_STR(VSFVMC_PARSER_INVALID_CLOSURE),
	TO_STR(VSFVMC_PARSER_INVALID_EXPR),
	TO_STR(VSFVMC_PARSER_UNINITED_CONST),
	TO_STR(VSFVMC_PARSER_INVALID_CONST),
	TO_STR(VSFVMC_PARSER_DIV0),
	TO_STR(VSFVMC_PARSER_EXPECT_FUNC_PARAM),
	TO_STR(VSFVMC_PARSER_TOO_MANY_FUNC_PARAM),
	TO_STR(VSFVMC_PARSER_MEMFUNC_NOT_FOUND),

	// compiler error
	TO_STR(VSFVMC_COMPILER_INVALID_MODULE),
	TO_STR(VSFVMC_COMPILER_INVALID_FUNC),
	TO_STR(VSFVMC_COMPILER_INVALID_FUNC_PARAM),
	TO_STR(VSFVMC_COMPILER_FAIL_USRLIB),
};

static struct vsfsm_state_t *
usrapp_vm_evthandler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct usrapp_t *app = (struct usrapp_t *)sm->user_data;
	struct vsfvm_t *vm = &app->vsfvm.vm;
	struct vsfvm_script_t *script = &app->vsfvm.script;

	switch (evt)
	{
	case USRAPP_EVT_VM_COMPILE:
		if (app->vsfvm.compiling)
			return VSFERR_NONE;
		if (app->vsfvm.polling)
		{
			app->vsfvm.polling = false;
			vsfvm_script_fini(vm, script);
			vsfvm_fini(vm);
		}

	{
		struct vsfvmc_t *vmc = &app->vsfvm.vmc;
		uint32_t *ptr, *token;
		char *src = (char *)app->vsfvm.source;
		int err;

		app->vsfvm.compiling = true;
		vsfdbg_prints("start compiling ..." VSFCFG_DEBUG_LINEEND);
		vsfvmc_init(vmc, &usrapp, NULL);
		vsfvmc_register_ext(vmc, &vsfvm_ext_std);
		vsfvmc_ext_register_vsf(vmc);
		vsfvmc_register_lexer(vmc, &app->vsfvm.dart);
		err = vsfvmc_script(vmc, "main.dart");
		if (err < 0) goto err_return;

		err = vsfvmc_input(vmc, src);
		if (err < 0)
		{
		err_return:
			err = -err;
			vsfdbg_printf("command line compile error: %s" VSFCFG_DEBUG_LINEEND,
				(err >= VSFVMC_ERRCODE_END) ? "unknwon error" :
				vsfvmc_errcode_str[err]);
		compile_end:
			vsfvmc_fini(vmc);
			app->vsfvm.compiling = false;
			return VSFERR_NONE;
		}

		err = vsfvmc_input(vmc, "\xFF");
		if (!err)
		{
			if (vmc->bytecode.sp >= dimof(app->vsfvm.token))
			{
				vsfdbg_prints("not enough space for token buffer" VSFCFG_DEBUG_LINEEND);
				goto compile_end;
			}
			vsfdbg_printf("compiled OK, token number : %d" VSFCFG_DEBUG_LINEEND,
					vmc->bytecode.sp);

			token = app->vsfvm.token;
			for (uint32_t i = 0; i < vmc->bytecode.sp; i++)
			{
				ptr = vsf_dynarr_get(&vmc->bytecode.var, i);
				token[i] = *ptr;
			}

			vsfdbg_prints("objdump:" VSFCFG_DEBUG_LINEEND);
			vsfvm_objdump(token, vmc->bytecode.sp);
			app->vsfvm.token_num = vmc->bytecode.sp;
		}
		goto compile_end;
	}
	case USRAPP_EVT_VM_RUN:
		if (!app->vsfvm.polling)
		{
			app->vsfvm.polling = true;

			memset(vm, 0, sizeof(*vm));
			memset(script, 0, sizeof(*script));
			vm->thread_pool.pool_size = 16;
			script->token = app->vsfvm.token;
			script->token_num = app->vsfvm.token_num;

			vsfvm_init(vm);
			vsfvm_register_ext(vm, &vsfvm_ext_std);
			vsfvm_ext_register_vsf(vm);
			vsfvm_script_init(vm, script);
		}
		break;
	case USRAPP_EVT_VM_STOP:
		if (app->vsfvm.polling)
		{
			app->vsfvm.polling = false;

			vsfvm_script_fini(vm, script);
			vsfvm_fini(vm);
		}
		break;
	}
	return NULL;
}

struct usrapp_t usrapp =
{
	.hcd_param.index						= VSFHAL_HCD_INDEX,
	.hcd_param.int_priority					= 0xFF,
	.usbh.hcd								= &vsfohci_drv,
	.usbh.hcd.param							= &usrapp.hcd_param,

	.fakefat32.root_dir						=
	{
		{
			.memfile.file.name				= "VSFVM",
			.memfile.file.attr				= VSFILE_ATTR_VOLUMID,
		},
		{
			.memfile.file.name				= "ide.py",
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_READONLY,
		},
		{
			.memfile.file.name				= "compile",
			.memfile.file.size				= 1,
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_HIDDEN | VSFILE_ATTR_SYSTEM,
			.cb.write						= usrapp_write_compile,
		},
		{
			.memfile.file.name				= "run",
			.memfile.file.size				= 1,
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_HIDDEN | VSFILE_ATTR_SYSTEM,
			.cb.write						= usrapp_write_run,
		},
		{
			.memfile.file.name				= "stop",
			.memfile.file.size				= 1,
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_HIDDEN | VSFILE_ATTR_SYSTEM,
			.cb.write						= usrapp_write_stop,
		},
		{
			.memfile.file.name				= "bytecode.bin",
			.memfile.file.size				= sizeof(usrapp.vsfvm.token),
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_HIDDEN | VSFILE_ATTR_SYSTEM,
			.cb.read						= usrapp_read_bytecode,
			.cb.write						= usrapp_write_bytecode,
		},
		{
			.memfile.file.name				= "source.txt",
			.memfile.file.attr				= VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_HIDDEN | VSFILE_ATTR_SYSTEM,
			.cb.read						= usrapp_read_source,
			.cb.write						= usrapp_write_source,
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

	.usbd.msc.param.ep_in					= 3,
	.usbd.msc.param.ep_out					= 3,
	.usbd.msc.param.scsi_dev				= &usrapp.mal.scsi_dev,

	.usbd.hid.param.ep_in					= 4,
	.usbd.hid.param.has_report_id			= 0,
	.usbd.hid.param.num_of_report			= 1,
	.usbd.hid.param.reports					= usrapp.usbd.hid.reports,
	.usbd.hid.param.desc					= (struct vsfusbd_desc_filter_t *)usrapp_param.usbd.HIDDesc,
	
	.usbd.hid.reports[0].type				= USB_HID_REPORT_INPUT,
	.usbd.hid.reports[0].id					= 2,
	.usbd.hid.reports[0].idle				= 0,
	.usbd.hid.reports[0].buffer.buffer		= usrapp.usbd.hid.report1_buffer,
	.usbd.hid.reports[0].buffer.size		= sizeof(usrapp.usbd.hid.report1_buffer),
	.usbd.hid.reports[0].changed			= true,
	.usbd.hid.report1_buffer[0]				= 2,
	
	.usbd.hid.reports[1].type				= USB_HID_REPORT_INPUT,
	.usbd.hid.reports[1].id					= 1,
	.usbd.hid.reports[1].idle				= 0,
	.usbd.hid.reports[1].buffer.buffer		= usrapp.usbd.hid.report0_buffer,
	.usbd.hid.reports[1].buffer.size		= sizeof(usrapp.usbd.hid.report0_buffer),
	.usbd.hid.reports[1].changed			= true,
	.usbd.hid.report0_buffer[0]				= 2,

	.usbd.ifaces[0].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMControl_class,
	.usbd.ifaces[0].protocol_param			= &usrapp.usbd.cdc.param,
	.usbd.ifaces[1].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMData_class,
	.usbd.ifaces[1].protocol_param			= &usrapp.usbd.cdc.param,
	.usbd.ifaces[2].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_MSCBOT_class,
	.usbd.ifaces[2].protocol_param			= &usrapp.usbd.msc.param,
	.usbd.ifaces[3].class_protocol			= (struct vsfusbd_class_protocol_t *)&vsfusbd_HID_class,
	.usbd.ifaces[3].protocol_param			= &usrapp.usbd.hid.param,
	.usbd.config[0].num_of_ifaces			= dimof(usrapp.usbd.ifaces),
	.usbd.config[0].iface					= usrapp.usbd.ifaces,
	.usbd.device.num_of_configuration		= dimof(usrapp.usbd.config),
	.usbd.device.config						= usrapp.usbd.config,
	.usbd.device.desc_filter				= (struct vsfusbd_desc_filter_t *)usrapp_param.usbd.StdDesc,
	.usbd.device.device_class_iface			= 0,
	.usbd.device.drv						= (struct vsfhal_usbd_t *)&vsfhal_usbd,
	.usbd.device.int_priority				= 0xFF,

	.vsfvm.sm.init_state.evt_handler		= usrapp_vm_evthandler,
	.vsfvm.sm.user_data						= &usrapp,
	.vsfvm.dart.op							= &vsfvmc_lexer_op_dart,
};

static void usrapp_usbd_conn(void *p)
{
	struct usrapp_t *app = (struct usrapp_t *)p;

	VSFSTREAM_INIT(&app->usbd.cdc.stream_rx);
	VSFSTREAM_INIT(&app->usbd.cdc.stream_tx);
	vsfdbg_init((struct vsf_stream_t *)&app->usbd.cdc.stream_tx);

	vsfscsi_init(&app->mal.scsi_dev);

	vsfusbd_device_init(&app->usbd.device);
	app->usbd.device.drv->connect();
	if (app_hwcfg.usbd.pullup.port != VSFHAL_DUMMY_PORT)
		vsfhal_gpio_set(app_hwcfg.usbd.pullup.port, 1 << app_hwcfg.usbd.pullup.pin);

	vsfusbh_init(&usrapp.usbh);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_hub_drv);
	vsfusbh_register_driver(&usrapp.usbh, &vsfusbh_libusb_drv);
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
}

void usrapp_srt_poll(struct usrapp_t *app){}

void usrapp_nrt_init(struct usrapp_t *app)
{
	vsfvm_ext_pool_init(16);
	app->fakefat32.root_dir[1].memfile.file.size = strlen(usrapp_param.ide.py);
	app->fakefat32.root_dir[1].memfile.f.buff = (uint8_t *)usrapp_param.ide.py;
	app->fakefat32.root_dir[6].memfile.file.size = 0;

	vsfsm_init(&app->vsfvm.sm);
}

void usrapp_nrt_poll(struct usrapp_t *app)
{
	if (app->vsfvm.polling)
		vsfvm_poll(&app->vsfvm.vm);
}

bool usrapp_cansleep(struct usrapp_t *app)
{
	return app->vsfvm.vm.appendlist.next == NULL;
}

void usrapp_initial_init(struct usrapp_t *app){}
