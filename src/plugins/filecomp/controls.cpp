// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <uxtheme.h>

#include <Shlwapi.h>

//#pragma comment(lib, "Shlwapi.lib")  // Petr: ve VC2008 mi neslape, davam to do projektu

HFONT EnvFont;     // font prostredi (edit, toolbar, header, status)
int EnvFontHeight; // vyska fontu
HBRUSH HDitheredBrush = NULL;

// ****************************************************************************
//
// CFileHeaderWindow
//

CFileHeaderWindow::CFileHeaderWindow(const char* text)
{
    CALL_STACK_MESSAGE2("CFileHeaderWindow::CFileHeaderWindow(%s)", text);
    strcpy(Text, text);
    TextLen = int(strlen(text));
    BkgndBrush = NULL;
}

CFileHeaderWindow::~CFileHeaderWindow()
{
    CALL_STACK_MESSAGE1("CFileHeaderWindow::~CFileHeaderWindow()");
    if (BkgndBrush)
        DeleteObject(BkgndBrush);
}

void CFileHeaderWindow::SetText(const char* text)
{
    CALL_STACK_MESSAGE2("CFileHeaderWindow::SetText(%s)", text);
    strcpy(Text, text);
    TextLen = int(strlen(text));
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

LRESULT
CFileHeaderWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFileHeaderWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        BkColor = SG->GetCurrentColor(SALCOL_INACTIVE_CAPTION_BK);
        BkgndBrush = CreateSolidBrush(BkColor);

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(HWindow, &ps);
        HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);

        // zajistime aby jsme meli vzdy aktualni barvu brushe
        COLORREF oldBk = BkColor;
        BkColor = SG->GetCurrentColor(SALCOL_INACTIVE_CAPTION_BK);
        if (BkColor != oldBk)
        {
            if (BkgndBrush)
                DeleteObject(BkgndBrush);
            BkgndBrush = CreateSolidBrush(BkColor);
        }

        COLORREF oldBkColor = SetBkColor(dc, SG->GetCurrentColor(SALCOL_INACTIVE_CAPTION_BK));
        COLORREF oldTexColor = SetTextColor(dc, SG->GetCurrentColor(SALCOL_INACTIVE_CAPTION_FG));
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect(dc, &r, BkgndBrush);
        r.top++;
        r.bottom--;
        r.left++;
        r.right--;

        // DT_PATH_ELLIPSIS nefunguje na nekterych retezcich, dochazi pak vytisteni oclipovaneho textu
        // PathCompactPath() sice potrebuje kopii do lokalniho bufferu, ale neclipuje texty
        char buff[2 * MAX_PATH];
        strncpy_s(buff, _countof(buff), Text, _TRUNCATE);
        PathCompactPath(dc, buff, r.right - r.left);

        DrawText(dc, buff, -1, &r, /*DT_PATH_ELLIPSIS | */ DT_SINGLELINE | DT_NOPREFIX);
        SetBkColor(dc, oldBkColor);
        SetTextColor(dc, oldTexColor);
        SelectObject(dc, oldFont);
        EndPaint(HWindow, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CSplitBarWindow
//

const char* SPLITBARWINDOW_CLASSNAME = "SFC Split Bar Class";

CSplitBarWindow::CSplitBarWindow(CSplitBarType type)
{
    CALL_STACK_MESSAGE_NONE
    Type = type;
    Tracking = FALSE;
    SplitProp = 0.5;
}

LRESULT
CSplitBarWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CSplitBarWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                             wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        ToolTip = new CToolTipWindow();
        if (!ToolTip)
            return Error(HWND(NULL), IDS_LOWMEM);
        if (!ToolTip->Create(CWINDOW_CLASSNAME,
                             "",
                             WS_POPUP | WS_BORDER,
                             0, 0, 0, 0,
                             GetParent(HWindow), // parent
                             NULL,               // menu
                             DLLInstance,
                             ToolTip))
        {
            TRACE_E("CreateWindowEx has failed; last error: " << GetLastError());
            return -1;
        }

        //CursorHeight = GetSystemMetrics(SM_CYCURSOR);

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        DoubleClick = FALSE;
        Tracking = TRUE;
        SetCapture(HWindow);
        Orig = (sbVertical == Type) ? LOWORD(lParam) : HIWORD(lParam);

        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);

        char buf[20];
        sprintf(buf, "%.1f %%", SplitProp * 100);
        SG->PointToLocalDecimalSeparator(buf, _countof(buf));
        SetWindowText(ToolTip->HWindow, buf);

        POINT p;
        if (sbVertical == Type)
        {
            p.x = 0;
            p.y = (SHORT)HIWORD(lParam);
        }
        else
        {
            p.x = (SHORT)LOWORD(lParam);
            p.y = 0;
        }
        ClientToScreen(HWindow, &p);
        SetWindowPos(ToolTip->HWindow, HWND_TOPMOST, p.x + 5, p.y + 10, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

        return 0;
    }

    case WM_CAPTURECHANGED:
        if (!Tracking)
            return 0;
        // pokracujem dal ...

    case WM_LBUTTONUP:
    {
        ShowWindow(ToolTip->HWindow, SW_HIDE);
        Tracking = FALSE;
        ReleaseCapture();

        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);

        if (DoubleClick)
        {
            SplitProp = 0.5;
            SendMessage(GetParent(HWindow), WM_USER_SLITPOSCHANGED, (WPARAM)&SplitProp, 0);
        }

        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        DoubleClick = TRUE;
        return 0;
    }

    case WM_RBUTTONUP:
    {
        if (Tracking)
            break;

        CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
        if (!menu)
            break;

        MENU_ITEM_INFO mii;
        mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STATE |
                   MENU_MASK_STRING | MENU_MASK_SUBMENU | MENU_MASK_IMAGEINDEX;
        mii.Type = MENU_TYPE_STRING;
        mii.State = 0;
        mii.ImageIndex = -1;
        mii.SubMenu = NULL;

        char buff[20];
        int i;
        for (i = 2; i < 9; i++)
        {
            sprintf(buff, "&%d0 / %d0", i, 10 - i);
            mii.ID = i;
            mii.State = i == 5 ? MENU_STATE_DEFAULT : 0;
            mii.String = buff;
            menu->InsertItem(0xffffffff, TRUE, &mii);
        }

        POINT p;
        p.x = LOWORD(lParam);
        p.y = HIWORD(lParam);
        ClientToScreen(HWindow, &p);
        int cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                              p.x, p.y, HWindow, NULL);
        if (cmd != 0)
        {
            SplitProp = (double)cmd / 10;
            SendMessage(GetParent(HWindow), WM_USER_SLITPOSCHANGED, (WPARAM)&SplitProp, 0);
        }

        SalGUI->DestroyMenuPopup(menu);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if ((wParam | MK_LBUTTON) && Tracking)
        {
            POINT pt;
            RECT r;
            double prop;

            pt.x = (SHORT)LOWORD(lParam);
            pt.y = (SHORT)HIWORD(lParam);
            HWND parent = GetParent(HWindow);
            MapWindowPoints(HWindow, parent, &pt, 1);
            GetClientRect(parent, &r);
            if (sbVertical == Type)
            {
                if (pt.x - Orig < 2)
                    pt.x = 2 + Orig;
                if (pt.x - Orig > r.right - 5)
                    pt.x = r.right - 5 + Orig;
                prop = (double)(pt.x - Orig + 1) / (double)r.right;
            }
            else
            {
                DWORD rebarHeight = (DWORD)SendMessage(GetParent(HWindow), WM_USER_GETREBARHEIGHT, 0, 0);
                DWORD headerHeight = (DWORD)SendMessage(GetParent(HWindow), WM_USER_GETHEADERHEIGHT, 0, 0);

                pt.y -= rebarHeight + headerHeight;
                r.bottom -= rebarHeight + 2 * headerHeight;
                if (pt.y - Orig < 2)
                    pt.y = 2 + Orig;
                if (pt.y - Orig > r.bottom - 5)
                    pt.y = r.bottom - 5 + Orig;
                prop = (double)(pt.y - Orig + 1) / (double)r.bottom;
            }
            if (SplitProp != prop)
            {
                SplitProp = prop;

                SendMessage(parent, WM_USER_SLITPOSCHANGED, (WPARAM)&SplitProp, 0);

                char buf[20];
                sprintf(buf, "%.1f %%", SplitProp * 100);
                SG->PointToLocalDecimalSeparator(buf, _countof(buf));
                SetWindowText(ToolTip->HWindow, buf);
            }

            if (sbVertical == Type)
            {
                pt.x = 0;
                pt.y = (SHORT)HIWORD(lParam);
            }
            else
            {
                pt.x = (SHORT)LOWORD(lParam);
                pt.y = 0;
            }
            char s[21];
            sprintf(s, "%d, %d\n", pt.x, pt.y);
            OutputDebugString(s);
            ClientToScreen(HWindow, &pt);
            SetWindowPos(ToolTip->HWindow, HWND_TOPMOST, pt.x + 5, pt.y + 10, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            return 0;
        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(HWindow, &ps);
        RECT r;
        GetClientRect(HWindow, &r);
        if (Tracking && HDitheredBrush)
        {
            COLORREF oldBk = SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            COLORREF oldText = SetTextColor(dc, GetSysColor(COLOR_3DDKSHADOW));
            FillRect(dc, &r, HDitheredBrush);
            SetBkColor(dc, oldBk);
            SetTextColor(dc, oldText);
        }
        else
            FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        EndPaint(HWindow, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SETCURSOR:
        if (sbHorizontal == Type)
        {
            SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_SPLIT_HOR)));
            return TRUE;
        }
        break;
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CToolTipWindow
//

