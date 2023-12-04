// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* UNARJ.C, UNARJ, R JUNG, 07/29/96
 * Main Extractor routine
 * Copyright (c) 1991-97 by ARJ Software, Inc.  All rights reserved.
 *
 *   This code may be freely used in programs that are NOT ARJ archivers
 *   (both compress and extract ARJ archives).
 *
 *   If you wish to distribute a modified version of this program, you
 *   MUST indicate that it is a modified version both in the program and
 *   source code.
 *
 *   We are holding the copyright on the source code, so please do not
 *   delete our name from the program files or from the documentation.
 *
 *   We wish to give credit to Haruhiko Okumura for providing the
 *   basic ideas for ARJ and UNARJ in his program AR.  Please note
 *   that UNARJ is significantly different from AR from an archive
 *   structural point of view.
 *
 * Modification history:
 * Date      Programmer  Description of modification.
 * 04/05/91  R. Jung     Rewrote code.
 * 04/23/91  M. Adler    Portabilized.
 * 04/29/91  R. Jung     Added l command.  Removed 16 bit dependency in
 *                       fillbuf().
 * 05/19/91  R. Jung     Fixed extended header skipping code.
 * 05/25/91  R. Jung     Improved find_header().
 * 06/03/91  R. Jung     Changed arguments in get_mode_str() and
 *                       set_ftime_mode().
 * 06/19/81  R. Jung     Added two more %c in printf() in list_arc().
 * 07/07/91  R. Jung     Added default_case_path() to extract().
 *                       Added strlower().
 * 07/20/91  R. Jung     Changed uint ratio() to static uint ratio().
 * 07/21/91  R. Jung     Added #ifdef VMS.
 * 08/28/91  R. Jung     Changed M_DIFFHOST message.
 * 08/31/91  R. Jung     Added changes to support MAC THINK_C compiler
 *                       per Eric Larson.
 * 10/07/91  R. Jung     Added missing ; to THINK_C additions.
 * 11/11/91  R. Jung     Added host_os test to fwrite_txt_crc().
 * 11/24/91  R. Jung     Added more error_count processing.
 * 12/03/91  R. Jung     Added backup file processing.
 * 02/17/93  R. Jung     Added archive modified date support.
 * 01/22/94  R. Jung     Changed copyright message.
 * 07/29/96  R. Jung     Added "/" to list of path separators.
 *
 */

#include "precomp.h"

#include "array2.h"

#include "unarjdll.h"
#include "unarj.h"
#include "unarjspl.h"

extern LPTSTR PathFindExtension(LPTSTR pszPath);

//***********************************************************************************
//
// globalky
//

HANDLE ArcFile = INVALID_HANDLE_VALUE;
char ArcName[MAX_PATH];
DWORD ArcFileSize;
DWORD ArcFilePos;

FARJChangeVolProc ChangeVolProc;
FARJProcessDataProc ProcessDataProc;
FARJErrorProc ErrorProc;

unsigned char InputBuffer[INBUFSIZ];
unsigned char* InPtr;
unsigned char* InEnd;

DWORD NextFileOffset;

BOOL SuccedingVolume;

CDynamicString* AchiveVolumes = NULL;

const SYSTEMTIME MinTime = {1980, 01, 2, 01, 00, 00, 00, 000};

//***********************************************************************************

/* Global variables */
UCRC crc;
ushort bitbuf;
int bitcount;
long compsize;
long origsize;
uchar subbitbuf;
uchar header[HEADERSIZE_MAX];
int file_type;
DWORD ext_size;

