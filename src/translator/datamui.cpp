// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "datarh.h"

BOOL PathAppend(char* path, const char* name, int pathSize);

BOOL FileExists(const char* fileName)
{
    DWORD attr = GetFileAttributes(fileName);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

BOOL TrimOnSecondUnderscore(char* masked)
{
    char* p = masked;
    int cnt = 0;
    while (*p != 0)
    {
        if (*p == '_')
        {
            cnt++;
            if (cnt == 2)
            {
                *(p + 1) = '*';
                *(p + 2) = 0;
                return TRUE;
            }
        }
        p++;
    }
    return FALSE;
}

// pokusi se v prelozenem strome zacinajicim na 'translatedMUIRoot' dohledat soubor 'fileName' na podceste 'originalMUIDir'
// pokud takovy soubor nalezne, ulozi jeho cestu do 'translatedFileName' a vrati TRUE
// jinak vrati FALSE
BOOL LookupForTranslatedFile(const char* originalMUIRoot, const char* originalMUISubDir, const char* fileName,
                             const char* translatedMUIRoot, char* translatedFileName)
{
    BOOL ret = FALSE;
    char buff[MAX_PATH];
    lstrcpy(buff, translatedMUIRoot);

    // nazev adesare obsahuje na konci za druhym podtrzitkem nejake cisla, ktera musime "zariznout", potoze jsou ruzna pro kazdou lokalizaci
    char trim[MAX_PATH];
    lstrcpy(trim, originalMUISubDir);
    TrimOnSecondUnderscore(trim);
    PathAppend(buff, trim, MAX_PATH);

    WIN32_FIND_DATA find;
    HANDLE hFind = HANDLES_Q(FindFirstFile(buff, &find));
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        { // hledame prvni level podadresaru
            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (find.cFileName[0] != 0 && strcmp(find.cFileName, ".") != 0 && strcmp(find.cFileName, "..") != 0)
                {
                    char foundFile[MAX_PATH];
                    lstrcpy(foundFile, translatedMUIRoot);
                    PathAppend(foundFile, find.cFileName, MAX_PATH);
                    PathAppend(foundFile, fileName, MAX_PATH);
                    if (FileExists(foundFile))
                    {
                        lstrcpy(translatedFileName, foundFile);
                        ret = TRUE;
                    }
                }
            }
        } while (!ret && FindNextFile(hFind, &find));
        HANDLES(FindClose(hFind));
    }

    return ret;
}

BOOL EnumMUIFiles(CData* data, const char* originalMUIRoot, const char* originalMUISubPath, const char* translatedMUIRoot)
{
    char buff[MAX_PATH];
    lstrcpy(buff, originalMUIRoot);
    PathAppend(buff, originalMUISubPath, MAX_PATH);
    PathAppend(buff, "*.mui", MAX_PATH);

    WIN32_FIND_DATA find;
    HANDLE hFind = HANDLES_Q(FindFirstFile(buff, &find));
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        { // hledame soubory
            if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                if (find.cFileName[0] != 0)
                {
                    char originalFileName[MAX_PATH];
                    lstrcpy(originalFileName, originalMUIRoot);
                    PathAppend(originalFileName, originalMUISubPath, MAX_PATH);
                    PathAppend(originalFileName, find.cFileName, MAX_PATH);

                    char translatedFileName[MAX_PATH];
                    if (LookupForTranslatedFile(originalMUIRoot, originalMUISubPath, find.cFileName, translatedMUIRoot, translatedFileName))
                    {
                        // nacte resourcy z original a translated DLL
                        data->Load(originalFileName, translatedFileName, FALSE);
                    }
                    else
                    {
                        TRACE_I("Ignoring original file " << originalFileName);
                    }
                }
            }
        } while (FindNextFile(hFind, &find));
        HANDLES(FindClose(hFind));
    }

    return TRUE;
}

BOOL EnumMUIDirectories(CData* data, const char* originalMUIRoot, const char* translatedMUIRoot)
{
    char buff[MAX_PATH];
    lstrcpy(buff, originalMUIRoot);
    PathAppend(buff, "*.*", MAX_PATH);

    WIN32_FIND_DATA find;
    HANDLE hFind = HANDLES_Q(FindFirstFile(buff, &find));
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        { // hledame prvni level podadresaru
            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (find.cFileName[0] != 0 && strcmp(find.cFileName, ".") != 0 && strcmp(find.cFileName, "..") != 0)
                {
                    // nasli jsou adresar, ktery jiz muze obsahovat vlastni *.dll.mui DLLka
                    // jdeme se po nich podivat
                    EnumMUIFiles(data, originalMUIRoot, find.cFileName, translatedMUIRoot);
                }
            }
        } while (FindNextFile(hFind, &find));
        HANDLES(FindClose(hFind));
    }
    return TRUE;
}

BOOL CData::LoadMUIPackages(const char* originalMUI, const char* translatedMUI)
{
    // sestrelime existujici data
    StrData.DestroyMembers();
    MenuData.DestroyMembers();
    DlgData.DestroyMembers();
    CleanTranslationStates();
    SalMenuSections.DestroyMembers();
    IgnoreLstItems.DestroyMembers();
    CheckLstItems.DestroyMembers();
    DataRH.Clean();

    MUIMode = TRUE;
    MUIDialogID = 1; // reset counteru pro unikatni ID
    MUIMenuID = 1;
    MUIStringID = 1;

    EnumMUIDirectories(this, originalMUI, translatedMUI);

    return TRUE;
}
