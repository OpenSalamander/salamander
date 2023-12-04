// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <Tlhelp32.h>

#ifndef EXT_VER
#error This module should be compiled only when EXT_VER is defined.
#endif //EXT_VER

#include "strings.h"
#include "comdefs.h"
#include "selfextr.h"
#include "extract.h"
#include "resource.h"
#include "dialog.h"
#include "extended.h"

const char* const DetachedExe = "sfx.exe";
const char* const DetachedBat = "sfx.bat";

int MVMode = 0;
char RealArchiveName[MAX_PATH];
HANDLE RAFile;
HANDLE RAMapping;
const unsigned char* RAData = NULL;
unsigned long RASize;
int DiskNum;
CEOCentrDirRecord* ECRec;
CFileHeader* CentrDir;
__UINT32 Keys[3];
bool Encrypted;
bool SearchLastFile = true;

const char* ReadPath(const char* cmdline, char* path)
{
    //const char * start = sour + 1;
    const char* end;
    while (*cmdline == ' ')
        cmdline++;
    if (*cmdline == '"')
    {
        cmdline++;
        end = StrChr(cmdline, '"');
    }
    else
    {
        end = StrChr(cmdline, ' ');
    }
    int len = end ? end - cmdline : lstrlen(cmdline);
    if (len >= MAX_PATH)
        len = MAX_PATH - 1;
    MemCpy(path, cmdline, len);
    path[len] = 0;
    if (end && *end == '"')
        len++;
    return cmdline + len - 1;
}

