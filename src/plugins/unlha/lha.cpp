// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*****************************************************************************************\
**                                                                                       **
**   LHA.CPP - LHA unpacker                                                              **
**                                                                                       **
**   Popis funkci viz LHA.H                                                              **
**                                                                                       **
**   Sestaveno ze zdrojovych kodu 'LHA for UNIX, v.1.14'                                 **
**                                                                                       **
\*****************************************************************************************/

#include "precomp.h"

#include "lha.h"
#include "unlha.h"
#include "unlha.rh"
#include "unlha.rh2"
#include "lang\lang.rh"

#pragma runtime_checks("c", off)

int iLHAErrorStrId;

/*****************************************************************************************\
**                                                                                       **
**                                                                                       **
**   HEADER CODE                                                                         **
**                                                                                       **
**                                                                                       **
\*****************************************************************************************/

static char* get_ptr;

#define I_HEADER_SIZE 0
#define I_HEADER_CHECKSUM 1
#define I_METHOD 2
#define I_PACKED_SIZE 7
#define I_ORIGINAL_SIZE 11
#define I_LAST_MODIFIED_STAMP 15
#define I_ATTRIBUTE 19
#define I_HEADER_LEVEL 20
#define I_NAME_LENGTH 21
#define I_NAME 22

#define EXTEND_GENERIC 0
#define EXTEND_UNIX 'U'
#define EXTEND_MSDOS 'M'
#define EXTEND_MACOS 'm'
#define EXTEND_OS9 '9'
#define EXTEND_OS2 '2'
#define EXTEND_OS68K 'K'
#define EXTEND_OS386 '3' /* OS-9000??? */
#define EXTEND_HUMAN 'H'
#define EXTEND_CPM 'C'
#define EXTEND_FLEX 'F'
#define EXTEND_RUNSER 'R'

#define METHOD_TYPE_STRAGE 5
#define LZHEADER_STRAGE 4096
#define FILENAME_LENGTH 1024

static const char* lha_methods[] =
    {
        "-lh0-", "-lh1-", "-lh2-", "-lh3-", "-lh4-", "-lh5-",
        "-lh6-", "-lh7-", "-lzs-", "-lz5-", "-lz4-", "-lhd-", NULL};

#define UNIX_RW_RW_RW 0000666

#define setup_get(PTR) (get_ptr = (PTR))
#define get_byte() (*get_ptr++ & 0xff)
#define put_ptr get_ptr
#define setup_put(PTR) (put_ptr = (PTR))
#define put_byte(c) (*put_ptr++ = (char)(c))

static int calc_sum(char* p, int len)
{
    int sum;

    for (sum = 0; len; len--)
        sum += *p++;

    return sum & 0xff;
}

static unsigned short get_word()
{
    int b0, b1;

    b0 = get_byte();
    b1 = get_byte();
    return (b1 << 8) + b0;
}

/*static void put_word(unsigned int v)
{
  put_byte(v);
  put_byte(v >> 8);
}*/

static long get_longword()
{
    long b0, b1, b2, b3;

    b0 = get_byte();
    b1 = get_byte();
    b2 = get_byte();
    b3 = get_byte();
    return (b3 << 24) + (b2 << 16) + (b1 << 8) + b0;
}

static unsigned __int64 get_qword()
{
    unsigned __int64 ret = get_longword();

    return ret | ((unsigned __int64)get_longword() << 32);
}

/*static void put_longword(long v)
{
  put_byte(v);
  put_byte(v >> 8);
  put_byte(v >> 16);
  put_byte(v >> 24);
}*/

static void generic_to_unix_filename(char* name, int len)
{
    int i;

    for (i = 0; i < len; i++)
        if (name[i] == '/')
            name[i] = '\\';
}

static void macos_to_unix_filename(char* name, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (name[i] == ':')
            name[i] = '\\';
        else if (name[i] == '/')
            name[i] = ':';
    }
}

static char* convdelim(char* path, char delim)
{
    char c;
    char* p;

    for (p = path; (c = *p) != 0; p++)
        if (c == '\\' || c == '/' || c == (char)0xff)
        {
            *p = delim;
            path = p + 1;
        }
    return path;
}

