// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>
#include <tchar.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr/comdefs.h"
#include "config.h"
#include "typecons.h"
#include "chicon.h"
#include "common.h"
#include "add_del.h"
#include "sfxmake/sfxmake.h"
#include "inflate.h"

#ifndef SSZIP
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"
#include "dialogs.h"
#else //SSZIP
#include "sszip/dialogs.h"
#endif //SSZIP

// ****************************************************************************

const CConfiguration DefConfig =
    {
        6,                            // Compression level
        EM_ZIP20,                     // Encryption Method
        false,                        //don't add empty directories to zip
        true,                         //create temporary backup of zip
        true,                         //display exteded pack options dialog
        false,                        //set zip file time to the newest file time
        {"1423", {0}, {0}, {0}, {0}}, //volume sizes
        {0, 0, 0, 0, 0},              //volume size units
        true,                         //automatic volume size last used
        false,                        //automatically expand multi-volume archives on non-removable disks
        0,                            //config version (0 - default; 1 - beta 3; 2 - beta 4)
        "english.sfx",                //default sfx package
        "",                           //default path to export sfx settings to
        0,                            //current version of Altap Salamnder, bude nastaveno jinde
        CLR_ASK,                      // ChangeLangReaction, viz CLR_xxx
        TRUE,                         // winzip compatible multi-volume archive names
        // Custom columns:
        TRUE, // Show custom column Packed Size
        0,    // LO/HI-WORD: left/right panel: Width for Packed Size column
        0     // LO/HI-WORD: left/right panel: FixedWidth for Packed Size column
};

const CExtendedOptions DefOptions;

CConfiguration Config;

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

#ifndef SSZIP
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    CALL_STACK_MESSAGE_NONE
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;
    }
    return TRUE; // DLL can be loaded
}
#endif //SSZIP

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

WCHAR* LoadStrW(int resID)
{
    return SalamanderGeneral->LoadStrW(HLanguage, resID);
}

/*
BOOL SalGetTempFileName(const char *path, const char *prefix, char *tmpName, BOOL file)
{
  CALL_STACK_MESSAGE4("SalGetTempFileName(%s, %s, , %d)", path, prefix, file);
  char tmpDir[MAX_PATH + 10];
  char *end = tmpDir + MAX_PATH + 10;
  if (path == NULL)
  {
    if (!GetTempPath(MAX_PATH, tmpDir))
    {
      DWORD err = GetLastError();
      TRACE_E("Unable to get TEMP directory.");
      SetLastError(err);
      return FALSE;
    }
  }
  else strcpy(tmpDir, path);

  char *s = tmpDir + strlen(tmpDir);
  if (s > tmpDir && *(s - 1) != '\\') *s++ = '\\';
  while (s < end && *prefix != 0) *s++ = *prefix++;

  if (s - tmpDir < MAX_PATH - 10)  // dost mista pro pripojeni "XXXX.tmp"
  {
    DWORD randNum = (GetTickCount() & 0xFFF);
    while (1)
    {
      sprintf(s, "%X.tmp", randNum++);
      if (file)  // soubor
      {
        HANDLE h = CreateFile(tmpDir, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
          CloseHandle(h);
          strcpy(tmpName, tmpDir);   // nakopirujeme vysledek
          return TRUE;
        }
      }
      else  // adresar
      {
        if (CreateDirectory(tmpDir, NULL))
        {
          strcpy(tmpName, tmpDir);   // nakopirujeme vysledek
          return TRUE;
        }
      }
      DWORD err = GetLastError();
      if (err != ERROR_FILE_EXISTS && err != ERROR_ALREADY_EXISTS)
      {
        TRACE_E("Unable to create temporary " << (file ? "file" : "directory") <<
                ": " << err);
        SetLastError(err);
        return FALSE;
      }
    }
  }
  else
  {
    TRACE_E("Too long file name in SalGetTempFileName().");
    SetLastError(ERROR_BUFFER_OVERFLOW);
    return FALSE;
  }
}
*/

//CExtInfo
CExtInfo::CExtInfo(LPCTSTR pName, bool isDir, int nItem)
{
    CALL_STACK_MESSAGE_NONE
    Name = _tcsdup(pName);
    IsDir = isDir;
    ItemNumber = nItem;
}

CExtInfo::~CExtInfo()
{
    CALL_STACK_MESSAGE_NONE
    if (Name)
        free(Name);
}

bool IsUTF8Encoded(const char* s, int len)
{
    int nUTF8 = 0;

    while (len-- > 0)
    {
        if (*s & 0x80)
        {
            if ((*s & 0xe0) == 0xc0)
            {
                if (!s[1])
                {
                    if (len)
                    { // incomplete 2-byte sequence
                        return false;
                    }
                    break;
                }
                else if ((s[1] & 0xc0) != 0x80)
                {
                    return false; // not in UCS2
                }
                else
                {
                    nUTF8++;
                    s += 2;
                    len--;
                }
            }
            else if ((*s & 0xf0) == 0xe0)
            {
                if (!s[1] || !s[2])
                {
                    if (len > 1)
                    { // incomplete 3-byte sequence
                        return false;
                    }
                    break;
                }
                else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80)
                {
                    return false; // not in UCS2
                }
                else
                {
                    nUTF8++;
                    s += 3;
                    len -= 2;
                }
            }
            else
            {
                return false; // not in UCS2
            }
        }
        else
        {
            s++;
        }
    }

    if (nUTF8 > 0)
    { // At least one 2-or-more-bytes UTF8 sequence found and no invalid sequences found
        return true;
    }
    return false;
}

//CZipCommon

CZipCommon::CZipCommon(const char* zipName, const char* zipRoot,
                       CSalamanderForOperationsAbstract* salamander,
                       TIndirectArray2<char>* archiveVolumes)
{
    CALL_STACK_MESSAGE3("CZipCommon::CZipCommon(%s, %s, )", zipName, zipRoot);
    Config = ::Config;
    ZipFile = 0;
    lstrcpy(ZipName, zipName);
    ZipRoot = zipRoot;
    RootLen = lstrlen(ZipRoot);
    ZeroZip = false;
    Zip64 = false;
    Salamander = salamander;
    ErrorID = 0;
    Fatal = false;
    UserBreak = false;
    Comment = NULL;
    Unix = FALSE;
    ArchiveVolumes = archiveVolumes;

    DWORD ret = GetCurrentDirectory(MAX_PATH + 1, OriginalCurrentDir);
    if (!ret || ret > MAX_PATH + 1)
        *OriginalCurrentDir = 0;
}

CZipCommon::~CZipCommon()
{
    CALL_STACK_MESSAGE1("CZipCommon::~CZipCommon()");

    if (ZipFile)
        CloseCFile(ZipFile);
    if (*OriginalCurrentDir)
        SetCurrentDirectory(OriginalCurrentDir);
    if (Comment)
        free(Comment);
}

int CZipCommon::CheckZip()
{
    CALL_STACK_MESSAGE1("CZipCommon::CheckZip()");

    /*
  if (writeAcess)
    ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL);
  else
    ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL);
  if (ret)
    if (ret == ERR_LOWMEM)
      errorID = IDS_LOWMEM;
    else
      errorID = IDS_NODISPLAY;
  else
  */
    if (ZipFile->Size) //if zero sized zip file, don't do anything
    {
        ErrorID = CheckZipFormat();
        if (!ErrorID)
            ErrorID = FindEOCentrDirSig(NULL);
        if (!ErrorID)
            ErrorID = FindZip64EOCentrDirLocator();
        //    if (!ErrorID && Zip64 && !Extract) ErrorID = IDS_ERRZIP64;
        if (!ErrorID)
            ErrorID = CheckForExtraBytes();
    }
    else
        ZeroZip = true;
    /*if (ErrorID)
  {
    CloseCFile(ZipFile);
    ZipFile = 0;
  }
  */
    return ErrorID;
}

int CZipCommon::Read(CFile* file, void* buffer, unsigned bytesToRead,
                     unsigned* bytesRead, bool* skipAll)
{
    CALL_STACK_MESSAGE_NONE // casove kriticka metoda
                            //  CALL_STACK_MESSAGE3("CZipCommon::Read(, , 0x%X, , ) file: %s", bytesToRead, file->FileName);
        unsigned long read; //number of butes read by ReadFile()
    unsigned long toRead;   //number of butes read by ReadFile()
    int result;             //temp variable
    int errorID;            //error string identifier
    int lastError;          //value returned by GetLastError()

    if (file->Size > file->FilePointer)
        if (file->Size - file->FilePointer > bytesToRead)
            toRead = bytesToRead;
        else
            toRead = unsigned(file->Size - file->FilePointer);
    else
        toRead = 0;
    if (toRead != bytesToRead && !bytesRead)
        return ProcessError(IDS_EOF, 0, file->FileName, file->Flags | PE_NORETRY, skipAll);
    if (toRead)
    {
        if (file->InputBuffer != NULL && toRead <= INPUT_BUFFER_SIZE)
        {
            // cteni provadime pres cache
            // RealFilePointer ukazuje v souboru na misto, odkud je nacten buffer
            // BufferPosition je pocet validnich bajtu v bufferu

            if (file->FilePointer > file->RealFilePointer &&
                file->FilePointer + toRead <= file->RealFilePointer + file->BufferPosition)
            {
                int offset = int(file->FilePointer - file->RealFilePointer);
                // pokud jsou pozadovana data kompletne v bufferu, pouze je prekopirujeme
                memcpy(buffer, file->InputBuffer + offset, toRead);
                file->FilePointer += toRead;
                if (bytesRead)
                    *bytesRead = toRead;
                return 0; //OK
            }

            // data nejsou v bufferu vubec nebo tam nejsou cela, jdeme je nacist
            while (1)
            {
                LONG distHi = HIDWORD(file->FilePointer);
                if (SetFilePointer(file->File, LODWORD((DWORD)(file->FilePointer & 0x00000000FFFFFFFF)), &distHi, FILE_BEGIN) != 0xFFFFFFFF ||
                    GetLastError() == NO_ERROR)
                {
                    file->RealFilePointer = file->FilePointer;
                    unsigned long myToRead = max(toRead, INPUT_BUFFER_SIZE);
                    if ((result = ReadFile(file->File, file->InputBuffer, myToRead, &read, NULL)) != 0 && toRead <= read)
                    {
                        file->BufferPosition = read;
                        memcpy(buffer, file->InputBuffer, toRead);
                        file->FilePointer += toRead;
                        if (bytesRead)
                            *bytesRead = toRead;
                        return 0; //OK
                    }
                    else if (result && toRead != read)
                    {
                        Fatal = true;
                        errorID = IDS_EOF; //end of file reached
                        file->Flags |= PE_NORETRY;
                    }
                    else
                        errorID = IDS_ERRREAD;
                }
                else
                    errorID = IDS_ERRACCESS;

                lastError = GetLastError();
                result = ProcessError(errorID, lastError, file->FileName, file->Flags, skipAll);
                if (result != ERR_RETRY)
                    return result;
            }
        }
        else
        {
            // puvodni necachovane cteni
            while (1)
            {
                LONG distHi = HIDWORD(file->FilePointer);
                if (SetFilePointer(file->File, LODWORD((DWORD)(file->FilePointer & 0x00000000FFFFFFFF)), &distHi, FILE_BEGIN) != 0xFFFFFFFF ||
                    GetLastError() == NO_ERROR)
                {
                    if ((result = ReadFile(file->File, buffer, toRead, &read, NULL)) != 0 && toRead == read)
                    {
                        file->FilePointer += toRead;
                        if (bytesRead)
                            *bytesRead = toRead;
                        return 0; //OK
                    }
                    else if (result && toRead != read)
                    {
                        Fatal = true;
                        errorID = IDS_EOF; //end of file reached
                        file->Flags |= PE_NORETRY;
                    }
                    else
                        errorID = IDS_ERRREAD;
                }
                else
                    errorID = IDS_ERRACCESS;

                lastError = GetLastError();
                result = ProcessError(errorID, lastError, file->FileName, file->Flags, skipAll);
                if (result != ERR_RETRY)
                    return result;
            }
        }
    }
    else
    {
        if (bytesRead)
            *bytesRead = 0;
        return 0; //OK
    }
}

