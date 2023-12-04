// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

// ****************************************************************************
// SEKCE VIEWERU
// ****************************************************************************

#define IDX_TB_TERMINATOR -2
#define IDX_TB_SEPARATOR -1
#define IDX_TB_CUT 0
#define IDX_TB_COPY 1
#define IDX_TB_PASTE 2
#define IDX_TB_FILTER 3
#define IDX_TB_COUNT 4

#define FILTER_COUNT (CM_FILTER_LAST - CM_FILTER_FIRST + 1)

MENU_TEMPLATE_ITEM MenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        // Files
        {MNTT_PB, IDS_MENU_FILES, MNTS_B | MNTS_I | MNTS_A, CML_VIEWER_FILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILES_OPEN, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_OPEN, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILES_EXIT, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_EXIT, -1, 0, NULL},
        {MNTT_PE},

        // Edit
        {MNTT_PB, IDS_MENU_EDIT, MNTS_B | MNTS_I | MNTS_A, CML_VIEWER_EDIT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_CUT, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_CUT, IDX_TB_CUT, 0, (DWORD*)vweCut},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_COPY, IDX_TB_COPY, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_PASTE, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_PASTE, IDX_TB_PASTE, 0, (DWORD*)vwePaste},
        {MNTT_PE},

        // View
        {MNTT_PB, IDS_MENU_VIEW, MNTS_B | MNTS_I | MNTS_A, CML_VIEWER_VIEW, -1, 0, NULL},
        {MNTT_PB, IDS_MENU_FILTER, MNTS_B | MNTS_I | MNTS_A, CML_VIEWER_FILTER, IDX_TB_FILTER, 0, NULL},
        {MNTT_PE},
        {MNTT_PE},

        // Options
        {MNTT_PB, IDS_MENU_OPTIONS, MNTS_B | MNTS_I | MNTS_A, CML_VIEWER_OPTIONS, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_OPTIONS_CFG, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_CFG, -1, 0, NULL},
        {MNTT_PE},

        // Help
        {MNTT_PB, IDS_MENU_HELP, MNTS_B | MNTS_I | MNTS_A, CML_VIEWER_HELP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_ABOUT, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_ABOUT, -1, 0, NULL},
        {MNTT_PE},

        {MNTT_PE}, // terminator
};

MENU_TEMPLATE_ITEM PopupMenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_CUT, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_CUT, IDX_TB_CUT, 0, (DWORD*)vweCut},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_COPY, IDX_TB_COPY, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_PASTE, MNTS_B | MNTS_I | MNTS_A, CM_VIEWER_PASTE, IDX_TB_PASTE, 0, (DWORD*)vwePaste},
        {MNTT_PE}, // terminator
};

struct CButtonData
{
    int ImageIndex;                   // zero base index
    WORD ToolTipResID;                // resID se stringem pro tooltip
    WORD ID;                          // univerzalni Command
    CViewerWindowEnablerEnum Enabler; // ridici promenna pro enablovani tlacitka
};

CButtonData ToolBarButtons[] =
    {
        // ImageIndex     ToolTipResID     ID                Enabler
        {IDX_TB_CUT, IDS_TBTT_CUT, CM_VIEWER_CUT, vweCut},
        {IDX_TB_COPY, IDS_TBTT_COPY, CM_VIEWER_COPY, vweAlwaysEnabled},
        {IDX_TB_SEPARATOR},
        {IDX_TB_PASTE, IDS_TBTT_PASTE, CM_VIEWER_PASTE, vwePaste},
        {IDX_TB_SEPARATOR},
        {IDX_TB_FILTER, IDS_TBTT_FILTER, CM_VIEWER_FILTER, vweAlwaysEnabled},
        {IDX_TB_TERMINATOR}};

CWindowQueue ViewerWindowQueue("DemoPlug Viewers"); // seznam vsech oken viewru
CThreadQueue ThreadQueue("DemoPlug Viewers");       // seznam vsech threadu oken

HACCEL ViewerAccels = NULL; // akceleratory pro viewer

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL InitViewer()
{
    if (!InitializeWinLib(PluginNameEN, DLLInstance))
        return FALSE;
    SetWinLibStrings(LoadStr(IDS_INVALID_NUM), LoadStr(IDS_PLUGINNAME));
    SetupWinLibHelp(HTMLHelpCallback);
    ViewerAccels = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_ACCELERATORS));
    return TRUE;
}

void ReleaseViewer()
{
    ReleaseWinLib(DLLInstance);
    DestroyAcceleratorTable(ViewerAccels);
    ViewerAccels = NULL;
}

