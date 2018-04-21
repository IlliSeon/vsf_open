#include "vsf.h"
#include "core.h"

#define M480_SDIO_NUM			2

#define M480_REGCFG_SDIO(sdio_idx, SDREG, DIVREG, SELREG, DIVPOS, SELPOS, AHBMSK, IRQN)	\
	[sdio_idx] = {.sdreg = (SDREG), .divreg = (DIVREG), .selreg = (SELREG), .divpos = (DIVPOS), .selpos = (SELPOS), .ahbmsk = (AHBMSK), .irqn = (IRQN)}
struct M480_sdio_t
{
	SDH_T *sdreg;
	volatile uint32_t *divreg;
	volatile uint32_t *selreg;
	uint32_t ahbmsk;
	uint8_t divpos;
	uint8_t selpos;
	uint8_t irqn;
} static const M480_sdio[M480_SDIO_NUM] =
{
	M480_REGCFG_SDIO(0, SDH0, &CLK->CLKDIV0, &CLK->CLKSEL0, CLK_CLKDIV0_SDH0DIV_Pos, CLK_CLKSEL0_SDH0SEL_Pos, CLK_AHBCLK_SDH0CKEN_Pos, SDH0_IRQn),
	M480_REGCFG_SDIO(1, SDH1, &CLK->CLKDIV3, &CLK->CLKSEL0, CLK_CLKDIV3_SDH1DIV_Pos, CLK_CLKSEL0_SDH1SEL_Pos, CLK_AHBCLK_SDH1CKEN_Pos, SDH1_IRQn),
};

struct vsfhal_sdio_param_t
{
	void (*callback)(void *);
	void *param;
	void (*d1_callback)(void *);
	void *d1_param;

	struct vsfhal_sdio_trans_t *trans;
	volatile uint8_t busy;
} static M480_sdio_param[M480_SDIO_NUM];

// 		index
//		bit0 - bit7		: sdio index		8bit
//		bit(8 + 8 * n) - bit(7 + 9 * n)
//						: sdio pin(lower 4-bit is port, higher 4-bit is pin)

#define M480_SDIO_PINNUM					7
#define M480_SDIO_IDX						8
#define M480_SDIO_IO						8
#define M480_SDIO_IO_PORT					4
#define M480_SDIO_IO_PIN					4

#define M480_SDIO_IDX_MASK					((1 << M480_SDIO_IDX) - 1)
#define M480_SDIO_IO_MASK					((1 << M480_SDIO_IO) - 1)
#define M480_SDIO_IO_PORT_OFFSET			(0)
#define M480_SDIO_IO_PORT_MASK				(((1 << M480_SDIO_IO_PORT) - 1) << M480_SDIO_IO_PORT_OFFSET)
#define M480_SDIO_IO_PIN_OFFSET				(M480_SDIO_IO_PORT_OFFSET + M480_SDIO_IO_PORT)
#define M480_SDIO_IO_PIN_MASK				(((1 << M480_SDIO_IO_PIN) - 1) << M480_SDIO_IO_PIN_OFFSET)

