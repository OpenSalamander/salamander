// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <Objbase.h>
#include <WTypes.h>

#include "config.h"

const char* DB_ROOT_KEY = "Software\\Open Salamander\\Bug Reporter";
const char* CONFIG_EMAIL_REG = "Email";

CConfiguration Config;

// ****************************************************************************

BOOL CreateKey(HKEY hKey, const char* name, HKEY& createdKey)
{
    DWORD createType; // info jestli byl klic vytvoren nebo jen otevren
    LONG res = HANDLES(RegCreateKeyEx(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                                      KEY_READ | KEY_WRITE, NULL, &createdKey,
                                      &createType));
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        return FALSE;
    }
}

// ****************************************************************************

BOOL SetValue(HKEY hKey, const char* name, DWORD type,
              const void* data, DWORD dataSize)
{
    if (dataSize == -1)
        dataSize = (lstrlen((char*)data) + 1) * sizeof(char);
    LONG res = RegSetValueEx(hKey, name, 0, type, (CONST BYTE*)data, dataSize);
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        return FALSE;
    }
}

// ****************************************************************************

BOOL OpenKey(HKEY hKey, const char* name, HKEY& openedKey)
{
    LONG res = HANDLES_Q(RegOpenKeyEx(hKey, name, 0, KEY_READ, &openedKey));
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        return FALSE;
    }
}

// ****************************************************************************

BOOL GetValue(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSizeInBytes)
{
    DWORD gettedType;
    LONG res = RegQueryValueEx(hKey, name, 0, &gettedType, (BYTE*)buffer, &bufferSizeInBytes);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            return FALSE;
        }
    else
    {
        return FALSE;
    }
}

//*****************************************************************************
//
// CConfiguration
//

CConfiguration::CConfiguration()
{
    ZeroMemory(Description, sizeof(Description));
    ZeroMemory(Email, sizeof(Email));
    Restart = TRUE;
}

BOOL CConfiguration::Load()
{
    HKEY hKey;
    if (OpenKey(HKEY_CURRENT_USER, DB_ROOT_KEY, hKey))
    {
        GetValue(hKey, CONFIG_EMAIL_REG, REG_SZ, Email, EMAIL_SIZE);
        HANDLES(RegCloseKey(hKey));
    }
    return TRUE;
}

BOOL CConfiguration::Save()
{
    HKEY hKey;
    if (CreateKey(HKEY_CURRENT_USER, DB_ROOT_KEY, hKey))
    {
        SetValue(hKey, CONFIG_EMAIL_REG, REG_SZ, Email, -1);
        HANDLES(RegCloseKey(hKey));
    }

    return TRUE;
}
