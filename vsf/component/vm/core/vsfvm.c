
#include "vsfvm.h"
#include "vsfvm_objdump.h"

static struct vsfvm_thread_t *vsfvm_alloc_thread(struct vsfvm_t *vm)
{
	return (struct vsfvm_thread_t *)vsf_dynpool_alloc(&vm->thread_pool);
}

static void vsfvm_free_thread(struct vsfvm_t *vm, struct vsfvm_thread_t *thread)
{
	vsf_dynpool_free(&vm->thread_pool, thread);
}

static uint32_t vsfvm_get_token(struct vsfvm_script_t *script,
	struct vsfvm_thread_t *thread)
{
	if (thread->func.pc < script->token_num)
	{
#ifdef VSFVM_VM_DEBUG
		uint32_t token = script->token[thread->func.pc++];
		VSFVM_LOG_INFO("%d:", thread->func.pc - 1);
		vsfvm_tkdump(token);
		return token;
#else
		return script->token[thread->func.pc++];
#endif
	}
	return VSFVM_CODE(VSFVM_CODE_TYPE_EOF, 0);
}

int vsfvm_get_res(struct vsfvm_script_t *script, uint32_t offset,
	struct vsf_buffer_t *buf)
{
	if (offset < script->token_num)
	{
		buf->size = script->token[offset] & 0xFFFF;
		buf->buffer = (uint8_t *)&script->token[offset + 1];
		return 0;
	}
	return -1;
}

static const struct vsfvm_extfunc_t *
vsfvm_get_extfunc(struct vsfvm_t *vm, uint16_t id)
{
	vsflist_foreach(ext, vm->ext.next, struct vsfvm_ext_t, list)
	{
		if (id < ext->op->func_num)
			return &ext->op->func[id];
		id -= ext->op->func_num;
	}
	return NULL;
}

static struct vsfvm_var_t *vsfvm_get_var(struct vsfvm_thread_t *thread,
	enum VSFVM_CODE_VARIABLE_POS_t pos, uint16_t id)
{
	struct vsfvm_script_t *script = thread->script;
	struct vsf_dynarr_t *dynarr;

	switch (pos)
	{
	case VSFVM_CODE_VARIABLE_POS_LOCAL:
		id += script->lvar_pos;
		if (id >= script->lvar->sp)
			return NULL;
		dynarr = &script->lvar->var;
		break;
	case VSFVM_CODE_VARIABLE_POS_STACK_BEGIN:
		dynarr = &thread->stack.var;
		if (id >= thread->stack.sp)
			return NULL;
		break;
	case VSFVM_CODE_VARIABLE_POS_STACK_END:
		return vsf_dynstack_get(&thread->stack, id);
	case VSFVM_CODE_VARIABLE_POS_FUNCARG:
		dynarr = &thread->stack.var;
		id = thread->func.arg_reg + id;
		if (id >= thread->stack.sp)
			return NULL;
		break;
	case VSFVM_CODE_VARIABLE_POS_FUNCAUTO:
		dynarr = &thread->stack.var;
		id = thread->func.auto_reg + id;
		if (id >= thread->stack.sp)
			return NULL;
		break;
	default: return NULL;
	}
	return (struct vsfvm_var_t *)vsf_dynarr_get(dynarr, id);
}

struct vsfvm_var_t *vsfvm_get_func_argu(struct vsfvm_thread_t *thread,
	uint8_t idx)
{
	if (thread->func.argc <= idx) return NULL;
	return vsfvm_get_var(thread, VSFVM_CODE_VARIABLE_POS_FUNCARG, idx);
}

struct vsfvm_var_t *vsfvm_get_func_argu_ref(struct vsfvm_thread_t *thread,
	uint8_t idx)
{
	struct vsfvm_var_t *var = vsfvm_get_func_argu(thread, idx);
	if ((var != NULL) && (var->type == VSFVM_VAR_TYPE_REFERENCE))
		var = vsfvm_get_ref(thread, var);
	return var;
}

