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
char LatestBugReport[MAX_PATH] = {0}; // jmeno casove posledniho bug reportu (pouze jmeno bez pripony)
BOOL ReportOldBugs = TRUE;            // uzivatel povolil upload i starych reportu

const char* APP_NAME = "Open Salamander Bug Reporter";

// ****************************************************************************

class C__StrCriticalSection
{
public:
    CRITICAL_SECTION cs;

    C__StrCriticalSection() { HANDLES(InitializeCriticalSection(&cs)); }
    ~C__StrCriticalSection() { HANDLES(DeleteCriticalSection(&cs)); }
};

// zajistime vcasnou konstrukci kriticke sekce
#pragma warning(disable : 4073)
#pragma init_seg(lib)
C__StrCriticalSection __StrCriticalSection;
C__StrCriticalSection __StrCriticalSection2;

// ****************************************************************************

char* LoadStr(int resID, HINSTANCE hInstance)
{
    static char buffer[10000]; // buffer pro mnoho stringu
    static char* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    if (10000 - (act - buffer) < 200)
        act = buffer;

    if (hInstance == NULL)
        hInstance = HLanguage;
#ifdef _DEBUG
    // radeji si pojistime, aby nas nekdo nevolal pred inicializaci handlu s resourcy
    if (hInstance == NULL)
        TRACE_E("LoadStr: hInstance == NULL");
#endif // _DEBUG

RELOAD:
    int size = LoadString(hInstance, resID, act, 10000 - (int)(act - buffer));
    // size obsahuje pocet nakopirovanych znaku bez terminatoru
    //  DWORD error = GetLastError();
    char* ret;
    if (size != 0) // error je NO_ERROR, i kdyz string neexistuje - nepouzitelne
    {
        if ((10000 - (act - buffer) == size + 1) && (act > buffer))
        {
            // pokud byl retezec presne na konci bufferu, mohlo
            // jit o oriznuti retezce -- pokud muzeme posunout okno
            // na zacatek bufferu, nacteme string jeste jednou
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
// az do soubehu minimalne 10 vypisovanych chyb naraz by to melo jiste fungovat,
// vic threadu nez 10 najednou neocekavame ;-)

char* GetErrorText(DWORD error)
{
    static char buffer[10 * MAX_PATH]; // buffer pro mnoho stringu
    static char* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection2.cs));

    if (10 * MAX_PATH - (act - buffer) < MAX_PATH + 20)
        act = buffer;

    char* ret = act;
    // POZOR: sprintf_s v debug verzi vyplnuje cely buffer, tedy nelze mu podat cely buffer (jsou
    // v nem i dalsi stringy), resit bud pres _CrtSetDebugFillThreshold nebo zadanim mensi velikosti)
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
        se.lpVerb = "explore"; // volba zda otevirat se stromeckem
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
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak GetFileAttributes
    // mezery/tecky orizne a pracuje tak s jinou cestou + u souboru to sice nefunguje,
    // ale porad lepsi nez ziskat atributy jineho souboru/adresare (pro "c:\\file.txt   "
    // pracuje se jmenem "c:\\file.txt")
    if (fileNameLen > 0 && (fileName[fileNameLen - 1] <= ' ' || fileName[fileNameLen - 1] == '.') &&
        fileNameLen + 1 < _countof(fileNameCopy))
    {
        memcpy(fileNameCopy, fileName, fileNameLen);
        fileNameCopy[fileNameLen] = '\\';
        fileNameCopy[fileNameLen + 1] = 0;
        return GetFileAttributes(fileNameCopy);
    }
    else // obycejna cesta, neni co resit, jen zavolame windowsovou GetFileAttributes
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
        // pokud se SLG nepovedlo nacist, nemusi existovat nebo nam nepredali validni nazev
        // pokusime se najit nejake vhodne jine, podle priority
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

// prohleda pole BugReports (ktere obsahuje nazvy vcetne pripon) a vrati index 'name'
// ignoruje pripony
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

    // pokud adresar neexistuje, nemuze obsahovat reporty
    if (!DirExists(BugReportPath))
        return FALSE;

    // hledame reporty podle pripon
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
                    // minidumpy nad 200MB proste smazeme, protoze neverim, ze by po zapakovani prosly na server
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
                    // ulozime nejnovejsi jmeno a cas
                    if (latestFilename[0] == 0 || CompareFileTime(&latestFiletime, &find.ftLastWriteTime) == -1)
                    {
                        strcpy(latestFilename, item.Name);
                        latestFiletime = find.ftLastWriteTime;
                    }

                    // jmena co nemame v seznamu do nej pridame
                    if (GetBugReportNameIndexIgnoreExt(item.Name) == -1)
                        BugReports.Add(item);
                }
            } while (FindNextFile(hFind, &find));
            HANDLES(FindClose(hFind));
        }
    }

    // nejnovejsi report vybublame na nultou pozici
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

    // pokud jde o padacky z WERu, nemaji predponu s UID a verzi - prejmenujeme je
    char uid[17];
    sprintf(uid, "%I64X", SalmonSharedMemory->UID);
    for (int i = 0; i < BugReports.Count; i++)
    {
        CBugReport* item = &BugReports[i];
        // pokud soubor nezacina nasim UID, generoval ho nekdo jiny (WER) a polozku prejmenujeme
        if (_strnicmp(item->Name, uid, strlen(uid)) != 0)
        {
            char fullOrgName[MAX_PATH];
            char fullNewName[MAX_PATH];
            char newName[MAX_PATH];
            char tmpName[MAX_PATH];
            char extName[MAX_PATH];

            strcpy(fullOrgName, BugReportPath);
            strcat(fullOrgName, item->Name);
            char* ext = strrchr(item->Name, '.'); // priponu dame az na konec
            if (ext != NULL)
                strcpy(extName, ext);
            else
                extName[0] = 0;

            SYSTEMTIME lt;
            GetLocalTime(&lt);

            strcpy(tmpName, item->Name);
            if (ext != NULL)
                *strrchr(tmpName, '.') = 0; // uvnitr jmena ji vystrihnem
            strcat(tmpName, "-");
            strcat(tmpName, SalmonSharedMemory->BugName);
            GetReportBaseName(newName, sizeof(newName), BugReportPath, tmpName, SalmonSharedMemory->UID, lt);
            strcat(newName, extName);

            // nove jmeno v poli
            strcpy(item->Name, newName);

            // nove jmeno na disku
            strcpy(fullNewName, BugReportPath);
            strcat(fullNewName, newName);
            MoveFile(fullOrgName, fullNewName);
        }
    }

    // pripona nas nezajima, ustrihneme ji
    for (int i = 0; i < BugReports.Count; i++)
    {
        CBugReport* item = &BugReports[i];
        char* ext = strrchr(item->Name, '.');
        if (ext != NULL)
            *ext = 0;
    }

    return BugReports.Count > 0;
}

