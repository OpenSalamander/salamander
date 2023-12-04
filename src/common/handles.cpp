// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h> // potrebuju LPCOLORMAP

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#ifdef HANDLES_ENABLE

// The order here is important.
// Section names must be 8 characters or less.
// The sections with the same name before the $
// are merged into one section. The order that
// they are merged is determined by sorting
// the characters after the $.
// i_handles and i_handles_end are used to set
// boundaries so we can find the real functions
// that we need to call for initialization.

#pragma warning(disable : 4075) // chceme definovat poradi inicializace modulu

typedef void(__cdecl* _PVFV)(void);

#pragma section(".i_hnd$a", read)
__declspec(allocate(".i_hnd$a")) const _PVFV i_handles = (_PVFV)1; // na zacatek sekce .i_hnd si dame promennou i_handles

#pragma section(".i_hnd$z", read)
__declspec(allocate(".i_hnd$z")) const _PVFV i_handles_end = (_PVFV)1; // a na konec sekce .i_hnd si dame promennou i_handles_end

void Initialize__Handles()
{
    const _PVFV* x = &i_handles;
    for (++x; x < &i_handles_end; ++x)
        if (*x != NULL)
            (*x)();
}

#pragma init_seg(".i_hnd$m")

#ifdef MULTITHREADED_HANDLES_ENABLE
#if defined(__HEADER_TRACE_H) && !defined(_INC_PROCESS)
#error "Your precomp.h includes trace.h, so it must also earlier include process.h."
#endif // defined(__HEADER_TRACE_H) && !defined(_INC_PROCESS)
#include <process.h>
#endif // MULTITHREADED_HANDLES_ENABLE

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"
#include "messages.h"
#include "handles.h"

#ifdef MESSAGES_DISABLE
#error "macro MESSAGES_DISABLE defined - unable to show errors"
#endif // MESSAGES_DISABLE

#ifndef TRACE_ENABLE
#define __HANDLES_STR2(x) #x
#define __HANDLES_STR(x) __HANDLES_STR2(x)
#pragma message(__FILE__ "(" __HANDLES_STR(__LINE__) "): Maybe Error: macro TRACE_ENABLE not defined - unable to list handles")
#endif // TRACE_ENABLE

#if defined(MULTITHREADED_HANDLES_ENABLE) && !defined(MULTITHREADED_MESSAGES_ENABLE)
#error "macro MULTITHREADED_MESSAGES_ENABLE not defined"
#endif // defined(MULTITHREADED_HANDLES_ENABLE) && !defined(MULTITHREADED_MESSAGES_ENABLE)

C__Handles __Handles;

#ifndef MULTITHREADED_HANDLES_ENABLE
DWORD __HandlesMainThreadID = 0;
#endif // MULTITHREADED_HANDLES_ENABLE

const char* __HandlesMessageCallFrom = "Called from: ";
const WCHAR* __HandlesMessageCallFromW = L"Called from: ";
const char* __HandlesMessageSpace = " ";
const WCHAR* __HandlesMessageSpaceW = L" ";
const char* __HandlesMessageLineEnd = ":\n\n";
const WCHAR* __HandlesMessageLineEndW = L":\n\n";
const char* __HandlesMessageNumberOpened = "Number of opened handles: ";

const char* __HandlesMessageBadType = "Error in function %s:\n"
                                      "Bad type of handle: %s.\n"
                                      "Expected type: %s.\n"
                                      "Handle value: 0x%p.";
const char* __HandlesMessageNotFound = "Error in function %s: Handle 0x%p not found.";
const char* __HandlesMessageReturnErrorMessage = "Error in function %s:\n%s";
const char* __HandlesMessageReturnError = "Error in function %s.";

const char* __HandlesMessageReturnErrorMessageParams = "Error in function %s:\n%s\nFunction parameters:\n%s";
const char* __HandlesMessageReturnErrorParams = "Error in function %s.\n\nFunction parameters:\n%s";
const WCHAR* __HandlesMessageReturnErrorMessageParamsW = L"Error in function %S:\n%s\nFunction parameters:\n%s";
const WCHAR* __HandlesMessageReturnErrorParamsW = L"Error in function %S.\n\nFunction parameters:\n%s";

const char* __HandlesMessageAlreadyExists = "Error in function %s:\n"
                                            "Named kernel object already exists.\n"
                                            "Type: %s, Name: %s";
const char* __HandlesMessageNotMultiThreaded = "Incorrect use of modul HANDLES.\n"
                                               "Multithreaded application must "
                                               "use multithreaded version of module.";
const char* __HandlesMessageLowMemory = "Insufficient memory for modul HANDLES.";

//*****************************************************************************
//
// __GetHandlesTypeName
//

const char* __GetHandlesTypeName(C__HandlesType type)
{
    switch (type)
    {
    case __htKey:
        return "Registry Key";
    case __htIcon:
        return "Icon";
    case __htHandle_comp_with_CloseHandle:
        return "Handle compatible with CloseHandle";
    case __htHandle_comp_with_DeleteObject:
        return "Handle compatible with DeleteObject";
    case __htGlobal:
        return "Global Memory";
    case __htMetaFile:
        return "Metafile";
    case __htEnhMetaFile:
        return "Enhanced Metafile";
    case __htCursor:
        return "Cursor";
    case __htViewOfFile:
        return "View of File";
    case __htAccel:
        return "Accelerator Table";
    case __htCriticalSection:
        return "Critical Section";
    case __htEnterCriticalSection:
        return "Enter to Critical Section";
    case __htFindFile:
        return "Find File";
    case __htFiber:
        return "Fiber";
    case __htThreadLocalStorage:
        return "Thread Local Storage";
    case __htLibrary:
        return "Library";
    case __htGlobalLock:
        return "Lock of Global Memory";
    case __htChangeNotification:
        return "Change Notification";
    case __htEnvStrings:
        return "Environment Strings";
    case __htLocal:
        return "Local Memory";
    case __htLocalLock:
        return "Lock of Local Memory";
    case __htDropTarget:
        return "Window with DropTarget Registered";

    case __htDC:
        return "DC";
    case __htPen:
        return "Pen";
    case __htBrush:
        return "Brush";
    case __htFont:
        return "Font";
    case __htBitmap:
        return "Bitmap";
    case __htRegion:
        return "Region";
    case __htPalette:
        return "Palette";

    case __htFile:
        return "File";
    case __htThread:
        return "Thread";
    case __htProcess:
        return "Process";
    case __htEvent:
        return "Event";
    case __htMutex:
        return "Mutex";
    case __htSemaphore:
        return "Semaphore";
    case __htWaitableTimer:
        return "Waitable Timer";
    case __htFileMapping:
        return "File Mapping";
    case __htMailslot:
        return "Mailslot";
    case __htReadPipe:
        return "Read End of Pipe";
    case __htWritePipe:
        return "Write End of Pipe";
    case __htServerNamedPipe:
        return "Server End of Named Pipe";
    case __htConsoleScreenBuffer:
        return "Console Screen Buffer";
    case __htDeferWindowPos:
        return "Defer Window Pos";
    case __htThreadToken:
        return "Thread Token";
    case __htProcessToken:
        return "Process Token";
    }
    return "Unknown";
}

//*****************************************************************************
//
// __GetHandlesOrigin
//

