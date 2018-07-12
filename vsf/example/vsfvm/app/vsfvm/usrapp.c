
#include "vsf.h"
#include <stdio.h>
#include <stdlib.h>

#include "usrapp.h"

#define LINE_BUFFER_SIZE			(4 * 1024)
#define TOKEN_BUFFER_SIZE			(32 * 1024)
#define VSFVM_SHELL_PROMPT			">>>"

static void usrapp_on_stdin(void *param);
struct usrapp_t usrapp =
{
	.vsfvm.compiler.dart.op = &vsfvmc_lexer_op_dart,
	.vsfvm.runtime.vm.thread_pool.pool_size = 16,

	.debug.stream_tx.op = &stdout_stream_op,
	.debug.stream_rx.callback_rx.param = &usrapp,
	.debug.stream_rx.callback_rx.on_inout = usrapp_on_stdin,
	.debug.stream_rx.op = &stdin_stream_op,
};

static const char *vsfvmc_errcode_str[] =
{
	TO_STR(VSFVMC_ERRCODE_NONE),

	// common error
	TO_STR(VSFVMC_BUG),
	TO_STR(VSFVMC_BYTECODE_TOOLONG),
	TO_STR(VSFVMC_NOT_ENOUGH_RESOURCES),
	TO_STR(VSFVMC_FATAL_ERROR),
	TO_STR(VSFVMC_NOT_SUPPORT),

	// lexer error
	TO_STR(VSFVMC_LEXER_NOT_SUPPORT),
	TO_STR(VSFVMC_LEXER_INVALID_OP),
	TO_STR(VSFVMC_LEXER_INVALID_STRING),
	TO_STR(VSFVMC_LEXER_INVALID_ESCAPE),
	TO_STR(VSFVMC_LEXER_SYMBOL_TOO_LONG),

	// parser error
	TO_STR(VSFVMC_PARSER_UNEXPECTED_TOKEN),
	TO_STR(VSFVMC_PARSER_INVALID_CLOSURE),
	TO_STR(VSFVMC_PARSER_INVALID_EXPR),
	TO_STR(VSFVMC_PARSER_UNINITED_CONST),
	TO_STR(VSFVMC_PARSER_INVALID_CONST),
	TO_STR(VSFVMC_PARSER_DIV0),
	TO_STR(VSFVMC_PARSER_EXPECT_FUNC_PARAM),

	// compiler error
	TO_STR(VSFVMC_COMPILER_INVALID_MODULE),
	TO_STR(VSFVMC_COMPILER_INVALID_FUNC),
	TO_STR(VSFVMC_COMPILER_INVALID_FUNC_PARAM),
	TO_STR(VSFVMC_COMPILER_FAIL_USRLIB),
};

static int vm_set_bytecode(void *param, uint32_t code, uint32_t pos)
{
	struct usrapp_t *app = (struct usrapp_t *)param;
	struct vsfvm_script_t *rt_script = &app->vsfvm.runtime.script;
	uint32_t *token = (uint32_t *)rt_script->token;

	if (pos >= TOKEN_BUFFER_SIZE)
		return -1;

	token[pos] = code;
	return 0;
}

static uint32_t vm_get_bytecode(void *param, uint32_t pos)
{
	struct usrapp_t *app = (struct usrapp_t *)param;
	struct vsfvm_script_t *rt_script = &app->vsfvm.runtime.script;
	uint32_t *token = (uint32_t *)rt_script->token;
	return pos >= TOKEN_BUFFER_SIZE ? 0xFFFFFFFF : token[pos];
}

static int vm_require_usrlib(void *param, struct vsfvmc_t *vmc, char *path)
{
	struct usrapp_t *app = (struct usrapp_t *)param;
	char *fbuff = NULL;
	size_t flen;
	int err = 0;

	FILE *fin = fopen(path, "rt");
	if (!fin)
	{
		vsfdbg_printf("can not open file %s\n", path);
		return -1;
	}

	fseek(fin, 0L, SEEK_END);
	flen = ftell(fin);
	rewind(fin);
	app->vsfvm.compiler.file_num++;
	if (flen)
	{
		fbuff = (char *)malloc(flen + 1);
		fread(fbuff, 1, flen, fin);
		fclose(fin);
		fin = NULL;

		fbuff[flen] = '\0';
		err = vsfvmc_input(vmc, fbuff);
		free(fbuff);
		fbuff = NULL;

		if (err < 0)
		{
		error:
			err = -err;
			vsfdbg_printf("command line compile error: %s\n",
				(err >= VSFVMC_ERRCODE_END) ? "unknwon error" :
				vsfvmc_errcode_str[err]);
			return -err;
		}
		if (!--app->vsfvm.compiler.file_num)
		{
			err = vsfvmc_input(vmc, "\xFF");	// EOF
			if (err < 0) goto error;
		}
	}
	else fclose(fin);

	return err;
}

