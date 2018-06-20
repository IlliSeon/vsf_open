/**************************************************************************
 *  Copyright (C) 2008 - 2012 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    VSF                                                       *
 *  File:       app_cfg.h                                                 *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    configuration file                                        *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

// hardware config file
//#include "hw_cfg_PC.h"

// compiler config
#include "compiler.h"

#define APPCFG_VSFTIMER_NUM				8
#define APPCFG_BUFMGR_SIZE				(32 * 1024)

// The 3 MACROs below define the Hard/Soft/Non-RealTime event queue
// undefine to indicate that the corresponding event queue is not supported
//	note that AT LEASE one event queue should be defined
// define to 0 indicating that the corresponding events will not be queued
//	note that the events can be unqueued ONLY IF the corresponding tasks will
//		not receive events from tasks in higher priority
// define to n indicating the length of corresponding real time event queue
//#define APPCFG_HRT_QUEUE_LEN			0
#define APPCFG_SRT_QUEUE_LEN			0
//#define APPCFG_NRT_QUEUE_LEN			0

#if (defined(APPCFG_HRT_QUEUE_LEN) && (APPCFG_HRT_QUEUE_LEN > 0)) ||\
	(defined(APPCFG_SRT_QUEUE_LEN) && (APPCFG_SRT_QUEUE_LEN > 0)) ||\
	(defined(APPCFG_NRT_QUEUE_LEN) && (APPCFG_NRT_QUEUE_LEN > 0))
#define VSFSM_CFG_PREMPT_EN				1
#else
#define VSFSM_CFG_PREMPT_EN				0
#endif

// define APPCFG_USR_POLL for round robin scheduling
//#define APPCFG_USR_POLL
// define APPCFG_USR_POLL_SLEEP for round robin scheduling with sleeping
#define APPCFG_USR_POLL_SLEEP

#ifdef APPCFG_USR_POLL
#define APPCFG_TICKCLK_PRIORITY			-1
#else
#define APPCFG_TICKCLK_PRIORITY			0xFF
#endif

#define APPCFG_ARGU_EN

// user configuration
#define VSFVM_VM
#define VSFVM_COMPILER

//#define VSFVM_VM_DEBUG
#define VSFVM_LEXER_DEBUG
#define VSFVM_PARSER_DEBUG
#define VSFVM_COMPILER_DEBUG

#define VSFVM_LOG_INFO					vsfdbg_printf
#define VSFVM_LOG_ERROR					vsfdbg_printf
#define VSFVM_LOG_LINEEND				"\n"
