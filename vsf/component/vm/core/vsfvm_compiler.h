
#ifndef __VSFVM_COMPILER_H_INCLUDED__
#define __VSFVM_COMPILER_H_INCLUDED__

#include "vsf.h"
#include "vsfvm_code.h"
#include "vsfvm_common.h"

enum vsfvmc_token_t
{
	VSFVMC_TOKEN_NONE = 0,
	VSFVMC_TOKEN_SYMBOL,

	VSFVMC_TOKEN_STMT_START,
	VSFVMC_TOKEN_IMPORT,
	VSFVMC_TOKEN_VAR,					// define a variable
	VSFVMC_TOKEN_FUNC,					// define a function
	VSFVMC_TOKEN_CONST,
	VSFVMC_TOKEN_IF,
	VSFVMC_TOKEN_ELSE,
	VSFVMC_TOKEN_WHILE,
	VSFVMC_TOKEN_RET,
	VSFVMC_TOKEN_BREAK,
	VSFVMC_TOKEN_CONTINUE,
	VSFVMC_TOKEN_BLOCK_BEGIN,
	VSFVMC_TOKEN_BLOCK_END,
	VSFVMC_TOKEN_EXPR,
	VSFVMC_TOKEN_STMT_END,

	VSFVMC_TOKEN_EXPR_START,
	VSFVMC_TOKEN_EXPR_TERMINATOR,		// to terminate a expression
	VSFVMC_TOKEN_SEMICOLON,
	VSFVMC_TOKEN_RGROUPING,
	VSFVMC_TOKEN_COMMA,
	VSFVMC_TOKEN_EXPR_TERMINATOR_END,
	VSFVMC_TOKEN_LGROUPING,

	VSFVMC_TOKEN_EXPR_OPERAND,
	VSFVMC_TOKEN_EXPR_OPERAND_CONST,
	VSFVMC_TOKEN_NUM,
	VSFVMC_TOKEN_RESOURCES,
	VSFVMC_TOKEN_FUNC_ID,				// use a function as a value
	VSFVMC_TOKEN_EXPR_OPERAND_VARIABLE,
	VSFVMC_TOKEN_VARIABLE,				// from lexer
	VSFVMC_TOKEN_VARIABLE_REF,
	VSFVMC_TOKEN_VAR_ID = VSFVMC_TOKEN_VARIABLE,
	VSFVMC_TOKEN_VAR_ID_REF = VSFVMC_TOKEN_VARIABLE_REF,
										// use a variable

	VSFVMC_TOKEN_OPERATOR,
	VSFVMC_TOKEN_OP_PRIO1 = 0x100,
	VSFVMC_TOKEN_FUNC_CALL,
	VSFVMC_TOKEN_DOT,
	VSFVMC_TOKEN_REF,

	VSFVMC_TOKEN_UNARY_OP,				// unary op
	VSFVMC_TOKEN_OP_PRIO2 = 0x200,
	VSFVMC_TOKEN_NOT,
	VSFVMC_TOKEN_REV,
	VSFVMC_TOKEN_NEGA,					// negative sign
	VSFVMC_TOKEN_POSI,					// positive sign
	VSFVMC_TOKEN_SIZEOF,
	VSFVMC_TOKEN_ADDR,
	VSFVMC_TOKEN_PTR,

	VSFVMC_TOKEN_BINARY_OP,				// binary operator
	VSFVMC_TOKEN_OP_PRIO3 = 0x300,
	VSFVMC_TOKEN_MUL,
	VSFVMC_TOKEN_DIV,
	VSFVMC_TOKEN_MOD,

	VSFVMC_TOKEN_OP_PRIO4 = 0x400,
	VSFVMC_TOKEN_ADD,
	VSFVMC_TOKEN_SUB,

	VSFVMC_TOKEN_OP_PRIO5 = 0x500,
	VSFVMC_TOKEN_SHL,
	VSFVMC_TOKEN_SHR,

	VSFVMC_TOKEN_OP_PRIO6 = 0x600,
	VSFVMC_TOKEN_LT,
	VSFVMC_TOKEN_LE,
	VSFVMC_TOKEN_GT,
	VSFVMC_TOKEN_GE,

	VSFVMC_TOKEN_OP_PRIO7 = 0x700,
	VSFVMC_TOKEN_EQ,
	VSFVMC_TOKEN_NE,

	VSFVMC_TOKEN_OP_PRIO8 = 0x800,
	VSFVMC_TOKEN_AND,

	VSFVMC_TOKEN_OP_PRIO9 = 0x900,
	VSFVMC_TOKEN_XOR,

	VSFVMC_TOKEN_OP_PRIO10 = 0xA00,
	VSFVMC_TOKEN_OR,

	VSFVMC_TOKEN_OP_PRIO11 = 0xB00,
	VSFVMC_TOKEN_LAND,

	VSFVMC_TOKEN_OP_PRIO12 = 0xC00,
	VSFVMC_TOKEN_LOR,

