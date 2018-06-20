#include "vsfvm_compiler.h"
#include "lexer/vsfvm_lexer.h"

struct vsfvmc_lexer_dart_t
{
	bool parse_enter;
	bool isconst;
	bool type_parsed;
	bool isref;
	struct vsfsm_pt_t pt_expr;

	struct vsfvmc_lexer_sym_t *type_sym;
	struct vsfvmc_lexer_etoken_t pre_etoken;
	struct vsfvmc_lexer_etoken_t etoken;
};

enum vsfvmc_dart_token_t
{
	VSFVMC_DART_TOKEN_NL = '\n',
	VSFVMC_DART_TOKEN_POUND = '#',
	VSFVMC_DART_TOKEN_DOT = '.',
	VSFVMC_DART_TOKEN_ASSIGN = '=',
	VSFVMC_DART_TOKEN_ADD = '+',
	VSFVMC_DART_TOKEN_SUB = '-',
	VSFVMC_DART_TOKEN_MUL = '*',
	VSFVMC_DART_TOKEN_DIV = '/',
	VSFVMC_DART_TOKEN_INTDIV = '~' + ('/' << 8),
	VSFVMC_DART_TOKEN_AND = '&',
	VSFVMC_DART_TOKEN_OR = '|',
	VSFVMC_DART_TOKEN_XOR = '^',
	VSFVMC_DART_TOKEN_MOD = '%',
	VSFVMC_DART_TOKEN_NOT = '!',
	VSFVMC_DART_TOKEN_GT = '>',
	VSFVMC_DART_TOKEN_LT = '<',
	VSFVMC_DART_TOKEN_COND = '?',
	VSFVMC_DART_TOKEN_COMMA = ',',
	VSFVMC_DART_TOKEN_SEMICOLON = ';',
	VSFVMC_DART_TOKEN_REV = '~',
	VSFVMC_DART_TOKEN_LBRACE = '{',
	VSFVMC_DART_TOKEN_RBRACE = '}',
	VSFVMC_DART_TOKEN_LGROUPING = '(',
	VSFVMC_DART_TOKEN_RGROUPING = ')',
	VSFVMC_DART_TOKEN_POINTER = '-' + ('>' << 8),
	VSFVMC_DART_TOKEN_2EQ = 0x10000,
	VSFVMC_DART_TOKEN_EQ = VSFVMC_DART_TOKEN_ASSIGN | VSFVMC_DART_TOKEN_2EQ,
	VSFVMC_DART_TOKEN_NE = VSFVMC_DART_TOKEN_NOT | VSFVMC_DART_TOKEN_2EQ,
	VSFVMC_DART_TOKEN_GE = VSFVMC_DART_TOKEN_GT | VSFVMC_DART_TOKEN_2EQ,
	VSFVMC_DART_TOKEN_LE = VSFVMC_DART_TOKEN_LT | VSFVMC_DART_TOKEN_2EQ,
	VSFVMC_DART_TOKEN_DOUBLE = 0x20000,
	VSFVMC_DART_TOKEN_LAND = VSFVMC_DART_TOKEN_AND | VSFVMC_DART_TOKEN_DOUBLE,
	VSFVMC_DART_TOKEN_LOR = VSFVMC_DART_TOKEN_OR | VSFVMC_DART_TOKEN_DOUBLE,
	VSFVMC_DART_TOKEN_SHL = VSFVMC_DART_TOKEN_LT | VSFVMC_DART_TOKEN_DOUBLE,
	VSFVMC_DART_TOKEN_SHR = VSFVMC_DART_TOKEN_GT | VSFVMC_DART_TOKEN_DOUBLE,
	VSFVMC_DART_TOKEN_NUM = 0x40000,

	VSFVMC_DART_TOKEN_SYM = 0x80000,
	VSFVMC_DART_TOKEN_SYM_VAR = VSFVMC_DART_TOKEN_SYM,
	VSFVMC_DART_TOKEN_SYM_CONST,
	VSFVMC_DART_TOKEN_SYM_REF,
	VSFVMC_DART_TOKEN_SYM_IMPORT,
	VSFVMC_DART_TOKEN_SYM_IF,
	VSFVMC_DART_TOKEN_SYM_ELSE,
	VSFVMC_DART_TOKEN_SYM_FOR,
	VSFVMC_DART_TOKEN_SYM_WHILE,
	VSFVMC_DART_TOKEN_SYM_DO,
	VSFVMC_DART_TOKEN_SYM_SWITCH,
	VSFVMC_DART_TOKEN_SYM_CASE,
	VSFVMC_DART_TOKEN_SYM_RETURN,
	VSFVMC_DART_TOKEN_SYM_BREAK,
	VSFVMC_DART_TOKEN_SYM_CONTINUE,
	VSFVMC_DART_TOKEN_SYM_THREAD,
};