static char usrapp_stdin_linebuf[LINE_BUFFER_SIZE + 1];
static void usrapp_on_stdin(void *param)
{
	struct usrapp_t *app = (struct usrapp_t *)param;
	struct vsf_stream_t *stream = &app->debug.stream_rx;
	uint32_t size = vsfstream_get_data_size(stream);
	struct vsf_buffer_t buffer = { usrapp_stdin_linebuf, size };

	struct vsfvmc_t *vmc = &app->vsfvm.compiler.vmc;
	struct vsfvm_t *vm = &app->vsfvm.runtime.vm;
	struct vsfvm_script_t *rt_script = &app->vsfvm.runtime.script;

	uint32_t *tkbuff = (uint32_t *)rt_script->token;
	uint32_t tkpos;
	int err;

	if (size > LINE_BUFFER_SIZE)
		return;

	size = vsfstream_read(stream, &buffer);
	usrapp_stdin_linebuf[size] = '\0';

	err = vsfvmc_input(vmc, usrapp_stdin_linebuf);
	if (err < 0)
	{
		err = -err;
		vsfdbg_printf("command line compile error: %s\n",
			(err >= VSFVMC_ERRCODE_END) ? "unknwon error" :
			vsfvmc_errcode_str[err]);
		if (vsfvmc_snapshot_restore(vmc, &app->vsfvm.snapshot) < 0)
		{
			vsfdbg_printf("fail to restore snapshot.\nexiting...\n");
		exit_vm:
			vsfvm_script_fini(vm, rt_script);
			vsfvm_fini(vm);
			vsfvmc_fini(vmc);
			exit(0);
		}
		vsfdbg_prints(VSFVM_SHELL_PROMPT);
		return;
	}
	else if (err > 0)
		return;
	else
	{
		vsfvmc_snapshot_free(&app->vsfvm.snapshot);
		if (vsfvmc_snapshot_take(vmc, &app->vsfvm.snapshot) < 0)
		{
			vsfdbg_printf("fail to take snapshot.\nexiting...\n");
			goto exit_vm;
		}

		// 1. check if unprocessed anchor
		if ((vmc->script.cur_func.curctx.etoken.token > 0) ||
			(vmc->script.cur_func.ctx.sp > 0) ||
			(vmc->script.func_stack.sp > 1) ||
			(app->vsfvm.vmc_bytecode_pos == vmc->bytecode_pos))
		{
			return;
		}

		// 2. save bytecode_pos
		tkpos = app->vsfvm.vmc_bytecode_pos = vmc->bytecode_pos;
		// 3. append breakpoint
		tkbuff[tkpos] = VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_breakpoint, 0, 0);
		// 4. wake all threads
		rt_script->token_num = tkpos + 1;
		vsflist_foreach(thread, rt_script->thread.next, struct vsfvm_thread_t, list)
			vsfvm_thread_ready(thread);
		// 5. run till breakpoint
		app->vsfvm.wait_prompt = true;
	}
}

static void usrapp_poll_vm(struct usrapp_vsfvm_t *vsfvm)
{
	struct vsfvm_t *vm = &vsfvm->runtime.vm;
	struct vsfvm_script_t *rt_script = &vsfvm->runtime.script;
	struct vsfvm_thread_t *thread = vsflist_get_container(rt_script->thread.next,
		struct vsfvm_thread_t, list);
	int err;

	while (1)
	{
		err = vsfvm_poll(vm);
		if (err < 0)
		{
			vsfdbg_printf("vsfvm_poll failed with %d\n", err);
			break;
		}
		else if (!err)
		{
			if (vsfvm->shell_mode && vsfvm->wait_prompt &&
				(thread->func.type == VSFVM_CODE_FUNCTION_SCRIPT) &&
				(thread->func.pc == (rt_script->token_num - 1)))
			{
				vsfdbg_prints(VSFVM_SHELL_PROMPT);
				vsfvm->wait_prompt = false;
			}
			break;
		}
	}
}