const char* __GetHandlesOrigin(C__HandlesOrigin origin)
{
    switch (origin)
    {
    case __hoCreateFile:
        return "CreateFile";
    case __hoCreateBrushIndirect:
        return "CreateBrushIndirect";
    case __hoGetDC:
        return "GetDC";
    case __hoCreateCompatibleBitmap:
        return "CreateCompatibleBitmap";
    case __hoCreatePen:
        return "CreatePen";
    case __hoBeginPaint:
        return "BeginPaint";
    case __hoCreateCompatibleDC:
        return "CreateCompatibleDC";
    case __hoCreateRectRgn:
        return "CreateRectRgn";
    case __hoCreateBitmap:
        return "CreateBitmap";
    case __hoCreateBitmapIndirect:
        return "CreateBitmapIndirect";
    case __hoCreateMetaFile:
        return "CreateMetaFile";
    case __hoCreateEnhMetaFile:
        return "CreateEnhMetaFile";
    case __hoCloseMetaFile:
        return "CloseMetaFile";
    case __hoCloseEnhMetaFile:
        return "CloseEnhMetaFile";
    case __hoCopyMetaFile:
        return "CopyMetaFile";
    case __hoCopyEnhMetaFile:
        return "CopyEnhMetaFile";
    case __hoGetEnhMetaFile:
        return "GetEnhMetaFile";
    case __hoSetWinMetaFileBits:
        return "SetWinMetaFileBits";
    case __hoCreateIcon:
        return "CreateIcon";
    case __hoCreateIconIndirect:
        return "CreateIconIndirect";
    case __hoCreateIconFromResource:
        return "CreateIconFromResource";
    case __hoCreateIconFromResourceEx:
        return "CreateIconFromResourceEx";
    case __hoCopyIcon:
        return "CopyIcon";
    case __hoCreateFont:
        return "CreateFont";
    case __hoCreateFontIndirect:
        return "CreateFontIndirect";
    case __hoCreateDC:
        return "CreateDC";
    case __hoGetDCEx:
        return "GetDCEx";
    case __hoGetWindowDC:
        return "GetWindowDC";
    case __hoCopyImage:
        return "CopyImage";
    case __hoCreateCursor:
        return "CreateCursor";
    case __hoRegCreateKey:
        return "RegCreateKey";
    case __hoRegCreateKeyEx:
        return "RegCreateKeyEx";
    case __hoRegOpenKey:
        return "RegOpenKey";
    case __hoRegOpenKeyEx:
        return "RegOpenKeyEx";
    case __hoRegConnectRegistry:
        return "RegConnectRegistry";
    case __hoCreateIC:
        return "CreateIC";
    case __hoCreateSolidBrush:
        return "CreateSolidBrush";
    case __hoCreateHatchBrush:
        return "CreateHatchBrush";
    case __hoCreatePatternBrush:
        return "CreatePatternBrush";
    case __hoCreateDIBPatternBrush:
        return "CreateDIBPatternBrush";
    case __hoCreateDIBPatternBrushPt:
        return "CreateDIBPatternBrushPt";
    case __hoCreateDIBitmap:
        return "CreateDIBitmap";
    case __hoCreateDIBSection:
        return "CreateDIBSection";
    case __hoCreateDiscardableBitmap:
        return "CreateDiscardableBitmap";
    case __hoCreateMappedBitmap:
        return "CreateMappedBitmap";
    case __hoLoadBitmap:
        return "LoadBitmap";
    case __hoCreatePenIndirect:
        return "CreatePenIndirect";
    case __hoCreateRectRgnIndirect:
        return "CreateRectRgnIndirect";
    case __hoCreateEllipticRgn:
        return "CreateEllipticRgn";
    case __hoCreateEllipticRgnIndirect:
        return "CreateEllipticRgnIndirect";
    case __hoCreatePolyPolygonRgn:
        return "CreatePolyPolygonRgn";
    case __hoCreatePolygonRgn:
        return "CreatePolygonRgn";
    case __hoCreateRoundRectRgn:
        return "CreateRoundRectRgn";
    case __hoCreatePalette:
        return "CreatePalette";
    case __hoCreateHalftonePalette:
        return "CreateHalftonePalette";
    case __hoCreateThread:
        return "CreateThread";
    case __hoCreateRemoteThread:
        return "CreateRemoteThread";
    case __hoCreateProcess:
        return "CreateProcess";
    case __hoOpenProcess:
        return "OpenProcess";
    case __hoCreateMutex:
        return "CreateMutex";
    case __hoOpenMutex:
        return "OpenMutex";
    case __hoCreateEvent:
        return "CreateEvent";
    case __hoOpenEvent:
        return "OpenEvent";
    case __hoCreateSemaphore:
        return "CreateSemaphore";
    case __hoOpenSemaphore:
        return "OpenSemaphore";
    case __hoCreateWaitableTimer:
        return "CreateWaitableTimer";
    case __hoOpenWaitableTimer:
        return "OpenWaitableTimer";
    case __hoCreateFileMapping:
        return "CreateFileMapping";
    case __hoOpenFileMapping:
        return "OpenFileMapping";
    case __hoCreateMailslot:
        return "CreateMailslot";
    case __hoCreatePipe:
        return "CreatePipe";
    case __hoCreateNamedPipe:
        return "CreateNamedPipe";
    case __hoCreateConsoleScreenBuffer:
        return "CreateConsoleScreenBuffer";
    case __hoDuplicateHandle:
        return "DuplicateHandle";
    case __hoMapViewOfFile:
        return "MapViewOfFile";
    case __hoMapViewOfFileEx:
        return "MapViewOfFileEx";
    case __hoCreateAcceleratorTable:
        return "CreateAcceleratorTable";
    case __hoLoadAccelerators:
        return "LoadAccelerators";
    case __hoEnterCriticalSection:
        return "EnterCriticalSection";
    case __hoInitializeCriticalSection:
        return "InitializeCriticalSection";
    case __hoTryEnterCriticalSection:
        return "TryEnterCriticalSection";
    case __hoFindFirstFile:
        return "FindFirstFile";
    case __hoCreateFiber:
        return "CreateFiber";
    case __hoTlsAlloc:
        return "TlsAlloc";
    case __hoLoadLibrary:
        return "LoadLibrary";
    case __hoLoadLibraryEx:
        return "LoadLibraryEx";
    case __hoGlobalAlloc:
        return "GlobalAlloc";
    case __hoGlobalReAlloc:
        return "GlobalReAlloc";
    case __hoGlobalLock:
        return "GlobalLock";
    case __hoFindFirstChangeNotification:
        return "FindFirstChangeNotification";
    case __hoGetEnvironmentStrings:
        return "GetEnvironmentStrings";
    case __hoLocalAlloc:
        return "LocalAlloc";
    case __hoLocalReAlloc:
        return "LocalReAlloc";
    case __hoLocalLock:
        return "LocalLock";
    case __hoLoadImage:
        return "LoadImage";
    case __hoLoadIcon:
        return "LoadIcon";
    case __ho_lcreat:
        return "_lcreat";
    case __hoOpenFile:
        return "OpenFile";
    case __ho_lopen:
        return "_lopen";
    case __ho_beginthreadex:
        return "_beginthreadex";
    case __hoRegisterDragDrop:
        return "RegisterDragDrop";
    case __hoGetStockObject:
        return "GetStockObject";
    case __hoBeginDeferWindowPos:
        return "BeginDeferWindowPos";
    case __hoDeferWindowPos:
        return "DeferWindowPos";
    case __hoOpenThreadToken:
        return "OpenThreadToken";
    case __hoOpenProcessToken:
        return "OpenProcessToken";
    }
    return "Unknown";
}

//*****************************************************************************
//
// C_HandlesDataArray
//

#define __HANDLES_ARRAY_DELTA 100

C_HandlesDataArray::C_HandlesDataArray()
{
    Available = Count = 0;
    Data = (C__HandlesData*)malloc(__HANDLES_ARRAY_DELTA * sizeof(C__HandlesData));
    if (Data == NULL)
    {
        MESSAGE_E(NULL, __HandlesMessageLowMemory, MB_OK | MB_SETFOREGROUND);
        return;
    }
    Available = __HANDLES_ARRAY_DELTA;
}

C_HandlesDataArray::~C_HandlesDataArray()
{
    if (Data != NULL)
    {
        free(Data);
        Data = NULL;
        Count = 0;
        Available = 0;
    }
}

int C_HandlesDataArray::Add(const C__HandlesData& member)
{
    if (Count == Available)
        EnlargeArray();
    Count++;
    operator[](Count - 1) = member;
    return Count - 1;
}

void C_HandlesDataArray::Delete(int index)
{
    Move(0, index + 1, Count - index - 1);
    Count--;
}

void C_HandlesDataArray::EnlargeArray()
{
    C__HandlesData* New = (C__HandlesData*)realloc(Data, (Available +
                                                          __HANDLES_ARRAY_DELTA) *
                                                             sizeof(C__HandlesData));
    if (New == NULL)
        TRACE_E(__HandlesMessageLowMemory);
    else
    {
        Data = New;
        Available += __HANDLES_ARRAY_DELTA;
    }
}

void C_HandlesDataArray::Move(int direction, int first, int count)
{
    if (count == 0)
    {
        if (direction == 1 && Available == Count)
            EnlargeArray();
        return;
    }
    if (direction == 1) // down
    {
        if (Available == Count)
            EnlargeArray();
        memmove(Data + first + 1, Data + first, count * sizeof(C__HandlesData));
    }
    else // Up
        memmove(Data + first - 1, Data + first, count * sizeof(C__HandlesData));
}

//*****************************************************************************
//
// C__Handles
//

C__Handles::C__Handles()
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    ::InitializeCriticalSection(&CriticalSection);
#else  // MULTITHREADED_HANDLES_ENABLE
    __HandlesMainThreadID = ::GetCurrentThreadId(); // jediny povoleny thread
#endif // MULTITHREADED_HANDLES_ENABLE
}

C__Handles::~C__Handles()
{
    // vyrazeni handlu, ktere se uvolnuji automaticky
    for (int i = Handles.Count - 1; i >= 0; i--)
    {
        if (Handles[i].Handle.Origin == __hoLoadAccelerators)
            Handles.Delete(i);
        else if (Handles[i].Handle.Origin == __hoLoadIcon)
            Handles.Delete(i);
        else if (Handles[i].Handle.Origin == __hoGetStockObject)
            Handles.Delete(i);
    }
    // kontrola + vypis zbylych
    if (Handles.Count != 0)
    {
        // musel jsem nahradit nize polozeny kod vyuzivajici MESSAGE_E, protoze pri volani
        // tohoto destruktoru jsou v ALTAPDB jiz zdestruovane facets streamu a pri poslani intu
        // nebo handlu do streamu to proste spadne (dela jen VC2010 a VC2012, ve VC2008
        // to jeste slape); v Salamanderovi to nezlobi, asi kvuli RTL v DLLku (v ALTAPDB je
        // static), ci co, po tom jsem uz dal nepatral, lepsi reseni by bylo zaridit
        // destrukci facets az po tomto modulu, ale to bohuzel neumim (jen tez na "lib" urovni)
        char msgBuf[1000];
        sprintf_s(msgBuf,
#ifdef MESSAGES_DEBUG
                  __FILE__ " %d:\n\n"
#endif MESSAGES_DEBUG
                           "Some monitored handles remained opened.\n%s%d"
                           "\nDo you want to list opened handles to Trace Server (ensure it is running) ?",
#ifdef MESSAGES_DEBUG
                  __LINE__,
#endif MESSAGES_DEBUG
                  __HandlesMessageNumberOpened, Handles.Count);
        HWND parent = __MessagesParent;
        if (!IsWindow(parent))
            parent = NULL;
        if (MessageBoxA(parent, msgBuf, __MessagesTitle,
                        MB_ICONEXCLAMATION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
        /*
    if (MESSAGE_E(NULL, "Some monitored handles remained opened.\n" <<
                        __HandlesMessageNumberOpened << Handles.Count <<
                        "\nDo you want to list opened handles to Trace Server (ensure it is running) ?",
                  MB_YESNO | MB_SETFOREGROUND) == IDYES)
*/
        {
            ConnectToTraceServer(); // v pripade, ze nebyl nahozeny server
            TRACE_I("List of opened handles:");
            for (int i = 0; i < Handles.Count; i++)
            {
                // saskarna pres msgBuf kvuli padackam v ALTAPDB, podrobnosti viz komentar vyse
                sprintf_s(msgBuf, "%p", Handles[i].Handle.Handle);
                TRACE_MI(Handles[i].File, Handles[i].Line,
                         __GetHandlesTypeName(Handles[i].Handle.Type) << " - " << __GetHandlesOrigin(Handles[i].Handle.Origin) << " - " << msgBuf);
            }
        }
        else
        {
            ConnectToTraceServer();
            // saskarna pres msgBuf kvuli padackam v ALTAPDB, podrobnosti viz komentar vyse
            sprintf_s(msgBuf, "%d", Handles.Count);
            TRACE_I(__HandlesMessageNumberOpened << msgBuf);
        }
    }
    else
        TRACE_I("All monitored handles were closed.");
#ifdef MULTITHREADED_HANDLES_ENABLE
    ::DeleteCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
}

C__Handles&
C__Handles::SetInfo(const char* file, int line, C__HandlesOutputType outputType)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    if (CriticalSection.RecursionCount > 1)
    {
        DebugBreak(); // rekurzivni volani handles !!! zase nejaka maskovana message-loopa - viz call-stack
    }
#endif // MULTITHREADED_HANDLES_ENABLE
    OutputType = outputType;
    TemporaryHandle.File = file;
    TemporaryHandle.Line = line;
    return *this;
}

void C__Handles::InformationsToTrace(BOOL detail)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
    if (Handles.Count > 0)
    {
        if (detail)
        {
            TRACE_I("List of currently monitored handles:");
            for (int i = 0; i < Handles.Count; i++)
            {
                TRACE_MI(Handles[i].File, Handles[i].Line,
                         __GetHandlesTypeName(Handles[i].Handle.Type) << " - " << __GetHandlesOrigin(Handles[i].Handle.Origin) << " - " << Handles[i].Handle.Handle);
            }
        }
        else
        {
            TRACE_I("Numbers of currently monitored handles:");
            int count[__htCount];
            memset(count, 0, sizeof(count));
            int i = 0;
            for (i = 0; i < Handles.Count; i++)
                count[Handles[i].Handle.Type]++;
            for (i = 0; i < __htCount; i++)
            {
                if (count[i] > 0)
                    TRACE_I(__GetHandlesTypeName((C__HandlesType)i) << ": " << count[i]);
            }
        }
    }
    else
        TRACE_I("There are none currently monitored handles.");
#ifdef MULTITHREADED_HANDLES_ENABLE
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
}

