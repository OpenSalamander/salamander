// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/****************************************************************************************\
**                                                                                      **
**                                                                                      **
**   decoder.cpp - MIME dekoder                                                         **
**                                                                                      **
**   v.1.0                                                                              **
**                                                                                      **
**   autor: Jakub Cerveny                                                               **
**                                                                                      **
**                                                                                      **
\****************************************************************************************/

#include "precomp.h"

#include "parser.h"
#include "decoder.h"
#include "unmime.h"
#include "unmime.rh"
#include "unmime.rh2"
#include "lang\lang.rh"

#define XX 127
#define EE 126
#define BUFSIZE (256 * 1024)     // velikost bufferu pro zapis
#define PROGRESS_MASK 0xffff8000 // pozice progress-meteru se updatuje kazdych 32KB

static CSalamanderForOperationsAbstract* Salamander;
static CQuadWord currentProgress;
static BOOL bAbort;

static int iBackup[4];

// *****************************************************************************
//
//  Build??Table
//

static void BuildQPTable(BYTE* pTable)
{
    CALL_STACK_MESSAGE1("BuildQPTable()");
    memset(pTable, XX, 256);
    int i;
    for (i = 0; i < 10; i++)
        pTable[i + '0'] = i;
    int j;
    for (j = 0; j < 6; j++)
        pTable[j + 'A'] = pTable[j + 'a'] = j + 10;
}

static void BuildBase64Table(BYTE* pTable)
{
    CALL_STACK_MESSAGE1("BuildBase64Table()");
    BYTE b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    memset(pTable, XX, 256);
    int i;
    for (i = 0; i < 64; i++)
        pTable[b64chars[i]] = i;
    pTable['='] = EE;
}

void BuildUUTable(BYTE* pTable)
{
    CALL_STACK_MESSAGE1("BuildUUTable()");
    memset(pTable, 255, 256);
    int i;
    for (i = 0; i < 64; i++)
        pTable[i + 32] = i;
    pTable[96] = 0; // ` bude to samy jako mezera
}

void BuildXXTable(BYTE* pTable)
{
    CALL_STACK_MESSAGE1("BuildXXTable()");
    BYTE xxchars[] = "+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    memset(pTable, 255, 256);
    int i;
    for (i = 0; i < 64; i++)
        pTable[xxchars[i]] = i;
}

static void BuildBinHexTable(BYTE* pTable)
{
    CALL_STACK_MESSAGE1("BuildBinHexTable()");
    BYTE binhexchars[] = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";
    memset(pTable, XX, 256);
    int i;
    for (i = 0; i < 64; i++)
        pTable[binhexchars[i]] = i;
    pTable[':'] = EE;
}

// *****************************************************************************
//
//  CDecoder - predek pro ostatni dekodery
//

BOOL CDecoder::Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize)
{
    CALL_STACK_MESSAGE2("CDecoder::Start(, %d)", bJustCalcSize);
    bCalcSize = bJustCalcSize;
    iDecodedSize = 0;
    if (!bCalcSize)
    {
        HFile = hFile;
        if (fileName != NULL)
            strcpy(FileName, fileName);
        PBuffer = new char[BUFSIZE];
        if (PBuffer == NULL)
        {
            iErrorStr = IDS_LOWMEM;
            return FALSE;
        }
        iBufPos = 0;
    }
    return TRUE;
}

BOOL CDecoder::BufferedWrite(const void* pData, int nBytes)
{
    // CALLSTACK nemam protoze zpomaloval...
    iDecodedSize += nBytes;
    if (!bCalcSize)
    {
        if (Salamander != NULL)
            if ((iDecodedSize & PROGRESS_MASK) != ((iDecodedSize - nBytes) & PROGRESS_MASK))
                if (!Salamander->ProgressSetSize(CQuadWord(iDecodedSize, 0), currentProgress + CQuadWord(iDecodedSize, 0), TRUE))
                {
                    bAbort = TRUE;
                    return FALSE;
                }

        if (iBufPos + nBytes <= BUFSIZE)
        {
            memcpy(PBuffer + iBufPos, pData, nBytes);
            iBufPos += nBytes;
            return TRUE;
        }
        int n = BUFSIZE - iBufPos;
        if (n > 0)
            memcpy(PBuffer + iBufPos, pData, n);
        DWORD numw;
        if (!SafeWriteFile(HFile, PBuffer, BUFSIZE, &numw, FileName))
        {
            iErrorStr = -1;
            iBufPos = 0; // aby CDecoder::End() nehlasil taky chybu...
            return FALSE;
        }
        memcpy(PBuffer, (char*)pData + n, iBufPos = (nBytes - n));
    }
    return TRUE;
}