/*   // varianta spusteni threadu vieweru bez pouziti objektu CThread
struct CTVData
{
  BOOL AlwaysOnTop;
  const char *Name;
  int Left, Top, Width, Height;
  UINT ShowCmd;
  BOOL ReturnLock;
  HANDLE *Lock;
  BOOL *LockOwner;
  BOOL Success;
  HANDLE Continue;
};

unsigned WINAPI ViewerThreadBody(void *param)
{
  CALL_STACK_MESSAGE1("ViewerThreadBody()");
  SetThreadNameInVCAndTrace("DOP Viewer");
  TRACE_I("Begin");

  // ukazka padu aplikace
//  int *p = 0;
//  *p = 0;       // ACCESS VIOLATION !

  CTVData *data = (CTVData *)param;

  CViewerWindow *window = new CViewerWindow;
  if (window != NULL)
  {
    if (data->ReturnLock)
    {
      *data->Lock = window->GetLock();
      *data->LockOwner = TRUE;
    }
    CALL_STACK_MESSAGE1("ViewerThreadBody::CreateWindowEx");
    if (!data->ReturnLock || *data->Lock != NULL)
    {
      if (CfgSavePosition && CfgWindowPlacement.length != 0)
      {
        WINDOWPLACEMENT place = CfgWindowPlacement;
        // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
        // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
        RECT monitorRect;
        RECT workRect;
        SalamanderGeneral->MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
        OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
                   workRect.top - monitorRect.top);
        SalamanderGeneral->MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
        data->Left = place.rcNormalPosition.left;
        data->Top = place.rcNormalPosition.top;
        data->Width = place.rcNormalPosition.right - place.rcNormalPosition.left;
        data->Height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
        data->ShowCmd = place.showCmd;
      }

      // POZNAMKA: na existujicim okne/dialogu je da top-most zaridit jednoduse:
      //   SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

      if (window->CreateEx(data->AlwaysOnTop ? WS_EX_TOPMOST : 0,
                           CWINDOW_CLASSNAME2,
                           "DOP Viewer",
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           data->Left,
                           data->Top,
                           data->Width,
                           data->Height,
                           NULL,
                           NULL,
                           DLLInstance,
                           window) != NULL)
      {
        CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");
        ShowWindow(window->HWindow, data->ShowCmd);
        SetForegroundWindow(window->HWindow);
        UpdateWindow(window->HWindow);
        data->Success = TRUE;
      }
      else
      {
        if (data->ReturnLock && *data->Lock != NULL) HANDLES(CloseHandle(*data->Lock));
      }
    }
  }

  CALL_STACK_MESSAGE1("ViewerThreadBody::SetEvent");
  char name[MAX_PATH];
  strcpy(name, data->Name);
  BOOL openFile = data->Success;
  SetEvent(data->Continue);    // pustime dale hl. thread, od tohoto bodu nejsou data platna (=NULL)
  data = NULL;

  // pokud probehlo vse bez potizi, otevreme v okne pozadovany soubor
  if (openFile)
  {
    CALL_STACK_MESSAGE1("ViewerThreadBody::OpenFile");
    window->OpenFile(name, FALSE);

    CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
    // message loopa
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
      if (!window->IsMenuBarMessage(&msg) &&
          !TranslateAccelerator(window->HWindow, ViewerAccels, &msg))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }
  if (window != NULL)
    delete window;

  TRACE_I("End");
  return 0;
}

BOOL
CPluginInterfaceForViewer::ViewFile(const char *name, int left, int top, int width, int height,
                                    UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE *lock,
                                    BOOL *lockOwner, CSalamanderPluginViewerData *viewerData,
                                    int enumFilesSourceUID, int enumFilesCurrentIndex)
{
  // 'viewerData' se v DemoPlug nepouzivaji, jinak by bylo potreba predat hodnoty (ne odkazem)
  // pres CTVData do threadu vieweru...

  CTVData data;
  data.AlwaysOnTop = alwaysOnTop;
  data.Name = name;
  data.Left = left;
  data.Top = top;
  data.Width = width;
  data.Height = height;
  data.ShowCmd = showCmd;
  data.ReturnLock = returnLock;
  data.Lock = lock;
  data.LockOwner = lockOwner;
  data.Success = FALSE;
  data.Continue = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
  if (data.Continue == NULL)
  {
    TRACE_E("Unable to create Continue event.");
    return FALSE;
  }

  if (ThreadQueue.StartThread(ViewerThreadBody, &data))
  {
    // pockame, az thread zpracuje predana data a vrati vysledky
    WaitForSingleObject(data.Continue, INFINITE);
  }
  else data.Success = FALSE;
  HANDLES(CloseHandle(data.Continue));
  return data.Success;
}
*/

class CViewerThread : public CThread
{
protected:
    char Name[MAX_PATH];
    int Left, Top, Width, Height;
    UINT ShowCmd;
    BOOL AlwaysOnTop;
    BOOL ReturnLock;

