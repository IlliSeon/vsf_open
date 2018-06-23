#include "vsfvm_compiler.h"
#include "vsfvm_objdump.h"
#include "lexer/vsfvm_lexer.h"

#ifdef VSFVM_COMPILER

static struct vsfvmc_func_t *vsfvmc_get_rootfunc(struct vsfvmc_script_t *script)
{
	if (script->func_stack.sp)
		return vsf_dynarr_get(&script->func_stack.var, 1);
	else
		return &script->cur_func;
}

static int vsfvmc_push_bytecode(struct vsfvmc_t *vsfvmc, uint32_t code)
{
#ifdef VSFVM_COMPILER_DEBUG
	VSFVM_LOG_INFO("%d:", vsfvmc->bytecode.sp);
	vsfvm_tkdump(code);
#endif
	return vsf_dynstack_push(&vsfvmc->bytecode, &code, 1);
}

static uint32_t *vsfvmc_get_bytecode(struct vsfvmc_t *vsfvmc, int offset)
{
	return vsf_dynarr_get(&vsfvmc->bytecode.var, offset);
}

static int vsfvmc_add_res(struct vsfvmc_func_t *func,
	struct vsfvmc_linktbl_t *linktbl)
{
	return vsf_dynstack_push(&func->linktbl, linktbl, 1);
}

static int vsfvmc_get_var(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_script_t *script, struct vsfvmc_func_t *func,
	struct vsfvmc_lexer_sym_t *sym, uint8_t *pos, uint16_t *idx)
{
	struct vsfvmc_func_t *rootfunc = vsfvmc_get_rootfunc(script);
	int varidx;

	// if rootfunc is NULL, then current func is root
	//	variable in root stack is local variable
	if (!rootfunc)
	{
		rootfunc = func;
		goto local_var;
	}

	varidx = vsflist_get_idx(func->varlist.next, &sym->list);
	if (varidx >= 0)
	{
		*pos = VSFVM_CODE_VARIABLE_POS_FUNCAUTO;
		*idx = varidx;
		return 0;
	}

	varidx = vsflist_get_idx(func->arglist.next, &sym->list);
	if (varidx >= 0)
	{
		*pos = VSFVM_CODE_VARIABLE_POS_FUNCARG;
		*idx = varidx;
		return 0;
	}

local_var:
	varidx = vsflist_get_idx(rootfunc->varlist.next, &sym->list);
	if (varidx >= 0)
	{
		*pos = VSFVM_CODE_VARIABLE_POS_LOCAL;
		*idx = varidx;
		return 0;
	}
	return -VSFVMC_BUG;
}

static int vsfvmc_push_res(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_func_t *func)
{
	struct vsfvmc_linktbl_t *linktbl = vsf_dynstack_pop(&func->linktbl, 1);
	uint32_t len, token_num, *ptr32;
	uint32_t *ptoken;
	uint32_t goto_anchor;

	if (linktbl != NULL)
	{
		goto_anchor = vsfvmc->bytecode.sp;
		vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_goto, 0, 0));

		while (linktbl != NULL)
		{
			switch (linktbl->type)
			{
			case VSFVMC_LINKTBL_STR:
				len = strlen(linktbl->sym->name) + 1;
				token_num = ((len + 3) & ~3) >> 2;
				break;
			default:
				return -VSFVMC_BUG;
			}

			ptoken = vsfvmc_get_bytecode(vsfvmc, linktbl->bytecode_pos);
			*ptoken |= (uint16_t)(vsfvmc->bytecode.sp - linktbl->bytecode_pos - 1);

			vsfvmc_push_bytecode(vsfvmc,
				VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_RESOURCES, 0, len));
			ptr32 = (uint32_t *)linktbl->sym->name;
			for (len = 0; len < token_num; len++)
				vsfvmc_push_bytecode(vsfvmc, *ptr32++);

			linktbl = vsf_dynstack_pop(&func->linktbl, 1);
		}

		ptoken = vsfvmc_get_bytecode(vsfvmc, goto_anchor);
		*ptoken |= (int16_t)(vsfvmc->bytecode.sp - goto_anchor - 1);
	}
	return 0;
}

static int vsfvmc_push_expr(struct vsfvmc_t *vsfvmc,
	struct vsf_dynstack_t *expr_stack)
{
	struct vsfvmc_script_t *script = &vsfvmc->script;
	struct vsfvmc_func_t *func = &script->cur_func;

