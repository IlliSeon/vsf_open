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
#include "M480.h"

// CEPCTL
#define USB_CEPCTL_NAKCLR					((uint32_t)0x00000000)
#define USB_CEPCTL_STALL					((uint32_t)0x00000002)
#define USB_CEPCTL_ZEROLEN					((uint32_t)0x00000004)
#define USB_CEPCTL_FLUSH					((uint32_t)0x00000008)

// EPxCFG
#define USB_EP_CFG_VALID					((uint32_t)0x00000001)
#define USB_EP_CFG_TYPE_BULK				((uint32_t)0x00000002)
#define USB_EP_CFG_TYPE_INT					((uint32_t)0x00000004)
#define USB_EP_CFG_TYPE_ISO					((uint32_t)0x00000006)
#define USB_EP_CFG_TYPE_MASK				((uint32_t)0x00000006)
#define USB_EP_CFG_DIR_OUT					((uint32_t)0x00000000)
#define USB_EP_CFG_DIR_IN					((uint32_t)0x00000008)

// EPxRSPCTL
#define USB_EP_RSPCTL_FLUSH					((uint32_t)0x00000001)
#define USB_EP_RSPCTL_MODE_AUTO				((uint32_t)0x00000000)
#define USB_EP_RSPCTL_MODE_MANUAL			((uint32_t)0x00000002)
#define USB_EP_RSPCTL_MODE_FLY				((uint32_t)0x00000004)
#define USB_EP_RSPCTL_MODE_MASK				((uint32_t)0x00000006)
#define USB_EP_RSPCTL_TOGGLE				((uint32_t)0x00000008)
#define USB_EP_RSPCTL_HALT					((uint32_t)0x00000010)
#define USB_EP_RSPCTL_ZEROLEN				((uint32_t)0x00000020)
#define USB_EP_RSPCTL_SHORTTXEN				((uint32_t)0x00000040)
#define USB_EP_RSPCTL_DISBUF				((uint32_t)0x00000080)

#define M480_USBD_EP_REG(ep, reg)			\
	*((__IO uint32_t *)((uint32_t)&HSUSBD->reg + (uint32_t)((uint8_t)(ep) * 0x28)))
#define M480_USBD_EP_REG8(ep, reg)		\
	*((__IO uint8_t *)((uint32_t)&HSUSBD->reg + (uint32_t)((uint8_t)(ep) * 0x28)))

// for M480_lock_reg and m480_unlock_reg
#include "core.h"

#define M480_USBD_EP_NUM					(12 + 2)
const uint8_t vsfhal_usbd_ep_num = M480_USBD_EP_NUM;
struct vsfhal_usbd_callback_t vsfhal_usbd_callback;
static uint16_t EP_Cfg_Ptr;
static uint16_t max_ctl_ep_size = 64;

// true if data direction in setup packet is device to host
static volatile bool vsfhal_setup_status_IN, vsfhal_status_out = false;

// vsfhal_usbd_epaddr does not include control endpoints
#define M480_USBD_EPIN					0x10
#define M480_USBD_EPOUT					0x00
static int8_t vsfhal_usbd_epaddr[M480_USBD_EP_NUM];

