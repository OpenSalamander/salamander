// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// Attributes
const char* FILTERCRITERIA_ATTRIBUTESMASK_REG = "Attributes Mask";
const char* FILTERCRITERIA_ATTRIBUTESVALUE_REG = "Attributes Value";
// Size Min/Max
const char* FILTERCRITERIA_USEMINSIZE_REG = "UseMinSize";
const char* FILTERCRITERIA_MINSIZELO_REG = "MinSizeLo";
const char* FILTERCRITERIA_MINSIZEHI_REG = "MinSizeHi";
const char* FILTERCRITERIA_MINSIZEUNITS_REG = "MinSizeUnits";
const char* FILTERCRITERIA_USEMAXSIZE_REG = "UseMaxSize";
const char* FILTERCRITERIA_MAXSIZELO_REG = "MaxSizeLo";
const char* FILTERCRITERIA_MAXSIZEHI_REG = "MaxSizeHi";
const char* FILTERCRITERIA_MAXSIZEUNITS_REG = "MaxSizeUnits";
// Date & Time
const char* FILTERCRITERIA_TIMEMODE_REG = "TimeMode";
const char* FILTERCRITERIA_DURINGTIMELO_REG = "DuringTimeLo";
const char* FILTERCRITERIA_DURINGTIMEHI_REG = "DuringTimeHi";
const char* FILTERCRITERIA_DURINGUNITS_REG = "DuringUnits";
const char* FILTERCRITERIA_USEFROMDATE_REG = "UseFromDate";
const char* FILTERCRITERIA_USEFROMTIME_REG = "UseFromTime";
const char* FILTERCRITERIA_FROMLO_REG = "FromLo";
const char* FILTERCRITERIA_FROMHI_REG = "FromHi";
const char* FILTERCRITERIA_USETODATE_REG = "UseToDate";
const char* FILTERCRITERIA_USETOTIME_REG = "UseToTime";
const char* FILTERCRITERIA_TOLO_REG = "ToLo";
const char* FILTERCRITERIA_TOHI_REG = "ToHi";

// nasledujici promenne jsme pouzivali do Altap Salamander 2.5,
// kde jsme presli na CFilterCriteria a jeho Save/Load
const char* OLD_FINDOPTIONSITEM_ARCHIVE_REG = "Archive";
const char* OLD_FINDOPTIONSITEM_READONLY_REG = "ReadOnly";
const char* OLD_FINDOPTIONSITEM_HIDDEN_REG = "Hidden";
const char* OLD_FINDOPTIONSITEM_SYSTEM_REG = "System";
const char* OLD_FINDOPTIONSITEM_COMPRESSED_REG = "Compressed";
const char* OLD_FINDOPTIONSITEM_DIRECTORY_REG = "Directory";
const char* OLD_FINDOPTIONSITEM_SIZEACTION_REG = "SizeAction";
const char* OLD_FINDOPTIONSITEM_SIZELO_REG = "SizeLo";
const char* OLD_FINDOPTIONSITEM_SIZEHI_REG = "SizeHi";
const char* OLD_FINDOPTIONSITEM_DATEACTION_REG = "DateAction";
const char* OLD_FINDOPTIONSITEM_DAY_REG = "Day";
const char* OLD_FINDOPTIONSITEM_MONTH_REG = "Month";
const char* OLD_FINDOPTIONSITEM_YEAR_REG = "Year";
const char* OLD_FINDOPTIONSITEM_TIMEACTION_REG = "TimeAction";
const char* OLD_FINDOPTIONSITEM_HOUR_REG = "Hour";
const char* OLD_FINDOPTIONSITEM_MINUTE_REG = "Minute";
const char* OLD_FINDOPTIONSITEM_SECOND_REG = "Second";

// prestupny rok
#define IsLeapYear(_yr) ((!((_yr) % 400) || ((_yr) % 100) && !((_yr) % 4)) ? TRUE : FALSE)

// pocty dnu v mesicich
const static WORD DaysPerMonth[] =
    {
        31, //  JAN
        28, //  FEB
        31, //  MAR
        30, //  APR
        31, //  MAY
        30, //  JUN
        31, //  JUL
        31, //  AUG
        30, //  SEP
        31, //  OCT
        30, //  NOV
        31  //  DEC
};

//****************************************************************************
//
// CFilterCriteria
//

CFilterCriteria::CFilterCriteria()
{
    Reset();
}

void CFilterCriteria::Reset()
{
    // Attributes
    AttributesMask = 0; // 0 -> indeterminate checkbox state
    AttributesValue = 0;

    // Size Min/Max
    UseMinSize = FALSE;
    MinSize.Set(1, 0);
    MinSizeUnits = fcsuKB;
    UseMaxSize = FALSE;
    MaxSize.Set(1, 0);
    MaxSizeUnits = fcsuKB;

    // Date & Time
    TimeMode = fctmIgnore;
    DuringTime.Set(1, 0);
    DuringTimeUnits = fctuDays;
    UseFromDate = TRUE;
    UseFromTime = FALSE;
    UseToDate = TRUE;
    UseToTime = FALSE;

    // 'From' nastavime den zpet, 'To' na aktualni datum
    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, (FILETIME*)&To);
    From = To - (unsigned __int64)10000000 * 60 * 60 * 24; // jeden den zpet

    // interni promenne
    NeedPrepare = FALSE; // za uvedene situace neni treba volat PrepareForTest
    UseMinTime = FALSE;
    UseMaxTime = FALSE;
}

void SizeToBytes(CQuadWord* out, const CQuadWord* in, CFilterCriteriaSizeUnitsEnum units)
{
    switch (units)
    {
    case fcsuBytes:
        *out = *in;
        break;
    case fcsuKB:
        out->SetUI64(in->Value * (unsigned __int64)1024);
        break;
    case fcsuMB:
        out->SetUI64(in->Value * (unsigned __int64)1024 * 1024);
        break;
    case fcsuGB:
        out->SetUI64(in->Value * (unsigned __int64)1024 * 1024 * 1024);
        break;
    case fcsuTB:
        out->SetUI64(in->Value * (unsigned __int64)1024 * 1024 * 1024 * 1024);
        break;
    case fcsuPB:
        out->SetUI64(in->Value * (unsigned __int64)1024 * 1024 * 1024 * 1024 * 1024);
        break;
    case fcsuEB:
        out->SetUI64(in->Value * (unsigned __int64)1024 * 1024 * 1024 * 1024 * 1024 * 1024);
        break;
    default:
    {
        TRACE_E("Unknown units=" << units);
    }
    }
}

