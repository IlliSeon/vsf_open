#ifdef VSFVM_VM
#include "vsfvm.h"
#endif

#ifdef VSFVM_COMPILER
#include "vsfvm_compiler.h"
#include "lexer/dart/vsfvm_lexer_dart.h"
#include "vsfvm_objdump.h"
#include "vsfvm_snapshot.h"
#endif

#include "../ext/std/vsfvm_ext_std.h"
#include "../ext/vsf/vsfvm_ext_vsf.h"

struct usrapp_t
{
	int argc;
	char **argv;

	struct usrapp_vsfvm_t
	{
		struct
		{
			struct vsfvmc_lexer_list_t dart;
			struct vsfvmc_t vmc;
			int file_num;
		} compiler;
#ifdef VSFVM_VM
		struct
		{
			struct vsfvm_t vm;
			struct vsfvm_script_t script;
		} runtime;
		struct vsfvmc_snapshot_t snapshot;
		uint32_t vmc_bytecode_pos;
		bool shell_mode;
		bool wait_prompt;
#endif
	} vsfvm;

	struct
	{
		struct vsf_stream_t stream_tx;
		struct vsf_stream_t stream_rx;
	} debug;
};

extern struct usrapp_t usrapp;

void usrapp_initial_init(struct usrapp_t *app);
void usrapp_srt_init(struct usrapp_t *app);
void usrapp_poll(struct usrapp_t *app);
