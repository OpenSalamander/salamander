// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* ENVIRON.C, UNARJ, R JUNG, 01/22/94
 * Implementation dependent routines
 * Copyright (c) 1991-97 by ARJ Software, Inc.  All rights reserved.
 *
 *   This code may be freely used in programs that are NOT ARJ archivers
 *   (both compress and extract ARJ archives).
 *
 *   If you wish to distribute a modified version of this program, you
 *   MUST indicate that it is a modified version both in the program and
 *   source code.
 *
 *   If you modify this program, we would appreciate a copy of the new
 *   source code.  We are holding the copyright on the source code, so
 *   please do not delete our name from the program files or from the
 *   documentation.
 *
 *   The UNIX file date-time stamping code is derived from ZOO by
 *   Rahul Dhesi.
 *
 * Modification history:
 * Date      Programmer  Description of modification.
 * 04/09/91  R. Jung     Rewrote code.
 * 04/23/91  M. Adler    Portabilized.
 * 04/29/91  R. Jung     Added get_mode_str().
 * 05/08/91  R. Jung     Combined set_ftime() and set_fmode().
 * 06/03/91  R. Jung     Changed arguments in get_mode_str() and
 *                       set_ftime_mode().
 * 07/07/91  R. Jung     Added default_case_path() and UNIX section.
 * 07/24/91  R. Jung     Fixed use of _chmod to handle directories.
 * 08/27/91  R. Jung     Added date/time handling to Coherent.
 * 09/01/91  R. Jung     Added #include <stdlib.h> to vanilla section.
 *                       Added file date-time stamping to UNIX section.
 * 01/22/94  R. Jung     Changed copyright message.
 *
 */

#include "precomp.h"
#include "unarj.h"

#ifdef __TURBOC__

#define SUBS_DEFINED

#include <string.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <alloc.h>

FILE* file_open(name, mode)
char* name;
char* mode;
{
    return fopen(name, mode);
}

int file_read(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fread(buf, (size_t)size, (size_t)nitems, stream);
}

int file_seek(stream, offset, mode)
FILE* stream;
long offset;
int mode;
{
    return fseek(stream, offset, mode);
}

long file_tell(stream)
FILE* stream;
{
    return ftell(stream);
}

int file_write(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fwrite(buf, (size_t)size, (size_t)nitems, stream);
}

void* xmalloc(size)
int size;
{
    return (void*)malloc((size_t)size);
}

void
    case_path(name) char* name;
{
    strupper(name);
}

void
    default_case_path(name) char* name;
{
    strupper(name);
}

int file_exists(name)
char* name;
{
    return (access(name, 0) == 0);
}

void
    get_mode_str(str, mode) char* str;
uint mode;
{
    strcpy(str, "---W");
    if (mode & FA_ARCH)
        str[0] = 'A';
    if (mode & FA_SYSTEM)
        str[1] = 'S';
    if (mode & FA_HIDDEN)
        str[2] = 'H';
    if (mode & FA_RDONLY)
        str[3] = 'R';
}

int set_ftime_mode(name, tstamp, attribute, host)
char* name;
ulong tstamp;
uint attribute;
uint host;
{
    FILE* fd;
    int code;

    if ((fd = fopen(name, "r+b")) == NULL)
        return -1;
    code = setftime(fileno(fd), (struct ftime*)&tstamp);
    fclose(fd);
    if (host == OS)
    {
        attribute &= 0x27;
        if (_chmod(name, 1, attribute) == -1)
            return -1;
    }
    return code;
}

#endif

#ifdef _QC

#define SUBS_DEFINED

#include <string.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>

FILE* file_open(name, mode)
char* name;
char* mode;
{
    return fopen(name, mode);
}

int file_read(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fread(buf, (size_t)size, (size_t)nitems, stream);
}

int file_seek(stream, offset, mode)
FILE* stream;
long offset;
int mode;
{
    return fseek(stream, offset, mode);
}

long file_tell(stream)
FILE* stream;
{
    return ftell(stream);
}

int file_write(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fwrite(buf, (size_t)size, (size_t)nitems, stream);
}

void* xmalloc(size)
int size;
{
    return (void*)malloc((size_t)size);
}

void
    case_path(name) char* name;
{
    strupper(name);
}

void
    default_case_path(name) char* name;
{
    strupper(name);
}

int file_exists(name)
char* name;
{
    return (access(name, 0) == 0);
}

void
    get_mode_str(str, mode) char* str;
uint mode;
{
    strcpy(str, "---W");
    if (mode & FA_ARCH)
        str[0] = 'A';
    if (mode & FA_SYSTEM)
        str[1] = 'S';
    if (mode & FA_HIDDEN)
        str[2] = 'H';
    if (mode & FA_RDONLY)
        str[3] = 'R';
}

