// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

const char* VarOriginalName = "OriginalName";
const char* VarDrive = "Drive";
const char* VarPath = "Path";
const char* VarRelativePath = "RelativePath";
const char* VarName = "Name";
const char* VarNamePart = "NamePart";
const char* VarExtPart = "ExtPart";
const char* VarSize = "Size";
const char* VarTime = "Time";
const char* VarDate = "Date";
const char* VarCounter = "Counter";

class CVariableEx : public CVarString::CVariable
{
public:
    // slouzi pouze k rozseparovani argumentu, vola SerArgument()
    virtual BOOL SetArguments(const char* argStart, const char* argEnd,
                              int& error, const char*& errorPos1, const char*& errorPos2)
    {
        const char* start = argStart;
        const char* end;
        int state = 0;
        while (start < argEnd)
        {
            end = StrQChr(start, argEnd, '\'', ',');
            if (end - start > 0)
            {
                if (!SetArgument(start, end, error, errorPos1, errorPos2, state))
                    return FALSE;
            }
            start = end + 1; // preskocime carku
        }
        return TRUE;
    };

    virtual BOOL SetArgument(const char* argStart, const char* argEnd,
                             int& error, const char*& errorPos1, const char*& errorPos2,
                             int& state) = 0;
};

class CVarAlterableString : public CVariableEx
{
    int CutStart, CountStartFrom, CutEnd, CountEndFrom;
    CChangeCase Case;

public:
    CVarAlterableString()
    {
        CutStart = CutEnd = 0;
        CountStartFrom = 1;
        CountEndFrom = -1;
        Case = ccDontChange;
    }

    virtual BOOL SetArgument(const char* argStart, const char* argEnd,
                             int& error, const char*& errorPos1, const char*& errorPos2,
                             int& state)
    {
        if (state == 0)
        {
            if (SG->StrICmpEx(argStart, (int)(argEnd - argStart), "lower", sizeof("lower") - 1) == 0)
            {
                Case = ccLower;
                return TRUE;
            }
            if (SG->StrICmpEx(argStart, (int)(argEnd - argStart), "upper", sizeof("upper") - 1) == 0)
            {
                Case = ccUpper;
                return TRUE;
            }
            if (SG->StrICmpEx(argStart, (int)(argEnd - argStart), "mixed", sizeof("mixed") - 1) == 0)
            {
                Case = ccMixed;
                return TRUE;
            }
            if (SG->StrICmpEx(argStart, (int)(argEnd - argStart), "stripdia", sizeof("stripdia") - 1) == 0)
            {
                Case = ccStripDia;
                return TRUE;
            }
        }

        int offs, from = 1;
        if (*argStart == '-')
        {
            from = -1;
            argStart++;
        }
        if (IsValidInt(argStart, argEnd, FALSE))
        {
            offs = atoi(argStart);
            if (state == 0)
            {
                CutStart = offs;
                CountStartFrom = from;
                state = 1;
            }
            else
            {
                CutEnd = offs;
                CountEndFrom = from;
                state = 0;
            }
            return TRUE;
        }
        error = state == 0 ? IDS_EXP_EXPECTCUTORCASE : IDS_EXP_EXPECTENDCUT;
        errorPos1 = argStart;
        errorPos2 = argEnd;
        return FALSE;
    }

    virtual int DoExpand(char*& string, char* end,
                         const char* srcStart, const char* srcEnd)
    {
        const char *s, *e;
        s = CountStartFrom > 0 ? srcStart + CutStart : srcEnd - CutStart;
        if (s < srcStart)
            s = srcStart;
        e = CountEndFrom > 0 ? srcStart + CutEnd : srcEnd - CutEnd;
        if (e > srcEnd)
            e = srcEnd;
        int l = (int)(e - s);
        if (l > 0)
        {
            if (end - string < l)
                return -1;
            if (Case == ccDontChange)
                memcpy(string, s, l);
            else
                ChangeCase(Case, string, srcStart, s, e);
            string += l;
            return l;
        }
        return 0;
    }
};

