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
#pragma pack(push, enter_include_spl_gui) // so that the structures are independent of the alignment setting
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//////////////////////////////////////////////////////////
//                                                      //
// The space WM_APP + 200 to WM_APP + 399 is in const.h //
// excluded from the space used for internal messages   //
// of Salamander.                                       //
//                                                      //
//////////////////////////////////////////////////////////

// menu messages
#define WM_USER_ENTERMENULOOP WM_APP + 200   // [0, 0] entering into menu has occured
#define WM_USER_LEAVEMENULOOP WM_APP + 201   // [0, 0] menu loop mode has been ended; this message is sent before posting command
#define WM_USER_LEAVEMENULOOP2 WM_APP + 202  // [0, 0] menu loop mode has been ended; this message is sent after posting command
#define WM_USER_INITMENUPOPUP WM_APP + 204   // [(CGUIMenuPopupAbstract*)menuPopup, LOWORD(uPos), HIWORD(uID)]
#define WM_USER_UNINITMENUPOPUP WM_APP + 205 // [(CGUIMenuPopupAbstract*)menuPopup, LOWORD(uPos), HIWORD(uID)]
#define WM_USER_CONTEXTMENU WM_APP + 206     // [(CGUIMenuPopupAbstract*)menuPopup, (BOOL)fromMouse \
                                             //  (if there's a mouse action, it's TRUE (use GetMessagePos); \
                                             //   if there's a keyboard action VK_APPS or Shift+F10, it's FALSE)] \
                                             // p.s. if it returns TRUE, the menu command will be executed or submenu will be opened \
                                             // If we want to cast menuPopup to CMenuPopup in Salamander, \
                                             // we use (CMenuPopup*)(CGUIMenuPopupAbstract*)menuPopup.

// toolbar messages
#define WM_USER_TBDROPDOWN WM_APP + 220    // [HWND hToolBar, int buttonIndex]
#define WM_USER_TBRESET WM_APP + 222       // [HWND hToolBar, TOOLBAR_TOOLTIP *tt]
#define WM_USER_TBBEGINADJUST WM_APP + 223 // [HWND hToolBar, 0]
#define WM_USER_TBENDADJUST WM_APP + 224   // [HWND hToolBar, 0]
#define WM_USER_TBGETTOOLTIP WM_APP + 225  // [HWND hToolBar, 0]
#define WM_USER_TBCHANGED WM_APP + 226     // [HWND hToolBar, 0]
#define WM_USER_TBENUMBUTTON2 WM_APP + 227 // [HWND hToolBar, TLBI_ITEM_INFO2 *tii]

// tooltip messages
#define TOOLTIP_TEXT_MAX 5000          // max length of tooltip text (message WM_USER_TTGETTEXT)
#define WM_USER_TTGETTEXT WM_APP + 240 // [ID passed in SetToolTip, buffer limited by TOOLTIP_TEXT_MAX]

// button pressed
#define WM_USER_BUTTON WM_APP + 244 // [(LO)WORD buttonID, (LO)WORD event was caused by keyboard, if we open menu, select first item]

// drop down of button pressed
#define WM_USER_BUTTONDROPDOWN WM_APP + 245 // [(LO)WORD buttonID, (LO)WORD event was caused by keyboard, if we open menu, select first item]

#define WM_USER_KEYDOWN WM_APP + 246 // [(LO)WORD ctrlID, DWORD virtual-key code]

//
// ****************************************************************************
// CGUIProgressBarAbstract
//

class CGUIProgressBarAbstract
{
public:
    // sets progress, optionally text in the middle
    //
    // there is a safer variant SetProgress2(), look at it before using this method
    //
    // progress can work in two modes:
    //   1) for 'progress' >= 0 it is a classic thermometer 0% to 100%
    //      in this mode you can set your own text in the middle using 'text' variable
    //      if 'text' == NULL, standard percent will be displayed in the middle
    //   2) for 'progress' == -1 it is an indefinite state, when a small rectangle moves back and forth
    //      the movement is controlled by methods SetSelfMoveTime(), SetSelfMoveSpeed() and Stop()
    //
    // repainting is done immediately; for most operations it is suitable to store data in the parent
    // dialog into cache and start a 100ms timer, on which this method is called
    //
    // can be called from any thread, the thread with the control must be running, otherwise it will be blocked
    // (SendMessage is used to deliver the value of 'progress' to the control);
    virtual void WINAPI SetProgress(DWORD progress, const char* text) = 0;

