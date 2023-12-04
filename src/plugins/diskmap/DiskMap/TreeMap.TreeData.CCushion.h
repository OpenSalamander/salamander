// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.FileData.CZFile.h"
#include "TreeMap.TreeData.CCushionHitInfo.h"
#include "TreeMap.Graphics.CCushionGraphics.h"

class CCushionDirectory;
class CCushionHitInfo;
class CZFile;

class CCushion
{
    friend CCushionRow;

protected:
    CZFile* _file;
    INT64 _datasize;
    COLORREF _color;

    int _size;
    CCushionDirectory* _parent;

    CCushion* _next;

public:
    CCushion(CZFile* file, CCushionDirectory* parent, int sizeid)
    {
        this->_file = file;
        this->_datasize = file->GetSizeEx(sizeid);
        this->_color = RGB(0, 64, 128);
        this->_parent = parent;
        this->_next = NULL;
    }
    ~CCushion()
    {
        //empty - all handled by the CRegionAllocator
    }
    void* operator new(size_t stAllocateBlock, CRegionAllocator* mem)
    {
        return mem->Allocate(stAllocateBlock);
    }

    void operator delete(void* pvMem, CRegionAllocator* mem)
    {
        //empty - all handled by the CRegionAllocator
    }

    CCushion* GetNext() { return this->_next; }
    void SetNext(CCushion* cs) { this->_next = cs; }

    virtual void SetBounds(int x, int y, int w, int h) {}
    virtual void FillFiles(CRegionAllocator* alloc, CTreeMapRendererBase* rdr, int level, EDirection parentdir) {}
    int GetSize() const { return this->_size; }
    INT64 GetDataSize() const { return this->_datasize; }

    COLORREF GetColor() const { return this->_color; }
    void SetColor(COLORREF col) { this->_color = col; }

    CCushionDirectory* GetParent() { return this->_parent; }

    CZFile* GetFile() { return this->_file; }

    virtual CCushionHitInfo* GetHitInfo(int x, int y, int cx, int cy, int cw, int ch, CCushionHitInfo* parent)
    {
        return new CCushionHitInfo(this, parent, cx, cy, cw, ch);
    }
    virtual BOOL IsDirectory() { return FALSE; }

    virtual COLORREF Colorize(CCushion* cs)
    {
        CZFile* f = cs->GetFile();
        //COLORREF c = 0;//black?
        TCHAR* e = f->GetExt();
        int r = 0, g = 0, b = 0;
        if (e != NULL)
        {
            e++; //za tecku
            if (*e != '\0')
            {
                if ((*e >= '0') && (*e <= '9'))
                    r = (*e - '0' + 1) * 25;
                else if ((*e >= 'a') && (*e <= 'z'))
                    r = (*e - 'a' + 1) * 9;
                else if ((*e >= 'A') && (*e <= 'Z'))
                    r = (*e - 'A' + 1) * 9;
                else
                    r = 255;
                e++;
            }
            if (*e != '\0')
            {
                if ((*e >= '0') && (*e <= '9'))
                    g = (*e - '0' + 1) * 25;
                else if ((*e >= 'a') && (*e <= 'z'))
                    g = (*e - 'a' + 1) * 9;
                else if ((*e >= 'A') && (*e <= 'Z'))
                    g = (*e - 'A' + 1) * 9;
                else
                    g = 255;
                e++;
            }
            if (*e != '\0')
            {
                if ((*e >= '0') && (*e <= '9'))
                    b = (*e - '0' + 1) * 25;
                else if ((*e >= 'a') && (*e <= 'z'))
                    b = (*e - 'a' + 1) * 9;
                else if ((*e >= 'A') && (*e <= 'Z'))
                    b = (*e - 'A' + 1) * 9;
                else
                    b = 255;
                e++;
            }
        }
        return RGB(r, g, b);
    }
};
