// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//
// ****************************************************************************

class CMainToolBar;

enum CBorderLines
{
    blNone = 0x00,
    blTop = 0x01,
    blBottom = 0x02
};

enum CSecurityIconState
{
    sisNone = 0x00,      // Icon is not displayed
    sisUnsecured = 0x01, // Icon of an unlocked lock displayed
    sisSecured = 0x02    // Icon of a locked lock displayed
};

/*
enum
{
  otStatusWindow = otLastWinLibObject
};
*/

//
// CHotTrackItem
//
// Item holds the index of the first character, the number of characters, and the offset of the first character in points
// and their length in points; for the displayed path a list of these items is created and kept
// in an array
//
// for the path "\\john\c\winnt
//
// the following items are created:
//
// (0, 9,  0, length of the first nine characters)   = \\john\c\
// (0, 14, 0, length of 14 characters)              = \\john\c\winnt
//
// for "DIR: 12"
//
// (0, 3, 0, length of the three characters "DIR")
// (5, 2, point offset of "12", length of two characters "12")

struct CHotTrackItem
{
    WORD Offset;       // Offset of the first character in characters
    WORD Chars;        // Number of characters
    WORD PixelsOffset; // Offset of the first character in points
    WORD Pixels;       // Their length in points
};

class CStatusWindow : public CWindow
{
public:
    CMainToolBar* ToolBar;
    CFilesWindow* FilesWindow;

protected:
    TDirectArray<CHotTrackItem> HotTrackItems;
    BOOL HotTrackItemsMeasured;

    int Border; // Separator line on the top/bottom
    char* Text;
    int TextLen; // Number of characters in 'Text' pointer without the terminator
    char* Size;
    int PathLen;          // -1 (the path occupies the entire Text); otherwise the path length in Text (the remainder is the filter)
    BOOL History;         // Show the arrow between the text and the size?
    BOOL Hidden;          // Show the filter symbol?
    int HiddenFilesCount; // how many files are filtered out?
    int HiddenDirsCount;  // And directories
    BOOL WholeTextVisible;

    BOOL ShowThrobber;             // TRUE if the 'progress' throbber should be shown after the text/hidden filter (regardless of the window's existence)
    BOOL DelayedThrobber;          // TRUE if the timer to show the throbber is already running
    DWORD DelayedThrobberShowTime; // The GetTickCount() value when the delayed throbber should appear (0 = do not show with a delay)
    BOOL Throbber;                 // Show the 'progress' throbber after the text/hidden filter? (TRUE only if the window exists)
    int ThrobberFrame;             // Index of the current animation frame
    char* ThrobberTooltip;         // Tooltip; if NULL, nothing is displayed
    int ThrobberID;                // identifier of the throbber (-1 = invalid)

    CSecurityIconState Security;
    char* SecurityTooltip; // Tooltip; if NULL, nothing is displayed

    int Allocated;
    int* AlpDX; // Array of lengths (from the first up to the X-th character in the string)
    BOOL Left;

    int ToolBarWidth; // Current toolbar width

    int EllipsedChars; // Number of characters omitted after the root; otherwise -1
    int EllipsedWidth; // Length of the omitted string after the root; otherwise -1

    CHotTrackItem* HotItem;     // Highlighted item
    CHotTrackItem* LastHotItem; // Last highlighted item
    BOOL HotSize;               // The size item is highlighted
    BOOL HotHistory;            // The history item is highlighted
    BOOL HotZoom;               // The zoom item is highlighted
    BOOL HotHidden;             // The filter symbol is highlighted
    BOOL HotSecurity;           // The lock symbol is highlighted

    RECT TextRect;     // Rectangle where the text was erased
    RECT HiddenRect;   // Rectangle where the filter symbol was erased
    RECT SizeRect;     // Rectangle where the size text was erased
    RECT HistoryRect;  // Rectangle where the history drop-down was erased
    RECT ZoomRect;     // Rectangle where the zoom drop-down was erased
    RECT ThrobberRect; // Rectangle where the throbber was erased
    RECT SecurityRect; // Rectangle where the lock was erased
    int MaxTextRight;
    BOOL MouseCaptured;
    BOOL RButtonDown;
    BOOL LButtonDown;
    POINT LButtonDownPoint; // Where the user pressed the left button

