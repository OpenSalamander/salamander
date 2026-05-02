// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include <strstream>
#include <spl_com.h>
#include <spl_base.h>
#include <spl_vers.h>
#include <spl_gen.h>

#define PHKEY ::PHKEY

#include <mhandles.h>
#include <dbg.h>
//---------------------------------------------------------------------------
extern HINSTANCE DLLInstance;
extern HINSTANCE HLanguage;
//---------------------------------------------------------------------------
char* SalLoadStr(int resID);

const pmDisconnectF12 = 1;
const pmDisconnectLeft = 2;
const pmDisconnectRight = 3;
const pmConnect = 4;
const pmFullSynchronize = 5;
// 6 and 7 was QueueUserActionLeft/QueueUserActionRight
// 8 was pmConfigure
// 9 was pmQueueShow
const pmSynchronize = 10;

class CPluginInterfaceForMenuExt;
class CPluginInterfaceForFS;
class CPluginFSInterface;
class CPluginInterface;
class TMessageParams;
//---------------------------------------------------------------------------
class CSalamanderGeneralLocal
{
public:
    CSalamanderGeneralLocal(CPluginInterface* APlugin);

    CSalamanderGeneralAbstract* SalamanderGeneral();
    CSalamanderGUIAbstract* SalamanderGUI();
    void SetParentWindow(HWND Parent);
    void ClearParentWindow();
    void ChangePanelPathOutOfPlugin(int Panel);
    CPluginFSInterface* __fastcall GetPanelFS(int Panel);
    int SalamanderMessageDialog(HWND Parent, const AnsiString Message,
                                TStrings* MoreMessages, int Type, int Answers,
                                AnsiString HelpKeyword = "", const TMessageParams* Params = NULL);
    void BugReport(AnsiString Report);

protected:
    CPluginInterface* FPlugin;
};
//---------------------------------------------------------------------------
class CPluginInterface : public CPluginInterfaceAbstract,
                         protected CSalamanderGeneralLocal
{
public:
    CPluginInterface();
    ~CPluginInterface();

    bool Init(CSalamanderPluginEntryAbstract* Salamander);

    virtual void WINAPI About(HWND Parent);

    virtual BOOL WINAPI Release(HWND Parent, BOOL Borce);

    virtual void WINAPI LoadConfiguration(HWND Parent, HKEY RegKey,
                                          CSalamanderRegistryAbstract* Registry);
    virtual void WINAPI SaveConfiguration(HWND Parent, HKEY RegKey,
                                          CSalamanderRegistryAbstract* Registry);
    virtual void WINAPI Configuration(HWND Parent);

    virtual void WINAPI Connect(HWND Parent,
                                CSalamanderConnectAbstract* Salamander);

    virtual void WINAPI ReleasePluginDataInterface(
        CPluginDataInterfaceAbstract* PluginData);

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver();
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader();

    virtual void WINAPI Event(int Event, DWORD Param);
    virtual void WINAPI ClearHistory(HWND Parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* Path,
                                                       BOOL IncludingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND Parent, int Event);

    const char* GetFSName(int Protocol, bool Current);
    int GetFSNameIndex(const char* FSName);
    const HINSTANCE GetDLLInstance() { return DLLInstance; }
    CSalamanderGeneralAbstract* GetSalamanderGeneral() { return FSalamanderGeneral; };
    CSalamanderGUIAbstract* GetSalamanderGUI() { return FSalamanderGUI; }
    void ConnectFileSystem(int Panel);
    void PreferencesDialog();
    void Help();
    void HelpContext(unsigned int HelpContext);
    HWND ParentWindow();
    void RecryptPasswords();

private:
    CSalamanderGeneralAbstract* FSalamanderGeneral;
    CSalamanderGUIAbstract* FSalamanderGUI;
    CPluginInterfaceForFS* FInterfaceForFS;
    CPluginInterfaceForMenuExt* FInterfaceForMenuExt;
    AnsiString FFSNames[3];
};
//---------------------------------------------------------------------------
extern CPluginInterface PluginInterface;
//---------------------------------------------------------------------------
class CPluginFSInterface;
//---------------------------------------------------------------------------
