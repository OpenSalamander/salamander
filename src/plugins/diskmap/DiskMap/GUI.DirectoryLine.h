// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CChildWindow.h"
#include "Utils.CZString.h"
#include "Utils.CStringFormatter.h"
#include "Utils.CZColors.h"
#include "GUI.ViewConnectorBase.h"

const TCHAR szDirectoryLineClass[] = TEXT("Zar.DM.DirLine.WC");

#define MAX_LONGSIZELEN 64
#define MAX_SHORTSIZELEN 32
#define LAST_NODEX 0x7fffffff

#define NODEID_NOXY -2
#define NODEID_NONE -1
#define NODEID_ROOT 0
#define NODEID_MAXP 0x7f
#define NODEID_SIZE 0x80
//#define NODEID_ICON 0x81 !! TODO !!

/*
 * /-------------------------------------------NODEID_NOXY---------------------------\
 * | NODEID_ICON | NODEID_ROOT\1\2\...\NODEID_MAXP      NODEID_NONE      NODEID_SIZE |
 * \---------------------------------------------------------------------------------/
 */

typedef struct drlDIRNODE
{
    int strpos;
    int xright;
} TDIRNODE, *PTDIRNODE;

class CDirectoryLine : public CChildWindow
{
protected:
    HFONT _hfont;

    HICON _hicon;
    INT_PTR _id; //icon ID

    CStringFormatter* _sFormatter;

    BOOL _trackingMouse;
    BOOL _isActive;

    //Data
    CZStringBuffer* _path;
    int _rootLen;
    INT64 _disksize;

    //cached values - size
    BOOL _sizeCached;
    TCHAR const* _pathstr; //points to this->_path (whole path) or this->_pathtemp (elipsis)
    int _pathlen;

    int _rootX;

    int _sizeX;
    CZStringBuffer* _sizestr;      //vybrany retezec
    CZStringBuffer* _sizestrlong;  //dlouha verze "1 234 567 bytes (1,23MB)"
    CZStringBuffer* _sizestrshort; //kratka verze "1,23MB"

    TDIRNODE _nodes[MAX_PATH / 2];
    int _nodeCount;
    int _mouseNode;

    //string for storing path copy with elispis
    TCHAR _pathtemp[MAX_PATH + 3];

    //cached values - colors: active/inactive
    COLORREF _backColor;
    COLORREF _rootColor;
    COLORREF _textColor;
    COLORREF _rootColorHot;
    COLORREF _textColorHot;

    //cached values - system
    int _textTop;
    int _iconTop;
    int _dotsWidth;
    int _cxSmIcon;
    int _cySmIcon;

    //shared objects
    CLogger* _logger;
    CFileInfoTooltip* _tooltip;
    CShellMenu* _shellmenu;

    CViewConnectorBase* _connector;

    HWND DoCreate(int left, int top, int width, int height)
    {
        return MyCreateWindow(
            0,
            szDirectoryLineClass,
            TEXT("Directory Line"),
            WS_CHILD | WS_VISIBLE,
            left, top,     //X, Y
            width, height, // WIDTH, HEIGHT
            NULL);
    }
    void UpdateValues()
    {
        if (this->_disksize >= 0)
        {
            this->_sFormatter->FormatHumanFileSize(this->_sizestrlong, this->_disksize);
            this->_sFormatter->FormatShortFileSize(this->_sizestrshort, this->_disksize);
        }

        TCHAR const* str = this->_path->GetString();
        int strlen = (int)this->_path->GetLength();

        int nodeid = 0;

        int p = this->_rootLen;

        this->_nodes[nodeid++].strpos = p;

        p++;
        while (p < strlen)
        {
            while (p < strlen && str[p] != TEXT('\\'))
                p++;
            this->_nodes[nodeid++].strpos = p;
            p++;
        }

        this->_nodeCount = nodeid;
        this->_nodes[this->_nodeCount].xright = LAST_NODEX;

        this->_sizeCached = FALSE;
        this->Repaint();
    }
    int SearchNode(int X)
    {
        if (X < 0)
            return NODEID_NONE;
        int ni = 0;
        while (X >= this->_nodes[ni].xright)
            ni++;
        if (this->_nodes[ni].xright == LAST_NODEX)
            return NODEID_NONE;
        return ni;
    }

