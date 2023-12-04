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

    // delegace
    return ch->m_pObj->lpVtbl->QueryInterface((IShellExt*)ch->m_pObj, riid, ppv);
}

STDMETHODIMP_(ULONG)
CH_AddRef(THIS)
{
    CopyHook* ch = (CopyHook*)This;

    WriteToLog("CH_AddRef");

    // delegace
    return ch->m_pObj->lpVtbl->AddRef((IShellExt*)ch->m_pObj);
}

STDMETHODIMP_(ULONG)
CH_Release(THIS)
{
    CopyHook* ch = (CopyHook*)This;

    WriteToLog("CH_Release");

    // delegace
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

// vraci TRUE jen pokud jsme zjistili uzivatele obou procesu a pokud jsou ruzni (pri
// chybe vraci FALSE)
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

    // Vista: pokud se udela Copy z archivu Salamanderovi (eskalovanem ci ne + pustenym pod
    //  jinym userem) a Paste se provede v eskalovanem (jinem nez se delalo Copy)
    // Salamanderovi, systemovy Drop, ktery se pro tuto akci pouziva nezavola tento
    // copy-hook, takze se zkopiruje jen CLIPFAKE adresar (pri Paste do browse okenek
    // eskalovaneho Salamandera se deje totez) -- Salamander by nemel bezet jako eskalovany
    // (zatim nouzovka kvuli absenci podpory UAC), prozatim bych tento problem neresil,
    // treba az si nekdo zacne stezovat (Paste do panelu Salamandera je v nasich rukach,
    // to je jiste resitelne, ale asi to vubec resit nechceme).

    // W2K: pokud se dela Copy&Paste mezi Salamandery pustenymi pod timto a jinym userem, tak musi
    // byt v procesu, kam se Pasti dostupny TEMP adresar usera procesu, ze ktereho se delalo
    // Copy, jinak wokna jen napisou chybovou hlasku, ze zdrojovy adresar neni pristupny (reakce
    // na to, ze si nemuzou sahnout na CLIPFAKE). Vzhledem k tomu, ze skoro vsichni useri jsou
    // admini, tak by se tenhle problem nemusel vubec projevit.

    CopyHook* ch = (CopyHook*)This;
    const char* s;
    char buf1[MAX_PATH];
    char buf2[MAX_PATH];
    char text[300];
    int count;

    // mutex pro pristup do sdilene pameti
    HANDLE salShExtSharedMemMutex = NULL;
    // sdilena pamet - viz struktura CSalShExtSharedMem
    HANDLE salShExtSharedMem = NULL;
    // event pro zaslani zadosti o provedeni Paste ve zdrojovem Salamanderovi (pouziva se jen ve Vista+)
    HANDLE salShExtDoPasteEvent = NULL;
    // namapovana sdilena pamet - viz struktura CSalShExtSharedMem
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

                salShExtSharedMemView = (CSalShExtSharedMem*)MapViewOfFile(salShExtSharedMem, // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
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

                        ret = IDNO; // operace s "fake" adresarem, nenechame ji provest (misto ni se provede unpack z archivu nebo copy z FS)
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
                        BOOL ignoreGetDataTime = FALSE; // na Vista+ pokud se Copy z archivu udela v Salamanderovi pustenem na jineho usera, nedochazi pri opakovanem Paste v Exploreru k volani metody GetData, tedy nenastavi se salShExtSharedMemView->ClipDataObjLastGetDataTime: resime ignoraci tohoto testu (side-effect: drag&drop CLIPFAKE adresare tim padem spousti vybalovani z archivu)
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

                            ret = IDNO; // operace s "fake" adresarem, nenechame ji provest (misto ni se provede unpack z archivu nebo copy z FS)
                            if (salShExtSharedMemView->SalamanderMainWndPID == GetCurrentProcessId() &&
                                salShExtSharedMemView->SalamanderMainWndTID == GetCurrentThreadId())
                            { // jsme v hl. threadu Salamandera (a nejde o panel, ten se resi zvlast), operaci nelze provest ("Salamander is busy") - vyvola pod W2K/XP napr. Paste v Plugins/Plugins/Add... okne
                                samePIDAndTIDMsg = TRUE;
                                lstrcpyn(text, salShExtSharedMemView->ArcUnableToPaste1, 300);
                            }
                            else // jiny process nebo aspon jiny thread -> komunikace s main-window mozna
                            {
                                salShExtSharedMemView->SalBusyState = 0 /* zjistuje se jestli Salamander neni "busy" */;
                                salShExtDoPasteEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, SALSHEXT_DOPASTEEVENTNAME);
                                if (salShExtDoPasteEvent != NULL) // Vista+: mezi copy-hookem a "as admin" spustenym Salamanderem nelze pouzit PostMessage, misto nej pouzijeme 'salShExtDoPasteEvent'
                                {

                                    WriteToLog("CH_CopyCallback: using salShExtDoPasteEvent");

                                    SetEvent(salShExtDoPasteEvent); // pozadame vsechny Salamandery (minimalne 2.52 beta 1), at si zkontroluji, jestli nejsou zdrojem Copy&Paste operace a ten co je zdrojem, si sam sobe postne message WM_USER_SALSHEXT_PASTE
                                }
                                else // na starsich OS staci primo postnout message WM_USER_SALSHEXT_PASTE
                                {

                                    WriteToLog("CH_CopyCallback: using PostMessage");

                                    salShExtSharedMemView->BlockPasteDataRelease = TRUE;                        // na W2K+ uz asi zbytecne: aby nam fakedataobj->Release() nezrusil paste-data v Salamanderovi
                                    salShExtSharedMemView->ClipDataObjLastGetDataTime = GetTickCount() - 60000; // na W2K+ uz asi zbytecne: cas posledniho GetData() nastavime o minutu zpatky, aby pripadny nasledny Release data-objektu probehl hladce
                                    PostMessage((HWND)salShExtSharedMemView->SalamanderMainWnd, WM_USER_SALSHEXT_PASTE,
                                                salShExtSharedMemView->PostMsgIndex, 0);
                                }

                                count = 0;
                                while (1)
                                {
                                    ReleaseMutex(salShExtSharedMemMutex);
                                    Sleep(100); // dame Salamanderovi 100ms na reakci na WM_USER_SALSHEXT_PASTE
                                    WaitForSingleObject(salShExtSharedMemMutex, INFINITE);
                                    if (salShExtSharedMemView->SalBusyState != 0 || // pokud uz Salamander zareagoval
                                        ++count >= 50)                              // dele nez 5 sekund cekat nebudeme
                                    {
                                        break;
                                    }
                                }
                                salShExtSharedMemView->PostMsgIndex++;
                                if (salShExtDoPasteEvent == NULL) // starsi nez Vista+
                                {
                                    salShExtSharedMemView->BlockPasteDataRelease = FALSE;
                                    PostMessage((HWND)salShExtSharedMemView->SalamanderMainWnd, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // hlasime odblokovani paste-dat, nejsou-li data dale chranena, nechame je zrusit
                                }
                                else
                                {
                                    ResetEvent(salShExtDoPasteEvent); // pokud to jeste nestihl udelat "zdrojovy" Salamander, udelame to zde (uz nehledame "zdrojoveho" Salamandera)
                                    CloseHandle(salShExtDoPasteEvent);
                                    salShExtDoPasteEvent = NULL;
                                }

                                if (salShExtSharedMemView->SalBusyState == 1 /* Salamander neni "busy" a uz ceka na zadani operace paste */)
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
                                    ret = IDCANCEL; // zkusime cancel, treba casem budou wokna nechavat dataobject na clipboardu pri "canclovanem" move
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

    // delegace
    return ch->m_pObj->lpVtbl->QueryInterface((IShellExt*)ch->m_pObj, riid, ppv);
}

STDMETHODIMP_(ULONG)
CHW_AddRef(THIS)
{
    CopyHookW* ch = (CopyHookW*)This;

    WriteToLog("CHW_AddRef");

    // delegace
    return ch->m_pObj->lpVtbl->AddRef((IShellExt*)ch->m_pObj);
}

STDMETHODIMP_(ULONG)
CHW_Release(THIS)
{
    CopyHookW* ch = (CopyHookW*)This;

    WriteToLog("CHW_Release");

    // delegace
    return ch->m_pObj->lpVtbl->Release((IShellExt*)ch->m_pObj);
}

STDMETHODIMP_(UINT)
CHW_CopyCallback(THIS_ HWND hwnd, UINT wFunc, UINT wFlags,
                 LPCWSTR pszSrcFile, DWORD dwSrcAttribs,
                 LPCWSTR pszDestFile, DWORD dwDestAttribs)
{
    WriteToLog("CHW_CopyCallback"); // zatim je tu tahle metoda jen z testovacich ucelu
    return IDYES;
}
