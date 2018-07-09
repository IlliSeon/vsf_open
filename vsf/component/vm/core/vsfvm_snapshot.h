#ifndef __VSFVM_SNAPSHOT_H_INCLUDED__
#define __VSFVM_SNAPSHOT_H_INCLUDED__

struct vsfvmc_snapshot_t
{
	struct
	{
		struct vsfvmc_snapshot_script_t
		{
			struct vsfvmc_snapshot_lexer_t
			{
				const struct vsfvmc_lexer_op_t *op;
				void *priv;
				int pt_state;

				uint32_t ctx_sp;
				uint32_t symtbl_sp;
				int symid;

				struct vsfvmc_snapshot_expr_t
				{
					struct vsfvmc_lexer_exprctx_t context;
					uint32_t nesting;
					uint32_t stack_exp_sp;
					uint32_t stack_op_sp;
				} expr;
			} lexer;

			struct vsfvmc_snapshot_func_t
			{
				struct vsfvmc_lexer_sym_t *name;
				int symtbl_idx;
				int block_level;

				struct vsflist_t arglist;
				struct vsflist_t varlist;

				struct vsfvmc_func_ctx_t curctx;
				uint32_t ctx_sp;
				uint32_t linktbl_sp;
			} cur_func;

			int pt_stmt_state;
			uint32_t func_stack_sp;
		} script;
		uint32_t bytecode_pos;
	} compiler;
};

int vsfvmc_snapshot_take(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_snapshot_t *snapshot);
int vsfvmc_snapshot_free(struct vsfvmc_snapshot_t *snapshot);
int vsfvmc_snapshot_restore(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_snapshot_t *snapshot);

#endif		// __VSFVM_SNAPSHOT_H_INCLUDED__
