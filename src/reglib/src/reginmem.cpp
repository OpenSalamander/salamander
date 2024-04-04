// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <tchar.h>

#include "regparse.h"

#define REG_MAX_KEY_NAME_LEN 256

BOOL StrEndsWith(LPCTSTR txt, LPCTSTR pattern, size_t patternLen);

namespace RegLib
{

    typedef TIndirectArray<class CKey> TKeyList;
    typedef TDirectArray<class CValue> TValueList;

#ifndef REG_QWORD
// Original MSVC6 SDK
#define REG_QWORD 11
#endif

    typedef enum eVT_VALUE_TYPE
    {
        VT_STRING = REG_SZ,
        VT_DWORD = REG_DWORD,
        VT_BINARY = REG_BINARY,
        VT_QWORD = REG_QWORD,
        VT_MULTI_SZ = REG_MULTI_SZ,
        VT_EXPAND_SZ = REG_EXPAND_SZ,
        VT_INVALID = -1
    } eVT_VALUE_TYPE;

    class CValue
    {
    public:
        LPTSTR Name;
        eVT_VALUE_TYPE Type;
        union
        {
            LPTSTR str;
            DWORD dw;
            LPBYTE data;
        } Value;
        DWORD Size; // For other types than VT_DWORD and VT_STRING

        CValue(LPCTSTR name, eVT_VALUE_TYPE type, LPCVOID data, DWORD size);
        ~CValue();

        // copy-constructor is default (memcpy), after adding object to array, you must call
        // Invaliduje() nebo data objektu budou uvolněna i pro objekt v poli

        BOOL IsOK();

        void Invalidate();
    };

    class CKey
    {
    public:
        LPTSTR Name;
        CKey* pParent;
        int nCount;
        TKeyList SubKeys;
        TValueList Values;

        CKey(CKey* parent, LPCTSTR name);
        ~CKey();
        int AddRef();
        int Release();

        BOOL RemoveKey(CKey* pChild);
        CKey* GetKey(LPCTSTR name);

        BOOL RemoveValue(CValue* pChild);
        CValue* GetValue(LPCTSTR name);

        BOOL Clear();

        virtual BOOL Dump(HANDLE hFile, LPCTSTR fullKeyName, LPTSTR name, size_t maxlen);
        virtual BOOL RemoveHiddenKeysAndValues();
    };

    class CRootKey : public CKey
    {
    public:
        CRootKey() : CKey(NULL, _T("")) {}

        virtual BOOL Dump(HANDLE hFile, LPCTSTR fullKeyName, LPTSTR name, size_t maxlen);
    };

    class CMemoryRegistry : public CSalamanderRegistryExAbstract
    {
    public:
        CMemoryRegistry();
        virtual ~CMemoryRegistry() {}

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

        virtual void WINAPI RemoveHiddenKeysAndValues();
        virtual BOOL WINAPI ClearKeyEx(HKEY key, BOOL /*doNotDeleteHiddenKeysAndValues*/, BOOL* /*keyIsNotEmpty*/) { return ClearKey(key); }

        virtual void WINAPI Release();
        virtual BOOL WINAPI Dump(LPCTSTR fileName, LPCTSTR clearKeyName);

    private:
        CRootKey RootKey;

        virtual BOOL CreateOpenKey(HKEY key, LPCTSTR name, HKEY& openedKey, BOOL bCreate);
    };