BOOL CDecoder::End()
{
    CALL_STACK_MESSAGE1("CDecoder::End()");
    BOOL ret = TRUE;
    if (!bCalcSize)
    {
        if (iBufPos)
        {
            DWORD numw;
            if (!SafeWriteFile(HFile, PBuffer, iBufPos, &numw, FileName))
            {
                iErrorStr = -1;
                ret = FALSE;
            }
        }
        delete[] PBuffer;
    }
    return ret;
}

void CDecoder::SaveState()
{
    iBackup[0] = iDecodedSize;
    iBackup[3] = bCalcSize;
}

void CDecoder::RestoreState()
{
    iDecodedSize = iBackup[0];
    bCalcSize = iBackup[3];
}

// *****************************************************************************
//
//  CTextDecoder - dekoder pro Plain Text nebo None
//

BOOL CTextDecoder::DecodeLine(LPTSTR pszLine, BOOL bLastLine)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CTextDecoder::DecodeLine()");
    if (!BufferedWrite(pszLine, (int)strlen(pszLine)))
        return FALSE;
    if (!bLastLine)
        return BufferedWrite("\r\n", 2);
    return TRUE;
}

// *****************************************************************************
//
//  CQPDecoder - Quoted Printable Decoder
//

BOOL CQPDecoder::Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize)
{
    CALL_STACK_MESSAGE2("CQPDecoder::Start(, %d)", bJustCalcSize);
    BuildQPTable(table);
    return CDecoder::Start(hFile, fileName, bJustCalcSize);
}

BOOL CQPDecoder::DecodeLine(LPTSTR pszLine, BOOL bLastLine)
{
    // CALLSTACK nemam protoze zpomaloval...
    // urizneme whitespace z konce radky, tak jak to rika RFC (MPACK to nedela...)
    int i = (int)strlen(pszLine) - 1;
    while (i >= 0 && (pszLine[i] == ' ' || pszLine[i] == '\t'))
        pszLine[i--] = 0;
    // sekvence '=XX' nahradime znaky s hexa hodnotou XX
    while (*pszLine)
    {
        if (*pszLine == '=')
        {
            char c;
            if (!pszLine[1] || !pszLine[2])
                return TRUE; // soft line break (viz RFC)
            int c1 = table[(unsigned char)pszLine[1]];
            int c2 = table[(unsigned char)pszLine[2]];
            c = (c1 << 4 | c2) & 0xff;
            if (!BufferedWrite(&c, 1))
                return FALSE;
            pszLine += 3;
        }
        else
        {
            if (!BufferedWrite(pszLine++, 1))
                return FALSE;
        }
    }
    if (!bLastLine)
        return BufferedWrite("\r\n", 2);
    return TRUE;
}

// *****************************************************************************
//
//  CBase64Decoder
//

BOOL CBase64Decoder::Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize)
{
    CALL_STACK_MESSAGE2("CBase64Decoder::Start(, %d)", bJustCalcSize);
    n = 0;
    bDataDone = FALSE;
    BuildBase64Table(table);
    return CDecoder::Start(hFile, fileName, bJustCalcSize);
}

