// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MPG_SUPPORT_

//#define TAG_TRACE_I(str) TRACE_I(str)
//#define TAG_TRACE_E(str) TRACE_E(str)
#define TAG_TRACE_I(str)
#define TAG_TRACE_E(str)

#include "id3tagv2.h"
#include "id3tagv1.h"
#include <assert.h>

extern CSalamanderGeneralAbstract* SalGeneral;

//#define PROCESS_ALSO_FUTURE_VERSIONS_OF_ID3V2TAG

class ID3TagV2FrameHeaderAbstract;

typedef BOOL (*READFRM_FNC)(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str);

BOOL ReadTextInfoFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str);
BOOL ReadUserTextFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str);
BOOL ReadUserURLFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str);
BOOL ReadCommFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str);
BOOL ReadLinkFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str);

typedef struct
{
    char id22[3];
    char id23[4];
    READFRM_FNC readfrm;
    int var_offset;
} ID3TAGV2_FRAME_HANDLE;

ID3TAGV2_FRAME_HANDLE id3v2_frames[] =
    {
        {'C', 'R', 'A', 'A', 'E', 'N', 'C', NULL, -1},
        {'P', 'I', 'C', 'A', 'P', 'I', 'C', NULL, -1},
        {'C', 'O', 'M', 'C', 'O', 'M', 'M', ReadCommFrm, 4},
        {'-', '-', '-', 'C', 'O', 'M', 'R', NULL, -1},
        {'-', '-', '-', 'E', 'N', 'C', 'R', NULL, -1},
        {'E', 'Q', 'U', 'E', 'Q', 'U', 'A', NULL, -1},
        {'E', 'T', 'C', 'E', 'T', 'C', 'O', NULL, -1},
        {'G', 'E', 'O', 'G', 'E', 'O', 'B', NULL, -1},
        {'-', '-', '-', 'G', 'R', 'I', 'D', NULL, -1},
        {'I', 'P', 'L', 'I', 'P', 'L', 'S', NULL, -1},
        {'L', 'N', 'K', 'L', 'I', 'N', 'K', NULL, -1},
        {'M', 'C', 'I', 'M', 'C', 'D', 'I', NULL, -1},
        {'M', 'L', 'L', 'M', 'L', 'L', 'T', NULL, -1},
        {'-', '-', '-', 'O', 'W', 'N', 'E', NULL, -1},
        {'-', '-', '-', 'P', 'R', 'I', 'V', NULL, -1},
        {'C', 'N', 'T', 'P', 'C', 'N', 'T', NULL, -1},
        {'P', 'O', 'P', 'P', 'O', 'P', 'M', NULL, -1},
        {'-', '-', '-', 'P', 'O', 'S', 'S', NULL, -1},
        {'B', 'U', 'F', 'R', 'B', 'U', 'F', NULL, -1},
        {'R', 'V', 'A', 'R', 'V', 'A', 'D', NULL, -1},
        {'R', 'E', 'V', 'R', 'V', 'R', 'B', NULL, -1},
        {'S', 'L', 'T', 'S', 'Y', 'L', 'T', NULL, -1},
        {'S', 'T', 'C', 'S', 'Y', 'T', 'C', NULL, -1},
        {'T', 'A', 'L', 'T', 'A', 'L', 'B', ReadTextInfoFrm, 2},
        {'T', 'B', 'P', 'T', 'B', 'P', 'M', ReadTextInfoFrm, -1},
        {'T', 'C', 'M', 'T', 'C', 'O', 'M', ReadTextInfoFrm, 9},
        {'T', 'C', 'O', 'T', 'C', 'O', 'N', ReadTextInfoFrm, 5},
        {'T', 'C', 'R', 'T', 'C', 'O', 'P', ReadTextInfoFrm, 7},
        {'T', 'D', 'A', 'T', 'D', 'A', 'T', ReadTextInfoFrm, -1},
        {'T', 'D', 'Y', 'T', 'D', 'L', 'Y', ReadTextInfoFrm, -1},
        {'T', 'E', 'N', 'T', 'E', 'N', 'C', ReadTextInfoFrm, 8},
        {'T', 'X', 'T', 'T', 'E', 'X', 'T', ReadTextInfoFrm, -1},
        {'T', 'F', 'T', 'T', 'F', 'L', 'T', ReadTextInfoFrm, -1},
        {'T', 'I', 'M', 'T', 'I', 'M', 'E', ReadTextInfoFrm, -1},
        {'T', 'T', '1', 'T', 'I', 'T', '1', ReadTextInfoFrm, -1},
        {'T', 'T', '2', 'T', 'I', 'T', '2', ReadTextInfoFrm, 0},
        {'T', 'T', '3', 'T', 'I', 'T', '3', ReadTextInfoFrm, -1},
        {'T', 'K', 'E', 'T', 'K', 'E', 'Y', ReadTextInfoFrm, -1},
        {'T', 'L', 'A', 'T', 'L', 'A', 'N', ReadTextInfoFrm, -1},
        {'T', 'L', 'E', 'T', 'L', 'E', 'N', ReadTextInfoFrm, -1},
        {'T', 'M', 'T', 'T', 'M', 'E', 'D', ReadTextInfoFrm, -1},
        {'T', 'O', 'T', 'T', 'O', 'A', 'L', ReadTextInfoFrm, -1},
        {'T', 'O', 'F', 'T', 'O', 'F', 'N', ReadTextInfoFrm, -1},
        {'T', 'O', 'L', 'T', 'O', 'L', 'Y', ReadTextInfoFrm, -1},
        {'T', 'O', 'A', 'T', 'O', 'P', 'E', ReadTextInfoFrm, 10},
        {'T', 'O', 'R', 'T', 'O', 'R', 'Y', ReadTextInfoFrm, -1},
        {'-', '-', '-', 'T', 'O', 'W', 'N', ReadTextInfoFrm, -1},
        {'T', 'P', '1', 'T', 'P', 'E', '1', ReadTextInfoFrm, 1},
        {'T', 'P', '2', 'T', 'P', 'E', '2', ReadTextInfoFrm, -1},
        {'T', 'P', '3', 'T', 'P', 'E', '3', ReadTextInfoFrm, -1},
        {'T', 'P', '4', 'T', 'P', 'E', '4', ReadTextInfoFrm, -1},
        {'T', 'P', 'A', 'T', 'P', 'O', 'S', ReadTextInfoFrm, -1},
        {'T', 'P', 'B', 'T', 'P', 'U', 'B', ReadTextInfoFrm, 11},
        {'T', 'R', 'K', 'T', 'R', 'C', 'K', ReadTextInfoFrm, 6},
        {'T', 'R', 'D', 'T', 'R', 'D', 'A', ReadTextInfoFrm, -1},
        {'T', 'R', 'N', 'T', 'R', 'S', 'N', ReadTextInfoFrm, -1},
        {'T', 'R', 'O', 'T', 'R', 'S', 'O', ReadTextInfoFrm, -1},
        {'T', 'S', 'I', 'T', 'S', 'I', 'Z', ReadTextInfoFrm, -1},
        {'T', 'R', 'C', 'T', 'S', 'R', 'C', ReadTextInfoFrm, -1},
        {'T', 'S', 'S', 'T', 'S', 'S', 'E', ReadTextInfoFrm, -1},
        {'T', 'Y', 'E', 'T', 'Y', 'E', 'R', ReadTextInfoFrm, 3},
        {'T', 'X', 'X', 'T', 'X', 'X', 'X', ReadUserTextFrm, 21},
        {'U', 'F', 'I', 'U', 'F', 'I', 'D', NULL, -1},
        {'-', '-', '-', 'U', 'S', 'E', 'R', NULL, -1},
        {'U', 'L', 'T', 'U', 'S', 'L', 'T', NULL, -1},
        {'W', 'C', 'M', 'W', 'C', 'O', 'M', ReadLinkFrm, 12},
        {'W', 'C', 'P', 'W', 'C', 'O', 'P', ReadLinkFrm, 13},
        {'W', 'A', 'F', 'W', 'O', 'A', 'F', ReadLinkFrm, 14},
        {'W', 'A', 'R', 'W', 'O', 'A', 'R', ReadLinkFrm, 15},
        {'W', 'A', 'S', 'W', 'O', 'A', 'S', ReadLinkFrm, 16},
        {'W', 'R', 'A', 'W', 'O', 'R', 'S', ReadLinkFrm, 17},
        {'W', 'P', 'Y', 'W', 'P', 'A', 'Y', ReadLinkFrm, 18},
        {'W', 'P', 'B', 'W', 'P', 'U', 'B', ReadLinkFrm, 19},
        {'W', 'X', 'X', 'W', 'X', 'X', 'X', ReadUserURLFrm, 20},

        //nove framy od verze 2.4
        {'-', '-', '-', 'A', 'S', 'P', 'I', NULL, -1},            //Audio seek point index [F:4.30]
        {'-', '-', '-', 'E', 'Q', 'U', '2', NULL, -1},            //Equalisation (2) [F:4.12]
        {'-', '-', '-', 'R', 'V', 'A', '2', NULL, -1},            //Relative volume adjustment (2) [F:4.11]
        {'-', '-', '-', 'S', 'E', 'E', 'K', NULL, -1},            //Seek frame [F:4.29]
        {'-', '-', '-', 'S', 'I', 'G', 'N', NULL, -1},            //Signature frame [F:4.28]
        {'-', '-', '-', 'T', 'D', 'E', 'N', ReadTextInfoFrm, -1}, //Encoding time [F:4.2.5]
        {'-', '-', '-', 'T', 'D', 'O', 'R', ReadTextInfoFrm, -1}, //Original release time [F:4.2.5]
        {'-', '-', '-', 'T', 'D', 'R', 'C', ReadTextInfoFrm, 3},  //Recording time [F:4.2.5] <-- nahrazka TYER !!
        {'-', '-', '-', 'T', 'D', 'R', 'L', ReadTextInfoFrm, -1}, //Release time [F:4.2.5]
        {'-', '-', '-', 'T', 'D', 'T', 'G', ReadTextInfoFrm, -1}, //Tagging time [F:4.2.5]
        {'-', '-', '-', 'T', 'I', 'P', 'L', ReadTextInfoFrm, -1}, //Involved people list [F:4.2.2]
        {'-', '-', '-', 'T', 'M', 'C', 'L', ReadTextInfoFrm, -1}, //Musician credits list [F:4.2.2]
        {'-', '-', '-', 'T', 'M', 'O', 'O', ReadTextInfoFrm, -1}, //Mood [F:4.2.3]
        {'-', '-', '-', 'T', 'P', 'R', 'O', ReadTextInfoFrm, -1}, //Produced notice [F:4.2.4]
        {'-', '-', '-', 'T', 'S', 'O', 'A', ReadTextInfoFrm, -1}, //Album sort order [F:4.2.5]
        {'-', '-', '-', 'T', 'S', 'O', 'P', ReadTextInfoFrm, -1}, //Performer sort order [F:4.2.5]
        {'-', '-', '-', 'T', 'S', 'O', 'T', ReadTextInfoFrm, -1}, //Title sort order [F:4.2.5]
        {'-', '-', '-', 'T', 'S', 'S', 'T', ReadTextInfoFrm, -1}, //Set subtitle [F:4.2.1]
};