    // makes sense in combination with calling SetProgress(-1)
    // specifies how many milliseconds after calling SetProgress(-1) the rectangle will still move by itself
    // if another SetProgress(-1) is called during this time, the time is counted from the beginning again
    // if 'time'==0, the rectangle will only move once when calling SetProgress(-1)
    // for the value 'time'==0xFFFFFFFF the rectangle will move indefinitely (default value)
    virtual void WINAPI SetSelfMoveTime(DWORD time) = 0;

    // makes sense in combination with calling SetProgress(-1)
    // specifies the time between rectangle moves in milliseconds
    // the default value is 'moveTime'==50, which means 20 moves per second
    virtual void WINAPI SetSelfMoveSpeed(DWORD moveTime) = 0;

    // makes sense in combination with calling SetProgress(-1)
    // if the rectangle is currently moving (thanks to SetSelfMoveTime), it will be stopped
    virtual void WINAPI Stop() = 0;

    // sets progress, optionally text in the middle
    //
    // compared to SetProgress() it has an advantage that if 'progressCurrent' >= 'progressTotal',
    // it sets the progress directly: if 'progressTotal' is 0, it sets 0%, otherwise 100% and does not perform calculation
    // (it is nonsensical + RTC yells at it because of the type conversion), this "forbidden" state occurs
    // e.g. when increasing the file during operation or when working with a link to a file - links have
    // zero size, but then there is data about the size of the linked file,
    // if you perform the calculation yourself, it is necessary to handle this "forbidden" state
    //
    // progress can work in two modes (see SetProgress()), this method can only be set in mode 1):
    //   1) it is a classic thermometer 0% to 100%
    //      in this mode you can set your own text in the middle using 'text' variable
    //      if 'text' == NULL, standard percent will be displayed in the middle
    //
    // repainting is done immediately; for most operations it is suitable to store data in the parent
    // dialog into cache and start a 100ms timer, on which this method is called
    //
    // can be called from any thread, the thread with the control must be running, otherwise it will be blocked
    // (SendMessage is used to deliver the value of 'progress' to the control);
    virtual void WINAPI SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                                     const char* text) = 0;

    // usage examples:
    //
    // 1. we want to move the rectangle manually, without our contribution it does not move
    //
    //   SetSelfMoveTime(0)           // disable automatic movement
    //   SetProgress(-1, NULL)        // move one piece
    //   ...
    //   SetProgress(-1, NULL)        // move one piece
    //
    // 2. the rectangle should move by itself and until Stop is called
    //
    //   SetSelfMoveTime(0xFFFFFFFF)  // infinite movement
    //   SetSelfMoveSpeed(50)         // 20 moves per second
    //   SetProgress(-1, NULL)        // start the rectangle
    //   ...                          // we are doing something
    //   Stop()                       // stop the rectangle
    //
    // 3. the rectangle should move for a limited time, after which it will stop
    //    if we "poke" it during this time, the time will be restored
    //
    //   SetSelfMoveTime(1000)        // it moves by itself for a second, then stops
    //   SetSelfMoveSpeed(50)         // 20 moves per second
    //   SetProgress(-1, NULL)        // start the rectangle for one second
    //   ...
    //   SetProgress(-1, NULL)        // revive the rectangle for another second
    //
    // 4. during the operation, there was a pause and we want to visualize it in the progress bar
    //
    //   SetProgress(0, NULL)         // 0%
    //   SetProgress(100, NULL)       // 10%
    //   SetProgress(200, NULL)       // 20%
    //   SetProgress(300, "(paused)") // 30% -- instead of "30 %" the text "(paused)" is displayed
    //   ... (waiting for resume)
    //   SetProgress(300, NULL)       // 30% (text paused) we turn off again and go on
    //   SetProgress(400, NULL)       // 40%
    //   ...
};

//
// ****************************************************************************
// CGUIStaticTextAbstract
//

#define STF_CACHED_PAINT 0x0000000001    // display text will be cached (will not blink) \
                                         // WARNING: display is much slower than without this flag. \
                                         // Do not use in case of text in dialog, which will be displayed \
                                         // once and then remain unchanged. \
                                         // Use for frequently / quickly changing text (performed operation).
#define STF_BOLD 0x0000000002            // bold font will be used for text
#define STF_UNDERLINE 0x0000000004       // text will be underlined (due to poor readability \
                                         // use only for HyperLink and special cases)
#define STF_DOTUNDERLINE 0x0000000008    // text will be dotted underlined (due to poor readability \
                                         // use only for HyperLink and special cases)
#define STF_HYPERLINK_COLOR 0x0000000010 // text color will be determined by hyperlink color
#define STF_END_ELLIPSIS 0x0000000020    // if the text is too long, it will be terminated by "..."
#define STF_PATH_ELLIPSIS 0x0000000040   // if the text is too long, it will be terminated by "..." \
                                         // and the path will be shortened so that the end is visible \