BOOL CBase64Decoder::DecodeChar(char newchar)
{
    // CALLSTACK nemam protoze zpomaloval...
    if (bDataDone)
        return TRUE; // pokud jsme skoncili, ale nasleduji dalsi znaky, ignorujeme je
    char b64 = table[(unsigned char)newchar];
    if (b64 == XX)
        return TRUE; // ilegalni znaky taky ignorujeme
    c[n++] = b64;
    if (n == 4) // mame naakumulovany 4 znaky?
    {
        n = 0;
        if (c[0] == EE || c[1] == EE)
        {
            bDataDone = TRUE;
            return TRUE;
        }
        char d = (c[0] << 2) | (c[1] >> 4);
        if (!BufferedWrite(&d, 1))
            return FALSE;
        if (c[2] == EE)
        {
            bDataDone = TRUE;
            return TRUE;
        }
        d = ((c[1] & 0xf) << 4) | (c[2] >> 2);
        if (!BufferedWrite(&d, 1))
            return FALSE;
        if (c[3] == EE)
        {
            bDataDone = TRUE;
            return TRUE;
        }
        d = ((c[2] & 0x3) << 6) | c[3];
        if (!BufferedWrite(&d, 1))
            return FALSE;
    }
    return TRUE;
}

BOOL CBase64Decoder::DecodeLine(LPTSTR pszLine, BOOL)
{
    // CALLSTACK nemam protoze zpomaloval...
    while (*pszLine)
        if (!DecodeChar(*pszLine++))
            return FALSE;
    return TRUE;
}

void CBase64Decoder::SaveState()
{
    CDecoder::SaveState();
    iBackup[1] = n;
    iBackup[2] = bDataDone;
}

void CBase64Decoder::RestoreState()
{
    CDecoder::RestoreState();
    n = iBackup[1];
    bDataDone = iBackup[2];
}

// *****************************************************************************
//
//  CUUXXDecoder
//

BOOL CUUXXDecoder::Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize)
{
    CALL_STACK_MESSAGE2("CUUXXDecoder::Start(, %d)", bJustCalcSize);
    if (bXX)
        BuildXXTable(table);
    else
        BuildUUTable(table);
    return CDecoder::Start(hFile, fileName, bJustCalcSize);
}

BOOL CUUXXDecoder::DecodeLine(LPTSTR pszLine, BOOL)
{
    const char* line = pszLine;
    char text[8];
    SkipWSP(line);
    GetWord(line, text, 8, " \t");
    if (!lstrcmpi(text, "begin") || !lstrcmpi(text, "end"))
        return TRUE;

    int c, len;
    len = table[*pszLine++];
    while (len)
    {
        c = table[pszLine[0]] << 2 | table[pszLine[1]] >> 4;
        if (!BufferedWrite(&c, 1))
            return FALSE;
        if (--len)
        {
            c = table[pszLine[1]] << 4 | table[pszLine[2]] >> 2;
            if (!BufferedWrite(&c, 1))
                return FALSE;
            if (--len)
            {
                c = table[pszLine[2]] << 6 | table[pszLine[3]];
                if (!BufferedWrite(&c, 1))
                    return FALSE;
                len--;
            }
        }
        pszLine += 4;
    }
    return TRUE;
}

// *****************************************************************************
//
//  CBinHexDecoder
//

BOOL CBinHexDecoder::Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize)
{
    CALL_STACK_MESSAGE2("CBinHexDecoder::Start(, %d)", bJustCalcSize);
    eCharState = CS_START;
    eRLEState = RS_NORMALBYTE;
    eBinaryState = BS_START;
    garbage_counter = 0;
    calculated_crc = 0;
    n = 0;
    bFinished = FALSE;
    BuildBinHexTable(table);
    return CDecoder::Start(hFile, fileName, bJustCalcSize);
}

BOOL CBinHexDecoder::End()
{
    bCRCFailed = data_crc != calculated_crc;
    return CDecoder::End();
}

BOOL CBinHexDecoder::DecodeLine(LPTSTR pszLine, BOOL)
{
    while (*pszLine)
        if (!DecodeChar(*pszLine++))
            return FALSE;
    return TRUE;
}

