// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//
// ****************************************************************************

class CSelectDialog : public CCommonDialog
{
public:
    CSelectDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent, char* mask,
                  CObjectOrigin origin = ooStandard)
        : CCommonDialog(modul, resID, helpID, parent, origin) { Mask = mask; }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    char* Mask;
};

//
// ****************************************************************************

class CCopyMoveDialog : public CCommonDialog
{
protected:
    char *Title,
        *Path;
    CTruncatedString* Subject;
    int PathBufSize;
    char** History;
    int HistoryCount;
    BOOL DirectoryHelper;
    int SelectionEnd;

public:
    // 'history' determines whether the dialog will contain a combobox (TRUE) or an editline (FALSE)
    // 'directoryHelper' specifies if a resource with a button behind the editline will be used to select a directory
    // 'selectionEnd' specifies up to which character the name is selected (used for quick rename), -1 == all
    CCopyMoveDialog(HWND parent, char* path, int pathBufSize, char* title,
                    CTruncatedString* subject, DWORD helpID,
                    char* history[], int historyCount, BOOL directoryHelper);

    void SetSelectionEnd(int selectionEnd);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CEditNewFileDialog : public CCopyMoveDialog
{
public:
    CEditNewFileDialog(HWND parent, char* path, int pathBufSize, CTruncatedString* subject, char* history[], int historyCount);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCriteriaData;
class CButton;

class CCopyMoveMoreDialog : public CCommonDialog
{
protected:
    char *Title,
        *Path;
    CTruncatedString* Subject;
    int PathBufSize;
    char** History;
    int HistoryCount;
    CCriteriaData* CriteriaInOut; // used to transfer data in and out of the dialog (on OK)
    CCriteriaData* Criteria;      // allocated because static declaration would require juggling headers
    BOOL HavePermissions;
    BOOL SupportsADS;

    int OriginalWidth;    // full dialog width
    int OriginalHeight;   // full dialog height
    int OriginalButtonsY; // Y position of the buttons in client coordinates
    int SpacerHeight;     // spacer used when shrinking/expanding the dialog
    BOOL Expanded;        // is the dialog currently expanded?

    CButton* MoreButton;

public:
    // 'history' determines whether the dialog will contain a combobox (TRUE) or an editline (FALSE)
    // 'directoryHelper' specifies if a resource with a button behind the editline will be used to select a directory
    CCopyMoveMoreDialog(HWND parent, char* path, int pathBufSize, char* title,
                        CTruncatedString* subject, DWORD helpID,
                        char* history[], int historyCount, CCriteriaData* criteriaInOut,
                        BOOL havePermissions, BOOL supportsADS);
    ~CCopyMoveMoreDialog();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void SetOptionsButtonState(BOOL more);
    void DisplayMore(BOOL more, BOOL fast); // fast means everything is freshly initialized and we don't need to reset values
    HDWP OffsetControl(HDWP hdwp, int id, int yOffset);
    void EnableControls();
    void TransferCriteriaControls(CTransferInfo& ti);
    void UpdateAdvancedText();
};

//
// ****************************************************************************

#define MESSAGEBOX_MAXBUTTONS 4 // maximum number of buttons

class CMessageBox : public CCommonDialog
{
protected:
    DWORD Flags;
    char* Title;
    char* CheckText;
    CTruncatedString Text;
    BOOL* Check;
    HICON HOwnIcon;
    MSGBOXEX_CALLBACK HelpCallback;
    char* AliasBtnNames;
    char* URL;
    char* URLText;
    // for WM_COPY:
    int ButtonsID[MESSAGEBOX_MAXBUTTONS]; // IDs of the buttons after remapping
    int BackgroundSeparator;              // Y offset dividing white/gray (Vista+)

public:
    CMessageBox(HWND parent, DWORD flags, const char* title, const char* text,
                const char* checkText, BOOL* check, HICON hOwnIcon,
                DWORD contextHelpId, MSGBOXEX_CALLBACK helpCallback,
                const char* aliasBtnNames, const char* url, const char* urlText);

    CMessageBox(HWND parent, DWORD flags, const char* title, CTruncatedString* text,
                const char* checkText, BOOL* check, HICON hOwnIcon,
                DWORD contextHelpId, MSGBOXEX_CALLBACK helpCallback,
                const char* aliasBtnNames, const char* url, const char* urlText);

    ~CMessageBox();

    virtual void Transfer(CTransferInfo& ti);

    int Execute();

    BOOL CopyToClipboard();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL EscapeEnabled();
};

//
// ****************************************************************************

class CChangeAttrDialog : public CCommonDialog
{
private:
    // handles for the TimeDate controls
    HWND HModifiedDate;
    HWND HModifiedTime;
    HWND HCreatedDate;
    HWND HCreatedTime;
    HWND HAccessedDate;
    HWND HAccessedTime;

    // state variables used to disable checkboxes
    BOOL SelectionContainsDirectory;
    BOOL FileBasedCompression;
    BOOL FileBasedEncryption;

    // variable is set to TRUE when the user clicks the corresponding checkbox
    BOOL ArchiveDirty;
    BOOL ReadOnlyDirty;
    BOOL HiddenDirty;
    BOOL SystemDirty;
    BOOL CompressedDirty;
    BOOL EncryptedDirty;

public:
    int Archive,
        ReadOnly,
        Hidden,
        System,
        Compressed,
        Encrypted,
        ChangeTimeModified,
        ChangeTimeCreated,
        ChangeTimeAccessed,
        RecurseSubDirs;

    SYSTEMTIME TimeModified;
    SYSTEMTIME TimeCreated;
    SYSTEMTIME TimeAccessed;

    CChangeAttrDialog(HWND parent, DWORD attr, DWORD attrDiff,
                      BOOL selectedDirectory, BOOL fileBasedCompression,
                      BOOL fileBasedEncryption,
                      const SYSTEMTIME* timeModified,
                      const SYSTEMTIME* timeCreated,
                      const SYSTEMTIME* timeAccessed);

    virtual void Transfer(CTransferInfo& ti);

    void EnableWindows();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL GetAndValidateTime(CTransferInfo* ti, int resIDDate, int resIDTime, SYSTEMTIME* time);
};

//******************************************************************************
//
// CProgressDlgArray
//

struct CProgressDlgArrItem
{
    HANDLE DlgThread; // handle of the dialog thread (may also be a handle of a terminated dialog thread)
    HWND DlgWindow;   // handle of the dialog window (NULL = the dialog has already closed)

    CProgressDlgArrItem()
    {
        DlgThread = NULL;
        DlgWindow = NULL;
    }
};

class CProgressDlgArray
{
protected:
    CRITICAL_SECTION Monitor;                 // section used to synchronize this object (behaves like a monitor)
    TIndirectArray<CProgressDlgArrItem> Dlgs; // array of operation dialogs (only those running in their own threads)

public:
    CProgressDlgArray();
    ~CProgressDlgArray();

    // allocates and inserts a structure for a new dialog into the array (data are filled in outside)
    // returns NULL on insufficient memory, otherwise returns the requested structure
    // call only from the main thread (otherwise a conflict with cleanup of finished threads may occur)
    CProgressDlgArrItem* PrepareNewDlg();

    // within the array's critical section set data in 'dlg' according to 'dlgThread' and 'dlgWindow'
    // data change only to values different from NULL (parameter NULL = no change)
    void SetDlgData(CProgressDlgArrItem* dlg, HANDLE dlgThread, HWND dlgWindow);

    // removes the 'dlg' structure from the array; 'dlg->DlgThread' must be NULL;
    // call only if starting the dialog for which 'dlg' was acquired via 
    // PrepareNewDlg() function failed
    void RemoveDlg(CProgressDlgArrItem* dlg);

    // removes all dialogs whose threads have already finished from the array (closes their handles);
    // returns the number of still running operation dialog threads (so when it returns zero, 
    // for example Salamander can be terminated)
    int RemoveFinishedDlgs();

    // finds a dialog with window 'hdlg' in the array and stores NULL to its 'DlgWindow'
    // (this way the dialog reports it is closing; the dialog thread should end shortly after)
    void ClearDlgWindow(HWND hdlg);

    // returns the next open dialog; if no dialog is open, returns NULL;
    // before the first call set 'index' to 0, use the returned value of 'index' 
    // for subsequent calls (do not touch 'index' between calls); dialogs are returned 
    // in cycles (after the last one it returns to the first)
    HWND GetNextOpenedDlg(int* index);

    // ensures all open dialogs are closed
    // call only from the main thread (otherwise another dialog might open and it won't 
    // know it should terminate)
    void PostCancelToAllDlgs();

    // sends a message to all dialogs that the icon (color) has changed and needs 
    // to be set again; call only from the main thread
    void PostIconChange();
};

//******************************************************************************
//
// CProgressDialog
//

struct CChangeAttrsData;
struct CConvertData;
class COperations;
class CStaticText;
class CProgressBar;
struct CStartProgressDialogData;

// returns FALSE if the progress dialog could not be opened in the new thread or 
// if starting an operation in the worker thread failed in this dialog; when FALSE 
// is returned the caller must free the script 'script' manually (otherwise the script
// is freed after the operation in the worker thread finishes)
BOOL StartProgressDialog(COperations* script, const char* caption,
                         CChangeAttrsData* attrsData, CConvertData* convertData);

class CProgressDialog : public CCommonDialog
{
public:
    CProgressDialog(HWND parent, COperations* script, const char* caption,
                    CChangeAttrsData* attrsData, CConvertData* convertData,
                    BOOL runningInOwnThread, CStartProgressDialogData* progrDlgData);
    ~CProgressDialog();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL FlushCachedData(); // sends modified data to statics and progress bars; returns TRUE if there was something to update (something was dirty)

    void SetDlgTitle(BOOL minimized);
    void SetWindowIcon();

protected:
    BOOL RunningInOwnThread;                // TRUE/FALSE = dialog runs in its own thread ("background") / dialog runs in the main thread and is modal to its parent (usually one of the panels)
    CStartProgressDialogData* ProgrDlgData; // non-NULL only if the dialog runs in its own thread and the main thread hasn't been resumed yet (waiting for dialog opening and operation start)

    HANDLE Worker;                // worker thread associated with this dialog (NULL if it doesn't exist yet/any more)
    HANDLE WContinue;             // multi-purpose event
    HANDLE WorkerNotSuspended;    // non-signaled == the worker should enter suspend mode
    BOOL CancelWorker;            // if TRUE, the worker thread will terminate
    int OperationProgress;        // progress value shared with the worker thread
    int SummaryProgress;          // progress value shared with the worker thread
    BOOL ShowPause;               // the "pause" button text: TRUE = pause, FALSE = resume
    BOOL IsInQueue;               // TRUE = the operation is queued (requested by the user and successfully added)
    BOOL AutoPaused;              // TRUE if the operation is queued and therefore paused
    BOOL StatusPaused;            // TRUE = the operation is stopped, e.g. when querying Cancel (+ other dialogs)
    DWORD NextTimeLeftUpdateTime; // time of the next allowed time-left update (frequent updates hurt for long times)
    CQuadWord TimeLeftLastValue;  // last displayed time-left value
    CITaskBarList3 TaskBarList3;  // controls taskbar progress since Windows 7

    CProgressBar *Operation,
        *Summary;
    CStaticText *OperationText,
        *Source,
        *Target,
        *Status;
    COperations* Script;
    char Caption[50];
    CChangeAttrsData* AttrsData;
    CConvertData* ConvertData;
    BOOL AcceptCommands;

    HWND HPreposition;

    BOOL CanClose; // prevent unwanted closing (handled inside this object's methods)

    BOOL TimerIsRunning; // if TRUE, a timer for text changes and the progress bar updates is running

    BOOL FirstUserSetDialog; // TRUE = WM_USER_SETDIALOG message isn't processed yet (the first call forces a repaint so the user sees the dialog at least flash)

    HWND NextForegroundWindow; // window that should be foreground after closing this dialog (on XP with service pack 1 and with a top-most window opened the activation may fail after closing the dialog - focus sometimes went to the top-most window instead of Salamander)

    BOOL DoNotBeepOnClose; // TRUE = operation cancels because Salamander is exiting; beeping makes no sense

    // texts are stored in a cache and drawn when the timer fires
    BOOL CacheIsDirty;
    char OperationCache[100];
    char PrepositionCache[100];
    char SourceCache[2 * MAX_PATH];
    char TargetCache[2 * MAX_PATH];

    // values are stored and drawn only when the timer fires
    BOOL OperationProgressCacheIsDirty;
    int OperationProgressCache;
    BOOL SummaryProgressCacheIsDirty;
    int SummaryProgressCache;
};

//
// ****************************************************************************

class CFileErrorDlg : public CCommonDialog
{
public:
    CFileErrorDlg(HWND parent, const char* caption, const char* file, const char* error,
                  BOOL noSkip = FALSE, int altRes = 0);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *Caption,
        *File,
        *Error;
};

//
// ****************************************************************************

class CErrorReadingADSDlg : public CCommonDialog
{
public:
    CErrorReadingADSDlg(HWND parent, const char* file, const char* error, const char* title = NULL);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *File,
        *Error,
        *Title;
};

//
// ****************************************************************************

class CErrorSettingAttrsDlg : public CCommonDialog
{
public:
    CErrorSettingAttrsDlg(HWND parent, const char* file, DWORD neededAttrs, DWORD currentAttrs);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* File;
    DWORD NeededAttrs;
    DWORD CurrentAttrs;
};

//
// ****************************************************************************

class CErrorCopyingPermissionsDlg : public CCommonDialog
{
public:
    CErrorCopyingPermissionsDlg(HWND parent, const char* sourceFile,
                                const char* targetFile, DWORD error);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* SourceFile;
    const char* TargetFile;
    DWORD Error;
};

//
// ****************************************************************************

class CErrorCopyingDirTimeDlg : public CCommonDialog
{
public:
    CErrorCopyingDirTimeDlg(HWND parent, const char* targetFile, DWORD error);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* TargetFile;
    DWORD Error;
};

//
// ****************************************************************************

class COverwriteDlg : public CCommonDialog
{
public:
    COverwriteDlg(HWND parent, const char* sourceName, const char* sourceAttr,
                  const char* targetName, const char* targetAttr, BOOL yesnocancel = FALSE,
                  BOOL dirOverwrite = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *SourceName,
        *SourceAttr,
        *TargetName,
        *TargetAttr;
};

//
// ****************************************************************************

class CHiddenOrSystemDlg : public CCommonDialog
{
public:
    CHiddenOrSystemDlg(HWND parent, const char* caption, const char* name,
                       const char* error, BOOL yesnocancel = FALSE, BOOL yesallcancel = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *Caption,
        *Name,
        *Error;
};

//
// ****************************************************************************

class CConfirmADSLossDlg : public CCommonDialog
{
public:
    CConfirmADSLossDlg(HWND parent, BOOL isFile, const char* name, const char* streams, BOOL isMove);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Name;
    const char* Streams;
    BOOL IsFile;
    BOOL IsMove;
};

//
// ****************************************************************************

class CConfirmLinkTgtCopyDlg : public CCommonDialog
{
public:
    CConfirmLinkTgtCopyDlg(HWND parent, const char* name, const char* details);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Name;
    const char* Details;
};

//
// ****************************************************************************

class CConfirmEncryptionLossDlg : public CCommonDialog
{
public:
    CConfirmEncryptionLossDlg(HWND parent, BOOL isFile, const char* name, BOOL isMove);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Name;
    BOOL IsFile;
    BOOL IsMove;
};

//
// ****************************************************************************

class CCannotMoveDlg : public CCommonDialog
{
public:
    CCannotMoveDlg(HWND parent, int resID, char* sourceName, char* targetName,
                   char* error);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    char *SourceName,
        *TargetName,
        *Error;
};

//
// ****************************************************************************

class CSizeResultsDlg : public CCommonDialog
{
public:
    CSizeResultsDlg(HWND parent, const CQuadWord& size, const CQuadWord& compressed,
                    const CQuadWord& occupied, int files, int dirs,
                    TDirectArray<CQuadWord>* sizes);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void UpdateEstimate();

    CQuadWord Size, Compressed, Occupied;
    int Files, Dirs;
    TDirectArray<CQuadWord>* Sizes;
    char UnknownText[100];
};

//
// ****************************************************************************

class CColorGraph;

class CDriveInfo : public CCommonDialog
{
protected:
    char VolumePath[MAX_PATH]; // which drive information should be shown (either the root or a junction point)
    char OldVolumeName[1000];  // for change detection
    CColorGraph* Graph;
    HICON HDriveIcon;

public:
    CDriveInfo(HWND parent, const char* path, CObjectOrigin origin = ooStandard);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void GrowWidth(int resID, int& width);
};

//
// ****************************************************************************

class CChangeCaseDlg : public CCommonDialog
{
private:
    BOOL SelectionContainsDirectory;

public:
    int FileNameFormat; // numbers compatible with AlterFileName function
    int Change;         // which part of the name should be modified  --||--
    BOOL SubDirs;       // including subdirectories?

    CChangeCaseDlg(HWND parent, BOOL selectionContainsDirectory);

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************

class CConvertFilesDlg : public CCommonDialog
{
private:
    BOOL SelectionContainsDirectory;

public:
    char Mask[MAX_PATH]; // which files will be converted?
    int Change;          // which conversion should be performed?
    BOOL SubDirs;        // include subdirectories?
    int CodeType;        // selected encoding (0 = none)
    int EOFType;         // selected line endings (0 = none)
                         // 1 = CRLF
                         // 2 = LF
                         // 3 = CR

    CConvertFilesDlg(HWND parent, BOOL selectionContainsDirectory);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void UpdateCodingText();
    //    void UpdateEOFText();

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CFilterDialog : public CCommonDialog
{
protected:
    CMaskGroup* Filter;
    BOOL* UseFilter;
    //    BOOL       *Inverse;
    char** FilterHistory;

public:
    CFilterDialog(HWND parent, CMaskGroup* filter, char** filterHistory,
                  BOOL* use /*, BOOL *inverse*/);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

class CSetSpeedLimDialog : public CCommonDialog
{
protected:
    BOOL* UseSpeedLim; // TRUE = speed limit enabled
    DWORD* SpeedLimit; // speed limit in bytes per second

public:
    CSetSpeedLimDialog(HWND parent, BOOL* useSpeedLim, DWORD* speedLimit);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

#define USERNAME_MAXLEN (256 + 1 + 256 + 1) // user + '@' + domain + '\0'
#define PASSWORD_MAXLEN (256 + 1)
#define DOMAIN_MAXLEN (256 + 1)

class CEnterPasswdDialog : public CCommonDialog
{
public:
    char Passwd[PASSWORD_MAXLEN];
    char User[USERNAME_MAXLEN];

    CEnterPasswdDialog(HWND parent, const char* path, const char* user, CObjectOrigin origin = ooStandard);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Path;
};

//
// ****************************************************************************

class CChangeDirDlg : public CCommonDialog
{
public:
    CChangeDirDlg(HWND parent, char* path, BOOL* sendDirectlyToPlugin);

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    char* Path;
    BOOL* SendDirectlyToPlugin;
};

//
// ****************************************************************************

class CPackerConfig;

class CPackDialog : public CCommonDialog
{
public:
    CPackDialog(HWND parent, char* path, const char* pathAlt,
                CTruncatedString* subject, CPackerConfig* config);

    virtual void Transfer(CTransferInfo& ti);

    void SetSelectionEnd(int selectionEnd);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // changes the extension in buffer 'name' to 'ext'; returns TRUE on success
    BOOL ChangeExtension(char* name, const char* ext);

    char* Path;
    const char* PathAlt;
    CTruncatedString* Subject;
    CPackerConfig* PackerConfig;
    int SelectionEnd;
};

//
// ****************************************************************************

class CUnpackerConfig;

class CUnpackDialog : public CCommonDialog
{
public:
    CUnpackDialog(HWND parent, char* path, const char* pathAlt, char* mask,
                  CTruncatedString* subject, CUnpackerConfig* config,
                  BOOL* delArchiveWhenDone);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableDelArcCheckbox();

    char *Mask,
        *Path;
    const char* PathAlt;
    CTruncatedString* Subject;
    CUnpackerConfig* UnpackerConfig;
    BOOL* DelArchiveWhenDone;
};

//
// ****************************************************************************

class CZIPSizeResultsDlg : public CCommonDialog
{
public:
    CZIPSizeResultsDlg(HWND parent, const CQuadWord& size, int files, int dirs);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    CQuadWord Size;
    int Files, Dirs;
};

//
// ****************************************************************************

class CSplashScreen : public CDialog
{
public:
    CSplashScreen();
    ~CSplashScreen();

    BOOL PaintText(const char* text, int x, int y, BOOL bold, COLORREF clr);
    void SetText(const char* text);
    int GetWidth() { return Width; }
    int GetHeight() { return Height; }

    BOOL PrepareBitmap();
    void DestroyBitmap();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    CBitmap* Bitmap;         // includes the text
    CBitmap* OriginalBitmap; // graphics only, without text
    HFONT HNormalFont;
    HFONT HBoldFont;
    RECT OpenSalR;
    RECT VersionR;
    RECT CopyrightR;
    RECT StatusR;
    int GradientY;
    int Width;
    int Height;
};

//
// ****************************************************************************

class CAboutDialog : public CCommonDialog
{
protected:
    HFONT HSmallFont;
    HBRUSH HGradientBkBrush;
    CBitmap* BackgroundBitmap;

public:
    CAboutDialog(HWND parent);
    ~CAboutDialog();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

struct CExecuteItem;
class CComboboxEdit;

class CFileListDialog : public CCommonDialog
{
protected:
    CComboboxEdit* EditLine;

public:
    CFileListDialog(HWND parent);
    ~CFileListDialog();

    virtual void Validate(CTransferInfo& ti);
    void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

#ifdef USE_BETA_EXPIRATION_DATE

class CBetaExpiredDialog : public CCommonDialog
{
protected:
    int Count;
    char OldOK[30];

public:
    CBetaExpiredDialog(HWND parent);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnTimer();
};

#endif // USE_BETA_EXPIRATION_DATE

//
// ****************************************************************************

class CTaskListDialog : public CCommonDialog
{
protected:
    DWORD DisplayedVersion; // currently shown version of the list

public:
    CTaskListDialog(HWND parent);

protected:
    void Refresh();
    DWORD GetCurPID(); // returns the PID selected in the list box

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CChangeIconDialog : public CCommonDialog
{
protected:
    // for transferring data to and from the dialog
    char* IconFile;
    int* IconIndex;
    BOOL Dirty;
    HICON* Icons;     // array of enumerated icon handles
    DWORD IconsCount; // number of icons in the array

public:
    CChangeIconDialog(HWND hParent, char* iconFile, int* iconIndex);
    ~CChangeIconDialog();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void GetShell32(char* fileName);

    BOOL LoadIcons();    // enumerates icons and fills the Icons array
    void DestroyIcons(); // clears the Icons array
};

//
// ****************************************************************************

class CToolbarHeader;
struct CPluginData;
class CHyperLink;

class CPluginsDlg : public CCommonDialog
{
protected:
    HWND HListView; // our listview
    CToolbarHeader* Header;
    HIMAGELIST HImageList;      // image list for the listview
    BOOL RefreshPanels;         // should the panels be refreshed after closing the dialog?
    BOOL DrivesBarChange;       // should the Drives bars be refreshed after closing the dialog?
    char FocusPlugin[MAX_PATH]; // empty if no plugin should get focus; otherwise contains its path
    CHyperLink* Url;
    char ShowInBarText[200];        // text taken from the checkbox when the dialog opens
    char ShowInChDrvText[200];      // text taken from the checkbox when the dialog opens
    char InstalledPluginsText[200]; // text taken from the listview caption when the dialog opens

public:
    CPluginsDlg(HWND hParent);

    // dialog return values:
    BOOL GetRefreshPanels() { return RefreshPanels; }
    BOOL GetDrivesBarChange() { return DrivesBarChange; }
    const char* GetFocusPlugin() { return FocusPlugin; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InitColumns();     // add columns to the listview
    void SetColumnWidths(); // set optimal column widths
    void RefreshListView(BOOL setOnly = TRUE, int selIndex = -1, const CPluginData* selectPlugin = NULL, BOOL setColumnWidths = FALSE);
    void OnSelChanged();                                                    // selected item in the listview changed
    CPluginData* GetSelectedPlugin(int* index = NULL, int* lvIndex = NULL); // returns NULL if no item is selected; index returns index to the Plugins array; lvIndex returns index within listview, can be NULL
    void EnableButtons(CPluginData* plugin);
    void OnContextMenu(int x, int y); // show the context menu for the selected item at coordinates x, y
    void OnMove(BOOL up);
    void OnSort();
    void EnableHeader();
};

//
// ****************************************************************************

struct CPluginMenuItem;

class CPluginKeys : public CCommonDialog
{
public:
    BOOL Reset; // return value (TRUE if the dialog was closed via Reset)

protected:
    HWND HListView; // our listview
    CToolbarHeader* Header;
    CPluginData* Plugin;
    DWORD* HotKeys; // local copy of hot keys (so cancel works)

public:
    CPluginKeys(HWND hParent, CPluginData* plugin);
    ~CPluginKeys();
    BOOL IsGood();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void InitColumns();     // add columns to the listview
    void SetColumnWidths(); // set optimal column widths
    void RefreshListView(BOOL setOnly = TRUE);

    WORD GetHotKey(BYTE* virtKey = NULL, BYTE* mods = NULL);
    CPluginMenuItem* GetSelectedItem(int* orgIndex); // orgIndex returns index to Plugin array->MenuItems; may be NULL
    CPluginMenuItem* GetItem(int index);
    void EnableButtons();
    void HandleConflictWarning();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************

class CFileTimeStamps;
class CFilesWindow;
class CArchiveUpdateDlg : public CCommonDialog
{
protected:
    CFileTimeStamps* FileStamps;
    CFilesWindow* Panel;

public:
    CArchiveUpdateDlg(HWND hParent, CFileTimeStamps* fileStamps, CFilesWindow* panel);

    void EnableButtons();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CWaitWindow
//
// Modal window used to display information about an operation in progress.
// The text may contain multiple lines separated by '\n'.
// 'showCloseButton' specifies whether the dialog includes a Close button,
// which is effectively the same as pressing Escape but accessible by mouse.
//

class CWaitWindow : public CWindow
{
protected:
    char* Caption;
    char* Text;
    SIZE TextSize;
    HWND HParent;
    HWND HForegroundWnd;
    BOOL ShowCloseButton;
    BOOL NeedWrap;

    BOOL ShowProgressBar;
    RECT BarRect;
    DWORD BarMax;
    DWORD BarPos;

    CBitmap* CacheBitmap; // used for flicker-free text drawing

public:
    CWaitWindow(HWND hParent, int textResID, BOOL showCloseButton, CObjectOrigin origin = ooAllocated, BOOL showProgressBar = FALSE);
    ~CWaitWindow();

    void SetCaption(const char* text); // if not called, the caption will be "Open Salamander"
    void SetText(const char* text);

    void SetProgressMax(DWORD max);
    void SetProgressPos(DWORD pos);

    // if hForegroundWnd is non-NULL the window will be centered relative to it
    // but not shown (display occurs in ThreadSafeWaitWindowFBody)
    HWND Create(HWND hForegroundWnd = NULL);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void PaintProgressBar(HDC dc);
    void PaintText(HDC hDC);
};

//****************************************************************************
//
// CTipOfTheDayWindow and CTipOfTheDayDialog
//
//
/*
class CTipOfTheDayDialog;

class CTipOfTheDayWindow: public CWindow
{
  protected:
    HFONT HHeadingFont;
    HFONT HBodyFont;
    CTipOfTheDayDialog *Parent;

  public:
    CTipOfTheDayWindow();
    ~CTipOfTheDayWindow();

    void PaintBodyText(HDC hDC = NULL);

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  friend class CTipOfTheDayDialog;
};

class CTipOfTheDayDialog: public CCommonDialog
{
  protected:
    CTipOfTheDayWindow  TipWindow;
    TDirectArray<DWORD> Tips;

  public:
    CTipOfTheDayDialog(BOOL quiet);
    ~CTipOfTheDayDialog();

    BOOL IsGood() {return Tips.Count > 0;}
    void IncrementTipIndex();
    void InvalidateTipWindow() {InvalidateRect(TipWindow.HWindow, NULL, FALSE);}

  protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void Transfer(CTransferInfo &ti);

    BOOL LoadTips(BOOL quiet);  // loads tips from the file into the Tips array
    void FreeTips();  // frees the array

  friend class CTipOfTheDayWindow;
};
*/
//****************************************************************************
//
// CImportConfigDialog
//

struct CImportOldKey
{
    char* SalamanderVersion;
    char* SalamanderPath;
};

class CImportConfigDialog : public CCommonDialog
{
public:
    // array corresponding to SalamanderConfigurationRoots array; TRUE:the configuration exists, FALSE:it doesn't
    BOOL ConfigurationExist[SALCFG_ROOTS_COUNT];
    // pointer to the same sized array where the dialog stores TRUE for configurations to delete
    BOOL* DeleteConfigurations;
    // dialog returns here which configuration the user wants to import; -1 -> none
    // index points into the SalamanderConfigurationRoots array
    int IndexOfConfigurationToLoad;

public:
    CImportConfigDialog();
    ~CImportConfigDialog();

protected:
    virtual void Transfer(CTransferInfo& ti);
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CLanguageSelectorDialog
//

class CHyperLink;

class CLanguageSelectorDialog : public CCommonDialog
{
protected:
    TDirectArray<CLanguage> Items;
    CHyperLink* Web;
    char* SLGName;
    BOOL OpenedFromConfiguration;
    BOOL OpenedForPlugin;
    HWND HListView;
    const char* PluginName;
    char ExitButtonLabel[100];

public:
    CLanguageSelectorDialog(HWND hParent, char* slgName, const char* pluginName);
    ~CLanguageSelectorDialog();

    int Execute();

    // scans the 'lang' directory and adds all valid SLG files to the array
    BOOL Initialize(const char* slgSearchPath = NULL, HINSTANCE pluginDLL = NULL);

    int GetLanguagesCount() { return Items.Count; }
    BOOL GetSLGName(char* path, int index = 0); // returns xxxx.slg of the item at index 'index'
    BOOL SLGNameExists(const char* slgName);    // checks whether 'slgName' exists in 'Items'

    void FillControls();

    void LoadListView();

    void Transfer(CTransferInfo& ti);

    // returns the index into Items array; highest priority is 'selectSLGName' (if not NULL), then
    // the Windows setting, and if 'exactMatch' is FALSE then english.slg or otherwise the first
    // found .slg; if 'exactMatch' is TRUE, it returns the index of 'selectSLGName' (if not NULL)
    // or the Windows setting or -1 (nothing found)
    int GetPreferredLanguageIndex(const char* selectSLGName, BOOL exactMatch = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CConversionTablesDialog
//

class CConversionTablesDialog : public CCommonDialog
{
protected:
    HWND HListView;
    char* DirName;

public:
    CConversionTablesDialog(HWND parent, char* dirName);
    ~CConversionTablesDialog();

    void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CSkillLevelDialog
//

class CSkillLevelDialog : public CCommonDialog
{
protected:
    int* Level;

public:
    CSkillLevelDialog(HWND hParent, int* level);

    void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CSharesDialog
//

class CSharesDialog : public CCommonDialog
{
protected:
    HWND HListView;
    CShares SharedDirs; // keep our own instance so nobody 
                        // refreshes it in the background
    BYTE SortBy;        // indicates the column used for sorting
    int FocusedIndex;   // used to return the value

public:
    CSharesDialog(HWND hParent);

    const char* GetFocusedPath(); // returns the path of the selected share; call only after the dialog returns
                                  // returns NULL if "Focus" wasn't clicked and the dialog returned IDOK
                                  // from Execute()

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    HIMAGELIST CreateImageList(); // creates an image list with a single icon (shared folder)

    void InitColumns(); // add columns to the ListView
    void Refresh();     // loads shared folders and adds them to the listview
    static int CALLBACK SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
    void SortItems();                        // sorts items based on the SortBy variable
    int GetFocusedIndex();                   // returns index to SharetDirs array or -1 if no item is selected
    void DeleteShare(const char* shareName); // request the system to remove the share
    void OnContextMenu(int x, int y);        // shows the context menu for the selected item at coordinates x, y
    void EnableControls();                   // button enabler
};

//****************************************************************************
//
// CDisconnectDialog
//

// types for CConnectionItem
enum CConnectionItemType
{
    citGroup,   // marks the following group (network resources, plugins, ...)
    citNetwork, // a network resource
    citPlugin   // a plugin connection
};

#define CONNECTION_ICON_NETWORK 0
#define CONNECTION_ICON_PLUGIN 1
#define CONNECTION_ICON_ACCESSIBLE 2
#define CONNECTION_ICON_INACCESSIBLE 3

// individual items displayed in the Disconnect dialog; filled by EnumConnections()
struct CConnectionItem
{
    CConnectionItemType Type;
    int IconIndex; // icon index in HImageList (CONNECTION_ICON_xxx)
    char* Name;    // Name column
    char* Path;    // Path column
    BOOL Default;  // if TRUE, the item is FOCUSED+SELECTED after the listview is filled; only one item in the array should have Default == TRUE

    // only for Type == citPlugin: FS interface (might not be valid, verify)
    CPluginFSInterfaceAbstract* PluginFS;
};

class CDisconnectDialog : public CCommonDialog
{
protected:
    CFilesWindow* Panel; // panel from which Disconnect was invoked and from which we take the default path
    HWND HListView;
    HIMAGELIST HImageList;
    BOOL NoConncection;
    TDirectArray<CConnectionItem> Connections;

public:
    CDisconnectDialog(CFilesWindow* panel);
    ~CDisconnectDialog();

    const char* GetFocusedPath();                 // returns the path of the selected share; call only after the dialog returns
                                                  // returns NULL if "Focus" wasn't clicked and the dialog returned IDOK
                                                  // from Execute()
    BOOL NoConnection() { return NoConncection; } // returns TRUE if the dialog wasn't opened because there was no connection

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Validate(CTransferInfo& ti);

    HIMAGELIST CreateImageList(); // creates an image list 0-accessible, 1-inaccessible network drive

    void EnumConnections(); // fills the Connections array

    void InitColumns(); // adds columns to the ListView
    void Refresh();

    // disconnects SELECTED items; returns TRUE if everything succeeded and the dialog can close
    // if an error occurs it shows a message box and returns FALSE; the dialog then stays open
    BOOL OnDisconnect();

    void EnableControls(); // button enabler

    // inserts a new item into the Connections array
    // if index == -1 the item is appended to the end of the list
    // if ignoreDuplicate is set, the array is searched first and the item is skipped if already present
    BOOL InsertItem(int index, BOOL ignoreDuplicate, CConnectionItemType type, int iconIndex,
                    const char* name, const char* path, BOOL defaultItem,
                    CPluginFSInterfaceAbstract* pluginFS);

    void DestroyConnections(); // empties/frees the Connections array
};

//****************************************************************************
//
// CSaveSelectionDialog
//

class CSaveSelectionDialog : public CCommonDialog
{
public:
    CSaveSelectionDialog(HWND hParent, BOOL* clipboard);

    virtual void Transfer(CTransferInfo& ti);

protected:
    BOOL* Clipboard;
};

//****************************************************************************
//
// CLoadSelectionDialog
//

enum CLoadSelectionOperation
{
    lsoCOPY,
    lsoOR,
    lsoDIFF,
    lsoAND
};

class CLoadSelectionDialog : public CCommonDialog
{
public:
    CLoadSelectionDialog(HWND hParent, CLoadSelectionOperation* operation, BOOL* clipboard,
                         BOOL clipboardValid, BOOL globalValid);

    virtual void Transfer(CTransferInfo& ti);

protected:
    CLoadSelectionOperation* Operation;
    BOOL* Clipboard;
    BOOL ClipboardValid;
    BOOL GlobalValid;
};

//****************************************************************************
//
// CCompareDirsDialog
//

class CCompareDirsDialog : public CCommonDialog
{
protected:
    BOOL EnableByDateAndTime;
    BOOL EnableBySize;
    BOOL EnableByAttrs;
    BOOL EnableByContent;
    BOOL EnableSubdirs;
    BOOL EnableCompAttrsOfSubdirs;
    CFilesWindow* LeftPanel;
    CFilesWindow* RightPanel;

    int OriginalWidth;    // full dialog width
    int OriginalHeight;   // full dialog height
    int OriginalButtonsY; // Y position of the buttons in client coordinates
    int SpacerHeight;     // spacer used when shrinking/expanding the dialog
    BOOL Expanded;        // are we currently expanded?

public:
    CCompareDirsDialog(HWND hParent, BOOL enableByDateAndTime, BOOL enableBySize,
                       BOOL enableByAttrs, BOOL enableByContent, BOOL enableSubdirs,
                       BOOL enableCompAttrsOfSubdirs, CFilesWindow* leftPanel,
                       CFilesWindow* rightPanel);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
    void DisplayMore(BOOL more);
    HDWP OffsetControl(HDWP hdwp, int id, int yOffset);
};

//****************************************************************************
//
// CCmpDirProgressDialog
//

class CCmpDirProgressDialog : public CCommonDialog
{
protected:
    BOOL HasProgress;
    BOOL Cancel;

    // dialog controls
    CStaticText* Source;
    CStaticText* Target;
    CProgressBar* Progress;
    CProgressBar* TotalProgress;

    char DelayedSource[2 * MAX_PATH]; // text displayed later
    char DelayedTarget[2 * MAX_PATH]; // text displayed later
    BOOL DelayedSourceDirty;
    BOOL DelayedTargetDirty;

    // progress displayed later
    BOOL SizeIsDirty;
    CQuadWord FileSize;
    CQuadWord ActualFileSize;
    CQuadWord TotalSize;
    CQuadWord ActualTotalSize;

    DWORD LastTickCount; // used to detect when the date must be redrawn

    CITaskBarList3* TaskBarList3; // pointer to the interface owned by the Salamander main window

public:
    CCmpDirProgressDialog(HWND hParent, BOOL hasProgress, CITaskBarList3* taskBarList3); // if 'hasProgress' is TRUE, the dialog shows a progress bar

    // text setup
    void SetSource(const char* text);
    void SetTarget(const char* text);

    // the following three methods are relevant only when the progress bar is shown

    // sets the total size of a single file
    void SetFileSize(const CQuadWord& size);
    // sets absolute progress
    void SetActualFileSize(const CQuadWord& size);
    // sets the total size of all files
    void SetTotalSize(const CQuadWord& size);
    // sets the current total size of all files
    void SetActualTotalSize(const CQuadWord& size);
    // retrieves the current total size of all files
    void GetActualTotalSize(CQuadWord& size);
    // changes progress relatively
    void AddSize(const CQuadWord& size);

    // distributes messages; returns FALSE if the user cancelled the operation
    BOOL Continue();

    void FlushDataToControls(); // passes stored values to controls for display

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CExitingOpenSal
//

class CExitingOpenSal : public CCommonDialog
{
protected:
    int NextOpenedDlgIndex;

public:
    CExitingOpenSal(HWND hParent);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CDriveSelectErrDlg : public CCommonDialog
{
public:
    CDriveSelectErrDlg(HWND parent, const char* errText, const char* drvPath);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    const char* ErrText;
    char DrvPath[MAX_PATH];
    int CounterForAllowedUseOfTimer;
};

//
// ****************************************************************************

class CCompareArgsDlg : public CCommonDialog
{
public:
    CCompareArgsDlg(HWND parent, BOOL comparingFiles, char* compareName1,
                    char* compareName2, int* cnfrmShowNamesToCompare);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    BOOL ComparingFiles;
    char* CompareName1;
    char* CompareName2;
    int* CnfrmShowNamesToCompare;
};

extern CProgressDlgArray ProgressDlgArray; // array of disk operation dialogs (only those running in their own threads)