    static BOOL WriteString(HANDLE hFile, LPCTSTR s)
    {
        DWORD nBytesWritten;
        TCHAR buf[64 + 2], *d;

        buf[0] = '\"';
        d = buf + 1;
        while (*s)
        {
#ifdef _UNICODE
            switch (*s)
            {
            case '\\':
                *d++ = '\\';
                *d++ = '\\';
                break;
            case '\"':
                *d++ = '\\';
                *d++ = '\"';
                break;
                //       case '\t': *d++ = '\\'; *d++ = 't'; break; // Tabs are not escaped...
            case '\r':
                *d++ = '\\';
                *d++ = 'r';
                break;
            case '\n':
                *d++ = '\\';
                *d++ = 'n';
                break;
            default:
                *d++ = *s;
            }
            s++;
#else
            if (!(*s & 0x80))
            {
                switch (*s)
                {
                case '\\':
                    *d++ = '\\';
                    *d++ = '\\';
                    break;
                case '\"':
                    *d++ = '\\';
                    *d++ = '\"';
                    break;
                    //          case '\t': *d++ = '\\'; *d++ = 't'; break; // Tabs are not escaped...
                case '\r':
                    *d++ = '\\';
                    *d++ = 'r';
                    break;
                case '\n':
                    *d++ = '\\';
                    *d++ = 'n';
                    break;
                default:
                    *d++ = *s;
                }
                s++;
            }
            else
            {
                if (IsDBCSLeadByte(*s))
                    *d++ = *s++;
                *d++ = *s++;
            }
#endif
            if (d - buf >= SizeOf(buf) - 2)
            {
                DWORD size = (DWORD)((d - buf) * sizeof(TCHAR));
                if (!WriteFile(hFile, buf, size, &nBytesWritten, NULL) || (size != nBytesWritten))
                {
                    return FALSE;
                }
                d = buf;
            }
        }
        *d++ = '\"';
        DWORD size = (DWORD)((d - buf) * sizeof(TCHAR));
        if (!WriteFile(hFile, buf, size, &nBytesWritten, NULL) || (size != nBytesWritten))
        {
            return FALSE;
        }
        return TRUE;
    }

    //////////////////////////// CValue ////////////////////////////

    CValue::CValue(LPCTSTR name, eVT_VALUE_TYPE type, LPCVOID data, DWORD size)
    {
        Name = _tcsdup(name);
        Type = type;
        switch (type)
        {
        case VT_DWORD:
            Value.dw = *(DWORD*)data;
            break;
        case VT_STRING:
            if ((DWORD)-1 == size)
                size = (DWORD)((_tcslen((LPCTSTR)data) + 1) * sizeof(TCHAR));
            if (!size || (((LPCTSTR)data)[size / sizeof(TCHAR) - 1]))
            {
                size += sizeof(TCHAR); // Ensure NUL termination
            }
            Value.str = (LPTSTR)malloc(size);
            if (size)
            {
                memcpy(Value.str, data, size - sizeof(TCHAR));
                Value.str[size / sizeof(TCHAR) - 1] = 0;
            }
            // NOTE: failed malloc checked by calling IsOK()
            Size = size;
            break;
        case VT_INVALID:
            Value.data = NULL;
            break;
        case VT_BINARY:
        default:
            Value.data = (LPBYTE)malloc(size);
            Size = size;
            if (Value.data)
                memcpy(Value.data, data, size);
            // NOTE: failed malloc checked by calling IsOK()
            break;
        }
    }

    CValue::~CValue()
    {
        if (Name)
        {
            free(Name);
            Name = NULL;
        }

        switch (Type)
        {
        case VT_STRING:
            if (Value.str)
                free(Value.str);
            break;
        case VT_INVALID:
        case VT_DWORD:
            break;
        case VT_BINARY:
        default:
            if (Value.data)
                free(Value.data);
            break;
        }
    }

    void CValue::Invalidate()
    {
        // Avoid freeing data
        Name = NULL;
        Type = VT_INVALID;
    }

    BOOL CValue::IsOK()
    {
        if (!Name)
            return FALSE;
        if (VT_STRING == Type)
            return Value.str != NULL;
        if ((VT_DWORD != Type) && !Value.data)
            return FALSE;
        return TRUE;
    }

