// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <Shlobj.h>
#include <Shellapi.h>
#include <Sddl.h>

HINSTANCE HLanguage = NULL;
CSalmonSharedMemory* SalmonSharedMemory = NULL;

char BugReportPath[MAX_PATH] = {0};
extern TDirectArray<CBugReport> BugReports(1, 10);
char LatestBugReport[MAX_PATH] = {0}; // name of the most recent bug report (name only, without extension)
BOOL ReportOldBugs = TRUE;            // the user allowed uploading old reports as well

const char* APP_NAME = "Open Salamander Bug Reporter";

// ****************************************************************************

class C__StrCriticalSection
{
public:
    CRITICAL_SECTION cs;

    C__StrCriticalSection() { HANDLES(InitializeCriticalSection(&cs)); }
    ~C__StrCriticalSection() { HANDLES(DeleteCriticalSection(&cs)); }
};

// ensure the critical section is constructed in time
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
    // better ensure nobody calls us before the resource handle is initialized
    if (hInstance == NULL)
        TRACE_E("LoadStr: hInstance == NULL");
#endif // _DEBUG

RELOAD:
    int size = LoadString(hInstance, resID, act, 10000 - (int)(act - buffer));
    // size contains the number of copied characters without the terminator
    //  DWORD error = GetLastError();
    char* ret;
    if (size != 0) // error is NO_ERROR even when the string does not exist - unusable
    {
        if ((10000 - (act - buffer) == size + 1) && (act > buffer))
        {
            // if the string ended exactly at the end of the buffer, it might
            // have been truncated -- if we can shift the window to the beginning
            // of the buffer, load the string once more
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
        TRACE_E("Error in LoadStr(" << resID << ").");
        static char errorBuff[] = "ERROR LOADING STRING";
        ret = errorBuff;
    }

    HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

//*****************************************************************************
//
// GetErrorText
//
// this should reliably work for at least 10 errors being written at once,
// we do not expect more than 10 threads at the same time ;-)

char* GetErrorText(DWORD error)
{
    static char buffer[10 * MAX_PATH]; // buffer for many strings
    static char* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection2.cs));

    if (10 * MAX_PATH - (act - buffer) < MAX_PATH + 20)
        act = buffer;

    char* ret = act;
    // NOTE: sprintf_s in the debug build fills the entire buffer, so we cannot pass it the whole buffer (it
    // also contains other strings); handle it via _CrtSetDebugFillThreshold or by specifying a smaller size
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

//*****************************************************************************
//
// GetItemIdListForFileName
// OpenFolder
//

LPITEMIDLIST
GetItemIdListForFileName(LPSHELLFOLDER folder, const char* fileName,
                         BOOL addUNCPrefix = FALSE, BOOL useEnumForPIDLs = FALSE,
                         const char* enumNamePrefix = NULL)
{
    OLECHAR olePath[MAX_PATH];
    if (addUNCPrefix)
        olePath[0] = olePath[1] = L'\\';
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fileName, -1, olePath + (addUNCPrefix ? 2 : 0),
                        MAX_PATH - (addUNCPrefix ? 2 : 0));
    olePath[MAX_PATH - 1] = 0;

    LPITEMIDLIST pidl;
    ULONG chEaten;
    HRESULT ret;
    if (SUCCEEDED((ret = folder->ParseDisplayName(NULL, NULL, olePath, &chEaten,
                                                  &pidl, NULL))))
    {
        return pidl;
    }
    else
    {
        TRACE_E("ParseDisplayName error: 0x" << std::hex << ret << std::dec);
        return NULL;
    }
}

void OpenFolder(HWND hWnd, const char* szDir)
{
    LPITEMIDLIST pidl = NULL;
    LPSHELLFOLDER desktop;
    if (SUCCEEDED(SHGetDesktopFolder(&desktop)))
    {
        pidl = GetItemIdListForFileName(desktop, szDir);
        desktop->Release();
    }

    if (pidl != NULL)
    {
        SHELLEXECUTEINFO se;
        memset(&se, 0, sizeof(SHELLEXECUTEINFO));
        se.cbSize = sizeof(SHELLEXECUTEINFO);
        se.fMask = SEE_MASK_IDLIST;
        se.lpVerb = "explore"; // option whether to open with the tree view
        se.hwnd = hWnd;
        se.nShow = SW_SHOWNORMAL;
        se.lpIDList = pidl;
        ShellExecuteEx(&se);

        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (pidl != NULL && alloc->DidAlloc(pidl) == 1)
                alloc->Free(pidl);
            alloc->Release();
        }
    }
}

