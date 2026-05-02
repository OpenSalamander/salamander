// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "lib/pvw32dll.h"
#include "renderer.h"
#include "pictview.h"
#include "pictview.rh"
#include "pictview.rh2"
#include "lang/lang.rh"

//****************************************************************************
//
// MultipleMonitors
//
// Returns TRUE if more than one monitor is present.
// If 'boundingRect' is not NULL, it receives the bounding rectangle of all monitors,
// i.e. the dimensions of the virtual desktop.
//

struct MonitorEnumProcData
{
    int Count;
    RECT* BoundingRect;
};

BOOL CALLBACK
MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MonitorEnumProcData* data = (MonitorEnumProcData*)dwData;
    data->Count++;

    if (data->BoundingRect != NULL)
    {
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(hMonitor, &mi);

        UnionRect(data->BoundingRect, &mi.rcMonitor, data->BoundingRect);
    }

    return TRUE;
}

BOOL MultipleMonitors(RECT* boundingRect)
{
    MonitorEnumProcData enumData;
    enumData.Count = 0;
    enumData.BoundingRect = boundingRect;
    if (enumData.BoundingRect != NULL)
        SetRectEmpty(enumData.BoundingRect);
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&enumData);
    return enumData.Count > 1;
}

/**********************************************************

GetCurrentCursorHandle (KB Q230495)

  Purpose:
    Retrieves a handle to the current cursor regardless of
    whether or not it's owned by the current thread. This is
    useful, for example, when you need to draw the image
    of the current cursor into a screen capture using
    DrawIcon().

  Input:
    <none>

  Return:
    The return value is the handle to the current cursor.
    If there is no cursor, the return value is NULL.

  Notes:
    This function cannot be used to capture the cursor on
    another desktop.

**********************************************************/

HCURSOR GetCurrentCursorHandle()
{
    POINT pt;
    HWND hWnd;
    DWORD dwThreadID, dwCurrentThreadID;
    HCURSOR hCursor = NULL;

    // Find out which window owns the cursor
    GetCursorPos(&pt);
    hWnd = WindowFromPoint(pt);

    // Get the thread ID for the cursor owner.
    dwThreadID = GetWindowThreadProcessId(hWnd, NULL);

    // Get the thread ID for the current thread
    dwCurrentThreadID = GetCurrentThreadId();

    // If the cursor owner is not us then we must attach to
    // the other thread in so that we can use GetCursor() to
    // return the correct hCursor
    if (dwCurrentThreadID != dwThreadID)
    {
        // Attach to the thread that owns the cursor
        if (AttachThreadInput(dwCurrentThreadID, dwThreadID, TRUE))
        {
            // Get the handle to the cursor
            hCursor = GetCursor();

            // Detach from the thread that owns the cursor
            AttachThreadInput(dwCurrentThreadID, dwThreadID, FALSE);
        }
    }
    else
        hCursor = GetCursor();

    return hCursor;
}

