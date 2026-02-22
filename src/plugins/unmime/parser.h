// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//// main export //////////////////////////////////////////////////////////////

#include "arraylt.h"

#define BLOCK_HEADER 0     // any header
#define BLOCK_MAINHEADER 1 // mail header (start)
#define BLOCK_BODY 2       // any body
#define BLOCK_PREAMBLE 3   // MIME preamble
#define BLOCK_EPILOG 4     // MIME epilog

#define ENCODING_NONE 0    // "7bit", "8bit", "binary", ???
#define ENCODING_QP 1      // "quoted-printable"
#define ENCODING_BASE64 2  // "base64"
#define ENCODING_UU 3      // uuencode
#define ENCODING_XX 4      // xxencode
#define ENCODING_BINHEX 5  // BinHex 4.0
#define ENCODING_YENC 6    // yEncode
#define ENCODING_UNKNOWN 7 // unknown encoding - these blocks must be skipped

typedef enum eMarkerType
{
    MARKER_START = 0, // marker for the start of a block
    MARKER_END        // marker for the end of a block
} eMarkerType;

#define BADBLOCK_DAMAGED 1
#define BADBLOCK_CRC 2

class CMarker
{
public:
    eMarkerType iMarkerType; // marker type, see MARKER_XXX
    int iLine;               // line number; for StartMarker this line belongs to the block,
}; // for EndMarker the line no longer belongs to the block

class CStartMarker : public CMarker
{
public:
    char iBlockType;          // block type, see BLOCK_XXX
    char iEncoding;           // encoding, see ENCODING_XXX
    char bEmpty;              // flag indicating the block is empty (just whitespace)
    char bSelected;           // flag showing selection for decoding
    char bAttachment;         // 1 if this file is an attachment
    char cFileName[MAX_PATH]; // file name the block will be decoded into
    char cCharset[20];        // name of the character set
    int iSize;                // size of the decoded file
    int iBadBlock;            // 0 or BADBLOCK_XXX
    int iPart;                // yEnc multipart file number, 0 if not multipart
};

typedef CMarker CEndMarker;

class CParserOutput
{
public:
    CParserOutput() : Markers(100, 100, dtDelete) {};
    void SelectBlock(LPCTSTR pszFileName);
    void UnselectAll();
    void StartBlock(int iType, int iLine);
    void EndBlock(int iLine);
    void ReturnToLastStart();

    TIndirectArray<CMarker> Markers;
    CStartMarker* pCurrentBlock;
    int iLevel;
};

BOOL ParseMailFile(LPCTSTR pszFileName, CParserOutput* pOutput, BOOL bAppendCharset);

extern int iErrorStr;

//// export for decoder.cpp ///////////////////////////////////////////////////

class CInputFile
{
public:
    BOOL Open(LPCTSTR pszName);
    int ReadByte();
    BOOL ReadLine(LPTSTR pszLine);
    void Close();
    void SavePosition();
    void RestorePosition();
    int iCurrentLine;

private:
    HANDLE hFile;
    char* pBuffer;
    int iBufPos, iFilePos, iNumRead;
    int iSavedBufPos, iSavedFilePos, iSavedCurrentLine;
};

void SkipWSP(LPCSTR& pszText);
void GetWord(LPCSTR& pszText, LPSTR pszWord, int iMaxWord, LPCSTR pszDelimiters);
