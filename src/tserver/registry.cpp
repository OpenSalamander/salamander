// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <crtdbg.h>
#include <ostream>
#include <limits.h>
#include <stdio.h>
#include <commctrl.h> // potrebuju LPCOLORMAP

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"

#include "registry.h"

CRegBOOL RegBOOL;
CRegWORD RegWORD;
CRegDWORD RegDWORD;
CRegInt RegInt;
CRegDouble RegDouble;
CRegCOLORREF RegCOLORREF;
CRegWindowPlacement RegWindowPlacement;
CRegLogFont RegLogFont;

//CRegIndirectArray<int> RegIndirectArray;

const WCHAR REGERR_LOWMEMORY[] = L"Low memory";
const WCHAR REGERR_BADTYPE[] = L"Error reading configuration.\nUnexpected type of value: %s\nRequired type: %d\nExisting type: %d";
const WCHAR REGERR_LOADITEM[] = L"Error reading value from registry:\n%s\\%s\\%s";
const WCHAR REGERR_SAVEITEM[] = L"Error writing value to registry:\n%s\\%s\\%s";
const WCHAR REGERR_OPENKEY[] = L"Error opening key in Registry:\n%s\\%s\n\nError: %s";
const WCHAR REGERR_CREATEKEY[] = L"Error creating key in Registry:\n%s\\%s\n\nError: %s";

const WCHAR REGERR_HKEY_LOCAL_MACHINE[] = L"HKEY_LOCAL_MACHINE";
const WCHAR REGERR_HKEY_USERS[] = L"HKEY_USERS";
const WCHAR REGERR_HKEY_CLASSES_ROOT[] = L"HKEY_CLASSES_ROOT";
const WCHAR REGERR_HKEY_CURRENT_USER[] = L"HKEY_CURRENT_USER";
const WCHAR REGERR_HKEY_UNKNOWN[] = L"?";

//*****************************************************************************
//
// GetHKeyName
//

const WCHAR*
GetHKeyName(HKEY hKey)
{
    if (hKey == HKEY_LOCAL_MACHINE)
        return REGERR_HKEY_LOCAL_MACHINE;
    if (hKey == HKEY_USERS)
        return REGERR_HKEY_USERS;
    if (hKey == HKEY_CLASSES_ROOT)
        return REGERR_HKEY_CLASSES_ROOT;
    if (hKey == HKEY_CURRENT_USER)
        return REGERR_HKEY_CURRENT_USER;
    return REGERR_HKEY_UNKNOWN;
}

//*****************************************************************************
//
// CReg
//

BOOL CReg::SaveVoid(HKEY hKey, const WCHAR* name, DWORD type, const void* data, DWORD dataSize)
{
    LONG ret = RegSetValueEx(hKey, name, 0, type, (const BYTE*)data, dataSize);
    if (ret != ERROR_SUCCESS)
    {
        MESSAGE_EW(NULL, errW(ret), MB_OK);
        return FALSE;
    }
    return TRUE;
}

BOOL CReg::LoadVoid(HKEY hKey, const WCHAR* name, DWORD type, void* data, DWORD dataSize, BOOL* notFound)
{
    if (notFound != NULL)
        *notFound = FALSE;
    DWORD gettedType;
    LONG ret = RegQueryValueEx(hKey, name, NULL, &gettedType, (BYTE*)data, &dataSize);
    if (ret != ERROR_SUCCESS)
    {
        if (ret != ERROR_FILE_NOT_FOUND)
        {
            MESSAGE_EW(NULL, errW(ret), MB_OK);
        }
        else
        {
            if (notFound != NULL)
                *notFound = TRUE;
        }
        return FALSE;
    }
    if (gettedType != type)
    {
        MESSAGE_EW(NULL, spfW(REGERR_BADTYPE, name, type, gettedType), MB_OK);
        return FALSE;
    }
    return TRUE;
}

