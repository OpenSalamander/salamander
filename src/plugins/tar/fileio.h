// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// size of file read buffer
#define BUFSIZE 0x8000 // buffer will be 32 KB

class CDecompressFile
{
public:
    // detects the archive type and returns a properly initialized object
    static CDecompressFile* CreateInstance(LPCTSTR filename, DWORD offset, CQuadWord inputSize);

    // constructor and destructor
    CDecompressFile(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord size);
    virtual ~CDecompressFile();

    virtual BOOL IsCompressed() { return FALSE; }
    virtual BOOL BuggySize() { return FALSE; }

    // returns our current state
    BOOL IsOk() { return Ok; }
    // if an error occurred, returns its code
    const unsigned int GetErrorCode() { return ErrorCode; }
    // if a system error occurred (I/O etc.) returns more detail (::GetLastError())
    const DWORD GetLastErr() { return LastError; }
    // returns the size of the archive on disk
    CQuadWord GetStreamSize() { return InputSize; }
    // returns the current position in the archive on disk
    CQuadWord GetStreamPos() { return StreamPos; }
    // returns the original file name stored in the archive (the tar name in gzip, etc.)
    const char* GetOldName();
    // returns the name of the file it works with (the archive name)
    const char* GetArchiveName() { return FileName; }

    // returns part or all of the last read block for further use
    virtual void Rewind(unsigned short size);

    virtual const unsigned char* GetBlock(unsigned short size, unsigned short* read = NULL);
    virtual void GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr);

protected:
    // reads a block from the file
    const unsigned char* FReadBlock(unsigned int number);
    // reads a byte from the file
    unsigned char FReadByte();
    // sets the original file name (if it was stored in the archive)
    void SetOldName(char* oldName);

    BOOL FreeBufAndFile;      // TRUE = we took over the file and buffer, release them in the destructor
    BOOL Ok;                  // state flag
    unsigned int ErrorCode;   // if an error occurred, it is specified here
    const char* FileName;     // archive name we are working with
    char* OldName;            // original file name before packing
    CQuadWord InputSize;      // archive size
    CQuadWord StreamPos;      // position in the archive (for progress)
    HANDLE File;              // opened archive
    DWORD LastError;          // if there was a system error (I/O...), the details are here
    unsigned char* Buffer;    // read buffer for the file
    unsigned char* DataStart; // start of unused data in the buffer
    unsigned char* DataEnd;   // end of unused data
};

class CZippedFile : public CDecompressFile
{
public:
    // constructor and destructor
    CZippedFile(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    virtual ~CZippedFile();

    virtual BOOL IsCompressed() { return TRUE; }

    virtual void GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr);
    virtual const unsigned char* GetBlock(unsigned short size, unsigned short* read);
    virtual void Rewind(unsigned short size);

protected:
    unsigned char* Window;    // output circular buffer
    unsigned char* ExtrStart; // start of unread data in circular buffer
    unsigned char* ExtrEnd;   // end of extracted data in circular buffer

    virtual BOOL DecompressBlock(unsigned short needed) = 0;
    virtual BOOL CompactBuffer();
};