    void GetPathRect(RECT* rect)
    {
        //POZOR! potreba synchronizovat s DoPaint()
        //TODO: pouzit i v DoPaint()
        GetClientRect(this->_hWnd, rect);
        rect->left += this->_cxSmIcon + 7 + /* margin */ 1 + /* inner border */ 1 + /* padding */ 1;
        rect->right -= /* margin */ 1 + /* inner border */ 1 + /* padding */ 1;

        rect->top += /* outer border */ 1 + /* margin */ 2 + /* inner border */ 1;
        rect->bottom -= /* outer border */ 1 + /* margin */ 2 + /* inner border */ 1;
    }
    BOOL IsInPathRect(int x, int y)
    {
        RECT rct;
        GetPathRect(&rct);
        return ((rct.top <= y && rct.bottom > y) && (rct.left - 1 <= x && rct.right + 1 > x));
    }
    int GetMouseNodeID(int x, int y)
    {
        if (IsInPathRect(x, y))
        {
            int ni = SearchNode(x);
            if (ni == NODEID_NONE)
            {
                if (x >= this->_sizeX && this->_sizeX >= 0)
                    ni = NODEID_SIZE;
            }
            return ni;
        }
        else
        {
            return NODEID_NOXY;
        }
    }
    void PrecalcTextSize(HDC hdc)
    {
        RECT rct;
        GetPathRect(&rct);

        int width = rct.right - rct.left;

        this->_sizeX = -1;

        SIZE sz;
        int sizewidth = 0;
        int pathwidth = 0;
        int dx[MAX_PATH + 3];
        GetTextExtentExPoint(hdc, this->_path->GetString(), (int)this->_path->GetLength(), 0, NULL, dx, &sz);
        pathwidth = sz.cx;
        if (this->_disksize >= 0)
        {
            GetTextExtentPoint32(hdc, this->_sizestrlong->GetString(), (int)this->_sizestrlong->GetLength(), &sz);
            sizewidth = sz.cx + 2;

            this->_sizestr = this->_sizestrlong;

            /*if (pathwidth <= width - sizewidth)
			{
				this->_sizestr = this->_sizestrlong;
			}
			else
			{
				GetTextExtentPoint32(hdc, this->_sizestrshort->GetString(), this->_sizestrshort->GetLength(), &sz);
				sizewidth = sz.cx + 4;
				this->_sizestr = this->_sizestrshort;
			}*/

            if (sizewidth < width)
            {
                width -= sizewidth;
                this->_sizeX = rct.right - sizewidth;
            }

            /*if (sz.cx < width)
			{
				width -= sz.cx + 1;
				this->_sizeX = rct.right - sz.cx - 1;
				//rct.right = this->_sizeX;
			}*/
        }

        if (pathwidth <= width)
        {
            this->_pathstr = this->_path->GetString();
            this->_pathlen = (int)this->_path->GetLength();

            this->_rootX = dx[this->_rootLen - 1] + rct.left;

            for (int i = 0; i < this->_nodeCount; i++)
            {
                int ppos = this->_nodes[i].strpos;
                this->_nodes[i].xright = dx[ppos - 1] + rct.left;
            }
        }
        else
        {
            TCHAR* ostr = this->_pathtemp;
            int ostrlen = 0;
            if (width > this->_dotsWidth)
            {
                TCHAR const* str = this->_path->GetString();
                int strlen = (int)this->_path->GetLength();

                int remwidth = width - this->_dotsWidth;

                int p = 0;
                while (p < strlen && dx[p] <= remwidth && str[p] != TEXT('\\'))
                {
                    ostr[p] = str[p];
                    p++;
                    ostrlen++;
                }
                BOOL found = (str[p] == TEXT('\\'));

                while (p < strlen && dx[p] <= remwidth && str[p] == TEXT('\\'))
                {
                    ostr[p] = str[p];
                    p++;
                    ostrlen++;
                }
                BOOL whole = (str[p] != TEXT('\\'));

                ostr[ostrlen++] = TEXT('.');
                ostr[ostrlen++] = TEXT('.');
                ostr[ostrlen++] = TEXT('.');

                if (found && whole)
                {
                    int rp = p;
                    int miswidth = pathwidth - width + this->_dotsWidth + dx[p - 1];

                    dx[strlen] = miswidth; //zarazka
                    while (miswidth > dx[rp])
                        rp++;

                    int i;
                    for (i = rp + 1; i < strlen; i++)
                    {
                        ostr[ostrlen++] = str[i];
                    }
                    if (this->_rootLen > rp)
                    {
                        this->_rootX = dx[this->_rootLen - 1] + rct.left - dx[rp] + this->_dotsWidth + dx[p - 1];
                    }
                    else
                    {
                        this->_rootX = dx[p - 1] + rct.left;
                    }
                    for (i = 0; i < this->_nodeCount; i++)
                    {
                        int ppos = this->_nodes[i].strpos;
                        if (ppos > rp)
                        {
                            this->_nodes[i].xright = dx[ppos - 1] + rct.left - dx[rp] + this->_dotsWidth + dx[p - 1];
                        }
                        else
                        {
                            this->_nodes[i].xright = dx[p - 1] + rct.left;
                        }
                    }
                }
                else
                {
                    this->_rootX = -1;
                    //TODO: nodes...
                    this->_nodes[0].xright = rct.left + width;
                    this->_nodes[1].xright = LAST_NODEX;
                }
            }
            else
            {
                this->_rootX = -1;
                //TODO: node clean - nic!
                this->_nodes[0].xright = LAST_NODEX;
            }
            ostr[ostrlen] = TEXT('\0');
            this->_pathstr = ostr;
            this->_pathlen = ostrlen;
        }

        if ((size_t)this->_rootLen == this->_path->GetLength())
        {
            this->_rootX = -1;
        }
    }

