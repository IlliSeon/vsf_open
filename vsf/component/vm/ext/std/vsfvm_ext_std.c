#include "vsf.h"
#include "vsfvm.h"
#include "vsfvm_ext_std.h"

#ifdef VSFVM_VM
static enum vsfvm_ret_t vsfvm_ext_print(struct vsfvm_thread_t *thread)
{
	struct vsf_buffer_t buffer;
	struct vsfvm_var_t *var;

	for (uint8_t i = 0; i < thread->func.argc; i++)
	{
		var = vsfvm_get_func_argu_ref(thread, i);
		if (!var) return VSFVM_RET_ERROR;

		switch (var->type)
		{
		case VSFVM_VAR_TYPE_VALUE:
			vsfdbg_printf("%d", var->value);
			break;
		case VSFVM_VAR_TYPE_RESOURCES:
			if (vsfvm_get_res(thread->script, var->value, &buffer))
				return VSFVM_RET_ERROR;
			vsfdbg_printf("%s", buffer.buffer);
			break;
		case VSFVM_VAR_TYPE_FUNCTION:
			vsfdbg_printf("function@%d", var->uval16);
			break;
		case VSFVM_VAR_TYPE_INSTANCE:
			if (var->inst != NULL)
			{
				if (var->inst->c->op.print != NULL)
					var->inst->c->op.print(var);
				else
					vsfdbg_printb(var->inst->buffer.buffer,
						var->inst->buffer.size, 1, 0, false, true);
			}
			else
			{
				vsfdbg_prints("NULL");
			}
			break;
		default:
			vsfdbg_printf("unknown type(%d)", var->type);
		}
	}
	return VSFVM_RET_FINISHED;
}

struct vsfvm_ext_array_t
{
	uint16_t dimension;
	uint16_t ele_size;
	uint32_t *dim_size;
	union
	{
		void *buffer;
		uint8_t *buf8;
		uint16_t *buf16;
		uint32_t *buf32;
	};
};