enum vsfvm_ret_t vsfvm_instance_alloc(struct vsfvm_var_t *var, uint32_t size,
	const struct vsfvm_class_t *c)
{
	var->type = VSFVM_VAR_TYPE_INSTANCE;
	var->inst = vsf_bufmgr_malloc(sizeof(struct vsfvm_instance_t) + size);
	if (!var->inst) return VSFVM_RET_ERROR;
#ifdef VSFVM_VM_DEBUG
	vsfdbg_printf("alloc instance %04X" VSFVM_LOG_LINEEND, var->inst);
#endif
	memset(var->inst, 0, sizeof(struct vsfvm_instance_t) + size);

	var->inst->buffer.buffer = (uint8_t *)&var->inst[1];
	var->inst->buffer.size = size;
	var->inst->ref = 1;
	var->inst->c = c;
	return VSFVM_RET_FINISHED;
}

enum vsfvm_ret_t vsfvm_instance_free(struct vsfvm_var_t *var)
{
	if (var->type != VSFVM_VAR_TYPE_INSTANCE)
		return VSFVM_RET_ERROR;

	if (var->inst != NULL)
	{
#ifdef VSFVM_VM_DEBUG
		vsfdbg_printf("free instance %04X" VSFVM_LOG_LINEEND, var->inst);
#endif
		if ((var->inst->c->op.destroy != NULL))
			var->inst->c->op.destroy(var);
		vsf_bufmgr_free(var->inst);
		var->inst = NULL;
	}
	return VSFVM_RET_FINISHED;
}

void vsfvm_instance_ref(struct vsfvm_thread_t *thread, struct vsfvm_var_t *var)
{
	if (var->type == VSFVM_VAR_TYPE_REFERENCE)
		var = vsfvm_get_ref(thread, var);

	if ((var->type == VSFVM_VAR_TYPE_INSTANCE) && (var->inst != NULL))
	{
		var->inst->ref++;
#ifdef VSFVM_VM_DEBUG
		VSFVM_LOG_INFO("var 0x%08X reference %d" VSFVM_LOG_LINEEND,
			var->inst, var->inst->ref);
#endif
	}
}

void vsfvm_instance_deref(struct vsfvm_thread_t *thread, struct vsfvm_var_t *var)
{
	if (var->type == VSFVM_VAR_TYPE_REFERENCE)
		var = vsfvm_get_ref(thread, var);

	if ((var->type == VSFVM_VAR_TYPE_INSTANCE) && (var->inst != NULL))
	{
		var->inst->ref--;
#ifdef VSFVM_VM_DEBUG
		VSFVM_LOG_INFO("var 0x%08X reference %d" VSFVM_LOG_LINEEND,
			var->inst, var->inst->ref);
#endif
		if (!var->inst->ref)
			vsfvm_instance_free(var);
	}
}

struct vsfvm_var_t *vsfvm_thread_stack_pop(struct vsfvm_thread_t *thread,
	uint32_t num)
{
#ifdef VSFVM_VM_DEBUG
	struct vsfvm_var_t *var = vsf_dynstack_pop(&thread->stack, num);
	if (num)
		VSFVM_LOG_INFO("pop stack = %d" VSFVM_LOG_LINEEND, thread->stack.sp);
	return var;
#else
	return vsf_dynstack_pop(&thread->stack, num);
#endif
}

static int vsfvm_thread_stack_pop_and_free(struct vsfvm_thread_t *thread,
	uint32_t num)
{
	struct vsfvm_var_t *var;

	while (num-- > 0)
	{
		var = vsfvm_thread_stack_pop(thread, 1);
		if (!var) return -1;
		vsfvm_instance_deref(thread, var);
	}
	return 0;
}

struct vsfvm_var_t *vsfvm_thread_stack_get(struct vsfvm_thread_t *thread,
	uint32_t offset)
{
	return vsf_dynstack_get(&thread->stack, offset);
}

