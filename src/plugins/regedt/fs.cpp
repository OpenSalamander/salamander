// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// image-list pro jednoduche ikony FS
HIMAGELIST ImageList = NULL;

const char* Str_REG_BINARY = "BINARY";
const char* Str_REG_DWORD = "DWORD";
const char* Str_REG_DWORD_BIG_ENDIAN = "DWORD_BIG_ENDIAN";
const char* Str_REG_EXPAND_SZ = "EXPAND_SZ";
const char* Str_REG_LINK = "LINK";
const char* Str_REG_MULTI_SZ = "MULTI_SZ";
const char* Str_REG_NONE = "NONE";
const char* Str_REG_QWORD = "QWORD";
const char* Str_REG_RESOURCE_LIST = "RESOURCE_LIST";
const char* Str_REG_SZ = "SZ";
const char* Str_REG_FULL_RESOURCE_DESCRIPTOR = "FULL_RESOURCE_DESCRIPTOR";
const char* Str_REG_RESOURCE_REQUIREMENTS_LIST = "RESOURCE_REQUIREMENTS_LIST";

int Len_REG_BINARY;
int Len_REG_DWORD;
int Len_REG_DWORD_BIG_ENDIAN;
int Len_REG_EXPAND_SZ;
int Len_REG_LINK;
int Len_REG_MULTI_SZ;
int Len_REG_NONE;
int Len_REG_QWORD;
int Len_REG_RESOURCE_LIST;
int Len_REG_SZ;
int Len_REG_FULL_RESOURCE_DESCRIPTOR;
int Len_REG_RESOURCE_REQUIREMENTS_LIST;

// pro combo do NewKeyDialogu
CRegTypeText RegTypeTexts[] =
    {
        {Str_REG_SZ, REG_SZ, 1, 1},
        {Str_REG_DWORD, REG_DWORD, 1, 1},
        {Str_REG_BINARY, REG_BINARY, 1, 0},
        {Str_REG_EXPAND_SZ, REG_EXPAND_SZ, 1, 1},
        {Str_REG_MULTI_SZ, REG_MULTI_SZ, 1, 0},
        {Str_REG_QWORD, REG_QWORD, 1, 1},
        {Str_REG_RESOURCE_LIST, REG_RESOURCE_LIST, 1, 0},
        {Str_REG_RESOURCE_REQUIREMENTS_LIST, REG_RESOURCE_REQUIREMENTS_LIST, 1, 0},
        {Str_REG_FULL_RESOURCE_DESCRIPTOR, REG_FULL_RESOURCE_DESCRIPTOR, 1, 0},
        {Str_REG_DWORD_BIG_ENDIAN, REG_DWORD_BIG_ENDIAN, 1, 1},
        {Str_REG_LINK, REG_LINK, 0, 0},
        {Str_REG_NONE, REG_NONE, 0, 0},
        {NULL, 0, 0, 0}};

char KeyText[10];
int KeyTextLen = 10;

CPredefinedHKey PredefinedHKeys[] =
    {
        HKEY_CLASSES_ROOT, L"HKEY_CLASSES_ROOT",
        HKEY_CURRENT_CONFIG, L"HKEY_CURRENT_CONFIG",
        HKEY_CURRENT_USER, L"HKEY_CURRENT_USER",
        HKEY_DYN_DATA, L"HKEY_DYN_DATA",
        HKEY_LOCAL_MACHINE, L"HKEY_LOCAL_MACHINE",
        //HKEY_PERFORMANCE_DATA, L"HKEY_PERFORMANCE_DATA",
        //HKEY_PERFORMANCE_NLSTEXT, L"HKEY_PERFORMANCE_NLSTEXT",
        //HKEY_PERFORMANCE_TEXT, L"HKEY_PERFORMANCE_TEXT",
        HKEY_USERS, L"HKEY_USERS",
        NULL, NULL};

WCHAR RecentFullPath[MAX_FULL_KEYNAME];

