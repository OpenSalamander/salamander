// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/****************************************************************************************\
**                                                                                      **
**                                                                                      **
**   parser.cpp - MIME file parser                                                      **
**                                                                                      **
**   v.1.0                                                                              **
**                                                                                      **
**   author: Jakub Cerveny                                                              **
**                                                                                      **
**                                                                                      **
\****************************************************************************************/

//
//  NOTES ABOUT THE PARSER:
//
//  Two main functions are exported from parser.cpp and decoder.cpp -
//  ParseMailFile and DecodeSelectedBlocks. Nothing else is needed from the outside.
//
//  How it works: the parser takes the mail and tries to find headers, UU/BinHex
//  blocks, etc. The output is a list of "markers" that delineate the individual
//  blocks (referencing their lines in the file). There are two types of markers -
//  start and end. The advantage is that this can also describe blocks nested in
//  other blocks.
//
//  Example - we have a mail message:
//
//    MAIL HEADER
//    ...
//    MESSAGE BODY, also containing a UUEncoded block
//    ...
//    ATTACHMENT
//
//  The parser output will look like this (S=start, E=end)
//
//    S, line 1, MAINHEADER
//    E, line 10
//
//    S, line 10, BODY, encoding=TEXT, charset=blabla, filename='yyy'
//
//    S, line 20, BODY, encoding=UUENCODE, charset='us-ascii', filename='xxx', attachment=TRUE
//    E, line 25   (end of the UU block)
//
//    E, line 80   (end of the message)
//
//    S, line 82, BODY, encoding=BASE64, filename='zzz', attachment=TRUE
//    E, line 555
//
//  The DecodeSelectedBlocks function calls itself recursively when it encounters
//  a start marker, so in the example above the UUEncoded block "eats" the lines
//  from the TEXT block. This is how UU and BinHex blocks are carved out.
//
//  The parser works in a single pass (except when it has to roll back from invalid
//  UU/BinHex blocks or from false headers, see below).
//
//  How headers are recognized: a header must have the format
//    HEADERNAME [whitespace] ':' [whitespace] TEXT
//  If the parser encounters such a line and knows HEADERNAME, it switches to the
//  'header' state. A header ends with an empty line.
//
//  A neat trick: to avoid incorrectly detecting lines that are not headers as
//  headers (for example, when an English text contains a line "Date: blablabla"),
//  each header name has its own "weight". The weights are added up and if, after
//  the header block ends, the sum of the weights is less than MIN_WEIGHT, the
//  parser rewinds and glues this "header" back to the previous block.
//
//  Mail analysis follows the MIME message syntax, i.e. multipart messages (including
//  nested multiparts) are detected and headers such as "Content-xxx" are analysed, etc.
//
//  UUEncode and BinHex blocks are completely independent of MIME syntax and can
//  appear anywhere except inside the header block. Once detected they are carved
//  out, entirely outside the MIME parser's awareness.
//
//  Required UU block syntax:
//    BEGIN whitespace sequence_of_octal_digits whitespace filename eol
//  The following lines must each start with a character that specifies the number
//  of encoded bytes. A UU block must end with a line whose first word is END. If
//  any of the conditions fails, the parser rewinds, treats the block as plain text,
//  and reports nothing. If characters from the XX charset are found on the first
//  encoded line, the parser switches to XXEncoding.
//
//  BinHex block syntax: a line containing the text "(This file must be converted
//  with BinHex 4.0)" or a line starting with ':' and exactly 64 characters long
//  is considered the start of a BinHex block (the test for the prescribed 64
//  characters per line is performed only here). Then we simply try to feed the
//  following lines into the decoder and, if they can be decoded, the block is
//  accepted as BinHex. Otherwise the parser rewinds and treats it as text, but
//  only if the header (This file..) was not found. If the header was present at
//  the beginning, damage to the BinHex block is reported.
//
//  Decoding: the program has several decoders available (with the common base
//  class CDecoder) that have three main functions: Start, DecodeLine, End. The
//  DecodeSelectedBlocks function walks through the parser output and feeds file
//  lines into the appropriate decoders, which produce the resulting files.
//
//  Decoding must also be done while parsing in order to determine the future size
//  of decoded files, see pDummyDecoder.
//
//  The program does not understand the extended UU block syntax (which is highly
//  non-standard and mostly concerns split blocks) and treats such blocks as text.
//
//  It also does not support splitting UU or BinHex blocks into multiple files.
//  WinCommander does, but it does not implement encoded files as archives and it
//  quite laboriously searches for the remaining parts of a block in other files.
//  I believe split blocks are a thing of the past (because mail servers limit the
//  size of a message), and if someone still received such files it is not a problem
//  to edit and glue them into one (assuming they even understand what is going on;
//  if not, WinCmd will not help either...).
//

#include "precomp.h"

#include "unmime.rh"
#include "unmime.rh2"
#include "lang\lang.rh"
#include "parser.h"
#include "decoder.h"
#include "unmime.h"

int iErrorStr;

// ****************************************************************************
//
//  Table of header names
//

#define MIN_WEIGHT 5

// List of header names - used to recognize the header block. 'name' is the header name,
// 'main' is a boolean that determines whether the header appears only in the main headers
// of the entire mail; 'weight' is the name weight - for a block to qualify as a header, the sum
// of the header-name weights must be at least MIN_WEIGHT.

static struct HEADERINFO
{
    const char* name;
    char main;
    char weight;
} HeaderNames[] =
    { // main=1
        {"Date", 1, 1},
        {"From", 1, 1},
        {"Sender", 1, 1},
        {"Reply-To", 1, 2},
        {"To", 1, 1},
        {"Cc", 1, 2},
        {"Bcc", 1, 2},
        {"Message-Id", 1, 2},
        {"Resent-Date", 1, 2},
        {"Resent-From", 1, 2},
        {"Resent-Sender", 1, 2},
        {"Resent-To", 1, 2},
        {"Resent-Cc", 1, 2},
        {"Resent-Bcc", 1, 2},
        {"Resent-Message-Id", 1, 2},
        {"In-Reply-To", 1, 2},
        {"Subject", 1, 2},
        {"Mime-Version", 1, MIN_WEIGHT},
        {"Return-Path", 1, 2},
        // main=0
        {"Content-Type", 0, MIN_WEIGHT},
        {"Content-Transfer-Encoding", 0, MIN_WEIGHT},
        {"Content-Disposition", 0, MIN_WEIGHT},
        {"References", 0, 1},
        {"Comments", 0, 1},
        {"Content-Location", 0, MIN_WEIGHT},
        {"Keywords", 0, 1},
        {"Received", 0, 1},
        {"Encoding", 0, 2},
        {"X-Mailer", 0, 2},
        {"X-Ms-Attachment", 0, 2},
        {"X-Priority", 0, 2},
        {"X-MSMail-Priority", 0, 2},
        {"X-Mozilla-Status", 0, 2},
        {"X-Mozilla-Status2", 0, 2},
        {"X-UIDL", 0, 2},
        {"Delivered-To", 0, 2},
        {"X-Mimeole", 0, 2},
        {"X-finfo", 0, 2},
        {"Priority", 0, 1},
        {"X-Originating-IP", 0, 2},
        {"Organization", 0, 1},
        {"Importance", 0, 1},
        {"Return-Receipt-To", 0, 2},
        {"X-Enclosure-Info", 0, 2},
        {"X-Total-Enclosures", 0, 2}};

