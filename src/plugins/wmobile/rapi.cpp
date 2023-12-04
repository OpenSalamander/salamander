// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#define COPY_BUFFER_SIZE (32 * 1024)

#define RAPI_FUNCTIONS \
    Func_Operator(CeRapiInitEx) \
        Func_Operator(CeRapiInit) \
            Func_Operator(CeRapiUninit) \
                Func_Operator(CeRapiGetError) \
                    Func_Operator(CeRapiFreeBuffer) \
                        Func_Operator(CeRapiInvoke) \
                            Func_Operator(CeCreateDatabase) \
                                Func_Operator(CeDeleteDatabase) \
                                    Func_Operator(CeDeleteRecord) \
                                        Func_Operator(CeFindFirstDatabase) \
                                            Func_Operator(CeFindNextDatabase) \
                                                Func_Operator(CeOidGetInfo) \
                                                    Func_Operator(CeOpenDatabase) \
                                                        Func_Operator(CeReadRecordProps) \
                                                            Func_Operator(CeSeekDatabase) \
                                                                Func_Operator(CeSetDatabaseInfo) \
                                                                    Func_Operator(CeWriteRecordProps) \
                                                                        Func_Operator(CeFindFirstFile) \
                                                                            Func_Operator(CeFindNextFile) \
                                                                                Func_Operator(CeFindClose) \
                                                                                    Func_Operator(CeGetFileAttributes) \
                                                                                        Func_Operator(CeSetFileAttributes) \
                                                                                            Func_Operator(CeCreateFile) \
                                                                                                Func_Operator(CeReadFile) \
                                                                                                    Func_Operator(CeWriteFile) \
                                                                                                        Func_Operator(CeCloseHandle) \
                                                                                                            Func_Operator(CeFindAllFiles) \
                                                                                                                Func_Operator(CeFindAllDatabases) \
                                                                                                                    Func_Operator(CeGetLastError) \
                                                                                                                        Func_Operator(CeSetFilePointer) \
                                                                                                                            Func_Operator(CeSetEndOfFile) \
                                                                                                                                Func_Operator(CeCreateDirectory) \
                                                                                                                                    Func_Operator(CeRemoveDirectory) \
                                                                                                                                        Func_Operator(CeCreateProcess) \
                                                                                                                                            Func_Operator(CeMoveFile) \
                                                                                                                                                Func_Operator(CeCopyFile) \
                                                                                                                                                    Func_Operator(CeDeleteFile) \
                                                                                                                                                        Func_Operator(CeGetFileSize) \
                                                                                                                                                            Func_Operator(CeRegOpenKeyEx) \
                                                                                                                                                                Func_Operator(CeRegEnumKeyEx) \
                                                                                                                                                                    Func_Operator(CeRegCreateKeyEx) \
                                                                                                                                                                        Func_Operator(CeRegCloseKey) \
                                                                                                                                                                            Func_Operator(CeRegDeleteKey) \
                                                                                                                                                                                Func_Operator(CeRegEnumValue) \
                                                                                                                                                                                    Func_Operator(CeRegDeleteValue) \
                                                                                                                                                                                        Func_Operator(CeRegQueryInfoKey) \
                                                                                                                                                                                            Func_Operator(CeRegQueryValueEx) \
                                                                                                                                                                                                Func_Operator(CeRegSetValueEx) \
                                                                                                                                                                                                    Func_Operator(CeGetStoreInformation) \
                                                                                                                                                                                                        Func_Operator(CeGetSystemMetrics) \
                                                                                                                                                                                                            Func_Operator(CeGetDesktopDeviceCaps) \
                                                                                                                                                                                                                Func_Operator(CeGetSystemInfo) \
                                                                                                                                                                                                                    Func_Operator(CeSHCreateShortcut) \
                                                                                                                                                                                                                        Func_Operator(CeSHGetShortcutTarget) \
                                                                                                                                                                                                                            Func_Operator(CeCheckPassword) \
                                                                                                                                                                                                                                Func_Operator(CeGetFileTime) \
                                                                                                                                                                                                                                    Func_Operator(CeSetFileTime) \
                                                                                                                                                                                                                                        Func_Operator(CeGetVersionEx) \
                                                                                                                                                                                                                                            Func_Operator(CeGetWindow) \
                                                                                                                                                                                                                                                Func_Operator(CeGetWindowLong) \
                                                                                                                                                                                                                                                    Func_Operator(CeGetWindowText) \
                                                                                                                                                                                                                                                        Func_Operator(CeGetClassName) \
                                                                                                                                                                                                                                                            Func_Operator(CeGlobalMemoryStatus) \
                                                                                                                                                                                                                                                                Func_Operator(CeGetSystemPowerStatusEx) \
                                                                                                                                                                                                                                                                    Func_Operator(CeGetTempPath) \
                                                                                                                                                                                                                                                                        Func_Operator(CeGetSpecialFolderPath) \
                                                                                                                                                                                                                                                                            Func_Operator(CeFindFirstDatabaseEx) \
                                                                                                                                                                                                                                                                                Func_Operator(CeFindNextDatabaseEx) \
                                                                                                                                                                                                                                                                                    Func_Operator(CeCreateDatabaseEx) \
                                                                                                                                                                                                                                                                                        Func_Operator(CeSetDatabaseInfoEx) \
                                                                                                                                                                                                                                                                                            Func_Operator(CeOpenDatabaseEx) \
                                                                                                                                                                                                                                                                                                Func_Operator(CeDeleteDatabaseEx) \
                                                                                                                                                                                                                                                                                                    Func_Operator(CeReadRecordPropsEx) \
                                                                                                                                                                                                                                                                                                        Func_Operator(CeMountDBVol) \
                                                                                                                                                                                                                                                                                                            Func_Operator(CeUnmountDBVol) \
                                                                                                                                                                                                                                                                                                                Func_Operator(CeFlushDBVol) \
                                                                                                                                                                                                                                                                                                                    Func_Operator(CeEnumDBVolumes) \
                                                                                                                                                                                                                                                                                                                        Func_Operator(CeOidGetInfoEx)