#define VSFVM_LEXER_DART_USER_SYMID		(VSFVMC_DART_TOKEN_SYM | VSFVM_LEXER_USER_SYMID)

static vsf_err_t vsfvmc_dart_on_expr(struct vsfvmc_lexer_t *lexer,
	uint32_t token, struct vsfvmc_token_data_t *data, bool ref)
{
	if (token == VSFVMC_DART_TOKEN_SEMICOLON)		token = VSFVMC_TOKEN_SEMICOLON;
	else if (token == VSFVMC_DART_TOKEN_DOT)		token = VSFVMC_TOKEN_DOT;
	else if (token == VSFVMC_DART_TOKEN_COMMA)		token = VSFVMC_TOKEN_COMMA;
	else if (token == VSFVMC_DART_TOKEN_LGROUPING)	token = VSFVMC_TOKEN_LGROUPING;
	else if (token == VSFVMC_DART_TOKEN_RGROUPING)	token = VSFVMC_TOKEN_RGROUPING;
	else if (token == VSFVMC_DART_TOKEN_ASSIGN)		token = VSFVMC_TOKEN_ASSIGN;
	else if (token == VSFVMC_DART_TOKEN_NUM)		token = VSFVMC_TOKEN_NUM;
	else if (token == VSFVMC_DART_TOKEN_ADD)		token = VSFVMC_TOKEN_ADD;
	else if (token == VSFVMC_DART_TOKEN_SUB)		token = VSFVMC_TOKEN_SUB;
	else if (token == VSFVMC_DART_TOKEN_MUL)		token = VSFVMC_TOKEN_MUL;
	else if (token == VSFVMC_DART_TOKEN_DIV)		token = VSFVMC_TOKEN_DIV;
	else if (token == VSFVMC_DART_TOKEN_AND)		token = VSFVMC_TOKEN_AND;
	else if (token == VSFVMC_DART_TOKEN_OR)			token = VSFVMC_TOKEN_OR;
	else if (token == VSFVMC_DART_TOKEN_XOR)		token = VSFVMC_TOKEN_XOR;
	else if (token == VSFVMC_DART_TOKEN_REV)		token = VSFVMC_TOKEN_REV;
	else if (token == VSFVMC_DART_TOKEN_MOD)		token = VSFVMC_TOKEN_MOD;
	else if (token == VSFVMC_DART_TOKEN_NOT)		token = VSFVMC_TOKEN_NOT;
	else if (token == VSFVMC_DART_TOKEN_GT)			token = VSFVMC_TOKEN_GT;
	else if (token == VSFVMC_DART_TOKEN_LT) 		token = VSFVMC_TOKEN_LT;
	else if (token == VSFVMC_DART_TOKEN_EQ)			token = VSFVMC_TOKEN_EQ;
	else if (token == VSFVMC_DART_TOKEN_NE)			token = VSFVMC_TOKEN_NE;
	else if (token == VSFVMC_DART_TOKEN_GE)			token = VSFVMC_TOKEN_GE;
	else if (token == VSFVMC_DART_TOKEN_LE)			token = VSFVMC_TOKEN_LE;
	else if (token == VSFVMC_DART_TOKEN_LAND)		token = VSFVMC_TOKEN_LAND;
	else if (token == VSFVMC_DART_TOKEN_LOR)		token = VSFVMC_TOKEN_LOR;
	else if (token == VSFVMC_DART_TOKEN_SHL)		token = VSFVMC_TOKEN_SHL;
	else if (token == VSFVMC_DART_TOKEN_SHR)		token = VSFVMC_TOKEN_SHR;
	else if (token & VSFVMC_DART_TOKEN_SYM)
	{
		if ((data->sym->type == VSFVMC_LEXER_SYM_VARIABLE) ||
			(data->sym->type == VSFVMC_LEXER_SYM_EXTVAR))
		{
			if (ref)
				token = VSFVMC_TOKEN_VARIABLE_REF;
			else
				token = VSFVMC_TOKEN_VARIABLE;
		}
		else if (data->sym->type == VSFVMC_LEXER_SYM_CONST)
		{
			token = VSFVMC_TOKEN_NUM;
			data->ival = data->sym->ival;
		}
		else if ((data->sym->type == VSFVMC_LEXER_SYM_FUNCTION) ||
			(data->sym->type == VSFVMC_LEXER_SYM_EXTFUNC))
		{
			token = VSFVMC_TOKEN_FUNC_CALL;
		}
		else if (data->sym->type == VSFVMC_LEXER_SYM_STRING)
			token = VSFVMC_TOKEN_RESOURCES;
		else
			token = VSFVMC_TOKEN_SYMBOL;
	}
	else
		return -VSFVMC_NOT_SUPPORT;

	return vsfvmc_lexer_on_expr(lexer, token, data);
}

