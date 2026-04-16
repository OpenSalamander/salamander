// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_gui) // to make the structures independent of the current packing alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//////////////////////////////////////////////////////
//                                                    //
// WM_APP + 200 through WM_APP + 399 are reserved in  //
// const.h and excluded from the range used for       //
// Salamander internal messages.                                       //
//                                                    //
//////////////////////////////////////////////////////

// menu messages
#define WM_USER_ENTERMENULOOP WM_APP + 200   // [0, 0] menu mode entered
#define WM_USER_LEAVEMENULOOP WM_APP + 201   // [0, 0] menu loop mode ended; this message is sent before the command is posted
#define WM_USER_LEAVEMENULOOP2 WM_APP + 202  // [0, 0] menu loop mode ended; this message is posted only after the command
#define WM_USER_INITMENUPOPUP WM_APP + 204   // [(CGUIMenuPopupAbstract*)menuPopup, LOWORD(uPos), HIWORD(uID)]
#define WM_USER_UNINITMENUPOPUP WM_APP + 205 // [(CGUIMenuPopupAbstract*)menuPopup, LOWORD(uPos), HIWORD(uID)]
#define WM_USER_CONTEXTMENU WM_APP + 206     // [(CGUIMenuPopupAbstract*)menuPopup, (BOOL)fromMouse \
                                             // (if triggered by a mouse event, it is TRUE (use GetMessagePos); \
                                             //    if triggered by the keyboard event VK_APPS or Shift+F10, it is FALSE)] \
                                             // p.s. if it returns TRUE, a menu command is executed or a submenu is opened \
                                             // To cast menuPopup to CMenuPopup in Salamander, \
                                             // use (CMenuPopup*)(CGUIMenuPopupAbstract*)menuPopup.

// toolbar messages
#define WM_USER_TBDROPDOWN WM_APP + 220    // [HWND hToolBar, int buttonIndex]
#define WM_USER_TBRESET WM_APP + 222       // [HWND hToolBar, TOOLBAR_TOOLTIP *tt]
#define WM_USER_TBBEGINADJUST WM_APP + 223 // [HWND hToolBar, 0]
#define WM_USER_TBENDADJUST WM_APP + 224   // [HWND hToolBar, 0]
#define WM_USER_TBGETTOOLTIP WM_APP + 225  // [HWND hToolBar, 0]
#define WM_USER_TBCHANGED WM_APP + 226     // [HWND hToolBar, 0]
#define WM_USER_TBENUMBUTTON2 WM_APP + 227 // [HWND hToolBar, TLBI_ITEM_INFO2 *tii]

// tooltip messages
#define TOOLTIP_TEXT_MAX 5000          // maximum tooltip text length (WM_USER_TTGETTEXT message)
#define WM_USER_TTGETTEXT WM_APP + 240 // [ID passed to SetCurrentToolTip, buffer limited to TOOLTIP_TEXT_MAX]

// button pressed
#define WM_USER_BUTTON WM_APP + 244 // [(LO)WORD buttonID, (LO)WORD event triggered from the keyboard; if opening a menu, select the first item]
// drop down of button pressed
#define WM_USER_BUTTONDROPDOWN WM_APP + 245 // [(LO)WORD buttonID, (LO)WORD event triggered from the keyboard; if opening a menu, select the first item]

#define WM_USER_KEYDOWN WM_APP + 246 // [(LO)WORD ctrlID, DWORD virtual-key code]

//
// ****************************************************************************
// CGUIProgressBarAbstract
//

class CGUIProgressBarAbstract
{
public:
    // Sets the progress and optionally the text in the center
    //
    // A safer variant, SetProgress2(), exists; check it before using this method.
    //
    // The progress control can work in two modes:
    //   1) for 'progress' >= 0 it is a standard progress bar from 0% to 100%
    //      in this mode, the 'text' variable can be used to set custom text shown in the center
    //      if 'text' == NULL, the standard percentage is shown in the center
    //   2) for 'progress' == -1 it is an indeterminate state in which a small rectangle moves back and forth
    //      the movement is controlled by SetSelfMoveTime(), SetSelfMoveSpeed(), and Stop()
    //
    // Redrawing is immediate; for most operations it is better to cache the data in the parent
    // dialog and start a 100 ms timer that calls this method.
    //
    // Can be called from any thread; the control's thread must be running, otherwise the call blocks
    // (SendMessage is used to deliver the 'progress' value to the control);
    virtual void WINAPI SetProgress(DWORD progress, const char* text) = 0;

    // Used together with SetProgress(-1)
    // specifies how many milliseconds the rectangle keeps moving on its own after SetProgress(-1) is called
    // if another SetProgress(-1) is called during that time, the time is counted from the start again
    // if 'time' == 0, the rectangle moves only once, exactly when SetProgress(-1) is called
    // for 'time' == 0xFFFFFFFF, the rectangle keeps moving indefinitely (default value)
    virtual void WINAPI SetSelfMoveTime(DWORD time) = 0;

    // Used together with SetProgress(-1)
    // specifies the time between rectangle moves, in milliseconds
    // the default value is 'moveTime' == 50, which means 20 moves per second
    virtual void WINAPI SetSelfMoveSpeed(DWORD moveTime) = 0;

    // Used together with SetProgress(-1)
    // if the rectangle is currently moving (because of SetSelfMoveTime), it is stopped
    virtual void WINAPI Stop() = 0;

    // Sets the progress and optionally the text in the center
    //
    // Compared to SetProgress(), this method has the advantage that if 'progressCurrent' >= 'progressTotal',
    // it sets the progress directly: if 'progressTotal' is 0 it sets 0%, otherwise 100%, and does not perform the calculation
    // (it is meaningless and triggers RTC because of the cast). This "disallowed" state occurs,
    // for example, when a file grows during the operation or when working with file links - links have
    // zero size, but then contain data with the size of the linked file.
    // If you do the calculation yourself, you must handle this "disallowed" state.
    //
    // The progress control can work in two modes (see SetProgress()); this method can be used
    // only in mode 1):
    //   1) it is a standard progress bar from 0% to 100%
    //      in this mode, the 'text' variable can be used to set custom text shown in the center
    //      if 'text' == NULL, the standard percentage is shown in the center
    //
    // Redrawing is immediate; for most operations it is better to cache the data in the parent
    // dialog and start a 100 ms timer that calls this method.
    //
    // Can be called from any thread; the control's thread must be running, otherwise the call blocks
    // (SendMessage is used to deliver the 'progress' value to the control);
    virtual void WINAPI SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                                     const char* text) = 0;

    // Usage examples:
    //
    // 1. We want to move the rectangle manually; it does not move on its own
    //
    //   SetSelfMoveTime(0)           // disable automatic movement
    //   SetProgress(-1, NULL)        // move it by one step
    //   ...
    //   SetProgress(-1, NULL)        // move it by one step
    //
    // 2. The rectangle should move on its own until Stop is called
    //
    //   SetSelfMoveTime(0xFFFFFFFF)  // infinite movement
    //   SetSelfMoveSpeed(50)         // 20 moves per second
    //   SetProgress(-1, NULL)        // start the rectangle
    //   ...                          // do some work
    //   Stop()                       // stop the rectangle
    //
    // 3. The rectangle should move for a limited time and then stop
    //   if we "nudge" it during that time, the timeout is refreshed
    //
    //   SetSelfMoveTime(1000)        // moves on its own for one second, then stops
    //   SetSelfMoveSpeed(50)         // 20 moves per second
    //   SetProgress(-1, NULL)        // start the rectangle for one second
    //   ...
    //   SetProgress(-1, NULL)        // keep the rectangle alive for another second
    //
    // 4. The operation was paused and we want to show it in the progress bar
    //
    //   SetProgress(0, NULL)         // 0%
    //   SetProgress(100, NULL)       // 10%
    //   SetProgress(200, NULL)       // 20%
    //   SetProgress(300, "(paused)") // 30% -- instead of "30 %" the text "(paused)" is shown
    //   ... (waiting for resume)
    //   SetProgress(300, NULL)       // 30% -- turn off the paused text and continue
    //   SetProgress(400, NULL)       // 40%
    //   ...
};

//
// ****************************************************************************
// CGUIStaticTextAbstract
//

#define STF_CACHED_PAINT 0x0000000001    // text is drawn through the cache (it will not flicker) \
                                         // WARNING: drawing is much slower than without this flag. \
                                         // Do not use it for dialog text that is displayed \
                                         // once and then remains unchanged. \
                                         // Use it for frequently or rapidly changing text (current operation).
#define STF_BOLD 0x0000000002            // a bold font is used for the text
#define STF_UNDERLINE 0x0000000004       // an underlined font is used for the text (because of poor readability \
                                         // use only for HyperLink and special cases)
#define STF_DOTUNDERLINE 0x0000000008    // the text uses dotted underline (because of poor readability \
                                         // use only for HyperLink and special cases)
#define STF_HYPERLINK_COLOR 0x0000000010 // the text color is determined by the hyperlink color
#define STF_END_ELLIPSIS 0x0000000020    // if the text is too long, it is truncated with an ellipsis "..."
#define STF_PATH_ELLIPSIS 0x0000000040   // if the text is too long, it is shortened and an ellipsis "..." is inserted \
                                         // so that the end remains visible
