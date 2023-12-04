// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//Persistent not supported!!!

class CBuffer
{
public:
    CBuffer(BOOL persistent = FALSE)
    {
        Buffer = NULL;
        Allocated = 0;
        Persistent = persistent;
    }
    ~CBuffer() { Release(); }
    BOOL Reserve(int size);
    void Release();
    int GetSize() { return Allocated; }

protected:
    void* Buffer;
    int Allocated;
    BOOL Persistent;
};

template <class DATA_TYPE>
class TBuffer : public CBuffer
{
public:
    TBuffer(BOOL persistent = FALSE) : CBuffer(persistent) { ; }
    BOOL Reserve(int size) { return CBuffer::Reserve(size * sizeof(DATA_TYPE)); }
    DATA_TYPE* Get() { return (DATA_TYPE*)Buffer; }
    int GetSize() { return Allocated / sizeof(DATA_TYPE); }
};