// vraci maximalni cislo, ktere se po prevodu na bajty jeste vejde do CQuadWord
void MaxSizeForUnits(CQuadWord* out, CFilterCriteriaSizeUnitsEnum units)
{
    switch (units)
    {
    case fcsuBytes:
        out->SetUI64((unsigned __int64)0xffffffffffffffff);
        break;
    case fcsuKB:
        out->SetUI64((unsigned __int64)0xffffffffffffffff / ((unsigned __int64)1024));
        break;
    case fcsuMB:
        out->SetUI64((unsigned __int64)0xffffffffffffffff / ((unsigned __int64)1024 * 1024));
        break;
    case fcsuGB:
        out->SetUI64((unsigned __int64)0xffffffffffffffff / ((unsigned __int64)1024 * 1024 * 1024));
        break;
    case fcsuTB:
        out->SetUI64((unsigned __int64)0xffffffffffffffff / ((unsigned __int64)1024 * 1024 * 1024 * 1024));
        break;
    case fcsuPB:
        out->SetUI64((unsigned __int64)0xffffffffffffffff / ((unsigned __int64)1024 * 1024 * 1024 * 1024 * 1024));
        break;
    case fcsuEB:
        out->SetUI64((unsigned __int64)0xffffffffffffffff / ((unsigned __int64)1024 * 1024 * 1024 * 1024 * 1024 * 1024));
        break;
    default:
    {
        TRACE_E("Unknown units=" << units);
    }
    }
}

// vraci maximalni cislo, ktere se po prevodu na vteriny jeste vejde do unsigned __int64
void MaxTimeForUnits(unsigned __int64* out, CFilterCriteriaTimeUnitsEnum units)
{
    // budeme umet hledat jen 300 let zpet :)
    switch (units)
    {
    case fctuSeconds:
        *out = (unsigned __int64)300 * 365 * 24 * 60 * 60;
        break;
    case fctuMinutes:
        *out = (unsigned __int64)300 * 365 * 24 * 60;
        break;
    case fctuHours:
        *out = (unsigned __int64)300 * 365 * 24;
        break;
    case fctuDays:
        *out = (unsigned __int64)300 * 365;
        break;
    case fctuWeeks:
        *out = (unsigned __int64)300 * 52;
        break;
    case fctuMonths:
        *out = (unsigned __int64)300 * 12;
        break;
    case fctuYears:
        *out = (unsigned __int64)300;
        break;
    default:
    {
        TRACE_E("Unknown units=" << units);
    }
    }
}

void CFilterCriteria::PrepareForTest()
{
    UseMinTime = FALSE;
    UseMaxTime = FALSE;

    // urcime velikosti v bajtech
    if (UseMinSize)
        SizeToBytes(&MinSizeBytes, &MinSize, MinSizeUnits);

    if (UseMaxSize)
        SizeToBytes(&MaxSizeBytes, &MaxSize, MaxSizeUnits);

    if (TimeMode == fctmDuring)
    {
        SYSTEMTIME st; // st muze byt meneno, viz nulovani hodin, minut a vterin
        GetLocalTime(&st);
        // den v tydnu je redundantni informace, nebudeme s ni pracovat
        st.wDayOfWeek = 0;
        SYSTEMTIME stCurrent = st; // aktualni cas, ktery nebudeme modifikovat

        unsigned __int64 offset = 0;
        switch (DuringTimeUnits)
        {
        case fctuSeconds:
        {
            offset = DuringTime.Value * (unsigned __int64)10000000;
            break;
        }

        case fctuMinutes:
        {
            offset = DuringTime.Value * (unsigned __int64)10000000 * 60;
            break;
        }

        case fctuHours:
        {
            offset = DuringTime.Value * (unsigned __int64)10000000 * 60 * 60;
            break;
        }

        case fctuDays:
        case fctuWeeks:
        {
            // v pripade dni a tydnu nezalezi na casovem udaji, ridime se pouze datumem
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
            st.wMilliseconds = 0;

            unsigned __int64 days;
            if (DuringTimeUnits == fctuDays)
                days = DuringTime.Value; // dny
            else
                days = DuringTime.Value * 7; // tydny

            offset = days * (unsigned __int64)10000000 * 60 * 60 * 24;

            break;
        }

        case fctuMonths:
        case fctuYears:
        {
            // v pripade mesicu a roku nezalezi na casovem udaji, ridime se pouze datumem
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
            st.wMilliseconds = 0;

            if (DuringTimeUnits == fctuMonths)
            {
                unsigned __int64 i;
                for (i = 0; i < DuringTime.Value; i++)
                {
                    // vratime se o mesic zpet
                    if (st.wMonth > 1)
                        st.wMonth--;
                    else
                    {
                        // leden -> prosinec && rok zpet
                        st.wMonth = 12;
                        st.wYear--;
                    }
                }
            }
            else
                st.wYear = st.wYear - (WORD)DuringTime.Value;

            // pokud den neexistuje, vracime se k existujicimu
            if (st.wMonth >= 1 && st.wMonth <= 12)
            {
                int maxDay = DaysPerMonth[st.wMonth - 1];
                if (IsLeapYear(st.wYear) && st.wMonth == 2)
                    maxDay++; // na prestupny rok ma unor o den vic
                if (st.wDay > maxDay)
                    st.wDay = maxDay;
            }
            break;
        }
        }

        if (SystemTimeToFileTime(&st, (FILETIME*)&MinTime))
        {
            MinTime -= offset;
            UseMinTime = TRUE;
        }
        if (SystemTimeToFileTime(&stCurrent, (FILETIME*)&MaxTime))
        {
            // od 2.52b1 nebereme u "Modified during" budouci soubry, viz hlaseni na foru
            // https://forum.altap.cz/viewtopic.php?t=2818
            UseMaxTime = TRUE;
        }
    }

    if (TimeMode == fctmFromTo)
    {
        if (UseFromDate)
        {
            SYSTEMTIME st;
            if (FileTimeToSystemTime((FILETIME*)&From, &st))
            {
                st.wMilliseconds = 0; // control vraci podezrele hodnoty, orezeme je
                if (!UseFromTime)
                {
                    st.wHour = 0;
                    st.wMinute = 0;
                    st.wSecond = 0;
                }
                if (SystemTimeToFileTime(&st, (FILETIME*)&MinTime))
                {
                    UseMinTime = TRUE;
                }
            }
        }

        if (UseToDate)
        {
            SYSTEMTIME st;
            if (FileTimeToSystemTime((FILETIME*)&To, &st))
            {
                st.wMilliseconds = 0; // control vraci podezrele hodnoty, orezeme je
                if (!UseToTime)
                {
                    st.wHour = 23;
                    st.wMinute = 59;
                    st.wSecond = 59;
                }
                if (SystemTimeToFileTime(&st, (FILETIME*)&MaxTime))
                {
                    // chceme byt opravdu maximalni cas, nalepime se uplne na konec intervalu
                    // pri rozlisovaci schopnosti FILETIME
                    MaxTime += 9999999; // temer jedna vterina

                    UseMaxTime = TRUE;
                }
            }
        }
    }

    NeedPrepare = FALSE;
}

