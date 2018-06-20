#include "vsfvm_compiler.h"
#include "lexer/vsfvm_lexer.h"
#include "vsfvm_snapshot.h"

int vsfvmc_snapshot_take(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_snapshot_t *snapshot)
{
	struct vsfvmc_lexer_t *lexer = &vsfvmc->script.lexer;
	struct vsfvmc_lexer_ctx_t *lctx = &lexer->curctx;
	struct vsfvmc_snapshot_lexer_t *snapshot_lexer =
		&snapshot->compiler.script.lexer;
	struct vsfvmc_snapshot_func_t *snapshot_curfunc =
		&snapshot->compiler.script.cur_func;
	struct vsfvmc_func_t *curfunc = &vsfvmc->script.cur_func;

	if (lctx->op->priv_size > 0)
	{
		snapshot_lexer->priv = vsf_bufmgr_malloc(lctx->op->priv_size);
		if (!snapshot_lexer->priv)
			return -1;
		memcpy(snapshot_lexer->priv, lctx->priv, lctx->op->priv_size);
	}

	snapshot_lexer->op = lctx->op;
	snapshot_lexer->pt_state = lctx->pt.state;
	snapshot_lexer->ctx_sp = lexer->ctx_stack.sp;
	snapshot_lexer->symtbl_sp = lexer->symtbl.sp;
	snapshot_lexer->symid = lexer->symid;
	snapshot_lexer->expr.nesting = lexer->expr.nesting;
	snapshot_lexer->expr.stack_exp_sp = lexer->expr.stack_exp.sp;
	snapshot_lexer->expr.stack_op_sp = lexer->expr.stack_op.sp;
	snapshot_lexer->expr.context = lexer->expr.context;

	snapshot_curfunc->name = curfunc->name;
	snapshot_curfunc->symtbl_idx = curfunc->symtbl_idx;
	snapshot_curfunc->block_level = curfunc->block_level;
	snapshot_curfunc->arglist = curfunc->arglist;
	snapshot_curfunc->varlist = curfunc->varlist;
	snapshot_curfunc->curctx = curfunc->curctx;
	snapshot_curfunc->ctx_sp = curfunc->ctx.sp;
	snapshot_curfunc->linktbl_sp = curfunc->linktbl.sp;

	snapshot->compiler.bytecode_sp = vsfvmc->bytecode.sp;
	snapshot->compiler.script.func_stack_sp = vsfvmc->script.func_stack.sp;
	snapshot->compiler.script.pt_stmt_state = vsfvmc->script.pt_stmt.state;
	return 0;
}

int vsfvmc_snapshot_free(struct vsfvmc_snapshot_t *snapshot)
{
	if (snapshot->compiler.script.lexer.priv != NULL)
		vsf_bufmgr_free(snapshot->compiler.script.lexer.priv);
	memset(snapshot, 0, sizeof(*snapshot));
	return 0;
}

int vsfvmc_snapshot_restore(struct vsfvmc_t *vsfvmc,
	struct vsfvmc_snapshot_t *snapshot)
{
	struct vsfvmc_lexer_t *lexer = &vsfvmc->script.lexer;
	struct vsfvmc_lexer_ctx_t *lctx = &lexer->curctx;
	struct vsfvmc_snapshot_lexer_t *snapshot_lexer =
		&snapshot->compiler.script.lexer;
	struct vsfvmc_snapshot_func_t *snapshot_curfunc =
		&snapshot->compiler.script.cur_func;
	struct vsfvmc_func_t *curfunc = &vsfvmc->script.cur_func;

	if (snapshot_lexer->priv != NULL)
		memcpy(lctx->priv, snapshot_lexer->priv, snapshot_lexer->op->priv_size);

	lctx->op = snapshot_lexer->op;
	lctx->pt.state = snapshot_lexer->pt_state;

	if (lexer->ctx_stack.sp < snapshot_lexer->ctx_sp)
		lexer->ctx_stack.sp = snapshot_lexer->ctx_sp;
	if (lexer->symtbl.sp < snapshot_lexer->symtbl_sp)
		return -1;
	lexer->symtbl.sp = snapshot_lexer->symtbl_sp;
	lexer->symid = snapshot_lexer->symid;

	if (lexer->expr.nesting < snapshot_lexer->expr.nesting)
		return -1;
	lexer->expr.nesting = snapshot_lexer->expr.nesting;
	if (lexer->expr.stack_exp.sp < snapshot_lexer->expr.stack_exp_sp)
		return -1;
	lexer->expr.stack_exp.sp = snapshot_lexer->expr.stack_exp_sp;
	if (lexer->expr.stack_op.sp < snapshot_lexer->expr.stack_op_sp)
		return -1;
	lexer->expr.stack_op.sp = snapshot_lexer->expr.stack_op_sp;
	lexer->expr.context = snapshot_lexer->expr.context;

	curfunc->name = snapshot_curfunc->name;
	curfunc->symtbl_idx = snapshot_curfunc->symtbl_idx;
	curfunc->block_level = snapshot_curfunc->block_level;
	curfunc->arglist = snapshot_curfunc->arglist;
	curfunc->varlist = snapshot_curfunc->varlist;
	curfunc->curctx = snapshot_curfunc->curctx;
	if (curfunc->ctx.sp < snapshot_curfunc->ctx_sp)
		return - 1;
	else
		vsf_dynstack_pop(&curfunc->ctx,
			snapshot_curfunc->ctx_sp - curfunc->ctx.sp);

	if (curfunc->linktbl.sp < snapshot_curfunc->linktbl_sp)
		return -1;
	curfunc->linktbl.sp = snapshot_curfunc->linktbl_sp;

	vsfvmc->bytecode.sp = snapshot->compiler.bytecode_sp;
	if (vsfvmc->script.func_stack.sp < snapshot->compiler.script.func_stack_sp)
		return -1;
	vsfvmc->script.func_stack.sp = snapshot->compiler.script.func_stack_sp;
	vsfvmc->script.pt_stmt.state = snapshot->compiler.script.pt_stmt_state;
	return 0;
}