LRESULT
CToolTipWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CToolTipWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        GetWindowText(HWindow, Text, 10);

        // zmerime delku stringu
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, EnvFont);
        SIZE s;
        GetTextExtentPoint32(hdc, Text, int(strlen(Text)), &s);
        SelectObject(hdc, oldFont);
        ReleaseDC(NULL, hdc);

        SetWindowPos(HWindow, 0, 0, 0, s.cx + 4, EnvFontHeight + 4,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW |
                         SWP_NOREPOSITION | SWP_NOZORDER);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        // podmazeme a soucasne vykreslime text
        HDC hDC = (HDC)wParam;
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect(hDC, &r, (HBRUSH)(COLOR_INFOBK + 1));
        HFONT hOldFont = (HFONT)SelectObject(hDC, EnvFont);
        COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_INFOTEXT));
        COLORREF oldBkColor = SetBkColor(hDC, GetSysColor(COLOR_INFOBK));
        ExtTextOut(hDC, 2, 1, ETO_OPAQUE, &r, Text, TextLen, NULL);
        SetBkColor(hDC, oldBkColor);
        SetTextColor(hDC, oldTextColor);
        SelectObject(hDC, hOldFont);
        return TRUE;
    }

    case WM_PAINT:
    {
        // nedelame nic - vse je zarizeno v WM_ERASEBKGND
        PAINTSTRUCT ps;
        BeginPaint(HWindow, &ps);
        EndPaint(HWindow, &ps);
        return 0;
    }

    case WM_SETTEXT:
    {
        lstrcpyn(Text, (char*)lParam, 10);
        TextLen = int(strlen(Text));

        // zjistime delku textu
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, EnvFont);
        SIZE s;
        GetTextExtentPoint32(hdc, Text, int(strlen(Text)), &s);
        SelectObject(hdc, oldFont);
        ReleaseDC(NULL, hdc);

        // nastavime rozmery okna
        SetWindowPos(HWindow, 0, 0, 0, s.cx + 6, EnvFontHeight + 4,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER |
                         SWP_NOREPOSITION | SWP_NOZORDER);
        InvalidateRect(HWindow, NULL, TRUE);
        UpdateWindow(HWindow);
        return TRUE;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CComboBoxEdit
