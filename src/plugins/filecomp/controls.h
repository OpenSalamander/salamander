// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern HFONT EnvFont;     // font prostredi (edit, toolbar, header, status)
extern int EnvFontHeight; // vyska fontu
extern HBRUSH HDitheredBrush;

// ****************************************************************************
//
// CFileHeaderWindow
//

class CFileHeaderWindow : public CWindow
{
protected:
    char Text[MAX_PATH];
    int TextLen;
    COLORREF BkColor;
    HBRUSH BkgndBrush;

public:
    CFileHeaderWindow(const char* text);
    virtual ~CFileHeaderWindow();
    void SetText(const char* text);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

enum CSplitBarType
{
    sbHorizontal,
    sbVertical
};

#define SBP_LEFT 0
#define SBP_TOP SB_LEFT
#define SBP_RIGHT 1
#define SBP_BOTTOM SB_RIGHT

extern const char* SPLITBARWINDOW_CLASSNAME;

class CToolTipWindow;

class CSplitBarWindow : public CWindow
{
protected:
    CSplitBarType Type;
    BOOL Tracking;
    LONG Orig;
    CToolTipWindow* ToolTip;
    // int CursorHeight;
    double SplitProp;
    BOOL DoubleClick;

public:
    CSplitBarWindow(CSplitBarType type);
    CSplitBarType GetType() { return Type; };
    void SetType(CSplitBarType type) { Type = type; };

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

class CToolTipWindow : public CWindow
{
protected:
    char Text[10];
    int TextLen;

public:
    CToolTipWindow()
    {
        *Text = 0;
        TextLen = 0;
    }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

class CComboBoxEdit : public CWindow
{
protected:
public:
    CComboBoxEdit() { ; }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

class CComboBox : public CWindow
{
protected:
    HPEN BtnFacePen;
    HPEN BtnShadowPen;
    HPEN BtnHilightPen;
    BOOL Tracking;

public:
    CComboBox();
    virtual ~CComboBox();
    void ChangeColors();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

struct CBandParams
{
    UINT Width;
    UINT Style;
    int Index;
};

class CRebar : public CWindow
{
protected:
public:
    CRebar() { ; }
    BOOL InsertBand(UINT uIndex, LPREBARBANDINFO lprbbi);
    BOOL GetBandParams(UINT id, CBandParams* params);
    BOOL SetBandParams(UINT id, CBandParams* params);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