#define STF_HANDLEPREFIX 0x0000000080    // Characters after '&' are underlined; cannot be used with STF_END_ELLIPSIS or STF_PATH_ELLIPSIS

class CGUIStaticTextAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
    //
    // The control can be reached from the keyboard in a dialog if it is given the WS_TABSTOP style.
public:
    // Sets the control text; calling this method is faster and less computationally expensive
    // than setting the text with WM_SETTEXT; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI SetText(const char* text) = 0;

    // Returns the control text; can be called from any thread.
    // Returns NULL if SetText has not been called yet and the static control had no text
    virtual const char* WINAPI GetText() = 0;

    // Sets the character used to separate path components; used with STF_PATH_ELLIPSIS;
    // default is '\\';
    virtual void WINAPI SetPathSeparator(char separator) = 0;

    // Sets the text to be shown as a tooltip
    // returns TRUE if a copy of the text was allocated successfully, otherwise FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // Sets the window and ID to which WM_USER_TTGETTEXT is sent when the tooltip is displayed
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIHyperLinkAbstract
//

class CGUIHyperLinkAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
    //
    // The control can be reached from the keyboard in a dialog if it is given the WS_TABSTOP style.
public:
    // Sets the control text; calling this method is faster and less computationally expensive
    // than setting the text with WM_SETTEXT; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI SetText(const char* text) = 0;

    // Returns the control text; can be called from any thread.
    // Returns NULL if SetText has not been called yet and the static control had no text
    virtual const char* WINAPI GetText() = 0;

    // Assigns an action that opens a URL (file="https://www.altap.cz") or
    // launches a program (file="C:\\TEST.EXE"); ShellExecute
    // is called for the parameter with the 'open' verb.
    virtual void WINAPI SetActionOpen(const char* file) = 0;

    // Assigns the action PostCommand(WM_COMMAND, command, 0) to the parent window
    virtual void WINAPI SetActionPostCommand(WORD command) = 0;

    // Assigns the action that shows the hint and tooltip text 'text'
    // if 'text' is NULL, the tooltip can be assigned by calling
    // SetToolTipText or SetToolTip; the method then always returns TRUE
    // if 'text' is not NULL, the method returns TRUE if it succeeds
    // in allocating a copy of the text; otherwise it returns FALSE
    // the tooltip can be shown with Space/Up/Down (if the control
    // has focus) or by clicking the mouse; the hint (tooltip) is then shown directly
    // below the text and stays open until the user clicks outside it with the mouse or
    // presses a key
    virtual BOOL WINAPI SetActionShowHint(const char* text) = 0;

    // Sets the text to be shown as a tooltip
    // returns TRUE if a copy of the text was allocated successfully, otherwise FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // Sets the window and ID to which WM_USER_TTGETTEXT is sent when the tooltip is displayed
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIButtonAbstract
//

class CGUIButtonAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
public:
    // Sets the text to be shown as a tooltip; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // Sets the window and ID to which WM_USER_TTGETTEXT is sent when the tooltip is displayed
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIColorArrowButtonAbstract
//

class CGUIColorArrowButtonAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
public:
    // Sets the text color 'textColor' and background color 'bkgndColor'
    virtual void WINAPI SetColor(COLORREF textColor, COLORREF bkgndColor) = 0;

    // Sets the text color 'textColor'
    virtual void WINAPI SetTextColor(COLORREF textColor) = 0;

    // Sets the background color 'bkgndColor'
    virtual void WINAPI SetBkgndColor(COLORREF bkgndColor) = 0;

    // Returns the text color
    virtual COLORREF WINAPI GetTextColor() = 0;

    // Returns the background color
    virtual COLORREF WINAPI GetBkgndColor() = 0;
};

//
// ****************************************************************************
// CGUIMenuPopupAbstract
//

#define MNTT_IT 1 // item
#define MNTT_PB 2 // popup begin
#define MNTT_PE 3 // popup end
#define MNTT_SP 4 // separator

#define MNTS_B 0x01 // skill level beginned
#define MNTS_I 0x02 // skill level intermediate
#define MNTS_A 0x04 // skill level advanced

struct MENU_TEMPLATE_ITEM
{
    int RowType;      // MNTT_*
    int TextResID;    // text resource
    BYTE SkillLevel;  // MNTS_*
    DWORD ID;         // generated command
    short ImageIndex; // -1 = no icon
    DWORD State;
    DWORD* Enabler; // state variable for enabling the item
};

//
// constants
//

#define MENU_MASK_TYPE 0x00000001       // Retrieves or sets the 'Type' member.
#define MENU_MASK_STATE 0x00000002      // Retrieves or sets the 'State' member.
#define MENU_MASK_ID 0x00000004         // Retrieves or sets the 'ID' member.
#define MENU_MASK_SUBMENU 0x00000008    // Retrieves or sets the 'SubMenu' member.
#define MENU_MASK_CHECKMARKS 0x00000010 // Retrieves or sets the 'HBmpChecked' and 'HBmpUnchecked' members.
#define MENU_MASK_BITMAP 0x00000020     // Retrieves or sets the 'HBmpItem' member.
#define MENU_MASK_STRING 0x00000080     // Retrieves or sets the 'String' member.
#define MENU_MASK_IMAGEINDEX 0x00000100 // Retrieves or sets the 'ImageIndex' member.
#define MENU_MASK_ICON 0x00000200       // Retrieves or sets the 'HIcon' member.
#define MENU_MASK_OVERLAY 0x00000400    // Retrieves or sets the 'HOverlay' member.
#define MENU_MASK_CUSTOMDATA 0x00000800 // Retrieves or sets the 'CustomData' member.
#define MENU_MASK_ENABLER 0x00001000    // Retrieves or sets the 'Enabler' member.
#define MENU_MASK_SKILLLEVEL 0x00002000 // Retrieves or sets the 'SkillLevel' member.
#define MENU_MASK_FLAGS 0x00004000      // Retrieves or sets the 'Flags' member.

#define MENU_TYPE_STRING 0x00000001     // Displays the menu item using a text string.
#define MENU_TYPE_BITMAP 0x00000002     // Displays the menu item using a bitmap.
#define MENU_TYPE_SEPARATOR 0x00000004  // Specifies that the menu item is a separator.
#define MENU_TYPE_OWNERDRAW 0x00000100  // Assigns responsibility for drawing the menu item to the window that owns the menu.
#define MENU_TYPE_RADIOCHECK 0x00000200 // Displays selected menu items using a radio-button mark instead of a check mark if the HBmpChecked member is NULL.

#define MENU_FLAG_NOHOTKEY 0x00000001 // AssignHotKeys will skip this item

#define MENU_STATE_GRAYED 0x00000001  // Disables the menu item and grays it so that it cannot be selected.
#define MENU_STATE_CHECKED 0x00000002 // Checks the menu item.
#define MENU_STATE_DEFAULT 0x00000004 // Specifies that the menu item is the default. A menu can contain only one default menu item, which is displayed in bold.

#define MENU_LEVEL_BEGINNER 0x00000001
#define MENU_LEVEL_INTERMEDIATE 0x00000002
#define MENU_LEVEL_ADVANCED 0x00000004

#define MENU_POPUP_THREECOLUMNS 0x00000001
#define MENU_POPUP_UPDATESTATES 0x00000002 // UpdateStates is called before opening

// these flags are modified for individual popups within a Track branch
#define MENU_TRACK_SELECT 0x00000001 // If this flag is set, the function select item specified by SetSelectedItemIndex.
//#define MENU_TRACK_LEFTALIGN    0x00000000 // If this flag is set, the function positions the shortcut menu so that its left side is aligned with the coordinate specified by the x parameter.
//#define MENU_TRACK_TOPALIGN     0x00000000 // If this flag is set, the function positions the shortcut menu so that its top side is aligned with the coordinate specified by the y parameter.
//#define MENU_TRACK_HORIZONTAL   0x00000000 // If the menu cannot be shown at the specified location without overlapping the excluded rectangle, the system tries to accommodate the requested horizontal alignment before the requested vertical alignment.
#define MENU_TRACK_CENTERALIGN 0x00000002  // If this flag is set, the function centers the shortcut menu horizontally relative to the coordinate specified by the x parameter.
#define MENU_TRACK_RIGHTALIGN 0x00000004   // Positions the shortcut menu so that its right side is aligned with the coordinate specified by the x parameter.
#define MENU_TRACK_VCENTERALIGN 0x00000008 // If this flag is set, the function centers the shortcut menu vertically relative to the coordinate specified by the y parameter.
#define MENU_TRACK_BOTTOMALIGN 0x00000010  // If this flag is set, the function positions the shortcut menu so that its bottom side is aligned with the coordinate specified by the y parameter.
#define MENU_TRACK_VERTICAL 0x00000100     // If the menu cannot be shown at the specified location without overlapping the excluded rectangle, the system tries to accommodate the requested vertical alignment before the requested horizontal alignment.
// flags shared by one Track branch
#define MENU_TRACK_NONOTIFY 0x00001000  // If this flag is set, the function does not send notification messages when the user clicks on a menu item.
#define MENU_TRACK_RETURNCMD 0x00002000 // If this flag is set, the function returns the menu item identifier of the user's selection in the return value.
//#define MENU_TRACK_LEFTBUTTON   0x00000000 // If this flag is set, the user can select menu items with only the left mouse button.
#define MENU_TRACK_RIGHTBUTTON 0x00010000 // If this flag is set, the user can select menu items with both the left and right mouse buttons.
#define MENU_TRACK_HIDEACCEL 0x00100000   // Salamander 2.51 or later: If this flag is set, the acceleration keys will not be underlined (specify when menu is opened by mouse event).

