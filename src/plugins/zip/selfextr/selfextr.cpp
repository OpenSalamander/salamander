// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "strings.h"
#include "comdefs.h"
#include "selfextr.h"
#include "extract.h"
#include "resource.h"
#include "dialog.h"
#include "extended.h"

#define STRING(code, string) string,

const char* const StringTable[] =
    {
#include "strings.h"
        NULL};

HANDLE Heap = NULL;
bool RemoveTemp = false;
const char* Title;
bool SafeMode = false;
HINSTANCE HInstance;
bool SafeToEnd = true;

#ifdef EXT_VER

CStringIndex MapFile(const char* name, HANDLE* file, HANDLE* mapping,
                     const unsigned char** data, unsigned long* size, int* err)
{
    DWORD sizeHigh;
    *file = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*file == INVALID_HANDLE_VALUE)
    {
        *err = GetLastError();
        return STR_ERROR_FOPEN;
    }

    if ((*size = GetFileSize(*file, &sizeHigh)) == 0xFFFFFFFF)
    {
        *err = GetLastError();
        CloseHandle(*file);
        return STR_ERROR_FSIZE;
    }
    if (sizeHigh != 0)
    {
        *err = 0;
        CloseHandle(*file);
        return STR_ERROR_MAXSIZE;
    }
    *mapping = CreateFileMapping(*file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping == NULL)
    {
        *err = GetLastError();
        CloseHandle(*file);
        return STR_ERROR_FMAPPING;
    }
    *data = (const unsigned char*)MapViewOfFile(*mapping, FILE_MAP_READ, 0, 0, 0);
    if (ArchiveBase == NULL)
    {
        *err = GetLastError();
        CloseHandle(*mapping);
        CloseHandle(*file);
        return STR_ERROR_FVIEWING;
    }
    return (CStringIndex)0;
}

void UnmapFile(HANDLE file, HANDLE mapping, const unsigned char* data)
{
    UnmapViewOfFile(data);
    CloseHandle(mapping);
    CloseHandle(file);
}

#endif

void CloseMapping()
{
#ifdef EXT_VER
    UnmapFile(ArchiveFile, ArchiveMapping, ArchiveBase);
#else
    UnmapViewOfFile(ArchiveBase);
    CloseHandle(ArchiveMapping);
    CloseHandle(ArchiveFile);
#endif
    ArchiveBase = NULL;
    Title = StringTable[STR_TITLE];
}

int HandleError(CStringIndex message, unsigned long err, char* fileName, bool* retry, unsigned silentFlag, bool noSkip)
{
    if (retry)
        *retry = false;
    if (Silent & silentFlag)
        return 1;
    //const char * title = DlgWin ? StringTable[STR_ERROR] : Title;
    char text[600];
    int ret;
    char title[SE_MAX_TITLE + 100];

    if (DlgWin)
        *title = 0;
    else
    {
        lstrcpy(title, Title);
        lstrcat(title, " - ");
    }
    lstrcat(title, StringTable[STR_ERROR]);
    lstrcpy(text, StringTable[message]);

    if (err)
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), text + lstrlen(text),
                      450 - lstrlen(text), NULL);
    if (message >= STR_ERROR_TEMPNAME && message <= STR_ERROR_MKDIR)
    {
#ifdef EXT_VER
        if (MVMode == MV_DETACH_ARCHIVE)
            lstrcat(text, StringTable[STR_ADVICE2]);
        else
#endif
            //if (DlgWin) noSkip = true;
            if (!DlgWin)
                lstrcat(text, StringTable[STR_ADVICE]);
    }
    if (message == STR_ERROR_WAIT)
        lstrcat(text, StringTable[STR_ERROR_WAIT2]);
    if (fileName)
        ret = ErrorDialog(title, fileName, text, retry != NULL, noSkip);
    else
        ret = MessageBox(DlgWin, text, title, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
    switch (ret)
    {
    case IDC_RETRY:
        *retry = true;
        break;
    case IDC_SKIPALL:
        Silent |= silentFlag;
    case IDC_SKIP:
        break;
    //case IDOK:
    //case IDCANEL:
    default:
        Stop = true;
        if (ArchiveBase && !noSkip)
            CloseMapping();
    }
    return 1;
}

