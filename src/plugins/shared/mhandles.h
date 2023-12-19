// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

// macro MHANDLES_ENABLE - enables monitoring of handles
// CAUTION: call HANDLES_CAN_USE_TRACE() immediately after initialization of
//          "dbg.h" module (after initialization of SalamanderDebug and SalamanderVersion)
// CAUTION: MHANDLES are initialized/destroyed on "lib" level, so if plugin
//          uses "lib" level (or "compiler" level), it must ensure that it will
//          not use MHANDLES on these levels (see #pragma init_seg (lib))
// NOTE: for easier placement of HANDLES() and HANDLES_Q() macros use CheckHnd.exe program

#define NOHANDLES(function) function

#ifndef MHANDLES_ENABLE

// so that no problems with semicolons in the below defined macros occur
inline void __HandlesEmptyFunction() {}

#define HANDLES_CAN_USE_TRACE() __HandlesEmptyFunction()
#define HANDLES(function) ::function
#define HANDLES_Q(function) ::function
#define HANDLES_COUNTS_TO_TRACE() __HandlesEmptyFunction()
#define HANDLES_LIST_TO_TRACE() __HandlesEmptyFunction()
#define HANDLES_ADD(type, origin, handle) __HandlesEmptyFunction()
#define HANDLES_ADD_EX(outputType, success, type, origin, handle, error, synchronize) __HandlesEmptyFunction()
#define HANDLES_REMOVE(handle, expType, function) __HandlesEmptyFunction()

#else // MHANDLES_ENABLE

#define HANDLES_CAN_USE_TRACE() __Handles.SetCanUseTrace()
#define HANDLES(function) __Handles.SetInfo(__FILE__, __LINE__).##function
#define HANDLES_Q(function) __Handles.SetInfo(__FILE__, __LINE__, __otQuiet).##function
#define HANDLES_COUNTS_TO_TRACE() __Handles.InformationsToTrace(FALSE)
#define HANDLES_LIST_TO_TRACE() __Handles.InformationsToTrace(TRUE)
#define HANDLES_ADD(type, origin, handle) __Handles.SetInfo(__FILE__, __LINE__).CheckCreate(TRUE, type, origin, handle)
#define HANDLES_ADD_EX(outputType, success, type, origin, handle, error, synchronize) __Handles.SetInfo(__FILE__, __LINE__, outputType).CheckCreate(success, type, origin, handle, error, synchronize)
#define HANDLES_REMOVE(handle, expType, function) __Handles.SetInfo(__FILE__, __LINE__).CheckClose(TRUE, handle, expType, function)

enum C__HandlesOutputType
{
    __otMessages,
    __otQuiet
};

enum C__HandlesType
{
    __htHandle_comp_with_CloseHandle,  // DeleteObject() and GetStockObject() compatible handle
    __htHandle_comp_with_DeleteObject, // DeleteObject() and GetStockObject() compatible handle
    __htKey,
    __htIcon,
    __htGlobal,
    __htMetaFile,
    __htEnhMetaFile,
    __htCursor,
    __htViewOfFile,
    __htAccel,
    __htCriticalSection,
    __htEnterCriticalSection,
    __htFindFile,
    __htFiber,
    __htThreadLocalStorage,
    __htLibrary,
    __htGlobalLock,
    __htChangeNotification,
    __htEnvStrings,
    __htLocal,
    __htLocalLock,
    __htDropTarget,

    __htDC,
    __htPen,
    __htBrush,
    __htFont,
    __htBitmap,
    __htRegion,
    __htPalette,

    __htFile,
    __htThread,
    __htProcess,
    __htEvent,
    __htMutex,
    __htSemaphore,
    __htWaitableTimer,
    __htFileMapping,
    __htMailslot,
    __htReadPipe,
    __htWritePipe,
    __htServerNamedPipe,
    __htConsoleScreenBuffer,
    __htDeferWindowPos,
    __htThreadToken,
    __htProcessToken,

    __htCount
};

