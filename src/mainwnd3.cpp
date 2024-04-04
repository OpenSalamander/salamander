// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <shlwapi.h>
#undef PathIsPrefix // otherwise collision with CSalamanderGeneral::PathIsPrefix

#include "htmlhelp.h"
#include "stswnd.h"
#include "editwnd.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "toolbar.h"
#include "mainwnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "snooper.h"
#include "shellib.h"
#include "menu.h"
#include "pack.h"
#include "filesbox.h"
#include "drivelst.h"
#include "cache.h"
#include "gui.h"
#include <uxtheme.h>
#include "zip.h"
#include "tasklist.h"
#include "jumplist.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "worker.h"
#include "find.h"
#include "viewer.h"

// critical shutdown: how much time can we spend in WM_QUERYENDSESSION (then comes
// KILL from woken), it's 5s (5s with an open msgbox, 10s without message pumping), I left
// reserve 500ms, I tried it on Vista, Win7, Win8, Win10
#define QUERYENDSESSION_TIMEOUT 4500

// Variables used during saving configuration during shutdown, log-off, or restart (we must
// pump messages to prevent our system from crashing like a "not responding" software)
CWaitWindow* GlobalSaveWaitWindow = NULL; // if there is a global wait window for Save, it is here (otherwise it is NULL)
int GlobalSaveWaitWindowProgress = 0;     // current value of the progress of the global wait window for Save

// Let's borrow constants from a newer SDK
#define WM_APPCOMMAND 0x0319
#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_KEY 0
#define FAPPCOMMAND_OEM 0x1000
#define FAPPCOMMAND_MASK 0xF000
#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define APPCOMMAND_BROWSER_BACKWARD 1
#define APPCOMMAND_BROWSER_FORWARD 2
/* we do not support yet
#define APPCOMMAND_BROWSER_SEARCH         5
#define APPCOMMAND_HELP                   27
#define APPCOMMAND_BROWSER_REFRESH        3
#define APPCOMMAND_FIND                   28
#define APPCOMMAND_COPY                   36
#define APPCOMMAND_CUT                    37
#define APPCOMMAND_PASTE                  38*/

const int SPLIT_LINE_WIDTH = 3; // Width of Split line in points
// if the middle toolbar is displayed, the composition will be SPLIT_LINE_WIDTH + toolbar + SPLIT_LINE_WIDTH

const int MIN_WIN_WIDTH = 2; // minimum panel width

extern BOOL CacheNextSetFocus;

BOOL MainFrameIsActive = FALSE;

// code for testing time losses
/*    const char *s1 = "aj hjka sakjSJKAHS AJKSH JKDSHFJSDH FJS HDFJSD HFJS";
  const char *s2 = "Aj hjka sakjSJKAHS AJKSH JKDSHFJSDH FJS HDFJSD HFJS";

  LARGE_INTEGER t1, t2, t3, f;

  int len1 = strlen(s1);
  int count = 100000;
  QueryPerformanceCounter(&t1);
  int c = 0;
  int i;
  for (i = 0; i < count; i++)
    c += MemICmp(s1, s2, len1);
  QueryPerformanceCounter(&t2);
  c = 0;
  for (i = 0; i < count; i++)
    c += StrICmp(s1, len1, s2, len1);
  QueryPerformanceCounter(&t3);

  QueryPerformanceFrequency(&f);

  char buff[200];
  double a = (double)(t2.QuadPart - t1.QuadPart) / f.QuadPart;
  double b = (double)(t3.QuadPart - t2.QuadPart) / f.QuadPart;
  sprintf(buff, "t1=%1.4lg\nt2=%1.4lg", a, b);
  MessageBox(HWindow, buff, "Results", MB_OK);*/

//****************************************************************************
//
// HtmlHelp support
//

// universal callback for our MessageBox, when the user clicks on the HELP button
// It is necessary to call like this, for example:
//    MSGBOXEX_PARAMS params;
//    params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
//    params.ContextHelpId = IDH_LICENSE;
//    params.HelpCallback = MessageBoxHelpCallback;
void CALLBACK MessageBoxHelpCallback(LPHELPINFO helpInfo)
{
    OpenHtmlHelp(NULL, MainWindow->HWindow, HHCDisplayContext, (UINT)helpInfo->dwContextId, FALSE); // MSGBOXEX_PARAMS::ContextHelpId
}

CSalamanderHelp SalamanderHelp;

void CSalamanderHelp::OnHelp(HWND hWindow, UINT helpID, HELPINFO* helpInfo,
                             BOOL ctrlPressed, BOOL shiftPressed)
{
    if (!ctrlPressed && !shiftPressed)
    {
        OpenHtmlHelp(NULL, hWindow, HHCDisplayContext, helpID, FALSE);
    }
}

void CSalamanderHelp::OnContextMenu(HWND hWindow, WORD xPos, WORD yPos)
{
}

typedef struct tagHH_LAST_ERROR
{
    int cbStruct;
    HRESULT hr;
    BSTR description;
} HH_LAST_ERROR;

BOOL OpenHtmlHelp(char* helpFileName, HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet)
{
    //  SalMessageBox(parent, "This beta version doesn't contain help.\nPlease wait for the next beta version.",
    //                "Open Salamander Help", MB_OK | MB_ICONINFORMATION);

    HANDLES(EnterCriticalSection(&OpenHtmlHelpCS));

    char helpPath[MAX_PATH + 50];
    if (CurrentHelpDir[0] == 0)
    {
        char helpSubdir[MAX_PATH];
        helpSubdir[0] = 0;
        CLanguage language;
        if (language.Init(Configuration.LoadedSLGName, NULL))
        {
            lstrcpyn(helpSubdir, language.HelpDir, MAX_PATH);
            language.Free();
        }
        if (helpSubdir[0] == 0)
        {
            TRACE_E("OpenHtmlHelp(): unable to get (or empty) SLGHelpDir!");
            strcpy(helpSubdir, "english");
        }
        BOOL ok = FALSE;
        if (GetModuleFileName(HInstance, CurrentHelpDir, MAX_PATH) != 0 &&
            CutDirectory(CurrentHelpDir) &&
            SalPathAppend(CurrentHelpDir, "help", MAX_PATH) &&
            DirExists(CurrentHelpDir))
        {
            lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
            if (!SalPathAppend(helpPath, helpSubdir, MAX_PATH) ||
                !DirExists(helpPath))
            { // directory obtained from the current .slg file of Salamander does not exist
                lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
                if (_stricmp(helpSubdir, "english") == 0 || // "english" has already been tested and does not exist, so trying it again doesn't make sense
                    !SalPathAppend(helpPath, "english", MAX_PATH) ||
                    !DirExists(helpPath))
                { // Directory ENGLISH does not exist
                    lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
                    if (SalPathAppend(helpPath, "*", MAX_PATH))
                    { // let's try to find at least some other directory
                        WIN32_FIND_DATA data;
                        HANDLE find = HANDLES_Q(FindFirstFile(helpPath, &data));
                        if (find != INVALID_HANDLE_VALUE)
                        {
                            do
                            {
                                if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0 &&
                                    (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) // only if it concerns a directory
                                {
                                    lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
                                    if (SalPathAppend(helpPath, data.cFileName, MAX_PATH))
                                    {
                                        ok = TRUE;
                                        break;
                                    }
                                }
                            } while (FindNextFile(find, &data));
                            HANDLES(FindClose(find));
                        }
                    }
                }
                else
                    ok = TRUE;
            }
            else
                ok = TRUE;
            if (ok)
                lstrcpyn(CurrentHelpDir, helpPath, MAX_PATH);
        }
        if (!ok)
        {
            CurrentHelpDir[0] = 0;

            HANDLES(LeaveCriticalSection(&OpenHtmlHelpCS));

            if (!quiet)
            {
                SalMessageBox(parent, LoadStr(IDS_FAILED_TO_FIND_HELP),
                              LoadStr(IDS_HELPERROR), MB_OK | MB_ICONEXCLAMATION);
            }
            return FALSE;
        }
    }

    HANDLES(LeaveCriticalSection(&OpenHtmlHelpCS));

    HH_FTS_QUERY query;
    DWORD uCommand = 0;
    switch (command)
    {
    case HHCDisplayTOC:
    {
        uCommand = HH_DISPLAY_TOC;
        break;
    }

    case HHCDisplayIndex:
    {
        uCommand = HH_DISPLAY_INDEX;
        if (dwData == 0)
            dwData = 0;
        break;
    }

    case HHCDisplaySearch:
    {
        uCommand = HH_DISPLAY_SEARCH;
        if (dwData == 0)
        {
            ZeroMemory(&query, sizeof(query));
            query.cbStruct = sizeof(query);
            dwData = (DWORD_PTR)&query;
        }
        break;
    }

    case HHCDisplayContext:
    {
        uCommand = HH_HELP_CONTEXT;
        break;
    }

    default:
    {
        TRACE_E("OpenHtmlHelp(): unknown command = " << command);
        return FALSE;
    }
    }

    if (helpFileName != NULL) // It's about the help plugin - to open the help window at the correct position +
    {                         // with remembered Favorites, it is necessary to open it for "salamand.chm" (subsequently
                              // open help for the plugin in the same window)
        lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
        if (SalPathAppend(helpPath, "salamand.chm", MAX_PATH) &&
            FileExists(helpPath))
        {
            HtmlHelp(NULL, helpPath, HH_DISPLAY_TOC, 0); // We ignore any potential errors
        }
    }

    BOOL ret = FALSE;

    lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
    if (SalPathAppend(helpPath, helpFileName == NULL ? "salamand.chm" : helpFileName, MAX_PATH) &&
        FileExists(helpPath))
    {
        if (HtmlHelp(NULL, helpPath, uCommand, dwData) == NULL)
        {
            BOOL errorHandled = FALSE;
            HH_LAST_ERROR lasterror;
            lasterror.cbStruct = sizeof(lasterror);
            if (HtmlHelp(NULL, NULL, HH_GET_LAST_ERROR, (DWORD_PTR)&lasterror) != NULL)
            {
                // Only report an error if we found one:
                if (FAILED(lasterror.hr))
                {
                    // Is there a text message to display...
                    if (lasterror.description)
                    {
                        if (!quiet)
                        {
                            char buff[5000];
                            // Convert the String to ANSI
                            WideCharToMultiByte(CP_ACP, 0, lasterror.description, -1, buff, 5000, NULL, NULL);
                            buff[5000 - 1] = 0;
                            SysFreeString(lasterror.description);

                            // Display
                            SalMessageBox(parent, buff, LoadStr(IDS_HELPERROR), MB_OK);
                        }
                        errorHandled = TRUE;
                    }
                }
            }
            if (!errorHandled && !quiet)
            {
                SalMessageBox(parent, LoadStr(IDS_FAILED_TO_LAUNCH_HELP),
                              LoadStr(IDS_HELPERROR), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        else
        {
            ret = TRUE;
        }
    }
    else
    {
        if (!quiet)
        {
            SalMessageBox(parent, LoadStr(IDS_FAILED_TO_FIND_HELP),
                          LoadStr(IDS_HELPERROR), MB_OK | MB_ICONEXCLAMATION);
        }
    }
    return ret;
}

//****************************************************************************
//
// CMWDropTarget
//
// is only used for dragging images
//

class CMWDropTarget : public IDropTarget
{
private:
    long RefCount; // object lifespan

public:
    CMWDropTarget()
    {
        RefCount = 1;
    }

    virtual ~CMWDropTarget()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of object");
    }

    STDMETHOD(QueryInterface)
    (REFIID refiid, void FAR* FAR* ppv)
    {
        if (refiid == IID_IUnknown || refiid == IID_IDropTarget)
        {
            *ppv = this;
            AddRef();
            return NOERROR;
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
    }

    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // We must not access the object, it no longer exists
        }
        return RefCount;
    }

    STDMETHOD(DragEnter)
    (IDataObject* pDataObject, DWORD grfKeyState,
     POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragEnter(pt.x, pt.y);
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHOD(DragOver)
    (DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragMove(pt.x, pt.y);
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHOD(DragLeave)
    ()
    {
        if (ImageDragging)
            ImageDragLeave();
        return E_UNEXPECTED;
    }

    STDMETHOD(Drop)
    (IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
     DWORD* pdwEffect)
    {
        *pdwEffect = DROPEFFECT_NONE;
        return E_UNEXPECTED;
    }
};

//
// ****************************************************************************
// MyShutdownBlockReasonCreate and MyShutdownBlockReasonDestroy
//
// Vista+: dynamically pull functions for setting / clearing reasons for shutdown blocking
//

BOOL MyShutdownBlockReasonCreate(HWND hWnd, LPCWSTR pwszReason)
{
    typedef BOOL(WINAPI * FT_ShutdownBlockReasonCreate)(HWND hWnd, LPCWSTR pwszReason);
    static FT_ShutdownBlockReasonCreate shutdownBlockReasonCreate = NULL;
    if (shutdownBlockReasonCreate == NULL && User32DLL != NULL && WindowsVistaAndLater)
    {
        shutdownBlockReasonCreate = (FT_ShutdownBlockReasonCreate)GetProcAddress(User32DLL,
                                                                                 "ShutdownBlockReasonCreate"); // Min: Vista
    }
    if (shutdownBlockReasonCreate != NULL)
        return shutdownBlockReasonCreate(hWnd, pwszReason);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL MyShutdownBlockReasonDestroy(HWND hWnd)
{
    typedef BOOL(WINAPI * FT_ShutdownBlockReasonDestroy)(HWND hWnd);
    static FT_ShutdownBlockReasonDestroy shutdownBlockReasonDestroy = NULL;
    if (shutdownBlockReasonDestroy == NULL && User32DLL != NULL && WindowsVistaAndLater)
    {
        shutdownBlockReasonDestroy = (FT_ShutdownBlockReasonDestroy)GetProcAddress(User32DLL,
                                                                                   "ShutdownBlockReasonDestroy"); // Min: Vista
    }
    if (shutdownBlockReasonDestroy != NULL)
        return shutdownBlockReasonDestroy(hWnd);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

//
// ****************************************************************************
// CMainWindow
//

VOID CALLBACK SkipOneARTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    SkipOneActivateRefresh = FALSE;
    KillTimer(hwnd, idEvent);
}

void CMainWindow::SafeHandleMenuNewMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult)
{
    __try
    {
        IContextMenu3* contextMenu3 = NULL;
        *plResult = 0;
        if (uMsg == WM_MENUCHAR)
        {
            if (SUCCEEDED(ContextMenuNew->GetMenu2()->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
            {
                contextMenu3->HandleMenuMsg2(uMsg, wParam, lParam, plResult);
                contextMenu3->Release();
                return;
            }
        }
        // The menu is destroyed directly from the menu it was attached to
        ContextMenuNew->GetMenu2()->HandleMenuMsg(uMsg, wParam, lParam); // this call falls here and there
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 11))
    {
        MenuNewExceptionHasOccured++;
        if (ContextMenuNew != NULL)
            ContextMenuNew->Release(); // Replacement for calling ReleaseMenuNew
                                       //    ReleaseMenuNew();
    }
}

void CMainWindow::PostChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CMainWindow::PostChangeOnPathNotification(%s, %d)", path, includingSubdirs);

    HANDLES(EnterCriticalSection(&DispachChangeNotifCS));

    // add this notification to the array (for later processing)
    CChangeNotifData data;
    lstrcpyn(data.Path, path, MAX_PATH);
    data.IncludingSubdirs = includingSubdirs;
    ChangeNotifArray.Add(data);
    if (!ChangeNotifArray.IsGood())
        ChangeNotifArray.ResetState(); // we ignore errors (at worst we won't refresh)

    // Request sending messages about road changes
    HANDLES(EnterCriticalSection(&TimeCounterSection));
    int t1 = MyTimeCounter++;
    HANDLES(LeaveCriticalSection(&TimeCounterSection));
    PostMessage(HWindow, WM_USER_DISPACHCHANGENOTIF, 0, t1);

    HANDLES(LeaveCriticalSection(&DispachChangeNotifCS));
}

void CMainWindowWindowProcAux(IContextMenu* menu2, CMINVOKECOMMANDINFO& ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        menu2->InvokeCommand(&ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 12))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void BroadcastConfigChanged()
{
    // Internal Viewer and Find: restore all windows (e.g. after changing global fonts)
    ViewerWindowQueue.BroadcastMessage(WM_USER_CFGCHANGED, 0, 0);
    FindDialogQueue.BroadcastMessage(WM_USER_CFGCHANGED, 0, 0);
}

void CMainWindow::FillViewModeMenu(CMenuPopup* popup, int firstIndex, int type)
{
    char buff[VIEW_NAME_MAX + 10];

    DWORD fistCMID;
    CFilesWindow* panel;

    switch (type)
    {
    case 0:
    {
        fistCMID = CM_ACTIVEMODE_1;
        panel = GetActivePanel();
        break;
    }

    case 1:
    {
        fistCMID = CM_LEFTMODE_1;
        panel = LeftPanel;
        break;
    }

    case 2:
    {
        fistCMID = CM_RIGHTMODE_1;
        panel = RightPanel;
        break;
    }

    default:
    {
        TRACE_E("Uknown type=" << type);
        return;
    }
    }

    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_STATE |
               MENU_MASK_ID /*| MENU_MASK_SKILLLEVEL*/;
    mii.Type = MENU_TYPE_STRING | MENU_TYPE_RADIOCHECK;
    mii.String = buff;
    int i;
    for (i = 0; i < VIEW_TEMPLATES_COUNT; i++)
    {
        if (i == 0) // We are not displaying the tree at the moment
            continue;

        CViewTemplate* tmpl = &ViewTemplates.Items[i];
        if (tmpl->Name[0] != 0)
        {
            sprintf(buff, "%s\tAlt+%d", tmpl->Name, i < VIEW_TEMPLATES_COUNT - 1 ? i + 1 : 0);

            mii.ID = fistCMID + i;

            //      mii.SkillLevel = MENU_LEVEL_INTERMEDIATE | MENU_LEVEL_ADVANCED;
            //      if (i > 2)
            //        mii.SkillLevel |= MENU_LEVEL_BEGINNER;

            mii.State = panel->ViewTemplate == tmpl ? MENU_STATE_CHECKED : 0;

            popup->InsertItem(firstIndex, TRUE, &mii);
            firstIndex++;
        }
    }
}

void CMainWindow::SetDoNotLoadAnyPlugins(BOOL doNotLoad)
{
    if (doNotLoad)
    {
        DoNotLoadAnyPlugins = TRUE;
    }
    else
    {
        DoNotLoadAnyPlugins = FALSE;
        if (!CriticalShutdown)
        {
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            int t2 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));

            if (LeftPanel->GetViewMode() == vmThumbnails)
            {
                PostMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1); // we will take care of the new icon-cache filling (thumbnails are possible again)
            }
            if (RightPanel->GetViewMode() == vmThumbnails)
            {
                PostMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2); // we will take care of the new icon-cache filling (thumbnails are possible again)
            }
        }
    }
}

void CMainWindow::ShowHideTwoDriveBarsInternal(BOOL show)
{
    LockWindowUpdate(HWindow);

    if (show)
    {
        REBARBANDINFO rbi;
        rbi.cbSize = sizeof(REBARBANDINFO);

        int count = (int)SendMessage(HTopRebar, RB_GETBANDCOUNT, 0, 0);
        // drive bar 1
        int index = (int)SendMessage(HTopRebar, RB_IDTOINDEX, BANDID_DRIVEBAR, 0);
        SendMessage(HTopRebar, RB_MOVEBAND, (WPARAM)index, (LPARAM)count - 1);
        rbi.fMask = RBBIM_STYLE;
        rbi.fStyle = RBBS_NOGRIPPER | RBBS_BREAK;
        SendMessage(HTopRebar, RB_SETBANDINFO, count - 1, (LPARAM)&rbi);

        // drive bar 2
        index = (int)SendMessage(HTopRebar, RB_IDTOINDEX, BANDID_DRIVEBAR2, 0);
        SendMessage(HTopRebar, RB_MOVEBAND, (WPARAM)index, (LPARAM)count - 1);
        rbi.fMask = RBBIM_STYLE;
        rbi.fStyle = RBBS_NOGRIPPER;
        SendMessage(HTopRebar, RB_SETBANDINFO, count - 1, (LPARAM)&rbi);
    }
    else
    {
        int index = (int)SendMessage(HTopRebar, RB_IDTOINDEX, BANDID_DRIVEBAR, 0);
        SendMessage(HTopRebar, RB_SHOWBAND, index, FALSE);

        index = (int)SendMessage(HTopRebar, RB_IDTOINDEX, BANDID_DRIVEBAR2, 0);
        SendMessage(HTopRebar, RB_SHOWBAND, index, FALSE);
    }

    LockWindowUpdate(NULL);
}

int CMainWindow::GetSplitBarWidth()
{
    if (MiddleToolBar != NULL && MiddleToolBar->HWindow != NULL)
        return 2 * SPLIT_LINE_WIDTH + MiddleToolBar->GetNeededWidth();
    else
        return SPLIT_LINE_WIDTH;
}

BOOL CMainWindow::IsPanelZoomed(BOOL leftPanel)
{
    if (leftPanel)
        return SplitPosition >= 0.99;
    else
        return SplitPosition <= 0.01;
}

void CMainWindow::ToggleSmartColumnMode(CFilesWindow* panel)
{
    if (panel->GetViewMode() == vmDetailed) // panel must run in detailed mode
    {
        if (panel->Columns.Count < 1)
            return;
        CColumn* column = &panel->Columns[0];
        BOOL leftPanel = (panel == LeftPanel);
        BOOL smartMode = !(!column->FixedWidth &&
                           (leftPanel && panel->ViewTemplate->LeftSmartMode ||
                            !leftPanel && panel->ViewTemplate->RightSmartMode));
        if (smartMode && column->FixedWidth)
        { // smart mode is only for elastic columns (we need to change it in the view template)
            if (leftPanel)
                panel->ViewTemplate->Columns[0].LeftFixedWidth = 0;
            else
                panel->ViewTemplate->Columns[0].RightFixedWidth = 0;
        }
        if (leftPanel)
        {
            panel->ViewTemplate->LeftSmartMode = smartMode;
            LeftPanel->SelectViewTemplate(LeftPanel->GetViewTemplateIndex(), TRUE, FALSE, VALID_DATA_ALL, TRUE);
        }
        else
        {
            panel->ViewTemplate->RightSmartMode = smartMode;
            RightPanel->SelectViewTemplate(RightPanel->GetViewTemplateIndex(), TRUE, FALSE, VALID_DATA_ALL, TRUE);
        }
    }
}

BOOL CMainWindow::GetSmartColumnMode(CFilesWindow* panel)
{
    if (panel->Columns.Count < 1)
        return FALSE;
    CColumn* column = &panel->Columns[0];
    BOOL smartMode = (!column->FixedWidth &&
                      (panel == LeftPanel && panel->ViewTemplate->LeftSmartMode ||
                       panel == RightPanel && panel->ViewTemplate->RightSmartMode));
    return smartMode;
}

void CMainWindow::SafeHandleMenuChngDrvMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult)
{
    CALL_STACK_MESSAGE_NONE
    __try
    {
        IContextMenu3* contextMenu3 = NULL;
        *plResult = 0;
        if (uMsg == WM_MENUCHAR)
        {
            if (SUCCEEDED(ContextMenuChngDrv->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
            {
                contextMenu3->HandleMenuMsg2(uMsg, wParam, lParam, plResult);
                contextMenu3->Release();
                return;
            }
        }
        ContextMenuChngDrv->HandleMenuMsg(uMsg, wParam, lParam);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 3))
    {
    }
}

void CMainWindow::ApplyCommandLineParams(const CCommandLineParams* cmdLineParams, BOOL setActivePanelAndPanelPaths)
{
    if (setActivePanelAndPanelPaths)
    {
        // first we will set the active panel
        if (cmdLineParams->ActivatePanel == 1 && GetActivePanel() == RightPanel ||
            cmdLineParams->ActivatePanel == 2 && GetActivePanel() == LeftPanel)
        {
            ChangePanel(FALSE);
        }
        // then we can set the path in the active panel
        if (cmdLineParams->LeftPath[0] == 0 && cmdLineParams->RightPath[0] == 0 && cmdLineParams->ActivePath[0] != 0)
            GetActivePanel()->ChangeDir(cmdLineParams->ActivePath); // It doesn't make sense to combine with setting the left/right panel
        else
        {
            if (cmdLineParams->LeftPath[0] != 0)
                LeftPanel->ChangeDir(cmdLineParams->LeftPath);
            if (cmdLineParams->RightPath[0] != 0)
                RightPanel->ChangeDir(cmdLineParams->RightPath);
        }
    }

    if (cmdLineParams->SetMainWindowIconIndex)
    {
        Configuration.MainWindowIconIndexForced = cmdLineParams->MainWindowIconIndex;
        SetWindowIcon();
    }
    if (cmdLineParams->SetTitlePrefix)
    {
        Configuration.UseTitleBarPrefixForced = TRUE;
        lstrcpyn(Configuration.TitleBarPrefixForced, cmdLineParams->TitlePrefix, TITLE_PREFIX_MAX);
        SetWindowTitle();
    }
}

BOOL CMainWindow::SHChangeNotifyInitialize()
{
    if (SHChangeNotifyRegisterID != 0)
    {
        TRACE_E("SHChangeNotifyRegisterID != 0");
        return FALSE;
    }

    LPITEMIDLIST pidl;
    if (!SUCCEEDED(SHGetSpecialFolderLocation(HWindow, CSIDL_DESKTOP, &pidl)))
    {
        TRACE_E("SHGetSpecialFolderLocation failed on CSIDL_DESKTOP");
        return FALSE;
    }

    SHChangeNotifyEntry entry;
    entry.pidl = pidl;
    entry.fRecursive = TRUE;

    // message WM_USER_SHCHANGENOTIFY, which will be delivered to us during notifications, exceeds the boundaries of the process
    // By defining the constant SHCNRF_NewDelivery (also known as SHCNF_NO_PROXY), we indicate that we are taking responsibility
    // for accessing memory passed by message (using SHChangeNotification_Lock) and that the OS should not create
    // proxy windows (note: there is a bug reported under XP where the proxy window is created but not destroyed):
    // http://groups.google.com/groups?selm=3CDFD449.6BA0CDB4%40ic.ac.uk&output=gplain
    //
    // via SHCNE_ASSOCCHANGED we will receive information about the change of association
    SHChangeNotifyRegisterID = SHChangeNotifyRegister(HWindow, SHCNRF_ShellLevel | SHCNRF_NewDelivery,
                                                      SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED | SHCNE_DRIVEREMOVED |
                                                          SHCNE_DRIVEADD | SHCNE_NETSHARE | SHCNE_NETUNSHARE |
                                                          SHCNE_DRIVEADDGUI | SHCNE_ASSOCCHANGED | SHCNE_UPDATEITEM,
                                                      WM_USER_SHCHANGENOTIFY,
                                                      1, &entry);

    // deallocation of pidl
    IMalloc* alloc;
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        alloc->Free(pidl);
        alloc->Release();
    }

    return TRUE;
}

BOOL CMainWindow::SHChangeNotifyRelease()
{
    if (SHChangeNotifyRegisterID != 0)
    {
        SHChangeNotifyDeregister(SHChangeNotifyRegisterID);
        SHChangeNotifyRegisterID = 0;
    }
    return TRUE;
}

typedef WINSHELLAPI BOOL(WINAPI* FT_FileIconInit)(
    BOOL bFullInit);

BOOL CMainWindow::OnAssociationsChangedNotification(BOOL showWaitWnd)
{
    // we will play with the sizes of the icons

    LoadSaveToRegistryMutex.Enter(); // Icons have been reduced for people, see https://forum.altap.cz/viewtopic.php?t=638
    // With this synchronization, we ensure that two Salamanders do not crawl into each other's cabbage
    // Unfortunately, the trick with changing the "Shell Icon Size" for rebuilding the cache is used by many (including Tweak UI),
    // so if they refresh at the same time as Salamander, there will be a conflict
    // In this situation, we are trying to prevent the postponement of the following nonsense using IDT_ASSOCIATIONSCHNG

    HKEY hKey;
    if (HANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", 0, KEY_READ | KEY_WRITE, &hKey)) == ERROR_SUCCESS)
    {
        // Older SHELL32.DLL versions may not have this export and fileIconInit will be NULL
        FT_FileIconInit fileIconInit = NULL;
        fileIconInit = (FT_FileIconInit)GetProcAddress(Shell32DLL, MAKEINTRESOURCE(660)); // no header

        char size[50];
        BOOL deleteVal = FALSE;
        if (!GetValueAux(NULL, hKey, "Shell Icon Size", REG_SZ, size, 50))
        {
            // The values for the icon size are Shell Icon Size and
            // Shell Small Icon Size (both are stored as strings - not
            // DWORDs). You only need to change one of them to cause
            // the refresh to happen (typically the large icon size). If those
            // values don't exist, the shell uses the SM_CXICON metric
            // (GetSystemMetrics) as the default size for large icons, and
            // half of that for the small icon size. If you're trying to cause
            // a refresh and the registry entry doesn't exist, you can just
            // assume that the size is set to SM_CXICON.
            sprintf(size, "%d", GetSystemMetrics(SM_CXICON));
            deleteVal = TRUE;
        }
        int val = atoi(size);
        if (val > 0) // Unfortunately, people (according to the internet) randomly set the size of icons (72, 96, 128, etc.), so it is not possible to filter out "strange sizes"
        {
            IgnoreWM_SETTINGCHANGE = TRUE;

            sprintf(size, "%d", val - 1);
            SetValueAux(NULL, hKey, "Shell Icon Size", REG_SZ, size, -1);
            SendMessage(MainWindow->HWindow, WM_SETTINGCHANGE, SPI_SETICONMETRICS, (LPARAM) "WindowMetrics");
            if (fileIconInit != NULL)
                fileIconInit(FALSE);
            sprintf(size, "%d", val);
            SetValueAux(NULL, hKey, "Shell Icon Size", REG_SZ, size, -1);
            SendMessage(MainWindow->HWindow, WM_SETTINGCHANGE, SPI_SETICONMETRICS, (LPARAM) "WindowMetrics");
            if (fileIconInit != NULL)
                fileIconInit(TRUE);
            if (deleteVal)
                RegDeleteValue(hKey, "Shell Icon Size"); // clean up after ourselves
            HANDLES(RegCloseKey(hKey));

            IgnoreWM_SETTINGCHANGE = FALSE;
        }
    }

    LoadSaveToRegistryMutex.Leave();

    /*    if (fileIconInit != NULL)
    fileIconInit(TRUE);

  // debugging display of the icon
  SHFILEINFO shi;
  HIMAGELIST systemIL = (HIMAGELIST)SHGetFileInfo("C:\\TEST.QWE", 0, &shi, sizeof(shi),
                                       SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE);
  TRACE_I("systemIL="<<hex<<systemIL <<" index="<<dec<<shi.iIcon);
  if (systemIL != NULL)
  {
    HDC hDC = GetWindowDC(MainWindow->HWindow);
    ImageList_Draw(systemIL, shi.iIcon, hDC, 0, 0, ILD_NORMAL);
    ImageList_Draw(systemIL, shi.iIcon, hDC, 0, 25, ILD_NORMAL);
    ReleaseDC(MainWindow->HWindow, hDC);
  }*/

    // own refresh association
    BOOL lCanDrawItems = LeftPanel->CanDrawItems;
    LeftPanel->CanDrawItems = FALSE;
    BOOL rCanDrawItems = RightPanel->CanDrawItems;
    RightPanel->CanDrawItems = FALSE;
    Associations.Release();
    Associations.ReadAssociations(showWaitWnd);
    LeftPanel->CanDrawItems = lCanDrawItems;
    RightPanel->CanDrawItems = rCanDrawItems;
    HANDLES(EnterCriticalSection(&TimeCounterSection));
    int t1 = MyTimeCounter++;
    int t2 = MyTimeCounter++;
    HANDLES(LeaveCriticalSection(&TimeCounterSection));
    SendMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
    SendMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2);

    return TRUE;
}

