// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "pack.h"
#include "codetbl.h"
#include "dialogs.h"

CSystemPolicies SystemPolicies;

const int ctsNotRunning = 0x00;   // muze byt spusten
const int ctsActive = 0x01;       // tento thread je aktivni/jen dobiha
const int ctsCanTerminate = 0x02; // muze byt terminovan - uz se nainicializoval z glob. dat

HANDLE ThreadCheckPath[NUM_OF_CHECKTHREADS];
int ThreadCheckState[NUM_OF_CHECKTHREADS]; // stav jednotlivych threadu
char ThreadPath[MAX_PATH];                 // vstup aktivniho threadu
BOOL ThreadValid;                          // vysledek aktivniho threadu
DWORD ThreadLastError;                     // vysledek aktivniho threadu

CRITICAL_SECTION CheckPathCS; // kriticka sekce check-path, nutne kvuli volani z vice threadu (nejen z hl.)

// optimalizace: prvni check-path thread se neukoncuje - pouziva se opakovane
BOOL CPFirstFree = FALSE;      // je mozne pouzit prvni check-path thread?
BOOL CPFirstTerminate = FALSE; // ma se ukoncit prvni check-path thread?
HANDLE CPFirstStart = NULL;    // event pro startovani prvniho check-path threadu
HANDLE CPFirstEnd = NULL;      // event pro test ukonceni prvniho check-path threadu
DWORD CPFirstExit;             // nahrada exit-codu prvniho check-path threadu (neukoncuje se)

char CheckPathRootWithRetryMsgBox[MAX_PATH] = ""; // root drivu (i UNC), pro ktery je zobrazen messagebox "drive not ready" s Retry+Cancel tlacitky (pouziva se pro automaticke Retry po vlozeni media do drivu)
HWND LastDriveSelectErrDlgHWnd = NULL;            // dialog "drive not ready" s Retry+Cancel tlacitky (pouziva se pro automaticke Retry po vlozeni media do drivu)

DWORD WINAPI ThreadCheckPathF(void* param);

CRITICAL_SECTION OpenHtmlHelpCS; // kriticka sekce pro OpenHtmlHelp()

// neblokujici cteni volume-name CD drivu:
CRITICAL_SECTION ReadCDVolNameCS;        // kriticka sekce pro pristup k datum
UINT_PTR ReadCDVolNameReqUID = 0;        // UID pozadavku (pro rozpoznani jestli na vysledek jeste nekdo ceka)
char ReadCDVolNameBuffer[MAX_PATH] = ""; // IN/OUT buffer (root/volume_name)

struct CInitOpenHtmlHelpCS
{
    CInitOpenHtmlHelpCS() { HANDLES(InitializeCriticalSection(&OpenHtmlHelpCS)); }
    ~CInitOpenHtmlHelpCS() { HANDLES(DeleteCriticalSection(&OpenHtmlHelpCS)); }
} __InitOpenHtmlHelpCS;

BOOL InitializeCheckThread()
{
    CALL_STACK_MESSAGE_NONE
    HANDLES(InitializeCriticalSection(&CheckPathCS));
    HANDLES(InitializeCriticalSection(&ReadCDVolNameCS));

    int i;
    for (i = 0; i < NUM_OF_CHECKTHREADS; i++)
    {
        ThreadCheckPath[i] = NULL;
        ThreadCheckState[i] = ctsNotRunning;
    }

    CPFirstStart = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    CPFirstEnd = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (CPFirstStart == NULL || CPFirstEnd == NULL)
    {
        TRACE_E("Unable to create events for CheckPath.");
        return FALSE;
    }

    // pokusime se nahodit prvni check-path thread
    DWORD ThreadID;
    ThreadCheckPath[0] = HANDLES(CreateThread(NULL, 0, ThreadCheckPathF, (void*)0, 0, &ThreadID));
    if (ThreadCheckPath[0] == NULL) // nezadarilo se, ale to neva ...
    {
        TRACE_E("Unable to start the first CheckPath thread.");
    }

    return TRUE;
}

