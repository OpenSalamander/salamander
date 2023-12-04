// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// menu item skill level ALL (zahrnuje beginned, intermediate a advanced)
#define MNTS_ALL (MNTS_B | MNTS_I | MNTS_A)

#ifndef QWORD
typedef unsigned __int64 QWORD;
typedef QWORD* LPQWORD;
#define LODWORD(qw) ((DWORD)qw)
#define HIDWORD(qw) ((DWORD)(qw >> 32))
#define MAKEQWORD(lo, hi) (((QWORD)hi << 32) + lo)
#endif

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

#define MAX_HISTORY_ENTRIES 20

#define elif else if

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define CLAMP(a, lb, ub) (__min(__max(a, lb), ub))

#define SUPER(sc) \
private: \
    typedef sc super

// ****************************************************************************
//
// utilbase.cpp
//

extern HINSTANCE DLLInstance;     // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;       // handle k SLG-cku - jazykove zavisle resourcy
extern BOOL WindowsVistaAndLater; // Windows Vista nebo pozdejsi z rady NT (6.0+)
extern BOOL WindowsXP64AndLater;  // Windows XP 64, Vista or later (5.2+)

// rozhrani Open Salamandera - platna od volani InitUtils() az do
// ukonceni pluginu
extern CSalamanderGeneralAbstract* SG;
extern CSalamanderGUIAbstract* SalGUI;

// ****************************************************************************
//
// CCS -- samo-inicializujici se a samo-destruujici se kriticka sekce
//

struct CCS
{
    CRITICAL_SECTION cs;

    CCS() { InitializeCriticalSection(&cs); }
    ~CCS() { DeleteCriticalSection(&cs); }

    void Enter() { EnterCriticalSection(&cs); }
    void Leave() { LeaveCriticalSection(&cs); }
};

// ****************************************************************************

BOOL InitLCUtils(CSalamanderPluginEntryAbstract* salamander, const char* pluginName);
void ReleaseLCUtils();

char* LoadStr(int resID);

BOOL ErrorHelper(HWND parent, const char* message, int lastError, va_list arglist);
BOOL Error(HWND parent, int error, ...);
BOOL Error(HWND parent, const char* error, ...);
BOOL Error(int error, ...);
BOOL ErrorL(int lastError, HWND parent, int error, ...);
BOOL ErrorL(int lastError, int error, ...);
int SalPrintf(char* buffer, unsigned count, const char* format, ...);
const char* FormatString(const char* format, ...);
const char* Concatenate(const char* string1, const char* string2);

// ****************************************************************************
//
// CDialogStack -- slouzi k ukladani hiearchie oken na zasobnik, vrchol
// zasobniku je parent pro zobrazovane message- a error-boxy, kazdy thread
// ma svou vlastni hierarchii oken
//

class CDialogStack
{
    TDirectArray<uintptr_t> Stack;
    CCS CS;

public:
    CDialogStack() : Stack(4, 4) { ; }
    void Push(HWND hWindow);
    void Pop();
    HWND Peek();
    HWND GetParent();
};

extern CDialogStack DialogStack;

class CDialogStackAutoObject
{
public:
    CDialogStackAutoObject(HWND hWindow) { DialogStack.Push(hWindow); }
    ~CDialogStackAutoObject() { DialogStack.Pop(); }
};

#define PARENT(wnd) CDialogStackAutoObject dsao(wnd);

// ****************************************************************************
//
// utildlg.cpp
//

// ****************************************************************************
//
// CDialogEx -- automaticky centrovany dialog, automaticky pridava a odebira
// HWND do/z DialogStack
//

class CDialogEx : public CDialog
{
protected:
    HWND CenterToHWnd;

public:
    CDialogEx(int resID, HWND parent, HWND centerToHWnd,
              CObjectOrigin origin = ooStandard) : CDialog(HLanguage, resID, parent, origin = ooStandard)
    {
        CenterToHWnd = centerToHWnd;
    }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

// ****************************************************************************

void HistoryComboBox(CTransferInfo& ti, int id, char* text, int textMax,
                     int historySize, char** history);

void LoadHistory(HKEY regKey, const char* keyPattern, char** history,
                 char* buffer, int bufferSize, CSalamanderRegistryAbstract* registry);

void SaveHistory(HKEY regKey, const char* keyPattern, char** history,
                 CSalamanderRegistryAbstract* registry);

// ****************************************************************************
//
// utilaux1.cpp
//

BOOL GetOpenFileName(HWND parent, const char* title, const char* filter,
                     char* buffer, BOOL save = FALSE);

BOOL FileErrorL(int lastError, HWND parent, const char* fileName, int error,
                BOOL retry, BOOL* skip = NULL, BOOL* skipAll = NULL,
                int title = -1);

inline BOOL FileError(HWND parent, const char* fileName, int error,
                      BOOL retry, BOOL* skip = NULL, BOOL* skipAll = NULL,
                      int title = -1)
{
    return FileErrorL(GetLastError(), parent, fileName, error,
                      retry, skip, skipAll, title);
}

// ****************************************************************************
//
// CSynchronizedCounter
//

class CSynchronizedCounter
{
    CCS CS;
    int Counter;
    HANDLE ChangeEvent;

public:
    CSynchronizedCounter();
    ~CSynchronizedCounter();
    int Up();
    int Down();
    int Value();
    DWORD WaitForChange();
};

// ****************************************************************************
//
// CArgv -- Vytvori argument-vector z commanline retezce
//

class CArgv : public TIndirectArray<char>
{
public:
    CArgv(const char* commandLine);

protected:
    virtual void CallDestructor(void*& member)
    {
        if (member)
            free((char*)member);
    }
};

// ****************************************************************************

char* DupStr(const char* begin, const char* end);
int RemoveCharacters(char* dest, const char* source, const char* charSet);
BOOL SalGetFullName(char* name, int* errTextID, const char* curDir);
