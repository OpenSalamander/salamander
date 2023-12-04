// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CSourceFile
//

CSourceFile::CSourceFile(const CFileData* fileData,
                         const char* path, int pathLen, BOOL isDir)
{
    CALL_STACK_MESSAGE_NONE
    NameLen = pathLen + fileData->NameLen;
    if (path[pathLen - 1] != '\\')
    {
        FullName = (char*)malloc(++NameLen + 1);
        memcpy(FullName, path, pathLen);
        FullName[pathLen++] = '\\';
    }
    else
    {
        FullName = (char*)malloc(NameLen + 1);
        memcpy(FullName, path, pathLen);
    }
    Name = FullName + pathLen;
    memcpy(Name, fileData->Name, fileData->NameLen + 1);
    Ext = Name + (isDir ? fileData->NameLen : fileData->Ext - fileData->Name);
    Size = fileData->Size;
    Attr = fileData->Attr;
    FileTimeToLocalFileTime(&fileData->LastWrite, &LastWrite);
    IsDir = isDir ? 1 : 0;
    State = 0;
}

CSourceFile::CSourceFile(CSourceFile* orig)
{
    CALL_STACK_MESSAGE_NONE
    FullName = SG->DupStr(orig->FullName);
    Name = FullName + (orig->Name - orig->FullName);
    Ext = FullName + (orig->Ext - orig->FullName);
    Size = orig->Size;
    Attr = orig->Attr;
    LastWrite = orig->LastWrite;
    NameLen = orig->NameLen;
    IsDir = orig->IsDir;
    State = 0;
}

CSourceFile::CSourceFile(CSourceFile* orig, const char* newName)
{
    CALL_STACK_MESSAGE_NONE
    FullName = Name = SG->DupStr(newName);
    Ext = NULL;
    char* iterator = FullName;
    while (*iterator != 0)
    {
        if (*iterator == '\\')
        {
            Name = iterator + 1;
            Ext = NULL;
        }
        if (*iterator == '.' /*&& iterator > Name*/) // ".cvspass" ve Windows je pripona
            Ext = iterator + 1;
        iterator++;
    }
    if (orig->IsDir || Ext == NULL)
        Ext = iterator; // adresare nemaji priponu + pokud soubor nema priponu
    NameLen = iterator - FullName;
    Size = orig->Size;
    Attr = orig->Attr;
    LastWrite = orig->LastWrite;
    IsDir = orig->IsDir;
    State = 0;
}

CSourceFile::CSourceFile(WIN32_FIND_DATA& fd, const char* path, int pathLen)
{
    CALL_STACK_MESSAGE_NONE
    NameLen = pathLen + strlen(fd.cFileName) + 1;
    FullName = (char*)malloc(NameLen + 1);
    memcpy(FullName, path, pathLen);
    FullName[pathLen++] = '\\';
    strcpy(FullName + pathLen, fd.cFileName);
    Name = FullName + pathLen;
    Ext = FullName + NameLen;
    while (--Ext >= Name && *Ext != '.')
        ;
    if (Ext < Name)
        Ext = FullName + NameLen; // ".cvspass" ve Windows je pripona
    else
        Ext++;
    Size = CQuadWord(fd.nFileSizeLow, fd.nFileSizeHigh);
    Attr = fd.dwFileAttributes;
    FileTimeToLocalFileTime(&fd.ftLastWriteTime, &LastWrite);
    IsDir = FALSE;
    State = 0;
}

CSourceFile::~CSourceFile()
{
    CALL_STACK_MESSAGE_NONE
    if (FullName)
        free(FullName);
}

CSourceFile*
CSourceFile::SetName(const char* name)
{
    CALL_STACK_MESSAGE_NONE
    if (FullName)
        free(FullName);
    FullName = Name = SG->DupStr(name);
    Ext = NULL;
    char* iterator = FullName;
    while (*iterator != 0)
    {
        if (*iterator == '\\')
        {
            Name = iterator + 1;
            Ext = NULL;
        }
        if (*iterator == '.' /*&& iterator > Name*/) // ".cvspass" ve Windows je pripona
            Ext = iterator + 1;
        iterator++;
    }
    if (IsDir || Ext == NULL)
        Ext = iterator; // adresare nemaji priponu + pokud soubor nema priponu
    NameLen = iterator - FullName;
    return this;
}

