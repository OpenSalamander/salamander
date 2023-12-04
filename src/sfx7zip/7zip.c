// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//
// 7zip stuff
//

//#include "StdAfx.h"

//#include "helpers.h"

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

#include <windows.h>

#include "sfx7zip.h"

#include "7zip.h"
#include "extract.h"
#include "resource.h"

#include "7zip/archive/7zHeader.h"
#include "7zip/archive/7zCrc.h"
#include "7zip/archive/7zIn.h"
#include "7zip/archive/7zExtract.h"

extern int terminate;

typedef struct _CFileInStream
{
    ISzInStream InStream;
    //  FILE *File;
    HANDLE File;
    unsigned long Offset;
} CFileInStream;

/* could be used for files smaller than 2GB */
#define GetFilePointer(hFile) SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
/* could be used for large files */
/* lpPositionHigh is a value of type PLONG. The high order long word of the current file position will be returned in lpPositionHigh */
#define GetVLFilePointer(hFile, lpPositionHigh) \
    (*lpPositionHigh = 0, \
     SetFilePointer(hFile, 0, lpPositionHigh, FILE_CURRENT))
/* forward declaration - defined in 7zIn.c */
int TestSignatureCandidate(Byte* testBytes);

/*
#ifdef _LZMA_IN_CB

#define kBufferSize (1 << 12)
Byte g_Buffer[kBufferSize];

SZ_RESULT SzFileReadImp(void *object, void **buffer, size_t maxRequiredSize, size_t *processedSize)
{
  CFileInStream *s = (CFileInStream *)object;
  size_t processedSizeLoc;
  if (maxRequiredSize > kBufferSize)
    maxRequiredSize = kBufferSize;
  processedSizeLoc = fread(g_Buffer, 1, maxRequiredSize, s->File);
  *buffer = g_Buffer;
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
  return SZ_OK;
}

#else
*/

SZ_RESULT SzFileReadImp(void* object, void* buffer, size_t size, size_t* processedSize)
{
    CFileInStream* s = (CFileInStream*)object;
    //  size_t processedSizeLoc = fread(buffer, 1, size, s->File);
    size_t processedSizeLoc;

    ReadFile(s->File, buffer, size, &processedSizeLoc, NULL);
    if (processedSize != 0)
        *processedSize = processedSizeLoc;
    return SZ_OK;
}

//#endif

SZ_RESULT SzFileSeekImp(void* object, CFileSize pos)
{
    CFileInStream* s = (CFileInStream*)object;
    //  int res = fseek(s->File, (long) pos, SEEK_SET);
    //  if (SetFilePointer(s->File, pos, NULL, FILE_BEGIN) != 0xFFFFFFFF)
    if (SetFilePointer(s->File, s->Offset + pos, NULL, FILE_BEGIN) != 0xFFFFFFFF)
        return SZ_OK;
    return SZE_FAIL;
}

/*
int Extract(char *fileName, char *targetPath) {
  CFileInStream archiveStream;
  CArchiveDatabaseEx db;
  SZ_RESULT res;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;

  archiveStream.File = fopen(fileName, "rb");
  if (archiveStream.File == 0)
  {
    // cant not open
    return 1;
  }

  archiveStream.InStream.Read = SzFileReadImp;
  archiveStream.InStream.Seek = SzFileSeekImp;

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;

  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  InitCrcTable();
  SzArDbExInit(&db);
  res = SzArchiveOpen(&archiveStream.InStream, &db, &allocImp, &allocTempImp);
  if (res == SZ_OK)
  {
    UInt32 i;

    // if you need cache, use these 3 variables.
    // if you use external function, you can make these variable as static.
    UInt32 blockIndex = 0xFFFFFFFF; // it can have any value before first call (if outBuffer = 0) 
    Byte *outBuffer = 0; // it must be 0 before first call for each new archive. 
    size_t outBufferSize = 0;  // it can have any value before first call (if outBuffer = 0) 

    for (i = 0; i < db.Database.NumFiles; i++)
    {
      size_t offset;
      size_t outSizeProcessed;
      CFileItem *f = db.Database.Files + i;
      res = SzExtract(&archiveStream.InStream, &db, i, 
          &blockIndex, &outBuffer, &outBufferSize, 
          &offset, &outSizeProcessed, 
          &allocImp, &allocTempImp);
      if (res != SZ_OK)
        break;

      if (f->IsDirectory) {
        CreateDirectory(f->Name, NULL);
      }
      else {

        HANDLE hFile = CreateFile(f->Name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) 
        {
          //PrintError("can not open output file");
          res = SZE_FAIL;
          break;
        }

        DWORD written;
        WriteFile(hFile, outBuffer + offset, outSizeProcessed, &written, NULL);
        if (written != outSizeProcessed)
        {
          //PrintError("can not write output file");
          res = SZE_FAIL;
          break;
        }

        // set file time
        if (f->IsLastWriteTimeDefined) {
          SetFileTime(hFile, NULL, NULL, &(f->LastWriteTime));
        }

        if (!CloseHandle(hFile))
        {
          //PrintError("can not close output file");
          res = SZE_FAIL;
          break;
        }

        // set attributes
        if (f->AreAttributesDefined) {
          SetFileAttributes(f->Name, (DWORD) f->Attributes);
        }
      }
    }
    allocImp.Free(outBuffer);
  }
  SzArDbExFree(&db, allocImp.Free);

  fclose(archiveStream.File);
  if (res == SZ_OK)
  {
    //printf("\nEverything is Ok\n");
    return 0;
  }

  if (res == SZE_OUTOFMEMORY)
    //PrintError("can not allocate memory");
    return 2;
//  else     
//    printf("\nERROR #%d\n", res);


  return 1;
}
*/

