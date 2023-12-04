// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define APPCOMMAND_BROWSER_BACKWARD 1
#define APPCOMMAND_BROWSER_FORWARD 2
#define APPCOMMAND_BROWSER_REFRESH 3
#define APPCOMMAND_BROWSER_STOP 4
#define APPCOMMAND_BROWSER_HOME 7
#define WM_APPCOMMAND 0x0319

#define FAPPCOMMAND_MOUSE 0x8000
#define FAPPCOMMAND_KEY 0
#define FAPPCOMMAND_OEM 0x1000
#define FAPPCOMMAND_MASK 0xF000

#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#define GET_DEVICE_LPARAM(lParam) ((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))
#define GET_MOUSEORKEY_LPARAM GET_DEVICE_LPARAM
#define GET_FLAGS_LPARAM(lParam) (LOWORD(lParam))
#define GET_KEYSTATE_LPARAM(lParam) GET_FLAGS_LPARAM(lParam)

#include "Utils.CZLocalizer.h"

#include "GUI.CFrameWindow.h"
#include "GUI.ToolTip.h"
#include "GUI.ShellMenu.h"

#include "GUI.AboutDialog.h"

#include "GUI.LogWindow.h"
#include "GUI.DirectoryLine.h"
#include "GUI.DiskMapView.h"
#include "GUI.ViewConnectorBase.h"

const TCHAR szMainWindowWindowClass[] = TEXT("Zar.DM.MainWin.WC");

class CDiskMapViewConnector : public CViewConnectorBase
{
protected:
    CDiskMapView* _diskMapView;
    CDirectoryLine* _directoryLine;
#ifdef SALAMANDER
    CSalamanderCallbackAbstract* _salamander;
#endif
public:
    CDiskMapViewConnector(CDiskMapView* diskMapView, CDirectoryLine* directoryLine)
    {
        this->_diskMapView = diskMapView;
        this->_directoryLine = directoryLine;
#ifdef SALAMANDER
        this->_salamander = NULL;
#endif

        this->_diskMapView->SetConnector(this);
        this->_directoryLine->SetConnector(this);
    }
    BOOL DM_ZoomOut(int level)
    {
        return this->_diskMapView->ZoomOut(level);
    }
    BOOL DL_SetSubPath(CZString const* path)
    {
        return this->_directoryLine->SetSubPath(path);
    }
    BOOL DL_SetDiskSize(INT64 size)
    {
        return this->_directoryLine->SetDiskSize(size);
    }
    CZDirectory* DM_GetViewDirectory(int parentlevel)
    {
        return this->_diskMapView->GetViewDirectory(parentlevel);
    }
#ifdef SALAMANDER
    void SetSalamander(CSalamanderCallbackAbstract* salamander)
    {
        this->_salamander = salamander;
    }
    CSalamanderCallbackAbstract* GetSalamander() { return this->_salamander; }
#endif
};

class CMainWindow : public CFrameWindow
{
protected:
    static CZLocalizer* s_localizer;

    CFileInfoTooltip* _tooltip;
    CShellMenu* _shellmenu;

    CDirectoryLine* _dirLine;
    CDiskMapView* _diskMap;

    CLogger* _logger;
    CLogWindow* _logWindow;

    CZString* _path;

    CDiskMapViewConnector* _connector;

#ifdef SALAMANDER
    CSalamanderCallbackAbstract* _callback;
#endif

    HWND DoCreate(int left, int top, int width, int height, BOOL isTopmost)
    {
        HMENU hMenu = LoadMenu(CWindow::s_hResInstance, MAKEINTRESOURCE(IDC_ZAREVAKDISKMAP));
        return MyCreateWindow(
            isTopmost ? WS_EX_TOPMOST : 0,
            szMainWindowWindowClass,
            CZResourceString::GetString(IDS_DISKMAP_TITLE),
            WS_OVERLAPPEDWINDOW,
            left, top,     //X,Y
            width, height, //Width, Height
            hMenu);
    }