static bool vsfvmc_dart_is_type(struct vsfvmc_lexer_dart_t *dart,
	uint32_t token, struct vsfvmc_token_data_t *data)
{
	bool istype = (token >= VSFVM_LEXER_DART_USER_SYMID) &&
						(data->sym->type == VSFVMC_LEXER_SYM_EXTCLASS);
	dart->type_sym = istype ? data->sym : NULL;
	return istype;
}

static vsf_err_t vsfvmc_dart_parse_token(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
	uint32_t token, struct vsfvmc_token_data_t *data)
{
#define dart_next()		\
	do {\
		dart->pre_etoken.token = token;\
		if (data != NULL)\
			dart->pre_etoken.data = *data;\
		evt = VSFSM_EVT_INVALID;\
		vsfsm_pt_entry(pt);\
		if (VSFSM_EVT_INVALID == evt)\
			return err;\
	} while (0)

	struct vsfvmc_lexer_t *lexer = (struct vsfvmc_lexer_t *)pt->user_data;
	struct vsfvmc_lexer_dart_t *dart =
		(struct vsfvmc_lexer_dart_t *)lexer->curctx.priv;
	vsf_err_t err = VSFERR_NOT_READY;

#ifdef VSFVM_LEXER_DEBUG
	if (token & VSFVMC_DART_TOKEN_2EQ)
		VSFVM_LOG_INFO("token: %c=" VSFVM_LOG_LINEEND, token & 0xFF);
	else if (token & VSFVMC_DART_TOKEN_DOUBLE)
		VSFVM_LOG_INFO("token: %c%c" VSFVM_LOG_LINEEND, token & 0xFF,
						token & 0xFF);
	else if (token & VSFVMC_DART_TOKEN_NUM)
		VSFVM_LOG_INFO("token: %d" VSFVM_LOG_LINEEND, data->ival);
	else if (token & VSFVMC_DART_TOKEN_SYM)
		VSFVM_LOG_INFO("token: %s" VSFVM_LOG_LINEEND, data->sym->name);
	else if (token == '\n')
		VSFVM_LOG_INFO("token: \\n" VSFVM_LOG_LINEEND);
	else
		VSFVM_LOG_INFO("token: %s" VSFVM_LOG_LINEEND, (char *)&token);
#endif

	vsfsm_pt_begin(pt);

	while (1)
	{
		if (token == VSFVMC_DART_TOKEN_LBRACE)
		{
			err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_BLOCK_BEGIN, data);
			if (err < 0) goto dart_error;
			goto dart_wait_next;
		}
		else if (token == VSFVMC_DART_TOKEN_RBRACE)
		{
			err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_BLOCK_END, data);
			if (err < 0) goto dart_error;
			goto dart_wait_next;
		}
		else if (token == VSFVMC_DART_TOKEN_SYM_IMPORT)
		{
			dart_next();
			if (token < VSFVM_LEXER_DART_USER_SYMID)
			{
			dart_invalid_symbol:
				err = -VSFVMC_PARSER_UNEXPECTED_TOKEN;
				goto dart_error;
			}

			dart_next();
			if (token != VSFVMC_DART_TOKEN_SEMICOLON)
			{
				err = -VSFVMC_PARSER_UNEXPECTED_TOKEN;
				goto dart_error;
			}
			err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_IMPORT,
					&dart->pre_etoken.data);
			if (err < 0) goto dart_error;
			goto dart_wait_next;
		}
		else if ((token >= VSFVMC_DART_TOKEN_SYM_VAR) &&
			(token <= VSFVMC_DART_TOKEN_SYM_CONST))
		{
			dart->isconst = (token == VSFVMC_DART_TOKEN_SYM_CONST);
			dart->type_parsed = false;

		dart_wait_var_name:
			dart_next();
			if (!dart->type_parsed && vsfvmc_dart_is_type(dart, token, data) &&
				(dart->pre_etoken.token >= VSFVMC_DART_TOKEN_SYM_VAR))
			{
				dart_next();
			}
			dart->type_parsed = true;

			if (data->sym->type != VSFVMC_LEXER_SYM_UNKNOWN)
			{
				err = -VSFVMC_PARSER_ALREADY_DEFINED;
				goto dart_error;
			}
			if (token < VSFVM_LEXER_DART_USER_SYMID)
				goto dart_invalid_symbol;
			dart->etoken.data = *data;
			if (dart->type_sym != NULL)
				dart->etoken.data.sym->c = dart->type_sym->c;

			dart_next();
			if (dart->isconst && (token != VSFVMC_DART_TOKEN_ASSIGN))
			{
				err = -VSFVMC_PARSER_UNINITED_CONST;
				goto dart_error;
			}

			dart->etoken.data.sym->type = dart->isconst ?
				VSFVMC_LEXER_SYM_CONST : VSFVMC_LEXER_SYM_VARIABLE;
			if (token == VSFVMC_DART_TOKEN_ASSIGN)
			{
				lexer->expr.context.comma_is_op = false;
				dart_next();
				err = vsfvmc_dart_on_expr(lexer, token, data, false);
				if (err < 0) goto dart_error;
				else if (err > 0) return err;
				else
				{
					if (dart->isconst)
					{
						struct vsfvmc_lexer_etoken_t *etoken;
						if (lexer->expr.stack_exp.sp != 1)
						{
							err = -VSFVMC_PARSER_INVALID_CONST;
							goto dart_expr_error;
						}
						etoken = vsfvmc_lexer_expr_popexp(lexer);
						if (!etoken || (etoken->token != VSFVMC_TOKEN_NUM))
						{
							err = -VSFVMC_PARSER_INVALID_CONST;
							goto dart_expr_error;
						}

						dart->etoken.data.sym->ival = etoken->data.ival;
						err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_CONST,
							&dart->etoken.data);
						if (err < 0)
						{
						dart_expr_error:
							vsfvmc_lexer_expr_reset(lexer);
							goto dart_error;
						}
					}
					else
					{
						err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_VAR,
							&dart->etoken.data);
						if (err < 0) goto dart_expr_error;
					}
					vsfvmc_lexer_expr_reset(lexer);

					if (token == VSFVMC_DART_TOKEN_COMMA)
						goto dart_wait_var_name;
					else if (token == VSFVMC_DART_TOKEN_SEMICOLON)
						goto dart_wait_next;
					else
						goto dart_unexpected_token;
				}
			}
			else if (token == VSFVMC_DART_TOKEN_COMMA)
			{
				if (dart->type_sym)
					dart->etoken.data.sym->c = dart->type_sym->c;
				err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_VAR,
					&dart->etoken.data);
				if (err < 0) goto dart_error;
				goto dart_wait_var_name;
			}
			else if (token == VSFVMC_DART_TOKEN_SEMICOLON)
			{
				if (dart->pre_etoken.token >= VSFVM_LEXER_DART_USER_SYMID)
				{
					if (dart->type_sym)
						dart->etoken.data.sym->c = dart->type_sym->c;
					err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_VAR,
						&dart->etoken.data);
					if (err < 0) goto dart_expr_error;
				}
				goto dart_wait_next;
			}
			else
			{
			dart_unexpected_token:
				err = -VSFVMC_PARSER_UNEXPECTED_TOKEN;
				goto dart_error;
			}
		}
		else if ((token == VSFVMC_DART_TOKEN_SYM_IF) ||
				(token == VSFVMC_DART_TOKEN_SYM_WHILE))
		{
			dart->etoken.token = token == VSFVMC_DART_TOKEN_SYM_IF ?
				VSFVMC_TOKEN_IF : VSFVMC_TOKEN_WHILE;

			dart_next();
			if (token != VSFVMC_DART_TOKEN_LGROUPING)
				goto dart_unexpected_token;

			lexer->expr.context.comma_is_op = true;
			dart_next();
			dart->isref = false;
			if (token == VSFVMC_DART_TOKEN_SYM_REF)
			{
				dart_next();
				if ((token < VSFVM_LEXER_DART_USER_SYMID) ||
					(data->sym->type != VSFVMC_LEXER_SYM_VARIABLE))
				{
					goto dart_unexpected_token;
				}
				dart->isref = true;
			}
			err = vsfvmc_dart_on_expr(lexer, token, data, dart->isref);
			if (err < 0) goto dart_error;
			else if (err > 0) return err;
			else if (token != VSFVMC_DART_TOKEN_RGROUPING)
			{
			dart_expr_unwanted_token:
				vsfvmc_lexer_expr_reset(lexer);
				goto dart_unexpected_token;
			}

			dart_next();
			if (token != VSFVMC_DART_TOKEN_LBRACE)
				goto dart_expr_unwanted_token;

			err = vsfvmc_lexer_on_stmt(lexer, dart->etoken.token, NULL);
			if (err < 0) goto dart_expr_error;
			err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_BLOCK_BEGIN, data);
			if (err < 0) goto dart_expr_error;
		dart_expr_finished:
			vsfvmc_lexer_expr_reset(lexer);
			goto dart_wait_next;
		}
		else if (token == VSFVMC_DART_TOKEN_SYM_ELSE)
		{
			err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_ELSE, NULL);
			if (err < 0) goto dart_error;

			dart_next();
			if ((token == VSFVMC_DART_TOKEN_SYM_IF) ||
				(token == VSFVMC_DART_TOKEN_LBRACE))
			{
				continue;
			}
			goto dart_unexpected_token;
		}
		else if (token == VSFVMC_DART_TOKEN_SYM_RETURN)
		{
			lexer->expr.context.comma_is_op = true;
			dart_next();
			dart->isref = false;
			if (token == VSFVMC_DART_TOKEN_SYM_REF)
			{
				dart_next();
				if ((token < VSFVM_LEXER_DART_USER_SYMID) ||
					(data->sym->type != VSFVMC_LEXER_SYM_VARIABLE))
				{
					goto dart_unexpected_token;
				}
				dart->isref = true;
			}
			err = vsfvmc_dart_on_expr(lexer, token, data, dart->isref);
			if (err < 0) goto dart_error;
			else if (err > 0) return err;
			else if (token != VSFVMC_DART_TOKEN_SEMICOLON)
				goto dart_expr_unwanted_token;

			err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_RET, NULL);
			if (err < 0) goto dart_expr_error;
			goto dart_expr_finished;
		}
		else if ((token == VSFVMC_DART_TOKEN_SYM_FOR) ||
			(token == VSFVMC_DART_TOKEN_SYM_DO) ||
			(token == VSFVMC_DART_TOKEN_SYM_BREAK) ||
			(token == VSFVMC_DART_TOKEN_SYM_CONTINUE))
		{
			err = -VSFVMC_NOT_SUPPORT;
			goto dart_error;
		}
		else if (token & VSFVMC_DART_TOKEN_SYM)
		{
			if (data->sym->type == VSFVMC_LEXER_SYM_UNKNOWN)
			{
				dart_next();
				if (token != VSFVMC_DART_TOKEN_LGROUPING)
					goto dart_unexpected_token;

				dart->etoken.data = dart->pre_etoken.data;
				dart->etoken.data.sym->type = VSFVMC_LEXER_SYM_FUNCTION;
				vsfvmc_lexer_symtbl_new(lexer);

				while (1)
				{
					dart_next();
					if (token == VSFVMC_DART_TOKEN_RGROUPING)
					{
						if ((dart->pre_etoken.token < VSFVM_LEXER_DART_USER_SYMID) &&
							(dart->pre_etoken.token !=
								VSFVMC_DART_TOKEN_LGROUPING))
						{
							goto dart_unexpected_token;
						}

						dart_next();
						if (token == VSFVMC_DART_TOKEN_SEMICOLON)
						{
							// function declare, not supported
							// function MUST be implemented before use
							vsfvmc_lexer_symtbl_delete(lexer);
							vsfvmc_lexer_expr_reset(lexer);
							goto dart_unexpected_token;
						}
						else if (token == VSFVMC_DART_TOKEN_LBRACE)
						{
							dart->etoken.data.sym->func.param_num =
								lexer->expr.stack_exp.sp;
							err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_FUNC,
								&dart->etoken.data);
							if (err < 0) goto dart_expr_error;
							err = vsfvmc_lexer_on_stmt(lexer,
								VSFVMC_TOKEN_BLOCK_BEGIN, NULL);
							if (err < 0) goto dart_expr_error;
							vsfvmc_lexer_symtbl_delete(lexer);
							goto dart_expr_finished;
						}
						else goto dart_unexpected_token;
					}
					else if (token == VSFVMC_DART_TOKEN_COMMA)
					{
						if (dart->pre_etoken.token < VSFVM_LEXER_DART_USER_SYMID)
						{
							goto dart_unexpected_token;
						}
					}
					else if (vsfvmc_dart_is_type(dart, token, data))
					{
						if ((dart->pre_etoken.token != VSFVMC_DART_TOKEN_LGROUPING) &&
							(dart->pre_etoken.token != VSFVMC_DART_TOKEN_COMMA))
						{
							goto dart_unexpected_token;
						}
					}
					else if (token >= VSFVM_LEXER_DART_USER_SYMID)
					{
						if ((dart->pre_etoken.token != VSFVMC_DART_TOKEN_LGROUPING) &&
							(dart->pre_etoken.token != VSFVMC_DART_TOKEN_COMMA) &&
							!vsfvmc_dart_is_type(dart, dart->pre_etoken.token, &dart->pre_etoken.data))
						{
							goto dart_unexpected_token;
						}
						data->sym->type = VSFVMC_LEXER_SYM_VARIABLE;
						if (dart->type_sym)
							data->sym->c = dart->type_sym->c;
						vsfvmc_lexer_expr_pushexp(lexer, VSFVMC_TOKEN_VARIABLE, data);
					}
					else
						goto dart_unexpected_token;
				}
			}
			else
				goto dart_expr;
		}
		else
		{
		dart_expr:
			lexer->expr.context.comma_is_op = true;
			while (1)
			{
				dart->isref = false;
				if (token == VSFVMC_DART_TOKEN_SYM_REF)
				{
					dart_next();
					if ((token < VSFVM_LEXER_DART_USER_SYMID) ||
						(data->sym->type != VSFVMC_LEXER_SYM_VARIABLE))
					{
						goto dart_unexpected_token;
					}
					dart->isref = true;
				}
				err = vsfvmc_dart_on_expr(lexer, token, data, dart->isref);
				if (err < 0) goto dart_error;
				else if (!err)
				{
					if (token != VSFVMC_DART_TOKEN_SEMICOLON)
					{
						err = -VSFVMC_PARSER_UNEXPECTED_TOKEN;
						goto dart_expr_error;
					}
					err = vsfvmc_lexer_on_stmt(lexer, VSFVMC_TOKEN_EXPR, NULL);
					if (err < 0) goto dart_expr_error;
					goto dart_expr_finished;
				}
				else dart_next();
			}
		}

	dart_error:
		dart->parse_enter = true;
		while (token != VSFVMC_DART_TOKEN_NL)
			dart_next();
		dart->parse_enter = false;
		return VSFERR_FAIL;

	dart_wait_next:
		err = VSFERR_NONE;
		dart_next();
	}

	vsfsm_pt_end(pt);
	return VSFERR_NONE;
}

