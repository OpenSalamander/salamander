// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.TreeData.CCushion.h"

#define TEXT_HEIGHT 0
#define MINDIRSIZE 30

class CCushion;
class CCushionDirectory;

class CCushionRow
{
protected:
    POINT _location;
    EDirection _dir;
    int _length; // DELKA
    int _width;  //sirka

    CCushion* _firstCushion;
    CCushion* _lastCushion;
    INT64 _datasize;

    CCushionRow* _next;

public:
    CCushionRow(POINT& location, int length, EDirection dir)
    {
        this->_next = NULL;
        this->_firstCushion = NULL;
        this->_lastCushion = NULL;

        this->_location = location;
        this->_length = length;
        this->_width = 0;

        this->_dir = dir;
        this->_width = 0;
        this->_datasize = 0;
    }
    ~CCushionRow()
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

    INT64 GetDataSize() { return this->_datasize; }

    EDirection GetDirection() { return this->_dir; }

    CCushionRow* GetNext() { return this->_next; }
    void SetNext(CCushionRow* csr) { this->_next = csr; }

    int GetLength() { return this->_length; }
    int GetWidth() { return this->_width; }
    void SetWidth(int width)
    {
        this->_width = width;
        this->RecalcCushions();
    }

    void RecalcCushions()
    {
        if (this->_width > 0)
        {
            double sx = 0;
            double ex = 0;
            //double bpl = (double)this->_datasize / this->_length / this->_width / this->_length;
            double bpl = (double)this->_datasize / this->_length;

            for (CCushion* cs = this->_firstCushion; cs != NULL; cs = cs->GetNext())
            {
                //double ts = (((double)cs->_datasize / (double)this->_datasize) * this->_length);
                double ts = cs->_datasize / bpl;
                sx += ts;
                cs->_size = (int)(sx + PRECISSION_THRESHOLD) - (int)(ex + PRECISSION_THRESHOLD);
                ex += ts;
            }
        }
        else
        {
            for (CCushion* cs = this->_firstCushion; cs != NULL; cs = cs->GetNext())
            {
                cs->_size = 0;
            }
        }
    }

    void GetLocation(RECT& rct)
    {
        rct.left = this->_location.x;
        rct.top = this->_location.y;
        if (this->_dir == dirVertical)
        {
            rct.right = rct.left + this->_width;
            rct.bottom = rct.top + this->_length;
        }
        else
        {
            rct.right = rct.left + this->_length;
            rct.bottom = rct.top + this->_width;
        }
    }
    /*
	double GetPreciseWidth()
	{
		if (this->_width < 0) return this->RecalcWidth();
		return this->_width;
	}
	double RecalcWidth()
	{
		this->_width = ((this->_datasize / this->_bytesperpixel) / this->_length);
		//this->_width = (int)((tw + PRECISSION_THRESHOLD));
		int tw = (int)((this->_width + PRECISSION_THRESHOLD));
		if (this->_width > 0)
		{
			double sx = 0;
			double ex = 0;
			double bpl = this->_bytesperpixel * tw;//this->_width;
			for (int i = 0; i < this->_list->GetCount(); i++)
			{
				CCushion *cs = this->_list->At(i);
				//double ts = ((cs->_datasize / this->_bytesperpixel) / this->_width);
				double ts = cs->_datasize / bpl;
				sx += ts;
				cs->_size = (int)(sx + PRECISSION_THRESHOLD) - (int)(ex + PRECISSION_THRESHOLD);
				ex += ts;
			}
		}
		return this->_width;
	}*/

    void AddCushion(CCushion* cs)
    {
        if (this->_lastCushion != NULL)
        {
            this->_lastCushion->SetNext(cs);
            this->_lastCushion = cs;
        }
        else
        {
            this->_firstCushion = cs;
            this->_lastCushion = cs;
        }
        this->_datasize += cs->GetDataSize();
        this->_width = -1;
    }
    BOOL IsEmpty()
    {
        return (this->_lastCushion == NULL);
    }
    CCushion* GetFirstCushion()
    {
        return this->_firstCushion;
    }
    CCushion* GetLastCushion()
    {
        return this->_lastCushion;
    }

    void FillFiles(CRegionAllocator* alloc, CTreeMapRendererBase* rdr, int level)
    {
        if (this->_dir == dirVertical)
        {
            int sy = this->_location.y;

            for (CCushion* cs = this->_firstCushion; cs != NULL; cs = cs->GetNext())
            {
                if (cs->IsDirectory())
                {
                    //CCushionDirectory *csd = (CCushionDirectory *)cs;
                    /*if (level == 1 && min(this->_width, cs->_size) > MINDIRSIZE)
					{
						cs->SetBounds(this->_location.x + 1, sy + 1 + TEXT_HEIGHT, this->_width - 2, cs->_size - 2 - TEXT_HEIGHT);
					}
					else
					{*/
                    cs->SetBounds(this->_location.x, sy, this->_width, cs->_size);
                    //}
                    cs->FillFiles(alloc, rdr, level, dirVertical);
                }
                sy += cs->_size;
            }
        }
        else
        {
            int sx = this->_location.x;

            for (CCushion* cs = this->_firstCushion; cs != NULL; cs = cs->GetNext())
            {
                if (cs->IsDirectory())
                {
                    //CCushionDirectory *csd = (CCushionDirectory *)cs;
                    /*if (level == 1 && min(this->_width, cs->_size) > MINDIRSIZE)
					{
						//cs->SetBounds(this->_location.x + 1, sy + 9, this->_width - 2, cs->_size - 10);
						cs->SetBounds(sx + 1, this->_location.y + 1 + TEXT_HEIGHT, cs->_size - 2,  this->_width - 2 - TEXT_HEIGHT);
					}
					else
					{*/
                    cs->SetBounds(sx, this->_location.y, cs->_size, this->_width);
                    //}
                    cs->FillFiles(alloc, rdr, level, dirHorizontal);
                }
                sx += cs->_size;
            }
        }
    }

    CCushionHitInfo* GetCushionByLocation(int x, int y, CCushionHitInfo* parent)
    {
        if (x < this->_location.x)
            return NULL;
        if (y < this->_location.y)
            return NULL;
        if (this->_dir == dirVertical) // |
        {
            if (x >= this->_location.x + this->_width)
                return NULL;
            if (y >= this->_location.y + this->_length)
                return NULL;
            //proslo, takze je to nejaky nas polstar
            int sy = this->_location.y;

            for (CCushion* cs = this->_firstCushion; cs != NULL; cs = cs->GetNext())
            {
                sy += cs->_size;
                if (y < sy)
                    return cs->GetHitInfo(x, y, this->_location.x, sy - cs->_size, this->_width, cs->_size, parent);
            }
            return NULL; // CHYBA (data)!!
        }
        else // ------
        {
            if (x >= this->_location.x + this->_length)
                return NULL;
            if (y >= this->_location.y + this->_width)
                return NULL;
            //proslo, takze je to nejaky nas polstar
            int sx = this->_location.x;

            for (CCushion* cs = this->_firstCushion; cs != NULL; cs = cs->GetNext())
            {
                sx += cs->_size;
                if (x < sx)
                    return cs->GetHitInfo(x, y, sx - cs->_size, this->_location.y, cs->_size, this->_width, parent);
            }
            return NULL; // CHYBA (data)!!
        }
        return NULL; // CHYBA (compiler)?!?!
    }
};
