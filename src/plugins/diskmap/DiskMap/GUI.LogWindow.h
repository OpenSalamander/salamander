// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CFrameWindow.h"
#include "Utils.CZString.h"

#include "System.CLogger.h"

const TCHAR szLogWindowClass[] = TEXT("Zar.DM.LogWin.WC");

#define LV_MARGIN 4

class CLogWindow : public CFrameWindow
{
protected:
    HWND _hWndListView;

    CLogger* _logger;

    CZResourceString* _logLevels[COUNT_LOGLEVELS];

    int _x;
    int _y;
    int _width;
    int _height;

    HWND DoCreate(int left, int top, int width, int height, BOOL isTopmost)
    {
        return MyCreateWindow(
            isTopmost ? WS_EX_TOPMOST : 0, //WS_EX_TOPMOST,// | WS_EX_NOACTIVATE,
            szLogWindowClass,
            CZResourceString::GetString(IDS_DISKMAP_LOG_TITLE),
            WS_POPUPWINDOW | WS_CAPTION,
            left, top,     //X, Y
            width, height, // WIDTH, HEIGHT
            NULL);
    }

    void DoPaint(PAINTSTRUCT* pps) {}

    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct)
    {
        this->_x = lpCreateStruct->x;
        this->_y = lpCreateStruct->y;
        this->_width = lpCreateStruct->cx;
        this->_height = lpCreateStruct->cy;

        RECT rct;
        GetClientRect(this->_hWnd, &rct);

        this->_hWndListView = CreateWindowEx(
            WS_EX_CLIENTEDGE | WS_EX_NOPARENTNOTIFY,
            WC_LISTVIEW, TEXT(""),
            WS_CHILDWINDOW | WS_GROUP | WS_TABSTOP | WS_VISIBLE |
                LVS_SHOWSELALWAYS | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER | LVS_OWNERDATA,

            LV_MARGIN, LV_MARGIN,
            /* width  */ rct.right - rct.left - 2 * LV_MARGIN,
            /* height */ rct.bottom - rct.top - 2 * LV_MARGIN,

            this->_hWnd, NULL, CWindow::s_hInstance, NULL);
        if (this->_hWndListView)
        {
            DWORD exStyle = LVS_EX_FULLROWSELECT;
            //if (DllGetVersion())
            //if (Shell >= 5.80) exStyle |= LVS_EX_LABELTIP;
            ListView_SetExtendedListViewStyle(this->_hWndListView, exStyle);

            HICON iinfo = LoadIcon(0, IDI_INFORMATION);
            HICON iwarn = LoadIcon(0, IDI_WARNING);
            HICON ierror = LoadIcon(0, IDI_ERROR);

            int cx = GetSystemMetrics(SM_CXICON);
            int cy = GetSystemMetrics(SM_CYICON);

            int scx = GetSystemMetrics(SM_CXSMICON);
            int scy = GetSystemMetrics(SM_CYSMICON);

            HIMAGELIST imglistl = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, 1, 1);
            HIMAGELIST imglists = ImageList_Create(scx, scy, ILC_COLOR32 | ILC_MASK, 1, 1);
            ImageList_AddIcon(imglistl, iinfo);
            ImageList_AddIcon(imglistl, iwarn);
            ImageList_AddIcon(imglistl, ierror);
            ImageList_AddIcon(imglists, iinfo);
            ImageList_AddIcon(imglists, iwarn);
            ImageList_AddIcon(imglists, ierror);

            ListView_SetImageList(this->_hWndListView, imglistl, LVSIL_NORMAL);
            ListView_SetImageList(this->_hWndListView, imglists, LVSIL_SMALL);

            LVCOLUMN lvc;
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt = LVCFMT_LEFT;

            lvc.cx = 90;
            lvc.pszText = const_cast<TCHAR*>(CZResourceString::GetString(IDS_DISKMAP_LOG_HTYPE));
            lvc.iSubItem = 0;
            ListView_InsertColumn(this->_hWndListView, 0, &lvc);

            lvc.cx = 125;
            lvc.pszText = const_cast<TCHAR*>(CZResourceString::GetString(IDS_DISKMAP_LOG_HTEXT));
            lvc.iSubItem = 1;
            ListView_InsertColumn(this->_hWndListView, 1, &lvc);

            lvc.cx = 250;
            lvc.pszText = const_cast<TCHAR*>(CZResourceString::GetString(IDS_DISKMAP_LOG_HFILE));
            lvc.iSubItem = 2;
            ListView_InsertColumn(this->_hWndListView, 2, &lvc);

            ListView_SetItemCountEx(this->_hWndListView, 0, LVSICF_NOINVALIDATEALL);
        }
        if (this->_logger)
        {
            this->_logger->AttachWindow(this->_hWnd, WM_APP_LOGUPDATED);
        }
        return true;
    }
    void OnDestroy()
    {
    }

