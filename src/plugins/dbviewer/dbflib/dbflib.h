// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* Define some usefull types */
#ifndef U8
#define U8 unsigned char
#endif
#ifndef U16
#define U16 unsigned short
#endif
#ifndef U32
#define U32 unsigned long
#endif
#ifndef BOOL
#define BOOL int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DBF_DBASE2 2
#define DBF_DBASE3 3
#define DBF_DBASE4 4
#define DBF_DBASE5 5
#define DBF_VISUALDBASE7 7
#define DBF_FOXPRO 10

#define DBF_FTYPE_CHAR 'C'    /* upto 254 B; upto 32 KB in Clipper & FoxPro */
#define DBF_FTYPE_NUM 'N'     /* upto 20 B */
#define DBF_FTYPE_LOGIC 'L'   /* ? (uninited),Y,y,N,n,F,f,T,t */
#define DBF_FTYPE_DATE 'D'    /* YYYYMMDD */
#define DBF_FTYPE_MEMO 'M'    /* 10-digit pointer to DBT/FPT file */
#define DBF_FTYPE_FLOAT 'F'   /* dBase IV, V, FoxPro, Clipper, 20 digits */
#define DBF_FTYPE_BIN 'B'     /* FoxPro, FoxBase */
#define DBF_FTYPE_GEN 'G'     /* dBase IV: OLE objects in MS Win */
#define DBF_FTYPE_PIC 'P'     /* FoxPro: Pictures */
#define DBF_FTYPE_CUR 'Y'     /* FoxPro: Currency */
#define DBF_FTYPE_DTIME 'T'   /* FoxPro: DateTime */
#define DBF_FTYPE_INT 'I'     /* FoxPro: 4-byte little-endian integer */
#define DBF_FTYPE_TSTAMP '@'  /* Visual dBase: TimeStamp: 8 bytes - two longs, first for date, second for time.  The date is the number of days since  01/01/4713 BC. Time is hours * 3600000L + minutes * 60000L + Seconds * 1000L */
#define DBF_FTYPE_INT_V7 1    /* Our ID to distinguish FoxPro Integer and Visual dBase Long */
#define DBF_FTYPE_AUTOINC '+' /* Visual dBase: AutoIncrement; same as long;  Leftmost bit used to indicate sign, 0 negative. */
#define DBF_FTYPE_DOUBLE 'O'  /* Visual dBase: 8 bytes - no conversions, stored as a double */

/* Visual dBase: types B,G,M: all stored as 10-digit strings w/ offset to .DBT */

#pragma pack(push, enter_1byte, 1)

/* Structures as they are stored in files */
/* Valid for dBase II */
typedef struct _dbf2file_header
{
    U8 version;
    U16 records;   /* Total number of records */
    U8 update_mon; /* Month of last update */
    U8 update_day; /* Day of last update */
    U8 update_yr;  /* Year of last update minus 1900 */
    U16 record_length;
} DBF2FILE_HDR, *DBF2FILE_HDR_PTR;

/* Valid for dBase III, III+, IV, V(?), FoxPro, FoxBase */
typedef struct _dbffile_header
{
    U8 version;
    U8 update_yr;  /* Year of last update minus 1900 */
    U8 update_mon; /* Month of last update */
    U8 update_day; /* Day of last update */
    U32 records;   /* Total number of records */
    U16 header_length;
    U16 record_length;
    U16 reserved1;
    U8 transaction_flag;    /* dBase IV: 1 if transaction not completed */
    U8 encryption_flag;     /* dBase IV: 1 if encrypted */
    U32 free_record_thread; /* Used on LAN only */
    U8 MDX_flag;            /* dBase IV: 1 if MDX present; FoxBase: 1 if CDX; Visual FoxPro: 2 with memo */
    U8 language;
    U8 reserved2[8]; /* dBase III+: for multi-user environment */
    U16 reserved3;
} DBFFILE_HDR, *DBFFILE_HDR_PTR;

/* Valid for Visual dBase version 7 */
typedef struct _dbf7file_header
{
    U8 version;    /* Must be 4 */
    U8 update_yr;  /* Year of last update minus 1900 */
    U8 update_mon; /* Month of last update */
    U8 update_day; /* Day of last update */
    U32 records;   /* Total number of records */
    U16 header_length;
    U16 record_length;
    U16 reserved1;
    U8 transaction_flag; /* dBase IV: 1 if transaction not completed */
    U8 encryption_flag;  /* dBase IV: 1 if encrypted */
    U8 multiUser[12];    /* Reserved for multi-user processing */
    U8 MDX_flag;         /* dBase IV: 1 if MDX present */
    U8 language;         /* language driver ID */
    U16 reserved2;
    U8 languageName[32]; /* Language driver name */
    U32 reserved3;       /* dBase III+: for multi-user environment */
} DBF7FILE_HDR, *DBF7FILE_HDR_PTR;