void C__Handles::AddHandle(C__HandlesHandle handle)
{
    TemporaryHandle.Handle = handle;
    Handles.Add(TemporaryHandle);
}

BOOL __Handles_IsCompWithCloseHandle(C__HandlesType type)
{
    return type == __htHandle_comp_with_CloseHandle ||
           type == __htFile ||
           type == __htThread ||
           type == __htProcess ||
           type == __htEvent ||
           type == __htMutex ||
           type == __htSemaphore ||
           type == __htWaitableTimer ||
           type == __htFileMapping ||
           type == __htMailslot ||
           type == __htReadPipe ||
           type == __htWritePipe ||
           type == __htServerNamedPipe ||
           type == __htConsoleScreenBuffer ||
           type == __htThreadToken ||
           type == __htProcessToken;
}

BOOL __Handles_IsCompWithDeleteObject(C__HandlesType type)
{
    return type == __htHandle_comp_with_DeleteObject ||
           type == __htPen ||
           type == __htBrush ||
           type == __htFont ||
           type == __htBitmap ||
           type == __htRegion ||
           type == __htPalette;
}

BOOL C__Handles::DeleteHandle(C__HandlesType& type, HANDLE handle,
                              C__HandlesOrigin* origin, C__HandlesType expType)
{
    int found = -1;
    int foundTypeOK = -1;
    for (int i = Handles.Count - 1; i >= 0; i--)
    {
        if (Handles[i].Handle.Handle == handle)
        {
            if (found == -1)
                found = i;
            type = Handles[i].Handle.Type;
            if (expType == type ||
                expType == __htHandle_comp_with_CloseHandle && __Handles_IsCompWithCloseHandle(type) ||
                expType == __htHandle_comp_with_DeleteObject && __Handles_IsCompWithDeleteObject(type))
            {
                C__HandlesOrigin org = Handles[i].Handle.Origin;
                if (org != __hoLoadAccelerators && org != __hoLoadIcon &&
                    org != __hoGetStockObject) // nejde o handle, ktery nemusi byt uvolneny (prioritne uvolnujeme handly, ktere se musi uvolnit)
                {
                    if (origin != NULL)
                        *origin = org;
                    Handles.Delete(i);
                    return TRUE;
                }
                else
                {
                    if (foundTypeOK == -1)
                        foundTypeOK = i;
                }
            }
        }
    }
    if (foundTypeOK != -1) // nalezen jen handle, ktery nemusi byt uvolneny
    {
        type = Handles[foundTypeOK].Handle.Type;
        if (origin != NULL)
            *origin = Handles[foundTypeOK].Handle.Origin;
        Handles.Delete(foundTypeOK);
        return TRUE;
    }
    if (found != -1) // nalezen jen handle se shodnym cislem
    {
#if defined(_DEBUG) || defined(__HANDLES_DEBUG)
        C__HandlesData* data = &(Handles[found]);
        TRACE_I("Returned unexpected handle: " << data->File << "::" << data->Line << " - " << __GetHandlesTypeName(data->Handle.Type) << ", " << __GetHandlesOrigin(data->Handle.Origin) << ", " << data->Handle.Handle);
#endif
        type = Handles[found].Handle.Type;
        if (origin != NULL)
            *origin = Handles[found].Handle.Origin;
        Handles.Delete(found);
        return TRUE;
    }
    return FALSE;
}

//*****************************************************************************
//
// Auxiliary functions:
//

void C__Handles::CheckCreate(BOOL success, C__HandlesType type,
                             C__HandlesOrigin origin, const HANDLE handle, DWORD error,
                             BOOL synchronize, const TCHAR* params, const char* paramsA,
                             const WCHAR* paramsW)
{
    DWORD old_error = GetLastError();
    WCHAR fileNameW[MAX_PATH];
#ifndef MULTITHREADED_HANDLES_ENABLE
    if (__HandlesMainThreadID != ::GetCurrentThreadId())
    {
        MESSAGE_E(NULL, __HandlesMessageNotMultiThreaded, MB_OK | MB_SETFOREGROUND);
        SetLastError(old_error);
        return;
    }
#endif // MULTITHREADED_HANDLES_ENABLE

#ifdef _UNICODE
    if (params != NULL)
        paramsW = params;
#else  // _UNICODE
    if (params != NULL)
        paramsA = params;
#endif // _UNICODE
    params = NULL;

    if (success)
    {
        AddHandle(C__HandlesHandle(type, origin, handle));
    }
    else
    {
        if (error != ERROR_SUCCESS)
        {
            switch (OutputType)
            {
            case __otMessages:
            {
                if (paramsW != NULL)
                {
                    _snwprintf_s(fileNameW, _TRUNCATE, L"%S", TemporaryHandle.File);
                    MESSAGE_TEW(__HandlesMessageCallFromW << fileNameW << __HandlesMessageSpaceW << TemporaryHandle.Line << __HandlesMessageLineEndW << spfW(__HandlesMessageReturnErrorMessageParamsW, __GetHandlesOrigin(origin), errW(error), paramsW), MB_OK);
                }
                else
                {
                    MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(paramsA != NULL ? __HandlesMessageReturnErrorMessageParams : __HandlesMessageReturnErrorMessage, __GetHandlesOrigin(origin), err(error), paramsA), MB_OK);
                }
                break;
            }

            case __otQuiet:
                break;
            }
        }
        else
        {
            switch (OutputType)
            {
            case __otMessages:
            {
                if (paramsW != NULL)
                {
                    _snwprintf_s(fileNameW, _TRUNCATE, L"%S", TemporaryHandle.File);
                    MESSAGE_TEW(__HandlesMessageCallFromW << fileNameW << __HandlesMessageSpaceW << TemporaryHandle.Line << __HandlesMessageLineEndW << spfW(__HandlesMessageReturnErrorParamsW, __GetHandlesOrigin(origin), paramsW), MB_OK);
                }
                else
                {
                    MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(paramsA != NULL ? __HandlesMessageReturnErrorParams : __HandlesMessageReturnError, __GetHandlesOrigin(origin), paramsA), MB_OK);
                }
                break;
            }

            case __otQuiet:
                break;
            }
        }
    }
    if (synchronize)
    {
#ifdef MULTITHREADED_HANDLES_ENABLE
        ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
    }
    SetLastError(old_error);
}

void C__Handles::CheckClose(BOOL success, const HANDLE handle, C__HandlesType expType,
                            const char* function, DWORD error, BOOL synchronize)
{
    DWORD old_error = GetLastError();
#ifndef MULTITHREADED_HANDLES_ENABLE
    if (__HandlesMainThreadID != ::GetCurrentThreadId())
    {
        MESSAGE_E(NULL, __HandlesMessageNotMultiThreaded, MB_OK | MB_SETFOREGROUND);
        SetLastError(old_error);
        return;
    }
#endif // MULTITHREADED_HANDLES_ENABLE

    if (success)
    {
        C__HandlesType type;
        if (DeleteHandle(type, handle, NULL, expType))
        {
            if (type != expType &&
                (expType != __htHandle_comp_with_CloseHandle || !__Handles_IsCompWithCloseHandle(type)) &&
                (expType != __htHandle_comp_with_DeleteObject || !__Handles_IsCompWithDeleteObject(type)))
            {
                MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageBadType, function, __GetHandlesTypeName(type), __GetHandlesTypeName(expType), handle), MB_OK);
            }
        }
        else
        {
            MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageNotFound, function, handle), MB_OK);
        }
    }
    else
    {
        if (error != ERROR_SUCCESS)
        {
            switch (OutputType)
            {
            case __otMessages:
            {
                MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageReturnErrorMessage, function, err(error)), MB_OK);
                break;
            }

            case __otQuiet:
                break;
            }
        }
        else
        {
            switch (OutputType)
            {
            case __otMessages:
            {
                MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageReturnError, function), MB_OK);
                break;
            }

            case __otQuiet:
                break;
            }
        }
    }
    if (synchronize)
    {
#ifdef MULTITHREADED_HANDLES_ENABLE
        ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
    }
    SetLastError(old_error);
}

//*****************************************************************************
//
// Monitored functions:
//

HANDLE
C__Handles::CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                        DWORD dwShareMode,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                        DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                        HANDLE hTemplateFile)
{
    HANDLE ret = ::CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                               lpSecurityAttributes, dwCreationDisposition,
                               dwFlagsAndAttributes, hTemplateFile);
    char paramsBuf[MAX_PATH + 200];
    const char* params = NULL;
    if (ret == INVALID_HANDLE_VALUE) // parameters to buffer only when error occurs (can be displayed)
    {
        DWORD err = GetLastError();
        _snprintf_s(paramsBuf, _TRUNCATE,
                    "%s,\ndwDesiredAccess=0x%X,\ndwShareMode=0x%X,\ndwCreationDisposition=0x%X,\ndwFlagsAndAttributes=0x%X",
                    lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes);
        params = paramsBuf;
        SetLastError(err);
    }
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htFile, __hoCreateFile, ret, GetLastError(), TRUE, NULL, params);
    return ret;
}

HANDLE
C__Handles::CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess,
                        DWORD dwShareMode,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                        DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                        HANDLE hTemplateFile)
{
    HANDLE ret = ::CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                               lpSecurityAttributes, dwCreationDisposition,
                               dwFlagsAndAttributes, hTemplateFile);
    WCHAR paramsBuf[800 + 200]; // na jmena souboru omezime na 800 znaku, stejne delsi nevytiskneme diky omezeni MESSAGES
    const WCHAR* params = NULL;
    if (ret == INVALID_HANDLE_VALUE) // parameters to buffer only when error occurs (can be displayed)
    {
        DWORD err = GetLastError();
        _snwprintf_s(paramsBuf, _TRUNCATE,
                     L"dwDesiredAccess=0x%X,\ndwShareMode=0x%X,\ndwCreationDisposition=0x%X,\ndwFlagsAndAttributes=0x%X,\nlpFileName=%s", // lpFileName mame schvalne az na konci, muze byt az 32k dlouhe, orizne se
                     dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes, lpFileName);
        params = paramsBuf;
        SetLastError(err);
    }
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htFile, __hoCreateFile, ret, GetLastError(), TRUE, NULL, NULL, params);
    return ret;
}

