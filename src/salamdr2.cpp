// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "dialogs.h"
#include "tasklist.h"
#include "versinfo.h"
#include "logo.h"
#include "reglib\src\regparse.h"

// ****************************************************************************

class C__StrCriticalSection
{
public:
    CRITICAL_SECTION cs;

    C__StrCriticalSection() { HANDLES(InitializeCriticalSection(&cs)); }
    ~C__StrCriticalSection() { HANDLES(DeleteCriticalSection(&cs)); }
};

// ensure timely construction of the critical section
#pragma warning(disable : 4073)
#pragma init_seg(lib)
C__StrCriticalSection __StrCriticalSection;
C__StrCriticalSection __StrCriticalSection2;

// ****************************************************************************

char* LoadStr(int resID, HINSTANCE hInstance)
{
    static char buffer[10000]; // buffer for many strings
    static char* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    if (10000 - (act - buffer) < 200)
        act = buffer;

    if (hInstance == NULL)
        hInstance = HLanguage;
#ifdef _DEBUG
    // better make sure no one calls us before the resource handle is initialized
    if (hInstance == NULL)
        TRACE_E("LoadStr: hInstance == NULL");
#endif // _DEBUG

RELOAD:
    int size = LoadString(hInstance, resID, act, 10000 - (int)(act - buffer));
    // size contains the number of copied characters without the terminator
    //  DWORD error = GetLastError();
    char* ret;
    if (size != 0 /* || error == NO_ERROR*/) // error is NO_ERROR even if the string does not exist - useless
    {
        if ((10000 - (act - buffer) == size + 1) && (act > buffer))
        {
            // if the string was exactly at the end of the buffer, it may
            // have been truncated -- if we can move the window
            // to the beginning of the buffer, load the string once more
            act = buffer;
            goto RELOAD;
        }
        else
        {
            ret = act;
            act += size + 1;
        }
    }
    else
    {
        TRACE_E("Error in LoadStr(" << resID << ")." /*"): " << GetErrorText(error)*/);
        static char bufferError[] = "ERROR LOADING STRING";
        ret = bufferError;
    }

    HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

WCHAR* LoadStrW(int resID, HINSTANCE hInstance)
{
    static WCHAR buffer[10000]; // buffer for many strings
    static WCHAR* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    if (10000 - (act - buffer) < 200)
        act = buffer;

    if (hInstance == NULL)
        hInstance = HLanguage;
#ifdef _DEBUG
    // better make sure no one calls us before the resource handle is initialized
    if (hInstance == NULL)
        TRACE_E("LoadStrW: hInstance == NULL");
#endif // _DEBUG

RELOAD:
    int size = LoadStringW(hInstance, resID, act, 10000 - (int)(act - buffer));
    // size contains the number of copied characters without the terminator
    //  DWORD error = GetLastError();
    WCHAR* ret;
    if (size != 0 /* || error == NO_ERROR*/) // error is NO_ERROR even if the string does not exist - useless
    {
        if ((10000 - (act - buffer) == size + 1) && (act > buffer))
        {
            // if the string was exactly at the end of the buffer, it may
            // have been truncated -- if we can move the window
            // to the beginning of the buffer, load the string once more
            act = buffer;
            goto RELOAD;
        }
        else
        {
            ret = act;
            act += size + 1;
        }
    }
    else
    {
        TRACE_E("Error in LoadStrW(" << resID << ")." /*"): " << GetErrorText(error)*/);
        static wchar_t bufferError[] = L"ERROR LOADING WIDE STRING";
        ret = bufferError;
    }

    HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

//*****************************************************************************
//
// GetErrorText
//
// with at least 10 concurrently reported errors at once this should certainly work,
// we do not expect more than 10 threads at the same time ;-)

char* GetErrorText(DWORD error)
{
    static char buffer[10 * MAX_PATH]; // buffer for many strings
    static char* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection2.cs));

    if (10 * MAX_PATH - (act - buffer) < MAX_PATH + 20)
        act = buffer;

    char* ret = act;
    // NOTE: sprintf_s fills the entire buffer in the debug build, so we cannot pass it the whole buffer (it contains
    // other strings as well); either handle it via _CrtSetDebugFillThreshold or provide a smaller size)
    int l = sprintf(act, ((int)error < 0 ? "(%08X) " : "(%d) "), error);
    int fl;
    if ((fl = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                            NULL,
                            error,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            act + l,
                            MAX_PATH + 20 - l,
                            NULL)) == 0 ||
        *(act + l) == 0)
    {
        if ((int)error < 0)
            act += sprintf(act, "System error %08X, text description is not available.", error) + 1;
        else
            act += sprintf(act, "System error %u, text description is not available.", error) + 1;
    }
    else
        act += l + fl + 1;

    HANDLES(LeaveCriticalSection(&__StrCriticalSection2.cs));

    return ret;
}

WCHAR* GetErrorTextW(DWORD error)
{
    static WCHAR buffer[10 * MAX_PATH]; // buffer for many strings
    static WCHAR* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection2.cs));

    if (10 * MAX_PATH - (act - buffer) < MAX_PATH + 20)
        act = buffer;

    WCHAR* ret = act;
    // NOTE: swprintf_s fills the entire buffer in the debug build, so we cannot pass it the whole buffer (it contains
    // other strings as well); either handle it via _CrtSetDebugFillThreshold or provide a smaller size)
    int l = swprintf(act, _countof(buffer) - (act - buffer), ((int)error < 0 ? L"(%08X) " : L"(%d) "), error);
    int fl;
    if ((fl = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
                             NULL,
                             error,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             act + l,
                             MAX_PATH + 20 - l,
                             NULL)) == 0 ||
        *(act + l) == 0)
    {
        if ((int)error < 0)
            act += swprintf(act, _countof(buffer) - (act - buffer), L"System error %08X, text description is not available.", error) + 1;
        else
            act += swprintf(act, _countof(buffer) - (act - buffer), L"System error %u, text description is not available.", error) + 1;
    }
    else
        act += l + fl + 1;

    HANDLES(LeaveCriticalSection(&__StrCriticalSection2.cs));

    return ret;
}

// ****************************************************************************

void ClearComboboxListbox(HWND hCombo)
{
    // keep the current text; clear the listbox
    char buff[3000];
    GetWindowText(hCombo, buff, 3000);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    SetWindowText(hCombo, buff);
}

// ****************************************************************************

BOOL SalamanderActive()
{
    HWND foreground = GetForegroundWindow();
    if (foreground == NULL)
        return TRUE; // during the Salamander activation blocked by the wait window, GetForegroundWindow() returns NULL
    DWORD pid;
    GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
    //  return MainWindow != NULL && foreground == MainWindow->HWindow;
}

// ****************************************************************************

BOOL SafeWaitMessageThreadStarted = FALSE;
DWORD SafeWaitMessageThreadID = 0;
char* SafeWaitMessageText = NULL;
char* SafeWaitMessageCaption = NULL;
CRITICAL_SECTION SafeWaitMessageTextSection; // for synchronizing access to SafeWaitMessageText
BOOL SafeWaitMessageCallerSet = FALSE;
unsigned SafeWaitMessageCallerID = 0;
BOOL SafeWaitWindowClosePressed = FALSE;

class C__SafeWaitMessageCallerSetSection
{
public:
    CRITICAL_SECTION cs;

    C__SafeWaitMessageCallerSetSection() { HANDLES(InitializeCriticalSection(&cs)); }
    ~C__SafeWaitMessageCallerSetSection() { HANDLES(DeleteCriticalSection(&cs)); }
};

C__SafeWaitMessageCallerSetSection SafeWaitMessageCallerSetSection;

