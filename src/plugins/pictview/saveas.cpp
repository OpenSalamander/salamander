// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <zmouse.h>
#include <shlobj.h>

#include "lib\\pvw32dll.h"
#include "renderer.h"
#include "dialogs.h"
#include "pictview.h"
#include "pictview.rh"
#include "pictview.rh2"
#include "lang\lang.rh"

#define SAVEAS_GRAY_FLAG 0x40000000
#define SAVEAS_GRAY_MASK 0x3FFFFFFF

#define SAVEAS_COMMENT_FLAG 0x4000
#define SAVEAS_COMMENT_MASK 0x3FFF

typedef struct _gen_ftype
{
    LPCTSTR ext;
    int type;
} PVFS_FTYPE;

static PVFS_FTYPE fs_types[] = {
    {_T("jpg"), PVF_JPG | SAVEAS_COMMENT_FLAG},
    {_T("bmp"), PVF_BMP},
    {_T("tif"), PVF_TIFF | SAVEAS_COMMENT_FLAG},
    {_T("gif"), PVF_GIF | SAVEAS_COMMENT_FLAG},
    {_T("tga"), PVF_TGA},
    {_T("wbmp"), PVF_WBMP},
    {_T("cel"), PVF_CEL},
    {_T("cit"), PVF_IRF | SAVEAS_COMMENT_FLAG},
    {_T("dat"), PVF_IRF | SAVEAS_COMMENT_FLAG},
    {_T("pbm"), PVF_PNM},
    {_T("pnm"), PVF_PNM},
    {_T("png"), PVF_PNG | SAVEAS_COMMENT_FLAG},
    {_T("iff"), PVF_LBM},
    {_T("rgb"), PVF_SGI},
    {_T("bw"), PVF_SGI},
    {_T("ras"), PVF_RAS},
    {_T("ska"), PVF_SKA},
    {_T("rle"), PVF_RLE},
    {_T("pcx"), PVF_PCX},
    {NULL, 0}};

static PVFS_FTYPE fs_comptypes[] = {
    {_T("ASCII"), PVCS_ASCII},
    {_T("CCITT G3"), PVCS_CCITT_3},
    {_T("CCITT G4"), PVCS_CCITT_4},
    {_T("Deflating (LZ77)"), PVCS_DEFLATE},
    {_T("Huffman"), PVCS_HUFFMAN},
    {_T("JPEG"), PVCS_JPEG_HUFFMAN},
    {_T("Lempel-Ziv-Welch (LZW)"), PVCS_LZW},
    {_T("PackBits"), PVCS_PACKBITS},
    {_T("Run-Length Encoding"), PVCS_RLE},
    {_T("Uncompressed"), PVCS_NO_COMPRESSION},
    {NULL, 0}};

void GetMyDocumentsPath(LPTSTR initDir)
{
    initDir[0] = 0;
    ITEMIDLIST* pidl = NULL;
    if (SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl) == NOERROR)
    {
        if (!SHGetPathFromIDList(pidl, initDir))
            initDir[0] = 0;
        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            alloc->Free(pidl);
            alloc->Release();
        }
    }
}

typedef struct tagProgBarInfo
{
    CViewerWindow* pViewer;
    DWORD lastUpdateTicks;
    DWORD lastCheckTicks;
} sProgBarInfo, *psProgBarInfo;

BOOL WINAPI SaveProgressProcedure(int done, void* data)
{
    DWORD ticks = GetTickCount();

    if ((ticks - ((psProgBarInfo)data)->lastUpdateTicks > 100) || (done > 95))
    {
        // for performance reasons, do it just 3 times a second
        ((psProgBarInfo)data)->pViewer->SetProgress(done);
        ((psProgBarInfo)data)->lastUpdateTicks = ticks;
    }
    if (ticks - ((psProgBarInfo)data)->lastCheckTicks > 500)
    {
        // for performance reasons, do it just twice a second
        MSG msg;
        HWND hWnd = ((psProgBarInfo)data)->pViewer->HWindow;

        while (PeekMessage(&msg, hWnd, WM_KEYUP, WM_KEYUP, PM_NOREMOVE))
        {
            int nVirtKey;    // virtual-key code
            LPARAM lKeyData; // key data

            GetMessage(&msg, hWnd, WM_KEYUP, WM_KEYUP);
            nVirtKey = (int)msg.wParam;
            lKeyData = msg.lParam;
            if (nVirtKey == 27)
            { // virtual-key code for ESC
                while (PeekMessage(&msg, hWnd, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE))
                {
                }
                return TRUE;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ((psProgBarInfo)data)->lastCheckTicks = ticks;
    }

    return FALSE;
}

void EnableDisableControls(HWND hDlg, int firstID, int lastID, BOOL bEnable)
{
    for (; firstID <= lastID; firstID++)
    {
        EnableWindow(GetDlgItem(hDlg, firstID), bEnable);
    }
} /* EnableDisableControls */

void PositionControl(HWND hDlg, int baseID, int moveID)
{
    RECT r1, r2;
    HWND hParent;

    hParent = GetParent(hDlg);
    // Our templated dialog is a subwindow of the main common dlg window
    // GetWindowRect returns screen coordinates
    GetWindowRect(GetDlgItem(hParent, baseID), &r1);
    GetWindowRect(GetDlgItem(hDlg, moveID), &r2);
    ScreenToClient(hParent, (LPPOINT)&r1);
    ScreenToClient(hParent, &((LPPOINT)&r1)[1]);
    ScreenToClient(hDlg, (LPPOINT)&r2);
    ScreenToClient(hDlg, &((LPPOINT)&r2)[1]);
    SetWindowPos(GetDlgItem(hDlg, moveID), 0, r1.left, r2.top,
                 r1.right - r1.left, r2.bottom - r2.top, SWP_NOZORDER | SWP_SHOWWINDOW);
} /* PositionControl */

void FillCombo(HWND hWnd, int controlID, TwoDWords* pData, DWORD value)
{
    hWnd = GetDlgItem(hWnd, controlID);

    while (pData[0][0])
    {
        LRESULT index = SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)LoadStr(pData[0][0]));
        SendMessage(hWnd, CB_SETITEMDATA, index, pData[0][1]);
        if (!index || (pData[0][1] == value))
        {
            // select first or with required data
            SendMessage(hWnd, CB_SETCURSEL, index, 0);
        }
        pData++;
    }
} /* FillCombo */