/*
char *M_USAGE  [] =
{
"Usage:  UNARJ archive[.arj]    (list archive)\n",
"        UNARJ e archive        (extract archive)\n",
"        UNARJ l archive        (list archive)\n",
"        UNARJ t archive        (test archive)\n",
"        UNARJ x archive        (extract with pathnames)\n",
"\n",
"This is an ARJ demonstration program and ** IS NOT OPTIMIZED ** for speed.\n",
"You may freely use, copy and distribute this program, provided that no fee\n",
"is charged for such use, copying or distribution, and it is distributed\n",
"ONLY in its original unmodified state.  UNARJ is provided as is without\n",
"warranty of any kind, express or implied, including but not limited to\n",
"the implied warranties of merchantability and fitness for a particular\n",
"purpose.  Refer to UNARJ.DOC for more warranty information.\n",
"\n",
"ARJ Software, Inc.      Internet address:  robjung@world.std.com\n",
"P.O. Box 249                    Web site:  www.arjsoft.com\n",
"Norwood MA 02062\n",
"USA\n",
NULL
};

char M_VERSION [] = "UNARJ (Demo version) 2.43 Copyright (c) 1991-97 ARJ Software, Inc.\n\n";

char M_ARCDATE [] = "Archive created: %s";
char M_ARCDATEM[] = ", modified: %s";
char M_BADCOMND[] = "Bad UNARJ command: %s";
char M_BADCOMNT[] = "Invalid comment header";
char M_BADHEADR[] = "Bad header";
char M_BADTABLE[] = "Bad Huffman code";
char M_CANTOPEN[] = "Can't open %s";
char M_CANTREAD[] = "Can't read file or unexpected end of file";
char M_CANTWRIT[] = "Can't write file. Disk full?";
char M_CRCERROR[] = "CRC error!\n";
char M_CRCOK   [] = "CRC OK\n";
char M_DIFFHOST[] = "  Binary file!";
char M_ENCRYPT [] = "File is password encrypted, ";
char M_ERRORCNT[] = "%sFound %5d error(s)!";
char M_EXTRACT [] = "Extracting %-25s";
char M_FEXISTS [] = "%-25s exists, ";
char M_HEADRCRC[] = "Header CRC error!";
char M_NBRFILES[] = "%5d file(s)\n";
char M_NOMEMORY[] = "Out of memory";
char M_NOTARJ  [] = "%s is not an ARJ archive";
char M_PROCARC [] = "Processing archive: %s\n";
char M_SKIPPED [] = "Skipped %s\n";
char M_SUFFIX  [] = ARJ_SUFFIX;
char M_TESTING [] = "Testing    %-25s";
char M_UNKNMETH[] = "Unsupported method: %d, ";
char M_UNKNTYPE[] = "Unsupported file type: %d, ";
char M_UNKNVERS[] = "Unsupported version: %d, ";
*/

#define setup_get(PTR) (get_ptr = (PTR))
#define get_byte() ((uchar)(*get_ptr++))
WORD get_word();
DWORD get_dword();

#define ASCII_MASK 0x7F

#define CRCPOLY 0xEDB88320L

#define UPDATE_CRC(r, c) r = crctable[((uchar)(r) ^ (uchar)(c)) & 0xff] ^ (r >> CHAR_BIT)

/* Local functions */

void MakeCrcTable();
void CrcBuf(char* str, int len);
void FixParity(uchar* buffer, DWORD size);
WORD GetWord();
DWORD GetDWord();
void ReadCrc(void* buffer, int size);
void DecodePath(char* name);
long FindHeader();
int ReadHeader(BOOL first, CARJHeaderData* headerData);
void UnStore();
void Extract();

void Seek(DWORD distance, DWORD method);
DWORD GetPos();
void Read(void* buffer, DWORD size);

/* Local variables */

uchar arj_nbr;
uchar arj_x_nbr;
uchar host_os;
uchar arj_flags;
uchar method;
uchar* get_ptr;
UCRC file_crc;
UCRC header_crc;

UCRC crctable[UCHAR_MAX + 1];

/* Functions */

WORD get_word()
{
    CALL_STACK_MESSAGE_NONE
    WORD ret = *(WORD*)get_ptr;
    get_ptr += 2;
    return ret;
}

DWORD
get_dword()
{
    CALL_STACK_MESSAGE_NONE
    DWORD ret = *(DWORD*)get_ptr;
    get_ptr += 4;
    return ret;
}

void MakeCrcTable()
{
    CALL_STACK_MESSAGE_NONE
    static BOOL HaveCrcTab = FALSE;
    if (!HaveCrcTab)
    {
        uint i, j;
        UCRC r;

        for (i = 0; i <= UCHAR_MAX; i++)
        {
            r = i;
            for (j = CHAR_BIT; j > 0; j--)
            {
                if (r & 1)
                    r = (r >> 1) ^ CRCPOLY;
                else
                    r >>= 1;
            }
            crctable[i] = r;
        }
        HaveCrcTab = TRUE;
    }
}

#pragma runtime_checks("c", off)
void CrcBuf(char* str, int len)
{
    CALL_STACK_MESSAGE_NONE
    while (len--)
        UPDATE_CRC(crc, *str++);
}
#pragma runtime_checks("", restore)