class CGUIMenuPopupAbstract;

struct MENU_ITEM_INFO
{
    DWORD Mask;
    DWORD Type;
    DWORD State;
    DWORD ID;
    CGUIMenuPopupAbstract* SubMenu;
    HBITMAP HBmpChecked;
    HBITMAP HBmpUnchecked;
    HBITMAP HBmpItem;
    char* String;
    DWORD StringLen;
    int ImageIndex;
    HICON HIcon;
    HICON HOverlay;
    ULONG_PTR CustomData;
    DWORD SkillLevel;
    DWORD* Enabler;
    DWORD Flags;
};

/*
Mask
  Members to retrieve or set. This member can be one or more of these values.

Type
  Item type. This member can have one or more of these values:

   MENU_TYPE_OWNERDRAW    Item drawing is handled by the window that owns the menu.
                          Each menu item receives WM_MEASUREITEM and
                          WM_DRAWITEM messages. TypeData contains a 32-bit
                          application-defined value.

   MENU_TYPE_RADIOCHECK   Checked items are shown with a dot instead of a check mark,
                          je-li HBmpChecked rovno NULL.

   MENU_TYPE_SEPARATOR    Horizontal separator line. TypeData is unused.

   MENU_TYPE_STRING       The item contains a text string. TypeData points to a null-
                          terminated string.

   MENU_TYPE_BITMAP       The item contains a bitmap.

  MENU_TYPE_BITMAP, MENU_TYPE_SEPARATOR, and MENU_TYPE_STRING cannot be used together.

State
  Item state. This member can have one or more of these values:

   MENU_STATE_CHECKED     The item is checked.

   MENU_STATE_DEFAULT     The menu can contain only one default item. It is
                          drawn in bold.

   MENU_STATE_GRAYED      Disables the item; it is shown in gray and cannot be selected.

SkillLevel
  User skill level of the item. This member can have one of these values:

   MENU_LEVEL_BEGINNER       beginner - always shown
   MENU_LEVEL_INTERMEDIATE   intermediate - shown to guru and intermediate users
   MENU_LEVEL_ADVANCED       advanced - shown only to guru users

ID
  Application-defined 16-bit value that identifies the menu item.

SubMenu
  Pointer to the popup menu attached to this item. If this item
  neotevira submenu, je SubMenu rovno NULL.

HBmpChecked
  Handle of the bitmap shown before the item when the item is
  checked. If this member is NULL, the default
  bitmap is used. If the MENU_TYPE_RADIOCHECK bit is set, the default
  bitmap is a dot; otherwise it is a check mark. If ImageIndex is not -1,
  this bitmap is not used.

HBmpUnchecked
  Handle of the bitmap shown before the item when the item is not
  checked. If this member is NULL, no
  bitmap is shown. If ImageIndex is not -1,
  this bitmap is not used.

ImageIndex
  Index bitmapy v ImageListu CMenuPopup::HImageList. Bitmapa je vykreslena
  before the item, depending on MENU_STATE_CHECKED and MENU_STATE_GRAYED.
  If the member is -1, nothing is drawn.

Enabler
  Pointer to a DWORD that determines the item state: TRUE->enabled, FALSE->grayed.
  If it is NULL, the item is enabled.
*/

class CGUIMenuPopupAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
public:
    //
    // LoadFromTemplate
    //   Builds menu contents based on 'menuTemplate',
    //
    // Parameters
    //   'hInstance'
    //      [in] Handle to the module containing the string resources (MENU_TEMPLATE_ITEM::TextResID).
    //
    //   'menuTemplate'
    //      [in] Pointer to a menu template.
    //
    //      A menu template consists of two or more MENU_TEMPLATE_ITEM structures.
    //      'MENU_TEMPLATE_ITEM::RowType' of first structure must be MNTT_PB (popup begin).
    //      'MENU_TEMPLATE_ITEM::RowType' of last structure must be MNTT_PE (popup end).
    //
    //   'enablersOffset'
    //      [in] Pointer to array of enablers.
    //
    //      If this parameter is NULL, 'MENU_ITEM_INFO::Enabler' value is pointer to enabler
    //      variable. Otherwise 'MENU_ITEM_INFO::Enabler' is index to the enablers array.
    //      Zero index is reserved for "always enabled" item.
    //
    //   'hImageList'
    //      [in] Handle of image list that the menu will use to display menu items images
    //      that are in their default state.
    //
    //      If this parameter is NULL, no images will be displayed in the menu items.
    //
    //   'hHotImageList'
    //      [in] Handle of image list that the menu will use to display menu items images
    //      that are in their selected or checked state.
    //
    //      If this parameter is NULL, normal images will be displayed instead of hot images.
    //
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI LoadFromTemplate(HINSTANCE hInstance,
                                         const MENU_TEMPLATE_ITEM* menuTemplate,
                                         DWORD* enablersOffset,
                                         HIMAGELIST hImageList,
                                         HIMAGELIST hHotImageList) = 0;

    //
    // SetSelectedItemIndex
    //   Sets the item which will be selected when menu popup is displayed.
    //
    // Parameters
    //   'index'
    //      [in] The index to select.
    //      If this value is -1, none of the items will be selected.
    //      This index is only applied when method Track with MENU_TRACK_SELECT flag is called.
    //
    // See Also
    //   GetSelectedItemIndex
    //
    virtual void WINAPI SetSelectedItemIndex(int index) = 0;

    //
    // GetSelectedItemIndex
    //   Retrieves the currently selected item in the menu.
    //
    // Return Values
    //   Returns the index of the selected item, or -1 if no item is selected.
    //
    // See Also
    //   SetSelectedItemIndex
    //
    virtual int WINAPI GetSelectedItemIndex() = 0;

    //
    // SetTemplateMenu
    //   Assigns the Windows menu handle which will be used as template when menu popup is displayed.
    //
    // Parameters
    //   'hWindowsMenu'
    //      [in] Handle to the Windows menu handle to be used as template.
    //      If this value is NULL, Windows menu handle will not be used.
    //
    // See Also
    //   GetTemplateMenu
    //
    virtual void WINAPI SetTemplateMenu(HMENU hWindowsMenu) = 0;

    //
    // GetTemplateMenu
    //   Retrieves a handle to the Windows menu assigned as template.
    //
    // Return Values
    //   The return value is a handle to the Windows menu.
    //   If the object has no Windows menu template assigned, the return value is NULL.
    //
    // See Also
    //   SetTemplateMenu
    //
    virtual HMENU WINAPI GetTemplateMenu() = 0;

    //
    // GetSubMenu
    //   Retrieves a pointer to the submenu activated by the specified menu item.
    //
    // Parameters
    //   'position'
    //      [in] Specifies the zero-based position of the menu item that activates the submenu.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    // Return Values
    //   If the function succeeds, the return value is a pointer to the submenu activated by the menu item.
    //   If the menu item does not activate submenu, the return value is NULL.
    //

    virtual CGUIMenuPopupAbstract* WINAPI GetSubMenu(DWORD position, BOOL byPosition) = 0;

    //
    // InsertItem
    //   Inserts a new menu item at the specified position in a menu.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item before which to insert the new item.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'mii'
    //      [in] Pointer to a MENU_ITEM_INFO structure that contains information about the new menu item.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   SetItemInfo, GetItemInfo
    //
    virtual BOOL WINAPI InsertItem(DWORD position,
                                   BOOL byPosition,
                                   const MENU_ITEM_INFO* mii) = 0;

    //
    // SetItemInfo
    //   Changes information about a menu item.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'mii'
    //      [in] Pointer to a MENU_ITEM_INFO structure that contains information about the menu item
    //      and specifies which menu item attributes to change.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem, GetItemInfo
    //
    virtual BOOL WINAPI SetItemInfo(DWORD position,
                                    BOOL byPosition,
                                    const MENU_ITEM_INFO* mii) = 0;

    //
    // GetItemInfo
    //   Retrieves information about a menu item.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to get information about.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'mii'
    //      [in/out] Pointer to a MENU_ITEM_INFO structure that contains information to retrieve
    //      and receives information about the menu item.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI GetItemInfo(DWORD position,
                                    BOOL byPosition,
                                    MENU_ITEM_INFO* mii) = 0;

    //
    // SetStyle
    //   Sets the menu popup style.
    //
    // Parameters
    //   'style'
    //      [in] New menu style.
    //      This parameter can be a combination of menu popup styles MENU_POPUP_*.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI SetStyle(DWORD style) = 0;

    //
    // CheckItem
    //   Sets the state of the specified menu item's check-mark attribute to either checked or clear.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'checked'
    //      [in] Indicates whether the menu item will be checked or cleared.
    //      If this parameter is TRUE, sets the check-mark attribute to the selected state.
    //      If this parameter is FALSE, sets the check-mark attribute to the clear state.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, CheckRadioItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI CheckItem(DWORD position,
                                  BOOL byPosition,
                                  BOOL checked) = 0;

    //
    // CheckRadioItem
    //   Checks a specified menu item and makes it a radio item. At the same time, the method
    //   clears all other menu items in the associated group and clears the radio-item type
    //   flag for those items.
    //
    // Parameters
    //   'positionFirst'
    //      [in] Identifier or position of the first menu item in the group.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'positionLast'
    //      [in] Identifier or position of the last menu item in the group.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'positionCheck'
    //      [in] Identifier or position of the menu item to check.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'positionFirst', 'positionLast', and
    //      'positionCheck'. If this parameter is FALSE, the other parameters specify
    //      menu item identifiers. Otherwise, the other parameters specify the menu
    //      item positions.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, CheckItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI CheckRadioItem(DWORD positionFirst,
                                       DWORD positionLast,
                                       DWORD positionCheck,
                                       BOOL byPosition) = 0;

    //
    // SetDefaultItem
    //   Sets the default menu item for the specified menu.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the new default menu item or –1 for no default item.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, CheckItem, CheckRadioItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI SetDefaultItem(DWORD position,
                                       BOOL byPosition) = 0;

    //
    // EnableItem
    //   Enables or disables the specified menu item.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to be enabled or disabled.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'enabled'
    //      [in] Indicates whether the menu item will be enabled or disabled.
    //      If this parameter is TRUE, enables the menu item.
    //      If this parameter is FALSE, disables the menu item.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   CheckItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI EnableItem(DWORD position,
                                   BOOL byPosition,
                                   BOOL enabled) = 0;

    //
    // EnableItem
    //   Determines the number of items in the specified menu.
    //
    // Return Values
    //   The return value specifies the number of items in the menu.
    //
    virtual int WINAPI GetItemCount() = 0;

    //
    // RemoveAllItems
    //   Deletes all items from the menu popup.
    //   If the removed menu item opens submenu, this method frees the memory used by the submenu.
    //
    // See Also
    //   RemoveItemsRange
    //
    virtual void WINAPI RemoveAllItems() = 0;

    //
    // RemoveItemsRange
    //   Deletes items range from the menu popup.
    //
    // Parameters
    //   'firstIndex'
    //      [in] Specifies the first menu item to be deleted.
    //
    //   'lastIndex'
    //      [in] Specifies the last menu item to be deleted.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   RemoveAllItems
    //
    virtual BOOL WINAPI RemoveItemsRange(int firstIndex,
                                         int lastIndex) = 0;

    //
    // BeginModifyMode
    //   Allows changes of the opened menu popup.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EndModifyMode
    //
    virtual BOOL WINAPI BeginModifyMode() = 0;

    //
    // EndModifyMode
    //   Ends modify mode.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   BeginModifyMode
    //
    virtual BOOL WINAPI EndModifyMode() = 0;

    //
    // SetSkillLevel
    //   Sets user skill level for this menu popup.
    //
    // Parameters
    //   'skillLevel'
    //      [in] Specifies the user skill level.
    //      This parameter must be one or a combination of MENU_LEVEL_*.
    //
    virtual void WINAPI SetSkillLevel(DWORD skillLevel) = 0;

    //
    // FindItemPosition
    //   Retrieves the menu item position in the menu popup.
    //
    // Parameters
    //   'id'
    //      [in] Specifies the identifier of the menu item whose position is to be retrieved.
    //
    // Return Values
    //   Zero-based position of the specified menu item.
    //   If menu item is not found, return value is -1.
    //
    virtual int WINAPI FindItemPosition(DWORD id) = 0;

    //
    // FillMenuHandle
    //   Inserts the menu items to the Windows menu popup.
    //
    // Parameters
    //   'hMenu'
    //      [in] Handle to the menu to be filled.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI FillMenuHandle(HMENU hMenu) = 0;

    //
    // GetStatesFromHWindowsMenu
    //   Applies Windows menu popup item states to the contained items.
    //
    // Parameters
    //   'hMenu'
    //      [in] Handle to the menu whose item states are to be retrieved.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetStatesFromHWindowsMenu(HMENU hMenu) = 0;

    //
    // SetImageList
    //   Sets the image list that the menu will use to display images in the items that
    //   are in their default state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //      If this parameter is NULL, no images will be displayed in the items.
    //
    //   'subMenu'
    //      [in] Specifies whether to set SubMenus image list to.
    //      If this parameter is TRUE, image list will be set also in all submenu items,
    //      otherwise image list will be set only in this menu popup.
    //
    // See Also
    //   SetHotImageList
    //
    virtual void WINAPI SetImageList(HIMAGELIST hImageList,
                                     BOOL subMenu) = 0;

    //
    // SetHotImageList
    //   Sets the image list that the menu will use to display images in the items that
    //   are in their hot or checked state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //      If this parameter is NULL, no images will be displayed in the items.
    //
    //   'subMenu'
    //      [in] Specifies whether to set SubMenus image list to.
    //      If this parameter is TRUE, image list will be set also in all submenu items,
    //      otherwise image list will be set only in this menu popup.
    //
    // See Also
    //   SetImageList
    //
    virtual void WINAPI SetHotImageList(HIMAGELIST hHotImageList,
                                        BOOL subMenu) = 0;

    //
    // Track
    //   Displays a shortcut menu at the specified location and tracks the selection of
    //   items on the shortcut menu.
    //
    // Parameters
    //   'trackFlags'
    //      [in] Specifies function options.
    //      This parameter can be a combination of MENU_TRACK_* flags.
    //
    //   'x'
    //      [in] Horizontal location of the shortcut menu, in screen coordinates.
    //
    //   'y'
    //      [in] Vertical location of the shortcut menu, in screen coordinates.
    //
    //   'hwnd'
    //      [in] Handle to the window that owns the shortcut menu. This window
    //      receives all messages from the menu. The window does not receive
    //      a WM_COMMAND message from the menu until the function returns.
    //
    //      If you specify MENU_TRACK_NONOTIFY in the 'trackFlags' parameter,
    //      the function does not send messages to the window identified by hwnd.
    //      However, you still have to pass a window handle in 'hwnd'. It can be
    //      any window handle from your application.
    //   'exclude'
    //      [in] Rectangle to exclude when positioning the window, in screen coordinates.
    //
    // Return Values
    //   If you specify MENU_TRACK_RETURNCMD in the 'trackFlags' parameter, the return
    //   value is the menu-item identifier of the item that the user selected. If the
    //   user cancels the menu without making a selection, or if an error occurs, then
    //   the return value is zero.
    //
    //   If you do not specify MENU_TRACK_RETURNCMD in the 'trackFlags' parameter, the
    //   return value is nonzero if the function succeeds and zero if it fails.
    //
    virtual DWORD WINAPI Track(DWORD trackFlags,
                               int x,
                               int y,
                               HWND hwnd,
                               const RECT* exclude) = 0;

    //
    // GetItemRect
    //   Retrieves the bounding rectangle of a item in the menu.
    //
    // Parameters
    //   'index'
    //      [in] Zero-based index of the item for which to retrieve information.
    //
    //   'rect'
    //      [out] Address of a RECT structure that receives the screen coordinates
    //      of the bounding rectangle.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetItemRect(int index,
                                    RECT* rect) = 0;

    //
    // UpdateItemsState
    //   Updates state for all items with specified 'Enabler'.
    //   Call this method when enabler variables altered.
    //
    virtual void WINAPI UpdateItemsState() = 0;

    //
    // SetMinWidth
    //   Sets the minimum width to be used for menu popup.
    //
    // Parameters
    //   'minWidth'
    //      [in] Specifies the minimum width of the menu popup.
    //
    virtual void WINAPI SetMinWidth(int minWidth) = 0;

    //
    // SetPopupID
    //   Sets the ID for menu popup.
    //
    // Parameters
    //   'id'
    //      [in] Specifies the ID of the menu popup.
    //
    virtual void WINAPI SetPopupID(DWORD id) = 0;

    //
    // GetPopupID
    //   Retrieves the ID for menu popup.
    //
    // Return Values
    //   Returns the ID for menu popup.
    //
    virtual DWORD WINAPI GetPopupID() = 0;

    //
    // AssignHotKeys
    //   Automatically assigns hot keys to the menu items that
    //   has not hot key already assigned.
    //
    virtual void WINAPI AssignHotKeys() = 0;
};

//
// ****************************************************************************
// CGUIMenuBarAbstract
//

class CGUIMenuBarAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
public:
    //
    // CreateWnd
    //   Creates child toolbar window.
    //
    // Parameters
    //   'hParent'
    //      [in] Handle to the parent or owner window of the toolbar being created.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI CreateWnd(HWND hParent) = 0;

    //
    // GetHWND
    //   Retrieves Windows HWND value of the toolbar.
    //
    // Return Values
    //   The Windows HWND handle of the toolbar.
    //
    virtual HWND WINAPI GetHWND() = 0;

    //
    // GetNeededWidth
    //   Retrieves the total width of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed width for the toolbar.
    //
    // See Also
    //   GetNeededHeight
    //
    virtual int WINAPI GetNeededWidth() = 0;

    //
    // GetNeededHeight
    //   Retrieves the total height of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed height for the toolbar.
    //
    // See Also
    //   GetNeededWidth
    //
    virtual int WINAPI GetNeededHeight() = 0;

    //
    // SetFont
    //   Updates the font that a menubar is to use when drawing text.
    //   Call this method after receiving PLUGINEVENT_SETTINGCHANGE through CPluginInterface::Event().
    //
    virtual void WINAPI SetFont() = 0;

    //
    // GetItemRect
    //   Retrieves the bounding rectangle of a button in the toolbar.
    //
    // Parameters
    //   'index'
    //      [in] Zero-based index of the button for which to retrieve information.
    //
    //   'r'
    //      [out] Address of a RECT structure that receives the screen coordinates
    //      of the bounding rectangle.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetItemRect(int index, RECT& r) = 0;

    // prepne menu do Menu mode (jako by user stisknul a pustil Alt)
    virtual void WINAPI EnterMenu() = 0;
    // returns TRUE if the menu is in Menu mode
    virtual BOOL WINAPI IsInMenuLoop() = 0;
    // prepina menu do Help mode (Shift + F1)
    virtual void WINAPI SetHelpMode(BOOL helpMode) = 0;

    //
    // IsMenuBarMessage
    //   The IsMenuBarMessage method determines whether a message is intended for
    //   the menubar or menu and, if it is, processes the message.
    //
    // Parameters
    //   'lpMsg'
    //      [in] Pointer to an MSG structure that contains the message to be checked.
    //
    // Return Values
    //   If the message has been processed, the return value is nonzero.
    //   If the message has not been processed, the return value is zero.
    //
    virtual BOOL WINAPI IsMenuBarMessage(CONST MSG* lpMsg) = 0;
};

//
// ****************************************************************************
// CGUIToolBarAbstract
//

// Toolbar Styles

#define TLB_STYLE_IMAGE 0x00000001      // icons with ImageIndex != -1 are displayed \
                                        // GetNeededSpace also accounts for icon height
#define TLB_STYLE_TEXT 0x00000002       // text is displayed for items with TLBI_STYLE_SHOWTEXT set \
                                        // GetNeededSpace also accounts for font size
#define TLB_STYLE_ADJUSTABLE 0x00000004 // can the toolbar be customized?
#define TLB_STYLE_VERTICAL 0x00000008   // buttons are arranged vertically, separators horizontally; incompatible with TLB_STYLE_TEXT, \
                                        // because vertical text is not supported

// Toolbar Item Masks
#define TLBI_MASK_ID 0x00000001         // Retrieves or sets the 'ID' member.
#define TLBI_MASK_CUSTOMDATA 0x00000002 // Retrieves or sets the 'CustomData' member.
#define TLBI_MASK_IMAGEINDEX 0x00000004 // Retrieves or sets the 'ImageIndex' member.
#define TLBI_MASK_ICON 0x00000008       // Retrieves or sets the 'HIcon' member.
#define TLBI_MASK_STATE 0x00000010      // Retrieves or sets the 'State' member.
#define TLBI_MASK_TEXT 0x00000020       // Retrieves or sets the 'Text' member.
#define TLBI_MASK_TEXTLEN 0x00000040    // Retrieves the 'TextLen' member.
#define TLBI_MASK_STYLE 0x00000080      // Retrieves or sets the 'Style' member.
#define TLBI_MASK_WIDTH 0x00000100      // Retrieves or sets the 'Width' member.
#define TLBI_MASK_ENABLER 0x00000200    // Retrieves or sets the 'Enabler' member.
#define TLBI_MASK_OVERLAY 0x00000800    // Retrieves or sets the 'HOverlay' member.

// Toolbar Item Styles
#define TLBI_STYLE_CHECK 0x00000001 // Creates a dual-state push button that toggles between \
                                    // the pressed and nonpressed states each time the user \
                                    // clicks it. The button has a different background color \
                                    // when it is in the pressed state.

#define TLBI_STYLE_RADIO 0x00000002 // If TLBI_STATE_CHECKED is not set when clicked, it switches to \
                                    // that state. If it is already set, it stays there.

#define TLBI_STYLE_DROPDOWN 0x00000004 // Creates a drop-down style button that can display a \
                                       // list when the button is clicked. Instead of the \
                                       // WM_COMMAND message used for normal buttons, \
                                       // drop-down buttons send a WM_USER_TBDROPDOWN notification. \
                                       // An application can then have the notification handler \
                                       // display a list of options.

#define TLBI_STYLE_NOPREFIX 0x00000008 // Specifies that the button text will not have an \
                                       // accelerator prefix associated with it.

#define TLBI_STYLE_SEPARATOR 0x00000010 // Creates a separator, providing a small gap between \
                                        // button groups. A button that has this style does not \
                                        // receive user input.

#define TLBI_STYLE_SHOWTEXT 0x00000020 // Specifies that button text should be displayed. \
                                       // All buttons can have text, but only those buttons \
                                       // with the BTNS_SHOWTEXT button style will display it. \
                                       // This style must be used with the TLB_STYLE_TEXT style.

#define TLBI_STYLE_WHOLEDROPDOWN 0x00000040 // Specifies that the button will have a drop-down arrow, \
                                            // but not as a separate section.

#define TLBI_STYLE_SEPARATEDROPDOWN 0x00000080 // Specifies that the button will have a drop-down arrow, \
                                               // in separated section.

#define TLBI_STYLE_FIXEDWIDTH 0x00000100 // the width of this item is not calculated automatically.

// Toolbar Item States
#define TLBI_STATE_CHECKED 0x00000001         // The button has the TLBI_STYLE_CHECK style and is being clicked.
#define TLBI_STATE_GRAYED 0x00000002          // The button is grayed and cannot receive user input.
#define TLBI_STATE_PRESSED 0x00000004         // The button is being clicked.
#define TLBI_STATE_DROPDOWNPRESSED 0x00000008 // The drop down is being clicked.

struct TLBI_ITEM_INFO2
{
    DWORD Mask;
    DWORD Style;
    DWORD State;
    DWORD ID;
    char* Text;
    int TextLen;
    int Width;
    int ImageIndex;
    HICON HIcon;
    HICON HOverlay;
    DWORD CustomData; // FIXME_X64 - too small for a pointer; is it ever needed?
    DWORD* Enabler;

    DWORD Index;
    char* Name;
    int NameLen;
};

/*
Mask
  TLBI_MASK_*

Style
  TLBI_STYLE_*

State
  TLBI_STATE_*

ID
  Command identifier associated with the button.
  This identifier is used in a WM_COMMAND message when the button is chosen.

Text
  Text string displayed in the toolbar item.

TextLen
  Length of the toolbar item text, when information is received.

Width
  Width of the toolbar item text.

ImageIndex
  Zero-based index of the button image in the image list.

HIcon
  Handle to the icon to display instead of image list image.
  Icon will not be destroyet.

CustomData
  Application-defined value associated with the toolbar item.

Enabler
  Pointer to the item enabler. Used in the UpdateItemsState.
  0 -> item is TLBI_STATE_GRAYED; else item is enabled

Index
  For enumeration items in customize dialog.

Name
  Name in customize dialog.

NameLen
  Name len in customize dialog.
*/

struct TOOLBAR_PADDING // The padding values are used to create a blank area
{
    WORD ToolBarVertical; // blank area above and below the button
    WORD ButtonIconText;  // blank area between icon and text
    WORD IconLeft;        // blank area before icon
    WORD IconRight;       // blank area behind icon
    WORD TextLeft;        // blank area before text
    WORD TextRight;       // blank area behind text
};

struct TOOLBAR_TOOLTIP
{
    HWND HToolBar;    // window that requests the tooltip
    DWORD ID;         // ID of the button whose tooltip is requested
    DWORD Index;      // index of the button whose tooltip is requested
    DWORD CustomData; // custom data of the button, if defined // FIXME_X64 - too small for a pointer; is it ever needed?
    char* Buffer;     // this buffer must be filled; the maximum number of characters is TOOLTIP_TEXT_MAX
                      // by default, the message is terminated at the first character
};

class CGUIToolBarAbstract
{
    // All methods may be called only from the thread of the parent window to whose Windows control
    // this object was attached and from which the pointer to this interface was obtained.
public:
    //
    // CreateWnd
    //   Creates child toolbar window.
    //
    // Parameters
    //   'hParent'
    //      [in] Handle to the parent or owner window of the toolbar being created.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI CreateWnd(HWND hParent) = 0;

    //
    // GetHWND
    //   Retrieves Windows HWND value of the toolbar.
    //
    // Return Values
    //   The Windows HWND handle of the toolbar.
    //
    virtual HWND WINAPI GetHWND() = 0;

    //
    // GetNeededWidth
    //   Retrieves the total width of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed width for the toolbar.
    //
    // See Also
    //   GetNeededHeight
    //
    virtual int WINAPI GetNeededWidth() = 0;