int CZipCommon::Write(CFile* file, const void* buffer, unsigned bytesToWrite,
                      bool* skipAll)
{
    CALL_STACK_MESSAGE_NONE // casove kriticka metoda
                            //  CALL_STACK_MESSAGE3("CZipCommon::Write(, , 0x%X, ) file: %s", bytesToWrite, file->FileName);
        int result;         //temp variable
    int errorID = 0;        //error string identifier

    if (file->RealFilePointer + file->BufferPosition != file->FilePointer)
    {
        result = Flush(file, file->OutputBuffer, file->BufferPosition, skipAll);
        if (result)
            return result;
        file->BufferPosition = 0;
        while (1)
        {
            LONG distHi = HIDWORD(file->FilePointer);
            if (SetFilePointer(file->File, LODWORD((DWORD)(file->FilePointer & 0x00000000FFFFFFFF)), &distHi, FILE_BEGIN) == 0xFFFFFFFF &&
                GetLastError() != NO_ERROR)
            {
                result = ProcessError(IDS_ERRACCESS, GetLastError(), file->FileName, file->Flags, skipAll);
                if (result != ERR_RETRY)
                    return result;
            }
            else
            {
                file->RealFilePointer = file->FilePointer;
                break;
            }
        }
    }
    if (file->BufferPosition + bytesToWrite <= OUTPUT_BUFFER_SIZE)
    {
        memcpy(file->OutputBuffer + file->BufferPosition, buffer, bytesToWrite);
        file->BufferPosition += bytesToWrite;
    }
    else
    {
        if (bytesToWrite >= OUTPUT_BUFFER_SIZE + OUTPUT_BUFFER_SIZE - file->BufferPosition)
        {
            result = Flush(file, file->OutputBuffer, file->BufferPosition, skipAll);
            if (result)
                return result;
            file->BufferPosition = 0;
            result = Flush(file, buffer, bytesToWrite, skipAll);
            if (result)
                return result;
        }
        else
        {
            memcpy(file->OutputBuffer + file->BufferPosition, buffer, OUTPUT_BUFFER_SIZE - file->BufferPosition);
            result = Flush(file, file->OutputBuffer, OUTPUT_BUFFER_SIZE, skipAll);
            if (result)
                return result;
            memcpy(file->OutputBuffer, (char*)buffer + OUTPUT_BUFFER_SIZE - file->BufferPosition,
                   bytesToWrite - (OUTPUT_BUFFER_SIZE - file->BufferPosition));
            file->BufferPosition = bytesToWrite - (OUTPUT_BUFFER_SIZE - file->BufferPosition);
        }
    }
    if (!file->BigFile &&
        file->FilePointer + bytesToWrite > 0xFFFFFFFF)
    {
        return ProcessError(IDS_TOOBIG2, 0, file->FileName, file->Flags | PE_NORETRY, skipAll);
    }
    file->FilePointer += bytesToWrite;
    return 0;
}

int CZipCommon::Flush(CFile* file, const void* buffer, unsigned bytesToWrite,
                      bool* skipAll)
{
    CALL_STACK_MESSAGE3("CZipCommon::Flush(, , 0x%X, ) file: %s", bytesToWrite, file->FileName);
    unsigned long bytesWritten; //number of butes read by ReadFile()
    int result;                 //temp variable
    int errorID = 0;            //error string identifier

    if (!bytesToWrite)
        return 0;
    if (!file->BigFile &&
        file->RealFilePointer + bytesToWrite > 0xFFFFFFFF)
    {
        return ProcessError(IDS_TOOBIG2, 0, file->FileName, file->Flags | PE_NORETRY, skipAll);
    }
    while (1)
    {
        LONG distHi = HIDWORD(file->RealFilePointer);
        if (SetFilePointer(file->File, LODWORD((DWORD)(file->RealFilePointer & 0x00000000FFFFFFFF)), &distHi, FILE_BEGIN) == 0xFFFFFFFF &&
            GetLastError() != NO_ERROR)
        {
            result = ProcessError(IDS_ERRACCESS, GetLastError(), file->FileName, file->Flags, skipAll);
            if (result != ERR_RETRY)
                return result;
        }
        else
        {
            if (WriteFile(file->File, buffer, bytesToWrite, &bytesWritten, NULL))
            {
                file->RealFilePointer += bytesWritten;
                return 0; //OK
            }
            else
            {
                result = ProcessError(IDS_ERRWRITE, GetLastError(), file->FileName, file->Flags, skipAll);
                if (result != ERR_RETRY)
                    return result;
            }
        }
    }
}

int CZipCommon::CreateCFile(CFile** file, LPCTSTR fileName, unsigned int access,
                            unsigned int share, unsigned int creation, unsigned int attributes,
                            int flags, bool* skipAll, bool bigFile, bool useReadCache)
{
    CALL_STACK_MESSAGE8("CZipCommon::CreateCFile(, %s, 0x%X, 0x%X, 0x%X, 0x%X, "
                        "%d, , %d)",
                        fileName, access, share, creation, attributes,
                        flags, useReadCache);
    int result; //temp variable
    int errorID;
    DWORD lastError; //value returned by GetLastError()
    int flagsNoRetry;
    CQuadWord size;

    if ((*file = (CFile*)malloc(sizeof(CFile))) == NULL ||
        ((*file)->FileName = _tcsdup(fileName)) == NULL)
    {
        if (*file)
        {
            free(*file);
            *file = NULL;
        }
        return ERR_LOWMEM;
    }
    (*file)->OutputBuffer = NULL;
    (*file)->InputBuffer = NULL;
    if (access & GENERIC_WRITE &&
        ((*file)->OutputBuffer = (char*)malloc(OUTPUT_BUFFER_SIZE)) == NULL)
    {
        free((*file)->FileName);
        free(*file);
        *file = NULL;
        return ERR_LOWMEM;
    }
    else
    {
        if ((access & GENERIC_READ) && !(access & GENERIC_WRITE) && useReadCache)
        {
            // varianta cachovaneho cteni i zapisu soucasne neni podporena

            // mame cist pres cache, alokujeme ji ted
            (*file)->InputBuffer = (char*)malloc(INPUT_BUFFER_SIZE);
            // pokud alokace nedopadne, nic se nedeje, fungujeme i bez ni
        }

        for (;;)
        {
            flagsNoRetry = 0;
            (*file)->File = CreateFile(fileName, access, share, NULL, creation, attributes, NULL);
            if ((*file)->File != INVALID_HANDLE_VALUE)
            {
                (*file)->FilePointer = 0;
                (*file)->RealFilePointer = 0;
                (*file)->Flags = flags;
                (*file)->BufferPosition = 0;
                //(*file)->InputPosition = 0;
                (*file)->BigFile = bigFile ? 1 : 0;
                if (SalamanderGeneral->SalGetFileSize((*file)->File, size, lastError))
                {
                    (*file)->Size = size.Value;
                    if (!bigFile && (*file)->Size > 0xFFFFFFFF)
                    {
                        errorID = IDS_TOOBIG;
                        flagsNoRetry = PE_NORETRY;
                        lastError = 0;
                    }
                    else
                        return 0; //OK
                }
                else
                    errorID = IDS_ERRACCESS;
                CloseHandle((*file)->File);
            }
            else
            {
                errorID = IDS_ERRCREATE;
                lastError = GetLastError();
            }
            result = ProcessError(errorID, lastError, fileName, flags | flagsNoRetry, skipAll);
            if (result != ERR_RETRY)
            {
                free((*file)->FileName);
                if (access & GENERIC_WRITE)
                    free((*file)->OutputBuffer);
                if ((*file)->InputBuffer != NULL)
                    free((*file)->InputBuffer);
                free(*file);
                *file = NULL;
                return result;
            }
        }
    }
}

int CZipCommon::CloseCFile(CFile* file)
{
    CALL_STACK_MESSAGE1("CZipCommon::CloseCFile()");
    CloseHandle(file->File);
    free(file->FileName);
    if (file->OutputBuffer)
        free(file->OutputBuffer);
    if (file->InputBuffer)
        free(file->InputBuffer);
    free(file);
    return 0;
}

int CZipCommon::ProcessError(int errorID, int lastError, const char* fileName,
                             int flags, bool* skipAll, char* extText)
{
    CALL_STACK_MESSAGE5("CZipCommon::ProcessError(%d, %d, %s, %d, , )", errorID,
                        lastError, fileName, flags);
    char errorBuf[1024]; //error text to display
    int exitCode;
    int result; //temp variable

    if (skipAll && *skipAll)
        exitCode = ERR_SKIP;
    else
    { //process error and display proper dialog
        if (lastError)
        {
            char lastErrorBuf[1024]; //temp variable
            *lastErrorBuf = 0;
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                              FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL, lastError,
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                          lastErrorBuf, 1024, NULL);
            if (extText)
                sprintf(errorBuf, "%s\n%s\n%s", LoadStr(errorID), lastErrorBuf, extText);
            else
                sprintf(errorBuf, "%s\n%s", LoadStr(errorID), lastErrorBuf);
        }
        else
        {
            if (extText)
                sprintf(errorBuf, "%s\n%s", LoadStr(errorID), extText);
            else
                sprintf(errorBuf, "%s", LoadStr(errorID));
        }

        if (flags & PE_QUIET)
            result = DIALOG_CANCEL;
        else //display dialog
            if (flags & PE_NORETRY)
                if (flags & PE_NOSKIP)
                {
                    SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_OK, fileName, errorBuf, NULL);
                    result = DIALOG_CANCEL;
                }
                else
                    result = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, fileName, errorBuf, NULL);
            else if (flags & PE_NOSKIP)
                result = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, fileName, errorBuf, NULL);
            else
                result = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, fileName, errorBuf, NULL);

        switch (result)
        {
        case DIALOG_RETRY:
            exitCode = ERR_RETRY;
            break;
        case DIALOG_SKIP:
            exitCode = ERR_SKIP;
            break;
        case DIALOG_SKIPALL:
            if (skipAll)
                *skipAll = true;
            exitCode = ERR_SKIP;
            break;
        case DIALOG_CANCEL:
            exitCode = ERR_CANCEL;
            break;
        case DIALOG_FAIL:
            TRACE_E("FileRead: cannot display DialogErrorX()");
            exitCode = ERR_CANCEL;
            break;
        default:
            TRACE_E("FileRead: unexpected value returned by DialogErrorX() " << result);
            exitCode = ERR_CANCEL;
        }
    }
    return exitCode;
}

