// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include "lstrfix.h"
#include "..\shexreg.h"
#include "shellext.h"

#undef INTERFACE
#define INTERFACE ImpICopyHook

STDMETHODIMP CH_QueryInterface(THIS_ REFIID riid, void** ppv)
{
    CopyHook* ch = (CopyHook*)This;

    WriteToLog("CH_QueryInterface");

    // delegate to the underlying object
    return ch->m_pObj->lpVtbl->QueryInterface((IShellExt*)ch->m_pObj, riid, ppv);
}

STDMETHODIMP_(ULONG)
CH_AddRef(THIS)
{
    CopyHook* ch = (CopyHook*)This;

    WriteToLog("CH_AddRef");

    // delegate to the underlying object
    return ch->m_pObj->lpVtbl->AddRef((IShellExt*)ch->m_pObj);
}

STDMETHODIMP_(ULONG)
CH_Release(THIS)
{
    CopyHook* ch = (CopyHook*)This;

    WriteToLog("CH_Release");

    // delegate to the underlying object
    return ch->m_pObj->lpVtbl->Release((IShellExt*)ch->m_pObj);
}

#pragma optimize("", off)
void MyZeroMemory(void* ptr, DWORD size)
{
    char* c = (char*)ptr;
    while (size-- != 0)
        *c++ = 0;
}
#pragma optimize("", on)

PTOKEN_USER GetProcessUser(DWORD pid)
{
    PTOKEN_USER pReturnTokenUser = NULL;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);

#ifdef SHEXT_LOG_ENABLED
    {
        char xxx[200];
        wsprintf(xxx, "GetProcessUser: PID=%d, OpenProcess error=%d", pid,
                 (hProcess == NULL ? GetLastError() : 0));
        WriteToLog(xxx);
    }
#endif // SHEXT_LOG_ENABLED

    if (hProcess != NULL)
    {
        HANDLE hToken = NULL;
        PTOKEN_USER pTokenUser = NULL;
        DWORD dwBufferSize = 0;

        WriteToLog("GetProcessUser: OpenProcess");

        // Open the access token associated with the process.
        if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
        {

            WriteToLog("GetProcessUser: OpenProcessToken");

            // get the size of the memory buffer needed for the SID
            if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize) &&
                GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {

                WriteToLog("GetProcessUser: GetTokenInformation");

                pTokenUser = (PTOKEN_USER)GlobalAlloc(GMEM_FIXED, dwBufferSize);
                if (pTokenUser != NULL)
                {
                    MyZeroMemory(pTokenUser, dwBufferSize);

                    // Retrieve the token information in a TOKEN_USER structure.
                    if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize) &&
                        IsValidSid(pTokenUser->User.Sid))
                    {

                        WriteToLog("GetProcessUser: success!");

                        pReturnTokenUser = pTokenUser;
                        pTokenUser = NULL;
                    }
                    if (pTokenUser != NULL)
                        GlobalFree(pTokenUser);
                }
            }
            CloseHandle(hToken);
        }
        CloseHandle(hProcess);
    }
    return pReturnTokenUser;
}

// returns TRUE only if we obtained the users of both processes and they differ (returns FALSE on error)
BOOL AreNotProcessesOfTheSameUser(DWORD pid1, DWORD pid2)
{
    if (pid1 == pid2)
        return FALSE;
    else
    {
        BOOL ret = FALSE;
        PTOKEN_USER user1 = GetProcessUser(pid1);
        PTOKEN_USER user2 = GetProcessUser(pid2);
        if (user1 == NULL && user2 != NULL || user1 != NULL && user2 == NULL ||
            user1 != NULL && user2 != NULL && !EqualSid(user1->User.Sid, user2->User.Sid))
            ret = TRUE;
        if (user1 != NULL)
            GlobalFree(user1);
        if (user2 != NULL)
            GlobalFree(user2);
        return ret;
    }
}

