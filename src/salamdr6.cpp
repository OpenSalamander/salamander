// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <Sddl.h>

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"
#include "tasklist.h"
#include "md5.h"

CProgressDlgArray ProgressDlgArray; // array of disk operations dialogs (only dialogs running in their own threads)

// section for calling GetNextFileNameForViewer, GetPreviousFileNameForViewer,
// IsFileNameForViewerSelected and SetSelectionOnFileNameForViewer
CRITICAL_SECTION FileNamesEnumSect;
// section for working with data related to enumeration (FileNamesEnumSources, FileNamesEnumData,
// FileNamesEnumDone, NextRequestUID and NextSourceUID)
CRITICAL_SECTION FileNamesEnumDataSect;

// array of resources for enumeration: even indexes (and zero): UID, odd indexes: HWND
TDirectArray<HWND> FileNamesEnumSources(10, 10);
// structure with request+results enumeration
CFileNamesEnumData FileNamesEnumData;
// the event is "signaled" once the source fills the result into FileNamesEnumData
HANDLE FileNamesEnumDone;
// next available UID request
int NextRequestUID = 0;
// next available UID resource
int NextSourceUID = 0;

HWND GetWndToFlash(HWND parent)
{
    HWND mainWnd = parent;
    if (parent != NULL)
    {
        HWND tmp;
        while ((tmp = ::GetParent(mainWnd)) != NULL && IsWindowEnabled(tmp))
            mainWnd = tmp;
        HWND foregrWnd = GetForegroundWindow();
        if (foregrWnd != mainWnd)
        {
            while ((tmp = ::GetParent(mainWnd)) != NULL)
                mainWnd = tmp;
            FlashWindow(mainWnd, TRUE);
        }
        else
            mainWnd = NULL;
    }
    return mainWnd;
}

BOOL CALLBACK CloseAllOwnedEnabledDialogsEnumProc(HWND wnd, LPARAM lParam)
{
    HWND parent = (HWND)lParam;
    LONG style = GetWindowLong(wnd, GWL_STYLE);
    if ((style & WS_CHILD) == 0 && IsWindowEnabled(wnd) && IsWindowVisible(wnd))
    {
        char clsName[200];
        if (GetClassName(wnd, clsName, _countof(clsName)) != 0 && strcmp(clsName, "#32770") == 0) // DIALOG class
        {
            HWND owner = wnd;
            while (1)
            {
                owner = IsWindow(owner) ? GetWindow(owner, GW_OWNER) : NULL;
                if (owner == NULL)
                    break;
                if (owner == parent)
                {
                    PostMessage(wnd, WM_CLOSE, 0, 0);
                    break;
                }
            }
        }
    }
    return TRUE; // iterate through all windows, there may be multiple dialogs from one owner next to each other
}

void CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid)
{
    if (!IsWindowEnabled(parent))
        EnumThreadWindows(tid == 0 ? GetCurrentThreadId() : tid, CloseAllOwnedEnabledDialogsEnumProc, (LPARAM)parent);
}

void InitFileNamesEnumForViewers()
{
    CALL_STACK_MESSAGE_NONE
    HANDLES(InitializeCriticalSection(&FileNamesEnumSect));
    HANDLES(InitializeCriticalSection(&FileNamesEnumDataSect));
    FileNamesEnumDone = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL)); // auto, nonsignaled
    if (FileNamesEnumDone == NULL)
        TRACE_E("Unable to create synchronization event for enumerating of file names for viewers!");
}

void ReleaseFileNamesEnumForViewers()
{
    CALL_STACK_MESSAGE_NONE
    if (FileNamesEnumDone != NULL)
        HANDLES(CloseHandle(FileNamesEnumDone));
    HANDLES(DeleteCriticalSection(&FileNamesEnumDataSect));
    HANDLES(DeleteCriticalSection(&FileNamesEnumSect));
}

BOOL IsFileEnumSourcePanel(int srcUID, int* panel)
{
    CALL_STACK_MESSAGE2("IsFileEnumSourcePanel(%d,)", srcUID);

    BOOL ret = FALSE;
    if (panel != NULL)
        *panel = -1;
    HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
    int i;
    for (i = 0; i < FileNamesEnumSources.Count; i += 2)
    {
        if ((int)(UINT_PTR)(FileNamesEnumSources[i]) == srcUID)
        {
            if (i + 1 < FileNamesEnumSources.Count &&
                MainWindow != NULL && MainWindow->LeftPanel != NULL && MainWindow->RightPanel != NULL) // a bit of paranoia
            {
                HWND hWnd = FileNamesEnumSources[i + 1];
                if (MainWindow->LeftPanel->HWindow == hWnd)
                {
                    ret = TRUE;
                    if (panel != NULL)
                        *panel = PANEL_LEFT;
                }
                if (MainWindow->RightPanel->HWindow == hWnd)
                {
                    ret = TRUE;
                    if (panel != NULL)
                        *panel = PANEL_RIGHT;
                }
            }
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
    return ret;
}

BOOL GetFileNameForViewer(CFileNamesEnumRequestType requestType, int srcUID, int* lastFileIndex, const char* lastFileName,
                          BOOL preferSelected, BOOL onlyAssociatedExtensions, char* fileName,
                          BOOL* noMoreFiles, BOOL* srcBusy, CPluginInterfaceAbstract* plugin,
                          BOOL* isFileSelected, BOOL select)
{
    CALL_STACK_MESSAGE9("GetFileNameForViewer(%d, %d, %d, %s, %d, %d, %s, , , %d)",
                        requestType, srcUID, *lastFileIndex, lastFileName, preferSelected,
                        onlyAssociatedExtensions, fileName, select);
    if (noMoreFiles != NULL)
        *noMoreFiles = FALSE;
    if (srcBusy != NULL)
        *srcBusy = FALSE;
    if (isFileSelected != NULL)
        *isFileSelected = FALSE;
    if (FileNamesEnumDone == NULL)
        return FALSE; // It probably will never happen, but we are still handling (error: "source does not exist")

    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&FileNamesEnumSect));

    HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
    int reqUID = FileNamesEnumData.RequestUID = NextRequestUID++;
    FileNamesEnumData.RequestType = requestType;
    FileNamesEnumData.SrcUID = srcUID;
    FileNamesEnumData.LastFileIndex = *lastFileIndex;
    lstrcpyn(FileNamesEnumData.LastFileName, lastFileName != NULL ? lastFileName : "", MAX_PATH);
    FileNamesEnumData.PreferSelected = preferSelected;
    FileNamesEnumData.OnlyAssociatedExtensions = onlyAssociatedExtensions;
    FileNamesEnumData.Plugin = plugin;
    FileNamesEnumData.FileName[0] = 0;
    FileNamesEnumData.TimedOut = FALSE;
    FileNamesEnumData.Found = FALSE;
    FileNamesEnumData.NoMoreFiles = FALSE;
    FileNamesEnumData.SrcBusy = FALSE;
    FileNamesEnumData.IsFileSelected = FALSE;
    FileNamesEnumData.Select = select;
    ResetEvent(FileNamesEnumDone);
    HWND hWnd = NULL;
    int i;
    for (i = 0; i < FileNamesEnumSources.Count; i += 2)
    {
        if ((int)(UINT_PTR)(FileNamesEnumSources[i]) == srcUID)
        {
            if (i + 1 < FileNamesEnumSources.Count)
                hWnd = FileNamesEnumSources[i + 1];
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));

    if (hWnd != NULL)
    {
        PostMessage(hWnd, WM_USER_ENUMFILENAMES, reqUID, 0);

        DWORD waitRes = WaitForSingleObject(FileNamesEnumDone, FILENAMESENUM_TIMEOUT);
        if (waitRes == WAIT_OBJECT_0) // done
        {
            HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
            *lastFileIndex = FileNamesEnumData.LastFileIndex;
            if (fileName != NULL)
                lstrcpyn(fileName, FileNamesEnumData.FileName, MAX_PATH);
            if (noMoreFiles != NULL)
                *noMoreFiles = FileNamesEnumData.NoMoreFiles;
            if (srcBusy != NULL)
                *srcBusy = FileNamesEnumData.SrcBusy;
            if (isFileSelected != NULL)
                *isFileSelected = FileNamesEnumData.IsFileSelected;
            ret = FileNamesEnumData.Found;
            HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
        }
        else // possible timeout
        {
            HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
            waitRes = WaitForSingleObject(FileNamesEnumDone, 0);
            if (waitRes == WAIT_OBJECT_0) // done (timeout was planned alarm, the message was delivered on time, only the search for the name was not completed)
            {
                *lastFileIndex = FileNamesEnumData.LastFileIndex;
                if (fileName != NULL)
                    lstrcpyn(fileName, FileNamesEnumData.FileName, MAX_PATH);
                if (noMoreFiles != NULL)
                    *noMoreFiles = FileNamesEnumData.NoMoreFiles;
                if (srcBusy != NULL)
                    *srcBusy = FileNamesEnumData.SrcBusy;
                if (isFileSelected != NULL)
                    *isFileSelected = FileNamesEnumData.IsFileSelected;
                ret = FileNamesEnumData.Found;
            }
            else // actual timeout (message delivery timeout to the source)
            {
                FileNamesEnumData.TimedOut = TRUE;
                if (srcBusy != NULL)
                    *srcBusy = TRUE;
            }
            HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
        }
    }

    HANDLES(LeaveCriticalSection(&FileNamesEnumSect));
    return ret;
}

