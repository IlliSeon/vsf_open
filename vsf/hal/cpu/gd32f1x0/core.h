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

#ifndef __GD32F1X0_CORE_H_INCLUDED__
#define __GD32F1X0_CORE_H_INCLUDED__

#define GD32F1X0_CLK_HSI				(1UL << 0)
#define GD32F1X0_CLK_HSE				(1UL << 1)
#define GD32F1X0_CLK_PLL				(1UL << 2)

enum vsfhal_clksrc_t
{
	GD32F1X0_CLKSRC_HSI = 0,
	GD32F1X0_CLKSRC_HSE = 1,
	GD32F1X0_CLKSRC_PLL = 2
};
enum vsfhal_pllsrc_t
{
	GD32F1X0_PLLSRC_HSID2,
	GD32F1X0_PLLSRC_HSE,
	GD32F1X0_PLLSRC_HSED2,
};
struct vsfhal_info_t
{
	uint8_t priority_group;
	uint32_t vector_table;
	
	uint32_t clk_enable;
	
	enum vsfhal_clksrc_t clksrc;
	enum vsfhal_pllsrc_t pllsrc;
	
	uint32_t hse_freq_hz;
	uint32_t hsi_freq_hz;
	uint32_t pll_freq_hz;
	uint32_t ahb_freq_hz;
	uint32_t apb1_freq_hz;
	uint32_t apb2_freq_hz;
	
	uint32_t cpu_freq_hz;
};

vsf_err_t vsfhal_core_get_info(struct vsfhal_info_t **info);

// gd32f1x0 private APIs
struct gd32f1x0_afio_pin_t
{
	uint8_t port;
	uint8_t pin;
	int8_t af;
};
vsf_err_t gd32f1x0_afio_config(const struct gd32f1x0_afio_pin_t *pin, uint32_t mode);

void gd32f1x0_dma_init(void);
int8_t gd32f1x0_dma_get(uint8_t index, void *param, void (*oncomplete)(void *));
DMA_Channel_TypeDef* gd32f1x0_dma_reg(uint8_t index);

#endif	// __GD32F1X0_CORE_H_INCLUDED__
