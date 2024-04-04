// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <ostream>
#include <limits.h>
#include <commctrl.h> // I need LPCOLORMAP

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"
#include "messages.h"
#include "handles.h"

#include "str.h"
#include "moore.h"

//
// ****************************************************************************
// Initialize
// Calculate the arrays Fail1 and Fail2
//

BOOL CSearchData::Initialize()
{
    if (Pattern == NULL || Length == 0)
    {
        TRACE_E("Empty search pattern.");
        return FALSE;
    }

    if (Fail1 == NULL)
        Fail1 = new int[256];
    if (Fail1 == NULL)
    {
        TRACE_E("Low memory for searching.");
        return FALSE;
    }

    if (Fail2 == NULL)
        Fail2 = new int[Length + 1];
    if (Fail2 == NULL)
    {
        delete[] (Fail1);
        Fail1 = NULL;
        TRACE_E("Low memory for searching.");
        return FALSE;
    }

    int* f = new int[Length + 1];
    if (f == NULL)
    {
        delete[] (Fail1);
        Fail1 = NULL;
        delete[] (Fail2);
        Fail2 = NULL;
        TRACE_E("Low memory for searching.");
        return FALSE;
    }

    int i;
    for (i = 0; i < 256; i++)
        Fail1[i] = Length;
    for (i = Length; i > 0; i--)
    {
        if (Fail1[Pattern[i - 1]] == Length)
            Fail1[Pattern[i - 1]] = Length - i;
        Fail2[i - 1] = 2 * Length - i;
    }
    Fail2[Length] = Length - 1; // updated, the algorithm reaches this value, no further action is needed

    i = Length;
    int t = Length + 1;
    while (i > 0)
    {
        f[i - 1] = t;
        while (t <= Length && Pattern[i - 1] != Pattern[t - 1])
        {
            Fail2[t - 1] = Minimum(Fail2[t - 1], Length - i);
            t = f[t - 1];
        }
        i--;
        t--;
    }

    for (i = 0; i < t; i++)
        Fail2[i] = Minimum(Fail2[i], Length + t - i - 1);

    i = f[t - 1];
    if (t <= Length)
    {
        while (1)
        {
            while (t <= i)
            {
                Fail2[t - 1] = Minimum(Fail2[t - 1], i - t + Length);
                t++;
            }
            if (t > Length)
                break;
            i = f[i - 1];
        }
    }

    delete[] (f);

    return TRUE;
}

void CSearchData::SetFlags(WORD flags)
{
    Flags = flags;
    if (Pattern == NULL)
        Pattern = (char*)malloc(Length + 1);
    if (Pattern == NULL)
    {
        TRACE_E("Low memory for searching.");
        return;
    }
    if (OriginalPattern != NULL)
        if (Flags & sfForward)
            memcpy(Pattern, OriginalPattern, Length + 1);
        else
        {
            char* s = Pattern;
            const char* s2 = OriginalPattern + Length - 1;
            int c = Length;
            while (c--)
                *s++ = *s2--;
            *s = 0;
        }

    if ((Flags & sfCaseSensitive) == 0)
    {
        char* s = Pattern;
        int c = Length;
        while (c--)
        {
            *s = LowerCase[*s];
            s++;
        }
    }
    Initialize();
}

void CSearchData::Set(const char* pattern, WORD flags)
{
    if (pattern != NULL)
        Length = (int)strlen(pattern);
    else
        Length = 0;
    if (OriginalPattern != NULL)
        free(OriginalPattern);
    OriginalPattern = (char*)malloc(Length + 1);
    if (OriginalPattern != NULL)
    {
        memcpy(OriginalPattern, pattern, Length);
        OriginalPattern[Length] = 0; // for compatibility with regular string
    }
    if (Pattern != NULL)
    {
        free(Pattern);
        Pattern = NULL;
    }
    if (Fail2 != NULL)
    {
        delete Fail2;
        Fail2 = NULL;
    }
    SetFlags(flags);
}

void CSearchData::Set(const char* pattern, const int length, WORD flags)
{
    if (pattern != NULL)
        Length = length;
    else
        Length = 0;
    if (OriginalPattern != NULL)
        free(OriginalPattern);
    OriginalPattern = (char*)malloc(Length + 1);
    if (OriginalPattern != NULL)
    {
        memcpy(OriginalPattern, pattern, Length);
        OriginalPattern[Length] = 0; // for compatibility with regular string
    }
    if (Pattern != NULL)
    {
        free(Pattern);
        Pattern = NULL;
    }
    if (Fail2 != NULL)
    {
        delete Fail2;
        Fail2 = NULL;
    }
    SetFlags(flags);
}
