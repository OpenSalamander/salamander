// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "strings.h"
#include "comdefs.h"
#include "selfextr.h"
#include "extract.h"
#include "dialog.h"
#include "resource.h"
#include "inflate.h"
#include "extended.h"

HANDLE ArchiveFile;
HANDLE ArchiveMapping;
const unsigned char* ArchiveBase = NULL;
CSelfExtrHeader* ArchiveStart;
unsigned long Size;
char TargetPath[MAX_PATH];
int TargetPathLen;
bool Stop = false;
unsigned Silent = 0;
#ifdef EXT_VER
HANDLE OutFile = 0;
#else  //EXT_VER
HANDLE OutFile;
#endif //EXT_VER
__UINT32 Crc;
__UINT32 CrcTab[CRC_TAB_SIZE];
bool Extracting = false;
int ExtractedFiles = 0;
int SkippedFiles = 0;
__int64 ExtractedMB = 0;
;

void* MemCpy(void* dst, const void* src, unsigned count)
{
    void* ret = dst;
    if (dst <= src || (char*)dst >= ((char*)src + count))
    {
        /*
     * Non-Overlapping Buffers
     * copy from lower addresses to higher addresses
     */
        while (count--)
        {
            *(char*)dst = *(char*)src;
            dst = (char*)dst + 1;
            src = (char*)src + 1;
        }
    }
    else
    {
        /*
     * Overlapping Buffers
     * copy from higher addresses to lower addresses
     */
        dst = (char*)dst + count - 1;
        src = (char*)src + count - 1;

        while (count--)
        {
            *(char*)dst = *(char*)src;
            dst = (char*)dst - 1;
            src = (char*)src - 1;
        }
    }
    return (ret);
}

#pragma optimize("", off)
void* MemZero(void* dst, unsigned count)
{
    void* start = dst;

    while (count--)
    {
        *(char*)dst = (char)0;
        dst = (char*)dst + 1;
    }
    return (start);
}
#pragma optimize("", on)

//***********************************************************************************
//
// Rutiny ze SHLWAPI.DLL
//

BOOL PathAppend(LPTSTR pPath, LPCTSTR pMore)
{
    if (pMore[0] == 0)
    {
        return TRUE;
    }
    PathAddBackslash(pPath);
    if (*pMore == '\\')
        pMore++;
    lstrcat(pPath, pMore);
    return TRUE;
}

BOOL PathRemoveFileSpec(LPTSTR pszPath)
{
    int len = lstrlen(pszPath);
    char* iterator = pszPath + len - 1;
    while (iterator >= pszPath)
    {
        if (*iterator == '\\')
        {
            if (iterator - 1 < pszPath || *(iterator - 1) == ':')
                iterator++;
            *iterator = 0;
            break;
        }
        iterator--;
    }
    return TRUE;
}

LPTSTR StrRChr(LPCTSTR lpStart, LPCTSTR lpEnd, char wMatch)
{
    lpEnd--;
    while (lpEnd >= lpStart)
    {
        if (*lpEnd == wMatch)
            return (LPTSTR)lpEnd;
        lpEnd--;
    }
    return NULL;
}

LPTSTR StrChr(LPCTSTR lpStart, char wMatch)
{
    if (lpStart == NULL)
        return NULL;
    while (*lpStart)
    {
        if (*lpStart == wMatch)
            return (LPTSTR)lpStart;
        lpStart++;
    }
    return NULL;
}

LPTSTR PathAddBackslash(LPTSTR pszPath)
{
    if (pszPath == NULL)
        return NULL;
    int len = lstrlen(pszPath);
    if (len > 0 && pszPath[len - 1] != '\\')
    {
        pszPath[len] = '\\';
        pszPath[len + 1] = 0;
        len++;
    }
    return pszPath + len;
}

LPTSTR PathRemoveBackslash(LPTSTR pszPath)
{
    if (pszPath == NULL)
        return NULL;
    int len = lstrlen(pszPath);
    if (len > 0 && pszPath[len - 1] == '\\')
    {
        pszPath[len - 1] = 0;
        len--;
    }
    else if (len > 1)
        len--;
    return pszPath + len;
}

