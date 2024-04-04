// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "tooltip.h"
#include "mainwnd.h"

#define WC_TOOLTIP "SalamanderToolTip"

//*****************************************************************************
//
// CToolTip
//

//replaced by the method GetTime()
//#define TOOLTIP_SHOWDELAY 1000  // [ms] time before opening the tooltip when hovering over an ID from a single window
//#define TOOLTIP_HIDEDELAY   80  // [ms] (times 100) time before the tooltip fades out if nothing else is hovering over it
#define TOOLTIP_KILLDELAY 300 // [ms] how long can we last before transitioning to the Killed mode (for transitioning across separators)

CToolTip* ToolTip = NULL;

CToolTip::CToolTip(CObjectOrigin origin)
    : CWindow(origin)
{
    CALL_STACK_MESSAGE_NONE
    HNotifyWindow = NULL;
    LastID = 0xFFFFFFFF;
    Text[0] = 0;
    TextLen = 0;
    WaitingMode = ttmNone;
    HideCounter = 0;
    IsModal = FALSE;
    ExitASAP = FALSE;
    TimerID = 0;

    LastCursorPos.x = -1;
    LastCursorPos.y = -1;

    if (ToolTip != NULL)
        TRACE_E("ToolTip already exists!");
    else
        ToolTip = this;
}

CToolTip::~CToolTip()
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow != NULL)
    {
        // we will cross the thread boundary and destroy the tooltip in the thread in which it was opened
        SendMessage(HWindow, WM_USER_HIDETOOLTIP, 0, 0);
    }
}

void CToolTip::SuppressToolTipOnCurrentMousePos()
{
    CALL_STACK_MESSAGE_NONE
    GetCursorPos(&LastCursorPos);
}

static BOOL ToolTipRegistred = FALSE;

BOOL CToolTip::RegisterClass()
{
    CALL_STACK_MESSAGE1("CToolTip::Create()");
    if (!ToolTipRegistred)
    {
        if (!CWindow::RegisterUniversalClass(CS_SAVEBITS, 0, 0, NULL,
                                             LoadCursor(NULL, IDC_ARROW), (HBRUSH)NULL,
                                             NULL, WC_TOOLTIP, NULL))
        {
            TRACE_E("CToolTip RegisterUniversalClass failed");
            return FALSE;
        }
        ToolTipRegistred = TRUE;
    }
    return TRUE;
}

BOOL CToolTip::Create(HWND hParent)
{
    CALL_STACK_MESSAGE1("CToolTip::Create()");
    if (!ToolTipRegistred && !RegisterClass())
        return FALSE;

    if (HWindow != NULL)
    {
        if (IsWindowVisible(HWindow)) // if the window is already hidden, we will not warn, because it is the delivery of a deferred message and the window would subsequently be destroyed, see CToolTip::MessageLoop()
            TRACE_E("CToolTip::Create() Tooltip window already exists!");
        // we will cross the thread boundary and turn off the tooltip in the thread where it was opened
        SendMessage(HWindow, WM_USER_HIDETOOLTIP, 0, 0);
    }

    if (hParent == NULL)
        TRACE_E("CToolTip::Create hParent==NULL!");
    CreateEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // extended window style
             WC_TOOLTIP,                       // address of registered class name
             "",                               // address of window name
             WS_POPUP | WS_BORDER,             // window style
             0, 0, 0, 0,
             hParent,   // handle of parent or owner window
             NULL,      // handle of menu or child-window identifier
             HInstance, // handle of application instance
             this);

    if (HWindow == NULL)
    {
        TRACE_E("CToolTip::Create() failed");
        return FALSE;
    }

    return TRUE;
}

DWORD
CToolTip::GetTime(BOOL init)
{
    // times see MSDN/TTM_SETDELAYTIME
    if (init)
        return GetDoubleClickTime();
    else
        return (GetDoubleClickTime() * 10) / 100;
}