void FixParity(uchar* buffer, DWORD size)
{
    CALL_STACK_MESSAGE_NONE
    uchar* ptr = buffer;
    while (ptr < buffer + size)
    {
        FIX_PARITY(*ptr);
        ptr++;
    }
}

WORD GetWord()
{
    CALL_STACK_MESSAGE_NONE
    WORD b0, b1;
    READ_BYTE(b0)
    READ_BYTE(b1)
    return (b1 << 8) + b0;
}

DWORD
GetDWord()
{
    CALL_STACK_MESSAGE_NONE
    DWORD b0, b1, b2, b3;

    READ_BYTE(b0)
    READ_BYTE(b1)
    READ_BYTE(b2)
    READ_BYTE(b3)
    return (b3 << 24) + (b2 << 16) + (b1 << 8) + b0;
}

void ReadCrc(void* buffer, int size)
{
    CALL_STACK_MESSAGE2("ReadCrc(, %d)", size);
    Read(buffer, size);
    origsize += size;
    CrcBuf((char*)buffer, size);
}

void WriteTxtCrc(uchar* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("WriteTxtCrc(, 0x%X)", size);
    CrcBuf((char*)buffer, size);

    if (file_type == TEXT_TYPE)
    {
        static uchar textbuf[16 * 1024 + 2];
        uchar* dest = textbuf;
        while (size)
        {
            if (*buffer == '\n')
                *dest++ = '\r';
            *dest++ = *buffer++;

            size--;

            if (dest - textbuf >= 16 * 1024 || size == 0)
            {
                if (host_os != OS)
                    FixParity(textbuf, (DWORD)(dest - textbuf));
                if (!ProcessDataProc(textbuf, (DWORD)(dest - textbuf)))
                {
                    Seek(NextFileOffset, FILE_BEGIN);
                    throw 0;
                }
                dest = textbuf;
            }
        }
    }
    else
    {
        if (!ProcessDataProc(buffer, size))
        {
            Seek(NextFileOffset, FILE_BEGIN);
            throw 0;
        }
    }
}

void DecodePath(char* name)
{
    CALL_STACK_MESSAGE_NONE
    for (; *name; name++)
    {
        if (*name == ARJ_PATH_CHAR)
            *name = PATH_CHAR;
    }
}

long FindHeader()
{
    CALL_STACK_MESSAGE1("FindHeader()");
    DWORD arcpos = 0;
    DWORD lastpos = ArcFileSize < MAXSFX ? ArcFileSize : MAXSFX;
    ;
    unsigned char c;

    for (; arcpos < lastpos; arcpos++)
    {
        Seek(arcpos, FILE_BEGIN);
        READ_BYTE(c)
        while (arcpos < lastpos)
        {
            if (c != HEADER_ID_LO) /* low order first */
                READ_BYTE(c)
            else
            {
                READ_BYTE(c)
                if (c == HEADER_ID_HI)
                    break;
            }
            arcpos++;
        }
        if (arcpos >= lastpos)
            break;
        ushort headersize = GetWord();
        if (headersize <= HEADERSIZE_MAX)
        {
            crc = CRC_MASK;
            ReadCrc(header, headersize);
            if ((crc ^ CRC_MASK) == GetDWord())
            {
                Seek(arcpos, FILE_BEGIN);
                return arcpos;
            }
        }
    }
    Error(AE_BADARC);
    return -1;
}

void EnsureBackslashes(char* path)
{
    CALL_STACK_MESSAGE1("EnsureBackslashes()");
    while (*path)
    {
        if (*path == '/')
            *path = '\\';
        path++;
    }
}

