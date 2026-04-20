// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//*****************************************************************************
//
// CToolBarItem
//

class CToolBar;
class CTBCustomizeDialog;

class CToolBarItem
{
protected:
    DWORD Style;    // TLBI_STYLE_xxx
    DWORD State;    // TLBI_STATE_xxx
    DWORD ID;       // command id
    char* Text;     // allocated string
    int TextLen;    // length of string
    int ImageIndex; // Image index of the item. Set this member to -1 to
                    // indicate that the button does not have an image.
                    // The button layout will not include any space for
                    // a bitmap, only text.
    HICON HIcon;
    HICON HOverlay;
    DWORD CustomData; // FIXME_X64 - too small for a pointer; do we ever need it?
    int Width;        // Width of the item (computed if TLBI_STYLE_AUTOSIZE is set)

    char* Name; // Name shown in the Customize dialog (valid only during the customization session).

    // These values are used for optimized access to item states.
    DWORD* Enabler; // Points to the variable that controls the item state.
                    // A non-zero value keeps TLBI_STATE_GRAYED cleared.
                    // Zero means TLBI_STATE_GRAYED is set.

    // internal data
    int Height; // Item height.
    int Offset; // Item offset within the entire toolbar.

    WORD IconX; // Horizontal offsets for the individual elements.
    WORD TextX;
    WORD InnerX;
    WORD OutterX;

public:
    CToolBarItem();
    ~CToolBarItem();

    BOOL SetText(const char* text, int len = -1);

    friend class CToolBar;
    friend class CTBCustomizeDialog;
};

//*****************************************************************************
//
// CToolBar
//

class CBitmap;

class CToolBar : public CWindow, public CGUIToolBarAbstract
{
protected:
    TIndirectArray<CToolBarItem> Items;

    int Width; // Width of the entire window.
    int Height;
    HFONT HFont;
    int FontHeight;
    HWND HNotifyWindow; // Target window for notifications.
    HIMAGELIST HImageList;
    HIMAGELIST HHotImageList;
    int ImageWidth; // Dimensions of a single image from the image list.
    int ImageHeight;
    DWORD Style;          // TLB_STYLE_xxx
    BOOL DirtyItems;      // TRUE when an operation affects item layout and a recalculation is required.
    CBitmap* CacheBitmap; // Off-screen bitmap we render into.
    CBitmap* MonoBitmap;  // Used to render grayed icons.
    int CacheWidth;       // Width of the cache bitmap.
    int CacheHeight;      // Height of the cache bitmap.
    int HotIndex;         // -1 = none.
    int DownIndex;
    BOOL DropPressed;
    BOOL MonitorCapture;
    BOOL RelayToolTip;
    TOOLBAR_PADDING Padding;
    BOOL HasIcon;       // TRUE when an icon is stored, so GetNeededSpace() must account for its height.
    BOOL HasIconDirty;  // TRUE when icon presence must be re-detected for GetNeededSpace().
    BOOL Customizing;   // Is the toolbar currently being customized?
    int InserMarkIndex; // -1 = none.
    BOOL InserMarkAfter;
    BOOL MouseIsTracked;  // Is the mouse being tracked via TrackMouseEvent?
    DWORD DropDownUpTime; // Timestamp in milliseconds when the drop-down was released to avoid immediate re-pressing.
    BOOL HelpMode;        // Salamander is in Shift+F1 (context help) mode; highlight disabled items under the cursor.

public:
    //
    // Custom methods
    //
    CToolBar(HWND hNotifyWindow, CObjectOrigin origin = ooAllocated);
    ~CToolBar();

    //
    // Implementation of CGUIToolBarAbstract methods
    //

    virtual BOOL WINAPI CreateWnd(HWND hParent);
    virtual HWND WINAPI GetHWND() { return HWindow; }

