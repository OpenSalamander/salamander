// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CChildWindow.h"
#include "Utils.CZString.h"
#include "TreeMap.CDiskMap.h"
#include "GUI.LoadAnimation.h"
#include "GUI.ViewConnectorBase.h"

const TCHAR szDiskMapViewClass[] = TEXT("Zar.DM.DMView.WC");

#define IDT_ANIMTIMER 1
#define ANIMTIMER_TIME 10

class CLogger;

class CDiskMapView : public CChildWindow
{
protected:
    CDiskMap* _diskmap;

    CAnimation* _anim;
    CLoadAnimation* _loadAnim;

    CLogger* _logger;
    CFileInfoTooltip* _tooltip;
    CShellMenu* _shellmenu;

    BOOL _trackingMouse;
    BOOL _isSizing;

    CZString* _path;

    CViewConnectorBase* _connector;

    SIZE _mapsize;

    POINT _cursorPos;

    BOOL _lastwasMouse;
    POINT _lastmousePos;

    DWORD _cushionDesign;
    CTreeMapRendererBase* _renderer;

    BOOL _tooltipEnabled;

    HWND DoCreate(int left, int top, int width, int height)
    {
        return MyCreateWindow(
            0,
            szDiskMapViewClass,
            TEXT("DiskMap"),
            WS_CHILD | WS_VISIBLE,
            left, top,     //X, Y
            width, height, // WIDTH, HEIGHT
            NULL);
    }
    void DoPaint(PAINTSTRUCT* pps)
    {
        HDC hdc = pps->hdc;

#ifdef _DEBUG
        LARGE_INTEGER lt1, lt2, lf;
        QueryPerformanceCounter(&lt1);
#endif

        RECT rect;
        GetClientRect(this->_hWnd, &rect);

        if (this->_anim)
        {
            this->_anim->Paint(hdc, rect);
        }
        else if (this->_diskmap && this->_diskmap->Paint(hdc, rect))
        {
            //OK... no body - vse nakresleno v DiskMap::Paint()
        }
        else
        {
            //TODO: Display something!
            BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, 0, 0, BLACKNESS);

            //HACK: toto bylo udelano narychlo, aby neco bylo pro verzi 1.0; nastesti by nemelo nastat moc casto
            //TODO: predelat poradne
            NONCLIENTMETRICS ncm;
            ncm.cbSize = sizeof ncm;
            SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
            HFONT hfnormal = CreateFontIndirect(&ncm.lfStatusFont);

            HFONT ofont = SelectFont(hdc, hfnormal);

            CZResourceString info1(IDS_DISKMAP_ABORT_TITLE);
            CZResourceString info2(IDS_DISKMAP_ABORT_FOOTER);
            SIZE sz1;
            SIZE sz2;
            GetTextExtentPoint32(hdc, info1.GetString(), (int)info1.GetLength(), &sz1);
            GetTextExtentPoint32(hdc, info2.GetString(), (int)info2.GetLength(), &sz2);

            int w = (8 + 1) * 24 + 3; // z LoadAnimation
            int x = (rect.left + rect.right - w) / 2;
            int y = (rect.bottom - 8 - 4) / 2 + 8 + 4 + 6; //odpovida titulku z LoadAnimation

            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkColor(hdc, RGB(0, 0, 0));

            ExtTextOut(hdc, x, y, 0, NULL, info1.GetString(), (UINT)info1.GetLength(), NULL);

            y += sz1.cy + 2;

            HPEN pn_rct = CreatePen(PS_SOLID, 1, RGB(192, 192, 192));

            HPEN open = SelectPen(hdc, pn_rct);
            //Dividing line
            MoveToEx(hdc, x, y, NULL);
            LineTo(hdc, x + w, y);

            SelectPen(hdc, open);
            DeletePen(pn_rct);

            y += 6;

            ExtTextOut(hdc, x + w - sz2.cx, y, 0, NULL, info2.GetString(), (UINT)info2.GetLength(), NULL);

            SelectFont(hdc, ofont);
            DeleteFont(hfnormal);
        }

#ifdef _DEBUG
        QueryPerformanceCounter(&lt2);
        QueryPerformanceFrequency(&lf);

