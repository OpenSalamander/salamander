// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	notifier.cpp
	Receives notification from the system when a WPD device arrives or
	is removed.
*/

#include "precomp.h"
#include "notifier.h"
#include "fx.h"
#include <Dbt.h>

const WCHAR CWpdNotifier::CLS_WPDNOTIFIER_W[] = L"ASPortablesPluginWpdNotiferWndCls";

CWpdNotifier::CWpdNotifier()
{
    m_hWnd = nullptr;
    m_hDevNotify = nullptr;
    RegisterWndCls();
    CreateWnd();
}

CWpdNotifier::~CWpdNotifier()
{
    if (m_hWnd != nullptr)
    {
        DestroyWnd();
    }
    UnregisterWndCls();
}

void CWpdNotifier::RegisterWndCls()
{
    WNDCLASSEXW wc = {
        0,
    };
    ATOM a;
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &CWpdNotifier::WndProcStatic;
    wc.lpszClassName = CLS_WPDNOTIFIER_W;
    wc.hInstance = FxGetModuleInstance();
    a = ::RegisterClassExW(&wc);
    _ASSERTE(a != 0);
}

void CWpdNotifier::UnregisterWndCls()
{
    BOOL ok = ::UnregisterClassW(CLS_WPDNOTIFIER_W, FxGetModuleInstance());
    _ASSERTE(ok);
}

void CWpdNotifier::CreateWnd()
{
    _ASSERTE(m_hWnd == nullptr);
    HWND hWnd = ::CreateWindowExW(
        0U, /* dwExStyle */
        CLS_WPDNOTIFIER_W,
        nullptr, /* lpWindowName */
        0U,      /* dwStyle */
        0,       /* X */
        0,       /* Y */
        0,       /* nWidth */
        0,       /* nHeight */
        HWND_MESSAGE,
        nullptr, /* hMenu */
        FxGetModuleInstance(),
        this);
    _ASSERTE(hWnd != nullptr);
    _ASSERTE(m_hWnd == hWnd);
}

void CWpdNotifier::DestroyWnd()
{
    _ASSERTE(m_hWnd != nullptr);
    BOOL ok = ::DestroyWindow(m_hWnd);
    _ASSERTE(ok);
    _ASSERTE(m_hWnd == nullptr);
}

LRESULT CWpdNotifier::WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CWpdNotifier* notifier = nullptr;
    LRESULT res;

    if (uMsg == WM_NCCREATE)
    {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        notifier = reinterpret_cast<CWpdNotifier*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(notifier));
        notifier->m_hWnd = hWnd;
    }
    else
    {
        notifier = reinterpret_cast<CWpdNotifier*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (notifier != nullptr)
    {
        res = notifier->WndProc(uMsg, wParam, lParam);
    }
    else
    {
        res = ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_NCDESTROY)
    {
        _ASSERTE(notifier != nullptr);
        _ASSERTE(notifier->m_hWnd != nullptr);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0L);
        notifier->m_hWnd = nullptr;
    }

    return res;
}

LRESULT CWpdNotifier::WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        RegisterDevNotification();
        break;

    case WM_DESTROY:
        UnregisterDevNotification();
        break;

    case WM_DEVICECHANGE:
        TRACE_I("WPD device change");
        break;
    }

    return ::DefWindowProcW(m_hWnd, uMsg, wParam, lParam);
}

void CWpdNotifier::RegisterDevNotification()
{
    _ASSERTE(m_hDevNotify == nullptr);
    _ASSERTE(m_hWnd != nullptr);

    DEV_BROADCAST_DEVICEINTERFACE_W db = {
        0,
    };
    db.dbcc_size = sizeof(db);
    db.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    db.dbcc_classguid = GUID_DEVINTERFACE_WPD;

    m_hDevNotify = ::RegisterDeviceNotificationW(m_hWnd, &db, DEVICE_NOTIFY_WINDOW_HANDLE);
    _ASSERTE(m_hDevNotify != nullptr);
}

void CWpdNotifier::UnregisterDevNotification()
{
    _ASSERTE(m_hDevNotify != nullptr);
    BOOL ok = ::UnregisterDeviceNotification(m_hDevNotify);
    _ASSERTE(ok);
    m_hDevNotify = nullptr;
}
