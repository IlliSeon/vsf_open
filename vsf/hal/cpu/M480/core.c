#include "vsf.h"
#include "core.h"

#define FMC_ISPCMD_READ_UID     			0x04

#define CORE_SYSTICK_TIMER					TIMER0

#define M480_CLK_PLLCTL_NR(x)				(((x) - 1) << 9)
#define M480_CLK_PLLCTL_NF(x)				(((x) - 2) << 0)

#define M480_CLK_PLLCTL_NO_1				(0x0UL << CLK_PLLCTL_OUTDIV_Pos)
#define M480_CLK_PLLCTL_NO_2				(0x1UL << CLK_PLLCTL_OUTDIV_Pos)
#define M480_CLK_PLLCTL_NO_4				(0x3UL << CLK_PLLCTL_OUTDIV_Pos)

#define M480_CLK_CLKSEL0_HCLKSEL_HXT		(0x00UL << CLK_CLKSEL0_HCLKSEL_Pos)
#define M480_CLK_CLKSEL0_HCLKSEL_LXT		(0x01UL << CLK_CLKSEL0_HCLKSEL_Pos)
#define M480_CLK_CLKSEL0_HCLKSEL_PLL		(0x02UL << CLK_CLKSEL0_HCLKSEL_Pos)
#define M480_CLK_CLKSEL0_HCLKSEL_LIRC		(0x03UL << CLK_CLKSEL0_HCLKSEL_Pos)
#define M480_CLK_CLKSEL0_HCLKSEL_HIRC		(0x07UL << CLK_CLKSEL0_HCLKSEL_Pos)

#define M480_CLK_CLKSEL1_TIM0SEL_HXT		(0x00UL << CLK_CLKSEL1_TMR0SEL_Pos)
#define M480_CLK_CLKSEL1_TIM0SEL_LXT		(0x01UL << CLK_CLKSEL1_TMR0SEL_Pos)
#define M480_CLK_CLKSEL1_TIM0SEL_PCLK		(0x02UL << CLK_CLKSEL1_TMR0SEL_Pos)
#define M480_CLK_CLKSEL1_TIM0SEL_EXTTRG		(0x03UL << CLK_CLKSEL1_TMR0SEL_Pos)
#define NM480_CLK_CLKSEL1_TIM0SEL_LIRC		(0x05UL << CLK_CLKSEL1_TMR0SEL_Pos)
#define M480_CLK_CLKSEL1_TIM0SEL_HIRC		(0x07UL << CLK_CLKSEL1_TMR0SEL_Pos)

#define M480_TCSR_MODE_ONESHOT				(0x00UL << TIMER_CTL_OPMODE_Pos)
#define M480_TCSR_MODE_PERIODIC				(0x01UL << TIMER_CTL_OPMODE_Pos)
#define M480_TCSR_MODE_TOGGLE				(0x02UL << TIMER_CTL_OPMODE_Pos)
#define M480_TCSR_MODE_COUNTINUOUS			(0x03UL << TIMER_CTL_OPMODE_Pos)

static struct vsfhal_info_t vsfhal_info =
{
	0, 
	CORE_VECTOR_TABLE,
	CORE_CLKEN,

	CORE_HCLKSRC,
	CORE_PCLKSRC,
	CORE_PLLSRC,

	OSC0_FREQ_HZ,
	OSC32_FREQ_HZ,
	22 * 1000 * 1000,
	10 * 1000,
	CORE_PLL_FREQ_HZ,
	CPU_FREQ_HZ,
	HCLK_FREQ_HZ,
	PCLK_FREQ_HZ,
};

void __low_level_init(void)
{
	CLK->AHBCLK |= CLK_AHBCLK_SPIMCKEN_Msk;
	SPIM->CTL1 |= 1 << 2;
}