        static TCHAR sbuff[260];
        static int slen = 0;
        double tf = (double)(lt2.QuadPart - lt1.QuadPart) / lf.QuadPart;
        slen = _stprintf(sbuff, TEXT("WM_PAINT - 1: t1=%1.8lg   t2=%1.8f   f=%I64d"), tf, tf, lf.QuadPart);
        TextOut(hdc, 0, 60, sbuff, slen);
#endif
    }

    void OnAppDirPopulated(WPARAM wParam, LPARAM lParam)
    {
        if (this->_diskmap)
        {
            this->_diskmap->OnDirPopulated(wParam, lParam);
            if (this->_anim)
                delete this->_anim;
            this->_anim = NULL;
            this->_loadAnim = NULL;
            this->UpdateMapView();
        }
    }
    void OnLButtonDown(BOOL fDoubleClick, int x, int y, UINT keyFlags)
    {
        CCushionHitInfo* cshi = this->SelectCushionByPos(x, y);
        if (cshi != NULL)
        {
            if (fDoubleClick == TRUE)
            {
                if (this->_diskmap->ZoomIn())
                {
                    this->UpdateMapView();
                }
            }
            delete cshi;
        }
    }
    void OnRButtonDown(/*BOOL fDoubleClick, */ int x, int y, UINT keyFlags)
    {
        CCushionHitInfo* cshi = this->SelectCushionByPos(x, y);
        delete cshi;
    }

    void OnContextMenu(HWND hwndContext, int xPos, int yPos)
    {
        if (this->_shellmenu && this->_diskmap && this->_diskmap->GetSelectedCushion() != NULL)
        {
            CCushion* cs = this->_diskmap->GetSelectedCushion();
            if (xPos == -1 && yPos == -1) // z klavesnice
            {
                /*POINT pt;
				this->_diskmap->GetSelectedCushionPos(&pt);*/

                RECT rct;
                this->_diskmap->GetSelectedCushionRect(&rct);

                POINT pt;
                pt.x = (rct.left + rct.right) >> 1;
                pt.y = (rct.top + rct.bottom) >> 1;
                ClientToScreen(this->_hWnd, &pt);

                xPos = pt.x;
                yPos = pt.y;
            }
            //Tooltip->Hide
            CZFile* f = cs->GetFile();
            if (f != NULL)
            {
                TCHAR buff[2 * MAX_PATH + 1];
                f->GetFullName(buff, ARRAYSIZE(buff));
                int icmd = 0;
#ifdef SALAMANDER
                if (this->_connector->GetSalamander() != NULL)
                {
                    icmd = this->_shellmenu->ShowFileMenu(buff, xPos, yPos, this->_connector->GetSalamander()->CanFocusFile(), this->_diskmap->CanZoomIn(), this->_diskmap->CanZoomOut());
                }
                else
                {
                    icmd = this->_shellmenu->ShowFileMenu(buff, xPos, yPos, FALSE, this->_diskmap->CanZoomIn(), this->_diskmap->CanZoomOut());
                }
#else
                icmd = this->_shellmenu->ShowFileMenu(buff, xPos, yPos, this->_diskmap->CanZoomIn(), this->_diskmap->CanZoomOut());
#endif
                if (icmd == IDM_ZOOMIN)
                {
                    if (this->_diskmap->ZoomIn())
                    {
                        this->UpdateMapView();
                    }
                }
                else if (icmd == IDM_ZOOMOUT)
                {
                    if (this->_diskmap->ZoomOut())
                    {
                        this->UpdateMapView();
                    }
                }
#ifdef SALAMANDER
                else if (icmd == IDM_FOCUS)
                {
                    if (this->_connector->GetSalamander())
                        this->_connector->GetSalamander()->FocusFile(buff);
                }
#endif
            }
        }
    }
    void ShowTooltip(CZFile* file, int x, int y, BOOL isMouse)
    {
        if (this->_tooltip && this->_tooltipEnabled)
        {
            this->_tooltip->ShowFileInfo(file);

            POINT pt;
            pt.x = x;
            pt.y = y;
            ClientToScreen(this->_hWnd, &pt);
            this->_tooltip->Move(pt.x, pt.y, isMouse);

            this->_tooltip->Show(SW_SHOWNOACTIVATE);

            if (isMouse && !this->_trackingMouse)
            {
                TRACKMOUSEEVENT trme;
                trme.cbSize = sizeof trme;
                trme.dwFlags = TME_LEAVE;
                trme.hwndTrack = this->_hWnd;
                this->_trackingMouse = TrackMouseEvent(&trme);
            }
        }
    }
    void ReshowTooltip()
    {
        if (this->_lastwasMouse)
        {
            int x = this->_lastmousePos.x;
            int y = this->_lastmousePos.y;
            CCushionHitInfo* cshi = this->_diskmap->GetCushionByLocation(x, y);
            if (cshi != NULL)
            {
                CZFile* file = cshi->GetCushion()->GetFile();
                if (file)
                {
                    this->ShowTooltip(file, x, y, true);
                }
                delete cshi;
            }
        }
        else
        {
            MoveCursor(NULL);
        }
    }
    void HideTooltip()
    {
        if (this->_tooltip)
            this->_tooltip->Hide();
    }
    void OnMouseMove(int x, int y, WPARAM keyFlags)
    {
        if (this->_diskmap)
        {
            if (this->_lastwasMouse || this->_lastmousePos.x != x || this->_lastmousePos.y != y)
            {
                CCushionHitInfo* cshi = this->_diskmap->GetCushionByLocation(x, y);
                if (cshi != NULL)
                {
                    CZFile* file = cshi->GetCushion()->GetFile();
                    if (file)
                    {
                        this->ShowTooltip(file, x, y, true);
                    }
                    else
                    {
                        this->HideTooltip();
                    }
                    delete cshi;
                }
                this->_lastmousePos.x = x;
                this->_lastmousePos.y = y;
                this->_lastwasMouse = true;
            }
        }
    }
    void OnMouseLeave()
    {
        this->_trackingMouse = FALSE;
        if (this->_tooltip)
            this->_tooltip->Hide();
    }
    void OnTimer(UINT id)
    {
        if (this->_anim)
        {
            if (this->_anim->OnTimerUpdate())
            {
                if (this->_loadAnim)
                {
                    int filecount, dircount;
                    INT64 datasize;
                    this->_diskmap->GetStats(filecount, dircount, datasize);
                    this->_loadAnim->SetInfo(filecount, dircount, datasize);
                }
                this->Repaint();
            }
        }
    }
    CCushionHitInfo* SelectCushionByPos(int x, int y)
    {
        if (this->_diskmap)
        {
            CCushionHitInfo* cshi = this->_diskmap->GetCushionByLocation(x, y);
            if (cshi != NULL)
            {
                this->_cursorPos.x = x;
                this->_cursorPos.y = y;
                if (this->_diskmap->SelectCushion(cshi))
                    this->Repaint();
            }
            return cshi;
        }
        return NULL;
    }
    BOOL UpdateMapView()
    {
        this->HideTooltip();
        //this->_diskmap->GetParentDir()
        this->_tooltip->SetDirInfo(this->_diskmap->GetRootDir(), this->_diskmap->GetViewDir());

        TCHAR buff[MAX_PATH];
        this->_diskmap->GetSubDirName(buff, ARRAYSIZE(buff));
        CZString s(buff);
        this->_connector->DL_SetSubPath(&s);

        INT64 size = this->_diskmap->GetDirSize();
        this->_connector->DL_SetDiskSize(size);

        return this->UpdateMapSize(TRUE);
    }
    BOOL UpdateMapSize(BOOL force)
    {
        if (!this->_diskmap)
            return FALSE;

        RECT ClientRect;
        GetClientRect(this->_hWnd, &ClientRect);

        if (force == FALSE && this->_mapsize.cx == ClientRect.right && this->_mapsize.cy == ClientRect.bottom)
            return TRUE;

        if (ClientRect.right <= 0 || ClientRect.bottom <= 0)
            return TRUE;

        this->_diskmap->PrepareView(ClientRect.right, ClientRect.bottom, this->_renderer);
        this->_mapsize.cx = ClientRect.right;
        this->_mapsize.cy = ClientRect.bottom;

        this->Repaint();

        return TRUE;
    }
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct)
    {
        this->_diskmap = new CDiskMap(this->_hWnd, this->_logger);

        SetTimer(this->_hWnd, IDT_ANIMTIMER, ANIMTIMER_TIME, NULL);

        UpdateFileList();
        return true;
    }
    void OnSize(WPARAM state, int cx, int cy)
    {
        if (!this->_isSizing)
        {
            /*this->_diskmap->Dim();
			this->Repaint();*/
            this->UpdateMapSize(FALSE);
        }
    }

    void OnDestroy()
    {
        if (this->_diskmap)
            delete this->_diskmap;
        this->_diskmap = NULL;

        this->_loadAnim = NULL;
        if (this->_anim)
            delete this->_anim;
        this->_anim = NULL;
    }

