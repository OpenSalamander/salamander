// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern LPWSTR PatternHistory[MAX_HISTORY_ENTRIES];
extern LPWSTR LookInHistory[MAX_HISTORY_ENTRIES];

//*********************************************************************************
//
// CStatusBar
//

class CStatusBar : public CWindow
{
protected:
    WCHAR Text[MAX_FULL_KEYNAME + 50];
    int BaseLen;
    BOOL Dirty;
    BOOL UpdateInIdle;
    CCS Section; // kriticka sekce pro pristup k textovemu bufferu
    int Width;
    int TextWidth;
    int Height;
    HBITMAP HBitmap;

public:
    CStatusBar();
    virtual ~CStatusBar();
    void AllocateBitmap();

    // nastavi zaklad, ke kteremu pomoci Set pripojuje dalsi text
    void SetBase(LPCWSTR text, BOOL updateInIdle = FALSE);
    // pripojovani k zakladu nastavenemu pres SetBase
    void Set(LPCWSTR text, BOOL updateInIdle = FALSE);
    // vraci komplet string
    //void Get(char *buf, int bufSize);

    void OnEnterIdle();

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*********************************************************************************
//
// CFoundFilesData
// CFoundFilesListView
//

// indexy sloupcu

#define CI_NAME 0
#define CI_TYPE 1
#define CI_DATA 2
#define CI_PATH 3
#define CI_SIZE 4
#define CI_DATE 5
#define CI_TIME 6

struct CFoundFilesData
{
    LPWSTR Name;
    LPWSTR Path;
    DWORD Type;
    DWORD Size;
    DWORD_PTR Data;
    unsigned Allocated : 1;
    FILETIME Time;
    BOOL IsDir;
    DWORD State;          // pro ulozeni state stavu
    unsigned Default : 1; // jde o default polozku

    CFoundFilesData()
    {
        Path = Name = NULL;
        Data = NULL;
        Allocated = 0;
        IsDir = FALSE;
    }
    CFoundFilesData(LPWSTR name, int root, LPWSTR key, DWORD type,
                    DWORD size, unsigned char* data,
                    FILETIME time, BOOL isDir);
    //CFoundFilesData(CFoundFilesData & orig);
    ~CFoundFilesData()
    {
        if (Name)
            free(Name);
        if (Path)
            free(Path);
        if (Allocated && Data)
            free((void*)Data);
    }
    /*
  BOOL Set(const char * name, const char * volume, const char * path,
           QWORD size, FILETIME time, DWORD attributes, BOOL isDir);
  */
    LPWSTR GetText(int i, LPWSTR buffer);
    //char *GetFullPath(char *buffer, int size, BOOL skipName = FALSE);
};

#define SYMBOL_CX 16 // rozmery bitmap
#define SYMBOL_CY 16

#define ICON_CX 16 // rozmery ikon v panelech a v cache-bitmape
#define ICON_CY 16

class CFindDialog;

class CFoundFilesListView : public CWindow
{
protected:
    TIndirectArray<CFoundFilesData> Data;
    CCS DataCriticalSection; // kriticka sekce pro pristup k datum
    CFindDialog* SearchDialog;

public:
    CFoundFilesListView(CFindDialog* searchDialog);
    virtual ~CFoundFilesListView();

    void StoreItemsState();
    void RestoreItemsState();

    int CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, int sortBy);
    void QuickSort(int left, int right, int sortBy);
    void SortItems(int sortBy);

    // interface pro Data
    CFoundFilesData* At(int index);
    void DestroyMembers();
    int GetCount();
    int Add(CFoundFilesData* item);
    BOOL IsGood();

