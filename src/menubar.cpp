// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "menu.h"
#include "mainwnd.h"

//*****************************************************************************
//
// CMenuBar
//

#define MENUBAR_LR_MARGIN 8 // number of points before and after the text, including the vertical line
#define MENUBAR_TB_MARGIN 4 // number of points above and below the text, including the horizontal line

CMenuBar::CMenuBar(CMenuPopup* menu, HWND hNotifyWindow, CObjectOrigin origin)
    : CWindow(origin)
{
    CALL_STACK_MESSAGE_NONE
    Menu = menu;
    HNotifyWindow = hNotifyWindow;
    Width = 0;
    Height = 0;
    HFont = NULL;
    FontHeight = 0;
    HotIndex = -1;
    MenuLoop = FALSE;
    RetValue = 0;
    IndexToOpen = -1;
    OpenWithSelect = FALSE;
    OpenByMouse = FALSE;
    ExitMenuLoop = FALSE;
    HelpMode2 = FALSE;
    DispatchDelayedMsg = FALSE;
    HotIndexIsTracked = FALSE;
    HandlingVK_MENU = FALSE;
    WheelDuringMenu = FALSE;
    Closing = FALSE;
    MouseIsTracked = FALSE;
    HelpMode = FALSE;

    UIState = 0;
    BOOL alwaysVisible;
    if (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &alwaysVisible, FALSE) == 0)
        alwaysVisible = TRUE;
    if (!alwaysVisible)
        UIState = UISF_HIDEACCEL | UISF_HIDEFOCUS;
    ForceAccelVisible = FALSE;

    HCloseEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual-reset event in the non-signaled state
    SetFont();
}

CMenuBar::~CMenuBar()
{
    CALL_STACK_MESSAGE1("CMenuBar::~CMenuBar()");
    if (HFont != NULL)
        HANDLES(DeleteObject(HFont));
    if (HCloseEvent != NULL)
        HANDLES(CloseHandle(HCloseEvent));
}

BOOL CMenuBar::CreateWnd(HWND hParent)
{
    CALL_STACK_MESSAGE1("CMenuBar::CreateWnd()");
    if (HWindow != NULL)
    {
        TRACE_E("HWindow != NULL");
        return TRUE;
    }

    Create(CWINDOW_CLASSNAME2,
           NULL,
           WS_CHILD | WS_CLIPSIBLINGS,
           0, 0, 0, 0, // dummy
           hParent,
           (HMENU)0,
           HInstance,
           this);

    if (HWindow == NULL)
    {
        TRACE_E("CMenuBar::CreateWnd failed.");
        return FALSE;
    }
    RefreshMinWidths();
    return TRUE;
}

void CMenuBar::SetFont()
{
    CALL_STACK_MESSAGE1("CMenuBar::SetFont()");
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);

    LOGFONT* lf = &ncm.lfMenuFont;
    if (HFont != NULL)
        HANDLES(DeleteObject(HFont));
    HFont = HANDLES(CreateFontIndirect(lf));

    HDC hDC = HANDLES(GetDC(NULL));
    TEXTMETRIC tm;
    HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);
    GetTextMetrics(hDC, &tm);
    FontHeight = tm.tmHeight;
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(NULL, hDC));
    RefreshMinWidths();
}

int CMenuBar::GetNeededWidth()
{
    CALL_STACK_MESSAGE_NONE
    int width = 0;
    int i;
    for (i = 0; i < Menu->Items.Count; i++)
        width += MENUBAR_LR_MARGIN + Menu->Items[i]->MinWidth + MENUBAR_LR_MARGIN;
    return width;
}

int CMenuBar::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    return MENUBAR_TB_MARGIN + FontHeight + MENUBAR_TB_MARGIN;
}