// Note on weights: header names that are simple English words have
// weight 1. Compounds that are unlikely to appear in normal text have weight 2.
// Important headers that can form a complete header block on their own have
// MIN_WEIGHT.

static BOOL bHeaderNamesSorted = FALSE; // the list of names is sorted during the first pass
// of ParseMailFile and bHeaderNamesSorted is set to TRUE

// comparison function for qsort and bsearch
static int __cdecl compare_header_names(const void* elem1, const void* elem2)
{
    return _stricmp(((HEADERINFO*)elem1)->name, ((HEADERINFO*)elem2)->name);
}

// ****************************************************************************
//
//  CInputFile methods
//

#define TEXTBUFSIZE (256 * 1024)

BOOL CInputFile::Open(LPCTSTR pszName)
{
    CALL_STACK_MESSAGE1("CInputFile::Open()");
    if ((hFile = CreateFile(pszName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, NULL)) == INVALID_HANDLE_VALUE)
    {
        iErrorStr = -1;
        return Error(IDS_OPENERROR);
    }
    pBuffer = new char[TEXTBUFSIZE];
    if (pBuffer == NULL)
    {
        iErrorStr = IDS_LOWMEM;
        return FALSE;
    }
    iBufPos = 0;
    iNumRead = 0;
    iCurrentLine = 0;
    iFilePos = 0;
    return TRUE;
}

void CInputFile::Close()
{
    CALL_STACK_MESSAGE1("CInputFile::Close()");
    CloseHandle(hFile);
    delete[] pBuffer;
}

int CInputFile::ReadByte()
{
    if (iBufPos >= iNumRead)
    {
        if (!ReadFile(hFile, pBuffer, TEXTBUFSIZE, (DWORD*)&iNumRead, NULL) || !iNumRead)
            return EOF;
        iBufPos = 0;
        iFilePos += iNumRead;
    }
    return (unsigned char)pBuffer[iBufPos++];
}

BOOL CInputFile::ReadLine(LPSTR pszLine)
{
    int i = 0;
    int c;
    while ((c = ReadByte()) != EOF && c != 0xD && c != 0xA && i < 998)
    {
        pszLine[i++] = c;
    }
    if (c == 0xD)
    {
        int c2 = ReadByte();
        if ((c2 != 0xA) && (c2 != EOF))
        {
            // Hmmm, Macintosh-originating file? (Macintoshes use just CR's as line separators)
            // unget the byte
            // NOTE: The current (as of 2005.03.22) implementation of ReadByte() allows us to do iBufPos--
            _ASSERTE(iBufPos > 0);
            iBufPos--;
        }
    }
    pszLine[i] = 0;
    iCurrentLine++;
    return c == EOF;
}

void CInputFile::SavePosition()
{
    iSavedBufPos = iBufPos;
    iSavedFilePos = iFilePos;
    iSavedCurrentLine = iCurrentLine;
}

void CInputFile::RestorePosition()
{
    iBufPos = iSavedBufPos;
    iCurrentLine = iSavedCurrentLine;
    if (iFilePos != iSavedFilePos)
    {
        SetFilePointer(hFile, iSavedFilePos, NULL, FILE_BEGIN);
        ReadFile(hFile, pBuffer, TEXTBUFSIZE, (DWORD*)&iNumRead, NULL);
        iFilePos = iSavedFilePos;
    }
}

// ****************************************************************************
//
//  Metody CParserOutput
//

void CParserOutput::StartBlock(int iType, int iLine)
{
    CALL_STACK_MESSAGE3("CParserOutput::StartBlock(%d, %d)", iType, iLine);
    CStartMarker* m = new CStartMarker;
    m->iMarkerType = MARKER_START;
    m->iLine = iLine;
    m->iBlockType = iType;
    m->iEncoding = ENCODING_NONE;
    m->bEmpty = 1;
    m->cFileName[0] = 0;
    m->bSelected = 0;
    m->bAttachment = 0;
    m->iSize = 0;
    m->iBadBlock = 0;
    m->iPart = 0;
    strcpy(m->cCharset, "us-ascii");
    Markers.Add(m);
    pCurrentBlock = (CStartMarker*)Markers[Markers.Count - 1];
    iLevel++;
}

void CParserOutput::EndBlock(int iLine)
{
    CALL_STACK_MESSAGE2("CParserOutput::EndBlock(%d)", iLine);
    CEndMarker* m = new CEndMarker;
    m->iMarkerType = MARKER_END;
    m->iLine = iLine;
    Markers.Add(m);
    iLevel--;
    ReturnToLastStart();
}

void CParserOutput::ReturnToLastStart()
{
    int i = Markers.Count - 1, j = 0;
    while (i > 0 && (j || Markers[i]->iMarkerType != MARKER_START))
    {
        if (Markers[i]->iMarkerType == MARKER_START)
            j--;
        else
            j++;
        i--;
    }
    if (i > 0)
        pCurrentBlock = (CStartMarker*)Markers[i];
    else
        pCurrentBlock = NULL;
}

void CParserOutput::SelectBlock(LPCTSTR pszFileName)
{
    CALL_STACK_MESSAGE2("CParserOutput::SelectBlock(%s)", pszFileName);
    int i;
    for (i = 0; i < Markers.Count; i++)
    {
        CStartMarker* p = (CStartMarker*)Markers[i];
        if (p->iMarkerType == MARKER_START)
            if (!lstrcmpi(p->cFileName, pszFileName))
            {
                p->bSelected = 1;
                return;
            }
    }
}

void CParserOutput::UnselectAll()
{
    CALL_STACK_MESSAGE1("CParserOutput::UnselectAll()");
    int i;
    for (i = 0; i < Markers.Count; i++)
        if (Markers[i]->iMarkerType == MARKER_START)
            ((CStartMarker*)Markers[i])->bSelected = 0;
}

// ****************************************************************************
//
//  Helper functions for MIME decoding
//

#define LINE_MAX 10000
#define NEXTLINE_MAX 1000
#define NAME_MAX 50
#define TEXT_MAX 1000
#define CURRENTCHARSET_MAX 20

// states the parser can be in
typedef enum eParserState
{
    STATE_UNRECOGNIZED = 0,
    STATE_HEADER,
    STATE_BODY
} eParserState;

static CInputFile InputFile;
static char* cLine;
static char* cNextLine;
static char* cSavedLine;
static char* cSavedNextLine;
static char* cName;
static char* cText;
static char iCurrentEncoding, iSavedCurrentEncoding;
static char* cCurrentCharset;
static int iMessageNumber, iBinaryNumber;
static char* cFileName;
static int iNameOrigin; // name origin: 0..default, 1..content-type, 2..content-disposition, 3..content-location
static int* piDefaultNumber;
static CDecoder* pDummyDecoder; // decoder used to compute the size of files stored in the "archive"
static eParserState iState, iSavedState;
static BOOL bLast, bNextLast, bSavedLast, bSavedNextLast;

