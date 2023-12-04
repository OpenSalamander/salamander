// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy
BOOL WindowsVistaAndLater;    // Windows Vista nebo pozdejsi z rady NT (6.0+)
BOOL WindowsXP64AndLater;     // Windows XP 64, Vista or later (5.2+)

// rozhrani Open Salamandera - platna od volani InitUtils() az do
// ukonceni pluginu
CSalamanderGeneralAbstract* SG = NULL;
CSalamanderGUIAbstract* SalGUI = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

DWORD MainThreadID;

CDialogStack DialogStack;

BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    CALL_STACK_MESSAGE_NONE
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DLLInstance = hinstDLL;
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        break;
    }
    }
    return TRUE; // DLL can be loaded
}

BOOL InitLCUtils(CSalamanderPluginEntryAbstract* salamander, const char* pluginName)
{
    CALL_STACK_MESSAGE_NONE

    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();

    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("InitLCUtils()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // tady nelze volat Error, protoze pouziva SG->SalMessageBox (SG neni inicializovane + jde o nekompatibilni rozhrani)
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   pluginName, MB_OK | MB_ICONERROR);
        return FALSE;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), pluginName);
    if (HLanguage == NULL)
        return FALSE;

    // ziskame rozhrani Salamandera
    SG = salamander->GetSalamanderGeneral();
    SalGUI = salamander->GetSalamanderGUI();

    // zjistime si na jakem bezime OS
    WindowsXP64AndLater = SalIsWindowsVersionOrGreater(5, 2, 0);
    WindowsVistaAndLater = SalIsWindowsVersionOrGreater(6, 0, 0);

    MainThreadID = GetCurrentThreadId();

    return TRUE;
}

void ReleaseLCUtils()
{
    CALL_STACK_MESSAGE_NONE
}

char* LoadStr(int resID)
{
    return SG->LoadStr(HLanguage, resID);
}

BOOL ErrorHelper(HWND parent, const char* message, int lastError, va_list arglist)
{
    CALL_STACK_MESSAGE3("ErrorHelper(, %s, %d, )", message, lastError);
    char buf[1024];
    *buf = 0;
    vsprintf(buf, message, arglist);
    if (lastError != ERROR_SUCCESS)
    {
        int l = lstrlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    if (SG)
    {
        if (parent == HWND(-1))
            parent = GetCurrentThreadId() == MainThreadID ? SG->GetMsgBoxParent() : NULL;
        SG->SalMessageBox(parent, buf, LoadStr(IDS_SPLERROR), MB_OK | MB_ICONERROR | (parent == NULL && AlwaysOnTop ? MB_TOPMOST : 0));
    }
    else
    {
        if (parent == HWND(-1))
            parent = 0;
        MessageBox(parent, buf, LoadStr(IDS_SPLERROR), MB_OK | MB_ICONERROR | (parent == NULL && AlwaysOnTop ? MB_TOPMOST : 0));
    }
    return FALSE;
}

BOOL Error(HWND parent, int error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastError = GetLastError();
    CALL_STACK_MESSAGE2("Error(, %d, )", error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(parent, LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL Error(HWND parent, const char* error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastError = GetLastError();
    CALL_STACK_MESSAGE2("Error(, %s, )", error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(parent, error, lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL Error(int error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastError = GetLastError();
    CALL_STACK_MESSAGE2("Error(%d, )", error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(DialogStack.Peek(), LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL ErrorL(int lastError, HWND parent, int error, ...)
{
    CALL_STACK_MESSAGE3("ErrorL(%d, , %d, )", lastError, error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(parent, LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL ErrorL(int lastError, int error, ...)
{
    CALL_STACK_MESSAGE3("ErrorL(%d, %d, )", lastError, error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(DialogStack.Peek(), LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

int SalPrintf(char* buffer, unsigned count, const char* format, ...)
{
    CALL_STACK_MESSAGE_NONE
    va_list arglist;
    va_start(arglist, format);
    int ret = _vsnprintf_s(buffer, count, _TRUNCATE, format, arglist);
    va_end(arglist);
    return ret;
}

const char*
FormatString(const char* format, ...)
{
    CALL_STACK_MESSAGE_NONE
    static char buffer[5120];
    static int iterator = 0;

    if (iterator > 4096)
        iterator = 0;
    char* actual = buffer + iterator;
    int size = 5120 - iterator;

    va_list arglist;
    va_start(arglist, format);
    int ret = _vsnprintf_s(actual, size, _TRUNCATE, format, arglist);
    iterator += ret >= 0 ? ret + 1 : size;
    va_end(arglist);

    actual[size - 1] = 0;
    return actual;
}

const char*
Concatenate(const char* string1, const char* string2)
{
    CALL_STACK_MESSAGE_NONE
    static char buffer[5120];
    static int iterator = 0;

    int len1 = lstrlen(string1);
    int len2 = lstrlen(string2);

    if (len1 + len2 >= 5120)
        return "STRING TOO LONG";
    if (iterator + len1 + len2 >= 5120)
        iterator = 0;

    const char* ret = buffer + iterator;
    memcpy(buffer + iterator, string1, len1);
    iterator += len1;
    memcpy(buffer + iterator, string2, len2);
    iterator += len2;

    buffer[iterator++] = 0;
    return ret;
}

// ****************************************************************************
//
// CDialogStack
//

void CDialogStack::Push(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CDialogStack::Push()");
    CS.Enter();
    Stack.Add(uintptr_t(hWindow));
    Stack.Add(GetCurrentThreadId());
    CS.Leave();
}

void CDialogStack::Pop()
{
    CALL_STACK_MESSAGE1("CDialogStack::Pop()");
    CS.Enter();
    int i = Stack.Count - 1;
    while (i > 0)
    {
        if (Stack[i] == GetCurrentThreadId())
        {
            Stack.Delete(i--);
            Stack.Delete(i);
            break;
        }
        i -= 2;
    }
    CS.Leave();
}

HWND CDialogStack::Peek()
{
    CALL_STACK_MESSAGE1("CDialogStack::Peek()");
    CS.Enter();
    HWND ret = (HWND)-1;
    int i = Stack.Count - 1;
    while (i > 0)
    {
        if (Stack[i] == GetCurrentThreadId())
        {
            ret = HWND(Stack[--i]);
            break;
        }
        i -= 2;
    }
    CS.Leave();
    return ret;
}

HWND CDialogStack::GetParent()
{
    CALL_STACK_MESSAGE1("CDialogStack::GetParent()");
    HWND ret = Peek();
    if (ret == (HWND)-1)
        ret = GetCurrentThreadId() == MainThreadID ? SG->GetMsgBoxParent() : NULL;
    return ret;
}
