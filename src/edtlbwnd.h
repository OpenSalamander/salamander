// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define EDTLBN_FIRST (0U - 3050U)
// chodi struktura EDTLB_DISPINFO
#define EDTLBN_GETDISPINFO (EDTLBN_FIRST - 1U)
//#define EDTLBN_MOVEITEM       (EDTLBN_FIRST - 2U)
#define EDTLBN_DELETEITEM (EDTLBN_FIRST - 3U)
#define EDTLBN_CONTEXTMENU (EDTLBN_FIRST - 4U)
#define EDTLBN_ENABLECOMMANDS (EDTLBN_FIRST - 6U)
#define EDTLBN_MOVEITEM2 (EDTLBN_FIRST - 7U)
#define EDTLBN_ICONCLICKED (EDTLBN_FIRST - 8U)

// itemID jsou povazovany za indexy a jsou tak obsluhovany pri insertu, movu a deletu
#define ELB_ITEMINDEXES 0x00000001
#define ELB_RIGHTARROW 0x00000002
#define ELB_ENABLECOMMANDS 0x00000004   // kontrol bude zasilat EDTLBN_ENABLECOMMANDS
#define ELB_SHOWICON 0x00000008         // kazda polozka ma ikonu
#define ELB_SPACEASICONCLICK 0x00000010 // klavesa space nageneruje EDTLBN_ICONCLICKED

enum CEdtLBEnum
{
    edtlbGetData,
    edtlbSetData,
};

typedef struct
{
    NMHDR Hdr;
    CEdtLBEnum ToDo;
    char* Buffer;
    HICON HIcon;
    int Index;
    int BufferLen;
    INT_PTR ItemID;
    BOOL Bold;    // ma se text vypsat tucne?
                  //  BOOL          Up;            // pro EDTLBN_MOVEITEM
    HWND HEdit;   // pro EDTLBN_CONTEXTMENU
    POINT Point;  // pro EDTLBN_CONTEXTMENU
    BYTE Enable;  // pro EDTLBN_ENABLECOMMANDS, obsahuje TLBHDRMASK_xxx
    int NewIndex; // pro EDTLBN_MOVEITEM2
} EDTLB_DISPINFO;

//******************************************************************************
//
// CEditListBox
//

class CEditListBox;

class CEditLBEdit : public CWindow
{
protected:
    CEditListBox* EditLB;

public:
    CEditLBEdit(CEditListBox* editLB);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CEditListBox;
};

class CToolbarHeader;

class CEditListBox : public CWindow
{
protected:
    CToolbarHeader* Header;
    CEditLBEdit* EditLine;
    HWND HDlg;
    EDTLB_DISPINFO DispInfo;
    char Buffer[MAX_PATH];
    int ItemsCount;
    BOOL SaveDisabled;
    DWORD Flags;
    int DragAnchorIndex;
    HFONT HNormalFont;
    HFONT HBoldFont;

    RECT ButtonRect;
    BOOL ButtonPressed;
    BOOL ButtonDrag;

    //    DWORD          DragNotify;

    BOOL WaitForDrag; // po WM_LBUTTONDOWN; je nastaven DragAnchor
    POINT DragAnchor; // kde doslo k WM_LBUTTONDOWN (client souradnice)
    BOOL Dragging;    // doslo k utrhnuti po WaitForDrag
    BOOL SelChanged;  // pri WM_LBUTTONDOWN doslo ke zmene polozky
                      //    BOOL           MovedDuringDrag;

    HWND HMarkWindow; // pokud je ruzny od NULL, je povolen d&d

public:
    // je-li nastavena promenna itemIndexes na TRUE, jsou itemID povazovany za
    // indexy a jsou tak obsluhovany pri insertu, movu a deletu
    CEditListBox(HWND hDlg, int ctrlID, DWORD flags = 0, CObjectOrigin origin = ooAllocated);
    ~CEditListBox();

    // prida polozku a vrati jeji index
    int AddItem(INT_PTR itemID = 0);
    // vlozi polozku a vrati jeji index
    int InsertItem(INT_PTR itemID, int index);
    BOOL SetItemData(INT_PTR itemID = 0);
    // smaze polozku na urcitem indexu
    BOOL DeleteItem(int index);
    // smaze vsechny polozky
    void DeleteAllItems();

    BOOL SetCurSel(int index);

    // vraci index vybrane polozky - pro novou polozku je o 1 vetsi, nez
    // pocet polozek
    BOOL GetCurSel(int& index);
    // vraci -1 pro prazdnou polozku
    BOOL GetCurSelItemID(INT_PTR& itemID);
    BOOL GetItemID(int index, INT_PTR& itemID);

    // zmeni ID dane polozky
    BOOL SetItemID(int index, INT_PTR itemID);

    int GetCount() { return ItemsCount; }

    // zmeni static tak, aby byl nad celym oknem a prid do nej toolbaru
    BOOL MakeHeader(int ctrlID);

    //    BOOL OnWMNotify(LPARAM lParam, LRESULT &result);
    void OnSelChanged();
    void OnDrawItem(LPARAM lParam);

    void OnNew();
    void OnDelete();
    void OnMoveUp();
    void OnMoveDown();
    void MoveItem(int newIndex);

    // vrati zda jsou povolene jednotlive commandy (umi se doptat parenta)
    BYTE GetEnabler();

    void RedrawFocusedItem();

    // zacneme editovat aktualni polozku
    void OnBeginEdit(int start = 0, int end = -1);
    void OnEndEdit();
    BOOL OnSaveEdit(); // vrati TRUE, pokud to uzivatel povoli

    // namaluje tlacitko za editline
    void PaintButton();
    void OnPressButton();

    // povili drag&drop polozek, 'hMarkWindow' je okno, kam bude vykreslovana znacka dopadu d&d
    // pokud je 'hMarkWindow' nastaveno na NULL, je d&d zakazan
    void EnableDrag(HWND hMarkWindow);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    int NotifyParent(void* nmhdr, UINT code, BOOL send = TRUE);
    void CommandParent(UINT code);

    friend class CEditLBEdit;
};