public:
    CLogWindow(HWND hwndParent, CLogger* logger, BOOL isTopmost) : CFrameWindow(hwndParent)
    {
        this->_hWndListView = NULL;

        this->_logger = logger;

        this->_logLevels[LOG_INFORMATION] = new CZResourceString(IDS_DISKMAP_LOG_LINFO);
        this->_logLevels[LOG_WARNING] = new CZResourceString(IDS_DISKMAP_LOG_LWARN);
        this->_logLevels[LOG_ERROR] = new CZResourceString(IDS_DISKMAP_LOG_LERROR);

        this->Create(isTopmost);
    }
    ~CLogWindow()
    {
        delete this->_logLevels[LOG_INFORMATION];
        delete this->_logLevels[LOG_WARNING];
        delete this->_logLevels[LOG_ERROR];
        this->Destroy();
    }

    HWND Create(BOOL isTopmost)
    {
        int width = 500;
        int height = 360;

        RECT rct;
        GetWindowRect(this->_hWndParent, &rct);
        int x = (rct.left + rct.right - width) / 2;
        int y = (rct.top + rct.bottom - height) / 2;

        return this->CFrameWindow::Create(x, y, width, height, isTopmost);
    }

    void ShowCenter()
    {
        RECT rct;
        GetWindowRect(this->_hWndParent, &rct);
        this->_x = (rct.left + rct.right - this->_width) / 2;
        this->_y = (rct.top + rct.bottom - this->_height) / 2;

        MoveWindow(this->_hWnd, this->_x, this->_y, this->_width, this->_height, TRUE);
        this->Show(SW_SHOW);
        SetFocus(this->_hWnd);
    }

    void OnClose()
    {
        this->Hide();
    }
    void OnLogUpdated(WPARAM wParam, LPARAM lParam)
    {
        this->_logger->ClearAnnounceFlag();
        int cnt = this->_logger->GetLogCount();
        ListView_SetItemCountEx(this->_hWndListView, cnt, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    }

    LRESULT OnNotify(int idFrom, NMHDR* pnmhdr)
    {
        NMLVDISPINFO* plvdi;
        NMLVKEYDOWN* plvkd;
        int il;
        switch (pnmhdr->code)
        {
        case LVN_KEYDOWN:
            plvkd = (NMLVKEYDOWN*)pnmhdr;
            if (plvkd->wVKey == VK_ESCAPE)
            {
                this->Hide();
            }
            return 0;
        case LVN_GETDISPINFO:
            plvdi = (NMLVDISPINFO*)pnmhdr;
            CLogItemBase* lgi = this->_logger->GetLogItem(plvdi->item.iItem);
            switch (plvdi->item.iSubItem)
            {
            case 0:
                il = lgi->GetLevel();
                plvdi->item.iImage = il;
                plvdi->item.iIndent = 0;
                plvdi->item.pszText = const_cast<TCHAR*>(this->_logLevels[il]->GetString());
                break;

            case 1:
                plvdi->item.pszText = const_cast<TCHAR*>(lgi->GetText());
                break;

            case 2:
                plvdi->item.pszText = const_cast<TCHAR*>(lgi->GetPath());
                break;

            default:
                break;
            }
            if (plvdi->item.pszText == NULL)
            {
                static TCHAR emptyBuff[] = TEXT("");
                plvdi->item.pszText = emptyBuff;
            }
            return 0;
        }
        return DefWindowProc(this->_hWnd, WM_NOTIFY, (WPARAM)(int)(idFrom), (LPARAM)(NMHDR*)(pnmhdr));
    }
    BOOL OnKey(UINT vkey, BOOL fDown, int cRepeat, UINT flags)
    {
        switch (vkey)
        {
        case VK_ESCAPE:
            this->Hide();
            return TRUE;
        }
        return FALSE;
    }
    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
            //case WM_SYSCOLORCHANGE:	return this->OnSysColorChange(), 0;
            //case WM_SETTINGCHANGE:	return this->OnSystemChange(), 0;

        case WM_KEYDOWN:
            return this->OnKey((UINT)(wParam), TRUE, (int)(short)LOWORD(lParam), (UINT)HIWORD(lParam))
                       ? 0
                       : DefWindowProc(hWnd, message, wParam, lParam);

        case WM_NOTIFY:
            return this->OnNotify((int)(wParam), (NMHDR*)(lParam));

        case WM_APP_LOGUPDATED:
            return this->OnLogUpdated(wParam, lParam), 0;

        case WM_PAINT:
            return this->OnPaint(), 0;

        case WM_CREATE:
            return this->OnCreate((LPCREATESTRUCT)lParam) ? 0 : (LRESULT)-1L;

        case WM_CLOSE:
            return this->OnClose(), 0;
        case WM_DESTROY:
            return this->OnDestroy(), 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    static BOOL RegisterClass()
    {
        static ATOM a = NULL;
        if (!a)
        {
            WNDCLASSEX wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);

            wcex.style = 0;
            wcex.lpfnWndProc = CLogWindow::s_WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = CWindow::s_hInstance;
            wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
            wcex.lpszMenuName = NULL;
            wcex.lpszClassName = szLogWindowClass;
            wcex.hIcon = (HICON)LoadImage(CWindow::s_hInstance, MAKEINTRESOURCE(IDI_ZAREVAKDISKMAP), IMAGE_ICON, 32, 32, LR_CREATEDIBSECTION);
            wcex.hIconSm = (HICON)LoadImage(CWindow::s_hInstance, MAKEINTRESOURCE(IDI_ZAREVAKDISKMAP), IMAGE_ICON, 16, 16, LR_CREATEDIBSECTION);

            a = ::RegisterClassEx(&wcex);
        }
        BOOL ret = (a != NULL);
        return ret;
    }
    static BOOL UnregisterClass()
    {
        BOOL ret = ::UnregisterClass(szLogWindowClass, CWindow::s_hInstance);
        if (!ret)
            TRACE_E("UnregisterClass(szLogWindowClass) has failed");
        return ret;
    }
};
