// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "mainwnd.h"
#include "plugins.h"
#include "filesbox.h"
#include "fileswnd.h"
#include "stswnd.h"
#include "editwnd.h"
#include "zip.h"
#include "cache.h"
#include "viewer.h"
#include "codetbl.h"
#include "shellib.h"
#include "gui.h"
#include "tasklist.h"
#include "olespy.h"
#include "md5.h"
#include "geticon.h"
#include "pack.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "crypt\fileenc.h"
#include "crypt\sha1.h"
#include "pwdmngr.h"

CPackerConfig PackerConfig;
CUnpackerConfig UnpackerConfig;

const char* STR_NONE = "(none)";

CSalamanderDirectory GlobalEmptySalDir(FALSE); // returns as an empty sal-dir (instead of NULL) - only for archives

HWND ProgressDialogActivateDrop = NULL;

//
// ****************************************************************************
// CZIPUnpackProgress
//

CZIPUnpackProgress::CZIPUnpackProgress() : CCommonDialog(HLanguage, IDD_ZIPUNPACKPROG, NULL, ooStatic)
{
    Init();
}

void CZIPUnpackProgress::Init()
{
    SetTotal(CQuadWord(0, 0), CQuadWord(0, 0));
    ActualSize = CQuadWord(0, 0);
    ActualSize2 = CQuadWord(0, 0);
    Title = NULL;
    Cancel = FALSE;
    SetRemapNames(NULL, NULL);
    int i;
    for (i = 0; i < ZIP_UNPACK_NUMLINES; i++)
        LinesCache[i][0] = 0;
    CacheIndex = 0;
    CacheIsDirty = FALSE;
    SizeIsDirty = FALSE;
    Size2IsDirty = FALSE;
    LastTickCount = 0;
    FileProgress = FALSE;
    TaskBarList3 = NULL;
}

CZIPUnpackProgress::CZIPUnpackProgress(const char* title, HWND parent, const CQuadWord& totalSize, CITaskBarList3* taskBarList3)
    : CCommonDialog(HLanguage, IDD_ZIPUNPACKPROG, parent, ooStatic)
{
    SetTotal(totalSize, CQuadWord(0, 0));
    ActualSize = CQuadWord(0, 0);
    ActualSize2 = CQuadWord(0, 0);
    Title = title;
    Cancel = FALSE;
    SetRemapNames(NULL, NULL);
    int i;
    for (i = 0; i < ZIP_UNPACK_NUMLINES; i++)
        LinesCache[i][0] = 0;
    CacheIndex = 0;
    CacheIsDirty = FALSE;
    SizeIsDirty = FALSE;
    Size2IsDirty = FALSE;
    LastTickCount = 0;
    FileProgress = FALSE;
    TaskBarList3 = taskBarList3;
}

void CZIPUnpackProgress::Set(const char* title, HWND parent, const CQuadWord& totalSize, BOOL fileProgress)
{
    ResID = IDD_ZIPUNPACKPROG;
    SetTotal(totalSize, CQuadWord(0, 0));
    SetParent(parent);
    Title = title;
    ActualSize = CQuadWord(0, 0);
    ActualSize2 = CQuadWord(0, 0);
    FileProgress = fileProgress;
}

void CZIPUnpackProgress::Set(const char* title, HWND parent, const CQuadWord& totalSize1,
                             const CQuadWord& totalSize2)
{
    ResID = IDD_ZIPUNPACKPROG2;
    SetTotal(totalSize1, totalSize2);
    SetParent(parent);
    Title = title;
    ActualSize = CQuadWord(0, 0);
    ActualSize2 = CQuadWord(0, 0);
    FileProgress = FALSE;
}

void CZIPUnpackProgress::SetTotal(const CQuadWord& total1, const CQuadWord& total2)
{
    if (total1 != CQuadWord(-1, -1))
    {
        TotalSize = max(CQuadWord(1, 0), total1);
        SizeIsDirty = FALSE;
    }
    if (total2 != CQuadWord(-1, -1))
    {
        TotalSize2 = max(CQuadWord(1, 0), total2);
        Size2IsDirty = FALSE;
    }
}

void CZIPUnpackProgress::SetRemapNames(const char* nameFrom, const char* nameTo)
{
    RemapNameFrom = nameFrom;
    RemapNameTo = nameTo;
}

void CZIPUnpackProgress::DoRemapNames(char* txt, int bufLen)
{
    if (RemapNameFrom != NULL && RemapNameTo != NULL)
    {
        char* s = strstr(txt, RemapNameFrom);
        if (s != NULL)
        {
            int len = (int)strlen(txt);
            int lenFrom = (int)strlen(RemapNameFrom);
            int lenTo = (int)strlen(RemapNameTo);
            if (len - lenFrom + lenTo < bufLen)
            {
                memmove(s + lenTo, s + lenFrom, len - ((s + lenFrom) - txt) + 1);
                memcpy(s, RemapNameTo, lenTo);
            }
            else
                TRACE_E("Remap: too long name.");
        }
    }
}

void CZIPUnpackProgress::SetTaskBarList3(CITaskBarList3* taskBarList3)
{
    TaskBarList3 = taskBarList3;
}

void CZIPUnpackProgress::DispatchMessages()
{
    // we are emptying the message queue
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

BOOL CZIPUnpackProgress::HasTwoProgress()
{
    return ResID == IDD_ZIPUNPACKPROG2;
}

int CZIPUnpackProgress::SetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint)
{
    // Does the user want to set size1?
    if (size1 != CQuadWord(-1, -1))
    {
        CQuadWord newSize = max(CQuadWord(0, 0), size1);
        if (newSize != ActualSize)
        {
            ActualSize = newSize;
            SizeIsDirty = TRUE;
        }
    }
    // Does the user want to set size2?
    if (size2 != CQuadWord(-1, -1))
    {
        CQuadWord newSize = max(CQuadWord(0, 0), size2);
        if (newSize != ActualSize2)
        {
            ActualSize2 = newSize;
            Size2IsDirty = TRUE;
        }
    }
    return AddSize(0, delayedPaint);
}

int CZIPUnpackProgress::AddSize(int size, BOOL delayedPaint)
{
    // time-critical function
    //  CALL_STACK_MESSAGE2("CZIPUnpackProgress::AddSize(%d)", size);

    ActualSize += CQuadWord(size, 0);
    if (size != 0)
        SizeIsDirty = TRUE;

    if (HasTwoProgress())
    {
        ActualSize2 += CQuadWord(size, 0);
        if (size != 0)
            Size2IsDirty = TRUE;
    }

    if (!delayedPaint)
    {
        // we need to render the texts immediately
        FlushDataToControls();
    }

    // Every 100ms we redraw the changed data (text + progress bars)
    DWORD ticks = GetTickCount();
    if (ticks - LastTickCount > 100)
    {
        LastTickCount = ticks;
        // if we haven't interrupted a moment ago, we'll do it now
        if (delayedPaint)
            FlushDataToControls();
    }

    DispatchMessages(); // we will focus on the user for a moment ...

    return !Cancel;
}

void CZIPUnpackProgress::NewLine(const char* txt, BOOL delayedPaint)
{
    // time-critical function
    //  CALL_STACK_MESSAGE2("CZIPUnpackProgress::NewLine(%s)", txt);
    if (txt == NULL)
        return;

    while (1) // print several lines to the console
    {
        while (*txt != 0 && (*txt == '\r' || *txt == '\n' || *txt == ' ' || *txt == '\t'))
            txt++;
        if (*txt == 0)
            break;

        // we will save to the cache, which we display on WM_TIMER

        // cycle through items in the cache index
        CacheIndex++;
        if (CacheIndex >= ZIP_UNPACK_NUMLINES)
            CacheIndex = 0;

        char* s = LinesCache[CacheIndex];
        char* sEnd = s + 300 - 1;
        while (*txt != 0 && *txt != '\r' && *txt != '\n') // reading one line + converting '/' to '\\'
        {
            if (*txt == '/')
            {
                if (s < sEnd)
                    *s++ = '\\';
                txt++;
            }
            else
            {
                if (s < sEnd)
                    *s++ = *txt++;
                else
                    txt++; // we will simply ignore the rest of the text (it won't fit in the dialogue anyway)
            }
        }
        *s = 0;
        DoRemapNames(LinesCache[CacheIndex], 300);

        // we have cleared the cache
        CacheIsDirty = TRUE;
    }

    if (!delayedPaint)
    {
        // we need to render the texts immediately
        FlushDataToControls();
    }

    // Every 100ms we redraw the changed data (text + progress bars)
    DWORD ticks = GetTickCount();
    if (ticks - LastTickCount > 100)
    {
        LastTickCount = ticks;
        // if we haven't interrupted a moment ago, we'll do it now
        if (delayedPaint)
            FlushDataToControls();
    }

    // Attention, do not call here
    DispatchMessages(); // we will focus on the user for a moment ...
}

void CZIPUnpackProgress::EnableCancel(BOOL enable)
{
    if (HWindow != NULL)
    {
        HWND cancel = GetDlgItem(HWindow, IDCANCEL);
        if (IsWindowEnabled(cancel) != enable)
        {
            EnableWindow(cancel, enable);
            if (enable)
                SetFocus(cancel);
            PostMessage(cancel, BM_SETSTYLE, enable ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);

            DispatchMessages(); // we will focus on the user for a moment ...
        }
    }
}

void CZIPUnpackProgress::FlushDataToControls()
{
    // texts
    if (CacheIsDirty)
    {
        int index = CacheIndex;
        int i;
        for (i = ZIP_UNPACK_NUMLINES - 1; i >= 0; i--)
        {
            if (Lines[i] != NULL)
                Lines[i]->SetText(LinesCache[index]);
            index--;
            if (index < 0)
                index = ZIP_UNPACK_NUMLINES - 1;
        }
        CacheIsDirty = FALSE;
    }

    if (TaskBarList3 != NULL)
    {
        if (HasTwoProgress())
        {
            if (Size2IsDirty)
                TaskBarList3->SetProgress2(ActualSize2, TotalSize2);
        }
        else
        {
            if (SizeIsDirty)
                TaskBarList3->SetProgress2(ActualSize, TotalSize);
        }
    }

    // size
    if (SizeIsDirty)
    {
        if (Summary != NULL)
        {
            Summary->SetProgress2(ActualSize, TotalSize);
        }
        SizeIsDirty = FALSE;
    }

    // size2
    if (Size2IsDirty)
    {
        if (Summary2 != NULL)
        {
            Summary2->SetProgress2(ActualSize2, TotalSize2);
        }
        Size2IsDirty = FALSE;
    }
}

INT_PTR
CZIPUnpackProgress::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (ResID == IDD_ZIPUNPACKPROG && FileProgress) // Replace the text "Total:" with "File:"
            SetDlgItemText(HWindow, IDT_PROGTITLE, LoadStr(IDS_UNPACKFILEPROGRESS));

        SetWindowText(HWindow, Title);
        // the whole object assumes that the allocation failed
        int i;
        for (i = 0; i < ZIP_UNPACK_NUMLINES; i++)
        {
            if ((Lines[i] = new CStaticText(HWindow, IDS_ZIPLINE1 + i, STF_PATH_ELLIPSIS | STF_CACHED_PAINT)) == NULL)
                TRACE_E(LOW_MEMORY);
        }
        if ((Summary = new CProgressBar(HWindow, IDC_ZIPSUMMARY)) == NULL)
            TRACE_E(LOW_MEMORY);
        if (HasTwoProgress())
        {
            if ((Summary2 = new CProgressBar(HWindow, IDC_ZIPSUMMARY2)) == NULL)
                TRACE_E(LOW_MEMORY);
        }
        break;
    }

    case WM_DESTROY:
    {
        if (TaskBarList3 != NULL)
            TaskBarList3->SetProgressState(TBPF_NOPROGRESS);
        break;
    }

    case WM_COMMAND:
    {
        // if the user clicked on the Cancel button and did not confirm it earlier, we will ask
        if (LOWORD(wParam) == IDCANCEL && !Cancel)
        {
            // Button Cancel must be enabled
            if (IsWindowEnabled(GetDlgItem(HWindow, IDCANCEL)))
            {
                // aby nedochazelo k prekreslovani pod messageboxem, prekreslime ted explicitne
                FlushDataToControls();

                // Ask the user if they want to interrupt the operation
                Cancel = (SalMessageBox(HWindow, LoadStr(IDS_CANCELOPERATION), LoadStr(IDS_QUESTION),
                                        MB_YESNO | MB_ICONQUESTION) == IDYES);
            }
        }
        // we will not let the command fail, otherwise the dialog will close on us
        return TRUE;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSalamanderGeneral
//

CSalamanderGeneral::CSalamanderGeneral()
{
    Plugin = NULL;
    LanguageModule = NULL;
    HelpFileName[0] = 0;
}

CSalamanderGeneral::~CSalamanderGeneral()
{
    if (LanguageModule != NULL)
    {
        TRACE_E("CSalamanderGeneral::~CSalamanderGeneral(): unexpected situation!");
        HANDLES(FreeLibrary(LanguageModule));
    }
}

void CSalamanderGeneral::Clear()
{
    if (LanguageModule != NULL)
        HANDLES(FreeLibrary(LanguageModule));
    LanguageModule = NULL;
    HelpFileName[0] = 0;
}

int CSalamanderGeneral::ShowMessageBox(const char* text, const char* title, int type)
{
    if (MainThreadID != GetCurrentThreadId()) // Petr: Let's just yell, I don't have the strength to look for where it's ringing stupidly everywhere
        TRACE_E("You can call CSalamanderGeneral::ShowMessageBox() only from main thread!");
    HWND parent = GetMsgBoxParent();
    switch (type)
    {
    case MSGBOX_INFO:
    {
        return SalMessageBox(parent, text, title, MB_OK | MB_ICONINFORMATION);
    }

    case MSGBOX_ERROR:
    {
        return SalMessageBox(parent, text, title, MB_OK | MB_ICONEXCLAMATION);
    }

    case MSGBOX_EX_ERROR:
    {
        return SalMessageBox(parent, text, title, MB_OKCANCEL | MB_ICONEXCLAMATION);
    }

    case MSGBOX_QUESTION:
    {
        return SalMessageBox(parent, text, title, MB_YESNO | MB_ICONQUESTION);
    }

    case MSGBOX_EX_QUESTION:
    {
        return SalMessageBox(parent, text, title, MB_YESNOCANCEL | MB_ICONQUESTION);
    }

    case MSGBOX_WARNING:
    {
        return SalMessageBox(parent, text, title, MB_OK | MB_ICONWARNING);
    }

    case MSGBOX_EX_WARNING:
    {
        return SalMessageBox(parent, text, title, MB_YESNOCANCEL | MB_ICONWARNING);
    }

    default:
    {
        TRACE_E("Unknown type of box in CSalamanderGeneral::ShowMessageBox().");
        return 0;
    }
    }
}

int CSalamanderGeneral::SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
    return ::SalMessageBox(hParent, lpText, lpCaption, uType);
}

int CSalamanderGeneral::SalMessageBoxEx(const MSGBOXEX_PARAMS* params)
{
    return ::SalMessageBoxEx(params);
}

HWND CSalamanderGeneral::GetMsgBoxParent()
{
    if (MainThreadID != GetCurrentThreadId()) // Petr: Let's just yell, I don't have the strength to look for where it's ringing stupidly everywhere
        TRACE_E("You can call CSalamanderGeneral::GetMsgBoxParent() only from main thread!");
    // if the following code changes, it is necessary to also change it in EnterPlugin - for the validation to work
    return PluginProgressDialog != NULL ? PluginProgressDialog : PluginMsgBoxParent;
}

int DialogError(HWND parent, DWORD flags, const char* fileName,
                const char* error, const char* title)
{
    HWND mainWnd = GetWndToFlash(parent);
    DWORD resID;
    BOOL noSkip;
    switch (flags & BUTTONS_MASK)
    {
    case BUTTONS_OK:
    {
        resID = IDD_ERROR3;
        noSkip = FALSE; // It doesn't apply, somewhere about redID==0
        break;
    }

    case BUTTONS_RETRYCANCEL:
    {
        resID = 0;
        noSkip = TRUE;
        break;
    }

    case BUTTONS_SKIPCANCEL:
    {
        resID = IDD_ERROR2;
        noSkip = FALSE; // It doesn't apply, somewhere about redID==0
        break;
    }

    case BUTTONS_RETRYSKIPCANCEL:
    {
        resID = 0;
        noSkip = FALSE;
        break;
    }

    default:
    {
        TRACE_E("CSalamanderGeneral::DialogError: unknow flags=0x" << std::hex << flags << std::dec);
        return DIALOG_FAIL;
    }
    }
    int res = (int)CFileErrorDlg(parent, title != NULL ? title : ::LoadStr(IDS_ERRORTITLE),
                                 fileName, error, noSkip, resID)
                  .Execute();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    switch (res)
    {
    case IDOK:
        return DIALOG_OK;
    case IDRETRY:
        return DIALOG_RETRY;
    case IDB_SKIP:
        return DIALOG_SKIP;
    case IDB_SKIPALL:
        return DIALOG_SKIPALL;
    case IDCANCEL:
        return DIALOG_CANCEL;
    default:
        return DIALOG_FAIL;
    }
}

int CSalamanderGeneral::DialogError(HWND parent, DWORD flags, const char* fileName,
                                    const char* error, const char* title)
{
    if (fileName == NULL || error == NULL)
    {
        TRACE_E("Invalid parametr (fileName == NULL || error == NULL) in CSalamanderGeneral::DialogError!");
        if (fileName == NULL)
            fileName = "";
        if (error == NULL)
            error = "";
    }
    return ::DialogError(parent, flags, fileName, error, title);
}

int DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                    const char* fileName2, const char* fileData2)
{
    HWND mainWnd = GetWndToFlash(parent);
    BOOL yesnocancel;
    switch (flags & BUTTONS_MASK)
    {
    case BUTTONS_YESALLSKIPCANCEL:
    {
        yesnocancel = FALSE;
        break;
    }

    case BUTTONS_YESNOCANCEL:
    {
        yesnocancel = TRUE;
        break;
    }

    default:
    {
        TRACE_E("CSalamanderGeneral::DialogOverwrite: unknow flags=0x" << std::hex << flags << std::dec);
        return DIALOG_FAIL;
    }
    }

    int res = (int)COverwriteDlg(parent, fileName1, fileData1, fileName2, fileData2, yesnocancel).Execute();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    switch (res)
    {
    case IDYES:
        return DIALOG_YES;
    case IDNO:
        return DIALOG_NO;
    case IDB_ALL:
        return DIALOG_ALL;
    case IDB_SKIP:
        return DIALOG_SKIP;
    case IDB_SKIPALL:
        return DIALOG_SKIPALL;
    case IDCANCEL:
        return DIALOG_CANCEL;
    default:
        return DIALOG_FAIL;
    }
}

int CSalamanderGeneral::DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                                        const char* fileName2, const char* fileData2)
{
    if (fileName1 == NULL || fileData1 == NULL || fileName2 == NULL || fileData2 == NULL)
    {
        TRACE_E("Invalid parametr (fileName1 == NULL || fileData1 == NULL || fileName2 == NULL || fileData2 == NULL) in CSalamanderGeneral::DialogOverwrite!");
        if (fileName1 == NULL)
            fileName1 = "";
        if (fileData1 == NULL)
            fileData1 = "";
        if (fileName2 == NULL)
            fileName2 = "";
        if (fileData2 == NULL)
            fileData2 = "";
    }
    return ::DialogOverwrite(parent, flags, fileName1, fileData1, fileName2, fileData2);
}

int DialogQuestion(HWND parent, DWORD flags, const char* fileName,
                   const char* question, const char* title)
{
    HWND mainWnd = GetWndToFlash(parent);
    BOOL yesnocancel, yesallcancel;
    switch (flags & BUTTONS_MASK)
    {
    case BUTTONS_YESALLSKIPCANCEL:
    {
        yesnocancel = FALSE;
        yesallcancel = FALSE;
        break;
    }

    case BUTTONS_YESNOCANCEL:
    {
        yesnocancel = TRUE;
        yesallcancel = FALSE;
        break;
    }

    case BUTTONS_YESALLCANCEL:
    {
        yesnocancel = TRUE;
        yesallcancel = TRUE;
        break;
    }

    default:
    {
        TRACE_E("CSalamanderGeneral::DialogQuestion: unknow flags=0x" << std::hex << flags << std::dec);
        return DIALOG_FAIL;
    }
    }
    int res = (int)CHiddenOrSystemDlg(parent, title != NULL ? title : ::LoadStr(IDS_QUESTION), fileName,
                                      question, yesnocancel, yesallcancel)
                  .Execute();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    switch (res)
    {
    case IDYES:
        return DIALOG_YES;
    case IDNO:
        return DIALOG_NO;
    case IDB_ALL:
        return DIALOG_ALL;
    case IDB_SKIP:
        return DIALOG_SKIP;
    case IDB_SKIPALL:
        return DIALOG_SKIPALL;
    case IDCANCEL:
        return DIALOG_CANCEL;
    default:
        return DIALOG_FAIL;
    }
}

int CSalamanderGeneral::DialogQuestion(HWND parent, DWORD flags, const char* fileName,
                                       const char* question, const char* title)
{
    if (fileName == NULL || question == NULL)
    {
        TRACE_E("Invalid parametr (fileName == NULL || question == NULL) in CSalamanderGeneral::DialogQuestion!");
        if (fileName == NULL)
            fileName = "";
        if (question == NULL)
            question = "";
    }
    return ::DialogQuestion(parent, flags, fileName, question, title);
}

HWND CSalamanderGeneral::GetMainWindowHWND()
{
    return MainWindow != NULL ? MainWindow->HWindow : NULL;
}

void RestoreFocusInSourcePanel()
{
    if (MainWindow != NULL)
    {
        CFilesWindow* p1 = MainWindow->GetActivePanel();
        if (p1 != NULL)
        {
            if (!MainWindow->EditMode)
                MainWindow->FocusPanel(p1);
            else
            {
                if (MainWindow->EditWindow != NULL && MainWindow->EditWindow->HWindow != NULL)
                    SetFocus(MainWindow->EditWindow->HWindow);
            }
        }
    }
}

void CSalamanderGeneral::RestoreFocusInSourcePanel()
{
    ::RestoreFocusInSourcePanel();
}

BOOL CSalamanderGeneral::CheckAndCreateDirectory(const char* dir, HWND parent, BOOL quiet,
                                                 char* errBuf, int errBufSize, char* firstCreatedDir,
                                                 BOOL manualCrDir)
{
    return ::CheckAndCreateDirectory(dir, parent, quiet, errBuf, errBufSize, firstCreatedDir, FALSE, manualCrDir);
}

BOOL CSalamanderGeneral::TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize,
                                       const char* messageTitle)
{
    return ::TestFreeSpace(parent, path, totalSize, messageTitle);
}

void CSalamanderGeneral::GetDiskFreeSpace(CQuadWord* retValue, const char* path, CQuadWord* total)
{
    if (retValue == NULL)
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::GetDiskFreeSpace(): retValue is NULL!");
        return;
    }
    *retValue = MyGetDiskFreeSpace(path, total);
}

BOOL CSalamanderGeneral::SalGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                                             LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                                             LPDWORD lpTotalNumberOfClusters)
{
    return MyGetDiskFreeSpace(path, lpSectorsPerCluster, lpBytesPerSector,
                              lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}

BOOL CSalamanderGeneral::SalGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, LPTSTR lpVolumeNameBuffer,
                                                 DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                                                 LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                                                 LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
    return MyGetVolumeInformation(path, rootOrCurReparsePoint, NULL, NULL, lpVolumeNameBuffer, nVolumeNameSize,
                                  lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
                                  lpFileSystemNameBuffer, nFileSystemNameSize);
}

UINT CSalamanderGeneral::SalGetDriveType(const char* path)
{
    return MyGetDriveType(path);
}

void CSalamanderGeneral::RemoveTemporaryDir(const char* dir)
{
    ::RemoveTemporaryDir(dir);
}

void CSalamanderGeneral::PrepareMask(char* mask, const char* src)
{
    ::PrepareMask(mask, src);
}

BOOL CSalamanderGeneral::AgreeMask(const char* filename, const char* mask, BOOL hasExtension)
{
    return ::AgreeMask(filename, mask, hasExtension, FALSE);
}

char* CSalamanderGeneral::MaskName(char* buffer, int bufSize, const char* name, const char* mask)
{
    return ::MaskName(buffer, bufSize, name, mask);
}

void CSalamanderGeneral::PrepareExtMask(char* mask, const char* src)
{
    ::PrepareMask(mask, src);
}

BOOL CSalamanderGeneral::AgreeExtMask(const char* filename, const char* mask, BOOL hasExtension)
{
    return ::AgreeMask(filename, mask, hasExtension, TRUE);
}

void* CSalamanderGeneral::Alloc(int size)
{
    return malloc(size);
}

void* CSalamanderGeneral::Realloc(void* ptr, int size)
{
    return realloc(ptr, size);
}

void CSalamanderGeneral::Free(void* ptr)
{
    free(ptr);
}

char* CSalamanderGeneral::DupStr(const char* str)
{
    return ::DupStr(str);
}

void CSalamanderGeneral::GetLowerAndUpperCase(unsigned char** lowerCase, unsigned char** upperCase)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetLowerAndUpperCase(,)");
    if (lowerCase != NULL)
        *lowerCase = LowerCase;
    if (upperCase != NULL)
        *upperCase = UpperCase;
}

void CSalamanderGeneral::ToLowerCase(char* str)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::ToLowerCase()");
    char* toLow = str;
    while (*toLow != 0)
    {
        *toLow = LowerCase[*toLow];
        toLow++;
    }
}

void CSalamanderGeneral::ToUpperCase(char* str)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::ToUpperCase()");
    char* toUpp = str;
    while (*toUpp != 0)
    {
        *toUpp = UpperCase[*toUpp];
        toUpp++;
    }
}

int CSalamanderGeneral::StrCmpEx(const char* s1, int l1, const char* s2, int l2)
{
    return ::StrCmpEx(s1, l1, s2, l2);
}

int CSalamanderGeneral::StrICpy(char* dest, const char* src)
{
    return ::StrICpy(dest, src);
}

int CSalamanderGeneral::StrICmp(const char* s1, const char* s2)
{
    return ::StrICmp(s1, s2);
}

int CSalamanderGeneral::StrICmpEx(const char* s1, int l1, const char* s2, int l2)
{
    return ::StrICmpEx(s1, l1, s2, l2);
}

int CSalamanderGeneral::StrNICmp(const char* s1, const char* s2, int n)
{
    return ::StrNICmp(s1, s2, n);
}

int CSalamanderGeneral::MemICmp(const void* buf1, const void* buf2, int n)
{
    return ::MemICmp(buf1, buf2, n);
}

int CSalamanderGeneral::RegSetStrICmp(const char* s1, const char* s2)
{
    return ::RegSetStrICmp(s1, s2);
}

