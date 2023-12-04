// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <tchar.h>

#include "regparse.h"

BOOL StrEndsWith(LPCTSTR txt, LPCTSTR pattern, size_t patternLen);

namespace RegLib
{

    class CSystemRegistry : public CSalamanderRegistryExAbstract
    {
    public:
        virtual BOOL WINAPI ClearKey(HKEY key);
        virtual BOOL WINAPI CreateKey(HKEY key, LPCTSTR name, HKEY& createdKey);
        virtual BOOL WINAPI OpenKey(HKEY key, LPCTSTR name, HKEY& openedKey);
        virtual void WINAPI CloseKey(HKEY key);
        virtual BOOL WINAPI DeleteKey(HKEY key, LPCTSTR name);
        virtual BOOL WINAPI GetValue(HKEY key, LPCTSTR name, DWORD type, LPVOID data, DWORD dataSize);
        virtual BOOL WINAPI SetValue(HKEY key, LPCTSTR name, DWORD type, LPCVOID data, DWORD dataSize);
        virtual BOOL WINAPI DeleteValue(HKEY key, LPCTSTR name);
        virtual BOOL WINAPI GetSize(HKEY key, LPCTSTR name, DWORD type, DWORD& bufferSize);

        virtual BOOL WINAPI EnumKey(HKEY key, DWORD subKeyIndex, LPTSTR name, DWORD bufferSize);
        virtual BOOL WINAPI EnumValue(HKEY key, DWORD valIndex, LPTSTR name, DWORD nameSize, LPDWORD valType, LPBYTE data, LPDWORD dataSize);

        virtual void WINAPI RemoveHiddenKeysAndValues() {}
        virtual BOOL WINAPI ClearKeyEx(HKEY key, BOOL doNotDeleteHiddenKeysAndValues, BOOL* keyIsNotEmpty);

        virtual void WINAPI Release();
        virtual BOOL WINAPI Dump(LPCTSTR /*fileName*/, LPCTSTR /*clearKeyName*/) { return TRUE; }

    private:
        BOOL ClearKeyAux(HKEY key, BOOL doNotDeleteHiddenKeysAndValues, BOOL* keyIsNotEmpty);
    };

    void CSystemRegistry::Release()
    {
        delete this;
    }

    BOOL CSystemRegistry::ClearKey(HKEY key)
    {
        return ClearKeyAux(key, FALSE, NULL);
    }

    BOOL CSystemRegistry::ClearKeyEx(HKEY key, BOOL doNotDeleteHiddenKeysAndValues, BOOL* keyIsNotEmpty)
    {
        return ClearKeyAux(key, doNotDeleteHiddenKeysAndValues, keyIsNotEmpty);
    }

    BOOL CSystemRegistry::CreateKey(HKEY key, LPCTSTR name, HKEY& createdKey)
    {
        char buff[] = "";
        return ERROR_SUCCESS == RegCreateKeyEx(key, name, 0, buff, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &createdKey, NULL);
    }

    BOOL CSystemRegistry::OpenKey(HKEY key, LPCTSTR name, HKEY& openedKey)
    {
        return ERROR_SUCCESS == RegOpenKeyEx(key, name, 0, KEY_ALL_ACCESS, &openedKey);
    }

    void CSystemRegistry::CloseKey(HKEY key)
    {
        RegCloseKey(key);
    }

    BOOL CSystemRegistry::DeleteKey(HKEY key, LPCTSTR name)
    {
        return ERROR_SUCCESS == RegDeleteKey(key, name);
    }

#ifndef INSIDE_SALAMANDER

    // nase varianta funkce RegQueryValueEx, narozdil od API varianty zajistuje
    // pridani null-terminatoru pro typy REG_SZ, REG_MULTI_SZ a REG_EXPAND_SZ
    // POZOR: pri zjistovani potrebne velikosti bufferu vraci o jeden nebo dva (dva
    //        jen u REG_MULTI_SZ) znaky vic pro pripad, ze by string bylo potreba
    //        zakoncit nulou/nulami
    LONG SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                            LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
    {
        DWORD dataBufSize = lpData == NULL ? 0 : *lpcbData;
        DWORD type = REG_NONE;
        LONG ret = RegQueryValueEx(hKey, lpValueName, lpReserved, &type, lpData, lpcbData);
        if (lpType != NULL)
            *lpType = type;
        if (type == REG_SZ || type == REG_MULTI_SZ || type == REG_EXPAND_SZ)
        {
            if (hKey != HKEY_PERFORMANCE_DATA &&
                lpcbData != NULL &&
                (ret == ERROR_MORE_DATA || lpData == NULL && ret == ERROR_SUCCESS))
            {
                (*lpcbData) += type == REG_MULTI_SZ ? 2 : 1; // rekneme si radsi o pripadny null-terminator(y) navic
                return ret;
            }
            if (ret == ERROR_SUCCESS && lpData != NULL)
            {
                if (*lpcbData < 1 || ((char*)lpData)[*lpcbData - 1] != 0)
                {
                    if (*lpcbData < dataBufSize)
                    {
                        ((char*)lpData)[*lpcbData] = 0;
                        (*lpcbData)++;
                    }
                    else // nedostatek mista pro null-terminator v bufferu
                    {
                        (*lpcbData) += type == REG_MULTI_SZ ? 2 : 1; // rekneme si o potrebny null-terminator(y)
                        return ERROR_MORE_DATA;
                    }
                }
                if (type == REG_MULTI_SZ && (*lpcbData < 2 || ((char*)lpData)[*lpcbData - 2] != 0))
                {
                    if (*lpcbData < dataBufSize)
                    {
                        ((char*)lpData)[*lpcbData] = 0;
                        (*lpcbData)++;
                    }
                    else // nedostatek mista pro druhy null-terminator v bufferu
                    {
                        (*lpcbData)++; // rekneme si o potrebny null-terminator
                        return ERROR_MORE_DATA;
                    }
                }
            }
        }
        return ret;
    }

#endif // INSIDE_SALAMANDER

