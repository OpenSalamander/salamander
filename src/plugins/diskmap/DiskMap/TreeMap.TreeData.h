// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define PRECISSION_THRESHOLD 0.00001
#define ARRAY_BLOCKSIZE_CCUSHIONLIST 32
#define ARRAY_BLOCKSIZE_CCUSHIONROWLIST 16

enum EDirection
{
    dirNULL,
    dirHorizontal,
    dirVertical
};
enum EDirectionStyle
{
    dsLonger,
    dsShorter,
    dsFixedHorizontal,
    dsFixedVertical,
    dsChangingHorizontal,
    dsChangingVertical,
    dsSquare
};
enum ERenderDecision
{
    rdSameRow,
    rdNewRow
};

typedef struct treRECT
{
    int x;
    int y;
    int w;
    int h;
} TRECT, *PTRECT;

typedef struct treRECTF
{
    double x;
    double y;
    double w;
    double h;
} TRECTF, *PTRECTF;

typedef struct trePOINTF
{
    double x;
    double y;
} TPOINTF, *PTPOINTF;

typedef struct treSIZEF
{
    double w;
    double h;
} TSIZEF, *PTSIZEF;

class CCushionRow;

class CTreeMapRendererBase
{
protected:
    EDirectionStyle _dirstyle;

public:
    CTreeMapRendererBase(EDirectionStyle dirstyle)
    {
        this->_dirstyle = dirstyle;
    }
    /*virtual COLORREF Colorize(CCushion *cs)
	{
		CZFile *f = cs->GetFile();
		//COLORREF c = 0;//black?
		TCHAR *e = f->GetExt();
		int r = 0, g = 0, b = 0;
		if (e)
		{
			e++; //za tecku
			if (e != '\0')
			{
				if ((*e >= '0') && (*e <= '9')) r = (*e - '0') * 25;
				else if ((*e >= 'a') && (*e <= 'z')) r = (*e - 'a') * 9;
				else if ((*e >= 'A') && (*e <= 'Z')) r = (*e - 'A') * 9;
				else r = 255;
				e++;
			}
			if (e != '\0')
			{
				if ((*e >= '0') && (*e <= '9')) g = (*e - '0') * 25;
				else if ((*e >= 'a') && (*e <= 'z')) g = (*e - 'a') * 9;
				else if ((*e >= 'A') && (*e <= 'Z')) g = (*e - 'A') * 9;
				else g = 255;
				e++;
			}
			if (e != '\0')
			{
				if ((*e >= '0') && (*e <= '9')) b = (*e - '0') * 25;
				else if ((*e >= 'a') && (*e <= 'z')) b = (*e - 'a') * 9;
				else if ((*e >= 'A') && (*e <= 'Z')) b = (*e - 'A') * 9;
				else b = 255;
				e++;
			}
		}
		return RGB(r, g, b);
	}*/
    virtual EDirection GetDirection(int w, int h, int level, EDirection lastdir, EDirection parentdir)
    {
        switch (this->_dirstyle)
        {
        case dsLonger:
            if (lastdir != dirNULL)
                return lastdir;
            if (w > h)
                return dirHorizontal;
            return dirVertical;
            break;
        case dsShorter:
            if (lastdir != dirNULL)
                return lastdir;
            if (w > h)
                return dirVertical;
            return dirHorizontal;
            break;
        case dsFixedHorizontal:
            return dirHorizontal;
            break;
        case dsFixedVertical:
            return dirVertical;
            break;
        case dsChangingHorizontal:
            if (lastdir != dirNULL)
                return lastdir;
            if (parentdir == dirVertical)
                return dirHorizontal;
            if (parentdir == dirHorizontal)
                return dirVertical;
            if (w > h)
                return dirHorizontal;
            return dirVertical;
            break;
        case dsChangingVertical:
            if (lastdir != dirNULL)
                return lastdir;
            if (parentdir == dirVertical)
                return dirHorizontal;
            if (parentdir == dirHorizontal)
                return dirVertical;
            if (w > h)
                return dirVertical;
            return dirHorizontal;
            break;
        case dsSquare:
            if (w > h)
                return dirVertical;
            return dirHorizontal;
            break;
        }
        return dirNULL; //TODO: chyba!
    }
    //virtual ERenderDecision Decide(CCushionRow *csr, double width, double length, int level, EDirection lastdir, INT64 datasize, INT64 remainingdata, int i, int remainingfiles)
    virtual ERenderDecision Decide(CCushionRow* csr, double width, double length, INT64 datasize, INT64 remainingdata) = 0;
    //{
    //return rdSameRow; //sloupce
    //	return rdNewRow;
    //}
};
