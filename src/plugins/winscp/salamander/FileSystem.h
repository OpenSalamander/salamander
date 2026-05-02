// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include <FileOperationProgress.h>
#include <Interface.h>
#include <WinInterface.h>
#include <Terminal.h>
#include <spl_fs.h>
//---------------------------------------------------------------------------
class TProgressForm;
class TSynchronizeProgressForm;
class TAuthenticateForm;
class TTerminalQueue;
class CPluginInterface;
class CPluginFSDataInterface;
class TSynchronizeController;
class TSalamandQueueController;
class TSynchronizeController;
//---------------------------------------------------------------------------
class CPluginFSInterface : public CPluginFSInterfaceAbstract,
                           protected CSalamanderGeneralLocal
{
    friend class CPluginFSDataInterface;

public:
    CPluginFSInterface(CPluginInterface* APlugin);
    ~CPluginFSInterface();

    void __fastcall Connect(TSessionData* Data);
    void __fastcall ForceClose();
    void __fastcall RecryptPasswords();

    void __fastcall FullSynchronize(bool Source);
    void __fastcall Synchronize();
    int __fastcall FileMenu(HWND Parent, int X, int Y);

    virtual BOOL WINAPI GetCurrentPath(char* UserPart);
    virtual BOOL WINAPI GetFullName(CFileData& File, int IsDir, char* Buf,
                                    int BufSize);
    virtual BOOL WINAPI GetFullFSPath(HWND Parent, const char* FSName,
                                      char* Path, int PathSize, BOOL& Success);
    virtual BOOL WINAPI GetRootPath(char* UserPart);

    virtual BOOL WINAPI IsCurrentPath(int CurrentFSNameIndex, int FSNameIndex,
                                      const char* UserPart);
    virtual BOOL WINAPI IsOurPath(int CurrentFSNameIndex, int FSNameIndex,
                                  const char* UserPart);

    virtual BOOL WINAPI ChangePath(int CurrentFSNameIndex, char* FSName,
                                   int FSNameIndex, const char* UserPart, char* CutFileName,
                                   BOOL* PathWasCut, BOOL ForceRefresh, int Mode);
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* Dir,
                                        CPluginDataInterfaceAbstract*& PluginData, int& IconsType,
                                        BOOL ForceRefresh);

    virtual BOOL WINAPI TryCloseOrDetach(BOOL ForceClose, BOOL CanDetach,
                                         BOOL& Detach, int Reason);

    virtual void WINAPI Event(int Event, DWORD Param);

    virtual void WINAPI ReleaseObject(HWND Parent);

    virtual DWORD WINAPI GetSupportedServices();

    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* FSName, char*& Title,
                                                       HICON& Icon, BOOL& DestroyIcon);
    virtual HICON WINAPI GetFSIcon(BOOL& DestroyIcon);
    virtual void WINAPI GetDropEffect(const char* SrcFSPath, const char* TgtFSPath,
                                      DWORD AllowedEffects, DWORD KeyState,
                                      DWORD* DropEffect);
    virtual void WINAPI GetFSFreeSpace(CQuadWord* RetValue) {}
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* Text,
                                                    int PathLen, int& Offset);
    virtual void WINAPI CompleteDirectoryLineHotPath(char* Path, int PathBufSize) {}
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* FSName, int Mode,
                                                  char* Buf, int BufSize)
    {
        return FALSE;
    }
    virtual void WINAPI ShowInfoDialog(const char* FSName, HWND Parent);
    virtual BOOL WINAPI ExecuteCommandLine(HWND Parent, char* Command,
                                           int& SelFrom, int& SelTo);
    virtual BOOL WINAPI QuickRename(const char* FSName, int Mode, HWND Parent, CFileData& File,
                                    BOOL IsDir, char* NewName, BOOL& Cancel);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* FSName,
                                                       const char* Path, BOOL IncludingSubdirs);
    virtual BOOL WINAPI CreateDir(const char* FSName, int Mode, HWND Parent, char* NewName,
                                  BOOL& Cancel);
    virtual void WINAPI ViewFile(const char* FSName, HWND Parent,
                                 CSalamanderForViewFileOnFSAbstract* Salamander, CFileData& File);
    virtual BOOL WINAPI Delete(const char* FSName, int Mode, HWND Parent,
                               int Panel, int SelectedFiles, int SelectedDirs, BOOL& CancelOrError);
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL Copy, int Mode, const char* FSName,
                                         HWND Parent, int Panel, int SelectedFiles, int SelectedDirs,
                                         char* TargetPath, BOOL& OperationMask, BOOL& CancelOrHandlePath,
                                         HWND DropTarget);
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL Copy, int Mode,
                                               const char* FSName, HWND Parent, const char* SourcePath,
                                               SalEnumSelection2 Next, void* NextParam, int SourceFiles, int SourceDirs,
                                               char* TargetPath, BOOL* InvalidPathOrCancel);
    virtual BOOL WINAPI ChangeAttributes(const char* FSName, HWND Parent,
                                         int Panel, int SelectedFiles, int SelectedDirs);
    virtual void WINAPI ShowProperties(const char* FSName, HWND Parent,
                                       int Panel, int SelectedFiles, int SelectedDirs) {}
    virtual void WINAPI ContextMenu(const char* FSName, HWND Parent, int MenuX,
                                    int MenuY, int Type, int Panel, int SelectedFiles, int SelectedDirs);
    virtual BOOL WINAPI OpenFindDialog(const char* FSName, int Panel);
    virtual void WINAPI OpenActiveFolder(const char* FSName, HWND Parent);
    virtual void WINAPI GetAllowedDropEffects(int Mode, const char* TgtFSPath,
                                              DWORD* AllowedEffects);
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                      LRESULT* plResult);
    virtual BOOL WINAPI GetNoItemsInPanelText(char* TextBuf, int TextBufSize);
    virtual void WINAPI ShowSecurityInfo(HWND Parent);

    void __fastcall RefreshPanel();
    AnsiString __fastcall CurrentPath();
    AnsiString __fastcall FullPath(AnsiString Path);