//

LRESULT
CComboBoxEdit::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CComboBoxEdit::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;

        switch (wParam)
        {
        case VK_RETURN:
        {
            if (!ctrlPressed && !shiftPressed && !altPressed)
            {
                HWND combo = GetParent(HWindow);
                if (SendMessage(combo, CB_GETDROPPEDSTATE, 0, 0))
                {
                    SendMessage(combo, WM_KEYDOWN, VK_RETURN, 0);
                }
            }
            break;
        }

            /*case VK_DOWN:
        {
          TRACE_I("VK_DOWN");
          if (!ctrlPressed && !shiftPressed && altPressed)
          {
            HWND combo = GetParent(HWindow);
            if (SendMessage(combo, CB_GETDROPPEDSTATE, 0, 0))
            {
              SendMessage(combo, CB_SHOWDROPDOWN, TRUE, 0);
              return 0;
            }
          }
          break;
        }
        */
        }
        break;
    }

    case EM_SETSEL:
    {
        if (GetFocus() != HWindow)
        {
            wParam = lParam = 0;
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CComboBox
//

CComboBox::CComboBox()
{
    CALL_STACK_MESSAGE1("CComboBox::CComboBox()");
    BtnFacePen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNFACE));
    BtnShadowPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
    BtnHilightPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNHIGHLIGHT));
    Tracking = FALSE;
}