BOOL C__Handles::CloseHandle(HANDLE hObject)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    BOOL ret = ::CloseHandle(hObject);

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckClose(ret, hObject, __htHandle_comp_with_CloseHandle, "CloseHandle", GetLastError());
    return ret;
}

HBRUSH
C__Handles::CreateBrushIndirect(CONST LOGBRUSH* lplb)
{
    HBRUSH ret = ::CreateBrushIndirect(lplb);
    CheckCreate(ret != NULL, __htBrush, __hoCreateBrushIndirect, ret);
    return ret;
}

HGDIOBJ
C__Handles::GetStockObject(int fnObject)
{
    HGDIOBJ ret = ::GetStockObject(fnObject);
    CheckCreate(ret != NULL, __htHandle_comp_with_DeleteObject, __hoGetStockObject, ret);
    return ret;
}

BOOL C__Handles::DeleteObject(HGDIOBJ hObject)
{
    BOOL ret = ::DeleteObject(hObject);
    CheckClose(ret, hObject, __htHandle_comp_with_DeleteObject, "DeleteObject", GetLastError());
    return ret;
}

HDC C__Handles::GetDC(HWND hWnd)
{
    HDC ret = ::GetDC(hWnd);
    CheckCreate(ret != NULL, __htDC, __hoGetDC, ret);
    return ret;
}

int C__Handles::ReleaseDC(HWND hWnd, HDC hDC)
{
    int ret = ::ReleaseDC(hWnd, hDC);
    CheckClose(ret != 0, hDC, __htDC, "ReleaseDC");
    return ret;
}

HBITMAP
C__Handles::CreateCompatibleBitmap(HDC hdc, int nWidth, int nHeight)
{
    HBITMAP ret = ::CreateCompatibleBitmap(hdc, nWidth, nHeight);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateCompatibleBitmap, ret);
    return ret;
}

HPEN C__Handles::CreatePen(int fnPenStyle, int nWidth, COLORREF crColor)
{
    HPEN ret = ::CreatePen(fnPenStyle, nWidth, crColor);
    CheckCreate(ret != NULL, __htPen, __hoCreatePen, ret);
    return ret;
}

HDC C__Handles::BeginPaint(HWND hwnd, LPPAINTSTRUCT lpPaint)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    HDC ret = ::BeginPaint(hwnd, lpPaint); // obsahuje message-loopu

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckCreate(ret != NULL, __htDC, __hoBeginPaint, ret);
    return ret;
}

BOOL C__Handles::EndPaint(HWND hWnd, CONST PAINTSTRUCT* lpPaint)
{
    HDC dc = (lpPaint != NULL) ? lpPaint->hdc : NULL;
    BOOL ret = ::EndPaint(hWnd, lpPaint);
    CheckClose(lpPaint != NULL, dc, __htDC, "EndPaint");
    return ret;
}

HDC C__Handles::CreateCompatibleDC(HDC hdc)
{
    HDC ret = ::CreateCompatibleDC(hdc);
    CheckCreate(ret != NULL, __htDC, __hoCreateCompatibleDC, ret);
    return ret;
}

HRGN C__Handles::CreateRectRgn(int nLeftRect, int nTopRect, int nRightRect,
                               int nBottomRect)
{
    HRGN ret = ::CreateRectRgn(nLeftRect, nTopRect, nRightRect, nBottomRect);
    CheckCreate(ret != NULL, __htRegion, __hoCreateRectRgn, ret);
    return ret;
}

BOOL C__Handles::DeleteDC(HDC hdc)
{
    BOOL ret = ::DeleteDC(hdc);
    CheckClose(ret, hdc, __htDC, "DeleteDC");
    return ret;
}

HBITMAP
C__Handles::CreateBitmap(int nWidth, int nHeight, UINT cPlanes,
                         UINT cBitsPerPel, CONST VOID* lpvBits)
{
    HBITMAP ret = ::CreateBitmap(nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateBitmap, ret);
    return ret;
}

HBITMAP
C__Handles::CreateBitmapIndirect(CONST BITMAP* lpbm)
{
    HBITMAP ret = ::CreateBitmapIndirect(lpbm);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateBitmapIndirect, ret);
    return ret;
}

HDC C__Handles::CreateMetaFile(LPCTSTR lpszFile)
{
    HDC ret = ::CreateMetaFile(lpszFile);
    CheckCreate(ret != NULL, __htDC, __hoCreateMetaFile, ret);
    return ret;
}

HDC C__Handles::CreateEnhMetaFile(HDC hdcRef, LPCTSTR lpFilename,
                                  CONST RECT* lpRect, LPCTSTR lpDescription)
{
    HDC ret = ::CreateEnhMetaFile(hdcRef, lpFilename, lpRect, lpDescription);
    CheckCreate(ret != NULL, __htDC, __hoCreateEnhMetaFile, ret, ERROR_SUCCESS, TRUE, lpFilename);
    return ret;
}

HMETAFILE
C__Handles::CloseMetaFile(HDC hdc)
{
    HMETAFILE ret = ::CloseMetaFile(hdc);
    if (ret != NULL)
    {
        CheckCreate(TRUE, __htMetaFile, __hoCloseMetaFile, ret, ERROR_SUCCESS, FALSE);
        CheckClose(TRUE, hdc, __htDC, __GetHandlesOrigin(__hoCloseMetaFile));
    }
    else
    {
        CheckCreate(FALSE, __htMetaFile, __hoCloseMetaFile, ret);
    }
    return ret;
}

HENHMETAFILE
C__Handles::CloseEnhMetaFile(HDC hdc)
{
    HENHMETAFILE ret = ::CloseEnhMetaFile(hdc);
    if (ret != NULL)
    {
        CheckCreate(TRUE, __htEnhMetaFile, __hoCloseEnhMetaFile, ret, ERROR_SUCCESS,
                    FALSE);
        CheckClose(TRUE, hdc, __htDC, __GetHandlesOrigin(__hoCloseEnhMetaFile));
    }
    else
    {
        CheckCreate(FALSE, __htEnhMetaFile, __hoCloseEnhMetaFile, ret);
    }
    return ret;
}

HMETAFILE
C__Handles::CopyMetaFile(HMETAFILE hmfSrc, LPCTSTR lpszFile)
{
    HMETAFILE ret = ::CopyMetaFile(hmfSrc, lpszFile);
    CheckCreate(ret != NULL, __htMetaFile, __hoCopyMetaFile, ret);
    return ret;
}

HENHMETAFILE
C__Handles::CopyEnhMetaFile(HENHMETAFILE hemfSrc, LPCTSTR lpszFile)
{
    HENHMETAFILE ret = ::CopyEnhMetaFile(hemfSrc, lpszFile);
    CheckCreate(ret != NULL, __htEnhMetaFile, __hoCopyEnhMetaFile, ret);
    return ret;
}

HENHMETAFILE
C__Handles::GetEnhMetaFile(LPCTSTR lpszMetaFile)
{
    HENHMETAFILE ret = ::GetEnhMetaFile(lpszMetaFile);
    CheckCreate(ret != NULL, __htEnhMetaFile, __hoGetEnhMetaFile, ret);
    return ret;
}

HENHMETAFILE
C__Handles::SetWinMetaFileBits(UINT cbBuffer, CONST BYTE* lpbBuffer, HDC hdcRef,
                               CONST METAFILEPICT* lpmfp)
{
    HENHMETAFILE ret = ::SetWinMetaFileBits(cbBuffer, lpbBuffer, hdcRef, lpmfp);
    CheckCreate(ret != NULL, __htEnhMetaFile, __hoSetWinMetaFileBits, ret);
    return ret;
}

BOOL C__Handles::DeleteMetaFile(HMETAFILE hmf)
{
    BOOL ret = ::DeleteMetaFile(hmf);
    CheckClose(ret, hmf, __htMetaFile, "DeleteMetaFile");
    return ret;
}

BOOL C__Handles::DeleteEnhMetaFile(HENHMETAFILE hemf)
{
    BOOL ret = ::DeleteEnhMetaFile(hemf);
    CheckClose(ret, hemf, __htEnhMetaFile, "DeleteEnhMetaFile");
    return ret;
}

HICON
C__Handles::CreateIcon(HINSTANCE hInstance, int nWidth, int nHeight,
                       BYTE cPlanes, BYTE cBitsPixel, CONST BYTE* lpbANDbits,
                       CONST BYTE* lpbXORbits)
{
    HICON ret = ::CreateIcon(hInstance, nWidth, nHeight, cPlanes, cBitsPixel,
                             lpbANDbits, lpbXORbits);
    CheckCreate(ret != NULL, __htIcon, __hoCreateIcon, ret, GetLastError());
    return ret;
}

HICON
C__Handles::CreateIconIndirect(PICONINFO piconinfo)
{
    HICON ret = ::CreateIconIndirect(piconinfo);
    CheckCreate(ret != NULL, __htIcon, __hoCreateIconIndirect, ret, GetLastError());
    return ret;
}

HICON
C__Handles::CreateIconFromResource(PBYTE presbits, DWORD dwResSize, BOOL fIcon,
                                   DWORD dwVer)
{
    HICON ret = ::CreateIconFromResource(presbits, dwResSize, fIcon, dwVer);
    CheckCreate(ret != NULL, __htIcon, __hoCreateIconFromResource, ret, GetLastError());
    return ret;
}

HICON
C__Handles::CreateIconFromResourceEx(PBYTE pbIconBits, DWORD cbIconBits,
                                     BOOL fIcon, DWORD dwVersion, int cxDesired,
                                     int cyDesired, UINT uFlags)
{
    HICON ret = ::CreateIconFromResourceEx(pbIconBits, cbIconBits, fIcon, dwVersion,
                                           cxDesired, cyDesired, uFlags);
    CheckCreate(ret != NULL, __htIcon, __hoCreateIconFromResourceEx, ret, GetLastError());
    return ret;
}

HICON
C__Handles::CopyIcon(HICON hIcon)
{
    HICON ret = ::CopyIcon(hIcon);
    CheckCreate(ret != NULL, __htIcon, __hoCopyIcon, ret, GetLastError());
    return ret;
}

BOOL C__Handles::DestroyIcon(HICON hIcon)
{
    BOOL ret = ::DestroyIcon(hIcon);
    CheckClose(ret, hIcon, __htIcon, "DestroyIcon", GetLastError());
    return ret;
}

HFONT
C__Handles::CreateFont(int nHeight, int nWidth, int nEscapement, int nOrientation,
                       int fnWeight, DWORD fdwItalic, DWORD fdwUnderline,
                       DWORD fdwStrikeOut, DWORD fdwCharSet,
                       DWORD fdwOutputPrecision, DWORD fdwClipPrecision,
                       DWORD fdwQuality, DWORD fdwPitchAndFamily, LPCTSTR lpszFace)
{
    HFONT ret = ::CreateFont(nHeight, nWidth, nEscapement, nOrientation, fnWeight,
                             fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet,
                             fdwOutputPrecision, fdwClipPrecision, fdwQuality,
                             fdwPitchAndFamily, lpszFace);
    CheckCreate(ret != NULL, __htFont, __hoCreateFont, ret, GetLastError());
    return ret;
}

