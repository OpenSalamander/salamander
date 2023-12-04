// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>

// modul MS_INIT zajistuje volani konstruktoru statickych objektu ve spravnem poradi
// a na urovni "lib" (pred "user")

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

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

C__MSInit::C__MSInit()
{
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
}
