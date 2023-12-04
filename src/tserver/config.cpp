// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <limits.h>
#include <commctrl.h> // potrebuju LPCOLORMAP

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"

#include "registry.h"
#include "config.h"

#define PATH_SOFTWARE_TSERVER L"Software\\Open Salamander\\Trace Server"

CRegistry Registry;
CConfigData ConfigData;

//*****************************************************************************
//
// CConfigData
//

CConfigData::CConfigData()
{
    memset(&MainWindowPlacement, 0, sizeof(MainWindowPlacement));

    MainWindowHidden = TRUE;
    UseToolbarCaption = TRUE;
    AlwaysOnTop = TRUE;
    UseMaxMessagesCount = TRUE;
    MaxMessagesCount = 100000;
    ScrollToLatestMessage = FALSE;
    ShowOnErrorMessage = TRUE;
    AutoClear = TRUE;
    HotKey = 635; // Ctrl+F12
    HotKeyClear = 0;

    ViewColumnVisible_Type = TRUE;
    ViewColumnVisible_PID = TRUE;
    ViewColumnVisible_UPID = TRUE;
    ViewColumnVisible_PName = TRUE;
    ViewColumnVisible_TID = TRUE;
    ViewColumnVisible_UTID = TRUE;
    ViewColumnVisible_TName = TRUE;
    ViewColumnVisible_Date = TRUE;
    ViewColumnVisible_Time = TRUE;
    ViewColumnVisible_Counter = TRUE;
    ViewColumnVisible_Modul = TRUE;
    ViewColumnVisible_Line = TRUE;
    ViewColumnVisible_Message = TRUE;

    ViewColumnWidth_Type = 20;
    ViewColumnWidth_PID = 40;
    ViewColumnWidth_UPID = 40;
    ViewColumnWidth_PName = 40;
    ViewColumnWidth_TID = 40;
    ViewColumnWidth_UTID = 40;
    ViewColumnWidth_TName = 40;
    ViewColumnWidth_Date = 50;
    ViewColumnWidth_Time = 50;
    ViewColumnWidth_Counter = 50;
    ViewColumnWidth_Modul = 100;
    ViewColumnWidth_Line = 40;
    ViewColumnWidth_Message = 150;
}

BOOL CConfigData::Register(CRegistry& registry)
{
    BOOL retReg;
    BOOL ret = TRUE;

    HRegistryPath Path1;
    retReg = registry.RegisterPath(Path1, HKEY_CURRENT_USER, PATH_SOFTWARE_TSERVER, L"Configuration", NULL);
    ret &= retReg;
    if (retReg)
    {
        if (ret)
            ret &= registry.RegisterClass(Path1, L"MainWindowPlacement", &MainWindowPlacement, &RegWindowPlacement);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"MainWindowHidden", &MainWindowHidden, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"UseToolbarCaption", &UseToolbarCaption, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"AlwaysOnTop", &AlwaysOnTop, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"UseMaxMessagesCount", &UseMaxMessagesCount, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"MaxMessagesCount", &MaxMessagesCount, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"ScrollToLatestMessage", &ScrollToLatestMessage, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"ShowOnErrorMessage", &ShowOnErrorMessage, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"AutoClear", &AutoClear, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"HotKey", &HotKey, &RegWORD);
        if (ret)
            ret &= registry.RegisterClass(Path1, L"HotKeyClear", &HotKeyClear, &RegWORD);
    }

    HRegistryPath Path2;
    retReg = registry.RegisterPath(Path2, HKEY_CURRENT_USER, PATH_SOFTWARE_TSERVER, L"Columns", NULL);
    ret &= retReg;
    if (retReg)
    {
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Type", &ViewColumnVisible_Type, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_PID", &ViewColumnVisible_PID, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_UPID", &ViewColumnVisible_UPID, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_PName", &ViewColumnVisible_PName, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_TID", &ViewColumnVisible_TID, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_UTID", &ViewColumnVisible_UTID, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_TName", &ViewColumnVisible_TName, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Date", &ViewColumnVisible_Date, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Time", &ViewColumnVisible_Time, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Counter", &ViewColumnVisible_Counter, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Modul", &ViewColumnVisible_Modul, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Line", &ViewColumnVisible_Line, &RegBOOL);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnVisible_Message", &ViewColumnVisible_Message, &RegBOOL);

        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Type", &ViewColumnWidth_Type, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_PID", &ViewColumnWidth_PID, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_UPID", &ViewColumnWidth_UPID, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_PName", &ViewColumnWidth_PName, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_TID", &ViewColumnWidth_TID, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_UTID", &ViewColumnWidth_UTID, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_TName", &ViewColumnWidth_TName, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Date", &ViewColumnWidth_Date, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Time", &ViewColumnWidth_Time, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Counter", &ViewColumnWidth_Counter, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Modul", &ViewColumnWidth_Modul, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Line", &ViewColumnWidth_Line, &RegInt);
        if (ret)
            ret &= registry.RegisterClass(Path2, L"ViewColumnWidth_Message", &ViewColumnWidth_Message, &RegInt);
    }

    return ret;
}