    void DoPaint(PAINTSTRUCT* pps)
    {
        HDC hdc = pps->hdc;
        RECT rect;
        GetClientRect(this->_hWnd, &rect);

        DrawEdge(hdc, &rect, BDR_SUNKENOUTER, BF_ADJUST | BF_TOP);
    }

    void OnActivate(UINT state, HWND hwndActDeact, BOOL fMinimized)
    {
        if (this->_dirLine)
        {
            this->_dirLine->SetActiveState(state != WA_INACTIVE);
        }
    }
    BOOL OnCommand(int id, HWND hwndCtl, UINT codeNotify)
    {
        // Parse the menu selections:
#ifdef DEBUG
        if (id >= IDM_DESIGN_FIRST && id <= IDM_DESIGN_LAST)
        {
            this->_diskMap->SetCushionDesign(id - IDM_DESIGN_FIRST);
            return TRUE;
        }
#endif
        if (id >= IDM_PATHFORMAT_FIRST && id <= IDM_PATHFORMAT_LAST)
        {
            this->_tooltip->SetPathFormat((ETooltipPathFormat)(id - IDM_PATHFORMAT_FIRST));
#ifdef SALAMANDER
            if (this->_callback != NULL)
            {
                this->_callback->SetPathFormat(id - IDM_PATHFORMAT_FIRST);
            }
#endif
            return TRUE;
        }
        switch (id)
        {
        case IDM_HELP_ABOUT:
#ifdef SALAMANDER
            if (this->_callback != NULL)
                this->_callback->About(this->_hWnd);
#else
            CAboutDialog::ShowModal(CWindow::s_hInstance, this->_hWnd);
#endif
            return TRUE;

        case IDM_FILE_FOCUSINSALAMANDER:
#ifdef SALAMANDER
            if (this->_callback != NULL)
            {
                TCHAR buff[2 * MAX_PATH + 1];
                if (this->_diskMap->GetSelectedFileName(buff, ARRAYSIZE(buff)) <= MAX_PATH)
                {
                    this->_callback->FocusFile(buff);
                }
            }
#endif
            return TRUE;
        case IDM_VIEW_ZOOMIN:
            this->_diskMap->ZoomIn();
            return TRUE;
        case IDM_VIEW_ZOOMOUT:
            this->_diskMap->ZoomOut();
            return TRUE;
        case IDM_VIEW_GOTOROOT:
            this->_diskMap->ZoomToRoot();
            return TRUE;
        case IDM_FILE_ABORT:
            this->_diskMap->Abort();
            return TRUE;
        case IDM_VIEW_LOG:
            if (this->_logWindow)
                this->_logWindow->ShowCenter();
            return TRUE;
        case IDM_FILE_REFRESH:
            this->_logger->Clear();
            this->_diskMap->UpdateFileList();
            return TRUE;
        case IDM_FILE_OPEN:
            this->_diskMap->OpenSelectedFile();
            return TRUE;
        case IDM_OPTIONS_SHOWFOLDERS:
#ifdef SALAMANDER
            if (this->_callback != NULL)
            {
                BOOL showFolders = !this->_diskMap->GetDirectoryOverlayVisibility();
                this->_callback->SetShowFolders(showFolders);
                this->_diskMap->SetDirectoryOverlayVisibility(showFolders);
            }
#else
            this->_diskMap->SetDirectoryOverlayVisibility(!this->_diskMap->GetDirectoryOverlayVisibility());
#endif
            return TRUE;
        case IDM_OPTIONS_SHOWTOOLTIP:
#ifdef SALAMANDER
            if (this->_callback != NULL)
            {
                BOOL showTooltip = !this->_diskMap->GetTooltipEnabled();
                this->_callback->SetShowTooltip(showTooltip);
                this->_diskMap->SetTooltipEnabled(showTooltip);
            }
#else
            this->_diskMap->SetTooltipEnabled(!this->_diskMap->GetTooltipEnabled());
#endif
            return TRUE;
        case IDM_OPTIONS_CONSFIRMCLOSE:
#ifdef SALAMANDER
            if (this->_callback != NULL)
            {
                this->_callback->SetCloseConfirm(!this->_callback->GetCloseConfirm());
            }
#endif
            return TRUE;
        case IDM_FILE_EXIT:
            this->Destroy();
            return TRUE;
        }
        return FALSE;
    }
    void OnInitMenu(HMENU hMenu)
    {
        if (hMenu == GetMenu(this->_hWnd))
        {
            BOOL isFileSelected = this->_diskMap->IsFileSelected();
            EnableMenuItem(hMenu, IDM_FILE_OPEN, MF_BYCOMMAND | (isFileSelected ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hMenu, IDM_FILE_ABORT, MF_BYCOMMAND | (this->_diskMap->CanAbort() ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hMenu, IDM_FILE_REFRESH, MF_BYCOMMAND | (this->_diskMap->CanPopulate() ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hMenu, IDM_VIEW_ZOOMIN, MF_BYCOMMAND | (this->_diskMap->CanZoomIn() ? MF_ENABLED : MF_GRAYED));
            BOOL canZoomOut = this->_diskMap->CanZoomOut();
            EnableMenuItem(hMenu, IDM_VIEW_ZOOMOUT, MF_BYCOMMAND | (canZoomOut ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hMenu, IDM_VIEW_GOTOROOT, MF_BYCOMMAND | (canZoomOut ? MF_ENABLED : MF_GRAYED));
#ifdef _DEBUG
            CheckMenuRadioItem(hMenu, IDM_DESIGN_FIRST, IDM_DESIGN_LAST, this->_diskMap->GetCushionDesignID() + IDM_DESIGN_FIRST, MF_BYCOMMAND);
#endif
            CheckMenuRadioItem(hMenu, IDM_PATHFORMAT_FIRST, IDM_PATHFORMAT_LAST, this->_tooltip->GetPathFormat() + IDM_PATHFORMAT_FIRST, MF_BYCOMMAND);

            BOOL showFolders = this->_diskMap->GetDirectoryOverlayVisibility();
            CheckMenuItem(hMenu, IDM_OPTIONS_SHOWFOLDERS, MF_BYCOMMAND | (showFolders ? MF_CHECKED : MF_UNCHECKED));
            BOOL showTooltip = this->_diskMap->GetTooltipEnabled();
            CheckMenuItem(hMenu, IDM_OPTIONS_SHOWTOOLTIP, MF_BYCOMMAND | (showTooltip ? MF_CHECKED : MF_UNCHECKED));
#ifdef SALAMANDER
            BOOL confirmClose = TRUE;
            BOOL focusenabled = FALSE;
            if (this->_callback != NULL)
            {
                focusenabled = isFileSelected && this->_callback->CanFocusFile();
                confirmClose = this->_callback->GetCloseConfirm();
            }
            EnableMenuItem(hMenu, IDM_FILE_FOCUSINSALAMANDER, MF_BYCOMMAND | (focusenabled ? MF_ENABLED : MF_GRAYED));
            CheckMenuItem(hMenu, IDM_OPTIONS_CONSFIRMCLOSE, MF_BYCOMMAND | (confirmClose ? MF_CHECKED : MF_UNCHECKED));
#endif
        }
    }

    BOOL OnKey(UINT vkey, BOOL fDown, int cRepeat, UINT flags)
    {
        switch (vkey)
        {
        case VK_ESCAPE:
            if (this->_diskMap->CanAbort())
            {
                this->_diskMap->Abort();
            }
            else
            {
#ifdef SALAMANDER
                if (this->_callback && this->_callback->ConfirmClose(this->_hWnd))
                    this->Destroy();
#endif
            }
            break;

        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
            this->_diskMap->MoveCursor(vkey);
            break;
        }
        return TRUE;
    }
    void OnContextMenu(HWND hwndContext, int xPos, int yPos)
    {
        if (xPos == -1 && yPos == -1)
            this->_diskMap->ShowContextMenu();
    }
    void OnEnterSizeMove()
    {
        this->_diskMap->SetSizingState(TRUE);
    }
    void OnExitSizeMove()
    {
        this->_diskMap->SetSizingState(FALSE);
    }
    void OnSize(WPARAM state, int cx, int cy) //TODO: used by OnSystemChange
    {
        int top = 1;
        if (this->_dirLine)
        {
            this->_dirLine->UpdateSize();
            top += this->_dirLine->GetHeight();
        }
        if (this->_diskMap)
            this->_diskMap->SetMargins(0, top, 0, 0);
    }
    void OnGetMinMaxInfo(LPMINMAXINFO lpMinMaxInfo)
    {
        lpMinMaxInfo->ptMinTrackSize.x = 320;
        lpMinMaxInfo->ptMinTrackSize.y = 240;
    }

    BOOL OnAppCommand(HWND hwnd, short cmd, WORD uDevice, DWORD dwKeys)
    {
        switch (cmd)
        {
        case APPCOMMAND_BROWSER_BACKWARD:
            this->_diskMap->ZoomOut();
            return TRUE;
        case APPCOMMAND_BROWSER_FORWARD:
            this->_diskMap->ZoomIn();
            return TRUE;
        case APPCOMMAND_BROWSER_REFRESH:
            this->_diskMap->UpdateFileList();
            return TRUE;
        case APPCOMMAND_BROWSER_STOP:
            this->_diskMap->Abort();
            return TRUE;
        case APPCOMMAND_BROWSER_HOME:
            this->_diskMap->ZoomToRoot();
            return TRUE;
        }
        return FALSE;
    }
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct)
    {
        this->_tooltip = new CFileInfoTooltip(this->_hWnd);
        this->_shellmenu = new CShellMenu(this->_hWnd);

        int top = 1;
        this->_dirLine = new CDirectoryLine(this->_hWnd, top, this->_logger, this->_tooltip, this->_shellmenu);
        top += this->_dirLine->GetHeight();
        this->_diskMap = new CDiskMapView(this->_hWnd, top, this->_logger, this->_tooltip, this->_shellmenu);
        this->_diskMap->SetCushionDesign(IDR_CUSHIONDATA_GLASS);

        this->_connector = new CDiskMapViewConnector(this->_diskMap, this->_dirLine);

        this->_logWindow = new CLogWindow(this->_hWnd, this->_logger, this->IsTopmost());

#ifdef SALAMANDER
        if (this->_callback)
        {
            //POZOR! Callback tady jeste neexistuje - zkontrolovat s SetCallback metodou, aby bylo stejne
            this->_diskMap->SetDirectoryOverlayVisibility(this->_callback->GetShowFolders());
            this->_diskMap->SetTooltipEnabled(this->_callback->GetShowTooltip());
            this->_tooltip->SetPathFormat((ETooltipPathFormat)(this->_callback->GetPathFormat()));
        }
#else
        TCHAR buff[MAX_PATH];
        int len = GetCurrentDirectory(ARRAYSIZE(buff), buff);
        if (len > 0)
        {
            this->SetPath(buff, TRUE);
        }
#endif

        return true;
    }
    void OnDestroy()
    {
        if (this->_shellmenu)
            delete this->_shellmenu;
        this->_shellmenu = NULL;
        if (this->_tooltip)
            delete this->_tooltip;
        this->_tooltip = NULL;

        if (this->_dirLine)
            delete this->_dirLine;
        this->_dirLine = NULL;
        if (this->_diskMap)
            delete this->_diskMap;
        this->_diskMap = NULL;

        if (this->_logWindow)
            delete this->_logWindow;
        this->_logWindow = NULL;

        if (this->_path)
            delete this->_path;
        this->_path = NULL;

        if (this->_connector)
            delete this->_connector;
        this->_connector = NULL;

        PostQuitMessage(0); //konec UI vlakna...
    }

public:
    CMainWindow(HWND hwndParent) : CFrameWindow(hwndParent)
    {
        this->_tooltip = NULL;
        this->_shellmenu = NULL;

        this->_dirLine = NULL;
        this->_diskMap = NULL;

        this->_path = NULL;
#ifdef SALAMANDER
        this->_callback = NULL;
#endif
        this->_connector = NULL;

        this->_logger = new CLogger();
        this->_logWindow = NULL;
    }
    ~CMainWindow()
    {
        if (this->_logger)
            delete this->_logger;
        this->_logger = NULL;
    }

#ifdef SALAMANDER
    void SetCallback(CSalamanderCallbackAbstract* callback)
    {
        this->_callback = callback;
        this->_connector->SetSalamander(callback);
        if (this->_diskMap)
        {
            this->_diskMap->SetDirectoryOverlayVisibility(this->_callback->GetShowFolders());
            this->_diskMap->SetTooltipEnabled(this->_callback->GetShowTooltip());
            this->_tooltip->SetPathFormat((ETooltipPathFormat)(this->_callback->GetPathFormat()));
        }
    }
    void PopulateStart()
    {
        this->_diskMap->UpdateFileList();
    }
#endif

    static BOOL LoadResourceStrings()
    {
        if (CMainWindow::s_localizer == NULL)
        {
            CMainWindow::s_localizer = new CZLocalizer(CWindow::s_hResInstance);
        }
        return TRUE;
    }
    static BOOL UnloadResourceStrings()
    {
        if (CMainWindow::s_localizer != NULL)
        {
            delete CMainWindow::s_localizer;
            CMainWindow::s_localizer = NULL;
        }
        return TRUE;
    }

    BOOL SetPath(TCHAR const* path, BOOL fStartEnum = TRUE)
    {
        if (this->_path)
            delete this->_path;
        this->_path = NULL;
        this->_path = new CZString(path);

        //CZString *title = new CZString(this->_path);
        //DISKMAP = 7chars
        // - = 3chars
        CZResourceString titleprefix(IDS_DISKMAP_TITLEPREFIX);
        CZStringBuffer* title = new CZStringBuffer(16 + this->_path->GetLength());
        title->Append(titleprefix.GetString(), titleprefix.GetLength());
        title->Append(this->_path);
        SetWindowText(this->_hWnd, title->GetString());
        delete title;

        if (this->_dirLine)
            this->_dirLine->SetPath(this->_path);
        if (this->_diskMap)
            this->_diskMap->SetPath(path, fStartEnum);

        return TRUE;
    }

    void OnSystemChange()
    {
        if (this->_dirLine)
            this->_dirLine->OnSystemChange();
        //if (this->_diskMap) this->_diskMap->OnSystemChange();
        this->OnSize(0, 0, 0); //TODO!
        this->Repaint(FALSE);
    }

    void OnSysColorChange()
    {
        if (this->_dirLine)
            this->_dirLine->OnSysColorChange();
        this->Repaint(FALSE);
    }

    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    static BOOL RegisterClass()
    {
        static ATOM a = NULL;
        if (!a)
        {
            WNDCLASSEX wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);

            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = CWindow::s_WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = CWindow::s_hInstance;
            wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wcex.lpszMenuName = NULL;
            wcex.lpszClassName = szMainWindowWindowClass;
            wcex.hIcon = (HICON)LoadImage(CWindow::s_hInstance, MAKEINTRESOURCE(IDI_ZAREVAKDISKMAP), IMAGE_ICON, 32, 32, LR_CREATEDIBSECTION);
            wcex.hIconSm = (HICON)LoadImage(CWindow::s_hInstance, MAKEINTRESOURCE(IDI_ZAREVAKDISKMAP), IMAGE_ICON, 16, 16, LR_CREATEDIBSECTION);
            a = ::RegisterClassEx(&wcex);
        }
        BOOL ret = (a != NULL);
        ret &= CDiskMapView::RegisterClass();
        ret &= CDirectoryLine::RegisterClass();
        ret &= CLogWindow::RegisterClass();
        ret &= CFileInfoTooltip::RegisterClass();
        return ret;
    }
    static BOOL UnregisterClass()
    {
        BOOL ret = ::UnregisterClass(szMainWindowWindowClass, CWindow::s_hInstance);
        if (!ret)
            TRACE_E("UnregisterClass(szMainWindowWindowClass) has failed");
        ret = CDiskMapView::UnregisterClass() & ret;
        ret = CDirectoryLine::UnregisterClass() & ret;
        ret = CLogWindow::UnregisterClass() & ret;
        ret = CFileInfoTooltip::UnregisterClass() & ret;
        return ret;
    }
};
