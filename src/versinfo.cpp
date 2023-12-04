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
    CALL_STACK_MESSAGE2("CVersionBlock::SetKey(len=%Iu)", wcslen(key));

    if (Key != NULL)
        free(Key);
    int size = ((int)wcslen(key) + 1) * sizeof(WCHAR);
    Key = (WCHAR*)malloc(size);
    if (Key == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    memcpy(Key, key, size);
    return TRUE;
}

BOOL CVersionBlock::SetValue(const BYTE* value, int size)
{
    CALL_STACK_MESSAGE2("CVersionBlock::SetValue(, %d)", size);

    Value = malloc(size);
    if (Value == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    memcpy(Value, value, size);
    return TRUE;
}

BOOL CVersionBlock::AddChild(CVersionBlock* block)
{
    CALL_STACK_MESSAGE1("CVersionBlock::AddChild()");

    Children.Add(block);
    if (!Children.IsGood())
    {
        TRACE_E(LOW_MEMORY);
        Children.ResetState();
        return FALSE;
    }
    return TRUE;
}

//*****************************************************************************
//
// CVersionInfo
//

#define ALIGN_DWORD(type, p) ((type)(((DWORD_PTR)p + 3) & ~3))

CVersionInfo::CVersionInfo()
{
    Root = NULL;
}

CVersionInfo::~CVersionInfo()
{
    if (Root != NULL)
        delete Root;
}

CVersionBlock*
CVersionInfo::LoadBlock(const BYTE*& ptr, CVersionBlock* parent)
{
    CALL_STACK_MESSAGE7("CVersionInfo::LoadBlock(0x%p,): len=%u (%c%c%c%c...)", ptr,
                        ((VERSIONINFO*)ptr)->wLength,
                        (((VERSIONINFO*)ptr)->szKey[0] != 0 ? ((char*)((VERSIONINFO*)ptr)->szKey)[0] : ' '),
                        (((VERSIONINFO*)ptr)->szKey[0] != 0 && ((VERSIONINFO*)ptr)->szKey[1] != 0 ? ((char*)((VERSIONINFO*)ptr)->szKey)[2] : ' '),
                        (((VERSIONINFO*)ptr)->szKey[0] != 0 && ((VERSIONINFO*)ptr)->szKey[1] != 0 && ((VERSIONINFO*)ptr)->szKey[2] != 0 ? ((char*)((VERSIONINFO*)ptr)->szKey)[4] : ' '),
                        (((VERSIONINFO*)ptr)->szKey[0] != 0 && ((VERSIONINFO*)ptr)->szKey[1] != 0 && ((VERSIONINFO*)ptr)->szKey[2] != 0 && ((VERSIONINFO*)ptr)->szKey[3] != 0 ? ((char*)((VERSIONINFO*)ptr)->szKey)[6] : ' '));

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
        TRACE_E(LOW_MEMORY);
        return NULL;
    }
    block->Text = info->wType == 1 ? TRUE : FALSE;
    if (!block->SetKey(info->szKey))
    {
        TRACE_E(LOW_MEMORY);
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

BOOL CVersionInfo::ReadResource(HINSTANCE hInstance, int resID)
{
    CALL_STACK_MESSAGE3("CVersionInfo::ReadResource(0x%p, %d)", hInstance, resID);

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
                    const BYTE* ptr = (const BYTE*)data;
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
    char* token = strtok(tmp, "\\");
    while (token != NULL)
    {
        WCHAR wideToken[2048];
        MultiByteToWideChar(CP_ACP, 0, token, -1, wideToken, sizeof(wideToken) / sizeof(WCHAR));
        wideToken[sizeof(wideToken) / sizeof(WCHAR) - 1] = 0;
        CVersionBlock* found = NULL;
        int i;
        for (i = 0; i < blockIter->Children.Count; i++)
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

        token = strtok(NULL, "\\");
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
        if (found->ValueSize > 0)
            *size = (int)wcslen((WCHAR*)found->Value);
        else
            *size = 0;
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

BOOL CVersionInfo::QueryString(const char* block, char* buffer, DWORD maxSize, WCHAR* bufferW, DWORD maxSizeW)
{
    BYTE* bf;
    DWORD sz;
    if (QueryValue(block, &bf, &sz))
    {
        if (maxSizeW > 0 && bufferW != NULL)
            lstrcpynW(bufferW, (WCHAR*)bf, min(sz + 1, maxSizeW));

        if (maxSize > 0 && buffer != NULL)
        {
            WideCharToMultiByte(CP_ACP, 0, (WCHAR*)bf, sz, buffer, maxSize, NULL, NULL);
            if (sz < maxSize)
                buffer[sz] = 0;
            buffer[maxSize - 1] = 0;
        }
        return TRUE;
    }
    return FALSE;
}

#ifdef VERSINFO_SUPPORT_WRITE

BOOL CVersionInfo::SetString(const char* block, const char* buffer)
{
    CVersionBlock* found = FindBlock(block);

    if (found == NULL)
    {
        TRACE_E("Cannot find block " << buffer);
        return FALSE;
    }

    if (found->Type != vbtString)
    {
        TRACE_E("Wrong block type=" << found->Type);
        return FALSE;
    }

    int len = (int)strlen(buffer);
    WCHAR* str = (WCHAR*)malloc((len + 1) * 2);
    if (str == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    MultiByteToWideChar(CP_ACP, 0, buffer, len + 1, str, len + 1);
    str[len] = 0;

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
    int size = ((int)wcslen(block->Key) + 1) * sizeof(WCHAR);
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
        int strLen = (int)wcslen((WCHAR*)block->Value);
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
        *wLength = (WORD)(ptr - oldPtr);

    // padding
    ptr = ALIGN_DWORD(BYTE*, ptr);

    // children
    int i;
    for (i = 0; i < block->Children.Count; i++)
    {
        CVersionBlock* child = block->Children[i];
        if (!SaveBlock(child, ptr, maxPtr))
            return FALSE;
    }

    // v opacnem pripad s paddingem
    if (block->Children.Count > 0)
        *wLength = (WORD)(ptr - oldPtr);

    return TRUE;
}

BOOL CVersionInfo::UpdateResource(HANDLE hUpdateRes, int resID)
{
    BYTE* buff = (BYTE*)malloc(50000);
    if (buff == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    memset(buff, 0, 50000);
    BYTE* ptr = buff; // pozor, hodnota bude zmenena
    if (!SaveBlock(Root, ptr, ptr + 49999))
    {
        free(buff);
        return FALSE;
    }
    DWORD resSize = (DWORD)(ptr - buff);

    if (!::UpdateResource(hUpdateRes, RT_VERSION, MAKEINTRESOURCE(resID),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                          buff, resSize))
    {
        free(buff);
        return FALSE;
    }
    free(buff);
    return TRUE;
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
