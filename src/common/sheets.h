// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************

class CPropertyDialog;
class CTreePropHolderDlg;

struct CElasticLayoutCtrl
{
    HWND HCtrl; // handl prvku, ktery mame posouvat
    POINT Pos;  // pozice prvku vuci spodni hrane opsane obalky
};

// pomocna trida slouzici pro layout prvku dialogu na zaklade jeho velikosti
class CElasticLayout
{
public:
    CElasticLayout(HWND hWindow);
    void AddResizeCtrl(int resID);
    // provede rozmisteni prvku
    void LayoutCtrls();

protected:
    static BOOL CALLBACK FindMoveControls(HWND hChild, LPARAM lParam);

    void FindMoveCtrls();

protected:
    // handle dialogu, jehoz layout zajistujeme
    HWND HWindow;
    // delici linka, od ktere jiz prvky posouvame (lezi na spodni hrane ResizeCntrls)
    // client souradnice v bodech
    int SplitY;
    // prvky ktere s velikosti natahujeme (typicky listview)
    TDirectArray<CElasticLayoutCtrl> ResizeCtrls;
    // docasne pole plnene z FindMoveCtrls; idealne by slo o lokalni promennou, ale
    // pro pohodlne volani callbacku FindMoveControls (kam ho potrebujeme predat)
    // ho umistuji jako atribut tridy
    TDirectArray<CElasticLayoutCtrl> MoveCtrls;
};

class CPropSheetPage : protected CDialog
{
public:
    CDialog::SetObjectOrigin; // zpristupneni povolenych metod predku
    CDialog::Transfer;
    CDialog::HWindow; // HWindow zustane take pristupne

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

    CPropertyDialog* ParentDialog; // vlastnik teto stranky
    // pro TreeDialog
    CPropSheetPage* ParentPage;
    HTREEITEM HTreeItem;
    BOOL* Expanded;

    // pokud je ruzne od NULL, se zmenou velikosti dialogu menime layout prvku
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
    HWND Parent; // parametry pro vytvareni dialogu
    HWND HWindow;
    HINSTANCE Modul;
    HICON Icon;
    const TCHAR* Caption;
    int StartPage;
    DWORD Flags;
    PFNPROPSHEETCALLBACK Callback;

    DWORD* LastPage; // posledni zvolena stranka (muze byt NULL, pokud nezajima)

    friend class CPropSheetPage;
};

// ****************************************************************************

class CTreePropDialog;

// sedivy stinovany pruh nad property sheet v tree variante PropertyDialog,
// kde je zobrazen nazev aktualni stranky
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

// na grip controlu chceme pouze top-down kurzor
class CTPHGripWindow : public CWindow
{
public:
    CTPHGripWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID){};

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// dialog, ktery drzi treeview, stinovany nadpis a aktualni property sheet
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
    int ExitButton; // idcko tlacitka, ktere ukoncilo dialog

    // rozmery v bodech
    SIZE MinWindowSize;  // minimalni rozmery dialogu (urcene podle nejvetsiho child dlg)
    DWORD* WindowHeight; // aktualni vyska dialogu
    int TreeWidth;       // sirka treeview, pocitana na zaklade obsahu
    int CaptionHeight;   // vyska titulku
    SIZE ButtonSize;     // rozmery tlacitek na spodni hrane dialogu
    int ButtonMargin;    // mezera mezi tlacitky
    SIZE GripSize;       // rozmery resize gripu v pravem spodnim rohu dialogu
    SIZE MarginSize;     // vodorovny a svisly okraj

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

// datovy drzak stranek pro tree verzi PropertyDialog
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

    // pouze pro forwarding zprav z CTreePropHolderDlg
    virtual void DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam){};
};