//*****************************************************************************
//
// SalGetFileAttributes
//

DWORD SalGetFileAttributes(const char* fileName)
{
    int fileNameLen = (int)strlen(fileName);
    char fileNameCopy[3 * MAX_PATH];
    // if the path ends with a space/dot, we must append '\\', otherwise GetFileAttributes trims
    // the spaces/dots and works with a different path; for files it does not work anyway,
    // but it is still better than getting attributes of another file/directory (for "c:\\file.txt   "
    // it works with the name "c:\\file.txt")
    if (fileNameLen > 0 && (fileName[fileNameLen - 1] <= ' ' || fileName[fileNameLen - 1] == '.') &&
        fileNameLen + 1 < _countof(fileNameCopy))
    {
        memcpy(fileNameCopy, fileName, fileNameLen);
        fileNameCopy[fileNameLen] = '\\';
        fileNameCopy[fileNameLen + 1] = 0;
        return GetFileAttributes(fileNameCopy);
    }
    else // a plain path, nothing special to do, just call the Windows GetFileAttributes
    {
        return GetFileAttributes(fileName);
    }
}

//*****************************************************************************
//
// DirExists
//

BOOL DirExists(const char* dirName)
{
    DWORD attr = SalGetFileAttributes(dirName);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

//*****************************************************************************
//
// ParseCommandLine
//

BOOL ParseCommandLine(const char* cmdLine, char* fileMappingName, char* slgName)
{
    const char* p1 = cmdLine;
    const char* p2;
    if (*p1 != '"')
        return FALSE;
    p1++;
    p2 = p1;
    while (*p2 != 0 && *p2 != '"')
        p2++;
    if (*p2 != '"')
        return FALSE;

    int paramLen = (int)(p2 - p1) + 1;
    if (paramLen >= SALMON_FILEMAPPIN_NAME_SIZE)
        return FALSE;

    lstrcpyn(fileMappingName, p1, paramLen);

    p1 = p2 + 1;
    if (*p1 != ' ')
        return FALSE;
    p1++;
    if (*p1 != '"')
        return FALSE;
    p1++;
    p2 = p1;
    while (*p2 != 0 && *p2 != '"')
        p2++;
    if (*p2 != '"')
        return FALSE;

    lstrcpyn(slgName, p1, (int)(p2 - p1) + 1);

    return TRUE;
}

//*****************************************************************************
//
// LoadSLG
//

const char* LANG_PATH = "lang\\%s";

HINSTANCE LoadSLG(const char* slgName)
{
    char path[MAX_PATH];
    sprintf(path, LANG_PATH, slgName);
    HINSTANCE hSLG = LoadLibrary(path);
    if (hSLG == NULL)
    {
        // if loading the SLG failed, it might not exist or we were not given a valid name
        // try to find another suitable one based on priority
        const char* masks[] = {"english.slg", "czech.slg", "german.slg", "spanish.slg", "*.slg", ""};
        for (int i = 0; *masks[i] != 0 && (hSLG == NULL); i++)
        {
            char findPath[MAX_PATH];
            sprintf(findPath, LANG_PATH, masks[i]);
            WIN32_FIND_DATA find;
            HANDLE hFind = HANDLES_Q(FindFirstFile(findPath, &find));
            if (hFind != INVALID_HANDLE_VALUE)
            {
                sprintf(path, LANG_PATH, find.cFileName);
                hSLG = LoadLibrary(path);
                HANDLES(FindClose(hFind));
            }
        }
    }
    if (hSLG == NULL)
        MessageBox(NULL, "Internal error: cannot load any language file. Please contact us at support@altap.cz.", APP_NAME, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
    return hSLG;
}

//------------------------------------------------------------------------------------------------
//
// RestartSalamander
//

BOOL RestartSalamander(HWND hParent)
{
    char path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    *(strrchr(path, '\\')) = '\0'; // strip \salamand.exe
    char* p = strrchr(path, '\\'); // strip \plugins
    if (p != NULL)
    {
        *p = 0;
        char initDir[MAX_PATH];
        strcpy(initDir, path);
        strcat(path, "\\salamand.exe");
        SHELLEXECUTEINFO se;
        memset(&se, 0, sizeof(SHELLEXECUTEINFO));
        se.cbSize = sizeof(SHELLEXECUTEINFO);
        se.nShow = SW_SHOWNORMAL;
        se.hwnd = hParent;
        se.lpDirectory = initDir;
        se.lpFile = path;
        return ShellExecuteEx(&se);
    }
    return FALSE;
}

//------------------------------------------------------------------------------------------------
//
// CleanBugReportsDirectory
//

BOOL CleanBugReportsDirectory(BOOL keep7ZipArchives)
{
    char path[MAX_PATH];
    strcpy(path, BugReportPath);
    if (strlen(path) == 0)
        return FALSE;
    if (*(path + strlen(path) - 1) != '\\')
        strcat(path, "\\");
    char* name = path + strlen(path);
    for (int i = 0; i < BugReports.Count; i++)
    {
        strcpy(name, BugReports[i].Name);
        strcat(name, ".*");
        WIN32_FIND_DATA find;
        HANDLE hFind = FindFirstFile(path, &find);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    if (find.cFileName[0] != 0 && strcmp(find.cFileName, ".") != 0 && strcmp(find.cFileName, "..") != 0)
                    {
                        BOOL skipDelete = FALSE;
                        if (keep7ZipArchives)
                        {
                            const char* ext = strrchr(find.cFileName, '.');
                            if (ext != NULL && _stricmp(ext, ".7z") == 0)
                                skipDelete = TRUE;
                        }
                        if (!skipDelete)
                        {
                            strcpy(name, find.cFileName);
                            DeleteFile(path);
                        }
                    }
                }
            } while (FindNextFile(hFind, &find));
            FindClose(hFind);
        }
    }
    return TRUE;
}

