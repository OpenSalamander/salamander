// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include <Consts.hpp>
#include "Salamand.h"
#include "Salamander.rh"
#include "FileSystem.h"
#include "FileSystemInterface.h"
#include "SalamandConfiguration.h"
#include "SalamandQueue.h"

#include <Common.h>
#include <Exceptions.h>
#include <CoreMain.h>
#include <TextsWin.h>
#include <HelpWin.h>
#include <Progress.h>
#include <SynchronizeProgress.h>
#include <Authenticate.h>
#include <Queue.h>
#include <Tools.h>
#include <VCLCommon.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
CPluginFSInterface::CPluginFSInterface(CPluginInterface* APlugin) : CPluginFSInterfaceAbstract(), CSalamanderGeneralLocal(APlugin)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::CPluginFSInterface(%p, %p)", this, APlugin);

    FTerminal = NULL;
    FClosing = false;
    FFileList = NULL;
    FProgressForm = NULL;
    FPendingConnect = false;
    FWaitWindowShown = false;
    FPreloaded = false;
    FForceClose = false;
    FLastDataInterface = NULL;
    FSynchronizeProgressForm = NULL;
    FAuthenticateForm = NULL;
    FSynchronizeController = NULL;

    FQueue = NULL;
    FQueueController = NULL;
}
//---------------------------------------------------------------------------
CPluginFSInterface::~CPluginFSInterface()
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::~CPluginFSInterface(%p)", this);

    assert(!FWaitWindowShown);
    assert(FFileList == NULL);
    assert(FProgressForm == NULL);
    assert(FSynchronizeProgressForm == NULL);
    assert(FAuthenticateForm == NULL);
    assert(FSynchronizeController == NULL);

    if (FLastDataInterface != NULL)
    {
        FLastDataInterface->Invalidate();
    }
    // may not be initialised if we never succeed to connect
    if (FQueueController != NULL)
    {
        // it seems more safe to first close the queue controlled, but not dispose it
        FQueueController->Close();
    }
    delete FQueue;
    FQueue = NULL;
    delete FQueueController;
    FQueueController = NULL;
    SAFE_DESTROY(FTerminal);
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::Connect(TSessionData* Data)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::Connect(%p, %p, %p)",
                        this, FTerminal, Data);

    assert(!FTerminal);

    FPendingConnect = true;

    if (Data != NULL)
    {
        FTerminal = new TTerminal(Data, Configuration);
        try
        {
            FTerminal->OnQueryUser = TerminalQueryUser;
            FTerminal->OnPromptUser = TerminalPromptUser;
            FTerminal->OnDisplayBanner = TerminalDisplayBanner;
            FTerminal->OnShowExtendedException = TerminalShowExtendedException;
            FTerminal->OnReadDirectory = TerminalReadDirectory;
            FTerminal->OnStartReadDirectory = TerminalStartReadDirectory;
            FTerminal->OnFinished = OperationFinished;
            FTerminal->OnProgress = OperationProgress;
            FTerminal->OnClose = TerminalClose;
            FTerminal->OnInformation = TerminalInformation;

            ConnectTerminal(FTerminal);

            FPendingConnect = false;

            assert(FQueue == NULL);
            FQueue = new TTerminalQueue(FTerminal, Configuration);
            FQueue->TransfersLimit = 0; // no limit

            FQueueController = new TSalamandQueueController(
                SalamanderGeneral()->GetMainWindowHWND(), FPlugin, FQueue);
        }
        catch (...)
        {
            SAFE_DESTROY(FTerminal);
            delete FQueue;
            FQueue = NULL;
            throw;
        }

        ScheduleTimer();
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::ConnectTerminal(TTerminal* Terminal)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ConnectTerminal(%p, %p)",
                        this, Terminal);

    try
    {
        Terminal->Open();
    }
    __finally
    {
        if (FWaitWindowShown)
        {
            FWaitWindowShown = false;
            SalamanderGeneral()->DestroySafeWaitWindow();
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::Connect(int FSNameIndex, const AnsiString UserPart,
                                            bool IgnoreFileName)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::Connect(%p, %p, %d, %s)",
                        this, FTerminal, FSNameIndex, UserPart.c_str());

    SetParentWindow(SalamanderGeneral()->GetMsgBoxParent());

    TSessionData* SessionData;

    AnsiString Path;
    SessionData = CPluginInterfaceForFS::GetSessionData(FSNameIndex, UserPart, &Path);
    if (SessionData != NULL)
    {
        try
        {
            Connect(SessionData);
            if (IgnoreFileName)
            {
                Path = UnixExtractFilePath(Path);
            }

            if (!Path.IsEmpty())
            {
                ChangePath(FSNameIndex, NULL, FSNameIndex,
                           Path.c_str(), NULL, NULL, false, 3);
            }
        }
        __finally
        {
            delete SessionData;
        }
    }
    else
    {
        Abort();
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::RecryptPasswords()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ScheduleTimer(%p, %p)",
                        this, FTerminal);

    assert(FTerminal != NULL);
    if (FTerminal != NULL)
    {
        FTerminal->RecryptPasswords();
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::ScheduleTimer()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ScheduleTimer(%p, %p)",
                        this, FTerminal);

    SalamanderGeneral()->AddPluginFSTimer(500, this, 0);
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::ForceClose()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ForceClose(%p, %p)", this, FTerminal);

    FForceClose = true;
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::GetPanel(int& Panel, bool Opposite)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetPanel(%p, %p, %d)",
                        this, FTerminal, Opposite);

    bool Result = SalamanderGeneral()->GetPanelWithPluginFS(this, Panel);
    if (Result && Opposite)
    {
        Panel = (Panel == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT);
    }
    return Result;
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::GetPathType(const AnsiString Path,
                                                int& PathType)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetPathType(%p, %p, %s)",
                        this, FTerminal, Path.c_str());

    BOOL IsDir;
    int Error;
    char* SecondPart;
    bool Result;

    Result = SalamanderGeneral()->SalParsePath(Application->Handle, Path.c_str(),
                                               PathType, IsDir, SecondPart, NULL, NULL, false, NULL, NULL, &Error, 2 * MAX_PATH) ||
             (Error == SPP_INCOMLETEPATH);
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::SaveSession()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::SaveSession(%p, %p)",
                        this, FTerminal);

    if (!FTerminal->SessionData->Name.IsEmpty())
    {
        TSessionData* Data;
        Data = dynamic_cast<TSessionData*>(
            StoredSessions->FindByName(FTerminal->SessionData->Name));
        if (Data != NULL)
        {
            bool Changed = false;
            if (FTerminal->SessionData->UpdateDirectories)
            {
                Data->RemoteDirectory = FTerminal->CurrentDirectory;
                Changed = true;
            }

            if (Changed)
            {
                // modified only, implicit
                StoredSessions->Save(false, false);
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::CreateFileList(int Panel,
                                                   TOperationSide Side, bool Focused)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::CreateFileList(%p, %p, %d, %d, %d)",
                        this, FTerminal, Panel, Side, Focused);

    assert(!FFileList);
    FFileList = new TStringList();
    try
    {
        const CFileData* File;
        int IsDir;
        int Index = 0;

        File = Focused ? SalamanderGeneral()->GetPanelFocusedItem(Panel, &IsDir) : SalamanderGeneral()->GetPanelSelectedItem(Panel, &Index, &IsDir);
        while (File != NULL)
        {
            TObject* Param = (Side == osRemote) ? reinterpret_cast<TObject*>(File->PluginData) : NULL;
            assert((Side == osLocal) || (Param != NULL));
            AnsiString FileName = File->Name;
            if (Side == osLocal)
            {
                assert(ExtractFilePath(FileName).IsEmpty());
                FileName = IncludeTrailingBackslash(GetCurrentDir()) + FileName;
            }
            FFileList->AddObject(FileName, Param);

            File = Focused ? NULL : SalamanderGeneral()->GetPanelSelectedItem(Panel, &Index, &IsDir);
        }

        FFocusedFileList = Focused;
    }
    catch (...)
    {
        SAFE_DESTROY(FFileList);
        throw;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::CreateFileList(
    AnsiString SourcePath, SalEnumSelection2 Next, void* NextParam)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::CreateFileList(%p, %p, %p, %s)",
                        this, FTerminal, FFileList, SourcePath.c_str());

    assert(!FFileList);
    FFileList = new TStringList();
    try
    {
        const char* FileName;
        SourcePath = IncludeTrailingBackslash(SourcePath);

        while ((FileName = Next(NULL, 0, NULL, NULL, NULL, NULL, NULL, NextParam, NULL)) != NULL)
        {
            FFileList->Add(SourcePath + FileName);
        }
    }
    catch (...)
    {
        SAFE_DESTROY(FFileList);
        throw;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::CreateFileList(CFileData* File,
                                                   TOperationSide Side)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::CreateFileList(%p, %p, %p, %p, %d)",
                        this, FTerminal, FFileList, File, Side);

    assert(!FFileList);
    assert(File);
    FFileList = new TStringList();
    try
    {
        TObject* Param = (Side == osRemote) ? reinterpret_cast<TObject*>(File->PluginData) : NULL;
        assert((Side == osLocal) || (Param != NULL));
        FFileList->AddObject(File->Name, Param);
    }
    catch (...)
    {
        SAFE_DESTROY(FFileList);
        throw;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::DestroyFileList()
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::DestroyFileList(%p, %p, %p)",
                        this, FTerminal, FFileList);

    assert(FFileList);
    SAFE_DESTROY(FFileList);
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::DataInterfaceDestroyed(
    CPluginFSDataInterface* DataInterface)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::DataInterfaceDestroyed(%p, %p, %p, %p)",
                        this, FTerminal, FLastDataInterface, DataInterface);

    if (DataInterface == FLastDataInterface)
    {
        FLastDataInterface = NULL;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::DisconnectFromPanel(int Panel)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::DisconnectFromPanel(%p, %p, %d)",
                        this, FTerminal, Panel);

    SalamanderGeneral()->PostMenuExtCommand(
        Panel == PANEL_LEFT ? pmDisconnectLeft : pmDisconnectRight, TRUE);
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalClose(TObject* /*Sender*/)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::TerminalClose(%p, %p, %d, %d)",
                        this, FTerminal, FClosing, FForceClose);

    assert(!FClosing);
    FClosing = true;
    int Panel;
    if (!FForceClose && GetPanel(Panel))
    {
        DisconnectFromPanel(Panel);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalQueryUser(TObject* /*Sender*/,
                                                      const AnsiString Query, TStrings* MoreMessages, int Answers,
                                                      const TQueryParams* Params, int& Answer, TQueryType Type, void* /*Arg*/)
{
    CALL_STACK_MESSAGE8("CPluginFSInterface::TerminalQueryUser(%p, %p, %s, %p, %d, %p, %d)",
                        this, FTerminal, Query.c_str(), MoreMessages, Answers, Params, Type);

    AnsiString HelpKeyword;
    TMessageParams MessageParams(Params);
    AnsiString AQuery = Query;

    if (Params != NULL)
    {
        HelpKeyword = Params->HelpKeyword;

        if (FLAGSET(Params->Params, qpFatalAbort))
        {
            AQuery = FMTLOAD(WARN_FATAL_ERROR, (AQuery));

            if (!MessageParams.TimerMessage.IsEmpty())
            {
                MessageParams.TimerMessage = FMTLOAD(WARN_FATAL_ERROR, (MessageParams.TimerMessage));
            }
        }
    }

    Answer = MoreMessageDialog(AQuery, MoreMessages, Type, Answers, HelpKeyword,
                               &MessageParams);
}
//---------------------------------------------------------------------------
TAuthenticateForm* __fastcall CPluginFSInterface::MakeAuthenticateForm(
    TSessionData* Data)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::MakeAuthenticateForm(%p, %p)",
                        this, Data);
    UpdateWindow(SalamanderGeneral()->GetMainWindowHWND());
    TAuthenticateForm* Dialog = SafeFormCreate<TAuthenticateForm>();
    Dialog->Init(Data);
    return Dialog;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalPromptUser(TTerminal* Terminal,
                                                       TPromptKind Kind, AnsiString Name, AnsiString Instructions,
                                                       TStrings* Prompts, TStrings* Results, bool& Result, void* /*Arg*/)
{
    CALL_STACK_MESSAGE8("CPluginFSInterface::TerminalPromptUser(%p, %p, %d, %s, %s, %d, %s)",
                        this, Terminal, Kind, Name.c_str(), Instructions.c_str(), Prompts->Count,
                        ((Prompts->Count > 0) ? Prompts->Strings[0].c_str() : "<no prompt>"));

    if ((Kind == pkPrompt) && (FAuthenticateForm == NULL) &&
        (Terminal->Status != ssOpening))
    {
        if (Prompts->Count > 0)
        {
            assert(Instructions.IsEmpty());
            assert(Prompts->Count == 1);
            assert(bool(Prompts->Objects[0]));
            AnsiString AResult = Results->Strings[0];
            Result = InputDialog(Name, Prompts->Strings[0], AResult);
            if (Result)
            {
                Results->Strings[0] = AResult;
            }
        }
    }
    else
    {
        TAuthenticateForm* AuthenticateForm = FAuthenticateForm;
        if (AuthenticateForm == NULL)
        {
            AuthenticateForm = MakeAuthenticateForm(Terminal->SessionData);
        }

        try
        {
            Result = AuthenticateForm->PromptUser(Kind, Name, Instructions, Prompts, Results,
                                                  (FAuthenticateForm != NULL), Terminal->StoredCredentialsTried);
        }
        __finally
        {
            if (FAuthenticateForm == NULL)
            {
                delete AuthenticateForm;
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalDisplayBanner(
    TTerminal* Terminal, AnsiString SessionName,
    const AnsiString& Banner, bool& NeverShowAgain, int Options)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::TerminalDisplayBanner(%p, %p, %s, %d, %d)",
                        this, Terminal, SessionName.c_str(), Banner.Length(), Options);

    assert(FAuthenticateForm != NULL);
    TAuthenticateForm* AuthenticateForm = FAuthenticateForm;
    if (AuthenticateForm == NULL)
    {
        AuthenticateForm = MakeAuthenticateForm(Terminal->SessionData);
    }

    try
    {
        AuthenticateForm->Banner(Banner, NeverShowAgain, Options);
    }
    __finally
    {
        if (FAuthenticateForm == NULL)
        {
            delete AuthenticateForm;
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalInformation(
    TTerminal* Terminal, const AnsiString& Str, bool /*Status*/, bool Active)
{
    if (Active)
    {
        if (Terminal->Status == ssOpening)
        {
            bool ShowPending = false;
            if (FAuthenticateForm == NULL)
            {
                FAuthenticateForm = MakeAuthenticateForm(Terminal->SessionData);
                ShowPending = true;
            }
            FAuthenticateForm->Log(Str);
            if (ShowPending)
            {
                FAuthenticateForm->ShowAsModal();
            }
        }
    }
    else
    {
        SAFE_DESTROY(FAuthenticateForm);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalShowExtendedException(
    TTerminal* /*Terminal*/, Exception* E, void* /*Arg*/)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::TerminalShowExtendedException(%p, %p, %p)",
                        this, FTerminal, E);

    ShowExtendedException(E);
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::OperationProgress(
    TFileOperationProgressType& ProgressData, TCancelStatus& /*Cancel*/)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::OperationProgress(%p, %p)",
                        this, FTerminal);

    // operation is being executed and we still didn't show up progress form
    if (ProgressData.InProgress && !FProgressForm)
    {
        //assert(Screen && Screen->ActiveCustomForm);
        FProgressForm = new TProgressForm(Application);
        FProgressForm->DeleteToRecycleBin = FTerminal->SessionData->DeleteToRecycleBin;
        FProgressForm->AllowMinimize = false;
    }
    // operation is finished (or terminated), so we hide progress form
    else if (!ProgressData.InProgress && FProgressForm)
    {
        SAFE_DESTROY(FProgressForm);
    }

    if (FProgressForm)
    {
        FProgressForm->SetProgressData(ProgressData);
        if (FProgressForm->Cancel > ProgressData.Cancel)
        {
            ProgressData.Cancel = FProgressForm->Cancel;
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::OperationFinished(TFileOperation Operation,
                                                      TOperationSide Side, bool /*DragDrop*/, const AnsiString& FileName,
                                                      bool Success, TOnceDoneOperation& OnceDoneOperation)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::OperationFinished(%p, %p, %p)",
                        this, FTerminal, FProgressForm);

    USEDPARAM(Side);

    if (Success && (FSynchronizeController != NULL))
    {
        if (Operation == foCopy)
        {
            assert(Side == osLocal);
            FSynchronizeController->LogOperation(soUpload, FileName);
        }
        else if (Operation == foDelete)
        {
            assert(Side == osRemote);
            FSynchronizeController->LogOperation(soDelete, FileName);
        }
    }

    if (FProgressForm)
    {
        OnceDoneOperation = FProgressForm->OnceDoneOperation;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalStartReadDirectory(TObject* /*Sender*/)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::TerminalStartReadDirectory(%p, %p, %p)",
                        this, FTerminal, FLastDataInterface);

    if (FLastDataInterface)
    {
        FLastDataInterface->Invalidate();
        FLastDataInterface = NULL;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalReadDirectory(TObject* /*Sender*/,
                                                          bool /*ReloadOnly*/)
{
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::HandleException(Exception* E)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::HandleException(%p, %p, %p)",
                        this, FTerminal, E);

    if (FTerminal != NULL)
    {
        FTerminal->ShowExtendedException(E);
    }
    else
    {
        ShowExtendedException(E);
    }
    bool Result = !E->InheritsFrom(__classid(EFatal));
    return Result;
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::UnixPaths()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::UnixPaths(%p, %p)",
                        this, FTerminal);

    assert(FTerminal);
    assert(FTerminal->Active);

    // IsEmpty() condition added because of suspect bug #340
    return !FTerminal->CurrentDirectory.IsEmpty() &&
           (FTerminal->CurrentDirectory[1] == '/');
}
//---------------------------------------------------------------------------
AnsiString __fastcall CPluginFSInterface::CurrentPath()
{
    return FTerminal->CurrentDirectory;
}
//---------------------------------------------------------------------------
AnsiString __fastcall CPluginFSInterface::CurrentFullPath()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::CurrentFullPath(%p, %p)",
                        this, FTerminal);

    assert(FTerminal);
    assert(FTerminal->Active);

    return FullPath(FTerminal->CurrentDirectory);
}
//---------------------------------------------------------------------------
AnsiString __fastcall CPluginFSInterface::FSName()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::FSName(%p, %p)", this, FTerminal);

    assert(FTerminal);
    AnsiString Result;
    if ((FTerminal != NULL) && (FTerminal->Status == ssOpened))
    {
        Result = FPlugin->GetFSName(FTerminal->FSProtocol, true);
    }
    else
    {
        Result = FPlugin->GetFSName(FTerminal->SessionData->FSProtocol, false);
    }
    return Result;
}
//---------------------------------------------------------------------------
AnsiString __fastcall CPluginFSInterface::FullFSPath(AnsiString Path)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::FullFSPath(%p, %p, %s)",
                        this, FTerminal, Path.c_str());

    AnsiString Result = FORMAT("%s:%s", (FSName(), Path));
    return Result;
}
//---------------------------------------------------------------------------
AnsiString __fastcall CPluginFSInterface::FullPath(AnsiString Path)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::FullPath(%p, %p, %s)",
                        this, FTerminal, Path.c_str());

    if (UnixPaths() && !Path.IsEmpty())
    {
        assert(Path[1] == '/');
        Path.Delete(1, 1);
    }
    return FORMAT("//%s/%s",
                  (FTerminal->SessionData->SessionName, Path));
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::StripFS(AnsiString& Path, bool AnyFS)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::StripFS(%p, %p, %s, %d)",
                        this, FTerminal, Path.c_str(), AnyFS);

    int P = Path.Pos(":");
    bool Result = (P > 0);
    if (Result)
    {
        Result = AnyFS || SameText(Path.SubString(1, P - 1), FSName());
        Path.Delete(1, P);
    }
    return Result;
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::IsOurUserPart(const AnsiString UserPart)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::IsOurUserPart(%p, %p, %s)",
                        this, FTerminal, UserPart.c_str());

    bool Result;

    try
    {
        assert(FTerminal);

        AnsiString ConnectInfo;
        AnsiString HostName;
        int PortNumber;
        AnsiString UserName;

        CPluginInterfaceForFS::ParseUserPart(UserPart, UnixPaths(), &ConnectInfo,
                                             &HostName, &PortNumber, &UserName, NULL, NULL);

        Result = !CPluginInterfaceForFS::GetForceNewSession() &&
                 (ConnectInfo.IsEmpty() ||
                  (ConnectInfo == FTerminal->SessionData->Name) ||
                  ((HostName == FTerminal->SessionData->HostName) &&
                   (UserName.IsEmpty() || (UserName == FTerminal->SessionData->UserName)) &&
                   ((PortNumber < 0) || (PortNumber == FTerminal->SessionData->PortNumber))));
    }
    catch (Exception& E)
    {
        Result = HandleException(&E);
    }

    return Result;
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::AffectsCurrentPath(const AnsiString Path,
                                                       bool IncludingSubDirs)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::AffectsCurrentPath(%p, %p, %s, %d)",
                        this, FTerminal, Path.c_str(), IncludingSubDirs);

    AnsiString Current = FullFSPath(CurrentFullPath());
    return UnixComparePaths(Path, Current) ||
           (IncludingSubDirs &&
            UnixComparePaths(Path, Current.SubString(1, Path.Length())));
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::PathChanged(const AnsiString Path,
                                                bool IncludingSubDirs)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::PathChanged(%p, %p, %s, %d)",
                        this, FTerminal, Path.c_str(), IncludingSubDirs);

    if (FTerminal && FTerminal->Active)
    {
        if (AffectsCurrentPath(Path, IncludingSubDirs))
        {
            // tohle byvalo pocitadlo, problem je, ze pri vicenasobnem
            // PostRefreshPanelFS() se ListCurrentPath zavola jen jednou
            FPreloaded = true;
        }
        SalamanderGeneral()->PostChangeOnPathNotification(
            Path.c_str(), IncludingSubDirs);
    }
    else
    {
        SalamanderGeneral()->PostRefreshPanelFS(this);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::OurPathChanged(const AnsiString Path,
                                                   bool IncludingSubDirs)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::OurPathChanged(%p, %p, %s, %d)",
                        this, FTerminal, Path.c_str(), IncludingSubDirs);

    if (FTerminal && FTerminal->Active)
    {
        PathChanged(FullFSPath(FullPath(FTerminal->AbsolutePath(Path, false))),
                    IncludingSubDirs);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::CurrentPathChanged(bool IncludingSubDirs)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::CurrentPathChanged(%p, %p, %d)",
                        this, FTerminal, IncludingSubDirs);

    PathChanged(FTerminal && FTerminal->Active ? FullFSPath(CurrentFullPath()) : AnsiString(), IncludingSubDirs);
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::RefreshPanel()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::RefreshPanel(%p, %p)",
                        this, FTerminal);

    FPreloaded = true;
    SalamanderGeneral()->PostRefreshPanelFS(this);
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::RemoteTransfer(AnsiString TargetDirectory,
                                                   AnsiString FileMask, bool SubDirs, bool NoConfirm, bool Move)
{
    CALL_STACK_MESSAGE8("CPluginFSInterface::RemoteTransfer(%p, %p, %s, %s, %d, %d, %d)",
                        this, FTerminal, TargetDirectory.c_str(), FileMask.c_str(), SubDirs, NoConfirm, Move);

    TargetDirectory = UnixIncludeTrailingBackslash(TargetDirectory);
    bool Result =
        (Move || EnsureCommandSessionFallback(fcRemoteCopy));

    if (Result && !NoConfirm)
    {
        if (Move)
        {
            Result = DoRemoteMoveDialog(TargetDirectory, FileMask);
        }
        else
        {
            void* DummySession = NULL;
            bool DummyDirectCopy = false;
            Result = DoRemoteCopyDialog(NULL, NULL, drcDisallow, DummySession,
                                        TargetDirectory, FileMask, DummyDirectCopy);
        }
    }

    if (Result)
    {
        try
        {
            if (Move)
            {
                FTerminal->MoveFiles(FFileList, TargetDirectory, FileMask);
            }
            else
            {
                FTerminal->CopyFiles(FFileList, TargetDirectory, FileMask);
            }
        }
        __finally
        {
            // must be called for "duplicate" too, as the current directory is
            // reloaded always, so we must force Salamander to do the same
            CurrentPathChanged(SubDirs);
            OurPathChanged(TargetDirectory, SubDirs);
        }
    }
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::RemoteTransfer(bool Move)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::RemoteTransfer(%p, %p, %d)",
                        this, FTerminal, Move);

    int Panel;

    if (GetPanel(Panel))
    {
        int SelectedFiles;
        int SelectedDirs;
        bool Focused;

        Focused =
            !SalamanderGeneral()->GetPanelSelection(Panel, &SelectedFiles, &SelectedDirs) ||
            (SelectedFiles + SelectedDirs == 0);

        CreateFileList(Panel, osRemote, Focused);
        try
        {
            RemoteTransfer(FTerminal->CurrentDirectory, "*.*", !Focused && (SelectedDirs > 0),
                           false, Move);
        }
        __finally
        {
            DestroyFileList();
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::SynchronizeDirectories(
    AnsiString& LocalDirectory, AnsiString& RemoteDirectory)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::SynchronizeDirectories(%p, %p)",
                        this, FTerminal);

    RemoteDirectory = FTerminal->CurrentDirectory;
    LocalDirectory = "";
    int Panel;
    if (GetPanel(Panel, true))
    {
        char Buf[MAX_PATH];
        int PathType;
        if (SalamanderGeneral()->GetPanelPath(Panel, Buf, sizeof(Buf), &PathType, NULL))
        {
            if (PathType != PATH_TYPE_WINDOWS)
            {
                SimpleErrorDialog(FMTLOAD(SAL_PATH_NOT_SUPPORTED, (Buf)));
            }
            else
            {
                LocalDirectory = Buf;
            }
        }
    }
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::SynchronizeAllowSelectedOnly()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::SynchronizeAllowSelectedOnly(%p, %p)",
                        this, FTerminal);

    int SelectedFiles;
    int SelectedDirs;

    return (SalamanderGeneral()->GetPanelSelection(PANEL_SOURCE, &SelectedFiles, &SelectedDirs) &&
            (SelectedFiles + SelectedDirs > 0)) ||
           (SalamanderGeneral()->GetPanelSelection(PANEL_TARGET, &SelectedFiles, &SelectedDirs) &&
            (SelectedFiles + SelectedDirs > 0));
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::GetSynchronizeOptions(
    int Params, TSynchronizeOptions& Options)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::GetSynchronizeOptions(%p, %p, %d, %p)",
                        this, FTerminal, Params, &Options);

    if (FLAGSET(Params, spSelectedOnly) && SynchronizeAllowSelectedOnly())
    {
        Options.Filter = new TStringList();
        Options.Filter->CaseSensitive = false;
        Options.Filter->Duplicates = dupAccept;

        for (int PanelI = 0; PanelI < 2; PanelI++)
        {
            int Panel = (PanelI == 0 ? PANEL_SOURCE : PANEL_TARGET);
            const CFileData* FileData;
            int Index = 0;
            FileData = SalamanderGeneral()->GetPanelSelectedItem(Panel, &Index, NULL);
            while (FileData != NULL)
            {
                Options.Filter->Add(FileData->Name);
                FileData = SalamanderGeneral()->GetPanelSelectedItem(Panel, &Index, NULL);
            }
        }

        Options.Filter->Sort();
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::FullSynchronize(bool Source)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::FullSynchronize(%p, %p, %d)",
                        this, FTerminal, Source);

    bool SaveMode = !(GUIConfiguration->SynchronizeModeAuto < 0);
    TSynchronizeMode Mode =
        (SaveMode ? (TSynchronizeMode)GUIConfiguration->SynchronizeModeAuto : (Source ? smLocal : smRemote));
    int Params = GUIConfiguration->SynchronizeParams;
    AnsiString RemoteDirectory;
    AnsiString LocalDirectory;
    SynchronizeDirectories(LocalDirectory, RemoteDirectory);

    bool SaveSettings = false;
    int Options =
        FLAGMASK(!FTerminal->IsCapable[fcTimestampChanging], fsoDisableTimestamp) |
        FLAGMASK(SynchronizeAllowSelectedOnly(), fsoAllowSelectedOnly) |
        fsoDoNotUsePresets;
    TCopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
    TUsableCopyParamAttrs CopyParamAttrs = FTerminal->UsableCopyParamAttrs(0);
    if (DoFullSynchronizeDialog(Mode, Params, LocalDirectory, RemoteDirectory,
                                &CopyParam, SaveSettings, SaveMode, Options, CopyParamAttrs))
    {
        TSynchronizeOptions SynchronizeOptions;
        GetSynchronizeOptions(Params, SynchronizeOptions);

        if (SaveSettings)
        {
            GUIConfiguration->SynchronizeParams = Params;

            if (SaveMode)
            {
                GUIConfiguration->SynchronizeModeAuto = Mode;
            }
        }

        TSynchronizeChecklist* Checklist = NULL;
        try
        {
            try
            {
                FSynchronizeProgressForm = new TSynchronizeProgressForm(Application, false, true);
                FSynchronizeProgressForm->Start();

                Checklist = FTerminal->SynchronizeCollect(LocalDirectory, RemoteDirectory,
                                                          static_cast<TTerminal::TSynchronizeMode>(Mode),
                                                          &CopyParam, Params | spNoConfirmation, TerminalSynchronizeDirectory,
                                                          &SynchronizeOptions);
            }
            __finally
            {
                SAFE_DESTROY(FSynchronizeProgressForm);
            }

            if (Checklist->Count == 0)
            {
                MessageDialog(LoadStr(COMPARE_NO_DIFFERENCES), qtInformation, qaOK,
                              HELP_SYNCHRONIZE_NO_DIFFERENCES);
            }
            else if (FLAGCLEAR(Params, spPreviewChanges) ||
                     DoSynchronizeChecklistDialog(Checklist, Mode, Params,
                                                  LocalDirectory, RemoteDirectory, NULL))
            {
                try
                {
                    FSynchronizeProgressForm = new TSynchronizeProgressForm(Application, false, false);
                    FSynchronizeProgressForm->Start();

                    FTerminal->SynchronizeApply(Checklist, LocalDirectory, RemoteDirectory,
                                                &CopyParam, Params | spNoConfirmation, TerminalSynchronizeDirectory);
                }
                __finally
                {
                    SAFE_DESTROY(FSynchronizeProgressForm);
                    CurrentPathChanged(true);
                }
            }
        }
        __finally
        {
            delete Checklist;
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::Synchronize(const AnsiString LocalDirectory,
                                                const AnsiString RemoteDirectory, TSynchronizeMode Mode,
                                                const TCopyParamType& CopyParam, int Params, TSynchronizeChecklist** Checklist,
                                                TSynchronizeOptions* Options)
{
    CALL_STACK_MESSAGE9("CPluginFSInterface::Synchronize(%p, %p, %s, %s, %d, %d, %p, %p)",
                        this, FTerminal, LocalDirectory.c_str(), RemoteDirectory.c_str(), Mode,
                        Params, Checklist, Options);

    TSynchronizeChecklist* AChecklist = NULL;
    try
    {
        FSynchronizeProgressForm = new TSynchronizeProgressForm(Application, false, true);
        if (FLAGCLEAR(Params, TTerminal::spDelayProgress))
        {
            FSynchronizeProgressForm->Start();
        }

        AChecklist = FTerminal->SynchronizeCollect(LocalDirectory, RemoteDirectory,
                                                   static_cast<TTerminal::TSynchronizeMode>(Mode),
                                                   &CopyParam, Params | spNoConfirmation, TerminalSynchronizeDirectory,
                                                   Options);

        SAFE_DESTROY(FSynchronizeProgressForm);

        FSynchronizeProgressForm = new TSynchronizeProgressForm(Application, false, false);
        if (FLAGCLEAR(Params, TTerminal::spDelayProgress))
        {
            FSynchronizeProgressForm->Start();
        }

        FTerminal->SynchronizeApply(AChecklist, LocalDirectory, RemoteDirectory,
                                    &CopyParam, Params | spNoConfirmation, TerminalSynchronizeDirectory);
    }
    __finally
    {
        if (Checklist == NULL)
        {
            delete AChecklist;
        }
        else
        {
            *Checklist = AChecklist;
        }

        SAFE_DESTROY(FSynchronizeProgressForm);
        CurrentPathChanged(true);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::Synchronize()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::Synchronize(%p, %p)",
                        this, FTerminal);

    TSynchronizeParamType Params;
    int UnusedParams =
        (GUIConfiguration->SynchronizeParams &
         (spPreviewChanges | spTimestamp | spNotByTime | spBySize));
    Params.Params = GUIConfiguration->SynchronizeParams & ~UnusedParams;
    Params.Options = GUIConfiguration->SynchronizeOptions;
    SynchronizeDirectories(Params.LocalDirectory, Params.RemoteDirectory);
    bool SaveSettings = false;
    TSynchronizeController Controller(&DoSynchronize, &DoSynchronizeInvalid,
                                      &DoSynchronizeTooManyDirectories);
    assert(FSynchronizeController == NULL);
    FSynchronizeController = &Controller;
    try
    {
        TCopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
        int CopyParamAttrs = FTerminal->UsableCopyParamAttrs(0).Upload;
        int Options =
            soDoNotUsePresets | soNoMinimize |
            FLAGMASK(SynchronizeAllowSelectedOnly(), soAllowSelectedOnly);
        if (DoSynchronizeDialog(Params, &CopyParam, Controller.StartStop,
                                SaveSettings, Options, CopyParamAttrs, GetSynchronizeOptions, false))
        {
            if (SaveSettings)
            {
                GUIConfiguration->SynchronizeParams = Params.Params | UnusedParams;
                GUIConfiguration->SynchronizeOptions = Params.Options;
            }
            else
            {
                if (FLAGSET(GUIConfiguration->SynchronizeOptions, soSynchronizeAsk) &&
                    FLAGCLEAR(Params.Options, soSynchronizeAsk) &&
                    FLAGSET(Params.Options, soSynchronize))
                {
                    GUIConfiguration->SynchronizeOptions =
                        (GUIConfiguration->SynchronizeOptions & ~soSynchronizeAsk) |
                        soSynchronize;
                }
            }
        }
    }
    __finally
    {
        FSynchronizeController = NULL;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::DoSynchronizeTooManyDirectories(
    TSynchronizeController* /*Sender*/, int& MaxDirectories)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::DoSynchronizeTooManyDirectories(%p, %p, %d)",
                        this, FTerminal, MaxDirectories);

    if (MaxDirectories < GUIConfiguration->MaxWatchDirectories)
    {
        MaxDirectories = GUIConfiguration->MaxWatchDirectories;
    }
    else
    {
        TMessageParams Params(mpNeverAskAgainCheck);
        int Result = MessageDialog(
            FMTLOAD(TOO_MANY_WATCH_DIRECTORIES, (MaxDirectories, MaxDirectories)),
            qtConfirmation, qaYes | qaNo, HELP_TOO_MANY_WATCH_DIRECTORIES, &Params);

        if ((Result == qaYes) || (Result == qaNeverAskAgain))
        {
            MaxDirectories *= 2;
            if (Result == qaNeverAskAgain)
            {
                GUIConfiguration->MaxWatchDirectories = MaxDirectories;
            }
        }
        else
        {
            Abort();
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::DoSynchronize(
    TSynchronizeController* /*Sender*/, const AnsiString LocalDirectory,
    const AnsiString RemoteDirectory, const TCopyParamType& CopyParam,
    const TSynchronizeParamType& Params, TSynchronizeChecklist** Checklist,
    TSynchronizeOptions* Options, bool Full)
{
    CALL_STACK_MESSAGE8("CPluginFSInterface::DoSynchronize(%p, %p, %s, %s, %p, %p, %d)",
                        this, FTerminal, LocalDirectory.c_str(), RemoteDirectory.c_str(), Checklist, Options, Full);

    try
    {
        int PParams = Params.Params;
        if (!Full)
        {
            PParams |= TTerminal::spNoRecurse | TTerminal::spUseCache |
                       TTerminal::spDelayProgress | TTerminal::spSubDirs;
        }
        Synchronize(LocalDirectory, RemoteDirectory, smRemote, CopyParam,
                    PParams, Checklist, Options);
    }
    catch (Exception& E)
    {
        HandleException(&E);
        throw;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::DoSynchronizeInvalid(
    TSynchronizeController* /*Sender*/, const AnsiString Directory,
    const AnsiString ErrorStr)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::DoSynchronizeInvalid(%p, %p, %s, %s)",
                        this, FTerminal, Directory.c_str(), ErrorStr.c_str());

    if (!Directory.IsEmpty())
    {
        SimpleErrorDialog(FMTLOAD(WATCH_ERROR_DIRECTORY, (Directory)), ErrorStr);
    }
    else
    {
        SimpleErrorDialog(LoadStr(WATCH_ERROR_GENERAL), ErrorStr);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::TerminalSynchronizeDirectory(
    const AnsiString LocalDirectory, const AnsiString RemoteDirectory,
    bool& Continue, bool /*Collect*/)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::TerminalSynchronizeDirectory(%p, %p, %s, %s)",
                        this, FTerminal, LocalDirectory.c_str(), RemoteDirectory.c_str());

    assert(FSynchronizeProgressForm != NULL);
    if (!FSynchronizeProgressForm->Started)
    {
        FSynchronizeProgressForm->Start();
    }
    FSynchronizeProgressForm->SetData(LocalDirectory, RemoteDirectory, Continue);
}
//---------------------------------------------------------------------------
int __fastcall CPluginFSInterface::FileMenu(HWND Parent, int X, int Y)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::FileMenu(%p, %p, %x, %d, %d)",
                        this, FTerminal, Parent, X, Y);

    int fmOwn = 0x1000;
    int fmMoveTo = fmOwn + 1;
    int fmCopyTo = fmOwn + 2;
    int SalamCommands[] = {SALCMD_VIEW, SALCMD_ALTVIEW, SALCMD_QUICKRENAME,
                           SALCMD_COPY, SALCMD_MOVE, SALCMD_DELETE, SALCMD_CHANGEATTRS};

    HMENU Menu = CreatePopupMenu();
    int LastType = sctyUnknown;
    int ItemIndex = 0;
    for (int Index = 0; Index < LENOF(SalamCommands); Index++)
    {
        char Name[200];
        BOOL Enabled;
        int Type;

        if (SalamanderGeneral()->GetSalamanderCommand(SalamCommands[Index],
                                                      Name, sizeof(Name), &Enabled, &Type))
        {
            MENUITEMINFO MI;

            if ((ItemIndex > 0) && (Type != LastType))
            {
                memset(&MI, 0, sizeof(MI));
                MI.cbSize = sizeof(MI);
                MI.fMask = MIIM_TYPE;
                MI.fType = MFT_SEPARATOR;
                InsertMenuItem(Menu, ItemIndex, TRUE, &MI);
                ItemIndex++;
            }
            LastType = Type;

            memset(&MI, 0, sizeof(MI));
            MI.cbSize = sizeof(MI);
            MI.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
            MI.fType = MFT_STRING;
            MI.wID = SalamCommands[Index] + 1;
            MI.dwTypeData = Name;
            MI.cch = strlen(Name);
            MI.fState = Enabled ? MFS_ENABLED : MFS_DISABLED;
            InsertMenuItem(Menu, ItemIndex, TRUE, &MI);
            ItemIndex++;

            if ((SalamCommands[Index] == SALCMD_COPY) ||
                (SalamCommands[Index] == SALCMD_MOVE))
            {
                AnsiString Caption;
                if (SalamCommands[Index] == SALCMD_COPY)
                {
                    Caption = LoadStr(SAL_COPY_TO);
                    MI.wID = fmCopyTo;
                    Enabled = true;
                }
                else
                {
                    Caption = LoadStr(SAL_MOVE_TO);
                    MI.wID = fmMoveTo;
                    Enabled = FTerminal->IsCapable[fcRemoteMove];
                }
                MI.dwTypeData = Caption.c_str();
                MI.cch = Caption.Length();
                MI.fState = Enabled ? MFS_ENABLED : MFS_DISABLED;
                InsertMenuItem(Menu, ItemIndex, TRUE, &MI);
                ItemIndex++;
            }
        }
    }

    int Result = TrackPopupMenuEx(Menu,
                                  TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, X, Y, Parent, NULL);

    if (Result != 0)
    {
        if ((Result & fmOwn) == 0)
        {
            SalamanderGeneral()->PostSalamanderCommand(Result - 1);
        }
        else if (Result == fmMoveTo)
        {
            RemoteTransfer(true);
        }
        else if (Result == fmCopyTo)
        {
            RemoteTransfer(false);
        }
    }
    return 0;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::ProcessQueue()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ProcessQueue(%p, %p)",
                        this, FTerminal);

    bool Remote;
    AnsiString Path;
    while (FQueueController->PathToRefresh(Remote, Path))
    {
        assert(!Path.IsEmpty());
        if (Remote)
        {
            OurPathChanged(Path, true);
        }
        else
        {
            PathChanged(Path, true);
        }
    }
}
//---------------------------------------------------------------------------
bool __fastcall CPluginFSInterface::EnsureCommandSessionFallback(int Capability)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::EnsureCommandSessionFallback(%p, %p, %d)",
                        this, FTerminal, Capability);

    bool Result = FTerminal->IsCapable[TFSCapability(Capability)] ||
                  FTerminal->CommandSessionOpened;

    if (!Result)
    {
        if (!GUIConfiguration->ConfirmCommandSession)
        {
            Result = true;
        }
        else
        {
            TMessageParams Params(mpNeverAskAgainCheck);
            int Answer = MessageDialog(FMTLOAD(PERFORM_ON_COMMAND_SESSION,
                                               (FTerminal->GetFileSystemInfo().ProtocolName,
                                                FTerminal->GetFileSystemInfo().ProtocolName)),
                                       qtConfirmation,
                                       qaOK | qaCancel, HELP_PERFORM_ON_COMMAND_SESSION, &Params);
            if (Answer == qaNeverAskAgain)
            {
                GUIConfiguration->ConfirmCommandSession = false;
                Result = true;
            }
            else if (Answer == qaOK)
            {
                Result = true;
            }
        }

        if (Result)
        {
            ConnectTerminal(FTerminal->CommandSession);
        }
    }

    return Result;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::FileSystemInfo()
{
    assert(FTerminal);

    const TSessionInfo& SessionInfo = FTerminal->GetSessionInfo();
    const TFileSystemInfo& FileSystemInfo = FTerminal->GetFileSystemInfo(true);
    TGetSpaceAvailable OnGetSpaceAvailable = NULL;
    if (FTerminal->IsCapable[fcCheckingSpaceAvailable])
    {
        OnGetSpaceAvailable = GetSpaceAvailable;
    }
    DoFileSystemInfoDialog(SessionInfo, FileSystemInfo, FTerminal->CurrentDirectory,
                           OnGetSpaceAvailable);
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetCurrentPath(char* UserPart)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::GetCurrentPath(%p, %p, %d, %p)",
                        this, FTerminal, FClosing, UserPart);

    if (FClosing)
    {
        return FALSE;
    }

    strcpy(UserPart, CurrentFullPath().c_str());

    return TRUE;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetFullName(CFileData& File, int /*IsDir*/,
                                            char* Buf, int BufSize)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::GetFullName(%p, %p, %d, %p, %d)",
                        this, FTerminal, FClosing, Buf, BufSize);

    if (FClosing)
    {
        return FALSE;
    }

    TRemoteFile* RemoteFile = reinterpret_cast<TRemoteFile*>(File.PluginData);
    assert(RemoteFile);
    AnsiString FullName = FullPath(RemoteFile->FullFileName);
    BOOL Result = (FullName.Length() < BufSize);
    if (Result)
    {
        strcpy(Buf, FullName.c_str());
    }
    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetFullFSPath(HWND Parent,
                                              const char* /*FSName*/, char* Path, int PathSize, BOOL& Success)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::GetFullFSPath(%p, %p, %d, %x, %s, %d)",
                        this, FTerminal, FClosing, Parent, Path, PathSize);

    if (FClosing)
    {
        return FALSE;
    }

    assert(FTerminal);
    assert(FTerminal->Active);

    try
    {
        SetParentWindow(Parent);

        AnsiString APath = FullFSPath(FullPath(FTerminal->AbsolutePath(Path, false)));
        strncpy(Path, APath.c_str(), PathSize);
        Path[PathSize - 1] = '\0';

        Success = TRUE;
    }
    catch (Exception& E)
    {
        Success = FALSE;
        HandleException(&E);
    }

    return TRUE;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetRootPath(char* UserPart)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetRootPath(%p, %p, %p)",
                        this, FTerminal, UserPart);

    strcpy(UserPart, "/");
    return TRUE;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::IsCurrentPath(int CurrentFSNameIndex,
                                              int FSNameIndex, const char* UserPart)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::IsCurrentPath(%p, %p, %d, %d, %s)",
                        this, FTerminal, CurrentFSNameIndex, FSNameIndex, UserPart);

    if (FClosing)
    {
        return FALSE;
    }

    return (CurrentFSNameIndex == FSNameIndex) &&
           UnixComparePaths(UserPart, CurrentFullPath());
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::IsOurPath(int CurrentFSNameIndex,
                                          int FSNameIndex, const char* UserPart)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::IsOurPath(%p, %p, %d, %d, %s)",
                        this, FTerminal, CurrentFSNameIndex, FSNameIndex, UserPart);

    if (FClosing)
    {
        return FALSE;
    }

    BOOL Result = (CurrentFSNameIndex == FSNameIndex);

    if (Result)
    {
        Result = IsOurUserPart(UserPart);
    }
    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::ChangePath(int CurrentFSNameIndex,
                                           char* FSName, int FSNameIndex, const char* UserPart,
                                           char* /*CutFileName*/, BOOL* /*PathWasCut*/, BOOL /*ForceRefresh*/, int /*Mode*/)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::ChangePath(%p, %p, %d, %s, %d, %s)",
                        this, FTerminal, CurrentFSNameIndex, FSName, FSNameIndex, UserPart);

    USEDPARAM(CurrentFSNameIndex);

    if (FClosing)
    {
        return FALSE;
    }

    BOOL Result = TRUE;

    try
    {
        if (FPendingConnect)
        {
            Connect(FSNameIndex, UserPart, false);
        }
        else
        {
            assert(FTerminal);
            assert(FTerminal->Active);

            assert(IsOurPath(CurrentFSNameIndex, FSNameIndex, UserPart));

            AnsiString ConnectInfo;
            Result = (strlen(UserPart) == 0);

            if (!Result)
            {
                AnsiString Path;

                CPluginInterfaceForFS::ParseUserPart(UserPart, UnixPaths(), &ConnectInfo,
                                                     NULL, NULL, NULL, NULL, &Path);

                if (!ConnectInfo.IsEmpty() && Path.IsEmpty())
                {
                    Path = "/";
                }

                if (!UnixComparePaths(Path, FTerminal->CurrentDirectory))
                {
                    FTerminal->ExceptionOnFail = true;
                    try
                    {
                        FTerminal->ChangeDirectory(Path);
                    }
                    __finally
                    {
                        FTerminal->ExceptionOnFail = false;
                    }
                }
                Result = TRUE;
            }
        }

        if (FSName != NULL)
        {
            strcpy(FSName, this->FSName().c_str());
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        Result = FALSE;
    }

    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* Dir,
                                                CPluginDataInterfaceAbstract*& PluginData, int& IconsType, BOOL ForceRefresh)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::ListCurrentPath(%p, %p, %p, %d)",
                        this, FTerminal, Dir, ForceRefresh);

    if (FClosing)
    {
        return FALSE;
    }

    assert(FTerminal);
    assert(FTerminal->Active);

    IconsType = pitFromRegistry;

    BOOL Result;

    try
    {
        if (ForceRefresh)
        {
            if (FPreloaded)
            {
                FPreloaded = false;
                FTerminal->RefreshDirectory();
            }
            else
            {
                FTerminal->ReloadDirectory();
            }
        }

        PluginData = new CPluginFSDataInterface(this);

        Dir->SetFlags(SALDIRFLAG_CASESENSITIVE | SALDIRFLAG_IGNOREDUPDIRS);
        Dir->SetValidData(VALID_DATA_EXTENSION | VALID_DATA_SIZE | VALID_DATA_TYPE |
                          VALID_DATA_DATE | VALID_DATA_TIME | VALID_DATA_ATTRIBUTES | VALID_DATA_HIDDEN |
                          VALID_DATA_ISLINK | VALID_DATA_ISOFFLINE);

        int SortByExtDirsAsFiles;
        SalamanderGeneral()->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES,
                                                &SortByExtDirsAsFiles, sizeof(SortByExtDirsAsFiles), NULL);

        CFileData FileData;

        TRemoteFile* File;

        Result = TRUE;

        int Index = 0;
        while (Index < FTerminal->Files->Count)
        {
            File = FTerminal->Files->Files[Index];

            FileData.Name = SalamanderGeneral()->DupStr(File->FileName.c_str());
            FileData.NameLen = File->FileName.Length();
            if (FileData.Name != NULL)
            {
                if (File->IsParentDirectory ||
                    (File->IsDirectory && !SortByExtDirsAsFiles))
                {
                    FileData.Ext = FileData.Name + FileData.NameLen; // adresare nemaji pripony
                }
                else
                {
                    char* s = strrchr(FileData.Name, '.');
                    if (s != NULL)
                        FileData.Ext = s + 1; // ".cvspass" ve Windows je pripona
                    else
                        FileData.Ext = FileData.Name + FileData.NameLen;
                }
            }
            FileData.Size = CQuadWord(
                static_cast<unsigned long>(File->Size & 0xFFFFFFFF),
                static_cast<unsigned long>(File->Size >> 32));
            FileData.LastWrite = DateTimeToFileTime(File->Modification, dstmUnix);

            FileData.Attr =
                (File->IsDirectory ? FILE_ATTRIBUTE_DIRECTORY : 0) |
                (File->IsHidden ? FILE_ATTRIBUTE_HIDDEN : 0) |
                (File->Rights->ReadOnly ? FILE_ATTRIBUTE_READONLY : 0);
            FileData.Hidden = File->IsHidden;

            FileData.PluginData = reinterpret_cast<unsigned long>(File);

            FileData.IsLink = File->IsSymLink;
            FileData.IsOffline = 0;

            if ((File->IsDirectory && !Dir->AddDir(NULL, FileData, PluginData)) ||
                (!File->IsDirectory && !Dir->AddFile(NULL, FileData, PluginData)))
            {
                SalamanderGeneral()->Free(FileData.Name);
                Result = FALSE;
            }

            Index++;
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        Result = FALSE;
    }

    if (!Result)
    {
        Dir->Clear(PluginData);
        delete PluginData;
        PluginData = NULL;
    }

    FLastDataInterface = static_cast<CPluginFSDataInterface*>(PluginData);

    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::TryCloseOrDetach(BOOL ForceClose, BOOL CanDetach,
                                                 BOOL& Detach, int Reason)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::TryCloseOrDetach(%p, %p, %d, %d, %d)",
                        this, FTerminal, ForceClose, CanDetach, Reason);

    if (FClosing)
    {
        // dovolime pluginu se zavrit, napr. pokud spadlo spojeni
        return TRUE;
    }

    bool Result = true;

    try
    {
        SetParentWindow(SalamanderGeneral()->GetMsgBoxParent());

        bool DetachPossible = (!ForceClose && !FForceClose && CanDetach &&
                               (Reason == FSTRYCLOSE_CHANGEPATH));
        Detach = DetachPossible;

        if (DetachPossible && SalamandConfiguration->ConfirmDetach)
        {
            TMessageParams Params(mpNeverAskAgainCheck);
            TQueryButtonAlias Aliases[2];
            Aliases[0].Button = qaYes;
            Aliases[0].Alias = LoadStr(SAL_DETACH_DISCONNECT);
            Aliases[1].Button = qaNo;
            Aliases[1].Alias = LoadStr(SAL_DETACH_KEEP);
            Params.Aliases = Aliases;
            Params.AliasesCount = LENOF(Aliases);
            Params.NewerAskAgainTitle = LoadStr(SAL_CONFIRM_DETACH_NEVER);
            Params.NewerAskAgainAnswer = qaNo;

            int Answer = MessageDialog(LoadStr(SAL_CONFIRM_DETACH), qtConfirmation,
                                       qaYes | qaNo | qaCancel, HELP_NONE, &Params);

            switch (Answer)
            {
            case qaNeverAskAgain:
                SalamandConfiguration->ConfirmDetach = false;
                break;

            case qaYes:
                Detach = false;
                break;

            case qaCancel:
                Result = false;
                break;
            }
        }

        if (!ForceClose && Result && !Detach)
        {
            assert(FQueue != NULL);
            if (!FQueue->IsEmpty)
            {
                TMessageParams Params;
                TQueryButtonAlias Aliases[2];
                Aliases[0].Button = qaNo;
                Aliases[0].Alias = LoadStr(SAL_DETACH_DISCONNECT);
                if (DetachPossible)
                {
                    Aliases[1].Button = qaYes;
                    Aliases[1].Alias = LoadStr(SAL_DETACH_KEEP);
                }
                Params.Aliases = Aliases;
                Params.AliasesCount = DetachPossible ? 2 : 1;

                int Answers = qaNo | qaCancel | FLAGMASK(DetachPossible, qaYes);
                int Answer = MessageDialog(LoadStr(SAL_PENDING_QUEUE_ITEMS), qtWarning,
                                           Answers, HELP_NONE, &Params);
                switch (Answer)
                {
                case qaYes:
                    Detach = true;
                    Result = true;
                    break;

                case qaCancel:
                    Result = false;
                    break;
                }
            }
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        Result = false;
    }

    FForceClose = Result && !Detach;

    return Result;
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::Event(int Event, DWORD /*Param*/)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::Event(%p, %p, %d)",
                        this, FTerminal, Event);

    if (FClosing)
    {
        return;
    }

    try
    {
        if (Event == FSE_TIMER)
        {
            assert(FTerminal && FTerminal->Active);
            if (FTerminal && FTerminal->Active)
            {
                FTerminal->Idle();
                FQueue->Idle();
                ProcessQueue();
            }
            ScheduleTimer();
        }
        else if (Event == FSE_PATHCHANGED)
        {
            assert((FTerminal != NULL) && FTerminal->Active);
            if ((FTerminal != NULL) && FTerminal->Active)
            {
                const TSessionInfo& SessionInfo = FTerminal->GetSessionInfo();

                bool SecurityEnabled = !SessionInfo.SecurityProtocolName.IsEmpty();
                // has to be in salamander plugin as ftp is not supported here
                assert(SecurityEnabled);
                if (SecurityEnabled)
                {
                    int Panel;
                    if (GetPanel(Panel))
                    {
                        AnsiString ToolTip =
                            FMTLOAD(SAL_SECURE, (SessionInfo.SecurityProtocolName));
                        SalamanderGeneral()->ShowSecurityIcon(Panel, true, true, ToolTip.c_str());
                    }
                }
            }
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::ReleaseObject(HWND Parent)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::ReleaseObject(%p, %p, %x)",
                        this, FTerminal, Parent);

    try
    {
        SetParentWindow(Parent);

        if (!FPendingConnect)
        {
            SaveSession();
            if (FTerminal->Active)
            {
                FForceClose = true;
                FTerminal->Close();
            }
        }
    }
    catch (...)
    {
    }
}
//---------------------------------------------------------------------------
DWORD WINAPI CPluginFSInterface::GetSupportedServices()
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetSupportedServices(%p, %p)",
                        this, FTerminal);

    unsigned int Services =
        FS_SERVICE_GETFSICON | FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM |
        FS_SERVICE_GETNEXTDIRLINEHOTPATH | FS_SERVICE_SHOWINFO |
        FS_SERVICE_ACCEPTSCHANGENOTIF | FS_SERVICE_VIEWFILE |
        FS_SERVICE_CREATEDIR | FS_SERVICE_DELETE |
        FS_SERVICE_COPYFROMFS | FS_SERVICE_MOVEFROMFS |
        FS_SERVICE_COPYFROMDISKTOFS | FS_SERVICE_MOVEFROMDISKTOFS |
        FS_SERVICE_CHANGEATTRS | FS_SERVICE_CONTEXTMENU | FS_SERVICE_COMMANDLINE |
        FS_SERVICE_SHOWSECURITYINFO;

    if (!FClosing && ((FTerminal == NULL) || FTerminal->IsCapable[fcRename]))
    {
        Services |= FS_SERVICE_QUICKRENAME;
    }

    return Services;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetChangeDriveOrDisconnectItem(const char* /*FSName*/,
                                                               char*& Title, HICON& Icon, BOOL& DestroyIcon)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetChangeDriveOrDisconnectItem(%p, %p)",
                        this, FTerminal);

    if (FClosing)
    {
        return FALSE;
    }

    AnsiString Caption;
    Caption = FORMAT("\t%s://%s\t",
                     (FSName(), FTerminal->SessionData->SessionName));
    Caption.SetLength(Caption.Length() * 2);
    SalamanderGeneral()->DuplicateAmpersands(Caption.c_str(), Caption.Length());
    Title = SalamanderGeneral()->DupStr(Caption.c_str());

    BOOL Result = (Title != NULL);
    if (Result)
    {
        Icon = GetFSIcon(DestroyIcon);
    }
    return Result;
}
//---------------------------------------------------------------------------
HICON WINAPI CPluginFSInterface::GetFSIcon(BOOL& DestroyIcon)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetFSIcon(%p, %p)",
                        this, FTerminal);

    HICON Handle = static_cast<HICON>(LoadImage(FPlugin->GetDLLInstance(),
                                                MAKEINTRESOURCE(IDI_WINSCP), IMAGE_ICON, 16, 16, SalamanderGeneral()->GetIconLRFlags()));
    DestroyIcon = TRUE;
    return Handle;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetNextDirectoryLineHotPath(const char* Text,
                                                            int PathLen, int& Offset)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::GetNextDirectoryLineHotPath(%p, %p, %s, %d, %d)",
                        this, FTerminal, Text, PathLen, Offset);

    if (FClosing)
    {
        return FALSE;
    }

    BOOL Result;
    if (Offset == 0)
    {
        AnsiString RootPath = FullFSPath(FullPath("/"));

        Offset = RootPath.Length();

        assert(SameText(AnsiString(Text).SubString(1, Offset), RootPath));

        Result = TRUE;
    }
    else
    {
        const char* Separator = strchr(Text + Offset + 1, '/');
        Result = (Separator != NULL) && (Separator < Text + PathLen);
        if (Result)
        {
            Offset = Separator - Text;
        }
    }
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::GetSpaceAvailable(const AnsiString Path,
                                                      TSpaceAvailable& ASpaceAvailable, bool& Close)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetSpaceAvailable(%p, %p, %s)",
                        this, FTerminal, Path.c_str());

    // terminal can be already closed (e.g. dropped connection)
    if (FClosing)
    {
        Close = true;
        return;
    }

    assert(FTerminal->Active);

    try
    {
        FTerminal->SpaceAvailable(Path, ASpaceAvailable);
    }
    catch (...)
    {
        if (!FTerminal->Active)
        {
            Close = true;
        }
        throw;
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::ShowInfoDialog(const char* /*FSName*/, HWND Parent)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::ShowInfoDialog(%p, %p, %x)",
                        this, FTerminal, Parent);

    if (FClosing)
    {
        return;
    }

    try
    {
        SetParentWindow(Parent);

        FileSystemInfo();
    }
    catch (Exception& E)
    {
        HandleException(&E);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::AcceptChangeOnPathNotification(
    const char* /*FSName*/, const char* APath, BOOL IncludingSubdirs)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::AcceptChangeOnPathNotification(%p, %p, %s, %d)",
                        this, FTerminal, APath, IncludingSubdirs);

    if (FClosing)
    {
        return;
    }

    assert(FTerminal);
    assert(FTerminal->Active);

    AnsiString FullPath(APath);
    if (StripFS(FullPath, false))
    {
        AnsiString Path;

        CPluginInterfaceForFS::ParseUserPart(FullPath, UnixPaths(), NULL, NULL,
                                             NULL, NULL, NULL, &Path);
        // pokud notifikaci poslal tento FS, tak nemazene cache
        // (pro pripad ze byla iniciovana prenosem na pozadi, protoze pak bychom
        // prIsli o nahrany adresar a nedoslo by k refreshi)
        // btw, je to dost nedokonala kontrola, ale zatim staci
        if (!FPreloaded)
        {
            FTerminal->DirectoryModified(Path, IncludingSubdirs);
        }

        if (AffectsCurrentPath(APath, IncludingSubdirs))
        {
            SalamanderGeneral()->PostRefreshPanelFS(this);
        }
    }
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::ExecuteCommandLine(HWND Parent,
                                                   char* Command, int& /*SelFrom*/, int& /*SelTo*/)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::ExecuteCommandLine(%p, %p, %x, %s)",
                        this, FTerminal, Parent, lstrlen(Command) > 350 ? "(too long command)" : Command);

    if (FClosing)
    {
        return FALSE;
    }

    assert(FTerminal);
    assert(FTerminal->Active);

    bool Result = true;

    try
    {
        Result = FTerminal->AllowedAnyCommand(Command) &&
                 EnsureCommandSessionFallback(fcAnyCommand);

        if (Result)
        {
            try
            {
                SetParentWindow(Parent);

                DoConsoleDialog(FTerminal, Command);
            }
            __finally
            {
                CurrentPathChanged(false);
            }
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
    }

    if (Result)
    {
        Command[0] = '\0';
    }

    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::QuickRename(const char* /*FSName*/, int Mode,
                                            HWND Parent, CFileData& File, BOOL IsDir, char* NewName, BOOL& Cancel)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::QuickRename(%p, %p, %d, %x, %d, %s)",
                        this, FTerminal, Mode, Parent, IsDir, NewName);

    if (FClosing)
    {
        Cancel = TRUE;
        return TRUE;
    }

    if (Mode != 2)
    {
        return FALSE;
    }

    Cancel = FALSE;

    try
    {
        SetParentWindow(Parent);

        TRemoteFile* RemoteFile = reinterpret_cast<TRemoteFile*>(File.PluginData);
        assert(RemoteFile);

        char RenameTo[2 * MAX_PATH + 1];
        SalamanderGeneral()->MaskName(RenameTo, 2 * MAX_PATH + 1, File.Name, NewName);
        if (strcmp(File.Name, RenameTo) == 0)
        {
            Cancel = TRUE;
        }
        else
        {
            lstrcpyn(NewName, RenameTo, MAX_PATH);

            try
            {
                FTerminal->RenameFile(RemoteFile, NewName, true);
            }
            __finally
            {
                CurrentPathChanged(IsDir);
            }
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        Cancel = TRUE;
    }
    return TRUE;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::CreateDir(const char* /*FSName*/, int Mode,
                                          HWND Parent, char* NewName, BOOL& Cancel)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::CreateDir(%p, %p, %d, %x, %s)",
                        this, FTerminal, Mode, Parent, NewName);

    if (FClosing)
    {
        Cancel = TRUE;
        return TRUE;
    }

    if (Mode != 2)
    {
        return FALSE;
    }

    Cancel = FALSE;

    try
    {
        SetParentWindow(Parent);

        try
        {
            FTerminal->CreateDirectory(NewName);
        }
        __finally
        {
            CurrentPathChanged(false);
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        Cancel = TRUE;
    }
    return TRUE;
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::ViewFile(const char* /*FSName*/, HWND Parent,
                                         CSalamanderForViewFileOnFSAbstract* Salamander, CFileData& File)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::ViewFile(%p, %p, %x, %p)",
                        this, FTerminal, Parent, Salamander);

    if (FClosing)
    {
        return;
    }

    try
    {
        SetParentWindow(Parent);

        TCopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
        CopyParam.ReplaceInvalidChars = true;
        CopyParam.FileNameCase = ncNoChange;
        CopyParam.PreserveReadOnly = false;
        CopyParam.ResumeSupport = rsOff;
        CopyParam.FileMask = "";
        CopyParam.ExcludeFileMask = TFileMasks();

        AnsiString UniqPath;
        AnsiString LocalFileName = CopyParam.ChangeFileName(File.Name, osRemote, false);
        UniqPath = FullFSPath(UnixIncludeTrailingBackslash(CurrentFullPath()) +
                              LocalFileName);

        CQuadWord FileSize(0, 0);
        BOOL CacheExisted;
        const char* TempFileName = Salamander->AllocFileNameInCache(
            Parent, UniqPath.c_str(), LocalFileName.c_str(), NULL, CacheExisted);

        if (TempFileName)
        {
            bool CacheCreated = false;
            HANDLE FileLock = NULL;
            BOOL FileLockOwner = FALSE;

            try
            {
                if (!CacheExisted)
                {
                    AnsiString TempDirectory(ExtractFilePath(TempFileName));

                    CreateFileList(&File, osRemote);
                    try
                    {
                        FTerminal->CopyToLocal(FFileList, TempDirectory, &CopyParam,
                                               cpTemporary | cpNoConfirmation);
                    }
                    __finally
                    {
                        DestroyFileList();
                    }

                    HANDLE Handle = CreateFile(TempFileName, GENERIC_READ,
                                               FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
                    CacheCreated = (Handle != INVALID_HANDLE_VALUE);
                    if (CacheCreated)
                    {
                        unsigned long Error;
                        SalamanderGeneral()->SalGetFileSize(Handle, FileSize, Error);
                        CloseHandle(Handle);
                    }
                }

                if ((!CacheExisted && !CacheCreated) ||
                    !Salamander->OpenViewer(Parent, TempFileName, &FileLock, &FileLockOwner))
                {
                    FileLock = NULL;
                    FileLockOwner = FALSE;
                }
                SalamanderGeneral()->RemoveOneFileFromCache(UniqPath.c_str());
            }
            __finally
            {
                Salamander->FreeFileNameInCache(UniqPath.c_str(), CacheExisted,
                                                CacheCreated, FileSize, FileLock, FileLockOwner, TRUE);
            }
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
    }
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::Delete(const char* /*FSName*/, int Mode,
                                       HWND Parent, int Panel, int SelectedFiles, int SelectedDirs,
                                       BOOL& CancelOrError)
{
    CALL_STACK_MESSAGE8("CPluginFSInterface::Delete(%p, %p, %d, %x, %d, %d, %d)",
                        this, FTerminal, Mode, Parent, Panel, SelectedFiles, SelectedDirs);

    if (FClosing)
    {
        CancelOrError = TRUE;
        return TRUE;
    }

    if (Mode != 2)
    {
        return FALSE;
    }

    CancelOrError = FALSE;

    try
    {
        SetParentWindow(Parent);

        CreateFileList(Panel, osRemote, SelectedFiles + SelectedDirs == 0);
        try
        {
            FTerminal->DeleteFiles(FFileList);
        }
        __finally
        {
            CurrentPathChanged(SelectedDirs > 0);
            DestroyFileList();
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        CancelOrError = TRUE;
    }
    return TRUE;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::CopyOrMoveFromFS(BOOL Copy, int Mode,
                                                 const char* /*FSName*/, HWND Parent, int Panel, int SelectedFiles,
                                                 int SelectedDirs, char* TargetPath, BOOL& OperationMask,
                                                 BOOL& CancelOrHandlePath, HWND /*DropTarget*/)
{
    CALL_STACK_MESSAGE10("CPluginFSInterface::CopyOrMoveFromFS(%p, %p, %d, %d, %x, %d, %d, %d, %s)",
                         this, FTerminal, Copy, Mode, Parent, Panel, SelectedFiles, SelectedDirs, TargetPath);

    assert(Mode == 1 || Mode == 2 || Mode == 5);

    if (FClosing)
    {
        CancelOrHandlePath = TRUE;
        return TRUE;
    }

    CancelOrHandlePath = FALSE;
    OperationMask = TRUE;
    BOOL Result = TRUE;

    try
    {
        SetParentWindow(Parent);

        AnsiString TargetDirectory = TargetPath;
        bool DoRemoteTransfer = false;
        int PathType;

        if (TargetDirectory.IsEmpty())
        {
            if ((Mode == 1) || (Mode == 2))
            {
                int Panel;
                if (GetPanel(Panel, true))
                {
                    char Buf[MAX_PATH];
                    if (SalamanderGeneral()->GetPanelPath(Panel, Buf, sizeof(Buf), &PathType, NULL))
                    {
                        TargetDirectory = Buf;
                    }
                }
            }
        }
        else
        {
            if (!GetPathType(TargetDirectory, PathType))
            {
                CancelOrHandlePath = TRUE;
            }
        }

        if (!CancelOrHandlePath)
        {
            if (!TargetDirectory.IsEmpty())
            {
                AnsiString UserPart = TargetDirectory;
                if ((PathType == PATH_TYPE_FS) && StripFS(UserPart, false) &&
                    IsOurUserPart(UserPart))
                {
                    if (Mode == 1)
                    {
                        strcpy(TargetPath, TargetDirectory.SubString(1, 2 * MAX_PATH - 1).c_str());
                        Result = FALSE;
                    }
                    else
                    {
                        CPluginInterfaceForFS::ParseUserPart(UserPart, UnixPaths(),
                                                             NULL, NULL, NULL, NULL, NULL, &TargetDirectory);
                        DoRemoteTransfer = true;
                    }
                }
                else if (PathType != PATH_TYPE_WINDOWS)
                {
                    SimpleErrorDialog(FMTLOAD(SAL_PATH_NOT_SUPPORTED, (TargetDirectory)));
                    TargetDirectory = "";
                }
            }

            if (Result)
            {
                CreateFileList(Panel, osRemote, SelectedFiles + SelectedDirs == 0);
                try
                {
                    TGUICopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;

                    // these parameters are known in advance
                    int Params =
                        FLAGMASK(!Copy, cpDelete);

                    if ((Mode == 5) && DoRemoteTransfer)
                    {
                        // for d&d intra-server transfer, do not use copy dialog,
                        // but rather the small one, see RemoteTransfer
                    }
                    // if user has already confirmed FS path in SS standard upload box,
                    // there is no need to show copy dialog anymore
                    else if ((Mode != 2) || !DoRemoteTransfer)
                    {
                        int Options =
                            FLAGMASK(Mode == 2, coDisableDirectory) |
                            coDoNotUsePresets | coAllowRemoteTransfer | coNoQueue | coNoQueueIndividually;
                        int OutputOptions = FLAGMASK(DoRemoteTransfer, cooRemoteTransfer);
                        int UsableCopyParamAttrs = FTerminal->UsableCopyParamAttrs(Params).Download;
                        CancelOrHandlePath = !DoCopyDialog(false, !Copy, FFileList,
                                                           TargetDirectory, &CopyParam, Options, UsableCopyParamAttrs,
                                                           &OutputOptions);

                        if (FLAGSET(OutputOptions, cooRemoteTransfer))
                        {
                            DoRemoteTransfer = true;
                        }
                    }

                    if (!CancelOrHandlePath)
                    {
                        if (DoRemoteTransfer)
                        {
                            // display the dialog for d&d (mode == 5)
                            CancelOrHandlePath = !RemoteTransfer(TargetDirectory,
                                                                 CopyParam.FileMask, SelectedDirs > 0, (Mode != 5), !Copy);
                        }
                        else
                        {
                            // these parameters are known only after transfer dialog
                            Params |=
                                FLAGMASK(CopyParam.NewerOnly, cpNewerOnly);
                            if (CopyParam.Queue)
                            {
                                SalamanderGeneral()->SetUserWorkedOnPanelPath(PANEL_TARGET);

                                // these parameters are known only after transfer dialog
                                Params |=
                                    FLAGMASK(CopyParam.QueueNoConfirmation, cpNoConfirmation);
                                FQueue->AddItem(new TDownloadQueueItem(FTerminal, FFileList,
                                                                       TargetDirectory, &CopyParam, Params));
                            }
                            else
                            {
                                strncpy(TargetPath, TargetDirectory.c_str(), MAX_PATH);

                                try
                                {
                                    FTerminal->CopyToLocal(FFileList, TargetDirectory, &CopyParam, Params);
                                }
                                __finally
                                {
                                    if (!Copy)
                                    {
                                        CurrentPathChanged(SelectedDirs > 0);
                                    }
                                    PathChanged(TargetDirectory, (SelectedDirs > 0));
                                }
                            }
                        }
                    }
                }
                __finally
                {
                    DestroyFileList();
                }
            }
        }
    }
    catch (Exception& E)
    {
        CancelOrHandlePath = !HandleException(&E);
    }

    return Result;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::CopyOrMoveFromDiskToFS(BOOL Copy, int Mode,
                                                       const char* FSName, HWND Parent, const char* SourcePath,
                                                       SalEnumSelection2 Next, void* NextParam, int /*SourceFiles*/,
                                                       int SourceDirs, char* TargetPath, BOOL* InvalidPathOrCancel)
{
    CALL_STACK_MESSAGE10("CPluginFSInterface::CopyOrMoveFromDiskToFS(%p, %p, %d, %d, %s, %x, %s, %d, %s)",
                         this, FTerminal, Copy, Mode, FSName, Parent, SourcePath, SourceDirs, TargetPath);

    assert(Mode == 1 || Mode == 2 || Mode == 3);

    if (FClosing)
    {
        if (InvalidPathOrCancel)
        {
            *InvalidPathOrCancel = TRUE;
        }
        return TRUE;
    }

    bool DisableQueue = false;
    bool Result;
    if (Mode == 1)
    {
        AnsiString ATargetPath = UnixIncludeTrailingBackslash(TargetPath) + "*.*";
        ATargetPath.SetLength(2 * MAX_PATH - 1);
        strcpy(TargetPath, ATargetPath.c_str());
        Result = TRUE;
    }
    else
    {
        SetParentWindow(Parent);

        bool AInvalidPathOrCancel = FALSE;
        try
        {
            AnsiString UserPart(TargetPath);
            StripFS(UserPart, FPendingConnect);

            int FSNameIndex = FPlugin->GetFSNameIndex(FSName);

            if (FPendingConnect)
            {
                Connect(FSNameIndex, UserPart, true);
                assert(IsOurUserPart(UserPart));
                DisableQueue = true;
            }

            assert(Mode == 2 || Mode == 3);
            assert(FTerminal);
            assert(FTerminal->Active);

            Result = IsOurUserPart(UserPart);
            if (Result)
            {
                AnsiString TargetDirectory;
                AnsiString FileMask;

                CPluginInterfaceForFS::ParseUserPart(UserPart, UnixPaths(), NULL, NULL,
                                                     NULL, NULL, NULL, &TargetDirectory);

                TGUICopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;

                if (Mode == 3)
                {
                    FileMask = "";
                }
                else
                {
                    FileMask = UnixExtractFileName(TargetDirectory);
                    TargetDirectory = UnixExtractFilePath(TargetDirectory);
                }
                CopyParam.FileMask = FileMask.IsEmpty() ? AnsiString("*.*") : FileMask;

                if (!TargetDirectory.IsEmpty())
                {
                    TargetDirectory = UnixIncludeTrailingBackslash(TargetDirectory);
                }

                // these parameters are known in advance
                int Params =
                    FLAGMASK(!Copy, cpDelete);

                CreateFileList(SourcePath, Next, NextParam);
                try
                {
                    if (SalamandConfiguration->ConfirmUpload)
                    {
                        int Options =
                            FLAGMASK(DisableQueue, coDisableQueue) |
                            FLAGMASK(Mode != 3, coDisableDirectory) |
                            coDoNotShowAgain | coDoNotUsePresets | coNoQueue | coNoQueueIndividually;
                        int OutputOptions = 0;
                        int UsableCopyParamAttrs = FTerminal->UsableCopyParamAttrs(Params).Upload;
                        AInvalidPathOrCancel = !DoCopyDialog(true, !Copy, FFileList,
                                                             TargetDirectory, &CopyParam, Options, UsableCopyParamAttrs,
                                                             &OutputOptions);
                        if (FLAGSET(OutputOptions, cooDoNotShowAgain))
                        {
                            SalamandConfiguration->ConfirmUpload = false;
                        }
                    }

                    if (!AInvalidPathOrCancel)
                    {
                        // these parameters are known only after transfer dialog
                        Params |=
                            FLAGMASK(CopyParam.NewerOnly, cpNewerOnly);
                        if (CopyParam.Queue && !DisableQueue)
                        {
                            // these parameters are known only after transfer dialog
                            Params |=
                                FLAGMASK(CopyParam.QueueNoConfirmation, cpNoConfirmation);
                            FQueue->AddItem(new TUploadQueueItem(FTerminal, FFileList,
                                                                 TargetDirectory, &CopyParam, Params));
                        }
                        else
                        {
                            try
                            {
                                FTerminal->CopyToRemote(FFileList, TargetDirectory, &CopyParam, Params);
                            }
                            __finally
                            {
                                if (FTerminal && FTerminal->Active)
                                {
                                    OurPathChanged(TargetDirectory, (SourceDirs > 0));
                                    // temporary workaround: core always reloads current path
                                    if (!UnixComparePaths(TargetDirectory, FTerminal->CurrentDirectory))
                                    {
                                        CurrentPathChanged(false);
                                    }
                                    if (!Copy)
                                    {
                                        PathChanged(SourcePath, (SourceDirs > 0));
                                    }
                                }
                            }
                        }
                    }
                }
                __finally
                {
                    DestroyFileList();
                }
            }
        }
        catch (Exception& E)
        {
            AInvalidPathOrCancel = !HandleException(&E);
        }

        if (InvalidPathOrCancel)
        {
            *InvalidPathOrCancel = AInvalidPathOrCancel;
        }
    }
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::CalculateSize(
    TStrings* FileList, __int64& Size, TCalculateSizeStats& Stats,
    bool& Close)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::CalculateSize(%p, %p)",
                        this, FTerminal);

    // terminal can be already closed (e.g. dropped connection)
    if (FClosing)
    {
        Close = true;
        return;
    }

    try
    {
        FTerminal->CalculateFilesSize(FileList, Size, 0, NULL, &Stats);
    }
    catch (...)
    {
        if (!FTerminal->Active)
        {
            Close = true;
        }
        throw;
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSInterface::CalculateChecksum(const AnsiString& Alg,
                                                      TStrings* FileList, TCalculatedChecksumEvent OnCalculatedChecksum,
                                                      bool& Close)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::CalculateChecksum(%p, %p)",
                        this, FTerminal);

    // terminal can be already closed (e.g. dropped connection)
    if (FClosing)
    {
        Close = true;
        return;
    }

    try
    {
        FTerminal->CalculateFilesChecksum(Alg, FileList, NULL, OnCalculatedChecksum);
    }
    catch (...)
    {
        if (!FTerminal->Active)
        {
            Close = true;
        }
        throw;
    }
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::ChangeAttributes(const char* /*FSName*/,
                                                 HWND Parent, int Panel, int SelectedFiles, int SelectedDirs)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::ChangeAttributes(%p, %p, %x, %d, %d, %d)",
                        this, FTerminal, Parent, Panel, SelectedFiles, SelectedDirs);

    if (FClosing)
    {
        return FALSE;
    }

    bool Result = TRUE;

    try
    {
        SetParentWindow(Parent);

        CreateFileList(Panel, osRemote, SelectedFiles + SelectedDirs == 0);
        try
        {
            TRemoteProperties CurrentProperties;

            if (FTerminal->LoadFilesProperties(FFileList))
            {
                RefreshPanel();
            }
            CurrentProperties = TRemoteProperties::CommonProperties(FFileList);

            int Flags = 0;
            if (FTerminal->IsCapable[fcModeChanging])
                Flags |= cpMode;
            if (FTerminal->IsCapable[fcOwnerChanging])
                Flags |= cpOwner;
            if (FTerminal->IsCapable[fcGroupChanging])
                Flags |= cpGroup;

            TCalculateChecksumEvent CalculateChecksumEvent = NULL;
            if (FTerminal->IsCapable[fcCalculatingChecksum])
            {
                CalculateChecksumEvent = CalculateChecksum;
            }

            TRemoteProperties NewProperties = CurrentProperties;
            Result = DoPropertiesDialog(FFileList, FTerminal->CurrentDirectory,
                                        FTerminal->Groups, FTerminal->Users, &NewProperties, Flags,
                                        FTerminal->IsCapable[fcGroupOwnerChangingByID],
                                        CalculateSize, CalculateChecksumEvent);
            if (Result)
            {
                if (FTerminal->Active)
                {
                    NewProperties = TRemoteProperties::ChangedProperties(CurrentProperties,
                                                                         NewProperties);
                    try
                    {
                        FTerminal->ChangeFilesProperties(FFileList, &NewProperties);
                    }
                    __finally
                    {
                        CurrentPathChanged(SelectedDirs > 0);
                    }
                }
            }
        }
        __finally
        {
            DestroyFileList();
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
        Result = FALSE;
    }
    return Result;
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::ContextMenu(const char* /*FSName*/, HWND Parent,
                                            int MenuX, int MenuY, int Type, int Panel, int /*SelectedFiles*/,
                                            int /*SelectedDirs*/)
{
    CALL_STACK_MESSAGE8("CPluginFSInterface::ContextMenu(%p, %p, %x, %d, %d, %d, %d)",
                        this, FTerminal, Parent, MenuX, MenuY, Type, Panel);

    if (FClosing || ((Type != fscmPathInPanel) && (Type != fscmItemsInPanel)))
    {
        return;
    }

    try
    {
        if (Type == fscmPathInPanel)
        {
            int Cmd = CPluginInterfaceForFS::SessionMenu(Parent, MenuX, MenuY, false);
            if (Cmd != 0)
            {
                Cmd--;
                if (Cmd == fsmClose)
                {
                    assert(Panel == PANEL_LEFT || Panel == PANEL_RIGHT);
                    DisconnectFromPanel(Panel);
                }
                else if (Cmd == fsmFullSynchronize)
                {
                    FullSynchronize(SalamanderGeneral()->GetSourcePanel() == Panel);
                }
                else if (Cmd == fsmSynchronize)
                {
                    Synchronize();
                }
            }
        }
        else if (Type == fscmItemsInPanel)
        {
            FileMenu(Parent, MenuX, MenuY);
        }
    }
    catch (Exception& E)
    {
        HandleException(&E);
    }
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::HandleMenuMsg(UINT /*Msg*/, WPARAM /*WParam*/,
                                              LPARAM /*LParam*/, LRESULT* /*plResult*/)
{
    return FALSE;
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::OpenFindDialog(const char* /*FSName*/, int /*Panel*/)
{
    assert(false);
    return false;
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::OpenActiveFolder(const char* /*FSName*/,
                                                 HWND /*Parent*/)
{
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::GetDropEffect(const char* SrcFSPath,
                                              const char* TgtFSPath, DWORD /*AllowedEffects*/, DWORD /*KeyState*/,
                                              DWORD* DropEffect)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::GetDropEffect(%p, %p, %s, %s, %p)",
                        this, FTerminal, SrcFSPath, TgtFSPath, DropEffect);

    AnsiString UserPart = SrcFSPath;
    if (FLAGSET(*DropEffect, DROPEFFECT_MOVE | DROPEFFECT_COPY) &&
        // (strcmp(SrcFSPath, TgtFSPath) != 0) &&  // Petr: zakomentovano, aby pri tazeni do vedlejsiho panelu na stejnou cestu byl effect Move (jinak byl Copy, coz nebylo OK)
        StripFS(UserPart, false) && IsOurUserPart(UserPart))
    {
        *DropEffect = DROPEFFECT_MOVE;
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::GetAllowedDropEffects(int Mode,
                                                      const char* TgtFSPath, DWORD* AllowedEffects)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::GetAllowedDropEffects(%p, %p, %d, %s, %p)",
                        this, FTerminal, Mode, TgtFSPath, AllowedEffects);

    if ((Mode == 1) && (AllowedEffects != NULL))
    {
        AnsiString UserPart = TgtFSPath;
        if (StripFS(UserPart, false) && IsOurUserPart(UserPart))
        {
            *AllowedEffects &= (DROPEFFECT_MOVE | DROPEFFECT_COPY);
        }
        else
        {
            *AllowedEffects = 0;
        }
    }
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginFSInterface::GetNoItemsInPanelText(char* /*TextBuf*/,
                                                      int /*TextBufSize*/)
{
    return FALSE;
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSInterface::ShowSecurityInfo(HWND Parent)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::ShowSecurityInfo(%p, %p, %x)",
                        this, FTerminal, Parent);

    if (FClosing)
    {
        return;
    }

    try
    {
        SetParentWindow(Parent);

        FileSystemInfo();
    }
    catch (Exception& E)
    {
        HandleException(&E);
    }
}
//---------------------------------------------------------------------------
CPluginFSDataInterface::CPluginFSDataInterface(CPluginFSInterface* AFileSystem)
{
    CALL_STACK_MESSAGE3("CPluginFSDataInterface::CPluginFSDataInterface(%p, %p, %p)",
                        this, AFileSystem);

    FFileSystem = AFileSystem;
    assert(FFileSystem);
    FInvalid = false;
}
//---------------------------------------------------------------------------
CPluginFSDataInterface::~CPluginFSDataInterface()
{
    CALL_STACK_MESSAGE3("CPluginFSDataInterface::~CPluginFSDataInterface(%p, %p)",
                        this, FFileSystem);

    if (!FInvalid)
    {
        assert(FFileSystem);
        FFileSystem->DataInterfaceDestroyed(this);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginFSDataInterface::Invalidate()
{
    CALL_STACK_MESSAGE3("CPluginFSDataInterface::Invalidate(%p, %p)",
                        this, FFileSystem);

    FInvalid = true;
    FFileSystem = NULL;
}
//---------------------------------------------------------------------------
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;
//---------------------------------------------------------------------------
void WINAPI GetColumnText()
{
    assert(dynamic_cast<CPluginFSDataInterface*>(*TransferPluginDataIface) != NULL);

    if (((CPluginFSDataInterface*)(*TransferPluginDataIface))->IsInvalid())
    {
        *TransferLen = 0;
        return;
    }

    TRemoteFile* RemoteFile;
    RemoteFile = reinterpret_cast<TRemoteFile*>((*TransferFileData)->PluginData);
    assert(RemoteFile);
    AnsiString Value;
    switch (*TransferActCustomData)
    {
    case pcRights:
        Value = RemoteFile->RightsStr;
        break;

    case pcOwner:
        Value = RemoteFile->Owner.DisplayText;
        break;

    case pcGroup:
        Value = RemoteFile->Group.DisplayText;
        break;

    case pcLinkTo:
        Value = RemoteFile->LinkTo;
        break;

    default:
        assert(false);
    }
    *TransferLen = Value.Length();
    strcpy(TransferBuffer, Value.c_str());
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSDataInterface::SetupView(BOOL LeftPanel,
                                              CSalamanderViewAbstract* View, const char* /*ArchivePath*/,
                                              const CFileData* /*UpperDir*/)
{
    CALL_STACK_MESSAGE4("CPluginFSDataInterface::SetupView(%p, %p, %p)",
                        this, FFileSystem, View);

    View->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer,
                               TransferLen, TransferRowData, TransferPluginDataIface, TransferActCustomData);

    if (View->GetViewMode() == VIEW_MODE_DETAILED)
    {
        for (int i = 0; i < View->GetColumnsCount(); i++)
        {
            const CColumn* Column = View->GetColumn(i);
            if (Column->ID == COLUMN_ID_ATTRIBUTES)
            {
                View->DeleteColumn(i);
            }
        }

        CColumn NewColumn;
        NewColumn.GetText = GetColumnText;
        NewColumn.SupportSorting = 0;
        NewColumn.LeftAlignment = 1;
        NewColumn.ID = COLUMN_ID_CUSTOM;

        void* Iterator = NULL;
        int Column;
        bool Show;
        while (SalamandConfiguration->IteratePanelColumns(Column, Show, Iterator))
        {
            if (Show)
            {
                strcpy(NewColumn.Name, LoadStr(ColumnNames[Column - 1]).c_str());
                strcpy(NewColumn.Description, LoadStr(ColumnDescs[Column - 1]).c_str());
                NewColumn.Width = LeftPanel ? SalamandConfiguration->LeftColumnWidth[Column] : SalamandConfiguration->RightColumnWidth[Column];
                NewColumn.FixedWidth = LeftPanel ? SalamandConfiguration->LeftColumnFixedWidth[Column] : SalamandConfiguration->RightColumnFixedWidth[Column];
                NewColumn.CustomData = Column;
                View->InsertColumn(View->GetColumnsCount(), &NewColumn);
            }
        }
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSDataInterface::ColumnFixedWidthShouldChange(BOOL LeftPanel,
                                                                 const CColumn* Column, int NewFixedWidth)
{
    if ((Column->CustomData > 0) && (Column->CustomData <= pcLast))
    {
        if (LeftPanel)
        {
            SalamandConfiguration->LeftColumnFixedWidth[Column->CustomData] = NewFixedWidth;
        }
        else
        {
            SalamandConfiguration->RightColumnFixedWidth[Column->CustomData] = NewFixedWidth;
        }
    }

    if (NewFixedWidth)
    {
        ColumnWidthWasChanged(LeftPanel, Column, Column->Width);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginFSDataInterface::ColumnWidthWasChanged(BOOL LeftPanel,
                                                          const CColumn* Column, int NewWidth)
{
    if ((Column->CustomData > 0) && (Column->CustomData <= pcLast))
    {
        if (LeftPanel)
        {
            SalamandConfiguration->LeftColumnWidth[Column->CustomData] = NewWidth;
        }
        else
        {
            SalamandConfiguration->RightColumnWidth[Column->CustomData] = NewWidth;
        }
    }
}