// ****************************************************************************
//
// CRenamerOptions
//

const char* CONFIG_NEWNAME = "NewName";
const char* CONFIG_SEARCHFOR = "SearchFor";
const char* CONFIG_REPLACEWITH = "ReplaceWith";
const char* CONFIG_CASESENSITIVE = "CaseSensitive";
const char* CONFIG_WHOLEWORDS = "WholeWords";
const char* CONFIG_GLOBAL = "Global";
const char* CONFIG_REGEXP = "RegExp";
const char* CONFIG_EXCLUDEEXT = "ExcludeExt";
const char* CONFIG_FILECASE = "FileCase";
const char* CONFIG_EXTCASE = "ExtCase";
const char* CONFIG_INCLUDEPATH = "IncludePath";
const char* CONFIG_SPEC = "Spec";

void CRenamerOptions::Reset(BOOL soft)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(NewName, "$(OriginalName)");
    SearchFor[0] = 0;
    ReplaceWith[0] = 0;
    CaseSensitive = TRUE;
    WholeWords = FALSE;
    Global = FALSE;
    RegExp = FALSE;
    ExcludeExt = FALSE;
    FileCase = ccDontChange;
    ExtCase = ccDontChange;
    IncludePath = FALSE;
    if (!soft)
        Spec = rsFileName;
}

BOOL CRenamerOptions::Load(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CRenamerOptions::Load(, )");
    Reset(FALSE);
    registry->GetValue(regKey, CONFIG_NEWNAME, REG_SZ, NewName, 2 * MAX_PATH);
    registry->GetValue(regKey, CONFIG_SEARCHFOR, REG_SZ, SearchFor, 2 * MAX_PATH);
    registry->GetValue(regKey, CONFIG_REPLACEWITH, REG_SZ, ReplaceWith, MAX_PATH);
    registry->GetValue(regKey, CONFIG_CASESENSITIVE, REG_DWORD, &CaseSensitive, sizeof(BOOL));
    registry->GetValue(regKey, CONFIG_WHOLEWORDS, REG_DWORD, &WholeWords, sizeof(BOOL));
    registry->GetValue(regKey, CONFIG_GLOBAL, REG_DWORD, &Global, sizeof(BOOL));
    registry->GetValue(regKey, CONFIG_REGEXP, REG_DWORD, &RegExp, sizeof(BOOL));
    registry->GetValue(regKey, CONFIG_EXCLUDEEXT, REG_DWORD, &ExcludeExt, sizeof(BOOL));
    registry->GetValue(regKey, CONFIG_FILECASE, REG_DWORD, &FileCase, sizeof(int));
    registry->GetValue(regKey, CONFIG_EXTCASE, REG_DWORD, &ExtCase, sizeof(int));
    registry->GetValue(regKey, CONFIG_INCLUDEPATH, REG_DWORD, &IncludePath, sizeof(BOOL));
    registry->GetValue(regKey, CONFIG_SPEC, REG_DWORD, &Spec, sizeof(int));
    return TRUE;
}

BOOL CRenamerOptions::Save(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CRenamerOptions::Save(, )");
    registry->SetValue(regKey, CONFIG_NEWNAME, REG_SZ, NewName, -1);
    registry->SetValue(regKey, CONFIG_SEARCHFOR, REG_SZ, SearchFor, -1);
    registry->SetValue(regKey, CONFIG_REPLACEWITH, REG_SZ, ReplaceWith, -1);
    registry->SetValue(regKey, CONFIG_CASESENSITIVE, REG_DWORD, &CaseSensitive, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_WHOLEWORDS, REG_DWORD, &WholeWords, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_GLOBAL, REG_DWORD, &Global, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_REGEXP, REG_DWORD, &RegExp, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_EXCLUDEEXT, REG_DWORD, &ExcludeExt, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_FILECASE, REG_DWORD, &FileCase, sizeof(int));
    registry->SetValue(regKey, CONFIG_EXTCASE, REG_DWORD, &ExtCase, sizeof(int));
    registry->SetValue(regKey, CONFIG_INCLUDEPATH, REG_DWORD, &IncludePath, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_SPEC, REG_DWORD, &Spec, sizeof(int));
    return TRUE;
}

