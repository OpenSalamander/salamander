// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CFrameWindow.h"
#include "TreeMap.FileData.CZDirectory.h"
#include "Utils.CZString.h"
#include "Utils.CStringFormatter.h"
#include "Utils.CZSmartIconLoader.h"

const TCHAR szToolTipWindowClass[] = TEXT("Zar.DM.ToolTip.WC");

#define CS_DROPSHADOW 0x00020000
#define SPI_GETDROPSHADOW 0x1024

#define MAX_TYPELEN 80 //odpovida SHFILEINFO.szTypeName[80]
#define MAX_DATELEN 64
#define MAX_SIZELEN 64

#define TT_MARGIN_X 6
#define TT_MARGIN_Y 6
#define TT_UNDER_OFFSET_X -2
#define TT_UNDER_OFFSET_Y 24

#define TT_ABOVE_OFFSET_X -2
#define TT_ABOVE_OFFSET_Y 12

#define TT_LID_CREATETIME 0
#define TT_LID_MODIFYTIME 1
#define TT_LID_ACCESSTIME 2

#define TT_LID_DATASIZE 3
#define TT_LID_REALSIZE 4
#define TT_LID_DISKSIZE 5

#define TT_LID_FILECOUNT 6
#define TT_LID_DIRSCOUNT 7

#define TT_LID_MAX (TT_LID_DIRSCOUNT + 1)
/*
const int TT_FileInfo[] = {
							TT_LID_CREATETIME, 
							TT_LID_MODIFYTIME, 
								-TT_MARGIN_X/2, 
							TT_LID_DATASIZE, 
							TT_LID_DISKSIZE 
							};

const int TT_FolderInfo[] = {
							TT_LID_FILECOUNT, 
							TT_LID_DIRSCOUNT, 
								-TT_MARGIN_X/2, 
							TT_LID_DATASIZE, 
							TT_LID_DISKSIZE 
};
*/
//const int *TT_LineInfo[] = { TT_FileInfo, TT_FolderInfo };
#define TT_LINECOUNT 5
const int TT_LineInfo[][TT_LINECOUNT] = {
    {TT_LID_CREATETIME,
     TT_LID_MODIFYTIME,
     -TT_MARGIN_X / 2,
     TT_LID_DATASIZE,
     TT_LID_DISKSIZE},
    {TT_LID_FILECOUNT,
     TT_LID_DIRSCOUNT,
     -TT_MARGIN_X / 2,
     TT_LID_DATASIZE,
     TT_LID_DISKSIZE}};

enum ETooltipType
{
    ETT_FileInfo,
    ETT_FolderInfo
};

enum ETooltipPathFormat
{
    ETPF_Absolute,
    ETPF_RelativeRoot,
    ETPF_RelativeView,
    ETPF_FileName
};

/*const int TT_LineInfo[][2] = { 
	{ 32.19, 47.29, 31.99, 19.11 },
	{ 11.29, 22.49, 33.47, 17.29 },
	{ 41.97, 22.09,  9.76, 22.55 }
};*/

class CFileInfoTooltip : public CFrameWindow
{
protected:
    HICON _hicon;

    CStringFormatter* _sFormatter;

    CZFile* _file;
    CZDirectory* _rootRelDir;
    CZDirectory* _viewRelDir;

    int _x;
    int _y;
    int _width;
    int _height;

    BOOL _isDirty;

    TCHAR _stitle[2 * MAX_PATH + 1];
    size_t _stitlelen;
    TCHAR _sftype[MAX_TYPELEN];
    size_t _sftypelen;

    ETooltipType _type;
    ETooltipPathFormat _pathFormat;

    CZResourceString* _headers[TT_LID_MAX];
    CZStringBuffer* _values[TT_LID_MAX];
    int _vals_x;
    int _headerheight;

    HFONT _hfnormal;
    HFONT _hftitle;

    CZSmartIconLoader* _iconLoader;