BOOL PathStripToRoot(LPTSTR pszPath)
{
    int len = lstrlen(pszPath);
    if (len < 2)
        return FALSE;
    if (pszPath[0] == '\\' && pszPath[1] == '\\') // UNC
    {
        char* s = pszPath + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        s++;
        *s = 0;
    }
    else
    {
        if (len > 3)
            pszPath[3] = 0;
    }
    len = lstrlen(pszPath);
    if (pszPath[len - 1] != '\\')
    {
        pszPath[len] = '\\';
        pszPath[len + 1] = 0;
    }
    return TRUE;
}

BOOL PathMerge(char* fullPath, const char* relativePath)
{
    char* destMax = fullPath + MAX_PATH - 1;
    char* dest;
    const char* sour = relativePath;

    PathAddBackslash(fullPath);
    if (relativePath[0] == '\\') //UNC or relative to root dir
    {
        if (relativePath[1] == '\\')
            *fullPath = 0; //UNC
        else
        {
            PathStripToRoot(fullPath); //root dir
            sour++;
        }
    }
    else
    {
        if (relativePath[1] == ':')
            *fullPath = 0; //drive letter
    }
    dest = fullPath + lstrlen(fullPath);

    while (*sour && dest < destMax)
    {
        if (*sour == '.' && (sour == relativePath || sour[-1] == '\\'))
        {
            if (sour[1] == '.' && (sour[2] == 0 || sour[2] == '\\'))
            {
                *dest = NULL;
                if (GetRootLen(fullPath) < lstrlen(fullPath))
                    dest = StrRChr(fullPath, dest - 1, '\\') + 1;
                sour += 2;
                if (*sour)
                    sour++; //preskocime jeste lomitko nejsme-li  na konci
                continue;
            }
            if (sour[1] == '\\')
            {
                sour += 2;
                continue;
            }
            if (sour[1] == 0)
            {
                break;
            }
        }
        *dest++ = *sour++;
    }
    *dest = NULL;
    PathRemoveBackslash(fullPath);
    return dest < destMax;
}

#pragma optimize("", off)
void i64toa(__int64 val, char* buf)
{
    char* p;         /* pointer to traverse string */
    char* firstdig;  /* pointer to first digit */
    char temp;       /* temp char */
    unsigned digval; /* value of digit */

    p = buf;

    firstdig = p; /* save pointer to first digit */

    do
    {
        digval = (unsigned)(val % 10);
        val /= 10; /* get next digit */

        /* convert to ascii and store */
        *p++ = (char)(digval + '0'); /* a digit */
    } while (val > 0);

    /* We now have the digit of the number in the buffer, but in reverse
     order.  Thus we reverse them now. */

    *p-- = '\0'; /* terminate string; p points to last digit */

    do
    {
        temp = *p;
        *p = *firstdig;
        *firstdig = temp; /* swap *p and *firstdig */
        --p;
        ++firstdig;         /* advance to next two digits */
    } while (firstdig < p); /* repeat until halfway */
}
#pragma optimize("", on)

char* NumberToStr(char* buffer, const __int64 number)
{
    char ThousandsSeparator[5];
    int ThousandsSeparatorLen;
    if ((ThousandsSeparatorLen = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, ThousandsSeparator, 5)) == 0 ||
        ThousandsSeparatorLen > 5)
    {
        ThousandsSeparator[0] = ' ';
        ThousandsSeparator[1] = 0;
        ThousandsSeparatorLen = 1;
    }
    else
    {
        ThousandsSeparatorLen--;
        ThousandsSeparator[ThousandsSeparatorLen] = 0; // posychrujeme nulu na konci
    }

    i64toa(number, buffer);
    int l = lstrlen(buffer);
    char* s = buffer + l;
    int c = 0;
    while (--s > buffer)
    {
        if ((++c % 3) == 0)
        {
            MemCpy(s + ThousandsSeparatorLen, s, (c / 3) * 3 + (c / 3 - 1) * ThousandsSeparatorLen + 1);
            MemCpy(s, ThousandsSeparator, ThousandsSeparatorLen);
        }
    }
    return buffer;
}

//void AddSlash(char * path)

