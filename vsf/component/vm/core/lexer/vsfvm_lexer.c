#include "vsfvm_compiler.h"
#include "lexer/vsfvm_lexer.h"

static char *vsfvmc_symtbl_strpush(struct vsfvmc_lexer_symtbl_t *symtbl,
	char *symbol)
{
	struct vsf_dynarr_t *strpool = &symtbl->strpool;
	uint32_t len = strlen(symbol) + 1;
	char *ret;

	if (len > strpool->item_size)
		return NULL;

	if (!symtbl->strbuf ||
		(symtbl->strpos + len >= symtbl->strbuf + strpool->item_size))
	{
		uint32_t size = vsf_dynarr_get_size(strpool);
		if (vsf_dynarr_set_size(strpool, size + 1) < 0)
			return NULL;
		symtbl->strbuf = symtbl->strpos = vsf_dynarr_get(strpool, size);
	}

	ret = symtbl->strpos;
	strcpy(symtbl->strpos, symbol);
	symtbl->strpos += len;
	return ret;
}

static struct vsfvmc_lexer_sym_t *vsfvmc_symarr_get(
	struct vsfvmc_lexer_symarr_t *symarr, char *symbol, uint16_t *id)
{
	struct vsfvmc_lexer_sym_t *sym = symarr->sym;
	for (uint16_t i = 0; i < symarr->num; i++, sym++)
	{
		if (!strcmp(sym->name, symbol))
		{
			if (id != NULL)
				*id = i;
			return sym;
		}
	}
	return NULL;
}

static struct vsfvmc_lexer_sym_t *vsfvmc_symtbl_get(
	struct vsfvmc_lexer_symtbl_t *symtbl, char *symbol)
{
	struct vsfvmc_lexer_sym_t *sym;
	uint32_t size = vsf_dynarr_get_size(&symtbl->table);

	for (uint32_t i = 0; i < size; i++)
	{
		sym = vsf_dynarr_get(&symtbl->table, i);
		if (!strcmp(sym->name, symbol))
			return sym;
	}
	return NULL;
}

static struct vsfvmc_lexer_sym_t *vsfvmc_symtbl_add(
	struct vsfvmc_lexer_symtbl_t *symtbl, char *symbol)
{
	struct vsfvmc_lexer_sym_t *sym = vsfvmc_symtbl_get(symtbl, symbol);
	if (!sym)
	{
		uint32_t size = vsf_dynarr_get_size(&symtbl->table);
		char *name = vsfvmc_symtbl_strpush(symtbl, symbol);

		if (!name || (vsf_dynarr_set_size(&symtbl->table, size + 1) < 0))
			return NULL;

		sym = vsf_dynarr_get(&symtbl->table, size);
		if (sym != NULL)
		{
			memset(sym, 0, sizeof(*sym));
			sym->name = name;
		}
	}
	return sym;
}

static int vsfvmc_symtbl_fini(struct vsfvmc_lexer_symtbl_t *symtbl)
{
	vsf_dynarr_fini(&symtbl->table);
	vsf_dynarr_fini(&symtbl->strpool);
	return 0;
}

static int vsfvmc_symtbl_init(struct vsfvmc_lexer_symtbl_t *symtbl)
{
	symtbl->varnum = 0;

	symtbl->table.item_size = sizeof(struct vsfvmc_lexer_sym_t);
	symtbl->table.item_num_bitlen = 2;
	symtbl->table.table_size_bitlen = 2;
	vsf_dynarr_init(&symtbl->table);

	symtbl->strpool.item_size = VSFVMC_MAX_SYMLEN;
	symtbl->strpool.item_num_bitlen = 0;
	symtbl->strpool.table_size_bitlen = 4;
	vsf_dynarr_init(&symtbl->strpool);
	symtbl->strbuf = symtbl->strpos = NULL;
	return 0;
}

struct vsfvmc_lexer_sym_t *vsfvmc_lexer_symtbl_get(
	struct vsfvmc_lexer_t *lexer, char *symbol, uint16_t *id)
{
	struct vsfvmc_lexer_symtbl_t *symtbl;
	struct vsfvmc_lexer_symarr_t *symarr;
	struct vsfvmc_lexer_sym_t *sym;
	uint16_t offset;
	uint32_t i = 0;