    void UpdateSizeCache(HDC hdc)
    {
        if (this->_sizeCached)
            return;
        PrecalcTextSize(hdc);
        this->_sizeCached = TRUE;
    }

    void OnSize(WPARAM state, int cx, int cy)
    {
        this->_sizeCached = FALSE;
    }

    void CalcColors(BOOL isActive)
    {
        if (this->_isActive)
        {
            this->_backColor = GetSysColor(COLOR_ACTIVECAPTION);
            //this->_rootColor = RGB(0,0,128);
            this->_textColor = GetSysColor(COLOR_CAPTIONTEXT);
            this->_rootColorHot = RGB(192, 96, 96); //RGB(128,0,0);
            this->_textColorHot = RGB(255, 128, 128);
        }
        else
        {
            this->_backColor = GetSysColor(COLOR_INACTIVECAPTION);
            //this->_rootColor = RGB(0,0,128);
            this->_textColor = GetSysColor(COLOR_INACTIVECAPTIONTEXT);
            this->_rootColorHot = RGB(0, 0, 0);
            this->_textColorHot = RGB(128, 128, 128);
        }

        COLORREF ch = GetSysColor(COLOR_HIGHLIGHT);
        this->_rootColor = CZColors::Grey(CZColors::Mix34(this->_textColor, this->_backColor));

        this->_textColorHot = CZColors::Mix(this->_textColor, ch);
        //this->_textColorHot = ch;
        this->_rootColorHot = CZColors::Mix(this->_rootColor, ch);

        if (!this->_isActive)
        {
            this->_textColorHot = CZColors::Grey(this->_textColorHot);
            this->_rootColorHot = CZColors::Grey(this->_rootColorHot);
        }
        //this->_rootColorHot = CZColors::Mix(this->_textColorHot, this->_backColor);
        //this->_rootColor = CZColors::Grey(this->_textColor);
    }