// the only way I have found to detect the cursor position
// is drawing the cursor mask into a DIB and then searching the bitmap
BOOL GetCursorHeight(HCURSOR hCursor)
{
    if (hCursor == 0)
        return 0;

    HDC hMemDC = HANDLES(CreateCompatibleDC(NULL));
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = 32;
    bi.bmiHeader.biHeight = 32;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 1; // each line will be represented by 32 bits
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biClrUsed = 0;
    bi.bmiHeader.biClrImportant = 0;
    void* bits;
    HBITMAP hDib = HANDLES(CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0));

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hDib);
    DrawIconEx(hMemDC, 0, 0, hCursor, 0, 0, 0, NULL, DI_MASK | DI_DEFAULTSIZE);
    SelectObject(hMemDC, hOldBitmap);

    GdiFlush(); // Flush the GDI batch, so we can play with the bits

    // bitmap saved from the last scanline towards zero
    int i;
    for (i = 0; i < 32; i++)
    {
        if (*((DWORD*)bits + i) != 0xffffffff)
            break;
    }

    HANDLES(DeleteObject(hDib));

    HANDLES(DeleteDC(hMemDC));

    ICONINFO ii;
    if (GetIconInfo(hCursor, &ii))
    {
        DeleteObject(ii.hbmMask);
        DeleteObject(ii.hbmColor);
    }
    else
        ii.yHotspot = 0;

    return 32 - i - ii.yHotspot;
}