void CMainWindow::RebuildDriveBarsIfNeeded(BOOL useDrivesMask, DWORD drivesMask, BOOL checkCloudStorages,
                                           DWORD cloudStoragesMask)
{
    if ((DriveBar != NULL && DriveBar->HWindow != NULL) || (DriveBar2 != NULL && DriveBar2->HWindow != NULL))
    {
        if (!useDrivesMask)
        {
            DWORD netDrives; // bit array of disk network
            GetNetworkDrives(netDrives, NULL);
            drivesMask = GetLogicalDrives() | netDrives;
        }

        CDriveBar* copyDrivesListFrom = NULL;
        if (DriveBar != NULL && DriveBar->HWindow != NULL)
        {
            if (DriveBar->GetCachedDrivesMask() != drivesMask ||
                checkCloudStorages && DriveBar->GetCachedCloudStoragesMask() != cloudStoragesMask)
            {
                // Notifications about disk changes or changes in cloud storage availability are not working, we will rebuild the drive bar "manually"
                TRACE_I("Forced drives rebuild for DriveBar!");
                DriveBar->RebuildDrives();
                copyDrivesListFrom = DriveBar;
            }
        }
        if (DriveBar2 != NULL && DriveBar2->HWindow != NULL)
        {
            if (DriveBar2->GetCachedDrivesMask() != drivesMask ||
                checkCloudStorages && DriveBar2->GetCachedCloudStoragesMask() != cloudStoragesMask)
            {
                // Notifications about disk changes or changes in cloud storage availability are not working, we will rebuild the drive bar "manually"
                TRACE_I("Forced drives rebuild for DriveBar2!");
                DriveBar2->RebuildDrives(copyDrivesListFrom);
            }
        }
    }
}