DWORD GetItemData(HWND hWnd, int controlID)
{
    hWnd = GetDlgItem(hWnd, controlID);
    return (DWORD)SendMessage(hWnd, CB_GETITEMDATA, SendMessage(hWnd, CB_GETCURSEL, 0, 0), 0);
} /* GetItemData */

BOOL GetFormatInfo(OPENFILENAME* lpOFN, DWORD* pFormat, LPTSTR* pExt)
{
    // given list of filters & filter index, extract PVF_xxx & default format extension
    // returns TRUE if the format supports comment (or better to say, we support saving it ;))
    int i = 2 * lpOFN->nFilterIndex - 1;
    LPTSTR s = (LPTSTR)lpOFN->lpstrFilter;
    PVFS_FTYPE* ftype = fs_types;

    while (i--)
    {
        s += _tcslen(s) + 1;
    }
    *pFormat = 0;
    while ((s = (LPTSTR)_tcschr(s, '.')) != NULL)
    {
        if (pExt)
            *pExt = s;
        s++;
        while (ftype->ext)
        {
            if (!_tcsnicmp(ftype->ext, s, _tcslen(ftype->ext)))
            {
                *pFormat = ftype->type & SAVEAS_COMMENT_MASK;
                return (ftype->type & SAVEAS_COMMENT_FLAG) ? TRUE : FALSE;
            }
            ftype++;
        }
    }
    return FALSE;
} /* GetFormatInfo */

