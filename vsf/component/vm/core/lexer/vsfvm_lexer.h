
#ifndef __VSFVM_LEXER_H_INCLUDED__
#define __VSFVM_LEXER_H_INCLUDED__

#include "vsf.h"
#include "vsfvm_code.h"
#include "vsfvm_common.h"

#ifndef VSFVMC_MAX_SYMLEN
#define VSFVMC_MAX_SYMLEN		256
#endif

#define VSFVM_LEXER_USER_SYMID	0x100

struct vsfvmc_lexer_symarr_t
{
	struct vsfvmc_lexer_sym_t *sym;
	uint16_t num;
	uint16_t id;
};

struct vsfvmc_lexer_symtbl_t
{
	struct vsf_dynarr_t table;
	struct vsf_dynarr_t strpool;
	char *strbuf;
	char *strpos;
	uint8_t varnum;
};

// expression token
struct vsfvmc_token_data_t
{
	union
	{
		uint32_t uval;
		int32_t ival;
		struct vsfvmc_lexer_sym_t *sym;
	};
};

struct vsfvmc_lexer_etoken_t
{
	uint32_t token;
	struct vsfvmc_token_data_t data;
};

struct vsfvmc_lexer_expr_t
{
	struct vsfvmc_lexer_exprctx_t
	{
		struct vsfvmc_lexer_etoken_t pre_etoken;
		bool comma_is_op;
		uint8_t func_param;
		uint32_t opsp;
		uint32_t expsp;
		struct vsfsm_pt_t pt;
	} context;
	uint32_t nesting;

	struct vsf_dynstack_t stack_exp;
	struct vsf_dynstack_t stack_op;
};

struct vsfvmc_lexer_sym_t
{
	char *name;

	union
	{
		uint16_t id;
		const struct vsfvm_ext_op_t *op;
	};

	enum
	{
		VSFVMC_LEXER_SYM_UNKNOWN = 0,
		VSFVMC_LEXER_SYM_GRAMA,		// keywords in extensions
		VSFVMC_LEXER_SYM_KEYWORD,

		VSFVMC_LEXER_SYM_FUNCTION,
		VSFVMC_LEXER_SYM_EXTFUNC,
		VSFVMC_LEXER_SYM_EXTCLASS,
		VSFVMC_LEXER_SYM_STRING,

		VSFVMC_LEXER_SYM_OPRAND,
		VSFVMC_LEXER_SYM_VARIABLE,
		VSFVMC_LEXER_SYM_CONST,
		VSFVMC_LEXER_SYM_EXTVAR,
	} type;

	union
	{
		int32_t ival;
		uint32_t uval;
		uint32_t length;			// for resources(string)
		struct
		{
			signed pos : 16;
			signed param_num : 8;
			const struct vsfvm_class_t *retc;
		} func;						// for functions
	};
	const struct vsfvm_class_t *c;

	struct vsflist_t list;
};

struct vsfvmc_lexer_ctx_t
{
	const struct vsfvmc_lexer_op_t *op;

	const char *pos;
	int line;
	int col;

	struct vsfsm_pt_t pt;
	void *priv;
};

struct vsfvmc_lexer_t
{
	struct vsfvmc_lexer_ctx_t curctx;
	struct vsf_dynstack_t ctx_stack;
	struct vsflist_t *ext;

	struct vsf_dynstack_t symtbl;
	struct vsfvmc_lexer_expr_t expr;
	char cur_symbol[VSFVMC_MAX_SYMLEN];
	int symid;
};

struct vsfvmc_lexer_op_t
{
	const char *name;
	const char *ext;
	uint16_t priv_size;

	const struct vsfvmc_lexer_symarr_t keyword;

	vsf_err_t (*parse_token)(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
		uint32_t token, struct vsfvmc_token_data_t *data);
	int (*init)(struct vsfvmc_lexer_t *lexer);
	int (*input)(struct vsfvmc_lexer_t *lexer);
};

struct vsfvmc_lexer_list_t
{
	struct vsfvmc_lexer_op_t *op;
	struct vsflist_t list;
};

// for specified lexer
#define vsfvmc_lexer_peekchar(lexer)		(*(lexer)->curctx.pos)
int vsfvmc_lexer_getchar(struct vsfvmc_lexer_t *lexer);

int vsfvmc_lexer_on_token(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data);
struct vsfvmc_lexer_sym_t *vsfvmc_lexer_symtbl_get(
	struct vsfvmc_lexer_t *lexer, char *symbol, uint16_t *id);
struct vsfvmc_lexer_sym_t *vsfvmc_lexer_symtbl_add(
	struct vsfvmc_lexer_t *lexer, char *symbol, uint16_t *id);
int vsfvmc_lexer_symtbl_new(struct vsfvmc_lexer_t *lexer);
int vsfvmc_lexer_symtbl_delete(struct vsfvmc_lexer_t *lexer);

// expression
int vsfvmc_lexer_expr_pushexp(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data);
int vsfvmc_lexer_expr_pushop(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data);
struct vsfvmc_lexer_etoken_t *vsfvmc_lexer_expr_popexp(
	struct vsfvmc_lexer_t *lexer);
struct vsfvmc_lexer_etoken_t *vsfvmc_lexer_expr_popop(
	struct vsfvmc_lexer_t *lexer);
vsf_err_t vsfvmc_lexer_on_expr(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data);
vsf_err_t vsfvmc_lexer_on_stmt(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data);
int vsfvmc_lexer_expr_reset(struct vsfvmc_lexer_t *lexer);

// for compiler
int vsfvmc_lexer_init(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_list_t *list, struct vsflist_t *ext);
void vsfvmc_lexer_fini(struct vsfvmc_lexer_t *lexer);
int vsfvmc_lexer_input(struct vsfvmc_lexer_t *lexer, const char *code);

int vsfvmc_lexer_ctx_new(struct vsfvmc_lexer_t *lexer,
	struct vsfvmc_lexer_list_t *list);
int vsfvmc_lexer_ctx_delete(struct vsfvmc_lexer_t *lexer);

// from vsfvm_compiler.c
vsf_err_t vsfvmc_on_stmt(struct vsfvmc_lexer_t *lexer, uint32_t token,
	struct vsfvmc_token_data_t *data);

#endif		// __VSFVM_LEXER_H_INCLUDED__