// ignoruje 'oversize' reporty, ktere konci -1 az -99
int GetUniqueBugReportCount()
{
    TDirectArray<CBugReport> UniqueNames(1, 10);
    char buff[MAX_PATH];
    for (int i = 0; i < BugReports.Count; i++)
    {
        CBugReport* item = &BugReports[i];
        //    MessageBox(NULL, item->Name, item->Name, MB_OK);
        // z nazvu orizneme koncove -1 az -99
        strcpy(buff, item->Name);
        int len = (int)strlen(buff);
        if (len > 3)
        {
            if (buff[len - 2] == '-')
                buff[len - 2] = 0;
            if (buff[len - 3] == '-')
                buff[len - 3] = 0;
        }
        // pokud polozku nenajdeme v unikatnich nazvech, pridame ji tam
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
    sprintf(name, "%s%s.INF", BugReportPath, BugReports[0].Name); // budeme pracovat s nejnovejsim nazvem
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

    // volajici musi uvolnit vracenou pamet pomoci LocalFree, viz MSDN
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

CMainDialogMutex MainDialogMutex; // mutex zajistujici, abychom uzivatelum ukazovali pro jednoho uzivatele (i na serveru) jen jeden dialog

void CMainDialogMutex::Init()
{
    LPTSTR sid = NULL;
    if (!GetStringSid(&sid))
        sid = NULL;

    char buff[1000];
    if (sid == NULL)
    {
        // chyba v ziskani SID -- pojedeme v nouzovem rezimu
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
            return FALSE; // je otevrene jine okno, nemuzeme se otevrit my
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
                // potrebujeme zobrazit GUI, musime nacist SLG
                if (LoadHLanguageVerbose(slgName))
                {
                    if (GetUniqueBugReportCount() > 1)
                    {
                        // reportu je vic, doptame se zda je mame poslat vsechny
                        int res = MessageBox(NULL, LoadStr(IDS_SALMON_MORE_REPORTS, HLanguage), LoadStr(IDS_SALMON_TITLE, HLanguage), MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND);
                        ReportOldBugs = (res == IDYES);
                    }
                    // nyni muzeme otevrit dialog
                    OpenMainDialog(FALSE);
                }
            }
            MainDialogMutex.Leave();
        }
    }

    ResetEvent(mem->CheckBugs);
    SetEvent(mem->Done); // dame do Salamandera vedet, ze jsme si nazev SLG prevzali
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
    // v 99% kdy Salamander nespadne pobezi salmon.exe nepozorovane na pozadi a mel by mit jen co nejmensi pametove / cpu naroky;
    // proto odlozime nacitani SLG az na okamzik, kdy bude potreba neco zobrazovat (pad Salamandera)

    SetTraceProcessName("Salmon");
    SetThreadNameInVCAndTrace("Main");
    TRACE_I("Begin");

    // nechceme zadne kriticke chyby jako "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    HInstance = hInstance;

    Config.Load();

    char fileMappingName[SALMON_FILEMAPPIN_NAME_SIZE];
    char slgName[MAX_PATH] = {0}; // nazev slg (napr "english.slg"), ktere se ma nacist do HLanguage; muze byt i prazdny retezec (potom se nacte nejaky default)

    if (!ParseCommandLine(cmdLine, fileMappingName, slgName) || strlen(fileMappingName) == 0)
    {
        HINSTANCE hLanguage = LoadSLG(slgName); // nactu default SLG, abychom mohli zobrazovat pripadne chyby
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
        HINSTANCE hLanguage = LoadSLG(slgName); // nactu default SLG, abychom mohli zobrazovat pripadne chyby
        if (hLanguage != NULL)
            MessageBox(NULL, LoadStr(IDS_SALMON_WRONG_CMDLINE, hLanguage), LoadStr(IDS_SALMON_TITLE, hLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return SALMON_RET_ERROR;
    }

    if (mem->Version != SALMON_SHARED_MEMORY_VERSION)
    {
        UnmapViewOfFile(mem);
        CloseHandle(fm);
        HINSTANCE hLanguage = LoadSLG(slgName); // nactu default SLG, abychom mohli zobrazovat pripadne chyby
        if (hLanguage != NULL)
            MessageBox(NULL, LoadStr(IDS_SALMON_WRONG_CMDLINE, hLanguage), LoadStr(IDS_SALMON_TITLE, hLanguage), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
        return SALMON_RET_ERROR;
    }

    HANDLE arr[4];
    arr[0] = mem->Process;
    arr[1] = mem->Fire;
    arr[2] = mem->SetSLG;
    arr[3] = mem->CheckBugs;

    SalmonSharedMemory = mem; // nastavime globalku, at to nemusime pasirovat pres parametry
    strcpy(BugReportPath, mem->BugPath);
    if (BugReportPath[0] != 0 && *(BugReportPath + strlen(BugReportPath) - 1) != '\\')
        strcat(BugReportPath, "\\");

    BOOL run = TRUE;
    while (run)
    {
        // pockame na nektery z hlidanych eventu
        DWORD waitRet = MsgWaitForMultipleObjects(4, arr, FALSE, INFINITE, QS_ALLINPUT);
        switch (waitRet)
        {
        case WAIT_OBJECT_0 + 0: // sharedMemory->Process
        {
            // parent proces prestal existovat, koncime take

            // pokud najdeme nejake dumpy, odbavime je - Salamander mohl padnou napriklad pri initu behem nacitani
            // shell extensions a nestihl otevrit hlavni okno a zavolat nam ChechForBugs
            // pripadne Salamander padnul jeste pred instalaci exception handleru a minidump zachytil WER, ktery
            // mame nasmerovany do naseho bug report adresare (Vista+)
            // dale mohl Salamander padnou tak, ze nezabral exception handler (typicky to delaji vadne shell extensions)
            ChechForBugs(mem, slgName);

            run = FALSE;
            break;
        }

        case WAIT_OBJECT_0 + 1: // sharedMemory->Fire
        {
            // parent proces po nas chce nagenerovani minidumpu
            if (LoadHLanguageVerbose(slgName)) // potrebujeme zobrazit GUI, musime nacist SLG
            {
                // pokud se nam podari mutex zabrat, pozdeji ho uvolnime; neprejeme si, aby
                // behem naseho okna vyskakovala dalsi z nove spoustenych procesu
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
            // Salamander nacetl "spravne" slg a dava nam vedet, at na nej take prejdeme
            // ulozime si jeho nazev, aktivne zatim nema smysl ho cist
            strcpy(slgName, mem->SLGName);
            ResetEvent(mem->SetSLG);
            SetEvent(mem->Done); // dame do Salamandera vedet, ze jsme si nazev SLG prevzali
            break;
        }

        case WAIT_OBJECT_0 + 3: // sharedMemory->CheckBugs
        {
            // Salamander nam dava vedet, ze ma otevrene hlavni okno a je cas provest kontrolu, zda
            // v adresari s bug reporty nelezi nejake stare soubory, co bychom meli odeslat na server
            ChechForBugs(mem, slgName);
            break;
        }

        case WAIT_OBJECT_0 + 4: // dorazila zprava do message queue, vypumpujeme ji
        {
            // salmon.exe pouziva win32 subsystem u ktereho Windows ocekavaji message loop, kterou vsak nemame
            // po spusteni salmon.exe se tak na 5s zobrazoval wait cursor, viz
            // https://forum.altap.cz/viewtopic.php?f=16&t=5572
            // abychom se ho zbavili, mame dve moznosti: prejit na "console" subsystem
            // nebo pumpovat message loop, coz jsem zvolil jako hezci reseni (pri spusteni uzivatelem neukaze shell okno, jen msgbox)
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
