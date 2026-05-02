// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include <HelpIntfs.hpp>
#include <WinHelpViewer.hpp>

#include "Salamand.h"
#include "Salamander.rh"
#include "HelpCore.h"
#include "HelpWin.h"
#include <Common.h>
#include <Tools.h>
#include <TextsWin.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
class TSalamandHelpSystem : public TInterfacedObject, public ICustomHelpViewer
{
public:
    virtual AnsiString __fastcall GetViewerName();
    virtual int __fastcall UnderstandsKeyword(const AnsiString HelpString);
    virtual TStringList* __fastcall GetHelpStrings(const AnsiString HelpString);
    virtual bool __fastcall CanShowTableOfContents();
    virtual void __fastcall ShowTableOfContents();
    virtual void __fastcall ShowHelp(const AnsiString HelpString);
    virtual void __fastcall NotifyID(const int ViewerID);
    virtual void __fastcall SoftShutDown();
    virtual void __fastcall ShutDown();

    IUNKNOWN
};
//---------------------------------------------------------------------------
void __fastcall InitializeWinHelp()
{
    InitializeCustomHelp(new TSalamandHelpSystem);
}
//---------------------------------------------------------------------------
void __fastcall FinalizeWinHelp()
{
    FinalizeCustomHelp();
}
//---------------------------------------------------------------------------
AnsiString __fastcall TSalamandHelpSystem::GetViewerName()
{
    return "Salamand";
}
//---------------------------------------------------------------------------
int __fastcall TSalamandHelpSystem::UnderstandsKeyword(const AnsiString HelpString)
{
    // pretend that we know everyting
    return 1;
}
//---------------------------------------------------------------------------
TStringList* __fastcall TSalamandHelpSystem::GetHelpStrings(const AnsiString HelpString)
{
    TStringList* Result = new TStringList();
    Result->Add(GetViewerName() + " : " + HelpString);
    return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TSalamandHelpSystem::CanShowTableOfContents()
{
    return true;
}
//---------------------------------------------------------------------------
void __fastcall TSalamandHelpSystem::ShowTableOfContents()
{
    PluginInterface.Help();
}
//---------------------------------------------------------------------------
void __fastcall TSalamandHelpSystem::ShowHelp(const AnsiString HelpString)
{
    unsigned int HelpContext;

#define SAL_HELP_MAP_BEGIN
#define SAL_HELP_MAP_END HelpContext = 0;
#define SAL_HELP_MAP(CONTEXT, KEYWORD) \
    if (HelpString == KEYWORD) \
        HelpContext = CONTEXT; \
    else

    SAL_HELP_MAP_BEGIN
    SAL_HELP_MAP(IDH_UI_ABOUT, "ui_about")
    SAL_HELP_MAP(IDH_SESSION_SAVE, "ui_login_save")
    SAL_HELP_MAP(IDH_UI_COPY, "ui_copy")
    SAL_HELP_MAP(IDH_UI_TRANSFER_CUSTOM, "ui_transfer_custom")
    SAL_HELP_MAP(IDH_UI_FSINFO, "ui_fsinfo")
    SAL_HELP_MAP(IDH_UI_SYNCHRONIZE, "ui_synchronize")
    SAL_HELP_MAP(IDH_UI_LOGIN, "ui_login")
    SAL_HELP_MAP(IDH_UI_LOGIN_SAVE, "ui_login_save")
    SAL_HELP_MAP(IDH_UI_LOGIN_STORED_SESSIONS, "ui_login_stored_sessions")
    SAL_HELP_MAP(IDH_UI_LOGIN_SESSION, "ui_login_session")
    SAL_HELP_MAP(IDH_UI_LOGIN_SSH, "ui_login_ssh")
    SAL_HELP_MAP(IDH_UI_LOGIN_ENVIRONMENT, "ui_login_environment")
    SAL_HELP_MAP(IDH_UI_LOGIN_DIRECTORIES, "ui_login_directories")
    SAL_HELP_MAP(IDH_UI_LOGIN_RECYCLE_BIN, "ui_login_proxy")
    SAL_HELP_MAP(IDH_UI_LOGIN_SCP, "ui_login_scp")
    SAL_HELP_MAP(IDH_UI_LOGIN_SFTP, "ui_login_sftp")
    SAL_HELP_MAP(IDH_UI_LOGIN_LOGGING, "ui_login_logging")
    SAL_HELP_MAP(IDH_UI_LOGIN_CONNECTION, "ui_login_connection")
    SAL_HELP_MAP(IDH_UI_LOGIN_PROXY, "ui_login_proxy")
    SAL_HELP_MAP(IDH_UI_LOGIN_BUGS, "ui_login_bugs")
    SAL_HELP_MAP(IDH_UI_LOGIN_AUTHENTICATION, "ui_login_authentication")
    SAL_HELP_MAP(IDH_UI_LOGIN_KEX, "ui_login_kex")
    SAL_HELP_MAP(IDH_UI_LOGIN_TUNNEL, "ui_login_tunnel")
    SAL_HELP_MAP(IDH_UI_PROGRESS, "ui_progress")
    SAL_HELP_MAP(IDH_UI_PROPERTIES, "ui_properties")
    SAL_HELP_MAP(IDH_UI_KEEPUPTODATE, "ui_keepuptodate")
    SAL_HELP_MAP(IDH_START, "start")
    SAL_HELP_MAP(IDH_SESSION_SAVE_OVERWRITE, HELP_SESSION_SAVE_OVERWRITE)
    SAL_HELP_MAP(IDH_SYNCHRONIZE_SAVE_MODE, HELP_SYNCHRONIZE_SAVE_MODE)
    SAL_HELP_MAP(IDH_SESSION_SAVE_PASSWORD, "ui_login_save")
    SAL_HELP_MAP(IDH_DELETE_SESSION, HELP_DELETE_SESSION)
    SAL_HELP_MAP(IDH_SESSION_SAVE_DEFAULT, HELP_SESSION_SAVE_DEFAULT)
    SAL_HELP_MAP(IDH_LOGIN_KEY_TYPE, HELP_LOGIN_KEY_TYPE)
    SAL_HELP_MAP(IDH_PROGRESS_CANCEL, HELP_PROGRESS_CANCEL)
    SAL_HELP_MAP(IDH_KEEPUPTODATE_SYNCHRONIZE, HELP_KEEPUPTODATE_SYNCHRONIZE)
    SAL_HELP_MAP(IDH_REMOTE_TRANSFER, HELP_REMOTE_MOVE)
    SAL_HELP_MAP(IDH_UI_SAL_PREF_CONFIRMATIONS, "ui_sal_pref_confirmations")
    SAL_HELP_MAP(IDH_UI_SAL_PREF_TRANSFER, "ui_sal_pref_transfer")
    SAL_HELP_MAP(IDH_UI_SAL_PREF_QUEUE, "ui_sal_pref_queue")
    SAL_HELP_MAP(IDH_UI_SAL_PREF_PANEL, "ui_sal_pref_panel")
    SAL_HELP_MAP(IDH_UI_SAL_PREFERENCES, "ui_sal_preferences")
    SAL_HELP_MAP(IDH_SHELL_SESSION, "shell_session")
    SAL_HELP_MAP(IDH_UI_AUTHENTICATE, "ui_authenticate")
    SAL_HELP_MAP(IDH_UI_CONSOLE, "ui_console")
    SAL_HELP_MAP(IDH_UI_LOGIN_TUNNEL, "ui_login_tunnel")
    SAL_HELP_MAP(IDH_UI_SYNCHRONIZE_CHECKLIST, "ui_synchronize_checklist")
    SAL_HELP_MAP_END

    if (HelpContext == 0)
    {
        ShowTableOfContents();
    }
    else
    {
        PluginInterface.HelpContext(HelpContext);
    }
}
//---------------------------------------------------------------------------
void __fastcall TSalamandHelpSystem::NotifyID(const int /*ViewerID*/)
{
}
//---------------------------------------------------------------------------
void __fastcall TSalamandHelpSystem::SoftShutDown()
{
}
//---------------------------------------------------------------------------
void __fastcall TSalamandHelpSystem::ShutDown()
{
}
