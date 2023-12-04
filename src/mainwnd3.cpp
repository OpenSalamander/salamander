// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <shlwapi.h>
#undef PathIsPrefix // jinak kolize s CSalamanderGeneral::PathIsPrefix

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

// critical shutdown: kolik maximalne casu muzeme stravit ve WM_QUERYENDSESSION (pak prijde
// KILL od woken), je to 5s (5s pri otevrenem msgboxu, 10s bez pumpovani zprav), nechal jsem
// rezervu 500ms, zkousel jsem na Vista, Win7, Win8, Win10
#define QUERYENDSESSION_TIMEOUT 4500

// promenne pouzite behem ukladani konfigurace pri shutdownu, log-offu nebo restartu (musime
// pumpovat zpravy, aby nas system nesestrelil jako "not responding" softik)
CWaitWindow* GlobalSaveWaitWindow = NULL; // pokud existuje globalni wait okenko pro Save, je zde (jinak je zde NULL)
int GlobalSaveWaitWindowProgress = 0;     // aktualni hodnota progresu globalniho wait okenka pro Save

// pujcime si konstanty z novejsiho SDK
#define WM_APPCOMMAND 0x0319
#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_KEY 0
#define FAPPCOMMAND_OEM 0x1000
#define FAPPCOMMAND_MASK 0xF000
#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define APPCOMMAND_BROWSER_BACKWARD 1
#define APPCOMMAND_BROWSER_FORWARD 2
/* zatim nepodporujeme
#define APPCOMMAND_BROWSER_SEARCH         5
#define APPCOMMAND_HELP                   27
#define APPCOMMAND_BROWSER_REFRESH        3
#define APPCOMMAND_FIND                   28
#define APPCOMMAND_COPY                   36
#define APPCOMMAND_CUT                    37
#define APPCOMMAND_PASTE                  38
*/

const int SPLIT_LINE_WIDTH = 3; // sirka Split line v bodech
// pokud je zobrazena middle toolbar, bude slozeni SPLIT_LINE_WIDTH + toolbar + SPLIT_LINE_WIDTH

const int MIN_WIN_WIDTH = 2; // minimalni sirka panelu

extern BOOL CacheNextSetFocus;

BOOL MainFrameIsActive = FALSE;

// kod pro testovani casovych ztrat
/*
  const char *s1 = "aj hjka sakjSJKAHS AJKSH JKDSHFJSDH FJS HDFJSD HFJS";
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
  MessageBox(HWindow, buff, "Results", MB_OK);
*/

//****************************************************************************
//
// HtmlHelp support
//

