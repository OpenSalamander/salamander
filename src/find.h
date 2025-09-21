// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// structure for adding messages to the Find Log; sent as the message parameter
// of WM_USER_ADDLOG; parameters will be copied into the log data (can be deallocated after returning)
#define FLI_INFO 0x00000000   // INFORMATION item
#define FLI_ERROR 0x00000001  // ERROR item
#define FLI_IGNORE 0x00000002 // allow the Ignore button on this item
struct FIND_LOG_ITEM
{
    DWORD Flags;      // FLI_xxx
    const char* Text; // message text, must not be NULL
    const char* Path; // path to a file or directory, may be NULL
};

#define WM_USER_ADDLOG WM_APP + 210     // add an item to the log [FIND_LOG_ITEM* item, 0]
#define WM_USER_ADDFILE WM_APP + 211    // [0, 0]
#define WM_USER_SETREADING WM_APP + 212 // [0, 0] - request to repaint the status bar ("Reading:")
#define WM_USER_BUTTONS WM_APP + 213    // call EnableButtons() yourself [HWND hButton]
#define WM_USER_FLASHICON WM_APP + 214  // after activating the find, flash the status icon

extern BOOL IsNotAlpha[256];

#define ITEMNAME_TEXT_LEN MAX_PATH + MAX_PATH + 10
#define NAMED_TEXT_LEN MAX_PATH  // maximum text length in the combobox
#define LOOKIN_TEXT_LEN MAX_PATH // maximum text length in the combobox
#define GREP_TEXT_LEN 201        // maximum text length in the combobox; NOTE: should match FIND_TEXT_LEN
#define GREP_LINE_LEN 10000      // maximum line length for regular expressions (viewer uses a different macro)

// Length of the mapped view; must be greater than the length of a line for regexp + EOL +
// AllocationGranularity
#define VOF_VIEW_SIZE 0x2800400 // 40 MB (more is risky, virtual memory may be limited) + 1 KB (space for a reasonable text line)

// history for the Named combobox
#define FIND_NAMED_HISTORY_SIZE 30 // number of remembered strings
extern char* FindNamedHistory[FIND_NAMED_HISTORY_SIZE];

// history for the LookIn combobox
#define FIND_LOOKIN_HISTORY_SIZE 30 // number of remembered strings
extern char* FindLookInHistory[FIND_LOOKIN_HISTORY_SIZE];

// history for the Containing combobox
#define FIND_GREP_HISTORY_SIZE 30 // number of remembered strings
extern char* FindGrepHistory[FIND_GREP_HISTORY_SIZE];

extern BOOL FindManageInUse; // is the Manage dialog open?
extern BOOL FindIgnoreInUse; // is the Ignore dialog open?

BOOL InitializeFind();
void ReleaseFind();

// clears all Find histories; if 'dataOnly' == TRUE, comboboxes of open windows are not cleared
void ClearFindHistory(BOOL dataOnly);

DWORD WINAPI GrepThreadF(void* ptr); // body of the grep thread

extern HACCEL FindDialogAccelTable;

class CFoundFilesListView;
class CFindDialog;
class CMenuPopup;
class CMenuBar;

//*********************************************************************************
//
// CSearchForData
//

struct CSearchForData
{
    char Dir[MAX_PATH];
    CMaskGroup MasksGroup;
    BOOL IncludeSubDirs;

    CSearchForData(const char* dir, const char* masksGroup, BOOL includeSubDirs)
    {
        Set(dir, masksGroup, includeSubDirs);
    }

    void Set(const char* dir, const char* masksGroup, BOOL includeSubDirs);
    const char* GetText(int i)
    {
        switch (i)
        {
        case 0:
            return MasksGroup.GetMasksString();
        case 1:
            return Dir;
        default:
            return IncludeSubDirs ? LoadStr(IDS_INCLUDESUBDIRSYES) : LoadStr(IDS_INCLUDESUBDIRSNO);
        }
    }
};

