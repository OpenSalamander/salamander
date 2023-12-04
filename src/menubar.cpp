// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "mainwnd.h"

//*****************************************************************************
//
// CMenuBar
//

#define MENUBAR_LR_MARGIN 8 // pocet bodu pred a za textem, vcetne svisle cary
#define MENUBAR_TB_MARGIN 4 // pocet bodu nad a pod textem, vcetne vodorovne cary

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

    // vypisu text
    const char* string = item->String;
    int stringLen = item->ColumnL1Len;

    // podmazneme pruh nahore a dole (pro Windows Vista rebar)
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

    // POZNAMKA: od Windows Vista MS neco vyprasili v rebar, takze resize okna vede k prekresleni vsech
    // bandu, v dusledku se prekresluje cele menu a obcas zamrka (na starsich OS se to nedelo, prekreslovalo
    // se jen to, co bylo potreba); problem by se dal potlacit cachovanim, ale lepsi reseni bude prepsat
    // rebar a zdrhnout z nefunkcni MS implementace
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

    // domazu zbytek
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
    //  testovaci padacka, lze ji vyvolat stiskem klavesy Alt
    //  int a = 0;
    //  printf("%d", 5 / a);
    EnterMenuInternal(-1, FALSE, FALSE); // user stisknul VK_MENU
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
    ExitMenuLoop = FALSE; // bude nastaven externe z TrackPopup
    DispatchDelayedMsg = FALSE;

    // zahakujeme tento thread
    HHOOK hOldHookProc = OldMenuHookTlsAllocator.HookThread();
    // pridame se do monitoringu zaviracich zprav
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
        TrackHotIndex(); // vybalene menu si veme capture
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
                            // smazu aktualni polozku
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
                if (msg.wParam == VK_MENU && (msg.lParam & 0x40000000) == 0 || // Alt down, ale ne autorepeat
                    (!shiftPressed && msg.wParam == VK_F10))
                {
                    leaveLoop = TRUE;
                    break; // nechame zpravu dorucit, aby po prichodu Up nedoslo ke vstupu do systmoveho menu
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
                    // cyklicke posuvani HotIndex polozky
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
                    // vybalime menu
                    if (HotIndex != -1)
                    {
                        OpenWithSelect = TRUE; // chceme vybrat polozku
                        OpenByMouse = FALSE;
                        TrackHotIndex();
                        if (ExitMenuLoop)
                            leaveLoop = TRUE;
                    }
                }
                }
                msg.hwnd = HWindow; // presmerujeme na nas (aby nedoslo k uniku klaves ven)
                // patch: nesmime nechat neagenerovat WM_CHAR, ktery by propadnul do majitele
                // menu a zpusobil pipnuti (Find dialog)
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
                            // smazu aktualni polozku
                            int oldHotIndex = HotIndex;
                            HotIndex = index2;
                            DrawItem(oldHotIndex);
                        }
                        OpenWithSelect = FALSE; // jsem vyvolani z kliknuti mysi - nevybereme prvni polozku
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
            // zadna zprava neni v queue - budeme cekat, dokud nejaka nedorazi
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

    // mame skryt akceleratory?
    if (ForceAccelVisible)
    {
        ForceAccelVisible = FALSE;
        InvalidateRect(HWindow, NULL, FALSE);
        UpdateWindow(HWindow);
    }

    // vyhodime se z monitoringu zaviracich zprav
    MenuWindowQueue.Remove(HWindow);
    // pokud jsme hookovali, budeme take uvolnovat
    if (hOldHookProc != NULL)
        OldMenuHookTlsAllocator.UnhookThread(hOldHookProc);

    MenuLoop = FALSE;

    if (MouseIsTracked)
        WindowProc(WM_MOUSELEAVE, 0, 0);

    SendMessage(HNotifyWindow, WM_USER_LEAVEMENULOOP, 0, 0);

    // prichazi dorucovani vysledku - stejna vec se resi v CMenuPopup::Track

    // simulujeme pohyb mysi, aby si ji ostatni mohli zachytit
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND hWndUnderCursor = WindowFromPoint(cursorPos);
    if (hWndUnderCursor != NULL)
    {
        ScreenToClient(hWndUnderCursor, &cursorPos);
        // j.r. do RC1 zde bylo SendMessage, ale zlobilo to s EroiicaViewer
        // pokud uzivatel zobrazil PDF, postavil kurzor nad okno vieweru/salamandera
        // a pomoci Alt+Tab, Alt+Tab prepinal mezi oknama EV/SS, okno EV se obcas na 2s
        // odmlcelo nez se vykreslil jeho obsah.
        // Zkusime prejit na PostMessage, zda se, ze funkcionalita je stejna.
        PostMessage(hWndUnderCursor, WM_MOUSEMOVE, 0, MAKELPARAM(cursorPos.x, cursorPos.y));
    }

    // dorucime opozdenou message
    if (DispatchDelayedMsg)
    {
        TranslateMessage(&DelayedMsg);
        PostMessage(DelayedMsg.hwnd, DelayedMsg.message, DelayedMsg.wParam, DelayedMsg.lParam);
    }

    // vratime command
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

        IndexToOpen = -1; // bude nastavena v TrackInternal
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

        // ulozime si pozici mysi (pokud je nad nama)
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
        HotIndex = -1; // uplne rusime hot item
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
            if ((lpMsg->lParam & 0x40000000) == 0) // pokud nejde o auto repeat
            {
                HandlingVK_MENU = TRUE;
                // zjistime, zda nemame zobrazit akceleratory
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
            WheelDuringMenu = FALSE; // pokud user zatocil koleckem a nasledne napsal cislo (Alt+numXXX), budeme toceni ignorovat
            HandlingVK_MENU = FALSE;
        }
        if (wParam == VK_F10)
        {
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (!shiftPressed && !altPressed && !ctrlPressed)
            {
                // zjistime, zda nemame zobrazit akceleratory
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
                return TRUE; // pusteni altu po Alt+Wheel zatlucu, jinak dojde do vstupu do windowmenu (bez jeho zobrazeni)
            }
            // pokud uzivatel netocil koleckem, nesmime zpravu zpracovat, protoze prestane fungovat
            // Alt+num064 (atd) pro vkladani znaku
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
                // pokud byl menubar aktivovan mysi, nyni je cas nastavit ForceAccelVisible, aby se
                // podtrzeni napropagovalo do oteviranych submenu
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
            DrawItem(HotIndex); // odmacknu HotIndex (Closing == TRUE)
        SetEvent(HCloseEvent);  // nechame rozjet message queue
        return 0;
    }

    case WM_ERASEBKGND:
    {
        if (WindowsVistaAndLater) // pod vistou blika rebar
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
        // posilame si same z mainwindow
        if (LOWORD(wParam) == UIS_CLEAR)
            UIState &= ~HIWORD(wParam);
        else if (LOWORD(wParam) == UIS_SET)
            UIState |= HIWORD(wParam);

        InvalidateRect(HWindow, NULL, FALSE); // jedeme pres cache bitmap, takze neblikneme
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
            // zamezuje konfliktum pohybu kurzoru a stisku Left/Right
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
                if (!HelpMode2 && !MenuLoop && !MouseIsTracked) // pouze v track-mode potrebujeme capture
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
                if (!MenuLoop) // pokud jsme v MenuLoop, nechame svitit posledni vybranou polozku
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
        // Pokud je otevrene windows popup menu a klikneme do toolbary, prijde rovnou
        // WM_LBUTTONDOWN takze HotIndex == -1, proto vyrazuji podminku index == HotIndex.
        if (hitItem && /*index == HotIndex && */ !HotIndexIsTracked)
            EnterMenuInternal(index, FALSE, TRUE);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
