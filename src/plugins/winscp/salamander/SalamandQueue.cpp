// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include "Common.h"
#include "Exceptions.h"
#include "Queue.h"
#include "SalamandQueue.h"
#include "Salamander.rh"
#include "GUITools.h"
#include "WinInterface.h"
#include "TextsWin.h"
#include "Progress.h"

#include <Consts.hpp>
#include <Common.h>
#include <CoreMain.h>
#include <GUIConfiguration.h>
#include "winliblt.h"
#include "spl_gui.h"

#define WM_WINSCP_CLOSE_DLG (WM_APP + 3300)
#define WM_WINSCP_UPDATE_DLG (WM_APP + 3301)
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
class TPasswordDialog : public CDialog,
                        protected CSalamanderGeneralLocal
{
public:
    TPasswordDialog(HWND Parent, CPluginInterface* APlugin, AnsiString Name,
                    AnsiString Prompt, TPromptKind Kind);

    bool Execute(AnsiString& Password);

protected:
    virtual BOOL DialogProc(UINT Msg, WPARAM WParam, LPARAM LParam);

private:
    AnsiString FPassword;
    AnsiString FName;
    AnsiString FPrompt;
    TPromptKind FKind;
};
//---------------------------------------------------------------------------
TPasswordDialog::TPasswordDialog(HWND Parent, CPluginInterface* APlugin,
                                 AnsiString Name, AnsiString Prompt, TPromptKind Kind) : CDialog(HLanguage, IDD_PASSWORD, Parent),
                                                                                         CSalamanderGeneralLocal(APlugin),
                                                                                         FName(Name), FPrompt(Prompt), FKind(Kind)
{
    CALL_STACK_MESSAGE7("TPasswordDialog::TPasswordDialog(%p, %x, %p, %s, %s, %d)",
                        this, Parent, APlugin, Name.c_str(), Prompt.c_str(), Kind);
}
//---------------------------------------------------------------------------
bool TPasswordDialog::Execute(AnsiString& Password)
{
    CALL_STACK_MESSAGE2("TPasswordDialog::Execute(%p)", this);

    bool Result = (CDialog::Execute() == IDOK);
    if (Result)
    {
        Password = FPassword.c_str();
    }
    return Result;
}
//---------------------------------------------------------------------------
BOOL TPasswordDialog::DialogProc(UINT Msg, WPARAM WParam, LPARAM LParam)
{
    CALL_STACK_MESSAGE5("TPasswordDialog::DialogProc(%p, %d, %d, %d)",
                        this, Msg, WParam, LParam);

    switch (Msg)
    {
    case WM_INITDIALOG:
        SalamanderGeneral()->MultiMonCenterWindow(HWindow, Parent, TRUE);
        SetDlgItemText(HWindow, IDC_PASSWORD_LABEL, FPrompt.c_str());
        SendMessage(HWindow, WM_SETTEXT, 0, reinterpret_cast<long>(FName.c_str()));
        break;

    case WM_COMMAND:
        switch (LOWORD(WParam))
        {
        case IDOK:
        {
            FPassword.SetLength(
                SendDlgItemMessage(HWindow, IDC_PASSWORD_EDIT, WM_GETTEXTLENGTH, 0, 0) + 1);
            SendDlgItemMessage(HWindow, IDC_PASSWORD_EDIT, WM_GETTEXT,
                               FPassword.Length(), reinterpret_cast<long>(FPassword.c_str()));
        }
        }
    }

    return CDialog::DialogProc(Msg, WParam, LParam);
}
//---------------------------------------------------------------------------
class TSalamandProgressForm : public CDialog,
                              protected CSalamanderGeneralLocal
{
public:
    TSalamandProgressForm(HWND Parent, CPluginInterface* APlugin, bool AlwaysOnTop,
                          TSalamandProgressThread* Thread, TQueueItemProxy* QueueItem);

    void ScheduleUpdate();
    void Update();
    void Close();
    void Closing();

protected:
    HWND FParent;
    bool FAlwaysOnTop;
    TQueueItemProxy* FQueueItem;
    TSalamandProgressThread* FThread;
    bool FUpdateScheduled;
    bool FPendingSchedule;
    bool FLastTotalSizeKnown;
    bool FConfigCalculateSize;
    bool FClosing;
    AnsiString FLastCaption;
    TDateTime FLastUpdate;
    CGUIStaticTextAbstract* TargetPathLabel;
    CGUIStaticTextAbstract* FileLabel;
    CGUIProgressBarAbstract* OperationProgress;
    CGUIProgressBarAbstract* FileProgress;
    CGUIStaticTextAbstract* TimeLabel;
    CGUIStaticTextAbstract* BytesTransferedLabel;
    CGUIStaticTextAbstract* TimeElapsedLabel;
    CGUIStaticTextAbstract* CPSLabel;

    virtual BOOL DialogProc(UINT Msg, WPARAM WParam, LPARAM LParam);

    bool InitDialog();
    void UpdateControls(bool Init);
    void CloseQuery();
};
//---------------------------------------------------------------------------
class TSalamandProgressThread : public TSimpleThread
{
public:
    TSalamandProgressThread(HWND Parent, CPluginInterface* APlugin,
                            TSalamandQueueController* Controller, TQueueItemProxy* QueueItem);

