// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

// ****************************************************************************

BOOL ConvertStringRegToTxt(char* buf, int bufSize, const char* regStr)
{
    if (bufSize == 0)
        return FALSE;
    const char* s = regStr;
    char* d = buf;
    char* end = buf + bufSize - 2;
    if (bufSize > 2)
    {
        while (*s != 0 && d < end)
        {
            if (*s == '\\') // escape sequence
            {
                s++;
                if (*s != 0)
                    *d++ = *s;
            }
            else
            {
                if (*s == '|') // CRLF
                {
                    *d++ = '\r';
                    *d++ = '\n';
                }
                else
                {
                    if (*s == '!')
                        *d++ = '\n'; // LF
                    else
                    {
                        if (*s == '$')
                            *d++ = '\r'; // CR
                        else
                            *d++ = *s; // normal character
                    }
                }
            }
            s++;
        }
    }
    *d = 0;
    return *s == 0;
}

// ****************************************************************************

BOOL ConvertStringTxtToReg(char* buf, int bufSize, const char* txtStr)
{
    if (bufSize == 0)
        return FALSE;
    const char* s = txtStr;
    char* d = buf;
    char* end = buf + bufSize - 2;
    if (bufSize > 2)
    {
        while (*s != 0 && d < end)
        {
            if (*s == '\r')
            {
                if (*(s + 1) == '\n')
                {
                    s++;
                    *d++ = '|'; // CRLF
                }
                else
                    *d++ = '$'; // CR
            }
            else
            {
                if (*s == '\n')
                    *d++ = '!'; // LF
                else
                {
                    if (*s == '|' || *s == '!' || *s == '$' || *s == '\\')
                        *d++ = '\\'; // escape sequence
                    *d++ = *s;
                }
            }
            s++;
        }
    }
    *d = 0;
    return *s == 0;
}

//
// ****************************************************************************
// CSrvTypeColumn
//

CSrvTypeColumn::CSrvTypeColumn(CSrvTypeColWidths* colWidths)
{
    Visible = FALSE;
    ID = NULL;
    NameID = -1;
    NameStr = NULL;
    DescrID = -1;
    DescrStr = NULL;
    Type = stctNone;
    EmptyValue = NULL;
    LeftAlignment = TRUE;
    if (colWidths != NULL)
        ColWidths = colWidths->AddRef();
    else
        ColWidths = new CSrvTypeColWidths;
}

CSrvTypeColumn::~CSrvTypeColumn()
{
    if (ID != NULL)
        SalamanderGeneral->Free(ID);
    if (NameStr != NULL)
        SalamanderGeneral->Free(NameStr);
    if (DescrStr != NULL)
        SalamanderGeneral->Free(DescrStr);
    if (EmptyValue != NULL)
        SalamanderGeneral->Free(EmptyValue);
    if (ColWidths != NULL && ColWidths->Release())
        delete ColWidths;
}

void CSrvTypeColumn::LoadFromObj(CSrvTypeColumn* copyFrom)
{
    Visible = copyFrom->Visible;
    ID = SalamanderGeneral->DupStr(copyFrom->ID);
    NameID = copyFrom->NameID;
    NameStr = SalamanderGeneral->DupStr(copyFrom->NameStr);
    DescrID = copyFrom->DescrID;
    DescrStr = SalamanderGeneral->DupStr(copyFrom->DescrStr);
    Type = copyFrom->Type;
    EmptyValue = SalamanderGeneral->DupStr(copyFrom->EmptyValue);
    LeftAlignment = copyFrom->LeftAlignment;
    ColWidths->FixedWidth = copyFrom->ColWidths->FixedWidth;
    ColWidths->Width = copyFrom->ColWidths->Width;
}

BOOL CSrvTypeColumn::LoadStr(const char** str, char** result, int limit)
{
    const char* s = *str;
    int esc = 0;
    while (*s != 0 && *s != ',')
    {
        if (*s == '\\')
        {
            esc++;
            s++;
        }
        s++;
    }
    BOOL ret = TRUE;
    if (esc == 1 && s - *str == 2 && (*str)[1] == '0')
        *result = NULL; // "\\0" is the escape sequence for NULL
    else
    {
        *result = (char*)SalamanderGeneral->Alloc((int)(s - *str) - esc + 1);
        if (*result != NULL)
        {
            s = *str;
            char* d = *result;
            while (*s != 0 && *s != ',')
            {
                if (*s == '\\')
                    s++;
                *d++ = *s++;
            }
            *d = 0;
            if (limit != -1 && d - *result > limit)
                (*result)[limit] = 0; // truncate the string to the limit
        }
        else
        {
            TRACE_E(LOW_MEMORY);
            ret = FALSE;
        }
    }
    if (*s == ',')
        s++;
    *str = s;
    return ret;
}

BOOL CSrvTypeColumn::LoadFromStr(const char* str)
{
    Visible = *str++ == '1';
    if (*str++ != ',')
        return FALSE;
    BOOL err = FALSE;
    if (!LoadStr(&str, &ID, STC_ID_MAX_SIZE - 1))
        return FALSE;
    char* strNameID;
    if (!LoadStr(&str, &strNameID, -1) || strNameID == NULL)
        return FALSE;
    NameID = atoi(strNameID);
    SalamanderGeneral->Free(strNameID);
    if (!LoadStr(&str, &NameStr, STC_NAME_MAX_SIZE - 1))
        return FALSE;
    char* strDescrID;
    if (!LoadStr(&str, &strDescrID, -1) || strDescrID == NULL)
        return FALSE;
    DescrID = atoi(strDescrID);
    SalamanderGeneral->Free(strDescrID);
    if (!LoadStr(&str, &DescrStr, STC_DESCR_MAX_SIZE - 1))
        return FALSE;
    char* strType;
    if (!LoadStr(&str, &strType, -1) || strType == NULL)
        return FALSE;
    Type = (CSrvTypeColumnTypes)atoi(strType);
    SalamanderGeneral->Free(strType);
    if (Type <= stctNone || Type >= stctLastItem)
        return FALSE;
    if (!LoadStr(&str, &EmptyValue, STC_EMPTYVAL_MAX_SIZE - 1))
        return FALSE;
    if (*str == 0)
        LeftAlignment = TRUE; // older versions did not have alignment yet = align to the left
    else
    {
        LeftAlignment = *str++ == '1';
        if (*str == ',')
            str++;
    }
    if (*str == 0)
        ColWidths->FixedWidth = 0; // older versions did not have FixedWidth yet = flexible column
    else
    {
        switch (*str++)
        {
        case '1':
            ColWidths->FixedWidth = MAKELONG(1, 0);
            break;
        case '2':
            ColWidths->FixedWidth = MAKELONG(0, 1);
            break;
        case '3':
            ColWidths->FixedWidth = MAKELONG(1, 1);
            break;
        default:
            ColWidths->FixedWidth = 0;
            break;
        }
        if (*str == ',')
            str++;
    }
    if (*str == 0)
        ColWidths->Width = 0; // older versions did not have Width yet = minimal width
    else
    {
        char* strWidth;
        if (!LoadStr(&str, &strWidth, -1) || strWidth == NULL)
            return FALSE;
        ColWidths->Width = atoi(strWidth);
        SalamanderGeneral->Free(strWidth);
    }
    return TRUE;
}

int CSrvTypeColumn::SaveStrLen(const char* str)
{
    if (str == NULL)
        return 2; // for NULL we have the escape sequence "\\0"
    const char* s = str;
    int size = 0;
    while (*s != 0)
    {
        if (*s == ',')
            size++; // for ',' we have the escape sequence "\\,"
        else if (*s == '\\')
            size++; // for '\\' we have the escape sequence "\\\\"
        s++;
    }
    return size + (int)(s - str);
}

void CSrvTypeColumn::SaveStr(char** s, const char* str, BOOL addComma)
{
    char* dest = *s;
    if (str == NULL)
    {
        *dest++ = '\\';
        *dest++ = '0';
    }
    else
    {
        while (*str != 0)
        {
            if (*str == ',' || *str == '\\')
                *dest++ = '\\'; // for ',' and '\\' we have the escape sequence
            *dest++ = *str++;
        }
    }
    if (addComma)
        *dest++ = ',';
    *s = dest;
}

BOOL CSrvTypeColumn::SaveToStr(char* buf, int bufSize, BOOL ignoreColWidths)
{
    char strNameID[20];
    sprintf(strNameID, "%d", NameID);
    char strDescrID[20];
    sprintf(strDescrID, "%d", DescrID);
    char strWidth[20];
    sprintf(strWidth, "%d", ColWidths->Width);
    int size = 2 +                           // Visible
               SaveStrLen(ID) + 1 +          // ID
               (int)strlen(strNameID) + 1 +  // NameID
               SaveStrLen(NameStr) + 1 +     // NameStr
               (int)strlen(strDescrID) + 1 + // DescrID
               SaveStrLen(DescrStr) + 1 +    // DescrStr
               3 +                           // Type
               SaveStrLen(EmptyValue) + 1 +  // EmptyValue
               2 +                           // LeftAlignment
               2 +                           // FixedWidth
               (int)strlen(strWidth);        // Width
    if (bufSize >= size + 1)
    {
        char* s = buf;
        *s++ = Visible ? '1' : '0';
        *s++ = ',';
        SaveStr(&s, ID, TRUE);
        SaveStr(&s, strNameID, TRUE);
        SaveStr(&s, NameStr, TRUE);
        SaveStr(&s, strDescrID, TRUE);
        SaveStr(&s, DescrStr, TRUE);
        char num[20];
        sprintf(num, "%d", (int)Type);
        *s++ = num[0];
        if (num[1] != 0)
            *s++ = num[1]; // Type has at most two digits
        *s++ = ',';
        SaveStr(&s, EmptyValue, FALSE);
        *s++ = ',';
        *s++ = LeftAlignment ? '1' : '0';
        if (!ignoreColWidths)
        {
            *s++ = ',';
            *s++ = '0' + (LOWORD(ColWidths->FixedWidth) != 0 ? 1 : 0) + (HIWORD(ColWidths->FixedWidth) != 0 ? 2 : 0);
            *s++ = ',';
            SaveStr(&s, strWidth, FALSE);
        }
        *s = 0;
        return TRUE;
    }
    else
    {
        TRACE_E("CSrvTypeColumn::SaveToStr(): Small buffer for saving column!");
        return FALSE;
    }
}

CSrvTypeColumn*
CSrvTypeColumn::MakeCopy()
{
    CSrvTypeColumn* n = new CSrvTypeColumn(ColWidths);
    if (n != NULL && n->IsGood())
    {
        n->Visible = Visible;
        n->ID = SalamanderGeneral->DupStr(ID);
        n->NameID = NameID;
        n->NameStr = SalamanderGeneral->DupStr(NameStr);
        n->DescrID = DescrID;
        n->DescrStr = SalamanderGeneral->DupStr(DescrStr);
        n->Type = Type;
        n->EmptyValue = SalamanderGeneral->DupStr(EmptyValue);
        n->LeftAlignment = LeftAlignment;
    }
    else
    {
        TRACE_E(LOW_MEMORY);
        if (n != NULL)
            delete n;
    }
    return n;
}

void CSrvTypeColumn::Set(BOOL visible, char* id, int nameID, char* nameStr, int descrID,
                         char* descrStr, CSrvTypeColumnTypes type, char* emptyValue,
                         BOOL leftAlignment, int fixedWidth, int width)
{
    Visible = visible;
    ID = SalamanderGeneral->DupStr(id);
    NameID = nameID;
    NameStr = SalamanderGeneral->DupStr(nameStr);
    DescrID = descrID;
    DescrStr = SalamanderGeneral->DupStr(descrStr);
    Type = type;
    EmptyValue = SalamanderGeneral->DupStr(emptyValue);
    LeftAlignment = leftAlignment;
    ColWidths->FixedWidth = fixedWidth;
    ColWidths->Width = width;
}

//
// ****************************************************************************