static bool vsfvmc_dart_is_space(int ch)
{
	return isspace(ch);
}

static const char *vsfvmc_dart_token_1char = ",;()[]{}?:.";
static const char *vsfvmc_dart_token_double = "+-|&<>";		// ?? not supported
static const char *vsfvmc_dart_token_2eq = "+-*/^%|&<>=!";
static int vsfvmc_dart_input(struct vsfvmc_lexer_t *lexer)
{
	struct vsfvmc_lexer_dart_t *dart =
		(struct vsfvmc_lexer_dart_t *)lexer->curctx.priv;
	struct vsfvmc_token_data_t data;
	uint32_t token, token_next;
	uint32_t value;
	uint16_t id;
	int err = 0;

	while (token = vsfvmc_lexer_getchar(lexer))
	{
		token_next = vsfvmc_lexer_peekchar(lexer);
		if (token == '\r')
		{
		}
		else if (token == '\n')
		{
			if (dart->parse_enter)
			{
				dart->parse_enter = false;
				err = vsfvmc_lexer_on_token(lexer, token, NULL);
				if (err < 0) return err;
			}
		}
		else if (strchr(vsfvmc_dart_token_1char, token))
		{
			err = vsfvmc_lexer_on_token(lexer, token, NULL);
			if (err < 0) return err;
		}
		else if (((token >= 'a') && (token <= 'z')) ||
			((token >= 'A') && (token <= 'Z')) || (token == '_'))
		{
			value = 0;
			lexer->cur_symbol[value++] = (char)token;
			while (((token_next >= 'a') && (token_next <= 'z')) ||
				((token_next >= 'A') && (token_next <= 'Z')) ||
				((token_next >= '0') && (token_next <= '9')) ||
				(token_next == '_'))
			{
				token = vsfvmc_lexer_getchar(lexer);
				token_next = vsfvmc_lexer_peekchar(lexer);
				lexer->cur_symbol[value++] = (char)token;
				if (value >= sizeof(lexer->cur_symbol))
					return -VSFVMC_LEXER_SYMBOL_TOO_LONG;
			}
			lexer->cur_symbol[value++] = '\0';

			data.sym = vsfvmc_lexer_symtbl_add(lexer, lexer->cur_symbol, &id);
			if (!data.sym) return -VSFVMC_NOT_ENOUGH_RESOURCES;

			err = vsfvmc_lexer_on_token(lexer, VSFVMC_DART_TOKEN_SYM | id, &data);
			if (err < 0) return err;
		}
		else if ((token >= '0') && (token <= '9'))
		{
			data.ival = 0;
			if ((token == '0') && ((token_next == 'x') || (token_next == 'X')))
			{
				vsfvmc_lexer_getchar(lexer);
				token_next = vsfvmc_lexer_peekchar(lexer);

				while (((token_next >= '0') && (token_next <= '9')) ||
					((token_next >= 'a') && (token_next <= 'f')) ||
					((token_next >= 'A') && (token_next <= 'F')))
				{
					token = vsfvmc_lexer_getchar(lexer);
					token_next = vsfvmc_lexer_peekchar(lexer);
					data.ival <<= 4;
					data.ival += (token & 15) + (token >= 'A' ? 9 : 0);
				}
			}
			else if ((token == '0') &&
				((token_next == 'b') || (token_next == 'B')))
			{
				vsfvmc_lexer_getchar(lexer);
				token_next = vsfvmc_lexer_peekchar(lexer);

				while ((token_next >= '0') && (token_next <= '1'))
				{
					token = vsfvmc_lexer_getchar(lexer);
					token_next = vsfvmc_lexer_peekchar(lexer);
					data.ival <<= 1;
					data.ival += token - '0';
				}
			}
			else
			{
				data.ival = token - '0';
				while ((token_next >= '0') && (token_next <= '9'))
				{
					token = vsfvmc_lexer_getchar(lexer);
					token_next = vsfvmc_lexer_peekchar(lexer);
					data.ival *= 10;
					data.ival += token - '0';
				}
			}

			err = vsfvmc_lexer_on_token(lexer, VSFVMC_DART_TOKEN_NUM, &data);
			if (err < 0) return err;
		}
		else if (token == '/')
		{
			if (token_next == '/')
				while ((token != '\n') && (token != '\0'))
					token = vsfvmc_lexer_getchar(lexer);
			else if (token_next == '*')
			{
				vsfvmc_lexer_getchar(lexer);
				while (!((token == '*') && (token_next == '/')))
				{
					token = vsfvmc_lexer_getchar(lexer);
					token_next = vsfvmc_lexer_peekchar(lexer);
				}
				vsfvmc_lexer_getchar(lexer);
			}
			else
				goto parse_2eq;
		}
		else if ((token == '"') || (token == '\''))
		{
			value = 0;
			while (1) {
				token_next = vsfvmc_lexer_getchar(lexer);
				if (token_next == token)
				{
					token_next = vsfvmc_lexer_peekchar(lexer);
					while (vsfvmc_dart_is_space(token_next))
					{
						vsfvmc_lexer_getchar(lexer);
						token_next = vsfvmc_lexer_peekchar(lexer);
					}
					if (token_next == token)
					{
						vsfvmc_lexer_getchar(lexer);
						continue;
					}
					break;
				}
				else if (token_next == '\\')
				{
				check_next:
					token_next = vsfvmc_lexer_getchar(lexer);
					if (token_next == '\0')			goto dart_unclosed_string;
					else if (token_next == 'a')		token_next = '\a';
					else if (token_next == 'b')		token_next = '\b';
					else if (token_next == 'f')		token_next = '\f';
					else if (token_next == 'n')		token_next = '\n';
					else if (token_next == 'r')		token_next = '\r';
					else if (token_next == 't')		token_next = '\t';
					else if (token_next == 'v')		token_next = '\v';
					else if (token_next == '\\')	token_next = '\\';
					else if (token_next == '\'')	token_next = '\'';
					else if (token_next == '\"')	token_next = '\"';
					else if (token_next == '?')		token_next = '\?';
					else if (token_next == '0')		token_next = '\0';
					else if (token_next == '\r')	goto check_next;
					else if (token_next == '\n')	continue;
					else return -VSFVMC_LEXER_INVALID_ESCAPE;
				}
				else if (token_next == '\r')
					continue;
				else if ((token_next == '\0') || (token_next == '\n'))
				{
				dart_unclosed_string:
					return -VSFVMC_LEXER_INVALID_STRING;
				}
				lexer->cur_symbol[value++] = (char)token_next;
			}
			lexer->cur_symbol[value++] = '\0';

			data.sym = vsfvmc_lexer_symtbl_add(lexer, lexer->cur_symbol, &id);
			if (!data.sym) return -VSFVMC_NOT_ENOUGH_RESOURCES;
			data.sym->type = VSFVMC_LEXER_SYM_STRING;
			data.sym->length = strlen(data.sym->name) + 1;

			err = vsfvmc_lexer_on_token(lexer, VSFVMC_DART_TOKEN_SYM | id, &data);
			if (err < 0) return err;
		}
		else if (strchr(vsfvmc_dart_token_double, token))
		{
			if (token == token_next)
			{
				vsfvmc_lexer_getchar(lexer);
				token |= VSFVMC_DART_TOKEN_DOUBLE;
				if (((token == '<') || (token == '>')) &&
					(vsfvmc_lexer_peekchar(lexer) == '='))
				{
					token |= VSFVMC_DART_TOKEN_2EQ;
					err = vsfvmc_lexer_on_token(lexer, token, NULL);
				}
				else
					err = vsfvmc_lexer_on_token(lexer, token, NULL);
				if (err < 0) return err;
			}
			else
				goto parse_2eq;
		}
		else if (strchr(vsfvmc_dart_token_2eq, token))
		{
		parse_2eq:
			if (token_next == '=')
			{
				vsfvmc_lexer_getchar(lexer);
				token |= VSFVMC_DART_TOKEN_2EQ;
			}

			err = vsfvmc_lexer_on_token(lexer, token, NULL);
			if (err < 0) return err;
		}
		else if (token == '~')
		{
			// ~ ~/ ~/=
			if (token_next != '/')
			{
				err = vsfvmc_lexer_on_token(lexer, token, NULL);
				if (err < 0) return err;
			}
			else
			{
				vsfvmc_lexer_getchar(lexer);
				token_next = vsfvmc_lexer_peekchar(lexer);
				if (token_next != '=')
				{
					err = vsfvmc_lexer_on_token(lexer, VSFVMC_DART_TOKEN_INTDIV,
						NULL);
					if (err < 0) return err;
				}
				else
				{
					vsfvmc_lexer_getchar(lexer);
					err = vsfvmc_lexer_on_token(lexer,
						VSFVMC_DART_TOKEN_INTDIV | VSFVMC_DART_TOKEN_2EQ, NULL);
					if (err < 0) return err;
				}
			}
		}
	}
	return err;
}

