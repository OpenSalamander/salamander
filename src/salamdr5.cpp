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

const int ctsNotRunning = 0x00;   // can be started
const int ctsActive = 0x01;       // this thread is active/just finishing
const int ctsCanTerminate = 0x02; // can be terminated - already initialized from global data

HANDLE ThreadCheckPath[NUM_OF_CHECKTHREADS];
int ThreadCheckState[NUM_OF_CHECKTHREADS]; // status of individual threads
char ThreadPath[MAX_PATH];                 // Input of the active thread
BOOL ThreadValid;                          // Result of the active thread
DWORD ThreadLastError;                     // Result of the active thread

CRITICAL_SECTION CheckPathCS; // critical section check-path, necessary for calling from multiple threads (not just from the main one)

// Optimization: the first check-path thread does not terminate - it is used repeatedly
BOOL CPFirstFree = FALSE;      // Is it possible to use the first check-path thread?
BOOL CPFirstTerminate = FALSE; // Should the first check-path thread be terminated?
HANDLE CPFirstStart = NULL;    // event for starting the first check-path thread
HANDLE CPFirstEnd = NULL;      // event for testing the termination of the first check-path thread
DWORD CPFirstExit;             // replacement of the exit code of the first check-path thread (does not terminate)

char CheckPathRootWithRetryMsgBox[MAX_PATH] = ""; // Root of the drive (including UNC) for which a message box "drive not ready" is displayed with Retry+Cancel buttons (used for automatic Retry after inserting media into the drive)
HWND LastDriveSelectErrDlgHWnd = NULL;            // dialog "drive not ready" with Retry+Cancel buttons (used for automatic Retry after inserting media into the drive)

DWORD WINAPI ThreadCheckPathF(void* param);

CRITICAL_SECTION OpenHtmlHelpCS; // critical section for OpenHtmlHelp()