LRESULT
CMainWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CMainWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        SHChangeNotifyInitialize(); // we will have Shell Notifications delivered

        SetTimer(HWindow, IDT_ADDNEWMODULES, 15000, NULL); // timer after 15 seconds for AddNewlyLoadedModulesToGlobalModulesStore()

        CMWDropTarget* dropTarget = new CMWDropTarget();
        if (dropTarget != NULL)
        {
            HANDLES(RegisterDragDrop(HWindow, dropTarget));
            dropTarget->Release(); // RegisterDragDrop called AddRef()
        }

        HMENU h = GetSystemMenu(HWindow, FALSE);
        if (h != NULL)
        {
            int items = GetMenuItemCount(h);
            int pos = items; // we will append new items to the end of the list

            // if the last two items in the menu are separator and Close, we insert ourselves above them
            // (users have long complained that they accidentally click on our AOT instead of the desired Close)
            if (items > 2)
            {
                UINT predLastCmd = GetMenuItemID(h, items - 2);
                UINT lastCmd = GetMenuItemID(h, items - 1);
                if (predLastCmd == 0 && lastCmd == SC_CLOSE)
                    pos = items - 2;
            }

            /* used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() call below...
MENU_TEMPLATE_ITEM AddToSystemMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_ALWAYSONTOP
  {MNTT_PE, 0
};*/
            InsertMenu(h, pos, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            InsertMenu(h, pos + 1, MF_BYPOSITION | MF_STRING | MF_ENABLED | (Configuration.AlwaysOnTop ? MF_CHECKED : MF_UNCHECKED),
                       CM_ALWAYSONTOP, LoadStr(IDS_ALWAYSONTOP));
        }
        SetWindowPos(HWindow,
                     Configuration.AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        HTopRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, "",
                                   WS_VISIBLE | WS_BORDER | WS_CHILD |
                                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                                       RBS_VARHEIGHT | CCS_NODIVIDER |
                                       RBS_BANDBORDERS | CCS_NOPARENTALIGN |
                                       RBS_AUTOSIZE,
                                   0, 0, 0, 0, // dummy
                                   HWindow, (HMENU)0, HInstance, NULL);
        if (HTopRebar == NULL)
        {
            TRACE_E("CreateWindowEx on " << REBARCLASSNAME);
            return -1;
        }

        // we do not want visual styles for rebar
        // disable them
        SetWindowTheme(HTopRebar, (L" "), (L" "));

        // force the WS_BORDER that got "lost" somewhere
        DWORD style = (DWORD)GetWindowLongPtr(HTopRebar, GWL_STYLE);
        style |= WS_BORDER;
        SetWindowLongPtr(HTopRebar, GWL_STYLE, style);

        MenuBar = new CMenuBar(&MainMenu, HWindow);
        if (MenuBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        if (!MenuBar->CreateWnd(HTopRebar))
            return -1;

        LeftPanel = new CFilesWindow(this);
        if (LeftPanel == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        if (!LeftPanel->Create(CWINDOW_CLASSNAME2, "",
                               WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                               0, 0, 0, 0,
                               HWindow,
                               NULL,
                               HInstance,
                               LeftPanel))
        {
            TRACE_E("LeftPanel->Create failed");
            return -1;
        }
        SetActivePanel(LeftPanel);
        //      ReleaseMenuNew();
        RightPanel = new CFilesWindow(this);
        if (RightPanel == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        if (!RightPanel->Create(CWINDOW_CLASSNAME2, "",
                                WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                0, 0, 0, 0,
                                HWindow,
                                NULL,
                                HInstance,
                                RightPanel))
        {
            TRACE_E("RightPanel->Create failed");
            return -1;
        }

        EditWindow = new CEditWindow;
        if (EditWindow == NULL || !EditWindow->IsGood())
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }

        TopToolBar = new CMainToolBar(HWindow, mtbtTop);
        if (TopToolBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        TopToolBar->SetImageList(HGrayToolBarImageList);
        TopToolBar->SetHotImageList(HHotToolBarImageList);
        TopToolBar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_ADJUSTABLE);
        TOOLBAR_PADDING padding;
        TopToolBar->GetPadding(&padding);
        padding.ToolBarVertical = 1;
        padding.IconLeft = 2;
        padding.IconRight = 3;
        TopToolBar->SetPadding(&padding);

        MiddleToolBar = new CMainToolBar(HWindow, mtbtMiddle);
        if (MiddleToolBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        MiddleToolBar->SetImageList(HGrayToolBarImageList);
        MiddleToolBar->SetHotImageList(HHotToolBarImageList);
        MiddleToolBar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_ADJUSTABLE | TLB_STYLE_VERTICAL);
        MiddleToolBar->GetPadding(&padding);
        padding.ToolBarVertical = 1;
        padding.IconLeft = 2;
        padding.IconRight = 3;
        MiddleToolBar->SetPadding(&padding);

        PluginsBar = new CPluginsBar(HWindow);
        if (PluginsBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }

        //      AnimateBar = new CAnimate(HWorkerBitmap, 50, 0, RGB(255, 255, 255)); // a total of 50 frames, starting from 0 in the loop, white background
        //      AnimateBar = new CAnimate(HWorkerBitmap, 43, 3, RGB(0, 0, 0)); // a total of 43 frames, starting from 3 in the loop, black background
        //      if (AnimateBar == NULL)
        //      {
        //        TRACE_E(LOW_MEMORY);
        //        return -1;
        //      }
        //      if (!AnimateBar->IsGood())
        //        return -1;

        // User Menu Bar
        UMToolBar = new CUserMenuBar(HWindow);
        if (UMToolBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        UMToolBar->GetPadding(&padding);
        padding.IconLeft = 2;
        padding.IconRight = 3;
        padding.ButtonIconText = 2;
        padding.TextRight = 4;
        UMToolBar->SetPadding(&padding);

        // Hot Path Bar
        HPToolBar = new CHotPathsBar(HWindow);
        if (HPToolBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        HPToolBar->GetPadding(&padding);
        padding.IconLeft = 2;
        padding.IconRight = 3;
        padding.ButtonIconText = 2;
        padding.TextRight = 4;
        HPToolBar->SetPadding(&padding);

        // Drive Bar
        DriveBar = new CDriveBar(HWindow);
        if (DriveBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        DriveBar2 = new CDriveBar(HWindow);
        if (DriveBar2 == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }

        BottomToolBar = new CBottomToolBar(HWindow);
        if (BottomToolBar == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return -1;
        }
        BottomToolBar->SetImageList(HBottomTBImageList);
        BottomToolBar->SetHotImageList(HHotBottomTBImageList);

        TaskbarRestartMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));

        Created = TRUE;
        return 0;
    }

    // case WM_CHANGEUISTATE: // it seems that both messages are always sent
    case WM_UPDATEUISTATE:
    {
        if (MenuBar != NULL && MenuBar->HWindow != NULL)
            SendMessage(MenuBar->HWindow, WM_UPDATEUISTATE, wParam, lParam);
        // TRACE_I("KeyboardCuesAlwaysVisible="<<std::hex<<KeyboardCuesAlwaysVisible );
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        UserMenuIconBkgndReader.SetSysColorsChanged();

        // we will propagate information about color change to the rebar
        if (HTopRebar != NULL)
            SendMessage(HTopRebar, uMsg, wParam, lParam);

        // there could have been a change in color depth - let's rebuild the imagelists and get
        // new icons
        ColorsChanged(TRUE, FALSE, TRUE); // let's rebuild everything; there is plenty of time
        return 0;
    }

    case WM_SETTINGCHANGE:
    {
        if (IgnoreWM_SETTINGCHANGE || LeftPanel == NULL || RightPanel == NULL) // There was a bug report where it is visible that WM_SETTINGCHANGE was delivered right from WM_CREATE of the main window (the panel did not exist yet, so it crashed on accessing NULL)
            return 0;

        // Detection based on EXPLORER.EXE under NT4
        if (lParam != 0 && stricmp((LPCTSTR)lParam, "Environment") == 0)
        {
            // Environment variables have changed, let's refresh them
            if (Configuration.ReloadEnvVariables)
                RegenEnvironmentVariables();
            return 0;
        }
        if (lParam != 0 && stricmp((LPCTSTR)lParam, "Extensions") == 0)
        {
            // Association has changed, let's refresh them
            //
            // this path is probably no longer used, it's some old branch, now
            // information is sent via SHCNE_ASSOCCHANGED, but in NT4 EXPLORER
            // they have caught this branch as well

            // Let's delay by a second to avoid colliding with other software using the icon size change for the reset icon cache
            if (!SetTimer(HWindow, IDT_ASSOCIATIONSCHNG, 1000, NULL))
                OnAssociationsChangedNotification(FALSE);
            return 0;
        }

        // unknown change, we will rebuild everything

        GotMouseWheelScrollLines = FALSE; //Let's reload the number of lines to scroll again
        InitLocales();
        SetFont(); // the panel font is normally controlled by the system font
        SetEnvFont();

        GetShortcutOverlay();
        // Internal Viewer and Find: restore all windows (the font has already changed)
        BroadcastConfigChanged();
        if (!IsIconic(HWindow))
        {
            // we will ensure the layout of child windows - for example, the dimensions of the toolbar may change
            RECT wr;
            GetWindowRect(HWindow, &wr);
            int width = wr.right - wr.left;
            int height = wr.bottom - wr.top;
            SetWindowPos(HWindow, NULL, 0, 0, width + 1, height + 1,
                         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
            SetWindowPos(HWindow, NULL, 0, 0, width, height,
                         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
        }
        LeftPanel->RefreshListBox(-1, -1, LeftPanel->FocusedIndex, FALSE, FALSE);
        RightPanel->RefreshListBox(-1, -1, RightPanel->FocusedIndex, FALSE, FALSE);
        RefreshDiskFreeSpace();

        // doslo ke zmene fontu, dame pluginum vedet, ze maji toolbaram a menubaram zavolat SetFont()
        Plugins.Event(PLUGINEVENT_SETTINGCHANGE, 0);

        return 0;
    }

    case WM_USER_SHCHANGENOTIFY: // we receive thanks to SHChangeNotifyRegister
    {
        LONG wEventId;
        HANDLE hLock = NULL;

        //      TRACE_E("WM_USER_SHCHANGENOTIFY lParam="<<hex<<lParam<<" wParam="<<hex<<wParam);

        // Under newer shell32.dll, it is necessary to request access to mapped memory where the passed parameters are located
        // (memory cannot be passed between processes and this message came from Explorer)
        // see doc\interesting.zip\Shell Notifications.mht (http://www.geocities.com/SiliconValley/4942/notify.html)

        LPITEMIDLIST* ppidl;
        hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &ppidl, &wEventId); // FIXME_X64 - verify typecasting to (DWORD)
        if (hLock == NULL)
        {
            TRACE_E("SHChangeNotification_Lock failed");
            break;
        }

        // convert path -> route
        char szPath[2 * MAX_PATH];
        szPath[0] = 0; // an empty road means a change of everything
        if (ppidl != NULL)
        {
            switch (wEventId)
            {
            case SHCNE_CREATE:
            case SHCNE_DELETE:
            case SHCNE_MKDIR:
            case SHCNE_RMDIR:
            case SHCNE_MEDIAINSERTED:
            case SHCNE_MEDIAREMOVED:
            case SHCNE_DRIVEREMOVED:
            case SHCNE_DRIVEADD:
            case SHCNE_NETSHARE:
            case SHCNE_NETUNSHARE:
            case SHCNE_ATTRIBUTES:
            case SHCNE_UPDATEDIR:
            case SHCNE_UPDATEITEM:
            case SHCNE_SERVERDISCONNECT:
            case SHCNE_DRIVEADDGUI:
            case SHCNE_EXTENDED_EVENT:
            {
                if (!SHGetPathFromIDList(ppidl[0], szPath))
                    szPath[0] = 0;
                break;
            }
            }
        }
        SHChangeNotification_Unlock(hLock); // ppidl is translated, we can release the memory
        ppidl = NULL;

        if (wEventId == SHCNE_UPDATEITEM)
        {
            //        TRACE_I("SHCNE_UPDATEITEM: " << szPath);
            if (LeftPanel != NULL && RightPanel != NULL)
            {
                LeftPanel->IconOverlaysChangedOnPath(szPath);
                RightPanel->IconOverlaysChangedOnPath(szPath);
                if (CutDirectory(szPath))
                {
                    LeftPanel->IconOverlaysChangedOnPath(szPath);
                    RightPanel->IconOverlaysChangedOnPath(szPath);
                }
            }
        }
        else
        {
            if (wEventId == SHCNE_ASSOCCHANGED)
            {
                // change in associations
                // Let's delay by a second to avoid colliding with other software using the icon size change for the reset icon cache
                if (!SetTimer(HWindow, IDT_ASSOCIATIONSCHNG, 1000, NULL))
                    OnAssociationsChangedNotification(FALSE);
            }
            else
            {
                // change in media or discs

                // After inserting the media, we will automatically perform a Retry in the "drive not ready" messagebox.
                // (if displayed for a drive with inserted media)
                if (wEventId == SHCNE_MEDIAINSERTED)
                {
                    if (CheckPathRootWithRetryMsgBox[0] != 0 &&
                        HasTheSameRootPath(CheckPathRootWithRetryMsgBox, szPath))
                    {
                        if (LastDriveSelectErrDlgHWnd != NULL)
                            PostMessage(LastDriveSelectErrDlgHWnd, WM_COMMAND, IDRETRY, 0);
                    }
                }

                // if the Alt+F1/F2 menu is open, we refresh (read the volume name)
                CFilesWindow* panel = GetActivePanel();
                if (panel != NULL)
                    PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

                // if there are CD-ROM or removable media in the drives, we will refresh them
                while (1)
                {
                    if ((panel->Is(ptDisk) || panel->Is(ptZIPArchive)) && !IsUNCPath(panel->GetPath()))
                    {
                        UINT type = MyGetDriveType(panel->GetPath());
                        if (type == DRIVE_CDROM || type == DRIVE_REMOVABLE)
                        {
                            HANDLES(EnterCriticalSection(&TimeCounterSection)); // we take the time when a refresh is needed
                            int t1 = MyTimeCounter++;
                            HANDLES(LeaveCriticalSection(&TimeCounterSection));
                            PostMessage(panel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
                        }
                        if (type == DRIVE_NO_ROOT_DIR) // device disappeared (drive is invalid)
                        {
                            if (LeftPanel == panel)
                            {
                                if (!ChangeLeftPanelToFixedWhenIdleInProgress)
                                    ChangeLeftPanelToFixedWhenIdle = TRUE;
                            }
                            else
                            {
                                if (!ChangeRightPanelToFixedWhenIdleInProgress)
                                    ChangeRightPanelToFixedWhenIdle = TRUE;
                            }
                        }
                    }
                    if (panel != GetNonActivePanel())
                        panel = GetNonActivePanel();
                    else
                        break;
                }
            }
        }
        break;
    }

        /*      // WM_DEVICECHANGE did not work well, for example under Win XP when connecting a DSC F707 camera
    // it provided information about the device being connected, but subsequent detection of the device name
    // (if the Alt+F1/2 menu was displayed) using SHGetFileInfo returned an empty string.
    // I found a thread on Google where a man complained about the same problem
    //
    // http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&threadm=99a435fa.0203280715.69a286a8%40posting.
    // google.com&rnum=1&prev=/groups%3Fhl%3Den%26lr%3D%26ie%3DUTF-8%26oe%3DUTF-8%26q%3Ddevice%2Bname%2Bshgetfileinfo
    //
    // and he solved it by waiting. People recommended him to move away from WM_DEVICECHANGE and switch to
    // the undocumented function SHChangeNotifyRegister...
    // (http://www.geocities.com/SiliconValley/4942/notify.html)
    case WM_DEVICECHANGE:
    {
      if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE ||
          wParam == DBT_CONFIGCHANGED)  // CD-ROM media exchange
      {
        // if the Alt+F1/F2 menu is open, we refresh it (reading volume name)
        CFilesWindow *panel = GetActivePanel();
        if (panel != NULL)
          PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

        // if CD-ROM or removable media are in the panels, we refresh them
        while (1)
        {
          if (panel->Is(ptDisk) || panel->Is(ptZIPArchive))
          {
            UINT type = MyGetDriveType(panel->GetPath());
            if (type == DRIVE_CDROM || type == DRIVE_REMOVABLE)
            {
              HANDLES(EnterCriticalSection(&TimeCounterSection));  // we take note of the time when a refresh is needed
              int t1 = MyTimeCounter++;
              HANDLES(LeaveCriticalSection(&TimeCounterSection));
              PostMessage(panel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            }
          }
          if (panel != GetNonActivePanel()) panel = GetNonActivePanel();
          else break;
        }
      }
      break;
    }*/

    case WM_USER_PROCESSDELETEMAN:
    {
        // Postponing data processing due to activating the main window after pressing ESC in the viewer under WinXP (without these
        // It didn't manage to monkey around - the main window wasn't active -> the safe-wait-window didn't show up)
        if (!SetTimer(HWindow, IDT_DELETEMNGR_PROCESS, 200, NULL))
            DeleteManager.ProcessData(); // if the timer doesn't work out, we'll do it right away, we don't care about WinXP
        return 0;
    }

    case WM_USER_DRIVES_CHANGE:
    {
        CFilesWindow* panel = GetActivePanel();
        if (panel->OpenedDrivesList != NULL)
        {
            // let's rebuild the menu
            panel->OpenedDrivesList->RebuildMenu();
        }
        CDriveBar* copyDrivesListFrom = NULL;
        if (DriveBar != NULL && DriveBar->HWindow != NULL)
        {
            DriveBar->RebuildDrives();
            copyDrivesListFrom = DriveBar;
        }
        if (DriveBar2 != NULL && DriveBar2->HWindow != NULL)
            DriveBar2->RebuildDrives(copyDrivesListFrom);
        return 0;
    }

    case WM_USER_ENTERMENULOOP:
    case WM_USER_LEAVEMENULOOP:
    {
        // Hide any tooltip
        SetCurrentToolTip(NULL, 0);

        // if someone is monitoring the mouse, I will stop monitoring
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_QUERY;
        if (TrackMouseEvent(&tme) && tme.hwndTrack != NULL)
            SendMessage(tme.hwndTrack, WM_MOUSELEAVE, 0, 0);

        // Let the existing caret fade out (and then reappear) to not disturb the user
        CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        if (EditMode)
        {
            if (uMsg == WM_USER_ENTERMENULOOP)
                EditWindow->HideCaret();
            else
                EditWindow->ShowCaret();
        }

        if (uMsg == WM_USER_ENTERMENULOOP)
            UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
        else
            UserMenuIconBkgndReader.EndUserMenuIconsInUse();

        // Ensure correct settings of enablers so that enabled items in the menu correspond
        // to the actual state. It also sets the state of the bottom toolbars.
        OnEnterIdle();
        return 0;
    }

    case WM_USER_TBDROPDOWN:
    {
        CToolBar* tlb = (CToolBar*)WindowsManager.GetWindowPtr((HWND)wParam);
        if (tlb == NULL)
            return 0;
        int index = (int)lParam;
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_ID;
        if (!tlb->GetItemInfo2(index, TRUE, &tii))
            return 0;

        DWORD id = tii.ID;

        RECT r;
        tlb->GetItemRect(index, r);

        switch (id)
        {
        case CM_LCHANGEDRIVE:
        case CM_RCHANGEDRIVE:
        {
            SendMessage(HWindow, WM_COMMAND, id, 0);
            break;
        }

        case CM_OPENHOTPATHSDROP:
        {
            CMenuPopup menu;
            HotPaths.FillHotPathsMenu(&menu, CM_ACTIVEHOTPATH_MIN);
            menu.Track(0, r.left, r.bottom, HWindow, &r);
            break;
        }

        case CM_USERMENUDROP:
        {
            UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
            CMenuPopup menu;
            FillUserMenu(&menu);
            // the next round of locking (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) will be
            // in WM_USER_ENTERMENULOOP+WM_USER_LEAVEMENULOOP, but that's already nested, no overhead,
            // so we ignore it, we will not fight against it in any way
            menu.Track(0, r.left, r.bottom, HWindow, &r);
            UserMenuIconBkgndReader.EndUserMenuIconsInUse();
            break;
        }

        case CM_NEWDROP:
        {
            CMenuPopup menu(CML_FILES_NEW);
            menu.Track(0, r.left, r.bottom, HWindow, &r);
            break;
        }

        case CM_OPEN_FOLDER_DROP:
        {
            CMenuPopup menu;

            CGUIMenuPopupAbstract* popup = MainMenu.GetSubMenu(CML_COMMANDS, FALSE);
            if (popup != NULL)
            {
                popup = popup->GetSubMenu(CML_COMMANDS_FOLDERS, FALSE);
                if (popup != NULL)
                    popup->Track(0, r.left, r.bottom, HWindow, &r);
            }
            break;
        }

        case CM_ACTIVEBACK:
        case CM_ACTIVEFORWARD:
        case CM_LBACK:
        case CM_LFORWARD:
        case CM_RBACK:
        case CM_RFORWARD:
        {
            BOOL forward = id == CM_ACTIVEFORWARD || id == CM_LFORWARD || id == CM_RFORWARD;

            CMenuPopup menu;
            CFilesWindow* panel = GetActivePanel();
            if (id == CM_LBACK || id == CM_LFORWARD)
                panel = LeftPanel;
            if (id == CM_RBACK || id == CM_RFORWARD)
                panel = RightPanel;
            panel->PathHistory->FillBackForwardPopupMenu(&menu, forward);
            DWORD cmd = menu.Track(MENU_TRACK_RETURNCMD,
                                   r.left, r.bottom,
                                   HWindow, &r);
            if (cmd != 0)
                panel->PathHistory->Execute(cmd, forward, panel);
            break;
        }

        case CM_ACTIVEVIEWMODE:
        case CM_LEFTVIEWMODE:
        case CM_RIGHTVIEWMODE:
        {
            CMenuPopup menu;
            int type = 0;
            if (id == CM_LEFTVIEWMODE)
                type = 1;
            else if (id == CM_RIGHTVIEWMODE)
                type = 2;
            FillViewModeMenu(&menu, 0, type);
            menu.Track(0, r.left, r.bottom, HWindow, &r);
            break;
        }

        case CM_VIEW:
        case CM_EDIT:
        {
            CFilesWindow* activePanel = GetActivePanel();
            if (activePanel == NULL)
                break;

            CMenuPopup popup(id == CM_VIEW ? CML_FILES_VIEWWITH : 0);

            if (id == CM_VIEW)
                activePanel->FillViewWithMenu(&popup);
            else
                activePanel->FillEditWithMenu(&popup);

            popup.Track(0, r.left, r.bottom, HWindow, &r);
            break;
        }
        }

        if (id >= CM_USERMENU_MIN && id <= CM_USERMENU_MAX)
        {
            // user clicked on the group in the User Menu Toolbar
            int iterator = id - CM_USERMENU_MIN;
            int endIndex = UserMenuItems->GetSubmenuEndIndex(iterator);
            if (endIndex != -1)
            {
                UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
                iterator++;
                CMenuPopup menu;
                FillUserMenu2(&menu, &iterator, endIndex);
                // the next round of locking (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) will be
                // in WM_USER_ENTERMENULOOP+WM_USER_LEAVEMENULOOP, but that's already nested, no overhead,
                // so we ignore it, we will not fight against it in any way
                menu.Track(0, r.left, r.bottom, HWindow, &r);
                UserMenuIconBkgndReader.EndUserMenuIconsInUse();
            }
        }

        if (id >= CM_PLUGINCMD_MIN && id <= CM_PLUGINCMD_MAX)
        {
            // user clicked on the plugin icon in PluginsBar;
            int index2 = id - CM_PLUGINCMD_MIN; // index of the plugin in CPlugions::Data
            CMenuPopup menu(CML_PLUGINS_SUBMENU);
            if (Plugins.InitPluginMenuItemsForBar(HWindow, index2, &menu))
                menu.Track(0, r.left, r.bottom, HWindow, &r);
        }

        if (id >= CM_DRIVEBAR_MIN && id <= CM_DRIVEBAR_MAX)
            DriveBar->Execute(id);
        if (id >= CM_DRIVEBAR2_MIN && id <= CM_DRIVEBAR2_MAX)
            DriveBar2->Execute(id);
        return 0;
    }

    case WM_USER_REPAINTALLICONS:
    {
        if (LeftPanel != NULL)
            LeftPanel->RepaintIconOnly(-1); // all
        if (RightPanel != NULL)
            RightPanel->RepaintIconOnly(-1); // all
        return 0;
    }

    case WM_USER_REPAINTSTATUSBARS:
    {
        if (LeftPanel != NULL && LeftPanel->DirectoryLine != NULL)
            LeftPanel->DirectoryLine->InvalidateAndUpdate(FALSE);
        if (RightPanel != NULL && RightPanel->DirectoryLine != NULL)
            RightPanel->DirectoryLine->InvalidateAndUpdate(FALSE);
        return 0;
    }

    case WM_USER_SHOWWINDOW:
    {
        if (!SalamanderBusy)
        {
            SalamanderBusy = TRUE; // now BUSY
            LastSalamanderIdleTime = GetTickCount();
            BringWindowToTop(HWindow); // This is probably not important, but I saw it in one sample, so I'm adding it as well
            if (IsIconic(HWindow))
            {
                // SetForegroundWindow: this is very important. If we don't call this and we have it set
                // only one instance a tray, so the salamander (sometimes) jumps to the background and only then
                // then it will move up - to the front.
                SetForegroundWindow(HWindow);
                ShowWindow(HWindow, SW_RESTORE);
            }
            else
                SetForegroundWindow(HWindow);
        }
        return 0;
    }

    case WM_USER_SKIPONEREFRESH:
    {
        if (!SetTimer(NULL, 0, 500, SkipOneARTimerProc))
        {
            SkipOneActivateRefresh = FALSE;
        }
        return 0;
    }

        /*      case WM_USER_SETPATHS:
    {
      if (!SalamanderBusy && MainWindow != NULL && MainWindow->CanClose)  // neni "busy" a uz je nastartovany, jinak ignorujeme vyzvy ostatnich procesu
      {
        SalamanderBusy = TRUE;   // ted uz je BUSY
        LastSalamanderIdleTime = GetTickCount();
        CSetPathsParams params;
        ZeroMemory(&params, sizeof(params)); // default hodnoty
        HANDLE sendingProcess = HANDLES_Q(OpenProcess(PROCESS_DUP_HANDLE, FALSE, wParam));
        HANDLE sendingFM = (HGLOBAL)lParam;

        HANDLE fm;
        BOOL alreadyDone = FALSE;
        if (sendingProcess != NULL &&
            HANDLES(DuplicateHandle(sendingProcess, sendingFM,          // sending-process file-mapping
                                    GetCurrentProcess(), &fm,           // this process file-mapping
                                    0, FALSE, DUPLICATE_SAME_ACCESS)))
        {
          CSetPathsParams *unsafe = (CSetPathsParams *)HANDLES(MapViewOfFile(fm, FILE_MAP_WRITE, 0, 0, sizeof(CSetPathsParams))); // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
          if (unsafe != NULL)
          {
            alreadyDone = unsafe->Received;
            if (!alreadyDone)
            {
              lstrcpyn(params.LeftPath, unsafe->LeftPath, MAX_PATH);
              lstrcpyn(params.RightPath, unsafe->RightPath, MAX_PATH - 1);

              if (unsafe->MagicSignature1 == 0x07f2ab13 && unsafe->MagicSignature2 == 0x471e0901)
              {
                // novinky od 2.52
                // WORD version = unsafe->StructVersion; // zatim nepouzivame, prvni verzi rozpozname podle existence signatur
                lstrcpyn(params.ActivePath, unsafe->ActivePath, MAX_PATH);
                params.ActivatePanel = unsafe->ActivatePanel;
              }
              // vratime navratovou hodnotu, prevzali jsme data
              unsafe->Received = TRUE;
            }
            HANDLES(UnmapViewOfFile(unsafe));
          }
          HANDLES(CloseHandle(fm));
        }
        if (sendingProcess != NULL) HANDLES(CloseHandle(sendingProcess));

        if (!alreadyDone)
          ApplyCommandLineParams(&params);
      }
      return 0;
    }
*/

    case WM_USER_AUTOCONFIG:
    {
        PackAutoconfig(HWindow);
        return 0;
    }

    case WM_USER_VIEWERCONFIG:
    {
        if (GetForegroundWindow() != HWindow)
            SetForegroundWindow(HWindow); // to get above the viewer
        WindowProc(WM_USER_CONFIGURATION, 3, 0);
        HWND hCaller = (HWND)wParam;
        if (IsWindow(hCaller))
        {
            // If the window that called us still exists, I will try to bring it up
            // up. It's problematic because if a modal dialog is opened in the meantime,
            // he won't get activation. But I take care of it, because the viewer anyway (I hope)
            // It will go to the salami - I mean to the plugin ;-)
            SetForegroundWindow(hCaller);
        }
        return 0;
    }

    case WM_USER_CONFIGURATION:
    {
        if (!SalamanderBusy)
        {
            SalamanderBusy = TRUE; // now BUSY
            LastSalamanderIdleTime = GetTickCount();
        }

        BeginStopRefresh(); // He was snoring in his sleep

        BOOL oldStatusArea = Configuration.StatusArea;
        BOOL oldPanelCaption = Configuration.ShowPanelCaption;
        BOOL oldPanelZoom = Configuration.ShowPanelZoom;

        UserMenuIconBkgndReader.ResetSysColorsChanged(); // Now we are starting to monitor the change of system colors (a reload of the user menu icons is necessary when changing).
        BOOL readingUMIcons = UserMenuIconBkgndReader.IsReadingIcons();
        if (readingUMIcons) // new icons are on the way to the user menu, we will show them after the configuration is completed (when OK is pressed, we will let the icons load again, so that any newly added icons are also read)
            UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
        BOOL oldUseCustomPanelFont = UseCustomPanelFont;
        LOGFONT oldLogFont = LogFont;
        CConfigurationDlg dlg(HWindow, UserMenuItems, (int)wParam, (int)lParam);
        int res = dlg.Execute(LoadStr(IDS_BUTTON_OK), LoadStr(IDS_BUTTON_CANCEL),
                              LoadStr(IDS_BUTTON_HELP));
        if (readingUMIcons)
            UserMenuIconBkgndReader.EndUserMenuIconsInUse();

        // the dialog was closed - the user could not change the clipboard, we will verify it ...
        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
        IdleCheckClipboard = TRUE; // we will also check the clipboard

        if (res == IDOK) // change values -> refresh everything possible
        {
            if (dlg.PageView.IsDirty())
            {
                // user changed something in the view configuration - let's rebuild the columns
                LeftPanel->SelectViewTemplate(LeftPanel->GetViewTemplateIndex(), TRUE, FALSE);
                RightPanel->SelectViewTemplate(RightPanel->GetViewTemplateIndex(), TRUE, FALSE);
            }
            if (memcmp(&oldLogFont, &LogFont, sizeof(LogFont)) != 0 ||
                oldUseCustomPanelFont != UseCustomPanelFont)
            {
                SetFont();
                // if the headerline is displayed, we need to set the correct size
                LeftPanel->LayoutListBoxChilds();
                RightPanel->LayoutListBoxChilds();
            }

            if (Configuration.ThumbnailSize != LeftPanel->GetThumbnailSize() ||
                Configuration.ThumbnailSize != RightPanel->GetThumbnailSize())
            {
                // if the thumbnail dimension has changed, it needs to be propagated to the panel
                LeftPanel->SetThumbnailSize(Configuration.ThumbnailSize);
                RightPanel->SetThumbnailSize(Configuration.ThumbnailSize);
            }

            if (oldStatusArea != Configuration.StatusArea)
            {
                if (Configuration.StatusArea)
                    AddTrayIcon();
                else
                    RemoveTrayIcon();
            }

            if (UMToolBar != NULL && UMToolBar->HWindow != NULL)
                UMToolBar->CreateButtons();

            if (HPToolBar != NULL && HPToolBar->HWindow != NULL)
                HPToolBar->CreateButtons();

            if (Windows7AndLater)
                CreateJumpList();

            // user could turn on/off Documents
            CDriveBar* copyDrivesListFrom = NULL;
            if (DriveBar != NULL && DriveBar->HWindow != NULL)
            {
                DriveBar->RebuildDrives(DriveBar); // We do not need slow disk enumeration
                copyDrivesListFrom = DriveBar;
            }
            if (DriveBar2 != NULL && DriveBar2->HWindow != NULL)
                DriveBar2->RebuildDrives(copyDrivesListFrom);

            if (oldPanelCaption != Configuration.ShowPanelCaption || oldPanelZoom != Configuration.ShowPanelZoom)
            {
                if (LeftPanel->DirectoryLine != NULL && LeftPanel->DirectoryLine->HWindow != NULL)
                    LeftPanel->DirectoryLine->Repaint();
                if (RightPanel->DirectoryLine != NULL && RightPanel->DirectoryLine->HWindow != NULL)
                    RightPanel->DirectoryLine->Repaint();
            }

            // Icon of the main window
            SetWindowIcon();
            // Icon in progress windows
            ProgressDlgArray.PostIconChange();

            // we will set both panels to refresh
            LeftPanel->RefreshForConfig();
            RightPanel->RefreshForConfig();

            // we will delete the saved data in SalShExtPastedData (the archive may have changed)
            SalShExtPastedData.ReleaseStoredArchiveData();

            // Internal Viewer and Find: restore all windows (the font has already changed)
            BroadcastConfigChanged();

            // we will also distribute this news among the plug-ins
            Plugins.Event(PLUGINEVENT_CONFIGURATIONCHANGED, 0);
        }

        EndStopRefresh(); // now he's sniffling again, he'll start up
        return 0;
    }

    case WM_SYSCOMMAND:
    {
        if (HasLockedUI())
            break;

        // if the user pressed the Alt button during the displayed initial Splash window,
        // Entering the system menu before displaying the MainWindow and Splash
        // The window remained open until the user pressed Escape
        // if MainWindow is not yet displayed, disable access to the Window menu
        if (wParam == SC_KEYMENU && !IsWindowVisible(HWindow))
            return 0;

        // set status bar as appropriate
        UINT nItemID = wParam != CM_ALWAYSONTOP ? ((UINT)wParam & 0xFFF0) : (UINT)wParam;

        // don't interfere with system commands if not in help mode
        if (HelpMode)
        {
            switch (nItemID)
            {
            case SC_SIZE:
            case SC_MOVE:
            case SC_MINIMIZE:
            case SC_MAXIMIZE:
            case SC_NEXTWINDOW:
            case SC_PREVWINDOW:
            case SC_CLOSE:
            case SC_RESTORE:
            case SC_TASKLIST:
            {
                OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, IDH_SYSMENUCMDS, FALSE);
                return 0;
            }

            case CM_ALWAYSONTOP:
            {
                OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, nItemID, FALSE);
                return 0;
            }
            }
        }

        if (wParam == CM_ALWAYSONTOP)
            WindowProc(WM_COMMAND, wParam, lParam); // pass on

        if (Configuration.StatusArea && wParam == SC_MINIMIZE)
        {
            ShowWindow(HWindow, SW_MINIMIZE);
            ShowWindow(HWindow, SW_HIDE);
            return 0;
        }
        break;
    }

    case WM_USER_FLASHWINDOW:
    {
        FlashWindow(HWindow, TRUE);
        Sleep(100);
        FlashWindow(HWindow, FALSE);
        return 0;
    }

    case WM_APPCOMMAND:
    {
        // we will handle messages coming especially from new mice (4 and other button)
        // and multimedia keyboards
        // see https://forum.altap.cz/viewtopic.php?t=192
        DWORD cmd = GET_APPCOMMAND_LPARAM(lParam);
        switch (cmd)
        {
        case APPCOMMAND_BROWSER_BACKWARD:
        {
            SendMessage(HWindow, WM_COMMAND, CM_ACTIVEBACK, 0);
            return TRUE;
        }

        case APPCOMMAND_BROWSER_FORWARD:
        {
            SendMessage(HWindow, WM_COMMAND, CM_ACTIVEFORWARD, 0);
            return TRUE;
        }
        }
        break;
    }

    case WM_COMMAND:
    {
        if (HelpMode && (HWND)lParam == NULL && LOWORD(wParam) != CM_HELP_CONTEXT)
        {
            DWORD id = LOWORD(wParam);

            if (id >= CM_PLUGINCMD_MIN && id <= CM_PLUGINCMD_MAX)
            { // command of the plugin (submenu of the plugin in the Plugins menu)
                if (Plugins.HelpForMenuItem(HWindow, LOWORD(wParam)))
                    return 0;
                else
                    id = CM_LAST_PLUGIN_CMD; // if the plugin does not have its own help, we will display Salamander's help "Using Plugins"
            }

            // adjust intervals to their first value
            if (id > CM_USERMENU_MIN && id <= CM_USERMENU_MAX)
                id = CM_USERMENU_MIN;
            if (id > CM_DRIVEBAR_MIN && id <= CM_DRIVEBAR_MAX)
                id = CM_DRIVEBAR_MIN;
            if (id > CM_DRIVEBAR2_MIN && id <= CM_DRIVEBAR2_MAX)
                id = CM_DRIVEBAR2_MIN;
            if (id > CM_PLUGINCFG_MIN && id <= CM_PLUGINCFG_MAX)
                id = CM_PLUGINCFG_MIN;
            if (id > CM_PLUGINABOUT_MIN && id <= CM_PLUGINABOUT_MAX)
                id = CM_PLUGINABOUT_MIN;

            if (id > CM_ACTIVEMODE_1 && id <= CM_ACTIVEMODE_10)
                id = CM_ACTIVEMODE_1;
            if (id > CM_LEFTMODE_1 && id <= CM_LEFTMODE_10)
                id = CM_LEFTMODE_1;
            if (id > CM_RIGHTMODE_1 && id <= CM_RIGHTMODE_10)
                id = CM_RIGHTMODE_1;

            if (id > CM_LEFTSORTBY_MIN && id <= CM_LEFTSORTBY_MAX)
                id = CM_LEFTSORTBY_MIN;
            if (id > CM_RIGHTSORTBY_MIN && id <= CM_RIGHTSORTBY_MAX)
                id = CM_RIGHTSORTBY_MIN;

            if (id > CM_LEFTHOTPATH_MIN && id <= CM_LEFTHOTPATH_MAX)
                id = CM_LEFTHOTPATH_MIN;
            if (id > CM_RIGHTHOTPATH_MIN && id <= CM_RIGHTHOTPATH_MAX)
                id = CM_RIGHTHOTPATH_MIN;

            if (id > CM_LEFTHISTORYPATH_MIN && id <= CM_LEFTHISTORYPATH_MAX)
                id = CM_LEFTHISTORYPATH_MIN;
            if (id > CM_RIGHTHISTORYPATH_MIN && id <= CM_RIGHTHISTORYPATH_MAX)
                id = CM_RIGHTHISTORYPATH_MIN;

            if (id > CM_CODING_MIN && id <= CM_CODING_MAX)
                id = CM_CODING_MIN;

            if (id > CM_NEWMENU_MIN && id <= CM_NEWMENU_MAX)
                id = CM_NEWMENU_MIN;

            OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, id, FALSE);

            return 0;
        }
        CFilesWindow* activePanel = GetActivePanel();
        if (activePanel == NULL || LeftPanel == NULL || RightPanel == NULL)
        {
            TRACE_E("activePanel == NULL || LeftPanel == NULL || RightPanel == NULL");
            return 0;
        }

        // End quick-search mode
        if (LOWORD(wParam) != CM_ACTIVEREFRESH &&         // except for refreshing in the active panel
            LOWORD(wParam) != CM_LEFTREFRESH &&           // except for the refresh in the left panel
            LOWORD(wParam) != CM_RIGHTREFRESH &&          // except for the refresh in the right panel
            (HIWORD(wParam) == 0 || HIWORD(wParam) == 1)) // only from the menu or accelerator
        {
            CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        }

        if (LOWORD(wParam) >= CM_NEWMENU_MIN && LOWORD(wParam) <= CM_NEWMENU_MAX)
        { // Command from the New menu
            if (ContextMenuNew->MenuIsAssigned() && activePanel->CheckPath(TRUE) == ERROR_SUCCESS)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                {
                    CALL_STACK_MESSAGE1("CMainWindow::WindowProc::menu_new");
                    IContextMenu* menu2 = ContextMenuNew->GetMenu2();
                    menu2->AddRef(); // if we accidentally lost ContextMenuNew (asynchronous closing - message loop)
                    CShellExecuteWnd shellExecuteWnd;
                    CMINVOKECOMMANDINFO ici;
                    ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
                    ici.fMask = 0;
                    ici.hwnd = shellExecuteWnd.Create(HWindow, "SEW: CMainWindow::WindowProc cmd=%d", LOWORD(wParam) - CM_NEWMENU_MIN);
                    ici.lpVerb = MAKEINTRESOURCE((LOWORD(wParam) - CM_NEWMENU_MIN));
                    ici.lpParameters = NULL;
                    ici.lpDirectory = activePanel->GetPath();
                    ici.nShow = SW_SHOWNORMAL;
                    ici.dwHotKey = 0;
                    ici.hIcon = 0;
                    activePanel->FocusFirstNewItem = TRUE; // to select the newly generated file/directory

                    CMainWindowWindowProcAux(menu2, ici);

                    menu2->Release();
                }
                //--- refresh non-automatically refreshed directories
                // we report a change in the current directory (a new file/directory can probably only be created in it)
                MainWindow->PostChangeOnPathNotification(activePanel->GetPath(), FALSE);
            }
            else
                TRACE_E("ContextMenuNew is not valid anymore, it is not posible to invoke menu New command.");
            return 0;
        }

        if (LOWORD(wParam) >= CM_PLUGINABOUT_MIN && LOWORD(wParam) <= CM_PLUGINABOUT_MAX)
        {
            Plugins.OnPluginAbout(HWindow, LOWORD(wParam) - CM_PLUGINABOUT_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_PLUGINCFG_MIN && LOWORD(wParam) <= CM_PLUGINCFG_MAX)
        {
            Plugins.OnPluginConfiguration(HWindow, LOWORD(wParam) - CM_PLUGINCFG_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_PLUGINCMD_MIN && LOWORD(wParam) <= CM_PLUGINCMD_MAX)
        { // command from the menu of some plug-in
            // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            if (Plugins.ExecuteMenuItem(activePanel, HWindow, LOWORD(wParam)))
            {
                activePanel->StoreSelection();                               // Save the selection for the Restore Selection command
                activePanel->SetSel(FALSE, -1, TRUE);                        // explicit override
                PostMessage(activePanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
            }

            // increase the thread priority again, operation completed
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            // Restoring the content of non-automatic panels is in the plugins

            UpdateWindow(HWindow);
            return 0;
        }

        if (LOWORD(wParam) == CM_LAST_PLUGIN_CMD)
        { // command plugins/last command
            // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            if (Plugins.OnLastCommand(activePanel, HWindow))
            {
                activePanel->StoreSelection();                               // Save the selection for the Restore Selection command
                activePanel->SetSel(FALSE, -1, TRUE);                        // explicit override
                PostMessage(activePanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
            }

            // increase the thread priority again, operation completed
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            // Restoring the content of non-automatic panels is in the plugins

            UpdateWindow(HWindow);
            return 0;
        }

        if (LOWORD(wParam) >= CM_USERMENU_MIN && LOWORD(wParam) <= CM_USERMENU_MAX)
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command

                CUserMenuAdvancedData userMenuAdvancedData;

                char* list = userMenuAdvancedData.ListOfSelNames;
                char* listEnd = list + USRMNUARGS_MAXLEN - 1;
                BOOL smallBuf = FALSE;
                if (activePanel->SelectedCount > 0)
                {
                    int count = activePanel->Files->Count + activePanel->Dirs->Count;
                    int i;
                    for (i = 0; i < count; i++)
                    {
                        CFileData* file = (i < activePanel->Dirs->Count) ? &activePanel->Dirs->At(i) : &activePanel->Files->At(i - activePanel->Dirs->Count);
                        if (file->Selected)
                        {
                            if (list > userMenuAdvancedData.ListOfSelNames)
                            {
                                if (list < listEnd)
                                    *list++ = ' ';
                                else
                                    break;
                            }
                            if (!AddToListOfNames(&list, listEnd, file->Name, file->NameLen))
                                break;
                        }
                    }
                    if (i < count)
                        smallBuf = TRUE;
                }
                else // we take the focused item
                {
                    BOOL subDir;
                    if (activePanel->Dirs->Count > 0)
                        subDir = (strcmp(activePanel->Dirs->At(0).Name, "..") == 0);
                    else
                        subDir = FALSE;
                    int index = activePanel->GetCaretIndex();
                    if (index >= 0 && index < activePanel->Files->Count + activePanel->Dirs->Count &&
                        (index != 0 || !subDir))
                    {
                        CFileData* file = (index < activePanel->Dirs->Count) ? &activePanel->Dirs->At(index) : &activePanel->Files->At(index - activePanel->Dirs->Count);
                        if (!AddToListOfNames(&list, listEnd, file->Name, file->NameLen))
                            smallBuf = TRUE;
                    }
                }
                if (smallBuf)
                {
                    userMenuAdvancedData.ListOfSelNames[0] = 0; // small buffer for list of selected names
                    userMenuAdvancedData.ListOfSelNamesIsEmpty = FALSE;
                }
                else
                {
                    *list = 0;
                    userMenuAdvancedData.ListOfSelNamesIsEmpty = userMenuAdvancedData.ListOfSelNames[0] == 0;
                }

                char* listFull = userMenuAdvancedData.ListOfSelFullNames;
                char* listFullEnd = listFull + USRMNUARGS_MAXLEN - 1;
                smallBuf = FALSE;
                char fullName[MAX_PATH];
                if (activePanel->SelectedCount > 0)
                {
                    int count = activePanel->Files->Count + activePanel->Dirs->Count;
                    int i;
                    for (i = 0; i < count; i++)
                    {
                        CFileData* file = (i < activePanel->Dirs->Count) ? &activePanel->Dirs->At(i) : &activePanel->Files->At(i - activePanel->Dirs->Count);
                        if (file->Selected)
                        {
                            if (listFull > userMenuAdvancedData.ListOfSelFullNames)
                            {
                                if (listFull < listFullEnd)
                                    *listFull++ = ' ';
                                else
                                    break;
                            }
                            lstrcpyn(fullName, activePanel->GetPath(), MAX_PATH);
                            if (!SalPathAppend(fullName, file->Name, MAX_PATH) ||
                                !AddToListOfNames(&listFull, listFullEnd, fullName, (int)strlen(fullName)))
                                break;
                        }
                    }
                    if (i < count)
                        smallBuf = TRUE;
                }
                else // we take the focused item
                {
                    BOOL subDir;
                    if (activePanel->Dirs->Count > 0)
                        subDir = (strcmp(activePanel->Dirs->At(0).Name, "..") == 0);
                    else
                        subDir = FALSE;
                    int index = activePanel->GetCaretIndex();
                    if (index >= 0 && index < activePanel->Files->Count + activePanel->Dirs->Count &&
                        (index != 0 || !subDir))
                    {
                        CFileData* file = (index < activePanel->Dirs->Count) ? &activePanel->Dirs->At(index) : &activePanel->Files->At(index - activePanel->Dirs->Count);
                        lstrcpyn(fullName, activePanel->GetPath(), MAX_PATH);
                        if (!SalPathAppend(fullName, file->Name, MAX_PATH) ||
                            !AddToListOfNames(&listFull, listFullEnd, fullName, (int)strlen(fullName)))
                        {
                            smallBuf = TRUE;
                        }
                    }
                }
                if (smallBuf)
                {
                    userMenuAdvancedData.ListOfSelFullNames[0] = 0; // small buffer for a list of selected full names
                    userMenuAdvancedData.ListOfSelFullNamesIsEmpty = FALSE;
                }
                else
                {
                    *listFull = 0;
                    userMenuAdvancedData.ListOfSelFullNamesIsEmpty = userMenuAdvancedData.ListOfSelFullNames[0] == 0;
                }

                if (LeftPanel->Is(ptDisk))
                {
                    lstrcpyn(userMenuAdvancedData.FullPathLeft, LeftPanel->GetPath(), MAX_PATH);
                    if (!SalPathAddBackslash(userMenuAdvancedData.FullPathLeft, MAX_PATH))
                        userMenuAdvancedData.FullPathLeft[0] = 0;
                }
                else
                    userMenuAdvancedData.FullPathLeft[0] = 0;
                if (RightPanel->Is(ptDisk))
                {
                    lstrcpyn(userMenuAdvancedData.FullPathRight, RightPanel->GetPath(), MAX_PATH);
                    if (!SalPathAddBackslash(userMenuAdvancedData.FullPathRight, MAX_PATH))
                        userMenuAdvancedData.FullPathRight[0] = 0;
                }
                else
                    userMenuAdvancedData.FullPathRight[0] = 0;
                userMenuAdvancedData.FullPathInactive = (activePanel == LeftPanel) ? userMenuAdvancedData.FullPathRight : userMenuAdvancedData.FullPathLeft;

                userMenuAdvancedData.CompareName1[0] = 0;
                userMenuAdvancedData.CompareName2[0] = 0;
                userMenuAdvancedData.CompareNamesAreDirs = FALSE;
                userMenuAdvancedData.CompareNamesReversed = FALSE;
                CFilesWindow* inactivePanel = (activePanel == LeftPanel) ? RightPanel : LeftPanel;
                CFileData* f1 = NULL;
                CFileData* f2 = NULL;
                BOOL f2FromInactPanel = FALSE;
                int focus = activePanel->GetCaretIndex();
                BOOL focusOnUpDir = (focus == 0 && activePanel->Dirs->Count > 0 &&
                                     strcmp(activePanel->Dirs->At(0).Name, "..") == 0);
                int indexes[3];
                int selCount = activePanel->GetSelItems(3, indexes); // we are interested in: 0-2 = number of marked, 3 = more than two
                int tgtIndexes[2];
                int tgtSelCount = inactivePanel->Is(ptDisk) ? inactivePanel->GetSelItems(2, tgtIndexes) : 0; // we are interested in: 0-1=number of marked, 2=more than one
                if (selCount == 2)                                                                           // two selected items in the source panel
                {
                    if ((indexes[0] < activePanel->Dirs->Count) == (indexes[1] < activePanel->Dirs->Count)) // both items are files/directories
                    {
                        f1 = (indexes[0] < activePanel->Dirs->Count) ? &activePanel->Dirs->At(indexes[0]) : &activePanel->Files->At(indexes[0] - activePanel->Dirs->Count);
                        f2 = (indexes[1] < activePanel->Dirs->Count) ? &activePanel->Dirs->At(indexes[1]) : &activePanel->Files->At(indexes[1] - activePanel->Dirs->Count);
                        userMenuAdvancedData.CompareNamesAreDirs = (indexes[0] < activePanel->Dirs->Count);
                    }
                }
                else
                {
                    if (selCount == 1) // one marked item in the source panel
                    {
                        f1 = (indexes[0] < activePanel->Dirs->Count) ? &activePanel->Dirs->At(indexes[0]) : &activePanel->Files->At(indexes[0] - activePanel->Dirs->Count);
                        userMenuAdvancedData.CompareNamesAreDirs = (indexes[0] < activePanel->Dirs->Count);
                        if (!focusOnUpDir && focus != indexes[0] && tgtSelCount != 1)
                        {
                            if ((focus < activePanel->Dirs->Count) == userMenuAdvancedData.CompareNamesAreDirs) // both items are files/directories
                            {
                                f2 = (focus < activePanel->Dirs->Count) ? &activePanel->Dirs->At(focus) : &activePanel->Files->At(focus - activePanel->Dirs->Count);
                            }
                        }
                    }
                    else
                    {
                        if (selCount == 0) // No item selected in the source panel, we take focus
                        {
                            if (!focusOnUpDir)
                            {
                                if (focus >= 0 && focus < activePanel->Dirs->Count + activePanel->Files->Count)
                                {
                                    f1 = (focus < activePanel->Dirs->Count) ? &activePanel->Dirs->At(focus) : &activePanel->Files->At(focus - activePanel->Dirs->Count);
                                    userMenuAdvancedData.CompareNamesAreDirs = (focus < activePanel->Dirs->Count);
                                }
                            }
                        }
                    }
                }
                if (f1 != NULL && f2 == NULL)
                {
                    if (tgtSelCount == 1 &&
                        (tgtIndexes[0] < inactivePanel->Dirs->Count) == userMenuAdvancedData.CompareNamesAreDirs) // both items are files/directories
                    {
                        f2 = (tgtIndexes[0] < inactivePanel->Dirs->Count) ? &inactivePanel->Dirs->At(tgtIndexes[0]) : &inactivePanel->Files->At(tgtIndexes[0] - inactivePanel->Dirs->Count);
                        f2FromInactPanel = TRUE;
                    }
                    else
                    {
                        if (inactivePanel->Is(ptDisk))
                        {
                            int c = inactivePanel->Dirs->Count + inactivePanel->Files->Count;
                            int i;
                            for (i = 0; i < c; i++)
                            {
                                CFileData* f = (i < inactivePanel->Dirs->Count) ? &inactivePanel->Dirs->At(i) : &inactivePanel->Files->At(i - inactivePanel->Dirs->Count);
                                if (f->NameLen == f1->NameLen &&
                                    StrICmp(f->Name, f1->Name) == 0)
                                {
                                    if ((i < inactivePanel->Dirs->Count) == userMenuAdvancedData.CompareNamesAreDirs) // both items are files/directories
                                    {
                                        f2 = f;
                                        f2FromInactPanel = TRUE;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                if (f1 != NULL)
                {
                    lstrcpyn(userMenuAdvancedData.CompareName1, activePanel->GetPath(), MAX_PATH);
                    if (!SalPathAppend(userMenuAdvancedData.CompareName1, f1->Name, MAX_PATH))
                        userMenuAdvancedData.CompareName1[0] = 0;
                }
                if (f2 != NULL)
                {
                    lstrcpyn(userMenuAdvancedData.CompareName2,
                             (f2FromInactPanel ? inactivePanel : activePanel)->GetPath(), MAX_PATH);
                    if (!SalPathAppend(userMenuAdvancedData.CompareName2, f2->Name, MAX_PATH))
                        userMenuAdvancedData.CompareName2[0] = 0;
                    else
                    {
                        if (f2FromInactPanel && inactivePanel == LeftPanel)
                            userMenuAdvancedData.CompareNamesReversed = TRUE;
                    }
                }
                if (userMenuAdvancedData.CompareName1[0] != 0 &&
                    userMenuAdvancedData.CompareName2[0] == 0 && activePanel == RightPanel)
                {
                    userMenuAdvancedData.CompareNamesReversed = TRUE;
                }

                CUMDataFromPanel data(activePanel);
                SetCurrentDirectory(activePanel->GetPath());
                UserMenu(HWindow, LOWORD(wParam) - CM_USERMENU_MIN, GetNextFileFromPanel,
                         &data, &userMenuAdvancedData);
                SetCurrentDirectoryToSystem();
            }
            return 0;
        }

        if (LOWORD(wParam) >= CM_VIEWWITH_MIN && LOWORD(wParam) <= CM_VIEWWITH_MAX)
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->OnViewFileWith(LOWORD(wParam) - CM_VIEWWITH_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_EDITWITH_MIN && LOWORD(wParam) <= CM_EDITWITH_MAX)
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->OnEditFileWith(LOWORD(wParam) - CM_EDITWITH_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_DRIVEBAR_MIN && LOWORD(wParam) <= CM_DRIVEBAR_MAX)
        {
            DriveBar->Execute(LOWORD(wParam));
            return 0;
        }

        if (LOWORD(wParam) >= CM_DRIVEBAR2_MIN && LOWORD(wParam) <= CM_DRIVEBAR2_MAX)
        {
            DriveBar2->Execute(LOWORD(wParam));
            return 0;
        }

        if (LOWORD(wParam) >= CM_ACTIVEHOTPATH_MIN && LOWORD(wParam) < CM_ACTIVEHOTPATH_MIN + HOT_PATHS_COUNT)
        {
            activePanel->GotoHotPath(LOWORD(wParam) - CM_ACTIVEHOTPATH_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_LEFTHOTPATH_MIN && LOWORD(wParam) < CM_LEFTHOTPATH_MIN + HOT_PATHS_COUNT)
        {
            LeftPanel->GotoHotPath(LOWORD(wParam) - CM_LEFTHOTPATH_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_RIGHTHOTPATH_MIN && LOWORD(wParam) < CM_RIGHTHOTPATH_MIN + HOT_PATHS_COUNT)
        {
            RightPanel->GotoHotPath(LOWORD(wParam) - CM_RIGHTHOTPATH_MIN);
            return 0;
        }

        if (LOWORD(wParam) >= CM_LEFTHISTORYPATH_MIN && LOWORD(wParam) <= CM_LEFTHISTORYPATH_MAX)
        {
            DirHistory->Execute(LOWORD(wParam) - CM_LEFTHISTORYPATH_MIN + 1, FALSE, LeftPanel, TRUE, FALSE);
            return 0;
        }

        if (LOWORD(wParam) >= CM_RIGHTHISTORYPATH_MIN && LOWORD(wParam) <= CM_RIGHTHISTORYPATH_MAX)
        {
            DirHistory->Execute(LOWORD(wParam) - CM_RIGHTHISTORYPATH_MIN + 1, FALSE, RightPanel, TRUE, FALSE);
            return 0;
        }

        if (LOWORD(wParam) >= CM_ACTIVEMODE_1 && LOWORD(wParam) <= CM_ACTIVEMODE_10)
        {
            int index = LOWORD(wParam) - CM_ACTIVEMODE_1;
            if (activePanel->IsViewTemplateValid(index))
                activePanel->SelectViewTemplate(index, TRUE, FALSE);
            return 0;
        }

        if (LOWORD(wParam) >= CM_LEFTMODE_1 && LOWORD(wParam) <= CM_LEFTMODE_10)
        {
            int index = LOWORD(wParam) - CM_LEFTMODE_1;
            if (LeftPanel->IsViewTemplateValid(index))
                LeftPanel->SelectViewTemplate(index, TRUE, FALSE);
            return 0;
        }

        if (LOWORD(wParam) >= CM_RIGHTMODE_1 && LOWORD(wParam) <= CM_RIGHTMODE_10)
        {
            int index = LOWORD(wParam) - CM_RIGHTMODE_1;
            if (RightPanel->IsViewTemplateValid(index))
                RightPanel->SelectViewTemplate(index, TRUE, FALSE);
            return 0;
        }

        if (LOWORD(wParam) >= CM_ACTIVEHOTPATH_MIN && LOWORD(wParam) < CM_ACTIVEHOTPATH_MIN + HOT_PATHS_COUNT)
        {
            activePanel->GotoHotPath(LOWORD(wParam) - CM_ACTIVEHOTPATH_MIN);
            return 0;
        }

        switch (LOWORD(wParam))
        {
        case CM_HELP_CONTEXT:
        {
            OnContextHelp();
            return 0;
        }

            /*          case CM_HELP_KEYBOARD:
        {
          ShellExecute(HWindow, "open", "https://www.altap.cz/salam_en/features/keyboard.html", NULL, NULL, SW_SHOWNORMAL);
          return 0;
        }*/
        case CM_FORUM:
        {
            ShellExecute(HWindow, "open", "https://forum.altap.cz/", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }

        case CM_HELP_CONTENTS:
        case CM_HELP_SEARCH:
        case CM_HELP_INDEX:
        case CM_HELP_KEYBOARD:
        {
            CHtmlHelpCommand command;
            DWORD_PTR dwData = 0;
            switch (LOWORD(wParam))
            {
            case CM_HELP_CONTENTS:
            {
                OpenHtmlHelp(NULL, HWindow, HHCDisplayTOC, 0, TRUE); // We don't want two message boxes in a row
                command = HHCDisplayContext;
                dwData = IDH_INTRODUCTION;
                break;
            }

            case CM_HELP_INDEX:
            {
                command = HHCDisplayIndex;
                break;
            }

            case CM_HELP_SEARCH:
            {
                command = HHCDisplaySearch;
                break;
            }

            case CM_HELP_KEYBOARD:
            {
                command = HHCDisplayContext;
                dwData = CM_HELP_KEYBOARD;
                break;
            }
            }

            OpenHtmlHelp(NULL, HWindow, command, dwData, FALSE);

            return 0;
        }

        case CM_HELP_ABOUT:
        {
            CAboutDialog dlg(HWindow);
            dlg.Execute();
            return 0;
        }

            /*          case CM_HELP_TIP:
        {
          BOOL openQuiet = lParam == 0xffffffff;
          if (TipOfTheDayDialog != NULL)
          {
            TipOfTheDayDialog->IncrementTipIndex();
            TipOfTheDayDialog->InvalidateTipWindow();
            SetForegroundWindow(TipOfTheDayDialog->HWindow);
          }
          else
          {
            TipOfTheDayDialog = new CTipOfTheDayDialog(openQuiet);
            if (TipOfTheDayDialog != NULL)
            {
              if (TipOfTheDayDialog->IsGood())
              {
                TipOfTheDayDialog->Create();
              }
              else
              {
                delete TipOfTheDayDialog;
                TipOfTheDayDialog = NULL;
                // the file probably does not exist - we will not try it at startup next time
                if (openQuiet)
                  Configuration.ShowTipOfTheDay = FALSE;
              }
            }
          }
          return 0;
        }*/
        case CM_ALWAYSONTOP:
        {
            if (!Configuration.AlwaysOnTop && Configuration.CnfrmAlwaysOnTop)
            {
                BOOL dontShow = !Configuration.CnfrmAlwaysOnTop;

                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = HWindow;
                params.Flags = MSGBOXEX_OKCANCEL | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                params.Caption = LoadStr(IDS_INFOTITLE);
                params.Text = LoadStr(IDS_WANTALWAYSONTOP);
                params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAINAT);
                params.CheckBoxValue = &dontShow;
                int ret = SalMessageBoxEx(&params);
                Configuration.CnfrmAlwaysOnTop = !dontShow;
                if (ret == IDCANCEL)
                    return 0;
            }

            Configuration.AlwaysOnTop = !Configuration.AlwaysOnTop;
            HMENU h = GetSystemMenu(HWindow, FALSE);
            if (h != NULL)
            {
                CheckMenuItem(h, CM_ALWAYSONTOP, MF_BYCOMMAND | (Configuration.AlwaysOnTop ? MF_CHECKED : MF_UNCHECKED));
            }

            SetWindowPos(HWindow,
                         Configuration.AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            return 0;
        }

        case CM_MINIMIZE:
            MinimizeApp(MainWindow->HWindow);
            return 0;

        case CM_TASKLIST:
        {
            CTaskListDialog(HWindow).Execute();
            return 0;
        }

        case CM_CLIPCOPYFULLNAME:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CopyFocusedNameToClipboard(cfnmFull);
            return 0;
        }

        case CM_CLIPCOPYNAME:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CopyFocusedNameToClipboard(cfnmShort);
            return 0;
        }

        case CM_CLIPCOPYFULLPATH:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CopyCurrentPathToClipboard();
            return 0;
        }

        case CM_CLIPCOPYUNCNAME:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CopyFocusedNameToClipboard(cfnmUNC);
            return 0;
        }

        case CM_OPEN_IN_OTHER_PANEL:
        case CM_OPEN_IN_OTHER_PANEL_ACT:
        {
            activePanel->OpenFocusedInOtherPanel(LOWORD(wParam) == CM_OPEN_IN_OTHER_PANEL_ACT);
            return 0;
        }

        case CM_PLUGINS:
        {
            BeginStopRefresh(); // He was snoring in his sleep

            CPluginsDlg dlg(HWindow);
            dlg.Execute();
            if (dlg.GetRefreshPanels())
            {
                UpdateWindow(HWindow);

                if ((LeftPanel->Is(ptDisk) || LeftPanel->Is(ptZIPArchive)) &&
                    IsUNCPath(LeftPanel->GetPath()) &&
                    LeftPanel->DirectoryLine != NULL)
                {
                    LeftPanel->DirectoryLine->BuildHotTrackItems();
                }
                if ((RightPanel->Is(ptDisk) || RightPanel->Is(ptZIPArchive)) &&
                    IsUNCPath(RightPanel->GetPath()) &&
                    RightPanel->DirectoryLine != NULL)
                {
                    RightPanel->DirectoryLine->BuildHotTrackItems();
                }

                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                int t2 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                SendMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
                SendMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2);
            }
            if (dlg.GetRefreshPanels() || // Refresh the drivebars for the Nethood plugin (the Network Neighborhood icon disappears/reappears)
                dlg.GetDrivesBarChange()) // Change visibility of the FS item in Drives bars
            {
                PostMessage(HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
            }

            const char* focusPlugin = dlg.GetFocusPlugin();
            if (focusPlugin[0] != 0)
            {
                char newPath[MAX_PATH];
                lstrcpyn(newPath, focusPlugin, MAX_PATH);
                const char* newName;
                char* p = strrchr(newPath, '\\');
                if (p != NULL)
                {
                    p++;
                    *p = 0;
                    newName = focusPlugin + int(p - newPath);
                }
                else
                    newName = "";
                SendMessage(GetActivePanel()->HWindow, WM_USER_FOCUSFILE, (WPARAM)newName, (LPARAM)newPath);
            }

            EndStopRefresh(); // now he's sniffling again, he'll start up
            return 0;
        }

        case CM_SAVECONFIG:
        {
            // if there is an exported configuration, display a warning
            if (FileExists(ConfigurationName))
            {
                char buff[3000];
                _snprintf_s(buff, _TRUNCATE, LoadStr(IDS_SAVECFG_EXPFILEEXISTS), ConfigurationName);
                int ret = SalMessageBox(HWindow, buff, LoadStr(IDS_INFOTITLE),
                                        MB_ICONINFORMATION | MB_OKCANCEL);
                if (ret == IDCANCEL)
                {
                    // Move the user to the correct directory and focus on the configuration file to make it easier for them
                    char path[MAX_PATH];
                    char* s = strrchr(ConfigurationName, '\\');
                    if (s != NULL)
                    {
                        memcpy(path, ConfigurationName, s - ConfigurationName);
                        path[s - ConfigurationName] = 0;
                        SendMessage(activePanel->HWindow, WM_USER_FOCUSFILE, (WPARAM)(s + 1), (LPARAM)path);
                    }
                    return 0;
                }
            }
            SaveConfig();
            return 0;
        }

        case CM_EXPORTCONFIG:
        {
            int ret = SalMessageBox(HWindow, LoadStr(IDS_PREDCONFIGEXPORT),
                                    LoadStr(IDS_QUESTION), MB_YESNOCANCEL | MB_ICONQUESTION);
            if (ret == IDCANCEL)
                return 0;

            if (ret == IDYES)
            {
                SaveConfig();
            }

            char file[MAX_PATH];
            char defDir[MAX_PATH];
            strcpy(file, "config_.reg");

            BOOL clearKeyBeforeImport = TRUE;

            MSGBOXEX_PARAMS params;
            memset(&params, 0, sizeof(params));
            params.HParent = HWindow;
            params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT;
            params.Caption = LoadStr(IDS_INFOTITLE);
            params.Text = LoadStr(WindowsVistaAndLater ? IDS_CONFIGEXPVISTA : IDS_CONFIGEXPUPTOXP);
            params.CheckBoxText = LoadStr(IDS_CONFIGEXPCLEARKEY);
            params.CheckBoxValue = &clearKeyBeforeImport;
            SalMessageBoxEx(&params);

            if (WindowsVistaAndLater)
            {
                if (!CreateOurPathInRoamingAPPDATA(defDir))
                {
                    TRACE_E("CM_EXPORTCONFIG: unexpected situation: unable to get our directory under CSIDL_APPDATA");
                    return 0;
                }
            }
            else
            {
                GetModuleFileName(HInstance, defDir, MAX_PATH);
                *strrchr(defDir, '\\') = 0;
            }
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            char* s = LoadStr(IDS_REGFILTER);
            ofn.lpstrFilter = s;
            while (*s != 0) // creating a double-null terminated list
            {
                if (*s == '|')
                    *s = 0;
                s++;
            }
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrInitialDir = defDir;

            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "reg";

            if (SafeGetSaveFileName(&ofn))
            {
                if (SalGetFullName(file))
                {
                    // perform export
                    if (ExportConfiguration(HWindow, file, clearKeyBeforeImport))
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_CONFIGEXPORTED), LoadStr(IDS_INFOTITLE),
                                      MB_OK | MB_ICONINFORMATION);
                    }
                    else
                        DeleteFile(file);
                }
            }
            return 0;
        }

        case CM_IMPORTCONFIG:
        {
            SalMessageBox(HWindow, LoadStr(IDS_CONFIGHOWTOIMPORT), LoadStr(IDS_INFOTITLE),
                          MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case CM_SHARES:
        {
            CSharesDialog dlg(HWindow);
            if (dlg.Execute() == IDOK)
            {
                // user selected Focus
                const char* path = dlg.GetFocusedPath();
                if (path != NULL)
                {
                    char newPath[MAX_PATH];
                    lstrcpyn(newPath, path, MAX_PATH);
                    const char* newName;
                    char* p = strrchr(newPath, '\\');
                    if (p != NULL)
                    {
                        p++;
                        *p = 0;
                        newName = path + int(p - newPath);
                    }
                    else
                        newName = "";
                    SendMessage(GetActivePanel()->HWindow, WM_USER_FOCUSFILE, (WPARAM)newName, (LPARAM)newPath);
                }
            }
            break;
        }

        case CM_SKILLLEVEL:
        {
            CSkillLevelDialog dlg(HWindow, &Configuration.SkillLevel);
            if (dlg.Execute() == IDOK)
                MainMenu.SetSkillLevel(CfgSkillLevelToMenu(Configuration.SkillLevel));
            break;
        }

        case CM_CONFIGURATION:
        {
            PostMessage(HWindow, WM_USER_CONFIGURATION, 0, 0); // normal configuration
            break;
        }

        case CM_AUTOCONFIG:
        {
            PostMessage(HWindow, WM_USER_AUTOCONFIG, 0, 0);
            break;
        }

        case CM_LCUSTOMIZEVIEW:
        {
            PostMessage(HWindow, WM_USER_CONFIGURATION, 4, LeftPanel->GetViewTemplateIndex());
            return 0;
        }

        case CM_RCUSTOMIZEVIEW:
        {
            PostMessage(HWindow, WM_USER_CONFIGURATION, 4, RightPanel->GetViewTemplateIndex());
            return 0;
        }

        case CM_LEFTNAME:
        {
            LeftPanel->ChangeSortType(stName, TRUE);
            return 0;
        }

        case CM_LEFTEXT:
        {
            LeftPanel->ChangeSortType(stExtension, TRUE);
            return 0;
        }

        case CM_LEFTTIME:
        {
            LeftPanel->ChangeSortType(stTime, TRUE);
            return 0;
        }

        case CM_LEFTSIZE:
        {
            LeftPanel->ChangeSortType(stSize, TRUE);
            return 0;
        }

        case CM_LEFTATTR:
        {
            LeftPanel->ChangeSortType(stAttr, TRUE);
            return 0;
        }
            // Change sorting in the right panel
        case CM_RIGHTNAME:
        {
            RightPanel->ChangeSortType(stName, TRUE);
            return 0;
        }

        case CM_RIGHTEXT:
        {
            RightPanel->ChangeSortType(stExtension, TRUE);
            return 0;
        }

        case CM_RIGHTTIME:
        {
            RightPanel->ChangeSortType(stTime, TRUE);
            return 0;
        }

        case CM_RIGHTSIZE:
        {
            RightPanel->ChangeSortType(stSize, TRUE);
            return 0;
        }

        case CM_RIGHTATTR:
        {
            RightPanel->ChangeSortType(stAttr, TRUE);
            return 0;
        }
            // change sorting in current
        case CM_ACTIVENAME:
            activePanel->ChangeSortType(stName, TRUE);
            return 0;
        case CM_ACTIVEEXT:
            activePanel->ChangeSortType(stExtension, TRUE);
            return 0;
        case CM_ACTIVETIME:
            activePanel->ChangeSortType(stTime, TRUE);
            return 0;
        case CM_ACTIVESIZE:
            activePanel->ChangeSortType(stSize, TRUE);
            return 0;
        case CM_ACTIVEATTR:
            activePanel->ChangeSortType(stAttr, TRUE);
            return 0;

        case CM_SORTOPTIONS:
        {
            PostMessage(HWindow, WM_USER_CONFIGURATION, 5, 0);
            return 0;
        }

        // Switch Smart Mode (Ctrl+N)
        case CM_ACTIVE_SMARTMODE:
            ToggleSmartColumnMode(activePanel);
            return 0;
        case CM_LEFT_SMARTMODE:
            ToggleSmartColumnMode(LeftPanel);
            return 0;
        case CM_RIGHT_SMARTMODE:
            ToggleSmartColumnMode(RightPanel);
            return 0;

            // Change the viewed disk in the left panel
        case CM_LCHANGEDRIVE:
        {
            if (activePanel != LeftPanel)
            {
                ChangePanel();
                if (GetActivePanel() != LeftPanel)
                    return 0;          // panel cannot be activated
                UpdateWindow(HWindow); // to ensure that the focus is set before displaying the menu
            }
            if (LeftPanel->DirectoryLine != NULL)
                LeftPanel->DirectoryLine->SetDrivePressed(TRUE);
            LeftPanel->ChangeDrive();
            if (LeftPanel->DirectoryLine != NULL)
                LeftPanel->DirectoryLine->SetDrivePressed(FALSE);
            return 0;
        }
            // Change the viewed disk in the right panel
        case CM_RCHANGEDRIVE:
        {
            if (activePanel != RightPanel)
            {
                ChangePanel();
                if (GetActivePanel() != RightPanel)
                    return 0;          // panel cannot be activated
                UpdateWindow(HWindow); // to ensure that the focus is set before displaying the menu
            }
            if (RightPanel->DirectoryLine != NULL)
                RightPanel->DirectoryLine->SetDrivePressed(TRUE);
            RightPanel->ChangeDrive();
            if (RightPanel->DirectoryLine != NULL)
                RightPanel->DirectoryLine->SetDrivePressed(FALSE);
            return 0;
        }
            // change filter to files
        case CM_LCHANGEFILTER:
        {
            LeftPanel->ChangeFilter();
            return 0;
        }

        case CM_RCHANGEFILTER:
        {
            RightPanel->ChangeFilter();
            return 0;
        }

        case CM_CHANGEFILTER:
            activePanel->ChangeFilter();
            return 0;

        case CM_ACTIVEPARENTDIR:
        {
            activePanel->CtrlPageUpOrBackspace();
            return 0;
        }

        case CM_LPARENTDIR:
        {
            LeftPanel->CtrlPageUpOrBackspace();
            return 0;
        }

        case CM_RPARENTDIR:
        {
            RightPanel->CtrlPageUpOrBackspace();
            return 0;
        }

        case CM_ACTIVEROOTDIR:
        {
            activePanel->GotoRoot();
            return 0;
        }

        case CM_LROOTDIR:
        {
            LeftPanel->GotoRoot();
            return 0;
        }

        case CM_RROOTDIR:
        {
            RightPanel->GotoRoot();
            return 0;
        }
            // Turning on/off the status of the left panel line
        case CM_LEFTSTATUS:
        {
            LeftPanel->ToggleStatusLine();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            return 0;
        }
            // Turning on/off the status bar of the right panel
        case CM_RIGHTSTATUS:
        {
            RightPanel->ToggleStatusLine();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            return 0;
        }
            // Enabling/disabling directory tree in the left panel
        case CM_LEFTDIRLINE:
        {
            LeftPanel->ToggleDirectoryLine();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            return 0;
        }
            // Enabling/disabling directory tree of the right panel
        case CM_RIGHTDIRLINE:
        {
            RightPanel->ToggleDirectoryLine();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            return 0;
        }

        case CM_LEFTHEADER:
        {
            LeftPanel->ToggleHeaderLine();
            LeftPanel->HeaderLineVisible = !LeftPanel->HeaderLineVisible;
            return 0;
        }

        case CM_RIGHTHEADER:
        {
            RightPanel->ToggleHeaderLine();
            RightPanel->HeaderLineVisible = !RightPanel->HeaderLineVisible;
            return 0;
        }

        case CM_LEFTREFRESH: // Refresh the left panel
        {
            LeftPanel->NextFocusName[0] = 0;
            while (SnooperSuspended)
                EndSuspendMode(); // refresh startup fuse
            while (StopRefresh)
                EndStopRefresh(FALSE); // refresh startup fuse
            while (StopIconRepaint)
                EndStopIconRepaint(FALSE); // refresh startup fuse
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RebuildDriveBarsIfNeeded(FALSE, 0, FALSE, 0); // Perhaps the user triggered a refresh to update the list of disks?
            return 0;
        }

        case CM_RIGHTREFRESH: // refresh right panel
        {
            RightPanel->NextFocusName[0] = 0;
            while (SnooperSuspended)
                EndSuspendMode(); // refresh startup fuse
            while (StopRefresh)
                EndStopRefresh(FALSE); // refresh startup fuse
            while (StopIconRepaint)
                EndStopIconRepaint(FALSE); // refresh startup fuse
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RebuildDriveBarsIfNeeded(FALSE, 0, FALSE, 0); // Perhaps the user triggered a refresh to update the list of disks?
            return 0;
        }

        case CM_ACTIVEREFRESH: // refresh right panel
        {
            activePanel->NextFocusName[0] = 0;
            while (SnooperSuspended)
                EndSuspendMode(); // refresh startup fuse
            while (StopRefresh)
                EndStopRefresh(FALSE); // refresh startup fuse
            while (StopIconRepaint)
                EndStopIconRepaint(FALSE); // refresh startup fuse
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(activePanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RebuildDriveBarsIfNeeded(FALSE, 0, FALSE, 0); // Perhaps the user triggered a refresh to update the list of disks?
            return 0;
        }

        case CM_ACTIVEFORWARD:
        {
            activePanel->PathHistory->Execute(1, TRUE, activePanel);
            return 0;
        }

        case CM_ACTIVEBACK:
        {
            activePanel->PathHistory->Execute(2, FALSE, activePanel);
            return 0;
        }

        case CM_LFORWARD:
        {
            LeftPanel->PathHistory->Execute(1, TRUE, LeftPanel);
            return 0;
        }

        case CM_LBACK:
        {
            LeftPanel->PathHistory->Execute(2, FALSE, LeftPanel);
            return 0;
        }

        case CM_RFORWARD:
        {
            RightPanel->PathHistory->Execute(1, TRUE, RightPanel);
            return 0;
        }

        case CM_RBACK:
        {
            RightPanel->PathHistory->Execute(2, FALSE, RightPanel);
            return 0;
        }

        case CM_REFRESHASSOC: // reload associations from the Registry
        {
            OnAssociationsChangedNotification(TRUE);
            return 0;
        }

        case CM_EMAILFILES: // Emailing files and directories
        {
            if (!EnablerFilesOnDisk)
                return 0;
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // Save the selection for the Restore Selection command

            // if no item is selected, we will select the one under focus and save its name
            char temporarySelected[MAX_PATH];
            activePanel->SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            activePanel->EmailFiles();

            // if we have selected an item, we will deselect it again
            activePanel->UnselectItemWithName(temporarySelected);

            return 0;
        }

        case CM_COPYFILES: // Copying files and directories
            if (!EnablerFilesCopy)
                return 0;
        case CM_MOVEFILES: // file and directory moving/renaming
            if (LOWORD(wParam) == CM_MOVEFILES && !EnablerFilesMove)
                return 0;
        case CM_DELETEFILES: // delete file and directory
            if (LOWORD(wParam) == CM_DELETEFILES && !EnablerFilesDelete)
                return 0;
        case CM_OCCUPIEDSPACE: // calculation of the occupied space on the disk
            if (LOWORD(wParam) == CM_OCCUPIEDSPACE && !EnablerOccupiedSpace)
                return 0;
        case CM_CHANGECASE: // change the case of letters in names
        {
            if (LOWORD(wParam) == CM_CHANGECASE && !EnablerFilesOnDisk)
                return 0;
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // Save the selection for the Restore Selection command

            // if no item is selected, we will select the one under focus and save its name
            char temporarySelected[MAX_PATH];
            activePanel->SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            if (activePanel->Is(ptDisk)) // source is disk - all operations go here
            {
                CActionType type;
                switch (LOWORD(wParam))
                {
                case CM_COPYFILES:
                    type = atCopy;
                    break;
                case CM_MOVEFILES:
                    type = atMove;
                    break;
                case CM_DELETEFILES:
                    type = atDelete;
                    break;
                case CM_OCCUPIEDSPACE:
                    type = atCountSize;
                    break;
                case CM_CHANGECASE:
                    type = atChangeCase;
                    break;
                }

                // perform the action
                activePanel->FilesAction(type, GetNonActivePanel());
            }
            else
            {
                if (activePanel->Is(ptZIPArchive)) // source is archive - all operations go here
                {
                    BOOL archMaybeUpdated;
                    activePanel->OfferArchiveUpdateIfNeeded(HWindow, IDS_ARCHIVECLOSEEDIT2, &archMaybeUpdated);
                    if (!archMaybeUpdated)
                    {
                        switch (LOWORD(wParam))
                        {
                        case CM_OCCUPIEDSPACE:
                            activePanel->CalculateOccupiedZIPSpace();
                            break;
                        case CM_COPYFILES:
                            activePanel->UnpackZIPArchive(GetNonActivePanel());
                            break;
                        case CM_DELETEFILES:
                            activePanel->DeleteFromZIPArchive();
                            break;
                        }
                    }
                }
                else
                {
                    if (activePanel->Is(ptPluginFS)) // source is FS - all operations go here
                    {
                        CPluginFSActionType type;
                        switch (LOWORD(wParam))
                        {
                        case CM_COPYFILES:
                            type = fsatCopy;
                            break;
                        case CM_MOVEFILES:
                            type = fsatMove;
                            break;
                        case CM_DELETEFILES:
                            type = fsatDelete;
                            break;
                        case CM_OCCUPIEDSPACE:
                            type = fsatCountSize;
                            break;
                        }
                        activePanel->PluginFSFilesAction(type);
                    }
                }
            }

            // if we have selected an item, we will deselect it again
            activePanel->UnselectItemWithName(temporarySelected);

            return 0;
        }

        case CM_MENU:
        {
            MenuBar->EnterMenu();
            return 0;
        }

        case CM_DIRMENU:
        {
            ShellAction(activePanel, saContextMenu, FALSE, FALSE);
            return 0;
        }

        case CM_CONTEXTMENU:
        { // Tests for the panel type are in ShellAction
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // Save the selection for the Restore Selection command
            ShellAction(activePanel, saContextMenu, TRUE, FALSE);
            return 0;
        }

        case CM_CALCDIRSIZES:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CalculateDirSizes();
            return 0;
        }

        case CM_RENAMEFILE:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->RenameFile();
            return 0;
        }

        case CM_CHANGEATTR:
        {
            if (EnablerChangeAttrs)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ChangeAttr();
            }
            return 0;
        }

        case CM_CONVERTFILES:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command

                // if no item is selected, we will select the one under focus and save its name
                char temporarySelected[MAX_PATH];
                activePanel->SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

                activePanel->Convert();

                // if we have selected an item, we will deselect it again
                activePanel->UnselectItemWithName(temporarySelected);
            }
            return 0;
        }

        case CM_COMPRESS:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ChangeAttr(TRUE, TRUE);
            }
            return 0;
        }

        case CM_UNCOMPRESS:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ChangeAttr(TRUE, FALSE);
            }
            return 0;
        }

        case CM_ENCRYPT:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ChangeAttr(FALSE, FALSE, TRUE, TRUE);
            }
            return 0;
        }

        case CM_DECRYPT:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ChangeAttr(FALSE, FALSE, TRUE, FALSE);
            }
            return 0;
        }

        case CM_PACK:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->Pack(GetNonActivePanel());
            }
            return 0;
        }

        case CM_UNPACK:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->Unpack(GetNonActivePanel());
            }
            return 0;
        }

        case CM_AFOCUSSHORTCUT:
        {
            if (EnablerFileOrDirLinkOnDisk) // enabler for activePanel
            {
                //            activePanel->UserWorkedOnThisPath = TRUE; // this is navigation, we won't dirty the path
                activePanel->FocusShortcutTarget(activePanel);
            }
            return 0;
        }

        case CM_PROPERTIES:
        {
            if (EnablerShowProperties)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                ShellAction(activePanel, saProperties, TRUE, FALSE);
            }
            return 0;
        }

        case CM_OPEN:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CtrlPageDnOrEnter(VK_RETURN);
            return 0;
        }

        case CM_VIEW:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->ViewFile(NULL, FALSE, 0xFFFFFFFF, activePanel->Is(ptDisk) ? activePanel->EnumFileNamesSourceUID : -1, -1);
            return 0;
        }

        case CM_ALTVIEW:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->ViewFile(NULL, TRUE, 0xFFFFFFFF, activePanel->Is(ptDisk) ? activePanel->EnumFileNamesSourceUID : -1, -1);
            return 0;
        }

        case CM_VIEW_WITH:
        {
            POINT menuPos;
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->GetContextMenuPos(&menuPos);
            activePanel->ViewFileWith(NULL, HWindow, &menuPos, NULL,
                                      activePanel->Is(ptDisk) ? activePanel->EnumFileNamesSourceUID : -1, -1);
            return 0;
        }

        case CM_EDIT:
        {
            if (EnablerFileOnDiskOrArchive)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                if (activePanel->Is(ptZIPArchive))
                {
                    int index = activePanel->GetCaretIndex();
                    if (index >= activePanel->Dirs->Count &&
                        index < activePanel->Dirs->Count + activePanel->Files->Count)
                    {
                        activePanel->ExecuteFromArchive(index, TRUE);
                    }
                }
                else
                    activePanel->EditFile(NULL);
            }
            return 0;
        }

        case CM_EDITNEW:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->EditNewFile();
            }
            return 0;
        }

        case CM_EDIT_WITH:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            POINT menuPos;
            activePanel->GetContextMenuPos(&menuPos);
            if (activePanel->Is(ptDisk))
            {
                activePanel->EditFileWith(NULL, HWindow, &menuPos);
            }
            else
            {
                if (activePanel->Is(ptZIPArchive))
                {
                    int index = activePanel->GetCaretIndex();
                    if (index >= activePanel->Dirs->Count &&
                        index < activePanel->Dirs->Count + activePanel->Files->Count)
                    {
                        activePanel->ExecuteFromArchive(index, TRUE, HWindow, &menuPos);
                    }
                }
            }
            return 0;
        }

        case CM_FINDFILE:
        {
            if (activePanel->Is(ptDisk)) // Does Find have a relationship with the current path? (for archives and file systems not yet)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
            }

            activePanel->FindFile();
            return 0;
        }

        case CM_DRIVEINFO:
        {
            activePanel->DriveInfo();
            return 0;
        }

        case CM_CREATEDIR:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->CreateDir(GetNonActivePanel());
            return 0;
        }

        case CM_ACTIVE_CHANGEDIR:
        {
            activePanel->ChangeDir();
            return 0;
        }

        case CM_LEFT_CHANGEDIR:
        {
            LeftPanel->ChangeDir();
            return 0;
        }

        case CM_RIGHT_CHANGEDIR:
        {
            RightPanel->ChangeDir();
            return 0;
        }

        case CM_ACTIVE_AS_OTHER:
        {
            activePanel->ChangePathToOtherPanelPath();
            return 0;
        }

        case CM_LEFT_AS_OTHER:
        {
            LeftPanel->ChangePathToOtherPanelPath();
            return 0;
        }

        case CM_RIGHT_AS_OTHER:
        {
            RightPanel->ChangePathToOtherPanelPath();
            return 0;
        }

        case CM_ACTIVESELECTALL:
        {
            activePanel->SelectUnselect(TRUE, TRUE, FALSE);
            return 0;
        }

        case CM_ACTIVEUNSELECTALL:
        {
            activePanel->SelectUnselect(TRUE, FALSE, FALSE);
            return 0;
        }

        case CM_ACTIVESELECT:
        {
            activePanel->SelectUnselect(FALSE, TRUE, TRUE);
            return 0;
        }

        case CM_ACTIVEUNSELECT:
        {
            activePanel->SelectUnselect(FALSE, FALSE, TRUE);
            return 0;
        }

        case CM_ACTIVEINVERTSEL:
        {
            activePanel->InvertSelection(FALSE);
            return 0;
        }

        case CM_ACTIVEINVERTSELALL:
        {
            activePanel->InvertSelection(TRUE);
            return 0;
        }

        case CM_RESELECT:
        {
            activePanel->Reselect();
            return 0;
        }

        case CM_SELECTBYFOCUSEDNAME:
        {
            activePanel->SelectUnselectByFocusedItem(TRUE, TRUE);
            return 0;
        }

        case CM_UNSELECTBYFOCUSEDNAME:
        {
            activePanel->SelectUnselectByFocusedItem(FALSE, TRUE);
            return 0;
        }

        case CM_SELECTBYFOCUSEDEXT:
        {
            activePanel->SelectUnselectByFocusedItem(TRUE, FALSE);
            return 0;
        }

        case CM_UNSELECTBYFOCUSEDEXT:
        {
            activePanel->SelectUnselectByFocusedItem(FALSE, FALSE);
            return 0;
        }

        case CM_HIDE_SELECTED_NAMES:
        {
            activePanel->ShowHideNames(1); // hide selected
            return 0;
        }

        case CM_HIDE_UNSELECTED_NAMES:
        {
            activePanel->ShowHideNames(2); // hide unselected
            return 0;
        }

        case CM_SHOW_ALL_NAME:
        {
            activePanel->ShowHideNames(0); // show all
            return 0;
        }

        case CM_STORESEL:
        {
            activePanel->StoreGlobalSelection();
            return 0;
        }

        case CM_RESTORESEL:
        {
            activePanel->RestoreGlobalSelection();
            return 0;
        }

        case CM_GOTO_PREV_SEL:
        case CM_GOTO_NEXT_SEL:
        {
            activePanel->GotoSelectedItem(LOWORD(wParam) == CM_GOTO_NEXT_SEL);
            return 0;
        }

        case CM_COMPAREDIRS:
        {
            // Currently, we can only handle ptDisk<->ptDisk, ptDisk<->ptZIPArchive, and ptZIPArchive<->ptZIPArchive
            //if (LeftPanel->Is(ptPluginFS) || RightPanel->Is(ptPluginFS))
            //{
            //  SalMessageBox(HWindow, LoadStr(IDS_COMPARE_FS), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONINFORMATION);
            //  return 0;
            //}

            // if both panels lead to the same path, we will fall out
            char leftPath[2 * MAX_PATH];
            char rightPath[2 * MAX_PATH];
            LeftPanel->GetGeneralPath(leftPath, 2 * MAX_PATH);
            RightPanel->GetGeneralPath(rightPath, 2 * MAX_PATH);
            if (strcmp(leftPath, rightPath) == 0) // case sensitive, if this condition fails, it doesn't matter
            {
                SalMessageBox(HWindow, LoadStr(IDS_COMPARE_SAMEPATH), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            BOOL enableByDateAndTime = (LeftPanel->ValidFileData & (VALID_DATA_DATE | VALID_DATA_PL_DATE)) &&
                                       (RightPanel->ValidFileData & (VALID_DATA_DATE | VALID_DATA_PL_DATE));
            BOOL enableBySize = (LeftPanel->ValidFileData & (VALID_DATA_SIZE | VALID_DATA_PL_SIZE)) &&
                                (RightPanel->ValidFileData & (VALID_DATA_SIZE | VALID_DATA_PL_SIZE));
            BOOL enableByAttrs = (LeftPanel->ValidFileData & VALID_DATA_ATTRIBUTES) &&
                                 (RightPanel->ValidFileData & VALID_DATA_ATTRIBUTES);
            BOOL enableByContent = LeftPanel->Is(ptDisk) && RightPanel->Is(ptDisk);
            BOOL enableSubdirs = !LeftPanel->Is(ptPluginFS) && !RightPanel->Is(ptPluginFS);
            BOOL enableCompAttrsOfSubdirs = enableSubdirs && enableByAttrs;
            CCompareDirsDialog dlg(HWindow, enableByDateAndTime, enableBySize, enableByAttrs,
                                   enableByContent, enableSubdirs, enableCompAttrsOfSubdirs,
                                   LeftPanel, RightPanel);
            if (dlg.Execute() == IDOK)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                DWORD flags = 0;
                if (enableByDateAndTime && Configuration.CompareByTime)
                    flags |= COMPARE_DIRECTORIES_BYTIME;
                if (enableBySize && Configuration.CompareBySize)
                    flags |= COMPARE_DIRECTORIES_BYSIZE;
                if (enableByContent && Configuration.CompareByContent)
                    flags |= COMPARE_DIRECTORIES_BYCONTENT;
                if (enableByAttrs && Configuration.CompareByAttr)
                    flags |= COMPARE_DIRECTORIES_BYATTR;
                if (enableSubdirs && Configuration.CompareSubdirs)
                    flags |= COMPARE_DIRECTORIES_SUBDIRS;
                else
                {
                    if (Configuration.CompareOnePanelDirs)
                    {
                        flags |= COMPARE_DIRECTORIES_ONEPANELDIRS;
                        Configuration.CompareSubdirs = FALSE; // handles the situation when CompareSubdirs is turned on and comparison for FS is started and the user enables CompareOnePanelDirs - without this line, when opening the disk dialog again, CompareSubdirs takes precedence over CompareOnePanelDirs, which is not entirely kosher...
                    }
                }
                if (enableCompAttrsOfSubdirs && Configuration.CompareSubdirsAttr)
                    flags |= COMPARE_DIRECTORIES_SUBDIRS_ATTR;
                if (Configuration.CompareIgnoreFiles)
                    flags |= COMPARE_DIRECTORIES_IGNFILENAMES;
                if ((enableSubdirs && Configuration.CompareSubdirs || Configuration.CompareOnePanelDirs) &&
                    Configuration.CompareIgnoreDirs)
                    flags |= COMPARE_DIRECTORIES_IGNDIRNAMES;
                CompareDirectories(flags);
            }
            return 0;
        }

        case CM_EXIT:
        {
            PostMessage(HWindow, WM_USER_CLOSE_MAINWND, 0, 0);
            return 0;
        }

        case CM_CONNECTNET:
        {
            activePanel->ConnectNet(FALSE);
            return 0;
        }

        case CM_DISCONNECTNET:
        {
            activePanel->DisconnectNet();
            return 0;
        }

        case CM_FILEHISTORY:
        {
            if (!FileHistory->HasItem())
                return 0;
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            BeginStopRefresh(); // He was snoring in his sleep

            RECT r;
            GetWindowRect(HWindow, &r);
            int x = r.left + (r.right - r.left) / 2;
            int y = r.top + (r.bottom - r.top) / 2;

            CMenuPopup menu;
            FileHistory->FillPopupMenu(&menu);
            DWORD cmd = menu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_CENTERALIGN | MENU_TRACK_VCENTERALIGN,
                                   x, y, HWindow, NULL);
            if (cmd != 0)
                FileHistory->Execute(cmd);

            EndStopRefresh(); // now he's sniffling again, he'll start up

            return 0;
        }

        case CM_DIRHISTORY:
        {
            activePanel->OpenDirHistory();
            return 0;
        }

        case CM_USERMENU:
        {
            if (activePanel->Is(ptDisk))
            {
                BeginStopRefresh(); // we do not need any refreshes

                MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

                UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
                CMenuPopup menu;
                FillUserMenu(&menu);
                POINT p;
                activePanel->GetContextMenuPos(&p);
                // the next round of locking (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) will be
                // in WM_USER_ENTERMENULOOP+WM_USER_LEAVEMENULOOP, but that's already nested, no overhead,
                // so we ignore it, we will not fight against it in any way
                menu.Track(0, p.x, p.y, HWindow, NULL);
                UserMenuIconBkgndReader.EndUserMenuIconsInUse();

                EndStopRefresh();
            }
            return 0;
        }

        case CM_OPENHOTPATHS:
        {
            BeginStopRefresh(); // we do not need any refreshes

            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            RECT r;
            GetWindowRect(GetActivePanelHWND(), &r);
            int dirHeight = GetDirectoryLineHeight();

            CMenuPopup menu;
            HotPaths.FillHotPathsMenu(&menu, CM_ACTIVEHOTPATH_MIN);
            menu.Track(0, r.left, r.top + dirHeight, HWindow, NULL);

            EndStopRefresh();
            return 0;
        }

        case CM_CUSTOMIZE_HOTPATHS:
        {
            PostMessage(HWindow, WM_USER_CONFIGURATION, 1, -1);
            return 0;
        }

        case CM_CUSTOMIZE_USERMENU:
        {
            PostMessage(HWindow, WM_USER_CONFIGURATION, 2, 0);
            return 0;
        }

        case CM_EDITLINE:
        {
            if (SystemPolicies.GetNoRun())
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = HWindow;
                params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
                params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
                params.Text = LoadStr(IDS_POLICIESRESTRICTION);
                params.ContextHelpId = IDH_GROUPPOLICY;
                params.HelpCallback = MessageBoxHelpCallback;
                SalMessageBoxEx(&params);
                return 0;
            }
            if (EditWindow->HWindow != NULL)
            {
                if (EditWindow->IsEnabled())
                    SetFocus(EditWindow->HWindow);
            }
            else
            {
                if (EditPermanentVisible || EditWindow->IsEnabled()) // archive can be in the panel
                    ShowCommandLine();
            }
            return 0;
        }

        case CM_TOGGLEEDITLINE:
        {
            if (SystemPolicies.GetNoRun())
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = HWindow;
                params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
                params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
                params.Text = LoadStr(IDS_POLICIESRESTRICTION);
                params.ContextHelpId = IDH_GROUPPOLICY;
                params.HelpCallback = MessageBoxHelpCallback;
                SalMessageBoxEx(&params);
                return 0;
            }
            EditPermanentVisible = !EditPermanentVisible;
            if (EditWindow->HWindow != NULL && !EditPermanentVisible)
                HideCommandLine();
            else if (EditWindow->HWindow == NULL)
            {
                if (EditPermanentVisible)
                {
                    ShowCommandLine();
                    if (lParam == 0)
                        SetFocus(EditWindow->HWindow);
                }
            }
            return 0;
        }

        case CM_TOGGLETOPTOOLBAR:
        {
            ToggleTopToolBar();
            //          LayoutWindows();
            break;
        }

        case CM_TOGGLEPLUGINSBAR:
        {
            TogglePluginsBar();
            break;
        }

        case CM_TOGGLEMIDDLETOOLBAR:
        {
            ToggleMiddleToolBar();
            InvalidateRect(HWindow, NULL, FALSE);
            LayoutWindows();
            break;
        }

        case CM_TOGGLEUSERMENUTOOLBAR:
        {
            ToggleUserMenuToolBar();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                                      //          LayoutWindows();
            break;
        }

        case CM_TOGGLEHOTPATHSBAR:
        {
            ToggleHotPathsBar();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                                      //          LayoutWindows();
            break;
        }

        case CM_TOGGLEDRIVEBAR:
        case CM_TOGGLEDRIVEBAR2:
        {
            ToggleDriveBar(LOWORD(wParam) == CM_TOGGLEDRIVEBAR2);
            //          LayoutWindows();
            break;
        }

        case CM_TOGGLEBOTTOMTOOLBAR:
        {
            ToggleBottomToolBar();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            LayoutWindows();
            break;
        }

        case CM_TOGGLE_UMLABELS:
        {
            UMToolBar->ToggleLabels();
            break;
        }

            //        case CM_TOGGLE_HPLABELS:
            //        {
            //          HPToolBar->ToggleLabels();
            //          break;
            //        }

        case CM_TOGGLE_GRIPS:
        {
            ToggleToolBarGrips();
            break;
        }

        case CM_CUSTOMIZETOP:
        {
            if (TopToolBar->HWindow == NULL)
            {
                ToggleTopToolBar();
                IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                LayoutWindows();
            }
            TopToolBar->Customize();
            break;
        }

        case CM_CUSTOMIZEPLUGINS:
        {
            if (PluginsBar->HWindow == NULL)
            {
                TogglePluginsBar();
                LayoutWindows();
            }
            // let's open the Plugins Manager
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_PLUGINS, 0);
            break;
        }

        case CM_CUSTOMIZEMIDDLE:
        {
            if (MiddleToolBar->HWindow == NULL)
            {
                ToggleMiddleToolBar();
                IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                LayoutWindows();
            }
            MiddleToolBar->Customize();
            break;
        }

        case CM_CUSTOMIZEUM:
        {
            if (UMToolBar->HWindow == NULL)
            {
                ToggleUserMenuToolBar();
                IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                LayoutWindows();
            }
            // Let's unpack the UserMenu page and edit the index item
            PostMessage(HWindow, WM_USER_CONFIGURATION, 2, 0);
            break;
        }

        case CM_CUSTOMIZEHP:
        {
            if (HPToolBar->HWindow == NULL)
            {
                ToggleHotPathsBar();
                IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                LayoutWindows();
            }
            // let's unpack the HotPaths page
            PostMessage(HWindow, WM_USER_CONFIGURATION, 1, -1);
            break;
        }

        case CM_CUSTOMIZELEFT:
        {
            if (LeftPanel->DirectoryLine->HWindow == NULL)
                LeftPanel->ToggleDirectoryLine();
            if (LeftPanel->DirectoryLine->ToolBar != NULL)
                LeftPanel->DirectoryLine->ToolBar->Customize();
            break;
        }

        case CM_CUSTOMIZERIGHT:
        {
            if (RightPanel->DirectoryLine->HWindow == NULL)
                RightPanel->ToggleDirectoryLine();
            if (RightPanel->DirectoryLine->ToolBar != NULL)
                RightPanel->DirectoryLine->ToolBar->Customize();
            break;
        }

        case CM_DOSSHELL:
        {
            activePanel->UserWorkedOnThisPath = TRUE;

            char cmd[MAX_PATH];
            if (!GetEnvironmentVariable("COMSPEC", cmd, MAX_PATH))
                cmd[0] = 0;

            if (SystemPolicies.GetNoRun() ||
                (SystemPolicies.GetMyRunRestricted() && !SystemPolicies.GetMyCanRun(cmd)))
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = HWindow;
                params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
                params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
                params.Text = LoadStr(IDS_POLICIESRESTRICTION);
                params.ContextHelpId = IDH_GROUPPOLICY;
                params.HelpCallback = MessageBoxHelpCallback;
                SalMessageBoxEx(&params);
                return 0;
            }

            AddDoubleQuotesIfNeeded(cmd, MAX_PATH); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)

            SetDefaultDirectories();

            STARTUPINFO si;
            memset(&si, 0, sizeof(STARTUPINFO));
            si.cb = sizeof(STARTUPINFO);
            si.lpTitle = LoadStr(IDS_COMMANDSHELL);
            // there is an undocumented flag 0x400, when we pass the monitor handle to si.hStdOutput
            // Unfortunately, it works with SOL.EXE, but not with CMD.EXE, so we're going the old way
            // through the construction of a dummy window
            // Under w2k, they call it #define STARTF_HASHMONITOR 0x00000400 // same as HASSHELLDATA
            // I found STARTF_MONITOR on the net in some article about undocumented functions
            si.dwFlags = STARTF_USESHOWWINDOW;
            POINT p;
            if (MultiMonGetDefaultWindowPos(MainWindow->HWindow, &p))
            {
                // if the main window is on a different monitor, we should also open it there
                // window being created and preferably at the default position (same as on primary)
                si.dwFlags |= STARTF_USEPOSITION;
                si.dwX = p.x;
                si.dwY = p.y;
                // TRACE_I("MultiMonGetDefaultWindowPos(): x = " << p.x << ", y = " << p.y);
            }
            si.wShowWindow = SW_SHOWNORMAL;

            PROCESS_INFORMATION pi;

            if (!HANDLES(CreateProcess(NULL, cmd, NULL, NULL, FALSE,
                                       CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL,
                                       (activePanel->Is(ptDisk) || activePanel->Is(ptZIPArchive)) ? activePanel->GetPath() : NULL, &si, &pi)))
            {
                DWORD err = GetLastError();
                SalMessageBox(HWindow, GetErrorText(err),
                              LoadStr(IDS_ERROREXECPROMPT), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                HANDLES(CloseHandle(pi.hProcess));
                HANDLES(CloseHandle(pi.hThread));
            }

            return 0;
        }

        case CM_FILELIST:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // Save the selection for the Restore Selection command
            MakeFileList();
            return 0;
        }

        case CM_OPENACTUALFOLDER:
        {
            activePanel->OpenActiveFolder();
            return 0;
        }

        case CM_SWAPPANELS:
        {
            // swap panels
            CFilesWindow* swap = LeftPanel;
            LeftPanel = RightPanel;
            RightPanel = swap;
            // swap toolbar records
            char buff[1024];
            lstrcpy(buff, Configuration.LeftToolBar);
            lstrcpy(Configuration.LeftToolBar, Configuration.RightToolBar);
            lstrcpy(Configuration.RightToolBar, buff);
            // we will set variables for the panels and let the toolbars load
            LeftPanel->DirectoryLine->SetLeftPanel(TRUE);
            RightPanel->DirectoryLine->SetLeftPanel(FALSE);
            // icon must be changed in the imagelist
            LeftPanel->UpdateDriveIcon(FALSE);
            RightPanel->UpdateDriveIcon(FALSE);

            // if the ZOOMed panel was active, after Ctrl+U the minimized panel would remain active
            if (GetActivePanel() == LeftPanel && IsPanelZoomed(FALSE) ||
                GetActivePanel() == RightPanel && IsPanelZoomed(TRUE))
            {
                // we then activate the visible one
                ChangePanel(TRUE);
            }

            LockWindowUpdate(HWindow);
            LayoutWindows();
            LockWindowUpdate(NULL);

            // let's load the columns again (column widths are not swapped)
            LeftPanel->SelectViewTemplate(LeftPanel->GetViewTemplateIndex(), TRUE, FALSE);
            RightPanel->SelectViewTemplate(RightPanel->GetViewTemplateIndex(), TRUE, FALSE);

            // we will also distribute this news among the plug-ins
            Plugins.Event(PLUGINEVENT_PANELSSWAPPED, 0);

            return 0;
        }

        case CM_OPENRECYCLEBIN:
        {
            OpenSpecFolder(HWindow, CSIDL_BITBUCKET);
            return 0;
        }

        case CM_OPENCONROLPANEL:
        {
            OpenSpecFolder(HWindow, CSIDL_CONTROLS);
            return 0;
        }

        case CM_OPENDESKTOP:
        {
            OpenSpecFolder(HWindow, CSIDL_DESKTOP);
            return 0;
        }

        case CM_OPENMYCOMP:
        {
            OpenSpecFolder(HWindow, CSIDL_DRIVES);
            return 0;
        }

        case CM_OPENFONTS:
        {
            OpenSpecFolder(HWindow, CSIDL_FONTS);
            return 0;
        }

        case CM_OPENNETNEIGHBOR:
        {
            OpenSpecFolder(HWindow, CSIDL_NETWORK);
            return 0;
        }

        case CM_OPENPRINTERS:
        {
            OpenSpecFolder(HWindow, CSIDL_PRINTERS);
            return 0;
        }

        case CM_OPENDESKTOPDIR:
        {
            OpenSpecFolder(HWindow, CSIDL_DESKTOPDIRECTORY);
            return 0;
        }

        case CM_OPENPERSONAL:
        {
            OpenSpecFolder(HWindow, CSIDL_PERSONAL);
            return 0;
        }

        case CM_OPENPROGRAMS:
        {
            OpenSpecFolder(HWindow, CSIDL_PROGRAMS);
            return 0;
        }

        case CM_OPENRECENT:
        {
            OpenSpecFolder(HWindow, CSIDL_RECENT);
            return 0;
        }

        case CM_OPENSENDTO:
        {
            OpenSpecFolder(HWindow, CSIDL_SENDTO);
            return 0;
        }

        case CM_OPENSTARTMENU:
        {
            OpenSpecFolder(HWindow, CSIDL_STARTMENU);
            return 0;
        }

        case CM_OPENSTARTUP:
        {
            OpenSpecFolder(HWindow, CSIDL_STARTUP);
            return 0;
        }

        case CM_OPENTEMPLATES:
        {
            OpenSpecFolder(HWindow, CSIDL_TEMPLATES);
            return 0;
        }

        case CM_CLIPCOPY:
        {
            if (activePanel->Is(ptDisk) || activePanel->Is(ptZIPArchive))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ClipboardCopy();
            }
            return 0;
        }

        case CM_CLIPCUT:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                activePanel->ClipboardCut();
            }
            return 0;
        }

        case CM_CLIPPASTE:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            if (!activePanel->Is(ptDisk) || !activePanel->ClipboardPaste()) // trying to paste files to disk
            {
                if (!activePanel->Is(ptZIPArchive) && !activePanel->Is(ptPluginFS) ||
                    !activePanel->ClipboardPasteToArcOrFS(FALSE, NULL)) // try to paste files into an archive or filesystem
                {
                    activePanel->ClipboardPastePath(); // or change the current path
                }
            }
            return 0;
        }

        case CM_CLIPPASTELINKS:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->ClipboardPasteLinks();
            }
            return 0;
        }

        case CM_TOGGLEELASTICSMART:
        {
            ToggleSmartColumnMode(activePanel);
            return 0;
        }

        case CM_TOGGLEHIDDENFILES:
        {
            Configuration.NotHiddenSystemFiles = !Configuration.NotHiddenSystemFiles;
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            int t2 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            SendMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2);

            // we will also distribute this news among the plug-ins
            Plugins.Event(PLUGINEVENT_CONFIGURATIONCHANGED, 0);
            return 0;
        }

        case CM_SEC_PERMISSIONS:
        {
            if (EnablerPermissions)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                ShellAction(activePanel, saPermissions, TRUE, FALSE);
            }
            return 0;
        }

        case CM_ACTIVEZOOMPANEL:
        case CM_LEFTZOOMPANEL:
        case CM_RIGHTZOOMPANEL:
        {
            if (IsPanelZoomed(TRUE) || IsPanelZoomed(FALSE))
            {
                SplitPosition = BeforeZoomSplitPosition;
                // we prefer to protect ourselves from a bad value in BeforeZoomSplitPosition
                if (IsPanelZoomed(TRUE) || IsPanelZoomed(FALSE))
                    SplitPosition = 0.5;
            }
            else
            {
                BeforeZoomSplitPosition = SplitPosition;
                if (LOWORD(wParam) == CM_ACTIVEZOOMPANEL)
                {
                    if (activePanel == LeftPanel)
                        SplitPosition = 1.0;
                    else
                        SplitPosition = 0.0;
                }
                else
                {
                    if (LOWORD(wParam) == CM_LEFTZOOMPANEL)
                        SplitPosition = 1.0;
                    else
                        SplitPosition = 0.0;
                }
            }
            LayoutWindows();
            FocusPanel(GetActivePanel());
            return 0;
        }

        case CM_FULLSCREEN:
        {
            if (IsZoomed(HWindow))
                ShowWindow(HWindow, SW_RESTORE);
            else
                ShowWindow(HWindow, SW_MAXIMIZE);
            return 0;
        }
        }
        break;
    }

    case WM_USER_DISPACHCHANGENOTIF:
    {
        if (LastDispachChangeNotifTime < lParam) // it's not about an old message
        {
            if (AlreadyInPlugin || StopRefresh > 0)
                NeedToResentDispachChangeNotif = TRUE;
            else
            {
                char path[MAX_PATH];
                BOOL includingSubdirs;
                BOOL ok = TRUE;
                while (1)
                {
                    HANDLES(EnterCriticalSection(&DispachChangeNotifCS));
                    if (ChangeNotifArray.Count > 0)
                    {
                        CChangeNotifData* item = &ChangeNotifArray[ChangeNotifArray.Count - 1];
                        strcpy(path, item->Path);
                        includingSubdirs = item->IncludingSubdirs;
                        ChangeNotifArray.Delete(ChangeNotifArray.Count - 1);
                        if (!ChangeNotifArray.IsGood())
                        {
                            ChangeNotifArray.ResetState();
                            ChangeNotifArray.DestroyMembers();
                            ChangeNotifArray.ResetState();
                            ok = FALSE;
                        }
                    }
                    else
                        ok = FALSE;
                    if (!ok) // we will save the time of the last refresh (still in the critical section)
                    {
                        HANDLES(EnterCriticalSection(&TimeCounterSection));
                        LastDispachChangeNotifTime = MyTimeCounter++;
                        HANDLES(LeaveCriticalSection(&TimeCounterSection));
                    }
                    HANDLES(LeaveCriticalSection(&DispachChangeNotifCS));

                    if (ok) // send a message about the change to 'path' with 'includingSubdirs'
                    {
                        // send a message to all loaded plugins
                        Plugins.AcceptChangeOnPathNotification(path, includingSubdirs);

                        if (GetNonActivePanel() != NULL) // first inactive (due to time changes in subdirectories on NTFS)
                        {
                            GetNonActivePanel()->AcceptChangeOnPathNotification(path, includingSubdirs);
                        }
                        if (GetActivePanel() != NULL) // then active panel
                        {
                            GetActivePanel()->AcceptChangeOnPathNotification(path, includingSubdirs);
                        }

                        if (DetachedFSList->Count > 0)
                        {
                            // For optimizing input/output to the plugin, the section is EnterPlugin+LeavePlugin
                            // exported up to here (not in the encapsulation of the interface)
                            EnterPlugin();
                            int i;
                            for (i = 0; i < DetachedFSList->Count; i++)
                            {
                                CPluginFSInterfaceEncapsulation* fs = DetachedFSList->At(i);
                                fs->AcceptChangeOnPathNotification(fs->GetPluginFSName(), path, includingSubdirs);
                            }
                            LeavePlugin();
                        }
                    }
                    else
                        break; // end of loop
                }
            }
        }
        return 0;
    }

    case WM_USER_DISPACHCFGCHANGE:
    {
        // send a message about configuration changes between plug-ins
        Plugins.Event(PLUGINEVENT_CONFIGURATIONCHANGED, 0);
        return 0;
    }

    case WM_USER_TBCHANGED:
    {
        HWND hToolBar = (HWND)wParam;
        if (TopToolBar != NULL && hToolBar == TopToolBar->HWindow)
        {
            TopToolBar->Save(Configuration.TopToolBar);
        }
        if (MiddleToolBar != NULL && hToolBar == MiddleToolBar->HWindow)
        {
            MiddleToolBar->Save(Configuration.MiddleToolBar);
        }
        if (LeftPanel->DirectoryLine->ToolBar != NULL && hToolBar == LeftPanel->DirectoryLine->ToolBar->HWindow)
        {
            LeftPanel->DirectoryLine->LayoutWindow();
            LeftPanel->DirectoryLine->ToolBar->Save(Configuration.LeftToolBar);
        }
        if (RightPanel->DirectoryLine->ToolBar != NULL && hToolBar == RightPanel->DirectoryLine->ToolBar->HWindow)
        {
            RightPanel->DirectoryLine->LayoutWindow();
            RightPanel->DirectoryLine->ToolBar->Save(Configuration.RightToolBar);
        }
        return FALSE; // we don't have any buttons
    }

    case WM_USER_TBENUMBUTTON2:
    {
        HWND hToolBar = (HWND)wParam;
        // we will pass it to our toolbar
        if (TopToolBar != NULL && hToolBar == TopToolBar->HWindow)
            return TopToolBar->OnEnumButton(lParam);
        if (MiddleToolBar != NULL && hToolBar == MiddleToolBar->HWindow)
            return MiddleToolBar->OnEnumButton(lParam);
        if (LeftPanel->DirectoryLine->ToolBar != NULL && hToolBar == LeftPanel->DirectoryLine->ToolBar->HWindow)
            return LeftPanel->DirectoryLine->ToolBar->OnEnumButton(lParam);
        if (RightPanel->DirectoryLine->ToolBar != NULL && hToolBar == RightPanel->DirectoryLine->ToolBar->HWindow)
            return RightPanel->DirectoryLine->ToolBar->OnEnumButton(lParam);
        return FALSE; // we don't have any buttons
    }

    case WM_USER_TBRESET:
    {
        HWND hToolBar = (HWND)wParam;
        // we will pass it to our toolbar
        if (TopToolBar != NULL && hToolBar == TopToolBar->HWindow)
            TopToolBar->OnReset();
        if (MiddleToolBar != NULL && hToolBar == MiddleToolBar->HWindow)
            MiddleToolBar->OnReset();
        if (LeftPanel->DirectoryLine->ToolBar != NULL && hToolBar == LeftPanel->DirectoryLine->ToolBar->HWindow)
            LeftPanel->DirectoryLine->ToolBar->OnReset();
        if (RightPanel->DirectoryLine->ToolBar != NULL && hToolBar == RightPanel->DirectoryLine->ToolBar->HWindow)
            RightPanel->DirectoryLine->ToolBar->OnReset();
        return FALSE; // we don't have any buttons
    }

    case WM_USER_TBGETTOOLTIP:
    {
        HWND hToolBar = (HWND)wParam;
        // we will pass it to our toolbar
        if (TopToolBar != NULL && hToolBar == TopToolBar->HWindow)
            TopToolBar->OnGetToolTip(lParam);
        if (MiddleToolBar != NULL && hToolBar == MiddleToolBar->HWindow)
            MiddleToolBar->OnGetToolTip(lParam);
        if (PluginsBar != NULL && hToolBar == PluginsBar->HWindow)
            PluginsBar->OnGetToolTip(lParam);
        if (UMToolBar != NULL && hToolBar == UMToolBar->HWindow)
            UMToolBar->OnGetToolTip(lParam);
        if (HPToolBar != NULL && hToolBar == HPToolBar->HWindow)
            HPToolBar->OnGetToolTip(lParam);
        if (DriveBar != NULL && hToolBar == DriveBar->HWindow)
            DriveBar->OnGetToolTip(lParam);
        if (DriveBar2 != NULL && hToolBar == DriveBar2->HWindow)
            DriveBar2->OnGetToolTip(lParam);
        if (LeftPanel->DirectoryLine->ToolBar != NULL && hToolBar == LeftPanel->DirectoryLine->ToolBar->HWindow)
            LeftPanel->DirectoryLine->ToolBar->OnGetToolTip(lParam);
        if (RightPanel->DirectoryLine->ToolBar != NULL && hToolBar == RightPanel->DirectoryLine->ToolBar->HWindow)
            RightPanel->DirectoryLine->ToolBar->OnGetToolTip(lParam);
        if (BottomToolBar != NULL && hToolBar == BottomToolBar->HWindow)
            BottomToolBar->OnGetToolTip(lParam);
        return FALSE; // we don't have any buttons
    }

    case WM_USER_TBENDADJUST:
    {
        // some of the toolbar was configured - force update
        IdleForceRefresh = TRUE;
        IdleRefreshStates = TRUE;
        return 0;
    }

    case WM_USER_LEAVEMENULOOP2:
    {
        // This message comes after the command, so any command from the new menu is already processed
        if (ContextMenuNew != NULL)
            ContextMenuNew->Release();
        return 0;
    }

    case WM_USER_UNINITMENUPOPUP:
    {
        CMenuPopup* popup = (CMenuPopup*)(CGUIMenuPopupAbstract*)wParam;
        WORD popupID = HIWORD(lParam);

        switch (popupID)
        {
        case CML_OPTIONS_PLUGINS:
        case CML_HELP_ABOUTPLUGINS:
        case CML_PLUGINS:
        case CML_PLUGINS_SUBMENU:
        case CML_FILES_VIEWWITH:
        {
            HIMAGELIST hIcons = popup->GetImageList();
            if (hIcons != NULL)
            {
                popup->SetImageList(NULL); // just to be sure, make sure the popup does not own an invalid handle
                ImageList_Destroy(hIcons);
            }
            hIcons = popup->GetHotImageList();
            if (hIcons != NULL)
            {
                popup->SetHotImageList(NULL); // just to be sure, make sure the popup does not own an invalid handle
                ImageList_Destroy(hIcons);
            }
            if (popupID == CML_PLUGINS) // zavirame Plugins menu, dynamicky ikony uz muzeme zahodit (buildi se znovu pred kazdym dalsim otevrenim menu)
                Plugins.ReleasePluginDynMenuIcons();
            break;
        }

        case CML_FILES_NEW:
        {
            popup->SetTemplateMenu(NULL);
            EndStopRefresh(); // we close in WM_USER_UNINITMENUPOPUP/WM_USER_INITMENUPOPUP
            break;
        }
        }
        return 0;
    }

    case WM_USER_INITMENUPOPUP:
    {
        CMenuPopup* popup = (CMenuPopup*)(CGUIMenuPopupAbstract*)wParam;
        WORD popupID = HIWORD(lParam);

        switch (popupID)
        {
        case CML_LEFT:
        case CML_RIGHT:
        {
            BOOL left = popupID == CML_LEFT;

            popup->CheckItem(left ? CM_LCHANGEFILTER : CM_RCHANGEFILTER, FALSE,
                             (left ? LeftPanel : RightPanel)->FilterEnabled);

            DWORD firstID = left ? CML_LEFT_VIEWS1 : CML_RIGHT_VIEWS1;
            DWORD lastID = left ? CML_LEFT_VIEWS2 : CML_RIGHT_VIEWS2;
            // search for separator above and below views
            int firstIndex = popup->FindItemPosition(firstID);
            int lastIndex = popup->FindItemPosition(lastID);
            if (firstIndex == -1 || lastIndex == -1)
            {
                TRACE_E("Requested items were not found");
            }
            else
            {
                // remove the existing content
                if (firstIndex + 1 < lastIndex - 1)
                    popup->RemoveItemsRange(firstIndex + 1, lastIndex - 1);

                // populate the list of views
                FillViewModeMenu(popup, firstIndex + 1, left ? 1 : 2);
            }
            break;
        }

        case CML_LEFT_GO:
        case CML_RIGHT_GO:
        {
            static int GO_ITEMS_COUNT = -1;

            int count = popup->GetItemCount();
            if (GO_ITEMS_COUNT == -1)
                GO_ITEMS_COUNT = count;

            if (count > GO_ITEMS_COUNT)
            {
                // remove the existing content
                popup->RemoveItemsRange(GO_ITEMS_COUNT, count - 1);
            }

            // we will connect hotpaths if they exist
            DWORD firstID = popupID == CML_LEFT_GO ? CM_LEFTHOTPATH_MIN : CM_RIGHTHOTPATH_MIN;
            HotPaths.FillHotPathsMenu(popup, firstID, FALSE, FALSE, FALSE, TRUE);

            // we will connect the history directory, maximum 10 items
            firstID = popupID == CML_LEFT_GO ? CM_LEFTHISTORYPATH_MIN : CM_RIGHTHISTORYPATH_MIN;
            DirHistory->FillHistoryPopupMenu(popup, firstID, 10, TRUE);
            break;
        }

        case CML_LEFT_VISIBLE:
        {
            popup->CheckItem(CM_LEFTDIRLINE, FALSE, LeftPanel->DirectoryLine->HWindow != NULL);
            popup->EnableItem(CM_LEFTHEADER, FALSE, LeftPanel->GetViewMode() == vmDetailed);
            popup->CheckItem(CM_LEFTHEADER, FALSE, LeftPanel->GetViewMode() == vmDetailed && LeftPanel->HeaderLineVisible);
            popup->CheckItem(CM_LEFTSTATUS, FALSE, LeftPanel->StatusLine->HWindow != NULL);
            break;
        }

        case CML_RIGHT_VISIBLE:
        {
            popup->CheckItem(CM_RIGHTDIRLINE, FALSE, RightPanel->DirectoryLine->HWindow != NULL);
            popup->EnableItem(CM_RIGHTHEADER, FALSE, RightPanel->GetViewMode() == vmDetailed);
            popup->CheckItem(CM_RIGHTHEADER, FALSE, RightPanel->GetViewMode() == vmDetailed && RightPanel->HeaderLineVisible);
            popup->CheckItem(CM_RIGHTSTATUS, FALSE, RightPanel->StatusLine->HWindow != NULL);
            break;
        }

        case CML_LEFT_SORTBY:
        case CML_RIGHT_SORTBY:
        {
            BOOL left = popupID == CML_LEFT_SORTBY;
            (left ? LeftPanel : RightPanel)->FillSortByMenu(popup);
            break;
        }

        case CML_FILES:
        {
            break;
        }

        case CML_EDIT:
        {
            // When it comes to a paste of type "change directory", we will display it in the Paste item
            char text[220];
            char tail[50];
            tail[0] = 0;

            strcpy(text, LoadStr(IDS_MENU_EDIT_PASTE));

            CFilesWindow* activePanel = GetActivePanel();
            BOOL activePanelIsDisk = (activePanel != NULL && activePanel->Is(ptDisk));
            if (EnablerPastePath &&
                (!activePanelIsDisk || !EnablerPasteFiles) && // PasteFiles is a priority
                !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS is a priority
            {
                char* p = strrchr(text, '\t');
                if (p != NULL)
                    strcpy(tail, p);
                else
                    p = text + strlen(text);

                sprintf(p, " (%s)%s", LoadStr(IDS_PASTE_CHANGE_DIRECTORY), tail);
            }

            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_STRING;
            mii.String = text;
            popup->SetItemInfo(CM_CLIPPASTE, FALSE, &mii);
            break;
        }

        case CML_FILES_NEW:
        {
            CFilesWindow* activePanel = GetActivePanel();
            if (activePanel == NULL)
                break;
            BeginStopRefresh(); // close in WM_USER_UNINITMENUPOPUP/CML_FILES_NEW, which
                                // guaranteed to match this input

            // if the menu does not exist, we will let it be created
            if ((!ContextMenuNew->MenuIsAssigned()) && activePanel->Is(ptDisk) &&
                activePanel->CheckPath(FALSE) == ERROR_SUCCESS)
                GetNewOrBackgroundMenu(HWindow, activePanel->GetPath(), ContextMenuNew, CM_NEWMENU_MIN, CM_NEWMENU_MAX, FALSE);

            // if the menu exists, we let our menu be built based on it
            if (ContextMenuNew->MenuIsAssigned())
                popup->SetTemplateMenu(ContextMenuNew->GetMenu());
            else
            {
                // otherwise insert the string that the menu new is not available
                popup->RemoveAllItems();
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_STATE;
                mii.Type = MENU_TYPE_STRING;
                mii.String = LoadStr(IDS_NEWISNOTAVAILABLE);
                mii.State = MENU_STATE_GRAYED;
                popup->InsertItem(0, TRUE, &mii);
            }
            break;
        }

        case CML_FILES_VIEWWITH:
        {
            CFilesWindow* activePanel = GetActivePanel();
            if (activePanel == NULL)
                break;

            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // Destruction of the imagelist is done in WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);

            activePanel->FillViewWithMenu(popup);
            break;
        }

        case CML_FILES_EDITWITH:
        {
            CFilesWindow* activePanel = GetActivePanel();
            if (activePanel == NULL)
                break;
            activePanel->FillEditWithMenu(popup);
            break;
        }

        case CML_COMMANDS_USERMENU:
        {
            popup->RemoveAllItems();
            FillUserMenu(popup); // this unpacking of the user menu is handled by WM_USER_ENTERMENULOOP/WM_USER_LEAVEMENULOOP (called UserMenuIconBkgndReader.BeginUserMenuIconsInUse / EndUserMenuIconsInUse)
            break;
        }

        case CML_PLUGINS:
        {
            // Initialize the Plugins menu
            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // Destruction of the imagelist is done in WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);

            Plugins.InitMenuItems(HWindow, popup);
            popup->AssignHotKeys();
            break;
        }

        case CML_PLUGINS_SUBMENU:
        {
            // Initialization of a submenu of a plugin
            Plugins.InitSubMenuItems(HWindow, popup);
            break;
        }

        case CML_OPTIONS:
        {
            popup->CheckItem(CM_ALWAYSONTOP, FALSE, Configuration.AlwaysOnTop);
            break;
        }

        case CML_OPTIONS_PLUGINS:
        {
            popup->RemoveAllItems();

            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // Destruction of the imagelist is done in WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);
            // we only want plugins with configuration options
            if (Plugins.AddNamesToMenu(popup, CM_PLUGINCFG_MIN, CM_PLUGINCFG_MAX - CM_PLUGINCFG_MIN, TRUE))
                popup->AssignHotKeys();
            break;
        }

        case CML_OPTIONS_VISIBLE:
        {
            popup->CheckItem(CM_TOGGLETOPTOOLBAR, FALSE, TopToolBar->HWindow != NULL);
            popup->CheckItem(CM_TOGGLEPLUGINSBAR, FALSE, PluginsBar->HWindow != NULL);
            popup->CheckItem(CM_TOGGLEMIDDLETOOLBAR, FALSE, MiddleToolBar->HWindow != NULL);
            popup->CheckItem(CM_TOGGLEUSERMENUTOOLBAR, FALSE, UMToolBar->HWindow != NULL);
            popup->CheckItem(CM_TOGGLEHOTPATHSBAR, FALSE, HPToolBar->HWindow != NULL);
            popup->CheckItem(CM_TOGGLEDRIVEBAR, FALSE, DriveBar->HWindow != NULL && DriveBar2->HWindow == NULL);
            popup->CheckItem(CM_TOGGLEDRIVEBAR2, FALSE, DriveBar2->HWindow != NULL);
            popup->CheckItem(CM_TOGGLEEDITLINE, FALSE, EditPermanentVisible);
            popup->CheckItem(CM_TOGGLEBOTTOMTOOLBAR, FALSE, BottomToolBar->HWindow != NULL);
            popup->CheckItem(CM_TOGGLE_UMLABELS, FALSE, Configuration.UserMenuToolbarLabels);
            popup->CheckItem(CM_TOGGLE_GRIPS, FALSE, !Configuration.GripsVisible);
            break;
        }

        case CML_HELP_ABOUTPLUGINS:
        {
            popup->RemoveAllItems();

            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // Destruction of the imagelist is done in WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);
            // we want all plugins
            if (Plugins.AddNamesToMenu(popup, CM_PLUGINABOUT_MIN, CM_PLUGINABOUT_MAX - CM_PLUGINABOUT_MIN, FALSE))
                popup->AssignHotKeys();
            break;
        }
        }
        return 0;
    }

    case WM_INITMENUPOPUP: // watch out, similar code is still in CFilesBox
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
    {
        LRESULT plResult = 0;
        if (ContextMenuChngDrv != NULL)
        {
            // if the user right-clicks on HotPath in the ChangeDrive menu, it will come here
            CALL_STACK_MESSAGE1("CMainWindow::WindowProc::ContextMenuChngDrv");
            SafeHandleMenuChngDrvMsg2(uMsg, wParam, lParam, &plResult);
        }
        if (ContextMenuNew != NULL && ContextMenuNew->MenuIsAssigned())
        {
            CALL_STACK_MESSAGE1("CMainWindow::WindowProc::SafeHandleMenuMsg2");
            SafeHandleMenuNewMsg2(uMsg, wParam, lParam, &plResult);
        }
        return plResult;
    }

    case WM_SETCURSOR:
    {
        if (HasLockedUI())
            break;
        if (HelpMode)
        {
            SetCursor(HHelpCursor);
            return TRUE;
        }
        POINT p, p2;
        GetCursorPos(&p);
        p2 = p;
        ScreenToClient(HWindow, &p);
        RECT r;
        GetSplitRect(r);
        if (IsWindowEnabled(HWindow) && PtInRect(&r, p) && GetCapture() == NULL)
        {
            BOOL aboveMiddle = FALSE;
            if (MiddleToolBar != NULL && MiddleToolBar->HWindow != NULL)
            {
                GetWindowRect(MiddleToolBar->HWindow, &r);
                aboveMiddle = PtInRect(&r, p2);
            }
            if (!aboveMiddle)
            {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;
    }

    case WM_CONTEXTMENU:
    {
        if (HasLockedUI())
            break;
        if (!DragMode)
        {
            OnWmContextMenu((HWND)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        if (HasLockedUI())
            break;

        POINT p;
        p.x = (short)LOWORD(lParam);
        p.y = (short)HIWORD(lParam);

        RECT r;
        GetSplitRect(r);

        if (PtInRect(&r, p))
        {
            if (uMsg == WM_LBUTTONDOWN) // click -> start dragging
            {
                UpdateWindow(HWindow);        // if Salamander is down, we will have all windows redrawn
                MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
                BeginStopIconRepaint();       // we do not want any repainting icons
                if (!DragFullWindows)
                    BeginStopStatusbarRepaint(); // we do not want to redraw the throbber when dragging an XOR-ed splitbar

                DragMode = TRUE;
                DragAnchorX = p.x - r.left;
                SetCapture(HWindow);

                HWND toolTip = CreateWindowEx(0,
                                              TOOLTIPS_CLASS,
                                              NULL,
                                              TTS_ALWAYSTIP | TTS_NOPREFIX,
                                              CW_USEDEFAULT,
                                              CW_USEDEFAULT,
                                              CW_USEDEFAULT,
                                              CW_USEDEFAULT,
                                              NULL,
                                              NULL,
                                              HInstance,
                                              NULL);
                ToolTipWindow.AttachToWindow(toolTip);
                ToolTipWindow.SetToolWindow(HWindow);
                TOOLINFO ti;
                ti.cbSize = sizeof(TOOLINFO);
                ti.uFlags = TTF_SUBCLASS | TTF_ABSOLUTE | TTF_TRACK;
                ti.hwnd = HWindow;
                ti.uId = 1;
                GetClientRect(HWindow, &ti.rect);
                ti.hinst = HInstance;
                ti.lpszText = LPSTR_TEXTCALLBACK;
                SendMessage(ToolTipWindow.HWindow, TTM_ADDTOOL, 0, (LPARAM)&ti);

                int splitWidth = MainWindow->GetSplitBarWidth();
                DragSplitPosition = SplitPosition;
                POINT mp;
                GetCursorPos(&mp);
                POINT p2;
                p2.x = r.left;
                p2.y = 0;
                ClientToScreen(HWindow, &p2);
                mp.x = p2.x;
                SendMessage(ToolTipWindow.HWindow, TTM_TRACKPOSITION, 0, (LPARAM)(DWORD)MAKELONG(mp.x + splitWidth + 2, mp.y + 10));
                SendMessage(ToolTipWindow.HWindow, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);

                GetWindowSplitRect(r);
                DragSplitX = p.x - DragAnchorX;
                DrawSplitLine(HWindow, DragSplitX, -1, r);
                return 0;
            }
            if (uMsg == WM_LBUTTONDBLCLK)
            {
                if (SplitPosition != 0.5)
                {
                    SplitPosition = 0.5;
                    LayoutWindows();
                    FocusPanel(GetActivePanel());
                }
                return 0;
            }
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (HasLockedUI())
            break;
        if (DragMode && (wParam & MK_LBUTTON))
        {
            int x = (short)LOWORD(lParam);
            RECT r;
            GetWindowSplitRect(r);

            int splitWidth = MainWindow->GetSplitBarWidth();

            // center stop
            double splitPosition = (double)(x - DragAnchorX) / (WindowWidth - splitWidth);

            if (splitPosition >= 0.49 && splitPosition <= 0.51)
            {
                x = (WindowWidth - splitWidth) / 2 + DragAnchorX;
                splitPosition = 0.5;
            }

            if (splitPosition < 0)
                splitPosition = 0;
            if (splitPosition > 1)
                splitPosition = 1;

            int leftWidth = x - DragAnchorX;
            if (leftWidth < MIN_WIN_WIDTH + 1)
                leftWidth = MIN_WIN_WIDTH + 1;
            int rightWidth = WindowWidth - 2 - leftWidth - splitWidth;
            if (rightWidth < MIN_WIN_WIDTH - 1)
            {
                rightWidth = MIN_WIN_WIDTH - 1;
                leftWidth = WindowWidth - 2 - splitWidth - rightWidth;
            }

            TOOLINFO ti;
            ti.cbSize = sizeof(TOOLINFO);
            ti.uFlags = 0;
            ti.hwnd = HWindow;
            ti.uId = 1;
            GetClientRect(HWindow, &ti.rect);

            DragSplitPosition = splitPosition;

            POINT p;
            GetCursorPos(&p);
            POINT p2;
            p2.x = leftWidth;
            p2.y = 0;
            ClientToScreen(HWindow, &p2);
            p.x = p2.x;
            SendMessage(ToolTipWindow.HWindow, TTM_TRACKPOSITION, 0, (LPARAM)(DWORD)MAKELONG(p.x + splitWidth + 2, p.y + 10));
            UpdateWindow(HWindow);

            if (DragFullWindows)
            {
                if (DragSplitX != leftWidth)
                {
                    DragSplitX = leftWidth;
                    SplitPosition = DragSplitPosition;
                    LayoutWindows();
                }
            }
            else
            {
                DrawSplitLine(HWindow, leftWidth, DragSplitX, r);
                DragSplitX = leftWidth;
            }

            //        ti.hinst = HInstance;
            //        ti.lpszText = LPSTR_TEXTCALLBACK;
            //        SendMessage(ToolTipWindow.HWindow, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
        }
        break;
    }

    case WM_CANCELMODE:
    case WM_LBUTTONUP:
    {
        if (HasLockedUI())
            break;
        if (DragMode)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            RECT r2;
            GetSplitRect(r2);
            r2.left = r.left;
            r2.right = r.right;
            DrawSplitLine(HWindow, -1, DragSplitX, r2);
            SendMessage(ToolTipWindow.HWindow, TTM_ACTIVATE, FALSE, 0);
            DestroyWindow(ToolTipWindow.HWindow); // just disconnects the toolTip from the object
            if (uMsg == WM_LBUTTONUP)
            {
                // Placement will only be accepted with legal confirmation
                //          int splitWidth = MainWindow->GetSplitBarWidth();
                //          SplitPosition = (double)DragSplitX / (WindowWidth - splitWidth);
                SplitPosition = DragSplitPosition;
                LayoutWindows();
            }
            DragMode = FALSE;
            ReleaseCapture();
            FocusPanel(GetActivePanel());
            EndStopIconRepaint(TRUE); // Let's disable icon redrawing and let them be redrawn
            if (!DragFullWindows)
                EndStopStatusbarRepaint(); // Disable redrawing of the throbber when dragging an XOR-splitbar
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (!Created)
            break;
        if (HasLockedUI())
            break;
        LPNMHDR lphdr = (LPNMHDR)lParam;
        if (lphdr->code == TTN_NEEDTEXT && lphdr->hwndFrom == ToolTipWindow.HWindow)
        {
            char* text = ((LPTOOLTIPTEXT)lParam)->szText;
            sprintf(text, "%.1lf %%", DragSplitPosition * 100);
            PointToLocalDecimalSeparator(text, 15);
            return 0;
        }

        if (lphdr->code == NM_RCLICK &&
            (LeftPanel->DirectoryLine->ToolBar != NULL &&
             lphdr->hwndFrom == LeftPanel->DirectoryLine->ToolBar->HWindow))
        {
            CToolBar* toolBar = LeftPanel->DirectoryLine->ToolBar;
            DWORD pos = GetMessagePos();
            POINT p;
            p.x = GET_X_LPARAM(pos);
            p.y = GET_Y_LPARAM(pos);
            ScreenToClient(toolBar->HWindow, &p);
            int index = toolBar->HitTest(p.x, p.y);
            if (index >= 0)
            {
                TLBI_ITEM_INFO2 tii;
                tii.Mask = TLBI_MASK_ID;
                if (toolBar->GetItemInfo2(index, TRUE, &tii))
                {
                    if (tii.ID == CM_LCHANGEDRIVE)
                    {
                        LeftPanel->UserWorkedOnThisPath = TRUE;
                        ShellAction(LeftPanel, saContextMenu, FALSE);
                        return 1;
                    }
                }
            }
            break;
        }

        if (lphdr->code == NM_RCLICK &&
            (RightPanel->DirectoryLine->ToolBar != NULL &&
             lphdr->hwndFrom == RightPanel->DirectoryLine->ToolBar->HWindow))
        {
            CToolBar* toolBar = RightPanel->DirectoryLine->ToolBar;
            DWORD pos = GetMessagePos();
            POINT p;
            p.x = GET_X_LPARAM(pos);
            p.y = GET_Y_LPARAM(pos);
            ScreenToClient(toolBar->HWindow, &p);
            int index = toolBar->HitTest(p.x, p.y);
            if (index >= 0)
            {
                TLBI_ITEM_INFO2 tii;
                tii.Mask = TLBI_MASK_ID;
                if (toolBar->GetItemInfo2(index, TRUE, &tii))
                {
                    if (tii.ID == CM_RCHANGEDRIVE)
                    {
                        RightPanel->UserWorkedOnThisPath = TRUE;
                        ShellAction(RightPanel, saContextMenu, FALSE);
                        return 1;
                    }
                }
            }
            break;
        }

        if (lphdr->code == NM_RCLICK &&
            (DriveBar != NULL && DriveBar->HWindow != NULL &&
             lphdr->hwndFrom == DriveBar->HWindow))
        {
            if (DriveBar->OnContextMenu())
                return 1;
            break;
        }

        if (lphdr->code == NM_RCLICK &&
            (DriveBar2 != NULL && DriveBar2->HWindow != NULL &&
             lphdr->hwndFrom == DriveBar2->HWindow))
        {
            if (DriveBar2->OnContextMenu())
                return 1;
            break;
        }

        if (lphdr->code == TBN_TOOLBARCHANGE)
        {
            if (LeftPanel->DirectoryLine->ToolBar != NULL &&
                lphdr->hwndFrom == LeftPanel->DirectoryLine->ToolBar->HWindow)
                LeftPanel->DirectoryLine->LayoutWindow();
            if (RightPanel->DirectoryLine->ToolBar != NULL &&
                lphdr->hwndFrom == RightPanel->DirectoryLine->ToolBar->HWindow)
                RightPanel->DirectoryLine->LayoutWindow();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            return 0;
        }
        if (lphdr->code == RBN_AUTOSIZE)
        {
            LPNMRBAUTOSIZE lpnmas = (LPNMRBAUTOSIZE)lParam;
            LayoutWindows();
            return 0;
        }
        if (lphdr->code == RBN_LAYOUTCHANGED)
        {
            StoreBandsPos();
            return 0;
        }

        if (lphdr->code == RBN_BEGINDRAG && DriveBar2->HWindow != NULL)
        {
            // During the bandit raid, we will hide the bars beforehand
            ShowHideTwoDriveBarsInternal(FALSE);
            return 0;
        }

        if (lphdr->code == RBN_ENDDRAG && DriveBar2->HWindow != NULL)
        {
            // After pulling the gang, we will display our two gangs again and push them to the end
            ShowHideTwoDriveBarsInternal(TRUE);
            return 0;
        }

        break;
    }

    case WM_WINDOWPOSCHANGED:
    {
        GetWindowRect(HWindow, &WindowRect);
        break;
    }

    case WM_SIZE: // resizing panel
    {
        // at Tonda's, WM_SIZE arrives before completing WM_CREATE
        // (bug report execution address = 0x004743C3)
        if (!Created)
        {
            PostMessage(HWindow, uMsg, wParam, lParam);
            break;
        }

        WindowWidth = LOWORD(lParam);
        WindowHeight = HIWORD(lParam);

        if (SplitPosition < 0)
            SplitPosition = 0;
        if (SplitPosition > 1)
            SplitPosition = 1;

        int splitWidth = GetSplitBarWidth();
        int middleToolbarWidth = 0;
        if (MiddleToolBar->HWindow != NULL)
            middleToolbarWidth = MiddleToolBar->GetNeededWidth();

        int leftWidth = (int)((WindowWidth - splitWidth) * SplitPosition) - 1;
        if (leftWidth < MIN_WIN_WIDTH)
            leftWidth = MIN_WIN_WIDTH;
        int rightWidth = WindowWidth - 2 - leftWidth - splitWidth;
        if (rightWidth < MIN_WIN_WIDTH)
        {
            rightWidth = MIN_WIN_WIDTH;
            leftWidth = WindowWidth - 2 - rightWidth - splitWidth;
        }
        SplitPositionPix = 1 + leftWidth;

        TopRebarHeight = 0;
        BottomToolBarHeight = 0;
        EditHeight = 0;
        PanelsHeight = WindowHeight - 1;

        int windowsCount = 3;

        RECT rebRect;
        GetWindowRect(HTopRebar, &rebRect);
        TopRebarHeight = rebRect.bottom - rebRect.top;

        if (MiddleToolBar->HWindow != NULL)
        {
            windowsCount++;
        }
        if (BottomToolBar->HWindow != NULL)
        {
            windowsCount++;
            BottomToolBarHeight = BottomToolBar->GetNeededHeight();
        }
        if (EditWindow->HWindow != NULL)
        {
            windowsCount++;
            EditHeight = EditWindow->GetNeededHeight() + 1;
        }

        PanelsHeight -= TopRebarHeight + BottomToolBarHeight + EditHeight;
        if (PanelsHeight < 0)
            PanelsHeight = 0;

        HDWP hdwp = HANDLES(BeginDeferWindowPos(windowsCount));
        if (hdwp != NULL)
        {
            hdwp = HANDLES(DeferWindowPos(hdwp, HTopRebar, NULL,
                                          0, 0, WindowWidth, TopRebarHeight,
                                          SWP_NOACTIVATE | SWP_NOZORDER));

            hdwp = HANDLES(DeferWindowPos(hdwp, LeftPanel->HWindow, NULL,
                                          1, TopRebarHeight, leftWidth, PanelsHeight,
                                          SWP_NOACTIVATE | SWP_NOZORDER));
            hdwp = HANDLES(DeferWindowPos(hdwp, RightPanel->HWindow, NULL,
                                          SplitPositionPix + splitWidth, TopRebarHeight, rightWidth, PanelsHeight,
                                          SWP_NOACTIVATE | SWP_NOZORDER));

            if (MiddleToolBar->HWindow != NULL)
            {
                // Move the toolbar down if any of the directory panels have a line
                int offset1 = 0;
                int offset2 = 0;
                if (LeftPanel->DirectoryLine != NULL && LeftPanel->DirectoryLine->HWindow != NULL)
                    offset1 = LeftPanel->DirectoryLine->GetNeededHeight();
                if (RightPanel->DirectoryLine != NULL && RightPanel->DirectoryLine->HWindow != NULL)
                    offset2 = RightPanel->DirectoryLine->GetNeededHeight();
                int offset = max(offset1, offset2);
                hdwp = HANDLES(DeferWindowPos(hdwp, MiddleToolBar->HWindow, NULL,
                                              SplitPositionPix + SPLIT_LINE_WIDTH, TopRebarHeight + offset,
                                              middleToolbarWidth, PanelsHeight - offset,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            }

            // HWND_BOTTOM - to prevent window flickering during resizing
            // if the bottom toolbar gets down, it blinks during resize
            if (EditWindow->HWindow != NULL)
                hdwp = HANDLES(DeferWindowPos(hdwp, EditWindow->HWindow, HWND_BOTTOM,
                                              0, TopRebarHeight + PanelsHeight + 2, WindowWidth, EditHeight + 150,
                                              SWP_NOACTIVATE /*| SWP_NOZORDER*/));

            if (BottomToolBar->HWindow != NULL)
                hdwp = HANDLES(DeferWindowPos(hdwp, BottomToolBar->HWindow, NULL,
                                              1, TopRebarHeight + PanelsHeight + EditHeight + 1, WindowWidth - 2, BottomToolBarHeight,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            HANDLES(EndDeferWindowPos(hdwp));
        }
        if (DriveBar2->HWindow != NULL)
        {
            REBARBANDINFO rbi;
            rbi.cbSize = sizeof(REBARBANDINFO);
            rbi.fMask = RBBIM_SIZE;

            RECT r;
            // u Tomase Jelinka se dokazal po maximalizaci hlavniho okna druhy band pruh
            // Stick to the right side and don't move, this could solve the problem
            GetClientRect(RightPanel->HWindow, &r);
            rbi.cx = r.right;
            int index = (int)SendMessage(HTopRebar, RB_IDTOINDEX, BANDID_DRIVEBAR2, 0);
            SendMessage(HTopRebar, RB_SETBANDINFO, index, (LPARAM)&rbi);

            GetClientRect(LeftPanel->HWindow, &r);
            rbi.cx = r.right + MainWindow->GetSplitBarWidth() / 2 - 1;
            index = (int)SendMessage(HTopRebar, RB_IDTOINDEX, BANDID_DRIVEBAR, 0);
            SendMessage(HTopRebar, RB_SETBANDINFO, index, (LPARAM)&rbi);
        }
        break;
    }

    case WM_NCACTIVATE:
    {
        // set global variable that indicates the state of the main frame
        CaptionIsActive = (BOOL)wParam;

        // let's repaint the directory line of the active window
        // if it is a loss of selection, we require an update - we hurry to
        // do not destroy the buffer of the open window with CS_SAVEBITS
        CFilesWindow* panel = GetActivePanel();
        if (panel != NULL && panel->DirectoryLine != NULL)
            panel->DirectoryLine->InvalidateAndUpdate(!CaptionIsActive);

        if (!CaptionIsActive)
        {
            // let's reset the bottom toolbar to its default position
            UpdateBottomToolBar();
        }
        break;
    }

    case WM_ENABLE:
    {
        if (WindowsVistaAndLater)
        {
            // patch for Windows Vista UAC: when running a file from the panel that caused the UAC elevation to be displayed
            // When prompting and subsequently closing with Cancel, Salamander lost focus from the panel
            // The main window is disabled when messages like WM_ACTIVATE or WM_SETFOCUS arrive and receive focus
            // Microsoft IME supports popups.
            BOOL enabled = (BOOL)wParam;
            if (enabled)
            {
                HWND hFocused = GetFocus();
                HWND hPanelListbox = NULL;
                CFilesWindow* activePanel = GetActivePanel();
                if (activePanel != NULL && !EditMode)
                {
                    hPanelListbox = activePanel->GetListBoxHWND();
                    if (hFocused == NULL || hFocused != hPanelListbox)
                        FocusPanel(activePanel);
                }
            }
        }
        break;
    }

    case WM_ACTIVATE:
    {
        int active = LOWORD(wParam);
        if (active == WA_INACTIVE)
            CacheNextSetFocus = TRUE; // for smooth switching to Salama (otherwise the aggressive front focus would be drawn (see old Salamys))
        else
            SuppressToolTipOnCurrentMousePos(); // Suppressing unwanted tooltip when switching to window
        ExitHelpMode();

        // Ensure hiding/displaying the Wait window (if it exists)
        ShowSafeWaitWindow(active != WA_INACTIVE);

        if (active != WA_INACTIVE)
            BringLockedUIToolWnd();

        if (active == WA_ACTIVE || active == WA_CLICKACTIVE)
        {
            if (!EditMode)
            {
                if (GetActivePanel() != NULL)
                {
                    FocusPanel(GetActivePanel());
                    return 0;
                }
            }
            else
            {
                if (EditWindow->HWindow != NULL)
                {
                    SetFocus(EditWindow->HWindow);
                    return 0;
                }
            }
        }
        break;
    }

    case WM_USER_POSTCMDORUNLOADPLUGIN:
    {
        CPluginData* data = Plugins.GetPluginData((CPluginInterfaceAbstract*)wParam);
        if (data != NULL && data->GetLoaded())
        {
            if (lParam == 0)
                data->ShouldUnload = TRUE; // setting flags for unloading plug-ins
            else
            {
                if (lParam == 1)
                    data->ShouldRebuildMenu = TRUE; // setting flags for rebuilding the menu plug-in
                else
                    data->Commands.Add(LOWORD(lParam - 2)); // adding salCmd/menuCmd
            }
            ExecCmdsOrUnloadMarkedPlugins = TRUE; // Inform Salama that he needs to search the data of all plug-ins
        }
        else
        {
            // can occur if waiting for the completion of the Release(force==TRUE) method of the plug-in
            //        TRACE_E("Unexpected situation in WM_USER_POSTCMDORUNLOADPLUGIN.");
        }
        return 0;
    }

    case WM_USER_POSTMENUEXTCMD:
    {
        CPluginData* data = Plugins.GetPluginData((CPluginInterfaceAbstract*)wParam);
        if (data != NULL && data->GetLoaded())
        {
            if (data->GetPluginInterfaceForMenuExt()->NotEmpty())
            {
                CALL_STACK_MESSAGE4("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d,) (%s v. %s)",
                                    (int)lParam, data->DLLName, data->Version);

                // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                data->GetPluginInterfaceForMenuExt()->ExecuteMenuItem(NULL, HWindow, (int)lParam, 0);

                // increase the thread priority again, operation completed
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
            }
            else
            {
                TRACE_E("Plugin must have PluginInterfaceForMenuExt when "
                        "calling CSalamanderGeneral::PostMenuExtCommand()!");
            }
        }
        else
        {
            // must be loaded because post-menu-ext-cmd was called from a loaded plug-in...
            // post-unload is triggered only in "idle", so unload couldn't be done...
            TRACE_E("Unexpected situation in WM_USER_POSTMENUEXTCMD.");
        }
        return 0;
    }

    case WM_USER_SALSHEXT_TRYRELDATA:
    {
        //      TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: begin");
        if (SalShExtSharedMemView != NULL) // Shared memory is available (we cannot handle cut/copy&paste errors)
        {
            WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
            BOOL needRelease = TRUE;
            if (!SalShExtSharedMemView->BlockPasteDataRelease)
            {
                if (!SalShExtPastedData.IsLocked())
                {
                    BOOL isOnClipboard = FALSE;
                    if (SalShExtSharedMemView->DoPasteFromSalamander &&
                        SalShExtSharedMemView->SalamanderMainWndPID == GetCurrentProcessId() &&
                        SalShExtSharedMemView->SalamanderMainWndTID == GetCurrentThreadId() &&
                        SalShExtSharedMemView->PastedDataID == SalShExtPastedData.GetDataID())
                    {
                        ReleaseMutex(SalShExtSharedMemMutex);
                        needRelease = FALSE;

                        IDataObject* dataObj;
                        if (OleGetClipboard(&dataObj) == S_OK && dataObj != NULL)
                        {
                            if (IsFakeDataObject(dataObj, NULL, NULL, 0))
                            {
                                isOnClipboard = TRUE;
                            }
                            dataObj->Release();
                        }
                    }

                    if (!isOnClipboard)
                    {
                        if (needRelease)
                            ReleaseMutex(SalShExtSharedMemMutex);
                        needRelease = FALSE;

                        //TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: clearing paste-data!");
                        SalShExtPastedData.Clear();
                    }
                    //            else TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: fake-data-object is still on clipboard");
                }
                //          else TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: paste-data is locked");
            }
            //        else TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: release of paste-data is blocked");
            if (needRelease)
                ReleaseMutex(SalShExtSharedMemMutex);
        }
        //      TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: end");
        return 0;
    }

    case WM_USER_SALSHEXT_PASTE:
    {
        //      TRACE_I("WM_USER_SALSHEXT_PASTE: begin");
        if (SalShExtSharedMemView != NULL) // Shared memory is available (we cannot handle cut/copy&paste errors)
        {
            BOOL tmpPasteDone = FALSE;
            char tgtPath[MAX_PATH];
            tgtPath[0] = 0;
            int operation = 0;
            DWORD dataID = -1;
            WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
            if (SalShExtSharedMemView->PostMsgIndex == (int)wParam) // we only take "current" news
            {
                if (SalamanderBusy)
                    SalShExtSharedMemView->SalBusyState = 2 /* Salamander is "busy", we will postpone the paste for later*/;
                else
                {
                    SalamanderBusy = TRUE;
                    SalShExtPastedData.SetLock(TRUE);
                    LastSalamanderIdleTime = GetTickCount();
                    SalShExtSharedMemView->SalBusyState = 1 /* Salamander is not "busy" and is already waiting for the paste operation to be entered*/;
                    SalShExtSharedMemView->PasteDone = FALSE;

                    int count = 0;
                    while (count++ < 50) // We won't wait more than 5 seconds
                    {
                        ReleaseMutex(SalShExtSharedMemMutex);
                        Sleep(100); // give copyhook 100ms to react
                        WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                        if (SalShExtSharedMemView->PasteDone) // copyhook passed us the target directory for Paste and other data
                        {
                            //                TRACE_I("WM_USER_SALSHEXT_PASTE: copy hook returned: paste done!");
                            lstrcpyn(tgtPath, SalShExtSharedMemView->TargetPath, MAX_PATH);
                            operation = SalShExtSharedMemView->Operation;
                            dataID = SalShExtSharedMemView->PastedDataID;
                            tmpPasteDone = TRUE;
                            break;
                        }
                    }
                    SalamanderBusy = FALSE;
                }
            }
            ReleaseMutex(SalShExtSharedMemMutex);

            if (tmpPasteDone && operation == SALSHEXT_COPY && SalShExtPastedData.GetDataID() == dataID) // perform the Paste operation
            {
                SalamanderBusy = TRUE;
                LastSalamanderIdleTime = GetTickCount();
                //          TRACE_I("WM_USER_SALSHEXT_PASTE: calling SalShExtPastedData.DoPasteOperation");
                ProgressDialogActivateDrop = LastWndFromPasteGetData;
                SalShExtPastedData.DoPasteOperation(operation == SALSHEXT_COPY, tgtPath);
                ProgressDialogActivateDrop = NULL; // we need to clean up the global variable for the next use of the progress dialog
                LastWndFromPasteGetData = NULL;    // for the next Paste we will reset it here
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, tgtPath, NULL);
                SalamanderBusy = FALSE;
            }
            SalShExtPastedData.SetLock(FALSE);
            PostMessage(HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // after unlocking, we may release the data if necessary
        }
        //      TRACE_I("WM_USER_SALSHEXT_PASTE: end");
        return 0;
    }

    case WM_USER_REFRESH_SHARES:
    {
        Shares.Refresh();
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        int t2 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        if (LeftPanel != NULL && LeftPanel->Is(ptDisk) && !LeftPanel->GetNetworkDrive())
        {
            PostMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
        if (RightPanel != NULL && RightPanel->Is(ptDisk) && !RightPanel->GetNetworkDrive())
        {
            PostMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
        return 0;
    }

    case WM_USER_END_SUSPMODE:
    {
        // if the main window is minimized (slowly restored or opening a context menu),
        // postpone the check of the panel content (risk of "retry" when removing the floppy disk, etc.)
        if (IsIconic(HWindow))
        {
            SetTimer(HWindow, IDT_POSTENDSUSPMODE, 500, NULL);
            //      Original place of the timer: PostMessage(HWindow, WM_USER_END_SUSPMODE, 0, 0);
            return 0;
        }

        if (--ActivateSuspMode < 0)
        {
            ActivateSuspMode = 0;
            // TRACE_E("WM_USER_END_SUSPMODE: problem 2");  // when opening a messagebox with a NULL parent, multiple WM_ACTIVATEAPP "activate" messages are sent (Salamander is already activated)
            return 0; // message has already been canceled
        }
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

        // first we need to finish activating the window
        static BOOL recursion = FALSE;
        if (!recursion)
        {
            recursion = TRUE;
            MSG msg;
            CanCloseButInEndSuspendMode = CanClose;
            BOOL oldCanClose = CanClose;
            CanClose = FALSE; // We won't let ourselves be closed, we are inside a method
            BOOL postWM_USER_CLOSE_MAINWND = FALSE;
            BOOL postWM_USER_FORCECLOSE_MAINWND = FALSE;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_USER_CLOSE_MAINWND && msg.hwnd == HWindow)
                    postWM_USER_CLOSE_MAINWND = TRUE;
                else
                {
                    if (msg.message == WM_USER_FORCECLOSE_MAINWND && msg.hwnd == HWindow)
                        postWM_USER_FORCECLOSE_MAINWND = TRUE;
                    else
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
            CanClose = oldCanClose;
            CanCloseButInEndSuspendMode = FALSE;

            if (postWM_USER_CLOSE_MAINWND)
                PostMessage(HWindow, WM_USER_CLOSE_MAINWND, 0, 0);
            if (postWM_USER_FORCECLOSE_MAINWND)
                PostMessage(HWindow, WM_USER_FORCECLOSE_MAINWND, 0, 0);

            recursion = FALSE;
        }
        //      else
        //      {
        //#pragma message (__FILE__ " (2120): throw")
        //        SalMessageBox(HWindow, "error3", "error3", MB_OK);
        //      }

        // Window is activated, we will perform a refresh
        // EndSuspendMode();   // removed, we want to refresh even when the main window is inactive

        LeftPanel->Activate(FALSE);
        RightPanel->Activate(FALSE);

        // if OneDrive Personal/Business was connected/disconnected, we will refresh the Drive bars,
        // when the icon disappears/ appears or the drop-down menu
        BOOL oneDrivePersonal = OneDrivePath[0] != 0;
        int oneDriveBusinessStoragesCount = OneDriveBusinessStorages.Count;
        InitOneDrivePath();
        if (oneDrivePersonal != (OneDrivePath[0] != 0) ||
            oneDriveBusinessStoragesCount != OneDriveBusinessStorages.Count)
        {
            PostMessage(HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
        }

        SetCursor(oldCur);
        return 0;
    }

    case WM_TIMER:
    {
        switch (wParam)
        {
        case IDT_DELETEMNGR_PROCESS:
        {
            KillTimer(HWindow, IDT_DELETEMNGR_PROCESS);
            DeleteManager.ProcessData();
            break;
        }

        case IDT_POSTENDSUSPMODE:
        {
            KillTimer(HWindow, IDT_POSTENDSUSPMODE);
            PostMessage(HWindow, WM_USER_END_SUSPMODE, 0, 0); // if ActivateSuspMode is not greater than or equal to 1, it will not happen
            break;
        }

        case IDT_ADDNEWMODULES:
        {
            AddNewlyLoadedModulesToGlobalModulesStore();
            break;
        }

        case IDT_PLUGINFSTIMERS:
        {
            Plugins.HandlePluginFSTimers();
            break;
        }

        case IDT_ASSOCIATIONSCHNG:
        {
            KillTimer(HWindow, IDT_ASSOCIATIONSCHNG);
            OnAssociationsChangedNotification(FALSE);
            break;
        }

        default:
        {
            TRACE_E("Unknown WM_TIMER wParam=" << wParam);
            break;
        }
        }
        break;
    }

    case WM_USER_SLGINCOMPLETE:
    {
        char buff[1000];
        sprintf(buff, "%s\n", LoadStr(IDS_SLGINCOMPLETE_TEXT));
        Configuration.ShowSLGIncomplete = FALSE;
        CMessageBox(HWindow, MSGBOXEX_OK | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT | MSGBOXEX_ICONINFORMATION,
                    LoadStr(IDS_SLGINCOMPLETE_TITLE), buff, NULL,
                    NULL, NULL, 0, NULL, NULL, IsSLGIncomplete, NULL)
            .Execute();
        break;
    }

    case WM_USER_USERMENUICONS_READY:
    {
        CUserMenuIconDataArr* bkgndReaderData = (CUserMenuIconDataArr*)wParam;
        DWORD threadID = (DWORD)lParam;
        if (bkgndReaderData != NULL && // "always true"
            UserMenuIconBkgndReader.EnterCSIfCanUpdateUMIcons(&bkgndReaderData, threadID))
        { // if the user menu is still displaying these icons:
            // if we can update the icons right away, we will lock access to the user menu from Find and update the icons, otherwise
            // update must be postponed until after closing the menu with icons (we can't cut them under their feet) or after closing
            // cfg (after OK, the newly loaded icons would be overwritten and the new icon reading would not start = the icons would remain unloaded)
            for (int i = 0; i < UserMenuItems->Count; i++)
                UserMenuItems->At(i)->GetIconHandle(bkgndReaderData, TRUE);
            UserMenuIconBkgndReader.LeaveCSAfterUMIconsUpdate();
            if (UMToolBar != NULL && UMToolBar->HWindow != NULL) // refresh the user menu toolbar
                UMToolBar->CreateButtons();
        }
        if (bkgndReaderData != NULL)
            delete bkgndReaderData;
        break;
    }

    case WM_ACTIVATEAPP:
    {
        //      TRACE_I("WM_ACTIVATEAPP: " << (wParam == TRUE ? "activate" : "deactivate"));
        if (FirstActivateApp)
        {
            if (IsWindowVisible(HWindow))
                FirstActivateApp = FALSE;
            else
                break;
        }

        // we will handle work for lost and undelivered messages
        int actSusMode = (wParam == TRUE) ? 1 : 0; // ActivateSuspMode should be 1 when activated, otherwise 0
        if (ActivateSuspMode < 0)
        {
            ActivateSuspMode = 0;
            TRACE_E("WM_USER_END_SUSPMODE: problem 6");
        }
        else
        {
            if (ActivateSuspMode != actSusMode) // for example two consecutive deactivations or missed activation
            {
                KillTimer(HWindow, IDT_POSTENDSUSPMODE); // if it has not been activated yet, we will cancel it (or activate it again)

                MSG msg; // we pump out WM_USER_END_SUSPMODE from the message queue (otherwise there will be a race condition to end the suspend mode: for example, just opening the File Comparator window - there is activation+deactivation after 10ms)
                while (PeekMessage(&msg, HWindow, WM_USER_END_SUSPMODE, WM_USER_END_SUSPMODE, PM_REMOVE))
                    ;

                while (ActivateSuspMode > actSusMode)
                {
                    // EndSuspendMode();  // thrown out, we want to refresh even when the main window is inactive
                    ActivateSuspMode--;
                }
            }
        }

        //      if (IsWindowVisible(HWindow))    // now resolved via FirstActivateApp
        //      {
        if (wParam == TRUE) // activate app
        {
            if (!LeftPanel->DontClearNextFocusName)
                LeftPanel->NextFocusName[0] = 0;
            else
                LeftPanel->DontClearNextFocusName = FALSE;
            if (!RightPanel->DontClearNextFocusName)
                RightPanel->NextFocusName[0] = 0;
            else
                RightPanel->DontClearNextFocusName = FALSE;
            if (Windows7AndLater && IsIconic(HWindow))
            {
                SetTimer(HWindow, IDT_POSTENDSUSPMODE, 200, NULL); // I hope we don't find out why this timer was here, well, if we do, then: it was commented out because it slows down (by 200ms) the directory refresh after a disk operation (e.g. moving a file to a subdirectory, it's also noticeable on a local disk)
            }
            else
            {
                // Until 2.53b1, there was only this branch here and the version with the timer was in the comment.
                // Under W7, users have started reporting issues with activating Salamander when icon grouping is enabled
                // and Salamander is minimized; sometimes then clicking on its thumbnail (or when switching with Alt+Tab) Salamander does not restore,
                // only a beep will be heard; more see https://forum.altap.cz/viewtopic.php?f=6&t=3791
                //
                // So we enable the delayed variant by 200ms again, but only from W7 and only if the window is minimized
                PostMessage(HWindow, WM_USER_END_SUSPMODE, 0, 0); // if ActivateSuspMode is not greater than or equal to 1, it will not happen
            }
            IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
            IdleCheckClipboard = TRUE; // we will also check the clipboard
        }
        else // deactivate app
        {
            // When deactivating the main window, we will exit the quick search and quick rename mode
            CancelPanelsUI();

            //        BeginSuspendMode();    // removed, we want to refresh even when the main window is inactive
            ActivateSuspMode++;
            //        }
            //      }
            //      if (wParam == FALSE)  // pri deaktivaci uteceme z adresaru zobrazenych v panelech,
            //      {                     // to be able to delete, disconnect, etc. from other software
            if (CanChangeDirectory())
            {
                SetCurrentDirectoryToSystem();
            }
        }
        break;
    }

    case WM_CLOSE:
    {
        PostMessage(HWindow, WM_USER_CLOSE_MAINWND, 0, 0);
        return 0;
    }

    case WM_ENDSESSION:
    {
        if (!wParam)
            return 0; // There is no need to shut down / log off, there is nothing to solve

        // Normal shutdown / log off should not come here at all, everything will be handled upon receipt
        // WM_QUERYENDSESSION, at its end the main window will be closed and thus killed
        // Salamander, theoretically then return TRUE = call WM_ENDSESSION, so that
        // can come here, everything is ready, we just return 0 according to MSDN, but for now all
        // Windows versions have preferred killing
        //
        // tady resime tzv. "critical shutdown" (i log off), ma flag ENDSESSION_CRITICAL
        // in lParam, we can invoke it by calling (crucial is EWX_FORCE):
        // ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, SHTDN_REASON_MAJOR_OPERATINGSYSTEM |
        //               SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED);
        // the code is here (search for SE_SHUTDOWN_NAME): https://msdn.microsoft.com/en-us/library/windows/desktop/aa376871%28v=vs.85%29.aspx
        //
        // Only Vista+: we are still dealing with shutdown with the flag EWX_FORCEIFHUNG, it is not set
        // flag ENDSESSION_CRITICAL, but forces the software to terminate regardless of the return value
        // from WM_QUERYENDSESSION sends WM_ENDSESSION and after its completion the software will be killed (if
        // Meanwhile, the user will not interrupt the action with Cancel from the system window that appears after 5s:
        // - I have named this mode "forced shutdown"
        // - the system will not kill our process, there is no timeout running, we will not forcibly terminate anything
        // - about a possible interruption of the shutdown, but we must inform the user - if they interrupt it, it will be
        //   after processing this WM_ENDSESSION, the software should continue running normally
        //
        // when "critical shutdown" (i log off):
        // - under W2K our process is killed without warning, there is nothing to solve
        // - under XP it is unpleasant that in WM_QUERYENDSESSION we do not know that it is a "critical shutdown", so
        //   if nothing prevents it, we will start saving the configuration normally and if we don't finish it within 5s,
        //   Windows killed our process = configuration is lost, naturally it would make sense to create a backup of the configuration
        //   with each shutdown, but serious problems with configuration loss are not common under XP;
        //   pod XP nam bez ohledu na navratovku z WM_QUERYENDSESSION (i kdyz se navratovky nedockaji do 5s,
        //   for example when there is a question about canceling running disk operations) send WM_ENDSESSION,
        //   So we don't save any configuration in WM_ENDSESSION, we just do the worst cleanup
        //   (stopping ongoing disk operations)
        // - under Vista+ we first back up the registry configuration key, we have 5 seconds for that
        //   in WM_QUERYENDSESSION, then return TRUE from it (continue with shutdown) and the system
        //   Give another 5 seconds to finish here in WM_ENDSESSION and we dedicate it to saving the configuration,
        //   it doesn't have to be done in time, then comes the killing and the configuration remains broken = at the next
        //   We will delete the old startup and copy the last configuration from the backup created in WM_QUERYENDSESSION;
        //   Under Vista+, when closing without saving the configuration, we first wait 5s in WM_QUERYENDSESSION
        //   upon completion of disk operations, and then another 5s here in WM_ENDSESSION

        // Experimentally determined this behavior during shutdowns of three types:
        //
        // EWX_FORCE: (starting from Vista+, the ENDSESSION_CRITICAL flag is set)
        // W2K: kill without anything
        // XP: WM_QUERYENDSESSION, without response: WM_ENDSESSION will come in 5s, then kill in 5s
        //     WM_QUERYENDSESSION returns TRUE/FALSE: WM_ENDSESSION arrives, then kill after 5s
        // Win7-10: WM_QUERYENDSESSION, no response: kill after 5s
        // +Vista   WM_QUERYENDSESSION returns TRUE/FALSE: WM_ENDSESSION comes, then kill after 5s
        //
        // EWX_FORCEIFHUNG: (the ENDSESSION_CRITICAL flag is not set)
        // W2K: like Log Off from the start menu
        // XP: like W2K: Log Off from the start menu
        // Win7-10: WM_QUERYENDSESSION, without response: after 5s black screen Kill/Cancel (Cancel interrupts shutdown),
        // +Vista WARNING: without a disabled main window (when there is no parent msgbox or wait-window) kill after 5s,
        //          WM_QUERYENDSESSION returns TRUE/FALSE: WM_ENDSESSION will come,
        //                                               After 5 seconds black screen Kill/Cancel (Cancel interrupts shutdown)
        //
        // Log Off from the start menu:
        // W2K: WM_QUERYENDSESSION, after 5s msgbox Kill/Cancel (Cancel interrupts shutdown),
        //      WM_QUERYENDSESSION returns TRUE -> WM_ENDSESSION comes, after 5s msgbox Kill/Cancel (Cancel works as KILL)
        //      WM_QUERYENDSESSION returns FALSE - interrupts shutdown
        // XP: same as W2K
        // Win7-10: WM_QUERYENDSESSION, without response: after 5s black screen Kill/Cancel (Cancel interrupts shutdown),
        // +Vista   WM_QUERYENDSESSION returns TRUE: WM_ENDSESSION will come,
        //                                         After 5 seconds black screen Kill/Cancel (Cancel interrupts shutdown)
        //          WM_QUERYENDSESSION returns FALSE: immediately shows black screen Kill/Cancel (Cancel interrupts shutdown)

        // description see above at "forced shutdown", we will give the user the opportunity to manually stop the shutdown,
        // if not desired, it can at least stop the ongoing disk operations, then only comes
        // about saving configuration
        if (!SaveCfgInEndSession && !WaitInEndSession && WindowsVistaAndLater &&
            (lParam & ENDSESSION_CRITICAL) == 0)
        {
            if (ProgressDlgArray.RemoveFinishedDlgs() > 0)
            {
                WCHAR blockReason[MAX_STR_BLOCKREASON];
                if (HLanguage != NULL &&
                    LoadStringW(HLanguage, IDS_BLOCKSHUTDOWNDISKOPER, blockReason, _countof(blockReason)))
                {
                    MyShutdownBlockReasonCreate(HWindow, blockReason);
                }

                if (SalMessageBox(HWindow, LoadStr(IDS_FORCEDSHUTDOWNDISKOPER),
                                  SALAMANDER_TEXT_VERSION, MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    ProgressDlgArray.PostCancelToAllDlgs(); // Dialogues and workers run in their own threads, there is a chance they will finish
                    while (ProgressDlgArray.RemoveFinishedDlgs() > 0)
                        Sleep(200); // Wait until the disk operations are canceled
                }

                MyShutdownBlockReasonDestroy(HWindow);
            }
            else
                SalMessageBox(HWindow, LoadStr(IDS_FORCEDSHUTDOWN), SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONINFORMATION);
            // Unfortunately, I couldn't find a way to determine if the shutdown is still in progress or not
            // if the user canceled it (black full screen window under Win7), if not, kill the software, warning
            // the user received, there is nothing more to solve;
            // Disk operations can occur, if not canceled, files will remain in an intermediate state,
            // For example, when copying, the full size of the file will be allocated, but the content is not copied,
            // only filled with zeros + in any case the configuration will not be saved
            return 0;
        }

        if (!SaveCfgInEndSession) // the configuration saving should not be performed (this will be addressed later)
        {                         // WaitInEndSession or XP "critical shutdown": we will wait for disk operations to complete (if any are running)
            if (!WindowsVistaAndLater && ProgressDlgArray.RemoveFinishedDlgs() > 0)
                ProgressDlgArray.PostCancelToAllDlgs(); // Dialogues and workers run in their own threads, there is a chance they will finish

            while (ProgressDlgArray.RemoveFinishedDlgs() > 0)
                Sleep(200);
            return 0; // let's close the software
        }

        if ((lParam & ENDSESSION_CRITICAL) == 0) // theoretically cannot occur (SaveCfgInEndSession is always TRUE)
        {
            TRACE_E("WM_ENDSESSION: unexpected SaveCfgInEndSession: it is not ENDSESSION_CRITICAL!");
            return 0;
        }

        // break; // the break is not missing here !!! only "critical shutdown" continues (including log off), configuration is being saved
    }
    case WM_QUERYENDSESSION:
    case WM_USER_CLOSE_MAINWND:
    case WM_USER_FORCECLOSE_MAINWND:
    {
        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::1");

        DWORD msgArrivalTime = GetTickCount(); // critical shutdown lasts 5s + 5s, if exceeded it will kill us, so we are measuring time

        if (uMsg == WM_QUERYENDSESSION)
        {
            TRACE_I("WM_QUERYENDSESSION: message received");
            SaveCfgInEndSession = FALSE;
            WaitInEndSession = FALSE;

            if ((lParam & ENDSESSION_CRITICAL) != 0)
            {
                // precautions against WM_ENDSESSION coming from the code solving IdleCheckClipboard
                // in OnEnterIdle() and CannotCloseSalMainWnd were TRUE (WM_ENDSESSION then refuses to execute)
                // End of the software is approaching quickly, IDLE seriously doesn't make sense to solve, just delays
                DisableIdleProcessing = TRUE;
            }
        }

        // Windows XP: when performing a critical shutdown, I do not set ENDSESSION_CRITICAL, W2K: when critical
        // shutdown does not send WM_QUERYENDSESSION either, see above at WM_ENDSESSION

        // Vista+: endAfterCleanup: TRUE = critical shutdown (kill in 5s) cannot be refused, soft
        // if 100% is not reached, we will at least do the worst cleanup: cancel running disk operations
        // (stopping the search + closing Find windows and viewers doesn't make sense to solve, just read)
        BOOL endAfterCleanup = FALSE;

        if (!CanClose)
        {
            if (CanCloseButInEndSuspendMode &&
                (uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION))
            { // CanClose is FALSE only for window activation, it does not prevent shutdown
            }
            else // "incomplete start did not occur" or "closing the window is postponed, it will be done after the window is activated", we are finishing now
            {
                if (uMsg == WM_QUERYENDSESSION)
                    TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: CanClose is FALSE");
                if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                    endAfterCleanup = TRUE; // cannot refuse = let's at least tidy up
                else
                    return 0; // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION
            }
        }

        if (!endAfterCleanup && CannotCloseSalMainWnd)
        {
            TRACE_E("WM_USER_CLOSE_MAINWND: CannotCloseSalMainWnd == TRUE!");
            if (uMsg == WM_QUERYENDSESSION)
                TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: CannotCloseSalMainWnd is TRUE");
            if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                endAfterCleanup = TRUE; // cannot refuse = let's at least tidy up
            else
                return 0; // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION
        }

        if (!endAfterCleanup && uMsg != WM_ENDSESSION)
        { // At WM_ENDSESSION it's already busy from WM_QUERYENDSESSION, we need to skip the test
            if (!SalamanderBusy)
            {
                SalamanderBusy = TRUE; // now BUSY, continue processing WM_USER_CLOSE_MAINWND
                LastSalamanderIdleTime = GetTickCount();
            }
            else
            {
                if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                    endAfterCleanup = TRUE; // cannot refuse = let's at least tidy up
                else
                {
                    if (LockedUIReason != NULL && HasLockedUI())
                        SalMessageBox(HWindow, LockedUIReason, SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONINFORMATION);
                    else
                        TRACE_E("WM_USER_CLOSE_MAINWND: SalamanderBusy == TRUE!");
                    if (uMsg == WM_QUERYENDSESSION)
                        TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: SalamanderBusy is TRUE");
                    return 0; // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION
                }
            }
        }

        if (!endAfterCleanup && AlreadyInPlugin > 0)
        {
            TRACE_E("WM_USER_CLOSE_MAINWND: AlreadyInPlugin > 0!");
            if (uMsg == WM_QUERYENDSESSION)
                TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: AlreadyInPlugin > 0");
            if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                endAfterCleanup = TRUE; // cannot refuse = let's at least tidy up
            else                        // Cannot unload the plugin when we are in it!
                return 0;               // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION
        }

        // if the OnClose confirmation is enabled, we let the user confirm the closing of the program
        if (uMsg == WM_USER_CLOSE_MAINWND && Configuration.CnfrmOnSalClose)
        {
            MSGBOXEX_PARAMS params;
            memset(&params, 0, sizeof(params));
            params.HParent = HWindow;
            params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
            params.Caption = LoadStr(IDS_QUESTION);
            params.Text = LoadStr(IDS_CANCLOSESALAMANDER);
            params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAINCS);
            BOOL dontShow = !Configuration.CnfrmOnSalClose;
            params.CheckBoxValue = &dontShow;
            int ret = SalMessageBoxEx(&params);
            Configuration.CnfrmOnSalClose = !dontShow;

            if (ret != IDYES)
                return 0;
        }

        // We have some dialogues with disk operations started
        WCHAR blockReason[MAX_STR_BLOCKREASON];
        if (ProgressDlgArray.RemoveFinishedDlgs() > 0)
        {
            if ((uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION) && (lParam & ENDSESSION_CRITICAL) != 0)
            {                                               // "critical shutdown" (i log off) = no time for discussion, cancel everything to avoid leaving anything behind
                                                            // "incomplete" mess on the disk
                if (uMsg == WM_QUERYENDSESSION)             // we only cancel on the first critical shutdown message
                    ProgressDlgArray.PostCancelToAllDlgs(); // Dialogues and workers run in their own threads, there is a chance they will finish
            }
            else // We will notify it in the window and wait until everything finishes, because WM_ENDSESSION cannot come here
            {
                if (uMsg == WM_QUERYENDSESSION && HLanguage != NULL &&
                    LoadStringW(HLanguage, IDS_BLOCKSHUTDOWNDISKOPER, blockReason, _countof(blockReason)))
                {
                    MyShutdownBlockReasonCreate(HWindow, blockReason);
                }
                CExitingOpenSal dlg(HWindow);
                INT_PTR res = dlg.Execute();
                if (uMsg == WM_QUERYENDSESSION)
                    MyShutdownBlockReasonDestroy(HWindow);
                if (res == IDCANCEL)
                {
                    if (uMsg == WM_QUERYENDSESSION)
                        TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: user rejects to close all disk operation progress dialogs");
                    // user still doesn't want to finish
                    return 0; // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION
                }
                UpdateWindow(HWindow);
            }
        }

        // critical shutdown cannot be declined, alternative solution: we will not save the configuration, we will only perform
        // nejhorsi uklid, a pak soft ukoncime (mozna nas sestreli drive, aktualni rezim: zabiti do 5s),
        // For simplicity, we do not proceed to close the Find and Viewer windows, the first one is unnecessary,
        // second would not hurt (files would disappear from TEMP)
        if (endAfterCleanup)
        { // always true: uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0
            // we are waiting for the completion of disk operations max. 5s after receiving WM_QUERYENDSESSION
            while (ProgressDlgArray.RemoveFinishedDlgs() > 0 &&
                   GetTickCount() - msgArrivalTime <= QUERYENDSESSION_TIMEOUT - 200)
                Sleep(200);
            WaitInEndSession = TRUE;
            return TRUE; // we continue to WM_ENDSESSION, where we will either finish or be killed during the next wait
        }

        int i = 0;
        TDirectArray<HWND> destroyArray(10, 5); // array of windows designated for destruction
        if (uMsg != WM_ENDSESSION)
        {
            BeginStopRefresh(); // we no longer want any panel refreshes

            // we will select all windows of find
            FindDialogQueue.AddToArray(destroyArray);
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::2");

        if (uMsg != WM_ENDSESSION)
        {
            HCURSOR hOldCursor = NULL;
            CWaitWindow closingFindWin(HWindow, IDS_CLOSINGFINDWINDOWS, FALSE, ooStatic);
            BOOL showCloseFindWin = destroyArray.Count > 0;
            if (showCloseFindWin)
            {
                if (uMsg == WM_QUERYENDSESSION && HLanguage != NULL &&
                    LoadStringW(HLanguage, IDS_BLOCKSHUTDOWNFINDFILES, blockReason, _countof(blockReason)))
                {
                    MyShutdownBlockReasonCreate(HWindow, blockReason);
                }

                hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
                closingFindWin.Create();
                EnableWindow(HWindow, FALSE);
            }

            // we will ask if the Find windows can be closed, we will stop the search in progress (on request)
            BOOL endProcessing = FALSE;
            for (i = 0; i < destroyArray.Count; i++)
            {
                if (IsWindow(destroyArray[i])) // if the window still exists
                {
                    BOOL canclose = TRUE; // in case SendMessage fails

                    WindowsManager.CS.Enter(); // We don't want any changes above WindowsManager
                    CFindDialog* findDlg = (CFindDialog*)WindowsManager.GetWindowPtr(destroyArray[i]);
                    if (findDlg != NULL) // if the window still exists, send it a request to close (otherwise it's pointless)
                    {
                        BOOL myPost = findDlg->StateOfFindCloseQuery == sofcqNotUsed;
                        if (myPost) // if it's not about nesting (it's probably possible, I haven't explored it, but unlikely)
                        {
                            findDlg->StateOfFindCloseQuery = sofcqSentToFind;
                            PostMessage(destroyArray[i], WM_USER_QUERYCLOSEFIND, 0,
                                        uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0); // During a critical shutdown, we do not ask, we cancel straight away
                        }
                        BOOL cont = TRUE;
                        while (cont)
                        {
                            cont = FALSE;
                            WindowsManager.CS.Leave();
                            // we are pretending to be a "responding" soft, so pump messages
                            MSG msg;
                            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                            {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                            // Give the Find thread some time to react
                            Sleep(50);
                            // time to test if our query has been answered yet
                            WindowsManager.CS.Enter(); // We don't want any changes above WindowsManager
                            findDlg = (CFindDialog*)WindowsManager.GetWindowPtr(destroyArray[i]);
                            if (findDlg != NULL) // We only solve if the window still exists (otherwise it's pointless)
                            {
                                if (findDlg->StateOfFindCloseQuery == sofcqCanClose ||
                                    findDlg->StateOfFindCloseQuery == sofcqCannotClose)
                                { // Decision made, we're finishing
                                    if (findDlg->StateOfFindCloseQuery == sofcqCannotClose)
                                        canclose = FALSE;
                                    if (myPost)
                                        findDlg->StateOfFindCloseQuery = sofcqNotUsed;
                                }
                                else
                                    cont = TRUE; // we are still waiting for a response from the Find thread
                            }
                        }
                    }
                    WindowsManager.CS.Leave();

                    if (!canclose)
                    {
                        if (uMsg == WM_QUERYENDSESSION)
                        {
                            TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: unable to close all Find windows");
                            MyShutdownBlockReasonDestroy(HWindow);
                        }
                        if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                        {
                            // EndStopRefresh(); // do not end stop-refresh during critical shutdown (refreshes are being sent to the panel)
                            endAfterCleanup = TRUE; // cannot refuse = let's at least tidy up
                        }
                        else
                        {
                            EndStopRefresh();
                            endProcessing = TRUE;
                        }
                        break;
                    }
                }
            }

            if (showCloseFindWin)
            {
                EnableWindow(HWindow, TRUE);
                DestroyWindow(closingFindWin.HWindow);
                SetCursor(hOldCursor);
            }

            if (endProcessing)
                return 0; // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION

            // Let Findu close the windows in its own thread
            // We do not do it during critical shutdown: closing Findu windows is unnecessary (even if not saved)
            // number of columns, window size, and a few other nonsense, but we ignore that)
            // Note: the test !endAfterCleanup is unnecessary here, as it is outside of critical shutdown
            // endAfterCleanup is always FALSE
            if (uMsg != WM_QUERYENDSESSION || (lParam & ENDSESSION_CRITICAL) == 0) // outside of critical shutdown
            {
                for (i = 0; i < destroyArray.Count; i++)
                {
                    if (IsWindow(destroyArray[i])) // if the window still exists
                        SendMessage(destroyArray[i], WM_USER_CLOSEFIND, 0, 0);
                }
            }

            if (showCloseFindWin && !endAfterCleanup && uMsg == WM_QUERYENDSESSION)
                MyShutdownBlockReasonDestroy(HWindow);

            if (!endAfterCleanup)
            {
                // destroying view windows (they are not child windows -> they do not receive WM_DESTROY automatically)
                // We also do it during critical shutdown to lubricate files in TEMP,
                // it could be harmful (e.g. viewer with decrypted file after closing
                // Start shredding the temporary file and our system will kill us during shredding,
                // A better option is to have the shredding done properly after the system restarts,
                // this is handled in the DeleteTmpCopy() method, it is not shredded during critical shutdown)
                ViewerWindowQueue.BroadcastMessage(WM_CLOSE, 0, 0);

                // Wait before unloading the plugin - if there are windows of Find and internal viewer,
                // here they have time to finish their work (they can hold Encryptu files)
                int winsCount = ViewerWindowQueue.GetWindowCount() + FindDialogQueue.GetWindowCount();
                int timeOut = 3;
                while (winsCount > 0 && timeOut--)
                {
                    Sleep(100);
                    int c = ViewerWindowQueue.GetWindowCount() + FindDialogQueue.GetWindowCount();
                    if (winsCount > c) // Currently, windows are still closing, we will wait for at least 300 ms
                    {
                        winsCount = c;
                        timeOut = 3;
                    }
                }
            }
        }

        // critical shutdown cannot be declined, alternative solution: we will not save the configuration, we will only perform
        // nejhorsi uklid, a pak soft ukoncime (mozna nas sestreli drive, aktualni rezim: zabiti do 5s),
        if (endAfterCleanup)
        {
            // always true: uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0
            // we are waiting for the completion of disk operations max. 5s after receiving WM_QUERYENDSESSION
            while (ProgressDlgArray.RemoveFinishedDlgs() > 0 &&
                   GetTickCount() - msgArrivalTime <= QUERYENDSESSION_TIMEOUT - 200)
                Sleep(200);

            WaitInEndSession = TRUE;
            return TRUE; // we continue to WM_ENDSESSION, where we will either finish or be killed during the next wait
        }

        if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0) // It's about Vista+
        {
            BOOL cfgOK = FALSE;
            if (SALAMANDER_ROOT_REG != NULL)
            {
                // we will secure exclusive access to the registry configuration
                LoadSaveToRegistryMutex.Enter();

                HKEY salamander;
                if (OpenKeyAux(NULL, HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
                {
                    DWORD saveInProgress;
                    if (!GetValueAux(NULL, salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
                    { // It's not about a damaged configuration
                        cfgOK = TRUE;
                    }
                    CloseKeyAux(salamander);
                }
                if (!cfgOK)
                    LoadSaveToRegistryMutex.Leave(); // we no longer work with the configuration, we will leave the section
                                                     // NOTE: We will call LoadSaveToRegistryMutex.Leave() in WM_ENDSESSION after saving CFG (below)
            }

            BOOL backupOK = FALSE;
            if (cfgOK) // old configuration looks OK, we are backing it up in case of configuration saving failure
            {
                char backup[200];
                sprintf_s(backup, "%s.backup.63A7CD13", SALAMANDER_ROOT_REG); // "63A7CD13" is a prevention of key name collision with user
                SHDeleteKey(HKEY_CURRENT_USER, backup);                       // Delete old backup if it exists
                HKEY salBackup;
                if (!OpenKeyAux(NULL, HKEY_CURRENT_USER, backup, salBackup)) // test non-existence of backup
                {
                    if (CreateKeyAux(NULL, HKEY_CURRENT_USER, backup, salBackup)) // creating a key for backup
                    {
                        // zkousel jsem i RegCopyTree (bez KEY_ALL_ACCESS neslapalo) a rychlost byla stejna jako SHCopyKey
                        if (SHCopyKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salBackup, 0) == ERROR_SUCCESS)
                        { // creating backup
                            DWORD copyIsOK = 1;
                            if (SetValueAux(NULL, salBackup, SALAMANDER_COPY_IS_OK, REG_DWORD, &copyIsOK, sizeof(DWORD)))
                                backupOK = TRUE;
                        }
                        CloseKeyAux(salBackup);
                    }
                }
                else
                    CloseKeyAux(salBackup);
                if (!backupOK)
                    LoadSaveToRegistryMutex.Leave(); // we no longer work with the configuration, we will leave the section
            }

            // we are waiting for the completion of disk operations max. 5s after receiving WM_QUERYENDSESSION
            while (ProgressDlgArray.RemoveFinishedDlgs() > 0 &&
                   GetTickCount() - msgArrivalTime <= QUERYENDSESSION_TIMEOUT - 200)
                Sleep(200);

            if (backupOK)                   // is backed up, we will save the configuration in WM_ENDSESSION,
                SaveCfgInEndSession = TRUE; // if we are killed in the process, load the configuration from the backup
            else
            {
                // EndStopRefresh();  // pri critical shutdown neukoncime stop-refreshe (rozesilaji se refreshe do panelu)
                WaitInEndSession = TRUE; // Failed to back up, we will not risk saving the configuration
            }
            return TRUE; // we want 5s in WM_ENDSESSION, so we return TRUE
        }

        if ((uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION) && HLanguage != NULL &&
            LoadStringW(HLanguage, IDS_BLOCKSHUTDOWNSAVECFG, blockReason, _countof(blockReason)))
        {
            MyShutdownBlockReasonCreate(HWindow, blockReason);
        }

        HCURSOR hOldCursor = NULL;
        CWaitWindow analysing(HWindow, IDS_SAVINGCONFIGURATION, FALSE, ooStatic, TRUE);
        HWND oldPluginMsgBoxParent = PluginMsgBoxParent;
        BOOL shutdown = uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION;
        if (shutdown) // During shutdown / log-off / restart, we will show a wait window for all Saves (including plugins) + we will process the message loop (to prevent being declared as "not responding" and killed prematurely)
        {
            // start a thread that will work with the registry while saving the configuration,
            // this (main) thread will meanwhile pump messages in the message loop
            RegistryWorkerThread.StartThread();

            hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            analysing.SetProgressMax(7 /* number from CMainWindow::SaveConfig() -- NECESSARY TO SYNCHRONIZE !!!*/ +
                                     Plugins.GetPluginSaveCount()); // Wait a minute, let them enjoy the view at 100%
            analysing.Create();
            GlobalSaveWaitWindow = &analysing;
            GlobalSaveWaitWindowProgress = 0;
            EnableWindow(HWindow, FALSE);

            // SaveConfiguration plug-in will also be called -> necessary setting of the parent for their message boxes
            PluginMsgBoxParent = analysing.HWindow;
        }

        // we declare "critical shutdown", all routines should behave accordingly and terminate everything as quickly as possible
        CriticalShutdown = uMsg == WM_ENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0;

        // Unload all plug-ins (paths in panels can be moved to fixed-drive)
        SetDoNotLoadAnyPlugins(TRUE); // for now because of thumbnails
        if (!Plugins.UnloadAll(shutdown ? analysing.HWindow : HWindow))
        {
            SetDoNotLoadAnyPlugins(FALSE);

            if (uMsg == WM_QUERYENDSESSION)
                TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: unable to unload all plugins");

        EXIT_WM_USER_CLOSE_MAINWND:
            if (shutdown)
            {
                GlobalSaveWaitWindow = NULL;
                GlobalSaveWaitWindowProgress = 0;
                EnableWindow(HWindow, TRUE);
                PluginMsgBoxParent = oldPluginMsgBoxParent;
                DestroyWindow(analysing.HWindow);
                SetCursor(hOldCursor);

                // we stop the thread that was working with the registry while saving the configuration...
                RegistryWorkerThread.StopThread();
            }
            if (uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION)
                MyShutdownBlockReasonDestroy(HWindow);

            if (uMsg != WM_ENDSESSION) // During critical shutdown, we do not stop the stop-refresh (refreshes are being sent to the panel)
            {
                EndStopRefresh();
                return 0; // We reject closing/shutdown/logoff, we detect any "forced shutdown" only in WM_ENDSESSION
            }
            else
            {
                // waiting for disk operations to complete, the system may kill our process earlier
                while (ProgressDlgArray.RemoveFinishedDlgs() > 0)
                    Sleep(200);
                CriticalShutdown = FALSE; // just for fun
                return 0;                 // end of software
            }
        }

        // if there are windows CShellExecuteWnd, we offer interrupt closing or sending a bug report + terminate
        char reason[BUG_REPORT_REASON_MAX]; // cause of the problem + list of windows (multiline)
        strcpy(reason, "Some faulty shell extension has locked our main window.");
        if (EnumCShellExecuteWnd(shutdown ? analysing.HWindow : HWindow,
                                 reason + (int)strlen(reason), BUG_REPORT_REASON_MAX - ((int)strlen(reason) + 1)) > 0)
        {
            // ask whether Salamander should continue or generate a bug report
            if (CriticalShutdown || // It doesn't make sense to ask anything during a critical shutdown, we will let ourselves be terminated peacefully
                SalMessageBox(shutdown ? analysing.HWindow : HWindow,
                              LoadStr(IDS_SHELLEXTBREAK3), SALAMANDER_TEXT_VERSION,
                              MSGBOXEX_CONTINUEABORT | MB_ICONINFORMATION) != IDABORT)
            {
                if (uMsg == WM_QUERYENDSESSION)
                    TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: some faulty shell extension has locked our main window");
                goto EXIT_WM_USER_CLOSE_MAINWND; // we should continue
            }

            // Let's break
            strcpy(BugReportReasonBreak, reason);
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // Freeze this thread
            while (1)
                Sleep(1000);
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::3");

        // Ask the panel if we can finish
        if (LeftPanel != NULL && RightPanel != NULL)
        {
            BOOL canClose = FALSE;
            BOOL detachFS1, detachFS2;
            if (LeftPanel->PrepareCloseCurrentPath(shutdown ? analysing.HWindow : LeftPanel->HWindow, TRUE, FALSE, detachFS1, FSTRYCLOSE_UNLOADCLOSEFS /* unnecessary - plugins (and FS) are already unloadable*/))
            {
                if (RightPanel->PrepareCloseCurrentPath(shutdown ? analysing.HWindow : RightPanel->HWindow, TRUE, FALSE, detachFS2, FSTRYCLOSE_UNLOADCLOSEFS /* unnecessary - plugins (and FS) are already unloadable*/))
                {
                    canClose = TRUE; // just both at once, otherwise we don't close either
                    if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
                        RightPanel->SleepIconCacheThread();
                    RightPanel->CloseCurrentPath(shutdown ? analysing.HWindow : RightPanel->HWindow, FALSE, detachFS2, FALSE, FALSE, TRUE); // close the right panel

                    // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                    RightPanel->ListBox->SetItemsCount(0, 0, 0, TRUE);
                    RightPanel->SelectedCount = 0;
                    // When WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
                    // scrollbars. It can be delivered by the message loop when creating a message box.
                    // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
                    PostMessage(RightPanel->HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                if (canClose)
                {
                    if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
                        LeftPanel->SleepIconCacheThread();
                    LeftPanel->CloseCurrentPath(shutdown ? analysing.HWindow : LeftPanel->HWindow, FALSE, detachFS1, FALSE, FALSE, TRUE); // close the left panel

                    // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                    LeftPanel->ListBox->SetItemsCount(0, 0, 0, TRUE);
                    LeftPanel->SelectedCount = 0;
                    // When WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
                    // scrollbars. It can be delivered by the message loop when creating a message box.
                    // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
                    PostMessage(LeftPanel->HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                else
                    LeftPanel->CloseCurrentPath(shutdown ? analysing.HWindow : LeftPanel->HWindow, TRUE, detachFS1, FALSE, FALSE, TRUE); // Cancel the close of the left panel
            }
            if (!canClose)
            {
                SetDoNotLoadAnyPlugins(FALSE);
                if (uMsg == WM_QUERYENDSESSION)
                    TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: unable to close paths in panels");
                goto EXIT_WM_USER_CLOSE_MAINWND; // panels cannot be closed
            }
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::4");

        // !!! WARNING: from this point onwards (until DestroyWindow) no interruption must occur,
        // When unpacking the window, the user finds that both panels are empty (the listing is released)
        // (this is already violated during Shutdown / Log Off / Restart, because we have to distribute
        // messages, otherwise they consider us "not responding" and the system will kill us prematurely)

        if (StrICmp(Configuration.SLGName, Configuration.LoadedSLGName) != 0) // if the user changed the language to Salama
        {
            Plugins.ClearLastSLGNames(); // to potentially trigger a new selection of the alternative language for all plugins
            Configuration.UseAsAltSLGInOtherPlugins = FALSE;
            Configuration.AltPluginSLGName[0] = 0;
        }

        if (Configuration.AutoSave)
            SaveConfig();

        if (uMsg == WM_ENDSESSION)
            LoadSaveToRegistryMutex.Leave(); // paired with Enter() called when receiving WM_QUERYENDSESSION

        if (shutdown)
        {
            GlobalSaveWaitWindow = NULL;
            GlobalSaveWaitWindowProgress = 0;
            EnableWindow(HWindow, TRUE);
            PluginMsgBoxParent = oldPluginMsgBoxParent;
            DestroyWindow(analysing.HWindow);
            SetCursor(hOldCursor);

            // we stop the thread that was working with the registry while saving the configuration...
            RegistryWorkerThread.StopThread();
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::5");

        DiskCache.PrepareForShutdown(); // We will now clean up empty tmp directories from the disk

        //      if (TipOfTheDayDialog != NULL)
        //        DestroyWindow(TipOfTheDayDialog->HWindow);  // dialog uz ma sva data ulozena (transfer tam probiha runtime)

        MainWindowCS.SetClosed();

        CanDestroyMainWindow = TRUE; // It is now safe to call DestroyWindow on MainWindow

        DestroyWindow(HWindow);

        // WM_QUERYENDSESSION and WM_ENDSESSION: all Windows will kill the process as soon as they shut down
        // removes the main window, so the following is dead code during shutdown

        CriticalShutdown = FALSE; // just for fun

        if (uMsg == WM_QUERYENDSESSION)
        {
            TRACE_I("WM_QUERYENDSESSION: allowing shutdown...");
            // main window is already closed = there is no one to deliver WM_ENDSESSION, WaitInEndSession
            // There is no need to set SaveCfgInEndSession either
            return TRUE; // if it has come this far, we will allow shut-down
        }
        return 0; // Return from WM_USER_CLOSE_MAINWND, WM_USER_FORCECLOSE_MAINWND, and WM_ENDSESSION
    }

    case WM_ERASEBKGND:
    {
        /*        HDC dc = (HDC)wParam;
      HPEN oldPen = (HPEN)SelectObject(dc, BtnFacePen);
      MoveToEx(dc, 0, 0, NULL);
      LineTo(dc, 0, WindowHeight - 1);
      LineTo(dc, WindowWidth - 1, WindowHeight - 1);
      LineTo(dc, WindowWidth - 1, 0);
      SelectObject(dc, oldPen);*/
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC dc = HANDLES(BeginPaint(HWindow, &ps));
        HPEN oldPen = (HPEN)SelectObject(dc, BtnShadowPen);

        RECT r;
        if (TopToolBar->HWindow != NULL)
        {
            MoveToEx(dc, 0, 0, NULL);
            LineTo(dc, WindowWidth + 1, 0);
            SelectObject(dc, BtnHilightPen);
            MoveToEx(dc, 0, 1, NULL);
            LineTo(dc, WindowWidth + 1, 1);
        }

        if (PanelsHeight > 0)
        {
            r.left = SplitPositionPix;
            r.top = TopRebarHeight;
            r.right = SplitPositionPix + MainWindow->GetSplitBarWidth();
            //        SelectObject(dc, shadowPen);
            //        MoveToEx(dc, r.left, r.top, NULL);
            //        LineTo(dc, r.right, r.top);
            //        SelectObject(dc, lightPen);
            //        MoveToEx(dc, r.left, r.top + 1, NULL);
            //        LineTo(dc, r.right, r.top + 1);
            r.bottom = r.top + PanelsHeight;
            FillRect(dc, &r, HDialogBrush);

            SelectObject(dc, BtnFacePen);
            MoveToEx(dc, 0, 0, NULL);
            LineTo(dc, 0, WindowHeight - 1);
            LineTo(dc, WindowWidth - 1, WindowHeight - 1);
            LineTo(dc, WindowWidth - 1, 0);
        }

        if (EditWindow->HWindow != NULL)
        {
            r.left = 0;
            r.top = TopRebarHeight + PanelsHeight;
            r.right = WindowWidth;
            r.bottom = r.top + 2;
            FillRect(dc, &r, HDialogBrush);
        }

        if (BottomToolBar->HWindow != NULL)
        {
            r.left = 0;
            r.top = TopRebarHeight + PanelsHeight + EditHeight;
            r.right = WindowWidth;
            r.bottom = r.top + 2;
            FillRect(dc, &r, HDialogBrush);
        }

        SelectObject(dc, oldPen);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_DESTROY:
    {
        if (!CanDestroyMainWindow)
        {
            // Some crazy shell extension just called DestroyWindow on the main window of Salamander

            MSG msg; // we are emptying the message queue (WMP9 buffered Enter and unlocked OK for us)
            // while (PeekMessage(&msg, HWindow, 0, 0, PM_REMOVE));  // Petr: replaced just by discarding keyboard messages (without TranslateMessage and DispatchMessage there is a risk of an infinite loop, discovered during unloading of Automation with memory leaks, before displaying the message box about leaks, an infinite loop occurred, WM_PAINT messages were constantly added to the queue and we were discarding them again)
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;

            // Ask the user to send us a break report
            SalMessageBox(HWindow, LoadStr(IDS_SHELLEXTBREAK), SALAMANDER_TEXT_VERSION,
                          MB_OK | MB_ICONSTOP);

            // and let's take a break
            strcpy(BugReportReasonBreak, "Some faulty shell extension destroyed our main window.");
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // Freeze this thread
            // MainWindow doesn't exist anymore, we would crash at the nearest possible opportunity
            while (1)
                Sleep(1000);
        }

        // Let the list of processes know that we are finishing
        TaskList.SetProcessState(PROCESS_STATE_ENDING, NULL);

        UserMenuIconBkgndReader.EndProcessing();

        SHChangeNotifyRelease(); // Shell Notifications are still not being accepted
        KillTimer(HWindow, IDT_ADDNEWMODULES);
        HANDLES(RevokeDragDrop(HWindow));
        if (Configuration.StatusArea)
            RemoveTrayIcon();
        //--- deleting child window
        if (EditWindow != NULL)
        {
            if (EditWindow->HWindow != NULL)
                DestroyWindow(EditWindow->HWindow);
            delete EditWindow;
            EditWindow = NULL;
        }
        if (TopToolBar != NULL)
        {
            if (TopToolBar->HWindow != NULL)
                DestroyWindow(TopToolBar->HWindow);
            delete TopToolBar;
            TopToolBar = NULL;
        }
        if (PluginsBar != NULL)
        {
            if (PluginsBar->HWindow != NULL)
                DestroyWindow(PluginsBar->HWindow);
            delete PluginsBar;
            PluginsBar = NULL;
        }
        if (MiddleToolBar != NULL)
        {
            if (MiddleToolBar->HWindow != NULL)
                DestroyWindow(MiddleToolBar->HWindow);
            delete MiddleToolBar;
            MiddleToolBar = NULL;
        }
        if (UMToolBar != NULL)
        {
            if (UMToolBar->HWindow != NULL)
                DestroyWindow(UMToolBar->HWindow);
            delete UMToolBar;
            UMToolBar = NULL;
        }
        if (HPToolBar != NULL)
        {
            if (HPToolBar->HWindow != NULL)
                DestroyWindow(HPToolBar->HWindow);
            delete HPToolBar;
            HPToolBar = NULL;
        }
        if (DriveBar != NULL)
        {
            if (DriveBar->HWindow != NULL)
                DestroyWindow(DriveBar->HWindow);
            delete DriveBar;
            DriveBar = NULL;
        }
        if (DriveBar2 != NULL)
        {
            if (DriveBar2->HWindow != NULL)
                DestroyWindow(DriveBar2->HWindow);
            delete DriveBar2;
            DriveBar2 = NULL;
        }
        if (BottomToolBar != NULL)
        {
            if (BottomToolBar->HWindow != NULL)
                DestroyWindow(BottomToolBar->HWindow);
            delete BottomToolBar;
            BottomToolBar = NULL;
        }
        if (MenuBar != NULL)
        {
            if (MenuBar->HWindow != NULL)
                DestroyWindow(MenuBar->HWindow);
            delete MenuBar;
            MenuBar = NULL;
        }
        SetMessagesParent(NULL);
        PostQuitMessage(0);
        break;
    }

    case WM_USER_ICON_NOTIFY:
    {
        UINT uID = (UINT)wParam;
        if (uID != TASKBAR_ICON_ID)
            break;
        UINT uMouseMsg = (UINT)lParam;
        if (uMouseMsg == WM_LBUTTONDOWN)
        {
            if (!IsWindowVisible(HWindow))
            {
                ShowWindow(HWindow, SW_SHOW);
                if (IsIconic(HWindow))
                    ShowWindow(HWindow, SW_RESTORE);
            }
            else
            {
                SetForegroundWindow(GetLastActivePopup(HWindow));
            }
        }
        if (uMouseMsg == WM_LBUTTONDBLCLK)
        {
            if (GetActiveWindow() == HWindow)
            {
                ShowWindow(HWindow, SW_MINIMIZE);
                ShowWindow(HWindow, SW_HIDE);
            }
        }
        if (uMouseMsg == WM_RBUTTONDOWN)
        {
            /* used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() call below...
MENU_TEMPLATE_ITEM TaskBarIconMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_CONTEXTMENU_EXIT
  {MNTT_PE, 0
};*/
            HMENU hMenu = CreatePopupMenu();
            InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, CM_EXIT, LoadStr(IDS_CONTEXTMENU_EXIT));

            POINT p;
            GetCursorPos(&p);

            DWORD cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                                       p.x, p.y, 0, HWindow, NULL);
            DestroyMenu(hMenu);
            if (cmd != 0)
                PostMessage(HWindow, WM_COMMAND, CM_EXIT, 0);
        }
        break;
    }

#if (_MSC_VER < 1700)
    // handle messages sent from the file manager extension
    case FM_GETDRIVEINFOW:
    {
        TRACE_E("FM_GETDRIVEINFOW not implemented");
        break;
    }

    case FM_GETFILESELW:
    {
        TRACE_E("FM_GETFILESELW not implemented");
        break;
    }

    case FM_GETFILESELLFNW:
    {
        if (!GetActivePanel()->Is(ptDisk))
            return 0; // we only walk over the disk

        int index = (int)wParam;
        FMS_GETFILESELW* fs = (FMS_GETFILESELW*)lParam;
        CFilesWindow* activePanel = GetActivePanel();

        int count = activePanel->GetSelCount();
        if (count != 0)
        {
            // Retrieve the index n-th (index) selected item
            int totalCount = activePanel->Dirs->Count + activePanel->Files->Count;
            if (totalCount == 0 || index >= totalCount)
                return 0;
            int selectedCount = 0;
            int i;
            for (i = 0; i < totalCount; i++)
            {
                CFileData* f = (i < activePanel->Dirs->Count) ? &activePanel->Dirs->At(i) : &activePanel->Files->At(i - activePanel->Dirs->Count);
                if (f->Selected == 1)
                {
                    if (index == selectedCount)
                    {
                        index = i;
                        break;
                    }
                    selectedCount++;
                }
            }
        }
        else
        {
            index = GetActivePanel()->GetCaretIndex();
        }

        CFileData* f;
        f = (index < GetActivePanel()->Dirs->Count) ? &GetActivePanel()->Dirs->At(index) : &GetActivePanel()->Files->At(index - GetActivePanel()->Dirs->Count);

        char buff[MAX_PATH];
        strcpy(buff, GetActivePanel()->GetPath());
        if (buff[strlen(buff) - 1] != '\\')
            strcat(buff, "\\");
        strcat(buff, f->Name);
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buff, -1, fs->szName, sizeof(fs->szName) / 2);
        fs->szName[sizeof(fs->szName) / 2 - 1] = 0;
        fs->ftTime = f->LastWrite;
        fs->dwSize = f->Size.LoDWord;
        fs->bAttr = (BYTE)f->Attr;
        return 0;
    }

    case FM_GETFOCUS:
    {
        return FMFOCUS_DIR;
    }

    case FM_GETSELCOUNT:
    {
        TRACE_E("FM_GETSELCOUNT not implemented");
        return 0;
    }

    case FM_GETSELCOUNTLFN:
    {
        if (!GetActivePanel()->Is(ptDisk))
            return 0; // we only walk over the disk

        CFilesWindow* activePanel = GetActivePanel();

        if (activePanel->Dirs->Count + activePanel->Files->Count == 0)
            return 0;
        int count = GetActivePanel()->GetSelCount();
        if (count == 0)
        {
            int index = GetActivePanel()->GetCaretIndex();
            if (index == 0 && GetActivePanel()->Dirs->Count > 0 &&
                strcmp(GetActivePanel()->Dirs->At(0).Name, "..") == 0)
                count = 0;
            else
                count = 1;
        }
        return count;
    }

    case FM_REFRESH_WINDOWS:
    {
        CFilesWindow* panel = GetActivePanel();
        if (panel != NULL && panel->Is(ptDisk))
        {
            //--- refresh non-automatically refreshed directories
            // change in the directory displayed in the panel and preferably also in subdirectories (who knows what the system is doing)
            PostChangeOnPathNotification(panel->GetPath(), TRUE);
        }
        break;
    }

    case FM_RELOAD_EXTENSIONS:
    {
        break;
    }
#endif // _MSC_VER < 1700

    default:
    {
        if (uMsg == TaskbarRestartMsg && Configuration.StatusArea)
            AddTrayIcon();
        if (TaskbarBtnCreatedMsg != 0 && uMsg == TaskbarBtnCreatedMsg)
            TaskBarList3.Init(HWindow);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