//------------------------------------------------------------------------------------------------
//
// GetBugReportNameIndexIgnoreExt()
//

// search the BugReports array (which contains names including extensions) and return the index of 'name'
// ignores extensions
int GetBugReportNameIndexIgnoreExt(const char* name)
{
    char strippedName[MAX_PATH];
    strcpy(strippedName, name);
    char* ext = strrchr(strippedName, '.');
    if (ext != NULL)
        *ext = 0;
    for (int i = 0; i < BugReports.Count; i++)
    {
        char strippedName2[MAX_PATH];
        strcpy(strippedName2, BugReports[i].Name);
        char* ext2 = strrchr(strippedName2, '.');
        if (ext2 != NULL)
            *ext2 = 0;
        if (_stricmp(strippedName2, strippedName) == 0)
            return i;
    }
    return -1;
}

//------------------------------------------------------------------------------------------------
//
// GetBugReportNames
//

BOOL GetBugReportNames()
{
    BugReports.DestroyMembers();

    if (BugReportPath[0] == 0)
        return FALSE;

    // if the directory does not exist, it cannot contain reports
    if (!DirExists(BugReportPath))
        return FALSE;

    // look for reports by extension
    char findPath[MAX_PATH];
    const char* extensions[] = {"*.DMP", "*.TXT", NULL};
    FILETIME latestFiletime;
    char latestFilename[MAX_PATH];
    latestFilename[0] = 0;
    for (int i = 0; extensions[i] != NULL; i++)
    {
        strcpy(findPath, BugReportPath);
        strcat(findPath, extensions[i]);
        WIN32_FIND_DATA find;
        HANDLE hFind = HANDLES_Q(FindFirstFile(findPath, &find));
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                CBugReport item;
                strcpy(item.Name, find.cFileName);
                char* ext = strrchr(item.Name, '.');

                BOOL skipFile = FALSE;
                if (_stricmp(ext + 1, "dmp") == 0)
                {
                    // delete minidumps over 200 MB because I do not believe they would pass to the server even after packing
                    if (find.nFileSizeHigh > 0 || find.nFileSizeLow > 200 * 1000 * 1024)
                    {
                        char deleteFileName[MAX_PATH];
                        strcpy(deleteFileName, BugReportPath);
                        strcat(deleteFileName, find.cFileName);
                        DeleteFile(deleteFileName);
                        skipFile = TRUE;
                    }
                }

                if (!skipFile)
                {
                    // store the newest name and timestamp
                    if (latestFilename[0] == 0 || CompareFileTime(&latestFiletime, &find.ftLastWriteTime) == -1)
                    {
                        strcpy(latestFilename, item.Name);
                        latestFiletime = find.ftLastWriteTime;
                    }

                    // add names that are missing from the list
                    if (GetBugReportNameIndexIgnoreExt(item.Name) == -1)
                        BugReports.Add(item);
                }
            } while (FindNextFile(hFind, &find));
            HANDLES(FindClose(hFind));
        }
    }

    // bubble the newest report to index zero
    if (BugReports.Count > 1)
    {
        int index = GetBugReportNameIndexIgnoreExt(latestFilename);
        if (index > 0)
        {
            CBugReport item = BugReports[index];
            BugReports.Delete(index);
            BugReports.Insert(0, item);
        }
    }

    // if these are crash dumps from WER, they lack the UID/version prefix - rename them
    char uid[17];
    sprintf(uid, "%I64X", SalmonSharedMemory->UID);
    for (int i = 0; i < BugReports.Count; i++)
    {
        CBugReport* item = &BugReports[i];
        // if the file does not start with our UID, someone else (WER) generated it and we rename the item
        if (_strnicmp(item->Name, uid, strlen(uid)) != 0)
        {
            char fullOrgName[MAX_PATH];
            char fullNewName[MAX_PATH];
            char newName[MAX_PATH];
            char tmpName[MAX_PATH];
            char extName[MAX_PATH];

            strcpy(fullOrgName, BugReportPath);
            strcat(fullOrgName, item->Name);
            char* ext = strrchr(item->Name, '.'); // move the extension to the end
            if (ext != NULL)
                strcpy(extName, ext);
            else
                extName[0] = 0;

            SYSTEMTIME lt;
            GetLocalTime(&lt);

            strcpy(tmpName, item->Name);
            if (ext != NULL)
                *strrchr(tmpName, '.') = 0; // cut it out inside the name
            strcat(tmpName, "-");
            strcat(tmpName, SalmonSharedMemory->BugName);
            GetReportBaseName(newName, sizeof(newName), BugReportPath, tmpName, SalmonSharedMemory->UID, lt);
            strcat(newName, extName);

            // new name in the array
            strcpy(item->Name, newName);

            // new name on disk
            strcpy(fullNewName, BugReportPath);
            strcat(fullNewName, newName);
            MoveFile(fullOrgName, fullNewName);
        }
    }

    // the extension is irrelevant, trim it off
    for (int i = 0; i < BugReports.Count; i++)
    {
        CBugReport* item = &BugReports[i];
        char* ext = strrchr(item->Name, '.');
        if (ext != NULL)
            *ext = 0;
    }

    return BugReports.Count > 0;
}