enum C__HandlesOrigin
{
    __hoUnknown,
    __hoCreateFile,
    __hoCreateBrushIndirect,
    __hoGetDC,
    __hoCreateCompatibleBitmap,
    __hoCreatePen,
    __hoBeginPaint,
    __hoCreateCompatibleDC,
    __hoCreateRectRgn,
    __hoCreateBitmap,
    __hoCreateBitmapIndirect,
    __hoCreateMetaFile,
    __hoCreateEnhMetaFile,
    __hoCloseMetaFile,
    __hoCloseEnhMetaFile,
    __hoCopyMetaFile,
    __hoCopyEnhMetaFile,
    __hoGetEnhMetaFile,
    __hoSetWinMetaFileBits,
    __hoCreateIcon,
    __hoCreateIconIndirect,
    __hoCreateIconFromResource,
    __hoCreateIconFromResourceEx,
    __hoCopyIcon,
    __hoCreateFont,
    __hoCreateFontIndirect,
    __hoCreateDC,
    __hoGetDCEx,
    __hoGetWindowDC,
    __hoCopyImage,
    __hoCreateCursor,
    __hoRegCreateKey,
    __hoRegCreateKeyEx,
    __hoRegOpenKey,
    __hoRegOpenKeyEx,
    __hoRegConnectRegistry,
    __hoCreateIC,
    __hoCreateSolidBrush,
    __hoCreateHatchBrush,
    __hoCreatePatternBrush,
    __hoCreateDIBPatternBrush,
    __hoCreateDIBPatternBrushPt,
    __hoCreateDIBitmap,
    __hoCreateDIBSection,
    __hoCreateDiscardableBitmap,
    __hoCreateMappedBitmap,
    __hoLoadBitmap,
    __hoCreatePenIndirect,
    __hoCreateRectRgnIndirect,
    __hoCreateEllipticRgn,
    __hoCreateEllipticRgnIndirect,
    __hoCreatePolyPolygonRgn,
    __hoCreatePolygonRgn,
    __hoCreateRoundRectRgn,
    __hoCreatePalette,
    __hoCreateHalftonePalette,
    __hoCreateThread,
    __hoCreateRemoteThread,
    __hoCreateProcess,
    __hoOpenProcess,
    __hoCreateMutex,
    __hoOpenMutex,
    __hoCreateEvent,
    __hoOpenEvent,
    __hoCreateSemaphore,
    __hoOpenSemaphore,
    __hoCreateWaitableTimer,
    __hoOpenWaitableTimer,
    __hoCreateFileMapping,
    __hoOpenFileMapping,
    __hoCreateMailslot,
    __hoCreatePipe,
    __hoCreateNamedPipe,
    __hoCreateConsoleScreenBuffer,
    __hoDuplicateHandle,
    __hoMapViewOfFile,
    __hoMapViewOfFileEx,
    __hoCreateAcceleratorTable,
    __hoLoadAccelerators,
    __hoEnterCriticalSection,
    __hoInitializeCriticalSection,
    __hoTryEnterCriticalSection,
    __hoFindFirstFile,
    __hoCreateFiber,
    __hoTlsAlloc,
    __hoLoadLibrary,
    __hoLoadLibraryEx,
    __hoGlobalAlloc,
    __hoGlobalReAlloc,
    __hoGlobalLock,
    __hoFindFirstChangeNotification,
    __hoGetEnvironmentStrings,
    __hoLocalAlloc,
    __hoLocalReAlloc,
    __hoLocalLock,
    __hoLoadImage,
    __hoLoadIcon,
    __ho_lcreat,
    __hoOpenFile,
    __ho_lopen,
    __ho_beginthreadex,
    __hoRegisterDragDrop,
    __hoGetStockObject,
    __hoBeginDeferWindowPos,
    __hoDeferWindowPos,
    __hoOpenThreadToken,
    __hoOpenProcessToken,
};

struct C__HandlesHandle
{
    C__HandlesType Type;
    C__HandlesOrigin Origin;
    HANDLE Handle; // universal, for all types of handles

    C__HandlesHandle() {}

    C__HandlesHandle(C__HandlesType type, C__HandlesOrigin origin, HANDLE handle)
    {
        Type = type;
        Origin = origin;
        Handle = handle;
    }
};

struct C__HandlesData
{
    const char* File;
    int Line;
    C__HandlesHandle Handle;
};

class C_HandlesDataArray
{
public:
    int Count;

protected:
    C__HandlesData* Data;
    int Available;

public:
    C_HandlesDataArray();
    ~C_HandlesDataArray();

    int Add(const C__HandlesData& member);
    void Delete(int index);
    C__HandlesData& operator[](int index) { return Data[index]; }

protected:
    void EnlargeArray();
    void Move(int direction, int first, int count);
};