BOOL InitFS()
{
    CALL_STACK_MESSAGE1("InitFS()");
    ImageList = ImageList_Create(16, 16, ILC_MASK | SG->GetImageListColorFlags(), 4, 0);
    if (ImageList == NULL)
    {
        TRACE_E("Nepodarilo se vytvorit image list.");
        return FALSE;
    }
    ImageList_SetImageCount(ImageList, 4); // inicializace
    ImageList_SetBkColor(ImageList, SG->GetCurrentColor(SALCOL_ITEM_BK_NORMAL));

    // ikonu adresare ziskame ze Salamandera
    HICON hIcon = SG->GetSalamanderIcon(SALICON_DIRECTORY, SALICONSIZE_16);
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(ImageList, 0, hIcon);
        DestroyIcon(hIcon);
    }

    // ostatni ikonky jsou hard-coded
    ImageList_ReplaceIcon(ImageList, 1, LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_SZ)));
    ImageList_ReplaceIcon(ImageList, 2, LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_BIN)));
    ImageList_ReplaceIcon(ImageList, 3, LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_UNKNOWN)));

    Len_REG_BINARY = (int)strlen(Str_REG_BINARY);
    Len_REG_DWORD = (int)strlen(Str_REG_DWORD);
    Len_REG_DWORD_BIG_ENDIAN = (int)strlen(Str_REG_DWORD_BIG_ENDIAN);
    Len_REG_EXPAND_SZ = (int)strlen(Str_REG_EXPAND_SZ);
    Len_REG_LINK = (int)strlen(Str_REG_LINK);
    Len_REG_MULTI_SZ = (int)strlen(Str_REG_MULTI_SZ);
    Len_REG_NONE = (int)strlen(Str_REG_NONE);
    Len_REG_QWORD = (int)strlen(Str_REG_QWORD);
    Len_REG_RESOURCE_LIST = (int)strlen(Str_REG_RESOURCE_LIST);
    Len_REG_SZ = (int)strlen(Str_REG_SZ);
    Len_REG_FULL_RESOURCE_DESCRIPTOR = (int)strlen(Str_REG_FULL_RESOURCE_DESCRIPTOR);
    Len_REG_RESOURCE_REQUIREMENTS_LIST = (int)strlen(Str_REG_RESOURCE_REQUIREMENTS_LIST);
    lstrcpyn(KeyText, LoadStr(IDS_KEY), 10);
    KeyTextLen = (int)strlen(KeyText);

    return TRUE;
}

void ReleaseFS()
{
    CALL_STACK_MESSAGE1("ReleaseFS()");
    if (ImageList)
        ImageList_Destroy(ImageList);
}

//****************************************************************************
//
// CTopIndexMem
//

void CTopIndexMem::Push(LPCWSTR path, int topIndex)
{
    CALL_STACK_MESSAGE2("CTopIndexMem::Push(, %d)", topIndex);
    // zjistime, jestli path navazuje na Path (path==Path+"\\jmeno")
    LPCWSTR s = path + wcslen(path);
    if (s > path && *(s - 1) == L'\\')
        s--;
    BOOL ok;
    if (s == path)
        ok = FALSE;
    else
    {
        if (s > path && *s == L'\\')
            s--;
        while (s > path && *s != L'\\')
            s--;

        int l = (int)wcslen(Path);
        if (l > 0 && Path[l - 1] == L'\\')
            l--;
        ok = s - path == l &&
             CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, path, l, Path, l) == CSTR_EQUAL;
    }

    if (ok) // navazuje -> zapamatujeme si dalsi top-index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // je potreba vyhodit z pameti prvni top-index
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        wcscpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // nenavazuje -> prvni top-index v rade
    {
        wcscpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(LPCWSTR path, int& topIndex)
{
    CALL_STACK_MESSAGE2("CTopIndexMem::FindAndPop(, %d)", topIndex);
    // zjistime, jestli path odpovida Path (path==Path)
    int l1 = (int)wcslen(path);
    if (l1 > 0 && path[l1 - 1] == L'\\')
        l1--;
    int l2 = (int)wcslen(Path);
    if (l2 > 0 && Path[l2 - 1] == L'\\')
        l2--;
    if (l1 == l2 &&
        CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, path, l1, Path, l1) == CSTR_EQUAL)
    {
        if (TopIndexesCount > 0)
        {
            LPWSTR s = Path + wcslen(Path);
            if (s > Path && *(s - 1) == L'\\')
                s--;
            if (s > Path && *s == L'\\')
                s--;
            while (s > Path && *s != L'\\')
                s--;
            *s = L'\0';
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // tuto hodnotu jiz nemame (nebyla ulozena nebo mala pamet->byla vyhozena)
        {
            Clear();
            return FALSE;
        }
    }
    else // dotaz na jinou cestu -> vycistime pamet, doslo k dlouhemu skoku
    {
        Clear();
        return FALSE;
    }
}

// ****************************************************************************
//
// CPluginInterfaceForFS
//
//

CPluginFSInterfaceAbstract*
CPluginInterfaceForFS::OpenFS(const char* fsName, int fsNameIndex)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForFS::OpenFS(%s, %d)", fsName, fsNameIndex);
    CPluginFSInterface* ret = new CPluginFSInterface();
    if (ret->IsGood())
        return ret;
    delete ret;
    return NULL;
}

void CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForFS::CloseFS()");
    if (fs != NULL)
        delete (CPluginFSInterface*)fs;
}

void CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);
    // zmenime cestu v aktualnim panelu na AssignedFSName:ConnectPath
    SG->ChangePanelPathToPluginFS(panel, AssignedFSName, "", NULL);
}

void EditValue(int root, LPWSTR key, LPWSTR name, BOOL rawEdit)
{
    CALL_STACK_MESSAGE3("EditValue(%d, , , %d)", root, rawEdit);
    if (root == -1)
        return;

    HKEY hKey;
    int ret = RegOpenKeyExW(PredefinedHKeys[root].HKey, key,
                            0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
    if (ret != ERROR_SUCCESS)
    {
        ErrorL(ret, IDS_QUERYVAL);
        return;
    }

    DWORD type;
    LPBYTE data = NULL;
    DWORD size;

    if (name)
    {
        // zjistime velikost dat
        ret = RegQueryValueExW(hKey, name, 0, &type, NULL, &size);
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            ErrorL(ret, IDS_QUERYVAL);
            return;
        }

        if (!rawEdit)
        {
            // otestujeme, zda nejsou SZ data moc velika
            if ((type == REG_SZ || type == REG_EXPAND_SZ) && size / 2 > 0x7FFFFFFE)
            {
                RegCloseKey(hKey);
                Error(IDS_LONGDATA);
                return;
            }
        }

        // allokujeme buffer na data
        data = (LPBYTE)malloc(size);
        if (!data)
        {
            RegCloseKey(hKey);
            Error(IDS_LOWMEM);
            return;
        }

        // nacteme data
        ret = RegQueryValueExW(hKey, name, 0, &type, data, &size);
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            ErrorL(ret, IDS_QUERYVAL);
            free(data);
            return;
        }
    }
    else
    {
        type = REG_SZ;
        size = 0;
    }

    if (!rawEdit)
    {
        // overime spravnost dat
        switch (type)
        {
        case REG_DWORD_BIG_ENDIAN:
        case REG_DWORD:
            if (size != 4)
            {
                if (size < 4)
                {
                    data = (LPBYTE)realloc(data, 4);
                    if (!data)
                    {
                        RegCloseKey(hKey);
                        Error(IDS_LOWMEM);
                        return;
                    }
                }
                *(DWORD*)data = 0;
            }
            size = 4;
            break;

        case REG_QWORD:
            if (size != 8)
            {
                if (size < 8)
                {
                    data = (LPBYTE)realloc(data, 8);
                    if (!data)
                    {
                        RegCloseKey(hKey);
                        Error(IDS_LOWMEM);
                        return;
                    }
                }
                if (size == 4)
                    *(LPQWORD)data &= 0xFFFFFFFF;
                else
                    *(LPQWORD)data = 0;
            }
            size = 8;
            break;

        case REG_SZ:
        case REG_EXPAND_SZ:
            if (size < 2)
            {
                data = (LPBYTE)realloc(data, 4);
                if (!data)
                {
                    RegCloseKey(hKey);
                    Error(IDS_LOWMEM);
                    return;
                }
                *(LPWSTR)data = L'\0';
            }
            break;

        default: // zatim neumime primo, ale pouze pres externi editor
            rawEdit = TRUE;
        }
    }

    if (rawEdit)
    {
        ret = (int)CRawEditValDialog(SG->GetMainWindowHWND(),
                                     name != NULL && *name != '\0' ? name : LoadStrW(IDS_DEFAULTVALUE),
                                     &type, data, size, name == NULL || *name == '\0')
                  .Execute();
    }
    else
    {
        ret = (int)CEditValDialog(SG->GetMainWindowHWND(),
                                  name != NULL && *name != '\0' ? name : LoadStrW(IDS_DEFAULTVALUE),
                                  &type, data, size, name == NULL || *name == '\0')
                  .Execute();
    }
    if (ret != IDOK)
    {
        if (data)
            free(data);
        RegCloseKey(hKey);
        return;
    }

    ret = RegSetValueExW(hKey, name, 0, type, data, size);

    if (data)
        free(data);
    RegCloseKey(hKey);

    if (ret != ERROR_SUCCESS)
    {
        ErrorL(ret, IDS_SETVAL);
        return;
    }
}

void CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                        const char* pluginFSName, int pluginFSNameIndex,
                                        CFileData& file, int isDir)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForFS::ExecuteOnFS(%d, , %s, %d, , %d)", panel,
                        pluginFSName, pluginFSNameIndex, isDir);
    CPluginFSInterface* fs = (CPluginFSInterface*)pluginFS;
    CPluginData* pluginData = (CPluginData*)file.PluginData;

    if (isDir)
    {
        WCHAR newPath[MAX_FULL_KEYNAME];
        WCHAR cutDir[MAX_KEYNAME];
        char cutDirA[MAX_KEYNAME];
        char* focus = NULL;

        fs->GetCurrentPathW(newPath, MAX_FULL_KEYNAME);
        if (isDir == 2) // up-dir
        {
            if (CutDirectory(newPath, cutDir, MAX_KEYNAME)) // zkratime cestu o posl. komponentu
            {
                if (WStrToStr(cutDirA, MAX_KEYNAME, cutDir) > 0)
                    focus = cutDirA;
                // zmenime cestu v panelu
                if (fs->SetNewPath(newPath))
                {
                    int topIndex; // pristi top-index, -1 -> neplatny
                    if (!fs->TopIndexMem.FindAndPop(newPath, topIndex))
                        topIndex = -1;

                    SG->ChangePanelPathToPluginFS(panel, pluginFSName, "?", NULL,
                                                  topIndex, focus);
                }
            }
        }
        else // podadresar
        {
            // zaloha udaju pro TopIndexMem (backupPath + topIndex)
            WCHAR backupPath[MAX_FULL_KEYNAME];
            wcscpy(backupPath, newPath);
            int topIndex = SG->GetPanelTopIndex(panel);

            if (PathAppend(newPath, pluginData->Name, MAX_FULL_KEYNAME)) // nastavime cestu
            {
                // zmenime cestu v panelu
                if (fs->SetNewPath(newPath))
                {
                    if (SG->ChangePanelPathToPluginFS(panel, pluginFSName, "?"))
                    {
                        fs->TopIndexMem.Push(backupPath, topIndex); // zapamatujeme top-index pro navrat
                    }
                }
            }
        }
    }
    else
        EditValue(fs->CurrentKeyRoot, fs->CurrentKeyName, pluginData->Name, FALSE);
}

BOOL WINAPI
CPluginInterfaceForFS::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForFS::DisconnectFS(, %d, %d, , %s, %d)",
                        isInPanel, panel, pluginFSName, pluginFSNameIndex);
    BOOL ret = FALSE;
    if (isInPanel)
    {
        SG->DisconnectFSFromPanel(parent, panel);
        ret = SG->GetPanelPluginFS(panel) != pluginFS;
    }
    else
    {
        ret = SG->CloseDetachedFS(parent, pluginFS);
    }
    return ret;
}