int GetName(CFileHeader* header, char* name)
{
    char *sour, *dest;
    int i = 0, l;

#ifdef EXT_VER
    if ((char*)header + sizeof(CFileHeader) + header->NameLen >
        (char*)CentrDir + ECRec->CentrDirSize)
    {
        return HandleError(STR_ERROR_BADDATA, 0);
    }

#else  //EXT_VER
    if ((unsigned char*)header + sizeof(CFileHeader) + header->NameLen > ArchiveBase + Size)
    {
        return HandleError(STR_ERROR_BADDATA, 0);
    }
#endif //EXT_VER

    l = lstrlen(name);
    if (l + header->NameLen > MAX_PATH)
    {
        char buf[101];
        if (header->NameLen > 100)
        {
            //MemCpy(buf, (char *)header + sizeof(CFileHeader), 50);
            MemCpy(buf, (char*)header + sizeof(CFileHeader) + header->NameLen - 100, 100);
            buf[100] = 0;
        }
        else
        {
            MemCpy(buf, (char*)header + sizeof(CFileHeader), header->NameLen);
            buf[header->NameLen] = 0;
        }
        return HandleError(STR_ERROR_BADDATA, 0, buf, NULL, SF_TOOLONGNAMES);
    }
    dest = name + l;
    sour = (char*)header + sizeof(CFileHeader);
    if (*sour == '/')
    {
        sour++;
        i++;
    }
    for (; i < header->NameLen; i++)
    {
        if (*sour == '/')
        {
            *dest++ = '\\';
            sour++;
        }
        else
            *dest++ = *sour++;
    }
    if (*(dest - 1) == '\\') //remove last slash if name specifies a directory
        dest--;
    *dest = 0;
    if (header->Version >> 8 == HS_FAT ||
        header->Version >> 8 == HS_HPFS ||
        header->Version >> 8 == HS_NTFS && (header->Version & 0x0F) == 0x50)
        OemToChar(name + l, name + l);
    return 0;
}

int GetFirstFile(CFileHeader** header, unsigned* totalEntries, char* name)
{
#ifdef EXT_VER
    *header = CentrDir;
    if ((char*)*header + sizeof(CFileHeader) > (char*)CentrDir + ECRec->CentrDirSize ||
        (*header)->Signature != SIG_CENTRFH)
    {
        return HandleError(STR_ERROR_BADDATA, 0);
    }
    *totalEntries = ECRec->TotalEntries;
    if (name)
        return GetName(*header, name);
    return 0;

#else //EXT_VER
    CEOCentrDirRecord* ecrec = (CEOCentrDirRecord*)((char*)ArchiveStart +
                                                    ArchiveStart->HeaderSize +
                                                    ArchiveStart->EOCentrDirOffs);
    *header = (CFileHeader*)((char*)ArchiveStart + ArchiveStart->HeaderSize + ecrec->CentrDirOffs);
    if ((unsigned char*)*header + sizeof(CFileHeader) > ArchiveBase + Size ||
        (*header)->Signature != SIG_CENTRFH)
    {
        return HandleError(STR_ERROR_BADDATA, 0);
    }
    *totalEntries = ecrec->TotalEntries;
    if (name)
        return GetName(*header, name);
    return 0;

#endif //EXT_VER
}

int GetNextFile(CFileHeader** header, char* name)
{
    *header = (CFileHeader*)((char*)*header + (*header)->NameLen + (*header)->ExtraLen + (*header)->CommentLen + sizeof(CFileHeader));

#ifdef EXT_VER
    if ((char*)*header + sizeof(CFileHeader) > (char*)CentrDir + ECRec->CentrDirSize ||
        (*header)->Signature != SIG_CENTRFH)
    {
        return HandleError(STR_ERROR_BADDATA, 0);
    }

#else //EXT_VER
    if ((unsigned char*)*header + sizeof(CFileHeader) > ArchiveBase + Size ||
        (*header)->Signature != SIG_CENTRFH)
    {
        return HandleError(STR_ERROR_BADDATA, 0);
    }

#endif //EXT_VER

    if (name)
        return GetName(*header, name);
    return 0;
}

int GetRootLen(const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        return (s - path) + 1;
    }
    else
        return 3;
}

