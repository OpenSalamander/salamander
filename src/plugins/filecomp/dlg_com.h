// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// pro okno filecompu; WM_USER_WORKERNOTIFIES informuje o ukonceni porovnavani
// a predava vysledky pokud window proc vrati TRUE je okno zodpovedne za
// deallokaci predanych bufferu
#define WM_USER_WORKERNOTIFIES (WM_APP + 1)

// wParam:
enum
{
    WN_ERROR,                // (const char *)lParam obsahuje text chyby
    WN_NO_DIFFERENCE,        // lParam je ignorovan
    WN_NO_ALL_DIFFS_IGNORED, // lParam je ignorovan
    WN_BINARY_FILES_DIFFER,  // (CBinaryCompareResults *)lParam obsahuje vysledky diffovani
    WN_TEXT_FILES_DIFFER,    // (CTextCompareResults<char> *)lParam obsahuje vysledky diffovani
    WN_UNICODE_FILES_DIFFER, // (CCTextCompareesults<wchar_t> *)lParam obsahuje vysledky diffovani
    WN_WORKER_CANCELED,      // lParam je ignorovan
    WN_SETCHANGES,           // (CBinaryChanges *) lParam je pole zmen
    WN_CBINIT_FINISHED,      // combo box byl upsesne inicializovan
    WN_CALCULATING_DETAILS,  // pocitaji se podrobne difference
    WN_COMPARE_FINISHED,     // comparison finishes
    WN_GET_CHANGESCOMBO,     // queries HWND of the Change combobox
    WN_SET_PROGRESS,         // sets progress MAKELPARAM(percent, changesCnt)
    WN_ADD_CHANGE            // registers a change in binary comparison
};

#define WM_USER_VSCROLL (WM_APP + 2)
#define WM_USER_HSCROLL (WM_APP + 3)
#define WM_USER_SLITPOSCHANGED (WM_APP + 4)
#define WM_USER_MOUSEWHEEL (WM_APP + 5)     // [wParam, lParam] z WM_MOUSEWHEEL
#define WM_USER_ACTIVATEWINDOW (WM_APP + 6) // [wParam, lParam] z WM_MOUSEWHEEL
#define WM_USER_CREATE (WM_APP + 7)
#define WM_USER_HANDLEFILEERROR (WM_APP + 8)
#define WM_USER_SETCURSOR (WM_APP + 9) //jako WM_SETCURSOR
#define WM_USER_CLEARHISTORY (WM_APP + 10)
#define WM_USER_GETREBARHEIGHT (WM_APP + 11)
#define WM_USER_GETHEADERHEIGHT (WM_APP + 12)
#define WM_USER_AUTOCOPY_CHANGED (WM_APP + 13)

// [wParam, (HWND) lParam] - pro otevrena okna pluginu: konfigurace pluginu se zmenila
#define WM_USER_CFGCHNG (WM_APP + 3246)

// wParam: kombinace nasledujicich hodnot
#define CC_FONT 0x01
#define CC_COLORS 0x02
#define CC_DEFOPTIONS 0x04
#define CC_CONTEXT 0x08
#define CC_TABSIZE 0x10
#define CC_CHAR 0x20
#define CC_HAVEHWND 0x40
#define CC_ALL (CC_FONT | CC_COLORS | CC_DEFOPTIONS | CC_CONTEXT | CC_TABSIZE | CC_CHAR)

// barvy

#define LINENUM_FG_NORMAL 0 // barvy textu ve sloupci s cisly radek
#define LINENUM_FG_LEFT_CHANGE 1
#define LINENUM_FG_RIGHT_CHANGE 2
#define LINENUM_FG_LEFT_CHANGE_FOCUSED 3
#define LINENUM_FG_RIGHT_CHANGE_FOCUSED 4
#define LINENUM_BK_NORMAL 5
#define LINENUM_BK_LEFT_CHANGE 6
#define LINENUM_BK_RIGHT_CHANGE 7
#define LINENUM_BK_LEFT_CHANGE_FOCUSED 8
#define LINENUM_BK_RIGHT_CHANGE_FOCUSED 9

#define LINENUM_LEFT_BORDER 10 // ramecek kolem aktualni zmeny
#define LINENUM_RIGHT_BORDER 11

#define TEXT_FG_NORMAL 12 // barvy zobrazeneho obsahu souboru
#define TEXT_FG_LEFT_FOCUSED 13
#define TEXT_FG_RIGHT_FOCUSED 14
#define TEXT_FG_LEFT_CHANGE 15
#define TEXT_FG_RIGHT_CHANGE 16
#define TEXT_FG_LEFT_CHANGE_FOCUSED 17
#define TEXT_FG_RIGHT_CHANGE_FOCUSED 18
#define TEXT_BK_NORMAL 19
#define TEXT_BK_LEFT_FOCUSED 20
#define TEXT_BK_RIGHT_FOCUSED 21
#define TEXT_BK_LEFT_CHANGE 22
#define TEXT_BK_RIGHT_CHANGE 23
#define TEXT_BK_LEFT_CHANGE_FOCUSED 24
#define TEXT_BK_RIGHT_CHANGE_FOCUSED 25