/* Valid for dBase II */
typedef struct _dbf2file_field
{
    U8 name[11];
    U8 type; /* See DBF_FTYPE_XXX */
    U8 length;
    U16 address; /* Internal memory pointer */
    U8 decimal;
} DBF2FILE_FIELD, *DBF2FILE_FIELD_PTR;

/* Valid for dBase III, III+, IV, V(?), FoxPro, FoxBase */
typedef struct _dbffile_field
{
    U8 name[11]; /* ASCIIZ field name */
    U8 type;     /* See DBF_FTYPE_XXX */
    U32 address; /* dBase III+: internal memory pointer */
    U8 length;
    U8 decimal;
    U16 multiuser1; /* dBase III+: for multi-user environment */
    U8 workAreaID;
    U16 multiuser2; /* dBase III+: for multi-user environment */
    U8 flag;        /* Flag for SET FIELDS */
    U8 reserved[7];
    U8 indexFlag; /* dBase IV: 1 if key for this field in MDX */
} DBFFILE_FIELD, *DBFFILE_FIELD_PTR;

/* Valid for Visual dBase version 7 */
typedef struct _dbf7file_field
{
    U8 name[32]; /* ASCIIZ field name */
    U8 type;     /* See DBF_FTYPE_XXX */
    U8 length;
    U8 decimal;
    U16 reserved1;
    U8 indexFlag; /* 1 if key for this field in MDX */
    U16 reserved2;
    U32 nextAutoIncrementVal;
    U32 reserved3;
} DBF7FILE_FIELD, *DBF7FILE_FIELD_PTR;

#pragma pack(pop, enter_1byte)

/* Our internal format-independent structures */
typedef struct _dbf_header
{
    U32 version;       /* DBF_DBASE2, DBF_DBASE3, DBF_DBASE4, DBF_FOXPRO */
    U32 recordsCnt;    /* total number of records in file */
    U32 fieldsCnt;     /* number of fields per record, including memo fields */
    U32 memoFieldsCnt; /* number of memo fields per record */
    U32 recordSize;    /* total size of record as in master DBF file */
    U32 headerSize;    /* total size of file header */
    U32 codePage;      /* codepage of DBF_FLDTYPE_CHAR fields */
} DBF_HEADER, *DBF_HEADER_PTR;

typedef struct _dbf_field
{
    U32 len;         /* total len of field in bytes */
    U32 decimals;    /* count of decimal digits */
    U32 posInRecord; /* position of field in record */
    U8 type;         /* see DBF_FTYPE_XXX */
    U8 name[32];     /* NULL-terminated */
} DBF_FIELD, *DBF_FIELD_PTR;

typedef enum tagDBFStatus
{
    DBFE_OK,
    DBFE_OOM,       /* Out of memory */
    DBFE_NOT_XBASE, /* Unrecognized input file format */
    DBFE_FILE_NOT_FOUND,
    DBFE_READ_ERROR,
    DBFE_WRITE_ERROR,
    DBFE_SEEK_ERROR,
    DBFE_NO_MEMO_FILE,
    DBFE_INVALID_BLOCK
} eDBFStatus;

class cDBF
{

private:
    DBF_HEADER hdr;
    DBF_FIELD_PTR fields;
    eDBFStatus status;
    FILE *f, *fmemo;
    U32 currField;
    U32 memoBlockSize;
    BOOL updated;

public:
    cDBF(const char* filename, BOOL readOnly = TRUE);
    ~cDBF(void);

    /* GetStatus should be called after constructing the object to verify success */
    eDBFStatus GetStatus(void) { return status; };
    eDBFStatus GetRecord(U32 index, char* data);
    eDBFStatus PutRecord(U32 index, char* data);
    eDBFStatus AppendRecord(char* data);
    eDBFStatus GetMemoField(U32 pos, char* buffer, U32 bufSize, U32* dataSize);
    DBF_HEADER_PTR GetHeader(void) { return &hdr; };
    DBF_FIELD_PTR GetFields(void) { return fields; };
    U32 GetCodePage(void) { return hdr.codePage; };
    U32 GetRecordsCnt(void) { return hdr.recordsCnt; };
    U32 GetFieldsCnt(void) { return hdr.fieldsCnt; };
    U32 GetMemoFieldsCnt(void) { return hdr.memoFieldsCnt; };
};