BOOL GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                              BOOL preferSelected, BOOL onlyAssociatedExtensions, char* fileName,
                              BOOL* noMoreFiles, BOOL* srcBusy,
                              CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE_NONE
    return GetFileNameForViewer(fnertFindNext, srcUID, lastFileIndex, lastFileName, preferSelected,
                                onlyAssociatedExtensions, fileName, noMoreFiles,
                                srcBusy, plugin, NULL, FALSE);
}

BOOL GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                  BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                  char* fileName, BOOL* noMoreFiles, BOOL* srcBusy,
                                  CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE_NONE
    return GetFileNameForViewer(fnertFindPrevious, srcUID, lastFileIndex, lastFileName, preferSelected,
                                onlyAssociatedExtensions, fileName, noMoreFiles,
                                srcBusy, plugin, NULL, FALSE);
}

BOOL IsFileNameForViewerSelected(int srcUID, int lastFileIndex, const char* lastFileName,
                                 BOOL* isFileSelected, BOOL* srcBusy)
{
    CALL_STACK_MESSAGE_NONE
    return GetFileNameForViewer(fnertIsSelected, srcUID, &lastFileIndex, lastFileName, FALSE,
                                FALSE, NULL, NULL, srcBusy, NULL, isFileSelected, FALSE);
}

BOOL SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex, const char* lastFileName,
                                     BOOL select, BOOL* srcBusy)
{
    CALL_STACK_MESSAGE_NONE
    return GetFileNameForViewer(fnertSetSelection, srcUID, &lastFileIndex, lastFileName, FALSE,
                                FALSE, NULL, NULL, srcBusy, NULL, NULL, select);
}

void EnumFileNamesChangeSourceUID(HWND hWnd, int* srcUID)
{
    CALL_STACK_MESSAGE1("EnumFileNamesChangeSourceUID()");
    HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
    *srcUID = NextSourceUID++;
    int i;
    for (i = 1; i < FileNamesEnumSources.Count; i += 2)
    {
        if (FileNamesEnumSources[i] == hWnd)
        {
            FileNamesEnumSources[i - 1] = (HWND)(UINT_PTR)*srcUID;
            break;
        }
    }
    if (i >= FileNamesEnumSources.Count)
        TRACE_E("Incorrect call to EnumFileNamesChangeSourceUID(): hWnd is not in array of sources!");
    HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
}

void EnumFileNamesAddSourceUID(HWND hWnd, int* srcUID)
{
    CALL_STACK_MESSAGE1("EnumFileNamesAddSourceUID()");
    HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
    *srcUID = NextSourceUID++;
    FileNamesEnumSources.Add((HWND)(UINT_PTR)(*srcUID));
    if (FileNamesEnumSources.IsGood())
    {
        FileNamesEnumSources.Add(hWnd);
        if (!FileNamesEnumSources.IsGood())
        {
            FileNamesEnumSources.ResetState();
            FileNamesEnumSources.Delete(FileNamesEnumSources.Count - 1);
            if (!FileNamesEnumSources.IsGood())
                FileNamesEnumSources.ResetState();
        }
    }
    else
        FileNamesEnumSources.ResetState();
    HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
}

void EnumFileNamesRemoveSourceUID(HWND hWnd)
{
    CALL_STACK_MESSAGE1("EnumFileNamesRemoveSourceUID()");
    HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
    int i;
    for (i = 1; i < FileNamesEnumSources.Count; i += 2)
    {
        if (FileNamesEnumSources[i] == hWnd)
        {
            FileNamesEnumSources.Delete(i);
            if (!FileNamesEnumSources.IsGood())
                FileNamesEnumSources.ResetState();
            FileNamesEnumSources.Delete(i - 1);
            if (!FileNamesEnumSources.IsGood())
                FileNamesEnumSources.ResetState();
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
}

void AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                const char* value, BOOL caseSensitiveValue)
{
    CALL_STACK_MESSAGE1("AddValueToStdHistoryValues()");
    if (historyItemsCount <= 0 || historyArr == NULL || value == NULL)
    {
        TRACE_E("AddValueToStdHistoryValues(): Incorrect parameters!");
        return;
    }
    int from = -1;
    int i;
    for (i = 0; i < historyItemsCount; i++)
    {
        if (historyArr[i] != NULL &&
            (!caseSensitiveValue && StrICmp(historyArr[i], value) == 0 ||
             caseSensitiveValue && strcmp(historyArr[i], value) == 0))
        {
            from = i;
            break;
        }
    }
    if (from == -1 || from > 0)
    {
        if (from == -1)
            from = historyItemsCount - 1;
        char* text = (char*)malloc(strlen(value) + 1);
        if (text != NULL)
        {
            free(historyArr[from]);
            for (i = from - 1; i >= 0; i--)
                historyArr[i + 1] = historyArr[i];
            historyArr[0] = text;
            strcpy(historyArr[0], value);
        }
    }
}

void LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount)
{
    CALL_STACK_MESSAGE1("LoadComboFromStdHistoryValues()");
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; i < historyItemsCount; i++)
        if (historyArr[i] != NULL && strlen(historyArr[i]) > 0)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)historyArr[i]);
}

BOOL IsPathOnVolumeSupADS(const char* path, BOOL* isFAT32)
{
    CALL_STACK_MESSAGE1("IsPathOnVolumeSupADS()");
    DWORD fileSystemFlags;
    char fileSystemNameBuffer[100];
    if (isFAT32 != NULL)
        *isFAT32 = FALSE;
    if (!MyGetVolumeInformation(path, NULL, NULL, NULL, NULL, 0, NULL, NULL, &fileSystemFlags, fileSystemNameBuffer, 100))
    {
        TRACE_E("MyGetVolumeInformation failed for: " << path);
        return TRUE; // we will assume that the file system supports ADS, if not, something will fail later
    }
    if (isFAT32 != NULL)
        *isFAT32 = StrICmp(fileSystemNameBuffer, "FAT32") == 0;
    return (fileSystemFlags & FILE_NAMED_STREAMS) != 0 && StrICmp(fileSystemNameBuffer, "FAT") != 0 || // flag to support ADS (+ not related to FAT - incorrect ADS support message on Windows DFS server) or
           StrICmp(fileSystemNameBuffer, "NTFS") == 0 && fileSystemFlags == 0x1F;                      // NTFS from NT 4.0
}

//******************************************************************************
//
// PrintDiskSize
//

char* PrintDiskSize(char* buf, const CQuadWord& size2, int mode)
{
    CALL_STACK_MESSAGE3("PrintDiskSize(, %g, %d)", size2.GetDouble(), mode);
    CQuadWord size(size2);

    buf[0] = 0;
    if (mode == 1 || mode == 2)
    {
        char expanded[200];
        ExpandPluralString(expanded, 200, HLanguage != NULL ? LoadStr(IDS_PLURAL_X_BYTES) : "{!}%s byte{s|0||1|s}", 1, &size);
        char num[50];
        sprintf(buf, expanded, NumberToStr(num, size));
    }
    switch (mode)
    {
    case 0: //mode==0 "1.23 MB"
    case 1: //mode==1 "1 230 000 bytes, 1.23 MB"
    case 4: //mode==4; always at least 3 valid digits, for example "2.00 MB"
    {
        int i = 0;
        while (size > CQuadWord(0xFFFFFFFF, 0)) // First, we will reduce it to be easily converted to double
        {
            size /= CQuadWord(1024, 0); // integer division! (could also be done by shifting, but...)
            i++;
        }

        double sizeDouble = size.GetDouble(); // we will perform the rest of the calculation in double, because we are interested in decimal places
        for (; i < 6; i++)
        {
            if (sizeDouble >= 1023.5)
                sizeDouble /= 1024; // Double division!
            else
                break;
        }
        const char* s = NULL;
        switch (i)
        {
        case 0:
        {
            if (mode == 0 || mode == 4)
                s = HLanguage != NULL ? LoadStr(IDS_SIZE_B) : "B";
            break;
        }

        case 1:
            s = HLanguage != NULL ? LoadStr(IDS_SIZE_KB) : "KB";
            break;
        case 2:
            s = HLanguage != NULL ? LoadStr(IDS_SIZE_MB) : "MB";
            break;
        case 3:
            s = HLanguage != NULL ? LoadStr(IDS_SIZE_GB) : "GB";
            break;
        case 4:
            s = HLanguage != NULL ? LoadStr(IDS_SIZE_TB) : "TB";
            break;
        case 5:
            s = HLanguage != NULL ? LoadStr(IDS_SIZE_PB) : "PB";
            break;
        case 6:
            s = HLanguage != NULL ? LoadStr(IDS_SIZE_EB) : "EB";
            break;
        }

        if (s != NULL)
        {
            if (sizeDouble > 0.01)
                sizeDouble += 1E-7; // we take only the first three places, this measure will eliminate 0.9999999 errors
            else
                sizeDouble = 0;
            char n[30];
            if (sizeDouble >= 999.5 && sizeDouble < 1000)
                sizeDouble = 1000; // "rounding" is not performed automatically
            if (sizeDouble >= 1000)
                sprintf(n, "%.4g", sizeDouble);
            else
            {
                sprintf(n, "%.3g", sizeDouble);
                if (mode == 4)
                {
                    char* t = n + 1;
                    if (*t == 0)
                        strcpy(t, ".00"); // "2" -> "2.00"
                    else
                    {
                        if (*t == '.') // "2."
                        {
                            t += 2;
                            if (*t == 0)
                                strcpy(t, "0"); // "2.2" -> "2.20"
                        }
                        else
                        {
                            if (*++t == 0)
                                strcpy(t, ".0"); // "22" -> "22.0"
                        }
                    }
                }
            }
            PointToLocalDecimalSeparator(n, _countof(n));
            sprintf(buf + strlen(buf), "%s%s %s%s", (mode == 1) ? " (" : "", n, s, (mode == 1) ? ")" : "");
        }
        break;
    }

    case 3: //mode==3 (always in whole KB)
    {
        size += CQuadWord(1023, 0);
        size /= CQuadWord(1024, 0); // integer division! (could also be done by shifting, but...)
        NumberToStr(buf, size);
        strcat(buf, " ");
        strcat(buf, HLanguage != NULL ? LoadStr(IDS_SIZE_KB) : "KB");
        break;
    }
    }
    return buf;
}

