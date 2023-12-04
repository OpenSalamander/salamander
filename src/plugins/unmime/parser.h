// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//// hlavni export ////////////////////////////////////////////////////////////

#include "arraylt.h"

#define BLOCK_HEADER 0     // jakakoliv hlavicka
#define BLOCK_MAINHEADER 1 // hlavicka (zacatek) mailu
#define BLOCK_BODY 2       // jakekoliv telo
#define BLOCK_PREAMBLE 3   // MIME preambule
#define BLOCK_EPILOG 4     // MIME epilog

#define ENCODING_NONE 0    // "7bit", "8bit", "binary", ???
#define ENCODING_QP 1      // "quoted-printable"
#define ENCODING_BASE64 2  // "base64"
#define ENCODING_UU 3      // uuencode
#define ENCODING_XX 4      // xxencode
#define ENCODING_BINHEX 5  // BinHex 4.0
#define ENCODING_YENC 6    // yEncode
#define ENCODING_UNKNOWN 7 // nezname kodovani - tyto bloky nutno preskocit

typedef enum eMarkerType
{
    MARKER_START = 0, // znacka zacatku bloku
    MARKER_END        // znacka konce bloku
} eMarkerType;

#define BADBLOCK_DAMAGED 1
#define BADBLOCK_CRC 2

class CMarker
{
public:
    eMarkerType iMarkerType; // typ znacky, viz MARKER_XXX
    int iLine;               // radka, pro StartMarker tato radka do bloku patri,
};                           // pro EndMarker uz radka do bloku nepatri

class CStartMarker : public CMarker
{
public:
    char iBlockType;          // typ bloku, viz BLOCK_XXX
    char iEncoding;           // kodovani, viz ENCODING_XXX
    char bEmpty;              // priznak, ze blok je prazdny (jen whitespace)
    char bSelected;           // priznak vybrani pro dekodovani
    char bAttachment;         // 1 pokud je tento soubor attachment
    char cFileName[MAX_PATH]; // jmeno souboru do ktereho bude blok dekodovan
    char cCharset[20];        // nazev znakove sady
    int iSize;                // velikost dekodovaneho souboru
    int iBadBlock;            // 0 nebo BADBLOCK_XXX
    int iPart;                // cislo yEnc multipart souboru, 0 pokud neni multipart
};

typedef CMarker CEndMarker;

class CParserOutput
{
public:
    CParserOutput() : Markers(100, 100, dtDelete){};
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

//// export pro decoder.cpp ///////////////////////////////////////////////////

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
