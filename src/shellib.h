// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// library initialization
BOOL InitializeShellib();

// library release
void ReleaseShellib();

// Safe call to IContextMenu2::GetCommandString() that avoids the occasional crash in Microsoft code
HRESULT AuxGetCommandString(IContextMenu2* menu, UINT_PTR idCmd, UINT uType, UINT* pReserved, LPSTR pszName, UINT cchMax);

// Callback that returns the names of selected files so that subsequent interfaces can be created
typedef const char* (*CEnumFileNamesFunction)(int index, void* param);

// Creates a data object for drag & drop operations on the selected files and directories from rootDir
IDataObject* CreateIDataObject(HWND hOwnerWindow, const char* rootDir, int files,
                               CEnumFileNamesFunction nextFile, void* param);

// Creates a context menu interface for the selected files and directories from rootDir
IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* rootDir, int files,
                                   CEnumFileNamesFunction nextFile, void* param);

// Creates a context menu's interface for the given directory
IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* dir);

// Does the given directory or file have a drop target?
BOOL HasDropTarget(const char* dir);

// Creates a drop target for drag & drop operations into the given directory or file
IDropTarget* CreateIDropTarget(HWND hOwnerWindow, const char* dir);

// Opens a special folder window
void OpenSpecFolder(HWND hOwnerWindow, int specFolder);

// Opens the folder window 'dir' and sets the focus to 'item'
void OpenFolderAndFocusItem(HWND hOwnerWindow, const char* dir, const char* item);

// Opens a browse dialog and lets the user choose a path (optionally restricted to network paths)
// hCenterWindow - window to which the dialog will be centered
BOOL GetTargetDirectory(HWND parent, HWND hCenterWindow, const char* title, const char* comment,
                        char* path, BOOL onlyNet = FALSE, const char* initDir = NULL);

// Detects whether the path is not a NetHood path (a directory containing the target.lnk file),
// optionally resolves target.lnk and writes the resolved path to 'path'; 'path' is an in/out buffer
// (at least MAX_PATH characters)
void ResolveNetHoodPath(char* path);

class CMenuNew;

// Returns the New menu: the popup-menu handle and the IContextMenu used to execute commands
void GetNewOrBackgroundMenu(HWND hOwnerWindow, const char* dir, CMenuNew* menu,
                            int minCmd, int maxCmd, BOOL backgoundMenu);

struct CDragDropOperData
{
    char SrcPath[MAX_PATH];     // Source path shared by all files/directories from Names ("" == path conversion from Unicode failed)
    TIndirectArray<char> Names; // Sorted, allocated names of files/directories (CF_HDROP does not distinguish whether it is a file or a directory) ("" == path conversion from Unicode failed)

    CDragDropOperData() : Names(200, 200) { SrcPath[0] = 0; }
};

// Determines whether 'pDataObject' contains files and directories from disk and whether all of them share a single path.
// Optionally stores the names in 'namesList' (if it is not NULL).
BOOL IsSimpleSelection(IDataObject* pDataObject, CDragDropOperData* namesList);

// Retrieves the name for 'pidl' via GetDisplayNameOf(flags) (shortens the ID list by one element, obtains
// the folder for the shortened ID list from the desktop, from this folder it calls GetDisplayNameOf for the last ID with the given 'flags'
// on success, it returns TRUE and stores the name in 'name' (buffer of size 'nameSize');
// it does not deallocate 'pidl'; 'alloc' is an interface obtained through CoGetMalloc
BOOL GetSHObjectName(ITEMIDLIST* pidl, DWORD flags, char* name, int nameSize, IMalloc* alloc);

// TRUE = the drag & drop effect was calculated in the plugin FS, so it is not necessary to force Copy
// in CImpIDropSource::GiveFeedback
extern BOOL DragFromPluginFSEffectIsFromPlugin;

//*****************************************************************************
//
// CImpIDropSource
//
// Basic version of the object; behaves normally (default cursors, etc.).
//
// Exception: when dragging from the plugin FS (with possible Copy and Move effects) into Explorer
// to a disk with a TEMP directory, Move is offered by default instead of Copy (which is illogical,
// users expect Copy), so this case is forced by showing
// a different cursor than dwEffect in GiveFeedback, and the final effect is then taken from the
// last cursor shape instead of from the result of DoDragDrop.

