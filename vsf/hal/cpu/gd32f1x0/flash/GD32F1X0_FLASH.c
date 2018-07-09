#include "vsf.h"

#define GD32F1X0_FLASH_CR_EOPIE			((uint32_t)1 << 12)
#define GD32F1X0_FLASH_CR_LOCK				((uint32_t)1 << 7)
#define GD32F1X0_FLASH_CR_STAT				((uint32_t)1 << 6)
#define GD32F1X0_FLASH_CR_PER				((uint32_t)1 << 1)
#define GD32F1X0_FLASH_CR_PG				((uint32_t)1 << 0)

#define GD32F1X0_FLASH_SR_EOP				((uint32_t)1 << 5)
#define GD32F1X0_FLASH_SR_WRPRTERR			((uint32_t)1 << 4)
#define GD32F1X0_FLASH_SR_PGERR			((uint32_t)1 << 2)
#define GD32F1X0_FLASH_SR_BSY				((uint32_t)1 << 0)
#define GD32F1X0_FLASH_SR_ERR				\
				(GD32F1X0_FLASH_SR_WRPRTERR | GD32F1X0_FLASH_SR_PGERR)


#define GD32F1X0_FLASH_BASEADDR				0x08000000
#define GD32F1X0_FLASH_ADDR(addr)			(GD32F1X0_FLASH_BASEADDR + (addr))
#define GD32F1X0_FLASH_SIZE_KB				(*(uint16_t *)0x1FFFF7E0)

#define GD32F1X0_FLASH_KEYR_KEY1			(uint32_t)0x45670123
#define GD32F1X0_FLASH_KEYR_KEY2			(uint32_t)0xCDEF89AB
#define GD32F1X0_FLASH_OPTKEYR_KEY1			(uint32_t)0x45670123
#define GD32F1X0_FLASH_OPTKEYR_KEY2			(uint32_t)0xCDEF89AB

#define VSFHAL_FLASH_NUM					1

struct vsfhal_flash_t
{
	struct
	{
		void *param;
		void (*onfinish)(void*, vsf_err_t);
	} cb;
} static vsfhal_flash[VSFHAL_FLASH_NUM];

static vsf_err_t vsfhal_flash_checkidx(uint8_t index)
{
	return (index < VSFHAL_FLASH_NUM) ? VSFERR_NONE : VSFERR_NOT_SUPPORT;
}

vsf_err_t vsfhal_flash_capacity(uint8_t index, uint32_t *pagesize, 
		uint32_t *pagenum)
{
	uint16_t flash_size;

	switch (index)
	{
	case 0:
		flash_size = GD32F1X0_FLASH_SIZE_KB;
		if (NULL != pagesize)
		{
			*pagesize = 1024;
		}
		if (NULL != pagenum)
		{
			*pagenum = flash_size;
		}
		return VSFERR_NONE;
	default:
		return VSFERR_NOT_SUPPORT;
	}
}

uint32_t vsfhal_flash_baseaddr(uint8_t index)
{
	return GD32F1X0_FLASH_BASEADDR;
}

// op -- operation: 0(ERASE), 1(READ), 2(WRITE)
uint32_t vsfhal_flash_blocksize(uint8_t index, uint32_t addr, uint32_t size,
		int op)
{
	uint32_t pagesize;
	if (vsfhal_flash_capacity(index, &pagesize, NULL))
		return 0;
	return !op ? pagesize : 4;
}

vsf_err_t vsfhal_flash_config_cb(uint8_t index, uint32_t int_priority,
		void *param, void (*onfinish)(void*, vsf_err_t))
{
	if (vsfhal_flash_checkidx(index))
		return VSFERR_NOT_SUPPORT;

	vsfhal_flash[index].cb.param = param;
	vsfhal_flash[index].cb.onfinish = onfinish;
	if (onfinish != NULL)
	{
		NVIC_SetPriority(FMC_IRQn, int_priority);
		NVIC_EnableIRQ(FMC_IRQn);
	}
	else
	{
		NVIC_DisableIRQ(FMC_IRQn);
	}
	return VSFERR_NONE;
}