void CToolTip::MessageLoop()
{
    CALL_STACK_MESSAGE1("CToolTip::MessageLoop()");
    SetCapture(HWindow);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    IsModal = TRUE;
    ExitASAP = FALSE;
    MSG msg;
    while (!ExitASAP && GetMessage(&msg, NULL, 0, 0))
    {
        switch (msg.message)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCLBUTTONUP:
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONUP:
        case WM_NCMBUTTONDOWN:
        case WM_NCMBUTTONUP:
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
        case WM_ACTIVATEAPP:
        case WM_ACTIVATE:
        case WM_NCACTIVATE:
        case WM_KILLFOCUS:
        {
            ExitASAP = TRUE;
            break;
        }
        }
        if (!ExitASAP)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (GetCapture() == HWindow)
        ReleaseCapture();

    ShowWindow(HWindow, SW_HIDE);
    IsModal = FALSE;

    // our window was captured, all messages were received by us
    // Now we need to deliver the last message to the recipient
    if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN ||
        msg.message == WM_MBUTTONDOWN)
    {
        HWND hWindow = WindowFromPoint(msg.pt);
        if (hWindow != NULL && hWindow != HWindow &&
            GetWindowThreadProcessId(hWindow, NULL) == GetCurrentThreadId()) // SendMessage(hWindow, WM_NCHITTEST to another thread was causing a deadlock
        {
            POINT p;
            p.x = GET_X_LPARAM(msg.lParam);
            p.y = GET_Y_LPARAM(msg.lParam);
            ClientToScreen(msg.hwnd, &p);
            LRESULT hit = SendMessage(hWindow, WM_NCHITTEST, 0, MAKELPARAM(p.x, p.y));
            if (hit == HTCLIENT)
                ScreenToClient(hWindow, &p);
            else
            {
                switch (msg.message)
                {
                case WM_LBUTTONDOWN:
                    msg.message = WM_NCLBUTTONDOWN;
                    break;
                case WM_RBUTTONDOWN:
                    msg.message = WM_NCRBUTTONDOWN;
                    break;
                case WM_MBUTTONDOWN:
                    msg.message = WM_NCMBUTTONDOWN;
                    break;
                }
                msg.wParam = hit;
            }
            msg.lParam = MAKELPARAM(p.x, p.y);
            msg.hwnd = hWindow;

            // give the opportunity for dialogue to set the default push button
            HWND hDialog = GetTopVisibleParent(hWindow);

            if (hDialog != NULL)
            {
                DWORD pid;
                GetWindowThreadProcessId(hDialog, &pid);
                // we must not deliver the message to another process,
                // otherwise Salamander was falling in USER32.DLL
                if (pid == GetCurrentProcessId())
                {
                    if (hDialog == NULL || !IsDialogMessage(hDialog, &msg))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
        }
    }
    //IsModal = FALSE; // it's too late here, switching between two tooltips doesn't work afterwards, for example Config > Change Drive Menu dialog
    HNotifyWindow = NULL;
    LastID = 0;
    Hide(); // IsModal is FALSE, we can call Hide()
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB)
    {
        HWND hDialog = GetForegroundWindow();
        if (hDialog == NULL || !IsDialogMessage(hDialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

BOOL CToolTip::GetText()
{
    if (HNotifyWindow != NULL)
    {
        Text[0] = 0;
        SendMessage(HNotifyWindow, WM_USER_TTGETTEXT, LastID, (LPARAM)Text);
        TextLen = lstrlen(Text);
    }
    if (TextLen == 0)
    {
        // We did not receive the text - we will turn off the previous tooltip and exit
        if (HWindow != NULL)
            Hide();
        return FALSE;
    }
    return TRUE;
}

void CToolTip::GetNeededWindowSize(SIZE* sz)
{
    HDC hDC = HANDLES(GetDC(HWindow));
    SelectObject(hDC, TooltipFont);
    RECT tR;
    tR.left = 0;
    tR.top = 0;
    tR.right = 0;
    tR.bottom = 0;
    DrawText(hDC, Text, TextLen, &tR, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_EXPANDTABS);
    HANDLES(ReleaseDC(HWindow, hDC));
    sz->cx = tR.right - tR.left;
    sz->cy = tR.bottom - tR.top;
    sz->cx += 3 + 3;
    sz->cy += 2 + 2;
}

BOOL CToolTip::Show(int x, int y, BOOL considerCursor, BOOL modal, HWND hParent)
{
    CALL_STACK_MESSAGE1("CToolTip::Show()");

    // extract text from the window
    TextLen = 0;

    if (GetText())
    {
        // measuring text
        SIZE sz;
        GetNeededWindowSize(&sz);

        int oldY = y;
        if (considerCursor)
            y += GetCursorHeight(GetCursor());
        if (HWindow != NULL)
            Hide(); // hide previous tooltip

        // ensure visibility of the tooltip on the screen
        RECT r, clipRect;
        r.left = x;
        r.right = x + 1;
        r.top = y;
        r.bottom = y + 1;
        MultiMonGetClipRectByRect(&r, &clipRect, NULL);
        int scrW = clipRect.right - clipRect.left;
        int scrH = clipRect.bottom - clipRect.top;
        if (x + sz.cx > clipRect.right)
            x = clipRect.right - sz.cx;
        if (y + sz.cy > clipRect.bottom)
            y = oldY - sz.cy; // place tooltip above the cursor
        if (x < clipRect.left)
            x = 0;
        if (y < clipRect.top)
            y = 0;

        Create(hParent);
        SetWindowPos(HWindow, NULL, x, y, sz.cx, sz.cy, SWP_SHOWWINDOW | SWP_NOACTIVATE);

        if (modal)
        {
            MessageLoop();
        }
    }
    else
        return FALSE;

    return TRUE;
}

void CToolTip::Hide()
{
    CALL_STACK_MESSAGE_NONE
    if (IsModal)
    {
        ExitASAP = TRUE;
    }
    else if (HWindow != NULL)
    {
        // we will cross the thread boundary and destroy the tooltip in the thread in which it was opened
        SendMessage(HWindow, WM_USER_HIDETOOLTIP, 0, 0);
    }
}

VOID CALLBACK ToolTipTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    if (MainWindow != NULL && MainWindow->ToolTip != NULL)
    {
        CToolTip* toolTip = MainWindow->ToolTip;
        //    if (toolTip->HWindow == NULL)
        //      toolTip->Create();

        toolTip->OnTimer();
    }
}

void CToolTip::MySetTimer(DWORD elapse)
{
    if (TimerID != 0)
        KillTimer(NULL, TimerID);
    TimerID = SetTimer(NULL, 0, elapse, ToolTipTimerProc);
}

void CToolTip::MyKillTimer()
{
    if (TimerID != 0)
        KillTimer(NULL, TimerID);
    TimerID = 0;
}

void CToolTip::SetCurrentToolTip(HWND hNotifyWindow, DWORD id, int showDelay)
{
    CALL_STACK_MESSAGE2("CToolTip::SetCurrentToolTip(, 0x%X)", id);
    if (IsModal)
        return; // we do not take this call during the modal tooltip

    HWND hOldNotifyWindow = HNotifyWindow;
    HNotifyWindow = hNotifyWindow;
    DWORD oldLastID = LastID;
    LastID = id;

    // prevent unwanted tooltip display when switching to window
    POINT currentPos;
    GetCursorPos(&currentPos);
    BOOL sameCursorPos = (currentPos.x == LastCursorPos.x && currentPos.y == LastCursorPos.y);
    LastCursorPos = currentPos;

    if (HNotifyWindow == NULL)
    {
        if (WaitingMode != ttmNone)
        {
            WaitingMode = ttmNone;
            MyKillTimer();
            Hide();
        }
        return;
    }
    if (hOldNotifyWindow != HNotifyWindow)
    {
        // the window has changed - I will shoot down the timer and turn off the lights
        if (WaitingMode != ttmNone)
        {
            WaitingMode = ttmNone;
            MyKillTimer();
            Hide();
        }
    }

    if (sameCursorPos)
        return;

    if (HWindow != NULL || WaitingMode == ttmWaitingKill)
    {
        if (HNotifyWindow == hOldNotifyWindow && LastID == oldLastID)
            return;
        DWORD pos = GetMessagePos();
        if (Show(GET_X_LPARAM(pos), GET_Y_LPARAM(pos), TRUE, FALSE, HNotifyWindow))
        {
            // if the text was displayed successfully, we will start waiting for it to turn off
            WaitingMode = ttmWaitingClose;
            MySetTimer(GetTime(FALSE));
            HideCounter = 0;
        }
        else
        {
            if (WaitingMode != ttmWaitingKill)
            {
                // text was not provided - we will start waiting for KILL
                WaitingMode = ttmWaitingKill;
                MySetTimer(TOOLTIP_KILLDELAY);
            }
        }
    }
    else
    {
        if (HNotifyWindow == hOldNotifyWindow && LastID == oldLastID && WaitingMode == ttmNone)
            return;
        // otherwise we will start (or reset) the timer
        if (showDelay >= 0)
        {
            if (showDelay == 0)
                showDelay = GetTime(TRUE);
            WaitingMode = ttmWaitingOpen;
            MySetTimer(showDelay);
        }
    }
}

BOOL HasActiveParent(HWND hWindow)
{
    CALL_STACK_MESSAGE_NONE
    HWND hIterator = hWindow;
    for (;;)
    {
        HWND hParent = GetParent(hIterator);
        if (hParent == NULL)
            return GetActiveWindow() == hIterator;
        if (hParent == GetForegroundWindow())
            return TRUE;
        hIterator = hParent;
    }
}

void CToolTip::OnTimer()
{
    CALL_STACK_MESSAGE1("CToolTip::OnTimer()");
    if (WaitingMode != ttmWaitingClose)
        MyKillTimer();
    switch (WaitingMode)
    {
    case ttmNone:
    {
        // how is it possible that the timer arrived to us when according to the state variable it should not be running
        TRACE_E("WaitingMode == ttmNone");
        break;
    }

    case ttmWaitingOpen:
    {
        WaitingMode = ttmNone;
        POINT p;
        GetCursorPos(&p);
        HWND hWnd = WindowFromPoint(p);
        if (hWnd == HNotifyWindow) // we must always be on the notify window
        {
            if (HasActiveParent(hWnd)) // and its root must be active
            {
                if (Show(p.x, p.y, TRUE, FALSE, HNotifyWindow))
                {
                    // if the text was displayed successfully, we will start waiting for it to turn off
                    WaitingMode = ttmWaitingClose;
                    MySetTimer(GetTime(FALSE));
                    HideCounter = 0;
                }
                else
                {
                    // text was not provided - we will start waiting for KILL
                    WaitingMode = ttmWaitingKill;
                    MySetTimer(TOOLTIP_KILLDELAY);
                }
            }
        }
        else
        {
            HNotifyWindow = NULL;
            LastID = 0;
        }
        break;
    }

    case ttmWaitingClose:
    {
        // Check if it is necessary to turn off the tooltip
        POINT p;
        GetCursorPos(&p);
        HWND hWnd = WindowFromPoint(p);
        if (HideCounter == 0)
            HideCounterMax = max(100, (TextLen * 10) / 4);
        if (hWnd != HNotifyWindow || HideCounter == HideCounterMax)
        {
            Hide();
            // if it was about turning off due to a timeout, I will let the timer continue running
            // for the time until the mouse leaves the window or the SetCurrentToolTip function is called
            if (hWnd != HNotifyWindow)
            {
                WaitingMode = ttmNone;
                MyKillTimer();
                POINT p2;
                GetCursorPos(&p2);
                if (WindowFromPoint(p2) != HNotifyWindow)
                {
                    HNotifyWindow = NULL;
                    LastID = 0;
                }
            }
        }
        else
            HideCounter++;
        break;
    }

    case ttmWaitingKill:
    {
        WaitingMode = ttmNone;
        break;
    }

    default:
    {
        TRACE_E("Unknown waiting mode=" << WaitingMode);
        break;
    }
    }
}

LRESULT
CToolTip::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CToolTip::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    switch (uMsg)
    {
    case WM_USER_REFRESHTOOLTIP:
    {
        // we need to keep the existing window -- just stretch it for the new text
        if (GetText())
        {
            SIZE sz;
            GetNeededWindowSize(&sz); // Ignore the treatment of screen recovery
            SetWindowPos(HWindow, NULL, 0, 0, sz.cx, sz.cy, SWP_NOACTIVATE | SWP_NOMOVE);
            InvalidateRect(HWindow, NULL, TRUE);
            UpdateWindow(HWindow);
        }
        return 0;
    }

    case WM_ERASEBKGND:
    {
        // we will lubricate and simultaneously render text
        HDC hDC = (HDC)wParam;
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect(hDC, &r, (HBRUSH)(COLOR_INFOBK + 1));
        HFONT hOldFont = (HFONT)SelectObject(hDC, TooltipFont);
        COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_INFOTEXT));
        int oldBkMode = SetBkMode(hDC, TRANSPARENT);
        r.left += 2;
        r.top += 1;
        DrawText(hDC, Text, TextLen, &r, DT_LEFT | DT_NOPREFIX | DT_NOCLIP | DT_EXPANDTABS);
        SetBkMode(hDC, oldBkMode);
        SetTextColor(hDC, oldTextColor);
        SelectObject(hDC, hOldFont);
        return TRUE;
    }

    case WM_PAINT:
    {
        // we are not doing anything - everything is handled in WM_ERASEBKGND
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_USER_HIDETOOLTIP:
    {
        DestroyWindow(HWindow);
        return 0;
    }

    case WM_DESTROY:
    {
        if (WaitingMode != ttmNone)
        {
            MyKillTimer();
            WaitingMode = ttmNone;
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