class ID3TagV2FrameHeaderAbstract
{
public:
    int headsize;

    ID3TagV2FrameHeaderAbstract(void) { headsize = 0; }
    virtual ~ID3TagV2FrameHeaderAbstract(void) {}

    virtual BOOL Read(FILE* f) = 0;
    virtual BOOL CompareID(ID3TAGV2_FRAME_HANDLE& h) const = 0;
    virtual BOOL IsValid(void) const = 0;
};

class ID3TagV22FrameHeader : public ID3TagV2FrameHeaderAbstract
{
protected:
    char id[3];
    BYTE size[3]; //Tag size excluding header

public:
    ID3TagV22FrameHeader(void) {}
    virtual ~ID3TagV22FrameHeader(void) {}

    virtual BOOL IsValid(void) const
    {
        return ((((id[0] >= 'A') && (id[0] <= 'Z')) || ((id[0] >= '0') && (id[0] <= '9'))) &&
                (((id[1] >= 'A') && (id[1] <= 'Z')) || ((id[1] >= '0') && (id[1] <= '9'))) &&
                (((id[2] >= 'A') && (id[2] <= 'Z')) || ((id[2] >= '0') && (id[2] <= '9'))));
    }

    virtual BOOL Read(FILE* f)
    {
        BOOL r = ((fread(&id, sizeof(id), 1, f) == 1) && (fread(&size, sizeof(size), 1, f) == 1));
        if (r)
        {
            headsize = CONVERT_C2DW3(size);
            return IsValid();
            ;
        }
        else
            return FALSE;
    }