int LHAGetHeader(FILE* fp, LHA_HEADER* lpHeader)
{
    CALL_STACK_MESSAGE1("LHAGetHeader( , )");

    int header_size;
    int name_length;
    char data[LZHEADER_STRAGE];
    char dirname[FILENAME_LENGTH];
    int dir_length = 0;
    int checksum;
    int i;
    char* ptr;
    int extend_size;
    int dmy;
    char method[METHOD_TYPE_STRAGE];

    ZeroMemory(lpHeader, sizeof(LHA_HEADER));

    if (((header_size = fgetc(fp)) == EOF) || (header_size == 0))
    {
        iLHAErrorStrId = IDS_FILECORRUPT;
        return GH_EOF;
    }

    if ((int)fread(data + I_HEADER_CHECKSUM, sizeof(char), header_size - 1, fp) < header_size - 1)
    {
        iLHAErrorStrId = IDS_FILECORRUPT;
        return GH_ERROR;
    }

    setup_get(data + I_HEADER_LEVEL);
    lpHeader->header_level = get_byte();
    if (lpHeader->header_level != 2 && fread(data + header_size, sizeof(char), 2, fp) < 2)
    {
        iLHAErrorStrId = IDS_FILECORRUPT;
        return GH_ERROR;
    }

    if (lpHeader->header_level >= 3)
    {
        iLHAErrorStrId = IDS_FILECORRUPT;
        return GH_ERROR;
    }

    setup_get(data + I_HEADER_CHECKSUM);
    checksum = get_byte();

    if (lpHeader->header_level >= 2)
    {
        lpHeader->header_size = header_size + checksum * 256;
    }
    else
    {
        lpHeader->header_size = header_size;
    }
    memcpy(method, data + I_METHOD, METHOD_TYPE_STRAGE);
    setup_get(data + I_PACKED_SIZE);
    lpHeader->packed_size = get_longword();
    lpHeader->original_size = get_longword();
    long stamp = get_longword();
    lpHeader->last_modified_stamp = *((LHA_TIMESTAMP*)&stamp);
    lpHeader->attribute = get_byte();

    if ((lpHeader->header_level = get_byte()) != 2)
    {
        if (calc_sum(data + I_METHOD, header_size) != checksum)
        {
            iLHAErrorStrId = IDS_CHECKSUMERROR;
            return GH_ERROR;
        }
        name_length = get_byte();
        for (i = 0; i < name_length; i++)
            lpHeader->name[i] = (char)get_byte();
        lpHeader->name[name_length] = '\0';
    }
    else
    {
        name_length = 0;
    }

    lpHeader->unix_mode = UNIX_FILE_REGULAR | UNIX_RW_RW_RW;

    if (lpHeader->header_level == 0)
    {
        extend_size = header_size - name_length - 22;
        if (extend_size < 0)
        {
            if (extend_size == -2)
            {
                lpHeader->extend_type = EXTEND_GENERIC;
                lpHeader->has_crc = FALSE;
            }
            else
            {
                iLHAErrorStrId = IDS_FILECORRUPT;
                return FALSE;
            }
        }
        else
        {
            lpHeader->has_crc = TRUE;
            lpHeader->crc = get_word();
        }

        if (extend_size >= 1)
        {
            lpHeader->extend_type = get_byte();
            extend_size--;
        }
        if (lpHeader->extend_type == EXTEND_UNIX)
        {
            if (extend_size >= 11)
            {
                lpHeader->minor_version = get_byte();
                get_longword();
                lpHeader->unix_mode = get_word();
                get_word();
                get_word();
                extend_size -= 11;
            }
            else
            {
                lpHeader->extend_type = EXTEND_GENERIC;
            }
        }
        while (extend_size-- > 0)
            dmy = get_byte();
        if (lpHeader->extend_type == EXTEND_UNIX)
            return TRUE;
    }
    else if (lpHeader->header_level == 1)
    {
        lpHeader->has_crc = TRUE;
        extend_size = header_size - name_length - 25;
        lpHeader->crc = get_word();
        lpHeader->extend_type = get_byte();
        while (extend_size-- > 0)
            dmy = get_byte();
    }
    else
    { /* level 2 */
        lpHeader->has_crc = TRUE;
        lpHeader->crc = get_word();
        lpHeader->extend_type = get_byte();
    }

    //  int x = sizeof(time_t);
    //  _ASSERT(sizeof(time_t) == 4);
    _ASSERT(sizeof(unsigned __int64) == sizeof(FILETIME));

    SYSTEMTIME st;
    st.wMilliseconds = st.wDayOfWeek = 0;
    if (lpHeader->header_level < 2)
    {
        st.wDay = lpHeader->last_modified_stamp.day;
        st.wHour = lpHeader->last_modified_stamp.hour;
        st.wMinute = lpHeader->last_modified_stamp.minute;
        st.wMonth = lpHeader->last_modified_stamp.month;
        st.wSecond = lpHeader->last_modified_stamp.second2 * 2;
        st.wYear = lpHeader->last_modified_stamp.year + 1980;
    }
    else
    {
        // This is the count of the elapsed seconds from 00:00:00 on Jan 1, 1970 UTC to the last modified time of file.
        struct tm* tajm = _gmtime32((__time32_t*)&lpHeader->last_modified_stamp);

        st.wDay = tajm->tm_wday;
        st.wHour = tajm->tm_hour;
        st.wMinute = tajm->tm_min;
        st.wMonth = tajm->tm_mon;
        st.wSecond = tajm->tm_sec;
        st.wYear = tajm->tm_year + 1900;
    }
    if (st.wDay == 0 && st.wHour == 0 && st.wMinute == 0 && st.wMonth == 0 &&
        st.wSecond == 0 && st.wYear == 1980) // nezobrazitelne datum LHA ulozi jako
    {                                        // same nuly -> nastavime minimum = 1.1.1980
        st.wDay = 1;
        st.wMonth = 1;
    }
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    LocalFileTimeToFileTime(&ft, &lpHeader->last_modified_filetime);

    if (lpHeader->header_level > 0)
    {
        /* Extend Header */
        if (lpHeader->header_level == 1)
            setup_get(data + lpHeader->header_size);
        ptr = get_ptr;
        while ((header_size = get_word()) != 0)
        {
            if (header_size > 500 ||
                (lpHeader->header_level != 2 &&
                 ((data + LZHEADER_STRAGE - get_ptr < header_size) ||
                  (int)fread(get_ptr, sizeof(char), header_size, fp) < header_size)))
            {
                iLHAErrorStrId = IDS_FILECORRUPT;
                return FALSE;
            }

            switch (get_byte())
            {
            case 1:
                /*
           * filename
           */
                for (i = 0; i < header_size - 3; i++)
                    lpHeader->name[i] = (char)get_byte();
                lpHeader->name[header_size - 3] = '\0';
                name_length = header_size - 3;
                break;
            case 2:
                /*
           * directory
           */
                for (i = 0; i < header_size - 3; i++)
                    dirname[i] = (char)get_byte();
                dirname[header_size - 3] = '\0';
                convdelim(dirname, '\\');
                dir_length = header_size - 3;
                break;
            case 0x40:
                /*
           * MS-DOS attribute
           */
                if (lpHeader->extend_type == EXTEND_MSDOS ||
                    lpHeader->extend_type == EXTEND_HUMAN ||
                    lpHeader->extend_type == EXTEND_GENERIC)
                    lpHeader->attribute = (unsigned char)get_word();
                break;
            case 0x41:
                /*
           * Windows time stamp header
           */
                if (lpHeader->extend_type == EXTEND_MSDOS)
                {
                    get_qword(); // Skip Creation Time
                    unsigned __int64 lastWwrite = get_qword();
                    LocalFileTimeToFileTime((FILETIME*)&lastWwrite, &lpHeader->last_modified_filetime);
                    get_qword(); // Skip Last Access Time
                }
                break;
            case 0x50:
                /*
           * UNIX permission
           */
                if (lpHeader->extend_type == EXTEND_UNIX)
                    lpHeader->unix_mode = get_word();
                break;
            default:
                /*
           * other headers
           */
                setup_get(get_ptr + header_size - 3);
                break;
            }
        }
        if (lpHeader->header_level == 1 && get_ptr - ptr != 2)
        {
            lpHeader->packed_size -= (long)(get_ptr - ptr - 2);
            lpHeader->header_size += (unsigned char)(get_ptr - ptr - 2);
        }
    }

    switch (lpHeader->extend_type)
    {
    case EXTEND_OS68K:
    case EXTEND_UNIX:
        break;
    case EXTEND_MACOS:
        macos_to_unix_filename(lpHeader->name, name_length);
        /* macos_to_unix_filename(dirname, dir_length); */ // ????
        break;
    default:
        generic_to_unix_filename(lpHeader->name, name_length);
        generic_to_unix_filename(dirname, dir_length);
    }

    if (dir_length)
    {
        strcat(dirname, lpHeader->name);
        strcpy(lpHeader->name, dirname);
        name_length += dir_length;
    }

    OemToChar(lpHeader->name, lpHeader->name); // češtiňka ... :-)

    for (i = 0;; i++)
        if (lha_methods[i] == NULL)
        {
            i = LHA_UNKNOWNMETHOD;
            break;
        }
        else if (memcmp(method, lha_methods[i], 5) == 0)
            break;
    lpHeader->method = i;

    return GH_SUCCESS;
}