// over, ze jde o podporovany archiv a priprav dekompresi
int DecompressInit(struct SCabinet* cabinet)
{
    /*  unsigned long header;
  unsigned long read;

  // precti prvni 4 bajty a zkontroluj header
  if (!ReadFile(cabinet->file, &header, 4, &read, NULL) || read != 4)
    if (read != 4)
      return HandleError(ERROR_TITLE, ARC_INEOF, 0);
    else
      return HandleError(ERROR_TITLE, ERROR_INFREAD, GetLastError());
  if (header != ('M' + ('S' << 8) + ('C' << 16) + ('F' << 24)))
    return HandleError(ERROR_TITLE, ARC_FORMAT, 0);
*/

    int i;
    unsigned char data[8]; // sizeof(k7zSignature)
    DWORD read;

    // najit v archivu signaturu 7zip souboru (tam zacina 7zip archiv)
    if (!ReadFile(cabinet->file, data + 1, sizeof(k7zSignature) - 1, &read, NULL) || read != sizeof(k7zSignature) - 1)
        return 0;

    do
    {
        // posun bufferu o jednu pozici vlevo
        for (i = 0; i < sizeof(k7zSignature) - 1; i++)
            data[i] = data[i + 1];
        // pridat 1 znak
        if (!ReadFile(cabinet->file, data + sizeof(k7zSignature) - 1, 1, &read, NULL) || read != 1)
            return 0;
        // otestovat
    } while (!TestSignatureCandidate(data));

    // posunout se v souboru pred nactenou hlavicku
    SetFilePointer(cabinet->file, -(signed)sizeof(k7zSignature), NULL, FILE_CURRENT);

    cabinet->offset = GetFilePointer(cabinet->file);

    return 1;
}

const wchar_t* MyLStrChrW(const wchar_t* str, wchar_t chr)
{
    const wchar_t* iter;
    iter = str;
    while (*iter != 0 && *iter != chr)
        iter++;
    if (*iter != 0)
        return iter;
    return NULL;
}

const wchar_t* MyRStrChrW(const wchar_t* str, wchar_t chr)
{
    const wchar_t* iter;
    iter = str + lstrlenW(str);
    while (iter >= str && *iter != chr)
        iter--;
    if (iter >= str && *iter == chr)
        return iter;
    return NULL;
}

void MyCopyMemory(PVOID dst, CONST VOID* src, DWORD len)
{
    DWORD i;
    for (i = 0; i < len; i++)
    {
        *((BYTE*)dst) = *((BYTE*)src);
        ((BYTE*)dst)++;
        ((BYTE*)src)++;
    }
}

//
// Strip file name from 'dir' and create the whole path
//
BOOL CreatePathForFile(const wchar_t* dir)
{
    wchar_t buf[MAX_PATH];
    wchar_t name[MAX_PATH];

    wchar_t* s;
    const wchar_t* st;
    int len = 0;

    lstrcpyW(buf, dir);

    s = (wchar_t*)MyRStrChrW(buf, L'/');
    if (s != NULL)
        *s = L'\0'; // strip file name
    else
        return TRUE; // dir is file name

    st = buf; // + lstrlen(dir);
    while (*st != 0)
    {
        const wchar_t* slash = MyLStrChrW(st, L'/');
        if (slash == NULL)
            slash = st + lstrlenW(st);
        MyCopyMemory(name + len, st, (slash - st) * 2);
        name[len += slash - st] = 0;
        if (!CreateDirectoryW(name, NULL))
        {
            DWORD err = GetLastError();
            if (err != ERROR_ALREADY_EXISTS)
                return HandleErrorW(ERROR_TITLE, IDS_CREATEDIRFAILED, err, name);
        }
        name[len++] = L'\\';
        if (*slash == L'/')
            slash++;
        st = slash;
    }
    return TRUE;
}

extern int LzmaShouldRefreshProgress;

