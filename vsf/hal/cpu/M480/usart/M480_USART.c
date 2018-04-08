#include "vsf.h"
#include "core.h"

#if VSFHAL_USART_EN

#define UART_IS_RX_READY(uart)		((uart->FIFOSTS & UART_FIFOSTS_RXEMPTY_Msk) >> UART_FIFOSTS_RXEMPTY_Pos)
#define UART_IS_TX_EMPTY(uart)		((uart->FIFOSTS & UART_FIFOSTS_TXEMPTYF_Msk) >> UART_FIFOSTS_TXEMPTYF_Pos)
#define UART_IS_TX_FIFO_FULL(uart)	((uart->FIFOSTS & UART_FIFOSTS_TXFULL_Msk) >> UART_FIFOSTS_TXFULL_Pos)

static void (*vsfhal_usart_ontx[VSFHAL_USART_NUM])(void *);
static void (*vsfhal_usart_onrx[VSFHAL_USART_NUM])(void *);
static void *vsfhal_usart_callback_param[VSFHAL_USART_NUM];

// 		index
//		bit0 - bit7		: usart index		8bit
//		RX_Pin
//		bit8 - bit11	: PORT				4bit
//		bit12 - bit15	: Pin				4bit
//		bit16 - bit19	: remap				4bit
//		TX_Pin
//		bit20 - bit23	: PORT				4bit
//		bit24 - bit27	: Pin				4bit
//		bit28 - bit31	: remap				4bit
//		nCTS_Pin
//		bit32 - bit35	: PORT				4bit
//		bit36 - bit39	: Pin				4bit
//		bit40 - bit43	: remap				4bit
//		nRTS_Pin
//		bit44 - bit47	: PORT				4bit
//		bit48 - bit51	: Pin				4bit
//		bit52 - bit55	: remap				4bit
//		enablePin
//		bit56 - bit59	: enable			4bit

#define M480_UART_PINNUM					4
#define M480_USART_IDX						8
#define M480_USART_IO						12
#define M480_USART_PORT						4
#define M480_USART_PIN						4
#define M480_USART_REMAP					4
#define M480_USART_PIN_ENABLE				M480_UART_PINNUM

