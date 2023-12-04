// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/// hlavni export /////////////////////////////////////////////////////////////

BOOL DecodeSelectedBlocks(LPCTSTR pszFileName, CParserOutput* output, LPCTSTR dir, FILETIME* pft,
                          CSalamanderForOperationsAbstract* Salamander, const CQuadWord& totalSize,
                          BOOL* pAborted = NULL, BOOL bOnlyOneFile = FALSE);

extern int iErrorStr;

/// export pro parser.cpp /////////////////////////////////////////////////////

class CDecoder
{
public:
    virtual BOOL Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize = FALSE);
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL bLastLine) { return TRUE; };
    virtual BOOL End();
    virtual void SaveState();
    virtual void RestoreState();

    int iDecodedSize;

protected:
    HANDLE HFile;
    BOOL bCalcSize;
    char* PBuffer;
    int iBufPos;
    char FileName[MAX_PATH];

    virtual BOOL BufferedWrite(const void* pData, int nBytes);
};

class CNullDecoder : public CDecoder
{
public:
    virtual BOOL Start(HANDLE, char*, BOOL) { return TRUE; };
    virtual BOOL End() { return TRUE; };
};

class CTextDecoder : public CDecoder
{
public:
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL bLastLine);
};

class CQPDecoder : public CDecoder
{
public:
    virtual BOOL Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize = FALSE);
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL bLastLine);

private:
    BYTE table[256];
};

class CBase64Decoder : public CDecoder
{
public:
    virtual BOOL Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize = FALSE);
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL);
    virtual void SaveState();
    virtual void RestoreState();

private:
    BOOL DecodeChar(char newchar);
    char c[4];
    int n;
    BOOL bDataDone;
    BYTE table[256];
};

class CUUXXDecoder : public CDecoder
{
public:
    virtual BOOL Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize = FALSE);
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL);
    BOOL bXX;

private:
    BYTE table[256];
};

class CBinHexDecoder : public CDecoder
{
public:
    virtual BOOL Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize = FALSE);
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL);
    virtual BOOL End();

    BOOL bFinished, bCRCFailed;
    int iDataLength, iResourceLength;
    char cFileName[64];

private:
    enum
    {
        CS_START,
        CS_DATA,
        CS_END
    } eCharState;

    enum
    {
        RS_NORMALBYTE,
        RS_COUNTBYTE
    } eRLEState;

    enum
    {
        BS_START,
        BS_FILENAME,
        BS_CRAP1,
        BS_DATALENGTH,
        BS_RESOURCELENGTH,
        BS_HEADERCRC,
        BS_DATA,
        BS_DATACRC,
        BS_END
    } eBinaryState;

    BOOL DecodeChar(char c);
    void DecodeRLE(BYTE b);
    void DecodeBinary(BYTE b);

    int counter, garbage_counter;
    int name_length;
    char acc[4];
    int n;
    WORD data_crc, calculated_crc;
    BYTE rle_last;
    BYTE table[256];
};

class CYEncDecoder : public CDecoder
{
public:
    virtual BOOL Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize = FALSE);
    virtual BOOL DecodeLine(LPTSTR pszLine, BOOL);

    BOOL bError;
    DWORD CRC;

private:
    UINT32 crctable[256];
};

void BuildUUTable(BYTE* pTable);
void BuildXXTable(BYTE* pTable);