    HANDLE Continue; // po naplneni nasledujicich navratovych hodnot se tento event prepne do "signaled"
    HANDLE* Lock;
    BOOL* LockOwner;
    BOOL* Success;

    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index prvniho souboru ve vieweru ve zdroji

public:
    CViewerThread(const char* name, int left, int top, int width, int height,
                  UINT showCmd, BOOL alwaysOnTop, BOOL returnLock,
                  HANDLE* lock, BOOL* lockOwner, HANDLE contEvent,
                  BOOL* success, int enumFilesSourceUID,
                  int enumFilesCurrentIndex) : CThread("DOP Viewer")
    {
        lstrcpyn(Name, name, MAX_PATH);
        Left = left;
        Top = top;
        Width = width;
        Height = height;
        ShowCmd = showCmd;
        AlwaysOnTop = alwaysOnTop;
        ReturnLock = returnLock;

        Continue = contEvent;
        Lock = lock;
        LockOwner = lockOwner;
        Success = success;

        EnumFilesSourceUID = enumFilesSourceUID;
        EnumFilesCurrentIndex = enumFilesCurrentIndex;
    }

    virtual unsigned Body();
};

unsigned
CViewerThread::Body()
{
    CALL_STACK_MESSAGE1("CViewerThread::Body()");
    TRACE_I("Begin");

    // ukazka padu aplikace
    //  int *p = 0;
    //  *p = 0;       // ACCESS VIOLATION !

    CViewerWindow* window = new CViewerWindow(EnumFilesSourceUID, EnumFilesCurrentIndex);
    if (window != NULL)
    {
        if (ReturnLock)
        {
            *Lock = window->GetLock();
            *LockOwner = TRUE;
        }
        CALL_STACK_MESSAGE1("ViewerThreadBody::CreateWindowEx");
        if (!ReturnLock || *Lock != NULL)
        {
            if (CfgSavePosition && CfgWindowPlacement.length != 0)
            {
                WINDOWPLACEMENT place = CfgWindowPlacement;
                // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
                // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
                RECT monitorRect;
                RECT workRect;
                SalamanderGeneral->MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
                OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
                           workRect.top - monitorRect.top);
                SalamanderGeneral->MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
                Left = place.rcNormalPosition.left;
                Top = place.rcNormalPosition.top;
                Width = place.rcNormalPosition.right - place.rcNormalPosition.left;
                Height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
                ShowCmd = place.showCmd;
            }

            // POZNAMKA: na existujicim okne/dialogu je da top-most zaridit jednoduse:
            //   SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            if (window->CreateEx(AlwaysOnTop ? WS_EX_TOPMOST : 0,
                                 CWINDOW_CLASSNAME2,
                                 "DOP Viewer",
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                 Left,
                                 Top,
                                 Width,
                                 Height,
                                 NULL,
                                 NULL,
                                 DLLInstance,
                                 window) != NULL)
            {
                CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");
                ShowWindow(window->HWindow, ShowCmd);
                SetForegroundWindow(window->HWindow);
                UpdateWindow(window->HWindow);
                *Success = TRUE;
            }
            else
            {
                if (ReturnLock && *Lock != NULL)
                    HANDLES(CloseHandle(*Lock));
            }
        }
    }

    CALL_STACK_MESSAGE1("ViewerThreadBody::SetEvent");
    BOOL openFile = *Success;
    SetEvent(Continue); // pustime dale hl. thread, od tohoto bodu nejsou platne nasl. promenne:
    Continue = NULL;    // vymaz je zbytecny, jen pro prehlednost
    Lock = NULL;        // vymaz je zbytecny, jen pro prehlednost
    LockOwner = NULL;   // vymaz je zbytecny, jen pro prehlednost
    Success = NULL;     // vymaz je zbytecny, jen pro prehlednost

    // pokud probehlo vse bez potizi, otevreme v okne pozadovany soubor
    if (openFile)
    {
        CALL_STACK_MESSAGE1("ViewerThreadBody::OpenFile");
        window->OpenFile(Name, FALSE);

        CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
        // message loopa
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!window->IsMenuBarMessage(&msg) &&
                !TranslateAccelerator(window->HWindow, ViewerAccels, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    if (window != NULL)
        delete window;

    TRACE_I("End");
    return 0;
}

BOOL WINAPI
CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width, int height,
                                    UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                    BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                    int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    HANDLE contEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (contEvent == NULL)
    {
        TRACE_E("Unable to create Continue event.");
        return FALSE;
    }

    // 'viewerData' se v DemoPlug nepouzivaji, jinak by bylo potreba predat hodnoty (ne odkazem)
    // do threadu vieweru...
    BOOL success = FALSE;
    CViewerThread* t = new CViewerThread(name, left, top, width, height,
                                         showCmd, alwaysOnTop, returnLock, lock,
                                         lockOwner, contEvent, &success,
                                         enumFilesSourceUID, enumFilesCurrentIndex);
    if (t != NULL)
    {
        if (t->Create(ThreadQueue) != NULL) // thread se spustil
        {
            t = NULL;                                 // zbytecne nulovani, jen pro poradek (ukazatel uz muze byt dealokovany)
            WaitForSingleObject(contEvent, INFINITE); // pockame, az thread zpracuje predana data a vrati vysledky
        }
        else
            delete t; // pri chybe je potreba dealokovat objekt threadu
    }
    HANDLES(CloseHandle(contEvent));
    return success;
}