// ignore 'oversize' reports that end with -1 to -99
int GetUniqueBugReportCount()
{
    TDirectArray<CBugReport> UniqueNames(1, 10);
    char buff[MAX_PATH];
    for (int i = 0; i < BugReports.Count; i++)
    {
        CBugReport* item = &BugReports[i];
        //    MessageBox(NULL, item->Name, item->Name, MB_OK);
        // remove trailing -1 to -99 from the name
        strcpy(buff, item->Name);
        int len = (int)strlen(buff);
        if (len > 3)
        {
            if (buff[len - 2] == '-')
                buff[len - 2] = 0;
            if (buff[len - 3] == '-')
                buff[len - 3] = 0;
        }
        // if we do not find the item among unique names, add it there
        int j;
        for (j = 0; j < UniqueNames.Count; j++)
        {
            if (strcmp(UniqueNames[j].Name, buff) == 0)
                break;
        }
        if (j == UniqueNames.Count)
        {
            CBugReport item2;
            strcpy(item2.Name, buff);
            UniqueNames.Add(item2);
        }
    }
    return UniqueNames.Count;
}

//------------------------------------------------------------------------------------------------
//
// SaveDescriptionAndEmail
//

BOOL SaveDescriptionAndEmail()
{
    if (BugReportPath[0] == 0 || BugReports.Count == 0)
        return FALSE;

    BOOL ret = FALSE;
    char name[MAX_PATH];
    sprintf(name, "%s%s.INF", BugReportPath, BugReports[0].Name); // work with the newest name
    HANDLE hFile = CreateFile(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        ret = TRUE;
        DWORD written;
        char buff[1000];
        sprintf(buff, "Email: %s\r\n", Config.Email);
        ret &= (WriteFile(hFile, buff, (DWORD)strlen(buff), &written, NULL) && written == strlen(buff));
        if (ret)
            ret &= (WriteFile(hFile, Config.Description, (DWORD)strlen(Config.Description), &written, NULL) && written == strlen(Config.Description));
        CloseHandle(hFile);
    }
    return ret;
}