int CSalamanderGeneral::RegSetStrICmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual)
{
    return ::RegSetStrICmpEx(s1, l1, s2, l2, numericalyEqual);
}

int CSalamanderGeneral::RegSetStrCmp(const char* s1, const char* s2)
{
    return ::RegSetStrCmp(s1, s2);
}

int CSalamanderGeneral::RegSetStrCmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual)
{
    return ::RegSetStrCmpEx(s1, l1, s2, l2, numericalyEqual);
}

CFilesWindow*
CSalamanderGeneral::GetPanel(int panel)
{
    return MainWindow->GetPanel(panel);
}

BOOL CSalamanderGeneral::GetPanelPath(int panel, char* buffer, int bufferSize, int* type,
                                      char** archiveOrFS, BOOL convertFSPathToExternal)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::GetPanelPath(%d, , %d, ,)", panel, bufferSize);
    if (type != NULL)
        *type = 0; // unknown
    if (archiveOrFS != NULL)
        *archiveOrFS = NULL;
    if (bufferSize > 0)
        buffer[0] = 0;
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelPath() only from main thread!");
        if (type != NULL)
            *type = 0;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        char buf[2 * MAX_PATH];
        int offset = -1; // Offset to buffer for calculating archiveOrFS (-1 means NULL)
        if (p->Is(ptZIPArchive))
        {
            if (type != NULL)
                *type = PATH_TYPE_ARCHIVE;
            offset = (int)strlen(p->GetZIPArchive());
            memcpy(buf, p->GetZIPArchive(), offset + 1);
            if (p->GetZIPPath()[0] != 0)
            {
                if (p->GetZIPPath()[0] != '\\')
                    strcpy(buf + offset, "\\");
                strcat(buf + offset, p->GetZIPPath());
            }
        }
        else
        {
            if (p->Is(ptPluginFS))
            {
                if (type != NULL)
                    *type = PATH_TYPE_FS;
                offset = (int)strlen(p->GetPluginFS()->GetPluginFSName());
                memcpy(buf, p->GetPluginFS()->GetPluginFSName(), offset);
                buf[offset] = ':';
                if (!p->GetPluginFS()->NotEmpty() || !p->GetPluginFS()->GetCurrentPath(buf + offset + 1))
                {
                    return FALSE; // error
                }
                if (convertFSPathToExternal)
                {
                    p->GetPluginFS()->GetPluginInterfaceForFS()->ConvertPathToExternal(p->GetPluginFS()->GetPluginFSName(),
                                                                                       p->GetPluginFS()->GetPluginFSNameIndex(),
                                                                                       buf + offset + 1);
                }
            }
            else
            {
                if (p->Is(ptDisk))
                {
                    if (type != NULL)
                        *type = PATH_TYPE_WINDOWS;
                    strcpy(buf, p->GetPath());
                }
                else
                {
                    TRACE_E("Unexpected situation in CSalamanderGeneral::GetPanelPath()");
                    return FALSE;
                }
            }
        }

        int l = (int)strlen(buf) + 1;
        if (l > bufferSize)
            return bufferSize == 0; // if the user does not want to return the path, we do not consider it an error
        memcpy(buffer, buf, l);

        if (archiveOrFS != NULL && offset != -1)
            *archiveOrFS = buffer + offset;

        return TRUE;
    }
    return FALSE;
}

BOOL CSalamanderGeneral::GetLastWindowsPanelPath(int panel, char* buffer, int bufferSize)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::GetLastWindowsPanelPath(%d, , %d)", panel, bufferSize);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetLastWindowsPanelPath() only from main thread!");
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && buffer != NULL)
    {
        int l = (int)strlen(p->GetPath()) + 1;
        if (l > bufferSize)
            return FALSE;
        memcpy(buffer, p->GetPath(), l);
        return TRUE;
    }
    return FALSE;
}

CPluginDataInterfaceAbstract*
CSalamanderGeneral::GetPanelPluginData(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelPluginData(%d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelPluginData() only from main thread!");
        return NULL;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        CPluginDataInterfaceAbstract* iface = p->PluginData.GetInterface();
        if (iface != NULL && p->PluginData.GetPluginInterface() != Plugin)
            iface = NULL; // object is not of this plugin -> will receive nothing
        return iface;
    }
    return NULL;
}

CPluginFSInterfaceAbstract*
CSalamanderGeneral::GetPanelPluginFS(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelPluginFS(%d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelPluginFS() only from main thread!");
        return NULL;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && p->Is(ptPluginFS))
    {
        CPluginFSInterfaceAbstract* iface = p->GetPluginFS()->GetInterface();
        if (iface != NULL && p->GetPluginFS()->GetPluginInterface() != Plugin)
            iface = NULL; // object is not of this plugin -> will receive nothing
        return iface;
    }
    return NULL;
}

const CFileData*
CSalamanderGeneral::GetPanelFocusedItem(int panel, BOOL* isDir)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelFocusedItem(%d,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelFocusedItem() only from main thread!");
        return NULL;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        int caret = p->GetCaretIndex();
        if (caret >= 0 && caret < p->Files->Count + p->Dirs->Count)
        {
            if (isDir != NULL)
                *isDir = caret < p->Dirs->Count;
            return (caret < p->Dirs->Count) ? &p->Dirs->At(caret) : &p->Files->At(caret - p->Dirs->Count);
        }
    }
    return NULL;
}

const CFileData*
CSalamanderGeneral::GetPanelItem(int panel, int* index, BOOL* isDir)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelItem(%d,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelItem() only from main thread!");
        return NULL;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && index != NULL)
    {
        int i = *index;
        if (i < 0)
            return NULL;                          // enumeration has ended
        if (i < p->Files->Count + p->Dirs->Count) // enum next items
        {
            *index = i + 1; // next will be another
            if (isDir != NULL)
                *isDir = i < p->Dirs->Count;
            return (i < p->Dirs->Count) ? &p->Dirs->At(i) : &p->Files->At(i - p->Dirs->Count);
        }
        else
        {
            *index = -1; // end of enumeration
            return NULL;
        }
    }
    return NULL;
}

BOOL CSalamanderGeneral::GetPanelSelection(int panel, int* selectedFiles, int* selectedDirs)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelSelection(%d, ,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelSelection() only from main thread!");
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        int count = p->GetSelCount();
        int selDirs = 0;
        if (count > 0)
        {
            CFilesArray* dirs = p->Dirs;
            // we will count how many directories are marked (the rest of the marked items are files)
            int i;
            for (i = 0; i < dirs->Count; i++) // ".." cannot be marked, the test would be pointless
            {
                if (dirs->At(i).Selected)
                    selDirs++;
            }
        }
        else
            count = 0;

        if (selectedDirs != NULL)
            *selectedDirs = selDirs;
        if (selectedFiles != NULL)
            *selectedFiles = count - selDirs;

        int i = p->GetCaretIndex();
        return p->Dirs->Count + p->Files->Count > 0 && // panel is not empty
               (i != 0 || count > 0 || p->Dirs->Count == 0 ||
                strcmp(p->Dirs->At(0).Name, "..") != 0); // focus is not on the up-dir or at least something is marked
    }
    return FALSE;
}

const CFileData*
CSalamanderGeneral::GetPanelSelectedItem(int panel, int* index, BOOL* isDir)
{
    SLOW_CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelSelectedItem(%d, ,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelSelectedItem() only from main thread!");
        return NULL;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && index != NULL)
    {
        int i = *index;
        if (i < 0)
            return NULL;                             // enumeration has ended
        while (i < p->Files->Count + p->Dirs->Count) // searching for the next marked item
        {
            CFileData* data = (i < p->Dirs->Count) ? &p->Dirs->At(i) : &p->Files->At(i - p->Dirs->Count);
            if (data->Selected) // marked item?
            {
                *index = i + 1; // We will start searching from the next item next time
                if (isDir != NULL)
                    *isDir = i < p->Dirs->Count;
                return data; // return the found marked item
            }
            i++;
        }
        *index = -1; // end of enumeration, no item is marked anymore
    }
    return NULL;
}

void CSalamanderGeneral::SelectPanelItem(int panel, const CFileData* file, BOOL select)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::SelectPanelItem(%d, , %d)", panel, select);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SelectPanelItem() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        int index = -1; // index 'file' in the panel
        if (p->Dirs->Count > 0)
        {
            CFileData* first = &p->Dirs->At(0);
            CFileData* last = &p->Dirs->At(p->Dirs->Count - 1);
            if (first <= file && file <= last)
                index = (int)(file - first); // it's about the directory
        }
        if (index == -1 && p->Files->Count > 0)
        {
            CFileData* first = &p->Files->At(0);
            CFileData* last = &p->Files->At(p->Files->Count - 1);
            if (first <= file && file <= last)
                index = p->Dirs->Count + (int)(file - first); // it's about the directory
        }
        if (index != -1)
            p->SetSel(select, index, FALSE); // change label
        else
            TRACE_E("Invalid parameter 'file' in CSalamanderGeneral::SelectPanelItem().");
    }
}

void CSalamanderGeneral::RepaintChangedItems(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::RepaintChangedItems(%d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::RepaintChangedItems() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        p->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
        PostMessage(p->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
    }
}

void CSalamanderGeneral::SelectAllPanelItems(int panel, BOOL select, BOOL repaint)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::SelectAllPanelItems(%d, %d, %d)", panel, select, repaint);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SelectAllPanelItems() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        p->SetSel(select, -1, repaint); // change label
        if (repaint)
            PostMessage(p->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
    }
}

void CSalamanderGeneral::SetPanelFocusedItem(int panel, const CFileData* file, BOOL partVis)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::SetPanelFocusedItem(%d, , %d)", panel, partVis);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SetPanelFocusedItem() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        int index = -1; // index 'file' in the panel
        if (p->Dirs->Count > 0)
        {
            CFileData* first = &p->Dirs->At(0);
            CFileData* last = &p->Dirs->At(p->Dirs->Count - 1);
            if (first <= file && file <= last)
                index = (int)(file - first); // it's about the directory
        }
        if (index == -1 && p->Files->Count > 0)
        {
            CFileData* first = &p->Files->At(0);
            CFileData* last = &p->Files->At(p->Files->Count - 1);
            if (first <= file && file <= last)
                index = p->Dirs->Count + (int)(file - first); // it's about the directory
        }
        if (index != -1)
            p->SetCaretIndex(index, partVis); // change of focus
        else
            TRACE_E("Invalid parameter 'file' in CSalamanderGeneral::SetPanelFocusedItem().");
    }
}

BOOL CSalamanderGeneral::GetFilterFromPanel(int panel, char* masks, int masksBufSize)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::GetFilterFromPanel(%d, , %d)", panel, masksBufSize);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetFilterFromPanel() only from main thread!");
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    BOOL ret = FALSE;
    if (p != NULL && p->FilterEnabled)
    {
        int len = (int)strlen(p->Filter.GetMasksString());
        if (len < masksBufSize)
        {
            memcpy(masks, p->Filter.GetMasksString(), len + 1);
            ret = TRUE;
        }
    }
    return ret;
}

// returns the position of the source panel (is it on the left or on the right?), returns PANEL_LEFT or PANEL_RIGHT
int CSalamanderGeneral::GetSourcePanel()
{
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetSourcePanel() only from main thread!");
        return PANEL_LEFT;
    }
    if (MainWindow->GetActivePanel() == MainWindow->LeftPanel)
        return PANEL_LEFT;
    else
        return PANEL_RIGHT;
}

// activates the second panel (like the TAB key); panels marked via PANEL_SOURCE and PANEL_TARGET
// are naturally swapped with it
void CSalamanderGeneral::ChangePanel()
{
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanel() only from main thread!");
        return;
    }
    MainWindow->ChangePanel();
}

void CSalamanderGeneral::SkipOneActivateRefresh()
{
    ::SkipOneActivateRefresh = TRUE;
    PostMessage(MainWindow->HWindow, WM_USER_SKIPONEREFRESH, 0, 0);
}

BOOL CSalamanderGeneral::SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file, DWORD* err)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalGetTempFileName()");
    BOOL ret = ::SalGetTempFileName(path, prefix, tmpName, file);
    if (err != NULL)
        *err = GetLastError();
    return ret;
}

char* CSalamanderGeneral::NumberToStr(char* buffer, const CQuadWord& number)
{
    return ::NumberToStr(buffer, number);
}

char* CSalamanderGeneral::PrintDiskSize(char* buf, const CQuadWord& size, int mode)
{
    return ::PrintDiskSize(buf, size, mode);
}

char* CSalamanderGeneral::PrintTimeLeft(char* buf, const CQuadWord& secs)
{
    return ::PrintTimeLeft(buf, secs);
}

BOOL CSalamanderGeneral::HasTheSameRootPath(const char* path1, const char* path2)
{
    return ::HasTheSameRootPath(path1, path2);
}

int CSalamanderGeneral::CommonPrefixLength(const char* path1, const char* path2)
{
    return ::CommonPrefixLength(path1, path2);
}

BOOL CSalamanderGeneral::PathIsPrefix(const char* prefix, const char* path)
{
    return ::SalPathIsPrefix(prefix, path);
}

BOOL CSalamanderGeneral::IsTheSamePath(const char* path1, const char* path2)
{
    return ::IsTheSamePath(path1, path2);
}

int CSalamanderGeneral::GetRootPath(char* root, const char* path)
{
    return ::GetRootPath(root, path);
}

BOOL CSalamanderGeneral::CutDirectory(char* path, char** cutDir)
{
    return ::CutDirectory(path, cutDir);
}

BOOL CSalamanderGeneral::SalPathAppend(char* path, const char* name, int pathSize)
{
    return ::SalPathAppend(path, name, pathSize);
}

BOOL CSalamanderGeneral::SalPathAddBackslash(char* path, int pathSize)
{
    return ::SalPathAddBackslash(path, pathSize);
}

void CSalamanderGeneral::SalPathRemoveBackslash(char* path)
{
    ::SalPathRemoveBackslash(path);
}

void CSalamanderGeneral::SalPathStripPath(char* path)
{
    ::SalPathStripPath(path);
}

void CSalamanderGeneral::SalPathRemoveExtension(char* path)
{
    ::SalPathRemoveExtension(path);
}

BOOL CSalamanderGeneral::SalPathAddExtension(char* path, const char* extension, int pathSize)
{
    return ::SalPathAddExtension(path, extension, pathSize);
}

BOOL CSalamanderGeneral::SalPathRenameExtension(char* path, const char* extension, int pathSize)
{
    return ::SalPathRenameExtension(path, extension, pathSize);
}

const char*
CSalamanderGeneral::SalPathFindFileName(const char* path)
{
    return ::SalPathFindFileName(path);
}

BOOL CSalamanderGeneral::SalGetFullName(char* name, int* errTextID, const char* curDir,
                                        char* nextFocus, int nameBufSize)
{
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SalGetFullName() only from main thread!");
        if (errTextID != NULL)
            *errTextID = GFN_PATHISINVALID;
        return FALSE;
    }
    BOOL ret = ::SalGetFullName(name, errTextID, curDir, nextFocus, NULL, nameBufSize);
    if (errTextID != NULL)
    {
        switch (*errTextID)
        {
        case IDS_SERVERNAMEMISSING:
            *errTextID = GFN_SERVERNAMEMISSING;
            break;
        case IDS_SHARENAMEMISSING:
            *errTextID = GFN_SHARENAMEMISSING;
            break;
        case IDS_TOOLONGPATH:
            *errTextID = GFN_TOOLONGPATH;
            break;
        case IDS_INVALIDDRIVE:
            *errTextID = GFN_INVALIDDRIVE;
            break;
        case IDS_INCOMLETEFILENAME:
            *errTextID = GFN_INCOMLETEFILENAME;
            break;
        case IDS_EMPTYNAMENOTALLOWED:
            *errTextID = GFN_EMPTYNAMENOTALLOWED;
            break;
        case IDS_PATHISINVALID:
            *errTextID = GFN_PATHISINVALID;
            break;
        }
    }

    return ret;
}

void CSalamanderGeneral::SalUpdateDefaultDir(BOOL activePrefered)
{
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SalUpdateDefaultDir() only from main thread!");
        return;
    }
    if (MainWindow != NULL)
        MainWindow->UpdateDefaultDir(activePrefered);
}

char* CSalamanderGeneral::GetGFNErrorText(int GFN, char* buf, int bufSize)
{
    char* s = NULL;
    switch (GFN)
    {
    case GFN_SERVERNAMEMISSING:
        s = ::LoadStr(IDS_SERVERNAMEMISSING);
        break;
    case GFN_SHARENAMEMISSING:
        s = ::LoadStr(IDS_SHARENAMEMISSING);
        break;
    case GFN_TOOLONGPATH:
        s = ::LoadStr(IDS_TOOLONGPATH);
        break;
    case GFN_INVALIDDRIVE:
        s = ::LoadStr(IDS_INVALIDDRIVE);
        break;
    case GFN_INCOMLETEFILENAME:
        s = ::LoadStr(IDS_INCOMLETEFILENAME);
        break;
    case GFN_EMPTYNAMENOTALLOWED:
        s = ::LoadStr(IDS_EMPTYNAMENOTALLOWED);
        break;
    case GFN_PATHISINVALID:
        s = ::LoadStr(IDS_PATHISINVALID);
        break;
    }
    if (s != NULL)
        lstrcpyn(buf, s, bufSize);
    else
        buf[0] = 0;
    return buf;
}

char* CSalamanderGeneral::GetErrorText(int err, char* buf, int bufSize)
{
    if (buf == NULL || bufSize == 0)
        return ::GetErrorText(err);

    int l = 0;
    if (bufSize > 20)
        l = sprintf(buf, "(%d) ", err);
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      buf + l,
                      bufSize - l,
                      NULL) == 0 ||
        bufSize > l && *(buf + l) == 0)
    {
        char txt[100];
        sprintf(txt, "System error %d, text description is not available.", err);
        lstrcpyn(buf, txt, bufSize);
    }
    return buf;
}

char* CSalamanderGeneral::LoadStr(HINSTANCE module, int resID)
{
    if (module == NULL)
    {
        TRACE_E("CSalamanderGeneral::LoadStr(): module == NULL");
        static char buffEmpty[] = "ERROR LOADING STRING";
        return buffEmpty;
    }
    return ::LoadStr(resID, module);
}

WCHAR*
CSalamanderGeneral::LoadStrW(HINSTANCE module, int resID)
{
    if (module == NULL)
    {
        TRACE_E("CSalamanderGeneral::LoadStrW(): module == NULL");
        static wchar_t buffEmpty[] = L"ERROR LOADING WIDE STRING";
        return buffEmpty;
    }
    return ::LoadStrW(resID, module);
}

COLORREF
CSalamanderGeneral::GetCurrentColor(int color)
{
    int index;
    SALCOLOR* arr = CurrentColors;
    switch (color)
    {
    // CurrentColors
    case SALCOL_FOCUS_ACTIVE_NORMAL:
        index = FOCUS_ACTIVE_NORMAL;
        break;
    case SALCOL_FOCUS_ACTIVE_SELECTED:
        index = FOCUS_ACTIVE_SELECTED;
        break;
    case SALCOL_FOCUS_FG_INACTIVE_NORMAL:
        index = FOCUS_FG_INACTIVE_NORMAL;
        break;
    case SALCOL_FOCUS_FG_INACTIVE_SELECTED:
        index = FOCUS_FG_INACTIVE_SELECTED;
        break;
    case SALCOL_FOCUS_BK_INACTIVE_NORMAL:
        index = FOCUS_BK_INACTIVE_NORMAL;
        break;
    case SALCOL_FOCUS_BK_INACTIVE_SELECTED:
        index = FOCUS_BK_INACTIVE_SELECTED;
        break;
    case SALCOL_ITEM_FG_NORMAL:
        index = ITEM_FG_NORMAL;
        break;
    case SALCOL_ITEM_FG_SELECTED:
        index = ITEM_FG_SELECTED;
        break;
    case SALCOL_ITEM_FG_FOCUSED:
        index = ITEM_FG_FOCUSED;
        break;
    case SALCOL_ITEM_FG_FOCSEL:
        index = ITEM_FG_FOCSEL;
        break;
    case SALCOL_ITEM_FG_HIGHLIGHT:
        index = ITEM_FG_HIGHLIGHT;
        break;
    case SALCOL_ITEM_BK_NORMAL:
        index = ITEM_BK_NORMAL;
        break;
    case SALCOL_ITEM_BK_SELECTED:
        index = ITEM_BK_SELECTED;
        break;
    case SALCOL_ITEM_BK_FOCUSED:
        index = ITEM_BK_FOCUSED;
        break;
    case SALCOL_ITEM_BK_FOCSEL:
        index = ITEM_BK_FOCSEL;
        break;
    case SALCOL_ITEM_BK_HIGHLIGHT:
        index = ITEM_BK_HIGHLIGHT;
        break;
    case SALCOL_ICON_BLEND_SELECTED:
        index = ICON_BLEND_SELECTED;
        break;
    case SALCOL_ICON_BLEND_FOCUSED:
        index = ICON_BLEND_FOCUSED;
        break;
    case SALCOL_ICON_BLEND_FOCSEL:
        index = ICON_BLEND_FOCSEL;
        break;
    case SALCOL_PROGRESS_FG_NORMAL:
        index = PROGRESS_FG_NORMAL;
        break;
    case SALCOL_PROGRESS_FG_SELECTED:
        index = PROGRESS_FG_SELECTED;
        break;
    case SALCOL_PROGRESS_BK_NORMAL:
        index = PROGRESS_BK_NORMAL;
        break;
    case SALCOL_PROGRESS_BK_SELECTED:
        index = PROGRESS_BK_SELECTED;
        break;
    case SALCOL_HOT_PANEL:
        index = HOT_PANEL;
        break;
    case SALCOL_HOT_ACTIVE:
        index = HOT_ACTIVE;
        break;
    case SALCOL_HOT_INACTIVE:
        index = HOT_INACTIVE;
        break;
    case SALCOL_ACTIVE_CAPTION_FG:
        index = ACTIVE_CAPTION_FG;
        break;
    case SALCOL_ACTIVE_CAPTION_BK:
        index = ACTIVE_CAPTION_BK;
        break;
    case SALCOL_INACTIVE_CAPTION_FG:
        index = INACTIVE_CAPTION_FG;
        break;
    case SALCOL_INACTIVE_CAPTION_BK:
        index = INACTIVE_CAPTION_BK;
        break;
    case SALCOL_THUMBNAIL_NORMAL:
        index = THUMBNAIL_FRAME_NORMAL;
        break;
    case SALCOL_THUMBNAIL_SELECTED:
        index = THUMBNAIL_FRAME_FOCUSED;
        break;
    case SALCOL_THUMBNAIL_FOCUSED:
        index = THUMBNAIL_FRAME_SELECTED;
        break;
    case SALCOL_THUMBNAIL_FOCSEL:
        index = THUMBNAIL_FRAME_FOCSEL;
        break;
    // ViewerColors
    case SALCOL_VIEWER_FG_NORMAL:
        index = VIEWER_FG_NORMAL;
        arr = ViewerColors;
        break;
    case SALCOL_VIEWER_BK_NORMAL:
        index = VIEWER_BK_NORMAL;
        arr = ViewerColors;
        break;
    case SALCOL_VIEWER_FG_SELECTED:
        index = VIEWER_FG_SELECTED;
        arr = ViewerColors;
        break;
    case SALCOL_VIEWER_BK_SELECTED:
        index = VIEWER_BK_SELECTED;
        arr = ViewerColors;
        break;

    default:
    {
        TRACE_E("Invalid color constant!");
        return COLORREF(0);
    }
    }
    return GetCOLORREF(arr[index]);
}

void CSalamanderGeneral::GetPluginFSName(char* buf, int fsNameIndex)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPluginFSName(, %d)", fsNameIndex);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPluginFSName() only from main thread!");
        buf[0] = 0;
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL && data->SupportFS && fsNameIndex >= 0 && fsNameIndex < data->FSNames.Count)
        lstrcpyn(buf, data->FSNames[fsNameIndex], MAX_PATH);
    else
    {
        TRACE_E("CSalamanderGeneral::GetPluginFSName(): incorrect call (not supporting FS or 'fsNameIndex' is out of range)!");
        buf[0] = 0;
    }
}

BOOL CSalamanderGeneral::SetFlagLoadOnSalamanderStart(BOOL start)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SetFlagLoadOnSalamanderStart(%d)", start);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SetFlagLoadOnSalamanderStart() only from main thread!");
        return FALSE;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
    {
        BOOL prev = data->LoadOnStart != 0;
        data->LoadOnStart = start != 0;
        return prev;
    }
    else
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::SetFlagLoadOnSalamanderStart().");
        return FALSE;
    }
}

void CSalamanderGeneral::PostUnloadThisPlugin()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::PostUnloadThisPlugin()");
    if (MainThreadID == GetCurrentThreadId())
    { // due to a call from the entry-point, where the Plugin is set to -1 (only for retrieving plugin data)
        // Before WM_USER_POSTCMDORUNLOADPLUGIN is reached, the Plugin would have been reset (according to the return value of the entry point)
        CPluginData* data = Plugins.GetPluginData(Plugin);
        if (data != NULL)
        {
            data->ShouldUnload = TRUE;
            ExecCmdsOrUnloadMarkedPlugins = TRUE;
        }
        else
        {
            TRACE_E("Unexpected situation in CSalamanderGeneral::PostUnloadThisPlugin().");
        }
    }
    else // Plugin is certainly set outside the entry-point...
    {
        if (MainWindow != NULL && MainWindow->HWindow != NULL)
        {
            // test for calling during entry-point execution (Plugin is set to -1)
            if ((INT_PTR)Plugin == -1)
            {
                TRACE_E("You can call CSalamanderGeneral::PostUnloadThisPlugin only from main "
                        "thread when plugin entry-point is not finished yet!");
            }
            else
            {
                PostMessage(MainWindow->HWindow, WM_USER_POSTCMDORUNLOADPLUGIN, (WPARAM)Plugin, 0);
            }
        }
        else
        {
            TRACE_E("Unexpected situation (2) in CSalamanderGeneral::PostUnloadThisPlugin().");
        }
    }
}

