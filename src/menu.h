// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

extern const char* WC_POPUPMENU;

#define UPDOWN_ARROW_WIDTH 9
#define UPDOWN_ARROW_HEIGHT 5
#define UPDOWN_ITEM_HEIGHT 12

class CMenuSharedResources;
class CMenuPopup;
class CMenuBar;
class CBitmap;

/*
Sent messages:
  WM_INITMENUPOPUP
    hmenuPopup = (HMENU) wParam;         // handle to submenu
    uPos = (UINT) LOWORD(lParam);        // submenu item position
    fSystemMenu = (BOOL) HIWORD(lParam); // window menu flag

    This message is sent only for Windows menu popups.

  WM_USER_INITMENUPOPUP
  WM_USER_UNINITMENUPOPUP
    menuPopup = (CGUIMenuPopupAbstract*) wParam; // pointer to submenu
    uPos =      LOWORD(lParam);                // submenu item position
    uID =       HIWORD(lParam);                // submenu ID

    These two messages are always sent, even for menus implemented
    on top of a Windows popup.
*/

//*****************************************************************************
//
// CMenuWindowQueue
//

class CMenuWindowQueue
{
private:
    TDirectArray<HWND> Data;
    CRITICAL_SECTION DataCriticalSection; // critical section for data access
    BOOL UsingData;

public:
    CMenuWindowQueue();
    ~CMenuWindowQueue();

    BOOL Add(HWND hWindow);    // adds an item to the queue, returns success
    void Remove(HWND hWindow); // removes an item from the queue
    void DispatchCloseMenu();  // sends WM_USER_CLOSEMENU to all open menu windows
};

extern CMenuWindowQueue MenuWindowQueue;

//*****************************************************************************
//
// COldMenuHookTlsAllocator
//

class COldMenuHookTlsAllocator
{
public:
    COldMenuHookTlsAllocator();
    ~COldMenuHookTlsAllocator();

    HHOOK HookThread();
    void UnhookThread(HHOOK hOldHookProc);
};

extern COldMenuHookTlsAllocator OldMenuHookTlsAllocator;

//*****************************************************************************
//
// CMenuSharedResources
//
// Only one instance of these resources exists for a given submenu tree.
// They are created for example in the Track function.
// All submenus then simply receive a pointer to these shared resources.
//

class CMenuSharedResources
{
public:
    // colors
    COLORREF NormalBkColor;
    COLORREF SelectedBkColor;
    COLORREF NormalTextColor;
    COLORREF SelectedTextColor;
    COLORREF HilightColor;
    COLORREF GrayTextColor;

    // cache bitmap
    CBitmap* CacheBitmap;
    CBitmap* MonoBitmap;

    // temp DC
    HDC HTempMemDC;  // memory DC for temporary transfers
    HDC HTemp2MemDC; // memory DC for temporary transfers

    // fonts
    HFONT HNormalFont; // continuously selected in HCacheMemoryDC
    HFONT HBoldFont;   // selected only temporarily

    // menu bitmaps
    HBITMAP HMenuBitmaps; // retrieved from the system: order according to CMenuBitmapEnum
    int MenuBitmapWidth;

    // other
    HWND HParent;          // window from which the menu was invoked
    int TextItemHeight;    // item height based on text
    BOOL BitmapsZoom;      // bitmap scaling relative to the original size
    DWORD ChangeTickCount; // GetTickCount value from the time the selected item changed
    POINT LastMouseMove;
    CMenuBar* MenuBar; // MenuBar from which the window was activated; otherwise NULL
    DWORD SkillLevel;  // value determining which items will be displayed
    BOOL HideAccel;    // should accelerators be hidden

    const RECT* ExcludeRect; // this rectangle must not be covered

    HANDLE HCloseEvent; // used to start the message queue

public:
    CMenuSharedResources();
    ~CMenuSharedResources();

    BOOL Create(HWND hParent, int width, int height);
};

//*****************************************************************************
//
// CMenuItem
//