	symarr = (struct vsfvmc_lexer_symarr_t *)&lexer->curctx.op->keyword;
	sym = vsfvmc_symarr_get(symarr, symbol, &offset);
	if (sym != NULL)
	{
	symarr_ok:
		if (id != NULL)
			*id = symarr->id + offset;
		return sym;
	}

	vsflist_foreach(ext, lexer->ext->next, struct vsfvm_ext_t, list)
	{
		symarr = &ext->symarr;
		sym = vsfvmc_symarr_get(symarr, symbol, &offset);
		if (sym != NULL) goto symarr_ok;
	}

	do {
		symtbl = vsf_dynstack_get(&lexer->symtbl, i++);
		if (symtbl != NULL)
		{
			sym = vsfvmc_symtbl_get(symtbl, symbol);
			if (sym != NULL)
			{
				if (id != NULL)
					*id = sym->id;
				return sym;
			}
		}
	} while (symtbl != NULL);
	return NULL;
}

struct vsfvmc_lexer_sym_t *vsfvmc_lexer_symtbl_add(
	struct vsfvmc_lexer_t *lexer, char *symbol, uint16_t *id)
{
	struct vsfvmc_lexer_sym_t *sym = vsfvmc_lexer_symtbl_get(lexer, symbol, id);
	if (sym != NULL) return sym;

	struct vsfvmc_lexer_symtbl_t *symtbl = vsf_dynstack_get(&lexer->symtbl, 0);
	sym = vsfvmc_symtbl_add(symtbl, symbol);
	if (sym != NULL)
	{
		sym->id = lexer->symid++;
		if (id != NULL)
			*id = sym->id;
#ifdef VSFVM_LEXER_DEBUG
		VSFVM_LOG_INFO("symbol: %s, id = %d" VSFVM_LOG_LINEEND, symbol, sym->id);
#endif
	}
	return sym;
}

int vsfvmc_lexer_symtbl_delete(struct vsfvmc_lexer_t *lexer)
{
	struct vsfvmc_lexer_symtbl_t *symtbl = vsf_dynstack_pop(&lexer->symtbl, 1);
	if (symtbl) return vsfvmc_symtbl_fini(symtbl);
	return -1;
}

int vsfvmc_lexer_symtbl_new(struct vsfvmc_lexer_t *lexer)
{
	struct vsfvmc_lexer_symtbl_t symtbl;
	vsfvmc_symtbl_init(&symtbl);
	return vsf_dynstack_push(&lexer->symtbl, &symtbl, 1);
}

int vsfvmc_lexer_getchar(struct vsfvmc_lexer_t *lexer)
{
	int ch = vsfvmc_lexer_peekchar(lexer);

	if (ch != '\0')
	{
		lexer->curctx.pos++;
		lexer->curctx.col++;
		if (ch == '\n')
		{
			lexer->curctx.line++;
			lexer->curctx.col = 0;
		}
	}
	return ch;
}

int vsfvmc_lexer_on_token(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data)
{
	return lexer->curctx.op->parse_token(&lexer->curctx.pt,
			VSFSM_EVT_USER, token, data);
}

struct vsfvmc_lexer_etoken_t *vsfvmc_lexer_expr_popexp(
	struct vsfvmc_lexer_t *lexer)
{
	return (struct vsfvmc_lexer_etoken_t *)
		vsf_dynstack_pop(&lexer->expr.stack_exp, 1);
}

int vsfvmc_lexer_expr_pushexp(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data)
{
	struct vsfvmc_lexer_etoken_t etoken = { token };
	if (data != NULL) etoken.data = *data;
	return vsf_dynstack_push(&lexer->expr.stack_exp, &etoken, 1);
}

struct vsfvmc_lexer_etoken_t *vsfvmc_lexer_expr_popop(
	struct vsfvmc_lexer_t *lexer)
{
	return (struct vsfvmc_lexer_etoken_t *)
		vsf_dynstack_pop(&lexer->expr.stack_op, 1);
}