class CImpIDropSource : public IDropSource
{
private:
    long RefCount;
    DWORD MouseButton; // -1 = uninitialized value, otherwise MK_LBUTTON or MK_RBUTTON

public:
    // Last effect returned by GiveFeedback method - introduced because
    // DoDragDrop does not return dwEffect == DROPEFFECT_MOVE; for MOVE it returns dwEffect == 0.
    // For the reasons, see "Handling Shell Data Transfer Scenarios", section "Handling Optimized Move Operations":
    // http://msdn.microsoft.com/en-us/library/windows/desktop/bb776904%28v=vs.85%29.aspx
    // (shortly: an optimized Move is done, which means that it does not create a copy to the destination followed by deleting
    //            the original, so that the source does not accidentally delete the original (it may not have been moved yet), and it receives
    //            the operation result DROPEFFECT_NONE or DROPEFFECT_COPY)
    DWORD LastEffect;

    BOOL DragFromPluginFSWithCopyAndMove; // dragging from the plugin FS with possible Copy and Move, details above

public:
    CImpIDropSource(BOOL dragFromPluginFSWithCopyAndMove)
    {
        RefCount = 1;
        MouseButton = -1;
        LastEffect = -1;
        DragFromPluginFSWithCopyAndMove = dragFromPluginFSWithCopyAndMove;
    }

    virtual ~CImpIDropSource()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
    }

    STDMETHOD(QueryInterface)
    (REFIID, void FAR* FAR*);
    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // we must not touch the object, it no longer exists
        }
        return RefCount;
    }

    STDMETHOD(GiveFeedback)
    (DWORD dwEffect)
    {
        if (DragFromPluginFSWithCopyAndMove && !DragFromPluginFSEffectIsFromPlugin)
        {
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if ((!shiftPressed || controlPressed) && (dwEffect & DROPEFFECT_MOVE))
            { // Copy should be performed, but Move is offered -> force this situation, show the Copy cursor, and store Copy into LastEffect
                LastEffect = DROPEFFECT_COPY;
                SetCursor(LoadCursor(HInstance, MAKEINTRESOURCE(IDC_DRAGCOPYEFFECT)));
                return S_OK;
            }
        }
        DragFromPluginFSEffectIsFromPlugin = FALSE;
        LastEffect = dwEffect;
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

    STDMETHOD(QueryContinueDrag)
    (BOOL fEscapePressed, DWORD grfKeyState)
    {
        DWORD mb = grfKeyState & (MK_LBUTTON | MK_RBUTTON);
        if (mb == 0)
            return DRAGDROP_S_DROP;
        if (MouseButton == -1)
            MouseButton = mb;
        if (fEscapePressed || MouseButton != mb)
            return DRAGDROP_S_CANCEL;
        return S_OK;
    }
};

//*****************************************************************************
//
// CImpDropTarget
//
// Calls the defined callbacks to obtain the drop target (directory),
// notifies about the drop or ESC,
// let the system IDropTarget object from IShellFolder perform the remaining operations

// record used by the copy/move callback data
struct CCopyMoveRecord
{
    char* FileName;
    char* MapName;

    CCopyMoveRecord(const char* fileName, const char* mapName);
    CCopyMoveRecord(const wchar_t* fileName, const char* mapName);
    CCopyMoveRecord(const char* fileName, const wchar_t* mapName);
    CCopyMoveRecord(const wchar_t* fileName, const wchar_t* mapName);

    char* AllocChars(const char* name);
    char* AllocChars(const wchar_t* name);
};

// data for the copy and move callback
class CCopyMoveData : public TIndirectArray<CCopyMoveRecord>
{
public:
    BOOL MakeCopyOfName; // TRUE if it should attempt "Copy of..." when the target already exists

public:
    CCopyMoveData(int base, int delta) : TIndirectArray<CCopyMoveRecord>(base, delta)
    {
        MakeCopyOfName = FALSE;
    }
};

// Callback for copy and move operations. It takes care of destroying 'data'.
typedef BOOL (*CDoCopyMove)(BOOL copy, char* targetDir, CCopyMoveData* data,
                            void* param);

// Callback for drag & drop operations. 'copy' is TRUE/FALSE (copy/move), 'toArchive' is TRUE/FALSE
// (to archive/FS), 'archiveOrFSName' (can be NULL if the information should be obtained from the panel)
// is the name of the archive file or the FS name, 'archivePathOrUserPart' is the path in the archive or
// the user part of the FS path, 'data' contains a description of the source files/directories, the function
// handles destruction of the 'data' object, and 'param' is the parameter passed to the CImpDropTarget constructor
typedef void (*CDoDragDropOper)(BOOL copy, BOOL toArchive, const char* archiveOrFSName,
                                const char* archivePathOrUserPart, CDragDropOperData* data,
                                void* param);