void CSalamanderGeneral::PostPluginMenuChanged()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::PostPluginMenuChanged()");
    if (MainThreadID == GetCurrentThreadId())
    { // due to a call from the entry-point, where the Plugin is set to -1 (only for retrieving plugin data)
        // Before WM_USER_POSTCMDORUNLOADPLUGIN is reached, the Plugin would have been reset (according to the return value of the entry point)
        CPluginData* data = Plugins.GetPluginData(Plugin);
        if (data != NULL)
        {
            data->ShouldRebuildMenu = TRUE;
            ExecCmdsOrUnloadMarkedPlugins = TRUE;
        }
        else
        {
            TRACE_E("Unexpected situation in CSalamanderGeneral::PostPluginMenuChanged().");
        }
    }
    else // Plugin is certainly set outside the entry-point...
    {
        if (MainWindow != NULL && MainWindow->HWindow != NULL)
        {
            // test for calling during entry-point execution (Plugin is set to -1)
            if ((INT_PTR)Plugin == -1)
            {
                TRACE_E("You can call CSalamanderGeneral::PostPluginMenuChanged only from main "
                        "thread when plugin entry-point is not finished yet!");
            }
            else
            {
                PostMessage(MainWindow->HWindow, WM_USER_POSTCMDORUNLOADPLUGIN, (WPARAM)Plugin, 1);
            }
        }
        else
        {
            TRACE_E("Unexpected situation (2) in CSalamanderGeneral::PostPluginMenuChanged().");
        }
    }
}

void CSalamanderGeneral::PostMenuExtCommand(int id, BOOL waitForSalIdle)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::PostMenuExtCommand(%d, %d)", id, waitForSalIdle);
    if (waitForSalIdle)
    {
        if (id < 0 || id >= 1000000)
        {
            TRACE_E("CSalamanderGeneral::PostMenuExtCommand: id is invalid (" << id << " is not in range 0-999999).");
            return;
        }

        if (MainThreadID == GetCurrentThreadId())
        { // due to a call from the entry-point, where the Plugin is set to -1 (only for retrieving plugin data)
            // Before WM_USER_POSTCMDORUNLOADPLUGIN is reached, the Plugin would have been reset (according to the return value of the entry point)
            CPluginData* data = Plugins.GetPluginData(Plugin);
            if (data != NULL)
            {
                data->Commands.Add(500 + id); // salCmd are in the range <0, 499>, 500 is the first available number
                ExecCmdsOrUnloadMarkedPlugins = TRUE;
            }
            else
            {
                TRACE_E("Unexpected situation in CSalamanderGeneral::PostMenuExtCommand().");
            }
        }
        else // Plugin is certainly set outside the entry-point...
        {
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
            {
                // test for calling during entry-point execution (Plugin is set to -1)
                if ((INT_PTR)Plugin == -1)
                {
                    TRACE_E("You can call CSalamanderGeneral::PostMenuExtCommand only from main "
                            "thread when plugin entry-point is not finished yet!");
                }
                else
                { // 0 - unload, 1 - rebuild menu, 2-501 salCmd, 502-1000501 menuCmd
                    PostMessage(MainWindow->HWindow, WM_USER_POSTCMDORUNLOADPLUGIN, (WPARAM)Plugin, 502 + id);
                }
            }
            else
            {
                TRACE_E("Unexpected situation (2) in CSalamanderGeneral::PostMenuExtCommand().");
            }
        }
    }
    else
    {
        if (MainThreadID == GetCurrentThreadId())
        { // test for calling from entry-point (Plugin is set to -1)
            if ((INT_PTR)Plugin == -1)
            {
                TRACE_E("You may not call CSalamanderGeneral::PostMenuExtCommand from entry-point!");
                return;
            }
        }
        else
        {
            // test for calling during entry-point execution (Plugin is set to -1)
            if ((INT_PTR)Plugin == -1)
            {
                TRACE_E("You may not call CSalamanderGeneral::PostMenuExtCommand when "
                        "entry-point is not finished yet!");
                return;
            }
        }
        if (MainWindow != NULL && MainWindow->HWindow != NULL)
        {
            PostMessage(MainWindow->HWindow, WM_USER_POSTMENUEXTCMD, (WPARAM)Plugin, (LPARAM)id);
        }
        else
        {
            TRACE_E("Unexpected situation in CSalamanderGeneral::PostMenuExtCommand().");
        }
    }
}

BOOL CSalamanderGeneral::SalamanderIsNotBusy(DWORD* lastIdleTime)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalamanderIsNotBusy()");
    return ::SalamanderIsNotBusy(lastIdleTime);
}

void CSalamanderGeneral::CallLoadOrSaveConfiguration(BOOL load,
                                                     FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                                     void* param)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::CallLoadOrSaveConfiguration(%d, ,)", load);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::CallLoadOrSaveConfiguration() only from main thread!");
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
    {
        data->CallLoadOrSaveConfiguration(load, loadOrSaveFunc, param);
    }
    else
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::CallLoadOrSaveConfiguration().");
    }
}

void CSalamanderGeneral::SetPluginBugReportInfo(const char* message, const char* email)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SetPluginBugReportInfo(%s)", message);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SetPluginBugReportInfo() only from main thread!");
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
    {
        if (data->BugReportMessage != NULL)
            free(data->BugReportMessage);
        if (message != NULL)
            data->BugReportMessage = ::DupStr(message);
        else
            data->BugReportMessage = NULL;
        if (data->BugReportEMail != NULL)
            free(data->BugReportEMail);
        if (email != NULL)
        {
            data->BugReportEMail = ::DupStr(email);
            if (data->BugReportEMail != NULL && strlen(data->BugReportEMail) > 100)
            {
                data->BugReportEMail[100] = 0;
            }
        }
        else
            data->BugReportEMail = NULL;
    }
    else
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::SetPluginBugReportInfo().");
    }
}

void CSalamanderGeneral::FocusNameInPanel(int panel, const char* path, const char* name)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::FocusNameInPanel(%d, %s, %s)", panel, path, name);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::FocusNameInPanel() only from main thread!");
        return;
    }
    if (name == NULL || path == NULL)
    {
        TRACE_E("CSalamanderGeneral::FocusNameInPanel(): incorrect parameters (name == NULL || path == NULL)!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    char pathBackup[MAX_PATH + 200];
    char nameBackup[MAX_PATH + 200];
    lstrcpyn(pathBackup, path, MAX_PATH + 200);
    lstrcpyn(nameBackup, name, MAX_PATH + 200);
    if (p != NULL)
        SendMessage(p->HWindow, WM_USER_FOCUSFILE, (WPARAM)nameBackup, (LPARAM)pathBackup);
}

BOOL CSalamanderGeneral::ChangePanelPath(int panel, const char* path, int* failReason,
                                         int suggestedTopIndex, const char* suggestedFocusName,
                                         BOOL convertFSPathToInternal)
{
    CALL_STACK_MESSAGE6("CSalamanderGeneral::ChangePanelPath(%d, %s, , %d, %s, %d)",
                        panel, path, suggestedTopIndex, suggestedFocusName, convertFSPathToInternal);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPath() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        return p->ChangeDir(path, suggestedTopIndex, suggestedFocusName, 3 /*change-dir*/,
                            failReason, convertFSPathToInternal);
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CSalamanderGeneral::ChangePanelPathToDisk(int panel, const char* path, int* failReason,
                                               int suggestedTopIndex, const char* suggestedFocusName)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::ChangePanelPathToDisk(%d, %s, , %d, %s)",
                        panel, path, suggestedTopIndex, suggestedFocusName);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPathToDisk() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        return p->ChangePathToDisk(GetMsgBoxParent(), path, suggestedTopIndex, suggestedFocusName,
                                   NULL, TRUE, FALSE, FALSE, failReason);
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CSalamanderGeneral::ChangePanelPathToArchive(int panel, const char* archive, const char* archivePath,
                                                  int* failReason, int suggestedTopIndex,
                                                  const char* suggestedFocusName, BOOL forceUpdate)
{
    CALL_STACK_MESSAGE7("CSalamanderGeneral::ChangePanelPathToArchive(%d, %s, %s, , %d, %s, %d)",
                        panel, archive, archivePath, suggestedTopIndex, suggestedFocusName, forceUpdate);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPathToArchive() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        return p->ChangePathToArchive(archive, archivePath, suggestedTopIndex, suggestedFocusName,
                                      forceUpdate, NULL, TRUE, failReason);
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CSalamanderGeneral::ChangePanelPathToPluginFS(int panel, const char* fsName, const char* fsUserPart,
                                                   int* failReason, int suggestedTopIndex,
                                                   const char* suggestedFocusName, BOOL forceUpdate,
                                                   BOOL convertPathToInternal)
{
    CALL_STACK_MESSAGE8("CSalamanderGeneral::ChangePanelPathToPluginFS(%d, %s, %s, , %d, %s, %d, %d)",
                        panel, fsName, fsUserPart, suggestedTopIndex, suggestedFocusName, forceUpdate,
                        convertPathToInternal);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPathToPluginFS() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        return p->ChangePathToPluginFS(fsName, fsUserPart, suggestedTopIndex, suggestedFocusName,
                                       forceUpdate, 2 /*Print all errors*/, NULL, TRUE, failReason,
                                       FALSE, FALSE, convertPathToInternal);
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CSalamanderGeneral::ChangePanelPathToDetachedFS(int panel, CPluginFSInterfaceAbstract* detachedFS,
                                                     int* failReason, int suggestedTopIndex,
                                                     const char* suggestedFocusName)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::ChangePanelPathToDetachedFS(%d, , , %d, %s)",
                        panel, suggestedTopIndex, suggestedFocusName);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPathToDetachedFS() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        int fsIndex = -1;
        CDetachedFSList* list = MainWindow->DetachedFSList;
        int i;
        for (i = 0; i < list->Count; i++)
        {
            if (list->At(i)->GetInterface() == detachedFS)
            {
                fsIndex = i;
                break;
            }
        }
        if (fsIndex != -1)
        {
            return p->ChangePathToDetachedFS(fsIndex, suggestedTopIndex, suggestedFocusName, TRUE, failReason);
        }
        else
        {
            TRACE_E("Parameter 'detachedFS' is not detached FS in "
                    "CSalamanderGeneral::ChangePanelPathToDetachedFS().");
        }
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CSalamanderGeneral::ChangePanelPathToFixedDrive(int panel, int* failReason)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::ChangePanelPathToFixedDrive(%d,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPathToFixedDrive() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        return p->ChangeToFixedDrive(GetMsgBoxParent(), NULL, TRUE, FALSE, failReason);
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CSalamanderGeneral::ChangePanelPathToRescuePathOrFixedDrive(int panel, int* failReason)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::ChangePanelPathToRescuePathOrFixedDrive(%d,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ChangePanelPathToRescuePathOrFixedDrive() only from main thread!");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        return p->ChangeToRescuePathOrFixedDrive(GetMsgBoxParent(), NULL, TRUE, FALSE, FSTRYCLOSE_CHANGEPATH, failReason);
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

void CSalamanderGeneral::RefreshPanelPath(int panel, BOOL forceRefresh, BOOL focusFirstNewItem)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::RefreshPanelPath(%d, %d, %d)",
                        panel, forceRefresh, focusFirstNewItem);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::RefreshPanelPath() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        if (forceRefresh && p->Is(ptZIPArchive))
        { // At the archive, we will ensure a hard refresh by damaging the archive stamp
            p->SetZIPArchiveSize(CQuadWord(-1, -1));
        }
        p->FocusFirstNewItem = focusFirstNewItem;
        p->RefreshDirectory(FALSE, forceRefresh);
    }
}

void CSalamanderGeneral::PostRefreshPanelPath(int panel, BOOL focusFirstNewItem)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::PostRefreshPanelPath(%d, %d)", panel, focusFirstNewItem);
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        // Post a hard refresh
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        p->FocusFirstNewItem = focusFirstNewItem; // not synchronized (can be called not only in the main thread), but it shouldn't matter
        PostMessage(p->HWindow, WM_USER_REFRESH_DIR, 0, t1);
    }
}

void CSalamanderGeneral::PostRefreshPanelFS(CPluginFSInterfaceAbstract* modifiedFS, BOOL focusFirstNewItem)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::PostRefreshPanelFS(, %d)", focusFirstNewItem);
    PostRefreshPanelFS2(modifiedFS, focusFirstNewItem);
}

BOOL CSalamanderGeneral::PostRefreshPanelFS2(CPluginFSInterfaceAbstract* modifiedFS, BOOL focusFirstNewItem)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::PostRefreshPanelFS2(, %d)", focusFirstNewItem);
    CFilesWindow* p = NULL;
    if (MainWindow != NULL)
    {
        // There is no synchronization problem because PluginFS is reset only after CloseFS, which would
        // should cancel the thread monitoring changes to the file system (after CloseFS there should be no call
        // PostRefreshPanelFS2)
        if (MainWindow->LeftPanel != NULL && MainWindow->LeftPanel->Is(ptPluginFS) &&
            MainWindow->LeftPanel->GetPluginFS()->Contains(modifiedFS))
        {
            p = MainWindow->LeftPanel;
        }
        if (MainWindow->RightPanel != NULL && MainWindow->RightPanel->Is(ptPluginFS) &&
            MainWindow->RightPanel->GetPluginFS()->Contains(modifiedFS))
        {
            p = MainWindow->RightPanel;
        }
    }
    if (p != NULL)
    {
        // Post a hard refresh
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        p->FocusFirstNewItem = focusFirstNewItem; // not synchronized (can be called not only in the main thread), but it shouldn't matter
        PostMessage(p->HWindow, WM_USER_REFRESH_DIR, 0, t1);
        return TRUE;
    }
    else
        return FALSE;
}

BOOL CSalamanderGeneral::CloseDetachedFS(HWND parent, CPluginFSInterfaceAbstract* detachedFS)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::CloseDetachedFS(,)");
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::CloseDetachedFS() only from main thread!");
        return FALSE;
    }
    if (MainWindow->DetachedFSList->IsGood()) // to ensure the success of Delete
    {
        CDetachedFSList* list = MainWindow->DetachedFSList;
        int i;
        for (i = 0; i < list->Count; i++)
        {
            if (list->At(i)->GetInterface() == detachedFS)
            {
                CPluginFSInterfaceEncapsulation* fs = list->At(i);
                BOOL dummy;
                if (fs->TryCloseOrDetach(FALSE, FALSE, dummy, FSTRYCLOSE_PLUGINCLOSEDETACHEDFS)) // FS has no objection to closing
                {
                    CPluginInterfaceForFSEncapsulation plugin(fs->GetPluginInterfaceForFS()->GetInterface(),
                                                              fs->GetPluginInterfaceForFS()->GetBuiltForVersion());
                    if (plugin.NotEmpty())
                    {
                        fs->ReleaseObject(parent);
                        plugin.CloseFS(fs->GetInterface());
                        list->Delete(i);
                        if (!list->IsGood())
                            list->ResetState();
                        return TRUE;
                    }
                    else
                        TRACE_E("Unexpected situation in CSalamanderGeneral::CloseDetachedFS()");
                }
                break;
            }
        }
    }
    return FALSE;
}

BOOL CSalamanderGeneral::DuplicateAmpersands(char* buffer, int bufferSize)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::DuplicateAmpersands(%s, %d)", buffer, bufferSize);
    return ::DuplicateAmpersands(buffer, bufferSize);
}

void CSalamanderGeneral::RemoveAmpersands(char* text)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::RemoveAmpersands(%s)", text);
    ::RemoveAmpersands(text);
}

BOOL CSalamanderGeneral::ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                                           const CSalamanderVarStrEntry* variables)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::ValidateVarString(, %s, , ,)", varText);
    if (varText == NULL || variables == NULL)
    {
        TRACE_E("CSalamanderGeneral::ValidateVarString(): invalid parameters!");
        return FALSE;
    }
    return ::ValidateVarString(msgParent, varText, errorPos1, errorPos2, variables);
}

BOOL CSalamanderGeneral::ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                                         const CSalamanderVarStrEntry* variables, void* param,
                                         BOOL ignoreEnvVarNotFoundOrTooLong,
                                         DWORD* varPlacements, int* varPlacementsCount,
                                         BOOL detectMaxVarWidths, int* maxVarWidths,
                                         int maxVarWidthsCount)
{
    CALL_STACK_MESSAGE6("CSalamanderGeneral::ExpandVarString(, %s, , %d, , , %d, , , %d, , %d)",
                        varText, bufferLen, ignoreEnvVarNotFoundOrTooLong, detectMaxVarWidths,
                        maxVarWidthsCount);
    if (bufferLen <= 0 || buffer == NULL || varText == NULL || variables == NULL)
    {
        TRACE_E("CSalamanderGeneral::ExpandVarString(): invalid parameters!");
        return FALSE;
    }
    return ::ExpandVarString(msgParent, varText, buffer, bufferLen, variables, param,
                             ignoreEnvVarNotFoundOrTooLong, varPlacements, varPlacementsCount,
                             detectMaxVarWidths, maxVarWidths, maxVarWidthsCount);
}

BOOL CSalamanderGeneral::EnumInstalledModules(int* index, char* module, char* version)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::EnumInstalledModules(, ,)");
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::EnumInstalledModules() only from main thread!");
        return FALSE;
    }
    return Plugins.EnumInstalledModules(index, module, version);
}

BOOL CSalamanderGeneral::CopyTextToClipboard(const char* text, int textLen, BOOL showEcho, HWND echoParent)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::CopyTextToClipboard(, %d, %d,)", textLen, showEcho);
    // Removed parameter text, which did not have to be null-terminated
    if (text == NULL)
    {
        TRACE_E("Unexpected parameter (NULL) in CSalamanderGeneral::CopyTextToClipboard().");
        return FALSE;
    }
    return ::CopyTextToClipboard(text, textLen, showEcho, echoParent);
}

BOOL CSalamanderGeneral::CopyTextToClipboardW(const wchar_t* text, int textLen, BOOL showEcho, HWND echoParent)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::CopyTextToClipboardW(, %d, %d,)", textLen, showEcho);
    // Removed parameter text, which did not have to be null-terminated
    if (text == NULL)
    {
        TRACE_E("Unexpected parameter (NULL) in CSalamanderGeneral::CopyTextToClipboardW().");
        return FALSE;
    }
    return ::CopyTextToClipboardW(text, textLen, showEcho, echoParent);
}

BOOL CSalamanderGeneral::IsPluginInstalled(const char* pluginSPL)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::IsPluginInstalled(%s)", pluginSPL);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::IsPluginInstalled() only from main thread!");
        return FALSE;
    }
    if (pluginSPL != NULL)
    {
        CPluginData* data = Plugins.GetPluginDataFromSuffix(pluginSPL);
        return data != NULL;
    }
    else
    {
        TRACE_E("Unexpected parameter 'pluginSPL' (NULL) in CSalamanderGeneral::IsPluginInstalled().");
        return FALSE;
    }
}

BOOL ViewFileInPluginViewer(const char* pluginSPL,
                            CSalamanderPluginViewerData* pluginData,
                            BOOL useCache, const char* rootTmpPath,
                            const char* fileNameInCache, int& error)
{
    error = -1; // unknown
    if (pluginData == NULL || pluginData->Size < sizeof(CSalamanderPluginViewerData) ||
        pluginData->FileName == NULL || pluginData->FileName[0] == 0)
    {
        TRACE_E("Unexpected value of 'pluginData' in CSalamanderGeneral::ViewFileInPluginViewer!");
        return FALSE;
    }

    CALL_STACK_MESSAGE7("CSalamanderGeneral::ViewFileInPluginViewer(%s, %d, %s, %d, %s, %s,)",
                        pluginSPL, pluginData->Size, pluginData->FileName, useCache,
                        (useCache ? rootTmpPath : "(ignored)"),
                        (useCache ? fileNameInCache : "(ignored)"));

    char viewUniqueName[50]; // We need a unique name for the cached view file
    viewUniqueName[0] = 0;
    const char* fileName; // name of the file that we will send to the viewer
    if (useCache)
    {
        // we verify if 'fileNameInCache' (name without path) was not entered incorrectly
        const char* s = NULL;
        if (fileNameInCache != NULL)
        {
            s = fileNameInCache;
            while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
                   *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
                s++;
        }
        if (fileNameInCache == NULL || fileNameInCache[0] == 0 || *s != 0)
        {
            TRACE_E("Unexpected value of 'fileNameInCache' in CSalamanderGeneral::ViewFileInPluginViewer!");
            error = 3;
            ::DeleteFile(pluginData->FileName);
            return FALSE;
        }

        // we insert the file 'pluginData->FileName' into the disk cache under the name 'fileNameInCache'
        while (1)
        {
            sprintf(viewUniqueName, "ViewFile %X", GetTickCount());
            BOOL exists;
            fileName = DiskCache.GetName(viewUniqueName, fileNameInCache, &exists, TRUE, rootTmpPath, FALSE, NULL, NULL);
            if (fileName == NULL) // error (if 'exists' is TRUE -> fatal, otherwise "file already exists")
            {
                if (!exists)
                    Sleep(100); // file exists -> almost impossible, but we will handle it anyway
                else            // fatal error
                {
                    error = 3;
                    ::DeleteFile(pluginData->FileName);
                    return FALSE; // fatal error
                }
            }
            else
                break; // we have the name in the disk cache, everything is OK
        }
        if (!::SalMoveFile(pluginData->FileName, fileName))
        {
            DWORD err = GetLastError();
            TRACE_E("Unable to move file to disk cache! (error " << ::GetErrorText(err) << ")");
            ::DeleteFile(pluginData->FileName);
            DiskCache.ReleaseName(viewUniqueName, FALSE);
            error = 3;
            return FALSE;
        }
        else // We managed to obtain the temporary file, we need to call NamePrepared()
        {
            CQuadWord size(0, 0);
            HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, 0, NULL));
            if (file != INVALID_HANDLE_VALUE)
            { // We ignore the error, the file size is not that important.
                DWORD err;
                ::SalGetFileSize(file, size, err);
                HANDLES(CloseHandle(file));
            }

            DiskCache.NamePrepared(viewUniqueName, size);
        }
    }
    else
        fileName = pluginData->FileName;

    // position for viewery
    WINDOWPLACEMENT place;
    place.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(MainWindow->HWindow, &place);
    // GetWindowPlacement reads Taskbar, so if the Taskbar is at the top or on the left,
    // the values are shifted by its dimensions. We will perform the correction.
    RECT monitorRect;
    RECT workRect;
    MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
    OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
               workRect.top - monitorRect.top);
    // We do not want a minimized viewer, even if the main window is minimized
    if (place.showCmd == SW_MINIMIZE || place.showCmd == SW_SHOWMINIMIZED ||
        place.showCmd == SW_SHOWMINNOACTIVE)
        place.showCmd = SW_SHOWNORMAL;

    // finally open the viewer alone
    BOOL diskCacheNameClosed = FALSE;
    error = 0;
    if (pluginSPL != NULL) // viewer from plug-in
    {
        CPluginData* data = Plugins.GetPluginDataFromSuffix(pluginSPL);
        if (data != NULL && data->SupportViewer)
        {
            if (data->InitDLL(MainWindow->HWindow)
                /*&& PluginIfaceForViewer.NotEmpty()*/) // unnecessary, because downgrade is not possible and InitDLL ifacy checks
            {
                HANDLE lock = NULL;
                BOOL lockOwner = FALSE;
                BOOL ret = data->GetPluginInterfaceForViewer()->ViewFile(fileName, place.rcNormalPosition.left,
                                                                         place.rcNormalPosition.top,
                                                                         place.rcNormalPosition.right - place.rcNormalPosition.left,
                                                                         place.rcNormalPosition.bottom - place.rcNormalPosition.top,
                                                                         place.showCmd, Configuration.AlwaysOnTop,
                                                                         useCache, &lock, &lockOwner, pluginData, -1, -1);
                if (!ret)
                {
                    TRACE_E("PluginIfaceForViewer.ViewFile() returns error.");
                    error = 2;
                }
                else
                {
                    if (useCache && lock != NULL)
                    {
                        if (lockOwner) // add handle to 'lock' to HANDLES (disk-cache will want to close it - it will be looking for it)
                            HANDLES_ADD(__htEvent, __hoCreateEvent, lock);
                        DiskCache.AssignName(viewUniqueName, lock, lockOwner, crtDirect);
                        diskCacheNameClosed = TRUE;
                    }
                }
            }
            else
                error = 1;
        }
        else
            error = 1;
        if (error == 1)
            TRACE_E("Unable to load plugin.");
    }
    else // internal viewer
    {
        if (Configuration.SavePosition &&
            Configuration.WindowPlacement.length != 0)
        {
            place = Configuration.WindowPlacement;
            // GetWindowPlacement reads Taskbar, so if the Taskbar is at the top or on the left,
            // the values are shifted by its dimensions. We will perform the correction.
            RECT monitorRect2;
            RECT workRect2;
            MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect2, &monitorRect2);
            OffsetRect(&place.rcNormalPosition, workRect2.left - monitorRect2.left,
                       workRect2.top - monitorRect2.top);
            MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
        }

        HANDLE lock = NULL;
        BOOL lockOwner = FALSE;
        if (OpenViewer(fileName, vtText,
                       place.rcNormalPosition.left,
                       place.rcNormalPosition.top,
                       place.rcNormalPosition.right - place.rcNormalPosition.left,
                       place.rcNormalPosition.bottom - place.rcNormalPosition.top,
                       place.showCmd, useCache, &lock, &lockOwner, pluginData, -1, -1))
        {
            if (useCache && lock != NULL)
            {
                DiskCache.AssignName(viewUniqueName, lock, lockOwner, crtDirect);
                diskCacheNameClosed = TRUE;
            }
        }
        else
        {
            TRACE_E("OpenViewer() returns error.");
            error = 2;
        }
    }

    // if we have not assigned a name to the disk cache, release the record...
    if (useCache && !diskCacheNameClosed)
    {
        DiskCache.ReleaseName(viewUniqueName, FALSE);
        //    ::DeleteFile(fileName);   // the cache has already deleted the file and fileName has been deallocated
    }
    return error == 0; // Returning success?
}

BOOL CSalamanderGeneral::ViewFileInPluginViewer(const char* pluginSPL,
                                                CSalamanderPluginViewerData* pluginData,
                                                BOOL useCache, const char* rootTmpPath,
                                                const char* fileNameInCache, int& error)
{
    error = -1; // unknown

    // Ensure that the call does not pass outside the main thread and in the entry-point
    if (MainThreadID != GetCurrentThreadId() || (INT_PTR)Plugin == -1)
    {
        if (MainThreadID == GetCurrentThreadId()) // If both errors occur (different thread + unfinished entry-point), the entry-point takes precedence
            TRACE_E("You may not call CSalamanderGeneral::ViewFileInPluginViewer from entry-point!");
        else
            TRACE_E("You can call CSalamanderGeneral::ViewFileInPluginViewer only from main thread!");
        return FALSE;
    }

    return ::ViewFileInPluginViewer(pluginSPL, pluginData, useCache,
                                    rootTmpPath, fileNameInCache, error);
}

void CSalamanderGeneral::ExecuteAssociation(HWND parent, const char* path, const char* name)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::ExecuteAssociation(0x%p, %s, %s)", parent, path, name);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ExecuteAssociation() only from main thread!");
        return;
    }
    MainWindow->SetDefaultDirectories(); // so that the starting process inherits the correct working directory
    ::ExecuteAssociation(parent, path, name);
}