void vsfhal_flash_security_set(uint8_t index, uint8_t l0_h1)
{
	uint16_t rdp = OB->RDP;
	
	if (rdp == 0x33cc)
		return;

	if (l0_h1 == 0)
	{
		if (rdp != 0x5aa5)
			return;
	}

	if (FMC->CMR & FMC_CMR_LK)
	{
		FMC->UKEYR = GD32F1X0_FLASH_KEYR_KEY1;
		FMC->UKEYR = GD32F1X0_FLASH_KEYR_KEY2;
	}
	
	if (!(FMC->CMR & FMC_CMR_OBWE))
	{
		FMC->OBKEYR = GD32F1X0_FLASH_OPTKEYR_KEY1;
		FMC->OBKEYR = GD32F1X0_FLASH_OPTKEYR_KEY2;
	}
	
	FMC->CMR |= FMC_CMR_OBER;
	FMC->CMR |= FMC_CMR_START;
	while (FMC->CSR & FMC_CSR_BUSY);
	
	if (l0_h1)
	{
		FMC->CMR &= ~FMC_CMR_OBER;
		FMC->CMR |= FMC_CMR_OBPG;
		OB->RDP = 0x33cc;
		while (FMC->CSR & FMC_CSR_BUSY);
	}

	FMC->CMR  = FMC_CMR_LK;
}

vsf_err_t vsfhal_flash_init(uint8_t index)
{
	uint32_t size, pagenum;
	if (vsfhal_flash_capacity(index, &size, &pagenum))
		return VSFERR_FAIL;

	memset(vsfhal_flash, 0, sizeof(struct vsfhal_flash_t));
	
	FMC->UKEYR = GD32F1X0_FLASH_KEYR_KEY1;
	FMC->UKEYR = GD32F1X0_FLASH_KEYR_KEY2;
	FMC->CSR |= FMC_CSR_BUSY | FMC_CSR_PGEF | FMC_CSR_WPEF | FMC_CSR_ENDF;

	return VSFERR_NONE;
}

vsf_err_t vsfhal_flash_fini(uint8_t index)
{
	if (vsfhal_flash_checkidx(index))
		return VSFERR_NOT_SUPPORT;
	
	FMC->CMR |= FMC_CMR_LK;
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_flash_erase(uint8_t index, uint32_t addr)
{
	switch (index)
	{
	case 0:
		if (FMC->CSR & FMC_CSR_BUSY)
			return VSFERR_NOT_READY;
		
		FMC->AR = GD32F1X0_FLASH_ADDR(addr);
		FMC->CMR |= FMC_CMR_PE | FMC_CMR_ENDIE;
		FMC->CMR |= FMC_CMR_START;
		break;
	default:
		return VSFERR_NOT_SUPPORT;
	}
	
	if (!vsfhal_flash[index].cb.onfinish)
	{
		while (FMC->CSR & FMC_CSR_BUSY);
		FMC->CMR &= ~(FMC_CMR_PE | FMC_CMR_ENDIE);
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_flash_read(uint8_t index, uint32_t addr, uint8_t *buff)
{
	return VSFERR_NOT_SUPPORT;
}

vsf_err_t vsfhal_flash_write(uint8_t index, uint32_t addr, uint8_t *buff)
{
	switch (index)
	{
	case 0:
		if (FMC->CSR & FMC_CSR_BUSY)
			return VSFERR_NOT_READY;
		
		FMC->CMR |= FMC_CMR_PG | FMC_CMR_ENDIE;
		*(uint32_t *)GD32F1X0_FLASH_ADDR(addr) =  *(uint32_t *)buff;
		break;
	default:
		return VSFERR_NOT_SUPPORT;
	}
	
	if (!vsfhal_flash[index].cb.onfinish)
	{
		while (FMC->CSR & FMC_CSR_BUSY);
		FMC->CMR &= ~(FMC_CMR_PG | FMC_CMR_ENDIE);
	}
	return VSFERR_NONE;
}

void FMC_IRQHandler(void)
{
	FMC->CMR &= ~(FMC_CMR_PE | FMC_CMR_PG | FMC_CMR_ENDIE);
	if (vsfhal_flash[0].cb.onfinish != NULL)
	{
		vsfhal_flash[0].cb.onfinish(vsfhal_flash[0].cb.param,
				(FMC->CSR & FMC_CSR_WPEF) ? VSFERR_FAIL : VSFERR_NONE);
	}
}