HANDLE CheckAndCreateDirectory(char* name, bool noSkip)
{
    if (GetRootLen(name) > lstrlen(name))
        return (HANDLE)1;
    DWORD attr = SalGetFileAttributes(name);
    if (attr == 0xFFFFFFFF || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    { /*
    char * slash = name + lstrlen(name) - 1;
    while (slash > name)
    {
      if (*slash == '\\') break;
      slash--;
    }
    if (slash > name)
    {*/
        char* slash = StrRChr(name, name + lstrlen(name) - 1, '\\');
        if (slash)
        {
            *slash = 0;
            if (CheckAndCreateDirectory(name) == INVALID_HANDLE_VALUE)
                return INVALID_HANDLE_VALUE;
            *slash = '\\';
        }
        while (1)
        {
            bool retry;
            if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                HandleError(STR_ERROR_DCREATE2, 0, name, &retry, SF_CREATEFILE, noSkip);
                if (!retry)
                    return INVALID_HANDLE_VALUE;
            }
            else
            {
                if (!CreateDirectory(name, NULL))
                {
                    HandleError(STR_ERROR_DCREATE, GetLastError(), name, &retry, SF_CREATEFILE, noSkip);
                    if (!retry)
                        return INVALID_HANDLE_VALUE;
                }
                else
                    break;
            }
            attr = SalGetFileAttributes(name);
        }
    }
    return (HANDLE)1;
}

DWORD SalGetFileAttributes(const char* fileName)
{
    int fileNameLen = (int)lstrlen(fileName);
    char fileNameCopy[3 * MAX_PATH];
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak GetFileAttributes mezery/tecky
    // orizne a pracuje tak s jinou cestou + u souboru to sice nefunguje, ale porad lepsi nez ziskat
    // atributy jineho souboru/adresare (pro "c:\\file.txt   " pracuje se jmenem "c:\\file.txt")
    if (fileNameLen > 0 && (fileName[fileNameLen - 1] <= ' ' || fileName[fileNameLen - 1] == '.') &&
        fileNameLen + 1 < _countof(fileNameCopy))
    {
        MemCpy(fileNameCopy, fileName, fileNameLen);
        fileNameCopy[fileNameLen] = '\\';
        fileNameCopy[fileNameLen + 1] = 0;
        return GetFileAttributes(fileNameCopy);
    }
    else // obycejna cesta, neni co resit, jen zavolame windowsovou GetFileAttributes
    {
        return GetFileAttributes(fileName);
    }
}

void GetInfo(char* buffer, FILETIME* lastWrite, unsigned size)
{
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        wsprintf(time, "%d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        wsprintf(date, "%d.%d.%d", st.wDay, st.wMonth, st.wYear);
    wsprintf(buffer, "%s, %s, %s", NumberToStr(number, size), date, time);
}

HANDLE SafeCreateFile(char* name, DWORD attr, FILETIME* time, unsigned size)
{
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        return CheckAndCreateDirectory(name);
    /*
  char * slash = name + lstrlen(name) - 1;
  while (slash > name)
  {
    if (*slash == '\\') break;
    slash--;
  }
  if (slash > name)
  {
  */
    char* slash = StrRChr(name, name + lstrlen(name) - 1, '\\');
    if (slash)
    {
        *slash = 0;
        if (CheckAndCreateDirectory(name) == INVALID_HANDLE_VALUE)
            return INVALID_HANDLE_VALUE;
        *slash = '\\';
    }
    DWORD create = CREATE_NEW;
    while (1)
    {
        DWORD attr;
        attr = SalGetFileAttributes(name);
        if (attr != 0xFFFFFFFF && attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            bool retry;
            HandleError(STR_ERROR_FCREATE2, 0, name, &retry, SF_CREATEFILE);
            if (!retry)
                return INVALID_HANDLE_VALUE;
            continue;
        }
        HANDLE file;
        file = CreateFile(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, create,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            unsigned err = GetLastError();
            if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS)
            {
                char attr1[101], attr2[101];
                FILETIME ft;
                //char *    buffer;
                file = CreateFile(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                  0, NULL);
                if (file == INVALID_HANDLE_VALUE)
                {
                    bool retry;
                    HandleError(STR_ERROR_FCREATE, err, name, &retry, SF_CREATEFILE);
                    if (!retry)
                        return INVALID_HANDLE_VALUE;
                }
                else
                {
                    GetFileTime(file, NULL, NULL, &ft);
                    GetInfo(attr2, &ft, GetFileSize(file, NULL));
                    GetInfo(attr1, time, size);
                    CloseHandle(file);
                    char buf[MAX_PATH * 2];
                    GetModuleFileName(NULL, buf, MAX_PATH);
                    lstrcat(buf, name + TargetPathLen - 1);
                    switch (OverwriteDialog(buf, attr1, name, attr2, attr))
                    {
                    case IDYES:
                        SetFileAttributes(name, FILE_ATTRIBUTE_NORMAL);
                        create = CREATE_ALWAYS;
                        break;
                    case IDC_SKIP:
                        return INVALID_HANDLE_VALUE;
                    case IDCANCEL:
                    default:
                        Stop = true;
                        return INVALID_HANDLE_VALUE;
                    }
                }
            }
            else
            {
                bool retry;
                HandleError(STR_ERROR_FCREATE, err, name, &retry, SF_CREATEFILE);
                if (!retry)
                    return INVALID_HANDLE_VALUE;
            }
        }
        else
            return file;
    }
}