//******************************************************************************
//
// PrintTimeLeft
//

char* PrintTimeLeft(char* buf, CQuadWord const& secs)
{
    CALL_STACK_MESSAGE1("PrintTimeLeft(,)");
    //  sprintf(buf, "%u:%02u:%02u", (int)(secs / CQuadWord(3600, 0)).Value,
    //          (int)((secs / CQuadWord(60, 0)) % CQuadWord(60, 0)).Value,
    //          (int)(secs % CQuadWord(60, 0)).Value);

    int s = (int)(secs % CQuadWord(60, 0)).Value;
    int m = (int)((secs / CQuadWord(60, 0)) % CQuadWord(60, 0)).Value;
    int h = (int)(secs / CQuadWord(3600, 0)).Value;
    int off = 0;
    if (h > 0)
    {
        int res = _snprintf_s(buf, 100, _TRUNCATE, ProgDlgHoursStr, h);
        if (res > 0)
            off += res;
    }
    if (m > 0)
    {
        if (off > 0 && off < 99)
            buf[off++] = ' ';
        int res = _snprintf_s(buf + off, 100 - off, _TRUNCATE, ProgDlgMinutesStr, m);
        if (res > 0)
            off += res;
    }
    if (s > 0 || off == 0)
    {
        if (off > 0 && off < 99)
            buf[off++] = ' ';
        int res = _snprintf_s(buf + off, 100 - off, _TRUNCATE, ProgDlgSecsStr, s);
        if (res > 0)
            off += res;
    }
    return buf;
}

//
// ****************************************************************************
// CNames
//

CNames::CNames()
    : Dirs(10, 50), Files(10, 300)
{
    CaseSensitive = FALSE;
    NeedSort = TRUE;
}

CNames::~CNames()
{
    Clear();
}

void CNames::SetCaseSensitive(BOOL caseSensitive)
{
    if (CaseSensitive != caseSensitive)
    {
        CaseSensitive = caseSensitive;
        if (Dirs.Count > 1 || Files.Count > 1)
            NeedSort = TRUE;
    }
}