BOOL ValidateSrvTypeColumns(TIndirectArray<CSrvTypeColumn>* columns, int* errResID)
{
    // check whether the first column is Name and visible
    if (columns->Count > 0 && columns->At(0)->Type == stctName && columns->At(0)->Visible)
    {
        // check ID non-emptiness + type counts + whether Ext is second and visible + check of "empty value"
        // + check non-emptiness of Name and Description
        int i, counts[stctLastItem - 1];
        memset(counts, 0, sizeof(counts));
        for (i = 0; i < columns->Count; i++)
        {
            CSrvTypeColumn* col = columns->At(i);

            if (!IsValidIdentifier(col->ID, errResID))
                return FALSE;
            CSrvTypeColumnTypes type = col->Type;
            if (type > stctNone && type < stctLastItem)
                counts[(int)type - 1]++;
            else
            {
                TRACE_E("Unexpected situation in ValidateSrvTypeColumns(): unknown column type!");
                if (errResID != NULL)
                    *errResID = IDS_STC_ERR_TYPEERR;
                return FALSE;
            }
            if (type == stctExt && (i != 1 || !col->Visible))
            {
                if (errResID != NULL)
                    *errResID = IDS_STC_ERR_EXTERR;
                return FALSE;
            }
            // empty values make no sense for the Name, Ext, and Type types, clear them
            if ((type == stctName || type == stctExt || type == stctType) && col->EmptyValue != NULL)
            {
                free(col->EmptyValue);
                col->EmptyValue = NULL;
            }
            if (!GetColumnEmptyValue(col->EmptyValue, type, NULL, NULL, NULL, FALSE))
            {
                if (errResID != NULL)
                    *errResID = IDS_STC_ERR_INVALEMPTY;
                return FALSE;
            }

            if (col->NameID == -1 && (col->NameStr == NULL || *(col->NameStr) == 0))
            {
                if (errResID != NULL)
                    *errResID = IDS_STC_ERR_EMPTYNAME;
                return FALSE;
            }

            if (col->DescrID == -1 && (col->DescrStr == NULL || *(col->DescrStr) == 0))
            {
                if (errResID != NULL)
                    *errResID = IDS_STC_ERR_EMPTYDESCR;
                return FALSE;
            }
        }
        for (i = 1; i < stctLastItem; i++)
        {
            if (counts[i - 1] > 1 && i < stctFirstGeneral)
            {
                if (errResID != NULL)
                    *errResID = IDS_STC_ERR_TYPEERR;
                return FALSE;
            }
        }
        // check uniqueness of IDs
        for (i = 0; i < columns->Count - 1; i++)
        {
            char* id = columns->At(i)->ID;
            int j;
            for (j = i + 1; j < columns->Count; j++)
            {
                if (SalamanderGeneral->StrICmp(id, columns->At(j)->ID) == 0)
                {
                    if (errResID != NULL)
                        *errResID = IDS_STC_ERR_IDNOTUNIQUE;
                    return FALSE;
                }
            }
        }
    }
    else
    {
        if (errResID != NULL)
            *errResID = IDS_STC_ERR_NAMEERR;
        return FALSE;
    }
    if (errResID != NULL)
        *errResID = 0;
    return TRUE;
}

//
// ****************************************************************************
// CServerType
//

void CServerType::Init()
{
    TypeName = NULL;
    AutodetectCond = NULL;
    RulesForParsing = NULL;
    CompiledAutodetCond = NULL;
    CompiledParser = NULL;
}

void CServerType::Release()
{
    if (TypeName != NULL)
        SalamanderGeneral->Free(TypeName);
    if (AutodetectCond != NULL)
        SalamanderGeneral->Free(AutodetectCond);
    Columns.DestroyMembers();
    if (RulesForParsing != NULL)
        SalamanderGeneral->Free(RulesForParsing);
    if (CompiledAutodetCond != NULL)
        delete CompiledAutodetCond;
    if (CompiledParser != NULL)
        delete CompiledParser;
    Init();
}

BOOL CServerType::Set(const char* typeName, const char* autodetectCond, int columnsCount,
                      const char* columnsStr[], const char* rulesForParsing)
{
    BOOL err = FALSE;
    if (typeName == NULL)
    {
        TRACE_E("Server Type Name can't be NULL!");
        err = TRUE;
    }
    else
    {
        UpdateStr(TypeName, typeName, &err);
        UpdateStr(AutodetectCond, autodetectCond, &err);
        UpdateStr(RulesForParsing, rulesForParsing, &err);
        Columns.DestroyMembers();
        int i;
        for (i = 0; i < columnsCount; i++)
        {
            CSrvTypeColumn* c = new CSrvTypeColumn;
            if (c != NULL && c->IsGood() && c->LoadFromStr(columnsStr[i]))
            {
                Columns.Add(c);
                if (!Columns.IsGood())
                {
                    delete c;
                    Columns.ResetState();
                    err = TRUE;
                    break;
                }
            }
            else
            {
                if (c != NULL)
                {
                    if (c->IsGood())
                        TRACE_E("Unable to load CSrvTypeColumn from string: " << columnsStr[i]);
                    else
                        TRACE_E(LOW_MEMORY);
                    delete c;
                }
                else
                    TRACE_E(LOW_MEMORY);
                err = TRUE;
                break;
            }
        }
        if (CompiledAutodetCond != NULL)
        {
            delete CompiledAutodetCond;
            CompiledAutodetCond = NULL;
        }
        if (CompiledParser != NULL)
        {
            delete CompiledParser;
            CompiledParser = NULL;
        }
    }
    return !err;
}

BOOL CServerType::Set(const char* typeName, CServerType* copyFrom)
{
    BOOL err = FALSE;
    if (typeName == NULL)
    {
        TRACE_E("Server Type Name can't be NULL!");
        err = TRUE;
    }
    else
    {
        UpdateStr(TypeName, typeName, &err);
        UpdateStr(AutodetectCond, copyFrom->AutodetectCond, &err);
        UpdateStr(RulesForParsing, copyFrom->RulesForParsing, &err);
        Columns.DestroyMembers();
        int i;
        for (i = 0; i < copyFrom->Columns.Count; i++)
        {
            CSrvTypeColumn* c = new CSrvTypeColumn;
            if (c != NULL && c->IsGood())
            {
                c->LoadFromObj(copyFrom->Columns[i]);
                Columns.Add(c);
                if (!Columns.IsGood())
                {
                    delete c;
                    Columns.ResetState();
                    err = TRUE;
                    break;
                }
            }
            else
            {
                if (c != NULL)
                {
                    if (!c->IsGood())
                        TRACE_E(LOW_MEMORY);
                    delete c;
                }
                else
                    TRACE_E(LOW_MEMORY);
                err = TRUE;
                break;
            }
        }
        if (CompiledAutodetCond != NULL)
        {
            delete CompiledAutodetCond;
            CompiledAutodetCond = NULL;
        }
        if (CompiledParser != NULL)
        {
            delete CompiledParser;
            CompiledParser = NULL;
        }
    }
    return !err;
}

BOOL CServerType::Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    char name[SERVERTYPE_MAX_SIZE];
    if (!registry->GetValue(regKey, CONFIG_STNAME, REG_SZ, name, SERVERTYPE_MAX_SIZE))
        return FALSE; // name is mandatory
    char autodetectCond[AUTODETCOND_MAX_SIZE];
    autodetectCond[0] = 0;
    registry->GetValue(regKey, CONFIG_STADCOND, REG_SZ, autodetectCond, AUTODETCOND_MAX_SIZE); // optional

    TIndirectArray<char> columnStrings(5, 5);
    if (!columnStrings.IsGood())
        return FALSE; // low memory, aborting
    HKEY columnsKey;
    if (registry->OpenKey(regKey, CONFIG_STCOLUMNS, columnsKey))
    {
        char colStr[STC_MAXCOLUMNSTR];
        char num[20] = "1";
        int i = 1;
        while (registry->GetValue(columnsKey, num, REG_SZ, colStr, STC_MAXCOLUMNSTR))
        {
            char* s = _strdup(colStr);
            if (s != NULL)
            {
                columnStrings.Add(s);
                if (!columnStrings.IsGood())
                {
                    columnStrings.ResetState();
                    return FALSE; // probably low memory, aborting
                }
            }
            else
                return FALSE; // all columns must be loaded, terminating
            sprintf(num, "%d", ++i);
        }
        registry->CloseKey(columnsKey);
    }
    else
        return FALSE; // columns are mandatory

    char rulesForParsingReg[PARSER_MAX_SIZE + 1000];
    rulesForParsingReg[0] = 0;
    char rulesForParsing[PARSER_MAX_SIZE];
    rulesForParsing[0] = 0;
    if (!registry->GetValue(regKey, CONFIG_STRULESFORPARS, REG_SZ, rulesForParsingReg, PARSER_MAX_SIZE + 1000))
        return FALSE; // parsing rules are mandatory as well
    if (!ConvertStringRegToTxt(rulesForParsing, PARSER_MAX_SIZE, rulesForParsingReg))
        TRACE_E("Unexpected error in CServerType::Load(): small buffer for rules for parsing!");
    BOOL ret = Set(name, autodetectCond[0] != 0 ? autodetectCond : NULL,
                   columnStrings.Count, (const char**)columnStrings.GetData(),
                   rulesForParsing[0] != 0 ? rulesForParsing : NULL);

    // verify that the list of columns is valid
    if (ret)
        ret = ValidateSrvTypeColumns(&Columns, NULL);

    // verify that the parsing rules are valid
    if (ret)
    {
        CFTPParser* parser = CompileParsingRules(HandleNULLStr(RulesForParsing), &Columns, NULL, NULL, NULL);
        if (parser != NULL)
            delete parser; // parser is OK, discard it again
        else
            ret = FALSE; // error in the rules
    }

    // verify that the autodetect condition is valid
    if (ret)
    {
        CFTPAutodetCondNode* node = CompileAutodetectCond(HandleNULLStr(AutodetectCond), NULL, NULL, NULL, NULL, 0);
        if (node != NULL)
            delete node; // condition is OK, discard it again
        else
            ret = FALSE; // error in the condition
    }

    return ret;
}

void CServerType::Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    registry->SetValue(regKey, CONFIG_STNAME, REG_SZ, TypeName, -1);
    registry->SetValue(regKey, CONFIG_STADCOND, REG_SZ, HandleNULLStr(AutodetectCond), -1);
    HKEY columnsKey;
    if (registry->CreateKey(regKey, CONFIG_STCOLUMNS, columnsKey))
    {
        registry->ClearKey(columnsKey);
        char num[20];
        char colStr[STC_MAXCOLUMNSTR];
        int i;
        for (i = 0; i < Columns.Count; i++)
        {
            sprintf(num, "%d", i + 1);
            Columns[i]->SaveToStr(colStr, STC_MAXCOLUMNSTR);
            registry->SetValue(columnsKey, num, REG_SZ, colStr, -1);
        }
        registry->CloseKey(columnsKey);
    }
    char rulesForParsingReg[PARSER_MAX_SIZE + 1000];
    rulesForParsingReg[0] = 0;
    if (!ConvertStringTxtToReg(rulesForParsingReg, PARSER_MAX_SIZE + 1000, HandleNULLStr(RulesForParsing)))
        TRACE_E("Unexpected error in CServerType::Save(): small buffer for rules for parsing!");
    registry->SetValue(regKey, CONFIG_STRULESFORPARS, REG_SZ, rulesForParsingReg, -1);
}

CServerType*
CServerType::MakeCopy()
{
    CServerType* n = new CServerType;
    if (n != NULL)
    {
        BOOL err = FALSE;
        n->TypeName = SalamanderGeneral->DupStr(TypeName);
        n->AutodetectCond = SalamanderGeneral->DupStr(AutodetectCond);
        n->RulesForParsing = SalamanderGeneral->DupStr(RulesForParsing);
        int i;
        for (i = 0; i < Columns.Count; i++)
        {
            CSrvTypeColumn* c = Columns[i]->MakeCopy();
            if (c != NULL)
            {
                n->Columns.Add(c);
                if (!n->Columns.IsGood())
                {
                    delete c;
                    n->Columns.ResetState();
                    err = TRUE;
                    break;
                }
            }
            else
            {
                err = TRUE;
                break;
            }
        }
        // CompiledAutodetCond = NULL;  // unnecessary, done in n->Init() in the constructor
        // CompiledParser = NULL;

        if (err) // not everything was duplicated
        {
            delete n;
            n = NULL;
        }
    }
    else
        TRACE_E(LOW_MEMORY);
    return n;
}

char* GetTypeNameForUser(char* typeName, char* buf, int bufSize)
{
    char* txt = typeName;
    if (*txt == '*' && bufSize > 0) // a name starting with '*' means "user defined server type" - strip the character and append the suffix
    {
        _snprintf_s(buf, bufSize, _TRUNCATE, "%s %s", txt + 1, UserDefinedSuffix);
        txt = buf;
    }
    return txt;
}

#define WSTF_MAX_LINE_LEN 80 // used when writing and reading a string to a file

