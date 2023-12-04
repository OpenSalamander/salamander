// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "7zip.h"
#include "7zclient.h"

#include "7zthreads.h"
#include "dialogs.h"
#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"

WNDPROC OldProgressDlgProc;

CSalamanderForOperationsAbstract* Salamander;

BOOL CALLBACK SubClassedProgressDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (WM_7ZIP == uMsg)
    {
        switch (wParam)
        {
        case WM_7ZIP_PROGRESS:
            if (!Salamander->ProgressSetSize(*(CQuadWord*)lParam, CQuadWord(-1, -1), TRUE))
            { // Canceled by the user
                Salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                Salamander->ProgressEnableCancel(FALSE);
                return E_ABORT;
            }
            return S_OK;

        case WM_7ZIP_SETTOTAL:
            Salamander->ProgressSetTotalSize(*(CQuadWord*)lParam, CQuadWord(-1, -1));
            return S_OK;

        case WM_7ZIP_ADDTEXT:
            Salamander->ProgressDialogAddText((char*)lParam, TRUE); // delayed paint, to avoid slow down by frequent refresh
            return S_OK;

        case WM_7ZIP_CREATEFILE:
        {
            // Ask whether to overwrite existing file
            CCreateFileParams* cfp = (CCreateFileParams*)lParam;

            HANDLE file = SalamanderSafeFile->SafeFileCreate(cfp->FileName, GENERIC_WRITE, FILE_SHARE_READ,
                                                             FILE_ATTRIBUTE_NORMAL, FALSE, hWnd, cfp->Name,
                                                             cfp->FileInfo, cfp->pSilent, TRUE, cfp->pSkip, NULL, 0, NULL, NULL);

            if (file == INVALID_HANDLE_VALUE)
            {
                return 1;
            }
            ::CloseHandle(file);
            return 0;
        }

        case WM_7ZIP_SHOWMBOXEX:
            return SalamanderGeneral->SalMessageBoxEx((MSGBOXEX_PARAMS*)lParam);

        case WM_7ZIP_DIALOGERROR:
        {
            CDialogErrorParams* dep = (CDialogErrorParams*)lParam;

            return SalamanderGeneral->DialogError(hWnd, dep->Flags, dep->FileName,
                                                  dep->Error, LoadStr(IDS_PACK_UPDATE_ERROR));
        }

        case WM_7ZIP_PASSWORD:
        {
            CEnterPasswordDialog dlg(hWnd);
            int res = (int)dlg.Execute();

            if (IDOK == res)
                strcpy((char*)lParam, dlg.GetPassword());
            return res;
        }
        }

        return 0;
    }

    return (BOOL)CallWindowProc(OldProgressDlgProc, hWnd, uMsg, wParam, lParam);
}

//
// worker threads
//

unsigned WINAPI DecompressThreadProcBody(LPVOID lpParameter)
{
    CDecompressParamObject* dpo = (CDecompressParamObject*)lpParameter;

    HRESULT result = (dpo->Archive)->Extract(dpo->FileIndex, dpo->Count, BoolToInt(dpo->Test), dpo->Callback);

    return result;
}

DWORD WINAPI DecompressThreadProc(LPVOID lpParameter)
{
    return SalamanderDebug->CallWithCallStack(DecompressThreadProcBody, lpParameter);
}

HRESULT LaunchAndDo7ZipTask(LPTHREAD_START_ROUTINE threadProc, LPVOID args)
{
    DWORD threadId;
    HANDLE hThread;

    OldProgressDlgProc = (WNDPROC)GetWindowLongPtr(Salamander->ProgressGetHWND(), GWLP_WNDPROC);
    SetWindowLongPtr(Salamander->ProgressGetHWND(), GWLP_WNDPROC, (LONG_PTR)SubClassedProgressDlgProc);

    hThread = ::CreateThread(NULL, 0, threadProc, args, 0, &threadId);

    if (!hThread)
    {
        return GetLastError();
    }

    for (;;)
    {
        if (WAIT_OBJECT_0 == MsgWaitForMultipleObjects(1, &hThread, FALSE, INFINITE, QS_ALLEVENTS | QS_SENDMESSAGE))
        {
            // Thread exited
            DWORD exitCode;

            GetExitCodeThread(hThread, &exitCode);
            CloseHandle(hThread);
            return exitCode; // Our thread body func returns HRESULT
        }

        MSG msg;
        while (PeekMessage(&msg, SalamanderGeneral->GetMainWindowHWND(), 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return S_OK;
}

HRESULT DoDecompress(CSalamanderForOperationsAbstract* salamander, CDecompressParamObject* dpo)
{
    Salamander = salamander;
    HRESULT result = LaunchAndDo7ZipTask(DecompressThreadProc, dpo);

    if (result == E_STOPEXTRACTION)
    {
        result = S_OK;
    }
    if ((result != E_ABORT) && (result != S_OK))
    {
        int label = dpo->Test ? IDS_TEST_ERROR : IDS_UNPACK_ERROR;

        if (FAILED(result) && ((FACILITY_WIN32 << 16) == (result & 0x7FFF0000)))
        {
            // LastError error encoded into HRESULT
            // There is something strange: E_OUTOFMEMORY as 0x8007000EL prints as "Not enough storage is available to complete this operation"
            // even when not truncated to 16 bits while 0x80000002L prints as "Ran out of memory"
            SysError(label, (result == E_OUTOFMEMORY) ? 0x80000002L : (result & 0xFFFF), FALSE);
        }
        else
        {
            Error(label, FALSE, result);
        }
        result = E_ABORT; // Do not display any other dialog
    }
    return result;
}

unsigned WINAPI UpdateThreadProcBody(LPVOID lpParameter)
{
    CUpdateParamObject* upo = (CUpdateParamObject*)lpParameter;

    HRESULT result = (upo->Archive)->UpdateItems(upo->Stream, upo->Count, upo->Callback);

    return result;
}

DWORD WINAPI UpdateThreadProc(LPVOID lpParameter)
{
    return SalamanderDebug->CallWithCallStack(UpdateThreadProcBody, lpParameter);
}

HRESULT DoUpdate(CSalamanderForOperationsAbstract* salamander, CUpdateParamObject* upo)
{
    Salamander = salamander;

    return LaunchAndDo7ZipTask(UpdateThreadProc, upo);
}

////////////////////////////////

// AddCallStackObject gets called by our code embedded in 7za.dll

struct AddCallStackObjectParam
{
    unsigned int(__stdcall* StartAddress)(void*);
    LPVOID Parameter;
};

unsigned __stdcall AddCallStackObject(void* param)
{
    AddCallStackObjectParam* p = (AddCallStackObjectParam*)param;
    return SalamanderDebug->CallWithCallStack(p->StartAddress, p->Parameter);
}