HFONT
C__Handles::CreateFontIndirectA(CONST LOGFONTA* lplf)
{
    HFONT ret = ::CreateFontIndirectA(lplf);
    CheckCreate(ret != NULL, __htFont, __hoCreateFontIndirect, ret);
    return ret;
}

HFONT
C__Handles::CreateFontIndirectW(CONST LOGFONTW* lplf)
{
    HFONT ret = ::CreateFontIndirectW(lplf);
    CheckCreate(ret != NULL, __htFont, __hoCreateFontIndirect, ret);
    return ret;
}

HDC C__Handles::CreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput,
                         CONST DEVMODE* lpInitData)
{
    HDC ret = ::CreateDC(lpszDriver, lpszDevice, lpszOutput, lpInitData);
    CheckCreate(ret != NULL, __htDC, __hoCreateDC, ret);
    return ret;
}

HDC C__Handles::GetDCEx(HWND hWnd, HRGN hrgnClip, DWORD flags)
{
    HDC ret = ::GetDCEx(hWnd, hrgnClip, flags);
    CheckCreate(ret != NULL, __htDC, __hoGetDCEx, ret);
    return ret;
}

HDC C__Handles::GetWindowDC(HWND hWnd)
{
    HDC ret = ::GetWindowDC(hWnd);
    CheckCreate(ret != NULL, __htDC, __hoGetWindowDC, ret);
    return ret;
}

HANDLE
C__Handles::CopyImage(HANDLE hImage, UINT uType, int cxDesired, int cyDesired,
                      UINT fuFlags)
{
    HANDLE ret = ::CopyImage(hImage, uType, cxDesired, cyDesired, fuFlags);
    C__HandlesType type;
    switch (uType)
    {
    case IMAGE_BITMAP:
        type = __htBitmap;
        break;
    case IMAGE_CURSOR:
        type = __htCursor;
        break;
    default:
        type = __htIcon;
        break;
    }
    CheckCreate(ret != NULL, type, __hoCopyImage, ret, GetLastError());
    return ret;
}

HCURSOR
C__Handles::CreateCursor(HINSTANCE hInst, int xHotSpot, int yHotSpot,
                         int nWidth, int nHeight, CONST VOID* pvANDPlane,
                         CONST VOID* pvXORPlane)
{
    HCURSOR ret = ::CreateCursor(hInst, xHotSpot, yHotSpot, nWidth, nHeight,
                                 pvANDPlane, pvXORPlane);
    CheckCreate(ret != NULL, __htCursor, __hoCreateCursor, ret, GetLastError());
    return ret;
}

BOOL C__Handles::DestroyCursor(HCURSOR hCursor)
{
    BOOL ret = ::DestroyCursor(hCursor);
    CheckClose(ret, hCursor, __htCursor, "DestroyCursor", GetLastError());
    return ret;
}

LONG C__Handles::RegCreateKey(HKEY hKey, LPCTSTR lpSubKey, PHKEY phkResult)
{
    LONG ret = ::RegCreateKey(hKey, lpSubKey, phkResult);
    CheckCreate(ret == ERROR_SUCCESS, __htKey, __hoRegCreateKey,
                (phkResult != NULL) ? *phkResult : NULL, ret);
    return ret;
}

LONG C__Handles::RegCreateKeyEx(HKEY hKey, LPCTSTR lpSubKey, DWORD Reserved,
                                LPTSTR lpClass, DWORD dwOptions, REGSAM samDesired,
                                LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                PHKEY phkResult, LPDWORD lpdwDisposition)
{
    LONG ret = ::RegCreateKeyEx(hKey, lpSubKey, Reserved, lpClass, dwOptions,
                                samDesired, lpSecurityAttributes, phkResult,
                                lpdwDisposition);
    CheckCreate(ret == ERROR_SUCCESS, __htKey, __hoRegCreateKeyEx,
                (phkResult != NULL) ? *phkResult : NULL, ret);
    return ret;
}

LONG C__Handles::RegOpenKey(HKEY hKey, LPCTSTR lpSubKey, PHKEY phkResult)
{
    LONG ret = ::RegOpenKey(hKey, lpSubKey, phkResult);
    CheckCreate(ret == ERROR_SUCCESS, __htKey, __hoRegOpenKey,
                (phkResult != NULL) ? *phkResult : NULL, ret);
    return ret;
}

LONG C__Handles::RegOpenKeyEx(HKEY hKey, LPCTSTR lpSubKey, DWORD ulOptions,
                              REGSAM samDesired, PHKEY phkResult)
{
    LONG ret = ::RegOpenKeyEx(hKey, lpSubKey, ulOptions, samDesired, phkResult);
    CheckCreate(ret == ERROR_SUCCESS, __htKey, __hoRegOpenKeyEx,
                (phkResult != NULL) ? *phkResult : NULL, ret);
    return ret;
}

LONG C__Handles::RegConnectRegistry(LPTSTR lpMachineName, HKEY hKey, PHKEY phkResult)
{
    LONG ret = ::RegConnectRegistry(lpMachineName, hKey, phkResult);
    CheckCreate(ret == ERROR_SUCCESS, __htKey, __hoRegConnectRegistry,
                (phkResult != NULL) ? *phkResult : NULL, ret);
    return ret;
}

LONG C__Handles::RegCloseKey(HKEY hKey)
{
    LONG ret = ::RegCloseKey(hKey);
    CheckClose(ret == ERROR_SUCCESS, hKey, __htKey, "RegCloseKey", ret);
    return ret;
}

HDC C__Handles::CreateIC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput,
                         CONST DEVMODE* lpdvmInit)
{
    HDC ret = ::CreateIC(lpszDriver, lpszDevice, lpszOutput, lpdvmInit);
    CheckCreate(ret != NULL, __htDC, __hoCreateIC, ret);
    return ret;
}

HBRUSH
C__Handles::CreateSolidBrush(COLORREF crColor)
{
    HBRUSH ret = ::CreateSolidBrush(crColor);
    CheckCreate(ret != NULL, __htBrush, __hoCreateSolidBrush, ret);
    return ret;
}

HBRUSH
C__Handles::CreateHatchBrush(int fnStyle, COLORREF clrref)
{
    HBRUSH ret = ::CreateHatchBrush(fnStyle, clrref);
    CheckCreate(ret != NULL, __htBrush, __hoCreateHatchBrush, ret);
    return ret;
}

HBRUSH
C__Handles::CreatePatternBrush(HBITMAP hbmp)
{
    HBRUSH ret = ::CreatePatternBrush(hbmp);
    CheckCreate(ret != NULL, __htBrush, __hoCreatePatternBrush, ret);
    return ret;
}

HBRUSH
C__Handles::CreateDIBPatternBrush(HGLOBAL hglbDIBPacked, UINT fuColorSpec)
{
    HBRUSH ret = ::CreateDIBPatternBrush(hglbDIBPacked, fuColorSpec);
    CheckCreate(ret != NULL, __htBrush, __hoCreateDIBPatternBrush, ret);
    return ret;
}

HBRUSH
C__Handles::CreateDIBPatternBrushPt(CONST VOID* lpPackedDIB, UINT iUsage)
{
    HBRUSH ret = ::CreateDIBPatternBrushPt(lpPackedDIB, iUsage);
    CheckCreate(ret != NULL, __htBrush, __hoCreateDIBPatternBrushPt, ret);
    return ret;
}

HBITMAP
C__Handles::CreateDIBitmap(HDC hdc, CONST BITMAPINFOHEADER* lpbmih,
                           DWORD fdwInit, CONST VOID* lpbInit,
                           CONST BITMAPINFO* lpbmi, UINT fuUsage)
{
    HBITMAP ret = ::CreateDIBitmap(hdc, lpbmih, fdwInit, lpbInit, lpbmi, fuUsage);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateDIBitmap, ret);
    return ret;
}

HBITMAP
C__Handles::CreateDIBSection(HDC hdc, CONST BITMAPINFO* pbmi, UINT iUsage,
                             VOID** ppvBits, HANDLE hSection, DWORD dwOffset)
{
    HBITMAP ret = ::CreateDIBSection(hdc, pbmi, iUsage, ppvBits, hSection,
                                     dwOffset);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateDIBSection, ret);
    return ret;
}

HBITMAP
C__Handles::CreateDiscardableBitmap(HDC hdc, int nWidth, int nHeight)
{
    HBITMAP ret = ::CreateDiscardableBitmap(hdc, nWidth, nHeight);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateDiscardableBitmap, ret);
    return ret;
}

HBITMAP
C__Handles::CreateMappedBitmap(HINSTANCE hInstance, int idBitmap, UINT wFlags,
                               LPCOLORMAP lpColorMap, int iNumMaps)
{
    HBITMAP ret = ::CreateMappedBitmap(hInstance, idBitmap, wFlags,
                                       lpColorMap, iNumMaps);
    CheckCreate(ret != NULL, __htBitmap, __hoCreateMappedBitmap, ret, GetLastError());
    return ret;
}

HBITMAP
C__Handles::LoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName)
{
    HBITMAP ret = ::LoadBitmap(hInstance, lpBitmapName);
    CheckCreate(ret != NULL, __htBitmap, __hoLoadBitmap, ret);
    return ret;
}

HPEN C__Handles::CreatePenIndirect(CONST LOGPEN* lplgpn)
{
    HPEN ret = ::CreatePenIndirect(lplgpn);
    CheckCreate(ret != NULL, __htPen, __hoCreatePenIndirect, ret);
    return ret;
}

HRGN C__Handles::CreateRectRgnIndirect(CONST RECT* lprc)
{
    HRGN ret = ::CreateRectRgnIndirect(lprc);
    CheckCreate(ret != NULL, __htRegion, __hoCreateRectRgnIndirect, ret);
    return ret;
}

HRGN C__Handles::CreateEllipticRgn(int nLeftRect, int nTopRect, int nRightRect,
                                   int nBottomRect)
{
    HRGN ret = ::CreateEllipticRgn(nLeftRect, nTopRect, nRightRect, nBottomRect);
    CheckCreate(ret != NULL, __htRegion, __hoCreateEllipticRgn, ret);
    return ret;
}

HRGN C__Handles::CreateEllipticRgnIndirect(CONST RECT* lprc)
{
    HRGN ret = ::CreateEllipticRgnIndirect(lprc);
    CheckCreate(ret != NULL, __htRegion, __hoCreateEllipticRgnIndirect, ret);
    return ret;
}