private:
    TTerminal* FTerminal;
    bool FClosing;
    TStringList* FFileList;
    bool FFocusedFileList;
    TProgressForm* FProgressForm;
    TSynchronizeProgressForm* FSynchronizeProgressForm;
    bool FPendingConnect;
    bool FWaitWindowShown;
    bool FPreloaded;
    bool FForceClose;
    CPluginFSDataInterface* FLastDataInterface;
    TTerminalQueue* FQueue;
    TSalamandQueueController* FQueueController;
    TAuthenticateForm* FAuthenticateForm;
    TSynchronizeController* FSynchronizeController;

    void __fastcall Connect(int FSNameIndex, const AnsiString UserPart, bool IgnoreFileName);
    void __fastcall ConnectTerminal(TTerminal* Terminal);
    void __fastcall TerminalClose(TObject* Sender);
    void __fastcall TerminalQueryUser(TObject* Sender,
                                      const AnsiString Query, TStrings* MoreMessages, int Answers,
                                      const TQueryParams* Params, int& Answer, TQueryType Type, void* Arg);
    void __fastcall TerminalPromptUser(TTerminal* Terminal,
                                       TPromptKind Kind, AnsiString Name, AnsiString Instructions,
                                       TStrings* Prompts, TStrings* Results, bool& Result, void* Arg);
    void __fastcall TerminalDisplayBanner(TTerminal* Terminal,
                                          AnsiString SessionName, const AnsiString& Banner, bool& NeverShowAgain,
                                          int Options);
    void __fastcall TerminalShowExtendedException(TTerminal* Terminal,
                                                  Exception* E, void* Arg);
    void __fastcall OperationProgress(
        TFileOperationProgressType& ProgressData, TCancelStatus& Cancel);
    void __fastcall OperationFinished(TFileOperation Operation,
                                      TOperationSide Side, bool DragDrop, const AnsiString& FileName, bool Success,
                                      TOnceDoneOperation& OnceDoneOperation);
    void __fastcall TerminalSynchronizeDirectory(const AnsiString LocalDirectory,
                                                 const AnsiString RemoteDirectory, bool& Continue, bool Collect);
    void __fastcall TerminalStartReadDirectory(TObject* Sender);
    void __fastcall TerminalReadDirectory(TObject* Sender, bool ReloadOnly);
    void __fastcall TerminalInformation(TTerminal* Terminal, const AnsiString& Str,
                                        bool Status, bool Active);
    bool __fastcall HandleException(Exception* E);
    void __fastcall SaveSession();
    void __fastcall PathChanged(const AnsiString Path, bool IncludingSubDirs);
    void __fastcall OurPathChanged(const AnsiString Path, bool IncludingSubDirs);
    void __fastcall CurrentPathChanged(bool IncludingSubDirs);
    void __fastcall CreateFileList(int Panel, TOperationSide Side, bool Focused);
    void __fastcall CreateFileList(AnsiString SourcePath,
                                   SalEnumSelection2 Next, void* NextParam);
    void __fastcall CreateFileList(CFileData* File, TOperationSide Side);
    void __fastcall DestroyFileList();
    AnsiString __fastcall CurrentFullPath();
    bool __fastcall AffectsCurrentPath(const AnsiString Path,
                                       bool IncludingSubDirs);
    bool __fastcall GetPathType(const AnsiString Path, int& PathType);
    bool __fastcall GetPanel(int& Panel, bool Opposite = false);
    AnsiString __fastcall FSName();
    AnsiString __fastcall FullFSPath(AnsiString Path);
    bool __fastcall StripFS(AnsiString& Path, bool AnyFS);
    bool __fastcall UnixPaths();
    void __fastcall DisconnectFromPanel(int Panel);
    void __fastcall DataInterfaceDestroyed(CPluginFSDataInterface* DataInterface);
    void __fastcall ScheduleTimer();
    void __fastcall ProcessQueue();
    bool __fastcall IsOurUserPart(const AnsiString UserPart);
    bool __fastcall RemoteTransfer(AnsiString TargetDirectory, AnsiString FileMask,
                                   bool SubDirs, bool NoConfirm, bool Move);
    void __fastcall RemoteTransfer(bool Move);
    void __fastcall SynchronizeDirectories(AnsiString& LocalDirectory,
                                           AnsiString& RemoteDirectory);
    void __fastcall DoSynchronize(TSynchronizeController* Sender,
                                  const AnsiString LocalDirectory, const AnsiString RemoteDirectory,
                                  const TCopyParamType& CopyParam, const TSynchronizeParamType& Params,
                                  TSynchronizeChecklist** Checklist, TSynchronizeOptions* Options, bool Full);
    void __fastcall DoSynchronizeInvalid(TSynchronizeController* Sender,
                                         const AnsiString Directory, const AnsiString ErrorStr);
    void __fastcall Synchronize(const AnsiString LocalDirectory,
                                const AnsiString RemoteDirectory, TSynchronizeMode Mode,
                                const TCopyParamType& CopyParam, int Params, TSynchronizeChecklist** Checklist,
                                TSynchronizeOptions* Options);
    bool __fastcall SynchronizeAllowSelectedOnly();
    void __fastcall GetSynchronizeOptions(int Params, TSynchronizeOptions& Options);
    void __fastcall DoSynchronizeTooManyDirectories(TSynchronizeController* Sender,
                                                    int& MaxDirectories);
    bool __fastcall EnsureCommandSessionFallback(int Capability);
    TAuthenticateForm* __fastcall MakeAuthenticateForm(TSessionData* Data);
    void __fastcall GetSpaceAvailable(const AnsiString Path,
                                      TSpaceAvailable& ASpaceAvailable, bool& Close);
    void __fastcall CalculateSize(TStrings* FileList, __int64& Size,
                                  TCalculateSizeStats& Stats, bool& Close);
    void __fastcall CalculateChecksum(const AnsiString& Alg, TStrings* FileList,
                                      TCalculatedChecksumEvent OnCalculatedChecksum, bool& Close);
    void __fastcall FileSystemInfo();
};
//---------------------------------------------------------------------------
class CPluginFSDataInterface : public CPluginDataInterfaceAbstract
{
public:
    CPluginFSDataInterface(CPluginFSInterface* AFileSystem);
    ~CPluginFSDataInterface();

