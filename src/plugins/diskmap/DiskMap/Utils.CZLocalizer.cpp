// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "Utils.CZLocalizer.h"

CZLocalizer* CZResourceString::s_localizer = NULL;

CZLocalizer::CZLocalizer(HINSTANCE HModule)
{
    //nejdriv vyplnit chybovou hlasku
    //this->_errlen = ARRAYSIZE(szStrLoadError);
    //_tcscpy(_buffer, szStrLoadError);
    //nacist vsechny retezce - nemuzeme odkladat, protoze by se musela resit synchronizace mezi vlakny
    this->_freepos = this->_buffer;
    this->_bufferend = this->_buffer + sizeof(_buffer);
    for (int i = IDS_DISKMAP_FIRST; i <= IDS_DISKMAP_LAST; i++)
    {
        int size = LoadString(HModule, i, this->_freepos, (int)(this->_bufferend - this->_freepos));
        if (size == 0) //nenalezeno
        {
            this->_start[i - IDS_DISKMAP_FIRST] = szStrLoadError;
            this->_length[i - IDS_DISKMAP_FIRST] = ARRAYSIZE(szStrLoadError);
        }
        else
        {
            this->_start[i - IDS_DISKMAP_FIRST] = this->_freepos;
            this->_length[i - IDS_DISKMAP_FIRST] = size;
            this->_freepos += size + 1;
        }
    }
    CZResourceString::s_localizer = this;
    //		CZLocalizer::s_instance = this;
}