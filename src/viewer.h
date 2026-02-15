// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define VIEW_BUFFER_SIZE 60000 // 0.5 * VIEW_BUFFER_SIZE must be > max. length \
                               // of a displayable line
#define BORDER_WIDTH 3         // separates the text from the window edge
#define APROX_LINE_LEN 1000

#define FIND_TEXT_LEN 201                    // +1; WARNING: should match GREP_TEXT_LEN
#define FIND_LINE_LEN 10000                  // must be > FIND_TEXT_LEN and the max line length for REGEXP (different macro for GREP)
#define TEXT_MAX_LINE_LEN 10000              // when a line is longer we ask about switching to hex mode; must be <= FIND_LINE_LEN
#define RECOGNIZE_FILE_TYPE_BUFFER_LEN 10000 // how many characters from the start of the file to use to recognize the file type (RecognizeFileType())

#define VIEWER_HISTORY_SIZE 30 // number of remembered strings

// menu positions - redo when the menu changes!
#define VIEWER_FILE_MENU_INDEX 0         // in the viewer main menu
#define VIEWER_FILE_MENU_OTHFILESINDEX 3 // in the File submenu of the viewer main menu
#define VIEWER_EDIT_MENU_INDEX 1         // in the viewer main menu
#define VIEW_MENU_INDEX 3                // in the viewer main menu
#define CODING_MENU_INDEX 4              // in the viewer main menu
#define OPTIONS_MENU_INDEX 5             // in the viewer main menu

#define WM_USER_VIEWERREFRESH WM_APP + 201 // [0, 0] - perform a refresh

#ifndef INSIDE_SALAMANDER
char* LoadStr(int resID);
char* GetErrorText(DWORD error);
#endif // INSIDE_SALAMANDER

extern char* ViewerHistory[VIEWER_HISTORY_SIZE];

void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* Text,
                     int textLen, BOOL hexMode, int historySize, char* history[],
                     BOOL changeOnlyHistory = FALSE);
void DoHexValidation(HWND edit, const int textLen);
void ConvertHexToString(char* text, char* hex, int& len);

int GetHexOffsetMode(unsigned __int64 fileSize, int& hexOffsetLength);
void PrintHexOffset(char* s, unsigned __int64 offset, int mode);

void GetDefaultViewerLogFont(LOGFONT* lf);

// ****************************************************************************

class CFindSetDialog : public CCommonDialog
{
public:
    int Forward, // forward/backward (1/0)
        WholeWords,
        CaseSensitive,
        HexMode,
        Regular;

    char Text[FIND_TEXT_LEN];

    CFindSetDialog(HINSTANCE modul, int resID, UINT helpID)
        : CCommonDialog(modul, resID, helpID, NULL, ooStatic)
    {
        Forward = TRUE;
        WholeWords = FALSE;
        CaseSensitive = FALSE;
        HexMode = FALSE;
        Regular = FALSE;
        Text[0] = 0;
    }

    CFindSetDialog& operator=(CFindSetDialog& d)
    {
        Forward = d.Forward;
        WholeWords = d.WholeWords;
        CaseSensitive = d.CaseSensitive;
        HexMode = d.HexMode;
        Regular = d.Regular;
        memmove(Text, d.Text, FIND_TEXT_LEN);
        return *this;
    }

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    int CancelHexMode, // only for the Cancel button to work correctly
        CancelRegular;
};

// ****************************************************************************

class CViewerGoToOffsetDialog : public CCommonDialog
{
public:
    CViewerGoToOffsetDialog(HWND parent, __int64* offset)
        : CCommonDialog(HLanguage, IDD_VIEWERGOTOOFFSET, IDD_VIEWERGOTOOFFSET, parent) { Offset = offset; }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    __int64* Offset;
};

// ****************************************************************************

enum CViewType
{
    vtText,
    vtHex
};

class CViewerWindow : public CWindow
{
public:
    CViewerWindow(const char* fileName, CViewType type, const char* caption,
                  BOOL wholeCaption, CObjectOrigin origin, int enumFileNamesSourceUID,
                  int enumFileNamesLastFileIndex);
    ~CViewerWindow();

    void OpenFile(const char* file, const char* caption, BOOL wholeCaption); // does not manage Lock

