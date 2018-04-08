#include "vsf.h"
#include "core.h"

#if VSFHAL_I2C_EN

#define I2C_CON_I2C_STS		I2C_CTL0_SI_Msk
#define I2C_CON_START		I2C_CTL0_STA_Msk
#define I2C_CON_STOP		I2C_CTL0_STO_Msk
#define I2C_CON_ACK			I2C_CTL0_AA_Msk
#define I2C_SET_CONTROL_REG(i2c, CTL_Msk) ( (i2c)->CTL0 = ((i2c)->CTL0 & ~0x3cul) | (CTL_Msk) )

struct i2c_ctrl_t
{
	uint8_t chip_addr;
	uint8_t msg_len;
	uint8_t msg_prt;
	uint16_t msg_buf_prt;
	struct vsfhal_i2c_msg_t *msg;
	void *param;
	void (*callback)(void *, vsf_err_t);
} static i2c_ctrl[VSFHAL_I2C_NUM];

// 		index:	32bit
//		bit0 - bit3		: i2c index			4bit		
//		SCL_pin:	12bit
//		bit4 - bit7		: PORT				4bit
//		bit8 - bit11	: Pin				4bit
//		bit12 - bit15	: remap				4bit		
//		SDA_pin:	12bit
//		bit16 - bit19	: PORT				4bit
//		bit20 - bit23	: Pin				4bit
//		bit24 - bit27	: remap				4bit
//		bit28 - bit31	: reserve			4bit
#define M480_I2C_PINNUM					2
#define M480_I2C_IDX					4
#define M480_I2C_IO						12
#define M480_I2C_PORT					4
#define M480_I2C_PIN					4
#define M480_I2C_REMAP					4

#define M480_I2C_IDX_MASK				((1 << M480_I2C_IDX) - 1)
#define M480_I2C_IO_OFFSET				(M480_I2C_IDX)
#define M480_I2C_IO_MASK				((1 << M480_I2C_IO) - 1)
#define M480_I2C_ATTR_MASK				((1 << M480_I2C_PIN) - 1)

vsf_err_t vsfhal_i2c_init(vsfhal_i2c_t index)
{
	uint8_t i;
	uint8_t i2c_idx = index & M480_I2C_IDX_MASK;
	uint8_t i2c_port, i2c_pin, i2c_remap;
	for(i = 0; i < M480_I2C_PINNUM; i++)
	{
		i2c_port = (index >> (M480_I2C_IDX + M480_I2C_IO * i)) & M480_I2C_ATTR_MASK;
		i2c_pin = (index >> (M480_I2C_IDX + M480_I2C_IO * i) >> M480_I2C_PORT) & (M480_I2C_ATTR_MASK);
		i2c_remap = (index >> (M480_I2C_IDX + M480_I2C_IO * i) >> M480_I2C_PORT + M480_I2C_PIN) & (M480_I2C_ATTR_MASK);
		if(i2c_port > 7)
				return VSFERR_IO;
		if(i2c_pin >=8)
		{
			*(&(SYS -> GPA_MFPH) + i2c_port * 8) &= ~(SYS_GPA_MFPH_PA8MFP_Msk << (i2c_pin % 8 << 2));
			*(&(SYS -> GPA_MFPH) + i2c_port * 8) |= i2c_remap << SYS_GPA_MFPH_PA8MFP_Pos + (i2c_pin % 8 << 2);
		}
		else
		{
			*(&(SYS -> GPA_MFPL) + i2c_port * 8) &= ~(SYS_GPA_MFPL_PA0MFP_Msk << (i2c_pin % 8 << 2));
			*(&(SYS -> GPA_MFPL) + i2c_port * 8) |= i2c_remap << SYS_GPA_MFPL_PA0MFP_Pos + (i2c_pin % 8 << 2);
		}
	}
	switch (i2c_idx)
	{
	#if I2C0_ENABLE
	case 0:
		CLK->APBCLK0 |= CLK_APBCLK0_I2C0CKEN_Msk;
		SYS->IPRST1 |= SYS_IPRST1_I2C0RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_I2C0RST_Msk;
		break;
	#endif // I2C0_ENABLE
	#if I2C1_ENABLE
	case 1:
		CLK->APBCLK0 |= CLK_APBCLK0_I2C1CKEN_Msk;
		SYS->IPRST1 |= SYS_IPRST1_I2C1RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_I2C1RST_Msk;
		break;
	#endif // I2C1_ENABLE
	#if I2C2_ENABLE
	case 2:
		CLK->APBCLK0 |= CLK_APBCLK0_I2C2CKEN_Msk;
		SYS->IPRST1 |= SYS_IPRST1_I2C2RST_Msk;
		SYS->IPRST1 &= ~SYS_IPRST1_I2C2RST_Msk;
		break;
	#endif // I2C2_ENABLE
	default:
		return VSFERR_FAIL;
	}

	return VSFERR_NONE;
}

