struct usrapp_t
{
	struct vsfusbh_t usbh;
	struct vsfohci_hcd_param_t hcd_param;

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
			uint8_t txbuff[8 * 1024];
			uint8_t rxbuff[65];
		} cdc;
		struct
		{
			struct vsfusbd_MSCBOT_param_t param;
		} msc;
		struct vsfusbd_iface_t ifaces[3];
		struct vsfusbd_config_t config[1];
		struct vsfusbd_device_t device;
	} usbd;

	struct vsfshell_t shell;
	struct
	{
		VSFPOOL_DEFINE(vfsfile_pool, struct vsfile_vfsfile_t, APPCFG_VSFILE_NUM);
		struct vsfile_t *file;

		struct vsfmim_t fat_mal;
		struct vsffat_t fat;
	} fs;

	struct
	{
		VSFPOOL_DEFINE(buffer_pool, struct vsfip_buffer_t, APPCFG_VSFIP_BUFFER_NUM);
		VSFPOOL_DEFINE(socket_pool, struct vsfip_socket_t, APPCFG_VSFIP_SOCKET_NUM);
		VSFPOOL_DEFINE(tcppcb_pool, struct vsfip_tcppcb_t, APPCFG_VSFIP_TCPPCB_NUM);
		uint8_t buffer_mem[APPCFG_VSFIP_BUFFER_NUM][VSFIP_BUFFER_SIZE];

		struct
		{
			struct vsfip_telnetd_t telnetd;
			struct vsfip_telnetd_session_t sessions[1];

			struct vsf_fifostream_t stream_tx;
			struct vsf_fifostream_t stream_rx;
			uint8_t txbuff[65];
			uint8_t rxbuff[65];
		} telnetd;
		struct vsfip_dhcpc_t dhcpc;
	} vsfip;

	struct vsfsm_t sm;
	struct vsfsm_pt_t pt;
	struct vsfsm_pt_t caller_pt;
};

extern struct usrapp_t usrapp;

void usrapp_initial_init(struct usrapp_t *app);
void usrapp_srt_init(struct usrapp_t *app);
