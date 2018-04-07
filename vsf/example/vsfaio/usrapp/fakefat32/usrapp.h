struct usrapp_t
{
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
			struct vsfusbd_MSCBOT_param_t param;
		} msc;
		struct vsfusbd_iface_t ifaces[1];
		struct vsfusbd_config_t config[1];
		struct vsfusbd_device_t device;
	} usbd;
};

extern struct usrapp_t usrapp;

void usrapp_initial_init(struct usrapp_t *app);
void usrapp_srt_init(struct usrapp_t *app);