// stack for nested multiparts
#define MULTIPARTSTACK_MAX 5
#define BOUNDARY_MAX 200
static char (*cBoundaries)[200]; // stack of multipart boundaries
static int iMultipart;           // indicates the nesting level of the multipart and also the stack top
static int iSavedMultipart;
#define STACKTOP (iMultipart - 1)

////// TEXT HANDLING /////////////////////////////////////////////////////////////

static BOOL IsWhiteLine(LPSTR pszLine)
{
    //CALL_STACK_MESSAGE2("IsWhiteLine(%s)", pszLine);
    for (; *pszLine; pszLine++)
        if (*pszLine != ' ' || *pszLine != '\t')
            return FALSE;
    return TRUE;
}

void SkipWSP(LPCSTR& pszText)
{
    //CALL_STACK_MESSAGE1("SkipWSP()");
    while (*pszText && (*pszText == ' ' || *pszText == '\t'))
    {
        pszText++;
    }
}

static void SkipCWSP(LPCSTR& pszText)
{
    //CALL_STACK_MESSAGE1("SkipCWSP()");
    while (*pszText && (*pszText == ' ' || *pszText == '\t'))
    {
        if (*pszText == '(')
        {
            int n = 1;
            pszText++;
            while (*pszText && n)
            {
                if (*pszText == '(')
                    n++;
                else if (*pszText == ')')
                    n--;
                pszText++;
            }
        }
        else
            pszText++;
    }
}

static void SkipChars(LPCSTR& pszText, LPCSTR pszChars)
{
    while (*pszText && strchr(pszChars, *pszText) != NULL)
    {
        pszText++;
    }
}

static void TrimChars(LPSTR pszText, LPCSTR pszChars)
{
    int i = (int)strlen(pszText) - 1;
    while (i >= 0 && strchr(pszChars, pszText[i]) != NULL)
        pszText[i--] = 0;
    LPCSTR p = pszText;
    SkipChars(p, pszChars);
    if (p > pszText)
        strcpy(pszText, p);
}

void GetWord(LPSTR& pszText, LPSTR pszWord, int iMaxWord, LPCSTR pszDelimiters)
{
    //CALL_STACK_MESSAGE4("GetWord(%s, , %d, %s)", pszText, iMaxWord, pszDelimiters);
    while (pszText && --iMaxWord && strchr(pszDelimiters, *pszText) == NULL)
    {
        *pszWord++ = *pszText++;
    }
    *pszWord = 0;
}

void GetWord(LPCSTR& pszText, LPSTR pszWord, int iMaxWord, LPCSTR pszDelimiters)
{
    //CALL_STACK_MESSAGE4("GetWord(%s, , %d, %s)", pszText, iMaxWord, pszDelimiters);
    while (pszText && --iMaxWord && strchr(pszDelimiters, *pszText) == NULL)
    {
        *pszWord++ = *pszText++;
    }
    *pszWord = 0;
}

////// HEADER PARSING ///////////////////////////////////////////////////////////

static BOOL ParseHeader(LPCSTR pszLine, LPSTR pszName, int iMaxName, LPSTR pszText, int iMaxText)
{
    //CALL_STACK_MESSAGE4("ParseHeader(%s, , %d, , %d)", pszLine, iMaxName, iMaxText);
    GetWord(pszLine, pszName, iMaxName, " \t:()");
    SkipWSP(pszLine);
    if (*pszLine == ':')
        pszLine++;
    else
    {
        pszName[0] = 0;
        pszText[0] = 0;
        return FALSE;
    }
    SkipCWSP(pszLine);
    lstrcpyn(pszText, pszLine, iMaxText);
    return TRUE;
}

static void ParseContentType(LPCSTR pszText, LPSTR pszType, int iMaxType, LPSTR pszSubType, int iMaxSubType)
{
    CALL_STACK_MESSAGE4("ParseContentType(%s, , %d, , %d)", pszText, iMaxType, iMaxSubType);
    GetWord(pszText, pszType, iMaxType, "/ \t()");
    SkipCWSP(pszText);
    if (*pszText == '/')
        pszText++;
    SkipCWSP(pszText);
    GetWord(pszText, pszSubType, iMaxSubType, "; \t()");
}

static BOOL GetParameter(LPSTR pszText, LPCSTR pszParam, LPSTR pszBuffer, int iBufferSize)
{
    CALL_STACK_MESSAGE3("GetParameter(%s, , , %d)", pszText, iBufferSize);
    // create lowercase copies of the strings so I can search case-insensitively,
    // I was afraid to use StrStrI... (I do not know if it is available on all systems)
    char* text = new char[strlen(pszText) + 1];
    strcpy(text, pszText);
    CharLower(text);
    char* param = new char[strlen(pszParam) + 1];
    strcpy(param, pszParam);
    CharLower(param);
    // ---
    char* substr = strstr(text, param);
    while (substr != NULL)
    {
        if (substr == text || strchr(" \t;()", substr[-1]) != NULL)
        {
            const char* p = substr + strlen(param);
            SkipCWSP(p);
            if (*p == '=')
            {
                SkipCWSP(++p);
                p = p - text + pszText;
                if (*p == '"')
                    GetWord(++p, pszBuffer, iBufferSize, "\"");
                else
                    GetWord(p, pszBuffer, iBufferSize, " \t;()");
                delete[] text;
                delete[] param;
                return TRUE;
            }
        }
        substr = strstr(substr + 1, pszParam);
    }
    delete[] text;
    delete[] param;
    return FALSE;
}

static HEADERINFO* GetHeaderInfo(LPSTR pszName)
{
    //CALL_STACK_MESSAGE2("GetHeaderInfo(%s)", pszName);
    HEADERINFO hi;
    hi.name = pszName;
    return (HEADERINFO*)bsearch(&hi, HeaderNames, sizeof(HeaderNames) / sizeof(HEADERINFO), sizeof(HEADERINFO),
                                compare_header_names);
}

static BOOL IsHeader()
{
    //CALL_STACK_MESSAGE1("IsHeader()");
    if (ParseHeader(cLine, cName, NAME_MAX, cText, TEXT_MAX) && GetHeaderInfo(cName) != NULL)
        return TRUE;

    char temp[10];
    const char* p = cLine;        // Check for the special case when the header block starts
    SkipWSP(p);                   // with a "From" line, which syntactically differs from the others.
    GetWord(p, temp, 10, "- \t"); // If the next line is already a header,
    return (                      // accept the "From" line as a header as well.
        !_stricmp(temp, "From") &&
        ParseHeader(cNextLine, cName, NAME_MAX, cText, TEXT_MAX) &&
        GetHeaderInfo(cName) != NULL);
}

////// BOUNDARIES ///////////////////////////////////////////////////////////////

static BOOL IsBoundary(LPCSTR pszBoundary, BOOL* bEnd)
{
    //CALL_STACK_MESSAGE1("IsBoundary(, )");
    const char* p = cLine;
    SkipWSP(p);
    if (p[0] == '-' && p[1] == '-')
    {
        int l = (int)strlen(pszBoundary);
        if (!memcmp(p + 2, pszBoundary, l))
        {
            p += l + 2;
            if (p[0] && p[0] == '-' && p[1] == '-')
                *bEnd = TRUE;
            else
                *bEnd = FALSE;
            return TRUE;
        }
    }
    return FALSE;
}

