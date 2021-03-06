#include "vsf.h"
#include "M480.h"

/*---------------------------------------------------------------------------------------------------------*/
/* Define Base Address                                                                                     */
/*---------------------------------------------------------------------------------------------------------*/
#define FMC_APROM_BASE          0x00000000UL    /*!< APROM Base Address           \hideinitializer */
#define FMC_APROM_END           0x00080000UL    /*!< APROM End Address            \hideinitializer */
#define FMC_LDROM_BASE          0x00100000UL    /*!< LDROM Base Address           \hideinitializer */
#define FMC_LDROM_END           0x00104000UL    /*!< LDROM End Address            \hideinitializer */
#define FMC_CONFIG_BASE         0x00300000UL    /*!< User Configuration Address   \hideinitializer */

#define FMC_FLASH_PAGE_SIZE     0x800           /*!< Flash Page Size (2 Kbytes)   \hideinitializer */
#define FMC_LDROM_SIZE          0x4000          /*!< LDROM Size (16 Kbytes)       \hideinitializer */

/*---------------------------------------------------------------------------------------------------------*/
/*  ISPCMD constant definitions                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define FMC_ISPCMD_READ         0x00            /*!< ISP Command: Read flash word          \hideinitializer */
#define FMC_ISPCMD_READ_64      0x40            /*!< ISP Command: Read flash double word   \hideinitializer */
#define FMC_ISPCMD_WRITE        0x21            /*!< ISP Command: Write flash word         \hideinitializer */
#define FMC_ISPCMD_WRITE_64     0x61            /*!< ISP Command: Write flash double word  \hideinitializer */
#define FMC_ISPCMD_PAGE_ERASE   0x22            /*!< ISP Command: Page Erase Flash         \hideinitializer */
#define FMC_ISPCMD_READ_CID     0x0B            /*!< ISP Command: Read Company ID          \hideinitializer */
#define FMC_ISPCMD_READ_DID     0x0C            /*!< ISP Command: Read Device ID           \hideinitializer */
#define FMC_ISPCMD_READ_UID     0x04            /*!< ISP Command: Read Unique ID           \hideinitializer */
#define ISP_ISPCMD_MULTI_WRITE  0x27            /*!< ISP Command: Multiple program         \hideinitializer */
#define FMC_ISPCMD_VECMAP       0x2E            /*!< ISP Command: Vector Page Remap        \hideinitializer */

#define IS_BOOT_FROM_APROM      0               /*!< Is booting from APROM                 \hideinitializer */
#define IS_BOOT_FROM_LDROM      1               /*!< Is booting from LDROM                 \hideinitializer */

#define FMC_SET_APROM_BOOT()        (FMC->ISPCTL &= ~FMC_ISPCTL_BS_Msk)         /*!< Select booting from APROM   \hideinitializer */
#define FMC_SET_LDROM_BOOT()        (FMC->ISPCTL |= FMC_ISPCTL_BS_Msk)          /*!< Select booting from LDROM   \hideinitializer */
#define FMC_ENABLE_AP_UPDATE()      (FMC->ISPCTL |=  FMC_ISPCTL_APUEN_Msk)      /*!< Enable APROM update         \hideinitializer */
#define FMC_DISABLE_AP_UPDATE()     (FMC->ISPCTL &= ~FMC_ISPCTL_APUEN_Msk)      /*!< Disable APROM update        \hideinitializer */
#define FMC_ENABLE_CFG_UPDATE()     (FMC->ISPCTL |=  FMC_ISPCTL_CFGUEN_Msk)     /*!< Enable User Config update   \hideinitializer */
#define FMC_DISABLE_CFG_UPDATE()    (FMC->ISPCTL &= ~FMC_ISPCTL_CFGUEN_Msk)     /*!< Disable User Config update  \hideinitializer */
#define FMC_ENABLE_LD_UPDATE()      (FMC->ISPCTL |=  FMC_ISPCTL_LDUEN_Msk)      /*!< Enable LDROM update         \hideinitializer */
#define FMC_DISABLE_LD_UPDATE()     (FMC->ISPCTL &= ~FMC_ISPCTL_LDUEN_Msk)      /*!< Disable LDROM update        \hideinitializer */
#define FMC_DISABLE_ISP()           (FMC->ISPCTL &= ~FMC_ISPCTL_ISPEN_Msk)      /*!< Disable ISP function        \hideinitializer */
#define FMC_ENABLE_ISP()            (FMC->ISPCTL |=  FMC_ISPCTL_ISPEN_Msk)      /*!< Enable ISP function         \hideinitializer */
#define FMC_GET_FAIL_FLAG()         (FMC->ISPSTS & FMC_ISPSTS_ISPFF_Msk)        /*!< Get ISP fail flag           \hideinitializer */
#define FMC_CLR_FAIL_FLAG()         (FMC->ISPSTS |= FMC_ISPSTS_ISPFF_Msk)       /*!< Clear ISP fail flag         \hideinitializer */