int UnpackArchive(struct SCabinet* cabinet)
{
    CFileInStream archiveStream;
    CArchiveDatabaseEx db;
    int res = 0;
    ISzAlloc allocImp;
    ISzAlloc allocTempImp;
    SZ_RESULT szRes;

    /*  archiveStream.File = fopen(fileName, "rb");
  if (archiveStream.File == 0)
  {
    // cant not open
    return 1;
  }
*/

    // nastavit handle predany ve strukture cabinet
    archiveStream.File = cabinet->file;
    archiveStream.Offset = cabinet->offset;

    archiveStream.InStream.Read = SzFileReadImp;
    archiveStream.InStream.Seek = SzFileSeekImp;

    allocImp.Alloc = SzAlloc;
    allocImp.Free = SzFree;

    allocTempImp.Alloc = SzAllocTemp;
    allocTempImp.Free = SzFreeTemp;

    InitCrcTable();
    SzArDbExInit(&db);
    if ((szRes = SzArchiveOpen(&archiveStream.InStream, &db, &allocImp, &allocTempImp)) == SZ_OK)
    {
        UInt32 i;
        unsigned long totalProgress = 0;
        unsigned long actualProgress = 0;

        // if you need cache, use these 3 variables.
        // if you use external function, you can make these variable as static.
        UInt32 blockIndex = 0xFFFFFFFF; // it can have any value before first call (if outBuffer = 0)
        Byte* outBuffer = 0;            // it must be 0 before first call for each new archive.
        size_t outBufferSize = 0;       // it can have any value before first call (if outBuffer = 0)

        // calc total progress
        for (i = 0; i < db.Database.NumFiles; i++)
        {
            CFileItem* f = db.Database.Files + i;
            if (f->IsDirectory)
            {
                totalProgress++;
            }
            else
            {
                totalProgress += f->Size + 1;
            }
        }

        LzmaShouldRefreshProgress = 1;

        for (i = 0; i < db.Database.NumFiles; i++)
        {
            wchar_t uName[MAX_PATH];
            int uNameLen;
            DWORD written;
            size_t offset;
            size_t outSizeProcessed;
            size_t writeSize;
            size_t writeOffset;
            CFileItem* f = db.Database.Files + i;

            if (terminate == 1)
            {
                res = 1;
                break;
            }

            // jry: soubory zrejme jiz davno nezobrazujeme
            // RefreshName(f->Name);

            if ((szRes = SzExtract(&archiveStream.InStream, &db, i, &blockIndex, &outBuffer,
                                   &outBufferSize, &offset, &outSizeProcessed, &allocImp, &allocTempImp)) != SZ_OK)
            {
                HandleError(ERROR_TITLE, (szRes == SZE_OUTOFMEMORY ? ERROR_MEMORY : ARC_BADDATA), 0, NULL);
                res = 1;
                break;
            }

            // jmena jsou v UTF-8, prevedeme na Unicode
            uNameLen = MultiByteToWideChar(CP_UTF8, 0, f->Name, -1, uName, MAX_PATH);
            uName[MAX_PATH - 1] = 0;

            if (f->IsDirectory)
            {
                actualProgress++;
                RefreshProgress(totalProgress, actualProgress, TRUE);
                CreateDirectoryW(uName, NULL);
            }
            else
            {
                HANDLE hFile;

                // create path
                CreatePathForFile(uName);

                hFile = CreateFileW(uName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    // can not open output file
                    res = HandleErrorW(ERROR_TITLE, ERROR_OUTFOPEN, GetLastError(), uName);
                    break;
                }

                // zapisujeme po mensich balikach, protoze pokus o zapsani 4MB souboru byl zablokovan
                // konkurencnim rozbalovani z archivu (i kdyz jsem se pokusil zvednout prioritu threadu)
                writeSize = outSizeProcessed;
                writeOffset = offset;
#define WRITE_BUFFER_SIZE (32 * 1024)
                while (writeSize > 0)
                {
                    size_t writeNow = min(writeSize, WRITE_BUFFER_SIZE);
                    WriteFile(hFile, outBuffer + writeOffset, writeNow, &written, NULL);
                    if (written != writeNow)
                    {
                        // can not write output file
                        res = HandleErrorW(ERROR_TITLE, ERROR_OUTFWRITE, GetLastError(), uName);
                        break;
                    }
                    writeSize -= writeNow;
                    writeOffset += writeNow;
                }
                if (writeSize > 0) // chyba
                    break;

                actualProgress += f->Size + 1;
                RefreshProgress(totalProgress, actualProgress, TRUE);

                // set file time
                if (f->IsLastWriteTimeDefined)
                {
                    SetFileTime(hFile, NULL, NULL, &(f->LastWriteTime));
                }

                if (!CloseHandle(hFile))
                {
                    // can not close output file
                    res = HandleErrorW(ERROR_TITLE, ERROR_OUTFCLOSE, GetLastError(), uName);
                    break;
                }

                // set attributes
                if (f->AreAttributesDefined)
                {
                    SetFileAttributesW(uName, (DWORD)f->Attributes);
                }
            }
        }
        allocImp.Free(outBuffer);
    }
    else
    {
        if (szRes == SZE_NOTIMPL)
            HandleError(ERROR_TITLE, ARC_METHOD, 0, NULL);
        else
            HandleError(ERROR_TITLE, ARC_INEOF, 0, NULL);

        res = 1;
    }
    SzArDbExFree(&db, allocImp.Free);

    return res;
}