class C__Messages
{
public:
    C__Messages();
    ~C__Messages();

    int MessageBoxT(LPCTSTR lpText,    // address of text in message box
                    LPCTSTR lpCaption, // address of title of message box
                    UINT uType);       // style of message box

    int MessageBox(HWND hWnd,         // handle of owner window
                   LPCTSTR lpText,    // address of text in message box
                   LPCTSTR lpCaption, // address of title of message box
                   UINT uType);       // style of message box
};

#ifdef __BORLANDC__
typedef unsigned int uintptr_t;
#endif // __BORLANDC__

class C__Handles
{
public:
    // objects for work with messages boxes, here is only for guarantee of construction/destruction order
    std::ostream __MessagesStrStream;
    C__Messages __Messages;

protected:
    C_HandlesDataArray Handles;       // all checked handles
    C__HandlesData TemporaryHandle;   // set from SetInfo() when inserting
    C__HandlesOutputType OutputType;  // type of messages output
    CRITICAL_SECTION CriticalSection; // to synchronize multi-thread access
    BOOL CanUseTrace;                 // TRUE after initialization of "dbg.h" module, after that TRACE_ macros can be used

public:
    C__Handles();

    ~C__Handles();

    C__Handles& SetInfo(const char* file, int line,
                        C__HandlesOutputType outputType = __otMessages);

    void InformationsToTrace(BOOL detail);

    void SetCanUseTrace() { CanUseTrace = TRUE; }

    // Auxiliary functions:

    void CheckCreate(BOOL success, C__HandlesType type,
                     C__HandlesOrigin origin, const HANDLE handle,
                     DWORD error = ERROR_SUCCESS, BOOL synchronize = TRUE,
                     const char* params = NULL);

    void CheckClose(BOOL success, const HANDLE handle, C__HandlesType expType,
                    const char* function, DWORD error = ERROR_SUCCESS,
                    BOOL synchronize = TRUE);

    // Monitored functions:

    HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT lpPaint);
    BOOL EndPaint(HWND hWnd, CONST PAINTSTRUCT* lpPaint);

    HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                        DWORD dwStackSize,
                        LPTHREAD_START_ROUTINE lpStartAddress,
                        LPVOID lpParameter, DWORD dwCreationFlags,
                        LPDWORD lpThreadId);

    HANDLE CreateRemoteThread(HANDLE hProcess,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes,
                              DWORD dwStackSize,
                              LPTHREAD_START_ROUTINE lpStartAddress,
                              LPVOID lpParameter, DWORD dwCreationFlags,
                              LPDWORD lpThreadId);

    BOOL CreateProcess(LPCTSTR lpApplicationName, LPTSTR lpCommandLine,
                       LPSECURITY_ATTRIBUTES lpProcessAttributes,
                       LPSECURITY_ATTRIBUTES lpThreadAttributes,
                       BOOL bInheritHandles, DWORD dwCreationFlags,
                       LPVOID lpEnvironment, LPCTSTR lpCurrentDirectory,
                       LPSTARTUPINFO lpStartupInfo,
                       LPPROCESS_INFORMATION lpProcessInformation);

    HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle,
                       DWORD dwProcessId);

    HANDLE CreateMutex(LPSECURITY_ATTRIBUTES lpMutexAttributes,
                       BOOL bInitialOwner, LPCTSTR lpName);

    HANDLE OpenMutex(DWORD dwDesiredAccess, BOOL bInheritHandle,
                     LPCTSTR lpName);

    HANDLE CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,
                       BOOL bManualReset, BOOL bInitialState, LPCTSTR lpName);

    HANDLE OpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCTSTR lpName);

    HANDLE CreateSemaphore(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
                           LONG lInitialCount, LONG lMaximumCount,
                           LPCTSTR lpName);

    HANDLE OpenSemaphore(DWORD dwDesiredAccess, BOOL bInheritHandle,
                         LPCTSTR lpName);

    HANDLE CreateWaitableTimer(LPSECURITY_ATTRIBUTES lpTimerAttributes,
                               BOOL bManualReset, LPCTSTR lpTimerName);

    HANDLE OpenWaitableTimer(DWORD dwDesiredAccess, BOOL bInheritHandle,
                             LPCTSTR lpTimerName);

    HANDLE CreateFileMapping(HANDLE hFile,
                             LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                             DWORD flProtect, DWORD dwMaximumSizeHigh,
                             DWORD dwMaximumSizeLow, LPCTSTR lpName);

    HANDLE OpenFileMapping(DWORD dwDesiredAccess, BOOL bInheritHandle,
                           LPCTSTR lpName);

    HANDLE CreateFile(LPCTSTR lpFileName, DWORD dwDesiredAccess,
                      DWORD dwShareMode,
                      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                      DWORD dwCreationDistribution, DWORD dwFlagsAndAttributes,
                      HANDLE hTemplateFile);

    HANDLE CreateMailslot(LPCTSTR lpName, DWORD nMaxMessageSize,
                          DWORD lReadTimeout,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes);

    BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe,
                    LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);

    HANDLE CreateNamedPipe(LPCTSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode,
                           DWORD nMaxInstances, DWORD nOutBufferSize,
                           DWORD nInBufferSize, DWORD nDefaultTimeOut,
                           LPSECURITY_ATTRIBUTES lpSecurityAttributes);

    HANDLE CreateConsoleScreenBuffer(DWORD dwDesiredAccess, DWORD dwShareMode,
                                     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                     DWORD dwFlags, LPVOID lpScreenBufferData);

    BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
                         HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle,
                         DWORD dwDesiredAccess, BOOL bInheritHandle,
                         DWORD dwOptions);

    BOOL CloseHandle(HANDLE hObject);

    HBRUSH CreateBrushIndirect(CONST LOGBRUSH* lplb);

    HBRUSH CreateSolidBrush(COLORREF crColor);

    HBRUSH CreateHatchBrush(int fnStyle, COLORREF clrref);

    HBRUSH CreatePatternBrush(HBITMAP hbmp);

    HBRUSH CreateDIBPatternBrush(HGLOBAL hglbDIBPacked, UINT fuColorSpec);

    HBRUSH CreateDIBPatternBrushPt(CONST VOID* lpPackedDIB, UINT iUsage);

    HBITMAP CreateBitmap(int nWidth, int nHeight, UINT cPlanes,
                         UINT cBitsPerPel, CONST VOID* lpvBits);

    HBITMAP CreateBitmapIndirect(CONST BITMAP* lpbm);

    HBITMAP CreateCompatibleBitmap(HDC hdc, int nWidth, int nHeight);

    HBITMAP CreateDIBitmap(HDC hdc, CONST BITMAPINFOHEADER* lpbmih,
                           DWORD fdwInit, CONST VOID* lpbInit,
                           CONST BITMAPINFO* lpbmi, UINT fuUsage);

    HBITMAP CreateDIBSection(HDC hdc, CONST BITMAPINFO* pbmi, UINT iUsage,
                             VOID** ppvBits, HANDLE hSection, DWORD dwOffset);

    HBITMAP CreateDiscardableBitmap(HDC hdc, int nWidth, int nHeight);

    HBITMAP CreateMappedBitmap(HINSTANCE hInstance, int idBitmap, UINT wFlags,
                               LPCOLORMAP lpColorMap, int iNumMaps);

    HBITMAP LoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName);

    HPEN CreatePen(int fnPenStyle, int nWidth, COLORREF crColor);

    HPEN CreatePenIndirect(CONST LOGPEN* lplgpn);

    HRGN CreateRectRgn(int nLeftRect, int nTopRect, int nRightRect,
                       int nBottomRect);

    HRGN CreateRectRgnIndirect(CONST RECT* lprc);

    HRGN CreateEllipticRgn(int nLeftRect, int nTopRect, int nRightRect,
                           int nBottomRect);

    HRGN CreateEllipticRgnIndirect(CONST RECT* lprc);

    HRGN CreatePolyPolygonRgn(CONST POINT* lppt, CONST INT* lpPolyCounts,
                              int nCount, int fnPolyFillMode);

    HRGN CreatePolygonRgn(CONST POINT* lppt, int cPoints, int fnPolyFillMode);

    HRGN CreateRoundRectRgn(int nLeftRect, int nTopRect, int nRightRect,
                            int nBottomRect, int nWidthEllipse,
                            int nHeightEllipse);

    HFONT CreateFont(int nHeight, int nWidth, int nEscapement, int nOrientation,
                     int fnWeight, DWORD fdwItalic, DWORD fdwUnderline,
                     DWORD fdwStrikeOut, DWORD fdwCharSet,
                     DWORD fdwOutputPrecision, DWORD fdwClipPrecision,
                     DWORD fdwQuality, DWORD fdwPitchAndFamily, LPCTSTR lpszFace);

    HFONT CreateFontIndirect(CONST LOGFONT* lplf);

    HPALETTE CreatePalette(CONST LOGPALETTE* lplgpl);

    HPALETTE CreateHalftonePalette(HDC hdc);

    HGDIOBJ GetStockObject(int fnObject);

    BOOL DeleteObject(HGDIOBJ hObject);

    HDC GetDC(HWND hWnd);

    HDC GetDCEx(HWND hWnd, HRGN hrgnClip, DWORD flags);

    HDC GetWindowDC(HWND hWnd);

    int ReleaseDC(HWND hWnd, HDC hDC);

    HDC CreateDC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput,
                 CONST DEVMODE* lpInitData);

    HDC CreateCompatibleDC(HDC hdc);

    HDC CreateIC(LPCTSTR lpszDriver, LPCTSTR lpszDevice, LPCTSTR lpszOutput,
                 CONST DEVMODE* lpdvmInit);

    BOOL DeleteDC(HDC hdc);

    HDC CreateMetaFile(LPCTSTR lpszFile);

    HDC CreateEnhMetaFile(HDC hdcRef, LPCTSTR lpFilename, CONST RECT* lpRect,
                          LPCTSTR lpDescription);

    HMETAFILE CloseMetaFile(HDC hdc);

    HENHMETAFILE CloseEnhMetaFile(HDC hdc);

    HMETAFILE CopyMetaFile(HMETAFILE hmfSrc, LPCTSTR lpszFile);

    HENHMETAFILE CopyEnhMetaFile(HENHMETAFILE hemfSrc, LPCTSTR lpszFile);

    HENHMETAFILE GetEnhMetaFile(LPCTSTR lpszMetaFile);

    HENHMETAFILE SetWinMetaFileBits(UINT cbBuffer, CONST BYTE* lpbBuffer,
                                    HDC hdcRef, CONST METAFILEPICT* lpmfp);

    BOOL DeleteMetaFile(HMETAFILE hmf);

    BOOL DeleteEnhMetaFile(HENHMETAFILE hemf);

    HICON CreateIcon(HINSTANCE hInstance, int nWidth, int nHeight,
                     BYTE cPlanes, BYTE cBitsPixel, CONST BYTE* lpbANDbits,
                     CONST BYTE* lpbXORbits);

    HICON CreateIconIndirect(PICONINFO piconinfo);

    HICON CreateIconFromResource(PBYTE presbits, DWORD dwResSize, BOOL fIcon,
                                 DWORD dwVer);

    HICON CreateIconFromResourceEx(PBYTE pbIconBits, DWORD cbIconBits,
                                   BOOL fIcon, DWORD dwVersion, int cxDesired,
                                   int cyDesired, UINT uFlags);

    HICON CopyIcon(HICON hIcon);

    HICON LoadIcon(HINSTANCE hInstance, LPCTSTR lpIconName);

    BOOL DestroyIcon(HICON hIcon);

    HANDLE CopyImage(HANDLE hImage, UINT uType, int cxDesired, int cyDesired,
                     UINT fuFlags);

    HANDLE LoadImage(HINSTANCE hinst, LPCTSTR lpszName, UINT uType,
                     int cxDesired, int cyDesired, UINT fuLoad);

    HCURSOR CreateCursor(HINSTANCE hInst, int xHotSpot, int yHotSpot,
                         int nWidth, int nHeight, CONST VOID* pvANDPlane,
                         CONST VOID* pvXORPlane);

    BOOL DestroyCursor(HCURSOR hCursor);

    LONG RegCreateKey(HKEY hKey, LPCTSTR lpSubKey, PHKEY phkResult);

    LONG RegCreateKeyEx(HKEY hKey, LPCTSTR lpSubKey, DWORD Reserved,
                        LPTSTR lpClass, DWORD dwOptions, REGSAM samDesired,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                        PHKEY phkResult, LPDWORD lpdwDisposition);

    LONG RegOpenKey(HKEY hKey, LPCTSTR lpSubKey, PHKEY phkResult);

    LONG RegOpenKeyEx(HKEY hKey, LPCTSTR lpSubKey, DWORD ulOptions,
                      REGSAM samDesired, PHKEY phkResult);

    LONG RegConnectRegistry(LPTSTR lpMachineName, HKEY hKey, PHKEY phkResult);

    LONG RegCloseKey(HKEY hKey);

    LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                         DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow,
                         DWORD dwNumberOfBytesToMap);

    LPVOID MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
                           DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow,
                           DWORD dwNumberOfBytesToMap, LPVOID lpBaseAddress);

    BOOL UnmapViewOfFile(LPCVOID lpBaseAddress);

    HACCEL CreateAcceleratorTable(LPACCEL lpaccl, int cEntries);

    HACCEL LoadAccelerators(HINSTANCE hInstance, LPCTSTR lpTableName);

    BOOL DestroyAcceleratorTable(HACCEL hAccel);

    VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

    VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

    VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