BOOL CBinHexDecoder::DecodeChar(char c)
{
    switch (eCharState)
    {
    case CS_START:
        if (c == ':')
            eCharState = CS_DATA;
        else if (++garbage_counter > 50)
            return FALSE;
        return TRUE;

    case CS_DATA:
    {
        if (c == ' ' || c == '\t')
            return TRUE;
        BYTE b = table[(BYTE)c];
        if (b == XX)
            return FALSE;
        acc[n++] = b;
        if (n == 4 || b == EE)
        {
            if (acc[0] == EE || acc[1] == EE)
                break;
            BYTE d = (acc[0] << 2) | (acc[1] >> 4);
            DecodeRLE(d);
            if (acc[2] == EE)
                break;
            d = ((acc[1] & 0xf) << 4) | (acc[2] >> 2);
            DecodeRLE(d);
            if (acc[3] == EE)
                break;
            d = ((acc[2] & 0x3) << 6) | acc[3];
            DecodeRLE(d);
            n = 0;
        }
        return TRUE;
    }

    default:
        return TRUE;
    }
    // sem se skace pri dosazeni konce z tech breaku nahore, trochu prasarna sorry...
    bFinished = TRUE;
    eCharState = CS_END;
    return TRUE;
}

void CBinHexDecoder::DecodeRLE(BYTE b)
{
    switch (eRLEState)
    {
    case RS_NORMALBYTE:
        if (b != 0x90)
            DecodeBinary(rle_last = b);
        else
            eRLEState = RS_COUNTBYTE;
        break;

    case RS_COUNTBYTE:
        if (b == 0)
            DecodeBinary(0x90);
        else
        {
            int i;
            for (i = 0; i < (int)b - 1; i++)
                DecodeBinary(rle_last);
        }
        eRLEState = RS_NORMALBYTE;
        break;
    }
}

#pragma runtime_checks("c", off)
void CBinHexDecoder::DecodeBinary(BYTE b)
{
    // vypocet CRC, prevzato z MPacku
    if (eBinaryState == BS_DATA || eBinaryState == BS_DATACRC)
    {
        WORD tmpcrc, cval;
        BYTE ctmp;
        ctmp = eBinaryState == BS_DATACRC ? 0 : b;
        cval = calculated_crc & 0xf000;
        tmpcrc = ((unsigned short)(calculated_crc << 4) | (ctmp >> 4)) ^ (cval | (cval >> 7) | (cval >> 12));
        cval = tmpcrc & 0xf000;
        calculated_crc = ((unsigned short)(tmpcrc << 4) | (ctmp & 0x0f)) ^ (cval | (cval >> 7) | (cval >> 12));
    }

    switch (eBinaryState)
    {
    case BS_START:
        if (b > 63)
            b = 63;
        name_length = b;
        counter = 0;
        eBinaryState = BS_FILENAME;
        break;

    case BS_FILENAME:
        cFileName[counter++] = b;
        if (counter == name_length)
        {
            cFileName[counter] = 0;
            eBinaryState = BS_CRAP1;
            counter = 0;
        }
        break;

    case BS_CRAP1: // this is in fact Version (BYTE), Type (DWORD), Creator (DWORD), Flags (WORD)
        if (++counter == 11)
        {
            eBinaryState = BS_DATALENGTH;
            counter = 4;
        }
        break;

    case BS_DATALENGTH:
        ((BYTE*)&iDataLength)[--counter] = b;
        if (counter == 0)
        {
            eBinaryState = BS_RESOURCELENGTH;
            counter = 4;
        }
        break;

    case BS_RESOURCELENGTH:
        ((BYTE*)&iResourceLength)[--counter] = b;
        if (counter == 0)
        {
            if (!bCalcSize && iResourceLength)
            {
                // We are gonna loose the resource fork
                if (!(G.nDontShowAnymore & DSA_RESOURCE_FORK_LOST))
                {
                    BOOL checked = FALSE;

                    ShowOneTimeMessage(SalamanderGeneral->GetMsgBoxParent(), IDS_RESOURCE_FORK_LOST, &checked);
                    if (checked)
                    {
                        G.nDontShowAnymore |= DSA_RESOURCE_FORK_LOST;
                    }
                }
            }
            eBinaryState = BS_HEADERCRC;
            counter = 0;
        }
        break;

    case BS_HEADERCRC:
        if (++counter == 2)
        {
            eBinaryState = BS_DATA;
            counter = 0;
        }
        break;

    case BS_DATA:
        BufferedWrite(&b, 1);
        if (++counter == iDataLength)
        {
            eBinaryState = BS_DATACRC;
            counter = 2;
        }
        break;

    case BS_DATACRC:
        ((BYTE*)&data_crc)[--counter] = b;
        if (counter == 0)
            eBinaryState = BS_END;
        break;

    case BS_END:
        break;
    }
}
#pragma runtime_checks("", restore)