class CMenuItem
{
protected:
    DWORD Type;
    DWORD State;
    DWORD ID;
    CMenuPopup* SubMenu;
    HBITMAP HBmpChecked;
    HBITMAP HBmpUnchecked;
    HBITMAP HBmpItem;
    char* String;
    int ImageIndex;
    HICON HIcon;
    HICON HOverlay;
    ULONG_PTR CustomData;
    DWORD SkillLevel; // MENU_LEVEL_BEGINNER, MENU_LEVEL_INTERMEDIATE, MENU_LEVEL_ADVANCED
    // these values are used for optimized access to item states
    DWORD* Enabler; // Points to a variable controlling the state of the item.
                    // A nonzero value corresponds to the MENU_STATE_GRAYED bit cleared.
                    // Zero corresponds to the MENU_STATE_GRAYED bit set.
    DWORD Flags;    // MENU_FLAG_xxx
    DWORD Temp;     // helper variable for some methods

    // calculated values
    int Height;
    int MinWidth;
    int YOffset;

    const char* ColumnL1; // text of the first column
    int ColumnL1Len;      // number of characters
    int ColumnL1Width;
    int ColumnL1X;
    const char* ColumnL2; // text of the second column (can be NULL)
    int ColumnL2Len;      // number of characters
    int ColumnL2Width;
    int ColumnL2X;
    const char* ColumnR; // text of the right column (can be NULL)
    int ColumnRLen;      // number of characters
    int ColumnRWidth;
    int ColumnRX;

public:
    CMenuItem();
    ~CMenuItem();

    BOOL SetText(const char* text, int len = -1);

    // walks through the TypeData string and according to separators and the threeCol variable
    // sets the variables ColumnL1 - ColumnR, ColumnL1Len - ColumnRLen,
    // ColumnL1Width - ColumnRWidth
    void DecodeSubTextLenghtsAndWidths(CMenuSharedResources* sharedRes, BOOL threeCol);

    friend class CMenuPopup;
    friend class CMenuBar;
};

//*****************************************************************************
//
// CMenuPopup
//

enum CMenuBitmapEnum
{
    menuBitmapArrowR,
    //  menuBitmapArrowL,
    //  menuBitmapArrowU,
    //  menuBitmapArrowD
};

enum CMenuPopupHittestEnum
{
    mphItem,            // on an item, userData = item index
    mphUpArrow,         // on the Up arrow
    mphDownArrow,       // on the Down arrow
    mphBorderOrOutside, // on the border or outside
    //  mphOutside, // outside the window
};

/*
Items
  List of items contained in the pop-up menu.

HParent
  Window that receives notification messages.

HImageList
  Icons displayed before items. The icon is chosen by CMenuItem::ImageIndex.

HWindowsMenu
  Handle of the Windows popup menu. Before opening this submenu its items are
  enumerated and transformed into a temporary CMenuPopup object. After the
  submenu is closed the temporary object is destroyed. Notifications
  WM_INITPOPUP, WM_DRAWITEM and WM_MEASUREITEM are sent for such menus.
*/

class CMenuPopup : public CWindow, public CGUIMenuPopupAbstract
{
protected:
    TIndirectArray<CMenuItem> Items;
    HMENU HWindowsMenu;

    RECT WindowRect;
    int TotalHeight; // total menu height; it may not be fully visible
    int Width;       // dimensions of the client area
    int Height;
    int TopItemY;           // y-coordinate of the first item
    BOOL UpArrowVisible;    // is the Up arrow displayed?
    BOOL UpDownTimerRunnig; // is our timer running?
    BOOL DownArrowVisible;  // is the Down arrow displayed?
    DWORD Style;            // MENU_POPUP_xxxx
    DWORD TrackFlags;       // MENU_TRACK_xxxx
    CMenuSharedResources* SharedRes;
    CMenuPopup* OpenedSubMenu; // if a submenu is open, points to it
    CMenuPopup* FirstPopup;    // if this is not the first window, points to it; otherwise points to itself
    int SelectedItemIndex;     // -1 == none
    BOOL SelectedByMouse;      // TRUE->ByMouse FALSE->ByKeyboard
    HIMAGELIST HImageList;
    HIMAGELIST HHotImageList;
    int ImageWidth; // width of one image from HImageList
    int ImageHeight;
    DWORD ID;                  // copy of ID from CMenuItem
    BOOL Closing;              // HideAll was called and we finish as soon as possible
    int MinWidth;              // during layout the width will not be smaller than this value
    BOOL ModifyMode;           // if the menu is visible, changes are allowed only in ModifyMode
    DWORD SkillLevel;          // determines which items will be shown in this popup
    int MouseWheelAccumulator; // vertical

public:
    //
    // custom methods
    //

