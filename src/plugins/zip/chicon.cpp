// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>

#ifdef ZIP_DLL
#include <ostream>
#include "spl_base.h"
#include "dbg.h"
#else //ZIP_DLL
#include "killdbg.h"
#endif //ZIP_DLL

#include "array2.h"
#include "chicon.h"
#include "resedit.h"
#include "selfextr\\comdefs.h"

CIcon* LoadIconsFromDirectory(HINSTANCE module, LPICONDIR directory, BOOL isIco);
LPICONDIR LoadIconDirectoryByResName(HINSTANCE module, LPTSTR lpszName);
LPICONDIR LoadIconDirectoryFromEXE(HINSTANCE module, DWORD index);
LPVOID LoadIconDataFromEXE(HINSTANCE module, DWORD id);
LPICONDIR LoadIconDirectoryFromICO(HANDLE icoFile);
LPVOID LoadIconDataFromICO(HANDLE icoFile, DWORD offset, DWORD size);
void LoadIDsList(HINSTANCE module, TIndirectArray2<TCHAR>& IDsArray);
WORD FindNextUnusedID(WORD IDVal, TIndirectArray2<TCHAR>& IDsArray);

BOOL Read(HANDLE file, void* buffer, DWORD size);

BOOL ChangeSfxIconAndAddManifest(const char* sfxFile, CIcon* icons, int iconsCount, LPVOID manifest, DWORD manifestSize)
{
    CALL_STACK_MESSAGE3("ChangeSfxIcon(%s, , %d)", sfxFile, iconsCount);
    if (!icons)
        return FALSE;

    TIndirectArray2<TCHAR> IDsArray(16, FALSE);
    BOOL ret = FALSE;

    HINSTANCE module = LoadLibraryEx(sfxFile, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (module)
    {
        LoadIDsList(module, IDsArray);
        FreeLibrary(module);

        DWORD size = sizeof(MEMICONDIR) + (iconsCount - 1) * sizeof(MEMICONDIRENTRY);
        LPMEMICONDIR dir = (LPMEMICONDIR)malloc(size);
        if (dir)
        {
            CResEdit re;
            if (re.BeginUpdateResource(sfxFile, FALSE))
            {
                WORD id = 0;
                dir->idReserved = 0;
                dir->idType = 1;
                dir->idCount = iconsCount;
                int i;
                for (i = 0; i < iconsCount; i++)
                {
                    dir->idEntries[i].bWidth = icons[i].bWidth;
                    dir->idEntries[i].bHeight = icons[i].bHeight;
                    dir->idEntries[i].bColorCount = icons[i].bColorCount;
                    dir->idEntries[i].bReserved = icons[i].bReserved;
                    dir->idEntries[i].wPlanes = icons[i].wPlanes;
                    dir->idEntries[i].wBitCount = icons[i].wBitCount;
                    dir->idEntries[i].dwBytesInRes = icons[i].dwBytesInRes;
                    dir->idEntries[i].nID = id = FindNextUnusedID(id, IDsArray);
                    if (!re.UpdateResource(RT_ICON, MAKEINTRESOURCE(id), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                                           icons[i].lpData, icons[i].dwBytesInRes))
                        break;
                }
                if (i == iconsCount &&
                    re.UpdateResource(RT_GROUP_ICON, MAKEINTRESOURCE(SE_IDI_ICON),
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), dir, size) &&
                    re.UpdateResource(RT_MANIFEST,
                                      CREATEPROCESS_MANIFEST_RESOURCE_ID,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), manifest, manifestSize))
                {
                    if (re.EndUpdateResource(FALSE))
                        ret = TRUE; //sucess
                }
                else
                    re.EndUpdateResource(TRUE);
            }
            free(dir);
        }
    }

    return ret;
}

BOOL HasExtension(const char* filename, const char* extension)
{
    CALL_STACK_MESSAGE_NONE
    const char* dot = strrchr(filename, '.');
    if (dot != NULL) // ".cvspass" is extension in Windows
    {
        if (lstrcmpi(++dot, extension) == 0)
            return TRUE;
    }
    return FALSE;
}