    BOOL InitColumns();
    void GetContextMenuPos(POINT* p);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

/*
//******************************************************************************
//
// CComboboxEdit
//
// Protoze je combobox vyprasenej, nelze klasickou cestou (CB_GETEDITSEL) zjistit
// po opusteni focusu, jaka byla selection. To resi tento control.
//

class CComboboxEdit: public CWindow
{
  protected:
    DWORD SelStart;
    DWORD SelEnd;

  public:
    CComboboxEdit();

    void GetSel(DWORD *start, DWORD *end);

    void ReplaceText(const char *text);

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/

//*********************************************************************************
//
// CFindDialog
//

#define IDT_REFRESH_LISTVIEW 1

class CFindDialog : public CDialogEx
{
protected:
    LPWSTR LookInInit;

    // layoutovaci parametry
    int MinDlgW;
    int MinDlgH;
    int HMargin;
    int VMargin;
    int CombosH;
    int CombosX;
    int FindNowW;
    INT FindNowY;
    int OptionsY;
    int OptionsH;
    int OptionsW;
    int ResultsY;
    int StatusHeight;
    int AddY;
    int FoundItemsH;

    CStatusBar* StatusBar;
    CFoundFilesListView* List;
    CFindDialog** ZeroOnDestroy; // hodnota ukazatele bude pri destrukci nulovana
    //CComboboxEdit * LookIn;

    // parametry dotazu
    WCHAR Pattern[MAX_KEYNAME];
    TIndirectArray<WCHAR> LookInList;
    BOOL IncludeSubkeys;
    BOOL LookAtKeys;
    BOOL LookAtValues;
    BOOL LookAtData;
    BOOL Hex;
    BOOL CaseSensitive;
    BOOL WholeWords;
    BOOL RegExp;
    BOOL UseMinTime, UseMaxTime;
    SYSTEMTIME MinTime, MaxTime;

    BOOL SearchInProgress;
    HANDLE CancelEvent;
    int FoundVisibleCount;
    DWORD NextUpdate;
    BOOL CloseWhenSearchFinishes;
    WCHAR LVItemTextBuffer[100];
    BOOL Stopped;
    BOOL ShowOptions;

public:
    CFindDialog(LPWSTR lookInInit);
    virtual ~CFindDialog() { ; }
    void SetZeroOnDestroy(CFindDialog** zeroOnDestroy) { ZeroOnDestroy = zeroOnDestroy; }

    void GetLayoutParams();
    int GetMinDlgH() { return ShowOptions ? MinDlgH : MinDlgH - ResultsY + (int)(OptionsY + OptionsH * 1.1); }
    void LayoutControls(BOOL showOrHideControls);

    void EnableControls(BOOL enable);
    void UpdateListViewItems();
    void UpdateStatusText(BOOL searchFinished = FALSE);
    void OnEnterIdle();

    void StartSearch();
    void StopSearch();
    virtual void Validate(CTransferInfoEx& ti);
    virtual void Transfer(CTransferInfoEx& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CFindThread;
};

class CFindDialogThread : public CThread
{
    WCHAR LookIn[MAX_FULL_KEYNAME];

public:
    CFindDialogThread(LPWSTR lookIn)
        : CThread("CFindDialogThread") { wcscpy(LookIn, lookIn); }
    virtual unsigned Body();
};

//*********************************************************************************
//
// CFindThread
//

class CFindThread : public CThread
{
    TIndirectArray<WCHAR>& LookIn;
    char PatternA[MAX_KEYNAME];
    char PatternW[MAX_KEYNAME * 2];
    int PatternALen, PatternWLen;
    BOOL IncludeSubkeys;
    BOOL LookAtKeys;
    BOOL LookAtValues;
    BOOL LookAtData;
    BOOL CaseSensitive;
    BOOL WholeWords;
    BOOL RegExp;
    QWORD Number;
    BOOL UseNumber;
    BOOL UseMinTime, UseMaxTime;
    SYSTEMTIME MinTime, MaxTime;
    CFindDialog* FindDialog;
    HANDLE CancelEvent;

    // pro pro preklad unicode retezce pri hledani pomoci regexp
    TBuffer<char> AsciiBuffer;

    CSalamanderBMSearchData* BMForPatternA;
    CSalamanderBMSearchData* BMForPatternW;
    CSalamanderREGEXPSearchData* SalRegExp;

    DWORD NextStatusUpdate;

public:
    CFindThread(TIndirectArray<WCHAR>& lookIn,
                char* patternA, int patternALen,
                char* patternW, int patternWLen,
                BOOL includeSubkeys, BOOL lookAtKeys, BOOL lookAtValues,
                BOOL lookAtData, BOOL caseSensitive, BOOL wholeWords,
                BOOL regExp, QWORD number, BOOL useNumber,
                BOOL useMinTime, BOOL useMaxTime,
                SYSTEMTIME& minTime, SYSTEMTIME& maxTime,
                CFindDialog* findDlg, HANDLE cancelEvent)
        : CThread("CFindThread"), LookIn(lookIn)
    {
        memcpy(PatternA, patternA, patternALen);
        memcpy(PatternW, patternW, patternWLen);
        PatternALen = patternALen;
        PatternWLen = patternWLen;
        IncludeSubkeys = includeSubkeys;
        LookAtKeys = lookAtKeys;
        LookAtValues = lookAtValues;
        LookAtData = lookAtData;
        CaseSensitive = caseSensitive;
        WholeWords = wholeWords;
        RegExp = regExp;
        Number = number;
        UseNumber = useNumber;
        UseMinTime = useMinTime;
        UseMaxTime = useMaxTime;
        MinTime = minTime;
        MaxTime = maxTime;
        FindDialog = findDlg;
        CancelEvent = cancelEvent;
        BMForPatternA = NULL;
        BMForPatternW = NULL;
        SalRegExp = NULL;
    }

    ~CFindThread()
    {
        if (BMForPatternA)
            SG->FreeSalamanderBMSearchData(BMForPatternA);
        if (BMForPatternW && BMForPatternA != BMForPatternW)
            SG->FreeSalamanderBMSearchData(BMForPatternW);
        if (SalRegExp)
            SG->FreeSalamanderREGEXPSearchData(SalRegExp);
    }

    BOOL TestTime(FILETIME& ft);
    BOOL Test(char* text, int len, BOOL name, DWORD type);
    BOOL ScanKeyAux(int root, LPWSTR key, BOOL& skip, BOOL& skipAllErrors,
                    LPWSTR nameBuffer, TIndirectArray<WCHAR>& stack);
    BOOL ScanKey(int root, LPWSTR key, BOOL& skip, BOOL& skipAllErrors,
                 LPWSTR nameBuffer, TIndirectArray<WCHAR>& stack);
    BOOL ScanRegistry(BOOL& skipAllErrors);
    BOOL Init();
    virtual unsigned Body();
};