int CZipCommon::CheckZipFormat()
{
    CALL_STACK_MESSAGE1("CZipCommon::CheckZipFormat()");
    //nedelam kontrolu ridim se pouze podle EOCentrDir na konci archivu
    //a kdyz ten nenajdu predpokladam, ze to je vicesvazkovy archiv

    if (ZipFile->Size >= 22)
        return 0;
    else
        return IDS_ERRFORMAT;

    /*
  __UINT32        signature;
  int           error = 0;
  
  if (ZipFile->Size >= 22)
  {
    if (Read(ZipFile, &signature, sizeof(signature), NULL, NULL))
    {
      error = IDS_NODISPLAY;
    }
    else
    {
      if ( signature == SIG_LOCALFH ||
           signature == SIG_EOCENTRDIR)
        error = 0;//ok
      else
      {
        error = IDS_ERRFORMAT;
      }
    }
  }
  else
  {
    error = IDS_ERRFORMAT;
  }
  if (error == IDS_ERRFORMAT)
    Fatal = true;
  return error;
  */
}

int CZipCommon::FindEOCentrDirSig(BOOL* success)
{
    CALL_STACK_MESSAGE1("CZipCommon::FindEOCentrDirSig()");
    const int bufSize = 1024;
    char buffer[bufSize];
    QWORD position;
    __UINT32* signature;
    int error = 0;
    int found = 0;
    QWORD i;
    unsigned size;
    bool retry;
    char lastFile[MAX_PATH];

    do
    {
        retry = false;
        //search from the end of file data
        if (ZipFile->Size < bufSize)
        {
            i = 0;
            position = 0;
        }
        else
        {
            position = ZipFile->Size - bufSize;
            i = (ZipFile->Size - 3) / (bufSize - 3);
        }
        while (i && !error && !found && position + 64 * 1024 + 22 >= ZipFile->Size)
        {
            ZipFile->FilePointer = position;
            if (Read(ZipFile, buffer, bufSize, NULL, NULL))
            {
                TRACE_E("Error reading to buffer");
                error = IDS_NODISPLAY;
            }
            else
            {
                for (signature = (__UINT32*)(buffer + bufSize - 4);
                     signature >= (__UINT32*)buffer;
                     signature = (__UINT32*)((char*)signature - 1))
                    if (*signature == SIG_EOCENTRDIR &&
                        position + ((char*)signature - buffer) < ZipFile->Size - 21)
                    {
                        EOCentrDirOffs = position + ((char*)signature - buffer);

                        if (buffer + bufSize - (char*)signature >= sizeof(CEOCentrDirRecord))
                            CopyMemory(&EOCentrDir, signature, sizeof(CEOCentrDirRecord));
                        else
                        {
                            ZipFile->FilePointer = EOCentrDirOffs;
                            if (Read(ZipFile, &EOCentrDir, sizeof(CEOCentrDirRecord), NULL, NULL))
                            {
                                TRACE_E("Error reading end of central dir record");
                                error = IDS_NODISPLAY;
                                break;
                            }
                        }
                        if (EOCentrDir.CommentLen)
                        {
                            Comment = (char*)malloc(EOCentrDir.CommentLen + 1);
                            if (!Comment)
                            {
                                error = IDS_LOWMEM;
                                break;
                            }
                            ZipFile->FilePointer = EOCentrDirOffs + sizeof(CEOCentrDirRecord);
                            if (Read(ZipFile, Comment, EOCentrDir.CommentLen, NULL, NULL))
                            {
                                if (SalamanderGeneral->ShowMessageBox(
                                        LoadStr(IDS_ERRREADCOMMENT),
                                        LoadStr(IDS_PLUGINNAME),
                                        MSGBOX_EX_ERROR) != IDOK)
                                {
                                    error = IDS_NODISPLAY;
                                }
                                free(Comment);
                                Comment = NULL;
                            }
                            else
                                Comment[EOCentrDir.CommentLen] = 0;
                        }
                        found = TRUE;
                        break;
                    }
            }
            position -= bufSize - 3; //tree overlapping bytes
            i--;
        }
        if (!error && !found && position + 64 * 1024 + 22 >= ZipFile->Size)
        {
            size = unsigned((ZipFile->Size - 3) % (bufSize - 3));
            if (size)
            {
                size += 3;
                ZipFile->FilePointer = 0;
                if (Read(ZipFile, buffer, size, NULL, NULL))
                {
                    TRACE_E("Error reading to buffer");
                    error = IDS_NODISPLAY;
                }
                else
                {
                    for (signature = (__UINT32*)(buffer + size - 4);
                         signature >= (__UINT32*)buffer;
                         signature = (__UINT32*)((char*)signature - 1))
                        if (*signature == SIG_EOCENTRDIR &&
                            (unsigned int)((char*)signature - buffer) < ZipFile->Size - 21)
                        {
                            EOCentrDirOffs = ((char*)signature - buffer);

                            if (buffer + size - (char*)signature >= sizeof(CEOCentrDirRecord))
                                CopyMemory(&EOCentrDir, signature, sizeof(CEOCentrDirRecord));
                            else
                            {
                                ZipFile->FilePointer = EOCentrDirOffs;
                                if (Read(ZipFile, &EOCentrDir, sizeof(CEOCentrDirRecord), NULL, NULL))
                                {
                                    TRACE_E("Error reading end of central dir record");
                                    error = IDS_NODISPLAY;
                                    break;
                                }
                            }
                            if (EOCentrDir.CommentLen)
                            {
                                Comment = (char*)malloc(EOCentrDir.CommentLen + 1);
                                if (!Comment)
                                {
                                    error = IDS_LOWMEM;
                                    break;
                                }
                                ZipFile->FilePointer = EOCentrDirOffs + sizeof(CEOCentrDirRecord);
                                // skipAll=true prevents Read showing EOF error. We show a better
                                // error ourselves immediatelly
                                bool skipAll = true;
                                if (Read(ZipFile, Comment, EOCentrDir.CommentLen, NULL, &skipAll))
                                {
                                    if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERRREADCOMMENT),
                                                                          LoadStr(IDS_PLUGINNAME), MSGBOX_EX_ERROR) != IDOK)
                                    {
                                        error = IDS_NODISPLAY;
                                    }
                                    free(Comment);
                                    Comment = NULL;
                                }
                                else
                                    Comment[EOCentrDir.CommentLen] = 0;
                            }
                            found = TRUE;
                            break;
                        }
                }
            }
        }
        if (!found && !error)
        {
            if (Extract)
            {
                INT_PTR ret;
                DetectRemovable();
                if (!Removable)
                {
                    FindLastFile(lastFile);
                    if (*lastFile &&
                        CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                      lastFile, -1, ZipName, -1) != CSTR_EQUAL)
                    {
                        lstrcpy(ZipName, lastFile);
                    }
                    else
                        Config.AutoExpandMV = false;
                }
                if (Removable || !Config.AutoExpandMV)
                {
                    ret = ChangeDiskDialog2(SalamanderGeneral->GetMsgBoxParent(), ZipName);
                }
                else
                    ret = IDOK;
                if (ret == IDOK)
                {
                    bool useReadCache = ZipFile->InputBuffer != NULL;
                    bool bigFile = ZipFile->BigFile != 0;
                    CloseCFile(ZipFile);
                    ZipFile = NULL;
                    ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                                      bigFile, useReadCache);
                    if (ret)
                    {
                        if (ret == ERR_LOWMEM)
                            error = IDS_LOWMEM;
                        else
                            error = IDS_NODISPLAY;
                    }
                    else
                    {
                        retry = true;
                    }
                }
                else
                {
                    error = IDS_NODISPLAY;
                }
            }
            else
            {
                error = IDS_ERRMULTIVOL;
                Fatal = true;
            }
        }
    } while (retry);
    if (success)
        *success = found;
    if (!error)
    {
        CentrDirSize = EOCentrDir.CentrDirSize;
        CentrDirOffs = EOCentrDir.CentrDirOffs;
        if (EOCentrDir.DiskNum > 0 &&
            // this indicates zip64, the actual value of DiskNum is stored in zip64 record,
            // and it is handled later in CZipCommon::FindZip64EOCentrDirLocator
            EOCentrDir.DiskNum != 0xFFFF)
        {
            if (!Extract)
            {
                error = IDS_ERRMULTIVOL;
                Fatal = true;
            }
            else
            {
                MultiVol = true;
                DiskNum = EOCentrDir.DiskNum;
                CentrDirStartDisk = EOCentrDir.StartDisk;
                DetectRemovable();
                CHDiskFlags = CHD_FIRST | (Removable ? 0 : CHD_SEQNAMES);

                // mala heuristika, zda nahodou nejde o winzip style names
                LPCTSTR arcName = _tcsrchr(ZipFile->FileName, '\\');
                if (arcName != NULL)
                    arcName++;
                else
                    arcName = ZipFile->FileName;
                LPCTSTR ext = _tcsrchr(arcName, '.');
                if (ext != NULL && lstrcmpi(ext, ".zip") == 0) // ".cvspass" ve Windows je pripona
                {
                    while (ext - 1 > arcName && isdigit(ext[-1]))
                        ext--;
                    if (!isdigit(*ext) || atoi(ext) != DiskNum + 1)
                        CHDiskFlags |= CHD_WINZIP;
                }

                //if (*OriginalCurrentDir)  SetCurrentDirToZipPath();
            }
        }
        else
        {
            DiskNum = 0;
            CentrDirStartDisk = 0;
            MultiVol = false;
        }
    }
    return error;
}

