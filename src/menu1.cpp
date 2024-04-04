// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"

const char* WC_POPUPMENU = "PopupMenuClass";

CMenuWindowQueue MenuWindowQueue;
COldMenuHookTlsAllocator OldMenuHookTlsAllocator;

//static DWORD MenuMessageHookTimeout = GetTickCount();

//*****************************************************************************
//
// CMenuWindowQueue
//

CMenuWindowQueue::CMenuWindowQueue()
    : Data(5, 5)
{
    CALL_STACK_MESSAGE_NONE
    UsingData = FALSE;
    HANDLES(InitializeCriticalSection(&DataCriticalSection));
}

CMenuWindowQueue::~CMenuWindowQueue()
{
    CALL_STACK_MESSAGE_NONE
    HANDLES(DeleteCriticalSection(&DataCriticalSection));
}

BOOL CMenuWindowQueue::Add(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CMenuWindowQueue::Add()");
    BOOL isGood;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    Data.Add(hWindow);
    isGood = Data.IsGood();
    if (!isGood)
        Data.ResetState();
    //  TRACE_I("XXX CMenuWindowQueue::Add() hWnd="<<hex<<hWindow);
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
    //  MenuMessageHookTimeout = GetTickCount();
    return isGood;
}

void CMenuWindowQueue::Remove(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CMenuWindowQueue::Remove()");

    if (UsingData)
        return;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    UsingData = TRUE;
    int i;
    for (i = Data.Count - 1; i >= 0; i--)
        if (Data[i] == hWindow)
        {
            Data.Detach(i);
            //      TRACE_I("XXX CMenuWindowQueue::Remove() hWnd="<<hex<<hWindow);
            break;
        }
    UsingData = FALSE;
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

void CMenuWindowQueue::DispatchCloseMenu()
{
    CALL_STACK_MESSAGE1("CMenuWindowQueue::DispatchCloseMenu()");
    if (UsingData)
        return;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    UsingData = TRUE;
    int i;
    for (i = Data.Count - 1; i >= 0; i--)
    {
        //    TRACE_I("XXX CMenuWindowQueue::DispatchCloseMenu() hWnd="<<hex<<Data[i]);
        CWindow::CWindowProc(Data[i], WM_USER_CLOSEMENU, 0, 0);
    }
    Data.DetachMembers();
    UsingData = FALSE;
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

//*****************************************************************************
//
// COldMenuHookTlsAllocator
//

DWORD OldMenuHookTlsIndexHOldHook = 0xFFFFFFFF;

LRESULT CALLBACK
MenuMessageHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE_NONE
    HHOOK hOldHookProc = NULL;
    if (OldMenuHookTlsIndexHOldHook != 0xFFFFFFFF)
        hOldHookProc = (HHOOK)TlsGetValue(OldMenuHookTlsIndexHOldHook);
    if (nCode >= 0)
    {
        CWPSTRUCT* cwp = (CWPSTRUCT*)lParam;
        if (cwp->message == WM_CANCELMODE || cwp->message == WM_ACTIVATEAPP ||
            cwp->message == WM_ACTIVATE || cwp->message == WM_NCACTIVATE ||
            cwp->message == WM_KILLFOCUS)
        {
            //      if (GetTickCount() - MenuMessageHookTimeout < 500)
            //        TRACE_I("SKIPPING !!!");
            //      else
            //      {
            //        TRACE_I("XXX MenuMessageHookProc(); message="<<hex<<cwp->message<<" hwnd="<<cwp->hwnd<<" wParam="<<cwp->wParam<<" lParam="<<cwp->lParam);
            MenuWindowQueue.DispatchCloseMenu();
            //        MenuMessageHookTimeout = GetTickCount();
            //      }
        }
    }
    if (hOldHookProc != NULL)
        return CallNextHookEx(hOldHookProc, nCode, wParam, lParam);
    else
        return 0; // we can be called from a completely different thread (according to MSDN)
}

COldMenuHookTlsAllocator::COldMenuHookTlsAllocator()
{
    CALL_STACK_MESSAGE1("COldMenuHookTlsAllocator::COldMenuHookTlsAllocator()");
    OldMenuHookTlsIndexHOldHook = HANDLES(TlsAlloc());
    if (OldMenuHookTlsIndexHOldHook != 0xFFFFFFFF)
        TlsSetValue(OldMenuHookTlsIndexHOldHook, (LPVOID)NULL);
    else
        TRACE_E("Unable to allocate TLS");
}

COldMenuHookTlsAllocator::~COldMenuHookTlsAllocator()
{
    CALL_STACK_MESSAGE1("COldMenuHookTlsAllocator::~COldMenuHookTlsAllocator()");
    if (OldMenuHookTlsIndexHOldHook != 0xFFFFFFFF)
    {
        HANDLES(TlsFree(OldMenuHookTlsIndexHOldHook));
        OldMenuHookTlsIndexHOldHook = 0xFFFFFFFF;
    }
}

HHOOK
COldMenuHookTlsAllocator::HookThread()
{
    CALL_STACK_MESSAGE1("COldMenuHookTlsAllocator::HookThread()");
    if (OldMenuHookTlsIndexHOldHook == 0xFFFFFFFF)
        return NULL;
    // every thread that enters here will hook a procedure (if it doesn't already have it hooked)
    HHOOK hOldHookProc = (HHOOK)TlsGetValue(OldMenuHookTlsIndexHOldHook);
    if (hOldHookProc == NULL)
    {
        DWORD threadID = GetCurrentThreadId();
        hOldHookProc = SetWindowsHookEx(WH_CALLWNDPROC, // HANDLES does not know how to!
                                        MenuMessageHookProc,
                                        NULL, threadID);
        // stores a handle to the procedure in TLS so that CallNextHookEx can be called
        TlsSetValue(OldMenuHookTlsIndexHOldHook, (LPVOID)hOldHookProc);
        return hOldHookProc;
    }
    else
        return NULL;
}

void COldMenuHookTlsAllocator::UnhookThread(HHOOK hOldHookProc)
{
    CALL_STACK_MESSAGE_NONE
    UnhookWindowsHookEx(hOldHookProc); // HANDLES does not know how to!
    if (OldMenuHookTlsIndexHOldHook != 0xFFFFFFFF)
        TlsSetValue(OldMenuHookTlsIndexHOldHook, NULL);
}

//*****************************************************************************
//
// Menu init / release
//

BOOL InitializeMenu()
{
    CALL_STACK_MESSAGE1("InitializeMenu()");
#define CS_DROPSHADOW 0x00020000
    DWORD styles = CS_OWNDC | CS_DBLCLKS | CS_SAVEBITS | CS_DROPSHADOW;
    if (!CWindow::RegisterUniversalClass(styles,
                                         0, 0, NULL, LoadCursor(NULL, IDC_ARROW), (HBRUSH)NULL,
                                         NULL, WC_POPUPMENU, NULL))
        return FALSE;
    return TRUE;
}
/* not called
void ReleaseMenu()
{
}*/
//*****************************************************************************
//
// CMenuItem
//

CMenuItem::CMenuItem()
{
    CALL_STACK_MESSAGE_NONE
    Type = 0;
    State = 0;
    ID = 0;
    SubMenu = NULL;
    HBmpChecked = NULL;
    HBmpUnchecked = NULL;
    HBmpItem = NULL;
    ImageIndex = -1;
    HIcon = NULL;
    HOverlay = NULL;
    CustomData = 0;
    SkillLevel = MENU_LEVEL_BEGINNER | MENU_LEVEL_INTERMEDIATE | MENU_LEVEL_ADVANCED;
    Enabler = NULL;
    String = NULL;
    Height = 0;
    MinWidth = 0;
    YOffset = 0;
    ColumnL1 = NULL;
    ColumnL2 = NULL;
    ColumnR = NULL;
    Flags = 0;
}

CMenuItem::~CMenuItem()
{
    CALL_STACK_MESSAGE_NONE
    if (SubMenu != NULL)
        delete SubMenu;
    if (String != NULL)
        delete[] String;
}

BOOL CMenuItem::SetText(const char* text, int len)
{
    CALL_STACK_MESSAGE_NONE
    if (text != NULL && len == -1)
        len = lstrlen(text);

    char* newString = NULL;
    if (text != NULL)
    {
        newString = new char[len + 1];
        if (newString == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        if (len > 0)
            memmove(newString, text, len);
        newString[len] = 0;
    }

    if (String != NULL)
    {
        delete[] String;
        String = NULL;
        ColumnL1Len = 0;
    }
    if (newString != NULL)
    {
        String = newString;
        ColumnL1Len = len;
    }
    return TRUE;
}

void CMenuItem::DecodeSubTextLenghtsAndWidths(CMenuSharedResources* sharedRes, BOOL threeCol)
{
    CALL_STACK_MESSAGE2("CMenuItem::DecodeSubTextLenghtsAndWidths(, %d)", threeCol);
    if ((Type & MENU_TYPE_STRING) == 0 || String == NULL)
    {
        TRACE_E("Incorrect call to CMenuItem::DecodeText");
        return;
    }
    char* str = String;
    char* iterator = str;
    char* begin = str;
    int column = 0;
    BOOL inifiniteLoop = TRUE;
    ColumnL1 = NULL;
    ColumnL2 = NULL;
    ColumnR = NULL;

    // set pointers and counts of characters for individual columns
    while (inifiniteLoop)
    {
        if (*iterator == '\t' || *iterator == 0)
        {
            if (column == 0)
            {
                ColumnL1 = begin;
                ColumnL1Len = (int)(iterator - begin);
                if (*iterator == 0)
                    break;
                begin = iterator + 1;
            }
            else if (column == 1 && threeCol)
            {
                ColumnL2 = begin;
                ColumnL2Len = (int)(iterator - begin);
                if (*iterator == 0)
                    break;
                begin = iterator + 1;
            }
            else
            {
                ColumnR = begin;
                ColumnRLen = (int)(iterator - begin);
                if (*iterator == 0)
                    break;
                begin = iterator + 1;
            }
            column++;
        }
        iterator++;
    }

    // calculate the width of the text
    HFONT hOldFont;
    if (State & MENU_STATE_DEFAULT)
        hOldFont = (HFONT)SelectObject(sharedRes->HTempMemDC, sharedRes->HBoldFont);
    else
        hOldFont = (HFONT)SelectObject(sharedRes->HTempMemDC, sharedRes->HNormalFont);
    RECT size;
    if (ColumnL1 != NULL)
    {
        ZeroMemory(&size, sizeof(size));
        DrawText(sharedRes->HTempMemDC, ColumnL1, ColumnL1Len,
                 &size, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
        ColumnL1Width = size.right;
    }
    if (ColumnL2 != NULL)
    {
        ZeroMemory(&size, sizeof(size));
        DrawText(sharedRes->HTempMemDC, ColumnL2, ColumnL2Len,
                 &size, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
        ColumnL2Width = size.right;
    }
    if (ColumnR != NULL)
    {
        ZeroMemory(&size, sizeof(size));
        DrawText(sharedRes->HTempMemDC, ColumnR, ColumnRLen,
                 &size, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
        ColumnRWidth = size.right;
    }
    SelectObject(sharedRes->HTempMemDC, hOldFont);
}