// non-blocking reading of the volume-name of the CD drive:
CRITICAL_SECTION ReadCDVolNameCS;        // critical section for accessing data
UINT_PTR ReadCDVolNameReqUID = 0;        // Request UID (for identifying if someone is still waiting for the result)
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

    // attempt to start the first check-path thread
    DWORD ThreadID;
    ThreadCheckPath[0] = HANDLES(CreateThread(NULL, 0, ThreadCheckPathF, (void*)0, 0, &ThreadID));
    if (ThreadCheckPath[0] == NULL) // It didn't work out, but no worries...
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
        CPFirstTerminate = TRUE; // let's terminate the first check-path thread
        SetEvent(CPFirstStart);
        Sleep(100); // Give him a chance to react
    }
    int i;
    for (i = 0; i < NUM_OF_CHECKTHREADS; i++)
    {
        if (ThreadCheckPath[i] != NULL)
        {
            DWORD code;
            if (GetExitCodeThread(ThreadCheckPath[i], &code) && code == STILL_ACTIVE)
            { // He has nothing left to run, we are terminating him
                TerminateThread(ThreadCheckPath[i], 666);
                WaitForSingleObject(ThreadCheckPath[i], INFINITE); // Wait until the thread actually finishes, sometimes it takes quite a while
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

unsigned ThreadCheckPathFBody(void* param) // test directory accessibility
{
    CALL_STACK_MESSAGE1("ThreadCheckPathFBody()");
    int i = (int)(INT_PTR)param;
    char threadPath[MAX_PATH + 5];

    SetThreadNameInVCAndTrace("CheckPath");
    //  if (i == 0) TRACE_I("First check-path thread: Begin");
    //  else TRACE_I("Begin");

CPF_AGAIN:

    if (i == 0) // first check-path thread (optimization: running continuously)
    {
        CPFirstFree = TRUE;                          // for entering the thread, otherwise unnecessary precaution ;-)
                                                     //    TRACE_I("First check-path thread: Wait for start");
        WaitForSingleObject(CPFirstStart, INFINITE); // waiting for start or finish
                                                     //    TRACE_I("First check-path thread: Wait satisfied");
        CPFirstFree = FALSE;
        if (CPFirstTerminate) // termination
        {
            //      TRACE_I("First check-path thread: End");
            return 0;
        }
    }
    //  TRACE_I("Testing path " << ThreadPath);

    strcpy(threadPath, ThreadPath);
    ThreadCheckState[i] |= ctsCanTerminate; // main thread can terminate now

    // It can freeze here, so we're doing this whole charade around it
    BOOL threadValid = (SalGetFileAttributes(threadPath) != 0xFFFFFFFF);
    DWORD error = GetLastError();
    if (!threadValid && error == ERROR_INVALID_PARAMETER) // reports removable media (CD/DVD, ZIP drive) on root
        error = ERROR_NOT_READY;                          // a bit of a mess, but it's just about the "not ready" problem and not "invalid parameter" ;-)

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
                // the path is still probably OK (cannot be used without a test on the fixed disk, unfortunately FindFirstFile
                // probably coming from cache, because when the network disk is disconnected, it can easily start paging
                // check-path is therefore unusable (it was already here and we had to replace it)
                threadValid = TRUE;
                HANDLES(FindClose(find));
            }
        }
    }

    if (i == 0) // first check-path thread (optimization: running continuously)
    {
        CPFirstFree = TRUE; // now everything will run smoothly until WaitForSingleObject(CPFirstStart, INFINITE)
    }

    if (!threadValid && error != ERROR_SUCCESS)
    {
        //    SetThreadNameInVCAndTrace("CheckPath");
        //    TRACE_I("Error: " << GetErrorText(error));
    }

    int ret;
    if (ThreadCheckState[i] & ctsActive) // Is the main thread waiting for the results?
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

    if (i == 0) // first check-path thread (optimization: running continuously)
    {
        CPFirstExit = ret;
        SetEvent(CPFirstEnd); // WARNING, immediately switches to the main thread (has higher priority)

        goto CPF_AGAIN; // we are waiting for the next request
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
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
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
    // Defense against multiple calls from multiple threads
    HANDLES(EnterCriticalSection(&CheckPathCS));

    // Defense against multiple calls from one thread
    static BOOL called = FALSE;
    if (called)
    {
        // We only know about the case of deactivation/activation after ESC in CheckPath() so far, are there any others?
        HANDLES(LeaveCriticalSection(&CheckPathCS));
        TRACE_I("SalCheckPath: recursive call (in one thread) is not allowed!");
        return 666;
    }
    called = TRUE;

    BeginStopRefresh(); // to prevent calling refresh - recursion

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
                            exit != STILL_ACTIVE) // already finished
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
                        ThreadCheckState[freeThreadIndex] = ctsNotRunning; // error
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
            if (runAsMainThread) // not networked -> to the main thread
            {
                valid = (SalGetFileAttributes(path) != 0xFFFFFFFF); // test directory accessibility
                if (!valid)
                    lastError = GetLastError();
                else
                    lastError = ERROR_SUCCESS;
            }
            else // is networked -> to one of the adjacent threads
            {
                Sleep(100); // So I'll take a short break and test it again
                goto TEST_AGAIN;
            }
        }
        else
        {
            DWORD ThreadID;
            BOOL success = TRUE;
            ThreadCheckState[freeThreadIndex] = ctsActive;
            if (freeThreadIndex == 0) // Starting the first check-path thread
            {
                ResetEvent(CPFirstEnd); // cancel any previous termination
                SetEvent(CPFirstStart); // start thread
            }
            else // start of others
            {
                ThreadCheckPath[freeThreadIndex] = HANDLES(CreateThread(NULL, 0, ThreadCheckPathF,
                                                                        (void*)(INT_PTR)freeThreadIndex,
                                                                        0, &ThreadID));
                if (ThreadCheckPath[freeThreadIndex] == NULL)
                {
                    TRACE_E("Unable to start CheckPath thread.");
                    ThreadCheckState[freeThreadIndex] = ctsNotRunning;
                    valid = (SalGetFileAttributes(path) != 0xFFFFFFFF); // test directory accessibility
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
                GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help
                if (freeThreadIndex == 0)    // first check-path thread, completion check
                {
                    if (WaitForSingleObject(CPFirstEnd, 200) != WAIT_TIMEOUT) // 200 ms - debounce time
                    {
                        exit = CPFirstExit; // return value replacement
                    }
                    else
                        exit = STILL_ACTIVE; // still running
                }
                else
                {
                    WaitForSingleObject(ThreadCheckPath[freeThreadIndex], 200); // 200 ms - debounce time
                    if (!GetExitCodeThread(ThreadCheckPath[freeThreadIndex], &exit))
                        exit = STILL_ACTIVE;
                }
                if (exit == STILL_ACTIVE) // Handle kill via ESC
                {
                    // after 3 seconds we will display the "ESC to cancel" window
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
                                // Thread cannot be terminated immediately, usually the system waits for it to finish
                                // last system call - when it comes to networking, it takes a few seconds
                                // Therefore, calling TerminateThread is unnecessary, the thread will finish on its own just as quickly
                                //                TerminateThread(ThreadCheckPath[freeThreadIndex], exit);
                                //                WaitForSingleObject(ThreadCheckPath[freeThreadIndex], INFINITE);  // wait until the thread actually finishes, sometimes it takes a while
                                break;
                            }
                        }

                        if (freeThreadIndex == 0) // first check-path thread, completion check
                        {
                            if (WaitForSingleObject(CPFirstEnd, 200) != WAIT_TIMEOUT) // 200 ms before the next test
                            {
                                exit = CPFirstExit; // return value replacement
                            }
                            else
                                exit = STILL_ACTIVE; // still running
                        }
                        else
                        {
                            WaitForSingleObject(ThreadCheckPath[freeThreadIndex], 200); // 200 ms before the next test
                            if (!GetExitCodeThread(ThreadCheckPath[freeThreadIndex], &exit))
                                exit = STILL_ACTIVE;
                        }
                        if (exit != STILL_ACTIVE)
                            break;
                    }
                    DestroySafeWaitWindow();
                }
                if (exit == 0) // successfully completed
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
                else // was terminated, let it finish
                {
                    valid = FALSE;
                    lastError = ERROR_USER_TERMINATED; // my error

                    MSG msg; // we discard the buffered ESC
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
                drive[strlen(drive) - 1] = 0; // we do not need the last '\\'
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
                path[1] == ':') // normal path (not UNC)
            {
                if (CheckAndRestoreNetworkConnection(parent, path[0], pathInvalid))
                {
                    if ((err = SalCheckPath(FALSE, path, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS)
                        ok = TRUE;
                }
            }
            else // if the user does not have an account on the desired machine at all
            {
                // we will perform a test of the accessibility of the UNC path, optionally allowing the user to log in
                if (CheckAndConnectUNCNetworkPath(parent, path, pathInvalid, FALSE))
                {
                    if ((err = SalCheckPath(FALSE, path, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS)
                        ok = TRUE;
                }
            }
        }
        if (!ok)
        {
            if (pathInvalid ||                                                // Interrupted connection recovery or unsuccessful attempt to recover
                err == ERROR_USER_TERMINATED ||                               // Interrupt CheckPath with the ESC key
                SalCheckPath(TRUE, path, err, TRUE, parent) != ERROR_SUCCESS) // Print out other errors
            {
                return FALSE;
            }
        }
    }

    if (tryNet) // if we have not already tried to restore the network connection
    {
        // we will perform a test of the accessibility of the UNC path, optionally allowing the user to log in
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
        { // In Windows: when changing physical connections (e.g. from Wi-Fi to LAN), it inexplicably reports this error, but the second time everything is fine, so we handle this nuisance for the user
            semTimeoutOccured = TRUE;
            Sleep(300);
            continue;
        }
        if (err == ERROR_USER_TERMINATED)
            break;
        if (tryNet) // we haven't tried it yet
        {
            tryNet = FALSE;
            if (LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                path[1] == ':') // It's a normal path (not UNC)
            {
                if (!donotReconnect && CheckAndRestoreNetworkConnection(parent, path[0], pathInvalid))
                    continue;
            }
            else // if the user does not have an account on the desired machine at all
            {
                // we will perform a test of the accessibility of the UNC path, optionally allowing the user to log in
                if (CheckAndConnectUNCNetworkPath(parent, path, pathInvalid, donotReconnect))
                    continue;
            }
            if (pathInvalid)
                break; // CutDirectory will not help with that ...
        }
        lastErr = err;
        if (!IsDirError(err))
            break; // CutDirectory will not help with that ...
        if (!CutDirectory(path))
            break;
        cut = TRUE;
    }
    // we will perform a test of the accessibility of the UNC path, optionally allowing the user to log in
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
    if (IsPluginFSPath(path, fsName, &fsUserPart)) // FS path
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
    else // Windows/archive paths
    {
        int len = (int)strlen(path);
        BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // path ends with a backslash -> must be a directory/archive (not a regular file name)
        BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                           path[1] == ':'); // A path of type "c:" must be a path even after expansion (not a file)

        if (nextFocus != NULL && !mustBePath) // Choose the next focus - either "name" or "name with a backslash at the end"
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
                if (curPath == NULL) // there is no way to replace an empty path (understood as the current directory)
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
                        // return FALSE without notifying the user - exception allowing further processing
                        // relative paths to the file system
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
            { // Helper for users: operation from archive to archive root -> must end with '\\', otherwise it will only go
                // overwrite an existing file
                SalPathAddBackslash(path, pathBufSize);
                backslashAtEnd = TRUE;
            }

            char root[MAX_PATH];
            GetRootPath(root, path);

            // We will not test network paths if we have not just accessed them.
            BOOL tryNet = !curPathIsDiskOrArchive || curPath == NULL || !HasTheSameRootPath(root, curPath);

            // we will check/connect the root path, if the root path works, the rest of the path should work too
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

            // if there is a mask in the path, we will cut it off without calling SalGetFileAttributes
            BOOL hasMask = FALSE;
            if (end > afterRoot) // not just root yet
            {
                char* end2 = end;
                while (*--end2 != '\\') // it is certain that at least one '\\' is behind the root path
                {
                    if (*end2 == '*' || *end2 == '?')
                        hasMask = TRUE;
                }
                if (hasMask) // in the name is a mask -> we will trim
                {
                    CutSpacesFromBothSides(end2 + 1); // Spaces at the beginning and end of the mask are 100% disposable, only a mistake is possible (e.g. "*.* " + "a" = "a. ")
                    end = end2;
                    lastChar = *end;
                    *end = 0;
                }
            }

            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            isDir = TRUE;

            while (end > afterRoot) // not just root yet
            {
                int len2 = (int)strlen(path);
                if (path[len2 - 1] != '\\') // Paths ending in a backslash behave differently (classic and UNC): UNC returns success, classic returns ERROR_INVALID_NAME: unpacking from an archive located on a UNC path to a path "" reported an unknown archive (because in PackerFormatConfig.PackIsArchive, for example, "...test.zip\\" is used instead of "...test.zip")
                {
                    DWORD attrs = len2 < MAX_PATH ? SalGetFileAttributes(path) : 0xFFFFFFFF;
                    if (attrs != 0xFFFFFFFF) // this part of the road exists
                    {
                        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // it is a file
                        {
                            if (lastChar != 0 || backslashAtEnd || mustBePath) // is there a backslash behind the archive name?
                            {
                                if (PackerFormatConfig.PackIsArchive(path)) // it's an archive
                                {
                                    *end = lastChar; // fix 'path'
                                    secondPart = end;
                                    type = PATH_TYPE_ARCHIVE;
                                    isDir = FALSE;

                                    SetCursor(oldCur);

                                    return TRUE;
                                }
                                else // should be an archive (the path in the file is also given), we will close
                                {
                                    text = LoadStr(IDS_NOTARCHIVEPATH);
                                    if (error != NULL)
                                        *error = SPP_NOTARCHIVEFILE;
                                    break; // report an error
                                }
                            }
                            else // it has not been shortened yet + there is no '\\' at the end -> it is a file rewrite
                            {
                                // existing path should not contain the file name, trim...
                                isDir = FALSE;
                                while (*--end != '\\')
                                    ;            // it is certain that at least one '\\' is behind the root path
                                lastChar = *end; // to prevent the road from being canceled
                                break;           // ordinary Windows path - but to a file
                            }
                        }
                        else
                            break; // ordinary Windows path
                    }
                    else
                    {
                        DWORD err = len2 < MAX_PATH ? GetLastError() : ERROR_INVALID_NAME /* too long path*/;
                        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_INVALID_NAME &&
                            err != ERROR_PATH_NOT_FOUND && err != ERROR_BAD_PATHNAME &&
                            err != ERROR_DIRECTORY) // weird error - just print it out
                        {
                            text = GetErrorText(err);
                            if (error != NULL)
                                *error = SPP_WINDOWSPATHERROR;
                            break; // report an error
                        }
                    }
                }
                *end = lastChar; // restore 'path'
                while (*--end != '\\')
                    ; // it is certain that at least one '\\' is behind the root path
                lastChar = *end;
                *end = 0;
            }
            *end = lastChar; // fix 'path'

            SetCursor(oldCur);

            if (text == NULL)
            {
                // Windows path
                if (*end == '\\')
                    end++;
                if (isDir && *end != 0 && !hasMask && strchr(end, '\\') == NULL)
                { // Path ends in a non-existent directory (not a mask), we will clean the name from unwanted characters at the beginning and end
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
            (mask - 1 > afterRoot || *path == '\\'))           // not root or is UNC root
        {                                                      // It is necessary to remove the unnecessary backslash from the end of the string
            memmove(mask - 2, mask - 1, 1 + strlen(mask) + 1); // '\0' + mask + '\0'
            mask--;
        }

        if (newDirs[0] != 0) // create new directories at the target path
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
                    break; // that was the last '\\'
                st = slash + 1;
            }

            //--- refresh non-automatically refreshed directories (will run after completion
            // stop-refresh, so only after the operation is finished)
            char changesRoot[MAX_PATH];
            memmove(changesRoot, path, secondPart - path);
            changesRoot[secondPart - path] = 0;
            // Change of path - creating new subdirectories on the path (needed even if
            // failed to create a new directory) - change without subdirectories (only subdirectories were created)
            MainWindow->PostChangeOnPathNotification(changesRoot, FALSE);

            if (!ok)
            {
                char* e = path + strlen(path); // fix 'path' (concatenation of 'path' and 'mask')
                if (e > path && *(e - 1) != '\\')
                    *e++ = '\\';
                if (e != mask)
                    memmove(e, mask, strlen(mask) + 1); // if needed, we will provide a mask
                return FALSE;                           // back to the copy/move dialog
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

    if (pathIsDir) // existing part of the path is a directory
    {
        if (*secondPart != 0) // there is also a non-existent part of the path
        {
            // analyze the non-existent part of the path - file/directory + mask?
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

            if (maskFrom != secondPart) // there is some path in front of the mask
            {
                memcpy(tmpNewDirs, secondPart, maskFrom - secondPart);
                tmpNewDirs[maskFrom - secondPart] = 0;
            }

            if (hasMask)
            {
                // we will ensure splitting into path (ending with a backslash) and mask
                memmove(maskFrom + 1, maskFrom, strlen(maskFrom) + 1);
                *maskFrom++ = 0;

                mask = maskFrom;
            }
            else
            {
                if (!backslashAtEnd) // just the name (mask without '*' and '?')
                {
                    if (selCount > 1 &&
                        SalMessageBox(parent, LoadStr(IDS_MOVECOPY_NONSENSE), title,
                                      MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
                    {
                        return FALSE; // back to the copy/move dialog
                    }

                    // we will ensure splitting into path (ending with a backslash) and mask
                    memmove(maskFrom + 1, maskFrom, strlen(maskFrom) + 1);
                    *maskFrom++ = 0;

                    mask = maskFrom;
                }
                else // name with a slash at the end -> directory
                {
                    SalPathAppend(tmpNewDirs, maskFrom, MAX_PATH);
                    SalPathAddBackslash(path, 2 * MAX_PATH); // The path should always end with a backslash, let's make sure of that...
                    mask = path + strlen(path) + 1;
                    strcpy(mask, "*.*");
                }
            }
            CutSpacesFromBothSides(mask); // Spaces at the beginning and end of the mask are 100% for removal, only a mishap is threatened

            if (tmpNewDirs[0] != 0) // still need to create those new directories
            {
                if (newDirs != NULL) // creation is supported
                {
                    strcpy(newDirs, tmpNewDirs);
                    memmove(tmpNewDirs, path, secondPart - path);
                    strcpy(tmpNewDirs + (secondPart - path), newDirs);
                    SalPathRemoveBackslash(tmpNewDirs);

                    if (Configuration.CnfrmCreatePath) // ask if the path should be created
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
                            char* e = path + strlen(path); // fix 'path' (concatenation of 'path' and 'mask')
                            if (e > path && *(e - 1) != '\\')
                                *e++ = '\\';
                            if (e != mask)
                                memmove(e, mask, strlen(mask) + 1); // if needed, we will provide a mask
                            return FALSE;                           // back to the copy/move dialog
                        }
                    }
                }
                else
                {
                    SalMessageBox(parent, LoadStr(IDS_TARGETPATHMUSTEXIST), errorTitle, MB_OK | MB_ICONEXCLAMATION);
                    char* e = path + strlen(path); // fix 'path' (concatenation of 'path' and 'mask')
                    if (e > path && *(e - 1) != '\\')
                        *e++ = '\\';
                    if (e != mask)
                        memmove(e, mask, strlen(mask) + 1); // if needed, we will provide a mask
                    return FALSE;                           // back to the copy/move dialog
                }
            }
            return TRUE; // we are leaving the Copy/Move dialog loop and proceeding with the operation
        }
        else // no non-existent part of the path exists (the specified path exists completely)
        {
            if (dirName != NULL && curPath != NULL &&
                !backslashAtEnd && selCount <= 1) // without '\\' at the end of the path (force directory) + one source
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
                    { // renaming a directory to the same name (except for letter case, identity possible)
                        // we will ensure splitting into path (ending with a backslash) and mask
                        memmove(name + 1, name, strlen(name) + 1);
                        *(name - 1) = '\\';
                        *name++ = 0;

                        mask = name;
                        // CutSpacesFromBothSides(mask); // here it is not possible: there is a directory with exactly this name, without spaces it would be a different directory (no problem: the "illegal" directory existed before the operation, nothing new "illegal" will be created)
                        return TRUE; // we are leaving the Copy/Move dialog loop and proceeding with the operation
                    }
                    *(name - 1) = '\\';
                }
            }

            // simple goal of the journey with a universal mask
            SalPathAddBackslash(path, 2 * MAX_PATH); // The path should always end with a backslash, let's make sure of that...
            mask = path + strlen(path) + 1;
            strcpy(mask, "*.*");
            return TRUE; // we are leaving the Copy/Move dialog loop and proceeding with the operation
        }
    }
    else // copy file - 'secondPart' points to the file name in the path 'path'
    {
        char* nameEnd = secondPart;
        while (*nameEnd != 0 && *nameEnd != '\\')
            nameEnd++;
        if (*nameEnd == 0 && !backslashAtEnd) // renaming/overwriting an existing file
        {
            if (selCount > 1 &&
                SalMessageBox(parent, LoadStr(IDS_MOVECOPY_NONSENSE), title,
                              MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
            {
                return FALSE; // back to the copy/move dialog
            }

            // we will ensure splitting into path (ending with a backslash) and mask
            memmove(secondPart + 1, secondPart, strlen(secondPart) + 1);
            *secondPart++ = 0;

            mask = secondPart;
            // CutSpacesFromBothSides(mask); // here it is not possible: there is a file with exactly this name, without spaces it would be a different file (it is not a problem: the "illegal" file already existed before the operation, nothing new "illegal" will be created)
            return TRUE; // we are leaving the Copy/Move dialog loop and proceeding with the operation
        }
        else // path to the archive? not possible here...
        {
            SalMessageBox(parent, LoadStr(IDS_ARCPATHNOTSUPPORTED), errorTitle, MB_OK | MB_ICONEXCLAMATION);
            if (backslashAtEnd)
                SalPathAddBackslash(path, 2 * MAX_PATH); // If '\\' was trimmed, we will add it back
            return FALSE;                                // back to the copy/move dialog
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
        return FALSE; // Periods and spaces at the end do not interest us now (a directory with that name may exist on the disk)
    int nameLen = (int)(s - name);
    return nameLen > 0 && (name[nameLen - 1] <= ' ' || name[nameLen - 1] == '.');
}

BOOL SalMoveFile(const char* srcName, const char* destName)
{
    // if the name ends with a space/dot, we need to append '\\', otherwise MoveFile
    // Trims spaces/dots and works with a different name
    char srcNameCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(srcName, srcNameCopy);
    char destNameCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(destName, destNameCopy);

    if (!MoveFile(srcName, destName))
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        { // It could be a problem with Novell (MoveFile returns an error for a file with a read-only attribute)
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
    // we allow everything
    EnableAll();
}

CSystemPolicies::~CSystemPolicies()
{
    // let's release the lists
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

    // release lists of allocated strings

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
    // skip spaces from the left
    while (*p != 0 && *p == ' ')
        p++;
    if (strlen(p) >= MAX_PATH)
        return RestrictRun == 0; // Disable execution if only running selected commands is allowed (this one failed to separate from the command line)
    char name[MAX_PATH];
    lstrcpyn(name, p, MAX_PATH);
    // trim spaces from the right
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
    // we allow everything
    EnableAll();

    // we will pull out restrictions
    HKEY hKey;
    if (OpenKeyAux(NULL, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", hKey))
    {
        // According to MSDN, values of type DWORD and BINARY can be used:
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
            RestrictRun = 0; // low memory; we will remove this option
        GetValueDontCheckTypeAux(hKey, "DisallowRun", /*REG_DWORD,*/ &DisallowRun, sizeof(DWORD));
        if (DisallowRun && !LoadList(&DisallowRunList, HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\DisallowRun"))
            DisallowRun = 0; // low memory; we will remove this option
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
    // if the path ends with a space/dot, we need to append '\\', otherwise GetFileAttributes
    // It trims spaces/dots and works with a different path + it doesn't work with files,
    // but still better than obtaining attributes of another file/directory (for "c:\\file.txt")
    // working with the name "c:\\file.txt")
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
        // we will only drop the RO (when hardlinking, it also changes the attributes of other hardlinks to the same file, so that it is as little as possible)
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
        if (!SetFileAttributes(name, FILE_ATTRIBUTE_ARCHIVE)) // Cannot read attributes, let's at least try to write (we don't care anymore if it's necessary)
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
    if (path[0] != '\\') // not UNC - it doesn't have to be a network path (which cannot be LANTASTIC)
    {
        if (GetDriveType(lastLantasticCheckRoot) != DRIVE_REMOTE)
            return FALSE; // not a network path
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
        return TRUE; // UNC path is always network
}

HCURSOR SetHandCursor()
{
    // we will use the system cursor -- prevent unnecessary
    // blinking when changing the cursor
    return SetCursor(LoadCursor(NULL, IDC_HAND));
}

void WaitForESCRelease()
{
    int c = 20; // Wait for 1/5 second for the ESC key to be released (so that after pressing ESC in the dialog, it does not immediately interrupt reading the directory)
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
    // Let's try if it's not a reserved name
    static const char* dev1_arr[] = {"CON", "PRN", "AUX", "NUL", NULL};
    if (end - s == 3)
    {
        const char** dev1 = dev1_arr;
        while (*dev1 != NULL)
            if (strnicmp(s, *dev1++, 3) == 0)
                return TRUE;
    }
    // Let's try if it's not a reserved name followed by a digit '1'..'9'
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
    // test white-spaces at the beginning (Petr: commented out because spaces at the beginning of file and directory names can simply exist)
    // if (*start != 0 && *start <= ' ') return FALSE;

    // test for maximum length MAX_PATH-4
    const char* s = fileNameComponent + strlen(fileNameComponent);
    if (s - fileNameComponent > MAX_PATH - 4)
        return FALSE;
    // test white-spaces and a '.' at the end of the name (file-system would trim them)
    s--;
    if (s >= start && (*s <= ' ' || *s == '.'))
        return FALSE;

    BOOL testSimple = TRUE;
    BOOL simple = TRUE; // TRUE = "lpt1", "prn" and other critical names are threatened, let's rather add '_'
    BOOL wasSpace = FALSE;

    while (*fileNameComponent != 0)
    {
        if (testSimple && *fileNameComponent > ' ' &&
            (*fileNameComponent < 'a' || *fileNameComponent > 'z') &&
            (*fileNameComponent < 'A' || *fileNameComponent > 'Z') &&
            (*fileNameComponent < '0' || *fileNameComponent > '9'))
        {
            simple = FALSE; // "prn.txt" and "prn  .txt" are reserved names
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
                return FALSE; // disallowed white-space
        }
        else
        {
            if (testSimple && wasSpace)
            {
                simple = FALSE; // "prn bla.txt" is not a reserved name
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
            return FALSE; // illegal character
        }
        fileNameComponent++;
    }
    if (simple && IsDeviceNameAux(start, fileNameComponent))
        return FALSE; // simple name + device
    return TRUE;
}

void SalMakeValidFileNameComponent(char* fileNameComponent)
{
    char* start = fileNameComponent;
    BOOL testSimple = TRUE;
    BOOL simple = TRUE; // TRUE = "lpt1", "prn" and other critical names are threatened, let's rather add '_'
    BOOL wasSpace = FALSE;
    // removing white-spaces at the beginning (Petr: commented out, because spaces at the beginning of file and directory names can simply exist)
    /*    while (*start != 0 && *start <= ' ') start++;
  if (start > fileNameComponent)
  {
    memmove(fileNameComponent, start, strlen(start) + 1);
    start = fileNameComponent;
  }*/
    // Trim to a maximum length of MAX_PATH-4
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
    else // empty string or sequence of characters '.' and white-spaces -> we replace with "_" (the system also trims all of this)
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
            simple = FALSE; // "prn.txt" and "prn  .txt" are reserved names
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
                    *fileNameComponent = 0; // for file names like "prn          .txt" (with multiple spaces)
                    break;
                }
            }
        }
        if (*fileNameComponent <= ' ')
        {
            wasSpace = TRUE;
            *fileNameComponent = ' '; // replace all white-spaces with ' '
        }
        else
        {
            if (testSimple && wasSpace)
            {
                simple = FALSE; // "prn bla.txt" is not a reserved name
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
    if (simple && IsDeviceNameAux(start, fileNameComponent)) // for simple names we will add '_'
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
    info.dwThreadID = -1 /* caller thread*/;
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
    return SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0 /* SHGFP_TYPE_CURRENT*/, buf) == S_OK &&
           SalPathAppend(buf, "Open Salamander", MAX_PATH);
}

BOOL CreateOurPathInRoamingAPPDATA(char* buf)
{
    static char path[MAX_PATH]; // It is called from the exception handler, the stack can be full
    if (buf != NULL)
        buf[0] = 0;
    if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0 /* SHGFP_TYPE_CURRENT*/, path) == S_OK)
    {
        if (SalPathAppend(path, "Open Salamander", MAX_PATH))
        {
            CreateDirectory(path, NULL); // if it fails (e.g. already exists), we don't solve...
            if (buf != NULL)
                lstrcpyn(buf, path, MAX_PATH);
            return TRUE;
        }
    }
    return FALSE;
}

void SlashesToBackslashesAndRemoveDups(char* path)
{
    char* s = path - 1; // we switch '/' to '\\' and eliminate duplicate backslashes (except at the beginning, where they represent a UNC path or \\.\C:)
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