BOOL CMenuBar::GetItemRect(int index, RECT& r)
{
    CALL_STACK_MESSAGE_NONE
    if (index >= Menu->Items.Count)
    {
        TRACE_E("Index is out of range. Index=" << index);
        return FALSE;
    }
    int x = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        int textWidth = Menu->Items[i]->MinWidth;
        x += MENUBAR_LR_MARGIN + textWidth + MENUBAR_LR_MARGIN;
    }
    POINT p;
    p.x = 0;
    p.y = 0;
    ClientToScreen(HWindow, &p);
    r.left = p.x + x;
    r.top = p.y + 1;
    r.right = p.x + x + MENUBAR_LR_MARGIN + Menu->Items[index]->MinWidth + MENUBAR_LR_MARGIN;
    r.bottom = p.y + Height - 1;
    return TRUE;
}

void CMenuBar::DrawItem(HDC hDC, int index, int x)
{
    CALL_STACK_MESSAGE_NONE
    const CMenuItem* item = Menu->Items[index];

    RECT r;
    r.left = x;
    r.right = x + MENUBAR_LR_MARGIN + item->MinWidth + MENUBAR_LR_MARGIN;
    r.top = 1;
    r.bottom = Height - 1;

    // output the text
    const char* string = item->String;
    int stringLen = item->ColumnL1Len;

    // fill the stripe above and below (for Windows Vista rebar)
    RECT r2 = r;
    r2.top = 0;
    r2.bottom = 1;
    FillRect(hDC, &r2, (HBRUSH)(COLOR_BTNFACE + 1));
    r2.top = Height - 1;
    r2.bottom = Height;
    FillRect(hDC, &r2, (HBRUSH)(COLOR_BTNFACE + 1));

    int bkColor = (HotIndex == index && !Closing) ? COLOR_HIGHLIGHT : COLOR_BTNFACE;
    int textColor = (HotIndex == index && !Closing) ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT;
    FillRect(hDC, &r, (HBRUSH)(UINT_PTR)(bkColor + 1));

    r.top += MENUBAR_TB_MARGIN - 1;
    r.left += MENUBAR_LR_MARGIN;
    SetTextColor(hDC, GetSysColor(textColor));

    // NOTE: Since Windows Vista Microsoft broke something in the rebar. Resizing
    // the window leads to redrawing all bands, as a result, the entire menu is redrawn and sometimes flickers
    // (older OSes only redrew what was needed). The problem could be mitigated by caching, but
    // a better solution is rewriting the rebar and avoiding the broken MS implementation.
    DWORD dtFlags = DT_LEFT | DT_SINGLELINE | DT_NOCLIP;
    if ((UIState & UISF_HIDEACCEL) && !ForceAccelVisible)
        dtFlags |= DT_HIDEPREFIX;
    DrawText(hDC, string, stringLen, &r, dtFlags);

    //  TRACE_I("DrawText "<<string<<" selected:"<< (HotIndex == index && !Closing));
}

void CMenuBar::DrawItem(int index)
{
    CALL_STACK_MESSAGE2("CMenuBar::DrawItem(%d)", index);

    HDC hDC = HANDLES(GetDC(HWindow));
    HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);
    int oldBkMode = SetBkMode(hDC, TRANSPARENT);
    COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
    int x = 0;
    int i;
    for (i = 0; i < index; i++)
        x += MENUBAR_LR_MARGIN + Menu->Items[i]->MinWidth + MENUBAR_LR_MARGIN;
    DrawItem(hDC, index, x);
    SetTextColor(hDC, oldTextColor);
    SetBkMode(hDC, oldBkMode);
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(HWindow, hDC));
}

void CMenuBar::DrawAllItems(HDC hDC)
{
    CALL_STACK_MESSAGE1("CMenuBar::DrawAllItems()");
    HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);
    int oldBkMode = SetBkMode(hDC, TRANSPARENT);
    COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_BTNTEXT));
    int x = 0;
    int i;
    for (i = 0; i < Menu->Items.Count; i++)
    {
        int textWidth = Menu->Items[i]->MinWidth;
        DrawItem(hDC, i, x);
        x += MENUBAR_LR_MARGIN + textWidth + MENUBAR_LR_MARGIN;
    }
    SetTextColor(hDC, oldTextColor);
    SetBkMode(hDC, oldBkMode);
    SelectObject(hDC, hOldFont);

    // erase the remaining area
    RECT r;
    GetClientRect(HWindow, &r);
    r.left = x;
    FillRect((HDC)hDC, &r, HDialogBrush);
}

