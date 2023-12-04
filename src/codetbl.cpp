// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "codetbl.h"
#include "cfgdlg.h"

CCodeTables CodeTables;

//
//*****************************************************************************
// CCodeTable
//

char* ReadTable(const char* fileName, char* table)
{ // cte soubor 'fileName', prekoduje 'table', v pripade chyby vraci text, jinak NULL
    CALL_STACK_MESSAGE2("ReadTable(%s,)", fileName);
    char* text = NULL;
    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ,
                                        FILE_SHARE_READ, NULL,
                                        OPEN_EXISTING,
                                        FILE_FLAG_SEQUENTIAL_SCAN,
                                        NULL));
    if (hFile != INVALID_HANDLE_VALUE)
    {
        if (GetFileSize(hFile, NULL) == 256)
        {
            char buf[256];
            DWORD read;
            if (ReadFile(hFile, buf, 256, &read, NULL) && read == 256)
            {
                int i;
                for (i = 0; i < 256; i++)
                    table[i] = buf[table[i]];
            }
            else
            {
                text = GetErrorText(GetLastError());
            }
        }
        else
            text = LoadStr(IDS_VIEWERBADFILESIZE);
        HANDLES(CloseHandle(hFile));
    }
    else
    {
        text = GetErrorText(GetLastError());
    }
    return text;
}