void ReleaseCheckThreads()
{
    CALL_STACK_MESSAGE_NONE
    HANDLES(DeleteCriticalSection(&ReadCDVolNameCS));
    HANDLES(DeleteCriticalSection(&CheckPathCS));

    if (CPFirstStart != NULL)
    {
        CPFirstTerminate = TRUE; // nechame ukoncit prvni check-path thread
        SetEvent(CPFirstStart);
        Sleep(100); // dame mu sanci zareagovat
    }
    int i;
    for (i = 0; i < NUM_OF_CHECKTHREADS; i++)
    {
        if (ThreadCheckPath[i] != NULL)
        {
            DWORD code;
            if (GetExitCodeThread(ThreadCheckPath[i], &code) && code == STILL_ACTIVE)
            { // uz nema co bezet, terminujeme ho
                TerminateThread(ThreadCheckPath[i], 666);
                WaitForSingleObject(ThreadCheckPath[i], INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
            }
            ThreadCheckState[i] = ctsNotRunning;
            HANDLES(CloseHandle(ThreadCheckPath[i]));
            ThreadCheckPath[i] = NULL;
        }
    }
    if (CPFirstStart != NULL)
    {
        HANDLES(CloseHandle(CPFirstStart));
        CPFirstStart = NULL;
    }
    if (CPFirstEnd != NULL)
    {
        HANDLES(CloseHandle(CPFirstEnd));
        CPFirstEnd = NULL;
    }
}

unsigned ThreadCheckPathFBody(void* param) // test pristupnosti adresare
{
    CALL_STACK_MESSAGE1("ThreadCheckPathFBody()");
    int i = (int)(INT_PTR)param;
    char threadPath[MAX_PATH + 5];

    SetThreadNameInVCAndTrace("CheckPath");
    //  if (i == 0) TRACE_I("First check-path thread: Begin");
    //  else TRACE_I("Begin");

CPF_AGAIN:

    if (i == 0) // prvni check-path thread (optimalizace: bezi stale)
    {
        CPFirstFree = TRUE;                          // pro prichod do threadu, jinak zbytecna pojistka ;-)
                                                     //    TRACE_I("First check-path thread: Wait for start");
        WaitForSingleObject(CPFirstStart, INFINITE); // cekame na odstartovani nebo ukonceni
                                                     //    TRACE_I("First check-path thread: Wait satisfied");
        CPFirstFree = FALSE;
        if (CPFirstTerminate) // ukonceni
        {
            //      TRACE_I("First check-path thread: End");
            return 0;
        }
    }
    //  TRACE_I("Testing path " << ThreadPath);

    strcpy(threadPath, ThreadPath);
    ThreadCheckState[i] |= ctsCanTerminate; // hl. threadu uz muze terminovat

    // tady to muze vytuhnout, a proto delame celou tu saskarnu kolem
    BOOL threadValid = (SalGetFileAttributes(threadPath) != 0xFFFFFFFF);
    DWORD error = GetLastError();
    if (!threadValid && error == ERROR_INVALID_PARAMETER) // hlasi na rootu removable medii (CD/DVD, ZIPka)
        error = ERROR_NOT_READY;                          // trochu prasarna, ale proste jde o problem "not ready" a ne "invalid parameter" ;-)

    // obchazime chybu pri cteni atributu (od W2K se da zakazat cteni atributu v Properties/Security) alespon na fixed discich
    if (!threadValid && error == ERROR_ACCESS_DENIED &&
        (threadPath[0] >= 'a' && threadPath[0] <= 'z' ||
         threadPath[0] >= 'A' && threadPath[0] <= 'Z') &&
        threadPath[1] == ':')
    {
        char root[20];
        root[0] = threadPath[0];
        strcpy(root + 1, ":\\");
        if (GetDriveType(root) == DRIVE_FIXED)
        {
            SalPathAppend(threadPath, "*", MAX_PATH + 5);
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(threadPath, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                // cesta je preci jen asi OK (bez testu na fixed disk nelze pouzit, bohuzel FindFirstFile
                // jede nejspis z cache, protoze odpojeny sitovy disk klidne zacne listovat, pro
                // check-path je tedy nepouzitelna (uz tu byla a museli jsme ji vymenit))
                threadValid = TRUE;
                HANDLES(FindClose(find));
            }
        }
    }

    if (i == 0) // prvni check-path thread (optimalizace: bezi stale)
    {
        CPFirstFree = TRUE; // ted uz vse probehne hladce az do WaitForSingleObject(CPFirstStart, INFINITE)
    }

    if (!threadValid && error != ERROR_SUCCESS)
    {
        //    SetThreadNameInVCAndTrace("CheckPath");
        //    TRACE_I("Error: " << GetErrorText(error));
    }

    int ret;
    if (ThreadCheckState[i] & ctsActive) // stoji hl. thread o vysledky ?
    {
        ThreadValid = threadValid;
        if (!ThreadValid)
            ThreadLastError = error;
        else
            ThreadLastError = ERROR_SUCCESS;
        ret = 0;
    }
    else
        ret = 1;

    if (i == 0) // prvni check-path thread (optimalizace: bezi stale)
    {
        CPFirstExit = ret;
        SetEvent(CPFirstEnd); // POZOR, okamzite prepne do hl. threadu (ma vyssi prioritu)

        goto CPF_AGAIN; // jdeme cekat na dalsi pozadavek
    }

    //  TRACE_I("End");
    return ret;
}

unsigned ThreadCheckPathFEH(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadCheckPathFBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread CheckPath: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadCheckPathF(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return ThreadCheckPathFEH(param);
}

DWORD SalCheckPath(BOOL echo, const char* path, DWORD err, BOOL postRefresh, HWND parent)
{
    CALL_STACK_MESSAGE5("SalCheckPath(%d, %s, 0x%X, %d, )", echo, path, err, postRefresh);
    // obrana proti vicenasobnemu volani z vice threadu
    HANDLES(EnterCriticalSection(&CheckPathCS));

    // obrana proti vicenasobnemu volani z jednoho threadu
    static BOOL called = FALSE;
    if (called)
    {
        // znamy je zatim jen pripad deaktivace/aktivace po ESC v CheckPath(), jsou i dalsi?
        HANDLES(LeaveCriticalSection(&CheckPathCS));
        TRACE_I("SalCheckPath: recursive call (in one thread) is not allowed!");
        return 666;
    }
    called = TRUE;

    BeginStopRefresh(); // aby se nevolal refresh - rekurze

    BOOL valid;
    DWORD lastError;

RETRY:

    if (err == ERROR_SUCCESS)
    {
        lstrcpyn(ThreadPath, path, MAX_PATH);

    TEST_AGAIN:

        BOOL runThread = FALSE;
        int freeThreadIndex = 0;
        if (!CPFirstFree)
        {
            freeThreadIndex = 1;
            for (; freeThreadIndex < NUM_OF_CHECKTHREADS; freeThreadIndex++)
            {
                if (ThreadCheckState[freeThreadIndex] == ctsNotRunning)
                {
                    runThread = TRUE;
                    break;
                }
                else
                {
                    if (ThreadCheckState[freeThreadIndex] & ctsActive)
                        continue;
                    else if (ThreadCheckPath[freeThreadIndex] != NULL)
                    {
                        DWORD exit;
                        if (!GetExitCodeThread(ThreadCheckPath[freeThreadIndex], &exit) ||
                            exit != STILL_ACTIVE) // uz skoncil
                        {
                            ThreadCheckState[freeThreadIndex] = ctsNotRunning;
                            HANDLES(CloseHandle(ThreadCheckPath[freeThreadIndex]));
                            ThreadCheckPath[freeThreadIndex] = NULL;
                            runThread = TRUE;
                            break;
                        }
                    }
                    else
                    {
                        ThreadCheckState[freeThreadIndex] = ctsNotRunning; // chyba
                        TRACE_E("This should never happen!");
                    }
                }
            }
        }
        else
            runThread = TRUE;

        if (!runThread)
        {
            BOOL runAsMainThread = FALSE;
            if (path[0] != '\\' && path[1] == ':')
            {
                char drive[4] = " :\\";
                drive[0] = path[0];
                runAsMainThread = (GetDriveType(drive) != DRIVE_REMOTE);
            }
            if (runAsMainThread) // neni sitovy -> do hl. threadu
            {
                valid = (SalGetFileAttributes(path) != 0xFFFFFFFF); // test pristupnosti adresare
                if (!valid)
                    lastError = GetLastError();
                else
                    lastError = ERROR_SUCCESS;
            }
            else // je sitovy -> do jednoho z vedl. threadu
            {
                Sleep(100); // tak si chvilku oddechnem a znovu to testnem
                goto TEST_AGAIN;
            }
        }
        else
        {
            DWORD ThreadID;
            BOOL success = TRUE;
            ThreadCheckState[freeThreadIndex] = ctsActive;
            if (freeThreadIndex == 0) // odstartujeme prvni check-path thread
            {
                ResetEvent(CPFirstEnd); // anulujeme pripadne predchozi ukonceni
                SetEvent(CPFirstStart); // spustime thread
            }
            else // start ostatnich
            {
                ThreadCheckPath[freeThreadIndex] = HANDLES(CreateThread(NULL, 0, ThreadCheckPathF,
                                                                        (void*)(INT_PTR)freeThreadIndex,
                                                                        0, &ThreadID));
                if (ThreadCheckPath[freeThreadIndex] == NULL)
                {
                    TRACE_E("Unable to start CheckPath thread.");
                    ThreadCheckState[freeThreadIndex] = ctsNotRunning;
                    valid = (SalGetFileAttributes(path) != 0xFFFFFFFF); // test pristupnosti adresare
                    if (!valid)
                        lastError = GetLastError();
                    else
                        lastError = ERROR_SUCCESS;
                    success = FALSE;
                }
            }

            if (success)
            {
                DWORD exit;
                GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help
                if (freeThreadIndex == 0)    // prvni check-path thread, kontrola dokonceni
                {
                    if (WaitForSingleObject(CPFirstEnd, 200) != WAIT_TIMEOUT) // 200 ms - doba hajeni
                    {
                        exit = CPFirstExit; // nahrada navratove hodnoty
                    }
                    else
                        exit = STILL_ACTIVE; // jeste bezi
                }
                else
                {
                    WaitForSingleObject(ThreadCheckPath[freeThreadIndex], 200); // 200 ms - doba hajeni
                    if (!GetExitCodeThread(ThreadCheckPath[freeThreadIndex], &exit))
                        exit = STILL_ACTIVE;
                }
                if (exit == STILL_ACTIVE) // postarame se o kill pres ESC
                {
                    // po 3 sekundach vybalime okno "ESC to cancel"
                    char buf[MAX_PATH + 100];
                    sprintf(buf, LoadStr(IDS_CHECKINGPATHESC), path);
                    CreateSafeWaitWindow(buf, NULL, 4800 + 200, TRUE, NULL);

                    while (1)
                    {
                        if (ThreadCheckState[freeThreadIndex] & ctsCanTerminate)
                        {
                            if (UserWantsToCancelSafeWaitWindow())
                            {
                                exit = 1;
                                ThreadCheckState[freeThreadIndex] &= ~ctsActive;
                                // thread se neda terminovat okamzite, vetsinou system ceka na skonceni
                                // posledniho systemoveho volani - pokud jde o sit, trva i par sekund
                                // tudiz je zbytecne TerminateThread vubec volat, thread dobehne sam stejne rychle
                                //                TerminateThread(ThreadCheckPath[freeThreadIndex], exit);
                                //                WaitForSingleObject(ThreadCheckPath[freeThreadIndex], INFINITE);  // pockame az thread skutecne skonci, nekdy mu to dost trva
                                break;
                            }
                        }

                        if (freeThreadIndex == 0) // prvni check-path thread, kontrola dokonceni
                        {
                            if (WaitForSingleObject(CPFirstEnd, 200) != WAIT_TIMEOUT) // 200 ms pred dalsim testem
                            {
                                exit = CPFirstExit; // nahrada navratove hodnoty
                            }
                            else
                                exit = STILL_ACTIVE; // jeste bezi
                        }
                        else
                        {
                            WaitForSingleObject(ThreadCheckPath[freeThreadIndex], 200); // 200 ms pred dalsim testem
                            if (!GetExitCodeThread(ThreadCheckPath[freeThreadIndex], &exit))
                                exit = STILL_ACTIVE;
                        }
                        if (exit != STILL_ACTIVE)
                            break;
                    }
                    DestroySafeWaitWindow();
                }
                if (exit == 0) // byl uspesne dokoncen
                {
                    valid = ThreadValid;
                    lastError = ThreadLastError;
                    ThreadCheckState[freeThreadIndex] = ctsNotRunning;
                    if (freeThreadIndex != 0)
                    {
                        HANDLES(CloseHandle(ThreadCheckPath[freeThreadIndex]));
                        ThreadCheckPath[freeThreadIndex] = NULL;
                    }
                }
                else // byl terminovan, nechame ho dobehnout
                {
                    valid = FALSE;
                    lastError = ERROR_USER_TERMINATED; // muj error

                    MSG msg; // vyhodime nabufferovany ESC
                    while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                        ;

                    char buf[MAX_PATH + 200];
                    sprintf(buf, LoadStr(IDS_TERMINATEDBYUSER), path);
                    SalMessageBox(parent, buf, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                }
            }
        }
    }
    else
    {
        lastError = err;
        err = ERROR_SUCCESS;
        valid = FALSE;
    }

    if ((err == ERROR_USER_TERMINATED || echo) && !valid)
    {
        switch (lastError)
        {
        case ERROR_USER_TERMINATED:
            break;

        case ERROR_NOT_READY:
        {
            char text[100 + MAX_PATH];
            char drive[MAX_PATH];
            UINT drvType;
            if (path[0] == '\\' && path[1] == '\\')
            {
                drvType = DRIVE_REMOTE;
                GetRootPath(drive, path);
                drive[strlen(drive) - 1] = 0; // nestojime o posledni '\\'
            }
            else
            {
                drive[0] = path[0];
                drive[1] = 0;
                drvType = MyGetDriveType(path);
            }
            if (drvType != DRIVE_REMOTE)
            {
                GetCurrentLocalReparsePoint(path, CheckPathRootWithRetryMsgBox);
                if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                {
                    lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                    SalPathRemoveBackslash(drive);
                }
            }
            else
                GetRootPath(CheckPathRootWithRetryMsgBox, path);
            sprintf(text, LoadStr(IDS_NODISKINDRIVE), drive);
            int msgboxRes = (int)CDriveSelectErrDlg(parent, text, path).Execute();
            CheckPathRootWithRetryMsgBox[0] = 0;
            UpdateWindow(MainWindow->HWindow);
            if (msgboxRes == IDRETRY)
                goto RETRY;
            break;
        }

        case ERROR_DIRECTORY:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_BAD_PATHNAME:
        {
            char text[MAX_PATH + 100];
            sprintf(text, LoadStr(IDS_DIRNAMEINVALID), path);
            SalMessageBox(parent, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            break;
        }

        default:
        {
            SalMessageBox(parent, GetErrorText(lastError), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            break;
        }
        }
    }

    EndStopRefresh(postRefresh);
    called = FALSE;

    HANDLES(LeaveCriticalSection(&CheckPathCS));
    return lastError;
}

BOOL SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet)
{
    CALL_STACK_MESSAGE3("SalCheckAndRestorePath(, %s, %d)", path, tryNet);
    DWORD err;
    if ((err = SalCheckPath(FALSE, path, ERROR_SUCCESS, TRUE, parent)) != ERROR_SUCCESS)
    {
        BOOL ok = FALSE;
        BOOL pathInvalid = FALSE;
        if (tryNet && err != ERROR_USER_TERMINATED)
        {
            tryNet = FALSE;
            if (LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                path[1] == ':') // normalni cesta (ne UNC)
            {
                if (CheckAndRestoreNetworkConnection(parent, path[0], pathInvalid))
                {
                    if ((err = SalCheckPath(FALSE, path, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS)
                        ok = TRUE;
                }
            }
            else // pokud user vubec nema konto na pozadovane masine
            {
                // provedeme test pristupnosti UNC cesty, pripadne nechame usera zalogovat
                if (CheckAndConnectUNCNetworkPath(parent, path, pathInvalid, FALSE))
                {
                    if ((err = SalCheckPath(FALSE, path, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS)
                        ok = TRUE;
                }
            }
        }
        if (!ok)
        {
            if (pathInvalid ||                                                // prerusene obnovovani spojeni nebo neuspesny pokus o obnoveni
                err == ERROR_USER_TERMINATED ||                               // preruseni CheckPath klavesou ESC
                SalCheckPath(TRUE, path, err, TRUE, parent) != ERROR_SUCCESS) // ostatni chyby vypiseme
            {
                return FALSE;
            }
        }
    }

    if (tryNet) // pokud jiz jsme obnovu sitoveho spojeni nezkouseli
    {
        // provedeme test pristupnosti UNC cesty, pripadne nechame usera zalogovat
        BOOL pathInvalid;
        if (CheckAndConnectUNCNetworkPath(parent, path, pathInvalid, FALSE))
        {
            if (SalCheckPath(TRUE, path, ERROR_SUCCESS, TRUE, parent) != ERROR_SUCCESS)
                return FALSE;
        }
        else
        {
            if (pathInvalid)
                return FALSE;
        }
    }

    return TRUE;
}

BOOL SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err, DWORD& lastErr,
                                   BOOL& pathInvalid, BOOL& cut, BOOL donotReconnect)
{
    CALL_STACK_MESSAGE4("SalCheckAndRestorePathWithCut(, %s, %d, , , , , %d)", path, tryNet,
                        donotReconnect);

    pathInvalid = FALSE;
    cut = FALSE;
    lastErr = ERROR_SUCCESS;
    BOOL semTimeoutOccured = FALSE;

_CHECK_AGAIN:

    while ((err = SalCheckPath(FALSE, path, ERROR_SUCCESS, TRUE, parent)) != ERROR_SUCCESS)
    {
        if (err == ERROR_SEM_TIMEOUT && !semTimeoutOccured)
        { // Vista: pri zmene fyzickeho pripojeni (napr. Wi-Fi a pak LAN) to nepochopitelne hlasi tuto chybu a na podruhe uz je vse OK, takze tenhle opruz delame za uzivatele
            semTimeoutOccured = TRUE;
            Sleep(300);
            continue;
        }
        if (err == ERROR_USER_TERMINATED)
            break;
        if (tryNet) // jeste jsme to nezkouseli
        {
            tryNet = FALSE;
            if (LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                path[1] == ':') // jde o normalni cestu (ne UNC)
            {
                if (!donotReconnect && CheckAndRestoreNetworkConnection(parent, path[0], pathInvalid))
                    continue;
            }
            else // pokud user vubec nema konto na pozadovane masine
            {
                // provedeme test pristupnosti UNC cesty, pripadne nechame usera zalogovat
                if (CheckAndConnectUNCNetworkPath(parent, path, pathInvalid, donotReconnect))
                    continue;
            }
            if (pathInvalid)
                break; // CutDirectory tomu nepomuze ...
        }
        lastErr = err;
        if (!IsDirError(err))
            break; // CutDirectory tomu nepomuze ...
        if (!CutDirectory(path))
            break;
        cut = TRUE;
    }
    // provedeme test pristupnosti UNC cesty, pripadne nechame usera zalogovat
    if (tryNet && err != ERROR_USER_TERMINATED)
    {
        tryNet = FALSE;
        if (CheckAndConnectUNCNetworkPath(parent, path, pathInvalid, donotReconnect))
            goto _CHECK_AGAIN;
    }

    return !pathInvalid && err == ERROR_SUCCESS;
}

BOOL SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                  const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                  const char* curPath, const char* curArchivePath, int* error,
                  int pathBufSize)
{
    CALL_STACK_MESSAGE7("SalParsePath(%s, , , , %s, , %d, %s, %s, , %d)", path, errorTitle,
                        curPathIsDiskOrArchive, curPath, curArchivePath, pathBufSize);

    char errBuf[3 * MAX_PATH + 300];
    type = -1;
    secondPart = NULL;
    isDir = FALSE;
    if (nextFocus != NULL)
        nextFocus[0] = 0;
    if (error != NULL)
        *error = 0;

PARSE_AGAIN:

    char fsName[MAX_PATH];
    char* fsUserPart;
    if (IsPluginFSPath(path, fsName, &fsUserPart)) // FS cesta
    {
        int index;
        int fsNameIndex;
        if (!Plugins.IsPluginFS(fsName, index, fsNameIndex))
        {
            sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_NOTPLUGINFS));
            SalMessageBox(parent, errBuf, errorTitle, MB_OK | MB_ICONEXCLAMATION);
            if (error != NULL)
                *error = SPP_NOTPLUGINFS;
            return FALSE;
        }

        type = PATH_TYPE_FS;
        secondPart = fsUserPart;
        return TRUE;
    }
    else // Windows/archive cesty
    {
        int len = (int)strlen(path);
        BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // cesta konci na backslash -> nutne adresar/archiv (a ne jmeno obyc. souboru)
        BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                           path[1] == ':'); // cesta typu "c:" musi byt i po expanzi cesta (ne soubor)

        if (nextFocus != NULL && !mustBePath) // vyber pristiho fokusu - jen "jmeno" nebo "jmeno s backslashem na konci"
        {
            char* s = strchr(path, '\\');
            if (s == NULL || *(s + 1) == 0)
            {
                int l;
                if (s != NULL)
                    l = (int)(s - path);
                else
                    l = (int)strlen(path);
                if (l < MAX_PATH)
                {
                    memcpy(nextFocus, path, l);
                    nextFocus[l] = 0;
                }
            }
        }

        int errTextID;
        const char* text = NULL;
        if (!SalGetFullName(path, &errTextID, curPathIsDiskOrArchive ? curPath : NULL, NULL, NULL,
                            pathBufSize, curPathIsDiskOrArchive))
        {
            if (errTextID == IDS_EMPTYNAMENOTALLOWED)
            {
                if (curPath == NULL) // neni cim nahradit prazdnou cestu (chapanou jako aktualni adresar)
                {
                    if (error != NULL)
                        *error = SPP_EMPTYPATHNOTALLOWED;
                }
                else
                {
                    lstrcpyn(path, curPath, pathBufSize);
                    goto PARSE_AGAIN;
                }
            }
            else
            {
                if (errTextID == IDS_INCOMLETEFILENAME)
                {
                    if (error != NULL)
                        *error = SPP_INCOMLETEPATH;
                    if (!curPathIsDiskOrArchive)
                    {
                        // vracime FALSE bez hlaseni uzivateli - vyjimka umoznujici dalsi zpracovani
                        // relativnich cest na FS
                        return FALSE;
                    }
                }
                else
                {
                    if (error != NULL)
                        *error = SPP_WINDOWSPATHERROR;
                }
            }
            text = LoadStr(errTextID);
        }
        if (text == NULL)
        {
            if (curArchivePath != NULL && StrICmp(path, curArchivePath) == 0)
            { // pomucka pro usery: operace z archivu do rootu archivu -> musi koncit na '\\', jinak pujde jen
                // o prepis existujiciho souboru
                SalPathAddBackslash(path, pathBufSize);
                backslashAtEnd = TRUE;
            }

            char root[MAX_PATH];
            GetRootPath(root, path);

            // sitove cesty nebudeme testovat, pokud jsme na ne zrovna pristupovali
            BOOL tryNet = !curPathIsDiskOrArchive || curPath == NULL || !HasTheSameRootPath(root, curPath);

            // zkontrolujeme/pripojime root cestu, pokud pojede root cesta, zbytek cesty uz snad taky pojede
            if (!SalCheckAndRestorePath(parent, root, tryNet))
            {
                if (backslashAtEnd || mustBePath)
                    SalPathAddBackslash(path, pathBufSize);
                if (error != NULL)
                    *error = SPP_WINDOWSPATHERROR;
                return FALSE;
            }

        FIND_AGAIN:
            char* end = path + strlen(path);
            char* afterRoot = path + strlen(root) - 1;
            if (*afterRoot == '\\')
                afterRoot++;
            char lastChar = 0;

            // pokud je v ceste maska, odrizneme ji bez volani SalGetFileAttributes
            BOOL hasMask = FALSE;
            if (end > afterRoot) // jeste neni jen root
            {
                char* end2 = end;
                while (*--end2 != '\\') // je jiste, ze aspon za root-cestou je jeden '\\'
                {
                    if (*end2 == '*' || *end2 == '?')
                        hasMask = TRUE;
                }
                if (hasMask) // ve jmene je maska -> orizneme
                {
                    CutSpacesFromBothSides(end2 + 1); // mezery na zacatku a konci masky jsou 100% na odstrel, hrozi jen neplecha (napr. "*.* " + "a" = "a. ")
                    end = end2;
                    lastChar = *end;
                    *end = 0;
                }
            }

            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            isDir = TRUE;

            while (end > afterRoot) // jeste neni jen root
            {
                int len2 = (int)strlen(path);
                if (path[len2 - 1] != '\\') // cesty koncici na backslash se chovaji ruzne (klasika a UNC): UNC vraci uspech, klasika ERROR_INVALID_NAME: vybalovani z archivu lezicim na UNC ceste na cestu "" hlasilo neznamy archiv (do PackerFormatConfig.PackIsArchive slo totiz napr. "...test.zip\\" misto "...test.zip")
                {
                    DWORD attrs = len2 < MAX_PATH ? SalGetFileAttributes(path) : 0xFFFFFFFF;
                    if (attrs != 0xFFFFFFFF) // tato cast cesty existuje
                    {
                        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // je to soubor
                        {
                            if (lastChar != 0 || backslashAtEnd || mustBePath) // je za jmenem archivu backslash?
                            {
                                if (PackerFormatConfig.PackIsArchive(path)) // je to archiv
                                {
                                    *end = lastChar; // opravime 'path'
                                    secondPart = end;
                                    type = PATH_TYPE_ARCHIVE;
                                    isDir = FALSE;

                                    SetCursor(oldCur);

                                    return TRUE;
                                }
                                else // mel byt archiv (je dana i cesta v souboru), zarveme
                                {
                                    text = LoadStr(IDS_NOTARCHIVEPATH);
                                    if (error != NULL)
                                        *error = SPP_NOTARCHIVEFILE;
                                    break; // ohlasime chybu
                                }
                            }
                            else // jeste se nezkracovalo + na konci neni '\\' -> jde o prepis souboru
                            {
                                // existujici cesta nema obsahovat jmeno souboru, orizneme...
                                isDir = FALSE;
                                while (*--end != '\\')
                                    ;            // je jiste, ze aspon za root-cestou je jeden '\\'
                                lastChar = *end; // aby se nezrusila cesta
                                break;           // obycejna Windows cesta - ale k souboru
                            }
                        }
                        else
                            break; // obycejna Windows cesta
                    }
                    else
                    {
                        DWORD err = len2 < MAX_PATH ? GetLastError() : ERROR_INVALID_NAME /* too long path */;
                        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_INVALID_NAME &&
                            err != ERROR_PATH_NOT_FOUND && err != ERROR_BAD_PATHNAME &&
                            err != ERROR_DIRECTORY) // divna chyba - jen vypiseme
                        {
                            text = GetErrorText(err);
                            if (error != NULL)
                                *error = SPP_WINDOWSPATHERROR;
                            break; // ohlasime chybu
                        }
                    }
                }
                *end = lastChar; // obnova 'path'
                while (*--end != '\\')
                    ; // je jiste, ze aspon za root-cestou je jeden '\\'
                lastChar = *end;
                *end = 0;
            }
            *end = lastChar; // opravime 'path'

            SetCursor(oldCur);

            if (text == NULL)
            {
                // Windows cesta
                if (*end == '\\')
                    end++;
                if (isDir && *end != 0 && !hasMask && strchr(end, '\\') == NULL)
                { // cesta konci neexistujicim adresarem (nejde o masku), ocistime jmeno od nezadoucich znaku na zacatku a konci
                    BOOL changeNextFocus = nextFocus != NULL && strcmp(nextFocus, end) == 0;
                    if (MakeValidFileName(end))
                    {
                        if (changeNextFocus)
                            strcpy(nextFocus, end);
                        goto FIND_AGAIN;
                    }
                }
                secondPart = end;
                type = PATH_TYPE_WINDOWS;
                return TRUE;
            }
        }

        sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, text);
        SalMessageBox(parent, errBuf, errorTitle, MB_OK | MB_ICONEXCLAMATION);
        if (backslashAtEnd || mustBePath)
            SalPathAddBackslash(path, pathBufSize);
        return FALSE;
    }
}

BOOL SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                         char* path, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                         const char* dirName, const char* curDiskPath, char*& mask)
{
    char root[MAX_PATH];
    GetRootPath(root, path);
    char* afterRoot = path + strlen(root) - 1;
    if (*afterRoot == '\\')
        afterRoot++;

    char newDirs[MAX_PATH];
    char textBuf[2 * MAX_PATH + 200];

    if (SalSplitGeneralPath(parent, title, errorTitle, selCount, path, afterRoot, secondPart,
                            pathIsDir, backslashAtEnd, dirName, curDiskPath, mask, newDirs, NULL))
    {
        if (mask - 1 > path && *(mask - 2) == '\\' &&
            (mask - 1 > afterRoot || *path == '\\'))           // neni root nebo je UNC root
        {                                                      // je treba odstranit zbytecny backslash z konce retezce
            memmove(mask - 2, mask - 1, 1 + strlen(mask) + 1); // '\0' + maska + '\0'
            mask--;
        }

        if (newDirs[0] != 0) // vytvorime nove adresare na cilove ceste
        {
            memmove(newDirs + (secondPart - path), newDirs, strlen(newDirs) + 1);
            memmove(newDirs, path, secondPart - path);
            SalPathRemoveBackslash(newDirs);

            BOOL ok = TRUE;
            char* st = newDirs + (secondPart - path);
            while (1)
            {
                BOOL invalidPath = *st != 0 && *st <= ' ';
                char* slash = strchr(st, '\\');
                if (slash != NULL)
                {
                    if (slash > st && (*(slash - 1) <= ' ' || *(slash - 1) == '.'))
                        invalidPath = TRUE;
                    *slash = 0;
                }
                else
                {
                    if (*st != 0)
                    {
                        char* end = st + strlen(st) - 1;
                        if (*end <= ' ' || *end == '.')
                            invalidPath = TRUE;
                    }
                }
                if (invalidPath || !CreateDirectory(newDirs, NULL))
                {
                    sprintf(textBuf, LoadStr(IDS_CREATEDIRFAILED), newDirs);
                    SalMessageBox(parent, textBuf, errorTitle, MB_OK | MB_ICONEXCLAMATION);
                    ok = FALSE;
                    break;
                }
                if (slash != NULL)
                    *slash = '\\';
                else
                    break; // to byl posledni '\\'
                st = slash + 1;
            }

            //---  refresh neautomaticky refreshovanych adresaru (probehne az po ukonceni
            // stop-refreshe, takze az po ukonceni operace)
            char changesRoot[MAX_PATH];
            memmove(changesRoot, path, secondPart - path);
            changesRoot[secondPart - path] = 0;
            // zmena cesty - vytvoreni novych podadresaru na ceste (je potreba i pokud
            // se nove adresare nepodarilo vytvorit) - zmena bez podadresaru (vytvarely se jen podadresare)
            MainWindow->PostChangeOnPathNotification(changesRoot, FALSE);

            if (!ok)
            {
                char* e = path + strlen(path); // oprava 'path' (spojeni 'path' a 'mask')
                if (e > path && *(e - 1) != '\\')
                    *e++ = '\\';
                if (e != mask)
                    memmove(e, mask, strlen(mask) + 1); // je-li potreba, prisuneme masku
                return FALSE;                           // znovu do copy/move dialogu
            }
        }
        return TRUE;
    }
    else
        return FALSE;
}