int DetachArchive(char* sfxBat, const char* parameters)
{
    char sfxExe[MAX_PATH];
    char buf[1024];
    DWORD written;

    if (TargetPath[0] == 0)
    {
        if (GetTempPath(MAX_PATH, buf) == 0)
            return HandleError(STR_ERROR_TEMPPATH, GetLastError());
    }
    else
        lstrcpy(buf, TargetPath);

    //*(TargetPath + lstrlen(TargetPath) - 1) = 0;//removes ending slash
    if (SalGetFileAttributes(buf) == 0xFFFFFFFF)
    {
        int err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND && TargetPath[0] == 0)
        {
            if (GetWindowsDirectory(buf, MAX_PATH) == 0)
                return HandleError(STR_ERROR_GETWD, GetLastError());
        }
        else
            return HandleError(STR_ERROR_GETATTR, err);
    }
    if (GetTempFileName(buf, "SFX", 0, TargetPath) == 0)
        return HandleError(STR_ERROR_TEMPNAME, GetLastError());
    if (DeleteFile(TargetPath) == 0)
        return HandleError(STR_ERROR_DELFILE, GetLastError());
    if (CreateDirectory(TargetPath, NULL) == 0)
        return HandleError(STR_ERROR_MKDIR, GetLastError());
    RemoveTemp = true;

    lstrcpy(sfxExe, TargetPath);
    PathAppend(sfxExe, DetachedExe);
    OutFile = CreateFile(sfxExe, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (OutFile == INVALID_HANDLE_VALUE)
    {
        HandleError(STR_ERROR_TEMPCREATE, GetLastError(), sfxExe, NULL, 0, true);
        CloseMapping();
        return 1;
    }
    if (!WriteFile(OutFile, ArchiveBase,
                   /*((unsigned char *)ArchiveStart - ArchiveBase) + ArchiveStart->HeaderSize*/
                   Size, &written, NULL))
    {
        HandleError(STR_ERROR_WRITE, GetLastError(), sfxExe, NULL, 0, true);
        CloseHandle(OutFile);
        OutFile = 0;
        CloseMapping();
        return 1;
    }
    CloseHandle(OutFile);
    OutFile = 0;

    lstrcpy(sfxBat, TargetPath);
    PathAppend(sfxBat, DetachedBat);
    OutFile = CreateFile(sfxBat, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (OutFile == INVALID_HANDLE_VALUE)
    {
        HandleError(STR_ERROR_TEMPCREATE, GetLastError(), sfxBat, NULL, 0, true);
        CloseMapping();
        return 1;
    }

    char archive[MAX_PATH];
    if (!GetModuleFileName(NULL, archive, MAX_PATH))
        HandleError(STR_ERROR_GETMODULENAME, GetLastError());
    PathRemoveFileSpec(archive);
    PathAppend(archive, (char*)ArchiveStart + ArchiveStart->ArchiveNameOffs);
    /*
  wsprintf(buf, "start /wait %s \"%s\" %s -a \"%s\"\r\n"
                "rmdir /s /q \"%s\"\r\n"
                "del \"%s\"\r\n"
                "del \"%s\"\r\n",
                "\"\"", sfxExe, parameters, archive, TargetPath, sfxExe, sfxBat);
                */
    wsprintf(buf, "start /wait \"\" \"%s\" %s -a \"%s\"\r\nrmdir /s /q \"%s\"\r\n",
             sfxExe, parameters, archive, TargetPath);

    if (!WriteFile(OutFile, buf, lstrlen(buf), &written, NULL))
    {
        HandleError(STR_ERROR_WRITE, GetLastError(), sfxBat, NULL, 0, true);
        CloseHandle(OutFile);
        OutFile = 0;
        CloseMapping();
        return 1;
    }
    CloseHandle(OutFile);
    OutFile = 0;
    return 0;
}

void SplitPath(char* path, char* file, char* ext)
{
    int l = lstrlen(path);
    char* dot = StrRChr(path, path + l, '.'); // ".cvspass" is extension in Windows
    int i = dot - path;
    if (dot > StrRChr(path, path + l, '\\'))
    {
        lstrcpy(ext, dot);
    }
    else
    {
        *ext = 0;
        i = l;
    }
    MemCpy(file, path, i);
    file[i] = 0;
}
/*
int atoi(const char *nptr)
{
  int c;              // current char
  int total;         // current total

  c = (int)(unsigned char)*nptr++;

  total = 0;

  while (c >= '0' && c <= '9' )
  {
    total = 10 * total + (c - '0');     // accumulate digit
    c = (int)(unsigned char)*nptr++;    // get next char
  }
  return total;
}

int OpenLastVolume()
{
  char lastFile[MAX_PATH];
  lstrcpy(lastFile, RealArchiveName);

  while (1)
  {
    if (ArchiveStart->Flags & SE_SEQNAMES && SearchLastFile)
    {
      //find last file
      char file[MAX_PATH];
      char ext[MAX_PATH];
      char mask[MAX_PATH];
      char buf[MAX_PATH];
      WIN32_FIND_DATA   data;
      HANDLE            search;
      int               biggest;
      int               j;

      biggest = 0;

      SplitPath(RealArchiveName, file, ext);
      char * sour = file + lstrlen(file) - 1;
      while (sour > file)
      {
        if ((*sour < '0' || *sour > '9' )) break;
        sour--;
      }
      if (sour >= file)
      {
        *(++sour) = 0;
        wsprintf(mask, "%s*%s", file, ext);
        search = FindFirstFile(mask, &data);
        if (search != INVALID_HANDLE_VALUE)
        {
          lstrcpy(buf, file);
          do
          {
            SplitPath(data.cFileName, file, ext);
            sour = file + lstrlen(file) - 1;
            while (sour > file)
            {
              if ((*sour < '0' || *sour > '9' )) break;
              sour--;
            }
            if (sour >= file)
            {
              j = atoi(++sour);
              if (j > biggest && !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
              {
                biggest = j;
                PathRemoveFileSpec(buf);
                PathAppend(buf, data.cFileName);
              }
            }
          } while (FindNextFile(search, &data));
          if (GetLastError() == ERROR_NO_MORE_FILES && biggest) lstrcpy(lastFile, buf);
          FindClose(search);
        }
      }
    }
    //check if file exists
    DWORD attr = SalGetFileAttributes(lastFile);
    if (attr == 0xFFFFFFFF || attr & FILE_ATTRIBUTE_DIRECTORY) goto repeat;
    int err;
    bool retry;
    //open the file
    CStringIndex ret;
    ret = MapFile(lastFile, &RAFile, &RAMapping, &RAData, &RASize, &err);
    if (ret)
    {
      HandleError(ret, err, lastFile, &retry, 0,  true);
      if (retry) continue;
      else
      {
        CloseMapping();
        return 1;
      }
    }
    else
    {
      const unsigned char *ptr = RAData + RASize - 22;
      while (ptr > RAData && ptr + 64*1024 > RAData + RASize)
      {
        if ( ((CEOCentrDirRecord * )ptr)->Signature == SIG_EOCENTRDIR)
        {
          return ReadCentralDirectory((CEOCentrDirRecord * )ptr);
        }
        ptr--;
      }
      UnmapFile(RAFile, RAMapping, RAData);
      RAData = NULL;
      //find ecrec
    }

repeat:
    if (MessageBox(DlgWin, StringTable[STR_INSERT_LAST_DISK], Title, MB_ICONQUESTION | MB_SETFOREGROUND | MB_OKCANCEL)== IDCANCEL)
    {
      CloseMapping();
      return 1;
    }
  }
}
*/

int ReadCentralDirectory()
{
    ECRec = (CEOCentrDirRecord*)((char*)ArchiveStart + ArchiveStart->HeaderSize);
    CentrDir = (CFileHeader*)HeapAlloc(Heap, 0, ECRec->CentrDirSize);
    if (!CentrDir)
        return HandleError(STR_LOWMEM, 0);
    if (ECRec->StartDisk != 0)
    {
        if (ChangeDisk(ECRec->StartDisk + 1, FALSE))
            return 1;
    }
    else if (ChangeDisk(ECRec->StartDisk + 1, TRUE))
        return 1; //otevreme si prvni soubor
    DiskNum = ECRec->StartDisk + 1;
    unsigned left = ECRec->CentrDirSize;
    char* dest = (char*)CentrDir;
    unsigned i;
    unsigned o = ECRec->CentrDirOffs;
    while (left)
    {
        i = left > RASize - o ? RASize - o : left;
        MemCpy(dest, RAData + o, i);
        dest = dest + i;
        left -= i;
        if (left)
        {
            if (ChangeDisk(++DiskNum, FALSE))
                return 1;
            o = 0;
        }
    }
    return 0;
}

int ChangeDisk(int number, BOOL firstVolume)
{
    if (MVMode != MV_PROCESS_ARCHIVE)
    {
        Stop = true;
        return 1;
    }
    char file[MAX_PATH];
    char ext[MAX_PATH];

    if (RAData)
        UnmapFile(RAFile, RAMapping, RAData);
    RAData = NULL;
    if (ArchiveStart->Flags & SE_SEQNAMES)
    {
        SplitPath(RealArchiveName, file, ext);
        char* sour = file + lstrlen(file) - 1;
        while (sour > file)
        {
            if ((*sour < '0' || *sour > '9'))
                break;
            sour--;
        }
        if (sour >= file)
        {
            *(++sour) = 0;
            wsprintf(RealArchiveName, "%s%02d%s", file, number, ext);
        }
    }

    while (1)
    {
        //check if file exists
        DWORD attr = SalGetFileAttributes(RealArchiveName);
        if (!firstVolume && !(ArchiveStart->Flags & SE_SEQNAMES) || attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            char buf[1024];
            wsprintf(buf, StringTable[STR_INSERT_NEXT_DISK], number);
            if (MessageBox(DlgWin, buf, Title, MB_ICONQUESTION | MB_SETFOREGROUND | MB_OKCANCEL) == IDCANCEL)
            {
                Stop = true;
                CloseMapping();
                return 1;
            }
        }
        firstVolume = FALSE;

        int err;
        bool retry;
        //open the file
        CStringIndex ret;
        ret = MapFile(RealArchiveName, &RAFile, &RAMapping, &RAData, &RASize, &err);
        if (ret)
        {
            HandleError(ret, err, RealArchiveName, &retry, 0, true);
            if (retry)
                continue;
            else
            {
                Stop = true;
                CloseMapping();
                return 1;
            }
        }
        else
            return 0; //OK
    }
}

//password dialog

BOOL WINAPI PasswordDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static char* password;
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        password = ((CPasswordDlgInfo*)lParam)->Password;
        SendDlgItemMessage(DlgWin, IDC_PASSWORD, EM_SETLIMITTEXT, MAX_PASSWORD - 1, 0);
        OrigTextControlProc = (WNDPROC)SetWindowLong(GetDlgItem(hDlg, IDC_FILE), GWL_WNDPROC, (LONG)TextControlProc);
        SetDlgItemText(hDlg, IDC_FILE, ((CPasswordDlgInfo*)lParam)->File);
        char title[200];
        *title = 0;
        if (DlgWin == NULL)
        {
            lstrcpy(title, Title);
            lstrcat(title, " - ");
            CenterDialog(hDlg);
        }
        lstrcat(title, StringTable[STR_PASSWORDDLGTITLE]);
        SetWindowText(hDlg, title);
        SetForegroundWindow(hDlg);
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (GetDlgItemText(hDlg, IDC_PASSWORD, password, MAX_PASSWORD - 1) == 0)
                *password = 0;
            EndDialog(hDlg, IDOK);
            return FALSE;

        case IDC_SKIPALL:
            Silent |= SF_SKIPALLCRYPT;
        case IDC_SKIP:
            EndDialog(hDlg, IDC_SKIP);
            return FALSE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return FALSE;
        }

    case WM_DESTROY:
        SetWindowLong(GetDlgItem(hDlg, IDC_FILE), GWL_WNDPROC, (LONG)OrigTextControlProc);
        return FALSE;
    }
    return FALSE;
}

