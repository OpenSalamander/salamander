// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//disable not finished source, which is yet under costruction
//#define DITRIBUTABLE_VERSION

//definitions for inflate module

//PKZIP 1.93a problem--live with it
#define PKZIP_BUG_WORKAROUND

//check Error field of CInputManager after calling NextByte() in
//NEEDBITS() macro
//2-3% speed penalty
#define CHECK_INPUT_ERROR

typedef unsigned __int8 uch;   // 8-bit unsigned type
typedef unsigned __int16 ush;  // 16-bit unsigned type
typedef unsigned int ulg;      // at least 32-bit type (usually processor register size)
typedef unsigned __int64 ullg; //

//crypt.c

#define MAX_PASSWORD 256

//heap sizes

#define INITIAL_HEAP_SIZE 512 * 1024
#define MAXIMUM_HEAP_SIZE 0

/*
//file trace
extern HANDLE  log;
#define FTRACE(s) {DWORD   dummy;\
                   if (log != INVALID_HANDLE_VALUE) {\
                     WriteFile(log, s, lstrlen(s), &dummy, NULL);\
                     WriteFile(log, "\r\n", 2, &dummy, NULL);}}\
*/