BOOL CFilterCriteria::Test(DWORD attributes, const CQuadWord* size, const FILETIME* modified)
{
    if (NeedPrepare)
        TRACE_E("You must call PrepareForTest before Test method is called");

    // Attributes
    BOOL ok = ((attributes & AttributesMask) == (AttributesValue & AttributesMask));

    // Size Min/Max
    if (ok && (UseMinSize || UseMaxSize))
    {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            // jde o adresar, vypadneme
            ok = FALSE;
        }
        else
        {
            // jedna se o soubor, muzeme provnat velikost
            if (UseMinSize)
                ok = *size >= MinSizeBytes;
            if (ok && UseMaxSize)
                ok = *size <= MaxSizeBytes;
        }
    }

    // Date & Time
    if (ok && TimeMode != fctmIgnore)
    {
        unsigned __int64 time;
        if (!FileTimeToLocalFileTime(modified, (FILETIME*)&time))
            time = 0; // pri chybe dame nulu (at je chovani deterministicke)

        if (UseMinTime)
            ok = time >= MinTime;

        if (ok && UseMaxTime)
            ok = time <= MaxTime;
    }

    return ok;
}

BOOL CFilterCriteria::GetAdvancedDescription(char* buffer, int maxLen, BOOL& dirty)
{
    char buff[300];

    int count = 1;

    PrepareForTest();

    lstrcpy(buff, LoadStr(IDS_FFA_OPTIONS));

    if (AttributesMask != 0)
    {
        lstrcat(buff, LoadStr(IDS_FFA_ATTRIBUTES));
        lstrcat(buff, ": ");
        if (AttributesMask & FILE_ATTRIBUTE_ARCHIVE)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_ARCHIVE ? "+A" : "-A");
        if (AttributesMask & FILE_ATTRIBUTE_READONLY)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_READONLY ? "+R" : "-R");
        if (AttributesMask & FILE_ATTRIBUTE_HIDDEN)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_HIDDEN ? "+H" : "-H");
        if (AttributesMask & FILE_ATTRIBUTE_SYSTEM)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_SYSTEM ? "+S" : "-S");
        if (AttributesMask & FILE_ATTRIBUTE_COMPRESSED)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_COMPRESSED ? "+C" : "-C");
        if (AttributesMask & FILE_ATTRIBUTE_ENCRYPTED)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_ENCRYPTED ? "+E" : "-E");
        if (AttributesMask & FILE_ATTRIBUTE_DIRECTORY)
            lstrcat(buff, AttributesValue & FILE_ATTRIBUTE_DIRECTORY ? "+D" : "-D");
        count++;
    }

    if (UseMinSize || UseMaxSize)
    {
        if (count > 1)
            lstrcat(buff, ", ");
        lstrcat(buff, LoadStr(IDS_FFA_SIZE));
        count++;
    }

    if (TimeMode != fctmIgnore)
    {
        if (count > 1)
            lstrcat(buff, ", ");
        if (TimeMode == fctmFromTo || DuringTimeUnits >= fctuDays)
            lstrcat(buff, LoadStr(IDS_FFA_DATE));
        else
            lstrcat(buff, LoadStr(IDS_FFA_TIME));
        count++;
    }

    if (TimeMode == fctmFromTo &&
        (UseFromTime || UseToTime))
    {
        if (count > 1)
            lstrcat(buff, ", ");
        lstrcat(buff, LoadStr(IDS_FFA_TIME));
        count++;
    }

    if (count == 1)
        lstrcpy(buff, LoadStr(IDS_FFA_NONE));

    lstrcpyn(buffer, buff, maxLen);
    dirty = count > 1;
    return maxLen > lstrlen(buff) + 1;
}

