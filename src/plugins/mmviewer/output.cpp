// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "output.h"
#include "renderer.h"
#include "mmviewer.h"

COutput::COutput() : Items(10, 5)
{
}

COutput::~COutput()
{
    DestroyItems();
}

int COutput::GetCount()
{
    return Items.Count;
}

const COutputItem*
COutput::GetItem(int i)
{
    return &Items[i];
}

void COutput::DestroyItems()
{
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        COutputItem* item = &Items[i];

        //vsechna okna smaze parent okno pri destrukci takze tohle je zbytecne
        if (IsWindow(item->hwnd))
            DestroyWindow(item->hwnd);

        if ((item->Flags & OIF_SEPARATOR) == 0)
        {
            if (item->Name)
                SalGeneral->Free(item->Name);
            if (item->Value)
                SalGeneral->Free(item->Value);
        }
    }
    Items.DetachMembers();
}

BOOL COutput::AddItem(const char* name, const char* value)
{
    COutputItem item;
    item.Flags = 0;
    item.hwnd = NULL;
    item.Name = SalGeneral->DupStr(name);
    item.Value = SalGeneral->DupStr(value);
    if (item.Name != NULL && item.Value != NULL)
    {
        if (IsUTF8Text(item.Value))
            item.Flags |= OIF_UTF8;
        Items.Add(item);
        if (Items.IsGood())
            return TRUE;
        else
            Items.ResetState();
    }
    return FALSE;
}

BOOL COutput::AddSeparator()
{
    COutputItem item;
    item.Flags = OIF_SEPARATOR;
    item.Name = NULL;
    item.Value = NULL;
    item.hwnd = NULL;
    Items.Add(item);
    if (Items.IsGood())
        return TRUE;
    else
        Items.ResetState();
    return FALSE;
}

BOOL COutput::AddHeader(const char* name, BOOL superHeader)
{
    COutputItem item;
    item.Flags = OIF_HEADER;
    if (superHeader)
        item.Flags |= OIF_EMPHASIZE;
    item.Name = SalGeneral->DupStr(name);
    item.Value = NULL;
    item.hwnd = NULL;
    Items.Add(item);
    if (Items.IsGood())
        return TRUE;
    else
        Items.ResetState();
    return FALSE;
}

LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_TAB:
        {
            HWND h = GetWindow(hwnd, KEY_DOWN(VK_SHIFT) ? GW_HWNDPREV : GW_HWNDNEXT);
            if (!h)
            {
                TRACE_I("EditSubclassProc: VK_TAB (parent): " << h);
                h = GetParent(hwnd);
                SetFocus(h);
            }
            else
            {
                TRACE_I("EditSubclassProc: VK_TAB (next/prev): " << h);
                SetFocus(h);
                SendMessage(h, EM_SETSEL, 0, -1);
                SendMessage(h, WM_ENSUREVISIBLE, 0, 0);
            }
        }
        break;
        }
    }
    break;

    case WM_MOUSEWHEEL:
    {
        HWND HWindow = GetParent(hwnd);
        SendMessage(HWindow, uMsg, wParam, lParam);
    }
        return 0;

    case WM_ENSUREVISIBLE:
    {
        TRACE_I("EditSubclassProc: WM_ENSUREVISIBLE: ");
        HWND HWindow = GetParent(hwnd);
        RECT r;
        GetClientRect(hwnd, &r);
        MapWindowPoints(hwnd, HWindow, (LPPOINT)&r, (sizeof(RECT) / sizeof(POINT)));
        SendMessage(HWindow, WM_ENSUREVISIBLE, 0, (LPARAM)&r);
    }
        return 0;
    }

    return CallWindowProc((WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
}

BOOL COutput::PrepareForRender(HWND parentWnd)
{
    BOOL r = TRUE;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        COutputItem& oi = Items[i];

        if ((oi.Flags & ~OIF_UTF8) == 0)
        { // Ignore non-text items
            if (oi.Flags & OIF_UTF8)
            { // NOTE: OIF_UTF8 is set only on NT-class OS's -> CreateWindowW is safe
                WCHAR buf[256];
                MultiByteToWideChar(CP_UTF8, 0, oi.Value, -1, buf, sizeof(buf) / sizeof(buf[0]));
                buf[sizeof(buf) / sizeof(buf[0]) - 1] = 0; // to be sure; 256 should be enough, however
                oi.hwnd = CreateWindowW(L"EDIT", buf,
                                        WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_READONLY | ES_LEFT | ES_AUTOVSCROLL | ES_MULTILINE,
                                        0, 0, 4, 4, parentWnd, NULL, DLLInstance, NULL);
            }
            else
            {
                oi.hwnd = CreateWindowA("EDIT", (oi.Flags & OIF_UTF8) ? "" : oi.Value,
                                        WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_READONLY | ES_LEFT | ES_AUTOVSCROLL | ES_MULTILINE,
                                        0, 0, 4, 4, parentWnd, NULL, DLLInstance, NULL);
            }

            if (oi.hwnd)
            {
                //do GWL_USERDATA schovej orig wnd proc ptr
                SetWindowLongPtr(oi.hwnd, GWLP_USERDATA, SetWindowLongPtr(oi.hwnd, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc));

                SendMessage(oi.hwnd, WM_SETFONT, (WPARAM)HBoldFont, 0);
                SendMessage(oi.hwnd, EM_SETMARGINS, EC_LEFTMARGIN, 5);
                SendMessage(oi.hwnd, EM_SETMARGINS, EC_RIGHTMARGIN, 5);
            }
            else
                r = FALSE;
        }
    }

    return r;
}