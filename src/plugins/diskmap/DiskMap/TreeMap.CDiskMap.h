// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Utils.CRegionAllocator.h"
#include "Utils.CPixMap.h"
#include "Utils.CZBitmap.h"
#include "TreeMap.TreeData.h"
#include "TreeMap.TreeData.CTreeMapRenderer.h"
#include "TreeMap.TreeData.CTreeMap.h"
#include "TreeMap.TreeData.CCushionDirectory.h"
#include "TreeMap.Graphics.CCushionGraphics.h"
#include "System.CLogger.h"
#include "System.RWLock.h"
#include "TreeMap.FileData.CZRoot.h"

#include "TreeMap.DiskMap.CDiskMapOverlay.h"
#include "TreeMap.DiskMap.CDirectoryOverlay.h"

//#define SELECTED_COLOR RGB(255, 192, 0)
#define SELECTED_COLOR RGB(128, 192, 255) //TODO: COLOR_HIGHLIGHTED + some effect

// opatreni proti runtime check failure v debug verzi: puvodni verze makra pretypovava rgb na WORD,
// takze hlasi ztratu dat (RED slozky)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

class CDiskMap
{
protected:
    int _mapWidth;
    int _mapHeight;

    CCushionGraphics* _graphics;

    HWND _hWnd;

    CZRoot* _rootdir;
    CZDirectory* _viewdir;
    CWorkerThread* _populateworker;

    CTreeMap* _map;

    CPixMap* _mapPix;

    CZBitmap* _outBmp; //pro to co je videt... to se zoomuje pri ReSize/Animace

    BOOL _mapReady;
    BOOL _viewValid;
    //BOOL _isDimmed;

    BOOL _directoryOverlayVisible;

    CLogger* _logger;

    CRegionAllocator* _allocator;

    CZFile* _selectedFile;
    CCushion* _selectedCushion;
    CSelectedCushionOverlay* _selectedOverlay;
    CDirectoryOverlay* _directoryOverlay;

    RECT _selectedRect;

#ifdef TIMINGTEST
    double _calctime;
    double _drawtime;
    double _hbmptime;
    int _cushionCount;
    int _drawcount;
#endif

    void SetSelectedCushion(CCushion* cushion, int x, int y, int w, int h)
    {
        this->_selectedCushion = cushion;
        if (cushion == NULL)
        {
            this->_selectedFile = NULL;

            this->_selectedOverlay->ClearCushion();
        }
        else
        {
            this->_selectedFile = cushion->GetFile();

            this->_selectedOverlay->SetCushion(x, y, w, h);

            this->_selectedRect.left = x;
            this->_selectedRect.top = y;
            this->_selectedRect.right = x + w;
            this->_selectedRect.bottom = y + h;
        }
    }
    void ClearSelectedCushion()
    {
        this->_selectedCushion = NULL;
        this->_selectedOverlay->ClearCushion();
        this->_directoryOverlay->ClearDirectory();
    }
    void ClearSelectedFile()
    {
        this->_selectedFile = NULL;
        ClearSelectedCushion();
    }

    void DrawCushion(BYTE* pix, int pw, int ph, int cshx, int cshy, int cshw, int cshh, COLORREF color, int level)
    {
        if (cshw == 0 || cshh == 0)
            return;

#ifdef TIMINGTEST
        this->_drawcount++;
#endif

        int r, g, b;

        level = min(level, 5);
        r = GetRValue(color) * (8 - level) / 8;
        g = GetGValue(color) * (8 - level) / 8;
        b = GetBValue(color) * (8 - level) / 8;

        this->_graphics->DrawCushion(pix, pw, ph, cshx, cshy, cshw, cshh, RGB(r, g, b));
    }