//
// ****************************************************************************
// CRendererWindow
//

CRendererWindow::CRendererWindow()
    : CWindow(ooStatic)
{
}

CRendererWindow::~CRendererWindow()
{
}

void CRendererWindow::OnContextMenu(const POINT* p)
{
    CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
    if (popup != NULL)
    {
        popup->LoadFromTemplate(HLanguage, PopupMenuTemplate, Viewer->Enablers, Viewer->HGrayToolBarImageList,
                                Viewer->HHotToolBarImageList);
        popup->SetStyle(MENU_POPUP_UPDATESTATES);
        popup->Track(MENU_TRACK_RIGHTBUTTON, p->x, p->y, HWindow, NULL);
        /*
    // using windows popup menu (NOTE: disabled menu items are not disabled/grayed + icons are not shown)
    HMENU hMenu = CreatePopupMenu();
    popup->FillMenuHandle(hMenu);
    DWORD flags = TPM_LEFTALIGN;
    TrackPopupMenu(hMenu, flags, p->x, p->y, 0, HWindow, NULL);
    DestroyMenu(hMenu);
*/
        SalamanderGUI->DestroyMenuPopup(popup);
    }
}

LRESULT
CRendererWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        if (hDC != NULL)
        {
            SetBkColor(hDC, RGB(70, 110, 200));
            SetTextColor(hDC, RGB(0, 0, 0));
            int i;
            for (i = 0; i < 12; i++)
                TextOut(hDC, 5, 5 + i * 18, Viewer->Name, (int)strlen(Viewer->Name));

            CSalamanderPNGAbstract* salamanderPNG = SalamanderGeneral->GetSalamanderPNG();
            // pro prehlednost neresime chybove stavy
            HBITMAP hBmp = salamanderPNG->LoadPNGBitmap(DLLInstance, MAKEINTRESOURCE(IDB_GRAYALPHA_PNG), SALPNG_GETALPHA | SALPNG_PREMULTIPLE, 0);

            // ziskame z handle podrobnosti o DIB
            DIBSECTION dibSec;
            GetObject(hBmp, sizeof(dibSec), &dibSec);

            // priklad: primym pristupem do dat natonujeme do zelena
            DWORD* ptr = (DWORD*)dibSec.dsBm.bmBits;
            int iterator;
            for (iterator = 0; iterator < dibSec.dsBm.bmWidth * dibSec.dsBm.bmHeight; iterator++)
            {
                DWORD argb = ptr[iterator];
                BYTE clrR = (BYTE)((argb & 0x00FF0000) >> 16);
                BYTE clrG = (BYTE)((argb & 0x0000FF00) >> 8);
                BYTE clrB = (BYTE)((argb & 0x000000FF));
                BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);
                clrR = clrR / 2;
                clrB = clrB / 2;
                ptr[iterator] = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
            }

            HDC hMemDC = HANDLES(CreateCompatibleDC(NULL));
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBmp);

            BLENDFUNCTION bf;
            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.AlphaFormat = AC_SRC_ALPHA; // use source alpha
            bf.SourceConstantAlpha = 0xff; // opaque (disable constant alpha)
            AlphaBlend(hDC, 30, 30, dibSec.dsBm.bmWidth, dibSec.dsBm.bmHeight, hMemDC, 0, 0, dibSec.dsBm.bmWidth, dibSec.dsBm.bmHeight, bf);

            SelectObject(hMemDC, hOldBitmap);
            HANDLES(DeleteDC(hMemDC));
            DeleteObject(hBmp);
        }
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_COMMAND:
    {
        SendMessage(Viewer->HWindow, WM_COMMAND, wParam, lParam); // forward commands from context menu
        break;
    }

    case WM_RBUTTONDOWN:
    {
        POINT p;
        GetCursorPos(&p);
        OnContextMenu(&p);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CViewerWindow
//

CViewerWindow::CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex) : CWindow(ooStatic)
{
    Lock = NULL;
    Name[0] = 0;
    Renderer.Viewer = this;
    HRebar = NULL;
    MainMenu = NULL;
    MenuBar = NULL;
    ToolBar = NULL;
    HGrayToolBarImageList = NULL;
    HHotToolBarImageList = NULL;

    EnumFilesSourceUID = enumFilesSourceUID;
    EnumFilesCurrentIndex = enumFilesCurrentIndex;

    ZeroMemory(Enablers, sizeof(Enablers));
}

