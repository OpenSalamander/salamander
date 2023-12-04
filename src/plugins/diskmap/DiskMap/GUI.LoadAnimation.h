// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CAnimation.h"
#include "Utils.CZString.h"
#include "Utils.CStringFormatter.h"

#define LA_LID_SIZE 0
#define LA_LID_FILE 1
#define LA_LID_DIRS 2

#define LA_LID_MAX (LA_LID_DIRS + 1)

class CLoadAnimation : public CAnimation
{
protected:
    INT64 _info_datasize;
    int _info_filecount;
    int _info_dircount;

    CStringFormatter* _sFormatter;

    CZResourceString* _title;
    CZResourceString* _escinfo;
    CZResourceString* _headers[LA_LID_MAX];
    CZStringBuffer* _values[LA_LID_MAX];

    BOOL _valuesChanged;
    int _headersWidth;
    int _escinfoWidth;
    int _lineHeight;

    HFONT _hfnormal;

public:
    CLoadAnimation() : CAnimation(100)
    {
        this->_sFormatter = new CStringFormatter(LOCALE_USER_DEFAULT); //TODO: OnSystemChange...

        this->_hfnormal = NULL;

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof ncm;
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
        this->_hfnormal = CreateFontIndirect(&ncm.lfStatusFont);

        this->_title = new CZResourceString(IDS_DISKMAP_LOAD_TITLE);
        this->_escinfo = new CZResourceString(IDS_DISKMAP_LOAD_FOOTER);
        this->_headers[LA_LID_SIZE] = new CZResourceString(IDS_DISKMAP_LOAD_HSIZE);
        this->_headers[LA_LID_FILE] = new CZResourceString(IDS_DISKMAP_LOAD_HFILE);
        this->_headers[LA_LID_DIRS] = new CZResourceString(IDS_DISKMAP_LOAD_HDIRS);

        for (int i = 0; i < LA_LID_MAX; i++)
        {
            this->_values[i] = new CZStringBuffer(MAX_DATELEN);
        }

        this->_headersWidth = 0;
        this->_lineHeight = 0;
        this->_escinfoWidth = 0;

        this->Reset();
    }
    ~CLoadAnimation()
    {
        delete this->_title;
        delete this->_escinfo;
        for (int i = 0; i < LA_LID_MAX; i++)
        {
            delete this->_values[i];
            delete this->_headers[i];
        }

        if (this->_sFormatter)
            delete this->_sFormatter;
        if (this->_hfnormal)
            DeleteFont(this->_hfnormal);
        this->_hfnormal = NULL;
    }

    void CalcHeadersWidth(HDC hdc)
    {
        HANDLE fo = SelectObject(hdc, this->_hfnormal);
        SIZE sz;
        int width = 0;

        for (int i = 0; i < LA_LID_MAX; i++)
        {
            GetTextExtentPoint32(hdc, this->_headers[i]->GetString(), (int)this->_headers[i]->GetLength(), &sz);
            width = max(sz.cx, width);
        }
        GetTextExtentPoint32(hdc, this->_escinfo->GetString(), (int)this->_escinfo->GetLength(), &sz);
        this->_escinfoWidth = sz.cx;
        this->_lineHeight = sz.cy; //TODO nicer!
        SelectObject(hdc, fo);
        this->_headersWidth = width;
        //return TRUE;
        //return width;
    }
    /*
	int CalcValuesWidth(HDC hdc)
	{
		SIZE sz;
		int width = 0;

		for (int i = 0; i < LA_LID_MAX; i++)
		{
			GetTextExtentPoint32(hdc, this->_values[i]->GetString(), this->_values[i]->GetLength(), &sz);
			width = max(sz.cx, width);
		}
		return width;
	}*/
    void PrepareValues()
    {
        this->_sFormatter->FormatExplorerFileSize(this->_values[LA_LID_SIZE], this->_info_datasize);
        this->_sFormatter->FormatInteger(this->_values[LA_LID_FILE], this->_info_filecount);
        this->_sFormatter->FormatInteger(this->_values[LA_LID_DIRS], this->_info_dircount);
        this->_valuesChanged = FALSE;
    }

    void SetInfo(int filecount, int dircount, INT64 datasize)
    {
        _info_filecount = filecount;
        _info_dircount = dircount;
        _info_datasize = datasize;
        this->_valuesChanged = TRUE;
    }
    void Reset()
    {
        CAnimation::Reset();
        this->_info_datasize = 0;
        this->_info_dircount = 0;
        this->_info_filecount = 0;
        this->_valuesChanged = TRUE;
    }
    void Paint(HDC hdc, RECT rect)
    {
        const int BoxCount = 4;
        const int BoxLength = 24; //18
        const int BoxSize = 8;

        if (this->_headersWidth == 0)
            CalcHeadersWidth(hdc);
        if (this->_valuesChanged == TRUE)
            this->PrepareValues();

        int frame = this->_frame;

        int xPos = 0, yPos = 0;
        int rctW = (BoxSize + 1) * BoxLength + 3;
        int rctH = BoxSize + 4;
        int x, y, w, h;

        xPos = (rect.right - rctW) / 2;
        yPos = (rect.bottom - rctH) / 2;

        //BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, 0, 0, WHITENESS);

        HPEN pn_rct = CreatePen(PS_SOLID, 1, RGB(192, 192, 192));
        HPEN pn_bg = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

        LOGBRUSH lgbr;
        lgbr.lbStyle = BS_SOLID;
        lgbr.lbColor = RGB(0, 0, 0);
        HBRUSH br_bg = CreateBrushIndirect(&lgbr);
        lgbr.lbColor = RGB(192, 192, 192);
        HBRUSH br1 = CreateBrushIndirect(&lgbr);
        lgbr.lbColor = RGB(160, 160, 160);
        HBRUSH br2 = CreateBrushIndirect(&lgbr);
        lgbr.lbColor = RGB(128, 128, 128);
        HBRUSH br3 = CreateBrushIndirect(&lgbr);
        lgbr.lbColor = RGB(96, 96, 96);
        HBRUSH br4 = CreateBrushIndirect(&lgbr);

        HANDLE po = SelectObject(hdc, pn_rct);
        HANDLE bo = SelectObject(hdc, br_bg);
        HANDLE fo = SelectObject(hdc, this->_hfnormal);

        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(0, 0, 0));