void InitAux(HWND hWindow, TIndirectArray<CCodeTablesData>& Data,
             char* fileMem, DWORD fileSize, char* fileName, char* fileNameEnd, char* textBuf,
             char* winCodePage, DWORD* identifier, char* description)
{
    char* txt = fileMem;
    char nameBuf[200];
    char* name;
    char table[256];

    char convertCfgFileName[MAX_PATH]; // pro chybove hlasky
    strcpy(convertCfgFileName, fileName);

    BOOL comment;
    char* text; // != NULL - text chyby
    char* endTxt = txt + fileSize;
    __try
    {
        while (txt < endTxt)
        {
            comment = FALSE;
            text = NULL;
            name = nameBuf;

            if (endTxt - txt >= 18 && StrNICmp(txt, "WINDOWS_CODE_PAGE=", 18) == 0)
            {
                txt += 18;      // preskok "WINDOWS_CODE_PAGE="
                comment = TRUE; // dale nezpracovavat (jako komentar)

                // nacteme jmeno "windows code page"
                while (txt < endTxt && (*txt == ' ' || *txt == '\t'))
                    txt++; // preskok white-spaces
                char* beg = txt;
                while (txt < endTxt && *txt != '\r' && *txt != '\n')
                    txt++;
                int l = (int)min(txt - beg, 100);
                memcpy(winCodePage, beg, l);
                while (l > 0 && (winCodePage[l - 1] == ' ' || winCodePage[l - 1] == '\t'))
                    l--; // orez white-spaces
                winCodePage[l] = 0;
            }
            else if (endTxt - txt >= 29 && StrNICmp(txt, "WINDOWS_CODE_PAGE_IDENTIFIER=", 29) == 0)
            {
                txt += 29;      // preskok "WINDOWS_CODE_PAGE_IDENTIFIER="
                comment = TRUE; // dale nezpracovavat (jako komentar)

                // nacteme identifier
                while (txt < endTxt && (*txt == ' ' || *txt == '\t'))
                    txt++; // preskok white-spaces
                char* beg = txt;
                while (txt < endTxt && *txt != '\r' && *txt != '\n')
                    txt++;
                int l = (int)min(txt - beg, 100);
                char buff[101];
                memcpy(buff, beg, l);
                while (l > 0 && (identifier[l - 1] == ' ' || identifier[l - 1] == '\t'))
                    l--; // orez white-spaces
                buff[l] = 0;
                *identifier = atoi(buff);
            }
            else if (endTxt - txt >= 30 && StrNICmp(txt, "WINDOWS_CODE_PAGE_DESCRIPTION=", 30) == 0)
            {
                txt += 30;      // preskok "WINDOWS_CODE_PAGE_DESCRIPTION="
                comment = TRUE; // dale nezpracovavat (jako komentar)

                // nacteme description
                while (txt < endTxt && (*txt == ' ' || *txt == '\t'))
                    txt++; // preskok white-spaces
                char* beg = txt;
                while (txt < endTxt && *txt != '\r' && *txt != '\n')
                    txt++;
                int l = (int)min(txt - beg, 100);
                memcpy(description, beg, l);
                while (l > 0 && (description[l - 1] == ' ' || description[l - 1] == '\t'))
                    l--; // orez white-spaces
                description[l] = 0;
            }
            else
            {
                if (*txt == '=' && txt + 1 < endTxt && *(txt + 1) == '=') // separator
                {
                    name = NULL;
                }
                else
                {
                    if (*txt == '#') // komentar
                    {
                        comment = TRUE;
                    }
                    else // name=files
                    {
                        char* beg = txt;
                        BOOL white = TRUE;
                        while (txt < endTxt && *txt != '=' && *txt != '\r' && *txt != '\n')
                        {
                            if (*txt != ' ' && *txt != '\t')
                                white = FALSE;
                            txt++;
                        }
                        if (txt < endTxt && *txt == '=' && txt > beg)
                        {
                            int l = (int)min(txt - beg, 199);
                            memcpy(name, beg, l);
                            name[l] = 0;

                            int maxFileNameLen = (int)(MAX_PATH - (fileNameEnd - fileName) - 1);
                            int i;
                            for (i = 0; i < 256; i++)
                                table[i] = i;
                            do
                            {
                                txt++; // preskok '=' nebo '|'
                                beg = txt;
                                while (txt < endTxt && *txt != '\r' && *txt != '\n' && *txt != '|')
                                    txt++;
                                if (beg < txt) // dalsi kodovaci soubor (nebo ANSI->OEM/OEM->ANSI)
                                {
                                    if (*beg == '\\' || beg + 1 < txt && *(beg + 1) == ':') // full-name (UNC, normal)
                                    {
                                        char fullName[MAX_PATH];
                                        l = (int)min(txt - beg, MAX_PATH - 1);
                                        memcpy(fullName, beg, l);
                                        fullName[l] = 0;

                                        text = ReadTable(fullName, table);
                                        if (text != NULL)
                                        {
                                            sprintf(textBuf, LoadStr(IDS_VIEWERERROPENFILE), fullName, text);
                                            text = textBuf;
                                        }
                                    }
                                    else
                                    {
                                        if (*beg == ':') // vnitrni promenna (ANSI->OEM/OEM->ANSI)
                                        {
                                            char buf[256];
                                            l = (int)min((txt - beg) - 1, 255);
                                            memcpy(buf, beg + 1, l);
                                            buf[l] = 0;
                                            if (StrICmp(buf, "ansi->oem") == 0)
                                            {
                                                CharToOemBuff(table, table, 256);
                                            }
                                            else
                                            {
                                                if (StrICmp(buf, "oem->ansi") == 0)
                                                {
                                                    OemToCharBuff(table, table, 256);
                                                }
                                                else
                                                {
                                                    sprintf(textBuf, LoadStr(IDS_VIEWERBADINTCODING), convertCfgFileName);
                                                    text = textBuf;
                                                }
                                            }
                                        }
                                        else // relativni jmeno souboru
                                        {
                                            l = (int)min(txt - beg, maxFileNameLen);
                                            memcpy(fileNameEnd, beg, l);
                                            fileNameEnd[l] = 0;

                                            text = ReadTable(fileName, table);
                                            if (text != NULL)
                                            {
                                                sprintf(textBuf, LoadStr(IDS_VIEWERERROPENFILE), fileName, text);
                                                text = textBuf;
                                            }
                                        }
                                    }
                                }
                            } while (text == NULL && txt < endTxt && *txt != '\r' && *txt != '\n');
                        }
                        else // chybi '=' nebo je na zacatku radky (a neni zdvojene)
                        {
                            if (!white || txt < endTxt && *txt == '=')
                            {
                                sprintf(textBuf, LoadStr(IDS_VIEWERINVALIDLINE), convertCfgFileName);
                                text = textBuf;
                            }
                            else
                                comment = TRUE;
                        }
                    }
                }
            }

            if (text == NULL && !comment) // pridani kodu do tabulky kodu
            {
                CCodeTablesData* code = new CCodeTablesData;
                if (code != NULL)
                {
                    if (name != NULL) // normalni polozka
                    {
                        code->Name = DupStr(name);
                    }
                    else // separator
                    {
                        code->Name = NULL;
                    }
                    if (name != NULL && code->Name == NULL)
                    {
                        delete code;
                        TRACE_E(LOW_MEMORY);
                    }
                    else
                    {
                        memcpy(code->Table, table, 256);
                        Data.Add(code);
                        if (!Data.IsGood())
                        {
                            Data.ResetState();
                            free(code->Name);
                            delete code;
                        }
                    }
                }
            }

            // posune txt za prvni EOL
            while (txt < endTxt && *txt != '\r' && *txt != '\n')
                txt++;
            if (txt < endTxt)
            {
                if (*txt == '\r') // '\r'
                {
                    if (txt + 1 < endTxt && *(txt + 1) == '\n')
                        txt += 2; // '\r\n'
                    else
                        txt++;
                }
                else
                    txt++; // '\n'
            }

            if (text != NULL)
            {
                SalMessageBox(hWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        if (winCodePage[0] == 0)
            TRACE_E("File " << fileName << " does not contain assignment to WINDOWS_CODE_PAGE!");
        if (*identifier == 0xffffffff)
            TRACE_E("File " << fileName << " does not contain assignment to WINDOWS_CODE_PAGE_IDENTIFIER!");
        if (description[0] == 0)
            TRACE_E("File " << fileName << " does not contain assignment to WINDOWS_CODE_PAGE_DESCRIPTION!");
    }
    __except (HandleFileException(GetExceptionInformation(), fileMem, fileSize))
    {
        // chyba v souboru
        char buf[MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_FILEREADERROR), fileName);
        SalMessageBox(hWindow, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
}

CCodeTable::CCodeTable(HWND hWindow, const char* dirName)
    : Data(10, 5)
{
    WinCodePage[0] = 0;
    WinCodePageIdentifier = 0xffffffff; // 0 neni vhodna, protoze GetACP vraci 0 pro UNICODE only kodovani
    WinCodePageDescription[0] = 0;
    strcpy(DirectoryName, dirName);
    State = ctsDefaultValues;

    char fileName[MAX_PATH];
    GetModuleFileName(HInstance, fileName, MAX_PATH);
    char* fileNameEnd = strrchr(fileName, '\\') + 1;
    sprintf(fileNameEnd, "convert\\%s\\", dirName);
    fileNameEnd += strlen(fileNameEnd);
    strcpy(fileNameEnd, "convert.cfg");

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ,
                                        FILE_SHARE_READ, NULL,
                                        OPEN_EXISTING,
                                        FILE_FLAG_SEQUENTIAL_SCAN,
                                        NULL));
    if (hFile != INVALID_HANDLE_VALUE)
    {
        char textBuf[MAX_PATH + 500];
        DWORD err = NO_ERROR;
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize != 0xFFFFFFFF)
        {
            HANDLE mFile = HANDLES(CreateFileMapping(hFile, NULL, PAGE_READONLY,
                                                     0, 0, NULL));
            if (mFile != NULL)
            {
                char* fileMem = (char*)HANDLES(MapViewOfFile(mFile, FILE_MAP_READ,
                                                             0, 0, 0));
                if (fileMem != NULL)
                {
                    InitAux(hWindow, Data, fileMem, fileSize, fileName, fileNameEnd,
                            textBuf, WinCodePage, &WinCodePageIdentifier, WinCodePageDescription);
                    State = ctsSuccessfullyLoaded;

                    HANDLES(UnmapViewOfFile(fileMem));
                }
                else
                    err = GetLastError();
                HANDLES(CloseHandle(mFile));
            }
            else
                err = GetLastError();
        }
        else
            err = GetLastError();
        HANDLES(CloseHandle(hFile));

        if (err != NO_ERROR)
        {
            sprintf(textBuf, LoadStr(IDS_VIEWERERROPENCODES), fileName, GetErrorText(err));
            SalMessageBox(hWindow, textBuf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        }
    }
    else // neni convert\\xxx\\convert.cfg, "natahneme" defaultni konfiguraci
    {
        lstrcpyn(WinCodePage, LoadStr(IDS_VIEWERANSICODEPAGE), 101); // "ANSI" code page
        int i;
        for (i = 0; i < 2; i++)
        {
            CCodeTablesData* code = new CCodeTablesData;
            if (code != NULL)
            {
                int k;
                for (k = 0; k < 256; k++)
                    code->Table[k] = k;
                switch (i)
                {
                case 0:
                {
                    code->Name = DupStr(LoadStr(IDS_VIEWEROEM2ANSICODING));
                    OemToCharBuff(code->Table, code->Table, 256);
                    break;
                }

                case 1:
                {
                    code->Name = DupStr(LoadStr(IDS_VIEWERANSI2OEMCODING));
                    CharToOemBuff(code->Table, code->Table, 256);
                    break;
                }
                }

                if (code->Name == NULL)
                {
                    delete code;
                    TRACE_E(LOW_MEMORY);
                }
                else
                {
                    Data.Add(code);
                    if (!Data.IsGood())
                    {
                        Data.ResetState();
                        free(code->Name);
                        delete code;
                    }
                }
            }
        }
    }
}

CCodeTable::~CCodeTable()
{
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->Name != NULL)
            free(Data[i]->Name);
    }
}