int CZipCommon::FindZip64EOCentrDirLocator()
{
    CALL_STACK_MESSAGE1("CZipCommon::FindZip64EOCentrDirLocator()");
    CZip64EOCentrDirLocator zip64Locator;

    // vejde se potencionalni zip64 end of central directory locator pred
    // end of central directory record?
    if (EOCentrDirOffs < sizeof(zip64Locator))
        return 0;

    ZipFile->FilePointer = EOCentrDirOffs - sizeof(zip64Locator);
    if (Read(ZipFile, &zip64Locator, sizeof(zip64Locator), NULL, NULL))
        return IDS_NODISPLAY;

    // je na predpokladanem miste opravdu zip64?
    if (zip64Locator.Signature != SIG_ZIP64LOCATOR)
        return 0;

    // prepneme se na disk obsahujici zip64 end of central directory record
    if (MultiVol &&
        (EOCentrDir.DiskNum == 0xFFFF ||
         DiskNum != (int)zip64Locator.Zip64StartDisk))
    {
        DiskNum = zip64Locator.Zip64StartDisk;
        int err = ChangeDisk();
        if (err)
            return err;
    }

    CZip64EOCentrDirRecord zip64Record;

    ZipFile->FilePointer = Zip64EOCentrDirOffs = zip64Locator.Zip64Offset;
    if (Read(ZipFile, &zip64Record, sizeof(zip64Record), NULL, NULL))
        return IDS_NODISPLAY;

    // nahradime udaje opravnymi udaji
    Zip64 = true;
    if (zip64Record.DiskNum < 0xFFFF)
    {
        // we keep this value unfixed so we can later recognize a weird ZIP file
        // and refuse its modification, so far we have encountered a single file
        // like this and we had trouble with deleting files inside the archive,
        // which ended with the corrupted archive data)
        // TODO save manison.zip to some repository for the weird ZIPs and put a correct reference here
        //EOCentrDir.DiskNum = __UINT16(zip64Record.DiskNum);
    }
    if (EOCentrDir.StartDisk == 0xFFFF)
    {
        CentrDirStartDisk = zip64Record.StartDisk;
        if (CentrDirStartDisk < 0xFFFF)
            EOCentrDir.StartDisk = CentrDirStartDisk;
    }
    if (zip64Record.DiskTotalEntries < 0xFFFF)
        EOCentrDir.DiskTotalEntries = __UINT16(zip64Record.DiskTotalEntries);
    if (zip64Record.TotalEntries < 0xFFFF)
        EOCentrDir.TotalEntries = __UINT16(zip64Record.TotalEntries);
    if (EOCentrDir.CentrDirSize == 0xFFFFFFFF)
    {
        CentrDirSize = zip64Record.CentrDirSize;
        if (CentrDirSize < 0xFFFFFFFF)
            EOCentrDir.CentrDirSize = __UINT32(CentrDirSize);
    }
    if (EOCentrDir.CentrDirOffs == 0xFFFFFFFF)
    {
        CentrDirOffs = zip64Record.CentrDirOffs;
        if (CentrDirOffs < 0xFFFFFFFF)
            EOCentrDir.CentrDirOffs = __UINT32(CentrDirOffs);
    }

    // Refuse files with encrypted Central Directory
    if (zip64Record.Size >= sizeof(CZip64EOCentrDirRecord) + sizeof(CZip64EOCentrDirRecordVer2) - sizeof(zip64Record.Signature) - sizeof(zip64Record.Size))
    {
        CZip64EOCentrDirRecordVer2 zip64RecordVer2;

        if (Read(ZipFile, &zip64RecordVer2, sizeof(zip64Record), NULL, NULL))
            return IDS_NODISPLAY;
        if ((zip64RecordVer2.Method != CM_STORED) || (zip64RecordVer2.AlgID != 0))
        {
            return IDS_UNSUP_CD_ENCRYPTION;
        }
    }

    return 0;
}

int CZipCommon::CheckForExtraBytes()
{
    CALL_STACK_MESSAGE1("CZipCommon::CheckForExtraBytes()");

    ExtraBytes = 0;

    //nekontroluju extra bytes - moc prace/malo uzitku
    if (MultiVol)
        return 0;

    QWORD endOfCentrDir = CentrDirOffs + CentrDirSize;
    QWORD expectedEndOfCentrDir = Zip64 ? Zip64EOCentrDirOffs : EOCentrDirOffs;

    if (endOfCentrDir == expectedEndOfCentrDir)
        return 0; // vse v poradku

    if (expectedEndOfCentrDir > endOfCentrDir)
    {
        ExtraBytes = expectedEndOfCentrDir - endOfCentrDir;
        if (CentrDirOffs == 0 && CentrDirSize > 0)
        {
            TRACE_E("correcting CentrDirOffs - zip 1.5 bug");
            EOCentrDir.CentrDirOffs = __UINT32(CentrDirOffs = ExtraBytes);
            ExtraBytes = 0;
            return 0;
        }
        else
        {
            if (Extract) // || MenuSfx)
            {
                __UINT32 sig = 0;
                ZipFile->FilePointer = CentrDirOffs + ExtraBytes;
                if (ZipFile->FilePointer + 4 <= ZipFile->Size &&
                    Read(ZipFile, &sig, 4, NULL, NULL))
                    return IDS_NODISPLAY;
                if (sig == SIG_CENTRFH)
                {
                    TRACE_W("extra bytes " << LODWORD((DWORD)(ExtraBytes & 0x00000000FFFFFFFF)) << ":" << HIDWORD(ExtraBytes));
                    return 0;
                }
                ZipFile->FilePointer = CentrDirOffs;
                if (Read(ZipFile, &sig, 4, NULL, NULL))
                    return IDS_NODISPLAY;
                if (sig == SIG_CENTRFH)
                {
                    ExtraBytes = 0;
                    return 0;
                }
            }
        }
    }
    else
    {
        if (Extract && expectedEndOfCentrDir > CentrDirOffs)
        {
            TRACE_W("spatna velikost centralniho adresare, kompenzuji chybu");
            CentrDirSize = expectedEndOfCentrDir - CentrDirOffs;
            return 0;
        }
    }
    return IDS_ERRFORMAT;
}

int CZipCommon::ReadCentralHeader(CFileHeader* fileHeader, LPQWORD offset,
                                  unsigned int* size)
{
    CALL_STACK_MESSAGE_NONE // casove kriticka metoda
                            //  CALL_STACK_MESSAGE1("CZipCommon::ReadCentralHeader(, )");
        unsigned bytesRead;
    //reads file header and name from disc
    ZipFile->FilePointer = *offset;
    if (ZipFile->FilePointer + sizeof(CFileHeader) > ZipFile->Size)
    {
        if (Extract && MultiVol)
        {
            DiskNum++;
            int err = ChangeDisk();
            if (err)
                return err;
        }
        else
        {
            Fatal = true;
            return IDS_EOF;
        }
    }
    QWORD s = ZipFile->FilePointer;
    if (Read(ZipFile, fileHeader, sizeof(CFileHeader), &bytesRead, NULL))
        return IDS_NODISPLAY;
    if (bytesRead != sizeof(CFileHeader))
    {
        Fatal = true;
        return IDS_EOF;
    }
    if (fileHeader->Signature != SIG_CENTRFH)
    {
        Fatal = true;
        return IDS_MISSCHSIG;
    }
    if (fileHeader->NameLen + fileHeader->ExtraLen +
            fileHeader->CommentLen + sizeof(CFileHeader) >
        MAX_HEADER_SIZE)
    {
        Fatal = true;
        return IDS_ERRFORMAT;
    }
    if (Read(ZipFile,
             (char*)fileHeader + sizeof(CFileHeader),
             fileHeader->NameLen +
                 fileHeader->ExtraLen +
                 fileHeader->CommentLen,
             &bytesRead,
             NULL))
        return IDS_NODISPLAY;
    if (bytesRead != (unsigned)fileHeader->NameLen +
                         fileHeader->ExtraLen +
                         fileHeader->CommentLen)
    {
        Fatal = true;
        return IDS_EOF;
    }
    *offset = ZipFile->FilePointer;
    if (size)
        *size = unsigned(ZipFile->FilePointer - s);
    return 0;
}

// makro umoznujici prehlednejsi nacitani extra headeru
#define NEXT(t) *(t*)((char*)fileHeader + (iterator += sizeof(t), iterator - sizeof(t)))

void CZipCommon::ProcessHeader(CFileHeader* fileHeader, CFileInfo* fileInfo)
{
    CALL_STACK_MESSAGE_NONE // casove kriticka metoda
                            //  CALL_STACK_MESSAGE1("CZipCommon::ProcessHeader(, )");
        FILETIME ft;

    /*
  TRACE_I("ProcessHeader()" <<
          " Version " << fileHeader->Version <<
          " VersionExtr " << fileHeader->VersionExtr <<
          " Flag " << fileHeader->Flag <<
          " Method " << fileHeader->Method <<
          " Time " << fileHeader->Time <<
          " Date " << fileHeader->Date <<
          " Crc " << fileHeader->Crc <<
          " CompSize " << fileHeader->CompSize <<
          " Size " << fileHeader->Size <<
          " StartDisk " << fileHeader->StartDisk <<
          " InterAttr " << fileHeader->InterAttr <<
          " ExternAttr " << fileHeader->ExternAttr <<
          " LocHeaderOffs " << fileHeader->LocHeaderOffs); 
          */
    if (!DosDateTimeToFileTime(fileHeader->Date, fileHeader->Time, &ft))
    {
        SystemTimeToFileTime(&MinZipTime, &ft);
    }
    LocalFileTimeToFileTime(&ft, &fileInfo->LastWrite);
    fileInfo->Flag = fileHeader->Flag;         //general purpose bit flag
    fileInfo->Method = fileHeader->Method;     //compression method
    fileInfo->Crc = fileHeader->Crc;           //crc-32
    fileInfo->CompSize = fileHeader->CompSize; //compressed size
    fileInfo->Size = fileHeader->Size;         //uncompressed size
    //fileInfo->NameLen = lstrlen(outFileHeader->Name);       //filename length
    //outFileHeader->ExtraLen = fileHeader.ExtraLen;     //extra field length
    //fileInfo->CommentLen = fileHeader.CommentLen; //file comment length
    fileInfo->StartDisk = fileHeader->StartDisk; //disk number start
    fileInfo->InterAttr = fileHeader->InterAttr; //internal file attributes
    switch (fileHeader->Version >> 8)
    {
    case HS_FAT:
    case HS_HPFS:
    case HS_VFAT:
    case HS_NTFS:
    case HS_ACORN:
    case HS_VM_CMS:
    case HS_MVS:
        fileInfo->FileAttr = fileHeader->ExternAttr & 0x0000FFFF;
        break;
    default:
        if (fileHeader->ExternAttr & FILE_ATTRIBUTE_DIRECTORY && fileHeader->Size && fileHeader->Size != 0xFFFFFFFF)
        {
            TRACE_E("directory has non zero size");
            fileHeader->ExternAttr &= ~FILE_ATTRIBUTE_DIRECTORY;
        }
        fileInfo->FileAttr = fileHeader->ExternAttr & 0x0000FFFF;
    }
    fileInfo->LocHeaderOffs = fileHeader->LocHeaderOffs + ExtraBytes;
    fileInfo->IsDir = fileHeader->ExternAttr & FILE_ATTRIBUTE_DIRECTORY;

    // pro pripad, ze se nektera z hodnot nevesla, zkusime najit zip64 extra field
    int expectedZip64Size = 0;
    if (fileHeader->Size == 0xFFFFFFFF)
        expectedZip64Size += 8;
    if (fileHeader->CompSize == 0xFFFFFFFF)
        expectedZip64Size += 8;
    if (fileHeader->LocHeaderOffs == 0xFFFFFFFF)
        expectedZip64Size += 8;
    if (fileHeader->StartDisk == 0xFFFF)
        expectedZip64Size += 4;
    if (expectedZip64Size > 0)
    {
        unsigned iterator = sizeof(CFileHeader) + fileHeader->NameLen;

        // vyhledame zip64 extra header;
        BOOL found = FALSE;
        while (iterator + 4 < sizeof(CFileHeader) + fileHeader->NameLen + fileHeader->ExtraLen)
        {
            if (NEXT(WORD) == ZIP64_HEADER_ID)
            {
                found = TRUE;
                break;
            }
            iterator += NEXT(WORD);
        }

        // nacteme hodnoty z headeru
        if (found)
        {
            if (NEXT(WORD) >= expectedZip64Size)
            {
                if (fileInfo->Size == 0xFFFFFFFF)
                    fileInfo->Size = NEXT(QWORD);
                if (fileInfo->CompSize == 0xFFFFFFFF)
                    fileInfo->CompSize = NEXT(QWORD);
                if (fileInfo->LocHeaderOffs == 0xFFFFFFFF)
                    fileInfo->LocHeaderOffs = NEXT(QWORD);
                if (fileInfo->StartDisk == 0xFFFF)
                    fileInfo->StartDisk = NEXT(DWORD);
            }
            else
                TRACE_E("zip64 extra-field je prilis maly, poskozeny zip?");
        }
        else
            TRACE_E("nenasli jsme zip64 extra-field, poskozeny zip?");
    }
}