#define M480_USART_IDX_MASK					((1 << M480_USART_IDX) - 1)
#define M480_USART_IO_OFFSET        		(M480_USART_IDX)
#define M480_USART_IO_MASK          		((1 << M480_USART_IO) - 1)
#define M480_USART_ATTR_MASK				((1 << M480_USART_PIN) - 1)
#define M480_USART_PIN_ENABLE_OFFSET		(M480_USART_IDX + M480_USART_IO * M480_UART_PINNUM)
#define M480_USART_PIN_ENABLE_MASK			((1 << M480_USART_PIN_ENABLE) -1)
vsf_err_t vsfhal_usart_init(vsfhal_uart_t index)
{
	uint8_t i;
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	uint8_t uart_port, uart_pin, uart_remap;
	uint8_t uart_enable = (index >> M480_USART_PIN_ENABLE_OFFSET) & M480_USART_PIN_ENABLE_MASK;
	for(i = 0; i < M480_UART_PINNUM; i++)
	{
		if((uart_enable >> i) & true)
		{
			uart_port = (index >> (M480_USART_IDX + M480_USART_IO * i)) & M480_USART_ATTR_MASK;
			uart_pin = (index >> (M480_USART_IDX + M480_USART_IO * i) >> M480_USART_PORT) & M480_USART_ATTR_MASK;
			uart_remap = (index >> (M480_USART_IDX + M480_USART_IO * i) >> M480_USART_PORT + M480_USART_PIN) & M480_USART_ATTR_MASK;
			if(uart_port > 7)
				return VSFERR_IO;
			if(uart_pin >=8)
			{
				*(&(SYS -> GPA_MFPH) + uart_port * 8) &= ~(SYS_GPA_MFPH_PA8MFP_Msk << (uart_pin % 8 << 2));
				*(&(SYS -> GPA_MFPH) + uart_port * 8) |= uart_remap << SYS_GPA_MFPH_PA8MFP_Pos + (uart_pin % 8 << 2);
			}
			else
			{
				*(&(SYS -> GPA_MFPL) + uart_port * 8) &= ~(SYS_GPA_MFPL_PA0MFP_Msk << (uart_pin % 8 << 2));
				*(&(SYS -> GPA_MFPL) + uart_port * 8) |= uart_remap << SYS_GPA_MFPL_PA0MFP_Pos + (uart_pin % 8 << 2);
			}
		}
	}
	switch (uart_idx)
	{
	#if VSFHAL_USART0_ENABLE
	case 0:
    {
		CLK->CLKDIV0 &= ~CLK_CLKDIV0_UART0DIV_Msk;
		CLK->CLKSEL1 &= ~CLK_CLKSEL1_UART0SEL_Msk;
		CLK->APBCLK0 |= CLK_APBCLK0_UART0CKEN_Msk;
        SYS->IPRST1 |= SYS_IPRST1_UART0RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_UART0RST_Msk;
		break;
	}
	#endif // VSFHAL_USART0_ENABLE
	#if VSFHAL_USART1_ENABLE
	case 1:
	{
		CLK->CLKDIV0 &= ~CLK_CLKDIV0_UART1DIV_Msk;
		CLK->CLKSEL1 &= ~CLK_CLKSEL1_UART1SEL_Msk;
		CLK->APBCLK0 |= CLK_APBCLK0_UART1CKEN_Msk;
        SYS->IPRST1 |= SYS_IPRST1_UART1RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_UART1RST_Msk;
		break;
	}
	#endif // VSFHAL_USART1_ENABLE
	#if VSFHAL_USART2_ENABLE
	case 2:
	{
        CLK->CLKDIV4 &= ~CLK_CLKDIV4_UART2DIV_Msk;
		CLK->CLKSEL3 &= ~CLK_CLKSEL3_UART2SEL_Msk;
		CLK->APBCLK0 |= CLK_APBCLK0_UART2CKEN_Msk;
        SYS->IPRST1 |= SYS_IPRST1_UART2RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_UART2RST_Msk;
		break;
	}
	#endif // VSFHAL_USART2_ENABLE
	#if VSFHAL_USART3_ENABLE
	case 3:
	{
		CLK->CLKDIV4 &= ~CLK_CLKDIV4_UART3DIV_Msk;
		CLK->CLKSEL3 &= ~CLK_CLKSEL3_UART3SEL_Msk;
		CLK->APBCLK0 |= CLK_APBCLK0_UART3CKEN_Msk;
        SYS->IPRST1 |= SYS_IPRST1_UART3RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_UART3RST_Msk;
		break;
	}
	#endif // VSFHAL_USART3_ENABLE
	#if VSFHAL_USART4_ENABLE
	case 4:
	{
		CLK->CLKDIV4 &= ~CLK_CLKDIV4_UART4DIV_Msk;
		CLK->CLKSEL3 &= ~CLK_CLKSEL3_UART4SEL_Msk;
		CLK->APBCLK0 |= CLK_APBCLK0_UART4CKEN_Msk;
        SYS->IPRST1 |= SYS_IPRST1_UART4RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_UART4RST_Msk;
		break;
	}
	#endif // VSFHAL_USART4_ENABLE
                
        #if VSFHAL_USART5_ENABLE
	case 5:
	{
		CLK->CLKDIV4 &= ~CLK_CLKDIV4_UART5DIV_Msk;
		CLK->CLKSEL3 &= ~CLK_CLKSEL3_UART5SEL_Msk;
		CLK->APBCLK0 |= CLK_APBCLK0_UART5CKEN_Msk;
        SYS->IPRST1 |= SYS_IPRST1_UART5RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_UART5RST_Msk;
		break;
	}
    #endif // VSFHAL_USART5_ENABLE
	default:
		return VSFERR_NOT_SUPPORT;
	}
    return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_fini(vsfhal_uart_t index)
{
	switch (index & M480_USART_IDX_MASK)
	{
	#if VSFHAL_USART0_ENABLE
	case 0:
		CLK->APBCLK0 &= ~CLK_APBCLK0_UART0CKEN_Msk;
		break;
	#endif // VSFHAL_USART0_ENABLE
	#if VSFHAL_USART1_ENABLE
	case 1:
		CLK->APBCLK0 &= ~CLK_APBCLK0_UART1CKEN_Msk;
		break;
	#endif // VSFHAL_USART1_ENABLE
	#if VSFHAL_USART2_ENABLE
	#endif // VSFHAL_USART2_ENABLE
	#if VSFHAL_USART3_ENABLE
	case 3:
		CLK->APBCLK0 &= ~CLK_APBCLK0_UART3CKEN_Msk;
		break;
	#endif // VSFHAL_USART3_ENABLE
	#if VSFHAL_USART4_ENABLE
	case 4:
		CLK->APBCLK0 &= ~CLK_APBCLK0_UART4CKEN_Msk;
		break;
	#endif // VSFHAL_USART4_ENABLE
	#if VSFHAL_USART4_ENABLE
	case 4:
		CLK->APBCLK0 &= ~CLK_APBCLK0_UART5CKEN_Msk;
		break;
	#endif // VSFHAL_USART4_ENABLE
	default:
		return VSFERR_NOT_SUPPORT;
	}
    return VSFERR_NONE;
}
vsf_err_t vsfhal_usart_config(vsfhal_uart_t index, uint32_t baudrate, uint32_t mode)
{
    UART_T *usart;
	struct vsfhal_info_t *info;
	uint32_t baud_div = 0, reg_line = 0;
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	
	usart = (UART_T *)(UART0_BASE + (uart_idx << 12));

	// mode:
	// bit0 - bit1: parity
	// ------------------------------------- bit2 - bit3: mode [nothing]
	// bit4       : stopbits
	reg_line |= (mode << 3) & 0x18;	//parity
	reg_line |= (mode >> 2) & 0x04;	//stopbits

	usart->FUNCSEL = 0;
	usart->LINE = reg_line | 3;
	usart->FIFO = 0x5ul << 4; // 46/14 (64/16)
	usart->TOUT = 60;

	if (vsfhal_core_get_info(&info))
	{
		return VSFERR_FAIL;
	}

	if(baudrate != 0)
	{
		int32_t error;
		baud_div = info->osc_freq_hz / baudrate;
		if ((baud_div < 11) || (baud_div > (0xffff + 2)))
			return VSFERR_INVALID_PARAMETER;
		error = (info->osc_freq_hz / baud_div) * 1000 / baudrate;
		error -= 1000;
		if ((error > 20) || ((error < -20)))
			return VSFERR_INVALID_PARAMETER;
		if (info->osc_freq_hz * 1000 / baud_div / baudrate)
		usart->BAUD = UART_BAUD_BAUDM0_Msk | UART_BAUD_BAUDM1_Msk |
						(baud_div - 2);
	}

	switch (uart_idx)
	{
	#if VSFHAL_USART0_ENABLE
	case 0:
		usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk |
						UART_INTEN_TOCNTEN_Msk;
		NVIC_EnableIRQ(UART0_IRQn);
		break;
	#endif
	#if VSFHAL_USART1_ENABLE
	case 1:
		usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk |
						UART_INTEN_TOCNTEN_Msk;
		NVIC_EnableIRQ(UART1_IRQn);
		break;
	#endif
	#if VSFHAL_USART2_ENABLE
	case 2:
		usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk |
						UART_INTEN_TOCNTEN_Msk;
		NVIC_EnableIRQ(UART2_IRQn);
		break;
	#endif
	#if VSFHAL_USART3_ENABLE
	case 3:
		usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk |
						UART_INTEN_TOCNTEN_Msk;
		NVIC_EnableIRQ(UART3_IRQn);
		break;
	#endif
	#if VSFHAL_USART4_ENABLE
	case 4:
		usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk |
						UART_INTEN_TOCNTEN_Msk;
		NVIC_EnableIRQ(UART4_IRQn);
		break;
	#endif
    #if VSFHAL_USART5_ENABLE
	case 5:
		usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk |
						UART_INTEN_TOCNTEN_Msk;
		NVIC_EnableIRQ(UART5_IRQn);
		break;
	#endif
	default:
		break;
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_config_cb(vsfhal_uart_t index, uint32_t int_priority,
				void *p, void (*ontx)(void *), void (*onrx)(void *))
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	vsfhal_usart_ontx[uart_idx] = ontx;
	vsfhal_usart_onrx[uart_idx] = onrx;
	vsfhal_usart_callback_param[uart_idx] = p;

	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_tx(vsfhal_uart_t index, uint16_t data)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));

	usart->DAT = (uint8_t)data;
	usart->INTEN |= UART_INTEN_THREIEN_Msk;
	return VSFERR_NONE;
}