//
//*****************************************************************************
// CCodeTables
//

CCodeTables::CCodeTables()
    : Preloaded(1, 5)
{
    Loaded = FALSE;
    HANDLES(InitializeCriticalSection(&LoadCS));
    HANDLES(InitializeCriticalSection(&PreloadCS));
    Table = NULL;
}

CCodeTables::~CCodeTables()
{
    if (Table != NULL)
        delete Table;
    HANDLES(DeleteCriticalSection(&LoadCS));
    HANDLES(DeleteCriticalSection(&PreloadCS));
}

void CCodeTables::PreloadAllConversions()
{
    HANDLES(EnterCriticalSection(&PreloadCS));
    Preloaded.DestroyMembers();

    char path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    lstrcpy(strrchr(path, '\\') + 1, "convert\\*.*");

    WIN32_FIND_DATA find;
    HANDLE hFind = HANDLES_Q(FindFirstFile(path, &find));
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (find.cFileName[0] != 0 && strcmp(find.cFileName, ".") != 0 && strcmp(find.cFileName, "..") != 0)
                {
                    // pokusime se otevrit vnitrni soubor convert.cfg
                    CCodeTable* table = new CCodeTable(NULL, find.cFileName);
                    if (table == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        break;
                    }
                    if (table->GetState() == ctsSuccessfullyLoaded)
                    {
                        Preloaded.Add(table);
                        if (!Preloaded.IsGood())
                        {
                            Preloaded.ResetState();
                            delete table;
                            break;
                        }
                    }
                    else
                        delete table; // o implicitni tabulku nemame zajem -- zahodime ji
                }
            }
        } while (FindNextFile(hFind, &find));
        HANDLES(FindClose(hFind));
    }
}