void _RemoveTemporaryDir(const char* dir)
{
    char path[MAX_PATH + 2];
    char* end;
    HANDLE find;
    WIN32_FIND_DATA file;

    lstrcpy(path, dir);
    end = path + lstrlen(path);
    if (*(end - 1) != '\\')
        *end++ = '\\';
    lstrcpy(end, "*.*");
    find = FindFirstFile(path, &file);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (file.cFileName[0] != 0 && lstrcmp(file.cFileName, "..") && lstrcmp(file.cFileName, ".") &&
                (end - path) + lstrlen(file.cFileName) < MAX_PATH)
            {
                lstrcpy(end, file.cFileName);
                if (file.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                    SetFileAttributes(path, file.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
                if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    _RemoveTemporaryDir(path);
                else
                    DeleteFile(path);
            }
        } while (FindNextFile(find, &file));
        FindClose(find);
    }
    *(end - 1) = 0;
    RemoveDirectory(path);
}

BOOL RemoveTemporaryDir()
{
    char buf[MAX_PATH];

    if (lstrlen(TargetPath) < MAX_PATH)
        _RemoveTemporaryDir(TargetPath);
    GetSystemDirectory(buf, MAX_PATH);
    SetCurrentDirectory(buf); // musime z adresare odejit, jinak nepujde smazat

    DWORD attr = SalGetFileAttributes(TargetPath);
    if (attr != INVALID_FILE_ATTRIBUTES)
    {
        if (attr & FILE_ATTRIBUTE_READONLY)
            SetFileAttributes(TargetPath, attr & ~FILE_ATTRIBUTE_READONLY);
    }
    else
        SetFileAttributes(TargetPath, FILE_ATTRIBUTE_ARCHIVE);
    return RemoveDirectory(TargetPath);
}

// nase varianta funkce RegQueryValueEx, narozdil od API varianty zajistuje
// pridani null-terminatoru pro typy REG_SZ, REG_MULTI_SZ a REG_EXPAND_SZ
// POZOR: pri zjistovani potrebne velikosti bufferu vraci o jeden nebo dva (dva
//        jen u REG_MULTI_SZ) znaky vic pro pripad, ze by string bylo potreba
//        zakoncit nulou/nulami
LONG SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
    DWORD dataBufSize = lpData == NULL ? 0 : *lpcbData;
    DWORD type = REG_NONE;
    LONG ret = RegQueryValueEx(hKey, lpValueName, lpReserved, &type, lpData, lpcbData);
    if (lpType != NULL)
        *lpType = type;
    if (type == REG_SZ || type == REG_MULTI_SZ || type == REG_EXPAND_SZ)
    {
        if (hKey != HKEY_PERFORMANCE_DATA &&
            lpcbData != NULL &&
            (ret == ERROR_MORE_DATA || lpData == NULL && ret == ERROR_SUCCESS))
        {
            (*lpcbData) += type == REG_MULTI_SZ ? 2 : 1; // rekneme si radsi o pripadny null-terminator(y) navic
            return ret;
        }
        if (ret == ERROR_SUCCESS && lpData != NULL)
        {
            if (*lpcbData < 1 || ((char*)lpData)[*lpcbData - 1] != 0)
            {
                if (*lpcbData < dataBufSize)
                {
                    ((char*)lpData)[*lpcbData] = 0;
                    (*lpcbData)++;
                }
                else // nedostatek mista pro null-terminator v bufferu
                {
                    (*lpcbData) += type == REG_MULTI_SZ ? 2 : 1; // rekneme si o potrebny null-terminator(y)
                    return ERROR_MORE_DATA;
                }
            }
            if (type == REG_MULTI_SZ && (*lpcbData < 2 || ((char*)lpData)[*lpcbData - 2] != 0))
            {
                if (*lpcbData < dataBufSize)
                {
                    ((char*)lpData)[*lpcbData] = 0;
                    (*lpcbData)++;
                }
                else // nedostatek mista pro druhy null-terminator v bufferu
                {
                    (*lpcbData)++; // rekneme si o potrebny null-terminator
                    return ERROR_MORE_DATA;
                }
            }
        }
    }
    return ret;
}

