// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"
//#include <windows.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif // _MSC_VER
#include <process.h>
//#include <commctrl.h>
#include <ostream>
#include <stdio.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(disable : 4073)
#pragma init_seg(lib)

#ifdef MHANDLES_ENABLE

#include "spl_base.h"
#include "dbg.h"
#include "mhandles.h"

C__StringStreamBuf __MessagesStringBuf;
C__Handles __Handles;

//*****************************************************************************
//
// included module MESSAGES (displaying messagebox in current or new thread)
//
//*****************************************************************************

#define __MESSAGES_BUFFER_SIZE 500
#define __SPRINTF_BUFFER_SIZE 300
#define __ERROR_BUFFER_SIZE 200

char __SPrintFBuffer[__SPRINTF_BUFFER_SIZE] = "";
char __ErrorBuffer[__ERROR_BUFFER_SIZE] = "";

const char* __MessagesTitle = "Message";
HWND __MessagesParent = NULL;

// provides access to functions and data of module to current thread
void EnterMessagesModul();
// call once the current thread does not need access to functions and data of module
void LeaveMessagesModul();

/// returns pointer to global buffer, which is filled with sprintf string
const char* spf(const char* formatString, ...);

/// returns pointer to global buffer, which is filled with error description
const char* err(DWORD error);

#define MESSAGE(parent, str, buttons) \
    (EnterMessagesModul(), __Handles.__MessagesStrStream << __FILE__ << " " << __LINE__ << ":\n\n" \
                                                         << str, \
     __Handles.__Messages) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesStringBuf.c_str(), __MessagesTitle, (buttons))

#define MESSAGE_T(str, buttons) \
    (EnterMessagesModul(), __Handles.__MessagesStrStream << __FILE__ << " " << __LINE__ << ":\n\n" \
                                                         << str, \
     __Handles.__Messages) \
        .MessageBoxT(__MessagesStringBuf.c_str(), \
                     __MessagesTitle, (buttons))

/** shows messagebox with given text and icon of the error, is not in new thread,
*    distributes message of calling thread */
#define MESSAGE_E(parent, str, buttons) \
    MESSAGE(parent, str, MB_ICONEXCLAMATION | (buttons))

/** shows messagebox with given text and icon of the information, in new thread,
    does not distribute message of calling thread */
#define MESSAGE_TI(str, buttons) \
    MESSAGE_T(str, MB_ICONINFORMATION | (buttons))

/** shows messagebox with given text and icon of the error, in new thread,
    does not distribute message of calling thread */
#define MESSAGE_TE(str, buttons) \
    MESSAGE_T(str, MB_ICONEXCLAMATION | (buttons))

// critical section for whole module - monitor
CRITICAL_SECTION __MessagesCriticalSection;

const char* __MessagesLowMemory = "Insufficient memory.";

//*****************************************************************************

void EnterMessagesModul()
{
    EnterCriticalSection(&__MessagesCriticalSection);
}

void LeaveMessagesModul()
{
    LeaveCriticalSection(&__MessagesCriticalSection);
}

//*****************************************************************************
//
// C__Messages
//

C__Messages::C__Messages()
{
    InitializeCriticalSection(&__MessagesCriticalSection);
}

C__Messages::~C__Messages()
{
    DeleteCriticalSection(&__MessagesCriticalSection);
}

struct C__MessageBoxData
{
    const char* Text;
    const char* Caption;
    UINT Type;
    int Return;
};

int CALLBACK __MessagesMessageBoxThreadF(C__MessageBoxData* data)
{ // must not wait for response of calling thread, because it will not react
    // therefore parent==NULL -> no disabling of windows etc.
    data->Return = MessageBox(NULL, data->Text, data->Caption, data->Type | MB_SETFOREGROUND);
    return 0;
}

int C__Messages::MessageBoxT(LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
    __Handles.__MessagesStrStream.flush(); // flush to buffer (in lpText)

    C__MessageBoxData data;
    data.Caption = lpCaption;
    data.Type = uType;
    data.Return = 0;

    int len = (int)strlen(lpText) + 1;
    char* message = (char*)malloc(len); // text backup
    if (message != NULL)
    {
        memcpy(message, lpText, len);
        data.Text = message;
    }
    else
        data.Text = __MessagesLowMemory;
    LeaveMessagesModul(); // now other threads + message loops can start to mess

    // MessageBox in new thread, so it does not send message to this thread
    DWORD threadID;
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)__MessagesMessageBoxThreadF, &data, 0, &threadID);
    if (thread != NULL)
    {
        WaitForSingleObject(thread, INFINITE); // wait until user confirms
        CloseHandle(thread);
    }
    // else TRACE_E("Unable to show MessageBox: " << data.Caption << ": " << data.Text);

    if (message != NULL)
        free(message);

    __MessagesStringBuf.erase(); // prepare for next message

    return data.Return;
}