int vsfvm_thread_stack_push(struct vsfvm_thread_t *thread, int32_t value,
	enum vsfvm_var_type_t type, uint32_t num)
{
	struct vsfvm_var_t var = { value, type };
	int err = vsf_dynstack_push(&thread->stack, &var, num);
	if (!err)
	{
		if (thread->max_sp < thread->stack.sp)
			thread->max_sp = thread->stack.sp;
#ifdef VSFVM_VM_DEBUG
		VSFVM_LOG_INFO("push stack = %d" VSFVM_LOG_LINEEND, thread->stack.sp);
#endif
	}
	return err;
}

static int vsfvm_thread_stack_push_ref(struct vsfvm_thread_t *thread,
	int32_t value, enum vsfvm_var_type_t type, uint32_t num)
{
	enum VSFVM_CODE_VARIABLE_POS_t pos =
		(enum VSFVM_CODE_VARIABLE_POS_t)((value >> 16) & 0xFF);
	uint16_t id = value & 0xFFFF;

	switch (pos)
	{
	case VSFVM_CODE_VARIABLE_POS_STACK_END:
		id = thread->stack.sp - id - 1;
		value = (VSFVM_CODE_VARIABLE_POS_STACK_BEGIN << 16) | id;
		break;
	case VSFVM_CODE_VARIABLE_POS_FUNCARG:
		id = thread->func.arg_reg + id;
		value = (VSFVM_CODE_VARIABLE_POS_STACK_BEGIN << 16) | id;
		break;
	case VSFVM_CODE_VARIABLE_POS_FUNCAUTO:
		id = thread->func.auto_reg + id;
		value = (VSFVM_CODE_VARIABLE_POS_STACK_BEGIN << 16) | id;
		break;
	}
	return vsfvm_thread_stack_push(thread, value, type, num);
}

struct vsfvm_var_t *vsfvm_get_ref(struct vsfvm_thread_t *thread,
	struct vsfvm_var_t *var)
{
	enum VSFVM_CODE_VARIABLE_POS_t pos =
		(enum VSFVM_CODE_VARIABLE_POS_t)((var->value >> 16) & 0xFF);
	uint16_t id = var->value & 0xFFFF;
	return vsfvm_get_var(thread, pos, id);
}

static int vsfvm_push_func(struct vsfvm_thread_t *thread)
{
	int err = vsf_dynstack_push_ext(&thread->stack, &thread->func,
		sizeof(thread->func));
	if (!err)
	{
		if (thread->max_sp < thread->stack.sp)
			thread->max_sp = thread->stack.sp;
#ifdef VSFVM_VM_DEBUG
		VSFVM_LOG_INFO("push func ctx = %d" VSFVM_LOG_LINEEND,
			thread->stack.sp);
#endif
	}
	return err;
}

static int vsfvm_pop_func(struct vsfvm_thread_t *thread)
{
	struct vsfvm_func_t func;

	if (vsf_dynstack_pop_ext(&thread->stack, &func, sizeof(func)) < 0)
		return -1;
#ifdef VSFVM_VM_DEBUG
	VSFVM_LOG_INFO("pop func ctx = %d" VSFVM_LOG_LINEEND, thread->stack.sp);
#endif
	thread->func = func;
	return 0;
}

static void vsfvm_thread_fini(struct vsfvm_t *vm, struct vsfvm_thread_t *thread)
{
	struct vsfvm_script_t *script = thread->script;
	struct vsfvm_var_t *var;

	do {
		var = vsfvm_thread_stack_pop(thread, 1);
		if (var != NULL) vsfvm_instance_deref(thread, var);
	} while (var != NULL);
	vsf_dynstack_fini(&thread->stack);
	vsflist_remove(&script->thread.next, &thread->list);
	vsfsm_fini(&thread->sm);
}

enum vsfvm_ret_t vsfvm_thread_run(struct vsfvm_t *vm,
	struct vsfvm_thread_t *thread)
{
	struct vsfvm_script_t *script = thread->script;
	struct vsfvm_func_t *func = &thread->func;
	struct vsfvm_var_t *arg1, *arg2, *var, *result;
	enum vsfvm_ret_t ret;
	uint32_t token;
	uint16_t arg16;
	uint8_t id, arg8;
	uint32_t keyword;