    //
    // GetNeededHeight
    //   Retrieves the total height of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed height for the toolbar.
    //
    // See Also
    //   GetNeededWidth
    //
    virtual int WINAPI GetNeededHeight() = 0;

    //
    // SetFont
    //   Updates the font that a menubar is to use when drawing text.
    //   Call this method after receiving PLUGINEVENT_SETTINGCHANGE through CPluginInterface::Event().
    //
    virtual void WINAPI SetFont() = 0;

    //
    // GetItemRect
    //   Retrieves the bounding rectangle of a button in the toolbar.
    //
    // Parameters
    //   'index'
    //      [in] Zero-based index of the button for which to retrieve information.
    //
    //   'r'
    //      [out] Address of a RECT structure that receives the screen coordinates
    //      of the bounding rectangle.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetItemRect(int index, RECT& r) = 0;

    // CheckItem
    //   Sets the state of the specified button's attribute to either checked or normal.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'checked'
    //      [in] Indicates whether the button will be checked or cleared.
    //      If this parameter is TRUE, sets the button to the checked state.
    //      If this parameter is FALSE, sets the button to the normal state.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI CheckItem(DWORD position,
                                  BOOL byPosition,
                                  BOOL checked) = 0;

    //
    // EnableItem
    //   Enables or disables the specified button.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to be enabled or disabled.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'enabled'
    //      [in] Indicates whether the button will be enabled or disabled.
    //      If this parameter is TRUE, enables the button.
    //      If this parameter is FALSE, disables the button.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   CheckItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI EnableItem(DWORD position,
                                   BOOL byPosition,
                                   BOOL enabled) = 0;

    //
    // ReplaceImage
    //   Replaces an existing bitmap with a new bitmap.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'hIcon'
    //      [in] Handle to the icon that contains the bitmap and mask for the new image.
    //
    //   'normal'
    //      [in] Specifies whether to replace normal image list icon.
    //
    //   'hot'
    //      [in] Specifies whether to replace hot image list icon.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI ReplaceImage(DWORD position,
                                     BOOL byPosition,
                                     HICON hIcon,
                                     BOOL normal,
                                     BOOL hot) = 0;

    //
    // FindItemPosition
    //   Retrieves the button position in the toolbar.
    //
    // Parameters
    //   'id'
    //      [in] Specifies the identifier of the button whose position is to be retrieved.
    //
    // Return Values
    //   Zero-based position of the specified button.
    //   If button is not found, return value is -1.
    //
    virtual int WINAPI FindItemPosition(DWORD id) = 0;

    //
    // SetImageList
    //   Sets the image list that the toolbar will use to display images in the button
    //   that are in their default state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //
    // See Also
    //   GetImageList, GetHotImageList, SetHotImageList
    //
    virtual void WINAPI SetImageList(HIMAGELIST hImageList) = 0;

    //
    // GetImageList
    //   Retrieves the image list that a toolbar uses to display buttons
    //   in their default state.
    //
    // Return Values
    //   Returns the handle to the image list, or NULL if no image list is set.
    //
    // See Also
    //   SetImageList, GetHotImageList, SetHotImageList
    //
    virtual HIMAGELIST WINAPI GetImageList() = 0;

    //
    // SetHotImageList
    //   Sets the image list that the toolbar will use to display images in the button
    //   that are in their hot state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //
    // See Also
    //   SetImageList, GetImageList, SetHotImageList
    //
    virtual void WINAPI SetHotImageList(HIMAGELIST hImageList) = 0;

    //
    // GetHotImageList
    //   Retrieves the image list that a toolbar uses to display hot buttons.
    //
    // Return Values
    //   Returns the handle to the image list that the toolbar uses to display hot
    //   buttons, or NULL if no hot image list is set.
    //
    // See Also
    //   SetImageList, GetImageList, SetHotImageList
    //
    virtual HIMAGELIST WINAPI GetHotImageList() = 0;

    //
    // SetStyle
    //   Sets the styles for the toolbar.
    //
    // Parameters
    //   'style'
    //      [in] Value specifying the styles to be set for the toolbar.
    //      This parameter can be a combination of TLB_STYLE_* styles.
    //
    // See Also
    //   GetStyle
    //
    virtual void WINAPI SetStyle(DWORD style) = 0;

    //
    // GetStyle
    //   Retrieves the styles currently in use for the toolbar.
    //
    // Return Values
    //   Returns a DWORD value that is a combination of TLB_STYLE_* styles.
    //
    // See Also
    //   SetStyle
    //
    virtual DWORD WINAPI GetStyle() = 0;

    //
    // RemoveItem
    //   Deletes a button from the toolbar.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to delete.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   RemoveAllItems
    //
    virtual BOOL WINAPI RemoveItem(DWORD position,
                                   BOOL byPosition) = 0;

    //
    // RemoveAllItems
    //   Deletes all buttons from the toolbar.
    //
    // See Also
    //   RemoveItem
    //
    virtual void WINAPI RemoveAllItems() = 0;

    //
    // GetItemCount
    //   Retrieves a count of the buttons currently in the toolbar.
    //
    // Return Values
    //   Returns the count of the buttons.
    //
    virtual int WINAPI GetItemCount() = 0;

    //
    // Customize
    //   Displays the Customize Toolbar dialog box.
    //
    virtual void WINAPI Customize() = 0;

    //
    // SetPadding
    //   Sets the padding for a toolbar control.
    //
    // Parameters
    //   'padding'
    //      [in] Address of a TOOLBAR_PADDING structure that contains
    //      the new padding information.
    //
    // See Also
    //   GetPadding
    //

    virtual void WINAPI SetPadding(const TOOLBAR_PADDING* padding) = 0;

    //
    // GetPadding
    //   Retrieves the padding for the toolbar.
    //
    // Parameters
    //   'padding'
    //      [out] Address of a TOOLBAR_PADDING structure that will receive
    //      the padding information.
    //
    // See Also
    //   SetPadding
    //
    virtual void WINAPI GetPadding(TOOLBAR_PADDING* padding) = 0;

    //
    // UpdateItemsState
    //   Updates state for all items with specified 'Enabler'.
    //   Call this method when enabler variables altered.
    //
    virtual void WINAPI UpdateItemsState() = 0;

    //
    // HitTest
    //   Determines where a point lies in the toolbar.
    //
    // Parameters
    //   'xPos'
    //      [in] The x-coordinate of the hit test.
    //
    //   'yPos'
    //      [in] The y-coordinate of the hit test.
    //
    // Return Values
    //   Returns an integer value. If the return value is zero or a positive value,
    //   it is the zero-based index of the nonseparator item in which the point lies.
    //   If the return value is negative, the point does not lie within a button.
    //
    // Remarks
    //   The coordinates are relative to the toolbar's client area.
    //
    virtual int WINAPI HitTest(int xPos,
                               int yPos) = 0;

    //
    // InsertMarkHitTest
    //   Retrieves the insertion mark information for a point in the toolbar.
    //
    // Parameters
    //   'xPos'
    //      [in] The x-coordinate of the hit test.
    //
    //   'yPos'
    //      [in] The y-coordinate of the hit test.
    //
    //   'index'
    //      [out] Zero-based index of the button with insertion mark.
    //      If this member is -1, there is no insertion mark.
    //
    //   'after'
    //      [out] Defines where the insertion mark is in relation to 'index'.
    //      If the value is FALSE, the insertion mark is to the left of the specified button.
    //      If the value is TRUE, the insertion mark is to the right of the specified button.
    //
    // Return Values
    //   Returns TRUE if the point is an insertion mark, or FALSE otherwise.
    //
    // Remarks
    //   The coordinates are relative to the toolbar's client area.
    //
    // See Also
    //   SetInsertMark
    //
    virtual BOOL WINAPI InsertMarkHitTest(int xPos,
                                          int yPos,
                                          int& index,
                                          BOOL& after) = 0;

    //
    // SetInsertMark
    //   Sets the current insertion mark for the toolbar.
    //
    // Parameters
    //   'index'
    //      [out] Zero-based index of the button with insertion mark.
    //      If this member is -1, there is no insertion mark.
    //
    //   'after'
    //      [out] Defines where the insertion mark is in relation to 'index'.
    //      If the value is FALSE, the insertion mark is to the left of the specified button.
    //      If the value is TRUE, the insertion mark is to the right of the specified button.
    //
    // See Also
    //   InsertMarkHitTest
    //
    virtual void WINAPI SetInsertMark(int index,
                                      BOOL after) = 0;

    //
    // SetHotItem
    //   Sets the hot item in the toolbar.
    //
    // Parameters
    //   'index'
    //      [out] Zero-based index of the item that will be made hot.
    //      If this value is -1, none of the items will be hot.
    //
    // Return Values
    //   Returns the index of the previous hot item, or -1 if there was no hot item.
    //
    virtual int WINAPI SetHotItem(int index) = 0;

    //
    // SetHotItem
    //   Updates the toolbar graphic handles.
    //
    virtual void WINAPI OnColorsChanged() = 0;

    //
    // InsertItem2
    //   Inserts a new button at the specified position in a toolbar.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button before which to insert the new button.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'tii'
    //      [in] Pointer to a TLBI_ITEM_INFO2 structure that contains information about the
    //      new button.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   SetItemInfo2, GetItemInfo2
    virtual BOOL WINAPI InsertItem2(DWORD position,
                                    BOOL byPosition,
                                    const TLBI_ITEM_INFO2* tii) = 0;

    //
    // SetItemInfo2
    //   Changes information about a button.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'mii'
    //      [in] Pointer to a TLBI_ITEM_INFO2 structure that contains information about the button
    //      and specifies which button attributes to change.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem2, GetItemInfo2
    virtual BOOL WINAPI SetItemInfo2(DWORD position,
                                     BOOL byPosition,
                                     const TLBI_ITEM_INFO2* tii) = 0;

    //
    // GetItemInfo2
    //   Retrieves information about a button.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to get information about.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'mii'
    //      [in/out] Pointer to a TLBI_ITEM_INFO2 structure that contains information to retrieve
    //      and receives information about the button.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem2, SetItemInfo2
    virtual BOOL WINAPI GetItemInfo2(DWORD position,
                                     BOOL byPosition,
                                     TLBI_ITEM_INFO2* tii) = 0;
};