BOOL CReg::GetSize(HKEY hKey, const WCHAR* name, DWORD type, DWORD& dataSize, BOOL* notFound)
{
    if (notFound != NULL)
        *notFound = FALSE;
    DWORD gettedType;
    LONG ret = RegQueryValueEx(hKey, name, NULL, &gettedType, NULL, &dataSize);
    if (ret != ERROR_SUCCESS)
    {
        if (ret != ERROR_FILE_NOT_FOUND)
        {
            MESSAGE_EW(NULL, errW(ret), MB_OK);
        }
        else
        {
            if (notFound != NULL)
                *notFound = TRUE;
        }
        return FALSE;
    }
    if (gettedType != type)
    {
        MESSAGE_EW(NULL, spfW(REGERR_BADTYPE, name, type, gettedType), MB_OK);
        return FALSE;
    }
    return TRUE;
}

//*****************************************************************************
//
// CRegBOOL
//

BOOL CRegBOOL::Save(HKEY hKey, const WCHAR* name, void* data)
{
    const WCHAR* val = *(BOOL*)data ? L"True" : L"False";
    return SaveVoid(hKey, name, REG_SZ, val, sizeof(WCHAR) * (wcslen(val) + 1));
}

BOOL CRegBOOL::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    WCHAR buff[6];
    if (!LoadVoid(hKey, name, REG_SZ, buff, sizeof(buff), notFound))
        return FALSE;
    *(BOOL*)data = (buff[0] == L'T' || buff[0] == L't');
    return TRUE;
}

//*****************************************************************************
//
// CRegWORD
//

BOOL CRegWORD::Save(HKEY hKey, const WCHAR* name, void* data)
{
    DWORD d = *(WORD*)data;
    return SaveVoid(hKey, name, REG_DWORD, &d, sizeof(DWORD));
}

BOOL CRegWORD::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    DWORD d;
    BOOL ret = LoadVoid(hKey, name, REG_DWORD, &d, sizeof(DWORD), notFound);
    if (ret)
        *(WORD*)data = (WORD)(d & 0xFFFF);
    return ret;
}

//*****************************************************************************
//
// CRegDWORD
//

BOOL CRegDWORD::Save(HKEY hKey, const WCHAR* name, void* data)
{
    return SaveVoid(hKey, name, REG_DWORD, data, sizeof(DWORD));
}

BOOL CRegDWORD::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    return LoadVoid(hKey, name, REG_DWORD, data, sizeof(DWORD), notFound);
}

//*****************************************************************************
//
// CRegInt
//

BOOL CRegInt::Save(HKEY hKey, const WCHAR* name, void* data)
{
    WCHAR buff[20];
    swprintf_s(buff, L"%d", *(int*)data);
    return SaveVoid(hKey, name, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
}

BOOL CRegInt::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    WCHAR buff[20];
    if (!LoadVoid(hKey, name, REG_SZ, buff, sizeof(buff), notFound))
        return FALSE;
    swscanf_s(buff, L"%d", (int*)data);
    return TRUE;
}

//*****************************************************************************
//
// CRegDouble
//

BOOL CRegDouble::Save(HKEY hKey, const WCHAR* name, void* data)
{
    WCHAR buff[30];
    swprintf_s(buff, L"%f", *(double*)data);
    return SaveVoid(hKey, name, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
}

BOOL CRegDouble::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    WCHAR buff[30];
    if (!LoadVoid(hKey, name, REG_SZ, buff, sizeof(buff), notFound))
        return FALSE;
    swscanf_s(buff, L"%lf", (double*)data);
    return TRUE;
}

//*****************************************************************************
//
// CRegCOLORREF
//

BOOL CRegCOLORREF::Save(HKEY hKey, const WCHAR* name, void* data)
{
    WCHAR buff[30];
    COLORREF& color = *(COLORREF*)data;
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);
    swprintf_s(buff, L"%d,%d,%d", r, g, b);
    return SaveVoid(hKey, name, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
}