vsf_err_t vsfhal_usbd_init(int32_t int_priority)
{
	CLK->AHBCLK |= CLK_AHBCLK_HSUSBDCKEN_Msk;
	HSUSBD->PHYCTL |= HSUSBD_PHYCTL_PHYEN_Msk;
	while (1)
	{
		HSUSBD->EP[0ul].EPMPS = 0x20ul;
		if (HSUSBD->EP[0ul].EPMPS == 0x20ul) 
			break;
	}

#ifdef VSFUSBD_CFG_HIGHSPEED
	HSUSBD->OPER = HSUSBD_OPER_HISPDEN_Msk;
#else
	HSUSBD->OPER = 0;
#endif

	// 8 nop for reg sync
	__asm("nop");
	__asm("nop");
	__asm("nop");
	__asm("nop");
	__asm("nop");
	__asm("nop");
	__asm("nop");

	// Enable USB interrupt
	HSUSBD->GINTEN = 0;
	// Enable BUS interrupt
	HSUSBD->BUSINTEN = HSUSBD_BUSINTEN_RSTIEN_Msk;
	HSUSBD->CEPINTEN = HSUSBD_CEPINTEN_SETUPPKIEN_Msk | HSUSBD_CEPINTEN_RXPKIEN_Msk |
			HSUSBD_CEPINTEN_TXPKIEN_Msk | HSUSBD_CEPINTEN_STSDONEIEN_Msk;
	// Enable USB interrupt
	HSUSBD->GINTEN = HSUSBD_GINTEN_USBIEN_Msk | HSUSBD_GINTEN_CEPIEN_Msk;

	if (int_priority >= 0)
	{
		NVIC_SetPriority(USBD20_IRQn, (uint32_t)int_priority);
		NVIC_EnableIRQ(USBD20_IRQn);
	}
	HSUSBD->PHYCTL |= HSUSBD_PHYCTL_DPPUEN_Msk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_fini(void)
{
	HSUSBD->PHYCTL &= ~HSUSBD_PHYCTL_PHYEN_Msk;
	CLK->AHBCLK &= ~CLK_AHBCLK_HSUSBDCKEN_Msk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_reset(void)
{
	return VSFERR_NONE;
}

void USB_Istr(void);
vsf_err_t vsfhal_usbd_poll(void)
{
	USB_Istr();
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_connect(void)
{
	HSUSBD->PHYCTL |= HSUSBD_PHYCTL_DPPUEN_Msk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_disconnect(void)
{
	HSUSBD->PHYCTL &= ~HSUSBD_PHYCTL_DPPUEN_Msk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_set_address(uint8_t address)
{
	HSUSBD->FADDR = address;
	return VSFERR_NONE;
}

uint8_t vsfhal_usbd_get_address(void)
{
	return HSUSBD->FADDR;
}

vsf_err_t vsfhal_usbd_wakeup(void)
{
	HSUSBD->OPER |= HSUSBD_OPER_RESUMEEN_Msk;
	return VSFERR_NONE;
}

uint32_t vsfhal_usbd_get_frame_number(void)
{
	return HSUSBD->FRAMECNT >> 3;
}

vsf_err_t vsfhal_usbd_get_setup(uint8_t *buffer)
{
	uint16_t temp = HSUSBD->SETUP1_0;
	buffer[0] = temp & 0xFF;
	vsfhal_setup_status_IN = (buffer[0] & 0x80) > 0;
	buffer[1] = temp >> 8;
	temp = HSUSBD->SETUP3_2;
	buffer[2] = temp & 0xFF;
	buffer[3] = temp >> 8;
	temp = HSUSBD->SETUP5_4;
	buffer[4] = temp & 0xFF;
	buffer[5] = temp >> 8;
	temp = HSUSBD->SETUP7_6;
	buffer[6] = temp & 0xFF;
	buffer[7] = temp >> 8;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_prepare_buffer(void)
{
	EP_Cfg_Ptr = 0x1000;
	memset(vsfhal_usbd_epaddr, -1, sizeof(vsfhal_usbd_epaddr));
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_ep_reset(uint8_t idx)
{
	return VSFERR_NONE;
}

static int8_t vsfhal_usbd_ep(uint8_t idx)
{
	uint8_t i;

	for (i = 0; i < sizeof(vsfhal_usbd_epaddr); i++)
	{
		if ((int8_t)idx == vsfhal_usbd_epaddr[i])
		{
			return (int8_t)i;
		}
	}
	return -1;
}

static int8_t vsfhal_usbd_get_free_ep(uint8_t idx)
{
	uint8_t i;

	for (i = 0; i < sizeof(vsfhal_usbd_epaddr); i++)
	{
		if (-1 == vsfhal_usbd_epaddr[i])
		{
			vsfhal_usbd_epaddr[i] = (int8_t)idx;
			return i;
		}
	}
	return -1;
}

static void vsfhal_usbd_set_eptype(uint8_t ep, uint32_t type)
{
	M480_USBD_EP_REG(ep, EP[0].EPRSPCTL) = USB_EP_RSPCTL_FLUSH | USB_EP_RSPCTL_MODE_MANUAL;
	M480_USBD_EP_REG(ep, EP[0].EPCFG) &= ~USB_EP_CFG_TYPE_MASK;
	M480_USBD_EP_REG(ep, EP[0].EPCFG) |= type | USB_EP_CFG_VALID;
}

vsf_err_t vsfhal_usbd_ep_set_type(uint8_t idx, enum vsfhal_usbd_eptype_t type)
{
	int8_t index_in = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	int8_t index_out = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	uint32_t eptype;

	if ((index_in < 0) && (index_out < 0))
	{
		return VSFERR_FAIL;
	}

	switch (type)
	{
	case USB_EP_TYPE_CONTROL:
		return VSFERR_NONE;
	case USB_EP_TYPE_INTERRUPT:
		eptype = USB_EP_CFG_TYPE_INT;
		break;
	case USB_EP_TYPE_BULK:
		eptype = USB_EP_CFG_TYPE_BULK;
		break;
	case USB_EP_TYPE_ISO:
		eptype = USB_EP_CFG_TYPE_ISO;
		break;
	default:
		return VSFERR_INVALID_PARAMETER;
	}
	if (index_in > 1)
	{
		index_in -= 2;
		vsfhal_usbd_set_eptype(index_in, eptype);
		HSUSBD->GINTEN |= HSUSBD_GINTEN_EPAIEN_Msk << index_in;
		M480_USBD_EP_REG(index_in, EP[0].EPINTEN) = HSUSBD_EPINTEN_TXPKIEN_Msk;
	}
	if (index_out > 1)
	{
		index_out -= 2;
		vsfhal_usbd_set_eptype(index_out, eptype);
		HSUSBD->GINTEN |= HSUSBD_GINTEN_EPAIEN_Msk << index_out;
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_ep_set_IN_dbuffer(uint8_t idx)
{
	return VSFERR_NONE;
}

bool vsfhal_usbd_ep_is_IN_dbuffer(uint8_t idx)
{
	return false;
}

vsf_err_t vsfhal_usbd_ep_switch_IN_buffer(uint8_t idx)
{
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_ep_set_IN_epsize(uint8_t idx, uint16_t epsize)
{
	int8_t index = vsfhal_usbd_get_free_ep(idx | M480_USBD_EPIN);
	if (index < 0)
		return VSFERR_FAIL;

	if (0 == idx)
	{
		if (EP_Cfg_Ptr < epsize)
			return VSFERR_NOT_ENOUGH_RESOURCES;

		EP_Cfg_Ptr -= epsize & 1 ? epsize + 1 : epsize;
		HSUSBD->CEPBUFST = EP_Cfg_Ptr;
		HSUSBD->CEPBUFEND = EP_Cfg_Ptr + epsize - 1;
		max_ctl_ep_size = epsize;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		if (EP_Cfg_Ptr < epsize)
			return VSFERR_NOT_ENOUGH_RESOURCES;

		index -= 2;
		EP_Cfg_Ptr -= epsize & 1 ? epsize + 1 : epsize;
		M480_USBD_EP_REG(index, EP[0].EPBUFST) = EP_Cfg_Ptr;
		M480_USBD_EP_REG(index, EP[0].EPBUFEND) = EP_Cfg_Ptr + epsize - 1;
		M480_USBD_EP_REG(index, EP[0].EPMPS) = epsize;
		M480_USBD_EP_REG(index, EP[0].EPCFG) |= USB_EP_CFG_DIR_IN;
		M480_USBD_EP_REG(index, EP[0].EPCFG) &= ~(0xF << 4);
		M480_USBD_EP_REG(index, EP[0].EPCFG) |= (idx & 0x0F) << 4;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

uint16_t vsfhal_usbd_ep_get_IN_epsize(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return 0;
	}

	if (0 == idx)
	{
		return max_ctl_ep_size;
	}
	else if (index > 1)
	{
		index -= 2;
		return M480_USBD_EP_REG(index, EP[0].EPMPS);
	}
	return 0;
}

vsf_err_t vsfhal_usbd_ep_set_IN_stall(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		HSUSBD->CEPCTL = 2;
		HSUSBD->CEPCTL |= USB_CEPCTL_FLUSH;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) =
			(M480_USBD_EP_REG(index, EP[0].EPRSPCTL) & 0xF7) | USB_EP_RSPCTL_HALT;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_FLUSH;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_clear_IN_stall(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		HSUSBD->CEPCTL &= ~3;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_FLUSH;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) &= ~USB_EP_RSPCTL_HALT;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

bool vsfhal_usbd_ep_is_IN_stall(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return true;
	}

	if (0 == idx)
	{
		return (HSUSBD->CEPCTL & USB_CEPCTL_STALL) > 0;
	}
	else if (index > 1)
	{
		index -= 2;
		return (M480_USBD_EP_REG(index, EP[0].EPRSPCTL) & USB_EP_RSPCTL_HALT) > 0;
	}
	return true;
}

vsf_err_t vsfhal_usbd_ep_reset_IN_toggle(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		return VSFERR_NOT_SUPPORT;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_toggle_IN_toggle(uint8_t idx)
{
	return VSFERR_NOT_SUPPORT;
}

vsf_err_t vsfhal_usbd_ep_set_IN_count(uint8_t idx, uint16_t size)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		if (!vsfhal_setup_status_IN && (0 == size))
		{
			HSUSBD->CEPCTL = USB_CEPCTL_NAKCLR;
		}
		else
		{
			HSUSBD->CEPTXCNT = size;
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPTXCNT) = size;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_write_IN_buffer(uint8_t idx, uint8_t *buffer,
		uint16_t size)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPIN);
	uint32_t i;

	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		HSUSBD->CEPCTL = USB_CEPCTL_FLUSH;
		for (i = 0; i < size; i++)
		{
			HSUSBD->CEPDAT_BYTE = buffer[i];
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		for (i = 0; i < size; i++)
		{
			M480_USBD_EP_REG8(index, EP[0].EPDAT_BYTE) = buffer[i];
		}
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_set_OUT_dbuffer(uint8_t idx)
{
	return VSFERR_NONE;
}

bool vsfhal_usbd_ep_is_OUT_dbuffer(uint8_t idx)
{
	return false;
}

vsf_err_t vsfhal_usbd_ep_switch_OUT_buffer(uint8_t idx)
{
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usbd_ep_set_OUT_epsize(uint8_t idx, uint16_t epsize)
{
	int8_t index = vsfhal_usbd_get_free_ep(idx | M480_USBD_EPOUT);
	if (index < 0)
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		// has already been(will be) allocated in set_IN_epsize
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		if (EP_Cfg_Ptr < epsize)
			return VSFERR_NOT_ENOUGH_RESOURCES;

		index -= 2;
		EP_Cfg_Ptr -= epsize & 1 ? epsize + 1 : epsize;
		M480_USBD_EP_REG(index, EP[0].EPBUFST) = EP_Cfg_Ptr;
		M480_USBD_EP_REG(index, EP[0].EPBUFEND) = EP_Cfg_Ptr + epsize - 1;
		M480_USBD_EP_REG(index, EP[0].EPMPS) = epsize;
		M480_USBD_EP_REG(index, EP[0].EPCFG) &= ~USB_EP_CFG_DIR_IN;
		M480_USBD_EP_REG(index, EP[0].EPCFG) &= ~(0xF << 4);
		M480_USBD_EP_REG(index, EP[0].EPCFG) |= (idx & 0x0F) << 4;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

uint16_t vsfhal_usbd_ep_get_OUT_epsize(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return 0;
	}

	if (0 == idx)
	{
		return max_ctl_ep_size;
	}
	else if (index > 1)
	{
		index -= 2;
		return M480_USBD_EP_REG(index, EP[0].EPMPS);
	}
	return 0;
}

vsf_err_t vsfhal_usbd_ep_set_OUT_stall(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		HSUSBD->CEPCTL = 2;
		HSUSBD->CEPCTL |= USB_CEPCTL_FLUSH;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) =
			(M480_USBD_EP_REG(index, EP[0].EPRSPCTL) & 0xF7) | USB_EP_RSPCTL_HALT;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_FLUSH;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_clear_OUT_stall(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		HSUSBD->CEPCTL = HSUSBD->CEPCTL & ~3;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_FLUSH;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) &= ~USB_EP_RSPCTL_HALT;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

bool vsfhal_usbd_ep_is_OUT_stall(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return true;
	}

	if (0 == idx)
	{
		return (HSUSBD->CEPCTL & USB_CEPCTL_STALL) > 0;
	}
	else if (index > 1)
	{
		index -= 2;
		return (M480_USBD_EP_REG(index, EP[0].EPRSPCTL) & USB_EP_RSPCTL_HALT) > 0;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_reset_OUT_toggle(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		return VSFERR_NOT_SUPPORT;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPRSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_toggle_OUT_toggle(uint8_t idx)
{
	return VSFERR_NOT_SUPPORT;
}

uint16_t vsfhal_usbd_ep_get_OUT_count(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return 0;
	}

	if (0 == idx)
	{
		// some ugly fix because M480 not have IN0/OUT0 for status stage
		if (vsfhal_status_out)
		{
			vsfhal_status_out = false;
			return 0;
		}
		return HSUSBD->CEPRXCNT;
	}
	else if (index > 1)
	{
		index -= 2;
		return M480_USBD_EP_REG(index, EP[0].EPDATCNT) & 0xFFFF;
	}
	return 0;
}

vsf_err_t vsfhal_usbd_ep_read_OUT_buffer(uint8_t idx, uint8_t *buffer,
											uint16_t size)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	uint32_t i;

	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		if (!vsfhal_setup_status_IN)
		{
			for (i = 0; i < size; i++)
			{
				buffer[i] = HSUSBD->CEPDAT_BYTE;
			}
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		size = min(size, M480_USBD_EP_REG(index, EP[0].EPMPS));
		for (i = 0; i < size; i++)
		{
			buffer[i] = M480_USBD_EP_REG8(index, EP[0].EPDAT_BYTE);
		}
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t vsfhal_usbd_ep_enable_OUT(uint8_t idx)
{
	int8_t index = vsfhal_usbd_ep(idx | M480_USBD_EPOUT);
	if ((index < 0) || (index >= M480_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}

	if (0 == idx)
	{
		if (vsfhal_setup_status_IN)
		{
			HSUSBD->CEPCTL = USB_CEPCTL_NAKCLR;
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		M480_USBD_EP_REG(index, EP[0].EPINTEN) |= HSUSBD_EPINTEN_RXPKIEN_Msk;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

static void vsfhal_usbd_cb(enum vsfhal_usbd_evt_t evt, uint32_t value)
{
	if (vsfhal_usbd_callback.on_event != NULL)
		vsfhal_usbd_callback.on_event(vsfhal_usbd_callback.param, evt, value);
}

void USB_Istr(void)
{
	uint32_t IrqStL, IrqSt;

	IrqStL = HSUSBD->GINTSTS;
	IrqStL &= HSUSBD->GINTEN;

	if (!IrqStL)
	{
		return;
	}

	// USB interrupt
	if (IrqStL & HSUSBD_GINTSTS_USBIF_Msk) {
		IrqSt = HSUSBD->BUSINTSTS;
		IrqSt &= HSUSBD->BUSINTEN;

		if (IrqSt & HSUSBD_BUSINTSTS_SOFIF_Msk) {
			vsfhal_usbd_cb(VSFHAL_USBD_ON_SOF, 0);
		}
		if (IrqSt & HSUSBD_BUSINTSTS_RSTIF_Msk) {
			vsfhal_status_out = false;
			vsfhal_usbd_cb(VSFHAL_USBD_ON_RESET, 0);
			HSUSBD->BUSINTSTS = HSUSBD_BUSINTSTS_RSTIF_Msk;
			HSUSBD->CEPINTSTS = 0x1ffc;
		}
		if (IrqSt & HSUSBD_BUSINTSTS_RESUMEIF_Msk) {
			vsfhal_usbd_cb(VSFHAL_USBD_ON_RESUME, 0);
		}
		if (IrqSt & HSUSBD_BUSINTSTS_SUSPENDIF_Msk) {
			vsfhal_usbd_cb(VSFHAL_USBD_ON_SUSPEND, 0);
		}
	}

	// CEP interrupt
	if (IrqStL & HSUSBD_GINTSTS_CEPIF_Msk) {
		IrqSt = HSUSBD->CEPINTSTS;
		IrqSt &= HSUSBD->CEPINTEN;

		// IMPORTANT:
		// 		the OUT ep of M480 has no flow control, so the order of
		// 		checking the interrupt flash MUST be as follow:
		// 		IN0 -->> STATUS -->> SETUP -->> OUT0
		// consider this:
		// 		SETUP -->> IN0 -->> STATUS -->> SETUP -->> OUT0 -->> STATUS
		// 		           ------------------------------------
		//		in some condition, the under line interrupt MAYBE in one routine
		if (IrqSt & HSUSBD_CEPINTSTS_TXPKIF_Msk) {
			HSUSBD->CEPINTSTS = HSUSBD_CEPINTSTS_TXPKIF_Msk;
			vsfhal_usbd_cb(VSFHAL_USBD_ON_IN, 0);
		}

		if (IrqSt & HSUSBD_CEPINTSTS_STSDONEIF_Msk) {
			HSUSBD->CEPINTSTS = HSUSBD_CEPINTSTS_STSDONEIF_Msk;

			if (!vsfhal_setup_status_IN)
			{
				vsfhal_usbd_cb(VSFHAL_USBD_ON_IN, 0);
			}
			else
			{
				vsfhal_status_out = true;
				vsfhal_usbd_cb(VSFHAL_USBD_ON_OUT, 0);
			}
		}

		if (IrqSt & HSUSBD_CEPINTSTS_SETUPPKIF_Msk) {
			HSUSBD->CEPINTSTS = HSUSBD_CEPINTSTS_SETUPPKIF_Msk;
			vsfhal_usbd_cb(VSFHAL_USBD_ON_SETUP, 0);
		}

		if (IrqSt & HSUSBD_CEPINTSTS_RXPKIF_Msk) {
			HSUSBD->CEPINTSTS = HSUSBD_CEPINTSTS_RXPKIF_Msk;
			vsfhal_usbd_cb(VSFHAL_USBD_ON_OUT, 0);
		}
	}

	// EP interrupt
	if (IrqStL & (~3))
	{
		int i;
		uint8_t ep;

		for (i = 0; i < M480_USBD_EP_NUM - 2; i++)
		{
			ep = vsfhal_usbd_epaddr[i + 2] & 0x0F;
			if (IrqStL & (1 << (i + 2)))
			{
				IrqSt = M480_USBD_EP_REG(i, EP[0].EPINTSTS);
				IrqSt &= M480_USBD_EP_REG(i, EP[0].EPINTEN);

				if (IrqSt & HSUSBD_EPINTSTS_TXPKIF_Msk)
				{
					M480_USBD_EP_REG(i, EP[0].EPINTSTS) = HSUSBD_EPINTSTS_TXPKIF_Msk;
					vsfhal_usbd_cb(VSFHAL_USBD_ON_IN, ep);
				}
				if (IrqSt & HSUSBD_EPINTSTS_RXPKIF_Msk)
				{
					M480_USBD_EP_REG(i, EP[0].EPINTEN) &= ~HSUSBD_EPINTEN_RXPKIEN_Msk;
					M480_USBD_EP_REG(i, EP[0].EPINTSTS) = HSUSBD_EPINTSTS_RXPKIF_Msk;

					vsfhal_usbd_cb(VSFHAL_USBD_ON_OUT, ep);
				}
			}
		}
	}
}


ROOTFUNC void USBD20_IRQHandler(void)
{
	USB_Istr();
}