void SortNames(char* files[], int left, int right)
{

LABEL_SortNames:

    int i = left, j = right;
    char* pivot = files[(i + j) / 2];

    do
    {
        while (StrICmp(files[i], pivot) < 0 && i < right)
            i++;
        while (StrICmp(pivot, files[j]) < 0 && j > left)
            j--;

        if (i <= j)
        {
            char* swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // We have replaced the following "nice" code with code significantly saving stack space (max. log(N) recursion depth)
    //  if (left < j) SortNames(files, left, j);
    //  if (i < right) SortNames(files, i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // both "halves" need to be sorted, so we will send the smaller one into recursion and process the other one using "goto"
            {
                SortNames(files, left, j);
                left = i;
                goto LABEL_SortNames;
            }
            else
            {
                SortNames(files, i, right);
                right = j;
                goto LABEL_SortNames;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortNames;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortNames;
        }
    }
}

void SortNamesCaseSensitive(char* files[], int left, int right)
{

LABEL_SortNamesCaseSensitive:

    int i = left, j = right;
    char* pivot = files[(i + j) / 2];

    do
    {
        while (strcmp(files[i], pivot) < 0 && i < right)
            i++;
        while (strcmp(pivot, files[j]) < 0 && j > left)
            j--;

        if (i <= j)
        {
            char* swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // We have replaced the following "nice" code with code significantly saving stack space (max. log(N) recursion depth)
    //  if (left < j) SortNamesCaseSensitive(files, left, j);
    //  if (i < right) SortNamesCaseSensitive(files, i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // both "halves" need to be sorted, so we will send the smaller one into recursion and process the other one using "goto"
            {
                SortNamesCaseSensitive(files, left, j);
                left = i;
                goto LABEL_SortNamesCaseSensitive;
            }
            else
            {
                SortNamesCaseSensitive(files, i, right);
                right = j;
                goto LABEL_SortNamesCaseSensitive;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortNamesCaseSensitive;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortNamesCaseSensitive;
        }
    }
}

BOOL FindNameInArray(TDirectArray<char*>* items, const char* name, BOOL caseSensitive, int* foundOnIndex)
{
    int l = 0, r = items->Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = caseSensitive ? strcmp(items->At(m), name) : StrICmp(items->At(m), name);
        if (res == 0)
        {
            if (foundOnIndex != NULL)
                *foundOnIndex = m;
            return TRUE; // found
        }
        else
        {
            if (res > 0)
            {
                if (l == r || l > m - 1)
                    return FALSE; // not found
                r = m - 1;
            }
            else
            {
                if (l == r)
                    return FALSE; // not found
                l = m + 1;
            }
        }
    }
}

void CNames::Sort()
{
    if (!NeedSort) // There is no need, we have it sorted
        return;

    if (Dirs.Count > 1)
    {
        if (CaseSensitive)
            SortNamesCaseSensitive(Dirs.GetData(), 0, Dirs.Count - 1);
        else
            SortNames(Dirs.GetData(), 0, Dirs.Count - 1);
    }

    if (Files.Count > 1)
    {
        if (CaseSensitive)
            SortNamesCaseSensitive(Files.GetData(), 0, Files.Count - 1);
        else
            SortNames(Files.GetData(), 0, Files.Count - 1);
    }

    NeedSort = FALSE;
}

void CNames::Clear()
{
    int i;
    for (i = 0; i < Dirs.Count; i++)
        free(Dirs[i]);
    Dirs.DestroyMembers();
    for (i = 0; i < Files.Count; i++)
        free(Files[i]);
    Files.DestroyMembers();
}

BOOL CNames::Add(BOOL nameIsDir, const char* name)
{
    char* dup = DupStr(name);
    if (dup == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    TDirectArray<char*>* items = nameIsDir ? &Dirs : &Files;

    items->Add(dup);
    if (!items->IsGood())
    {
        items->ResetState();
        return FALSE;
    }

    NeedSort = TRUE;

    return TRUE;
}

BOOL CNames::Contains(BOOL nameIsDir, const char* name, int* foundOnIndex)
{
    TDirectArray<char*>* items;
    if (foundOnIndex != NULL)
        *foundOnIndex = -1;
    if (nameIsDir)
    {
        if (Dirs.Count == 0)
            return FALSE;
        items = &Dirs;
    }
    else
    {
        if (Files.Count == 0)
            return FALSE;
        items = &Files;
    }

    if (NeedSort)
    {
        TRACE_E("CNames::Contains is called on unsorted data. Calling Sort() now.");
        Sort();
    }

    return FindNameInArray(items, name, CaseSensitive, foundOnIndex);
}

BOOL CNames::LoadFromClipboard(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CNamesFromClipboard::LoadFromClipboard()");

    Clear();

    char buff[MAX_PATH];
    buff[0] = 0;
    BOOL changePath = FALSE;

    if (OpenClipboard(hWindow))
    {
        const char* text = NULL;
        BOOL textIsAlloc = FALSE;
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (handle != NULL)
        {
            const WCHAR* textW = (const WCHAR*)HANDLES(GlobalLock(handle));
            if (textW != NULL)
            {
                SIZE_T size = GlobalSize(handle); // size in bytes
                if (size == 0)
                    TRACE_E("CNames::LoadFromClipboard(): unexpected situation: size == 0!");

                // for example, Miranda adds some Unicode data after the text, so
                // We prefer to find the end of the string terminated by zero ourselves, without exceeding it
                // end of the allocated memory block
                const WCHAR* s = textW;
                const WCHAR* end = textW + size / sizeof(WCHAR);
                while (s < end && *s != 0)
                    s++;
                size = s - textW; // now already in WCHARech

                text = ConvertAllocU2A(textW, (int)size);
                if (text != NULL)
                    textIsAlloc = TRUE;
                else
                    TRACE_E("CNames::LoadFromClipboard(): unable to convert unicode from clipboard to ANSI.");
                HANDLES(GlobalUnlock(handle));
            }
        }
        if (text == NULL) // probably not unicode, let's try ANSI (if it's unicode, ANSI is often broken - without diacritics)
        {
            handle = GetClipboardData(CF_TEXT);
            if (handle != NULL)
                text = (const char*)HANDLES(GlobalLock(handle));
        }

        if (text != NULL)
        {
            const char* textEnd;
            if (textIsAlloc)
                textEnd = text + strlen(text);
            else
            {
                SIZE_T size = GlobalSize(handle);
                if (size == 0)
                    TRACE_E("CNames::LoadFromClipboard(): unexpected situation: size == 0!");

                // for example, Miranda adds some Unicode data after the text, so
                // We prefer to find the end of the string terminated by zero ourselves, without exceeding it
                // end of the allocated memory block
                textEnd = text + size;
                const char* s = text;
                while (s < textEnd && *s != 0)
                    s++;
                textEnd = s;
            }

            // searching from the left for CR | LF | CRLF or end of memory
            // add found file/directory names to an array
            char name[2 * MAX_PATH];
            const char* s = text;
            const char* begin = s;
            while (s <= textEnd)
            {
                // !warning! 's' and 'begin' will point outside of valid memory at the end => UNCLEAN
                if (s >= textEnd || *s == '\r' || *s == '\n' || *s == 0)
                {
                    if (s - begin > 0)
                    {
                        if (s - begin < 2 * MAX_PATH - 1)
                        {
                            memcpy(name, begin, s - begin);
                            name[s - begin] = 0;
                            // Trim the backslash at the end of the path
                            char* end = name + (s - begin) - 1;
                            if (*end == '\\')
                            {
                                *end = 0;
                                end--;
                            }
                            // trim the waste at the end of the road
                            while (end > name && *end <= ' ')
                            {
                                *end = 0;
                                end--;
                            }
                            // trim full paths
                            while (end > name && *end != '\\')
                                end--;
                            if (*end == '\\')
                                end++;

                            // we will cut the waste at the beginning of the path
                            while (*end != 0 && *end <= ' ')
                                end++;

                            if (*end != 0)
                            {
                                if (!Add(FALSE, end))
                                    break;
                            }
                        }
                        else
                            TRACE_E("CNames::LoadFromClipboard(): path is too long, skipping.");
                    }
                    begin = s + 1; // we are at the terminator, we will move to its beginning
                }
                s++;
            }

            if (textIsAlloc)
                free((void*)text); // we are using an allocated ANSI copy of the unicode text from the clipboard
            else
                HANDLES(GlobalUnlock(handle)); // we are using the text directly from the clipboard (it is locked)
        }
        CloseClipboard();
    }
    else
        TRACE_E("CNames::LoadFromClipboard(): OpenClipboard() has failed!");

    return TRUE;
}

//******************************************************************************
//
// CDirectorySizes
//

CDirectorySizes::CDirectorySizes(const char* path, BOOL caseSensitive)
    : Names(20, 50)
{
    Path = DupStr(path);
    if (Path == NULL)
        TRACE_E(LOW_MEMORY); // IsGood returns FALSE
    CaseSensitive = caseSensitive;
    NeedSort = FALSE;
}

CDirectorySizes::~CDirectorySizes()
{
    Clean();

    if (Path != NULL)
    {
        free(Path);
        Path = NULL;
    }
}

void CDirectorySizes::Clean()
{
    int i;
    for (i = 0; i < Names.Count; i++)
        free(Names[i]);
    Names.DestroyMembers();
}

BOOL CDirectorySizes::Add(const char* name, const CQuadWord* size)
{
    int nameLen = (int)strlen(name);
    char* item = (char*)malloc(nameLen + 1 + sizeof(CQuadWord));
    if (item == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    memcpy(item, name, nameLen + 1);
    CQuadWord* itemSize = (CQuadWord*)(item + nameLen + 1);
    *itemSize = *size;

    Names.Add(item);
    if (!Names.IsGood())
    {
        TRACE_E(LOW_MEMORY);
        Names.ResetState();
        return FALSE;
    }

    NeedSort = TRUE;

    return TRUE;
}

const CQuadWord*
CDirectorySizes::GetSize(const char* name)
{
    int index = GetIndex(name);
    if (index == -1)
        return NULL;
    else
    {
        const char* item = Names[index];
        CQuadWord* itemSize = (CQuadWord*)(item + strlen(item) + 1);
        return itemSize;
    }
}

void CDirectorySizes::Sort()
{
    if (!NeedSort) // There is no need, we have it sorted
        return;

    SortNames(Names.GetData(), 0, Names.Count - 1);

    NeedSort = FALSE;
}

int CDirectorySizes::GetIndex(const char* name)
{
    if (Names.Count == 0)
        return -1;

    if (NeedSort)
    {
        TRACE_E("CDirectorySizes::GetIndex is called on unsorted data. Calling Sort() now.");
        Sort();
    }

    int l = 0, r = Names.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = CaseSensitive ? strcmp(Names[m], name) : StrICmp(Names[m], name);
        if (res == 0)
            return m; // found
        else
        {
            if (res > 0)
            {
                if (l == r || l > m - 1)
                    return -1; // not found
                r = m - 1;
            }
            else
            {
                if (l == r)
                    return -1; // not found
                l = m + 1;
            }
        }
    }
}

//******************************************************************************
//
// CDirectorySizesHolder
//

CDirectorySizesHolder::CDirectorySizesHolder()
{
    int i;
    for (i = 0; i < DIRECOTRY_SIZES_COUNT; i++)
        Items[i] = NULL;
    ItemsCount = 0;
}

CDirectorySizesHolder::~CDirectorySizesHolder()
{
    Clean();
}

void CDirectorySizesHolder::Clean()
{
    int i;
    for (i = 0; i < DIRECOTRY_SIZES_COUNT; i++)
    {
        if (Items[i] != NULL)
        {
            delete Items[i];
            Items[i] = NULL;
        }
    }
    ItemsCount = 0;
}

CDirectorySizes*
CDirectorySizesHolder::Add(const char* path)
{
    // we will try to find the desired path
    int index = GetIndex(path);
    if (index != -1)
        return Items[index];

    // path does not exist, allocating it
    CDirectorySizes* item = new CDirectorySizes(path, FALSE);
    if (item == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    if (!item->IsGood())
    {
        delete item;
        return FALSE;
    }

    if (ItemsCount >= DIRECOTRY_SIZES_COUNT)
    {
        // array is full
        // we remove the item at index zero
        delete Items[0];
        // and we shake the field
        memmove(Items, Items + 1, sizeof(void*) * (DIRECOTRY_SIZES_COUNT - 1));
        // we will append the item to the end
        Items[ItemsCount] = item;
    }
    else
    {
        // we will append the item to the end
        Items[ItemsCount] = item;
        ItemsCount++;
    }

    return item;
}

CDirectorySizes*
CDirectorySizesHolder::Get(const char* path)
{
    int index = GetIndex(path);
    if (index != -1)
        return Items[index];
    else
        return NULL;
}

int CDirectorySizesHolder::GetIndex(const char* path)
{
    int i;
    for (i = 0; i < ItemsCount; i++)
    {
        if (stricmp(Items[i]->Path, path) == 0)
            return i;
    }
    return -1;
}

BOOL CDirectorySizesHolder::Store(CFilesWindow* panel)
{
    CDirectorySizes* item = Add(panel->GetPath());
    if (item == NULL)
        return FALSE;

    // we will delete the previous directory + size
    item->Clean();

    // add directories for which the size is known
    int first = 0;
    if (panel->Dirs->Count > 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0)
        first = 1;
    int i;
    for (i = first; i < panel->Dirs->Count; i++)
    {
        CFileData* dir = &panel->Dirs->At(i);
        if (dir->SizeValid == 1)
        {
            if (!item->Add(dir->Name, &dir->Size))
                return FALSE;
        }
    }

    return TRUE;
}

void CDirectorySizesHolder::Restore(CFilesWindow* panel)
{
    int index = GetIndex(panel->GetPath());
    if (index == -1)
        return;

    CDirectorySizes* item = Items[index];

    int first = 0;
    if (panel->Dirs->Count > 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0)
        first = 1;
    int i;
    for (i = first; i < panel->Dirs->Count; i++)
    {
        CFileData* dir = &panel->Dirs->At(i);
        if (dir->SizeValid == 0)
        {
            const CQuadWord* size = item->GetSize(dir->Name);
            if (size != NULL)
            {
                dir->SizeValid = 1;
                dir->Size = *size;
            }
        }
    }
}

//******************************************************************************
//
// CProgressDlgArray
//

CProgressDlgArray::CProgressDlgArray() : Dlgs(5, 10)
{
    HANDLES(InitializeCriticalSection(&Monitor));
}

CProgressDlgArray::~CProgressDlgArray()
{
    if (Dlgs.Count > 0)
        TRACE_E("Unexpected situation in CProgressDlgArray::~CProgressDlgArray(): some dialog with disk operation still exists!");
    HANDLES(DeleteCriticalSection(&Monitor));
}

CProgressDlgArrItem*
CProgressDlgArray::PrepareNewDlg()
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::PrepareNewDlg()");

    HANDLES(EnterCriticalSection(&Monitor));
    CProgressDlgArrItem* ret = new CProgressDlgArrItem;
    if (ret != NULL)
    {
        Dlgs.Add(ret);
        if (!Dlgs.IsGood())
        {
            Dlgs.ResetState();
            delete ret;
            ret = NULL;
        }
    }
    else
        TRACE_E(LOW_MEMORY);
    HANDLES(LeaveCriticalSection(&Monitor));
    return ret;
}

void CProgressDlgArray::SetDlgData(CProgressDlgArrItem* dlg, HANDLE dlgThread, HWND dlgWindow)
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::SetDlgData()");

    HANDLES(EnterCriticalSection(&Monitor));
    if (dlgThread != NULL)
        dlg->DlgThread = dlgThread;
    if (dlgWindow != NULL)
        dlg->DlgWindow = dlgWindow;
    HANDLES(LeaveCriticalSection(&Monitor));
}

void CProgressDlgArray::RemoveDlg(CProgressDlgArrItem* dlg)
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::RemoveDlg()");

    HANDLES(EnterCriticalSection(&Monitor));
    int i;
    for (i = Dlgs.Count - 1; i >= 0; i--)
    {
        if (Dlgs[i] == dlg)
        {
            Dlgs.Delete(i);
            if (!Dlgs.IsGood())
                Dlgs.ResetState();
            break;
        }
    }
    if (i < 0)
        TRACE_E("CProgressDlgArray::RemoveDlg(): specified dialog was not found!");
    HANDLES(LeaveCriticalSection(&Monitor));
}