vsf_err_t vsfhal_i2c_fini(vsfhal_i2c_t index)
{
	uint8_t i2c_idx = index & M480_I2C_IDX_MASK;
	switch (i2c_idx)
	{
	#if I2C0_ENABLE
	case 0:
		SYS->IPRST1 |= SYS_IPRST1_I2C0RST_Msk;
		CLK->APBCLK0 &= ~CLK_APBCLK0_I2C0CKEN_Msk;
		break;
	#endif // I2C0_ENABLE
	#if I2C1_ENABLE
	case 1:
		SYS->IPRST1 |= SYS_IPRST1_I2C1RST_Msk;
		CLK->APBCLK0 &= ~CLK_APBCLK0_I2C1CKEN_Msk;
		break;
	#endif // I2C1_ENABLE
	#if I2C2_ENABLE
	case 2:
		SYS->IPRST2 |= SYS_IPRST1_I2C2RST_Msk;
		CLK->APBCLK1 &= ~CLK_APBCLK0_I2C2CKEN_Msk;
		break;
	#endif // I2C2_ENABLE
	default:
		return VSFERR_FAIL;
	}
	return VSFERR_NONE;
}

vsf_err_t vsfhal_i2c_config(vsfhal_i2c_t index, uint16_t kHz)
{
	uint32_t div;
	uint8_t i2c_idx = index & M480_I2C_IDX_MASK;
	I2C_T *i2c;
	struct vsfhal_info_t *info;

	if (index >= VSFHAL_I2C_NUM)
		return VSFERR_FAIL;

	if (vsfhal_core_get_info(&info))
		return VSFERR_FAIL;
	
	switch (i2c_idx)
	{
	case 0:
		i2c = I2C0;
		NVIC_EnableIRQ(I2C0_IRQn);
		break;
	case 1:
		i2c = I2C1;
		NVIC_EnableIRQ(I2C1_IRQn);
		break;
	case 2:
		i2c = I2C2;
		NVIC_EnableIRQ(I2C2_IRQn);
		break;
	}

	div = info->pclk_freq_hz / (kHz * 1000 * 4) - 1;
	if (div < 4)
		div = 4;
	else if (div > 255)
		div = 255;
	i2c->CLKDIV = div;
	i2c->CTL0 = I2C_CTL0_INTEN_Msk | I2C_CTL0_I2CEN_Msk;

	return VSFERR_NONE;
}