BOOL CFilterCriteria::Save(HKEY hKey)
{
    // optimalizace na velikost v Registry: ukladame pouze "non-default hodnoty";
    // pred ukladanim je proto treba promazat klic, do ktereho se budeme ukladat
    CFilterCriteria def;

    // Attributes
    if (AttributesMask != def.AttributesMask)
        SetValue(hKey, FILTERCRITERIA_ATTRIBUTESMASK_REG, REG_DWORD, &AttributesMask, sizeof(DWORD));
    if (AttributesValue != def.AttributesValue)
        SetValue(hKey, FILTERCRITERIA_ATTRIBUTESVALUE_REG, REG_DWORD, &AttributesValue, sizeof(DWORD));

    // Size Min/Max
    if (UseMinSize != def.UseMinSize)
        SetValue(hKey, FILTERCRITERIA_USEMINSIZE_REG, REG_DWORD, &UseMinSize, sizeof(DWORD));
    if (MinSize != def.MinSize)
    {
        SetValue(hKey, FILTERCRITERIA_MINSIZELO_REG, REG_DWORD, &MinSize.LoDWord, sizeof(DWORD));
        SetValue(hKey, FILTERCRITERIA_MINSIZEHI_REG, REG_DWORD, &MinSize.HiDWord, sizeof(DWORD));
    }
    if (MinSizeUnits != def.MinSizeUnits)
        SetValue(hKey, FILTERCRITERIA_MINSIZEUNITS_REG, REG_DWORD, &MinSizeUnits, sizeof(DWORD));
    if (UseMaxSize != def.UseMaxSize)
        SetValue(hKey, FILTERCRITERIA_USEMAXSIZE_REG, REG_DWORD, &UseMaxSize, sizeof(DWORD));
    if (MaxSize != def.MaxSize)
    {
        SetValue(hKey, FILTERCRITERIA_MAXSIZELO_REG, REG_DWORD, &MaxSize.LoDWord, sizeof(DWORD));
        SetValue(hKey, FILTERCRITERIA_MAXSIZEHI_REG, REG_DWORD, &MaxSize.HiDWord, sizeof(DWORD));
    }
    if (MaxSizeUnits != def.MaxSizeUnits)
        SetValue(hKey, FILTERCRITERIA_MAXSIZEUNITS_REG, REG_DWORD, &MaxSizeUnits, sizeof(DWORD));

    // Date & Time
    if (TimeMode != def.TimeMode)
        SetValue(hKey, FILTERCRITERIA_TIMEMODE_REG, REG_DWORD, &TimeMode, sizeof(DWORD));
    if (DuringTime != def.DuringTime)
    {
        SetValue(hKey, FILTERCRITERIA_DURINGTIMELO_REG, REG_DWORD, &DuringTime.LoDWord, sizeof(DWORD));
        SetValue(hKey, FILTERCRITERIA_DURINGTIMEHI_REG, REG_DWORD, &DuringTime.HiDWord, sizeof(DWORD));
    }
    if (DuringTimeUnits != def.DuringTimeUnits)
        SetValue(hKey, FILTERCRITERIA_DURINGUNITS_REG, REG_DWORD, &DuringTimeUnits, sizeof(DWORD));
    if (UseFromDate != def.UseFromDate)
        SetValue(hKey, FILTERCRITERIA_USEFROMDATE_REG, REG_DWORD, &UseFromDate, sizeof(DWORD));
    if (UseFromTime != def.UseFromTime)
        SetValue(hKey, FILTERCRITERIA_USEFROMTIME_REG, REG_DWORD, &UseFromTime, sizeof(DWORD));

    // poznamka: od 2.53 budeme "zapominat" casy v disablenych FROM/TO controlech (viz podminka TimeMode == fctmFromTo)
    // pokud si uzivatele vyzadaji navrat ke staremu chovani, mohly bychom defaultne vypinat checkboxy v Date controlech,
    // cimz by nebyla splnena podminka UseFromDate/UseToDate; checkbox bychom zapnuli az ve chvili, kdy by user control enabloval radiakem
    if (From != def.From && TimeMode == fctmFromTo && (UseFromDate || UseFromTime)) // casy nema smysl ukladat, pokud se nepouzivaji (controly pak dosadi aktualni cas)
    {
        SetValue(hKey, FILTERCRITERIA_FROMLO_REG, REG_DWORD, &(((FILETIME*)&From)->dwLowDateTime), sizeof(DWORD));
        SetValue(hKey, FILTERCRITERIA_FROMHI_REG, REG_DWORD, &(((FILETIME*)&From)->dwHighDateTime), sizeof(DWORD));
    }
    if (UseToDate != def.UseToDate)
        SetValue(hKey, FILTERCRITERIA_USETODATE_REG, REG_DWORD, &UseToDate, sizeof(DWORD));
    if (UseToTime != def.UseToTime)
        SetValue(hKey, FILTERCRITERIA_USETOTIME_REG, REG_DWORD, &UseToTime, sizeof(DWORD));
    if (To != def.To && TimeMode == fctmFromTo && (UseToDate || UseToTime)) // casy nema smysl ukladat, pokud se nepouzivaji (controly pak dosadi aktualni cas)
    {
        SetValue(hKey, FILTERCRITERIA_TOLO_REG, REG_DWORD, &(((FILETIME*)&To)->dwLowDateTime), sizeof(DWORD));
        SetValue(hKey, FILTERCRITERIA_TOHI_REG, REG_DWORD, &(((FILETIME*)&To)->dwHighDateTime), sizeof(DWORD));
    }
    return TRUE;
}

BOOL CFilterCriteria::Load(HKEY hKey)
{
    // Attributes
    GetValue(hKey, FILTERCRITERIA_ATTRIBUTESMASK_REG, REG_DWORD, &AttributesMask, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_ATTRIBUTESVALUE_REG, REG_DWORD, &AttributesValue, sizeof(DWORD));

    // Size Min/Max
    GetValue(hKey, FILTERCRITERIA_USEMINSIZE_REG, REG_DWORD, &UseMinSize, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_MINSIZELO_REG, REG_DWORD, &MinSize.LoDWord, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_MINSIZEHI_REG, REG_DWORD, &MinSize.HiDWord, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_MINSIZEUNITS_REG, REG_DWORD, &MinSizeUnits, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_USEMAXSIZE_REG, REG_DWORD, &UseMaxSize, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_MAXSIZELO_REG, REG_DWORD, &MaxSize.LoDWord, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_MAXSIZEHI_REG, REG_DWORD, &MaxSize.HiDWord, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_MAXSIZEUNITS_REG, REG_DWORD, &MaxSizeUnits, sizeof(DWORD));
    // Date & Time
    GetValue(hKey, FILTERCRITERIA_TIMEMODE_REG, REG_DWORD, &TimeMode, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_DURINGTIMELO_REG, REG_DWORD, &DuringTime.LoDWord, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_DURINGTIMEHI_REG, REG_DWORD, &DuringTime.HiDWord, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_DURINGUNITS_REG, REG_DWORD, &DuringTimeUnits, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_USEFROMDATE_REG, REG_DWORD, &UseFromDate, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_USEFROMTIME_REG, REG_DWORD, &UseFromTime, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_FROMLO_REG, REG_DWORD, &(((FILETIME*)&From)->dwLowDateTime), sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_FROMHI_REG, REG_DWORD, &(((FILETIME*)&From)->dwHighDateTime), sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_USETODATE_REG, REG_DWORD, &UseToDate, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_USETOTIME_REG, REG_DWORD, &UseToTime, sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_TOLO_REG, REG_DWORD, &(((FILETIME*)&To)->dwLowDateTime), sizeof(DWORD));
    GetValue(hKey, FILTERCRITERIA_TOHI_REG, REG_DWORD, &(((FILETIME*)&To)->dwHighDateTime), sizeof(DWORD));

    NeedPrepare = TRUE;

    return TRUE;
}

