
#ifndef __USRAPP_H_INCLUDED__
#define __USRAPP_H_INCLUDED__

#include "vsfvm.h"
#include "lexer/dart/vsfvm_lexer_dart.h"
#include "vsfvm_compiler.h"
#include "std/vsfvm_ext_std.h"
#include "vsf/vsfvm_ext_vsf.h"

struct usrapp_t
{
	struct vsfusbh_t usbh;
	struct vsfohci_hcd_param_t hcd_param;

	struct
	{
		struct fakefat32_file_t root_dir[8];
	} fakefat32;

	struct
	{
		struct vsfscsi_device_t scsi_dev;
		struct vsfscsi_lun_t lun[1];

		struct vsf_scsistream_t scsistream;
		struct vsf_mal2scsi_t mal2scsi;
		struct vsfmal_t mal;
		struct fakefat32_param_t fakefat32;
		uint8_t *pbuffer[2];
		uint8_t buffer[2][512];
	} mal;

	struct
	{
		struct
		{
			struct vsfusbd_CDCACM_param_t param;
			struct vsf_fifostream_t stream_tx;
			struct vsf_fifostream_t stream_rx;
			uint8_t txbuff[4 * 1024];
			uint8_t rxbuff[65];
		} cdc;
		struct
		{
			struct vsfusbd_MSCBOT_param_t param;
		} msc;
		struct
		{
			struct vsfusbd_HID_param_t param;
			struct vsfusbd_HID_report_t reports[2];
			uint8_t report0_buffer[9];
			uint8_t report1_buffer[7];
		} hid;
		struct vsfusbd_iface_t ifaces[4];
		struct vsfusbd_config_t config[1];
		struct vsfusbd_device_t device;
	} usbd;

	struct
	{
		struct vsfvm_t vm;
		struct vsfvm_script_t script;
		struct vsfsm_t sm;
		bool polling;

		struct vsfvmc_t vmc;
		struct vsfvmc_lexer_list_t dart;
		bool compiling;

		uint32_t token[4 * 1024];
		uint32_t token_num;
		uint8_t source[4 * 1024];
	} vsfvm;
};

extern struct usrapp_t usrapp;

void usrapp_initial_init(struct usrapp_t *app);
bool usrapp_cansleep(struct usrapp_t *app);
void usrapp_srt_init(struct usrapp_t *app);
void usrapp_srt_poll(struct usrapp_t *app);
void usrapp_nrt_init(struct usrapp_t *app);
void usrapp_nrt_poll(struct usrapp_t *app);

#endif		// __USRAPP_H_INCLUDED__