    virtual int WINAPI GetNeededWidth(); // Returns the width the window requires.
    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI SetFont();
    virtual BOOL WINAPI GetItemRect(int index, RECT& r); // Returns the item's rectangle in screen coordinates.

    virtual BOOL WINAPI CheckItem(DWORD position, BOOL byPosition, BOOL checked);
    virtual BOOL WINAPI EnableItem(DWORD position, BOOL byPosition, BOOL enabled);

    // If an image list is assigned, inserts the icon at the corresponding position.
    // The 'normal' and 'hot' flags determine which image lists are affected.
    virtual BOOL WINAPI ReplaceImage(DWORD position, BOOL byPosition, HICON hIcon, BOOL normal = TRUE, BOOL hot = FALSE);

    virtual int WINAPI FindItemPosition(DWORD id);

    virtual void WINAPI SetImageList(HIMAGELIST hImageList);
    virtual HIMAGELIST WINAPI GetImageList();

    virtual void WINAPI SetHotImageList(HIMAGELIST hImageList);
    virtual HIMAGELIST WINAPI GetHotImageList();

    // Toolbar style
    virtual void WINAPI SetStyle(DWORD style);
    virtual DWORD WINAPI GetStyle();

    virtual BOOL WINAPI RemoveItem(DWORD position, BOOL byPosition);
    virtual void WINAPI RemoveAllItems();

    virtual int WINAPI GetItemCount() { return Items.Count; }

    // Opens the Customize dialog.
    virtual void WINAPI Customize();

    virtual void WINAPI SetPadding(const TOOLBAR_PADDING* padding);
    virtual void WINAPI GetPadding(TOOLBAR_PADDING* padding);

    // Walks all items and, when 'EnablerData' is set, compares the referenced value
    // with the actual item state; updates the state when they differ.
    virtual void WINAPI UpdateItemsState();

    // If the point is above an item (not a separator), returns its index;
    // otherwise returns a negative value.
    virtual int WINAPI HitTest(int xPos, int yPos);

    // Returns TRUE if the point lies on an item boundary; in that case, it sets 'index'
    // to that item and the 'after' flag to indicate whether the insertion mark is before or
    // after it. Returns FALSE if the point lies over an item. If the point does not lie over
    // any item, it returns TRUE and sets 'index' to -1.
    virtual BOOL WINAPI InsertMarkHitTest(int xPos, int yPos, int& index, BOOL& after);

    // Sets the insert mark to the given index (before or after).
    // If index == -1, removes the insert mark.
    virtual void WINAPI SetInsertMark(int index, BOOL after);

    // Sets the hot item in a toolbar. Returns the index of the previous hot item, or -1 if there was no hot item.
    virtual int WINAPI SetHotItem(int index);

    // Rebuild CacheBitmap when the display color depth changes.
    virtual void WINAPI OnColorsChanged();

    virtual BOOL WINAPI InsertItem2(DWORD position, BOOL byPosition, const TLBI_ITEM_INFO2* tii);
    virtual BOOL WINAPI SetItemInfo2(DWORD position, BOOL byPosition, const TLBI_ITEM_INFO2* tii);
    virtual BOOL WINAPI GetItemInfo2(DWORD position, BOOL byPosition, TLBI_ITEM_INFO2* tii);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DrawDropDown(HDC hDC, int x, int y, BOOL grayed);
    void DrawItem(int index);
    void DrawItem(HDC hDC, int index);
    void DrawAllItems(HDC hDC);

    void DrawInsertMark(HDC hDC);

    // Returns TRUE if an item is at the position; in that case it also sets 'index'.
    // Otherwise it returns FALSE.
    // If the user clicked the drop-down, it sets 'dropDown' to TRUE.
    BOOL HitTest(int xPos, int yPos, int& index, BOOL& dropDown);

    // Iterates through every item and calculates 'MinWidth' and 'XOffset'.
    // Controlled by (and updates) DirtyItems.
    // Returns TRUE if all items were redrawn.
    BOOL Refresh();