    virtual BOOL CompareID(ID3TAGV2_FRAME_HANDLE& h) const { return (memcmp(id, h.id22, sizeof(id)) == 0); }
};

class ID3TagV23FrameHeader : public ID3TagV2FrameHeaderAbstract
{
protected:
    char id[4];
    BYTE size[4];  //Tag size excluding header
    BYTE flags[2]; //Flags of tag

public:
    ID3TagV23FrameHeader(void) {}
    virtual ~ID3TagV23FrameHeader(void) {}

    virtual BOOL IsValid(void) const
    {
        return ((((id[0] >= 'A') && (id[0] <= 'Z')) || ((id[0] >= '0') && (id[0] <= '9'))) &&
                (((id[1] >= 'A') && (id[1] <= 'Z')) || ((id[1] >= '0') && (id[1] <= '9'))) &&
                (((id[2] >= 'A') && (id[2] <= 'Z')) || ((id[2] >= '0') && (id[2] <= '9'))) &&
                (((id[3] >= 'A') && (id[3] <= 'Z')) || ((id[3] >= '0') && (id[3] <= '9'))));
    }

    virtual BOOL Read(FILE* f)
    {
        BOOL r = ((fread(&id, sizeof(id), 1, f) == 1) && (fread(&size, sizeof(size), 1, f) == 1) && (fread(&flags, sizeof(flags), 1, f) == 1));
        if (r)
        {
            headsize = CONVERT_C2DW4(size);
            return IsValid();
        }
        else
            return FALSE;
    }

    virtual BOOL CompareID(ID3TAGV2_FRAME_HANDLE& h) const { return (memcmp(id, h.id23, sizeof(id)) == 0); }
};

static void WipeOutBlanks(int init_pos, char* str)
{
    int p = init_pos;

    while (((str[p] == 32) || (str[p] == '\0')))
    {
        str[p] = '\0';
        p--;
    }
}

static void WipeOutNewLines(char* str, int len)
{
    int idx = 0;
    while (idx < len)
    {
        if (str[idx + 1]) //kontrola windows/dos newline
        {
            if ((str[idx] == 13) && (str[idx + 1] == 10))
            {
                str[idx] = 32;
                str[idx + 1] = 32;
            }
        }

        if (str[idx] == 10) //kontrola unix newline
        {
            str[idx] = 32;
        }

        idx++;
    }
}

// NOTE: converts to UTF8 on NT-class OS's
// NOTE: NULL lpStr & new_len -> get needed buffer size
int Unicode16ToAnsi(char* lpStr, LPCWSTR wstr, int len, int new_len)
{
    int ret;

    // The last two arguments must be NULL for CP_UTF8
    ret = WideCharToMultiByte(CP_UTF8, 0, wstr, len, lpStr, new_len, NULL, NULL);
    if (lpStr)
        lpStr[new_len - 1] = 0;
    return ret;
}