int ReadHeader(BOOL first, CARJHeaderData* headerData)
{
    CALL_STACK_MESSAGE2("ReadHeader(%d, )", first);
    ushort header_id = GetWord();
    if (header_id != HEADER_ID)
    {
        if (first)
            Error(AE_BADARC);
        else
            Error(AE_BADDATA);
    }

    ushort headersize = GetWord();
    if (headersize == 0)
    {
        if (first)
            Error(AE_BADARC);
        return 0; /* end of archive */
    }
    if (headersize > HEADERSIZE_MAX)
        Error(AE_BADDATA);

    crc = CRC_MASK;
    ReadCrc(header, headersize);
    header_crc = GetDWord();
    if ((crc ^ CRC_MASK) != header_crc)
        Error(AE_BADDATA);

    setup_get(header);
    uchar first_hdr_size = get_byte();
    headerData->ArcVer = arj_nbr = get_byte();
    headerData->ArcVerMin = arj_x_nbr = get_byte();
    headerData->HostOS = host_os = get_byte();
    headerData->Flags = arj_flags = get_byte();
    headerData->Method = method = get_byte();
    headerData->FileType = file_type = get_byte();
    if (first && file_type != 2)
        Error(AE_BADDATA);
    get_byte();
    DWORD dateTime = get_dword();
    FILETIME ft;
    if (!DosDateTimeToFileTime(HIWORD(dateTime),
                               LOWORD(dateTime), &ft))
    {
        SystemTimeToFileTime(&MinTime, &ft);
    }
    LocalFileTimeToFileTime(&ft, &headerData->Time);
    headerData->CompSize = compsize = get_dword();
    headerData->Size = origsize = get_dword();
    file_crc = get_dword();
    get_word();
    headerData->Attr = get_word() & 0x27;
    get_word();
    if (arj_flags & EXTFILE_FLAG)
        ext_size = get_dword();
    else
        ext_size = 0;

    char* hdr_filename = (char*)&header[first_hdr_size];
    lstrcpyn(headerData->FileName, hdr_filename, ARJ_MAX_PATH);
    if (host_os != OS && host_os != 11) // ani DOS, ani Win32
        FixParity((uchar*)headerData->FileName, lstrlen(headerData->FileName));
    if ((arj_flags & PATHSYM_FLAG) != 0)
        DecodePath(headerData->FileName);
    EnsureBackslashes(headerData->FileName);
    OemToChar(headerData->FileName, headerData->FileName);

    /*hdr_comment = (char *)&header[first_hdr_size + strlen(hdr_filename) + 1];
  lstrcpyn(comment, hdr_comment, sizeof(comment));
  if (host_os != OS)
      strparity((uchar *)comment);*/

    /* if extheadersize == 0 then no CRC */
    /* otherwise read extheader data and read 4 bytes for CRC */

    ushort extheadersize;
    while ((extheadersize = GetWord()) != 0)
        Seek(extheadersize + 4, FILE_CURRENT);

    NextFileOffset = GetPos() + compsize;

    return 1; /* success */
}

void UnStore()
{
    CALL_STACK_MESSAGE1("UnStore()");
    DWORD read;
    while (compsize)
    {
        read = compsize > DDICSIZ ? DDICSIZ : compsize;
        Read(text, read);
        WriteTxtCrc(text, read);
        compsize -= read;
    }
}

void Extract()
{
    CALL_STACK_MESSAGE1("Extract()");
    if (arj_x_nbr > ARJ_X_VERSION)
        ErrorSkip(AE_BADVERSION);

    if ((arj_flags & GARBLE_FLAG) != 0)
        ErrorSkip(AE_ENCRYPT);

    if (method < 0 || method > MAXMETHOD || (method == 4 && arj_nbr == 1))
        ErrorSkip(AE_METHOD);

    if (file_type != BINARY_TYPE && file_type != TEXT_TYPE)
        ErrorSkip(AE_UNKNTYPE);

    crc = CRC_MASK;

    switch (method)
    {
    case 0:
        UnStore();
        break;
    case 1:
    case 2:
    case 3:
        decode();
        break;
    case 4:
        decode_f();
        break;
    }

    if ((crc ^ CRC_MASK) != file_crc)
        ErrorSkip(AE_CRC);
}

//***********************************************************************************

void Error(int code)
{
    CALL_STACK_MESSAGE2("Error(%d)", code);
    ErrorProc(code, 0);
    throw 0;
}

void ErrorSkip(int code)
{
    CALL_STACK_MESSAGE2("ErrorSkip(%d)", code);
    ErrorProc(code, 0);
    Seek(NextFileOffset, FILE_BEGIN);
    throw 0;
}