int SafeWrite(HANDLE file, const unsigned char* buffer, unsigned size)
{
    unsigned long bytesWritten; //number of butes read by ReadFile()
    bool retry;
    unsigned filePos;
    LONG dummy;

    if (!size)
        return 0;
    while (1)
    {
        dummy = 0;
        filePos = SetFilePointer(file, 0, &dummy, FILE_CURRENT);
        if (filePos == 0xFFFFFFFF)
        {
            HandleError(STR_ERROR_ACCES, GetLastError(), TargetPath, &retry, SF_WRITEFILE);
            if (!retry)
                return 1;
        }
        else
            break;
    }
    while (1)
    {
        dummy = 0;
        if (SetFilePointer(file, filePos, &dummy, FILE_BEGIN) == 0xFFFFFFFF)
        {
            HandleError(STR_ERROR_ACCES, GetLastError(), TargetPath, &retry, SF_WRITEFILE);
            if (!retry)
                return 1;
        }
        else
        {
            if (WriteFile(file, buffer, size, &bytesWritten, NULL))
                return 0;
            else
            {
                HandleError(STR_ERROR_WRITE, GetLastError(), TargetPath, &retry, SF_WRITEFILE);
                if (!retry)
                    return 1;
            }
        }
    }
}

//fill up crc table
void MakeCrcTable()
{
    __UINT32 c;
    int n, k;
    const __UINT32 poly = 0xedb88320L; //polynomial exclusive-or pattern

    /*
  //generate crc polonomial, using precomputed poly should be faster
  // terms of polynomial defining this crc (except x^32):
  static const Byte p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  // make exclusive-or pattern from polynomial (0xedb88320L)
  poly = 0L;
  for (n = 0; n < sizeof(p)/sizeof(Byte); n++)
    poly |= 1L << (31 - p[n]);
*/
    for (n = 0; n < 256; n++)
    {
        c = (__UINT32)n;
        for (k = 0; k < 8; k++)
            c = c & 1 ? poly ^ (c >> 1) : c >> 1;
        CrcTab[n] = c;
    }
}

void UpdateCrc(__UINT8* buffer, unsigned length)
{
    register __UINT32 c; /* temporary variable */

    c = Crc ^ 0xffffffffL;
    while (length)
    {
        c = CrcTab[((int)c ^ (*buffer++)) & 0xff] ^ (c >> 8);
        length--;
    }
    /*
  if (length) do
  {
    c = crcTab[((int)c ^ (*buffer++)) & 0xff] ^ (c >> 8);
  } while (--length);
  */
    Crc = c ^ 0xffffffffL; /* (instead of ~c for 64-bit machines) */
}

int InflateFile(const unsigned char* data, unsigned size)
{
    InPtr = (unsigned char*)data;
    InEnd = InPtr + size;

#ifdef EXT_VER
    if (InEnd > RAData + RASize)
        InEnd = (unsigned char*)RAData + RASize;

#endif //EXT_VER

    switch (Inflate())
    {
    case 4:
#ifdef EXT_VER
        if (MVMode == MV_PROCESS_ARCHIVE)
            return 1;
#endif
    case 1:
    case 2:
        return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
    case 3:
        return HandleError(STR_LOWMEM, 0);
    case 5:
    case 6:
        return 1;
    }
    return 0;
}

