// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*****************************************************************************
 * Revisions:
 *
 *  0306xx: Visual dBase 7 support
 *  010919: dBaseIII/IV/Fox*: field name checked for 0x0d & 0x00
 *  010917: dBaseIII/IV/Fox*: max record size check removed; max rec cnt 1G
 *************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crtdbg.h>
#include <time.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "DBFLib.h"

#define strend(x) (x + strlen(x))
#define min(a, b) ((a) < (b) ? (a) : (b))

/*****************************************************************************
 * cDBF::cDBF - open DBF file, parses header and opens memo file, if it exists
 * ~~~~~~~~~~   User should check GetStatus() after instantiating this class
 *              to see whether the file was open successfully
 *              Can only open existing file.
 */
cDBF::cDBF(const char* filename, BOOL readOnly) : fields(NULL), status(DBFE_OK),
                                                  f(NULL), fmemo(NULL), updated(FALSE)
{
    /* Info: GetStatus should be called after constructing the object to verify success */
    union
    {
        DBFFILE_HDR fhdr;
        DBF2FILE_HDR fhdr2;
        DBF7FILE_HDR fhdr7;
    };
    union
    {
        DBFFILE_FIELD fld;
        DBF2FILE_FIELD fld2;
        DBF7FILE_FIELD fld7;
    };
    U32 nBytesRead, pos;
    int i;
    char memo[_MAX_PATH], *s;

    /* Ensure we have a proper structure alignment */
    _ASSERT(sizeof(DBFFILE_HDR) == 32);
    _ASSERT(sizeof(DBFFILE_FIELD) == 32);
    _ASSERT(sizeof(DBF7FILE_HDR) == 68);

    f = fopen(filename, readOnly ? "rb" : "rb+");
    if (!f)
    {
        status = DBFE_FILE_NOT_FOUND;
        return;
    }

    nBytesRead = (unsigned long)fread(&fhdr, sizeof(DBFFILE_HDR), 1, f);
    if (1 != nBytesRead)
    {
        status = DBFE_READ_ERROR;
        return;
    }

    switch (fhdr.version)
    {
    case 0x02:
        hdr.version = DBF_DBASE2;
        break;
    case 0x03:
        hdr.version = DBF_DBASE3;
        break;
    case 0x04:
        hdr.version = DBF_VISUALDBASE7;
        break; /* Visual dBase Version 7 */
    case 0x05:
        hdr.version = DBF_DBASE5;
        break; /* Not tested. Does it exist? */
    case 0x83:
        hdr.version = DBF_DBASE3;
        break; /* III+ w/ memo */
    case 0x8B:
        hdr.version = DBF_DBASE4;
        break; /* IV w/ memo */
    case 0x8C:
        hdr.version = DBF_VISUALDBASE7;
        break; /* Visual dBase Version 7 w/ memo */
    case 0x8E:
        hdr.version = DBF_DBASE4;
        break; /* IV w/ SQL table */
    case 0xF5:
        hdr.version = DBF_FOXPRO;
        break; /* FoxPro w/ memo */
    case 0x30:
        hdr.version = DBF_FOXPRO;
        break; /* Visual FoxPro w/ DBC */
    case 0x7B:
        hdr.version = DBF_DBASE4;
        break; /* IV w/ memo */
    default:
        status = DBFE_NOT_XBASE;
        return;
    }
    /* dBase II files have a completely different header than the other formats */
    if (hdr.version == DBF_DBASE2)
    {

        hdr.recordsCnt = fhdr2.records;
        hdr.fieldsCnt = 0;
        hdr.recordSize = fhdr2.record_length;
        hdr.headerSize = 8 + 32 * 16 + 1;
        hdr.memoFieldsCnt = 0;
        hdr.codePage = 0;

        fseek(f, sizeof(DBF2FILE_HDR), SEEK_SET);

        fields = (DBF_FIELD_PTR)calloc(sizeof(DBF_FIELD), 32 /* this is a small wasting */);
        if (!fields)
        {
            status = DBFE_OOM;
            return;
        }

        pos = 1; /* skip the deletion flag */
        for (i = 0; i < 32 /* max fields */; i++)
        {
            nBytesRead = (unsigned long)fread(&fld2, sizeof(DBF2FILE_FIELD), 1, f);
            if (1 != nBytesRead)
            {
                status = DBFE_READ_ERROR;
                return;
            }
            if (!fld2.name[0] || (fld2.name[0] == 0x0d))
            {
                /* no more fields */
                break;
            }
            hdr.fieldsCnt++;
            strncpy_s((char*)fields[i].name, sizeof(fields[i].name), (char*)fld2.name, _TRUNCATE);
            fields[i].type = fld2.type;
            fields[i].len = fld2.length;
            fields[i].decimals = fld2.decimal;
            fields[i].posInRecord = pos;
            pos += fld2.length;
        }
        currField = -1; /* to force seek on first GetRecord */

        return;
    }
    if ((fhdr.update_mon > 12) || (fhdr.update_day > 31) || (fhdr.records > 1000000000))
    {
        /* does not look like an XBase file */
        status = DBFE_NOT_XBASE;
        return;
    }

    switch (fhdr.language)
    {
    case 0x02:
        hdr.codePage = 850;
        break; /* DOS Multilingual (Latin I) */
    case 0x03:
        hdr.codePage = 1251;
        break;
    case 0xc8:
        hdr.codePage = 1250;
        break;
    case 0x64:
        hdr.codePage = 852;
        break; /* Latin II MS-DOS */
    case 0x65:
        hdr.codePage = 865;
        break; /* Nordic MS-DOS */
    case 0x66:
        hdr.codePage = 866;
        break; /* Russian MS-DOS */
    case 0x01:
        hdr.codePage = 437;
        break;
    /* WARNING: 99.99% of files have no language specified */
    default:
        hdr.codePage = 0; /* unknown */
    }
    hdr.recordsCnt = fhdr.records;
    hdr.recordSize = fhdr.record_length;
    hdr.headerSize = fhdr.header_length;
    hdr.memoFieldsCnt = 0;

    pos = 1; /* The first byte (pos 0) determines whether the record has been deleted */
    if (hdr.version != DBF_VISUALDBASE7)
    {
        hdr.fieldsCnt = (fhdr.header_length - sizeof(DBFFILE_HDR) - 1) / sizeof(DBFFILE_FIELD);
        fields = (DBF_FIELD_PTR)calloc(sizeof(DBF_FIELD), hdr.fieldsCnt);
        if (!fields)
        {
            status = DBFE_OOM;
            return;
        }
        for (i = 0; i < (int)hdr.fieldsCnt; i++)
        {
            nBytesRead = (unsigned long)fread(&fld, sizeof(DBFFILE_FIELD), 1, f);
            if (1 != nBytesRead)
            {
                status = DBFE_READ_ERROR;
                return;
            }
            if (!fld.name[0] || (fld.name[0] == 0x0d))
            {
                /* no more fields - this seems to be produced by Visual FoxPro */
                hdr.fieldsCnt = i;
                break;
            }

            strncpy_s((char*)fields[i].name, sizeof(fields[i].name), (char*)fld.name,
                      min(sizeof(fld.name), sizeof(fields[i].name) - 1));
            fields[i].type = fld.type;
            fields[i].len = fld.length;
            if (fld.type == DBF_FTYPE_MEMO)
            {
                hdr.memoFieldsCnt++;
            }
            /* FoxPro files can contain character fields having upto 32KB
           Then the high byte is stored where count of decimals is usually stored */
            if ((fld.type == DBF_FTYPE_CHAR) && (hdr.version == DBF_FOXPRO))
            {
                fields[i].len += (U32)fld.decimal << 8;
                fields[i].decimals = 0;
            }
            else
            {
                fields[i].decimals = fld.decimal;
            }
            fields[i].posInRecord = pos;
            pos += fields[i].len;
        }
    }
    else
    { /* now processing Version 7 header */
        /* Read in the rest of the file header */
        nBytesRead = (unsigned long)fread(&fhdr7.languageName, sizeof(DBF7FILE_HDR) - sizeof(DBFFILE_HDR), 1, f);
        if (1 != nBytesRead)
        {
            status = DBFE_READ_ERROR;
            return;
        }
        hdr.fieldsCnt = (fhdr.header_length - sizeof(DBF7FILE_HDR) - 1) / sizeof(DBF7FILE_FIELD);
        fields = (DBF_FIELD_PTR)calloc(sizeof(DBF_FIELD), hdr.fieldsCnt);
        if (!fields)
        {
            status = DBFE_OOM;
            return;
        }
        for (i = 0; i < (int)hdr.fieldsCnt; i++)
        {
            nBytesRead = (unsigned long)fread(&fld7, sizeof(DBF7FILE_FIELD), 1, f);
            if (1 != nBytesRead)
            {
                status = DBFE_READ_ERROR;
                return;
            }
            if (!fld7.name[0] || (fld7.name[0] == 0x0d))
            {
                /* no more fields - the rest of the header is Field Properties Structure,
              Standard Property and Constraint Descriptor Array,
              Custom Property Descriptor Array,
              Referential Integrity Property Descriptor Array */
                hdr.fieldsCnt = i;
                break;
            }

            strncpy_s((char*)fields[i].name, sizeof(fields[i].name), (char*)fld7.name,
                      min(sizeof(fld7.name), sizeof(fields[i].name) - 1));
            fields[i].type = fld7.type;
            if (fld7.type == DBF_FTYPE_INT)
            {
                // Visual dBase 'I' has different format than older dBase/FoxPro
                fields[i].type = DBF_FTYPE_INT_V7;
            }
#if defined(DEBUG) || defined(_DEBUG)
            switch (fld7.type)
            {
            case DBF_FTYPE_LOGIC:
                _ASSERT(fld7.length == 1);
                break;
            case DBF_FTYPE_DATE:
            case DBF_FTYPE_TSTAMP:
            case DBF_FTYPE_DOUBLE:
                _ASSERT(fld7.length == 8);
                break;
            case DBF_FTYPE_MEMO:
            case DBF_FTYPE_BIN:
            case DBF_FTYPE_GEN:
            case DBF_FTYPE_PIC:
                _ASSERT(fld7.length == 10);
                break;
            case DBF_FTYPE_INT:
            case DBF_FTYPE_AUTOINC:
                _ASSERT(fld7.length == 4);
                break;
            }
#endif // #if defined(DEBUG) || defined(_DEBUG)
            fields[i].len = fld7.length;
            if (fld7.type == DBF_FTYPE_MEMO)
            {
                hdr.memoFieldsCnt++;
            }
            fields[i].decimals = fld7.decimal;
            fields[i].posInRecord = pos;
            pos += fields[i].len;
        }
    }
    currField = -1; /* to force seek on first GetRecord */

    /* Look for memo file */
    strcpy(memo, filename);
    s = strrchr(memo, '.');
    if (!s)
        s = strend(memo);

    switch (hdr.version)
    {
    case DBF_FOXPRO:
        strcpy(s, ".fpt");
        fmemo = fopen(memo, "rb");
        if (fmemo)
        {
            U8 tmp[2];

            fseek(fmemo, 6, SEEK_SET);
            if (1 != fread(&tmp, 2, 1, fmemo))
            {
                /* Incomplete file? */
                fclose(fmemo);
                fmemo = NULL;
            }
            else
            {
                memoBlockSize = (U32)tmp[1] + ((U32)tmp[0] << 8); /* Byte swap */
                if ((memoBlockSize < 1) || (memoBlockSize > 32768 /*some reasonable value*/))
                {
                    /* Does not look like a valid memo file */
                    fclose(fmemo);
                    fmemo = NULL;
                }
            }
        }
        break;

    case DBF_DBASE3:
    case DBF_DBASE4:
    case DBF_DBASE5:
        memoBlockSize = 512;
        strcpy(s, ".dbt");
        fmemo = fopen(memo, "rb");
        if (fmemo)
        {
            U8 tmp;
            fseek(fmemo, 16, SEEK_SET);
            fread(&tmp, 1, 1, fmemo);
            if (!(((hdr.version == DBF_DBASE3) && (tmp == 3)) || (tmp == 0 /*dBase IV*/) || (tmp == 0xE5 /*???dBase IV sample from Delphi???*/)))
            {
                /* Does not look like a valid memo file */
                fclose(fmemo);
                fmemo = NULL;
            }
            else
            {
                if (!tmp || (tmp == 0xE5 /*???dBase IV sample from Delphi???*/))
                { /* dBase IV or V */
                    /* On offset 20 there seems to be a block length; similar info might be on ofs 4 */
                    fseek(fmemo, 20, SEEK_SET);
                    if ((1 != fread(&memoBlockSize, 4, 1, fmemo)) || (memoBlockSize > 32768))
                    {
                        /* Does not look like a valid memo file */
                        fclose(fmemo);
                        fmemo = NULL;
                    }
                    else if (!memoBlockSize)
                    {
                        memoBlockSize = 512; /* restore default value */
                    }
                }
            }
            /* dBase IV stores file name starting on position 8. Should we check??? */
        }
        break;
    }

    return;
} /* ::cDBF */