void FillTypeFmt(HWND hDlg, OPENFILENAME* lpOFN, BOOL bUpdCompressions, BOOL bUpdBitDepths, BOOL onlyEnable)
{
    DWORD format;
    //  char   *ext;
    PVFS_FTYPE* ctype = fs_comptypes;
    int cnt = 0, i, compr = PVCS_DEFAULT;
    LRESULT index;
    DWORD cm, clrs;
    BOOL bSelected = FALSE;
    HWND hCtrl;
    static TwoWords colorDepths[] = {{2, IDS_CLRS_MONO},
                                     {16, IDS_CLRS_16},
                                     {256, IDS_CLRS_256},
                                     {256, IDS_CLRS_256GR},
                                     {PV_COLOR_HC15, IDS_CLRS_HC15},
                                     {PV_COLOR_HC16, IDS_CLRS_HC16},
                                     {PV_COLOR_TC24, IDS_CLRS_TC24},
                                     {PV_COLOR_TC32, IDS_CLRS_TC32}};

    EnableDisableControls(hDlg, IDC_SAVE_COMMENT_FIRST, IDC_SAVE_COMMENT_LAST,
                          GetFormatInfo(lpOFN, &format, NULL /*&ext*/));
    if (!onlyEnable)
    {
        cm = ((SAVEAS_INFO_PTR)lpOFN->lCustData)->pvii->ColorModel;
        clrs = ((SAVEAS_INFO_PTR)(lpOFN->lCustData))->pvii->Colors;

        if (clrs == 2)
        {
            // save bilevel images as bilevel by default: some format may claim PVCM_GRAYS
            cm = PVCM_RGB;
        }
        hCtrl = GetDlgItem(hDlg, IDC_SAVE_BIT_DEPTH);
        if (bUpdBitDepths)
        {
            if (!bUpdCompressions)
            {
                // user clicked compressions -> update available bit-depths
                compr = GetItemData(hDlg, IDC_SAVE_COMPRESSION);
                if (compr < 0)
                {
                    compr = PVCS_DEFAULT;
                }
            }
            SendMessage(hCtrl, CB_RESETCONTENT, 0, 0);
            if ((clrs > 2) && (clrs < 16))
                clrs = 16;
            if ((clrs > 16) && (clrs < 256))
                clrs = 256;
            // Patera 2005.03.20: Save originally 32bit images as 24bit: The alpha was lost anyway
            if (clrs == PV_COLOR_TC32)
                clrs = PV_COLOR_TC24;
            // 256 Grayscale are coded as 256 | SAVEAS_GRAY_FLAG in item data
            for (i = 0; i < sizeof(colorDepths) / sizeof(colorDepths[0]); i++)
            {
                if ((colorDepths[i][0] >= clrs) || (i > 0))
                {
                    if (PVW32DLL.PVIsOutCombSupported(format, compr, colorDepths[i][0],
                                                      colorDepths[i][1] != IDS_CLRS_256GR ? PVCM_RGB : PVCM_GRAYS) != -1)
                    {
                        index = SendMessage(hCtrl, CB_ADDSTRING, 0, (LPARAM)LoadStr(colorDepths[i][1]));
                        SendMessage(hCtrl, CB_SETITEMDATA, index, colorDepths[i][0] | (colorDepths[i][1] != IDS_CLRS_256GR ? 0 : SAVEAS_GRAY_FLAG));
                        if ((clrs == colorDepths[i][0]) && ((colorDepths[i][1] != IDS_CLRS_256GR) ^ (cm == PVCM_GRAYS)))
                        {
                            SendMessage(hCtrl, CB_SETCURSEL, index, 0);
                            bSelected = TRUE;
                        }
                    }
                }
            }
            if (!bSelected)
            {
                // Selected format doesn't support chosen bitdepth -> find the smallest higher
                // or highest smaller non-grays if there is no higher
                for (i = 0; i < SendMessage(hCtrl, CB_GETCOUNT, 0, 0); i++)
                {

                    DWORD clrs2 = (DWORD)SendMessage(hCtrl, CB_GETITEMDATA, i, 0); // X64 - ITEMDATA obsahuje DWORD
                    if (!(clrs2 & SAVEAS_GRAY_FLAG))
                    {
                        SendMessage(hCtrl, CB_SETCURSEL, i, 0);
                    }
                    if (((clrs2 & SAVEAS_GRAY_MASK) >= clrs) && (((clrs2 & SAVEAS_GRAY_FLAG) == 0) ^ (cm == PVCM_GRAYS)))
                    {
                        // Note: can never be equal
                        clrs = clrs2;
                        break;
                    }
                    if ((clrs == PV_COLOR_TC32) && (clrs2 == PV_COLOR_TC24))
                    {
                        // saving 32bit image to a format supporting 24 bits at max
                        clrs = PV_COLOR_TC24;
                    }
                }
            }
        }
        else
        {
            clrs = GetItemData(hDlg, IDC_SAVE_BIT_DEPTH);
            if (clrs & SAVEAS_GRAY_FLAG)
            {
                cm = PVCM_GRAYS;
                clrs &= SAVEAS_GRAY_MASK;
            }
        }
        hCtrl = GetDlgItem(hDlg, IDC_SAVE_COMPRESSION);
        if (bUpdCompressions)
        {
            LRESULT oldCompr = -1;

            bSelected = FALSE;
            // Get the current selection, if any
            index = SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
            if (index >= 0)
            {
                oldCompr = SendMessage(hCtrl, CB_GETITEMDATA, index, 0);
                if (bUpdBitDepths && (((oldCompr == PVCS_JPEG_HUFFMAN) || (oldCompr == PVCS_NO_COMPRESSION)) && (format == PVF_TIFF)))
                {
                    // looks like changing file format from JPEG to TIFF -> don't make JPEG-compr or raw the default for TIFF
                    oldCompr = -1;
                }
            }
            SendMessage(hCtrl, CB_RESETCONTENT, 0, 0);
            while (ctype->ext)
            {
                if (PVW32DLL.PVIsOutCombSupported(format, ctype->type, clrs,
                                                  cm == PVCM_CMYK ? PVCM_RGB : cm) != -1)
                {
                    index = SendMessage(hCtrl, CB_ADDSTRING, 0, (LPARAM)ctype->ext);
                    SendMessage(hCtrl, CB_SETITEMDATA, index, ctype->type);
                    if (ctype->type == oldCompr)
                    {
                        // restore the original selection
                        SendMessage(hCtrl, CB_SETCURSEL, index, 0);
                        bSelected = TRUE;
                    }
                    cnt++;
                }
                ctype++;
            }
            if (cnt != 1)
            {
                // Note: cnt is zero for formats not directly supporting this bit depth (i.e. saving TC img as GIF)
                SendMessage(hCtrl, CB_INSERTSTRING, 0, (LPARAM)LoadStr(IDS_SAVE_DEFAULT));
            }
            if (!bSelected)
            {
                // select the first (default or only) compression if nothing is selected yet
                SendMessage(hCtrl, CB_SETCURSEL, 0 /*index*/, 0);
            }
        }
    }
    cnt = GetItemData(hDlg, IDC_SAVE_COMPRESSION);
    EnableDisableControls(hDlg, IDC_SAVE_GIF_FIRST, IDC_SAVE_GIF_LAST, format == PVF_GIF);
    EnableDisableControls(hDlg, IDC_SAVE_JPEG_FIRST, IDC_SAVE_JPEG_LAST, (format == PVF_JPG) || (cnt == PVCS_JPEG_HUFFMAN));
    EnableDisableControls(hDlg, IDC_SAVE_TIFF_FIRST, IDC_SAVE_TIFF_LAST, format == PVF_TIFF);

    RECT r1, r2;
    GetWindowRect(GetParent(hDlg), &r1);
    GetWindowRect(GetDlgItem(hDlg, IDC_SAVE_ADVANCED), &r2);
    BOOL advancedVersion = r2.bottom + 50 <= r1.bottom;
    if (!advancedVersion)
        EnableDisableControls(hDlg, IDC_SAVE_ADVANCED_FIRST, IDC_SAVE_ADVANCED_LAST, FALSE); // Advanced je zabaleny -> disable vseho (aby pres to nechodil TAB)
} /* FillTypeFmt */

void OnSaveAsCommand(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    RECT r1, r2;

    switch
        LOWORD(wParam)
        {
        case IDC_SAVE_ADVANCED:
        {
            GetWindowRect(GetParent(hDlg), &r1);
            GetWindowRect(GetDlgItem(hDlg, IDC_SAVE_ADVANCED), &r2);
            BOOL shortVersion = (r2.bottom + 50 > r1.bottom);
            CheckDlgButton(hDlg, IDC_SAVE_ADVANCED, shortVersion ? BST_CHECKED : BST_UNCHECKED);
            if (shortVersion)
            {
                // short version visible only
                GetWindowRect(GetDlgItem(hDlg, IDC_SAVE_GROUP_TIFF), &r2);
                EnableDisableControls(hDlg, IDC_SAVE_ADVANCED_FIRST, IDC_SAVE_ADVANCED_LAST, TRUE); // Advanced bude vybaleny -> enable vseho (aby pres to chodil TAB)
            }
            r1.bottom = r2.bottom + 10;
            SetWindowPos(GetParent(hDlg), 0, 0, 0,
                         r1.right - r1.left, r1.bottom - r1.top, SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
            SetWindowPos(hDlg, 0, 0, 0,
                         r1.right - r1.left, r2.bottom - r1.top, SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
            FillTypeFmt(hDlg, (OPENFILENAME*)GetWindowLongPtr(hDlg, GWLP_USERDATA), FALSE, FALSE, TRUE);
            break;
        }
        case IDC_SAVE_COMPRESSION:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                FillTypeFmt(hDlg, (OPENFILENAME*)GetWindowLongPtr(hDlg, GWLP_USERDATA), FALSE, TRUE, FALSE);
            }
            break;
        case IDC_SAVE_BIT_DEPTH:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                FillTypeFmt(hDlg, (OPENFILENAME*)GetWindowLongPtr(hDlg, GWLP_USERDATA), TRUE, FALSE, FALSE);
            }
            break;
        }
} /* OnSaveAsCommand */

