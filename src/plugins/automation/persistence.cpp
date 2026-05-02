// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	persistence.cpp
	Implements storage for the persistent values.
*/

#include "precomp.h"
#include "persistence.h"
#include "aututils.h"

CPersistentValueStorage::CPersistentValueStorage() : m_values(4, 4)
{
    m_bModified = false;
}

CPersistentValueStorage::~CPersistentValueStorage()
{
}

bool CPersistentValueStorage::Load(
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    LONG res = NO_ERROR;
    DWORD cchName;
    DWORD cbData;
    DWORD dwIndex;
    DWORD dwType;
    WCHAR wzName[256];

    m_values.DestroyMembers();

    for (dwIndex = 0; res == NO_ERROR; dwIndex++)
    {
        PERSISTENT_ENTRY entry;

        cchName = _countof(wzName);
        cbData = 0;
        res = RegEnumValueW(hKey, dwIndex, wzName, &cchName,
                            NULL, &dwType, NULL, &cbData);
        if (res == ERROR_NO_MORE_ITEMS)
        {
            break;
        }

        if (res != NO_ERROR)
        {
            _ASSERT(0);
            return false;
        }

        if (dwType == REG_SZ)
        {
            UINT cch = cbData / sizeof(WCHAR);
            WCHAR* pwzTmp;

            pwzTmp = new WCHAR[(cch > 0) ? cch : 1];

            if (cch > 0)
            {
                cchName = _countof(wzName);
                cbData = cch * sizeof(WCHAR);
                res = RegEnumValueW(hKey, dwIndex, wzName, &cchName,
                                    NULL, NULL, (BYTE*)pwzTmp, &cbData);
                if (res != NO_ERROR)
                {
                    _ASSERTE(0);
                    delete[] pwzTmp;
                    continue;
                }

                // cbData includes nul terminator (if any), but
                // SysAllocString excludes nul terminator
                _ASSERTE(pwzTmp[cch - 1] == L'\0');
                if (pwzTmp[cch - 1] == L'\0')
                {
                    --cch;
                }
            }
            else
            {
                pwzTmp[0] = L'\0';
            }

            V_VT(&entry.value) = VT_BSTR;
            V_BSTR(&entry.value) = SysAllocStringLen(pwzTmp, cch);

            delete[] pwzTmp;
        }
        else if (dwType == REG_DWORD && cbData == sizeof(DWORD))
        {
            DWORD dwValue;

            cchName = _countof(wzName);
            cbData = sizeof(dwValue);
            res = RegEnumValueW(hKey, dwIndex, wzName, &cchName,
                                NULL, NULL, (BYTE*)&dwValue, &cbData);

            if (res == NO_ERROR)
            {
                V_VT(&entry.value) = VT_INT;
                V_INT(&entry.value) = dwValue;
            }
            else
            {
                _ASSERTE(0);
                continue;
            }
        }
        else
        {
            continue;
        }

        entry.name = SysAllocString(wzName);
        m_values.Add(entry);
    }

    m_bModified = false;
    return true;
}

bool CPersistentValueStorage::Save(
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    _ASSERTE(hKey != NULL);

    for (int i = 0; i < m_values.Count; i++)
    {
        PERSISTENT_ENTRY& entry = m_values.At(i);

        switch (entry.state)
        {
        case ENTRY_STALE:
            // no change to this entry
            break;

        case ENTRY_NEW:
        case ENTRY_MODIFIED:
            SaveEntry(entry.name, &entry.value, hKey, registry);
            break;

        case ENTRY_DELETED:
            registry->DeleteValue(hKey, OLE2T(entry.name));
            break;

        default:
            _ASSERTE(0);
        }
    }

    m_bModified = false;

    return true;
}

HRESULT CPersistentValueStorage::GetValue(
    BSTR strName,
    VARIANT* value)
{
    PERSISTENT_ENTRY* entry;

    entry = LookupValue(strName);
    if (entry == NULL || entry->state == ENTRY_DELETED)
    {
        V_VT(value) = VT_NULL;
        return S_OK;
    }

    VariantInit(value);
    return VariantCopy(value, &entry->value);
}