void CRendererWindow::ScreenCapture()
{
    HDC hMonitorDC = CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
    HDC hMemDC = CreateCompatibleDC(hMonitorDC);

    HRGN hWndRgn = CreateRectRgn(0, 0, 0, 0); // window region, will be used for clipping of non-rectangular windows (XP, Vista, ...)
    int wndRgnType = NULLREGION;

    RECT bndR;
    MultipleMonitors(&bndR);
    RECT rect;
    switch (G.CaptureScope)
    {
    case CAPTURE_SCOPE_DESKTOP:
    {
        rect.left = 0;
        rect.top = 0;
        rect.right = GetDeviceCaps(hMonitorDC, HORZRES);
        rect.bottom = GetDeviceCaps(hMonitorDC, VERTRES);
        break;
    }

    case CAPTURE_SCOPE_WINDOW:
    case CAPTURE_SCOPE_CLIENT:
    case CAPTURE_SCOPE_APPL:
    {
        HWND hForeground = GetForegroundWindow();
        if (hForeground != NULL)
        {
            if (G.CaptureScope == CAPTURE_SCOPE_WINDOW)
            {
                GetWindowRect(hForeground, &rect);
                wndRgnType = GetWindowRgn(hForeground, hWndRgn);
            }
            else if (G.CaptureScope == CAPTURE_SCOPE_APPL)
            {
                HWND hParent;

                while (((hParent = GetParent(hForeground)) != NULL) && IsWindowVisible(hParent))
                {
                    hForeground = hParent;
                }
                GetWindowRect(hForeground, &rect);
                wndRgnType = GetWindowRgn(hForeground, hWndRgn);
            }
            else
            {
                GetClientRect(hForeground, &rect);
                MapWindowPoints(hForeground, NULL, (LPPOINT)&rect, 2);
            }
            RECT dummy;
            if (IntersectRect(&dummy, &bndR, &rect))
                break;
        }
        // if the window does not exist or lies outside the usable area, fall back to the virtual screen
    }
    case CAPTURE_SCOPE_VIRTUAL:
    {
        rect = bndR;
        break;
    }
    }
    IntersectRect(&rect, &bndR, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HBITMAP hBitmap = CreateCompatibleBitmap(hMonitorDC, width, height);

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    ShowWindow(Viewer->HWindow, SW_HIDE);
    Sleep(0); // let the window disappear from the taskbar

    if (wndRgnType == COMPLEXREGION) // clip only non-rectangular windows
    {
        // fill "transparent" background
        HBRUSH hBrush = CreateSolidBrush(GetCOLORREF(G.Colors[vceTransparent]));
        RECT fillR = {0, 0, width, height};
        FillRect(hMemDC, &fillR, hBrush);
        DeleteObject(hBrush);

        // clip painting using windows region
        SelectClipRgn(hMemDC, hWndRgn);
    }

    BOOL ret = BitBlt(hMemDC,
                      0, 0,
                      width, height,
                      hMonitorDC,
                      rect.left, rect.top,
                      SRCCOPY);

    ShowWindow(Viewer->HWindow, SW_SHOW);

    // if the user also wants to capture the cursor, add it to the bitmap
    if (G.CaptureCursor)
    {
        HCURSOR hCursor = GetCurrentCursorHandle();

        POINT cursorPos;
        GetCursorPos(&cursorPos);
        ICONINFO ii;
        GetIconInfo(hCursor, &ii);
        DrawIconEx(hMemDC, cursorPos.x - ii.xHotspot - rect.left, cursorPos.y - ii.yHotspot - rect.top,
                   hCursor, 0, 0, 0, NULL, DI_DEFAULTSIZE | DI_NORMAL);
        DeleteObject(ii.hbmMask);
        if (ii.hbmColor != NULL)
            DeleteObject(ii.hbmColor);
    }

    SelectObject(hMemDC, hOldBitmap);

    // clean up after ourselves
    DeleteDC(hMonitorDC);
    DeleteDC(hMemDC);
    // DeleteObject(hBitmap);   // hBitmap is destroyed in OpenFile()
    DeleteObject(hWndRgn);

    CancelCapture();

    // pass the bitmap to PictView
    EnumFilesSourceUID = -1;
    OpenFile(CAPTURE, -1, hBitmap);

    SalamanderGeneral->Free(FileName);
    FileName = SalamanderGeneral->DupStr(LoadStr(IDS_CAPTURE_TITLE));

    // when entering this function the window was minimized, now restore it to its original position
    Viewer->UpdateEnablers();
    Viewer->UpdateToolBar();
    ShowWindow(Viewer->HWindow, SW_RESTORE);
    SetForegroundWindow(Viewer->HWindow);
}

void CRendererWindow::CancelCapture()
{
    if (Capturing)
    {
        if (G.CaptureTrigger == CAPTURE_TRIGGER_HOTKEY)
            UnregisterHotKey(HWindow, G.CaptureAtomID);
        else
            KillTimer(HWindow, CAPTURE_TIMER_ID);
        Capturing = FALSE;
    }
}

void CRendererWindow::FreeComment(void)
{
    if (Comment)
    {
        free(Comment);
        Comment = NULL;
    }
}

void CRendererWindow::DuplicateComment(void)
{
    FreeComment();
    if (pvii.CommentSize && pvii.Comment)
    {
        Comment = (char*)malloc(pvii.CommentSize + 1);
        if (Comment)
        {
            memcpy(Comment, pvii.Comment, pvii.CommentSize);
            Comment[pvii.CommentSize] = 0;
        }
    }
}

PVCODE CRendererWindow::InitiatePageLoad(void)
{
    PVCODE result;
    PVImageInfo pviiNew;

    result = PVW32DLL.PVGetImageInfo(PVHandle, &pviiNew, sizeof(pviiNew), pvii.CurrentImage);
    if (PVC_OK == result)
    {

        SetScrollPos(HWindow, SB_BOTH, 0, TRUE);
        memcpy(&pvii, &pviiNew, sizeof(PVImageInfo));

        DuplicateComment(); // automatically frees the old comment
        XStretchedRange = XRange = max(1, pvii.Width);
        if (pvii.HorDPI && pvii.VerDPI)
        {
            YRange = pvii.Height * pvii.HorDPI / pvii.VerDPI;
        }
        else
        {
            YRange = pvii.Height;
        }
        YStretchedRange = YRange;
        ZoomFactor = ZOOM_SCALE_FACTOR;
        ZoomIndex = 0;
        Canceled = ImageLoaded = DoNotAttempToLoad = FALSE;
        InvalidateRect(HWindow, NULL, FALSE);
        InvalidateCage(&SelectRect);
        Viewer->SetStatusBarTexts();
        WMSize();
        if (pvii.Format == PVF_ANI)
        {
            if (PVSequence)
            {
                // User requested specific page -> stop animating
                KillTimer(HWindow, IMGSEQ_TIMER_ID);
                PVCurImgInSeq = PVSequence = NULL;
            }
            pvii.Flags &= ~PVFF_IMAGESEQUENCE;
        }
    }
    else
    {
        SalamanderGeneral->SalMessageBox(HWindow, (PVC_UNKNOWN_FILE_STRUCT == result) ? LoadStr(IDS_UNSUPPORTED_IMAGE_TYPE) : PVW32DLL.PVGetErrorText(result),
                                         LoadStr(IDS_ERROR_CALLING_PVW32_DLL), MB_ICONEXCLAMATION);
    }
    return result;
} /* CRendererWindow::InitiatePageLoad */

void CRendererWindow::TryEnterHandToolMode(void)
{
    if ((XStretchedRange > PageWidth) || (YStretchedRange > PageHeight))
    {
        ShutdownTool();
        SelectTool(RT_HAND);
        SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_HAND1)));
    }
} /* CRendererWindow::TryEnterHandToolMode */