BOOL CRegCOLORREF::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    WCHAR buff[30];
    int r;
    int g;
    int b;
    if (!LoadVoid(hKey, name, REG_SZ, buff, sizeof(buff), notFound))
        return FALSE;
    swscanf_s(buff, L"%d,%d,%d", &r, &g, &b);
    *(COLORREF*)data = RGB(r, g, b);
    return TRUE;
}

//*****************************************************************************
//
// CRegWindowPlacement
//

BOOL CRegWindowPlacement::Save(HKEY hKey, const WCHAR* name, void* data)
{
    WCHAR buff[255];
    WINDOWPLACEMENT* wp = (WINDOWPLACEMENT*)data;
    swprintf_s(buff, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
               wp->length,
               wp->flags,
               wp->showCmd,
               wp->ptMinPosition.x,
               wp->ptMinPosition.y,
               wp->ptMaxPosition.x,
               wp->ptMaxPosition.y,
               wp->rcNormalPosition.left,
               wp->rcNormalPosition.top,
               wp->rcNormalPosition.right,
               wp->rcNormalPosition.bottom);
    return SaveVoid(hKey, name, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
}

BOOL CRegWindowPlacement::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    WCHAR buff[255];
    if (!LoadVoid(hKey, name, REG_SZ, buff, sizeof(buff), notFound))
        return FALSE;
    WINDOWPLACEMENT& wp = *(WINDOWPLACEMENT*)data;
    swscanf_s(buff, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
              &wp.length,
              &wp.flags,
              &wp.showCmd,
              &wp.ptMinPosition.x,
              &wp.ptMinPosition.y,
              &wp.ptMaxPosition.x,
              &wp.ptMaxPosition.y,
              &wp.rcNormalPosition.left,
              &wp.rcNormalPosition.top,
              &wp.rcNormalPosition.right,
              &wp.rcNormalPosition.bottom);
    return TRUE;
}

//*****************************************************************************
//
// CRegLogFont
//

BOOL CRegLogFont::Save(HKEY hKey, const WCHAR* name, void* data)
{
    WCHAR buff[1024];
    LOGFONT* lf = (LOGFONT*)data;
    swprintf_s(buff, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;%s",
               lf->lfHeight,
               lf->lfWidth,
               lf->lfEscapement,
               lf->lfOrientation,
               lf->lfWeight,
               lf->lfItalic,
               lf->lfUnderline,
               lf->lfStrikeOut,
               lf->lfCharSet,
               lf->lfOutPrecision,
               lf->lfClipPrecision,
               lf->lfQuality,
               lf->lfPitchAndFamily,
               lf->lfFaceName);
    return SaveVoid(hKey, name, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
}

BOOL CRegLogFont::Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound)
{
    WCHAR buff[1024];
    if (!LoadVoid(hKey, name, REG_SZ, buff, sizeof(buff), notFound))
        return FALSE;
    LOGFONT& lf = *(LOGFONT*)data;
    swscanf_s(buff, L"%d,%d,%d,%d,%d,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
              &lf.lfHeight,
              &lf.lfWidth,
              &lf.lfEscapement,
              &lf.lfOrientation,
              &lf.lfWeight,
              &lf.lfItalic,
              &lf.lfUnderline,
              &lf.lfStrikeOut,
              &lf.lfCharSet,
              &lf.lfOutPrecision,
              &lf.lfClipPrecision,
              &lf.lfQuality,
              &lf.lfPitchAndFamily);
    WCHAR* p = buff;
    while (*p != L';' && *p != 0)
        p++;
    if (*p == L';')
    {
        p++;
        lstrcpyn(lf.lfFaceName, p, _countof(lf.lfFaceName));
    }
    else
        lf.lfFaceName[0] = 0;
    return TRUE;
}

//*****************************************************************************
//
// CRegistryPath
//