int InitExtraction(const char* name)
{
    const unsigned char* ptr;
    unsigned long offset, direntries;
    int section;

    //
    // otevreme soubor s archivem (exe) a namapujeme do pameti
    //
#ifdef EXT_VER
    int err;
    CStringIndex ret = MapFile(name, &ArchiveFile, &ArchiveMapping, &ArchiveBase, &Size, &err);
    if (ret)
        return HandleError(ret, err);
#else
    unsigned long sizeHigh;
    ArchiveFile = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (ArchiveFile == INVALID_HANDLE_VALUE)
        return HandleError(STR_ERROR_FOPEN, GetLastError());

    offset = 0;
    if ((Size = GetFileSize(ArchiveFile, &sizeHigh)) == 0xFFFFFFFF)
    {
        HandleError(STR_ERROR_FSIZE, GetLastError());
        CloseHandle(ArchiveFile);
        return 1;
    }
    if (sizeHigh != 0)
    {
        HandleError(STR_ERROR_MAXSIZE, 0);
        CloseHandle(ArchiveFile);
        return 1;
    }
    ArchiveMapping = CreateFileMapping(ArchiveFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (ArchiveMapping == NULL)
    {
        HandleError(STR_ERROR_FMAPPING, GetLastError());
        CloseHandle(ArchiveFile);
        return 1;
    }
    ArchiveBase = (const unsigned char*)MapViewOfFile(ArchiveMapping, FILE_MAP_READ, 0, 0, 0);
    if (ArchiveBase == NULL)
    {
        HandleError(STR_ERROR_FVIEWING, GetLastError());
        CloseHandle(ArchiveMapping);
        CloseHandle(ArchiveFile);
        return 1;
    }
#endif
    //
    // mame ukazatel, ted zanalyzujeme header exe fajlu
    //
    ptr = ArchiveBase;
    if (*(unsigned short*)ptr != ('M' + ('Z' << 8)))
    {
        return HandleError(STR_ERROR_NOEXESIG, 0);
    }
    ptr += *(unsigned long*)(ptr + 60); // move to PE exe header
    if (*(unsigned long*)ptr != ('P' + ('E' << 8) + ('\0' << 16) + ('\0' << 24)))
    {
        return HandleError(STR_ERROR_NOPESIG, 0);
    }
    ptr += 4;                                          // skip PE signature
    offset = *(unsigned long*)(ptr + 20 + 15 * 4);     // size of headers
    direntries = *(unsigned long*)(ptr + 20 + 23 * 4); // number of dir entries
    for (section = *(unsigned short*)(ptr + 2) - 1; section >= 0; section--)
    {
        offset += *(unsigned long*)(ptr + 20 + 24 * 4 + direntries * 8 + section * 10 * 4 + 4 * 4); // size of section
    }
    //test header consistence
    ArchiveStart = (CSelfExtrHeader*)(ArchiveBase + offset);
    if (offset + sizeof(CSelfExtrHeader) > Size)
        return HandleError(STR_ERROR_INCOMPLETE, 0);
    if (ArchiveStart->Signature != SELFEXTR_SIG)
        return HandleError(STR_ERROR_SFXSIG, 0);

#ifdef EXT_VER
    if (sizeof(CEOCentrDirRecord) + ArchiveStart->HeaderSize + offset > Size || (ArchiveStart->Flags & SE_MULTVOL) == 0 &&
                                                                                    ArchiveStart->EOCentrDirOffs + sizeof(CEOCentrDirRecord) + ArchiveStart->HeaderSize + offset > Size)
        return HandleError(STR_ERROR_INCOMPLETE, 0);

#else //EXT_VER
    if (/*ArchiveStart->HeaderSize + ArchiveStart->ArchiveSize + offset > Size ||*/
        ArchiveStart->EOCentrDirOffs + sizeof(CEOCentrDirRecord) + ArchiveStart->HeaderSize + offset > Size)
        return HandleError(STR_ERROR_INCOMPLETE, 0);

#endif //EXT_VER

    Title = (const char*)HeapAlloc(Heap, 0, lstrlen((char*)ArchiveStart + ArchiveStart->TitleOffs) + 1);
    if (!Title)
        return HandleError(STR_LOWMEM, 0);
    lstrcpyn((char*)Title, (char*)ArchiveStart + ArchiveStart->TitleOffs, SE_MAX_TITLE);

#ifdef EXT_VER
    if (ArchiveStart->Flags & SE_MULTVOL && MVMode != MV_PROCESS_ARCHIVE)
    {
        MVMode = MV_DETACH_ARCHIVE;
        return 0;
    }
#endif
    //set target path
    switch (ArchiveStart->Flags & SE_DIRMASK)
    {
    case SE_TEMPDIREX:
    {
#ifdef EXT_VER
        bool b = TargetPath[0] == 0;

        if (b) //temp path specified on commandline
#endif         //EXT_VER
            if (GetTempPath(MAX_PATH, TargetPath) == 0)
                return HandleError(STR_ERROR_TEMPPATH, GetLastError());

        PathRemoveBackslash(TargetPath);
        //*(TargetPath + lstrlen(TargetPath) - 1) = 0;//removes ending slash
        if (SalGetFileAttributes(TargetPath) == 0xFFFFFFFF)
        {
            int err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND

#ifdef EXT_VER
                && b
#endif //EXT_VER
            )
            {
                goto LCURENTDIR;
            }
            else
                return HandleError(STR_ERROR_GETATTR, err);
        }
        break;
    }

    case SE_WINDIR:
    {
        if (!GetWindowsDirectory(TargetPath, MAX_PATH))
        {
            SafeMode = true;
            goto LCURENTDIR;
        }
        break;
    }

    case SE_SYSDIR:
    {
        if (!GetSystemDirectory(TargetPath, MAX_PATH))
        {
            SafeMode = true;
            goto LCURENTDIR;
        }
        break;
    }

    case SE_ENVVAR:
    {
        if (!GetEnvironmentVariable((char*)ArchiveStart + ArchiveStart->TargetDirSpecOffs, TargetPath, MAX_PATH))
        {
            SafeMode = true;
            goto LCURENTDIR;
        }
        break;
    }

    case SE_REGVALUE:
    {
        HKEY key;
        char buffer[MAX_PATH];
        const char* subKey = (char*)ArchiveStart + ArchiveStart->TargetDirSpecOffs + sizeof(LONG); // sizeof(HKEY) nelze pouzit v 64-bit verzi, ulozena je jen 32-bit hodnota
        if (RegOpenKeyEx((HKEY)(ULONG_PTR) * (LONG*)((char*)ArchiveStart + ArchiveStart->TargetDirSpecOffs),
                         subKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
            goto LCURENTDIR;
        DWORD size = MAX_PATH;
        DWORD type;
        const char* value = subKey + lstrlen(subKey) + 1;
        LONG ret = SalRegQueryValueEx(key, value, 0, &type, (unsigned char*)buffer, &size);
        RegCloseKey(key);
        if (ret != ERROR_SUCCESS)
            goto LCURENTDIR;
        if (type == REG_EXPAND_SZ)
        {
            if (!ExpandEnvironmentStrings(buffer, TargetPath, MAX_PATH))
                goto LCURENTDIR;
        }
        else
        {
            if (type != REG_SZ)
                goto LCURENTDIR;
            lstrcpy(TargetPath, buffer);
        }
        break;
    }

    //SE_CURDIR
    default:
    {

    LCURENTDIR:

        if (GetCurrentDirectory(MAX_PATH, TargetPath) == 0)
            return HandleError(STR_ERROR_GETCWD, GetLastError());
    }
    }
    if (!PathMerge(TargetPath, (char*)ArchiveStart + ArchiveStart->SubDirOffs))
        SafeMode = true;
    return 0;
}

int SplitCommandLine(char* exeName, const char** parameters, const char* ptr /*commandLine*/)
{
    int i = 0;
    // Extract program name from the command line
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;
    if (*ptr == '"')
    {
        ptr++;
        while (i < MAX_PATH && *ptr != '\0' && *ptr != '"')
            exeName[i++] = *ptr++;
        if (*ptr == '\"')
            ptr++;
    }
    else
        while (i < MAX_PATH && *ptr != '\0' && *ptr != ' ' && *ptr != '\t')
            exeName[i++] = *ptr++;
    exeName[i] = '\0';
    while (*ptr != '\0' && (*ptr == ' ' || *ptr == '\t'))
        ptr++;
    *parameters = ptr;
    return i;
}

void GetRunDLL(char* path)
{
    GetWindowsDirectory(path, MAX_PATH);
    //if (path[l - 1] != '\\') path[l++] = '\\';
    char* ptr = PathAddBackslash(path);
    lstrcpyn(ptr, "System32\\rundll32.exe", MAX_PATH - (ptr - path));
}

LRESULT CALLBACK MainWindowWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HANDLE* process;
    TRACE4("MainWindowWndProc, msg %X, wParam %X, lParam %X", uMsg, wParam, lParam)
    switch (uMsg)
    {
    case WM_CREATE:
        process = (HANDLE*)((CREATESTRUCT*)lParam)->lpCreateParams;
        break;

    case WM_ENDSESSION:
        TRACE1("WM_ENDSESSION")
        if (wParam)
        {
            TerminateProcess(*process, 0);
#ifdef EXT_VER
            if ((char*)ArchiveStart + ArchiveStart->WaitForOffs && EnumProcOK && WaitingForFirst)
            {
                HANDLE p;
                BOOL ret = EnumProcesses(&p);
                if (p)
                    TerminateProcess(p, 0);
            }
#endif //EXT_VER
            RemoveTemporaryDir();
        }
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int MyWinMain()
{
    char exeName[MAX_PATH];
    const char* parameters;
    unsigned int i;
    bool showStat;

    HInstance = GetModuleHandle(NULL);

    Title = StringTable[STR_TITLE];

    Heap = GetProcessHeap();

    i = SplitCommandLine(exeName, &parameters, GetCommandLine());
    // If the extension is not present, add ".exe"
    if (i < 5 || exeName[i - 4] != '.')
    {
        lstrcat(exeName, "*.exe");
    }

#ifdef EXT_VER
    TargetPath[0] = 0;
#endif //EXT_VER

    if (*parameters)
    {
#ifdef EXT_VER
        for (const char* sour = parameters; *sour != 0; sour++)
        {
            if (*sour == ' ')
                continue;
            if (*sour == '-')
            {
                sour++;
                switch (*sour)
                {
                case 's':
                {
                    SafeMode = true;
                    break;
                }

                    //          case 'l': SearchLastFile = false;
                case 'a':
                {
                    sour = ReadPath(sour + 1, RealArchiveName);
                    MVMode = MV_PROCESS_ARCHIVE;
                    break;
                }

                case 't':
                {
                    sour = ReadPath(sour + 1, TargetPath);
                    //MVMode = MV_PROCESS_ARCHIVE;
                    break;
                }

                default:
                    goto badparam;
                }
            }
            else
            {

            badparam:

                MessageBox(NULL, StringTable[STR_PARAMTERS], Title, MB_ICONINFORMATION | MB_SETFOREGROUND);
                return 1;
            }
        }

#else  //EXT_VER
        if (lstrlen(parameters) >= 2 &&
            *((short*)parameters) == (short)('-' | 's' << 8))
            SafeMode = true;
        else
        {
            MessageBox(NULL, StringTable[STR_PARAMTERS], Title, MB_ICONINFORMATION | MB_SETFOREGROUND);
            return 1;
        }
#endif //EXT_VER
    }

#ifdef _DEBUG
    if (InitExtraction("test.exe"))
        return 1;
#else  //_DEBUG
    if (InitExtraction(exeName))
        return 1;
#endif //_DEBUG

#ifdef EXT_VER
    if (MVMode == MV_DETACH_ARCHIVE)
    {
        char sfxBat[MAX_PATH];
        i = DetachArchive(sfxBat, parameters);
        if (!i)
        {
            STARTUPINFO si;
            PROCESS_INFORMATION pi;

            si.dwFlags = STARTF_USESHOWWINDOW;
            si.lpReserved = 0;
            si.lpDesktop = NULL;
            si.lpTitle = NULL;
            si.cbReserved2 = 0;
            si.lpReserved2 = 0;
            si.wShowWindow = SW_HIDE;
            if (!CreateProcess(NULL, sfxBat, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                i = HandleError(STR_ERROR_SPAWNPROCESS, GetLastError());
            }
        }
        if (i && RemoveTemp)
            RemoveTemporaryDir();
        if (MVMode == MV_PROCESS_ARCHIVE && RAData)
            UnmapFile(RAFile, RAMapping, RAData);
        CloseMapping();
        return i;
    }
#endif
    /*  if (ArchiveStart->Flags & SE_CONFIRMEXTRACTING)
  {
    if (MessageBox(NULL, (char *)ArchiveStart + sizeof(CSelfExtrHeader) +
        ArchiveStart->CommandLen, Title, MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
      CloseMapping();
      return 1;
    }
  }
  */

    //jeste zobrazime messagebox
    if (ArchiveStart->MBoxStyle != -1)
    {
        int r = (int)ArchiveStart->MBoxStyle < 0 ? DialogBox(HInstance, MAKEINTRESOURCE(IDD_LONGMESSAGE), NULL, LongMessageDlgProc) : MessageBox(NULL, (char*)ArchiveStart + ArchiveStart->MBoxTextOffs, (char*)ArchiveStart + ArchiveStart->MBoxTitleOffs, ArchiveStart->MBoxStyle);
        if (r != IDOK && r != IDYES)
        {
            CloseMapping();
            return 1;
        }
    }

    showStat = (ArchiveStart->Flags & SE_SHOWSUMARY) != 0;
    if (ArchiveStart->Flags & SE_OVEWRITEALL)
        Silent |= SF_OVEWRITEALL | SF_OVEWRITEALL_RO;
    if (ArchiveStart->Flags & SE_HIDEMAINDLG && !SafeMode)
    {
        if ((i = StartExtracting()) == 0)
        {
            if (WaitForSingleObject((HANDLE)Thread, INFINITE) == WAIT_FAILED)
            {
                i = HandleError(STR_ERROR_WAITTHREAD, GetLastError());
            }
            else
                GetExitCodeThread(Thread, (LPDWORD)&i);
        }
    }
    else
    {
        if ((i = DialogBoxParam(HInstance, MAKEINTRESOURCE(SE_IDD_SFXDIALOG),
                                NULL, SfxDlgProc, (LPARAM)NULL)) == -1)
            return HandleError(STR_ERROR_DLG, GetLastError());
    }
    if (showStat)
    {
        char num[50];
        char message[500];
        char skipped[100];

        if (SkippedFiles)
            wsprintf(skipped, StringTable[STR_SKIPPED], SkippedFiles);
        else
            *skipped = 0;

        wsprintf(message, StringTable[ExtractedFiles ? STR_STATOK : STR_STATNOTCOMPLETE],
                 ExtractedFiles, skipped, NumberToStr(num, ExtractedMB));
        MessageBox(NULL, message, Title, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
    }
    if (!i && ArchiveStart->CommandOffs)
    {
        static char expandedCmdLine[SE_MAX_COMMANDLINE * 2];
        char buf[1024];
        int r = ExpandEnvironmentStrings((char*)ArchiveStart + sizeof(CSelfExtrHeader),
                                         expandedCmdLine, sizeof(expandedCmdLine));

        // pokud expanze nevydarila, bereme puvodni hodnotu
        int len = SplitCommandLine(exeName, &parameters,
                                   r > 0 && r <= sizeof(expandedCmdLine) ? expandedCmdLine : (char*)ArchiveStart + sizeof(CSelfExtrHeader));

        if (lstrcmpi(exeName + len - 4, ".inf") == 0)
        {
            wsprintf(buf, "setupapi,InstallHinfSection DefaultInstall 132 %s", exeName);
            GetRunDLL(exeName);
            parameters = buf;
        }
        else
        {
            if (lstrcmpi(exeName + len - 4, ".exe") &&
                lstrcmpi(exeName + len - 4, ".com") &&
                lstrcmpi(exeName + len - 4, ".bat"))
                parameters = NULL;
        }

#ifdef EXT_VER
        if (RemoveTemp && (char*)ArchiveStart + ArchiveStart->WaitForOffs)
        {
            EnumProcOK = EnumProcesses(NULL);
        }
#endif //EXT_VER

        SHELLEXECUTEINFO info;
        info.cbSize = sizeof(SHELLEXECUTEINFO);
        info.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        info.hwnd = NULL;
        info.lpVerb = NULL;
        info.lpFile = exeName;
        info.lpParameters = parameters;
        info.lpDirectory = NULL;
        info.nShow = SW_SHOWDEFAULT;

    execute:
        if (!ShellExecuteEx(&info))
        {
            int e = GetLastError();
            if (e == ERROR_NO_ASSOCIATION && !RemoveTemp)
            {
                wsprintf(buf, "shell32.dll,OpenAs_RunDLL %s", exeName);
                GetRunDLL(exeName);
                info.lpParameters = buf;
                goto execute;
            }
            i = HandleError(STR_ERROR_EXECSHELL, GetLastError(), exeName, NULL, 0, true);
            CloseMapping();
        }
        else
        {
            if (RemoveTemp)
            {
                WNDCLASS wc;
                MemZero(&wc, sizeof(WNDCLASS));
                wc.lpfnWndProc = MainWindowWndProc;
                wc.hInstance = HInstance;
                wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
                const char* wndClassName = "MainWindowWndClass";
                wc.lpszClassName = wndClassName;
                RegisterClass(&wc);
                TRACE2("process handle %X", (HANDLE)info.hProcess)
                CreateWindow(wndClassName, "", WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, HInstance, (LPVOID)&info.hProcess);
                BOOL wait = TRUE;
                while (wait)
                {
                    switch (MsgWaitForMultipleObjects(1, &(HANDLE)info.hProcess, FALSE, INFINITE, QS_ALLINPUT))
                    {
                    case -1:
                        i = HandleError(STR_ERROR_WAIT, GetLastError());
                        wait = FALSE;
                        break;

                    case WAIT_OBJECT_0 + 1:
                    {
                        MSG msg;
                        TRACE1("WAIT_OBJECT_0 + 1")
                        PeekMessage(&msg, (HWND)NULL, 0, 0, PM_REMOVE);
                        /*BOOL b = FALSE; pro debug ucely
	      if (b) MainWindowWndProc(NULL, WM_USER + 1, TRUE, 0);*/
                        break;
                    }

                    default:
                    {
#ifdef EXT_VER
                        if ((char*)ArchiveStart + ArchiveStart->WaitForOffs && WaitingForFirst)
                        {
                            if (!EnumProcOK)
                                goto LERROR_WAIT;
                            info.hProcess = 0;
                            BOOL ret;
                            ret = EnumProcesses(&(HANDLE)info.hProcess);
                            if (!info.hProcess)
                            {
                                if (!ret)
                                {
                                LERROR_WAIT:

                                    i = HandleError(STR_ERROR_WAIT, GetLastError());
                                }
                                wait = FALSE;
                            }
                            WaitingForFirst = FALSE;
                        }
                        else
#endif EXT_VER
                            wait = FALSE;
                    }
                    }
                }
                TRACE1("konec cekani")
            }
        }
    }

    if (RemoveTemp)
        RemoveTemporaryDir();
    TRACE1("konec mazani")

#ifdef EXT_VER
    if (MVMode == MV_PROCESS_ARCHIVE && RAData)
        UnmapFile(RAFile, RAMapping, RAData);
#endif

    if (!i)
        CloseMapping();
    return i;
}

#ifdef EXT_VER
LONG WINAPI MyExceptionFilter(_EXCEPTION_POINTERS* exception)
{
    if ((exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
         exception->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        exception->ExceptionRecord->NumberParameters >= 2 &&
        exception->ExceptionRecord->ExceptionInformation[0] == 0 &&
        RAData != 0 &&
        exception->ExceptionRecord->ExceptionInformation[1] >= (ULONG_PTR)RAData &&
        exception->ExceptionRecord->ExceptionInformation[1] < (ULONG_PTR)RAData + RASize)
    {
        if (MessageBox(DlgWin, StringTable[STR_ERROR_READMAPPED], Title,
                       MB_RETRYCANCEL | MB_ICONEXCLAMATION | MB_SETFOREGROUND) == IDRETRY)
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        else
        {
            if (OutFile)
                CloseHandle(OutFile);
            if (RemoveTemp)
                RemoveTemporaryDir();
            ExitProcess(1);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif //EXT_VER

#ifdef _DEBUG
HANDLE TraceLogFile = NULL;
void Trace(char* message, ...)
{
    va_list arglist;
    va_start(arglist, message);
    char buf[1024];
    wvsprintf(buf, message, arglist);
    va_end(arglist);
    lstrcat(buf, "\r\n");
    DWORD dummy;
    if (TraceLogFile)
        WriteFile(TraceLogFile, buf, lstrlen(buf), &dummy, NULL);
}
#endif //_DEBUG

void WinMainCRTStartup()
{
    int ret;

#ifdef _DEBUG
    TraceLogFile = CreateFile("c:\\trace.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#endif //_DEBUG

#ifdef EXT_VER
    SetUnhandledExceptionFilter(MyExceptionFilter);
#endif //EXT_VER

    ret = MyWinMain();

#ifdef _DEBUG
    CloseHandle(TraceLogFile);
#endif //_DEBUG
    TRACE1("Exiting process")
    ExitProcess(ret);
}
