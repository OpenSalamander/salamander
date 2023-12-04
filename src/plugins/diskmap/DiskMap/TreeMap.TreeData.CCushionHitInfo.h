// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.TreeData.h"

class CCushion;
class CCushionHitInfo;

class CCushionHitInfo
{
protected:
    TRECT _rct;
    CCushion* _cs;
    CCushionHitInfo* _parent;

public:
    CCushionHitInfo(CCushion* cs, CCushionHitInfo* parent, int x, int y, int w, int h)
    {
        this->_cs = cs;
        this->_parent = parent;
        this->_rct.x = x;
        this->_rct.y = y;
        this->_rct.w = w;
        this->_rct.h = h;
    }
    ~CCushionHitInfo()
    {
        if (this->_parent)
            delete this->_parent;
    }
    CCushion* GetCushion() { return this->_cs; }
    CCushionHitInfo* GetParent() { return this->_parent; }
    int GetPosX() { return this->_rct.x; }
    int GetPosY() { return this->_rct.y; }
    int GetWidth() { return this->_rct.w; }
    int GetHeight() { return this->_rct.h; }
};