//
// ****************************************************************************
// CGUIIconListAbstract
//
// Nas interni 32-bitovy image list. 8 bitu na kazdy RGB kanal a 8 bitu alfa transparence.

class CGUIIconListAbstract
{
public:
    // Creates an image list with icon size 'imageWidth' x 'imageHeight' and 'imageCount' icons.
    // The image list must then be filled by calling ReplaceIcon().
    // Returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI Create(int imageWidth, int imageHeight, int imageCount) = 0;

    // Creates the image list from the supplied Windows image list ('hIL'); 'requiredImageSize' specifies
    // the icon size; if it is -1, the size from 'hIL' is used; returns TRUE on success,
    // otherwise FALSE
    virtual BOOL WINAPI CreateFromImageList(HIMAGELIST hIL, int requiredImageSize) = 0;

    // Creates the image list from the specified PNG resource; 'hInstance' and 'lpBitmapName' specify the resource,
    // 'imageWidth' specifies the width of one icon in pixels; returns TRUE on success, otherwise FALSE
    // Note: the PNG must be a one-row strip of icons.
    // Note: it is recommended to compress the PNG with PNGSlim; see https://forum.altap.cz/viewtopic.php?f=15&t=3278
    virtual BOOL WINAPI CreateFromPNG(HINSTANCE hInstance, LPCTSTR lpBitmapName, int imageWidth) = 0;

    // Replaces the icon at the given index with 'hIcon'; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI ReplaceIcon(int index, HICON hIcon) = 0;

    // Creates an icon from the specified index and returns its handle; the caller is responsible for destroying it
    // (call DestroyIcon(hIcon)); returns NULL on failure
    virtual HICON WINAPI GetIcon(int index) = 0;

    // Creates the image list from PNG data supplied in memory; 'rawPNG' is a pointer to memory containing the PNG
    // (for example loaded from a file) and 'rawPNGSize' specifies the size of the memory occupied by the PNG in bytes,
    // 'imageWidth' specifies the width of one icon in pixels; returns TRUE on success, otherwise FALSE
    // Note: the PNG must be a one-row strip of icons
    // Note: it is recommended to compress the PNG with PNGSlim; see https://forum.altap.cz/viewtopic.php?f=15&t=3278
    virtual BOOL WINAPI CreateFromRawPNG(const void* rawPNG, DWORD rawPNGSize, int imageWidth) = 0;

    // is created as a copy of another (created) icon list; if 'grayscale' is TRUE,
    // it is also converted to a black-and-white version; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI CreateAsCopy(const CGUIIconListAbstract* iconList, BOOL grayscale) = 0;

    // Creates an HIMAGELIST and returns its handle, or NULL on failure.
    // The returned image list must be destroyed with the ImageList_Destroy() API after use
    virtual HIMAGELIST WINAPI GetImageList() = 0;
};

//
// ****************************************************************************
// CGUIToolbarHeaderAbstract
//
// Pomocna lista umistena nad seznamem (napr. konfigurace HotPaths, UserMenu),
// ktera muze vpravo obsahovat toolbar s tlacitky pro ovladadni seznamu.
//
// Vsechny metody je mozne volat pouze z threadu okna, ve kterem
// byl objekt pripojen na windows control.
//

// bit masks for EnableToolbar() and CheckToolbar()
#define TLBHDRMASK_MODIFY 0x01
#define TLBHDRMASK_NEW 0x02
#define TLBHDRMASK_DELETE 0x04
#define TLBHDRMASK_SORT 0x08
#define TLBHDRMASK_UP 0x10
#define TLBHDRMASK_DOWN 0x20

// button identifiers for WM_COMMAND; see SetNotifyWindow()
#define TLBHDR_MODIFY 1
#define TLBHDR_NEW 2
#define TLBHDR_DELETE 3
#define TLBHDR_SORT 4
#define TLBHDR_UP 5
#define TLBHDR_DOWN 6
// number of items
#define TLBHDR_COUNT 6

class CGUIToolbarHeaderAbstract
{
public:
    // By default all buttons are enabled; after calling this method, only the buttons matching mask 'enableMask' are enabled,
    // where the mask consists of one or more
    // combined TLBHDRMASK_xxx values
    virtual void WINAPI EnableToolbar(DWORD enableMask) = 0;

    // By default all buttons are unpressed; after calling this method, the buttons matching mask 'checkMask' are pressed,
    // where the mask consists of one or more
    // combined TLBHDRMASK_xxx values
    virtual void WINAPI CheckToolbar(DWORD checkMask) = 0;

    // By calling this method, the caller specifies the 'hWnd' window that receives
    // WM_COMMAND messages from ToolbarHeader; LOWORD(wParam) contains the 'ctrlID' from
    // AttachToolbarHeader(), and LOWORD(wParam) is one of the TLBHDR_xxx values (depending on the button
    // clicked by the user).
    // Note: call this method only in special cases when the messages
    // must be delivered to a window other than the parent window, which receives them by default
    virtual void WINAPI SetNotifyWindow(HWND hWnd) = 0;
};

//
// ****************************************************************************
// CSalamanderGUIAbstract
//

#define BTF_CHECKBOX 0x00000001    // button behaves like a checkbox
#define BTF_DROPDOWN 0x00000002    // button has a drop-down part at the end and sends WM_USER_BUTTONDROPDOWN to the parent
#define BTF_LBUTTONDOWN 0x00000004 // button reacts to LBUTTONDOWN and sends WM_USER_BUTTON
#define BTF_RIGHTARROW 0x00000008  // button has a right-pointing arrow at the end
#define BTF_MORE 0x00000010        // button has the symbol for expanding the dialog at the end