uint16_t vsfhal_usart_rx(vsfhal_uart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));

	return usart->DAT;
}

uint16_t vsfhal_usart_tx_bytes(vsfhal_uart_t index, uint8_t *data, uint16_t size)
{
	uint16_t i;
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));

	for (i = 0; i < size; i++)
	{
		usart->DAT = data[i];
	}
	usart->INTEN |= UART_INTEN_THREIEN_Msk;

	return 0;
}

uint16_t vsfhal_usart_tx_get_free_size(vsfhal_uart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint32_t fifo_len = 16;

	if (usart->FIFOSTS & UART_FIFOSTS_TXFULL_Msk)
		return 0;
	else
		return fifo_len - ((usart->FIFOSTS & UART_FIFOSTS_TXPTR_Msk) >>
							UART_FIFOSTS_TXPTR_Pos);
}

uint16_t vsfhal_usart_rx_bytes(vsfhal_uart_t index, uint8_t *data, uint16_t size)
{
	uint16_t i;
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));

	for (i = 0; i < size; i++)
	{
		if (usart->FIFOSTS & (UART_FIFOSTS_RXFULL_Msk | UART_FIFOSTS_RXPTR_Msk))
		{
			data[i] = usart->DAT;
		}
		else
		{
			break;
		}
	}
	return i;
}

