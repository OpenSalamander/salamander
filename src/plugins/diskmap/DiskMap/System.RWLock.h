// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "System.Lock.h"

class CRWLock
{
protected:
    CLock* _lock;
    volatile LONG _readlock;
    volatile LONG _writelock;
    volatile LONG _writewaitcount;

public:
    CRWLock()
    {
        this->_readlock = 0;
        this->_writelock = 0;
        this->_writewaitcount = 0;
        this->_lock = new CLock();
    }
    ~CRWLock()
    {
        delete this->_lock;
    }
    BOOL TryEnterRead()
    {
        BOOL res = FALSE;
        this->_lock->Enter();
        if (this->_writelock == 0 && this->_writewaitcount == 0)
        {
            this->_readlock++;
            res = TRUE;
        }
        this->_lock->Leave();
        return res;
    }
    BOOL EnterRead()
    {
        while (!this->TryEnterRead())
            Sleep(0);
        return TRUE;
    }
    void LeaveRead()
    {
        this->_lock->Enter();
        this->_readlock--;
        this->_lock->Leave();
        //InterlockedDecrement(this->_readlock);
    }
    BOOL TryEnterWrite()
    {
        BOOL res = FALSE;
        this->_lock->Enter();
        if (this->_readlock == 0 && this->_writelock == 0)
        {
            this->_writelock = 1;
            res = TRUE;
        }
        this->_lock->Leave();
        return res;
    }
    BOOL EnterWrite(BOOL prioritize = TRUE)
    {
        if (!this->TryEnterWrite())
        {
            if (prioritize)
                InterlockedIncrement(&this->_writewaitcount);
            while (!this->TryEnterWrite())
                Sleep(0);
            if (prioritize)
                InterlockedDecrement(&this->_writewaitcount);
        }
        return TRUE;
    }
    void LeaveWrite()
    {
        this->_lock->Enter();
        this->_writelock = 0;
        this->_lock->Leave();
        //this->_writelock = 0;
    }
};