void CMenuBar::RefreshMinWidths()
{
    CALL_STACK_MESSAGE1("CMenuBar::RefreshMinWidths()");
    HDC hDC = HANDLES(GetDC(HWindow));
    HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);
    RECT r;
    int i;
    for (i = 0; i < Menu->Items.Count; i++)
    {
        CMenuItem* item = Menu->Items[i];
        r.left = 0;
        r.top = 0;
        r.right = 0;
        r.bottom = 0;
        DrawText(hDC, item->String, item->ColumnL1Len,
                 &r, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
        item->MinWidth = r.right;
    }
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(HWindow, hDC));
}

BOOL CMenuBar::HitTest(int xPos, int yPos, int& index)
{
    CALL_STACK_MESSAGE_NONE
    if (yPos >= 1 && yPos < Height - 1)
    {
        int left = 0;
        int right;
        int i;
        for (i = 0; i < Menu->Items.Count; i++)
        {
            right = left + MENUBAR_LR_MARGIN + Menu->Items[i]->MinWidth + MENUBAR_LR_MARGIN;
            if (xPos >= left && xPos < right)
            {
                index = i;
                return TRUE;
            }
            left = right;
        }
    }
    return FALSE;
}

void CMenuBar::EnterMenu()
{
    CALL_STACK_MESSAGE1("CMenuBar::EnterMenu()");
    //  test crash, it can be triggered by pressing the Alt key
    //  int a = 0;
    //  printf("%d", 5 / a);
    EnterMenuInternal(-1, FALSE, FALSE); // user pressed VK_MENU
}