int UnStoreFile(const unsigned char* data, unsigned size)
{
    unsigned toWrite;

    const unsigned char* ptr = data;
    unsigned left = size;

#ifdef EXT_VER
    unsigned leftMV = left;

    if (ptr + left > RAData + RASize)
        left = RASize - (ptr - RAData);

    while (leftMV)
    {
#endif

        while (left)
        {
            toWrite = left > WSIZE ? WSIZE : left;

#ifdef EXT_VER
            if (Encrypted)
            {
                unsigned i;
                for (i = 0; i < toWrite; i++)
                    update_keys(SlideWin[i] = ptr[i] ^ decrypt_byte());
                UpdateCrc(SlideWin, toWrite);
                if (SafeWrite(OutFile, SlideWin, toWrite))
                    return 1;
            }
            else
#endif
            {
                UpdateCrc((unsigned char*)ptr, toWrite);
                if (SafeWrite(OutFile, ptr, toWrite))
                    return 1;
            }
            if (DlgWin)
                SendMessage(ProgressWin, WM_USER_REFRESHPROGRESS, toWrite, 0);
            ptr += toWrite;
            left -= toWrite;

#ifdef EXT_VER
            leftMV -= toWrite;
#endif
            if (Stop)
                return 1;
        }

#ifdef EXT_VER
        leftMV -= left;
        if (leftMV)
        {
            if (ChangeDisk(++DiskNum, FALSE))
                return 1;
            ptr = RAData;
            left = leftMV > RASize ? RASize : leftMV;
        }
    }

#endif

    return 0;
}

/*
int InflateFile(CFileHeader *header)
{
#ifdef EXT_VER
  if (header->StartDisk + 1 != DiskNum && MVMode == MV_PROCESS_ARCHIVE)
  {
    if (ChangeDisk(header->StartDisk + 1)) return 1;
    DiskNum = header->StartDisk + 1;
  }
  InPtr = (unsigned char *) RAData + header->LocHeaderOffs;
  if (InPtr + sizeof(CLocalFileHeader) > RAData + RASize)
    return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
  InEnd = InPtr + sizeof(CLocalFileHeader) + ((CLocalFileHeader *)InPtr)->ExtraLen +
          ((CLocalFileHeader *)InPtr)->NameLen + header->CompSize;
  InPtr = InPtr + sizeof(CLocalFileHeader) + ((CLocalFileHeader *)InPtr)->ExtraLen + ((CLocalFileHeader *)InPtr)->NameLen;
  if (InEnd > RAData + RASize )
  {
    if (MVMode == MV_PROCESS_ARCHIVE)
    {
      InEnd = (unsigned char *) RAData + RASize;
    }
    else return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
  }

#else //EXT_VER
  InPtr = (unsigned char *)ArchiveStart + ArchiveStart->HeaderSize + header->LocHeaderOffs;
  if (InPtr + sizeof(CLocalFileHeader) > ArchiveBase + Size)
    return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
  InEnd = InPtr + sizeof(CLocalFileHeader) + ((CLocalFileHeader *)InPtr)->ExtraLen +
          ((CLocalFileHeader *)InPtr)->NameLen + header->CompSize;
  InPtr = InPtr + sizeof(CLocalFileHeader) + ((CLocalFileHeader *)InPtr)->ExtraLen + ((CLocalFileHeader *)InPtr)->NameLen;
  if (InEnd > ArchiveBase + Size)
    return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);

#endif //EXT_VER

  switch (Inflate())
  {
    case 4 : if (MVMode == MV_PROCESS_ARCHIVE) return 1;
    case 1 :
    case 2 : return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
    case 3 : return HandleError(STR_LOWMEM, 0);
    case 5 :
    case 6 : return 1;
  }
  return 0;
}

int UnStoreFile(CFileHeader *header)
{
  unsigned toWrite;

#ifdef EXT_VER
  if (header->StartDisk + 1 != DiskNum && MVMode == MV_PROCESS_ARCHIVE)
  {
    if (ChangeDisk(header->StartDisk + 1)) return 1;
    DiskNum = header->StartDisk + 1;
  }
  char * ptr = (char *)RAData + header->LocHeaderOffs;
  unsigned size;
  if (ptr + sizeof(CLocalFileHeader) > (char *)RAData + RASize)
    return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
  unsigned left = size = ((CLocalFileHeader *)ptr)->CompSize;
  ptr = ptr + sizeof(CLocalFileHeader) + ((CLocalFileHeader *)ptr)->ExtraLen + ((CLocalFileHeader *)ptr)->NameLen;
  if(ptr + size > (char *)RAData + RASize)
  {
    if (MVMode == MV_PROCESS_ARCHIVE)
    {
      size = RASize - (ptr - (char*)RAData);
    }
    else return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
  }

#else //EXT_VER
  char * ptr = (char *)ArchiveStart + ArchiveStart->HeaderSize + header->LocHeaderOffs;
  unsigned size;
  if (ptr + sizeof(CLocalFileHeader) > (char *)ArchiveBase + Size)
    return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
  size = ((CLocalFileHeader *)ptr)->CompSize;
  ptr = ptr + sizeof(CLocalFileHeader) + ((CLocalFileHeader *)ptr)->ExtraLen + ((CLocalFileHeader *)ptr)->NameLen;
  if(ptr + size > (char *)ArchiveBase + Size)
    return HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);

#endif //EXT_VER

#ifdef EXT_VER
  while (left)
  {
#endif

    UpdateCrc((unsigned char *)ptr, size);
    while (size)
    {
      toWrite = size > 16*1024 ? 16*1024 : size;
      if (SafeWrite(OutFile, ptr, toWrite)) return 1;
      if (DlgWin) SendMessage(ProgressWin, WM_USER_REFRESHPROGRESS, toWrite, 0);
      //Progress += toWrite;
      //InvalidateRect(ProgressWin, NULL, FALSE);
      //UpdateWindow(ProgressWin);
      ptr += toWrite;
      size -= toWrite;
      if (Stop) return 1;
    }

#ifdef EXT_VER
    left -= size;
    if (left)
    {
      if (ChangeDisk(++DiskNum)) return 1;
      ptr = (char *) RAData;
      size = left > RASize ? RASize : left;
    }
  }

#endif

  return 0;
}
*/