    void DoPaint(PAINTSTRUCT* pps)
    {
        RECT rct;
        HDC hdc = pps->hdc;
        GetClientRect(this->_hWnd, &rct);

        //edge painting
        DrawEdge(hdc, &rct, BDR_RAISEDINNER, BF_BOTTOM | BF_TOP);

        //background painting
        RECT trct;
        SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));

        //top part
        trct.top = rct.top + 1;
        trct.bottom = rct.top + 1 + 2;
        trct.left = rct.left;
        trct.right = rct.right;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &trct, NULL, 0, NULL);

        //left (behind icon) - ?TODO?: draw by parts to remove all flicker
        trct.top = rct.top + 3;
        trct.bottom = rct.bottom - 3;
        trct.right = rct.left + this->_cxSmIcon + 7 + 1;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &trct, NULL, 0, NULL);

        //right part
        trct.left = rct.right - 1;
        trct.right = rct.right;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &trct, NULL, 0, NULL);

        //bottom part
        trct.top = rct.bottom - 3;
        trct.bottom = rct.bottom - 1;
        trct.left = rct.left;
        trct.right = rct.right;
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &trct, NULL, 0, NULL);

        if (this->_hicon)
            DrawIconEx(hdc, rct.left + 1 + 3, rct.top + 1 + 3 + this->_iconTop, this->_hicon, 0, 0, 0, GetSysColorBrush(COLOR_BTNFACE), DI_NORMAL);

        /*RECT xrc;
		xrc.left = rct.left;
		xrc.top = rct.top - 2;
		xrc.bottom = xrc.top + this->_cySmIcon + 4 + 2;
		xrc.right = xrc.left + this->_cxSmIcon + 4 + 2;
		DrawEdge(hdc, &xrc, BDR_RAISEDINNER, BF_RECT);*/

        rct.left += this->_cxSmIcon + 7 + 1;
        rct.top += 3;
        rct.right -= 1;
        rct.bottom -= 3;

        DrawEdge(hdc, &rct, BDR_SUNKENOUTER, BF_ADJUST | BF_RECT);

        SetBkColor(hdc, this->_backColor);

        HFONT hfold = SelectFont(hdc, this->_hfont);

        UpdateSizeCache(hdc);

        int texttop = this->_textTop;

        if (this->_sizeX >= 0)
        {
            RECT xrc = rct;
            xrc.left = this->_sizeX;
            //size string - textColor/hotTextColor
            if (this->_mouseNode != NODEID_SIZE)
            {
                SetTextColor(hdc, this->_textColor);
            }
            else
            {
                SetTextColor(hdc, this->_textColorHot);
            }
            ExtTextOut(hdc, xrc.left + 2, rct.top + texttop, ETO_OPAQUE | ETO_CLIPPED, &xrc, this->_sizestr->GetString(), (UINT)this->_sizestr->GetLength(), NULL);
            rct.right = this->_sizeX;
        }

        //root string color:
        if ((this->_mouseNode >= NODEID_ROOT) && (this->_mouseNode <= NODEID_MAXP))
        {
            SetTextColor(hdc, this->_rootColorHot);
        }
        else
        {
            SetTextColor(hdc, this->_rootColor);
        }

        if (this->_rootX >= 0)
        {
            //root string + path
            trct = rct;
            if (this->_rootX > 0)
            {
                //root string
                trct.right = this->_rootX;
                ExtTextOut(hdc, rct.left + 1, rct.top + texttop, ETO_OPAQUE | ETO_CLIPPED, &trct, this->_pathstr, this->_pathlen, NULL);
                trct.right = rct.right;
            }
            trct.left = this->_rootX;
            BOOL isLastNodeHighlighted = (this->_mouseNode == this->_nodeCount - 1);

            //path string
            if ((this->_mouseNode > NODEID_ROOT) && (this->_mouseNode <= NODEID_MAXP))
            {
                //highlighted part
                SetTextColor(hdc, this->_textColorHot);

                if (!isLastNodeHighlighted)
                {
                    int oR = trct.right;
                    int nr = this->_nodes[this->_mouseNode].xright;
                    trct.right = nr;
                    ExtTextOut(hdc, rct.left + 1, rct.top + texttop, ETO_OPAQUE | ETO_CLIPPED, &trct, this->_pathstr, this->_pathlen, NULL);
                    trct.left = nr;
                    trct.right = oR;
                }
                else
                {
                    //whole string highlighted
                    ExtTextOut(hdc, rct.left + 1, rct.top + texttop, ETO_OPAQUE | ETO_CLIPPED, &trct, this->_pathstr, this->_pathlen, NULL);
                }
            }
            if (!isLastNodeHighlighted)
            {
                //normal - remaining part
                SetTextColor(hdc, this->_textColor);
                ExtTextOut(hdc, rct.left + 1, rct.top + texttop, ETO_OPAQUE | ETO_CLIPPED, &trct, this->_pathstr, this->_pathlen, NULL);
            }
        }
        else
        {
            //root string only
            ExtTextOut(hdc, rct.left + 1, rct.top + texttop, ETO_OPAQUE | ETO_CLIPPED, &rct, this->_pathstr, this->_pathlen, NULL);
        }

        SelectFont(hdc, hfold);
    }

    void OnLButtonDown(/*BOOL fDoubleClick, */ int x, int y, UINT keyFlags)
    {
        if (this->_mouseNode >= NODEID_ROOT && this->_mouseNode <= NODEID_MAXP)
        {
            int gotolevel = this->_nodeCount - this->_mouseNode - 1;
            if (gotolevel > 0)
            {
                this->_connector->DM_ZoomOut(gotolevel);
            }
        }
    }

    void OnContextMenu(HWND hwndContext, int xPos, int yPos)
    {
        if (this->_mouseNode >= NODEID_ROOT && this->_mouseNode <= NODEID_MAXP)
        {
            TCHAR buff[MAX_PATH];
            size_t plen = this->_nodes[this->_mouseNode].strpos;
            size_t l = this->_path->GetSubString(0, plen, buff, ARRAYSIZE(buff));
            if (l > 0)
            {
                int icmd = 0;
                int gotolevel = this->_nodeCount - this->_mouseNode - 1;
#ifdef SALAMANDER
                if (this->_connector->GetSalamander() != NULL)
                {
                    icmd = this->_shellmenu->ShowDirMenu(buff, xPos, yPos, this->_connector->GetSalamander()->CanOpenFolder(), gotolevel > 0);
                }
                else
                {
                    icmd = this->_shellmenu->ShowDirMenu(buff, xPos, yPos, FALSE, gotolevel > 0);
                }
#else
                icmd = this->_shellmenu->ShowDirMenu(buff, xPos, yPos, gotolevel > 0);
#endif
                if (icmd == IDM_GOTO)
                {
                    this->_connector->DM_ZoomOut(gotolevel);
                }
#ifdef SALAMANDER
                else if (icmd == IDM_OPEN)
                {
                    if (this->_connector->GetSalamander())
                        this->_connector->GetSalamander()->OpenFolder(buff);
                }
#endif
            }
        }
    }

    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct)
    {
        this->_x = lpCreateStruct->x;
        this->_y = lpCreateStruct->y;
        this->_width = lpCreateStruct->cx;
        this->_height = lpCreateStruct->cy;

        return true;
    }

    void OnDestroy()
    {
        if (this->_hicon)
            DestroyIcon(this->_hicon);
        this->_hicon = NULL;
    }