/*****************************************************************************************\
**                                                                                       **
**                                                                                       **
**   UNPACKER CODE                                                                       **
**                                                                                       **
**                                                                                       **
\*****************************************************************************************/

// PROGRESS ///////////////////////////////////////////////////////////////////////////

BOOL(*pfLHAProgress)
(int size) = NULL;
static int iProgress;
#define PROGRESS_MASK 0xffff8000 // pozice progress-meteru se updatuje kazdych 32KB

////////////////////////////////////////////////////////////////////////////////////////
// BUFFERED OUTPUT /////////////////////////////////////////////////////////////////////

static char* outputFileName;

BOOL SafeWriteFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten)
{
    while (!WriteFile(hFile, lpBuffer, nBytesToWrite, pnBytesWritten, NULL))
    {
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        if (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, outputFileName, error,
                                           LoadStr(IDS_WRITEERROR)) != DIALOG_RETRY)
            return FALSE;
    }
    return TRUE;
}

#define BUFSIZE (256 * 1024) // velikost bufferu pro zapis

static char* buffer;
static int bufpos;

static int buffer_init()
{
    buffer = (char*)malloc(BUFSIZE);
    if (buffer == NULL)
    {
        iLHAErrorStrId = IDS_LOWMEM;
        return 0;
    }
    bufpos = 0;
    return 1;
}

static int buffer_done(HANDLE hFile)
{
    int ret = 1;
    if (bufpos)
    {
        DWORD numw;
        if (!SafeWriteFile(hFile, buffer, bufpos, &numw))
        {
            iLHAErrorStrId = IDS_WRITEERROR;
            ret = 0;
        }
    }
    free(buffer);
    return ret;
}

static int my_fwrite(const void* b, int /* size == 1 */, int count, HANDLE hFile)
{
    iProgress += count;
    if ((iProgress & PROGRESS_MASK) != ((iProgress - count) & PROGRESS_MASK))
        if (pfLHAProgress != NULL)
            if (!pfLHAProgress(iProgress))
            {
                iLHAErrorStrId = -1;
                return 0;
            }

    if (bufpos + count <= BUFSIZE)
    {
        memcpy(buffer + bufpos, b, count);
        bufpos += count;
        return count;
    }

    int n = BUFSIZE - bufpos;
    if (n > 0)
        memcpy(buffer + bufpos, b, n);
    DWORD numw;
    if (!SafeWriteFile(hFile, buffer, BUFSIZE, &numw))
    {
        iLHAErrorStrId = IDS_WRITEERROR;
        return 0;
    }
    memcpy(buffer, (char*)b + n, bufpos = (count - n));
    return count;
}

////////////////////////////////////////////////////////////////////////////////////////
// LHA_MACRO.H /////////////////////////////////////////////////////////////////////////

typedef short node;

#define CHAR_BIT 8

#define CRCPOLY 0xA001 /* CRC-16 */
#define UPDATE_CRC(c) crc = crctable[(crc ^ (c)) & 0xFF] ^ (crc >> CHAR_BIT)

#define N_CHAR (256 + 60 - THRESHOLD + 1)
#define TREESIZE_C (N_CHAR * 2)
#define TREESIZE_P (128 * 2)
#define TREESIZE (TREESIZE_C + TREESIZE_P)
#define ROOT_C 0
#define ROOT_P TREESIZE_C

#define NP (MAX_DICBIT + 1)
#define NT (USHRT_BIT + 3)

#define PBIT 5 /* smallest integer such that (1 << PBIT) > * NP */
#define TBIT 5 /* smallest integer such that (1 << TBIT) > * NT */

#define NC (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD)

#define NPT 0x80

#define MAGIC0 18
#define MAGIC5 19

#define N1 286          /* alphabet size */
#define N2 (2 * N1 - 1) /* # of nodes in Huffman tree */
#define EXTRABITS 8     /* >= log2(F-THRESHOLD+258-N1) */
#define BUFBITS 16      /* >= log2(MAXBUF) */
#define LENFIELD 4      /* bit size of length field for tree output */

#define MAX_DICBIT 16 /* lh7 use 16bits */

#define MAX_DICSIZ (1 << MAX_DICBIT)
#define MATCHBIT 8   /* bits for MAXMATCH - THRESHOLD */
#define MAXMATCH 256 /* formerly F (not more than UCHAR_MAX + 1) */
#define THRESHOLD 3  /* choose optimal value */

#define CBIT 9       /* $\lfloor \log_2 NC \rfloor + 1$ */
#define USHRT_BIT 16 /* (CHAR_BIT * sizeof(ushort)) */

////////////////////////////////////////////////////////////////////////////////////////
// LHA.H ///////////////////////////////////////////////////////////////////////////////

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;

struct decode_option
{
    unsigned short (*decode_c)();
    unsigned short (*decode_p)();
    void (*decode_start)();
};

struct interfacing
{
    FILE* infile;
    HANDLE outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
};

static struct interfacing iface;

static unsigned long origsize, compsize;
static unsigned short dicbit;
static unsigned short maxmatch;
static unsigned long count;
static unsigned long loc;
static unsigned char* text;
static int prev_char;

static FILE* infile;
static HANDLE outfile;
static unsigned short crc, bitbuf;
static long reading_size;

static unsigned int n_max;

static void output_st1(int);
static uchar* alloc_buf();
static ushort decode_c_st1();
static ushort decode_p_st1();
static void decode_start_st1();