int CProgressDlgArray::RemoveFinishedDlgs()
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::RemoveFinishedDlgs()");

    HANDLES(EnterCriticalSection(&Monitor));
    int i;
    for (i = Dlgs.Count - 1; i >= 0; i--)
    {
        CProgressDlgArrItem* dlg = Dlgs[i];
        DWORD code;
        if (dlg->DlgThread != NULL &&
            (!GetExitCodeThread(dlg->DlgThread, &code) || code != STILL_ACTIVE))
        { // found completed thread, we will remove it from the array
            HANDLES(CloseHandle(dlg->DlgThread));
            Dlgs.Delete(i);
            if (!Dlgs.IsGood())
                Dlgs.ResetState();
        }
    }
    int count = Dlgs.Count;
    HANDLES(LeaveCriticalSection(&Monitor));
    return count;
}

void CProgressDlgArray::ClearDlgWindow(HWND hdlg)
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::ClearDlgWindow()");

    HANDLES(EnterCriticalSection(&Monitor));
    if (hdlg != NULL)
    {
        int i;
        for (i = Dlgs.Count - 1; i >= 0; i--)
        {
            CProgressDlgArrItem* dlg = Dlgs[i];
            if (dlg->DlgWindow == hdlg)
            {
                dlg->DlgWindow = NULL;
                break;
            }
        }
        if (i < 0)
            TRACE_E("CProgressDlgArray::ClearDlgWindow(): specified dialog was not found!");
    }
    HANDLES(LeaveCriticalSection(&Monitor));
}

HWND CProgressDlgArray::GetNextOpenedDlg(int* index)
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::GetNextOpenedDlg()");

    HANDLES(EnterCriticalSection(&Monitor));
    HWND ret = NULL;
    if (index != NULL)
    {
        if (*index >= Dlgs.Count)
            *index = 0;
        int attempts = 0;
        while (*index < Dlgs.Count)
        {
            ret = Dlgs[*index]->DlgWindow;
            if (ret != NULL)
            {
                (*index)++;
                break; // found an open dialog, returning it
            }
            else // We encountered a closed dialog, we need to skip it
            {
                if (++attempts >= Dlgs.Count)
                    break; // all dialogs in the array are closed
                (*index)++;
                if (*index >= Dlgs.Count)
                    *index = 0;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&Monitor));
    return ret;
}

void CProgressDlgArray::PostCancelToAllDlgs()
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::PostCancelToAllDlgs()");

    HANDLES(EnterCriticalSection(&Monitor));
    int i;
    for (i = Dlgs.Count - 1; i >= 0; i--)
    {
        CProgressDlgArrItem* dlg = Dlgs[i];
        if (dlg->DlgWindow != NULL)
            PostMessage(dlg->DlgWindow, WM_USER_CANCELPROGRDLG, 0, 0);
    }
    HANDLES(LeaveCriticalSection(&Monitor));
}

void CProgressDlgArray::PostIconChange()
{
    CALL_STACK_MESSAGE1("CProgressDlgArray::PostIconChange()");

    HANDLES(EnterCriticalSection(&Monitor));
    int i;
    for (i = Dlgs.Count - 1; i >= 0; i--)
    {
        CProgressDlgArrItem* dlg = Dlgs[i];
        if (dlg->DlgWindow != NULL)
            PostMessage(dlg->DlgWindow, WM_USER_PROGRDLG_UPDATEICON, 0, 0);
    }
    HANDLES(LeaveCriticalSection(&Monitor));
}

//****************************************************************************
//
// CShellExecuteWnd
//

CShellExecuteWnd::CShellExecuteWnd()
    : CWindow(ooStatic)
{
    CanClose = FALSE;
}

CShellExecuteWnd::~CShellExecuteWnd()
{
    CanClose = TRUE;
    if (HWindow != NULL)
        DestroyWindow(HWindow);
}

HWND CShellExecuteWnd::Create(HWND hParent, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    if (HWindow != NULL)
    {
        TRACE_E("CShellExecuteWnd::Create HWindow=0x" << HWindow);
    }
    else
    {
        CanClose = FALSE;
        char buff[2 * MAX_PATH];
        _vsnprintf_s(buff, _TRUNCATE, format, args);

        // Retrieve the size of the parent window, as some users complained that emails were
        // opened from SS (via Mozilla) are displayed in a small window; some shell extensions probably
        // berou velikost CMINVOKECOMMANDINFOEX::hwnd v potaz pri konstrukci sveho okna a 1x1 pixel nebyla
        // optimal size
        RECT r;
        if (!IsWindow(hParent) || !GetClientRect(hParent, &r))
        {
            // hopefully a good default...
            r.right = 800;
            r.bottom = 600;
        }

        // The window will be stretched over the MainWindow/Findu surface and must be transparent (otherwise the dialog background will not be rendered)
        // Example: if I throw out WS_EX_TRANSPARENT, I open Find and put Delete (into the trash), just restart afterwards
        // Confirmation dialog for deleting to the trash and the background of the Find window will not be redrawn underneath it (it will be there
        // display this shell window, which has suppressed WM_ERASEBKGND/WM_PAINT
        CWindow::CreateEx(WS_EX_TRANSPARENT,
                          SHELLEXECUTE_CLASSNAME,
                          buff,
                          WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE, // without WS_CLIPSIBLINGS the main window flickered
                          0, 0, r.right, r.bottom,
                          hParent,
                          (HMENU)0,
                          HInstance,
                          this);
    }
    va_end(args);
    if (HWindow != NULL)
        return HWindow;
    else
        return hParent; // in case of an error, return at least the right parent
}

LRESULT
CShellExecuteWnd::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CShellExecuteWnd::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        // The window will be inserted below the other children, but to be safe, we will disable erase
        return TRUE;
    }

    case WM_DESTROY:
    {
        if (!CanClose)
        {
            MSG msg; // we are emptying the message queue (WMP9 buffered Enter and unlocked OK for us)
            // while (PeekMessage(&msg, HWindow, 0, 0, PM_REMOVE));  // Petr: replaced just by discarding keyboard messages (without TranslateMessage and DispatchMessage there is a risk of an infinite loop, discovered during unloading of Automation with memory leaks, before displaying the message box about leaks, an infinite loop occurred, WM_PAINT messages were constantly added to the queue and we were discarding them again)
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;

            MSGBOXEX_PARAMS params;
            memset(&params, 0, sizeof(params));
            params.HParent = HWindow;
            params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION;
            params.Caption = SALAMANDER_TEXT_VERSION;
            params.Text = LoadStr(IDS_SHELLEXTBREAK2);
            SalMessageBoxEx(&params);

            // Let's break
            char text[200];
            GetWindowText(HWindow, text, 200);
            sprintf(BugReportReasonBreak, "Some faulty shell extension has destroyed our window.\r\n%s", text);
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());

            // Freeze this thread
            while (1)
                Sleep(1000);
            /*          // bug report creation started in a separate thread
        // now we will ensure that this thread is blocked as long as the bug report dialog is open
        // this thread will freeze at the following macro due to the variables DontSuspend and ExceptionExists
        CALL_STACK_MESSAGE1("CShellExecuteWnd::WindowProc: LOCK");*/
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

struct EnumWndStruct
{
    char* Iterator; // add another window from this point
    int Remains;    // how many characters are left until the end of the buffer?
    int Count;
};

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    // Is it a window with class == SHELLEXECUTE_CLASSNAME?
    char className[100];
    if (GetClassName(hwnd, className, 100) > 0 && stricmp(className, SHELLEXECUTE_CLASSNAME) == 0)
    {
        EnumWndStruct* data = (EnumWndStruct*)lParam;
        // title of the window holds the desired string
        if (data->Remains > 2)
        {
            if (GetWindowTextLength(hwnd) > 0)
            {
                strcpy(data->Iterator, "\r\n"); // We don't have to worry about buffer overflow, we have a reserve of three characters
                data->Remains -= 2;
                data->Iterator += 2;
                int chars = GetWindowText(hwnd, data->Iterator, data->Remains);
                if (chars > 0)
                {
                    data->Iterator += chars;
                    data->Remains -= chars;
                }
            }
        }
        data->Count++;
    }
    return TRUE; // we continue searching, we want all windows
}

int EnumCShellExecuteWnd(HWND hParent, char* text, int textMax)
{
    // enumerate all child windows of hParent (including sub-children)
    EnumWndStruct data;
    data.Iterator = text;
    data.Remains = textMax - 3; // reserve
    data.Count = 0;
    EnumChildWindows(hParent, EnumChildProc, (LPARAM)&data);
    *data.Iterator = 0; // terminator
    return data.Count;
}

int IsFileLink(const char* fileExtension)
{
    // Convert extension characters to lowercase and determine if it is a link
    char lowerExt[7];
    char* dstExt = lowerExt;
    if (*fileExtension != 0)
        *dstExt++ = LowerCase[*fileExtension++];
    if (*fileExtension != 0)
        *dstExt++ = LowerCase[*fileExtension++];
    if (*fileExtension != 0)
        *dstExt++ = LowerCase[*fileExtension++];
    if (*fileExtension != 0)
        return 0; // extension is longer than three characters => cannot be .lnk, .url or .pif
    *((DWORD*)dstExt) = 0;
    return (*(DWORD*)lowerExt == *(DWORD*)"lnk" ||
            *(DWORD*)lowerExt == *(DWORD*)"pif" ||
            *(DWORD*)lowerExt == *(DWORD*)"url")
               ? 1
               : 0;
}

