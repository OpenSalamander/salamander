// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	guidefs.h
	Miscelaneous definitions for the GUI.
*/

#pragma once

struct CBounds : SALGUI_BOUNDS
{
    void Default()
    {
        x = y = cx = cy = CW_USEDEFAULT;
    }

    void Empty()
    {
        x = y = cx = cy = 0;
    }

    void Set(RECT& rc)
    {
        x = rc.left;
        y = rc.top;
        cx = rc.right - rc.left;
        cy = rc.bottom - rc.top;
    }

    void Get(RECT& rc)
    {
        rc.left = x;
        rc.top = y;
        rc.right = x + cx;
        rc.bottom = y + cy;
    }

    CBounds& operator=(const SALGUI_BOUNDS& bounds)
    {
        x = bounds.x;
        y = bounds.y;
        cx = bounds.cx;
        cy = bounds.cy;
        return *this;
    }

    bool IsEmpty() const
    {
        _ASSERTE(x != CW_USEDEFAULT);
        _ASSERTE(y != CW_USEDEFAULT);
        _ASSERTE(cx != CW_USEDEFAULT);
        _ASSERTE(cy != CW_USEDEFAULT);

        return (cx == 0) || (cy == 0);
    }

    CBounds& Union(const CBounds& bounds)
    {
        _ASSERTE(bounds.x != CW_USEDEFAULT);
        _ASSERTE(bounds.y != CW_USEDEFAULT);
        _ASSERTE(bounds.cx != CW_USEDEFAULT);
        _ASSERTE(bounds.cy != CW_USEDEFAULT);

        _ASSERTE(x != CW_USEDEFAULT);
        _ASSERTE(y != CW_USEDEFAULT);
        _ASSERTE(cx != CW_USEDEFAULT);
        _ASSERTE(cy != CW_USEDEFAULT);

        if (IsEmpty())
        {
            if (bounds.IsEmpty())
            {
                Empty();
            }
            else
            {
                *this = bounds;
            }
        }
        else if (!bounds.IsEmpty())
        {
            int x12, y12;
            int x22, y22;

            x12 = x + cx;
            y12 = y + cy;

            x22 = bounds.x + bounds.cx;
            y22 = bounds.y + bounds.cy;

            x = min(x, bounds.x);
            y = min(y, bounds.y);
            cx = max(x12, x22) - x;
            cy = max(y12, y22) - y;
        }

        return *this;
    }
};