	while (1)
	{
		if (func->type == VSFVM_CODE_FUNCTION_EXT)
		{
			ret = func->handler(thread);
			if ((int)ret < 0) return ret;
			switch (ret)
			{
			case VSFVM_RET_PEND:		return ret;
			case VSFVM_RET_FINISHED:	goto do_return;
			case VSFVM_RET_GOON:		break;
			}
			continue;
		}

		token = vsfvm_get_token(script, thread);
		keyword = VSFVM_CODE_KEYWORD(token);
		id = VSFVM_CODE_ID(token);
		arg8 = VSFVM_CODE_ARG8(token);
		arg16 = VSFVM_CODE_ARG16(token);
		if (keyword == VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_breakpoint, 0, 0))
		{
			thread->func.pc--;
			return VSFVM_RET_PEND;
		}
		else if (func->expression_sp)
		{
		do_expr:
			if (keyword == VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SEMICOLON, 0, 0))
			{
				if (thread->stack.sp != (thread->func.expression_sp + 1))
					return VSFVM_RET_ERROR;
				thread->func.expression_sp = 0;
				switch ((enum VSFVM_CODE_SYBMOL_SEMICOLIN_ID_t)arg8)
				{
				case VSFVM_CODE_SYMBOL_SEMICOLON_POP:
					vsfvm_thread_stack_pop_and_free(thread, 1);
#ifdef VSFVM_VM_DEBUG
					VSFVM_LOG_INFO("expr end stack = %d" VSFVM_LOG_LINEEND,
						thread->stack.sp);
#endif
				case VSFVM_CODE_SYMBOL_SEMICOLON_NOPOP:
					continue;
				default:
					return VSFVM_RET_ERROR;
				}
			}
			else if (VSFVM_CODE_TYPE(token) == VSFVM_CODE_TYPE_SYMBOL)
			{
				if (id > VSFVM_CODE_SYMBOL_ASSIGN)
					return VSFVM_RET_ERROR;

				if (id < VSFVM_CODE_SYMBOL_POSI)
				{
					arg1 = result = vsfvm_thread_stack_get(thread, 0);
					if (!arg1) return VSFVM_RET_ERROR;
					if (arg1->type == VSFVM_VAR_TYPE_RESOURCES)
					{
						arg1 = vsfvm_get_ref(thread, arg1);
						if (!arg1) return VSFVM_RET_ERROR;
					}
					else if (arg1->type == VSFVM_VAR_TYPE_RESOURCES)
						return VSFVM_RET_ERROR;

					result->type = VSFVM_VAR_TYPE_VALUE;
					switch (id)
					{
					case VSFVM_CODE_SYMBOL_NOT: result->value = !arg1->value; break;
					case VSFVM_CODE_SYMBOL_REV: result->value = ~arg1->value; break;
					case VSFVM_CODE_SYMBOL_NEGA: result->value = -arg1->value; break;
					case VSFVM_CODE_SYMBOL_POSI: result->value = arg1->value; break;
					default: return VSFVM_RET_ERROR;
					}
				}
				else
				{
					arg2 = vsfvm_thread_stack_pop(thread, 1);
					if (!arg2) return VSFVM_RET_ERROR;
					if (arg2->type == VSFVM_VAR_TYPE_REFERENCE)
					{
						arg2 = vsfvm_get_ref(thread, arg2);
						if (!arg2) return VSFVM_RET_ERROR;
					}
					else if (arg2->type == VSFVM_VAR_TYPE_RESOURCES)
						return VSFVM_RET_ERROR;

					arg1 = result = vsfvm_thread_stack_get(thread, 0);
					if (!arg1) return VSFVM_RET_ERROR;
					if (id != VSFVM_CODE_SYMBOL_ASSIGN)
					{
						if (arg1->type == VSFVM_VAR_TYPE_REFERENCE)
						{
							arg1 = vsfvm_get_ref(thread, arg1);
							if (!arg1) return VSFVM_RET_ERROR;
						}
						else if (arg1->type == VSFVM_VAR_TYPE_RESOURCES)
							return VSFVM_RET_ERROR;
					}
					else if (arg1->type != VSFVM_VAR_TYPE_REFERENCE)
						return VSFVM_RET_ERROR;

					vsfvm_instance_deref(thread, result);
					switch (id)
					{
					case VSFVM_CODE_SYMBOL_MUL:		result->value = arg1->value * arg2->value; break;
					case VSFVM_CODE_SYMBOL_DIV:
						if (!arg2->value) return VSFVM_RET_DIV0;
						result->value = arg1->value / arg2->value;
						break;
					case VSFVM_CODE_SYMBOL_MOD:
						if (!arg2->value) return VSFVM_RET_DIV0;
						result->value = arg1->value % arg2->value;
						break;
					case VSFVM_CODE_SYMBOL_ADD:		result->value = arg1->value + arg2->value; break;
					case VSFVM_CODE_SYMBOL_SUB:		result->value = arg1->value - arg2->value; break;
					case VSFVM_CODE_SYMBOL_SHL:		result->value = arg1->value << arg2->value; break;
					case VSFVM_CODE_SYMBOL_SHR:		result->value = arg1->value >> arg2->value; break;
					case VSFVM_CODE_SYMBOL_AND:		result->value = arg1->value & arg2->value; break;
					case VSFVM_CODE_SYMBOL_OR:		result->value = arg1->value | arg2->value; break;
					case VSFVM_CODE_SYMBOL_XOR:		result->value = arg1->value ^ arg2->value; break;
					case VSFVM_CODE_SYMBOL_EQ:		result->value = arg1->value == arg2->value; break;
					case VSFVM_CODE_SYMBOL_NE:		result->value = arg1->value != arg2->value; break;
					case VSFVM_CODE_SYMBOL_GT:		result->value = arg1->value > arg2->value; break;
					case VSFVM_CODE_SYMBOL_GE:		result->value = arg1->value >= arg2->value; break;
					case VSFVM_CODE_SYMBOL_LT:		result->value = arg1->value < arg2->value; break;
					case VSFVM_CODE_SYMBOL_LE:		result->value = arg1->value <= arg2->value; break;
					case VSFVM_CODE_SYMBOL_LAND:	result->value = arg1->value && arg2->value; break;
					case VSFVM_CODE_SYMBOL_LOR:		result->value = arg1->value || arg2->value; break;
					case VSFVM_CODE_SYMBOL_LXOR:	result->value = !!arg1->value != !!arg2->value; break;
					case VSFVM_CODE_SYMBOL_COMMA:	result->value = arg2->value; break;
					case VSFVM_CODE_SYMBOL_ASSIGN:
						var = vsfvm_get_ref(thread, arg1);
						if (!var) return VSFVM_RET_ERROR;

						vsfvm_instance_deref(thread, var);
						result->value = var->value = arg2->value;
						result->type = var->type = arg2->type;
						vsfvm_instance_ref(thread, var);
						vsfvm_instance_ref(thread, result);
						vsfvm_instance_deref(thread, arg2);
						continue;
					default:
						return VSFVM_RET_ERROR;
					}
					result->type = VSFVM_VAR_TYPE_VALUE;
					vsfvm_instance_deref(thread, arg2);
				}
				continue;
			}
			else if (VSFVM_CODE_TYPE(token) == VSFVM_CODE_TYPE_NUMBER)
			{
				if (vsfvm_thread_stack_push(thread, VSFVM_CODE_VALUE(token),
						VSFVM_VAR_TYPE_VALUE, 1))
				{
					return VSFVM_RET_STACK_FAIL;
				}
			}
			else if (VSFVM_CODE_TYPE(token) == VSFVM_CODE_TYPE_VARIABLE)
			{
				switch (id)
				{
				case VSFVM_CODE_VARIABLE_NORMAL:
					arg1 = vsfvm_get_var(thread,
						(enum VSFVM_CODE_VARIABLE_POS_t)arg8, arg16);
					if (!arg1) return VSFVM_RET_ERROR;
					if (arg1->type == VSFVM_VAR_TYPE_REFERENCE)
					{
						arg1 = vsfvm_get_ref(thread, arg1);
						if (!arg1) return VSFVM_RET_ERROR;
					}
					vsfvm_instance_ref(thread, arg1);
					if (vsfvm_thread_stack_push(thread, arg1->value, arg1->type, 1))
						return VSFVM_RET_STACK_FAIL;
					break;
				case VSFVM_CODE_VARIABLE_REFERENCE:
				case VSFVM_CODE_VARIABLE_REFERENCE_NOTRACE:
					arg1 = vsfvm_get_var(thread,
						(enum VSFVM_CODE_VARIABLE_POS_t)arg8, arg16);
					if (!arg1) return VSFVM_RET_ERROR;
					vsfvm_instance_ref(thread, arg1);
					if ((id != VSFVM_CODE_VARIABLE_REFERENCE_NOTRACE) &&
						(arg1->type == VSFVM_VAR_TYPE_REFERENCE))
					{
						if (vsfvm_thread_stack_push(thread, arg1->value,
								VSFVM_VAR_TYPE_REFERENCE, 1))
						{
							return VSFVM_RET_STACK_FAIL;
						}
					}
					else if (vsfvm_thread_stack_push_ref(thread,
							VSFVM_CODE_ARG24(token), VSFVM_VAR_TYPE_REFERENCE, 1))
					{
						return VSFVM_RET_STACK_FAIL;
					}
					break;
				case VSFVM_CODE_VARIABLE_RESOURCES:
					arg16 = func->pc + (int16_t)arg16;
					if (vsfvm_thread_stack_push(thread, arg16, VSFVM_VAR_TYPE_RESOURCES, 1))
						return VSFVM_RET_STACK_FAIL;
					break;
				case VSFVM_CODE_VARIABLE_FUNCTION:
					arg16 = func->pc + (int16_t)arg16;
					if (vsfvm_thread_stack_push(thread, (arg8 << 16) + arg16,
							VSFVM_VAR_TYPE_FUNCTION, 1))
					{
						return VSFVM_RET_STACK_FAIL;
					}
					break;
				}
			}
			else if (VSFVM_CODE_TYPE(token) == VSFVM_CODE_TYPE_FUNCTION)
			{
				const struct vsfvm_extfunc_t *ext;
				uint32_t sp = thread->stack.sp - arg8;

				// at least one arg, for result value
				if (!arg8)
				{
					arg8++;
					if (vsfvm_thread_stack_push(thread, 0, VSFVM_VAR_TYPE_VALUE, 1))
						return VSFVM_RET_STACK_FAIL;
				}

				if (vsfvm_push_func(thread))
					return VSFVM_RET_STACK_FAIL;

				func->argc = arg8;
				func->type = (enum VSFVM_CODE_FUNCTION_ID_t)id;
				func->arg_reg = sp;
				func->auto_reg = thread->stack.sp;
				func->expression_sp = 0;
				switch (func->type)
				{
				case VSFVM_CODE_FUNCTION_SCRIPT:
					func->pc += (int16_t)arg16;
					break;
				case VSFVM_CODE_FUNCTION_EXT:
					ext = vsfvm_get_extfunc(thread->vm, arg16);
					if (!ext) return VSFVM_RET_ERROR;
					func->handler = ext->handler;
					break;
				case VSFVM_CODE_FUNCTION_THREAD:
					vsfvm_pop_func(thread);
					if (!vsfvm_thread_init(vm, script, 0, arg8, thread, NULL))
						return VSFVM_RET_ERROR;
					break;
				default:
					return VSFVM_RET_ERROR;
				}
				continue;
			}
			else
				return VSFVM_RET_ERROR;
		}
		else if (VSFVM_CODE_TYPE(token) == VSFVM_CODE_TYPE_EOF)
			return VSFVM_RET_FINISHED;
		else if (keyword == VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_var, 0, 0))
		{
			if (vsfvm_thread_stack_push(thread, 0, (enum vsfvm_var_type_t)arg8, 1))
				return VSFVM_RET_STACK_FAIL;
#ifdef VSFVM_VM_DEBUG
			VSFVM_LOG_INFO("push var = %d" VSFVM_LOG_LINEEND, thread->stack.sp);
#endif
		}
		else if (keyword == VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_if, 0, 0))
		{
			var = vsfvm_thread_stack_pop(thread, 1);
			if (!var->value) goto do_goto;
		}
		else if (keyword == VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_goto, 0, 0))
		{
			vsfvm_thread_stack_pop_and_free(thread, arg8);
		do_goto:
			func->pc += (int16_t)arg16;
		}
		else if (keyword == VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_return, 0, 0))
		{
		do_return:
			vsfvm_thread_stack_pop_and_free(thread,
				thread->stack.sp - func->auto_reg);
			// reserve one arg as return value
			arg8 = func->argc - 1;
			vsfvm_pop_func(thread);
			vsfvm_thread_stack_pop_and_free(thread, arg8);
			if (!func->pc)
			{
				vsfvm_thread_fini(vm, thread);
				return VSFVM_RET_FINISHED;
			}
		}
		else
		{
			func->expression_sp = thread->stack.sp;
#ifdef VSFVM_VM_DEBUG
			VSFVM_LOG_INFO("expr start stack = %d" VSFVM_LOG_LINEEND,
				func->expression_sp);
#endif
			goto do_expr;
		}
	}
}

