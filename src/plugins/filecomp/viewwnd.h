// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CMainWindow;

// ****************************************************************************
//
// CFileViewWindow
//

#define SB_FASTLEFT 0x8000
#define SB_FASTRIGHT 0xC000

#define BORDER_WIDTH 2

// line color index
#define LC_NORMAL 0 // shodne text
#define LC_CHANGE 1 // zmeneny text
#define LC_FOCUS 2  // zmeneny a fokusovany text

struct CLineColors
{
    COLORREF LineNumFgColor;
    COLORREF LineNumBkColor;
    HPEN LineNumBorderPen; // platne jen pro LC_FOCUS (ramecek kolem fokusu) a LC_NORMAL (separator differenci)
    COLORREF FgColor;
    COLORREF BkColor;
    HPEN BorderPen;    // platne jen pro LC_FOCUS (ramecek kolem fokusu) a LC_NORMAL (separator differenci)
    COLORREF FgCommon; // barva spolecneho textu pro zobrazeni rozdilu v radce
    COLORREF BkCommon;
};

// identifikace okna
enum CFileViewID
{
    fviLeft,
    fviRight
};

// identifikace okna
enum CFileViewType
{
    fvtText,
    fvtHex
};

extern HCURSOR HIbeamCursor;
extern HCURSOR HArrowCursor;

extern const char* FILEVIEWWINDOW_CLASSNAME;

// proporcionalne k sirce okna, na okraje kasleme, jde o "odhad"
#define FAST_LEFTRIGHT __max(1, width / FontWidth / 6)

BOOL TestHScrollWParam(WPARAM wParam);

class CFileViewWindow : public CWindow
{
protected:
    CFileViewID ID;
    CFileViewType Type;
    BOOL DataValid;
    int LineNumDigits;
    int LineNumWidth;
    HFONT HFont;
    LONG FontHeight;
    LONG FontWidth;
    CLineColors LineColors[3];
    LONG Height;
    LONG Width;
    CFileViewWindow* Siblink;
    LONG SiblinkWidth;
    CFileViewMode ViewMode;
    TMappedTextOut<char> MappedASCII8TextOut;
    BOOL Tracking;

    // slouzi pro akumulaci mikrokroku pri otaceni koleckem mysi, viz
    // http://msdn.microsoft.com/en-us/library/ms997498.aspx (Best Practices for Supporting Microsoft Mouse and Keyboard Devices)
    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal

public:
    CFileViewWindow(CFileViewID id, CFileViewType type);
    virtual ~CFileViewWindow();

    virtual BOOL Is(CFileViewType type) { return Type == type; };

    virtual void InvalidateData();
    virtual void DestroyData() { DataValid = FALSE; }
    virtual void SetSiblink(CFileViewWindow* wnd) = 0;
    virtual CFileViewWindow* GetSiblink() { return Siblink; }
    virtual void ReloadConfiguration(DWORD flags, BOOL updateWindow);
    virtual void UpdateScrollBars(BOOL repaint = TRUE) = 0;
    virtual void Paint() = 0;
    virtual int GetLineNumDigits() { return LineNumDigits; }

    virtual BOOL IsSelected() = 0;
    virtual BOOL CopySelection() = 0;
    virtual void RemoveSelection(BOOL repaint) = 0;