static void PushBoundary(LPSTR pszBoundary)
{
    CALL_STACK_MESSAGE1("PushBoundary()");
    iMultipart++;
    strcpy(cBoundaries[STACKTOP], pszBoundary);
}

static void PopBoundary()
{
    CALL_STACK_MESSAGE1("PopBoundary()");
    if (iMultipart > 0)
        iMultipart--;
}

static void DestroyIllegalChars(LPSTR pszPath)
{
    CALL_STACK_MESSAGE2("DestroyIllegalChars(%s)", pszPath);
    while (*pszPath)
    {
        if (strchr("*?\\/<>|\":", *pszPath) != NULL)
            *pszPath = '_';
        pszPath++;
    }
}

////// JMENA DEKODOVANYCH SOUBORU //////////////////////////////////////////////

static void SetDefaultFileName(BOOL bAppendCharset)
{
    CALL_STACK_MESSAGE1("SetDefaultFileName()");
    iNameOrigin = 0;
    sprintf(cFileName, "message%#03ld", iMessageNumber);
    if (bAppendCharset && _stricmp(cCurrentCharset, "us-ascii"))
    {
        strcat(cFileName, "(");
        strcat(cFileName, cCurrentCharset);
        strcat(cFileName, ")");
        DestroyIllegalChars(cFileName);
    }
    strcat(cFileName, ".txt");
    piDefaultNumber = &iMessageNumber;
}

static void SetDefaultMIMEFileName(LPCSTR pszType, LPCSTR pszSubType, BOOL bAppendCharset)
{
    CALL_STACK_MESSAGE3("SetDefaultMIMEFileName(%s, %s)", pszType, pszSubType);
    iNameOrigin = 0;
    if (!_stricmp(pszType, "text") || !_stricmp(pszType, "message"))
    {
        if (!_stricmp(pszSubType, "html"))
        {
            sprintf(cFileName, "message%#03ld.htm", iMessageNumber);
            piDefaultNumber = &iMessageNumber;
        }
        else
            SetDefaultFileName(bAppendCharset);
    }
    else
    {
        sprintf(cFileName, "binary%#03ld.bin", iBinaryNumber);
        piDefaultNumber = &iBinaryNumber;
    }
}

static void InsertSuffix(char* filename, int suffix)
{
    char temp[MAX_PATH];
    char* ext = strrchr(filename, '.');
    if (ext != NULL) // ".cvspass" is an extension in Windows
    {
        *ext++ = 0;
        sprintf(temp, "%s(%ld).%s", filename, suffix, ext);
    }
    else
        sprintf(temp, "%s(%ld)", filename, suffix);
    strcpy(filename, temp);
}

static int __cdecl compare_file_names(const void* elem1, const void* elem2)
{
    return _stricmp(*((char**)elem1), *((char**)elem2));
}

static void MakeNamesUnique(CParserOutput* pOutput)
{
    CALL_STACK_MESSAGE1("MakeNamesUnique()");
    int numblocks = pOutput->Markers.Count / 2;
    char** index = new char*[numblocks];
    // build an index of file names
    int i, j;
    for (i = 0, j = 0; i < pOutput->Markers.Count; i++)
    {
        if (pOutput->Markers[i]->iMarkerType == MARKER_START)
        {
            if (j >= numblocks) // should not happen, but just to be safe...
            {
                TRACE_E("MakeNamesUnique(): inconsistency in parser output - numblocks != pOutput->Markers.Count / 2");
                delete[] index;
                return;
            }
            index[j++] = ((CStartMarker*)pOutput->Markers[i])->cFileName;
        }
    }
    // sort the index
    qsort(index, numblocks, sizeof(char*), compare_file_names);
    // identical names now lie next to each other and we can easily catch them
    int k, l;
    for (k = 1, l = 0; k < numblocks; k++)
    {
        if (!_stricmp(index[k], index[l]))
            InsertSuffix(index[k], k - l);
        else
            l = k;
    }
    delete[] index;
}

// custom decoders for decoding encoded names (they decode into the buffer)
class CBase64MiniDecoder : public CBase64Decoder
{
public:
    CBase64MiniDecoder(char*& ptr) : q(ptr) {}
    virtual BOOL BufferedWrite(void* pBuffer, int nBytes)
    {
        *q++ = *((char*)pBuffer);
        return TRUE;
    };
    char*& q;
};

class CQPMiniDecoder : public CQPDecoder
{
public:
    CQPMiniDecoder(char*& ptr) : q(ptr) {}
    virtual BOOL BufferedWrite(void* pBuffer, int nBytes)
    {
        *q++ = *((char*)pBuffer);
        return TRUE;
    };
    char*& q;
};

static BOOL DecodeWord(const char*& p, char*& q)
{
    CALL_STACK_MESSAGE1("DecodeWord()");
    char conversion[200]; // space for the code page (51 characters) plus the target code-page suffix ('-' and ' ' are ignored)
    GetWord(p, conversion, 50, "?");
    if (*p++ != '?')
        return FALSE;
    char text[500];
    GetWord(p, text, 2, "?");
    if (*p++ != '?')
        return FALSE;
    char enc = tolower(text[0]);
    if (enc != 'q' && enc != 'b')
        return FALSE;
    GetWord(p, text, 500, "?");
    if (p[0] != '?' || p[1] != '=')
        return FALSE;
    p += 2;
    char* start = q;
    if (enc == 'q')
    {
        CQPMiniDecoder Decoder(q);
        Decoder.Start(NULL, NULL, TRUE);
        Decoder.DecodeLine(text, TRUE);
    }
    else
    {
        CBase64MiniDecoder Decoder(q);
        Decoder.Start(NULL, NULL, TRUE);
        Decoder.DecodeLine(text, TRUE);
    }

    // 'conversion' is the conversion that needs to be performed (conversion to the code page used by Windows);
    // 'start' through 'q' is the string to recode
    char codePage[101];
    SalamanderGeneral->GetWindowsCodePage(NULL, codePage);
    if (codePage[0] != 0) // only if the Windows code page is known
    {
        strcat(conversion, codePage);
        char table[256];
        if (SalamanderGeneral->GetConversionTable(NULL, table, conversion))
        {
            char* s = start;
            while (s < q)
            {
                *s = table[(unsigned char)*s];
                s++;
            }
        }
    }
    return TRUE;
}

// this function decodes special file names, e.g. "=?iso-8859-2?B?vmx1u2916Gv9IGv58i54eHg=?="
// it requires modified QP and Base64 decoders
static void DecodeSpecialWords(LPSTR pszText)
{
    CALL_STACK_MESSAGE2("DecodeSpecialWords(%s)", pszText);
    char temp[MAX_PATH];
    int i = 0, j = 0;
    while (pszText[i])
    {
        if (pszText[i] == '=' && pszText[i + 1] == '?')
        {
            const char* p = pszText + i + 2;
            char* q = temp + j;
            if (!DecodeWord(p, q))
                temp[j++] = pszText[i++];
            else
            {
                i = (int)(p - pszText);
                j = (int)(q - temp);
            }
        }
        else
            temp[j++] = pszText[i++];
    }
    temp[j] = 0;
    strcpy(pszText, temp);
}

////////////////////////////////////////////////////////////////////////////////