#if (_WIN32_WINNT >= 0x0400)
    BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
#endif // (_WIN32_WINNT >= 0x0400)

    VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

    HANDLE FindFirstFile(LPCTSTR lpFileName, LPWIN32_FIND_DATA lpFindFileData);

    BOOL FindClose(HANDLE hFindFile);

#if (_WIN32_WINNT >= 0x0400)
    LPVOID CreateFiber(DWORD dwStackSize, LPFIBER_START_ROUTINE lpStartAddress,
                       LPVOID lpParameter);

    VOID DeleteFiber(LPVOID lpFiber);
#endif // (_WIN32_WINNT >= 0x0400)

    DWORD TlsAlloc(VOID);

    BOOL TlsFree(DWORD dwTlsIndex);

    HINSTANCE LoadLibrary(LPCTSTR lpLibFileName);

    HINSTANCE LoadLibraryEx(LPCTSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

    BOOL FreeLibrary(HMODULE hLibModule);

    VOID FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode);

    HGLOBAL GlobalAlloc(UINT uFlags, DWORD dwBytes);

    HGLOBAL GlobalReAlloc(HGLOBAL hMem, DWORD dwBytes, UINT uFlags);

    HGLOBAL GlobalFree(HGLOBAL hMem);

    LPVOID GlobalLock(HGLOBAL hMem);

    BOOL GlobalUnlock(HGLOBAL hMem);

    HANDLE FindFirstChangeNotification(LPCTSTR lpPathName, BOOL bWatchSubtree,
                                       DWORD dwNotifyFilter);

    BOOL FindCloseChangeNotification(HANDLE hChangeHandle);

    LPVOID GetEnvironmentStrings(VOID);

    BOOL FreeEnvironmentStrings(LPTSTR lpszEnvironmentBlock);

    HLOCAL LocalAlloc(UINT uFlags, UINT uBytes);

    HLOCAL LocalReAlloc(HLOCAL hMem, UINT uBytes, UINT uFlags);

    HLOCAL LocalFree(HLOCAL hMem);

    LPVOID LocalLock(HLOCAL hMem);

    BOOL LocalUnlock(HLOCAL hMem);

    HFILE _lcreat(LPCSTR lpPathName, int iAttribute);

    HFILE OpenFile(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle);

    HFILE _lopen(LPCSTR lpPathName, int iReadWrite);

    HFILE _lclose(HFILE hFile);

    uintptr_t _beginthreadex(void* security, unsigned stack_size,
                             unsigned(__stdcall* start_address)(void*),
                             void* arglist, unsigned initflag,
                             unsigned* thrdid);

    HRESULT RegisterDragDrop(HWND hwnd, IDropTarget* pDropTarget);

    HRESULT RevokeDragDrop(HWND hwnd);

    HDWP BeginDeferWindowPos(int nNumWindows);

    HDWP DeferWindowPos(HDWP hWinPosInfo, HWND hWnd, HWND hWndInsertAfter,
                        int x, int y, int cx, int cy, UINT uFlags);

    BOOL EndDeferWindowPos(HDWP hWinPosInfo);

    BOOL OpenThreadToken(HANDLE ThreadHandle, DWORD DesiredAccess,
                         BOOL OpenAsSelf, PHANDLE TokenHandle);

    BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle);

protected:
    void AddHandle(C__HandlesHandle handle); // adds TemporaryHandle

    // removes handle, returns TRUE on success
    BOOL DeleteHandle(C__HandlesType& type, HANDLE handle,
                      C__HandlesOrigin* origin,
                      C__HandlesType expType);
};

extern C__Handles __Handles;

#endif // MHANDLES_ENABLE