#define M480_FLASH_NUM					1

#define M480_FLASH_BASEADDR				0x00000000
#define M480_FLASH_ADDR(addr)				(M480_FLASH_BASEADDR + (addr))
#define M480_FLASH_SIZE_KB				(512)

extern void m480_unlock_reg(void);
extern void m480_lock_reg(void);
	
vsf_err_t vsfhal_flash_checkidx(uint8_t index)
{
	return (index < M480_FLASH_NUM) ? VSFERR_NONE : VSFERR_NOT_SUPPORT;
}

vsf_err_t vsfhal_flash_capacity(uint8_t index, uint32_t *pagesize, 
		uint32_t *pagenum)
{
	switch (index)
	{
	case 0:
		if (NULL != pagesize)
		{
			*pagesize = 2 * 1024;
		}
		if (NULL != pagenum)
		{
			*pagenum = M480_FLASH_SIZE_KB / 2;
		}
		return VSFERR_NONE;
	default:
		return VSFERR_NOT_SUPPORT;
	}
}

uint32_t vsfhal_flash_baseaddr(uint8_t index)
{
	return M480_FLASH_BASEADDR;
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

vsf_err_t vsfhal_flash_init(uint8_t index)
{
	switch (index)
	{
	case 0:
		m480_unlock_reg();
		FMC->ISPCTL |= FMC_ISPCTL_APUEN_Msk;
		FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk;
		m480_lock_reg();
		return VSFERR_NONE;
	default:
		return VSFERR_NOT_SUPPORT;
	}
}

vsf_err_t vsfhal_flash_fini(uint8_t index)
{
	switch (index)
	{
	case 0:
		m480_unlock_reg();
		FMC->ISPCTL &= ~FMC_ISPCTL_ISPEN_Msk;
		m480_lock_reg();
		return VSFERR_NONE;
	default:
		return VSFERR_NOT_SUPPORT;
	}
}

vsf_err_t vsfhal_flash_erase(uint8_t index, uint32_t addr)
{
	switch (index)
	{
	case 0:
		m480_unlock_reg();
		FMC->ISPCMD = FMC_ISPCMD_PAGE_ERASE;
		FMC->ISPADDR = M480_FLASH_ADDR(addr);
		FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;
		while (FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk);
		m480_lock_reg();
		break;
	default:
		return VSFERR_NOT_SUPPORT;
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
		m480_unlock_reg();
		FMC->ISPCMD = FMC_ISPCMD_WRITE;
		FMC->ISPADDR = M480_FLASH_ADDR(addr);
		FMC->MPDAT0 = *(uint32_t *)buff;
		FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;
		while (FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk);
		m480_lock_reg();
		break;
	default:
		return VSFERR_NOT_SUPPORT;
	}
	return VSFERR_NONE;
}
