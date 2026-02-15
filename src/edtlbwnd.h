// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define EDTLBN_FIRST (0U - 3050U)
// the EDTLB_DISPINFO structure is sent
#define EDTLBN_GETDISPINFO (EDTLBN_FIRST - 1U)
//#define EDTLBN_MOVEITEM       (EDTLBN_FIRST - 2U)
#define EDTLBN_DELETEITEM (EDTLBN_FIRST - 3U)
#define EDTLBN_CONTEXTMENU (EDTLBN_FIRST - 4U)
#define EDTLBN_ENABLECOMMANDS (EDTLBN_FIRST - 6U)
#define EDTLBN_MOVEITEM2 (EDTLBN_FIRST - 7U)
#define EDTLBN_ICONCLICKED (EDTLBN_FIRST - 8U)

// itemID values are considered indexes and are handled as such during insert, move and delete
#define ELB_ITEMINDEXES 0x00000001
#define ELB_RIGHTARROW 0x00000002
#define ELB_ENABLECOMMANDS 0x00000004   // the control will send EDTLBN_ENABLECOMMANDS
#define ELB_SHOWICON 0x00000008         // each item has an icon
#define ELB_SPACEASICONCLICK 0x00000010 // the Space key generates EDTLBN_ICONCLICKED

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
    BOOL Bold;    // should the text be printed bold?
                  //  BOOL          Up;            // for EDTLBN_MOVEITEM
    HWND HEdit;   // for EDTLBN_CONTEXTMENU
    POINT Point;  // for EDTLBN_CONTEXTMENU
    BYTE Enable;  // for EDTLBN_ENABLECOMMANDS, contains TLBHDRMASK_xxx
    int NewIndex; // for EDTLBN_MOVEITEM2
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

    BOOL WaitForDrag; // after WM_LBUTTONDOWN DragAnchor is set
    POINT DragAnchor; // where WM_LBUTTONDOWN occurred (client coordinates)
    BOOL Dragging;    // break-away happened after WaitForDrag
    BOOL SelChanged;  // WM_LBUTTONDOWN changed the item
                      //    BOOL           MovedDuringDrag;

    HWND HMarkWindow; // if not NULL, drag&drop is enabled

public:
    // if the itemIndexes variable is set to TRUE, itemID values are treated as
    // indexes and are handled as such when inserting, moving and deleting
    CEditListBox(HWND hDlg, int ctrlID, DWORD flags = 0, CObjectOrigin origin = ooAllocated);
    ~CEditListBox();

    // adds an item and returns its index
    int AddItem(INT_PTR itemID = 0);
    // inserts an item and returns its index
    int InsertItem(INT_PTR itemID, int index);
    BOOL SetItemData(INT_PTR itemID = 0);
    // deletes the item at the specified index
    BOOL DeleteItem(int index);
    // deletes all items
    void DeleteAllItems();

    BOOL SetCurSel(int index);

    // returns index of the selected item - for a new item it is one more than
    // the number of items
    BOOL GetCurSel(int& index);
    // returns -1 for an empty item
    BOOL GetCurSelItemID(INT_PTR& itemID);
    BOOL GetItemID(int index, INT_PTR& itemID);

    // changes the ID of the specified item
    BOOL SetItemID(int index, INT_PTR itemID);

    int GetCount() { return ItemsCount; }

    // modifies the static control so it covers the entire window and adds a toolbar to it
    BOOL MakeHeader(int ctrlID);

    //    BOOL OnWMNotify(LPARAM lParam, LRESULT &result);
    void OnSelChanged();
    void OnDrawItem(LPARAM lParam);

    void OnNew();
    void OnDelete();
    void OnMoveUp();
    void OnMoveDown();
    void MoveItem(int newIndex);

    // returns which commands are enabled (can query the parent)
    BYTE GetEnabler();

    void RedrawFocusedItem();

    // start editing the current item
    void OnBeginEdit(int start = 0, int end = -1);
    void OnEndEdit();
    BOOL OnSaveEdit(); // returns TRUE if the user allows it

    // draws the button after the edit line
    void PaintButton();
    void OnPressButton();

    // enables drag&drop of items; 'hMarkWindow' is the window where the drop marker will be drawn
    // if 'hMarkWindow' is set to NULL, drag&drop is disabled
    void EnableDrag(HWND hMarkWindow);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    int NotifyParent(void* nmhdr, UINT code, BOOL send = TRUE);
    void CommandParent(UINT code);

    friend class CEditLBEdit;
};