//------------------------------------------------------------------------------------------------
//
// LoadHLanguageVerbose
//

BOOL LoadHLanguageVerbose(const char* slgName)
{
    HINSTANCE hLanguage = LoadSLG(slgName);
    if (hLanguage == NULL)
    {
        char buff[2 * MAX_PATH];
        sprintf(buff, "Failed to load resources from %s.", slgName);
        MessageBox(NULL, buff, APP_NAME, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return FALSE;
    }
    if (HLanguage != NULL)
        FreeLibrary(HLanguage);
    HLanguage = hLanguage;
    return TRUE;
}

//------------------------------------------------------------------------------------------------
//
// SID utilities
//

SECURITY_ATTRIBUTES* CreateAccessableSecurityAttributes(SECURITY_ATTRIBUTES* sa, SECURITY_DESCRIPTOR* sd,
                                                        DWORD allowedAccessMask, PSID* psidEveryone, PACL* paclNewDacl)
{
    SID_IDENTIFIER_AUTHORITY siaWorld = SECURITY_WORLD_SID_AUTHORITY;
    int nAclSize;

    *psidEveryone = NULL;
    *paclNewDacl = NULL;

    // Create the everyone sid
    if (!AllocateAndInitializeSid(&siaWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AllocateAndInitializeSid() failed!");
        goto ErrorExit;
    }

    nAclSize = GetLengthSid(psidEveryone) * 2 + sizeof(ACCESS_ALLOWED_ACE) + sizeof(ACCESS_DENIED_ACE) + sizeof(ACL);
    *paclNewDacl = (PACL)LocalAlloc(LPTR, nAclSize);
    if (*paclNewDacl == NULL)
    {
        TRACE_E("CreateAccessableSecurityAttributes(): LocalAlloc() failed!");
        goto ErrorExit;
    }
    if (!InitializeAcl(*paclNewDacl, nAclSize, ACL_REVISION))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): InitializeAcl() failed!");
        goto ErrorExit;
    }
    if (!AddAccessDeniedAce(*paclNewDacl, ACL_REVISION, WRITE_DAC | WRITE_OWNER, *psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AddAccessDeniedAce() failed!");
        goto ErrorExit;
    }
    if (!AddAccessAllowedAce(*paclNewDacl, ACL_REVISION, allowedAccessMask, *psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AddAccessAllowedAce() failed!");
        goto ErrorExit;
    }
    if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): InitializeSecurityDescriptor() failed!");
        goto ErrorExit;
    }
    if (!SetSecurityDescriptorDacl(sd, TRUE, *paclNewDacl, FALSE))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): SetSecurityDescriptorDacl() failed!");
        goto ErrorExit;
    }
    sa->nLength = sizeof(SECURITY_ATTRIBUTES);
    sa->bInheritHandle = FALSE;
    sa->lpSecurityDescriptor = sd;
    return sa;