// Callback that returns the target directory for the given point 'pt'
typedef const char* (*CGetCurDir)(POINTL& pt, void* param, DWORD* pdwEffect, BOOL rButton,
                                  BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType);

// Callback notifying about the end of the drop operation; drop == FALSE on ESC
typedef void (*CDropEnd)(BOOL drop, BOOL shortcuts, void* param, BOOL ownRutine,
                         BOOL isFakeDataObject, int tgtType);

// Callback for a confirmation prompt before completing the operation (drop)
typedef BOOL (*CConfirmDrop)(DWORD& effect, DWORD& defEffect, DWORD& grfKeyState);

// Callback notifying mouse entering and leaving the target
typedef void (*CEnterLeaveDrop)(BOOL enter, void* param);

// Callback that allows the use of our own routines for copy/move
typedef BOOL (*CUseOwnRutine)(IDataObject* pDataObject);

// Callback that determines the default drop effect when dragging from one FS path to another
typedef void (*CGetFSToFSDropEffect)(const char* srcFSPath, const char* tgtFSPath,
                                     DWORD allowedEffects, DWORD keyState,
                                     DWORD* dropEffect, void* param);

enum CIDTTgtType
{
    idtttWindows,          // files/directories from a Windows path to a Windows path
    idtttArchive,          // files/directories from a Windows path into an archive
    idtttPluginFS,         // files/directories from a Windows path into FS
    idtttArchiveOnWinPath, // archive on a Windows path (drop = pack to archive)
    idtttFullPluginFSPath, // FS to FS
};

class CImpDropTarget : public IDropTarget
{
private:
    long RefCount;
    HWND OwnerWindow;
    IDataObject* OldDataObject;
    BOOL OldDataObjectIsFake;
    int OldDataObjectIsSimple;                 // -1 (unknown value), TRUE/FALSE = is/is not simple (all names share one path)
    int OldDataObjectSrcType;                  // 0 (unknown type), 1/2 = archive/FS
    char OldDataObjectSrcFSPath[2 * MAX_PATH]; // only for FS type: source FS path

    CDoCopyMove DoCopyMove;
    void* DoCopyMoveParam;

    CDoDragDropOper DoDragDropOper;
    void* DoDragDropOperParam;

    CGetCurDir GetCurDir;
    void* GetCurDirParam;

    CDropEnd DropEnd;
    void* DropEndParam;

    CConfirmDrop ConfirmDrop;
    BOOL* ConfirmDropEnable;

    int TgtType; // see CIDTTgtType values; idtttWindows also for archives and FS where dropping the current data object is not possible
    IDropTarget* CurDirDropTarget;
    char CurDir[2 * MAX_PATH];

    CEnterLeaveDrop EnterLeaveDrop;
    void* EnterLeaveDropParam;

    BOOL RButton; // action invoked with the right mouse button

    CUseOwnRutine UseOwnRutine;

    DWORD LastEffect; // last effect detected in DragEnter or DragOver (-1 => invalid)