    void DrawCCushionDirectory(BYTE* pix, int width, int height, CCushionDirectory* csd, int level = 0)
    {
        if ((width == 0 || height == 0) && (this->_selectedCushion != NULL))
            return;

        for (CCushionRow* row = csd->GetFirstRow(); row != NULL; row = row->GetNext())
        {
            RECT rct;
            //			if (level > 0)
            //				Sleep(0);
            row->GetLocation(rct);
            int sx = 0;

            for (CCushion* cs = row->GetFirstCushion(); cs != NULL; cs = cs->GetNext())
            {
#ifdef TIMINGTEST
                this->_cushionCount++;
#endif
                if (this->_selectedCushion == NULL && this->_selectedFile != NULL && this->_selectedFile == cs->GetFile())
                {
                    //this->_selectedCushion = cs;
                    int scx, scy, scw, sch;
                    if (row->GetDirection() == dirVertical)
                    {
                        scx = rct.left;
                        scy = rct.top + sx;
                        scw = row->GetWidth();
                        sch = cs->GetSize();
                    }
                    else
                    {
                        scx = rct.left + sx;
                        scy = rct.top;
                        scw = cs->GetSize();
                        sch = row->GetWidth();
                    }
                    this->SetSelectedCushion(cs, scx, scy, scw, sch);
                    //this->DrawSelectedCushion(scx, scy, scw, sch);
                    //this->_selectedOverlay->SetCushion(scx, scy, scw, sch);
                }
                if (cs->IsDirectory())
                {
                    DrawCCushionDirectory(pix, width, height, (CCushionDirectory*)cs, level + 1);
                    //sx += cs->GetSize();

                    if (level <= 1)
                    {
                        if (row->GetDirection() == dirVertical)
                        {
                            //if (min(row->GetWidth(), cs->GetSize()) > MINDIRSIZE)
                            //DrawDirectory(pix, width, height, rct.left, rct.top + sx, row->GetWidth(), cs->GetSize(), 18, level);
                            //sx += cs->GetSize();
                            this->_directoryOverlay->AddDirectory((CCushionDirectory*)cs, rct.left, rct.top + sx, row->GetWidth(), cs->GetSize(), level);
                        }
                        else
                        {
                            //if (min(row->GetWidth(), cs->GetSize()) > MINDIRSIZE)
                            //DrawDirectory(pix, width, height, rct.left + sx, rct.top, cs->GetSize(), row->GetWidth(), 18, level);
                            //sx += cs->GetSize();
                            this->_directoryOverlay->AddDirectory((CCushionDirectory*)cs, rct.left + sx, rct.top, cs->GetSize(), row->GetWidth(), level);
                        }
                    }
                }
                else
                {
                    if (row->GetDirection() == dirVertical)
                    {
                        this->DrawCushion(pix, width, height, rct.left, rct.top + sx, row->GetWidth(), cs->GetSize(), cs->GetColor(), level);
                        //sx += cs->GetSize();
                    }
                    else
                    {
                        this->DrawCushion(pix, width, height, rct.left + sx, rct.top, cs->GetSize(), row->GetWidth(), cs->GetColor(), level);
                        //sx += cs->GetSize();
                    }
                }
                sx += cs->GetSize();
            }
        }
    }

public:
    CDiskMap(HWND hWnd, CLogger* logger)
    {
        this->_hWnd = hWnd;

        this->_mapWidth = 0;
        this->_mapHeight = 0;

        this->_rootdir = NULL;
        this->_viewdir = NULL;
        this->_populateworker = NULL;

        this->_map = NULL;

        this->_mapPix = new CPixMap();

        this->_outBmp = new CZBitmap();

        this->_mapReady = FALSE;
        this->_viewValid = FALSE;
        //this->_isDimmed = FALSE;

        this->_directoryOverlayVisible = TRUE;

        this->_graphics = new CCushionGraphics();
        //this->_graphics->LoadFromFile(TEXT("cushion1.ztc"));

        this->_selectedCushion = NULL;
        this->_selectedFile = NULL;

        //this->_selectedColor = GetSysColor(COLOR_HIGHLIGHT); //TODO!
        this->_selectedOverlay = new CSelectedCushionOverlay(SELECTED_COLOR, this->_graphics);
        this->_directoryOverlay = new CDirectoryOverlay();

        this->_logger = logger;

        this->_allocator = new CRegionAllocator();
    }

    ~CDiskMap()
    {
        this->Abort();

        if (this->_selectedOverlay)
            delete this->_selectedOverlay;
        this->_selectedOverlay = NULL;
        if (this->_directoryOverlay)
            delete this->_directoryOverlay;
        this->_directoryOverlay = NULL;

        if (this->_graphics)
            delete this->_graphics;
        this->_graphics = NULL;
        if (this->_map)
            delete this->_map;
        this->_map = NULL;
        if (this->_rootdir)
            delete this->_rootdir;
        this->_rootdir = NULL;
        this->_viewdir = NULL;

        delete this->_allocator;
        this->_allocator = NULL;

        if (this->_mapPix)
            delete this->_mapPix;
        this->_mapPix = NULL;

        if (this->_outBmp)
            delete this->_outBmp;
        this->_outBmp = NULL;
    }