    virtual BOOL Is(int type) { return type == otViewerWindow || CWindow::Is(type); }
    BOOL IsGood() { return Buffer != NULL && ViewerFont != NULL; }
    void InitFindDialog(CFindSetDialog& dlg)
    {
        FindDialog = dlg;
        if (FindDialog.Text[0] == 0)
            return;
        else
        {
            if (FindDialog.Regular)
            {
                RegExp.Set(FindDialog.Text, 0);
            }
            else
            {
                if (FindDialog.HexMode)
                {
                    char hex[FIND_TEXT_LEN];
                    int len;
                    ConvertHexToString(FindDialog.Text, hex, len);
                    SearchData.Set(hex, len, 0);
                }
                else
                    SearchData.Set(FindDialog.Text, 0);
            }
        }
    }

    HANDLE GetLockObject(); // object for the disk cache - viewing from a ZIP
    void CloseLockObject();

    void ConfigHasChanged(); // called after OK in the configuration dialog

    // returns text for Find - the (null-terminated) selected block; 'buf' is at least
    // FIND_TEXT_LEN bytes; returns TRUE if the buffer is filled (a block exists, etc.); returns the number
    // of written characters without the null terminator into 'len'
    BOOL GetFindText(char* buf, int& len);

protected:
    void FatalFileErrorOccured(DWORD repeatCmd = -1); // called when a file error occurs (viewer refresh/clear is required)

    void OnVScroll();

    void CodeCharacters(unsigned char* start, unsigned char* end);
    // if 'hFile' is NULL, Prepare/LoadBefore/LoadBehind open and close the file themselves
    // if 'hFile' points to a variable (initialize its value to NULL at the start),
    // the methods open the file and store the file handle into that variable (when opening succeeds).
    // On the next call they do not open the file again and reuse the handle from that variable.
    // They also do not close the handle when exiting; the caller must handle that.
    // This is an optimization for network drives where repeatedly opening/closing the file 
    // slowed down searching terribly.
    BOOL LoadBefore(HANDLE* hFile);
    BOOL LoadBehind(HANDLE* hFile);

    // if a read error occurs, fatalErr == TRUE; ExitTextMode does not arise here (it does not become TRUE)
    __int64 Prepare(HANDLE* hFile, __int64 offset, __int64 bytes, BOOL& fatalErr);

    void GoToEnd() { SeekY = MaxSeekY; }
    // if a read error occurs, fatalErr == TRUE; ExitTextMode is TRUE when switching to hex mode
    void FileChanged(HANDLE file, BOOL testOnlyFileSize, BOOL& fatalErr, BOOL detectFileType,
                     BOOL* calledHeightChanged = NULL);
    // if a read error occurs, fatalErr == TRUE; ExitTextMode is TRUE when switching to hex mode
    void HeightChanged(BOOL& fatalErr);
    // if a read error occurs, fatalErr == TRUE; ExitTextMode is TRUE when switching to hex mode
    __int64 ZeroLineSize(BOOL& fatalErr, __int64* firstLineEndOff = NULL, __int64* firstLineCharLen = NULL);

    // text view only; if a read error occurs, fatalErr == TRUE; ExitTextMode is TRUE
    // when switching to hex mode
    __int64 FindSeekBefore(__int64 seek, int lines, BOOL& fatalErr, __int64* firstLineEndOff = NULL,
                           __int64* firstLineCharLen = NULL, BOOL addLineIfSeekIsWrap = FALSE);
    // 'hFile' parameter, see the comment for Prepare(); if a read error occurs, fatalErr == TRUE;
    // ExitTextMode does not arise here (it does not become TRUE)
    BOOL FindNextEOL(HANDLE* hFile, __int64 seek, __int64 maxSeek, __int64& lineEnd, __int64& nextLineBegin, BOOL& fatalErr);
    // if a read error occurs, fatalErr == TRUE; ExitTextMode is TRUE when switching to hex mode
    BOOL FindPreviousEOL(HANDLE* hFile, __int64 seek, __int64 minSeek, __int64& lineBegin,
                         __int64& previousLineEnd, BOOL allowWrap,
                         BOOL takeLineBegin, BOOL& fatalErr, int* lines, __int64* firstLineEndOff = NULL,
                         __int64* firstLineCharLen = NULL, BOOL addLineIfSeekIsWrap = FALSE);

    // if a read error occurs, fatalErr == TRUE; ExitTextMode is TRUE when switching to hex mode
    __int64 FindBegin(__int64 seek, BOOL& fatalErr);
    void ChangeType(CViewType type);