BOOL SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                         char* path, char* afterRoot, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                         const char* dirName, const char* curPath, char*& mask, char* newDirs,
                         SGP_IsTheSamePathF isTheSamePathF)
{
    mask = NULL;
    char textBuf[2 * MAX_PATH + 200];
    char tmpNewDirs[MAX_PATH];
    tmpNewDirs[0] = 0;
    if (newDirs != NULL)
        newDirs[0] = 0;

    if (pathIsDir) // existujici cast cesty je adresar
    {
        if (*secondPart != 0) // je zde i neexistujici cast cesty
        {
            // rozanalyzujeme neexistujici cast cesty - soubor/adresar + maska?
            char* s = secondPart;
            BOOL hasMask = FALSE;
            char* maskFrom = secondPart;
            while (1)
            {
                while (*s != 0 && *s != '?' && *s != '*' && *s != '\\')
                    s++;
                if (*s == '\\')
                    maskFrom = ++s;
                else
                {
                    hasMask = (*s != 0);
                    break;
                }
            }

            if (maskFrom != secondPart) // je tu nejaka cesta pred maskou
            {
                memcpy(tmpNewDirs, secondPart, maskFrom - secondPart);
                tmpNewDirs[maskFrom - secondPart] = 0;
            }

            if (hasMask)
            {
                // zajistime rozdeleni na cestu (konci backslashem) a masku
                memmove(maskFrom + 1, maskFrom, strlen(maskFrom) + 1);
                *maskFrom++ = 0;

                mask = maskFrom;
            }
            else
            {
                if (!backslashAtEnd) // jen jmeno (maska bez '*' a '?')
                {
                    if (selCount > 1 &&
                        SalMessageBox(parent, LoadStr(IDS_MOVECOPY_NONSENSE), title,
                                      MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
                    {
                        return FALSE; // znovu do copy/move dialogu
                    }

                    // zajistime rozdeleni na cestu (konci backslashem) a masku
                    memmove(maskFrom + 1, maskFrom, strlen(maskFrom) + 1);
                    *maskFrom++ = 0;

                    mask = maskFrom;
                }
                else // jmeno s lomitkem na konci -> adresar
                {
                    SalPathAppend(tmpNewDirs, maskFrom, MAX_PATH);
                    SalPathAddBackslash(path, 2 * MAX_PATH); // cesta ma vzdy koncit na backslash, zajistime to...
                    mask = path + strlen(path) + 1;
                    strcpy(mask, "*.*");
                }
            }
            CutSpacesFromBothSides(mask); // mezery na zacatku a konci masky jsou 100% na odstrel, hrozi jen neplecha

            if (tmpNewDirs[0] != 0) // zbyva jeste vytvorit ty nove adresare
            {
                if (newDirs != NULL) // vytvareni je podporovane
                {
                    strcpy(newDirs, tmpNewDirs);
                    memmove(tmpNewDirs, path, secondPart - path);
                    strcpy(tmpNewDirs + (secondPart - path), newDirs);
                    SalPathRemoveBackslash(tmpNewDirs);

                    if (Configuration.CnfrmCreatePath) // zeptame se, jestli se ma cesta vytvorit
                    {
                        BOOL dontShow = FALSE;
                        sprintf(textBuf, LoadStr(IDS_MOVECOPY_CREATEPATH), tmpNewDirs);

                        MSGBOXEX_PARAMS params;
                        memset(&params, 0, sizeof(params));
                        params.HParent = parent;
                        params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_HINT;
                        params.Caption = title;
                        params.Text = textBuf;
                        params.CheckBoxText = LoadStr(IDS_MOVECOPY_CREATEPATH_CNFRM);
                        params.CheckBoxValue = &dontShow;
                        BOOL cont = (SalMessageBoxEx(&params) != IDYES);
                        Configuration.CnfrmCreatePath = !dontShow;
                        if (cont)
                        {
                            char* e = path + strlen(path); // oprava 'path' (spojeni 'path' a 'mask')
                            if (e > path && *(e - 1) != '\\')
                                *e++ = '\\';
                            if (e != mask)
                                memmove(e, mask, strlen(mask) + 1); // je-li potreba, prisuneme masku
                            return FALSE;                           // znovu do copy/move dialogu
                        }
                    }
                }
                else
                {
                    SalMessageBox(parent, LoadStr(IDS_TARGETPATHMUSTEXIST), errorTitle, MB_OK | MB_ICONEXCLAMATION);
                    char* e = path + strlen(path); // oprava 'path' (spojeni 'path' a 'mask')
                    if (e > path && *(e - 1) != '\\')
                        *e++ = '\\';
                    if (e != mask)
                        memmove(e, mask, strlen(mask) + 1); // je-li potreba, prisuneme masku
                    return FALSE;                           // znovu do copy/move dialogu
                }
            }
            return TRUE; // opustime smycku Copy/Move dialogu a jdeme provest operaci
        }
        else // zadna neexistujici cast cesty neni (zadana cesta komplet existuje)
        {
            if (dirName != NULL && curPath != NULL &&
                !backslashAtEnd && selCount <= 1) // bez '\\' na konci cesty (force adresare) + jeden zdroj
            {
                char* name = path + strlen(path);
                while (name >= afterRoot && *(name - 1) != '\\')
                    name--;
                if (name >= afterRoot && *name != 0)
                {
                    *(name - 1) = 0;
                    if (StrICmp(dirName, name) == 0 &&
                        (isTheSamePathF != NULL && isTheSamePathF(path, curPath) ||
                         isTheSamePathF == NULL && IsTheSamePath(path, curPath)))
                    { // prejmenovani adresare na stejne jmeno (krom velikosti pismen, identita mozna)
                        // zajistime rozdeleni na cestu (konci backslashem) a masku
                        memmove(name + 1, name, strlen(name) + 1);
                        *(name - 1) = '\\';
                        *name++ = 0;

                        mask = name;
                        // CutSpacesFromBothSides(mask); // tady nelze: existuje adresar presne tohoto jmena, bez mezer uz by slo o jiny adresar (neni problem: "nelegalni" adresar existoval uz pred operaci, nic noveho "nelegalniho" nevznikne)
                        return TRUE; // opustime smycku Copy/Move dialogu a jdeme provest operaci
                    }
                    *(name - 1) = '\\';
                }
            }

            // jednoduchy cil cesty s univerzalni maskou
            SalPathAddBackslash(path, 2 * MAX_PATH); // cesta ma vzdy koncit na backslash, zajistime to...
            mask = path + strlen(path) + 1;
            strcpy(mask, "*.*");
            return TRUE; // opustime smycku Copy/Move dialogu a jdeme provest operaci
        }
    }
    else // prepis souboru - 'secondPart' ukazuje na jmeno souboru v ceste 'path'
    {
        char* nameEnd = secondPart;
        while (*nameEnd != 0 && *nameEnd != '\\')
            nameEnd++;
        if (*nameEnd == 0 && !backslashAtEnd) // prejmenovani/prepis existujiciho souboru
        {
            if (selCount > 1 &&
                SalMessageBox(parent, LoadStr(IDS_MOVECOPY_NONSENSE), title,
                              MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
            {
                return FALSE; // znovu do copy/move dialogu
            }

            // zajistime rozdeleni na cestu (konci backslashem) a masku
            memmove(secondPart + 1, secondPart, strlen(secondPart) + 1);
            *secondPart++ = 0;

            mask = secondPart;
            // CutSpacesFromBothSides(mask); // tady nelze: existuje soubor presne tohoto jmena, bez mezer uz by slo o jiny soubor (neni problem: "nelegalni" soubor existoval uz pred operaci, nic noveho "nelegalniho" nevznikne)
            return TRUE; // opustime smycku Copy/Move dialogu a jdeme provest operaci
        }
        else // cesta do archivu? tady neni mozna...
        {
            SalMessageBox(parent, LoadStr(IDS_ARCPATHNOTSUPPORTED), errorTitle, MB_OK | MB_ICONEXCLAMATION);
            if (backslashAtEnd)
                SalPathAddBackslash(path, 2 * MAX_PATH); // pokud byl '\\' oriznut, doplnime ho
            return FALSE;                                // znovu do copy/move dialogu
        }
    }
}

void MakeCopyWithBackslashIfNeeded(const char*& name, char (&nameCopy)[3 * MAX_PATH])
{
    int nameLen = (int)strlen(name);
    if (nameLen > 0 && (name[nameLen - 1] <= ' ' || name[nameLen - 1] == '.') &&
        nameLen + 1 < _countof(nameCopy))
    {
        memcpy(nameCopy, name, nameLen);
        nameCopy[nameLen] = '\\';
        nameCopy[nameLen + 1] = 0;
        name = nameCopy;
    }
}

BOOL NameEndsWithBackslash(const char* name)
{
    int nameLen = (int)strlen(name);
    return nameLen > 0 && name[nameLen - 1] == '\\';
}

BOOL FileNameIsInvalid(const char* name, BOOL isFullName, BOOL ignInvalidName)
{
    const char* s = name;
    if (isFullName && (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z') && *(s + 1) == ':')
        s += 2;
    while (*s != 0 && *s != ':')
        s++;
    if (*s == ':')
        return TRUE;
    if (ignInvalidName)
        return FALSE; // tecky a mezery na konci nas ted nezajimaji (adresar toho jmena muze existovat na disku)
    int nameLen = (int)(s - name);
    return nameLen > 0 && (name[nameLen - 1] <= ' ' || name[nameLen - 1] == '.');
}

BOOL SalMoveFile(const char* srcName, const char* destName)
{
    // pokud jmeno konci mezerou/teckou, musime pripojit '\\', jinak MoveFile
    // mezery/tecky orizne a pracuje tak s jinym jmenem
    char srcNameCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(srcName, srcNameCopy);
    char destNameCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(destName, destNameCopy);

    if (!MoveFile(srcName, destName))
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        { // mohlo by jit o problem Novellu (MoveFile vraci chybu u souboru s read-only atributem)
            DWORD attr = SalGetFileAttributes(srcName);
            if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_READONLY))
            {
                SetFileAttributes(srcName, FILE_ATTRIBUTE_ARCHIVE);
                if (MoveFile(srcName, destName))
                {
                    SetFileAttributes(destName, attr);
                    return TRUE;
                }
                else
                {
                    err = GetLastError();
                    SetFileAttributes(srcName, attr);
                }
            }
            SetLastError(err);
        }
        return FALSE;
    }
    return TRUE;
}

void RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                       BOOL* isText, char* codePage)
{
    CodeTables.Init(parent);
    CodeTables.RecognizeFileType(pattern, patternLen, forceText, isText, codePage);
}

//*****************************************************************************
//
// CSystemPolicies
//

CSystemPolicies::CSystemPolicies()
    : RestrictRunList(10, 50), DisallowRunList(10, 50)
{
    // vsechno povolime
    EnableAll();
}

CSystemPolicies::~CSystemPolicies()
{
    // uvolnime seznamy
    EnableAll();
}

void CSystemPolicies::EnableAll()
{
    NoRun = 0;
    NoDrives = 0;
    //NoViewOnDrive = 0;
    NoFind = 0;
    NoShellSearchButton = 0;
    NoNetHood = 0;
    //NoEntireNetwork = 0;
    //NoComputersNearMe = 0;
    NoNetConnectDisconnect = 0;
    RestrictRun = 0;
    DisallowRun = 0;
    NoDotBreakInLogicalCompare = 0;

    // uvolnim seznamy alokovanych string

    int i;
    for (i = 0; i < RestrictRunList.Count; i++)
        if (RestrictRunList[i] != NULL)
            free(RestrictRunList[i]);
    RestrictRunList.DetachMembers();

    for (i = 0; i < DisallowRunList.Count; i++)
        if (DisallowRunList[i] != NULL)
            free(DisallowRunList[i]);
    DisallowRunList.DetachMembers();
}

BOOL CSystemPolicies::LoadList(TDirectArray<char*>* list, HKEY hRootKey, const char* keyName)
{
    HKEY hKey;
    if (OpenKeyAux(NULL, hRootKey, keyName, hKey))
    {
        DWORD values;
        DWORD res = RegQueryInfoKey(hKey, NULL, 0, 0, NULL, NULL, NULL, &values, NULL,
                                    NULL, NULL, NULL);
        if (res == ERROR_SUCCESS)
        {
            int i;
            for (i = 0; i < (int)values; i++)
            {
                char valueName[2];
                DWORD valueNameLen = 2;
                DWORD type;
                char data[2 * MAX_PATH];
                DWORD dataLen = 2 * MAX_PATH;
                res = RegEnumValue(hKey, i, valueName, &valueNameLen, 0, &type, (BYTE*)data, &dataLen);
                if (res == ERROR_SUCCESS && type == REG_SZ)
                {
                    int len = lstrlen(data);
                    char* appName = (char*)malloc(len + 1);
                    if (appName == NULL)
                    {
                        CloseKeyAux(hKey);
                        return FALSE;
                    }
                    list->Add(appName);
                    if (!list->IsGood())
                    {
                        list->ResetState();
                        free(appName);
                        CloseKeyAux(hKey);
                        return FALSE;
                    }
                    memcpy(appName, data, len + 1);
                }
            }
        }
        CloseKeyAux(hKey);
    }
    return TRUE;
}

BOOL CSystemPolicies::FindNameInList(TDirectArray<char*>* list, const char* name)
{
    int i;
    for (i = 0; i < list->Count; i++)
        if (StrICmp(list->At(i), name) == 0)
            return TRUE;
    return FALSE;
}

BOOL CSystemPolicies::GetMyCanRun(const char* fileName)
{
    const char* p = strrchr(fileName, '\\');
    if (p == NULL)
        p = fileName;
    else
        p++;
    // zleva preskocim mezery
    while (*p != 0 && *p == ' ')
        p++;
    if (strlen(p) >= MAX_PATH)
        return RestrictRun == 0; // zakazeme spousteni pokud je povoleno spoustet jen vybrane prikazy (tento se nepodarilo separovat z prikazove radky)
    char name[MAX_PATH];
    lstrcpyn(name, p, MAX_PATH);
    // zprava oriznu mezery
    char* p2 = name + strlen(name) - 1;
    while (p2 >= name && *p2 == ' ')
    {
        *p2 = 0;
        p2--;
    }
    if (DisallowRun != 0)
    {
        if (FindNameInList(&DisallowRunList, name))
            return FALSE;
    }
    if (RestrictRun != 0)
    {
        if (!FindNameInList(&RestrictRunList, name))
            return FALSE;
    }
    return TRUE;
}

void CSystemPolicies::LoadFromRegistry()
{
    // vsechno povolime
    EnableAll();

    // vytahneme restrikce
    HKEY hKey;
    if (OpenKeyAux(NULL, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", hKey))
    {
        // podle MSDN muzou byt hodnoty typu DWORD i BINARY:
        // It is a REG_DWORD or 4-byte REG_BINARY data value, found under the same key.
        GetValueDontCheckTypeAux(hKey, "NoRun", /*REG_DWORD,*/ &NoRun, sizeof(DWORD));
        GetValueDontCheckTypeAux(hKey, "NoDrives", /*REG_DWORD,*/ &NoDrives, sizeof(DWORD));
        //GetValueDontCheckTypeAux(hKey, "NoViewOnDrive", /*REG_DWORD,*/ &NoViewOnDrive, sizeof(DWORD));
        GetValueDontCheckTypeAux(hKey, "NoFind", /*REG_DWORD,*/ &NoFind, sizeof(DWORD));
        GetValueDontCheckTypeAux(hKey, "NoShellSearchButton", /*REG_DWORD,*/ &NoShellSearchButton, sizeof(DWORD));
        GetValueDontCheckTypeAux(hKey, "NoNetHood", /*REG_DWORD,*/ &NoNetHood, sizeof(DWORD));
        //GetValueDontCheckTypeAux(hKey, "NoComputersNearMe", /*REG_DWORD,*/ &NoComputersNearMe, sizeof(DWORD));
        GetValueDontCheckTypeAux(hKey, "NoNetConnectDisconnect", /*REG_DWORD,*/ &NoNetConnectDisconnect, sizeof(DWORD));
        GetValueDontCheckTypeAux(hKey, "RestrictRun", /*REG_DWORD,*/ &RestrictRun, sizeof(DWORD));
        if (RestrictRun && !LoadList(&RestrictRunList, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\RestrictRun"))
            RestrictRun = 0; // malo pameti; zrusime tento option
        GetValueDontCheckTypeAux(hKey, "DisallowRun", /*REG_DWORD,*/ &DisallowRun, sizeof(DWORD));
        if (DisallowRun && !LoadList(&DisallowRunList, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\DisallowRun"))
            DisallowRun = 0; // malo pameti; zrusime tento option
        CloseKeyAux(hKey);
    }

    //  if (OpenKeyAux(NULL, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Network", hKey))
    //  {
    //GetValueDontCheckTypeAux(hKey, "NoEntireNetwork", /*REG_DWORD,*/ &NoEntireNetwork, sizeof(DWORD));
    //    CloseKeyAux(hKey);
    //  }

    if (OpenKeyAux(NULL, HKEY_CURRENT_USER, "SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer", hKey))
    {
        GetValueDontCheckTypeAux(hKey, "NoDotBreakInLogicalCompare", /*REG_DWORD,*/ &NoDotBreakInLogicalCompare, sizeof(DWORD));
        CloseKeyAux(hKey);
    }
    if (OpenKeyAux(NULL, HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer", hKey))
    {
        GetValueDontCheckTypeAux(hKey, "NoDotBreakInLogicalCompare", /*REG_DWORD,*/ &NoDotBreakInLogicalCompare, sizeof(DWORD));
        CloseKeyAux(hKey);
    }
}

BOOL SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err)
{
    CALL_STACK_MESSAGE1("SalGetFileSize(, ,)");
    if (file == NULL || file == INVALID_HANDLE_VALUE)
    {
        TRACE_E("SalGetFileSize(): file handle is invalid!");
        err = ERROR_INVALID_HANDLE;
        size.Set(0, 0);
        return FALSE;
    }

    BOOL ret = FALSE;
    size.LoDWord = GetFileSize(file, &size.HiDWord);
    if ((size.LoDWord != INVALID_FILE_SIZE || (err = GetLastError()) == NO_ERROR))
    {
        ret = TRUE;
        err = NO_ERROR;
    }
    else
        size.Set(0, 0);
    return ret;
}

BOOL SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err)
{
    HANDLE hFile = HANDLES_Q(CreateFile(fileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        NULL, OPEN_EXISTING, 0, NULL));
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD dummyErr;
        BOOL ret = SalGetFileSize(hFile, size, err != NULL ? *err : dummyErr);
        HANDLES(CloseHandle(hFile));
        return ret;
    }
    if (err != NULL)
        *err = GetLastError();
    size.Set(0, 0);
    return FALSE;
}

DWORD SalGetFileAttributes(const char* fileName)
{
    CALL_STACK_MESSAGE2("SalGetFileAttributes(%s)", fileName);
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak GetFileAttributes
    // mezery/tecky orizne a pracuje tak s jinou cestou + u souboru to sice nefunguje,
    // ale porad lepsi nez ziskat atributy jineho souboru/adresare (pro "c:\\file.txt   "
    // pracuje se jmenem "c:\\file.txt")
    char fileNameCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(fileName, fileNameCopy);

    return GetFileAttributes(fileName);
}

BOOL ClearReadOnlyAttr(const char* name, DWORD attr)
{
    if (attr == -1)
        attr = SalGetFileAttributes(name);
    if (attr != INVALID_FILE_ATTRIBUTES)
    {
        // shodime jen RO (u hardlinku zmeni i atributy ostatnich hardlinku na stejny soubor, tak at je to co nejmene)
        if ((attr & FILE_ATTRIBUTE_READONLY) != 0)
        {
            if (!SetFileAttributes(name, attr & ~FILE_ATTRIBUTE_READONLY))
                TRACE_E("ClearReadOnlyAttr(): error setting attrs (0x" << std::hex << (attr & ~FILE_ATTRIBUTE_READONLY) << std::dec << "): " << name);
            return TRUE;
        }
    }
    else
    {
        TRACE_E("ClearReadOnlyAttr(): error getting attrs: " << name);
        if (!SetFileAttributes(name, FILE_ATTRIBUTE_ARCHIVE)) // nelze cist atributy, zkusime aspon zapsat (uz neresime, jestli je to potreba)
            TRACE_E("ClearReadOnlyAttr(): error setting attrs (FILE_ATTRIBUTE_ARCHIVE): " << name);
        return TRUE;
    }
    return FALSE;
}

BOOL IsNetworkProviderDrive(const char* path, DWORD providerType)
{
    HANDLE hEnumNet;
    DWORD err = WNetOpenEnum(RESOURCE_CONNECTED, RESOURCETYPE_DISK,
                             RESOURCEUSAGE_CONNECTABLE, NULL, &hEnumNet);
    if (err == NO_ERROR)
    {
        char* provider = NULL;
        DWORD bufSize;
        char buf[1000];
        NETRESOURCE* netSource = (NETRESOURCE*)buf;
        while (1)
        {
            DWORD e = 1;
            bufSize = 1000;
            err = WNetEnumResource(hEnumNet, &e, netSource, &bufSize);
            if (err == NO_ERROR && e == 1)
            {
                if (path[0] == '\\')
                {
                    if (netSource->lpRemoteName != NULL &&
                        HasTheSameRootPath(path, netSource->lpRemoteName))
                    {
                        provider = netSource->lpProvider;
                        break;
                    }
                }
                else
                {
                    if (netSource->lpLocalName != NULL &&
                        LowerCase[path[0]] == LowerCase[netSource->lpLocalName[0]])
                    {
                        provider = netSource->lpProvider;
                        break;
                    }
                }
            }
            else
                break;
        }
        WNetCloseEnum(hEnumNet);

        if (provider != NULL)
        {
            NETINFOSTRUCT ni;
            memset(&ni, 0, sizeof(ni));
            ni.cbStructure = sizeof(ni);
            if (WNetGetNetworkInformation(provider, &ni) == NO_ERROR)
            {
                return ni.wNetType == HIWORD(providerType);
            }
        }
    }
    return FALSE;
}

BOOL IsNOVELLDrive(const char* path)
{
    return IsNetworkProviderDrive(path, WNNC_NET_NETWARE);
}

BOOL IsLantasticDrive(const char* path, char* lastLantasticCheckRoot, BOOL& lastIsLantasticPath)
{
    if (lastLantasticCheckRoot[0] != 0 &&
        HasTheSameRootPath(lastLantasticCheckRoot, path))
    {
        return lastIsLantasticPath;
    }

    GetRootPath(lastLantasticCheckRoot, path);
    lastIsLantasticPath = FALSE;
    if (path[0] != '\\') // neni UNC - nemusi jit o sitovou cestu (ta nemuze byt LANTASTIC)
    {
        if (GetDriveType(lastLantasticCheckRoot) != DRIVE_REMOTE)
            return FALSE; // neni sitova cesta
    }

    return lastIsLantasticPath = IsNetworkProviderDrive(lastLantasticCheckRoot, WNNC_NET_LANTASTIC);
}

BOOL IsNetworkPath(const char* path)
{
    if (path[0] != '\\' || path[1] != '\\')
    {
        char root[MAX_PATH];
        GetRootPath(root, path);
        return GetDriveType(root) == DRIVE_REMOTE;
    }
    else
        return TRUE; // UNC cesta je vzdy sitova
}

HCURSOR SetHandCursor()
{
    // pouzijeme systemovy kurzor -- zamezime zbytecnemu
    // poblikavani pri zmene kurzoru
    return SetCursor(LoadCursor(NULL, IDC_HAND));
}

void WaitForESCRelease()
{
    int c = 20; // do 1/5 sekundy pockame na pusteni ESC (aby po ESC v dialogu hned neprerusil cteni adresare)
    while (c--)
    {
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) == 0)
            break;
        Sleep(10);
    }
}

void GetListViewContextMenuPos(HWND hListView, POINT* p)
{
    if (ListView_GetItemCount(hListView) == 0)
    {
        p->x = 0;
        p->y = 0;
        ClientToScreen(hListView, p);
        return;
    }
    int focIndex = ListView_GetNextItem(hListView, -1, LVNI_FOCUSED);
    if (focIndex != -1)
    {
        if ((ListView_GetItemState(hListView, focIndex, LVNI_SELECTED) & LVNI_SELECTED) == 0)
            focIndex = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    }
    RECT cr;
    GetClientRect(hListView, &cr);
    RECT r;
    ListView_GetItemRect(hListView, 0, &r, LVIR_LABEL);
    p->x = r.left;
    if (p->x < 0)
        p->x = 0;
    if (focIndex != -1)
        ListView_GetItemRect(hListView, focIndex, &r, LVIR_BOUNDS);
    if (focIndex == -1 || r.bottom < 0 || r.bottom > cr.bottom)
        r.bottom = 0;
    p->y = r.bottom;
    ClientToScreen(hListView, p);
}

BOOL IsDeviceNameAux(const char* s, const char* end)
{
    while (end > s && *(end - 1) <= ' ')
        end--;
    // zkusime jestli to neni vyhrazene jmeno
    static const char* dev1_arr[] = {"CON", "PRN", "AUX", "NUL", NULL};
    if (end - s == 3)
    {
        const char** dev1 = dev1_arr;
        while (*dev1 != NULL)
            if (strnicmp(s, *dev1++, 3) == 0)
                return TRUE;
    }
    // zkusime jestli to neni vyhrazene jmeno nasledovane cislici '1'..'9'
    static const char* dev2_arr[] = {"COM", "LPT", NULL};
    if (end - s == 4 && *(end - 1) >= '1' && *(end - 1) <= '9')
    {
        const char** dev2 = dev2_arr;
        while (*dev2 != NULL)
            if (strnicmp(s, *dev2++, 3) == 0)
                return TRUE;
    }
    return FALSE;
}

BOOL SalIsValidFileNameComponent(const char* fileNameComponent)
{
    const char* start = fileNameComponent;
    // test white-spaces na zacatku (Petr: zakomentovano, protoze mezery na zacatku jmen souboru a adresaru proste muzou byt)
    // if (*start != 0 && *start <= ' ') return FALSE;

    // test na maximalni delku MAX_PATH-4
    const char* s = fileNameComponent + strlen(fileNameComponent);
    if (s - fileNameComponent > MAX_PATH - 4)
        return FALSE;
    // test white-spaces a '.' na konci jmena (file-system by je orizl)
    s--;
    if (s >= start && (*s <= ' ' || *s == '.'))
        return FALSE;

    BOOL testSimple = TRUE;
    BOOL simple = TRUE; // TRUE = hrozi "lpt1", "prn" a dalsi kriticky jmena, radsi doplnime '_'
    BOOL wasSpace = FALSE;

    while (*fileNameComponent != 0)
    {
        if (testSimple && *fileNameComponent > ' ' &&
            (*fileNameComponent < 'a' || *fileNameComponent > 'z') &&
            (*fileNameComponent < 'A' || *fileNameComponent > 'Z') &&
            (*fileNameComponent < '0' || *fileNameComponent > '9'))
        {
            simple = FALSE; // "prn.txt" i "prn  .txt" jsou rezervovana jmena
            testSimple = FALSE;
            if (*fileNameComponent == '.' && fileNameComponent > start &&
                IsDeviceNameAux(start, fileNameComponent))
            {
                return FALSE;
            }
        }
        if (*fileNameComponent <= ' ')
        {
            wasSpace = TRUE;
            if (*fileNameComponent != ' ')
                return FALSE; // nepovoleny white-space
        }
        else
        {
            if (testSimple && wasSpace)
            {
                simple = FALSE; // "prn bla.txt" neni rezervovane jmeno
                testSimple = FALSE;
            }
        }
        switch (*fileNameComponent)
        {
        case '*':
        case '?':
        case '\\':
        case '/':
        case '<':
        case '>':
        case '|':
        case '"':
        case ':':
            return FALSE; // nepovoleny znak
        }
        fileNameComponent++;
    }
    if (simple && IsDeviceNameAux(start, fileNameComponent))
        return FALSE; // jednoduche jmeno + device
    return TRUE;
}

void SalMakeValidFileNameComponent(char* fileNameComponent)
{
    char* start = fileNameComponent;
    BOOL testSimple = TRUE;
    BOOL simple = TRUE; // TRUE = hrozi "lpt1", "prn" a dalsi kriticky jmena, radsi doplnime '_'
    BOOL wasSpace = FALSE;
    // odstraneni white-spaces na zacatku (Petr: zakomentovano, protoze mezery na zacatku jmen souboru a adresaru proste muzou byt)
    /*
  while (*start != 0 && *start <= ' ') start++;
  if (start > fileNameComponent)
  {
    memmove(fileNameComponent, start, strlen(start) + 1);
    start = fileNameComponent;
  }
*/
    // orizneme na maximalni delku MAX_PATH-4
    char* s = fileNameComponent + strlen(fileNameComponent);
    if (s - fileNameComponent > MAX_PATH - 4)
    {
        s = fileNameComponent + (MAX_PATH - 4);
        *s = 0;
    }
    // orizneme white-spaces a '.' na konci jmena (file-system by to stejne udelal, aspon bude jasno hned)
    s--;
    while (s >= start && (*s <= ' ' || *s == '.'))
        s--;
    if (s >= start)
        *(s + 1) = 0;
    else // prazdny retezec nebo sekvence znaku '.' a white-spaces -> nahradime jmenem "_" (system tohle vsechno tez orezava)
    {
        strcpy(start, "_");
        simple = FALSE;
        testSimple = FALSE;
    }

    while (*fileNameComponent != 0)
    {
        if (testSimple && *fileNameComponent > ' ' &&
            (*fileNameComponent < 'a' || *fileNameComponent > 'z') &&
            (*fileNameComponent < 'A' || *fileNameComponent > 'Z') &&
            (*fileNameComponent < '0' || *fileNameComponent > '9'))
        {
            simple = FALSE; // "prn.txt" i "prn  .txt" jsou rezervovana jmena
            testSimple = FALSE;
            if (*fileNameComponent == '.' && fileNameComponent > start &&
                IsDeviceNameAux(start, fileNameComponent))
            {
                *fileNameComponent++ = '_';
                int len = (int)strlen(fileNameComponent);
                if ((fileNameComponent - start) + len + 1 > MAX_PATH - 4)
                    len = (int)(MAX_PATH - 4 - ((fileNameComponent - start) + 1));
                if (len > 0)
                {
                    memmove(fileNameComponent + 1, fileNameComponent, len);
                    *(fileNameComponent + len + 1) = 0;
                    *fileNameComponent = '.';
                }
                else
                {
                    *fileNameComponent = 0; // u jmen typu "prn          .txt" (s vice mezerami)
                    break;
                }
            }
        }
        if (*fileNameComponent <= ' ')
        {
            wasSpace = TRUE;
            *fileNameComponent = ' '; // vsechny white-spaces nahradime ' '
        }
        else
        {
            if (testSimple && wasSpace)
            {
                simple = FALSE; // "prn bla.txt" neni rezervovane jmeno
                testSimple = FALSE;
            }
        }
        switch (*fileNameComponent)
        {
        case '*':
        case '?':
        case '\\':
        case '/':
        case '<':
        case '>':
        case '|':
        case '"':
        case ':':
            *fileNameComponent = '_';
            break;
        }
        fileNameComponent++;
    }
    if (simple && IsDeviceNameAux(start, fileNameComponent)) // u jednoduchych jmen doplnime '_'
    {
        *fileNameComponent++ = '_';
        *fileNameComponent = 0;
    }
}

typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // must be 0x1000
    LPCSTR szName;    // pointer to name (in user addr space)
    DWORD dwThreadID; // thread ID (-1=caller thread)
    DWORD dwFlags;    // reserved for future use, must be zero
} THREADNAME_INFO;

void SetThreadNameInVC(LPCSTR szThreadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = -1 /* caller thread */;
    info.dwFlags = 0;

    __try
    {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_CONTINUE_EXECUTION)
    {
    }
}

void SetThreadNameInVCAndTrace(const char* name)
{
    SetTraceThreadName(name);
    SetThreadNameInVC(name);
}

BOOL GetOurPathInRoamingAPPDATA(char* buf)
{
    return SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0 /* SHGFP_TYPE_CURRENT */, buf) == S_OK &&
           SalPathAppend(buf, "Open Salamander", MAX_PATH);
}

BOOL CreateOurPathInRoamingAPPDATA(char* buf)
{
    static char path[MAX_PATH]; // vola se z handleru exceptiony, stack muze byt plnej
    if (buf != NULL)
        buf[0] = 0;
    if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0 /* SHGFP_TYPE_CURRENT */, path) == S_OK)
    {
        if (SalPathAppend(path, "Open Salamander", MAX_PATH))
        {
            CreateDirectory(path, NULL); // jestli selze (napr. uz existuje), neresime...
            if (buf != NULL)
                lstrcpyn(buf, path, MAX_PATH);
            return TRUE;
        }
    }
    return FALSE;
}

void SlashesToBackslashesAndRemoveDups(char* path)
{
    char* s = path - 1; // preklopime '/' na '\\' a eliminujeme zdvojene backslashe (krome zacatku, kde znamenaji UNC cestu nebo \\.\C:)
    while (*++s != 0)
    {
        if (*s == '/')
            *s = '\\';
        if (*s == '\\' && s > path + 1 && *(s - 1) == '\\')
        {
            memmove(s, s + 1, strlen(s + 1) + 1);
            s--;
        }
    }
}
