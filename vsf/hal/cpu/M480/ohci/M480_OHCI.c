/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       GPIO.h                                                    *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    GPIO interface header file                                *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/
#include "vsf.h"
#include "core.h"

#define SYS_GPC_MFPH_PC14MFP_USB_VBUS_ST	(0x0EUL<<SYS_GPC_MFPH_PC14MFP_Pos) /*!< Power supply from USB Full speed host or HUB. \hideinitializer */
#define SYS_GPB_MFPH_PB15MFP_USB_VBUS_EN	(0x0EUL<<SYS_GPB_MFPH_PB15MFP_Pos) /*!< Power supply from USB Full speed host or HUB. \hideinitializer */
#define SYS_GPA_MFPH_PA12MFP_USB_VBUS		(0x0EUL<<SYS_GPA_MFPH_PA12MFP_Pos) /*!< Power supply from USB Full speed host or HUB. \hideinitializer */
#define SYS_GPA_MFPH_PA13MFP_USB_D_N		(0x0EUL<<SYS_GPA_MFPH_PA13MFP_Pos) /*!< USB Full speed differential signal D-. \hideinitializer */
#define SYS_GPA_MFPH_PA14MFP_USB_D_P		(0x0EUL<<SYS_GPA_MFPH_PA14MFP_Pos) /*!< USB Full speed differential signal D+. \hideinitializer */
#define SYS_GPA_MFPH_PA15MFP_USB_OTG_ID		(0x0EUL<<SYS_GPA_MFPH_PA15MFP_Pos) /*!< USB Full speed identification. \hideinitializer */

struct vsfhal_ohci_irq_t
{
	void *param;
	void (*irq)(void*);
} static vsfhal_ohci_irq;

ROOTFUNC void OHCI_IRQHandler(void)
{
 
	if (vsfhal_ohci_irq.irq != NULL)
	{
		vsfhal_ohci_irq.irq(vsfhal_ohci_irq.param);
	}
}

vsf_err_t vsfhal_hcd_init(uint32_t index, int32_t int_priority, void (*ohci_irq)(void *), void *param)
{
	uint32_t i;
	if (ohci_irq != NULL)
	{
		vsfhal_ohci_irq.irq = ohci_irq;
		vsfhal_ohci_irq.param = param;
	}
	if (int_priority >= 0)
	{
		NVIC_SetPriority(USBH_IRQn, (uint32_t)int_priority);
		NVIC_EnableIRQ(USBH_IRQn);
	}

	m480_unlock_reg();

	CLK->AHBCLK |= CLK_AHBCLK_USBHCKEN_Msk;
	CLK->CLKDIV0 = (CLK->CLKDIV0 & ~CLK_CLKDIV0_USBDIV_Msk) | ((CORE_PLL_FREQ_HZ/(1000*1000)/48-1) << CLK_CLKDIV0_USBDIV_Pos);
	CLK->APBCLK0 |= CLK_APBCLK0_USBDCKEN_Msk | CLK_APBCLK0_OTGCKEN_Msk;

	SYS->GPB_MFPH = (SYS->GPB_MFPH & ~SYS_GPB_MFPH_PB15MFP_Msk) | SYS_GPB_MFPH_PB15MFP_USB_VBUS_EN;
	SYS->GPC_MFPH = (SYS->GPC_MFPH & ~SYS_GPC_MFPH_PC14MFP_Msk) | SYS_GPC_MFPH_PC14MFP_USB_VBUS_ST;

	SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA12MFP_Msk | SYS_GPA_MFPH_PA13MFP_Msk | SYS_GPA_MFPH_PA14MFP_Msk | SYS_GPA_MFPH_PA15MFP_Msk);
	SYS->GPA_MFPH |= SYS_GPA_MFPH_PA12MFP_USB_VBUS | SYS_GPA_MFPH_PA13MFP_USB_D_N | SYS_GPA_MFPH_PA14MFP_USB_D_P | SYS_GPA_MFPH_PA15MFP_USB_OTG_ID;

	SYS->USBPHY = (SYS->USBPHY & 0xffff0000) | SYS_USBPHY_USBEN_Msk | SYS_USBPHY_SBO_Msk | (0x1 << SYS_USBPHY_USBROLE_Pos);
	for (i=0; i<0x2000; i++); 
	SYS->USBPHY |= SYS_USBPHY_HSUSBACT_Msk;

	m480_lock_reg();

	return VSFERR_NONE;
}

vsf_err_t vsfhal_hcd_fini(uint32_t index)
{
	switch (index)
	{
	case 0:
		return VSFERR_NONE;
	default:
		return VSFERR_NOT_SUPPORT;
	}
}

void* vsfhal_hcd_regbase(uint32_t index)
{
	switch (index)
	{
	case 0:
		return (void*)USBH;
	default:
		return NULL;
	}
}