CRegistryPath::CRegistryPath(HKEY hRootKey, WCHAR* path)
{
    HRootKey = hRootKey;
    Path = path;
    HKey = NULL;
}

CRegistryPath::~CRegistryPath()
{
    delete Path;
}

BOOL CRegistryPath::CreateKey()
{
    DWORD createType; // info jestli byl klic vytvoren nebo jen otevren
    LONG ret = HANDLES(RegCreateKeyEx(HRootKey, Path, 0, NULL, REG_OPTION_NON_VOLATILE,
                                      KEY_READ | KEY_WRITE, NULL, &HKey,
                                      &createType));
    if (ret == ERROR_SUCCESS)
        return TRUE;
    else
    {
        MESSAGE_EW(NULL, spfW(REGERR_CREATEKEY, GetHKeyName(HRootKey), Path, errW(ret)), MB_OK);
        return FALSE;
    }
}

BOOL CRegistryPath::OpenKey(BOOL* notFound)
{
    if (notFound != NULL)
        *notFound = FALSE;
    LONG ret = HANDLES_Q(RegOpenKeyEx(HRootKey, Path, 0, KEY_READ, &HKey));
    if (ret == ERROR_SUCCESS)
        return TRUE;
    else
    {
        if (ret != ERROR_FILE_NOT_FOUND)
        {
            MESSAGE_EW(NULL, spfW(REGERR_OPENKEY, GetHKeyName(HRootKey), Path, errW(ret)), MB_OK);
            return FALSE;
        }
        else
        {
            if (notFound != NULL)
                *notFound = TRUE;
        }
        return FALSE;
    }
}

BOOL CRegistryPath::CloseKey()
{
    if (HKey != NULL)
        HANDLES(RegCloseKey(HKey));
    return TRUE;
}

//*****************************************************************************
//
// CRegistryClass
//

CRegistryClass::CRegistryClass(CRegistryPath* registryPath, const WCHAR* dataName,
                               void* data, CReg* ioClass)
{
    RegistryPath = registryPath;
    DataName = dataName;
    Data = data;
    IOClass = ioClass;
}

BOOL CRegistryClass::SaveValue()
{
    if (RegistryPath == NULL || DataName == NULL || Data == NULL)
    {
        TRACE_E("Neni provedena registrace.");
        return FALSE;
    }
    if (!IOClass->Save(RegistryPath->GetHKey(), DataName, Data))
    {
        HKEY hRootKey = RegistryPath->GetHRootKey();
        MESSAGE_EW(NULL, spfW(REGERR_SAVEITEM, GetHKeyName(hRootKey), RegistryPath->GetPath(), DataName), MB_OK);
        return FALSE;
    }
    return TRUE;
}

BOOL CRegistryClass::LoadValue(BOOL& notFound)
{
    notFound = FALSE;
    if (RegistryPath == NULL || DataName == NULL || Data == NULL)
    {
        TRACE_E("Neni provedena registrace.");
        return FALSE;
    }
    HKEY hKey = RegistryPath->GetHKey();
    if (hKey == NULL)
    {
        notFound = TRUE;
        return FALSE;
    }
    if (!IOClass->Load(hKey, DataName, Data, &notFound))
    {
        if (!notFound)
        {
            HKEY hRootKey = RegistryPath->GetHRootKey();
            MESSAGE_EW(NULL, spfW(REGERR_LOADITEM, GetHKeyName(hRootKey), RegistryPath->GetPath(), DataName), MB_OK);
            return FALSE;
        }
    }
    return TRUE;
}

//*****************************************************************************
//
// CRegistry
//

CRegistry::CRegistry()
    : RegistryClasses(10, 5), RegistryPaths(10, 5)
{
}