int CSalamanderGeneral::GetPanelTopIndex(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelTopIndex(%d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelTopIndex() only from main thread!");
        return 0;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
        return p->ListBox->GetTopIndex();
    return 0; // error, should not occur...
}

void CSalamanderGeneral::GetPanelEnumFilesParams(int panel, int* enumFilesSourceUID, int* enumFilesCurrentIndex)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetPanelEnumFilesParams(%d, ,)", panel);
    if (enumFilesCurrentIndex != NULL)
        *enumFilesCurrentIndex = -1;
    if (enumFilesSourceUID != NULL)
        *enumFilesSourceUID = -1;
    else
    {
        TRACE_E("CSalamanderGeneral::GetPanelEnumFilesParams(): 'enumFilesSourceUID' cannot be NULL!");
        return;
    }
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelEnumFilesParams() only from main thread!");
        return;
    }

    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && p->Is(ptDisk))
    {
        *enumFilesSourceUID = p->EnumFileNamesSourceUID;
        if (enumFilesCurrentIndex != NULL)
        {
            int i = p->GetCaretIndex();
            if (i >= p->Dirs->Count && i < p->Dirs->Count + p->Files->Count)
                *enumFilesCurrentIndex = i - p->Dirs->Count;
        }
    }
}

BOOL CSalamanderGeneral::GetPanelWithPluginFS(CPluginFSInterfaceAbstract* pluginFS, int& panel)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetPanelWithPluginFS(, )");
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetPanelWithPluginFS() only from main thread!");
        return FALSE;
    }
    if (pluginFS == NULL)
        return FALSE;
    if (MainWindow->LeftPanel->Is(ptPluginFS) &&
        MainWindow->LeftPanel->GetPluginFS()->GetInterface() == pluginFS)
    {
        panel = PANEL_LEFT;
        return TRUE;
    }
    if (MainWindow->RightPanel->Is(ptPluginFS) &&
        MainWindow->RightPanel->GetPluginFS()->GetInterface() == pluginFS)
    {
        panel = PANEL_RIGHT;
        return TRUE;
    }
    return FALSE;
}

void CSalamanderGeneral::PostChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::PostChangeOnPathNotification(%s, %d)", path, includingSubdirs);
    MainWindow->PostChangeOnPathNotification(path, includingSubdirs);
}

DWORD
CSalamanderGeneral::SalCheckPath(BOOL echo, const char* path, DWORD err, HWND parent)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::SalCheckPath(%d, %s, %u,)", echo, path, err);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SalCheckPath() only from main thread!");
        return ERROR_SUCCESS;
    }
    return ::SalCheckPath(echo, path, err, TRUE, parent); // does not depend on the 'postRefresh' value (StopRefresh is certainly > 0)
}

BOOL CSalamanderGeneral::SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::SalCheckAndRestorePath(, %s, %d)", path, tryNet);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SalCheckAndRestorePath() only from main thread!");
        return FALSE;
    }
    return ::SalCheckAndRestorePath(parent, path, tryNet);
}

BOOL CSalamanderGeneral::SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err,
                                                       DWORD& lastErr, BOOL& pathInvalid, BOOL& cut,
                                                       BOOL donotReconnect)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::SalCheckAndRestorePathWithCut(, %s, %d, , , , , %d)",
                        path, tryNet, donotReconnect);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SalCheckAndRestorePathWithCut() only from main thread!");
        lastErr = err = ERROR_SUCCESS;
        pathInvalid = TRUE;
        cut = FALSE;
        return FALSE;
    }
    return ::SalCheckAndRestorePathWithCut(parent, path, tryNet, err, lastErr, pathInvalid, cut,
                                           donotReconnect);
}

BOOL CSalamanderGeneral::SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                                      const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                                      const char* curPath, const char* curArchivePath, int* error,
                                      int pathBufSize)
{
    CALL_STACK_MESSAGE7("CSalamanderGeneral::SalParsePath(, %s, , , , %s, , %d, %s, %s, , %d)",
                        path, errorTitle, curPathIsDiskOrArchive, curPath, curArchivePath,
                        pathBufSize);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SalParsePath() only from main thread!");
        if (error != NULL)
            *error = SPP_WINDOWSPATHERROR;
        return FALSE;
    }
    return ::SalParsePath(parent, path, type, isDir, secondPart, errorTitle, nextFocus,
                          curPathIsDiskOrArchive, curPath, curArchivePath, error, pathBufSize);
}

BOOL CSalamanderGeneral::SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle,
                                             int selCount, char* path, char* secondPart, BOOL pathIsDir,
                                             BOOL backslashAtEnd, const char* dirName,
                                             const char* curDiskPath, char*& mask)
{
    CALL_STACK_MESSAGE10("CSalamanderGeneral::SalSplitWindowsPath(, %s, %s, %d, %s, %s, %d, %d, %s, %s,)",
                         title, errorTitle, selCount, path, secondPart, pathIsDir, backslashAtEnd,
                         dirName, curDiskPath);
    return ::SalSplitWindowsPath(parent, title, errorTitle, selCount, path, secondPart,
                                 pathIsDir, backslashAtEnd, dirName, curDiskPath, mask);
}

BOOL CSalamanderGeneral::SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle,
                                             int selCount, char* path, char* afterRoot, char* secondPart,
                                             BOOL pathIsDir, BOOL backslashAtEnd, const char* dirName,
                                             const char* curPath, char*& mask, char* newDirs,
                                             SGP_IsTheSamePathF isTheSamePathF)
{
    CALL_STACK_MESSAGE11("CSalamanderGeneral::SalSplitGeneralPath(, %s, %s, %d, %s, %s, %s, %d, %d, %s, %s, , ,)",
                         title, errorTitle, selCount, path, afterRoot, secondPart, pathIsDir, backslashAtEnd,
                         dirName, curPath);
    return ::SalSplitGeneralPath(parent, title, errorTitle, selCount, path, afterRoot, secondPart,
                                 pathIsDir, backslashAtEnd, dirName, curPath, mask, newDirs,
                                 isTheSamePathF);
}

BOOL CSalamanderGeneral::SalRemovePointsFromPath(char* afterRoot)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SalRemovePointsFromPath(%s)", afterRoot);
    return ::SalRemovePointsFromPath(afterRoot);
}

BOOL CSalamanderGeneral::GetConfigParameter(int paramID, void* buffer, int bufferSize, int* type)
{
    SLOW_CALL_STACK_MESSAGE3("CSalamanderGeneral::GetConfigParameter(%d, , %d,)", paramID, bufferSize);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetConfigParameter() only from main thread!");
        if (type != NULL)
            *type = SALCFGTYPE_NOTFOUND;
        return FALSE;
    }
    char auxBuf[500];
    int auxType = SALCFGTYPE_BOOL;
    int auxDataSize = 4;
    BOOL ret = TRUE;
    switch (paramID)
    {
    case SALCFG_SELOPINCLUDEDIRS:
        *((DWORD*)auxBuf) = (DWORD)Configuration.IncludeDirs;
        break;
    case SALCFG_SAVEONEXIT:
        *((DWORD*)auxBuf) = (DWORD)Configuration.AutoSave;
        break;
    case SALCFG_MINBEEPWHENDONE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.MinBeepWhenDone;
        break;
    case SALCFG_HIDEHIDDENORSYSTEMFILES:
        *((DWORD*)auxBuf) = (DWORD)Configuration.NotHiddenSystemFiles;
        break;
    case SALCFG_ALWAYSONTOP:
        *((DWORD*)auxBuf) = (DWORD)Configuration.AlwaysOnTop;
        break;
        //    case SALCFG_FASTDIRMOVE: *((DWORD *)auxBuf) = (DWORD)Configuration.FastDirectoryMove; break;
    case SALCFG_SORTUSESLOCALE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SortUsesLocale;
        break;
    case SALCFG_SORTDETECTNUMBERS:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SortDetectNumbers;
        break;
    case SALCFG_SORTBYEXTDIRSASFILES:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SortDirsByExt;
        break;
    case SALCFG_SINGLECLICK:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SingleClick;
        break;
    case SALCFG_TOPTOOLBARVISIBLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.TopToolBarVisible;
        break;
    case SALCFG_MIDDLETOOLBARVISIBLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.MiddleToolBarVisible;
        break;
    case SALCFG_BOTTOMTOOLBARVISIBLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.BottomToolBarVisible;
        break;
    case SALCFG_USERMENUTOOLBARVISIBLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.UserMenuToolBarVisible;
        break;
    case SALCFG_SAVEHISTORY:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SaveHistory;
        break;
    case SALCFG_ENABLECMDLINEHISTORY:
        *((DWORD*)auxBuf) = (DWORD)Configuration.EnableCmdLineHistory;
        break;
    case SALCFG_SAVECMDLINEHISTORY:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SaveCmdLineHistory;
        break;
    case SALCFG_SIZEFORMAT:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SizeFormat;
        break;
    case SALCFG_SELECTWHOLENAME:
        *((DWORD*)auxBuf) = (DWORD)Configuration.QuickRenameSelectAll;
        break;

    case SALCFG_FILENAMEFORMAT:
    {
        auxType = SALCFGTYPE_INT;
        auxDataSize = 4;
        *((DWORD*)auxBuf) = (DWORD)Configuration.FileNameFormat;
        break;
    }

    case SALCFG_INFOLINECONTENT:
    {
        auxType = SALCFGTYPE_STRING;
        auxDataSize = (int)strlen(Configuration.InfoLineContent) + 1;
        if (auxDataSize > 200)
            auxDataSize = 200; // We have limited the required buffer to 200 characters
        memcpy(auxBuf, Configuration.InfoLineContent, auxDataSize);
        auxBuf[auxDataSize - 1] = 0;
        break;
    }

    case SALCFG_USERECYCLEBIN:
    {
        auxType = SALCFGTYPE_INT;
        auxDataSize = 4;
        *((DWORD*)auxBuf) = (DWORD)Configuration.UseRecycleBin;
        break;
    }

    case SALCFG_RECYCLEBINMASKS:
    {
        auxType = SALCFGTYPE_STRING;
        auxDataSize = (int)strlen(Configuration.RecycleMasks.GetMasksString()) + 1;
        if (auxDataSize > MAX_PATH)
            auxDataSize = MAX_PATH; // We have limited the required buffer to MAX_PATH characters
        memcpy(auxBuf, Configuration.RecycleMasks.GetMasksString(), auxDataSize);
        auxBuf[auxDataSize - 1] = 0;
        break;
    }

    case SALCFG_COMPDIRSUSETIMERES:
        *((DWORD*)auxBuf) = (DWORD)Configuration.UseTimeResolution;
        break;

    case SALCFG_COMPDIRTIMERES:
    {
        auxType = SALCFGTYPE_INT;
        auxDataSize = 4;
        *((DWORD*)auxBuf) = (DWORD)Configuration.TimeResolution;
        break;
    }

    case SALCFG_CNFRMFILEDIRDEL:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmFileDirDel;
        break;
    case SALCFG_CNFRMNEDIRDEL:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmNEDirDel;
        break;
    case SALCFG_CNFRMFILEOVER:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmFileOver;
        break;
    case SALCFG_CNFRMDIROVER:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmDirOver;
        break;
    case SALCFG_CNFRMSHFILEDEL:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmSHFileDel;
        break;
    case SALCFG_CNFRMSHDIRDEL:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmSHDirDel;
        break;
    case SALCFG_CNFRMSHFILEOVER:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmSHFileOver;
        break;
    case SALCFG_CNFRMCREATEPATH:
        *((DWORD*)auxBuf) = (DWORD)Configuration.CnfrmCreatePath;
        break;
    case SALCFG_DRVSPECFLOPPYMON:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecFloppyMon;
        break;
    case SALCFG_DRVSPECFLOPPYSIM:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecFloppySimple;
        break;
    case SALCFG_DRVSPECREMOVABLEMON:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecRemovableMon;
        break;
    case SALCFG_DRVSPECREMOVABLESIM:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecRemovableSimple;
        break;
    case SALCFG_DRVSPECFIXEDMON:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecFixedMon;
        break;
    case SALCFG_DRVSPECFIXEDSIMPLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecFixedSimple;
        break;
    case SALCFG_DRVSPECREMOTEMON:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecRemoteMon;
        break;
    case SALCFG_DRVSPECREMOTESIMPLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecRemoteSimple;
        break;
    case SALCFG_DRVSPECREMOTEDONOTREF:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecRemoteDoNotRefreshOnAct;
        break;
    case SALCFG_DRVSPECCDROMMON:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecCDROMMon;
        break;
    case SALCFG_DRVSPECCDROMSIMPLE:
        *((DWORD*)auxBuf) = (DWORD)Configuration.DrvSpecCDROMSimple;
        break;

    case SALCFG_IFPATHISINACCESSIBLEGOTO:
    {
        auxType = SALCFGTYPE_STRING;
        char ifPathIsInaccessibleGoTo[MAX_PATH];
        GetIfPathIsInaccessibleGoTo(ifPathIsInaccessibleGoTo);
        auxDataSize = (int)strlen(ifPathIsInaccessibleGoTo) + 1;
        if (auxDataSize > MAX_PATH)
            auxDataSize = MAX_PATH; // We have limited the required buffer to MAX_PATH characters
        memcpy(auxBuf, ifPathIsInaccessibleGoTo, auxDataSize);
        auxBuf[auxDataSize - 1] = 0;
        break;
    }

    case SALCFG_VIEWEREOLCRLF:
        *((DWORD*)auxBuf) = (DWORD)Configuration.EOL_CRLF;
        break;
    case SALCFG_VIEWEREOLCR:
        *((DWORD*)auxBuf) = (DWORD)Configuration.EOL_CR;
        break;
    case SALCFG_VIEWEREOLLF:
        *((DWORD*)auxBuf) = (DWORD)Configuration.EOL_LF;
        break;
    case SALCFG_VIEWEREOLNULL:
        *((DWORD*)auxBuf) = (DWORD)Configuration.EOL_NULL;
        break;
    case SALCFG_VIEWERSAVEPOSITION:
        *((DWORD*)auxBuf) = (DWORD)Configuration.SavePosition;
        break;
    case SALCFG_VIEWERWRAPTEXT:
        *((DWORD*)auxBuf) = (DWORD)Configuration.WrapText;
        break;
    case SALCFG_AUTOCOPYSELTOCLIPBOARD:
        *((DWORD*)auxBuf) = (DWORD)Configuration.AutoCopySelection;
        break;

    case SALCFG_VIEWERTABSIZE:
    {
        auxType = SALCFGTYPE_INT;
        auxDataSize = 4;
        *((DWORD*)auxBuf) = (DWORD)Configuration.TabSize;
        break;
    }

    case SALCFG_VIEWERFONT:
    {
        auxType = SALCFGTYPE_LOGFONT;
        auxDataSize = sizeof(LOGFONT);
        if (UseCustomViewerFont)
            *((LOGFONT*)auxBuf) = ViewerLogFont;
        else
            GetDefaultViewerLogFont((LOGFONT*)auxBuf);
        break;
    }

    case SALCFG_ARCOTHERPANELFORPACK:
        *((DWORD*)auxBuf) = (DWORD)Configuration.UseAnotherPanelForPack;
        break;
    case SALCFG_ARCOTHERPANELFORUNPACK:
        *((DWORD*)auxBuf) = (DWORD)Configuration.UseAnotherPanelForUnpack;
        break;
    case SALCFG_ARCSUBDIRBYARCFORUNPACK:
        *((DWORD*)auxBuf) = (DWORD)Configuration.UseSubdirNameByArchiveForUnpack;
        break;
    case SALCFG_ARCUSESIMPLEICONS:
        *((DWORD*)auxBuf) = (DWORD)Configuration.UseSimpleIconsInArchives;
        break;

    default:
    {
        auxType = SALCFGTYPE_NOTFOUND;
        auxDataSize = 0;
        TRACE_E("Unknown parameter ID (" << paramID << ") in CSalamanderGeneral::GetConfigParameter().");
        ret = FALSE;
    }
    }
    if (type != NULL)
        *type = auxType;
    if (auxDataSize > 0 && auxDataSize <= bufferSize)
        memcpy(buffer, auxBuf, auxDataSize);
    else
    {
        if (bufferSize > 0 && auxDataSize > 0) // Let's copy at least what fits
        {
            memcpy(buffer, auxBuf, bufferSize);
            if (auxType == SALCFGTYPE_STRING)
                ((char*)buffer)[bufferSize - 1] = 0; // Trim string by zero
        }
        ret = FALSE;
    }
    return ret;
}

void CSalamanderGeneral::AlterFileName(char* tgtName, char* srcName, int format, int changedParts,
                                       BOOL isDir)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::AlterFileName(, %s, %d, %d, %d)",
                        srcName, format, changedParts, isDir);
    ::AlterFileName(tgtName, srcName, -1, format, changedParts, isDir);
}

void CSalamanderGeneral::CreateSafeWaitWindow(const char* message, const char* caption,
                                              int delay, BOOL showCloseButton, HWND hForegroundWnd)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::CreateSafeWaitWindow(%s, , %d, %d, 0x%p)", message, delay, showCloseButton, hForegroundWnd);
    ::CreateSafeWaitWindow(message, caption, delay, showCloseButton, hForegroundWnd);
}

void CSalamanderGeneral::DestroySafeWaitWindow()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::DestroySafeWaitWindow()");
    ::DestroySafeWaitWindow();
}

void CSalamanderGeneral::ShowSafeWaitWindow(BOOL show)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::ShowSafeWaitWindow(%d)", show);
    ::ShowSafeWaitWindow(show);
}

BOOL CSalamanderGeneral::GetSafeWaitWindowClosePressed()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetSafeWaitWindowClosePressed()");
    return ::GetSafeWaitWindowClosePressed();
}

void CSalamanderGeneral::SetSafeWaitWindowText(const char* message)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SetSafeWaitWindowText(%s)", message);
    ::SetSafeWaitWindowText(message);
}

BOOL CSalamanderGeneral::GetFileFromCache(const char* uniqueFileName, const char*& tmpName,
                                          HANDLE fileLock)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetFileFromCache(%s, ,)", uniqueFileName);
    tmpName = NULL;
    if (uniqueFileName == NULL || fileLock == NULL)
    {
        TRACE_E("Invalid parameter in CSalamanderGeneral::GetFileFromCache!");
        return FALSE;
    }

    BOOL fileExists;
    const char* name = DiskCache.GetName(uniqueFileName, NULL, &fileExists, FALSE, NULL, FALSE, NULL, NULL);
    if (name != NULL) // file found
    {
        if (!fileExists) // some do-gooder deleted it straight from the disk
        {
            // unable to prepare file, let disk cache know we are giving up
            DiskCache.ReleaseName(uniqueFileName, FALSE);
        }
        else
        {
            DiskCache.AssignName(uniqueFileName, fileLock, FALSE, crtCache);
            tmpName = name;
            return TRUE;
        }
    }
    return FALSE;
}

void CSalamanderGeneral::UnlockFileInCache(HANDLE fileLock)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::UnlockFileInCache(0x%p)", fileLock);

    SetEvent(fileLock); // start file cleanup
    DiskCache.WaitForIdle();
    ResetEvent(fileLock); // Finish cleaning up files
}

BOOL CSalamanderGeneral::MoveFileToCache(const char* uniqueFileName, const char* nameInCache,
                                         const char* rootTmpPath, const char* newFileName,
                                         const CQuadWord& newFileSize, BOOL* alreadyExists)
{
    CALL_STACK_MESSAGE6("CSalamanderGeneral::MoveFileToCache(%s, %s, %s, %s, %g, )",
                        uniqueFileName, nameInCache, rootTmpPath, newFileName, newFileSize.GetDouble());
    if (alreadyExists != NULL)
        *alreadyExists = FALSE;
    if (uniqueFileName == NULL || newFileName == NULL || nameInCache == NULL)
    {
        TRACE_E("Invalid parameter in CSalamanderGeneral::GetFileFromCache!");
        return FALSE;
    }

    // we verify if 'nameInCache' (name without path) was not entered incorrectly
    const char* s = nameInCache;
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (nameInCache[0] == 0 || *s != 0)
    {
        TRACE_E("Unexpected value of 'nameInCache' in CSalamanderGeneral::MoveFileToCache!");
        return FALSE;
    }

    // Add the file 'newFileName' to the disk cache under the name 'uniqueFileName'
    BOOL exists;
    const char* fileName = DiskCache.GetName(uniqueFileName, nameInCache, &exists, TRUE, rootTmpPath, FALSE, NULL, NULL);
    if (fileName == NULL) // error (if 'exists' is TRUE -> fatal, otherwise "file already exists")
    {
        if (alreadyExists != NULL)
            *alreadyExists = !exists;
        return FALSE;
    }

    if (!::SalMoveFile(newFileName, fileName))
    {
        DWORD err = GetLastError();
        TRACE_E("Unable to move file to disk cache! (error " << ::GetErrorText(err) << ")");
        DiskCache.ReleaseName(uniqueFileName, FALSE); // nothing to leave in cache
        return FALSE;
    }
    else // We managed to obtain the temporary file, we need to call NamePrepared()
    {
        DiskCache.NamePrepared(uniqueFileName, newFileSize);
        DiskCache.ReleaseName(uniqueFileName, TRUE); // Keep the file ready in cache (even if it is not locked)
        return TRUE;
    }
}

void CSalamanderGeneral::RemoveOneFileFromCache(const char* uniqueFileName)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::RemoveOneFileFromCache(%s)", uniqueFileName);
    if (uniqueFileName == NULL)
    {
        TRACE_E("Invalid parametr (NULL) in CSalamanderGeneral::RemoveOneFileFromCache!");
        return;
    }
    DiskCache.FlushOneFile(uniqueFileName);
}

void CSalamanderGeneral::RemoveFilesFromCache(const char* fileNamesRoot)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::RemoveFilesFromCache(%s)", fileNamesRoot);
    if (fileNamesRoot == NULL)
    {
        TRACE_E("Invalid parametr (NULL) in CSalamanderGeneral::RemoveFilesFromCache!");
        return;
    }
    DiskCache.FlushCache(fileNamesRoot);
}

BOOL CSalamanderGeneral::EnumConversionTables(HWND parent, int* index, const char** name, const char** table)
{
    if (index == NULL)
    {
        TRACE_E("Unexpected value of 'index' (NULL) in CSalamanderGeneral::EnumConversionTables().");
        return FALSE;
    }
    CALL_STACK_MESSAGE2("CSalamanderGeneral::EnumConversionTables(, %d, ,)", *index);
    parent = (parent == NULL ? MainWindow->HWindow : parent);
    return CodeTables.EnumCodeTables(parent, index, name, table);
}

BOOL CSalamanderGeneral::GetConversionTable(HWND parent, char* table, const char* conversion)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetConversionTable(, , %s)", conversion);
    if (table == NULL)
    {
        TRACE_E("Invalid parametr (table==NULL) in CSalamanderGeneral::GetConversionTable!");
        return FALSE;
    }
    parent = (parent == NULL ? MainWindow->HWindow : parent);
    BOOL ret = CodeTables.Init(parent);
    int codeType;
    if (ret)
        ret &= CodeTables.GetCodeType(conversion, codeType);
    if (ret)
        ret &= CodeTables.GetCode(table, codeType);
    return ret;
}

void CSalamanderGeneral::GetWindowsCodePage(HWND parent, char* codePage)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetWindowsCodePage(,)");
    parent = (parent == NULL ? MainWindow->HWindow : parent);
    CodeTables.Init(parent);
    CodeTables.GetWinCodePage(codePage);
}

void CSalamanderGeneral::RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                                           BOOL* isText, char* codePage)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::RecognizeFileType(, , %d, %d, ,)", patternLen, forceText);
    parent = (parent == NULL ? MainWindow->HWindow : parent);
    ::RecognizeFileType(parent, pattern, patternLen, forceText, isText, codePage);
}

BOOL CSalamanderGeneral::IsANSIText(const char* text, int textLen)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::IsANSIText(, %d)", textLen);

    const unsigned char* s = (const unsigned char*)text;
    const unsigned char* end = s + textLen;
    while (s < end)
    {
        if (*s < ' ' && *s != '\a' && *s != '\b' && *s != '\r' && // *s != 0 must not be here because of Unicode files ("0A 00" cannot be expanded to "0D 0A 00")
            *s != '\f' && *s != '\n' && *s != '\t' && *s != '\v' &&
            *s != '\x1a' && *s != '\x04' && *s != '\x06')
        { // illegal character
            break;
        }
        s++;
    }
    return s == end;
}

BOOL CSalamanderGeneral::SalMoveFile(const char* srcName, const char* destName, DWORD* err)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::SalMoveFile(%s, %s,)", srcName, destName);
    BOOL ret = ::SalMoveFile(srcName, destName);
    if (err != NULL)
        *err = GetLastError();
    return ret;
}

BOOL CSalamanderGeneral::SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalGetFileSize(, ,)");
    return ::SalGetFileSize(file, size, err);
}

BOOL CSalamanderGeneral::GetTargetDirectory(HWND parent, HWND hCenterWindow, const char* title,
                                            const char* comment, char* path, BOOL onlyNet,
                                            const char* initDir)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::GetTargetDirectory(, , %s, %s, , %d, %s)",
                        title, comment, onlyNet, initDir);
    return ::GetTargetDirectory(parent, hCenterWindow, title, comment, path, onlyNet, initDir);
}

void CSalamanderGeneral::CallPluginOperationFromDisk(int panel, SalPluginOperationFromDisk callback,
                                                     void* param)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::CallPluginOperationFromDisk(%d, ,)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::CallPluginOperationFromDisk() only from main thread!");
        return;
    }
    if (callback == NULL)
    {
        TRACE_E("Unexpected value of parameter 'callback' (NULL) in CSalamanderGeneral::CallPluginOperationFromDisk().");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        if (!p->Is(ptDisk))
        {
            TRACE_E("CSalamanderGeneral::CallPluginOperationFromDisk(): there must be windows (disk) path in panel!");
            return;
        }
        if (p->Files->Count + p->Dirs->Count <= 0)
        {
            TRACE_I("CSalamanderGeneral::CallPluginOperationFromDisk(): no items in panel!");
            return;
        }
        // prepare data for enumerating files and directories from the panel
        CPanelTmpEnumData data;
        int oneIndex = -1;
        int count = p->GetSelCount();
        if (count > 0) // some files are marked
        {
            data.IndexesCount = count;
            data.Indexes = new int[count];
            if (data.Indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                return;
            }
            else
                p->GetSelItems(count, data.Indexes);
        }
        else // we take focus
        {
            oneIndex = p->GetCaretIndex();

            BOOL subDir;
            if (p->Dirs->Count > 0)
                subDir = (strcmp(p->Dirs->At(0).Name, "..") == 0);
            else
                subDir = FALSE;
            if (oneIndex == 0 && subDir)
            {
                TRACE_E("Unexpected situation in CSalamanderGeneral::CallPluginOperationFromDisk(): no files nor directories selected and focus is on up-dir symbol.");
                return;
            }

            data.IndexesCount = 1;
            data.Indexes = &oneIndex; // not deallocated
        }
        data.CurrentIndex = 0;
        data.ZIPPath = p->GetZIPPath();
        data.Dirs = p->Dirs;
        data.Files = p->Files;
        data.ArchiveDir = p->GetArchiveDir();
        lstrcpyn(data.WorkPath, p->GetPath(), MAX_PATH);
        data.EnumLastDir = NULL;
        data.EnumLastIndex = -1;

        callback(p->GetPath(), PanelEnumDiskSelection, &data, param);

        if (count > 0)
            delete[] (data.Indexes);
    }
}

