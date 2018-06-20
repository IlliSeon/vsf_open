#include "vsf.h"
#include "windows.h"
#include <stdio.h>

static CRITICAL_SECTION curThreadSleep;
static HANDLE hWakeupSem;

// PendSV
struct vsfhal_pendsv_t
{
	void(*on_pendsv)(void *);
	void *param;
} static vsfhal_pendsv;

vsf_err_t vsfhal_core_pendsv_config(void(*on_pendsv)(void *), void *param)
{
	vsfhal_pendsv.on_pendsv = on_pendsv;
	vsfhal_pendsv.param = param;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_pendsv_trigger(void)
{
	if (vsfhal_pendsv.on_pendsv != NULL)
		vsfhal_pendsv.on_pendsv(vsfhal_pendsv.param);
	return VSFERR_NONE;
}

// critical section
static CRITICAL_SECTION gCrit;
static bool bCritInited = false;
static bool bInCrit = false;

void vsf_enter_critical(void)
{
	if (!bCritInited)
	{
		bCritInited = true;
		InitializeCriticalSection(&gCrit);
	}
	EnterCriticalSection(&gCrit);
	bInCrit = true;
}

void vsf_leave_critical(void)
{
	bInCrit = false;
	LeaveCriticalSection(&gCrit);
}

void vsf_set_gint(bool enter)
{
	if (enter)
		vsf_enter_critical();
}

bool vsf_get_gint(void)
{
	return bInCrit;
}

uint8_t vsfhal_core_set_intlevel(uint8_t level)
{
	return level;
}

// core
vsf_err_t vsfhal_core_fini(void *p)
{
	return VSFERR_NONE;
}

vsf_err_t vsfhal_core_init(void *p)
{
	hWakeupSem = CreateSemaphore(0, 0, 1, 0);
	InitializeCriticalSection(&curThreadSleep);
	EnterCriticalSection(&curThreadSleep);
	return VSFERR_NONE;
}

void vsfhal_core_sleep(uint32_t mode)
{
	LeaveCriticalSection(&curThreadSleep);
	WaitForSingleObject(hWakeupSem, INFINITE);
}

vsf_err_t vsfhal_core_reset(void *p)
{
	return VSFERR_NONE;
}

// Tickclk
static void(*tickclk_callback)(void *param) = NULL;
static void *tickclk_param = NULL;
static HANDLE hTickclkThread;
bool bTickclkExit = false, bTickclkRun = false;

static void CALLBACK TimeProc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	if (bTickclkRun && (tickclk_callback != NULL))
	{
		EnterCriticalSection(&curThreadSleep);
		tickclk_callback(tickclk_param);
		ReleaseSemaphore(hWakeupSem, 1, NULL);
	}
}

static DWORD CALLBACK TickclkThreadProc(PVOID pvoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	SetTimer(NULL, 10, 1, TimeProc);
	while (!bTickclkExit && GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == WM_TIMER)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	KillTimer(NULL, 10);
	bTickclkExit = false;
	return 0;
}

vsf_err_t vsfhal_tickclk_init(int32_t int_priority)
{
	DWORD dwTickclkThreadId;
	bTickclkExit = false;
	hTickclkThread = CreateThread(NULL, 0, TickclkThreadProc, 0, 0, &dwTickclkThreadId);
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_fini(void)
{
	bTickclkExit = true;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_config_cb(void(*callback)(void*), void *param)
{
	tickclk_callback = callback;
	tickclk_param = param;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_start(void)
{
	bTickclkRun = true;
	return VSFERR_NONE;
}

vsf_err_t vsfhal_tickclk_stop(void)
{
	bTickclkRun = false;
	return VSFERR_NONE;
}

uint32_t vsfhal_tickclk_get_ms(void)
{
	return GetTickCount();
}

// stdio stream
static char stdout_buff[4 * 1024 + 1];
static void stdout_stream_init(struct vsf_stream_t *stream)
{
	vsfstream_connect_rx(stream);
}

static uint32_t stdout_stream_write(struct vsf_stream_t *stream,
	struct vsf_buffer_t *buffer)
{
	if (buffer->size < (sizeof(stdout_buff) - 1))
	{
		memcpy(stdout_buff, buffer->buffer, buffer->size);
		stdout_buff[buffer->size] = '\0';
		fwrite(stdout_buff, 1, buffer->size, stdout);
		return buffer->size;
	}
	return 0;
}

static uint32_t stdout_stream_get_avail_length(struct vsf_stream_t *stream)
{
	return sizeof(stdout_buff) - 1;
}

const struct vsf_stream_op_t stdout_stream_op =
{
	.init = stdout_stream_init,
	.write = stdout_stream_write,
	.get_avail_length = stdout_stream_get_avail_length,
};

static char stdin_buff[4 * 1024 + 1], *stdin_ptr;
static size_t stdin_datasize;

static DWORD CALLBACK StdinThreadProc(PVOID pvoid)
{
	struct vsf_stream_t *stream = (struct vsf_stream_t *)pvoid;
	char ch;

	stdin_datasize = 0;
	while (1)
	{
		if ((fread(&ch, 1, 1, stdin) == 1) && (ch != '\r'))
		{
			if (stdin_datasize >= (sizeof(stdin_buff) - 1))
				continue;

			stdin_buff[stdin_datasize++] = ch;
			if (ch == '\n')
			{
				EnterCriticalSection(&curThreadSleep);
				stdin_ptr = stdin_buff;
				if (stream->callback_rx.on_inout != NULL)
					stream->callback_rx.on_inout(stream->callback_rx.param);
				ReleaseSemaphore(hWakeupSem, 1, NULL);
			}
		}
	}
	return 0;
}

static void stdin_stream_init(struct vsf_stream_t *stream)
{
	DWORD dwStdinThreadId;
	vsfstream_connect_tx(stream);
	CreateThread(NULL, 0, StdinThreadProc, stream, 0, &dwStdinThreadId);
}

static uint32_t stdin_stream_read(struct vsf_stream_t *stream,
	struct vsf_buffer_t *buffer)
{
	uint32_t realsize = min(buffer->size, stdin_datasize);

	memcpy(buffer->buffer, stdin_ptr, realsize);
	stdin_datasize -= realsize;
	return realsize;
}

static uint32_t stdin_stream_get_data_length(struct vsf_stream_t *stream)
{
	return stdin_datasize;
}

const struct vsf_stream_op_t stdin_stream_op =
{
	.init = stdin_stream_init,
	.read = stdin_stream_read,
	.get_data_length = stdin_stream_get_data_length,
};