BOOL CALLBACK MoveControlsProc(HWND hWnd, LPARAM shift)
{
    int ID = GetDlgCtrlID(hWnd);

    if ((ID != IDC_SAVE_COMPRESSION_LABEL) && (ID != IDC_SAVE_COMPRESSION) && (ID != IDC_SAVE_ADVANCED))
    {
        RECT r;

        GetWindowRect(hWnd, &r);
        ScreenToClient(GetParent(hWnd), (LPPOINT)&r);
        SetWindowPos(hWnd, 0, r.left + (int)shift, r.top, 0, 0, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOSIZE);
    }
    return TRUE;
}

void PositionControls(HWND hDlg)
{
    RECT r1, r2;

    // Determine the horizontal shift
    GetWindowRect(GetDlgItem(hDlg, IDC_SAVE_COMPRESSION_LABEL), &r1);
    PositionControl(hDlg, 0x441, IDC_SAVE_COMPRESSION_LABEL);
    GetWindowRect(GetDlgItem(hDlg, IDC_SAVE_COMPRESSION_LABEL), &r2);
    PositionControl(hDlg, 0x470, IDC_SAVE_COMPRESSION);
    PositionControl(hDlg, IDCANCEL, IDC_SAVE_ADVANCED);
    EnumChildWindows(hDlg, MoveControlsProc, r2.left - r1.left);
} /* PositionControls */

UINT_PTR CALLBACK SaveAsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    union
    {
        LPOFNOTIFY lpOFNot;
        SAVEAS_INFO_PTR psai;
        HWND hCtrl;
    };
    static TwoDWords sRots[] = {
        {IDS_ROT_NONE, 0},
        {IDS_ROT_90, PVSF_ROTATE90},
        {IDS_ROT_180, PVSF_FLIP_VERT | PVSF_FLIP_HOR},
        {IDS_ROT_270, PVSF_ROTATE90 | PVSF_FLIP_VERT | PVSF_FLIP_HOR},
        {0, 0}};
    static TwoDWords sFlips[] = {
        {IDS_FLIP_NONE, 0},
        {IDS_FLIP_VERT, PVSF_FLIP_VERT},
        {IDS_FLIP_HOR, PVSF_FLIP_HOR},
        {0, 0}};

    CALL_STACK_MESSAGE5("SaveAsDlgProc(h: 0x%p, m: 0x%x, w: 0x%IX, l: 0x%IX", hDlg, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
        // SalamanderGUI->ArrangeHorizontalLines(hDlg);  // pro Windows common dialogy tohle nedelame
        PositionControls(hDlg);

        SalamanderGUI->AttachButton(hDlg, IDC_SAVE_ADVANCED, BTF_MORE | BTF_CHECKBOX);

        FillTypeFmt(hDlg, (OPENFILENAME*)lParam, TRUE, TRUE, FALSE);
        EnableDisableControls(hDlg, IDC_SAVE_ADVANCED_FIRST, IDC_SAVE_ADVANCED_LAST, FALSE); // Advanced je zabaleny -> disable vseho (aby pres to nechodil TAB)
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
        psai = (SAVEAS_INFO_PTR)(((OPENFILENAME*)lParam)->lCustData);
        FillCombo(hDlg, IDC_SAVE_ROTATION, sRots, psai->Rotation);
        FillCombo(hDlg, IDC_SAVE_FLIP, sFlips, psai->Flip);
        if (psai->Flags & PVSF_INVERT)
        {
            CheckDlgButton(hDlg, IDC_SAVE_INVERT, BST_CHECKED);
        }
        if (G.Save.Flags & PVSF_INTERLACE)
        {
            CheckDlgButton(hDlg, IDC_SAVE_GIF_INTERLACED, BST_CHECKED);
        }
        if (G.Save.Flags & PVSF_GIF89)
        {
            CheckDlgButton(hDlg, IDC_SAVE_GIF_89a, BST_CHECKED);
        }
        if (!(G.Save.Flags & PVSF_DO_NOT_STRIP))
        {
            CheckDlgButton(hDlg, IDC_SAVE_TIFF_MAKE_STRIPS, BST_CHECKED);
        }

        SetDlgItemInt(hDlg, IDC_SAVE_JPEG_QUALITY, G.Save.JPEGQuality, FALSE);
        SendDlgItemMessage(hDlg, IDC_SAVE_JPEG_QUALITY, EM_LIMITTEXT, 3, 0);
        hCtrl = GetDlgItem(hDlg, IDC_SAVE_JPEG_SUBSAMPLING);
        SendMessage(hCtrl, CB_ADDSTRING, 0, (LPARAM) _T("1:1:1"));
        SendMessage(hCtrl, CB_ADDSTRING, 0, (LPARAM) _T("2:1:1"));
        SendMessage(hCtrl, CB_SETCURSEL, G.Save.JPEGSubsampling, 0);
        SetDlgItemInt(hDlg, IDC_SAVE_TIFF_STRIP_SIZE, G.Save.TIFFStripSize, FALSE);
        SendDlgItemMessage(hDlg, IDC_SAVE_COMMENT, EM_LIMITTEXT, SAVEAS_MAX_COMMENT_SIZE - 1, 0);
        SalamanderGeneral->MultiMonCenterWindow(GetParent(hDlg), GetParent(GetParent(hDlg)), FALSE);
        break;
    case WM_COMMAND:
        OnSaveAsCommand(hDlg, wParam, lParam);
        break;
    case WM_DESTROY:
        psai = (SAVEAS_INFO_PTR)((OPENFILENAME*)GetWindowLongPtr(hDlg, GWLP_USERDATA))->lCustData;
        psai->Compression = GetItemData(hDlg, IDC_SAVE_COMPRESSION);
        psai->Flags = 0;
        psai->Flip = psai->Flags |= GetItemData(hDlg, IDC_SAVE_FLIP);
        // Note: must remain ^= to anihilate double flip flags
        psai->Flags ^= psai->Rotation = GetItemData(hDlg, IDC_SAVE_ROTATION);
        psai->Colors = GetItemData(hDlg, IDC_SAVE_BIT_DEPTH);
        if (IsDlgButtonChecked(hDlg, IDC_SAVE_INVERT))
        {
            psai->Flags |= PVSF_INVERT;
        }
        GetDlgItemTextA(hDlg, IDC_SAVE_COMMENT, psai->Comment, SAVEAS_MAX_COMMENT_SIZE);
        psai->Comment[SAVEAS_MAX_COMMENT_SIZE - 1] = 0;
        G.Save.Flags = 0;
        if (IsDlgButtonChecked(hDlg, IDC_SAVE_GIF_INTERLACED))
        {
            G.Save.Flags |= PVSF_INTERLACE;
        }
        if (IsDlgButtonChecked(hDlg, IDC_SAVE_GIF_89a))
        {
            G.Save.Flags |= PVSF_GIF89;
        }
        if (!IsDlgButtonChecked(hDlg, IDC_SAVE_TIFF_MAKE_STRIPS))
        {
            G.Save.Flags |= PVSF_DO_NOT_STRIP;
        }
        G.Save.JPEGQuality = GetDlgItemInt(hDlg, IDC_SAVE_JPEG_QUALITY, NULL, FALSE);
        G.Save.JPEGQuality = min(100, G.Save.JPEGQuality);
        G.Save.JPEGQuality = max(G.Save.JPEGQuality, 1);
        G.Save.JPEGSubsampling = (DWORD)SendDlgItemMessage(hDlg, IDC_SAVE_JPEG_SUBSAMPLING, CB_GETCURSEL, 0, 0);
        G.Save.TIFFStripSize = GetDlgItemInt(hDlg, IDC_SAVE_TIFF_STRIP_SIZE, NULL, FALSE);
        break;
    case WM_NOTIFY:
        lpOFNot = (LPOFNOTIFY)lParam;
        if (lpOFNot->hdr.code == CDN_TYPECHANGE)
        {
            FillTypeFmt(hDlg, lpOFNot->lpOFN, TRUE, TRUE, FALSE);
        }
        break;
    }
    return FALSE;
}

