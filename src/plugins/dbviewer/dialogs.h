// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CRendererWindow;

#define FIND_HISTORY_SIZE 10 // pocet pamatovanych stringu
extern char* FindHistory[FIND_HISTORY_SIZE];

//****************************************************************************
//
// CCommonDialog
//
// Dialog centrovany k parentu
//

class CCommonDialog : public CDialog
{
public:
    CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent);
    CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

//****************************************************************************
//
// CGoToDialog
//

class CGoToDialog : public CCommonDialog
{
protected:
    int* pRecord;
    int RecordCount;

public:
    CGoToDialog(HWND hParent, int* record, int recordCount);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CCSVOptionsDialog
//

struct CCSVConfig;

class CCSVOptionsDialog : public CCommonDialog
{
private:
    CCSVConfig* CfgCSV;
    CCSVConfig* DefaultCfgCSV;

public:
    CCSVOptionsDialog(HWND hParent, CCSVConfig* cfgCSV, CCSVConfig* defaultCfgCSV);

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    void MyTransfer(CTransferInfo& ti, CCSVConfig* cfg);
    void EnableControls();
};

//****************************************************************************
//
// CPropertiesDialog
//

class CPropertiesDialog : public CCommonDialog
{
protected:
    CRendererWindow* Renderer;

public:
    CPropertiesDialog(HWND hParent, CRendererWindow* renderer);

    virtual void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CConfigurationDialog
//

class CConfigurationDialog : public CCommonDialog
{
protected:
    BOOL bEnableCSVOptions;
    HFONT HFont;
    BOOL UseCustomFont;
    LOGFONT LogFont;

public:
    CConfigurationDialog(HWND hParent, BOOL enableCSVOptions);
    ~CConfigurationDialog();

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void SetFontText(); // na zaklade aktualniho fontu nastavi text v okenku
};

//****************************************************************************
//
// CColumnsDialog
//

class CColumnsDialog : public CCommonDialog
{
protected:
    CRendererWindow* Renderer;
    HWND HListView;
    BOOL DisableNotification;
    TDirectArray<CDatabaseColumn> MyColumns; // kopie sloupcu pro ucely konfigurace
    DWORD MinDlgW, MinDlgH, PrevDlgW, PrevDlgH;

public:
    CColumnsDialog(HWND hParent, CRendererWindow* renderer);

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnMove(BOOL up);

    void RecalcLayout(DWORD NewDlgW, DWORD NewDlgH);

    void SetLVTexts(int index); // podle MyColumns nastavi sloupce v LV
};

// ****************************************************************************

#define FIND_TEXT_LEN 201 // +1

class CFindDialog : public CCommonDialog
{
public:
    int Forward, // forward/backward (1/0)
        WholeWords,
        CaseSensitive,
        Regular;

    char Text[FIND_TEXT_LEN];

    CFindDialog(HINSTANCE hInstance, int resID, int helpID)
        : CCommonDialog(hInstance, resID, helpID, NULL)
    {
        Forward = TRUE;
        WholeWords = FALSE;
        CaseSensitive = FALSE;
        Regular = FALSE;
        Text[0] = 0;
    }

    CFindDialog& operator=(CFindDialog& d)
    {
        Forward = d.Forward;
        WholeWords = d.WholeWords;
        CaseSensitive = d.CaseSensitive;
        Regular = d.Regular;
        memcpy(Text, d.Text, sizeof(Text[0]) * FIND_TEXT_LEN);
        return *this;
    }

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