static void decode_start_st0();
static void decode_start_fix();
static ushort decode_c_st0();
static ushort decode_p_st0();

static void start_c_dyn();
static void decode_start_dyn();
static ushort decode_c_dyn();
static ushort decode_p_dyn();
static void output_dyn(int code, unsigned int pos);

static ushort decode_c_lzs();
static ushort decode_p_lzs();
static ushort decode_c_lz5();
static ushort decode_p_lz5();
static void decode_start_lzs();
static void decode_start_lz5();

static void make_table(short nchar, uchar bitlen[], short tablebits, ushort table[]);

static void make_crctable();
static ushort calccrc(uchar* p, uint n);
static void fillbuf(uchar n);
static ushort getbits(uchar n);
static int fwrite_crc(uchar* p, int n, HANDLE f);
static void init_getbits();
static void make_crctable();

////////////////////////////////////////////////////////////////////////////////////////
// LARC.C //////////////////////////////////////////////////////////////////////////////

static int flag, flagcnt, matchpos_1;

static unsigned short decode_c_lzs()
{
    if (getbits(1))
    {
        return getbits(8);
    }
    else
    {
        matchpos_1 = getbits(11);
        return getbits(4) + 0x100;
    }
}

static unsigned short decode_p_lzs()
{
    return (ushort)((loc - matchpos_1 - MAGIC0) & 0x7ff);
}

static void decode_start_lzs()
{
    init_getbits();
}

static unsigned short decode_c_lz5()
{
    int c;

    if (flagcnt == 0)
    {
        flagcnt = 8;
        flag = fgetc(infile);
    }
    flagcnt--;
    c = fgetc(infile);
    if ((flag & 1) == 0)
    {
        matchpos_1 = c;
        c = fgetc(infile);
        matchpos_1 += (c & 0xf0) << 4;
        c &= 0x0f;
        c += 0x100;
    }
    flag >>= 1;
    return c;
}

static unsigned short decode_p_lz5()
{
    return (ushort)((loc - matchpos_1 - MAGIC5) & 0xfff);
}

static void decode_start_lz5()
{
    int i;

    flagcnt = 0;
    for (i = 0; i < 256; i++)
        memset(&text[i * 13 + 18], i, 13);
    for (i = 0; i < 256; i++)
        text[256 * 13 + 18 + i] = i;
    for (i = 0; i < 256; i++)
        text[256 * 13 + 256 + 18 + i] = 255 - i;
    memset(&text[256 * 13 + 512 + 18], 0, 128);
    memset(&text[256 * 13 + 512 + 128 + 18], ' ', 128 - 18);
}

////////////////////////////////////////////////////////////////////////////////////////
// HUF.C ///////////////////////////////////////////////////////////////////////////////

static unsigned short left[2 * NC - 1], right[2 * NC - 1];
static unsigned char c_len[NC], pt_len[NPT];
static unsigned short c_freq[2 * NC - 1], c_table[4096], c_code[NC], p_freq[2 * NP - 1],
    pt_table[256], pt_code[NPT], t_freq[2 * NT - 1];

static unsigned char* buf;
static unsigned int bufsiz;
static unsigned short blocksize;
static unsigned short output_pos, output_mask;
static int pbit;
static int np_1;

static void read_pt_len(short nn, short nbit, short i_special)
{
    int i, c, n;

    n = getbits((uchar)nbit);
    if (n == 0)
    {
        c = getbits((uchar)nbit);
        for (i = 0; i < nn; i++)
            pt_len[i] = 0;
        for (i = 0; i < 256; i++)
            pt_table[i] = c;
    }
    else
    {
        i = 0;
        while (i < n)
        {
            c = bitbuf >> (16 - 3);
            if (c == 7)
            {
                unsigned short mask = 1 << (16 - 4);
                while (mask & bitbuf)
                {
                    mask >>= 1;
                    c++;
                }
            }
            fillbuf((c < 7) ? 3 : c - 3);
            pt_len[i++] = c;
            if (i == i_special)
            {
                c = getbits(2);
                while (--c >= 0)
                    pt_len[i++] = 0;
            }
        }
        while (i < nn)
            pt_len[i++] = 0;
        make_table(nn, pt_len, 8, pt_table);
    }
}

static void read_c_len()
{
    short i, c, n;

    n = getbits(CBIT);
    if (n == 0)
    {
        c = getbits(CBIT);
        for (i = 0; i < NC; i++)
            c_len[i] = 0;
        for (i = 0; i < 4096; i++)
            c_table[i] = c;
    }
    else
    {
        i = 0;
        while (i < n)
        {
            c = pt_table[bitbuf >> (16 - 8)];
            if (c >= NT)
            {
                unsigned short mask = 1 << (16 - 9);
                do
                {
                    if (bitbuf & mask)
                        c = right[c];
                    else
                        c = left[c];
                    mask >>= 1;
                } while (c >= NT);
            }
            fillbuf(pt_len[c]);
            if (c <= 2)
            {
                if (c == 0)
                    c = 1;
                else if (c == 1)
                    c = getbits(4) + 3;
                else
                    c = getbits(CBIT) + 20;
                while (--c >= 0)
                    c_len[i++] = 0;
            }
            else
                c_len[i++] = c - 2;
        }
        while (i < NC)
            c_len[i++] = 0;
        make_table(NC, c_len, 12, c_table);
    }
}

static unsigned short decode_c_st1()
{
    unsigned short j, mask;

    if (blocksize == 0)
    {
        blocksize = getbits(16);
        read_pt_len(NT, TBIT, 3);
        read_c_len();
        read_pt_len(np_1, pbit, -1);
    }
    blocksize--;
    j = c_table[bitbuf >> 4];
    if (j < NC)
        fillbuf(c_len[j]);
    else
    {
        fillbuf(12);
        mask = 1 << (16 - 1);
        do
        {
            if (bitbuf & mask)
                j = right[j];
            else
                j = left[j];
            mask >>= 1;
        } while (j >= NC);
        fillbuf(c_len[j] - 12);
    }
    return j;
}

static unsigned short decode_p_st1()
{
    unsigned short j, mask;

    j = pt_table[bitbuf >> (16 - 8)];
    if (j < np_1)
        fillbuf(pt_len[j]);
    else
    {
        fillbuf(8);
        mask = 1 << (16 - 1);
        do
        {
            if (bitbuf & mask)
                j = right[j];
            else
                j = left[j];
            mask >>= 1;
        } while (j >= np_1);
        fillbuf(pt_len[j] - 8);
    }
    if (j != 0)
        j = (1 << (j - 1)) + getbits(j - 1);
    return j;
}