    friend class CTBCustomizeDialog;
};

//*****************************************************************************
//
// CTBCustomizeDialog
//

class CTBCustomizeDialog : public CCommonDialog
{
    enum TBCDDragMode
    {
        tbcdDragNone,
        tbcdDragAvailable,
        tbcdDragCurrent,
    };

protected:
    TDirectArray<TLBI_ITEM_INFO2> AllItems; // All available items.
    CToolBar* ToolBar;
    HWND HAvailableLB;
    HWND HCurrentLB;
    DWORD DragNotify;
    TBCDDragMode DragMode;
    int DragIndex;

public:
    CTBCustomizeDialog(CToolBar* toolBar);
    ~CTBCustomizeDialog();
    BOOL Execute();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DestroyItems();
    BOOL EnumButtons(); // Uses the WM_USER_TBENUMBUTTON2 notification to list every button the toolbar can host.

    BOOL PresentInToolBar(DWORD id);      // Is this command present in the toolbar?
    BOOL FindIndex(DWORD id, int* index); // Finds the command in AllItems.
    void FillLists();                     // Populates both list boxes.

    void EnableControls();
    void MoveItem(int srcIndex, int tgtIndex);
    void OnAdd();
    void OnRemove();
    void OnUp();
    void OnDown();
    void OnReset();
};

//*****************************************************************************
//
// CMainToolBar
//
// Configurable toolbar that holds command buttons. It sits at the top of Salamander,
// above each panel.
//

enum CMainToolBarType
{
    mtbtTop,
    mtbtMiddle,
    mtbtLeft,
    mtbtRight,
};

class CMainToolBar : public CToolBar
{
protected:
    CMainToolBarType Type;

public:
    CMainToolBar(HWND hNotifyWindow, CMainToolBarType type, CObjectOrigin origin = ooStatic);

    BOOL Load(const char* data);
    BOOL Save(char* data);

    // Must return a tooltip.
    void OnGetToolTip(LPARAM lParam);
    // Supplies the customization dialog with items while customizing.
    BOOL OnEnumButton(LPARAM lParam);
    // The user pressed Reset in the configuration dialog—load the default layout.
    void OnReset();

    void SetType(CMainToolBarType type);

protected:
    // Fills 'tii' with data for the 'tbbeIndex' item and returns TRUE.
    // Returns FALSE when the item is incomplete (command removed).
    BOOL FillTII(int tbbeIndex, TLBI_ITEM_INFO2* tii, BOOL fillName); // 'buttonIndex' belongs to the TBBE_xxxx family; -1 = separator.
};

//*****************************************************************************
//
// CBottomToolBar
//
// Toolbar at the bottom of Salamander—shows help for F1-F12 in
// combination with Ctrl, Alt, and Shift.
//

enum CBottomTBStateEnum
{
    btbsNormal,
    btbsAlt,
    btbsCtrl,
    btbsShift,
    btbsCtrlShift,
    //  btbsCtrlAlt,
    btbsAltShift,
    //  btbsCtrlAltShift,
    btbsMenu,
    btbsCount
};

class CBottomToolBar : public CToolBar
{
public:
    CBottomToolBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);

    virtual BOOL WINAPI CreateWnd(HWND hParent);

    // Called whenever modifier keys (Ctrl, Alt, Shift) change—iterates the populated
    // toolbar and updates its texts and IDs.
    BOOL SetState(CBottomTBStateEnum state);

    // Initializes the static array used to feed the toolbar.
    static BOOL InitDataFromResources();

    void OnGetToolTip(LPARAM lParam);

    virtual void WINAPI SetFont();

protected:
    CBottomTBStateEnum State;

    // Internal helper called from InitDataFromResources.
    static BOOL InitDataResRow(CBottomTBStateEnum state, int textResID);

    // Finds the longest text for each button and sets the button width accordingly.
    BOOL SetMaxItemWidths();
};

