// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Utils.CPixMap.h"

enum ELockState
{
    ELS_None,
    ELS_Pixmap,
    ELS_GDI
};

class CZBitmap
{
protected:
    int _width;
    int _height;

    BYTE* _pix;
    HBITMAP _bmp;

    ELockState _lockState;
    BOOL _isGdiFlushed;

    HDC _hdc;
    HGDIOBJ _obmp;

    void DisposeBitmap()
    {
        if (this->_hdc != NULL)
        {
            this->DeleteDC();
        }
        if (this->_bmp != NULL)
        {
            DeleteObject(this->_bmp);
            this->_bmp = NULL;
            this->_pix = NULL;
        }
    }
    void CreateBitmap(HDC hdc)
    {
        this->DisposeBitmap();

        BITMAPINFO bmix;
        memset(&bmix, 0, sizeof(bmix));
        bmix.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmix.bmiHeader.biWidth = this->_width;
        bmix.bmiHeader.biHeight = -this->_height; // top-down image
        bmix.bmiHeader.biPlanes = 1;
        bmix.bmiHeader.biBitCount = 32;
        bmix.bmiHeader.biCompression = BI_RGB;
        bmix.bmiHeader.biSizeImage = 0; //0 pouze pro BI_RGB obrazky

        BYTE* pix = NULL;

        HBITMAP bm = CreateDIBSection(hdc, &bmix, DIB_RGB_COLORS, (void**)&pix, NULL, 0);
        if (bm)
        {
            this->_pix = pix;
            this->_bmp = bm;
        }
    }
    /*
	void CreateBitmap(HDC hdc, CPixMap *pixmap)
	{
		this->DisposeBitmap();

		HBITMAP bm = CreateCompatibleBitmap(hdc, pixmap->_width, pixmap->_height);
		if (bm)
		{
			BITMAPINFO bmix;
			memset(&bmix, 0, sizeof(bmix));
			bmix.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
			bmix.bmiHeader.biWidth       = pixmap->_width;
			bmix.bmiHeader.biHeight      = -pixmap->_height; // top-down image
			bmix.bmiHeader.biPlanes      = 1;
			bmix.bmiHeader.biBitCount    = 32;
			bmix.bmiHeader.biCompression = BI_RGB;
			bmix.bmiHeader.biSizeImage   = 0; //0 pouze pro BI_RGB obrazky

			if (SetDIBits(hdc, bm, 0, pixmap->_height, pixmap->_pixmap, &bmix, DIB_RGB_COLORS) > 0)
			{
				this->_bmp = bm;
				this->_width = pixmap->_width;
				this->_height = pixmap->_height;
			}
			else
			{
				DeleteObject(bm);
			}
		}
	}*/
public:
    /*CZBitmap(HDC hdc, CPixMap *pixmap)
	{
		this->_width = pixmap->GetWidth();
		this->_height = pixmap->GetHeight();

		this->_hdc = NULL;
		this->_bmp = NULL;

		this->_lockState = ELS_None;

		CreateBitmap(hdc);

		BYTE *pix = LockBits();
		if (pix)
		{
			size_t size = this->_width * this->_height * 4;
			memcpy(pix, other->_pixmap, size);
		}
	}*/
    /*CZBitmap(HDC hdc, int width, int height) //empty bitmap
	{
		this->_hdc = NULL;
		this->_bmp = NULL;

		this->_width = width;
		this->_height = height;

		this->_lockState = ELS_None;

		CreateBitmap(hdc);
	}*/
    CZBitmap() //empty bitmap
    {
        this->_hdc = NULL;
        this->_bmp = NULL;

        this->_width = 0;
        this->_height = 0;

        this->_lockState = ELS_None;

        //CreateBitmap(hdc);
    }

    ~CZBitmap()
    {
        this->DisposeBitmap();
    }
    int GetWidth() { return this->_width; }
    int GetHeight() { return this->_height; }

    BOOL ResizeBitmap(HDC hdc, int width, int height)
    {
        if ((this->_width == width) && (this->_height == height) && (this->_bmp != NULL))
            return TRUE;

        this->DisposeBitmap();

        this->_width = width;
        this->_height = height;

        this->CreateBitmap(hdc);

        return TRUE;
    }

    void CopyFromPixMap(CPixMap* pixmap)
    {
        int width = pixmap->GetWidth();
        int height = pixmap->GetHeight();

        if ((width == this->_width) && (height == this->_height))
        {
            BYTE* pix = this->LockBits();
            if (pix != NULL)
            {
                size_t size = width * height * 4;
                memcpy(pix, pixmap->_pixmap, size);
                this->UnlockBits();
            }
        }
    }

    BYTE* LockBits()
    {
        if (this->_lockState == ELS_GDI)
            return NULL;

        this->_lockState = ELS_Pixmap;
        return this->_pix;
    }
    void UnlockBits()
    {
        if (this->_lockState != ELS_GDI)
            this->_lockState = ELS_None;
    }

    HDC CreateDC(HDC hdc)
    {
        if (this->_lockState == ELS_Pixmap)
            return NULL;

        if (this->_hdc)
            return this->_hdc;

        this->_hdc = CreateCompatibleDC(hdc);
        if (this->_hdc != NULL)
        {
            this->_obmp = SelectObject(this->_hdc, this->_bmp);
        }
        return this->_hdc;
    }
    void DeleteDC()
    {
        if (this->_lockState != ELS_Pixmap)
            this->_lockState = ELS_None;

        if (this->_hdc == NULL)
            return;

        SelectObject(this->_hdc, this->_obmp);
        ::DeleteDC(this->_hdc);
        this->_hdc = NULL;
    }
    /*
	BOOL CombineBitmap(int x, int y, CZBitmap *bm1)
	{
		BOOL res = FALSE;
		if (this->_bmp != NULL && this->_hdc != NULL && bm1 != NULL)
		{
			res = bm1->Paint(this->_hdc, x, y);
		}
		return res;
	}
	BOOL Paint(HDC hdc, int x, int y)
	{
		BOOL res = FALSE;
		if (this->_bmp != NULL)
		{
			HDC dc = this->CreateDC(hdc);
			if (dc)
			{
				res = BitBlt(hdc, x, y, this->_width, this->_height, dc, 0, 0, SRCCOPY);
#ifdef _DEBUG
				TextOut(hdc, 8, 30, TEXT("BitBlt"), 6);
#endif
				this->DeleteDC();
			}
		}
		return res;
	}*/
    BOOL Paint(HDC hdc, int x, int y, int w, int h)
    {
        BOOL res = FALSE;
        if (this->_bmp != NULL)
        {
            HDC dc = this->CreateDC(hdc);
            if (dc != NULL)
            {
                SetStretchBltMode(hdc, STRETCH_DELETESCANS);
                res = StretchBlt(hdc, x, y, w, h, dc, 0, 0, this->_width, this->_height, SRCCOPY);
#ifdef _DEBUG
                TextOut(hdc, 8, 30, TEXT("StretchBlt"), 10);
#endif
                this->DeleteDC();
            }
        }
        return res;
    }
};
