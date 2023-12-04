// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define OPERATION_BUFFER 16348 // buffer pro Copy

extern int DialogWidth;
extern int DialogHeight;
extern BOOL Maximized;

extern CRenamerOptions LastOptions;
extern char LastMask[MAX_GROUPMASK];
extern BOOL LastSubdirs;
extern BOOL LastRemoveSourcePath;

extern LOGFONT ManualModeLogFont;
extern BOOL UseCustomFont;
extern BOOL ConfirmESCClose;

extern HACCEL HAccels;

extern DWORD Silent;

struct CRenameScriptEntry;

struct CUndoStackEntry
{
    char* Source;
    char* Target;
    CSourceFile* RenamedFile;
    unsigned int IsDir : 1;
    unsigned int Blocks : 1;
    unsigned int Independent : 1; // pro komponenty cesty, aby bylo mozne znovu navazat
                                  // undo operaci
    CUndoStackEntry(char* source, char* target, CSourceFile* renamedFile,
                    BOOL isDir, BOOL blocks);
    ~CUndoStackEntry();
};

class CRenamerDialog : public CDialog
{
protected:
    CRenamerDialog** ZeroOnDestroy; // hodnota ukazatele bude pri destrukci nulovana
    int MinDlgW, MinDlgH;
    int MenuDY;
    int RenameRX, RenameY;
    int CombosLX, CombosRX, CombosDY;
    int ArrowRX;
    int NameArrowY;
    int SearchArrowY;
    int ReplaceArrowY;
    int RuleRX, RuleDY;
    int NameRuleLX;
    int ReplaceRuleLX;
    int CaseRuleLX;
    int ManualDY, ManualLX;
    int CountRX, CountY;
    int PreviewY;
    HFONT ManualModeHFont;
    CSalamanderMaskGroup* SalMaskGroup;
    BOOL SkipAllLongNames;
    BOOL SkipAllBadDirs;
    HWND FocusHWnd;
    int SortBy;
    BOOL TransferDontSaveHistory;
    BOOL WaitCursor;
    BOOL CloseOnEnable;
    char TempFile[MAX_PATH];

    CPreviewWindow* Preview;
    CComboboxEdit *MaskEdit, *NewName, *SearchFor, *ReplaceWith;
    CNotifyEdit* ManualEdit;
    CGUIMenuPopupAbstract* MainMenu;
    CGUIMenuBarAbstract* MenuBar;

    // options
    char DefMask[MAX_GROUPMASK]; // maska z panelu
    char Mask[MAX_GROUPMASK];
    BOOL Subdirs;
    CRenamerOptions RenamerOptions;
    BOOL ProcessRenamed, ProcessNotRenamed;
    BOOL RemoveSourcePath;
    BOOL ManualMode;

    // data for rename operation
    TIndirectArray<CSourceFile> RenamedFiles;
    TIndirectArray<CSourceFile> NotRenamedFiles;
    TIndirectArray<CSourceFile> SourceFiles;
    BOOL SourceFilesValid;
    BOOL SourceFilesNeedUpdate;
    DWORD LastUpdateTime;
    char Root[MAX_PATH];
    int RootLen;

    BOOL Errors;
    CProgressDialog* Progress;
    DWORD Silent;
    BOOL SkipAllOverwrite, SkipAllMove, SkipAllDeleteErr, SkipAllBadWrite,
        SkipAllBadRead, SkipAllOpenOut, SkipAllOpenIn, SkipAllFileDir,
        SkipAllDirChangeCase, SkipAllCreateDir, SkipAllDependingNames,
        SkipAllRemoveDir;

    TIndirectArray<CUndoStackEntry> UndoStack;
    BOOL Undoing;

public:
    CRenamerDialog(HWND parent);
    ~CRenamerDialog();
    void SetZeroOnDestroy(CRenamerDialog** zeroOnDestroy)
    {
        ZeroOnDestroy = zeroOnDestroy;
    }

    BOOL Init();
    void Destroy();

    void GetDlgItemWindowRect(HWND wnd, LPRECT rect);
    void GetDlgItemWindowRect(int id, LPRECT rect);
    static BOOL CALLBACK MoveControls(HWND hwnd, LPARAM lParam);
    void GetLayoutParams();
    void LayoutControls();
    void UpdateMenuItems();
    void UpdateManualModeFont();
    void ShowControls();
    void SetWait(BOOL wait);

    void LoadSelection();
    void ReloadSourceFiles();
    BOOL LoadSubdir(char* path, const char* subdir);

    BOOL ReloadManualModeEdit();

    void OnEnterIdle();
    BOOL IsMenuBarMessage(CONST MSG* lpMsg);

    void SetOptions(CRenamerOptions& options, char* mask,
                    BOOL subdirs, BOOL removeSourcePath);
    void ResetOptions(BOOL soft);
    BOOL TransferForPreview();
    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void Rename(BOOL validate);
    BOOL BuildScript(CRenameScriptEntry*& script, int& count,
                     BOOL validate, BOOL& somethingToDo);
    int GetManualModeNewName(CSourceFile* file, int index,
                             char* newName, char*& newPart);
    void ExecuteScript(CRenameScriptEntry* script, int count);
    void Undo();

    BOOL MoveFile(char* sourceName, char* targetName, char* newPart,
                  BOOL overwrite, BOOL isDir, BOOL& skip);
    BOOL CheckAndCreateDirectory(char* directory, char* newPart, BOOL& skip);
    BOOL CopyFile(char* sourceName, char* targetName, BOOL overwrite,
                  BOOL& skip);

    BOOL ExportToTempFile();
    BOOL ImportFromTempFile();
    BOOL ExecuteCommand(const char* command);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();

    friend CPreviewWindow;
    friend CPluginInterface;
};

class CRenamerDialogThread : public CThread
{
    CRenamerDialog* Dialog;

public:
    CRenamerDialogThread();
    virtual ~CRenamerDialogThread()
    {
        if (Dialog)
            delete Dialog;
    }
    HANDLE Create(CThreadQueue& queue);
    virtual unsigned Body();
};
