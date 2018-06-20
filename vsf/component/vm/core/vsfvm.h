#ifndef __VSFVM_H_INCLUDED__
#define __VSFVM_H_INCLUDED__

#include "vsf.h"
#include "vsfvm_code.h"
#include "vsfvm_common.h"

#define VSFVM_CFG_DYNARR_ITEM_BITLEN	4
#define VSFVM_CFG_DYNARR_TABLE_BITLEN	4

struct vsfvm_t;
struct vsfvm_script_t;

enum vsfvm_ret_t
{
	VSFVM_RET_STACK_FAIL = -3,
	VSFVM_RET_DIV0 = -2,
	VSFVM_RET_ERROR = -1,
	VSFVM_RET_FINISHED = 0,
	VSFVM_RET_PEND,
	VSFVM_RET_GOON,
};

struct vsfvm_thread_t
{
	struct vsfsm_t sm;
	struct vsfsm_pt_t pt;

	struct vsfvm_t *vm;
	struct vsfvm_script_t *script;

	struct vsfvm_func_t
	{
		uint8_t argc;

		enum VSFVM_CODE_FUNCTION_ID_t type;
		union
		{
			vsfvm_extfunc_handler_t handler;
			uint32_t pc;
		};

		uint32_t arg_reg;
		uint32_t auto_reg;
		uint32_t expression_sp;
	} func;
	struct vsf_dynstack_t stack;
	uint32_t max_sp;
	struct vsflist_t rdylist;
	struct vsflist_t list;
};

struct vsfvm_script_t
{
	const uint32_t *token;
	uint32_t token_num;
	struct
	{
		uint8_t argc;
		struct vsfvm_var_t *arg;
	} param;

	// private
	struct vsf_dynstack_t *lvar;
	uint32_t lvar_pos;

	enum
	{
		VSFVM_SCRIPTSTAT_UNKNOWN,
		VSFVM_SCRIPTSTAT_RUNNING,
		VSFVM_SCRIPTSTAT_ERROR,
		VSFVM_SCRIPTSTAT_FINING,
		VSFVM_SCRIPTSTAT_FINIED,
	} state;

	struct vsflist_t thread;
	struct vsflist_t list;
};

struct vsfvm_t
{
	struct vsf_dynpool_t thread_pool;

	// private
	struct vsflist_t script;
	struct vsflist_t ext;

	struct vsflist_t appendlist;
	struct vsflist_t rdylist;
};

int vsfvm_register_ext(struct vsfvm_t *vm, const struct vsfvm_ext_op_t *op);
int vsfvm_init(struct vsfvm_t *vm);
int vsfvm_fini(struct vsfvm_t *vm);
int vsfvm_poll(struct vsfvm_t *vm);
int vsfvm_gc(struct vsfvm_t *vm);

int vsfvm_script_init(struct vsfvm_t *vm, struct vsfvm_script_t *script);
int vsfvm_script_fini(struct vsfvm_t *vm, struct vsfvm_script_t *script);

// for extfunc
void vsfvm_thread_ready(struct vsfvm_thread_t *thread);
struct vsfvm_var_t *vsfvm_get_func_argu(struct vsfvm_thread_t *thread,
	uint8_t idx);
struct vsfvm_var_t *vsfvm_get_func_argu_ref(struct vsfvm_thread_t *thread,
	uint8_t idx);
struct vsfvm_var_t *vsfvm_thread_stack_get(struct vsfvm_thread_t *thread,
	uint32_t offset);
int vsfvm_thread_stack_push(struct vsfvm_thread_t *thread, int32_t value,
	enum vsfvm_var_type_t type, uint32_t num);
struct vsfvm_var_t *vsfvm_thread_stack_pop(struct vsfvm_thread_t *thread,
	uint32_t num);

struct vsfvm_var_t *vsfvm_get_ref(struct vsfvm_thread_t *thread,
	struct vsfvm_var_t *var);
int vsfvm_get_res(struct vsfvm_script_t *script, uint32_t offset,
	struct vsf_buffer_t *buf);

enum vsfvm_ret_t vsfvm_instance_alloc(struct vsfvm_var_t *var, uint32_t size,
	const struct vsfvm_class_t *c);
enum vsfvm_ret_t vsfvm_instance_free(struct vsfvm_var_t *var);
void vsfvm_instance_ref(struct vsfvm_thread_t *thread, struct vsfvm_var_t *var);
void vsfvm_instance_deref(struct vsfvm_thread_t *thread, struct vsfvm_var_t *var);

struct vsfvm_thread_t * vsfvm_thread_init(struct vsfvm_t *vm,
	struct vsfvm_script_t *script, uint16_t start_pos, uint8_t argc,
	struct vsfvm_thread_t *orig_thread, struct vsfvm_var_t *arg);
enum vsfvm_ret_t vsfvm_thread_run(struct vsfvm_t *vm,
	struct vsfvm_thread_t *thread);

#endif	// __VSFVM_H_INCLUDED__