BOOL SwitchUnicodeString(LPWSTR wstr, int len)
{
    if ((len % 2) != 0)
        return FALSE;

    char* str = (char*)wstr;
    char tmp;

    int i;
    for (i = 0; i < len; i += 2)
    {
        tmp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = tmp;
    }

    return TRUE;
}

//precte retezec a vzdy vytvori NULL/NULLNULL
BOOL ReadStr(FILE* f, char** out_str, long& size, int encoding = 0)
{
    if (*out_str)
        free(*out_str);

    if (size == 0)
        return TRUE; //vycistil jsem retezec a konec

    int nullterm = (encoding == 0) ? 1 : 2;

    *out_str = (char*)malloc(size + nullterm);

    if (*out_str)
    {
        if (signed(fread(*out_str, 1, size, f)) != size)
        {
            free(*out_str);
            *out_str = NULL;
            return FALSE;
        }

        //pridal jsem NULL terminator retezce (i pro unicode)
        (*out_str)[size] = 0;

        if (encoding == 1) //a dalsi pro unicode
            (*out_str)[size + 1] = 0;

        size += nullterm;
        return TRUE;
    }

    return FALSE;
}

BOOL ConvertStr(char** out_str, long& size, int encoding = 0)
{
    assert(size > 0);
    /*encoding:
    0 - ISO-8859-1 .....na konci NULL terminator
    1 - 16bit Unicode ..na zacatku Unicode BOM ($FF FE or $FE FF) - definuje byte order
                        na konci Unicode NULL ($FF FE 00 00 nebo $FE FF 00 00).
    2 - UTF16 BE with BOM
    3 - UTF8 without BOM
  */

    if (*out_str)
    {
        if (encoding == 0) //ISO-8859-1
        {
        }
        else if ((encoding == 1 /*UTF16LE*/) || (encoding == 2 /*UTF16BE*/))
        {
            if (size < 4)
                return FALSE;

            if (((*out_str)[0] == 0xFE) && ((*out_str)[1] == 0xFF)) //obraceny BOM, prohodit
                SwitchUnicodeString(((WCHAR*)(*out_str)) + 1, size - 2);

            assert((size % 2) == 0);

            if ((size % 2) != 0) //to je nakej kix, zkus to takhle
                size++;

            //a tady uz mame ansi size
            size = (size - 2 /*-BOM*/) / 2;

            int new_size = Unicode16ToAnsi(NULL, ((WCHAR*)(*out_str)) + 1, size, 0);

            char* new_out_str = (char*)malloc(new_size);

            if (!new_out_str)
            {
                free(*out_str);
                *out_str = NULL;
                return FALSE;
            }

            Unicode16ToAnsi(new_out_str, ((WCHAR*)(*out_str)) + 1, size, new_size); //zkonvertuj (preskocime BOM)
            size = new_size;

            //free(*out_str);
            *out_str = new_out_str;
        }
        else if (encoding == 3 /*UTF8 w/o BOM*/)
        {
        }
        else
            return FALSE;

        (*out_str)[size - 1] = '\0';
        WipeOutBlanks(size - 1, *out_str); //smaz pripadne prebytecne mezery (u tagu1 byvaly, tady asi ne, ale sichr je sichr)
        WipeOutNewLines(*out_str, size - 1);

        return TRUE;
    }

    return FALSE;
}

//rozdeli retezec na dva v miste, kde je NULL term.
BOOL BreakStr(char** out_str, int size, int encoding, char** pp1, int& sizep1, char** pp2, int& sizep2, BOOL second_is_ansi = FALSE)
{
    char* part1;
    char* part2;

    long size1;
    long size2;

    *pp1 = NULL;
    *pp2 = NULL;
    sizep1 = 0;
    sizep2 = 0;

    part1 = *out_str;
    part2 = *out_str;

    if (encoding == 0)
    {
        while (*part2 != '\0')
            part2++;

        part2++; //ted preskocim i ten null terminator

        size1 = (long)(part2 - part1);
        size2 = size - size1;

        char* tmp = (char*)malloc(size2);
        if (tmp)
        {
            memcpy(tmp, part2, size2);
            part2 = tmp;
        }
        else
            return FALSE;
    }
    else if (encoding == 1)
    {
        LPWSTR ws = (WCHAR*)part2;

        while (*ws != 0x0000)
            ws++;

        ws++; //ted preskocim i ten null terminator

        size1 = (long)(((char*)(ws)) - part1);
        size2 = size - size1;

        // NOTE: tmp is likely to leak :-(
        char* tmp = (char*)malloc(size2);
        if (tmp)
        {
            memcpy(tmp, ws, size2);
            part2 = tmp;
        }
        else
            return FALSE;
    }
    else
        return FALSE;

    if (size1 && ConvertStr((char**)&part1, size1, encoding) &&
        size2 && ConvertStr((char**)&part2, size2, second_is_ansi ? 0 : encoding))
    {
        *pp1 = part1;
        *pp2 = part2;
        sizep1 = size1;
        sizep2 = size2;

        return TRUE;
    }

    return FALSE;
}