    BOOL CSystemRegistry::GetValue(HKEY key, LPCTSTR name, DWORD type, LPVOID data, DWORD dataSize)
    {
        DWORD realType;

        return (ERROR_SUCCESS == SalRegQueryValueEx(key, name, NULL, &realType, (BYTE*)data, &dataSize)) && (realType == type);
    }

    BOOL CSystemRegistry::SetValue(HKEY key, LPCTSTR name, DWORD type, LPCVOID data, DWORD dataSize)
    {
        return ERROR_SUCCESS == RegSetValueEx(key, name, NULL, type, (BYTE*)data, dataSize);
    }

    BOOL CSystemRegistry::DeleteValue(HKEY key, LPCTSTR name)
    {
        return ERROR_SUCCESS == RegDeleteValue(key, name);
    }

    BOOL CSystemRegistry::GetSize(HKEY key, LPCTSTR name, DWORD type, DWORD& dataSize)
    {
        DWORD realType;

        return (ERROR_SUCCESS == SalRegQueryValueEx(key, name, NULL, &realType, NULL, &dataSize)) && (realType == type);
    }

    BOOL CSystemRegistry::EnumKey(HKEY key, DWORD subKeyIndex, LPTSTR name, DWORD bufferSize)
    {
        return ERROR_SUCCESS == RegEnumKeyEx(key, subKeyIndex, name, &bufferSize, NULL, NULL, NULL, NULL);
    }

    BOOL CSystemRegistry::EnumValue(HKEY key, DWORD valIndex, LPTSTR name, DWORD nameSize, LPDWORD valType, LPBYTE data, LPDWORD dataSize)
    {
        return ERROR_SUCCESS == RegEnumValue(key, valIndex, name, &nameSize, NULL, valType, data, dataSize);
    }

    BOOL CSystemRegistry::ClearKeyAux(HKEY key, BOOL doNotDeleteHiddenKeysAndValues, BOOL* keyIsNotEmpty)
    {
        TCHAR name[MAX_PATH];
        HKEY subKey;
        DWORD size = SizeOf(name);
        DWORD index = 0;

        while (RegEnumKeyEx(key, index, name, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            if (!doNotDeleteHiddenKeysAndValues ||
                !::StrEndsWith(name, _T(".hidden"), SizeOf(_T(".hidden")) - 1))
            {
                if (RegOpenKeyEx(key, name, 0, KEY_READ | KEY_WRITE, &subKey) == ERROR_SUCCESS)
                {
                    BOOL subkeyIsNotEmpty = FALSE;
                    BOOL ret = ClearKeyAux(subKey, doNotDeleteHiddenKeysAndValues, &subkeyIsNotEmpty);
                    if (subkeyIsNotEmpty && keyIsNotEmpty != NULL)
                        *keyIsNotEmpty = TRUE;

                    RegCloseKey(subKey);
                    if (!ret || !subkeyIsNotEmpty && RegDeleteKey(key, name) != ERROR_SUCCESS)
                        return FALSE;
                    if (subkeyIsNotEmpty)
                        index++;
                }
                else
                {
                    return FALSE;
                }
            }
            else
            {
                if (keyIsNotEmpty != NULL)
                    *keyIsNotEmpty = TRUE;
                index++;
            }
            size = SizeOf(name);
        }

        size = SizeOf(name);
        index = 0;
        while (RegEnumValue(key, index, name, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            if (!doNotDeleteHiddenKeysAndValues ||
                !::StrEndsWith(name, _T(".hidden"), SizeOf(_T(".hidden")) - 1))
            {
                if (RegDeleteValue(key, name) != ERROR_SUCCESS)
                { // Unable to delete values in specified key (in registry)
                    break;
                }
            }
            else
            {
                if (keyIsNotEmpty != NULL)
                    *keyIsNotEmpty = TRUE;
                index++;
            }
            size = SizeOf(name);
        }

        return TRUE;
    }

} // namespace RegLib

CSalamanderRegistryExAbstract* REG_SysRegistryFactory()
{
    return new RegLib::CSystemRegistry();
}