    CMenuPopup(DWORD id = 0);
    BOOL LoadFromTemplate2(HINSTANCE hInstance, const MENU_TEMPLATE_ITEM* menuTemplate, DWORD* enablersOffset, HIMAGELIST hImageList, HIMAGELIST hHotImageList, int* addedRows);

    //
    // implementation of CGUIMenuPopupAbstract methods
    //

    virtual BOOL WINAPI LoadFromTemplate(HINSTANCE hInstance, const MENU_TEMPLATE_ITEM* menuTemplate, DWORD* enablersOffset, HIMAGELIST hImageList = NULL, HIMAGELIST hHotImageList = NULL);

    virtual void WINAPI SetSelectedItemIndex(int index); // used to preset the selected item (MENU_TRACK_SELECT must be set or it is ignored)
    virtual int WINAPI GetSelectedItemIndex() { return SelectedItemIndex; }

    virtual void WINAPI SetTemplateMenu(HMENU hWindowsMenu) { HWindowsMenu = hWindowsMenu; }
    virtual HMENU WINAPI GetTemplateMenu() { return HWindowsMenu; }

    virtual CGUIMenuPopupAbstract* WINAPI GetSubMenu(DWORD position, BOOL byPosition);

    // The InsertItem method inserts a new menu item into a menu, moving other items
    // down the menu.
    //
    // Parameters:
    //
    // 'position'     [in] Identifier or position of the menu item before which to insert
    //                the new item. The meaning of this parameter depends on the
    //                value of 'byPosition'.
    //
    // 'byPosition'   [in] Value specifying the meaning of 'position'. If this parameter is FALSE,
    //                'position' is a menu item identifier. Otherwise, it is a menu item position.
    //                If 'byPosition' is TRUE and 'position' is -1, the new menu item is appended
    //                to the end of the menu.
    //
    // 'mii'          [in] Pointer to a MENU_ITEM_INFO structure that contains information about
    //                the new menu item.
    virtual BOOL WINAPI InsertItem(DWORD position, BOOL byPosition, const MENU_ITEM_INFO* mii);

    virtual BOOL WINAPI SetItemInfo(DWORD position, BOOL byPosition, const MENU_ITEM_INFO* mii);
    virtual BOOL WINAPI GetItemInfo(DWORD position, BOOL byPosition, MENU_ITEM_INFO* mii);
    virtual BOOL WINAPI SetStyle(DWORD style); // rodina MENU_POPUP_xxxxx
    virtual BOOL WINAPI CheckItem(DWORD position, BOOL byPosition, BOOL checked);
    virtual BOOL WINAPI CheckRadioItem(DWORD positionFirst, DWORD positionLast, DWORD positionCheck, BOOL byPosition);
    virtual BOOL WINAPI SetDefaultItem(DWORD position, BOOL byPosition);
    virtual BOOL WINAPI EnableItem(DWORD position, BOOL byPosition, BOOL enabled);
    virtual int WINAPI GetItemCount() { return Items.Count; }

    virtual void WINAPI RemoveAllItems();
    virtual BOOL WINAPI RemoveItemsRange(int firstIndex, int lastIndex);

    // allows making changes while the menu popup is open
    virtual BOOL WINAPI BeginModifyMode(); // begins edit mode
    virtual BOOL WINAPI EndModifyMode();   // ends the mode - the menu is redrawn