        RECT rct = rect;
        rct.bottom = yPos;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rct, NULL, 0, NULL);
        rct.top = yPos;
        rct.right = xPos;
        rct.bottom = yPos + rctH;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rct, NULL, 0, NULL);
        rct.left = xPos + rctW;
        rct.right = rect.right;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rct, NULL, 0, NULL);

        //Y-position for next text line
        int texty = yPos + rctH + 6;

        //title + black BG above dividing line
        rct.left = rect.left;
        rct.top = yPos + rctH;
        rct.bottom = texty + this->_lineHeight + 2;
        ExtTextOut(hdc, xPos, texty, ETO_OPAQUE, &rct, this->_title->GetString(), (UINT)this->_title->GetLength(), NULL);

        texty += this->_lineHeight + 2;

        //left black line
        rct.top = texty;
        rct.right = xPos;
        rct.bottom = texty + 1;
        ExtTextOut(hdc, xPos, texty, ETO_OPAQUE, &rct, NULL, 0, NULL);

        //right black line
        rct.left = xPos + rctW;
        rct.right = rect.right;
        ExtTextOut(hdc, xPos, texty, ETO_OPAQUE, &rct, NULL, 0, NULL);

        //bottom black
        rct.left = rect.left;
        rct.top = texty + 1;
        rct.bottom = texty + 6;
        ExtTextOut(hdc, xPos, texty, ETO_OPAQUE, &rct, NULL, 0, NULL);

        //Dividing line
        MoveToEx(hdc, xPos, texty, NULL);
        LineTo(hdc, xPos + rctW, texty);

        texty += 6;

        for (int i = 0; i < LA_LID_MAX; i++)
        {
            rct.top = texty;
            rct.bottom = texty + this->_lineHeight;
            rct.left = rect.left;
            rct.right = xPos + this->_headersWidth + 6;
            ExtTextOut(hdc, xPos, texty, ETO_OPAQUE, &rct, this->_headers[i]->GetString(), (UINT)this->_headers[i]->GetLength(), NULL);
            rct.left = xPos + this->_headersWidth + 6;
            rct.right = rect.right;
            ExtTextOut(hdc, xPos + this->_headersWidth + 6, texty, ETO_OPAQUE, &rct, this->_values[i]->GetString(), (UINT)this->_values[i]->GetLength(), NULL);
            texty += this->_lineHeight;
        }
        rct.top = texty;
        rct.bottom = rect.bottom;
        rct.left = rect.left;
        rct.right = rect.right;

        ExtTextOut(hdc, xPos + rctW - this->_escinfoWidth, texty + 2, ETO_OPAQUE, &rct, this->_escinfo->GetString(), (UINT)this->_escinfo->GetLength(), NULL);

        //animation black background
        Rectangle(hdc, xPos, yPos, xPos + rctW, yPos + rctH);

        SelectObject(hdc, pn_bg);
        w = h = BoxSize + 2;

        x = xPos + (BoxSize + 1) * ((frame - 1) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br1);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * ((frame - 2) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br2);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * ((frame - 3) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br3);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * ((frame - 4) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br4);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * ((frame - 5) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br_bg);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * (BoxLength + BoxCount) - (BoxSize + 1) * ((frame - (BoxLength + 0)) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br1);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * (BoxLength + BoxCount) - (BoxSize + 1) * ((frame - (BoxLength + 1)) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br2);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * (BoxLength + BoxCount) - (BoxSize + 1) * ((frame - (BoxLength + 2)) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br3);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * (BoxLength + BoxCount) - (BoxSize + 1) * ((frame - (BoxLength + 3)) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br4);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        x = xPos + (BoxSize + 1) * (BoxLength + BoxCount) - (BoxSize + 1) * ((frame - (BoxLength + 4)) % (2 * (BoxLength + BoxCount))) + 1;
        y = yPos + 1;
        SelectObject(hdc, br_bg);
        if ((x > xPos) && (x + w < xPos + rctW))
            Rectangle(hdc, x, y, x + w, y + h);

        SelectObject(hdc, bo);
        DeleteObject(br_bg);
        DeleteObject(br1);
        DeleteObject(br2);
        DeleteObject(br3);
        DeleteObject(br4);

        SelectObject(hdc, po);
        DeleteObject(pn_rct);
        DeleteObject(pn_bg);

        SelectObject(hdc, fo);
    }
};