BOOL CFilterCriteria::LoadOld(HKEY hKey)
{
    // attributes
    int archive = 2;
    int readOnly = 2;
    int hidden = 2;
    int system = 2;
    int compressed = 2;
    int directory = 2;

    GetValue(hKey, OLD_FINDOPTIONSITEM_ARCHIVE_REG, REG_DWORD, &archive, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_READONLY_REG, REG_DWORD, &readOnly, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_HIDDEN_REG, REG_DWORD, &hidden, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_SYSTEM_REG, REG_DWORD, &system, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_COMPRESSED_REG, REG_DWORD, &compressed, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_DIRECTORY_REG, REG_DWORD, &directory, sizeof(DWORD));

    AttributesMask = 0;
    AttributesValue = 0;

    if (archive != 2)
    {
        AttributesMask |= FILE_ATTRIBUTE_ARCHIVE;
        if (archive == 1)
            AttributesValue |= FILE_ATTRIBUTE_ARCHIVE;
    }

    if (readOnly != 2)
    {
        AttributesMask |= FILE_ATTRIBUTE_READONLY;
        if (readOnly == 1)
            AttributesValue |= FILE_ATTRIBUTE_READONLY;
    }

    if (hidden != 2)
    {
        AttributesMask |= FILE_ATTRIBUTE_HIDDEN;
        if (hidden == 1)
            AttributesValue |= FILE_ATTRIBUTE_HIDDEN;
    }

    if (system != 2)
    {
        AttributesMask |= FILE_ATTRIBUTE_SYSTEM;
        if (system == 1)
            AttributesValue |= FILE_ATTRIBUTE_SYSTEM;
    }

    if (compressed != 2)
    {
        AttributesMask |= FILE_ATTRIBUTE_COMPRESSED;
        if (compressed == 1)
            AttributesValue |= FILE_ATTRIBUTE_COMPRESSED;
    }

    if (directory != 2)
    {
        AttributesMask |= FILE_ATTRIBUTE_DIRECTORY;
        if (directory == 1)
            AttributesValue |= FILE_ATTRIBUTE_DIRECTORY;
    }

    // size
    UseMinSize = FALSE;
    UseMaxSize = FALSE;

    int sizeAction = 0;
    CQuadWord size = CQuadWord(0, 0);
    GetValue(hKey, OLD_FINDOPTIONSITEM_SIZEACTION_REG, REG_DWORD, &sizeAction, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_SIZELO_REG, REG_DWORD, &size.LoDWord, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_SIZEHI_REG, REG_DWORD, &size.HiDWord, sizeof(DWORD));

    switch (sizeAction) // size:
    {
    case 1: // greater than
    {
        UseMinSize = TRUE;
        MinSize.SetUI64(size.Value + (unsigned __int64)1);
        MinSizeUnits = fcsuBytes;
        break;
    }

    case 2: // greater or equal to
    {
        UseMinSize = TRUE;
        MinSize = size;
        MinSizeUnits = fcsuBytes;
        break;
    }

    case 3: // equal to
    {
        UseMinSize = TRUE;
        MinSize = size;
        MinSizeUnits = fcsuBytes;
        UseMaxSize = TRUE;
        MaxSize = size;
        MaxSizeUnits = fcsuBytes;
        break;
    }

    case 4: // smaller or equal to
    {
        UseMaxSize = TRUE;
        MaxSize = size;
        MaxSizeUnits = fcsuBytes;
        break;
    }

    case 5: // smaller than
    {
        UseMaxSize = TRUE;
        if (size.Value > (unsigned __int64)0)
            MaxSize.SetUI64(size.Value - (unsigned __int64)1);
        MaxSizeUnits = fcsuBytes;
        break;
    }
    }

    // date/time
    UseFromDate = FALSE;
    UseToDate = FALSE;
    int dateAction;
    int day;
    int month;
    int year;
    int timeAction;
    int hour;
    int minute;
    int second;
    GetValue(hKey, OLD_FINDOPTIONSITEM_DATEACTION_REG, REG_DWORD, &dateAction, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_DAY_REG, REG_DWORD, &day, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_MONTH_REG, REG_DWORD, &month, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_YEAR_REG, REG_DWORD, &year, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_TIMEACTION_REG, REG_DWORD, &timeAction, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_HOUR_REG, REG_DWORD, &hour, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_MINUTE_REG, REG_DWORD, &minute, sizeof(DWORD));
    GetValue(hKey, OLD_FINDOPTIONSITEM_SECOND_REG, REG_DWORD, &second, sizeof(DWORD));

    SYSTEMTIME st;
    ZeroMemory(&st, sizeof(st));
    st.wYear = year;
    st.wMonth = month;
    st.wDay = day;
    st.wHour = hour;
    st.wMinute = minute;
    st.wSecond = second;

    switch (dateAction)
    {
    case 1: // older than
    case 2: // older or equal to
    {
        if (timeAction == 0)
        {
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
        }
        if (SystemTimeToFileTime(&st, (FILETIME*)&To))
        {
            UseToDate = TRUE;
            if (timeAction == 1)
                UseToTime = TRUE;
            if (dateAction == 1) // older than
            {
                if (timeAction == 1)
                    To -= (unsigned __int64)10000000; // vterina
                else
                    To -= (unsigned __int64)10000000 * 60 * 60 * 24; // den
            }
        }
        break;
    }

    case 3: // equal to
    {
        if (timeAction == 0)
        {
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
        }
        if (SystemTimeToFileTime(&st, (FILETIME*)&From))
        {
            To = From;
            UseFromDate = TRUE;
            UseToDate = TRUE;

            if (timeAction == 1)
            {
                UseFromTime = TRUE;
                UseToTime = TRUE;
            }
        }
        break;
    }

    case 4: // newer or equal to
    case 5: // newer than
    {
        if (timeAction == 0)
        {
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
        }
        if (SystemTimeToFileTime(&st, (FILETIME*)&From))
        {
            UseFromDate = TRUE;
            if (timeAction == 1)
                UseFromTime = TRUE;
            if (dateAction == 5) // newer than
            {
                if (timeAction == 1)
                    From += (unsigned __int64)10000000; // vterina
                else
                    From += (unsigned __int64)10000000 * 60 * 60 * 24; // den
            }
        }
        break;
    }
    }

    if (UseFromDate || UseToDate)
        TimeMode = fctmFromTo;

    NeedPrepare = TRUE;
    return TRUE;
}

//****************************************************************************
//
// CFilterCriteriaDialog
//

CFilterCriteriaDialog::CFilterCriteriaDialog(HWND hParent, CFilterCriteria* data, BOOL enableDirectory)
    : CCommonDialog(HLanguage, IDD_FINDADVANCED, IDD_FINDADVANCED, hParent)
{
    Data = data;
    EnableDirectory = enableDirectory;
}

BOOL QuadWordEditLineTransfer(CTransferInfo* ti, int ctrlID, CQuadWord& value)
{
    BOOL ret = TRUE;
    HWND HWindow;
    char buff[50];
    if (ti->GetControl(HWindow, ctrlID))
    {
        switch (ti->Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, 21, 0);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)_ui64toa(value.Value, buff, 10));
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, 22, (LPARAM)buff);
            value.Value = StrToUInt64(buff, (int)strlen(buff));
            break;
        }
        }
    }
    return ret;
}