class CDynRapi
{
    static HINSTANCE m_hLib;

public:
    static BOOL Load()
    {
        if (m_hLib != NULL)
            return TRUE;

        // nacteme knihovnu
        UINT fuError;
        fuError = SetErrorMode(SEM_NOOPENFILEERRORBOX);
        m_hLib = LoadLibrary("rapi.dll");
        SetErrorMode(fuError);

        if (m_hLib == NULL)
        {
            TRACE_E("CDynRapi::Load() Failed to load rapi.dll");
            return FALSE;
        }

// napojime ukazatele na jeji exporty
#define Func_Operator(Name) Name = (RapiNS::Name*)GetProcAddress(m_hLib, #Name);
        RAPI_FUNCTIONS
#undef Func_Operator

        if (!IsGood()) // A vse zkontrolujeme
        {
            Unload();
            return FALSE;
        }

        return TRUE;
    }

    static void Unload()
    {
        if (m_hLib == NULL)
            return;

#define Func_Operator(Name) Name = NULL;
        RAPI_FUNCTIONS
#undef Func_Operator

        FreeLibrary(m_hLib);
        m_hLib = NULL;
    }

    static BOOL IsGood()
    {
        // knihovna musi byt nactena
        if (m_hLib == NULL)
            return FALSE;

// vsechny exporty musi byt nalinkovane
#define Func_Operator(Name) \
    if (Name == NULL) \
    { \
        TRACE_E("Export " << #Name << " was not found in the Rapi.dll"); \
        return FALSE; \
    }
        RAPI_FUNCTIONS
#undef Func_Operator

        return TRUE;
    }

#define Func_Operator(Name) static RapiNS::Name* Name;
    RAPI_FUNCTIONS
#undef Func_Operator
};

#define Func_Operator(Name) RapiNS::Name* CDynRapi::Name = NULL;
RAPI_FUNCTIONS
#undef Func_Operator

HINSTANCE CDynRapi::m_hLib = NULL;

CDynRapi dynRapi;

/////////////////////////////////////////////////////////////////////////////
// CRAPI class

BOOL CRAPI::initialized = FALSE;

BOOL CRAPI::Init()
{
    if (initialized)
        return TRUE;

    if (CDynRapi::Load())
        ReInit();

    if (!initialized)
    {
        TRACE_E("CRAPI::Init() Failed");

        CDynRapi::Unload();

        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_CONNECTION), TitleWMobileError, MSGBOX_ERROR);
    }

    return initialized;
}

void CRAPI::UnInit()
{
    if (initialized)
    {
        CDynRapi::CeRapiUninit();
        initialized = FALSE;
    }
    CDynRapi::Unload();
}

BOOL CRAPI::ReInit()
{
    if (initialized)
    {
        CDynRapi::CeRapiUninit();
        initialized = FALSE;

        CPluginFSInterface::EmptyCache(); //Je mozne, ze doslo k vymene zarizeni a proto cache vyprázdníme
    }

    //JR REVIEW: please wait + cancel dialog?
    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_CONNECTING), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());

    //  Sleep(500); //JR REVIEW: Proc to tady je?

    HANDLE hExit = CreateEvent(NULL, FALSE, FALSE, NULL);
    HRESULT hRapiResult = InitRapi(hExit, 3000);
    CloseHandle(hExit);

    if (SUCCEEDED(hRapiResult))
        initialized = TRUE;

    SalamanderGeneral->DestroySafeWaitWindow();

    return initialized;
}

/////////////////////////////////////////////////////////////////////////////
//RAPI

BOOL CRAPI::FindAllFiles(LPCTSTR szPath, DWORD dwFlags, LPDWORD lpdwFoundCount,
                         RapiNS::LPLPCE_FIND_DATA ppFindDataArray, BOOL tryReinit)
{
    OLECHAR olePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, szPath, -1, olePath, MAX_PATH - 1);
    olePath[MAX_PATH - 1] = 0;

    BOOL ret = CDynRapi::CeFindAllFiles(olePath, dwFlags, lpdwFoundCount, ppFindDataArray);
    if (!ret && tryReinit && GetLastError() != ERROR_NO_MORE_FILES && ReInit())
        ret = CDynRapi::CeFindAllFiles(olePath, dwFlags, lpdwFoundCount, ppFindDataArray);

    return ret;
}