const char* StrIStr(const char* txt, const char* pattern)
{
    if (txt == NULL || pattern == NULL)
        return NULL;

    const char* s = txt;
    int len = (int)strlen(pattern);
    int txtLen = (int)strlen(txt);
    while (txtLen >= len)
    {
        if (SalamanderGeneral->StrNICmp(s, pattern, len) == 0)
            return s;
        s++;
        txtLen--;
    }
    return NULL;
}

BOOL CRendererWindow::OnFileSaveAs(LPCTSTR pInitDir)
{
    static int cntClipboard = 1;
    static int cntCapture = 1;
    static int cntScan = 1;
    static SAVEAS_INFO sai = {PVCS_DEFAULT, 0, 0, 0, 0, 0, ""};
    SAVEAS_INFO lsai;
    TCHAR errBuff[1000];
    TCHAR fileName[MAX_PATH];
    LPTSTR s;
    int* pCnt = NULL;
    struct
    {
        OPENFILENAME ofn;
#if _WIN32_WINNT < 0x0500
        // Ver5 struct size needed for showing Favorites when template is used
        void* pvReserved;
        DWORD dwReserved;
        DWORD FlagsEx;
#endif
    } ofn;
    int ret;
    DWORD format;
    TCHAR initDir[MAX_PATH] = _T("");

    if (((pvii.Format == PVF_ICO) || (pvii.Format == PVF_PNG) || (pvii.Format == PVF_TGA) || (pvii.Format == PVF_TIFF) || (pvii.Format == PVF_ANI) || (pvii.Format == PVF_PSD)) && (pvii.Colors == PV_COLOR_TC32))
    {
        /*     MSGBOXEX_PARAMS  mboxParams;

     memset(&mboxParams, 0, sizeof(mboxParams));
     mboxParams.HParent = HWindow;
     mboxParams.Text = LoadStr(IDS_SAVE_LOST_ALPHA); mboxParams.Caption = LoadStr(IDS_PLUGINNAME);
     mboxParams.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_ESCAPEENABLED;
     mboxParams.CheckBoxText = LoadStr(IDS_DONT_SHOW_AGAIN);
*/
        if (!(G.DontShowAnymore & DSA_ALPHA_LOST))
        {
            BOOL bChecked = FALSE;
            int ret2 = ShowOneTimeMessage(HWindow, IDS_SAVE_LOST_ALPHA, &bChecked,
                                          MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT,
                                          IDS_DONT_SHOW_AGAIN_SLA);

            if (bChecked)
            {
                G.DontShowAnymore |= DSA_ALPHA_LOST;
            }
            if (IDYES != ret2)
            {
                //            SalamanderGeneral->SalMessageBoxEx(&mboxParams)
                return FALSE;
            }
        }
    }
    // use local copy to make several simultaneously open SaveAs dialogs work
    lsai = sai;
    if (pInitDir)
    {
        lstrcpyn(initDir, pInitDir, SizeOf(initDir));
    }
    else
    {
        if (FileName[0] == '<')
        {
            lstrcpyn(initDir, G.Save.InitDir, SizeOf(initDir));
        }
        else
        {
            lstrcpyn(initDir, FileName, SizeOf(initDir));
            if (!SalamanderGeneral->CutDirectory(initDir))
                initDir[0] = 0;
        }
    }

    if (initDir[0] == 0)
        GetMyDocumentsPath(initDir);

    // store the filename without path and suffix
    s = (LPTSTR)_tcsrchr(FileName, '\\');
    _tcscpy(fileName, s ? (s + 1) : _T(""));
    s = (LPTSTR)_tcsrchr(fileName, '.');
    if (s)
        *s = 0; // ".cvspass" is extension in Windows
    if (FileName[0] == '<')
    {
        int nameID;

        if (!_tcscmp(FileName, LoadStr(IDS_CLIPBOARD_TITLE)))
        {
            nameID = IDS_CLIPBOARD_FNAME;
            pCnt = &cntClipboard;
        }
        else if (!_tcscmp(FileName, LoadStr(IDS_CAPTURE_TITLE)))
        {
            nameID = IDS_CAPTURE_FNAME;
            pCnt = &cntCapture;
        }
        else if (!_tcscmp(FileName, LoadStr(IDS_SCAN_TITLE)))
        {
            nameID = IDS_SCAN_FNAME;
            pCnt = &cntScan;
        }
        else
        { // can only be deleted image -> no name provided
            fileName[0] = 0;
            nameID = -1;
        }
        if (nameID != -1)
        {
            _stprintf(fileName, _T("%s%d"), LoadStr(nameID), *pCnt);
        }
    }

    memset(&ofn, 0, sizeof(ofn));
    ofn.ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.ofn.lStructSize = sizeof(ofn);
    ofn.ofn.hwndOwner = HWindow;
    TCHAR filterStr[1000];
    if (pvii.Colors == 2)
    {
        ofn.ofn.nFilterIndex = G.LastSaveAsFilterIndexMono;
        lstrcpyn(filterStr, LoadStr(IDS_SAVEASFILTERMONO), 1000);
    }
    else
    {
        ofn.ofn.nFilterIndex = G.LastSaveAsFilterIndexColor;
        lstrcpyn(filterStr, LoadStr(IDS_SAVEASFILTERCOLOR), 1000);
    }
    s = filterStr;
    ofn.ofn.lpstrFilter = s;
    DWORD filtersDoubledCount = 0;
    while (*s != 0)
    { // vytvoreni double-null terminated listu
        if (*s == '|')
        {
            *s = 0;
            filtersDoubledCount++;
        }
        s++;
    }
    if (ofn.ofn.nFilterIndex > filtersDoubledCount / 2)
        ofn.ofn.nFilterIndex = filtersDoubledCount / 2;
    ofn.ofn.lpstrFile = fileName;
    ofn.ofn.nMaxFile = SizeOf(fileName);
    ofn.ofn.lpstrInitialDir = initDir;
    ofn.ofn.lpfnHook = SaveAsDlgProc;
    ofn.ofn.lpTemplateName = MAKEINTRESOURCE(IDD_SAVEEX);
    ofn.ofn.hInstance = HLanguage;
    ofn.ofn.lCustData = (LPARAM)&lsai;
    ofn.ofn.Flags = OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE | OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_NOTESTFILECREATE | OFN_HIDEREADONLY;
    lsai.pvii = &pvii;
    // Start with no rotation & no flip
    lsai.Rotation = lsai.Flip = 0;
    CALL_STACK_MESSAGE2(_T("OnFileSaveAs: GSFN(%s)"), FileName);
    for (;;)
    {
        LPTSTR s2;
        HANDLE hFile;

        if (!SalamanderGeneral->SafeGetSaveFileName(&ofn.ofn))
        {
            // don't save options
            return FALSE;
        }

        format = 0;
        if (pvii.Colors == 2)
        {
            // remember the filter index for the next time
            G.LastSaveAsFilterIndexMono = ofn.ofn.nFilterIndex;
        }
        else
        {
            G.LastSaveAsFilterIndexColor = ofn.ofn.nFilterIndex;
        }
        lstrcpyn(initDir, fileName, ofn.ofn.nFileOffset);
        initDir[ofn.ofn.nFileOffset - 1] = 0;
        GetFormatInfo(&ofn.ofn, &format, &s);
        if (_tcschr(s, '*') && (format == PVF_IRF))
        {
            // suffix depends on compression
            if ((lsai.Compression == PVCS_CCITT_4) || (lsai.Compression == PVCS_DEFAULT))
            {
                static char buffCIT[] = ".cit";
                s = _T(buffCIT);
            }
            else
            {
                static char buffDAT[] = ".dat";
                s = _T(buffDAT);
            }
        }

        if ((FileName[0] == '<') && G.Save.RememberPath)
            lstrcpyn(G.Save.InitDir, initDir, SizeOf(G.Save.InitDir));

        ret = (int)_tcslen(fileName);
        if (fileName[ret - 1] == '.')
        {
            // user doesn't want us to append any suffix
            fileName[ret - 1] = 0;
        }
        else
        {
            LPTSTR ext;

            ext = (LPTSTR)_tcsrchr(fileName /* + ofn.nFileOffset*/, '.');
            if (ext < fileName + ofn.ofn.nFileOffset)
            { // ".cvspass" is extension in Windows
                ext = fileName + ret;
            }
            if (StrIStr(s, ext))
                s = (LPTSTR)StrIStr(s, ext);
            // strip off additional suffixes, if present
            s2 = (LPTSTR)_tcschr(s, ';');
            if (s2)
                *s2 = 0;

            if (_tcsicmp(ext, s))
            {
                // not a default one
                s2 = (LPTSTR)_tcsrchr(fileName + ofn.ofn.nFileOffset, '\\');
                // find file name beginning
                if (!s2)
                {
                    s2 = fileName + ofn.ofn.nFileOffset;
                }
                else
                {
                    s2++;
                }
                // check whether it consists just of upper-case letters & digits
                while (((*s2 >= 'A') && (*s2 <= 'Z')) || ((*s2 >= '0') && (*s2 <= '9')) || (*s2 == '_'))
                    s2++;
                if (!*s2)
                {
                    // yes, it does -> make it lower-case like extension
                    _tcslwr(fileName + ofn.ofn.nFileOffset);
                }
                if ((ext - fileName) + strlen(s) < _countof(fileName))
                    _tcscat(ext, s); // append the default suffix
                else
                {
                    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME),
                                                     LoadStr(IDS_ERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    return FALSE;
                }
            }
        }
        hFile = CreateFile(fileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, 0, 0);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            _stprintf(errBuff, LoadStr(IDS_SAVE_ERR_EXISTS_OVERWRITE), fileName);
            ret = SalamanderGeneral->SalMessageBox(HWindow, errBuff,
                                                   LoadStr(IDS_ERRORTITLE), MB_ICONEXCLAMATION | MB_YESNOCANCEL);
            if (ret == IDCANCEL)
            {
                // store options
                sai = lsai;
                return TRUE;
            }
            if (ret != IDYES)
            {
                // ask for a new name
                continue;
            }
        }
        else
        {
            ret = GetLastError();
            if (ret == ERROR_FILE_NOT_FOUND)
            {
                break;
            }
            if (ret == ERROR_ACCESS_DENIED)
            {
                // No rights or R/O attribute - check what is the case
                ret = SalamanderGeneral->SalGetFileAttributes(fileName); // 0xFFFFFFFF on error
                if ((ret != 0xFFFFFFFF) && (ret & FILE_ATTRIBUTE_READONLY))
                {
                    // R/O attrib
                    _stprintf(errBuff, LoadStr(IDS_READ_ONLY_REWRITE), fileName);
                    ret = SalamanderGeneral->SalMessageBox(HWindow, errBuff,
                                                           LoadStr(IDS_ERRORTITLE), MB_ICONEXCLAMATION | MB_YESNOCANCEL);
                    if (ret == IDCANCEL)
                    {
                        sai = lsai; // store options
                        return TRUE;
                    }
                    if (ret != IDYES)
                    {
                        // ask for a new name
                        continue;
                    }
                    SalamanderGeneral->ClearReadOnlyAttr(fileName);
                }
                // else: no rights: DeleteFile should also fail with ERROR_ACCESS_DENIED
            }
        }
        if (!DeleteFile(fileName))
        {
            ret = GetLastError();
            SalamanderGeneral->GetErrorText(ret, errBuff, SizeOf(errBuff));
            if (IDCANCEL == SalamanderGeneral->SalMessageBox(HWindow, errBuff,
                                                             LoadStr(IDS_ERRORTITLE), MB_ICONEXCLAMATION | MB_OKCANCEL))
            {
                sai = lsai; // store options
                return TRUE;
            }
        }
        else
        {
            break; // file deleted successfully
        }
    }
    ret = SaveImage(fileName, format, &lsai);

    // ohlasime zmenu na ceste (pribyl nas soubor)
    TCHAR changedPath[MAX_PATH];
    lstrcpyn(changedPath, fileName, MAX_PATH);
    SalamanderGeneral->CutDirectory(changedPath);
    SalamanderGeneral->PostChangeOnPathNotification(changedPath, FALSE);

    if (ret != PVC_OK)
    {
        if (ret != PVC_CANCELED)
        {
            _stprintf(errBuff, LoadStr(IDS_SAVEERROR), PVW32DLL.PVGetErrorText(ret)); //"Canceled (error example)");
            SalamanderGeneral->SalMessageBox(HWindow, errBuff, LoadStr(IDS_ERRORTITLE),
                                             MB_ICONEXCLAMATION | MB_OK);
        }
        else
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CANCELED_BY_USER),
                                             LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION | MSGBOXEX_SILENT);
        }
    }
    else
    {
        if (pCnt)
        {
            (*pCnt)++; // increase counter only on successful save
        }
        if (!(G.DontShowAnymore & DSA_SAVE_SUCCESS))
        {
            BOOL checked = FALSE;

            ShowOneTimeMessage(HWindow, IDS_SAVE_AS_SUCCESS, &checked, MSGBOXEX_OK);
            if (checked)
            {
                G.DontShowAnymore |= DSA_SAVE_SUCCESS;
            }
        }
        else
        {
            Viewer->SetStatusBarTexts(IDS_SAVE_AS_SUCCESS);
            bEatSBTextOnce = TRUE;
        }
        sai.PrevInputColors = pvii.Colors;
    }
    sai = lsai;
    return TRUE;
}