bool CZipCommon::IsDirByHeader(CFileHeader* fileHeader)
{
    CALL_STACK_MESSAGE_NONE
    unsigned attr = fileHeader->ExternAttr;
    switch (fileHeader->Version >> 8)
    {
    case HS_FAT:
    case HS_HPFS:
    case HS_VFAT:
    case HS_NTFS:
    case HS_ACORN:
    case HS_VM_CMS:
    case HS_MVS:
        break;
    default:
        if (attr & FILE_ATTRIBUTE_DIRECTORY && fileHeader->Size)
        {
            TRACE_E("directory has non zero size");
            attr &= ~FILE_ATTRIBUTE_DIRECTORY;
        }
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int CZipCommon::ProcessName(CFileHeader* fileHeader, char* outputName)
{
    CALL_STACK_MESSAGE_NONE // casove kriticka metoda
                            //  CALL_STACK_MESSAGE1("CZipCommon::ProcessName");
        char* sour;
    char* dest;
    sour = (char*)fileHeader + sizeof(CFileHeader);
    size_t len = fileHeader->NameLen;

    char* sourLocEnc = NULL;
    // If the file is originating on Unix (or Mac), we check if it is a true UTF8 string
    // if yes, then we convert it to local encoding
    if (((fileHeader->Version >> 8 == HS_UNIX) || (fileHeader->Flag & GPF_UTF8)))
    {
        void* zero = memchr(sour, 0, len);
        if (zero)
        {
            len = (char*)zero - sour + 1;
        }
        if (IsUTF8Encoded(sour, (int)len))
        {
            LPWSTR wsour = (LPWSTR)malloc(len * sizeof(WCHAR));
            if (wsour)
            {
                // CodePage 65001 is UTF8 and is supported since W2K (or WNT4?)
                int wlen = (int)MultiByteToWideChar(CP_UTF8, 0, sour, (int)len, wsour, (int)len);

                if (wlen > 0)
                {
                    // Convert back to local encoding, convert composite chars to precomposed (e.g. accents from Mac)
                    int lenLocEnc = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wsour, wlen, NULL, 0, NULL, NULL);
                    if (lenLocEnc > 0)
                    {
                        sourLocEnc = (char*)malloc(lenLocEnc);
                        if (sourLocEnc)
                        {
                            len = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wsour, wlen, sourLocEnc, lenLocEnc, NULL, NULL);
                            sour = sourLocEnc;
                        }
                    }
                }
                free(wsour);
            }
        }
    }

    char* end = sour + len;
    dest = outputName;

    // ostranime uvodni lomitka
    while ((sour < end) && (*sour == '/' || *sour == '\\'))
        sour++;
    // replace leading spaces with underscores
    while ((sour < end) && *sour == ' ')
    {
        *dest++ = '_';
        sour++;
    }
    while ((sour < end) && *sour != 0)
    {
        if (*sour == '/' || *sour == '\\')
        {
            sour++;
            // odstranime vicenasobna lomitka
            while ((sour < end) && (*sour == '/' || *sour == '\\'))
                sour++;
            // replace trailing spaces in the last filename component with underscores
            char* iter = dest - 1;
            while ((iter >= outputName) && *iter == ' ')
                *iter-- = '_';
            *dest++ = '\\';
            // replace leading spaces in the current filename component with underscores
            while ((sour < end) && *sour == ' ')
            {
                *dest++ = '_';
                sour++;
            }
        }
        else if (((unsigned char)*sour < 32) || strchr("*?<>|\":", *sour))
        { // Replace illegal char with underscore
            *dest++ = '_';
            sour++;
        }
        else
        {
            *dest++ = *sour++;
        }
    }
    if (dest > outputName && *(dest - 1) == '\\') //skip last slash if name specifies a directory
    {
        dest--;
        //nastavime atribut adresare, nektere archivery nenastavuji atributy
        //tak aby se nam nezobrazil jako soubor
        fileHeader->ExternAttr |= FILE_ATTRIBUTE_DIRECTORY;
    }
    // replace trailing spaces with underscores
    char* iter = dest - 1;
    while ((iter >= outputName) && *iter == ' ')
        *iter-- = '_';

    if (sourLocEnc)
        free(sourLocEnc);

    *dest = 0;
    if (!(fileHeader->Flag & GPF_UTF8) && ((fileHeader->Version >> 8 == HS_FAT /*0*/) ||
                                           (fileHeader->Version >> 8 == HS_HPFS /*6*/) ||
                                           //       ((fileHeader->Version >> 8 == HS_NTFS/*11*/) && ((fileHeader->Version & 0x0F) == 0x50)) // Patera 2010.03.30: This doesn't make sense -> disabled
                                           // The following line got inspiration in MultiArc plugin of FAR
                                           ((fileHeader->Version >> 8 == HS_NTFS /*11*/) && (((fileHeader->Version & 0xFF) <= 20) || ((fileHeader->Version & 0xFF) >= 25)))))
        // ZIP built-in to WinXP writes Version 0x0b14 and uses OEM
        // AS writes version 0x0016 and uses OEM
        OemToChar(outputName, outputName);
    //  TRACE_I("Processed name " << outputName);
    return (int)(dest - outputName);
}

int CZipCommon::ReadLocalHeader(CLocalFileHeader* fileHeader, QWORD offset)
{
    CALL_STACK_MESSAGE2("CZipCommon::ReadLocalHeader(, 0x%I64X)", offset);
    unsigned bytesRead;

    //reads file header and name from disc
    ZipFile->FilePointer = offset;
    if (Read(ZipFile, fileHeader, sizeof(CLocalFileHeader), &bytesRead, NULL))
        return IDS_NODISPLAY;
    if (bytesRead != sizeof(CLocalFileHeader))
    {
        Fatal = true;
        return IDS_EOF;
    }
    if (fileHeader->Signature != SIG_LOCALFH)
    {
        Fatal = true;
        return IDS_MISSLHSIG;
    }
    if (fileHeader->NameLen + fileHeader->ExtraLen + sizeof(CFileHeader) > MAX_HEADER_SIZE)
    {
        Fatal = true;
        return IDS_ERRFORMAT;
    }
    if (Read(ZipFile,
             (char*)fileHeader + sizeof(CLocalFileHeader),
             fileHeader->NameLen +
                 fileHeader->ExtraLen,
             &bytesRead,
             NULL))
        return IDS_NODISPLAY;
    if (bytesRead != (unsigned)fileHeader->NameLen +
                         fileHeader->ExtraLen)
    {
        Fatal = true;
        return IDS_EOF;
    }
    return 0;
}

void CZipCommon::ProcessLocalHeader(CLocalFileHeader* fileHeader,
                                    CFileInfo* fileInfo, CAESExtraField* aesExtraField)
{
    CALL_STACK_MESSAGE1("CZipCommon::ProcessLocalHeader(, )");
    fileInfo->DataOffset = fileInfo->LocHeaderOffs +
                           sizeof(CLocalFileHeader) +
                           fileHeader->NameLen +
                           fileHeader->ExtraLen;

    if (fileHeader->Method == CM_AES)
    {
        // najdeme AES extra-field

        // pro pripad, ze bychom ho nenasli
        aesExtraField->HeaderID = -1;

        DWORD offset = sizeof(CLocalFileHeader) + fileHeader->NameLen;
        while (offset + sizeof(CAESExtraField) <=
               sizeof(CLocalFileHeader) + fileHeader->NameLen + fileHeader->ExtraLen)
        {
            if (*LPWORD(LPBYTE(fileHeader) + offset) == AES_HEADER_ID)
            {
                memcpy(aesExtraField, LPBYTE(fileHeader) + offset, sizeof(CAESExtraField));
                break;
            }

            offset += *LPWORD(LPBYTE(fileHeader) + offset + 2) + 4;
        }
    }
}

void CZipCommon::SplitPath(char** path, char** name, const char* pathToSplit)
{
    CALL_STACK_MESSAGE2("CZipCommon::SplitPath(, , %s)", pathToSplit);
    char* lastSlash = NULL;
    char* dest = *path;

    strcpy(dest, pathToSplit);
    // _tcsrchr (or _mbcsrchr) correctly handles MBCS
    lastSlash = _tcsrchr(dest, '\\');
    /*  while (*pathToSplit)
  {
   if (*pathToSplit == '\\')
     lastSlash = dest;
   *dest++ = *pathToSplit++;
  }
  *dest = 0;*/
    if (lastSlash)
    {
        *lastSlash++ = 0;
        *name = lastSlash;
    }
    else
    {
        *name = *path;
        //    *path = dest; // empty string
        *path = dest + strlen(dest); // empty string
    }
}

