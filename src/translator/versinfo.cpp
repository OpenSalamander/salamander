// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "versinfo.h"

//*****************************************************************************
//
// CVersionBlock
//

CVersionBlock::CVersionBlock()
    : Children(5, 5)
{
    Type = vbtUNKNOWN;
    Key = NULL;
    Text = FALSE;
    Value = NULL;
    ValueSize = 0;
}

CVersionBlock::~CVersionBlock()
{
    if (Key != NULL)
        free(Key);
    if (Value != NULL)
        free(Value);
}

BOOL CVersionBlock::SetKey(const WCHAR* key)
{
    if (Key != NULL)
        free(Key);
    int size = (wcslen(key) + 1) * sizeof(WCHAR);
    Key = (WCHAR*)malloc(size);
    if (Key == NULL)
    {
        TRACE_E("LOW MEMORY");
        return FALSE;
    }
    memcpy(Key, key, size);
    return TRUE;
}

BOOL CVersionBlock::SetValue(const BYTE* value, int size)
{
    Value = malloc(size == 0 ? sizeof(WCHAR) : size);
    if (Value == NULL)
    {
        TRACE_E("LOW MEMORY");
        return FALSE;
    }
    if (size == 0)
        *(WCHAR*)Value = 0;
    else
        memcpy(Value, value, size);
    return TRUE;
}

BOOL CVersionBlock::AddChild(CVersionBlock* block)
{
    Children.Add(block);
    if (!Children.IsGood())
    {
        TRACE_E("LOW MEMORY");
        Children.ResetState();
        return FALSE;
    }
    return TRUE;
}

//*****************************************************************************
//
// CVersionInfo
//

#define ALIGN_DWORD(type, p) ((type)(((DWORD)p + 3) & ~3))

CVersionInfo::CVersionInfo()
{
    Root = NULL;
    TLangID = 0;
}

CVersionInfo::~CVersionInfo()
{
    if (Root != NULL)
        delete Root;
}

CVersionBlock*
CVersionInfo::LoadBlock(BYTE*& ptr, CVersionBlock* parent)
{
    VERSIONINFO* info = (VERSIONINFO*)ptr;
    if (info->wType != 0 && info->wType != 1)
    {
        TRACE_E("Unknown wType=" << info->wType);
        return NULL;
    }

    const BYTE* terminatorPtr = (BYTE*)info + info->wLength;

    CVersionBlock* block = new CVersionBlock();
    if (block == NULL)
    {
        TRACE_E("LOW MEMORY");
        return NULL;
    }
    block->Text = info->wType == 1 ? TRUE : FALSE;
    if (!block->SetKey(info->szKey))
    {
        TRACE_E("LOW MEMORY");
        delete block;
        return NULL;
    }
    block->ValueSize = info->wValueLength;

    if (parent == NULL)
        block->Type = vbtVersionInfo;
    else
    {
        switch (parent->Type)
        {
        case vbtVersionInfo:
        {
            if (wcscmp(block->Key, L"StringFileInfo") == 0)
                block->Type = vbtStringFileInfo;
            else if (wcscmp(block->Key, L"VarFileInfo") == 0)
                block->Type = vbtVarFileInfo;
            else
            {
                TRACE_EW(L"Unknown block key=" << block->Key);
                delete block;
                return NULL;
            }
            break;
        }

        case vbtStringFileInfo:
        {
            block->Type = vbtStringTable;
            break;
        }

        case vbtStringTable:
        {
            block->Type = vbtString;
            break;
        }

        case vbtVarFileInfo:
        {
            block->Type = vbtVar;
            break;
        }

        default:
        {
            TRACE_E("Unknown block type=" << parent->Type);
            delete block;
            return NULL;
        }
        }
    }

    // preskocime VERSIONINFO a retezec Key
    ptr += sizeof(VERSIONINFO) + sizeof(WCHAR) * wcslen(block->Key);
    // preskocime Padding
    ptr = ALIGN_DWORD(BYTE*, ptr);

    switch (block->Type)
    {
    case vbtVersionInfo:
    case vbtString:
    case vbtVar:
    {
        int valSize = info->wValueLength;

        if (block->Type == vbtVersionInfo) // udelame par kontrol konzistence dat
        {
            if (valSize != sizeof(VS_FIXEDFILEINFO))
            {
                TRACE_E("valSize != sizeof(VS_FIXEDFILEINFO); valSize=" << valSize);
                delete block;
                return FALSE;
            }
            if (((VS_FIXEDFILEINFO*)ptr)->dwSignature != 0xFEEF04BD)
            {
                TRACE_E("Unknown dwSignature: 0x" << std::hex << ((VS_FIXEDFILEINFO*)ptr)->dwSignature << std::dec);
                delete block;
                return FALSE;
            }
        }

        if (block->Type == vbtString)
            valSize *= 2;
        if (!block->SetValue(ptr, valSize))
        {
            delete block;
            return NULL;
        }

        // preskocime Value
        ptr += valSize;
        // preskocime padding
        ptr = ALIGN_DWORD(BYTE*, ptr);

        if (block->Type == vbtString || block->Type == vbtVar)
        {
            // String a Var nemaji childy, takze vypadneme
            return block;
        }
    }
    }

    // pridame child bloky
    while (ptr < terminatorPtr)
    {
        CVersionBlock* child = LoadBlock(ptr, block);
        if (child == NULL)
        {
            delete block;
            return NULL;
        }

        if (!block->AddChild(child))
        {
            delete child;
            delete block;
            return NULL;
        }
    }

    return block;
}