BOOL CViewerWindow::IsMenuBarMessage(CONST MSG* lpMsg)
{
    if (MenuBar == NULL)
        return FALSE;
    return MenuBar->IsMenuBarMessage(lpMsg);
}

BOOL CViewerWindow::InitializeGraphics()
{
    HBITMAP hTmpMaskBitmap;
    HBITMAP hTmpGrayBitmap;
    HBITMAP hTmpColorBitmap;

    CSalamanderPNGAbstract* salamanderPNG = SalamanderGeneral->GetSalamanderPNG();
    hTmpColorBitmap = salamanderPNG->LoadPNGBitmap(DLLInstance, MAKEINTRESOURCE(SalamanderGeneral->CanUse256ColorsBitmap() ? IDB_TOOLBAR256_PNG : IDB_TOOLBAR16_PNG), 0, 0);
    if (hTmpColorBitmap != NULL) // ziskany handle bitmapy vlozime do HANDLES
        HANDLES_ADD(__htBitmap, __hoCreateDIBitmap, hTmpColorBitmap);

    BOOL ok = SalamanderGUI->CreateGrayscaleAndMaskBitmaps(hTmpColorBitmap, RGB(255, 0, 255),
                                                           hTmpGrayBitmap, hTmpMaskBitmap);
    if (ok) // ziskane handly bitmap vlozime do HANDLES (ukazka rucniho vkladani; jednodussi
    {       // v teto situaci (DeleteObject okamzite nasleduje) je pouzit u DeleteObject makro NOHANDLES)
        HANDLES_ADD(__htBitmap, __hoCreateDIBitmap, hTmpGrayBitmap);
        HANDLES_ADD(__htBitmap, __hoCreateDIBitmap, hTmpMaskBitmap);
    }
    HHotToolBarImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
    HGrayToolBarImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
    ImageList_Add(HHotToolBarImageList, hTmpColorBitmap, hTmpMaskBitmap);
    ImageList_Add(HGrayToolBarImageList, hTmpGrayBitmap, hTmpMaskBitmap);
    HANDLES(DeleteObject(hTmpMaskBitmap));
    HANDLES(DeleteObject(hTmpGrayBitmap));
    HANDLES(DeleteObject(hTmpColorBitmap));
    return TRUE;
}

BOOL CViewerWindow::ReleaseGraphics()
{
    if (HHotToolBarImageList != NULL)
        ImageList_Destroy(HHotToolBarImageList);
    if (HGrayToolBarImageList != NULL)
        ImageList_Destroy(HGrayToolBarImageList);
    return TRUE;
}

void CViewerWindow::UpdateEnablers()
{
    //  Enablers[vweCut] = Renderer.Database.IsOpened();
    //  Enablers[vwePaste] = lstrcmpi(Renderer.Database.GetParserName(), "csv") == 0;
    if (ToolBar != NULL)
        ToolBar->UpdateItemsState();
}

BOOL CViewerWindow::FillToolBar()
{
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID | TLBI_MASK_ENABLER | TLBI_MASK_STYLE;

    ToolBar->SetImageList(HGrayToolBarImageList);
    ToolBar->SetHotImageList(HHotToolBarImageList);

    int i;
    for (i = 0; ToolBarButtons[i].ImageIndex != IDX_TB_TERMINATOR; i++)
    {
        if (ToolBarButtons[i].ImageIndex == IDX_TB_SEPARATOR)
        {
            tii.Style = TLBI_STYLE_SEPARATOR;
        }
        else
        {
            if (ToolBarButtons[i].ImageIndex == IDX_TB_FILTER)
                tii.Style = TLBI_STYLE_DROPDOWN | TLBI_STYLE_WHOLEDROPDOWN;
            else
                tii.Style = 0;
            tii.ImageIndex = ToolBarButtons[i].ImageIndex;
            tii.ID = ToolBarButtons[i].ID;
            if (ToolBarButtons[i].Enabler == vweAlwaysEnabled)
                tii.Enabler = NULL;
            else
                tii.Enabler = Enablers + ToolBarButtons[i].Enabler;
        }
        ToolBar->InsertItem2(0xFFFFFFFF, TRUE, &tii);
    }

    // obehne enablery
    ToolBar->UpdateItemsState();
    return TRUE;
}

BOOL CViewerWindow::InsertMenuBand()
{
    REBARBANDINFO rbbi;
    ZeroMemory(&rbbi, sizeof(rbbi));

    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID;
    rbbi.cxMinChild = MenuBar->GetNeededWidth();
    rbbi.cyMinChild = MenuBar->GetNeededHeight();
    rbbi.fStyle = RBBS_NOGRIPPER;
    rbbi.hwndChild = MenuBar->GetHWND();
    rbbi.wID = BANDID_MENU;
    SendMessage(HRebar, RB_INSERTBAND, (WPARAM)0, (LPARAM)&rbbi);
    return TRUE;
}