BOOL AttributeCheckBox(CTransferInfo* ti, int ctrlID, DWORD attribute, DWORD* attrMask, DWORD* attrValue)
{
    BOOL ret = TRUE;
    HWND HWindow;
    if (ti->GetControl(HWindow, ctrlID))
    {
        switch (ti->Type)
        {
        case ttDataToWindow:
        {
            int value = 2; // indeterminate state
            if (((*attrMask) & attribute) != 0)
            {
                if (((*attrValue) & attribute) != 0)
                    value = 1; // checked
                else
                    value = 0; // unchecked
            }
            SendMessage(HWindow, BM_SETCHECK, value, 0);
            break;
        }

        case ttDataFromWindow:
        {
            int value = (int)SendMessage(HWindow, BM_GETCHECK, 0, 0);
            if (value == 2)
            {
                *attrMask &= ~attribute;
                *attrValue &= ~attribute; // pouze pro formu
            }
            else
            {
                *attrMask |= attribute;
                if (value == 1)
                    *attrValue |= attribute;
                else
                    *attrValue &= ~attribute;
            }

            break;
        }
        }
    }
    return ret;
}

void CFilterCriteriaDialog::FillUnits(int editID, int comboID, int* units, BOOL appendSizes)
{
    HWND hEdit = GetDlgItem(HWindow, editID);
    HWND hCombo = GetDlgItem(HWindow, comboID);

    // vytahnu z edit line ridici hodnotu
    char buff[100];
    char buff2[100];
    SendMessage(hEdit, WM_GETTEXT, 22, (LPARAM)buff);
    buff[21] = 0;
    CQuadWord editValue;
    editValue.Value = StrToUInt64(buff, (int)strlen(buff));

    BOOL dirty = FALSE; // pokud neuspinime
    SendMessage(hCombo, WM_SETREDRAW, FALSE, 0);
    int curSel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);

    int origCount = (int)SendMessage(hCombo, CB_GETCOUNT, 0, 0);

    int i;
    for (i = 0; units[i] != -1; i++)
    {
        ExpandPluralString(buff, 100, LoadStr(units[i]), 1, &editValue);

        if (curSel >= 0 && !dirty && i == curSel)
        {
            SendMessage(hCombo, CB_GETLBTEXT, i, (LPARAM)buff2);
            if (strcmp(buff, buff2) != 0)
                dirty = TRUE;
        }

        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buff);
    }

    for (i = 0; i < origCount; i++)
        SendMessage(hCombo, CB_DELETESTRING, 0, 0);

    if (appendSizes)
    {
        int sizes[] = {IDS_SIZE_KB, IDS_SIZE_MB, IDS_SIZE_GB, IDS_SIZE_TB, IDS_SIZE_PB, IDS_SIZE_EB, 0};
        for (i = 0; sizes[i] != 0; i++)
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)LoadStr(sizes[i]));
    }

    SendMessage(hCombo, CB_SETCURSEL, curSel, 0);
    SendMessage(hCombo, WM_SETREDRAW, TRUE, 0);
    if (dirty)
        InvalidateRect(hCombo, NULL, FALSE);
}

void CFilterCriteriaDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFilterCriteriaDialog::Transfer()");

    // Attributes
    AttributeCheckBox(&ti, IDC_FFA_ATTRARCHIVE, FILE_ATTRIBUTE_ARCHIVE, &Data->AttributesMask, &Data->AttributesValue);
    AttributeCheckBox(&ti, IDC_FFA_ATTRREADONLY, FILE_ATTRIBUTE_READONLY, &Data->AttributesMask, &Data->AttributesValue);
    AttributeCheckBox(&ti, IDC_FFA_ATTRHIDDEN, FILE_ATTRIBUTE_HIDDEN, &Data->AttributesMask, &Data->AttributesValue);
    AttributeCheckBox(&ti, IDC_FFA_ATTRSYSTEM, FILE_ATTRIBUTE_SYSTEM, &Data->AttributesMask, &Data->AttributesValue);
    AttributeCheckBox(&ti, IDC_FFA_ATTRCOMPRESSED, FILE_ATTRIBUTE_COMPRESSED, &Data->AttributesMask, &Data->AttributesValue);
    AttributeCheckBox(&ti, IDC_FFA_ATTRENCRYPTED, FILE_ATTRIBUTE_ENCRYPTED, &Data->AttributesMask, &Data->AttributesValue);
    AttributeCheckBox(&ti, IDC_FFA_ATTRDIRECTORY, FILE_ATTRIBUTE_DIRECTORY, &Data->AttributesMask, &Data->AttributesValue);

    // Size Min/Max
    ti.CheckBox(IDC_FFA_SIZEMIN, Data->UseMinSize);
    QuadWordEditLineTransfer(&ti, IDC_FFA_SIZEMIN_VALUE, Data->MinSize);
    if (ti.Type == ttDataToWindow)
        SendDlgItemMessage(HWindow, IDC_FFA_SIZEMIN_UNITS, CB_SETCURSEL, (WPARAM)Data->MinSizeUnits, 0);
    else
        Data->MinSizeUnits = (CFilterCriteriaSizeUnitsEnum)SendDlgItemMessage(HWindow, IDC_FFA_SIZEMIN_UNITS, CB_GETCURSEL, 0, 0);
    ti.CheckBox(IDC_FFA_SIZEMAX, Data->UseMaxSize);
    QuadWordEditLineTransfer(&ti, IDC_FFA_SIZEMAX_VALUE, Data->MaxSize);
    if (ti.Type == ttDataToWindow)
        SendDlgItemMessage(HWindow, IDC_FFA_SIZEMAX_UNITS, CB_SETCURSEL, (WPARAM)Data->MaxSizeUnits, 0);
    else
        Data->MaxSizeUnits = (CFilterCriteriaSizeUnitsEnum)SendDlgItemMessage(HWindow, IDC_FFA_SIZEMAX_UNITS, CB_GETCURSEL, 0, 0);

    // Date & Time
    int mode = Data->TimeMode;
    ti.RadioButton(IDC_FFA_TIMEIGNORE, fctmIgnore, mode);
    ti.RadioButton(IDC_FFA_TIMEDURING, fctmDuring, mode);
    ti.RadioButton(IDC_FFA_TIMEFROMTO, fctmFromTo, mode);
    Data->TimeMode = (CFilterCriteriaTimeModeEnum)mode;

    QuadWordEditLineTransfer(&ti, IDC_FFA_TIMEDURING_VALUE, Data->DuringTime);
    if (ti.Type == ttDataToWindow)
        SendDlgItemMessage(HWindow, IDC_FFA_TIMEDURING_UNITS, CB_SETCURSEL, (WPARAM)Data->DuringTimeUnits, 0);
    else
        Data->DuringTimeUnits = (CFilterCriteriaTimeUnitsEnum)SendDlgItemMessage(HWindow, IDC_FFA_TIMEDURING_UNITS, CB_GETCURSEL, 0, 0);

    if (ti.Type == ttDataToWindow)
    {
        SYSTEMTIME st;
        FileTimeToSystemTime((FILETIME*)&Data->From, &st);
        DateTime_SetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_DATE),
                               Data->UseFromDate ? GDT_VALID : GDT_NONE, &st);
        DateTime_SetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_TIME),
                               Data->UseFromTime ? GDT_VALID : GDT_NONE, &st);
        FileTimeToSystemTime((FILETIME*)&Data->To, &st);
        DateTime_SetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_DATE),
                               Data->UseToDate ? GDT_VALID : GDT_NONE, &st);
        DateTime_SetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_TIME),
                               Data->UseToTime ? GDT_VALID : GDT_NONE, &st);
    }
    else
    {
        SYSTEMTIME st;
        SYSTEMTIME st2;

        Data->UseFromDate = DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_DATE), &st) == GDT_VALID;
        Data->UseFromTime = DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_TIME), &st2) == GDT_VALID;
        if (Data->UseFromTime)
        {
            st.wHour = st2.wHour;
            st.wMinute = st2.wMinute;
            st.wSecond = st2.wSecond;
        }
        else
        {
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
        }
        st.wMilliseconds = 0;
        if (!SystemTimeToFileTime(&st, (FILETIME*)&Data->From))
            Data->From = (unsigned __int64)0; // error pro Validate

        Data->UseToDate = DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_DATE), &st) == GDT_VALID;
        Data->UseToTime = DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_TIME), &st2) == GDT_VALID;
        if (Data->UseToTime)
        {
            st.wHour = st2.wHour;
            st.wMinute = st2.wMinute;
            st.wSecond = st2.wSecond;
        }
        else
        {
            st.wHour = 0;
            st.wMinute = 0;
            st.wSecond = 0;
        }
        st.wMilliseconds = 0;
        if (!SystemTimeToFileTime(&st, (FILETIME*)&Data->To))
            Data->To = (unsigned __int64)0; // error pro Validate
    }

    if (ti.Type == ttDataToWindow)
        EnableControls();
    else
        Data->NeedPrepare = TRUE;
}

void CFilterCriteriaDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFilterCriteriaDialog::Validate()");

    // ulozime ukazatel na data
    CFilterCriteria* oldData = Data;
    // a prejdeme na docasnou kopii
    CFilterCriteria data;
    memmove(&data, Data, sizeof(data));
    Data = &data;

    TransferData(ttDataFromWindow);

    // Size Min/Max

    if (ti.IsGood() && Data->UseMinSize)
    {
        // hodnota prevedena na bajty se musi vejit do unsigned __int64
        CQuadWord limit;
        MaxSizeForUnits(&limit, Data->MinSizeUnits);
        if (Data->MinSize > limit)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SIZE_LIMIT_16EB), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_SIZEMIN_VALUE);
        }
    }

    if (ti.IsGood() && Data->UseMaxSize)
    {
        // hodnota prevedena na bajty se musi vejit do unsigned __int64
        CQuadWord limit;
        MaxSizeForUnits(&limit, Data->MaxSizeUnits);
        if (Data->MaxSize > limit)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SIZE_LIMIT_16EB), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_SIZEMAX_VALUE);
        }
    }

    if (ti.IsGood() && Data->UseMinSize && Data->UseMaxSize)
    {
        // pokud uzivatel nastavi obe meze, maximum nesmi byt mensi nez minimum
        CQuadWord minSizeBytes;
        CQuadWord maxSizeBytes;
        SizeToBytes(&minSizeBytes, &Data->MinSize, Data->MinSizeUnits);
        SizeToBytes(&maxSizeBytes, &Data->MaxSize, Data->MaxSizeUnits);
        if (maxSizeBytes < minSizeBytes)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SIZE_MAX_MIN), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_SIZEMAX_VALUE);
        }
    }

    if (Data->TimeMode == fctmDuring)
    {
        // hodnota musi byt nejmene 1
        if (ti.IsGood() && Data->DuringTime.Value < 1)
        {
            SalMessageBox(HWindow, LoadStr(IDS_TIME_MIN), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_TIMEDURING_VALUE);
        }

        if (ti.IsGood())
        {
            // hodnota nesmi presahnout stanoveny limit
            unsigned __int64 limit;
            MaxTimeForUnits(&limit, Data->DuringTimeUnits);
            if (Data->DuringTime.Value > limit)
            {
                SalMessageBox(HWindow, LoadStr(IDS_TIME_MAX), LoadStr(IDS_ERRORTITLE),
                              MB_ICONEXCLAMATION | MB_OK);
                ti.ErrorOn(IDC_FFA_TIMEDURING_VALUE);
            }
        }

        if (ti.IsGood())
        {
            // overime, ze s v PrepareForTest podarilo cas prevest na existujici datum
            Data->PrepareForTest();
            if (!Data->UseMinTime)
            {
                SalMessageBox(HWindow, LoadStr(IDS_INVALIDDATE), LoadStr(IDS_ERRORTITLE),
                              MB_ICONEXCLAMATION | MB_OK);
                ti.ErrorOn(IDC_FFA_TIMEDURING_VALUE);
            }
        }
    }

    if (Data->TimeMode == fctmFromTo)
    {
        // FROM
        if (ti.IsGood() && !Data->UseFromDate && !Data->UseToDate)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SPECIFY_DATE), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_FROM_DATE);
        }

        if (ti.IsGood() && Data->UseFromTime && Data->From == (unsigned __int64)0)
        {
            SalMessageBox(HWindow, LoadStr(IDS_INVALIDDATE), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_FROM_DATE);
        }

        // TO
        if (ti.IsGood() && Data->UseToTime && Data->To == (unsigned __int64)0)
        {
            SalMessageBox(HWindow, LoadStr(IDS_INVALIDDATE), LoadStr(IDS_ERRORTITLE),
                          MB_ICONEXCLAMATION | MB_OK);
            ti.ErrorOn(IDC_FFA_TO_DATE);
        }

        if (ti.IsGood() && Data->UseFromDate && Data->UseToDate)
        {
            Data->PrepareForTest();
            if (Data->MinTime > Data->MaxTime)
            {
                SalMessageBox(HWindow, LoadStr(IDS_TIME_MAX_MIN), LoadStr(IDS_ERRORTITLE),
                              MB_ICONEXCLAMATION | MB_OK);
                ti.ErrorOn(IDC_FFA_TO_DATE);
            }
        }
    }

    // navrat k puvodnim datum
    Data = oldData;
}