#define STF_HANDLEPREFIX 0x0000000080    // characters after '&' will be underlined; cannot be used with STF_END_ELLIPSIS or STF_PATH_ELLIPSIS

class CGUIStaticTextAbstract
{
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
    //
    // The control can be visited from the keyboard in the dialog if we assign it the style WS_TABSTOP.
public:
    // sets text of the control; calling this method is faster and less computationally expensive
    // than setting the text using WM_SETTEXT; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI SetText(const char* text) = 0;

    // returns text of the control; can be called from any thread;
    // returns NULL if SetText has not been called yet and the static control was without text
    virtual const char* WINAPI GetText() = 0;

    // sets character for separating parts of the path; it makes sense in case of STF_PATH_ELLIPSIS;
    // it is set to '\\' by default;
    virtual void WINAPI SetPathSeparator(char separator) = 0;

    // assigns text which will be displayed as a tooltip
    // returns TRUE if it was possible to allocate a copy of the text, otherwise FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // assigns window and id to which WM_USER_TTGETTEXT will be sent when displaying tooltip
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIHyperLinkAbstract
//

class CGUIHyperLinkAbstract
{
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
    //
    // The control can be visited from the keyboard in the dialog if we assign it the style WS_TABSTOP.
public:
    // sets text of the control; calling this method is faster and less computationally expensive
    // than setting the text using WM_SETTEXT; returns TRUE on success, otherwise FALSE
    virtual BOOL WINAPI SetText(const char* text) = 0;

    // returns text of the control; can be called from any thread;
    // returns NULL if SetText has not been called yet and the static control was without text
    virtual const char* WINAPI GetText() = 0;

    // assigns opening URL address action (file="https://www.altap.cz") or
    // running program (file="C:\\TEST.EXE"); ShellExecute is called with 'open' command on the parameter
    virtual void WINAPI SetActionOpen(const char* file) = 0;

    // assigns PostCommand(WM_COMMAND, command, 0) action to parent window
    virtual void WINAPI SetActionPostCommand(WORD command) = 0;

    // assigns displaying hint and tooltip 'text' action
    // if text is NULL, it is possible to assign tooltip by calling method
    // SetToolTipText or SetToolTip; method then always returns TRUE
    // if text is different from NULL, the method returns TRUE if it was possible
    // to allocate a copy of the text, otherwise returns FALSE
    // tooltip can be displayed by pressing Space/Up/Down (if focus is on the control)
    // and by clicking the mouse; the hint (tooltip) is then displayed directly
    // below the text and does not close until the user clicks outside it with the mouse
    // or presses a key
    virtual BOOL WINAPI SetActionShowHint(const char* text) = 0;

    // assigns text which will be displayed as a tooltip
    // returns TRUE if it was possible to allocate a copy of the text, otherwise FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // assigns window and id to which WM_USER_TTGETTEXT will be sent when displaying tooltip
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIButtonAbstract
//

class CGUIButtonAbstract
{
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
public:
    // assigns text which will be displayed as a tooltip; returns TRUE in case of success, otherwise FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // assigns window and id to which WM_USER_TTGETTEXT will be sent when displaying tooltip
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIColorArrowButtonAbstract
//

class CGUIColorArrowButtonAbstract
{
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
public:
    // sets color of the text 'textColor' and background color 'bkgndColor'
    virtual void WINAPI SetColor(COLORREF textColor, COLORREF bkgndColor) = 0;

    // sets color of the text 'textColor'
    virtual void WINAPI SetTextColor(COLORREF textColor) = 0;

    // sets background color 'bkgndColor'
    virtual void WINAPI SetBkgndColor(COLORREF bkgndColor) = 0;

    // returns color of the text
    virtual COLORREF WINAPI GetTextColor() = 0;