//crypt
//Return the next byte in the pseudo-random sequence
int decrypt_byte()
{
    unsigned temp;

    temp = ((unsigned)Keys[2] & 0xffff) | 2;
    return (int)(((temp * (temp ^ 1)) >> 8) & 0xff);
}

#define CRC32(c, b) (CrcTab[((int)(c) ^ (b)) & 0xff] ^ ((c) >> 8))

//Update the encryption keys with the next byte of plain text
int update_keys(int c)
{
    Keys[0] = CRC32(Keys[0], c);
    Keys[1] += Keys[0] & 0xff;
    Keys[1] = Keys[1] * 134775813L + 1;
    {
        register int keyshift = (int)(Keys[1] >> 24);
        Keys[2] = CRC32(Keys[2], keyshift);
    }
    return c;
}

//initialize the encryption keys and the random header according to
//the given password.
void init_keys(const char* password)
{
    Keys[0] = 305419896L;
    Keys[1] = 591751049L;
    Keys[2] = 878082192L;
    while (*password)
    {
        update_keys((int)*password);
        password++;
    }
}

int testkey(const char* password, const char* header, char check)
{
    char buf[ENCRYPT_HEADER_SIZE]; //decrypted header

    //set keys and save the encrypted header
    init_keys(password);
    MemCpy(buf, header, ENCRYPT_HEADER_SIZE);

    //decrypt header
    //Decrypt(buf, ENCRYPT_HEADER_SIZE);
    char* ptr = buf;
    while (ptr < buf + ENCRYPT_HEADER_SIZE)
        update_keys(*ptr++ ^= decrypt_byte());

    //check password
    if (buf[ENCRYPT_HEADER_SIZE - 1] == check)
        return 0;
    else
        return -1; // bad
}