    BOOL LoadGraphicsFromResource(HINSTANCE hInst, TCHAR const* name)
    {
        if (this->_graphics == NULL)
            return FALSE;
        if (name == NULL)
        {
            this->_graphics->ResetPix();
            return TRUE;
        }
        return this->_graphics->LoadFromResource(hInst, name);
    }
    BOOL CanPopulate()
    {
        return (this->_populateworker == NULL);
    }
    //FIXME: clustersize hack
#ifdef SALAMANDER
    void PopulateAsync(TCHAR const* name, size_t namelen, int clustersize, int sortorder = FILESIZE_DISK)
#else
    void PopulateAsync(TCHAR const* name, size_t namelen = 0, int sortorder = FILESIZE_DISK)
#endif

    {
        this->_mapReady = FALSE;
        //this->InvalidateMap();
        this->Invalidate();
        if (this->_map)
        {
            delete this->_map;
            this->_map = NULL;
        }

        this->ClearSelectedFile();
        //this->_selectedFile = NULL;
        //this->SetSelectedCushion(NULL, 0, 0, 0, 0);
        //this->_selectedCushion = NULL;
        //this->_selectedOverlay->ClearCushion();

        if (this->_rootdir)
            delete this->_rootdir;
        this->_rootdir = NULL;
        this->_viewdir = NULL;

        this->_rootdir = new CZRoot(name, this->_logger, sortorder);
#ifdef SALAMANDER
        this->_rootdir->SetClusterSize(clustersize);
#endif

        //this->_rootdir->SyncPopulate();
        //PostMessage(this->_hWnd, WM_APP_DIRPOPULATED, 0, (LPARAM)this->_rootdir);
        this->_populateworker = this->_rootdir->BeginAsyncPopulate(this->_hWnd, WM_APP_DIRPOPULATED);
    }
    void OnDirPopulated(WPARAM wParam, LPARAM lParam) //vzdy ve hlavnim vlaknu
    {
        CZRoot* czdir = (CZRoot*)lParam;
        if (czdir == this->_rootdir && wParam == TRUE)
        {
            delete this->_populateworker;
            this->_populateworker = NULL;
            this->_viewdir = this->_rootdir;
        }
        else
        {
            if (wParam == TRUE)
                delete czdir;
        }
    }

    BOOL GetDirectoryOverlayVisibility()
    {
        return this->_directoryOverlayVisible;
    }

    void SetDirectoryOverlayVisibility(BOOL isVisible)
    {
        if (this->_directoryOverlayVisible != isVisible)
        {
            this->_directoryOverlayVisible = isVisible;
            this->Invalidate();
        }
    }

    void GetStats(int& filecount, int& dircount, INT64& size)
    {
        if (this->_rootdir)
        {
            this->_rootdir->GetStats(filecount, dircount, size);
        }
        else
        {
            filecount = 0;
            dircount = 0;
            size = 0;
        }
    }

    void Invalidate()
    {
        this->_viewValid = FALSE;
    }

    void PrepareView(int width, int height, CTreeMapRendererBase* renderer)
    {
        //if (width == this->_mapWidth && height == this->_mapHeight) return;

        this->_mapReady = FALSE;
        this->Invalidate();

        //this->_selectedCushion = NULL;
        //this->_selectedOverlay->ClearCushion();
        this->ClearSelectedCushion();

        if (this->_map)
        {
            delete this->_map; //TODO: optimize!
            this->_map = NULL;
        }

        if (this->_viewdir)
        {
            this->_map = new CTreeMap(this->_allocator, this->_viewdir, renderer, width, height);
        }
        if (this->_map)
        {

#ifdef TIMINGTEST
            LARGE_INTEGER lt1, lt2, lf;
            QueryPerformanceFrequency(&lf);
            QueryPerformanceCounter(&lt1);
#endif

            this->_map->Prepare(FILESIZE_DISK); //TODO: optimize!

#ifdef TIMINGTEST
            QueryPerformanceCounter(&lt2);
            this->_calctime = (double)(lt2.QuadPart - lt1.QuadPart) / lf.QuadPart;
#endif

            CCushionDirectory* cshr = this->_map->GetRoot();
            BYTE* pix = this->_mapPix->AllocatePixMap(width, height);
            if (pix != NULL)
            {
#ifdef TIMINGTEST
                this->_cushionCount = 0;
                QueryPerformanceCounter(&lt1);
                this->_drawcount = 0;
#endif

                this->DrawCCushionDirectory(pix, width, height, cshr);

#ifdef TIMINGTEST
                QueryPerformanceCounter(&lt2);
                this->_drawtime = (double)(lt2.QuadPart - lt1.QuadPart) / lf.QuadPart;
#endif

                this->_mapWidth = width;
                this->_mapHeight = height;
                this->_mapReady = TRUE;
                //this->_isDimmed = FALSE;
            }
        }
    }
    CCushionHitInfo* GetCushionByLocation(int x, int y)
    {
        if (this->_map)
            return this->_map->GetCushionByLocation(x, y);
        return NULL;
    }