int set_ftime_mode(name, tstamp, attribute, host)
char* name;
ulong tstamp;
uint attribute;
uint host;
{
    FILE* fd;
    int code;
    uint date_stamp, time_stamp;

    date_stamp = (uint)(tstamp >> 16);
    time_stamp = (uint)(tstamp & 0xFFFF);
    if ((fd = fopen(name, "r+b")) == NULL)
        return -1;
    code = _dos_setftime(fileno(fd), date_stamp, time_stamp);
    fclose(fd);
    if (host == OS)
    {
        if (_dos_setfileattr(name, attribute))
            return -1;
    }
    return code;
}

#endif

#ifdef _OS2

#define SUBS_DEFINED

#include <string.h>
#define INCL_DOSFILEMGR
#include <os2.h>
#include <io.h>
#include <fcntl.h>

FILE* file_open(name, mode)
char* name;
char* mode;
{
    return fopen(name, mode);
}

int file_read(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fread(buf, (size_t)size, (size_t)nitems, stream);
}

int file_seek(stream, offset, mode)
FILE* stream;
long offset;
int mode;
{
    return fseek(stream, offset, mode);
}

long file_tell(stream)
FILE* stream;
{
    return ftell(stream);
}

int file_write(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fwrite(buf, (size_t)size, (size_t)nitems, stream);
}

void* xmalloc(size)
int size;
{
    return (void*)malloc((size_t)size);
}

void
    case_path(name) char* name;
{
    strupper(name);
}

void
    default_case_path(name) char* name;
{
    strupper(name);
}

int file_exists(name)
char* name;
{
    return (access(name, 0) == 0);
}

void
    get_mode_str(str, mode) char* str;
uint mode;
{
    strcpy(str, "---W");
    if (mode & FA_ARCH)
        str[0] = 'A';
    if (mode & FA_SYSTEM)
        str[1] = 'S';
    if (mode & FA_HIDDEN)
        str[2] = 'H';
    if (mode & FA_RDONLY)
        str[3] = 'R';
}

int set_ftime_mode(name, tstamp, attribute, host)
char* name;
ulong tstamp;
uint attribute;
uint host;
{
    int code;
    FDATE date_stamp;
    FTIME time_stamp;
    HFILE handle;
    FILESTATUS info;
    USHORT action;

    date_stamp.day = ts_day(tstamp);
    date_stamp.month = ts_month(tstamp);
    date_stamp.year = ts_year(tstamp) - 1980;
    time_stamp.twosecs = ts_sec(tstamp) / 2;
    time_stamp.minutes = ts_min(tstamp);
    time_stamp.hours = ts_hour(tstamp);
    if (DosOpen(name, &handle, &action, 0L, 0, FILE_OPEN,
                OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE, 0L) != 0)
        return -1;
    info.fdateCreation = date_stamp;
    info.ftimeCreation = time_stamp;
    info.fdateLastAccess = date_stamp;
    info.ftimeLastAccess = time_stamp;
    info.fdateLastWrite = date_stamp;
    info.ftimeLastWrite = time_stamp;
    info.cbFile = 0;
    info.cbFileAlloc = 0;
    info.attrFile = 0;
    code = (int)DosSetFileInfo(handle, 1, (PBYTE)&info, sizeof(info));
    (void)DosClose(handle);
    if (host == OS)
    {
        if (DosSetFileMode(name, attribute, 0L))
            return -1;
    }
    return code;
}

#endif

#ifdef UNIX

#define SUBS_DEFINED

#include <time.h>

#ifndef time_t
#define time_t long
#endif

extern struct tm* localtime();
extern time_t time();
extern char* strcpy();
extern void* malloc();

FILE* file_open(name, mode)
char* name;
char* mode;
{
    return fopen(name, mode);
}

int file_read(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fread(buf, (int)size, (int)nitems, stream);
}

int file_seek(stream, offset, mode)
FILE* stream;
long offset;
int mode;
{
    return fseek(stream, offset, mode);
}

long file_tell(stream)
FILE* stream;
{
    return ftell(stream);
}

int file_write(buf, size, nitems, stream)
char* buf;
int size;
int nitems;
FILE* stream;
{
    return fwrite(buf, (int)size, (int)nitems, stream);
}

void* xmalloc(size)
int size;
{
    return (void*)malloc((uint)size);
}

void
    case_path(name) char* name;
{
    (char*)name;
}

void
    default_case_path(name) char* name;
{
    strlower(name);
}

int file_exists(name)
char* name;
{
    FILE* fd;

    if ((fd = fopen(name, "rb")) == NULL)
        return 0;
    fclose(fd);
    return 1;
}