	struct vsfvmc_linktbl_t linktbl;
	struct vsfvmc_lexer_etoken_t *etoken;
	struct vsfvmc_token_data_t *data;
	uint8_t arg8;
	uint16_t arg16, token, token_param;

	for (uint32_t i = 0; i < expr_stack->sp; i++)
	{
		etoken = vsf_dynarr_get(&expr_stack->var, i);
		if (!etoken) return -VSFVMC_BUG;

		data = &etoken->data;
		token_param = etoken->token >> 16;
		token = etoken->token & 0xFFFF;
		switch (token)
		{
		case VSFVMC_TOKEN_NUM:
			if (data->ival & (0xFFFFFFFF << VSFVM_CODE_LENGTH))
			{
				vsfvmc_push_bytecode(vsfvmc, VSFVM_NUMBER(data->ival >> (32 - VSFVM_CODE_LENGTH)));
				vsfvmc_push_bytecode(vsfvmc, VSFVM_NUMBER(32 - VSFVM_CODE_LENGTH));
				vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SHL, 0, 0));
				vsfvmc_push_bytecode(vsfvmc, VSFVM_NUMBER(data->ival & ((1 << (32 - VSFVM_CODE_LENGTH)) - 1)));
				vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_ADD, 0, 0));
			}
			else
				vsfvmc_push_bytecode(vsfvmc, VSFVM_NUMBER(data->ival));
			break;
		case VSFVMC_TOKEN_RESOURCES:
			linktbl.bytecode_pos = vsfvmc->bytecode.sp;
			linktbl.sym = etoken->data.sym;
			linktbl.type = VSFVMC_LINKTBL_STR;
			if (vsfvmc_add_res(func, &linktbl))
				return -VSFVMC_NOT_ENOUGH_RESOURCES;
			vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_RESOURCES, 0, 0));
			break;
		case VSFVMC_TOKEN_VAR_ID:
			if (vsfvmc_get_var(vsfvmc, script, func, data->sym, &arg8, &arg16))
				return -VSFVMC_PARSER_INVALID_EXPR;
			vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_NORMAL, arg8, arg16));
			break;
		case VSFVMC_TOKEN_VAR_ID_REF:
			if (vsfvmc_get_var(vsfvmc, script, func, data->sym, &arg8, &arg16))
				return -VSFVMC_PARSER_INVALID_EXPR;
			vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_REFERENCE, arg8, arg16));
			break;
		case VSFVMC_TOKEN_FUNC_ID:
			vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_FUNCTION,
				data->sym->func.param_num, data->sym->func.pos - vsfvmc->bytecode.sp - 1));
			break;
		case VSFVMC_TOKEN_FUNC_CALL:
			if ((data->sym->func.param_num >= 0) &&
				(data->sym->func.param_num != token_param))
			{
				return -VSFVMC_COMPILER_INVALID_FUNC_PARAM;
			}
			if (data->sym->type == VSFVMC_LEXER_SYM_FUNCTION)
			{
				if (!strcmp(data->sym->name, "thread"))
					vsfvmc_push_bytecode(vsfvmc, VSFVM_FUNCTION(VSFVM_CODE_FUNCTION_THREAD,
						token_param, 0));
				else
					vsfvmc_push_bytecode(vsfvmc, VSFVM_FUNCTION(VSFVM_CODE_FUNCTION_SCRIPT,
						token_param, data->sym->func.pos - vsfvmc->bytecode.sp - 1));
			}
			else if (data->sym->type == VSFVMC_LEXER_SYM_EXTFUNC)
			{
				struct vsfvm_ext_t *ext = NULL;
				vsflist_foreach(__ext, vsfvmc->ext.next, struct vsfvm_ext_t, list)
				{
					if (__ext->op == data->sym->op)
					{
						ext = __ext;
						break;
					}
				}
				vsfvmc_push_bytecode(vsfvmc, VSFVM_FUNCTION(VSFVM_CODE_FUNCTION_EXT,
					token_param, ext->func_id + data->sym->func.pos));
			}
			else
				return -VSFVMC_COMPILER_INVALID_FUNC;
			break;
		default:
			if (token > VSFVMC_TOKEN_BINARY_OP)
			{
				switch (token)
				{
				case VSFVMC_TOKEN_MUL:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_MUL, 0, 0));	break;
				case VSFVMC_TOKEN_DIV:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_DIV, 0, 0));	break;
				case VSFVMC_TOKEN_MOD:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_MOD, 0, 0));	break;
				case VSFVMC_TOKEN_ADD:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_ADD, 0, 0));	break;
				case VSFVMC_TOKEN_SUB:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SUB, 0, 0));	break;
				case VSFVMC_TOKEN_SHL:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SHL, 0, 0));	break;
				case VSFVMC_TOKEN_SHR:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SHR, 0, 0));	break;
				case VSFVMC_TOKEN_LT:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_LT, 0, 0));		break;
				case VSFVMC_TOKEN_LE:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_LE, 0, 0));		break;
				case VSFVMC_TOKEN_GT:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_GT, 0, 0));		break;
				case VSFVMC_TOKEN_GE:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_GE, 0, 0));		break;
				case VSFVMC_TOKEN_EQ:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_EQ, 0, 0));		break;
				case VSFVMC_TOKEN_NE:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_NE, 0, 0));		break;
				case VSFVMC_TOKEN_AND:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_AND, 0, 0));	break;
				case VSFVMC_TOKEN_XOR:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_XOR, 0, 0));	break;
				case VSFVMC_TOKEN_OR:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_OR, 0, 0));		break;
				case VSFVMC_TOKEN_LAND:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_LAND, 0, 0));	break;
				case VSFVMC_TOKEN_LOR:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_LOR, 0, 0));	break;
				case VSFVMC_TOKEN_ASSIGN:	vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_ASSIGN, 0, 0));	break;
				case VSFVMC_TOKEN_COMMA_OP:	vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_COMMA, 0, 0));	break;
				default:
					return -VSFVMC_NOT_SUPPORT;
				}
			}
			else if (token > VSFVMC_TOKEN_UNARY_OP)
			{
				switch (token)
				{
				case VSFVMC_TOKEN_NOT:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_NOT, 0, 0));	break;
				case VSFVMC_TOKEN_REV:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_REV, 0, 0));	break;
				case VSFVMC_TOKEN_NEGA:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_NEGA, 0, 0));	break;
				case VSFVMC_TOKEN_POSI:		vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_POSI, 0, 0));	break;
				default:
					return -VSFVMC_NOT_SUPPORT;
				}
			}
			else
			{
				return -VSFVMC_NOT_SUPPORT;
			}
		}
	}
	return 0;
}