int C__Messages::MessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
    __Handles.__MessagesStrStream.flush(); // flush to buffer (in lpText)

    int len = (int)strlen(lpText) + 1;
    char* message = (char*)malloc(len); // text backup
    char* txt = message;
    if (txt != NULL)
        memcpy(txt, lpText, len);
    else
        txt = (char*)__MessagesLowMemory;
    LeaveMessagesModul(); // now other threads + message loops can start to mess

    if (!IsWindow(hWnd))
        hWnd = NULL;
    int ret = ::MessageBox(hWnd, txt, lpCaption, uType);

    if (message != NULL)
        free(message);

    __MessagesStringBuf.erase(); // prepare for next message

    return ret;
}

//*****************************************************************************

const char* spf(const char* formatString, ...)
{
    va_list params;
    va_start(params, formatString);
#ifdef __BORLANDC__
    _vsnprintf(__SPrintFBuffer, __SPRINTF_BUFFER_SIZE, formatString, params);
    __SPrintFBuffer[__SPRINTF_BUFFER_SIZE - 1] = 0;
#else  // __BORLANDC__
    _vsnprintf_s(__SPrintFBuffer, _TRUNCATE, formatString, params);
#endif // __BORLANDC__
    va_end(params);
    return __SPrintFBuffer;
}

const char* err(DWORD error)
{
    wsprintf(__ErrorBuffer, "(%d) ", error);
    int len = (int)strlen(__ErrorBuffer);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  __ErrorBuffer + len,
                  __ERROR_BUFFER_SIZE - len,
                  NULL);
    return __ErrorBuffer;
}

//*****************************************************************************
//
// the end of included module MESSAGES
//
//*****************************************************************************

const char* __HandlesMessageCallFrom = "Called from: ";
const char* __HandlesMessageSpace = " ";
const char* __HandlesMessageLineEnd = ":\n\n";
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
        MESSAGE_E(NULL, __HandlesMessageLowMemory, MB_OK | MB_SETFOREGROUND);
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

C__Handles::C__Handles() : __MessagesStrStream(&__MessagesStringBuf), __Messages()
{
    ::InitializeCriticalSection(&CriticalSection);
    CanUseTrace = FALSE;
}

C__Handles::~C__Handles()
{
    // exclude handles, which are released automatically
    for (int i = Handles.Count - 1; i >= 0; i--)
    {
        if (Handles[i].Handle.Origin == __hoLoadAccelerators)
            Handles.Delete(i);
        else if (Handles[i].Handle.Origin == __hoLoadIcon)
            Handles.Delete(i);
        else if (Handles[i].Handle.Origin == __hoGetStockObject)
            Handles.Delete(i);
    }
    // check + print of the remaining
    if (Handles.Count != 0)
    {
        if (MESSAGE_E(NULL, "Some monitored handles remained opened.\n"
                                << __HandlesMessageNumberOpened << Handles.Count << "\nDo you want to list opened handles to Trace Server (ensure it is running) ?",
                      MB_YESNO | MB_SETFOREGROUND) == IDYES)
        {
            if (CanUseTrace)
            {
                SalamanderDebug->TraceConnectToServer(); // in case that server was not started
                TRACE_I("List of opened handles:");
                for (int i = 0; i < Handles.Count; i++)
                {
                    TRACE_MI(Handles[i].File, Handles[i].Line,
                             __GetHandlesTypeName(Handles[i].Handle.Type) << " - " << __GetHandlesOrigin(Handles[i].Handle.Origin) << " - " << Handles[i].Handle.Handle);
                }
            }
            else
            {
                MESSAGE_E(NULL, "Unable to send " << Handles.Count << " opened handles to Trace Server.\n"
                                                                      "You forgot to use HANDLES_CAN_USE_TRACE() in your plugin.",
                          MB_OK | MB_SETFOREGROUND);
            }
        }
        else
        {
            if (CanUseTrace)
            {
                SalamanderDebug->TraceConnectToServer(); // in case that server was not started
                TRACE_I(__HandlesMessageNumberOpened << Handles.Count);
            }
        }
    }
    else
    {
        if (CanUseTrace)
            TRACE_I("All monitored handles were closed.");
    }
    ::DeleteCriticalSection(&CriticalSection);
}