/*****************************************************************************
 * cDBF::~cDDBF - closes DBF and memo files. 
 * ~~~~~~~~~~~~   Last update date & record counter are updated if necessary
 *
 */
cDBF::~cDBF(void)
{
    if (updated)
    {
        DBFFILE_HDR dbfhdr;
        time_t timenow;
        struct tm* tmnow;
        U8 tmp;

        /* Write out the terminator to be sure it exists */
        fseek(f, hdr.headerSize + hdr.recordsCnt * hdr.recordSize, SEEK_SET);
        tmp = 0x1a;
        fwrite(&tmp, 1, 1, f);

        /* Read in the file header */
        fseek(f, 0, SEEK_SET);
        fread(&dbfhdr, sizeof(DBFFILE_HDR), 1, f);

        /* Update the record counter */
        dbfhdr.records = hdr.recordsCnt;

        /* Update the date of last modification */
        timenow = time(NULL);
        tmnow = localtime(&timenow);
        dbfhdr.update_yr = tmnow->tm_yday - 1900;
        dbfhdr.update_mon = tmnow->tm_mon + 1; /* tm_mon is zero-based */
        dbfhdr.update_day = tmnow->tm_mday;

        /* Write out the file header */
        fseek(f, 0, SEEK_SET);
        fwrite(&dbfhdr, sizeof(DBFFILE_HDR), 1, f);
    }
    if (f)
        fclose(f);
    if (fmemo)
        fclose(fmemo);
    if (fields)
        free(fields);
} /* ::~cDBF */

