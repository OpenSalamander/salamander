// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "TreeMap.FileData.CZFile.h"
#include "TreeMap.FileData.CZDirectory.h"

size_t CZFile::GetFullName(TCHAR* buff, size_t size)
{
    size_t pos = 0;
    if (this->_parent)
        pos = this->_parent->GetFullName(buff, size);
    if ((pos + this->_namelen + 1) < size) //pos = cesta + NULL; celkem = cesta + ( '\' + namelen ) + NULL < size
    {
        if (pos && buff[pos - 1] != TEXT('\\'))
        {
            buff[pos] = TEXT('\\');
            pos++;
        }
        _tcscpy(buff + pos, this->_name);
        pos += this->_namelen;
    }
    else
    {
        buff[pos] = TEXT('\0');
    }
    return pos;
}

size_t CZFile::GetRelativeName(CZDirectory* root, TCHAR* buff, size_t size)
{
    size_t pos = 0;
    if (this == root)
    {
        buff[0] = TEXT('\0');
        return 0;
    }

    if (this->_parent)
        pos = this->_parent->GetRelativeName(root, buff, size);
    if ((pos + this->_namelen + 1) < size)
    {
        if (pos && buff[pos - 1] != TEXT('\\'))
        {
            buff[pos] = TEXT('\\');
            pos++;
        }

        _tcscpy(buff + pos, this->_name);
        pos += this->_namelen;
    }
    else
    {
        buff[pos] = TEXT('\0');
    }
    return pos;
}