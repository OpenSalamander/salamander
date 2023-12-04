// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//
// Tohle je uprava pro Salamander 7zip plugin
//

#include "StdAfx.h"

/*#ifndef _UNICODE
#include "../CPP/Common/StringConvert.h"
#endif
*/

typedef struct
{
    LPTHREAD_START_ROUTINE StartAddress;
    LPVOID Parameter;
} AddCallStackObjectParam;

typedef unsigned(__stdcall* FThreadBody)(void*);

DWORD
RunThreadWithCallStackObject(LPTHREAD_START_ROUTINE startAddress, LPVOID parameter)
{
    HMODULE module = NULL;
    FThreadBody addCallStackObject = NULL;

    if ((module = GetModuleHandle("7zip.spl")) != NULL &&
        (addCallStackObject = (FThreadBody)GetProcAddress(module, "AddCallStackObject")) != NULL)
    {
        // podarilo se nam ziskat AddCallStackObject ze 7zip.spl
        AddCallStackObjectParam p;
        p.StartAddress = startAddress;
        p.Parameter = parameter;

        return addCallStackObject(&p);
    }
    else
    {
        // smula, musime jet po stary koleji
        return startAddress(parameter);
    }
}