uint16_t vsfhal_usart_rx_get_data_size(vsfhal_uart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint32_t fifo_len = 16;

	if (usart->FIFOSTS & UART_FIFOSTS_RXFULL_Msk)
		return fifo_len;
	else
		return (usart->FIFOSTS & UART_FIFOSTS_RXPTR_Msk) >>
				UART_FIFOSTS_RXPTR_Pos;
}

static void uart_handler(vsfhal_uart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	
	if (usart->INTSTS & UART_INTSTS_RDAIF_Msk)
	{
		vsfhal_usart_onrx[uart_idx](vsfhal_usart_callback_param[uart_idx]);
	}
	else if (usart->INTSTS & UART_INTSTS_RXTOINT_Msk)
	{
		vsfhal_usart_onrx[uart_idx](vsfhal_usart_callback_param[uart_idx]);
	}
	if (usart->INTSTS & UART_INTSTS_THREINT_Msk)
	{
		usart->INTEN &= ~UART_INTEN_THREIEN_Msk;
		vsfhal_usart_ontx[uart_idx](vsfhal_usart_callback_param[uart_idx]);
	}
}

#if VSFHAL_USART0_ENABLE
ROOTFUNC void UART0_IRQHandler(void)
{
	uart_handler(0);
}
#endif
#if VSFHAL_USART1_ENABLE
ROOTFUNC void UART1_IRQHandler(void)
{
	uart_handler(1);
}
#endif
#if VSFHAL_USART2_ENABLE
ROOTFUNC void UART2_IRQHandler(void)
{
	uart_handler(2);
}
#endif
#if VSFHAL_USART3_ENABLE
ROOTFUNC void UART3_IRQHandler(void)
{
	uart_handler(3);
}
#endif
#if VSFHAL_USART4_ENABLE
ROOTFUNC void UART4_IRQHandler(void)
{
	uart_handler(4);
}
#endif
#if VSFHAL_USART5_ENABLE
ROOTFUNC void UART5_IRQHandler(void)
{
	uart_handler(5);
}
#endif

#endif