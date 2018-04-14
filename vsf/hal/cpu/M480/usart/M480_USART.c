#include "vsf.h"
#include "core.h"

#define VSFHAL_USART_NUM			6

#define M480_REGCFG_UART(uart_idx, DIVMSK, SELMSK, APBMSK, RSTMSK, IRQN)	\
	[uart_idx] = {.divmsk = (DIVMSK), .selmsk = (SELMSK), .apbmsk = (APBMSK), .rstmsk = (RSTMSK), .irqn = (IRQN)}
struct M480_uart_t
{
	uint32_t divmsk;
	uint32_t selmsk;
	uint32_t apbmsk;
	uint32_t rstmsk;
	uint8_t irqn;
} static const M480_uart[VSFHAL_USART_NUM] =
{
	M480_REGCFG_UART(0, CLK_CLKDIV0_UART0DIV_Msk, CLK_CLKSEL1_UART0SEL_Msk, CLK_APBCLK0_UART0CKEN_Msk, SYS_IPRST1_UART0RST_Msk, UART0_IRQn),
	M480_REGCFG_UART(1, CLK_CLKDIV0_UART1DIV_Msk, CLK_CLKSEL1_UART1SEL_Msk, CLK_APBCLK0_UART1CKEN_Msk, SYS_IPRST1_UART1RST_Msk, UART1_IRQn),
	M480_REGCFG_UART(2, CLK_CLKDIV4_UART2DIV_Msk, CLK_CLKSEL3_UART2SEL_Msk, CLK_APBCLK0_UART2CKEN_Msk, SYS_IPRST1_UART2RST_Msk, UART2_IRQn),
	M480_REGCFG_UART(3, CLK_CLKDIV4_UART3DIV_Msk, CLK_CLKSEL3_UART3SEL_Msk, CLK_APBCLK0_UART3CKEN_Msk, SYS_IPRST1_UART3RST_Msk, UART3_IRQn),
	M480_REGCFG_UART(4, CLK_CLKDIV4_UART4DIV_Msk, CLK_CLKSEL3_UART4SEL_Msk, CLK_APBCLK0_UART4CKEN_Msk, SYS_IPRST1_UART4RST_Msk, UART4_IRQn),
	M480_REGCFG_UART(5, CLK_CLKDIV4_UART5DIV_Msk, CLK_CLKSEL3_UART5SEL_Msk, CLK_APBCLK0_UART5CKEN_Msk, SYS_IPRST1_UART5RST_Msk, UART5_IRQn),
};

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

#define M480_UART_PINNUM					4
#define M480_USART_IDX						8
#define M480_USART_IO						12
#define M480_USART_IO_PORT					4
#define M480_USART_IO_PIN					4
#define M480_USART_IO_REMAP					4

#define M480_USART_IDX_MASK					((1 << M480_USART_IDX) - 1)
#define M480_USART_IO_MASK					((1 << M480_USART_IO) - 1)
#define M480_USART_IO_PORT_OFFSET			(0)
#define M480_USART_IO_PORT_MASK				(((1 << M480_USART_IO_PORT) - 1) << M480_USART_IO_PORT_OFFSET)
#define M480_USART_IO_PIN_OFFSET			(M480_USART_IO_PORT_OFFSET + M480_USART_IO_PORT)
#define M480_USART_IO_PIN_MASK				(((1 << M480_USART_IO_PIN) - 1) << M480_USART_IO_PIN_OFFSET)
#define M480_USART_IO_REMAP_OFFSET			(M480_USART_IO_PIN_OFFSET + M480_USART_IO_PIN)
#define M480_USART_IO_REMAP_MASK			(((1 << M480_USART_IO_REMAP) - 1) << M480_USART_IO_REMAP_OFFSET)