BOOL ReadTextInfoFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str)
{
    if (!pfh || !out_str)
        return FALSE;

    long size = pfh->headsize;

    char encoding;

    if (fread(&encoding, 1, 1, f) != 1)
        return FALSE;

    size--;

    if (size == 0)
        return FALSE;

    if (ReadStr(f, out_str, size, encoding))
    {
        if (ConvertStr(out_str, size, encoding))
        {
            if ((*out_str) && pfh->CompareID(id3v2_frames[26]))
            {
                //specialni prefiltrovani GENRE (zavorky s cislem jakozto index v tag1 genre-listu...)
                char* tmp;
                char* string = *out_str;
                char* new_out_str = NULL;

                if ((string[0] == '(') && (tmp = strchr(string, ')')) != NULL)
                {
                    if (strlen(tmp) > 1)
                    {
                        //Zkonvertuj genre zapsane jako '(3)Dance' --> 'Dance'
                        new_out_str = _strdup /*SalGeneral->DupStr*/ (tmp + 1);
                    }
                    else
                    {
                        *tmp = '\0';
                        new_out_str = _strdup /*SalGeneral->DupStr*/ (ID3TAGV1_GetGenreStr(atoi(string + 1)));
                    }
                }

                if (new_out_str)
                {
                    free(*out_str);
                    *out_str = new_out_str;
                }
            }
        }

        return TRUE;
    }

    if (*out_str)
    {
        free(*out_str);
        *out_str = NULL;
    }

    return FALSE;
}

void ReadBreakStr(FILE* f, char** out_str, long size, int encoding)
{
    if (ReadStr(f, out_str, size, encoding))
    {
        char* p1;
        char* p2;
        int p1size;
        int p2size;

        //komentar se sklada ze 2 casti. Mezi nimi je NULL terminator. Rozdelim je
        if (BreakStr(out_str, size, encoding, &p1, p1size, &p2, p2size))
        {
            char separator[] = " - ";
            char* new_out_str = (char*)malloc(p1size + p2size + sizeof(separator)); //to bude par znaku navic, to je OK

            if (new_out_str)
            {
                strcpy(new_out_str, p1);

                if (p1[0])
                    strcat(new_out_str, separator);

                strcat(new_out_str, p2);
            }

            if (p1)
                free(p1);

            if (p2)
                free(p2);

            if (*out_str && (p1 != *out_str /*sometimes!*/))
                free(*out_str);

            *out_str = new_out_str; //v pripade chyby alokace vrati null
        }
    }
}

BOOL ReadCommFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str)
{
    if (!pfh || !out_str)
        return FALSE;

    long size = pfh->headsize;

    struct
    {
        BYTE encoding;
        BYTE lng[3];
    } comm;

    if (fread(&comm, sizeof(comm), 1, f) != 1)
        return FALSE;

    size -= sizeof(comm);

    if (size == 0)
        return FALSE;

    ReadBreakStr(f, out_str, size, comm.encoding);

    return FALSE;
}

BOOL ReadUserTextFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str)
{
    if (!pfh || !out_str)
        return FALSE;

    long size = pfh->headsize;

    char encoding;

    if (fread(&encoding, sizeof(encoding), 1, f) != 1)
        return FALSE;

    size -= sizeof(encoding);

    if (size == 0)
        return FALSE;

    ReadBreakStr(f, out_str, size, encoding);

    return FALSE;
}

BOOL ReadUserURLFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str)
{
    if (!pfh || !out_str)
        return FALSE;

    long size = pfh->headsize;

    char encoding;

    if (fread(&encoding, sizeof(encoding), 1, f) != 1)
        return FALSE;

    size -= sizeof(encoding);

    if (size == 0)
        return FALSE;

    ReadBreakStr(f, out_str, size, encoding);

    return FALSE;
}

BOOL ReadLinkFrm(FILE* f, const ID3TagV2FrameHeaderAbstract* pfh, char** out_str)
{
    if (!pfh || !out_str)
        return FALSE;

    long size = pfh->headsize;

    if (size == 0)
        return FALSE;

    return (ReadStr(f, out_str, size)) && (ConvertStr(out_str, size));
}

