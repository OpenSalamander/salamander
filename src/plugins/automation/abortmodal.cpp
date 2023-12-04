// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	abortmodal.cpp
	Wrapper to allow aborting modal windows.
*/

#include "precomp.h"
#include "scriptlist.h"
#include "abortmodal.h"

extern HINSTANCE g_hInstance;

static const TCHAR WC_SCRATCH[] = _T("AutomationAbortableTarget");
#define ID_TIMER 1

static bool g_bScratchWindowClassRegistered;

struct ABORTSTATE
{
    HWND hwndReenable;
    bool bAborted;
};

void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    ABORTSTATE* state;
    state = reinterpret_cast<ABORTSTATE*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (state->bAborted)
    {
        if (state->hwndReenable != NULL)
        {
            EnableWindow(state->hwndReenable, TRUE);
        }
        PostQuitMessage(0);
    }
}

static LRESULT CALLBACK ScratchWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NCCREATE:
    {
        CREATESTRUCT* pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        break;
    }

    case WM_SAL_ABORTMODAL:
    {
        // Simply posting quit message from here does not exit
        // the modal message loop for some reason (at least
        // not on Win8). The hack with the timer works (courtesy
        // http://blogs.msdn.com/b/oldnewthing/archive/2005/03/04/385100.aspx).
        ABORTSTATE* state;
        state = reinterpret_cast<ABORTSTATE*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        state->bAborted = true;
        return 0;
    }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static bool RegisterScratchWindowClass()
{
    WNDCLASSEX cls = {
        0,
    };

    cls.cbSize = sizeof(WNDCLASSEX);
    cls.lpfnWndProc = ScratchWindowProc;
    cls.hInstance = g_hInstance;
    cls.lpszClassName = WC_SCRATCH;

    return RegisterClassEx(&cls) != FALSE;
}

HRESULT AbortableModalDialogWrapper(
    __in class CScriptInfo* pScript,
    __in_opt HWND hwndOwner,
    __in void(WINAPI* pfnDialogFn)(void*),
    __in_opt void* pContext)
{
    HRESULT hr = S_OK;
    ABORTSTATE state = {
        0,
    };

    if (!g_bScratchWindowClassRegistered)
    {
        if (!RegisterScratchWindowClass())
        {
            _ASSERTE(0);
            return E_FAIL;
        }
        g_bScratchWindowClassRegistered = true;
    }

    if (hwndOwner != NULL && IsWindowEnabled(hwndOwner))
    {
        state.hwndReenable = hwndOwner;
    }

    HWND hwndScratch = CreateWindowEx(0, WC_SCRATCH, NULL, WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, g_hInstance, &state);
    _ASSERTE(hwndScratch != NULL);
    if (hwndScratch == NULL)
    {
        return E_FAIL;
    }

    SetTimer(hwndScratch, ID_TIMER, 100, TimerProc);

    HWND hwndPrevious = pScript->SetAbortTargetHwnd(hwndScratch);

    pfnDialogFn(pContext);

    KillTimer(hwndScratch, ID_TIMER);

    if (state.bAborted)
    {
        hr = SALAUT_E_ABORT;

        // Pump out the WM_QUIT we generated. The eventual messages
        // before WM_QUIT are lost.
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            DispatchMessage(&msg);
        }
    }

    pScript->SetAbortTargetHwnd(hwndPrevious);

    DestroyWindow(hwndScratch);

    return hr;
}

void UninitializeAbortableModalDialogWrapper()
{
    if (g_bScratchWindowClassRegistered)
    {
        if (!UnregisterClass(WC_SCRATCH, g_hInstance))
            TRACE_E("UnregisterClass(WC_SCRATCH) has failed");
    }
}