STDMETHODIMP_(UINT)
CH_CopyCallback(THIS_ HWND hwnd, UINT wFunc, UINT wFlags,
                LPCSTR pszSrcFile, DWORD dwSrcAttribs,
                LPCSTR pszDestFile, DWORD dwDestAttribs)
{

    // Vista: when Copy originates from Salamander's archive (regardless of elevation or user)
    // and Paste occurs in a different elevated Salamander, the system Drop operation does not
    // call this copy hook, so only the CLIPFAKE directory is copied. The same happens when the
    // Paste target is one of the elevated Salamander browser windows. Salamander ideally should
    // not run elevated (this is only a temporary workaround while UAC support is missing). We
    // will likely leave this unresolved unless someone complains. Pasting into Salamander panels
    // is under our control and therefore solvable, but we probably do not want to address it.

    // W2K: when Copy & Paste occurs between Salamander instances running under different users,
    // the process handling Paste must have access to the TEMP directory of the user who executed
    // Copy. Otherwise Windows reports that the source directory is inaccessible because it cannot
    // reach CLIPFAKE. Since almost all users run as administrators, this issue may never show up.

    CopyHook* ch = (CopyHook*)This;
    const char* s;
    char buf1[MAX_PATH];
    char buf2[MAX_PATH];
    char text[300];
    int count;

    // mutex for accessing the shared memory
    HANDLE salShExtSharedMemMutex = NULL;
    // shared memory - see the CSalShExtSharedMem structure
    HANDLE salShExtSharedMem = NULL;
    // event for sending a request to perform Paste in the source Salamander (used only on Vista+)
    HANDLE salShExtDoPasteEvent = NULL;
    // mapped shared memory - see the CSalShExtSharedMem structure
    CSalShExtSharedMem* salShExtSharedMemView = NULL;

    UINT ret = IDYES;
    BOOL samePIDAndTIDMsg = FALSE;
    BOOL salBusyMsg = FALSE;

    WriteToLog("CH_CopyCallback");

    if (wFunc == FO_MOVE || wFunc == FO_COPY)
    {
        salShExtSharedMemMutex = OpenMutex(SYNCHRONIZE, FALSE, SALSHEXT_SHAREDMEMMUTEXNAME);
        if (salShExtSharedMemMutex != NULL)
        {

            WriteToLog("CH_CopyCallback: salShExtSharedMemMutex");

            WaitForSingleObject(salShExtSharedMemMutex, INFINITE);
            salShExtSharedMem = OpenFileMapping(FILE_MAP_WRITE, FALSE, SALSHEXT_SHAREDMEMNAME);
            if (salShExtSharedMem != NULL)
            {

                WriteToLog("CH_CopyCallback: salShExtSharedMem");

                salShExtSharedMemView = (CSalShExtSharedMem*)MapViewOfFile(salShExtSharedMem, // FIXME_X64 are we passing x86/x64 incompatible data?
                                                                           FILE_MAP_WRITE, 0, 0, 0);
                if (salShExtSharedMemView != NULL)
                {

                    WriteToLog("CH_CopyCallback: salShExtSharedMemView");

                    if (salShExtSharedMemView->DoDragDropFromSalamander &&
                        (lstrcmpi(salShExtSharedMemView->DragDropFakeDirName, pszSrcFile) == 0 ||
                         GetShortPathName(salShExtSharedMemView->DragDropFakeDirName, buf1, MAX_PATH) &&
                             GetShortPathName(pszSrcFile, buf2, MAX_PATH) && lstrcmpi(buf1, buf2) == 0))
                    {

                        WriteToLog("CH_CopyCallback: drop!");

                        // The operation targets the "fake" directory; block it so Salamander can
                        // unpack the archive or copy from the file system instead.
                        ret = IDNO;
                        salShExtSharedMemView->DropDone = TRUE;
                        salShExtSharedMemView->PasteDone = FALSE;
                        s = pszDestFile + lstrlen(pszDestFile);
                        while (s > pszDestFile && *s != '\\')
                            s--;
                        lstrcpyn(salShExtSharedMemView->TargetPath, pszDestFile, MAX_PATH);
                        *(salShExtSharedMemView->TargetPath + (s - pszDestFile) + 1) = 0;
                        salShExtSharedMemView->Operation = wFunc == FO_COPY ? SALSHEXT_COPY : SALSHEXT_MOVE;
                    }
                    else
                    {
                        // On Vista+ if Copy from an archive is performed in Salamander running under another user,
                        // repeated Paste in Explorer does not call GetData, so ClipDataObjLastGetDataTime is not set.
                        // Treat this case by skipping the check (side effect: dragging the CLIPFAKE directory always
                        // triggers unpacking from the archive).
                        BOOL ignoreGetDataTime = FALSE;
                        if (hwnd != NULL)
                        {
                            DWORD tgtPID = -1;
                            GetWindowThreadProcessId(hwnd, &tgtPID);
                            if (tgtPID != -1)
                                ignoreGetDataTime = AreNotProcessesOfTheSameUser(salShExtSharedMemView->SalamanderMainWndPID, tgtPID);

                            if (ignoreGetDataTime)
                                WriteToLog("CH_CopyCallback: ignoring ClipDataObjLastGetDataTime");
                            else
                                WriteToLog("CH_CopyCallback: using ClipDataObjLastGetDataTime");
                        }

#ifdef SHEXT_LOG_ENABLED
                        {
                            char xxx[200];
                            wsprintf(xxx, "CH_CopyCallback: time from last GetData()=%d",
                                     GetTickCount() - salShExtSharedMemView->ClipDataObjLastGetDataTime);
                            WriteToLog(xxx);
                        }
#endif // SHEXT_LOG_ENABLED

                        if (salShExtSharedMemView->DoPasteFromSalamander &&
                            (ignoreGetDataTime || GetTickCount() - salShExtSharedMemView->ClipDataObjLastGetDataTime < 3000) &&
                            (lstrcmpi(salShExtSharedMemView->PasteFakeDirName, pszSrcFile) == 0 ||
                             GetShortPathName(salShExtSharedMemView->PasteFakeDirName, buf1, MAX_PATH) &&
                                 GetShortPathName(pszSrcFile, buf2, MAX_PATH) && lstrcmpi(buf1, buf2) == 0))
                        {

                            WriteToLog("CH_CopyCallback: paste!");

                            // The operation targets the "fake" directory; block it so Salamander can
                            // unpack the archive or copy from the file system instead.
                            ret = IDNO;
                            if (salShExtSharedMemView->SalamanderMainWndPID == GetCurrentProcessId() &&
                                salShExtSharedMemView->SalamanderMainWndTID == GetCurrentThreadId())
                            { // we are in Salamander's main thread (and it is not a panel, that is handled separately), the operation cannot be executed ("Salamander is busy") - triggered on W2K/XP for example by Paste in the Plugins/Plugins/Add... window
                                samePIDAndTIDMsg = TRUE;
                                lstrcpyn(text, salShExtSharedMemView->ArcUnableToPaste1, 300);
                            }
                            else // another process or at least a different thread -> communication with the main window is possible
                            {
                                salShExtSharedMemView->SalBusyState = 0 /* will be used to detect whether Salamander reports itself as "busy" */;
                                salShExtDoPasteEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, SALSHEXT_DOPASTEEVENTNAME);
                                if (salShExtDoPasteEvent != NULL) // Vista+: PostMessage cannot be used between the copy-hook and Salamander running "as admin", so we use 'salShExtDoPasteEvent' instead
                                {

                                    WriteToLog("CH_CopyCallback: using salShExtDoPasteEvent");

                                    SetEvent(salShExtDoPasteEvent); // ask all running Salamander instances (2.52 beta 1 or newer) to check whether they initiated the Copy & Paste operation; the source instance posts WM_USER_SALSHEXT_PASTE to itself
                                }
                                else // on older OSes it is enough to post the WM_USER_SALSHEXT_PASTE message directly
                                {

                                    WriteToLog("CH_CopyCallback: using PostMessage");

                                    salShExtSharedMemView->BlockPasteDataRelease = TRUE;                        // probably unnecessary on W2K+: prevents fakedataobj->Release() from removing the paste data in Salamander
                                    salShExtSharedMemView->ClipDataObjLastGetDataTime = GetTickCount() - 60000; // probably unnecessary on W2K+: set the last GetData() time one minute back to let a potential subsequent data-object Release run smoothly
                                    PostMessage((HWND)salShExtSharedMemView->SalamanderMainWnd, WM_USER_SALSHEXT_PASTE,
                                                salShExtSharedMemView->PostMsgIndex, 0);
                                }

                                count = 0;
                                while (1)
                                {
                                    ReleaseMutex(salShExtSharedMemMutex);
                                    Sleep(100); // give Salamander 100 ms to react to WM_USER_SALSHEXT_PASTE
                                    WaitForSingleObject(salShExtSharedMemMutex, INFINITE);
                                    if (salShExtSharedMemView->SalBusyState != 0 || // if Salamander has already reacted
                                        ++count >= 50)                              // do not wait more than 5 seconds
                                    {
                                        break;
                                    }
                                }
                                salShExtSharedMemView->PostMsgIndex++;
                                if (salShExtDoPasteEvent == NULL) // pre-Vista systems
                                {
                                    salShExtSharedMemView->BlockPasteDataRelease = FALSE;
                                    PostMessage((HWND)salShExtSharedMemView->SalamanderMainWnd, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // report unblocking the paste data; if the data are not protected further, let them be freed
                                }
                                else
                                {
                                    ResetEvent(salShExtDoPasteEvent); // if the "source" Salamander has not done it yet, do it here (we no longer search for the "source" Salamander)
                                    CloseHandle(salShExtDoPasteEvent);
                                    salShExtDoPasteEvent = NULL;
                                }

                                if (salShExtSharedMemView->SalBusyState == 1 /* Salamander reported that it is not "busy" and is already waiting for the paste request */)
                                {
                                    salShExtSharedMemView->PasteDone = TRUE;
                                    salShExtSharedMemView->DropDone = FALSE;
                                    s = pszDestFile + lstrlen(pszDestFile);
                                    while (s > pszDestFile && *s != '\\')
                                        s--;
                                    lstrcpyn(salShExtSharedMemView->TargetPath, pszDestFile, MAX_PATH);
                                    *(salShExtSharedMemView->TargetPath + (s - pszDestFile) + 1) = 0;
                                    salShExtSharedMemView->Operation = wFunc == FO_COPY ? SALSHEXT_COPY : SALSHEXT_MOVE;
                                }
                                else
                                {
                                    salBusyMsg = TRUE;
                                    lstrcpyn(text, salShExtSharedMemView->ArcUnableToPaste2, 300);
                                    ret = IDCANCEL; // try cancel; perhaps Windows will eventually keep the data object on the clipboard for a "cancelled" move
                                }
                            }
                        }
                    }
                    UnmapViewOfFile(salShExtSharedMemView);
                }
                CloseHandle(salShExtSharedMem);
            }
            ReleaseMutex(salShExtSharedMemMutex);
            CloseHandle(salShExtSharedMemMutex);

            if (samePIDAndTIDMsg || salBusyMsg)
            {
                MessageBox(hwnd, text, "Open Salamander", MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
    return ret;
}

#undef INTERFACE
#define INTERFACE ImpICopyHookW

STDMETHODIMP CHW_QueryInterface(THIS_ REFIID riid, void** ppv)
{
    CopyHookW* ch = (CopyHookW*)This;

    WriteToLog("CHW_QueryInterface");

    // delegate to the underlying object
    return ch->m_pObj->lpVtbl->QueryInterface((IShellExt*)ch->m_pObj, riid, ppv);
}

STDMETHODIMP_(ULONG)
CHW_AddRef(THIS)
{
    CopyHookW* ch = (CopyHookW*)This;

    WriteToLog("CHW_AddRef");

    // delegate to the underlying object
    return ch->m_pObj->lpVtbl->AddRef((IShellExt*)ch->m_pObj);
}

STDMETHODIMP_(ULONG)
CHW_Release(THIS)
{
    CopyHookW* ch = (CopyHookW*)This;

    WriteToLog("CHW_Release");

    // delegate to the underlying object
    return ch->m_pObj->lpVtbl->Release((IShellExt*)ch->m_pObj);
}

STDMETHODIMP_(UINT)
CHW_CopyCallback(THIS_ HWND hwnd, UINT wFunc, UINT wFlags,
                 LPCWSTR pszSrcFile, DWORD dwSrcAttribs,
                 LPCWSTR pszDestFile, DWORD dwDestAttribs)
{
    WriteToLog("CHW_CopyCallback"); // for now this method exists only for testing purposes
    return IDYES;
}
