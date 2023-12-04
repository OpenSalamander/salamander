// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

void CenterWindowToWindow(HWND hWnd, HWND hBaseWnd);

class CLayoutEditor;

//*****************************************************************************
//
// horizontalne i vertikalne centrovany dialog k hlavnimu oknu
// predek vsech dialogu
//
// pokud je 'centerToWindow' rovno TRUE, je centrovany k hlavnimu oknu
// jinak je centrovany k obrazovce
//

class CCommonDialog : public CDialog
{
protected:
    BOOL CenterToWindow;

public:
    CCommonDialog(HINSTANCE modul, int resID, HWND parent,
                  CObjectOrigin origin = ooStandard, BOOL centerToWindow = TRUE)
        : CDialog(modul, resID, parent, origin)
    {
        CenterToWindow = centerToWindow;
    }

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CNewDialog
//

class CImportMUIDialog : public CCommonDialog
{
public:
    CImportMUIDialog(HWND parent);

    //    virtual void Validate(CTransferInfo &ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CNewDialog
//
/*
class CNewDialog: public CCommonDialog
{
  public:
    char ProjectFile[MAX_PATH];
    char SourceFile[MAX_PATH];
    char TargetFile[MAX_PATH];
    char IncludeFile[MAX_PATH];

  public:
    CNewDialog(HWND parent);

    virtual void Validate(CTransferInfo &ti);
    virtual void Transfer(CTransferInfo &ti);

  protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/
//*****************************************************************************
//
// CPropertiesDialog
//

class CLocales
{
public:
    CLocales();

    void EnumLocales();

    void FillComboBox(HWND hCombo);

public:
    TDirectArray<WORD> Items;
};

extern CLocales Locales;

class CPropertiesDialog : public CCommonDialog
{
public:
    CPropertiesDialog(HWND parent);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CFindDialog
//

class CFindDialog : public CCommonDialog
{
public:
    CFindDialog(HWND parent);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//*****************************************************************************
//
// COptionsDialog
//

class COptionsDialog : public CCommonDialog
{
public:
    COptionsDialog(HWND parent);

    virtual void Transfer(CTransferInfo& ti);
};

//*****************************************************************************
//
// CImportDialog
//

class CImportDialog : public CCommonDialog
{
protected:
    char* Project;

public:
    CImportDialog(HWND parent, char* project);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CValidateDialog
//

class CValidateDialog : public CCommonDialog
{
public:
    CValidateDialog(HWND parent);

    void EnableControls();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CAgreementDialog
//

class CAgreementDialog : public CCommonDialog
{
protected:
    char* Agreement;

public:
    CAgreementDialog(HWND parent);
    ~CAgreementDialog();

    void EnableControls();

    BOOL HasAgreement() { return Agreement != NULL; }

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CAlignToDialog
//

class CAlignToDialog : public CDialog
{
protected:
    HWND HEditedDlg;
    CAlignToParams* Params;
    CLayoutEditor* LayoutEditor;

public:
    CAlignToDialog(HWND parent, HWND hEditedDlg, CAlignToParams* params, CLayoutEditor* layoutEditor);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableChildWindow(int id, BOOL enabled);
    void EnableControls();
    void OnChange();
};

//*****************************************************************************
//
// CSetSizeDialog
//

class CSetSizeDialog : public CDialog
{
protected:
    HWND HEditedDlg;
    CLayoutEditor* LayoutEditor;
    int* Width;
    int* Height;

public:
    CSetSizeDialog(HWND parent, HWND hEditedDlg, CLayoutEditor* layoutEditor, int* width, int* height);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual BOOL DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableChildWindow(int id, BOOL enabled);
    void EnableControls();
};