const struct vsfvmc_lexer_sym_t vsfvmc_lexer_dart_keyword[] =
{
	VSFVM_LEXERSYM_KEYWORKD("var"),
	VSFVM_LEXERSYM_KEYWORKD("const"),
	VSFVM_LEXERSYM_KEYWORKD("ref"),
	VSFVM_LEXERSYM_KEYWORKD("import"),
	VSFVM_LEXERSYM_KEYWORKD("if"),
	VSFVM_LEXERSYM_KEYWORKD("else"),
	VSFVM_LEXERSYM_KEYWORKD("for"),
	VSFVM_LEXERSYM_KEYWORKD("while"),
	VSFVM_LEXERSYM_KEYWORKD("do"),
	VSFVM_LEXERSYM_KEYWORKD("switch"),
	VSFVM_LEXERSYM_KEYWORKD("case"),
	VSFVM_LEXERSYM_KEYWORKD("return"),
	VSFVM_LEXERSYM_KEYWORKD("break"),
	VSFVM_LEXERSYM_KEYWORKD("continue"),
	VSFVM_LEXERSYM_FUNCTION("thread", -1),
};

const struct vsfvmc_lexer_op_t vsfvmc_lexer_op_dart =
{
	.name = "dart",
	.ext = "dart",
	.priv_size = sizeof(struct vsfvmc_lexer_dart_t),

	.keyword.sym = (struct vsfvmc_lexer_sym_t *)vsfvmc_lexer_dart_keyword,
	.keyword.num = dimof(vsfvmc_lexer_dart_keyword),
	.keyword.id = 0,

	.parse_token = vsfvmc_dart_parse_token,
	.input = vsfvmc_dart_input,
};
