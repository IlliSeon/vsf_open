#include "vsf.h"

#if VSFHAL_GPIO_EN

static uint32_t vsfhal_gpio_port[16];
vsf_err_t vsfhal_gpio_init(uint8_t index)
{
	vsfdbg_printf("vsfhal_gpio_init(%d)\n", index);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_fini(uint8_t index)
{
	vsfdbg_printf("vsfhal_gpio_fini(%d)\n", index);
	return VSFERR_NONE;
}

const char *vsfhal_gpio_mode[] =
{
	TO_STR(VSFHAL_GPIO_INPUT),
	TO_STR(VSFHAL_GPIO_OUTPP),
	TO_STR(VSFHAL_GPIO_OUTOD),
	TO_STR(VSFHAL_GPIO_PULLUP),
	TO_STR(VSFHAL_GPIO_PULLDOWN),
};
vsf_err_t vsfhal_gpio_config(uint8_t index, uint8_t pin_idx, uint32_t mode)
{
	if (mode >= dimof(vsfhal_gpio_mode))
		vsfdbg_printf("invalid gpio mode: %d\n", mode);
	vsfdbg_printf("vsfhal_gpio_config(%d, %d, %s)\n", index, pin_idx,
		vsfhal_gpio_mode[mode]);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_set(uint8_t index, uint32_t pin_mask)
{
	vsfdbg_printf("vsfhal_gpio_set(%d, 0x%04X)\n", index, pin_mask);
	vsfhal_gpio_port[index] |= pin_mask;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_clear(uint8_t index, uint32_t pin_mask)
{
	vsfdbg_printf("vsfhal_gpio_clear(%d, 0x%04X)\n", index, pin_mask);
	vsfhal_gpio_port[index] &= ~pin_mask;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_out(uint8_t index, uint32_t pin_mask, uint32_t value)
{
	vsfdbg_printf("vsfhal_gpio_set(%d, 0x%04X, 0x%04X)\n", index, pin_mask, value);
	vsfhal_gpio_port[index] |= pin_mask & value;
	vsfhal_gpio_port[index] &= ~(pin_mask & ~value);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_gpio_in(uint8_t index, uint32_t pin_mask, uint32_t *value)
{
	vsfdbg_printf("vsfhal_gpio_in(%d, 0x%04X, *value)\n", index, pin_mask);
	*value = vsfhal_gpio_get(index, pin_mask);
	return VSFERR_NONE;
}

uint32_t vsfhal_gpio_get(uint8_t index, uint32_t pin_mask)
{
	vsfdbg_printf("vsfhal_gpio_get(%d, 0x%04X)\n", index, pin_mask);
	return vsfhal_gpio_port[index] & pin_mask;
}

#endif