BOOL IsSambaDrivePath(const char* path)
{
    char root[MAX_PATH];
    GetRootPath(root, path);

    DWORD dummy1, flags;
    char fsName[MAX_PATH];
    BOOL sambaDrive = FALSE;
    if (GetVolumeInformation(root, NULL, 0, NULL, &dummy1, &flags, fsName, MAX_PATH))
    {
        return StrICmp(fsName, "NTFS") == 0 && (flags & FS_UNICODE_STORED_ON_DISK) == 0;
    }
    return FALSE;
}

BOOL AddToListOfNames(char** list, char* listEnd, const char* name, int nameLen)
{
    if (strchr(name, ' ') != NULL) // if it contains a space, put it in quotes
    {
        if (*list + nameLen + 2 /* within quotation marks*/ <= listEnd)
        {
            *(*list)++ = '"';
            memcpy(*list, name, nameLen);
            *list += nameLen;
            *(*list)++ = '"';
        }
        else
            return FALSE;
    }
    else
    {
        if (*list + nameLen <= listEnd)
        {
            memcpy(*list, name, nameLen);
            *list += nameLen;
        }
        else
            return FALSE;
    }
    return TRUE;
}

CTargetPathState GetTargetPathState(CTargetPathState upperDirState, const char* targetPath)
{
    switch (upperDirState)
    {
    case tpsUnknown:
    {
        DWORD attr = SalGetFileAttributes(targetPath);
        if (attr == INVALID_FILE_ATTRIBUTES)
        {
            TRACE_E("GetTargetPathState(): unexpected situation, target path should always exists!");
            return tpsNotEncryptedNotExisting; // if the path really does not exist, nothing will be done anyway + if only attributes cannot be read, we assume a path without Encrypted attributes
        }
        if (attr & FILE_ATTRIBUTE_ENCRYPTED)
            return tpsEncryptedExisting;
        else
            return tpsNotEncryptedExisting;
    }

    case tpsEncryptedExisting:
    case tpsNotEncryptedExisting:
    {
        DWORD attr = SalGetFileAttributes(targetPath);
        if (attr == INVALID_FILE_ATTRIBUTES) // the next subdirectory no longer exists, let's inherit the Encrypted attribute
            return upperDirState == tpsEncryptedExisting ? tpsEncryptedNotExisting : tpsNotEncryptedNotExisting;
        if (attr & FILE_ATTRIBUTE_ENCRYPTED)
            return tpsEncryptedExisting;
        else
            return tpsNotEncryptedExisting;
    }

    case tpsEncryptedNotExisting:
    case tpsNotEncryptedNotExisting:
        return upperDirState;
    }
    TRACE_E("GetTargetPathState(): unexpected situation, unknown value of upperDirState!");
    return tpsUnknown;
}

BOOL SafeGetOpenFileName(LPOPENFILENAME lpofn)
{
    BOOL ret = GetOpenFileName(lpofn);
    if (!ret && FNERR_INVALIDFILENAME == CommDlgExtendedError())
    {
        // Windows refuse to open a dialog for a path like "C:\" or for a non-existent path.
        // In this case, we force Documents
        char initDir[MAX_PATH];
        const char* oldInitDir = lpofn->lpstrInitialDir;
        lpofn->lpstrInitialDir = initDir;
        if (!GetMyDocumentsOrDesktopPath(initDir, MAX_PATH))
            strcpy(initDir, "");
        strcpy(lpofn->lpstrFile, "");
        ret = GetOpenFileName(lpofn);
        lpofn->lpstrInitialDir = oldInitDir;
    }
    if (!ret && CommDlgExtendedError() != 0 /* only if it's not Cancel in the dialog*/)
        TRACE_E("Cannot open OpenFile dialog box. CommDlgExtendedError()=" << CommDlgExtendedError());
    return ret;
}

BOOL SafeGetSaveFileName(LPOPENFILENAME lpofn)
{
    BOOL ret = GetSaveFileName(lpofn);
    if (!ret && FNERR_INVALIDFILENAME == CommDlgExtendedError())
    {
        // Windows refuse to open a dialog for a path like "C:\" or for a non-existent path.
        // In this case, we force Documents
        char initDir[MAX_PATH];
        const char* oldInitDir = lpofn->lpstrInitialDir;
        lpofn->lpstrInitialDir = initDir;
        if (!GetMyDocumentsOrDesktopPath(initDir, MAX_PATH))
            strcpy(initDir, "");
        strcpy(lpofn->lpstrFile, "");
        ret = GetSaveFileName(lpofn);
        lpofn->lpstrInitialDir = oldInitDir;
    }
    if (!ret && CommDlgExtendedError() != 0 /* only if it's not Cancel in the dialog*/)
        TRACE_E("Cannot open SaveFile dialog box. CommDlgExtendedError()=" << CommDlgExtendedError());
    return ret;
}

void GetIfPathIsInaccessibleGoTo(char* path, BOOL forceIsMyDocs)
{
    if (forceIsMyDocs || Configuration.IfPathIsInaccessibleGoToIsMyDocs)
    {
        if (!GetMyDocumentsOrDesktopPath(path, MAX_PATH))
        {
            char winPath[MAX_PATH];
            if (GetWindowsDirectory(winPath, MAX_PATH) != 0)
                GetRootPath(path, winPath);
            else
                strcpy(path, "C:\\");
        }
    }
    else
    {
        lstrcpyn(path, Configuration.IfPathIsInaccessibleGoTo, MAX_PATH);
        if (path[0] != 0 && path[1] == ':')
        {
            path[0] = UpperCase[path[0]];
            if (path[2] == 0)
            { // "C:" -> "C:\"
                path[2] = '\\';
                path[3] = 0;
            }
        }
    }
}

HICON SalLoadImage(int vistaResID, int otherResID, int cx, int cy, UINT flags)
{
    // JRYFIXME - convert to call SalLoadIcon
    return SalLoadIcon(ImageResDLL, vistaResID, cx);
    //return (HICON)HANDLES(LoadImage(ImageResDLL, MAKEINTRESOURCE(vistaResID), IMAGE_ICON, cx, cy, flags));
}

HICON LoadArchiveIcon(int cx, int cy, UINT flags)
{
    // JRYFIXME - convert to call SalLoadIcon
    return SalLoadIcon(ImageResDLL, 174, cx);
    //return (HICON)HANDLES(LoadImage(ImageResDLL, MAKEINTRESOURCE(174), IMAGE_ICON, cx, cy, flags));
}

BOOL DuplicateBackslashes(char* buffer, int bufferSize)
{
    if (buffer == NULL)
    {
        TRACE_E("Unexpected situation (1) in DuplicateBackslashes()");
        return FALSE;
    }
    char* s = buffer;
    int l = (int)strlen(buffer);
    if (l >= bufferSize)
    {
        TRACE_E("Unexpected situation (2) in DuplicateBackslashes()");
        return FALSE;
    }
    BOOL ret = TRUE;
    while (*s != 0)
    {
        if (*s == '\\')
        {
            if (l + 1 < bufferSize)
            {
                memmove(s + 1, s, l - (s - buffer) + 1); // double the '\\'
                l++;
                s++;
            }
            else // Doesn't fit, we'll trim the buffer
            {
                ret = FALSE;
                memmove(s + 1, s, l - (s - buffer)); // double '\\', remove one character
                buffer[l] = 0;
                s++;
            }
        }
        s++;
    }
    return ret;
}

BOOL DuplicateDollars(char* buffer, int bufferSize)
{
    if (buffer == NULL)
    {
        TRACE_E("Unexpected situation (1) in DuplicateDollars()");
        return FALSE;
    }
    char* s = buffer;
    int l = (int)strlen(buffer);
    if (l >= bufferSize)
    {
        TRACE_E("Unexpected situation (2) in DuplicateDollars()");
        return FALSE;
    }
    BOOL ret = TRUE;
    while (*s != 0)
    {
        if (*s == '$')
        {
            if (l + 1 < bufferSize)
            {
                memmove(s + 1, s, l - (s - buffer) + 1); // double the '$'
                l++;
                s++;
            }
            else // Doesn't fit, we'll trim the buffer
            {
                ret = FALSE;
                memmove(s + 1, s, l - (s - buffer)); // double '$', trim by one character
                buffer[l] = 0;
                s++;
            }
        }
        s++;
    }
    return ret;
}

BOOL AddDoubleQuotesIfNeeded(char* buf, int bufSize)
{
    char* beg = buf;
    while (*beg != 0 && *beg <= ' ')
        beg++;
    char* end = beg + strlen(beg);
    char* bufEnd = end;
    while (end > beg && *(end - 1) <= ' ')
        end--;
    if (end > beg && (*beg != '"' || *(end - 1) != '"'))
    {
        char* sp = beg;
        while (*++sp > ' ')
            ;
        if (sp < end) // name contains at least one space, quotes need to be added
        {
            if ((bufEnd - buf) + 2 >= bufSize)
                return FALSE; // Not enough space in the buffer
            memmove(end + 2, end, (bufEnd - end) + 1);
            memmove(beg + 1, beg, end - beg);
            *beg = '"';
            *(end + 1) = '"';
        }
    }
    return TRUE;
}

BOOL GetStringSid(LPTSTR* stringSid)
{
    *stringSid = NULL;

    HANDLE hToken = NULL;
    DWORD dwBufferSize = 0;
    PTOKEN_USER pTokenUser = NULL;

    // Open the access token associated with the calling process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TRACE_E("OpenProcessToken failed.");
        return FALSE;
    }

    // get the size of the memory buffer needed for the SID
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize);

    pTokenUser = (PTOKEN_USER)malloc(dwBufferSize);
    memset(pTokenUser, 0, dwBufferSize);

    // Retrieve the token information in a TOKEN_USER structure.
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
    {
        TRACE_E("GetTokenInformation failed.");
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    if (!IsValidSid(pTokenUser->User.Sid))
    {
        TRACE_E("The owner SID is invalid.\n");
        free(pTokenUser);
        return FALSE;
    }

    // The caller must release the allocated memory using LocalFree, see MSDN
    ConvertSidToStringSid(pTokenUser->User.Sid, stringSid);

    free(pTokenUser);

    return TRUE;
}