    // returns background color
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
    int TextResID;    // resource of text
    BYTE SkillLevel;  // MNTS_*
    DWORD ID;         // generated command
    short ImageIndex; // -1 = no icon
    DWORD State;
    DWORD* Enabler; // control variable for enabling item
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
#define MENU_POPUP_UPDATESTATES 0x00000002 // pred otevrenim se zavola UpdateStates

// following flags are modified during the branch for individual popups
#define MENU_TRACK_SELECT 0x00000001 // If this flag is set, the function select item specified by SetSelectedItemIndex.
//#define MENU_TRACK_LEFTALIGN    0x00000000 // If this flag is set, the function positions the shortcut menu so that its left side is aligned with the coordinate specified by the x parameter.
//#define MENU_TRACK_TOPALIGN     0x00000000 // If this flag is set, the function positions the shortcut menu so that its top side is aligned with the coordinate specified by the y parameter.
//#define MENU_TRACK_HORIZONTAL   0x00000000 // If the menu cannot be shown at the specified location without overlapping the excluded rectangle, the system tries to accommodate the requested horizontal alignment before the requested vertical alignment.
#define MENU_TRACK_CENTERALIGN 0x00000002  // If this flag is set, the function centers the shortcut menu horizontally relative to the coordinate specified by the x parameter.
#define MENU_TRACK_RIGHTALIGN 0x00000004   // Positions the shortcut menu so that its right side is aligned with the coordinate specified by the x parameter.
#define MENU_TRACK_VCENTERALIGN 0x00000008 // If this flag is set, the function centers the shortcut menu vertically relative to the coordinate specified by the y parameter.
#define MENU_TRACK_BOTTOMALIGN 0x00000010  // If this flag is set, the function positions the shortcut menu so that its bottom side is aligned with the coordinate specified by the y parameter.
#define MENU_TRACK_VERTICAL 0x00000100     // If the menu cannot be shown at the specified location without overlapping the excluded rectangle, the system tries to accommodate the requested vertical alignment before the requested horizontal alignment.
// shared flags for one branch of Track
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
  Item type. This variable can have one or more of these values:

   MENU_TYPE_OWNERDRAW    The window that owns the menu is responsible for drawing items.
                          For each menu item, WM_MEASUREITEM and WM_DRAWITEM messages
                          are sent. TypeData variable contains 32-bit value defined by application.

   MENU_TYPE_RADIOCHECK   Checked items are displayed with a dot instead of a check mark,
                          if HBmpChecked is NULL.

   MENU_TYPE_SEPARATOR    Horizontal separator line. TypeData is meaningless.

   MENU_TYPE_STRING       Item contains a text string. TypeData points to a null-terminated
                          string.

   MENU_TYPE_BITMAP       Item contains a bitmap.

   Values MENU_TYPE_BITMAP, MENU_TYPE_SEPARATOR and MENU_TYPE_STRING cannot be used together.

State
  Item state. This variable can have one or more of these values:

   MENU_STATE_CHECKED     Item is checked.

   MENU_STATE_DEFAULT     Menu can contain only one default item. It is displayed bold.

   MENU_STATE_GRAYED      Disables item and grays it so that it cannot be selected.

SkillLevel
 User skill level of item. This variable can have one or more of these values:

   MENU_LEVEL_BEGINNER       Beginner - will be displayed always
   MENU_LEVEL_INTERMEDIATE   Intermediate - will be displayed to advanced and intermediate users
   MENU_LEVEL_ADVANCED       Advanced - will be displayed only to advanced users

ID
  16bit value defined by application, it identifies menu item.

SubMenu
  Pointer to popup menu attached to this item. If this item does not open submenu,
    SubMenu is NULL.

HBmpChecked
  Handle to bitmap displayed before item if item is checked. If this variable is NULL,
  default bitmap is used. If MENU_TYPE_RADIOCHECK is set, default bitmap is dot,
  otherwise check mark. If ImageIndex is not -1, this bitmap is not used.

HBmpUnchecked
    Handle to bitmap displayed before item if item is not checked. If this variable is NULL,
    no bitmap is displayed. If ImageIndex is not -1, this bitmap is not used.

ImageIndex
  Index of bitmap in ImageList CMenuPopup::HImageList. Bitmap is displayed before item
  depending on MENU_STATE_CHECKED and MENU_STATE_GRAYED. If this variable is -1,
  no bitmap is displayed.

Enabler
  Pointer to DWORD variable, which determines state of item: TRUE->enabled, FALSE->grayed.
  If NULL, item is enabled.

*/

class CGUIMenuPopupAbstract
{
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
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
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
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
    // vraci TRUE, pokud je menu v Menu mode
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

#define TLB_STYLE_IMAGE 0x00000001    // icons with ImageIndex != -1 will be displayed \
                                        // and GetNeededSpace will count with icons height
#define TLB_STYLE_TEXT 0x00000002     // will be displayed for items with TLBI_STYLE_SHOWTEXT \
                                        // and GetNeededSpace will count with text height \
#define TLB_STYLE_ADJUSTABLE 0x00000004 // can the toolbar be configured?
#define TLB_STYLE_VERTICAL 0x00000008 // buttons are under each other, separators are horizontal, \
                                        // excludes with TLB_STYLE_HORIZONTAL, \
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

#define TLBI_STYLE_RADIO 0x00000002    // If TLBI_STATE_CHECKED is not set during a click, \
                                    // the button is switched to this state. If it is already \
                                    // in this state, it remains there. \

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

#define TLBI_STYLE_FIXEDWIDTH 0x00000100 // Width of this item is not automatically computed. \

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
    DWORD CustomData; // FIXME_X64 - small for pointer, is it sometimes needed?
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
    HWND HToolBar;    // a window, which the tooltip is asking for
    DWORD ID;         // ID of button, for which the tooltip is requested
    DWORD Index;      // ID of button, for which the tooltip is requested
    DWORD CustomData; // custom data of button, if defined // FIXME_X64 - small for pointer, is it sometimes needed?
    char* Buffer;     // this buffer has to be filled, max number of characters is TOOLTIP_TEXT_MAX
                      // by default, terminator is inserted to the 0th character
};

class CGUIToolBarAbstract
{
    // All methods can be called only from the thread of the parent window in which
    // the object was attached to the windows control and the pointer to this interface was obtained.
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
// Out internal 32-bit image list. 8 bits for each RGB channel and 8 bits for alpha transparency.

class CGUIIconListAbstract
{
public:
    // creates image list with icons of size 'imageWidth' x 'imageHeight' and 'imageCount' number
    // of icons; after calling method ReplaceIcon() the image list needs to be filled;
    // returns TRUE on success, FALSE otherwise
    virtual BOOL WINAPI Create(int imageWidth, int imageHeight, int imageCount) = 0;