void ThreadSafeWaitWindowFBody(BOOL showCloseButton)
{
    CALL_STACK_MESSAGE1("ThreadSafeWaitWindowFBody()");
    SetThreadNameInVCAndTrace("SafeWaitWindow");
    TRACE_I("Begin");

    CWaitWindow waitWnd(NULL, 0, showCloseButton, ooStatic);
    MSG msg;
    UINT_PTR timer = 0;
    BOOL run = TRUE;
    HWND hForegroundWnd = NULL;
    while (run && GetMessage(&msg, NULL, 0, 0))
    {
        switch (msg.message)
        {
        case WM_USER_CREATEWAITWND:
        {
            hForegroundWnd = (HWND)msg.wParam; // if it is not NULL, open the window only if hForegroundWnd is active
            if ((int)msg.lParam > 0)           // if the delay > 0
            {
                if (timer != 0)
                {
                    // kill the timer
                    KillTimer(NULL, timer);
                    // clear the message queue of any WM_TIMER messages
                    MSG msg2;
                    while (PeekMessage(&msg2, NULL, WM_TIMER, WM_TIMER, PM_REMOVE))
                        ;
                    timer = 0;
                }
                timer = SetTimer(NULL, 0, (UINT)msg.lParam, NULL);
                if (timer != 0)
                    break;
            }
        }
        case WM_TIMER: // when delay == 0, WM_USER_CREATEWAITWND arrives here as well
        {
            if (msg.message == WM_USER_CREATEWAITWND || msg.wParam == timer)
            {
                if (timer != 0)
                {
                    // kill the timer
                    KillTimer(NULL, timer);
                    // clear the message queue of any WM_TIMER messages
                    MSG msg2;
                    while (PeekMessage(&msg2, NULL, WM_TIMER, WM_TIMER, PM_REMOVE))
                        ;
                    timer = 0;
                }

                if (waitWnd.HWindow == NULL)
                {
                    HANDLES(EnterCriticalSection(&SafeWaitMessageTextSection));
                    if (SafeWaitMessageText != NULL)
                        waitWnd.SetText(SafeWaitMessageText);
                    waitWnd.SetCaption(SafeWaitMessageCaption);
                    HANDLES(LeaveCriticalSection(&SafeWaitMessageTextSection));
                    waitWnd.Create(hForegroundWnd);
                }

                BOOL showWindow;
                if (hForegroundWnd != NULL)
                {
                    // show the window only when hForegroundWnd is active
                    showWindow = GetForegroundWindow() == hForegroundWnd;
                }
                else
                {
                    // check whether any window of the process is active; otherwise do not show the window
                    HWND foreground = GetForegroundWindow();
                    DWORD pid;
                    GetWindowThreadProcessId(foreground, &pid);
                    showWindow = pid == GetCurrentProcessId();
                }

                // seemingly redundant call to SetWindowPos twice, but otherwise the window unfortunately stays
                // all the way at the bottom (above the desktop but below all other windows)
                SetWindowPos(waitWnd.HWindow, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                SetWindowPos(waitWnd.HWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                             (showWindow ? SWP_SHOWWINDOW : 0) | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
            break;
        }

        case WM_USER_SHOWWAITWND:
        {
            if (waitWnd.HWindow == NULL) // if the window does not exist yet, there is nothing to do
                break;
            BOOL show = (BOOL)msg.wParam;
            if (show)
            {
                // postpone the display for a brief moment; the reason is the case where the display
                // happens because hForegroundWnd is activated after closing a MessageBox;
                // if the user interrupted the operation there, an immediate display would
                // cause a brief flash of the wait window and its immediate destruction;
                // the delay prevents that
                if (timer == 0)
                    timer = SetTimer(NULL, 0, 100, NULL); // show the window again after 100ms
            }
            else
                ShowWindow(waitWnd.HWindow, SW_HIDE);
            break;
        }

        case WM_USER_SETWAITMSG:
        {
            HANDLES(EnterCriticalSection(&SafeWaitMessageTextSection));
            if (SafeWaitMessageText != NULL)
                waitWnd.SetText(SafeWaitMessageText);
            HANDLES(LeaveCriticalSection(&SafeWaitMessageTextSection));
            break;
        }
            /*
      case WM_USER_ACTIVATEWAITMSG:
      {
        if (waitWnd.HWindow != NULL)   // only if the window is open
        {
          // seemingly redundant call to SetWindowPos twice, but otherwise the window unfortunately stays
          // all the way at the bottom (above the desktop but below all other windows)

          // It is necessary to show the window only in the second operation,
          // because changing the Z-order while the window is shown means the window loses
          // its cached bitmap (it has CS_SAVEBITS set) and after it closes
          // it triggers a repaint of the windows beneath it.

          BOOL visible = IsWindowVisible(waitWnd.HWindow);
          SetWindowPos(waitWnd.HWindow, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
          SetWindowPos(waitWnd.HWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                       visible ? SWP_SHOWWINDOW : 0 | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
        break;
      }
*/
        case WM_USER_DESTROYWAITWND:
        {
            if (timer != 0)
            {
                // kill the timer
                KillTimer(NULL, timer);
                // clear the message queue of any WM_TIMER messages
                MSG msg2;
                while (PeekMessage(&msg2, NULL, WM_TIMER, WM_TIMER, PM_REMOVE))
                    ;
                timer = 0;
            }
            if (waitWnd.HWindow != NULL) // only if the window is open
            {
                DestroyWindow(waitWnd.HWindow);
                waitWnd.HWindow = NULL;
            }
            if (msg.wParam)
                run = FALSE; // terminate the thread
            hForegroundWnd = NULL;
            break;
        }

        default:
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;
        }
        }
    }

    HANDLES(DeleteCriticalSection(&SafeWaitMessageTextSection));
    SafeWaitMessageThreadStarted = FALSE; // we have finished
    if (SafeWaitMessageText != NULL)
    {
        free(SafeWaitMessageText);
        SafeWaitMessageText = NULL;
    }
    if (SafeWaitMessageCaption != NULL)
    {
        free(SafeWaitMessageCaption);
        SafeWaitMessageCaption = NULL;
    }
    TRACE_I("End");
}

void ThreadSafeWaitWindowFEH(BOOL showCloseButton)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        ThreadSafeWaitWindowFBody(showCloseButton);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread SafeWaitWindow: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // more forceful exit (this one still invokes something)
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadSafeWaitWindowF(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    ThreadSafeWaitWindowFEH((BOOL)(UINT_PTR)param);
    return 0;
}

void CreateSafeWaitWindow(const char* message, const char* caption,
                          int delay, BOOL showCloseButton, HWND hForegroundWnd)
{
    HANDLES(EnterCriticalSection(&SafeWaitMessageCallerSetSection.cs));
    if (!SafeWaitMessageCallerSet) // only one message (check for availability)
    {
        SafeWaitMessageCallerSet = TRUE;
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        SafeWaitWindowClosePressed = FALSE;
        SafeWaitMessageCallerID = GetCurrentThreadId();
        if (!SafeWaitMessageThreadStarted) // the thread is not running
        {
            HANDLE thread = HANDLES(CreateThread(NULL, 0, ThreadSafeWaitWindowF,
                                                 (void*)(UINT_PTR)showCloseButton, 0, &SafeWaitMessageThreadID));
            if (thread == NULL)
            {
                TRACE_E("Unable to start ThreadSafeWaitWindow thread.");
                return;
            }
            SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL); // so it actually wins against the main thread
            AddAuxThread(thread);
            HANDLES(InitializeCriticalSection(&SafeWaitMessageTextSection));
            SafeWaitMessageThreadStarted = TRUE;
        }

        HANDLES(EnterCriticalSection(&SafeWaitMessageTextSection));
        if (SafeWaitMessageText != NULL)
            free(SafeWaitMessageText);
        SafeWaitMessageText = DupStr(message);
        if (SafeWaitMessageCaption != NULL)
            free(SafeWaitMessageCaption);
        SafeWaitMessageCaption = DupStr(caption);
        HANDLES(LeaveCriticalSection(&SafeWaitMessageTextSection));

        while (PostThreadMessage(SafeWaitMessageThreadID, WM_USER_CREATEWAITWND, (WPARAM)hForegroundWnd, delay) == 0)
        {
            if (GetLastError() == ERROR_INVALID_THREAD_ID)
                Sleep(100); // not started yet, wait
            else
                break; // different error
        }
    }
    else
    { // can happen - if the internal viewer searches and switches to the main window, it tries to bring up another safe-wait window and the message appears
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        TRACE_I("Incorrect call to CreateSafeWaitWindow() from " << (SafeWaitMessageCallerID == GetCurrentThreadId() ? "owner" : "strange") << " thread");
    }
}

void DestroySafeWaitWindow(BOOL killThread)
{
    HANDLES(EnterCriticalSection(&SafeWaitMessageCallerSetSection.cs));
    if (killThread ||                                        // kill applies to everyone
        SafeWaitMessageCallerSet &&                          // the window is created
            SafeWaitMessageCallerID == GetCurrentThreadId()) // this is the thread that opened it
    {
        SafeWaitMessageCallerSet = FALSE;
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        SafeWaitMessageCallerID = 0;

        if (SafeWaitMessageThreadStarted) // the thread is running; hide the window and possibly destroy it
        {
            PostThreadMessage(SafeWaitMessageThreadID, WM_USER_DESTROYWAITWND, killThread, 0);
        }
    }
    else
    { // can happen - if the internal viewer searches and switches to the main window, it tries to bring up another safe-wait window and the message appears
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        TRACE_I("Incorrect call to DestroySafeWaitWindow()");
    }
}

BOOL GetSafeWaitWindowClosePressed()
{
    HANDLES(EnterCriticalSection(&SafeWaitMessageCallerSetSection.cs));
    if (SafeWaitMessageCallerSet &&                      // the window is created
        SafeWaitMessageCallerID == GetCurrentThreadId()) // this is the thread that opened it
    {
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        if (SafeWaitMessageThreadStarted) // the thread is running
        {
            return SafeWaitWindowClosePressed;
        }
    }
    else
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
    // for example, if the wait window is open for the internal viewer and the panel
    // (Ctrl+Shift+F10) requests another wait window, the new window does not appear;
    // at the same time, we must not return TRUE to the main Salamander thread when Close
    // button is clicked in the viewer's wait window
    return FALSE;
}

BOOL UserWantsToCancelSafeWaitWindow()
{
    return (GetAsyncKeyState(VK_ESCAPE) & 0x8001) && SalamanderActive() || GetSafeWaitWindowClosePressed();
}

void ShowSafeWaitWindow(BOOL show)
{
    HANDLES(EnterCriticalSection(&SafeWaitMessageCallerSetSection.cs));
    if (SafeWaitMessageCallerSet &&                      // the window is created
        SafeWaitMessageCallerID == GetCurrentThreadId()) // this is the thread that opened it
    {
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        if (SafeWaitMessageThreadStarted) // the thread is running; send a command to show or hide
        {
            PostThreadMessage(SafeWaitMessageThreadID, WM_USER_SHOWWAITWND, show, 0);
            // we must reset the pressed button and this is a good opportunity,
            // because the user clearly noticed it was pressed (otherwise they would not call us now)
            SafeWaitWindowClosePressed = FALSE;
        }
    }
    else
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
}

void SetSafeWaitWindowText(const char* message)
{
    HANDLES(EnterCriticalSection(&SafeWaitMessageCallerSetSection.cs));
    if (SafeWaitMessageCallerSet &&                      // the window is created
        SafeWaitMessageCallerID == GetCurrentThreadId()) // this is the thread that opened it
    {
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
        if (SafeWaitMessageThreadStarted) // the thread is running; send a command to show or hide
        {
            HANDLES(EnterCriticalSection(&SafeWaitMessageTextSection));
            if (SafeWaitMessageText != NULL)
                free(SafeWaitMessageText);
            SafeWaitMessageText = DupStr(message);
            HANDLES(LeaveCriticalSection(&SafeWaitMessageTextSection));
            PostThreadMessage(SafeWaitMessageThreadID, WM_USER_SETWAITMSG, 0, 0);
        }
    }
    else
        HANDLES(LeaveCriticalSection(&SafeWaitMessageCallerSetSection.cs));
}

// ****************************************************************************

BOOL FileExists(const char* fileName)
{
    /*
  // beware of GENERIC_READ: it would return FALSE for files that have all permissions removed
  // j.r. note: without GENERIC_READ (with value 0) NT4 always returns TRUE for files on UNC paths
  // the solution would probably be to set GENERIC_READ and check what error occurred via GetLastError()
  HANDLE hFile = HANDLES_Q(CreateFile(fileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL));
  if (hFile == INVALID_HANDLE_VALUE)
  {
    return FALSE;
  }
  else
  {
    HANDLES(CloseHandle(hFile));
    return TRUE;
  }
  */
    // forget that; we will do it via attributes
    //
    // j.r. FIXME: discuss with Petr; I tried removing the file's right to read
    // attributes, but SalGetFileAttributes() still does not report an error. How is that possible?
    DWORD attr = SalGetFileAttributes(fileName);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

BOOL DirExists(const char* dirName)
{
    // j.r. FIXME: discuss with Petr; I tried removing the file's right to read
    // attributes, but SalGetFileAttributes() still does not report an error. How is that possible?
    DWORD attr = SalGetFileAttributes(dirName);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

// ****************************************************************************

BOOL DoExpandVarString(HWND msgParent, const char* varText, BOOL validateOnly, int& errorPos1,
                       int& errorPos2, char* buffer, int bufferLen, const CSalamanderVarStrEntry* variables,
                       void* param, DWORD* varPlacements, int* varPlacementsCount,
                       BOOL detectMaxVarWidths, int* maxVarWidths, int maxVarWidthsCount,
                       BOOL ignoreEnvVarNotFoundOrTooLong)
{
    char buf[MAX_PATH];
    const char* s = varText;
    char* out = buffer;
    char* outEnd = buffer + bufferLen;
    int varPlacementIndex = 0;
    int varPlacementIndexCount = (varPlacementsCount != NULL) ? *varPlacementsCount : 0;
    int currentMaxVarIndex = 0;

    if (varPlacementIndexCount > 0 && varPlacements == NULL)
    {
        TRACE_E("DoExpandVarString(): *varPlacementsCount is greater than 0 and varPlacements is NULL!");
        return FALSE;
    }
    if (maxVarWidthsCount > 0 && maxVarWidths == NULL)
    {
        TRACE_E("DoExpandVarString(): maxVarWidthsCount is greater than 0 and maxVarWidths is NULL!");
        return FALSE;
    }

    while (*s != 0)
    {
        if (*s == '$')
        {
            if (*++s != 0)
            {
                const char* value;
                DWORD valueOutLen = 0; // output width of the variable (0 = normal width)
                BOOL detectMax = FALSE;
                switch (*s)
                {
                case '$':
                {
                    value = "$";
                    s++;
                    break;
                }

                case '(':
                {
                    const char* var = s + 1;
                    while (*s != ')' && *s != 0)
                        s++;
                    if (*s == 0)
                    {
                        const char* text = LoadStr(IDS_EXP_UNMATCHEDPAR);
                        if (msgParent == NULL)
                            TRACE_I(text);
                        else
                        {
                            SalMessageBox(msgParent, text,
                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                        errorPos1 = (int)(var - varText - 2);
                        errorPos2 = (int)(s - varText);
                        return FALSE;
                    }
                    else
                    {
                        int varLen = (int)(s - var);
                        // check the variable width
                        const char* s2 = var;
                        int varWidth = 0;
                        while (s2 < s)
                        {
                            if (*s2 == ':')
                            {
                                if (*++s2 != ':')
                                {
                                    int tmpLen = (int)(s - s2);
                                    BOOL validMax = tmpLen == 3 && (StrNICmp(s2, "max", 3) == 0);
                                    detectMax = (validMax && detectMaxVarWidths);
                                    BOOL validNum = FALSE;
                                    if (!validMax)
                                    {
                                        if (tmpLen > 0 && tmpLen <= 4)
                                        {
                                            const char* s3 = s2;
                                            while (s3 < s && *s3 >= '0' && *s3 <= '9')
                                                s3++;
                                            if (s3 == s) // only digits
                                            {
                                                char widthBuff[5];
                                                lstrcpyn(widthBuff, s2, tmpLen + 1);
                                                varWidth = atoi(widthBuff);
                                                validNum = (varWidth >= 1);
                                            }
                                        }
                                    }
                                    if (!validMax && !validNum)
                                    {
                                        const char* text = LoadStr(IDS_EXP_INVALIDVARWIDTH);
                                        if (msgParent == NULL)
                                            TRACE_I(text);
                                        else
                                        {
                                            SalMessageBox(msgParent, text,
                                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                        }
                                        errorPos1 = (int)(s2 - varText);
                                        errorPos2 = (int)(s - varText);
                                        return FALSE;
                                    }
                                    else
                                    {
                                        if (!validateOnly && validMax && !detectMax) // phase of using precomputed values
                                        {
                                            if (currentMaxVarIndex < maxVarWidthsCount)
                                            {
                                                valueOutLen = maxVarWidths[currentMaxVarIndex];
                                                currentMaxVarIndex++;
                                            }
                                            else
                                                TRACE_E("Buffer maxVarWidths is small");
                                        }
                                        if (validNum)
                                            valueOutLen = varWidth;
                                        varLen -= (int)(s - s2) + 1; // the colon and the variable width
                                    }
                                    break;
                                }
                            }
                            s2++;
                        }

                        const CSalamanderVarStrEntry* entry = variables;
                        while (entry->Name != NULL)
                        {
                            if (StrICmpEx(var, varLen, entry->Name, (int)strlen(entry->Name)) == 0)
                            {
                                if (!validateOnly)
                                {
                                    value = entry->Execute(msgParent, param);
                                    if (value == NULL)
                                    {
                                        const char* text = LoadStr(IDS_EXP_INTERNALERR);
                                        if (msgParent == NULL)
                                            TRACE_I(text);
                                        else
                                        {
                                            SalMessageBox(msgParent, text,
                                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                        }
                                        errorPos1 = (int)(var - varText - 2);
                                        errorPos2 = (int)(s - varText + 1);
                                        return FALSE;
                                    }
                                }
                                else
                                    value = "";
                                break;
                            }
                            entry++;
                        }
                        if (entry->Name == NULL)
                        {
                            if (varLen >= MAX_PATH)
                                varLen = MAX_PATH - 1;
                            memcpy(buf, var, varLen);
                            buf[varLen] = 0;
                            char text[200];
                            sprintf(text, LoadStr(IDS_EXP_VARNOTFOUND), buf);
                            if (msgParent == NULL)
                                TRACE_I(text);
                            else
                            {
                                SalMessageBox(msgParent, text,
                                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                            errorPos1 = (int)(var - varText - 2);
                            errorPos2 = (int)(s - varText + 1);
                            return FALSE;
                        }
                        s++;
                        break;
                    }
                }

                case '[':
                {
                    const char* var = s + 1;
                    while (*s != ']' && *s != 0)
                        s++;
                    if (*s == 0)
                    {
                        const char* text = LoadStr(IDS_EXP_UNMATCHEDBRACKET);
                        if (msgParent == NULL)
                            TRACE_I(text);
                        else
                        {
                            SalMessageBox(msgParent, text,
                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                        errorPos1 = (int)(var - varText - 2);
                        errorPos2 = (int)(s - varText);
                        return FALSE;
                    }
                    else
                    {
                        if (!validateOnly)
                        {
                            int varLen = (int)(s - var);
                            char envVar[MAX_PATH];
                            if (varLen >= MAX_PATH)
                                varLen = MAX_PATH - 1;
                            memcpy(envVar, var, varLen);
                            envVar[varLen] = 0;
                            DWORD res = GetEnvironmentVariable(envVar, buf, MAX_PATH);
                            if (res == 0 || res >= MAX_PATH)
                            {
                                char text[MAX_PATH + 100];
                                if (res == 0)
                                    sprintf(text, LoadStr(IDS_EXP_ENVVARNOTFOUND), envVar);
                                else
                                    sprintf(text, LoadStr(IDS_EXP_ENVVARTOOLARGE), envVar);
                                if (msgParent == NULL)
                                    TRACE_I(text);
                                else
                                {
                                    if (!ignoreEnvVarNotFoundOrTooLong)
                                    {
                                        MSGBOXEX_PARAMS params;
                                        memset(&params, 0, sizeof(params));
                                        params.HParent = msgParent;
                                        params.Flags = MSGBOXEX_OKCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_SILENT;
                                        params.Caption = LoadStr(IDS_ERRORTITLE);
                                        params.Text = text;
                                        char aliasBtnNames[100];
                                        /* serves the export_mnu.py script that generates salmenu.mnu for Translator
   let the message box buttons handle hotkey collisions by pretending it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_BUTTON_IGNORE
  {MNTT_PE, 0
};
*/
                                        sprintf(aliasBtnNames, "%d\t%s", DIALOG_OK, LoadStr(IDS_BUTTON_IGNORE));
                                        params.AliasBtnNames = aliasBtnNames;
                                        if (SalMessageBoxEx(&params) == DIALOG_CANCEL)
                                        {
                                            errorPos1 = (int)(var - varText);
                                            errorPos2 = (int)(s - varText);
                                            return FALSE;
                                        }
                                    }
                                }
                                buf[0] = 0;
                            }
                            value = buf;
                        }
                        s++;
                        break;
                    }
                }

                default:
                {
                    const char* text = LoadStr(IDS_EXP_UNEXPECTEDCHAR);
                    if (msgParent == NULL)
                        TRACE_I(text);
                    else
                    {
                        SalMessageBox(msgParent, text,
                                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                    errorPos1 = (int)(s - varText);
                    errorPos2 = (int)(s - varText + 1);
                    return FALSE;
                }
                }

                if (!validateOnly)
                {
                    int len = (int)strlen(value);
                    if (detectMax)
                    {
                        if (currentMaxVarIndex < maxVarWidthsCount)
                        {
                            if (maxVarWidths[currentMaxVarIndex] < len)
                                maxVarWidths[currentMaxVarIndex] = len;
                            currentMaxVarIndex++;
                        }
                        else
                            TRACE_E("Buffer maxVarWidths is small");
                    }
                    if (buffer != NULL)
                    {
                        int totalLen = (valueOutLen > 0) ? valueOutLen : len;
                        if (out + totalLen + 1 <= outEnd) // it must still be possible to null-terminate it
                        {
                            if (varPlacementIndex < varPlacementIndexCount)
                            {
                                varPlacements[varPlacementIndex] = MAKELPARAM(out - buffer, totalLen);
                                varPlacementIndex++;
                            }
                            else
                            {
                                if (varPlacements != NULL)
                                    TRACE_E("Buffer varPlacements is small");
                            }
                            memcpy(out, value, min(totalLen, len));
                            if (len < totalLen)
                                memset(out + len, ' ', totalLen - len);
                            out += totalLen;
                        }
                        else
                        {
                            const char* text = LoadStr(IDS_EXP_SMALLBUFFER);
                            if (msgParent == NULL)
                                TRACE_I(text);
                            else
                            {
                                SalMessageBox(msgParent, text,
                                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                            errorPos1 = (int)(s - varText);
                            errorPos2 = (int)(s - varText);
                            return FALSE;
                        }
                    }
                }
            }
            else
            {
                const char* text = LoadStr(IDS_EXP_TRAILINGDOLLAR);
                if (msgParent == NULL)
                    TRACE_I(text);
                else
                {
                    SalMessageBox(msgParent, text,
                                  LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                errorPos1 = (int)(s - varText - 1);
                errorPos2 = (int)(s - varText);
                return FALSE;
            }
        }
        else
        {
            if (!validateOnly && buffer != NULL)
            {
                if (out + 2 <= outEnd)
                    *out++ = *s; // it must still be possible to null-terminate it
                else
                {
                    const char* text = LoadStr(IDS_EXP_SMALLBUFFER);
                    if (msgParent == NULL)
                        TRACE_I(text);
                    else
                    {
                        SalMessageBox(msgParent, text,
                                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                    errorPos1 = (int)(s - varText);
                    errorPos2 = (int)(s - varText + 1);
                    return FALSE;
                }
            }
            s++;
        }
    }
    if (!validateOnly && buffer != NULL)
    {
        if (out < outEnd)
            *out = 0;
        else
        {
            const char* text = LoadStr(IDS_EXP_SMALLBUFFER);
            if (msgParent == NULL)
                TRACE_I(text);
            else
            {
                SalMessageBox(msgParent, text,
                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            errorPos1 = (int)(s - varText);
            errorPos2 = (int)(s - varText);
            return FALSE;
        }
    }
    if (varPlacementsCount != NULL)
        *varPlacementsCount = varPlacementIndex;
    return TRUE;
}

BOOL ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                       const CSalamanderVarStrEntry* variables)
{
    return DoExpandVarString(msgParent, varText, TRUE, errorPos1, errorPos2, NULL, 0, variables, NULL,
                             NULL, NULL, FALSE, NULL, 0, TRUE);
}

BOOL ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                     const CSalamanderVarStrEntry* variables, void* param,
                     BOOL ignoreEnvVarNotFoundOrTooLong, DWORD* varPlacements,
                     int* varPlacementsCount, BOOL detectMaxVarWidths, int* maxVarWidths,
                     int maxVarWidthsCount)
{
    int errorPos1, errorPos2;
    return DoExpandVarString(msgParent, varText, FALSE, errorPos1, errorPos2, buffer,
                             bufferLen, variables, param,
                             varPlacements, varPlacementsCount,
                             detectMaxVarWidths, maxVarWidths, maxVarWidthsCount,
                             ignoreEnvVarNotFoundOrTooLong);
}

// ****************************************************************************

CQuadWord MyGetDiskFreeSpace(const char* path, CQuadWord* total)
{
    CALL_STACK_MESSAGE2("MyGetDiskFreeSpace(%s, )", path);
    CQuadWord ret = CQuadWord(-1, -1);
    if (total != NULL)
        *total = CQuadWord(-1, -1);
    ULARGE_INTEGER availBytes, totalBytes, freeBytes;
    char ourPath[MAX_PATH + 200];
    lstrcpyn(ourPath, path, MAX_PATH + 200);
    SalPathAddBackslash(ourPath, MAX_PATH + 200);
    if (GetDiskFreeSpaceEx(ourPath, &availBytes, &totalBytes, &freeBytes))
    {
        ret.Value = (unsigned __int64)availBytes /*freeBytes*/.QuadPart; // I used availBytes instead of freeBytes because a user Jan Kobr <jan.kobr@pvk.cz> reported that we were showing 35GB instead of 2GB of free space (it was a Novell network disk with quotas under XP)
        if (total != NULL)
            total->Value = (unsigned __int64)totalBytes.QuadPart;
    }
    if (ret == CQuadWord(-1, -1))
    {
        DWORD a, b, c, d;
        if (MyGetDiskFreeSpace(path, &a, &b, &c, &d))
        {
            ret = CQuadWord(a, 0) * CQuadWord(b, 0) * CQuadWord(c, 0);
            if (total != NULL)
                *total = CQuadWord(a, 0) * CQuadWord(b, 0) * CQuadWord(d, 0);
        }
        else
            ret = CQuadWord(-1, -1); // error, do not display it
    }
    return ret;
}

UINT GetDriveTypeForDriveLetterPath(const char* path)
{
    char root[4] = " :\\";
    root[0] = path[0];
    return GetDriveType(root);
}

BOOL IsUNCPath(const char* path)
{
    const char* server = NULL;
    if (path[0] == '\\' && path[1] == '\\' && path[2] != '?')
        server = path + 2;
    else if (_strnicmp(path, "\\\\?\\UNC\\", 8) == 0)
        server = path + 8;
    if (server != NULL)
    {
        const char* share = strchr(server, '\\');
        if (share != NULL && *(share + 1) != 0)
        {
            const char* end = strchr(share + 1, '\\');
            if (end == NULL)
                end = share + strlen(share);
            if (end - path + 1 < MAX_PATH)
                return TRUE; // above MAX_PATH it would no longer be a UNC root path (+1 for the trailing backslash)
        }
    }
    return FALSE;
}

BOOL IsUNCRootPath(const char* path)
{
    if (path[0] == '\\' && path[1] == '\\' && path[2] != '?' &&
        (path[2] != '.' || path[3] != '\\' || path[4] == 0 || path[5] != ':')) // not paths like "\\.\C:\"
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s == '\\')
            s++;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s == '\\')
            s++;
        return *s == 0;
    }
    return FALSE;
}

BOOL ResolveSubsts(char* resPath)
{
    BOOL ret = TRUE;
    int cycle = 0;
    while ((resPath[0] >= 'a' && resPath[0] <= 'z' || resPath[0] >= 'A' && resPath[0] <= 'Z') &&
           resPath[1] == ':')
    {
        if (cycle++ == 50)
        {
            TRACE_E("ResolveSubsts(): infinite loop found!");
            ret = FALSE;
            break;
        }
        char tgt[MAX_PATH];
        if (GetSubstInformation(LowerCase[resPath[0]] - 'a', tgt, MAX_PATH) && tgt[0] != '\\' /* mapped network disks handled elsewhere */)
        {
            if (!SalPathAppend(tgt, resPath + 2, MAX_PATH))
            {
                TRACE_E("ResolveSubsts(): too long path!");
                ret = FALSE;
                break;
            }
            if (tgt[0] != 0 && tgt[1] == ':' && tgt[2] == 0) // "C:" -> "C:\\"
            {
                tgt[2] = '\\';
                tgt[3] = 0;
            }
            lstrcpyn(resPath, tgt, MAX_PATH);
        }
        else
            break;
    }
    return ret;
}

void ResolveLocalPathWithReparsePoints(char* resPath, const char* path, BOOL* cutResPathIsPossible,
                                       BOOL* rootOrCurReparsePointSet, char* rootOrCurReparsePoint,
                                       char* junctionOrSymlinkTgt, int* linkType, char* netPath)
{
    lstrcpyn(resPath, path, MAX_PATH);
    ResolveSubsts(resPath);
    if (!SalPathAddBackslash(resPath, MAX_PATH))
        TRACE_E("ResolveLocalPathWithReparsePoints(): too long path");
    else
    {
        if (strlen(resPath) > 3)
        {
            char repPointPath[MAX_PATH];
            int allowedDepth = 50;
            BOOL firstRepPoint = TRUE;
            while (GetCurrentLocalReparsePoint(resPath, repPointPath))
            {
                if (rootOrCurReparsePointSet != NULL && !*rootOrCurReparsePointSet && rootOrCurReparsePoint != NULL)
                {
                    GetRootPath(resPath, path);
                    ResolveSubsts(resPath);
                    GetRootPath(rootOrCurReparsePoint, path);
                    if (strlen(resPath) < strlen(repPointPath)) // if the path to the current reparse point is longer than the path obtained by resolving the subst, we must append this part of the path after the subst root
                    {
                        if (_strnicmp(resPath, repPointPath, strlen(resPath)) == 0) // always true
                        {
                            if (!SalPathAppend(rootOrCurReparsePoint, repPointPath + strlen(resPath), MAX_PATH))
                            {
                                TRACE_E("ResolveLocalPathWithReparsePoints(): unexpected situation: too long path for substed path");
                                lstrcpyn(rootOrCurReparsePoint, repPointPath, MAX_PATH);
                            }
                            else
                                SalPathAddBackslash(rootOrCurReparsePoint, MAX_PATH);
                        }
                        else
                        {
                            TRACE_E("ResolveLocalPathWithReparsePoints(): unexpected prefix of resolved path");
                            lstrcpyn(rootOrCurReparsePoint, repPointPath, MAX_PATH);
                        }
                    }
                    *rootOrCurReparsePointSet = TRUE;
                }
                lstrcpyn(resPath, repPointPath, MAX_PATH);
                if (!SalPathAddBackslash(resPath, MAX_PATH))
                    TRACE_E("ResolveLocalPathWithReparsePoints(): too long path");
                int repPointType;
                BOOL getRepPointDestRes = GetReparsePointDestination(repPointPath, repPointPath, MAX_PATH, &repPointType, TRUE);
                if (getRepPointDestRes && (repPointType == 2 /* JUNCTION POINT */ || repPointType == 3 /* SYMBOLIC LINK */))
                {
                    if (firstRepPoint)
                    {
                        if (junctionOrSymlinkTgt != NULL)
                            lstrcpyn(junctionOrSymlinkTgt, repPointPath, MAX_PATH);
                        if (linkType != NULL)
                            *linkType = repPointType;
                    }
                    ResolveSubsts(repPointPath);
                }
                firstRepPoint = FALSE;
                UINT drvType = getRepPointDestRes && repPointPath[0] != 0 && repPointPath[1] == ':' ? GetDriveTypeForDriveLetterPath(repPointPath) : DRIVE_UNKNOWN;
                if (getRepPointDestRes && (IsUNCPath(repPointPath) || drvType == DRIVE_REMOTE)) // symlink to a UNC or mapped network path (available since Vista)
                {                                                                               // it only makes sense to look for reparse points on fixed disks, so stop here (network paths are a problem because their reparse points will return "local paths" (C:\...), which, if used on this machine (instead of the remote one they come from), it will lead to nonsensical results)
                    if (netPath != NULL)
                        lstrcpyn(netPath, repPointPath, MAX_PATH);
                    GetRootPath(resPath, repPointPath);
                    break;
                }
                if (!getRepPointDestRes || repPointPath[0] == 0 || repPointPath[1] != ':')
                { // unknown reparse point or volume mount point; in any case do not traverse it, let the system try + the path must not be shortened or it may refer to another volume
                    *cutResPathIsPossible = FALSE;
                    break;
                }
                if (allowedDepth-- == 0) // looks like an endless loop
                {
                    lstrcpyn(resPath, path, MAX_PATH); // let the system handle it on its own
                    ResolveSubsts(resPath);
                    if (!SalPathAddBackslash(resPath, MAX_PATH))
                        TRACE_E("ResolveLocalPathWithReparsePoints(): too long path");
                    if (rootOrCurReparsePointSet != NULL)
                        *rootOrCurReparsePointSet = FALSE;
                    if (junctionOrSymlinkTgt != NULL)
                        *junctionOrSymlinkTgt = 0;
                    if (linkType != NULL)
                        *linkType = 0 /* UNKNOWN */;
                    break;
                }
                lstrcpyn(resPath, repPointPath, MAX_PATH);
                if (!SalPathAddBackslash(resPath, MAX_PATH))
                {
                    TRACE_E("ResolveLocalPathWithReparsePoints(): too long path");
                    break;
                }
                if (drvType != DRIVE_FIXED)
                    break; // reparse points only make sense to look for on fixed disks
            }
        }
    }
}

BOOL MyGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters)
{
    CALL_STACK_MESSAGE2("MyGetDiskFreeSpace(%s, , , , )", path);
    char ourPath[MAX_PATH];
    char resPath[MAX_PATH];
    lstrcpyn(resPath, path, MAX_PATH);
    ResolveSubsts(resPath);
    GetRootPath(ourPath, resPath);
    if (!IsUNCPath(ourPath) && GetDriveType(ourPath) == DRIVE_FIXED) // reparse points only make sense to look for on fixed disks
    {                                                                // gradually try shortening the path; on a mounted directory it can return the mounted disk parameters
        // if it is not a root path, try traversing the reparse points as well
        BOOL cutPathIsPossible = TRUE;
        ResolveLocalPathWithReparsePoints(ourPath, path, &cutPathIsPossible, NULL, NULL, NULL, NULL, NULL);

        while (!GetDiskFreeSpace(ourPath, lpSectorsPerCluster, lpBytesPerSector,
                                 lpNumberOfFreeClusters, lpTotalNumberOfClusters))
        {
            if (!cutPathIsPossible || !CutDirectory(ourPath))
                return FALSE; // we must not cut it or even the root did not succeed; abort with error
            SalPathAddBackslash(ourPath, MAX_PATH);
        }
        return TRUE;
    }
    else
    {
        return GetDiskFreeSpace(ourPath, lpSectorsPerCluster, lpBytesPerSector,
                                lpNumberOfFreeClusters, lpTotalNumberOfClusters);
    }
}

BOOL MyGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, char* junctionOrSymlinkTgt, int* linkType,
                            LPTSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                            LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                            LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
    char ourPath[MAX_PATH];
    BOOL ret = TRUE;
    if (junctionOrSymlinkTgt != NULL)
        *junctionOrSymlinkTgt = 0;
    if (linkType != NULL)
        *linkType = 0;
    char resPath[MAX_PATH];
    lstrcpyn(resPath, path, MAX_PATH);
    ResolveSubsts(resPath);
    GetRootPath(ourPath, resPath);
    if (!IsUNCPath(ourPath) && GetDriveType(ourPath) == DRIVE_FIXED) // reparse points only make sense to look for on fixed disks
    {                                                                // gradually try shortening the path; on a mounted directory it can return the mounted disk parameters
        // if it is not a root path, try traversing the reparse points as well
        BOOL rootOrCurReparsePointSet = FALSE;
        BOOL cutPathIsPossible = TRUE;
        ResolveLocalPathWithReparsePoints(ourPath, path, &cutPathIsPossible, &rootOrCurReparsePointSet,
                                          rootOrCurReparsePoint, junctionOrSymlinkTgt, linkType, NULL);

        while (!GetVolumeInformation(ourPath, lpVolumeNameBuffer, nVolumeNameSize,
                                     lpVolumeSerialNumber, lpMaximumComponentLength,
                                     lpFileSystemFlags, lpFileSystemNameBuffer,
                                     nFileSystemNameSize))
        {
            if (!cutPathIsPossible || !CutDirectory(ourPath))
            {
                ret = FALSE; // we must not cut it or even the root did not succeed; abort with error
                break;
            }
            SalPathAddBackslash(ourPath, MAX_PATH);
        }
        if (!rootOrCurReparsePointSet && rootOrCurReparsePoint != NULL)
        { // ourPath is ResolveSubsts(path) or a shortened version of ResolveSubsts(path)
            GetRootPath(resPath, path);
            ResolveSubsts(resPath);
            GetRootPath(rootOrCurReparsePoint, path);
            if (strlen(resPath) < strlen(ourPath)) // if the path we return volume info for is longer than the path obtained by resolving the subst, we must append this part of the path after the subst root
            {
                if (_strnicmp(resPath, ourPath, strlen(resPath)) == 0) // always true
                {
                    if (!SalPathAppend(rootOrCurReparsePoint, ourPath + strlen(resPath), MAX_PATH))
                    {
                        TRACE_E("MyGetVolumeInformation(): unexpected situation: too long path for substed path");
                        lstrcpyn(rootOrCurReparsePoint, ourPath, MAX_PATH);
                    }
                    else
                        SalPathAddBackslash(rootOrCurReparsePoint, MAX_PATH);
                }
                else
                {
                    TRACE_E("MyGetVolumeInformation(): unexpected prefix of resolved path");
                    lstrcpyn(rootOrCurReparsePoint, ourPath, MAX_PATH);
                }
            }
            int l = (int)strlen(rootOrCurReparsePoint);
            if (l > 3 && rootOrCurReparsePoint[l - 1] == '\\')
                rootOrCurReparsePoint[l - 1] = 0; // remove the trailing backslash except for "c:\"
        }
    }
    else
    {
        ret = GetVolumeInformation(ourPath, lpVolumeNameBuffer, nVolumeNameSize,
                                   lpVolumeSerialNumber, lpMaximumComponentLength,
                                   lpFileSystemFlags, lpFileSystemNameBuffer,
                                   nFileSystemNameSize);
        if (rootOrCurReparsePoint != NULL)
        {
            GetRootPath(rootOrCurReparsePoint, path);
            int l = (int)strlen(rootOrCurReparsePoint);
            if (l > 3 && rootOrCurReparsePoint[l - 1] == '\\')
                rootOrCurReparsePoint[l - 1] = 0; // remove the trailing backslash except for "c:\"
        }
    }
    return ret;
}

// Structure for FSCTL_SET_REPARSE_POINT, FSCTL_GET_REPARSE_POINT, and
// FSCTL_DELETE_REPARSE_POINT.
// This version of the reparse data buffer is only for Microsoft tags.

struct TMN_REPARSE_DATA_BUFFER
{
    DWORD ReparseTag;
    WORD ReparseDataLength;
    WORD Reserved;
    WORD SubstituteNameOffset;
    WORD SubstituteNameLength;
    WORD PrintNameOffset;
    WORD PrintNameLength;
    WCHAR PathBuffer[1];
};

#define FSCTL_GET_REPARSE_POINT CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS) // REPARSE_DATA_BUFFER
#define IO_REPARSE_TAG_SYMLINK (0xA000000CL)

BOOL GetReparsePointDestination(const char* repPointDir, char* repPointDstBuf, DWORD repPointDstBufSize,
                                int* repPointType, BOOL makeRelPathAbs)
{
    if (repPointType != NULL)
        *repPointType = 0 /* UNKNOWN */;

    // if the path ends with a space/dot we must append '\\', otherwise GetFileAttributes
    // and CreateFile will trim spaces/dots and operate on a different path
    const char* repPointDirCrFile = repPointDir;
    char repPointDirCrFileCopy[3 * MAX_PATH];
    MakeCopyWithBackslashIfNeeded(repPointDirCrFile, repPointDirCrFileCopy);

    DWORD attrs = GetFileAttributes(repPointDirCrFile);
    if (attrs == 0xffffffff || (attrs & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
        //    TRACE_I("GetReparsePointDestination(): Reparse point not found: " << repPointDir);
        return FALSE;
    }

    HANDLE file = HANDLES_Q(CreateFile(repPointDirCrFile, 0 /*GENERIC_READ*/, 0, 0, OPEN_EXISTING,
                                       FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
    if (file == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        TRACE_E("GetReparsePointDestination(): Unable to open reparse point: " << repPointDir << ", error: " << err);
        return FALSE;
    }
    DWORD dummy;
    char buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    TMN_REPARSE_DATA_BUFFER* juncData = (TMN_REPARSE_DATA_BUFFER*)buf;
    if (DeviceIoControl(file, FSCTL_GET_REPARSE_POINT, NULL, 0, juncData,
                        MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dummy, NULL) == 0 ||
        juncData->ReparseTag != IO_REPARSE_TAG_MOUNT_POINT &&
            juncData->ReparseTag != IO_REPARSE_TAG_SYMLINK)
    {
        HANDLES(CloseHandle(file));
        TRACE_E("GetReparsePointDestination(): Unable to get data of reparse point: " << repPointDir);
        return FALSE;
    }
    HANDLES(CloseHandle(file));

    WCHAR substName[1000];
    WCHAR printName[1000];
    int myType = 0;
    if (juncData->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
    {
        lstrcpynW(substName, (WCHAR*)((char*)juncData->PathBuffer + juncData->SubstituteNameOffset),
                  min(juncData->SubstituteNameLength / sizeof(WCHAR) + 1, 1000));
        lstrcpynW(printName, (WCHAR*)((char*)juncData->PathBuffer + juncData->PrintNameOffset),
                  min(juncData->PrintNameLength / sizeof(WCHAR) + 1, 1000));
        myType = _wcsnicmp(substName, L"\\??\\Volume", 10) == 0 ? 1 /* MOUNT POINT */ : 2 /* JUNCTION POINT */;
    }
    else
    {
        if (juncData->ReparseTag == IO_REPARSE_TAG_SYMLINK)
        {
            lstrcpynW(substName, (WCHAR*)((char*)juncData->PathBuffer + 4 /* ULONG Flags */ + juncData->SubstituteNameOffset),
                      min(juncData->SubstituteNameLength / sizeof(WCHAR) + 1, 1000));
            lstrcpynW(printName, (WCHAR*)((char*)juncData->PathBuffer + 4 /* ULONG Flags */ + juncData->PrintNameOffset),
                      min(juncData->PrintNameLength / sizeof(WCHAR) + 1, 1000));
            myType = 3 /* SYMBOLIC LINK */;
        }
        else
        {
            TRACE_E("GetReparsePointDestination(): Unknown type of reparse point: " << repPointDir);
            return FALSE;
        }
    }
    if (repPointType != NULL)
        *repPointType = myType;
    if (repPointDstBuf != NULL)
    {
        WCHAR* s = printName;
        if (*s == 0)
        {
            s = substName;
            if (_wcsnicmp(s, L"\\??\\", 4) == 0)
                s = substName + 4; // skip "\\??\\" in substName
        }
        if (myType == 2 /* JUNCTION POINT */ && (*s == 0 || *(s + 1) != L':' || *(s + 2) != L'\\'))
        {
            TRACE_E("GetReparsePointDestination(): Unexpected format of junction point (relative path): " << repPointDir);
            return FALSE;
        }
        WCHAR symlinkAbsPath[1000];
        if (makeRelPathAbs && myType == 3 /* SYMBOLIC LINK */ &&
            !(*s != 0 && *(s + 1) == L':' && *(s + 2) == L'\\' || *s == L'\\' && *(s + 1) == L'\\'))
        { // the symlink is relative; try converting it to an absolute path
            if (repPointDir[0] == 0 || repPointDir[1] != ':')
            {
                TRACE_E("GetReparsePointDestination(): Unexpected format of symbolic link name (it is not a local path): " << repPointDir);
                return FALSE;
            }
            symlinkAbsPath[0] = (WCHAR)(unsigned char)repPointDir[0]; // a bit of a hack (we rely on a fact that 'a-zA-Z' convert to Unicode 1:1)
            symlinkAbsPath[1] = L':';
            if (*s == L'\\')
                lstrcpynW(symlinkAbsPath + 2, s, 1000 - 2);
            else
            {
                if (MultiByteToWideChar(CP_ACP, 0, repPointDir + 2, -1, symlinkAbsPath + 2, 1000 - 2) == 0)
                {
                    DWORD err = GetLastError();
                    TRACE_E("GetReparsePointDestination(): MultiByteToWideChar error: " << err);
                    return FALSE;
                }
                symlinkAbsPath[1000 - 1] = 0;
                int len = lstrlenW(symlinkAbsPath);
                if (symlinkAbsPath[len - 1] == L'\\')
                    symlinkAbsPath[len - 1] = 0;
                WCHAR* lastComp = wcsrchr(symlinkAbsPath, L'\\');
                if (lastComp == NULL)
                {
                    TRACE_E("GetReparsePointDestination(): Unexpected format of symbolic link name (it does not contain backslash): " << repPointDir);
                    return FALSE;
                }
                lstrcpynW(lastComp + 1, s, (int)(1000 - ((lastComp + 1) - symlinkAbsPath)));
            }
            s = symlinkAbsPath;
        }
        if (myType == 3 /* SYMBOLIC LINK */ && *s != 0 && *(s + 1) == L':' && *(s + 2) == L'\\')
            SalRemovePointsFromPath(s + 3);
        if (WideCharToMultiByte(CP_ACP, 0, s, -1, repPointDstBuf, repPointDstBufSize, NULL, NULL) == 0)
        {
            DWORD err = GetLastError();
            TRACE_I("WideCharToMultiByte error: " << err);
            return FALSE;
        }
        if (repPointDstBufSize > 0)
            repPointDstBuf[repPointDstBufSize - 1] = 0;
    }
    return TRUE;
}

BOOL GetCurrentLocalReparsePoint(const char* path, char* currentReparsePoint, BOOL* error)
{
    BOOL ret = TRUE;
    lstrcpyn(currentReparsePoint, path, MAX_PATH);
    if (!SalPathAddBackslash(currentReparsePoint, MAX_PATH))
    {
        TRACE_E("GetCurrentLocalReparsePoint(): too long path");
        if (error != NULL)
            *error = TRUE;
        ret = FALSE;
    }
    else
    {
        // walk through reparse points from the beginning of the path to the end or to the first symlink leading to a network path
        char repPointPath[MAX_PATH];
        char* end = (char*)SkipRoot(currentReparsePoint) + 1; // currentReparsePoint always ends with a backslash
        char* lastRepPointEnd = NULL;
        while (1)
        {
            end = strchr(end, '\\');
            if (end == NULL)
                break; // that was the last component of the path, stop here
            end++;
            char backup = *end;
            *end = 0;
            if (GetReparsePointDestination(currentReparsePoint, repPointPath, MAX_PATH, NULL, TRUE))
            {
                lastRepPointEnd = end;
                if (IsUNCPath(repPointPath) ||
                    repPointPath[0] != 0 && repPointPath[1] == ':' &&
                        GetDriveTypeForDriveLetterPath(repPointPath) == DRIVE_REMOTE) // symlink to a UNC or mapped network path (available since Vista)
                {
                    break;
                }
            }
            *end = backup;
        }
        if (lastRepPointEnd != NULL)
            *lastRepPointEnd = 0;
        else
            ret = FALSE; // no reparse point found
    }
    if (!ret)
        GetRootPath(currentReparsePoint, path);
    return ret;
}

UINT MyGetDriveType(const char* path)
{
    char ourPath[MAX_PATH];
    char resPath[MAX_PATH];
    lstrcpyn(resPath, path, MAX_PATH);
    ResolveSubsts(resPath);
    GetRootPath(ourPath, resPath);
    UINT ret = DRIVE_UNKNOWN;
    if (!IsUNCPath(ourPath))
    {
        UINT drvType = GetDriveType(ourPath);
        if (drvType == DRIVE_FIXED) // reparse points only make sense to look for on fixed disks
        {                           // gradually try shortening the path; on a mounted directory it can return the mounted disk parameters
            // if it is not a root path, try traversing the reparse points as well
            BOOL cutPathIsPossible = TRUE;
            ResolveLocalPathWithReparsePoints(ourPath, path, &cutPathIsPossible, NULL, NULL, NULL, NULL, NULL);

            while ((ret = GetDriveType(ourPath)) == DRIVE_UNKNOWN)
            { // NOTE: differs from MyGetVolumeInformation because GetDriveType returns success for any path (not just root + mounted volume)
                if (!cutPathIsPossible || !CutDirectory(ourPath))
                    break; // we must not cut it or even the root did not succeed; end with error
                SalPathAddBackslash(ourPath, MAX_PATH);
            }
        }
        else
            ret = drvType;
    }
    else
        ret = GetDriveType(ourPath);
    return ret;
}

//****************************************************************************
//
// GetSubstInformation
//

BOOL MyQueryDosDevice(BYTE driveNum, char* target, int maxTarget)
{
    char deviceName[3];
    deviceName[0] = driveNum + 'A';
    deviceName[1] = ':';
    deviceName[2] = 0;
    return QueryDosDevice(deviceName, target, MAX_PATH);
}

BOOL GetSubstInformation(BYTE driveNum, char* path, int pathMax)
{
    char target[MAX_PATH];
    if (MyQueryDosDevice(driveNum, target, MAX_PATH))
    {
        //  A (floppy)                          \Device\Floppy0
        //  C (fixed disk)                      \Device\HarddiskVolume1
        //  D (fixed disk)                      \Device\HarddiskVolume2
        //  U -> V:                             \??\V:
        //  V (mapped \\drak\share)           \Device\LanmanRedirector\;V:00000000000fdf1\drak\share
        //  W -> \\drak\share:                  \??\UNC\drak\share
        //  X -> D:                             \??\D:
        //  Y -> C:\Windows                     \??\C:\Windows
        if (memcmp(target, "\\??\\", 4) == 0)
        {
            if (((target[4] >= 'a' && target[4] <= 'z') || (target[4] >= 'A' && target[4] <= 'Z')) &&
                target[5] == ':')
            {
                lstrcpyn(path, target + 4, pathMax);
                return TRUE;
            }
            if (memcmp(target + 4, "UNC\\", 4) == 0)
            {
                lstrcpyn(path, "\\", pathMax);
                if (pathMax > 2)
                    lstrcpyn(path + 1, target + 7, pathMax - 1);
                return TRUE;
            }
        }
    }
    return FALSE;
}

//****************************************************************************
//
// GetMessagePos
//

void GetMessagePos(POINT& p)
{
    DWORD w = GetMessagePos();
    p.x = ((int)(short)LOWORD(w));
    p.y = ((int)(short)HIWORD(w));
}

//
// ****************************************************************************
// AlterFileName
//  - changes the format of the file name (letter casing)
//
//   tgtName - buffer for the result (at least as large as filename)
//
//   filename - input file name
//   filenameLen - length of the filename string; if it is -1, the function determines it itself
//                 optimization for CFilesWindow::RefreshListBox(), which
//                 calls this function extensively; in random cases -1 is sufficient
//
//   format - 1 - capitalized words
//            2 - all lowercase letters
//            3 - all uppercase letters
//            4 - unchanged
//            5 - if it is a DOS name (8.3) -> capitalized words
//            6 - file lowercase, directory uppercase
//            7 - capitalized name and lowercase extension
//
//   change - 0 - change both the name and the extension
//            1 - change only the name (possible only with format == 1, 2, 3, 4)
//            2 - change only the extension (possible only with format == 1, 2, 3, 4)
//
//   dir    - is it a directory?

void AlterFileName(char* tgtName, char* filename, int filenameLen, int format, int change, BOOL dir)
{
    // j.r. I disabled the macro because AlterFileName is called heavily from RefreshListBox()
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE6("AlterFileName(, %s, %d, %d, %d, %d)", filename, filenameLen, format, change, dir);
    if (format == 6)
        format = dir ? 3 : 2; // VC display style
    if (format == 7 && change != 0)
        format = (change == 1) ? 1 : 2; // convert to mixed/lower case

    char* ext = NULL; // points past the last dot or is NULL (no extension)
    if (change != 0 && format != 5 && format != 7)
    {
        char* s = filename;
        while (*s != 0) // searching for the last dot (file extensions)
            if (*s++ == '.')
                ext = s;
        //  if (ext != NULL && ext <= filename + 1) ext = NULL;  // ".cvspass" in Windows is considered an extension ..
        if (change == 1) // change only the name
        {
            if (ext != NULL)
                *(ext - 1) = 0; // overwrite '.' with the end of string (0)
        }
        else // change only the extension
        {
            if (ext == NULL || *ext == 0) // no extension
            {
                strcpy(tgtName, filename);
                return;
            }
            memmove(tgtName, filename, ext - filename); // copy of the name + '.'
            tgtName += ext - filename;
            filename = ext;
        }
    }

    switch (format)
    {
    case 5: // explorer style
    {
        char* s = filename;
        int c = 8;
        while (c-- && *s != 0 && *s == UpperCase[*s] && *s != '.')
            s++; // name
        if (*s == '.')
            s++;
        else if (*s != 0)
        {
            strcpy(tgtName, filename);
            break;
        }
        c = 3;
        while (c-- && *s != 0 && *s != '.' && *s == UpperCase[*s])
            s++; // ext
        if (*s != 0)
        {
            strcpy(tgtName, filename);
            break;
        }
        BOOL capital = TRUE;
        char* tgt = tgtName;
        char* name = filename;
        while (*name != 0)
        {
            if (!capital)
            {
                *tgt++ = LowerCase[*name];
                if (*name++ == ' ')
                    capital = TRUE;
            }
            else
            {
                *tgt++ = UpperCase[*name];
                if (*name++ != ' ')
                    capital = FALSE;
            }
        }
        *tgt = 0;
        break;
    }

    case 1: // capitalize
    {
        BOOL capital = TRUE;
        char* tgt = tgtName;
        char* name = filename;
        while (*name != 0)
        {
            if (!capital)
            {
                *tgt++ = LowerCase[*name];
                if (*name == ' ' || *name == '.')
                    capital = TRUE;
            }
            else
            {
                *tgt++ = UpperCase[*name];
                if (*name != ' ' || *name == '.')
                    capital = FALSE;
            }
            name++;
        }
        *tgt = 0;
        break;
    }

    case 2: // lower case
    {
        char* tgt = tgtName;
        char* name = filename;
        while (*name != 0)
            *tgt++ = LowerCase[*name++];
        *tgt = 0;
        break;
    }

    case 3: // upper case
    {
        char* tgt = tgtName;
        char* name = filename;
        while (*name != 0)
            *tgt++ = UpperCase[*name++];
        *tgt = 0;
        break;
    }

    case 7: // name mixed case, extension lower case
    {
        char* s = filename;
        while (*s != 0) // searching for the last dot (file extension)
            if (*s++ == '.')
                ext = s;
        //    if (ext == NULL || ext <= filename + 1) ext = s;  // ".cvspass" in Windows is considered an extension ...
        if (ext == NULL)
            ext = s;

        BOOL capital = TRUE;
        char* tgt = tgtName;
        char* name = filename;
        while (name < ext) // name mixed case
        {
            if (!capital)
            {
                *tgt++ = LowerCase[*name];
                if (*name++ == ' ')
                    capital = TRUE;
            }
            else
            {
                *tgt++ = UpperCase[*name];
                if (*name++ != ' ')
                    capital = FALSE;
            }
        }
        while (*name != 0)
            *tgt++ = LowerCase[*name++]; // extension lower case
        *tgt = 0;
        break;
    }

    default:
    {
        if (filenameLen == -1)
            strcpy(tgtName, filename);
        else
            memcpy(tgtName, filename, filenameLen + 1);
        break;
    }
    }

    if (change == 1 && format != 5 && format != 7) // change only the name
    {
        if (ext != NULL)
        {
            *--ext = '.';                            // restore '.' in the name
            strcpy(tgtName + (ext - filename), ext); // append the extension
        }
    }
}

// ****************************************************************************

void RestoreApp(HWND mainWnd, HWND dlgWnd)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    EnableWindow(mainWnd, FALSE); // because of the task list (Alt+TAB)
    if (Configuration.StatusArea)
        ShowWindow(mainWnd, SW_SHOW);
    ShowWindow(mainWnd, SW_RESTORE); // activate minimized wnd
    SetActiveWindow(dlgWnd);
    PostMessage(mainWnd, WM_NCACTIVATE, FALSE, 0);
}

// ****************************************************************************

void MinimizeApp(HWND mainWnd)
{
    ShowWindow(mainWnd, SW_MINIMIZE);
    if (Configuration.StatusArea)
        ShowWindow(mainWnd, SW_HIDE);
    EnableWindow(mainWnd, TRUE); // because of the task list (Alt+TAB)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}

// ****************************************************************************
BOOL CheckOnlyOneInstance(const CCommandLineParams* cmdLineParams)
{
    // :-) a small gift for the transition to the text config :-))))
    // load even if ForceOnlyOneInstance == TRUE
    LoadSaveToRegistryMutex.Enter();
    HKEY salamander;
    if (SALAMANDER_ROOT_REG != NULL &&
        OpenKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
    {
        HKEY actKey;
        if (OpenKey(salamander, SALAMANDER_CONFIG_REG, actKey))
        {
            GetValue(actKey, CONFIG_ONLYONEINSTANCE_REG, REG_DWORD,
                     &Configuration.OnlyOneInstance, sizeof(DWORD));
            CloseKey(actKey);
        }
        CloseKey(salamander);
    }
    LoadSaveToRegistryMutex.Leave();

    if (Configuration.ForceOnlyOneInstance || Configuration.OnlyOneInstance)
    {
        return TaskList.ActivateRunningInstance(cmdLineParams);
    }
    return FALSE;
}
/*
BOOL CheckOnlyOneInstance(const char *leftPath, const char *rightPath, const char *activePath, BYTE activatePanel)
{
  // :-) a small gift for the transition to the text config :-))))
  // load even if ForceOnlyOneInstance == TRUE
  LoadSaveToRegistryMutex.Enter();
  HKEY salamander;
  if (SALAMANDER_ROOT_REG != NULL &&
      OpenKey(HKEY_CURRENT_USER, SALAMANDER_ROOT_REG, salamander))
  {
    HKEY actKey;
    if (OpenKey(salamander, SALAMANDER_CONFIG_REG, actKey))
    {
      GetValue(actKey, CONFIG_ONLYONEINSTANCE_REG, REG_DWORD,
               &Configuration.OnlyOneInstance, sizeof(DWORD));
      CloseKey(actKey);
    }
    CloseKey(salamander);
  }
  LoadSaveToRegistryMutex.Leave();

  if (Configuration.ForceOnlyOneInstance || Configuration.OnlyOneInstance)
  {
    HWND wnd;
    int c = 100;   // wait up to five seconds to find the predecessor (it may not have opened the main window yet)
    while (c--)
    {
      wnd = FindWindow(CMAINWINDOW_CLASSNAME, NULL);
      if (wnd == NULL && !FirstLocalInstance_252b1_or_later) 
        Sleep(50);
      else 
        break; // professional optimization since 2.52 -- why loop 100 times ;-)
    }
    if (wnd != NULL)  // we have a predecessor
    {
      // allow the use of SetForegroundWindow, otherwise Salamander will not be able to bring itself to the front
      DWORD otherSalPID;
      GetWindowThreadProcessId(wnd, &otherSalPID);
      AllowSetForegroundWindow(otherSalPID);

      PostMessage(wnd, WM_USER_SHOWWINDOW, 0, 0);

      // allocate shared space in pagefile.sys
      HANDLE fm = HANDLES(CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, // FIXME_X64 are we passing x86/x64 incompatible data?
                                            sizeof(CSetPathsParams), NULL));
      if (fm != NULL)
      {
        int c = -1;
        CSetPathsParams *params = (CSetPathsParams*)HANDLES(MapViewOfFile(fm, FILE_MAP_WRITE, 0, 0, 0)); // FIXME_X64 are we passing x86/x64 incompatible data?
        if (params != NULL)
        {
          ZeroMemory(params, sizeof(CSetPathsParams));
          lstrcpyn(params->LeftPath, leftPath, MAX_PATH);
          lstrcpyn(params->RightPath, rightPath, MAX_PATH - 1); // Salamander older than 2.52 could crash on a path of length MAX_PATH - 1

          // unfortunately when receiving mapped memory we cannot determine its size to the byte (only with page-size granularity)
          // so we use a "trick" -- append a signature after the structure; if it is there, it is very likely our data
          // and the receiver can keep reading
          params->MagicSignature1 = 0x07f2ab13;
          params->MagicSignature2 = 0x471e0901;
          params->StructVersion = 1;
          lstrcpyn(params->ActivePath, activePath, MAX_PATH);
          params->ActivatePanel = activatePanel;

          // let the old process read the memory and change directories
          c = 51;  // give it 5 seconds
          while (--c)
          {
            PostMessage(wnd, WM_USER_SETPATHS, (WPARAM)GetCurrentProcessId(), (LPARAM)fm);
            Sleep(100);
            if (params->Received)
            {
              c = -1;
              break;
            }
          }

          // then we wrap it up
          HANDLES(UnmapViewOfFile(params));
        }
        HANDLES(CloseHandle(fm));
        if (c == 0)
        {
          TRACE_I("Target process is not responding.");
          return SalMessageBox(NULL, LoadStr(IDS_SALAMANDBUSY), LoadStr(IDS_QUESTION),
                               MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES;
        }
      }
      return TRUE;
    }
  }
  return FALSE;
}
*/
// ****************************************************************************

void DrawSplitLine(HWND HWindow, int newDragSplitX, int oldDragSplitX, RECT client)
{
    if (DragFullWindows)
        return;

    RECT wr;
    POINT p;
    p.x = 0;
    p.y = 0;
    ClientToScreen(HWindow, &p);
    GetWindowRect(HWindow, &wr);
    int xOffset = wr.left - p.x;
    int yOffset = wr.top - p.y;

    HDC dc = HANDLES(GetWindowDC(HWindow));
    SetViewportOrgEx(dc, -xOffset, -yOffset, &p);

    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, HDitherBrush);
    HPEN oldPen = (HPEN)SelectObject(dc, HANDLES(GetStockObject(NULL_PEN)));
    int oldROP = SetROP2(dc, R2_XORPEN); // we will AND

    int splitThick = MainWindow->GetSplitBarWidth() + 1;
    client.bottom++;
    int l0 = client.left + newDragSplitX;
    int r0 = client.left + newDragSplitX + splitThick;
    if (newDragSplitX == -1)
        r0 = l0 = -1;
    if (oldDragSplitX != -1) // compute the stripe for invalidation
    {
        int l1 = client.left + oldDragSplitX;
        int r1 = client.left + oldDragSplitX + splitThick;
        if (l1 >= l0 && l1 < r0)
        {
            int tmp = l1;
            l1 = r0 - 1;
            r0 = tmp + 1;
        }
        if (r1 > l0 && r1 <= r0)
        {
            int tmp = r1;
            r1 = l0 + 1;
            l0 = tmp - 1;
        }
        if (l1 <= r1)
        {
            Rectangle(dc, l1, client.top, r1, client.bottom);
        }
    }
    if (l0 != -1 || r0 != -1)
        Rectangle(dc, l0, client.top, r0, client.bottom);

    SetROP2(dc, oldROP);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    HANDLES(ReleaseDC(HWindow, dc));
}

// ****************************************************************************

BOOL CreateKey(HKEY hKey, const char* name, HKEY& createdKey)
{
    return RegistryWorkerThread.CreateKey(hKey, name, createdKey);
}

// ****************************************************************************

BOOL SetValue(HKEY hKey, const char* name, DWORD type, const void* data, DWORD dataSize)
{
    return RegistryWorkerThread.SetValue(hKey, name, type, data, dataSize);
}

// ****************************************************************************

BOOL OpenKey(HKEY hKey, const char* name, HKEY& openedKey)
{
    return RegistryWorkerThread.OpenKey(hKey, name, openedKey);
}

// ****************************************************************************

void CloseKey(HKEY hKey)
{
    RegistryWorkerThread.CloseKey(hKey);
}

// ****************************************************************************

BOOL DeleteKey(HKEY hKey, const char* name)
{
    return RegistryWorkerThread.DeleteKey(hKey, name);
}

// ****************************************************************************

BOOL DeleteValue(HKEY hKey, const char* name)
{
    return RegistryWorkerThread.DeleteValue(hKey, name);
}

// ****************************************************************************

BOOL GetValue(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize)
{
    return RegistryWorkerThread.GetValue(hKey, name, type, buffer, bufferSize);
}

BOOL GetValue2(HKEY hKey, const char* name, DWORD type1, DWORD type2, DWORD* returnedType, void* buffer, DWORD bufferSize)
{
    return RegistryWorkerThread.GetValue2(hKey, name, type1, type2, returnedType, buffer, bufferSize);
}

// ****************************************************************************

BOOL GetSize(HKEY hKey, const char* name, DWORD type, DWORD& bufferSize)
{
    return RegistryWorkerThread.GetSize(hKey, name, type, bufferSize);
}

// ****************************************************************************

BOOL ClearKey(HKEY key)
{
    return RegistryWorkerThread.ClearKey(key);
}

// ****************************************************************************

BOOL LoadRGB(HKEY hKey, const char* name, COLORREF& color)
{
    char buf[50];
    DWORD returnedType;
    // for backward compatibility (up to reg:\HKEY_CURRENT_USER\Software\Altap\Altap Salamander 2.53 beta 1 (DB 33) inclusive) we can load both
    // the representation as a string and the more efficient binary one
    if (GetValue2(hKey, name, REG_SZ, REG_DWORD, &returnedType, buf, 50))
    {
        if (returnedType == REG_SZ)
        {
            BOOL end;
            char *s, *st;
            BYTE c[3];
            c[0] = c[1] = c[2] = 0;
            s = st = buf;
            int i;
            for (i = 0; i < 3; i++)
            {
                while (*s != 0 && *s != ',')
                    s++;
                end = (*s == 0);
                *s = 0;
                c[i] = (BYTE)(DWORD)atoi(st);
                if (end)
                    break;
                st = ++s;
            }
            color = RGB(c[0], c[1], c[2]);
        }
        else
        {
            color = (*(DWORD*)buf) & 0x00ffffff;
        }
        return TRUE;
    }
    return FALSE;
}

// ****************************************************************************

BOOL SaveRGB(HKEY hKey, const char* name, COLORREF color)
{
    //  char buf[50];
    //  sprintf(buf, "%d, %d, %d", GetRValue(color), GetGValue(color), GetBValue(color));
    //  return SetValue(hKey, name, REG_SZ, buf, strlen(buf) + 1);
    DWORD clr = color & 0x00ffffff; // discard the "alpha" channel
    return SetValue(hKey, name, REG_DWORD, &clr, 4);
}

// ****************************************************************************

BOOL LoadRGBF(HKEY hKey, const char* name, SALCOLOR& color)
{
    char buf[50];
    DWORD returnedType;
    // for backward compatibility (up to reg:\HKEY_CURRENT_USER\Software\Altap\Altap Salamander 2.53 beta 1 (DB 33) inclusive) we can load both
    // the representation as a string and the more efficient binary one
    if (GetValue2(hKey, name, REG_SZ, REG_DWORD, &returnedType, buf, 50))
    {
        if (returnedType == REG_SZ)
        {
            BOOL end;
            char *s, *st;
            BYTE c[4];
            c[0] = c[1] = c[2] = c[3] = 0;
            s = st = buf;
            int i;
            for (i = 0; i < 4; i++)
            {
                while (*s != 0 && *s != ',')
                    s++;
                end = (*s == 0);
                *s = 0;
                c[i] = (BYTE)(DWORD)atoi(st);
                if (end)
                    break;
                st = ++s;
            }
            color = RGBF(c[0], c[1], c[2], c[3]);
        }
        else
        {
            color = *(DWORD*)buf;
        }
        return TRUE;
    }
    return FALSE;
}

// ****************************************************************************

BOOL SaveRGBF(HKEY hKey, const char* name, SALCOLOR color)
{
    //  char buf[50];
    //  sprintf(buf, "%d, %d, %d, %d", GetRValue(color), GetGValue(color), GetBValue(color), GetFValue(color));
    //  return SetValue(hKey, name, REG_SZ, buf, strlen(buf) + 1);
    return SetValue(hKey, name, REG_DWORD, &color, 4);
}

// ****************************************************************************

BOOL LoadLogFont(HKEY hKey, const char* name, LOGFONT* logFont)
{
    char buf[200];
    if (logFont != NULL && GetValue(hKey, name, REG_SZ, buf, 200))
    {
        logFont->lfHeight = -10;
        logFont->lfWidth = 0;
        logFont->lfEscapement = 0;
        logFont->lfOrientation = 0;
        logFont->lfWeight = FW_NORMAL;
        logFont->lfItalic = 0;
        logFont->lfUnderline = 0;
        logFont->lfStrikeOut = 0;
        logFont->lfCharSet = UserCharset;
        logFont->lfOutPrecision = OUT_DEFAULT_PRECIS;
        logFont->lfClipPrecision = CLIP_DEFAULT_PRECIS;
        logFont->lfQuality = DEFAULT_QUALITY;
        logFont->lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
        strcpy(logFont->lfFaceName, "MS Shell Dlg 2");

        char *s, *st;
        BOOL end;
        st = buf;
        s = buf;
        int i;
        for (i = 0; i < 9; i++)
        {
            while (*s != 0 && *s != ',')
                s++;
            end = (*s == 0);
            *s = 0;

            switch (i)
            {
            case 0: // lfFaceName
            {
                int l = (int)(s - st);
                if (l >= LF_FACESIZE)
                    l = LF_FACESIZE - 1;
                if (l > 0)
                {
                    memmove(logFont->lfFaceName, st, l);
                    logFont->lfFaceName[l] = 0;
                }
                break;
            }
            case 1:
                logFont->lfHeight = atoi(st);
                break;
            case 2:
                logFont->lfWeight = atoi(st);
                break;
            case 3:
                logFont->lfItalic = (BYTE)atoi(st);
                break;
            case 4:
                logFont->lfCharSet = (BYTE)atoi(st);
                break;
            case 5:
                logFont->lfOutPrecision = (BYTE)atoi(st);
                break;
            case 6:
                logFont->lfClipPrecision = (BYTE)atoi(st);
                break;
            case 7:
                logFont->lfQuality = (BYTE)atoi(st);
                break;
            case 8:
                logFont->lfPitchAndFamily = (BYTE)atoi(st);
                break;
            }

            if (end)
                break;
            s++;
            st = s;
        }
        return TRUE;
    }
    else
        return FALSE;
}

// ****************************************************************************

BOOL SaveLogFont(HKEY hKey, const char* name, LOGFONT* logFont)
{
    char buf[200];
    if (logFont != NULL)
    {
        char* s = buf;
        int i;
        for (i = 0; i < 9; i++)
        {
            switch (i)
            {
            case 0:
                strcpy(s, logFont->lfFaceName);
                break;
            case 1:
                itoa(logFont->lfHeight, s, 10);
                break;
            case 2:
                itoa(logFont->lfWeight, s, 10);
                break;
            case 3:
                itoa(logFont->lfItalic, s, 10);
                break;
            case 4:
                itoa(logFont->lfCharSet, s, 10);
                break;
            case 5:
                itoa(logFont->lfOutPrecision, s, 10);
                break;
            case 6:
                itoa(logFont->lfClipPrecision, s, 10);
                break;
            case 7:
                itoa(logFont->lfQuality, s, 10);
                break;
            case 8:
                itoa(logFont->lfPitchAndFamily, s, 10);
                break;
            }
            if (i < 8)
            {
                s += strlen(s);
                *s++ = ',';
                *s = 0;
            }
        }
        return SetValue(hKey, name, REG_SZ, buf, (int)(strlen(buf) + 1));
    }
    else
        return FALSE;
}

// ****************************************************************************

BOOL LoadHistory(HKEY hKey, const char* name, char* history[], int maxCount)
{
    HKEY historyKey;
    int i;
    for (i = 0; i < maxCount; i++)
        if (history[i] != NULL)
        {
            free(history[i]);
            history[i] = NULL;
        }
    if (OpenKey(hKey, name, historyKey))
    {
        char buf[10];
        for (i = 0; i < maxCount; i++)
        {
            itoa(i + 1, buf, 10);
            DWORD bufferSize;
            if (GetSize(historyKey, buf, REG_SZ, bufferSize))
            {
                history[i] = (char*)malloc(bufferSize);
                if (history[i] == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    break;
                }
                if (!GetValue(historyKey, buf, REG_SZ, history[i], bufferSize))
                    break;
            }
        }
        CloseKey(historyKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL SaveHistory(HKEY hKey, const char* name, char* history[], int maxCount, BOOL onlyClear)
{
    HKEY historyKey;
    if (CreateKey(hKey, name, historyKey))
    {
        ClearKey(historyKey);

        if (!onlyClear) // if the key should not only be cleared, store the values from history
        {
            char buf[10];
            int i;
            for (i = 0; i < maxCount; i++)
            {
                if (history[i] != NULL)
                {
                    itoa(i + 1, buf, 10);
                    SetValue(historyKey, buf, REG_SZ, history[i], (int)strlen(history[i]) + 1);
                }
                else
                    break;
            }
        }
        CloseKey(historyKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL LoadViewers(HKEY hKey, const char* name, CViewerMasks* viewerMasks)
{
    HKEY viewersKey;
    if (OpenKey(hKey, name, viewersKey))
    {
        HKEY subKey;
        char buf[30];
        strcpy(buf, "1");
        char masks[MAX_PATH];
        char command[MAX_PATH];
        char arguments[MAX_PATH];
        char initDir[MAX_PATH];
        int type;
        int i = 1;
        viewerMasks->DestroyMembers();

        while (OpenKey(viewersKey, buf, subKey))
        {
            if (GetValue(subKey, VIEWERS_MASKS_REG, REG_SZ, masks, MAX_PATH) &&
                strchr(masks, '|') == NULL &&
                GetValue(subKey, VIEWERS_TYPE_REG, REG_DWORD, &type, sizeof(DWORD)))
            {
                if (!GetValue(subKey, VIEWERS_COMMAND_REG, REG_SZ, command, MAX_PATH))
                    *command = 0;
                if (!GetValue(subKey, VIEWERS_ARGUMENTS_REG, REG_SZ, arguments, MAX_PATH))
                    *arguments = 0;
                if (!GetValue(subKey, VIEWERS_INITDIR_REG, REG_SZ, initDir, MAX_PATH))
                    *initDir = 0;

                if (Configuration.ConfigVersion < 44) // convert extensions to lowercase
                {
                    char masksAux[MAX_PATH];
                    lstrcpyn(masksAux, masks, MAX_PATH);
                    StrICpy(masks, masksAux);
                }
                CViewerMasksItem* item = new CViewerMasksItem(masks, command, arguments,
                                                              initDir, type, Configuration.ConfigVersion < 6);
                if (item != NULL && item->IsGood())
                {
                    viewerMasks->Add(item);
                    if (!viewerMasks->IsGood())
                    {
                        delete item;
                        viewerMasks->ResetState();
                        break;
                    }
                }
                else
                {
                    if (item != NULL)
                        delete item;
                    TRACE_E(LOW_MEMORY);
                    break;
                }
            }
            else
                break;
            itoa(++i, buf, 10);
            CloseKey(subKey);
        }
        CloseKey(viewersKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL SaveViewers(HKEY hKey, const char* name, CViewerMasks* viewerMasks)
{
    HKEY viewersKey;
    if (CreateKey(hKey, name, viewersKey))
    {
        ClearKey(viewersKey);
        HKEY subKey;
        char buf[30];
        int i;
        for (i = 0; i < viewerMasks->Count; i++)
        {
            itoa(i + 1, buf, 10);
            if (CreateKey(viewersKey, buf, subKey))
            {
                SetValue(subKey, VIEWERS_MASKS_REG, REG_SZ, viewerMasks->At(i)->Masks->GetMasksString(), -1);
                if (viewerMasks->At(i)->Command[0] != 0)
                    SetValue(subKey, VIEWERS_COMMAND_REG, REG_SZ, viewerMasks->At(i)->Command, -1);
                if (viewerMasks->At(i)->Arguments[0] != 0)
                    SetValue(subKey, VIEWERS_ARGUMENTS_REG, REG_SZ, viewerMasks->At(i)->Arguments, -1);
                if (viewerMasks->At(i)->InitDir[0] != 0)
                    SetValue(subKey, VIEWERS_INITDIR_REG, REG_SZ, viewerMasks->At(i)->InitDir, -1);
                SetValue(subKey, VIEWERS_TYPE_REG, REG_DWORD,
                         &viewerMasks->At(i)->ViewerType, sizeof(DWORD));
                CloseKey(subKey);
            }
            else
                break;
        }
        CloseKey(viewersKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL LoadEditors(HKEY hKey, const char* name, CEditorMasks* editorMasks)
{
    HKEY editorKey;
    if (OpenKey(hKey, name, editorKey))
    {
        HKEY subKey;
        char buf[30];
        strcpy(buf, "1");
        char masks[MAX_PATH];
        char command[MAX_PATH];
        char arguments[MAX_PATH];
        char initDir[MAX_PATH];
        int i = 1;
        editorMasks->DestroyMembers();

        while (OpenKey(editorKey, buf, subKey))
        {
            if (GetValue(subKey, EDITORS_MASKS_REG, REG_SZ, masks, MAX_PATH))
            {
                if (!GetValue(subKey, EDITORS_COMMAND_REG, REG_SZ, command, MAX_PATH))
                    *command = 0;
                if (!GetValue(subKey, EDITORS_ARGUMENTS_REG, REG_SZ, arguments, MAX_PATH))
                    *arguments = 0;
                if (!GetValue(subKey, EDITORS_INITDIR_REG, REG_SZ, initDir, MAX_PATH))
                    *initDir = 0;

                if (Configuration.ConfigVersion < 44) // convert extensions to lowercase
                {
                    char masksAux[MAX_PATH];
                    lstrcpyn(masksAux, masks, MAX_PATH);
                    StrICpy(masks, masksAux);
                }
                CEditorMasksItem* item = new CEditorMasksItem(masks, command, arguments, initDir);
                if (item != NULL && item->IsGood())
                {
                    editorMasks->Add(item);
                    if (!editorMasks->IsGood())
                    {
                        delete item;
                        editorMasks->ResetState();
                        break;
                    }
                }
                else
                {
                    if (item != NULL)
                        delete item;
                    TRACE_E(LOW_MEMORY);
                    break;
                }
            }
            else
                break;
            itoa(++i, buf, 10);
            CloseKey(subKey);
        }
        CloseKey(editorKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL SaveEditors(HKEY hKey, const char* name, CEditorMasks* editorMasks)
{
    HKEY editorKey;
    if (CreateKey(hKey, name, editorKey))
    {
        ClearKey(editorKey);
        HKEY subKey;
        char buf[30];
        int i;
        for (i = 0; i < editorMasks->Count; i++)
        {
            itoa(i + 1, buf, 10);
            if (CreateKey(editorKey, buf, subKey))
            {
                SetValue(subKey, EDITORS_MASKS_REG, REG_SZ, editorMasks->At(i)->Masks->GetMasksString(), -1);
                SetValue(subKey, EDITORS_COMMAND_REG, REG_SZ, editorMasks->At(i)->Command, -1);
                SetValue(subKey, EDITORS_ARGUMENTS_REG, REG_SZ, editorMasks->At(i)->Arguments, -1);
                SetValue(subKey, EDITORS_INITDIR_REG, REG_SZ, editorMasks->At(i)->InitDir, -1);
                CloseKey(subKey);
            }
            else
                break;
        }
        CloseKey(editorKey);
    }
    return TRUE;
}

// ****************************************************************************

void ShowFileError(HWND hParent, int errTextID, const char* fileName, DWORD err)
{
    char text[MAX_PATH + 300];
    _snprintf_s(text, _TRUNCATE, LoadStr(errTextID), fileName, GetErrorText(err));
    SalMessageBox(hParent, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
}

BOOL ExportConfiguration(HWND hParent, const char* fileName, BOOL clearKeyBeforeImport)
{
    if (SALAMANDER_ROOT_REG == NULL)
    {
        TRACE_E("ExportConfiguration(): SALAMANDER_ROOT_REG == NULL");
        return FALSE;
    }

    BOOL ret = FALSE;
    char keyName[MAX_PATH];
    _snprintf_s(keyName, _TRUNCATE, "HKEY_CURRENT_USER\\%s", SALAMANDER_ROOT_REG);
    CSalamanderRegistryExAbstract* sysReg = REG_SysRegistryFactory();
    CSalamanderRegistryExAbstract* memReg = REG_MemRegistryFactory();
    if (sysReg != NULL && memReg != NULL)
    {
        LoadSaveToRegistryMutex.Enter();
        eRPE_ERROR regerr = CopyBranch(keyName, sysReg, memReg);
        LoadSaveToRegistryMutex.Leave();
        if (RPE_OK == regerr)
        {
            memReg->RemoveHiddenKeysAndValues(); // cut out keys and values that should not be exported
            if (!memReg->Dump(fileName, clearKeyBeforeImport ? keyName : NULL))
                ShowFileError(hParent, IDS_EXPORTCFG_FILEERR, fileName, 0 /* not used */);
            else
                ret = TRUE;
        }
        else
            ShowFileError(hParent, IDS_EXPORTCFG_REGERR, fileName, 0 /* not used */);
    }
    if (sysReg != NULL)
        sysReg->Release();
    if (memReg != NULL)
        memReg->Release();

    return ret;
}

// ****************************************************************************

BOOL ImportConfiguration(HWND hParent, const char* fileName, BOOL ignoreIfNotExists,
                         BOOL autoImportConfig, BOOL* importCfgFromFileWasSkipped)
{
    TRACE_I("ImportConfiguration(): begin");
    DWORD err = 0;
    HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                                       OPEN_EXISTING, 0, 0));
    if (file == INVALID_HANDLE_VALUE)
    {
        err = GetLastError();
        if (!ignoreIfNotExists || err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)
            ShowFileError(hParent, IDS_IMPORTCFG_OPENERR, fileName, err);
        TRACE_I("ImportConfiguration(): end");
        return FALSE;
    }

    if (autoImportConfig)
    {
        HANDLES(CloseHandle(file));
        *importCfgFromFileWasSkipped = TRUE;
        TRACE_I("ImportConfiguration(): end");
        return FALSE;
    }

    IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_IMPORT_CONFIG));

    BOOL ret = FALSE;
    LPTSTR buf = NULL;
    CQuadWord size;
    if (SalGetFileSize(file, size, err))
    {
        if (size <= CQuadWord(10000000, 0)) // above 10MB it is 100% nonsense...
        {
            buf = (LPTSTR)malloc((DWORD)size.Value + sizeof(WCHAR));
            if (buf != NULL) // "always true" (in case of an error we just don’t crash; the user dismissed the out-of-memory message)
            {
                DWORD bytesRead;
                if (!ReadFile(file, buf, (DWORD)size.Value, &bytesRead, NULL))
                {
                    ShowFileError(hParent, IDS_IMPORTCFG_OPENERR, fileName, GetLastError());
                    free(buf);
                    buf = NULL;
                }
                else
                {
                    if ((DWORD)size.Value > bytesRead)
                    {
                        size.Set(bytesRead, 0);
                        TRACE_E("ImportConfiguration(): reading only " << bytesRead << " bytes from configuration file (" << fileName << ")");
                    }
                }
            }
        }
        else
            ShowFileError(hParent, IDS_IMPORTCFG_TOOBIG, fileName, 0 /* not used */);
    }
    else
        ShowFileError(hParent, IDS_IMPORTCFG_OPENERR, fileName, GetLastError());

    HANDLES(CloseHandle(file));

    if (buf != NULL && (DWORD)size.Value > 0)
    {
        *(WCHAR*)((LPBYTE)buf + (DWORD)size.Value) = 0; // safety net for too short file
        if (ConvertIfNeeded(&buf, (DWORD)size.Value) == 0)
        { // "always false" (in case of an error we just don’t crash; the user dismissed the out-of-memory message)
            free(buf);
            buf = NULL;
        }
    }

    if (buf != NULL)
    {
        // first try to parse it into memory; if it contains format errors we will not shove it into the registry at all
        CSalamanderRegistryExAbstract* memReg = REG_MemRegistryFactory();
        LPTSTR bufMem = _tcsdup(buf); // the Parse call changes the buffer, so we must keep the original for the next Parse
        TRACE_I("ImportConfiguration(): Parse to memory: begin");
        eRPE_ERROR regerr = bufMem != NULL ? Parse(bufMem, memReg, TRUE) : RPE_OUT_OF_MEMORY; // dirty hack: when deleting the configuration key, we do not remove .hidden keys and values (because of the trial version + checkver)
        TRACE_I("ImportConfiguration(): Parse to memory: end");
        free(bufMem);
        BOOL verIsOK = RPE_OK == regerr; // verify whether the file even contains our configuration version
        if (verIsOK)
        {
            HKEY key;
            if (memReg->OpenKey(HKEY_CURRENT_USER, SalamanderConfigurationRoots[0], key))
                memReg->CloseKey(key);
            else
            {
                char text[MAX_PATH + 300];
                _snprintf_s(text, _TRUNCATE, LoadStr(IDS_IMPORTCFG_NOTOURVER), fileName);
                if (SalMessageBox(hParent, text, LoadStr(IDS_QUESTION),
                                  MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
                {
                    verIsOK = FALSE;
                }
            }
        }
        memReg->Release();
        if (verIsOK && RPE_OK == regerr) // both the config version and the file itself look OK; import it into the registry
        {
            CSalamanderRegistryExAbstract* sysReg = REG_SysRegistryFactory();

            LoadSaveToRegistryMutex.Enter();
            TRACE_I("ImportConfiguration(): Parse to registry: begin");
            regerr = Parse(buf, sysReg, TRUE); // dirty hack: when deleting the configuration key, we do not remove .hidden keys and values (because of the trial version + checkver)
            TRACE_I("ImportConfiguration(): Parse to registry: end");
            if (RPE_OK == regerr)
                ret = TRUE; // success
            LoadSaveToRegistryMutex.Leave();

            Configuration.ConfigWasImported = TRUE;
            sysReg->Release();
        }
        if (RPE_OK != regerr)
        {
            int errTextID = IDS_IMPORTCFG_REGERR;
            switch (regerr)
            {
            case RPE_NOT_REG_FILE:
                errTextID = IDS_IMPORTCFG_NOTREG;
                break; // not a reg4 or reg5 file

            case RPE_ROOT_INVALID_KEY:
            case RPE_INVALID_KEY:
            case RPE_VALUE_MISSING_QUOTE:
            case RPE_VALUE_MISSING_ASSIG:
            case RPE_VALUE_INVALID_TYPE:
            case RPE_VALUE_DWORD:
            case RPE_VALUE_STRING:
            case RPE_VALUE_HEX:
            case RPE_INVALID_MBCS:
            case RPE_INVALID_FORMAT:
                errTextID = IDS_IMPORTCFG_INVALIDFORMAT;
                break; // format error

                // case RPE_OUT_OF_MEMORY:
                // case RPE_KEY_OPEN:
                // case RPE_KEY_CREATE:
                // case RPE_VALUE_GET_SIZE:
                // case RPE_VALUE_GET:
                // case RPE_VALUE_SET: errTextID = IDS_IMPORTCFG_REGERR; break;   // other error (memory + registry write failure)
            }
            ShowFileError(hParent, errTextID, fileName, 0 /* not used */);
        }

        free(buf);
    }
    TRACE_I("ImportConfiguration(): end");
    return ret;
}

//****************************************************************************
//
// CLanguage
//

const char* RT_SLGSIGN = "SLGSIGN";

typedef DWORD(WINAPI* FSalamanderLanguageEntry)();

CLanguage::CLanguage()
{
    FileName = NULL;
    LanguageID = 0;
    AuthorW = NULL;
    Web = NULL;
    CommentW = NULL;
    HelpDir = NULL;
}

void CLanguage::Free()
{
    if (FileName != NULL)
    {
        free(FileName);
        FileName = NULL;
    }
    if (AuthorW != NULL)
    {
        free(AuthorW);
        AuthorW = NULL;
    }
    if (Web != NULL)
    {
        free(Web);
        Web = NULL;
    }
    if (CommentW != NULL)
    {
        free(CommentW);
        CommentW = NULL;
    }
    if (HelpDir != NULL)
    {
        free(HelpDir);
        HelpDir = NULL;
    }
}

BOOL CLanguage::Init(const char* fileName, WORD languageID, const WCHAR* authorW,
                     const char* web, const WCHAR* commentW, const char* helpdir)
{
    LanguageID = languageID;
    FileName = DupStr(fileName);
    AuthorW = DupStr(authorW);
    Web = DupStr(web);
    CommentW = DupStr(commentW);
    HelpDir = DupStr(helpdir);
    if (FileName == NULL || AuthorW == NULL || Web == NULL || CommentW == NULL || HelpDir == NULL)
    {
        TRACE_E(LOW_MEMORY);
        Free();
        return FALSE;
    }
    return TRUE;
}

/*
// a copy of this routine also exists in the Translator program
BOOL LoadSLGData(HINSTANCE hModule, const char *resName, LPVOID buff, int buffSize, BOOL string)
{
  HRSRC hrsrc = FindResource(hModule, resName, RT_SLGSIGN);
  if (hrsrc != NULL)
  {
    int size = SizeofResource(hModule, hrsrc);
    if (size > 0)
    {
      HGLOBAL hglb = LoadResource(hModule, hrsrc);
      if (hglb != NULL)
      {
        LPVOID data = LockResource(hglb);
        if (data != NULL)
        {
          ZeroMemory(buff, buffSize);
          if (string)
          {
            int sz = min(buffSize - 1, size);
            strncpy_s((char*)buff, buffSize, (char*)data, sz);
          }
          else
          {
            int sz = min(buffSize, size);
            memcpy(buff, data, sz);
          }
          return TRUE;
        }
      }
    }
  }
  ZeroMemory(buff, buffSize);
  return FALSE;
}
*/

BOOL IsSLGFileValid(HINSTANCE hModule, HINSTANCE hSLG, WORD& slgLangID, char* isIncomplete)
{
    // compare the SLG VERSIONINFO version against Salamander's and return TRUE if they match,
    // otherwise FALSE; in case of a match also extract \\VarFileInfo\\Translation and set 'langID'
    CVersionInfo slgVer;
    CVersionInfo moduleVer;

    char path[MAX_PATH];
    BYTE *slgBuf, *moduleBuf;
    DWORD slgSize, moduleSize;

    if (!slgVer.ReadResource(hSLG, VS_VERSION_INFO))
    {
        GetModuleFileName(hSLG, path, MAX_PATH);
        TRACE_E("Unable to load VERSIONINFO resource from SLG module: " << path);
        return FALSE;
    }
    if (!moduleVer.ReadResource(hModule, VS_VERSION_INFO))
    {
        GetModuleFileName(hModule, path, MAX_PATH);
        TRACE_E("Unable to load VERSIONINFO resource from plugin: " << path);
        return FALSE;
    }

    // retrieve pointers to the VS_FIXEDFILEINFO structures
    if (!slgVer.QueryValue("\\", &slgBuf, &slgSize))
        return FALSE;
    if (!moduleVer.QueryValue("\\", &moduleBuf, &moduleSize))
        return FALSE;

    // the SLG version must be identical to our version
    if (((VS_FIXEDFILEINFO*)slgBuf)->dwFileVersionMS != ((VS_FIXEDFILEINFO*)moduleBuf)->dwFileVersionMS ||
        ((VS_FIXEDFILEINFO*)slgBuf)->dwFileVersionLS != ((VS_FIXEDFILEINFO*)moduleBuf)->dwFileVersionLS)
    {
        char ver1[20];
        sprintf(ver1, "%08X%08X", ((VS_FIXEDFILEINFO*)moduleBuf)->dwFileVersionMS,
                ((VS_FIXEDFILEINFO*)moduleBuf)->dwFileVersionLS);
        char ver2[20];
        sprintf(ver2, "%08X%08X", ((VS_FIXEDFILEINFO*)slgBuf)->dwFileVersionMS,
                ((VS_FIXEDFILEINFO*)slgBuf)->dwFileVersionLS);
        GetModuleFileName(hModule, path, MAX_PATH);
        TRACE_E("Plugin and SLG module are not of the same version (0x" << ver1 << " != 0x" << ver2 << "). Plugin: " << path);
        GetModuleFileName(hSLG, path, MAX_PATH);
        TRACE_E("... SLG module: " << path);
        return FALSE;
    }

    if (isIncomplete != NULL)
    {
        isIncomplete[0] = 0;
        if (!slgVer.QueryString("\\StringFileInfo\\040904b0\\SLGIncomplete", isIncomplete, ISSLGINCOMPLETE_SIZE))
        {
            GetModuleFileName(hSLG, path, MAX_PATH);
            TRACE_E("Missing SLGIncomplete value in VERSIONINFO resource in SLG module: " << path);
            return FALSE;
        }
    }

    // extract the language in which the SLG is written
    if (!slgVer.QueryValue("\\VarFileInfo\\Translation", &slgBuf, &slgSize))
    {
        GetModuleFileName(hSLG, path, MAX_PATH);
        TRACE_E("Missing Translation value in VERSIONINFO resource in SLG module: " << path);
        return FALSE;
    }

    slgLangID = *((WORD*)slgBuf);

    return TRUE;
}

BOOL CLanguage::Init(const char* fileName, HINSTANCE modul)
{
    BOOL ret = FALSE;
    char path[MAX_PATH];
    if (modul == NULL)
        modul = HInstance;
    GetModuleFileName(modul, path, MAX_PATH);
    sprintf(strrchr(path, '\\') + 1, "lang\\%s", fileName);

    HINSTANCE hLib = HANDLES(LoadLibrary(path));
    if (hLib != NULL)
    {
        WORD langID;
        if (IsSLGFileValid(modul, hLib, langID, NULL))
        {
            CVersionInfo ver;
            WCHAR slg_athorW[500] = {0};
            char slg_web[500] = {0};
            WCHAR slg_commentW[500] = {0};
            char slg_helpdir[100] = {0};
            char slg_incomplete[200] = {0};

            BOOL ok = TRUE;
            if (ok)
            {
                ok &= ver.ReadResource(hLib, VS_VERSION_INFO);
                if (!ok)
                    TRACE_E("Missing VERSIONINFO resource in language file " << path);
            }
            if (ok)
            {
                ok &= ver.QueryString("\\StringFileInfo\\040904b0\\SLGAuthor", NULL, 0, slg_athorW, _countof(slg_athorW));
                if (!ok)
                    TRACE_E("Missing SLGAuthor in VERSIONINFO resource in language file " << path);
            }
            if (ok)
            {
                ok &= ver.QueryString("\\StringFileInfo\\040904b0\\SLGWeb", slg_web, _countof(slg_web));
                if (!ok)
                    TRACE_E("Missing SLGWeb in VERSIONINFO resource in language file " << path);
            }
            if (ok)
            {
                ok &= ver.QueryString("\\StringFileInfo\\040904b0\\SLGComment", NULL, 0, slg_commentW, _countof(slg_commentW));
                if (!ok)
                    TRACE_E("Missing SLGComment in VERSIONINFO resource in language file " << path);
            }
            if (ok)
            {
                if (!ver.QueryString("\\StringFileInfo\\040904b0\\SLGHelpDir", slg_helpdir, _countof(slg_helpdir)))
                {
                    slg_helpdir[0] = 0; // plugins do not have SLGHelpDir defined (used only in Salamander's .slg)
                    if (modul == HInstance)
                        ok = FALSE; // however this variable cannot be missing in Salamander
                }
                // read only to test that the item exists
                if (!ver.QueryString("\\StringFileInfo\\040904b0\\SLGIncomplete", slg_incomplete, _countof(slg_incomplete)))
                {
                    slg_incomplete[0] = 0; // plugins do not have SLGIncomplete defined (used only in Salamander's .slg)
                    if (modul == HInstance)
                        ok = FALSE; // however this variable cannot be missing in Salamander
                }
            }
            if (ok)
                ok &= Init(fileName, langID, slg_athorW, slg_web, slg_commentW, slg_helpdir);
            if (ok)
                ret = TRUE;
        }
        else
            TRACE_E("SLG is not valid (or plugin's VERSIONINFO resource is not set properly): " << path);

        HANDLES(FreeLibrary(hLib));
    }
    else
        TRACE_E("Cannot load SLG module " << path);
    return ret;
}

BOOL CLanguage::GetLanguageName(char* buffer, int bufferSize)
{
    if (GetLocaleInfo(MAKELCID(LanguageID, SORT_DEFAULT), LOCALE_SLANGUAGE, buffer, bufferSize) == 0)
    {
        lstrcpyn(buffer, "?", bufferSize);
    }
    return TRUE;
}