HRGN C__Handles::CreatePolyPolygonRgn(CONST POINT* lppt, CONST INT* lpPolyCounts,
                                      int nCount, int fnPolyFillMode)
{
    HRGN ret = ::CreatePolyPolygonRgn(lppt, lpPolyCounts, nCount, fnPolyFillMode);
    CheckCreate(ret != NULL, __htRegion, __hoCreatePolyPolygonRgn, ret);
    return ret;
}

HRGN C__Handles::CreatePolygonRgn(CONST POINT* lppt, int cPoints, int fnPolyFillMode)
{
    HRGN ret = ::CreatePolygonRgn(lppt, cPoints, fnPolyFillMode);
    CheckCreate(ret != NULL, __htRegion, __hoCreatePolygonRgn, ret);
    return ret;
}

HRGN C__Handles::CreateRoundRectRgn(int nLeftRect, int nTopRect, int nRightRect,
                                    int nBottomRect, int nWidthEllipse, int nHeightEllipse)
{
    HRGN ret = ::CreateRoundRectRgn(nLeftRect, nTopRect, nRightRect, nBottomRect,
                                    nWidthEllipse, nHeightEllipse);
    CheckCreate(ret != NULL, __htRegion, __hoCreateRoundRectRgn, ret);
    return ret;
}

HPALETTE
C__Handles::CreatePalette(CONST LOGPALETTE* lplgpl)
{
    HPALETTE ret = ::CreatePalette(lplgpl);
    CheckCreate(ret != NULL, __htPalette, __hoCreatePalette, ret, GetLastError());
    return ret;
}

HPALETTE
C__Handles::CreateHalftonePalette(HDC hdc)
{
    HPALETTE ret = ::CreateHalftonePalette(hdc);
    CheckCreate(ret != NULL, __htPalette, __hoCreateHalftonePalette, ret,
                GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                         DWORD dwStackSize,
                         LPTHREAD_START_ROUTINE lpStartAddress,
                         LPVOID lpParameter, DWORD dwCreationFlags,
                         LPDWORD lpThreadId)
{
    HANDLE ret = ::CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress,
                                lpParameter, dwCreationFlags, lpThreadId);
    CheckCreate(ret != NULL, __htThread, __hoCreateThread, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateRemoteThread(HANDLE hProcess,
                               LPSECURITY_ATTRIBUTES lpThreadAttributes,
                               DWORD dwStackSize,
                               LPTHREAD_START_ROUTINE lpStartAddress,
                               LPVOID lpParameter, DWORD dwCreationFlags,
                               LPDWORD lpThreadId)
{
    HANDLE ret = ::CreateRemoteThread(hProcess, lpThreadAttributes, dwStackSize,
                                      lpStartAddress, lpParameter, dwCreationFlags,
                                      lpThreadId);
    CheckCreate(ret != NULL, __htThread, __hoCreateRemoteThread, ret, GetLastError());
    return ret;
}

BOOL C__Handles::CreateProcess(LPCTSTR lpApplicationName, LPTSTR lpCommandLine,
                               LPSECURITY_ATTRIBUTES lpProcessAttributes,
                               LPSECURITY_ATTRIBUTES lpThreadAttributes,
                               BOOL bInheritHandles, DWORD dwCreationFlags,
                               LPVOID lpEnvironment, LPCTSTR lpCurrentDirectory,
                               LPSTARTUPINFO lpStartupInfo,
                               LPPROCESS_INFORMATION lpProcessInformation)
{
    BOOL ret = ::CreateProcess(lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes,
                               bInheritHandles, dwCreationFlags, lpEnvironment,
                               lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
    if (ret)
    {
        if (lpProcessInformation != NULL)
        {
            if (lpProcessInformation->hProcess != NULL)
                CheckCreate(TRUE, __htProcess, __hoCreateProcess,
                            lpProcessInformation->hProcess, ERROR_SUCCESS, FALSE);
            if (lpProcessInformation->hThread != NULL)
                CheckCreate(TRUE, __htThread, __hoCreateProcess,
                            lpProcessInformation->hThread);
#ifdef MULTITHREADED_HANDLES_ENABLE
            else
            {
                ::LeaveCriticalSection(&CriticalSection);
            }
#endif // MULTITHREADED_HANDLES_ENABLE
        }
#ifdef MULTITHREADED_HANDLES_ENABLE
        else
        {
            ::LeaveCriticalSection(&CriticalSection);
        }
#endif // MULTITHREADED_HANDLES_ENABLE
    }
    else
    {
        CheckCreate(FALSE, __htProcess, __hoCreateProcess, NULL, GetLastError());
    }
    return ret;
}

HANDLE
C__Handles::OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle,
                        DWORD dwProcessId)
{
    HANDLE ret = ::OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
    CheckCreate(ret != NULL, __htProcess, __hoOpenProcess, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateMutex(LPSECURITY_ATTRIBUTES lpMutexAttributes,
                        BOOL bInitialOwner, LPCTSTR lpName)
{
    HANDLE ret = ::CreateMutex(lpMutexAttributes, bInitialOwner, lpName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        DWORD err = GetLastError();
        switch (OutputType)
        {
        case __otMessages:
        {
            MESSAGE_TI(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageAlreadyExists, __GetHandlesOrigin(__hoCreateMutex), __GetHandlesTypeName(__htMutex), lpName), MB_OK);
            break;
        }

        case __otQuiet:
            break;
        }
        SetLastError(err);
    }
    CheckCreate(ret != NULL, __htMutex, __hoCreateMutex, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::OpenMutex(DWORD dwDesiredAccess, BOOL bInheritHandle,
                      LPCTSTR lpName)
{
    HANDLE ret = ::OpenMutex(dwDesiredAccess, bInheritHandle, lpName);
    CheckCreate(ret != NULL, __htMutex, __hoOpenMutex, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,
                        BOOL bManualReset, BOOL bInitialState, LPCTSTR lpName)
{
    HANDLE ret = ::CreateEvent(lpEventAttributes, bManualReset, bInitialState,
                               lpName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        DWORD err = GetLastError();
        switch (OutputType)
        {
        case __otMessages:
        {
            MESSAGE_TI(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageAlreadyExists, __GetHandlesOrigin(__hoCreateEvent), __GetHandlesTypeName(__htEvent), lpName), MB_OK);
            break;
        }

        case __otQuiet:
            break;
        }
        SetLastError(err);
    }
    CheckCreate(ret != NULL, __htEvent, __hoCreateEvent, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::OpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCTSTR lpName)
{
    HANDLE ret = ::OpenEvent(dwDesiredAccess, bInheritHandle, lpName);
    CheckCreate(ret != NULL, __htEvent, __hoOpenEvent, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateSemaphore(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
                            LONG lInitialCount, LONG lMaximumCount,
                            LPCTSTR lpName)
{
    HANDLE ret = ::CreateSemaphore(lpSemaphoreAttributes, lInitialCount,
                                   lMaximumCount, lpName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        DWORD err = GetLastError();
        switch (OutputType)
        {
        case __otMessages:
        {
            MESSAGE_TI(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageAlreadyExists, __GetHandlesOrigin(__hoCreateSemaphore), __GetHandlesTypeName(__htSemaphore), lpName), MB_OK);
            break;
        }

        case __otQuiet:
            break;
        }
        SetLastError(err);
    }
    CheckCreate(ret != NULL, __htSemaphore, __hoCreateSemaphore, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::OpenSemaphore(DWORD dwDesiredAccess, BOOL bInheritHandle,
                          LPCTSTR lpName)
{
    HANDLE ret = ::OpenSemaphore(dwDesiredAccess, bInheritHandle, lpName);
    CheckCreate(ret != NULL, __htSemaphore, __hoOpenSemaphore, ret, GetLastError());
    return ret;
}

#if (_WIN32_WINNT >= 0x0400)
HANDLE
C__Handles::CreateWaitableTimer(LPSECURITY_ATTRIBUTES lpTimerAttributes,
                                BOOL bManualReset, LPCTSTR lpTimerName)
{
    HANDLE ret = ::CreateWaitableTimer(lpTimerAttributes, bManualReset,
                                       lpTimerName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        DWORD err = GetLastError();
        switch (OutputType)
        {
        case __otMessages:
        {
            MESSAGE_TI(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageAlreadyExists, __GetHandlesOrigin(__hoCreateWaitableTimer), __GetHandlesTypeName(__htWaitableTimer), lpTimerName), MB_OK);
            break;
        }

        case __otQuiet:
            break;
        }
        SetLastError(err);
    }
    CheckCreate(ret != NULL, __htWaitableTimer, __hoCreateWaitableTimer, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::OpenWaitableTimer(DWORD dwDesiredAccess, BOOL bInheritHandle,
                              LPCTSTR lpTimerName)
{
    HANDLE ret = ::OpenWaitableTimer(dwDesiredAccess, bInheritHandle,
                                     lpTimerName);
    CheckCreate(ret != NULL, __htWaitableTimer, __hoOpenWaitableTimer, ret,
                GetLastError());
    return ret;
}
#endif // (_WIN32_WINNT >= 0x0400)

HANDLE
C__Handles::CreateFileMapping(HANDLE hFile,
                              LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                              DWORD flProtect, DWORD dwMaximumSizeHigh,
                              DWORD dwMaximumSizeLow, LPCTSTR lpName)
{
    HANDLE ret = ::CreateFileMapping(hFile, lpFileMappingAttributes, flProtect,
                                     dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        DWORD err = GetLastError();
        switch (OutputType)
        {
        case __otMessages:
        {
            MESSAGE_TI(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(__HandlesMessageAlreadyExists, __GetHandlesOrigin(__hoCreateFileMapping), __GetHandlesTypeName(__htFileMapping), lpName), MB_OK);
            break;
        }

        case __otQuiet:
            break;
        }
        SetLastError(err);
    }
    CheckCreate(ret != NULL, __htFileMapping, __hoCreateFileMapping, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::OpenFileMapping(DWORD dwDesiredAccess, BOOL bInheritHandle,
                            LPCTSTR lpName)
{
    HANDLE ret = ::OpenFileMapping(dwDesiredAccess, bInheritHandle, lpName);
    CheckCreate(ret != NULL, __htFileMapping, __hoOpenFileMapping, ret,
                GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateMailslot(LPCTSTR lpName, DWORD nMaxMessageSize,
                           DWORD lReadTimeout,
                           LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    HANDLE ret = ::CreateMailslot(lpName, nMaxMessageSize, lReadTimeout,
                                  lpSecurityAttributes);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htMailslot, __hoCreateMailslot, ret,
                GetLastError());
    return ret;
}

BOOL C__Handles::CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe,
                            LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize)
{
    BOOL ret = ::CreatePipe(hReadPipe, hWritePipe, lpPipeAttributes, nSize);
    if (ret)
    {
        CheckCreate(TRUE, __htReadPipe, __hoCreatePipe,
                    (hReadPipe != NULL) ? *hReadPipe : NULL, GetLastError(), FALSE);
    }
    CheckCreate(ret, __htWritePipe, __hoCreatePipe,
                (hWritePipe != NULL) ? *hWritePipe : NULL, GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateNamedPipe(LPCTSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode,
                            DWORD nMaxInstances, DWORD nOutBufferSize,
                            DWORD nInBufferSize, DWORD nDefaultTimeOut,
                            LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    HANDLE ret = ::CreateNamedPipe(lpName, dwOpenMode, dwPipeMode, nMaxInstances,
                                   nOutBufferSize, nInBufferSize, nDefaultTimeOut,
                                   lpSecurityAttributes);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htServerNamedPipe,
                __hoCreateNamedPipe, ret, GetLastError());
    return ret;
}

HANDLE
C__Handles::CreateConsoleScreenBuffer(DWORD dwDesiredAccess, DWORD dwShareMode,
                                      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                      DWORD dwFlags, LPVOID lpScreenBufferData)
{
    HANDLE ret = ::CreateConsoleScreenBuffer(dwDesiredAccess, dwShareMode,
                                             lpSecurityAttributes, dwFlags,
                                             lpScreenBufferData);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htConsoleScreenBuffer,
                __hoCreateConsoleScreenBuffer, ret, GetLastError());
    return ret;
}

BOOL C__Handles::DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
                                 HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle,
                                 DWORD dwDesiredAccess, BOOL bInheritHandle,
                                 DWORD dwOptions)
{
    BOOL ret = ::DuplicateHandle(hSourceProcessHandle, hSourceHandle,
                                 hTargetProcessHandle, lpTargetHandle,
                                 dwDesiredAccess, bInheritHandle, dwOptions);
    if (ret)
    {
        if (hTargetProcessHandle != GetCurrentProcess() ||
            hSourceProcessHandle != hTargetProcessHandle &&
                (dwOptions & DUPLICATE_CLOSE_SOURCE))
        {
            TRACE_E("HANDLES(DuplicateHandle(...)) is designed only for handles from "
                    "current process.");
        }

        // GetCurrentProcess vraci jakysi pseudohandle, takze tahle konstrukce
        // neni spravna, meli by se porovnat ID procesu a ne jejich handly ...

        if ((dwOptions & DUPLICATE_CLOSE_SOURCE) &&
            hSourceProcessHandle == GetCurrentProcess())
        {
            CheckClose(TRUE, hSourceHandle, __htHandle_comp_with_CloseHandle,
                       __GetHandlesOrigin(__hoDuplicateHandle), ERROR_SUCCESS, FALSE);
        }

        if (hTargetProcessHandle == GetCurrentProcess())
        {
            CheckCreate(TRUE, __htHandle_comp_with_CloseHandle, __hoDuplicateHandle,
                        (lpTargetHandle != NULL) ? *lpTargetHandle : NULL,
                        GetLastError(), FALSE);
        }
#ifdef MULTITHREADED_HANDLES_ENABLE
        ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
    }
    else
    {
        CheckCreate(FALSE, __htHandle_comp_with_CloseHandle, __hoDuplicateHandle,
                    (lpTargetHandle != NULL) ? *lpTargetHandle : NULL,
                    GetLastError());
    }
    return ret;
}

LPVOID
C__Handles::MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                          DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow,
                          DWORD dwNumberOfBytesToMap)
{
    LPVOID ret = ::MapViewOfFile(hFileMappingObject, dwDesiredAccess,
                                 dwFileOffsetHigh, dwFileOffsetLow,
                                 dwNumberOfBytesToMap);
    CheckCreate(ret != NULL, __htViewOfFile, __hoMapViewOfFile, ret, GetLastError());
    return ret;
}

LPVOID
C__Handles::MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                            DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow,
                            DWORD dwNumberOfBytesToMap, LPVOID lpBaseAddress)
{
    LPVOID ret = ::MapViewOfFileEx(hFileMappingObject, dwDesiredAccess,
                                   dwFileOffsetHigh, dwFileOffsetLow,
                                   dwNumberOfBytesToMap, lpBaseAddress);
    CheckCreate(ret != NULL, __htViewOfFile, __hoMapViewOfFileEx, ret, GetLastError());
    return ret;
}

BOOL C__Handles::UnmapViewOfFile(LPCVOID lpBaseAddress)
{
    BOOL ret = ::UnmapViewOfFile(lpBaseAddress);
    CheckClose(ret, (HANDLE)lpBaseAddress, __htViewOfFile,
               "UnmapViewOfFile", GetLastError());
    return ret;
}

HACCEL
C__Handles::CreateAcceleratorTable(LPACCEL lpaccl, int cEntries)
{
    HACCEL ret = ::CreateAcceleratorTable(lpaccl, cEntries);
    CheckCreate(ret != NULL, __htAccel, __hoCreateAcceleratorTable, ret);
    return ret;
}

HACCEL
C__Handles::LoadAccelerators(HINSTANCE hInstance, LPCTSTR lpTableName)
{
    HACCEL ret = ::LoadAccelerators(hInstance, lpTableName);
    CheckCreate(ret != NULL, __htAccel, __hoLoadAccelerators, ret);
    return ret;
}

BOOL C__Handles::DestroyAcceleratorTable(HACCEL hAccel)
{
    BOOL ret = ::DestroyAcceleratorTable(hAccel);
    CheckClose(ret, hAccel, __htAccel, "DestroyAcceleratorTable", GetLastError());
    return ret;
}

VOID C__Handles::EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    ::EnterCriticalSection(lpCriticalSection);

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckCreate(TRUE, __htEnterCriticalSection, __hoEnterCriticalSection,
                lpCriticalSection);
}

#if (_WIN32_WINNT >= 0x0400)
BOOL C__Handles::TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    BOOL ret = ::TryEnterCriticalSection(lpCriticalSection);

    if (ret)
        CheckCreate(TRUE, __htEnterCriticalSection, __hoTryEnterCriticalSection,
                    lpCriticalSection, ERROR_SUCCESS, FALSE);

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
    return ret;
}
#endif // (_WIN32_WINNT >= 0x0400)

VOID C__Handles::LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    ::LeaveCriticalSection(lpCriticalSection);
    CheckClose(TRUE, lpCriticalSection, __htEnterCriticalSection,
               "LeaveCriticalSection");
}

VOID C__Handles::InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    ::InitializeCriticalSection(lpCriticalSection);
    CheckCreate(TRUE, __htCriticalSection, __hoInitializeCriticalSection,
                lpCriticalSection);
}

VOID C__Handles::DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    ::DeleteCriticalSection(lpCriticalSection);
    CheckClose(TRUE, lpCriticalSection, __htCriticalSection,
               "DeleteCriticalSection");
}

HANDLE
C__Handles::FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    HANDLE ret = ::FindFirstFileA(lpFileName, lpFindFileData);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htFindFile, __hoFindFirstFile,
                ret, GetLastError(), TRUE, NULL, lpFileName);
    return ret;
}

HANDLE
C__Handles::FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
    HANDLE ret = ::FindFirstFileW(lpFileName, lpFindFileData);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htFindFile, __hoFindFirstFile,
                ret, GetLastError(), TRUE, NULL, NULL, lpFileName);
    return ret;
}