//*********************************************************************************
//
// CSearchingString
//
// synchronized buffer for the "Searching" text in the Find dialog's status bar

class CSearchingString
{
protected:
    char Buffer[MAX_PATH + 50];
    int BaseLen;
    BOOL Dirty;
    CRITICAL_SECTION Section;

public:
    CSearchingString();
    ~CSearchingString();

    // sets the base to which additional text is appended via Set, and sets dirty to FALSE
    void SetBase(const char* buf);
    // appending to the base value set via SetBase
    void Set(const char* buf);
    // returns the complete string
    void Get(char* buf, int bufSize);

    // sets the dirty flag (is a redraw already pending?)
    void SetDirty(BOOL dirty);
    // returns TRUE if a redraw is already pending
    BOOL GetDirty();
};

//*********************************************************************************
//
// CGrepData
//

// flags for searching identical files
// at least _NAME or _SIZE must be specified
// _CONTENT can be set only when _SIZE is set as well
#define FIND_DUPLICATES_NAME 0x00000001    // same name
#define FIND_DUPLICATES_SIZE 0x00000002    // same size
#define FIND_DUPLICATES_CONTENT 0x00000004 // same content

struct CGrepData
{
    BOOL FindDuplicates; // do we search for duplicates?
    DWORD FindDupFlags;  // FIND_DUPLICATES_xxx; meaningful only if 'FindDuplicates' is TRUE
    int Refine;          // 0: search new data, 1 & 2: search within found data; 1: intersect with old data; 2: subtract from old data
    BOOL Grep;           // use grep?
    BOOL WholeWords;     // match whole words only?
    BOOL Regular;        // regular expression?
    BOOL EOL_CRLF,       // EOL handling when searching regular expressions
        EOL_CR,
        EOL_LF;
    //       EOL_NULL;              // unsupported by the regular expression parser :(

    CSearchData SearchData;
    CRegularExpression RegExp;
    // advanced search
    DWORD AttributesMask;  // mask first
    DWORD AttributesValue; // then compare
    CFilterCriteria Criteria;
    // control and data
    BOOL StopSearch;    // the main thread sets this to terminate the grep thread
    BOOL SearchStopped; // has it been terminated or not?
    HWND HWindow;       // window the grep thread communicates with
    TIndirectArray<CSearchForData>* Data;
    CFoundFilesListView* FoundFilesListView; // found files are loaded here
    // two criteria for updating the list view
    int FoundVisibleCount;  // number of items displayed in the list view
    DWORD FoundVisibleTick; // when it was last displayed
    BOOL NeedRefresh;       // need to refresh the display (an item was added without being shown)

    CSearchingString* SearchingText;  // synchronized "Searching" text in the Find status bar
    CSearchingString* SearchingText2; // [optional] second text on the right; used for "Total: 35%"
};

//*********************************************************************************
//
// CFindOptionsItem
//

class CFindOptionsItem
{
public:
    // Internal
    char ItemName[ITEMNAME_TEXT_LEN];

    CFilterCriteria Criteria;

    // Find dialog
    int SubDirectories;
    int WholeWords;
    int CaseSensitive;
    int HexMode;
    int RegularExpresions;

    BOOL AutoLoad;

    char NamedText[NAMED_TEXT_LEN];
    char LookInText[LOOKIN_TEXT_LEN];
    char GrepText[GREP_TEXT_LEN];

public:
    CFindOptionsItem();
    // WARNING! Once the object contains allocated data the code for moving items
    // in the Options Manager stops working because temporary items get destroyed

    CFindOptionsItem& operator=(const CFindOptionsItem& s);

    // builds name of the item (ItemName) based on NamedText and LookInText
    void BuildItemName();

    // WARNING: saving is optimized; only changed values are stored. The key has to be cleared first, before saving.
    BOOL Save(HKEY hKey);                   // saves the item
    BOOL Load(HKEY hKey, DWORD cfgVersion); // loads the item
};

//*********************************************************************************
//
// CFindOptions
//