// *****************************************************************************
//
//  CYEncDecoder
//

BOOL CYEncDecoder::Start(HANDLE hFile, char* fileName, BOOL bJustCalcSize)
{
    CALL_STACK_MESSAGE2("CUUXXDecoder::Start(, %d)", bJustCalcSize);
    bError = FALSE;
    CRC = 0;
    //MakeCrcTable(crctable);
    return CDecoder::Start(hFile, fileName, bJustCalcSize);
}

BOOL CYEncDecoder::DecodeLine(LPTSTR pszLine, BOOL)
{
    if (!memcmp(pszLine, "=ybegin", 7) || !memcmp(pszLine, "=yend", 5) || !memcmp(pszLine, "=ypart", 6))
        return TRUE;

    while (*pszLine)
    {
        BYTE b = *pszLine;
        if (b == '=')
        {
            pszLine++;
            if (!*pszLine)
                return bError = TRUE;
            b = *pszLine;
            b -= 42 + 64;
        }
        else
        {
            b -= 42;
        }
        if (!BufferedWrite(&b, 1))
            return FALSE;
        CRC = SalamanderGeneral->UpdateCrc32(&b, 1, CRC);
        pszLine++;
    }
    return TRUE;
}

// *****************************************************************************
//
//  DecodeSelectedBlocks
//

static CInputFile InputFile;
static char* pszLine;
static CParserOutput* pOutput;
static int iNextMarker;
static LPCTSTR pszDir;
static FILETIME* pFileTime;
static LPCTSTR pszArcName;
static DWORD iSilent;
static BOOL bFileExtracted;
static char* pszText2;
static char* pszBuf;
static BOOL bLastLine;

static void GetInfo(char* buffer, FILETIME* lastWrite, unsigned size)
{
    CALL_STACK_MESSAGE2("GetInfo(, , 0x%X)", size);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, CQuadWord(size, 0)), date, time);
}