class CVarOriginalName : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarOriginalName(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;
        char *strStart, *strEnd;
        switch (p->Spec)
        {
        case rsFileName:
            strStart = p->File->Name;
            break;
        case rsRelativePath:
            strStart = StripRoot(p->File->FullName, p->RootLen);
            break;
        case rsFullPath:
            strStart = p->File->FullName;
            break;
        }
        strEnd = p->File->FullName + p->File->NameLen;
        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarDrive : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarDrive(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        if (p->Spec == rsFileName || p->Spec == rsRelativePath)
            return 0;

        char *strStart, *strEnd;
        strStart = p->File->FullName;
        if (strStart[0] == '\\' && strStart[1] == '\\') // UNC
        {
            strEnd = strStart + 2;
            while (*strEnd != 0 && *strEnd != '\\')
                strEnd++;
            if (*strEnd != 0)
                strEnd++; // '\\'
            while (*strEnd != 0 && *strEnd != '\\')
                strEnd++;
        }
        else
            strEnd = strStart + 2;

        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarPath : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarPath(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        if (p->Spec == rsFileName || p->Spec == rsRelativePath)
            return 0;

        char *strStart, *strEnd;
        strStart = p->File->FullName;
        if (strStart[0] == '\\' && strStart[1] == '\\') // UNC
        {
            strStart += 2;
            while (*strStart != 0 && *strStart != '\\')
                strStart++;
            if (*strStart != 0)
                strStart++; // '\\'
            while (*strStart != 0 && *strStart != '\\')
                strStart++;
        }
        else
            strStart += 2;

        strEnd = p->File->Name;

        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarRelativePath : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarRelativePath(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        if (p->Spec == rsFileName)
            return 0;

        char *strStart, *strEnd;
        strStart = StripRoot(p->File->FullName, p->RootLen);
        strEnd = p->File->Name;

        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarName : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarName(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        char *strStart, *strEnd;
        strStart = p->File->Name;
        strEnd = p->File->FullName + p->File->NameLen;

        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarNamePart : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarNamePart(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        char *strStart, *strEnd;
        strStart = p->File->Name;
        strEnd = p->File->Ext;
        if (*strEnd != 0)
            strEnd--; //ext ukazuje za tecku

        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarExtPart : public CVarAlterableString
{
public:
    static CVarString::CVariable* Alloc() { return new CVarExtPart(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        char *strStart, *strEnd;
        strStart = p->File->Ext;
        strEnd = p->File->FullName + p->File->NameLen;

        return DoExpand(string, end, strStart, strEnd);
    }
};

class CVarSize : public CVarString::CVariable
{
public:
    static CVarString::CVariable* Alloc() { return new CVarSize(); }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        if (p->File->IsDir)
            return 0;

        int l = SalPrintf(string, (int)(end - string), "%I64u", p->File->Size.Value);
        if (l > 0)
            string += l;
        return l;
    }
};

class CVarTime : public CVariableEx
{
    char* Format;

public:
    CVarTime() { Format = NULL; }
    ~CVarTime()
    {
        if (Format)
            delete[] Format;
    }

    static CVarString::CVariable* Alloc() { return new CVarTime(); }

    virtual BOOL SetArguments(const char* argStart, const char* argEnd,
                              int& error, const char*& errorPos1, const char*& errorPos2)
    {
        if (argStart < argEnd)
            return CVariableEx::SetArguments(
                argStart, argEnd, error, errorPos1, errorPos2);

        error = IDS_EXP_MISTIMEFORMAT;
        errorPos1 = argStart;
        errorPos2 = argEnd;
        return FALSE;
    }

    virtual BOOL SetArgument(const char* argStart, const char* argEnd,
                             int& error, const char*& errorPos1, const char*& errorPos2,
                             int& state)
    {
        if (state == 1)
        {
            error = IDS_EXP_UNEXPECTEDARGUMENT;
            errorPos1 = argStart;
            errorPos2 = argEnd;
            return FALSE;
        }

        state = 1;

        int l = (int)(argEnd - argStart);
        char* fmt = new char[l + 1];
        memcpy(fmt, argStart, l);
        fmt[l] = 0;

        SYSTEMTIME st;
        char buf[100];
        GetSystemTime(&st);
        if (!GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, fmt, buf, 100))
        {
            error = IDS_EXP_INVALIDTIMEFMT;
            errorPos1 = argStart;
            errorPos2 = argEnd;
            delete[] fmt;
            return FALSE;
        }

        Format = fmt;
        return TRUE;
    }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        SYSTEMTIME st;
        FileTimeToSystemTime(&p->File->LastWrite, &st);
        int l = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st,
                              Format, string, (int)(end - string)) -
                1;
        if (l > 0)
            string += l;
        return l;
    }
};

class CVarDate : public CVariableEx
{
    char* Format;

public:
    CVarDate() { Format = NULL; }
    ~CVarDate()
    {
        if (Format)
            delete[] Format;
    }

    static CVarString::CVariable* Alloc() { return new CVarDate(); }

    virtual BOOL SetArguments(const char* argStart, const char* argEnd,
                              int& error, const char*& errorPos1, const char*& errorPos2)
    {
        if (argStart < argEnd)
            return CVariableEx::SetArguments(
                argStart, argEnd, error, errorPos1, errorPos2);

        error = IDS_EXP_MISDATEFORMAT;
        errorPos1 = argStart;
        errorPos2 = argEnd;
        return FALSE;
    }

    virtual BOOL SetArgument(const char* argStart, const char* argEnd,
                             int& error, const char*& errorPos1, const char*& errorPos2,
                             int& state)
    {
        if (state == 1)
        {
            error = IDS_EXP_TOOMUCHARGUMENTS;
            errorPos1 = argStart;
            errorPos2 = argEnd;
            return FALSE;
        }

        state = 1;

        int l = (int)(argEnd - argStart);
        char* fmt = new char[l + 1];
        memcpy(fmt, argStart, l);
        fmt[l] = 0;

        SYSTEMTIME st;
        char buf[100];
        GetSystemTime(&st);
        if (!GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, fmt, buf, 100))
        {
            error = IDS_EXP_INVALIDDATEFMT;
            errorPos1 = argStart;
            errorPos2 = argEnd;
            delete[] fmt;
            return FALSE;
        }

        Format = fmt;
        return TRUE;
    }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        SYSTEMTIME st;
        FileTimeToSystemTime(&p->File->LastWrite, &st);
        int l = GetDateFormat(LOCALE_USER_DEFAULT, 0, &st,
                              Format, string, (int)(end - string)) -
                1;
        if (l > 0)
            string += l;
        return l;
    }
};

class CVarCounter : public CVariableEx
{
    int Start;
    double Step;
    char Format[3];
    BOOL LeftAlign;
    int Width;
    char Fill;

public:
    CVarCounter()
    {
        Start = 0;
        Step = 1;
        strcpy(Format, "%d");
        LeftAlign = FALSE;
        Width = 0;
        Fill = ' ';
    }

    static CVarString::CVariable* Alloc() { return new CVarCounter(); }

    virtual BOOL SetArgument(const char* argStart, const char* argEnd,
                             int& error, const char*& errorPos1, const char*& errorPos2,
                             int& state)
    {
        int oldState = state;
        switch (state)
        {
        // expecting 'start'
        case 0:
        {
            if (!IsValidInt(argStart, argEnd, TRUE))
            {
                error = IDS_EXP_EXPECTSTART;
                errorPos1 = argStart;
                errorPos2 = argEnd;
                return FALSE;
            }

            Start = atoi(argStart);
            break;
        }

        // expecting 'step'
        case 1:
        {
            if (!IsValidFloat(argStart, argEnd))
            {
                error = IDS_EXP_EXPECTSTEP;
                errorPos1 = argStart;
                errorPos2 = argEnd;
                return FALSE;
            }

            Step = atof(argStart);
            break;
        }

        // expecting 'base', 'left-align' or 'width'
        case 2:
        {
            if (argEnd - argStart == 1 && strchr("dxX", *argStart))
            {
                Format[1] = *argStart;
                break;
            }
            state++;
        }

        // expecting 'left-align' or 'width'
        case 3:
        {
            if (argEnd - argStart == 1 && *argStart == 'l')
            {
                LeftAlign = TRUE;
                break;
            }
            state++;
        }

        // expecting 'width'
        case 4:
        {
            if (!IsValidInt(argStart, argEnd, FALSE))
            {
                switch (oldState)
                {
                case 2:
                    error = IDS_EXP_EXPECTBASEALIGNWIDTH;
                case 3:
                    error = IDS_EXP_EXPECTALIGNWIDTH;
                case 4:
                    error = IDS_EXP_EXPECTWIDTH;
                }
                errorPos1 = argStart;
                errorPos2 = argEnd;
                return FALSE;
            }

            Width = atoi(argStart);
            break;
        }

        // expecting 'fill'
        case 5:
        {
            if (argEnd - argStart != 1)
            {
                error = IDS_EXP_EXPECTFILL;
                errorPos1 = argStart;
                errorPos2 = argEnd;
                return FALSE;
            }

            Fill = *argStart;
            break;
        }

        default:
        {
            error = IDS_EXP_UNEXPECTEDARGUMENT;
            errorPos1 = argStart;
            errorPos2 = argEnd;
            return FALSE;
        }
        }

        state++;
        return TRUE;
    }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        CExecuteNewNameParam* p = (CExecuteNewNameParam*)param;

        int l, fill;
        if (Width <= 1 || LeftAlign)
        {
            l = SalPrintf(string, (int)(end - string), Format, (int)(Start + Step * p->Counter));
            if (l < 0)
                return -1;
            fill = Width - l;
            if (fill > 0)
            {
                if (Width > end - string)
                    return -1;
                memset(string + l, Fill, fill);
                l = Width;
            }
        }
        else
        {
            char buffer[50];
            l = SalPrintf(buffer, 50, Format, (int)(Start + Step * p->Counter));
            fill = Width - l;
            if (fill > 0)
            {
                if (Width > end - string)
                    return -1;
                memset(string, Fill, fill);
                memcpy(string + fill, buffer, l);
                l = Width;
            }
            else
            {
                if (l > end - string)
                    return -1;
                memcpy(string, buffer, l);
            }
        }
        string += l;
        return l;
    }
};

CVarString::CVariableEntry NewNameVariables[] =
    {
        VarOriginalName, CVarOriginalName::Alloc,
        VarDrive, CVarDrive::Alloc,
        VarPath, CVarPath::Alloc,
        VarRelativePath, CVarRelativePath::Alloc,
        VarName, CVarName::Alloc,
        VarNamePart, CVarNamePart::Alloc,
        VarExtPart, CVarExtPart::Alloc,
        VarSize, CVarSize::Alloc,
        VarTime, CVarTime::Alloc,
        VarDate, CVarDate::Alloc,
        VarCounter, CVarCounter::Alloc,
        NULL, NULL};