C__Handles&
C__Handles::SetInfo(const char* file, int line, C__HandlesOutputType outputType)
{
    ::EnterCriticalSection(&CriticalSection);
    if (CriticalSection.RecursionCount > 1)
    {
        DebugBreak(); // recursive call of handles !!! a masked message-loop again - see call-stack
    }
    OutputType = outputType;
    TemporaryHandle.File = file;
    TemporaryHandle.Line = line;
    return *this;
}

void C__Handles::InformationsToTrace(BOOL detail)
{
    ::EnterCriticalSection(&CriticalSection);
    if (Handles.Count > 0)
    {
        if (CanUseTrace)
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
                int i;
                for (i = 0; i < Handles.Count; i++)
                    count[Handles[i].Handle.Type]++;
                for (i = 0; i < __htCount; i++)
                {
                    if (count[i] > 0)
                        TRACE_I(__GetHandlesTypeName((C__HandlesType)i) << ": " << count[i]);
                }
            }
        }
    }
    else
    {
        if (CanUseTrace)
            TRACE_I("There are none currently monitored handles.");
    }
    ::LeaveCriticalSection(&CriticalSection);
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
                    org != __hoGetStockObject) // not a handle, which doesn't have to be released (priority is to release handles, which have to be released)
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
    if (foundTypeOK != -1) // found only handle, which doesn't have to be released
    {
        type = Handles[foundTypeOK].Handle.Type;
        if (origin != NULL)
            *origin = Handles[foundTypeOK].Handle.Origin;
        Handles.Delete(foundTypeOK);
        return TRUE;
    }
    if (found != -1) // found only handle with matching number
    {
#if defined(_DEBUG) || defined(__HANDLES_DEBUG)
        if (CanUseTrace)
        {
            C__HandlesData* data = &(Handles[found]);
            TRACE_I("Returned unexpected handle: " << data->File << "::" << data->Line << " - " << __GetHandlesTypeName(data->Handle.Type) << ", " << __GetHandlesOrigin(data->Handle.Origin) << ", " << data->Handle.Handle);
        }
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
                             BOOL synchronize, const char* params)
{
    DWORD old_error = GetLastError();

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
                MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(params != NULL ? __HandlesMessageReturnErrorMessageParams : __HandlesMessageReturnErrorMessage, __GetHandlesOrigin(origin), err(error), params), MB_OK);
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
                MESSAGE_TE(__HandlesMessageCallFrom << TemporaryHandle.File << __HandlesMessageSpace << TemporaryHandle.Line << __HandlesMessageLineEnd << spf(params != NULL ? __HandlesMessageReturnErrorParams : __HandlesMessageReturnError, __GetHandlesOrigin(origin), params), MB_OK);
                break;
            }

            case __otQuiet:
                break;
            }
        }
    }
    if (synchronize)
    {
        ::LeaveCriticalSection(&CriticalSection);
    }
    SetLastError(old_error);
}

void C__Handles::CheckClose(BOOL success, const HANDLE handle, C__HandlesType expType,
                            const char* function, DWORD error, BOOL synchronize)
{
    DWORD old_error = GetLastError();

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
        ::LeaveCriticalSection(&CriticalSection);
    }
    SetLastError(old_error);
}

//*****************************************************************************
//
// Monitored functions:
//

HANDLE
C__Handles::CreateFile(LPCTSTR lpFileName, DWORD dwDesiredAccess,
                       DWORD dwShareMode,
                       LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                       DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                       HANDLE hTemplateFile)
{
    HANDLE ret = ::CreateFile(lpFileName, dwDesiredAccess, dwShareMode,
                              lpSecurityAttributes, dwCreationDisposition,
                              dwFlagsAndAttributes, hTemplateFile);
    char paramsBuf[MAX_PATH + 200];
    const char* params = NULL;
    if (ret == INVALID_HANDLE_VALUE) // parameters to buffer only when error occurs (can be displayed)
    {
#ifdef __BORLANDC__
        _snprintf(paramsBuf, MAX_PATH + 200,
                  "%s,\ndwDesiredAccess=0x%X,\ndwShareMode=0x%X,\ndwCreationDisposition=0x%X,\ndwFlagsAndAttributes=0x%X",
                  lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes);
        paramsBuf[MAX_PATH + 200 - 1] = 0;
#else  // __BORLANDC__
        _snprintf_s(paramsBuf, _TRUNCATE,
                    "%s,\ndwDesiredAccess=0x%X,\ndwShareMode=0x%X,\ndwCreationDisposition=0x%X,\ndwFlagsAndAttributes=0x%X",
                    lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes);
#endif // __BORLANDC__
        params = paramsBuf;
    }
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htFile, __hoCreateFile, ret, GetLastError(), TRUE, params);
    return ret;
}