void CMenuBar::EnterMenuInternal(int index, BOOL openWidthSelect, BOOL byMouse)
{
    CALL_STACK_MESSAGE4("CMenuBar::EnterMenuInternal(%d, %d, %d)", index,
                        openWidthSelect, byMouse);
    if (MenuLoop)
    {
        TRACE_E("Recursive call to CMenuBar::EnterMenuInternal");
        return;
    }
    //  TRACE_I("CMenuBar::EnterMenuInternal begin");
    MenuLoop = TRUE;
    Closing = FALSE;
    ResetEvent(HCloseEvent);
    RetValue = 0;
    BOOL leaveLoop = FALSE;
    ExitMenuLoop = FALSE; // will be set externally from TrackPopup
    DispatchDelayedMsg = FALSE;

    // hook this thread
    HHOOK hOldHookProc = OldMenuHookTlsAllocator.HookThread();
    // add ourselves to the monitoring of closing messages
    MenuWindowQueue.Add(HWindow);

    if (GetCapture() != NULL)
        ReleaseCapture();

    SendMessage(HNotifyWindow, WM_USER_ENTERMENULOOP, 0, 0);

    OpenWithSelect = openWidthSelect;
    OpenByMouse = byMouse;
    if (HotIndex != -1 && HotIndex != index)
    {
        int oldHotIndex = HotIndex;
        HotIndex = -1;
        DrawItem(oldHotIndex);
    }
    if (index == -1)
    {
        HotIndex = 0;
        DrawItem(HotIndex);
    }
    else
    {
        if (HotIndex != index)
        {
            HotIndex = index;
            DrawItem(HotIndex);
        }
        //    TRACE_I("XXX TrackHotIndex();");
        TrackHotIndex(); // the expanded menu will take capture
        if (ExitMenuLoop)
            leaveLoop = TRUE;
    }

    MSG msg;
    while (!leaveLoop)
    {
        if (Closing)
        {
            leaveLoop = TRUE;
            continue;
        }
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
        {
            //      TRACE_I("MenuLoop msg=0x"<<hex<<msg.message<<" wParam=0x"<<msg.wParam<<" lParam=0x"<<msg.lParam);
            switch (msg.message)
            {
            case WM_USER_CLOSEMENU:
            {
                leaveLoop = TRUE;
                continue;
            }

            case WM_CHAR:
            {
                if (!HotIndexIsTracked)
                {
                    BOOL found = HotKeyIndexLookup((char)msg.wParam, index);
                    if (found)
                    {
                        if (index != HotIndex)
                        {
                            // remove the current item
                            int oldHotIndex = HotIndex;
                            HotIndex = index;
                            DrawItem(oldHotIndex);
                            DrawItem(HotIndex);
                        }
                        OpenWithSelect = TRUE;
                        OpenByMouse = FALSE;
                        TrackHotIndex();
                        if (ExitMenuLoop)
                            leaveLoop = TRUE;
                    }
                }
                continue;
            }

            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            {
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (msg.wParam == VK_MENU && (msg.lParam & 0x40000000) == 0 || // Alt down, but not an autorepeat
                    (!shiftPressed && msg.wParam == VK_F10))
                {
                    leaveLoop = TRUE;
                    break; // let the message be delivered, so that on receiving Up, we don’t enter the system menu
                }

                switch (msg.wParam)
                {
                case VK_ESCAPE:
                {
                    leaveLoop = TRUE;
                    break;
                }

                case VK_LEFT:
                case VK_RIGHT:
                {
                    // cyclically move the HotIndex item
                    int newHotIndex = HotIndex;
                    if (newHotIndex == -1)
                        newHotIndex = 0;
                    if (msg.wParam == VK_LEFT)
                    {
                        newHotIndex--;
                        if (newHotIndex < 0)
                            newHotIndex = Menu->Items.Count - 1;
                    }
                    else
                    {
                        newHotIndex++;
                        if (newHotIndex >= Menu->Items.Count)
                            newHotIndex = 0;
                    }
                    if (newHotIndex != HotIndex)
                    {
                        int oldHotIndex = HotIndex;
                        HotIndex = newHotIndex;
                        DrawItem(oldHotIndex);
                        DrawItem(HotIndex);
                    }
                    break;
                }

                case VK_UP:
                case VK_DOWN:
                case VK_RETURN:
                {
                    // expand the menu
                    if (HotIndex != -1)
                    {
                        OpenWithSelect = TRUE; // we want to select the item
                        OpenByMouse = FALSE;
                        TrackHotIndex();
                        if (ExitMenuLoop)
                            leaveLoop = TRUE;
                    }
                }
                }
                msg.hwnd = HWindow; // redirect to us (so keystrokes do not leak out)
                // patch: we must not let WM_CHAR to be generated which would reach the menu owner
                // and cause a beep (Find dialog)
                if (msg.message != WM_KEYDOWN || msg.wParam != VK_ESCAPE)
                    TranslateMessage(&msg);
                continue;
            }

            case WM_MOUSEMOVE:
            {
                HWND hWndUnderCursor = WindowFromPoint(msg.pt);
                if (hWndUnderCursor == HWindow)
                    break;
                continue;
            }

            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
            {
                HWND hWndUnderCursor = WindowFromPoint(msg.pt);
                if (hWndUnderCursor == HWindow)
                {
                    int xPos = (short)LOWORD(msg.lParam);
                    int yPos = (short)HIWORD(msg.lParam);
                    int index2;
                    BOOL hitItem = HitTest(xPos, yPos, index2);
                    if (hitItem)
                    {
                        if (index2 != HotIndex)
                        {
                            // remove the current item
                            int oldHotIndex = HotIndex;
                            HotIndex = index2;
                            DrawItem(oldHotIndex);
                        }
                        OpenWithSelect = FALSE; // invoked by mouse click - do not select the first item
                        OpenByMouse = TRUE;
                        TrackHotIndex();
                        if (ExitMenuLoop)
                            leaveLoop = TRUE;
                        continue;
                    }
                }
            }

            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_NCLBUTTONDOWN:
            case WM_NCRBUTTONDOWN:
            case WM_NCMBUTTONDOWN:
            case WM_NCLBUTTONUP:
            case WM_NCRBUTTONUP:
            case WM_NCMBUTTONUP:
            {
                leaveLoop = TRUE;
                DelayedMsg = msg;
                DispatchDelayedMsg = TRUE;
                continue;
            }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // no message is in the queue - wait until one arrives
            DWORD ret = MsgWaitForMultipleObjects(1, &HCloseEvent, FALSE, INFINITE, QS_ALLINPUT);
            if (ret == 0xFFFFFFFF)
                TRACE_E("MsgWaitForMultipleObjects failed");
            /*
      if (ret == WAIT_OBJECT_0 + 0)
        TRACE_I("Event set");
      if (ret == WAIT_OBJECT_0 + 1)
        TRACE_I("Event set 2");
      if (ret == WAIT_ABANDONED_0 + 0)
        TRACE_I("Event set 3");
*/
        }
    }

    if (HotIndex != -1)
    {
        int oldHotIndex = HotIndex;
        HotIndex = -1;
        DrawItem(oldHotIndex);
    }

    // should we hide accelerators?
    if (ForceAccelVisible)
    {
        ForceAccelVisible = FALSE;
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
    }

    // remove ourselves from monitoring closing messages
    MenuWindowQueue.Remove(HWindow);
    // if we hooked, we will also unhook
    if (hOldHookProc != NULL)
        OldMenuHookTlsAllocator.UnhookThread(hOldHookProc);

    MenuLoop = FALSE;

    if (MouseIsTracked)
        WindowProc(WM_MOUSELEAVE, 0, 0);

    SendMessage(HNotifyWindow, WM_USER_LEAVEMENULOOP, 0, 0);

    // result delivery is incoming - the same thing is handled in CMenuPopup::Track

    // simulate mouse movement so other windows can capture it
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND hWndUnderCursor = WindowFromPoint(cursorPos);
    if (hWndUnderCursor != NULL)
    {
        ScreenToClient(hWndUnderCursor, &cursorPos);
        // j.r. until RC1 SendMessage was used here but it misbehaved with EroiicaViewer
        // if the user displayed a PDF and placed the cursor over the viewer/salamander window
        // and switched between EV/SS windows using Alt+Tab, the EV window sometimes paused for 2s
        // before its content was drawn.
        // Let's try switching to PostMessage; the functionality seems the same.
        PostMessage(hWndUnderCursor, WM_MOUSEMOVE, 0, MAKELPARAM(cursorPos.x, cursorPos.y));
    }

    // we willdeliver the delayed message
    if (DispatchDelayedMsg)
    {
        TranslateMessage(&DelayedMsg);
        PostMessage(DelayedMsg.hwnd, DelayedMsg.message, DelayedMsg.wParam, DelayedMsg.lParam);
    }

    // return the command
    if (RetValue != 0)
        PostMessage(HNotifyWindow, WM_COMMAND, RetValue, 0);

    PostMessage(HNotifyWindow, WM_USER_LEAVEMENULOOP2, 0, 0);
    Closing = FALSE;
    //  TRACE_I("CMenuBar::EnterMenuInternal end");
}

void CMenuBar::TrackHotIndex()
{
    CALL_STACK_MESSAGE1("CMenuBar::TrackHotIndex()");
    //  TRACE_I("CMenuBar::TrackHotIndex begin");
    CMenuPopup* popup;
    do
    {
        popup = Menu->Items[HotIndex]->SubMenu;

        RECT itemRect;
        GetItemRect(HotIndex, itemRect);

        //    DrawItem(HotIndex);

        IndexToOpen = -1; // will be set in TrackInternal
        DWORD trackFlags = MENU_TRACK_VERTICAL;
        if (OpenWithSelect)
        {
            popup->SetSelectedItemIndex(0);
            trackFlags |= MENU_TRACK_SELECT;
        }
        popup->SelectedByMouse = OpenByMouse;

        if (!ForceAccelVisible)
            trackFlags |= MENU_TRACK_HIDEACCEL;

        HotIndexIsTracked = TRUE;

        popup->SetSkillLevel(Menu->SkillLevel);
        RetValue = popup->TrackInternal(trackFlags, itemRect.left, itemRect.bottom, HNotifyWindow,
                                        &itemRect, this, DelayedMsg, DispatchDelayedMsg);

        // store the mouse position (if it is over us)
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        HWND hWndUnderCursor = WindowFromPoint(cursorPos);
        if (hWndUnderCursor == HWindow)
        {
            ScreenToClient(HWindow, &cursorPos);
            LastMouseMove = cursorPos;
        }

        HotIndexIsTracked = FALSE;
        if (IndexToOpen != -1 && HotIndex != IndexToOpen)
        {
            int oldHotIndex = HotIndex;
            HotIndex = IndexToOpen;
            DrawItem(oldHotIndex);
            DrawItem(HotIndex);
        }
    } while (IndexToOpen != -1);
    int oldHotIndex = HotIndex;
    if (!MenuLoop)
        HotIndex = -1; // completely clear the hot item
    //  DrawItem(oldHotIndex);
    //  TRACE_I("CMenuBar::TrackHotIndex end");
}

BOOL CMenuBar::HotKeyIndexLookup(char hotKey, int& itemIndex)
{
    CALL_STACK_MESSAGE3("CMenuBar::HotKeyIndexLookup(%u, %d)", hotKey, itemIndex);
    int i;
    for (i = 0; i < Menu->Items.Count; i++)
    {
        const char* found = NULL;
        const char* s = Menu->Items[i]->String;
        while (*s != 0)
        {
            if (*s == '&')
            {
                if (*(s + 1) == '&')
                {
                    s += 2;
                    continue;
                }
                if (*(s + 1) != 0)
                    found = s + 1;
                break;
            }
            s++;
        }
        if (found != NULL && UpperCase[*found] == UpperCase[hotKey])
        {
            itemIndex = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CMenuBar::IsMenuBarMessage(CONST MSG* lpMsg)
{
    SLOW_CALL_STACK_MESSAGE4("CMenuBar::IsMenuBarMessage(0x%X, 0x%IX, 0x%IX)", lpMsg->message, lpMsg->wParam, lpMsg->lParam);
    switch (lpMsg->message)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        LPARAM wParam = lpMsg->wParam;
        if (wParam == VK_MENU)
        {
            if ((lpMsg->lParam & 0x40000000) == 0) // if this is not an auto repeat
            {
                HandlingVK_MENU = TRUE;
                // check whether to show accelerators
                if (UIState & UISF_HIDEACCEL)
                {
                    ForceAccelVisible = TRUE;
                    InvalidateRect(HWindow, NULL, FALSE);
                    UpdateWindow(HWindow);
                }
            }
        }
        else
        {
            WheelDuringMenu = FALSE; // if the user scrolled the wheel and then typed a number (Alt+numXXX), we will ignore the scrolling
            HandlingVK_MENU = FALSE;
        }
        if (wParam == VK_F10)
        {
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (!shiftPressed && !altPressed && !ctrlPressed)
            {
                // check whether to show accelerators
                if (UIState & UISF_HIDEACCEL)
                {
                    ForceAccelVisible = TRUE;
                    InvalidateRect(HWindow, NULL, FALSE);
                    UpdateWindow(HWindow);
                }

                EnterMenu();
                return TRUE;
            }
        }
        break;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        if (lpMsg->wParam == VK_MENU)
        {
            if (HandlingVK_MENU)
            {
                HandlingVK_MENU = FALSE;
                EnterMenu();
                //          return TRUE;
            }
            if (WheelDuringMenu)
            {
                WheelDuringMenu = FALSE;
                return TRUE; // suppress releasing Alt after Alt+Wheel, otherwise the window menu opens (without being shown)
            }
            // if the user did not use the wheel, we must not process the message
            // Alt+num064 (etc.) is used for inserting characters
        }
        else
        {
            if (HandlingVK_MENU)
                HandlingVK_MENU = FALSE;
        }
        break;
    }

    case WM_SYSCHAR:
    {
        int index;
        BOOL found = HotKeyIndexLookup((char)lpMsg->wParam, index);
        if (found)
        {
            if ((UIState & UISF_HIDEACCEL) && !ForceAccelVisible)
            {
                // if the menu bar was activated by the mouse, now is the time to set ForceAccelVisible so
                // the underline propagates to opened submenus
                ForceAccelVisible = TRUE;
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateWindow(HWindow);
            }
            EnterMenuInternal(index, TRUE, FALSE);
            return TRUE;
        }
        break;
    }

    case WM_USER_MOUSEWHEEL:
    {
        WheelDuringMenu = TRUE;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    {
        HandlingVK_MENU = FALSE;
        break;
    }
    }
    return FALSE;
}

LRESULT
CMenuBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CMenuBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_USER_CLOSEMENU:
    {
        //      TRACE_I("WM_USER_CLOSEMENU");
        Closing = TRUE;
        if (HotIndex != -1)
            DrawItem(HotIndex); // remove HotIndex highlight (Closing == TRUE)
        SetEvent(HCloseEvent);  // let the message queue run
        return 0;
    }

    case WM_ERASEBKGND:
    {
        if (WindowsVistaAndLater) // under Vista the rebar flickers
            return TRUE;
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect((HDC)wParam, &r, HDialogBrush);
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        DrawAllItems(hDC);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_UPDATEUISTATE:
    {
        // we send it to ourselves from the main window
        if (LOWORD(wParam) == UIS_CLEAR)
            UIState &= ~HIWORD(wParam);
        else if (LOWORD(wParam) == UIS_SET)
            UIState |= HIWORD(wParam);

        InvalidateRect(HWindow, NULL, FALSE); // using cached bitmap so it does not flicker
        UpdateWindow(HWindow);
        return 0;
    }

    case WM_SIZE:
    {
        if (HWindow != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            Width = r.right;
            Height = r.bottom;
        }
        break;
    }

    case WM_USER_HELP_MOUSELEAVE:
    {
        HelpMode2 = FALSE;
    }
    case WM_MOUSELEAVE:
    case WM_CANCELMODE:
    {
        MouseIsTracked = FALSE;
        if (!HelpMode2 && !MenuLoop && HotIndex != -1)
        {
            int oldHotIndex = HotIndex;
            HotIndex = -1;
            DrawItem(oldHotIndex);
        }
        break;
    }

    case WM_USER_HELP_MOUSEMOVE:
    {
        HelpMode2 = TRUE;
    }
    case WM_MOUSEMOVE:
    {
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);
        if (MenuLoop)
        {
            // prevents conflicts between cursor movement and Left/Right clicks
            if (LastMouseMove.x == xPos && LastMouseMove.y == yPos)
                break;
        }
        LastMouseMove.x = xPos;
        LastMouseMove.y = yPos;
        if (!HotIndexIsTracked)
        {
            int index = HotIndex;
            BOOL hitItem = HitTest(xPos, yPos, index);
            int newHotIndex = HotIndex;
            if (hitItem)
            {
                if (!HelpMode2 && !MenuLoop && !MouseIsTracked) // capture is needed only in the track mode
                {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = HWindow;
                    TrackMouseEvent(&tme);
                    MouseIsTracked = TRUE;
                }
                newHotIndex = index;
            }
            else
            {
                if (!MenuLoop) // if we are in MenuLoop, keep the last selected item highlighted
                {
                    newHotIndex = -1;
                    if (MouseIsTracked)
                        WindowProc(WM_MOUSELEAVE, 0, 0);
                }
            }
            if (newHotIndex != HotIndex)
            {
                int oldHotIndex = HotIndex;
                HotIndex = newHotIndex;
                if (oldHotIndex != -1)
                    DrawItem(oldHotIndex);
                if (HotIndex != -1)
                    DrawItem(HotIndex);
            }
            break;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        UpdateWindow(MainWindow->HWindow);

        SetForegroundWindow(HNotifyWindow);
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);
        int index;
        BOOL hitItem = HitTest(xPos, yPos, index);
        // If a Windows popup menu is open and we click the toolbar, WM_LBUTTONDOWN arrives immediately
        // so HotIndex == -1, therefore the condition index == HotIndex is removed.
        if (hitItem && /*index == HotIndex && */ !HotIndexIsTracked)
            EnterMenuInternal(index, FALSE, TRUE);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
