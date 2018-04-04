#include "vsf.h"
#include "usrapp.h"

struct vsf_busybox_ctx_t
{
	struct vsfile_t *root;
	struct vsfile_t *curfile;
	uint8_t usrbuf[256];
} vsf_busybox_ctx;
extern struct usrapp_t usrapp;

// common handlers
static vsf_err_t vsf_busybox_help(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_help_t
	{
		struct vsfshell_handler_t *handler;
	} *lparam = (struct vsf_busybox_help_t *)ctx->usrbuf;

	vsfsm_pt_begin(pt);

	lparam->handler = param->shell->handlers;
	while ((lparam->handler != NULL) && (lparam->handler->name != NULL))
	{
		vsfshell_printf(outpt, "%s"VSFSHELL_LINEEND, lparam->handler->name);
		lparam->handler = lparam->handler->next;
	}

	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

// fs handlers
static char* vsf_busybox_filetype(struct vsfile_t *file)
{
	if (file->attr & VSFILE_ATTR_VOLUMID)
		return "VOL";
	else if (file->attr & VSFILE_ATTR_DIRECTORY)
		return "DIR";
	else
		return "FIL";
}
static vsf_err_t vsf_busybox_ls(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_ls_t
	{
		struct vsfile_t *file;
		struct vsfsm_pt_t local_pt;
	} *lparam = (struct vsf_busybox_ls_t *)ctx->usrbuf;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	lparam->local_pt.sm = pt->sm;
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_findfirst(&lparam->local_pt, evt, ctx->curfile, &lparam->file);
	if (err > 0) return err; else if (err < 0) goto end;

	while (lparam->file)
	{
		vsfshell_printf(outpt, "%s(%lld):%s"VSFSHELL_LINEEND,
				vsf_busybox_filetype(lparam->file),
				lparam->file->size, lparam->file->name);

		// close file
		lparam->local_pt.state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_close(&lparam->local_pt, evt, lparam->file);
		if (err > 0) return err; else if (err < 0) goto srch_end;

		lparam->local_pt.state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_findnext(&lparam->local_pt, evt, ctx->curfile, &lparam->file);
		if (err > 0) return err; else if (err < 0) goto srch_end;
	}

srch_end:
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_findend(&lparam->local_pt, evt, ctx->curfile);
	if (err > 0) return err; else if (err < 0) goto end;
end:
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_cd(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_cd_t
	{
		struct vsfile_t *file;
		struct vsfsm_pt_t local_pt;
	} *lparam = (struct vsf_busybox_cd_t *)ctx->usrbuf;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	if (param->argc != 2)
	{
		vsfshell_printf(outpt, "format: %s PATH"VSFSHELL_LINEEND,
							param->argv[0]);
		goto end;
	}
	lparam->local_pt.sm = pt->sm;

	if (!strcmp(param->argv[1], "."))
	{
		goto end;
	}
	else if (!strcmp(param->argv[1], ".."))
	{
		if (ctx->curfile == ctx->root)
		{
			goto end;
		}

		vsfshell_printf(outpt, "not support"VSFSHELL_LINEEND);
		goto end;
	}
	else if (!strcmp(param->argv[1], "/"))
	{
		lparam->file = ctx->root;
	}
	else
	{
		lparam->local_pt.state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_getfile(&lparam->local_pt, evt, ctx->curfile,
								param->argv[1], &lparam->file);
		if (err > 0) return err; else if (err < 0)
		{
			vsfshell_printf(outpt, "directory not found: %s"VSFSHELL_LINEEND,
								param->argv[1]);
			goto end;
		}
	}

	if (!(lparam->file->attr & VSFILE_ATTR_DIRECTORY))
	{
		vsfshell_printf(outpt, "%s is not a directory"VSFSHELL_LINEEND,
								param->argv[1]);
		goto close_end;
	}

	ctx->curfile = (struct vsfile_t *)\
					((uint32_t)lparam->file - (uint32_t)ctx->curfile);
	lparam->file = (struct vsfile_t *)\
					((uint32_t)lparam->file - (uint32_t)ctx->curfile);
	ctx->curfile = (struct vsfile_t *)\
					((uint32_t)lparam->file + (uint32_t)ctx->curfile);

close_end:
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_close(&lparam->local_pt, evt, lparam->file);
	if (err > 0) return err; else if (err < 0) goto end;
end:
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_pwd(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;

	vsfsm_pt_begin(pt);
	vsfshell_printf(outpt, "%s"VSFSHELL_LINEEND, ctx->curfile->name);
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_cat(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_cat_t
	{
		struct vsfile_t *file;
		uint8_t *buff;
		struct vsfsm_pt_t local_pt;
		uint32_t rsize;
		char *line;
		int pos;
		char tmp;
		bool enter;
	} *lparam = (struct vsf_busybox_cat_t *)ctx->usrbuf;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	if (param->argc != 2)
	{
		vsfshell_printf(outpt, "format: %s TEXT_FILE"VSFSHELL_LINEEND,
							param->argv[0]);
		goto end;
	}

	lparam->local_pt.sm = pt->sm;
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_getfile(&lparam->local_pt, evt, ctx->curfile, param->argv[1],
							&lparam->file);
	if (err > 0) return err; else if (err < 0)
	{
		vsfshell_printf(outpt, "file not found: %s"VSFSHELL_LINEEND,
								param->argv[1]);
		goto end;
	}

	if (!(lparam->file->attr & VSFILE_ATTR_ARCHIVE))
	{
		vsfshell_printf(outpt, "%s is not a file"VSFSHELL_LINEEND,
								param->argv[1]);
		goto close_end;
	}

	lparam->buff = vsf_bufmgr_malloc(lparam->file->size + 1);
	if (!lparam->buff)
	{
		vsfshell_printf(outpt, "can not allocate buffer"VSFSHELL_LINEEND);
		goto close_end;
	}

	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_read(&lparam->local_pt, evt, lparam->file, 0,
				lparam->file->size, lparam->buff, &lparam->rsize);
	if (err > 0) return err; else if (err < 0) goto free_end;

	// output line by line, because vsfshell_printf not use big buffer
	lparam->line = (char *)lparam->buff;

	while (lparam->rsize > 0)
	{
		lparam->enter = false;
		for (lparam->pos = 0; lparam->pos < lparam->rsize;)
		{
			if (lparam->line[lparam->pos++] == '\r')
			{
				lparam->tmp = lparam->line[lparam->pos];
				lparam->enter = true;
				break;
			}
		}
		lparam->line[lparam->pos] = '\0';

		vsfshell_printf(outpt, "%s%s", lparam->line,
				lparam->enter ? "" : VSFSHELL_LINEEND);
		lparam->rsize -= strlen(lparam->line);
		lparam->line += strlen(lparam->line);
		lparam->line[0] = lparam->tmp;
	}

free_end:
	vsf_bufmgr_free(lparam->buff);
close_end:
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_close(&lparam->local_pt, evt, lparam->file);
	if (err > 0) return err; else if (err < 0) goto end;
end:
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

// net handlers
extern struct vsfip_t vsfip;
static vsf_err_t vsf_busybox_ipconfig(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_ipconfig_t
	{
		int i;
		struct vsfip_netif_t *netif;
	} *lparam = (struct vsf_busybox_ipconfig_t *)ctx->usrbuf;

	vsfsm_pt_begin(pt);

	lparam->i = 0;
	lparam->netif = vsfip.netif_list;
	while (lparam->netif != NULL)
	{
		vsfshell_printf(outpt,
			"netif%d:"VSFSHELL_LINEEND\
				"\tmac: %02X:%02X:%02X:%02X:%02X:%02X"VSFSHELL_LINEEND\
				"\tipaddr: %d.%d.%d.%d"VSFSHELL_LINEEND\
				"\tnetmask: %d.%d.%d.%d"VSFSHELL_LINEEND\
				"\tgateway: %d.%d.%d.%d"VSFSHELL_LINEEND,
			lparam->i,
			lparam->netif->macaddr.addr.s_addr_buf[0],
			lparam->netif->macaddr.addr.s_addr_buf[1],
			lparam->netif->macaddr.addr.s_addr_buf[2],
			lparam->netif->macaddr.addr.s_addr_buf[3],
			lparam->netif->macaddr.addr.s_addr_buf[4],
			lparam->netif->macaddr.addr.s_addr_buf[5],
			lparam->netif->ipaddr.addr.s_addr_buf[0],
			lparam->netif->ipaddr.addr.s_addr_buf[1],
			lparam->netif->ipaddr.addr.s_addr_buf[2],
			lparam->netif->ipaddr.addr.s_addr_buf[3],
			lparam->netif->netmask.addr.s_addr_buf[0],
			lparam->netif->netmask.addr.s_addr_buf[1],
			lparam->netif->netmask.addr.s_addr_buf[2],
			lparam->netif->netmask.addr.s_addr_buf[3],
			lparam->netif->gateway.addr.s_addr_buf[0],
			lparam->netif->gateway.addr.s_addr_buf[1],
			lparam->netif->gateway.addr.s_addr_buf[2],
			lparam->netif->gateway.addr.s_addr_buf[3]);
		lparam->i++;
		lparam->netif = lparam->netif->next;
	}

	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_arp(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_arp_t
	{
		int i;
		struct vsfip_netif_t *netif;
		struct vsfip_ipaddr_t *ip;
		struct vsfip_macaddr_t *mac;
	} *lparam = (struct vsf_busybox_arp_t *)ctx->usrbuf;

	vsfsm_pt_begin(pt);

	lparam->netif = vsfip.netif_list;
	while (lparam->netif != NULL)
	{
		vsfshell_printf(outpt, "%d.%d.%d.%d:"VSFSHELL_LINEEND,
					lparam->netif->ipaddr.addr.s_addr_buf[0],
					lparam->netif->ipaddr.addr.s_addr_buf[1],
					lparam->netif->ipaddr.addr.s_addr_buf[2],
					lparam->netif->ipaddr.addr.s_addr_buf[3]);
	
		for (lparam->i = 0; lparam->i < dimof(lparam->netif->arp_cache);
				lparam->i++)
		{
			if (lparam->netif->arp_cache[lparam->i].time != 0)
			{
				lparam->ip = &lparam->netif->arp_cache[lparam->i].assoc.ip;
				lparam->mac = &lparam->netif->arp_cache[lparam->i].assoc.mac;
				vsfshell_printf(outpt,
					"\t%d.%d.%d.%d"
					"-%02X:%02X:%02X:%02X:%02X:%02X"VSFSHELL_LINEEND,
					lparam->ip->addr.s_addr_buf[0],
					lparam->ip->addr.s_addr_buf[1],
					lparam->ip->addr.s_addr_buf[2],
					lparam->ip->addr.s_addr_buf[3],
					lparam->mac->addr.s_addr_buf[0],
					lparam->mac->addr.s_addr_buf[1],
					lparam->mac->addr.s_addr_buf[2],
					lparam->mac->addr.s_addr_buf[3],
					lparam->mac->addr.s_addr_buf[4],
					lparam->mac->addr.s_addr_buf[5]);
			}
		}

		lparam->netif = lparam->netif->next;
	}

	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_ping(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;

	vsfsm_pt_begin(pt);
	vsfshell_printf(outpt, "not supported now"VSFSHELL_LINEEND);
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_httpd(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_httpd_t
	{
		struct vsfip_httpd_t *httpd;
		uint16_t port;
		struct vsfile_t *root;
		struct vsfsm_pt_t local_pt;
	} *lparam = (struct vsf_busybox_httpd_t *)ctx->usrbuf;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	if (param->argc != 5)
	{
		vsfshell_printf(outpt,
						"format: %s SERVICE_NUM PORT ROOT HOME"VSFSHELL_LINEEND,
							param->argv[0]);
		goto end;
	}

	memset(lparam, 0, sizeof(*lparam));
	lparam->httpd = vsf_bufmgr_malloc(sizeof(struct vsfip_httpd_t));
	if (!lparam->httpd)
	{
		vsfshell_printf(outpt, "fail to allocate httpd"VSFSHELL_LINEEND);
		goto end;
	}
	memset(lparam->httpd, 0, sizeof(struct vsfip_httpd_t));
	lparam->httpd->service_num = atoi(param->argv[1]);
	if (!lparam->httpd->service_num)
	{
		vsfshell_printf(outpt, "service number is 0"VSFSHELL_LINEEND);
		goto free_end;
	}
	lparam->httpd->service = vsf_bufmgr_malloc(
			lparam->httpd->service_num * sizeof(struct vsfip_httpd_service_t));
	if (!lparam->httpd->service)
	{
		vsfshell_printf(outpt, "fail to allocate %d service(s)"VSFSHELL_LINEEND,
							lparam->httpd->service_num);
		goto free_end;
	}
	lparam->httpd->homepage = vsf_bufmgr_malloc(strlen(param->argv[4]) + 1);
	if (!lparam->httpd->homepage)
	{
		vsfshell_printf(outpt, "fail to allocate homepage: %s"VSFSHELL_LINEEND,
							param->argv[4]);
		goto free_end;
	}
	strcpy(lparam->httpd->homepage, param->argv[4]);

	lparam->port = atoi(param->argv[2]);
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfile_getfile(&lparam->local_pt, evt, ctx->curfile, param->argv[3],
							&lparam->httpd->root);
	if (err > 0) return err; else if (err < 0)
	{
		vsfshell_printf(outpt, "fail to open root %s"VSFSHELL_LINEEND,
							param->argv[3]);
		goto free_end;
	}

	vsfshell_printf(outpt, "start httpd on port %d, root: %s"VSFSHELL_LINEEND,
							lparam->port, param->argv[3]);
	vsfip_httpd_start(lparam->httpd, lparam->port);
	goto end;

free_end:
	if (lparam->httpd->root != NULL)
	{
		lparam->local_pt.state = 0;
		vsfsm_pt_entry(pt);
		err = vsfile_close(&lparam->local_pt, evt, lparam->httpd->root);
		if (err > 0) return err;
	}
	if (lparam->httpd != NULL)
	{
		if (lparam->httpd->service != NULL)
			vsf_bufmgr_free(lparam->httpd->service);
		if (lparam->httpd->homepage != NULL)
			vsf_bufmgr_free(lparam->httpd->homepage);
		vsf_bufmgr_free(lparam->httpd);
	}
end:
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static vsf_err_t vsf_busybox_dns(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *outpt = &param->output_pt;
	struct vsf_busybox_ctx_t *ctx = (struct vsf_busybox_ctx_t *)param->context;
	struct vsf_busybox_dns_t
	{
		struct vsfip_ipaddr_t ip;
		struct vsfsm_pt_t local_pt;
		struct vsfip_ipaddr_t *dns_server;
	} *lparam = (struct vsf_busybox_dns_t *)ctx->usrbuf;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	if ((param->argc > 3) || (param->argc < 2))
	{
		vsfshell_printf(outpt, "format: %s DOMAIN [SERVER]"VSFSHELL_LINEEND,
							param->argv[0]);
		goto end;
	}

	if (param->argc == 3)
	{
		err = vsfip_ip4_pton(&lparam->ip, param->argv[2]);
		if (err < 0)
		{
			vsfshell_printf(outpt,
						"fail to parse ip address: %s"VSFSHELL_LINEEND,
							param->argv[2]);
			goto end;
		}
		lparam->dns_server = &lparam->ip;
	}
	else
	{
		lparam->dns_server = &vsfip.netif_default->dns[0];
	}

	err = vsfip_dnsc_setserver(0, lparam->dns_server);
	if (err < 0)
	{
		vsfshell_printf(outpt,
						"fail to set dns server: %d.%d.%d.%d"VSFSHELL_LINEEND,
							lparam->dns_server->addr.s_addr_buf[0],
							lparam->dns_server->addr.s_addr_buf[1],
							lparam->dns_server->addr.s_addr_buf[2],
							lparam->dns_server->addr.s_addr_buf[3]);
		goto end;
	}

	lparam->local_pt.sm = pt->sm;
	lparam->local_pt.state = 0;
	vsfsm_pt_entry(pt);
	err = vsfip_gethostbyname(&lparam->local_pt, evt, param->argv[1],
							&lparam->ip);
	if (err > 0) return err; else if (err < 0)
	{
		vsfshell_printf(outpt, "fail to get ip address for: %s"VSFSHELL_LINEEND,
							param->argv[1]);
		goto end;
	}

	vsfshell_printf(outpt, "%d.%d.%d.%d"VSFSHELL_LINEEND,
							lparam->ip.addr.s_addr_buf[0],
							lparam->ip.addr.s_addr_buf[1],
							lparam->ip.addr.s_addr_buf[2],
							lparam->ip.addr.s_addr_buf[3]);

end:
	vsfshell_handler_exit(param);
	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

struct vsfshell_handler_t vsf_busybox_handlers[] =
{
	// common handlers
	{"help", vsf_busybox_help, &vsf_busybox_ctx},
	// fs handlers
	{"ls", vsf_busybox_ls, &vsf_busybox_ctx},
	{"cd", vsf_busybox_cd, &vsf_busybox_ctx},
	{"pwd", vsf_busybox_pwd, &vsf_busybox_ctx},
	{"cat", vsf_busybox_cat, &vsf_busybox_ctx},
	// net handlers
	{"ipconfig", vsf_busybox_ipconfig, &vsf_busybox_ctx},
	{"arp", vsf_busybox_arp, &vsf_busybox_ctx},
	{"ping", vsf_busybox_ping, &vsf_busybox_ctx},
	{"httpd", vsf_busybox_httpd, &vsf_busybox_ctx},
	{"dns", vsf_busybox_dns, &vsf_busybox_ctx},
};

void vsf_busybox_init(struct vsfshell_t *shell, struct vsfile_t *root)
{
	vsf_busybox_ctx.root = vsf_busybox_ctx.curfile = root;
	vsfshell_register_handlers(shell,
			vsf_busybox_handlers, dimof(vsf_busybox_handlers));
}
