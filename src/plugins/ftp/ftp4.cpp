// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// *********************************************************************************

char* HandleNULLStr(char* str)
{
    static char emptyBuff[] = "";
    return str == NULL ? emptyBuff : str;
}

void GetMyDocumentsPath(char* initDir)
{
    initDir[0] = 0;
    ITEMIDLIST* pidl = NULL;
    if (SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl) == NOERROR)
    {
        if (!SHGetPathFromIDList(pidl, initDir))
            initDir[0] = 0;
        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            alloc->Free(pidl);
            alloc->Release();
        }
    }
}

BOOL LoadStdColumnStrName(char* buf, int bufSize, int id)
{
    int resID = -1;
    switch (id)
    {
    case 0:
        resID = IDS_STC_STDNAME_NAME;
        break; // name
    case 1:
        resID = IDS_STC_STDNAME_EXT;
        break; // extension
    case 2 /* pri zmene zmenit i "2" v CFTPListingPluginDataInterface::CFTPListingPluginDataInterface()*/:
        resID = IDS_STC_STDNAME_SIZE;
        break; // size
    case 3 /* pri zmene zmenit i "3" v CFTPListingPluginDataInterface::CFTPListingPluginDataInterface()*/:
        resID = IDS_STC_STDNAME_DATE;
        break; // date
    case 4 /* pri zmene zmenit i "4" v CFTPListingPluginDataInterface::CFTPListingPluginDataInterface()*/:
        resID = IDS_STC_STDNAME_TIME;
        break; // time
    case 5:
        resID = IDS_STC_STDNAME_TYPE;
        break; // type
    case 6 /* pri zmene zmenit i "6" v CFTPListingPluginDataInterface::FindRightsColumn() */:
        resID = IDS_STC_STDNAME_RIGHTS;
        break; // rights
    case 7:
        resID = IDS_STC_STDNAME_USER;
        break; // user
    case 8:
        resID = IDS_STC_STDNAME_GROUP;
        break; // group
    case 9:
        resID = IDS_STC_STDNAME_DEVICE;
        break; // device
    case 10 /* pri zmene zmenit i "10" v CFTPListingPluginDataInterface::CFTPListingPluginDataInterface()*/:
        resID = IDS_STC_STDNAME_BLKSIZE;
        break; // block size
    case 11:
        resID = IDS_STC_STDNAME_ALLOCBLKS;
        break; // allocated blocks
    case 12:
        resID = IDS_STC_STDNAME_LINKTGT;
        break; // link target
    case 13:
        resID = IDS_STC_STDNAME_FORMAT;
        break; // format (used in IBM z/VM (CMS))
    case 14:
        resID = IDS_STC_STDNAME_LRECL;
        break; // logical record length (used in IBM z/VM (CMS))
    case 15:
        resID = IDS_STC_STDNAME_RECORDS;
        break; // records (used in IBM z/VM (CMS))
    case 16:
        resID = IDS_STC_STDNAME_FILEMODE;
        break; // filemode (used in IBM z/VM (CMS))
    case 17:
        resID = IDS_STC_STDNAME_VOLUME;
        break; // Volume (used on MVS)
    case 18:
        resID = IDS_STC_STDNAME_UNIT;
        break; // Unit (used on MVS)
    case 19:
        resID = IDS_STC_STDNAME_EXTENTS;
        break; // Extents (used on MVS)
    case 20:
        resID = IDS_STC_STDNAME_USED;
        break; // Used (used on MVS)
    case 21:
        resID = IDS_STC_STDNAME_RECFM;
        break; // Record Format (used on MVS)
    case 22 /* pri zmene zmenit i "22" v CFTPListingPluginDataInterface::CFTPListingPluginDataInterface()*/:
        resID = IDS_STC_STDNAME_BLKSZ;
        break; // Physical Block Length (used on MVS)
    case 23:
        resID = IDS_STC_STDNAME_DSORG;
        break; // Data Set Organization (used on MVS)
    case 24:
        resID = IDS_STC_STDNAME_VVMM;
        break; // VV.MM (used on MVS)
    case 25:
        resID = IDS_STC_STDNAME_CREATED;
        break; // Created (used on MVS)
    case 26:
        resID = IDS_STC_STDNAME_INIT;
        break; // Initial Size (used on MVS)
    case 27:
        resID = IDS_STC_STDNAME_MOD;
        break; // Mod (used on MVS)
    case 28:
        resID = IDS_STC_STDNAME_ID;
        break; // Id (used on MVS)
    case 29:
        resID = IDS_STC_STDNAME_TTR;
        break; // TTR (used on MVS)
    case 30:
        resID = IDS_STC_STDNAME_ALIASOF;
        break; // Alias-of (used on MVS)
    case 31:
        resID = IDS_STC_STDNAME_AC;
        break; // AC (used on MVS)
    case 32:
        resID = IDS_STC_STDNAME_ATTR;
        break; // Attributes (used on MVS)
    case 33:
        resID = IDS_STC_STDNAME_AMODE;
        break; // Amode (used on MVS)
    case 34:
        resID = IDS_STC_STDNAME_RMODE;
        break; // Rmode (used on MVS)
    case 35:
        resID = IDS_STC_STDNAME_CODE;
        break; // Code (used on Tandem)
    case 36:
        resID = IDS_STC_STDNAME_RWEP;
        break; // RWEP (used on Tandem)

        // pri pridavani je nutne zvysit hodnotu konstanty STC_STD_NAMES_COUNT !!!
    }
    if (bufSize > 0)
    {
        if (resID != -1)
            lstrcpyn(buf, LoadStr(resID), bufSize);
        else
        {
            TRACE_E("LoadStdColumnStrName: unknown ID: " << id);
            _snprintf_s(buf, bufSize, _TRUNCATE, "Unknown (%d)", id);
        }
    }
    return resID != -1;
}