vsf_err_t vsfhal_usart_init(vsfhal_usart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	const struct M480_uart_t *uart_regparam = &M480_uart[uart_idx];
	struct vsfhal_afio_t afio;
	vsf_err_t err;
	uint8_t i;
	uint16_t remap;

	if(uart_idx > 5)
	{
		return VSFERR_NOT_SUPPORT;
	}
	else if(uart_idx <= 1)
	{
		CLK->CLKDIV0 &= ~uart_regparam->divmsk;
		CLK->CLKSEL1 &= ~uart_regparam->selmsk;
		CLK->APBCLK0 |= uart_regparam->apbmsk;
		SYS->IPRST1 |= uart_regparam->rstmsk;
		SYS->IPRST1 &= ~uart_regparam->rstmsk;
	}
	else
	{
		CLK->CLKDIV4 &= ~uart_regparam->divmsk;
		CLK->CLKSEL3 &= ~uart_regparam->selmsk;
		CLK->APBCLK0 |= uart_regparam->apbmsk;
		SYS->IPRST1 |= uart_regparam->rstmsk;
		SYS->IPRST1 &= ~uart_regparam->rstmsk;
	}

	index >>= M480_USART_IDX;
	for(i = 0; i < M480_UART_PINNUM; i++)
	{
		remap = index & M480_USART_IO_MASK;
		index >>= M480_USART_IO;
		if(remap)
		{
			afio.port = (remap & M480_USART_IO_PORT_MASK) >> M480_USART_IO_PORT_OFFSET;
			afio.pin = (remap & M480_USART_IO_PIN_MASK) >> M480_USART_IO_PIN_OFFSET;
			afio.remap = (remap & M480_USART_IO_REMAP_MASK) >> M480_USART_IO_REMAP_OFFSET;
			err = vsfhal_afio_config(&afio);
			if(err != VSFERR_NONE)
			{
				return err;
			}
		}
	}	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_fini(vsfhal_usart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	const struct M480_uart_t *uart_regparam = &M480_uart[uart_idx];

	if(uart_idx > 5)
	{
		return VSFERR_NOT_SUPPORT;
	}

	CLK->APBCLK0 |= uart_regparam->apbmsk;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_config(vsfhal_usart_t index, uint32_t baudrate, uint32_t mode)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	const struct M480_uart_t *uart_regparam = &M480_uart[uart_idx];
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint32_t baud_div = 0, reg_line = 0;
	struct vsfhal_info_t *info;

	// mode:
	// bit0 - bit1: parity
	// ------------------------------------- bit2 - bit3: mode [nothing]
	// bit4       : stopbits
	reg_line |= (mode << 3) & 0x18;	//parity
	reg_line |= (mode >> 2) & 0x04;	//stopbits

	usart->FUNCSEL = 0;
	usart->LINE = reg_line | 3;
	usart->FIFO = 0x2ul << 4; // 8
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
		usart->BAUD = UART_BAUD_BAUDM0_Msk | UART_BAUD_BAUDM1_Msk | (baud_div - 2);
	}
	usart->INTEN = UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk | UART_INTEN_TOCNTEN_Msk;

	NVIC_EnableIRQ((IRQn_Type)uart_regparam->irqn);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_config_cb(vsfhal_usart_t index, uint32_t int_priority,
				void *p, void (*ontx)(void *), void (*onrx)(void *))
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	vsfhal_usart_ontx[uart_idx] = ontx;
	vsfhal_usart_onrx[uart_idx] = onrx;
	vsfhal_usart_callback_param[uart_idx] = p;
	return VSFERR_NONE;
}

uint16_t vsfhal_usart_tx_bytes(vsfhal_usart_t index, uint8_t *data, uint16_t size)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint16_t i;
	
	for (i = 0; i < size; i++)
	{
		usart->DAT = data[i];
	}
	usart->INTEN |= UART_INTEN_THREIEN_Msk;

	return size;
}

uint16_t vsfhal_usart_tx_get_data_size(vsfhal_usart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint32_t fifo_len = 16;

	return (usart->FIFOSTS & UART_FIFOSTS_TXFULL_Msk) ? fifo_len :
		((usart->FIFOSTS & UART_FIFOSTS_TXPTR_Msk) >> UART_FIFOSTS_TXPTR_Pos);
}

uint16_t vsfhal_usart_tx_get_free_size(vsfhal_usart_t index)
{
	uint32_t fifo_len = 16;
	return fifo_len - vsfhal_usart_tx_get_data_size(index);
}

uint16_t vsfhal_usart_rx_bytes(vsfhal_usart_t index, uint8_t *data, uint16_t size)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint16_t i;
	
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

uint16_t vsfhal_usart_rx_get_data_size(vsfhal_usart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));
	uint32_t fifo_len = 16;

	if (usart->FIFOSTS & UART_FIFOSTS_RXFULL_Msk)
	{
		return fifo_len;
	}
	else
	{
		return (usart->FIFOSTS & UART_FIFOSTS_RXPTR_Msk) >> UART_FIFOSTS_RXPTR_Pos;
	}
}

uint16_t vsfhal_usart_rx_get_free_size(vsfhal_usart_t index)
{
	uint32_t fifo_len = 16;
	return fifo_len - vsfhal_usart_rx_get_data_size(index);
}

static void uart_handler(vsfhal_usart_t index)
{
	uint8_t uart_idx = (uint8_t)(index & M480_USART_IDX_MASK);
	UART_T *usart = (UART_T *)(UART0_BASE + (uart_idx << 12));

	if (usart->INTSTS & UART_INTSTS_RDAIF_Msk)
	{
		vsfhal_usart_onrx[uart_idx](vsfhal_usart_callback_param[uart_idx]);
	}
	if (usart->INTSTS & UART_INTSTS_RXTOIF_Msk)
	{
		// on idle
		vsfhal_usart_onrx[uart_idx](vsfhal_usart_callback_param[uart_idx]);
	}

	if (usart->INTSTS & UART_INTSTS_THREINT_Msk)
	{
		usart->INTEN &= ~UART_INTEN_THREIEN_Msk;
		vsfhal_usart_ontx[uart_idx](vsfhal_usart_callback_param[uart_idx]);
	}
}

ROOTFUNC void UART0_IRQHandler(void)
{
	uart_handler(0);
}
ROOTFUNC void UART1_IRQHandler(void)
{
	uart_handler(1);
}
ROOTFUNC void UART2_IRQHandler(void)
{
	uart_handler(2);
}
ROOTFUNC void UART3_IRQHandler(void)
{
	uart_handler(3);
}
ROOTFUNC void UART4_IRQHandler(void)
{
	uart_handler(4);
}
ROOTFUNC void UART5_IRQHandler(void)
{
	uart_handler(5);
}