#define TEXT_LEFT_BORDER 26 // ramecek kolem aktualni zmeny
#define TEXT_RIGHT_BORDER 27

#define TEXT_FG_SELECTION 28 // barva vybraneho bloku textu
#define TEXT_BK_SELECTION 29

#define NUMBER_OF_COLORS 30 // pocet barev ve schematu

// interni drzak barvy, ktery navic obsahuje flag
typedef DWORD SALCOLOR;

// SALCOLOR flags
#define SCF_DEFAULT 0x01 // barevna slozka se ignoruje a pouzije se default hodnota

#define GetCOLORREF(rgbf) ((COLORREF)rgbf & 0x00ffffff)
#define GetPALETTERGB(rgbf) (0x02000000 | (COLORREF)rgbf & 0x00ffffff) // viz PALETTERGB ve wingdi.h
#define RGBF(r, g, b, f) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16) | (((DWORD)(BYTE)(f)) << 24)))
#define GetFValue(rgbf) ((BYTE)((rgbf) >> 24))
#define SetFValue(rgbf, f) ((rgbf) |= ((BYTE)f << 24))

inline void SetSALCOROR(SALCOLOR* salColor, COLORREF rgb, BYTE flags)
{
    CALL_STACK_MESSAGE_NONE
    *salColor = rgb & 0x00ffffff | (flags << 24);
}

inline void SetRGBPart(SALCOLOR* salColor, COLORREF rgb)
{
    CALL_STACK_MESSAGE_NONE
    *salColor = rgb & 0x00ffffff | (((DWORD)(BYTE)((BYTE)((*salColor) >> 24))) << 24);
}

extern SALCOLOR Colors[NUMBER_OF_COLORS];        // aktualni barvy
extern SALCOLOR DefaultColors[NUMBER_OF_COLORS]; // standardni barvy
extern COLORREF CustomColors[16];                // custom colors pro ChooseColor dialog

#if (WINVER < 0x0600)
typedef enum _NORM_FORM
{
    NormalizationOther = 0,
    NormalizationC = 0x1,
    NormalizationD = 0x2,
    NormalizationKC = 0x5,
    NormalizationKD = 0x6
} NORM_FORM;
#endif //(WINVER < 0x0600)

typedef int(WINAPI* TNormalizeString)(NORM_FORM NormForm,
                                      LPCWSTR lpSrcString,
                                      int cwSrcLength,
                                      LPWSTR lpDstString,
                                      int cwDstLength);

// Present in normaliz.dll on Win Vista+ or if idndlpackage.EXE or MSIE7 or WMP11 was installed on XP/2003
extern TNormalizeString PNormalizeString;

enum CFileViewMode
{
    fvmStandard = 0, // nemenit hodnoty
    fvmOnlyDifferences = 1
};

struct CRegBLOBConfiguration
{ // Configuration as stored in binary form in registry. DO NOT MODIFY!!!
    BOOL ConfirmSelection;
    int Context;
    int TabSize;
    LOGFONT FileViewLogFont;
    BOOL UseViewerFont;
    unsigned char WhiteSpace;

    // switches
    CFileViewMode ViewMode;
    BOOL ShowWhiteSpace;
    BOOL DetailedDifferences;
};

struct CConfiguration
{
    // Configuration as used by Salamander - superset of CRegBLOBConfiguration
    // Whenever you add any new item worth of storing in registry, please use separate reg key for it
    BOOL ConfirmSelection;
    int Context;
    int TabSize;
    LOGFONT FileViewLogFont;    // The font currently used by FC
    LOGFONT InternalViewerFont; // The font used by Internal Viewer. We cache the value because it can be
                                // obtained only in the main thread but we need it in the config dialog.
    BOOL UseViewerFont;         // TRUE when "automatic" font is used (the font used by Internal Viewer)
    unsigned char WhiteSpace;

    // switches
    CFileViewMode ViewMode;
    BOOL ShowWhiteSpace;
    BOOL DetailedDifferences;

    // Not part of CRegBLOBConfiguration:
    BOOL HorizontalView;
    BOOL AutoCopy; // AutoCopy Selection to Clipboard
};

extern CCompareOptions DefCompareOptions;

extern CConfiguration Configuration;

extern HACCEL HAccels;

extern int CaretWidth;

//****************************************************************************
//
// CCommonDialog
//
// Dialog centrovany k parentu
//

class CCommonDialog : public CDialog
{
public:
    CCommonDialog(int resID, HWND hParent, CObjectOrigin origin = ooStandard);
    CCommonDialog(int resID, int helpID, HWND hParent, CObjectOrigin origin = ooStandard);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

void CenterWindow(HWND hWindow);

// ****************************************************************************

BOOL CheckMouseWheelMsg(HWND window, UINT uMsg, WPARAM wParam, LPARAM lParam);

void UpdateDefaultColors(SALCOLOR* colors, HPALETTE& palette);

BOOL InitDialogs();
void ReleaseDialogs();