// writes string 'str' to file 'file', returns any error in 'err' (must not be NULL);
// format (apart from "// " at the beginning of the line exactly as it will be in the file - '"', '\', etc. have no C++ meaning):
// "first line starts with double quotes
// long lines has backslash after each part of line:
// line-part1\
// line-part2\
// line-part3
// next-line with escape sequence \" for double quotes and \\ for backslash
// NULL string is written only as \0 escape sequence
// lastline ends with double quotes"
void WriteStrToFile(HANDLE file, const char* str, DWORD* err)
{
    char line[WSTF_MAX_LINE_LEN + 3]; // 3 characters are overhead for easy writing
    ULONG written;
    BOOL success;
    if (str == NULL)
    {
        strcpy(line, "\"\\0\"\r\n"); // string for a NULL value
        int len = (int)strlen(line);
        if ((success = WriteFile(file, line, len, &written, NULL)) == 0 ||
            written != (DWORD)len)
        {
            if (!success)
                *err = GetLastError();
            else
                *err = ERROR_DISK_FULL;
        }
    }
    else
    {
        const char* s = str;
        char* end = line + WSTF_MAX_LINE_LEN + 3;
        BOOL firstLine = TRUE;
        while (1)
        {
            char* d = line;
            if (firstLine)
            {
                *d++ = '"';
                firstLine = FALSE;
            }
            while (*s != 0 && *s != '\r' && *s != '\n')
            {
                if (d > end - 5) // for simplicity we check that we can fit: '\\'+'X'+'"'+'\r'+'\n'
                {
                    *d++ = '\\';
                    break; // finish this part of the line, continue on the next line in the file
                }
                if (*s == '"' || *s == '\\')
                    *d++ = '\\';
                *d++ = *s++;
            }
            if (*s == 0)
                *d++ = '"';
            *d++ = '\r';
            *d++ = '\n';

            // write to the file
            if ((success = WriteFile(file, line, (DWORD)(d - line), &written, NULL)) == 0 ||
                written != (DWORD)(d - line))
            {
                if (!success)
                    *err = GetLastError();
                else
                    *err = ERROR_DISK_FULL;
                break; // write error, abort with error
            }

            if (*s == 0)
                break; // finished writing the string

            if (*s == '\r')
                s++;
            if (*s == '\n')
                s++;
        }
    }
}

// texts in the file with an exported "server type" (extension .STR)
const char* STR_FILE_HEADER = "Open Salamander - FTP Client - Exported Server Type";
const char* STR_FILE_TYPENAME = "Type Name:";
const char* STR_FILE_ADCOND = "Autodetect Condition:";
const char* STR_FILE_COLUMNS = "Columns:";
const char* STR_FILE_RULES = "Rules for Parsing:";

DWORD
CServerType::ExportToFile(HANDLE file)
{
    DWORD err = NO_ERROR;
    char line[500];
    int i = 0;
    while (1)
    {
        switch (i)
        {
        case 0:
            _snprintf_s(line, _TRUNCATE, "%s\r\n\r\n", STR_FILE_HEADER);
            break;
        case 1:
            _snprintf_s(line, _TRUNCATE, "%s %s\r\n", STR_FILE_TYPENAME, TypeName);
            break;
        case 2:
            _snprintf_s(line, _TRUNCATE, "%s ", STR_FILE_ADCOND);
            break; // header for AutodetectCond

        case 3: // write the AutodetectCond string
        {
            WriteStrToFile(file, AutodetectCond, &err);
            line[0] = 0; // we will not write it this way
            break;
        }

        case 4:
            _snprintf_s(line, _TRUNCATE, "\r\n%s\r\n", STR_FILE_COLUMNS);
            break; // header for columns

        case 5: // write all columns
        {
            char colStr[STC_MAXCOLUMNSTR];
            int j;
            for (j = 0; j < Columns.Count; j++)
            {
                Columns[j]->SaveToStr(colStr, STC_MAXCOLUMNSTR, TRUE);
                WriteStrToFile(file, colStr, &err);
                if (err != NO_ERROR)
                    break; // file error, aborting
            }
            line[0] = 0; // we will not write it this way
            break;
        }

        case 6:
            _snprintf_s(line, _TRUNCATE, "\r\n%s\r\n", STR_FILE_RULES);
            break; // header for RulesForParsing

        case 7: // write the RulesForParsing string
        {
            WriteStrToFile(file, RulesForParsing, &err);
            line[0] = 0; // we will not write it this way
            break;
        }

        default:
            i = -1;
            break; // end of file
        }
        if (err != NO_ERROR)
            break; // file error, aborting
        if (i == -1)
            break; // end of file, finishing successfully
        else
            i++; // move to the next line

        // write the data line to the file
        ULONG written;
        BOOL success;
        int len = (int)strlen(line);
        if (len > 0 && // there is something to write
            ((success = WriteFile(file, line, len, &written, NULL)) == 0 ||
             written != (DWORD)len))
        {
            if (!success)
                err = GetLastError();
            else
                err = ERROR_DISK_FULL;
            break; // aborting with an error
        }
    }
    return err;
}

// on the line from 'beg' to 'end' it searches for the string 'str' - if it is there (comparison is case sensitive),
// returns TRUE and stores in 'afterStr' the continuation of the line after the found string,
// if it is not there, returns FALSE and puts 'beg' into 'afterStr'
BOOL MatchString(char* beg, char* end, const char* str, char** afterStr)
{
    int l = (int)strlen(str);
    if (l <= end - beg && strncmp(beg, str, l) == 0)
    {
        *afterStr = beg + l;
        return TRUE;
    }
    *afterStr = beg;
    return FALSE;
}

// returns a null-terminated allocated string from 'beg' to 'end'; on low memory
// returns NULL and stores FALSE in 'ret'
char* AllocString(char* beg, char* end, BOOL* ret)
{
    char* s = (char*)malloc(end - beg + 1);
    if (s != NULL)
    {
        memcpy(s, beg, end - beg);
        s[end - beg] = 0;
    }
    else
    {
        TRACE_E(LOW_MEMORY);
        *ret = FALSE;
    }
    return s;
}

// gradually reads a string from the file lines; the currently processed line is 'line' to 'lineEnd'
// (not null-terminated); 'firstLine' is an IN/OUT variable initialized to
// TRUE, the ReadStrFromLines function changes it to FALSE as soon as it finds the beginning of the string;
// 'dynStr' receives the part of the string read from the currently processed line; the pointers
// 'firstLine' and 'dynStr' do not change during the loading of one string (they act as global
// data); in 'success' (must not be NULL) FALSE is returned on a syntax error or a
// 'dynStr' allocation error; returns TRUE if the entire string has already been read and in 'isNULLStr' returns
// TRUE if it is NULL (otherwise the string is stored in 'dynStr')
BOOL ReadStrFromLines(const char* line, const char* lineEnd, BOOL* firstLine,
                      CDynString* dynStr, BOOL* success, BOOL* isNULLStr)
{
    const char* s = line;
    *isNULLStr = FALSE;
    if (*firstLine)
    {
        while (s < lineEnd && *s <= ' ')
            s++;
        if (s == lineEnd)
            return FALSE; // we still have to find the string
        if (*s != '"')    // the string was supposed to start here
        {
            *success = FALSE; // syntax error
            return TRUE;      // error, stop processing the string
        }
        s++;
        *firstLine = FALSE;
        dynStr->Clear();
        if (s + 2 < lineEnd && strncmp(s, "\\0\"", 3) == 0) // escape sequence for NULL
        {
            *isNULLStr = TRUE;
            return TRUE;
        }
    }
    char buf[WSTF_MAX_LINE_LEN + 2]; // 2 characters of overhead for "\r\n"
    char* d = buf;
    char* end = buf + WSTF_MAX_LINE_LEN + 2;
    BOOL eol = TRUE;
    BOOL foundEnd = FALSE;
    while (s < lineEnd)
    {
        if (*s == '\\')
        {
            if (s + 1 < lineEnd)
                s++;
            else
            {
                eol = FALSE;
                break; // the string continues directly on the next line (without an EOL)
            }
        }
        else
        {
            if (*s == '"')
            {
                foundEnd = TRUE;
                eol = FALSE;
                break; // end of the string found (+ no more EOL)
            }
        }
        if (d + 2 >= end) // need to "flush" the local buffer to 'dynStr' (no space left for a character + EOL)
        {
            if (!dynStr->Append(buf, (int)(d - buf)))
                *success = FALSE; // allocation error
            d = buf;
        }
        *d++ = *s++;
    }
    if (eol)
    {
        *d++ = '\r';
        *d++ = '\n';
    }
    if (d - buf > 0 && !dynStr->Append(buf, (int)(d - buf)))
        *success = FALSE; // allocation error
    return foundEnd;
}

// helper function: if 'isNULLStr' is TRUE, it returns NULL in 'str'; otherwise, if 'success'
// is TRUE, it tries to duplicate the 'dynStr' string into 'str', and on low memory returns
// 'success' as FALSE
void GetStrResult(char** str, BOOL* success, CDynString* dynStr, BOOL isNULLStr)
{
    if (isNULLStr)
        *str = NULL;
    else
    {
        if (*success)
        {
            const char* s = dynStr->GetString();
            if (s == NULL)
                s = ""; // this is not NULL, we must duplicate an empty string
            *str = SalamanderGeneral->DupStr(s);
            if (*str == NULL)
                *success = FALSE; // out of memory, aborting
        }
    }
}

#define IMPORT_FILE_BUF_SIZE 1024 // buffer size for reading from a file