#define ITEM_LH_OFFSET(i) (((CFileInfo*)(*headersArray)[i])->LocHeaderOffs)

void CZipCommon::QuickSortHeaders(int left, int right, TIndirectArray2<CFileInfo>& headers)
{
    CALL_STACK_MESSAGE_NONE

LABEL_QuickSortHeaders:

    int i = left, j = right;
    QWORD pivotOffset = headers[(i + j) / 2]->LocHeaderOffs;
    do
    {
        while (headers[i]->LocHeaderOffs < pivotOffset && i < right)
            i++;
        while (pivotOffset < headers[j]->LocHeaderOffs && j > left)
            j--;
        if (i <= j)
        {
            CFileInfo* tmp = headers[i];
            headers[i] = headers[j];
            headers[j] = tmp;
            i++;
            j--;
        }
    } while (i <= j); //musej bejt shodny?

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) QuickSortHeaders(left, j, headers);
    //  if (i < right) QuickSortHeaders(i, right, headers);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                QuickSortHeaders(left, j, headers);
                left = i;
                goto LABEL_QuickSortHeaders;
            }
            else
            {
                QuickSortHeaders(i, right, headers);
                right = j;
                goto LABEL_QuickSortHeaders;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSortHeaders;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSortHeaders;
        }
    }
}

int CZipCommon::EnumFiles(TIndirectArray2<CExtInfo>& namesArray, int& dirs, SalEnumSelection next, void* param)
{
    CALL_STACK_MESSAGE1("CZipCommon::EnumFiles(, , )");
    const char* nextName;
    CExtInfo* info;
    BOOL isDir;
    int errorID = 0;
    const CFileData* fileData;

    dirs = 0;
    while ((nextName = next(NULL, 0, &isDir, NULL, &fileData, param, NULL)) != NULL)
    {
        CZIPFileData* zipFileData = (CZIPFileData*)fileData->PluginData;

        // NOTE: zipFileData is NULL for dirs that are not present in the ZIP as such but only with subitems
        // Then Salamander creates CFileData for us
        Unix = Unix || (zipFileData ? zipFileData->Unix : 0);

        info = new CExtInfo(nextName, isDir ? true : false, zipFileData ? zipFileData->ItemNumber : -1);
        if (!info || !info->Name)
        {
            if (info)
                delete info;
            errorID = IDS_LOWMEM;
            break;
        }
        if (!namesArray.Add(info))
        {
            delete info;
            errorID = IDS_LOWMEM;
            break;
        }
        if (isDir)
            dirs++;
    }
    return errorID;
}

int CompareExtInfos(const CExtInfo* left, const CExtInfo* right, bool unix)
{
    CALL_STACK_MESSAGE_NONE
    if (left->IsDir)
    {
        if (right->IsDir)
        {
            int ret = unix ? _tcscmp(left->Name, right->Name) : SalamanderGeneral->StrICmp(left->Name, right->Name);
            return ret ? ret : (left->ItemNumber < right->ItemNumber ? -1 : (left->ItemNumber == right->ItemNumber ? 0 : 1));
        }
        return -1;
    }

    if (right->IsDir)
        return 1;

    int ret = _tcscmp(left->Name, right->Name);
    return ret ? ret : (left->ItemNumber < right->ItemNumber ? -1 : (left->ItemNumber == right->ItemNumber ? 0 : 1));
}

void QuickSortNames(int left, int right, TIndirectArray2<CExtInfo>& names, bool unix)
{
    CALL_STACK_MESSAGE_NONE

LABEL_QuickSortNames:

    int i = left, j = right;
    CExtInfo* pivot = names[(i + j) / 2];
    do
    {
        while (CompareExtInfos(names[i], pivot, unix) < 0 && i < right)
            i++;
        while (CompareExtInfos(pivot, names[j], unix) < 0 && j > left)
            j--;
        if (i <= j)
        {
            CExtInfo* tmp = names[i];
            names[i] = names[j];
            names[j] = tmp;
            i++;
            j--;
        }
    } while (i <= j); //musej bejt shodny?

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) QuickSortNames(left, j, names, unix);
    //  if (i < right) QuickSortNames(i, right, names, unix);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                QuickSortNames(left, j, names, unix);
                left = i;
                goto LABEL_QuickSortNames;
            }
            else
            {
                QuickSortNames(i, right, names, unix);
                right = j;
                goto LABEL_QuickSortNames;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSortNames;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSortNames;
        }
    }
}

bool BSearchName(LPCTSTR name, int ItemNumber, int left, int right, TIndirectArray2<CExtInfo>& names, bool respectCase)
{
    CALL_STACK_MESSAGE5("BSearchName(%s, %d, %d, , %d)", name, left, right,
                        respectCase);
    int c, mid;
    while (left < right)
    {
        mid = (left + right) >> 1;
        c = respectCase ? _tcscmp(name, names[mid]->Name) : SalamanderGeneral->StrICmp(name, names[mid]->Name);
        // Files are matched only when the ItemNumber also matches
        if (c == 0)
        {
            if ((ItemNumber == names[mid]->ItemNumber) || (-1 == names[mid]->ItemNumber) || (-1 == ItemNumber))
                return true;
            if (ItemNumber < names[mid]->ItemNumber)
                right = mid;
            else
                left = mid + 1;
            continue;
        }
        if (c < 0)
            right = mid;
        else
            left = mid + 1;
    }
    return false;
}

int CZipCommon::MatchFiles(TIndirectArray2<CFileInfo>& files, TIndirectArray2<CExtInfo>& namesArray, int dirs, char* centrDir)
{
    CALL_STACK_MESSAGE1("CZipCommon::MatchFiles(, , )");
    CFileHeader* centralHeader = NULL;
    char* tempName = NULL;
    int tempNameLen;
    CFileInfo* fileInfo = NULL;
    int errorID = 0;
    QWORD readOffset;
    QWORD readSize;
    char* name = NULL;
    DWORD pathFlag = Unix ? 0 : NORM_IGNORECASE;

    if (!centrDir)
        centralHeader = (CFileHeader*)malloc(MAX_HEADER_SIZE);
    tempName = (char*)malloc(MAX_HEADER_SIZE);
    if ((!centralHeader && !centrDir) || !tempName)
    {
        if (centralHeader && !centrDir)
            free(centralHeader);
        if (tempName)
            free(tempName);
        return IDS_LOWMEM;
    }
    //i = EOCentrDir.TotalEntries;//directory entries counter
    readOffset = CentrDirOffs + ExtraBytes;
    readSize = 0;
    if (DiskNum != CentrDirStartDisk && MultiVol)
    {
        DiskNum = CentrDirStartDisk;
        errorID = ChangeDisk();
    }
    QuickSortNames(0, namesArray.Count - 1, namesArray, Unix != 0);

    int cnt;
    for (cnt = 0; readSize < CentrDirSize && !errorID; cnt++)
    {
        unsigned int s;
        if (centrDir) //for files deletion
        {
            centralHeader = (CFileHeader*)(centrDir + readSize);
            if (readSize + sizeof(CFileHeader) > CentrDirSize ||
                readSize + sizeof(CFileHeader) + centralHeader->NameLen +
                        centralHeader->ExtraLen + centralHeader->CommentLen >
                    CentrDirSize)
            {
                errorID = IDS_ERRFORMAT;
                Fatal = true;
            }
            else
                s = sizeof(CFileHeader) +
                    centralHeader->NameLen +
                    centralHeader->ExtraLen +
                    centralHeader->CommentLen;
        }
        else
            errorID = ReadCentralHeader(centralHeader, &readOffset, &s);
        if (errorID)
            break;
        readSize += s;
        tempNameLen = ProcessName(centralHeader, tempName);

        // otestujeme prefix jmena
        if (tempNameLen < RootLen || RootLen && tempName[RootLen] != '\\' ||
            (Unix ? memcmp(tempName, ZipRoot, RootLen) : SalamanderGeneral->MemICmp(tempName, ZipRoot, RootLen)))
            continue;

        name = tempName + (RootLen ? RootLen + 1 : 0);
        LPTSTR slash = _tcschr(name, '\\');
        if (slash) // tempName + RootLen + 1, obsahuje lomitko
        {
            // hledame komponentu cesty v seznamu adresaru, case-sensitivity zavisi
            // na flagu Unix
            *slash = 0;
            bool r = BSearchName(name, -1 /*any file inside this folder*/, 0, dirs, namesArray, Unix != 0);
            *slash = '\\';
            if (!r)
                continue;
        }
        else
        {
            if (IsDirByHeader(centralHeader)) // je to adresar
            {
                // hledame v seznamu adresaru, case-sensitivity zavisi na flagu Unix
                if (!BSearchName(name, cnt, 0, dirs, namesArray, Unix != 0))
                    continue;
            }
            else
            {
                // hledame v seznamu souboru, na case zalezi
                if (!BSearchName(name, cnt, dirs, namesArray.Count, namesArray, true))
                    continue;
            }
        }

        // nasli jsme ho binarnim searchem v seznamu vybranych souboru, pridame ho
        // tedy do seznamu pro vybaleni
        fileInfo = new CFileInfo;
        if (!fileInfo)
        {
            errorID = IDS_LOWMEM;
            break;
        }
        ProcessHeader(centralHeader, fileInfo);
        fileInfo->NameLen = tempNameLen;
        fileInfo->Name = _tcsdup(tempName);
        if (!fileInfo->Name)
        {
            delete fileInfo;
            errorID = IDS_LOWMEM;
            break;
        }
        if (!files.Add(fileInfo))
        {
            delete fileInfo;
            errorID = IDS_LOWMEM;
        }
        if (!errorID)
        {
            if (centrDir) //for files deletion
                MatchedTotalSize += CQuadWord().SetUI64(fileInfo->CompSize);
            else
                MatchedTotalSize += CQuadWord().SetUI64(fileInfo->Size);
        }
    }

    if (!centrDir)
        free(centralHeader);
    free(tempName);
    return errorID;
}

void CZipCommon::DetectRemovable()
{
    CALL_STACK_MESSAGE1("CZipCommon::DetectRemovable()");
    char pathRoot[MAX_PATH];
    const char* sour = ZipName;
    char* dest = pathRoot;

    while (*sour && *sour != '\\')
        *dest++ = *sour++;
    *dest = 0;

    if (GetDriveType(pathRoot) == DRIVE_REMOVABLE)
        Removable = true;
    else
        Removable = false;
}

/*
void CZipCommon::SetCurrentDirToZipPath()
{
  char   buf[MAX_PATH + 1];
  char * path;
  char * dummy;

  path = buf;
  SplitPath(&path, &dummy, ZipName);
  if (*path) SetCurrentDirectory(path);
}
*/

