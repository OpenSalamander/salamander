// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include "Salamand.h"
#include "SalamandConfiguration.h"
#include "Salamander.rh"

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <SecureShell.h>
#include <Exceptions.h>
#include <WinInterface.h>
#include <About.h>
#include <TextsWin.h>
#include <Tools.h>
#include <HelpWin.h>
//---------------------------------------------------------------------------
#undef TForm
//---------------------------------------------------------------------------
void __fastcall LocalSystemSettings(TCustomForm* Control)
{
    dynamic_cast<TForm*>(Control)->Position = poDesigned;
}
//---------------------------------------------------------------------------
__fastcall TSalamandForm::TSalamandForm(TComponent* AOwner) : TForm(AOwner)
{
    LocalSystemSettings();
}
//---------------------------------------------------------------------------
__fastcall TSalamandForm::TSalamandForm(TComponent* AOwner, int Dummy) : TForm(AOwner, Dummy)
{
    LocalSystemSettings();
}
//---------------------------------------------------------------------------
void __fastcall TSalamandForm::DoShow()
{
    TForm::DoShow();

    if (Showing)
    {
        HWND HParent =
            (Parent != NULL ? Parent->Handle : (FPreviousForm != NULL ? FPreviousForm->Handle : PluginInterface.GetSalamanderGeneral()->GetMainWindowHWND()));
        PluginInterface.GetSalamanderGeneral()->MultiMonCenterWindow(
            Handle, HParent, TRUE);
    }
}
//---------------------------------------------------------------------------
void TSalamandForm::LocalSystemSettings()
{
    FPreviousForm = Screen->ActiveForm;
}
//---------------------------------------------------------------------------
const AnsiString AppName = "WinSCP for Open Salamander";
//---------------------------------------------------------------------------
TConfiguration* __fastcall CreateConfiguration()
{
    return new TSalamandConfiguration(&PluginInterface);
}
//---------------------------------------------------------------------------
AnsiString __fastcall SshVersionString()
{
    return FORMAT("WinSCP-Salamander-release-%s", (Configuration->Version));
}
//---------------------------------------------------------------------------
AnsiString __fastcall AppNameString()
{
    return "WinSCP-Salamander";
}
//---------------------------------------------------------------------------
AnsiString __fastcall GetRegistryKey()
{
    return "Software\\Martin Prikryl\\WinSCP 2";
}
//---------------------------------------------------------------------------
void __fastcall ShowExtendedException(Exception* E)
{
    AnsiString Message; // not used
    if (ExceptionMessage(E, Message))
    {
        ESshTerminate* Terminate = dynamic_cast<ESshTerminate*>(E);

        if (Terminate == NULL)
        {
            ExceptionMessageDialog(E, qtError);
        }
        else
        {
            if ((Terminate->Operation == odoDisconnect) &&
                CustomWinConfiguration->ConfirmExitOnCompletion)
            {
                TMessageParams Params(mpNeverAskAgainCheck);
                if (ExceptionMessageDialog(E, qtInformation, "", qaOK, HELP_NONE, &Params) ==
                    qaNeverAskAgain)
                {
                    CustomWinConfiguration->ConfirmExitOnCompletion = false;
                }
            }

            switch (Terminate->Operation)
            {
            case odoDisconnect:
                break;

            case odoShutDown:
                ShutDownWindows();
                break;

            default:
                assert(false);
            }
        }
    }
}
//---------------------------------------------------------------------------
void __fastcall FlashOnBackground()
{
}
//---------------------------------------------------------------------
void __fastcall DoProductLicense()
{
    assert(false);
}
//---------------------------------------------------------------------------
void __fastcall CheckForUpdates(bool /*CachedResults*/)
{
    assert(false);
}
//---------------------------------------------------------------------------
bool __fastcall DoCleanupDialog(TStoredSessionList* /*SessionList*/,
                                TConfiguration* /*Configuration*/)
{
    assert(false);
    return false;
}
//---------------------------------------------------------------------------
bool __fastcall DoImportSessionsDialog(TStoredSessionList* /*SessionList*/)
{
    assert(false);
    return false;
}
//---------------------------------------------------------------------------
void __fastcall MenuPopup(TPopupMenu* Menu, TPoint Point,
                          TComponent* PopupComponent)
{
    Menu->PopupComponent = PopupComponent;
    Menu->Popup(Point.x, Point.y);
}
//---------------------------------------------------------------------------
void __fastcall UpgradeSpeedButton(TSpeedButton* /*Button*/)
{
    // no-op yet
}
//---------------------------------------------------------------------------
struct TThreadParam
{
    TThreadFunc ThreadFunc;
    void* Parameter;
};
//---------------------------------------------------------------------------
static unsigned int __stdcall SalamThreadProc(void* AParam)
{
    CALL_STACK_MESSAGE2("SalamThreadProc(%p)", AParam);

    SalamanderDebug->TraceAttachThread(GetCurrentThread(), GetCurrentThreadId());
    SetThreadNameInVCAndTrace("WinSCP background");

    TThreadParam* Param = reinterpret_cast<TThreadParam*>(AParam);
    unsigned int Result = Param->ThreadFunc(Param->Parameter);
    delete Param;

    return Result;
}
//---------------------------------------------------------------------------
static int __fastcall ThreadProc(void* Param)
{
    int Result = SalamanderDebug->CallWithCallStack(SalamThreadProc, Param);
    EndThread(Result);
    return Result;
}
//---------------------------------------------------------------------------
int __fastcall StartThread(void* SecurityAttributes, unsigned StackSize,
                           TThreadFunc ThreadFunc, void* Parameter, unsigned CreationFlags,
                           unsigned& ThreadId)
{
    CALL_STACK_MESSAGE1("StartThread");

    TThreadParam* Param = new TThreadParam;
    Param->ThreadFunc = ThreadFunc;
    Param->Parameter = Parameter;
    return BeginThread(SecurityAttributes, StackSize, ThreadProc, Param,
                       CreationFlags, ThreadId);
}
//---------------------------------------------------------------------------
void __fastcall ShowNotification(TTerminal* /*Terminal*/, const AnsiString& /*Str*/,
                                 TQueryType /*Type*/)
{
    // noop
}
//---------------------------------------------------------------------------
bool __fastcall DoRemoteCopyDialog(TStrings* /*Sessions*/, TStrings* /*Directories*/,
                                   TDirectRemoteCopy /*AllowDirectCopy*/, void*& /*Session*/,
                                   AnsiString& Target, AnsiString& FileMask, bool& /*DirectCopy*/)
{
    AnsiString Value = UnixIncludeTrailingBackslash(Target) + FileMask;
    TStrings* History = CustomWinConfiguration->History["RemoteTarget"];
    bool Result = InputDialog(
        LoadStr(REMOTE_COPY_TITLE), LoadStr(REMOTE_TRANSFER_PROMPT),
        Value, HELP_REMOTE_MOVE, History, true);
    if (Result)
    {
        CustomWinConfiguration->History["RemoteTarget"] = History;
        Target = UnixExtractFilePath(Value);
        FileMask = UnixExtractFileName(Value);
    }
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall MessageWithNoHelp(const AnsiString& /*Message*/)
{
    TMessageParams Params;
    Params.AllowHelp = false; // to avoid recursion
    MessageDialog(LoadStr(SAL_NO_HELP), qtInformation, qaOK, HELP_NONE, &Params);
}
//---------------------------------------------------------------------------