//test password and set up keys
int InitKeys(const char* password, const char* header, char check)
{
    int ret;

    ret = testkey(password, header, check);

#ifdef CHECK_OUT_OEM_PASWORD
    if (ret)
    {
        char buffer[MAX_PASSWORD];

        CharToOem(password, buffer);
        ret = testkey(buffer, header, check);
    }
#endif

    return ret;
}

BOOL WaitingForFirst = TRUE;
DWORD* ProcessChache;
int PCSize;
int PCCount;
BOOL EnumProcOK;

BOOL EnumProcesses(HANDLE* processHandle)
{
    BOOL ret = FALSE;
    DWORD id = 0;

    if (!processHandle)
    {
        PCSize = 32 * 4;
        ProcessChache = (DWORD*)HeapAlloc(Heap, 0, PCSize);
        PCCount = 0;
    }
    if (!ProcessChache)
        return FALSE;

    HANDLE ss;
    if ((DWORD)(ss = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) != -1)
    {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        BOOL ok = Process32First(ss, &pe);
        while (ok)
        {
            TRACE1("nalezen process:")
            if ((DWORD)&pe.szExeFile - (DWORD)&pe < pe.dwSize)
            {
                TRACE1(pe.szExeFile)
                char* filename;
                filename = StrRChr(pe.szExeFile, pe.szExeFile + lstrlen(pe.szExeFile), '\\');
                if (filename)
                    filename++;
                else
                    filename = pe.szExeFile;
                if (lstrcmpi((char*)ArchiveStart + ArchiveStart->WaitForOffs, filename) == 0)
                {
                    if (processHandle)
                    {
                        int i;
                        for (i = 0; i < PCCount; i++)
                        {
                            if (ProcessChache[i] == pe.th32ProcessID)
                                break;
                        }
                        if (i == PCCount)
                        {
                            id = pe.th32ProcessID;
                            ret = TRUE;
                            break;
                        }
                    }
                    else
                    {
                        if (PCCount == PCSize)
                        {
                            PCSize += 4 * 32;
                            void* ptr = HeapReAlloc(Heap, 0, ProcessChache, PCSize);
                            break;
                            ProcessChache = (DWORD*)ptr;
                        }
                        ProcessChache[PCCount++] = pe.th32ProcessID;
                    }
                }
            }
            pe.dwSize = sizeof(PROCESSENTRY32);
            ok = Process32Next(ss, &pe);
        }
        if (!ok && GetLastError() == ERROR_NO_MORE_FILES)
            ret = TRUE;
        CloseHandle(ss);
    }
    if (ret && id)
    {
        *processHandle = OpenProcess(STANDARD_RIGHTS_REQUIRED | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, id);
        if (!*processHandle)
            ret = FALSE;
    }
    return ret;
}