static struct vsfvmc_lexer_list_t *
vsfvmc_get_lexer(struct vsfvmc_t *vsfvmc, const char *ext)
{
	vsflist_foreach(lexer, vsfvmc->lexer_list.next, struct vsfvmc_lexer_list_t, list)
	{
		if (!strcmp(lexer->op->ext, ext))
			return lexer;
	}
	return NULL;
}

static int vsfvmc_script_func(struct vsfvmc_script_t *script,
	struct vsfvmc_lexer_sym_t *sym)
{
	struct vsfvmc_func_t *func = &script->cur_func;

	int err = vsf_dynstack_push(&script->func_stack, func, 1);
	if (err) return err;

	memset(func, 0, sizeof(*func));
	func->name = sym;
	func->symtbl_idx = -1;
	func->curctx.symtbl_idx = -1;
	vsf_dynstack_init(&func->ctx, sizeof(struct vsfvmc_func_ctx_t), 4, 4);
	vsf_dynstack_init(&func->linktbl, sizeof(struct vsfvmc_linktbl_t), 4, 4);
	return 0;
}

static void vsfvmc_func_fini(struct vsfvmc_func_t *func)
{
	vsf_dynstack_fini(&func->linktbl);
	vsf_dynstack_fini(&func->ctx);
}

#ifdef VSFVM_PARSER_DEBUG
static void vsfvmc_print_expr(struct vsf_dynstack_t *expr_stack)
{
	struct vsfvmc_lexer_etoken_t *etoken;
	uint16_t token, token_param;

	VSFVM_LOG_INFO("parser expr: ");
	for (uint32_t i = 0; i < expr_stack->sp; i++)
	{
		etoken = vsf_dynarr_get(&expr_stack->var, i);
		if (!etoken)
		{
			VSFVM_LOG_ERROR("fail to get token" VSFVM_LOG_LINEEND);
			return;
		}

		token_param = etoken->token >> 16;
		token = etoken->token & 0xFFFF;
		switch (token)
		{
		case VSFVMC_TOKEN_NUM:
			VSFVM_LOG_INFO("num=%d,", etoken->data.ival);
			break;
		case VSFVMC_TOKEN_RESOURCES:
			VSFVM_LOG_INFO("res=%s,", etoken->data.sym->name);
			break;
		case VSFVMC_TOKEN_VAR_ID:
			VSFVM_LOG_INFO("var=%s,", etoken->data.sym->name);
			break;
		case VSFVMC_TOKEN_VAR_ID_REF:
			VSFVM_LOG_INFO("ref=%s,", etoken->data.sym->name);
			break;
		case VSFVMC_TOKEN_FUNC_ID:
			VSFVM_LOG_INFO("func=%s,", etoken->data.sym->name);
			break;
		case VSFVMC_TOKEN_FUNC_CALL:
			VSFVM_LOG_INFO("call=%s(%d),", etoken->data.sym->name, token_param);
			break;
		default:
			if (token > VSFVMC_TOKEN_BINARY_OP)
			{
				switch (token)
				{
				case VSFVMC_TOKEN_MUL:		VSFVM_LOG_INFO("mul,");		break;
				case VSFVMC_TOKEN_DIV:		VSFVM_LOG_INFO("div,");		break;
				case VSFVMC_TOKEN_MOD:		VSFVM_LOG_INFO("mod,");		break;
				case VSFVMC_TOKEN_ADD:		VSFVM_LOG_INFO("add,");		break;
				case VSFVMC_TOKEN_SUB:		VSFVM_LOG_INFO("sub,");		break;
				case VSFVMC_TOKEN_SHL:		VSFVM_LOG_INFO("shl,");		break;
				case VSFVMC_TOKEN_SHR:		VSFVM_LOG_INFO("shr,");		break;
				case VSFVMC_TOKEN_LT:		VSFVM_LOG_INFO("lt,");		break;
				case VSFVMC_TOKEN_LE:		VSFVM_LOG_INFO("le,");		break;
				case VSFVMC_TOKEN_GT:		VSFVM_LOG_INFO("gt,");		break;
				case VSFVMC_TOKEN_GE:		VSFVM_LOG_INFO("ge,");		break;
				case VSFVMC_TOKEN_EQ:		VSFVM_LOG_INFO("eq,");		break;
				case VSFVMC_TOKEN_NE:		VSFVM_LOG_INFO("ne,");		break;
				case VSFVMC_TOKEN_AND:		VSFVM_LOG_INFO("and,");		break;
				case VSFVMC_TOKEN_XOR:		VSFVM_LOG_INFO("xor,");		break;
				case VSFVMC_TOKEN_OR:		VSFVM_LOG_INFO("or,");		break;
				case VSFVMC_TOKEN_LAND:		VSFVM_LOG_INFO("land,");	break;
				case VSFVMC_TOKEN_LOR:		VSFVM_LOG_INFO("lor,");		break;
				case VSFVMC_TOKEN_ASSIGN:	VSFVM_LOG_INFO("assign,");	break;
				case VSFVMC_TOKEN_COMMA_OP:	VSFVM_LOG_INFO("comma,");	break;
				default:
					VSFVM_LOG_ERROR("token not support" VSFVM_LOG_LINEEND);
					return;
				}
			}
			else if (token > VSFVMC_TOKEN_UNARY_OP)
			{
				switch (token)
				{
				case VSFVMC_TOKEN_NOT:		VSFVM_LOG_INFO("not,");		break;
				case VSFVMC_TOKEN_REV:		VSFVM_LOG_INFO("rev,");		break;
				case VSFVMC_TOKEN_NEGA:		VSFVM_LOG_INFO("nega,");	break;
				case VSFVMC_TOKEN_POSI:		VSFVM_LOG_INFO("posi,");	break;
				default:
					VSFVM_LOG_ERROR("token not support" VSFVM_LOG_LINEEND);
					return;
				}
			}
			else
			{
				VSFVM_LOG_ERROR("token not support" VSFVM_LOG_LINEEND);
				return;
			}
		}
	}
	VSFVM_LOG_INFO(VSFVM_LOG_LINEEND);
}
#endif

