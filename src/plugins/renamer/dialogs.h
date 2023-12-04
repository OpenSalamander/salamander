// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MNTS_ALL (MNTS_B | MNTS_I | MNTS_A)

#define MAX_HISTORY_ENTRIES 20

#define WM_USER_RELOADSOURCE WM_APP
#define WM_USER_CARETMOVE WM_APP + 1 // informuje o mozne zmene pozice \
                                     // caretu v edit controlu

#define WM_USER_CFGCHNG (WM_APP + 3246)

// [0, 0] - pro otevrena okna vieweru: Salamander pregeneroval fonty, mame zavolat SetFont() listam
#define WM_USER_SETTINGCHANGE (WM_APP + 3248)

// TODO: pouzit
extern BOOL MinBeepWhenDone;

extern char* MaskHistory[MAX_HISTORY_ENTRIES];
extern char* NewNameHistory[MAX_HISTORY_ENTRIES];
extern char* SearchHistory[MAX_HISTORY_ENTRIES];
extern char* ReplaceHistory[MAX_HISTORY_ENTRIES];
extern char* CommandHistory[MAX_HISTORY_ENTRIES];

BOOL InitDialogs();
void ReleaseDialogs();

void DialogStackPush(HWND hWindow);
void DialogStackPop();
HWND DialogStackPeek();
inline HWND GetParent()
{
    HWND ret = DialogStackPeek();
    return ret == (HWND)-1 ? SG->GetMsgBoxParent() : ret;
}

class CDialogStackAutoObject
{
public:
    CDialogStackAutoObject(HWND hWindow) { DialogStackPush(hWindow); }
    ~CDialogStackAutoObject() { DialogStackPop(); }
};

#define PARENT(wnd) CDialogStackAutoObject dsao(wnd);

UINT_PTR CALLBACK ComDlgHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

void HistoryComboBox(CTransferInfo& ti, int id, char* text, int textMax,
                     int historySize, char** history);

void TransferCombo(CTransferInfo& ti, int id, int* comboContent, int& value);

//******************************************************************************
//
// CComboboxEdit
//
// Protoze je combobox vyprasenej, nelze klasickou cestou (CB_GETEDITSEL) zjistit
// po opusteni focusu, jaka byla selection. To resi tento control.
//

class CComboboxEdit : public CWindow
{
protected:
    DWORD SelStart;
    DWORD SelEnd;
    BOOL SetSelOnFocus;
    BOOL HelperButton;
    BOOL SkipCharacter;

public:
    CComboboxEdit(BOOL helperButton = FALSE);

    void GetSel(DWORD* start, DWORD* end);
    void SetSel(DWORD start, DWORD end);

    void ReplaceText(const char* text);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//******************************************************************************
//
// CNotifyEdit
//
// Posila WM_USER_KEYDOWN pri stisku klavesy nebo mysiho tlacitka
//

class CNotifyEdit : public CWindow
{
protected:
    BOOL SkipCharacter;

public:
    CNotifyEdit();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CAddCutDialog
//

class CAddCutDialog : public CDialog
{
    const char** CutFrom;
    int *Left, *LeftSign, *Right, *RightSign;

public:
    CAddCutDialog(HWND parent, const char** cutFrom,
                  int* left, int* leftSign, int* right, int* rightSign)
        : CDialog(HLanguage, IDD_CUT, parent)
    {
        CutFrom = cutFrom;
        Left = left;
        Right = right;
        LeftSign = leftSign;
        RightSign = rightSign;
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
};

// ****************************************************************************
//
// CAddCounterDialog
//

class CAddCounterDialog : public CDialog
{
    char* Buffer;

public:
    CAddCounterDialog(HWND parent, char* buffer)
        : CDialog(HLanguage, IDD_COUNTER, parent)
    {
        Buffer = buffer;
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
};

// ****************************************************************************
//
// CDefineClassDialog
//

class CDefineClassDialog : public CDialog
{
    char* Buffer;

public:
    CDefineClassDialog(HWND parent, char* buffer)
        : CDialog(HLanguage, IDD_DEFINESET, parent)
    {
        Buffer = buffer;
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
};

// ****************************************************************************
//
// CSubexpressionDialog
//

class CSubexpressionDialog : public CDialog
{
    int* Number;
    CChangeCase* Case;

public:
    CSubexpressionDialog(HWND parent, int* number, CChangeCase* changeCase)
        : CDialog(HLanguage, IDD_SUBEXPRESSION, parent)
    {
        Number = number;
        Case = changeCase;
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
};

// ****************************************************************************
//
// CCommandDialog
//

class CCommandDialog : public CDialog
{
    char* Command;
    CChangeCase* Case;

public:
    CCommandDialog(HWND parent, char* command)
        : CDialog(HLanguage, IDD_COMMAND, parent)
    {
        Command = command;
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
};

// ****************************************************************************
//
// CCommandErrorDialog
//

class CCommandErrorDialog : public CDialog
{
    const char* Command;
    DWORD ExitCode;
    const char* StdError;

public:
    CCommandErrorDialog(HWND parent, const char* command, DWORD exitCode,
                        const char* stdError)
        : CDialog(HLanguage, IDD_ERROR, parent)
    {
        Command = command;
        ExitCode = exitCode;
        StdError = stdError;
    }

    virtual void Transfer(CTransferInfo& ti);
};

// ****************************************************************************
//
// CProgressDialog
//

class CProgressDialog : public CDialog
{
    CGUIProgressBarAbstract* Progress;
    BOOL Cancel;
    DWORD NextUpdate;
    DWORD Current;

public:
    CProgressDialog(HWND parent) : CDialog(HLanguage, IDD_PROGRESS, parent) { Cancel = FALSE; }
    virtual ~CProgressDialog() { ; }

    BOOL Update(DWORD current, BOOL force = FALSE);
    void CancelOperation();
    void SetText(const char* text);
    void EmptyMessageLoop();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CConfigDialog
//

class CConfigDialog : public CDialog
{
protected:
public:
    CConfigDialog(HWND parent) : CDialog(HLanguage, IDD_CONFIG, IDD_CONFIG, parent)
    {
        ;
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