void vsfvm_thread_ready(struct vsfvm_thread_t *thread)
{
	uint8_t origlevel = vsfsm_sched_lock();
	vsflist_append(&thread->vm->appendlist, &thread->rdylist);
	vsfsm_sched_unlock(origlevel);
}

struct vsfvm_thread_t * vsfvm_thread_init(struct vsfvm_t *vm,
	struct vsfvm_script_t *script, uint16_t start_pos, uint8_t argc,
	struct vsfvm_thread_t *orig_thread, struct vsfvm_var_t *arg)
{
	struct vsfvm_thread_t *thread = vsfvm_alloc_thread(vm);
	struct vsfvm_func_t *func;
	struct vsfvm_var_t *var;
	uint8_t i;

	if (NULL == thread) goto fail_alloc_thread;
	memset(thread, 0, sizeof(*thread));
	func = &thread->func;

	vsf_dynstack_init(&thread->stack, sizeof(struct vsfvm_var_t),
		VSFVM_CFG_DYNARR_ITEM_BITLEN, VSFVM_CFG_DYNARR_TABLE_BITLEN);

	func->arg_reg = thread->stack.sp;
	// stack layout while calling a function:
	//	arg(s)
	//	func context
	//	auto variable(s)
	if (orig_thread != NULL)
	{
		int8_t argc_declared;

		argc--;		// first argu is FUNCTION_POS
		for (i = 0; i < argc; i++)
		{
			var = vsfvm_thread_stack_get(orig_thread, argc - i - 1);
			if (!var) goto fail_stack;
			if (var->type == VSFVM_VAR_TYPE_REFERENCE)
				var = vsfvm_get_ref(orig_thread, var);
			if (vsfvm_thread_stack_push(thread, var->value, var->type, 1))
				goto fail_stack;
		}
		for (i = 0; i < argc; i++)
			vsfvm_thread_stack_pop(orig_thread, 1);

		// leave the first arg as return value in stack
		var = vsfvm_thread_stack_get(orig_thread, 0);
		if (!var || (var->type != VSFVM_VAR_TYPE_FUNCTION))
			goto fail_stack;
		argc_declared = (int8_t)VSFVM_CODE_ARG8(var->uval32);
		if ((argc_declared >= 0) && (argc_declared != argc))
			goto fail_stack;
		start_pos = VSFVM_CODE_ARG16(var->uval32);
	}
	else if (arg != NULL)
	{
		for (i = 0; i < argc; i++, arg++)
		{
			if (vsfvm_thread_stack_push(thread, arg->value, arg->type, 1))
				goto fail_stack;
		}
	}
	if (vsfvm_push_func(thread))
		goto fail_stack;
	if (!script->lvar)
	{
		script->lvar = &thread->stack;
		script->lvar_pos = thread->stack.sp;
	}
	func->auto_reg = thread->stack.sp;