static vsf_err_t vsfvmc_func_push_ctx(struct vsfvmc_func_t *func)
{
	struct vsfvmc_func_ctx_t *ctx = &func->curctx;
	vsf_err_t err;

	err = vsf_dynstack_push(&func->ctx, ctx, 1);
	if (err) return err;

	memset(ctx, 0, sizeof(*ctx));
	ctx->symtbl_idx = -1;
	return VSFERR_NONE;
}

static struct vsfvmc_func_ctx_t *vsfvmc_func_pop_ctx(struct vsfvmc_func_t *func)
{
	struct vsfvmc_func_ctx_t *ctx = vsf_dynstack_pop(&func->ctx, 1);
	if (ctx != NULL)
		func->curctx = *ctx;
	return &func->curctx;
}

static vsf_err_t vsfvmc_parse_stmt(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
	uint32_t token, struct vsfvmc_token_data_t *data)
{
	struct vsfvmc_t *vsfvmc = (struct vsfvmc_t *)pt->user_data;
	struct vsfvmc_script_t *script = &vsfvmc->script;
	struct vsfvmc_lexer_t *lexer = &script->lexer;
	struct vsfvmc_func_t *func = &script->cur_func;
	struct vsfvmc_func_ctx_t *ctx = &func->curctx;
	struct vsf_dynstack_t *stack_exp = &lexer->expr.stack_exp;
	uint32_t *ptoken;
	int err;
	bool token_unprocessed = false;

	vsfsm_pt_begin(pt);

	while (1)
	{
		if (token == VSFVMC_TOKEN_BLOCK_BEGIN)
		{
			if (func->symtbl_idx < 0)
				func->symtbl_idx = script->lexer.symtbl.sp - 1;
			else if (ctx->etoken.token > 0)
			{
				if (ctx->symtbl_idx < 0)
					ctx->symtbl_idx = script->lexer.symtbl.sp - 1;
			}

#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: block begin, level = %d" VSFVM_LOG_LINEEND,
				func->block_level);
#endif
			func->block_level++;
		}
		else if (token == VSFVMC_TOKEN_BLOCK_END)
		{
			func->block_level--;
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: block end, level = %d" VSFVM_LOG_LINEEND,
				func->block_level);
#endif

			if (func->block_level < 0)
				return -VSFVMC_PARSER_INVALID_CLOSURE;
			else if (!func->block_level)
			{
#ifdef VSFVM_PARSER_DEBUG
				VSFVM_LOG_INFO("parser: func %s end" VSFVM_LOG_LINEEND,
					func->name->name);
#endif
				vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_return, 0, 0));

				vsfvmc_func_fini(func);
				func = vsf_dynstack_pop(&script->func_stack, 1);
				if (!func) return -VSFVMC_BUG;

				script->cur_func = *func;
				func = &script->cur_func;
				ctx = &func->curctx;
				if (ctx->etoken.token == VSFVMC_TOKEN_FUNC)
				{
					if (ctx->func_ctx.goto_anchor >= 0)
					{
						ptoken = vsfvmc_get_bytecode(vsfvmc, ctx->func_ctx.goto_anchor);
						*ptoken |= (int16_t)(vsfvmc->bytecode.sp - ctx->func_ctx.goto_anchor - 1);
					}
					ctx = vsfvmc_func_pop_ctx(func);
				}
			}
			else
			{
				if (data->uval)
				{
					struct vsflist_t *list, *next;

					vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_goto, data->uval, 0));
					while (data->uval--)
					{
						list = &func->varlist;
						next = list->next;
						while (next->next != NULL)
						{
							list = next;
							next = list->next;
						}
						list->next = NULL;
					}
				}

				if ((ctx->etoken.token > 0) &&
					(ctx->block_level == func->block_level))
				{
				block_close:
					if (ctx->etoken.token == VSFVMC_TOKEN_IF)
					{
						vsfsm_pt_wait(pt);
						if (token != VSFVMC_TOKEN_ELSE)
						{
							token_unprocessed = true;
							ptoken = vsfvmc_get_bytecode(vsfvmc, ctx->if_ctx.if_anchor);
							*ptoken |= (int16_t)(vsfvmc->bytecode.sp - ctx->if_ctx.if_anchor - 1);

							ctx = vsfvmc_func_pop_ctx(func);
							if ((ctx->etoken.token > 0) &&
								(ctx->block_level == func->block_level))
							{
								goto block_close;
							}
						}
						else
						{
							ctx->etoken.token = VSFVMC_TOKEN_ELSE;
							ctx->if_ctx.else_anchor = vsfvmc->bytecode.sp;
							vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_goto, 0, 0));
							ptoken = vsfvmc_get_bytecode(vsfvmc, ctx->if_ctx.if_anchor);
							*ptoken |= (int16_t)(vsfvmc->bytecode.sp - ctx->if_ctx.if_anchor - 1);
						}
					}
					else if (ctx->etoken.token == VSFVMC_TOKEN_ELSE)
					{
						ptoken = vsfvmc_get_bytecode(vsfvmc, ctx->if_ctx.else_anchor);
						*ptoken |= (int16_t)(vsfvmc->bytecode.sp - ctx->if_ctx.else_anchor - 1);

						ctx = vsfvmc_func_pop_ctx(func);
						if ((ctx->etoken.token > 0) &&
							(ctx->block_level == func->block_level))
						{
							goto block_close;
						}
					}
					else if (ctx->etoken.token == VSFVMC_TOKEN_WHILE)
					{
						vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_goto, 0,
							(int16_t)(ctx->while_ctx.calc_anchor - vsfvmc->bytecode.sp - 1)));
						ptoken = vsfvmc_get_bytecode(vsfvmc, ctx->while_ctx.if_anchor);
						*ptoken |= (int16_t)(vsfvmc->bytecode.sp - ctx->while_ctx.if_anchor - 1);
						ctx = vsfvmc_func_pop_ctx(func);

						if ((ctx->etoken.token > 0) &&
							(ctx->block_level == func->block_level))
						{
							goto block_close;
						}
					}
					else
						return -VSFVMC_BUG;
				}
			}
		}
		else if (token == VSFVMC_TOKEN_IMPORT)
		{
			char *path = data->sym->name;

#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: import module \"%s\"" VSFVM_LOG_LINEEND, path);
#endif
			if (vsfvmc->cb.require_lib != NULL)
			{
				char *ext = vsfile_getfileext(path);
				struct vsfvmc_lexer_list_t *list = vsfvmc_get_lexer(vsfvmc, ext);
				int err;

				if (!list)
				{
					VSFVM_LOG_ERROR("fail to find lexer for usrlib %s" VSFVM_LOG_LINEEND, path);
					return -VSFVMC_COMPILER_FAIL_USRLIB;
				}

				vsfvmc_lexer_ctx_new(lexer, list);
				err = vsfvmc->cb.require_lib(vsfvmc->cb.param, vsfvmc, path);
				vsfvmc_lexer_ctx_delete(lexer);

				if (err)
				{
					VSFVM_LOG_ERROR("fail to import usrlib %s" VSFVM_LOG_LINEEND, path);
					return -VSFVMC_COMPILER_FAIL_USRLIB;
				}
			}
			else
			{
				VSFVM_LOG_ERROR("moudle %s not found" VSFVM_LOG_LINEEND, path);
				return -VSFVMC_COMPILER_INVALID_MODULE;
			}
		}
		else if (token == VSFVMC_TOKEN_VAR)
		{
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: variable \"%s\", type %s" VSFVM_LOG_LINEEND, data->sym->name,
					data->sym->c ? data->sym->c->name : "value");
			if (stack_exp->sp > 0)
				vsfvmc_print_expr(stack_exp);