    int Height;
    int Width; // Width

    BOOL NeedToInvalidate; // For SetAutomatic() - has a change occurred that requires repainting?

    DWORD* SubTexts;     // Array of DWORDs: LOWORD = position, HIWORD = length
    DWORD SubTextsCount; // Number of items in SubTexts array

    IDropTarget* IDropTargetPtr;

public:
    CStatusWindow(CFilesWindow* filesWindow, int border, CObjectOrigin origin = ooAllocated);
    ~CStatusWindow();

    BOOL SetSubTexts(DWORD* subTexts, DWORD subTextsCount);
    // Sets 'text' text into the status line; 'pathLen' specifies the path length (the rest is the filter),
    // if 'pathLen' is not used (the path equals the entire 'text') it is set to -1
    BOOL SetText(const char* text, int pathLen = -1);

    // Builds the HotTrackItems array: for disks and archives based on backslashes
    // and asks the plugin for FS entries
    void BuildHotTrackItems();

    void GetHotText(char* buffer, int bufSize);

    void DestroyWindow();

    int GetToolBarWidth() { return ToolBarWidth; }

    int GetNeededHeight();
    void SetSize(const CQuadWord& size);
    void SetHidden(int hiddenFiles, int hiddenDirs);
    void SetHistory(BOOL history);
    void SetThrobber(BOOL show, int delay = 0, BOOL calledFromDestroyWindow = FALSE); // Call only from the main (GUI) thread, just like the other object methods
    // Sets the text displayed as the tooltip when the mouse hovers over the throbber; the object makes its own copy
    // If NULL, no tooltip is shown
    void SetThrobberTooltip(const char* throbberTooltip);
    int ChangeThrobberID(); // Changes ThrobberID and returns its new value
    BOOL IsThrobberVisible(int throbberID) { return ShowThrobber && ThrobberID == throbberID; }
    void HideThrobberAndSecurityIcon();

    void SetSecurity(CSecurityIconState iconState);
    void SetSecurityTooltip(const char* tooltip);

    void InvalidateIfNeeded();

    void LayoutWindow();
    void Paint(HDC hdc, BOOL highlightText = FALSE, BOOL highlightHotTrackOnly = FALSE);
    void Repaint(BOOL flashText = FALSE, BOOL hotTrackOnly = FALSE);
    void InvalidateAndUpdate(BOOL update); // May also be called when HWindow == NULL
    void FlashText(BOOL hotTrackOnly = FALSE);

    BOOL FindHotTrackItem(int xPos, int& index);

    void SetLeftPanel(BOOL left);
    BOOL ToggleToolBar();

    BOOL IsLeft() { return Left; }

    BOOL SetDriveIcon(HICON hIcon);     // The icon is copied into the image list - the caller is responsible for destruction
    void SetDrivePressed(BOOL pressed); // Presses the drive icon

    BOOL GetTextFrameRect(RECT* r);   // Returns the rectangle around the text in screen coordinates
    BOOL GetFilterFrameRect(RECT* r); // Returns the rectangle around the filter symbol in screen coordinates

    // The screen color depth may have changed; CacheBitmap needs to be rebuilt
    void OnColorsChanged();

    void SetFont();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void RegisterDragDrop();
    void RevokeDragDrop();

    // Creates an image list with a single item that will be used to display drag progress
    // After the drag finishes, the image list must be released
    // Input is the point relative to which dxHotspot and dyHotspot offsets are calculated
    HIMAGELIST CreateDragImage(const char* text, int& dxHotspot, int& dyHotspot, int& imgWidth, int& imgHeight);

    void PaintThrobber(HDC hDC);
    //    void RepaintThrobber();

    void PaintSecurity(HDC hDC);
};
