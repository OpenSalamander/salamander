// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include "spl_fs.h"
#include <set>
//---------------------------------------------------------------------------
class TSessionData;
class CPluginInterface;
class CPluginFSInterfaceAbstract;
//---------------------------------------------------------------------------
const fsmClose = 0x00;
const fsmFullSynchronize = 0x01;
const fsmSynchronize = 0x02;
//---------------------------------------------------------------------------
class CPluginInterfaceForFS : public CPluginInterfaceForFSAbstract,
                              protected CSalamanderGeneralLocal
{
public:
    CPluginInterfaceForFS(CPluginInterface* APlugin);
    virtual ~CPluginInterfaceForFS();

    void __fastcall ConnectFileSystem(int Panel);
    void __fastcall RecryptPasswords();

    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(const char* fsName, int fsNameIndex);
    virtual void WINAPI CloseFS(CPluginFSInterfaceAbstract* FS);

    virtual void WINAPI ExecuteOnFS(int Panel,
                                    CPluginFSInterfaceAbstract* PluginFS, const char* PluginFSName,
                                    int PluginFSNameIndex, CFileData& File, int IsDir);
    virtual BOOL WINAPI DisconnectFS(HWND Parent, BOOL IsInPanel, int Panel,
                                     CPluginFSInterfaceAbstract* PluginFS, const char* PluginFSName,
                                     int PluginFSNameIndex);

    virtual void WINAPI ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) {}
    virtual void WINAPI ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) {}

    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server,
                                                  const char* share) {}

    virtual void WINAPI ExecuteChangeDriveMenuItem(int Panel);
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND Parent, int Panel,
                                                       int X, int Y, CPluginFSInterfaceAbstract* PluginFS, const char* PluginFSName,
                                                       int PluginFSNameIndex, BOOL IsDetachedFS, BOOL& RefreshMenu,
                                                       BOOL& CloseMenu, int& PostCmd, void*& PostCmdParam);
    virtual void WINAPI ExecuteChangeDrivePostCommand(int Panel, int PostCmd,
                                                      void* PostCmdParam);

    static void __fastcall ParseUserPart(AnsiString UserPart,
                                         bool UnixPath, AnsiString* ConnectInfo, AnsiString* HostName, int* PortNumber,
                                         AnsiString* UserName, AnsiString* Password, AnsiString* Path);
    static TSessionData* __fastcall GetSessionData(int FSNameIndex = 0,
                                                   const AnsiString UserPart = "", AnsiString* Path = NULL);
    static void __fastcall SetForceNewSession(bool value);
    static bool __fastcall GetForceNewSession();
    static int SessionMenu(HWND Parent, int X, int Y, bool /*IsDetachedFS*/);

protected:
    static bool FForceNewSesion;
    int FActiveFSCount;
    TSessionData* FSessionToOpen;
    typedef std::set<CPluginFSInterface*> TFSs;
    TFSs FFSs;
};
//---------------------------------------------------------------------------
