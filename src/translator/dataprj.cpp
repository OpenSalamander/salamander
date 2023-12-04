// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "translator.h"

#include "wndout.h"

int ConvertU2A(const WCHAR* src, int srcLen, char* buf, int bufSize, BOOL compositeCheck, UINT codepage)
{
    if (buf == NULL || bufSize <= 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    buf[0] = 0;
    if (src == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (srcLen != -1 && srcLen <= 0)
    {
        if (srcLen == 0)
            return 1;
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    int res = WideCharToMultiByte(codepage, compositeCheck ? WC_COMPOSITECHECK : 0, src, srcLen, buf, bufSize, NULL, NULL);
    if (srcLen != -1 && res > 0)
        res++;
    if (compositeCheck && res == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER) // nektere codepage nepodporuji WC_COMPOSITECHECK
    {
        res = WideCharToMultiByte(codepage, 0, src, srcLen, buf, bufSize, NULL, NULL);
        if (srcLen != -1 && res > 0)
            res++;
    }
    if (res > 0 && res <= bufSize)
        buf[res - 1] = 0; // uspech, zakoncime string nulou
    else
    {
        if (res > bufSize || res == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            buf[bufSize - 1] = 0; // maly buffer, vratime chybu, ale castecne prelozeny string nechame v bufferu
        }
        else
            buf[0] = 0; // jina chyba, zajistime prazdny buffer
        res = 0;
    }
    return res;
}

//***********************************************************
//
// PATH rutiny od Lukase z SFX
//

LPTSTR PathAddBackslash(LPTSTR pszPath)
{
    if (pszPath == NULL)
        return NULL;
    int len = lstrlen(pszPath);
    if (len > 0 && pszPath[len - 1] != '\\')
    {
        pszPath[len] = '\\';
        pszPath[len + 1] = 0;
        len++;
    }
    return pszPath + len;
}

BOOL PathAppend(LPTSTR pPath, LPCTSTR pMore)
{
    if (pMore[0] == 0)
    {
        return TRUE;
    }
    PathAddBackslash(pPath);
    if (*pMore == '\\')
        pMore++;
    lstrcat(pPath, pMore);
    return TRUE;
}

BOOL PathRemoveFileSpec(LPTSTR pszPath)
{
    int len = lstrlen(pszPath);
    char* iterator = pszPath + len - 1;
    while (iterator >= pszPath)
    {
        if (*iterator == '\\')
        {
            if (iterator - 1 < pszPath || *(iterator - 1) == ':')
                iterator++;
            *iterator = 0;
            break;
        }
        iterator--;
    }
    return TRUE;
}

LPTSTR StrRChr(LPCTSTR lpStart, LPCTSTR lpEnd, char wMatch)
{
    lpEnd--;
    while (lpEnd >= lpStart)
    {
        if (*lpEnd == wMatch)
            return (LPTSTR)lpEnd;
        lpEnd--;
    }
    return NULL;
}

LPTSTR StrChr(LPCTSTR lpStart, char wMatch)
{
    if (lpStart == NULL)
        return NULL;
    while (*lpStart)
    {
        if (*lpStart == wMatch)
            return (LPTSTR)lpStart;
        lpStart++;
    }
    return NULL;
}

LPTSTR PathRemoveBackslash(LPTSTR pszPath)
{
    if (pszPath == NULL)
        return NULL;
    int len = lstrlen(pszPath);
    if (len > 0 && pszPath[len - 1] == '\\')
    {
        pszPath[len - 1] = 0;
        len--;
    }
    else if (len > 1)
        len--;
    return pszPath + len;
}

BOOL PathStripToRoot(LPTSTR pszPath)
{
    int len = lstrlen(pszPath);
    if (len < 2)
        return FALSE;
    if (pszPath[0] == '\\' && pszPath[1] == '\\') // UNC
    {
        char* s = pszPath + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        s++;
        *s = 0;
    }
    else
    {
        if (len > 3)
            pszPath[3] = 0;
    }
    len = lstrlen(pszPath);
    if (pszPath[len - 1] != '\\')
    {
        pszPath[len] = '\\';
        pszPath[len + 1] = 0;
    }
    return TRUE;
}

int GetRootLen(const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        return (s - path) + 1;
    }
    else
        return 3;
}

BOOL PathMerge(char* fullPath, const char* relativePath)
{
    char* destMax = fullPath + MAX_PATH - 1;
    char* dest;
    const char* sour = relativePath;

    PathAddBackslash(fullPath);
    if (relativePath[0] == '\\') //UNC or relative to root dir
    {
        if (relativePath[1] == '\\')
            *fullPath = 0; //UNC
        else
        {
            PathStripToRoot(fullPath); //root dir
            sour++;
        }
    }
    else
    {
        if (relativePath[1] == ':')
            *fullPath = 0; //drive letter
    }
    dest = fullPath + lstrlen(fullPath);

    while (*sour && dest < destMax)
    {
        if (*sour == '.' && (sour == relativePath || sour[-1] == '\\'))
        {
            if (sour[1] == '.' && (sour[2] == 0 || sour[2] == '\\'))
            {
                *dest = NULL;
                if (GetRootLen(fullPath) < lstrlen(fullPath))
                    dest = StrRChr(fullPath, dest - 1, '\\') + 1;
                sour += 2;
                if (*sour)
                    sour++; //preskocime jeste lomitko nejsme-li  na konci
                continue;
            }
            if (sour[1] == '\\')
            {
                sour += 2;
                continue;
            }
            if (sour[1] == 0)
            {
                break;
            }
        }
        *dest++ = *sour++;
    }
    *dest = NULL;
    PathRemoveBackslash(fullPath);
    return dest < destMax;
}

//*****************************************************************************
//
// CRC32
//

static DWORD Crc32Tab[256];
static BOOL Crc32TabInitialized = FALSE;

void MakeCrc32Table(DWORD* crcTab)
{
    DWORD c;
    DWORD poly = 0xedb88320L; //polynomial exclusive-or pattern

    for (int n = 0; n < 256; n++)
    {
        c = (UINT32)n;

        for (int k = 0; k < 8; k++)
            c = c & 1 ? poly ^ (c >> 1) : c >> 1;

        crcTab[n] = c;
    }
}

DWORD UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal)
{
    if (buffer == NULL)
        return 0;

    if (!Crc32TabInitialized)
    {
        MakeCrc32Table(Crc32Tab);
        Crc32TabInitialized = TRUE;
    }

    BYTE* p = (BYTE*)buffer;
    DWORD c = crcVal ^ 0xFFFFFFFF;

    if (count)
        do
        {
            c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
        } while (--count);

    return c ^ 0xFFFFFFFF; /* (instead of ~c for 64-bit machines) */
}

BOOL GetFileCRC(const char* fileName, DWORD* crc)
{
    char buf[MAX_PATH + 1000];
    wchar_t outputBuff[500];

    *crc = 0;
    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        swprintf_s(outputBuff, L"Getting CRC32 of existing SLT file FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0xFFFFFFFF || size == 0)
    {
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        swprintf_s(outputBuff, L"Getting CRC32 of existing SLT file FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    char* data = (char*)malloc(size + 2);
    if (data == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        swprintf_s(outputBuff, L"Getting CRC32 of existing SLT file FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, data, size, &read, NULL) || read != size)
    {
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(data);
        HANDLES(CloseHandle(hFile));
        swprintf_s(outputBuff, L"Getting CRC32 of existing SLT file FAILED.");
        OutWindow.AddLine(outputBuff, mteError);
        return FALSE;
    }
    data[size] = 0; // vlozime terminator

    *crc = UpdateCrc32(data, size, 0);

    free(data);
    HANDLES(CloseHandle(hFile));
    return TRUE;
}

// *****************************************************************************
//
// CData
//

BOOL CData::ProcessProjectLine(CProjectSectionEnum* section, const char* line, int row)
{
    char identifier[100];

    if (*line == 0)
        return TRUE;

    if (*line == '[')
    {
        *section = pseNone;
        if (strcmp(line, "[Files]") == 0)
            *section = pseFiles;
        if (strcmp(line, "[Settings]") == 0)
            *section = pseSettings;
        if (strcmp(line, "[Progress]") == 0)
            *section = pseDummy; // tuto sekci ignorujeme
        if (strcmp(line, "[DialogsTranslation]") == 0)
            *section = pseDialogsTranslation;
        if (strcmp(line, "[MenusTranslation]") == 0)
            *section = pseMenusTranslation;
        if (strcmp(line, "[StringsTranslation]") == 0)
            *section = pseStringsTranslation;
        if (strcmp(line, "[Relayout]") == 0)
            *section = pseRelayout;

        if (*section == pseNone)
        {
            char errbuf[MAX_PATH + 100];
            sprintf_s(errbuf, "Error reading Project.\nSyntax error on line %d", row);
            MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        return TRUE;
    }

    // vyhledame znak '='
    const char* p = line;
    while (*p != '=' && *p != 0)
        p++;
    int len = p - line;
    if (*p != '=' || len == 0 || len > 99 || *(p + 1) == 0)
    {
        char errbuf[MAX_PATH + 100];
        sprintf_s(errbuf, "Error reading Project.\nSyntax error on line %d", row);
        MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    p++;

    memcpy(identifier, line, len);
    identifier[len] = 0;

    if (*section == pseFiles)
    {
        if (strcmp(identifier, "Original") == 0)
        {
            lstrcpyn(SourceFile, p, MAX_PATH);

            lstrcpy(FullSourceFile, ProjectFile);
            PathRemoveFileSpec(FullSourceFile);
            PathMerge(FullSourceFile, SourceFile);

            return TRUE;
        }

        if (strcmp(identifier, "Translated") == 0)
        {
            lstrcpyn(TargetFile, p, MAX_PATH);

            lstrcpy(FullTargetFile, ProjectFile);
            PathRemoveFileSpec(FullTargetFile);
            PathMerge(FullTargetFile, TargetFile);

            return TRUE;
        }

        if (strcmp(identifier, "Include") == 0)
        {
            lstrcpyn(IncludeFile, p, MAX_PATH);

            lstrcpy(FullIncludeFile, ProjectFile);
            PathRemoveFileSpec(FullIncludeFile);
            PathMerge(FullIncludeFile, IncludeFile);

            return TRUE;
        }

        if (strcmp(identifier, "SalMenu") == 0)
        {
            lstrcpyn(SalMenuFile, p, MAX_PATH);

            lstrcpy(FullSalMenuFile, ProjectFile);
            PathRemoveFileSpec(FullSalMenuFile);
            PathMerge(FullSalMenuFile, SalMenuFile);

            return TRUE;
        }

        if (strcmp(identifier, "IgnoreList") == 0)
        {
            lstrcpyn(IgnoreLstFile, p, MAX_PATH);

            lstrcpy(FullIgnoreLstFile, ProjectFile);
            PathRemoveFileSpec(FullIgnoreLstFile);
            PathMerge(FullIgnoreLstFile, IgnoreLstFile);

            return TRUE;
        }

        if (strcmp(identifier, "CheckList") == 0)
        {
            lstrcpyn(CheckLstFile, p, MAX_PATH);

            lstrcpy(FullCheckLstFile, ProjectFile);
            PathRemoveFileSpec(FullCheckLstFile);
            PathMerge(FullCheckLstFile, CheckLstFile);

            return TRUE;
        }

        if (strcmp(identifier, "SalamanderExe") == 0)
        {
            lstrcpyn(SalamanderExeFile, p, MAX_PATH);

            lstrcpy(FullSalamanderExeFile, ProjectFile);
            PathRemoveFileSpec(FullSalamanderExeFile);
            PathMerge(FullSalamanderExeFile, SalamanderExeFile);

            return TRUE;
        }

        if (strcmp(identifier, "Export") == 0)
        {
            lstrcpyn(ExportFile, p, MAX_PATH);

            lstrcpy(FullExportFile, ProjectFile);
            PathRemoveFileSpec(FullExportFile);
            PathMerge(FullExportFile, ExportFile);

            return TRUE;
        }

        if (strcmp(identifier, "ExportAsTextArchive") == 0)
        {
            lstrcpyn(ExportAsTextArchiveFile, p, MAX_PATH);

            //      lstrcpy(FullExportFile, ProjectFile);
            //      PathRemoveFileSpec(FullExportFile);
            //      PathMerge(FullExportFile, ExportFile);

            return TRUE;
        }
    }

    if (*section == pseSettings)
    {
        if (strcmp(identifier, "ExpandStrings") == 0)
        {
            OpenStringTables = atoi(p);
            return TRUE;
        }

        if (strcmp(identifier, "ExpandMenus") == 0)
        {
            OpenMenuTables = atoi(p);
            return TRUE;
        }

        if (strcmp(identifier, "ExpandDialogs") == 0)
        {
            OpenDialogTables = atoi(p);
            return TRUE;
        }

        if (strcmp(identifier, "SelectedTreeItem") == 0)
        {
            SelectedTreeItem = atoi(p);
            return TRUE;
        }
    }

    if (*section == pseDummy)
    {
        // sekci ignorujeme
        return TRUE;
    }

    if (*section == pseDialogsTranslation || *section == pseMenusTranslation || *section == pseStringsTranslation)
    {
        if (identifier[0] >= '0' && identifier[0] <= '9')
        {
            DWORD id = 0;
            sscanf_s(identifier, "%08x", &id);
            if (id == 0)
            {
                char errbuf[MAX_PATH + 100];
                sprintf_s(errbuf, "Error reading Project.\nSyntax error on line %d", row);
                MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
            int state = atoi(p);
            CTranslationTypeEnum transType = tteDialogs;
            if (*section == pseMenusTranslation)
                transType = tteMenus;
            if (*section == pseStringsTranslation)
                transType = tteStrings;
            if (!AddTranslationState(transType, LOWORD(id), HIWORD(id), state))
            {
                return FALSE;
            }
            return TRUE;
        }
    }

    if (*section == pseRelayout)
    {
        if (identifier[0] >= '0' && identifier[0] <= '9')
        {
            DWORD id = 0;
            sscanf_s(identifier, "%u", &id);
            if (id == 0)
            {
                char errbuf[MAX_PATH + 100];
                sprintf_s(errbuf, "Error reading Project.\nSyntax error on line %d", row);
                MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
            AddRelayout(LOWORD(id));
            return TRUE;
        }
    }

    char errbuf[MAX_PATH + 100];
    sprintf_s(errbuf, "Error reading Project.\nSyntax error on line %d", row);
    MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
    return FALSE;
}

BOOL CData::LoadProject(const char* fileName)
{
    lstrcpy(ProjectFile, fileName);

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error opening file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0xFFFFFFFF || size == 0)
    {
        char buf[MAX_PATH + 100];
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    char* data = (char*)malloc(size + 1);
    if (data == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, data, size, &read, NULL) || read != size)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(data);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }
    data[size] = 0; // vlozime terminator

    char* lineStart = data;
    DWORD count = 0;
    DWORD line = 0;
    CProjectSectionEnum section = pseNone;
    while (count < size)
    {
        char* lineEnd = lineStart;
        while (*lineEnd != '\r' && *lineEnd != '\n' && count < size)
        {
            lineEnd++;
            count++;
        }
        line++;
        int lineLen = lineEnd - lineStart;

        if (count + 1 < size &&
            ((*lineEnd == '\r' && *(lineEnd + 1) == '\n') ||
             (*lineEnd == '\n' && *(lineEnd + 1) == '\r')))
        {
            *lineEnd = 0;
            *(lineEnd + 1) = 0;
            lineEnd += 2;
            count += 2;
        }

        if (lineLen > 0)
        {
            if (!ProcessProjectLine(&section, lineStart, line))
            {
                // chyba byla zobrazena uvnitr
                free(data);
                HANDLES(CloseHandle(hFile));
                return FALSE;
            }
        }
        lineStart = lineEnd;
    }

    HANDLES(CloseHandle(hFile));

    free(data);

    return TRUE;
}

BOOL CData::WriteProjectLine(HANDLE hFile, const char* line)
{
    DWORD len = lstrlen(line);
    DWORD written;
    if (!WriteFile(hFile, line, len, &written, NULL) || written != len)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing project file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    const char* buff = "\r\n";
    len = 2;
    if (!WriteFile(hFile, buff, len, &written, NULL) || written != len)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing project file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

#define UNICODE_BOM 0xFEFF
BOOL CData::WriteUnicodeBOM(HANDLE hFile)
{
    DWORD len = 2;
    DWORD written;
    WORD data = UNICODE_BOM;
    if (!WriteFile(hFile, &data, len, &written, NULL) || written != len)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing unicode text file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

#define UTF8_BOM 0xFEFF
BOOL CData::WriteUTF8BOM(HANDLE hFile, DWORD* fileCRC32)
{
    DWORD len = 3;
    DWORD written;
    BYTE data[3] = {0xEF, 0xBB, 0xBF};
    if (!WriteFile(hFile, data, len, &written, NULL) || written != len)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing UTF-8 BOM file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    *fileCRC32 = UpdateCrc32(data, len, *fileCRC32);
    return TRUE;
}

/*
BOOL
CData::VerifyBOM(WORD bom)
{
  if (bom != UNICODE_BOM)
  {
    char buf[MAX_PATH + 100];
    DWORD err = GetLastError();
    sprintf_s(buf, "Wrong BOM in the unicode text file.\n%s", GetErrorText(err));
    MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
    return FALSE;
  }
  return TRUE;
}
*/
BOOL CData::WriteUnicodeTextLine(HANDLE hFile, const wchar_t* line)
{
    DWORD len = wcslen(line);
    DWORD written;
    if (!WriteFile(hFile, line, len * 2, &written, NULL) || written != len * 2)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing unicode text file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    const wchar_t* buff = L"\r\n";
    len = 2;
    if (!WriteFile(hFile, buff, len * 2, &written, NULL) || written != len * 2)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing unicode text file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

BOOL CData::WriteUTF8TextLine(HANDLE hFile, const wchar_t* line, DWORD* fileCRC32)
{
    char utf8Line[10000];
    ConvertU2A(line, -1, utf8Line, 10000, FALSE, CP_UTF8);
    DWORD len = strlen(utf8Line);

    DWORD written;
    if (!WriteFile(hFile, utf8Line, len, &written, NULL) || written != len)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing unicode text file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    *fileCRC32 = UpdateCrc32(utf8Line, len, *fileCRC32);
    const char* buff = "\r\n";
    len = 2;
    if (!WriteFile(hFile, buff, len, &written, NULL) || written != len)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing unicode text file.\n%s", GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    *fileCRC32 = UpdateCrc32(buff, len, *fileCRC32);
    return TRUE;
}

BOOL CData::SaveProject()
{
    HANDLE hFile = HANDLES_Q(CreateFile(ProjectFile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                        CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error writing file %s.\n%s", ProjectFile, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    BOOL ret = TRUE;
    char buff[10000];
    if (ret)
        ret &= WriteProjectLine(hFile, "[Files]");
    sprintf_s(buff, "Original=%s", Data.SourceFile);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    sprintf_s(buff, "Translated=%s", Data.TargetFile);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    sprintf_s(buff, "Include=%s", Data.IncludeFile);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    if (Data.SalMenuFile[0] != 0)
    {
        sprintf_s(buff, "SalMenu=%s", Data.SalMenuFile);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
    }
    if (Data.IgnoreLstFile[0] != 0)
    {
        sprintf_s(buff, "IgnoreList=%s", Data.IgnoreLstFile);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
    }
    if (Data.CheckLstFile[0] != 0)
    {
        sprintf_s(buff, "CheckList=%s", Data.CheckLstFile);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
    }
    if (Data.SalamanderExeFile[0] != 0)
    {
        sprintf_s(buff, "SalamanderExe=%s", Data.SalamanderExeFile);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
    }
    if (Data.ExportFile[0] != 0)
    {
        sprintf_s(buff, "Export=%s", Data.ExportFile);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
    }
    if (Data.ExportAsTextArchiveFile[0] != 0)
    {
        sprintf_s(buff, "ExportAsTextArchive=%s", Data.ExportAsTextArchiveFile);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
    }

    if (ret)
        ret &= WriteProjectLine(hFile, "");

    if (ret)
        ret &= WriteProjectLine(hFile, "[Settings]");
    sprintf_s(buff, "ExpandStrings=%d", Data.OpenStringTables);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    sprintf_s(buff, "ExpandMenus=%d", Data.OpenMenuTables);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    sprintf_s(buff, "ExpandDialogs=%d", Data.OpenDialogTables);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    sprintf_s(buff, "SelectedTreeItem=%d", Data.SelectedTreeItem);
    if (ret)
        ret &= WriteProjectLine(hFile, buff);
    if (ret)
        ret &= WriteProjectLine(hFile, "");

    if (ret)
        ret &= WriteProjectLine(hFile, "[DialogsTranslation]");
    for (int i = 0; i < DialogsTranslationStates.Count; i++)
    {
        sprintf_s(buff, "%08x=%d", DialogsTranslationStates[i].ID, DialogsTranslationStates[i].State);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
        if (!ret)
            break;
    }
    if (ret)
        ret &= WriteProjectLine(hFile, "");

    if (ret)
        ret &= WriteProjectLine(hFile, "[MenusTranslation]");
    for (int i = 0; i < MenusTranslationStates.Count; i++)
    {
        sprintf_s(buff, "%08x=%d", MenusTranslationStates[i].ID, MenusTranslationStates[i].State);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
        if (!ret)
            break;
    }
    if (ret)
        ret &= WriteProjectLine(hFile, "");

    if (ret)
        ret &= WriteProjectLine(hFile, "[StringsTranslation]");
    for (int i = 0; i < StringsTranslationStates.Count; i++)
    {
        sprintf_s(buff, "%08x=%d", StringsTranslationStates[i].ID, StringsTranslationStates[i].State);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
        if (!ret)
            break;
    }
    if (ret)
        ret &= WriteProjectLine(hFile, "");

    if (ret)
        ret &= WriteProjectLine(hFile, "[Relayout]");
    for (int i = 0; i < Relayout.Count; i++)
    {
        sprintf_s(buff, "%u=1", Relayout[i]);
        if (ret)
            ret &= WriteProjectLine(hFile, buff);
        if (!ret)
            break;
    }
    if (ret)
        ret &= WriteProjectLine(hFile, "");

    HANDLES(CloseHandle(hFile));
    return ret;
}

void CData::CleanRelayout()
{
    Relayout.DestroyMembers();
}

void CData::AddRelayout(WORD id)
{
    for (int i = 0; i < Relayout.Count; i++)
    {
        if (Relayout[i] == id)
            return;
        if (Relayout[i] > id)
        {
            Relayout.Insert(i, id);
            return;
        }
    }
    if (Relayout.Count == 0 || Relayout[Relayout.Count - 1] != id)
        Relayout.Add(id);
}

BOOL CData::RemoveRelayout(WORD id)
{
    BOOL found = FALSE;
    for (int i = 0; i < Relayout.Count; i++)
    {
        if (Relayout[i] == id)
        {
            Relayout.Delete(i);
            i--;
            found = TRUE;
        }
    }
    return found;
}

void CData::CleanTranslationStates()
{
    DialogsTranslationStates.DestroyMembers();
    MenusTranslationStates.DestroyMembers();
    StringsTranslationStates.DestroyMembers();
}

TDirectArray<CProgressItem>*
CData::GetTranslationStates(CTranslationTypeEnum type)
{
    switch (type)
    {
    case tteDialogs:
        return &DialogsTranslationStates;
    case tteMenus:
        return &MenusTranslationStates;
    case tteStrings:
        return &StringsTranslationStates;
    default:
        TRACE_E("CData::GetTranslationStates - Unknown type " << type);
    }
    return NULL;
}

BOOL CData::AddTranslationState(CTranslationTypeEnum type, WORD id1, WORD id2, WORD state)
{
    TDirectArray<CProgressItem>* states = GetTranslationStates(type);
    if (states == NULL)
        return FALSE;

    int index;
    if (QueryTranslationStateIndex(type, id1, id2, &index))
    {
        DWORD id = MAKELPARAM(id1, id2);
        TRACE_E("CData::AddTranslationState - state id1=" << id1 << " id2=" << id2 << " ID=" << id << " already exists! Overwriting...");
        states->At(index).State = state;
    }

    CProgressItem item;
    item.ID = MAKELPARAM(id1, id2);
    item.State = state;
    BOOL offset = 0;
    if (index >= 0 && index < states->Count && item.ID > states->At(index).ID)
        offset = 1;
    states->Insert(index + offset, item);
    if (!states->IsGood())
    {
        MessageBox(GetMsgParent(), "Out of memory", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

// odhadneme, zda uz je text prelozeny
WORD GuessTranslationState(const wchar_t* oStr, const wchar_t* tStr)
{
    return IsTranslatableControl(tStr) &&
                   wcscmp(oStr, tStr) == 0
               ? PROGRESS_STATE_UNTRANSLATED
               : PROGRESS_STATE_TRANSLATED;
}

BOOL CData::QueryTranslationStateIndex(CTranslationTypeEnum type, WORD id1, WORD id2, int* index)
{
    TDirectArray<CProgressItem>* states = GetTranslationStates(type);
    if (states == NULL)
        return FALSE;

    DWORD id = MAKELPARAM(id1, id2);
    if (states->Count == 0)
    {
        *index = 0;
        return FALSE; // nenalezeno
    }

    int l = 0, r = states->Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = states->At(m).ID - id;
        if (res == 0)
        {
            *index = m;
            return TRUE; // nalezeno
        }
        else
        {
            if (res > 0)
            {
                if (l == r || l > m - 1)
                {
                    *index = m;
                    return FALSE; // nenalezeno
                }
                r = m - 1;
            }
            else
            {
                if (l == r)
                {
                    *index = m;
                    return FALSE; // nenalezeno
                }
                l = m + 1;
            }
        }
    }
}

WORD CData::QueryTranslationState(CTranslationTypeEnum type, WORD id1, WORD id2, const wchar_t* oStr, const wchar_t* tStr)
{
    TDirectArray<CProgressItem>* states = GetTranslationStates(type);
    if (states == NULL)
        return FALSE;

    int index;
    if (QueryTranslationStateIndex(type, id1, id2, &index))
        return states->At(index).State; // nalezeno
    else
        return GuessTranslationState(oStr, tStr); // nenalezeno
}

/*
WORD
CData::QueryTranslationState(WORD id1, WORD id2, const wchar_t *oStr, const wchar_t *tStr)
{
  DWORD id = MAKELPARAM(id1, id2);
  if (TranslationStates.Count == 0)
    return GuessTranslationState(oStr, tStr);  // nenalezeno

  int l = 0, r = TranslationStates.Count - 1, m;
  while (1)
  {
    m = (l + r) / 2;
    int res = TranslationStates[m].ID - id;
    if (res == 0)
    {
      return TranslationStates[m].State; // nalezeno
    }
    else
    {
      if (res > 0)
      {
        if (l == r || l > m - 1) return GuessTranslationState(oStr, tStr); // nenalezeno
        r = m - 1;
      }
      else
      {
        if (l == r) return GuessTranslationState(oStr, tStr); // nenalezeno
        l = m + 1;
      }
    }
  }
}

*/

BOOL CData::IsEnglishProject()
{
    return (Data.SLGSignature.LanguageID == 0x409); // English
}