    //////////////////////////// CKey ////////////////////////////
    CKey::CKey(CKey* parent, LPCTSTR name) : SubKeys(1, 2, dtNoDelete), Values(1, 2)
    {
        Name = _tcsdup(name);
        pParent = parent;
        if (pParent)
        {
            pParent->SubKeys.Add(this);
        }
        nCount = 1;
    }

    CKey::~CKey()
    {
        Clear();
        if (Name)
            free(Name);
        if (pParent)
            pParent->RemoveKey(this);
    }

    int CKey::AddRef()
    {
        return nCount++;
    }

    int CKey::Release()
    {
        int old = nCount--;

        if (!nCount)
            delete this;
        return old;
    }

    BOOL CKey::RemoveKey(CKey* pChild)
    {
        int i;
        for (i = 0; i < SubKeys.Count; i++)
        {
            if (SubKeys[i] == pChild)
            {
                SubKeys.Detach(i);
                return TRUE;
            }
        }
        return FALSE;
    }

    CKey* CKey::GetKey(LPCTSTR name)
    {
        int i;
        for (i = 0; i < SubKeys.Count; i++)
        {
            if (!_tcsicmp(SubKeys[i]->Name, name))
            {
                return SubKeys[i];
            }
        }
        return NULL;
    }

    BOOL CKey::RemoveValue(CValue* pChild)
    {
        int i;
        for (i = 0; i < Values.Count; i++)
        {
            if (&Values[i] == pChild)
            {
                Values.Delete(i);
                return TRUE;
            }
        }
        return FALSE;
    }

    CValue* CKey::GetValue(LPCTSTR name)
    {
        int i;
        for (i = 0; i < Values.Count; i++)
        {
            if (!_tcsicmp(Values[i].Name, name))
            {
                return &Values[i];
            }
        }
        return NULL;
    }

    BOOL CKey::Clear()
    {
        int i;
        for (i = SubKeys.Count - 1; i >= 0; i--)
        {
            SubKeys[i]->Release();
        }
        Values.DestroyMembers();
        return TRUE;
    }