void CRendererWindow::ShutdownTool(void)
{
    switch (CurrTool)
    {
    case RT_PIPETTE:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        SendMessage(PipWindow, WM_CLOSE, 0, 0);
        PipWindow = NULL;
        break;
    }
    //  CurrTool = RT_HAND;
} /* CRendererWindow::ShutdownTool */

//****************************************************************************
//
// SetAsWallpaper
//

struct CWallpaper
{
    TCHAR Wallpaper[2 * MAX_PATH];
    TCHAR WallpaperStyle[200];
    TCHAR TileWallpaper[200];
};

LPCTSTR REG_WALLPAPER = _T("Wallpaper");
LPCTSTR REG_TILEWALLPAPER = _T("TileWallpaper");
LPCTSTR REG_WALLPAPERSTYLE = _T("WallpaperStyle");
LPCTSTR REG_PREV_WALLPAPER = _T("PrevWallpaper");
LPCTSTR REG_PREV_TILEWALLPAPER = _T("PrevTileWallpaper");
LPCTSTR REG_PREV_WALLPAPERSTYLE = _T("PrevWallpaperStyle");

void GetWallpaper(HKEY hKey, BOOL prev, CWallpaper* wallpaper)
{
    DWORD size;
    size = sizeof(wallpaper->Wallpaper);
    if (SalamanderGeneral->SalRegQueryValueEx(hKey, prev ? REG_PREV_WALLPAPER : REG_WALLPAPER, 0, NULL,
                                              (LPBYTE)wallpaper->Wallpaper, &size) != ERROR_SUCCESS)
    {
        wallpaper->Wallpaper[0] = 0;
    }

    size = sizeof(wallpaper->WallpaperStyle);
    if (SalamanderGeneral->SalRegQueryValueEx(hKey, prev ? REG_PREV_WALLPAPERSTYLE : REG_WALLPAPERSTYLE, 0, NULL,
                                              (LPBYTE)wallpaper->WallpaperStyle, &size) != ERROR_SUCCESS)
    {
        wallpaper->WallpaperStyle[0] = 0;
    }

    size = sizeof(wallpaper->TileWallpaper);
    if (SalamanderGeneral->SalRegQueryValueEx(hKey, prev ? REG_PREV_TILEWALLPAPER : REG_TILEWALLPAPER, 0, NULL,
                                              (LPBYTE)wallpaper->TileWallpaper, &size) != ERROR_SUCCESS)
    {
        wallpaper->TileWallpaper[0] = 0;
    }
}