BOOL ID3TAGV2_Read(FILE* f, const ID3TAGV2_HEADER* pmh, ID3TAGV2_DECODED* phd)
{
    if (f && pmh && phd)
    {
        if (pmh->ver > 4)
            return FALSE; //zatim nepodporuji verze > id3v2.4

        memset(phd, 0, sizeof(ID3TAGV2_DECODED));

        phd->version = MAKEWORD(pmh->ver, 2);
        phd->tagsize = ConvertInt28(pmh->size);
        //projedu vsechny framy, ktere tag obsahuje

        ID3TagV2FrameHeaderAbstract* frm = NULL;
        switch (pmh->ver)
        {
        case 2:
            frm = new ID3TagV22FrameHeader;
            break;

        case 3:
        case 4:
#ifdef PROCESS_ALSO_FUTURE_VERSIONS_OF_ID3V2TAG
        default: //zkusim parsnout i tag budouci (vyssi) verze
#endif
            frm = new ID3TagV23FrameHeader; //2.4 je stejny jako 2.3
            break;
        }

        if (frm == NULL)
            return FALSE;

        int cnt = sizeof(id3v2_frames) / sizeof(ID3TAGV2_FRAME_HANDLE);

        long p;
        long endtagpos = ftell(f) + phd->tagsize;

        while (!feof(f) && (ftell(f) < endtagpos))
        {
            if (frm->Read(f))
            {
                p = ftell(f);
                long size = frm->headsize;

                if (size > 0)
                {
                    int i;
                    for (i = 0; i < cnt; i++)
                    {
                        ID3TAGV2_FRAME_HANDLE& fh = id3v2_frames[i];
                        if (frm->CompareID(fh))
                        {
                            if (fh.readfrm && (fh.var_offset != -1))
                            {
                                TAG_TRACE_I("Reading frame " << fh.id23);
                                //(char**)(LPBYTE(phd)+fh.var_offset/4)
                                if (!fh.readfrm(f, frm, &(phd->items[fh.var_offset])))
                                {
                                    //
                                    TAG_TRACE_E("  Failed to read the frame " << fh.id23);
                                }
                                TAG_TRACE_I("  Frame " << fh.id23 << " successfully parsed (" << phd->items[fh.var_offset] << ")");
                            }

                            break;
                        }
                    }
                }

                fseek(f, p + size, SEEK_SET); //nastav se na konec frame
            }
        }

        delete frm;

        return TRUE;
    }

    return FALSE;
}

#define SAFE_FREE_TAGITEM(x) \
    if (phd->x) \
    { \
        /*SalGeneral->Free*/ free(phd->x); \
        phd->x = NULL; \
    }

BOOL ID3TAGV2_Free(ID3TAGV2_DECODED* phd)
{
    if (phd)
    {
        int i;
        for (i = 0; i < sizeof(phd->items) / sizeof(char*); i++)
            SAFE_FREE_TAGITEM(items[i]);

        phd->tagsize = 0;
        phd->version = 0;
        phd->reserved = 0;

        return TRUE;
    }

    return FALSE;
}

#define ID3V2H_UNSYNC (1 << 7)
#define ID3V2H_EXTENDED (1 << 6)
#define ID3V2H_EXPERIMENTAL (1 << 5)

BOOL ID3TAGV2_ReadMainHead(FILE* f, ID3TAGV2_HEADER* pmh)
{
    if (f && pmh)
    {
        int fpos = ftell(f);

        if ((fread(pmh, sizeof(ID3TAGV2_HEADER), 1, f) == 1) &&
            (pmh->id[0] == 'I') && (pmh->id[1] == 'D') && (pmh->id[2] == '3'))
        {
            if (pmh->flags & ID3V2H_EXTENDED)
            {
                //zpracuj extended header
                switch (pmh->ver)
                {
                case 2:           //v2.2
                    return FALSE; //v teto verzi znamena 6 bit compressed. Komprimovane neberu. Extended zde neni

                case 3: //v2.3
                case 4: //v2.4
#ifdef PROCESS_ALSO_FUTURE_VERSIONS_OF_ID3V2TAG
                default: //zkusim parsnout i tag budouci (vyssi) verze
#endif
                {
                    BYTE exthead_size[4];
                    if (fread(&exthead_size, sizeof(exthead_size), 1, f) == 1)
                    {
                        DWORD exthead_dwsize = ConvertInt28(exthead_size);
                        return (fseek(f, exthead_dwsize, SEEK_CUR) == 0);
                    }
                }
                break;

#ifndef PROCESS_ALSO_FUTURE_VERSIONS_OF_ID3V2TAG
                default: //zkusim parsnout i tag budouci (vyssi) verze
                    return FALSE;
#endif
                }
            }

            return TRUE;
        }
        else
        {
            //get back to init position
            fseek(f, fpos, SEEK_SET);
            return FALSE;
        }
    }

    return FALSE;
}

#define MASK(bits) ((1 << (bits)) - 1)

DWORD ConvertInt28(const BYTE size[4])
{
    DWORD val = 0;
    const unsigned short BITSUSED = 7;

    size_t i;
    for (i = 0; i < sizeof(DWORD); ++i)
        val = (val << BITSUSED) | static_cast<DWORD>(size[i]) & MASK(BITSUSED);

    return val;
}

DWORD ConvertBENumber(const BYTE size[4])
{
    DWORD val = 0;

    int i;
    for (i = 0; i < sizeof(DWORD); ++i)
    {
        val *= 256; // 2^8
        val += static_cast<DWORD>(0xFF & size[i]);
    }

    return val;
}

#endif