BOOL CRegistry::RegisterPath(HRegistryPath& hRegistryPath, HKEY hRootKey, const WCHAR* path, ...)
{
    int len = 1;
    const WCHAR* iterator = path;
    va_list params;
    va_start(params, path);
    while (iterator != NULL)
    {
        len += wcslen(iterator) + 1;
        iterator = va_arg(params, const WCHAR*);
    }
    va_end(params);

    WCHAR* newPath = new WCHAR[len];
    if (newPath == NULL)
    {
        MESSAGE_EW(NULL, REGERR_LOWMEMORY, MB_OK);
        return FALSE;
    }

    newPath[0] = 0;
    iterator = path;
    va_start(params, path);
    while (iterator != NULL)
    {
#ifdef DIAGNOSTICS_ENABLE
        if (iterator[0] == L'\\' || iterator[0] != 0 && iterator[wcslen(iterator) - 1] == L'\\')
        {
            TRACE_EW(L"Jednotlive polozky cesty nesmi zacinat nebo koncit zpetnym lomitkem. Polozka: " << iterator);
        }
        if (wcslen(iterator) == 0)
        {
            TRACE_E("Polozka cesty ma nulovou delku");
        }
#endif //DIAGNOSTICS_ENABLE
        wcscat_s(newPath, len, iterator);
        wcscat_s(newPath, len, L"\\");
        iterator = va_arg(params, const WCHAR*);
    }
    va_end(params);

    int lenNewPath = wcslen(newPath);
    if (lenNewPath > 0 && newPath[lenNewPath - 1] == L'\\')
        newPath[lenNewPath - 1] = 0; // W95 Hell

    CRegistryPath* registryPath = new CRegistryPath(hRootKey, newPath);
    if (registryPath == NULL)
    {
        delete[] newPath;
        MESSAGE_EW(NULL, REGERR_LOWMEMORY, MB_OK);
        return FALSE;
    }

    int index = RegistryPaths.Add(registryPath);
    if (!RegistryPaths.IsGood())
    {
        delete registryPath;
        return FALSE;
    }
    hRegistryPath = (HRegistryPath)index;
    return TRUE;
}

BOOL CRegistry::RegisterClass(HRegistryPath hRegistryPath, const WCHAR* dataName,
                              void* dataClass, CReg* ioClass)
{
    CRegistryClass* registryClass;
    registryClass = new CRegistryClass(RegistryPaths[hRegistryPath],
                                       dataName, dataClass, ioClass);
    if (registryClass == NULL)
    {
        MESSAGE_EW(NULL, REGERR_LOWMEMORY, MB_OK);
        return FALSE;
    }

    RegistryClasses.Add(registryClass);
    if (!RegistryClasses.IsGood())
    {
        delete registryClass;
        return FALSE;
    }
    return TRUE;
}

BOOL CRegistry::CreateKeys()
{
    BOOL ret = TRUE;
    for (int i = 0; i < (int)RegistryPaths.Count; i++)
        ret &= RegistryPaths[i]->CreateKey();
    return ret;
}

BOOL CRegistry::OpenKeys()
{
    BOOL notFound;
    BOOL ret;
    for (int i = 0; i < (int)RegistryPaths.Count; i++)
        ret &= RegistryPaths[i]->OpenKey(&notFound);
    return ret;
}

BOOL CRegistry::CloseKeys()
{
    BOOL ret = TRUE;
    for (int i = 0; i < (int)RegistryPaths.Count; i++)
        ret &= RegistryPaths[i]->CloseKey();
    return ret;
}

BOOL CRegistry::Save()
{
    BOOL ret = TRUE;
    ret &= CreateKeys();
    for (int i = 0; i < (int)RegistryClasses.Count; i++)
        ret &= RegistryClasses[i]->SaveValue();
    ret &= CloseKeys();
    return ret;
}

BOOL CRegistry::Load()
{
    BOOL notFound;
    BOOL ret = TRUE;
    ret &= OpenKeys();
    for (int i = 0; i < (int)RegistryClasses.Count; i++)
        ret &= !RegistryClasses[i]->LoadValue(notFound);
    ret &= CloseKeys();
    return ret;
}