BOOL CViewerWindow::InsertToolBarBand()
{
    REBARBANDINFO rbbi;
    ZeroMemory(&rbbi, sizeof(rbbi));

    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_SIZE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID;
    rbbi.cxMinChild = ToolBar->GetNeededWidth();
    rbbi.cyMinChild = ToolBar->GetNeededHeight();
    rbbi.cx = 10000;
    rbbi.fStyle = RBBS_NOGRIPPER;
    rbbi.hwndChild = ToolBar->GetHWND();
    rbbi.wID = BANDID_TOOLBAR;
    SendMessage(HRebar, RB_INSERTBAND, (WPARAM)1, (LPARAM)&rbbi);
    return TRUE;
}

void CViewerWindow::LayoutWindows()
{
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
}

void FillMenuFilter(CGUIMenuPopupAbstract* popup, int cmdFirst, int filterCount)
{
    popup->RemoveAllItems();

    MENU_ITEM_INFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_STATE;
    mi.Type = MENU_TYPE_STRING;
    mi.State = 0;

    char buff[MAX_PATH + 3];
    mi.String = buff;
    int index = 0;
    while (index < filterCount)
    {
        wsprintf(buff, "&%d %s %d", index < 9 ? index + 1 : 0, LoadStr(IDS_FILTER), index + 1);
        mi.ID = cmdFirst + index;
        popup->InsertItem(-1, TRUE, &mi);
        index++;
    }
}