class CFindOptions
{
protected:
    TIndirectArray<CFindOptionsItem> Items;

public:
    CFindOptions();

    BOOL Save(HKEY hKey);                   // saves the entire array
    BOOL Load(HKEY hKey, DWORD cfgVersion); // loads the entire array

    BOOL Load(CFindOptions& source);

    int GetCount() { return Items.Count; }
    BOOL Add(CFindOptionsItem* item);
    CFindOptionsItem* At(int i) { return Items[i]; }
    void Delete(int i) { Items.Delete(i); }

    // clears previous inserted items and fills in the new ones
    void InitMenu(CMenuPopup* popup, BOOL enabled, int originalCount);
};

//*********************************************************************************
//
// CFindIgnore
//

enum CFindIgnoreItemType
{
    fiitUnknow,
    fiitFull,     // Full path including root: 'C:\' 'D:\TMP\' \\server\share\'
    fiitRooted,   // Path starting in any root
    fiitRelative, // Path without a root: 'aaa' 'aaa\bbbb\ccc'
};

class CFindIgnoreItem
{
public:
    BOOL Enabled;
    char* Path;

    // the following data are not saved; they are initialized in Prepare()
    CFindIgnoreItemType Type;
    int Len;

public:
    CFindIgnoreItem();
    ~CFindIgnoreItem();
};

// The CFindIgnore object serves two purposes:
// 1. A global object holding the list of paths editable in the Find/Options/Ignore Directory List
// 2. A temporary copy used for searching -- contains only Enabled items which are
//    adjusted (backslashes added) and classified (CFindIgnoreItem::Type set)
class CFindIgnore
{
protected:
    TIndirectArray<CFindIgnoreItem> Items;

public:
    CFindIgnore();

    BOOL Save(HKEY hKey);                   // saves the entire array
    BOOL Load(HKEY hKey, DWORD cfgVersion); // loads the entire array

    // called for the local copy of the object which is then used for searching
    // must be called before calling Contains()
    // copies items from 'source' and prepares them for search
    // returns TRUE on success, otherwise FALSE
    BOOL Prepare(CFindIgnore* source);

    // returns TRUE if the list contains an item matching the 'path'
    // only items with 'Enabled' == TRUE are evaluated
    // returns FALSE if no such item is found
    // Note: the method must receive the full path with a trailing slash
    BOOL Contains(const char* path, int startPathLen);

    // adds the path only if it does not already exist in the list
    BOOL AddUnique(BOOL enabled, const char* path);

protected:
    void DeleteAll();
    void Reset(); // clears existing items and adds default values

    BOOL Load(CFindIgnore* source);

    int GetCount() { return Items.Count; }
    BOOL Add(BOOL enabled, const char* path);
    BOOL Set(int index, BOOL enabled, const char* path);
    CFindIgnoreItem* At(int i) { return Items[i]; }
    void Delete(int i) { Items.Delete(i); }
    BOOL Move(int srcIndex, int dstIndex);

    friend class CFindIgnoreDialog;
};

