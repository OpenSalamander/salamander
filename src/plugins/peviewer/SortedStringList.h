// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: noexpandtab:sw=8:ts=8

/*
	PE File Viewer Plugin for Open Salamander

	Copyright (c) 2015-2016 Milan Kase <manison@manison.cz>
	Copyright (c) 2015-2016 Open Salamander Authors

	SortedStringList.h
	Sorted string list.
*/

#pragma once

#include "precomp.h"

#if _MSC_VER < 1600 // ve VC2008 (pred Visual Studio 2010) neexistuje nullptr, tady by melo stacit tohle:
#define nullptr 0
#endif

template <typename TChar, class TTraits>
class CSortedStringListT
{
public:
    typedef TChar XCHAR;
    typedef int (*PFN_Comparer)(const XCHAR*, const XCHAR*);
    typedef void* HITEM;

private:
    typedef struct _LIST_ENTRY
    {
        struct LIST_ENTRY* pNext;
        DWORD cch;
        XCHAR sz[ANYSIZE_ARRAY];
    } LIST_ENTRY;

    LIST_ENTRY* m_pHead;
    PFN_Comparer m_pfnComparer;

    void Init(PFN_Comparer pfnComparer)
    {
        _ASSERTE(pfnComparer != nullptr);
        m_pfnComparer = pfnComparer;
        m_pHead = nullptr;
    }

public:
    CSortedStringListT(PFN_Comparer pfnComparer)
    {
        Init(pfnComparer);
    }

    CSortedStringListT()
    {
        Init(TTraits::Compare);
    }

    ~CSortedStringListT()
    {
        LIST_ENTRY* pNext;
        LIST_ENTRY* pEntry = m_pHead;

        while (pEntry != nullptr)
        {
            pNext = pEntry->pNext;
            free(pEntry);
            pEntry = pNext;
        }
    }

    /**
		Allocates string item with specified capacity.
		\param cchCapacity Maximum length of the string. The capacity
		       includes the nul-terminating character.
	*/
    HITEM Alloc(DWORD cchCapacity)
    {
        _ASSERTE(cchCapacity > 0);
        LIST_ENTRY* pEntry;
        size_t cb = sizeof(LIST_ENTRY) + (cchCapacity - ANYSIZE_ARRAY) * sizeof(XCHAR);
        pEntry = reinterpret_cast<LIST_ENTRY*>(malloc(cb));
        pEntry->pNext = nullptr;
        pEntry->cch = cchCapacity;
        return reinterpret_cast<HITEM>(pEntry);
    }

    /**
		Gets the string buffer of item allocated by previous call
		to the Alloc method.
	*/
    XCHAR* GetBuffer(HITEM hItem, _Out_ DWORD& cchCapacity)
    {
        _ASSERTE(hItem != nullptr);
        LIST_ENTRY* pEntry = reinterpret_cast<LIST_ENTRY*>(hItem);
        cchCapacity = pEntry->cch;
        return pEntry->sz;
    }

    const XCHAR* GetBuffer(HITEM hItem) const
    {
        _ASSERTE(hItem != nullptr);
        LIST_ENTRY* pEntry = reinterpret_cast<LIST_ENTRY*>(hItem);
        return pEntry->sz;
    }

    /**
		Frees the item allocated by previous call to the Alloc method.
		The item must not be inserted into the list.
	*/
    void Free(HITEM hItem)
    {
        _ASSERTE(hItem != nullptr);
        free(hItem);
    }

    void Insert(HITEM hItem)
    {
        _ASSERTE(hItem != nullptr);

        LIST_ENTRY* pEntry = reinterpret_cast<LIST_ENTRY*>(hItem);
        if (m_pHead == nullptr || m_pfnComparer(pEntry->sz, m_pHead->sz) < 0)
        {
            pEntry->pNext = m_pHead;
            m_pHead = pEntry;
        }
        else
        {
            LIST_ENTRY* pIter = m_pHead;
            LIST_ENTRY* pPrev;
            while (pIter != nullptr && m_pfnComparer(pEntry->sz, pIter->sz) >= 0)
            {
                pPrev = pIter;
                pIter = pIter->pNext;
            }

            pEntry->pNext = pPrev->pNext;
            pPrev->pNext = pEntry;
        }
    }

    bool Next(_Inout_ HITEM& hItem)
    {
        LIST_ENTRY* pEntry = reinterpret_cast<LIST_ENTRY*>(hItem);

        if (pEntry == nullptr)
        {
            pEntry = m_pHead;
        }
        else
        {
            pEntry = pEntry->pNext;
        }

        hItem = reinterpret_cast<HITEM>(pEntry);

        return pEntry != nullptr;
    }
};

class CSortedStringListTraitsA
{
public:
    static inline int Compare(const CHAR* s1, const CHAR* s2)
    {
        return strcmp(s1, s2);
    }

    static inline int CompareNoCase(const CHAR* s1, const CHAR* s2)
    {
        return _stricmp(s1, s2);
    }
};

class CSortedStringListTraitsW
{
public:
    static inline int Compare(const WCHAR* s1, const WCHAR* s2)
    {
        return wcscmp(s1, s2);
    }

    static inline int CompareNoCase(const WCHAR* s1, const WCHAR* s2)
    {
        return _wcsicmp(s1, s2);
    }
};

typedef CSortedStringListT<CHAR, CSortedStringListTraitsA> CSortedStringListA;
typedef CSortedStringListT<WCHAR, CSortedStringListTraitsW> CSortedStringListW;