vsf_err_t vsfhal_i2c_config_cb(vsfhal_i2c_t index, void *param,
		void (*cb)(void*, vsf_err_t))
{
	uint8_t i2c_idx = index & M480_I2C_IDX_MASK;
	if (i2c_idx >= VSFHAL_I2C_NUM)
		return VSFERR_FAIL;
	
	i2c_ctrl[i2c_idx].param = param;
	i2c_ctrl[i2c_idx].callback = cb;
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_i2c_xfer(vsfhal_i2c_t index, uint16_t addr,
		struct vsfhal_i2c_msg_t *msg, uint8_t msg_len)
{
	uint8_t i2c_idx = index & M480_I2C_IDX_MASK;
	I2C_T *i2c;

	if ((i2c_idx >= VSFHAL_I2C_NUM) || !msg || !msg_len)
		return VSFERR_FAIL;

	switch (i2c_idx)
	{
	case 0:
		i2c = I2C0;
		break;
	case 1:
		i2c = I2C1;
		break;
	case 2:
		i2c = I2C2;
		break;
	}

	
	i2c_ctrl[i2c_idx].chip_addr = addr;
	i2c_ctrl[i2c_idx].msg = msg;
	i2c_ctrl[i2c_idx].msg_len = msg_len;
	i2c_ctrl[i2c_idx].msg_prt = 0;
	i2c_ctrl[i2c_idx].msg_buf_prt = 0;

	i2c->TOCTL = I2C_TOCTL_TOIF_Msk;
	i2c->TOCTL = I2C_TOCTL_TOCEN_Msk;
	I2C_SET_CONTROL_REG(i2c, I2C_CON_START);

	return VSFERR_NONE;
}

static void i2c_handler(I2C_T *i2c, struct i2c_ctrl_t *i2c_ctrl)
{
	if (i2c->TOCTL & I2C_TOCTL_TOIF_Msk)
		goto error;
	else if (i2c->CTL0 & I2C_CON_I2C_STS)
	{
		uint32_t status = i2c->STATUS0;
		struct vsfhal_i2c_msg_t *msg = &i2c_ctrl->msg[i2c_ctrl->msg_prt];

		if (msg->flag & VSFHAL_I2C_READ)
		{
			if ((status == 0x08) || (status == 0x10))
			{
				i2c->DAT = (i2c_ctrl->chip_addr << 1) + 1;
				I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS);
			}
			else if (status == 0x40)
			{
				if (msg->len > 1)
				{
					// host reply ack
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_ACK);
				}
				else if (msg->len == 1)
				{
					// host reply nack
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS);
				}
				else
				{
					goto error;
				}
			}
			else if (status == 0x50)
			{
				if (i2c_ctrl->msg_buf_prt < msg->len)
					msg->buf[i2c_ctrl->msg_buf_prt++] = i2c->DAT;
				if (i2c_ctrl->msg_buf_prt < msg->len - 1)
				{
					// host reply ack
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_ACK);
				}
				else
				{
					// host reply nack
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS);
				}
			}
			else if (status == 0x58)
			{
				if (i2c_ctrl->msg_buf_prt < msg->len)
					msg->buf[i2c_ctrl->msg_buf_prt++] = i2c->DAT;

				if (++i2c_ctrl->msg_prt < i2c_ctrl->msg_len)
				{
					i2c_ctrl->msg_buf_prt = 0;
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_START);
				}
				else
				{
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_STOP);
					i2c->TOCTL = I2C_TOCTL_TOIF_Msk;
					if (i2c_ctrl->callback)
						i2c_ctrl->callback(i2c_ctrl->param, VSFERR_NONE);
				}
			}
			else
			{
				goto error;
			}
		}
		else
		{
			if ((status == 0x08) || (status == 0x10))	// start send finish
			{
				i2c->DAT = i2c_ctrl->chip_addr << 1;
				I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS);
			}
			else if ((status == 0x18) || (status == 0x28))	// addr/data send finish and ACK received
			{
				if (i2c_ctrl->msg_buf_prt < msg->len)
				{
					i2c->DAT = msg->buf[i2c_ctrl->msg_buf_prt++];
					I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS);
				}
				else
				{
					if (++i2c_ctrl->msg_prt < i2c_ctrl->msg_len)
					{
						i2c_ctrl->msg_buf_prt = 0;
						I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_START);
					}
					else
					{
						I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_STOP);
						i2c->TOCTL = I2C_TOCTL_TOIF_Msk;
						if (i2c_ctrl->callback)
							i2c_ctrl->callback(i2c_ctrl->param, VSFERR_NONE);				
					}
				}
			}
			else
			{
				goto error;
			}
		}
	}
	return;

error:
	I2C_SET_CONTROL_REG(i2c, I2C_CON_I2C_STS | I2C_CON_STOP);
	if (i2c_ctrl->callback)
		i2c_ctrl->callback(i2c_ctrl->param, VSFERR_FAIL);
	i2c->TOCTL = I2C_TOCTL_TOIF_Msk;
}

ROOTFUNC void I2C0_IRQHandler(void)
{
	i2c_handler(I2C0, &i2c_ctrl[0]);
}

ROOTFUNC void I2C1_IRQHandler(void)
{
	i2c_handler(I2C1, &i2c_ctrl[1]);
}

ROOTFUNC void I2C2_IRQHandler(void)
{
	i2c_handler(I2C2, &i2c_ctrl[2]);
}

#endif

