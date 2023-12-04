// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CPixMap
{
    friend class CZBitmap;

protected:
    int _width;
    int _height;
    BYTE* _pixmap;
    size_t _pixsize;

    void PreparePixMap(int width, int height, BOOL clear)
    {
        size_t size = width * height * 4;
        if (size > this->_pixsize)
        {
            if (this->_pixmap)
                free(this->_pixmap);
            this->_pixmap = (BYTE*)malloc(size);
            if (this->_pixmap == NULL)
                size = 0;
            this->_pixsize = size;
        }
        if (this->_pixmap != NULL)
        {
            if (clear == TRUE)
                memset(this->_pixmap, 0x00, size);
            this->_width = width;
            this->_height = height;
        }
        else
        {
            this->_width = 0;
            this->_height = 0;
            this->_pixsize = 0;
        }
    }

public:
    CPixMap()
    {
        this->_width = 0;
        this->_height = 0;
        this->_pixsize = 0;
        this->_pixmap = NULL;
    }
    ~CPixMap()
    {
        if (this->_pixmap)
            free(this->_pixmap);
        this->_pixmap = NULL;
        this->_pixsize = 0;
    }

    BYTE* AllocatePixMap(int width, int height, BOOL clear = FALSE)
    {
        PreparePixMap(width, height, clear);
        return this->_pixmap;
    }

    int GetWidth() { return this->_width; }
    int GetHeight() { return this->_height; }
};