// ****************************************************************************
//
// CRenamer
//

CRenamer::CRenamer(char (&root)[MAX_PATH], int& rootLen)
    : Root(root), RootLen(rootLen)
{
    CALL_STACK_MESSAGE2("CRenamer::CRenamer(, %d)", rootLen);
    BMSearch = SG->AllocSalamanderBMSearchData();
    RegExp = CreateRegExp();
}

CRenamer::~CRenamer()
{
    CALL_STACK_MESSAGE1("CRenamer::~CRenamer()");
    SG->FreeSalamanderBMSearchData(BMSearch);
    ReleaseRegExp(RegExp);
}

BOOL CRenamer::SetOptions(CRenamerOptions* options)
{
    CALL_STACK_MESSAGE1("CRenamerOptions( )");
    Spec = options->Spec;
    FileCase = options->FileCase;
    ExtCase = options->ExtCase;
    IncludePath = options->IncludePath;

    if (!NewName.Compile(options->NewName, Error, ErrorPos1, ErrorPos2, NewNameVariables))
    {
        ErrorType = retNewName;
        return FALSE;
    }

    if (options->SearchFor[0] != '\0')
    {
        Substitute = TRUE;
        strcpy(ReplaceWith, options->ReplaceWith);
        ReplaceWithLen = (int)strlen(ReplaceWith);
        WholeWords = options->WholeWords;
        Global = options->Global;
        ExcludeExt = options->ExcludeExt;

        if ((UseRegExp = options->RegExp) != 0)
        {
            unsigned opts = RE_SINGLELINE;
            if (!options->CaseSensitive)
                opts |= RE_CASELES;

            if (!RegExp->RegComp(options->SearchFor, opts))
            {
                Error = GetRegExpErrorID(RegExp->GetState());
                ErrorPos1 = 0;
                ErrorPos2 = 0;
                ErrorType = retRegExp;
                return FALSE;
            }

            if (!ValidetaReplacePattern())
                return FALSE;
        }
        else
        {
            WORD flags = SASF_FORWARD;
            if (options->CaseSensitive)
                flags |= SASF_CASESENSITIVE;

            BMSearch->Set(options->SearchFor, flags);
            if (!BMSearch->IsGood())
            {
                Error = IDS_LOWMEM;
                ErrorPos1 = 0;
                ErrorPos2 = 0;
                ErrorType = retBMSearch;
                return FALSE;
            }
        }
    }
    else
        Substitute = FALSE;

    Error = 0;
    return TRUE;
}

int CRenamer::Rename(CSourceFile* file, int counter, char* newName, char** newPart)
{
    CALL_STACK_MESSAGE_NONE
    if (!IsGood())
        return -1;

    int pathLen = 0;
    if (newPart)
    {
        switch (Spec)
        {
        case rsFileName:
        {
            pathLen = (int)(file->Name - file->FullName);
            memcpy(newName, file->FullName, pathLen);
            break;
        }
        case rsRelativePath:
        {
            pathLen = RootLen;
            memcpy(newName, Root, pathLen);
            if (newName[pathLen - 1] != '\\')
                newName[pathLen++] = '\\';
            break;
        }
        case rsFullPath:
            break;
        }
        newName += pathLen;
        *newPart = newName;
    }

    // parametry pro expanzi var-stringu New Name
    CExecuteNewNameParam param;
    param.Spec = Spec;
    param.File = file;
    param.Counter = counter;
    param.RootLen = RootLen;

    int l;
    if (Substitute)
    {
        char tmp[MAX_PATH];

        // expandujeme New Name do pomocneho bufferu
        l = NewName.Execute(tmp, MAX_PATH, &param);
        if (l < 0)
            return -1;
        // l is strlen(tmp)
        if (ExcludeExt && !file->IsDir) // pripony dohledavame jen u souboru
        {
            int i = l, namel = l;
            while (--i >= 0 && tmp[i] != '\\')
            {
                if (tmp[i] == '.') // ".cvspass" ve Windows je pripona
                {
                    namel = i;
                    break;
                }
            }

            // provedeme pozadovanou substituci ve jmene
            int substl = UseRegExp ? RESubst(tmp, namel, newName, MAX_PATH - pathLen) : BMSubst(tmp, namel, newName, MAX_PATH - pathLen);

            // dokopirujeme extension
            if (substl < 0 || substl + (l - namel) >= MAX_PATH - pathLen)
                return -1;
            memcpy(newName + substl, tmp + namel, l - namel + 1);
            // calculate new name length : substed name len + appended ext len - '\0'
            l = substl + l - namel;
        }
        else
        {
            // provedeme pozadovanou substituci
            l = UseRegExp ? RESubst(tmp, l, newName, MAX_PATH - pathLen) : BMSubst(tmp, l, newName, MAX_PATH - pathLen);
        }
    }
    else
    {
        // expandujeme New Name
        l = NewName.Execute(newName, MAX_PATH - pathLen, &param);
    }

    if (l < 0)
        return -1;

    if (FileCase != ccDontChange || ExtCase != ccDontChange)
    {
        char* filePart = newName + l - 1;
        char* ext = NULL;
        while (filePart >= newName && *filePart != '\\')
        {
            if (ext == NULL && *filePart == '.')
                ext = filePart; // ".cvspass" ve Windows je pripona
            filePart--;
        }
        filePart++;
        if (file->IsDir || ext == NULL)
            ext = newName + l; // pripony dohledavame jen u souboru

        if (FileCase != ccDontChange)
        {
            char* s = IncludePath ? newName : filePart;
            ChangeCase(FileCase, s, s, s, ext);
        }
        if (ExtCase != ccDontChange)
            ChangeCase(ExtCase, ext, ext, ext, newName + l);
    }

    return l;
}