BYTE CSalamanderGeneral::GetUserDefaultCharset()
{
    return (BYTE)UserCharset;
}

class CSalamanderBMSearchDataImp : public CSalamanderBMSearchData
{
protected:
    CSearchData Moore;

public:
    CSalamanderBMSearchDataImp() : Moore() {}

    virtual void WINAPI Set(const char* pattern, WORD flags) { Moore.Set(pattern, flags); }
    virtual void WINAPI Set(const char* pattern, const int length, WORD flags) { Moore.Set(pattern, length, flags); }
    virtual void WINAPI SetFlags(WORD flags) { Moore.SetFlags(flags); }
    virtual int WINAPI GetLength() const { return Moore.GetLength(); }
    virtual const char* WINAPI GetPattern() const { return Moore.GetPattern(); }
    virtual BOOL WINAPI IsGood() const { return Moore.IsGood(); }
    virtual int WINAPI SearchForward(const char* text, int length, int start) { return Moore.SearchForward(text, length, start); }
    virtual int WINAPI SearchBackward(const char* text, int length) { return Moore.SearchBackward(text, length); }
};

CSalamanderBMSearchData*
CSalamanderGeneral::AllocSalamanderBMSearchData()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::AllocSalamanderBMSearchData()");
    CSalamanderBMSearchData* ret = new CSalamanderBMSearchDataImp;
    if (ret == NULL)
        TRACE_E(LOW_MEMORY);
    return ret;
}

void CSalamanderGeneral::FreeSalamanderBMSearchData(CSalamanderBMSearchData* data)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::FreeSalamanderBMSearchData()");
    if (data != NULL)
        delete ((CSalamanderBMSearchDataImp*)data);
}

class CSalamanderREGEXPSearchDataImp : public CSalamanderREGEXPSearchData
{
protected:
    CRegularExpression REGEXP;

public:
    CSalamanderREGEXPSearchDataImp() : REGEXP() {}

    virtual BOOL WINAPI Set(const char* pattern, WORD flags) { return REGEXP.Set(pattern, flags); }
    virtual BOOL WINAPI SetFlags(WORD flags) { return REGEXP.SetFlags(flags); }
    virtual const char* WINAPI GetLastErrorText() const { return REGEXP.GetLastErrorText(); }
    virtual const char* WINAPI GetPattern() const { return REGEXP.GetPattern(); }
    virtual BOOL WINAPI SetLine(const char* start, const char* end)
    {
        REGEXP.SetLine(start, end);
        return TRUE;
    }
    virtual int WINAPI SearchForward(int start, int& foundLen) { return REGEXP.SearchForward(start, foundLen); }
    virtual int WINAPI SearchBackward(int length, int& foundLen) { return REGEXP.SearchBackward(length, foundLen); }
};

CSalamanderREGEXPSearchData*
CSalamanderGeneral::AllocSalamanderREGEXPSearchData()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::AllocSalamanderREGEXPSearchData()");
    CSalamanderREGEXPSearchData* ret = new CSalamanderREGEXPSearchDataImp;
    return ret;
}

void CSalamanderGeneral::FreeSalamanderREGEXPSearchData(CSalamanderREGEXPSearchData* data)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::FreeSalamanderREGEXPSearchData()");
    if (data != NULL)
        delete ((CSalamanderREGEXPSearchDataImp*)data);
}

struct CSalCommandsAux
{
    int SalCmd;
    int Cmd;
    int TextID;
    DWORD* Enabled; // NULL == "always TRUE"
    int Type;
};

CSalCommandsAux SalCommandsArray[] = // end of item with 'SalCmd' == -1
    {
        {SALCMD_VIEW, CM_VIEW, IDS_MENU_FILES_VIEW, &EnablerViewFile, sctyForFocusedFile},
        {SALCMD_ALTVIEW, CM_ALTVIEW, IDS_MENU_FILES_ALTVIEW, &EnablerViewFile, sctyForFocusedFile},
        {SALCMD_VIEWWITH, CM_VIEW_WITH, IDS_SALCMD_VIEWWITH, &EnablerViewFile, sctyForFocusedFile},
        {SALCMD_EDIT, CM_EDIT, IDS_MENU_FILES_EDIT, &EnablerFileOnDiskOrArchive, sctyForFocusedFile},
        {SALCMD_EDITWITH, CM_EDIT_WITH, IDS_SALCMD_EDITWITH, &EnablerFileOnDiskOrArchive, sctyForFocusedFile},

        {SALCMD_OPEN, CM_OPEN, IDS_SALCMD_OPEN, NULL, sctyForFocusedFileOrDirectory},
        {SALCMD_QUICKRENAME, CM_RENAMEFILE, IDS_MENU_FILES_RENAME, &EnablerQuickRename, sctyForFocusedFileOrDirectory},

        {SALCMD_COPY, CM_COPYFILES, IDS_MENU_FILES_COPY, &EnablerFilesCopy, sctyForSelectedFilesAndDirectories},
        {SALCMD_MOVE, CM_MOVEFILES, IDS_MENU_FILES_MOVE, &EnablerFilesMove, sctyForSelectedFilesAndDirectories},
        {SALCMD_EMAIL, CM_EMAILFILES, IDS_MENU_FILES_EMAIL, &EnablerFilesOnDisk, sctyForSelectedFilesAndDirectories},
        {SALCMD_DELETE, CM_DELETEFILES, IDS_MENU_FILES_DELETE, &EnablerFilesDelete, sctyForSelectedFilesAndDirectories},
        {SALCMD_PROPERTIES, CM_PROPERTIES, IDS_MENU_FILES_PROPERTIES, &EnablerShowProperties, sctyForSelectedFilesAndDirectories},
        {SALCMD_CHANGECASE, CM_CHANGECASE, IDS_MENU_FILES_CHANGECASE, &EnablerFilesOnDisk, sctyForSelectedFilesAndDirectories},
        {SALCMD_CHANGEATTRS, CM_CHANGEATTR, IDS_MENU_FILES_CHANGEATTR, &EnablerChangeAttrs, sctyForSelectedFilesAndDirectories},
        {SALCMD_OCCUPIEDSPACE, CM_OCCUPIEDSPACE, IDS_MENU_CMD_OCCUPIED, &EnablerOccupiedSpace, sctyForSelectedFilesAndDirectories},

        {SALCMD_EDITNEWFILE, CM_EDITNEW, IDS_MENU_FILES_EDITNEW, &EnablerOnDisk, sctyForCurrentPath},
        {SALCMD_REFRESH, CM_ACTIVEREFRESH, IDS_MENU_LEFT_REFRESH, NULL, sctyForCurrentPath},
        {SALCMD_CREATEDIRECTORY, CM_CREATEDIR, IDS_MENU_CMD_CREATEDIR, &EnablerCreateDir, sctyForCurrentPath},
        {SALCMD_DRIVEINFO, CM_DRIVEINFO, IDS_MENU_CMD_DRIVEINFO, &EnablerDriveInfo, sctyForCurrentPath},
        {SALCMD_CALCDIRSIZES, CM_CALCDIRSIZES, IDS_MENU_CMD_CALCDIRSIZES, &EnablerCalcDirSizes, sctyForCurrentPath},

        {SALCMD_DISCONNECT, CM_DISCONNECTNET, IDS_MENU_CMD_DISCONNECTNET, NULL, sctyForConnectedDrivesAndFS},

        {-1, -1, -1, NULL, sctyUnknown} // terminator
};

int GetWMCommandFromSalCmd(int salCmd)
{
    int index = 0;
    while (SalCommandsArray[index].SalCmd != -1)
    {
        if (SalCommandsArray[index].SalCmd == salCmd)
            return SalCommandsArray[index].Cmd;
        index++;
    }
    TRACE_E("You have used CSalamanderGeneral::PostSalamanderCommand for invalid command (" << salCmd << ")!");
    return -1;
}

BOOL CSalamanderGeneral::GetSalamanderCommand(int salCmd, char* nameBuf, int nameBufSize, BOOL* enabled,
                                              int* type)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::GetSalamanderCommand(%d, , %d, ,)", salCmd, nameBufSize);
    int index = 0;
    while (SalCommandsArray[index].SalCmd != -1)
    {
        if (SalCommandsArray[index].SalCmd == salCmd) // found
        {
            // It is necessary to calculate the states of commands, the SalCommandsArray array uses states
            MainWindow->OnEnterIdle();

            if (nameBuf != NULL && nameBufSize > 0)
            {
                lstrcpyn(nameBuf, ::LoadStr(SalCommandsArray[index].TextID), nameBufSize);
            }
            if (SalCommandsArray[index].Enabled != NULL)
            {
                if (enabled != NULL)
                    *enabled = *(SalCommandsArray[index].Enabled);
            }
            if (type != NULL)
                *type = SalCommandsArray[index].Type;

            return TRUE;
        }
        index++;
    }
    return FALSE;
}

BOOL CSalamanderGeneral::EnumSalamanderCommands(int* index, int* salCmd, char* nameBuf, int nameBufSize,
                                                BOOL* enabled, int* type)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::EnumSalamanderCommands(, , , %d, ,)", nameBufSize);
    if (salCmd != NULL)
        *salCmd = -1;
    if (nameBuf != NULL && nameBufSize > 0)
        nameBuf[0] = 0;
    if (enabled != NULL)
        *enabled = TRUE;
    if (type != NULL)
        *type = sctyUnknown;

    if (index != NULL && *index >= 0 && SalCommandsArray[*index].SalCmd != -1)
    {
        if (*index == 0)
        {
            // It is necessary to calculate the states of commands, the SalCommandsArray array uses states
            MainWindow->OnEnterIdle();
        }

        if (salCmd != NULL)
            *salCmd = SalCommandsArray[*index].SalCmd;
        if (nameBuf != NULL && nameBufSize > 0)
        {
            lstrcpyn(nameBuf, ::LoadStr(SalCommandsArray[*index].TextID), nameBufSize);
        }
        if (SalCommandsArray[*index].Enabled != NULL)
        {
            if (enabled != NULL)
                *enabled = *(SalCommandsArray[*index].Enabled);
        }
        if (type != NULL)
            *type = SalCommandsArray[*index].Type;

        (*index)++;
        return TRUE;
    }
    return FALSE;
}

void CSalamanderGeneral::PostSalamanderCommand(int salCmd)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::PostSalamanderCommand(%d)", salCmd);
    if (salCmd < 0 || salCmd >= 500)
    {
        TRACE_E("CSalamanderGeneral::PostSalamanderCommand: salCmd is invalid (" << salCmd << " is not in range 0-499).");
        return;
    }

    if (MainThreadID == GetCurrentThreadId())
    { // due to a call from the entry-point, where the Plugin is set to -1 (only for retrieving plugin data)
        // Before WM_USER_POSTCMDORUNLOADPLUGIN is reached, the Plugin would have been reset (according to the return value of the entry point)
        CPluginData* data = Plugins.GetPluginData(Plugin);
        if (data != NULL)
        {
            data->Commands.Add(salCmd);
            ExecCmdsOrUnloadMarkedPlugins = TRUE;
        }
        else
        {
            TRACE_E("Unexpected situation in CSalamanderGeneral::PostSalamanderCommand().");
        }
    }
    else // Plugin is certainly set outside the entry-point...
    {
        if (MainWindow != NULL && MainWindow->HWindow != NULL)
        {
            // test for calling during entry-point execution (Plugin is set to -1)
            if ((INT_PTR)Plugin == -1)
            {
                TRACE_E("You can call CSalamanderGeneral::PostSalamanderCommand only from main "
                        "thread when plugin entry-point is not finished yet!");
            }
            else
            { // 0 - unload, 1 - rebuild menu, 2-501 salCmd, 502-1000501 menuCmd
                PostMessage(MainWindow->HWindow, WM_USER_POSTCMDORUNLOADPLUGIN, (WPARAM)Plugin, 2 + salCmd);
            }
        }
        else
        {
            TRACE_E("Unexpected situation (2) in CSalamanderGeneral::PostSalamanderCommand().");
        }
    }
}

void CSalamanderGeneral::SetUserWorkedOnPanelPath(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SetUserWorkedOnPanelPath(%d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SetUserWorkedOnPanelPath() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
        p->UserWorkedOnThisPath = TRUE;
}

void CSalamanderGeneral::StoreSelectionOnPanelPath(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::StoreSelectionOnPanelPath(%d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::StoreSelectionOnPanelPath() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
        p->StoreSelection();
}

class CSalamanderMaskGroupImp : public CSalamanderMaskGroup
{
protected:
    CMaskGroup maskGroup;

public:
    CSalamanderMaskGroupImp() : maskGroup() {}

    virtual void WINAPI SetMasksString(const char* masks, BOOL extendedMode) { maskGroup.SetMasksString(masks, extendedMode); }
    virtual void WINAPI GetMasksString(char* buffer) { lstrcpyn(buffer, maskGroup.GetMasksString(), MAX_GROUPMASK); }
    virtual BOOL WINAPI GetExtendedMode() { return maskGroup.GetExtendedMode(); }
    virtual BOOL WINAPI PrepareMasks(int& errorPos) { return maskGroup.PrepareMasks(errorPos); }
    virtual BOOL WINAPI AgreeMasks(const char* fileName, const char* fileExt) { return maskGroup.AgreeMasks(fileName, fileExt); }
};

CSalamanderMaskGroup*
CSalamanderGeneral::AllocSalamanderMaskGroup()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::AllocSalamanderMaskGroup()");
    CSalamanderMaskGroup* ret = new CSalamanderMaskGroupImp;
    if (ret == NULL)
        TRACE_E(LOW_MEMORY);
    return ret;
}

void CSalamanderGeneral::FreeSalamanderMaskGroup(CSalamanderMaskGroup* maskGroup)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::FreeSalamanderMaskGroup()");
    if (maskGroup != NULL)
        delete ((CSalamanderMaskGroupImp*)maskGroup);
}

DWORD
CSalamanderGeneral::UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal)
{
    CALL_STACK_MESSAGE_NONE
    return ::UpdateCrc32(buffer, count, crcVal);
}

class CSalamanderMD5Imp : public CSalamanderMD5
{
protected:
    MD5 md5;

public:
    CSalamanderMD5Imp() : md5() {}

    virtual void WINAPI Init() { md5.init(); }
    virtual void WINAPI Update(const void* input, DWORD input_length) { md5.update((unsigned char*)input, input_length); }
    virtual void WINAPI Finalize() { md5.finalize(); }
    virtual void WINAPI GetDigest(void* dest) { memcpy(dest, md5.digest, 16); }
};

CSalamanderMD5*
CSalamanderGeneral::AllocSalamanderMD5()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::AllocSalamanderMD5()");
    CSalamanderMD5Imp* ret = new CSalamanderMD5Imp;
    if (ret == NULL)
        TRACE_E(LOW_MEMORY);
    return ret;
}

void CSalamanderGeneral::FreeSalamanderMD5(CSalamanderMD5* md5)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::FreeSalamanderMD5()");
    if (md5 != NULL)
        delete ((CSalamanderMD5Imp*)md5);
}

BOOL CSalamanderGeneral::LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount)
{
    CALL_STACK_MESSAGE_NONE
    return ::LookForSubTexts(text, varPlacements, varPlacementsCount);
}

void CSalamanderGeneral::WaitForESCRelease()
{
    CALL_STACK_MESSAGE_NONE
    ::WaitForESCRelease();
}

DWORD
CSalamanderGeneral::GetMouseWheelScrollLines()
{
    CALL_STACK_MESSAGE_NONE
    return ::GetMouseWheelScrollLines();
}

DWORD
CSalamanderGeneral::GetMouseWheelScrollChars()
{
    CALL_STACK_MESSAGE_NONE
    return ::GetMouseWheelScrollChars();
}

HWND CSalamanderGeneral::GetTopVisibleParent(HWND hParent)
{
    CALL_STACK_MESSAGE_NONE
    return ::GetTopVisibleParent(hParent);
}

BOOL CSalamanderGeneral::MultiMonGetDefaultWindowPos(HWND hByWnd, POINT* p)
{
    CALL_STACK_MESSAGE_NONE
    return ::MultiMonGetDefaultWindowPos(hByWnd, p);
}

void CSalamanderGeneral::MultiMonGetClipRectByRect(const RECT* rect, RECT* workClipRect, RECT* monitorClipRect)
{
    CALL_STACK_MESSAGE_NONE
    ::MultiMonGetClipRectByRect(rect, workClipRect, monitorClipRect);
}

void CSalamanderGeneral::MultiMonGetClipRectByWindow(HWND hByWnd, RECT* workClipRect, RECT* monitorClipRect)
{
    CALL_STACK_MESSAGE_NONE
    ::MultiMonGetClipRectByWindow(hByWnd, workClipRect, monitorClipRect);
}

void CSalamanderGeneral::MultiMonCenterWindow(HWND hWindow, HWND hByWnd, BOOL findTopWindow)
{
    CALL_STACK_MESSAGE_NONE
    ::MultiMonCenterWindow(hWindow, hByWnd, findTopWindow);
}

BOOL CSalamanderGeneral::MultiMonEnsureRectVisible(RECT* rect, BOOL partialOK)
{
    CALL_STACK_MESSAGE_NONE
    return ::MultiMonEnsureRectVisible(rect, partialOK);
}

BOOL CSalamanderGeneral::InstallWordBreakProc(HWND hWindow)
{
    CALL_STACK_MESSAGE_NONE
    return ::InstallWordBreakProc(hWindow);
}

BOOL CSalamanderGeneral::IsFirstInstance3OrLater()
{
    CALL_STACK_MESSAGE_NONE
    return FirstInstance_3_or_later;
}

int CSalamanderGeneral::ExpandPluralString(char* buffer, int bufferSize, const char* format,
                                           int parametersCount, const CQuadWord* parametersArray)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::ExpandPluralString(, %d, %s, %d, )",
                        bufferSize, format, parametersCount);
    return ::ExpandPluralString(buffer, bufferSize, format, parametersCount, parametersArray);
}

int CSalamanderGeneral::ExpandPluralFilesDirs(char* buffer, int bufferSize, int files, int dirs,
                                              int mode, BOOL forDlgCaption)
{
    CALL_STACK_MESSAGE6("CSalamanderGeneral::ExpandPluralFilesDirs(, %d, %d, %d, %d, %d)",
                        bufferSize, files, dirs, (int)mode, forDlgCaption);
    return ::ExpandPluralFilesDirs(buffer, bufferSize, files, dirs, mode, forDlgCaption);
}

int CSalamanderGeneral::ExpandPluralBytesFilesDirs(char* buffer, int bufferSize,
                                                   const CQuadWord& selectedBytes, int files, int dirs,
                                                   BOOL useSubTexts)
{
    CALL_STACK_MESSAGE6("CSalamanderGeneral::FreeSalamanderMaskGroup(, %d, %g, %d, %d, %d)",
                        bufferSize, selectedBytes.GetDouble(), files, dirs, useSubTexts);
    return ::ExpandPluralBytesFilesDirs(buffer, bufferSize, selectedBytes, files, dirs, useSubTexts);
}

void CSalamanderGeneral::GetCommonFSOperSourceDescr(char* sourceDescr, int sourceDescrSize,
                                                    int panel, int selectedFiles, int selectedDirs,
                                                    const char* fileOrDirName, BOOL isDir,
                                                    BOOL forDlgCaption)
{
    CALL_STACK_MESSAGE8("CSalamanderGeneral::GetCommonFSOperSourceDescr(, %d, %d, %d, %d, %s, %d, %d)",
                        sourceDescrSize, panel, selectedFiles, selectedDirs, fileOrDirName,
                        isDir, forDlgCaption);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetCommonFSOperSourceDescr() only from main thread!");
        return;
    }
    if (sourceDescrSize <= 0)
        return;
    if (sourceDescr == NULL)
    {
        TRACE_E("CSalamanderGeneral::GetCommonFSOperSourceDescr(): 'sourceDescr' may not be NULL!");
        return;
    }
    if (selectedFiles + selectedDirs <= 1 && panel == -1 && fileOrDirName == NULL)
    {
        TRACE_E("CSalamanderGeneral::GetCommonFSOperSourceDescr(): 'fileOrDirName' may not be NULL!");
        sourceDescr[0] = 0;
        return;
    }
    if (selectedFiles + selectedDirs <= 1) // one marked item or focus
    {
        BOOL nameIsDir;
        char* name;
        char nameBuf[MAX_PATH];
        if (panel != -1)
        {
            const CFileData* f;
            if (selectedFiles == 0 && selectedDirs == 0)
                f = GetPanelFocusedItem(panel, &nameIsDir);
            else
            {
                int index = 0;
                f = GetPanelSelectedItem(panel, &index, &nameIsDir);
            }
            if (f != NULL && f->Name != NULL)
                name = f->Name;
            else
            {
                TRACE_E("Unexpected situation in CSalamanderGeneral::GetCommonFSOperSourceDescr()!");
                sourceDescr[0] = 0;
                return;
            }
        }
        else
        {
            lstrcpyn(nameBuf, fileOrDirName, MAX_PATH);
            name = nameBuf;
            nameIsDir = isDir;
        }
        int fileNameFormat;
        GetConfigParameter(SALCFG_FILENAMEFORMAT, &fileNameFormat,
                           sizeof(fileNameFormat), NULL);
        char formatedFileName[MAX_PATH]; // CFileData::Name is long max. MAX_PATH-5 characters - limit given by Salamander
        ::AlterFileName(formatedFileName, name, -1, fileNameFormat, 0, nameIsDir);
        _snprintf_s(sourceDescr, sourceDescrSize, _TRUNCATE,
                    ::LoadStr(nameIsDir ? (forDlgCaption ? IDS_DLG_QUESTION_DIRECTORY : IDS_QUESTION_DIRECTORY) : (forDlgCaption ? IDS_DLG_QUESTION_FILE : IDS_QUESTION_FILE)),
                    formatedFileName);
    }
    else // several directories and files
    {
        ExpandPluralFilesDirs(sourceDescr, sourceDescrSize, selectedFiles, selectedDirs,
                              epfdmNormal, forDlgCaption);
    }
}

void CSalamanderGeneral::AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::AddStrToStr(, ,)");
    if (dstBufSize < 2)
    {
        TRACE_E("CSalamanderGeneral::AddStrToStr(): dstBufSize must be greater or equal to 2");
        return;
    }
    ::AddStrToStr(dstStr, dstBufSize, srcStr);
}

BOOL CSalamanderGeneral::SalIsValidFileNameComponent(const char* fileNameComponent)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalIsValidFileNameComponent()");
    return ::SalIsValidFileNameComponent(fileNameComponent);
}

void CSalamanderGeneral::SalMakeValidFileNameComponent(char* fileNameComponent)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalMakeValidFileNameComponent()");
    ::SalMakeValidFileNameComponent(fileNameComponent);
}

BOOL CSalamanderGeneral::IsFileEnumSourcePanel(int srcUID, int* panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::IsFileEnumSourcePanel(%d,)", srcUID);
    return ::IsFileEnumSourcePanel(srcUID, panel);
}

BOOL CSalamanderGeneral::GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                  BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                  char* fileName, BOOL* noMoreFiles, BOOL* srcBusy)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::GetNextFileNameForViewer(%d, , , %d, %d, %s, ,)",
                        srcUID, preferSelected, onlyAssociatedExtensions, fileName);
    if (fileName == NULL || lastFileIndex == NULL)
    {
        if (noMoreFiles != NULL)
            *noMoreFiles = FALSE;
        if (srcBusy != NULL)
            *srcBusy = FALSE;
        TRACE_E("CSalamanderGeneral::GetNextFileNameForViewer(): invalid parameters (fileName == NULL || lastFileIndex == NULL)!");
        return FALSE;
    }
    if (Plugin == NULL || (INT_PTR)Plugin == -1)
    {
        if (noMoreFiles != NULL)
            *noMoreFiles = FALSE;
        if (srcBusy != NULL)
            *srcBusy = FALSE;
        TRACE_E("CSalamanderGeneral::GetNextFileNameForViewer(): unexpected call, plugin is not initialized yet!");
        return FALSE;
    }
    return ::GetNextFileNameForViewer(srcUID, lastFileIndex, lastFileName, preferSelected,
                                      onlyAssociatedExtensions, fileName,
                                      noMoreFiles, srcBusy, Plugin);
}

BOOL CSalamanderGeneral::GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                      BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                      char* fileName, BOOL* noMoreFiles, BOOL* srcBusy)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::GetPreviousFileNameForViewer(%d, , , %d, %d, %s, ,)",
                        srcUID, preferSelected, onlyAssociatedExtensions, fileName);
    if (fileName == NULL || lastFileIndex == NULL)
    {
        if (noMoreFiles != NULL)
            *noMoreFiles = FALSE;
        if (srcBusy != NULL)
            *srcBusy = FALSE;
        TRACE_E("CSalamanderGeneral::GetPreviousFileNameForViewer(): invalid parameters (fileName == NULL || lastFileIndex == NULL)!");
        return FALSE;
    }
    if (Plugin == NULL || (INT_PTR)Plugin == -1)
    {
        if (noMoreFiles != NULL)
            *noMoreFiles = FALSE;
        if (srcBusy != NULL)
            *srcBusy = FALSE;
        TRACE_E("CSalamanderGeneral::GetPreviousFileNameForViewer(): unexpected call, plugin is not initialized yet!");
        return FALSE;
    }
    return ::GetPreviousFileNameForViewer(srcUID, lastFileIndex, lastFileName, preferSelected,
                                          onlyAssociatedExtensions, fileName,
                                          noMoreFiles, srcBusy, Plugin);
}

BOOL CSalamanderGeneral::IsFileNameForViewerSelected(int srcUID, int lastFileIndex,
                                                     const char* lastFileName,
                                                     BOOL* isFileSelected, BOOL* srcBusy)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::IsFileNameForViewerSelected(%d, %d, , , ,)",
                        srcUID, lastFileIndex);
    if (isFileSelected == NULL)
    {
        if (srcBusy != NULL)
            *srcBusy = FALSE;
        TRACE_E("CSalamanderGeneral::IsFileNameForViewerSelected(): invalid parameters (isFileSelected == NULL)!");
        return FALSE;
    }
    return ::IsFileNameForViewerSelected(srcUID, lastFileIndex, lastFileName,
                                         isFileSelected, srcBusy);
}