    // determines which items will be displayed in the menu
    // 'skillLevel' can be MENU_LEVEL_BEGINNER, MENU_LEVEL_INTERMEDIATE or MENU_LEVEL_ADVANCED
    virtual void WINAPI SetSkillLevel(DWORD skillLevel);

    // The FindItemPosition method finds a menu item position.
    //
    // Parameters:
    //
    // 'id'           [in] Identifier of the menu item
    //
    // Return Values:
    //
    // If the method succeeds, the return value is zero base index of the menu item.
    //
    // If menu item is not found, return value is -1.
    virtual int WINAPI FindItemPosition(DWORD id);

    virtual BOOL WINAPI FillMenuHandle(HMENU hMenu);
    virtual BOOL WINAPI GetStatesFromHWindowsMenu(HMENU hMenu);
    virtual void WINAPI SetImageList(HIMAGELIST hImageList, BOOL subMenu = FALSE); // if subMenu==TRUE, the image list handle is also set for submenus
    virtual HIMAGELIST WINAPI GetImageList();
    virtual void WINAPI SetHotImageList(HIMAGELIST hHotImageList, BOOL subMenu = FALSE);
    virtual HIMAGELIST WINAPI GetHotImageList();

    // The TrackPopupMenuEx function displays a shortcut menu at the specified location
    // and tracks the selection of items on the shortcut menu. The shortcut menu can
    // appear anywhere on the screen.
    //
    // Parameters:
    //
    // 'trackFlags'   [in] Use one of the following flags to specify how the function
    //                positions the shortcut menu horizontally: MENU_TRACK_xxxx
    //
    // 'x'            [in] Horizontal location of the shortcut menu, in screen coordinates.
    //
    // 'y'            [in] Vertical location of the shortcut menu, in screen coordinates.
    //
    // 'hwnd'         [in] Handle to the window that owns the shortcut menu. This window
    //                receives all messages from the menu. The window does not receive a
    //                WM_COMMAND message from the menu until the function returns.
    //                If you specify MENU_TRACK_NONOTIFY in the 'trackFlags' parameter, the
    //                function does not send messages to the window identified by hwnd.
    //                However, you still have to pass a window handle in hwnd. It can be
    //                any window handle from your application.
    //
    // 'exclude'      [in] Rectangle to exclude when positioning the menu, in screen
    //                coordinates. This parameter can be NULL.
    //
    // Return Values:
    //   If you specify MENU_TRACK_RETURNCMD in the 'trackFlags' parameter, the return
    //   value is the menu-item identifier of the item that the user selected. If the
    //   user cancels the menu without making a selection, or if an error occurs, then
    //   the return value is zero.
    //
    //   If you do not specify MENU_TRACK_RETURNCMD in the 'trackFlags' parameter, the
    //   return value is nonzero if the function succeeds and zero if it fails.
    virtual DWORD WINAPI Track(DWORD trackFlags, int x, int y, HWND hwnd, const RECT* exclude);

    virtual BOOL WINAPI GetItemRect(int index, RECT* rect); // returns bounding rectangle of the item in screen coordinates

    // iterates through all items and if they have the 'EnablerData' pointer set
    // compares the pointed value with the actual item state.
    // If the state differs, it is updated.
    virtual void WINAPI UpdateItemsState();

    virtual void WINAPI SetMinWidth(int minWidth);

    virtual void WINAPI SetPopupID(DWORD id);
    virtual DWORD WINAPI GetPopupID();
    virtual void WINAPI AssignHotKeys();

protected:
    void Cleanup(); // initializes the object
    BOOL LoadFromHandle();
    void LayoutColumns(); // iterates over items and sets values according to their size
    DWORD GetOwnerDrawItemState(const CMenuItem* item, BOOL selected);
    void DrawCheckBitmapVista(HDC hDC, CMenuItem* item, int yOffset, BOOL selected); // Vista version with alpha blend
    void DrawCheckBitmap(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);      // user-supplied check marks (HBmpChecked and HBmpUnchecked)
    void DrawCheckImage(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);       // standard check marks, ImageIndex, HIcon
    void DrawCheckMark(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);        // calls the appropriate function
    void DrawItem(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);             // draws a single item
    void DrawUpDownItem(HDC hDC, BOOL up);                                           // draws the item containing the up or down arrow
    CMenuPopupHittestEnum HitTest(const POINT* point, int* userData);