BOOL C__Handles::FindClose(HANDLE hFindFile)
{
    BOOL ret = ::FindClose(hFindFile);
    CheckClose(ret, hFindFile, __htFindFile, "FindClose", GetLastError());
    return ret;
}

#if (_WIN32_WINNT >= 0x0400)
LPVOID
C__Handles::CreateFiber(DWORD dwStackSize, LPFIBER_START_ROUTINE lpStartAddress,
                        LPVOID lpParameter)
{
    LPVOID ret = ::CreateFiber(dwStackSize, lpStartAddress, lpParameter);
    CheckCreate(ret != NULL, __htFiber, __hoCreateFiber, ret, GetLastError());
    return ret;
}

VOID C__Handles::DeleteFiber(LPVOID lpFiber)
{
    ::DeleteFiber(lpFiber);
    CheckClose(TRUE, lpFiber, __htFiber, "DeleteFiber");
}
#endif // (_WIN32_WINNT >= 0x0400)

DWORD
C__Handles::TlsAlloc(VOID)
{
    DWORD ret = ::TlsAlloc();
    CheckCreate(ret != 0xFFFFFFFF, __htThreadLocalStorage, __hoTlsAlloc,
                (HANDLE)(DWORD_PTR)ret, GetLastError());
    return ret;
}

BOOL C__Handles::TlsFree(DWORD dwTlsIndex)
{
    BOOL ret = ::TlsFree(dwTlsIndex);
    CheckClose(ret, (HANDLE)(DWORD_PTR)dwTlsIndex, __htThreadLocalStorage, "TlsFree");
    return ret;
}

HINSTANCE
C__Handles::LoadLibrary(LPCTSTR lpLibFileName)
{
    HINSTANCE ret = ::LoadLibrary(lpLibFileName);
    CheckCreate(ret != NULL, __htLibrary, __hoLoadLibrary, ret, GetLastError(), TRUE, lpLibFileName);
    return ret;
}

HINSTANCE
C__Handles::LoadLibraryEx(LPCTSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HINSTANCE ret = ::LoadLibraryEx(lpLibFileName, hFile, dwFlags);
    CheckCreate(ret != NULL, __htLibrary, __hoLoadLibraryEx, ret, GetLastError(), TRUE, lpLibFileName);
    return ret;
}

BOOL C__Handles::FreeLibrary(HMODULE hLibModule)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    BOOL ret = ::FreeLibrary(hLibModule); // obsahuje volani destruktoru globalek DLLka, muze obsahovat message-loopu

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckClose(ret, hLibModule, __htLibrary, "FreeLibrary");
    return ret;
}

VOID C__Handles::FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    ::FreeLibraryAndExitThread(hLibModule, dwExitCode); // obsahuje volani destruktoru globalek DLLka, muze obsahovat message-loopu

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckClose(TRUE, hLibModule, __htLibrary, "FreeLibraryAndExitThread");
}

HGLOBAL
C__Handles::GlobalAlloc(UINT uFlags, DWORD dwBytes)
{
    HGLOBAL ret = ::GlobalAlloc(uFlags, dwBytes);
    CheckCreate(ret != NULL, __htGlobal, __hoGlobalAlloc, ret, GetLastError());
    return ret;
}