void
    get_mode_str(str, mode) char* str;
uint mode;
{
    strcpy(str, "---W");
    if (mode & FA_ARCH)
        str[0] = 'A';
    if (mode & FA_SYSTEM)
        str[1] = 'S';
    if (mode & FA_HIDDEN)
        str[2] = 'H';
    if (mode & FA_RDONLY)
        str[3] = 'R';
}

long gettz() /* returns the offset from GMT in seconds */
{
#define NOONOFFSET 43200L
#define SEC_IN_DAY (24L * 60L * 60L)
#define INV_VALUE (SEC_IN_DAY + 1L)
    static long retval = INV_VALUE;
    long now, noon;
    struct tm* noontm;

    if (retval != INV_VALUE)
        return retval;
    now = (long)time((long*)0);
    /* Find local time for GMT noon today */
    noon = now - now % SEC_IN_DAY + NOONOFFSET;
    noontm = localtime(&noon);
    retval = NOONOFFSET - 60 * (60 * noontm->tm_hour - noontm->tm_min);
    return retval;
}

long mstonix(tstamp)
ulong tstamp;
{
    uint date, time;
    int year, month, day, hour, min, sec, daycount;
    long longtime;
    /* no. of days to beginning of month for each month */
    static int dsboy[12] =
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    date = (uint)((tstamp >> 16) & 0xffff);
    time = (uint)(tstamp & 0xffff);
    if (date == 0 && time == 0)
        return 0L;

    year = ((date >> 9) & 0x7f) + 1980;
    month = (date >> 5) & 0x0f;
    day = date & 0x1f;
    hour = (time >> 11) & 0x1f;
    min = (time >> 5) & 0x3f;
    sec = (time & 0x1f) * 2;

    daycount = 365 * (year - 1970) + /* days due to whole years */
               (year - 1969) / 4 +   /* days due to leap years */
               dsboy[month - 1] +    /* days since beginning of this year */
               day - 1;              /* days since beginning of month */

    if (year % 4 == 0 &&
        year % 400 != 0 && month >= 3) /* if this is a leap year and month */
        daycount++;                    /* is March or later, add a day */

    longtime = daycount * 24L * 60L * 60L +
               hour * 60L * 60L + min * 60 + sec;
    return longtime;
}

int set_ftime_mode(name, tstamp, attribute, host)
char* name;
ulong tstamp;
uint attribute;
uint host;
{
    time_t m_time;
    struct utimbuf
    {
        time_t atime; /* New access time */
        time_t mtime; /* New modification time */
    } tb;

    (char*)name;
    (uint) attribute;
    (uint) host;

    m_time = mstonix(tstamp) + gettz();

    tb.mtime = m_time; /* Set modification time */
    tb.atime = m_time; /* Set access time */

    /* set the time stamp on the file */
    return utime(name, &tb);
}

#endif /* end of UNIX section */

#ifndef SUBS_DEFINED /* vanilla version for other compilers */

#include <string.h>
extern char* strcpy();

FILE* file_open(char* name,
                char* mode)
{
    return fopen(name, mode);
}

int file_read(char* buf,
              int size,
              int nitems,
              FILE* stream)
{
    return fread(buf, (int)size, (int)nitems, stream);
}

int file_seek(FILE* stream,
              long offset,
              int mode)
{
    return fseek(stream, offset, mode);
}

long file_tell(FILE* stream)
{
    return ftell(stream);
}

int file_write(char* buf,
               int size,
               int nitems,
               FILE* stream)
{
    return fwrite(buf, (int)size, (int)nitems, stream);
}

/*
void *
xmalloc(int size)
{
    return (void *)malloc((uint) size);
}
*/

void case_path(char* name)
{
    (char*)name;
}

void default_case_path(char* name)
{
    (char*)name;
}

int file_exists(char* name)
{
    FILE* fd;

    if ((fd = fopen(name, "rb")) == NULL)
        return 0;
    fclose(fd);
    return 1;
}

void get_mode_str(char* str,
                  uint mode)
{
    strcpy(str, "---W");
    if (mode & FA_ARCH)
        str[0] = 'A';
    if (mode & FA_SYSTEM)
        str[1] = 'S';
    if (mode & FA_HIDDEN)
        str[2] = 'H';
    if (mode & FA_RDONLY)
        str[3] = 'R';
}

int set_ftime_mode(char* name,
                   ulong tstamp,
                   uint attribute,
                   uint host)
{
    (char*)name;
    (ulong) tstamp;
    (uint) attribute;
    (uint) host;
    return 0;
}

#endif /* end of vanilla section */

/* end ENVIRON.C */
