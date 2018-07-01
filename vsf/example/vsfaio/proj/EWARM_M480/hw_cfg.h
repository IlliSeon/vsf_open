/**************************************************************************
 *  Copyright (C) 2008 - 2012 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       hw_cfg_VSFCore_STM32.h                                    *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    hardware configuration file for VSFCore_STM32             *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 *      2008-11-22:     rewrite GPIO_Dir(by SimonQian)                    *
 **************************************************************************/

#define OSC_HZ							((uint32_t)12000000)

#define USB_PULLUP_PORT					VSFHAL_DUMMY_PORT
#define USB_PULLUP_PIN					0

#define VSFHAL_HCD_INDEX				0

// UART4 on PC6/PC7
#define DEBUG_UART_INDEX				\
		((4ULL << 0) | \
		(2ULL << 8) | (6ULL << 12) | (5ULL << 16) | \
		(2ULL << 20) | (7ULL << 24) | (5ULL << 28))
