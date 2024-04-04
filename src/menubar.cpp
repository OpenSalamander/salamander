// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "mainwnd.h"

//*****************************************************************************
//
// CMenuBar
//

#define MENUBAR_LR_MARGIN 8 // number of points before and after the text, including vertical lines
#define MENUBAR_TB_MARGIN 4 // number of points above and below the text, including horizontal lines

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

    HCloseEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // "non-signaled" state, manual
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

    // print text
    const char* string = item->String;
    int stringLen = item->ColumnL1Len;

    // grease the strip at the top and bottom (for Windows Vista rebar)
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

    // NOTE: Since Windows Vista, MS asked for something in the rebar, so resizing the window leads to redrawing everything
    // band, as a result, the whole menu is redrawn and sometimes blinks (this did not happen on older OS, it was redrawn
    // only what was necessary); the problem could be suppressed by caching, but a better solution would be to rewrite
    // rebar and escape from the non-functional MS implementation
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

    // Ask the rest
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
    //  test pad, can be invoked by pressing the Alt key
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
    ExitMenuLoop = FALSE; // Will be set externally from TrackPopup
    DispatchDelayedMsg = FALSE;

    // hook this thread
    HHOOK hOldHookProc = OldMenuHookTlsAllocator.HookThread();
    // we will add ourselves to the monitoring of closing messages
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
        TrackHotIndex(); // take the unpacked menu and capture it
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
                            // delete current item
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
                if (msg.wParam == VK_MENU && (msg.lParam & 0x40000000) == 0 || // Alt down, but not autorepeat
                    (!shiftPressed && msg.wParam == VK_F10))
                {
                    leaveLoop = TRUE;
                    break; // we will let the message be delivered so that upon the arrival of Up, there is no entry into the system menu
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
                    // cyclic shifting of the HotIndex item
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
                    // unpack the menu
                    if (HotIndex != -1)
                    {
                        OpenWithSelect = TRUE; // we want to select an item
                        OpenByMouse = FALSE;
                        TrackHotIndex();
                        if (ExitMenuLoop)
                            leaveLoop = TRUE;
                    }
                }
                }
                msg.hwnd = HWindow; // redirect to ours (to prevent the escape of keys)
                // patch: we must not allow WM_CHAR to be generated, which would fall through to the owner
                // menu and caused a beep (Find dialog)
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
                            // delete current item
                            int oldHotIndex = HotIndex;
                            HotIndex = index2;
                            DrawItem(oldHotIndex);
                        }
                        OpenWithSelect = FALSE; // I am triggered by a mouse click - do not select the first item
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
            // no message is in the queue - we will wait until one arrives
            DWORD ret = MsgWaitForMultipleObjects(1, &HCloseEvent, FALSE, INFINITE, QS_ALLINPUT);
            if (ret == 0xFFFFFFFF)
                TRACE_E("MsgWaitForMultipleObjects failed");
            /*        if (ret == WAIT_OBJECT_0 + 0)
        TRACE_I("Event set");
      if (ret == WAIT_OBJECT_0 + 1)
        TRACE_I("Event set 2");
      if (ret == WAIT_ABANDONED_0 + 0)
        TRACE_I("Event set 3");*/
        }
    }

    if (HotIndex != -1)
    {
        int oldHotIndex = HotIndex;
        HotIndex = -1;
        DrawItem(oldHotIndex);
    }

    // Do we have hidden accelerators?
    if (ForceAccelVisible)
    {
        ForceAccelVisible = FALSE;
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
    }

    // we will exclude ourselves from the monitoring of closing messages
    MenuWindowQueue.Remove(HWindow);
    // if we hooked, we will also release
    if (hOldHookProc != NULL)
        OldMenuHookTlsAllocator.UnhookThread(hOldHookProc);

    MenuLoop = FALSE;

    if (MouseIsTracked)
        WindowProc(WM_MOUSELEAVE, 0, 0);

    SendMessage(HNotifyWindow, WM_USER_LEAVEMENULOOP, 0, 0);

    // delivery of results is coming - the same thing is being handled in CMenuPopup::Track

    // simulating mouse movement so that others can catch it
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND hWndUnderCursor = WindowFromPoint(cursorPos);
    if (hWndUnderCursor != NULL)
    {
        ScreenToClient(hWndUnderCursor, &cursorPos);
        // in RC1 there was SendMessage, but it was causing trouble with EroiicaViewer
        // if the user displayed the PDF, placed the cursor over the viewer/salamander window
        // and using Alt+Tab, Alt+Tab switches between EV/SS windows, the EV window sometimes for 2s
        // It went silent before its content was rendered.
        // Let's try switching to PostMessage to see if the functionality is the same.
        PostMessage(hWndUnderCursor, WM_MOUSEMOVE, 0, MAKELPARAM(cursorPos.x, cursorPos.y));
    }

    // deliver delayed message
    if (DispatchDelayedMsg)
    {
        TranslateMessage(&DelayedMsg);
        PostMessage(DelayedMsg.hwnd, DelayedMsg.message, DelayedMsg.wParam, DelayedMsg.lParam);
    }

    // return command
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

        // we will save the mouse position (if it is above us)
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
        HotIndex = -1; // completely disrupting the hot item
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
            if ((lpMsg->lParam & 0x40000000) == 0) // if it's not about auto repeat
            {
                HandlingVK_MENU = TRUE;
                // check if we should display accelerators
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
                // check if we should display accelerators
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
                return TRUE; // Releasing Alt after Alt+Wheel press will close it, otherwise it will enter the window menu (without displaying it)
            }
            // if the user did not rotate the wheel, we must not process the message, because it will stop working
            // Alt+num064 (etc) for inserting characters
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
                // if the menubar was activated by the mouse, it is now time to set ForceAccelVisible to
                // underscore promoted to open submenus
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
            DrawItem(HotIndex); // Unlocking HotIndex (Closing == TRUE)
        SetEvent(HCloseEvent);  // let's start the message queue
        return 0;
    }

    case WM_ERASEBKGND:
    {
        if (WindowsVistaAndLater) // under the flashing rib
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
        // we are sending it from the mainwindow
        if (LOWORD(wParam) == UIS_CLEAR)
            UIState &= ~HIWORD(wParam);
        else if (LOWORD(wParam) == UIS_SET)
            UIState |= HIWORD(wParam);

        InvalidateRect(HWindow, NULL, FALSE); // we are going through the bitmap cache, so we won't flicker
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
            // prevents conflicts between cursor movement and Left/Right button press
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
                if (!HelpMode2 && !MenuLoop && !MouseIsTracked) // we only need to capture in track-mode
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
                if (!MenuLoop) // if we are in MenuLoop, we let the last selected item shine
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
        // If the Windows popup menu is open and we click into the toolbar, it will come right away
        // WM_LBUTTONDOWN so HotIndex == -1, therefore I am removing the condition index == HotIndex.
        if (hitItem && /*index == HotIndex &&*/ !HotIndexIsTracked)
            EnterMenuInternal(index, FALSE, TRUE);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
