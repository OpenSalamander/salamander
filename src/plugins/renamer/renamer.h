// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define CURRENT_CONFIG_VERSION 1

// menu ID definitions
#define MID_RENAME 1
#define MID_UNDO 2

// Salamander general interface - valid from startup until the plugin terminates
extern CSalamanderGeneralAbstract* SG;

extern CSalamanderGUIAbstract* SalGUI;

class CWindowQueueEx : public CWindowQueue
{
public:
    CWindowQueueEx() : CWindowQueue("Renamer Dialogs") {}

    HWND GetLastWnd()
    {
        CS.Enter();
        HWND w = NULL;
        CWindowQueueItem* next = Head;
        if (next != NULL) // find the last window
        {
            while (next->Next)
                next = next->Next;
            w = next->HWindow;
        }
        CS.Leave();
        return w;
    }
};

extern CThreadQueue ThreadQueue;
extern CWindowQueueEx WindowQueue;
extern BOOL AlwaysOnTop;

// ****************************************************************************

struct CCS
{
    CRITICAL_SECTION cs;

    CCS() { InitializeCriticalSection(&cs); }
    ~CCS() { DeleteCriticalSection(&cs); }

    void Enter() { EnterCriticalSection(&cs); }
    void Leave() { LeaveCriticalSection(&cs); }
};

// ****************************************************************************

//char * ErrorStr(char * buf, int error, ...);
BOOL ErrorHelper(HWND parent, const char* message, int lastError, va_list arglist);
BOOL Error(HWND parent, int error, ...);
BOOL Error(HWND parent, const char* error, ...);
BOOL Error(int error, ...);
// BOOL ErrorL(int lastError, HWND parent, int error, ...);
BOOL ErrorL(int lastError, int error, ...);
int SalPrintf(char* buffer, unsigned count, const char* format, ...);

void OnConfiguration(HWND hParent);

// ****************************************************************************
//
// Plugin interface
//

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) { ; }

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; };
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) { ; }

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

// ****************************************************************************

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
};

// ****************************************************************************

extern HINSTANCE DLLInstance; // handle to the SPL - language-independent resources
extern HINSTANCE HLanguage;   // handle to the SLG - language-dependent resources

char* LoadStr(int resID);