LRESULT
CViewerWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        InitializeGraphics();
        DragAcceptFiles(HWindow, TRUE); // drag&drop open file
        MainMenu = SalamanderGUI->CreateMenuPopup();
        if (MainMenu == NULL)
            return -1;
        ToolBar = SalamanderGUI->CreateToolBar(HWindow);
        if (ToolBar == NULL)
            return -1;
        MainMenu->LoadFromTemplate(HLanguage, MenuTemplate, Enablers, HGrayToolBarImageList, HHotToolBarImageList);
        MenuBar = SalamanderGUI->CreateMenuBar(MainMenu, HWindow);
        if (MenuBar == NULL)
            return -1;

        RECT r;
        GetClientRect(HWindow, &r);
        HRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, "",
                                WS_VISIBLE | /*WS_BORDER |  */ WS_CHILD |
                                    WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                                    RBS_VARHEIGHT | CCS_NODIVIDER |
                                    RBS_BANDBORDERS | CCS_NOPARENTALIGN |
                                    RBS_AUTOSIZE,
                                0, 0, r.right, r.bottom, // dummy
                                HWindow, (HMENU)0, DLLInstance, NULL);

        // nechceme vizualni styly pro rebar
        SalamanderGUI->DisableWindowVisualStyles(HRebar);

        Renderer.CreateEx(WS_EX_STATICEDGE /*WS_EX_CLIENTEDGE*/,
                          CWINDOW_CLASSNAME2,
                          "",
                          WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS,
                          0,
                          0,
                          0,
                          0,
                          HWindow,
                          NULL,
                          DLLInstance,
                          &Renderer);

        MenuBar->CreateWnd(HRebar);
        InsertMenuBand();
        ToolBar->CreateWnd(HRebar);
        FillToolBar();
        InsertToolBarBand();
        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));
        break;
    }

    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_DESTROY:
    {
        DragAcceptFiles(HWindow, FALSE); // drag&drop open file
        if (CfgSavePosition)
        {
            CfgWindowPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &CfgWindowPlacement);
        }
        if (MenuBar != NULL)
        {
            SalamanderGUI->DestroyMenuBar(MenuBar);
            MenuBar = NULL;
        }
        if (MainMenu != NULL)
        {
            SalamanderGUI->DestroyMenuPopup(MainMenu);
            MainMenu = NULL;
        }
        if (ToolBar != NULL)
        {
            SalamanderGUI->DestroyToolBar(ToolBar);
            ToolBar = NULL;
        }

        if (Renderer.HWindow != NULL)
            DestroyWindow(Renderer.HWindow);

        ViewerWindowQueue.Remove(HWindow);

        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL;
        }
        ReleaseGraphics();
        PostQuitMessage(0);
        break;
    }

    case WM_DROPFILES: // drag&drop open file
    {
        UINT drag;
        char path[MAX_PATH];

        drag = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0); // kolik souboru nam hodili
        if (drag > 0)
        {
            DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);
            OpenFile(path);
        }
        DragFinish((HDROP)wParam);
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR lphdr = (LPNMHDR)lParam;
        if (lphdr->code == RBN_AUTOSIZE)
        {
            LayoutWindows();
            return 0;
        }
        break;
    }

    case WM_SIZE:
    {
        if (Renderer.HWindow != NULL && HRebar != NULL && MenuBar != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);

            RECT rebarRect;
            GetWindowRect(HRebar, &rebarRect);
            int rebarHeight = rebarRect.bottom - rebarRect.top;

            HDWP hdwp = HANDLES(BeginDeferWindowPos(2));
            if (hdwp != NULL)
            {
                // + 4: pri zvetsovani sirky okna mi nechodilo prekreslovani poslednich 4 bodu
                // v rebaru; ani po nekolika hodinach jsem nenasel pricinu; v Salamu to slape;
                // zatim to resim takto; treba si casem vzpomenu, kde je problem
                hdwp = HANDLES(DeferWindowPos(hdwp, HRebar, NULL,
                                              0, 0, r.right + 4, rebarHeight,
                                              SWP_NOACTIVATE | SWP_NOZORDER));

                hdwp = HANDLES(DeferWindowPos(hdwp, Renderer.HWindow, NULL,
                                              0, rebarHeight, r.right, r.bottom - rebarHeight,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
                HANDLES(EndDeferWindowPos(hdwp));
            }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        // tady by se mely premapovat barvy
        TRACE_I("CViewerWindow::WindowProc - WM_SYSCOLORCHANGE");
        break;
    }

    case WM_USER_VIEWERCFGCHNG:
    {
        // tady by se mely projevit zmeny v konfiguraci pluginu
        TRACE_I("CViewerWindow::WindowProc - config has changed");
        InvalidateRect(HWindow, NULL, TRUE);
        return 0;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (MenuBar != NULL)
            MenuBar->SetFont();
        if (ToolBar != NULL)
            ToolBar->SetFont();
        return 0;
    }

    case WM_ACTIVATE:
    {
        if (!LOWORD(wParam))
        {
            // hlavni okno pri prepnuti z viewru nebude delat refresh
            SalamanderGeneral->SkipOneActivateRefresh();
        }
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
            PostMessage(HWindow, WM_CLOSE, 0, 0);

        BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if ((wParam == VK_F10 && shiftPressed || wParam == VK_APPS))
        {
            POINT p;
            p.x = 0;
            p.y = 0;
            ClientToScreen(Renderer.HWindow, &p);
            Renderer.OnContextMenu(&p);
            return 0;
        }

        if (wParam == VK_SPACE)
        {
            if (!ctrlPressed && !altPressed)
            {
                BOOL ok = FALSE;
                BOOL srcBusy = FALSE;
                BOOL noMoreFiles = FALSE;
                char fileName[MAX_PATH];
                fileName[0] = 0;
                if (shiftPressed) // zastarala hot-key: pouzivat Backspace (klavesy + prikazy v menu viz PictView, menu File/Other Files)
                {
                    ok = SalamanderGeneral->GetPreviousFileNameForViewer(EnumFilesSourceUID,
                                                                         &EnumFilesCurrentIndex,
                                                                         Name, FALSE, TRUE,
                                                                         fileName, &noMoreFiles,
                                                                         &srcBusy);
                }
                else
                {
                    ok = SalamanderGeneral->GetNextFileNameForViewer(EnumFilesSourceUID,
                                                                     &EnumFilesCurrentIndex,
                                                                     Name, FALSE, TRUE, fileName,
                                                                     &noMoreFiles, &srcBusy);
                }

                if (ok)
                    OpenFile(fileName); // mame nove jmeno
                else
                {
                    if (noMoreFiles)
                        TRACE_I("Next/previous file does not exist.");
                    else
                    {
                        if (srcBusy)
                            TRACE_I("Connected panel or Find window is busy, please try to repeat your request later.");
                        else
                        {
                            if (EnumFilesSourceUID == -1)
                                TRACE_I("This service is not available from archive nor file system path.");
                            else
                                TRACE_I("Connected panel or Find window does not contain original list of files.");
                        }
                    }
                }
                return 0;
            }
        }
        break;
    }

        /*
    case WM_USER_ENTERMENULOOP: TRACE_I("enter loop"); break;
    case WM_USER_LEAVEMENULOOP: TRACE_I("leave loop"); break;
    case WM_USER_LEAVEMENULOOP2: TRACE_I("leave loop2"); break;
    case WM_USER_UNINITMENUPOPUP : TRACE_I("uninit popup"); break;
    case WM_USER_CONTEXTMENU: TRACE_I("context menu"); break;
    */

    case WM_USER_INITMENUPOPUP:
    {
        CGUIMenuPopupAbstract* popup = (CGUIMenuPopupAbstract*)wParam;
        WORD popupID = HIWORD(lParam);
        switch (popupID)
        {
        case CML_VIEWER_FILTER:
        {
            FillMenuFilter(popup, CM_FILTER_FIRST, FILTER_COUNT);
            break;
        }
        }
        return 0;
    }

    case WM_USER_TBDROPDOWN:
    {
        if (ToolBar != NULL && ToolBar->GetHWND() == (HWND)wParam)
        {
            int index = (int)lParam;
            TLBI_ITEM_INFO2 tii;
            tii.Mask = TLBI_MASK_ID;
            if (!ToolBar->GetItemInfo2(index, TRUE, &tii))
                return 0;

            RECT r;
            ToolBar->GetItemRect(index, r);

            switch (tii.ID)
            {
            case CM_VIEWER_FILTER:
            {
                CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
                if (popup != NULL)
                {
                    FillMenuFilter(popup, CM_FILTER_FIRST, FILTER_COUNT);

                    // using windows popup menu
                    HMENU hMenu = CreatePopupMenu();
                    popup->FillMenuHandle(hMenu);
                    TPMPARAMS tpm;
                    tpm.cbSize = sizeof(tpm);
                    tpm.rcExclude = r;
                    DWORD flags = TPM_LEFTALIGN;
                    if (LOWORD(wParam) == IDC_CE_PB)
                        flags |= TPM_HORIZONTAL;
                    else
                        flags |= TPM_VERTICAL;
                    TrackPopupMenuEx(hMenu, flags, r.left, r.bottom, HWindow, &tpm);
                    DestroyMenu(hMenu);

                    // using Salamander popup menu
                    //               popup->Track(0, r.left, r.bottom, HWindow, &r);

                    SalamanderGUI->DestroyMenuPopup(popup);
                }
                break;
            }
            }
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) >= CM_FILTER_FIRST && LOWORD(wParam) <= CM_FILTER_LAST)
        {
            char buff[100];
            wsprintf(buff, "%s %d", LoadStr(IDS_FILTER), LOWORD(wParam) - CM_FILTER_FIRST + 1);
            SalamanderGeneral->SalMessageBox(HWindow, buff, LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
            return 0;
        }

        switch (LOWORD(wParam))
        {
        case CM_VIEWER_OPEN:
        {
            char file[MAX_PATH];
            file[0] = 0;
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            char filterBuf[100];
            lstrcpyn(filterBuf, LoadStr(IDS_DOP_FILES_FILTER), 100);
            char* s = filterBuf;
            ofn.lpstrFilter = s;
            while (*s != 0) // vytvoreni double-null terminated listu
            {
                if (*s == '|')
                    *s = 0;
                s++;
            }
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            char curDir[MAX_PATH];
            lstrcpyn(curDir, Name, MAX_PATH);
            SalamanderGeneral->CutDirectory(curDir);
            ofn.lpstrInitialDir = curDir[0] != 0 ? curDir : NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

            if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
                OpenFile(file);
            break;
        }

        case CM_VIEWER_CUT:
        {
            SalamanderGeneral->SalMessageBox(HWindow, "TODO: Cut", LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
            Enablers[vwePaste] = TRUE;
            UpdateEnablers();
            break;
        }

        case CM_VIEWER_COPY:
        {
            SalamanderGeneral->SalMessageBox(HWindow, "TODO: Copy", LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
            Enablers[vwePaste] = TRUE;
            UpdateEnablers();
            break;
        }

        case CM_VIEWER_PASTE:
        {
            SalamanderGeneral->SalMessageBox(HWindow, "TODO: Paste", LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
            Enablers[vwePaste] = FALSE; // it's OK only if it was Cut (Copy should not change it)
            UpdateEnablers();
            break;
        }

        case CM_VIEWER_EXIT:
        {
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            break;
        }

        case CM_VIEWER_CFG:
        {
            OnConfiguration(HWindow);
            break;
        }

        case CM_VIEWER_ABOUT:
        {
            OnAbout(HWindow);
            break;
        }
        }
        return 0;
    }

    case WM_USER_TBGETTOOLTIP:
    {
        HWND hToolBar = (HWND)wParam;
        if (ToolBar != NULL && hToolBar == ToolBar->GetHWND())
        {
            TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
            lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tt->Index].ToolTipResID));
            SalamanderGUI->PrepareToolTipText(tt->Buffer, FALSE);
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

HANDLE
CViewerWindow::GetLock()
{
    if (Lock == NULL)
        Lock = NOHANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    return Lock;
}

void CViewerWindow::OpenFile(const char* name, BOOL setLock)
{
    if (setLock && Lock != NULL)
    {
        SetEvent(Lock);
        Lock = NULL; // ted uz je to jen na disk-cache
    }
    lstrcpyn(Name, name, MAX_PATH);
    InvalidateRect(HWindow, NULL, TRUE);
    InvalidateRect(Renderer.HWindow, NULL, TRUE);
}
