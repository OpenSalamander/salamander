// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define COUNT_IDS_DISKMAP (IDS_DISKMAP_LAST - IDS_DISKMAP_FIRST + 1)

const TCHAR szStrLoadError[] = TEXT("ERROR LOADING STRING");
const TCHAR szWrongIDError[] = TEXT("WRONG STRING ID");

//class CZLocalizer;
class CZResourceString;

class CZLocalizer
{
protected:
    //friend class CZResourceString;

    //CZResourceString *_strings[IDS_DISKMAP_LAST - IDS_DISKMAP_FIRST + 1];
    TCHAR _buffer[100 * COUNT_IDS_DISKMAP]; //buffer na vsechny retezce... pocitame 100 znaku na retezec = celkem cca 2Kb
    TCHAR const* _start[COUNT_IDS_DISKMAP]; //zacatky jednotlivych retezcu
    size_t _length[COUNT_IDS_DISKMAP];      //delky jednotlivych retezcu
    TCHAR* _freepos;                        //odkaz na volne misto
    TCHAR const* _bufferend;                //odkaz na konec bufferu - znak za bufferem

    //size_t _errlen; //delka chybove hlasky
    //CZString _error("ERROR LOADING STRING");
    //static CZLocalizer *s_instance;
public:
    CZLocalizer(HINSTANCE HModule);
    ~CZLocalizer()
    {
    }
    TCHAR const* GetString(UINT id) const
    {
        if ((id >= IDS_DISKMAP_FIRST) && (id <= IDS_DISKMAP_LAST))
            return this->_start[id - IDS_DISKMAP_FIRST];
        return szWrongIDError;
    }
    size_t GetLength(UINT id) const
    {
        if ((id >= IDS_DISKMAP_FIRST) && (id <= IDS_DISKMAP_LAST))
            return this->_length[id - IDS_DISKMAP_FIRST];
        return ARRAYSIZE(szWrongIDError);
    }
};

class CZResourceString
{
    friend class CZLocalizer;

protected:
    UINT _id;
    static CZLocalizer* s_localizer;

public:
    explicit CZResourceString(UINT id)
    {
        this->_id = id;
    }

    ~CZResourceString()
    {
    }
    TCHAR const* GetString() const { return CZResourceString::s_localizer->GetString(this->_id); }
    size_t GetLength() const { return CZResourceString::s_localizer->GetLength(this->_id); }

    static TCHAR const* GetString(UINT id) { return CZResourceString::s_localizer->GetString(id); }
    static size_t GetLength(UINT id) { return CZResourceString::s_localizer->GetLength(id); }
};