static BOOL Decode(CDecoder* pDec, BOOL bOnlyOneFile)
{
    CALL_STACK_MESSAGE1("Decode()");
    while (1)
    {
        CMarker* pNextMarker = pOutput->Markers[iNextMarker];
        if (InputFile.iCurrentLine >= pNextMarker->iLine)
        {
            iNextMarker++;
            char text[MAX_PATH + 32]; // musi byt delsi nez MAX_PATH kvuli "too long name" (niz nize)
            if (pNextMarker->iMarkerType == MARKER_START)
            {
                CStartMarker* pStartMarker = (CStartMarker*)pNextMarker;
                CDecoder* pDecoder;
                HANDLE hFile = INVALID_HANDLE_VALUE;
                BOOL bNull = FALSE;

                if (pStartMarker->bEmpty || !pStartMarker->bSelected)
                {
                    pDecoder = new CNullDecoder;
                    bNull = TRUE;
                }
                else
                {
                    strncpy_s(text, MAX_PATH, pszDir, _TRUNCATE);
                    if (!SalamanderGeneral->SalPathAppend(text, pStartMarker->cFileName, MAX_PATH))
                    { // too long name - ohlasi se v SalamanderSafeFile->SafeFileCreate
                        char* end = text + strlen(text);
                        if (end > text && *(end - 1) != '\\')
                            *end++ = '\\';
                        strncpy_s(end, _countof(text) - (end - text), pStartMarker->cFileName, _TRUNCATE);
                    }
                    strcpy_s(pszText2, MAX_PATH, pszArcName);
                    SalamanderGeneral->SalPathAppend(pszText2, pStartMarker->cFileName, MAX_PATH);
                    GetInfo(pszBuf, pFileTime, pStartMarker->iSize);
                    BOOL bSkip;
                    hFile = SalamanderSafeFile->SafeFileCreate(text, GENERIC_WRITE, FILE_SHARE_READ,
                                                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                                               FALSE, SalamanderGeneral->GetMsgBoxParent(),
                                                               pszText2, pszBuf, &iSilent, TRUE, &bSkip, NULL, 0, NULL, NULL);
                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                        pDecoder = new CNullDecoder;
                        bNull = TRUE;
                    }
                    else
                    {
                        switch (pStartMarker->iEncoding)
                        {
                        case ENCODING_QP:
                            pDecoder = new CQPDecoder;
                            break;

                        case ENCODING_BASE64:
                            pDecoder = new CBase64Decoder;
                            break;

                        case ENCODING_UU:
                        case ENCODING_XX:
                            pDecoder = new CUUXXDecoder;
                            ((CUUXXDecoder*)pDecoder)->bXX = pStartMarker->iEncoding == ENCODING_XX;
                            break;

                        case ENCODING_BINHEX:
                            pDecoder = new CBinHexDecoder;
                            break;

                        case ENCODING_YENC:
                            pDecoder = new CYEncDecoder;
                            break;

                        default:
                            pDecoder = new CTextDecoder;
                        }
                    }
                }

                if (!bNull && Salamander != NULL)
                {
                    strcpy_s(pszText2, MAX_PATH, LoadStr(IDS_UNPACKING));
                    strcat_s(pszText2, MAX_PATH, pStartMarker->cFileName);
                    Salamander->ProgressDialogAddText(pszText2, TRUE);
                    Salamander->ProgressSetTotalSize(CQuadWord(pStartMarker->iSize, 0), CQuadWord(-1, -1));
                    if (!Salamander->ProgressSetSize(CQuadWord(0, 0), currentProgress, TRUE))
                        bAbort = TRUE;
                }

                BOOL err = !pDecoder->Start(hFile, text) || !Decode(pDecoder, bOnlyOneFile);
                err = !pDecoder->End() || err;

                if (!bNull && Salamander != NULL)
                {
                    currentProgress += CQuadWord(pDecoder->iDecodedSize, 0);
                    if (!Salamander->ProgressSetSize(CQuadWord(0, 0), currentProgress, TRUE))
                        bAbort = TRUE;
                }

                delete pDecoder;

                if (hFile != INVALID_HANDLE_VALUE)
                    if (!bAbort)
                    {
                        SetFileTime(hFile, pFileTime, pFileTime, pFileTime);
                        CloseHandle(hFile);
                        bFileExtracted = TRUE;
                    }
                    else
                    {
                        CloseHandle(hFile);
                        DeleteFile(text);
                    }

                if (err || bAbort)
                    return FALSE;
                if (bOnlyOneFile && bFileExtracted)
                    return TRUE;
            }
            else
            {
                return TRUE;
            }
        }
        else
        {
            if (!pDec->DecodeLine(pszLine, bLastLine))
                return FALSE;
            bLastLine = InputFile.ReadLine(pszLine);
        }
    }
}

BOOL DecodeSelectedBlocks(LPCTSTR pszFileName, CParserOutput* output, LPCTSTR dir, FILETIME* pft,
                          CSalamanderForOperationsAbstract* sal, const CQuadWord& totalSize,
                          BOOL* pAborted, BOOL bOnlyOneFile)
{
    CALL_STACK_MESSAGE3("DecodeSelectedBlocks(%s, , %s)", pszFileName, dir);
    if (output->Markers.Count == 0)
        return TRUE;
    if (!InputFile.Open(pszFileName))
        return FALSE;

    pszLine = new char[1000];
    pOutput = output;
    pszDir = dir;
    iNextMarker = 0;
    pszArcName = pszFileName;
    pFileTime = pft;
    iSilent = 0;
    bFileExtracted = FALSE;
    Salamander = sal;
    currentProgress = CQuadWord(0, 0);
    bAbort = FALSE;
    pszText2 = new char[MAX_PATH];
    pszBuf = new char[100];
    bLastLine = FALSE;

    if (Salamander != NULL)
        Salamander->ProgressSetTotalSize(CQuadWord(0, 0), totalSize);
    InputFile.ReadLine(pszLine);

    CNullDecoder* pDecoder = new CNullDecoder;
    BOOL ret = Decode(pDecoder, bOnlyOneFile);

    delete pDecoder;
    delete[] pszBuf;
    delete[] pszText2;
    delete[] pszLine;
    InputFile.Close();

    if (pAborted != NULL)
        *pAborted = bAbort;
    return ret;
}