void usrapp_initial_init(struct usrapp_t *app) {}
void usrapp_poll(struct usrapp_t *app)
{
	usrapp_poll_vm(&app->vsfvm);
}
void usrapp_srt_init(struct usrapp_t *app)
{
	int argc = app->argc;
	char **argv = app->argv;

	int srcfile_idx = -1;
	FILE *fin = NULL, *fout = NULL;
	char *fout_path = NULL;
	long flen;
	uint32_t *tkbuff = NULL;

	struct vsfvmc_t *vmc = &app->vsfvm.compiler.vmc;
	struct vsfvm_t *vm = &app->vsfvm.runtime.vm;
	struct vsfvm_script_t *rt_script = &app->vsfvm.runtime.script;

	VSFSTREAM_INIT(&app->debug.stream_tx);
	VSFSTREAM_INIT(&app->debug.stream_rx);
	vsfdbg_init(&app->debug.stream_tx);
	vsfvm_ext_pool_init(16);

	if (argc <= 1)
		goto print_help;
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'h':
			print_help:
				vsfdbg_prints("vsfvmc 0.1alpha\n");
				vsfdbg_prints("format: vsfvmc [-h] [-o OUTFILE INFILE] [-s]\n");
				return;
			case 'o':
				if (++i >= argc)
				{
					vsfdbg_prints("please specify out file\n");
					return;
				}
				fout_path = argv[i];
				break;
			case 's':
				app->vsfvm.compiler.file_num = 1;
				vsfvmc_init(vmc, &usrapp, vm_require_usrlib, vm_set_bytecode, vm_get_bytecode);
				vsfvmc_register_ext(vmc, &vsfvm_ext_std);
				vsfvmc_ext_register_vsf(vmc);
				vsfvmc_register_lexer(vmc, &app->vsfvm.compiler.dart);
				vsfvmc_script(vmc, "shell.dart");

				tkbuff = malloc(TOKEN_BUFFER_SIZE + 1);
				rt_script->token = tkbuff;
				rt_script->token_num = 1;
				tkbuff[0] = VSFVM_KEYWORD(VSFVM_CODE_KEYWORD_breakpoint, 0, 0);

				vsfvm_init(vm);
				vsfvm_register_ext(vm, &vsfvm_ext_std);
				vsfvm_ext_register_vsf(vm);
				if (vsfvm_script_init(vm, rt_script) < 0)
				{
					vsfdbg_prints("fail to initialize script\n");
					goto clean_up_and_exit;
				}

				vsfvmc_snapshot_take(vmc, &app->vsfvm.snapshot);
				vsfstream_connect_rx(&app->debug.stream_rx);
				app->vsfvm.shell_mode = true;
				app->vsfvm.wait_prompt = true;
				return;
			case 'r':
				if (++i >= argc)
				{
					vsfdbg_prints("please specify run file\n");
					return;
				}

				fin = fopen(argv[i], "rb");
				if (!fin)
				{
					vsfdbg_printf("can not open file %s\n", argv[i]);
					return;
				}

				fseek(fin, 0L, SEEK_END);
				flen = ftell(fin);
				rewind(fin);

				tkbuff = malloc(flen);
				fread(tkbuff, 1, flen, fin);
				fclose(fin);
				fin = NULL;

				rt_script->token = tkbuff;
				rt_script->token_num = flen >> 2;

			run_vm:
				vsfdbg_prints("run vm:\n");
				vsfvm_init(vm);
				vsfvm_register_ext(vm, &vsfvm_ext_std);
				vsfvm_ext_register_vsf(vm);
				if (vsfvm_script_init(vm, rt_script) < 0)
				{
					vsfdbg_prints("fail to initialize script\n");
					goto clean_up_and_exit;
				}
				app->vsfvm.shell_mode = false;
				return;
			}
		}
		else
		{
			srcfile_idx = i;
			break;
		}
	}
	if (srcfile_idx < 0)
		goto print_help;

	app->vsfvm.compiler.file_num = 0;
	vsfvmc_init(vmc, &usrapp, vm_require_usrlib, vm_set_bytecode, vm_get_bytecode);
	vsfvmc_register_ext(vmc, &vsfvm_ext_std);
	vsfvmc_ext_register_vsf(vmc);
	vsfvmc_register_lexer(vmc, &app->vsfvm.compiler.dart);
	vsfvmc_script(vmc, argv[srcfile_idx]);

	tkbuff = malloc(TOKEN_BUFFER_SIZE + 1);
	rt_script->token = tkbuff;
	rt_script->token_num = 0;

	if (vm_require_usrlib(&usrapp, vmc, argv[srcfile_idx]))
		goto clean_up_and_exit;
	else
	{
		if (!fout_path)
			fout_path = "a.bin";
		fout = fopen(fout_path, "wb+");
		if (!fout)
		{
			vsfdbg_printf("can not create file %s\n", fout_path);
			goto clean_up_and_exit;
		}
		if (fwrite(tkbuff, 1, 4 * vmc->bytecode_pos, fout) != 4 * vmc->bytecode_pos)
		{
			vsfdbg_prints("fail to write output file\n");
			fclose(fout);
			goto clean_up_and_exit;
		}
		rt_script->token_num = vmc->bytecode_pos;

		vsfdbg_prints("objdump:\n");
		vsfvm_objdump((uint32_t *)rt_script->token, rt_script->token_num);

		vsfvmc_fini(vmc);
		fclose(fout);
		fout = NULL;
		goto run_vm;
	}

clean_up_and_exit:
	vsfvm_ext_pool_fini();
	if (tkbuff != NULL)
		free(tkbuff);
	if (fin != NULL)
		fclose(fin);
	exit(0);
}