    CGetFSToFSDropEffect GetFSToFSDropEffect;
    void* GetFSToFSDropEffectParam;

public:
    CImpDropTarget(HWND ownerWindow, CDoCopyMove doCopyMove, void* doCopyMoveParam,
                   CGetCurDir getCurDir, void* getCurDirParam, CDropEnd dropEnd,
                   void* dropEndParam, CConfirmDrop confirmDrop, BOOL* confirmDropEnable,
                   CEnterLeaveDrop enterLeaveDrop, void* enterLeaveDropParam,
                   CUseOwnRutine useOwnRutine, CDoDragDropOper doDragDropOper,
                   void* doDragDropOperParam, CGetFSToFSDropEffect getFSToFSDropEffect,
                   void* getFSToFSDropEffectParam)
    {
        RefCount = 1;
        OwnerWindow = ownerWindow;
        DoCopyMove = doCopyMove;
        DoCopyMoveParam = doCopyMoveParam;
        DoDragDropOper = doDragDropOper;
        DoDragDropOperParam = doDragDropOperParam;
        GetCurDir = getCurDir;
        GetCurDirParam = getCurDirParam;
        TgtType = idtttWindows;
        CurDirDropTarget = NULL;
        CurDir[0] = 0;
        DropEnd = dropEnd;
        DropEndParam = dropEndParam;
        OldDataObject = NULL;
        OldDataObjectIsFake = FALSE;
        OldDataObjectIsSimple = -1; // unknown value
        OldDataObjectSrcType = 0;   // unknown type
        OldDataObjectSrcFSPath[0] = 0;
        ConfirmDrop = confirmDrop;
        ConfirmDropEnable = confirmDropEnable;
        RButton = FALSE;
        EnterLeaveDrop = enterLeaveDrop;
        EnterLeaveDropParam = enterLeaveDropParam;
        UseOwnRutine = useOwnRutine;
        LastEffect = -1;
        GetFSToFSDropEffect = getFSToFSDropEffect;
        GetFSToFSDropEffectParam = getFSToFSDropEffectParam;
    }
    virtual ~CImpDropTarget()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
        if (CurDirDropTarget != NULL)
            CurDirDropTarget->Release();
    }

    void SetDirectory(const char* path, DWORD grfKeyState, POINTL pt,
                      DWORD* effect, IDataObject* dataObject, BOOL tgtIsFile, int tgtType);
    BOOL TryCopyOrMove(BOOL copy, IDataObject* pDataObject, UINT CF_FileMapA,
                       UINT CF_FileMapW, BOOL cfFileMapA, BOOL cfFileMapW);
    BOOL ProcessClipboardData(BOOL copy, const DROPFILES* data, const char* mapA,
                              const wchar_t* mapW);

    STDMETHOD(QueryInterface)
    (REFIID, void FAR* FAR*);
    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // we must not touch the object, it no longer exists
        }
        return RefCount;
    }

    STDMETHOD(DragEnter)
    (IDataObject* pDataObject, DWORD grfKeyState,
     POINTL pt, DWORD* pdwEffect);
    STDMETHOD(DragOver)
    (DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHOD(DragLeave)
    ();
    STDMETHOD(Drop)
    (IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
     DWORD* pdwEffect);
};

struct IShellFolder;
struct IContextMenu;
struct IContextMenu2;

class CMenuNew
{
protected:
    IContextMenu2* Menu2; // IContextMenu2 interface associated with the menu
    HMENU Menu;           // New submenu handle

public:
    CMenuNew() { Init(); }
    ~CMenuNew() { Release(); }

    void Init()
    {
        Menu2 = NULL;
        Menu = NULL;
    }

    void Set(IContextMenu2* menu2, HMENU menu)
    {
        if (menu == NULL)
            return; // is-not-set
        Menu2 = menu2;
        Menu = menu;
    }

    BOOL MenuIsAssigned() { return Menu != NULL; }

    HMENU GetMenu() { return Menu; }
    IContextMenu2* GetMenu2() { return Menu2; }

    void Release();
    void ReleaseBody();
};

//
//*****************************************************************************
// CTextDataObject
//

class CTextDataObject : public IDataObject
{
private:
    long RefCount;
    HGLOBAL Data;

public:
    CTextDataObject(HGLOBAL data)
    {
        RefCount = 1;
        Data = data;
    }
    virtual ~CTextDataObject()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
        NOHANDLES(GlobalFree(Data));
    }

    STDMETHOD(QueryInterface)
    (REFIID, void FAR* FAR*);
    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // we must not touch the object, it no longer exists
        }
        return RefCount;
    }

    STDMETHOD(GetData)
    (FORMATETC* formatEtc, STGMEDIUM* medium);

    STDMETHOD(GetDataHere)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(QueryGetData)
    (FORMATETC* formatEtc)
    {
        if (formatEtc == NULL)
            return E_INVALIDARG;
        if ((formatEtc->cfFormat == CF_TEXT || formatEtc->cfFormat == CF_UNICODETEXT) &&
            (formatEtc->tymed & TYMED_HGLOBAL))
        {
            return S_OK;
        }
        return (formatEtc->tymed & TYMED_HGLOBAL) ? DV_E_FORMATETC : DV_E_TYMED;
    }

    STDMETHOD(GetCanonicalFormatEtc)
    (FORMATETC* pFormatetcIn, FORMATETC* pFormatetcOut)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(SetData)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium, BOOL fRelease)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(EnumFormatEtc)
    (DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(DAdvise)
    (FORMATETC* pFormatetc, DWORD advf, IAdviseSink* pAdvSink,
     DWORD* pdwConnection)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(DUnadvise)
    (DWORD dwConnection)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHOD(EnumDAdvise)
    (IEnumSTATDATA** ppenumAdvise)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
};

// Releases a CopyMoveData
void DestroyCopyMoveData(CCopyMoveData* data);