    BOOL FindNextItemIndex(int fromIndex, BOOL topToDown, int* index);
    inline CMenuPopup* FindActivePopup();       // finds the last opened popup; returns a pointer to the object
    inline CMenuPopup* FindPopup(HWND hWindow); // searches from this popup down to the last child; returns a pointer to the popup object or NULL
    inline void DoDispatchMessage(MSG* msg, BOOL* leaveMenu, DWORD* retValue, BOOL* dispatchLater);
    void OnTimerTimeout();
    void CheckSelectedPath(CMenuPopup* terminator); // traverses the whole branch and sets SelectedItems so they lead to the last popup

    // Compared to Track, adds [in] menuBar
    //                    [in] delayedMsg
    //                    [in] dispatchDelayedMsg: should delayedMsg be delivered after this method returns?
    //
    DWORD TrackInternal(DWORD trackFlags, int x, int y, HWND hwnd, const RECT* exclude,
                        CMenuBar* menuBar, MSG& delayedMsg, BOOL& dispatchDelayedMsg);

    void CloseOpenedSubmenu();
    void HideAll();

    void PaintAllItems(HRGN hUpdateRgn);

    void OnKeyRight(BOOL* leaveMenu);
    void OnKeyReturn(BOOL* leaveMenu, DWORD* retValue);
    void OnChar(char key, BOOL* leaveMenu, DWORD* retValue);
    int FindNextItemIndex(int firstIndex, char key);

    // for navigation using PgDn/PgUp, searches for the index of the first item
    // after a separator; if 'down' is TRUE it searches downward from 'firstIndex',
    // otherwise upward
    int FindGroupIndex(int firstIndex, BOOL down);

    // if 'byMouse' is TRUE, it is a mouse change, otherwise a keyboard change
    // selection set by keyboard "sticks" while the user moves the mouse outside popups
    void SelectNewItemIndex(int newItemIndex, BOOL byMouse);

    void EnsureItemVisible(int index); // if the item lies outside the visible area,
                                       // scroll and repaint so the item becomes
                                       // fully visible

    void OnMouseWheel(WPARAM wParam, LPARAM lParam);

    // x, y are coordinates of the upper left corner of the window
    // submenuItemPos is used to send a notification to the application
    BOOL CreatePopupWindow(CMenuPopup* firstPopup, int x, int y, int submenuItemPos, const RECT* exclude);

    // Returns the handle of the popup window under the cursor; if a child window
    // is under the cursor, its parent is found.
    //
    // Introduced because of PicaView, which inserts a child window into the context
    // menu and renders an image there with a delay. In Salamander 2.0 moving the
    // cursor over such an image deselected the item because WindowFromPoint returned
    // a window other than the popup.
    HWND PopupWindowFromPoint(POINT point);

    void ResetMouseWheelAccumulator() { MouseWheelAccumulator = 0; }

    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CMenuItem;
    friend class CMenuBar;
};

//*****************************************************************************
//
// CMenuBar
//

class CMenuBar : public CWindow, public CGUIMenuBarAbstract
{
protected:
    CMenuPopup* Menu;
    int Width; // dimensions of the entire window
    int Height;
    HFONT HFont;
    int FontHeight;
    int HotIndex;       // item that is either expanded or pressed (-1 = none)
    HWND HNotifyWindow; // where notifications will be delivered
    BOOL MenuLoop;      // are submenus expanded
    DWORD RetValue;     // which command should be sent to the application window
    MSG DelayedMsg;
    BOOL DispatchDelayedMsg;
    BOOL HotIndexIsTracked; // is a popup open under HotIndex?
    BOOL HandlingVK_MENU;
    BOOL WheelDuringMenu;
    POINT LastMouseMove;
    BOOL Closing;        // WM_USER_CLOSEMENU was called and we exit as soon as possible
    HANDLE HCloseEvent;  // used to start the message queue
    BOOL MouseIsTracked; // is the mouse tracked using TrackMouseEvent?
    BOOL HelpMode;       // Context Help mode (Shift+F1)?