BOOL CSalamanderGeneral::SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex,
                                                         const char* lastFileName, BOOL select,
                                                         BOOL* srcBusy)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::SetSelectionOnFileNameForViewer(%d, %d, , %d,)",
                        srcUID, lastFileIndex, select);
    return ::SetSelectionOnFileNameForViewer(srcUID, lastFileIndex, lastFileName, select, srcBusy);
}

BOOL CSalamanderGeneral::GetStdHistoryValues(int historyID, char*** historyArr, int* historyItemsCount)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetStdHistoryValues(%d, ,)", historyID);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetStdHistoryValues() only from main thread!");
        if (historyArr != NULL)
            *historyArr = NULL;
        if (historyItemsCount != NULL)
            *historyItemsCount = 0;
        return FALSE;
    }
    if (historyArr == NULL || historyItemsCount == NULL)
    {
        TRACE_E("CSalamanderGeneral::GetStdHistoryValues(): invalid parameters!");
        return FALSE;
    }
    switch (historyID)
    {
    case SALHIST_COPYMOVETGT:
    {
        *historyArr = Configuration.CopyHistory;
        *historyItemsCount = COPY_HISTORY_SIZE;
        return TRUE;
    }

    case SALHIST_CREATEDIR:
    {
        *historyArr = Configuration.CreateDirHistory;
        *historyItemsCount = CREATEDIR_HISTORY_SIZE;
        return TRUE;
    }

    case SALHIST_CHANGEDIR:
    {
        *historyArr = Configuration.ChangeDirHistory;
        *historyItemsCount = CHANGEDIR_HISTORY_SIZE;
        return TRUE;
    }

    case SALHIST_QUICKRENAME:
    {
        *historyArr = Configuration.QuickRenameHistory;
        *historyItemsCount = QUICKRENAME_HISTORY_SIZE;
        return TRUE;
    }

    case SALHIST_EDITNEW:
    {
        *historyArr = Configuration.EditNewHistory;
        *historyItemsCount = EDITNEW_HISTORY_SIZE;
        return TRUE;
    }

    case SALHIST_CONVERT:
    {
        *historyArr = Configuration.ConvertHistory;
        *historyItemsCount = CONVERT_HISTORY_SIZE;
        return TRUE;
    }

    default:
    {
        *historyArr = NULL;
        *historyItemsCount = 0;
        return FALSE;
    }
    }
}

void CSalamanderGeneral::AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                                    const char* value, BOOL caseSensitiveValue)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::AddValueToStdHistoryValues(, %d, %s, %d)",
                        historyItemsCount, value, caseSensitiveValue);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::AddValueToStdHistoryValues() only from main thread!");
        return;
    }
    if (historyArr == NULL || value == NULL)
    {
        TRACE_E("CSalamanderGeneral::AddValueToStdHistoryValues(): 'historyArr' and 'value' may not be NULL!");
        return;
    }
    ::AddValueToStdHistoryValues(historyArr, historyItemsCount, value, caseSensitiveValue);
}

void CSalamanderGeneral::LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::LoadComboFromStdHistoryValues(, , %d)",
                        historyItemsCount);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::LoadComboFromStdHistoryValues() only from main thread!");
        return;
    }
    if (historyArr == NULL)
    {
        TRACE_E("CSalamanderGeneral::LoadComboFromStdHistoryValues(): 'historyArr' may not be NULL!");
        return;
    }
    ::LoadComboFromStdHistoryValues(combo, historyArr, historyItemsCount);
}

BOOL CSalamanderGeneral::CanUse256ColorsBitmap()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::CanUse256ColorsBitmap()");
    return ::Use256ColorsBitmap();
}

HWND CSalamanderGeneral::GetWndToFlash(HWND parent)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetWndToFlash()");
    return ::GetWndToFlash(parent);
}

void ActivateDropTarget(HWND dropTarget, HWND progressWnd)
{
    if (dropTarget != NULL)
    { // this mess will get us out of the activated state without an active application (visually inactive, but WM_ACTIVATEAPP with "activate" has already arrived)
        HWND tgtWnd = dropTarget;
        HWND tmp;
        while ((tmp = ::GetParent(tgtWnd)) != NULL && IsWindowEnabled(tmp))
            tgtWnd = tmp;
        if (MainWindow != NULL && tgtWnd != MainWindow->HWindow)
        { // Execute only if it is not an operation inside our Salamander
            SetForegroundWindow(progressWnd);
            SetForegroundWindow(tgtWnd);
            //        TRACE_I("SetForegroundWindow: " << hex << tgtWnd);
        }
    }
}

void CSalamanderGeneral::ActivateDropTarget(HWND dropTarget, HWND progressWnd)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::ActivateDropTarget(,)");
    ::ActivateDropTarget(dropTarget, progressWnd);
}

void CSalamanderGeneral::PostOpenPackDlgForThisPlugin(int delFilesAfterPacking)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::PostOpenPackDlgForThisPlugin(%d)", delFilesAfterPacking);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::PostOpenPackDlgForThisPlugin() only from main thread!");
        return;
    }

    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
    {
        data->OpenPackDlg = TRUE;
        data->PackDlgDelFilesAfterPacking = delFilesAfterPacking;
        OpenPackOrUnpackDlgForMarkedPlugins = TRUE;
    }
    else
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::PostOpenPackDlgForThisPlugin().");
    }
}

void CSalamanderGeneral::PostOpenUnpackDlgForThisPlugin(const char* unpackMask)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::PostOpenUnpackDlgForThisPlugin(%s)", unpackMask);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::PostOpenUnpackDlgForThisPlugin() only from main thread!");
        return;
    }

    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
    {
        data->OpenUnpackDlg = TRUE;
        if (data->UnpackDlgUnpackMask != NULL)
            free(data->UnpackDlgUnpackMask);
        if (unpackMask != NULL)
            data->UnpackDlgUnpackMask = DupStr(unpackMask);
        else
            data->UnpackDlgUnpackMask = NULL;
        OpenPackOrUnpackDlgForMarkedPlugins = TRUE;
    }
    else
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::PostOpenUnpackDlgForThisPlugin().");
    }
}

HANDLE
CSalamanderGeneral::SalCreateFileEx(const char* fileName, DWORD desiredAccess, DWORD shareMode,
                                    DWORD flagsAndAttributes, DWORD* err)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalCreateFileEx()");
    HANDLE ret = ::SalCreateFileEx(fileName, desiredAccess, shareMode, flagsAndAttributes, NULL);
    if (err != NULL)
        *err = GetLastError();
    return ret;
}

BOOL CSalamanderGeneral::SalCreateDirectoryEx(const char* name, DWORD* err)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalCreateDirectoryEx()");
    return ::SalCreateDirectoryEx(name, err);
}

void CSalamanderGeneral::PanelStopMonitoring(int panel, BOOL stopMonitoring)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::PanelStopMonitoring(%d, %d)", panel, stopMonitoring);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::PanelStopMonitoring() only from main thread!");
        return;
    }
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
        p->HandsOff(stopMonitoring);
}

CSalamanderDirectoryAbstract*
CSalamanderGeneral::AllocSalamanderDirectory(BOOL isForFS)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::AllocSalamanderDirectory(%d)", isForFS);
    CSalamanderDirectory* ret = new CSalamanderDirectory(isForFS);
    if (ret == NULL)
        TRACE_E(LOW_MEMORY);
    else
        ret->AllocAddCache();
    return ret;
}

void CSalamanderGeneral::FreeSalamanderDirectory(CSalamanderDirectoryAbstract* salDir)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::FreeSalamanderDirectory()");
    if (salDir != NULL)
        delete ((CSalamanderDirectory*)salDir);
}

BOOL CSalamanderGeneral::AddPluginFSTimer(int timeout, CPluginFSInterfaceAbstract* timerOwner,
                                          DWORD timerParam) // FIXME_X64 - Go through the Salamander interface to check if we are not passing any parameters that should be able to hold x64 pointers; for example, 'timerParam' here
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::AddPluginFSTimer(%d, , 0x%X)", timeout, timerParam);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::AddPluginFSTimer() only from main thread!");
        return FALSE;
    }
    if (timeout < 0)
    {
        TRACE_E("CSalamanderGeneral::AddPluginFSTimer(): invalid timeout value (" << timeout << ")!");
        return FALSE;
    }
    if (timerOwner == NULL)
    {
        TRACE_E("CSalamanderGeneral::AddPluginFSTimer(): invalid timerOwner (NULL)!");
        return FALSE;
    }
    return Plugins.AddPluginFSTimer(timeout, timerOwner, timerParam);
}

int CSalamanderGeneral::KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers,
                                          DWORD timerParam)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::KillPluginFSTimer(, %d, 0x%X)", allTimers, timerParam);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::KillPluginFSTimer() only from main thread!");
        return 0;
    }
    if (timerOwner == NULL)
    {
        TRACE_E("CSalamanderGeneral::KillPluginFSTimer(): invalid timer owner (NULL)!");
        return 0;
    }
    return Plugins.KillPluginFSTimer(timerOwner, allTimers, timerParam);
}

BOOL CSalamanderGeneral::GetChangeDriveMenuItemVisibility()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetChangeDriveMenuItemVisibility()");
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetChangeDriveMenuItemVisibility() only from main thread!");
        return FALSE;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
        return data->ChDrvMenuFSItemVisible;
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::GetChangeDriveMenuItemVisibility().");
    return FALSE;
}

void CSalamanderGeneral::SetChangeDriveMenuItemVisibility(BOOL visible)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SetChangeDriveMenuItemVisibility(%d)", visible);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::SetChangeDriveMenuItemVisibility() only from main thread!");
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
        data->ChDrvMenuFSItemVisible = (visible != FALSE);
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::SetChangeDriveMenuItemVisibility().");
}

void CSalamanderGeneral::OleSpySetBreak(int alloc)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::OleSpySetBreak(%d)", alloc);
    ::OleSpySetBreak(alloc);
}

HICON
CSalamanderGeneral::GetSalamanderIcon(int icon, int iconSize)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::GetSalamanderIcon(%d, %d)", icon, iconSize);

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetSalamanderIcon() only from main thread!");
        return NULL;
    }

    CSymbolsImageListIndexes iconIndex;
    switch (icon)
    {
    case SALICON_EXECUTABLE:
        iconIndex = symbolsExecutable;
        break;
    case SALICON_DIRECTORY:
        iconIndex = symbolsDirectory;
        break;
    case SALICON_NONASSOCIATED:
        iconIndex = symbolsNonAssociated;
        break;
    case SALICON_ASSOCIATED:
        iconIndex = symbolsAssociated;
        break;
    case SALICON_UPDIR:
        iconIndex = symbolsUpDir;
        break;
    case SALICON_ARCHIVE:
        iconIndex = symbolsArchive;
        break;
    default:
    {
        TRACE_E("CSalamanderGeneral::GetSalamanderIcon: invalid icon=" << icon << " forcing SALICON_NONASSOCIATED");
        iconIndex = symbolsNonAssociated;
    }
    }

    CIconSizeEnum salIconSize;
    switch (iconSize)
    {
    case SALICONSIZE_16:
        salIconSize = ICONSIZE_16;
        break;
    case SALICONSIZE_32:
        salIconSize = ICONSIZE_32;
        break;
    case SALICONSIZE_48:
        salIconSize = ICONSIZE_48;
        break;
    default:
    {
        TRACE_E("CSalamanderGeneral::GetSalamanderIcon: invalid iconSize=" << iconSize << " forcing SALICONSIZE_16");
        salIconSize = ICONSIZE_16;
    }
    }

    CIconList* list = SimpleIconLists[salIconSize];
    if (list == NULL)
    {
        TRACE_E("CSalamanderGeneral::GetSalamanderIcon: list == NULL");
        return NULL;
    }

    return list->GetIcon(iconIndex, FALSE);
}

BOOL CSalamanderGeneral::GetFileIcon(const char* path, BOOL pathIsPIDL, HICON* hIcon, int iconSize,
                                     BOOL fallbackToDefIcon, BOOL defIconIsDir)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::GetFileIcon(, %d, , %d, %d, %d)",
                        pathIsPIDL, iconSize, fallbackToDefIcon, defIconIsDir);
    CIconSizeEnum salIconSize;
    switch (iconSize)
    {
    case SALICONSIZE_16:
        salIconSize = ICONSIZE_16;
        break;
    case SALICONSIZE_32:
        salIconSize = ICONSIZE_32;
        break;
    case SALICONSIZE_48:
        salIconSize = ICONSIZE_48;
        break;
    default:
    {
        TRACE_E("CSalamanderGeneral::GetFileIcon: invalid iconSize=" << iconSize << " forcing SALICONSIZE_16");
        salIconSize = ICONSIZE_16;
    }
    }
    return ::GetFileIcon(path, pathIsPIDL, hIcon, salIconSize, fallbackToDefIcon, defIconIsDir);
}

class CSalamanderPNG : public CSalamanderPNGAbstract
{
public:
    virtual HBITMAP WINAPI LoadPNGBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, DWORD flags, COLORREF unused)
    {
        HBITMAP hBitmap = ::LoadPNGBitmap(hInstance, lpBitmapName, flags);
        if (hBitmap != NULL) // handle is passed out to the plugin, the plugin is responsible for its deletion, we remove it from the Salamander HANDLES
            HANDLES_REMOVE(hBitmap, __htHandle_comp_with_DeleteObject, "DeleteObject");
        return hBitmap;
    }

    virtual HBITMAP WINAPI LoadRawPNGBitmap(const void* rawPNG, DWORD rawPNGSize, DWORD flags, COLORREF unused)
    {
        HBITMAP hBitmap = ::LoadRawPNGBitmap(rawPNG, rawPNGSize, flags);
        if (hBitmap != NULL) // handle is passed out to the plugin, the plugin is responsible for its deletion, we remove it from the Salamander HANDLES
            HANDLES_REMOVE(hBitmap, __htHandle_comp_with_DeleteObject, "DeleteObject");
        return hBitmap;
    }
};

CSalamanderPNG SalamanderPNG;

CSalamanderPNGAbstract*
CSalamanderGeneral::GetSalamanderPNG()
{
    CALL_STACK_MESSAGE_NONE
    return &SalamanderPNG;
}

CSalamanderPasswordManagerAbstract*
CSalamanderGeneral::GetSalamanderPasswordManager()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetSalamanderPasswordManager()");
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetSalamanderPasswordManager() only from main thread!");
        return NULL;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
        return &data->SalamanderPasswordManager;
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::GetSalamanderPasswordManager().");
    return NULL;
}

class CSalamanderCrypt : public CSalamanderCryptAbstract
{
public:
    /* AES functions*/
    virtual int WINAPI AESInit(CSalAES* aes,
                               int mode,       /* Mode (key size) to be used (input)*/
                               LPCSTR pwd,     /* User specified password (input)*/
                               size_t pwd_len, /* Password length (input)*/
                               LPBYTE salt,    /* Salt (input)*/
                               LPWORD pwd_ver) /* 2 byte password verifier (output)*/
    {
        _ASSERT(sizeof(aes->nonce) == sizeof(((fcrypt_ctx*)aes)->nonce));
        _ASSERT(sizeof(aes->encr_bfr) == sizeof(((fcrypt_ctx*)aes)->encr_bfr));
        _ASSERT(sizeof(aes->encr_ctx) == sizeof(((fcrypt_ctx*)aes)->encr_ctx));
        _ASSERT(sizeof(aes->auth_ctx) == sizeof(((fcrypt_ctx*)aes)->auth_ctx));
        _ASSERT(sizeof(aes->nonce) == sizeof(((fcrypt_ctx*)aes)->nonce));
        _ASSERT(sizeof(CSalAES) == sizeof(fcrypt_ctx));
        return fcrypt_init(mode, (unsigned char*)pwd, (unsigned int)pwd_len, salt, (unsigned char*)pwd_ver, (fcrypt_ctx*)aes);
    }

    virtual void WINAPI AESEncrypt(CSalAES* aes, LPVOID data, size_t dataLen)
    {
        fcrypt_encrypt((unsigned char*)data, (unsigned int)dataLen, (fcrypt_ctx*)aes);
    }

    virtual void WINAPI AESDecrypt(CSalAES* aes, LPVOID data, size_t dataLen)
    {
        fcrypt_decrypt((unsigned char*)data, (unsigned int)dataLen, (fcrypt_ctx*)aes);
    }

    virtual void WINAPI AESEnd(CSalAES* aes, LPBYTE mac, LPDWORD pMacLen)
    {
        if (pMacLen)
            *pMacLen = SAL_AES_MAC_LENGTH(aes->mode);
        fcrypt_end(mac, (fcrypt_ctx*)aes);
    }

    /* SHA1 functions*/
    virtual void WINAPI SHA1Init(CSalSHA1* sha1)
    {
        _ASSERT(sizeof(CSalSHA1) == sizeof(SHA1_Context));
        ::SHA1Init((SHA1_CTX*)sha1);
    }

    virtual void WINAPI SHA1Update(CSalSHA1* sha1, const LPBYTE data, size_t dataLen)
    {
        ::SHA1Update((SHA1_CTX*)sha1, data, (unsigned int)dataLen);
    }

    virtual void WINAPI SHA1Final(CSalSHA1* sha1, BYTE digest[20])
    {
        ::SHA1Final(digest, (SHA1_CTX*)sha1);
    }
};

CSalamanderCrypt SalamanderCrypt;

CSalamanderCryptAbstract* GetSalamanderCrypt() // for internal use from Salamander
{
    CALL_STACK_MESSAGE_NONE
    return &SalamanderCrypt;
}

CSalamanderCryptAbstract*
CSalamanderGeneral::GetSalamanderCrypt()
{
    CALL_STACK_MESSAGE_NONE
    return &SalamanderCrypt;
}

BOOL CSalamanderGeneral::FileExists(const char* fileName)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::FileExists(%s)", fileName);
    return ::FileExists(fileName);
}

void CSalamanderGeneral::DisconnectFSFromPanel(HWND parent, int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::DisconnectFSFromPanel(, %d)", panel);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::DisconnectFSFromPanel() only from main thread!");
        return;
    }
    char buf[MAX_PATH];
    BOOL rescueOrFixed = TRUE;
    if (GetLastWindowsPanelPath(panel, buf, MAX_PATH))
    { // change the path to the last visited Windows path
        BOOL tryNet = FALSE;
        DWORD err;
        DWORD lastErr;
        BOOL pathInvalid;
        BOOL cut;
        if (::SalCheckAndRestorePathWithCut(parent, buf, tryNet, err, lastErr,
                                            pathInvalid, cut, TRUE))
        {
            int failReason;
            if (ChangePanelPathToDisk(panel, buf, &failReason) ||
                failReason != CHPPFR_INVALIDPATH) // outside errors "wrong path" (refusal to close FS, etc.)
            {
                rescueOrFixed = FALSE;
            }
        }
    }
    if (rescueOrFixed)
        ChangePanelPathToRescuePathOrFixedDrive(panel); // "always false"
}

BOOL CSalamanderGeneral::IsArchiveHandledByThisPlugin(const char* name)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::IsArchiveHandledByThisPlugin(%s)", name);
    if (Plugin == NULL || (INT_PTR)Plugin == -1)
    {
        TRACE_E("CSalamanderGeneral::IsArchiveHandledByThisPlugin() unexpected call, plugin is not initialized yet!");
        return FALSE;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data == NULL)
    {
        TRACE_E("Unexpected situation in CSalamanderGeneral::IsArchiveHandledByThisPlugin()");
        return FALSE;
    }
    if (!data->SupportPanelView)
        return FALSE;

    int format = PackerFormatConfig.PackIsArchive(name);
    if (format != 0) // We found a supported archive
    {
        format--;
        int index = PackerFormatConfig.GetUnpackerIndex(format);
        if (index < 0) // view: is it internal processing (plugin)?
        {
            CPluginData* foundData = Plugins.Get(-index - 1);
            if (foundData == data) // Is it us?
                return TRUE;
        }
    }
    return FALSE;
}

DWORD
CSalamanderGeneral::GetIconLRFlags()
{
    return IconLRFlags;
}

int CSalamanderGeneral::IsFileLink(const char* fileExtension)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CSalamanderGeneral::IsFileLink()");
    if (fileExtension == NULL)
        return 0;
    return ::IsFileLink(fileExtension);
}

DWORD
CSalamanderGeneral::GetImageListColorFlags()
{
    CALL_STACK_MESSAGE_NONE
    return ::GetImageListColorFlags();
}

void CSalamanderGeneral::SetHelpFileName(const char* chmName)
{
    CALL_STACK_MESSAGE_NONE
    if (chmName == NULL || *chmName == 0)
        TRACE_E("CSalamanderGeneral::SetHelpFileName(): invalid parameter 'chmName'.");
    else
        lstrcpyn(HelpFileName, chmName, MAX_PATH);
}

BOOL CSalamanderGeneral::OpenHtmlHelp(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::OpenHtmlHelp(, %d, %Iu, %d)", command, dwData, quiet);
    if (HelpFileName[0] == 0)
    {
        TRACE_E("CSalamanderGeneral::OpenHtmlHelp(): plugin must call CSalamanderGeneral::SetHelpFileName() first!");
        return FALSE;
    }
    else
    {
        return ::OpenHtmlHelp(HelpFileName, parent, command, dwData, quiet);
    }
}

BOOL CSalamanderGeneral::OpenHtmlHelpForSalamander(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::OpenHtmlHelpForSalamander(, %d, %Iu, %d)", command, dwData, quiet);
    DWORD_PTR newData = dwData;
    if (command == HHCDisplayContext)
    {
        switch (dwData)
        {
        case HTMLHELP_SALID_PWDMANAGER:
            newData = IDH_PWDMANAGER;
            break; // password manager help
        default:
        {
            TRACE_E("CSalamanderGeneral::OpenHtmlHelpForSalamander(): invalid dwData parameter, see allowed HTMLHELP_SALID_XXX constants.");
            return FALSE;
        }
        }
    }
    return ::OpenHtmlHelp(NULL, parent, command, newData, quiet);
}

BOOL CSalamanderGeneral::PathsAreOnTheSameVolume(const char* path1, const char* path2,
                                                 BOOL* resIsOnlyEstimation)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::PathsAreOnTheSameVolume(, ,)");
    return ::PathsAreOnTheSameVolume(path1, path2, resIsOnlyEstimation);
}

BOOL CSalamanderGeneral::SafeGetOpenFileName(LPOPENFILENAME lpofn)
{
    CALL_STACK_MESSAGE_NONE
    return ::SafeGetOpenFileName(lpofn);
}

BOOL CSalamanderGeneral::SafeGetSaveFileName(LPOPENFILENAME lpofn)
{
    CALL_STACK_MESSAGE_NONE
    return ::SafeGetSaveFileName(lpofn);
}

void CSalamanderGeneral::SetPluginIsNethood()
{
    CALL_STACK_MESSAGE_NONE
    if (MainThreadID != GetCurrentThreadId() || (INT_PTR)Plugin != -1)
    {
        TRACE_E("You can call CSalamanderGeneral::SetPluginIsNethood() only from entry-point!");
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
        data->PluginIsNethood = TRUE;
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::SetPluginIsNethood().");
}

void CSalamanderGeneral::SetPluginUsesPasswordManager()
{
    CALL_STACK_MESSAGE_NONE
    if (MainThreadID != GetCurrentThreadId() || (INT_PTR)Plugin != -1)
    {
        TRACE_E("You can call CSalamanderGeneral::SetPluginUsesPasswordManager() only from entry-point!");
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
        data->PluginUsesPasswordManager = TRUE;
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::SetPluginUsesPasswordManager().");
}

void CSalamanderGeneral::OpenNetworkContextMenu(HWND parent, int panel, BOOL forItems, int menuX,
                                                int menuY, const char* netPath, char* newlyMappedDrive)
{
    CALL_STACK_MESSAGE6("CSalamanderGeneral::OpenNetworkContextMenu(, %d, %d, %d, %d, %s,)",
                        panel, forItems, menuX, menuY, netPath);

    if (newlyMappedDrive != NULL)
        *newlyMappedDrive = 0;

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::OpenNetworkContextMenu() only from main thread!");
        return;
    }

    if (netPath == NULL || netPath[0] != '\\' || netPath[1] != '\\' || strchr(netPath + 2, '\\') != NULL)
    {
        TRACE_E("CSalamanderGeneral::OpenNetworkContextMenu(): invalid netPath: " << (netPath != NULL ? netPath : "(null)"));
        return;
    }

    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        BeginStopRefresh(); // we do not need any refreshes (formality: the call goes from the plugin, so refreshes are already disabled in EnterPlugin)

        int* indexes = NULL;
        int index = 0;
        int count = 0;
        if (forItems)
        {
            BOOL subDir;
            if (p->Dirs->Count > 0)
                subDir = (strcmp(p->Dirs->At(0).Name, "..") == 0);
            else
                subDir = FALSE;

            count = p->GetSelCount();
            if (count != 0)
            {
                indexes = new int[count];
                p->GetSelItems(count, indexes, TRUE); // we have backed off from this (see GetSelItems): for the context menu, we start from the focused item and end with the item after the focus (there is an intermediate return to the beginning of the list of names) (the system does it too, see Add To Windows Media Player List on MP3 files)
            }
            else
            {
                index = p->GetCaretIndex();
                if (subDir && index == 0)
                {
                    EndStopRefresh();
                    return;
                }
            }
        }
        else
            index = -1;

        if (p->ContextMenu != NULL)
        {
            TRACE_E("CSalamanderGeneral::OpenNetworkContextMenu: p->ContextMenu must be NULL (probably forbidden recursive call)!");
        }
        else
        {
            if (forItems)
            {
                CTmpEnumData data;
                data.Indexes = (count == 0) ? &index : indexes;
                data.Panel = p;
                p->ContextMenu = CreateIContextMenu2(MainWindow->HWindow, netPath, (count == 0) ? 1 : count,
                                                     EnumFileNames, &data);
            }
            else
            {
                p->ContextMenu = CreateIContextMenu2(MainWindow->HWindow, netPath);
            }

            HMENU h = CreatePopupMenu();
            if (p->ContextMenu != NULL && h != NULL)
            {
                UINT flags = CMF_NORMAL | CMF_EXPLORE;
                // we will handle the pressed shift - extended context menu, under W2K there is for example Run as...
#define CMF_EXTENDEDVERBS 0x00000100 // rarely used verbs
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shiftPressed)
                    flags |= CMF_EXTENDEDVERBS;

                ShellActionAux5(flags, p, h);
                RemoveUselessSeparatorsFromMenu(h);

                int cmd = 0;
                if (GetMenuItemCount(h) > 0) // Protection against completely cut menu
                {
                    CMenuPopup contextPopup;
                    contextPopup.SetTemplateMenu(h);
                    cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                             menuX, menuY, parent, NULL);
                }
                if (cmd != 0)
                {
                    CALL_STACK_MESSAGE1("CSalamanderGeneral::OpenNetworkContextMenu::exec");

                    char cmdName[2000]; // intentionally we have 2000 instead of 200, shell extensions sometimes write double (consideration: unicode = 2 * "number of characters"), etc.
                    if (AuxGetCommandString(p->ContextMenu, cmd, GCS_VERB, NULL, cmdName, 200) != NOERROR)
                        cmdName[0] = 0;

                    // the Map Network Drive command is defined under XP 40, under W2K 43, and only under Vista does it have cmdName defined
                    if ((stricmp(cmdName, "connectNetworkDrive") == 0 ||
                         !WindowsVistaAndLater && cmd == 40) &&
                        forItems && netPath[2] != 0)
                    {
                        char root[MAX_PATH];
                        strcpy(root, netPath);
                        int focus = p->GetCaretIndex();
                        if (SalPathAppend(root, (focus < p->Dirs->Count ? p->Dirs->At(focus) : p->Files->At(focus - p->Dirs->Count)).Name, MAX_PATH))
                        {
                            char newDrive = 0;
                            p->ConnectNet(TRUE, root, FALSE /* It is called from the plugin, we must not change the path in the panel, otherwise we would be returning to a deallocated FS object*/
                                          ,
                                          &newDrive);
                            if (newlyMappedDrive != NULL)
                                *newlyMappedDrive = newDrive;
                        }
                    }
                    else
                    {
                        CShellExecuteWnd shellExecuteWnd;
                        CMINVOKECOMMANDINFOEX ici;
                        ZeroMemory(&ici, sizeof(CMINVOKECOMMANDINFOEX));
                        ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
                        ici.fMask = CMIC_MASK_PTINVOKE;
                        if (CanUseShellExecuteWndAsParent(cmdName))
                            ici.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: CSalamanderGeneral::OpenNetworkContextMenu cmd=%d", cmd);
                        else
                            ici.hwnd = MainWindow->HWindow;
                        ici.lpVerb = MAKEINTRESOURCE(cmd);
                        ici.nShow = SW_SHOWNORMAL;
                        ici.ptInvoke.x = menuX;
                        ici.ptInvoke.y = menuY;

                        AuxInvokeCommand(p, (CMINVOKECOMMANDINFO*)&ici);

                        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                        IdleCheckClipboard = TRUE; // we will also check the clipboard
                    }
                }
            }
            {
                CALL_STACK_MESSAGE1("CSalamanderGeneral::OpenNetworkContextMenu::release");
                ShellActionAux6(p);
                if (h != NULL)
                    DestroyMenu(h);
            }
        }

        if (count != 0)
            delete[] (indexes);

        EndStopRefresh();
    }
}

