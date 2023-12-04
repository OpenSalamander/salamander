// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* UNARJ.H, UNARJ, R JUNG, 07/29/96
 * Include file
 * Copyright (c) 1990-97 ARJ Software, Inc.  All rights reserved.
 *
 *   This code may be freely used in programs that are NOT ARJ archivers
 *   (both compress and extract ARJ archives).
 *
 *   If you wish to distribute a modified version of this program, you
 *   MUST indicate that it is a modified version both in the program and
 *   source code.
 *
 *   If you modify this program, we would appreciate a copy of the new
 *   source code.  we are holding the copyright on the source code, so
 *   please do not delete our name from the program files or from the
 *   documentation.
 *
 * Modification history:
 * Date      Programmer  Description of modification.
 * 04/05/91  R. Jung     Rewrote code.
 * 04/23/91  M. Adler    Portabilized.
 * 04/29/91  R. Jung     Added volume label support.
 * 05/30/91  R. Jung     Added SEEK_END definition.
 * 06/03/91  R. Jung     Changed arguments in get_mode_str() and
 *                       set_ftime_mode().
 * 06/28/91  R. Jung     Added new HOST OS numbers.
 * 07/08/91  R. Jung     Added default_case_path() and strlower().
 * 07/21/91  R. Jung     Fixed #endif _QC comment.
 * 08/27/91  R. Jung     Added #ifdef for COHERENT.
 * 09/01/91  R. Jung     Added new host names.
 * 12/03/91  R. Jung     Added BACKUP_FLAG.
 * 04/06/92  R. Jung     Added ARCHIMEDES.
 * 02/17/93  R. Jung     Improved ARJ header information.  Added ARJ_M_VERSION.
 * 01/22/94  R. Jung     Changed copyright message.
 * 07/29/96  R. Jung     Added "/" to list of path separators.
 *
 */

#ifndef _ARH_DEF_
#define _ARH_DEF_

#include <stdio.h>
#include <limits.h>

typedef unsigned char uchar;   /*  8 bits or more */
typedef unsigned int uint;     /* 16 - 32 bits or more */
typedef unsigned short ushort; /* 16 bits or more */
typedef unsigned long ulong;   /* 32 bits or more */

#define USHRT_BIT (CHAR_BIT * sizeof(ushort))

/* ********************************************************* */
/* Environment definitions (implementation dependent)        */
/* ********************************************************* */

#define OS 0
#define PATH_SEPARATORS "\\:/"
#define PATH_CHAR '\\'
#define MAXSFX 250000L // lukas: tohle jsem zvednul 10x
#define ARJ_SUFFIX ".ARJ"
#define SWITCH_CHARS "-"
#define FIX_PARITY(c) c &= ASCII_MASK
#define ARJ_DOT '.'
#define DEFAULT_DIR ""

/* ********************************************************* */
/* end of environmental defines                              */
/* ********************************************************* */

