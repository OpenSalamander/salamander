// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dlldefs.h"
#include "fileio.h"

#include "gzip/gzip.h"
#include "bzip/bzlib.h"
#include "bzip/bzip.h"
#include "compress/compress.h"
#include "rpm/rpm.h"
#include "lzh/lzh.h"

#include "tar.rh"
#include "tar.rh2"
#include "lang\lang.rh"

//********************************************************
//
//  CDecompressFile
//

CDecompressFile*
CDecompressFile::CreateInstance(LPCTSTR fileName, DWORD inputOffset, CQuadWord inputSize)
{
    CALL_STACK_MESSAGE2("CDecompressFile::CreateInstance(%s)", fileName);
    // otevreme vstupni soubor
    HANDLE file = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        char txtbuf[1000];
        int err = GetLastError();
        strcpy(txtbuf, LoadStr(IDS_GZERR_FOPEN));
        strcat(txtbuf, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(txtbuf, LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return NULL;
    }
    if (inputOffset != 0)
    {
        if (SetFilePointer(file, inputOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        {
            char txtbuf[1000];
            int err = GetLastError();
            CloseHandle(file);
            strcpy(txtbuf, LoadStr(IDS_GZERR_SEEK));
            strcat(txtbuf, SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->ShowMessageBox(txtbuf, LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
            return NULL;
        }
    }
    // naalokujeme buffer pro cteni souboru
    unsigned char* buffer = (unsigned char*)malloc(BUFSIZE);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        CloseHandle(file);
        return NULL;
    }
    // precteme prvni blok dat
    DWORD read;
    if (!ReadFile(file, buffer, BUFSIZE, &read, NULL))
    {
        // chyba cteni
        char txtbuf[1000];
        int err = GetLastError();
        strcpy(txtbuf, LoadStr(IDS_ERR_FREAD));
        strcat(txtbuf, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(txtbuf, LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        free(buffer);
        CloseHandle(file);
        return NULL;
    }
    //
    // detekujeme kompresni algoritmus
    //

    // zkusime RPM
    CDecompressFile* archive = new CRPM(fileName, file, buffer, read);
    if (archive != NULL && !archive->IsOk() && archive->GetErrorCode() == 0)
    {
        delete archive;
        // pak gzip
        archive = new CGZip(fileName, file, buffer, inputOffset, read, inputSize);
        if (archive != NULL && !archive->IsOk() && archive->GetErrorCode() == 0)
        {
            // neni to gzip, zkusime compress
            delete archive;
            archive = new CCompress(fileName, file, buffer, read, inputSize);
            if (archive != NULL && !archive->IsOk() && archive->GetErrorCode() == 0)
            {
                // neni to compress, zkusime bzip
                delete archive;
                archive = new CBZip(fileName, file, buffer, inputOffset, read, inputSize);
                if (archive != NULL && !archive->IsOk() && archive->GetErrorCode() == 0)
                {
                    // neni to compress, zkusime lzh
                    delete archive;
                    archive = new CLZH(fileName, file, buffer, read);
                    if (archive != NULL && !archive->IsOk() && archive->GetErrorCode() == 0)
                    {
                        // neni to kompresene, berem zakladni tridu
                        delete archive;
                        archive = new CDecompressFile(fileName, file, buffer, inputOffset, read, inputSize);
                    }
                }
            }
        }
    }
    if (archive == NULL || !archive->IsOk())
    {
        if (archive == NULL)
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                              MSGBOX_ERROR);
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(archive->GetErrorCode(), archive->GetLastErr()),
                                              LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
            delete archive;
        }
        CloseHandle(file);
        free(buffer);
        return NULL;
    }
    return archive;
}

// class constructor
CDecompressFile::CDecompressFile(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize) : FileName(filename), File(file), Buffer(buffer), DataStart(buffer), DataEnd(buffer + read),
                                                                                                                                                           OldName(NULL), Ok(TRUE), StreamPos(start, 0), ErrorCode(0), LastError(0), FreeBufAndFile(TRUE)
{
    CALL_STACK_MESSAGE3("CDecompressFile::CDecompressFile(%s, , %u)", filename, read);

    if (CQuadWord(0, 0) == inputSize)
    {
        // zjistime velikost fajlu
        DWORD SizeLow, SizeHigh;
        SizeLow = GetFileSize(File, &SizeHigh);
        InputSize.Set(SizeLow, SizeHigh);
    }
    else
    {
        InputSize = inputSize;
    }
}

CDecompressFile::~CDecompressFile()
{
    CALL_STACK_MESSAGE1("CDecompressFile::~CDecompressFile()");
    if (FreeBufAndFile)
    {
        if (File != INVALID_HANDLE_VALUE)
            CloseHandle(File);
        if (Buffer != NULL)
            free(Buffer);
    }
    if (OldName != NULL)
        free(OldName);
}

void CDecompressFile::SetOldName(char* oldName)
{
    if (OldName != NULL)
        free(OldName);
    OldName = _strdup(oldName);
}

// nacte ze vstupniho souboru blok
const unsigned char*
CDecompressFile::FReadBlock(unsigned int number)
{
    if (!Ok)
    {
        TRACE_E("Volani ReadBlock na vadnem streamu.");
        return NULL;
    }

    // mame dost velky buffer ?
    if (number > BUFSIZE)
    {
        TRACE_E("Pozadovan prilis velky blok.");
        Ok = FALSE;
        ErrorCode = IDS_ERR_INTERNAL;
        return NULL;
    }
    // jestlize jsme na konci bufferu, musime ho reinicializovat
    if (DataEnd == DataStart && DataStart == Buffer + BUFSIZE)
    {
        DataEnd = Buffer;
        DataStart = Buffer;
    }
    // jestlize nemame dost kontinualniho mista, srazime buffer
    if (BUFSIZE - (DataStart - Buffer) < (int)number)
    {
        memmove(Buffer, DataStart, DataEnd - DataStart);
        DataEnd = Buffer + (DataEnd - DataStart);
        DataStart = Buffer;
    }
    // jestlize nemame dostatek dat k dispozici, doplnime buffer
    if (DataEnd == DataStart || (unsigned int)(DataEnd - DataStart) < number)
    {
        DWORD read = (DWORD)(Buffer + BUFSIZE - DataEnd);

        if (StreamPos.Value + read > InputSize.Value)
        {
            read = (DWORD)(InputSize.Value - StreamPos.Value);
        }

        if (!ReadFile(File, DataEnd, read, &read, NULL))
        {
            // chyba cteni
            Ok = FALSE;
            ErrorCode = IDS_ERR_FREAD;
            LastError = GetLastError();
            return NULL;
        }
        DataEnd += read;
        // nacetli jsme dostatek dat ?
        if ((unsigned int)(DataEnd - DataStart) < number)
        {
            ErrorCode = IDS_ERR_EOF;
            LastError = 0;
            return NULL;
        }
    }
    unsigned char* ret = DataStart;
    // upravime ukazatele
    DataStart += number;
    StreamPos += CQuadWord(number, 0);
    // a vratime vysledek
    return ret;
}

// nacte ze vstupniho souboru 1 byte
unsigned char
CDecompressFile::FReadByte()
{
    if (!Ok)
    {
        TRACE_E("Volani ReadByte na vadnem streamu.");
        return 0;
    }
    // jestlize je buffer prazdny, naplnime ho
    if (DataEnd - DataStart < 1)
    {
        DataStart = Buffer;
        DWORD read;
        if (StreamPos >= InputSize)
        {
            Ok = FALSE;
            ErrorCode = IDS_ERR_EOF;
            return 0;
        }
        if (!ReadFile(File, DataStart, BUFSIZE, &read, NULL))
        {
            // chyba cteni
            Ok = FALSE;
            ErrorCode = IDS_ERR_FREAD;
            LastError = GetLastError();
            return 0;
        }
        DataEnd = Buffer + read;
        // nacetli jsme dostatek dat ?
        if (read < 1)
        {
            // mame stale mene, nez je treba - chyba
            Ok = FALSE;
            ErrorCode = IDS_ERR_EOF;
            return 0;
        }
    }
    // upravime ukazatele
    ++StreamPos;
    return *(DataStart++);
}

const char*
CDecompressFile::GetOldName()
{
    if (OldName == NULL)
    {
        // vytvorime jmeno puvodniho souboru - pouzijeme jen jmeno, bez cesty
        const char* begin = FileName + strlen(FileName);
        while (begin >= FileName && *begin != '\\' && *begin != '/')
            begin--;
        begin++;
        // odrizneme priponu
        const char* end = strrchr(begin, '.');
        if (end == NULL) // ".cvspass" ve Windows je pripona
            end = begin + strlen(begin);
        OldName = (char*)malloc(end - begin + 1);
        if (OldName == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                              MSGBOX_ERROR);
            return NULL;
        }
        memcpy(OldName, begin, end - begin);
        OldName[end - begin] = '\0';
    }
    return OldName;
}

// slouzi k vraceni prave prectenych dat k novemu pouziti u kompreseneho streamu
// nedokaze vratit vic, nez posledni nacteny blok (drivejsi data uz nemusi
// byt vubec v pameti)
void CDecompressFile::Rewind(unsigned short size)
{
    if (DataStart - size >= Buffer)
    {
        DataStart -= size;
        StreamPos.Value -= size;
    }
    else
    {
        TRACE_E("Rewind - pozadovan navrat o prilis velky kus.");
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_INTERNAL), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        Ok = FALSE;
        ErrorCode = IDS_ERR_INTERNAL;
    }
}

// public funkce pro cteni ze vstupu (s pripadnou dekompresi)
const unsigned char*
CDecompressFile::GetBlock(unsigned short size, unsigned short* read /* = NULL*/)
{
    const unsigned char* src = FReadBlock(size);
    if (read != NULL)
        *read = (unsigned short)(DataEnd - DataStart);
    return src;
}

void CDecompressFile::GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr)
{
    TRACE_E("Volani GetFileInfo na necompresenem streamu.");
    memset(&lastWrite, 0, sizeof(FILETIME));
    fileSize.Set(0, 0);
    fileAttr = 0;
}

//********************************************************
//
//  CZippedFile
//

CZippedFile::CZippedFile(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize) : CDecompressFile(filename, file, buffer, start, read, inputSize), Window(NULL), ExtrStart(NULL), ExtrEnd(NULL)
{
    // pokud neprosel konstruktor parenta, balime to rovnou
    if (!Ok)
        return;

    // alokujeme circular buffer
    Window = (unsigned char*)malloc(BUFSIZE);
    if (Window == NULL)
    {
        FreeBufAndFile = FALSE;
        Ok = FALSE;
        ErrorCode = IDS_ERR_MEMORY;
        return;
    }
    ExtrStart = Window;
    ExtrEnd = Window;
}

CZippedFile::~CZippedFile()
{
    CALL_STACK_MESSAGE1("CZippedFile::~CZippedFile()");
    if (Window)
        free(Window);
}

// slouzi k vraceni prave prectenych dat k novemu pouziti u kompreseneho streamu
// nedokaze vratit vic, nez posledni nacteny blok (drivejsi data uz nemusi
// byt vubec v pameti)
void CZippedFile::Rewind(unsigned short size)
{
    if (ExtrStart - size >= Window)
        ExtrStart -= size;
    else
    {
        TRACE_E("Rewind - pozadovan navrat o prilis velky kus.");
        Ok = FALSE;
        ErrorCode = IDS_ERR_INTERNAL;
    }
}

BOOL CZippedFile::CompactBuffer()
{
    CALL_STACK_MESSAGE1("CZippedFile::CompactBuffer()");
    unsigned int old = (unsigned int)(ExtrStart - Window);
    // musime soupnout datama tak, abychom meli dost mista
    memmove(Window, ExtrStart, BUFSIZE - old);
    // a updatni ukazatele
    ExtrEnd -= old;
    ExtrStart = Window;
    return TRUE;
}

// public funkce pro cteni ze vstupu (s pripadnou dekompresi)
const unsigned char*
CZippedFile::GetBlock(unsigned short size, unsigned short* read /* = NULL*/)
{
    if (size > BUFSIZE)
    {
        TRACE_E("GetBlock - pozadovan prilis velky blok.");
        ErrorCode = IDS_ERR_INTERNAL;
        Ok = FALSE;
        return NULL;
    }
    // mame v bufferu potrebny pocet bajtu ?
    if (ExtrStart == ExtrEnd || ExtrEnd - ExtrStart < size)
    {
        // vycistili jsme cely buffer...
        if (ExtrStart == ExtrEnd && ExtrStart == Window + BUFSIZE)
        {
            ExtrStart = Window;
            ExtrEnd = Window;
        }
        // mame dost kontinualniho mista ?
        if (Window + BUFSIZE - ExtrStart < size)
            if (!CompactBuffer())
                return NULL;
        // decompress block do bufferu
        if (!DecompressBlock(size) || ExtrEnd - ExtrStart < size)
        {
            if (read != NULL)
                *read = (unsigned short)(ExtrEnd - ExtrStart);
            return NULL;
        }
    }
    if (read != NULL)
        *read = (unsigned short)(ExtrEnd - ExtrStart);
    unsigned char* ret = ExtrStart;
    // size bajtu bude pouzito...
    ExtrStart += size;
    return ret;
}

void CZippedFile::GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr)
{
    CALL_STACK_MESSAGE1("CZippedFile::GetFileInfo(,,)");

    // neni, jak zjistit velikost souboru v archivu krome dekomprese - dame nulu
    fileSize.Set(0, 0);
    // ostatni prevezmeme ze souboru s archivem
    BY_HANDLE_FILE_INFORMATION fileinfo;
    if (GetFileInformationByHandle(File, &fileinfo))
    {
        lastWrite = fileinfo.ftLastWriteTime;
        fileAttr = fileinfo.dwFileAttributes;
    }
    else
    {
        fileAttr = FILE_ATTRIBUTE_ARCHIVE;
        lastWrite.dwLowDateTime = 0;
        lastWrite.dwHighDateTime = 0;
    }
}