BOOL LoadStdColumnStrDescr(char* buf, int bufSize, int id)
{
    int resID = -1;
    switch (id)
    {
    case 0:
        resID = IDS_STC_STDDESCR_NAME;
        break; // name
    case 1:
        resID = IDS_STC_STDDESCR_EXT;
        break; // extension
    case 2:
        resID = IDS_STC_STDDESCR_SIZE;
        break; // size
    case 3:
        resID = IDS_STC_STDDESCR_DATE;
        break; // date
    case 4:
        resID = IDS_STC_STDDESCR_TIME;
        break; // time
    case 5:
        resID = IDS_STC_STDDESCR_TYPE;
        break; // type
    case 6:
        resID = IDS_STC_STDDESCR_RIGHTS;
        break; // rights
    case 7:
        resID = IDS_STC_STDDESCR_USER;
        break; // user
    case 8:
        resID = IDS_STC_STDDESCR_GROUP;
        break; // group
    case 9:
        resID = IDS_STC_STDDESCR_DEVICE;
        break; // device
    case 10:
        resID = IDS_STC_STDDESCR_BLKSIZE;
        break; // block size
    case 11:
        resID = IDS_STC_STDDESCR_ALLOCBLKS;
        break; // allocated blocks
    case 12:
        resID = IDS_STC_STDDESCR_LINKTGT;
        break; // link target
    case 13:
        resID = IDS_STC_STDDESCR_FORMAT;
        break; // format (used in IBM z/VM (CMS))
    case 14:
        resID = IDS_STC_STDDESCR_LRECL;
        break; // logical record length (used in IBM z/VM (CMS))
    case 15:
        resID = IDS_STC_STDDESCR_RECORDS;
        break; // records (used in IBM z/VM (CMS))
    case 16:
        resID = IDS_STC_STDDESCR_FILEMODE;
        break; // filemode (used in IBM z/VM (CMS))
    case 17:
        resID = IDS_STC_STDDESCR_VOLUME;
        break; // Volume (used on MVS)
    case 18:
        resID = IDS_STC_STDDESCR_UNIT;
        break; // Unit (used on MVS)
    case 19:
        resID = IDS_STC_STDDESCR_EXTENTS;
        break; // Extents (used on MVS)
    case 20:
        resID = IDS_STC_STDDESCR_USED;
        break; // Used (used on MVS)
    case 21:
        resID = IDS_STC_STDDESCR_RECFM;
        break; // Record Format (used on MVS)
    case 22:
        resID = IDS_STC_STDDESCR_BLKSZ;
        break; // Physical Block Length (used on MVS)
    case 23:
        resID = IDS_STC_STDDESCR_DSORG;
        break; // Data Set Organization (used on MVS)
    case 24:
        resID = IDS_STC_STDDESCR_VVMM;
        break; // VV.MM (used on MVS)
    case 25:
        resID = IDS_STC_STDDESCR_CREATED;
        break; // Created (used on MVS)
    case 26:
        resID = IDS_STC_STDDESCR_INIT;
        break; // Initial Size (used on MVS)
    case 27:
        resID = IDS_STC_STDDESCR_MOD;
        break; // Mod (used on MVS)
    case 28:
        resID = IDS_STC_STDDESCR_ID;
        break; // Id (used on MVS)
    case 29:
        resID = IDS_STC_STDDESCR_TTR;
        break; // TTR (used on MVS)
    case 30:
        resID = IDS_STC_STDDESCR_ALIASOF;
        break; // Alias-of (used on MVS)
    case 31:
        resID = IDS_STC_STDDESCR_AC;
        break; // AC (used on MVS)
    case 32:
        resID = IDS_STC_STDDESCR_ATTR;
        break; // Attributes (used on MVS)
    case 33:
        resID = IDS_STC_STDDESCR_AMODE;
        break; // Amode (used on MVS)
    case 34:
        resID = IDS_STC_STDDESCR_RMODE;
        break; // Rmode (used on MVS)
    case 35:
        resID = IDS_STC_STDDESCR_CODE;
        break; // Code (used on Tandem)
    case 36:
        resID = IDS_STC_STDDESCR_RWEP;
        break; // RWEP (used on Tandem)

        // pri pridavani je nutne zvysit hodnotu konstanty STC_STD_NAMES_COUNT !!!
    }
    if (bufSize > 0)
    {
        if (resID != -1)
            lstrcpyn(buf, LoadStr(resID), bufSize);
        else
        {
            TRACE_E("LoadStdColumnStrDescr: unknown ID: " << id);
            _snprintf_s(buf, bufSize, _TRUNCATE, "Unknown (%d)", id);
        }
    }
    return resID != -1;
}