HGLOBAL
C__Handles::GlobalReAlloc(HGLOBAL hMem, DWORD dwBytes, UINT uFlags)
{
    HGLOBAL ret = ::GlobalReAlloc(hMem, dwBytes, uFlags);
    if (ret != NULL)
    {
        if (ret != hMem)
        {
            CheckClose(TRUE, hMem, __htGlobal, __GetHandlesOrigin(__hoGlobalReAlloc),
                       ERROR_SUCCESS, FALSE);
            CheckCreate(TRUE, __htGlobal, __hoGlobalReAlloc, ret, GetLastError());
        }
        else
        {
#ifdef MULTITHREADED_HANDLES_ENABLE
            ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
        }
    }
    else
        CheckCreate(FALSE, __htGlobal, __hoGlobalReAlloc, ret, GetLastError());
    return ret;
}

HGLOBAL
C__Handles::GlobalFree(HGLOBAL hMem)
{
    HGLOBAL ret = ::GlobalFree(hMem);
    CheckClose(ret == NULL, hMem, __htGlobal, "GlobalFree", GetLastError());
    return ret;
}

LPVOID
C__Handles::GlobalLock(HGLOBAL hMem)
{
    LPVOID ret = ::GlobalLock(hMem);
    CheckCreate(ret != NULL, __htGlobalLock, __hoGlobalLock, hMem, GetLastError());
    return ret;
}

BOOL C__Handles::GlobalUnlock(HGLOBAL hMem)
{
    BOOL ret = ::GlobalUnlock(hMem);
    CheckClose(ret || GetLastError() == NO_ERROR, hMem, __htGlobalLock,
               "GlobalUnlock", GetLastError());
    return ret;
}

HANDLE
C__Handles::FindFirstChangeNotification(LPCTSTR lpPathName, BOOL bWatchSubtree,
                                        DWORD dwNotifyFilter)
{
    HANDLE ret = ::FindFirstChangeNotification(lpPathName, bWatchSubtree,
                                               dwNotifyFilter);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htChangeNotification,
                __hoFindFirstChangeNotification, ret, GetLastError());
    return ret;
}

BOOL C__Handles::FindCloseChangeNotification(HANDLE hChangeHandle)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    BOOL ret = ::FindCloseChangeNotification(hChangeHandle); // muze se kousnout i na dost dlouho

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckClose(ret, hChangeHandle, __htChangeNotification,
               "FindCloseChangeNotification", GetLastError());
    return ret;
}

LPVOID
C__Handles::GetEnvironmentStrings(VOID)
{
    LPVOID ret = ::GetEnvironmentStrings();
    CheckCreate(TRUE, __htEnvStrings, __hoGetEnvironmentStrings, ret);
    return ret;
}

BOOL C__Handles::FreeEnvironmentStrings(LPTSTR lpszEnvironmentBlock)
{
    BOOL ret = ::FreeEnvironmentStrings(lpszEnvironmentBlock);
    CheckClose(ret, lpszEnvironmentBlock, __htEnvStrings,
               "FreeEnvironmentStrings", GetLastError());
    return ret;
}

HLOCAL
C__Handles::LocalAlloc(UINT uFlags, UINT uBytes)
{
    HLOCAL ret = ::LocalAlloc(uFlags, uBytes);
    CheckCreate(ret != NULL, __htLocal, __hoLocalAlloc, ret, GetLastError());
    return ret;
}

HLOCAL
C__Handles::LocalReAlloc(HLOCAL hMem, UINT uBytes, UINT uFlags)
{
    HLOCAL ret = ::LocalReAlloc(hMem, uBytes, uFlags);
    if (ret != NULL)
    {
        if (ret != hMem)
        {
            CheckClose(TRUE, hMem, __htLocal, __GetHandlesOrigin(__hoLocalReAlloc),
                       ERROR_SUCCESS, FALSE);
            CheckCreate(TRUE, __htLocal, __hoLocalReAlloc, ret, GetLastError());
        }
        else
        {
#ifdef MULTITHREADED_HANDLES_ENABLE
            ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
        }
    }
    else
        CheckCreate(FALSE, __htLocal, __hoLocalReAlloc, ret, GetLastError());
    return ret;
}

HLOCAL
C__Handles::LocalFree(HLOCAL hMem)
{
    HLOCAL ret = ::LocalFree(hMem);
    CheckClose(ret == NULL, hMem, __htLocal, "LocalFree", GetLastError());
    return ret;
}

LPVOID
C__Handles::LocalLock(HLOCAL hMem)
{
    LPVOID ret = ::LocalLock(hMem);
    CheckCreate(ret != NULL, __htLocalLock, __hoLocalLock, ret, GetLastError());
    return ret;
}

BOOL C__Handles::LocalUnlock(HLOCAL hMem)
{
    BOOL ret = ::LocalUnlock(hMem);
    CheckClose(ret || GetLastError() == NO_ERROR, hMem, __htLocalLock,
               "LocalUnlock", GetLastError());
    return ret;
}

HANDLE
C__Handles::LoadImage(HINSTANCE hinst, LPCTSTR lpszName, UINT uType,
                      int cxDesired, int cyDesired, UINT fuLoad)
{
    HANDLE ret = ::LoadImage(hinst, lpszName, uType, cxDesired, cyDesired, fuLoad);
    C__HandlesType type;
    switch (uType)
    {
    case IMAGE_BITMAP:
        type = __htBitmap;
        break;
    case IMAGE_CURSOR:
        type = __htCursor;
        break;
    default:
        type = __htIcon;
        break;
    }
    CheckCreate(ret != NULL, type, __hoLoadImage, ret, GetLastError());
    return ret;
}

HICON
C__Handles::LoadIcon(HINSTANCE hInstance, LPCTSTR lpIconName)
{
    HICON ret = ::LoadIcon(hInstance, lpIconName);
    CheckCreate(ret != NULL, __htIcon, __hoLoadIcon, ret, GetLastError());
    return ret;
}

HFILE
C__Handles::_lcreat(LPCSTR lpPathName, int iAttribute)
{
    HFILE ret = ::_lcreat(lpPathName, iAttribute);
    CheckCreate(ret != HFILE_ERROR, __htFile, __ho_lcreat, (HANDLE)(UINT_PTR)ret, GetLastError());
    return ret;
}

HFILE
C__Handles::OpenFile(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle)
{
    HFILE ret = ::OpenFile(lpFileName, lpReOpenBuff, uStyle);
    CheckCreate(ret != HFILE_ERROR, __htFile, __hoOpenFile, (HANDLE)(UINT_PTR)ret, GetLastError(), TRUE); // lpFileName je jen char i v Unicode verzi = nepouzitelna funkce (zastarala), vyradil jsem ji z vypisu parametru
    return ret;
}

HFILE
C__Handles::_lopen(LPCSTR lpPathName, int iReadWrite)
{
    HFILE ret = ::_lopen(lpPathName, iReadWrite);
    CheckCreate(ret != HFILE_ERROR, __htFile, __ho_lopen, (HANDLE)(UINT_PTR)ret, GetLastError());
    return ret;
}

HFILE
C__Handles::_lclose(HFILE hFile)
{
    HFILE ret = ::_lclose(hFile);
    CheckClose(ret == NULL, (HANDLE)(UINT_PTR)hFile, __htFile, "_lclose", GetLastError());
    return ret;
}

HDWP C__Handles::BeginDeferWindowPos(int nNumWindows)
{
    HDWP ret = ::BeginDeferWindowPos(nNumWindows);
    CheckCreate(ret != NULL, __htDeferWindowPos, __hoBeginDeferWindowPos, (HANDLE)ret, GetLastError());
    return ret;
}

HDWP C__Handles::DeferWindowPos(HDWP hWinPosInfo, HWND hWnd, HWND hWndInsertAfter,
                                int x, int y, int cx, int cy, UINT uFlags)
{
    HDWP ret = ::DeferWindowPos(hWinPosInfo, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);

    if (ret != hWinPosInfo) // doslo k realokaci struktury - musime zmenit hodnotu hlidaneho handlu
    {
        CheckClose(TRUE, (HANDLE)hWinPosInfo, __htDeferWindowPos, __GetHandlesOrigin(__hoDeferWindowPos), ERROR_SUCCESS, FALSE);
        CheckCreate(ret != NULL, __htDeferWindowPos, __hoDeferWindowPos, (HANDLE)ret, GetLastError());
    }
    else
    {
#ifdef MULTITHREADED_HANDLES_ENABLE
        ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE
    }
    return ret;
}

BOOL C__Handles::EndDeferWindowPos(HDWP hWinPosInfo)
{
#ifdef MULTITHREADED_HANDLES_ENABLE
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);
#endif // MULTITHREADED_HANDLES_ENABLE

    BOOL ret = ::EndDeferWindowPos(hWinPosInfo); // obsahuje message-loopu

#ifdef MULTITHREADED_HANDLES_ENABLE
    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;
#endif // MULTITHREADED_HANDLES_ENABLE

    CheckClose(ret, (HANDLE)hWinPosInfo, __htDeferWindowPos, "EndDeferWindowPos", GetLastError());
    return ret;
}

#ifdef MULTITHREADED_HANDLES_ENABLE

uintptr_t
C__Handles::_beginthreadex(void* security, unsigned stack_size,
                           unsigned(__stdcall* start_address)(void*),
                           void* arglist, unsigned initflag,
                           unsigned* thrdid)
{
    uintptr_t ret = ::_beginthreadex(security, stack_size, start_address,
                                     arglist, initflag, thrdid);
    CheckCreate(ret != 0, __htThread, __ho_beginthreadex, (HANDLE)ret);
    return ret;
}

#endif // MULTITHREADED_HANDLES_ENABLE

HRESULT
C__Handles::RegisterDragDrop(HWND hwnd, IDropTarget* pDropTarget)
{
    HRESULT ret = ::RegisterDragDrop(hwnd, pDropTarget);
    CheckCreate(ret == S_OK, __htDropTarget, __hoRegisterDragDrop, hwnd);
    return ret;
}

HRESULT
C__Handles::RevokeDragDrop(HWND hwnd)
{
    HRESULT ret = ::RevokeDragDrop(hwnd);
    CheckClose(ret == S_OK, hwnd, __htDropTarget, "RevokeDragDrop");
    return ret;
}

BOOL C__Handles::OpenThreadToken(HANDLE ThreadHandle, DWORD DesiredAccess,
                                 BOOL OpenAsSelf, PHANDLE TokenHandle)
{
    BOOL ret = ::OpenThreadToken(ThreadHandle, DesiredAccess, OpenAsSelf, TokenHandle);
    CheckCreate(ret != 0, __htThreadToken, __hoOpenThreadToken, TokenHandle != NULL ? *TokenHandle : NULL, GetLastError());
    return ret;
}

BOOL C__Handles::OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle)
{
    BOOL ret = ::OpenProcessToken(ProcessHandle, DesiredAccess, TokenHandle);
    CheckCreate(ret != 0, __htProcessToken, __hoOpenProcessToken, TokenHandle != NULL ? *TokenHandle : NULL, GetLastError());
    return ret;
}

#endif // HANDLES_ENABLE