BOOL CServerType::ImportFromFile(HANDLE file, DWORD* err, int* errResID)
{
    *err = NO_ERROR;
    *errResID = 0;
    BOOL ret = TRUE;

    if (CompiledAutodetCond != NULL)
    {
        delete CompiledAutodetCond;
        CompiledAutodetCond = NULL;
    }
    if (CompiledParser != NULL)
    {
        delete CompiledParser;
        CompiledParser = NULL;
    }

    char buffer[IMPORT_FILE_BUF_SIZE];
    int readBytes = 0; // how many bytes have been read
    int i = 0;
    BOOL skipNextEOLN = FALSE; // TRUE = '\r' was already processed, if '\n' is at the start of the buffer it is the remainder of the EOL and should be skipped
    CDynString dynStr;         // shared dynamic string for reading strings of unknown length from the file
    BOOL strFirstLine = TRUE;  // shared helper variable for reading strings of unknown length from the file
    while (ret)
    {
        DWORD read;
        if (!ReadFile(file, buffer + readBytes, IMPORT_FILE_BUF_SIZE - readBytes, &read, NULL))
        {
            *err = GetLastError();
            ret = FALSE;
            break; // aborting with an error
        }
        readBytes += read;
        if (readBytes == 0)
            break; // end of file

        // process the read buffer
        char* end = buffer + readBytes; // end of data in the buffer
        char* s = buffer;               // searching for the end of the line
        char* lineBeg = s;              // start of the line
        while (1)
        {
            if (skipNextEOLN && s < end && *s == '\n') // in case the EOL '\r\n' was split
            {
                lineBeg = ++s;
                skipNextEOLN = FALSE;
            }
            while (s < end && *s != '\r' && *s != '\n')
                s++;
            if (s == end)
            {
                if (readBytes == IMPORT_FILE_BUF_SIZE)
                {
                    if (lineBeg == buffer) // line too long
                    {
                        *errResID = IDS_SRVTYPEIMPMOREDATA;
                        ret = FALSE;
                        break;
                    }
                    else // the rest of the line is no longer in the buffer, read it
                    {
                        memmove(buffer, lineBeg, end - lineBeg);
                        readBytes = (int)(end - lineBeg);
                        break;
                    }
                }
                else // the last line is not terminated by an EOL
                {
                    if (s == lineBeg)
                    { // end of buffer; if this is an empty line, the EOL will be at the start of the next buffer read
                        readBytes = 0;
                        break;
                    }
                }
            }
            // else;  // line ended with an EOL

            // process the line ('lineBeg' to 'lineEnd')
            char* lineEnd = s;
            char* afterTitle;
            BOOL isNULLStr = FALSE;
            switch (i)
            {
            case 0: // signature
            {
                if ((int)strlen(STR_FILE_HEADER) != lineEnd - lineBeg ||
                    strncmp(STR_FILE_HEADER, lineBeg, lineEnd - lineBeg) != 0) // bad signature, not an STR file
                {
                    *errResID = IDS_SRVTYPEIMPNOTSTRFILE;
                    ret = FALSE;
                }
                break;
            }

            case 1: // type name
            {
                if (lineEnd == lineBeg)
                    i--; // skip empty lines
                else
                {
                    if (MatchString(lineBeg, lineEnd, STR_FILE_TYPENAME, &afterTitle))
                    {
                        if (afterTitle < lineEnd && *afterTitle == ' ')
                            afterTitle++; // skip the space after ':'
                        if (afterTitle < lineEnd)
                            TypeName = AllocString(afterTitle, lineEnd, &ret);
                        else
                            ret = FALSE; // error
                    }
                    else
                        ret = FALSE; // error
                }
                break;
            }

            case 2: // header for AutodetectCond
            {
                if (lineEnd == lineBeg)
                    i--; // skip empty lines
                else
                {
                    if (MatchString(lineBeg, lineEnd, STR_FILE_ADCOND, &afterTitle))
                    {
                        strFirstLine = TRUE;
                        if (afterTitle < lineEnd &&
                            ReadStrFromLines(afterTitle, lineEnd, &strFirstLine, &dynStr, &ret, &isNULLStr))
                        {
                            GetStrResult(&AutodetectCond, &ret, &dynStr, isNULLStr);
                            i++; // string fully loaded, skip reading the remainder
                        }
                    }
                    else
                        ret = FALSE; // error
                }
                break;
            }

            case 3: // finish reading the AutodetectCond string
            {
                if (!ReadStrFromLines(lineBeg, lineEnd, &strFirstLine, &dynStr, &ret, &isNULLStr))
                    i--; // the string continues on the next line
                else
                    GetStrResult(&AutodetectCond, &ret, &dynStr, isNULLStr);
                break;
            }

            case 4: // header for Columns
            {
                if (lineEnd == lineBeg)
                    i--; // skip empty lines
                else
                {
                    if (MatchString(lineBeg, lineEnd, STR_FILE_COLUMNS, &afterTitle))
                    {
                        strFirstLine = TRUE;
                        while (afterTitle < lineEnd && *afterTitle <= ' ')
                            afterTitle++; // skip whitespace after ':'
                        if (afterTitle < lineEnd)
                            ret = FALSE; // syntax error, nothing should be here
                    }
                    else
                        ret = FALSE; // error
                }
                break;
            }

            case 5: // load columns
            {
                BOOL read2 = TRUE;
                if (strFirstLine) // the string has not started yet, check whether there is still a column on the line or whether they have ended
                {
                    char* s2 = lineBeg;
                    while (s2 < lineEnd && *s2 <= ' ')
                        s2++;
                    if (s2 < lineEnd && *lineBeg != '"') // this line is no longer ours (the column description is a string)
                    {
                        read2 = FALSE;
                        i++;
                    }
                }
                if (read2)
                {
                    if (!ReadStrFromLines(lineBeg, lineEnd, &strFirstLine, &dynStr, &ret, &isNULLStr))
                        i--; // the string continues on the next line
                    else
                    {
                        if (isNULLStr)
                            ret = FALSE; // syntax error, NULL cannot appear here
                        if (ret)
                        {
                            CSrvTypeColumn* c = new CSrvTypeColumn;
                            if (c != NULL && c->IsGood() && c->LoadFromStr(dynStr.GetString()))
                            {
                                Columns.Add(c);
                                if (Columns.IsGood())
                                {
                                    i--;
                                    strFirstLine = TRUE; // go read the next line with column data
                                }
                                else
                                {
                                    delete c;
                                    Columns.ResetState();
                                    ret = FALSE; // out of memory
                                }
                            }
                            else
                            {
                                if (c != NULL)
                                {
                                    if (c->IsGood())
                                        TRACE_E("Unable to load CSrvTypeColumn from string: " << dynStr.GetString());
                                    delete c;
                                }
                                else
                                    TRACE_E(LOW_MEMORY);
                                ret = FALSE; // out of memory or syntax error
                            }
                        }
                    }
                    break;
                }
                // else break;  // break must not be here (the received line is not ours, pass it on)
            }
            case 6: // load RulesForParsing
            {
                if (lineEnd == lineBeg)
                    i--; // skip empty lines
                else
                {
                    if (MatchString(lineBeg, lineEnd, STR_FILE_RULES, &afterTitle))
                    {
                        strFirstLine = TRUE;
                        if (afterTitle < lineEnd &&
                            ReadStrFromLines(afterTitle, lineEnd, &strFirstLine, &dynStr, &ret, &isNULLStr))
                        {
                            GetStrResult(&RulesForParsing, &ret, &dynStr, isNULLStr);
                            i++; // string fully loaded, skip reading the remainder
                        }
                    }
                    else
                        ret = FALSE; // error
                }
                break;
            }

            case 7: // finish reading the RulesForParsing string
            {
                if (!ReadStrFromLines(lineBeg, lineEnd, &strFirstLine, &dynStr, &ret, &isNULLStr))
                    i--; // the string continues on the next line
                else
                    GetStrResult(&RulesForParsing, &ret, &dynStr, isNULLStr);
                break;
            }

            default: // all expected lines have already been read
            {
                if (lineEnd - lineBeg == 0)
                    i--; // skip empty lines
                else
                {
                    *errResID = IDS_SRVTYPEIMPMOREDATA;
                    ret = FALSE;
                }
                break;
            }
            }
            if (!ret)
            {
                if (*errResID == 0)
                    *errResID = IDS_SRVTYPEIMPSYNERR;
                break; // error, aborting
            }
            i++;

            // skip the EOL
            if (s < end && *s == '\r' && ++s == end)
                skipNextEOLN = TRUE;
            if (s < end && *s == '\n')
                s++;
            lineBeg = s;
        }
    }

    // verify that the list of columns is valid
    if (ret)
        ret = ValidateSrvTypeColumns(&Columns, errResID);

    // verify that the parsing rules are valid
    if (ret)
    {
        CFTPParser* parser = CompileParsingRules(HandleNULLStr(RulesForParsing), &Columns, NULL, NULL, NULL);
        if (parser != NULL)
            delete parser; // parser is OK, discard it again
        else
        {
            ret = FALSE; // error in the rules
            *errResID = IDS_SRVTYPEIMPERRINRULES;
        }
    }

    // verify that the autodetect condition is valid
    if (ret)
    {
        CFTPAutodetCondNode* node = CompileAutodetectCond(HandleNULLStr(AutodetectCond), NULL, NULL, NULL, NULL, 0);
        if (node != NULL)
            delete node; // condition is OK, discard it again
        else
        {
            ret = FALSE; // error in the condition
            *errResID = IDS_SRVTYPEIMPERRINACOND;
        }
    }

    return ret;
}

//
// ***********************************************************************************
// Functions ProcessProxyScript plus helper function ExpandText plus function GetProxyScriptText
// ***********************************************************************************
//

CProxyScriptParams::CProxyScriptParams(CFTPProxyServer* proxyServer, const char* host,
                                       int port, const char* user, const char* password,
                                       const char* account, BOOL allowEmptyPassword)
{
    if (proxyServer == NULL)
    {
        ProxyHost[0] = 0;
        ProxyPort = 21;
        ProxyUser[0] = 0;
        ProxyPassword[0] = 0;
    }
    else
    {
        lstrcpyn(ProxyHost, HandleNULLStr(proxyServer->ProxyHost), HOST_MAX_SIZE);
        ProxyPort = proxyServer->ProxyPort;
        lstrcpyn(ProxyUser, HandleNULLStr(proxyServer->ProxyUser), USER_MAX_SIZE);
        lstrcpyn(ProxyPassword, HandleNULLStr(proxyServer->ProxyPlainPassword), PASSWORD_MAX_SIZE);
    }

    lstrcpyn(Host, host, HOST_MAX_SIZE);
    Port = port;
    if (user == NULL)
        User[0] = 0;
    else
        lstrcpyn(User, user, USER_MAX_SIZE);
    if (password == NULL)
        Password[0] = 0;
    else
        lstrcpyn(Password, password, PASSWORD_MAX_SIZE);
    if (account == NULL)
        Account[0] = 0;
    else
        lstrcpyn(Account, account, ACCOUNT_MAX_SIZE);

    NeedProxyHost = FALSE;
    NeedProxyPassword = FALSE;
    NeedUser = FALSE;
    NeedPassword = FALSE;
    NeedAccount = FALSE;

    AllowEmptyPassword = allowEmptyPassword;
}

CProxyScriptParams::CProxyScriptParams()
{
    ProxyHost[0] = 0;
    ProxyPort = 21;
    ProxyUser[0] = 0;
    ProxyPassword[0] = 0;
    Host[0] = 0;
    Port = 21;
    User[0] = 0;
    Password[0] = 0;
    Account[0] = 0;

    NeedProxyHost = FALSE;
    NeedProxyPassword = FALSE;
    NeedUser = FALSE;
    NeedPassword = FALSE;
    NeedAccount = FALSE;

    AllowEmptyPassword = FALSE;
}