CComboBox::~CComboBox()
{
    CALL_STACK_MESSAGE1("CComboBox::~CComboBox()");
    if (BtnFacePen)
        DeleteObject(BtnFacePen);
    if (BtnShadowPen)
        DeleteObject(BtnShadowPen);
    if (BtnHilightPen)
        DeleteObject(BtnHilightPen);
}

void CComboBox::ChangeColors()
{
    CALL_STACK_MESSAGE1("CComboBox::ChangeColors()");
    if (BtnFacePen)
        DeleteObject(BtnFacePen);
    if (BtnShadowPen)
        DeleteObject(BtnShadowPen);
    if (BtnHilightPen)
        DeleteObject(BtnHilightPen);
    BtnFacePen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNFACE));
    BtnShadowPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
    BtnHilightPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNHIGHLIGHT));
}

LRESULT
CComboBox::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CComboBox::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                             lParam);
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
    {
        int sbWidth = GetSystemMetrics(SM_CXVSCROLL);
        RECT cr;
        GetClientRect(HWindow, &cr);
        if (!SendMessage(HWindow, CB_GETDROPPEDSTATE, 0, 0) &&
            LOWORD(lParam) > cr.right - 3 - sbWidth &&
            LOWORD(lParam) < cr.right - 2 &&
            HIWORD(lParam) > cr.top + 1 &&
            HIWORD(lParam) < cr.bottom - 2)
        {
            Tracking = TRUE;
        }
        break;
    }

    case WM_CAPTURECHANGED:

    case WM_LBUTTONUP:
    {
        Tracking = FALSE;
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
        SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_PAINT:
    {
        RECT cr;
        GetClientRect(HWindow, &cr);
        // zajistime, aby dvoubodovy ramecek kolem comba nebyl smeten behem paintu
        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = cr.right;
        r.bottom = 2;
        ValidateRect(HWindow, &r);
        r.left = 0;
        r.top = cr.bottom - 2;
        r.right = cr.right;
        r.bottom = cr.bottom;
        ValidateRect(HWindow, &r);
        r.left = 0;
        r.top = 2;
        r.right = 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 2;
        r.top = 2;
        r.right = cr.right;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);

        // nakreslime si vlastni (jeden vnejsi sedivy a vnitrni promackly)
        HDC hDC = GetDC(HWindow);
        HPEN hOldPen = (HPEN)SelectObject(hDC, BtnFacePen);
        SelectObject(hDC, GetStockObject(NULL_BRUSH));
        Rectangle(hDC, cr.left, cr.top, cr.right, cr.bottom);
        SelectObject(hDC, BtnShadowPen);
        MoveToEx(hDC, cr.left + 1, cr.bottom - 2, NULL);
        LineTo(hDC, cr.left + 1, cr.top + 1);
        LineTo(hDC, cr.right - 2, cr.top + 1);
        SelectObject(hDC, BtnHilightPen);
        MoveToEx(hDC, cr.right - 2, cr.top + 1, NULL);
        LineTo(hDC, cr.right - 2, cr.bottom - 2);
        LineTo(hDC, cr.left, cr.bottom - 2);

        if (IsAppThemed())
        {
            SelectObject(hDC, hOldPen);
            ReleaseDC(HWindow, hDC);
            break;
        }

        // zajistime, aby dvoubodovy ramecek kolem tlacitka nebyl smeten behem paintu
        int sbWidth = GetSystemMetrics(SM_CXVSCROLL);
        r.left = cr.right - 2 - sbWidth;
        r.top = 2;
        r.right = cr.right - 2;
        r.bottom = 4;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 2 - sbWidth;
        r.top = cr.bottom - 4;
        r.right = cr.right - 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 2 - sbWidth;
        r.top = 2;
        r.right = cr.right - 2 - sbWidth + 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 4;
        r.top = 2;
        r.right = cr.right - 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);

        // nakreslime vlastni ramecek kolem tlacitka
        HPEN leftTopPen;
        HPEN bottomRightPen;
        POINT pos;
        GetCursorPos(&pos);
        ScreenToClient(HWindow, &pos);
        if (Tracking && pos.x > cr.right - 3 - sbWidth &&
            pos.x < cr.right - 2 &&
            pos.y > cr.top + 1 &&
            pos.y < cr.bottom - 2)
        {
            // nakreslime promackly ramecek
            leftTopPen = BtnShadowPen;
            //bottomRightPen = BtnHilightPen;
            bottomRightPen = BtnShadowPen;
        }
        else
        {
            // nakreslime normalni ramecek
            leftTopPen = BtnHilightPen;
            bottomRightPen = BtnShadowPen;
        }
        SelectObject(hDC, BtnFacePen);
        SelectObject(hDC, GetStockObject(NULL_BRUSH));
        Rectangle(hDC, cr.right - 2 - sbWidth + 1, 3, cr.right - 3, cr.bottom - 3);
        SelectObject(hDC, leftTopPen);
        MoveToEx(hDC, cr.right - 2 - sbWidth, cr.bottom - 4, NULL);
        LineTo(hDC, cr.right - 2 - sbWidth, cr.top + 2);
        LineTo(hDC, cr.right - 3, cr.top + 2);
        SelectObject(hDC, bottomRightPen);
        MoveToEx(hDC, cr.right - 3, cr.top + 2, NULL);
        LineTo(hDC, cr.right - 3, cr.bottom - 3);
        LineTo(hDC, cr.right - 2 - sbWidth - 1, cr.bottom - 3);

        SelectObject(hDC, hOldPen);
        ReleaseDC(HWindow, hDC);

        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CRebar
