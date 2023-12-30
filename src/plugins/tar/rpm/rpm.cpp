// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "../gzip/gzip.h"
#include <bzlib.h>
#include "../bzip/bzip.h"
#include "rpm.h"

CRPM::CRPM(const char* filename, HANDLE file, unsigned char* buffer, unsigned long read, FILE* fContents) : CDecompressFile(filename, file, buffer, 0, read, CQuadWord(0, 0)), Stream(NULL)
{
    CALL_STACK_MESSAGE2("CRPM::CRPM(%s, , , , )", filename);

    // pokud neprosel konstruktor parenta, balime to rovnou
    if (!Ok)
        return;

    short signatureType;

    // precteme RPM lead
    if (!RPMDumpLead(fContents, signatureType) || !Ok)
    {
        // pokud doslo k chybe, nic nehlasime, jen nastavime chybovy flag
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // precteme RPM signaturu
    if (!RPMReadSignature(fContents, signatureType) || !Ok)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // precteme header
    if (!RPMReadSection(fContents) || !Ok)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // vytvorime gzipovany stream nad datovou casti
    read = (unsigned long)(DataEnd - DataStart);
    _ASSERTE(read > 512); // what should we do if there are too few bytes available???
    memmove(buffer, DataStart, read);
    Stream = new CGZip(filename, file, buffer, GetStreamPos().LoDWord, read, CQuadWord(0, 0));
    if (Stream)
    {
        if (!Stream->IsOk())
        {
            delete Stream;
            Stream = NULL;
        }
    }
    if (!Stream)
    {
        Stream = new CBZip(filename, file, buffer, GetStreamPos().LoDWord, read, CQuadWord(0, 0));
        if (Stream)
        {
            if (!Stream->IsOk())
            {
                delete Stream;
                Stream = NULL;
            }
        }
    }
    if (!Stream)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
    }
    else
    {
        // Kludge: prevent freeing in our destructor: will be freed in Stream destructor
        File = INVALID_HANDLE_VALUE;
        Buffer = NULL;
        FreeBufAndFile = FALSE;
    }
    // hotovo
}

CRPM::~CRPM(void)
{
    if (Stream)
    {
        delete Stream;
    }
}

const unsigned char* CRPM::GetBlock(unsigned short size, unsigned short* read)
{
    if (Stream)
    {
        const unsigned char* ret = Stream->GetBlock(size, read);
        if (!Stream->IsOk())
            Ok = FALSE;
        return ret;
    }
    return CDecompressFile::GetBlock(size, read);
}

void CRPM::Rewind(unsigned short size)
{
    if (Stream)
    {
        Stream->Rewind(size);
        if (!Stream->IsOk())
            Ok = FALSE;
        return;
    }
    CDecompressFile::Rewind(size);
}

void CRPM::GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr)
{
    if (Stream)
    {
        Stream->GetFileInfo(lastWrite, fileSize, fileAttr);
        if (!Stream->IsOk())
            Ok = FALSE;
        return;
    }
    CDecompressFile::GetFileInfo(lastWrite, fileSize, fileAttr);
}

// Should other functions be forwarded to Stream?????

/*const unsigned char *CRPM::FReadBlock(unsigned int number)
{
  if (Stream) {
     const unsigned char *ret = Stream->FReadBlock(number);
     if (!Stream->IsOk()) Ok = FALSE;
     return ret;
  }
  return CDecompressFile::FReadBlock(number);
}*/