static void decode_start_st1()
{
    if (dicbit <= 13)
    {
        np_1 = 14;
        pbit = 4;
    }
    else
    {
        if (dicbit == 16)
        {
            np_1 = 17; /* for -lh7- */
        }
        else
        {
            np_1 = 16;
        }
        pbit = 5;
    }

    init_getbits();
    blocksize = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// MAKETABLE.C /////////////////////////////////////////////////////////////////////////

static void make_table(short nchar, uchar bitlen[], short tablebits, ushort table[])
{
    unsigned short count2[17]; /* count of bitlen */
    unsigned short weight[17]; /* 0x10000ul >> bitlen */
    unsigned short start[17];  /* first code of bitlen */
    unsigned short total;
    unsigned int i, l;
    int j, k, m, n, avail;
    unsigned short* p;

    avail = nchar;

    /* initialize */
    for (i = 1; i <= 16; i++)
    {
        count2[i] = 0;
        weight[i] = 1 << (16 - i);
    }

    /* count2 */
    for (i = 0; (int)i < nchar; i++)
        count2[bitlen[i]]++;

    /* calculate first code */
    total = 0;
    for (i = 1; i <= 16; i++)
    {
        start[i] = total;
        total += weight[i] * count2[i];
    }
    /*if ((total & 0xffff) != 0)
    error("make_table()", "Bad table (5)\n");*/
    //FIX

    /* shift data for make table. */
    m = 16 - tablebits;
    for (i = 1; (int)i <= tablebits; i++)
    {
        start[i] >>= m;
        weight[i] >>= m;
    }

    /* initialize */
    j = start[tablebits + 1] >> m;
    k = 1 << tablebits;
    if (j != 0)
        for (i = j; (int)i < k; i++)
            table[i] = 0;

    /* create table and tree */
    for (j = 0; j < nchar; j++)
    {
        k = bitlen[j];
        if (k == 0)
            continue;
        l = start[k] + weight[k];
        if (k <= tablebits)
        {
            /* code in table */
            for (i = start[k]; i < l; i++)
                table[i] = j;
        }
        else
        {
            /* code not in table */
            p = &table[(i = start[k]) >> m];
            i <<= tablebits;
            n = k - tablebits;
            /* make tree (n length) */
            while (--n >= 0)
            {
                if (*p == 0)
                {
                    right[avail] = left[avail] = 0;
                    *p = avail++;
                }
                if (i & 0x8000)
                    p = &right[*p];
                else
                    p = &left[*p];
                i <<= 1;
            }
            *p = j;
        }
        start[k] = l;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// SHUF.C //////////////////////////////////////////////////////////////////////////////

#undef NP
#undef NP2

#define NP (8 * 1024 / 64)
#define NP2 (NP * 2 - 1)

static unsigned int np;

static int fixed[2][16] = {
    {3, 0x01, 0x04, 0x0c, 0x18, 0x30, 0},            /* old compatible */
    {2, 0x01, 0x01, 0x03, 0x06, 0x0D, 0x1F, 0x4E, 0} /* 8K buf */
};

static void decode_start_st0()
{
    n_max = 286;
    maxmatch = MAXMATCH;
    init_getbits();
    np = 1 << (MAX_DICBIT - 7);
}

static void ready_made(int method)
{
    int i, j;
    unsigned int code, weight;
    int* tbl;

    tbl = fixed[method];
    j = *tbl++;
    weight = 1 << (16 - j);
    code = 0;
    for (i = 0; i < (int)np; i++)
    {
        while (*tbl == i)
        {
            j++;
            tbl++;
            weight >>= 1;
        }
        pt_len[i] = j;
        pt_code[i] = code;
        code += weight;
    }
}

static void read_tree_c()
{
    int i, c;

    i = 0;
    while (i < N1)
    {
        if (getbits(1))
            c_len[i] = getbits(LENFIELD) + 1;
        else
            c_len[i] = 0;
        if (++i == 3 && c_len[0] == 1 && c_len[1] == 1 && c_len[2] == 1)
        {
            c = getbits(CBIT);
            for (i = 0; i < N1; i++)
                c_len[i] = 0;
            for (i = 0; i < 4096; i++)
                c_table[i] = c;
            return;
        }
    }
    make_table(N1, c_len, 12, c_table);
}

static void read_tree_p()
{
    int i, c;

    i = 0;
    while (i < NP)
    {
        pt_len[i] = (unsigned char)getbits(LENFIELD);
        if (++i == 3 && pt_len[0] == 1 && pt_len[1] == 1 && pt_len[2] == 1)
        {
            c = getbits(MAX_DICBIT - 7);
            for (i = 0; i < NP; i++)
                c_len[i] = 0;
            for (i = 0; i < 256; i++)
                c_table[i] = c;
            return;
        }
    }
}

static void decode_start_fix()
{
    n_max = 314;
    maxmatch = 60;
    init_getbits();
    np = 1 << (12 - 6);
    start_c_dyn();
    ready_made(0);
    make_table(np, pt_len, 8, pt_table);
}

static unsigned short decode_c_st0()
{
    int i, j;
    static unsigned short blocksize2 = 0;

    if (blocksize2 == 0)
    {
        blocksize2 = getbits(BUFBITS);
        read_tree_c();
        if (getbits(1))
        {
            read_tree_p();
        }
        else
        {
            ready_made(1);
        }
        make_table(NP, pt_len, 8, pt_table);
    }
    blocksize2--;
    j = c_table[bitbuf >> 4];
    if (j < N1)
        fillbuf(c_len[j]);
    else
    {
        fillbuf(12);
        i = bitbuf;
        do
        {
            if ((short)i < 0)
                j = right[j];
            else
                j = left[j];
            i <<= 1;
        } while (j >= N1);
        fillbuf(c_len[j] - 12);
    }
    if (j == N1 - 1)
        j += getbits(EXTRABITS);
    return j;
}

static unsigned short decode_p_st0()
{
    int i, j;

    j = pt_table[bitbuf >> 8];
    if (j < (int)np)
    {
        fillbuf(pt_len[j]);
    }
    else
    {
        fillbuf(8);
        i = bitbuf;
        do
        {
            if ((short)i < 0)
                j = right[j];
            else
                j = left[j];
            i <<= 1;
        } while (j >= (int)np);
        fillbuf(pt_len[j] - 8);
    }
    return (j << 6) + getbits(6);
}

////////////////////////////////////////////////////////////////////////////////////////
// DHUF.C //////////////////////////////////////////////////////////////////////////////

static short child[TREESIZE], parent[TREESIZE], block[TREESIZE], edge[TREESIZE],
    stock[TREESIZE], s_node[TREESIZE / 2];

static unsigned short freq[TREESIZE];
static unsigned short total_p;
static int avail, n1;
static int most_p, nn;
static unsigned long nextcount;

static void start_c_dyn()
{
    int i, j, f;

    n1 = ((int)n_max >= 256 + maxmatch - THRESHOLD + 1) ? 512 : n_max - 1;
    for (i = 0; i < TREESIZE_C; i++)
    {
        stock[i] = i;
        block[i] = 0;
    }
    for (i = 0, j = n_max * 2 - 2; i < (int)n_max; i++, j--)
    {
        freq[j] = 1;
        child[j] = ~i;
        s_node[i] = j;
        block[j] = 1;
    }
    avail = 2;
    edge[1] = n_max - 1;
    i = n_max * 2 - 2;
    while (j >= 0)
    {
        f = freq[j] = freq[i] + freq[i - 1];
        child[j] = i;
        parent[i] = parent[i - 1] = j;
        if (f == freq[j + 1])
        {
            edge[block[j] = block[j + 1]] = j;
        }
        else
        {
            edge[block[j] = stock[avail++]] = j;
        }
        i -= 2;
        j--;
    }
}

static void start_p_dyn()
{
    freq[ROOT_P] = 1;
    child[ROOT_P] = ~(N_CHAR);
    s_node[N_CHAR] = ROOT_P;
    edge[block[ROOT_P] = stock[avail++]] = ROOT_P;
    most_p = ROOT_P;
    total_p = 0;
    nn = 1 << dicbit;
    nextcount = 64;
}

static void decode_start_dyn()
{
    n_max = 286;
    maxmatch = MAXMATCH;
    init_getbits();
    start_c_dyn();
    start_p_dyn();
}

static void reconst(int start, int end)
{
    int i, j, k, l, b;
    unsigned int f, g;

    for (i = j = start; i < end; i++)
    {
        if ((k = child[i]) < 0)
        {
            freq[j] = (freq[i] + 1) / 2;
            child[j] = k;
            j++;
        }
        if (edge[b = block[i]] == i)
        {
            stock[--avail] = b;
        }
    }
    j--;
    i = end - 1;
    l = end - 2;
    while (i >= start)
    {
        while (i >= l)
        {
            freq[i] = freq[j];
            child[i] = child[j];
            i--, j--;
        }
        f = freq[l] + freq[l + 1];
        for (k = start; f < freq[k]; k++)
            ;
        while (j >= k)
        {
            freq[i] = freq[j];
            child[i] = child[j];
            i--, j--;
        }
        freq[i] = f;
        child[i] = l + 1;
        i--;
        l -= 2;
    }
    f = 0;
    for (i = start; i < end; i++)
    {
        if ((j = child[i]) < 0)
            s_node[~j] = i;
        else
            parent[j] = parent[j - 1] = i;
        if ((g = freq[i]) == f)
        {
            block[i] = b;
        }
        else
        {
            edge[b = block[i] = stock[avail++]] = i;
            f = g;
        }
    }
}

static int swap_inc(int p)
{
    int b, q, r, s;

    b = block[p];
    if ((q = edge[b]) != p)
    { /* swap for leader */
        r = child[p];
        s = child[q];
        child[p] = s;
        child[q] = r;
        if (r >= 0)
            parent[r] = parent[r - 1] = q;
        else
            s_node[~r] = q;
        if (s >= 0)
            parent[s] = parent[s - 1] = p;
        else
            s_node[~s] = p;
        p = q;
        goto Adjust;
    }
    else if (b == block[p + 1])
    {
    Adjust:
        edge[b]++;
        if (++freq[p] == freq[p - 1])
        {
            block[p] = block[p - 1];
        }
        else
        {
            edge[block[p] = stock[avail++]] = p; /* create block */
        }
    }
    else if (++freq[p] == freq[p - 1])
    {
        stock[--avail] = b; /* delete block */
        block[p] = block[p - 1];
    }
    return parent[p];
}

static void update_c(int p)
{
    int q;

    if (freq[ROOT_C] == 0x8000)
    {
        reconst(0, n_max * 2 - 1);
    }
    freq[ROOT_C]++;
    q = s_node[p];
    do
    {
        q = swap_inc(q);
    } while (q != ROOT_C);
}

static void update_p(int p)
{
    int q;

    if (total_p == 0x8000)
    {
        reconst(ROOT_P, most_p + 1);
        total_p = freq[ROOT_P];
        freq[ROOT_P] = 0xffff;
    }
    q = s_node[p + N_CHAR];
    while (q != ROOT_P)
    {
        q = swap_inc(q);
    }
    total_p++;
}

static void make_new_node(int p)
{
    int q, r;

    r = most_p + 1;
    q = r + 1;
    s_node[~(child[r] = child[most_p])] = r;
    child[q] = ~(p + N_CHAR);
    child[most_p] = q;
    freq[r] = freq[most_p];
    freq[q] = 0;
    block[r] = block[most_p];
    if (most_p == ROOT_P)
    {
        freq[ROOT_P] = 0xffff;
        edge[block[ROOT_P]]++;
    }
    parent[r] = parent[q] = most_p;
    edge[block[q] = stock[avail++]] = s_node[p + N_CHAR] = most_p = q;
    update_p(p);
}

static unsigned short decode_c_dyn()
{
    int c;
    short buf2, cnt;

    c = child[ROOT_C];
    buf2 = bitbuf;
    cnt = 0;
    do
    {
        c = child[c - (buf2 < 0)];
        buf2 <<= 1;
        if (++cnt == 16)
        {
            fillbuf(16);
            buf2 = bitbuf;
            cnt = 0;
        }
    } while (c > 0);
    fillbuf((uchar)cnt);
    c = ~c;
    update_c(c);
    if (c == n1)
        c += getbits(8);
    return c;
}

static unsigned short decode_p_dyn()
{
    int c;
    short buf2, cnt;

    while (count > nextcount)
    {
        make_new_node(nextcount / 64);
        if ((int)(nextcount += 64) >= nn)
            nextcount = 0xffffffff;
    }
    c = child[ROOT_P];
    buf2 = bitbuf;
    cnt = 0;
    while (c > 0)
    {
        c = child[c - (buf2 < 0)];
        buf2 <<= 1;
        if (++cnt == 16)
        {
            fillbuf(16);
            buf2 = bitbuf;
            cnt = 0;
        }
    }
    fillbuf((uchar)cnt);
    c = (~c) - N_CHAR;
    update_p(c);

    return (c << 6) + getbits(6);
}

////////////////////////////////////////////////////////////////////////////////////////
// CRCIO.C /////////////////////////////////////////////////////////////////////////////

static unsigned short crctable[UCHAR_MAX + 1];
static unsigned char subbitbuf, bitcount;

static void make_crctable()
{
    unsigned int i, j, r;

    for (i = 0; i <= UCHAR_MAX; i++)
    {
        r = i;
        for (j = 0; j < CHAR_BIT; j++)
            if (r & 1)
                r = (r >> 1) ^ CRCPOLY;
            else
                r >>= 1;
        crctable[i] = r;
    }
}

static unsigned short calccrc(unsigned char* p, unsigned int n)
{
    reading_size += n;
    while (n-- > 0)
        UPDATE_CRC(*p++);
    return crc;
}

static void fillbuf(uchar n)
{
    while (n > bitcount)
    {
        n -= bitcount;
        bitbuf = (bitbuf << bitcount) + (subbitbuf >> (CHAR_BIT - bitcount));
        if (compsize != 0)
        {
            compsize--;
            subbitbuf = (unsigned char)fgetc(infile);
        }
        else
            subbitbuf = 0;
        bitcount = CHAR_BIT;
    }
    bitcount -= n;
    bitbuf = (bitbuf << n) + (subbitbuf >> (CHAR_BIT - n));
    subbitbuf <<= n;
}

static unsigned short getbits(uchar n)
{
    unsigned short x;
    x = bitbuf >> (2 * CHAR_BIT - n);
    fillbuf(n);
    return x;
}

static int fwrite_crc(uchar* p, int n, HANDLE fp)
{
    calccrc(p, n);
    return my_fwrite(p, 1, n, fp) == n;
}

static void init_getbits()
{
    bitbuf = 0;
    subbitbuf = 0;
    bitcount = 0;
    fillbuf(2 * CHAR_BIT);
}

////////////////////////////////////////////////////////////////////////////////////////
// SLIDE.C /////////////////////////////////////////////////////////////////////////////

static struct decode_option decode_define[] = {
    /* lh1 */
    {decode_c_dyn, decode_p_st0, decode_start_fix},
    /* lh2 */
    {decode_c_dyn, decode_p_dyn, decode_start_dyn},
    /* lh3 */
    {decode_c_st0, decode_p_st0, decode_start_st0},
    /* lh4 */
    {decode_c_st1, decode_p_st1, decode_start_st1},
    /* lh5 */
    {decode_c_st1, decode_p_st1, decode_start_st1},
    /* lh6 */
    {decode_c_st1, decode_p_st1, decode_start_st1},
    /* lh7 */
    {decode_c_st1, decode_p_st1, decode_start_st1},
    /* lzs */
    {decode_c_lzs, decode_p_lzs, decode_start_lzs},
    /* lz5 */
    {decode_c_lz5, decode_p_lz5, decode_start_lz5}};

static decode_option decode_set;
static unsigned long dicsiz;

static int decode(interfacing* iface2)
{
    CALL_STACK_MESSAGE1("decode( )");

    unsigned int i, j, k, c;
    unsigned int dicsiz1, offset;
    unsigned char* dtext;

    if (!buffer_init())
        return 0;

    infile = iface2->infile;
    outfile = iface2->outfile;
    dicbit = iface2->dicbit;
    origsize = iface2->original;
    compsize = iface2->packed;
    decode_set = decode_define[iface2->method - 1];

    prev_char = -1;
    dicsiz = 1L << dicbit;
    dtext = (unsigned char*)malloc(dicsiz);
    if (dtext == NULL)
    {
        iLHAErrorStrId = IDS_LOWMEM;
        return 0;
    }
    for (i = 0; i < dicsiz; i++)
        dtext[i] = 0x20;
    decode_set.decode_start();
    dicsiz1 = dicsiz - 1;
    offset = (iface2->method == LARC_METHOD_NUM) ? 0x100 - 2 : 0x100 - 3;
    count = 0;
    loc = 0;
    int ret = 1;
    while (count < origsize)
    {
        c = decode_set.decode_c();
        if (c <= UCHAR_MAX)
        {
            dtext[loc++] = c;
            if (loc == dicsiz)
            {
                if (!fwrite_crc(dtext, dicsiz, outfile))
                {
                    ret = 0;
                    goto exit;
                }
                loc = 0;
            }
            count++;
        }
        else
        {
            j = c - offset;
            i = (loc - decode_set.decode_p() - 1) & dicsiz1;
            count += j;
            for (k = 0; k < j; k++)
            {
                c = dtext[(i + k) & dicsiz1];

                dtext[loc++] = c;
                if (loc == dicsiz)
                {
                    if (!fwrite_crc(dtext, dicsiz, outfile))
                    {
                        ret = 0;
                        goto exit;
                    }
                    loc = 0;
                }
            }
        }
    }
    if (loc != 0)
        if (!fwrite_crc(dtext, loc, outfile))
            ret = 0;

exit:
    free(dtext);
    if (!buffer_done(outfile))
        ret = 0;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////
// COPYFILE ////////////////////////////////////////////////////////////////////////////

static int copyfile(FILE* infile2, HANDLE outfile2, int size)
{
    CALL_STACK_MESSAGE1("copyfile( , , )");

    char* buffer2 = (char*)malloc(BUFSIZE);
    if (buffer2 == NULL)
    {
        iLHAErrorStrId = IDS_LOWMEM;
        return 0;
    }

    int ret = 1;
    while (size)
    {
        int n = (size < BUFSIZE) ? size : BUFSIZE;
        if ((int)fread(buffer2, 1, n, infile2) < n)
        {
            iLHAErrorStrId = IDS_READERROR;
            ret = 0;
            break;
        }
        DWORD numw;
        if (!SafeWriteFile(outfile2, buffer2, n, &numw))
        {
            iLHAErrorStrId = IDS_WRITEERROR;
            ret = 0;
            break;
        }
        calccrc((uchar*)buffer2, n);
        size -= n;
        iProgress += n;
        if (pfLHAProgress != NULL)
            if (!pfLHAProgress(iProgress))
            {
                iLHAErrorStrId = -1;
                ret = 0;
                break;
            }
    }

    free(buffer2);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////
// EXTRACT.C ///////////////////////////////////////////////////////////////////////////

BOOL LHAUnpackFile(FILE* infile, HANDLE outfile, LHA_HEADER* lpHeader, int* CRC, char* fileName)
{
    CALL_STACK_MESSAGE1("LHAUnpackFile( , , , )");

    iface.method = lpHeader->method;
    iface.dicbit = 13; /* method + 8; -lh5- */
    iface.infile = infile;
    iface.outfile = outfile;
    iface.original = lpHeader->original_size;
    iface.packed = lpHeader->packed_size;

    outputFileName = fileName;

    crc = 0;
    iProgress = 0;

    int ret = 0;
    switch (lpHeader->method)
    {

    case LZHUFF0_METHOD_NUM:
    case LARC4_METHOD_NUM:
        ret = copyfile(infile, outfile, lpHeader->original_size);
        break;

    case LARC_METHOD_NUM: /* -lzs- */
        iface.dicbit = 11;
        ret = decode(&iface);
        break;

    case LZHUFF1_METHOD_NUM: /* -lh1- */
    case LZHUFF4_METHOD_NUM: /* -lh4- */
    case LARC5_METHOD_NUM:   /* -lz5- */
        iface.dicbit = 12;
        ret = decode(&iface);
        break;

    case LHA_UNKNOWNMETHOD:
        break;

    case LZHUFF6_METHOD_NUM: /* -lh6- */ /* Added N.Watazaki (^_^) */
    case LZHUFF7_METHOD_NUM:             /* -lh7- */
        iface.dicbit = (lpHeader->method - LZHUFF6_METHOD_NUM) + 15;

    default:
        ret = decode(&iface);
    }

    outputFileName = NULL;

    *CRC = crc;
    return ret;
}

/*****************************************************************************************\
**                                                                                       **
**                                                                                       **
**   LHAInit                                                                             **
**                                                                                       **
**                                                                                       **
\*****************************************************************************************/

void LHAInit()
{
    CALL_STACK_MESSAGE1("LHAInit()");
    make_crctable();
}

/*****************************************************************************************\
**                                                                                       **
**                                                                                       **
**   LHAOpenArchive                                                                      **
**                                                                                       **
**                                                                                       **
\*****************************************************************************************/

#define MAXSFXCODE 1024 * 64

int LHAOpenArchive(FILE*& f, LPCTSTR lpName)
{
    CALL_STACK_MESSAGE2("LHAOpenArchive( , %s)", lpName);

    if ((f = fopen(lpName, "rb")) == NULL)
    {
        iLHAErrorStrId = IDS_OPENERROR;
        return FALSE;
    }

    LHA_HEADER hdr;
    if (LHAGetHeader(f, &hdr) != GH_ERROR)
    { // zkusime precist jednu hlavicku
        fseek(f, 0, SEEK_SET);
        return TRUE; // vse OK, archiv je LHZ
    }

    // nepovedlo se precist prvni hlavicku, zkusime, jestli archiv neni SFX
    unsigned char* buffer2;
    unsigned char *p, *q;
    int n;

    buffer2 = (unsigned char*)malloc(MAXSFXCODE);
    if (buffer2 == NULL)
    {
        fclose(f);
        iLHAErrorStrId = IDS_LOWMEM; // nelze naalokovat buffer2, tak to zabalime
        return FALSE;
    }

    fseek(f, 0, SEEK_SET);
    n = (int)fread(buffer2, sizeof(char), MAXSFXCODE, f);

    for (p = buffer2 + 2, q = buffer2 + n - /* 5 */ (I_HEADER_LEVEL + 1) - 2; p < q; p++)
    {
        /* found "-l??-" keyword (as METHOD type string) */
        if (p[0] == '-' && p[1] == 'l' && p[4] == '-')
        {
            /* size and checksum validate check */
            if ((p[I_HEADER_LEVEL - 2] == 0 || p[I_HEADER_LEVEL - 2] == 0) && p[I_HEADER_SIZE - 2] > 20 && p[I_HEADER_CHECKSUM - 2] == calc_sum((char*)p, p[-2]))
            {
                fseek(f, (long)((p - 2) - buffer2) - n, SEEK_CUR);
                free(buffer2);
                return TRUE;
            }
            else if (p[I_HEADER_LEVEL - 2] == 2 && p[I_HEADER_SIZE - 2] >= 24 && p[I_ATTRIBUTE - 2] == 0x20)
            {
                fseek(f, (long)((p - 2) - buffer2) - n, SEEK_CUR);
                free(buffer2);
                return TRUE;
            }
        }
    }

    free(buffer2);
    fclose(f);
    if (iLHAErrorStrId == IDS_CHECKSUMERROR)
        iLHAErrorStrId = IDS_FILECORRUPT;
    return FALSE; // archiv neni LHZ ani SFX LHZ
}

// TestLHAOpenArchive - otestuje LHAOpenArchive tim, ze ji pusti na vsechny
// soubory na ceste "path" (prochazi i podadresare). Kdyz to nespadne, je
// vse OK :-)
//
// Volat takto:
// char path[MAX_PATH] = "c:";
// TestLHAOpenArchive(path);

/*void TestLHAOpenArchive(char* path)
{
  static WIN32_FIND_DATA fd;
  HANDLE hFind;
  int plen = strlen(path);
  strcat(path, "\\*");

  if ((hFind = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE)
  {
    do 
    {
      if (fd.cFileName[0] != 0 && strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
      {
        strcpy(path + plen + 1, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          TestLHAOpenArchive(path);
        }
        else
        {
          FILE* f;
          LHAOpenArchive(f, path);
          if (f) fclose(f);
        }
      }
    } 
    while (FindNextFile(hFind, &fd));
    FindClose(hFind);
  }

  path[plen] = 0;
}*/