void CFilterCriteriaDialog::EnableControls()
{
    CALL_STACK_MESSAGE1("CFilterCriteriaDialog::EnableControls()");
    BOOL checked;
    HWND hFocus = GetFocus();

    BOOL minOrMax = FALSE;

    // Size Min
    checked = IsDlgButtonChecked(HWindow, IDC_FFA_SIZEMIN) == BST_CHECKED;
    minOrMax |= checked;
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_SIZEMIN_VALUE), checked);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_SIZEMIN_UPDOWN), checked);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_SIZEMIN_UNITS), checked);

    // Size Max
    checked = IsDlgButtonChecked(HWindow, IDC_FFA_SIZEMAX) == BST_CHECKED;
    minOrMax |= checked;
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_SIZEMAX_VALUE), checked);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_SIZEMAX_UPDOWN), checked);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_SIZEMAX_UNITS), checked);

    if (minOrMax)
        CheckDlgButton(HWindow, IDC_FFA_ATTRDIRECTORY, BST_INDETERMINATE);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_ATTRDIRECTORY), EnableDirectory && !minOrMax);

    // Time mode (Ignore/During/FromTo)
    CFilterCriteriaTimeModeEnum timeMode = fctmIgnore;
    if (IsDlgButtonChecked(HWindow, IDC_FFA_TIMEDURING) == BST_CHECKED)
        timeMode = fctmDuring;
    if (IsDlgButtonChecked(HWindow, IDC_FFA_TIMEFROMTO) == BST_CHECKED)
        timeMode = fctmFromTo;

    // mode During
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_TIMEDURING_VALUE), timeMode == fctmDuring);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_TIMEDURING_UPDOWN), timeMode == fctmDuring);
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_TIMEDURING_UNITS), timeMode == fctmDuring);

    // mode From To
    SYSTEMTIME st;
    // from
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_FROM_DATE), timeMode == fctmFromTo);
    BOOL enabledFrom = (timeMode == fctmFromTo) &&
                       DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_DATE), &st) == GDT_VALID;
    if (timeMode == fctmFromTo && !enabledFrom)
    {
        DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_TIME), &st);
        DateTime_SetSystemtime(GetDlgItem(HWindow, IDC_FFA_FROM_TIME), GDT_NONE, &st);
    }
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_FROM_TIME), enabledFrom);

    // to
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_TO_DATE), timeMode == fctmFromTo);
    BOOL enabledTo = (timeMode == fctmFromTo) &&
                     DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_DATE), &st) == GDT_VALID;
    if (timeMode == fctmFromTo && !enabledTo)
    {
        DateTime_GetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_TIME), &st);
        DateTime_SetSystemtime(GetDlgItem(HWindow, IDC_FFA_TO_TIME), GDT_NONE, &st);
    }
    EnableWindow(GetDlgItem(HWindow, IDC_FFA_TO_TIME), enabledTo);

    // pokud user stisknul tlacitko Reset pres hot key, mohlo by dojit
    // disablu controlu, kde stoji focus, osetrime to
    if (hFocus != NULL && !IsWindowEnabled(hFocus))
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDC_FFA_SIZEMIN), TRUE);
}

INT_PTR
CFilterCriteriaDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFilterCriteriaDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // pripojime UpDown control do editlajn
        int resID[] = {IDC_FFA_SIZEMIN_VALUE, IDC_FFA_SIZEMAX_VALUE, IDC_FFA_TIMEDURING_VALUE, -1};
        int upDownID[] = {IDC_FFA_SIZEMIN_UPDOWN, IDC_FFA_SIZEMAX_UPDOWN, IDC_FFA_TIMEDURING_UPDOWN};
        int i;
        for (i = 0; resID[i] != -1; i++)
        {
            HWND hEdit = GetDlgItem(HWindow, resID[i]);
            HWND hWnd = CreateUpDownControl(WS_VISIBLE | WS_CHILD | WS_BORDER | UDS_SETBUDDYINT |
                                                UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                                            0, 0, 0, 0, HWindow, upDownID[i], HInstance,
                                            hEdit, 10000, i == 2 ? 1 : 0, 0);
            // posuneme UpDown control v z-orderu hned za editline, jinak
            // zobrazovani dialogu na pomalem stroji vypadalo divne
            // (UpDown se dokreslil az po vsech ostatnich controlech)
            SetWindowPos(hWnd, hEdit, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        if (!EnableDirectory)
            EnableWindow(GetDlgItem(HWindow, IDC_FFA_ATTRDIRECTORY), FALSE);

        break;
    }

    case WM_COMMAND:
    {
        // zmena na nejakem combo boxu
        if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == CBN_SELCHANGE)
            EnableControls();

        // zmena v ridicich edit line
        if (HIWORD(wParam) == EN_UPDATE)
        {
            int sizeUnits[] = {IDS_FFA_BYTES,
                               -1};

            int timeUnits[] = {IDS_FFA_SECONDS,
                               IDS_FFA_MINUTES,
                               IDS_FFA_HOURS,
                               IDS_FFA_DAYS,
                               IDS_FFA_WEEKS,
                               IDS_FFA_MONTHS,
                               IDS_FFA_YEARS,
                               -1};

            if (LOWORD(wParam) == IDC_FFA_SIZEMIN_VALUE)
            {
                // size min
                FillUnits(IDC_FFA_SIZEMIN_VALUE, IDC_FFA_SIZEMIN_UNITS, sizeUnits, TRUE);
            }
            if (LOWORD(wParam) == IDC_FFA_SIZEMAX_VALUE)
            {
                // size max
                FillUnits(IDC_FFA_SIZEMAX_VALUE, IDC_FFA_SIZEMAX_UNITS, sizeUnits, TRUE);
            }
            if (LOWORD(wParam) == IDC_FFA_TIMEDURING_VALUE)
            {
                // date during
                FillUnits(IDC_FFA_TIMEDURING_VALUE, IDC_FFA_TIMEDURING_UNITS, timeUnits, FALSE);
            }
        }

        if (LOWORD(wParam) == IDC_FFA_DEFAULTVALS)
        {
            Data->Reset();
            TransferData(ttDataToWindow);
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR nmh = (LPNMHDR)lParam;
        if (nmh->code == DTN_DATETIMECHANGE)
            EnableControls();
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