#endif

			if (!data->sym->c)
			{
				struct vsfvmc_lexer_etoken_t *etoken = vsf_dynstack_get(stack_exp, 0);

				data->sym->c = NULL;
				if ((etoken != NULL) &&
					((etoken->token & 0xFFFF) == VSFVMC_TOKEN_FUNC_CALL) &&
					(etoken->data.sym->func.retc != NULL))
				{
					data->sym->c = etoken->data.sym->func.retc;
				}
			}

			vsflist_append(&func->varlist, &data->sym->list);
			vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_var, VSFVM_CODE_VAR_I32, 0));
			if (stack_exp->sp)
			{
				uint8_t pos;
				uint16_t idx;

				if (vsfvmc_get_var(vsfvmc, script, func, data->sym, &pos, &idx))
					return -VSFVMC_BUG;
				vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_REFERENCE, pos, idx));
				err = vsfvmc_push_expr(vsfvmc, stack_exp);
				if (err) return err;
				vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_ASSIGN, 0, 0));
				vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SEMICOLON, 0, 0));
				vsfvmc_push_res(vsfvmc, func);
			}
		}
		else if (token == VSFVMC_TOKEN_CONST)
		{
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: const \"%s\" = %d" VSFVM_LOG_LINEEND, data->sym->name,
				data->sym->ival);