HANDLE
CRAPI::FindFirstFile(LPCTSTR lpFileName, RapiNS::LPCE_FIND_DATA lpFindFileData, BOOL tryReinit)
{
    OLECHAR oleFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, oleFileName, MAX_PATH - 1);
    oleFileName[MAX_PATH - 1] = 0;

    HANDLE hFindFile = CDynRapi::CeFindFirstFile(oleFileName, lpFindFileData);
    if (hFindFile == INVALID_HANDLE_VALUE && tryReinit && GetLastError() != ERROR_NO_MORE_FILES && ReInit())
        hFindFile = CDynRapi::CeFindFirstFile(oleFileName, lpFindFileData);

    return hFindFile;
}

BOOL CRAPI::FindNextFile(HANDLE hFindFile, RapiNS::LPCE_FIND_DATA lpFindFileData)
{
    return CDynRapi::CeFindNextFile(hFindFile, lpFindFileData);
}

BOOL CRAPI::FindClose(HANDLE hFindFile)
{
    return CDynRapi::CeFindClose(hFindFile);
}

DWORD
CRAPI::GetFileAttributes(LPCTSTR lpFileName, BOOL tryReinit)
{
    OLECHAR oleFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, oleFileName, MAX_PATH - 1);
    oleFileName[MAX_PATH - 1] = 0;

    DWORD attr = CDynRapi::CeGetFileAttributes(oleFileName);
    if (attr == 0xFFFFFFFF && tryReinit && GetLastError() != ERROR_FILE_NOT_FOUND && ReInit())
        attr = CDynRapi::CeGetFileAttributes(oleFileName);

    return attr;
}

BOOL CRAPI::SetFileAttributes(LPCTSTR lpFileName, DWORD dwFileAttributes)
{
    OLECHAR oleFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, oleFileName, MAX_PATH - 1);
    oleFileName[MAX_PATH - 1] = 0;

    return CDynRapi::CeSetFileAttributes(oleFileName, dwFileAttributes);
}

BOOL CRAPI::GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
{
    return CDynRapi::CeGetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

BOOL CRAPI::SetFileTime(HANDLE hFile, FILETIME* lpCreationTime, FILETIME* lpLastAccessTime, FILETIME* lpLastWriteTime)
{
    return CDynRapi::CeSetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

DWORD
CRAPI::GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
    return CDynRapi::CeGetFileSize(hFile, lpFileSizeHigh);
}

BOOL CRAPI::GetStoreInformation(RapiNS::LPSTORE_INFORMATION lpsi)
{
    return CDynRapi::CeGetStoreInformation(lpsi);
}

HANDLE
CRAPI::CreateFile(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    OLECHAR oleFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, oleFileName, MAX_PATH - 1);
    oleFileName[MAX_PATH - 1] = 0;

    return CDynRapi::CeCreateFile(oleFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL CRAPI::CloseHandle(HANDLE hObject)
{
    return CDynRapi::CeCloseHandle(hObject);
}

BOOL CRAPI::ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    return CDynRapi::CeReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL CRAPI::WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
    return CDynRapi::CeWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

BOOL CRAPI::CopyFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists)
{
    OLECHAR oleExistingFileName[MAX_PATH], oleNewFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpExistingFileName, -1, oleExistingFileName, MAX_PATH - 1);
    oleExistingFileName[MAX_PATH - 1] = 0;
    MultiByteToWideChar(CP_ACP, 0, lpNewFileName, -1, oleNewFileName, MAX_PATH - 1);
    oleNewFileName[MAX_PATH - 1] = 0;

    return CDynRapi::CeCopyFile(oleExistingFileName, oleNewFileName, bFailIfExists);
}

BOOL CRAPI::MoveFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName)
{
    OLECHAR oleExistingFileName[MAX_PATH], oleNewFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpExistingFileName, -1, oleExistingFileName, MAX_PATH - 1);
    oleExistingFileName[MAX_PATH - 1] = 0;
    MultiByteToWideChar(CP_ACP, 0, lpNewFileName, -1, oleNewFileName, MAX_PATH - 1);
    oleNewFileName[MAX_PATH - 1] = 0;

    return CDynRapi::CeMoveFile(oleExistingFileName, oleNewFileName);
}

BOOL CRAPI::DeleteFile(LPCTSTR lpFileName)
{
    OLECHAR oleFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, oleFileName, MAX_PATH - 1);
    oleFileName[MAX_PATH - 1] = 0;

    return CDynRapi::CeDeleteFile(oleFileName);
}

BOOL CRAPI::CreateDirectory(LPCTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    OLECHAR olePathName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpPathName, -1, olePathName, MAX_PATH - 1);
    olePathName[MAX_PATH - 1] = 0;

    return CDynRapi::CeCreateDirectory(olePathName, lpSecurityAttributes);
}

BOOL CRAPI::RemoveDirectory(LPCTSTR lpPathName)
{
    OLECHAR olePathName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpPathName, -1, olePathName, MAX_PATH - 1);
    olePathName[MAX_PATH - 1] = 0;

    return CDynRapi::CeRemoveDirectory(olePathName);
}