BOOL CSalamanderGeneral::DuplicateBackslashes(char* buffer, int bufferSize)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneral::DuplicateBackslashes(%s, %d)", buffer, bufferSize);
    return ::DuplicateBackslashes(buffer, bufferSize);
}

int CSalamanderGeneral::StartThrobber(int panel, const char* tooltip, int delay)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::StartThrobber(%d, %s, %d)", panel, tooltip, delay);

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::StartThrobber() only from main thread!");
        return -1;
    }

    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && p->DirectoryLine != NULL)
    {
        p->DirectoryLine->SetThrobber(TRUE, delay);
        p->DirectoryLine->SetThrobberTooltip(tooltip);
        return p->DirectoryLine->ChangeThrobberID();
    }
    return -1;
}

BOOL CSalamanderGeneral::StopThrobber(int id)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::StopThrobber(%d)", id);

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::StopThrobber() only from main thread!");
        return FALSE;
    }

    if (MainWindow->LeftPanel->DirectoryLine != NULL &&
        MainWindow->LeftPanel->DirectoryLine->IsThrobberVisible(id))
    {
        MainWindow->LeftPanel->DirectoryLine->SetThrobber(FALSE);
        return TRUE;
    }
    if (MainWindow->RightPanel->DirectoryLine != NULL &&
        MainWindow->RightPanel->DirectoryLine->IsThrobberVisible(id))
    {
        MainWindow->RightPanel->DirectoryLine->SetThrobber(FALSE);
        return TRUE;
    }
    return FALSE;
}

void CSalamanderGeneral::ShowSecurityIcon(int panel, BOOL showIcon, BOOL isLocked,
                                          const char* tooltip)
{
    CALL_STACK_MESSAGE5("CSalamanderGeneral::ShowSecurityIcon(%d, %d, %d, %s)",
                        panel, showIcon, isLocked, tooltip);

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::ShowSecurityIcon() only from main thread!");
        return;
    }

    CFilesWindow* p = GetPanel(panel);
    if (p != NULL && p->DirectoryLine != NULL)
    {
        p->DirectoryLine->SetSecurity(showIcon ? (isLocked ? sisSecured : sisUnsecured) : sisNone);
        p->DirectoryLine->SetSecurityTooltip(tooltip);
    }
}

void CSalamanderGeneral::RemoveCurrentPathFromHistory(int panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::RemoveCurrentPathFromHistory(%d)", panel);

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::RemoveCurrentPathFromHistory() only from main thread!");
        return;
    }

    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        p->RemoveCurrentPathFromHistory();
        if (MainWindow != NULL)
            MainWindow->DirHistoryRemoveActualPath(p);
        p->UserWorkedOnThisPath = FALSE;
    }
}

BOOL CSalamanderGeneral::IsUserAdmin()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::IsUserAdmin()");
    return ::IsUserAdmin();
}

BOOL CSalamanderGeneral::IsRemoteSession()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::IsRemoteSession()");
    return ::IsRemoteSession();
}

DWORD
CSalamanderGeneral::SalWNetAddConnection2Interactive(LPNETRESOURCE lpNetResource)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalWNetAddConnection2Interactive()");
    DWORD err;
    RestoreNetworkConnection(NULL, NULL, NULL, &err, lpNetResource);
    return err;
}

void CSalamanderGeneral::GetFocusedItemMenuPos(POINT* pos)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetFocusedItemMenuPos()");

    if (pos == NULL)
    {
        TRACE_E("CSalamanderGeneral::GetFocusedItemMenuPos(): invalid 'pos' (NULL)!");
        return;
    }

    pos->x = 0;
    pos->y = 0;

    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetFocusedItemMenuPos() only from main thread!");
        return;
    }

    if (MainWindow != NULL)
    {
        CFilesWindow* activePanel = MainWindow->GetActivePanel();
        if (activePanel != NULL)
        {
            activePanel->GetContextMenuPos(pos);
            return;
        }
    }

    pos->x = 0;
    pos->y = 0;
}

void CSalamanderGeneral::LockMainWindow(BOOL lock, HWND hToolWnd, const char* lockReason)
{
    CALL_STACK_MESSAGE4("CSalamanderGeneral::LockMainWindow(%d, 0x%p, %s)", lock, hToolWnd, lockReason);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::LockMainWindow() only from main thread!");
        return;
    }
    if (MainWindow != NULL)
        MainWindow->LockUI(lock, hToolWnd, lockReason);
}

BOOL CSalamanderGeneral::GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetMenuItemHotKey(%d, , , )", id);
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderGeneral::GetMenuItemHotKey() only from main thread!");
        return FALSE;
    }
    BOOL ret = FALSE;
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
        ret = data->GetMenuItemHotKey(id, hotKey, hotKeyText, hotKeyTextSize);
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::GetMenuItemHotKey().");
    return ret;
}

LONG CSalamanderGeneral::SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalRegQueryValue(, , ,)");
    return ::SalRegQueryValue(hKey, lpSubKey, lpData, lpcbData);
}

LONG CSalamanderGeneral::SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                                            LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalRegQueryValueEx(, , , , ,)");
    return ::SalRegQueryValueEx(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

DWORD
CSalamanderGeneral::SalGetFileAttributes(const char* fileName)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalGetFileAttributes()");
    return ::SalGetFileAttributes(fileName);
}

BOOL CSalamanderGeneral::IsPathOnSSD(const char* path)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::IsPathOnSSD()");
    return ::IsPathOnSSD(path);
}

BOOL CSalamanderGeneral::IsUNCPath(const char* path)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::IsUNCPath()");
    return ::IsUNCPath(path);
}

BOOL CSalamanderGeneral::ResolveSubsts(char* resPath)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::ResolveSubsts()");
    return ::ResolveSubsts(resPath);
}

void CSalamanderGeneral::ResolveLocalPathWithReparsePoints(char* resPath, const char* path, BOOL* cutResPathIsPossible,
                                                           BOOL* rootOrCurReparsePointSet, char* rootOrCurReparsePoint,
                                                           char* junctionOrSymlinkTgt, int* linkType, char* netPath)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::ResolveLocalPathWithReparsePoints()");
    ::ResolveLocalPathWithReparsePoints(resPath, path, cutResPathIsPossible,
                                        rootOrCurReparsePointSet, rootOrCurReparsePoint,
                                        junctionOrSymlinkTgt, linkType, netPath);
}

BOOL CSalamanderGeneral::GetResolvedPathMountPointAndGUID(const char* path, char* mountPoint, char* guidPath)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::GetResolvedPathMountPointAndGUID(%s, ,)", path);
    return ::GetResolvedPathMountPointAndGUID(path, mountPoint, guidPath);
}

BOOL CSalamanderGeneral::PointToLocalDecimalSeparator(char* buffer, int bufferSize)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::PointToLocalDecimalSeparator()");
    return ::PointToLocalDecimalSeparator(buffer, bufferSize);
}

void CSalamanderGeneral::SetPluginIconOverlays(int iconOverlaysCount, HICON* iconOverlays)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::SetPluginIconOverlays(%d,)", iconOverlaysCount);
    if (MainThreadID != GetCurrentThreadId())
    { // It's a call error, we don't release icons, not interested...
        TRACE_E("You can call CSalamanderGeneral::SetPluginIconOverlays() only from main thread!");
        return;
    }
    CPluginData* data = Plugins.GetPluginData(Plugin);
    if (data != NULL)
    {
        data->ReleaseIconOverlays();
        if (iconOverlaysCount > 0)
        {
            if (iconOverlays != NULL)
            {
                data->IconOverlays = (HICON*)malloc(3 * iconOverlaysCount * sizeof(HICON));
                memcpy(data->IconOverlays, iconOverlays, 3 * iconOverlaysCount * sizeof(HICON));
                data->IconOverlaysCount = iconOverlaysCount;

                BOOL err = FALSE;
                for (int i = 0; i < 3 * iconOverlaysCount; i++)
                {
                    if (data->IconOverlays[i] == NULL)
                    {
                        if (!err)
                            TRACE_E("CSalamanderGeneral::SetPluginIconOverlays(): invalid 'iconOverlays' (contains at least one NULL instead of icon handle)!");
                        err = TRUE;
                    }
                    else
                        HANDLES_ADD(__htIcon, __hoLoadImage, data->IconOverlays[i]);
                }
                if (err)
                    data->ReleaseIconOverlays();
            }
            else
                TRACE_E("CSalamanderGeneral::SetPluginIconOverlays(): invalid 'iconOverlays' (NULL)!");
        }
        else
        {
            if (iconOverlaysCount < 0)
                TRACE_E("CSalamanderGeneral::SetPluginIconOverlays(): invalid 'iconOverlaysCount' (negative value)!");
        }
    }
    else
        TRACE_E("Unexpected situation in CSalamanderGeneral::SetPluginIconOverlays().");
}

BOOL CSalamanderGeneral::SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::SalGetFileSize2()");
    return ::SalGetFileSize2(fileName, size, err);
}

BOOL CSalamanderGeneral::GetLinkTgtFileSize(HWND parent, const char* fileName, CQuadWord* size,
                                            BOOL* cancel, BOOL* ignoreAll)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::GetLinkTgtFileSize()");
    return ::GetLinkTgtFileSize(parent, fileName, NULL, size, cancel, ignoreAll);
}

BOOL CSalamanderGeneral::DeleteDirLink(const char* name, DWORD* err)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::DeleteDirLink()");
    return ::DeleteDirLink(name, err);
}

BOOL CSalamanderGeneral::ClearReadOnlyAttr(const char* name, DWORD attr)
{
    CALL_STACK_MESSAGE1("CSalamanderGeneral::ClearReadOnlyAttr()");
    return ::ClearReadOnlyAttr(name, attr);
}

BOOL CSalamanderGeneral::IsCriticalShutdown()
{
    return CriticalShutdown;
}

void CSalamanderGeneral::CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneral::CloseAllOwnedEnabledDialogs(, %d)", tid);
    ::CloseAllOwnedEnabledDialogs(parent, tid);
}

//
// ****************************************************************************
// CSalamanderForOperations
//

CSalamanderForOperations::CSalamanderForOperations(CFilesWindow* panel)
{
    Panel = panel;
    FocusWnd = NULL;
    ThreadID = GetCurrentThreadId();
    Destroyed = FALSE;
}

CSalamanderForOperations::~CSalamanderForOperations()
{
    if (UnpackProgress.HWindow != NULL)
    {
        TRACE_E("Progress dialog remains opened.");
        CloseProgressDialog();
    }
    Destroyed = TRUE;
}

void CSalamanderForOperations::OpenProgressDialog(const char* title, BOOL twoProgressBars, HWND parent,
                                                  BOOL fileProgress)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::OpenProgressDialog() only from thread ID:" << ThreadID);
        return;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::OpenProgressDialog() after destruction!");
        return;
    }
    if (UnpackProgress.HWindow == NULL)
    {
        if (parent == NULL)
        {
            parent = MainWindow->HWindow;
            UnpackProgress.SetTaskBarList3(&MainWindow->TaskBarList3);
        }
        if (!twoProgressBars)
            UnpackProgress.Set(title, parent, CQuadWord(0, 0), fileProgress);
        else
            UnpackProgress.Set(title, parent, CQuadWord(0, 0), CQuadWord(0, 0));
        FocusWnd = GetFocus();
        EnableWindow(UnpackProgress.GetParent(), FALSE);
        UnpackProgress.Create();

        ActivateDropTarget(ProgressDialogActivateDrop, UnpackProgress.HWindow);

        ProgressDialog2 = twoProgressBars;
        PluginProgressDialog = UnpackProgress.HWindow;
    }
}

void CSalamanderForOperations::CloseProgressDialog()
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::CloseProgressDialog() only from thread ID:" << ThreadID);
        return;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::CloseProgressDialog() after destruction!");
        return;
    }
    if (UnpackProgress.HWindow != NULL)
    {
        PluginProgressDialog = NULL;
        EnableWindow(UnpackProgress.GetParent(), TRUE);
        HWND actWnd = GetForegroundWindow();
        BOOL activate = actWnd == UnpackProgress.HWindow || actWnd == UnpackProgress.GetParent();
        DestroyWindow(UnpackProgress.HWindow);
        if (activate && FocusWnd != NULL)
            SetFocus(FocusWnd);
        UnpackProgress.Init();
    }
}

void CSalamanderForOperations::ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::ProgressSetTotalSize() only from thread ID:" << ThreadID);
        return;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::ProgressSetTotalSize() after destruction!");
        return;
    }
    if (!ProgressDialog2 && totalSize2 != CQuadWord(-1, -1))
    {
        TRACE_E("Incorrect call to CSalamanderForOperations::ProgressSetTotalSize(): progress dialog has only one progress!");
        return;
    }
    if (UnpackProgress.HWindow != NULL)
        UnpackProgress.SetTotal(totalSize1, totalSize2);
}

void CSalamanderForOperations::ProgressDialogAddText(const char* txt, BOOL delayedPaint)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::ProgressDialogAddText() only from thread ID:" << ThreadID);
        return;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::ProgressDialogAddText() after destruction!");
        return;
    }
    if (UnpackProgress.HWindow != NULL)
        UnpackProgress.NewLine(txt, delayedPaint);
}

BOOL CSalamanderForOperations::ProgressAddSize(int size, BOOL delayedPaint)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::ProgressAddSize() only from thread ID:" << ThreadID);
        return FALSE;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::ProgressAddSize() after destruction!");
        return FALSE;
    }
    if (UnpackProgress.HWindow != NULL)
        return UnpackProgress.AddSize(size, delayedPaint) == 1;
    return TRUE;
}

BOOL CSalamanderForOperations::ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::ProgressSetSize() only from thread ID:" << ThreadID);
        return FALSE;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::ProgressSetSize() after destruction!");
        return FALSE;
    }
    if (!ProgressDialog2 && size2 != CQuadWord(-1, -1))
    {
        TRACE_E("Incorrect call to CSalamanderForOperations::ProgressSetSize(): progress dialog has only one progress!");
        return FALSE;
    }
    if (UnpackProgress.HWindow != NULL)
        return UnpackProgress.SetSize(size1, size2, delayedPaint) == 1;
    return TRUE;
}

void CSalamanderForOperations::ProgressEnableCancel(BOOL enable)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::ProgressEnableCancel() only from thread ID:" << ThreadID);
        return;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::ProgressEnableCancel() after destruction!");
        return;
    }
    if (UnpackProgress.HWindow != NULL)
        UnpackProgress.EnableCancel(enable);
}

BOOL CSalamanderForOperations::MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                         const char* remapNameTo)
{
    if (ThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderForOperations::MoveFiles() only from thread ID:" << ThreadID);
        return FALSE;
    }
    if (Destroyed)
    {
        TRACE_E("You are calling CSalamanderForOperations::MoveFiles() after destruction!");
        return FALSE;
    }
    if (Panel == NULL)
    {
        TRACE_E("Incorrect call to CSalamanderForOperations::MoveFiles");
        return FALSE;
    }
    return Panel->MoveFiles(source, target, remapNameFrom, remapNameTo);
}

//
// ****************************************************************************
// SalamanerForViewFileOnFS
//