//language
/*
Abkhazian; abk
Achinese; ace
Acoli; ach
Adangme; ada
Afar; aar
Afrihili; afh
Afrikaans; afr
Afro-Asiatic (Other); afa
Akan; aka
Akkadian; akk
Albanian; alb/sqi
Aleut; ale
Algonquian languages; alg
Altaic (Other); tut
Amharic; amh
Apache languages; apa
Arabic; ara
Aramaic; arc
Arapaho; arp
Araucanian; arn
Arawak; arw
Armenian; arm/hye
Artificial (Other); art
Assamese; asm

Asturian; ast
Athapascan languages; ath
Australian languages; aus
Austronesian (Other); map
Avaric; ava
Avestan; ave
Awadhi; awa
Aymara; aym
Azerbaijani; aze
Bable; ast

Balinese; ban
Baltic (Other); bat
Baluchi; bal
Bambara; bam
Bamileke languages; bai
Banda; bad
Bantu (Other); bnt
Basa; bas
Bashkir; bak
Basque; baq/eus
Batak (Indonesia); btk
Beja; bej
Belarusian; bel
Bemba; bem
Bengali; ben
Berber (Other); ber
Bhojpuri; bho
Bihari; bih
Bikol; bik
Bini; bin
Bislama; bis
Bokmal, Norwegian; nob
Bosnian; bos
Braj; bra
Breton; bre
Buginese; bug
Bulgarian; bul
Buriat; bua
Burmese; bur/mya

Caddo; cad
Carib; car
Castilian; spa
Catalan; cat
Caucasian (Other); cau
Cebuano; ceb
Celtic (Other); cel
Central American Indian (Other); cai
Chagatai; chg
Chamic languages; cmc
Chamorro; cha
Chechen; che
Cherokee; chr
Chewa; nya
Cheyenne; chy
Chibcha; chb
Chichewa; nya
Chinese; chi/zho
Chinook jargon; chn
Chipewyan; chp
Choctaw; cho
Chuang; zha
Church Slavic; chu
Church Slavonic; chu
Chuukese; chk
Chuvash; chv
Coptic; cop
Cornish; cor
Corsican; cos
Cree; cre
Creek; mus
Creoles and pidgins(Other); crp
Creoles and pidgins, English-based (Other); cpe
Creoles and pidgins, French-based (Other); cpf
Creoles and pidgins, Portuguese-based (Other); cpp
Croatian; scr/hrv
Cushitic (Other); cus
Czech; cze/ces

Dakota; dak
Danish; dan
Dayak; day
Delaware; del
Dinka; din
Divehi; div
Dogri; doi
Dogrib; dgr
Dravidian (Other); dra
Duala; dua
Dutch; dut/nld
Dutch, Middle (ca. 1050-1350); dum
Dyula; dyu
Dzongkha; dzo

Efik; efi
Egyptian (Ancient); egy
Ekajuk; eka
Elamite; elx
English; eng
English, Middle (1100-1500); enm
English, Old (ca.450-1100); ang
Esperanto; epo
Estonian; est
Ewe; ewe
Ewondo; ewo

Fang; fan
Fanti; fat
Faroese; fao
Fijian; fij
Finnish; fin
Finno-Ugrian (Other); fiu
Fon; fon
French; fre/fra
French, Middle (ca.1400-1600); frm
French, Old (842-ca.1400); fro
Frisian; fry
Friulian; fur
Fulah; ful

Ga; gaa
Gaelic; gla
Gallegan; glg
Ganda; lug
Gayo; gay
Gbaya; gba
Geez; gez
Georgian; geo/kat
German; ger/deu
German, Low; nds
German, Middle High (ca.1050-1500); gmh
German, Old High (ca.750-1050); goh
Germanic (Other); gem
Gikuyu; kik
Gilbertese; gil
Gondi; gon
Gorontalo; gor
Gothic; got
Grebo; grb
Greek, Ancient (to 1453); grc
Greek, Modern (1453-); gre/ell
Guarani; grn
Gujarati; guj
Gwich´in; gwi

Haida; hai
Hausa; hau
Hawaiian; haw
Hebrew; heb
Herero; her
Hiligaynon; hil
Himachali; him
Hindi; hin
Hiri Motu; hmo
Hittite; hit
Hmong; hmn
Hungarian; hun
Hupa; hup

Iban; iba
Icelandic; ice/isl
Ido; ido
Igbo; ibo
Ijo; ijo
Iloko; ilo
Inari Sami; smn
Indic (Other); inc
Indo-European (Other); ine
Indonesian; ind
Interlingua (International Auxiliary Language Association); ina
Interlingue; ile
Inuktitut; iku
Inupiaq; ipk
Iranian (Other); ira
Irish; gle
Irish, Middle (900-1200); mga
Irish, Old (to 900); sga
Iroquoian languages; iro
Italian; ita

Japanese; jpn
Javanese; jav
Judeo-Arabic; jrb
Judeo-Persian; jpr

Kabyle; kab
Kachin; kac
Kalaallisut; kal
Kamba; kam
Kannada; kan
Kanuri; kau
Kara-Kalpak; kaa
Karen; kar
Kashmiri; kas
Kawi; kaw
Kazakh; kaz
Khasi; kha
Khmer; khm
Khoisan (Other); khi
Khotanese; kho
Kikuyu; kik
Kimbundu; kmb
Kinyarwanda; kin
Kirghiz; kir
Komi; kom
Kongo; kon
Konkani; kok
Korean; kor
Kosraean; kos
Kpelle; kpe
Kru; kro
Kuanyama; kua
Kumyk; kum
Kurdish; kur
Kurukh; kru
Kutenai; kut
Kwanyama; kua

Ladino; lad
Lahnda; lah
Lamba; lam
Lao; lao
Latin; lat
Latvian; lav
Letzeburgesch; ltz
Lezghian; lez
Lingala; lin
Lithuanian; lit
Low German; nds
Low Saxon; nds
Lozi; loz
Luba-Katanga; lub
Luba-Lulua; lua
Luiseno; lui
Lule Sami; smj
Lunda; lun
Luo (Kenya and Tanzania); luo
Luxembourgish; ltz
Lushai; lus

Macedonian; mac/mkd
Madurese; mad
Magahi; mag
Maithili; mai
Makasar; mak
Malagasy; mlg
Malay; may/msa
Malayalam; mal
Maltese; mlt
Manchu; mnc
Mandar; mdr
Mandingo; man
Manipuri; mni
Manobo languages; mno
Manx; glv
Maori; mao/mri
Marathi; mar
Mari; chm
Marshallese; mah
Marwari; mwr
Masai; mas
Mayan languages; myn
Mende; men
Micmac; mic
Minangkabau; min
Miscellaneous languages; mis
Mohawk; moh
Moldavian; mol
Mon-Khmer (Other); mkh
Mongo; lol
Mongolian; mon
Mossi; mos
Multiple languages; mul
Munda languages; mun

Nahuatl; nah
Nauru; nau
Navaho; nav
Navajo; nav
Ndebele, North; nde
Ndebele, South; nbl
Ndonga; ndo
Nepali; nep
Newari; new
Nias; nia
Niger-Kordofanian (Other); nic
Nilo-Saharan (Other); ssa
Niuean; niu
Norse, Old; non
North American Indian (Other); nai
Northern Sami; sme
North Ndebele; nde
Norwegian; nor
Norwegian Bokmal; nob
Norwegian Nynorsk; nno
Nubian languages; nub
Nyamwezi; nym
Nyanja; nya
Nyankole; nyn
Nynorsk, Norwegian; nno
Nyoro; nyo
Nzima; nzi

Occitan (post 1500); oci
Ojibwa; oji
Old Bulgarian; chu
Old Church Slavonic; chu
Old Slavonic; chu
Oriya; ori
Oromo; orm
Osage; osa
Ossetian; oss
Ossetic; oss
Otomian languages; oto

Pahlavi; pal
Palauan; pau
Pali; pli
Pampanga; pam
Pangasinan; pag
Panjabi; pan
Papiamento; pap
Papuan (Other); paa
Persian; per/fas
Persian, Old (ca.600-400); peo
Philippine (Other); phi
Phoenician; phn
Pohnpeian; pon
Polish; pol
Portuguese; por
Prakrit languages; pra
Provençal; oci
Provençal, Old (to 1500); pro
Pushto; pus

Quechua; que

Raeto-Romance; roh
Rajasthani; raj
Rapanui; rap
Rarotongan; rar
Reserved for local user; qaa-qtz
Romance (Other); roa
Romanian; rum/ron
Romany; rom
Rundi; run
Russian; rus

Salishan languages; sal
Samaritan Aramaic; sam
Sami languages (Other); smi
Samoan; smo
Sandawe; sad
Sango; sag
Sanskrit; san
Santali; sat
Sardinian; srd
Sasak; sas
Saxon, Low; nds
Scots; sco
Scottish Gaelic; gla
Selkup; sel
Semitic (Other); sem
Serbian; scc/srp
Serer; srr
Shan; shn
Shona; sna
Sidamo; sid
Sign languages; sgn
Siksika; bla
Sindhi; snd
Sinhalese; sin
Sino-Tibetan (Other); sit
Siouan languages; sio
Skolt Sami; sms
Slave (Athapascan); den
Slavic (Other); sla
Slovak; slo/slk
Slovenian; slv
Sogdian; sog
Somali; som
Songhai; son
Soninke; snk
Sorbian languages; wen
Sotho, Northern; nso
Sotho, Southern; sot
South American Indian (Other); sai
Southern Sami; sma
South Ndebele; nbl
Spanish; spa
Sukuma; suk
Sumerian; sux
Sundanese; sun
Susu; sus
Swahili; swa
Swati; ssw
Swedish; swe
Syriac; syr

Tagalog; tgl
Tahitian; tah
Tai (Other); tai
Tajik; tgk
Tamashek; tmh
Tamil; tam
Tatar; tat
Telugu; tel
Tereno; ter
Tetum; tet
Thai; tha
Tibetan; tib/bod
Tigre; tig
Tigrinya; tir
Timne; tem
Tiv; tiv
Tlingit; tli
Tok Pisin; tpi
Tokelau; tkl
Tonga (Nyasa); tog
Tonga (Tonga Islands); ton
Tsimshian; tsi
Tsonga; tso
Tswana; tsn
Tumbuka; tum
Tupi languages; tup
Turkish; tur
Turkish, Ottoman (1500-1928); ota
Turkmen; tuk
Tuvalu; tvl
Tuvinian; tyv
Twi; twi

Ugaritic; uga
Uighur; uig
Ukrainian; ukr
Umbundu; umb
Undetermined; und
Urdu; urd
Uzbek; uzb

Vai; vai
Venda; ven
Vietnamese; vie
Volapük; vol
Votic; vot

Wakashan languages; wak
Walamo; wal
Walloon; wln
Waray; war
Washo; was
Welsh; wel/cym
Wolof; wol

Xhosa; xho

Yakut; sah
Yao; yao
Yapese; yap
Yiddish; yid
Yoruba; yor
Yupik languages; ypk

Zande; znd
Zapotec; zap
Zenaga; zen
Zhuang; zha
Zulu; zul
Zuni;j zun
*/