// 'buf' with size 'bufSize' is the buffer for the resulting text (host or command);
// 'logBuf' with size 'logBufSize' is the buffer for the resulting text that can be published in the log
// (commands only - passwords are not published, they are replaced with the word "hidden"); 'strBeg' to 'strEnd'
// is the script text to be expanded; 'hostVarsOnly' is TRUE when expanding
// the host (only the Host and ProxyHost variables are allowed, and CRLF is not appended at the end as it is for commands);
// 'proxyHostNeeded' (if not NULL) returns TRUE when the $(ProxyHost) variable is used during the expansion
// return values:
// - error: returns FALSE, the error code is returned in 'errCode' and '*errorPos' gives the error position
// - the line should be skipped: returns TRUE with 'skipThisLine'==TRUE
// - missing variable values: returns FALSE, but 'errCode' is 0
// - everything OK: returns TRUE with text in 'buf'
BOOL ExpandText(char* buf, int bufSize, char* logBuf, int logBufSize, const char* strBeg, const char* strEnd,
                CProxyScriptParams* scriptParams, const char** errorPos, int* errCode,
                BOOL hostVarsOnly, BOOL* skipThisLine, BOOL* proxyHostNeeded)
{
    BOOL ret = TRUE;
    DWORD needUserInput = 0; // != 0 - the user should enter the values of the marked variables (bitwise ORed in this DWORD)
    BOOL localSkipThisLine = FALSE;
    if (skipThisLine != NULL)
        *skipThisLine = FALSE;
    char* bufEnd;
    if (bufSize > 0)
        bufEnd = buf + bufSize - 1;
    else
        bufEnd = NULL;
    char* logBufEnd;
    if (logBufSize > 0)
        logBufEnd = logBuf + logBufSize - 1;
    else
        logBufEnd = NULL;
    const char* s = strBeg;
    char* txt = buf;
    char* logTxt = logBuf;
    while (s < strEnd)
    {
        if (*s == '$')
        {
            if (s + 1 < strEnd && *(s + 1) == '$') // escape sequence for '$'
            {
                if (txt < bufEnd)
                    *txt++ = '$';
                if (logTxt < logBufEnd)
                    *logTxt++ = '$';
                s += 2;
            }
            else
            {
                if (s + 1 < strEnd && *(s + 1) == '(')
                {
                    s += 2;
                    int i;
                    for (i = 0; i < 9; i++)
                    {
                        const char* varName;
                        switch (i)
                        {
                        case 0:
                            varName = "ProxyHost";
                            break;
                        case 1:
                            varName = "ProxyPort";
                            break;
                        case 2:
                            varName = "ProxyUser";
                            break;
                        case 3:
                            varName = "ProxyPassword";
                            break;
                        case 4:
                            varName = "Host";
                            break;
                        case 5:
                            varName = "Port";
                            break;
                        case 6:
                            varName = "User";
                            break;
                        case 7:
                            varName = "Password";
                            break;
                        case 8:
                            varName = "Account";
                            break;
                        }
                        int varNameLen = (int)strlen(varName);
                        if (s + varNameLen < strEnd && *(s + varNameLen) == ')' &&
                            SalamanderGeneral->StrNICmp(s, varName, varNameLen) == 0) // variable found
                        {
                            if (i == 0 && proxyHostNeeded != NULL)
                                *proxyHostNeeded = TRUE;
                            if (hostVarsOnly && i != 0 && i != 4)
                            {
                                *errorPos = s;
                                *errCode = IDS_PRXSCRERR_HOSTVARSONLY; // variable not allowed
                                ret = FALSE;
                                break;
                            }
                            if (scriptParams != NULL) // if we have variable values, insert the variable value
                            {
                                char portBuf[20];
                                const char* value = "";
                                BOOL hidden = FALSE;
                                switch (i)
                                {
                                case 0:
                                {
                                    if (scriptParams->ProxyHost[0] != 0)
                                        value = scriptParams->ProxyHost;
                                    else
                                        needUserInput |= 1;
                                    break;
                                }

                                case 1:
                                {
                                    _itoa(scriptParams->ProxyPort, portBuf, 10);
                                    value = portBuf;
                                    break;
                                }

                                case 2:
                                {
                                    if (scriptParams->ProxyUser[0] != 0)
                                        value = scriptParams->ProxyUser;
                                    else
                                        localSkipThisLine = TRUE;
                                    break;
                                }

                                case 3:
                                {
                                    hidden = TRUE;
                                    if (scriptParams->ProxyPassword[0] != 0)
                                        value = scriptParams->ProxyPassword;
                                    else
                                        needUserInput |= 1 << 3;
                                    break;
                                }

                                case 4:
                                    value = scriptParams->Host;
                                    break;

                                case 5:
                                {
                                    _itoa(scriptParams->Port, portBuf, 10);
                                    value = portBuf;
                                    break;
                                }

                                case 6:
                                {
                                    if (scriptParams->User[0] != 0)
                                        value = scriptParams->User;
                                    else
                                        needUserInput |= 1 << 6;
                                    break;
                                }

                                case 7:
                                {
                                    hidden = TRUE;
                                    if (scriptParams->Password[0] != 0 || scriptParams->AllowEmptyPassword)
                                    {
                                        scriptParams->AllowEmptyPassword = FALSE;
                                        value = scriptParams->Password;
                                    }
                                    else
                                        needUserInput |= 1 << 7;
                                    break;
                                }

                                case 8:
                                {
                                    hidden = TRUE;
                                    if (scriptParams->Account[0] != 0)
                                        value = scriptParams->Account;
                                    else
                                        needUserInput |= 1 << 8;
                                    break;
                                }
                                }
                                int valLen = (int)strlen(value);
                                if (txt < bufEnd)
                                {
                                    if (valLen > bufEnd - txt)
                                        valLen = (int)(bufEnd - txt); // trim text, destination buffer is too small
                                    memmove(txt, value, valLen);
                                    txt += valLen;
                                }
                                if (hidden)
                                    value = LoadStr(IDS_HIDDENPASSWORD);
                                valLen = (int)strlen(value);
                                if (hidden && logTxt < logBufEnd)
                                    *logTxt++ = '(';
                                if (logTxt < logBufEnd)
                                {
                                    if (valLen > logBufEnd - logTxt)
                                        valLen = (int)(logBufEnd - logTxt); // trim text, destination buffer is too small
                                    memmove(logTxt, value, valLen);
                                    logTxt += valLen;
                                }
                                if (hidden && logTxt < logBufEnd)
                                    *logTxt++ = ')';
                            }
                            s += varNameLen + 1; // skip the variable
                            break;
                        }
                    }
                    if (i == 9)
                    {
                        *errorPos = s;
                        *errCode = IDS_PRXSCRERR_UNKNOWNVAR; // unknown variable
                        ret = FALSE;
                    }
                    if (*errCode != 0)
                        break; // stop
                }
                else // something other than '(' and '$' follows '$' - copy 1:1
                {
                    if (txt < bufEnd)
                        *txt++ = *s;
                    if (logTxt < logBufEnd)
                        *logTxt++ = *s;
                    s++;
                }
            }
        }
        else
        {
            if (txt < bufEnd)
                *txt++ = *s;
            if (logTxt < logBufEnd)
                *logTxt++ = *s;
            s++;
        }
    }
    if (ret)
    {
        if (localSkipThisLine && skipThisLine != NULL)
            *skipThisLine = TRUE;
        if (!localSkipThisLine && needUserInput != 0) // transfer flags from needUserInput to scriptParams->NeedXXX
        {
            ret = FALSE;
            int i;
            for (i = 0; i < 9; i++)
            {
                if (needUserInput & (1 << i)) // necessarily means that 'scriptParams' is not NULL
                {
                    switch (i)
                    {
                    case 0:
                        scriptParams->NeedProxyHost = TRUE;
                        break;
                    case 3:
                        scriptParams->NeedProxyPassword = TRUE;
                        break;
                    case 6:
                        scriptParams->NeedUser = TRUE;
                        break;
                    case 7:
                        scriptParams->NeedPassword = TRUE;
                        break;
                    case 8:
                        scriptParams->NeedAccount = TRUE;
                        break;
                    }
                }
            }
        }
    }
    if (localSkipThisLine || !ret)
    {
        if (bufSize > 0)
            buf[0] = 0; // just to be safe
        if (logBufSize > 0)
            logBuf[0] = 0; // just to be safe
    }
    else
    {
        if (!hostVarsOnly) // append CRLF for FTP commands
        {
            if (txt < bufEnd)
                *txt++ = '\r';
            if (txt < bufEnd)
                *txt++ = '\n';
            if (logTxt < logBufEnd)
                *logTxt++ = '\r';
            if (logTxt < logBufEnd)
                *logTxt++ = '\n';
        }
        if (bufSize > 0)
            *txt = 0;
        if (logBufSize > 0)
            *logTxt = 0;
    }
    return ret;
}

BOOL ProcessProxyScript(const char* script, const char** execPoint, int lastCmdReply,
                        CProxyScriptParams* scriptParams, char* hostBuf, unsigned short* port,
                        char* sendCmdBuf, char* logCmdBuf, char* errDescrBuf,
                        BOOL* proxyHostNeeded)
{
    CALL_STACK_MESSAGE2("ProcessProxyScript(, , %d, , , , , , ,)", lastCmdReply);

    if (script == NULL || execPoint == NULL)
    {
        TRACE_E("ProcessProxyScript(): script and execPoint may not be NULL!");
        return FALSE;
    }
    if (scriptParams != NULL)
    {
        scriptParams->NeedProxyHost = FALSE;
        scriptParams->NeedProxyPassword = FALSE;
        scriptParams->NeedUser = FALSE;
        scriptParams->NeedPassword = FALSE;
        scriptParams->NeedAccount = FALSE;
    }
    if (hostBuf != NULL)
        *hostBuf = 0;
    if (port != NULL)
        *port = 21;
    if (sendCmdBuf != NULL)
        *sendCmdBuf = 0;
    if (logCmdBuf != NULL)
        *logCmdBuf = 0;
    if (errDescrBuf != NULL)
        *errDescrBuf = 0;
    if (proxyHostNeeded != NULL)
        *proxyHostNeeded = FALSE;
    int errCode = 0;
    const char* s = *execPoint == NULL ? script : *execPoint;
    BOOL validateScript = scriptParams == NULL;
    BOOL processNextLine = s > script; // TRUE = we should process the next line with a command in the script
    if (s == script)                   // we are at the beginning of the script
    {
        while (*s != 0 && *s <= ' ')
            s++; // skip white-spaces
        if (SalamanderGeneral->StrNICmp(s, "Connect to:", 11) == 0)
        {
            s += 11;
            while (*s != 0 && *s <= ' ' && *s != '\r' && *s != '\n')
                s++; // skip white-spaces to EOL
            const char* host = s;
            while (*s > ' ' && *s != ':')
                s++; // look for the end of the host
            if (s > host)
            {
                const char* hostEnd = s;
                const char* portStr = NULL;
                const char* portEnd = NULL;
                if (*s == ':')
                {
                    s++;
                    while (*s != 0 && *s <= ' ' && *s != '\r' && *s != '\n')
                        s++; // skip white-spaces to EOL
                    portStr = s;
                    while (*s > ' ')
                        s++; // look for the end of the port
                    if (s > portStr)
                        portEnd = s;
                    else
                        portStr = NULL;
                }
                while (*s != 0 && *s <= ' ' && *s != '\r' && *s != '\n')
                    s++; // skip white-spaces to EOL
                if (*s == 0 || *s == '\r' || *s == '\n')
                {
                    // 'host' to 'hostEnd' is the host; 'portStr' to 'portEnd' is the port ('portStr'==NULL -> port 21)
                    if (portStr != NULL)
                    {
                        if (portEnd - portStr == 12 && SalamanderGeneral->StrNICmp(portStr, "$(ProxyPort)", 12) == 0)
                        {
                            if (scriptParams != NULL && port != NULL)
                                *port = (unsigned short)scriptParams->ProxyPort;
                        }
                        else
                        {
                            if (portEnd - portStr == 7 && SalamanderGeneral->StrNICmp(portStr, "$(Port)", 7) == 0)
                            {
                                if (scriptParams != NULL && port != NULL)
                                    *port = (unsigned short)scriptParams->Port;
                            }
                            else
                            {
                                int portNum = 0;
                                const char* n = portStr;
                                while (n < portEnd && *n >= '0' && *n <= '9')
                                {
                                    portNum = 10 * portNum + (*n - '0');
                                    n++;
                                }
                                if (n == portEnd)
                                {
                                    if (portNum >= 1 && portNum <= 65535)
                                    {
                                        if (port != NULL)
                                            *port = (unsigned short)portNum;
                                    }
                                    else
                                    {
                                        s = portStr;
                                        errCode = IDS_PORTISUSHORT;
                                    }
                                }
                                else
                                {
                                    s = n;
                                    errCode = IDS_PRXSCRERR_INVPORT;
                                }
                            }
                        }
                    }
                    if (errCode == 0) // port is OK, continue with the host
                    {
                        if (ExpandText(hostBuf, hostBuf == NULL ? 0 : HOST_MAX_SIZE, NULL, 0,
                                       host, hostEnd, scriptParams, &s, &errCode, TRUE, NULL,
                                       proxyHostNeeded))
                        {
                            if (*s == '\r')
                                s++;
                            if (*s == '\n')
                                s++;
                            if (validateScript)
                                processNextLine = TRUE;
                        }
                    }
                }
                else
                    errCode = IDS_PRXSCRERR_INVHOSTORPORT;
            }
            else
                errCode = IDS_PRXSCRERR_HOSTEMPTY;
        }
        else
            errCode = IDS_PRXSCRERR_INVSTART;
    }

    BOOL testIfFirstCmdLineIs3xx = validateScript;
    BOOL errorNoCommandsInScript = processNextLine && validateScript;
    while (processNextLine && *s != 0) // processing command lines
    {
        processNextLine = FALSE;
        while (*s != 0 && *s <= ' ')
            s++; // skip white-spaces (including EOL)
        BOOL sendOnlyIf3xxReply = SalamanderGeneral->StrNICmp(s, "3xx:", 4) == 0;
        if (sendOnlyIf3xxReply)
        {
            if (testIfFirstCmdLineIs3xx) // on the first line, "3xx:" makes no sense (no previous command, therefore no reply)
            {
                errCode = IDS_PRXSCRERR_3XXONFIRSTLINE;
                break;
            }
            else
            {
                s += 4;
                while (*s != 0 && *s <= ' ' && *s != '\r' && *s != '\n')
                    s++; // skip white-spaces (without EOL)
            }
        }
        testIfFirstCmdLineIs3xx = FALSE;
        const char* lineBeg = s;
        while (*s != 0 && *s != '\r' && *s != '\n')
            s++; // to the end of the script or end of the line

        if (s > lineBeg &&
            (validateScript || !sendOnlyIf3xxReply ||
             lastCmdReply != -1 && FTP_DIGIT_1(lastCmdReply) == FTP_D1_PARTIALSUCCESS /* 3xx */))
        {
            BOOL skipThisLine;
            if (ExpandText(sendCmdBuf, sendCmdBuf == NULL ? 0 : FTPCOMMAND_MAX_SIZE,
                           logCmdBuf, logCmdBuf == NULL ? 0 : FTPCOMMAND_MAX_SIZE,
                           lineBeg, s, scriptParams, &s, &errCode, FALSE, &skipThisLine, NULL))
            {
                if (skipThisLine) // the line should be skipped (contains an optional variable)
                {
                    lastCmdReply = -1;
                    processNextLine = TRUE;
                }
                else // everything OK, return the command and the text for the log
                {
                    if (*s == '\r')
                        s++;
                    if (*s == '\n')
                        s++;
                }
                if (validateScript)
                    processNextLine = TRUE;
                errorNoCommandsInScript = FALSE;
            }
        }
        else // empty line at the end of the script or after "3xx:" or a line skipped due to the "3xx:" condition
        {
            if (s > lineBeg)
                lastCmdReply = -1; // just skip the empty line (pretend it was not there)
            processNextLine = TRUE;
        }
    }

    if (errCode == 0 && errorNoCommandsInScript)
        errCode = IDS_PRXSCRERR_NONECMDS;
    if (errCode != 0 || scriptParams == NULL || !scriptParams->NeedUserInput())
        *execPoint = s;
    if (errCode != 0 && errDescrBuf != NULL) // error text not yet in the buffer, text resource ID stored in errCode
        lstrcpyn(errDescrBuf, LoadStr(errCode), 300);
    return errCode == 0;
}