/* ********************************************************* */
/*
/* Structure of archive main header (low order byte first):
/*
/*  2  header id (comment and local file) = 0x60, 0xEA
/*  2  basic header size (from 'first_hdr_size' thru 'comment' below)
/*	     = first_hdr_size + strlen(filename) + 1 + strlen(comment) + 1
/*	     = 0 if end of archive
/*
/*  1  first_hdr_size (size up to 'extra data')
/*  1  archiver version number
/*  1  minimum archiver version to extract
/*  1  host OS	 (0 = MSDOS, 1 = PRIMOS, 2 = UNIX, 3 = AMIGA, 4 = MACDOS)
/*               (5 = OS/2, 6 = APPLE GS, 7 = ATARI ST, 8 = NEXT)
/*               (9 = VAX VMS) (11 Win32)
/*  1  arj flags (0x01 = GARBLED_FLAG, 0x02 = OLD_SECURED_FLAG)
/*               (0x04 = VOLUME_FLAG,  0x08 = EXTFILE_FLAG)
/*               (0x10 = PATHSYM_FLAG, 0x20 = BACKUP_FLAG)
/*               (0x40 = SECURED_FLAG)
/*  1  arj security version (2 = current)
/*  1  file type            (2 = comment header)
/*  1  ?                   ]
/*  4  date time stamp created
/*  4  date time stamp modified
/*  4  archive size up to the end of archive marker
/*  4  file position of security envelope data
/*  2  entryname position in filename
/*  2  length in bytes of trailing security data
/*  2  host data
/*  ?  extra data
/*
/*  ?  archive filename (null-terminated)
/*  ?  archive comment  (null-terminated)
/*
/*  4  basic header CRC
/*
/*  2  1st extended header size (0 if none)
/*  ?  1st extended header
/*  4  1st extended header's CRC
/*  ...
/*
/*
/* Structure of archive file header (low order byte first):
/*
/*  2  header id (comment and local file) = 0x60, 0xEA
/*  2  basic header size (from 'first_hdr_size' thru 'comment' below)
/*	     = first_hdr_size + strlen(filename) + 1 + strlen(comment) + 1
/*	     = 0 if end of archive
/*
/*  1  first_hdr_size (size up to 'extra data')
/*  1  archiver version number
/*  1  minimum archiver version to extract
/*  1  host OS	 (0 = MSDOS, 1 = PRIMOS, 2 = UNIX, 3 = AMIGA, 4 = MACDOS)
/*               (5 = OS/2, 6 = APPLE GS, 7 = ATARI ST, 8 = NEXT)
/*               (9 = VAX VMS)
/*  1  arj flags (0x01 = GARBLED_FLAG, 0x02 = NOT USED)
/*               (0x04 = VOLUME_FLAG,  0x08 = EXTFILE_FLAG)
/*               (0x10 = PATHSYM_FLAG, 0x20 = BACKUP_FLAG)
/*               (0x40 = NOT USED)
/*  1  method    (0 = stored, 1 = compressed most ... 4 compressed fastest)
/*  1  file type (0 = binary, 1 = text, 2 = comment header, 3 = directory)
/*               (4 = label)
/*  1  garble password modifier
/*  4  date time stamp modified
/*  4  compressed size
/*  4  original size
/*  4  original file's CRC
/*  2  entryname position in filename
/*  2  file access mode
/*  2  host data
/*  ?  extra data
/*     4 bytes for extended file position
/*
/*  ?  filename (null-terminated)
/*  ?  comment	(null-terminated)
/*
/*  4  basic header CRC
/*
/*  2  1st extended header size (0 if none)
/*  ?  1st extended header
/*  4  1st extended header's CRC
/*  ...
/*  ?  compressed file
/*
/* ********************************************************* */
/* ********************************************************* */
/*                                                           */
/*     Time stamp format:                                    */
/*                                                           */
/*      31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16      */
/*     |<---- year-1980 --->|<- month ->|<--- day ---->|     */
/*                                                           */
/*      15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0      */
/*     |<--- hour --->|<---- minute --->|<- second/2 ->|     */
/*                                                           */
/* ********************************************************* */

#define DDICSIZ 26624
extern uchar text[];

#define CODE_BIT 16

#define NULL_CHAR '\0'
#define MAXMETHOD 4

#define ARJ_VERSION 3
#define ARJ_M_VERSION 6 /* ARJ version that supports modified date. */
#define ARJ_X_VERSION 3 /* decoder version */
#define ARJ_X1_VERSION 1
#define DEFAULT_METHOD 1
#define DEFAULT_TYPE 0 /* if type_sw is selected */
#define HEADER_ID 0xEA60
#define HEADER_ID_HI 0xEA
#define HEADER_ID_LO 0x60
#define FIRST_HDR_SIZE 30
#define FIRST_HDR_SIZE_V 34
#define COMMENT_MAX 2048
#define HEADERSIZE_MAX (FIRST_HDR_SIZE + 10 + ARJ_MAX_PATH + COMMENT_MAX)
#define BINARY_TYPE 0 /* This must line up with binary/text strings */
#define TEXT_TYPE 1
#define COMMENT_TYPE 2
#define DIR_TYPE 3
#define LABEL_TYPE 4

#define GARBLE_FLAG 0x01
#define VOLUME_FLAG 0x04
#define EXTFILE_FLAG 0x08
#define PATHSYM_FLAG 0x10
#define BACKUP_FLAG 0x20

typedef ulong UCRC; /* CRC-32 */

#define CRC_MASK 0xFFFFFFFFL

#define ARJ_PATH_CHAR '/'

/* unarj.c */

extern long origsize;
extern long compsize;

extern ushort bitbuf;

extern uchar subbitbuf;
extern uchar header[HEADERSIZE_MAX];

extern int bitcount;

/* Global functions */

/* decode.c */

void decode(void);
void decode_f(void);

//***********************************************************************************
//
// moje "novinky"
//

#define INBUFSIZ (16 * 1024)
#define READ_BYTE(c) \
    { \
        if (InPtr == InEnd) \
            FillInputBuffer(); \
        c = *InPtr++; \
    }

extern unsigned char InputBuffer[INBUFSIZ];
extern unsigned char* InPtr;
extern unsigned char* InEnd;

void Error(int code);
void ErrorSkip(int code);
void __fastcall FillInputBuffer();
void WriteTxtCrc(uchar* buffer, DWORD size);

#endif

/* end UNARJ.H */