// univerzalni callback pro nas MessagBox, kdyz uzivatel klikne na tlacitko HELP
// je treba volat napr takto:
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
            { // neexistuje adresar ziskany z aktualniho .slg souboru Salamandera
                lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
                if (_stricmp(helpSubdir, "english") == 0 || // "english" uz jsme testovali a neexistuje, takze znovu to zkouset nema smysl
                    !SalPathAppend(helpPath, "english", MAX_PATH) ||
                    !DirExists(helpPath))
                { // neexistuje adresar ENGLISH
                    lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
                    if (SalPathAppend(helpPath, "*", MAX_PATH))
                    { // zkusime najit aspon nejaky jiny adresar
                        WIN32_FIND_DATA data;
                        HANDLE find = HANDLES_Q(FindFirstFile(helpPath, &data));
                        if (find != INVALID_HANDLE_VALUE)
                        {
                            do
                            {
                                if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0 &&
                                    (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) // jen pokud jde o adresar
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

    if (helpFileName != NULL) // jde o help pluginu - aby se otevrelo okno helpu na spravne pozici + se
    {                         // zapamatovanymi Favourites, je nutne ho otevrit pro "salamand.chm" (nasledne
                              // se otevre help pro plugin v tomto stejnem okne)
        lstrcpyn(helpPath, CurrentHelpDir, MAX_PATH);
        if (SalPathAppend(helpPath, "salamand.chm", MAX_PATH) &&
            FileExists(helpPath))
        {
            HtmlHelp(NULL, helpPath, HH_DISPLAY_TOC, 0); // pripadnou chybu ignorujeme
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
// slouzi pouze pro posouvani tazenych obrazku
//

class CMWDropTarget : public IDropTarget
{
private:
    long RefCount; // zivotnost objektu

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
            return 0; // nesmime sahnout do objektu, uz neexistuje
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
// MyShutdownBlockReasonCreate a MyShutdownBlockReasonDestroy
//
// Vista+: dynamicky vytahneme funkce pro nastavovani / cisteni duvodu blokace shutdownu
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
        // Menu je zdestruovano primo z menu, do ktereho bylo pripojeno
        ContextMenuNew->GetMenu2()->HandleMenuMsg(uMsg, wParam, lParam); // toto volani sem tam pada
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 11))
    {
        MenuNewExceptionHasOccured++;
        if (ContextMenuNew != NULL)
            ContextMenuNew->Release(); // nahrada za volani ReleaseMenuNew
                                       //    ReleaseMenuNew();
    }
}

void CMainWindow::PostChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CMainWindow::PostChangeOnPathNotification(%s, %d)", path, includingSubdirs);

    HANDLES(EnterCriticalSection(&DispachChangeNotifCS));

    // pridame tuto notifikaci do pole (pro pozdejsi zpracovani)
    CChangeNotifData data;
    lstrcpyn(data.Path, path, MAX_PATH);
    data.IncludingSubdirs = includingSubdirs;
    ChangeNotifArray.Add(data);
    if (!ChangeNotifArray.IsGood())
        ChangeNotifArray.ResetState(); // chyby ignorujeme (prinejhorsim nerefreshnem)

    // postneme zadost o rozeslani zprav o zmenach na cestach
    HANDLES(EnterCriticalSection(&TimeCounterSection));
    int t1 = MyTimeCounter++;
    HANDLES(LeaveCriticalSection(&TimeCounterSection));
    PostMessage(HWindow, WM_USER_DISPACHCHANGENOTIF, 0, t1);

    HANDLES(LeaveCriticalSection(&DispachChangeNotifCS));
}

void CMainWindowWindowProcAux(IContextMenu* menu2, CMINVOKECOMMANDINFO& ici)
{
    CALL_STACK_MESSAGE_NONE

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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
    // Internal Viewer a Find: obnova vsech oken (doslo napr ke zmene globalnich fontu)
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
        if (i == 0) // tree zatim nezobrazujeme
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
                PostMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1); // postarame se o nove naplneni icon-cache (thumbnaily uz jsou zase mozne)
            }
            if (RightPanel->GetViewMode() == vmThumbnails)
            {
                PostMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2); // postarame se o nove naplneni icon-cache (thumbnaily uz jsou zase mozne)
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
    if (panel->GetViewMode() == vmDetailed) // panel musi bezet v detailed rezimu
    {
        if (panel->Columns.Count < 1)
            return;
        CColumn* column = &panel->Columns[0];
        BOOL leftPanel = (panel == LeftPanel);
        BOOL smartMode = !(!column->FixedWidth &&
                           (leftPanel && panel->ViewTemplate->LeftSmartMode ||
                            !leftPanel && panel->ViewTemplate->RightSmartMode));
        if (smartMode && column->FixedWidth)
        { // smart mode je jen pro elasticke sloupce (musime to zmenit v sablone pohledu)
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
        // napred nastavime aktivni panel
        if (cmdLineParams->ActivatePanel == 1 && GetActivePanel() == RightPanel ||
            cmdLineParams->ActivatePanel == 2 && GetActivePanel() == LeftPanel)
        {
            ChangePanel(FALSE);
        }
        // potom muzeme nastavit cestu v aktivnim panelu
        if (cmdLineParams->LeftPath[0] == 0 && cmdLineParams->RightPath[0] == 0 && cmdLineParams->ActivePath[0] != 0)
            GetActivePanel()->ChangeDir(cmdLineParams->ActivePath); // nema smysl kombinovat s nastavenim leveho/praveho panelu
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

    // message WM_USER_SHCHANGENOTIFY, ktera nam bude dorucena pri notifikacich prekracuje hranice procesu
    // konstantou SHCNRF_NewDelivery (zname take jako SHCNF_NO_PROXY) rikame, ze prebirame odpovednost
    // za pristup do pameti predavane zpravou (pomoci SHChangeNotification_Lock) a ze OS nema vytvaret
    // proxy windows (pozor, je hlasen bug pod XP, kde se proxy okno vytvori, ale nedestrukti):
    // http://groups.google.com/groups?selm=3CDFD449.6BA0CDB4%40ic.ac.uk&output=gplain
    //
    // pres SHCNE_ASSOCCHANGED si nechame dorucit informaci o zmene asociaci
    SHChangeNotifyRegisterID = SHChangeNotifyRegister(HWindow, SHCNRF_ShellLevel | SHCNRF_NewDelivery,
                                                      SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED | SHCNE_DRIVEREMOVED |
                                                          SHCNE_DRIVEADD | SHCNE_NETSHARE | SHCNE_NETUNSHARE |
                                                          SHCNE_DRIVEADDGUI | SHCNE_ASSOCCHANGED | SHCNE_UPDATEITEM,
                                                      WM_USER_SHCHANGENOTIFY,
                                                      1, &entry);

    // dealokace pidl
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
    // zahejbeme s velikosti ikonek

    LoadSaveToRegistryMutex.Enter(); // lidem se zmensovaly ikonky, viz https://forum.altap.cz/viewtopic.php?t=638
    // touto synchronizaci zajistime, ze si dva Salamandery nepolezou do zeli
    // bohuzel trik se zmenou "Shell Icon Size" pro rebuild cache pouziva kde kdo (vcetne Tweak UI),
    // takze pokud budou refreshovat ve stejnou dobou jako Salamander, dojde ke konfliktu
    // teto situaci se snazime predchazet odlozeni nasledujici prasarny pomoci IDT_ASSOCIATIONSCHNG

    HKEY hKey;
    if (HANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", 0, KEY_READ | KEY_WRITE, &hKey)) == ERROR_SUCCESS)
    {
        // starsi SHELL32.DLL nemuseji tento export mit a fileIconInit bude NULL
        FT_FileIconInit fileIconInit = NULL;
        fileIconInit = (FT_FileIconInit)GetProcAddress(Shell32DLL, MAKEINTRESOURCE(660)); // nema header

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
        if (val > 0) // bohuzel si (podle netu) lidi nastavuji velikost ikonek nahodile (72, 96, 128, atd), takze neni mozne odfiltrovat "divne velikosti"
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
                RegDeleteValue(hKey, "Shell Icon Size"); // zameteme po sobe
            HANDLES(RegCloseKey(hKey));

            IgnoreWM_SETTINGCHANGE = FALSE;
        }
    }

    LoadSaveToRegistryMutex.Leave();

    /*
  if (fileIconInit != NULL)
    fileIconInit(TRUE);

  // ladici zobrazeni ikonky
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
  }
  */

    // vlastni refresh asociaci
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
            DWORD netDrives; // bitove pole network disku
            GetNetworkDrives(netDrives, NULL);
            drivesMask = GetLogicalDrives() | netDrives;
        }

        CDriveBar* copyDrivesListFrom = NULL;
        if (DriveBar != NULL && DriveBar->HWindow != NULL)
        {
            if (DriveBar->GetCachedDrivesMask() != drivesMask ||
                checkCloudStorages && DriveBar->GetCachedCloudStoragesMask() != cloudStoragesMask)
            {
                // nefunguji notifikace o zmenach disku nebo zmena v dostupnosti cloud storages, prebuildime drive bar "rucne"
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
                // nefunguji notifikace o zmenach disku nebo zmena v dostupnosti cloud storages, prebuildime drive bar "rucne"
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
        SHChangeNotifyInitialize(); // nechame si dorucovat Shell Notifications

        SetTimer(HWindow, IDT_ADDNEWMODULES, 15000, NULL); // timer po 15 sekundach pro AddNewlyLoadedModulesToGlobalModulesStore()

        CMWDropTarget* dropTarget = new CMWDropTarget();
        if (dropTarget != NULL)
        {
            HANDLES(RegisterDragDrop(HWindow, dropTarget));
            dropTarget->Release(); // RegisterDragDrop volala AddRef()
        }

        HMENU h = GetSystemMenu(HWindow, FALSE);
        if (h != NULL)
        {
            int items = GetMenuItemCount(h);
            int pos = items; // pripojime nove polozky na konec seznamu

            // pokud posledni dve polozky menu jsou separator a Close, vlozime se nad ne
            // (uzivatele si dlouhodobe stezovali, ze omylem klikaji na nase AOT mito na chtene Close)
            if (items > 2)
            {
                UINT predLastCmd = GetMenuItemID(h, items - 2);
                UINT lastCmd = GetMenuItemID(h, items - 1);
                if (predLastCmd == 0 && lastCmd == SC_CLOSE)
                    pos = items - 2;
            }

            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenu() dole...
MENU_TEMPLATE_ITEM AddToSystemMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_ALWAYSONTOP
  {MNTT_PE, 0
};
*/
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

        // nechceme vizualni styly pro rebar
        // zakazeme je
        SetWindowTheme(HTopRebar, (L" "), (L" "));

        // vynutime si WS_BORDER, ktery se nekam "ztratil"
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

        //      AnimateBar = new CAnimate(HWorkerBitmap, 50, 0, RGB(255, 255, 255)); // celkem 50 policek, pri smycce jedeme od 0, bile pozadi
        //      AnimateBar = new CAnimate(HWorkerBitmap, 43, 3, RGB(0, 0, 0)); // celkem 43 policek, pri smycce jedeme od 3, cerne pozadi
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

    // case WM_CHANGEUISTATE: // zda se, ze chodi vzdy obe zpravy
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

        // informaci o zmene barev napropagujeme do rebaru
        if (HTopRebar != NULL)
            SendMessage(HTopRebar, uMsg, wParam, lParam);

        // mohlo dojit ke zmene barevne hloubky - nechame rebuildnout imagelisty a ziskat
        // nove ikony
        ColorsChanged(TRUE, FALSE, TRUE); // nechame rebuildnout vse; casu je dost
        return 0;
    }

    case WM_SETTINGCHANGE:
    {
        if (IgnoreWM_SETTINGCHANGE || LeftPanel == NULL || RightPanel == NULL) // prisel bug-report, kde je videt, ze se WM_SETTINGCHANGE dorucilo hned z WM_CREATE hlavniho okna (panelu jeste neexistovali, takze to spadlo na pristupu na NULL)
            return 0;

        // detekce dle EXPLORER.EXE pod NT4
        if (lParam != 0 && stricmp((LPCTSTR)lParam, "Environment") == 0)
        {
            // doslo ke zmene environment variables, nechame je refreshnout
            if (Configuration.ReloadEnvVariables)
                RegenEnvironmentVariables();
            return 0;
        }
        if (lParam != 0 && stricmp((LPCTSTR)lParam, "Extensions") == 0)
        {
            // doslo ke zmene asociaci, nechame je refreshnout
            //
            // tato cesta uz se asi nepouziva, je to nejaka stara vetev, ted se
            // informace rozesila pres SHCNE_ASSOCCHANGED, ale v NT4 EXPLORERu
            // maji chycenou i tuto vetev

            // odlozime o vterinu, at se nepotkame s ostatnim SW pouzivajici zmenu velikosti ikon pro reset icon cache
            if (!SetTimer(HWindow, IDT_ASSOCIATIONSCHNG, 1000, NULL))
                OnAssociationsChangedNotification(FALSE);
            return 0;
        }

        // neznama zmena, rebuildneme vse

        GotMouseWheelScrollLines = FALSE; //nechame znovu nacist pocet radek k rolovani
        InitLocales();
        SetFont(); // font panelu se standardne ridi fontem ze systemu
        SetEnvFont();

        GetShortcutOverlay();
        // Internal Viewer a Find:  obnova vsech oken (font se jiz zmenil)
        BroadcastConfigChanged();
        if (!IsIconic(HWindow))
        {
            // zajistime layoutnuti child oken - napriklad se mohly zmenit rozmery toolbar
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

    case WM_USER_SHCHANGENOTIFY: // dostavame diky SHChangeNotifyRegister
    {
        LONG wEventId;
        HANDLE hLock = NULL;

        //      TRACE_E("WM_USER_SHCHANGENOTIFY lParam="<<hex<<lParam<<" wParam="<<hex<<wParam);

        // pod novejsima shell32.dll je treba vyzadat pristup do mapovane pameti, kde jsou predany parametry
        // (mezi procesy nelze predavat pamet a tato message prisla z Explorera)
        // viz doc\interesting.zip\Shell Notifications.mht (http://www.geocities.com/SiliconValley/4942/notify.html)

        LPITEMIDLIST* ppidl;
        hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &ppidl, &wEventId); // FIXME_X64 - overit pretypovani na (DWORD)
        if (hLock == NULL)
        {
            TRACE_E("SHChangeNotification_Lock failed");
            break;
        }

        // prevedeme pidl -> cestu
        char szPath[2 * MAX_PATH];
        szPath[0] = 0; // prazdna cesta znamena zmenu vseho
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
        SHChangeNotification_Unlock(hLock); // ppidl je prelozen, muzeme uvolnit pamet
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
                // zmena v asociacich
                // odlozime o vterinu, at se nepotkame s ostatnim SW pouzivajici zmenu velikosti ikon pro reset icon cache
                if (!SetTimer(HWindow, IDT_ASSOCIATIONSCHNG, 1000, NULL))
                    OnAssociationsChangedNotification(FALSE);
            }
            else
            {
                // zmena v mediich nebo discich

                // po vlozeni media provedeme automaticky Retry v messageboxu "drive not ready"
                // (je-li zobrazen pro drive s vlozenym mediem)
                if (wEventId == SHCNE_MEDIAINSERTED)
                {
                    if (CheckPathRootWithRetryMsgBox[0] != 0 &&
                        HasTheSameRootPath(CheckPathRootWithRetryMsgBox, szPath))
                    {
                        if (LastDriveSelectErrDlgHWnd != NULL)
                            PostMessage(LastDriveSelectErrDlgHWnd, WM_COMMAND, IDRETRY, 0);
                    }
                }

                // pokud je otevrene Alt+F1/F2 menu, udelame refresh (cteme volume name)
                CFilesWindow* panel = GetActivePanel();
                if (panel != NULL)
                    PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

                // pokud jsou v panelech CD-ROM nebo vymenna media, udelame u nich refresh
                while (1)
                {
                    if ((panel->Is(ptDisk) || panel->Is(ptZIPArchive)) && !IsUNCPath(panel->GetPath()))
                    {
                        UINT type = MyGetDriveType(panel->GetPath());
                        if (type == DRIVE_CDROM || type == DRIVE_REMOVABLE)
                        {
                            HANDLES(EnterCriticalSection(&TimeCounterSection)); // sejmeme cas, kdy je treba refreshe
                            int t1 = MyTimeCounter++;
                            HANDLES(LeaveCriticalSection(&TimeCounterSection));
                            PostMessage(panel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
                        }
                        if (type == DRIVE_NO_ROOT_DIR) // zmizel device (drive je neplatny)
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

        /*
    // WM_DEVICECHANGE nechodila dobre napriklad pod Win XP pri pripojovani fotaku DSC F707
    // prisla sice informace o pripojeni zarizeni, ale nasledna detekce nazvu zarizeni
    // (pokud bylo zobrazeno Alt+F1/2 menu) pres SHGetFileInfo vracela prazdny retezec.
    // Na google jsem nasel thread, kde si manik stezuje na stejny problem
    //
    // http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&oe=UTF-8&threadm=99a435fa.0203280715.69a286a8%40posting.
    // google.com&rnum=1&prev=/groups%3Fhl%3Den%26lr%3D%26ie%3DUTF-8%26oe%3DUTF-8%26q%3Ddevice%2Bname%2Bshgetfileinfo
    //
    // a resil to waitem. Lidi mu doporucili odchod z WM_DEVICECHANGE a prechod na
    // nedokumentovanou funkci SHChangeNotifyRegister...
    // (http://www.geocities.com/SiliconValley/4942/notify.html)
    case WM_DEVICECHANGE:
    {
      if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE ||
          wParam == DBT_CONFIGCHANGED)  // vymena CD-ROM media
      {
        // pokud je otevrene Alt+F1/F2 menu, udelame refresh (cteme volume name)
        CFilesWindow *panel = GetActivePanel();
        if (panel != NULL)
          PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

        // pokud jsou v panelech CD-ROM nebo vymenna media, udelame u nich refresh
        while (1)
        {
          if (panel->Is(ptDisk) || panel->Is(ptZIPArchive))
          {
            UINT type = MyGetDriveType(panel->GetPath());
            if (type == DRIVE_CDROM || type == DRIVE_REMOVABLE)
            {
              HANDLES(EnterCriticalSection(&TimeCounterSection));  // sejmeme cas, kdy je treba refreshe
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
    }
    */

    case WM_USER_PROCESSDELETEMAN:
    {
        // odkladame zpracovani dat kvuli aktivaci hl. okna po ESC z vieweru pod WinXP (bez tyhle
        // opicarny se to nejak nestihalo - hl. okno nebylo aktivni -> safe-wait-okenko se neukazovalo)
        if (!SetTimer(HWindow, IDT_DELETEMNGR_PROCESS, 200, NULL))
            DeleteManager.ProcessData(); // kdyz nevyjde timer, udelame to hned, kasleme na WinXP
        return 0;
    }

    case WM_USER_DRIVES_CHANGE:
    {
        CFilesWindow* panel = GetActivePanel();
        if (panel->OpenedDrivesList != NULL)
        {
            // nechame rebuildnout menu
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
        // zhasnu pripadny tooltip
        SetCurrentToolTip(NULL, 0);

        // pokud nekdo monitoruje mys, ukoncim monitoring
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_QUERY;
        if (TrackMouseEvent(&tme) && tme.hwndTrack != NULL)
            SendMessage(tme.hwndTrack, WM_MOUSELEAVE, 0, 0);

        // nechame zhasnou (zase zobrazit) existujici caret, aby nerusil usera
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

        // Zajisti spravne nastaveni enableru, aby enablene polozky v menu odpovidaly
        // skutecnemu stavu. Zaroven nastavi stav spodni toolbary.
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
            // dalsi kolo zamykani (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) bude
            // v WM_USER_ENTERMENULOOP+WM_USER_LEAVEMENULOOP, ale to uz je vnorene, zadna rezie,
            // takze ignorujeme, nebudeme proti tomu nijak bojovat
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
            // user kliknul na grupu v User Menu Toolbar
            int iterator = id - CM_USERMENU_MIN;
            int endIndex = UserMenuItems->GetSubmenuEndIndex(iterator);
            if (endIndex != -1)
            {
                UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
                iterator++;
                CMenuPopup menu;
                FillUserMenu2(&menu, &iterator, endIndex);
                // dalsi kolo zamykani (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) bude
                // v WM_USER_ENTERMENULOOP+WM_USER_LEAVEMENULOOP, ale to uz je vnorene, zadna rezie,
                // takze ignorujeme, nebudeme proti tomu nijak bojovat
                menu.Track(0, r.left, r.bottom, HWindow, &r);
                UserMenuIconBkgndReader.EndUserMenuIconsInUse();
            }
        }

        if (id >= CM_PLUGINCMD_MIN && id <= CM_PLUGINCMD_MAX)
        {
            // user kliknul na ikonku pluginu v PluginsBar;
            int index2 = id - CM_PLUGINCMD_MIN; // index pluginu v CPlugions::Data
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
            LeftPanel->RepaintIconOnly(-1); // vsechny
        if (RightPanel != NULL)
            RightPanel->RepaintIconOnly(-1); // vsechny
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
            SalamanderBusy = TRUE; // ted uz je BUSY
            LastSalamanderIdleTime = GetTickCount();
            BringWindowToTop(HWindow); // tohle asi neni dulezite, ale videl jsem to v jednom samplu, takze to taky pridavam
            if (IsIconic(HWindow))
            {
                // SetForegroundWindow: tohle je veledulezite. Pokud tohle nezavolame a mame nastaveno
                // only one instance a tray, tak salamander (nekdy) vyskakuje na pozadi a teprve
                // potom se dostane nahoru - do popredi.
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

        /*
    case WM_USER_SETPATHS:
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
            SetForegroundWindow(HWindow); // abychom se dostali nad viewer
        WindowProc(WM_USER_CONFIGURATION, 3, 0);
        HWND hCaller = (HWND)wParam;
        if (IsWindow(hCaller))
        {
            // Pokud okno, ktere nas vyvolalo, stale existuje, pokusim se ho vytahnout
            // nahoru. Je to vyprasene, protoze pokud mezi tim otevre modalni dialog,
            // nedostane aktivaci on. Ale na to pecu, protoze viewer stejne (doufam)
            // pujde do salamu - tedy do pluginu ;-)
            SetForegroundWindow(hCaller);
        }
        return 0;
    }

    case WM_USER_CONFIGURATION:
    {
        if (!SalamanderBusy)
        {
            SalamanderBusy = TRUE; // ted uz je BUSY
            LastSalamanderIdleTime = GetTickCount();
        }

        BeginStopRefresh(); // cmuchal si da pohov

        BOOL oldStatusArea = Configuration.StatusArea;
        BOOL oldPanelCaption = Configuration.ShowPanelCaption;
        BOOL oldPanelZoom = Configuration.ShowPanelZoom;

        UserMenuIconBkgndReader.ResetSysColorsChanged(); // ted zaciname sledovat zmenu systemovych barev (pri zmene je nutny reload ikon user menu)
        BOOL readingUMIcons = UserMenuIconBkgndReader.IsReadingIcons();
        if (readingUMIcons) // nove ikony jsou na ceste do user menu, ukazeme je az po dokonceni cfg (pri OK nechame ikony nacist znovu, aby se necetly i pripadne pridane ikony)
            UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
        BOOL oldUseCustomPanelFont = UseCustomPanelFont;
        LOGFONT oldLogFont = LogFont;
        CConfigurationDlg dlg(HWindow, UserMenuItems, (int)wParam, (int)lParam);
        int res = dlg.Execute(LoadStr(IDS_BUTTON_OK), LoadStr(IDS_BUTTON_CANCEL),
                              LoadStr(IDS_BUTTON_HELP));
        if (readingUMIcons)
            UserMenuIconBkgndReader.EndUserMenuIconsInUse();

        // zavrel se dialog - user v nem mohl zmenit clipboard, overime to ...
        IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
        IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

        if (res == IDOK) // zmena hodnot -> refresh vseho moznyho
        {
            if (dlg.PageView.IsDirty())
            {
                // user cosi menil v konfiguraci pohledu - nechame znovu sestavit sloupce
                LeftPanel->SelectViewTemplate(LeftPanel->GetViewTemplateIndex(), TRUE, FALSE);
                RightPanel->SelectViewTemplate(RightPanel->GetViewTemplateIndex(), TRUE, FALSE);
            }
            if (memcmp(&oldLogFont, &LogFont, sizeof(LogFont)) != 0 ||
                oldUseCustomPanelFont != UseCustomPanelFont)
            {
                SetFont();
                // je-li zobrazena headerline, musime ji nastavit spravnou velikost
                LeftPanel->LayoutListBoxChilds();
                RightPanel->LayoutListBoxChilds();
            }

            if (Configuration.ThumbnailSize != LeftPanel->GetThumbnailSize() ||
                Configuration.ThumbnailSize != RightPanel->GetThumbnailSize())
            {
                // pokud se zmenil rozmer thumbnailu, je treba ho napropagovat do panelu
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

            // uzivatel mohl zapnout/vypnout Documents
            CDriveBar* copyDrivesListFrom = NULL;
            if (DriveBar != NULL && DriveBar->HWindow != NULL)
            {
                DriveBar->RebuildDrives(DriveBar); // nepotrebujeme pomalou enumeraci disku
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

            // ikonka hlavniho okna
            SetWindowIcon();
            // ikonka v progress oknech
            ProgressDlgArray.PostIconChange();

            // nastavime oboum panelum, ze se maji refreshnout
            LeftPanel->RefreshForConfig();
            RightPanel->RefreshForConfig();

            // zrusime ulozena data v SalShExtPastedData (mohlo dojit ke zmene archiveru)
            SalShExtPastedData.ReleaseStoredArchiveData();

            // Internal Viewer a Find:  obnova vsech oken (font se jiz zmenil)
            BroadcastConfigChanged();

            // rozesleme tuto novinku i mezi plug-iny
            Plugins.Event(PLUGINEVENT_CONFIGURATIONCHANGED, 0);
        }

        EndStopRefresh(); // ted uz zase cmuchal nastartuje
        return 0;
    }

    case WM_SYSCOMMAND:
    {
        if (HasLockedUI())
            break;

        // pokud uzivatel stisknul tlacitko Alt behem zobrazeneho uvodniho Splash okenka,
        // doslo ke vstupu do systemoveho menu jeste nezobrazeneho MainWindow a Splash
        // okenko zustalo otevrene do doby, kdy user stisknul Escape
        // pokud MainWindow jeste neni zobrazeno, zakazu vstup do Window menu
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
            WindowProc(WM_COMMAND, wParam, lParam); // predame dale

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
        // pochytame zpravy prichazejici zejmena z novych mysi (4 a dalsi tlacitko)
        // a multimedialnich klavesnic
        // viz https://forum.altap.cz/viewtopic.php?t=192
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
            { // prikaz pluginu (submenu pluginu v menu Plugins)
                if (Plugins.HelpForMenuItem(HWindow, LOWORD(wParam)))
                    return 0;
                else
                    id = CM_LAST_PLUGIN_CMD; // pokud plugin nema zadny svuj help, zobrazime Salamanderi help "Using Plugins"
            }

            // upravime intervaly na jejich prvni hodnotu
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

        // ukoncime quick-search mode
        if (LOWORD(wParam) != CM_ACTIVEREFRESH &&         // krom refreshe v aktivnim panelu
            LOWORD(wParam) != CM_LEFTREFRESH &&           // krom refreshe v levem panelu
            LOWORD(wParam) != CM_RIGHTREFRESH &&          // krom refreshe v pravem panelu
            (HIWORD(wParam) == 0 || HIWORD(wParam) == 1)) // jen z menu nebo akceleratoru
        {
            CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        }

        if (LOWORD(wParam) >= CM_NEWMENU_MIN && LOWORD(wParam) <= CM_NEWMENU_MAX)
        { // prikaz z menu New
            if (ContextMenuNew->MenuIsAssigned() && activePanel->CheckPath(TRUE) == ERROR_SUCCESS)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                {
                    CALL_STACK_MESSAGE1("CMainWindow::WindowProc::menu_new");
                    IContextMenu* menu2 = ContextMenuNew->GetMenu2();
                    menu2->AddRef(); // kdybychom nahodou priseli o ContextMenuNew (asynchroni zavreni - message-loopa)
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
                    activePanel->FocusFirstNewItem = TRUE; // aby doslo k vyberu nove nagenerovaneho souboru/adresare

                    CMainWindowWindowProcAux(menu2, ici);

                    menu2->Release();
                }
                //---  refresh neautomaticky refreshovanych adresaru
                // ohlasime zmenu v aktualnim adresari (novy soubor/adresar lze vytvorit snad jen v nem)
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
        { // prikaz z menu nektereho plug-inu
            // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            if (Plugins.ExecuteMenuItem(activePanel, HWindow, LOWORD(wParam)))
            {
                activePanel->StoreSelection();                               // ulozime selection pro prikaz Restore Selection
                activePanel->SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                PostMessage(activePanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
            }

            // opet zvysime prioritu threadu, operace dobehla
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            // obnova obsahu neautomatickych panelu je na plug-inech

            UpdateWindow(HWindow);
            return 0;
        }

        if (LOWORD(wParam) == CM_LAST_PLUGIN_CMD)
        { // prikaz plugins/last command
            // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            if (Plugins.OnLastCommand(activePanel, HWindow))
            {
                activePanel->StoreSelection();                               // ulozime selection pro prikaz Restore Selection
                activePanel->SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                PostMessage(activePanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
            }

            // opet zvysime prioritu threadu, operace dobehla
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            // obnova obsahu neautomatickych panelu je na plug-inech

            UpdateWindow(HWindow);
            return 0;
        }

        if (LOWORD(wParam) >= CM_USERMENU_MIN && LOWORD(wParam) <= CM_USERMENU_MAX)
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection

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
                else // bereme fokuslou polozku
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
                    userMenuAdvancedData.ListOfSelNames[0] = 0; // maly buffer pro seznam vybranych jmen
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
                else // bereme fokuslou polozku
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
                    userMenuAdvancedData.ListOfSelFullNames[0] = 0; // maly buffer pro seznam vybranych plnych jmen
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
                int selCount = activePanel->GetSelItems(3, indexes); // zajima nas: 0-2=pocet oznacenych, 3=vic nez dva
                int tgtIndexes[2];
                int tgtSelCount = inactivePanel->Is(ptDisk) ? inactivePanel->GetSelItems(2, tgtIndexes) : 0; // zajima nas: 0-1=pocet oznacenych, 2=vic nez jeden
                if (selCount == 2)                                                                           // dve oznacene polozky ve zdrojovem panelu
                {
                    if ((indexes[0] < activePanel->Dirs->Count) == (indexes[1] < activePanel->Dirs->Count)) // obe polozky jsou soubory/adresare
                    {
                        f1 = (indexes[0] < activePanel->Dirs->Count) ? &activePanel->Dirs->At(indexes[0]) : &activePanel->Files->At(indexes[0] - activePanel->Dirs->Count);
                        f2 = (indexes[1] < activePanel->Dirs->Count) ? &activePanel->Dirs->At(indexes[1]) : &activePanel->Files->At(indexes[1] - activePanel->Dirs->Count);
                        userMenuAdvancedData.CompareNamesAreDirs = (indexes[0] < activePanel->Dirs->Count);
                    }
                }
                else
                {
                    if (selCount == 1) // jedna oznacena polozka ve zdrojovem panelu
                    {
                        f1 = (indexes[0] < activePanel->Dirs->Count) ? &activePanel->Dirs->At(indexes[0]) : &activePanel->Files->At(indexes[0] - activePanel->Dirs->Count);
                        userMenuAdvancedData.CompareNamesAreDirs = (indexes[0] < activePanel->Dirs->Count);
                        if (!focusOnUpDir && focus != indexes[0] && tgtSelCount != 1)
                        {
                            if ((focus < activePanel->Dirs->Count) == userMenuAdvancedData.CompareNamesAreDirs) // obe polozky jsou soubory/adresare
                            {
                                f2 = (focus < activePanel->Dirs->Count) ? &activePanel->Dirs->At(focus) : &activePanel->Files->At(focus - activePanel->Dirs->Count);
                            }
                        }
                    }
                    else
                    {
                        if (selCount == 0) // zadna oznacena polozka ve zdrojovem panelu, bereme fokus
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
                        (tgtIndexes[0] < inactivePanel->Dirs->Count) == userMenuAdvancedData.CompareNamesAreDirs) // obe polozky jsou soubory/adresare
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
                                    if ((i < inactivePanel->Dirs->Count) == userMenuAdvancedData.CompareNamesAreDirs) // obe polozky jsou soubory/adresare
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

            /*
        case CM_HELP_KEYBOARD:
        {
          ShellExecute(HWindow, "open", "https://www.altap.cz/salam_en/features/keyboard.html", NULL, NULL, SW_SHOWNORMAL);
          return 0;
        }
*/
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
                OpenHtmlHelp(NULL, HWindow, HHCDisplayTOC, 0, TRUE); // nechceme dva messageboxy za sebou
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

            /*
        case CM_HELP_TIP:
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
                // soubor asi neexistuje - priste uz to ani nebudeme pri startu zkouset
                if (openQuiet)
                  Configuration.ShowTipOfTheDay = FALSE;
              }
            }
          }
          return 0;
        }
*/
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
            BeginStopRefresh(); // cmuchal si da pohov

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
            if (dlg.GetRefreshPanels() || // refreshneme i drivebary kvuli Nethood pluginu (mizi/objevuje se Network Neigborhood ikona)
                dlg.GetDrivesBarChange()) // zmena viditelnosti FS polozky v Drives barach
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

            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return 0;
        }

        case CM_SAVECONFIG:
        {
            // pokud existuje exportovana konfigurace, zobrazim varovani
            if (FileExists(ConfigurationName))
            {
                char buff[3000];
                _snprintf_s(buff, _TRUNCATE, LoadStr(IDS_SAVECFG_EXPFILEEXISTS), ConfigurationName);
                int ret = SalMessageBox(HWindow, buff, LoadStr(IDS_INFOTITLE),
                                        MB_ICONINFORMATION | MB_OKCANCEL);
                if (ret == IDCANCEL)
                {
                    // hodime usera do spravneho adresare a vyfokusime konfiguracni souborf, aby to mel snazsi
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
            while (*s != 0) // vytvoreni double-null terminated listu
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
                    // provedeme export
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
                // user zvolil Focus
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
            PostMessage(HWindow, WM_USER_CONFIGURATION, 0, 0); // normalni konfigurace
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
            // zmena setrideni v pravem panelu
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
            // zmena setrideni v aktualnim
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

        // prepnuti Smart Mode (Ctrl+N)
        case CM_ACTIVE_SMARTMODE:
            ToggleSmartColumnMode(activePanel);
            return 0;
        case CM_LEFT_SMARTMODE:
            ToggleSmartColumnMode(LeftPanel);
            return 0;
        case CM_RIGHT_SMARTMODE:
            ToggleSmartColumnMode(RightPanel);
            return 0;

            // zmena prohlizeneho disku v levem panelu
        case CM_LCHANGEDRIVE:
        {
            if (activePanel != LeftPanel)
            {
                ChangePanel();
                if (GetActivePanel() != LeftPanel)
                    return 0;          // panel nejde aktivovat
                UpdateWindow(HWindow); // aby doslo u vykresleni focusu jeste pred zobrazenim menu
            }
            if (LeftPanel->DirectoryLine != NULL)
                LeftPanel->DirectoryLine->SetDrivePressed(TRUE);
            LeftPanel->ChangeDrive();
            if (LeftPanel->DirectoryLine != NULL)
                LeftPanel->DirectoryLine->SetDrivePressed(FALSE);
            return 0;
        }
            // zmena prohlizeneho disku v pravem panelu
        case CM_RCHANGEDRIVE:
        {
            if (activePanel != RightPanel)
            {
                ChangePanel();
                if (GetActivePanel() != RightPanel)
                    return 0;          // panel nejde aktivovat
                UpdateWindow(HWindow); // aby doslo u vykresleni focusu jeste pred zobrazenim menu
            }
            if (RightPanel->DirectoryLine != NULL)
                RightPanel->DirectoryLine->SetDrivePressed(TRUE);
            RightPanel->ChangeDrive();
            if (RightPanel->DirectoryLine != NULL)
                RightPanel->DirectoryLine->SetDrivePressed(FALSE);
            return 0;
        }
            // zmena filteru na soubory
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
            // zapinani/vypinani status liny leveho panelu
        case CM_LEFTSTATUS:
        {
            LeftPanel->ToggleStatusLine();
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
            return 0;
        }
            // zapinani/vypinani status liny praveho panelu
        case CM_RIGHTSTATUS:
        {
            RightPanel->ToggleStatusLine();
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
            return 0;
        }
            // zapinani/vypinani directory liny leveho panelu
        case CM_LEFTDIRLINE:
        {
            LeftPanel->ToggleDirectoryLine();
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
            return 0;
        }
            // zapinani/vypinani directory liny praveho panelu
        case CM_RIGHTDIRLINE:
        {
            RightPanel->ToggleDirectoryLine();
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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

        case CM_LEFTREFRESH: // refresh leveho panelu
        {
            LeftPanel->NextFocusName[0] = 0;
            while (SnooperSuspended)
                EndSuspendMode(); // pojistka pro rozbeh refreshovani
            while (StopRefresh)
                EndStopRefresh(FALSE); // pojistka pro rozbeh refreshovani
            while (StopIconRepaint)
                EndStopIconRepaint(FALSE); // pojistka pro rozbeh refreshovani
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RebuildDriveBarsIfNeeded(FALSE, 0, FALSE, 0); // mozna uzivatel vyvolal refresh, aby obnovil listu s disky?
            return 0;
        }

        case CM_RIGHTREFRESH: // refresh praveho panelu
        {
            RightPanel->NextFocusName[0] = 0;
            while (SnooperSuspended)
                EndSuspendMode(); // pojistka pro rozbeh refreshovani
            while (StopRefresh)
                EndStopRefresh(FALSE); // pojistka pro rozbeh refreshovani
            while (StopIconRepaint)
                EndStopIconRepaint(FALSE); // pojistka pro rozbeh refreshovani
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RebuildDriveBarsIfNeeded(FALSE, 0, FALSE, 0); // mozna uzivatel vyvolal refresh, aby obnovil listu s disky?
            return 0;
        }

        case CM_ACTIVEREFRESH: // refresh praveho panelu
        {
            activePanel->NextFocusName[0] = 0;
            while (SnooperSuspended)
                EndSuspendMode(); // pojistka pro rozbeh refreshovani
            while (StopRefresh)
                EndStopRefresh(FALSE); // pojistka pro rozbeh refreshovani
            while (StopIconRepaint)
                EndStopIconRepaint(FALSE); // pojistka pro rozbeh refreshovani
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            SendMessage(activePanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RebuildDriveBarsIfNeeded(FALSE, 0, FALSE, 0); // mozna uzivatel vyvolal refresh, aby obnovil listu s disky?
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

        case CM_REFRESHASSOC: // znovunacteni asociaci z Registry
        {
            OnAssociationsChangedNotification(TRUE);
            return 0;
        }

        case CM_EMAILFILES: // emailovani souboru a adresaru
        {
            if (!EnablerFilesOnDisk)
                return 0;
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection

            // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
            char temporarySelected[MAX_PATH];
            activePanel->SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            activePanel->EmailFiles();

            // pokud jsme nejakou polozku vybrali, zase ji odvyberem
            activePanel->UnselectItemWithName(temporarySelected);

            return 0;
        }

        case CM_COPYFILES: // kopirovani souboru a adresaru
            if (!EnablerFilesCopy)
                return 0;
        case CM_MOVEFILES: // presun/prejmenovani souboru a adresaru
            if (LOWORD(wParam) == CM_MOVEFILES && !EnablerFilesMove)
                return 0;
        case CM_DELETEFILES: // vymaz souboru a adresaru
            if (LOWORD(wParam) == CM_DELETEFILES && !EnablerFilesDelete)
                return 0;
        case CM_OCCUPIEDSPACE: // vypocet zabraneho mista na disku
            if (LOWORD(wParam) == CM_OCCUPIEDSPACE && !EnablerOccupiedSpace)
                return 0;
        case CM_CHANGECASE: // zmena velikosti pismen v nazvech
        {
            if (LOWORD(wParam) == CM_CHANGECASE && !EnablerFilesOnDisk)
                return 0;
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection

            // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
            char temporarySelected[MAX_PATH];
            activePanel->SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            if (activePanel->Is(ptDisk)) // zdroj je disk - jdou sem vsechny operace
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

                // provedeme akci
                activePanel->FilesAction(type, GetNonActivePanel());
            }
            else
            {
                if (activePanel->Is(ptZIPArchive)) // zdroj je archiv - jdou sem vsechny operace
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
                    if (activePanel->Is(ptPluginFS)) // zdroj je FS - jdou sem vsechny operace
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

            // pokud jsme nejakou polozku vybrali, zase ji odvyberem
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
        { // testy na typ panelu jsou az v ShellAction
            activePanel->UserWorkedOnThisPath = TRUE;
            activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
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
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ChangeAttr();
            }
            return 0;
        }

        case CM_CONVERTFILES:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection

                // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
                char temporarySelected[MAX_PATH];
                activePanel->SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

                activePanel->Convert();

                // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                activePanel->UnselectItemWithName(temporarySelected);
            }
            return 0;
        }

        case CM_COMPRESS:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ChangeAttr(TRUE, TRUE);
            }
            return 0;
        }

        case CM_UNCOMPRESS:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ChangeAttr(TRUE, FALSE);
            }
            return 0;
        }

        case CM_ENCRYPT:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ChangeAttr(FALSE, FALSE, TRUE, TRUE);
            }
            return 0;
        }

        case CM_DECRYPT:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ChangeAttr(FALSE, FALSE, TRUE, FALSE);
            }
            return 0;
        }

        case CM_PACK:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->Pack(GetNonActivePanel());
            }
            return 0;
        }

        case CM_UNPACK:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->Unpack(GetNonActivePanel());
            }
            return 0;
        }

        case CM_AFOCUSSHORTCUT:
        {
            if (EnablerFileOrDirLinkOnDisk) // enabler pro activePanel
            {
                //            activePanel->UserWorkedOnThisPath = TRUE; // jedna se o navigaci, nebudeme cestu spinit
                activePanel->FocusShortcutTarget(activePanel);
            }
            return 0;
        }

        case CM_PROPERTIES:
        {
            if (EnablerShowProperties)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
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
            if (activePanel->Is(ptDisk)) // ma Find vztah k aktualni ceste? (u archivu a FS zatim ne)
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
            // zatim umime pouze ptDisk<->ptDisk, ptDisk<->ptZIPArchive a ptZIPArchive<->ptZIPArchive
            //if (LeftPanel->Is(ptPluginFS) || RightPanel->Is(ptPluginFS))
            //{
            //  SalMessageBox(HWindow, LoadStr(IDS_COMPARE_FS), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONINFORMATION);
            //  return 0;
            //}

            // pokud oba panely vedou na stejnou cestu, vypadneme
            char leftPath[2 * MAX_PATH];
            char rightPath[2 * MAX_PATH];
            LeftPanel->GetGeneralPath(leftPath, 2 * MAX_PATH);
            RightPanel->GetGeneralPath(rightPath, 2 * MAX_PATH);
            if (strcmp(leftPath, rightPath) == 0) // case sensitive, kdyz tato podminka selze, nevadi
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
                        Configuration.CompareSubdirs = FALSE; // resi situaci, kdy je zaply CompareSubdirs a pusti se porovnavani pro FS a user zapne CompareOnePanelDirs - bez tohoto radku pri dalsim otevreni dialogu pro disky dostane prednost CompareSubdirs pred CompareOnePanelDirs, coz neni uplne koser...
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

            BeginStopRefresh(); // cmuchal si da pohov

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

            EndStopRefresh(); // ted uz zase cmuchal nastartuje

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
                BeginStopRefresh(); // zadne refreshe nepotrebujeme

                MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

                UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
                CMenuPopup menu;
                FillUserMenu(&menu);
                POINT p;
                activePanel->GetContextMenuPos(&p);
                // dalsi kolo zamykani (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) bude
                // v WM_USER_ENTERMENULOOP+WM_USER_LEAVEMENULOOP, ale to uz je vnorene, zadna rezie,
                // takze ignorujeme, nebudeme proti tomu nijak bojovat
                menu.Track(0, p.x, p.y, HWindow, NULL);
                UserMenuIconBkgndReader.EndUserMenuIconsInUse();

                EndStopRefresh();
            }
            return 0;
        }

        case CM_OPENHOTPATHS:
        {
            BeginStopRefresh(); // zadne refreshe nepotrebujeme

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
                if (EditPermanentVisible || EditWindow->IsEnabled()) // v panelu muze byt archiv
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
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
                                      //          LayoutWindows();
            break;
        }

        case CM_TOGGLEHOTPATHSBAR:
        {
            ToggleHotPathsBar();
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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
                IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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
            // nechame otevrit Plugins Manager
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_PLUGINS, 0);
            break;
        }

        case CM_CUSTOMIZEMIDDLE:
        {
            if (MiddleToolBar->HWindow == NULL)
            {
                ToggleMiddleToolBar();
                IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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
                IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
                LayoutWindows();
            }
            // nechame vybalit stranku UserMenu a rozeditovat polozku index
            PostMessage(HWindow, WM_USER_CONFIGURATION, 2, 0);
            break;
        }

        case CM_CUSTOMIZEHP:
        {
            if (HPToolBar->HWindow == NULL)
            {
                ToggleHotPathsBar();
                IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
                LayoutWindows();
            }
            // nechame vybalit stranku HotPaths
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

            AddDoubleQuotesIfNeeded(cmd, MAX_PATH); // CreateProcess chce mit jmeno s mezerama v uvozovkach (jinak zkousi ruzny varianty, viz help)

            SetDefaultDirectories();

            STARTUPINFO si;
            memset(&si, 0, sizeof(STARTUPINFO));
            si.cb = sizeof(STARTUPINFO);
            si.lpTitle = LoadStr(IDS_COMMANDSHELL);
            // existuje nedokumentovany flag 0x400, kdy do si.hStdOutput predame handle monitoru
            // bohuzel funguje treba se SOL.EXE, ale ne s CMD.EXE, takze jedeme starym zpusobem
            // pres konstrukci dummy okenka
            // pod w2k falgu rikaji #define STARTF_HASHMONITOR       0x00000400  // same as HASSHELLDATA
            // na netu jsem nasel STARTF_MONITOR v nejakem clanku o nedokumentovanych funkcich
            si.dwFlags = STARTF_USESHOWWINDOW;
            POINT p;
            if (MultiMonGetDefaultWindowPos(MainWindow->HWindow, &p))
            {
                // pokud je hlavni okno na jinem monitoru, meli bychom tam take otevrit
                // okno vznikajici a nejlepe na default pozici (stejne jako na primaru)
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
            activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
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
            // prohodime panely
            CFilesWindow* swap = LeftPanel;
            LeftPanel = RightPanel;
            RightPanel = swap;
            // prohodime zaznamy toolbar
            char buff[1024];
            lstrcpy(buff, Configuration.LeftToolBar);
            lstrcpy(Configuration.LeftToolBar, Configuration.RightToolBar);
            lstrcpy(Configuration.RightToolBar, buff);
            // nastavime panelum promenne a nechame nacist toolbary
            LeftPanel->DirectoryLine->SetLeftPanel(TRUE);
            RightPanel->DirectoryLine->SetLeftPanel(FALSE);
            // ikonka se musi zmenit v imagelistu
            LeftPanel->UpdateDriveIcon(FALSE);
            RightPanel->UpdateDriveIcon(FALSE);

            // pokud byl aktivni panel ZOOMed, po Ctrl+U by zustal aktivni minimalizovany panel
            if (GetActivePanel() == LeftPanel && IsPanelZoomed(FALSE) ||
                GetActivePanel() == RightPanel && IsPanelZoomed(TRUE))
            {
                // aktivujeme tedy ten viditelny
                ChangePanel(TRUE);
            }

            LockWindowUpdate(HWindow);
            LayoutWindows();
            LockWindowUpdate(NULL);

            // nechame znovu nacist sloupce (sirky sloupcu se neprohazuji)
            LeftPanel->SelectViewTemplate(LeftPanel->GetViewTemplateIndex(), TRUE, FALSE);
            RightPanel->SelectViewTemplate(RightPanel->GetViewTemplateIndex(), TRUE, FALSE);

            // rozesleme tuto novinku i mezi plug-iny
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
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ClipboardCopy();
            }
            return 0;
        }

        case CM_CLIPCUT:
        {
            if (activePanel->Is(ptDisk))
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
                activePanel->ClipboardCut();
            }
            return 0;
        }

        case CM_CLIPPASTE:
        {
            activePanel->UserWorkedOnThisPath = TRUE;
            if (!activePanel->Is(ptDisk) || !activePanel->ClipboardPaste()) // zkusim pastnout soubory na disk
            {
                if (!activePanel->Is(ptZIPArchive) && !activePanel->Is(ptPluginFS) ||
                    !activePanel->ClipboardPasteToArcOrFS(FALSE, NULL)) // zkusim pastnout soubory do archivu nebo FS
                {
                    activePanel->ClipboardPastePath(); // nebo zmenit aktualni cestu
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

            // rozesleme tuto novinku i mezi plug-iny
            Plugins.Event(PLUGINEVENT_CONFIGURATIONCHANGED, 0);
            return 0;
        }

        case CM_SEC_PERMISSIONS:
        {
            if (EnablerPermissions)
            {
                activePanel->UserWorkedOnThisPath = TRUE;
                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
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
                // radeji se ochranime pred spatnou hodnotou v BeforeZoomSplitPosition
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
        if (LastDispachChangeNotifTime < lParam) // nejde o starou zpravu
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
                    if (!ok) // ulozime si cas posledniho refreshe (jeste v kriticke sekci)
                    {
                        HANDLES(EnterCriticalSection(&TimeCounterSection));
                        LastDispachChangeNotifTime = MyTimeCounter++;
                        HANDLES(LeaveCriticalSection(&TimeCounterSection));
                    }
                    HANDLES(LeaveCriticalSection(&DispachChangeNotifCS));

                    if (ok) // rozesleme zpravu o zmene na 'path' s 'includingSubdirs'
                    {
                        // posleme zpravu do vsech loadlych pluginu
                        Plugins.AcceptChangeOnPathNotification(path, includingSubdirs);

                        if (GetNonActivePanel() != NULL) // nejprve neaktivni (kvuli casum zmen v podadresarich na NTFS)
                        {
                            GetNonActivePanel()->AcceptChangeOnPathNotification(path, includingSubdirs);
                        }
                        if (GetActivePanel() != NULL) // pak aktivni panel
                        {
                            GetActivePanel()->AcceptChangeOnPathNotification(path, includingSubdirs);
                        }

                        if (DetachedFSList->Count > 0)
                        {
                            // pro optimalizaci vstupu/vystupu do plug-inu je sekce EnterPlugin+LeavePlugin
                            // vyvezena az sem (neni v zapouzdreni ifacu)
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
                        break; // konec smycky
                }
            }
        }
        return 0;
    }

    case WM_USER_DISPACHCFGCHANGE:
    {
        // rozesleme zpravu o zmenach v konfiguraci mezi plug-iny
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
        return FALSE; // nemame zadna tlacitka
    }

    case WM_USER_TBENUMBUTTON2:
    {
        HWND hToolBar = (HWND)wParam;
        // predame do nasi toolbary
        if (TopToolBar != NULL && hToolBar == TopToolBar->HWindow)
            return TopToolBar->OnEnumButton(lParam);
        if (MiddleToolBar != NULL && hToolBar == MiddleToolBar->HWindow)
            return MiddleToolBar->OnEnumButton(lParam);
        if (LeftPanel->DirectoryLine->ToolBar != NULL && hToolBar == LeftPanel->DirectoryLine->ToolBar->HWindow)
            return LeftPanel->DirectoryLine->ToolBar->OnEnumButton(lParam);
        if (RightPanel->DirectoryLine->ToolBar != NULL && hToolBar == RightPanel->DirectoryLine->ToolBar->HWindow)
            return RightPanel->DirectoryLine->ToolBar->OnEnumButton(lParam);
        return FALSE; // nemame zadna tlacitka
    }

    case WM_USER_TBRESET:
    {
        HWND hToolBar = (HWND)wParam;
        // predame do nasi toolbary
        if (TopToolBar != NULL && hToolBar == TopToolBar->HWindow)
            TopToolBar->OnReset();
        if (MiddleToolBar != NULL && hToolBar == MiddleToolBar->HWindow)
            MiddleToolBar->OnReset();
        if (LeftPanel->DirectoryLine->ToolBar != NULL && hToolBar == LeftPanel->DirectoryLine->ToolBar->HWindow)
            LeftPanel->DirectoryLine->ToolBar->OnReset();
        if (RightPanel->DirectoryLine->ToolBar != NULL && hToolBar == RightPanel->DirectoryLine->ToolBar->HWindow)
            RightPanel->DirectoryLine->ToolBar->OnReset();
        return FALSE; // nemame zadna tlacitka
    }

    case WM_USER_TBGETTOOLTIP:
    {
        HWND hToolBar = (HWND)wParam;
        // predame do nasi toolbary
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
        return FALSE; // nemame zadna tlacitka
    }

    case WM_USER_TBENDADJUST:
    {
        // nektera z toolbar byla konfigurovana - forcneme update
        IdleForceRefresh = TRUE;
        IdleRefreshStates = TRUE;
        return 0;
    }

    case WM_USER_LEAVEMENULOOP2:
    {
        // tato message chodi az po commandu, takze pripadny command z menu new uz je zpracovan
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
                popup->SetImageList(NULL); // pro jistotu, at popup nevlastni invalidni handle
                ImageList_Destroy(hIcons);
            }
            hIcons = popup->GetHotImageList();
            if (hIcons != NULL)
            {
                popup->SetHotImageList(NULL); // pro jistotu, at popup nevlastni invalidni handle
                ImageList_Destroy(hIcons);
            }
            if (popupID == CML_PLUGINS) // zavirame Plugins menu, dynamicky ikony uz muzeme zahodit (buildi se znovu pred kazdym dalsim otevrenim menu)
                Plugins.ReleasePluginDynMenuIcons();
            break;
        }

        case CML_FILES_NEW:
        {
            popup->SetTemplateMenu(NULL);
            EndStopRefresh(); // zavirame v WM_USER_UNINITMENUPOPUP/WM_USER_INITMENUPOPUP
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
            // vyhledam separator nad a pod pohledama
            int firstIndex = popup->FindItemPosition(firstID);
            int lastIndex = popup->FindItemPosition(lastID);
            if (firstIndex == -1 || lastIndex == -1)
            {
                TRACE_E("Requested items were not found");
            }
            else
            {
                // sestrelim stavajici obsah
                if (firstIndex + 1 < lastIndex - 1)
                    popup->RemoveItemsRange(firstIndex + 1, lastIndex - 1);

                // naleju seznam pohledu
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
                // sestrelim stavajici obsah
                popup->RemoveItemsRange(GO_ITEMS_COUNT, count - 1);
            }

            // pripojime hotpaths, existuji-li
            DWORD firstID = popupID == CML_LEFT_GO ? CM_LEFTHOTPATH_MIN : CM_RIGHTHOTPATH_MIN;
            HotPaths.FillHotPathsMenu(popup, firstID, FALSE, FALSE, FALSE, TRUE);

            // pripojime dir history, maximalne 10 polozek
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
            // Pokud jde o paste typu "zmena adresare", zobrazime to do polozky Paste
            char text[220];
            char tail[50];
            tail[0] = 0;

            strcpy(text, LoadStr(IDS_MENU_EDIT_PASTE));

            CFilesWindow* activePanel = GetActivePanel();
            BOOL activePanelIsDisk = (activePanel != NULL && activePanel->Is(ptDisk));
            if (EnablerPastePath &&
                (!activePanelIsDisk || !EnablerPasteFiles) && // PasteFiles je prioritni
                !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS je prioritni
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
            BeginStopRefresh(); // uzavreme v WM_USER_UNINITMENUPOPUP/CML_FILES_NEW, ktera
                                // ma garantovane parovani s timto vstupem

            // pokud menu neexistuje, nechame ho vytvorit
            if ((!ContextMenuNew->MenuIsAssigned()) && activePanel->Is(ptDisk) &&
                activePanel->CheckPath(FALSE) == ERROR_SUCCESS)
                GetNewOrBackgroundMenu(HWindow, activePanel->GetPath(), ContextMenuNew, CM_NEWMENU_MIN, CM_NEWMENU_MAX, FALSE);

            // pokud menu existuje, nechame na jeho zaklade postavit nase menu
            if (ContextMenuNew->MenuIsAssigned())
                popup->SetTemplateMenu(ContextMenuNew->GetMenu());
            else
            {
                // jinak vlozim retezec, ze menu new neni k dispozici
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

            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // destrukce imagelistu se provede v WM_USER_UNINITMENUPOPUP
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
            FillUserMenu(popup); // toto vybaleni user menu je osetreno z WM_USER_ENTERMENULOOP/WM_USER_LEAVEMENULOOP (vola se UserMenuIconBkgndReader.BeginUserMenuIconsInUse / EndUserMenuIconsInUse)
            break;
        }

        case CML_PLUGINS:
        {
            // inicializace menu Plugins
            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // destrukce imagelistu se provede v WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);

            Plugins.InitMenuItems(HWindow, popup);
            popup->AssignHotKeys();
            break;
        }

        case CML_PLUGINS_SUBMENU:
        {
            // inicializace submenu nektereho z pluginu
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

            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // destrukce imagelistu se provede v WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);
            // chceme pouze pluginy s moznosti konfigurace
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

            HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // destrukce imagelistu se provede v WM_USER_UNINITMENUPOPUP
            HIMAGELIST hIconsGray = Plugins.CreateIconsList(TRUE);
            popup->SetImageList(hIconsGray);
            popup->SetHotImageList(hIcons);
            // chceme vsechny pluginy
            if (Plugins.AddNamesToMenu(popup, CM_PLUGINABOUT_MIN, CM_PLUGINABOUT_MAX - CM_PLUGINABOUT_MIN, FALSE))
                popup->AssignHotKeys();
            break;
        }
        }
        return 0;
    }

    case WM_INITMENUPOPUP: // pozor, obdobny kod je jeste v CFilesBox
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
    {
        LRESULT plResult = 0;
        if (ContextMenuChngDrv != NULL)
        {
            // pokud uzivatel klikne pravym tlacitkem na HotPath v ChangeDrive menu, prijde to sem
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
            if (uMsg == WM_LBUTTONDOWN) // klik -> pocatek tazeni
            {
                UpdateWindow(HWindow);        // pokud je Salamander dole, nechame prekreslit vsechna okna
                MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
                BeginStopIconRepaint();       // neprejeme si zadne repainty ikon
                if (!DragFullWindows)
                    BeginStopStatusbarRepaint(); // neprejeme si prekreslovani throbberu, pokud tahame xorovany splitbar

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

            // zarazka na stredu
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
            DestroyWindow(ToolTipWindow.HWindow); // jen odpoji toolTip od objektu
            if (uMsg == WM_LBUTTONUP)
            {
                // pouze pri legalnim potrvrzeni prijmeme umisteni
                //          int splitWidth = MainWindow->GetSplitBarWidth();
                //          SplitPosition = (double)DragSplitX / (WindowWidth - splitWidth);
                SplitPosition = DragSplitPosition;
                LayoutWindows();
            }
            DragMode = FALSE;
            ReleaseCapture();
            FocusPanel(GetActivePanel());
            EndStopIconRepaint(TRUE); // uvolnime prekreslovani ikon a nechame je prekreslit
            if (!DragFullWindows)
                EndStopStatusbarRepaint(); // uvolnime prekreslovani throbberu, pokud tahame xorovany splitbar
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
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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
            // po dobu tazeni bandu schovame drive bary
            ShowHideTwoDriveBarsInternal(FALSE);
            return 0;
        }

        if (lphdr->code == RBN_ENDDRAG && DriveBar2->HWindow != NULL)
        {
            // po tazeni bandu zase zobrazime nase dva bandy a soupneme je na konec
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

    case WM_SIZE: // uprava velikosti panelu
    {
        // u Tondy nam prichazi WM_SIZE jeste pred dokoncenim WM_CREATE
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
                // toolbar posuneme dolu, pokud ma nektery z panelu directory line
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

            // HWND_BOTTOM - aby nedochazelo k blikani pri resize okna
            // pokud se dolu dostane bottom toolbar, blika pri resize
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
            // nalepit na pravou stranu a nechtel se pohnout, tohle by mohlo problem resit
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
        // nastavime globalni promennou, ktera udava stav ramecku hlavniho ona
        CaptionIsActive = (BOOL)wParam;

        // nechame premalovat directory line aktivniho okna
        // pokud se jedna o ztratu selectu, vyzadame si update - spechame, abychom
        // oteviranemu oknu s CS_SAVEBITS nesestrelili buffer
        CFilesWindow* panel = GetActivePanel();
        if (panel != NULL && panel->DirectoryLine != NULL)
            panel->DirectoryLine->InvalidateAndUpdate(!CaptionIsActive);

        if (!CaptionIsActive)
        {
            // nechame bottom toolbar nastavit do zakladni polohy
            UpdateBottomToolBar();
        }
        break;
    }

    case WM_ENABLE:
    {
        if (WindowsVistaAndLater)
        {
            // patch pro Windows Vista UAC: pri spusteni souboru z panelu, ktere zpusobilo zobrazeni UAC elevacniho
            // promptu a jeho naslednem zavreni pomoci Cancel dochazelo ke stavu, kdy Salamander ztratil focus z panelu
            // Hlavni okno je zakazane v dobe, kdy prijdou zpravy jako WM_ACTIVATE nebo WM_SETFOCUS a focus obdrzi
            // Microsofti IME podporune popupy.
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
            CacheNextSetFocus = TRUE; // pro hladke prepnuti do Salama (jinak by se agresivne predne nakreslil focus (viz stare Salamy))
        else
            SuppressToolTipOnCurrentMousePos(); // potlaceni nechteneho tooltipu pri prepnuti do okna
        ExitHelpMode();

        // zajistime schovani/zobrazeni Wait okenka (pokud existuje)
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
                data->ShouldUnload = TRUE; // nastaveni flagu pro unload plug-inu
            else
            {
                if (lParam == 1)
                    data->ShouldRebuildMenu = TRUE; // nastaveni flagu pro rebuild menu plug-inu
                else
                    data->Commands.Add(LOWORD(lParam - 2)); // pridani salCmd/menuCmd
            }
            ExecCmdsOrUnloadMarkedPlugins = TRUE; // informujeme Salama, ze ma prohledat data vsech plug-inu
        }
        else
        {
            // muze nastat, pokud se ceka na dokonceni metody Release(force==TRUE) plug-inu
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

                // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                data->GetPluginInterfaceForMenuExt()->ExecuteMenuItem(NULL, HWindow, (int)lParam, 0);

                // opet zvysime prioritu threadu, operace dobehla
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
            // musi byt naloaden, protoze post-menu-ext-cmd se volalo z naloadenyho plug-inu...
            // post-unload se spousti az v "idle", takze tim se unload take "nemohl" udelat...
            TRACE_E("Unexpected situation in WM_USER_POSTMENUEXTCMD.");
        }
        return 0;
    }

    case WM_USER_SALSHEXT_TRYRELDATA:
    {
        //      TRACE_I("WM_USER_SALSHEXT_TRYRELDATA: begin");
        if (SalShExtSharedMemView != NULL) // sdilena pamet je k dispozici (pri chybe cut/copy&paste neumime)
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
        if (SalShExtSharedMemView != NULL) // sdilena pamet je k dispozici (pri chybe cut/copy&paste neumime)
        {
            BOOL tmpPasteDone = FALSE;
            char tgtPath[MAX_PATH];
            tgtPath[0] = 0;
            int operation = 0;
            DWORD dataID = -1;
            WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
            if (SalShExtSharedMemView->PostMsgIndex == (int)wParam) // bereme jen "aktualni" zpravy
            {
                if (SalamanderBusy)
                    SalShExtSharedMemView->SalBusyState = 2 /* Salamander je "busy", paste odlozime na pozdeji */;
                else
                {
                    SalamanderBusy = TRUE;
                    SalShExtPastedData.SetLock(TRUE);
                    LastSalamanderIdleTime = GetTickCount();
                    SalShExtSharedMemView->SalBusyState = 1 /* Salamander neni "busy" a uz ceka na zadani operace paste */;
                    SalShExtSharedMemView->PasteDone = FALSE;

                    int count = 0;
                    while (count++ < 50) // dele nez 5 sekund cekat nebudeme
                    {
                        ReleaseMutex(SalShExtSharedMemMutex);
                        Sleep(100); // dame copyhooku 100ms na reakci
                        WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                        if (SalShExtSharedMemView->PasteDone) // copyhook nam predal cilovy adresar pro Paste a dalsi data
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

            if (tmpPasteDone && operation == SALSHEXT_COPY && SalShExtPastedData.GetDataID() == dataID) // provedeme Paste operaci
            {
                SalamanderBusy = TRUE;
                LastSalamanderIdleTime = GetTickCount();
                //          TRACE_I("WM_USER_SALSHEXT_PASTE: calling SalShExtPastedData.DoPasteOperation");
                ProgressDialogActivateDrop = LastWndFromPasteGetData;
                SalShExtPastedData.DoPasteOperation(operation == SALSHEXT_COPY, tgtPath);
                ProgressDialogActivateDrop = NULL; // pro dalsi pouziti progress dialogu musime globalku vycistit
                LastWndFromPasteGetData = NULL;    // pro dalsi Paste to budeme nulovat zde
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, tgtPath, NULL);
                SalamanderBusy = FALSE;
            }
            SalShExtPastedData.SetLock(FALSE);
            PostMessage(HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // po odemceni pripadne provedeme uvolneni dat
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
        // pokud je hlavni okno minimalizovano (pomaly restore nebo otevreni kontextoveho menu),
        // odlozime kontrolu obsahu panelu (hrozi "retry" pri vyndani diskety, atd.)
        if (IsIconic(HWindow))
        {
            SetTimer(HWindow, IDT_POSTENDSUSPMODE, 500, NULL);
            //      puvodne misto timeru: PostMessage(HWindow, WM_USER_END_SUSPMODE, 0, 0);
            return 0;
        }

        if (--ActivateSuspMode < 0)
        {
            ActivateSuspMode = 0;
            // TRACE_E("WM_USER_END_SUSPMODE: problem 2");  // pri otevreni messageboxu s NULL parentem dojde k opakovanemu poslani WM_ACTIVATEAPP "activate" (Salamander uz je aktivovany)
            return 0; // zprava uz byla stornovana
        }
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

        // nejprve musime dokoncit aktivaci okna
        static BOOL recursion = FALSE;
        if (!recursion)
        {
            recursion = TRUE;
            MSG msg;
            CanCloseButInEndSuspendMode = CanClose;
            BOOL oldCanClose = CanClose;
            CanClose = FALSE; // nenechame se zavrit, jsme uvnitr metody
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
        //#pragma message (__FILE__ " (2120): vyhodit")
        //        SalMessageBox(HWindow, "pruser3", "pruser3", MB_OK);
        //      }

        // okno je aktivovano, provedeme refresh
        // EndSuspendMode();   // vyhozeno, chceme refreshovat i pri neaktivnim hl. okne

        LeftPanel->Activate(FALSE);
        RightPanel->Activate(FALSE);

        // pokud byl OneDrive Personal/Business pripojen/odpojen, provedeme refresh Drive bary,
        // at zmizne/objevi se ikona nebo drop down menu
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
            PostMessage(HWindow, WM_USER_END_SUSPMODE, 0, 0); // neni-li ActivateSuspMode >= 1, nedojde
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
        { // pokud user menu stale jeste stoji o tyto ikony:
            // pokud muzeme updatnout ikonky hned, zamkneme pristup k user menu z Findu a provedeme update ikon, jinak se
            // update musi odlozit az po zavreni menu s ikonama (nemuzeme jim je podriznout pod nohama) nebo po zavreni
            // cfg (po OK by se nove nactene ikony premazaly a nezacalo by nove cteni ikon = zustaly by nenactene ikony)
            for (int i = 0; i < UserMenuItems->Count; i++)
                UserMenuItems->At(i)->GetIconHandle(bkgndReaderData, TRUE);
            UserMenuIconBkgndReader.LeaveCSAfterUMIconsUpdate();
            if (UMToolBar != NULL && UMToolBar->HWindow != NULL) // refreshneme user menu toolbar
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

        // odvedeme praci za ztracene a nedorucene zpravy
        int actSusMode = (wParam == TRUE) ? 1 : 0; // ActivateSuspMode by melo byt pri aktivaci 1, jinak 0
        if (ActivateSuspMode < 0)
        {
            ActivateSuspMode = 0;
            TRACE_E("WM_USER_END_SUSPMODE: problem 6");
        }
        else
        {
            if (ActivateSuspMode != actSusMode) // napr. dve deaktivace za sebou nebo nestihnuty activate
            {
                KillTimer(HWindow, IDT_POSTENDSUSPMODE); // pokud se jeste nestihl activate, zrusime ho (prip. se nahodi znovu)

                MSG msg; // vypumpujeme WM_USER_END_SUSPMODE z message queue (jinak dojde v zapeti k ukonceni suspend-modu: situace: staci otevreni File Comparator okna - je tam aktivace+deaktivace po 10ms)
                while (PeekMessage(&msg, HWindow, WM_USER_END_SUSPMODE, WM_USER_END_SUSPMODE, PM_REMOVE))
                    ;

                while (ActivateSuspMode > actSusMode)
                {
                    // EndSuspendMode();  // vyhozeno, chceme refreshovat i pri neaktivnim hl. okne
                    ActivateSuspMode--;
                }
            }
        }

        //      if (IsWindowVisible(HWindow))    // nyni vyreseno pres FirstActivateApp
        //      {
        if (wParam == TRUE) // aktivace app
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
                SetTimer(HWindow, IDT_POSTENDSUSPMODE, 200, NULL); // doufam, ze nezjistime, proc tu byl tenhle timer, no, kdyby jo, tak: zakomentovan byl protoze brzdi (o 200ms) refresh adresare po diskove operaci (napr. Move souboru do podadresare, je to dobre videt i na lokalnim disku)
            }
            else
            {
                // do 2.53b1 zde byla pouze tato vetev a verze s timerem byla v komentari
                // pod W7 nam vsak uzivatele zacali hlasit problem s aktivaci Salamandera v pripade, ze je zapnute groupovani ikon
                // a Salamander je minimalizovany; nekdy potom kliknuti na jeho nahled (nebo pri Alt+Tab prepnuti) Salama nerestorne,
                // pouze se ozve pipnuti; vice viz https://forum.altap.cz/viewtopic.php?f=6&t=3791
                //
                // takze odlozenou variantu o 200ms zase povolujeme, ale pouze od W7 a pouze pokud je okno minimalizovane
                PostMessage(HWindow, WM_USER_END_SUSPMODE, 0, 0); // neni-li ActivateSuspMode >= 1, nedojde
            }
            IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
            IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard
        }
        else // deaktivace app
        {
            // pri deaktivaci hlavniho okna zrusime rezim quick search a quick rename
            CancelPanelsUI();

            //        BeginSuspendMode();    // vyhozeno, chceme refreshovat i pri neaktivnim hl. okne
            ActivateSuspMode++;
            //        }
            //      }
            //      if (wParam == FALSE)  // pri deaktivaci uteceme z adresaru zobrazenych v panelech,
            //      {                     // aby sly mazat, odpojovat atd. z jinych softu
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
            return 0; // nema se delat shut down / log off, neni co resit

        // bezny shutdown / log off sem vubec nema prijit, cely se vyresi pri prijmu
        // WM_QUERYENDSESSION, na jejim konci se zavre hl. okno a tim dojde k zabiti
        // Salamandera, teoreticky se pak vraci TRUE = zavolat WM_ENDSESSION, takze to
        // sem prijit muze, uz je vse hotove, jen vratime 0 dle MSDN, ale zatim vsechny
        // verze Windows uprednostnily zabiti
        //
        // tady resime tzv. "critical shutdown" (i log off), ma flag ENDSESSION_CRITICAL
        // v lParam, vyvolat ho umime volanim (zasadni je EWX_FORCE):
        // ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, SHTDN_REASON_MAJOR_OPERATINGSYSTEM |
        //               SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED);
        // kod je zde (hledat SE_SHUTDOWN_NAME): https://msdn.microsoft.com/en-us/library/windows/desktop/aa376871%28v=vs.85%29.aspx
        //
        // jen Vista+: jeste tady resime shutdown s flagem EWX_FORCEIFHUNG, nema nastaveny
        // flag ENDSESSION_CRITICAL, ale nuti soft k ukonceni, bez ohledu na navratovou hodnotu
        // z WM_QUERYENDSESSION posle WM_ENDSESSION a po jejim dokonceni soft zabije (pokud
        // mezitim uzivatel akci neprerusi Cancelem ze systemoveho okna, ktere se ukaze po 5s):
        // - tenhle rezim jsem nazval "forced shutdown"
        // - system nas proces nezabije, nebezi nam zadny timeout, nebudeme nic nasilne ukoncovat
        // - o pripadne preruseni shutdownu ale musime rict uzivateli - pokud ho prerusi, bude
        //   po dokonceni zpracovani teto WM_ENDSESSION soft normalne pokracovat dale v behu
        //
        // pri "critical shutdown" (i log off):
        // - pod W2K nas proces zabiji bez varovani, neni co resit
        // - pod XP je neprijemne, ze ve WM_QUERYENDSESSION nevime, ze jde o "critical shutdown", takze
        //   pokud tomu nic nezabrani, zacneme normalne ukladat konfiguraci a pokud to do 5s nestihneme,
        //   Windows nas proces zabiji = konfigurace je ztracena, prirozene by to slo delat kopii konfigurace
        //   pri kazdem shutdownu, ale tak vazne problemy pod XP se ztratami konfigurace nejsou;
        //   pod XP nam bez ohledu na navratovku z WM_QUERYENDSESSION (i kdyz se navratovky nedockaji do 5s,
        //   napr. kdyz tam visi dotaz na cancelovani bezicich diskovych operaci) poslou WM_ENDSESSION,
        //   takze zadne ukladani konfigurace ve WM_ENDSESSION nedelame, jen provedeme nejhorsi uklid
        //   (zastaveni probihajicich diskovych operaci)
        // - pod Vista+ nejprve provedeme zalohu klice s konfiguraci v registry, mame na to 5s
        //   ve WM_QUERYENDSESSION, pak z ni vratime TRUE (pokracovat v shutdownu) a system nam
        //   da dalsich 5s na ukonceni zde ve WM_ENDSESSION a to venujeme ukladani konfigurace,
        //   nemusi se to stihnout, pak prijde zabiti a konfigurace zustane rozbita = pri dalsim
        //   startu ji smazneme a nakopirujeme posledni konfiguraci ze zalohy vytvorene ve WM_QUERYENDSESSION;
        //   pod Vista+ pri zavirani bez ukladani konfigurace nejprve pockame 5s ve WM_QUERYENDSESSION
        //   na dokonceni diskovych operaci, a pak dalsich 5s zde ve WM_ENDSESSION

        // Experimentalne zjisteno toto chovani pri shutdownech tri druhu:
        //
        // EWX_FORCE:  (od Vista+ je nahozeny flag ENDSESSION_CRITICAL)
        // W2K: kill bez cehokoliv
        // XP: WM_QUERYENDSESSION, bez odpovedi: za 5s prijde WM_ENDSESSION, za 5s pak kill
        //     WM_QUERYENDSESSION vraci TRUE/FALSE: prijde WM_ENDSESSION, za 5s pak kill
        // Win7-10: WM_QUERYENDSESSION, bez odpovedi: za 5s kill
        // +Vista   WM_QUERYENDSESSION vraci TRUE/FALSE: prijde WM_ENDSESSION, za 5s pak kill
        //
        // EWX_FORCEIFHUNG:  (neni nahozeny flag ENDSESSION_CRITICAL)
        // W2K: jako Log Off ze start menu
        // XP: jako W2K: Log Off ze start menu
        // Win7-10: WM_QUERYENDSESSION, bez odpovedi: za 5s black screen Kill/Cancel (Cancel prerusi shutdown),
        // +Vista   POZOR: bez disablovaneho hl. okna (kdyz neni parent msgboxu ani wait-okna) po 5s kill,
        //          WM_QUERYENDSESSION vrati TRUE/FALSE: prijde WM_ENDSESSION,
        //                                               po 5s black screen Kill/Cancel (Cancel prerusi shutdown)
        //
        // Log Off ze start menu:
        // W2K: WM_QUERYENDSESSION, po 5s msgbox Kill/Cancel (Cancel prerusi shutdown),
        //      WM_QUERYENDSESSION vrati TRUE -> prijde WM_ENDSESSION, po 5s msgbox Kill/Cancel (Cancel funguje jako KILL)
        //      WM_QUERYENDSESSION vrati FALSE - prerusi shutdown
        // XP: stejne s W2K
        // Win7-10: WM_QUERYENDSESSION, bez odpovedi: za 5s black screen Kill/Cancel (Cancel prerusi shutdown),
        // +Vista   WM_QUERYENDSESSION vrati TRUE: prijde WM_ENDSESSION,
        //                                         po 5s black screen Kill/Cancel (Cancel prerusi shutdown)
        //          WM_QUERYENDSESSION vrati FALSE: ihned ukaze black screen Kill/Cancel (Cancel prerusi shutdown)

        // popis viz vyse u "forced shutdown", dame uzivateli prilezitost shutdown zastavit rucne,
        // pokud si to nepreje, muze aspon zastavit probihajici diskove operace, pak uz prijde jen
        // o ulozeni konfigurace
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
                    ProgressDlgArray.PostCancelToAllDlgs(); // dialogy i workeri bezi ve svych threadech, je sance, ze se ukonci
                    while (ProgressDlgArray.RemoveFinishedDlgs() > 0)
                        Sleep(200); // pockame az se diskove operace zcanceluji
                }

                MyShutdownBlockReasonDestroy(HWindow);
            }
            else
                SalMessageBox(HWindow, LoadStr(IDS_FORCEDSHUTDOWN), SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONINFORMATION);
            // bohuzel jsem nenasel, jak prijit na to, jestli shutdown stale probiha nebo jestli
            // ho user zrusil (cerne full screen okno pod Win7), pokud ne, killnou soft, upozorneni
            // na to user dostal, neni co dal resit;
            // muzou probihat diskove operace, pokud se nezcanceluji, soubory zustanou v mezistavu,
            // napr. pri kopirovani bude alokovana plna velikost souboru, ale obsah neni nakopirovany,
            // pouze vyplneny nulami + kazdopadne nebude ulozena konfigurace
            return 0;
        }

        if (!SaveCfgInEndSession) // nema se provest ulozeni konfigurace (to se resi az dale)
        {                         // WaitInEndSession nebo XP "critical shutdown": pockame na dokonceni diskovych operaci (bezi-li nejake)
            if (!WindowsVistaAndLater && ProgressDlgArray.RemoveFinishedDlgs() > 0)
                ProgressDlgArray.PostCancelToAllDlgs(); // dialogy i workeri bezi ve svych threadech, je sance, ze se ukonci

            while (ProgressDlgArray.RemoveFinishedDlgs() > 0)
                Sleep(200);
            return 0; // nechame zavrit soft
        }

        if ((lParam & ENDSESSION_CRITICAL) == 0) // teoreticky nemuze nastat (SaveCfgInEndSession je vzdy TRUE)
        {
            TRACE_E("WM_ENDSESSION: unexpected SaveCfgInEndSession: it is not ENDSESSION_CRITICAL!");
            return 0;
        }

        // break; // tady break nechybi !!! dal pokracuje jen "critical shutdown" (i log off), jde se ukladat konfigurace
    }
    case WM_QUERYENDSESSION:
    case WM_USER_CLOSE_MAINWND:
    case WM_USER_FORCECLOSE_MAINWND:
    {
        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::1");

        DWORD msgArrivalTime = GetTickCount(); // critical shutdown trva 5s + 5s, pri prekroceni nas zabiji, takze merime cas

        if (uMsg == WM_QUERYENDSESSION)
        {
            TRACE_I("WM_QUERYENDSESSION: message received");
            SaveCfgInEndSession = FALSE;
            WaitInEndSession = FALSE;

            if ((lParam & ENDSESSION_CRITICAL) != 0)
            {
                // opatreni proti tomu, aby WM_ENDSESSION prisel z kodu resiciho IdleCheckClipboard
                // v OnEnterIdle() a CannotCloseSalMainWnd bylo TRUE (WM_ENDSESSION se pak odmita vykonat)
                // konec softu se rychle blizi, IDLE vazne nema smysl resit, jen zdrzuje
                DisableIdleProcessing = TRUE;
            }
        }

        // Windows XP: pri critical shutdown nenastavuji ENDSESSION_CRITICAL, W2K: pri critical
        // shutdown neposilaji ani WM_QUERYENDSESSION, vice viz vyse u WM_ENDSESSION

        // Vista+: endAfterCleanup: TRUE = critical shutdown (zabiti do 5s) nelze odmitnout, soft
        // se 100% ukonci, udelame aspon nejhorsi uklid: cancel bezicich diskovych operaci
        // (zastaveni hledani + zavreni Find oken a vieweru nema smysl resit, jen se cte)
        BOOL endAfterCleanup = FALSE;

        if (!CanClose)
        {
            if (CanCloseButInEndSuspendMode &&
                (uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION))
            { // CanClose je FALSE jen kvuli aktivaci okna, shutdownu to nijak nebrani
            }
            else // "neprobehl kompletni start" nebo "close okna je odlozeny, provede se az po aktivaci okna", ted koncime
            {
                if (uMsg == WM_QUERYENDSESSION)
                    TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: CanClose is FALSE");
                if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                    endAfterCleanup = TRUE; // nelze odmitnout = udelame aspon uklid
                else
                    return 0; // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION
            }
        }

        if (!endAfterCleanup && CannotCloseSalMainWnd)
        {
            TRACE_E("WM_USER_CLOSE_MAINWND: CannotCloseSalMainWnd == TRUE!");
            if (uMsg == WM_QUERYENDSESSION)
                TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: CannotCloseSalMainWnd is TRUE");
            if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                endAfterCleanup = TRUE; // nelze odmitnout = udelame aspon uklid
            else
                return 0; // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION
        }

        if (!endAfterCleanup && uMsg != WM_ENDSESSION)
        { // u WM_ENDSESSION uz je to busy z WM_QUERYENDSESSION, test musime preskocit
            if (!SalamanderBusy)
            {
                SalamanderBusy = TRUE; // uz je BUSY, pokracujeme ve zpracovani WM_USER_CLOSE_MAINWND
                LastSalamanderIdleTime = GetTickCount();
            }
            else
            {
                if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                    endAfterCleanup = TRUE; // nelze odmitnout = udelame aspon uklid
                else
                {
                    if (LockedUIReason != NULL && HasLockedUI())
                        SalMessageBox(HWindow, LockedUIReason, SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONINFORMATION);
                    else
                        TRACE_E("WM_USER_CLOSE_MAINWND: SalamanderBusy == TRUE!");
                    if (uMsg == WM_QUERYENDSESSION)
                        TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: SalamanderBusy is TRUE");
                    return 0; // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION
                }
            }
        }

        if (!endAfterCleanup && AlreadyInPlugin > 0)
        {
            TRACE_E("WM_USER_CLOSE_MAINWND: AlreadyInPlugin > 0!");
            if (uMsg == WM_QUERYENDSESSION)
                TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: AlreadyInPlugin > 0");
            if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0)
                endAfterCleanup = TRUE; // nelze odmitnout = udelame aspon uklid
            else                        // nelze unloadnout plugin, kdyz jsme v nem!
                return 0;               // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION
        }

        // pokud je zapnuta konfirmace OnClose, nechame usera potvrdit zavreni programu
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

        // mame nastartovane nejake dialogy s diskovymi operacemi
        WCHAR blockReason[MAX_STR_BLOCKREASON];
        if (ProgressDlgArray.RemoveFinishedDlgs() > 0)
        {
            if ((uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION) && (lParam & ENDSESSION_CRITICAL) != 0)
            {                                               // "critical shutdown" (i log off) = neni cas na diskuze, vse zcancelovat, aby nezustal
                                                            // "nedokonceny" bordel na disku
                if (uMsg == WM_QUERYENDSESSION)             // cancelujeme jen pri prvni zprave o critical shutdownu
                    ProgressDlgArray.PostCancelToAllDlgs(); // dialogy i workeri bezi ve svych threadech, je sance, ze se ukonci
            }
            else // ohlasime to v okne a pockame az se vse ukonci, sem WM_ENDSESSION nemuze prijit
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
                    // user jeste nechce koncit
                    return 0; // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION
                }
                UpdateWindow(HWindow);
            }
        }

        // critical shutdown nelze odmitnout, nahradni reseni: nebudeme ukladat konfiguraci, provedeme jen
        // nejhorsi uklid, a pak soft ukoncime (mozna nas sestreli drive, aktualni rezim: zabiti do 5s),
        // pro jednoduchost nepokracujeme na zavirani oken Findu a vieweru, prvni je zbytecne,
        // druhe by neskodilo (zmizely by soubory z TEMPu)
        if (endAfterCleanup)
        { // vzdy plati: uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0
            // cekame na dokonceni diskovych operaci max. 5s od prijmu WM_QUERYENDSESSION
            while (ProgressDlgArray.RemoveFinishedDlgs() > 0 &&
                   GetTickCount() - msgArrivalTime <= QUERYENDSESSION_TIMEOUT - 200)
                Sleep(200);
            WaitInEndSession = TRUE;
            return TRUE; // pokracujeme do WM_ENDSESSION, kde to skoncime nebo nas zabijou pri dalsim cekani
        }

        int i = 0;
        TDirectArray<HWND> destroyArray(10, 5); // pole oken urcenych k destrukci
        if (uMsg != WM_ENDSESSION)
        {
            BeginStopRefresh(); // uz si neprejeme zadne refreshe panelu

            // vybereme vsechna okna findu
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

            // zeptame se, jestli okna Findu pujdou zavrit, rozjeta hledani zastavime (na dotaz)
            BOOL endProcessing = FALSE;
            for (i = 0; i < destroyArray.Count; i++)
            {
                if (IsWindow(destroyArray[i])) // pokud jeste okno existuje
                {
                    BOOL canclose = TRUE; // pro pripad, ze by nasl. SendMessage nevysla

                    WindowsManager.CS.Enter(); // nechceme zadne zmeny nad WindowsManager
                    CFindDialog* findDlg = (CFindDialog*)WindowsManager.GetWindowPtr(destroyArray[i]);
                    if (findDlg != NULL) // pokud okno jeste existuje, posleme mu dotaz na zavreni (jinak uz je to zbytecne)
                    {
                        BOOL myPost = findDlg->StateOfFindCloseQuery == sofcqNotUsed;
                        if (myPost) // pokud nejde o zanoreni (asi je mozne, nezkoumal jsem, ale nepravdepodobne)
                        {
                            findDlg->StateOfFindCloseQuery = sofcqSentToFind;
                            PostMessage(destroyArray[i], WM_USER_QUERYCLOSEFIND, 0,
                                        uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0); // pri critical shutdown se neptame, rovnou cancelujeme
                        }
                        BOOL cont = TRUE;
                        while (cont)
                        {
                            cont = FALSE;
                            WindowsManager.CS.Leave();
                            // jdeme se tvarit jako "responding" soft, takze pumpovat zpravy
                            MSG msg;
                            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                            {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                            // dame threadu Findu nejaky cas na reakci
                            Sleep(50);
                            // cas na test, jestli uz neni nas dotaz zodpovezeny
                            WindowsManager.CS.Enter(); // nechceme zadne zmeny nad WindowsManager
                            findDlg = (CFindDialog*)WindowsManager.GetWindowPtr(destroyArray[i]);
                            if (findDlg != NULL) // resime jen pokud okno jeste existuje (jinak uz je to zbytecne)
                            {
                                if (findDlg->StateOfFindCloseQuery == sofcqCanClose ||
                                    findDlg->StateOfFindCloseQuery == sofcqCannotClose)
                                { // je rozhodnuto, koncime
                                    if (findDlg->StateOfFindCloseQuery == sofcqCannotClose)
                                        canclose = FALSE;
                                    if (myPost)
                                        findDlg->StateOfFindCloseQuery = sofcqNotUsed;
                                }
                                else
                                    cont = TRUE; // cekame dale na odpoved z Find threadu
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
                            // EndStopRefresh(); // pri critical shutdown neukoncime stop-refreshe (rozesilaji se refreshe do panelu)
                            endAfterCleanup = TRUE; // nelze odmitnout = udelame aspon uklid
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
                return 0; // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION

            // necham okna Findu zavrit v jejich threadu
            // nedelame pri critical shutdown: zavirani oken Findu je zbytecne (sice se neulozi
            // sirky sloupcu, velikost okna a par dalsich blbin, ale to ignorujeme)
            // pozn.: test !endAfterCleanup je zde zbytecny, protoze mimo criticky shutdown
            // je endAfterCleanup vzdy FALSE
            if (uMsg != WM_QUERYENDSESSION || (lParam & ENDSESSION_CRITICAL) == 0) // mimo criticky shutdown
            {
                for (i = 0; i < destroyArray.Count; i++)
                {
                    if (IsWindow(destroyArray[i])) // pokud jeste okno existuje
                        SendMessage(destroyArray[i], WM_USER_CLOSEFIND, 0, 0);
                }
            }

            if (showCloseFindWin && !endAfterCleanup && uMsg == WM_QUERYENDSESSION)
                MyShutdownBlockReasonDestroy(HWindow);

            if (!endAfterCleanup)
            {
                // zruseni oken viewru (nejsou child-okna -> neprijde jim automaticky WM_DESTROY)
                // delame i pri critical shutdown, aby se podmazly soubory v TEMPu,
                // mohlo by to byt skodlive (napr. viewer s desifrovanym souborem po zavreni
                // spusti shredovani docasneho souboru a behem shredovani nas system zabije,
                // lepsi varianta je to shredovani nechat udelat poradne po restartu systemu,
                // to se ale resi v metode DeleteTmpCopy(), pri critical shutdown se neshreduje)
                ViewerWindowQueue.BroadcastMessage(WM_CLOSE, 0, 0);

                // dodame cekani pred volanim unloadu pluginu - pokud existuji okna Findu a interniho vieweru,
                // tady maji cas na sve zavreni (muzou drzet soubory Encryptu)
                int winsCount = ViewerWindowQueue.GetWindowCount() + FindDialogQueue.GetWindowCount();
                int timeOut = 3;
                while (winsCount > 0 && timeOut--)
                {
                    Sleep(100);
                    int c = ViewerWindowQueue.GetWindowCount() + FindDialogQueue.GetWindowCount();
                    if (winsCount > c) // zatim jeste ubyvaji okna, budeme cekat dale aspon 300 ms
                    {
                        winsCount = c;
                        timeOut = 3;
                    }
                }
            }
        }

        // critical shutdown nelze odmitnout, nahradni reseni: nebudeme ukladat konfiguraci, provedeme jen
        // nejhorsi uklid, a pak soft ukoncime (mozna nas sestreli drive, aktualni rezim: zabiti do 5s),
        if (endAfterCleanup)
        {
            // vzdy plati: uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0
            // cekame na dokonceni diskovych operaci max. 5s od prijmu WM_QUERYENDSESSION
            while (ProgressDlgArray.RemoveFinishedDlgs() > 0 &&
                   GetTickCount() - msgArrivalTime <= QUERYENDSESSION_TIMEOUT - 200)
                Sleep(200);

            WaitInEndSession = TRUE;
            return TRUE; // pokracujeme do WM_ENDSESSION, kde to skoncime nebo nas zabijou pri dalsim cekani
        }

        if (uMsg == WM_QUERYENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0) // jde o Vista+
        {
            BOOL cfgOK = FALSE;
            if (SALAMANDER_ROOT_REG != NULL)
            {
                // zajistime si exkluzivni pristup ke konfiguraci v registry
                LoadSaveToRegistryMutex.Enter();

                HKEY salamander;
                if (OpenKeyAux(NULL, HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
                {
                    DWORD saveInProgress;
                    if (!GetValueAux(NULL, salamander, SALAMANDER_SAVE_IN_PROGRESS, REG_DWORD, &saveInProgress, sizeof(DWORD)))
                    { // nejde o poskozenou konfiguraci
                        cfgOK = TRUE;
                    }
                    CloseKeyAux(salamander);
                }
                if (!cfgOK)
                    LoadSaveToRegistryMutex.Leave(); // s konfiguraci dale uz nepracujeme, opustime sekci
                                                     // POZNAMKA: LoadSaveToRegistryMutex.Leave() zavolame az ve WM_ENDSESSION po dokonceni ukladani CFG (je nize)
            }

            BOOL backupOK = FALSE;
            if (cfgOK) // stara konfigurace vypada OK, zalohujeme ji pro pripad selhani ukladani konfigurace
            {
                char backup[200];
                sprintf_s(backup, "%s.backup.63A7CD13", SALAMANDER_ROOT_REG); // "63A7CD13" je prevence shody jmena klice s uzivatelskym
                SHDeleteKey(HKEY_CURRENT_USER, backup);                       // smazneme stary backup, pokud nejaky existuje
                HKEY salBackup;
                if (!OpenKeyAux(NULL, HKEY_CURRENT_USER, backup, salBackup)) // test neexistence backupu
                {
                    if (CreateKeyAux(NULL, HKEY_CURRENT_USER, backup, salBackup)) // vytvoreni klice pro backup
                    {
                        // zkousel jsem i RegCopyTree (bez KEY_ALL_ACCESS neslapalo) a rychlost byla stejna jako SHCopyKey
                        if (SHCopyKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salBackup, 0) == ERROR_SUCCESS)
                        { // vytvoreni backupu
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
                    LoadSaveToRegistryMutex.Leave(); // s konfiguraci dale uz nepracujeme, opustime sekci
            }

            // cekame na dokonceni diskovych operaci max. 5s od prijmu WM_QUERYENDSESSION
            while (ProgressDlgArray.RemoveFinishedDlgs() > 0 &&
                   GetTickCount() - msgArrivalTime <= QUERYENDSESSION_TIMEOUT - 200)
                Sleep(200);

            if (backupOK)                   // je zazalohovano, konfiguraci ulozime ve WM_ENDSESSION,
                SaveCfgInEndSession = TRUE; // pokud nas pri tom zabiji, nacte se konfigurace ze zalohy
            else
            {
                // EndStopRefresh();  // pri critical shutdown neukoncime stop-refreshe (rozesilaji se refreshe do panelu)
                WaitInEndSession = TRUE; // nepodarilo se zazalohovat, ukladani konfigurace nebudeme riskovat
            }
            return TRUE; // chceme 5s ve WM_ENDSESSION, tak vratime TRUE
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
        if (shutdown) // pri shutdown / log-off / restart ukazeme wait-okenko pro vsechny Save (i pluginu) + budeme zpracovavat message-loopu (aby nas neprohlasili za "not responding" a nezabili predcasne)
        {
            // nahodime thread, ktery bude provadet praci s registry behem ukladani konfigurace,
            // tento (hlavni) thread bude mezitim pumpovat zpravy v message loope
            RegistryWorkerThread.StartThread();

            hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            analysing.SetProgressMax(7 /* pocet z CMainWindow::SaveConfig() -- NUTNE SYNCHRONIZOVAT !!! */ +
                                     Plugins.GetPluginSaveCount()); // o jednu min, at si uzijou pohled na 100%
            analysing.Create();
            GlobalSaveWaitWindow = &analysing;
            GlobalSaveWaitWindowProgress = 0;
            EnableWindow(HWindow, FALSE);

            // bude se volat i SaveConfiguration plug-inu -> nutne nastaveni parenta pro jejich messageboxy
            PluginMsgBoxParent = analysing.HWindow;
        }

        // vyhlasime "critical shutdown", vsechny rutiny by se podle toho meli chovat a ukoncit vse co mozna nejrychleji
        CriticalShutdown = uMsg == WM_ENDSESSION && (lParam & ENDSESSION_CRITICAL) != 0;

        // unloadneme vsechny plug-iny (cesty v panelech se prip. daji na fixed-drive)
        SetDoNotLoadAnyPlugins(TRUE); // prozatim kvuli thumbnailum
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

                // stopneme thread, ktery provadel praci s registry behem ukladani konfigurace...
                RegistryWorkerThread.StopThread();
            }
            if (uMsg == WM_QUERYENDSESSION || uMsg == WM_ENDSESSION)
                MyShutdownBlockReasonDestroy(HWindow);

            if (uMsg != WM_ENDSESSION) // pri critical shutdown neukoncime stop-refreshe (rozesilaji se refreshe do panelu)
            {
                EndStopRefresh();
                return 0; // zavreni/shutdown/logoff odmitneme, pripadny "forced shutdown" detekujeme az ve WM_ENDSESSION
            }
            else
            {
                // cekame na dokonceni diskovych operaci, mozna drive system zabije nas proces
                while (ProgressDlgArray.RemoveFinishedDlgs() > 0)
                    Sleep(200);
                CriticalShutdown = FALSE; // jen tak pro sychr
                return 0;                 // ukonceni softu
            }
        }

        // pokud existuji okna CShellExecuteWnd, nabidneme preruseni zavirani nebo zaslani bug reportu + terminate
        char reason[BUG_REPORT_REASON_MAX]; // pricina problemu + seznam oken (multiline)
        strcpy(reason, "Some faulty shell extension has locked our main window.");
        if (EnumCShellExecuteWnd(shutdown ? analysing.HWindow : HWindow,
                                 reason + (int)strlen(reason), BUG_REPORT_REASON_MAX - ((int)strlen(reason) + 1)) > 0)
        {
            // zeptame se, zda ma Salamander pokracovat nebo jestli ma vygenerovat bug report
            if (CriticalShutdown || // pri critical shutdown se nema smysl na nic ptat, nechame se v klidu ukoncit
                SalMessageBox(shutdown ? analysing.HWindow : HWindow,
                              LoadStr(IDS_SHELLEXTBREAK3), SALAMANDER_TEXT_VERSION,
                              MSGBOXEX_CONTINUEABORT | MB_ICONINFORMATION) != IDABORT)
            {
                if (uMsg == WM_QUERYENDSESSION)
                    TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: some faulty shell extension has locked our main window");
                goto EXIT_WM_USER_CLOSE_MAINWND; // mame pokracovat
            }

            // breakneme se
            strcpy(BugReportReasonBreak, reason);
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // zamrazime tento thread
            while (1)
                Sleep(1000);
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::3");

        // optame se panelu, jestli muzeme koncit
        if (LeftPanel != NULL && RightPanel != NULL)
        {
            BOOL canClose = FALSE;
            BOOL detachFS1, detachFS2;
            if (LeftPanel->PrepareCloseCurrentPath(shutdown ? analysing.HWindow : LeftPanel->HWindow, TRUE, FALSE, detachFS1, FSTRYCLOSE_UNLOADCLOSEFS /* zbytecne - pluginy (i FS) uz jsou unloadle */))
            {
                if (RightPanel->PrepareCloseCurrentPath(shutdown ? analysing.HWindow : RightPanel->HWindow, TRUE, FALSE, detachFS2, FSTRYCLOSE_UNLOADCLOSEFS /* zbytecne - pluginy (i FS) uz jsou unloadle */))
                {
                    canClose = TRUE; // jen oba najednou, jinak nezavirame ani jeden
                    if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
                        RightPanel->SleepIconCacheThread();
                    RightPanel->CloseCurrentPath(shutdown ? analysing.HWindow : RightPanel->HWindow, FALSE, detachFS2, FALSE, FALSE, TRUE); // zavreme pravy panel

                    // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                    RightPanel->ListBox->SetItemsCount(0, 0, 0, TRUE);
                    RightPanel->SelectedCount = 0;
                    // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekreslnei obsahu panelu a nastaveni
                    // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu.
                    // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                    PostMessage(RightPanel->HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                if (canClose)
                {
                    if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
                        LeftPanel->SleepIconCacheThread();
                    LeftPanel->CloseCurrentPath(shutdown ? analysing.HWindow : LeftPanel->HWindow, FALSE, detachFS1, FALSE, FALSE, TRUE); // zavreme levy panel

                    // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                    LeftPanel->ListBox->SetItemsCount(0, 0, 0, TRUE);
                    LeftPanel->SelectedCount = 0;
                    // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekreslnei obsahu panelu a nastaveni
                    // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu.
                    // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                    PostMessage(LeftPanel->HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                else
                    LeftPanel->CloseCurrentPath(shutdown ? analysing.HWindow : LeftPanel->HWindow, TRUE, detachFS1, FALSE, FALSE, TRUE); // cancel na close leveho panelu
            }
            if (!canClose)
            {
                SetDoNotLoadAnyPlugins(FALSE);
                if (uMsg == WM_QUERYENDSESSION)
                    TRACE_I("WM_QUERYENDSESSION: cancelling shutdown: unable to close paths in panels");
                goto EXIT_WM_USER_CLOSE_MAINWND; // panely nejdou uzavrit
            }
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::4");

        // !!! POZOR: od tohoto bodu (az po DestroyWindow) jiz nesmi dojit k preruseni,
        // pri vybaleni okna user zjisti, ze jsou oba panely prazdne (listing je uvolneny)
        // (toto uz je porusene pri Shutdown / Log Off / Restart, protoze musime distribuovat
        // zpravy, jinak nas povazuji za "not responding" a zabije nas system predcasne)

        if (StrICmp(Configuration.SLGName, Configuration.LoadedSLGName) != 0) // pokud user zmenil jazyk Salama
        {
            Plugins.ClearLastSLGNames(); // aby pripadne doslo k nove volbe nahradniho jazyka u vsech pluginu
            Configuration.UseAsAltSLGInOtherPlugins = FALSE;
            Configuration.AltPluginSLGName[0] = 0;
        }

        if (Configuration.AutoSave)
            SaveConfig();

        if (uMsg == WM_ENDSESSION)
            LoadSaveToRegistryMutex.Leave(); // paruje k Enter() volanemu pri prijmu WM_QUERYENDSESSION

        if (shutdown)
        {
            GlobalSaveWaitWindow = NULL;
            GlobalSaveWaitWindowProgress = 0;
            EnableWindow(HWindow, TRUE);
            PluginMsgBoxParent = oldPluginMsgBoxParent;
            DestroyWindow(analysing.HWindow);
            SetCursor(hOldCursor);

            // stopneme thread, ktery provadel praci s registry behem ukladani konfigurace...
            RegistryWorkerThread.StopThread();
        }

        CALL_STACK_MESSAGE1("WM_USER_CLOSE_MAINWND::5");

        DiskCache.PrepareForShutdown(); // jeste vycistime z disku prazdne tmp-adresare

        //      if (TipOfTheDayDialog != NULL)
        //        DestroyWindow(TipOfTheDayDialog->HWindow);  // dialog uz ma sva data ulozena (transfer tam probiha runtime)

        MainWindowCS.SetClosed();

        CanDestroyMainWindow = TRUE; // nyni lze beztrestne zavolat DestroyWindow na MainWindow

        DestroyWindow(HWindow);

        // WM_QUERYENDSESSION a WM_ENDSESSION: vsechny Windows killnou process jakmile se pri shutdownu
        // zrusi hl. okno, takze nize uvedene je pri shutdownu dead code

        CriticalShutdown = FALSE; // jen tak pro sychr

        if (uMsg == WM_QUERYENDSESSION)
        {
            TRACE_I("WM_QUERYENDSESSION: allowing shutdown...");
            // hl. okno uz je zavrene = neni komu dorucit WM_ENDSESSION, WaitInEndSession
            // ani SaveCfgInEndSession neni potreba nastavovat
            return TRUE; // pokud to doslo az sem, povolime shut-down
        }
        return 0; // navratovka z WM_USER_CLOSE_MAINWND, WM_USER_FORCECLOSE_MAINWND a WM_ENDSESSION
    }

    case WM_ERASEBKGND:
    {
        /*
      HDC dc = (HDC)wParam;
      HPEN oldPen = (HPEN)SelectObject(dc, BtnFacePen);
      MoveToEx(dc, 0, 0, NULL);
      LineTo(dc, 0, WindowHeight - 1);
      LineTo(dc, WindowWidth - 1, WindowHeight - 1);
      LineTo(dc, WindowWidth - 1, 0);
      SelectObject(dc, oldPen);
*/
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
            // nektera silena shell extension prave zavolala DestroyWindow na hlavni okno Salamandera

            MSG msg; // vypumpujeme message queue (WMP9 bufferoval Enter a odmacknul nam OK)
            // while (PeekMessage(&msg, HWindow, 0, 0, PM_REMOVE));  // Petr: nahradil jsem jen zahozenim zprav z klavesky (bez TranslateMessage a DispatchMessage hrozi nekonecny cyklus, zjisteno pri unloadu Automationu s memory leaky, pred zobrazenim msgboxu s hlaskou o leakach doslo k nekonecnemu cyklu, do fronty porad pridavali WM_PAINT a my ho z ni zase zahazovali)
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;

            // pozadame uzivatele, aby nam poslal break-report
            SalMessageBox(HWindow, LoadStr(IDS_SHELLEXTBREAK), SALAMANDER_TEXT_VERSION,
                          MB_OK | MB_ICONSTOP);

            // a breakneme se
            strcpy(BugReportReasonBreak, "Some faulty shell extension destroyed our main window.");
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // zamrazime tento thread
            // MainWindow uz stejne neexistuje, spadli bychom pri nejblizsi mozne prilezitosti
            while (1)
                Sleep(1000);
        }

        // dame seznamu procesu vedet, ze koncime
        TaskList.SetProcessState(PROCESS_STATE_ENDING, NULL);

        UserMenuIconBkgndReader.EndProcessing();

        SHChangeNotifyRelease(); // nadale neprijimame Shell Notifications
        KillTimer(HWindow, IDT_ADDNEWMODULES);
        HANDLES(RevokeDragDrop(HWindow));
        if (Configuration.StatusArea)
            RemoveTrayIcon();
        //--- zruseni child-oken
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
            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenu() dole...
MENU_TEMPLATE_ITEM TaskBarIconMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_CONTEXTMENU_EXIT
  {MNTT_PE, 0
};
*/
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
    // osetrim zpravy posilane z file manager extenzi
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
            return 0; // chodime pouze nad diskem

        int index = (int)wParam;
        FMS_GETFILESELW* fs = (FMS_GETFILESELW*)lParam;
        CFilesWindow* activePanel = GetActivePanel();

        int count = activePanel->GetSelCount();
        if (count != 0)
        {
            // vytahnu index n-te (index) selected polozky
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
            return 0; // chodime pouze nad diskem

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
            //---  refresh neautomaticky refreshovanych adresaru
            // zmena v adresari zobrazenem v panelu a radsi i v podadresarich (buh vi co system provadi)
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
