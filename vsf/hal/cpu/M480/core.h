#ifndef __CORE_H__
#define __CORE_H__

#define M480_CLK_HXT			(1UL << 0)
#define M480_CLK_LXT			(1UL << 1)
#define M480_CLK_HIRC			(1UL << 2)
#define M480_CLK_LIRC			(1UL << 3)

#define MODULE_APBCLK(x)        (((x) >>30) & 0x3UL)    /*!< Calculate AHBCLK/APBCLK offset on MODULE index, 0x0:AHBCLK, 0x1:APBCLK0, 0x2:APBCLK1 \hideinitializer */
#define MODULE_CLKSEL(x)        (((x) >>28) & 0x3UL)    /*!< Calculate CLKSEL offset on MODULE index, 0x0:CLKSEL0, 0x1:CLKSEL1, 0x2:CLKSEL2, 0x3:CLKSEL3 \hideinitializer */
#define MODULE_CLKSEL_Msk(x)    (((x) >>25) & 0x7UL)    /*!< Calculate CLKSEL mask offset on MODULE index \hideinitializer */
#define MODULE_CLKSEL_Pos(x)    (((x) >>20) & 0x1fUL)   /*!< Calculate CLKSEL position offset on MODULE index \hideinitializer */
#define MODULE_CLKDIV(x)        (((x) >>18) & 0x3UL)    /*!< Calculate APBCLK CLKDIV on MODULE index, 0x0:CLKDIV0, 0x1:CLKDIV1, 0x2:CLKDIV3, 0x3:CLKDIV4 \hideinitializer */
#define MODULE_CLKDIV_Msk(x)    (((x) >>10) & 0xffUL)   /*!< Calculate CLKDIV mask offset on MODULE index \hideinitializer */
#define MODULE_CLKDIV_Pos(x)    (((x) >>5 ) & 0x1fUL)   /*!< Calculate CLKDIV position offset on MODULE index \hideinitializer */
#define MODULE_IP_EN_Pos(x)     (((x) >>0 ) & 0x1fUL)   /*!< Calculate APBCLK offset on MODULE index \hideinitializer */
#define MODULE_NoMsk            0x0UL                   /*!< Not mask on MODULE index \hideinitializer */
#define NA                      MODULE_NoMsk            /*!< Not Available \hideinitializer */

enum vsfhal_hclksrc_t
{
	M480_HCLKSRC_HIRC = 7,
	M480_HCLKSRC_PLL2FOUT = 4,
	M480_HCLKSRC_LIRC = 3,
	M480_HCLKSRC_PLLFOUT = 2,
	M480_HCLKSRC_LXT = 1,
	M480_HCLKSRC_HXT = 0,
};
enum vsfhal_pclksrc_t
{
	M480_PCLKSRC_HCLK = 0,
	M480_PCLKSRC_HCLKd2 = 1,
};
enum vsfhal_pllsrc_t
{
	M480_PLLSRC_HXT = 0,
	M480_PLLSRC_HIRC = 1,
	M480_PLLSRC_NONE = -1,
};
struct vsfhal_info_t
{
	uint8_t priority_group;
	uint32_t vector_table;
	
	uint32_t clk_enable;
	
	enum vsfhal_hclksrc_t hclksrc;
	enum vsfhal_pclksrc_t pclksrc;
	enum vsfhal_pllsrc_t pllsrc;
	
	uint32_t osc_freq_hz;
	uint32_t osc32k_freq_hz;
	uint32_t hirc_freq_hz;
	uint32_t lirc_freq_hz;
	uint32_t pll_freq_hz;
	uint32_t cpu_freq_hz;
	uint32_t hclk_freq_hz;
	uint32_t pclk_freq_hz;
};
struct vsfhal_afio_t
{
	uint8_t port;
	uint8_t pin;
	uint8_t remap;
};

vsf_err_t vsfhal_afio_config(struct vsfhal_afio_t *afio);
vsf_err_t vsfhal_core_get_info(struct vsfhal_info_t **info);

// special
void m480_unlock_reg(void);
void m480_lock_reg(void);
extern void SystemInit (void);
#endif	// __M480_CORE_H_INCLUDED__

