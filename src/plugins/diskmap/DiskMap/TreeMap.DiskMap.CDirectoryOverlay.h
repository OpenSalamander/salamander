// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MAX_DIRECTORIES 1024

const BYTE CDirectoryOverlay_shadowdata[] = {
    192, 142, 92, 42,
    142, 132, 86, 38,
    92, 86, 71, 27,
    42, 38, 27, 0};

struct SCDO_DirectoryItem
{
    CCushionDirectory* directory;
    int x;
    int y;
    int width;
    int height;
};

class CDirectoryOverlay : public CDiskMapOverlay
{
protected:
    SCDO_DirectoryItem _majorDirectories[MAX_DIRECTORIES];
    int _majorDirectoryCount;

    SCDO_DirectoryItem _minorDirectories[4 * MAX_DIRECTORIES];
    int _minorDirectoryCount;

    BOOL DrawMinorDirectory(BYTE* p, int pw, int ph, int cshx, int cshy, int cshw, int cshh, int txt_heigth)
    {
        if (cshx >= pw)
            return FALSE;
        if (cshy >= ph)
            return FALSE;
        if (cshw < 6)
            return FALSE;
        if (cshh < 6)
            return FALSE;

        //if (txt_heigth > cshh - 5) txt_heigth = cshh - 5;

        BYTE* tBits = p;

        int i, j;

        //COLORREF col = RGB(192, 192, 192);
        COLORREF col = RGB(128, 128, 128);

        tBits += 4 * pw * cshy;
        tBits += 4 * cshx;

        //horni linka
        //for (i = 0; i < 2; i++)
        {
            //prvni radka
            unsigned int* piBits = (unsigned int*)tBits;

            piBits++;
            for (j = 0; j < cshw - 2; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            tBits += 4 * pw;
        }
        //dolni cast - zatoceni rohu
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //levy okraj 2px + 1px kulaty roh
            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            //stred preskocit
            piBits += cshw - 4;
            //pravy okraj 2px + 1px kulaty roh
            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            tBits += 4 * pw;
        }
        /*
		//titulek
		for (i = 0; i < txt_heigth; i++)
		{
			unsigned int *piBits = (unsigned int *)tBits;
			//levy okraj
			*piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);

			//stredni cast
			for (j = 0; j < cshw - 2; j++)
			{
				//hlidat horni rohy
				if (i == 0) 
				{
					//hlidat levy roh
					if (j == 0)
					{
						*piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
						continue;
					}
					//hlidat pravy roh
					if (j == cshw - 5)
					{
						*piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
						continue;
					}
				}
				//vypln
				*piBits++ = coltxt + ((*piBits >> 1) & 0x007f7f7f);
			}
			//pravy okraj
			*piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
			tBits += 4 * pw;
		}*/
        //pravy a levy okraj
        for (i = 0; i < cshh - 4; i++)
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //levy okraj 2px
            for (j = 0; j < 1; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            //stred preskocit
            piBits += cshw - 2;
            //pravy okraj 2px
            for (j = 0; j < 1; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            tBits += 4 * pw;
        }
        //dolni cast - zatoceni rohu
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //levy okraj 2px + 1px kulaty roh
            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            //stred preskocit
            piBits += cshw - 4;
            //pravy okraj 2px + 1px kulaty roh
            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            tBits += 4 * pw;
        }
        //spodni linka
        //for (i = 0; i < 2; i++)
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //pravni radka
            /*for (j = 0; j < cshw; j++)
			{
				*piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
			}
			tBits += 4 * pw;*/

            //druha radka - bez rohu pro efekt zakulaceni
            piBits = (unsigned int*)tBits;
            piBits++;
            for (j = 0; j < cshw - 2; j++)
            {
                *piBits++ = col + ((*piBits >> 1) & 0x007f7f7f);
            }
            tBits += 4 * pw;
        }
        return TRUE;
    }

    BOOL DrawMajorDirectory(BYTE* p, int pw, int ph, int cshx, int cshy, int cshw, int cshh, BYTE* txt_data, int txt_width, int txt_heigth)
    {
        if (cshx >= pw)
            return FALSE;
        if (cshy >= ph)
            return FALSE;
        if (cshw < 6)
            return FALSE;
        if (cshh < 6)
            return FALSE;

        if (txt_heigth > cshh - 5)
            txt_heigth = cshh - 5;

        BYTE* tBits = p;

        int i, j;

        COLORREF col = RGB(192, 192, 192);
        COLORREF coltxt = RGB(128, 128, 128);

        tBits += 4 * pw * cshy;
        tBits += 4 * cshx;

        //horni linka
        //for (i = 0; i < 2; i++)
        {
            //prvni radka
            unsigned int* piBits = (unsigned int*)tBits;

            piBits++;
            for (j = 0; j < cshw - 2; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;
            //druha radka
            piBits = (unsigned int*)tBits;
            for (j = 0; j < cshw; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;
        }

        //titulek
        for (i = 0; i < txt_heigth; i++)
        {
            unsigned int* piBits = (unsigned int*)tBits;
            unsigned int* txtBits = (unsigned int*)txt_data;

            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            txtBits += 2; //preskocit 2px okraj

            //stredni cast
            //for (j = 0; j < cshw - 6; j++)
            for (j = 0; j < cshw - 4; j++)
            {
                //hlidat horni rohy
                if (i == 0)
                {
                    //hlidat levy roh
                    if (j == 0)
                    {
                        *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
                        txtBits++;
                        continue;
                    }
                    //hlidat pravy roh
                    if (j == cshw - 5)
                    {
                        *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
                        txtBits++;
                        continue;
                    }
                }
                //optimalizace...
                if (j > txt_width)
                {
                    *piBits++ = coltxt + ((*piBits >> 1) & 0x007f7f7f);
                    txtBits++;
                }
                else
                {
                    // from: http://www.stereopsis.com/doubleblend.html
                    const unsigned int a = (*txtBits & 0xff) + 1;

                    const unsigned int dstrb = *piBits & 0xFF00FF;
                    const unsigned int dstg = *piBits & 0xFF00;

                    const unsigned int srcrb = 0xFF00FF;
                    const unsigned int srcg = 0xFF00;

                    unsigned int drb = srcrb - dstrb;
                    unsigned int dg = srcg - dstg;

                    drb *= a;
                    dg *= a;
                    drb >>= 8;
                    dg >>= 8;

                    unsigned int rb = (drb + dstrb) & 0xFF00FF;
                    unsigned int g = (dg + dstg) & 0xFF00;

                    *piBits = rb | g;

                    piBits++;
                    txtBits++;
                }
            }

            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;
            txt_data += 4 * pw;
        }
        //pravy a levy okraj
        for (i = 0; i < cshh - txt_heigth - 5; i++)
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //levy okraj 2px
            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            //stred preskocit
            piBits += cshw - 4;
            //pravy okraj 2px
            for (j = 0; j < 2; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;
        }
        //dolni cast - zatoceni rohu
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //levy okraj 2px + 1px kulaty roh
            for (j = 0; j < 3; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            //stred preskocit
            piBits += cshw - 6;
            //pravy okraj 2px + 1px kulaty roh
            for (j = 0; j < 3; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;
        }
        //spodni linka
        //for (i = 0; i < 2; i++)
        {
            unsigned int* piBits = (unsigned int*)tBits;
            //pravni radka
            for (j = 0; j < cshw; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;

            //druha radka - bez rohu pro efekt zakulaceni
            piBits = (unsigned int*)tBits;
            piBits++;
            for (j = 0; j < cshw - 2; j++)
            {
                *piBits++ = col + ((*piBits >> 2) & 0x003f3f3f);
            }
            tBits += 4 * pw;
        }
        return TRUE;
    }

    BOOL PaintMajorDirectories(CZBitmap* pix, HDC refDC)
    {
        if (this->_majorDirectoryCount == 0)
            return TRUE; //ok, ale nic nekreslim

        if ((pix == NULL) || (refDC == NULL))
            return FALSE; //chyba: spatne parametry

        int width = pix->GetWidth();
        int height = pix->GetHeight();

        BYTE* dta = pix->LockBits();

        if (dta != NULL)
        {
            NONCLIENTMETRICS ncm;
            ncm.cbSize = sizeof ncm;
            SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);

            HFONT hfnormal = CreateFontIndirect(&ncm.lfStatusFont);

            int titleheight = 18;

            HFONT hfold = SelectFont(refDC, hfnormal);

            SIZE sz;
            GetTextExtentPoint32(refDC, TEXT("..."), 3, &sz); //ziskani sirky "..." a vysky znaku
            titleheight = sz.cy + 5;

            SelectFont(refDC, hfold);

            CZBitmap* bgTxt = new CZBitmap(); //pomocna bitmapa pro vypocet stinu
            bgTxt->ResizeBitmap(refDC, width, titleheight + 3);
            RECT bgRect = {0, 0, width, titleheight};

            for (int i = 0; i < this->_majorDirectoryCount; i++)
            {
                SCDO_DirectoryItem* item = &this->_majorDirectories[i];
                HDC bgdc = bgTxt->CreateDC(refDC);

                SetBkColor(bgdc, RGB(0, 0, 0));
                SetTextColor(bgdc, RGB(255, 255, 255));
                SetBkMode(bgdc, OPAQUE);
                hfold = SelectFont(bgdc, hfnormal);
                CZFile* file = item->directory->GetFile();
                bgRect.right = item->width;
                ExtTextOut(bgdc, 5, 1, ETO_OPAQUE | ETO_CLIPPED, &bgRect, file->GetName(), (UINT)file->GetNameLen(), NULL);
                SelectFont(bgdc, hfold);

                BYTE* titledta = bgTxt->LockBits();

                int titlewidth = -1;
                unsigned int* txt_data = (unsigned int*)titledta;
                for (int ty = 0; ty < titleheight; ty++)
                {
                    for (int tx = 0; tx < item->width; tx++)
                    {

                        int cxp = ty * width + tx;
                        if ((txt_data[cxp] & 0xffff00) == 0) //neni pixel textu
                        {
                            //namixovat zakladni odstin 50% alpha
                            txt_data[cxp] = ((txt_data[cxp] & 0xff) + ((255 - (txt_data[cxp] & 0xff)) / 2));
                        }
                        else //pixel textu
                        {
                            if (tx > titlewidth)
                                titlewidth = tx; //prodlouzit max delku textu

                            txt_data[cxp] = 0xff; //pixel textu bude bily

                            for (int py = 0; py < 4; py++)
                            {
                                for (int px = 0; px < 4; px++)
                                {
                                    if ((px == 0) && (py == 0))
                                        continue; //stred neni treba

                                    int txa = CDirectoryOverlay_shadowdata[py * 4 + px];
                                    if (tx - px > 0)
                                    {
                                        if (ty - py > 0)
                                        {
                                            int txp = (ty - py) * width + (tx - px);
                                            if ((txt_data[txp] & 0xffff00) == 0)
                                            {
                                                txt_data[txp] = ((txt_data[txp] & 0xff) + ((255 - (txt_data[txp] & 0xff)) * txa / 255));
                                            }
                                        }
                                        if ((py != 0) && (ty + py < titleheight)) //pokud py = 0, pak nekreslit podruhe
                                        {
                                            int txp = (ty + py) * width + (tx - px);
                                            if ((txt_data[txp] & 0xffff00) == 0)
                                            {
                                                txt_data[txp] = ((txt_data[txp] & 0xff) + ((255 - (txt_data[txp] & 0xff)) * txa / 255));
                                            }
                                        }
                                    }
                                    if ((px != 0) && (tx + px < width)) //pokud px = 0, pak nekreslit podruhe
                                    {
                                        if (ty - py > 0)
                                        {
                                            int txp = (ty - py) * width + (tx + px);
                                            if ((txt_data[txp] & 0xffff00) == 0)
                                            {
                                                txt_data[txp] = ((txt_data[txp] & 0xff) + ((255 - (txt_data[txp] & 0xff)) * txa / 255));
                                            }
                                        }
                                        if ((py != 0) && (ty + py < titleheight)) //pokud py = 0, pak nekreslit podruhe
                                        {
                                            int txp = (ty + py) * width + (tx + px);
                                            if ((txt_data[txp] & 0xffff00) == 0)
                                            {
                                                txt_data[txp] = ((txt_data[txp] & 0xff) + ((255 - (txt_data[txp] & 0xff)) * txa / 255));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (titlewidth > 0)
                    titlewidth = titlewidth + 2;
                this->DrawMajorDirectory(dta, width, height, item->x, item->y, item->width, item->height, titledta, titlewidth, titleheight);
                bgTxt->UnlockBits();
            }
            if (bgTxt)
                delete bgTxt;

            pix->UnlockBits();

            HDC dc = pix->CreateDC(refDC);
            if (dc != NULL)
            {
                SetTextColor(dc, RGB(0, 0, 0));
                SetBkMode(dc, TRANSPARENT);

                hfold = SelectFont(dc, hfnormal);

                for (int i = 0; i < this->_majorDirectoryCount; i++)
                {
                    SCDO_DirectoryItem* item = &this->_majorDirectories[i];

                    RECT rct;
                    rct.left = item->x;
                    rct.right = item->x + item->width - 4;
                    rct.top = item->y;
                    rct.bottom = item->y + item->height - 2 - 1; //TODO: otazka: dojet s titulkem az ke spodnimu okraji??

                    CZFile* file = item->directory->GetFile();

                    ExtTextOut(dc, item->x + 5, item->y + 3, ETO_CLIPPED, &rct, file->GetName(), (UINT)file->GetNameLen(), NULL);
                }
                SelectFont(dc, hfold);

                pix->DeleteDC();
            }

            if (hfnormal)
                DeleteFont(hfnormal);
            return TRUE;
        }
        return FALSE;
    }

    BOOL PaintMinorDirectories(CZBitmap* pix, HDC refDC)
    {
        if (this->_minorDirectoryCount == 0)
            return TRUE; //ok, ale nic nekreslim

        if ((pix == NULL) || (refDC == NULL))
            return FALSE; //chyba: spatne parametry

        int width = pix->GetWidth();
        int height = pix->GetHeight();

        BYTE* dta = pix->LockBits();

        if (dta != NULL)
        {
            for (int i = 0; i < this->_minorDirectoryCount; i++)
            {
                SCDO_DirectoryItem* item = &this->_minorDirectories[i];

                this->DrawMinorDirectory(dta, width, height, item->x, item->y, item->width, item->height, 5);
            }

            pix->UnlockBits();

            return TRUE;
        }
        return FALSE;
    }

public:
    CDirectoryOverlay()
    {
        this->_majorDirectoryCount = 0;
        this->_minorDirectoryCount = 0;
    }

    BOOL Paint(CZBitmap* pix, HDC refDC)
    {
        BOOL res = this->PaintMinorDirectories(pix, refDC);
        res = this->PaintMajorDirectories(pix, refDC) && res;
        return res;
    }

    BOOL AddMajorDirectory(CCushionDirectory* dir, int x, int y, int w, int h)
    {
        if (this->_majorDirectoryCount == ARRAYSIZE(this->_majorDirectories))
            return FALSE;

        int newid = this->_majorDirectoryCount;
        this->_majorDirectories[newid].directory = dir;
        this->_majorDirectories[newid].x = x;
        this->_majorDirectories[newid].y = y;
        this->_majorDirectories[newid].width = w;
        this->_majorDirectories[newid].height = h;

        this->_majorDirectoryCount++;

        return TRUE;
    }

    BOOL AddMinorDirectory(CCushionDirectory* dir, int x, int y, int w, int h)
    {
        if (this->_minorDirectoryCount == ARRAYSIZE(this->_minorDirectories))
            return FALSE;

        int newid = this->_minorDirectoryCount;
        this->_minorDirectories[newid].directory = dir;
        this->_minorDirectories[newid].x = x;
        this->_minorDirectories[newid].y = y;
        this->_minorDirectories[newid].width = w;
        this->_minorDirectories[newid].height = h;

        this->_minorDirectoryCount++;

        return TRUE;
    }
    BOOL AddDirectory(CCushionDirectory* dir, int x, int y, int w, int h, int level)
    {
        if (level == 0)
            return this->AddMajorDirectory(dir, x, y, w, h);

        return this->AddMinorDirectory(dir, x, y, w, h);
        //return FALSE;
    }

    void ClearDirectory()
    {
        this->_majorDirectoryCount = 0;
        this->_minorDirectoryCount = 0;
    }
};