HRESULT CPersistentValueStorage::SetValue(
    BSTR strName,
    const VARIANT* value)
{
    PERSISTENT_ENTRY* entry;
    bool bNull;

    // there are some constraints put on the value name
    if (!ValidateName(strName))
    {
        return E_INVALIDARG;
    }

    bNull = !IsArgumentPresent(value);

    // we can only store subset of data types
    // supported by VARIANT
    if (!bNull && !SupportedType(V_VT(value)))
    {
        return DISP_E_BADVARTYPE;
    }

    entry = LookupValue(strName);
    if (entry)
    {
        m_bModified = true;

        if (bNull)
        {
            entry->state = ENTRY_DELETED;
        }
        else
        {
            VariantClear(&entry->value);
            VariantCopy(&entry->value, value);
            entry->state = ENTRY_MODIFIED;
        }
    }
    else if (!bNull)
    {
        m_values.Add(PERSISTENT_ENTRY(strName, value, ENTRY_NEW));
        m_bModified = true;
    }

    return S_OK;
}

CPersistentValueStorage::PERSISTENT_ENTRY*
CPersistentValueStorage::LookupValue(BSTR strName)
{
    for (int i = 0; i < m_values.Count; i++)
    {
        if (_wcsicmp(strName, m_values.At(i).name) == 0)
        {
            return &m_values.At(i);
        }
    }

    return NULL;
}

bool CPersistentValueStorage::ValidateName(BSTR strName)
{
    if (strName == NULL || *strName == L'\0')
    {
        return false;
    }

    const OLECHAR* s = strName;
    while (*s)
    {
        if (!((*s >= L'A' && *s <= 'Z') ||
              (*s >= L'a' && *s <= 'z') ||
              (*s >= L'0' && *s <= '9') ||
              *s == L'.' || *s == L':' ||
              *s == '_'))
        {
            return false;
        }

        ++s;
    }

    if (s - strName > 255)
    {
        return false;
    }

    return true;
}

bool CPersistentValueStorage::SupportedType(VARTYPE vt)
{
    switch (vt)
    {
    case VT_EMPTY:
    case VT_NULL:
        // actually deletes the value
        return true;

    case VT_BSTR:
        // saved as REG_SZ
        return true;

    case VT_BOOL:
    case VT_I1:
    case VT_I2:
    case VT_I4:
    case VT_UI1:
    case VT_UI2:
    case VT_UI4:
    case VT_INT:
    case VT_UINT:
        // saved as REG_DWORD
        return true;
    }

    return false;
}

bool CPersistentValueStorage::SaveEntry(
    PCWSTR pwzName,
    const VARIANT* value,
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    if (V_VT(value) == VT_BSTR)
    {
        // explicitly use the wide version of the function
        // to save all Unicode characters
        return RegSetValueExW(hKey, pwzName, 0, REG_SZ,
                              (const BYTE*)V_BSTR(value),
                              ((DWORD)wcslen(V_BSTR(value)) + 1) * sizeof(WCHAR)) == NO_ERROR;
    }
    else
    {
        DWORD dwVal;

        switch (V_VT(value))
        {
        case VT_BOOL:
            dwVal = (V_BOOL(value) == VARIANT_TRUE) ? 1 : 0;
            break;

        case VT_I1:
            dwVal = V_I1(value);
            break;

        case VT_I2:
            dwVal = V_I2(value);
            break;

        case VT_I4:
            dwVal = V_I4(value);
            break;

        case VT_UI1:
            dwVal = V_UI1(value);
            break;

        case VT_UI2:
            dwVal = V_UI2(value);
            break;

        case VT_UI4:
            dwVal = V_UI4(value);
            break;

        case VT_INT:
            dwVal = V_INT(value);
            break;

        case VT_UINT:
            dwVal = V_UINT(value);
            break;

        default:
            _ASSERTE(0);
            return false;
        }

        return !!registry->SetValue(hKey, OLE2T(pwzName), REG_DWORD,
                                    &dwVal, sizeof(dwVal));
    }

    _ASSERTE(0);
    return false;
}