vsf_err_t vsfhal_sdio_init(vsfhal_sdio_t index)
{
	uint8_t sdio_index = index & 0xFF, remap;
	struct vsfhal_sdio_param_t *sdio_param;
	const struct M480_sdio_t *sdio;
	SDH_T *sdreg;

	struct vsfhal_afio_t afio;
	vsf_err_t err;
	GPIO_T *gpio;
	uint8_t i;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio = &M480_sdio[sdio_index];
	sdio_param = &M480_sdio_param[sdio_index];
	sdreg = sdio->sdreg;

	index >>= M480_SDIO_IDX;
	for(i = 0; i < M480_SDIO_PINNUM; i++)
	{
		remap = index & M480_SDIO_IO_MASK;
		index >>= M480_SDIO_IO;

		if(remap != 0xFF)
		{
			afio.port = (remap & M480_SDIO_IO_PORT_MASK) >> M480_SDIO_IO_PORT_OFFSET;
			afio.pin = (remap & M480_SDIO_IO_PIN_MASK) >> M480_SDIO_IO_PIN_OFFSET;
			afio.remap = sdio_index == 0 ? 3 :
				(afio.port == 0) || (afio.port == 4) ? 5 :
					afio.port == 1 ? 7 : afio.port == 6 ? 3 : 0;
			err = vsfhal_afio_config(&afio);
			if(err != VSFERR_NONE)
			{
				return err;
			}

			gpio = (GPIO_T *)(GPIOA_BASE + ((uint32_t)afio.port << 6));
			gpio->PUSEL = (gpio->PUSEL & ~(((uint32_t)0x03) << (afio.pin << 1))) |
						1 << (afio.pin << 1);
		}
	}

	*sdio->divreg &= ~(0xFF << sdio->divpos);
	*sdio->selreg &= ~(0x03 << sdio->selpos);
	CLK->AHBCLK |= sdio->ahbmsk;
	SYS->IPRST1 |= sdio->ahbmsk;
	SYS->IPRST1 &= ~sdio->ahbmsk;

	sdreg->DMACTL = SDH_DMACTL_DMARST_Msk;
	while(sdreg->DMACTL & SDH_DMACTL_DMARST_Msk);
	sdreg->DMACTL = SDH_DMACTL_DMAEN_Msk;

	sdreg->GCTL |= SDH_GCTL_GCTLRST_Msk;
	while(sdreg->GCTL & SDH_GCTL_GCTLRST_Msk);
	sdreg->GCTL |= SDH_GCTL_SDEN_Msk;

	sdreg->GINTEN |= SDH_GINTEN_DTAIEN_Msk;
	sdreg->INTEN |= SDH_INTEN_DITOIEN_Msk | SDH_INTEN_RTOIEN_Msk |
			SDH_INTEN_CRCIEN_Msk | SDH_INTEN_BLKDIEN_Msk;
	NVIC_EnableIRQ((IRQn_Type)sdio->irqn);

	sdio_param->busy = 0;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_sdio_fini(vsfhal_sdio_t index)
{
	uint8_t sdio_index = index & 0xFF;
	const struct M480_sdio_t *sdio;
	SDH_T *sdreg;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio = &M480_sdio[sdio_index];
	sdreg = sdio->sdreg;

	sdreg->DMACTL &= ~SDH_DMACTL_DMAEN_Msk;
	sdreg->GCTL &= ~SDH_GCTL_SDEN_Msk;
	CLK->AHBCLK &= ~sdio->ahbmsk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_sdio_config(vsfhal_sdio_t index, uint32_t kHz,
		uint8_t buswidth, void (*callback)(void *), void *param)
{
	uint8_t sdio_index = index & 0xFF;
	struct vsfhal_sdio_param_t *sdio_param;
	const struct M480_sdio_t *sdio;
	SDH_T *sdreg;

	struct vsfhal_info_t *info;
	uint32_t div;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio = &M480_sdio[sdio_index];
	sdio_param = &M480_sdio_param[sdio_index];
	sdreg = sdio->sdreg;

	sdio_param->callback = callback;
	sdio_param->param = param;

	vsfhal_core_get_info(&info);
	*sdio->divreg &= ~(0xFF << sdio->divpos);
	*sdio->selreg &= ~(0x03 << sdio->selpos);
	if (kHz > 4000)
	{
		if (info->hclk_freq_hz <= kHz * 1000)
			kHz = info->hclk_freq_hz;
		div = info->pll_freq_hz / (kHz * 100);
		div = (div + 5) / 10;
		*sdio->selreg |= 0x02 << sdio->selpos;
	}
	else
	{
		div = info->osc_freq_hz / (kHz * 100);
		div = (div + 5) / 10;
	}
	*sdio->divreg |= div << sdio->divpos;

	if (buswidth == 1)
	{
		sdreg->CTL &= ~SDH_CTL_DBW_Msk;
	}
	else
	{
		sdreg->CTL |= SDH_CTL_DBW_Msk;
	}

	return VSFERR_NONE;
}

static void sdio_callback(struct vsfhal_sdio_param_t *sdio_param)
{
	if (sdio_param->busy)
	{
		sdio_param->busy = 0;
		sdio_param->callback(sdio_param->param);
	}
}

vsf_err_t vsfhal_sdio_start(vsfhal_sdio_t index, uint8_t cmd, uint32_t arg,
		struct vsfhal_sdio_trans_t *trans)
{
	uint8_t sdio_index = index & 0xFF;
	struct vsfhal_sdio_param_t *sdio_param;
	const struct M480_sdio_t *sdio;
	SDH_T *sdreg;

	uint32_t ctl;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio = &M480_sdio[sdio_index];
	sdio_param = &M480_sdio_param[sdio_index];
	sdreg = sdio->sdreg;

	if (sdio_param->busy)
		return VSFERR_NOT_READY;

	sdio_param->busy = 1;

	sdreg->CMDARG = arg;
	ctl = sdreg->CTL & ~(SDH_CTL_CMDCODE_Msk | SDH_CTL_BLKCNT_Msk);
	sdio_param->trans = trans;

	if (trans == NULL)
	{
		goto noblk;
	}

	if (trans->need_resp)
	{
		if (trans->long_resp)
			ctl |= SDH_CTL_R2EN_Msk;
		else
			ctl |= SDH_CTL_RIEN_Msk;
	}

	if (trans->buffer != NULL)
	{
		ctl |= ((uint32_t)cmd << SDH_CTL_CMDCODE_Pos) | SDH_CTL_COEN_Msk |
				((uint32_t)trans->block_cnt << SDH_CTL_BLKCNT_Pos);
		ctl |= trans->r0w1 ? SDH_CTL_DOEN_Msk : SDH_CTL_DIEN_Msk;
		sdreg->BLEN = trans->block_len - 1;
		sdreg->DMASA = (uint32_t)trans->buffer;

		if (trans->r0w1 == 0)
		{
			sdreg->TOUT = (trans->block_len * trans->block_cnt) * 2 + 0x100000;
		}

		sdreg->CTL = ctl;
		return VSFERR_NONE;
	}

noblk:
	sdreg->TOUT = 0x400;
	ctl |= ((uint32_t)cmd << SDH_CTL_CMDCODE_Pos) | SDH_CTL_COEN_Msk |
			(1ul << SDH_CTL_BLKCNT_Pos);
	sdreg->CTL = ctl;
	if (ctl & SDH_CTL_R2EN_Msk)
	{
		while ((sdreg->CTL & SDH_CTL_R2EN_Msk) && sdio_param->busy);
	}
	else if (ctl & SDH_CTL_RIEN_Msk)
	{
		while ((sdreg->CTL & SDH_CTL_RIEN_Msk) && sdio_param->busy);
	}
	else
	{
		while ((sdreg->CTL & SDH_CTL_COEN_Msk) && sdio_param->busy);
	}
	if (trans && trans->need_resp)
	{
		if (trans->long_resp)
		{
			// TODO
		}
		else
		{
			trans->resp[0] = sdreg->RESP1 & 0xfful;
			trans->resp[0] |= sdreg->RESP0 << 8;
			trans->resp[1] = sdreg->RESP0 >> 24;
		}
	}

	sdio_callback(sdio_param);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_sdio_stop(vsfhal_sdio_t index)
{
	uint8_t sdio_index = index & 0xFF;
	struct vsfhal_sdio_param_t *sdio_param;
	const struct M480_sdio_t *sdio;
	SDH_T *sdreg;
	uint32_t inten;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio = &M480_sdio[sdio_index];
	sdio_param = &M480_sdio_param[sdio_index];
	sdreg = sdio->sdreg;

	sdreg->CTL |= SDH_CTL_CTLRST_Msk;
	while (sdreg->CTL & SDH_CTL_CTLRST_Msk);
	sdreg->DMACTL |= SDH_DMACTL_DMARST_Msk;

	inten = sdreg->INTEN;
	sdreg->INTEN = 0;
	if (sdio_param->busy)
	{
		if (sdio_param->trans)
			sdio_param->trans->stop = 1;

		sdio_param->busy = 0;
		sdio_param->callback(sdio_param->param);
	}
	sdreg->INTEN = inten;

	return VSFERR_NONE;
}

vsf_err_t vsfhal_sdio_config_d1int(vsfhal_sdio_t index,
		void (*callback)(void *param), void *param)
{
	uint8_t sdio_index = index & 0xFF;
	struct vsfhal_sdio_param_t *sdio_param;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio_param = &M480_sdio_param[sdio_index];

	if (callback != NULL)
	{
		sdio_param->d1_callback = callback;
		sdio_param->d1_param = param;
	}
	else
	{
//		const struct M480_sdio_t *sdio = &M480_sdio[sdio_index];
//		SDH_T *sdreg = sdio->sdreg;
//		sdreg->INTEN &= ~SDH_INTEN_SDHIEN0_Msk;
//		sdreg->INTSTS = SDH_INTSTS_SDHIF0_Msk;
	}
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_sdio_enable_d1int(vsfhal_sdio_t index)
{
	uint8_t sdio_index = index & 0xFF;
	struct vsfhal_sdio_param_t *sdio_param;

	if(sdio_index >= dimof(M480_sdio))
	{
		return VSFERR_NOT_SUPPORT;
	}
	sdio_param = &M480_sdio_param[sdio_index];

	if (sdio_param->d1_callback != NULL)
	{
//		const struct M480_sdio_t *sdio = &M480_sdio[sdio_index];
//		SDH_T *sdreg = sdio->sdreg;
//		sdreg->INTSTS = SDH_INTSTS_SDHIF0_Msk;
//		sdreg->INTEN |= SDH_INTEN_SDHIEN0_Msk;
		return VSFERR_NONE;
	}
	
	return VSFERR_FAIL;
}

static void sdio_handler(uint8_t sdio_index)
{
	struct vsfhal_sdio_param_t *sdio_param = &M480_sdio_param[sdio_index];
	const struct M480_sdio_t *sdio = &M480_sdio[sdio_index];
	struct vsfhal_sdio_trans_t *trans = sdio_param->trans;
	SDH_T *sdreg = sdio->sdreg;

	uint32_t intsts = sdreg->INTSTS;

/*	if (intsts & SDH_INTSTS_SDHIF0_Msk)
	{
		if (sdio_param->d1_callback != NULL)
		{
			sdio_param->d1_callback(sdio_param->d1_param);
		}
		*sdreg->INTEN &= ~SDH_INTEN_SDHIEN0_Msk;
		*sdreg->INTSTS = SDH_INTSTS_SDHIF0_Msk;
	}
*/
	if (intsts & (SDH_INTSTS_DITOIF_Msk | SDH_INTSTS_RTOIF_Msk))
	{
		sdreg->CTL |= SDH_CTL_CTLRST_Msk;
		while (sdreg->CTL & SDH_CTL_CTLRST_Msk);
		sdreg->INTSTS = SDH_INTSTS_DITOIF_Msk | SDH_INTSTS_RTOIF_Msk;
		if (trans)
		{
			trans->timeout_error = 1;
		}
		sdio_callback(sdio_param);
	}
	if (intsts & SDH_INTSTS_CRCIF_Msk)
	{
		sdreg->CTL |= SDH_CTL_CTLRST_Msk;
		while (sdreg->CTL & SDH_CTL_CTLRST_Msk);
		sdreg->INTSTS = SDH_INTSTS_CRCIF_Msk;
		if (trans)
		{
			trans->crc7_error = (intsts & SDH_INTSTS_CRC7_Msk) ? 0 : 1;
			trans->crc16_error = (intsts & SDH_INTSTS_CRC16_Msk) ? 0 : 1;

			if (trans->buffer == NULL)
			{
				sdio_callback(sdio_param);
			}
		}
	}

	if (intsts & SDH_INTSTS_BLKDIF_Msk)
	{
		sdreg->INTSTS = SDH_INTSTS_BLKDIF_Msk;
		sdio_callback(sdio_param);
	}
	sdreg->TOUT = 0;
}

ROOTFUNC void SDH0_IRQHandler(void)
{
	sdio_handler(0);
}

ROOTFUNC void SDH1_IRQHandler(void)
{
	sdio_handler(1);
}