	func->argc = argc;
	func->type = VSFVM_CODE_FUNCTION_SCRIPT;
	func->pc = start_pos;

	thread->vm = vm;
	thread->script = script;
	thread->list.next = script->thread.next;
	script->thread.next = &thread->list;
	vsfvm_thread_ready(thread);
	return thread;

fail_stack:
	vsf_dynstack_fini(&thread->stack);
	vsfvm_free_thread(vm, thread);
fail_alloc_thread:
	return NULL;
}

int vsfvm_script_init(struct vsfvm_t *vm, struct vsfvm_script_t *script)
{
	script->state = VSFVM_SCRIPTSTAT_RUNNING;
	script->list.next = vm->script.next;
	script->lvar = NULL;
	vm->script.next = &script->list;

	if (!vsfvm_thread_init(vm, script, 0, script->param.argc, NULL,
			script->param.arg))
	{
		vsfvm_script_fini(vm, script);
		return -1;
	}
	return 0;
}

int vsfvm_script_fini(struct vsfvm_t *vm, struct vsfvm_script_t *script)
{
	script->state = VSFVM_SCRIPTSTAT_FINIED;

	vsflist_foreach_next(thread, thread_next, script->thread.next, struct vsfvm_thread_t, list)
	{
		vsfvm_thread_fini(vm, thread);
		vsfvm_free_thread(vm, thread);
	}
	vsflist_init_node(script->thread);
	vsflist_remove(&vm->script.next, &script->list);
	return 0;
}