BOOL CRAPI::CreateProcess(LPCTSTR lpApplicationName, LPCTSTR lpCommandLine)
{
    OLECHAR oleApplicationName[MAX_PATH], oleCommandLine[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpApplicationName, -1, oleApplicationName, MAX_PATH - 1);
    oleApplicationName[MAX_PATH - 1] = 0;
    if (lpCommandLine)
    {
        MultiByteToWideChar(CP_ACP, 0, lpCommandLine, -1, oleCommandLine, MAX_PATH - 1);
        oleCommandLine[MAX_PATH - 1] = 0;
    }

    return CDynRapi::CeCreateProcess(oleApplicationName, lpCommandLine ? oleCommandLine : NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, NULL);
}

BOOL CRAPI::SHGetShortcutTarget(LPCTSTR lpszShortcut, LPTSTR lpszTarget, int cbMax)
{
    OLECHAR oleShortcut[MAX_PATH], oleTarget[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpszShortcut, -1, oleShortcut, MAX_PATH - 1);
    oleShortcut[MAX_PATH - 1] = 0;

    if (!CDynRapi::CeSHGetShortcutTarget(oleShortcut, oleTarget, cbMax))
        return FALSE;

    WideCharToMultiByte(CP_ACP, 0, oleTarget, -1, lpszTarget, cbMax, NULL, NULL);
    if (cbMax > 0)
        lpszTarget[cbMax - 1] = 0;
    return TRUE;
}

HRESULT
CRAPI::FreeBuffer(LPVOID Buffer)
{
    return CDynRapi::CeRapiFreeBuffer(Buffer);
}

DWORD
CRAPI::GetLastError(void)
{
    DWORD err = CDynRapi::CeGetLastError();
    if (err == 0)
        err = CDynRapi::CeRapiGetError();

    return err;
}

HRESULT
CRAPI::RapiGetError(void)
{
    return CDynRapi::CeRapiGetError();
}

/////////////////////////////////////////////////////////////////////////////
// Helpers

//SalPathAppend nefunguje uplne podle nasich potreb
//SalPathAppend("\\", "dir") vraci "dir", moje verze vrati "\dir"
BOOL CRAPI::PathAppend(char* path, const char* name, int pathSize)
{
    if (path == NULL || name == NULL)
    {
        TRACE_E("CRAPI::PathAppend()");
        return FALSE;
    }
    if (*name == '\\')
        name++;
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] != '\\')
    {
        if (l >= pathSize)
            return FALSE;

        path[l++] = '\\';
    }
    if (*name != 0)
    {
        int n = (int)strlen(name);
        if (l + 1 + n < pathSize) // vejdeme se i s nulou na konci?
            memcpy(path + l, name, n + 1);
        else
            return FALSE;
    }
    else
        path[l] = 0;

    return TRUE;
}

BOOL CRAPI::FindAllFilesInTree(const char* rootPath, const char* fileName, CFileInfoArray& array, int block, BOOL dirFirst)
{
    char path[MAX_PATH];
    path[0] = 0;

    return FindAllFilesInTree(rootPath, path, fileName, array, block, dirFirst);
}

/////////////////////////////////////////////////////////////////////////////
//Implementation

DWORD
CRAPI::WaitAndDispatch(DWORD nCount, HANDLE* phWait, DWORD dwTimeout, BOOL bOnlySendMessage)
{
    DWORD dwObj;
    DWORD dwStart = GetTickCount();
    DWORD dwTimeLeft = dwTimeout;

    for (;;)
    {
        dwObj = MsgWaitForMultipleObjects(nCount, phWait, FALSE, dwTimeLeft,
                                          (bOnlySendMessage) ? QS_SENDMESSAGE : QS_ALLINPUT);

        if (dwObj == (DWORD)-1)
        {
            dwObj = WaitForMultipleObjects(nCount, phWait, FALSE, 100);

            if (dwObj == (DWORD)-1)
                break;
        }
        else
        {
            if (dwObj == WAIT_TIMEOUT)
                break;
        }

        if ((UINT)(dwObj - WAIT_OBJECT_0) < nCount)
            break;

        MSG msg;

        if (bOnlySendMessage)
            PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
        else
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                DispatchMessage(&msg);
        }

        if (INFINITE != dwTimeout)
        {
            dwTimeLeft = dwTimeout - (GetTickCount() - dwStart);
            if ((int)dwTimeLeft < 0)
                break;
        }
    }

    return dwObj;
}

HRESULT
CRAPI::InitRapi(HANDLE hExit, DWORD dwTimeout)
{
    RapiNS::RAPIINIT rapiinit = {sizeof(rapiinit)};
    HRESULT hResult = CDynRapi::CeRapiInitEx(&rapiinit);

    if (FAILED(hResult))
        return hResult;

    HANDLE hWait[2] = {hExit, rapiinit.heRapiInit};
    enum
    {
        WAIT_EXIT = WAIT_OBJECT_0,
        WAIT_INIT
    };

    DWORD dwObj = WaitAndDispatch(2, hWait, dwTimeout, TRUE);

    // Event signaled by RAPI
    if (WAIT_INIT == dwObj)
    {
        // If the connection failed, uninitialize the
        // Windows CE RAPI.
        if (FAILED(rapiinit.hrRapiInit))
            CDynRapi::CeRapiUninit();

        return rapiinit.hrRapiInit;
    }

    // Either event signaled by user or a time-out occurred.
    CDynRapi::CeRapiUninit();

    if (WAIT_EXIT == dwObj)
        return HRESULT_FROM_WIN32(ERROR_CANCELLED);

    return E_FAIL;
}

