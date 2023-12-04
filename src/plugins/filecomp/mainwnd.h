// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CMainWindow
//

#define BI_TOOLBAR 0  // band s toollbarou
#define BI_DIFFLIST 1 // band s combacem

#define IDC_REBAR 100
#define IDC_TOOLBAR 101
#define IDC_DIFFLIST 102
#define IDC_LEFTFILEVIEW 103
#define IDC_RIGHTFILEVIEW 104

// indexy bitmap pro toolbaru
#define BI_COPY 0
#define BI_SELECTALL 1
#define BI_FIRSTDIFF 2
#define BI_PREVDIFF 3
#define BI_NEXTDIFF 4
#define BI_LASTDIFF 5

#define REBAR_BORDER 2 // vyska mezery pod rebarou

// flagy pro CMainWindow::UpdateToolbarButtons()
#define UTB_DIFFSSELECTION 0x01
#define UTB_TEXTSELECTION 0x02
#define UTB_MENU 0x04
#define UTB_FORCEDISABLE 0x08
#define UTB_ALL (UTB_DIFFSSELECTION | UTB_TEXTSELECTION | UTB_MENU)

#define CW_CONTINUE 0 // jedeme dal
#define CW_CANCEL 1   // storno operace
#define CW_EXIT 2     // storno a exit file comparatoru
#define CW_SILENT 3   // ukonceni threadu bez message boxu pro usera

extern HCURSOR HWaitCursor;

extern HPALETTE Palette;
extern BOOL UsePalette;

extern CBandParams BandsParams[2];

extern const char* MAINWINDOW_CLASSNAME;

class CMainWindow : public CWindow
{
protected:
    char *Path1,
        *Path2;
    DWORD HeaderHeight;
    CFileHeaderWindow* LeftHeader;
    CFileHeaderWindow* RightHeader;
    HWND LeftFileViewHWnd;
    HWND RightFileViewHWnd;
    CFileViewWindow* FileView[2];
    int Active;
    CSplitBarWindow* SplitBar;
    CRebar* Rebar;
    LONG RebarHeight; // vyska rebary + vyska mezery pod rebarou
    CComboBox* ComboBox;
    HWND HToolbar;
    CIntIndexes LinesToChanges;
    CIntIndexes ChangesToLines[2]; // fvmStandard, fvmOnlyDifferences
    CIntIndexes ChangesLengths;
    CEditScript TextChanges; // data to fill combo box
    int DifferencesCount;
    LONG Height;
    LONG Width;
    double SplitProp, PrevSplitProp;
    BOOL Initialized;
    int SelectedDifference;
    BOOL DataValid;
    BOOL InputEnabled;
    int CancelWorker; // viz. konstanty CW_xxx
    BOOL FirstCompare;
    CCompareOptions Options;
    //int DifferencesCount;
    CFileViewMode ViewMode;
    BOOL ShowWhiteSpace;
    BOOL ShowCaret;
    HANDLE WorkerEvent;
    BOOL Wait;
    BOOL OptionsChanged;
    BOOL DetailedDifferences;
    BOOL CalculatingDetailedDifferences;
    BOOL Recompare;
    BOOL bOptionsChangedBeingHandled; // Simple semaphore

    // pro binarni compare
    CBinaryChanges Changes;
    BOOL OutOfRange;
    UINT ShowCmd;

public:
    CMainWindow(char* path1, char* path2, CCompareOptions* options, UINT showCmd);
    virtual ~CMainWindow();
    BOOL Init();
    void EnableInput(BOOL enable);
    void LayoutChilds();
    void SelectDifference(int i, int cmd = 0, BOOL setSel = TRUE, BOOL center = TRUE);
    void SelectDifferenceByLine(int line, BOOL center);
    BOOL GetDifferenceRange(int line, int* start, int* end);
    void SelectDifferenceByOffset(QWORD offset, BOOL center);
    int FindDifference(QWORD offset);
    BOOL EnableToolbarButton(UINT cmd, BOOL enable);
    void UpdateToolbarButtons(DWORD flags);
    BOOL RebuildFileViewScripts(BOOL& cancel);
    void ResetComboBox(BOOL* cancel = NULL);
    void SaveRebarLayout();
    void RestoreRebarLayout();
    //void UpdateSelection();
    void SpawnWorker(const char* path1, const char* path2, BOOL recompare,
                     const CCompareOptions& options);
    void SetActiveFileView(CFileViewID id) { Active = id; }
    void SetWait(BOOL wait);
    int GetChangeFirstLine(int line)
    {
        return int(ChangesToLines[ViewMode][LinesToChanges[line]]);
    }
    template <class CChar>
    bool TextFilesDiffer(CTextCompareResults<CChar>* res, char* message, UINT& type, const char* (&encoding)[2]);
    friend class CFileViewWindow;

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
