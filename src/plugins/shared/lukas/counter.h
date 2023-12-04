// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CCounter -- hight precision counter for performance metering

class CCounter
{
public:
    CCounter() : OnHold(true)
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        Frequency = double(f.QuadPart);
        Reset();
    }
    // reset counter
    void Reset()
    {
        Counter.QuadPart = 0;
        HoldCount = 0;
    }
    // enable counting
    void HoldOff()
    {
        QueryPerformanceCounter(&Start);
    }
    // disable counting
    void HoldOn()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        Counter.QuadPart += counter.QuadPart - Start.QuadPart;
        HoldCount++;
    }
    // get current counter value expressed in seconds
    double Read()
    {
        LARGE_INTEGER counter = Counter;
        if (!OnHold)
        {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            Counter.QuadPart += (now.QuadPart - Start.QuadPart);
        }
        return double(Counter.QuadPart) / Frequency;
    }
    // get current counter value expressend in seconds divided by the number of
    // hold-ons
    double Average() { return Read() / HoldCount; }
    // reset counter and enable counting
    void Tic()
    {
        Reset();
        HoldOff();
    }
    // stop counting and return counter value expressed in seconds
    double Toc()
    {
        HoldOn();
        return Read();
    }

protected:
    double Frequency;
    LARGE_INTEGER Counter;
    LARGE_INTEGER Start;
    bool OnHold;
    int HoldCount;
};