int CRenamer::BMSearchForward(const char* string, int length, int offset)
{
    CALL_STACK_MESSAGE_NONE
    while (offset < length)
    {
        int ret = BMSearch->SearchForward(string, length, offset);
        if (ret == -1)
            return -1;

        if (WholeWords)
        {
            // test na word break;
            int prev = ret > 0 && IsAlnum(string[ret - 1]) > 0;
            int start = IsAlnum(string[ret]) > 0;
            int end = IsAlnum(string[ret + BMSearch->GetLength() - 1]) > 0;
            int next = ret + BMSearch->GetLength() < length &&
                       IsAlnum(string[ret + BMSearch->GetLength()]) > 0;
            if (prev != start && end != next)
                return ret;
            offset++;
        }
        else
            return ret;
    }
    return -1;
}

BOOL SafeCopy(char* dest, int max, int& pos, const char* source, int count)
{
    CALL_STACK_MESSAGE_NONE
    if (count + pos > max)
        return FALSE;
    memcpy(dest + pos, source, count);
    pos += count;
    return TRUE;
}

BOOL SafeCopy(char* dest, int max, int& pos, const char* source, int count,
              CChangeCase changeCase)
{
    CALL_STACK_MESSAGE_NONE
    if (count + pos > max)
        return FALSE;
    if (changeCase == ccDontChange)
        memcpy(dest + pos, source, count);
    else
        ChangeCase(changeCase, dest + pos, source, source, source + count);
    pos += count;
    return TRUE;
}

int CRenamer::BMSubst(const char* source, int len, char* dest, int max)
{
    CALL_STACK_MESSAGE_NONE
    int start;
    int pos = 0;
    int offset = 0;

    while ((start = BMSearchForward(source, len, offset)) >= 0)
    {
        // nakopirujem cast pred nalezenym vzorem
        if (!SafeCopy(dest, max, pos, source + offset, start - offset))
            return -1;

        // nahradime nalezeny vzor pozadovanou substituci
        if (!SafeCopy(dest, max, pos, ReplaceWith, ReplaceWithLen))
            return -1;

        // posuneme offset
        offset = start + BMSearch->GetLength();
        if (!Global || offset >= len)
            break;
    }

    // nakopirujem cast (vcetne ukoncovaciho NULL) za poslednim nalezeny retezcem
    if (!SafeCopy(dest, max, pos, source + offset, len - offset + 1))
        return -1;

    return pos - 1; // nepocitame ukoncovaci NULL
}

