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
    // umisteni hlavniho okna
    WINDOWPLACEMENT MainWindowPlacement;
    // okno je zhasnute - pouze pro rezim ToolbatCaption
    BOOL MainWindowHidden;
    // program ma toolbarovy caption; nezobrazuje se na tasklistu
    BOOL UseToolbarCaption;
    // okno je topmost
    BOOL AlwaysOnTop;
    // dodrzovat mezni pocet message
    BOOL UseMaxMessagesCount;
    // maximalni pocet message v poli
    int MaxMessagesCount;
    // v seznamu se zobrazuje posledni message
    BOOL ScrollToLatestMessage;
    // pokud prijde Error message, program pipne a vybehne nahoru
    BOOL ShowOnErrorMessage;
    // automaticke mazani message pri pripojeni noveho programu
    BOOL AutoClear;
    // hotkey
    WORD HotKey;
    WORD HotKeyClear;

    // viditelnost jednotlivych sloupcu
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

    // sirky jednotlivych sloupcu
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