int Extract()
{
    CFileHeader* header;
    unsigned totalEntries;
    FILETIME ft, lft;
    int ret;
    bool ok;
    __int64 oldProgress;
    const unsigned char* data;
    unsigned size;

#ifdef EXT_VER
    char cryptHeader[ENCRYPT_HEADER_SIZE];
    char password[MAX_PASSWORD];
    *password = 0;
    char check;
#endif //EXT_VER

    SetCurrentDirectory(TargetPath);
    PathAddBackslash(TargetPath);
    TargetPathLen = lstrlen(TargetPath);
    MakeCrcTable();
    if (GetFirstFile(&header, &totalEntries, TargetPath))
        return 1;
    while (1)
    {
        oldProgress = Progress;
        Crc = INIT_CRC;
        DosDateTimeToFileTime(header->Date, header->Time, &lft);
        LocalFileTimeToFileTime(&lft, &ft);
        if (!(header->ExternAttr & FILE_ATTRIBUTE_DIRECTORY) && DlgWin)
            SendMessage(DlgWin, WM_USER_REFRESHFILENAME, (WPARAM)(TargetPath + TargetPathLen), 0);
        OutFile = SafeCreateFile(TargetPath, header->ExternAttr & 0x0000FFFF, &ft, header->Size);
        if (header->ExternAttr & FILE_ATTRIBUTE_DIRECTORY)
        {
            //ExtractedFiles++; budem pocitat i adresare jakou soubory do vysledku?
            OutFile = INVALID_HANDLE_VALUE;
            ok = true; //don't erase the file
            goto next;
        }
        if (OutFile == INVALID_HANDLE_VALUE)
        {
            SkippedFiles++;
            ok = true; //don't erase the file
            goto next;
        }
        if (header->CompSize == 0)
        {
            ExtractedFiles++;
            ok = true; //don't erase the file
            goto next;
        }
        ok = false;

#ifdef EXT_VER
        if (header->StartDisk + 1 != DiskNum && MVMode == MV_PROCESS_ARCHIVE)
        {
            if (ChangeDisk(header->StartDisk + 1, FALSE))
                return 1;
            DiskNum = header->StartDisk + 1;
        }
        data = RAData + header->LocHeaderOffs;
        if (data + sizeof(CLocalFileHeader) > RAData + RASize)
        {
            HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
            goto next;
        }
        size = header->CompSize;
        check = header->Flag & GPF_DATADESCR ? ((CLocalFileHeader*)data)->Time >> 8 : header->Crc >> 24;
        data = data + sizeof(CLocalFileHeader) + ((CLocalFileHeader*)data)->ExtraLen + ((CLocalFileHeader*)data)->NameLen;
        if (data + size > RAData + RASize && MVMode != MV_PROCESS_ARCHIVE)
        {
            HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
            goto next;
        }

#else //EXT_VER
        data = (const unsigned char*)ArchiveStart + ArchiveStart->HeaderSize + header->LocHeaderOffs;
        if (data + sizeof(CLocalFileHeader) > ArchiveBase + Size)
        {
            HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
            goto next;
        }
        size = header->CompSize;
        data = data + sizeof(CLocalFileHeader) + ((CLocalFileHeader*)data)->ExtraLen + ((CLocalFileHeader*)data)->NameLen;
        if (data + size > ArchiveBase + Size)
        {
            HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
            goto next;
        }

#endif //EXT_VER

        if (header->Flag & GPF_ENCRYPTED)
        {
#ifdef EXT_VER
            if (Silent & SF_SKIPALLCRYPT)
                goto next;
            int left = ENCRYPT_HEADER_SIZE;
            unsigned i = RASize - (data - RAData);
            if (ENCRYPT_HEADER_SIZE > i)
            {
                MemCpy(cryptHeader, data, i);
                left = ENCRYPT_HEADER_SIZE - i;
                if (MVMode == MV_PROCESS_ARCHIVE)
                {
                    if (ChangeDisk(header->StartDisk + 1, FALSE))
                        goto next;
                    DiskNum = header->StartDisk + 1;
                    data = RAData;
                }
                else
                {
                    HandleError(STR_ERROR_BADDATA, 0, TargetPath + TargetPathLen, NULL, SF_BADDATA);
                    goto next;
                }
            }
            MemCpy(cryptHeader, data, left);
            data += left;
            size -= ENCRYPT_HEADER_SIZE;
            while (InitKeys(password, cryptHeader, check))
            {
                CPasswordDlgInfo info;
                info.File = TargetPath + TargetPathLen;
                info.Password = password;
                switch (DialogBoxParam(HInstance, MAKEINTRESOURCE(IDD_PASSWORD),
                                       DlgWin, PasswordDlgProc, (LPARAM)&info))
                {
                case IDOK:
                    break;
                case -1:
                case IDCANCEL:
                    Stop = true;
                default:
                    goto next;
                }
            }
            Encrypted = true;

#else  //EXT_VER

            HandleError(STR_ERROR_ENCRYPT, 0, TargetPath + TargetPathLen, NULL, SF_SKIPALLCRYPT);
            goto next;
#endif //EXT_VER
        }
        switch (header->Method)
        {
        case CM_DEFLATED:
            ret = InflateFile(data, size);
            break;
        case CM_STORED:
            ret = UnStoreFile(data, size);
            break;
        default:
            ret = HandleError(STR_ERROR_METHOD, 0, TargetPath + TargetPathLen, NULL, SF_BADMETHOD);
        }
        if (!ret)
        {
            if (Crc != header->Crc)
                HandleError(STR_ERROR_CRC, 0, TargetPath + TargetPathLen, 0, SF_BADDATA);
            else
            {
                ok = true;
                ExtractedFiles++;
                ExtractedMB += header->Size;
            }
        }
        SetFileTime(OutFile, &ft, NULL, &ft);

    next:
        if (OutFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(OutFile);
            if (ok)
                SetFileAttributes(TargetPath, header->ExternAttr & 0x0000FFFF);
        }
#ifdef EXT_VER
        OutFile = 0;
#endif

        if (!ok)
        {
            DeleteFile(TargetPath);
            SkippedFiles++;
        }

        *(TargetPath + TargetPathLen) = 0;
        if (Stop)
            break;
        if (DlgWin)
        {
            Progress = oldProgress + header->Size + 1;
            InvalidateRect(ProgressWin, NULL, FALSE);
            UpdateWindow(ProgressWin);
        }
#ifdef _DEBUG
        //Sleep(1000);
#endif
        if (!--totalEntries)
            break;
        if (GetNextFile(&header, TargetPath))
            return 1;
    }
    TargetPathLen--;
    *(TargetPath + TargetPathLen) = 0;
    FreeFixedHufman();
    if (Stop)
        return 1;
    else
        return 0;
}

DWORD WINAPI ExtractThreadProc(LPVOID lpParameter)
{
    Extracting = true;
    int ret = Extract();
    if (DlgWin)
        EndDialog(DlgWin, ret);
    Extracting = false;
    return ret;
}