    BOOL CKey::Dump(HANDLE hFile, LPCTSTR fullKeyName, LPTSTR name, size_t maxlen)
    {
        LPTSTR s = name;
        LPTSTR n = Name;

        while (*n)
        {
            if (s - name + sizeof("\\\\]\r\n") > maxlen)
            {
                return FALSE;
            }
#ifdef _UNICODE
            switch (*n)
            {
            case '\\':
                *s++ = '\\';
                *s++ = '\\';
                break;
            case '\"':
                *s++ = '\\';
                *s++ = '\"';
                break;
                //       case '\t': *s++ = '\\'; *s++ = 't'; break; // Tabs are not escaped...
            case '\r':
                *s++ = '\\';
                *s++ = 'r';
                break;
            case '\n':
                *s++ = '\\';
                *s++ = 'n';
                break;
            default:
                *s++ = *n;
            }
            n++;
#else
            if (!(*n & 0x80))
            {
                switch (*n)
                {
                case '\\':
                    *s++ = '\\';
                    *s++ = '\\';
                    break;
                case '\"':
                    *s++ = '\\';
                    *s++ = '\"';
                    break;
                    //          case '\t': *s++ = '\\'; *s++ = 't'; break; // Tabs are not escaped...
                case '\r':
                    *s++ = '\\';
                    *s++ = 'r';
                    break;
                case '\n':
                    *s++ = '\\';
                    *s++ = 'n';
                    break;
                default:
                    *s++ = *n;
                }
                n++;
            }
            else
            {
                if (IsDBCSLeadByte(*n))
                    *s++ = *n++;
                *s++ = *n++;
            }
#endif
        }
        *s++ = ']';
        *s++ = '\r';
        *s++ = '\n';

        DWORD nBytesWritten, size = (DWORD)((s - fullKeyName) * sizeof(TCHAR));
        if (pParent->pParent)
        {
            // Do not write out root keys, regedit cannot parse such .REG files...
            if (!WriteFile(hFile, fullKeyName, size, &nBytesWritten, NULL) || (size != nBytesWritten))
            {
                return FALSE;
            }
        }
        int i;
        for (i = 0; i < Values.Count; i++)
        {
            TCHAR buf[82];
            CValue* pValue = &Values[i];

            if (*pValue->Name)
            {
                if (!WriteString(hFile, pValue->Name))
                {
                    return FALSE;
                }
            }
            else
            {
                // The (Default) value of the key
                if (!WriteFile(hFile, _T("@"), sizeof(TCHAR), &nBytesWritten, NULL) || (sizeof(TCHAR) != nBytesWritten))
                {
                    return FALSE;
                }
            }
            if (!WriteFile(hFile, _T("="), sizeof(TCHAR), &nBytesWritten, NULL) || (sizeof(TCHAR) != nBytesWritten))
            {
                return FALSE;
            }

            switch (pValue->Type)
            {
            case VT_DWORD:
                _stprintf(buf, _T("dword:%08x\r\n"), pValue->Value.dw);
                size = (DWORD)(_tcslen(buf) * sizeof(TCHAR));
                if (!WriteFile(hFile, buf, size, &nBytesWritten, NULL) || (size != nBytesWritten))
                {
                    return FALSE;
                }
                break;
            case VT_STRING:
                if (!WriteString(hFile, pValue->Value.str))
                {
                    return FALSE;
                }
                if (!WriteFile(hFile, _T("\r\n"), 2 * sizeof(TCHAR), &nBytesWritten, NULL) || (2 * sizeof(TCHAR) != nBytesWritten))
                {
                    return FALSE;
                }
                break;
            case VT_INVALID:
                break;
            case VT_BINARY:
            default:
            {
                if (pValue->Type == VT_BINARY)
                {
                    _tcscpy(buf, _T("hex:"));
                }
                else
                {
                    _stprintf(buf, _T("hex(%x):"), pValue->Type); // FIXME_X64 - can't contain x64 value?
                }
                size_t buflen = SizeOf(buf) - _tcslen(buf) - _tcslen(pValue->Name) - 3, srclen = pValue->Size;
                LPTSTR d = buf + _tcslen(buf);
                LPBYTE src = pValue->Value.data;
                DWORD size2, nBytesWritten2;
                if ((long)buflen < 0)
                    buflen = 6; // allow just one sequence on this line...
                while (srclen--)
                {
                    static const TCHAR* hex = _T("0123456789abcdef");
                    *d++ = hex[*src >> 4];
                    *d++ = hex[*src++ & 0x0f];
                    if (srclen)
                    {
                        *d++ = ',';
                        buflen -= 3;
                        if (buflen < 6)
                        {
                            *d++ = '\\';
                            *d++ = '\r';
                            *d++ = '\n';
                            size2 = (DWORD)((d - buf) * sizeof(TCHAR));
                            if (!WriteFile(hFile, buf, size2, &nBytesWritten2, NULL) || (size2 != nBytesWritten2))
                            {
                                return FALSE;
                            }
                            d = buf + 2;
                            buf[0] = buf[1] = ' ';
                            buflen = SizeOf(buf) - 2;
                        }
                    }
                }
                *d++ = '\r';
                *d++ = '\n';
                size2 = (DWORD)((d - buf) * sizeof(TCHAR));
                if (!WriteFile(hFile, buf, size2, &nBytesWritten2, NULL) || (size2 != nBytesWritten2))
                {
                    return FALSE;
                }
                break;
            }
            }
        }
        s -= 2;
        s[-1] = '\\';
        int j;
        for (j = 0; j < SubKeys.Count; j++)
        {
            if (!SubKeys[j]->Dump(hFile, fullKeyName, s, maxlen - (s - name)))
            {
                return FALSE;
            }
        }
        return TRUE;
    } /* CKey::Dump*/