BOOL CRAPI::FindAllFilesInTree(LPCTSTR rootPath, char (&path)[MAX_PATH], LPCTSTR fileName, CFileInfoArray& array, int block, BOOL dirFirst)
{
    HANDLE find = INVALID_HANDLE_VALUE;

    RapiNS::CE_FIND_DATA data;
    char fullPath[MAX_PATH];
    strcpy(fullPath, rootPath);
    if (!PathAppend(fullPath, path, MAX_PATH) || !PathAppend(fullPath, fileName, MAX_PATH))
        goto ONERROR_TOOLONG;

    find = FindFirstFile(fullPath, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        //JR Nektere storage vraceji ERROR_FILE_NOT_FOUND místo ERROR_NO_MORE_FILES
        int nError = CDynRapi::CeGetLastError();
        if (nError == ERROR_NO_MORE_FILES || nError == ERROR_FILE_NOT_FOUND)
            return TRUE; //JR prazdny adresar, koncim

        char buf[2 * MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf(buf, LoadStr(IDS_PATH_ERROR), fullPath, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
        return FALSE;
    }

    for (;;)
    {
        //JR TODO: Toto nefunguje!
        if (SalamanderGeneral->GetSafeWaitWindowClosePressed())
        {
            if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_YESNO_CANCEL), TitleWMobileQuestion,
                                                  MSGBOX_QUESTION) == IDYES)
                goto ONERROR;
        }

        if (data.cFileName[0] != 0 &&
            (data.cFileName[0] != '.' || //JR Windows Mobile nevraci "." a ".." cesty, ale pro jistotu to osetrim
             (data.cFileName[1] != 0 && (data.cFileName[1] != '.' || data.cFileName[2] != 0))))
        {
            char cFileName[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, data.cFileName, -1, cFileName, MAX_PATH, NULL, NULL);
            cFileName[MAX_PATH - 1] = 0;

            CFileInfo fi;
            strcpy(fi.cFileName, path);
            if (!PathAppend(fi.cFileName, cFileName, MAX_PATH))
                goto ONERROR_TOOLONG;

            fi.dwFileAttributes = data.dwFileAttributes;

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                fi.size = 0;

                if (dirFirst)
                {
                    fi.block = -1;
                    array.Add(fi);
                    if (array.State != etNone)
                    {
                        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                        TRACE_E("Low memory");
                        goto ONERROR;
                    }
                }

                int len = (int)strlen(path);
                if (!PathAppend(path, cFileName, MAX_PATH))
                    goto ONERROR_TOOLONG;

                if (!FindAllFilesInTree(rootPath, path, "*", array, block, dirFirst))
                    goto ONERROR; //JR Chyba uz byla hlasena

                path[len] = 0;
            }
            else
                fi.size = data.nFileSizeLow;

            fi.block = block;
            array.Add(fi);
            if (array.State != etNone)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                TRACE_E("Low memory");
                goto ONERROR;
            }
        }

        if (!FindNextFile(find, &data))
        {
            if (CDynRapi::CeGetLastError() == ERROR_NO_MORE_FILES)
                break; //JR Vse v poradku, koncim

            DWORD err = GetLastError();
            SalamanderGeneral->ShowMessageBox(SalamanderGeneral->GetErrorText(err), TitleWMobileError, MSGBOX_ERROR);
            FindClose(find);
            return FALSE;
        }
    }

    FindClose(find);
    return TRUE;

ONERROR_TOOLONG:
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                      TitleWMobileError, MSGBOX_ERROR);
ONERROR:
    if (find != INVALID_HANDLE_VALUE)
        FindClose(find);
    return FALSE;
}