void OpenArcFile()
{
    CALL_STACK_MESSAGE1("OpenArcFile()");
    while (1)
    {
        ArcFile = CreateFile(ArcName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (ArcFile != INVALID_HANDLE_VALUE)
            break; // ok
        if (!ErrorProc(AE_OPEN, EF_RETRY))
            throw 0;
    }
    while (1)
    {
        ArcFileSize = GetFileSize(ArcFile, NULL);
        if (ArcFileSize != 0xFFFFFFFF)
            break; // ok   //zde byl test na 0xFFFFFFF, coz neni -1
        if (!ErrorProc(AE_ACCESS, EF_RETRY))
        {
            CloseHandle(ArcFile);
            ArcFile = INVALID_HANDLE_VALUE;
            throw 0;
        }
    }
    InEnd = InPtr = InputBuffer;
    ArcFilePos = 0;

    DWORD pos = FindHeader();
    Seek(pos, FILE_BEGIN);
    CARJHeaderData headerData;
    ReadHeader(TRUE, &headerData);

    SuccedingVolume = headerData.Flags & VOLUME_FLAG;
}

void SafeSeek(DWORD position)
{
    CALL_STACK_MESSAGE2("SafeSeek(0x%X)", position);
    while (1)
    {
        if (SetFilePointer(ArcFile, position, NULL, FILE_BEGIN) != 0xFFFFFFFF)
            break;
        if (!ErrorProc(AE_ACCESS, EF_RETRY))
            throw 0;
    }
    ArcFilePos = position;
}

void __fastcall FillInputBuffer()
{
    CALL_STACK_MESSAGE1("FillInputBuffer()");
    DWORD toRead = INBUFSIZ;
    /*DWORD pos;
  while (1)
  {
    pos = SetFilePointer(ArcFile, 0, NULL, FILE_CURRENT);
    if (pos != 0xFFFFFFFF) break;
    if (!ErrorProc(AE_ACCESS, EF_RETRY)) throw 0;
  }
  */
    if (ArcFilePos == ArcFileSize)
        Error(AE_EOF);
    if (ArcFilePos + toRead > ArcFileSize)
        toRead = ArcFileSize - ArcFilePos;
    DWORD read;
    while (1)
    {
        if (ReadFile(ArcFile, InputBuffer, toRead, &read, NULL) && toRead == read)
            break; //sucess
        if (!ErrorProc(AE_ACCESS, EF_RETRY))
            throw 0;
        SafeSeek(ArcFilePos);
    }
    ArcFilePos += read;
    InPtr = InputBuffer;
    InEnd = InputBuffer + read;
}

void Seek(DWORD distance, DWORD method)
{
    CALL_STACK_MESSAGE_NONE
    DWORD destPos;
    if (method == FILE_BEGIN)
        destPos = distance;
    else
        destPos = ArcFilePos - (DWORD)(InEnd - InPtr) + distance;

    if (destPos >= ArcFilePos - (InEnd - InputBuffer) &&
        destPos < ArcFilePos)
    {
        InPtr = InEnd - (ArcFilePos - destPos);
    }
    else
    {
        SafeSeek(destPos);
        InEnd = InPtr = InputBuffer;
    }
}

DWORD
GetPos()
{
    CALL_STACK_MESSAGE_NONE
    return ArcFilePos - DWORD(InEnd - InPtr);
}

void Read(void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("Read(, 0x%X)", size);
    DWORD i;

    i = size > (DWORD)(InEnd - InPtr) ? (DWORD)(InEnd - InPtr) : size;
    memcpy(buffer, InPtr, i);
    InPtr += i;
    size -= i;
    if (size)
    {
        if (ArcFilePos + size > ArcFileSize)
            Error(AE_EOF);
        DWORD read;
        while (1)
        {
            if (ReadFile(ArcFile, (char*)buffer + i, size, &read, NULL) && size == read)
                break; //sucess
            if (!ErrorProc(AE_ACCESS, EF_RETRY))
                throw 0;
            SafeSeek(ArcFilePos);
        }
        ArcFilePos += read;
        InEnd = InPtr = InputBuffer;
    }
}

void NextVolume(BOOL forceQuestion)
{
    CALL_STACK_MESSAGE2("NextVolume(%d)", forceQuestion);
    CloseHandle(ArcFile);
    ArcFile = INVALID_HANDLE_VALUE;

    char prevName[MAX_PATH];
    char* ptr = strrchr(ArcName, '\\');
    if (!ptr)
        ptr = ArcName;
    else
        ptr++;
    strcpy(prevName, ptr);

    char* ext = PathFindExtension(ArcName);
    if (lstrcmpi(ext, ".arj") == 0)
    {
        lstrcpy(ext, ".a01");
    }
    else
    {
        if ((toupper(ext[1]) == 'A' || isdigit(ext[1])) &&
            isdigit(ext[2]) && isdigit(ext[3]))
        {
            int n;
            if (toupper(ext[1]) == 'A')
                n = atoi(ext + 2);
            else
                n = atoi(ext + 1);
            n++;
            if (n > 99 || isdigit(ext[1]))
                sprintf(ext, ".%03d", n);
            else
                sprintf(ext, ".a%02d", n);
        }
        else
            forceQuestion = TRUE;
    }
    DWORD attr = SalamanderGeneral->SalGetFileAttributes(ArcName);
    if (!forceQuestion && attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        if (!ChangeVolProc(ArcName, prevName, CVM_NOTIFY))
            throw 0;
    }
    else
    {
        while (1)
        {
            if (!ChangeVolProc(ArcName, prevName, CVM_ASK))
                throw 0;
            attr = SalamanderGeneral->SalGetFileAttributes(ArcName);
            if (attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                break; // ok mame ho, jedeme dal
        }
    }
    OpenArcFile();
}

//***********************************************************************************
//
// Interface UNARJ.DLL
//

BOOL WINAPI
ARJOpenArchive(CARJOpenData* openData)
{
    CALL_STACK_MESSAGE1("ARJOpenArchive()");
    ArcFile = INVALID_HANDLE_VALUE;
    lstrcpy(ArcName, openData->ArcName);
    ChangeVolProc = openData->ARJChangeVolProc;
    ProcessDataProc = openData->ARJProcessDataProc;
    ErrorProc = openData->ARJErrorProc;
    if (AchiveVolumes != NULL)
        TRACE_E("ARJOpenArchive(): unexpected situation: AchiveVolumes is not NULL!");
    AchiveVolumes = openData->AchiveVolumes;

    MakeCrcTable();

    try
    {
        OpenArcFile();

        return TRUE;
    }

    catch (int dummy)
    {
        dummy;
        if (ArcFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(ArcFile);
            ArcFile = INVALID_HANDLE_VALUE;
        }
        AchiveVolumes = NULL;
        return FALSE;
    }
}

BOOL WINAPI
ARJCloseArchive()
{
    CALL_STACK_MESSAGE1("ARJCloseArchive()");
    AchiveVolumes = NULL;
    if (ArcFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ArcFile);
        ArcFile = INVALID_HANDLE_VALUE;
    }
    return TRUE;
}

BOOL WINAPI
ARJReadHeader(CARJHeaderData* headerData)
{
    CALL_STACK_MESSAGE1("ARJReadHeader()");
    try
    {
        while (1)
        {
            if (ReadHeader(FALSE, headerData))
                break;
            if (!SuccedingVolume)
            {
                *headerData->FileName = 0;
                break;
            }
            NextVolume(FALSE);
            if (AchiveVolumes != NULL)
                AchiveVolumes->Add(ArcName, -2);
        }

        return TRUE;
    }

    catch (int dummy)
    {
        dummy;
        return FALSE;
    }
}

BOOL WINAPI
ARJProcessFile(int operation, LPDWORD size)
{
    CALL_STACK_MESSAGE2("ARJProcessFile(%d)", operation);
    if (size)
        *size = origsize;
    try
    {
        DWORD expectExtSize;
        BOOL forceQuestion;
        while (1)
        {
            if (operation == PFO_SKIP)
                Seek(NextFileOffset, FILE_BEGIN);
            else
            {
                Extract();
            }
            if (!(arj_flags & VOLUME_FLAG))
                break;

            expectExtSize = ext_size + origsize;
            forceQuestion = FALSE;

        PF_NEXTVOL:

            NextVolume(forceQuestion);
            CARJHeaderData headerData;
            ReadHeader(FALSE, &headerData);
            if (ext_size != expectExtSize)
            {
                if (!ErrorProc(AE_BADVOL, EF_RETRY))
                    throw 0;
                forceQuestion = TRUE;
                goto PF_NEXTVOL;
            }
            if (size)
                *size = ext_size + origsize;
            if (AchiveVolumes != NULL)
                AchiveVolumes->Add(ArcName, -2);
        }

        return TRUE;
    }

    catch (int dummy)
    {
        dummy;
        return FALSE;
    }
}