static void CreateDecoder()
{
    CALL_STACK_MESSAGE1("CreateDecoder()");
    switch (iCurrentEncoding)
    {
    case ENCODING_NONE:
        pDummyDecoder = new CTextDecoder;
        break;
    case ENCODING_QP:
        pDummyDecoder = new CQPDecoder;
        break;
    case ENCODING_BASE64:
        pDummyDecoder = new CBase64Decoder;
        break;
    default:
        pDummyDecoder = NULL;
    }
}

static void EndCalcSize(CParserOutput* pOutput)
{
    CALL_STACK_MESSAGE1("EndCalcSize()");
    if (pDummyDecoder != NULL)
    {
        pOutput->pCurrentBlock->iSize = pDummyDecoder->iDecodedSize;
        delete pDummyDecoder;
        pDummyDecoder = NULL;
    }
}

////// FUNKCE PRO ULOZENI/OBNOVENI STAVU PARSERU ///////////////////////////////

static void SaveState()
{
    CALL_STACK_MESSAGE1("SaveState()");
    strcpy(cSavedLine, cLine);
    strcpy(cSavedNextLine, cNextLine);
    InputFile.SavePosition();
    iSavedCurrentEncoding = iCurrentEncoding;
    iSavedState = iState;
    bSavedLast = bLast;
    bSavedNextLast = bNextLast;
    iSavedMultipart = iMultipart;
    if (pDummyDecoder != NULL)
        pDummyDecoder->SaveState();
}

static void RestoreState(BOOL bRestoreDecoder = FALSE)
{
    CALL_STACK_MESSAGE2("RestoreState(%d)", bRestoreDecoder);
    strcpy(cLine, cSavedLine);
    strcpy(cNextLine, cSavedNextLine);
    InputFile.RestorePosition();
    iCurrentEncoding = iSavedCurrentEncoding;
    iState = iSavedState;
    bLast = bSavedLast;
    bNextLast = bSavedNextLast;
    iMultipart = iSavedMultipart;
    if (bRestoreDecoder)
    {
        CreateDecoder();
        if (pDummyDecoder != NULL)
            pDummyDecoder->RestoreState();
    }
}

static void EverythingBaaack(CParserOutput* pOutput, int plus)
{
    CALL_STACK_MESSAGE1("EverythingBack()");
    int iEndLine = InputFile.iCurrentLine;
    RestoreState(TRUE);
    TIndirectArray<CMarker>* p = &pOutput->Markers;
    if (p->Count)
        p->Delete(p->Count - 1);
    if (p->Count)
        p->Delete(p->Count - 1);
    pOutput->ReturnToLastStart();
    int numlines = iEndLine - InputFile.iCurrentLine + plus;
    int i;
    for (i = 0; i < numlines; i++)
    {
        if (pDummyDecoder != NULL)
            pDummyDecoder->DecodeLine(cLine, bLast);
        if (i < numlines - plus)
        {
            strcpy(cLine, cNextLine);
            bLast = bNextLast;
            bNextLast = InputFile.ReadLine(cNextLine);
        }
    }
}

// ****************************************************************************
//
//  TestUUBlock - if it finds a UU/XX block, it checks whether it is valid and adds
//  it to ParserOutput; otherwise it returns to the original line
//

static BOOL TestUUBlock(CParserOutput* pOutput, BOOL& bEnd)
{
    bEnd = FALSE;
    // are we on a UU header?
    char text[8];
    char filename[MAX_PATH];
    const char* line = cLine;
    SkipWSP(line);
    GetWord(line, text, 8, " \t");
    if (_stricmp(text, "begin"))
        return FALSE; // is the first word 'begin'?
    SkipWSP(line);
    if (!*line)
        return FALSE;
    while (*line && *line != ' ' && *line != '\t') // is a sequence of octal digits following?
        if ((BYTE)*line < (BYTE)'0' || (BYTE)*line > (BYTE)'7')
            return FALSE;
        else
            line++;
    SkipWSP(line);
    if (!*line)
        return FALSE;
    GetWord(line, filename, MAX_PATH, " \t"); // the file name also has to be present
    SkipWSP(line);
    if (*line)
        return FALSE; // and nothing more

    // we found a UU header
    SaveState(); // save the position in case we need to return
    int iLineStart = InputFile.iCurrentLine;
    int size = 0;
    strcpy(cLine, cNextLine); // next line

    BYTE uutable[256], xxtable[256], *table = uutable;
    BuildUUTable(uutable);
    BuildXXTable(xxtable);

    int i;
    for (i = 0; cLine[i]; i++) // test for the XX character set
        if (uutable[(BYTE)cLine[i]] == 255 && xxtable[(BYTE)cLine[i]] != 255)
        {
            table = xxtable; // switch to XX
            break;
        }

    do
    {
        if (!cLine[0]) // empty lines are not allowed
        {
            RestoreState();
            return FALSE;
        }

        line = cLine;
        SkipWSP(line);
        GetWord(line, text, 8, " \t");
        if (!_stricmp(text, "end"))
        { // we successfully reached the end, add the block to the output
            pOutput->StartBlock(BLOCK_BODY, iLineStart);
            CStartMarker* p = pOutput->pCurrentBlock;
            p->iEncoding = table == uutable ? ENCODING_UU : ENCODING_XX;
            p->bAttachment = 1;
            p->bEmpty = 0;
            p->iSize = size;
            strcpy(p->cCharset, "us-ascii");
            strcpy(p->cFileName, filename);
            DestroyIllegalChars(p->cFileName);
            pOutput->EndBlock(InputFile.iCurrentLine + 2);
            return TRUE;
        }

        int j;
        for (j = 0; cLine[j]; j++)
            if (table[(BYTE)cLine[j]] == 255) // invalid characters are not allowed
            {
                RestoreState();
                return FALSE;
            }

        int numbytes = table[(BYTE)cLine[0]];
        if ((int)(strlen(cLine) - 1) < (numbytes * 3 / 4)) // there must be enough characters on the line...
        {                                                  // Note: there may sometimes be extra characters on the line, so we do not check for equality.
            RestoreState();
            return FALSE;
        }
        size += numbytes;
    } while ((bEnd = InputFile.ReadLine(cLine)) == 0 || cLine[0]);

    RestoreState();
    return FALSE;
}

// ****************************************************************************
//
//  TestYEncBlock - if it finds a yEnc block, it checks whether it is valid and
//  adds it to ParserOutput; otherwise it returns to the original line
//