BOOL GetSidMD5(BYTE* sidMD5)
{
    ZeroMemory(sidMD5, 16);

    HANDLE hToken = NULL;
    DWORD dwBufferSize = 0;
    PTOKEN_USER pTokenUser = NULL;

    // Open the access token associated with the calling process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TRACE_E("OpenProcessToken failed.");
        return FALSE;
    }

    // get the size of the memory buffer needed for the SID
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize);

    pTokenUser = (PTOKEN_USER)malloc(dwBufferSize);
    memset(pTokenUser, 0, dwBufferSize);

    // Retrieve the token information in a TOKEN_USER structure.
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
    {
        TRACE_E("GetTokenInformation failed.");
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    if (!IsValidSid(pTokenUser->User.Sid))
    {
        TRACE_E("The owner SID is invalid.\n");
        free(pTokenUser);
        return FALSE;
    }

    MD5 context;
    context.update((BYTE*)pTokenUser->User.Sid, GetLengthSid(pTokenUser->User.Sid));
    context.finalize();
    memcpy(sidMD5, context.digest, 16);

    free(pTokenUser);

    return TRUE;
}

// For more information see:
//   http://www.codeguru.com/cpp/w-p/win32/tutorials/print.php/c4545 (A NotQuiteNullDacl Class)
//   http://forums.microsoft.com/msdn/ShowPost.aspx?PostID=748596&SiteID=1 (already solves Vista)
//   Programming Server-Side Applications for Microsoft Windows 2000, Richter/Clark, Microsoft Press 2000, Chapter 10, pp 458-460
//   Ask Dr. Gui #49 (I can't find it properly online, so I'm inserting it here):
// Mutex Madness
// Dear Dr. GUI:
// I'm a French engineer and my English isn't perfect so I hope that you understand my question.
// I have a DLL that creates a mutex. I have some problems synchronizing all processes that use my DLL. I create the mutex with code that looks like the following:
// ghMutexExe = CreateMutex(NULL, FALSE, "APPLICOM_IO_MUTEX");
// When I use my DLL with an application that is running in Real time priority, I can't run another application that uses the same DLL. When I use the function:
// ghMutexExe = CreateMutex(NULL, FALSE, "APPLICOM_IO_MUTEX");
// It returns NULL (ghMutexExe = NULL), and GetLastError returns 5 (Access is denied. ERROR_ACCESS_DENIED).
// Can you help me?
// Bertrand Lauret
//
// Dr. GUI replies:
// Your English is just fine. (Dr. GUI is just glad you didn't ask to get my reply in French.) Most people do not know this, but the good doctor is bilingual. He speaks English and C++.
// It is certainly possible to have problems with thread synchronization due to differing priorities of threads. However, it is very unlikely that you would receive an ERROR_ACCESS_DENIED when trying to obtain a handle to an existing mutex because of the priority class of the creating process.
// Typically, ERROR_ACCESS_DENIED is returned as a result of failure because of the security implications of the function being called. Let me describe a scenario where this could happen with Microsoft Windows NT or Windows 2000:
// Process A is running as a service (perhaps in real time, perhaps not) and creates a named mutex passing NULL as the first parameter indicating default security for the object.
// Process B is launched by the interactive user and attempts to obtain a handle to the named mutex through a similar call to CreateMutex. This call fails with ERROR_ACCESS_DENIED because the default security of the service excludes all but the local system for ALL_ACCESS security to the object. This process does not have access even if it is running as an administrator of the system.
// This type of failure is the result of a combination of points:
// Most services are installed in the local system account and run with special security rights as a result.
// Processes running in the local system account grant GENERIC_ALL access to other processes running in the local system, and READ_CONTROL, GENERIC_EXECUTE, and GENERIC_READ access to members of the Administrators group. All other access to the object by any other users or groups is denied.
// All calls to CreateMutex implicitly request MUTEX_ALL_ACCESS for the object in question. An interactive user does not have the rights required to obtain a handle to an object created from the local system security context as a result.
// There are several solutions to this problem:
// You can set the security descriptor in your call to CreateMutex to contain a "NULL DACL." An object with NULL-DACL security grants all access to everyone, regardless of security context. One downside of this approach is that the object is now completely unsecured. In fact, a malicious application could obtain a handle to a named object created with a NULL DACL and change its security access such that other processes are unable to use the object, effectively ruining the object. PLEASE NOTE: The doctor seriously advises against this "solution."
// A second option would be to create a security descriptor that explicitly grants the necessary rights to the built-in group: Everyone. In the case of a mutex, this would be MUTEX_ALL_ACCESS. This is preferable because it will not allow malicious (or buggy) software to affect other software's access to the object.
// You can also choose to explicitly allow access to the user accounts that will need to use the object. This type of explicit security can be good but comes with the disadvantage of needing to know who will need access at the time the object is created.
// My preference in cases of creating objects for general availability to users of the system is the second. Regardless of which approach you choose, you have the additional choice of whether to apply the security descriptor to each object as it is created, or to change the default security for your process by changing the token's default DACL. In this case, you would continue to pass NULL for the security parameter when creating an object.
// Dr. GUI thinks that in your case you should only set the security for specific objects because you are developing a DLL, which may not want to change the security for every object in the process.
// The following function wraps CreateMutex with the additional functionality of creating security that is more relaxed than is the default for a service:
/*      // assign all possible rights to the mutex (for example, to allow opening between AsAdmin and User accounts)
    // cleaner would be to call the ObtainAccessableMutex() function, see its detailed comment
    SECURITY_ATTRIBUTES secAttr;
    char secDesc[ SECURITY_DESCRIPTOR_MIN_LENGTH ];
    secAttr.nLength = sizeof(secAttr);
    secAttr.bInheritHandle = FALSE;
    secAttr.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    // give the security descriptor a NULL DACL, done using the  "TRUE, (PACL)NULL" here
    SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, 0, FALSE);*/

/*  // According to http://forums.microsoft.com/msdn/ShowPost.aspx?PostID=748596&SiteID=1
// integrity level should be set under Vista, but I couldn't reproduce the issue on Vista/Server2008
// so for now I'm leaving it only in the comments until we encounter it
//
// Windows Integrity Mechanism Design
// http://msdn.microsoft.com/en-us/library/bb625963.aspx
if (windowsVistaAndLater) // FIXME: I haven't encountered the need to handle integrity level on Vista (between AdAdmin / normal application)
{
  PSECURITY_DESCRIPTOR pSD;
  ConvertStringSecurityDescriptorToSecurityDescriptor(
      "S:(ML;;NW;;;LW)", // this means "low integrity"
      SDDL_REVISION_1,
      &pSD,
      NULL);
  PACL pSacl = NULL;                  // not allocated
  BOOL fSaclPresent = FALSE;
  BOOL fSaclDefaulted = FALSE;
  GetSecurityDescriptorSacl(
      pSD,
      &fSaclPresent,
      &pSacl,
      &fSaclDefaulted);
  SetSecurityDescriptorSacl(secAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
}*/

SECURITY_ATTRIBUTES* CreateAccessableSecurityAttributes(SECURITY_ATTRIBUTES* sa, SECURITY_DESCRIPTOR* sd,
                                                        DWORD allowedAccessMask, PSID* psidEveryone, PACL* paclNewDacl)
{
    SID_IDENTIFIER_AUTHORITY siaWorld = SECURITY_WORLD_SID_AUTHORITY;
    int nAclSize;

    *psidEveryone = NULL;
    *paclNewDacl = NULL;

    // Create the everyone SID
    if (!AllocateAndInitializeSid(&siaWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AllocateAndInitializeSid() failed!");
        goto ErrorExit;
    }

    nAclSize = GetLengthSid(psidEveryone) * 2 + sizeof(ACCESS_ALLOWED_ACE) + sizeof(ACCESS_DENIED_ACE) + sizeof(ACL);
    *paclNewDacl = (PACL)LocalAlloc(LPTR, nAclSize);
    if (*paclNewDacl == NULL)
    {
        TRACE_E("CreateAccessableSecurityAttributes(): LocalAlloc() failed!");
        goto ErrorExit;
    }
    if (!InitializeAcl(*paclNewDacl, nAclSize, ACL_REVISION))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): InitializeAcl() failed!");
        goto ErrorExit;
    }
    if (!AddAccessDeniedAce(*paclNewDacl, ACL_REVISION, WRITE_DAC | WRITE_OWNER, *psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AddAccessDeniedAce() failed!");
        goto ErrorExit;
    }
    if (!AddAccessAllowedAce(*paclNewDacl, ACL_REVISION, allowedAccessMask, *psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AddAccessAllowedAce() failed!");
        goto ErrorExit;
    }
    if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): InitializeSecurityDescriptor() failed!");
        goto ErrorExit;
    }
    if (!SetSecurityDescriptorDacl(sd, TRUE, *paclNewDacl, FALSE))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): SetSecurityDescriptorDacl() failed!");
        goto ErrorExit;
    }
    sa->nLength = sizeof(SECURITY_ATTRIBUTES);
    sa->bInheritHandle = FALSE;
    sa->lpSecurityDescriptor = sd;
    return sa;