BOOL GetColumnTypeName(char* buf, int bufSize, CSrvTypeColumnTypes type)
{
    int resID = -1;
    switch (type)
    {
    case stctName:
        resID = IDS_STC_TYPE_NAME;
        break;
    case stctExt:
        resID = IDS_STC_TYPE_EXT;
        break;
    case stctSize:
        resID = IDS_STC_TYPE_SIZE;
        break;
    case stctDate:
        resID = IDS_STC_TYPE_DATE;
        break;
    case stctTime:
        resID = IDS_STC_TYPE_TIME;
        break;
    case stctType:
        resID = IDS_STC_TYPE_TYPE;
        break;
    case stctGeneralText:
        resID = IDS_STC_TYPE_ANYTEXT;
        break;
    case stctGeneralDate:
        resID = IDS_STC_TYPE_ANYDATE;
        break;
    case stctGeneralTime:
        resID = IDS_STC_TYPE_ANYTIME;
        break;
    case stctGeneralNumber:
        resID = IDS_STC_TYPE_ANYNUM;
        break;
    }
    if (bufSize > 0)
    {
        if (resID != -1)
            lstrcpyn(buf, LoadStr(resID), bufSize);
        else
        {
            TRACE_E("GetColumnTypeName: unknown type: " << (int)type);
            _snprintf_s(buf, bufSize, _TRUNCATE, "Unknown (%d)", (int)type);
        }
    }
    return resID != -1;
}

BOOL GetColumnEmptyValueForType(char* buf, int bufSize, CSrvTypeColumnTypes type)
{
    const char* s = NULL;
    switch (type)
    {
    case stctName:
    case stctExt:
    case stctType:
    case stctGeneralText:
    case stctGeneralDate:
    case stctGeneralTime:
    case stctGeneralNumber:
        s = "";
        break;

    case stctSize:
        s = "0";
        break;

    case stctDate:
        s = "1.1.1602";
        break;

    case stctTime:
        s = "0:00:00";
        break;
    }
    if (bufSize > 0)
    {
        if (s != NULL)
            lstrcpyn(buf, s, bufSize);
        else
        {
            TRACE_E("GetColumnEmptyValueForType: unknown type: " << (int)type);
            _snprintf_s(buf, bufSize, _TRUNCATE, "Unknown (%d)", (int)type);
        }
    }
    return s != NULL;
}