    virtual void UpdateSelection(int x, int y) = 0;

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }
    void ResetMouseWheelAccumulatorHandler(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// TTextFileViewWindow
//

// // line flags
// #define LF_COMMON     0
// #define LF_CHANGE     0x40000000
// #define LF_SEPARATOR  0x80000000
// #define LF_BLANK      0xC0000000
//
// #define MAX_LINES (~LF_BLANK)
// #define MAX_LINES_DIGITS (10)
//
// inline BOOL IsBlank(DWORD line)
// {
//   CALL_STACK_MESSAGE_NONE
//   return (line & LF_BLANK) == LF_BLANK || (line & LF_BLANK) == LF_SEPARATOR;
// }

// id timeru pro automaticky scroll pri vyberu textu pomoci mysi
#define IDT_SCROLLTEXT 1

// znak, ktery se obevi misto prazdnych znaku
//#define WHITE_SPACE_CHAR ((char)0xB7)

// update selection flag
#define USF_REMOVE 0
#define USF_EXPAND 1
#define USF_NOCHANGE 2

class CTextFileViewWindowBase : public CFileViewWindow
{
protected:
    CLineScript Script[2]; // fvmStandard, fvmOnlyDifferences
    int Context;           // context lines in Script[fvmOnlyDifferences]
    int FirstVisibleLine;
    int FirstVisibleChar;
    int MaxWidth;
    int SelectDiffBegin;
    int SelectDiffEnd;
    BOOL SelectDifferenceOnButtonUp;
    int TrackingLineBegin;
    int TrackingCharBegin;
    int SelectionLineBegin;
    int SelectionCharBegin;
    int SelectionLineEnd;
    int SelectionCharEnd;
    BOOL DisplayCaret;
    int CaretXPos;
    int CaretYPos;
    BOOL ShowWhiteSpace;
    // prvni prvek LineChanges[cislo_zmeny][cislo_radky_ve_zmene] udava pocet
    // nasledujicich paru, kazdy par urcuje offset pocatku a konce zmeny v
    // radce
    // unsigned int ***LineChanges;
    // int * ChangesToLinesMap;
    // int * LinesToChangesMap;
    // CEditScript * EdScript; // potrebne poze pro in-line changes
    BOOL DetailedDifferences;
    int TabSize;
    CMainWindow* MainWindow;

public:
    CTextFileViewWindowBase(CFileViewID id, BOOL showWhiteSpace, CMainWindow* mainWindow);
    virtual ~CTextFileViewWindowBase();
    //virtual void DestroyLineChanges();
    virtual void DestroyData();
    virtual int GetLines() = 0;
    virtual int GetScriptSize() { return int(Script[ViewMode].size()); }
    BOOL RebuildScript(const CIntIndexes& changesLengths, CIntIndexes* changesToLines, CFileViewMode viewMode, BOOL updateChangesToLines, int& maxWidth, BOOL& cancel);
    BOOL SetMaxWidth(int maxWidth);
    virtual void SetSiblink(CFileViewWindow* wnd);
    virtual void ReloadConfiguration(DWORD flags, BOOL updateWindow);
    virtual void UpdateScrollBars(BOOL repaint = TRUE);
    void SelectDifference(int line, int count, BOOL center);
    BOOL IsTracking() { return Tracking; }
    void UpdateSelection();
    virtual void UpdateSelection(int x, int y);
    void RepaintSelection(int oldLineBegin, int oldLineEnd,
                          int oldCharBegin, int oldCharEnd);
    virtual void RemoveSelection(BOOL repaint);
    virtual BOOL IsSelected()
    {
        return (SelectionLineBegin != SelectionLineEnd ||
                SelectionCharBegin != SelectionCharEnd) &&
               DataValid;
    }
    int GetCaretPos(int& yPos)
    {
        yPos = CaretYPos;
        return CaretXPos;
    }
    void SetCaretPos(int xPos, int yPos);
    void UpdateCaretPos(BOOL enablePosChange);
    void SetCaret(BOOL displayCaret);
    void ShowCaret();
    void SetShowWhiteSpace(BOOL show) { ShowWhiteSpace = show; }
    void SetDetailedDifferences(BOOL newValue)
    {
        DetailedDifferences = newValue;
    }

    // char type dependent methods
    virtual int MeasureLine(int line, int length) = 0;
    //    virtual BOOL RebuildLineDiffScripts(CEditScript * edScript, int count,
    //	CCompareOptions * options, BOOL &cancel) = 0;
    virtual BOOL ReallocLineBuffer() = 0;
    virtual int TransformXOrg(int x, int line) = 0;
    virtual void MoveCaret(int yPos) = 0;
    virtual int GetLeftmostVisibleChar(int line) = 0;
    virtual int GetRightmostVisibleChar(int line) = 0;
    virtual int LineLength(int line) = 0;
    virtual void SelectWord(int line, int& start, int& end) = 0;
    virtual void NextCharBlock(int line, int& xpos) = 0;
    virtual void PrevCharBlock(int line, int& xpos) = 0;
    virtual void Paint() = 0;

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CHexFileViewWindow
//

__inline __int64 Abs64(__int64 n) { return n >= 0 ? n : -n; }

char* QWord2Ascii(QWORD qw, char* buffer, int digits);
int ComputeAddressCharWidth(QWORD size1, QWORD size2);

class CHexFileViewWindow : public CFileViewWindow
{
protected:
    CFileMapping Mapping;
    QWORD FirstDiff;
    double ScrollScale;
    int BytesPerLine;
    QWORD ViewOffset;
    QWORD SiblinkSize;
    char Path[MAX_PATH];
    BOOL PaintEnabled;
    int HScrollOffs;
    QWORD FocusedDiffOffset;
    QWORD FocusedDiffLength;
    BOOL SelectDifferenceOnButtonUp;
    QWORD TrackingOffsetBegin;
    QWORD SelectedOffset;
    QWORD SelectedLength;

public:
    CHexFileViewWindow(CFileViewID id);
    virtual ~CHexFileViewWindow();

    virtual void DestroyData();
    virtual void SetSiblink(CFileViewWindow* wnd);
    virtual void UpdateScrollBars(BOOL repaint = TRUE);
    virtual BOOL SetData(QWORD firstDiff, const char* path, QWORD siblinkSize);
    virtual void Paint();
    void EnablePaint() { PaintEnabled = TRUE; }
    void DisablePaint() { PaintEnabled = FALSE; }
    const char* GetPath() { return Path; }
    int HandleFileException(EXCEPTION_POINTERS* e);
    void HandleFileError(const char* path, int error);
    QWORD FindDifference(int cmd, QWORD* pOffset);
    void SelectDifference(QWORD offset, QWORD length, BOOL center);
    QWORD GetFocusedDifference(QWORD* length)
    {
        if (length)
            *length = FocusedDiffLength;
        return FocusedDiffOffset;
    };

    virtual BOOL IsSelected() { return DataValid && (SelectedOffset != -1); };
    virtual BOOL CopySelection();
    virtual void RemoveSelection(BOOL repaint);

    virtual void UpdateSelection(int x, int y);

protected:
    QWORD CalculateOffset(int x, int y);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