ErrorExit:
    if (*paclNewDacl != NULL)
    {
        LocalFree(*paclNewDacl);
        *paclNewDacl = NULL;
    }
    if (*psidEveryone != NULL)
    {
        FreeSid(*psidEveryone);
        *psidEveryone = NULL;
    }
    return NULL;
}

//****************************************************************************
//
// GetProcessIntegrityLevel (taken from MSDN)
// If successful, it returns TRUE and fills the DWORD pointed to by 'integrityLevel'.
// otherwise (in case of failure or under operating systems other than Vista) returns FALSE
//

BOOL GetProcessIntegrityLevel(DWORD* integrityLevel)
{
    HANDLE hToken;
    HANDLE hProcess;

    DWORD dwLengthNeeded;
    DWORD dwError = ERROR_SUCCESS;

    PTOKEN_MANDATORY_LABEL pTIL = NULL;
    DWORD dwIntegrityLevel;

    BOOL ret = FALSE;

    if (WindowsVistaAndLater) // Integrity levels were introduced since Windows Vista
    {
        hProcess = GetCurrentProcess();
        if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
        {
            // Get the Integrity level.
            if (!GetTokenInformation(hToken, (_TOKEN_INFORMATION_CLASS)25 /*TokenIntegrityLevel*/, NULL, 0, &dwLengthNeeded))
            {
                dwError = GetLastError();
                if (dwError == ERROR_INSUFFICIENT_BUFFER)
                {
                    pTIL = (PTOKEN_MANDATORY_LABEL)LocalAlloc(0, dwLengthNeeded);
                    if (pTIL != NULL)
                    {
                        if (GetTokenInformation(hToken, (_TOKEN_INFORMATION_CLASS)25 /*TokenIntegrityLevel*/, pTIL, dwLengthNeeded, &dwLengthNeeded))
                        {
                            dwIntegrityLevel = *GetSidSubAuthority(pTIL->Label.Sid, (DWORD)(UCHAR)(*GetSidSubAuthorityCount(pTIL->Label.Sid) - 1));
                            ret = TRUE;

                            /*                if (dwIntegrityLevel == SECURITY_MANDATORY_LOW_RID)
              {
               // Low Integrity
               wprintf(L"Low Process");
              }
              else if (dwIntegrityLevel >= SECURITY_MANDATORY_MEDIUM_RID && 
                   dwIntegrityLevel < SECURITY_MANDATORY_HIGH_RID)
              {
               // Medium Integrity
               wprintf(L"Medium Process");
              }
              else if (dwIntegrityLevel >= SECURITY_MANDATORY_HIGH_RID)
              {
               // High Integrity
               wprintf(L"High Integrity Process");
              }
              else if (dwIntegrityLevel >= SECURITY_MANDATORY_SYSTEM_RID)
              {
               // System Integrity
               wprintf(L"System Integrity Process");
              }*/
                        }
                        LocalFree(pTIL);
                    }
                }
            }
            CloseHandle(hToken);
        }
    }
    if (ret)
        *integrityLevel = dwIntegrityLevel;
    else
        *integrityLevel = 0;
    return ret;
}

LONG SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData)
{
    DWORD dataBufSize = lpData == NULL || lpcbData == NULL ? 0 : *lpcbData;
    LONG ret = RegQueryValue(hKey, lpSubKey, lpData, lpcbData);
    if (lpcbData != NULL &&
        (ret == ERROR_MORE_DATA || lpData == NULL && ret == ERROR_SUCCESS))
    {
        (*lpcbData)++; // let's rather talk about a possible additional null-terminator
    }
    if (ret == ERROR_SUCCESS && lpData != NULL)
    {
        if (*lpcbData < 1 || ((char*)lpData)[*lpcbData - 1] != 0)
        {
            if ((DWORD)*lpcbData < dataBufSize) // Only REG_SZ and REG_EXPAND_SZ values are crawling here, so one null-terminator is enough
            {
                ((char*)lpData)[*lpcbData] = 0;
                (*lpcbData)++;
            }
            else // lack of space for null-terminator in buffer
            {
                (*lpcbData)++; // Let's ask for the necessary null-terminator
                return ERROR_MORE_DATA;
            }
        }
    }
    return ret;
}

LONG SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
    DWORD dataBufSize = lpData == NULL ? 0 : *lpcbData;
    DWORD type = REG_NONE;
    LONG ret = RegQueryValueEx(hKey, lpValueName, lpReserved, &type, lpData, lpcbData);
    if (lpType != NULL)
        *lpType = type;
    if (type == REG_SZ || type == REG_MULTI_SZ || type == REG_EXPAND_SZ)
    {
        if (hKey != HKEY_PERFORMANCE_DATA &&
            lpcbData != NULL &&
            (ret == ERROR_MORE_DATA || lpData == NULL && ret == ERROR_SUCCESS))
        {
            (*lpcbData) += type == REG_MULTI_SZ ? 2 : 1; // let's rather talk about any additional null-terminator(s)
            return ret;
        }
        if (ret == ERROR_SUCCESS && lpData != NULL)
        {
            if (*lpcbData < 1 || ((char*)lpData)[*lpcbData - 1] != 0)
            {
                if (*lpcbData < dataBufSize)
                {
                    ((char*)lpData)[*lpcbData] = 0;
                    (*lpcbData)++;
                }
                else // lack of space for null-terminator in buffer
                {
                    (*lpcbData) += type == REG_MULTI_SZ ? 2 : 1; // Let's ask for the necessary null-terminator(s)
                    return ERROR_MORE_DATA;
                }
            }
            if (type == REG_MULTI_SZ && (*lpcbData < 2 || ((char*)lpData)[*lpcbData - 2] != 0))
            {
                if (*lpcbData < dataBufSize)
                {
                    ((char*)lpData)[*lpcbData] = 0;
                    (*lpcbData)++;
                }
                else // Insufficient space for the second null-terminator in the buffer
                {
                    (*lpcbData)++; // Let's ask for the necessary null-terminator
                    return ERROR_MORE_DATA;
                }
            }
        }
    }
    return ret;
}

//******************************************************************************
//
// SalGetProcessId
//
// Functional also under W2K (SDK7.1 seems that GetProcessId is present under W2K,
// but it's an error and SDK8 already requires _WIN32_WINNT >= 0x0501 for GetProcessId
//
// As long as we want to support Windows 2000, we cannot use GetProcessId() or ZwQueryInformationProcess
// statically link.
//

DWORD SalGetProcessId(HANDLE hProcess)
{
    // They call us at the beginning of the process, so we cannot rely on global variables set sometime later, like
    // is WindowsVistaAndLater or NtDLL
    static BOOL osDetected = FALSE;
    static BOOL vistaAndLater = FALSE;
    if (!osDetected)
    {
        vistaAndLater = SalIsWindowsVersionOrGreater(6, 0, 0);
        osDetected = TRUE;
    }
    DWORD ret = 0xffffffff;
    if (vistaAndLater)
    {
        // The function was already in XPSP1, but there ZwQueryInformationProcess still worked 100%, see below
        typedef DWORD(WINAPI * PGetProcessId)(IN HANDLE Process);
        HINSTANCE hDLL = NOHANDLES(LoadLibrary("kernel32.dll"));
        if (hDLL != NULL)
        {
            PGetProcessId pGetProcessId = (PGetProcessId)GetProcAddress(hDLL, "GetProcessId");
            if (pGetProcessId != NULL)
                ret = pGetProcessId(hProcess);
            else
                TRACE_E("SalGetProcessId() failed while calling GetProcessId()");
            NOHANDLES(FreeLibrary(hDLL));
        }
    }
    else
    {
// For W2K and XP, we will use the "undocumented" API http://msdn.microsoft.com/en-us/library/windows/desktop/ms687420%28v=vs.85%29.aspx
// where Microsoft threatens to eventually cancel it
#if !defined PROCESSINFOCLASS
        typedef LONG PROCESSINFOCLASS;
#endif
#if !defined PPEB
        typedef struct _PEB* PPEB;
#endif
#if !defined PROCESS_BASIC_INFORMATION
        typedef struct _PROCESS_BASIC_INFORMATION
        {
            PVOID Reserved1;
            PPEB PebBaseAddress;
            PVOID Reserved2[2];
            ULONG_PTR UniqueProcessId;
            PVOID Reserved3;
        } PROCESS_BASIC_INFORMATION;
#endif;
        typedef NTSTATUS(WINAPI * PFN_ZWQUERYINFORMATIONPROCESS)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

        HINSTANCE hDLL = NOHANDLES(LoadLibrary("ntdll.dll"));
        if (hDLL != NULL)
        {
            PFN_ZWQUERYINFORMATIONPROCESS fnProcInfo = PFN_ZWQUERYINFORMATIONPROCESS(GetProcAddress(hDLL, "ZwQueryInformationProcess"));
            if (fnProcInfo != NULL)
            {
                PROCESS_BASIC_INFORMATION pbi;
                ZeroMemory(&pbi, sizeof(PROCESS_BASIC_INFORMATION));
                if (fnProcInfo(hProcess, 0, &pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL) == 0) // STATUS_SUCCESS
                    ret = (DWORD)pbi.UniqueProcessId;
                else
                    TRACE_E("SalGetProcessId() failed while calling ZwQueryInformationProcess()");
            }
            else
                TRACE_E("SalGetProcessId() failed to get ZwQueryInformationProcess() proc address");
            NOHANDLES(FreeLibrary(hDLL));
        }
        else
            TRACE_E("SalGetProcessId() failed to load ntdll.dll");
    }
    return ret;
}