    void Paint(HDC dc);
    void SetScrollBar();
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // posts WM_MOUSEMOVE (used to move the block end to new mouse coordinates or
    // to recalculate the block end when the view shifts)
    void PostMouseMove();

    // scroll the view one line up
    BOOL ScrollViewLineUp(DWORD repeatCmd = -1, BOOL* scrolled = NULL, BOOL repaint = TRUE,
                          __int64* firstLineEndOff = NULL, __int64* firstLineCharLen = NULL);

    // scroll the view one line down
    BOOL ScrollViewLineDown(BOOL fullRedraw = FALSE);

    // invalidate + optionally update selected rows of the view
    void InvalidateRows(int minRow, int maxRow, BOOL update = TRUE);

    // adjust OriginX if needed so that the x coordinate 'x' is visible in the view
    void EnsureXVisibleInView(__int64 x, BOOL showPrevChar, BOOL& fullRedraw,
                              __int64 newFirstLineLen = -1, BOOL ignoreFirstLine = FALSE,
                              __int64 maxLineLen = -1);

    // determine the maximum length of a visible line in the view (text mode: must be repainted,
    // otherwise LineOffset is outdated)
    __int64 GetMaxVisibleLineLen(__int64 newFirstLineLen = -1, BOOL ignoreFirstLine = FALSE);

    // determine the maximum OriginX for the current view (text mode: must be repainted,
    // otherwise LineOffset is outdated)
    __int64 GetMaxOriginX(__int64 newFirstLineLen = -1, BOOL ignoreFirstLine = FALSE, __int64 maxLineLen = -1);

    // determine the x coordinate 'x' of position 'offset' in the file on the line; if 'lineInView' is not -1,
    // use the line 'lineInView' from LineOffset and ignore 'lineBegOff', 'lineCharLen',
    // and 'lineEndOff'
    BOOL GetXFromOffsetInText(__int64* x, __int64 offset, int lineInView, __int64 lineBegOff = -1,
                              __int64 lineCharLen = -1, __int64 lineEndOff = -1);

    // find the closest file position 'offset' for the suggested x coordinate 'suggestedX' on the line;
    // returns in 'x' the x coordinate of position 'offset' in the file on the line; if 'lineInView' is not -1,
    // use the line 'lineInView' from LineOffset and ignore 'lineBegOff', 'lineCharLen',
    // and 'lineEndOff'
    BOOL GetOffsetFromXInText(__int64* x, __int64* offset, __int64 suggestedX, int lineInView,
                              __int64 lineBegOff = -1, __int64 lineCharLen = -1,
                              __int64 lineEndOff = -1);

    // return the file offset based on the [x, y] coordinates on the screen; if 'leftMost'
    // is FALSE the left and right halves of a character correspond to different offsets, if TRUE any point on the character
    // shares the same offset (the offset of that character) - used when detecting a click inside a block;
    // if 'onHexNum' != NULL and we are in hex mode and [x, y] is on a hex digit or a character,
    // '*onHexNum' becomes TRUE
    // if a read error occurs, fatalErr == TRUE; ExitTextMode does not arise here (it does not become TRUE)
    BOOL GetOffset(__int64 x, __int64 y, __int64& offset, BOOL& fatalErr, BOOL leftMost = FALSE,
                   BOOL* onHexNum = NULL);

    // return the file offset 'offset' at x coordinate 'x' on a line beginning at the offset
    // 'lineBegOff' with a length of 'lineCharLen' displayed characters (expanded tabs; this is
    // not used in hex mode, set to 0) and ending at 'lineEndOff' (not used in hex mode, set to 0);
    // returns in 'offsetX' the x coordinate of 'offset' (implemented only for text mode and
    // not necessarily equal to 'x'); if 'onHexNum' != NULL and we are in hex mode and the x coordinate 'x' is on
    // a hex digit or character, '*onHexNum' becomes TRUE; if a read error occurs, fatalErr == TRUE;
    // ExitTextMode does not arise here (it does not become TRUE); if 'getXFromOffset' is TRUE (we support it only for
    // the text view; set 'offset', 'offsetX', and 'onHexNum' to NULL), return the X coordinate (in 'foundX')
    // of the character at offset 'findOffset' instead of offset
    BOOL GetOffsetOrXAbs(__int64 x, __int64* offset, __int64* offsetX, __int64 lineBegOff, __int64 lineCharLen,
                         __int64 lineEndOff, BOOL& fatalErr, BOOL* onHexNum, BOOL getXFromOffset = FALSE,
                         __int64 findOffset = -1, __int64* foundX = NULL);

