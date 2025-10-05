// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//
// This is a modification for the Salamander 7-Zip plugin
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
        // successfully obtained AddCallStackObject from 7zip.spl
        AddCallStackObjectParam p;
        p.StartAddress = startAddress;
        p.Parameter = parameter;

        return addCallStackObject(&p);
    }
    else
    {
        // unlucky; we have to fall back to the old approach
        return startAddress(parameter);
    }
}