void CCodeTables::FreePreloadedConversions()
{
    Preloaded.DestroyMembers();
    HANDLES(LeaveCriticalSection(&PreloadCS));
}

BOOL CCodeTables::EnumPreloadedConversions(int* index, const char** winCodePage,
                                           DWORD* winCodePageIdentifier,
                                           const char** winCodePageDescription,
                                           const char** dirName)
{
    if (*index < 0 || *index >= Preloaded.Count)
        return FALSE;
    CCodeTable* table = Preloaded[*index];
    *winCodePage = table->WinCodePage;
    *winCodePageIdentifier = table->WinCodePageIdentifier;
    *winCodePageDescription = table->WinCodePageDescription;
    *dirName = table->DirectoryName;
    (*index)++;
    return TRUE;
}

BOOL CCodeTables::GetPreloadedIndex(const char* dirName, int* index)
{
    int i;
    for (i = 0; i < Preloaded.Count; i++)
    {
        CCodeTable* table = Preloaded[i];
        if (stricmp(dirName, table->DirectoryName) == 0)
        {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

void CCodeTables::GetBestPreloadedConversion(const char* cfgDirName, char* dirName)
{
    //  kriterium (1): cfgDirName
    int dummy;
    if (cfgDirName[0] != '*' &&
        GetPreloadedIndex(cfgDirName, &dummy))
    {
        strcpy(dirName, cfgDirName);
        return;
    }

    //  kriterium (2): Polozka odpovidajici kodove strance operacniho systemu
    DWORD cp = GetACP();
    int i;
    for (i = 0; i < Preloaded.Count; i++)
    {
        CCodeTable* table = Preloaded[i];
        if (table->WinCodePageIdentifier == cp)
        {
            strcpy(dirName, table->DirectoryName);
            return;
        }
    }

    //  kriterium (3): westeuro
    if (GetPreloadedIndex("westeuro", &dummy))
    {
        strcpy(dirName, "westeuro");
        return;
    }

    if (Preloaded.Count > 0)
    {
        //  kriterium (4): prvni v seznam
        strcpy(dirName, Preloaded[0]->DirectoryName);
        return;
    }
    else
    {
        //  kriterium (5): pokud neni zadna polozka v seznamu, vratime prazdny retezec
        dirName[0] = 0;
        return;
    }
}

BOOL CCodeTables::Init(HWND hWindow)
{
    BOOL ret = TRUE;
    CALL_STACK_MESSAGE1("CCodeTables::Init()");
    HANDLES(EnterCriticalSection(&LoadCS));
    if (!Loaded)
    {
        BOOL findBest = FALSE;
        if (Configuration.ConversionTable[0] != '*')
        {
            // pokud je inicializovana cesta ke convert.cfg, pokusime se jej nacist
            Table = new CCodeTable(hWindow, Configuration.ConversionTable);
            if (Table != NULL && Table->GetState() != ctsSuccessfullyLoaded)
            {
                // pokud se load nepovedl idealne, dame prilezitost ostatnim tabulkam
                findBest = TRUE;
            }
        }
        else
            findBest = TRUE;

        if (findBest)
        {
            if (Table != NULL)
            {
                delete Table;
                Table = NULL;
            }
            PreloadAllConversions();
            char dirName[MAX_PATH];
            GetBestPreloadedConversion(Configuration.ConversionTable, dirName);
            FreePreloadedConversions();
            strcpy(Configuration.ConversionTable, dirName);
            Table = new CCodeTable(hWindow, Configuration.ConversionTable);
            // Table->State uz netestujeme - bereme vse
        }
        if (Table == NULL)
            TRACE_E(LOW_MEMORY);
        Loaded = Table != NULL;
        ret = Loaded;
    }
    HANDLES(LeaveCriticalSection(&LoadCS));
    return ret;
}

void CCodeTables::InitMenu(HMENU menu, int& codeType)
{
    CALL_STACK_MESSAGE1("CCodeTables::InitMenu(,)");
    if (!Loaded)
    {
        TRACE_E("CCodeTables::InitMenu: Table is not loaded");
        return;
    }
    MENUITEMINFO mi;
    if (GetMenuItemCount(menu) == 0) // prazdne menu, je treba naplnit
    {
        int count = 0;
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID;
        mi.fType = MFT_STRING;
        mi.wID = CM_CODING_MIN;
        mi.dwTypeData = LoadStr(IDS_VIEWERNONECODING);
        InsertMenuItem(menu, count++, TRUE, &mi);

        int i;
        for (i = 0; i < Table->Data.Count; i++)
        {
            if (Table->Data[i]->Name == NULL) // separator
            {
                // pokud by za separatorem nic nebylo, nebudu ho vkladat
                if (i == Table->Data.Count - 1)
                    continue;

                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_TYPE;
                mi.fType = MFT_SEPARATOR;
                InsertMenuItem(menu, count++, TRUE, &mi);
            }
            else
            {
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_TYPE | MIIM_ID;
                mi.fType = MFT_STRING;
                mi.wID = CM_CODING_MIN + i + 1; // +1 kvuli 'None'
                if (mi.wID > CM_CODING_MAX)
                {
                    TRACE_E("mi.wID > CM_CODING_MAX");
                    break;
                }
                mi.dwTypeData = Table->Data[i]->Name;
                InsertMenuItem(menu, count++, TRUE, &mi);
            }
        }
    }

    // nastavime zvolene koncerzi radiak
    if (!Valid(codeType))
        codeType = 0;                                             // invalidni codeType -> prechod na 'none'
    if (codeType == 0 || Table->Data[codeType - 1]->Name != NULL) // ani 'none', ani separator
        CheckMenuRadioItem(menu, CM_CODING_MIN, CM_CODING_MAX, CM_CODING_MIN + codeType, MF_BYCOMMAND);
}

void CCodeTables::Next(int& codeType)
{
    CALL_STACK_MESSAGE1("CCodeTables::Next()");
    if (!Loaded)
    {
        TRACE_E("CCodeTables::Next: Table is not loaded");
        return;
    }
    int i;
    for (i = codeType + 1; i < Table->Data.Count + 1; i++)
    {
        if (Table->Data[i - 1]->Name != NULL)
        {
            codeType = i;
            return;
        }
    }
    codeType = 0;
}

void CCodeTables::Previous(int& codeType)
{
    CALL_STACK_MESSAGE1("CCodeTables::Previous()");
    if (!Loaded)
    {
        TRACE_E("CCodeTables::Previous: Table is not loaded");
        return;
    }
    if (codeType > Table->Data.Count)
        codeType = Table->Data.Count;
    if (codeType == 0)
        codeType = Table->Data.Count + 1;
    int i;
    for (i = codeType - 1; i > 0; i--)
    {
        if (Table->Data[i - 1]->Name != NULL)
        {
            codeType = i;
            return;
        }
    }
    codeType = 0;
}

BOOL CCodeTables::EnumCodeTables(HWND parent, int* index, const char** name, const char** table)
{
    CALL_STACK_MESSAGE1("CCodeTables::EnumCodeTables(, , ,)");
    if (name != NULL)
        *name = NULL;
    if (table != NULL)
        *table = NULL;
    if (*index == 0)
    {
        CodeTables.Init(parent);
        if (!Loaded)
        {
            TRACE_E("CCodeTables::EnumCodeTables: Table is not loaded");
            return FALSE;
        }
    }
    if (*index < Table->Data.Count)
    {
        char* n = Table->Data[*index]->Name;
        if (n != NULL)
        {
            if (name != NULL)
                *name = n;
            if (table != NULL)
                *table = Table->Data[*index]->Table;
        }
        (*index)++;
        return TRUE;
    }
    return FALSE;
}

BOOL CCodeTables::GetCode(char* table, int& codeType)
{
    CALL_STACK_MESSAGE1("CCodeTables::GetCode(,)");
    if (!Loaded)
    {
        TRACE_E("CCodeTables::GetCode: Table is not loaded");
        return FALSE;
    }
    if (!Valid(codeType))
        codeType = 0;
    if (codeType == 0)
        return FALSE; // 'none'
    memcpy(table, Table->Data[codeType - 1]->Table, 256);
    return TRUE;
}

BOOL CCodeTables::GetCodeType(const char* coding, int& codeType)
{
    CALL_STACK_MESSAGE2("CCodeTables::GetCode(%s, )", coding);
    if (!Loaded)
    {
        TRACE_E("CCodeTables::GetCodeType: Table is not loaded");
        return FALSE;
    }
    int i;
    for (i = 0; i < Table->Data.Count; i++)
    {
        const char* n = Table->Data[i]->Name;
        if (n != NULL) // nejde o separator
        {
            const char* c = coding;
            while (1)
            {
                while (*n != 0 && (*n <= ' ' || *n == '-' || *n == '&'))
                    n++;
                while (*c != 0 && (*c <= ' ' || *c == '-') || *c == '&')
                    c++;
                if (LowerCase[*n] != LowerCase[*c] || *n == 0)
                    break;
                n++;
                c++;
            }
            if (*n == 0 && *c == 0)
            {
                codeType = i + 1;
                return TRUE;
            }
        }
    }
    codeType = 0;
    return FALSE; // nenalezeno
}

BOOL CCodeTables::Valid(int codeType)
{
    CALL_STACK_MESSAGE2("CCodeTables::Valid(%d)", codeType);
    if (!Loaded)
    {
        TRACE_E("CCodeTables::Valid: Table is not loaded");
        return FALSE;
    }
    return codeType >= 0 && codeType < Table->Data.Count + 1 &&
           (codeType == 0 || Table->Data[codeType - 1]->Name != NULL);
}

BOOL CCodeTables::GetCodeName(int codeType, char* buffer, int bufferLen)
{
    CALL_STACK_MESSAGE3("CCodeTables::GetCodeName(%d, , %d)", codeType, bufferLen);
    if (!Loaded)
    {
        TRACE_E("CCodeTables::GetCodeName: Table is not loaded");
        return FALSE;
    }
    char buff[1024];
    if (bufferLen > 0)
        buffer[0] = 0;
    if (!Valid(codeType))
        return FALSE;
    if (codeType == 0)
        strcpy(buff, LoadStr(IDS_VIEWERNONECODING));
    else
        strcpy(buff, Table->Data[codeType - 1]->Name);
    int len = (int)strlen(buff);
    if (len > bufferLen)
        len = bufferLen - 1;
    strncpy(buffer, buff, len);
    buffer[len] = 0;
    if ((int)strlen(buff) > bufferLen)
        return FALSE;
    return TRUE;
}

void CCodeTables::GetWinCodePage(char* buf)
{
    CALL_STACK_MESSAGE1("CCodeTables::GetWinCodePage()");
    if (!Loaded)
    {
        TRACE_E("CCodeTables::GetWinCodePage: Table is not loaded");
        buf[0] = 0;
        return;
    }
    strcpy(buf, Table->WinCodePage);
}

void CCodeTables::RecognizeFileType(const char* pattern, int patternLen, BOOL forceText, BOOL* isText,
                                    char* codePage)
{
    CALL_STACK_MESSAGE3("CCodeTables::RecognizeFileType(, %d, %d, ,)", patternLen, forceText);
    if (!Loaded)
    {
        TRACE_E("CCodeTables::RecognizeFileType: Table is not loaded");
        if (codePage != NULL)
            codePage[0] = 0;
        if (isText != NULL)
            *isText = FALSE;
        return;
    }

    if (isText != NULL)
        *isText = FALSE;
    if (codePage != NULL)
        codePage[0] = 0;
    if (patternLen == 0)
    {
        if (isText != NULL)
            *isText = TRUE;
        return; // neni co delat
    }

    char* buf = (char*)malloc(patternLen);
    const char* testBuf;
    char lastCodePage[101];
    lastCodePage[0] = 0;
    int winCodePageLen = (int)strlen(Table->WinCodePage);
    DWORD bestPenalty = 0xFFFFFFFF;
    if (buf != NULL)
    {
        int i;
        for (i = -1; i < Table->Data.Count; i++)
        {
            const char* n = (i == -1 ? NULL : Table->Data[i]->Name);
            testBuf = NULL;
            if (i == -1) // beze zmeny kodovani (text ve WinCodePage)
            {
                testBuf = pattern;
                strcpy(lastCodePage, Table->WinCodePage);
            }
            else
            {
                if (n != NULL && winCodePageLen > 0) // nejde o separator + WinCodePage je nactene
                {
                    // odstranime znaky '&', pri porovnavani by prekazely
                    char buf3[200];
                    lstrcpyn(buf3, n, 200);
                    RemoveAmpersands(buf3);
                    n = buf3;

                    int nameLen = (int)strlen(n); // test jestli "cil" konverze je WinCodePage
                    if (nameLen > winCodePageLen && StrICmp(n + nameLen - winCodePageLen, Table->WinCodePage) == 0)
                    {
                        const char* s = n + nameLen - winCodePageLen;
                        while (s > n && (*(s - 1) <= ' ' || *(s - 1) == '-'))
                            s--;
                        int l = (int)min(100, s - n);
                        memcpy(lastCodePage, n, l);
                        lastCodePage[l] = 0;

                        testBuf = buf;
                        const char* src = pattern;
                        const char* end = pattern + patternLen;
                        char* dst = buf;
                        char* table = Table->Data[i]->Table;
                        while (src < end)
                            *dst++ = table[(unsigned char)*src++];
                    }
                }
            }

            if (testBuf != NULL)
            {
#define PENALTY_TWOSAME_ALPHA_NOR_NUM_PENALTY 50  // dva stejne alpha nebo num znaky
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY 20      // neni alpha ani num znak
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD1 2  // pridavek: neni alpha ani num znak + predchazi alpha/num
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD2 1  // pridavek: neni alpha ani num znak + nasleduje alpha/num
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD3 50 // pridavek: aspon tri stejne znaky (ani alpha ani num) + predchazi alpha/num
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD4 50 // pridavek: aspon tri stejne znaky (ani alpha ani num) + nasleduje alpha/num
#define PENALTY_UPPER_TO_LOWER 2                  // velke pismeno nasledovane malym pismenem
#define PENALTY_LOWER_TO_UPPER 10                 // male pismeno nasledovane velkym pismenem
#define PENALTY_CHANGE 1                          // typ sousednich znaku se lisi (typ = male/velke/cislice)
#define PENALTY_MAYBE_UNKNOWN_CHAR 1              // za pet znaku '?' - mozna jde o nezname znaky v cilovem kodovani (standard je nahrazeni neznameho znaku znakem '?')

                const unsigned char* s = (const unsigned char*)testBuf;
                DWORD penalty = 0;
                const unsigned char* end = s + patternLen;
                int nonAscii = 0; // znaky >= 128 (nepatri do ASCII)
                int binar = 0;
                int minBinar = patternLen / 200;
                int nulls = 0;
                int questions = 0;
                DWORD ignoreChar = -1;
                while (s < end)
                {
                    if (!forceText)
                    {
                        if (*s < ' ' && *s != 0 && *s != '\a' && *s != '\b' && *s != '\r' &&
                            *s != '\f' && *s != '\n' && *s != '\t' && *s != '\v' &&
                            *s != '\x1a' && *s != '\x04')
                        { // nepovoleny znak
                            if (++binar > minBinar)
                                break; // vic nez 0.5% nepovolenych znaku
                        }
                        if (*s == 0)
                        {
                            if (++nulls > 10)
                                break; // vic nez deset NULL za sebou -> nejspis jde o binar
                        }
                        else
                            nulls = 0;
                    }
                    if (*s >= 128)
                        nonAscii++;
                    if (IsNotAlphaNorNum[*s]) // znak neni alpha ani num
                    {
                        if (*s == '?')
                        {
                            if (++questions == 5) // delime peti, abychom oslabili tuto penaltu proti ostatnim
                            {
                                penalty += PENALTY_MAYBE_UNKNOWN_CHAR;
                                questions = 0;
                            }
                        }

                        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
                        { // mezery, tabelatory a konce radku ignorujeme
                            BOOL skipChar = FALSE;
                            if (*s >= 128) // jeden ne-ascii znak ignorujeme (zpusob jak skipnout extra-apostrof ('\x92') v ascii textu)
                            {
                                if (ignoreChar != (DWORD)*s)
                                {
                                    if (ignoreChar == -1)
                                    {
                                        ignoreChar = (DWORD)*s;
                                        skipChar = TRUE;
                                    }
                                }
                                else
                                    skipChar = TRUE;
                            }
                            if (!skipChar)
                            {
                                if (s > (const unsigned char*)testBuf && !IsNotAlphaNorNum[*(s - 1)])
                                {
                                    penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD1;
                                    // aspon tri stejne znaky uvozene alpha nebo num (roh ramecku je pismenko - napr. text v CP437 a testovana stranka CP852)
                                    if (s + 2 < end && *(s + 2) == *(s + 1) && *(s + 1) == *s)
                                    {
                                        penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD3;
                                    }
                                }
                                penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY;
                                if (s + 1 < end && !IsNotAlphaNorNum[*(s + 1)])
                                {
                                    penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD2;
                                    // aspon tri stejne znaky nasledovane alpha nebo num (roh ramecku je pismenko - napr. text v CP437 a testovana stranka CP852)
                                    if (s - 2 >= (const unsigned char*)testBuf && *(s - 2) == *(s - 1) && *(s - 1) == *s)
                                    {
                                        penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD4;
                                    }
                                }
                            }
                        }
                    }
                    else // znak je alpha nebo num
                    {
                        if (s + 1 < end && !IsNotAlphaNorNum[*(s + 1)]) // nasledujici znak je take alpha nebo num
                        {
                            int c1; // aktualni znak je: 1 - lower, 2 - upper, 3 - cislice
                            if (!IsAlpha[*s])
                                c1 = 3;
                            else
                                c1 = UpperCase[*s] == *s ? 2 : 1;
                            int c2; // nasledujici znak je: 1 - lower, 2 - upper, 3 - cislice
                            if (!IsAlpha[*(s + 1)])
                                c2 = 3;
                            else
                                c2 = UpperCase[*(s + 1)] == *(s + 1) ? 2 : 1;

                            if (c1 == 2 && c2 == 1)
                            {
                                if (s > (const unsigned char*)testBuf && IsAlpha[*(s - 1)])
                                    penalty += PENALTY_UPPER_TO_LOWER; // slovo Úrok se jinak prekoduje na MACCE (velke 'U' se totiz zmeni v MACCE na male 'r')
                            }
                            else
                            {
                                if (c1 == 1 && c2 == 2)
                                    penalty += PENALTY_LOWER_TO_UPPER;
                                else
                                {
                                    if (c1 != c2)
                                        penalty += PENALTY_CHANGE;
                                    else
                                    {
                                        if (*s == *(s + 1))
                                            penalty += PENALTY_TWOSAME_ALPHA_NOR_NUM_PENALTY;
                                    }
                                }
                            }
                        }
                    }
                    s++;
                }

                if (s == end) // jde o text
                // && penalty / patternLen <= 5)  // a neni to totalne necitelnej gulas (Lukasuv test:
                // znaky 0x04 -> vyhovel jen EBCDIC, ale pomer byl
                // 10 -> necitelne) - POZOR: nepouzitelne, protoze
                // konfiguracni soubory a .inf soubory vypadaji
                // podle 'penalty' taky jako totalni gulas
                {
                    if (isText != NULL)
                        *isText = TRUE;

                    if (penalty < bestPenalty)
                    {
                        bestPenalty = penalty;
                        if (codePage != NULL)
                            strcpy(codePage, lastCodePage);

                        if (i == -1 && nonAscii * 200 < patternLen)
                            break; // pod 0.5% ne-ASCII znaku -> ASCII, dal nehledame
                    }
                }
            }
        }
        free(buf);
    }
    else
        TRACE_E(LOW_MEMORY);
}

int CCodeTables::GetConversionToWinCodePage(const char* codePage)
{
    CALL_STACK_MESSAGE2("CCodeTables::GetConversionToWinCodePage(%s)", codePage);

    if (!Loaded)
    {
        TRACE_E("CCodeTables::GetConversionToWinCodePage: Table is not loaded");
        return 0;
    }

    // odstranime znaky '&', pri porovnavani by prekazely
    char buf2[200];
    lstrcpyn(buf2, codePage, 200);
    RemoveAmpersands(buf2);
    codePage = buf2;

    if (StrICmp(codePage, Table->WinCodePage) == 0)
        return 0; // vracime "none"
    int winCodePageLen = (int)strlen(Table->WinCodePage);
    if (winCodePageLen > 0) // jen pokud je WinCodePage nactena
    {
        int i;
        for (i = 0; i < Table->Data.Count; i++)
        {
            const char* n = Table->Data[i]->Name;
            if (n != NULL) // nejde o separator
            {
                // odstranime znaky '&', pri porovnavani by prekazely
                char buf3[200];
                lstrcpyn(buf3, n, 200);
                RemoveAmpersands(buf3);
                n = buf3;

                int nameLen = (int)strlen(n); // test jestli "cil" konverze je WinCodePage
                if (nameLen > winCodePageLen && StrICmp(n + nameLen - winCodePageLen, Table->WinCodePage) == 0)
                {
                    const char* s = n + nameLen - winCodePageLen;
                    while (s > n && (*(s - 1) <= ' ' || *(s - 1) == '-'))
                        s--;
                    int l = (int)min(100, s - n);
                    if (StrNICmp(n, codePage, l) == 0)
                        return i + 1; // nalezeno
                }
            }
        }
    }
    return -1; // nenalezeno
}