int CZipCommon::TestIfExist(const char* name)
{
    CALL_STACK_MESSAGE2("CZipCommon::TestIfExist(%s)", name);
    DWORD fattr = SalamanderGeneral->SalGetFileAttributes(name);
    if (fattr == 0xFFFFFFFF)
    {
        int err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND)
        {
            ProcessError(IDS_ERRACCESS, err, name, PE_NORETRY | PE_NOSKIP, NULL);
            return ErrorID = IDS_NODISPLAY;
        }
    }
    else
    {
        char attr[101];
        FILETIME ft;
        CFile* file;

        int ret = CreateCFile(&file, name, GENERIC_READ, FILE_SHARE_READ,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                              true, false);
        if (ret)
        {
            if (ret == ERR_LOWMEM)
                return ErrorID = IDS_LOWMEM;
            else
                return ErrorID = IDS_NODISPLAY;
        }
        GetFileTime(file->File, NULL, NULL, &ft);
        GetInfo(attr, &ft, file->Size);
        CloseCFile(file);
        if (OverwriteDialog2(SalamanderGeneral->GetMsgBoxParent(), name, attr) != IDC_YES)
            return ErrorID = IDS_NODISPLAY;
        SetFileAttributes(name, FILE_ATTRIBUTE_NORMAL);
    }
    return 0;
}

BOOL Read(HANDLE file, void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("Read(, , 0x%X)", size);
    DWORD read;
    if (!ReadFile(file, buffer, size, &read, NULL) || read != size)
        return FALSE;
    return TRUE;
}

struct CInflateBufferData
{
    char* Output;
    char* OutputEnd;
};

void InflateBufferRefill(CDecompressionObject* decompress)
{
    CALL_STACK_MESSAGE1("InflateBufferRefill()");

    decompress->Input->Error = 1;
}

int InflateBufferFlush(unsigned bytes, CDecompressionObject* decompress)
{
    CALL_STACK_MESSAGE2("InflateBufferFlush(0x%X, )", bytes);
    CInflateBufferData* data = (CInflateBufferData*)decompress->UserData;

    if (data->Output + bytes > data->OutputEnd)
        return 1;
    memcpy(data->Output, decompress->Output->SlideWin, bytes);
    data->Output += bytes;
    return 0;
}

int InflateBuffer(char* sour, int sourSize, char* dest, int* destSize)
{
    CALL_STACK_MESSAGE1("CZipUnpack::InflateBuffer(, )");
    int ret = 0;
    CDecompressionObject decompress;
    COutputManager output;
    CInputManager input;
    CInflateBufferData data;
    input.NextByte = (__UINT8*)sour;
    input.BytesLeft = sourSize;
    input.Error = 0;
    input.Refill = InflateBufferRefill;
    output.SlideWin = (__UINT8*)malloc(SLIDE_WINDOW_SIZE);
    if (!output.SlideWin)
        return IDS_LOWMEM;
    output.WinSize = SLIDE_WINDOW_SIZE;
    output.Flush = InflateBufferFlush;
    data.Output = dest;
    data.OutputEnd = dest + *destSize;
    decompress.Input = &input;
    decompress.Output = &output;
    decompress.UserData = &data;
    decompress.fixed_tl64 = (huft*)NULL;
    decompress.fixed_td64 = (huft*)NULL;
    decompress.fixed_bl64 = 0;
    decompress.fixed_bd64 = 0;
    decompress.fixed_tl32 = (huft*)NULL;
    decompress.fixed_td32 = (huft*)NULL;
    decompress.fixed_bl32 = 0;
    decompress.fixed_bd32 = 0;
    decompress.HeapInfo = (void*)HeapCreate(HEAP_NO_SERIALIZE, INITIAL_HEAP_SIZE, MAXIMUM_HEAP_SIZE);
    if (!decompress.HeapInfo)
    {
        ret = IDS_LOWMEM;
        goto FAIL;
    }
    switch (Inflate(&decompress, 0))
    {
    case 1:;
    case 2:;
    case 4:;
    case 5:
    {
        ret = IDS_CORRUPTSFX2;
        break;
    }
    case 3:
        ret = IDS_LOWMEM;
        break;
    }
    *destSize = (int)(data.Output - dest);
FAIL:
    free(output.SlideWin);
    FreeFixedHufman(&decompress);
    if (decompress.HeapInfo)
        HeapDestroy(decompress.HeapInfo);
    return ret;
}

int LoadSfxFileData(char* fileName, CSfxLang** lang)
{
    CALL_STACK_MESSAGE1("LoadSfxFileData(, )");
    HANDLE file = CreateFile(fileName, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return IDS_UNABLEREADSFX;

    *lang = new CSfxLang;
    if (!*lang)
    {
        CloseHandle(file);
        return IDS_LOWMEM;
    }

    int ret = IDS_UNABLEREADSFX;
    char* buf1 = NULL;
    char* buf2 = NULL;

    CSfxFileHeader header;
    if (!Read(file, &header, sizeof(CSfxFileHeader)))
        goto FAIL;

    if (header.Signature != SFX_SIGNATURE)
    {
        ret = IDS_CORRUPTSFX2;
        goto FAIL;
    }
    if (header.CompatibleVersion != SFX_SUPPORTEDVERSION)
    {
        ret = IDS_BADSFXVER;
        goto FAIL;
    }
    if (header.HeaderCRC != SalamanderGeneral->UpdateCrc32(&header, sizeof(CSfxFileHeader) - sizeof(DWORD), INIT_CRC))
    {
        ret = IDS_CORRUPTSFX2;
        goto FAIL;
    }

    int s;
    s = 0;
    for (int i = 0; i < LENARRAYSIZE; s += header.TextLen[i++])
        ;
    (*lang)->DlgTitle = (char*)malloc(s + LENARRAYSIZE);
    buf1 = (char*)malloc(s);
    buf2 = (char*)malloc(header.TotalTextSize);
    if (!buf1 || !buf2 || !(*lang)->DlgTitle)
    {
        if (buf1)
            free(buf1);
        if (buf2)
            free(buf2);
        ret = IDS_LOWMEM;
        goto FAIL;
    }

    if (!Read(file, buf2, header.TotalTextSize))
        goto FAIL;
    ret = InflateBuffer(buf2, header.TotalTextSize, buf1, &s);
    if (ret)
        goto FAIL;
    if (header.TextsCRC != SalamanderGeneral->UpdateCrc32(buf1, s, INIT_CRC))
    {
        ret = IDS_CORRUPTSFX2;
        goto FAIL;
    }

    char* ptr;
    ptr = buf1;
    memcpy((*lang)->DlgTitle, ptr, header.TextLen[TITLELEN]);
    (*lang)->DlgTitle[header.TextLen[TITLELEN]] = 0;
    ptr += header.TextLen[TITLELEN];

    (*lang)->DlgText = (*lang)->DlgTitle + header.TextLen[TITLELEN] + 1;
    memcpy((*lang)->DlgText, ptr, header.TextLen[TEXTLEN]);
    (*lang)->DlgText[header.TextLen[TEXTLEN]] = 0;
    ptr += header.TextLen[TEXTLEN];

    (*lang)->AboutLicenced = (*lang)->DlgText + header.TextLen[TEXTLEN] + 1;
    memcpy((*lang)->AboutLicenced, ptr, header.TextLen[ABOUTLICENCEDLEN]);
    (*lang)->AboutLicenced[header.TextLen[ABOUTLICENCEDLEN]] = 0;
    ptr += header.TextLen[ABOUTLICENCEDLEN];

    (*lang)->ButtonText = (*lang)->AboutLicenced + header.TextLen[ABOUTLICENCEDLEN] + 1;
    memcpy((*lang)->ButtonText, ptr, header.TextLen[BUTTONTEXTLEN]);
    (*lang)->ButtonText[header.TextLen[BUTTONTEXTLEN]] = 0;
    ptr += header.TextLen[BUTTONTEXTLEN];

    (*lang)->Vendor = (*lang)->ButtonText + header.TextLen[BUTTONTEXTLEN] + 1;
    memcpy((*lang)->Vendor, ptr, header.TextLen[VENDORLEN]);
    (*lang)->Vendor[header.TextLen[VENDORLEN]] = 0;
    ptr += header.TextLen[VENDORLEN];

    (*lang)->WWW = (*lang)->Vendor + header.TextLen[VENDORLEN] + 1;
    memcpy((*lang)->WWW, ptr, header.TextLen[WWWLEN]);
    (*lang)->WWW[header.TextLen[WWWLEN]] = 0;
    ptr += header.TextLen[WWWLEN];

    (*lang)->LangID = header.LangID;
    char* c;
    c = strrchr(fileName, '\\');
    if (c)
        c++;
    else
        c = fileName;
    lstrcpy((*lang)->FileName, c);
    ret = 0;

FAIL:

    CloseHandle(file);
    if (buf1)
        free(buf1);
    if (buf2)
        free(buf2);
    if (ret)
    {
        delete *lang;
        *lang = NULL;
    }

    return ret;
}

void GetInfo(char* buffer, FILETIME* lastWrite, QWORD size)
{
    CALL_STACK_MESSAGE1("GetInfo(, , )");
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, CQuadWord().SetUI64(size)), date, time);
}

int MakeFileName(int number, bool seqNames, const char* archive,
                 char* name, BOOL winZipNames)
{
    CALL_STACK_MESSAGE5("MakeFileName(%d, %d, %s, , %d)", number, seqNames, archive, winZipNames);

    if (!seqNames)
        return (int)strlen(strcpy(name, archive));

    const char* arcName = strrchr(archive, '\\');
    if (arcName != NULL)
        arcName++;
    else
        arcName = archive;
    const char* ext = strrchr(arcName, '.');
    if (ext == NULL)
        ext = archive + strlen(archive); // ".cvspass" is extension in Windows
    int namelen = (int)(ext - archive);

    char buf[MAX_PATH + 12];
    memcpy(buf, archive, namelen);

    if (winZipNames)
        sprintf(buf + namelen, ".z%02d", number);
    else
        sprintf(buf + namelen, ext > archive && isdigit(ext[-1]) ? "_%02d%s" : "%02d%s", number, ext);

    int ret = (int)strlen(buf);
    if (ret > MAX_PATH)
    {
        TRACE_I("archive name is too long to add file numbers:" << buf);
        return (int)strlen(strcpy(name, archive));
    }

    strcpy(name, buf);
    return ret;
}