int vsfvmc_lexer_expr_pushop(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data)
{
	uint32_t prio = token & VSFVMC_TOKEN_OP_PRIO_MASK;
	struct vsfvmc_lexer_etoken_t *etoken =
		vsf_dynstack_get(&lexer->expr.stack_op, 0);

	while ((lexer->expr.stack_op.sp > lexer->expr.context.opsp) &&
		(etoken != NULL) &&
		((etoken->token & VSFVMC_TOKEN_OP_PRIO_MASK) <= prio))
	{
		etoken = vsfvmc_lexer_expr_popop(lexer);
		vsfvmc_lexer_expr_pushexp(lexer, etoken->token, &etoken->data);
		etoken = vsf_dynstack_get(&lexer->expr.stack_op, 0);
	}

	struct vsfvmc_lexer_etoken_t etoken_tmp = { token };
	if (data != NULL) etoken_tmp.data = *data;
	return vsf_dynstack_push(&lexer->expr.stack_op, &etoken_tmp, 1);
}

static int vsfvmc_lexer_expr_pushctx(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_exprctx_t *ctx)
{
	int err = vsf_dynstack_push_ext(&lexer->expr.stack_op, ctx, sizeof(*ctx));
	if (!err) lexer->expr.nesting++;
	return err;
}

static int vsfvmc_lexer_expr_popctx(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_exprctx_t *ctx)
{
	int err = vsf_dynstack_pop_ext(&lexer->expr.stack_op, ctx, sizeof(*ctx));
	if (!err) lexer->expr.nesting--;
	return err;
}

static int vsfvmc_lexer_expr_nesting(struct vsfvmc_lexer_t *lexer)
{
	struct vsfvmc_lexer_exprctx_t *ctx = &lexer->expr.context;
	uint32_t pre_token = ctx->pre_etoken.token;

	if (vsfvmc_lexer_expr_pushctx(lexer, ctx))
		return -VSFVMC_NOT_ENOUGH_RESOURCES;

	if (pre_token == VSFVMC_TOKEN_FUNC_CALL)
		ctx->comma_is_op = false;
	else if ((pre_token == VSFVMC_TOKEN_LGROUPING) ||
		(pre_token == VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE))
	{
		ctx->comma_is_op = true;
	}
	ctx->pt.state = 0;
	ctx->pre_etoken.token = VSFVMC_TOKEN_NONE;
	ctx->opsp = lexer->expr.stack_op.sp;
	ctx->expsp = lexer->expr.stack_exp.sp;
	return 0;
}

int vsfvmc_lexer_expr_reset(struct vsfvmc_lexer_t *lexer)
{
	vsf_dynarr_set_size(&lexer->expr.stack_exp.var, 0);
	vsf_dynarr_set_size(&lexer->expr.stack_op.var, 0);
	lexer->expr.stack_exp.sp = 0;
	lexer->expr.stack_op.sp = 0;
	memset(&lexer->expr.context, 0, sizeof(lexer->expr.context));
	lexer->expr.nesting = 0;
	lexer->expr.context.pt.user_data = lexer;
	return 0;
}