    BOOL CRootKey::Dump(HANDLE hFile, LPCTSTR fullKeyName, LPTSTR name, size_t maxlen)
    {
        int j;
        for (j = 0; j < SubKeys.Count; j++)
        {
            if (!SubKeys[j]->Dump(hFile, fullKeyName, name, maxlen))
            {
                return FALSE;
            }
        }
        return TRUE;
    } /* CRootKey::Dump*/

    BOOL CKey::RemoveHiddenKeysAndValues()
    {
        if (::StrEndsWith(Name, _T(".hidden"), SizeOf(_T(".hidden")) - 1))
        {
            return TRUE; // delete this key
        }
        int j;
        for (j = 0; j < SubKeys.Count; j++)
        {
            if (SubKeys[j]->RemoveHiddenKeysAndValues())
            {
                SubKeys[j]->Release();
                j--;
            }
        }
        int i;
        for (i = 0; i < Values.Count; i++)
        {
            if (::StrEndsWith(Values[i].Name, _T(".hidden"), SizeOf(_T(".hidden")) - 1))
            {
                Values.Delete(i);
                i--;
            }
        }
        return FALSE;
    }

    //////////////////////////// CMemoryRegistry //////////////////////////

    CMemoryRegistry::CMemoryRegistry()
    {
    }

    void CMemoryRegistry::RemoveHiddenKeysAndValues()
    {
        RootKey.RemoveHiddenKeysAndValues();
    }

    void CMemoryRegistry::Release()
    {
        delete this;
    }

    BOOL CMemoryRegistry::Dump(LPCTSTR fileName, LPCTSTR clearKeyName)
    {
        HANDLE hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);