#endif
		}
		else if (token == VSFVMC_TOKEN_FUNC)
		{
			vsfvmc_func_push_ctx(func);
			ctx = &func->curctx;
			ctx->etoken.token = token;
			ctx->block_level = func->block_level;

			if (func->name)
			{
				ctx->func_ctx.goto_anchor = vsfvmc->bytecode.sp;
				vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_goto, 0, 0));
			}
			else
				ctx->func_ctx.goto_anchor = -1;

			data->sym->func.pos = vsfvmc->bytecode.sp;
			vsfvmc_script_func(script, data->sym);
			func = &script->cur_func;

			struct vsfvmc_lexer_etoken_t *etoken;
			for (uint32_t i = 0; i < stack_exp->sp; i++)
			{
				etoken = vsf_dynarr_get(&stack_exp->var, i);
				vsflist_append(&func->arglist, &etoken->data.sym->list);
			}

#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: function \"%s\"(%d)" VSFVM_LOG_LINEEND,
				data->sym->name, stack_exp->sp);
			if (stack_exp->sp > 0)
			{
				VSFVM_LOG_INFO("parser: function argument" VSFVM_LOG_LINEEND);
				vsfvmc_print_expr(stack_exp);
			}
#endif
		}
		else if (token == VSFVMC_TOKEN_IF)
		{
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: if" VSFVM_LOG_LINEEND);
			vsfvmc_print_expr(stack_exp);
#endif

			vsfvmc_func_push_ctx(func);
			ctx = &func->curctx;
			ctx->etoken.token = token;
			ctx->block_level = func->block_level;

		push_if:
			vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_var, VSFVM_CODE_VAR_I32, 0));
			vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_REFERENCE, VSFVM_CODE_VARIABLE_POS_STACK_END, 0));
			err = vsfvmc_push_expr(vsfvmc, stack_exp);
			if (err) return err;
			vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_ASSIGN, 0, 0));
			vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SEMICOLON, 0, 0));
			vsfvmc_push_res(vsfvmc, func);
			ctx->if_ctx.if_anchor = vsfvmc->bytecode.sp;
			vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_if, 0, 0));
		}
		else if (token == VSFVMC_TOKEN_WHILE)
		{
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: while" VSFVM_LOG_LINEEND);
			vsfvmc_print_expr(stack_exp);
#endif

			vsfvmc_func_push_ctx(func);
			ctx = &func->curctx;
			ctx->etoken.token = token;
			ctx->block_level = func->block_level;
			ctx->while_ctx.calc_anchor = vsfvmc->bytecode.sp;
			goto push_if;
		}
		else if (token == VSFVMC_TOKEN_RET)
		{
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: return" VSFVM_LOG_LINEEND);
			vsfvmc_print_expr(stack_exp);
#endif

			if (stack_exp->sp)
			{
				vsfvmc_push_bytecode(vsfvmc, VSFVM_VARIABLE(VSFVM_CODE_VARIABLE_REFERENCE_NOTRACE, VSFVM_CODE_VARIABLE_POS_FUNCARG, 0));
				err = vsfvmc_push_expr(vsfvmc, stack_exp);
				if (err) return err;
				vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_ASSIGN, 0, 0));
				vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SEMICOLON, 0, 0));
				vsfvmc_push_res(vsfvmc, func);
			}
			vsfvmc_push_bytecode(vsfvmc, VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_return, 0, 0));
		}
		else if (token == VSFVMC_TOKEN_EXPR)
		{
#ifdef VSFVM_PARSER_DEBUG
			VSFVM_LOG_INFO("parser: expression" VSFVM_LOG_LINEEND);
			vsfvmc_print_expr(stack_exp);
#endif

			err = vsfvmc_push_expr(vsfvmc, stack_exp);
			if (err) return err;
			vsfvmc_push_bytecode(vsfvmc, VSFVM_SYMBOL(VSFVM_CODE_SYMBOL_SEMICOLON, 0, 0));
			vsfvmc_push_res(vsfvmc, func);
		}
		else
			return -VSFVMC_NOT_SUPPORT;

		if (!token_unprocessed)
			vsfsm_pt_wait(pt);

		token_unprocessed = false;
	}

	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