static BOOL TestYEncBlock(CParserOutput* pOutput, BOOL& bEnd)
{
    // are we on a header?
    if (memcmp(cLine, "=ybegin", 7))
        return FALSE;

    // yes, extract the attributes line, size, part, and name
    const char* line = cLine + 8;
    char text[50], value[50], filename[MAX_PATH];
    int size, part = 0, attrLine = 0, attrSize = 0, attrName = 0, partsize;
    while (*line)
    {
        SkipWSP(line);
        GetWord(line, text, 50, " \t=");
        SkipChars(line, " \t=");
        if (!strcmp(text, "line"))
        {
            attrLine = 1;
            GetWord(line, text, 50, " \t");
        }
        else if (!strcmp(text, "size"))
        {
            attrSize = 1;
            GetWord(line, text, 50, " \t");
            size = atol(text);
        }
        else if (!strcmp(text, "name"))
        {
            attrName = 1;
            strncpy_s(filename, line, _TRUNCATE);
            TrimChars(filename, " \"");
            line = "";
        }
        else if (!strcmp(text, "part"))
        {
            GetWord(line, text, 50, " \t");
            part = atol(text);
        }
        else
            GetWord(line, text, 50, " \t");
    }

    // line, size, and name are mandatory
    if (!attrLine || !attrSize || !attrName)
        return FALSE;

    // if this is a multipart file, the next line must start with =ypart
    if (part)
        if (!memcmp(cNextLine, "=ypart", 6))
        {
            line = cNextLine + 7;
            int begin, end, attrBegin = 0, attrEnd = 0;
            while (*line)
            {
                SkipWSP(line);
                GetWord(line, text, 50, " \t=");
                SkipChars(line, " \t=");
                GetWord(line, value, 50, " \t");
                if (!strcmp(text, "begin"))
                {
                    attrBegin = 1;
                    begin = atol(value);
                }
                else if (!strcmp(text, "end"))
                {
                    attrEnd = 1;
                    end = atol(value);
                }
            }
            partsize = end - begin + 1;
            if (!attrBegin || !attrEnd || partsize < 0 || (bEnd = InputFile.ReadLine(cNextLine)) != 0)
                return FALSE;
        }
        else
            return FALSE;

    SaveState();
    CYEncDecoder* pDecoder = new CYEncDecoder;
    pDecoder->Start(NULL, NULL, TRUE);
    int iLineStart = InputFile.iCurrentLine;
    strcpy(cLine, cNextLine);

    do
    {
        line = cLine;
        GetWord(line, text, 6, " \t");
        if (!strcmp(text, "=yend")) // are we at the end?
        {
            int endsize, endpart, attrCRC = 0, attrPartCRC = 0;
            attrSize = 0;
            DWORD CRC, partCRC;
            while (*line)
            {
                SkipWSP(line);
                GetWord(line, text, 50, " \t=");
                SkipChars(line, " \t=");
                GetWord(line, value, 50, " \t");
                if (!strcmp(text, "size"))
                {
                    attrSize = 1;
                    endsize = atol(value);
                }
                else if (!strcmp(text, "part"))
                {
                    endpart = atol(value);
                }
                else if (!strcmp(text, "crc32"))
                {
                    attrCRC = 1;
                    sscanf(value, "%x", &CRC);
                }
                else if (!strcmp(text, "pcrc32"))
                {
                    attrPartCRC = 1;
                    sscanf(value, "%x", &partCRC);
                }
            }

            pOutput->StartBlock(BLOCK_BODY, iLineStart);
            CStartMarker* p = pOutput->pCurrentBlock;

            if (!attrSize || (!part && (endsize != size || size != pDecoder->iDecodedSize)) ||
                (part && (partsize != endsize || partsize != pDecoder->iDecodedSize)))
                p->iBadBlock = BADBLOCK_DAMAGED;
            else if ((!part && attrCRC && CRC != pDecoder->CRC) ||
                     (part && attrPartCRC && partCRC != pDecoder->CRC))
                p->iBadBlock = BADBLOCK_CRC;

            p->iEncoding = ENCODING_YENC;
            p->bAttachment = 1;
            p->bEmpty = 0;
            p->iSize = pDecoder->iDecodedSize;
            strcpy(p->cCharset, "us-ascii");
            if (part)
                sprintf(p->cFileName, "%s.%03d", filename, part);
            else
                strcpy(p->cFileName, filename);
            DestroyIllegalChars(p->cFileName);
            pOutput->EndBlock(InputFile.iCurrentLine + 2);
            delete pDecoder;
            return TRUE;
        }
        else if (!pDecoder->DecodeLine(cLine, FALSE))
            break;
    } while ((bEnd = InputFile.ReadLine(cLine)) == 0 || cLine[0]);

    RestoreState();
    delete pDecoder;
    return FALSE;
}

// ****************************************************************************
//
//  TestBinHexBlock - if it finds a BinHex block, it checks whether it is valid and
//  adds it to ParserOutput; otherwise it returns to the original line
//

static BOOL TestBinHexBlock(CParserOutput* pOutput, BOOL& bEnd)
{
    BOOL bNoHeader = FALSE;
    if (memcmp(cLine, "(This file must be converted with BinHex 4.0)", 45) &&
        (bNoHeader = TRUE, (cLine[0] != ':' || strlen(cLine) != 64)))
        return FALSE;

    if (!bNoHeader)
        strcpy(cLine, cLine + 45);
    SaveState();
    CBinHexDecoder* pDecoder = new CBinHexDecoder;
    pDecoder->Start(NULL, NULL, TRUE);
    int iLineStart = InputFile.iCurrentLine;
    strcat(cLine, cNextLine);

    do
    {
        if (!pDecoder->DecodeLine(cLine, FALSE))
            break;
        if (pDecoder->bFinished)
        {
            if (!bNoHeader && pDecoder->iDecodedSize != pDecoder->iDataLength)
                break;
            pDecoder->End();
            pOutput->StartBlock(BLOCK_BODY, iLineStart);
            CStartMarker* p = pOutput->pCurrentBlock;
            p->iEncoding = ENCODING_BINHEX;
            p->bAttachment = 1;
            p->bEmpty = 0;
            p->iSize = pDecoder->iDecodedSize;
            if (pDecoder->iDecodedSize != pDecoder->iDataLength)
                p->iBadBlock = BADBLOCK_DAMAGED;
            else if (pDecoder->bCRCFailed)
                p->iBadBlock = BADBLOCK_CRC;
            strcpy(p->cCharset, "us-ascii");
            strcpy(p->cFileName, pDecoder->cFileName);
            DestroyIllegalChars(p->cFileName);
            pOutput->EndBlock(InputFile.iCurrentLine + 2);
            delete pDecoder;
            return TRUE;
        }
    } while ((bEnd = InputFile.ReadLine(cLine)) == 0 || cLine[0]);

    RestoreState();
    delete pDecoder;
    return FALSE;
}

// ****************************************************************************
//
//  ParseMailFile - main function for decoding mail
//