    // this is created per specified windows image list ('hIL'); 'requiredImageSize' specifies
    // the size of the icons, if it is -1, the size of the icons in 'hIL' is used; returns TRUE
    // on success, FALSE otherwise
    virtual BOOL WINAPI CreateFromImageList(HIMAGELIST hIL, int requiredImageSize) = 0;

    // this is created per provided PNG resource; 'hInstance' and 'lpBitmapName' specify the resource,
    // 'imageWidth' specifies the width of one icon in points; returns TRUE on success, FALSE otherwise
    // note: PNG must be a row of icons which is one row high
    // note: PNG should be compressed using PNGSlim, see https://forum.altap.cz/viewtopic.php?f=15&t=3278
    virtual BOOL WINAPI CreateFromPNG(HINSTANCE hInstance, LPCTSTR lpBitmapName, int imageWidth) = 0;

    // replaces icon at specified index with icon 'hIcon'; returns TRUE on success, FALSE otherwise
    virtual BOOL WINAPI ReplaceIcon(int index, HICON hIcon) = 0;

    // creates icon from specified index and returns its handle; caller is responsible for destroying
    // the icon (calling DestroyIcon(hIcon)); returns NULL on failure
    virtual HICON WINAPI GetIcon(int index) = 0;

    // this is created per provided PNG in memory; 'rawPNG' is a pointer to memory containing PNG
    // (e.g. loaded from file) and 'rawPNGSize' specifies the size of the PNG in bytes,
    // 'imageWidth' specifies the width of one icon in points; returns TRUE on success, FALSE otherwise
    // note: PNG must be a row of icons which is one row high
    // note: PNG should be compressed using PNGSlim, see https://forum.altap.cz/viewtopic.php?f=15&t=3278
    virtual BOOL WINAPI CreateFromRawPNG(const void* rawPNG, DWORD rawPNGSize, int imageWidth) = 0;

    // this is created as a copy of another (already created) icon list; if 'grayscale' is TRUE,
    // the conversion to grayscale is performed; returns TRUE on success, FALSE otherwise
    virtual BOOL WINAPI CreateAsCopy(const CGUIIconListAbstract* iconList, BOOL grayscale) = 0;

    // creates HIMAGELIST, returns its handle or NULL on failure; the returned image list must be
    // destroyed using ImageList_Destroy() API
    virtual HIMAGELIST WINAPI GetImageList() = 0;
};

//
// ****************************************************************************
// CGUIToolbarHeaderAbstract
//
// Helper bar above the list (e.g. HotPaths, UserMenu configuration), which can
// contain toolbar in the right side, with buttons for controlling the list.
//
// All methods can be called only from the thread of the window in which
// the object was attached to the windows control.
//

// Bit masks for EnableToolbar() and CheckToolbar()
#define TLBHDRMASK_MODIFY 0x01
#define TLBHDRMASK_NEW 0x02
#define TLBHDRMASK_DELETE 0x04
#define TLBHDRMASK_SORT 0x08
#define TLBHDRMASK_UP 0x10
#define TLBHDRMASK_DOWN 0x20

// Identification of buttons for WM_COMMAND, see SetNotifyWindow()
#define TLBHDR_MODIFY 1
#define TLBHDR_NEW 2
#define TLBHDR_DELETE 3
#define TLBHDR_SORT 4
#define TLBHDR_UP 5
#define TLBHDR_DOWN 6
// Number of items
#define TLBHDR_COUNT 6

class CGUIToolbarHeaderAbstract
{
public:
    // by default all buttons are enabled; after calling this method, only buttons
    // specified by 'enableMask' will be enabled. The mask is a combination of
    // one or more (summed) TLBHDRMASK_xxx values
    virtual void WINAPI EnableToolbar(DWORD enableMask) = 0;