ErrorExit:
    if (*paclNewDacl != NULL)
    {
        LocalFree(*paclNewDacl);
        *paclNewDacl = NULL;
    }
    if (*psidEveryone != NULL)
    {
        FreeSid(*psidEveryone);
        *psidEveryone = NULL;
    }
    return NULL;
}

BOOL GetStringSid(LPTSTR* stringSid)
{
    *stringSid = NULL;

    HANDLE hToken = NULL;
    DWORD dwBufferSize = 0;
    PTOKEN_USER pTokenUser = NULL;

    // Open the access token associated with the calling process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TRACE_E("OpenProcessToken failed.");
        return FALSE;
    }

    // get the size of the memory buffer needed for the SID
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize);

    pTokenUser = (PTOKEN_USER)malloc(dwBufferSize);
    memset(pTokenUser, 0, dwBufferSize);

    // Retrieve the token information in a TOKEN_USER structure.
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
    {
        TRACE_E("GetTokenInformation failed.");
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    if (!IsValidSid(pTokenUser->User.Sid))
    {
        TRACE_E("The owner SID is invalid.\n");
        free(pTokenUser);
        return FALSE;
    }

    // the caller must release the returned memory with LocalFree, see MSDN
    ConvertSidToStringSid(pTokenUser->User.Sid, stringSid);

    free(pTokenUser);

    return TRUE;
}

#define SALMON_MAINDLG_MUTEX_NAME "AltapSalamanderSalmonMainDialog"

class CMainDialogMutex
{
protected:
    HANDLE Mutex;

public:
    CMainDialogMutex()
    {
        Mutex = NULL;
        Init();
    }

    ~CMainDialogMutex()
    {
        if (Mutex != NULL)
            HANDLES(CloseHandle(Mutex));
        Mutex = NULL;
    }

    void Init();

    BOOL Enter();
    void Leave();
};

CMainDialogMutex MainDialogMutex; // mutex ensuring that we show only one dialog per user (even on the server)

void CMainDialogMutex::Init()
{
    LPTSTR sid = NULL;
    if (!GetStringSid(&sid))
        sid = NULL;

    char buff[1000];
    if (sid == NULL)
    {
        // failed to obtain the SID -- fall back to a degraded mode
        _snprintf_s(buff, _TRUNCATE, "%s", SALMON_MAINDLG_MUTEX_NAME);
    }
    else
    {
        _snprintf_s(buff, _TRUNCATE, "Global\\%s_%s", SALMON_MAINDLG_MUTEX_NAME, sid);
        LocalFree(sid);
    }

    PSID psidEveryone;
    PACL paclNewDacl;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES* saPtr = CreateAccessableSecurityAttributes(&sa, &sd, SYNCHRONIZE /*| MUTEX_MODIFY_STATE*/, &psidEveryone, &paclNewDacl);

    Mutex = HANDLES_Q(CreateMutex(saPtr, FALSE, buff));
    if (Mutex == NULL)
    {
        Mutex = HANDLES_Q(OpenMutex(SYNCHRONIZE, FALSE, buff));
        if (Mutex == NULL)
        {
            DWORD err = GetLastError();
            TRACE_I("CreateMainDialogMutex(): Unable to create/open mutex for the main dialog window! Error: " << GetErrorText(err));
        }
    }

    if (psidEveryone != NULL)
        FreeSid(psidEveryone);
    if (paclNewDacl != NULL)
        LocalFree(paclNewDacl);
}

BOOL CMainDialogMutex::Enter()
{
    if (Mutex != NULL)
    {
        DWORD ret = WaitForSingleObject(Mutex, 0);
        if (ret == WAIT_FAILED)
            TRACE_E("CMainDialogMutex::Enter(): WaitForSingleObject() failed!");
        if (ret == WAIT_TIMEOUT)
            return FALSE; // another window is open, we cannot open ourselves
    }
    else
        TRACE_E("CMainDialogMutex::Enter(): the Mutex==NULL! Not initialized?");
    return TRUE;
}

