struct usrapp_net_t
{
	struct vsfip_netif_t *netif;
	struct vsfip_dhcpc_t dhcpc;
	struct vsfip_ipaddr_t ipaddr;
};

struct usrapp_t
{
	struct vsfusbh_t usbh;
	struct vsfohci_hcd_param_t hcd_param;

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
		struct vsfusbd_iface_t ifaces[2];
		struct vsfusbd_config_t config[1];
		struct vsfusbd_device_t device;
	} usbd;

	struct
	{
		struct usrapp_net_t ipheth;
		struct usrapp_net_t ecm;
	} net;
};

extern struct usrapp_t usrapp;

void usrapp_initial_init(struct usrapp_t *app);
void usrapp_srt_init(struct usrapp_t *app);
void usrapp_nrt_init(struct usrapp_t *app);