BOOL CRenamer::ValidetaReplacePattern()
{
    CALL_STACK_MESSAGE_NONE
    ErrorType = retReplacePattern;
    const char* replace = ReplaceWith;
    BOOL bs = FALSE;
    char paren;
    const char* numberStart;
    const char* numberEnd;
    while (*replace)
    {
        // '\\' berem jako normalni znak
        // if ((bs = *replace == '\\') || *replace == '$')
        if (*replace == '$')
        {
            replace++;
            paren = 0;
            if (!bs && (*replace == '(' || *replace == '{'))
            {
                paren = *replace == '(' ? ')' : '}';
                replace++;
            }
            if (IsDigit(*replace))
            {
                numberStart = replace;
                int i = 0;
                do
                {
                    i = i * 10 + *replace - '0';
                } while (IsDigit(*++replace));
                numberEnd = replace;
                CChangeCase changeCase = ccDontChange;
                if (paren)
                {
                    if (*replace == ':')
                    {
                        replace++;
                        if (SG->StrNICmp(replace, "lower", sizeof("lower") - 1) == 0)
                            replace += sizeof("lower") - 1;
                        else if (SG->StrNICmp(replace, "upper", sizeof("upper") - 1) == 0)
                            replace += sizeof("upper") - 1;
                        else if (SG->StrNICmp(replace, "mixed", sizeof("mixed") - 1) == 0)
                            replace += sizeof("mixed") - 1;
                        else if (SG->StrNICmp(replace, "stripdia", sizeof("stripdia") - 1) == 0)
                            replace += sizeof("stripdia") - 1;
                        else if (*replace != paren)
                        {
                            // ocekavam zaviraci zavorku nebo definici velikosti
                            Error = IDS_REP_EXPCLOSEPAR1;
                            ErrorPos1 = ErrorPos2 = (int)(replace - ReplaceWith);
                            return FALSE;
                        }
                        if (*replace != paren)
                        {
                            // ocekavam zaviraci zavorku
                            Error = IDS_REP_EXPCLOSEPAR2;
                            ErrorPos1 = ErrorPos2 = (int)(replace - ReplaceWith);
                            return FALSE;
                        }
                    }
                    else
                    {
                        if (*replace != paren)
                        {
                            // ocekavam zaviraci zavorku nebo dvojtecku a definici velikosti
                            Error = IDS_REP_EXPCLOSEPAR3;
                            ErrorPos1 = ErrorPos2 = (int)(replace - ReplaceWith);
                            return FALSE;
                        }
                    }
                    replace++;
                }
                if (i >= RegExp->SubExpCount)
                {
                    // odkaz na nedefinovany subpattern
                    Error = IDS_REP_BADREF;
                    ErrorPos1 = (int)(numberStart - ReplaceWith);
                    ErrorPos2 = (int)(numberEnd - ReplaceWith);
                    return FALSE;
                }
            }
            else
            {
                if (paren)
                {
                    Error = IDS_EXP_EXPECTSUBNUM1;
                    ErrorPos1 = ErrorPos2 = (int)(replace - ReplaceWith);
                    return FALSE;
                }
                if (!bs && *replace != '$')
                {
                    Error = IDS_EXP_EXPECTSUBNUM2;
                    ErrorPos1 = ErrorPos2 = (int)(replace - ReplaceWith);
                    return FALSE;
                }
                if (*replace == 0)
                {
                    Error = IDS_EXP_EXPECTSUBNUM2;
                    ErrorPos1 = ErrorPos2 = (int)(replace - ReplaceWith);
                    return FALSE;
                }
                replace++;
            }
        }
        else
            replace++;
    }
    return TRUE;
}