//

BOOL CRebar::InsertBand(UINT uIndex, LPREBARBANDINFO lprbbi)
{
    CALL_STACK_MESSAGE2("CRebar::InsertBand(0x%X, )", uIndex);

    if (lprbbi->fMask & RBBIM_TEXT)
    {
        // vyhodime prefix
        char buffer[512];
        strcpy(buffer, lprbbi->lpText);
        char* prefix = strchr(buffer, '&');
        if (prefix)
            strcpy(prefix, prefix + 1);

        // zmerime delku stringu
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, EnvFont);
        SIZE s;
        GetTextExtentPoint32(hdc, buffer, int(strlen(buffer)), &s);
        SelectObject(hdc, oldFont);
        ReleaseDC(NULL, hdc);

        lprbbi->fMask |= RBBIM_HEADERSIZE;
        lprbbi->cxHeader = s.cx + 13;
    }
    return SendMessage(HWindow, RB_INSERTBAND, (WPARAM)uIndex, (LPARAM)lprbbi) != 0;
}

BOOL CRebar::GetBandParams(UINT id, CBandParams* params)
{
    CALL_STACK_MESSAGE2("CRebar::GetBandParams(0x%X, )", id);
    LRESULT index = SendMessage(HWindow, RB_IDTOINDEX, id, 0);
    if (index == -1)
    {
        TRACE_E("RB_IDTOINDEX failed");
        return FALSE;
    }
    REBARBANDINFO rbbi;
    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_SIZE | RBBIM_STYLE;
    if (!SendMessage(HWindow, RB_GETBANDINFO, index, (LPARAM)&rbbi))
    {
        TRACE_E("RB_GETBANDINFO failed");
        return FALSE;
    }
    params->Width = rbbi.cx;
    params->Style = rbbi.fStyle & RBBS_BREAK;
    params->Index = int(index);
    return TRUE;
}