DWORD
CRAPI::CopyFileToPC(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists, CProgressDlg* dlg, INT64 totalCopied, INT64 totalSize, LPCTSTR* errorFileName)
{
    DWORD err = 0;

    HANDLE srcHandle = INVALID_HANDLE_VALUE, dstHandle = INVALID_HANDLE_VALUE;
    FILETIME creationTime, accessedTime, writeTime;
    DWORD size, copied = 0;

    DWORD attr = GetFileAttributes(lpExistingFileName);
    if (attr == 0xFFFFFFFF)
        goto ONERROR_SRC;

    srcHandle = CreateFile(lpExistingFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (srcHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_SRC;

    size = GetFileSize(srcHandle, NULL); //JR REVIEW: Na Windows Mobile asi nebudou soubory > 4 GB
    if (size == 0xFFFFFFFF)
        goto ONERROR_SRC;

    if (totalSize < totalCopied + size)
        totalSize = totalCopied + size;

    if (!GetFileTime(srcHandle, &creationTime, &accessedTime, &writeTime))
        goto ONERROR_SRC;

    dstHandle = ::CreateFile(lpNewFileName, GENERIC_WRITE, 0, NULL, bFailIfExists ? CREATE_NEW : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dstHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_DST;

    DWORD read, written;

    char buffer[COPY_BUFFER_SIZE];

    do
    {
        if (!CDynRapi::CeReadFile(srcHandle, buffer, sizeof(buffer), &read, NULL))
            goto ONERROR_SRC;

        if (!::WriteFile(dstHandle, &buffer, read, &written, NULL))
            goto ONERROR_DST;

        copied += read;
        totalCopied += read;

        if (dlg != NULL)
        {
            float progress = (size ? ((float)copied / (float)size) : 1) * 1000;
            float progressTotal = (totalSize ? ((float)totalCopied / (float)totalSize) : 1) * 1000;
            dlg->SetProgress((DWORD)progressTotal, (DWORD)progress, TRUE);

            if (dlg->GetWantCancel())
            {
                err = -1;
                ::CloseHandle(dstHandle);
                dstHandle = INVALID_HANDLE_VALUE;
                ::DeleteFile(lpNewFileName);
                goto RETURN;
            }
        }
    } while (read >= sizeof(buffer));

    ::SetFileTime(dstHandle, &creationTime, &accessedTime, &writeTime); //JR REVIEW: ignorovat pripadne chyby?
    ::SetFileAttributes(lpNewFileName, attr);

RETURN:
    if (srcHandle != INVALID_HANDLE_VALUE)
        CloseHandle(srcHandle);
    if (dstHandle != INVALID_HANDLE_VALUE)
        ::CloseHandle(dstHandle);

    return err;

ONERROR_SRC:
    err = GetLastError();
    if (!err)
        err = E_FAIL; //pro jistotu
    if (errorFileName)
        *errorFileName = lpExistingFileName;
    goto RETURN;

ONERROR_DST:
    err = ::GetLastError();
    if (!err)
        err = E_FAIL; //pro jistotu
    if (errorFileName)
        *errorFileName = lpNewFileName;
    goto RETURN;
}

DWORD
CRAPI::CopyFileToCE(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists, CProgressDlg* dlg, INT64 totalCopied, INT64 totalSize, LPCTSTR* errorFileName)
{
    DWORD err = 0;

    HANDLE dstHandle = INVALID_HANDLE_VALUE, srcHandle = INVALID_HANDLE_VALUE;
    FILETIME creationTime, accessedTime, writeTime;
    DWORD size, copied = 0;

    DWORD attr = SalamanderGeneral->SalGetFileAttributes(lpExistingFileName);
    if (attr == 0xFFFFFFFF)
        goto ONERROR_SRC;

    srcHandle = ::CreateFile(lpExistingFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (srcHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_SRC;

    size = ::GetFileSize(srcHandle, NULL); //JR REVIEW: Na Windows Mobile asi nebudou soubory > 4 GB
    if (size == 0xFFFFFFFF)
        goto ONERROR_SRC;

    if (totalSize < totalCopied + size)
        totalSize = totalCopied + size;

    if (!::GetFileTime(srcHandle, &creationTime, &accessedTime, &writeTime))
        goto ONERROR_SRC;

    dstHandle = CreateFile(lpNewFileName, GENERIC_WRITE, 0, NULL, bFailIfExists ? CREATE_NEW : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dstHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_DST;

    DWORD read, written;

    char buffer[COPY_BUFFER_SIZE];

    do
    {
        if (!::ReadFile(srcHandle, buffer, sizeof(buffer), &read, NULL))
            goto ONERROR_SRC;

        if (!WriteFile(dstHandle, &buffer, read, &written, NULL))
            goto ONERROR_DST;

        copied += read;
        totalCopied += read;

        if (dlg != NULL)
        {
            float progress = (size ? ((float)copied / (float)size) : 1) * 1000;
            float progressTotal = (totalSize ? ((float)totalCopied / (float)totalSize) : 1) * 1000;
            dlg->SetProgress((DWORD)progressTotal, (DWORD)progress, TRUE);

            if (dlg->GetWantCancel())
            {
                err = -1;
                CloseHandle(dstHandle);
                dstHandle = INVALID_HANDLE_VALUE;
                DeleteFile(lpNewFileName);
                goto RETURN;
            }
        }
    } while (read >= sizeof(buffer));

    SetFileTime(dstHandle, &creationTime, &accessedTime, &writeTime); //JR REVIEW: ignorovat pripadne chyby?
    SetFileAttributes(lpNewFileName, attr);

RETURN:
    if (dstHandle != INVALID_HANDLE_VALUE)
        CloseHandle(dstHandle);
    if (srcHandle != INVALID_HANDLE_VALUE)
        ::CloseHandle(srcHandle);

    if (err == 0)
        SetFileAttributes(lpNewFileName, attr);

    return err;

ONERROR_SRC:
    err = ::GetLastError();
    if (!err)
        err = E_FAIL; //pro jistotu
    if (errorFileName)
        *errorFileName = lpExistingFileName;
    goto RETURN;

ONERROR_DST:
    err = GetLastError();
    if (!err)
        err = E_FAIL; //pro jistotu
    if (errorFileName)
        *errorFileName = lpNewFileName;
    goto RETURN;
}

DWORD
CRAPI::CopyFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists, CProgressDlg* dlg, INT64 totalCopied, INT64 totalSize, LPCTSTR* errorFileName)
{
    DWORD err = 0;

    HANDLE dstHandle = INVALID_HANDLE_VALUE, srcHandle = INVALID_HANDLE_VALUE;
    FILETIME creationTime, accessedTime, writeTime;
    DWORD size, copied = 0;

    DWORD attr = GetFileAttributes(lpExistingFileName);
    if (attr == 0xFFFFFFFF)
        goto ONERROR_SRC;

    srcHandle = CreateFile(lpExistingFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (srcHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_SRC;

    size = GetFileSize(srcHandle, NULL); //JR REVIEW: Na Windows Mobile asi nebudou soubory > 4 GB
    if (size == 0xFFFFFFFF)
        goto ONERROR_SRC;

    if (totalSize < totalCopied + size)
        totalSize = totalCopied + size;

    if (!GetFileTime(srcHandle, &creationTime, &accessedTime, &writeTime))
        goto ONERROR_SRC;

    dstHandle = CreateFile(lpNewFileName, GENERIC_WRITE, 0, NULL, bFailIfExists ? CREATE_NEW : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dstHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_DST;

    DWORD read, written;

    char buffer[COPY_BUFFER_SIZE];

    do
    {
        if (!ReadFile(srcHandle, buffer, sizeof(buffer), &read, NULL))
            goto ONERROR_SRC;

        if (!WriteFile(dstHandle, &buffer, read, &written, NULL))
            goto ONERROR_DST;

        copied += read;
        totalCopied += read;

        if (dlg != NULL)
        {
            float progress = (size ? ((float)copied / (float)size) : 1) * 1000;
            float progressTotal = (totalSize ? ((float)totalCopied / (float)totalSize) : 1) * 1000;
            dlg->SetProgress((DWORD)progressTotal, (DWORD)progress, TRUE);

            if (dlg->GetWantCancel())
            {
                err = -1;
                CloseHandle(dstHandle);
                dstHandle = INVALID_HANDLE_VALUE;
                DeleteFile(lpNewFileName);
                goto RETURN;
            }
        }
    } while (read >= sizeof(buffer));

    SetFileTime(dstHandle, &creationTime, &accessedTime, &writeTime); //JR REVIEW: ignorovat pripadne chyby?
    SetFileAttributes(lpNewFileName, attr);

RETURN:
    if (dstHandle != INVALID_HANDLE_VALUE)
        CloseHandle(dstHandle);
    if (srcHandle != INVALID_HANDLE_VALUE)
        CloseHandle(srcHandle);

    if (err == 0)
        SetFileAttributes(lpNewFileName, attr);

    return err;

ONERROR_SRC:
    err = GetLastError();
    if (!err)
        err = E_FAIL; //pro jistotu
    if (errorFileName)
        *errorFileName = lpExistingFileName;
    goto RETURN;

ONERROR_DST:
    err = GetLastError();
    if (!err)
        err = E_FAIL; //pro jistotu
    if (errorFileName)
        *errorFileName = lpNewFileName;
    goto RETURN;
}

DWORD
CRAPI::SetFileTime(LPCTSTR lpFileName, const SYSTEMTIME* lpCreationTime,
                   const SYSTEMTIME* lpLastAccessTime, const SYSTEMTIME* lpLastWriteTime)
{
    if (lpCreationTime == NULL && lpLastAccessTime == NULL && lpLastWriteTime == NULL)
        return 0;

    HANDLE ceHandle = INVALID_HANDLE_VALUE;

    DWORD attr = GetFileAttributes(lpFileName);
    if (attr == 0xFFFFFFFF)
        goto ONERROR_CE;

    if (attr & FILE_ATTRIBUTE_READONLY)
    {
        if (!SetFileAttributes(lpFileName, attr & (~FILE_ATTRIBUTE_READONLY)))
            goto ONERROR_CE;
    }

    ceHandle = CreateFile(lpFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (ceHandle == INVALID_HANDLE_VALUE)
        goto ONERROR_CE;

    FILETIME time, timeCreated, timeAccessed, timeModified;
    if (lpCreationTime)
    {
        SystemTimeToFileTime(lpCreationTime, &time);
        LocalFileTimeToFileTime(&time, &timeCreated);
    }
    if (lpLastAccessTime)
    {
        SystemTimeToFileTime(lpLastAccessTime, &time);
        LocalFileTimeToFileTime(&time, &timeAccessed);
    }
    if (lpLastWriteTime)
    {
        SystemTimeToFileTime(lpLastWriteTime, &time);
        LocalFileTimeToFileTime(&time, &timeModified);
    }

    if (!SetFileTime(ceHandle, lpCreationTime ? &timeCreated : NULL,
                     lpLastAccessTime ? &timeAccessed : NULL, lpLastWriteTime ? &timeModified : NULL))
        goto ONERROR_CE;

    CloseHandle(ceHandle);
    if (attr & FILE_ATTRIBUTE_READONLY)
        SetFileAttributes(lpFileName, attr);

    return 0;

ONERROR_CE:
    DWORD err = GetLastError();

    if (ceHandle != INVALID_HANDLE_VALUE)
        CloseHandle(ceHandle);
    if (attr & FILE_ATTRIBUTE_READONLY)
        SetFileAttributes(lpFileName, attr);

    return err;
}

BOOL CRAPI::CheckAndCreateDirectory(const char* dir, HWND parent, BOOL quiet, char* errBuf,
                                    int errBufSize, char* newDir)
{
    CALL_STACK_MESSAGE2("CheckAndCreateDirectory(%s)", dir);

    DWORD attrs = GetFileAttributes(dir);
    char buf[MAX_PATH + 100];
    char name[MAX_PATH];
    if (newDir != NULL)
        newDir[0] = 0;
    //  if (parent == NULL) parent = MainWindow->HWindow;
    if (attrs == 0xFFFFFFFF) // asi neexistuje, umoznime jej vytvorit
    {
        char root[MAX_PATH] = "\\";      //GetRootPath(root, dir);
        if (strlen(dir) <= strlen(root)) // dir je root adresar
        {
            sprintf(buf, LoadStr(IDS_ERR_CREATEDIR), dir);
            if (errBuf != NULL)
            {
                int l = (int)strlen(buf);
                l = min(l + 1, errBufSize);
                memcpy(errBuf, buf, l);
                errBuf[errBufSize - 1] = 0;
            }
            else
                SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        sprintf(buf, LoadStr(IDS_YESNO_CREATEDIR), dir);
        if (quiet || SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileQuestion,
                                                      MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
        {
            strcpy(name, dir);
            char* s;
            while (1) // najdeme prvni existujici adresar
            {
                s = strrchr(name, '\\');
                if (s == NULL)
                {
                    sprintf(buf, LoadStr(IDS_ERR_CREATEDIR), dir);
                    if (errBuf != NULL)
                    {
                        int l = (int)strlen(buf);
                        l = min(l + 1, errBufSize);
                        memcpy(errBuf, buf, l);
                        errBuf[errBufSize - 1] = 0;
                    }
                    else
                        SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                    return FALSE;
                }
                if (s - name > (int)strlen(root))
                    *s = 0;
                else
                {
                    strcpy(name, root);
                    break; // uz jsme na root-adresari
                }
                attrs = GetFileAttributes(name);
                if (attrs != 0xFFFFFFFF) // jmeno existuje
                {
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        break; // budeme stavet od tohoto adresare
                    else       // je to soubor, to by neslo ...
                    {
                        sprintf(buf, LoadStr(IDS_ERR_DIRNAMEISFILE), name);
                        if (errBuf != NULL)
                        {
                            int l = (int)strlen(buf);
                            l = min(l + 1, errBufSize);
                            memcpy(errBuf, buf, l);
                            errBuf[errBufSize - 1] = 0;
                        }
                        else
                            SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                        return FALSE;
                    }
                }
            }
            s = name + strlen(name) - 1;
            if (*s != '\\')
            {
                *++s = '\\';
                *++s = 0;
            }
            const char* st = dir + strlen(name);
            if (*st == '\\')
                st++;
            int len = (int)strlen(name);
            BOOL first = TRUE;
            while (*st != 0)
            {
                const char* slash = strchr(st, '\\');
                if (slash == NULL)
                    slash = st + strlen(st);
                memcpy(name + len, st, slash - st);
                name[len += (int)(slash - st)] = 0;
                if (!CreateDirectory(name, NULL))
                {
                    sprintf(buf, LoadStr(IDS_ERR_CREATEDIR), name);
                    if (errBuf != NULL)
                    {
                        int l = (int)strlen(buf);
                        l = min(l + 1, errBufSize);
                        memcpy(errBuf, buf, l);
                        errBuf[errBufSize - 1] = 0;
                    }
                    else
                        SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                    return FALSE;
                }
                else
                {
                    if (first && newDir != NULL)
                        strcpy(newDir, name);
                    first = FALSE;
                }
                name[len++] = '\\';
                if (*slash == '\\')
                    slash++;
                st = slash;
            }
            return TRUE;
        }
        return FALSE;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return TRUE;
    else // soubor, to by neslo ...
    {
        sprintf(buf, LoadStr(IDS_ERR_DIRNAMEISFILE), dir);
        if (errBuf != NULL)
        {
            int l = (int)strlen(buf);
            l = min(l + 1, errBufSize);
            memcpy(errBuf, buf, l);
            errBuf[errBufSize - 1] = 0;
        }
        else
            SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

void CRAPI::GetFileData(const char* name, char (&buf)[100])
{
    buf[0] = '?';
    buf[1] = 0;
    char tmp[50];

    RapiNS::CE_FIND_DATA data;

    HANDLE find = FindFirstFile(name, &data);
    if (find == INVALID_HANDLE_VALUE)
        return;

    CQuadWord size(data.nFileSizeLow, data.nFileSizeHigh);
    SalamanderGeneral->NumberToStr(buf, size);

    FILETIME time;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&data.ftLastWriteTime, &time);
    FileTimeToSystemTime(&time, &st);

    if (!GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, tmp, 50))
        sprintf(tmp, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);

    strcat(buf, ", ");
    strcat(buf, tmp);

    if (!GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, tmp, 50))
        sprintf(tmp, "%u:%u:%u", st.wHour, st.wMinute, st.wSecond);

    strcat(buf, ", ");
    strcat(buf, tmp);

    FindClose(find);
}

BOOL CRAPI::CheckConnection()
{
    RapiNS::CEOSVERSIONINFO vi;
    ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);

    if (CDynRapi::CeGetVersionEx(&vi))
        return TRUE;

    return FALSE;
}
