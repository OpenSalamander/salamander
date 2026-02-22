// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CConfigData
//

class CConfigData
{
public:
    // placement of the main window
    WINDOWPLACEMENT MainWindowPlacement;
    // window is hidden - used only by the ToolbarCaption mode
    BOOL MainWindowHidden;
    // program has a toolbar caption; it is hidden from the task list
    BOOL UseToolbarCaption;
    // window stays on top
    BOOL AlwaysOnTop;
    // enforce the message limit
    BOOL UseMaxMessagesCount;
    // maximum number of messages in the array
    int MaxMessagesCount;
    // display the latest message in the list
    BOOL ScrollToLatestMessage;
    // when an Error message arrives, beep and bring the window to front
    BOOL ShowOnErrorMessage;
    // automatically clear messages when a new program connects
    BOOL AutoClear;
    // hotkey
    WORD HotKey;
    WORD HotKeyClear;

    // visibility of individual columns
    BOOL ViewColumnVisible_Type;
    BOOL ViewColumnVisible_PID;
    BOOL ViewColumnVisible_UPID;
    BOOL ViewColumnVisible_PName;
    BOOL ViewColumnVisible_TID;
    BOOL ViewColumnVisible_UTID;
    BOOL ViewColumnVisible_TName;
    BOOL ViewColumnVisible_Date;
    BOOL ViewColumnVisible_Time;
    BOOL ViewColumnVisible_Counter;
    BOOL ViewColumnVisible_Modul;
    BOOL ViewColumnVisible_Line;
    BOOL ViewColumnVisible_Message;

    // width of individual columns
    int ViewColumnWidth_Type;
    int ViewColumnWidth_PID;
    int ViewColumnWidth_UPID;
    int ViewColumnWidth_PName;
    int ViewColumnWidth_TID;
    int ViewColumnWidth_UTID;
    int ViewColumnWidth_TName;
    int ViewColumnWidth_Date;
    int ViewColumnWidth_Time;
    int ViewColumnWidth_Counter;
    int ViewColumnWidth_Modul;
    int ViewColumnWidth_Line;
    int ViewColumnWidth_Message;

public:
    CConfigData();

    BOOL Register(CRegistry& registry);
};

extern CConfigData ConfigData;
extern CRegistry Registry;