static vsf_err_t vsfvmc_lexer_calc_expr(struct vsf_dynstack_t *expr_stack,
	int32_t *ival)
{
	struct vsf_dynstack_t calc_stack;
	struct vsfvmc_lexer_etoken_t *etoken, *arg1, *arg2;
	vsf_err_t err = -VSFVMC_NOT_SUPPORT;

	vsf_dynstack_init(&calc_stack, expr_stack->var.item_size,
		expr_stack->var.item_num_bitlen, expr_stack->var.table_size_bitlen);

	for (uint32_t i = 0; i < expr_stack->sp; i++)
	{
		etoken = vsf_dynarr_get(&expr_stack->var, i);
		if (!etoken)
		{
		expr_bug:
			err = -VSFVMC_BUG;
			goto end;
		}

		switch (etoken->token)
		{
		case VSFVMC_TOKEN_NUM:
			vsf_dynstack_push(&calc_stack, etoken, 1);
			break;
		case VSFVMC_TOKEN_VARIABLE:
		case VSFVMC_TOKEN_VARIABLE_REF:
		case VSFVMC_TOKEN_FUNC_ID:
		case VSFVMC_TOKEN_FUNC_CALL:
			err = -VSFVMC_NOT_SUPPORT;
			goto end;
		default:
			if (etoken->token > VSFVMC_TOKEN_BINARY_OP)
			{
				arg2 = vsf_dynstack_pop(&calc_stack, 1);
				arg1 = vsf_dynstack_get(&calc_stack, 0);
				if (!arg1 || !arg2)
					goto expr_bug;

				switch (etoken->token)
				{
				case VSFVMC_TOKEN_MUL:		arg1->data.ival = arg1->data.ival * arg2->data.ival;	break;
				case VSFVMC_TOKEN_DIV:
					if (!arg2->data.ival)
					{
						err = -VSFVMC_PARSER_DIV0;
						goto end;
					}
											arg1->data.ival = arg1->data.ival / arg2->data.ival;	break;
				case VSFVMC_TOKEN_MOD:		arg1->data.ival = arg1->data.ival & arg2->data.ival;	break;
				case VSFVMC_TOKEN_ADD:		arg1->data.ival = arg1->data.ival + arg2->data.ival;	break;
				case VSFVMC_TOKEN_SUB:		arg1->data.ival = arg1->data.ival - arg2->data.ival;	break;
				case VSFVMC_TOKEN_SHL:		arg1->data.ival = arg1->data.ival << arg2->data.ival;	break;
				case VSFVMC_TOKEN_SHR:		arg1->data.ival = arg1->data.ival >> arg2->data.ival;	break;
				case VSFVMC_TOKEN_LT:		arg1->data.ival = arg1->data.ival < arg2->data.ival;	break;
				case VSFVMC_TOKEN_LE:		arg1->data.ival = arg1->data.ival <= arg2->data.ival;	break;
				case VSFVMC_TOKEN_GT:		arg1->data.ival = arg1->data.ival > arg2->data.ival;	break;
				case VSFVMC_TOKEN_GE:		arg1->data.ival = arg1->data.ival >= arg2->data.ival;	break;
				case VSFVMC_TOKEN_EQ:		arg1->data.ival = arg1->data.ival == arg2->data.ival;	break;
				case VSFVMC_TOKEN_NE:		arg1->data.ival = arg1->data.ival != arg2->data.ival;	break;
				case VSFVMC_TOKEN_AND:		arg1->data.ival = arg1->data.ival & arg2->data.ival;	break;
				case VSFVMC_TOKEN_XOR:		arg1->data.ival = arg1->data.ival ^ arg2->data.ival;	break;
				case VSFVMC_TOKEN_OR:		arg1->data.ival = arg1->data.ival | arg2->data.ival;	break;
				case VSFVMC_TOKEN_LAND:		arg1->data.ival = arg1->data.ival && arg2->data.ival;	break;
				case VSFVMC_TOKEN_LOR:		arg1->data.ival = arg1->data.ival || arg2->data.ival;	break;
				case VSFVMC_TOKEN_COMMA_OP:	arg1->data.ival = arg2->data.ival;						break;
				default:
				expr_not_support:
					err = -VSFVMC_NOT_SUPPORT;
					goto end;
				}
			}
			else if (etoken->token > VSFVMC_TOKEN_UNARY_OP)
			{
				arg1 = vsf_dynstack_get(&calc_stack, 0);
				if (!arg1)
					goto expr_bug;

				switch (etoken->token)
				{
				case VSFVMC_TOKEN_NOT:		arg1->data.ival = !arg1->data.ival;						break;
				case VSFVMC_TOKEN_REV:		arg1->data.ival = ~arg1->data.ival;						break;
				case VSFVMC_TOKEN_NEGA:		arg1->data.ival = -arg1->data.ival;						break;
				case VSFVMC_TOKEN_POSI:		arg1->data.ival = +arg1->data.ival;						break;
				default: goto expr_not_support;
				}
			}
			else goto expr_not_support;
		}
	}
	if (calc_stack.sp == 1)
	{
		arg1 = vsf_dynstack_pop(&calc_stack, 1);
		if (ival != NULL)
			*ival = arg1->data.ival;
		err = VSFERR_NONE;
	}

end:
	vsf_dynstack_fini(&calc_stack);
	return err;
}