    virtual void __fastcall Start();
    virtual void __fastcall Execute();
    virtual void __fastcall Terminate();
    void ScheduleUpdate();
    void Update();
    void UpdateMe();
    HWND GetHWindow();
    void FormRunning();
    void Closing();

private:
    TQueueItemProxy* FQueueItem;
    HWND FParent;
    CPluginInterface* FPlugin;
    TSalamandProgressForm* FForm;
    bool FAlwaysOnTop;
    TSalamandQueueController* FController;
    bool FPendingUpdate;
    HANDLE FRunningEvent;
};
//---------------------------------------------------------------------------
TSalamandProgressForm::TSalamandProgressForm(HWND Parent,
                                             CPluginInterface* APlugin, bool AlwaysOnTop, TSalamandProgressThread* Thread,
                                             TQueueItemProxy* QueueItem) : CDialog(HLanguage, IDD_PROGRESS, NULL),
                                                                           CSalamanderGeneralLocal(APlugin),
                                                                           FParent(Parent), FAlwaysOnTop(AlwaysOnTop), FThread(Thread), FQueueItem(QueueItem),
                                                                           FUpdateScheduled(false), FLastTotalSizeKnown(false), FClosing(false)
{
    CALL_STACK_MESSAGE7("TSalamandProgressForm::TSalamandProgressForm(%p, %x, %p, %d, %p, %p)",
                        this, Parent, APlugin, AlwaysOnTop, Thread, QueueItem);

    assert(GUIConfiguration != NULL);
    FConfigCalculateSize = GUIConfiguration->DefaultCopyParam.CalculateSize;
}
//---------------------------------------------------------------------------
BOOL TSalamandProgressForm::DialogProc(UINT Msg, WPARAM WParam, LPARAM LParam)
{
    CALL_STACK_MESSAGE5("TSalamandProgressForm::DialogProc(%p, %d, %d, %d)",
                        this, Msg, WParam, LParam);

    switch (Msg)
    {
    case WM_INITDIALOG:
        if (!InitDialog())
        {
            DestroyWindow(HWindow);
            return FALSE;
        }
        FThread->FormRunning();
        break;

    case WM_COMMAND:
        if (WParam == IDCANCEL)
        {
            CloseQuery();
            return TRUE;
        }
        break;

    case WM_WINSCP_CLOSE_DLG:
        EndDialog(HWindow, 0);
        return TRUE;

    case WM_WINSCP_UPDATE_DLG:
        assert(FUpdateScheduled);
        // ask for update only if we have not been asked to close already
        if (!FClosing)
        {
            FThread->UpdateMe();
        }
        return TRUE;
    }

    return CDialog::DialogProc(Msg, WParam, LParam);
}
//---------------------------------------------------------------------------
void TSalamandProgressForm::CloseQuery()
{
    CALL_STACK_MESSAGE2("TSalamandProgressForm::CloseQuery(%p)", this);

    if (SalamanderMessageDialog(HWindow, LoadStr(CANCEL_OPERATION), NULL,
                                qtConfirmation, qaOK | qaCancel) == qaOK)
    {
        FQueueItem->Delete();
    }
}
//---------------------------------------------------------------------------
bool TSalamandProgressForm::InitDialog()
{
    CALL_STACK_MESSAGE2("TSalamandProgressForm::InitDialog(%p)", this);

    SendMessage(HWindow, WM_SETICON, ICON_BIG,
                (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_WINSCP)));
    SalamanderGeneral()->MultiMonCenterWindow(HWindow, FParent, TRUE);
    if (FAlwaysOnTop)
    {
        // always-on-top osetrime aspon "staticky" (neni v system menu)
        SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    bool Result =
        ((FileLabel = SalamanderGUI()->AttachStaticText(
              HWindow, IDC_PROGRESS_FILE_LABEL, STF_PATH_ELLIPSIS)) != NULL) &&
        ((TargetPathLabel = SalamanderGUI()->AttachStaticText(
              HWindow, IDC_PROGRESS_TARGET_PATH_LABEL, STF_PATH_ELLIPSIS)) != NULL) &&
        ((OperationProgress = SalamanderGUI()->AttachProgressBar(
              HWindow, IDC_PROGRESS_OPERATION_PROGRESS)) != NULL) &&
        ((FileProgress = SalamanderGUI()->AttachProgressBar(
              HWindow, IDC_PROGRESS_FILE_PROGRESS)) != NULL) &&
        ((TimeLabel = SalamanderGUI()->AttachStaticText(
              HWindow, IDC_PROGRESS_TIME_LABEL, STF_CACHED_PAINT)) != NULL) &&
        ((BytesTransferedLabel = SalamanderGUI()->AttachStaticText(
              HWindow, IDC_PROGRESS_BYTES_TRANSFERRED_LABEL, STF_CACHED_PAINT)) != NULL) &&
        ((TimeElapsedLabel = SalamanderGUI()->AttachStaticText(
              HWindow, IDC_PROGRESS_TIME_ELAPSED_LABEL, STF_CACHED_PAINT)) != NULL) &&
        ((CPSLabel = SalamanderGUI()->AttachStaticText(
              HWindow, IDC_PROGRESS_CPS_LABEL, STF_CACHED_PAINT)) != NULL) &&
        true;

    if (Result)
    {
        UpdateControls(true);

        SetForegroundWindow(HWindow);

        SetDlgItemText(HWindow, IDC_PROGRESS_FILE_LABEL_LABEL, SalLoadStr(SAL_PROGRESS_FILE_LABEL));
        SetDlgItemText(HWindow, IDC_PROGRESS_TARGET_PATH_LABEL_LABEL, SalLoadStr(SAL_PROGRESS_TARGET_PATH_LABEL));
        SetDlgItemText(HWindow, IDC_PROGRESS_BYTES_TRANSFERRED_LABEL_LABEL, SalLoadStr(SAL_PROGRESS_BYTES_TRANSFERRED_LABEL));
        SetDlgItemText(HWindow, IDC_PROGRESS_TIME_ELAPSED_LABEL_LABEL, SalLoadStr(SAL_PROGRESS_TIME_ELAPSED_LABEL));
        SetDlgItemText(HWindow, IDC_PROGRESS_CPS_LABEL_LABEL, SalLoadStr(SAL_PROGRESS_SPEED_LABEL));
        SetDlgItemText(HWindow, IDCANCEL, LoadResourceString(&_SMsgDlgCancel).c_str());
    }

    return Result;
}
//---------------------------------------------------------------------------
void TSalamandProgressForm::UpdateControls(bool Init)
{
    CALL_STACK_MESSAGE3("TSalamandProgressForm::UpdateControls(%p, %d)", this, Init);

    assert(FQueueItem != NULL);
    TQueueItem::TInfo* Info = FQueueItem->Info;
    assert(Info != NULL);
    TFileOperationProgressType* ProgressData = FQueueItem->ProgressData;

    bool TotalSizeKnown =
        ((ProgressData != NULL) && (ProgressData->Operation == Info->Operation)) ? ProgressData->TotalSizeSet : FConfigCalculateSize;

    if (Init || (FLastTotalSizeKnown != TotalSizeKnown))
    {
        FLastTotalSizeKnown = TotalSizeKnown;

        SetDlgItemText(HWindow, IDC_PROGRESS_TIME_LABEL_LABEL,
                       SalLoadStr(TotalSizeKnown ? SAL_PROGRESS_TIME_LEFT_LABEL : SAL_PROGRESS_START_TIME_LABEL));
    }

    if (Init)
    {
        AnsiString TargetPath;
        if (Info->Side == osLocal)
        {
            TargetPath = UnixExtractFileDir(Info->Destination);
        }
        else
        {
            TargetPath = ExtractFileDir(Info->Destination);
        }
        TargetPathLabel->SetText(TargetPath.c_str());
    }

    AnsiString Caption;
    bool Clear = true;

    if (ProgressData == NULL)
    {
        FileLabel->SetText("");
        if ((FQueueItem->Status == TQueueItem::qsPending) ||
            (FQueueItem->Status == TQueueItem::qsConnecting) ||
            TQueueItem::IsUserActionStatus(FQueueItem->Status))
        {
            Caption = LoadStr(QUEUE_CONNECTING);
        }
    }
    else
    {
        FileLabel->SetText(ProgressData->FileName.c_str());

        if (ProgressData->Operation == Info->Operation)
        {
            int OverallProgress = ProgressData->OverallProgress();
            Caption = FORMAT("%d%% %s", (OverallProgress,
                                         TProgressForm::OperationName(Info->Operation)));

            OperationProgress->SetProgress(OverallProgress * 10, "");
            FileProgress->SetProgress(ProgressData->TransferProgress() * 10, "");

            if (TotalSizeKnown)
            {
                TimeLabel->SetText(FormatDateTimeSpan(Configuration->TimeFormat.c_str(),
                                                      ProgressData->TotalTimeLeft())
                                       .c_str());
            }
            else
            {
                TimeLabel->SetText(ProgressData->StartTime.TimeString().c_str());
            }
            TimeElapsedLabel->SetText(FormatDateTimeSpan(Configuration->TimeFormat.c_str(),
                                                         ProgressData->TimeElapsed())
                                          .c_str());
            BytesTransferedLabel->SetText(FormatBytes(ProgressData->TotalTransfered).c_str());
            CPSLabel->SetText(FORMAT("%s/s", (FormatBytes(ProgressData->CPS()))).c_str());

            Clear = false;
        }
        else if (ProgressData->Operation == foCalculateSize)
        {
            Caption = LoadStr(QUEUE_CALCULATING_SIZE);
        }
    }

    if (Clear)
    {
        OperationProgress->SetProgress(0, "");
        FileProgress->SetProgress(0, "");

        TimeLabel->SetText("");
        TimeElapsedLabel->SetText("");
        BytesTransferedLabel->SetText("");
        CPSLabel->SetText("");
    }

    if (Caption.IsEmpty())
    {
        Caption = TProgressForm::OperationName(Info->Operation);
    }

    if (Init || (FLastCaption != Caption))
    {
        FLastCaption = Caption;
        SendMessage(HWindow, WM_SETTEXT, 0, reinterpret_cast<long>(Caption.c_str()));
    }

    FLastUpdate = Now();
}
//---------------------------------------------------------------------------
void TSalamandProgressForm::Close()
{
    CALL_STACK_MESSAGE2("TSalamandProgressForm::Close(%p)", this);

    FQueueItem = NULL;
    PostMessage(HWindow, WM_WINSCP_CLOSE_DLG, 0, 0);
}
//---------------------------------------------------------------------------
void TSalamandProgressForm::Closing()
{
    CALL_STACK_MESSAGE2("TSalamandProgressForm::Closing(%p)", this);

    FClosing = true;

    // mind that this can be called more than once!

    if (!IsWindowEnabled(HWindow))
    {
        // close any dialog (password prompt, message box...)
        HWND Dialog = GetLastActivePopup(HWindow);
        if ((Dialog != NULL) && (Dialog != HWindow))
        {
            PostMessage(Dialog, WM_CLOSE, 0, 0);
        }
    }
}
//---------------------------------------------------------------------------
void TSalamandProgressForm::Update()
{
    CALL_STACK_MESSAGE2("TSalamandProgressForm::Update(%p)", this);

    // if FClosing is true, we are just required to exit the UpdateMe callback
    // to controller ASAP so that we can finally close
    if (!FClosing)
    {
        // at this point, we are called from the form's thread and guarded from
        // being disposed

        FQueueItem->Update();

        static double UpdateInterval = double(1) / (24 * 60 * 60 * 10); // 100ms
        if (double(Now()) - double(FLastUpdate) > UpdateInterval)
        {
            UpdateControls(false);
        }

        if (FQueueItem != NULL)
        {
            if (TQueueItem::IsUserActionStatus(FQueueItem->Status))
            {
                FQueueItem->ProcessUserAction(FThread);
            }
        }

        FUpdateScheduled = false;
        if (FPendingSchedule)
        {
            ScheduleUpdate();
        }
    }
}
//---------------------------------------------------------------------------
void TSalamandProgressForm::ScheduleUpdate()
{
    CALL_STACK_MESSAGE2("TSalamandProgressForm::ScheduleUpdate(%p)", this);

    if (FUpdateScheduled)
    {
        FPendingSchedule = true;
    }
    else
    {
        FPendingSchedule = false;
        FUpdateScheduled = true;
        PostMessage(HWindow, WM_WINSCP_UPDATE_DLG, 0, 0);
    }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
TSalamandProgressThread::TSalamandProgressThread(HWND Parent,
                                                 CPluginInterface* APlugin, TSalamandQueueController* Controller,
                                                 TQueueItemProxy* QueueItem) : FParent(Parent), FPlugin(APlugin), FQueueItem(QueueItem), FForm(NULL),
                                                                               FController(Controller), FRunningEvent(NULL)
{
    CALL_STACK_MESSAGE6("TSalamandProgressThread::TSalamandProgressThread(%p, %x, %p, %p, %p)",
                        this, Parent, APlugin, Controller, QueueItem);

    FAlwaysOnTop = FPlugin->GetSalamanderGeneral()->GetConfigParameter(
        SALCFG_ALWAYSONTOP, &FAlwaysOnTop, sizeof(FAlwaysOnTop), NULL);
}
//---------------------------------------------------------------------------
void __fastcall TSalamandProgressThread::Start()
{
    CALL_STACK_MESSAGE2("TSalamandProgressThread::Start(%p)", this);

    assert(FRunningEvent == NULL);
    FRunningEvent = CreateEvent(NULL, false, false, NULL);
    TSimpleThread::Start();
    HANDLE Objects[2];
    Objects[0] = FRunningEvent;
    Objects[1] = FThread;
    // do not let the calling thread continue until the progress form popups
    // (so we have handle to it sooner then the first call to ScheduleUpdate() comes)
    WaitForMultipleObjects(LENOF(Objects), Objects, false, INFINITE);
    CloseHandle(FRunningEvent);
    FRunningEvent = NULL;
}
//---------------------------------------------------------------------------
void TSalamandProgressThread::FormRunning()
{
    CALL_STACK_MESSAGE2("TSalamandProgressThread::FormRunning(%p)", this);

    assert(FRunningEvent != NULL);
    SetEvent(FRunningEvent);
}
//---------------------------------------------------------------------------
void __fastcall TSalamandProgressThread::Execute()
{
    CALL_STACK_MESSAGE2("TSalamandProgressThread::Execute(%p)", this);

    FForm = new TSalamandProgressForm(FParent, FPlugin, FAlwaysOnTop, this, FQueueItem);
    try
    {
        FForm->Execute();
    }
    __finally
    {
        delete FForm;
        FForm = NULL;
    }
}
//---------------------------------------------------------------------------
void TSalamandProgressThread::ScheduleUpdate()
{
    CALL_STACK_MESSAGE3("TSalamandProgressThread::ScheduleUpdate(%p, %p)", this, FForm);

    FForm->ScheduleUpdate();
}
//---------------------------------------------------------------------------
void TSalamandProgressThread::Update()
{
    CALL_STACK_MESSAGE3("TSalamandProgressThread::Update(%p, %p)", this, FForm);

    assert(FForm != NULL);
    FForm->Update();
}
//---------------------------------------------------------------------------
void TSalamandProgressThread::UpdateMe()
{
    CALL_STACK_MESSAGE3("TSalamandProgressThread::UpdateMe(%p, %p)", this, FForm);

    assert(FForm != NULL);
    FController->UpdateMe(this);
}
//---------------------------------------------------------------------------
HWND TSalamandProgressThread::GetHWindow()
{
    CALL_STACK_MESSAGE3("TSalamandProgressThread::GetHWindow(%p, %p)", this, FForm);

    assert(FForm != NULL);
    return FForm->HWindow;
}
//---------------------------------------------------------------------------
void __fastcall TSalamandProgressThread::Terminate()
{
    CALL_STACK_MESSAGE3("TSalamandProgressThread::Terminate(%p, %p)", this, FForm);

    assert(FForm != NULL);
    FForm->Close();
}
//---------------------------------------------------------------------------
void TSalamandProgressThread::Closing()
{
    CALL_STACK_MESSAGE3("TSalamandProgressThread::Closing(%p, %p)", this, FForm);

    assert(FForm != NULL);
    FForm->Closing();
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
TSalamandQueueController::TSalamandQueueController(HWND Parent, CPluginInterface* APlugin,
                                                   TTerminalQueue* Queue) : CSalamanderGeneralLocal(APlugin),
                                                                            FParent(Parent), FQueue(Queue),
                                                                            FClosing(false)
{
    CALL_STACK_MESSAGE5("TSalamandQueueController::TSalamandQueueController(%p, %x, %p, %p)",
                        this, Parent, APlugin, Queue);

    FSection = new TCriticalSection;
    FPendingRefreshes = new TStringList();

    FQueue->OnListUpdate = QueueListUpdate;
    FQueue->OnQueueItemUpdate = QueueItemUpdate;
    FQueue->OnQueryUser = QueueQueryUser;
    FQueue->OnPromptUser = QueuePromptUser;
    FQueue->OnShowExtendedException = QueueShowExtendedException;

    FQueueStatus = FQueue->CreateStatus(NULL);
}
//---------------------------------------------------------------------------
TSalamandQueueController::~TSalamandQueueController()
{
    CALL_STACK_MESSAGE2("TSalamandQueueController::~TSalamandQueueController(%p)",
                        this);

    // As a last-resort, close controller now,
    // but generally the controller should be closed before the queue is disposed
    // to cover all thread-issues
    if (!FClosing)
    {
        Close();
    }

    {
        TGuard Guard(FSection);
        CALL_STACK_MESSAGE1("TSalamandQueueController::~TSalamandQueueController(guarded)");
        CloseAll(FForms);
    }

    delete FQueueStatus;
    FQueueStatus = NULL;

    delete FPendingRefreshes;
    FPendingRefreshes = NULL;

    delete FSection;
    FSection = NULL;
}
//---------------------------------------------------------------------------
void TSalamandQueueController::Close()
{
    CALL_STACK_MESSAGE3("TSalamandQueueController::Close(%p, %d)",
                        this, FSection->Acquired);

    TGuard Guard(FSection);

    CALL_STACK_MESSAGE1("TSalamandQueueController::Close(guarded)");

    // make sure no new user interaction starts
    FClosing = true;

    assert(FQueue != NULL);
    FQueue->OnListUpdate = NULL;
    FQueue->OnQueueItemUpdate = NULL;
    FQueue->OnQueryUser = NULL;
    FQueue->OnPromptUser = NULL;
    FQueue->OnShowExtendedException = NULL;

    // close all pending user interactions
    ClosingAll();
}
//---------------------------------------------------------------------------
void TSalamandQueueController::ClosingAll()
{
    CALL_STACK_MESSAGE2("TSalamandQueueController::ClosingAll(%p)", this);

    TForms::iterator fi = FForms.begin();
    while (fi != FForms.end())
    {
        (*fi)->Closing();
        fi++;
    }
}
//---------------------------------------------------------------------------
void TSalamandQueueController::CloseAll(TForms& Forms)
{
    CALL_STACK_MESSAGE2("TSalamandQueueController::CloseAll(%p)", this);

    TForms::iterator fi = Forms.begin();
    while (fi != Forms.end())
    {
        TSalamandProgressThread* Form = *fi;
        // call to Closing() is duplicated in Close() to avoid dead-locks, when exiting.
        // probably it is not necessary anymore here
        Form->Closing();
        Form->Close();
        delete Form;
        fi++;
    }
    Forms.clear();
}
//---------------------------------------------------------------------------
void __fastcall TSalamandQueueController::QueueListUpdate(TTerminalQueue* Queue)
{
    CALL_STACK_MESSAGE4("TSalamandQueueController::QueueListUpdate(%p, %d, %p)",
                        this, FSection->Acquired, Queue);

    TForms Forms;

    {
        TGuard Guard(FSection);

        CALL_STACK_MESSAGE1("TSalamandQueueController::QueueListUpdate(guarded)");

        if (!FClosing && (FQueue == Queue))
        {
            FQueueStatus = FQueue->CreateStatus(FQueueStatus);

            if (FQueueStatus != NULL)
            {
                for (int ItemIndex = 0; ItemIndex < FQueueStatus->Count; ItemIndex++)
                {
                    TQueueItemProxy* QueueItem = FQueueStatus->Items[ItemIndex];
                    TSalamandProgressThread* Form;

                    if (QueueItem->UserData == NULL)
                    {
                        Form = new TSalamandProgressThread(FParent, FPlugin, this, QueueItem);
                        Form->Start();
                        QueueItem->UserData = Form;
                    }
                    else
                    {
                        Form = static_cast<TSalamandProgressThread*>(QueueItem->UserData);
                        TForms::iterator fi = std::find(FForms.begin(), FForms.end(), Form);
                        assert(fi != FForms.end());
                        FForms.erase(fi);
                        Form->ScheduleUpdate();
                    }
                    Forms.push_back(Form);
                }
            }

            ClosingAll();

            FForms.swap(Forms);
        }
    }

    // let the pending forms exit their guarded callbacks to controller

    CloseAll(Forms);
}
//---------------------------------------------------------------------------
void __fastcall TSalamandQueueController::QueueItemUpdate(TTerminalQueue* Queue,
                                                          TQueueItem* Item)
{
    CALL_STACK_MESSAGE5("TSalamandQueueController::QueueListUpdate(%p, %d, %p, %p)",
                        this, FSection->Acquired, Queue, Item);

    TGuard Guard(FSection);

    CALL_STACK_MESSAGE1("TSalamandQueueController::QueueListUpdate(guarded)");

    if (!FClosing && (FQueue == Queue))
    {
        assert(FQueueStatus != NULL);

        TQueueItemProxy* QueueItem = FQueueStatus->FindByQueueItem(Item);
        // not sure if QueueItemUpdate for new item can be ever called before
        // update of the whole list (QueueListUpdate)
        if (QueueItem != NULL)
        {
            if (Item->Status == TQueueItem::qsDone)
            {
                if (!QueueItem->Info->ModifiedLocal.IsEmpty())
                {
                    FPendingRefreshes->AddObject(QueueItem->Info->ModifiedLocal, (TObject*)false);
                }
                if (!QueueItem->Info->ModifiedRemote.IsEmpty())
                {
                    FPendingRefreshes->AddObject(QueueItem->Info->ModifiedRemote, (TObject*)true);
                }
            }

            if (QueueItem != NULL)
            {
                TSalamandProgressThread* Form =
                    static_cast<TSalamandProgressThread*>(QueueItem->UserData);

                assert(Form != NULL);
                Form->ScheduleUpdate();
            }
        }
    }
}
//---------------------------------------------------------------------------
void TSalamandQueueController::UpdateMe(TSalamandProgressThread* Form)
{
    CALL_STACK_MESSAGE4("TSalamandQueueController::UpdateMe(%p, %d, %p)",
                        this, FSection->Acquired, Form);

    {
        TGuard Guard(FSection);

        CALL_STACK_MESSAGE1("TSalamandQueueController::UpdateMe(guarded)");

        Form->Update();
    }
}
//---------------------------------------------------------------------------
bool TSalamandQueueController::PathToRefresh(bool& Remote, AnsiString& Path)
{
    CALL_STACK_MESSAGE3("TSalamandQueueController::PathToRefresh(%p, %d)",
                        this, FSection->Acquired);

    bool Result;

    {
        TGuard Guard(FSection);

        CALL_STACK_MESSAGE1("TSalamandQueueController::PathToRefresh(guarded)");

        Result = (FPendingRefreshes->Count > 0);
        if (Result)
        {
            // for performance reasons pop from back
            Remote = (bool)FPendingRefreshes->Objects[FPendingRefreshes->Count - 1];
            Path = FPendingRefreshes->Strings[FPendingRefreshes->Count - 1];
            FPendingRefreshes->Delete(FPendingRefreshes->Count - 1);
        }
    }

    return Result;
}
//---------------------------------------------------------------------------
void __fastcall TSalamandQueueController::QueueQueryUser(TObject* /*Sender*/,
                                                         const AnsiString Query, TStrings* MoreMessages, int Answers,
                                                         const TQueryParams* Params, int& Answer, TQueryType Type, void* Arg)
{
    CALL_STACK_MESSAGE8("TSalamandQueueController::QueueQueryUser(%p, %s, %p, %x, %p, %d, %p)",
                        this, Query.c_str(), MoreMessages, Answers, Params, Type, Arg);

    if (FClosing)
    {
        Answer = AbortAnswer(Answers);
    }
    else
    {
        TSalamandProgressThread* Form = static_cast<TSalamandProgressThread*>(Arg);
        assert(Form != NULL);

        AnsiString HelpKeyword;
        TMessageParams MessageParams(Params);
        if (Params != NULL)
        {
            HelpKeyword = Params->HelpKeyword;
        }

        {
            // Now we can safely leave the controller's critical section to allow
            // controller update other transfers while we interact with user.
            // We rely here on queue not to dispose the queue item while it interacts with user.
            TUnguard Unguard(FSection);
            CALL_STACK_MESSAGE1("TSalamandQueueController::QueueQueryUser(unguarded)");
            Answer = SalamanderMessageDialog(Form->GetHWindow(),
                                             Query, MoreMessages, Type, Answers, HelpKeyword, &MessageParams);
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TSalamandQueueController::QueuePromptUser(TTerminal* /*Terminal*/,
                                                          TPromptKind Kind, AnsiString Name, AnsiString /*Instructions*/,
                                                          TStrings* Prompts, TStrings* Results, bool& Result, void* Arg)
{
    CALL_STACK_MESSAGE4("TSalamandQueueController::QueuePromptUser(%p, %s, %p)",
                        this, Name.c_str(), Arg);

    if (FClosing)
    {
        Result = false;
    }
    else
    {
        TSalamandProgressThread* Form = static_cast<TSalamandProgressThread*>(Arg);
        assert(Form != NULL);

        for (int Index = 0; Index < Prompts->Count; Index++)
        {
            TPasswordDialog Dialog(Form->GetHWindow(), FPlugin, Name, Prompts->Strings[Index], Kind);

            {
                // Now we can safely leave the controller's critical section to allow
                // controller update other transfers while we interact with user.
                // We rely here on queue not to dispose the queue item while it interacts with user.
                TUnguard Unguard(FSection);
                CALL_STACK_MESSAGE1("TSalamandQueueController::QueuePromptUser(unguarded)");
                AnsiString Password = Results->Strings[Index];
                Result = Dialog.Execute(Password);
                if (Result)
                {
                    Results->Strings[Index] = Password;
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall TSalamandQueueController::QueueShowExtendedException(
    TTerminal* /*Terminal*/, Exception* E, void* Arg)
{
    CALL_STACK_MESSAGE4("TSalamandQueueController::QueueShowExtendedException(%p, %p, %p)",
                        this, E, Arg);

    AnsiString Message;
    if (!FClosing &&
        ExceptionMessage(E, Message))
    {
        TSalamandProgressThread* Form = static_cast<TSalamandProgressThread*>(Arg);
        assert(Form != NULL);

        TStrings* MoreMessages = NULL;
        ExtException* EE = dynamic_cast<ExtException*>(E);
        if (EE != NULL)
        {
            MoreMessages = EE->MoreMessages;
        }
        TQueryType Type = (E->InheritsFrom(__classid(ESshTerminate)) ? qtInformation : qtError);

        {
            // Now we can safely leave the controller's critical section to allow
            // controller update other transfers while we interact with user.
            // We rely here on queue not to dispose the queue item while it interacts with user.
            TUnguard Unguard(FSection);
            CALL_STACK_MESSAGE1("TSalamandQueueController::QueueShowExtendedException(unguarded)");
            SalamanderMessageDialog(Form->GetHWindow(), Message, MoreMessages, Type,
                                    qaOK, HELP_NONE, NULL);
        }
    }
}
