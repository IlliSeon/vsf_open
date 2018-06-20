#include "vsf.h"
#include "vsfvm_code.h"
#include "vsfvm_common.h"

static const char *vsfvmc_variable_id[] =
{
	TO_STR(VSFVM_CODE_VARIABLE_NORMAL),
	TO_STR(VSFVM_CODE_VARIABLE_REFERENCE),
	TO_STR(VSFVM_CODE_VARIABLE_REFERENCE_NOTRACE),
	TO_STR(VSFVM_CODE_VARIABLE_RESOURCES),
	TO_STR(VSFVM_CODE_VARIABLE_FUNCTION),
};

static const char *vsfvmc_variable_pos[] =
{
	TO_STR(VSFVM_CODE_VARIABLE_POS_LOCAL),
	TO_STR(VSFVM_CODE_VARIABLE_POS_STACK_BEGIN),
	TO_STR(VSFVM_CODE_VARIABLE_POS_STACK_END),
	TO_STR(VSFVM_CODE_VARIABLE_POS_FUNCARG),
	TO_STR(VSFVM_CODE_VARIABLE_POS_FUNCAUTO),
};

static const char *vsfvmc_funtion_id[] =
{
	TO_STR(VSFVM_CODE_FUNCTION_SCRIPT),
	TO_STR(VSFVM_CODE_FUNCTION_EXT),
	TO_STR(VSFVM_CODE_FUNCTION_THREAD),
};

void vsfvm_tkdump(uint32_t token)
{
	uint32_t type = VSFVM_CODE_TYPE(token);
	uint32_t id = VSFVM_CODE_ID(token);
	uint32_t arg24 = VSFVM_CODE_ARG24(token);
	uint32_t arg8 = VSFVM_CODE_ARG8(token);
	uint32_t arg16 = VSFVM_CODE_ARG16(token);
	int32_t value = VSFVM_CODE_VALUE(token);

	VSFVM_LOG_INFO("0x%04X:", token);
	switch (type)
	{
	case VSFVM_CODE_TYPE_SYMBOL:
#define case_print_symbol(sym)			case sym: VSFVM_LOG_INFO("VSFVM_SYMBOL(" TO_STR(sym) ", %d, %d)," VSFVM_LOG_LINEEND, arg8, arg16); break
		switch (id)
		{
			case_print_symbol(VSFVM_CODE_SYMBOL_NOT);
			case_print_symbol(VSFVM_CODE_SYMBOL_REV);
			case_print_symbol(VSFVM_CODE_SYMBOL_NEGA);
			case_print_symbol(VSFVM_CODE_SYMBOL_POSI);
			case_print_symbol(VSFVM_CODE_SYMBOL_MUL);
			case_print_symbol(VSFVM_CODE_SYMBOL_DIV);
			case_print_symbol(VSFVM_CODE_SYMBOL_MOD);
			case_print_symbol(VSFVM_CODE_SYMBOL_ADD);
			case_print_symbol(VSFVM_CODE_SYMBOL_SUB);
			case_print_symbol(VSFVM_CODE_SYMBOL_AND);
			case_print_symbol(VSFVM_CODE_SYMBOL_OR);
			case_print_symbol(VSFVM_CODE_SYMBOL_XOR);
			case_print_symbol(VSFVM_CODE_SYMBOL_EQ);
			case_print_symbol(VSFVM_CODE_SYMBOL_NE);
			case_print_symbol(VSFVM_CODE_SYMBOL_GT);
			case_print_symbol(VSFVM_CODE_SYMBOL_GE);
			case_print_symbol(VSFVM_CODE_SYMBOL_LT);
			case_print_symbol(VSFVM_CODE_SYMBOL_LE);
			case_print_symbol(VSFVM_CODE_SYMBOL_LAND);
			case_print_symbol(VSFVM_CODE_SYMBOL_LOR);
			case_print_symbol(VSFVM_CODE_SYMBOL_LXOR);
			case_print_symbol(VSFVM_CODE_SYMBOL_COMMA);
			case_print_symbol(VSFVM_CODE_SYMBOL_SHL);
			case_print_symbol(VSFVM_CODE_SYMBOL_SHR);
			case_print_symbol(VSFVM_CODE_SYMBOL_ASSIGN);
			case_print_symbol(VSFVM_CODE_SYMBOL_SEMICOLON);
			default: VSFVM_LOG_ERROR("unknown symbol: %d" VSFVM_LOG_LINEEND, id);
		}
		break;
	case VSFVM_CODE_TYPE_KEYWORD:
#define case_print_keyword(kw)			case kw: VSFVM_LOG_INFO("VSFVM_KEYWORD(" TO_STR(kw) ", %d, %d)," VSFVM_LOG_LINEEND, arg8, arg16); break
		switch (id)
		{
			case_print_keyword(VSFVM_CODE_KEYWORD_var);
			case_print_keyword(VSFVM_CODE_KEYWORD_goto);
			case_print_keyword(VSFVM_CODE_KEYWORD_if);
			case_print_keyword(VSFVM_CODE_KEYWORD_return);
			case_print_keyword(VSFVM_CODE_KEYWORD_breakpoint);
			default: VSFVM_LOG_ERROR("unknown keyword: %d" VSFVM_LOG_LINEEND, id);
		}
		break;
	case VSFVM_CODE_TYPE_NUMBER:
		VSFVM_LOG_INFO("VSFVM_NUMBER(%d)," VSFVM_LOG_LINEEND, value);
		break;
	case VSFVM_CODE_TYPE_VARIABLE:
		VSFVM_LOG_INFO("VSFVM_VARIABLE(%s, %s, %d)," VSFVM_LOG_LINEEND,
				id > VSFVM_CODE_VARIABLE_FUNCTION ? "unknown variable type" : vsfvmc_variable_id[id],
				arg8 > VSFVM_CODE_VARIABLE_POS_FUNCAUTO ? "unknown variable position" : vsfvmc_variable_pos[arg8],
				arg16);
		break;
	case VSFVM_CODE_TYPE_FUNCTION:
		VSFVM_LOG_INFO("VSFVM_FUNCTION(%s, %d, %d)," VSFVM_LOG_LINEEND,
			id > VSFVM_CODE_FUNCTION_THREAD ? "unknown function type" : vsfvmc_funtion_id[id],
			arg8, arg16);
		break;
	case VSFVM_CODE_TYPE_EOF:
		VSFVM_LOG_INFO("VSFVM_EOF(),");
		break;
	}
}

void vsfvm_objdump(uint32_t *buff, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
	{
		VSFVM_LOG_INFO("%d:", i);
		vsfvm_tkdump(*buff++);
	}
}