const char* GetProxyScriptText(CFTPProxyServerType type, BOOL textForDialog)
{
    switch (type)
    {
    case fpstNotUsed: // "direct connection" (without a proxy server)
        return "Connect to: $(Host):$(Port)\r\n"
               "USER $(User)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstSocks4:
    case fpstSocks4A:
    case fpstSocks5:
    case fpstHTTP1_1:
        return textForDialog ? "" : "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
                                    "USER $(User)\r\n"
                                    "3xx: PASS $(Password)\r\n"
                                    "3xx: ACCT $(Account)\r\n";

    case fpstFTP_SITE_host_colon_port: // USER fw_user;PASS fw_pass;SITE host:port;USER user;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "SITE $(Host):$(Port)\r\n"
               "USER $(User)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_SITE_host_space_port: // USER fw_user;PASS fw_pass;SITE host port;USER user;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "SITE $(Host) $(Port)\r\n"
               "USER $(User)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_SITE_user_host_colon_port: // USER fw_user;PASS fw_pass;SITE user@host:port;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "SITE $(User)@$(Host):$(Port)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_SITE_user_host_space_port: // USER fw_user;PASS fw_pass;SITE user@host port;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "SITE $(User)@$(Host) $(Port)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_OPEN_host_port: // USER fw_user;PASS fw_pass;OPEN host:port;USER user;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "OPEN $(Host):$(Port)\r\n"
               "USER $(User)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_transparent: // (fw_host+fw_port are not used - connect to host+port) USER fw_user;PASS fw_pass;USER user;PASS pass;ACCT acct
        return "Connect to: $(Host):$(Port)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "USER $(User)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_USER_user_host_colon_port: // USER fw_user;PASS fw_pass;USER user@host:port;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "USER $(User)@$(Host):$(Port)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_USER_user_host_space_port: // USER fw_user;PASS fw_pass;USER user@host port;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "USER $(User)@$(Host) $(Port)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_USER_fireuser_host: // USER fw_user@host:port;PASS fw_pass;USER user;PASS pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(ProxyUser)@$(Host):$(Port)\r\n"
               "3xx: PASS $(ProxyPassword)\r\n"
               "USER $(User)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_USER_user_host_fireuser: // USER user@host:port fw_user;PASS pass;ACCT fw_pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(User)@$(Host):$(Port) $(ProxyUser)\r\n"
               "3xx: PASS $(Password)\r\n"
               "3xx: ACCT $(ProxyPassword)\r\n"
               "3xx: ACCT $(Account)\r\n";

    case fpstFTP_USER_user_fireuser_host: // USER user@fw_user@host:port;PASS pass@fw_pass;ACCT acct
        return "Connect to: $(ProxyHost):$(ProxyPort)\r\n"
               "USER $(User)@$(ProxyUser)@$(Host):$(Port)\r\n"
               "3xx: PASS $(Password)@$(ProxyPassword)\r\n"
               "3xx: ACCT $(Account)\r\n";

    default:
        return "";
    }
}

unsigned short GetProxyDefaultPort(CFTPProxyServerType type)
{
    switch (type)
    {
    case fpstSocks4:
    case fpstSocks4A:
    case fpstSocks5:
        return 1080;

    case fpstHTTP1_1:
        return 8080;

    case fpstFTP_transparent:
        return 0; // (fw_host+fw_port are not used - connect to host+port) USER fw_user;PASS fw_pass;USER user;PASS pass;ACCT acct

        //    case fpstNotUsed:                        // "direct connection" (without a proxy server)
        //    case fpstFTP_SITE_host_colon_port:       // USER fw_user;PASS fw_pass;SITE host:port;USER user;PASS pass;ACCT acct
        //    case fpstFTP_SITE_host_space_port:       // USER fw_user;PASS fw_pass;SITE host port;USER user;PASS pass;ACCT acct
        //    case fpstFTP_SITE_user_host_colon_port:  // USER fw_user;PASS fw_pass;SITE user@host:port;PASS pass;ACCT acct
        //    case fpstFTP_SITE_user_host_space_port:  // USER fw_user;PASS fw_pass;SITE user@host port;PASS pass;ACCT acct
        //    case fpstFTP_OPEN_host_port:             // USER fw_user;PASS fw_pass;OPEN host:port;USER user;PASS pass;ACCT acct
        //    case fpstFTP_USER_user_host_colon_port:  // USER fw_user;PASS fw_pass;USER user@host:port;PASS pass;ACCT acct
        //    case fpstFTP_USER_user_host_space_port:  // USER fw_user;PASS fw_pass;USER user@host port;PASS pass;ACCT acct
        //    case fpstFTP_USER_fireuser_host:         // USER fw_user@host:port;PASS fw_pass;USER user;PASS pass;ACCT acct
        //    case fpstFTP_USER_user_host_fireuser:    // USER user@host:port fw_user;PASS pass;ACCT fw_pass;ACCT acct
        //    case fpstFTP_USER_user_fireuser_host:    // USER user@fw_user@host:port;PASS pass@fw_pass;ACCT acct
        //    case fpstOwnScript:                      // the user wrote their own script for connecting to the FTP server
    default:
        return 21;
    }
}

BOOL HavePassword(CFTPProxyServerType type)
{
    return type != fpstSocks4 && type != fpstSocks4A;
}

BOOL HaveHostAndPort(CFTPProxyServerType type)
{
    return type != fpstFTP_transparent;
}

const char* GetProxyTypeName(CFTPProxyServerType type)
{
    switch (type)
    {
    case fpstSocks4:
        return LoadStr(IDS_PROXYSERVER_SOCKS4);
    case fpstSocks4A:
        return LoadStr(IDS_PROXYSERVER_SOCKS4A);
    case fpstSocks5:
        return LoadStr(IDS_PROXYSERVER_SOCKS5);
    case fpstHTTP1_1:
        return LoadStr(IDS_PROXYSERVER_HTTP11);
    case fpstFTP_SITE_host_colon_port:
        return LoadStr(IDS_PROXYSERVER_SITE1);
    case fpstFTP_SITE_host_space_port:
        return LoadStr(IDS_PROXYSERVER_SITE2);
    case fpstFTP_SITE_user_host_colon_port:
        return LoadStr(IDS_PROXYSERVER_SITE3);
    case fpstFTP_SITE_user_host_space_port:
        return LoadStr(IDS_PROXYSERVER_SITE4);
    case fpstFTP_OPEN_host_port:
        return LoadStr(IDS_PROXYSERVER_OPEN);
    case fpstFTP_transparent:
        return LoadStr(IDS_PROXYSERVER_TRANSPAR);
    case fpstFTP_USER_user_host_colon_port:
        return LoadStr(IDS_PROXYSERVER_USER1);
    case fpstFTP_USER_user_host_space_port:
        return LoadStr(IDS_PROXYSERVER_USER2);
    case fpstFTP_USER_fireuser_host:
        return LoadStr(IDS_PROXYSERVER_USER3);
    case fpstFTP_USER_user_host_fireuser:
        return LoadStr(IDS_PROXYSERVER_USER4);
    case fpstFTP_USER_user_fireuser_host:
        return LoadStr(IDS_PROXYSERVER_USER5);
    case fpstOwnScript:
        return LoadStr(IDS_PROXYSERVER_USERDEF);
    default:
        TRACE_E("GetProxyTypeName(): unknown proxy server type!");
        return "";
    }
}

BOOL IsSOCKSOrHTTPProxy(CFTPProxyServerType type)
{
    switch (type)
    {
    case fpstSocks4:
    case fpstSocks4A:
    case fpstSocks5:
    case fpstHTTP1_1:
        return TRUE;

    default:
        return FALSE;
    }
}

//
// ****************************************************************************
// CFTPProxyForDataCon
//

CFTPProxyForDataCon::CFTPProxyForDataCon(CFTPProxyServerType proxyType, DWORD proxyHostIP,
                                         unsigned short proxyPort, const char* proxyUser,
                                         const char* proxyPassword, const char* host,
                                         DWORD hostIP, unsigned short hostPort)
{
    ProxyType = proxyType;
    ProxyHostIP = proxyHostIP;
    ProxyPort = proxyPort;
    ProxyUser = SalamanderGeneral->DupStr(proxyUser);
    ProxyPassword = SalamanderGeneral->DupStr(proxyPassword);
    Host = SalamanderGeneral->DupStr(host);
    HostIP = hostIP;
    HostPort = hostPort;
}

CFTPProxyForDataCon::~CFTPProxyForDataCon()
{
    if (ProxyUser != NULL)
        free(ProxyUser);
    if (ProxyPassword != NULL)
        free(ProxyPassword);
    if (Host != NULL)
        free(Host);
}

//
// ****************************************************************************
// CFTPProxyServer
//

void CFTPProxyServer::Init(int proxyUID)
{
    ProxyUID = proxyUID;
    ProxyName = NULL;
    ProxyType = fpstFTP_USER_user_host_colon_port;
    ProxyHost = NULL;
    ProxyPort = 21;
    ProxyUser = NULL;
    ProxyEncryptedPassword = NULL;
    ProxyEncryptedPasswordSize = 0;
    ProxyPlainPassword = NULL;
    SaveProxyPassword = FALSE;
    ProxyScript = NULL;

    // NOTE: the default values here must match the default values used in the Save() method
}

void CFTPProxyServer::Release()
{
    if (ProxyName != NULL)
        free(ProxyName);
    if (ProxyHost != NULL)
        free(ProxyHost);
    if (ProxyUser != NULL)
        free(ProxyUser);
    if (ProxyEncryptedPassword != NULL)
    {
        memset(ProxyEncryptedPassword, 0, ProxyEncryptedPasswordSize); // cleaning memory containing the password
        SalamanderGeneral->Free(ProxyEncryptedPassword);
    }
    if (ProxyPlainPassword != NULL)
    {
        memset(ProxyPlainPassword, 0, strlen(ProxyPlainPassword)); // cleaning memory containing the password
        SalamanderGeneral->Free(ProxyPlainPassword);
    }
    if (ProxyScript != NULL)
        free(ProxyScript);
    Init(ProxyUID);
}

CFTPProxyServer*
CFTPProxyServer::MakeCopy()
{
    CFTPProxyServer* n = new CFTPProxyServer(0);
    if (n != NULL)
    {
        n->ProxyUID = ProxyUID;
        n->ProxyName = SalamanderGeneral->DupStr(ProxyName);
        n->ProxyType = ProxyType;
        n->ProxyHost = SalamanderGeneral->DupStr(ProxyHost);
        n->ProxyPort = ProxyPort;
        n->ProxyUser = SalamanderGeneral->DupStr(ProxyUser);
        n->ProxyEncryptedPassword = DupEncryptedPassword(ProxyEncryptedPassword, ProxyEncryptedPasswordSize);
        n->ProxyEncryptedPasswordSize = ProxyEncryptedPasswordSize;
        n->ProxyPlainPassword = SalamanderGeneral->DupStr(ProxyPlainPassword);
        n->SaveProxyPassword = SaveProxyPassword;
        n->ProxyScript = SalamanderGeneral->DupStr(ProxyScript);
    }
    else
        TRACE_E(LOW_MEMORY);
    return n;
}

CFTPProxyForDataCon*
CFTPProxyServer::AllocProxyForDataCon(DWORD proxyHostIP, const char* host,
                                      DWORD hostIP, unsigned short hostPort)
{
    CFTPProxyForDataCon* n = new CFTPProxyForDataCon(ProxyType, proxyHostIP, ProxyPort,
                                                     ProxyUser, ProxyPlainPassword, host,
                                                     hostIP, hostPort);
    if (n == NULL)
        TRACE_E(LOW_MEMORY);
    return n;
}

BOOL CFTPProxyServer::SetProxyHost(const char* proxyHost)
{
    BOOL err = FALSE;
    UpdateStr(ProxyHost, proxyHost, &err);
    return !err;
}

void CFTPProxyServer::SetProxyPort(int proxyPort)
{
    ProxyPort = proxyPort;
}

BOOL CFTPProxyServer::SetProxyPassword(const char* proxyPassword)
{
    BOOL err = FALSE;
    UpdateStr(ProxyPlainPassword, proxyPassword, &err, TRUE);
    return !err;
}

BOOL CFTPProxyServer::SetProxyUser(const char* proxyUser)
{
    BOOL err = FALSE;
    UpdateStr(ProxyUser, proxyUser, &err);
    return !err;
}

