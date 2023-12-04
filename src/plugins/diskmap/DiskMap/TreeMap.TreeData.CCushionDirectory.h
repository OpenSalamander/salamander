// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.FileData.CZDirectory.h"
#include "TreeMap.TreeData.CCushion.h"
#include "TreeMap.TreeData.CCushionRow.h"

#define MINSIZE 3.0

class CCushionDirectory : public CCushion
{
protected:
    //int _level;
    TRECT _bounds;
    int _sortorder;

    CCushionRow* _firstRow;
    CCushionRow* _lastRow;

public:
    CCushionDirectory(CZDirectory* file, CCushionDirectory* parent, int sortorder) : CCushion(file, parent, sortorder)
    {
        this->_firstRow = NULL;
        this->_lastRow = NULL;
        this->_sortorder = sortorder;
    }

    ~CCushionDirectory()
    {
        //empty - all handled by the CRegionAllocator
    }

    virtual void SetBounds(int x, int y, int w, int h)
    {
        this->_bounds.x = x;
        this->_bounds.y = y;
        this->_bounds.w = w;
        this->_bounds.h = h;
    }

    CCushionRow* GetFirstRow() { return this->_firstRow; }
    CCushionRow* GetLastRow() { return this->_lastRow; }

    CZDirectory* GetDirectory() { return (CZDirectory*)this->_file; }

    virtual void FillFiles(CRegionAllocator* alloc, CTreeMapRendererBase* rdr, int level, EDirection parentdir)
    {
        double x = this->_bounds.x;
        double y = this->_bounds.y;
        double w = this->_bounds.w;
        double h = this->_bounds.h;
        //int area = this->_bounds.w * this->_bounds.h;

        double length;
        double maxwidth;

        INT64 remainingdata = this->_datasize;

        //double bps = (double)remainingdata / (w * h);
        double bps = 1e63;
        if ((this->_bounds.w > 0) && (this->_bounds.h > 0))
        {
            bps = (double)remainingdata / (w * h);
        }

        CZDirectory* dir = (CZDirectory*)this->_file;

        CCushionRow* csr = NULL;
        EDirection lastdir = dirNULL;

        int i = 0;
        int filecount = dir->GetFileCount();

        while (i < filecount)
        {
            /*
			 *   Create new Row
			 */
            lastdir = rdr->GetDirection((int)(w + PRECISSION_THRESHOLD), (int)(h + PRECISSION_THRESHOLD), level, lastdir, parentdir);

            POINT loc;
            loc.x = (int)(x + PRECISSION_THRESHOLD);
            loc.y = (int)(y + PRECISSION_THRESHOLD);

            if (lastdir == dirHorizontal)
            {
                length = w;
                maxwidth = h;
                csr = new (alloc) CCushionRow(loc, (int)(x + length + PRECISSION_THRESHOLD) - (int)(x + PRECISSION_THRESHOLD), lastdir);
            }
            else
            {
                length = h;
                maxwidth = w;
                csr = new (alloc) CCushionRow(loc, (int)(y + length + PRECISSION_THRESHOLD) - (int)(y + PRECISSION_THRESHOLD), lastdir);
            }

            /*
			 *  Create new row and add it to the list
			 */
            if (this->_lastRow != NULL)
            {
                this->_lastRow->SetNext(csr);
                this->_lastRow = csr;
            }
            else
            {
                this->_firstRow = csr;
                this->_lastRow = csr;
            }

            /*
			 *   Add children
			 */
            while (i < filecount)
            {
                CZFile* f = dir->GetFile(i);
                INT64 datasize = f->GetSizeEx(this->_sortorder);
                if (datasize > 0)
                {
                    if ((rdr->Decide(csr, maxwidth, length, datasize, remainingdata) == rdSameRow) || csr->IsEmpty())
                    {
                        remainingdata -= datasize;
                        if (f->IsDirectory())
                        {
                            CCushionDirectory* cs = new (alloc) CCushionDirectory((CZDirectory*)f, this, this->_sortorder);
                            csr->AddCushion(cs);
                        }
                        else
                        {
                            CCushion* cs = new (alloc) CCushion(f, this, this->_sortorder);
                            cs->SetColor(cs->Colorize(cs));
                            csr->AddCushion(cs);
                        }
                        i++;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    //fix pro sparse 0 byte velky soubor...
                    i++;
                }
            }

            /*
			*   Close the row
			*/
            INT64 ds = csr->GetDataSize();
            double width = 0;
            if (length > 0)
            {
                width = (double)ds / bps / length;
            }
            if (lastdir == dirHorizontal)
            {
                csr->SetWidth((int)(y + width + PRECISSION_THRESHOLD) - (int)(y + PRECISSION_THRESHOLD));
                y += width;
                h -= width;
            }
            else
            {
                csr->SetWidth((int)(x + width + PRECISSION_THRESHOLD) - (int)(x + PRECISSION_THRESHOLD));
                x += width;
                w -= width;
            }
        }

        for (csr = this->_firstRow; csr != NULL; csr = csr->GetNext())
        {
            csr->FillFiles(alloc, rdr, level + 1);
        }
    }
    CCushionHitInfo* GetCushionByLocation(int x, int y, CCushionHitInfo* parent = NULL)
    {
        for (CCushionRow* csr = this->_firstRow; csr != NULL; csr = csr->GetNext())
        {
            CCushionHitInfo* cshi = csr->GetCushionByLocation(x, y, parent);
            if (cshi)
                return cshi;
        }
        return parent;
    }
    virtual CCushionHitInfo* GetHitInfo(int x, int y, int cx, int cy, int cw, int ch, CCushionHitInfo* parent)
    {
        CCushionHitInfo* cshi = this->GetCushionByLocation(x, y, parent);
        if (!cshi)
            cshi = new CCushionHitInfo(this, parent, cx, cy, cw, ch);
        return cshi;
    }
    virtual BOOL IsDirectory() { return TRUE; }
};