//**********************************
//
// return values:
//   0  success
//   1  error openning file
//   2  module is not valid win32 application
//   3  unable to load module containing icon data
//   4  unable to load icon from file

int LoadIcons(const char* iconFile, DWORD index, CIcon** icons, int* count)
{
    CALL_STACK_MESSAGE3("LoadIcons(%s, 0x%X, , )", iconFile, index);
    HINSTANCE module;
    LPICONDIR directory;
    BOOL isIco = HasExtension(iconFile, "ico");

    if (isIco)
    {
        module = (HINSTANCE)CreateFile(iconFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
        if (module == INVALID_HANDLE_VALUE)
            return 1;

        directory = LoadIconDirectoryFromICO((HANDLE)module);
    }
    else
    {
        module = (HINSTANCE)LoadLibraryEx(iconFile, NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (!module)
        {
            int err = GetLastError();
            if (err == 193)
            {
                SetLastError(0);
                return 2;
                //return IDS_ERRLOADLIB;
            }
            return 3;
            //return IDS_ERRLOADLIB2;
        }
        if ((int)index < 0)
            directory = LoadIconDirectoryByResName(module, MAKEINTRESOURCE(-(int)index));
        else
            directory = LoadIconDirectoryFromEXE(module, index);
    }
    //int ret = IDS_ERRLOADICON;
    int ret = 4;
    if (directory)
    {
        *icons = LoadIconsFromDirectory(module, directory, isIco);
        if (*icons)
        {
            *count = directory->idCount;
            ret = 0;
        }
        free(directory);
    }
    if (isIco)
        CloseHandle((HANDLE)module);
    else
        FreeLibrary(module);
    SetLastError(0);
    return ret;
}

void DestroyIcons(CIcon* icons, int count)
{
    CALL_STACK_MESSAGE2("DestroyIcons(, %d)", count);
    int i;
    for (i = 0; i < count; i++)
    {
        free(icons[i].lpData);
    }
    free(icons);
}

CIcon* LoadIconsFromDirectory(HINSTANCE module, LPICONDIR directory, BOOL isIco)
{
    CALL_STACK_MESSAGE2("LoadIconsFromDirectory(, , %d)", isIco);
    CIcon* icons = (CIcon*)malloc(directory->idCount * sizeof(CIcon));
    if (!icons)
        return NULL;
    int i;
    for (i = 0; i < directory->idCount; i++)
    {
        if (isIco)
            icons[i].lpData = LoadIconDataFromICO((HANDLE)module, directory->idEntries[i].dwImageOffset, directory->idEntries[i].dwBytesInRes);
        else
            icons[i].lpData = LoadIconDataFromEXE(module, directory->idEntries[i].dwImageOffset /*it is ID*/);
        if (!icons[i].lpData)
        {
            DestroyIcons(icons, i);
            return NULL;
        }
        icons[i].bWidth = directory->idEntries[i].bWidth;
        icons[i].bHeight = directory->idEntries[i].bHeight;
        icons[i].bColorCount = directory->idEntries[i].bColorCount;
        icons[i].bReserved = directory->idEntries[i].bReserved;
        icons[i].wPlanes = directory->idEntries[i].wPlanes;
        icons[i].wBitCount = directory->idEntries[i].wBitCount;
        icons[i].dwBytesInRes = directory->idEntries[i].dwBytesInRes;
    }
    return icons;
}

LPICONDIR LoadIconDirectoryByResName(HINSTANCE module, LPTSTR lpszName)
{
    CALL_STACK_MESSAGE1("LoadIconDirectoryByResName(, )");
    LPMEMICONDIR memDir = NULL;
    LPICONDIR dir;
    HRSRC hRsrc = FindResource(module, lpszName, RT_GROUP_ICON);
    if (hRsrc)
    {
        HGLOBAL hGlobal = LoadResource(module, hRsrc);
        if (hGlobal)
        {
            memDir = (LPMEMICONDIR)LockResource(hGlobal);
            if (memDir)
            {
                dir = (LPICONDIR)malloc(sizeof(ICONDIR) + (memDir->idCount - 1) * sizeof(ICONDIRENTRY));
                if (dir)
                {
                    dir->idReserved = memDir->idReserved;
                    dir->idType = memDir->idType;
                    dir->idCount = memDir->idCount;
                    int i;
                    for (i = 0; i < memDir->idCount; i++)
                    {
                        dir->idEntries[i].bWidth = memDir->idEntries[i].bWidth;
                        dir->idEntries[i].bHeight = memDir->idEntries[i].bHeight;
                        dir->idEntries[i].bColorCount = memDir->idEntries[i].bColorCount;
                        dir->idEntries[i].bReserved = memDir->idEntries[i].bReserved;
                        dir->idEntries[i].wPlanes = memDir->idEntries[i].wPlanes;
                        dir->idEntries[i].wBitCount = memDir->idEntries[i].wBitCount;
                        dir->idEntries[i].dwBytesInRes = memDir->idEntries[i].dwBytesInRes;
                        dir->idEntries[i].dwImageOffset = memDir->idEntries[i].nID;
                    }
                }
            }
        }
    }
    return dir;
}

typedef struct
{
    DWORD WantIndex;
    DWORD CurrentIndex;
    LPICONDIR Directory;
} CEnumIconDirsData;

BOOL CALLBACK EnumIconDirectories(HINSTANCE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
    CALL_STACK_MESSAGE_NONE
    CEnumIconDirsData* data = (CEnumIconDirsData*)lParam;
    if (data->WantIndex == data->CurrentIndex)
    {
        data->Directory = LoadIconDirectoryByResName(hModule, lpszName);
        return FALSE;
    }
    data->CurrentIndex++;
    return TRUE;
}

LPICONDIR LoadIconDirectoryFromEXE(HINSTANCE module, DWORD index)
{
    CALL_STACK_MESSAGE2("LoadIconDirectoryFromEXE(, 0x%X)", index);
    CEnumIconDirsData data;
    data.WantIndex = index;
    data.CurrentIndex = 0;
    data.Directory = NULL;
    EnumResourceNames(module, RT_GROUP_ICON, EnumIconDirectories, (LONG_PTR)&data);
    return data.Directory;
}

LPVOID LoadIconDataFromEXE(HINSTANCE module, DWORD id)
{
    CALL_STACK_MESSAGE2("LoadIconDataFromEXE(, 0x%X)", id);
    LPVOID data = NULL;

    HRSRC hRsrc = FindResource(module, MAKEINTRESOURCE(id), RT_ICON);
    if (hRsrc)
    {
        HGLOBAL hGlobal = LoadResource(module, hRsrc);
        if (hGlobal)
        {
            LPVOID icon = LockResource(hGlobal);
            if (icon)
            {
                DWORD s = SizeofResource(module, hRsrc);
                data = malloc(s);
                if (data)
                    memcpy(data, icon, s);
            }
        }
    }
    return data;
}

LPICONDIR LoadIconDirectoryFromICO(HANDLE icoFile)
{
    CALL_STACK_MESSAGE1("LoadIconDirectoryFromICO()");
    WORD w;

    //read reserved
    if (!Read(icoFile, &w, sizeof(WORD)))
        return NULL;

    //must be 0
    if (w != 0)
        return NULL;

    //read type
    if (!Read(icoFile, &w, sizeof(WORD)))
        return NULL;
    //must be 1 for icons
    if (w != 1)
        return 0;

    //read count
    if (!Read(icoFile, &w, sizeof(WORD)))
        return NULL;

    LPICONDIR dir = (LPICONDIR)malloc(sizeof(ICONDIR) + (w - 1) * sizeof(ICONDIRENTRY));
    if (dir)
    {
        dir->idReserved = 0;
        dir->idType = 1;
        dir->idCount = w;
        if (!Read(icoFile, &dir->idEntries, dir->idCount * sizeof(ICONDIRENTRY)))
        {
            free(dir);
            return NULL;
        }
    }
    return dir;
}

LPVOID LoadIconDataFromICO(HANDLE icoFile, DWORD offset, DWORD size)
{
    CALL_STACK_MESSAGE3("LoadIconDataFromICO(, 0x%X, 0x%X)", offset, size);
    LONG dummy;
    dummy = 0;
    if (SetFilePointer(icoFile, offset, &dummy, FILE_BEGIN) == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        return FALSE;

    LPVOID data = malloc(size);
    if (data)
    {
        if (!Read(icoFile, data, size))
        {
            free(data);
            return NULL;
        }
    }
    return data;
}

BOOL CALLBACK EnumResNames(HINSTANCE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
    CALL_STACK_MESSAGE_NONE
    TIndirectArray2<TCHAR>* IDsArray = (TIndirectArray2<TCHAR>*)lParam;
    if (((DWORD_PTR)lpszName & ~(DWORD_PTR)0xFFFF) == 0)
    {
        IDsArray->Add(lpszName);
    }
    return TRUE;
}

BOOL CALLBACK EnumResTypes(HINSTANCE hModule, LPTSTR lpszType, LONG_PTR lParam)
{
    CALL_STACK_MESSAGE2("EnumResTypes(, , 0x%IX)", lParam);
    EnumResourceNames(hModule, lpszType, EnumResNames, lParam);
    return TRUE;
}

void QuickSortIDsArray(int left, int right, TIndirectArray2<TCHAR>& IDsArray)
{
    CALL_STACK_MESSAGE_NONE
    int i = left, j = right;
    WORD pivot = (WORD)(DWORD_PTR)IDsArray[(i + j) / 2];
    do
    {
        while ((WORD)(DWORD_PTR)IDsArray[i] <= pivot && i < right)
            i++;
        while (pivot < (WORD)(DWORD_PTR)IDsArray[j] && j > left)
            j--;
        if (i <= j)
        {
            LPTSTR tmp = IDsArray[i];
            IDsArray[i] = IDsArray[j];
            IDsArray[j] = tmp;
            i++;
            j--;
        }
    } while (i <= j); //musej bejt shodny?
    if (left < j)
        QuickSortIDsArray(left, j, IDsArray);
    if (i < right)
        QuickSortIDsArray(i, right, IDsArray);
}

void LoadIDsList(HINSTANCE module, TIndirectArray2<TCHAR>& IDsArray)
{
    CALL_STACK_MESSAGE1("LoadIDsList(, )");
    EnumResourceTypes(module, EnumResTypes, (LONG_PTR)&IDsArray);
    if (IDsArray.Count)
    {
        QuickSortIDsArray(0, IDsArray.Count - 1, IDsArray);
    }
}

WORD FindNextUnusedID(WORD IDVal, TIndirectArray2<TCHAR>& IDsArray)
{
    CALL_STACK_MESSAGE2("FindNextUnusedID(0x%X, )", IDVal);
    IDVal++;
    int i;
    for (i = 0; i < IDsArray.Count; i++)
    {
        if (IDVal == (WORD)(DWORD_PTR)IDsArray[i])
            IDVal++;
        else
        {
            if (IDVal < (WORD)(DWORD_PTR)IDsArray[i])
                break;
        }
    }
    return IDVal;
}

/*
LPICONDIRENTRY CZipPack::FindBestIcon(LPICONDIR directory)
{
  LPICONDIRENTRY best = directory->idEntries;
  LPICONDIRENTRY ptr = directory->idEntries;

  for ( ; ptr < directory->idEntries + directory->idCount; ptr++)
  {
    if (best->bWidth == 32)
    {
      if (ptr->bWidth == 32 && ptr->wBitCount > best->wBitCount) best = ptr;
    }
    else
    {
      if (ptr->bWidth == 32) best = ptr;
      else
      {
        if (ptr->bWidth > best->bWidth || 
            ptr->bWidth == best->bWidth && ptr->wBitCount > best->wBitCount) best = ptr;
      }
    }
  }
  return best;
}
*/
