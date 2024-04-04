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
{ // reads file 'fileName', encodes 'table', returns text in case of error, otherwise NULL
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

    char convertCfgFileName[MAX_PATH]; // for error messages
    strcpy(convertCfgFileName, fileName);

    BOOL comment;
    char* text; // != NULL - error message
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
                txt += 18;      // jump "WINDOWS_CODE_PAGE="
                comment = TRUE; // do not process further (as a comment)

                // read the name "windows code page"
                while (txt < endTxt && (*txt == ' ' || *txt == '\t'))
                    txt++; // skip white-spaces
                char* beg = txt;
                while (txt < endTxt && *txt != '\r' && *txt != '\n')
                    txt++;
                int l = (int)min(txt - beg, 100);
                memcpy(winCodePage, beg, l);
                while (l > 0 && (winCodePage[l - 1] == ' ' || winCodePage[l - 1] == '\t'))
                    l--; // Trim white-spaces
                winCodePage[l] = 0;
            }
            else if (endTxt - txt >= 29 && StrNICmp(txt, "WINDOWS_CODE_PAGE_IDENTIFIER=", 29) == 0)
            {
                txt += 29;      // jump "WINDOWS_CODE_PAGE_IDENTIFIER="
                comment = TRUE; // do not process further (as a comment)

                // read the identifier
                while (txt < endTxt && (*txt == ' ' || *txt == '\t'))
                    txt++; // skip white-spaces
                char* beg = txt;
                while (txt < endTxt && *txt != '\r' && *txt != '\n')
                    txt++;
                int l = (int)min(txt - beg, 100);
                char buff[101];
                memcpy(buff, beg, l);
                while (l > 0 && (identifier[l - 1] == ' ' || identifier[l - 1] == '\t'))
                    l--; // Trim white-spaces
                buff[l] = 0;
                *identifier = atoi(buff);
            }
            else if (endTxt - txt >= 30 && StrNICmp(txt, "WINDOWS_CODE_PAGE_DESCRIPTION=", 30) == 0)
            {
                txt += 30;      // jump "WINDOWS_CODE_PAGE_DESCRIPTION="
                comment = TRUE; // do not process further (as a comment)

                // read the description
                while (txt < endTxt && (*txt == ' ' || *txt == '\t'))
                    txt++; // skip white-spaces
                char* beg = txt;
                while (txt < endTxt && *txt != '\r' && *txt != '\n')
                    txt++;
                int l = (int)min(txt - beg, 100);
                memcpy(description, beg, l);
                while (l > 0 && (description[l - 1] == ' ' || description[l - 1] == '\t'))
                    l--; // Trim white-spaces
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
                    if (*txt == '#') // comment
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
                                txt++; // jump '=' or '|'
                                beg = txt;
                                while (txt < endTxt && *txt != '\r' && *txt != '\n' && *txt != '|')
                                    txt++;
                                if (beg < txt) // another encoding file (or ANSI->OEM/OEM->ANSI)
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
                                        if (*beg == ':') // internal variable (ANSI->OEM/OEM->ANSI)
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
                                        else // relative file name
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
                        else // missing '=' or is at the beginning of the line (and not doubled)
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

            if (text == NULL && !comment) // adding code to the code table
            {
                CCodeTablesData* code = new CCodeTablesData;
                if (code != NULL)
                {
                    if (name != NULL) // normal item
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

            // move txt after the first EOL
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
        // error in file
        char buf[MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_FILEREADERROR), fileName);
        SalMessageBox(hWindow, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
}

CCodeTable::CCodeTable(HWND hWindow, const char* dirName)
    : Data(10, 5)
{
    WinCodePage[0] = 0;
    WinCodePageIdentifier = 0xffffffff; // 0 is not suitable because GetACP returns 0 for UNICODE only encoding
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
    else // if there is no convert\\xxx\\convert.cfg, we will load the default configuration
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
                    // try to open the internal file convert.cfg
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
                        delete table; // we are not interested in the default table -- we will discard it
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
    //  criterion (1): cfgDirName
    int dummy;
    if (cfgDirName[0] != '*' &&
        GetPreloadedIndex(cfgDirName, &dummy))
    {
        strcpy(dirName, cfgDirName);
        return;
    }

    //  criterion (2): Item corresponding to the code page of the operating system
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

    //  criterion (3): westeuro
    if (GetPreloadedIndex("westeuro", &dummy))
    {
        strcpy(dirName, "westeuro");
        return;
    }

    if (Preloaded.Count > 0)
    {
        //  criterion (4): first in the list
        strcpy(dirName, Preloaded[0]->DirectoryName);
        return;
    }
    else
    {
        //  criterion (5): if there are no items in the list, we return an empty string
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
            // if the path to convert.cfg is initialized, we will try to load it
            Table = new CCodeTable(hWindow, Configuration.ConversionTable);
            if (Table != NULL && Table->GetState() != ctsSuccessfullyLoaded)
            {
                // if the load did not succeed perfectly, we will give a chance to other tables
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
            // We no longer test Table->State - we take everything
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
    if (GetMenuItemCount(menu) == 0) // empty menu, needs to be filled
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
                // if there is nothing behind the separator, I will not insert it
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
                mi.wID = CM_CODING_MIN + i + 1; // +1 for 'None'
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

    // set the selected concert radio
    if (!Valid(codeType))
        codeType = 0;                                             // invalid codeType -> switching to 'none'
    if (codeType == 0 || Table->Data[codeType - 1]->Name != NULL) // neither 'none' nor separator
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
        if (n != NULL) // not a separator
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
    return FALSE; // not found
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
        return; // nothing to do
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
            if (i == -1) // without changing the encoding (text in WinCodePage)
            {
                testBuf = pattern;
                strcpy(lastCodePage, Table->WinCodePage);
            }
            else
            {
                if (n != NULL && winCodePageLen > 0) // It's not about the separator + WinCodePage is loaded
                {
                    // remove characters '&', they would interfere with comparison
                    char buf3[200];
                    lstrcpyn(buf3, n, 200);
                    RemoveAmpersands(buf3);
                    n = buf3;

                    int nameLen = (int)strlen(n); // test if the "target" conversion is WinCodePage
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
#define PENALTY_TWOSAME_ALPHA_NOR_NUM_PENALTY 50  // two same alpha or num characters
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY 20      // not an alpha nor a numeric character
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD1 2  // modifier: not an alpha or num character + preceded by an alpha/num
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD2 1  // condition: not an alpha or num character + followed by an alpha/num
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD3 50 // condition: at least three identical characters (neither alpha nor num) + preceded by alpha/num
#define PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD4 50 // condition: at least three identical characters (neither alpha nor num) + followed by alpha/num
#define PENALTY_UPPER_TO_LOWER 2                  // capital letter followed by a lowercase letter
#define PENALTY_LOWER_TO_UPPER 10                 // lowercase letter followed by uppercase letter
#define PENALTY_CHANGE 1                          // the type of neighboring characters differs (type = lowercase/uppercase/digit)
#define PENALTY_MAYBE_UNKNOWN_CHAR 1              // for five characters '?' - it may be unknown characters in the target encoding (the standard is to replace an unknown character with '?')

                const unsigned char* s = (const unsigned char*)testBuf;
                DWORD penalty = 0;
                const unsigned char* end = s + patternLen;
                int nonAscii = 0; // characters >= 128 (not part of ASCII)
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
                        { // illegal character
                            if (++binar > minBinar)
                                break; // more than 0.5% of illegal characters
                        }
                        if (*s == 0)
                        {
                            if (++nulls > 10)
                                break; // more than ten NULLs in a row -> probably binary
                        }
                        else
                            nulls = 0;
                    }
                    if (*s >= 128)
                        nonAscii++;
                    if (IsNotAlphaNorNum[*s]) // character is neither alpha nor num
                    {
                        if (*s == '?')
                        {
                            if (++questions == 5) // Dividing by five to weaken this penalty against the others
                            {
                                penalty += PENALTY_MAYBE_UNKNOWN_CHAR;
                                questions = 0;
                            }
                        }

                        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
                        { // we ignore spaces, tabs, and end of lines
                            BOOL skipChar = FALSE;
                            if (*s >= 128) // we ignore one non-ascii character (way to skip extra-apostrophe ('\x92') in ascii text)
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
                                    // at least three identical characters enclosed in alpha or num (the corner frame is a letter - for example, text in CP437 and the tested page in CP852)
                                    if (s + 2 < end && *(s + 2) == *(s + 1) && *(s + 1) == *s)
                                    {
                                        penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD3;
                                    }
                                }
                                penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY;
                                if (s + 1 < end && !IsNotAlphaNorNum[*(s + 1)])
                                {
                                    penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD2;
                                    // at least three identical characters followed by an alpha or num (the corner bracket is a letter - for example, text in CP437 and the tested page in CP852)
                                    if (s - 2 >= (const unsigned char*)testBuf && *(s - 2) == *(s - 1) && *(s - 1) == *s)
                                    {
                                        penalty += PENALTY_NOT_ALPHA_NOR_NUM_PENALTY_ADD4;
                                    }
                                }
                            }
                        }
                    }
                    else // character is alpha or num
                    {
                        if (s + 1 < end && !IsNotAlphaNorNum[*(s + 1)]) // the following character is also alpha or num
                        {
                            int c1; // current character is: 1 - lower, 2 - upper, 3 - digit
                            if (!IsAlpha[*s])
                                c1 = 3;
                            else
                                c1 = UpperCase[*s] == *s ? 2 : 1;
                            int c2; // the following character is: 1 - lower, 2 - upper, 3 - digit
                            if (!IsAlpha[*(s + 1)])
                                c2 = 3;
                            else
                                c2 = UpperCase[*(s + 1)] == *(s + 1) ? 2 : 1;

                            if (c1 == 2 && c2 == 1)
                            {
                                if (s > (const unsigned char*)testBuf && IsAlpha[*(s - 1)])
                                    penalty += PENALTY_UPPER_TO_LOWER; // The word 'Úrok' is otherwise encoded as MACCE (because the capital 'U' changes to lowercase 'r' in MACCE)
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

                if (s == end) // it's about text
                // && penalty / patternLen <= 5)  // and it's not a completely unreadable mess (Lukas's test:
                // characters 0x04 -> only satisfied EBCDIC, but the ratio was
                // 10 -> unreadable) - WARNING: unusable because
                // configuration files and .inf files look
                // according to 'penalty' also like total goulash
                {
                    if (isText != NULL)
                        *isText = TRUE;

                    if (penalty < bestPenalty)
                    {
                        bestPenalty = penalty;
                        if (codePage != NULL)
                            strcpy(codePage, lastCodePage);

                        if (i == -1 && nonAscii * 200 < patternLen)
                            break; // below 0.5% non-ASCII characters -> ASCII, further we do not search
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

    // remove characters '&', they would interfere with comparison
    char buf2[200];
    lstrcpyn(buf2, codePage, 200);
    RemoveAmpersands(buf2);
    codePage = buf2;

    if (StrICmp(codePage, Table->WinCodePage) == 0)
        return 0; // returning "none"
    int winCodePageLen = (int)strlen(Table->WinCodePage);
    if (winCodePageLen > 0) // only if WinCodePage is loaded
    {
        int i;
        for (i = 0; i < Table->Data.Count; i++)
        {
            const char* n = Table->Data[i]->Name;
            if (n != NULL) // not a separator
            {
                // remove characters '&', they would interfere with comparison
                char buf3[200];
                lstrcpyn(buf3, n, 200);
                RemoveAmpersands(buf3);
                n = buf3;

                int nameLen = (int)strlen(n); // test if the "target" conversion is WinCodePage
                if (nameLen > winCodePageLen && StrICmp(n + nameLen - winCodePageLen, Table->WinCodePage) == 0)
                {
                    const char* s = n + nameLen - winCodePageLen;
                    while (s > n && (*(s - 1) <= ' ' || *(s - 1) == '-'))
                        s--;
                    int l = (int)min(100, s - n);
                    if (StrNICmp(n, codePage, l) == 0)
                        return i + 1; // found
                }
            }
        }
    }
    return -1; // not found
}