BOOL ParseMailFile(LPCTSTR pszFileName, CParserOutput* pOutput, BOOL bAppendCharset)
{
    CALL_STACK_MESSAGE2("ParseMailFile(%s, )", pszFileName);

    if (!bHeaderNamesSorted)
    {
        qsort(HeaderNames, sizeof(HeaderNames) / sizeof(HEADERINFO),
              sizeof(HEADERINFO), compare_header_names);
        bHeaderNamesSorted = TRUE;
    }

    if (!InputFile.Open(pszFileName))
        return FALSE;

    cLine = new char[10000];
    cNextLine = new char[1000];
    cSavedLine = new char[10000];
    cSavedNextLine = new char[1000];
    cName = new char[50];
    cText = new char[1000];
    cCurrentCharset = new char[20];
    cFileName = new char[MAX_PATH];
    cBoundaries = new char[MULTIPARTSTACK_MAX][200];

    BOOL goback, bEnd;
    CStartMarker* pMultipartStart;
    BOOL bNextBlockIsAttachment;
    int iWeightSum;

    iState = STATE_UNRECOGNIZED;
    InputFile.iCurrentLine = -1; // because we read one line ahead
    iMultipart = 0;
    iCurrentEncoding = ENCODING_UNKNOWN;
    iMessageNumber = iBinaryNumber = 0;
    piDefaultNumber = &iMessageNumber;
    strcpy(cCurrentCharset, "us-ascii");
    pOutput->iLevel = 0;
    bNextLast = InputFile.ReadLine(cLine);
    iNameOrigin = 0;

    do
    {
        bLast = bNextLast;
        bNextLast = InputFile.ReadLine(cNextLine);

        do
        {
            goback = FALSE;
            if (!bLast && !bNextLast && iState != STATE_HEADER)
            {
                if (TestUUBlock(pOutput, bEnd) || TestBinHexBlock(pOutput, bEnd) || TestYEncBlock(pOutput, bEnd))
                {
                    if (bEnd)
                        goto skipout;
                    bLast = InputFile.ReadLine(cLine);
                    bNextLast = InputFile.ReadLine(cNextLine);
                    goback = TRUE;
                }
            }
        } while (goback);

        do
        {
            goback = FALSE; // this part tracks multipart boundaries
            if (iMultipart)
            {
                BOOL bEnd2;
                if (IsBoundary(cBoundaries[STACKTOP], &bEnd2))
                {
                    EndCalcSize(pOutput);
                    if (iState == STATE_HEADER && iWeightSum < MIN_WEIGHT)
                    {
                        EverythingBaaack(pOutput, 0);
                    }
                    else
                    {
                        pOutput->EndBlock(InputFile.iCurrentLine);
                        if (bEnd2)
                        {
                            if (!bLast)
                            {
                                pOutput->StartBlock(BLOCK_EPILOG, InputFile.iCurrentLine + 1);
                                pDummyDecoder = new CTextDecoder;
                                pDummyDecoder->Start(NULL, NULL, TRUE);
                            }
                            else
                                pDummyDecoder = NULL;
                            iState = STATE_BODY;
                            PopBoundary();
                        }
                        else
                        {
                            pOutput->StartBlock(BLOCK_HEADER, InputFile.iCurrentLine + 1);
                            pDummyDecoder = new CTextDecoder;
                            pDummyDecoder->Start(NULL, NULL, TRUE);
                            iState = STATE_HEADER;
                            iWeightSum = MIN_WEIGHT; // we definitely want this header..
                        }
                        iCurrentEncoding = ENCODING_NONE;
                        bNextBlockIsAttachment = FALSE;
                        strcpy(cLine, cNextLine);
                        bLast = bNextLast;
                        bNextLast = InputFile.ReadLine(cNextLine);
                    }
                    goback = TRUE;
                }
                else // Handling the case when the previous multipart
                {    // was not properly terminated, yet we are at a boundary
                    int i;
                    for (i = STACKTOP - 1; i >= 0; i--) // one of the previous ones.
                        if (IsBoundary(cBoundaries[i], &bEnd2))
                        {
                            iMultipart = i + 1; // switch to that previous one...
                            goback = TRUE;
                            break;
                        }
                }
            }
        } while (goback);

        do
        {
            goback = FALSE;
            switch (iState)
            {
            case STATE_UNRECOGNIZED:
            case STATE_BODY:
            {
                if (IsHeader())
                {
                    SaveState(); // save the state in case this is not a header
                    if (pOutput->iLevel > 0)
                    {
                        EndCalcSize(pOutput);
                        pOutput->EndBlock(InputFile.iCurrentLine);
                    }
                    iState = STATE_HEADER;
                    pOutput->StartBlock(BLOCK_HEADER, InputFile.iCurrentLine); // start the header
                    strcpy(cCurrentCharset, "us-ascii");                       // default charset
                    SetDefaultFileName(FALSE);
                    iCurrentEncoding = ENCODING_NONE;
                    bNextBlockIsAttachment = FALSE;
                    pDummyDecoder = new CTextDecoder;       // to compute the resulting file size
                    pDummyDecoder->Start(NULL, NULL, TRUE); // bJustCalcSize == TRUE
                    iWeightSum = 0;
                    goback = TRUE;
                    break;
                }
                if (iState != STATE_UNRECOGNIZED)
                {
                    if (!IsWhiteLine(cLine))
                        pOutput->pCurrentBlock->bEmpty = 0;
                    if (pDummyDecoder != NULL)
                        pDummyDecoder->DecodeLine(cLine, bLast);
                }
                break;
            }

            case STATE_HEADER: // we are in the header block
            {
                pDummyDecoder->DecodeLine(cLine, bLast); // computing the size...

                // unfolding of headers (see RFC822)
                while ((cNextLine[0] == ' ' || cNextLine[0] == '\t') && !IsWhiteLine(cNextLine))
                {
                    if (strlen(cLine) + strlen(cNextLine + 1) >= LINE_MAX)
                        break; // overflow test
                    pDummyDecoder->DecodeLine(cNextLine, bLast);
                    strcat(cLine, cNextLine + 1);
                    bLast = bNextLast;
                    bNextLast = InputFile.ReadLine(cNextLine);
                }

                // split the line into the header name and its value
                if (ParseHeader(cLine, cName, NAME_MAX, cText, TEXT_MAX))
                {
                    if (!_stricmp(cName, "Content-Type"))
                    {
                        GetParameter(cText, "charset", cCurrentCharset, CURRENTCHARSET_MAX);
                        char cType[20], cSubType[20];
                        ParseContentType(cText, cType, sizeof(cType), cSubType, sizeof(cSubType));
                        if (iMultipart < MULTIPARTSTACK_MAX && !_stricmp(cType, "multipart"))
                        { // if the message is multipart, we care about the string that separates the individual parts
                            char cBoundary[BOUNDARY_MAX];
                            if (GetParameter(cText, "boundary", cBoundary, BOUNDARY_MAX))
                            {
                                PushBoundary(cBoundary);
                                pMultipartStart = pOutput->pCurrentBlock; // remember where we are
                            } // we have started working with the multipart
                        }
                        else if (!iNameOrigin)
                            SetDefaultMIMEFileName(cType, cSubType, bAppendCharset);

                        if (iNameOrigin == 0)
                        {
                            if (GetParameter(cText, "name", cFileName, MAX_PATH))
                            {
                                iNameOrigin = 1;
                                bNextBlockIsAttachment = TRUE;
                            }
                        }
                    }
                    else if (!_stricmp(cName, "Content-Transfer-Encoding"))
                    {
                        char cEncoding[20];
                        const char* p = cText;
                        GetWord(p, cEncoding, sizeof(cEncoding), " \t();");
                        if (!_stricmp(cEncoding, "quoted-printable"))
                            iCurrentEncoding = ENCODING_QP;
                        else if (!_stricmp(cEncoding, "base64"))
                            iCurrentEncoding = ENCODING_BASE64;
                        else if (!_stricmp(cEncoding, "7bit") || !_stricmp(cEncoding, "8bit") ||
                                 !_stricmp(cEncoding, "binary") || !_stricmp(cEncoding, "none"))
                            iCurrentEncoding = ENCODING_NONE;
                        else
                            iCurrentEncoding = ENCODING_UNKNOWN;
                    }
                    else if (!_stricmp(cName, "Content-Disposition"))
                    {
                        char cDisp[20];
                        const char* p = cText;
                        GetWord(p, cDisp, sizeof(cDisp), " \t();");
                        if (!_stricmp(cDisp, "attachment"))
                            bNextBlockIsAttachment = TRUE;
                        if (GetParameter(cText, "filename", cFileName, MAX_PATH))
                        {
                            iNameOrigin = 2;
                            bNextBlockIsAttachment = TRUE;
                        }
                    }
                    else if (!_stricmp(cName, "Content-Location"))
                    {
                        char cDisp[MAX_PATH];
                        char* p = cText;
                        GetWord(p, cDisp, sizeof(cDisp), " \t();"); // Perhaps a better separators needed. Entire line needed in examined examples
                        if (!_strnicmp(cDisp, "file:", 5) || !_strnicmp(cDisp, "http:", 5))
                        {
                            char* p2 = strrchr(cDisp, '\\');
                            p = strrchr(cDisp, '/');
                            p = max(p, p2);
                            if (p && p[1])
                            { // Do not take empty fname from from e.g. http://www.altap.cz/
                                iNameOrigin = 3;
                                bNextBlockIsAttachment = TRUE;
                                strcpy(cFileName, p + 1);
                                // Strip params after .asp in http://toplist.cz/count.asp?id=1034442
                                p = strchr(cFileName, '?');
                                // And also after .jpg in http://www.anna-reality.cz/showthumb.php?img=fotky/brezova_936m2_1.jpg&width=120&height=120&quality=80
                                if (!p)
                                    p = strchr(cFileName, '&');
                                if (p)
                                    *p = 0;
                            }
                        }
                        else if (!*p)
                        {
                            // Hack: Handle .MHT files created by MSIE7 or Win7 problem report creation tool:
                            // Content-Location: screenshot_0001.jpeg
                            // But right now I cannot find any example with file: :-(((
                            // Should we look for \ and / as well?
                            iNameOrigin = 3;
                            bNextBlockIsAttachment = TRUE;
                            strcpy(cFileName, cDisp);
                        }
                    }

                    HEADERINFO* phi = GetHeaderInfo(cName);
                    if (phi != NULL)
                    {
                        iWeightSum += phi->weight;

                        if (phi->main) // should we promote this block to the main header?
                        {
                            pOutput->pCurrentBlock->iBlockType = BLOCK_MAINHEADER;
                            if (iWeightSum >= MIN_WEIGHT && pMultipartStart != pOutput->pCurrentBlock)
                                iMultipart = 0; // finish the multipart from the previous mail
                        }
                    }
                }

                // setting the bEmpty flag (a header is never empty)
                pOutput->pCurrentBlock->bEmpty = 0;

                // are we at the end of the header block (= empty line)?
                if (IsWhiteLine(cNextLine))
                {
                    pDummyDecoder->DecodeLine(cNextLine, bLast);
                    EndCalcSize(pOutput);

                    if (iWeightSum < MIN_WEIGHT)
                    {
                        EverythingBaaack(pOutput, 1); // roll everything back!!! this was not a header...
                    }
                    else
                    {
                        BOOL bLastStartedMPart = pOutput->pCurrentBlock == pMultipartStart;
                        pOutput->EndBlock(InputFile.iCurrentLine + 2); // close this block
                        iState = STATE_BODY;                           // the body will follow

                        if (iMultipart && bLastStartedMPart)
                        { // If the header that started the multipart has just ended, a MIME preamble begins.
                            pOutput->StartBlock(BLOCK_PREAMBLE, InputFile.iCurrentLine + 2);
                            pDummyDecoder = new CTextDecoder;
                            pDummyDecoder->Start(NULL, NULL, TRUE);
                        }
                        else
                        { // Otherwise it is an ordinary body/message part.
                            pOutput->StartBlock(BLOCK_BODY, InputFile.iCurrentLine + 2);
                            CStartMarker* p = pOutput->pCurrentBlock;
                            p->iEncoding = iCurrentEncoding;
                            p->bAttachment = bNextBlockIsAttachment;
                            strcpy(p->cCharset, cCurrentCharset);
                            strcpy(p->cFileName, cFileName);
                            if (iNameOrigin == 0)
                                (*piDefaultNumber)++;
                            else
                            {
                                iNameOrigin = 0;
                                DecodeSpecialWords(p->cFileName);
                                DestroyIllegalChars(p->cFileName);
                            }
                            CreateDecoder();
                            if (pDummyDecoder != NULL)
                                pDummyDecoder->Start(NULL, NULL, TRUE);
                        }

                        bLast = bNextLast;
                        bNextLast = InputFile.ReadLine(cNextLine);
                    }
                }
                break;
            }
            }
        } while (goback);

        strcpy(cLine, cNextLine);
    } while (!bLast);

skipout:
    if (pOutput->iLevel > 0)
    {
        EndCalcSize(pOutput);
        pOutput->EndBlock(InputFile.iCurrentLine + 1);
    }

    InputFile.Close();

    if (pOutput->Markers.Count > 0)
    {
        CStartMarker* ptr = (CStartMarker*)pOutput->Markers[0];
        // There is some ASCII rubbish before first MIME separator
        if (ptr->iMarkerType == MARKER_END)
        {
            ptr = new CStartMarker;
            ptr->iMarkerType = MARKER_START;
            ptr->iLine = 0;
            ptr->iBlockType = BLOCK_MAINHEADER;
            ptr->iEncoding = ENCODING_NONE;
            ptr->bEmpty = !G.bListMailHeaders;
            ptr->cFileName[0] = 0;
            ptr->bSelected = 0;
            ptr->bAttachment = 0;
            ptr->iSize = 0;
            ptr->iBadBlock = 0;
            ptr->iPart = 0;
            strcpy(ptr->cCharset, "us-ascii");
            pOutput->Markers.Insert(0, ptr);
        }
    }

    // add names to the parts that are not BODY
    int h = 0, s = 0, p = 0, e = 0;
    int i;
    for (i = 0; i < pOutput->Markers.Count; i++)
    {
        CStartMarker* ptr = (CStartMarker*)pOutput->Markers[i];
        if (ptr->iMarkerType == MARKER_START)
        {
            switch (ptr->iBlockType)
            {
            case BLOCK_MAINHEADER:
                sprintf(ptr->cFileName, "header%#03ld.txt", h++);
                break;
            case BLOCK_HEADER:
                sprintf(ptr->cFileName, "subheader%#03ld.txt", s++);
                break;
            case BLOCK_PREAMBLE:
                sprintf(ptr->cFileName, "preamble%#03ld.txt", p++);
                break;
            case BLOCK_EPILOG:
                sprintf(ptr->cFileName, "epilog%#03ld.txt", e++);
                break;
            }
        }
    }

    MakeNamesUnique(pOutput);

    pOutput->EndBlock(InputFile.iCurrentLine + 1); // one extra End because of the decoder

    delete[] cLine;
    delete[] cNextLine;
    delete[] cSavedLine;
    delete[] cSavedNextLine;
    delete[] cName;
    delete[] cText;
    delete[] cCurrentCharset;
    delete[] cFileName;
    delete[] cBoundaries;

    return TRUE;
}