int vsfvm_poll(struct vsfvm_t *vm)
{
	struct vsfvm_script_t *script;
	int ret = 0;
	uint8_t origlevel = vsfsm_sched_lock();

	vsflist_foreach_next(thread, thread_next, vm->appendlist.next, struct vsfvm_thread_t,
		rdylist)
	{
		vsflist_append(&thread->vm->rdylist, &thread->rdylist);
	}
	vm->appendlist.next = NULL;
	vsfsm_sched_unlock(origlevel);

	vsflist_foreach_next(thread, thread_next, vm->rdylist.next, struct vsfvm_thread_t,
		rdylist)
	{
		script = thread->script;
		switch (script->state)
		{
		case VSFVM_SCRIPTSTAT_RUNNING:
			if (vsfvm_thread_run(vm, thread) < 0)
			{
				script->state = VSFVM_SCRIPTSTAT_ERROR;
				break;
			}
			ret++;
			break;
		case VSFVM_SCRIPTSTAT_UNKNOWN:
		case VSFVM_SCRIPTSTAT_ERROR:
		case VSFVM_SCRIPTSTAT_FINING:
		case VSFVM_SCRIPTSTAT_FINIED:
			break;
		}
		vsflist_remove(&vm->rdylist.next, &thread->rdylist);
	}
	return ret;
}

int vsfvm_gc(struct vsfvm_t *vm)
{
	vsflist_foreach(script, vm->script.next, struct vsfvm_script_t, list)
	{
		vsflist_foreach(thread, script->thread.next, struct vsfvm_thread_t, list)
		{
			vsf_dynarr_set_size(&thread->stack.var, thread->max_sp);
			thread->max_sp = thread->stack.sp;
		}
	}
	return 0;
}

int vsfvm_register_ext(struct vsfvm_t *vm, const struct vsfvm_ext_op_t *op)
{
	struct vsfvm_ext_t *ext = vsfvm_alloc_ext();

	if (ext != NULL)
	{
		ext->op = op;
		vsflist_init_node(ext->list);
		vsflist_append(&vm->ext, &ext->list);
		return 0;
	}
	return -1;
}

int vsfvm_init(struct vsfvm_t *vm)
{
	vm->thread_pool.item_size = sizeof(struct vsfvm_thread_t);
	vsf_dynpool_init(&vm->thread_pool);
	return 0;
}

int vsfvm_fini(struct vsfvm_t *vm)
{
	vsfvm_free_extlist(&vm->ext);
	vsf_dynpool_fini(&vm->thread_pool);
	return 0;
}