vsf_err_t vsfhal_afio_config(struct vsfhal_afio_t *afio)
{
	if(afio->port > 7 || afio->pin > 15)
	{
		return VSFERR_IO;
	}

	if(afio->pin >= 8)
	{
		*(&(SYS -> GPA_MFPH) + (afio->port << 1)) &= ~(SYS_GPA_MFPH_PA8MFP_Msk << (afio->pin % 8 << 2));
		*(&(SYS -> GPA_MFPH) + (afio->port << 1)) |= afio->remap << (SYS_GPA_MFPH_PA8MFP_Pos + (afio->pin % 8 << 2));
	}
	else
	{
		*(&(SYS -> GPA_MFPL) + (afio->port << 1)) &= ~(SYS_GPA_MFPL_PA0MFP_Msk << (afio->pin % 8 << 2));
		*(&(SYS -> GPA_MFPL) + (afio->port << 1)) |= afio->remap << (SYS_GPA_MFPL_PA0MFP_Pos + (afio->pin % 8 << 2));
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_get_info(struct vsfhal_info_t **info)
{
	*info = &vsfhal_info;
	return VSFERR_NONE;
}

void m480_unlock_reg(void)
{
	do {
		SYS->REGLCTL = 0x59;
		SYS->REGLCTL = 0x16;
		SYS->REGLCTL = 0x88;
	} while (SYS->REGLCTL != SYS_REGLCTL_REGLCTL_Msk);
}

void m480_lock_reg(void)
{
	SYS->REGLCTL = 0;
}

vsf_err_t vsfhal_core_reset(void *p)
{
	m480_unlock_reg();
	SYS->IPRST0 |= SYS_IPRST0_CHIPRST_Msk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_init(void *p)
{
	uint32_t temp32;
	uint32_t freq_in;

	if (p != NULL)
	{
		vsfhal_info = *(struct vsfhal_info_t *)p;
	}

	m480_unlock_reg();

	// enable clocks
	CLK->PWRCTL |= CLK_PWRCTL_HXTEN_Msk;
	while ((CLK->STATUS & CLK_STATUS_HXTSTB_Msk) != CLK_STATUS_HXTSTB_Msk);

	CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;
	while ((CLK->STATUS & CLK_STATUS_HXTSTB_Msk) != CLK_STATUS_HXTSTB_Msk);
	CLK->CLKSEL0 |= CLK_CLKSEL0_HCLKSEL_Msk;
	CLK->CLKDIV0 &= (~CLK_CLKDIV0_HCLKDIV_Msk);

	temp32 = vsfhal_info.clk_enable;
	temp32 = ((temp32 & M480_CLK_HXT) ? CLK_STATUS_HXTSTB_Msk : 0) |
				((temp32 & M480_CLK_LXT) ? CLK_STATUS_LXTSTB_Msk : 0) |
				((temp32 & M480_CLK_HIRC) ? CLK_STATUS_HIRCSTB_Msk : 0) |
				((temp32 & M480_CLK_LIRC) ? CLK_STATUS_LIRCSTB_Msk : 0);
	// enable PLLs
	if (vsfhal_info.pllsrc != M480_PLLSRC_NONE)
	{
		uint8_t no;
		uint32_t no_mask;

		switch (vsfhal_info.pllsrc)
		{
		case M480_PLLSRC_HXT:
			temp32 = 0;
			freq_in = vsfhal_info.osc_freq_hz;
			break;
		case M480_PLLSRC_HIRC:
			temp32 = CLK_PLLCTL_PLLSRC_Msk;
			freq_in = vsfhal_info.hirc_freq_hz;
			break;
		default:
			return VSFERR_INVALID_PARAMETER;
		}
		// Fin/NR: 2MHz
		if ((vsfhal_info.pll_freq_hz * 1 > ( 200* 1000 * 1000)) &&
				(vsfhal_info.pll_freq_hz * 1 < (500 * 1000 * 1000)))
		{
			no = 1;
					no_mask = M480_CLK_PLLCTL_NO_1;			
		}
		else if ((vsfhal_info.pll_freq_hz * 2 > (200 * 1000 * 1000)) &&
				(vsfhal_info.pll_freq_hz * 2 < (500 * 1000 * 1000)))
		{
			no = 2;
			no_mask = M480_CLK_PLLCTL_NO_2;
		}
		else if ((vsfhal_info.pll_freq_hz * 4 > (200 * 1000 * 1000)) &&
				(vsfhal_info.pll_freq_hz * 4 < (500 * 1000 * 1000)))
		{
			no = 4;
			no_mask = M480_CLK_PLLCTL_NO_4;
		}
		else
		{
			return VSFERR_INVALID_PARAMETER;
		}
		CLK->PLLCTL |= CLK_PLLCTL_PD_Msk;
		CLK->PLLCTL = temp32 | no_mask |
		M480_CLK_PLLCTL_NR(freq_in / 6000000) |
		M480_CLK_PLLCTL_NF(vsfhal_info.pll_freq_hz * no / 12000000);
		while ((CLK->STATUS & CLK_STATUS_PLLSTB_Msk) != CLK_STATUS_PLLSTB_Msk);
	}

	if (vsfhal_info.cpu_freq_hz < 27 * 1000 * 1000)
	{
		FMC->CYCCTL = 1;
	}
	else if (vsfhal_info.cpu_freq_hz < 54 * 1000 * 1000)
	{
		FMC->CYCCTL = 2;
	}
	else if (vsfhal_info.cpu_freq_hz < 81 * 1000 * 1000)
	{
		FMC->CYCCTL = 3;
	}
	else if (vsfhal_info.cpu_freq_hz < 108 * 1000 * 1000)
	{
		FMC->CYCCTL = 4;
	}
	else if (vsfhal_info.cpu_freq_hz < 135 * 1000 * 1000)
	{
		FMC->CYCCTL = 5;
	}
	else if (vsfhal_info.cpu_freq_hz < 162 * 1000 * 1000)
	{
		FMC->CYCCTL = 6;
	}
	else if (vsfhal_info.cpu_freq_hz < 192 * 1000 * 1000)
	{
		FMC->CYCCTL = 7;
	}
	else
	{
		FMC->CYCCTL = 8;
	}

	// set hclk
	switch (vsfhal_info.hclksrc)
	{
	case M480_HCLKSRC_HIRC:
		freq_in = vsfhal_info.hirc_freq_hz;
		temp32 = M480_CLK_CLKSEL0_HCLKSEL_HIRC;
		break;
	case M480_HCLKSRC_LIRC:
		freq_in = vsfhal_info.lirc_freq_hz;
		temp32 = M480_CLK_CLKSEL0_HCLKSEL_LIRC;
		break;
	case M480_HCLKSRC_PLLFOUT:
		freq_in = vsfhal_info.pll_freq_hz;
		temp32 = M480_CLK_CLKSEL0_HCLKSEL_PLL;
		break;
	case M480_HCLKSRC_LXT:
		freq_in = vsfhal_info.osc32k_freq_hz;
		temp32 = M480_CLK_CLKSEL0_HCLKSEL_LXT;
		break;
	case M480_HCLKSRC_HXT:
		freq_in = vsfhal_info.osc_freq_hz;
		temp32 = M480_CLK_CLKSEL0_HCLKSEL_HXT;
		break;
	default:
		return VSFERR_INVALID_PARAMETER;
	}

	CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;
	while ((CLK->STATUS & CLK_STATUS_HXTSTB_Msk) != CLK_STATUS_HXTSTB_Msk);
	CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | M480_CLK_CLKSEL0_HCLKSEL_HIRC;

	CLK->CLKDIV0 = (CLK->CLKDIV0 & ~CLK_CLKDIV0_HCLKDIV_Msk) |
			((freq_in / vsfhal_info.hclk_freq_hz) - 1);
	CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_HCLKSEL_Msk) | temp32;

	SYS->USBPHY &= ~SYS_USBPHY_HSUSBROLE_Msk;
	SYS->USBPHY = (SYS->USBPHY & ~(SYS_USBPHY_HSUSBROLE_Msk | SYS_USBPHY_HSUSBACT_Msk)) | SYS_USBPHY_HSUSBEN_Msk;
	for (volatile int i = 0; i < 0x1000; i++);
	SYS->USBPHY |= SYS_USBPHY_HSUSBACT_Msk;

	m480_lock_reg();
	SCB->VTOR = vsfhal_info.vector_table;
	SCB->AIRCR = 0x05FA0000 | vsfhal_info.priority_group;
	return VSFERR_NONE;
}

uint32_t vsfhal_uid_get(uint8_t *buffer, uint32_t size)
{
	uint32_t buf[3], i;

	for (i = 0 ; i < 3; i++)
	{
		m480_unlock_reg();
		FMC->ISPCMD = FMC_ISPCMD_READ_UID;
		FMC->ISPADDR = 0x04 * i;
		FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;
		while (FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk);
		buf[i] = FMC->ISPDAT;
		m480_lock_reg();
	}

	size = min(size, 12);
	memcpy(buffer, (uint8_t *)buf, size);
	return size;
}

#include "hal/arch/arm_cm/arm_cm.h"