// ulozi obrazek do souboru 'name' ve formatu 'format' (PVF_xxx)
// vraci navratovou hodnotu funkce PVSaveImage
int CRendererWindow::SaveImage(LPCTSTR fileName, DWORD format, SAVEAS_INFO_PTR psai)
{
    PVSaveImageInfo sii;
    sProgBarInfo pbi;

    memset(&sii, 0, sizeof(PVSaveImageInfo));
    sii.cbSize = sizeof(PVSaveImageInfo);
    sii.Format = format;
    sii.ColorModel = pvii.ColorModel == PVCM_CMYK ? PVCM_RGB : pvii.ColorModel;
    // psai is NULL when saving wallpaper
    sii.Colors = psai ? (psai->Colors & SAVEAS_GRAY_MASK) : pvii.Colors;
    if (psai)
    {
        if (psai->Colors & SAVEAS_GRAY_FLAG)
        {
            sii.ColorModel = PVCM_GRAYS;
        }
        else if (sii.Colors > 256)
        {
            // must be reset to RGB when saving originally Grayscale image as Hi/TrueColor
            sii.ColorModel = PVCM_RGB;
        }
    }
    sii.Compression = psai ? psai->Compression : PVCS_DEFAULT;
    sii.Flags = psai ? (psai->Flags & (PVSF_INVERT | PVSF_ROTATE90 | PVSF_FLIP_HOR | PVSF_FLIP_VERT)) : 0;
    // Flip DPI if rotating
    sii.HorDPI = (sii.Flags & PVSF_ROTATE90) ? pvii.VerDPI : pvii.HorDPI;
    sii.VerDPI = (sii.Flags & PVSF_ROTATE90) ? pvii.HorDPI : pvii.VerDPI;
#if 0
  if (PVW32DLL.PVIsOutCombSupported(sii.Format, PVCS_DEFAULT, sii.Colors, sii.ColorModel) == -1) {
     // the target format does not support the source bit depth with any compression scheme
     // we must perform some bit-depth conversion
     int i;
     for (i = 0; i < 6; i++) {
        switch (sii.Colors) { // we try to upgrade bit depth
           case 2: sii.Colors = 16; break;
           case PV_COLOR_HC15: sii.Colors = PV_COLOR_HC16; break;
           case PV_COLOR_HC16: sii.Colors = PV_COLOR_TC24; break;
           case PV_COLOR_TC24: sii.Colors = PV_COLOR_TC32; break;
           case PV_COLOR_TC32: sii.Colors = 256; break; // some formats are 8bit only
           default: if ((sii.Colors >= 3) && (sii.Colors <= 16)) sii.Colors = 256;
             else if ((sii.Colors > 16) && (sii.Colors <= 256)) sii.Colors = PV_COLOR_HC15;
        }
        if (PVW32DLL.PVIsOutCombSupported(sii.Format, PVCS_DEFAULT, sii.Colors, sii.ColorModel) != -1) {
           break; // found one!!
        }
     }
  }
#endif
    if (fMirrorHor)
    {
        // Patch: PVW32Cnv.dll will mirror the image in memory
        sii.Flags ^= PVSF_FLIP_HOR;
        fMirrorHor = FALSE;
        PVW32DLL.PVSetStretchParameters(PVHandle, XStretchedRange,
                                        YStretchedRange * (1 - 2 * fMirrorVert), COLORONCOLOR);
    }
    if (fMirrorVert)
    {
        sii.Flags ^= PVSF_FLIP_VERT;
    }
    if (sii.Format == PVF_GIF)
    {
        sii.Flags |= G.Save.Flags & (PVSF_GIF89 | PVSF_INTERLACE);
    }
    if (sii.Format == PVF_JPG)
    {
        sii.Misc.JPEG.Quality = G.Save.JPEGQuality;
        // 0 means 2x1:1:1        = here 0 means 1:1:1
        sii.Misc.JPEG.SubSampling = !G.Save.JPEGSubsampling;
    }
    if (sii.Format == PVF_TIFF)
    {
        sii.Flags |= G.Save.Flags & PVSF_DO_NOT_STRIP;
        sii.Misc.TIFF.StripSize = G.Save.TIFFStripSize;
        sii.Misc.TIFF.JPEGQuality = G.Save.JPEGQuality;
        // 0 means 2x1:1:1        = here 0 means 1:1:1
        sii.Misc.TIFF.JPEGSubSampling = !G.Save.JPEGSubsampling;
    }
    sii.Transp.Flags = PVTF_ORIGINAL; // Preserve transparency
    if (psai && psai->Comment[0])
    {
        sii.Comment = psai->Comment;
        // we save terminating zero only in TIFFs
        sii.CommentSize = (int)strlen(sii.Comment) + (sii.Format == PVF_TIFF ? 1 : 0);
    }

    // zajistime prekresleni okna, at nestrasi behem delsich savu v rozhrabanem stavu po SaveAs dialogu
    UpdateWindow(Viewer->HWindow);
    HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    CALL_STACK_MESSAGE6("Save pars: %ux%ux%u, %u, %u", pvii.Width, pvii.Height, sii.Colors, sii.Format, sii.Flags);

    Viewer->InitProgressBar();
    pbi.pViewer = Viewer;
    pbi.lastCheckTicks = pbi.lastUpdateTicks = GetTickCount();
#ifdef _UNICODE
    char fileNameA[_MAX_PATH];

    WideCharToMultiByte(CP_ACP, 0, fileName, -1, fileNameA, sizeof(fileNameA), NULL, NULL);
    fileNameA[sizeof(fileNameA) - 1] = 0;
    int saveRet = PVW32DLL.PVSaveImage(PVHandle, fileNameA, &sii, SaveProgressProcedure, &pbi, pvii.CurrentImage);
#else
    int saveRet = PVW32DLL.PVSaveImage(PVHandle, fileName, &sii, SaveProgressProcedure, &pbi, pvii.CurrentImage);
#endif
    Viewer->KillProgressBar();
    SetCursor(hOldCur);

    return saveRet;
}
