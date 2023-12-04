// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "file.h"
#include "hfs.h" // To get UInt64 and other types

#define BF_BLOCKTYPE_ZERO 0
#define BF_BLOCKTYPE_COPY 1
#define BF_BLOCKTYPE_ZLIB 2
#define BF_BLOCKTYPE_BZIP2 3
#define BF_BLOCKTYPE_ADC 4
#define BF_BLOCKTYPE_UNKNOWN 255

class CBlockedFile : public CFile
{

public:
    // Used internally
    struct BlockInfo
    {
        UInt64 outPos;
        UInt64 inPos;
        UInt64 outSize;
        UInt64 inSize;
        int blockType;
    };

    class CCachedBlock
    {
    public:
        BlockInfo* pBlockInfo;
        size_t PosInBuf; // must remain unsigned!

        CCachedBlock();
        virtual ~CCachedBlock(){};
        virtual DWORD Read(char* buf, DWORD BytesToRead) = 0;
    };

    class CZeroBlock : public CCachedBlock
    {
    public:
        CZeroBlock(BlockInfo* blockInfo);
        virtual ~CZeroBlock();
        virtual DWORD Read(char* buf, DWORD BytesToRead);
    };

    class CZLIBBlock : public CCachedBlock
    {
    public:
        CZLIBBlock(BlockInfo* blockInfo, HANDLE hFile);
        virtual ~CZLIBBlock();
        virtual DWORD Read(char* buf, DWORD BytesToRead);

        bool IsOK() { return Buffer ? true : false; };

    private:
        char* Buffer;
    };

    class CBZIP2Block : public CCachedBlock
    {
    public:
        CBZIP2Block(BlockInfo* blockInfo, HANDLE hFile);
        virtual ~CBZIP2Block();
        virtual DWORD Read(char* buf, DWORD BytesToRead);

        bool IsOK() { return Buffer ? true : false; };

    private:
        char* Buffer;
    };

    class CCopyBlock : public CCachedBlock
    {
    public:
        CCopyBlock(BlockInfo* blockInfo, HANDLE hFile);
        virtual ~CCopyBlock();
        virtual DWORD Read(char* buf, DWORD BytesToRead);

    private:
        HANDLE File;
    };

public:
    CBlockedFile();
    ~CBlockedFile();

    virtual BOOL Read(LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, const char* fileName, HWND parent);
    virtual BOOL Write(LPCVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName, HWND parent);
    virtual __int64 Seek(__int64 lDistanceToMove, DWORD dwMoveMethod);

    virtual BOOL Close(LPCTSTR fileName, HWND parent);

    virtual DWORD GetFileSize(LPDWORD lpFileSizeHigh);

    virtual BOOL IsOK();

protected:
    BlockInfo* pBlocks;
    int nBlocks;

    UInt64 CurrentPos;
    UInt64 FileSize;

    CCachedBlock* BlockCache[32]; // 32 MRU blocks
    int MRUCachedBlock;           // index into MRUCachedBlock
    CCachedBlock* pCurBlock;
    bool bUnknownBlockErrShown;

    virtual CCachedBlock* LoadBlock(UInt64 pos);
};