//*********************************************************************************
//
// CFindAdvancedDialog
//
/*
class CFindAdvancedDialog: public CCommonDialog
{
  public:
    BOOL             SetDateAndTime;
    CFindOptionsItem *Data;

  public:
    CFindAdvancedDialog(CFindOptionsItem *data);

    int Execute();
    virtual void Validate(CTransferInfo &ti);
    virtual void Transfer(CTransferInfo &ti);
    void EnableControls();   // handles disabling/enabling operations
    void LoadTime();

  protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/
//*********************************************************************************
//
// CFindManageDialog
//

class CFindManageDialog : public CCommonDialog
{
protected:
    CEditListBox* EditLB;
    CFindOptions* FO;
    const CFindOptionsItem* CurrenOptionsItem;

public:
    CFindManageDialog(HWND hParent, const CFindOptionsItem* currenOptionsItem);
    ~CFindManageDialog();

    virtual void Transfer(CTransferInfo& ti);
    void LoadControls();

    BOOL IsGood() { return FO != NULL; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*********************************************************************************
//
// CFindIgnoreDialog
//

class CFindIgnoreDialog : public CCommonDialog
{
protected:
    CEditListBox* EditLB;
    CFindIgnore* IgnoreList; // our working copy of the data
    CFindIgnore* GlobalIgnoreList;
    BOOL DisableNotification;
    HICON HChecked;
    HICON HUnchecked;

public:
    CFindIgnoreDialog(HWND hParent, CFindIgnore* globalIgnoreList);
    ~CFindIgnoreDialog();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    BOOL IsGood() { return IgnoreList != NULL; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void FillList();
};

//****************************************************************************
//
// CFindDuplicatesDialog
//

class CFindDuplicatesDialog : public CCommonDialog
{
public:
    // the settings will be remembered for the duration of Salamander's run
    static BOOL SameName;
    static BOOL SameSize;
    static BOOL SameContent;

public:
    CFindDuplicatesDialog(HWND hParent);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls(); // handles disabling/enabling operations
};

//*********************************************************************************
//
// CFindLog
//
// Used for storing errors that occurred during the search.
//

struct CFindLogItem
{
    DWORD Flags;
    char* Text;
    char* Path;
};

class CFindLog
{
protected:
    TDirectArray<CFindLogItem> Items;
    int SkippedErrors; // number of errors that were not stored
    int ErrorCount;
    int InfoCount;

public:
    CFindLog();
    ~CFindLog();

    void Clean(); // releases all held items

    BOOL Add(DWORD flags, const char* text, const char* path);
    int GetCount() { return Items.Count; }
    int GetSkippedCount() { return SkippedErrors; }
    const CFindLogItem* Get(int index);
    int GetErrorCount() { return ErrorCount; }
    int GetInfoCount() { return InfoCount; }
};

//*********************************************************************************
//
// CFindLogDialog
//
// Used to display errors that occurred during the search.
//

class CFindLogDialog : public CCommonDialog
{
protected:
    CFindLog* Log;
    HWND HListView;

public:
    CFindLogDialog(HWND hParent, CFindLog* log);

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnFocusFile();
    void OnIgnore();
    void EnableControls();
    const CFindLogItem* GetSelectedItem();
};

//*********************************************************************************
//
// CFoundFilesData
// CFoundFilesListView
//

#define MD5_DIGEST_SIZE 16

struct CMD5Digest
{
    BYTE Digest[MD5_DIGEST_SIZE];
};

struct CFoundFilesData
{
    char* Name;
    char* Path;
    CQuadWord Size;
    DWORD Attr;
    FILETIME LastWrite;

    // 'Group' is used in two ways:
    // 1) while searching for duplicate files, when contents are compared,
    //    it holds a pointer to CMD5Digest with the computed MD5 of the file
    // 2) before passing duplicate search results to the ListView
    //    it contains a number connecting multiple files into an equivalent group
    DWORD_PTR Group;

    unsigned IsDir : 1; // 0 - item is a file, 1 - item is a directory
    // Selected and Focused are used only locally for StoreItemsState/RestoreItemsState
    unsigned Selected : 1; // 0 - item not selected, 1 - item selected
    unsigned Focused : 1;  // 0 - item is focused, 1 - item is not focused
    // 'Different' is used to distinguish file groups during duplicate search
    unsigned Different : 1; // 0 - item has standard white background, 1 - item uses a different one (for difference highlighting)

    CFoundFilesData()
    {
        Path = NULL;
        Name = NULL;
        Attr = 0;
        ZeroMemory(&LastWrite, sizeof(LastWrite));
        Group = 0;
        IsDir = 0;
        Selected = 0;
        Different = 0;
    }
    ~CFoundFilesData()
    {
        if (Path != NULL)
            free(Path);
        if (Name != NULL)
            free(Name);
    }
    BOOL Set(const char* path, const char* name, const CQuadWord& size, DWORD attr,
             const FILETIME* lastWrite, BOOL isDir);
    // if 'i' refers to Name or Path, returns a pointer to the corresponding variable
    // otherwise fills the buffer 'text' (must be at least 50 characters long) with the appropriate value
    // and returns a pointer to 'text'
    // 'fileNameFormat' determines formatting of names of found items
    char* GetText(int i, char* text, int fileNameFormat);
};

class CFoundFilesListView : public CWindow
{
protected:
    TIndirectArray<CFoundFilesData> Data;
    CRITICAL_SECTION DataCriticalSection; // critical section for accessing data
    CFindDialog* FindDialog;
    TIndirectArray<CFoundFilesData> DataForRefine;

public:
    int EnumFileNamesSourceUID; // UID of the source for name enumeration in viewers

public:
    CFoundFilesListView(HWND dlg, int ctrlID, CFindDialog* findDialog);
    ~CFoundFilesListView();

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL InitColumns();

    void StoreItemsState();
    void RestoreItemsState();

    int CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, int sortBy);
    void QuickSort(int left, int right, int sortBy);
    void SortItems(int sortBy);

    void QuickSortDuplicates(int left, int right, BOOL byName);
    int CompareDuplicatesFunc(CFoundFilesData* f1, CFoundFilesData* f2, BOOL byName);
    void SetDifferentByGroup(); // sets the Different bit based on Group so that the Different bit alternates at group boundaries

    // interface for Data
    CFoundFilesData* At(int index);
    void DestroyMembers();
    int GetCount();
    int Add(CFoundFilesData* item);
    void Delete(int index);
    BOOL IsGood();
    void ResetState();

    // moves the necessary parts from Data to DataForRefine
    // may only be called  when the search thread is not running
    BOOL TakeDataForRefine();
    void DestroyDataForRefine();
    int GetDataForRefineCount();
    CFoundFilesData* GetDataForRefine(int index);

    DWORD GetSelectedListSize();                     // returns how many bytes are needed to store all selected
                                                     // items as "c:\\bla\\bla.txt\0c:\\bla\\bla2.txt\0\0"
                                                     // if no item is selected, returns 2 (two terminators)
    BOOL GetSelectedList(char* list, DWORD maxSize); // fills the list according to GetSelectedListSize
                                                     // without exceeding maxSize

    // scans all selected files and directories and removes those that no longer exist
    // if 'forceRemove' variable is TRUE, selected items are removed without needing checks
    // 'lastFocusedIndex' indicates which index was focused before deletion started
    // 'lastFocusedItem' points to a copy of the focused item so we can try to find it again by name and path
    void CheckAndRemoveSelectedItems(BOOL forceRemove, int lastFocusedIndex, const CFoundFilesData* lastFocusedItem);
};

//****************************************************************************
//
// CFindTBHeader
//

class CToolBar;

class CFindTBHeader : public CWindow
{
protected:
    CToolBar* ToolBar;
    CToolBar* LogToolBar;
    HWND HNotifyWindow; // window to which commands are sent
    char Text[200];
    int FoundCount;
    int ErrorsCount;
    int InfosCount;
    HICON HWarningIcon;
    HICON HInfoIcon;
    HICON HEmptyIcon;
    BOOL WarningDisplayed;
    BOOL InfoDisplayed;
    int FlashIconCounter;
    BOOL StopFlash;

public:
    CFindTBHeader(HWND hDlg, int ctrlID);

    void SetNotifyWindow(HWND hWnd) { HNotifyWindow = hWnd; }

    int GetNeededHeight();

    BOOL EnableItem(DWORD position, BOOL byPosition, BOOL enabled);

    void SetFoundCount(int foundCount);
    void SetErrorsInfosCount(int errorsCount, int infosCount);

    void OnColorsChange();

    BOOL CreateLogToolbar(BOOL errors, BOOL infos);

    void StartFlashIcon();
    void StopFlashIcon();

    void SetFont();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*********************************************************************************
//
// CFindDialog
//

enum CCopyNameToClipboardModeEnum
{
    cntcmFullName,
    cntcmName,
    cntcmFullPath,
    cntcmUNCName,
};

enum CStateOfFindCloseQueryEnum
{
    sofcqNotUsed,     // idle, nothing is happening
    sofcqSentToFind,  // request sent; the response has not arrived yet or the user has not answered "stop searching?"
    sofcqCanClose,    // the Find window can be closed
    sofcqCannotClose, // the Find window cannot be closed
};

class CComboboxEdit;
class CButton;

class CFindDialog : public CCommonDialog
{
protected:
    // data needed for laying out the dialog
    BOOL FirstWMSize;
    int VMargin; // space on the left and right between the dialog frame and controls
    int HMargin; // space below between buttons and the status bar
    int ButtonW; // button width
    int ButtonH;
    int RegExpButtonW; // size of the RegExpBrowse button
    int RegExpButtonY; // position of the RegExpBrowse button
    int MenuBarHeight; // height of the menu bar
    int StatusHeight;  // height of the status bar
    int ResultsY;      // position of the results list
    int AdvancedY;     // position of the Advanced button
    int AdvancedTextY; // position of the text after the Advanced button
    int AdvancedTextX; // position of the text after the Advanced button
    int FindTextY;     // position of the header above the results
    int FindTextH;     // height of the header
    int CombosX;       // position of the comboboxes
    int CombosH;       // height of the comboboxes
    int BrowseY;       // position of the Browse button
    int Line2X;        // position of the separator line of Search file content
    int FindNowY;      // position of the Find now button
    int SpacerH;       // height by which the dialog shrinks or expands

    BOOL Expanded; // dialog is expanded - the SearchFileContent items are visible

    int MinDlgW; // minimal width of the Find dialog
    int MinDlgH; // minimal height

    int FileNameFormat; // how to adjust filenames after reading from disk, taken from global configuration due to synchronization issues
    BOOL SkipCharacter; // prevents a beep when Alt+Enter is pressed in Find

    // additional data
    BOOL DlgFailed;
    CMenuPopup* MainMenu;
    CMenuBar* MenuBar;
    HWND HStatusBar;
    HWND HProgressBar; // status bar child window shown for certain operations in a special field
    BOOL TwoParts;     // does the status bar have two texts?
                       //    CFindAdvancedDialog FindAdvanced;
    CFoundFilesListView* FoundFilesListView;
    char FoundFilesDataTextBuffer[MAX_PATH]; // for obtaining text from CFoundFilesData::GetText
    CFindTBHeader* TBHeader;
    BOOL SearchInProgress;
    BOOL CanClose; // the window can be closed (we are not inside a method of this object)
    HANDLE GrepThread;
    CGrepData GrepData;
    CSearchingString SearchingText;
    CSearchingString SearchingText2;
    CComboboxEdit* EditLine;
    BOOL UpdateStatusBar;
    IContextMenu2* ContextMenu;
    CFindDialog** ZeroOnDestroy; // the pointer will be zeroed on destruction
    CButton* OKButton;

    BOOL OleInitialized;

    TIndirectArray<CSearchForData> SearchForData; // list of directories and masks that will be searched

    // a single item worked with by both the Find dialog and the Advanced dialog
    CFindOptionsItem Data;

    BOOL ProcessingEscape; // the message loop is currently handling ESCAPE -- if
                           // IDCANCEL is generated, a confirmation is shown

    CFindLog Log; // storage for errors and information

    CBitmap* CacheBitmap; // used when drawing the path

    BOOL FlashIconsOnActivation; // flash the status icons when we get activated

    char FindNowText[100];

public:
    CStateOfFindCloseQueryEnum StateOfFindCloseQuery; // main thread asks the Find thread whether the window can close; unsynchronized, used only during shutdown, more than enough...

public:
    CFindDialog(HWND hCenterAgainst, const char* initPath);
    ~CFindDialog();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    BOOL IsGood() { return EditLine != NULL; }

    void SetZeroOnDestroy(CFindDialog** zeroOnDestroy) { ZeroOnDestroy = zeroOnDestroy; }

    BOOL GetFocusedFile(char* buffer, int bufferLen, int* viewedIndex /* can be NULL */);
    const char* GetName(int index);
    const char* GetPath(int index);
    void UpdateInternalViewerData();

    BOOL IsSearchInProgress() { return SearchInProgress; }

    void OnEnterIdle();

    // If the message is translated, the return value is TRUE.
    BOOL IsMenuBarMessage(CONST MSG* lpMsg);

    // allocates the appropriate data for selected items
    HGLOBAL CreateHDrop();
    HGLOBAL CreateShellIdList();

    // the main window calls this method for all Find dialogs - colors have changed
    void OnColorsChange();

    void SetProcessingEscape(BOOL value) { ProcessingEscape = value; }

    // allows processing Alt+C and other hotkeys that belong to hidden controls:
    // if the dialog is collapsed and a hidden control's hot key is pressed,
    // the dialog expands. Always returns FALSE
    BOOL ManageHiddenShortcuts(const MSG* msg);

