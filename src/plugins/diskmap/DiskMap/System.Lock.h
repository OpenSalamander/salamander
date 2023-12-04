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
        return InterlockedExchange(&this->_lock, 1) == 0; // pred zapisem 0 znamena zamknuti, jinak uz bylo zamknute (1)
    }
    void Enter()
    {
        while (!this->TryEnter())
            Sleep(0); // Sleep(0): cas pro tento thread preplanujeme jinemu threadu naseho procesu
    }
    void Leave()
    {
        this->_lock = 0; // zapis 32-bit je vzdy atomicky (nevyzaduje "interlocked" funkci)
    }
};