    /*void Dim()
	{
		if (this->_isDimmed) return;
		this->_mapPix->Dim();
		this->_outPix->Dim();
		//this->InvalidateMap();
		this->_isDimmed = TRUE;
	}*/

    BOOL SelectCushion(CCushionHitInfo* cshi)
    {
        if (this->_selectedCushion == cshi->GetCushion())
            return FALSE;

        //this->_selectedCushion = cshi->GetCushion();
        //this->_selectedFile = cshi->GetCushion()->GetFile();

        this->SetSelectedCushion(cshi->GetCushion(), cshi->GetPosX(), cshi->GetPosY(), cshi->GetWidth(), cshi->GetHeight());

        //this->DrawSelectedCushion(cshi->GetPosX(), cshi->GetPosY(), cshi->GetWidth(), cshi->GetHeight());
        //this->_selectedOverlay->SetCushion(cshi->GetPosX(), cshi->GetPosY(), cshi->GetWidth(), cshi->GetHeight());

        //this->InvalidateView();
        this->Invalidate();

        return TRUE;
    }

    BOOL CanZoomIn()
    {
        if (this->_selectedCushion == NULL)
            return FALSE;
        CCushionDirectory* parent = this->_selectedCushion->GetParent();
        if (parent != NULL && parent->GetParent() != NULL)
            return TRUE;
        return FALSE;
    }
    BOOL ZoomIn(/*CCushionHitInfo *cshi*/)
    {
        CCushionDirectory* oparent = NULL;
        //CCushionDirectory *parent = cshi->GetCushion()->GetParent();
        CCushionDirectory* parent = this->_selectedCushion->GetParent();
        while (parent != NULL && parent->GetParent() != NULL)
        {
            oparent = parent;
            parent = parent->GetParent();
        }
        if (oparent != NULL)
        {
            CZDirectory* dir = oparent->GetDirectory();
            if (dir != NULL && dir != this->_viewdir)
            {
                this->_viewdir = dir;
                return TRUE;
            }
        }
        return FALSE;
    }

    CZDirectory* GetParentDir(int level)
    {
        if (this->_viewdir == NULL)
            return NULL;
        CZDirectory* res = this->_viewdir;
        while (level > 0 && res != this->_rootdir && res != NULL)
        {
            level--;
            res = res->GetParent();
        }
        //if (res == NULL) return this->_rootdir; //oprava mozneho nedostatku?
        return res;
    }
    CZDirectory* GetRootDir() { return this->_rootdir; }
    CZDirectory* GetViewDir() { return this->_viewdir; }

    size_t GetSubDirName(TCHAR* buff, size_t bufflen)
    {
        buff[0] = TEXT('\0');
        if (this->_viewdir == NULL)
            return 0;
        if (this->_viewdir == this->_rootdir)
            return 0;

        return this->_viewdir->GetRelativeName(this->_rootdir, buff, bufflen);
        //return 0;
    }

    INT64 GetDirSize()
    {
        if (this->_viewdir == NULL)
            return -1;
        return this->_viewdir->GetSizeEx(FILESIZE_DISK);
    }

    BOOL CanZoomOut()
    {
        if (this->_rootdir == NULL)
            return FALSE;
        if (this->_viewdir == NULL)
            return FALSE;
        return (this->_viewdir != this->_rootdir);
    }
    BOOL ZoomOut()
    {
        if (this->_viewdir == NULL)
            return FALSE; //pokud neni null, pak by nemel byt null ani root
        if (this->_viewdir != this->_rootdir)
        {
            this->_viewdir = this->_viewdir->GetParent();
            if (this->_viewdir == NULL)
                this->_viewdir = this->_rootdir; //to be safe
            return TRUE;
        }
        return FALSE;
    }
    BOOL ZoomToRoot()
    {
        if (this->_viewdir == NULL)
            return FALSE;
        if (this->_viewdir != this->_rootdir)
        {
            this->_viewdir = this->_rootdir;
            return TRUE;
        }
        return FALSE;
    }
    BOOL CanAbort()
    {
        return (this->_populateworker != NULL);
    }
    BOOL Abort()
    {
        CWorkerThread* wrk = this->_populateworker;
        if (wrk == NULL)
            return FALSE;

        this->_rootdir = NULL;
        this->_populateworker = NULL;
        wrk->SetSelfDelete(TRUE);
        wrk->Abort(TRUE); //TODO: opravdu chci cekat?
        return TRUE;
    }