vsf_err_t vsfvmc_on_stmt(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data)
{
	struct vsfvmc_script_t *script =
		container_of(lexer, struct vsfvmc_script_t, lexer);
	return vsfvmc_parse_stmt(&script->pt_stmt, VSFSM_EVT_USER, token, data);
}

int vsfvmc_register_lexer(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_lexer_list_t *lexer_list)
{
	lexer_list->list.next = vsfvmc->lexer_list.next;
	vsfvmc->lexer_list.next = &lexer_list->list;
	return 0;
}

int vsfvmc_register_ext(struct vsfvmc_t *vsfvmc, const struct vsfvm_ext_op_t *op)
{
	struct vsfvm_ext_t *ext = vsfvm_alloc_ext();

	if (ext != NULL)
	{
		ext->op = op;
		vsflist_init_node(ext->list);
		vsflist_append(&vsfvmc->ext, &ext->list);
		return 0;
	}
	return -1;
}

static void vsfvmc_script_fini(struct vsfvmc_t *vsfvmc);
void vsfvmc_fini(struct vsfvmc_t *vsfvmc)
{
	vsfvmc_script_fini(vsfvmc);
	vsf_dynstack_fini(&vsfvmc->bytecode);
	vsfvm_free_extlist(&vsfvmc->ext);
}