public:
    CDirectoryLine(HWND hwndParent, int top, CLogger* logger, CFileInfoTooltip* tooltip, CShellMenu* shellmenu) : CChildWindow(hwndParent)
    {
        this->_isActive = TRUE;
        this->_trackingMouse = FALSE;

        this->_sizeCached = FALSE;

        this->_sFormatter = NULL;

        this->_path = new CZStringBuffer(MAX_PATH + 4);
        this->_rootLen = 0;
        this->_disksize = -1;
        //this->_disksize = 12345678;

        this->_mouseNode = NODEID_NONE;

        this->_sizestrlong = new CZStringBuffer(MAX_LONGSIZELEN);
        this->_sizestrshort = new CZStringBuffer(MAX_SHORTSIZELEN);
        this->_sizestr = this->_sizestrlong;

        this->_logger = logger;
        this->_tooltip = tooltip;
        this->_shellmenu = shellmenu;

        this->_hfont = NULL;
        this->_dotsWidth = 0;
        this->_id = 0;

        this->_connector = NULL;

        this->OnSystemChange();

        this->UpdateValues();

        this->Create(top);
    }
    ~CDirectoryLine()
    {
        if (this->_path)
            delete this->_path;
        this->_path = NULL;
        if (this->_sFormatter)
            delete this->_sFormatter;
        this->_sFormatter = NULL;
        if (this->_sizestrlong)
            delete this->_sizestrlong;
        this->_sizestrlong = NULL;
        if (this->_sizestrshort)
            delete this->_sizestrshort;
        this->_sizestrshort = NULL;

        if (this->_hfont)
            DeleteObject(this->_hfont);
        this->_hfont = NULL;

        this->Destroy();
    }

    void SetActiveState(BOOL isActive)
    {
        if (this->_isActive != isActive)
        {
            this->_isActive = isActive;
            CalcColors(isActive);
            this->Repaint(FALSE);
        }
    }

    HWND Create(int top)
    {
        this->SetMargins(0, top, 0, NO_MARGIN);

        return this->CChildWindow::Create(this->_x, this->_y, this->_width, this->_height);
    }

    BOOL SetPath(CZString const* path)
    {
        this->_id++;

        CZIconLoader::BeginAsyncLoadIcon(path, SHGFI_SMALLICON | SHGFI_ADDOVERLAYS, this->_hWnd, WM_APP_ICONLOADED, (void*)this->_id)->SetSelfDelete(TRUE);

        this->_path->AppendAt(path, 0);
        this->_rootLen = (int)this->_path->GetLength();

        this->UpdateValues();
        return TRUE;
    }
    BOOL SetSubPath(CZString const* subpath) //subpath NEzacina lomitkem
    {
        if (subpath->GetLength() == 0)
        {
            this->_path->Left(this->_rootLen);
        }
        else
        {
            //pokud root "C:\", tak jiz konci zpetnym lomitkem
            if (this->_path->IsCharAt(TEXT('\\'), this->_rootLen - 1)) //pokud by nahodou _rootLen bylo 0, tak pretece na MAX_INT a vrati FALSE
            {
                this->_path->AppendAt(subpath, this->_rootLen);
            }
            else
            {
                //normalni cesty nekonci lomitkem, tak pridame
                this->_path->AppendAt(TEXT('\\'), this->_rootLen);
                this->_path->AppendAt(subpath, this->_rootLen + 1);
            }
        }

        this->UpdateValues();
        return TRUE;
    }
    BOOL SetDiskSize(INT64 disksize)
    {
        this->_disksize = disksize;

        this->UpdateValues();
        return TRUE;
    }
    void OnIconLoad(HICON hicon, INT_PTR id)
    {
        if (id == this->_id)
        {
            if (this->_hicon)
                DestroyIcon(this->_hicon);
            this->_hicon = hicon;
            this->Repaint(TRUE); //kresli se jen ikona pres prazdne misto
        }
        else
        {
            DestroyIcon(hicon);
        }
    }
    void OnSystemChange()
    {
        if (this->_hfont)
            DeleteFont(this->_hfont);
        if (this->_sFormatter)
            delete this->_sFormatter;

        this->_sFormatter = new CStringFormatter(LOCALE_USER_DEFAULT);

        this->_cxSmIcon = GetSystemMetrics(SM_CXSMICON);
        this->_cySmIcon = GetSystemMetrics(SM_CYSMICON);

        this->CalcColors(this->_isActive);

        LOGFONT lf;
        SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0);
        lf.lfItalic = FALSE; //HACK: natvrdo potlacena kurziva, aby nepretekaly znaky pres sirku (Overhang/Underhang: GetCharABCWidths() a GetCharABCWidthsFloat() )
        this->_hfont = CreateFontIndirect(&lf);

        HDC hdc = GetDC(this->_hWnd); //pokud NULL, tak neva
        HFONT hfold = SelectFont(hdc, this->_hfont);

        SIZE sz;
        GetTextExtentPoint32(hdc, TEXT("..."), 3, &sz); //ziskani sirky "..." a vysky znaku
        this->_dotsWidth = sz.cx;
        this->_height = max(sz.cy, this->_cySmIcon) + 8;

        this->_textTop = max(0, (this->_height - 8 - sz.cy) / 2);
        this->_iconTop = max(0, (this->_height - 8 - this->_cySmIcon) / 2);

        SelectFont(hdc, hfold);

        ReleaseDC(this->_hWnd, hdc);
    }
    void OnSysColorChange()
    {
        this->CalcColors(this->_isActive);
    }

    void OnMouseMove(int x, int y, WPARAM keyFlags)
    {
        CZDirectory* dir = NULL;
        int ni = GetMouseNodeID(x, y);
        if (ni != NODEID_NOXY)
        {
            if (ni != this->_mouseNode)
            {
                this->_mouseNode = ni;
                if (!this->_trackingMouse && ni != NODEID_NONE)
                {
                    TRACKMOUSEEVENT trme;
                    trme.cbSize = sizeof trme;
                    trme.dwFlags = TME_LEAVE;
                    trme.hwndTrack = this->_hWnd;
                    this->_trackingMouse = TrackMouseEvent(&trme);
                }
                this->Repaint();
            }
            if (this->_mouseNode >= NODEID_ROOT && this->_mouseNode <= NODEID_MAXP)
            {
                int parentlevel = this->_nodeCount - this->_mouseNode - 1;
                dir = this->_connector->DM_GetViewDirectory(parentlevel);
            }
            else if (this->_mouseNode == NODEID_SIZE)
            {
                dir = this->_connector->DM_GetViewDirectory(0);
            }
        }
        else
        {
            if (this->_mouseNode != NODEID_NONE)
            {
                this->_mouseNode = NODEID_NONE;
                this->Repaint();
            }
        }
        if (dir)
        {
            this->ShowTooltip(dir, x, y);
        }
        else
        {
            this->HideTooltip();
        }
    }
    void ShowTooltip(CZDirectory* file, int x, int y)
    {
        if (this->_tooltip)
        {
            this->_tooltip->ShowDirectoryInfo(file);

            POINT pt;
            pt.x = x;
            pt.y = y;
            ClientToScreen(this->_hWnd, &pt);
            this->_tooltip->Move(pt.x, pt.y, true);

            this->_tooltip->Show(SW_SHOWNOACTIVATE);

            if (!this->_trackingMouse)
            {
                TRACKMOUSEEVENT trme;
                trme.cbSize = sizeof trme;
                trme.dwFlags = TME_LEAVE;
                trme.hwndTrack = this->_hWnd;
                this->_trackingMouse = TrackMouseEvent(&trme);
            }
        }
    }
    void HideTooltip()
    {
        if (this->_tooltip)
            this->_tooltip->Hide();
    }
    void OnMouseLeave()
    {
        this->_trackingMouse = FALSE;
        if (this->_mouseNode != NODEID_NONE)
        {
            this->_mouseNode = NODEID_NONE;
            this->Repaint();
        }
        if (this->_tooltip)
            this->_tooltip->Hide();
    }
    void SetConnector(CViewConnectorBase* connector)
    {
        this->_connector = connector;
    }

    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_MOUSEMOVE:
            return this->OnMouseMove((short)LOWORD(lParam), (short)HIWORD(lParam), wParam), 0;
        case WM_MOUSELEAVE:
            return this->OnMouseLeave(), 0;

            //case WM_APP_LOGUPDATED: return this->OnLogUpdated(wParam, lParam), 0;

        case WM_LBUTTONDOWN:
            return this->OnLButtonDown(/*FALSE,*/ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0;
            //case WM_RBUTTONDOWN:	return this->OnRButtonDown(/*FALSE,*/ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam), (UINT)(wParam)), 0;

        case WM_CONTEXTMENU:
            return this->OnContextMenu((HWND)wParam, (short)LOWORD(lParam), (short)HIWORD(lParam)), 0;

        case WM_APP_ICONLOADED:
            return this->OnIconLoad((HICON)wParam, (INT_PTR)lParam), 0;

        case WM_SIZE:
            return this->OnSize(wParam, (short)LOWORD(lParam), (short)HIWORD(lParam)), 0;

        case WM_ERASEBKGND:
            return TRUE;
        case WM_PAINT:
            return this->OnPaint(), 0;

        case WM_CREATE:
            return this->OnCreate((LPCREATESTRUCT)lParam) ? 0 : (LRESULT)-1L;

        case WM_DESTROY:
            return this->OnDestroy(), 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    static BOOL RegisterClass()
    {
        static ATOM a = NULL;
        if (!a)
        {
            WNDCLASSEX wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);

            wcex.style = 0;
            wcex.lpfnWndProc = CDirectoryLine::s_WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = CWindow::s_hInstance;
            wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
            //wcex.hbrBackground	= (HBRUSH)(COLOR_BTNTEXT + 1);
            wcex.lpszMenuName = NULL;
            wcex.lpszClassName = szDirectoryLineClass;
            wcex.hIcon = NULL;
            wcex.hIconSm = NULL;

            a = ::RegisterClassEx(&wcex);
        }
        BOOL ret = (a != NULL);
        return ret;
    }
    static BOOL UnregisterClass()
    {
        BOOL ret = ::UnregisterClass(szDirectoryLineClass, CWindow::s_hInstance);
        if (!ret)
            TRACE_E("UnregisterClass(szDirectoryLineClass) has failed");
        return ret;
    }
};