    CCushion* GetSelectedCushion()
    {
        return this->_selectedCushion;
    }

    /*BOOL GetSelectedCushionPos(POINT *pt)
	{
		pt->x = this->_selectedX;
		pt->y = this->_selectedY;

		return this->_selectedCushion != NULL;
	}*/
    BOOL GetSelectedCushionRect(RECT* rect)
    {
        /*rect->left = this->_selectedX;
		rect->top = this->_selectedY;
		rect->right = this->_selectedX + this->_selectedW;
		rect->bottom = this->_selectedY + this->_selectedH;*/
        *rect = this->_selectedRect;

        return this->_selectedCushion != NULL;
    }

    void RenderView(HDC hdc)
    {
        if (this->_mapReady == FALSE)
            return;

        if (this->_viewValid == TRUE)
            return;

#ifdef TIMINGTEST
        LARGE_INTEGER lt1, lt2, lf;
        QueryPerformanceCounter(&lt1);
#endif

        //this->_outBmp->CopyFromPixMap(this->_mapPix);

        //if (this->_outPix == NULL)
        //if (this->_mapBmp == NULL) this->_mapBmp = new CZBitmap(hdc, this->_mapPix);

        //if (this->_viewValid == FALSE)
        {
            //this->_outBmp = new CZBitmap(hdc, this->_mapWidth, this->_mapHeight);
            this->_outBmp->ResizeBitmap(hdc, this->_mapWidth, this->_mapHeight);
            this->_outBmp->CopyFromPixMap(this->_mapPix);

            this->_selectedOverlay->Paint(this->_outBmp, hdc);
            if (this->_directoryOverlayVisible)
            {
                this->_directoryOverlay->Paint(this->_outBmp, hdc);
            }

            this->_viewValid = TRUE;
        }
        //this->_outBmp->CreateDC(hdc);
        //this->_outBmp->CombineBitmap(0, 0, this->_mapBmp);
        /*if (this->_selectedCushion != NULL)
		{
		if (this->_cshBmp == NULL) this->_cshBmp = new CZBitmap(hdc, this->_cshPix);
		this->_outBmp->CombineBitmap(this->_selectedX, this->_selectedY, this->_cshBmp);
		}
		this->_outBmp->DeleteDC();*/
        //}

#ifdef TIMINGTEST
        QueryPerformanceCounter(&lt2);
        QueryPerformanceFrequency(&lf);
        this->_hbmptime = (double)(lt2.QuadPart - lt1.QuadPart) / lf.QuadPart;
#endif
    }

    BOOL Paint(HDC hdc, RECT rct)
    {
        if (this->_mapReady == TRUE && this->_mapPix != NULL)
        {
            int xw = rct.right - rct.left;
            int xh = rct.bottom - rct.top;

            this->RenderView(hdc);
            this->_outBmp->Paint(hdc, rct.left, rct.top, xw, xh);
#ifdef TIMINGTEST
            TCHAR buff[200];
            int c;
            //c = _stprintf(buff, TEXT("Cushions: %d -> %d...  Calctime: t=%1.8lf   Drawtime: t=%1.8lf"), this->_cushionCount, this->_drawcount, this->_calctime, this->_drawtime);
            //WM_PAINT - 1: t1=%1.8lg   t2=%1.8f   f=%I64d"), tf, tf, lf.QuadPart
            c = _stprintf(buff, TEXT("Cushions: %d -> %d...  "), this->_cushionCount, this->_drawcount);
            TextOut(hdc, 8, xh / 2 - 4, buff, c);
            c = _stprintf(buff, TEXT("Calctime: t=%1.8lf"), this->_calctime);
            TextOut(hdc, 8, xh / 2 - 4 + 20, buff, c);
            c = _stprintf(buff, TEXT("Drawtime: t=%1.8lf"), this->_drawtime);
            TextOut(hdc, 8, xh / 2 - 4 + 40, buff, c);
            c = _stprintf(buff, TEXT("HBMPtime: t=%1.8lf"), this->_hbmptime);
            TextOut(hdc, 8, xh / 2 - 4 + 60, buff, c);
#endif
            return TRUE;
        }
        return FALSE;
        /*else
		{

			//TODO: Display something!
			BitBlt(hdc, rct.left, rct.top, rct.right - rct.left, rct.bottom - rct.top, NULL, 0, 0, BLACKNESS);

			TCHAR buff[200];
			int c; 
			c = _stprintf(buff, TEXT("No DiskMap"));
			//TextOut(hdc, 0, xh/2-5, buff, c);
		}*/
    }
};