bool Atod(const char* string, char* decSep, double* val)
{
    CALL_STACK_MESSAGE3("Atod(%s, %s, )", string, decSep);
    int c;           /* current char */
    double total, d; /* current total */
    int sign;        /* if '-', then negative, otherwise positive */

    /* skip whitespace */
    while (*string && *string == ' ')
        string++;

    c = sign = (int)(unsigned char)*string; /* save sign indication */
    if (c == '-' || c == '+')
        c = (int)(unsigned char)*++string; /* skip sign */

    total = 0;

    while (isdigit(c))
    {
        total = 10 * total + (c - '0');    /* accumulate digit */
        c = (int)(unsigned char)*++string; /* get next char */
    }

    //c = (int)(unsigned char)*string;

    int decSepLen = (int)strlen(decSep);
    if (strncmp(string, decSep, decSepLen) == 0 || c == '.' || c == ',')
    {
        d = 10;
        if (strncmp(string, decSep, decSepLen) == 0)
            string += decSepLen;
        else
            string++;
        c = (int)(unsigned char)*string;
        while (isdigit(c))
        {
            total += (double)(c - '0') / d;    /* accumulate digit */
            c = (int)(unsigned char)*++string; /* get next char */
            d *= 10;
        }
    }

    if (sign == '-')
        *val = -total;
    else
        *val = total; /* negate result, if necessary */

    while (*string && *string == ' ')
        ++string;

    if (*string == 0)
        return true;
    else
        return false;
}

#define GET_DWORD(dw) \
    { \
        if (size < 4) \
            return -1; \
        size -= 4; \
        dw = *(DWORD*)ptr; \
        ptr += 4; \
    }
#define GET_STRING(str) \
    { \
        DWORD s; \
        GET_DWORD(s); \
        if (size < s) \
            return -1; \
        size -= s; \
        memcpy(str, ptr, s); \
        str[s] = 0; \
        ptr += s; \
    }
#define GET_STRING_ALLOC(str) \
    { \
        DWORD s; \
        GET_DWORD(s); \
        if (size < s) \
            return -1; \
        size -= s; \
        str = (char*)realloc(str, s + 1); \
        memcpy(str, ptr, s); \
        str[s] = 0; \
        ptr += s; \
    }

DWORD ExpandSfxSettings(CSfxSettings* settings, void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("ExpandSfxSettings(, , 0x%X)", size);
    char* ptr = (char*)buffer;
    GET_DWORD(settings->Flags);
    GET_STRING(settings->Command);
    GET_STRING(settings->SfxFile);
    GET_STRING(settings->Text);
    GET_STRING(settings->Title);
    GET_STRING(settings->TargetDir);
    GET_STRING(settings->ExtractBtnText);
    GET_STRING(settings->IconFile);
    GET_DWORD(settings->IconIndex);
    GET_DWORD(settings->MBoxStyle);
    GET_STRING_ALLOC(settings->MBoxText);
    GET_STRING(settings->MBoxTitle);
    GET_STRING(settings->WaitFor);
    if (size > 0)
    {
        GET_STRING(settings->Vendor);
        GET_STRING(settings->WWW);
    }
    else
    {
        // pridame defaultni hodnoty
        CSfxLang* lang = NULL;
        char file[MAX_PATH];
        GetModuleFileName(DLLInstance, file, MAX_PATH);
        SalamanderGeneral->CutDirectory(file);
        SalamanderGeneral->SalPathAppend(file, "sfx", MAX_PATH);
        SalamanderGeneral->SalPathAppend(file, settings->SfxFile, MAX_PATH);
        if (LoadSfxFileData(file, &lang) == 0)
        {
            strcpy(settings->Vendor, lang->Vendor);
            strcpy(settings->WWW, lang->WWW);
        }
        if (lang)
            delete lang;
    }
    return (int)(ptr - (char*)buffer);
}

#define PUT_DWORD(dw) \
    { \
        if (stored + 4 > size) \
            buffer = (char*)realloc(buffer, (size = max(size * 2, size + 4))); \
        *(DWORD*)(buffer + stored) = dw; \
        stored += 4; \
    }
#define PUT_STRING(str) \
    { \
        DWORD s = lstrlen(str); \
        PUT_DWORD(s); \
        if (stored + s > size) \
            buffer = (char*)realloc(buffer, (size = max(size * 2, size + s))); \
        memcpy(buffer + stored, str, s); \
        stored += s; \
    }

DWORD PackSfxSettings(CSfxSettings* settings, char*& buffer, DWORD& size)
{
    CALL_STACK_MESSAGE2("PackSfxSettings(, , 0x%X)", size);
    DWORD stored = 0;
    PUT_DWORD(settings->Flags);
    PUT_STRING(settings->Command);
    PUT_STRING(settings->SfxFile);
    PUT_STRING(settings->Text);
    PUT_STRING(settings->Title);
    PUT_STRING(settings->TargetDir);
    PUT_STRING(settings->ExtractBtnText);
    PUT_STRING(settings->IconFile);
    PUT_DWORD(settings->IconIndex);
    PUT_DWORD(settings->MBoxStyle);
    PUT_STRING(settings->MBoxText);
    PUT_STRING(settings->MBoxTitle);
    PUT_STRING(settings->WaitFor);
    PUT_STRING(settings->Vendor);
    PUT_STRING(settings->WWW);
    return stored;
}

char* FormatMessage(char* buffer, int errorID, int lastError)
{
    CALL_STACK_MESSAGE3("FormatMessage(, %d, %d)", errorID, lastError);
    lstrcpy(buffer, LoadStr(errorID));
    if (lastError)
        ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer + lstrlen(buffer),
                        512 - lstrlen(buffer), NULL);
    return buffer;
}

LPTSTR
StrNChr(LPCTSTR lpStart, int nChar, char wMatch)
{
    CALL_STACK_MESSAGE_NONE
    if (lpStart == NULL)
        return NULL;
    int i = lstrlen(lpStart);
    if (i > nChar)
        i = nChar;
    LPCTSTR lpEnd = lpStart + nChar;
    while (lpStart < lpEnd)
    {
        if (*lpStart == wMatch)
            return (LPTSTR)lpStart;
        lpStart++;
    }
    return NULL;
}

LPTSTR StrRChr(LPCTSTR lpStart, LPCTSTR lpEnd, char wMatch)
{
    CALL_STACK_MESSAGE_NONE
    lpEnd--;
    while (lpEnd >= lpStart)
    {
        if (*lpEnd == wMatch)
            return (LPTSTR)lpEnd;
        lpEnd--;
    }
    return NULL;
}

LPTSTR TrimTralingSpaces(LPTSTR lpString)
{
    CALL_STACK_MESSAGE_NONE
    if (lpString)
    {
        char* sour = lpString + lstrlen(lpString);
        while (--sour >= lpString && *sour == ' ')
            ;
        sour[1] = 0;
    }
    return lpString;
}

/*

structure of icon resource

typedef struct
{
   BITMAPINFOHEADER   icHeader;      // DIB header
   RGBQUAD            icColors[1];   // Color table
   BYTE               icXOR[1];      // DIB bits for XOR mask
   BYTE               icAND[1];      // DIB bits for AND mask
} ICONIMAGE, *LPICONIMAGE;
*/

//***********************************************************************************
//
// Rutiny ze SHLWAPI.DLL
//
/*
BOOL PathAppend(LPTSTR  pPath, LPCTSTR pMore)
{
  if (pPath == NULL || pMore == NULL)
  {
    TRACE_E("pPath == NULL || pMore == NULL");
    return FALSE;
  }
  if (pMore[0] == 0)
  {
    TRACE_E("pMore[0] == 0");
    return TRUE;
  }
  int len = lstrlen(pPath);
  // ostrim zpetne lomitko pred pripojenim
  if (len > 1 && pPath[len - 1] != '\\' && pMore[0] != '\\')
  {
    pPath[len] = '\\';
    len++;
  }
  lstrcpy(pPath + len, pMore);
  return TRUE;
}*/
/*
BOOL PathAddExtension(LPTSTR pszPath, LPCTSTR pszExtension)
{
  if (pszPath == NULL || pszExtension == NULL)
  {
    TRACE_E("pszPath == NULL || pszExtension == NULL");
    return FALSE;
  }
  if (pszExtension[0] == 0)
    return TRUE;
  int len = lstrlen(pszPath);
  if (len > 0)
  {
    // podivam se, jestli uz tam pripona neni
    char *iterator = pszPath + len - 1;
    while (iterator >= pszPath)
    {
      if (*iterator == '.')    // ".cvspass" ve Windows je pripona
        return TRUE;  // pripona uz existuje - mizime
      if (*iterator == '\\')
        break;
      iterator--;
    }
  }
  lstrcat(pszPath, pszExtension);
  return TRUE;
}
*/
/*
BOOL PathRenameExtension(LPTSTR pszPath, LPCTSTR pszExt)
{
  if (pszPath == NULL || pszExt == NULL)
  {
    TRACE_E("pszPath == NULL || pszExtension == NULL");
    return FALSE;
  }
  if (pszExt[0] == 0)
    return TRUE;
  PathRemoveExtension(pszPath);
  lstrcat(pszPath, pszExt);
  return TRUE;
}*/

/*
LPTSTR PathFindFileName(LPCTSTR pPath)
{
  if (pPath == NULL)
  {
    TRACE_E("pPath == NULL");
    return NULL;
  }
  int len = lstrlen(pPath);
  if (len == 0)
    return (LPTSTR)pPath;

  const char *iterator = pPath + len - 1;
  if (len > 0 && *(iterator) == '\\')
    iterator--;
  while (iterator > pPath)
  {
    if (*iterator == '\\')
    {
      iterator++;
      break;
    }
    iterator--;
  }
  return (LPTSTR)iterator;
}
*/

/*void PathRemoveExtension(LPTSTR pszPath)
{
  if (pszPath == NULL)
  {
    TRACE_E("pszPath == NULL");
    return;
  }
  int len = lstrlen(pszPath);
  char *iterator = pszPath + len - 1;
  while (iterator >= pszPath)
  {
    if (*iterator == '.')   // ".cvspass" ve Windows je pripona
    {
      *iterator = 0;
      break;
    }
    if (*iterator == '\\')
      break;
    iterator--;
  }
}*/
/*
BOOL PathRemoveFileSpec(LPTSTR pszPath)
{
  if (pszPath == NULL)
  {
    TRACE_E("pszPath == NULL");
    return FALSE;
  }
  int len = lstrlen(pszPath);
  char *iterator = pszPath + len - 1;
  while (iterator >= pszPath)
  {
    if (*iterator == '\\')
    {
      if (iterator - 1 < pszPath || *(iterator - 1) == ':') iterator++;
      *iterator = 0;
      break;
    }
    iterator--;
  }
  return TRUE;
}*/
/*
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
}*/
/*
LPTSTR PathStripPath(LPTSTR pszPath)
{
 if (pszPath == NULL)
   return NULL;
 char * file = strrchr(pszPath, '\\');
 if (file)
 {
   file++;
   lstrcpy(pszPath, file);
 }
 return pszPath;
}
*/