BOOL CRenamer::SafeSubst(char* dest, int max, int& pos)
{
    CALL_STACK_MESSAGE_NONE
    // retezec ReplaceWith musi byt validovan, zde je optimalizovany kod
    // bez kontroly syntax
    const char* replace = ReplaceWith;
    BOOL paren;
    while (*replace)
    {
        if (pos == max)
            return FALSE;
        // '\\' berem jako normalni znak
        // if (*replace == '\\' || *replace == '$')
        if (*replace == '$')
        {
            paren = FALSE;
            if (*++replace == '(' || *replace == '{')
            {
                paren = TRUE;
                replace++;
            }
            if (IsDigit(*replace))
            {
                int i = 0;
                do
                {
                    i = i * 10 + *replace - '0';
                } while (IsDigit(*++replace));
                CChangeCase changeCase = ccDontChange;
                if (paren)
                {
                    if (*replace++ == ':') // preskocime zaviraci zavorku nebo ':'
                    {
                        switch (*replace)
                        {
                        case 'l':
                            changeCase = ccLower;
                            replace += sizeof("lower") - 1 + 1;
                            break;
                        case 'u':
                            changeCase = ccUpper;
                            replace += sizeof("upper") - 1 + 1;
                            break;
                        case 'm':
                            changeCase = ccMixed;
                            replace += sizeof("mixed") - 1 + 1;
                            break;
                        case 's':
                            changeCase = ccStripDia;
                            replace += sizeof("stripdia") - 1 + 1;
                            break;
                        default: // '}' nebo ')'
                            replace++;
                            break;
                        }
                    }
                }
                if (i < RegExp->SubExpCount &&
                    !SafeCopy(dest, max, pos, RegExp->Startp[i],
                              (int)(RegExp->Endp[i] - RegExp->Startp[i]), changeCase))
                    return FALSE;
            }
            else
                dest[pos++] = *replace++;
        }
        else
            dest[pos++] = *replace++;
    }
    return TRUE;
}

int CRenamer::RESubst(const char* source, int len, char* dest, int max)
{
    CALL_STACK_MESSAGE_NONE
    int pos = 0;
    int offset = 0;
    int skipChar = 0;

    while (RegExp->RegExec((char*)source, len, offset + skipChar))
    {
        // nakopirujem cast pred nalezenym vzorem
        if (!SafeCopy(dest, max, pos, source + offset,
                      (int)(RegExp->Startp[0] - source - offset)))
            return -1;

        // nahradime nalezeny vzor pozadovanou substituci
        if (!SafeSubst(dest, max, pos))
            return -1;

        // posuneme offset
        offset = (int)(RegExp->Endp[0] - source);
        skipChar = RegExp->Endp[0] - RegExp->Startp[0] == 0 ? 1 : 0;
        if (!Global)
            break;
    }

    // nakopirujem cast (vcetne ukoncovaciho NULL) za poslednim nalezeny retezcem
    if (!SafeCopy(dest, max, pos, source + offset, len - offset + 1))
        return -1;

    return pos - 1; // nepocitame ukoncovaci NULL
}

// ****************************************************************************

void ChangeCase(CChangeCase change, char* dst, const char* src,
                const char* start, const char* end)
{
    CALL_STACK_MESSAGE_NONE
    switch (change)
    {
    case ccLower:
        while (start < end)
            *dst++ = LowerCase[*start++];
        return;

    case ccUpper:
        while (start < end)
            *dst++ = UpperCase[*start++];
        return;

    case ccMixed:
    {
        char prev = start > src ? start[-1] : ' ';
        while (start < end)
        {
            //if (IsCType(prev, C1_SPACE | C1_PUNCT))
            if (!IsAlnum(prev))
            {
                prev = *start;
                *dst++ = UpperCase[*start++];
            }
            else
            {
                prev = *start;
                *dst++ = LowerCase[*start++];
            }
        }
        return;
    }

    case ccStripDia:
    {
        int l = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, start, (int)(end - start), NULL, 0);
        if (l >= 0)
        {
            LPWSTR wstr = (LPWSTR)malloc(l * sizeof(WCHAR));
            if (wstr)
            {
                LPWSTR s, d;
                // Convert to composite form
                MultiByteToWideChar(CP_ACP, MB_COMPOSITE, start, (int)(end - start), wstr, l);
                s = d = wstr;
                // Remove combining diacritics marks
                int i;
                for (i = 0; i < l; i++)
                {
                    if (!((*s >= 0x300) && (*s <= 0x36f)))
                        *d++ = *s;
                    s++;
                }
                // Convert back to MBCS, check for orther composite characters
                WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wstr, (int)(d - wstr), dst, (int)(end - start), NULL, NULL);
                free(wstr);
            }
            else
            { // Out of memory???
                memcpy(dst, start, end - start);
            }
        }
        else
        { // Empty string? Or what's wrong??
            memcpy(dst, start, end - start);
        }
        return;
    }
    }
}