    HWND DoCreate(int left, int top, int width, int height, BOOL isTopmost)
    {
        return MyCreateWindow(
            isTopmost ? WS_EX_TOPMOST : 0, //WS_EX_TOPMOST,// | WS_EX_NOACTIVATE,
            szToolTipWindowClass,
            TEXT("ToolTip"),
            WS_BORDER | WS_POPUP,
            left, top,     //X, Y
            width, height, // WIDTH, HEIGHT
            NULL);
    }
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct)
    {
        this->_x = lpCreateStruct->x;
        this->_y = lpCreateStruct->y;
        this->_width = lpCreateStruct->cx;
        this->_height = lpCreateStruct->cy;

        this->OnSystemChange();
        return true;
    }
    void OnDestroy()
    {
        if (this->_hicon)
            DestroyIcon(this->_hicon);
        this->_hicon = NULL;
    }

    void PrepareTitle()
    {
        if (this->_file == NULL)
        {
            this->_stitle[0] = TEXT('\0');
            this->_stitlelen = 0;
            return;
        }

        if (this->_type == ETT_FileInfo)
        {
            switch (this->_pathFormat)
            {
            case ETPF_Absolute:
                this->_stitlelen = this->_file->GetFullName(this->_stitle, ARRAYSIZE(this->_stitle));
                break;
            case ETPF_RelativeRoot:
                this->_stitlelen = this->_file->GetRelativeName(this->_rootRelDir, this->_stitle, ARRAYSIZE(this->_stitle));
                break;
            case ETPF_RelativeView:
                this->_stitlelen = this->_file->GetRelativeName(this->_viewRelDir, this->_stitle, ARRAYSIZE(this->_stitle));
                break;
            case ETPF_FileName:
                //this->_stitlelen = file->GetRelativeName(view, this->_stitle, ARRAYSIZE(this->_stitle));
                _tcscpy(this->_stitle, this->_file->GetName());
                this->_stitlelen = this->_file->GetNameLen();
                break;
            default:
                this->_stitlelen = this->_file->GetFullName(this->_stitle, ARRAYSIZE(this->_stitle));
                break;
            }
        }
        else
        {
            this->_stitlelen = this->_file->GetFullName(this->_stitle, ARRAYSIZE(this->_stitle));
        }
    }

    void MakeDirty()
    {
        this->_isDirty = TRUE;
        //if (this->IsVisible()) this->Repaint(TRUE);
    }