BOOL C__Handles::CloseHandle(HANDLE hObject)
{
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    BOOL ret = ::CloseHandle(hObject);

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

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
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    HDC ret = ::BeginPaint(hwnd, lpPaint); // contains message-loop

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

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
C__Handles::CreateFontIndirect(CONST LOGFONT* lplf)
{
    HFONT ret = ::CreateFontIndirect(lplf);
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
            else
            {
                ::LeaveCriticalSection(&CriticalSection);
            }
        }
        else
        {
            ::LeaveCriticalSection(&CriticalSection);
        }
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
    }
    CheckCreate(ret != NULL, __htWaitableTimer, __hoCreateWaitableTimer, ret,
                GetLastError());
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
    }
    CheckCreate(ret != NULL, __htFileMapping, __hoCreateFileMapping, ret,
                GetLastError());
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
            MESSAGE_TE("HANDLES(DuplicateHandle(...)) is designed only for handles from "
                       "current process.",
                       MB_OK);
        }

        // GetCurrentProcess return some pseudohandle, so this is not correct
        // way, we should compare process IDs and not their handles ...

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
        ::LeaveCriticalSection(&CriticalSection);
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
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    ::EnterCriticalSection(lpCriticalSection);

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

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

    ::LeaveCriticalSection(&CriticalSection);
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
C__Handles::FindFirstFile(LPCTSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData)
{
    HANDLE ret = ::FindFirstFile(lpFileName, lpFindFileData);
    CheckCreate(ret != INVALID_HANDLE_VALUE, __htFindFile, __hoFindFirstFile,
                ret, GetLastError(), TRUE, lpFileName);
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
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    BOOL ret = ::FreeLibrary(hLibModule); // contains call to DLL destructor, may contain message-loop

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

    CheckClose(ret, hLibModule, __htLibrary, "FreeLibrary");
    return ret;
}

VOID C__Handles::FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode)
{
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    ::FreeLibraryAndExitThread(hLibModule, dwExitCode); // contains call to DLL destructor, may contain message-loop

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

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
            ::LeaveCriticalSection(&CriticalSection);
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
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    BOOL ret = ::FindCloseChangeNotification(hChangeHandle); // can stuck for quite long time

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

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
            ::LeaveCriticalSection(&CriticalSection);
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
    CheckCreate(ret != HFILE_ERROR, __htFile, __hoOpenFile, (HANDLE)(UINT_PTR)ret, GetLastError(), TRUE, lpFileName);
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

    if (ret != hWinPosInfo) // structure reallocation occured - we must change value of watched handle
    {
        CheckClose(TRUE, (HANDLE)hWinPosInfo, __htDeferWindowPos, __GetHandlesOrigin(__hoDeferWindowPos), ERROR_SUCCESS, FALSE);
        CheckCreate(ret != NULL, __htDeferWindowPos, __hoDeferWindowPos, (HANDLE)ret, GetLastError());
    }
    else
    {
        ::LeaveCriticalSection(&CriticalSection);
    }
    return ret;
}

BOOL C__Handles::EndDeferWindowPos(HDWP hWinPosInfo)
{
    C__HandlesOutputType tmpOutputType = OutputType;
    C__HandlesData tmpTemporaryHandle = TemporaryHandle;
    ::LeaveCriticalSection(&CriticalSection);

    BOOL ret = ::EndDeferWindowPos(hWinPosInfo); // contains message-loop

    ::EnterCriticalSection(&CriticalSection);
    OutputType = tmpOutputType;
    TemporaryHandle = tmpTemporaryHandle;

    CheckClose(ret, (HANDLE)hWinPosInfo, __htDeferWindowPos, "EndDeferWindowPos", GetLastError());
    return ret;
}

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

#endif // MHANDLES_ENABLE