/*****************************************************************************
 * cDBF::GetRecord - retrieves specified record.
 * ~~~~~~~~~~~~~~~   Assumes buffer has at least hdr.recordSize bytes.
 *                   Records are numbered starting with 0.
 *                   Memo fields are copied as is, i.e. position in memo file.
 */
eDBFStatus cDBF::GetRecord(U32 index, char* data)
{
    if (index != currField)
    {
        if (fseek(f, hdr.headerSize + index * hdr.recordSize, SEEK_SET))
        {
            currField = -1;
            return DBFE_SEEK_ERROR;
        }
    }
    size_t nBytesRead = fread(data, 1, hdr.recordSize, f);
    if (hdr.recordSize != nBytesRead)
    {
        currField = -1;
        if (!nBytesRead)
        {
            // Truncated file?
            hdr.recordsCnt = index;
        }
        return DBFE_READ_ERROR;
    }
    currField = index + 1;
    return DBFE_OK;
} /* cDBF::GetRecord */

/*****************************************************************************
 * cDBF::PutRecord - stores specified record.
 * ~~~~~~~~~~~~~~~   Assumes buffer has at least hdr.recordSize bytes.
 *                   Records are numbered starting with 0.
 *                   Memo fields are copied as is, i.e. position in memo file.
 */
