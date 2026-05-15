// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CLock
{
protected:
    volatile LONG _lock;

public:
    CLock()
    {
        this->_lock = 0;
    }
    BOOL TryEnter()
    {
        return InterlockedExchange(&this->_lock, 1) == 0; // before the write, 0 means the lock was acquired; otherwise it was already locked (1)
    }
    void Enter()
    {
        while (!this->TryEnter())
            Sleep(0); // Sleep(0): reschedule this thread's time slice to another thread in our process
    }
    void Leave()
    {
        this->_lock = 0; // 32-bit writes are always atomic (no "interlocked" function required)
    }
};