BOOL CRebar::SetBandParams(UINT id, CBandParams* params)
{
    CALL_STACK_MESSAGE2("CRebar::SetBandParams(0x%X, )", id);
    LRESULT index = SendMessage(HWindow, RB_IDTOINDEX, id, 0);
    if (index == -1)
    {
        TRACE_E("RB_IDTOINDEX failed");
        return FALSE;
    }
    if (params->Index != index)
    {
        if (!SendMessage(HWindow, RB_MOVEBAND, index, params->Index))
        {
            TRACE_E("RB_MOVEBAND failed");
            return FALSE;
        }
        index = params->Index;
    }
    REBARBANDINFO rbbi;
    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_STYLE;
    if (!SendMessage(HWindow, RB_GETBANDINFO, index, (LPARAM)&rbbi))
    {
        TRACE_E("RB_GETBANDINFO failed");
        return FALSE;
    }
    rbbi.fMask |= RBBIM_SIZE;
    rbbi.fStyle = (rbbi.fStyle & ~RBBS_BREAK) | params->Style;
    rbbi.cx = params->Width;
    if (!SendMessage(HWindow, RB_SETBANDINFO, index, (LPARAM)&rbbi))
    {
        TRACE_E("RB_SETBANDINFO failed");
        return FALSE;
    }
    return TRUE;
}

LRESULT
CRebar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRebar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    switch (uMsg)
    {
    case WM_PAINT:
    {
        int bandCount = int(SendMessage(HWindow, RB_GETBANDCOUNT, 0, 0));
        // zajistime vykresleni podtrzitek u textu s prefixem (napr &Differences)
        int i;
        for (i = 0; i < bandCount; i++)
        {
            char text[512];
            *text = 0;
            REBARBANDINFO rbbi;
            rbbi.cbSize = sizeof(REBARBANDINFO);
            rbbi.fMask = RBBIM_TEXT | RBBIM_HEADERSIZE;
            rbbi.lpText = text;
            rbbi.cch = 512;
            SendMessage(HWindow, RB_GETBANDINFO, i, (LPARAM)&rbbi);

            if (*text)
            {
                RECT r;
                SendMessage(HWindow, RB_GETRECT, i, (LPARAM)&r);
                //SendMessage(HWindow, RB_GETBANDBORDERS, i, (LPARAM)&borders);

                r.left += 9;
                r.right = r.left + rbbi.cxHeader;
                r.top = r.top + (r.bottom - r.top - EnvFontHeight) / 2 - 1;
                r.bottom = r.top + EnvFontHeight + 1;

                // zajistime, aby nas text nebyl smeten behem paintu
                ValidateRect(HWindow, &r);
                r.right -= 13;

                // nakreslime vlastni text
                HDC hdc = GetDC(HWindow);
                HFONT oldFont = (HFONT)SelectObject(hdc, (HFONT)EnvFont);
                SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
                DrawText(hdc, text, -1, &r, DT_SINGLELINE | DT_TOP);

                // linka pod textem
                r.top += EnvFontHeight;
                FillRect(hdc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
                r.top -= EnvFontHeight;

                // plocha za textem
                r.left = r.right;
                r.right += 13;
                FillRect(hdc, &r, (HBRUSH)(COLOR_BTNFACE + 1));

                SelectObject(hdc, oldFont);
                ReleaseDC(HWindow, hdc);
            }
        }

        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        //SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
        SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
        //return 0;
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}