    // by default all buttons are unchecked; after calling this method, the buttons
    // specified by 'checkMask' will be checked. The mask is a combination of
    // one or more (summed) TLBHDRMASK_xxx values
    virtual void WINAPI CheckToolbar(DWORD checkMask) = 0;

    // by calling this method, the caller specifies the window 'hWnd' to which
    // WM_COMMAND messages will be sent from ToolbarHeader; LOWORD(wParam) will
    // contain 'ctrlID' from AttachToolbarHeader() call and LOWORD(wParam) will
    // be one of TLBHDR_xxx values (depending on the button, which the user clicked)
    // note: this method should be called only in special situations, when the messages
    // should be sent to a window other than the parent window, where the messages
    // are sent to implicitly
    virtual void WINAPI SetNotifyWindow(HWND hWnd) = 0;
};

//
// ****************************************************************************
// CSalamanderGUIAbstract
//

#define BTF_CHECKBOX 0x00000001    // button behaves like a checkbox
#define BTF_DROPDOWN 0x00000002    // button in the back contains a drop down part, sends WM_USER_BUTTONDROPDOWN message to parent
#define BTF_LBUTTONDOWN 0x00000004 // button reacts to LBUTTONDOWN and sends WM_USER_BUTTON
#define BTF_RIGHTARROW 0x00000008  // button has an arrow at the end, the arrow is pointing to the right
#define BTF_MORE 0x00000010        // button has a symbol to expand a dialog, the symbol is at the end of the button

class CSalamanderGUIAbstract
{
public:
    ///////////////////////////////////////////////////////////////////////////
    //
    // ProgressBar
    //
    // Used to display the progress of an operation in percent.
    // It is useful for operations that can take a long time.
    // Progress is better than a simple WAIT cursor.
    //
    // Attaches Salamander's progress bar to a Windows control (this control
    // determines the position of the progress bar); 'hParent' is the handle
    // of the parent window (dialog or window); 'ctrlID' is the ID of the
    // Windows control; if the attachment is successful, the progress bar
    // interface is returned, otherwise NULL is returned; the interface is
    // valid until the Windows control is destroyed (WM_DESTROY); after
    // attachment, the progress bar is set to 0%; the frame draws in its
    // own mode, so do not assign flags SS_WHITEFRAME | WS_BORDER to the control
    virtual CGUIProgressBarAbstract* WINAPI AttachProgressBar(HWND hParent, int ctrlID) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // StaticText
    //
    // Used to display non-standard text in a dialog (bold, underlined),
    // text that changes quickly and would blink or text with unpredictable
    // length, which needs to be intelligently shortened.
    //
    // Attaches Salamander's StaticText to a Windows control (this control
    // determines the position of the StaticText); 'hParent' is the handle
    // of the parent window (dialog or window); 'ctrlID' is the ID;
    // 'flags' is from the STF_* family, can be 0 or any combination of values;
    // of Windows control; if the attachment is successful, the StaticText
    // interface is returned, otherwise NULL is returned; the interface is
    // valid until the Windows control is destroyed (WM_DESTROY); after
    // attachment, the text and alignment is extracted from the Windows control
    // tested on Windows control "STATIC"
    virtual CGUIStaticTextAbstract* WINAPI AttachStaticText(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // HyperLink
    //
    // Used to display a hyperlink. After clicking, it is possible to open the URL
    // or run a file (SetActionOpen), or post the command back to the dialog
    // (SetActionPostCommand).
    // The control is accessible via the TAB key (can have focus), but it is
    // necessary to set WS_TABSTOP. The action is then invoked by the Space key.
    // The right mouse button or Shift+F10 can be used to display a menu with
    // options to copy the text of the control to the clipboard.
    //
    // Attaches Salamander's HyperLink to a Windows control (this control
    // determines the position of the HyperLink); 'hParent' is the handle
    // of the parent window (dialog or window); 'ctrlID' is the ID of the
    // Windows control; 'flags' is from the STF_* family, can be 0 or any
    // combination of values; recommended combination for HyperLink is
    // STF_UNDERLINE | STF_HYPERLINK_COLOR; if the attachment is successful,
    // the HyperLink interface is returned, otherwise NULL is returned; the
    // interface is valid until the Windows control is destroyed (WM_DESTROY);
    // after attachment, the text and alignment is extracted from the Windows control
    // tested on Windows control "STATIC"
    virtual CGUIHyperLinkAbstract* WINAPI AttachHyperLink(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // Button
    //
    // Used to create a button with text or an icon. The button can contain an arrow
    // on the right or a drop-down arrow. See BTF_xxx flags.
    //
    // Attaches Salamander's TextArrowButton to a Windows control (this control
    // determines the position, text or icon and the command of the button);
    // 'hParent' is the handle of the parent window (dialog or window);
    // 'ctrlID' is the ID of the Windows control;
    // if the attachment is successful, the CGUIButtonAbstract interface is
    // returned, otherwise NULL is returned; the interface is valid until the
    // Windows control is destroyed (WM_DESTROY);
    // tested on Windows control "BUTTON"
    virtual CGUIButtonAbstract* WINAPI AttachButton(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ColorArrowButton
    //
    // Slouzi pro vytvoreni tlacitka s barevnym obdelnickem, ktery naselduje sipka smerujici vpravo.
    // (pokud je showArrow==TRUE)
    // V obdelnicku je zobrazen text, ktery muze mit prirazenou jinou barvu nez barva pozadi obdelnicku.
    // Pouziva se v konfiguracich barev, kde dokaze zobrazit jednu nebo dve barvy.
    // Po stisku je vybaleno popup menu s moznosti zvolit, kterou barvu konfigurujeme..
    //
    // pripoji Salamanderovsky ColorArrowButton na Windows control (tento control urcuje pozici,
    // text a command ColorArrowButtonu); 'hParent' je handle parent okna (dialog nebo okno);
    // ctrlID je ID Windows controlu;
    // pri uspesnem pripojeni vraci rozhrani ColorArrowButtonu, jinak vraci NULL; rozhrani je
    // platne az do okamziku destrukce (doruceni WM_DESTROY) Windows controlu;
    // Testovano na Windows controlu "BUTTON".
    virtual CGUIColorArrowButtonAbstract* WINAPI AttachColorArrowButton(HWND hParent, int ctrlID, BOOL showArrow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ChangeToArrowButton
    //
    // Used to create a button with an arrow pointing to the right in the middle
    // of the button. It is inserted after the input field and after pressing
    // the button, a popup menu is displayed with items that can be inserted
    // into the input field (form of help).
    //
    // Changes the style of the button so that it can hold an icon with an arrow
    // and then assigns this icon. It does not attach any Salamander object to
    // the control, because the operating system can handle everything.
    // Returns TRUE on success, otherwise FALSE.
    // The button text is ignored.
    // Tested on Windows control "BUTTON".
    virtual BOOL WINAPI ChangeToArrowButton(HWND hParent, int ctrlID) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // MenuPopup
    //
    // Used to create an empty popup menu. Returns a pointer to the interface
    // or NULL on error.
    virtual CGUIMenuPopupAbstract* WINAPI CreateMenuPopup() = 0;
    // Used to free the allocated menu.
    virtual BOOL WINAPI DestroyMenuPopup(CGUIMenuPopupAbstract* popup) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // MenuBar
    //
    // Used to create a menu bar. The 'menu' items will be displayed in the menu bar,
    // their children will be submenus. 'hNotifyWindow' identifies the window to
    // which commands and notifications will be sent. Returns a pointer to the
    // interface or NULL on error.
    virtual CGUIMenuBarAbstract* WINAPI CreateMenuBar(CGUIMenuPopupAbstract* menu, HWND hNotifyWindow) = 0;
    // Used to free the allocated menu bar. Also destroys the window.
    virtual BOOL WINAPI DestroyMenuBar(CGUIMenuBarAbstract* menuBar) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolBar support
    //
    // Used to create an inactive (gray) version of the bitmap for the menu or toolbar.
    // From the source bitmap 'hSource', it creates a bitmap in gray scales 'hGrayscale'
    // and a black and white mask 'hMask'. The color 'transparent' is considered transparent.
    // Returns TRUE on success and 'hGrayscale' and 'hMask'; on error, returns FALSE.
    virtual BOOL WINAPI CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                                      HBITMAP& hGrayscale, HBITMAP& hMask) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolBar
    //
    // Used to create a toolbar; 'hNotifyWindow' identifies the window to which
    // commands and notifications will be sent. Returns a pointer to the interface
    // or NULL on error.
    virtual CGUIToolBarAbstract* WINAPI CreateToolBar(HWND hNotifyWindow) = 0;
    // Used to free the allocated toolbar. Also destroys the window.
    virtual BOOL WINAPI DestroyToolBar(CGUIToolBarAbstract* toolBar) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolTip
    //
    // This method executes the timer and if it is not called again before the timer
    // expires, it asks the window 'hNotifyWindow' for text using the message
    // WM_USER_TTGETTEXT, which then displays it under the cursor at its current
    // coordinates. The 'id' variable is used to distinguish the area when communicating
    // with the window 'hNotifyWindow'. If this method is called multiple times with
    // the same 'id' parameter, these additional calls will be ignored.
    // The value 0 of the 'hNotifyWindow' parameter is reserved for turning off the window
    // and interrupting the running timer.
    virtual void WINAPI SetCurrentToolTip(HWND hNotifyWindow, DWORD id) = 0;
    // supress displaying tooltip at current mouse position
    // useful to call when activating a window that uses tooltips
    // to prevent unwanted displaying of tooltips
    virtual void WINAPI SuppressToolTipOnCurrentMousePos() = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // XP Visual Styles
    //
    // If this is called under an operating system that supports visual styles,
    // it calls SetWindowTheme(hWindow, L" ", L" ") to disable visual styles
    // for the window 'hWindow'.
    // Returns TRUE if the operating system supports visual styles, otherwise FALSE.
    virtual BOOL WINAPI DisableWindowVisualStyles(HWND hWindow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // IconList
    //
    // Two methods for allocating and destroying an IconList object used to hold
    // 32bpp icons (3 x 8 bits for color and 8 bits for alpha transparency).
    // Other operations on IconList see the description of CGUIIconListAbstract.
    virtual CGUIIconListAbstract* WINAPI CreateIconList() = 0;
    virtual BOOL WINAPI DestroyIconList(CGUIIconListAbstract* iconList) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolTip support
    //
    // Searches 'buf' for the first occurrence of the '\t' character. If 'stripHotKey'
    // is TRUE, the string is terminated at this character. Otherwise, a space is
    // inserted at this character and the rest of the text is enclosed in parentheses.
    // The buffer 'buf' must be large enough to extend the text in the buffer by two
    // characters (parentheses) when 'stripHotKey' is FALSE.
    virtual void WINAPI PrepareToolTipText(char* buf, BOOL stripHotKey) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // Subject with file/dir name truncated if needed
    //
    // Sets the text created as sprintf(, subjectFormatString, fileName) to the static 'subjectWnd'.
    // The format string 'subjectFormatString' must contain exactly one '%s' (to insert 'fileName').
    // If the text exceeds the length of the static, it will be shortened by shortening 'fileName'.
    // In addition, it converts 'fileName' according to SALCFG_FILENAMEFORMAT (to match how 'fileName'
    // is displayed in the panel) using CSalamanderGeneralAbstract::AlterFileName.
    // If it is a file, 'isDir' is FALSE, otherwise TRUE. If the static 'subjectWnd' has SS_NOPREFIX,
    // 'duplicateAmpersands' is FALSE, otherwise TRUE (duplicates the second and subsequent ampersands ('&'),
    // the first ampersand indicates the hotkey in the subject and must be contained in 'subjectFormatString'
    // before '%s').
    // Example of use: SetSubjectTruncatedText(GetDlgItem(HWindow, IDS_SUBJECT), "&Rename %s to",
    //                                          file->Name, fileIsDir, TRUE)
    // can be called from any thread
    virtual void WINAPI SetSubjectTruncatedText(HWND subjectWnd, const char* subjectFormatString, const char* fileName,
                                                BOOL isDir, BOOL duplicateAmpersands) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolbarHeader
    //
    // Used to create a header above the list (for listview or listbox), which contains
    // a text description and a group of buttons on the right side. An example can be
    // seen in the configuration of Salamander, see Hot Paths or User Menu.
    // 'hParent' is the handle of the dialog, 'ctrlID' is the ID of the static text
    // around which the ToolbarHeader will be created, 'hAlignWindow' is the handle
    // of the list to which the header will be attached, 'buttonMask' is one or (sum)
    // more values TLBHDRMASK_xxx and specifies which buttons will be displayed in
    // the header.
    virtual CGUIToolbarHeaderAbstract* WINAPI AttachToolbarHeader(HWND hParent, int ctrlID, HWND hAlignWindow, DWORD buttonMask) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ArrangeHorizontalLines
    //
    // Finds horizontal lines in the dialog 'hWindow' and connects them from the right
    // to the static text or checkbox or radiobox to which they are attached. In addition,
    // it finds checkboxes and radioboxes that form the labels of groupboxes and shortens
    // them according to their text and the current font in the dialog. Eliminates
    // unnecessary spaces due to different DPI of the screen.
    virtual void WINAPI ArrangeHorizontalLines(HWND hWindow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // GetWindowFontHeight
    //
    // gets the current font of the window 'hWindow' using WM_GETFONT and returns its height
    // using GetObject()
    virtual int WINAPI GetWindowFontHeight(HWND hWindow) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_gui)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
