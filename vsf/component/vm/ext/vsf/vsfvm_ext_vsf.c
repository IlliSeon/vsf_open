#include "vsf.h"
#ifdef VSFVM_VM
#include "vsfvm.h"
#endif
#ifdef VSFVM_COMPILER
#include "vsfvm_compiler.h"
#endif
#include "vsfvm_ext_vsf.h"

enum vsfvm_halext_gpio_mode_t
{
	VSFVM_HALEXT_GPIO_INPUT = 0,
	VSFVM_HALEXT_GPIO_OUTPP,
	VSFVM_HALEXT_GPIO_OUTOD,
	VSFVM_HALEXT_GPIO_PULLUP,
	VSFVM_HALEXT_GPIO_PULLDOWN,
};

#ifdef VSFVM_VM
static struct vsfsm_state_t *
vsfvm_ext_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfvm_thread_t *thread = (struct vsfvm_thread_t *)sm->user_data;
	switch (evt)
	{
	case VSFSM_EVT_USER:
		vsfvm_thread_ready(thread);
		break;
	}
	return NULL;
}

// GPIO implementation
static enum vsfvm_ret_t vsfvm_ext_gpio_config(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *gpio = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *mode = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfhal_gpio_pin_t *pin =
		(struct vsfhal_gpio_pin_t *)gpio->inst->buffer.buffer;

	switch (mode->uval)
	{
	case VSFVM_HALEXT_GPIO_INPUT:	mode->uval = VSFHAL_GPIO_INPUT;		break;
	case VSFVM_HALEXT_GPIO_OUTPP:	mode->uval = VSFHAL_GPIO_OUTPP;		break;
	case VSFVM_HALEXT_GPIO_OUTOD:	mode->uval = VSFHAL_GPIO_OUTOD;		break;
	case VSFVM_HALEXT_GPIO_PULLUP:	mode->uval = VSFHAL_GPIO_PULLUP;	break;
	case VSFVM_HALEXT_GPIO_PULLDOWN:mode->uval = VSFHAL_GPIO_PULLDOWN;	break;
	default: return VSFVM_RET_ERROR;
	}
	vsfhal_gpio_config(pin->port, pin->pin, mode->uval);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_gpio_set(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *gpio = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfhal_gpio_pin_t *pin;

	if (!gpio->inst) return VSFVM_RET_ERROR;
	pin = (struct vsfhal_gpio_pin_t *)gpio->inst->buffer.buffer;
	vsfhal_gpio_set(pin->port, 1 << pin->pin);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_gpio_clear(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *gpio = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfhal_gpio_pin_t *pin;

	if (!gpio->inst) return VSFVM_RET_ERROR;
	pin = (struct vsfhal_gpio_pin_t *)gpio->inst->buffer.buffer;
	vsfhal_gpio_clear(pin->port, 1 << pin->pin);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_gpio_out(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *gpio = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *value = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfhal_gpio_pin_t *pin;

	if (!gpio->inst) return VSFVM_RET_ERROR;
	pin = (struct vsfhal_gpio_pin_t *)gpio->inst->buffer.buffer;
	if (value->uval32)
		vsfhal_gpio_set(pin->port, 1 << pin->pin);
	else
		vsfhal_gpio_clear(pin->port, 1 << pin->pin);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_gpio_in(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *gpio = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfhal_gpio_pin_t *pin;
	uint32_t gpio_val;

	if (!gpio->inst) return VSFVM_RET_ERROR;
	pin = (struct vsfhal_gpio_pin_t *)gpio->inst->buffer.buffer;
	gpio_val = vsfhal_gpio_get(pin->port, 1 << pin->pin);
	vsfvm_instance_deref(thread, result);
	result->uval32 = gpio_val > 0;
	result->type = VSFVM_VAR_TYPE_VALUE;
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_gpio_toggle(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *gpio = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfhal_gpio_pin_t *pin;

	if (!gpio->inst) return VSFVM_RET_ERROR;
	pin = (struct vsfhal_gpio_pin_t *)gpio->inst->buffer.buffer;
	if (vsfhal_gpio_get(pin->port, 1 << pin->pin))
		vsfhal_gpio_clear(pin->port, 1 << pin->pin);
	else
		vsfhal_gpio_set(pin->port, 1 << pin->pin);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_gpio_create(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *port = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *pin = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfhal_gpio_pin_t *gpio;
	uint8_t p = port->uval8;

	if (vsfhal_gpio_init(p) < 0)
		return VSFVM_RET_ERROR;
	if (vsfvm_instance_alloc(result, sizeof(struct vsfhal_gpio_pin_t), &vsfvm_ext_gpio))
		return VSFVM_RET_ERROR;
	gpio = (struct vsfhal_gpio_pin_t *)result->inst->buffer.buffer;
	gpio->port = p;
	gpio->pin = pin->uval8;
	return VSFVM_RET_FINISHED;
}

// tickclk implementation
static enum vsfvm_ret_t vsfvm_ext_tickclk_getms(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);

	result->uval32 = vsfhal_tickclk_get_ms();
	return VSFVM_RET_FINISHED;
}

// timer implementation
static enum vsfvm_ret_t vsfvm_ext_timer_delayms(struct vsfvm_thread_t *thread)
{
	if (!thread->sm.init_state.evt_handler)
	{
		struct vsfvm_var_t *timeout = vsfvm_get_func_argu_ref(thread, 0);

		thread->sm.init_state.evt_handler = vsfvm_ext_evt_handler;
		thread->sm.user_data = thread;
		vsfsm_init(&thread->sm);
		if (!vsftimer_create(&thread->sm, timeout->uval32, 1, VSFSM_EVT_USER))
			return VSFVM_RET_ERROR;
		return VSFVM_RET_PEND;
	}
	else
	{
		vsfsm_fini(&thread->sm);
		thread->sm.init_state.evt_handler = NULL;
		return VSFVM_RET_FINISHED;
	}
}
#endif

const struct vsfvm_class_t vsfvm_ext_gpio =
{
#ifdef VSFVM_COMPILER
	.name = "gpio",
#endif
#ifdef VSFVM_VM
	.type = VSFVM_CLASS_USER,
#endif
};

#ifdef VSFVM_COMPILER
extern const struct vsfvm_ext_op_t vsfvm_ext_vsfhal;
static const struct vsfvmc_lexer_sym_t vsfvm_ext_halsym[] =
{
	VSFVM_LEXERSYM_CONST("GPIO_INPUT", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, VSFVM_HALEXT_GPIO_INPUT),
	VSFVM_LEXERSYM_CONST("GPIO_OUTPP", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, VSFVM_HALEXT_GPIO_OUTPP),
	VSFVM_LEXERSYM_CONST("GPIO_OUTOD", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, VSFVM_HALEXT_GPIO_OUTOD),
	VSFVM_LEXERSYM_CONST("GPIO_PULLUP", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, VSFVM_HALEXT_GPIO_PULLUP),
	VSFVM_LEXERSYM_CONST("GPIO_PULLDOWN", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, VSFVM_HALEXT_GPIO_PULLDOWN),
	VSFVM_LEXERSYM_CLASS("gpio", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio),
	VSFVM_LEXERSYM_EXTFUNC("gpio_create", &vsfvm_ext_vsfhal, NULL, &vsfvm_ext_gpio, 2, 0),
	VSFVM_LEXERSYM_EXTFUNC("gpio_config", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, &vsfvm_ext_gpio, 2, 1),
	VSFVM_LEXERSYM_EXTFUNC("gpio_set", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, &vsfvm_ext_gpio, 1, 2),
	VSFVM_LEXERSYM_EXTFUNC("gpio_clear", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, &vsfvm_ext_gpio, 1, 3),
	VSFVM_LEXERSYM_EXTFUNC("gpio_out", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, &vsfvm_ext_gpio, 2, 4),
	VSFVM_LEXERSYM_EXTFUNC("gpio_in", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, NULL, 1, 5),
	VSFVM_LEXERSYM_EXTFUNC("gpio_toggle", &vsfvm_ext_vsfhal, &vsfvm_ext_gpio, &vsfvm_ext_gpio, 1, 6),
	VSFVM_LEXERSYM_EXTFUNC("tickclk_getms", &vsfvm_ext_vsfhal, NULL, NULL, 0, 7),
};
#endif

#ifdef VSFVM_VM
static const struct vsfvm_extfunc_t vsfvm_ext_halfunc[] =
{
	// GPIO
	VSFVM_EXTFUNC("gpio_create", vsfvm_ext_gpio_create, 2),
	VSFVM_EXTFUNC("gpio_config", vsfvm_ext_gpio_config, 2),
	VSFVM_EXTFUNC("gpio_set", vsfvm_ext_gpio_set, 1),
	VSFVM_EXTFUNC("gpio_clear", vsfvm_ext_gpio_clear, 1),
	VSFVM_EXTFUNC("gpio_out", vsfvm_ext_gpio_out, 2),
	VSFVM_EXTFUNC("gpio_in", vsfvm_ext_gpio_in, 1),
	VSFVM_EXTFUNC("gpio_toggle", vsfvm_ext_gpio_toggle, 1),

	VSFVM_EXTFUNC("tickclk_getms", vsfvm_ext_tickclk_getms, 0),
};
#endif

const struct vsfvm_ext_op_t vsfvm_ext_vsfhal =
{
#ifdef VSFVM_COMPILER
	.name = "vsfhal",
	.sym = vsfvm_ext_halsym,
	.sym_num = dimof(vsfvm_ext_halsym),
#endif
#ifdef VSFVM_VM
	.init = NULL,
	.fini = NULL,
	.func = (struct vsfvm_extfunc_t *)vsfvm_ext_halfunc,
#endif
	.func_num = dimof(vsfvm_ext_halfunc),
};

#ifdef VSFVM_COMPILER
extern const struct vsfvm_ext_op_t vsfvm_ext_vsftimer;
static const struct vsfvmc_lexer_sym_t vsfvm_ext_timersym[] =
{
	VSFVM_LEXERSYM_EXTFUNC("timer_delayms", &vsfvm_ext_vsftimer, NULL, NULL, 1, 0),
};
#endif

#ifdef VSFVM_VM
static const struct vsfvm_extfunc_t vsfvm_ext_timerfunc[] =
{
	VSFVM_EXTFUNC("timer_delayms", vsfvm_ext_timer_delayms, 1),
};
#endif

const struct vsfvm_ext_op_t vsfvm_ext_vsftimer =
{
#ifdef VSFVM_COMPILER
	.name = "vsftimer",
	.sym = vsfvm_ext_timersym,
	.sym_num = dimof(vsfvm_ext_timersym),
#endif
#ifdef VSFVM_VM
	.init = NULL,
	.fini = NULL,
	.func = (struct vsfvm_extfunc_t *)vsfvm_ext_timerfunc,
#endif
	.func_num = dimof(vsfvm_ext_timerfunc),
};

#ifdef VSFHAL_HAS_USBH
#include "vsfvm_ext_vsfusbh.h"
#endif

#ifdef VSFVM_VM
void vsfvm_ext_register_vsf(struct vsfvm_t *vm)
{
	vsfvm_register_ext(vm, &vsfvm_ext_vsfhal);
	vsfvm_register_ext(vm, &vsfvm_ext_vsftimer);
#ifdef VSFHAL_HAS_USBH
	vsfvm_register_ext(vm, &vsfvm_ext_vsfusbh);
#endif
}
#endif

#ifdef VSFVM_COMPILER
void vsfvmc_ext_register_vsf(struct vsfvmc_t *vmc)
{
	vsfvmc_register_ext(vmc, &vsfvm_ext_vsfhal);
	vsfvmc_register_ext(vmc, &vsfvm_ext_vsftimer);
#ifdef VSFHAL_HAS_USBH
	vsfvmc_register_ext(vmc, &vsfvm_ext_vsfusbh);
#endif
}
#endif
