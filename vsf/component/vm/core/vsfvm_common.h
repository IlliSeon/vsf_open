#ifndef __VSFVM_COMMON_H_INCLUDED__
#define __VSFVM_COMMON_H_INCLUDED__

#include "app_cfg.h"
#ifdef VSFVM_COMPILER
#include "vsfvm_compiler.h"
#include "lexer/vsfvm_lexer.h"
#endif

#ifndef VSFVM_LOG_INFO
#define VSFVM_LOG_INFO(format, ...)
#endif

#ifndef VSFVM_LOG_ERROR
#define VSFVM_LOG_ERROR(format, ...)
#endif

enum vsfvm_var_type_t
{
	VSFVM_VAR_TYPE_VALUE = 0,
	VSFVM_VAR_TYPE_RESOURCES,
	VSFVM_VAR_TYPE_REFERENCE,
	VSFVM_VAR_TYPE_FUNCTION,
	VSFVM_VAR_TYPE_INSTANCE,
};

struct vsfvm_class_t;
struct vsfvm_instance_t
{
	struct vsf_buffer_t buffer;
	uint32_t ref;
	const struct vsfvm_class_t *c;
};

struct vsfvm_var_t
{
	union
	{
		int value;
		int32_t ival;
		uint32_t uval;
		int32_t ival32;
		uint32_t uval32;
		int16_t ival16;
		uint16_t uval16;
		int8_t ival8;
		uint8_t uval8;
		float f;
		struct vsfvm_instance_t *inst;
	};
	enum vsfvm_var_type_t type;
};

#ifdef VSFVM_VM
#define VSFVM_EXTFUNC(name, handler, argu_num)			{(handler), (argu_num)}
#endif

#ifdef VSFVM_COMPILER
#define VSFVM_LEXERSYM_EXTFUNC(n, o, cl, retcl, p, po)	{.name = (n), .op = (o), .func.param_num = (p), .func.retc = (retcl), .func.pos = (po), .c = (cl), .type = VSFVMC_LEXER_SYM_EXTFUNC}
#define VSFVM_LEXERSYM_CLASS(n, o, cl)					{.name = (n), .op = (o), .c = (cl), .type = VSFVMC_LEXER_SYM_EXTCLASS,}
#define VSFVM_LEXERSYM_CONST(n, o, cl, v)				{.name = (n), .op = (o), .ival = (v), .c = (cl), .type = VSFVMC_LEXER_SYM_CONST,}
#define VSFVM_LEXERSYM_KEYWORKD(n)						{.name = (n), .type = VSFVMC_LEXER_SYM_KEYWORD,}
#define VSFVM_LEXERSYM_FUNCTION(n, p)					{.name = (n), .func.param_num = (p), .type = VSFVMC_LEXER_SYM_FUNCTION}
#endif

#if defined(VSFVM_VM)
struct vsfvm_t;
struct vsfvm_thread_t;
struct vsfvm_script_t;
#endif

enum vsfvm_class_type_t
{
	VSFVM_CLASS_USER,
	VSFVM_CLASS_STRING,
	VSFVM_CLASS_BUFFER,
	VSFVM_CLASS_ARRAY,
};

struct vsfvm_class_op_t
{
	void (*print)(struct vsfvm_var_t *var);
	void (*destroy)(struct vsfvm_var_t *var);
};

struct vsfvm_class_t
{
#ifdef VSFVM_COMPILER
	char *name;
#endif
#ifdef VSFVM_VM
	enum vsfvm_class_type_t type;
	struct vsfvm_class_op_t op;
#endif
};

typedef enum vsfvm_ret_t(*vsfvm_extfunc_handler_t)(struct vsfvm_thread_t *thread);

#ifdef VSFVM_VM
struct vsfvm_extfunc_t
{
	vsfvm_extfunc_handler_t handler;
	// if argu_num is -1, use dynamic paramter
	int16_t argu_num;
};
#endif

struct vsfvm_ext_op_t
{
#ifdef VSFVM_COMPILER
	char *name;
#endif
#ifdef VSFVM_VM
	int (*init)(struct vsfvm_t *vm, struct vsfvm_script_t *script);
	int (*fini)(struct vsfvm_t *vm, struct vsfvm_script_t *script);
	const struct vsfvm_extfunc_t *func;
#endif
#ifdef VSFVM_COMPILER
	const struct vsfvmc_lexer_sym_t *sym;
	uint32_t sym_num;
#endif
	uint32_t func_num;
};

struct vsfvm_ext_t
{
	const struct vsfvm_ext_op_t *op;
#ifdef VSFVM_COMPILER
	struct vsfvmc_lexer_symarr_t symarr;
	uint16_t func_id;
#endif
	struct vsflist_t list;
};

int vsfvm_ext_pool_init(uint16_t pool_size);
void vsfvm_ext_pool_fini(void);
struct vsfvm_ext_t *vsfvm_alloc_ext(void);
void vsfvm_free_ext(struct vsfvm_ext_t *ext);
void vsfvm_free_extlist(struct vsflist_t *list);

#endif		// __VSFVM_COMMON_H_INCLUDED__