void SetWallpaper(HKEY hKey, BOOL prev, const CWallpaper* wallpaper)
{
    // RegSetValueEx: cbData in bytes including NULL
    // old RegSetValue: in chars excluding NULL
    RegSetValueEx(hKey, prev ? REG_PREV_WALLPAPER : REG_WALLPAPER, 0, REG_SZ,
                  (LPBYTE)wallpaper->Wallpaper, (DWORD)(_tcslen(wallpaper->Wallpaper) + 1) * sizeof(TCHAR));

    RegSetValueEx(hKey, prev ? REG_PREV_WALLPAPERSTYLE : REG_WALLPAPERSTYLE, 0, REG_SZ,
                  (LPBYTE)wallpaper->WallpaperStyle, (DWORD)(_tcslen(wallpaper->WallpaperStyle) + 1) * sizeof(TCHAR));

    RegSetValueEx(hKey, prev ? REG_PREV_TILEWALLPAPER : REG_TILEWALLPAPER, 0, REG_SZ,
                  (LPBYTE)wallpaper->TileWallpaper, (DWORD)(_tcslen(wallpaper->TileWallpaper) + 1) * sizeof(TCHAR));
}

// stores the currently viewed image into the file specified by fileName (as a BMP)
// returns TRUE if the operation succeeded, otherwise FALSE
BOOL CRendererWindow::SaveWallpaper(LPCTSTR fileName)
{
    int ret;
    SAVEAS_INFO sai;

    memset(&sai, 0, sizeof(SAVEAS_INFO));
    sai.Colors = (pvii.Colors > 256) ? PV_COLOR_TC24 : pvii.Colors;

    ret = SaveImage(fileName, PVF_BMP, &sai);
    if (ret != PVC_OK)
    {
        TCHAR errBuff[1000];

        _stprintf(errBuff, LoadStr(IDS_SAVEERROR), PVW32DLL.PVGetErrorText(ret)); //"Canceled (error example)");
        SalamanderGeneral->SalMessageBox(HWindow, errBuff, LoadStr(IDS_ERRORTITLE),
                                         MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }

    return TRUE;
}

void CRendererWindow::SetAsWallpaper(WORD command)
{
    if (FileName != NULL)
    {
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Control Panel\\Desktop"), 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            switch (command)
            {
            case CMD_WALLPAPER_CENTER:
            case CMD_WALLPAPER_TILE:
            case CMD_WALLPAPER_STRETCH:
            {
                LPCTSTR PICTVIEW_WALLPAPER = _T("PictView_Wallpaper.bmp");

                // save the image into a BMP file in the Windows directory
                // (where it is then left on its own); a cleaner solution is unknown
                TCHAR fileName[MAX_PATH];
                GetWindowsDirectory(fileName, SizeOf(fileName));
                SalamanderGeneral->SalPathAppend(fileName, PICTVIEW_WALLPAPER, MAX_PATH);
                if (SaveWallpaper(fileName))
                {
                    CWallpaper cur;
                    GetWallpaper(hKey, FALSE, &cur);

                    // if the current wallpaper is not ours, back it up as the previous version
                    LPCTSTR s = _tcsrchr(cur.Wallpaper, '\\');
                    if (s == NULL)
                        s = cur.Wallpaper;
                    else
                        s++;
                    if (_tcsicmp(s, PICTVIEW_WALLPAPER) != 0)
                        SetWallpaper(hKey, TRUE, &cur);

                    _tcscpy(cur.Wallpaper, fileName);
                    _tcscpy(cur.WallpaperStyle, command == CMD_WALLPAPER_STRETCH ? _T("2") : _T("0"));
                    _tcscpy(cur.TileWallpaper, command == CMD_WALLPAPER_TILE ? _T("1") : _T("0"));
                    SetWallpaper(hKey, FALSE, &cur);
                }

                break;
            }

            case CMD_WALLPAPER_RESTORE:
            {
                // swap the current and previous versions
                CWallpaper cur, prev;
                GetWallpaper(hKey, TRUE, &prev);
                GetWallpaper(hKey, FALSE, &cur);
                SetWallpaper(hKey, TRUE, &cur);
                SetWallpaper(hKey, FALSE, &prev);
                break;
            }

            case CMD_WALLPAPER_NONE:
            {
                // back up the current one into prev and then clear it
                CWallpaper cur;
                GetWallpaper(hKey, FALSE, &cur);
                SetWallpaper(hKey, TRUE, &cur);
                memset(&cur, 0, sizeof(cur));
                SetWallpaper(hKey, FALSE, &cur);
                break;
            }

            default:
            {
                TRACE_E("Unknown command: " << command);
                break;
            }
            }
            RegCloseKey(hKey);
        }

        // notify the OS that it should perform an update
        SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, NULL, SPIF_SENDCHANGE);
    }
}

void CRendererWindow::SelectTool(eTool tool)
{
    if (tool == CurrTool)
        return;

    CurrTool = tool;
    Viewer->UpdateToolBar();
    Viewer->SetStatusBarTexts();
    POINT p;
    GetCursorPos(&p);
    SetCursorPos(p.x, p.y);
}