BOOL CFTPProxyServer::Set(int proxyUID,
                          const char* proxyName,
                          CFTPProxyServerType proxyType,
                          const char* proxyHost,
                          int proxyPort,
                          const char* proxyUser,
                          const BYTE* proxyEncryptedPassword,
                          int proxyEncryptedPasswordSize,
                          int saveProxyPassword,
                          const char* proxyScript)
{
    BOOL err = FALSE;
    ProxyUID = proxyUID;
    UpdateStr(ProxyName, proxyName, &err);
    ProxyType = proxyType;
    UpdateStr(ProxyHost, proxyHost, &err);
    ProxyPort = proxyPort;
    UpdateStr(ProxyUser, proxyUser, &err);
    UpdateEncryptedPassword(&ProxyEncryptedPassword, &ProxyEncryptedPasswordSize, proxyEncryptedPassword, proxyEncryptedPasswordSize);
    UpdateStr(ProxyPlainPassword, NULL, &err, TRUE);
    SaveProxyPassword = saveProxyPassword;
    UpdateStr(ProxyScript, proxyScript, &err);
    return !err;
}

BOOL CFTPProxyServer::Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    int proxyUID;
    char proxyName[PROXYSRVNAME_MAX_SIZE];
    DWORD proxyType;
    char proxyHost[HOST_MAX_SIZE];
    int proxyPort;
    char proxyUser[USER_MAX_SIZE];
    BYTE* proxyEncryptedPassword;
    int proxyEncryptedPasswordSize;
    int saveProxyPassword;
    char proxyScript[PROXYSCRIPT_MAX_SIZE];

    // take over default values (the object is clean, just initialized)
    proxyType = ProxyType;
    strcpy(proxyHost, HandleNULLStr(ProxyHost));
    proxyPort = ProxyPort;
    strcpy(proxyUser, HandleNULLStr(ProxyUser));
    proxyEncryptedPassword = NULL;
    proxyEncryptedPasswordSize = 0;
    saveProxyPassword = FALSE;
    strcpy(proxyScript, HandleNULLStr(ProxyScript));

    if (!registry->GetValue(regKey, CONFIG_FTPPRXUID, REG_DWORD, &proxyUID, sizeof(DWORD)))
    {
        TRACE_E("Unexpected error in CFTPProxyServer::Load(): UID of proxy server was not found!");
        return FALSE; // UID is mandatory
    }
    if (!registry->GetValue(regKey, CONFIG_FTPPRXNAME, REG_SZ, proxyName, PROXYSRVNAME_MAX_SIZE) ||
        proxyName[0] == 0)
    {
        TRACE_E("Unexpected error in CFTPProxyServer::Load(): empty name of proxy server is not allowed!");
        return FALSE; // name is mandatory
    }
    registry->GetValue(regKey, CONFIG_FTPPRXTYPE, REG_DWORD, &proxyType, sizeof(DWORD));
    if (proxyType < 0 || proxyType > fpstOwnScript)
    {
        TRACE_E("Unexpected error in CFTPProxyServer::Load(): unknown type of proxy server!");
        return FALSE; // unknown proxy server type
    }
    registry->GetValue(regKey, CONFIG_FTPPRXHOST, REG_SZ, proxyHost, HOST_MAX_SIZE);
    registry->GetValue(regKey, CONFIG_FTPPRXPORT, REG_DWORD, &proxyPort, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPPRXUSER, REG_SZ, proxyUser, USER_MAX_SIZE);

    LoadPassword(regKey, registry, CONFIG_FTPPRXPASSWD_OLD, CONFIG_FTPPRXPASSWD_SCRAMBLED, CONFIG_FTPPRXPASSWD_ENCRYPTED,
                 &proxyEncryptedPassword, &proxyEncryptedPasswordSize);
    if (proxyEncryptedPassword != NULL && proxyEncryptedPasswordSize > 0)
    {
        saveProxyPassword = TRUE;

        // detect and, if needed, clear the "unnecessary" (due to storing "save password" TRUE) empty scrambled password
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        if (!passwordManager->IsPasswordEncrypted(proxyEncryptedPassword, proxyEncryptedPasswordSize))
        {
            char* plainPassword;
            if (passwordManager->DecryptPassword(proxyEncryptedPassword, proxyEncryptedPasswordSize, &plainPassword))
            {
                if (plainPassword[0] == 0)
                {
                    memset(proxyEncryptedPassword, 0, proxyEncryptedPasswordSize);
                    SalamanderGeneral->Free(proxyEncryptedPassword);
                    proxyEncryptedPassword = NULL;
                    proxyEncryptedPasswordSize = 0;
                }
                memset(plainPassword, 0, lstrlen(plainPassword));
                SalamanderGeneral->Free(plainPassword);
            }
        }
    }

    char proxyScriptReg[PROXYSCRIPT_MAX_SIZE + 300];
    if (registry->GetValue(regKey, CONFIG_FTPPRXSCRIPT, REG_SZ, proxyScriptReg, PROXYSCRIPT_MAX_SIZE + 300))
    {
        if (!ConvertStringRegToTxt(proxyScript, PROXYSCRIPT_MAX_SIZE, proxyScriptReg))
            TRACE_E("Unexpected error in CFTPProxyServer::Load(): small buffer for proxy script!");
    }
    const char* errPos = NULL;
    char errDescr[300];
    BOOL proxyHostNeeded = proxyType != fpstFTP_transparent;
    if (proxyType == fpstOwnScript &&
        !ProcessProxyScript(proxyScript, &errPos, -1, NULL, NULL, NULL, NULL, NULL, errDescr, &proxyHostNeeded))
    {
        TRACE_E("Unexpected error in CFTPProxyServer::Load(): syntax error in proxy script! err-pos: " << (errPos - proxyScript) << ", error: " << errDescr);
        return FALSE;
    }
    if (proxyHostNeeded && proxyHost[0] == 0)
    {
        TRACE_E("Unexpected error in CFTPProxyServer::Load(): ProxyHost is empty but it may not be!");
        return FALSE;
    }

    BOOL ret = Set(proxyUID,
                   proxyName,
                   (CFTPProxyServerType)proxyType,
                   GetStrOrNULL(proxyHost),
                   proxyPort,
                   GetStrOrNULL(proxyUser),
                   proxyEncryptedPassword, proxyEncryptedPasswordSize,
                   saveProxyPassword,
                   GetStrOrNULL(proxyScript));
    if (proxyEncryptedPassword != NULL)
    {
        memset(proxyEncryptedPassword, 0, proxyEncryptedPasswordSize);
        SalamanderGeneral->Free(proxyEncryptedPassword);
    }
    return ret;
}

void CFTPProxyServer::Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    registry->SetValue(regKey, CONFIG_FTPPRXUID, REG_DWORD, &ProxyUID, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_FTPPRXNAME, REG_SZ, HandleNULLStr(ProxyName), -1);
    if (ProxyType != fpstFTP_USER_user_host_colon_port)
    {
        DWORD dw = ProxyType;
        registry->SetValue(regKey, CONFIG_FTPPRXTYPE, REG_DWORD, &dw, sizeof(DWORD));
    }
    if (IsNotEmptyStr(ProxyHost))
        registry->SetValue(regKey, CONFIG_FTPPRXHOST, REG_SZ, ProxyHost, -1);
    if (ProxyPort != 21)
        registry->SetValue(regKey, CONFIG_FTPPRXPORT, REG_DWORD, &ProxyPort, sizeof(DWORD));
    if (IsNotEmptyStr(ProxyUser))
        registry->SetValue(regKey, CONFIG_FTPPRXUSER, REG_SZ, ProxyUser, -1);
    if (SaveProxyPassword) // "save password" is not stored (if a password is saved, it means "save password" is TRUE - so we must also store an empty password)
    {
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        if (ProxyEncryptedPassword != NULL && ProxyEncryptedPasswordSize > 0)
        {
            BOOL encrypted = passwordManager->IsPasswordEncrypted(ProxyEncryptedPassword, ProxyEncryptedPasswordSize);
            registry->SetValue(regKey, encrypted ? CONFIG_FTPPRXPASSWD_ENCRYPTED : CONFIG_FTPPRXPASSWD_SCRAMBLED,
                               REG_BINARY, ProxyEncryptedPassword, ProxyEncryptedPasswordSize);
        }
        else // store an artificially created empty password only to keep "save password" TRUE
        {
            BYTE* scrambledPassword;
            int scrambledPasswordSize;
            if (passwordManager->EncryptPassword("", &scrambledPassword, &scrambledPasswordSize, FALSE))
            {
                registry->SetValue(regKey, CONFIG_FTPPRXPASSWD_SCRAMBLED, REG_BINARY, scrambledPassword, scrambledPasswordSize);
                // free the buffer allocated in EncryptPassword()
                SalamanderGeneral->Free(scrambledPassword);
            }
        }
    }
    if (IsNotEmptyStr(ProxyScript))
    {
        char proxyScriptReg[PROXYSCRIPT_MAX_SIZE + 300];
        ConvertStringTxtToReg(proxyScriptReg, PROXYSCRIPT_MAX_SIZE + 300, ProxyScript);
        registry->SetValue(regKey, CONFIG_FTPPRXSCRIPT, REG_SZ, proxyScriptReg, -1);
    }
}

//
// ****************************************************************************
// CFTPProxyServerList
//

CFTPProxyServerList::CFTPProxyServerList() : TIndirectArray<CFTPProxyServer>(5, 10)
{
    HANDLES(InitializeCriticalSection(&ProxyServerListCS));
    NextFreeProxyUID = 1;
}

CFTPProxyServerList::~CFTPProxyServerList()
{
    HANDLES(DeleteCriticalSection(&ProxyServerListCS));
}

BOOL CFTPProxyServerList::CopyMembersToList(CFTPProxyServerList& dstList)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    dstList.DestroyMembers();
    dstList.NextFreeProxyUID = NextFreeProxyUID;
    BOOL ret = TRUE;
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPProxyServer* n = At(i)->MakeCopy();
        if (n != NULL)
        {
            dstList.Add(n);
            if (!dstList.IsGood())
            {
                dstList.ResetState();
                delete n;
                ret = FALSE;
                break;
            }
        }
        else
        {
            ret = FALSE; // error
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
    return ret;
}

void CFTPProxyServerList::Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    HKEY actKey;
    if (registry->OpenKey(regKey, CONFIG_FTPPROXYLIST, actKey))
    {
        HKEY subKey;
        char buf[30];
        int i = 0;
        DestroyMembers();
        NextFreeProxyUID = 1;
        while (registry->OpenKey(actKey, _itoa(++i, buf, 10), subKey))
        {
            CFTPProxyServer* item = new CFTPProxyServer(0);
            if (item == NULL)
            {
                TRACE_E(LOW_MEMORY);
                break;
            }
            if (!item->Load(parent, subKey, registry)) // loading failed, skip this item
            {
                delete item;
            }
            else
            {
                BOOL notUnique = FALSE;
                if (item->ProxyUID < 0)
                {
                    TRACE_E("Unexpected situation in CFTPProxyServerList::Load(): proxy server UID < 0: " << item->ProxyUID);
                    notUnique = TRUE;
                }
                else
                {
                    int x;
                    for (x = 0; x < Count; x++)
                    {
                        CFTPProxyServer* proxy = At(x);
                        if (proxy->ProxyUID == item->ProxyUID ||
                            SalamanderGeneral->StrICmp(proxy->ProxyName, item->ProxyName) == 0)
                        {
                            TRACE_E("Unexpected situation in CFTPProxyServerList::Load(): not unique proxy server - skipping it...");
                            notUnique = TRUE;
                            break;
                        }
                    }
                }
                if (!notUnique)
                    Add(item);
                if (notUnique || !IsGood())
                {
                    if (!notUnique)
                        ResetState();
                    delete item;
                    break;
                }
                else
                { // addition succeeded, ensure NextFreeProxyUID is unique with respect to the newly added proxy server
                    if (NextFreeProxyUID <= item->ProxyUID)
                        NextFreeProxyUID = item->ProxyUID + 1;
                }
            }
            registry->CloseKey(subKey);
        }
        registry->CloseKey(actKey);
    }
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
}

void CFTPProxyServerList::Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    HKEY actKey;
    if (registry->CreateKey(regKey, CONFIG_FTPPROXYLIST, actKey))
    {
        registry->ClearKey(actKey);
        HKEY subKey;
        char buf[30];
        int i;
        for (i = 0; i < Count; i++)
        {
            if (registry->CreateKey(actKey, _itoa(i + 1, buf, 10), subKey))
            {
                At(i)->Save(parent, subKey, registry);
                registry->CloseKey(subKey);
            }
            else
                break;
        }
        registry->CloseKey(actKey);
    }
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
}