eDBFStatus cDBF::PutRecord(U32 index, char* data)
{
    if (index != currField)
    {
        if (fseek(f, hdr.headerSize + index * hdr.recordSize, SEEK_SET))
        {
            currField = -1;
            return DBFE_SEEK_ERROR;
        }
    }
    if (1 != fwrite(data, hdr.recordSize, 1, f))
    {
        currField = -1;
        return DBFE_WRITE_ERROR;
    }
    if (index >= hdr.recordsCnt)
    {
        hdr.recordsCnt = index + 1;
    }
    updated = TRUE;
    currField = index + 1;
    return DBFE_OK;
} /* cDBF::PutRecord */

/*****************************************************************************
 * cDBF::AppendRecord - Appends record to the end of the file.
 * ~~~~~~~~~~~~~~~~~~
 *
 */
eDBFStatus cDBF::AppendRecord(char* data)
{
    if (hdr.recordsCnt != currField)
    {
        if (fseek(f, hdr.headerSize + hdr.recordsCnt * hdr.recordSize, SEEK_SET))
        {
            currField = -1;
            return DBFE_SEEK_ERROR;
        }
    }
    if (1 != fwrite(data, hdr.recordSize, 1, f))
    {
        currField = -1;
        return DBFE_WRITE_ERROR;
    }
    hdr.recordsCnt++;
    updated = TRUE;
    currField = hdr.recordsCnt;
    return DBFE_OK;
} /* cDBF::AppendRecord */