	VSFVMC_TOKEN_OP_PRIO13 = 0xD00,
	VSFVMC_TOKEN_COND,
	VSFVMC_TOKEN_COLON,

	VSFVMC_TOKEN_OP_PRIO14 = 0xE00,
	VSFVMC_TOKEN_ASSIGN,

	VSFVMC_TOKEN_OP_PRIO15 = 0xF00,
	VSFVMC_TOKEN_COMMA_OP = VSFVMC_TOKEN_COMMA + VSFVMC_TOKEN_OP_PRIO15,

	VSFVMC_TOKEN_OP_PRIO16 = 0x1000,
	VSFVMC_TOKEN_OP_PRIO_MAX = VSFVMC_TOKEN_OP_PRIO16,
	VSFVMC_TOKEN_OP_PRIO_MASK = 0xFF00,

	VSFVMC_TOKEN_EXPR_END,
};

enum vsfvmc_errcode_t
{
	VSFVMC_ERRCODE_NONE = 0,

	// common error
	VSFVMC_BUG,
	VSFVMC_NOT_ENOUGH_RESOURCES,
	VSFVMC_FATAL_ERROR,			// fatal error above
	VSFVMC_NOT_SUPPORT,

	// lexer error
	VSFVMC_LEXER_NOT_SUPPORT,
	VSFVMC_LEXER_INVALID_OP,
	VSFVMC_LEXER_INVALID_STRING,
	VSFVMC_LEXER_INVALID_ESCAPE,
	VSFVMC_LEXER_SYMBOL_TOO_LONG,

	// parser error
	VSFVMC_PARSER_UNEXPECTED_TOKEN,
	VSFVMC_PARSER_ALREADY_DEFINED,
	VSFVMC_PARSER_INVALID_CLOSURE,
	VSFVMC_PARSER_INVALID_EXPR,
	VSFVMC_PARSER_UNINITED_CONST,
	VSFVMC_PARSER_INVALID_CONST,
	VSFVMC_PARSER_DIV0,
	VSFVMC_PARSER_EXPECT_FUNC_PARAM,
	VSFVMC_PARSER_TOO_MANY_FUNC_PARAM,
	VSFVMC_PARSER_MEMFUNC_NOT_FOUND,

	// compiler error
	VSFVMC_COMPILER_INVALID_MODULE,
	VSFVMC_COMPILER_INVALID_FUNC,
	VSFVMC_COMPILER_INVALID_FUNC_PARAM,
	VSFVMC_COMPILER_FAIL_USRLIB,

	VSFVMC_ERRCODE_END,
};

#include "lexer/vsfvm_lexer.h"

struct vsfvmc_func_ctx_t
{
	struct vsfvmc_lexer_etoken_t etoken;
	int symtbl_idx;
	int block_level;
	union
	{
		struct
		{
			int if_anchor;
			int else_anchor;
		} if_ctx;
		struct
		{
			int if_anchor;
			int calc_anchor;
		} while_ctx;
		struct
		{
			int goto_anchor;
		} func_ctx;
	};
};

struct vsfvmc_linktbl_t
{
	int bytecode_pos;
	enum
	{
		VSFVMC_LINKTBL_STR,
	} type;
	struct vsfvmc_lexer_sym_t *sym;
};

struct vsfvmc_func_t
{
	struct vsfvmc_lexer_sym_t *name;
	int symtbl_idx;
	int block_level;

	struct vsflist_t arglist;
	struct vsflist_t varlist;

	struct vsf_dynstack_t linktbl;
	struct vsfvmc_func_ctx_t curctx;
	struct vsf_dynstack_t ctx;
};

struct vsfvmc_script_t
{
	const char *name;
	struct vsfvmc_lexer_t lexer;
	struct vsflist_t ext;

	struct vsfsm_pt_t pt_stmt;
	struct vsfvmc_func_t cur_func;

	struct vsf_dynstack_t func_stack;
};

struct vsfvmc_t;
typedef int (*require_usrlib_t)(void *param, struct vsfvmc_t *vsfvmc, char *path);
struct vsfvmc_t
{
	struct
	{
		void *param;
		require_usrlib_t require_lib;
	} cb;

	struct vsfvmc_script_t script;
	struct vsf_dynstack_t bytecode;

	struct vsflist_t ext;
	struct vsflist_t lexer_list;
};

int vsfvmc_register_lexer(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_lexer_list_t *lexer_list);
int vsfvmc_register_ext(struct vsfvmc_t *vm, const struct vsfvm_ext_op_t *op);
int vsfvmc_script(struct vsfvmc_t *vsfvmc, const char *script_name);
int vsfvmc_init(struct vsfvmc_t *vsfvmc, void *param,
	require_usrlib_t require_lib);
void vsfvmc_fini(struct vsfvmc_t *vsfvmc);
int vsfvmc_input(struct vsfvmc_t *vsfvmc, const char *code);

#endif		// __VSFVM_COMPILER_H_INCLUDED__