const char*
CSalamanderForViewFileOnFS::AllocFileNameInCache(HWND parent, const char* uniqueFileName, const char* nameInCache,
                                                 const char* rootTmpPath, BOOL& fileExists)
{
    CALL_STACK_MESSAGE4("CSalamanderForViewFileOnFS::AllocFileNameInCache(, %s, %s, %s, )",
                        uniqueFileName, nameInCache, rootTmpPath);
    if (CallsCounter > 0)
    {
        TRACE_E("You are calling CSalamanderForViewFileOnFS::AllocFileNameInCache more than once! Is it O.K.?");
    }

    int errorCode;
    const char* name = DiskCache.GetName(uniqueFileName, nameInCache, &fileExists, FALSE, rootTmpPath, FALSE, NULL, &errorCode);
    if (name == NULL)
    {
        char buff[2000];
        strcpy(buff, LoadStr(IDS_VIEWFILEFAILED));
        if (errorCode == DCGNE_TOOLONGNAME)
        {
            strcat(buff, "\n");
            strcat(buff, LoadStr(IDS_VIEWFILETOOLONGNAME));
        }
        SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
    else
    {
        CallsCounter++;
    }

    return name;
}

BOOL CSalamanderForViewFileOnFS::OpenViewer(HWND parent, const char* fileName, HANDLE* fileLock,
                                            BOOL* fileLockOwner)
{
    CALL_STACK_MESSAGE2("CSalamanderForViewFileOnFS::OpenViewer(, %s, ,)", fileName);

    HANDLE lock = NULL;
    BOOL lockOwner = FALSE;

    BOOL ret = ViewFileInt(parent, fileName, AltView, HandlerID,
                           (fileLock != NULL && fileLockOwner != NULL),
                           lock, lockOwner, FALSE, -1, -1);

    if (fileLock != NULL)
        *fileLock = lock;
    if (fileLockOwner != NULL)
        *fileLockOwner = lockOwner;
    return ret;
}

void CSalamanderForViewFileOnFS::FreeFileNameInCache(const char* uniqueFileName, BOOL fileExists, BOOL newFileOK,
                                                     const CQuadWord& newFileSize, HANDLE fileLock,
                                                     BOOL fileLockOwner, BOOL removeAsSoonAsPossible)
{
    CALL_STACK_MESSAGE8("CSalamanderForViewFileOnFS::FreeFileNameInCache(%s, %d, %d, %g, 0x%p, %d, %d)",
                        uniqueFileName, fileExists, newFileOK, newFileSize.GetDouble(), fileLock,
                        fileLockOwner, removeAsSoonAsPossible);

    if (CallsCounter == 0)
    {
        TRACE_E("Unmatched call to CSalamanderForViewFileOnFS::FreeFileNameInCache!");
        return;
    }
    CallsCounter--;

    if (!fileExists) // newly downloaded copy of the file
    {
        if (newFileOK) // download successful
        {
            DiskCache.NamePrepared(uniqueFileName, newFileSize);
        }
        else
        {
            DiskCache.ReleaseName(uniqueFileName, FALSE); // Download failed, there is nothing to cache
            return;                                       // There is nothing left to solve
        }
    }

    if (fileLock != NULL) // we have a "lock" object of the viewer, we tie the viewer and disk-cache
    {
        DiskCache.AssignName(uniqueFileName, fileLock, fileLockOwner,
                             (fileExists || removeAsSoonAsPossible) ? crtDirect : crtCache); // For files already existing in the disk cache, we use crtDirect because it does not affect the "lifespan" settings (it remains as the file creator intended).
    }
    else // The viewer did not open or just does not have a "lock" object
    {
        DiskCache.ReleaseName(uniqueFileName, !fileExists && !removeAsSoonAsPossible); // if 'removeAsSoonAsPossible' is not TRUE, we will try to at least keep a copy of the file in the disk cache (if it was not an existing file, we do not change its "lifespan")
    }
}

//
// ****************************************************************************
// CSalamanderDirectory
//

CSalamanderDirectory::CSalamanderDirectory(BOOL isForFS, DWORD validData, DWORD flags)
    : Dirs(10, 200), SalamDirs(10, 200), Files(10, 200)
{
    ValidData = validData;
    if (flags == -1)
        flags = isForFS ? SALDIRFLAG_IGNOREDUPDIRS : 0;
    Flags = flags;
    IsForFS = isForFS;
    AddCache = NULL;
}

CSalamanderDirectory::~CSalamanderDirectory()
{
    Clear(NULL); // data plugin is only released in the root-sal-dir
    FreeAddCache();
}

void CSalamanderDirectory::AllocAddCache()
{
    if (AddCache == NULL)
    {
        AddCache = (CSalamanderDirectoryAddCache*)malloc(sizeof(CSalamanderDirectoryAddCache));
        if (AddCache != NULL)
            ZeroMemory(AddCache, sizeof(CSalamanderDirectoryAddCache));
        // If the cache allocation fails, it's okay, we are fully functional even without it
    }
}

void CSalamanderDirectory::FreeAddCache()
{
    if (AddCache != NULL)
    {
        free(AddCache);
        AddCache = NULL;
    }
}

int CSalamanderDirectory::SalDirStrCmp(const char* s1, const char* s2)
{
    if (Flags & SALDIRFLAG_CASESENSITIVE)
        return strcmp(s1, s2);
    else
        return StrICmp(s1, s2);
}

int CSalamanderDirectory::SalDirStrCmpEx(const char* s1, int l1, const char* s2, int l2)
{
    if (Flags & SALDIRFLAG_CASESENSITIVE)
        return StrCmpEx(s1, l1, s2, l2);
    else
        return StrICmpEx(s1, l1, s2, l2);
}

void CSalamanderDirectory::Clear(CPluginDataInterfaceAbstract* pluginData)
{
    if (pluginData != NULL) // Release of data specific to plug-ins
    {
        CPluginDataInterfaceEncapsulation plugin(pluginData, STR_NONE, STR_NONE, NULL, 0);
        BOOL releaseFiles = plugin.CallReleaseForFiles();
        BOOL releaseDirs = plugin.CallReleaseForDirs();
        if (releaseFiles || releaseDirs)
        {
            ReleasePluginData(plugin, releaseFiles, releaseDirs);
        }
    }
    int i;
    for (i = 0; i < SalamDirs.Count; i++)
    {
        CSalamanderDirectory* salDir = SalamDirs[i];
        if (salDir != NULL)
            delete salDir;
    }
    SalamDirs.DestroyMembers();
    Dirs.DestroyMembers();
    Files.DestroyMembers();
    if (AddCache != NULL)
    {
        AddCache->PathLen = 0;
        AddCache->Path[0] = 0;
        AddCache->Dir = NULL;
    }
    ValidData = VALID_DATA_ALL_FS_ARC;
    Flags = IsForFS ? SALDIRFLAG_IGNOREDUPDIRS : 0;
}

void CSalamanderDirectory::SetValidData(DWORD validData)
{
    if (ValidData != validData)
    {
        ValidData = validData;
        int i;
        for (i = 0; i < SalamDirs.Count; i++)
        {
            CSalamanderDirectory* salDir = SalamDirs[i];
            if (salDir != NULL)
                salDir->SetValidData(validData);
        }
    }
}

void CSalamanderDirectory::SetFlags(DWORD flags)
{
    if (Flags != flags)
    {
        Flags = flags;
        int i;
        for (i = 0; i < SalamDirs.Count; i++)
        {
            CSalamanderDirectory* salDir = SalamDirs[i];
            if (salDir != NULL)
                salDir->SetFlags(flags);
        }
    }
}

CSalamanderDirectory*
CSalamanderDirectory::AllocSalamDir(int index)
{
    CALL_STACK_MESSAGE_NONE // time-critical method

        if (index < 0 || index >= SalamDirs.Count || SalamDirs[index] != NULL)
    {
        TRACE_E("Unexpected error in CSalamanderDirectory::AllocSalamDir().");
        return NULL;
    }
    CSalamanderDirectory* dir = new CSalamanderDirectory(IsForFS, ValidData, Flags);
    if (dir == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return NULL;
    }
    SalamDirs[index] = dir;
    return dir;
}

// ***************************************************************************
// FindDir:
//
// 'path' - input: path in the archive (relative to this directory)
// 's' - vystup: ukazuje za prvni jmeno v ceste 'path'
// 'i' - output: index of the found subdirectory (which will further process the path 's')
// 'file' - input: if a directory needs to be created from which to copy the data
// 'pluginData' - input: iface for creating plug-in specific data of a new directory (if needed)
// 'archivePath' - vstup: kompletni cesta v archivu ('path' i 's' ukazuji do ni)

BOOL CSalamanderDirectory::FindDir(const char* path, const char*& s, int& i, const CFileData& file,
                                   CPluginDataInterfaceAbstract* pluginData, const char* archivePath)
{
    CALL_STACK_MESSAGE_NONE // time-critical method
        //  CALL_STACK_MESSAGE2("CSalamanderDirectory::FindDir(%s, , , ,)", path);
        s = path;
    while (*s != 0 && *s != '\\')
        s++;

    for (i = 0; i < Dirs.Count; i++)
    {
        if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, path, (int)(s - path)) == 0)
            break;
    }
    if (i == Dirs.Count) // we need to initialize it
    {
        CFileData data;
        //--- name
        data.Name = (char*)malloc((s - path) + 1); // allocation
        if (data.Name == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        memcpy(data.Name, path, s - path); // copy of text
        data.Name[s - path] = 0;
        data.NameLen = s - path;
        //--- extension
        if (!Configuration.SortDirsByExt)
            data.Ext = data.Name + data.NameLen; // directories do not have extensions
        else
        {
            const char* ss = s;
            while (--ss >= path && *ss != '.')
                ;
            if (ss >= path)
                data.Ext = data.Name + (ss - path + 1); // ".cvspass" in Windows is an extension ...
                                                        //      if (ss > path) data.Ext = data.Name + (ss - path + 1);
            else
                data.Ext = data.Name + data.NameLen;
        }
        //--- others
        data.Size = CQuadWord(0, 0);
        data.Attr = 0;
        data.LastWrite = file.LastWrite; // we will take the date from the first file in the directory
        data.DosName = NULL;
        data.PluginData = 0;
        data.Hidden = 0;
        data.IsLink = 0;
        data.IsOffline = 0;
        // private Salamander data
        data.Association = 0;
        data.Selected = 0;
        data.Shared = 0;
        data.Archive = 0;
        data.SizeValid = 0;
        data.Dirty = 0; // not mandatory, just for the sake of formality
        data.CutToClip = 0;
        data.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        data.IconOverlayDone = 0;

        if (pluginData != NULL) // let the plug-in add its specific data
        {
            char arcPath[MAX_PATH]; // Name of the added directory inside the archive
            memcpy(arcPath, archivePath, s - archivePath);
            arcPath[s - archivePath] = 0;
            CPluginDataInterfaceEncapsulation plugin(pluginData, STR_NONE, STR_NONE, NULL, 0);
            if (!plugin.GetFileDataForNewDir(arcPath, data)) // Unable to add plug-in data
            {
                free(data.Name);
                return FALSE;
            }
        }

        Dirs.Add(data);
        if (!Dirs.IsGood())
        {
            Dirs.ResetState();
            if (pluginData != NULL) // Release of data specific to plug-ins
            {
                CPluginDataInterfaceEncapsulation plugin(pluginData, STR_NONE, STR_NONE, NULL, 0);
                if (plugin.CallReleaseForDirs())
                    plugin.ReleasePluginData2(data, TRUE);
            }
            free(data.Name);
            return FALSE;
        }
        //--- adding salamander directory corresponding to the new directory
        /*      CSalamanderDirectory *dir = new CSalamanderDirectory(IsForFS, ValidData, Flags);
    if (dir != NULL) SalamDirs.Add((DWORD)dir);
    else TRACE_E(LOW_MEMORY);
    if (dir == NULL || !SalamDirs.IsGood())
    {
      if (dir != NULL) delete dir;
      SalamDirs.ResetState();
      if (pluginData != NULL)   // releasing plugin-specific data
      {
        CPluginDataInterfaceEncapsulation plugin(pluginData, STR_NONE, STR_NONE, NULL, 0);
        if (plugin.CallReleaseForDirs()) plugin.ReleasePluginData2(Dirs[Dirs.Count - 1], TRUE);
      }
      Dirs.Delete(Dirs.Count - 1);
      return FALSE;
    }*/
        SalamDirs.Add(NULL); // add NULL (the object will be allocated when first needed)
        if (!SalamDirs.IsGood())
        {
            SalamDirs.ResetState();
            if (pluginData != NULL) // Release of data specific to plug-ins
            {
                CPluginDataInterfaceEncapsulation plugin(pluginData, STR_NONE, STR_NONE, NULL, 0);
                if (plugin.CallReleaseForDirs())
                    plugin.ReleasePluginData2(Dirs[Dirs.Count - 1], TRUE);
            }
            Dirs.Delete(Dirs.Count - 1);
            if (!Dirs.IsGood())
                Dirs.ResetState();
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CSalamanderDirectory::AddFile(const char* path, CFileData& file, CPluginDataInterfaceAbstract* pluginData)
{
    CALL_STACK_MESSAGE_NONE // time-critical method

        int pathLen = 0;
    if (path != NULL && ((pathLen = (int)strlen(path)) > MAX_PATH - 5 || file.NameLen > MAX_PATH - 5))
    {
        TRACE_E("Too long path or file name!");
        return FALSE;
    }

    //  TRACE_I("AddFile path="<<path<<" file="<<file.Name);

    // we are zeroing variables that the plugin does not define
    if ((ValidData & VALID_DATA_EXTENSION) == 0)
        file.Ext = file.Name + file.NameLen;
    if ((ValidData & VALID_DATA_DOSNAME) == 0)
        file.DosName = NULL;
    if ((ValidData & VALID_DATA_SIZE) == 0)
        file.Size = CQuadWord(0, 0);
    if ((ValidData & VALID_DATA_DATE) == 0 || (ValidData & VALID_DATA_TIME) == 0)
    {
        SYSTEMTIME st;
        FILETIME ft;
        if ((ValidData & (VALID_DATA_DATE | VALID_DATA_TIME)) == 0 ||
            FileTimeToLocalFileTime(&file.LastWrite, &ft) &&
                FileTimeToSystemTime(&ft, &st))
        {
            if ((ValidData & VALID_DATA_DATE) == 0) // missing date
            {
                st.wYear = 1602;
                st.wMonth = 1;
                st.wDay = 1;
                st.wDayOfWeek = 2;
            }
            if ((ValidData & VALID_DATA_TIME) == 0) // missing time
            {
                st.wHour = 0;
                st.wMinute = 0;
                st.wSecond = 0;
                st.wMilliseconds = 0;
            }
            SystemTimeToFileTime(&st, &ft);
            LocalFileTimeToFileTime(&ft, &file.LastWrite);
        }
        else // invalid file.LastWrite
        {
            TRACE_E("CSalamanderDirectory::AddFile(): invalid file.LastWrite!");
            file.LastWrite.dwLowDateTime = 0;
            file.LastWrite.dwHighDateTime = 0;
        }
    }
    if ((ValidData & VALID_DATA_ATTRIBUTES) == 0)
        file.Attr = 0;
    if ((ValidData & VALID_DATA_HIDDEN) == 0)
        file.Hidden = 0;
    if ((ValidData & VALID_DATA_ISLINK) == 0)
        file.IsLink = 0;
    if ((ValidData & VALID_DATA_ISOFFLINE) == 0)
        file.IsOffline = 0;
    if ((ValidData & VALID_DATA_ICONOVERLAY) == 0)
        file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;

    file.Association = 0;
    file.Selected = 0;
    file.Shared = 0;
    file.Archive = 0;
    file.SizeValid = 0;
    file.Dirty = 0; // optional, just for the sake of form
    file.CutToClip = 0;
    file.IconOverlayDone = 0;

    // If we have the path cached from the previous addition, we can insert the file directly in its place
    if (path != NULL && AddCache != NULL && pathLen > 0 &&
        pathLen == AddCache->PathLen && memcmp(path, AddCache->Path, pathLen) == 0)
    {
        // cache contained our path, we can add it directly
        AddCache->Dir->Files.Add(file);
        if (!AddCache->Dir->Files.IsGood())
        {
            AddCache->Dir->Files.ResetState();
            return FALSE;
        }
        return TRUE;
    }

    CSalamanderDirectory* ret = AddFileInt(path, file, pluginData, path);

    // if the addition was successful and we are using cache, we save the path
    if (ret != NULL && AddCache != NULL && pathLen > 0)
    {
        AddCache->PathLen = pathLen;
        memcpy(AddCache->Path, path, pathLen);
        AddCache->Dir = ret;
    }

    return ret != NULL;
}

BOOL CSalamanderDirectory::AddDir(const char* path, CFileData& dir, CPluginDataInterfaceAbstract* pluginData)
{
    CALL_STACK_MESSAGE_NONE // time-critical method

        if (path != NULL && (strlen(path) > MAX_PATH - 5 || dir.NameLen > MAX_PATH - 5))
    {
        TRACE_E("Too long path or file name!");
        return FALSE;
    }

    //  TRACE_I("AddDir path="<<path<<" dir="<<dir.Name);

    // we are zeroing variables that the plugin does not define
    if ((ValidData & VALID_DATA_EXTENSION) == 0)
        dir.Ext = dir.Name + dir.NameLen;
    if ((ValidData & VALID_DATA_DOSNAME) == 0)
        dir.DosName = NULL;
    if ((ValidData & VALID_DATA_SIZE) == 0)
        dir.Size = CQuadWord(0, 0);
    if ((ValidData & VALID_DATA_DATE) == 0 || (ValidData & VALID_DATA_TIME) == 0)
    {
        SYSTEMTIME st;
        FILETIME ft;
        if ((ValidData & (VALID_DATA_DATE | VALID_DATA_TIME)) == 0 ||
            FileTimeToLocalFileTime(&dir.LastWrite, &ft) &&
                FileTimeToSystemTime(&ft, &st))
        {
            if ((ValidData & VALID_DATA_DATE) == 0) // missing date
            {
                st.wYear = 1602;
                st.wMonth = 1;
                st.wDay = 1;
                st.wDayOfWeek = 2;
            }
            if ((ValidData & VALID_DATA_TIME) == 0) // missing time
            {
                st.wHour = 0;
                st.wMinute = 0;
                st.wSecond = 0;
                st.wMilliseconds = 0;
            }
            SystemTimeToFileTime(&st, &ft);
            LocalFileTimeToFileTime(&ft, &dir.LastWrite);
        }
        else // invalid dir.LastWrite
        {
            TRACE_E("CSalamanderDirectory::AddDir(): invalid dir.LastWrite!");
            dir.LastWrite.dwLowDateTime = 0;
            dir.LastWrite.dwHighDateTime = 0;
        }
    }
    if ((ValidData & VALID_DATA_ATTRIBUTES) == 0)
        dir.Attr = 0;
    if ((ValidData & VALID_DATA_HIDDEN) == 0)
        dir.Hidden = 0;
    if ((ValidData & VALID_DATA_ISLINK) == 0)
        dir.IsLink = 0;
    if ((ValidData & VALID_DATA_ISOFFLINE) == 0)
        dir.IsOffline = 0;
    if ((ValidData & VALID_DATA_ICONOVERLAY) == 0)
        dir.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;

    dir.Association = 0;
    dir.Selected = 0;
    dir.Shared = 0;
    dir.Archive = 0;
    dir.SizeValid = 0;
    dir.Dirty = 0; // not mandatory, just for the sake of formality
    dir.CutToClip = 0;
    dir.IconOverlayDone = 0;

    return AddDirInt(path, dir, pluginData, path) != NULL;
}

int CSalamanderDirectory::GetFilesCount() const
{
    CALL_STACK_MESSAGE_NONE // time-critical method
        return Files.Count;
}

int CSalamanderDirectory::GetDirsCount() const
{
    CALL_STACK_MESSAGE_NONE // time-critical method
        return Dirs.Count;
}

CFileData const*
CSalamanderDirectory::GetFile(int i) const
{
    CALL_STACK_MESSAGE_NONE // time-critical method
        if (i >= 0 && i < Files.Count) return &(*((CFilesArray*)&Files))[i];
    else return NULL;
}

CFileData const*
CSalamanderDirectory::GetDir(int i) const
{
    CALL_STACK_MESSAGE_NONE // time-critical method
        if (i >= 0 && i < Dirs.Count) return &(*((CFilesArray*)&Dirs))[i];
    else return NULL;
}

CSalamanderDirectoryAbstract const*
CSalamanderDirectory::GetSalDir(int i) const
{
    CALL_STACK_MESSAGE_NONE // time-critical method
        if (i >= 0 && i < SalamDirs.Count)
    {
        CSalamanderDirectoryAbstract const* salDir = (CSalamanderDirectoryAbstract const*)(*((TDirectArray<CSalamanderDirectory*>*)&SalamDirs))[i];
        if (salDir == NULL)
            salDir = &GlobalEmptySalDir; // it is an empty directory - we will return the global empty directory
        return salDir;
    }
    else return NULL;
}

CSalamanderDirectory*
CSalamanderDirectory::AddFileInt(const char* path, CFileData& file,
                                 CPluginDataInterfaceAbstract* pluginData, const char* archivePath)
{
    CALL_STACK_MESSAGE_NONE // time-critical method; additionally, path can be NULL
                            //  CALL_STACK_MESSAGE3("CSalamanderDirectory::AddFileInt(%s, , , %s)", path, archivePath);

        if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // It's not about this directory, we will find a subdirectory
        {
            const char* s;
            int i;
            if (!FindDir(path, s, i, file, pluginData, archivePath))
                return NULL;

            CSalamanderDirectory* salDir = SalamDirs[i];
            if (salDir != NULL ||                    // allocated now
                (salDir = AllocSalamDir(i)) != NULL) // or a new object was successfully allocated
            {
                return salDir->AddFileInt(s, file, pluginData, archivePath);
            }
            else
                return NULL;
        }
    }

    // Attention, if AddCache is applied, the item is added directly in AddFile
    Files.Add(file);
    if (!Files.IsGood())
    {
        Files.ResetState();
        return NULL;
    }
    return this;
}

CSalamanderDirectory*
CSalamanderDirectory::AddDirInt(const char* path, CFileData& dir,
                                CPluginDataInterfaceAbstract* pluginData, const char* archivePath)
{
    CALL_STACK_MESSAGE_NONE // time-critical method; additionally, path can be NULL
                            //  CALL_STACK_MESSAGE3("CSalamanderDirectory::AddDirInt(%s, , , %s)", path, archivePath);

        if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // It's not about this directory, we will find a subdirectory
        {
            const char* s;
            int i;
            if (!FindDir(path, s, i, dir, pluginData, archivePath))
                return NULL;

            CSalamanderDirectory* salDir = SalamDirs[i];
            if (salDir != NULL ||                    // allocated now
                (salDir = AllocSalamDir(i)) != NULL) // or a new object was successfully allocated
            {
                return salDir->AddDirInt(s, dir, pluginData, archivePath);
            }
            else
                return NULL;
        }
    }

    BOOL newDir = TRUE;
    if ((Flags & SALDIRFLAG_IGNOREDUPDIRS) == 0) // if we are testing for duplicate directories
    {
        int i;
        for (i = 0; i < Dirs.Count; i++)
        {
            if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, dir.Name, dir.NameLen) == 0)
                break;
        }
        newDir = (i == Dirs.Count); // has not been created yet
        if (!newDir)                // change existing data
        {
            if (pluginData != NULL) // Release of data specific to plug-ins
            {
                CPluginDataInterfaceEncapsulation plugin(pluginData, STR_NONE, STR_NONE, NULL, 0);
                if (plugin.CallReleaseForDirs())
                    plugin.ReleasePluginData2(Dirs[i], TRUE);
            }

            if (Dirs[i].Name != NULL)
                free(Dirs[i].Name);
            Dirs[i].Name = dir.Name; // we better take a new name (for possible data after '\0' in the string)
            Dirs[i].Ext = dir.Ext;
            Dirs[i].Size = dir.Size;
            Dirs[i].Attr = dir.Attr;
            Dirs[i].LastWrite = dir.LastWrite;
            if (Dirs[i].DosName != NULL)
                free(Dirs[i].DosName);
            Dirs[i].DosName = dir.DosName;
            Dirs[i].PluginData = dir.PluginData;
            // Dirs[i].NameLen should be the same as dir.NameLen
            Dirs[i].Hidden = dir.Hidden;
            Dirs[i].IsLink = dir.IsLink;
            Dirs[i].IsOffline = dir.IsOffline;
            // The rest of Dirs[i] should be zeroed out just like the rest of dir
        }
    }
    if (newDir)
    {
        //--- adding salamander directory corresponding to the new directory
        /*      CSalamanderDirectory *SalamDir = new CSalamanderDirectory(IsForFS, ValidData, Flags);
    if (SalamDir != NULL) SalamDirs.Add((DWORD)SalamDir);
    else TRACE_E(LOW_MEMORY);
    if (SalamDir == NULL || !SalamDirs.IsGood())
    {
      if (SalamDir != NULL) delete SalamDir;
      SalamDirs.ResetState();
      return FALSE;
    }

    Dirs.Add(dir);
    if (!Dirs.IsGood())
    {
      Dirs.ResetState();
      SalamDirs.Delete(SalamDirs.Count - 1);
      delete SalamDir;
      return FALSE;
    }*/
        if (IsForFS && dir.NameLen == 2 && dir.Name[0] == '.' && dir.Name[1] == '.')
        {
            CFileData* firstDir = Dirs.Count > 0 ? &Dirs[0] : NULL;
            if (firstDir != NULL && firstDir->NameLen == 2 &&
                firstDir->Name[0] == '.' && firstDir->Name[1] == '.')
            { // we have one up-dir already
                TRACE_E("CSalamanderDirectory::AddFile(): you can add up-dir (\"..\") at most once!");
                return NULL;
            }
            SalamDirs.Insert(0, NULL); // add NULL (the object will be allocated when first needed)
            if (!SalamDirs.IsGood())
            {
                SalamDirs.ResetState();
                return NULL;
            }

            Dirs.Insert(0, dir);
            if (!Dirs.IsGood())
            {
                Dirs.ResetState();
                SalamDirs.Delete(0);
                if (!SalamDirs.IsGood())
                    SalamDirs.ResetState();
                return NULL;
            }
        }
        else
        {
            SalamDirs.Add(NULL); // add NULL (the object will be allocated when first needed)
            if (!SalamDirs.IsGood())
            {
                SalamDirs.ResetState();
                return NULL;
            }

            Dirs.Add(dir);
            if (!Dirs.IsGood())
            {
                Dirs.ResetState();
                SalamDirs.Delete(SalamDirs.Count - 1);
                if (!SalamDirs.IsGood())
                    SalamDirs.ResetState();
                return NULL;
            }
        }
    }
    return this;
}

extern int DeltaForTotalCount(int total);

void CSalamanderDirectory::SetApproximateCount(int files, int dirs)
{
    CALL_STACK_MESSAGE3("CSalamanderDirectory::SetApproximateCount(%d, %d)", files, dirs);
    if (files > 1)
    {
        if (Files.Count == 0)
            Files.SetDelta(DeltaForTotalCount(files));
        else
            TRACE_E("CSalamanderDirectory::SetApproximateCount() Files.Count = " << Files.Count);
    }
    if (dirs > 1)
    {
        if (Dirs.Count == 0)
            Dirs.SetDelta(DeltaForTotalCount(dirs));
        else
            TRACE_E("CSalamanderDirectory::SetApproximateCount() Dirs.Count = " << Dirs.Count);
    }
}

void CSalamanderDirectory::ReleasePluginData(CPluginDataInterfaceEncapsulation& pluginData,
                                             BOOL releaseFiles, BOOL releaseDirs)
{
    SLOW_CALL_STACK_MESSAGE3("CSalamanderDirectory::ReleasePluginData(, %d, %d)",
                             releaseFiles, releaseDirs);
    if (releaseFiles)
        pluginData.ReleaseFilesOrDirs(&Files, FALSE);
    if (releaseDirs)
        pluginData.ReleaseFilesOrDirs(&Dirs, TRUE);
    int i;
    for (i = 0; i < SalamDirs.Count; i++)
    {
        CSalamanderDirectory* salDir = SalamDirs[i];
        if (salDir != NULL)
            salDir->ReleasePluginData(pluginData, releaseFiles, releaseDirs);
    }
}

CFilesArray*
CSalamanderDirectory::GetDirs(const char* path)
{
    CALL_STACK_MESSAGE2("CSalamanderDirectory::GetDirs(%s)", path);
    if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // some subdirectory
        {
            const char* s = path;
            while (*s != 0 && *s != '\\')
                s++;

            int i;
            for (i = 0; i < Dirs.Count; i++)
            {
                if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, path, (int)(s - path)) == 0)
                {
                    CSalamanderDirectory* salDir = SalamDirs[i];
                    if (salDir != NULL ||                    // allocated now
                        (salDir = AllocSalamDir(i)) != NULL) // or a new object was successfully allocated
                    {
                        return salDir->GetDirs(s);
                    }
                    else
                        return NULL; // low memory error (as if the directory did not exist)
                }
            }
        }
        else
            return &Dirs;
    }
    return NULL;
}

CFilesArray*
CSalamanderDirectory::GetFiles(const char* path)
{
    CALL_STACK_MESSAGE2("CSalamanderDirectory::GetFiles(%s)", path);
    if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // some subdirectory
        {
            const char* s = path;
            while (*s != 0 && *s != '\\')
                s++;

            int i;
            for (i = 0; i < Dirs.Count; i++)
            {
                if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, path, (int)(s - path)) == 0)
                {
                    CSalamanderDirectory* salDir = SalamDirs[i];
                    if (salDir != NULL ||                    // allocated now
                        (salDir = AllocSalamDir(i)) != NULL) // or a new object was successfully allocated
                    {
                        return salDir->GetFiles(s);
                    }
                    else
                        return NULL; // low memory error (as if the directory did not exist)
                }
            }
        }
        else
            return &Files;
    }
    return NULL;
}

const CFileData*
CSalamanderDirectory::GetUpperDir(const char* path)
{
    CALL_STACK_MESSAGE2("CSalamanderDirectory::GetUpperDir(%s)", path);
    if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // some subdirectory
        {
            const char* s = path;
            while (*s != 0 && *s != '\\')
                s++;

            int i;
            for (i = 0; i < Dirs.Count; i++)
            {
                if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, path, (int)(s - path)) == 0)
                {
                    if (*s == 0 || *(s + 1) == 0)
                        return &Dirs[i]; // last component of the path = the desired parent directory
                    else
                    {
                        CSalamanderDirectory* salDir = SalamDirs[i];
                        if (salDir != NULL ||                    // allocated now
                            (salDir = AllocSalamDir(i)) != NULL) // or a new object was successfully allocated
                        {
                            return salDir->GetUpperDir(s);
                        }
                        else
                            return NULL; // low memory error (as if the directory did not exist)
                    }
                }
            }
        }
        else
            return NULL; // returning NULL for root
    }
    return NULL; // for root and unknown paths we return NULL
}

CQuadWord
CSalamanderDirectory::GetSize(int* dirsCount, int* filesCount, TDirectArray<CQuadWord>* sizes)
{
    CALL_STACK_MESSAGE1("CSalamanderDirectory::GetSize(,)");
    CQuadWord size(0, 0);
    int i;
    for (i = 0; i < Files.Count; i++)
    {
        size += Files[i].Size;
        if (sizes != NULL)
            sizes->Add(Files[i].Size); // error handling is handled up to the output dialog level
    }
    if (filesCount != NULL)
        *filesCount += Files.Count;
    for (i = 0; i < SalamDirs.Count; i++)
    {
        CSalamanderDirectory* salDir = SalamDirs[i];
        if (salDir != NULL)
            size += salDir->GetSize(dirsCount, filesCount, sizes);
    }
    if (dirsCount != NULL)
        *dirsCount += SalamDirs.Count;
    return size;
}

CQuadWord
CSalamanderDirectory::GetDirSize(const char* path, const char* dirName, int* dirsCount,
                                 int* filesCount, TDirectArray<CQuadWord>* sizes)
{
    CALL_STACK_MESSAGE3("CSalamanderDirectory::GetDirSize(%s, %s, , ,)", path, dirName);
    if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // some subdirectory
        {
            const char* s = path;
            while (*s != 0 && *s != '\\')
                s++;

            int i;
            for (i = 0; i < Dirs.Count; i++)
            {
                if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, path, (int)(s - path)) == 0)
                {
                    CSalamanderDirectory* salDir = SalamDirs[i];
                    if (salDir != NULL)
                        return salDir->GetDirSize(s, dirName, dirsCount, filesCount, sizes);
                    else
                        return CQuadWord(0, 0); // does not contain anything, otherwise it would have been allocated already
                }
            }
        }
        else
        {
            int i;
            for (i = 0; i < Dirs.Count; i++)
            {
                if (SalDirStrCmp(Dirs[i].Name, dirName) == 0)
                {
                    CSalamanderDirectory* salDir = SalamDirs[i];
                    if (salDir != NULL)
                        return salDir->GetSize(dirsCount, filesCount, sizes);
                    else
                        return CQuadWord(0, 0); // does not contain anything, otherwise it would have been allocated already
                }
            }
            TRACE_E("Incorrect call to CSalamanderDirectory::GetDirSize() - directory does not exist!");
            return CQuadWord(0, 0); // not found
        }
    }
    return CQuadWord(0, 0);
}

CSalamanderDirectory*
CSalamanderDirectory::GetSalamanderDir(const char* path, BOOL readOnly)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE3("CSalamanderDirectory::GetSalamanderDir(%s, %d)", path, readOnly);
    if (path != NULL)
    {
        if (*path == '\\')
            path++;
        if (*path != 0) // some subdirectory
        {
            const char* s = path;
            while (*s != 0 && *s != '\\')
                s++;

            int i;
            for (i = 0; i < Dirs.Count; i++)
            {
                if (SalDirStrCmpEx(Dirs[i].Name, Dirs[i].NameLen, path, (int)(s - path)) == 0)
                {
                    CSalamanderDirectory* salDir = SalamDirs[i];
                    if (salDir != NULL)
                        return salDir->GetSalamanderDir(s, readOnly);
                    else // it is an empty directory
                    {
                        if (readOnly)
                            return &GlobalEmptySalDir; // only reading - return global empty directory
                        else                           // for writing
                        {
                            if ((salDir = AllocSalamDir(i)) != NULL) // we need to allocate a new object
                            {
                                return salDir->GetSalamanderDir(s, readOnly);
                            }
                            else
                                return NULL; // allocation error
                        }
                    }
                }
            }
        }
        else
            return this;
    }
    return NULL;
}

CSalamanderDirectory*
CSalamanderDirectory::GetSalamanderDir(int i)
{
    if (i >= 0 && i < SalamDirs.Count)
    {
        CSalamanderDirectory* salDir = SalamDirs[i];
        if (salDir == NULL)
            salDir = &GlobalEmptySalDir; // it is an empty directory - we will return the global empty directory
        return salDir;
    }
    else
        return NULL;
}

int CSalamanderDirectory::GetIndex(const char* dir)
{
    if (dir != NULL)
    {
        int i;
        for (i = 0; i < Dirs.Count; i++)
        {
            if (SalDirStrCmp(Dirs[i].Name, dir) == 0)
                return i;
        }
    }
    return -1; // not found
}

// ****************************************************************************

BOOL TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize, const char* messageTitle)
{
    CQuadWord freeSpace = MyGetDiskFreeSpace(path);
    if (freeSpace != CQuadWord(-1, -1) && freeSpace < totalSize)
    {
        char buf1[50];
        char buf2[50];
        char buf3[200];
        sprintf(buf3, LoadStr(IDS_NOTENOUGHSPACE),
                NumberToStr(buf1, totalSize),
                NumberToStr(buf2, freeSpace));
        return SalMessageBox(parent, buf3, messageTitle, MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) == IDYES;
    }
    return TRUE;
}
