// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************

class CPropertyDialog;
class CTreePropHolderDlg;

struct CElasticLayoutCtrl
{
    HWND HCtrl; // handle of the control to move
    POINT Pos;  // control position relative to the bottom edge of the bounding box
};

// helper class for laying out dialog controls based on the dialog size
class CElasticLayout
{
public:
    CElasticLayout(HWND hWindow);
    void AddResizeCtrl(int resID);
    // lays out the controls
    void LayoutCtrls();

protected:
    static BOOL CALLBACK FindMoveControls(HWND hChild, LPARAM lParam);

    void FindMoveCtrls();

protected:
    // handle of the dialog whose layout we manage
    HWND HWindow;
    // divider line from which controls start moving (lies on the bottom edge of ResizeCtrls)
    // client coordinates, in pixels
    int SplitY;
    // controls that resize with the dialog (typically list views)
    TDirectArray<CElasticLayoutCtrl> ResizeCtrls;
    // temporary array populated by FindMoveCtrls; ideally this would be a local variable, but
    // for convenient calls to the FindMoveControls callback (to which it must be passed)
    // it is stored as a class member
    TDirectArray<CElasticLayoutCtrl> MoveCtrls;
};

class CPropSheetPage : protected CDialog
{
public:
    CDialog::SetObjectOrigin; // expose selected base-class methods
    CDialog::Transfer;
    CDialog::HWindow; // HWindow also remains accessible

    CPropSheetPage(const TCHAR* title, HINSTANCE modul, int resID,
                   DWORD flags /* = PSP_USETITLE*/, HICON icon,
                   CObjectOrigin origin = ooStatic);
    CPropSheetPage(const TCHAR* title, HINSTANCE modul, int resID, UINT helpID,
                   DWORD flags /* = PSP_USETITLE*/, HICON icon,
                   CObjectOrigin origin = ooStatic);
    ~CPropSheetPage();

    void Init(const TCHAR* title, HINSTANCE modul, int resID,
              HICON icon, DWORD flags, CObjectOrigin origin);

    virtual BOOL ValidateData();
    virtual BOOL TransferData(CTransferType type);

    HPROPSHEETPAGE CreatePropSheetPage();
    virtual BOOL Is(int type) { return type == otPropSheetPage || CDialog::Is(type); }
    virtual int GetObjectType() { return otPropSheetPage; }
    virtual BOOL IsAllocated() { return ObjectOrigin == ooAllocated; }

    static INT_PTR CALLBACK CPropSheetPageProc(HWND hwndDlg, UINT uMsg,
                                               WPARAM wParam, LPARAM lParam);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL ElasticVerticalLayout(int count, ...);

    TCHAR* Title;
    DWORD Flags;
    HICON Icon;

    CPropertyDialog* ParentDialog; // owner of this page
    // for TreeDialog
    CPropSheetPage* ParentPage;
    HTREEITEM HTreeItem;
    BOOL* Expanded;

    // if not NULL, resizing the dialog also updates the control layout
    CElasticLayout* ElasticLayout;

    friend class CPropertyDialog;
    friend class CTreePropDialog;
    friend class CTreePropHolderDlg;
};

// ****************************************************************************

class CPropertyDialog : public TIndirectArray<CPropSheetPage>
{
public:
    CPropertyDialog(HWND parent, HINSTANCE modul, const TCHAR* caption,
                    int startPage, DWORD flags, HICON icon = NULL,
                    DWORD* lastPage = NULL, PFNPROPSHEETCALLBACK callback = NULL)
        : TIndirectArray<CPropSheetPage>(10, 5, dtNoDelete)
    {
        Parent = parent;
        HWindow = NULL;
        Modul = modul;
        Icon = icon;
        Caption = caption;
        StartPage = startPage;
        Flags = flags;
        LastPage = lastPage;
        Callback = callback;
    }

    virtual INT_PTR Execute();

    virtual int GetCurSel();

protected:
    HWND Parent; // dialog creation parameters
    HWND HWindow;
    HINSTANCE Modul;
    HICON Icon;
    const TCHAR* Caption;
    int StartPage;
    DWORD Flags;
    PFNPROPSHEETCALLBACK Callback;

    DWORD* LastPage; // last selected page (may be NULL if not needed)

    friend class CPropSheetPage;
};

// ****************************************************************************

class CTreePropDialog;

// gray shaded bar above the property sheet in the tree variant of PropertyDialog,
// showing the name of the current page
class CTPHCaptionWindow : protected CWindow
{
protected:
    TCHAR* Text;
    int Allocated;

public:
    CTPHCaptionWindow(HWND hDlg, int ctrlID);
    ~CTPHCaptionWindow();

    void SetText(const TCHAR* text);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// show only the resize cursor over the grip control
class CTPHGripWindow : public CWindow
{
public:
    CTPHGripWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {};

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// dialog that hosts the tree view, shaded caption, and current property sheet
class CTreePropHolderDlg : public CDialog
{
protected:
    CTreePropDialog* TPD;
    HWND HTreeView;
    CTPHCaptionWindow* CaptionWindow;
    CTPHGripWindow* GripWindow;
    RECT ChildDialogRect;
    int CurrentPageIndex;
    CPropSheetPage* ChildDialog;
    int ExitButton; // ID of the button that closed the dialog

    // dimensions in points
    SIZE MinWindowSize;  // minimum dialog size (determined by the largest child dialog)
    DWORD* WindowHeight; // current dialog height
    int TreeWidth;       // tree view width, calculated from the contents
    int CaptionHeight;   // caption height
    SIZE ButtonSize;     // size of the buttons along the bottom edge of the dialog
    int ButtonMargin;    // spacing between buttons
    SIZE GripSize;       // size of the resize grip in the dialog's bottom-right corner
    SIZE MarginSize;     // horizontal and vertical margins

public:
    CTreePropHolderDlg(HWND hParent, DWORD* windowHeight);

    int ExecuteIndirect(LPCDLGTEMPLATE hDialogTemplate);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnCtrlTab(BOOL shift);
    void LayoutControls();
    int BuildAndMeasureTree();
    void EnableButtons();
    BOOL SelectPage(int pageIndex);

    friend class CTreePropDialog;
};

// page data holder for the tree version of PropertyDialog
class CTreePropDialog : public CPropertyDialog
{
protected:
    CTreePropHolderDlg Dialog;

public:
    CTreePropDialog(HWND hParent, HINSTANCE hInstance, TCHAR* caption,
                    int startPage, DWORD flags, DWORD* lastPage,
                    DWORD* windowHeight)
        : CPropertyDialog(hParent, hInstance, caption, startPage, flags, NULL, lastPage),
          Dialog(hParent, windowHeight)
    {
        Dialog.TPD = this;
    }

    virtual int Execute(const TCHAR* buttonOK,
                        const TCHAR* buttonCancel,
                        const TCHAR* buttonHelp);
    virtual int GetCurSel();
    int Add(CPropSheetPage* page, CPropSheetPage* parent = NULL, BOOL* expanded = NULL);

protected:
    WORD* lpdwAlign(WORD* lpIn);
    int AddItemEx(LPWORD& lpw, const TCHAR* className, WORD id, int x, int y, int cx, int cy,
                  UINT style, UINT exStyle, const TCHAR* text);

    //    DLGTEMPLATE *DoLockDlgRes(int page);
    friend class CTreePropHolderDlg;

    // only for forwarding messages from CTreePropHolderDlg
    virtual void DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {};
};
