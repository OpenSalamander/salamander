// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CAnimation
{
protected:
    int _frame;
    int _frameTime;
    LONG _lastTime;
    int _timeRem;

    CAnimation(int frametime)
    {
        this->_frameTime = frametime;
        this->Reset();
    }
    CAnimation()
    {
        this->_frameTime = 100;
        this->Reset();
    }
    virtual BOOL DoStep(int milis)
    {
        this->_timeRem += milis;
        int fcnt = this->_timeRem / this->_frameTime;
        this->_timeRem %= this->_frameTime;
        if (fcnt > 0)
        {
            this->IncFrame(fcnt);
            return TRUE; //repaint required
        }
        return FALSE;
    }
    virtual void IncFrame(int count = 1)
    {
        this->_frame += count;
    }

public:
    virtual ~CAnimation()
    {
    }

    BOOL OnTimerUpdate()
    {
        LONG now = GetMessageTime();
        BOOL res = this->DoStep(now - this->_lastTime);
        this->_lastTime = now;
        return res;
    }
    virtual void Reset()
    {
        this->_frame = 0;
        this->_lastTime = GetMessageTime();
        this->_timeRem = 0;
    }
    virtual void Paint(HDC hdc, RECT rect) = 0;
};