//*****************************************************************************
//
// CUserMenuBar
//

class CUserMenuBar : public CToolBar
{
public:
    CUserMenuBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);

    // Retrieves items from the UserMenu and populates the toolbar with buttons.
    BOOL CreateButtons();

    void ToggleLabels();
    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI Customize();

    virtual void WINAPI SetInsertMark(int index, BOOL after);
    virtual int WINAPI SetHotItem(int index);

    void OnGetToolTip(LPARAM lParam);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CHotPathsBar
//

class CHotPathsBar : public CToolBar
{
public:
    CHotPathsBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);

    // Retrieves items from HotPaths and populates the toolbar with buttons.
    BOOL CreateButtons();

    void ToggleLabels();
    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI Customize();

    //    void SetInsertMark(int index, BOOL after);
    //    int SetHotItem(int index);

    void OnGetToolTip(LPARAM lParam);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CDriveBar
//

class CDrivesList;

class CDriveBar : public CToolBar
{
protected:
    // Return values for List.
    DWORD DriveType;
    DWORD_PTR DriveTypeParam;
    int PostCmd;
    void* PostCmdParam;
    BOOL FromContextMenu;
    CDrivesList* List;

    // Cache: stores ?:, \\ for UNC, or an empty string.
    char CheckedDrive[3];

public:
    // Plugin icons are kept in image lists so we can display them in grayscale.
    HIMAGELIST HDrivesIcons;
    HIMAGELIST HDrivesIconsGray;

public:
    CDriveBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);
    ~CDriveBar();

    void DestroyImageLists();

    // Removes existing buttons and loads new ones.
    // If 'copyDrivesListFrom' is not NULL, copy its drive data instead of querying again.
    // 'copyDrivesListFrom' may even point to the called object itself.
    BOOL CreateDriveButtons(CDriveBar* copyDrivesListFrom);

    virtual int WINAPI GetNeededHeight();

    void OnGetToolTip(LPARAM lParam);

    // The user clicked the button with the given command id.
    void Execute(DWORD id);

    // Depresses the icon that corresponds to the path; if none matches, all buttons are released.
    // The 'force' argument invalidates the cache.
    void SetCheckedDrive(CFilesWindow* panel, BOOL force = FALSE);

    // Rebuilds the list after a drive-add or drive-remove notification.
    // If 'copyDrivesListFrom' is not NULL, copy its drive data instead of querying again.
    // 'copyDrivesListFrom' may even point to the called object itself.
    void RebuildDrives(CDriveBar* copyDrivesListFrom = NULL);

    // Displays the context menu; the item is determined by GetMessagePos.
    // Returns TRUE if a button was hit and the menu was shown; otherwise returns FALSE.
    BOOL OnContextMenu();

    // Returns the bit mask of drives obtained by the last List->BuildData().
    // Returns 0 if BuildData() has not run yet.
    // Can be used to quickly detect whether any drive changed.
    DWORD GetCachedDrivesMask();

    // Returns the bit mask of available cloud storages obtained by the last List->BuildData().
    // Returns 0 if BuildData() has not run yet.
    // Can be used to quickly detect whether cloud storage availability changed.
    DWORD GetCachedCloudStoragesMask();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CPluginsBar
//

class CPluginsBar : public CToolBar
{
protected:
    // Icons representing plugins, created by CPlugins::CreateIconsList.
    HIMAGELIST HPluginsIcons;
    HIMAGELIST HPluginsIconsGray;

public:
    CPluginsBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);
    ~CPluginsBar();

    void DestroyImageLists();

    // Removes existing buttons and loads new ones.
    BOOL CreatePluginButtons();

    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI Customize();

    void OnGetToolTip(LPARAM lParam);

    //  protected:
    //    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern void PrepareToolTipText(char* buff, BOOL stripHotKey);

extern void GetSVGIconsMainToolbar(CSVGIcon** svgIcons, int* svgIconsCount);
