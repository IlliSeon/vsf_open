struct usrapp_t
{
	struct vsfusbh_t usbh;
	struct vsfohci_hcd_param_t hcd_param;

	struct
	{
		struct vsf_usart_stream_t uart_stream;
		struct vsf_fifostream_t stream_tx;
		struct vsf_fifostream_t stream_rx;
		uint8_t txbuff[100 * 1024];
		uint8_t rxbuff[65];
	} debug;
};

extern struct usrapp_t usrapp;

void usrapp_initial_init(struct usrapp_t *app);
void usrapp_srt_init(struct usrapp_t *app);
#if defined(APPCFG_USR_POLL)
void usrapp_poll(struct usrapp_t *app);
#endif