    // for a large selection (over 100 MB) ask the user whether they really want to allocate the selection
    // for drag & drop or the clipboard
    BOOL CheckSelectionIsNotTooBig(HWND parent, BOOL* msgBoxDisplayed = NULL);

    // if a read error occurs, fatalErr == TRUE; ExitTextMode does not arise here (it does not become TRUE)
    HGLOBAL GetSelectedText(BOOL& fatalErr); // text for clipboard and drag & drop operations

    void SetToolTipOffset(__int64 offset);

    void SetViewerCaption();

    // set CodeType + UseCodeTable + CodeTable + window caption
    // WARNING: CodeTables.Valid(c) must return TRUE
    void SetCodeType(int c);

    BOOL CreateViewerBrushs();
    void ReleaseViewerBrushs();
    void SetViewerFont();

    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }

    void ReleaseMouseDrag();

    void FindNewSeekY(__int64 newSeekY, BOOL& fatalErr);

    // calls SalMessageBox internally and blocks Paint just for it (only clears the viewer background, does not touch the file)
    int SalMessageBoxViewerPaintBlocked(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);

    unsigned char* Buffer; // buffer with size VIEW_BUFFER_SIZE
    char* FileName;        // currently viewed file
    __int64 Seek,          // offset of byte 0 in Buffer within the file
        Loaded,            // number of valid bytes in Buffer
        OriginX,           // first displayed column (in characters)
        SeekY,             // seek of the first displayed line
        MaxSeekY,          // seek of the end of the viewed file
        FileSize,          // file size
        ViewSize,          // size of the displayed portion of the file (in bytes)
        FirstLineSize,     // size of the first displayed line (in bytes)
        LastLineSize;      // size of the last fully displayed line (in bytes)

    __int64 StartSelection,           // seek of the selection start (including that character) (-1 = no selection yet)
        EndSelection;                 // seek of the selection end (up to but not including that character) (-1 = no selection yet)
    int TooBigSelAction;              // D&D or auto-/copy-to-clip of blocks over 100 MB: 0 = ask, 1 = YES, 2 = NO
    int EndSelectionRow;              // y offset of the line containing EndSelection (relative to the client area)
                                      // valid only during block dragging; used to optimize painting
                                      // if it is -1, the optimization is skipped
    __int64 EndSelectionPrefX;        // preferred x coordinate when dragging the block end via Shift+up/down arrows (-1 = none)
    TDirectArray<__int64> LineOffset; // array with offsets of line beginnings and ends (without EOL) + lengths in displayed characters (a triple per line)
    BOOL WrapIsBeforeFirstLine;       // text view only in wrap mode: the line before the first view line is a wrap (not an EOL)
    BOOL MouseDrag;                   // is the block being dragged with the mouse?
    BOOL ChangingSelWithShiftKey;     // is the selection being changed via Shift+key (arrows, End, Home)

    CFindSetDialog FindDialog;
    CSearchData SearchData;
    CRegularExpression RegExp;
    __int64 FindOffset,              // seek from which to search
        LastFindSeekY,               // seek of the first screen line after searching, for detecting back-and-forth movement
        LastFindOffset;              // seek from which to search (set after searching), for detecting back-and-forth movement
    BOOL ResetFindOffsetOnNextPaint; // TRUE = set FindOffset during the next WM_PAINT (the visible page size is known only after drawing, enabling FindOffset to be set even for backward searches)
    BOOL SelectionIsFindResult;      // TRUE = the selection was created as a search result

    int DefViewMode; // 0 = Auto-Select, 1 = Text, 2 = Hex
    CViewType Type;  // display type
    BOOL EraseBkgnd; // ensure the background is cleared once at the beginning

    int Width,  // window width (in points)
        Height; // window height (in points)

    BOOL EnablePaint; // for recursive Paint calls on errors: FALSE = only clear the viewer background (leave the file alone)

    BOOL ScrollToSelection; // during the next drawing shift to the selection position (OriginX)

    double ScrollScaleX,  // horizontal scrollbar coefficient
        ScrollScaleY;     // vertical scrollbar coefficient
    BOOL EnableSetScroll; // do not refresh scrollbar data while dragging

    __int64 ToolTipOffset; // hex mode: file offset (shown in the tooltip)
    HWND HToolTip;         // tooltip window

    HANDLE Lock; // handle for the disk cache

    BOOL WrapText; // local copy of Configuration.WrapText

    BOOL CodePageAutoSelect;  // local copy of Configuration.CodePageAutoSelect
    char DefaultConvert[200]; // local copy of Configuration.DefaultConvert

    BOOL ExitTextMode;  // TRUE = the current message processing must end quickly; switching to hex mode
                        //         (the file is unsuitable for text mode, it lacks EOLs)
    BOOL ForceTextMode; // TRUE = the user insists on text mode at any cost (they will wait)

    int CodeType;        // numeric encoding identifier; CodeTables memory for this viewer window
    BOOL UseCodeTable;   // should CodeTable be used for recoding?
    char CodeTable[256]; // code table

    char CurrentDir[MAX_PATH]; // path for the open dialog

    BOOL WaitForViewerRefresh;   // TRUE - waiting for WM_USER_VIEWERREFRESH; other commands are skipped
    __int64 LastSeekY;           // SeekY before the error
    __int64 LastOriginX;         // OriginX before the error
    DWORD RepeatCmdAfterRefresh; // command to repeat after refresh (-1 = no command)

    char* Caption;     // if not NULL, contains the proposed viewer window caption
    BOOL WholeCaption; // meaningful if Caption != NULL. TRUE -> only 
                       // Caption is displayed in the viewer title; FALSE -> append 
                       // the standard " - Viewer" to Caption.

    BOOL CanSwitchToHex,           // TRUE if switching to hex is possible when there are more than 10000 characters per line
        CanSwitchQuietlyToHex,     // TRUE if switching does not need confirmation (while loading a file)
        FindingSoDonotSwitchToHex; // TRUE if switching to hex should be blocked when there are more than 10000 characters per line (undesirable during text searching)

    int HexOffsetLength; // hex mode: number of characters in the offset (in the leftmost column)

    // GDI objects (each thread has its own)
    HBRUSH BkgndBrush;    // solid brush with the window background color
    HBRUSH BkgndBrushSel; // solid brush with the window background color - selected text

    CBitmap Bitmap;
    HFONT ViewerFont;

    int EnumFileNamesSourceUID;     // UID of our source for enumerating names in the viewer
    int EnumFileNamesLastFileIndex; // index of the currently viewed file

    WPARAM VScrollWParam; // wParam from WM_VSCROLL/SB_THUMB*; -1 if dragging is not in progress
    WPARAM VScrollWParamOld;

    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal
};