BOOL GetColumnEmptyValue(const char* empty, CSrvTypeColumnTypes type, CQuadWord* qwVal,
                         __int64* int64Val, SYSTEMTIME* stVal, BOOL skipDateCheck)
{
    if (empty != NULL && *empty == 0)
        empty = NULL;
    switch (type)
    {
    case stctName:
    case stctExt:
    case stctType:
    case stctGeneralText:
        return TRUE; // jakykoliv string je OK

    case stctSize: // unsigned int64
    {
        unsigned __int64 num = 0;
        if (empty != NULL)
        {
            if (*empty == '+')
                empty++;
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                num = num * 10 + (*empty++ - '0');
        }
        if (qwVal != NULL)
            qwVal->SetUI64(num);
        return empty == NULL || *empty == 0;
    }

    case stctGeneralNumber: // signed int64
    {
        __int64 num;
        if (empty != NULL)
        {
            num = 0;
            BOOL minus = FALSE;
            if (*empty == '+')
                empty++;
            else
            {
                if (*empty == '-')
                {
                    empty++;
                    minus = TRUE;
                }
            }
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                num = num * 10 + (*empty++ - '0');
            if (minus)
                num = -num;
        }
        else
            num = INT64_EMPTYNUMBER; // ma se zobrazit hodnota ""
        if (int64Val != NULL)
            *int64Val = num;
        return empty == NULL || *empty == 0 && num != INT64_EMPTYNUMBER;
    }

    case stctDate:
    case stctGeneralDate: // dd.mm.yyyy
    {
        int day = 0;
        int month = 0;
        int year = 0;
        if (empty != NULL)
        {
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                day = day * 10 + (*empty++ - '0');
            if (*empty == '.')
                empty++;
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                month = month * 10 + (*empty++ - '0');
            if (*empty == '.')
                empty++;
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                year = year * 10 + (*empty++ - '0');
        }
        else
        {
            if (type == stctGeneralDate) // pro stctGeneralDate se ma zobrazit ""
            {
                day = 0;
                skipDateCheck = TRUE;
            }
            else
                day = 1;
            month = 1;
            year = 1602;
        }
        if (empty == NULL || *empty == 0)
        {
            SYSTEMTIME st;
            if (!skipDateCheck)
            {
                memset(&st, 0, sizeof(st));
                st.wYear = year;
                st.wMonth = month;
                st.wDay = day;
            }
            FILETIME ft;
            if (skipDateCheck || year >= 1602 && SystemTimeToFileTime(&st, &ft)) // test datumu
            {
                if (stVal != NULL)
                {
                    stVal->wYear = year;
                    stVal->wMonth = month;
                    stVal->wDay = day;
                    stVal->wDayOfWeek = 0;
                }
                return TRUE; // uspech, mame datum
            }
        }
        return FALSE; // spatna syntaxe nebo spatne datum
    }

    case stctTime:
    case stctGeneralTime: // hh:mm:ss
    {
        int hours = 0;
        int minutes = 0;
        int secs = 0;
        BOOL skipTimeCheck = FALSE;
        if (empty != NULL)
        {
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                hours = hours * 10 + (*empty++ - '0');
            if (*empty == ':')
                empty++;
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                minutes = minutes * 10 + (*empty++ - '0');
            if (*empty == ':')
                empty++;
            while (*empty != 0 && *empty >= '0' && *empty <= '9')
                secs = secs * 10 + (*empty++ - '0');
        }
        else
        {
            if (type == stctGeneralTime) // pro stctGeneralTime se ma zobrazit ""
            {
                skipTimeCheck = TRUE;
                hours = 24;
            }
        }
        if (empty == NULL || *empty == 0)
        {
            if (skipTimeCheck ||
                hours >= 0 && hours < 24 &&
                    minutes >= 0 && minutes < 60 &&
                    secs >= 0 && secs < 60)
            {
                if (stVal != NULL)
                {
                    stVal->wHour = hours;
                    stVal->wMinute = minutes;
                    stVal->wSecond = secs;
                    stVal->wMilliseconds = 0;
                }
                return TRUE; // uspech, mame cas
            }
        }
        return FALSE; // spatna syntaxe nebo spatny cas
    }
    }
    TRACE_E("Unknown server type column type!");
    return FALSE;
}