        if (INVALID_HANDLE_VALUE != hFile)
        {
            TCHAR s[10 * REG_MAX_KEY_NAME_LEN];
            DWORD size, nBytesWritten;
#ifdef _UNICODE
            static LPCWSTR hdr = L"\xFEFFWindows Registry Editor Version 5.00\r\n";
#else
            static LPCSTR hdr = "REGEDIT4\r\n";
#endif

            size = (DWORD)(_tcslen(hdr) * sizeof(TCHAR));
            BOOL ret = WriteFile(hFile, hdr, size, &nBytesWritten, NULL) && (size == nBytesWritten);
            if (clearKeyName != NULL && ret)
            {
                _sntprintf(s, SizeOf(s), _T("\r\n[-%s]\r\n"), clearKeyName);
                s[SizeOf(s) - 1] = 0;
                size = (DWORD)(_tcslen(s) * sizeof(TCHAR));
                ret = WriteFile(hFile, s, size, &nBytesWritten, NULL) && (size == nBytesWritten);
            }
            if (ret)
            {
                s[0] = '\r';
                s[1] = '\n';
                s[2] = '[';
                ret &= RootKey.Dump(hFile, s, s + 3, SizeOf(s) - 3);
            }
            CloseHandle(hFile);
            return ret;
        }
        return FALSE;
    }

    BOOL CMemoryRegistry::ClearKey(HKEY key)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        return pKey->Clear();
    }

    BOOL CMemoryRegistry::CreateOpenKey(HKEY key, LPCTSTR name, HKEY& createdKey, BOOL bCreate)
    {
        CKey* pParentKey = NULL;
        LPCTSTR pRootName = NULL;

        switch (reinterpret_cast<ULONG_PTR>(key))
        {
        case reinterpret_cast<ULONG_PTR>(HKEY_CLASSES_ROOT):
            pRootName = _T("HKEY_CLASSES_ROOT");
            break;
        case reinterpret_cast<ULONG_PTR>(HKEY_CURRENT_USER):
            pRootName = _T("HKEY_CURRENT_USER");
            break;
        case reinterpret_cast<ULONG_PTR>(HKEY_LOCAL_MACHINE):
            pRootName = _T("HKEY_LOCAL_MACHINE");
            break;
        case reinterpret_cast<ULONG_PTR>(HKEY_USERS):
            pRootName = _T("HKEY_USERS");
            break;
        case reinterpret_cast<ULONG_PTR>(HKEY_CURRENT_CONFIG):
            pRootName = _T("HKEY_CURRENT_CONFIG");
            break;
        case reinterpret_cast<ULONG_PTR>(HKEY_DYN_DATA):
            pRootName = _T("HKEY_DYN_DATA");
            break;
        case reinterpret_cast<ULONG_PTR>(HKEY_PERFORMANCE_DATA):
            pRootName = _T("HKEY_PERFORMANCE_DATA");
            break;
        case 0:
            // Invalid parent key :-(
            return FALSE;
        default:
            pParentKey = (CKey*)key;
        }
        if (pRootName)
        {
            pParentKey = RootKey.GetKey(pRootName);
            if (!pParentKey)
            {
                if (!bCreate)
                {
                    return FALSE;
                }
                pParentKey = new CKey(&RootKey, pRootName);
                if (!pParentKey)
                {
                    // OOM :-(
                    return FALSE;
                }
            }
        }

        if (*name == '\\')
        {
            // invalid key name (it cannot start with '\\')
            return FALSE;
        }
        while (*name)
        {
            TCHAR keyName[REG_MAX_KEY_NAME_LEN], *s;

            s = keyName;
            while (*name)
            {
                if (*name == '\\')
                {
                    name++;
                    break;
                }
                if (s - keyName < SizeOf(keyName))
                {
                    *s++ = *name++;
                    continue;
                }
                // Too long key name :-(
                return FALSE;
            }
            *s = 0;
            if (keyName[0] != 0)
            { // ignore doubled '\\' in 'name'
                CKey* pKey = pParentKey->GetKey(keyName);
                if (!pKey)
                {
                    if (!bCreate)
                    {
                        return FALSE;
                    }
                    pKey = new CKey(pParentKey, keyName);
                    if (!pKey)
                    {
                        // OOM :-(
                        return FALSE;
                    }
                }
                pParentKey = pKey;
            }
        }
        pParentKey->AddRef();
        createdKey = (HKEY)pParentKey;

        return TRUE;
    }

    BOOL CMemoryRegistry::CreateKey(HKEY key, LPCTSTR name, HKEY& createdKey)
    {
        return CreateOpenKey(key, name, createdKey, TRUE);
    }

    BOOL CMemoryRegistry::OpenKey(HKEY key, LPCTSTR name, HKEY& openedKey)
    {
        return CreateOpenKey(key, name, openedKey, FALSE);
    }

    void CMemoryRegistry::CloseKey(HKEY key)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return;
        pKey->Release();
    }

    BOOL CMemoryRegistry::DeleteKey(HKEY key, LPCTSTR name)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        // Get the subkey
        pKey = pKey->GetKey(name);
        if (!pKey)
            return FALSE;

        // The key will be deleted when its counter reaches zero when all handles are closed
        pKey->Release();

        return TRUE;
    }

    BOOL CMemoryRegistry::GetValue(HKEY key, LPCTSTR name, DWORD type, LPVOID buffer, DWORD bufferSize)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        CValue* pValue = pKey->GetValue(name);
        if (pValue)
        {
            if (pValue->Type != (eVT_VALUE_TYPE)type)
                return FALSE;
            switch (pValue->Type)
            {
            case VT_DWORD:
                if (bufferSize != sizeof(DWORD))
                    return FALSE;
                *(DWORD*)buffer = pValue->Value.dw;
                break;
            case VT_STRING:
                if (bufferSize < pValue->Size)
                    return FALSE;
                _tcscpy((LPTSTR)buffer, pValue->Value.str);
                break;
            case VT_INVALID:
                return FALSE;
            case VT_BINARY:
            default:
                if (bufferSize < pValue->Size)
                    return FALSE;
                memcpy(buffer, pValue->Value.data, pValue->Size);
                break;
            }
            return TRUE;
        }

        return FALSE;
    }

    BOOL CMemoryRegistry::SetValue(HKEY key, LPCTSTR name, DWORD type, LPCVOID data, DWORD dataSize)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        CValue* pValue = pKey->GetValue(name);
        if (pValue)
        {
            pKey->RemoveValue(pValue);
        }

        CValue value(name, (eVT_VALUE_TYPE)type, data, dataSize);
        if (value.IsOK())
        {
            pKey->Values.Add(value);
            value.Invalidate(); // All pointers were copied in Add() -> do not free them now
            return TRUE;
        }
        // Out of memory :-(

        return FALSE;
    }

    BOOL CMemoryRegistry::DeleteValue(HKEY key, LPCTSTR name)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        CValue* pValue = pKey->GetValue(name);
        if (pValue)
        {
            pKey->RemoveValue(pValue);
            return TRUE;
        }

        return FALSE;
    }

    BOOL CMemoryRegistry::GetSize(HKEY key, LPCTSTR name, DWORD type, DWORD& bufferSize)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        CValue* pValue = pKey->GetValue(name);
        if (pValue)
        {
            if (pValue->Type != (eVT_VALUE_TYPE)type)
                return FALSE;
            switch (pValue->Type)
            {
            case VT_DWORD:
                bufferSize = sizeof(DWORD);
                break;
            case VT_INVALID:
                return FALSE;
            case VT_STRING:
            case VT_BINARY:
            default:
                bufferSize = pValue->Size;
                break;
            }
            return TRUE;
        }

        return FALSE;
    }

    BOOL CMemoryRegistry::EnumKey(HKEY key, DWORD subKeyIndex, LPTSTR name, DWORD bufferSize)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey || !name || !bufferSize)
            return FALSE;

        if ((int)subKeyIndex >= pKey->SubKeys.Count)
            return FALSE;

        CKey* pSubKey = pKey->SubKeys[subKeyIndex];

        _tcsncpy_s(name, bufferSize, pSubKey->Name, _TRUNCATE);

        return TRUE;
    }

    BOOL CMemoryRegistry::EnumValue(HKEY key, DWORD valIndex, LPTSTR name, DWORD nameSize, LPDWORD valType, LPBYTE data, LPDWORD dataSize)
    {
        CKey* pKey = (CKey*)key;

        if (!pKey)
            return FALSE;

        if (!pKey || !name || !nameSize)
            return FALSE;

        if ((int)valIndex >= pKey->Values.Count)
            return FALSE;

        CValue* pValue = &pKey->Values[valIndex];

        if (data && dataSize)
        {
            DWORD size;
            LPCVOID srcData;

            switch (pValue->Type)
            {
            case VT_DWORD:
                size = sizeof(DWORD);
                srcData = &pValue->Value.dw;
                break;
            case VT_INVALID:
                size = 0;
                srcData = NULL;
                break;
            case VT_STRING:
                size = pValue->Size;
                srcData = pValue->Value.str;
                break;
            case VT_BINARY:
            default:
                size = pValue->Size;
                srcData = pValue->Value.data;
                break;
            }

            if (*dataSize < size)
                return FALSE;
            memcpy(data, srcData, size);
            *dataSize = size;
        }

        _tcsncpy_s(name, nameSize, pValue->Name, _TRUNCATE);

        if (valType)
            *valType = pValue->Type;
        return TRUE;
    }

} // namespace RegLib

BOOL StrEndsWith(LPCTSTR txt, LPCTSTR pattern, size_t patternLen)
{
    if (txt == NULL || pattern == NULL)
        return FALSE;
    size_t txtLen = _tcslen(txt);
    return txtLen >= patternLen && _tcsicmp(txt + txtLen - patternLen, pattern) == 0;
}

CSalamanderRegistryExAbstract* REG_MemRegistryFactory()
{
    return new RegLib::CMemoryRegistry();
}