class CSalamanderGUIAbstract
{
public:
    ///////////////////////////////////////////////////////////////////////////
    //
    // ProgressBar
    //
    // Used to display the state of an operation as the percentage of work already done.
    // It is useful for operations that can take a longer time. In that case a
    // progress bar is better than a plain WAIT cursor.
    //
    // Attaches a Salamander progress bar to a Windows control (this control determines the position of the
    // progress bar); 'hParent' is the handle of the parent window (dialog or window); ctrlID is the ID of the
    // Windows control; on successful attachment returns the progress bar interface,
    // otherwise returns NULL; the interface remains valid until the Windows control is destroyed (WM_DESTROY
    // is delivered to the Windows control); after attachment the progress bar is set to 0%;
    // it draws its own frame, so do not assign SS_WHITEFRAME | WS_BORDER to the control
    virtual CGUIProgressBarAbstract* WINAPI AttachProgressBar(HWND hParent, int ctrlID) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // StaticText
    //
    // Used to display nonstandard text in a dialog (bold, underlined),
    // text that changes quickly and would flicker, or text with unpredictable length
    // that must be shortened intelligently.
    //
    // Attaches Salamander StaticText to a Windows control (this control determines the position of the
    // StaticText); 'hParent' is the handle of the parent window (dialog or window); ctrlID is the ID;
    // 'flags' is from the STF_* family; it can be 0 or any combination of values;
    // on successful attachment returns the StaticText interface,
    // otherwise returns NULL; the interface remains valid until the Windows control is destroyed (WM_DESTROY)
    // is delivered to the Windows control; after attachment, the text and alignment are taken from the Windows control.
    // Tested on the Windows "STATIC" control
    virtual CGUIStaticTextAbstract* WINAPI AttachStaticText(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // HyperLink
    //
    // Used to display a hyperlink. Clicking it can open a URL
    // or launch a file (SetActionOpen), or post a command back
    // to the dialog (SetActionPostCommand).
    // The control is accessible with the TAB key (it can take focus), but it must have
    // WS_TABSTOP set. The action is then invoked with the Space key. Right-clicking
    // or Shift+F10 can open a menu with an option to copy the control text
    // to the clipboard.
    //
    // Attaches Salamander HyperLink to a Windows control (this control determines the position of the
    // HyperLink); 'hParent' is the handle of the parent window (dialog or window); ctrlID is the ID of the Windows control;
    // 'flags' is from the STF_* family; it can be 0 or any combination of values;
    // the recommended combination for HyperLink is STF_UNDERLINE | STF_HYPERLINK_COLOR
    // on successful attachment returns the HyperLink interface, otherwise returns NULL; the interface is
    // valid until the Windows control is destroyed (WM_DESTROY is delivered); after attachment,
    // the text and alignment are taken from the Windows control.
    // Tested on the Windows "STATIC" control
    virtual CGUIHyperLinkAbstract* WINAPI AttachHyperLink(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // Button
    //
    // Creates a button with text or an icon. The button can contain a right arrow
    // or a drop-down arrow. See the BTF_xxx flags.
    //
    // Attaches a Salamander TextArrowButton to a Windows control (that control determines the position,
    // text or icon, and the generated command); 'hParent' is the handle of the parent window (dialog or window);
    // ctrlID is the ID of the Windows control;
    // on successful attachment, returns the CGUIButtonAbstract interface, otherwise returns NULL; the interface is
    // valid until the Windows control is destroyed (WM_DESTROY is delivered);
    // Tested on the Windows "BUTTON" control.
    virtual CGUIButtonAbstract* WINAPI AttachButton(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ColorArrowButton
    //
    // Creates a button with a colored rectangle followed by a right-pointing arrow.
    // (if showArrow==TRUE)
    // The rectangle displays text that can have a different color than the rectangle background.
    // It is used in color configurations, where it can display one or two colors.
    // Clicking it opens a popup menu with options for choosing which color is being configured.
    //
    // Attaches a Salamander ColorArrowButton to a Windows control (this control determines the position,
    // text, and command of the ColorArrowButton); 'hParent' is the handle of the parent window (dialog or window);
    // ctrlID is the ID of the Windows control;
    // if attachment succeeds, returns the ColorArrowButton interface, otherwise returns NULL; the interface is
    // valid until the Windows control is destroyed (WM_DESTROY is delivered);
    // Tested on the Windows "BUTTON" control.
    virtual CGUIColorArrowButtonAbstract* WINAPI AttachColorArrowButton(HWND hParent, int ctrlID, BOOL showArrow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ChangeToArrowButton
    //
    // Creates a button with a right-pointing arrow centered on the button.
    // It is placed after an edit box, and clicking it opens a popup menu next to the button
    // with items that can be inserted into the edit box (a form of hint).
    //
    // Changes the button style so it can hold the arrow icon and then assigns the icon.
    // It does not attach any Salamander object to the control because
    // all handling is provided by the operating system. Returns TRUE on success, otherwise FALSE.
    // The button text is ignored.
    // Tested on the Windows "BUTTON" control.
    virtual BOOL WINAPI ChangeToArrowButton(HWND hParent, int ctrlID) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // MenuPopup
    //
    // Creates an empty popup menu. Returns an interface pointer or
    // NULL on failure.
    virtual CGUIMenuPopupAbstract* WINAPI CreateMenuPopup() = 0;
    // releases the allocated menu.
    virtual BOOL WINAPI DestroyMenuPopup(CGUIMenuPopupAbstract* popup) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // MenuBar
    //
    // Creates a menu bar; the items in 'menu' are shown in the menu bar,
    // and their children become submenus; 'hNotifyWindow' identifies the window
    // that receives commands and notifications. Returns an interface pointer or NULL
    // on failure.
    virtual CGUIMenuBarAbstract* WINAPI CreateMenuBar(CGUIMenuPopupAbstract* menu, HWND hNotifyWindow) = 0;
    // releases the allocated menu bar. Also destroys the window.
    virtual BOOL WINAPI DestroyMenuBar(CGUIMenuBarAbstract* menuBar) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolBar support
    //
    // Used to create a disabled (gray) version of bitmaps for a menu or toolbar.
    // From source bitmap 'hSource' it creates grayscale bitmap 'hGrayscale'
    // and black-and-white mask 'hMask'. Color 'transparent' is treated as transparent.
    // On success it returns TRUE and outputs 'hGrayscale' and 'hMask'; on error it returns FALSE.
    virtual BOOL WINAPI CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                                      HBITMAP& hGrayscale, HBITMAP& hMask) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolBar
    //
    // Used to create a toolbar; 'hNotifyWindow' identifies the window to which
    // commands and notifications are sent. Returns an interface pointer or NULL
    // on failure.
    virtual CGUIToolBarAbstract* WINAPI CreateToolBar(HWND hNotifyWindow) = 0;
    // releases the allocated toolbar. Also destroys the window.
    virtual BOOL WINAPI DestroyToolBar(CGUIToolBarAbstract* toolBar) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolTip
    //
    // This method starts a timer and, if it is not called again before it expires,
    // asks window 'hNotifyWindow' for the text by sending message WM_USER_TTGETTEXT,
    // which it then shows below the cursor at its current coordinates.
    // Variable 'id' is used to distinguish areas when communicating with window 'hNotifyWindow'.
    // If this method is called several times with the same 'id' parameter,
    // those additional calls are ignored.
    // Value 0 of parameter 'hNotifyWindow' is reserved for hiding the window and stopping
    // the running timer.
    virtual void WINAPI SetCurrentToolTip(HWND hNotifyWindow, DWORD id) = 0;
    // Suppresses tooltip display at the current mouse coordinates
    // Useful to call when activating a window that uses tooltips
    // so that tooltips are not shown unintentionally
    virtual void WINAPI SuppressToolTipOnCurrentMousePos() = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // XP Visual Styles
    //
    // When called on an operating system that supports visual styles,
    // it calls SetWindowTheme(hWindow, L" ", L" ") to disable visual styles
    // for window 'hWindow'
    // returns TRUE if the operating system supports visual styles, otherwise FALSE
    virtual BOOL WINAPI DisableWindowVisualStyles(HWND hWindow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // IconList
    //
    // Two methods for allocating and destroying the IconList object used to hold
    // 32bpp icons (3 x 8 bits for color and 8 bits for alpha transparency)
    // For other operations on IconList, see the description of CGUIIconListAbstract
    virtual CGUIIconListAbstract* WINAPI CreateIconList() = 0;
    virtual BOOL WINAPI DestroyIconList(CGUIIconListAbstract* iconList) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolTip support
    //
    // Searches 'buf' for the first occurrence of '\t'. If 'stripHotKey' is TRUE, the
    // string is terminated at that character. Otherwise a space is inserted in its place and the rest
    // of the text is enclosed in parentheses. Buffer 'buf' must be large enough when 'stripHotKey' is FALSE
    // for the text in the buffer to grow by two characters (the parentheses).
    virtual void WINAPI PrepareToolTipText(char* buf, BOOL stripHotKey) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // Subject with file/dir name truncated if needed
    //
    // Sets the text produced by sprintf(, subjectFormatString, fileName) in static control 'subjectWnd'.
    // Format string 'subjectFormatString' must contain exactly one '%s' (at the position where
    // 'fileName' is inserted). If the text would exceed the length of the static control, it is shortened by shortening
    // 'fileName'. It also converts 'fileName' according to SALCFG_FILENAMEFORMAT (so it matches
    // the way 'fileName' is displayed in the panel) using CSalamanderGeneralAbstract::AlterFileName.
    // If it is a file, 'isDir' is FALSE, otherwise TRUE. If static control 'subjectWnd' has SS_NOPREFIX,
    // 'duplicateAmpersands' is FALSE, otherwise TRUE (the second and subsequent ampersands ('&') are doubled; the first
    // ampersand marks the hotkey in the subject and must be present in 'subjectFormatString' before '%s').
    // Example: SetSubjectTruncatedText(GetDlgItem(HWindow, IDS_SUBJECT), "&Rename %s to",
    //                                          file->Name, fileIsDir, TRUE)
    // Can be called from any thread
    virtual void WINAPI SetSubjectTruncatedText(HWND subjectWnd, const char* subjectFormatString, const char* fileName,
                                                BOOL isDir, BOOL duplicateAmpersands) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolbarHeader
    //
    // Used to create a header above a list (either listview or listbox) that contains
    // a text description and a group of buttons on the right side. An example can be seen in Salamander configuration,
    // such as Hot Paths or User Menu. 'hParent' is the handle of the dialog, 'ctrlID' is the ID of the static text,
    // around which the ToolbarHeader is created, 'hAlignWindow' is the handle of the list to which
    // the header is aligned, 'buttonMask' is one or a sum of multiple TLBHDRMASK_xxx values
    // and specifies which buttons are shown in the header.
    virtual CGUIToolbarHeaderAbstract* WINAPI AttachToolbarHeader(HWND hParent, int ctrlID, HWND hAlignWindow, DWORD buttonMask) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ArrangeHorizontalLines
    //
    // Finds horizontal lines in dialog 'hWindow' and extends them on the right to the static text,
    // checkbox, or radio button they connect to. It also finds checkboxes and
    // radio buttons that serve as groupbox labels and shortens them according to their text and the
    // current dialog font. Eliminates unnecessary gaps caused by different
    // screen DPI.
    virtual void WINAPI ArrangeHorizontalLines(HWND hWindow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // GetWindowFontHeight
    //
    // For 'hWindow', gets the current font using WM_GETFONT and returns its height
    // using GetObject()
    virtual int WINAPI GetWindowFontHeight(HWND hWindow) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_gui)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
