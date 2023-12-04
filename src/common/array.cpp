// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <limits.h>
#include <ostream>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "array.h"

#ifdef TRACE_ENABLE

std::ostream& operator<<(std::ostream& out, const CErrorType& err)
{
    switch (err)
    {
    case etNone:
        out << "etNone";
        break;
    case etLowMemory:
        out << "etLowMemory";
        break;
    case etUnknownIndex:
        out << "etUnknownIndex";
        break;
    case etBadInsert:
        out << "etBadInsert";
        break;
    case etBadDispose:
        out << "etBadDispose";
        break;
    case etDestructed:
        out << "etDestructed";
        break;
    default:
        out << (int)err;
    }
    return out;
}

std::wostream& operator<<(std::wostream& out, const CErrorType& err)
{
    switch (err)
    {
    case etNone:
        out << L"etNone";
        break;
    case etLowMemory:
        out << L"etLowMemory";
        break;
    case etUnknownIndex:
        out << L"etUnknownIndex";
        break;
    case etBadInsert:
        out << L"etBadInsert";
        break;
    case etBadDispose:
        out << L"etBadDispose";
        break;
    case etDestructed:
        out << L"etDestructed";
        break;
    default:
        out << (int)err;
    }
    return out;
}

#endif //TRACE_ENABLE