static vsf_err_t vsfvmc_lexer_terminate_expr(struct vsfvmc_lexer_t *lexer,
	uint32_t token)
{
	struct vsfvmc_lexer_exprctx_t *ctx = &lexer->expr.context, ctx_tmp;
	struct vsfvmc_lexer_etoken_t *etoken, etoken_tmp = { VSFVMC_TOKEN_NUM };

	while (lexer->expr.stack_op.sp > ctx->opsp)
	{
		etoken = vsfvmc_lexer_expr_popop(lexer);
		if (!etoken) return -VSFVMC_BUG;
		vsfvmc_lexer_expr_pushexp(lexer, etoken->token, &etoken->data);
	}
	if (lexer->expr.nesting)
	{
		if (vsfvmc_lexer_expr_popctx(lexer, &ctx_tmp) < 0)
			return -VSFVMC_BUG;

		if (ctx_tmp.pre_etoken.token == VSFVMC_TOKEN_FUNC_CALL)
		{
			if (token == VSFVMC_TOKEN_RGROUPING)
			{
				if (ctx->pre_etoken.token != VSFVMC_TOKEN_NONE)
				{
					if (((ctx->pre_etoken.token < VSFVMC_TOKEN_EXPR_OPERAND) ||
							(ctx->pre_etoken.token > VSFVMC_TOKEN_OPERATOR)) &&
						(ctx->pre_etoken.token != VSFVMC_TOKEN_FUNC_CALL))
					{
						return -VSFVMC_PARSER_EXPECT_FUNC_PARAM;
					}
					ctx_tmp.func_param++;
					if (!ctx_tmp.func_param)
						return -VSFVMC_PARSER_TOO_MANY_FUNC_PARAM;
				}

				vsfvmc_lexer_expr_pushexp(lexer,
					ctx_tmp.pre_etoken.token + (ctx_tmp.func_param << 16),
					&ctx_tmp.pre_etoken.data);
			expr_goon:
				*ctx = ctx_tmp;
				return VSFERR_NOT_READY;
			}
			else if (token == VSFVMC_TOKEN_COMMA)
			{
				ctx_tmp.func_param++;
				if (!ctx_tmp.func_param)
					return -VSFVMC_PARSER_TOO_MANY_FUNC_PARAM;
				vsfvmc_lexer_expr_pushctx(lexer, &ctx_tmp);
				ctx->pre_etoken.token = VSFVMC_TOKEN_NONE;
				return VSFERR_NOT_READY;
			}
			else
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
		}
		else if (ctx_tmp.pre_etoken.token == VSFVMC_TOKEN_LGROUPING)
		{
			if (token == VSFVMC_TOKEN_RGROUPING)
				goto expr_goon;
			else if (token == VSFVMC_TOKEN_COMMA)
			{
			expr_comma_op_nesting:
				vsfvmc_lexer_expr_pushctx(lexer, &ctx_tmp);
				ctx->pre_etoken.token = VSFVMC_TOKEN_NONE;
				goto expr_comma_op;
			}
			else
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
		}
		else if (ctx_tmp.pre_etoken.token == VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE)
		{
			if (token == VSFVMC_TOKEN_COMMA)
				goto expr_comma_op_nesting;
			else
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
		}
		else
			return -VSFVMC_BUG;
	}
	else if (token == VSFVMC_TOKEN_COMMA)
	{
		if (ctx->comma_is_op)
		{
		expr_comma_op:
			if (ctx->pre_etoken.token > VSFVMC_TOKEN_OPERATOR)
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			vsfvmc_lexer_expr_pushop(lexer, VSFVMC_TOKEN_COMMA_OP, NULL);
			return VSFERR_NOT_READY;
		}
		goto terminate_expr;
	}
	else if ((token == VSFVMC_TOKEN_SEMICOLON) ||
		(token == VSFVMC_TOKEN_RGROUPING))
	{
	terminate_expr:
		// VSFVMC_TOKEN_VAR_OP and VSFVMC_TOKEN_FUNC_CALL are valid
		if (ctx->pre_etoken.token > VSFVMC_TOKEN_UNARY_OP)
			return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
		if (lexer->expr.stack_op.sp != 0)
			return -VSFVMC_PARSER_INVALID_EXPR;

		if (!vsfvmc_lexer_calc_expr(&lexer->expr.stack_exp,
				&etoken_tmp.data.ival))
		{
			vsf_dynstack_reset(&lexer->expr.stack_exp);
			vsf_dynstack_push(&lexer->expr.stack_exp, &etoken_tmp, 1);
		}
		return VSFERR_NONE;
	}
	else
		return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
}