void CFTPProxyServerList::InitCombo(HWND combo, int focusProxyUID, BOOL addDefault)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_PROXYSERVER_NOTUSED));
    if (addDefault)
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_PROXYSERVER_DEFAULT));
    int focusIndex = addDefault ? 1 : 0;
    if (focusProxyUID == -1 && addDefault)
        focusIndex = 0;
    int i;
    for (i = 0; i < Count; i++) // fill the combo-box list
    {
        CFTPProxyServer* proxy = At(i);
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)proxy->ProxyName);
        if (focusProxyUID >= 0 && proxy->ProxyUID == focusProxyUID)
            focusIndex = i + (addDefault ? 2 : 1);
    }
    SendMessage(combo, CB_SETCURSEL, focusIndex, 0);
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
}

void CFTPProxyServerList::GetProxyUIDFromCombo(HWND combo, int& focusedProxyUID, BOOL addDefault)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    focusedProxyUID = addDefault ? -2 : -1;
    if (sel != CB_ERR)
    {
        int index = sel - (addDefault ? 2 : 1);
        if (index >= 0 && index < Count)
            focusedProxyUID = At(index)->ProxyUID;
        else
        {
            if (index == -2 && addDefault)
                focusedProxyUID = -1;
        }
    }
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
}

BOOL CFTPProxyServerList::IsValidUID(int proxyServerUID)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    int i;
    for (i = 0; i < Count; i++) // fill the combo-box list
    {
        if (At(i)->ProxyUID == proxyServerUID)
            break;
    }
    BOOL found = i < Count;
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
    return found;
}

BOOL CFTPProxyServerList::IsProxyNameOK(CFTPProxyServer* proxyServer, const char* proxyName)
{
    BOOL ok = FALSE;
    if (proxyName[0] != 0)
    {
        HANDLES(EnterCriticalSection(&ProxyServerListCS));
        int i;
        for (i = 0; i < Count; i++) // check whether the name has already been used elsewhere
        {
            CFTPProxyServer* proxy = At(i);
            if (proxy != proxyServer && SalamanderGeneral->StrICmp(proxy->ProxyName, proxyName) == 0)
                break;
        }
        ok = (i == Count);
        HANDLES(LeaveCriticalSection(&ProxyServerListCS));
    }
    return ok;
}

BOOL CFTPProxyServerList::SetProxyServer(CFTPProxyServer* proxyServer,
                                         int proxyUID,
                                         const char* proxyName,
                                         CFTPProxyServerType proxyType,
                                         const char* proxyHost,
                                         int proxyPort,
                                         const char* proxyUser,
                                         const BYTE* proxyEncryptedPassword,
                                         int proxyEncryptedPasswordSize,
                                         int saveProxyPassword,
                                         const char* proxyScript)
{
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    BOOL ret = proxyServer->Set(proxyUID,
                                proxyName,
                                proxyType,
                                proxyHost,
                                proxyPort,
                                proxyUser,
                                proxyEncryptedPassword,
                                proxyEncryptedPasswordSize,
                                saveProxyPassword,
                                proxyScript);
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
    return ret;
}

CFTPProxyServer*
CFTPProxyServerList::MakeCopyOfProxyServer(int proxyServerUID, BOOL* lowMem)
{
    if (lowMem != NULL)
        *lowMem = FALSE;
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    CFTPProxyServer* ret = NULL;
    int i;
    for (i = 0; i < Count; i++)
    {
        if (At(i)->ProxyUID == proxyServerUID)
        {
            ret = At(i)->MakeCopy();
            if (ret == NULL && lowMem != NULL)
                *lowMem = TRUE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
    return ret;
}

void CFTPProxyServerList::AddProxyServer(HWND parent, HWND combo)
{
    CFTPProxyServer* n = new CFTPProxyServer(0);
    if (n != NULL)
    {
        // the value change happens in CProxyServerDlg::Transfer(), which calls SetProxyServer() on this object
        if (CProxyServerDlg(parent, this, n, FALSE).Execute() == IDOK)
        {
            HANDLES(EnterCriticalSection(&ProxyServerListCS));
            Add(n);
            if (IsGood())
            {
                n->ProxyUID = NextFreeProxyUID++; // initialization of ProxyUID
                // add to the combo box and focus the added item
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)n->ProxyName);
                int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
                if (count != CB_ERR && count > 0)
                    PostMessage(combo, CB_SETCURSEL, count - 1, 0);
            }
            else
            {
                ResetState();
                delete n;
            }
            HANDLES(LeaveCriticalSection(&ProxyServerListCS));
        }
        else
            delete n;
    }
    else
        TRACE_E(LOW_MEMORY);
}

void CFTPProxyServerList::EditProxyServer(HWND parent, HWND combo, BOOL addDefault)
{
    int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR)
    {
        int index = sel - (addDefault ? 2 : 1);
        if (index >= 0 && index < Count) // reading values in the main thread does not need a critical section
        {
            CFTPProxyServer* e = At(index);
            // the value change happens in CProxyServerDlg::Transfer(), which calls SetProxyServer() on this object
            BOOL edited = (CProxyServerDlg(parent, this, e, TRUE).Execute() == IDOK);
            if (edited)
            { // just refresh the combo box
                if (SendMessage(combo, CB_INSERTSTRING, sel, (LPARAM)e->ProxyName) == sel)
                {
                    SendMessage(combo, CB_DELETESTRING, sel + 1, 0);
                    SendMessage(combo, CB_SETCURSEL, sel, 0);
                }
            }
        }
    }
}

void CFTPProxyServerList::DeleteProxyServer(HWND parent, HWND combo, BOOL addDefault)
{
    int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR)
    {
        int index = sel - (addDefault ? 2 : 1);
        if (index >= 0 && index < Count) // reading values in the main thread does not need a critical section
        {
            char buf[500];
            sprintf(buf, LoadStr(IDS_WANTDELPRXSRV), At(index)->ProxyName);
            BOOL del = (SalamanderGeneral->SalMessageBox(parent, buf,
                                                         LoadStr(IDS_FTPPLUGINTITLE),
                                                         MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                             MB_ICONQUESTION) == IDYES);
            if (del && index >= 0 && index < Count)
            {
                SendMessage(combo, CB_DELETESTRING, sel, 0);
                int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
                if (count != CB_ERR)
                {
                    if (sel < count)
                        SendMessage(combo, CB_SETCURSEL, sel, 0);
                    else
                        SendMessage(combo, CB_SETCURSEL, count - 1, 0);
                }
                HANDLES(EnterCriticalSection(&ProxyServerListCS));
                Delete(index);
                HANDLES(LeaveCriticalSection(&ProxyServerListCS));
            }
        }
    }
}

void CFTPProxyServerList::MoveUpProxyServer(HWND combo, BOOL addDefault)
{
    int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR)
    {
        int index = sel - (addDefault ? 2 : 1);
        if (index > 0 && index < Count) // reading values in the main thread does not need a critical section
        {
            if (SendMessage(combo, CB_INSERTSTRING, sel - 1, (LPARAM)At(index)->ProxyName) == sel - 1)
            {
                SendMessage(combo, CB_DELETESTRING, sel + 1, 0);
                SendMessage(combo, CB_SETCURSEL, sel - 1, 0);
                HANDLES(EnterCriticalSection(&ProxyServerListCS));
                CFTPProxyServer* swap = At(index - 1);
                At(index - 1) = At(index);
                At(index) = swap;
                HANDLES(LeaveCriticalSection(&ProxyServerListCS));
            }
        }
    }
}

void CFTPProxyServerList::MoveDownProxyServer(HWND combo, BOOL addDefault)
{
    int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
    if (sel != CB_ERR && count != CB_ERR)
    {
        int index = sel - (addDefault ? 2 : 1);
        if (index >= 0 && index + 1 < Count) // reading values in the main thread does not need a critical section
        {
            if (SendMessage(combo, CB_INSERTSTRING, sel, (LPARAM)At(index + 1)->ProxyName) == sel)
            {
                SendMessage(combo, CB_DELETESTRING, sel + 2, 0);
                SendMessage(combo, CB_SETCURSEL, sel + 1, 0);
                HANDLES(EnterCriticalSection(&ProxyServerListCS));
                CFTPProxyServer* swap = At(index);
                At(index) = At(index + 1);
                At(index + 1) = swap;
                HANDLES(LeaveCriticalSection(&ProxyServerListCS));
            }
        }
    }
}

BOOL CFTPProxyServerList::GetProxyName(char* buf, int bufSize, int proxyServerUID)
{
    if (proxyServerUID == -1)
    {
        lstrcpyn(buf, LoadStr(IDS_ADVSTRPROXYNOTUSED), bufSize);
        return TRUE;
    }
    int i;
    for (i = 0; i < Count; i++) // reading values in the main thread does not need a critical section
    {
        CFTPProxyServer* s = At(i);
        if (s->ProxyUID == proxyServerUID)
        {
            lstrcpyn(buf, HandleNULLStr(s->ProxyName), bufSize);
            return TRUE;
        }
    }
    return FALSE;
}

CFTPProxyServerType
CFTPProxyServerList::GetProxyType(int proxyServerUID)
{
    int i;
    for (i = 0; i < Count; i++) // reading values in the main thread does not need a critical section
    {
        CFTPProxyServer* s = At(i);
        if (s->ProxyUID == proxyServerUID)
            return s->ProxyType;
    }
    return fpstNotUsed;
}

BOOL CFTPProxyServerList::ContainsUnsecuredPassword()
{
    // NOTE, the same method exists for CFTPServerList
    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
    int i;
    for (i = 0; i < Count; i++) // reading values in the main thread does not need a critical section
    {
        CFTPProxyServer* s = At(i);
        if (s->SaveProxyPassword && s->ProxyEncryptedPassword != NULL)
        {
            if (!passwordManager->IsPasswordEncrypted(s->ProxyEncryptedPassword, s->ProxyEncryptedPasswordSize))
                return TRUE;
        }
    }
    return FALSE;
}

BOOL CFTPProxyServerList::EncryptPasswords(HWND hParent, BOOL encrypt)
{
    // NOTE, the same method exists for CFTPServerList
    HANDLES(EnterCriticalSection(&ProxyServerListCS));
    BOOL ret = TRUE;
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPProxyServer* s = At(i);
        ret &= EncryptPasswordAux(&s->ProxyEncryptedPassword, &s->ProxyEncryptedPasswordSize, s->SaveProxyPassword, encrypt);
    }
    HANDLES(LeaveCriticalSection(&ProxyServerListCS));
    return ret;
}

BOOL CFTPProxyServerList::EnsurePasswordCanBeDecrypted(HWND hParent, int proxyServerUID)
{
    if (proxyServerUID == -1 /* not used */)
        return TRUE; // proxy server is not used, nothing to verify

    CFTPProxyServer* s = NULL;
    int i;
    for (i = 0; i < Count; i++) // reading values in the main thread does not need a critical section
    {
        if (At(i)->ProxyUID == proxyServerUID)
        {
            s = At(i);
            break;
        }
    }

    if (s == NULL)
    {
        TRACE_E("CFTPProxyServerList::EnsurePasswordCanBeDecrypted(): cannot find proxy server with proxyServerUID " << proxyServerUID);
        return FALSE; // invalid proxy server
    }

    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
    if (s->ProxyEncryptedPassword != NULL &&
        passwordManager->IsPasswordEncrypted(s->ProxyEncryptedPassword, s->ProxyEncryptedPasswordSize))
    {
        // verify whether entering the master password is necessary to decrypt the password
        if (passwordManager->IsUsingMasterPassword() && !passwordManager->IsMasterPasswordSet())
        {
            if (!passwordManager->AskForMasterPassword(hParent))
                return FALSE; // the user did not enter the correct master password
        }
        // verify that this is the correct master password for this password
        if (!passwordManager->DecryptPassword(s->ProxyEncryptedPassword, s->ProxyEncryptedPasswordSize, NULL))
        {
            int ret = SalamanderGeneral->SalMessageBox(hParent, LoadStr(IDS_CANNOT_DECRYPT_PASSWORD_DELETE),
                                                       LoadStr(IDS_FTPERRORTITLE), MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_DEFBUTTON2 | MB_ICONEXCLAMATION);
            if (ret == IDNO)
                return FALSE; // failed to decrypt the password

            // the user wanted to delete the password
            HANDLES(EnterCriticalSection(&ProxyServerListCS));
            UpdateEncryptedPassword(&s->ProxyEncryptedPassword, &s->ProxyEncryptedPasswordSize, NULL, 0);
            // clear the save password checkbox
            s->SaveProxyPassword = FALSE;
            HANDLES(LeaveCriticalSection(&ProxyServerListCS));
        }
    }
    return TRUE;
}