/*****************************************************************************
 * cDBF::GetMemoField - retrieves memo field or calculates buffer size needed
 * ~~~~~~~~~~~~~~~~~~   to hold given memo if buffer is NULL.
 *                      pos is a block index to the memo file. It should have
 *                      a value scanfed from a memo field.
 */
eDBFStatus cDBF::GetMemoField(U32 pos, char* buffer, U32 bufSize, U32* dataSize)
{
    union
    {
        U8 tmp8[8];
        U32 tmp32[2];
    } tmp;

    if (!fmemo) /* we do not have a memo file */
        return DBFE_NO_MEMO_FILE;

    fseek(fmemo, pos * memoBlockSize, SEEK_SET);

    switch (hdr.version)
    {
    case DBF_FOXPRO:
        if (1 != fread(&tmp, sizeof(tmp), 1, fmemo))
        {
            return DBFE_READ_ERROR;
        }
        if ((tmp.tmp32[0] != 0 /*picture*/) && (tmp.tmp32[0] != 1 << 24 /*memo*/) && (tmp.tmp32[0] != 2 << 24 /*object*/))
        {
            return DBFE_INVALID_BLOCK;
        }
        *dataSize = ((U32)tmp.tmp8[7]) + ((U32)tmp.tmp8[6] << 8) + ((U32)tmp.tmp8[5] << 16) + ((U32)tmp.tmp8[4] << 24);
        if (!buffer)
        {
            /* Only data size was requested */
            return DBFE_OK;
        }
        /* do not read more than buffer size bytes */
        *dataSize = min(bufSize, *dataSize);
        /* if not everything can be read, we adjust byte counter */
        *dataSize = (unsigned long)fread(buffer, 1, *dataSize, fmemo);
        break;

    case DBF_DBASE3:
        /* This weird old format never stores the actual size of memo items/
          We must read entire block and calculate the size.
          The end of data is denoted b 1A 1A sequence */
        {
            BOOL lookFor1a = FALSE; /* 1A 1A may be split into two blocks */

            if (!buffer)
            {
                /* Faking bufSize when enquiring needed buf size makes things simpler */
                bufSize = 0x7FFFFFFF;
            }
            while (bufSize > 0)
            {
                U8 tmp2[512];
                int i, read;
                BOOL found = FALSE;

                read = (int)fread(tmp2, 1, sizeof(tmp2), fmemo);
                if (!read)
                {
                    break;
                }
                if (lookFor1a && (tmp2[0] == 0x1A))
                {
                    /* 1A 1A split into two blocks */
                    /* Do not count the last byte of the previous block */
                    *dataSize--;
                    break;
                }
                if (buffer)
                {
                    U32 move = min(*dataSize, (U32)read);
                    memcpy(buffer, tmp2, move);
                    buffer += move;
                    *dataSize += move;
                }
                for (i = 0; i < read - 1; i++)
                {
                    if ((tmp2[i] == 0x1A) && (tmp2[i + 1] == 0x1A))
                    {
                        found = TRUE;
                        break;
                    }
                }
                if (found)
                {
                    *dataSize -= read - i;
                    break;
                }
                if (tmp2[read - 1] == 0x1A)
                {
                    lookFor1a = TRUE;
                }
            }
        }
        break;

    case DBF_DBASE4:
    case DBF_DBASE5: /* We hope dBase IV & V use the same memo file format */
        if (1 != fread(&tmp, sizeof(tmp), 1, fmemo))
        {
            return DBFE_READ_ERROR;
        }
        if (tmp.tmp32[0] != 0x0008FFFF)
        {
            return DBFE_INVALID_BLOCK;
        }
        *dataSize = tmp.tmp32[1];
        if (!buffer)
        {
            /* Only data size was requested */
            return DBFE_OK;
        }
        /* do not read more than buffer size bytes */
        *dataSize = min(bufSize, *dataSize);
        /* if not everything can be read, we adjust byte counter */
        *dataSize = (unsigned long)fread(buffer, 1, *dataSize, fmemo);
        break;

    default:
        /* We do not support anything else */
        return DBFE_NO_MEMO_FILE;
    }

    return DBFE_OK;
} /* cDBF::GetMemoField */

/* DBF.cpp */
/* end of file */