public:
    CFileInfoTooltip(HWND hwndParent) : CFrameWindow(hwndParent)
    {
        this->_hicon = NULL;

        this->_type = ETT_FileInfo;
        this->_pathFormat = ETPF_RelativeView;

        this->_file = NULL;
        this->_rootRelDir = NULL;
        this->_viewRelDir = NULL;

        this->_x = 0;
        this->_y = 0;
        this->_width = 250;
        this->_height = 60;

        this->_isDirty = TRUE;

        this->_sFormatter = NULL;

        this->_stitlelen = 0;
        this->_sftypelen = 0;

        this->_headers[TT_LID_CREATETIME] = new CZResourceString(IDS_DISKMAP_TTIP_TIMECREATE);
        this->_headers[TT_LID_MODIFYTIME] = new CZResourceString(IDS_DISKMAP_TTIP_TIMEMODIFY);
        this->_headers[TT_LID_ACCESSTIME] = new CZResourceString(IDS_DISKMAP_TTIP_TIMEACCESS);

        this->_headers[TT_LID_DATASIZE] = new CZResourceString(IDS_DISKMAP_TTIP_SIZEDATA);
        this->_headers[TT_LID_REALSIZE] = new CZResourceString(IDS_DISKMAP_TTIP_SIZEREAL);
        this->_headers[TT_LID_DISKSIZE] = new CZResourceString(IDS_DISKMAP_TTIP_SIZEDISK);

        this->_headers[TT_LID_FILECOUNT] = new CZResourceString(IDS_DISKMAP_TTIP_COUNTFILE);
        this->_headers[TT_LID_DIRSCOUNT] = new CZResourceString(IDS_DISKMAP_TTIP_COUNTDIRS);

        for (int i = 0; i < TT_LID_MAX; i++)
        {
            this->_values[i] = new CZStringBuffer(MAX_DATELEN);
        }
        this->_vals_x = 0;

        this->_hfnormal = NULL;
        this->_hftitle = NULL;

        this->_iconLoader = NULL;
        this->Create(FALSE); //FIXME?

        this->_iconLoader = new CZSmartIconLoader(this->_hWnd, WM_APP_ICONLOADED);
    }

    ~CFileInfoTooltip()
    {
        for (int i = 0; i < TT_LID_MAX; i++)
        {
            delete this->_values[i];
            delete this->_headers[i];
        }
        if (this->_sFormatter)
            delete this->_sFormatter;
        this->_sFormatter = NULL;
        if (this->_hicon)
            DestroyIcon(this->_hicon);
        this->_hicon = NULL;

        if (this->_iconLoader)
            delete this->_iconLoader;
        this->_iconLoader = NULL;

        if (this->_hfnormal)
            DeleteObject(this->_hfnormal);
        this->_hfnormal = NULL;
        if (this->_hftitle)
            DeleteObject(this->_hftitle);
        this->_hftitle = NULL;
        this->Destroy();
    }

    ETooltipPathFormat GetPathFormat() { return this->_pathFormat; }
    void SetPathFormat(ETooltipPathFormat format)
    {
        if (this->_pathFormat != format)
        {
            this->_pathFormat = format;
            this->MakeDirty();
            /*if (this->IsVisible())
			{
				this->PrepareTitle();
				this->CalcSize();
			}*/
        }
    }

    void SetDirInfo(CZDirectory* root, CZDirectory* view)
    {
        if ((this->_rootRelDir != root) || (this->_viewRelDir != view))
        {
            this->_rootRelDir = root;
            this->_viewRelDir = view;
            this->MakeDirty();
            /*if (this->IsVisible())
			{
				this->PrepareTitle();
				this->CalcSize();
			}*/
        }
    }

    int CalcHeadersWidth(HDC hdc)
    {
        SIZE sz;
        int width = 0;

        for (int i = 0; i < TT_LID_MAX; i++)
        {
            GetTextExtentPoint32(hdc, this->_headers[i]->GetString(), (int)this->_headers[i]->GetLength(), &sz);
            width = max(sz.cx, width);
        }
        return width;
    }
    int CalcValuesWidth(HDC hdc)
    {
        SIZE sz;
        int width = 0;

        for (int i = 0; i < TT_LID_MAX; i++)
        {
            GetTextExtentPoint32(hdc, this->_values[i]->GetString(), (int)this->_values[i]->GetLength(), &sz);
            width = max(sz.cx, width);
        }
        return width;
    }

    void ShowDirectoryInfo(CZDirectory* dir)
    {
        if (dir == NULL)
            return;
        if (!this->_isDirty && (this->_file == dir))
            return;
        if (this->_hWnd == NULL)
            return;

        this->_type = ETT_FolderInfo;
        this->_file = dir;

        this->PrepareTitle();
        //this->_stitlelen = dir->GetFullName(this->_stitle, ARRAYSIZE(this->_stitle));

        //this->_stitlelen = dir->GetFullName(this->_stitle, 2 * MAX_PATH);
        //this->_stitlelen = _tcslen(this->_stitle);

        dir->LoadFileInfo(NULL, this->_sftype);
        this->_sftypelen = _tcslen(this->_sftype);

        if (this->_sFormatter)
        {
            this->_sFormatter->FormatInteger(this->_values[TT_LID_FILECOUNT], dir->GetSubFileCount());
            this->_sFormatter->FormatInteger(this->_values[TT_LID_DIRSCOUNT], dir->GetSubDirsCount());

            //this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_ACCESSTIME], 18446744073709551615);

            this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_DATASIZE], dir->GetSizeEx(FILESIZE_DATA));
            //this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_REALSIZE], file->GetSizeEx(FILESIZE_REAL));
            this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_DISKSIZE], dir->GetSizeEx(FILESIZE_DISK));
        }

        this->CalcSize();
    }

    void ShowFileInfo(CZFile* file)
    {
        if (file == NULL)
            return;
        if (!this->_isDirty && (this->_file == file))
            return;
        if (this->_hWnd == NULL)
            return;

        this->_type = ETT_FileInfo;
        this->_file = file;

        this->PrepareTitle();

        //this->_stitlelen = file->GetFullName(this->_stitle, 2 * MAX_PATH);
        //file->GetRelativeName(file->GetParent()->GetRoot(),this->_stitle, MAX_PATH);
        //this->_stitlelen = _tcslen(this->_stitle);

        file->LoadFileInfo(NULL, this->_sftype);
        this->_sftypelen = _tcslen(this->_sftype);

        if (this->_sFormatter)
        {
            this->_sFormatter->FormatLongFileDate(this->_values[TT_LID_CREATETIME], file->GetCreateTime());
            this->_sFormatter->FormatLongFileDate(this->_values[TT_LID_MODIFYTIME], file->GetModifyTime());

            //this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_ACCESSTIME], 18446744073709551615);

            this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_DATASIZE], file->GetSizeEx(FILESIZE_DATA));
            //this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_REALSIZE], file->GetSizeEx(FILESIZE_REAL));
            this->_sFormatter->FormatHumanFileSize(this->_values[TT_LID_DISKSIZE], file->GetSizeEx(FILESIZE_DISK));
        }

        this->CalcSize();
    }

    void CalcSize()
    {
        SIZE wsz;
        wsz.cx = 200;
        wsz.cy = 40;

        HDC hdc = GetDC(this->_hWnd);
        SIZE sz;

        this->_headerheight = TT_MARGIN_Y;

        HFONT hfold;
        hfold = SelectFont(hdc, this->_hftitle);
        GetTextExtentPoint32(hdc, this->_stitle, (int)this->_stitlelen, &sz);
        wsz.cx = max(sz.cx + 32 + TT_MARGIN_X + TT_MARGIN_X, wsz.cx);
        this->_headerheight += sz.cy;

        //Select normal font
        SelectFont(hdc, this->_hfnormal);
        //Check type length
        GetTextExtentPoint32(hdc, this->_sftype, (int)this->_sftypelen, &sz);
        wsz.cx = max(sz.cx + 32 + TT_MARGIN_X + TT_MARGIN_X, wsz.cx);

        //Check normal font height
        GetTextExtentPoint32(hdc, TEXT("W"), 1, &sz);
        this->_headerheight += sz.cy; //even when type string is empty add the space so the tooltip doesn't change its size unexpectedly

        int dataheight = 0;
        for (int i = 0; i < TT_LINECOUNT; i++)
        {
            if (TT_LineInfo[this->_type][i] < 0)
            {
                dataheight += -TT_LineInfo[this->_type][i];
            }
            else
            {
                dataheight += sz.cy;
            }
        }
        this->_headerheight = max(32, this->_headerheight);
        wsz.cy = this->_headerheight + TT_MARGIN_Y / 2 + dataheight;

        int hwidth = this->CalcHeadersWidth(hdc);
        int vwidth = this->CalcValuesWidth(hdc);

        this->_vals_x = hwidth + TT_MARGIN_X;
        if (this->_vals_x + vwidth > wsz.cx)
            wsz.cx = this->_vals_x + vwidth;

        SelectFont(hdc, hfold);

        ReleaseDC(this->_hWnd, hdc);

        this->Size(wsz.cx, wsz.cy);

        if (this->_hicon)
            DestroyIcon(this->_hicon);
        this->_hicon = NULL;

        if (this->_iconLoader)
            this->_iconLoader->PlanLoadIcon(this->_file, SHGFI_SHELLICONSIZE | SHGFI_ADDOVERLAYS);

        this->_isDirty = FALSE;
        this->Repaint(TRUE);
    }

    void Move(int x, int y, BOOL isMouse)
    {
        POINT pt;
        pt.x = x;
        pt.y = y;
        RECT rcWork;
        HMONITOR hmn = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mni;
        mni.cbSize = sizeof mni;
        if (GetMonitorInfo(hmn, &mni))
        {
            rcWork = mni.rcWork;
        }
        else
        {
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
        }

        if (isMouse)
        {
            if (y + this->_height + TT_UNDER_OFFSET_Y > rcWork.bottom)
            {
                x = x + TT_ABOVE_OFFSET_X;
                y = y - TT_ABOVE_OFFSET_Y - this->_height;
            }
            else
            {
                x = x + TT_UNDER_OFFSET_X;
                y = y + TT_UNDER_OFFSET_Y;
            }
        }
        else
        {
            if (y + this->_height > rcWork.bottom)
                y = y - this->_height;
        }

        if (x + this->_width > rcWork.right)
            x = rcWork.right - this->_width;

        if (x < rcWork.left)
            x = rcWork.left;
        if (y < rcWork.top)
            y = rcWork.top;

        this->_x = x;
        this->_y = y;
        MoveWindow(this->_hWnd, this->_x, this->_y, this->_width, this->_height, TRUE);
        /*SetWindowPos(
			this->_hWnd, HWND_NOTOPMOST, 
			this->_x, this->_y, this->_width, this->_height, 
			SWP_NOZORDER | SWP_NOACTIVATE);*/
    }
    void Size(int w, int h)
    {
        this->_width = w + 2 * (TT_MARGIN_X + 1);
        this->_height = h + 2 * (TT_MARGIN_Y + 1);
    }
    void OnIconLoad(HICON hicon, CZFile* file)
    {
        if (file == this->_file)
        {
            if (this->_hicon)
                DestroyIcon(this->_hicon);
            this->_hicon = hicon;
            this->Repaint(TRUE); //kresli se jen ikona pres prazdne misto
        }
        else
        {
            DestroyIcon(hicon);
        }
    }
    void DoPaint(PAINTSTRUCT* ps)
    {
        /*
		::FrameRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_WINDOWFRAME));
		InflateRect(&widgetRect, -1, -1);
		::FillRect(hdc, &widgetRect, ::GetSysColorBrush(COLOR_INFOBK));
*/

        HDC hdc = ps->hdc;
        RECT oR;
        GetClientRect(this->_hWnd, &oR);

        HFONT hfold;
        SetBkColor(hdc, GetSysColor(COLOR_INFOBK));
        SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));

        //File Name
        hfold = SelectFont(hdc, this->_hftitle);
        SIZE szc;
        GetTextExtentPoint32(hdc, TEXT("W"), 1, &szc);

        int linepos = TT_MARGIN_Y + max(16, szc.cy + TT_MARGIN_Y / 2);
        RECT rct = oR;
        rct.bottom = linepos + 1;
        ExtTextOut(hdc, TT_MARGIN_X + TT_MARGIN_X + 32 + TT_MARGIN_X / 2, TT_MARGIN_Y + 0, ETO_OPAQUE, &rct, this->_stitle, (UINT)this->_stitlelen, NULL);

        //Dividing line
        MoveToEx(hdc, TT_MARGIN_X + TT_MARGIN_X + 32, linepos, NULL);
        LineTo(hdc, oR.right - TT_MARGIN_X, linepos);

        //File Type
        SelectFont(hdc, this->_hfnormal);
        SIZE szn;
        GetTextExtentPoint32(hdc, TEXT("W"), 1, &szn);

        int y = TT_MARGIN_Y + this->_headerheight + TT_MARGIN_Y / 2;

        rct.top = rct.bottom;
        rct.bottom = y;
        ExtTextOut(hdc, TT_MARGIN_X + TT_MARGIN_X + 32 + TT_MARGIN_X / 2, TT_MARGIN_Y + this->_headerheight - szn.cy, ETO_OPAQUE, &rct, this->_sftype, (UINT)this->_sftypelen, NULL);

        for (int i = 0; i < TT_LINECOUNT; i++)
        {
            int j = TT_LineInfo[this->_type][i];
            if (j < 0)
            {
                y += -j;
            }
            else
            {
                rct.top = rct.bottom;
                rct.bottom = y + szn.cy;
                rct.left = oR.left;
                rct.right = TT_MARGIN_X + this->_vals_x;
                ExtTextOut(hdc, TT_MARGIN_X, y, ETO_OPAQUE, &rct, this->_headers[j]->GetString(), (UINT)this->_headers[j]->GetLength(), NULL);
                rct.left = rct.right;
                rct.right = oR.right;
                ExtTextOut(hdc, TT_MARGIN_X + this->_vals_x, y, ETO_OPAQUE, &rct, this->_values[j]->GetString(), (UINT)this->_values[j]->GetLength(), NULL);
                y += szn.cy;
            }
        }
        rct.left = oR.left;
        rct.top = rct.bottom;
        rct.bottom = oR.bottom;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rct, NULL, 0, NULL);

        int iy = (this->_headerheight - 32) / 2;
        //TODO: Detect icon size
        //HACK: Force 32x32 icon - auto size looks awful on high DPI system
        if (this->_hicon)
            DrawIconEx(hdc, TT_MARGIN_X + 0, TT_MARGIN_Y + iy, this->_hicon, 32, 32, 0, GetSysColorBrush(COLOR_INFOBK), DI_NORMAL);
        SelectObject(hdc, hfold);
    }

    void OnSystemChange()
    {
        if (this->_hfnormal)
            DeleteFont(this->_hfnormal);
        if (this->_hftitle)
            DeleteFont(this->_hftitle);

        if (this->_sFormatter)
            delete this->_sFormatter;

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof ncm;
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
        LOGFONT cpt = ncm.lfStatusFont;
        cpt.lfWeight = FW_BOLD;
        this->_hftitle = CreateFontIndirect(&cpt);
        this->_hfnormal = CreateFontIndirect(&ncm.lfStatusFont);

        this->_sFormatter = new CStringFormatter(LOCALE_USER_DEFAULT);

        this->Repaint(TRUE);
    }
    void OnSysColorChange()
    {
        this->Repaint(TRUE);
    }
    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_SYSCOLORCHANGE:
            return this->OnSysColorChange(), 0;

        case WM_SETTINGCHANGE:
            return this->OnSystemChange(), 0;

        case WM_APP_ICONLOADED:
            return this->OnIconLoad((HICON)wParam, (CZFile*)lParam), 0;

        case WM_ERASEBKGND:
            return TRUE;
        case WM_PAINT:
            return this->OnPaint(), 0;

        case WM_CREATE:
            return this->OnCreate((LPCREATESTRUCT)lParam) ? 0 : (LRESULT)-1L;
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
            BOOL isShadow = FALSE;
            if (SalIsWindowsVersionOrGreater(6, 0, 0) || // Vista
                SalIsWindowsVersionOrGreater(5, 1, 0))   // Windows XP
            {
                SystemParametersInfo(SPI_GETDROPSHADOW, 0, &isShadow, 0);
            }

            WNDCLASSEX wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);

            wcex.style = (isShadow ? CS_DROPSHADOW : 0);
            wcex.lpfnWndProc = CFileInfoTooltip::s_WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = CWindow::s_hInstance;
            wcex.hIcon = NULL;
            wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
            //wcex.hbrBackground  = NULL;
            wcex.hbrBackground = (HBRUSH)(COLOR_INFOBK + 1);
            wcex.lpszMenuName = NULL;
            wcex.lpszClassName = szToolTipWindowClass;
            wcex.hIconSm = NULL;

            a = ::RegisterClassEx(&wcex);
        }
        BOOL ret = (a != NULL);
        return ret;
    }
    static BOOL UnregisterClass()
    {
        BOOL ret = ::UnregisterClass(szToolTipWindowClass, CWindow::s_hInstance);
        if (!ret)
            TRACE_E("UnregisterClass(szToolTipWindowClass) has failed");
        return ret;
    }
};