static vsf_err_t vsfvmc_lexer_parse_expr(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
	uint32_t token, struct vsfvmc_token_data_t *data)
{
	struct vsfvmc_lexer_t *lexer = (struct vsfvmc_lexer_t *)pt->user_data;
	struct vsfvmc_lexer_exprctx_t *ctx = &lexer->expr.context;
	vsf_err_t err;

	vsfsm_pt_begin(pt);

	while (1)
	{
		if (token < VSFVMC_TOKEN_STMT_END)
			return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
		else if ((token > VSFVMC_TOKEN_EXPR_TERMINATOR) &&
			(token < VSFVMC_TOKEN_EXPR_TERMINATOR_END))
		{
			err = vsfvmc_lexer_terminate_expr(lexer, token);
			if (err < 0) return err;
			else if (err > 0)
			{
				if ((token == VSFVMC_TOKEN_RGROUPING) &&
						((ctx->pre_etoken.token != VSFVMC_TOKEN_FUNC_CALL) ||
							!ctx->pre_etoken.data.sym->func.retc))
				{
					ctx->pre_etoken.token = VSFVMC_TOKEN_EXPR_OPERAND_CONST;
				}
				goto expr_wait_next;
			}
			else return VSFERR_NONE;
		}
		else if ((token == VSFVMC_TOKEN_NUM) ||
			(token == VSFVMC_TOKEN_RESOURCES))
		{
			vsfvmc_lexer_expr_pushexp(lexer, token, data);
			ctx->pre_etoken.token = VSFVMC_TOKEN_EXPR_OPERAND_CONST;
		}
		else if (token == VSFVMC_TOKEN_FUNC_CALL)
		{
			ctx->pre_etoken.token = VSFVMC_TOKEN_FUNC_CALL;
			ctx->pre_etoken.data = *data;
			vsfsm_pt_wait(pt);
			if (token == VSFVMC_TOKEN_LGROUPING)
			{
			expr_nesting:
				err = vsfvmc_lexer_expr_nesting(lexer);
				if (err < 0) return err;
				goto expr_wait_next;
			}
			else
			{
				vsfvmc_lexer_expr_pushexp(lexer, VSFVMC_TOKEN_FUNC_ID,
					&ctx->pre_etoken.data);
				ctx->pre_etoken.token = VSFVMC_TOKEN_EXPR_OPERAND_CONST;
				continue;
			}
		}
		else if (token == VSFVMC_TOKEN_VARIABLE)
		{
			ctx->pre_etoken.token = VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE;
			ctx->pre_etoken.data = *data;
			vsfsm_pt_wait(pt);
			if (token == VSFVMC_TOKEN_ASSIGN)
			{
				vsfvmc_lexer_expr_pushexp(lexer, VSFVMC_TOKEN_VARIABLE_REF,
					&ctx->pre_etoken.data);
				continue;
			}
			else
			{
				vsfvmc_lexer_expr_pushexp(lexer, VSFVMC_TOKEN_VARIABLE,
					&ctx->pre_etoken.data);
				continue;
			}
		}
		else if (token == VSFVMC_TOKEN_VARIABLE_REF)
		{
			ctx->pre_etoken.token = VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE;
			ctx->pre_etoken.data = *data;
			vsfvmc_lexer_expr_pushexp(lexer, VSFVMC_TOKEN_VARIABLE_REF,
				&ctx->pre_etoken.data);
		}
		else if (token == VSFVMC_TOKEN_DOT)
		{
			vsfsm_pt_wait(pt);
			if ((token != VSFVMC_TOKEN_SYMBOL) &&
				(token != VSFVMC_TOKEN_VARIABLE) &&
				(token != VSFVMC_TOKEN_FUNC_CALL))
			{
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			}

			const struct vsfvm_class_t *c;
			if (ctx->pre_etoken.token == VSFVMC_TOKEN_FUNC_CALL)
				c = ctx->pre_etoken.data.sym->func.retc;
			else if (ctx->pre_etoken.token == VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE)
				c = ctx->pre_etoken.data.sym->c;
			else return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			if (!c) return -VSFVMC_PARSER_UNEXPECTED_TOKEN;

			strcpy(lexer->cur_symbol, c->name);
			strcat(lexer->cur_symbol, "_");
			strcat(lexer->cur_symbol, data->sym->name);
			data->sym = vsfvmc_lexer_symtbl_get(lexer, lexer->cur_symbol, NULL);
			if (!data->sym) return -VSFVMC_PARSER_MEMFUNC_NOT_FOUND;

			{
				struct vsfvmc_lexer_etoken_t etoken = ctx->pre_etoken;

				ctx->pre_etoken.token = VSFVMC_TOKEN_FUNC_CALL;
				ctx->pre_etoken.data = *data;
				ctx->func_param = 0;
				err = vsfvmc_lexer_expr_nesting(lexer);
				if (err < 0) return err;
				ctx->pre_etoken = etoken;
			}
			vsfsm_pt_wait(pt);
			if (token != VSFVMC_TOKEN_LGROUPING)
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			vsfsm_pt_wait(pt);
			if (token != VSFVMC_TOKEN_RGROUPING)
				vsfvmc_lexer_terminate_expr(lexer, VSFVMC_TOKEN_COMMA);
			continue;
		}
		else if (token == VSFVMC_TOKEN_LGROUPING)
		{
			if ((ctx->pre_etoken.token != VSFVMC_TOKEN_NONE) &&
				(ctx->pre_etoken.token < VSFVMC_TOKEN_OPERATOR))
			{
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			}
			ctx->pre_etoken.token = VSFVMC_TOKEN_LGROUPING;
			goto expr_nesting;
		}
		else if (token > VSFVMC_TOKEN_BINARY_OP)
		{
			if ((ctx->pre_etoken.token != VSFVMC_TOKEN_EXPR_OPERAND_CONST) &&
				((ctx->pre_etoken.token != VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE) ||
					(ctx->pre_etoken.data.sym->type < VSFVMC_LEXER_SYM_OPRAND) ||
					(ctx->pre_etoken.data.sym->c != NULL)))
			{
				if (token == VSFVMC_TOKEN_ADD)
				{
					token = VSFVMC_TOKEN_POSI;
					goto expr_parse_unary_op;
				}
				else if (token == VSFVMC_TOKEN_SUB)
				{
					token = VSFVMC_TOKEN_NEGA;
					goto expr_parse_unary_op;
				}
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			}
			else if ((token == VSFVMC_TOKEN_ASSIGN) &&
				(ctx->pre_etoken.token != VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE))
			{
				return -VSFVMC_PARSER_UNEXPECTED_TOKEN;
			}
			vsfvmc_lexer_expr_pushop(lexer, token, data);
			ctx->pre_etoken.token = VSFVMC_TOKEN_BINARY_OP;
			ctx->pre_etoken.data.uval = token;
		}
		else if (token > VSFVMC_TOKEN_UNARY_OP)
		{
		expr_parse_unary_op:
			vsfvmc_lexer_expr_pushop(lexer, token, data);
			ctx->pre_etoken.token = VSFVMC_TOKEN_UNARY_OP;
			ctx->pre_etoken.data.uval = token;
		}
		else
			return -VSFVMC_PARSER_UNEXPECTED_TOKEN;

	expr_wait_next:
		vsfsm_pt_wait(pt);
	}

	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

vsf_err_t vsfvmc_lexer_on_expr(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data)
{
	vsf_err_t err = vsfvmc_lexer_parse_expr(&lexer->expr.context.pt,
		VSFSM_EVT_USER, token, data);
	if (err < 0) vsfvmc_lexer_expr_reset(lexer);
	return err;
}

vsf_err_t vsfvmc_lexer_on_stmt(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data)
{
	struct vsfvmc_token_data_t local_data;

	switch (token)
	{
	case VSFVMC_TOKEN_BLOCK_BEGIN:
		if (vsfvmc_lexer_symtbl_new(lexer))
			return -VSFVMC_NOT_ENOUGH_RESOURCES;
		break;
	case VSFVMC_TOKEN_BLOCK_END:
		{
			struct vsfvmc_lexer_symtbl_t *symtbl =
				vsf_dynstack_get(&lexer->symtbl, 0);
			if (!symtbl) return -VSFVMC_BUG;
			local_data.uval = symtbl->varnum;
			data = &local_data;
		}
		if (vsfvmc_lexer_symtbl_delete(lexer))
			return -VSFVMC_BUG;
		break;
	case VSFVMC_TOKEN_VAR:
		{
			struct vsfvmc_lexer_symtbl_t *symtbl;
			symtbl = vsf_dynstack_get(&lexer->symtbl, 0);
			if (!symtbl) return -VSFVMC_BUG;
			symtbl->varnum++;
		}
		break;
	}
	return vsfvmc_on_stmt(lexer, token, data);
}

void vsfvmc_lexer_fini(struct vsfvmc_lexer_t *lexer)
{
	int err;

	if (lexer->curctx.priv != NULL)
	{
		vsf_bufmgr_free(lexer->curctx.priv);
		lexer->curctx.priv = NULL;
	}

	vsfvmc_lexer_expr_reset(lexer);

	do {
		err = vsfvmc_lexer_ctx_delete(lexer);
	} while (!err);
	vsf_dynstack_fini(&lexer->ctx_stack);

	do {
		err = vsfvmc_lexer_symtbl_delete(lexer);
	} while (!err);
	vsf_dynstack_fini(&lexer->symtbl);
}

static int vsfvmc_lexer_ctx_fini(struct vsfvmc_lexer_t *lexer)
{
	struct vsfvmc_lexer_ctx_t *ctx = &lexer->curctx;
	if (ctx->priv != NULL)
	{
		vsf_bufmgr_free(ctx->priv);
		ctx->priv = NULL;
	}
	return 0;
}

static int vsfvmc_lexer_ctx_init(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_list_t *list)
{
	struct vsfvmc_lexer_ctx_t *ctx = &lexer->curctx;
	int ret;

	memset(ctx, 0, sizeof(*ctx));
	ctx->op = list->op;
	if (ctx->op->priv_size)
	{
		ctx->priv = vsf_bufmgr_malloc(ctx->op->priv_size);
		if (!ctx->priv) return -VSFVMC_NOT_ENOUGH_RESOURCES;
		memset(ctx->priv, 0, ctx->op->priv_size);
	}

	ctx->pt.user_data = lexer;
	if (ctx->op->init != NULL)
	{
		ret = ctx->op->init(lexer);
		if (ret) return ret;
	}

	lexer->symid = VSFVM_LEXER_USER_SYMID;
	return 0;
}

int vsfvmc_lexer_ctx_delete(struct vsfvmc_lexer_t *lexer)
{
	struct vsfvmc_lexer_ctx_t *ctx;

	vsfvmc_lexer_ctx_fini(lexer);
	ctx = vsf_dynstack_pop(&lexer->ctx_stack, 1);
	if (!ctx) return -1;
	lexer->curctx = *ctx;
	return 0;
}

int vsfvmc_lexer_ctx_new(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_list_t *list)
{
	struct vsfvmc_lexer_ctx_t *ctx = &lexer->curctx;
	int err;

	if (ctx->op != NULL)
		vsf_dynstack_push(&lexer->ctx_stack, ctx, 1);
	err = vsfvmc_lexer_ctx_init(lexer, list);
	if (err) vsfvmc_lexer_ctx_delete(lexer);
	return err;
}

int vsfvmc_lexer_init(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_list_t *list, struct vsflist_t *ext)
{
	int ret;

	memset(&lexer->expr, 0, sizeof(lexer->expr));
	vsf_dynstack_init(&lexer->ctx_stack,
		sizeof(struct vsfvmc_lexer_ctx_t), 1, 4);
	vsf_dynstack_init(&lexer->expr.stack_exp,
		sizeof(struct vsfvmc_lexer_etoken_t), 4, 4);
	vsf_dynstack_init(&lexer->expr.stack_op,
		sizeof(struct vsfvmc_lexer_etoken_t), 4, 4);
	vsf_dynstack_init(&lexer->symtbl,
		sizeof(struct vsfvmc_lexer_symtbl_t), 1, 4);

	ret = vsfvmc_lexer_symtbl_new(lexer);
	if (ret) return ret;
	lexer->expr.context.pt.user_data = lexer;

	ret = vsfvmc_lexer_ctx_new(lexer, list);
	if (ret) goto cleanup_symtbl;

	lexer->ext = ext;
	return 0;
cleanup_symtbl:
	vsfvmc_lexer_symtbl_delete(lexer);
	return ret;
}

int vsfvmc_lexer_input(struct vsfvmc_lexer_t *lexer, const char *code)
{
	if (!lexer->curctx.op)
		return -VSFVMC_LEXER_INVALID_OP;

	lexer->curctx.pos = code;
	return lexer->curctx.op->input(lexer);
}
