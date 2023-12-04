// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <tchar.h>

#include "regparse.h"

static eRPE_ERROR CopyKey(CSalamanderRegistryExAbstract* pInReg, HKEY hInKey, CSalamanderRegistryExAbstract* pOutReg, HKEY hOutKey)
{
    eRPE_ERROR ret = RPE_OK;

    TCHAR name[MAX_PATH];

    DWORD indKey;
    for (indKey = 0;; indKey++)
    {
        HKEY hSubInKey, hSubOutKey;
        if (!pInReg->EnumKey(hInKey, indKey, name, SizeOf(name)))
        {
            break;
        }
        if (!pInReg->OpenKey(hInKey, name, hSubInKey))
        {
            return RPE_KEY_OPEN;
        }
        if (!pOutReg->CreateKey(hOutKey, name, hSubOutKey))
        {
            pInReg->CloseKey(hSubInKey);
            return RPE_KEY_CREATE;
        }
        ret = CopyKey(pInReg, hSubInKey, pOutReg, hSubOutKey);
        pInReg->CloseKey(hSubInKey);
        pOutReg->CloseKey(hSubOutKey);
        if (RPE_OK != ret)
        {
            return ret;
        }
    }

    DWORD indVal;
    for (indVal = 0;; indVal++)
    {
        DWORD valType, dataSize;
        BYTE data[256];
        LPBYTE dataPtr = data;

        if (!pInReg->EnumValue(hInKey, indVal, name, SizeOf(name), &valType, NULL, NULL))
        {
            break;
        }
        if (!pInReg->GetSize(hInKey, name, valType, dataSize))
        {
            ret = RPE_VALUE_GET_SIZE;
            break;
        }
        if (dataSize > sizeof(data))
        {
            dataPtr = (LPBYTE)malloc(dataSize);
            if (!dataPtr)
            {
                ret = RPE_OUT_OF_MEMORY;
                break;
            }
        }
        if (!pInReg->GetValue(hInKey, name, valType, dataPtr, dataSize))
        {
            if (dataPtr != data)
                free(dataPtr);
            ret = RPE_VALUE_GET;
            break;
        }
        if (!pOutReg->SetValue(hOutKey, name, valType, dataPtr, dataSize))
        {
            if (dataPtr != data)
                free(dataPtr);
            ret = RPE_VALUE_SET;
            break;
        }
        if (dataPtr != data)
            free(dataPtr);
    }
    return ret;
}

eRPE_ERROR CopyBranch(LPCTSTR branch, CSalamanderRegistryExAbstract* pInReg, CSalamanderRegistryExAbstract* pOutReg)
{
    HKEY hParentKey, hInKey, hOutKey;
    eRPE_ERROR ret = RPE_OK;

    if (!_tcsnicmp(branch, _T("HKEY_CURRENT_USER"), SizeOf(_T("HKEY_CURRENT_USER")) - 1))
    {
        branch += SizeOf(_T("HKEY_CURRENT_USER")) - 1;
        hParentKey = HKEY_CURRENT_USER;
    }
    else if (!_tcsnicmp(branch, _T("HKEY_CLASSES_ROOT"), SizeOf(_T("HKEY_CLASSES_ROOT")) - 1))
    {
        branch += SizeOf(_T("HKEY_CLASSES_ROOT")) - 1;
        hParentKey = HKEY_CLASSES_ROOT;
    }
    else if (!_tcsnicmp(branch, _T("HKEY_LOCAL_MACHINE"), SizeOf(_T("HKEY_LOCAL_MACHINE")) - 1))
    {
        branch += SizeOf(_T("HKEY_LOCAL_MACHINE")) - 1;
        hParentKey = HKEY_LOCAL_MACHINE;
    }
    else if (!_tcsnicmp(branch, _T("HKEY_USERS"), SizeOf(_T("HKEY_USERS")) - 1))
    {
        branch += SizeOf(_T("HKEY_USERS")) - 1;
        hParentKey = HKEY_USERS;
    }
    else if (!_tcsnicmp(branch, _T("HKEY_CURRENT_CONFIG"), SizeOf(_T("HKEY_CURRENT_CONFIG")) - 1))
    {
        branch += SizeOf(_T("HKEY_CURRENT_CONFIG")) - 1;
        hParentKey = HKEY_CURRENT_CONFIG;
    }
    else if (!_tcsnicmp(branch, _T("HKEY_DYN_DATA"), SizeOf(_T("HKEY_DYN_DATA")) - 1))
    {
        branch += SizeOf(_T("HKEY_DYN_DATA")) - 1;
        hParentKey = HKEY_DYN_DATA;
    }
    else if (!_tcsnicmp(branch, _T("HKEY_PERFORMANCE_DATA"), SizeOf(_T("HKEY_PERFORMANCE_DATA")) - 1))
    {
        branch += SizeOf(_T("HKEY_PERFORMANCE_DATA")) - 1;
        hParentKey = HKEY_PERFORMANCE_DATA;
    }
    else
    {
        return RPE_ROOT_INVALID_KEY;
    }

    if (*branch != 0)
    {
        if (*branch != '\\')
        {
            return RPE_ROOT_INVALID_KEY;
        }
        else
        {
            branch++;
        }
    }

    if (!pInReg->OpenKey(hParentKey, branch, hInKey))
    {
        return RPE_KEY_OPEN;
    }

    if (pOutReg->CreateKey(hParentKey, branch, hOutKey))
    {
        ret = CopyKey(pInReg, hInKey, pOutReg, hOutKey);
        pOutReg->CloseKey(hOutKey);
    }
    else
    {
        ret = RPE_KEY_CREATE;
    }

    pInReg->CloseKey(hInKey);

    return ret;
}
