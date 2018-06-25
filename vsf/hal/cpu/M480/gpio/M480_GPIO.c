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

#define M480_GPIO_NUM					16

vsf_err_t vsfhal_gpio_init(uint8_t index)
{
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_fini(uint8_t index)
{
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_config(uint8_t index, uint8_t pin_idx, uint32_t mode)
{
	GPIO_T *gpio;
	uint8_t mode_reg = mode & 0x03, pusel_reg = mode >> 4;

#if __VSF_DEBUG__
	if ((index >= M480_GPIO_NUM) || (pin_idx >= 32))
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)index << 6));
	gpio->MODE = (gpio->MODE & ~(((uint32_t)0x03) << (pin_idx << 1))) |
						mode_reg << (pin_idx << 1);
	gpio->PUSEL = (gpio->PUSEL & ~(((uint32_t)0x03) << (pin_idx << 1))) |
						pusel_reg << (pin_idx << 1);
	gpio->DINOFF &= ~(0x100UL << pin_idx);
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_set(uint8_t index, uint32_t pin_mask)
{
	GPIO_T *gpio;
	
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)index << 6));
	gpio->DATMSK = ~pin_mask;
	gpio->DOUT = 0xFFFF;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_clear(uint8_t index, uint32_t pin_mask)
{
	GPIO_T *gpio;
	
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)index << 6));
	gpio->DATMSK = ~pin_mask;
	gpio->DOUT = 0;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_out(uint8_t index, uint32_t pin_mask, uint32_t value)
{
	GPIO_T *gpio;
	
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)index << 6));
	gpio->DATMSK = ~pin_mask;
	gpio->DOUT = value;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_in(uint8_t index, uint32_t pin_mask, uint32_t *value)
{
	GPIO_T *gpio;
	
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return VSFERR_NOT_SUPPORT;
	}
#endif
	
	gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)index << 6));
	*value = gpio->PIN & pin_mask;
	return VSFERR_NONE;
}

uint32_t vsfhal_gpio_get(uint8_t index, uint32_t pin_mask)
{
	GPIO_T *gpio;
	
#if __VSF_DEBUG__
	if (index >= M480_GPIO_NUM)
	{
		return 0;
	}
#endif
	
	gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)index << 6));
	return gpio->PIN & pin_mask;
}