void CMainDialogMutex::Leave()
{
    if (Mutex != NULL)
    {
        if (!ReleaseMutex(Mutex))
            TRACE_E("CMainDialogMutex::Enter(): ReleaseMutex() failed!");
    }
    else
        TRACE_E("CMainDialogMutex::Leave(): the Mutex==NULL! Not initialized?");
}

//------------------------------------------------------------------------------------------------
//
// OpenMainDialog()
//

BOOL OpenMainDialog(BOOL minidumpOnOpen)
{
    CMainDialog* mainDlg = new CMainDialog(HLanguage, IDD_SALMON_MAIN, minidumpOnOpen);
    if (mainDlg != NULL)
    {
        if (mainDlg->Create())
        {
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                if (msg.message != WM_TIMER)
                    AppIsBusy = TRUE;
                if (!IsDialogMessage(mainDlg->HWindow, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                AppIsBusy = FALSE;
            }
            DestroyWindow(mainDlg->HWindow);
        }
        mainDlg = NULL;
    }
    return TRUE;
}

//------------------------------------------------------------------------------------------------
//
// SetThreadNameInVCAndTrace
//

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

void ChechForBugs(CSalmonSharedMemory* mem, const char* slgName)
{
    if (DirExists(mem->BugPath))
    {
        if (MainDialogMutex.Enter())
        {
            if (GetBugReportNames())
            {
                // we need to display the GUI, we must load the SLG
                if (LoadHLanguageVerbose(slgName))
                {
                    if (GetUniqueBugReportCount() > 1)
                    {
                        // if multiple reports exist, ask whether to send them all
                        int res = MessageBox(NULL, LoadStr(IDS_SALMON_MORE_REPORTS, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND);
                        ReportOldBugs = (res == IDYES);
                    }
                    // with that decision made we can open the dialog
                    OpenMainDialog(FALSE);
                }
            }
            MainDialogMutex.Leave();
        }
    }

    ResetEvent(mem->CheckBugs);
    SetEvent(mem->Done); // let Salamander know we have taken over the SLG name
}

//------------------------------------------------------------------------------------------------
//
// WinMain
//

#define SALMON_RET_ERROR 0
#define SALMON_RET_OK 1

BOOL AppIsBusy = FALSE;

UINT PostponedMsg = 0;
WPARAM PostponedMsgWParam = 0;
LPARAM PostponedMsgLParam = 0;

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int cmdShow)
{
    // in 99% of cases when Salamander does not crash, salmon.exe will run unnoticed in the background and should
    // consume as little memory/CPU as possible; therefore delay loading the SLG until something needs to be shown (a Salamander crash)

    SetTraceProcessName("Salmon");
    SetThreadNameInVCAndTrace("Main");
    TRACE_I("Begin");

    // we do not want critical errors such as "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    HInstance = hInstance;

    Config.Load();

    char fileMappingName[SALMON_FILEMAPPIN_NAME_SIZE];
    char slgName[MAX_PATH] = {0}; // name of the SLG (e.g. "english.slg") to load into HLanguage; can be empty (then a default is loaded)

    if (!ParseCommandLine(cmdLine, fileMappingName, slgName) || strlen(fileMappingName) == 0)
    {
        HINSTANCE hLanguage = LoadSLG(slgName); // load the default SLG so that we can display possible errors
        if (hLanguage != NULL)
            MessageBox(NULL, LoadStr(IDS_SALMON_WRONG_CMDLINE, hLanguage), LoadStr(IDS_SALMON_TITLE, hLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return SALMON_RET_ERROR;
    }

    CSalmonSharedMemory* mem = NULL;
    HANDLE fm = OpenFileMapping(FILE_MAP_WRITE, FALSE, fileMappingName);
    if (fm != NULL)
        mem = (CSalmonSharedMemory*)MapViewOfFile(fm, FILE_MAP_WRITE, 0, 0, 0);
    if (mem == NULL)
    {
        if (fm != NULL)
            CloseHandle(fm);
        HINSTANCE hLanguage = LoadSLG(slgName); // load the default SLG so that we can display possible errors
        if (hLanguage != NULL)
            MessageBox(NULL, LoadStr(IDS_SALMON_WRONG_CMDLINE, hLanguage), LoadStr(IDS_SALMON_TITLE, hLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return SALMON_RET_ERROR;
    }

    if (mem->Version != SALMON_SHARED_MEMORY_VERSION)
    {
        UnmapViewOfFile(mem);
        CloseHandle(fm);
        HINSTANCE hLanguage = LoadSLG(slgName); // load the default SLG so that we can display possible errors
        if (hLanguage != NULL)
            MessageBox(NULL, LoadStr(IDS_SALMON_WRONG_CMDLINE, hLanguage), LoadStr(IDS_SALMON_TITLE, hLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return SALMON_RET_ERROR;
    }

    HANDLE arr[4];
    arr[0] = mem->Process;
    arr[1] = mem->Fire;
    arr[2] = mem->SetSLG;
    arr[3] = mem->CheckBugs;

    SalmonSharedMemory = mem; // set the global pointer so we do not have to thread it through parameters
    strcpy(BugReportPath, mem->BugPath);
    if (BugReportPath[0] != 0 && *(BugReportPath + strlen(BugReportPath) - 1) != '\\')
        strcat(BugReportPath, "\\");

    BOOL run = TRUE;
    while (run)
    {
        // wait for one of the monitored events
        DWORD waitRet = MsgWaitForMultipleObjects(4, arr, FALSE, INFINITE, QS_ALLINPUT);
        switch (waitRet)
        {
        case WAIT_OBJECT_0 + 0: // sharedMemory->Process
        {
            // the parent process has terminated, so we exit as well

            // if we find any dumps, process them - Salamander could have crashed during init while loading
            // shell extensions and did not manage to open the main window and call CheckForBugs
            // or Salamander crashed before the exception handler was installed and the minidump was captured by WER,
            // which we have redirected to our bug report directory (Vista+)
            // Salamander could also have crashed in a way that bypassed the exception handler (typically caused by faulty shell extensions)
            ChechForBugs(mem, slgName);

            run = FALSE;
            break;
        }

        case WAIT_OBJECT_0 + 1: // sharedMemory->Fire
        {
            // the parent process wants us to generate a minidump
            if (LoadHLanguageVerbose(slgName)) // we need to display the GUI, we must load the SLG
            {
                // if we manage to lock the mutex, release it later; we do not want
                // additional processes started afterwards to pop up their windows during ours
                BOOL leave = MainDialogMutex.Enter();
                OpenMainDialog(TRUE);
                if (leave)
                    MainDialogMutex.Leave();
            }
            run = FALSE;
            break;
        }

        case WAIT_OBJECT_0 + 2: // sharedMemory->SetSLG
        {
            // Salamander loaded the “correct” SLG and lets us know we should switch to it
            // store its name; actively reading it now makes no sense yet
            strcpy(slgName, mem->SLGName);
            ResetEvent(mem->SetSLG);
            SetEvent(mem->Done); // let Salamander know we have taken over the SLG name
            break;
        }

        case WAIT_OBJECT_0 + 3: // sharedMemory->CheckBugs
        {
            // Salamander informs us that the main window is open and it is time to check whether
            // there are old files in the bug report directory that we should send to the server
            ChechForBugs(mem, slgName);
            break;
        }

        case WAIT_OBJECT_0 + 4: // a message arrived in the message queue, pump it out
        {
            // salmon.exe uses the Win32 subsystem where Windows expects a message loop, which we do not have.
            // After starting salmon.exe a wait cursor was shown for about 5 seconds, see
            // https://forum.altap.cz/viewtopic.php?f=16&t=5572.
            // To get rid of it we had two options: switch to the "console" subsystem
            // or pump the message loop, which I chose as the cleaner solution (when launched by the user it shows no shell window, only a message box).
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            break;
        }
        }
    }

    SalmonSharedMemory = NULL;
    UnmapViewOfFile(mem);
    CloseHandle(fm);

    Config.Save();

    if (HLanguage != NULL)
    {
        FreeLibrary(HLanguage);
        HLanguage = NULL;
    }

    TRACE_I("End");
    return SALMON_RET_OK;
}
