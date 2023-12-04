// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.FileData.CZDirectory.h"
#include "TreeMap.TreeData.CCushionDirectory.h"

class CZFile;
class CZDirectory;
class CCushion;
class CCushionRow;
class CCushionDirectory;

class CTreeMap
{
protected:
    CZDirectory* _root;
    CCushionDirectory* _cshroot;
    CTreeMapRendererBase* _renderer;

    CRegionAllocator* _allocator;

    int _w, _h;

public:
    CTreeMap(CRegionAllocator* alloc, CZDirectory* dir, CTreeMapRendererBase* renderer, int w, int h)
    {
        this->_root = dir;
        this->_cshroot = NULL;
        this->_renderer = renderer;

        this->_allocator = alloc;

        this->_w = w;
        this->_h = h;
    }
    ~CTreeMap()
    {
        this->_allocator->FreeAll(FALSE);
    }
    CCushionDirectory* GetRoot()
    {
        return this->_cshroot;
    }
    BOOL Prepare(int sortorder)
    {
        if (this->_cshroot != NULL)
            this->_allocator->FreeAll(FALSE);
        this->_cshroot = new (this->_allocator) CCushionDirectory(this->_root, NULL, sortorder);
        this->_cshroot->SetBounds(0, 0, this->_w, this->_h);
        this->_cshroot->FillFiles(this->_allocator, this->_renderer, 0, dirNULL);
        return TRUE;
    }
    CCushionHitInfo* GetCushionByLocation(int x, int y)
    {
        if (this->_cshroot)
            return this->_cshroot->GetCushionByLocation(x, y, NULL);
        return NULL;
    }
};