protected:
    void GetLayoutParams();
    void LayoutControls(); // arranges controls within the dialog

    void SetTwoStatusParts(BOOL two, BOOL force = FALSE); // sets one or two status bar parts; sizes are adjusted according to the status bar length

    void SetContentVisible(BOOL visible);
    void UpdateAdvancedText();

    void LoadControls(int index); // loads the item from Items[index] into the dialog

    void StartSearch(WORD command);
    void StopSearch();

    void BuildSerchForData(); // fills the SearchForData list

    void EnableControls(BOOL nextIsButton = FALSE);
    void EnableToolBar();

    void InsertDrives(HWND hEdit, BOOL network); // fills hEdit with a list of fixed drives (and network drives if requested)

    void UpdateListViewItems();

    void OnContextMenu(int x, int y);
    void OnFocusFile();
    void OnViewFile(BOOL alternate);
    void OnEditFile();
    void OnViewFileWith();
    void OnEditFileWith();
    void OnHideSelection();
    void OnHideDuplicateNames();
    void OnDelete(BOOL toRecycle);
    void OnSelectAll();
    void OnInvertSelection();
    void OnShowLog();
    void OnOpen(BOOL onlyFocused);
    void UpdateStatusText(); // if we are not in search mode, displays the count and total size of selected items

    // OLE clipboard operations

    // creates a context menu for the selected items and calls ContextMenuInvoke for the specified lpVerb
    // returns TRUE if Invoke was called, otherwise returns FALSE if something fails
    BOOL InvokeContextMenu(const char* lpVerb);

    void OnCutOrCopy(BOOL cut);
    void OnDrag(BOOL rightMouseButton);

    void OnProperties();

    void OnUserMenu();

    void OnCopyNameToClipboard(CCopyNameToClipboardModeEnum mode);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // iterates over selected list view items and tries to find a common parent directory
    // if it founds it, it copies it to the buffer and returns TRUE
    // if it does not found it or the buffer is too small, it returns FALSE
    BOOL GetCommonPrefixPath(char* buffer, int bufferMax, int& commonPrefixChars);

    BOOL InitializeOle();
    void UninitializeOle();

    BOOL CanCloseWindow();

    BOOL DoYouWantToStopSearching();

    void SetFullRowSelect(BOOL fullRow);

    friend class CFoundFilesListView;
};

class CFindDialogQueue : public CWindowQueue
{
public:
    CFindDialogQueue(const char* queueName) : CWindowQueue(queueName) {}

    void AddToArray(TDirectArray<HWND>& arr);
};

//*********************************************************************************
//
// externs
//

BOOL OpenFindDialog(HWND hCenterAgainst, const char* initPath);

extern CFindOptions FindOptions;
extern CFindIgnore FindIgnore;
extern CFindDialogQueue FindDialogQueue; // list of all Find dialogs