    // these two variables are used for cooperation between MenuBar and MenuPopup
    // they are set in CMenuPopup::TrackInternal and determine further behavior
    // of MenuBar after the popup is closed
    int IndexToOpen;     // -1 means no additional popup should be opened,
                         // otherwise the index of the popup to open
    BOOL OpenWithSelect; // should the first item be selected when the menu opens?
    BOOL OpenByMouse;    // opened via mouse or keyboard?
    BOOL ExitMenuLoop;   // TRUE to exit MenuLoop
    BOOL HelpMode2;      // did we receive WM_USER_HELP_MOUSEMOVE and wait for
                         // WM_USER_HELP_MOUSELEAVE? (we must highlight the item under the cursor)
    WORD UIState;        // accelerator display
    BOOL ForceAccelVisible;

public:
    //
    // custom methods
    //
    CMenuBar(CMenuPopup* menu, HWND hNotifyWindow, CObjectOrigin origin = ooStatic);
    ~CMenuBar();

    //
    // implementation of CGUIMenuBarAbstract methods
    //

    virtual BOOL WINAPI CreateWnd(HWND hParent);
    virtual HWND WINAPI GetHWND() { return HWindow; }

    virtual int WINAPI GetNeededWidth();                 // returns the width needed for the window
    virtual int WINAPI GetNeededHeight();                // returns the height needed for the window
    virtual void WINAPI SetFont();                       // obtains the menu bar font from the system
    virtual BOOL WINAPI GetItemRect(int index, RECT& r); // Returns the item's rectangle in screen coordinates.
    virtual void WINAPI EnterMenu();                     // user pressed VK_MENU
    virtual BOOL WINAPI IsInMenuLoop() { return MenuLoop; }
    virtual void WINAPI SetHelpMode(BOOL helpMode) { HelpMode = helpMode; }

    // If the message is translated, the return value is TRUE.
    virtual BOOL WINAPI IsMenuBarMessage(CONST MSG* lpMsg);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DrawItem(int index);
    void DrawItem(HDC hDC, int index, int x);
    void DrawAllItems(HDC hDC);
    void RefreshMinWidths(); // iterates all items and computes the 'MinWidth' of each

    void TrackHotIndex();                                                  // presses HotIndex and calls TrackPopup; returns once it closes
    void EnterMenuInternal(int index, BOOL openWidthSelect, BOOL byMouse); // byMouse indicates whether the menu was opened by mouse or keyboard

    // returns TRUE if there is an item at the position and sets 'index';
    // otherwise returns FALSE
    BOOL HitTest(int xPos, int yPos, int& index);

    // searches inserted submenus and returns TRUE if one with the hot key 'hotKey' is found;
    // the index of the item is returned as well
    BOOL HotKeyIndexLookup(char hotKey, int& itemIndex);

    friend class CMenuPopup;
};

BOOL InitializeMenu();
void ReleaseMenu();

extern CMenuPopup MainMenu;
extern CMenuPopup ArchiveMenu;
extern CMenuPopup ArchivePanelMenu;

BOOL BuildSalamanderMenus();           // builds the global menu for Salamander
BOOL BuildFindMenu(CMenuPopup* popup); // builds an instance of the find menu

// Adds to 'popup' items created from the 'buttonsID' array.
// 'hWindow' is the parent of the buttons referenced by the 'buttonsID' array.
// The 'buttonsID' array may contain any number of IDs terminated by 0.
// ID -1 denotes a separator and -2 marks the default item (the next item will
// have the default state set). Other numbers are treated as button IDs; their
// text is extracted and added to the menu and their enabled state is mirrored in
// the menu item.
void FillContextMenuFromButtons(CMenuPopup* popup, HWND hWindow, int* buttonsID);