    virtual BOOL WINAPI CallReleaseForFiles() { return FALSE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return FALSE; }

    virtual void WINAPI ReleasePluginData(CFileData& File, BOOL IsDir) {}

    virtual void WINAPI GetFileDataForUpDir(const char* ArchivePath,
                                            CFileData& UpDir) {}
    virtual BOOL WINAPI GetFileDataForNewDir(const char* DirName, CFileData& Dir)
    {
        return FALSE;
    }

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize)
    {
        return NULL;
    };
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& File, BOOL IsDir)
    {
        return FALSE;
    }
    virtual HICON WINAPI GetPluginIcon(const CFileData* File, int iconSize, BOOL& DestroyIcon)
    {
        return NULL;
    };
    virtual int WINAPI CompareFilesFromFS(const CFileData* File1, const CFileData* File2)
    {
        return 0;
    }

    virtual void WINAPI SetupView(BOOL LeftPanel, CSalamanderViewAbstract* View,
                                  const char* ArchivePath, const CFileData* UpperDir);
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL LeftPanel, const CColumn* Column,
                                                     int NewFixedWidth);
    virtual void WINAPI ColumnWidthWasChanged(BOOL LeftPanel, const CColumn* Column,
                                              int NewWidth);

    virtual BOOL WINAPI GetInfoLineContent(int Panel, const CFileData* File,
                                           BOOL IsDir, int SelectedFiles, int SelectedDirs, BOOL DisplaySize,
                                           const CQuadWord& SelectedSize, char* Buffer, DWORD* HotTexts,
                                           int& HotTextsCount) { return FALSE; };

    virtual BOOL WINAPI CanBeCopiedToClipboard() { return TRUE; }

    virtual BOOL WINAPI GetByteSize(const CFileData* File, BOOL IsDir, CQuadWord* Size)
    {
        return FALSE;
    }
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* File, BOOL IsDir, SYSTEMTIME* Date)
    {
        return FALSE;
    }
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* File, BOOL IsDir, SYSTEMTIME* Time)
    {
        return FALSE;
    }

    void __fastcall Invalidate();
    bool __fastcall IsInvalid() { return FInvalid; };

private:
    CPluginFSInterface* FFileSystem;
    bool FInvalid;
};
//---------------------------------------------------------------------------
