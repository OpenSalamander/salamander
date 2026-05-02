// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//silent flags
#define SF_TOOLONGNAMES 0x0001
#define SF_CREATEFILE 0x0002
#define SF_WRITEFILE 0x0004
#define SF_BADDATA 0x0008
#define SF_BADMETHOD 0x0010
#define SF_SKIPALL 0x0020
#define SF_OVEWRITEALL 0x0040
#define SF_OVEWRITEALL_RO 0x0080
#define SF_SKIPALLCRYPT 0x0100

//multivol modes
#define MV_DETACH_ARCHIVE 1
#define MV_PROCESS_ARCHIVE 2

extern HANDLE Heap;
extern bool RemoveTemp;
extern const char* Title;
extern bool SafeMode;
extern HINSTANCE HInstance;
void CloseMapping();
int HandleError(CStringIndex message, unsigned long err, char* fileName = NULL, bool* retry = NULL, unsigned silentFlag = 0, bool noSkip = false);

#ifdef EXT_VER
CStringIndex MapFile(const char* name, HANDLE* file, HANDLE* mapping,
                     const unsigned char** data, unsigned long* size, int* err);
void UnmapFile(HANDLE file, HANDLE mapping, const unsigned char* data);
#endif //EXT_VER

#ifdef _DEBUG
void Trace(char* message, ...);
#define TRACE1(a1) Trace(a1);
#define TRACE2(a1, a2) Trace(a1, a2);
#define TRACE3(a1, a2, a3) Trace(a1, a2, a3);
#define TRACE4(a1, a2, a3, a4) Trace(a1, a2, a3, a4);
#define TRACE5(a1, a2, a3, a4, a5) Trace(a1, a2, a3, a5);

#else //_DEBUG
#define TRACE1(a1) ;
#define TRACE2(a1, a2) ;
#define TRACE3(a1, a2, a3) ;
#define TRACE4(a1, a2, a3, a4) ;
#define TRACE5(a1, a2, a3, a4, a5) ;

#endif //_DEBUG