static enum vsfvm_ret_t vsfvm_ext_array_create(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *dimension = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *ele_size = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfvm_var_t *var;
	struct vsfvm_ext_array_t *arr;
	uint32_t size, buffsize, dim;

	if (!dimension || (thread->func.argc != (2 + dimension->uval8)) ||
		((ele_size->uval8 != 1) && (ele_size->uval8 != 2) && (ele_size->uval8 != 4)))
	{
		return VSFVM_RET_ERROR;
	}
	dim = dimension->uval8;

	size = sizeof(*arr) + dim * sizeof(uint32_t);
	buffsize = ele_size->uval8;
	for (uint8_t i = 0; i < dim; i++)
	{
		var = vsfvm_get_func_argu_ref(thread, i + 2);
		if (!var) return VSFVM_RET_ERROR;
		buffsize *= var->uval8;
	}
	size += buffsize;

	if (vsfvm_instance_alloc(result, size, &vsfvm_ext_array))
		return VSFVM_RET_ERROR;
	arr = (struct vsfvm_ext_array_t *)result->inst->buffer.buffer;
	arr->dimension = dim;
	arr->ele_size = ele_size->uval8;
	arr->dim_size = (uint32_t *)&arr[1];
	arr->buffer = &arr->dim_size[arr->dimension];
	for (uint8_t i = 0; i < dim; i++)
	{
		var = vsfvm_get_func_argu_ref(thread, i + 2);
		arr->dim_size[i] = var->uval8;
	}
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_array_get(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *thiz = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *var;
	struct vsfvm_ext_array_t *arr;
	uint32_t pos, size, allsize;

	if (!thiz || !thiz->inst) return VSFVM_RET_ERROR;
	arr = (struct vsfvm_ext_array_t *)thiz->inst->buffer.buffer;
	if (thread->func.argc != (1 + arr->dimension)) return VSFVM_RET_ERROR;

	pos = 0;
	allsize = 1;
	for (uint8_t i = 0; i < arr->dimension; i++)
	{
		var = vsfvm_get_func_argu_ref(thread, i + 1);
		size = 1;
		for (uint8_t j = i + 1; j < arr->dimension; j++)
			size *= arr->dim_size[j];
		pos += size * var->uval8;
		allsize *= arr->dim_size[i];
	}
	if (pos >= allsize) return VSFVM_RET_ERROR;

	vsfvm_instance_deref(thread, thiz);
	result->uval32 = 0;
	switch (arr->ele_size)
	{
	case 1: result->uval8 = arr->buf8[pos]; break;
	case 2: result->uval16 = arr->buf16[pos]; break;
	case 4: result->uval32 = arr->buf32[pos]; break;
	}
	result->type = VSFVM_VAR_TYPE_VALUE;
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_array_set(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *thiz = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *var;
	struct vsfvm_ext_array_t *arr;
	uint32_t pos, size, allsize;

	if (!thiz || !thiz->inst) return VSFVM_RET_ERROR;
	arr = (struct vsfvm_ext_array_t *)thiz->inst->buffer.buffer;
	if (thread->func.argc < (1 + arr->dimension)) return VSFVM_RET_ERROR;

	pos = 0;
	allsize = 1;
	for (uint8_t i = 0; i < arr->dimension; i++)
	{
		var = vsfvm_get_func_argu_ref(thread, i + 1);
		size = 1;
		for (uint8_t j = i + 1; j < arr->dimension; j++)
			size *= arr->dim_size[j];
		pos += size * var->uval8;
		allsize *= arr->dim_size[i];
	}
	for (uint8_t i = 1 + arr->dimension; i < thread->func.argc; i++)
	{
		var = vsfvm_get_func_argu_ref(thread, i);
		if (pos >= allsize) return VSFVM_RET_ERROR;
		switch (arr->ele_size)
		{
		case 1: arr->buf8[pos++] = var->uval8; break;
		case 2: arr->buf16[pos++] = var->uval16; break;
		case 4: arr->buf32[pos++] = var->uval32; break;
		}
	}
	return VSFVM_RET_FINISHED;
}

static void vsfvm_ext_array_print(struct vsfvm_var_t *var)
{
	
}

enum vsfvm_stdext_buffer_endian_t
{
	VSFVM_STDEXT_BUFFER_BE = 0,
	VSFVM_STDEXT_BUFFER_LE,
};

static enum vsfvm_ret_t vsfvm_ext_buffer_create(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *size = vsfvm_get_func_argu_ref(thread, 0);
	uint8_t *buffer;

	if (vsfvm_instance_alloc(result, size->uval32, &vsfvm_ext_buffer))
		return VSFVM_RET_ERROR;
	buffer = result->inst->buffer.buffer;
	memset(buffer, 0, size->uval32);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_buffer_get(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *result = vsfvm_get_func_argu(thread, 0);
	struct vsfvm_var_t *thiz = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *endian = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfvm_var_t *pos = vsfvm_get_func_argu_ref(thread, 2);
	struct vsfvm_var_t *size = vsfvm_get_func_argu_ref(thread, 3);
	enum vsfvm_stdext_buffer_endian_t e = (enum vsfvm_stdext_buffer_endian_t)endian->value;
	uint8_t *buffer, ele_size = size->uval8, p = pos->uval32;
	uint32_t value = 0;

	if (!thiz || !thiz->inst ||
		((e != VSFVM_STDEXT_BUFFER_BE) && (e != VSFVM_STDEXT_BUFFER_LE)) ||
		(ele_size > 4))
	{
		return VSFVM_RET_ERROR;
	}
	buffer = thiz->inst->buffer.buffer;
	for (uint8_t i = 0; i < ele_size; i++)
	{
		switch (e)
		{
		case VSFVM_STDEXT_BUFFER_BE:
			value <<= 8;
			value += buffer[p++];
			break;
		case VSFVM_STDEXT_BUFFER_LE:
			value += buffer[p++] << (i << 3);
			break;
		}
	}

	vsfvm_instance_deref(thread, thiz);
	result->uval32 = value;
	result->type = VSFVM_VAR_TYPE_VALUE;
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_buffer_set(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *thiz = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *endian = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfvm_var_t *pos = vsfvm_get_func_argu_ref(thread, 2);
	struct vsfvm_var_t *size = vsfvm_get_func_argu_ref(thread, 3);
	struct vsfvm_var_t *value = vsfvm_get_func_argu_ref(thread, 4);
	enum vsfvm_stdext_buffer_endian_t e = (enum vsfvm_stdext_buffer_endian_t)endian->value;
	uint8_t *buffer, ele_size = size->uval8;
	uint32_t v = value->uval32, p = pos->uval32;

	if (!thiz || !thiz->inst ||
		((e != VSFVM_STDEXT_BUFFER_BE) && (e != VSFVM_STDEXT_BUFFER_LE)) ||
		(ele_size > 4))
	{
		return VSFVM_RET_ERROR;
	}

	buffer = thiz->inst->buffer.buffer;
	for (uint8_t i = 0; i < ele_size; i++)
	{
		switch (e)
		{
		case VSFVM_STDEXT_BUFFER_BE:
			buffer[p++] = (v >> (((ele_size - i - 1) << 3))) & 0xFF;
			break;
		case VSFVM_STDEXT_BUFFER_LE:
			buffer[p++] = v & 0xFF;
			v >>= 8;
			break;
		}
	}

	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_buffer_memset(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *thiz = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *value = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfvm_var_t *pos = vsfvm_get_func_argu_ref(thread, 2);
	struct vsfvm_var_t *size = vsfvm_get_func_argu_ref(thread, 3);
	uint8_t *buffer, v = value->uval8;
	uint32_t p = pos->uval32, s = size->uval32;

	if (!thiz || !thiz->inst)
		return VSFVM_RET_ERROR;

	buffer = thiz->inst->buffer.buffer;
	memset(&buffer[p], v, s);
	return VSFVM_RET_FINISHED;
}

static enum vsfvm_ret_t vsfvm_ext_buffer_memcpy(struct vsfvm_thread_t *thread)
{
	struct vsfvm_var_t *dst = vsfvm_get_func_argu_ref(thread, 0);
	struct vsfvm_var_t *dst_pos = vsfvm_get_func_argu_ref(thread, 1);
	struct vsfvm_var_t *src = vsfvm_get_func_argu_ref(thread, 2);
	struct vsfvm_var_t *src_pos = vsfvm_get_func_argu_ref(thread, 3);
	struct vsfvm_var_t *size = vsfvm_get_func_argu_ref(thread, 4);
	uint8_t *dstb, *srcb;
	uint32_t dstp = dst_pos->uval32, srcp = src_pos->uval32, s = size->uval32;

	if (!src || !src->inst || !dst || !dst->inst)
		return VSFVM_RET_ERROR;

	dstb = dst->inst->buffer.buffer;
	srcb = src->inst->buffer.buffer;
	memcpy(&dstb[dstp], &srcb[srcp], s);
	return VSFVM_RET_FINISHED;
}

static void vsfvm_ext_string_print(struct vsfvm_var_t *var)
{
	char *str = (char *)var->inst->buffer.buffer;
	vsfdbg_prints(str);
}
#endif

const struct vsfvm_class_t vsfvm_ext_array =
{
#ifdef VSFVM_COMPILER
	.name = "array",
#endif
#ifdef VSFVM_VM
	.type = VSFVM_CLASS_ARRAY,
	.op.print = vsfvm_ext_array_print,
#endif
};
const struct vsfvm_class_t vsfvm_ext_buffer =
{
#ifdef VSFVM_COMPILER
	.name = "buffer",
#endif
#ifdef VSFVM_VM
	.type = VSFVM_CLASS_BUFFER,
#endif
};
const struct vsfvm_class_t vsfvm_ext_string =
{
#ifdef VSFVM_COMPILER
	.name = "string",
#endif
#ifdef VSFVM_VM
	.type = VSFVM_CLASS_STRING,
	.op.print = vsfvm_ext_string_print,
#endif
};

#ifdef VSFVM_COMPILER
extern const struct vsfvm_ext_op_t vsfvm_ext_std;
static const struct vsfvmc_lexer_sym_t vsfvm_ext_stdsym[] =
{
	VSFVM_LEXERSYM_CONST("true", &vsfvm_ext_std, NULL, 1),
	VSFVM_LEXERSYM_CONST("false", &vsfvm_ext_std, NULL, 0),

	VSFVM_LEXERSYM_CLASS("array", &vsfvm_ext_std, &vsfvm_ext_array),
	VSFVM_LEXERSYM_EXTFUNC("print", &vsfvm_ext_std, NULL, NULL, -1, 0),
	VSFVM_LEXERSYM_EXTFUNC("array_create", &vsfvm_ext_std, NULL, &vsfvm_ext_array, -1, 1),
	VSFVM_LEXERSYM_EXTFUNC("array_get", &vsfvm_ext_std, &vsfvm_ext_array, NULL, -1, 2),
	VSFVM_LEXERSYM_EXTFUNC("array_set", &vsfvm_ext_std, &vsfvm_ext_array, &vsfvm_ext_array, -1, 3),

	VSFVM_LEXERSYM_CLASS("buffer", &vsfvm_ext_std, &vsfvm_ext_buffer),
	VSFVM_LEXERSYM_CONST("BUFFER_BE", &vsfvm_ext_std, &vsfvm_ext_buffer, VSFVM_STDEXT_BUFFER_BE),
	VSFVM_LEXERSYM_CONST("BUFFER_LE", &vsfvm_ext_std, &vsfvm_ext_buffer, VSFVM_STDEXT_BUFFER_LE),
	VSFVM_LEXERSYM_EXTFUNC("buffer_create", &vsfvm_ext_std, NULL, &vsfvm_ext_buffer, 1, 4),
	VSFVM_LEXERSYM_EXTFUNC("buffer_get", &vsfvm_ext_std, &vsfvm_ext_buffer, NULL, 4, 5),
	VSFVM_LEXERSYM_EXTFUNC("buffer_set", &vsfvm_ext_std, &vsfvm_ext_buffer, &vsfvm_ext_buffer, 5, 6),
	VSFVM_LEXERSYM_EXTFUNC("buffer_memset", &vsfvm_ext_std, &vsfvm_ext_buffer, &vsfvm_ext_buffer, 4, 7),
	VSFVM_LEXERSYM_EXTFUNC("buffer_memcpy", &vsfvm_ext_std, &vsfvm_ext_buffer, &vsfvm_ext_buffer, 5, 8),

	VSFVM_LEXERSYM_CLASS("string", &vsfvm_ext_std, &vsfvm_ext_string),
};
#endif

#ifdef VSFVM_VM
static const struct vsfvm_extfunc_t vsfvm_ext_stdfunc[] =
{
	VSFVM_EXTFUNC("print", vsfvm_ext_print, -1),
	// array class
	VSFVM_EXTFUNC("array_create", vsfvm_ext_array_create, -1),
	VSFVM_EXTFUNC("array_get", vsfvm_ext_array_get, -1),
	VSFVM_EXTFUNC("array_set", vsfvm_ext_array_set, -1),
	VSFVM_EXTFUNC("buffer_create", vsfvm_ext_buffer_create, 1),
	VSFVM_EXTFUNC("buffer_get", vsfvm_ext_buffer_get, 4),
	VSFVM_EXTFUNC("buffer_set", vsfvm_ext_buffer_set, 5),
	VSFVM_EXTFUNC("buffer_memset", vsfvm_ext_buffer_memset, 4),
	VSFVM_EXTFUNC("buffer_memcpy", vsfvm_ext_buffer_memcpy, 5),
};
#endif

const struct vsfvm_ext_op_t vsfvm_ext_std =
{
#ifdef VSFVM_COMPILER
	.name = "std",
	.sym = vsfvm_ext_stdsym,
	.sym_num = dimof(vsfvm_ext_stdsym),
#endif
#ifdef VSFVM_VM
	.init = NULL,
	.fini = NULL,
	.func = (struct vsfvm_extfunc_t *)vsfvm_ext_stdfunc,
#endif
	.func_num = dimof(vsfvm_ext_stdfunc),
};