BOOL CALLBACK EnumResLangProc(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName,
                              WORD wIDLanguage, LONG_PTR lParam);

BOOL CVersionInfo::ReadResource(HINSTANCE hInstance, int resID)
{
    if (Root != NULL)
    {
        delete Root;
        Root = NULL;
    }
    DWORD langID = 0xFFFFFFFF;
    EnumResourceLanguages(hInstance, RT_VERSION, MAKEINTRESOURCE(resID),
                          (ENUMRESLANGPROC)EnumResLangProc, (LONG_PTR)&langID);
    if (langID == 0xFFFFFFFF)
    {
        char errtext[3000];
        sprintf_s(errtext, "Multiple language resources for versioninfo resID: %d.", resID);
        MessageBox(GetMsgParent(), errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    TLangID = (WORD)langID;
    HRSRC hrsrc = FindResource(hInstance, MAKEINTRESOURCE(resID), RT_VERSION);
    if (hrsrc != NULL)
    {
        int size = SizeofResource(hInstance, hrsrc);
        if (size > 0)
        {
            HGLOBAL hglb = LoadResource(hInstance, hrsrc);
            if (hglb != NULL)
            {
                LPVOID data = LockResource(hglb);
                if (data != NULL)
                {
                    BYTE* ptr = (BYTE*)data;
                    Root = LoadBlock(ptr, NULL);
                }
            }
        }
    }
    else
        TRACE_E("CVersionInfo::ReadResource() FindResource cannot find resID=" << resID);
    return Root != NULL;
}

CVersionBlock*
CVersionInfo::FindBlock(const char* block)
{
    if (Root == NULL)
    {
        TRACE_E("CVersionInfo::QueryValue() Root==NULL!");
        return FALSE;
    }

    char tmp[2048];
    lstrcpyn(tmp, block, 2048);

    if (strcmp(tmp, "\\") == 0)
        return Root;

    CVersionBlock* blockIter = Root;
    char* next_token = NULL;
    char* token = strtok_s(tmp, "\\", &next_token);
    while (token != NULL)
    {
        WCHAR wideToken[2048];
        MultiByteToWideChar(CP_ACP, 0, token, -1, wideToken, sizeof(wideToken));
        CVersionBlock* found = NULL;
        for (int i = 0; i < blockIter->Children.Count; i++)
        {
            CVersionBlock* child = blockIter->Children[i];
            if (wcscmp(child->Key, wideToken) == 0)
            {
                found = child;
                break;
            }
        }
        if (found != NULL)
            blockIter = found;
        else
            return FALSE;

        token = strtok_s(NULL, "\\", &next_token);
    }
    return blockIter;
}

BOOL CVersionInfo::QueryValue(const char* block, BYTE** buffer, DWORD* size)
{
    CVersionBlock* found = FindBlock(block);

    if (found == NULL)
        return FALSE;

    if (found == Root)
    {
        *buffer = (BYTE*)Root->Value;
        *size = sizeof(VS_FIXEDFILEINFO);
        return TRUE;
    }

    switch (found->Type)
    {
    case vbtString:
    {
        *buffer = (BYTE*)found->Value;
        *size = wcslen((WCHAR*)found->Value);
        return TRUE;
    }

    case vbtVar:
    {
        *buffer = (BYTE*)found->Value;
        *size = found->ValueSize;
        return TRUE;
    }
    }
    TRACE_E("Wrong block type=" << found->Type);
    return FALSE;
}

BOOL CVersionInfo::QueryString(const char* block, wchar_t* buffer, DWORD maxSize)
{
    BYTE* bf;
    DWORD sz;
    if (QueryValue(block, &bf, &sz))
    {
        wcsncpy_s(buffer, maxSize, (wchar_t*)bf, sz + 1);
        return TRUE;
    }
    return FALSE;
}

#ifdef VERSINFO_SUPPORT_WRITE

BOOL CVersionInfo::SetString(const char* block, const wchar_t* buffer)
{
    CVersionBlock* found = FindBlock(block);

    if (found == NULL)
    {
        TRACE_EW(L"Cannot find block " << buffer);
        return FALSE;
    }

    if (found->Type != vbtString)
    {
        TRACE_E("Wrong block type=" << found->Type);
        return FALSE;
    }

    int len = lstrlenW(buffer);
    WCHAR* str = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
    if (str == NULL)
    {
        TRACE_E("LOW MEMORY");
        return FALSE;
    }

    wcsncpy_s(str, len + 1, buffer, len + 1);

    free(found->Value);
    found->Value = str;

    return TRUE;
}

BOOL CVersionInfo::SaveBlock(CVersionBlock* block, BYTE*& ptr, const BYTE* maxPtr)
{
    if (ptr + 10 > maxPtr)
    {
        TRACE_E("Buffer too small!");
        return FALSE;
    }

    BYTE* oldPtr = ptr; // ulozim si pro nasledny vypocet velikosti nas a nasich childu

    // wLength
    WORD* wLength = (WORD*)ptr; // ulozim si pro nasledne nastaveni
    ptr += 2;

    // wValueLength
    WORD* wValueLength = (WORD*)ptr; // ulozim si pro nasledne nastaveni
    *wValueLength = 0;
    ptr += 2;

    // wType
    *((WORD*)ptr) = (block->Text) ? 1 : 0;
    ptr += 2;

    // szKey
    int size = (wcslen(block->Key) + 1) * sizeof(WCHAR);
    memcpy(ptr, block->Key, size);
    ptr += size;

    // padding
    ptr = ALIGN_DWORD(BYTE*, ptr);

    switch (block->Type)
    {
    case vbtVersionInfo:
    {
        if (ptr + sizeof(VS_FIXEDFILEINFO) + 4 > maxPtr)
        {
            TRACE_E("Buffer too small!");
            return FALSE;
        }

        size = sizeof(VS_FIXEDFILEINFO);
        *wValueLength = size;
        memcpy(ptr, block->Value, size);
        ptr += size;
        break;
    }

    case vbtString:
    {
        int strLen = wcslen((WCHAR*)block->Value);
        size = (strLen + 1) * sizeof(WCHAR);

        if (ptr + size + 4 > maxPtr)
        {
            TRACE_E("Buffer too small!");
            return FALSE;
        }

        *wValueLength = strLen + 1;
        memcpy(ptr, block->Value, size);
        ptr += size;
        break;
    }

    case vbtVar:
    {
        size = block->ValueSize;

        if (ptr + size + 4 > maxPtr)
        {
            TRACE_E("Buffer too small!");
            return FALSE;
        }

        *wValueLength = size;
        memcpy(ptr, block->Value, size);
        ptr += size;
        break;
    }
    }

    // pokud nemam childy, ulozime velikost bez paddingu
    if (block->Children.Count == 0)
        *wLength = ptr - oldPtr;

    // padding
    ptr = ALIGN_DWORD(BYTE*, ptr);

    // children
    for (int i = 0; i < block->Children.Count; i++)
    {
        CVersionBlock* child = block->Children[i];
        if (!SaveBlock(child, ptr, maxPtr))
            return FALSE;
    }

    // v opacnem pripad s paddingem
    if (block->Children.Count > 0)
        *wLength = ptr - oldPtr;

    return TRUE;
}

BOOL CVersionInfo::UpdateResource(HANDLE hUpdateRes, int resID)
{
    BYTE* buff = (BYTE*)malloc(50000);
    if (buff == NULL)
    {
        TRACE_E("LOW MEMORY");
        return FALSE;
    }
    memset(buff, 0, 50000);
    BYTE* ptr = buff; // pozor, hodnota bude zmenena
    if (!SaveBlock(Root, ptr, ptr + 49999))
    {
        free(buff);
        return FALSE;
    }
    DWORD resSize = ptr - buff;

    BOOL result = TRUE;
    if (TLangID != MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)) // resource neni "neutral", musime ho smaznout, aby ve vyslednem .SLG nebyly version-infa dve
    {
        result = ::UpdateResource(hUpdateRes, RT_VERSION, MAKEINTRESOURCE(resID),
                                  TLangID, NULL, 0);
    }
    if (result)
    {
        result = ::UpdateResource(hUpdateRes, RT_VERSION, MAKEINTRESOURCE(resID),
                                  MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                                  buff, resSize);
    }
    free(buff);
    return result;
}

#endif //VERSINFO_SUPPORT_WRITE

#ifdef VERSINFO_SUPPORT_DEBUG
BOOL CVersionInfo::WriteResourceToFile(HINSTANCE hInstance, int resID, const char* fileName)
{
    BOOL ret = FALSE;
    HRSRC hrsrc = FindResource(hInstance, MAKEINTRESOURCE(resID), RT_VERSION);
    if (hrsrc != NULL)
    {
        DWORD size = SizeofResource(hInstance, hrsrc);
        if (size > 0)
        {
            HGLOBAL hglb = LoadResource(hInstance, hrsrc);
            if (hglb != NULL)
            {
                LPVOID data = LockResource(hglb);
                if (data != NULL)
                {
                    BYTE* ptr = (BYTE*)data;
                    HANDLE file = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (file != INVALID_HANDLE_VALUE)
                    {
                        DWORD written;
                        if (WriteFile(file, ptr, size, &written, NULL) && written == size)
                            ret = TRUE;
                        CloseHandle(file);
                    }
                }
            }
        }
    }
    return ret;
}
#endif //VERSINFO_SUPPORT_DEBUG
