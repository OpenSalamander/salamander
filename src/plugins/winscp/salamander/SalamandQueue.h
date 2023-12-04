// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include <vector>
//---------------------------------------------------------------------------
#include "Queue.h"
#include "Salamand.h"
class TCriticalSection;
class TTerminalQueueStatus;
class TSalamandProgressThread;
//---------------------------------------------------------------------------
class TSalamandQueueController : protected CSalamanderGeneralLocal
{
    friend class TSalamandProgressThread;

public:
    TSalamandQueueController(HWND Parent, CPluginInterface* APlugin, TTerminalQueue* Queue);
    ~TSalamandQueueController();

    void Close();
    bool PathToRefresh(bool& Remote, AnsiString& Path);

protected:
    void UpdateMe(TSalamandProgressThread* Form);

private:
    TCriticalSection* FSection;

    typedef std::vector<TSalamandProgressThread*> TForms;
    TForms FForms;
    TTerminalQueueStatus* FQueueStatus;
    HWND FParent;
    TTerminalQueue* FQueue;
    TStrings* FPendingRefreshes;
    bool FClosing;

    void CloseAll(TForms& Forms);
    void ClosingAll();
    void __fastcall QueueListUpdate(TTerminalQueue* Queue);
    void __fastcall QueueItemUpdate(TTerminalQueue* Queue, TQueueItem* Item);
    void __fastcall QueueQueryUser(TObject* Sender,
                                   const AnsiString Query, TStrings* MoreMessages, int Answers,
                                   const TQueryParams* Params, int& Answer, TQueryType Type, void* Arg);
    void __fastcall QueuePromptUser(TTerminal* Terminal,
                                    TPromptKind Kind, AnsiString Name, AnsiString Instructions,
                                    TStrings* Prompts, TStrings* Results, bool& Result, void* Arg);
    void __fastcall QueueShowExtendedException(TTerminal* Terminal,
                                               Exception* E, void* Arg);
};
//---------------------------------------------------------------------------