public:
    CDiskMapView(HWND hwndParent, int top, CLogger* logger, CFileInfoTooltip* tooltip, CShellMenu* shellmenu) : CChildWindow(hwndParent)
    {
        this->_diskmap = NULL;

        this->_anim = NULL;
        this->_loadAnim = NULL;

        this->_trackingMouse = FALSE;
        this->_isSizing = FALSE;

        this->_logger = logger;
        this->_tooltip = tooltip;
        this->_shellmenu = shellmenu;

        this->_path = NULL;

        this->_connector = NULL;

        this->_cushionDesign = 0;

        this->_tooltipEnabled = TRUE;

        //CTreeMapRendererBase *renderer = new CTreeMapRendererBase(dsSquare);
        //CTreeMapRendererBase *renderer = new CTreeMapRendererMaxRatio(dsSquare, 0);//Sequoia
        //CTreeMapRendererBase *renderer = new CTreeMapRendererMaxRatio(dsLonger, 2.5);KDirStat

        this->_renderer = new CTreeMapRendererMaxRatio(dsLonger, 2.5); //KDirStat

        this->Create(top);
    }
    ~CDiskMapView()
    {
        if (this->_renderer)
            delete this->_renderer;
        this->_renderer = NULL;

        if (this->_path)
            delete this->_path;
        this->_path = NULL;

        this->Destroy();
    }

    CZDirectory* GetViewDirectory(int parentlevel)
    {
        return this->_diskmap->GetParentDir(parentlevel);
    }

    BOOL GetDirectoryOverlayVisibility()
    {
        return this->_diskmap->GetDirectoryOverlayVisibility();
    }
    void SetDirectoryOverlayVisibility(BOOL isVisible)
    {
        if (this->_diskmap->GetDirectoryOverlayVisibility() != isVisible)
        {
            this->_diskmap->SetDirectoryOverlayVisibility(isVisible);
            this->Repaint(TRUE);
        }
    }

    BOOL GetTooltipEnabled()
    {
        return this->_tooltipEnabled;
    }
    void SetTooltipEnabled(BOOL isEnabled)
    {
        if (this->_tooltipEnabled != isEnabled)
        {
            this->_tooltipEnabled = isEnabled;
            if (this->_tooltipEnabled)
            {
                this->ReshowTooltip();
            }
            else
            {
                this->HideTooltip();
            }
        }
    }

    void ShowContextMenu(int x = -1, int y = -1)
    {
        this->OnContextMenu(this->_hWnd, x, y);
    }
    void SetSizingState(BOOL isSizing)
    {
        if (this->_isSizing != isSizing)
        {
            this->_isSizing = isSizing;
            if (!isSizing)
            {
                this->UpdateMapSize(FALSE);
            }
        }
    }

    BOOL SetCushionDesign(DWORD resourceID)
    {
        //this->_diskmap->LoadGraphicsFromResource(CWindow::s_hInstance, MAKEINTRESOURCE(IDR_CUSHIONDATA_GLASS));
        if (this->_cushionDesign != resourceID)
        {
            if (this->_diskmap->LoadGraphicsFromResource(CWindow::s_hInstance, MAKEINTRESOURCE(resourceID)))
            {
                this->_cushionDesign = resourceID;
            }
            else
            {
                this->_cushionDesign = 0;
            }
            this->UpdateMapSize(TRUE);
            return TRUE;
        }
        return FALSE;
    }

    DWORD GetCushionDesignID() { return this->_cushionDesign; }

    HWND Create(int top)
    {
        this->SetMargins(0, top, 0, 0);

        return this->CChildWindow::Create(this->_x, this->_y, this->_width, this->_height);
    }

    BOOL IsFileSelected()
    {
        BOOL isFileSelected = FALSE;
        CCushion* csel = this->_diskmap->GetSelectedCushion();
        if (csel != NULL && csel->GetFile() != NULL)
            isFileSelected = TRUE;
        return isFileSelected;
    }
    BOOL OpenSelectedFile()
    {
        if (this->_shellmenu != NULL)
        {
            CCushion* csel = this->_diskmap->GetSelectedCushion();
            if (csel != NULL)
            {
                CZFile* f = csel->GetFile();
                if (f != NULL)
                {
                    TCHAR buff[2 * MAX_PATH + 1];
                    f->GetFullName(buff, ARRAYSIZE(buff));
                    return this->_shellmenu->InvokeDefaultCommand(buff);
                }
            }
        }
        return FALSE;
    }
    BOOL MoveCursor(UINT vkey)
    {
        RECT rct;
        int x = 0;
        int y = 0;

        if (this->_diskmap->GetSelectedCushionRect(&rct))
        {
            x = this->_cursorPos.x;
            y = this->_cursorPos.y;

            if (x < rct.left)
                x = rct.left;
            if (x >= rct.right)
                x = rct.right - 1;

            if (y < rct.top)
                y = rct.top;
            if (y >= rct.bottom)
                y = rct.bottom - 1;

            switch (vkey)
            {
            case VK_LEFT:
                x = rct.left - 1;
                break;
            case VK_RIGHT:
                x = rct.right;
                break;
            case VK_UP:
                y = rct.top - 1;
                break;
            case VK_DOWN:
                y = rct.bottom;
                break;
            case NULL:
                break;
            }
        }

        CCushionHitInfo* cshi = this->SelectCushionByPos(x, y);
        if (cshi != NULL)
        {
            //aby po vybrani upravena souradnice nebyla na okraji polstare, ale uprostred - lepe pochopitelne
            switch (vkey)
            {
            case VK_LEFT:
            case VK_RIGHT:
                this->_cursorPos.x = cshi->GetPosX() + cshi->GetWidth() / 2;
                break;
            case VK_UP:
            case VK_DOWN:
                this->_cursorPos.y = cshi->GetPosY() + cshi->GetHeight() / 2;
                break;
            }
            //zobrazit tooltip
            CZFile* f = cshi->GetCushion()->GetFile();
            if (f != NULL)
            {
                this->ShowTooltip(f, this->_cursorPos.x, this->_cursorPos.y, false);
            }
            delete cshi;
        }
        this->_lastwasMouse = false;

        return FALSE;
    }
    BOOL GetSelectedFileName(TCHAR* buff, size_t bufflen)
    {
        CCushion* cs = this->_diskmap->GetSelectedCushion();
        if (cs != NULL)
        {
            CZFile* f = cs->GetFile();
            if (f != NULL)
            {
                size_t s = f->GetFullName(buff, bufflen);
                return (s > 0);
            }
        }
        return FALSE;
    }
    BOOL CanAbort()
    {
        return this->_diskmap->CanAbort();
    }
    BOOL CanPopulate()
    {
        return this->_diskmap->CanPopulate();
    }
    BOOL CanZoomIn()
    {
        return this->_diskmap->CanZoomIn();
    }
    BOOL CanZoomOut()
    {
        return this->_diskmap->CanZoomOut();
    }
    BOOL Abort()
    {
        if (this->_diskmap->Abort())
        {
            this->UpdateMapView();
            return TRUE;
        }
        return FALSE;
    }
    BOOL ZoomIn()
    {
        if (this->_diskmap->ZoomIn())
        {
            this->UpdateMapView();
            return TRUE;
        }
        return FALSE;
    }
    BOOL ZoomOut(int level = 1)
    {
        BOOL changed = FALSE;
        while (level > 0 && this->_diskmap->ZoomOut())
        {
            level--;
            changed = TRUE;
        }
        if (changed)
        {
            this->UpdateMapView();
            return TRUE;
        }
        return FALSE;
    }
    BOOL ZoomToRoot()
    {
        if (this->_diskmap->ZoomToRoot())
        {
            this->UpdateMapView();
            return TRUE;
        }
        return FALSE;
    }
    BOOL UpdateFileList()
    {
        if (this->_diskmap)
        {
            if (this->_path != NULL)
            {
                this->HideTooltip();
                if (this->_anim)
                    delete this->_anim;

                this->_loadAnim = new CLoadAnimation();
                this->_anim = this->_loadAnim;

#ifdef SALAMANDER
                int cs = 512;
                if (this->_connector->GetSalamander() != NULL)
                    cs = this->_connector->GetSalamander()->GetClusterSize(this->_path->GetString());
                this->_diskmap->PopulateAsync(this->_path->GetString(), this->_path->GetLength(), cs);
#else
                this->_diskmap->PopulateAsync(this->_path->GetString(), this->_path->GetLength());
#endif

                return TRUE;
            }
        }
        return FALSE;
    }
    void SetConnector(CViewConnectorBase* connector)
    {
        this->_connector = connector;
    }
    BOOL SetPath(TCHAR const* path, BOOL fStartEnum = TRUE)
    {
        if (this->_path)
            delete this->_path;
        this->_path = NULL;
        this->_path = new CZString(path);

        if (fStartEnum)
            this->UpdateFileList();
        return TRUE;
    }
    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            return TRUE;
        case WM_PAINT:
            return this->OnPaint(), 0;

        case WM_CREATE:
            return this->OnCreate((LPCREATESTRUCT)lParam) ? 0 : (LRESULT)-1L;

        case WM_LBUTTONDOWN:
            return this->OnLButtonDown(FALSE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0;
        case WM_LBUTTONDBLCLK:
            return this->OnLButtonDown(TRUE, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0;
        case WM_RBUTTONDOWN:
            return this->OnRButtonDown(/*FALSE,*/ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0;

        case WM_CONTEXTMENU:
            return this->OnContextMenu((HWND)wParam, (short)LOWORD(lParam), (short)HIWORD(lParam)), 0;

        case WM_SIZE:
            return this->OnSize(wParam, (short)LOWORD(lParam), (short)HIWORD(lParam)), 0;

        case WM_MOUSEMOVE:
            return this->OnMouseMove((short)LOWORD(lParam), (short)HIWORD(lParam), wParam), 0;
        case WM_MOUSELEAVE:
            return this->OnMouseLeave(), 0;

        case WM_APP_DIRPOPULATED:
            return this->OnAppDirPopulated(wParam, lParam), 0;

        case WM_TIMER:
            return this->OnTimer((UINT)wParam), 0;

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

            wcex.style = CS_DBLCLKS;
            wcex.lpfnWndProc = CDiskMapView::s_WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = CWindow::s_hInstance;
            wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
            wcex.lpszMenuName = NULL;
            wcex.lpszClassName = szDiskMapViewClass;
            wcex.hIcon = NULL;
            wcex.hIconSm = NULL;

            a = ::RegisterClassEx(&wcex);
        }
        BOOL ret = (a != NULL);
        return ret;
    }
    static BOOL UnregisterClass()
    {
        BOOL ret = ::UnregisterClass(szDiskMapViewClass, CWindow::s_hInstance);
        if (!ret)
            TRACE_E("UnregisterClass(szDiskMapViewClass) has failed");
        return ret;
    }
};