// ****************************************************************************

BOOL InitializeViewer();
void ReleaseViewer();
void ClearViewerHistory(BOOL dataOnly); // clears histories; for dataOnly==FALSE also clears the Find dialog combobox (if any)
void UpdateViewerColors(SALCOLOR* colors);

extern const char* CVIEWERWINDOW_CLASSNAME; // viewer window class

extern CWindowQueue ViewerWindowQueue; // list of all viewer windows

extern CFindSetDialog GlobalFindDialog; // for initializing a new viewer window

extern BOOL UseCustomViewerFont; // if TRUE, use the ViewerLogFont structure stored in the configuration; otherwise default values
extern LOGFONT ViewerLogFont;
extern HMENU ViewerMenu;
extern HACCEL ViewerTable;
extern int CharWidth, // character width (in points)
    CharHeight;       // character height (in points)

// Vista: the fixedsys font contains characters that do not have the expected width (even though it is a fixed-width font), therefore
// measure individual characters and map those with an incorrect width to a replacement character with the correct width
extern CRITICAL_SECTION ViewerFontMeasureCS; // critical section for measuring the font
extern BOOL ViewerFontMeasured;              // TRUE = the font created from ViewerLogFont was already measured; FALSE = the font must be measured
extern BOOL ViewerFontNeedsMapping;          // TRUE = ViewerFontMapping must be used; FALSE = the font is OK and mapping is unnecessary
extern char ViewerFontMapping[256];          // remapping to characters that have the expected fixed width

extern HANDLE ViewerContinue; // helper event - waiting for the message-loop thread to start

BOOL OpenViewer(const char* name, CViewType mode, int left, int top, int width, int height,
                UINT showCmd, BOOL returnLock, HANDLE* lock, BOOL* lockOwner,
                CSalamanderPluginViewerData* viewerData, int enumFileNamesSourceUID,
                int enumFileNamesLastFileIndex);
