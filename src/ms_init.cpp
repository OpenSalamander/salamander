﻿// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

DWORD SalamanderStartTime = 0;
DWORD SalamanderExceptionTime = 0;

// module MS_INIT ensures calling the constructor of static objects in the correct order
// and at the "lib" level (before "user")

// WARNING: sections are aligned to 4096 bytes, which is the size by which they will be increased
// salamande.exe by adding a module containing perhaps only a single DWORD variable
// See PEViewer on salamand.exe, time "Section Table".

#pragma warning(disable : 4073)
#pragma init_seg(lib)

class C__MSInit
{
public:
    C__MSInit();
};

C__MSInit __MSInit;

#ifdef HANDLES_ENABLE
void Initialize__Handles();
#endif // HANDLES_ENABLE

#if defined(_DEBUG) && !defined(HEAP_DISABLE)
void Initialize__Heap();
#endif // defined(_DEBUG) && !defined(HEAP_DISABLE)

#ifndef ALLOCHAN_DISABLE
void Initialize__Allochan();
#endif // ALLOCHAN_DISABLE

#ifndef MESSAGES_DISABLE
void Initialize__Messages();
#endif // MESSAGES_DISABLE

#ifndef STR_DISABLE
void Initialize__Str();
#endif // STR_DISABLE

#ifdef TRACE_ENABLE
void Initialize__Trace();
#endif // TRACE_ENABLE

#ifndef CALLSTK_DISABLE
void Initialize__CallStk();
#endif // CALLSTK_DISABLE

C__MSInit::C__MSInit()
{
    SalamanderStartTime = GetTickCount();
    SYSTEMTIME st;
    GetLocalTime(&st);

#ifdef TRACE_ENABLE
    Initialize__Trace(); // trace.cpp
#endif                   // TRACE_ENABLE
#ifndef MESSAGES_DISABLE
    Initialize__Messages(); // messages.cpp
#endif                      // MESSAGES_DISABLE
#if defined(_DEBUG) && !defined(HEAP_DISABLE)
    Initialize__Heap(); // heap.cpp
#endif                  // defined(_DEBUG) && !defined(HEAP_DISABLE)
#ifndef ALLOCHAN_DISABLE
    Initialize__Allochan(); // allochan.cpp
#endif                      // ALLOCHAN_DISABLE
#ifndef STR_DISABLE
    Initialize__Str(); // str.cpp
#endif                 // STR_DISABLE
#ifdef HANDLES_ENABLE
    Initialize__Handles(); // handles.cpp
#endif                     // HANDLES_ENABLE
#ifndef CALLSTK_DISABLE
    Initialize__CallStk(); // callstk.cpp
#endif                     // CALLSTK_DISABLE
}