int vsfvmc_init(struct vsfvmc_t *vsfvmc, void *param,
	require_usrlib_t require_lib)
{
	memset(vsfvmc, 0, sizeof(*vsfvmc));
	vsfvmc->cb.param = param;
	vsfvmc->cb.require_lib = require_lib;
	vsf_dynstack_init(&vsfvmc->bytecode, sizeof(uint32_t), 8, 4);
	return 0;
}

static int vsfvmc_ext_init(struct vsfvmc_t *vsfvmc, struct vsfvm_ext_t *ext)
{
	struct vsfvmc_script_t *script = &vsfvmc->script;

	ext->symarr.sym = (struct vsfvmc_lexer_sym_t *)ext->op->sym;
	ext->symarr.num = ext->op->sym_num;
	ext->symarr.id = script->lexer.symid;
	script->lexer.symid += ext->symarr.num;
	return 0;
}

static void vsfvmc_script_fini(struct vsfvmc_t *vsfvmc)
{
	struct vsfvmc_script_t *script = &vsfvmc->script;
	struct vsfvmc_func_t *f;

	vsfvmc_lexer_fini(&script->lexer);
	vsfvmc_func_fini(&script->cur_func);

	do {
		if (f = vsf_dynstack_pop(&script->func_stack, 1))
			vsfvmc_func_fini(f);
	} while (f != NULL);
	vsf_dynstack_fini(&script->func_stack);
}

int vsfvmc_script(struct vsfvmc_t *vsfvmc, const char *script_name)
{
	struct vsfvmc_script_t *script = &vsfvmc->script;
	struct vsfvmc_lexer_list_t *lexer;
	uint16_t func_id = 0;
	int err;

	memset(script, 0, sizeof(*script));
	vsf_dynstack_init(&script->func_stack, sizeof(script->cur_func), 1, 4);
	script->name = script_name;
	script->pt_stmt.user_data = vsfvmc;
	script->pt_stmt.state = 0;

	lexer = vsfvmc_get_lexer(vsfvmc, vsfile_getfileext((char *)script_name));
	if (!lexer) return VSFVMC_NOT_SUPPORT;
	err = vsfvmc_lexer_init(&script->lexer, lexer, &vsfvmc->ext);
	if (err) return err;

	vsflist_foreach(ext, vsfvmc->ext.next, struct vsfvm_ext_t, list)
	{
		err = vsfvmc_ext_init(vsfvmc, ext);
		if (err) return err;
		ext->func_id = func_id;
		func_id += ext->op->func_num;
	}

	err = vsfvmc_input(vsfvmc, "__startup__(){");
	if (err < 0) vsfvmc_script_fini(vsfvmc);
	return err;
}

int vsfvmc_input(struct vsfvmc_t *vsfvmc, const char *code)
{
	struct vsfvmc_lexer_t *lexer = &vsfvmc->script.lexer;
	int err;

	if (code[0] == '\xFF')
	{
		err = vsfvmc_lexer_input(lexer, "return;}");
		vsfvmc_script_fini(vsfvmc);
	}
	else
		err = vsfvmc_lexer_input(lexer, code);
	return err;
}

#endif
