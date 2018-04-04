#include "vsf.h"

#include "core.h"

static struct vsfhal_info_t vsfhal_info =
{
	0, CORE_VECTOR_TABLE, 
	CORE_CLKEN, CORE_CLKSRC, GD32F1X0_PLLSRC_HSE,
	HSE_FREQ_HZ, HSI_FREQ_HZ, CORE_PLL_FREQ_HZ,
	CORE_MCLK_FREQ_HZ, CORE_APB_FREQ_HZ, CORE_MCLK_FREQ_HZ, CORE_MCLK_FREQ_HZ,
};

vsf_err_t vsfhal_core_get_info(struct vsfhal_info_t **info)
{
	*info = &vsfhal_info;
	return VSFERR_NONE;
}

// Pendsv
struct vsfhal_pendsv_t
{
	void (*on_pendsv)(void *);
	void *param;
} static vsfhal_pendsv;

ROOTFUNC void PendSV_Handler(void)
{
	if (vsfhal_pendsv.on_pendsv != NULL)
	{
		vsfhal_pendsv.on_pendsv(vsfhal_pendsv.param);
	}
}

vsf_err_t vsfhal_core_pendsv_config(void (*on_pendsv)(void *), void *param)
{
	vsfhal_pendsv.on_pendsv = on_pendsv;
	vsfhal_pendsv.param = param;

	if (vsfhal_pendsv.on_pendsv != NULL)
	{
		SCB->SHP[10] = 0xFF;
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_pendsv_trigger(void)
{
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	return VSFERR_NONE;
}

ROOTFUNC void HardFault_Handler(void)
{
	while (1);
}

uint32_t vsfhal_core_get_stack(void)
{
	return __get_MSP();
}

vsf_err_t vsfhal_core_set_stack(uint32_t sp)
{
	__set_MSP(sp);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_fini(void *p)
{
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_reset(void *p)
{
	NVIC_SystemReset();
	return VSFERR_NONE;
}

uint8_t vsfhal_core_set_intlevel(uint8_t level)
{
	uint8_t origlevel = __get_BASEPRI();
	__set_BASEPRI(level);
	return origlevel;
}

// sleep will enable interrupt
// for cortex processor, if an interrupt occur between enable the interrupt
// 		and __WFI, wfi will not make the core sleep
void vsfhal_core_sleep(uint32_t mode)
{
	vsf_leave_critical();
	__WFI();
}

static uint32_t __log2__(uint32_t n)
{
	uint32_t i, value = 1;
	
	for (i = 0; i < 31; i++)
	{
		if (value == n)
		{
			return i;
		}
		value <<= 1;
	}
	return 0;
}

#define RCC_GCFGR_FREQ_DIVIDE_RESET		(uint32_t)0x083F000C	/* Reset SCS[1:0], AHBPS[3:0], APB1PS[2:0],APB2PS[2:0],
																	ADCPS[1:0],USBPS[1:0],CKOTUSRC[2:0], CKOUTDIV, and PLLDV bits*/
#define RCC_GCCR_CLK_ENABLE_RESET		(uint32_t)0xFEF6FFFF	/* Reset HSEEN, CKMEN and PLLEN bits */
#define RCC_GCCR_HSEBPS_RESET			(uint32_t)0xFFFBFFFF	/* Reset HSEBPS bit */
#define RCC_GCFGR_CLK_SELECT_RESET		(uint32_t)0xF7C0FFFF	/* Reset PLLSEL, PLLPREDV and PLLMF[3:0] bits */
#define RCC_GCFGR2_HSEPREDV1_RESET		(uint32_t)0xFFFFFFF0	/* Reset HSEPREDV1[3:0] bits */
#define RCC_GCFGR3_RESET				(uint32_t)0xFFFFFEBC	/* Reset USARTSEL[1:0], I2CSEL, CECSEL and ADCSEL bits */
#define RCC_GCCR2_HSI14_RESET			(uint32_t)0xFFFFFFFE	/* Reset HSI14 bit */
#define RCC_GCIR_DISABLE				(uint32_t)0x00000000	/* Disable all interrupts */
#define RCC_GCFGR_PLLMF3_0				(uint32_t)0x003C0000	/* PLLMF[3:0] Bits */
vsf_err_t vsfhal_core_init(void *p)
{
	uint32_t tmp32;
	
	if (p != NULL)
	{
		vsfhal_info = *(struct vsfhal_info_t *)p;
	}

	// move from System_Init
	RCC->GCCR |= RCC_GCCR_HSIEN;
	RCC->GCFGR &= RCC_GCFGR_FREQ_DIVIDE_RESET;
	RCC->GCCR &= RCC_GCCR_CLK_ENABLE_RESET;
	RCC->GCCR &= RCC_GCCR_HSEBPS_RESET;
	RCC->GCFGR &= RCC_GCFGR_CLK_SELECT_RESET;
	RCC->GCFGR2 &= RCC_GCFGR2_HSEPREDV1_RESET;
	RCC->GCFGR3 &= RCC_GCFGR3_RESET;
	RCC->GCCR2 &= RCC_GCCR2_HSI14_RESET;
	RCC->GCIR = RCC_GCIR_DISABLE;
	
	if (vsfhal_info.clk_enable & GD32F1X0_CLK_HSI)
	{
		RCC->GCCR |= RCC_GCCR_HSIEN;
		while (!(RCC->GCCR & RCC_GCCR_HSISTB));
		RCC->GCFGR &= ~RCC_GCFGR_SCS;
		while (RCC->GCFGR & RCC_GCFGR_SCSS);
	}
	
	if (vsfhal_info.clk_enable & GD32F1X0_CLK_HSE)
	{
		RCC->GCCR |= RCC_GCCR_HSEEN;
		while (!(RCC->GCCR & RCC_GCCR_HSESTB));
		RCC->GCFGR = (RCC->GCFGR & ~RCC_GCFGR_SCS) | RCC_GCFGR_SCS_HSE;
		while ((RCC->GCFGR & RCC_GCFGR_SCSS) != RCC_GCFGR_SCSS_HSE);
	}
	
	RCC->GCCR &= ~RCC_GCCR_PLLEN;
	RCC->GCFGR &= ~(RCC_GCFGR_AHBPS | RCC_GCFGR_APB1PS | RCC_GCFGR_APB2PS);
	
	if (vsfhal_info.clk_enable & GD32F1X0_CLK_PLL)
	{
		if (vsfhal_info.pllsrc == GD32F1X0_PLLSRC_HSID2)
		{
			RCC->GCFGR &= ~RCC_GCFGR_PLLSEL;
			tmp32 = vsfhal_info.hsi_freq_hz / 2;
		}
		else if (vsfhal_info.pllsrc == GD32F1X0_PLLSRC_HSE)
		{
			RCC->GCFGR |= RCC_GCFGR_PLLSEL_HSEPREDIV;
			RCC->GCFGR &= ~RCC_GCFGR_PLLPREDV;
			tmp32 = vsfhal_info.hse_freq_hz;
		}
		else if (vsfhal_info.pllsrc == GD32F1X0_PLLSRC_HSED2)
		{
			RCC->GCFGR |= RCC_GCFGR_PLLSEL_HSEPREDIV;
			RCC->GCFGR |= RCC_GCFGR_PLLPREDV_PREDIV1_DIV2;
			tmp32 = vsfhal_info.hse_freq_hz / 2;
		}
		else
			return VSFERR_FAIL;
		
		RCC->GCFGR &= ~(RCC_GCFGR_PLLMF | RCC_GCFGR_USBPS);
		tmp32 = vsfhal_info.pll_freq_hz / tmp32;
		tmp32 -= 2;
		if (tmp32 & 0xf)
			RCC->GCFGR |= (tmp32 & 0xf) << 18;
		if (tmp32 & 0x10)
			RCC->GCFGR |= RCC_GCFGR_PLLMF_4;
		
		RCC->GCCR |= RCC_GCCR_PLLEN;
		while (!(RCC->GCCR & RCC_GCCR_PLLSTB));
		
		if (vsfhal_info.pll_freq_hz == 48000000)
			RCC->GCFGR |= RCC_GCFGR_USBPS_Div1;
		else if (vsfhal_info.pll_freq_hz == 72000000)
			RCC->GCFGR |= RCC_GCFGR_USBPS_Div1_5;
		else if (vsfhal_info.pll_freq_hz == 96000000)
			RCC->GCFGR |= RCC_GCFGR_USBPS_Div2;
		else if (vsfhal_info.pll_freq_hz == 120000000)
			RCC->GCFGR |= RCC_GCFGR_USBPS_Div2_5;
	}

	if (vsfhal_info.clksrc == GD32F1X0_CLKSRC_PLL)
	{
		tmp32 = vsfhal_info.pll_freq_hz / vsfhal_info.ahb_freq_hz;

		RCC->GCFGR &= ~RCC_GCFGR_AHBPS;
		tmp32 = __log2__(tmp32);
		if (tmp32 > 0)
			RCC->GCFGR |= ((0x08 | (tmp32 - 1)) << 4);
		tmp32 = RCC_GCFGR_SCS_PLL;
	}
	else
		return VSFERR_FAIL;
	RCC->GCFGR = (RCC->GCFGR & ~RCC_GCFGR_SCS) | tmp32;
	while ((RCC->GCFGR & RCC_GCFGR_SCSS) != (tmp32 << 2));

	RCC->APB2CCR |= RCC_APB2CCR_CFGEN;

	SCB->VTOR = vsfhal_info.vector_table;
	SCB->AIRCR = 0x05FA0000 | vsfhal_info.priority_group;
	return VSFERR_NONE;
}

uint32_t vsfhal_uid_get(uint8_t *buffer, uint32_t size)
{
	if (NULL == buffer)
	{
		return 0;
	}
	
	if (size > 12)
	{
		size = 12;
	}
	
	memcpy(buffer, (uint8_t *)0x1ffff7ac, size);
	return size;
}

static void (*tickclk_callback)(void *param) = NULL;
static void *tickclk_param = NULL;
static volatile uint32_t tickcnt = 0;

static uint32_t tickclk_get_ms_local(void)
{
	return tickcnt;
}

uint32_t vsfhal_tickclk_get_ms(void)
{
	uint32_t count1, count2;

	do {
		count1 = tickclk_get_ms_local();
		count2 = tickclk_get_ms_local();
	} while (count1 != count2);
	return count1;
}

uint16_t vsfhal_tickclk_get_us(void)
{
	uint32_t temp = SysTick->VAL * 1000;
	return temp / SysTick->LOAD;
}

ROOTFUNC void SysTick_Handler(void)
{
	tickcnt++;
	if (tickclk_callback != NULL)
	{
		tickclk_callback(tickclk_param);
	}
}

vsf_err_t vsfhal_tickclk_config_cb(void (*callback)(void*), void *param)
{
	tickclk_callback = callback;
	tickclk_param = param;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_start(void)
{
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_stop(void)
{
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	return VSFERR_NONE;
}

void vsfhal_tickclk_poll(void)
{
	if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
		SysTick_Handler();
}

vsf_err_t vsfhal_tickclk_init(int32_t int_priority)
{
	SysTick->LOAD = vsfhal_info.cpu_freq_hz / 1000;
	if (int_priority >= 0)
	{
		SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk;
		NVIC_SetPriority(SysTick_IRQn, (uint32_t)int_priority);
	}
	else
	{
		SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk;
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_fini(void)
{
	return vsfhal_tickclk_stop();
}

// gd32f1x0 private APIs
// AFIO
vsf_err_t gd32f1x0_afio_config(const struct gd32f1x0_afio_pin_t *pin, uint32_t mode)
{
	GPIO_TypeDef *GPIOx = (GPIO_TypeDef *)(GPIOA_BASE + (pin->port << 10));
	RCC->AHBCCR |= RCC_AHBCCR_PAEN << pin->port;

	if (pin->af >= 0)
	{
		GPIOx->AFS[pin->pin >> 3] &= ~((uint32_t)0xF << ((pin->pin & 7) << 2));
		GPIOx->AFS[pin->pin >> 3] |= (uint32_t)pin->af << ((pin->pin & 7) << 2);
	}
	vsfhal_gpio_config(pin->port, pin->pin, mode);
	return VSFERR_NONE;
}

// DMA
struct gd32f1x0_dma_t
{
	void *param;
	void (*oncomplete)(void *);
} static gd32f1x0_dma[7];
DMA_Channel_TypeDef* gd32f1x0_dma_reg(uint8_t index)
{
	return (DMA_Channel_TypeDef *)(DMA1_CHANNEL1_BASE + index * 0x14);
}
static int8_t gd32f1x0_dma_irqn[7] =
{
	DMA1_Channel1_IRQn,
	DMA1_Channel2_3_IRQn, DMA1_Channel2_3_IRQn,
	DMA1_Channel4_5_IRQn, DMA1_Channel4_5_IRQn,
	DMA1_Channel6_7_IRQn, DMA1_Channel6_7_IRQn,
};

void gd32f1x0_dma_init(void)
{
	RCC->AHBCCR |= RCC_AHBCCR_DMA1EN;
}

int8_t gd32f1x0_dma_get(uint8_t index, void *param, void (*oncomplete)(void *))
{
	DMA_Channel_TypeDef *dma = gd32f1x0_dma_reg(index);
	gd32f1x0_dma[index].param = param;
	gd32f1x0_dma[index].oncomplete = oncomplete;
	return gd32f1x0_dma_irqn[index];
}

void gd32f1x0_dma_irq(uint8_t index)
{
	DMA1->ICR &= ~(0x0F << (index << 2));
	if (gd32f1x0_dma[index].oncomplete != NULL)
		gd32f1x0_dma[index].oncomplete(gd32f1x0_dma[index].param);
}

ROOTFUNC void DMA1_Channel1_IRQHandler(void)
{
	gd32f1x0_dma_irq(0);
}

ROOTFUNC void DMA1_Channel2_3_IRQHandler(void)
{
	if (DMA1->IFR & (0x02 << (1 << 2)))
		gd32f1x0_dma_irq(1);
	if (DMA1->IFR & (0x02 << (2 << 2)))
		gd32f1x0_dma_irq(2);
}

ROOTFUNC void DMA1_Channel4_5_IRQHandler(void)
{
	if (DMA1->IFR & (0x02 << (3 << 2)))
		gd32f1x0_dma_irq(3);
	if (DMA1->IFR & (0x02 << (4 << 2)))
		gd32f1x0_dma_irq(4);
}

ROOTFUNC void DMA1_Channel6_7_IRQHandler(void)
{
	if (DMA1->IFR & (0x02 << (5 << 2)))
		gd32f1x0_dma_irq(5);
	if (DMA1->IFR & (0x02 << (6 << 2)))
		gd32f1x0_dma_irq(6);
}
