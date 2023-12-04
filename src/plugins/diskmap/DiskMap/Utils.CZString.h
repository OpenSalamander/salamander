// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.FileData.CZFile.h"

class CStringFormatter;

class CZString
{
protected:
    TCHAR* _s;
    size_t _l;

public:
    explicit CZString(TCHAR const* s)
    {
        this->_l = _tcslen(s);
        this->_s = (TCHAR*)malloc((this->_l + 1) * sizeof TCHAR);
        _tcscpy(this->_s, s);
    }

    explicit CZString(CZFile* file)
    {
        TCHAR buff[2 * MAX_PATH + 1];
        this->_l = file->GetFullName(buff, ARRAYSIZE(buff));
        this->_s = (TCHAR*)malloc((this->_l + 1) * sizeof TCHAR);
        _tcscpy(this->_s, buff);
    }

    ~CZString()
    {
        free(this->_s);
        this->_s = NULL;
        this->_l = 0;
    }
    TCHAR const* GetString() const { return this->_s; }
    size_t GetLength() const { return this->_l; }
};

class CZStringBuffer
{
    friend class CStringFormatter;

protected:
    TCHAR* _s;
    size_t _l; //delka retezce
    size_t _c; //velikost buff v TCHARech
public:
    explicit CZStringBuffer(TCHAR const* s)
    {
        this->_l = _tcslen(s);
        this->_c = this->_l + 1;
        this->_s = (TCHAR*)malloc(this->_c * sizeof TCHAR);
    }
    explicit CZStringBuffer(size_t size)
    {
        this->_l = 0;
        this->_c = size;
        this->_s = (TCHAR*)malloc(this->_c * sizeof TCHAR);
        *this->_s = TEXT('\0');
    }
    ~CZStringBuffer()
    {
        free(this->_s);
        this->_s = NULL;
        this->_c = 0;
        this->_l = 0;
    }
    TCHAR const* GetString() const { return this->_s; }
    size_t GetLength() const { return this->_l; }
    size_t GetBuffSize() const { return this->_c; }

    BOOL EndsWith(TCHAR chr) const
    {
        if (this->_l == 0)
            return FALSE;
        return (this->_s[this->_l - 1] == chr);
    }
    BOOL StartsWith(TCHAR chr) const
    {
        if (this->_l == 0)
            return FALSE;
        return (this->_s[0] == chr);
    }
    BOOL IsCharAt(TCHAR chr, size_t pos) const
    {
        if (this->_l < pos)
            return FALSE;
        return (this->_s[pos] == chr);
    }

    CZStringBuffer* Append(TCHAR const* s)
    {
        size_t len = _tcslen(s);

        return this->AppendAt(s, len, this->_l);
    }

    CZStringBuffer* Append(TCHAR const* s, size_t len)
    {
        return this->AppendAt(s, len, this->_l);
    }

    CZStringBuffer* Append(TCHAR c)
    {
        return this->AppendAt(c, this->_l);
    }

    CZStringBuffer* AppendAt(TCHAR const* s, int pos)
    {
        size_t len = _tcslen(s);

        return this->AppendAt(s, len, pos);
    }
    CZStringBuffer* AppendAt(TCHAR const* s, size_t len, size_t pos)
    {
        int p = (int)min(pos, this->_l);
        if (len + p > this->_c)
            return this; //TODO!

        //len = this->_c - this->_l - 1;
        _tcscpy(&this->_s[p], s);
        //memcpy(this->_s[this->_l], s, len * sizeof TCHAR);

        this->_l = len + p;
        this->_s[this->_l] = TEXT('\0');

        return this;
    }
    CZStringBuffer* AppendAt(TCHAR c, size_t pos)
    {
        size_t p = min(pos, this->_l);
        if (p >= this->_c)
            return this; //TODO!

        this->_s[p] = c;

        this->_l = p + 1;
        this->_s[this->_l] = TEXT('\0');

        return this;
    }
    CZStringBuffer* Append(CZString const* s)
    {
        return this->AppendAt(s->GetString(), s->GetLength(), this->_l);
    }
    CZStringBuffer* AppendAt(CZString const* s, int pos)
    {
        return this->AppendAt(s->GetString(), s->GetLength(), pos);
    }

    CZStringBuffer* Left(size_t len)
    {
        if (len > this->_l)
            len = this->_l;

        this->_l = len;
        this->_s[this->_l] = TEXT('\0');

        return this;
    }
    size_t GetSubString(size_t pos, size_t length, TCHAR* buff, size_t bufflen)
    {
        if (pos >= this->_l)
            return 0;

        if (length >= bufflen)
            length = bufflen - 1;
        if (pos + length > this->_l)
            length = this->_l - pos;

        //_tcscpy(&this->_s[p], s);
        memcpy(buff, &this->_s[pos], length * sizeof TCHAR);
        buff[length] = TEXT('\0');

        return length;
    }
};
