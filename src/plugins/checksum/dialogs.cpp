// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "checksum.h"
#include "checksum.rh"
#include "checksum.rh2"
#include "lang\lang.rh"
#include "dialogs.h"
#include "misc.h"

CWindowQueue ModelessQueue("CheckSum Modeless Windows");  // seznam vsech nemodalnich oken
CThreadQueue ThreadQueue("CheckSum Dialogs and Workers"); // seznam vsech threadu oken a vypoctu

#define BUFSIZE (4 * 65536) // velikost bufferu pro cteni

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#define CM_REMOVEITEM 100

// i pro soubory male velikosti chceme posun progress bar
// puvodne mela konstanta hodnotu 1, ale progress se choval hodne nelinearne pokud se pocitaly tisice
// malych souboru namixovane se soubory 1MB+, experimentalne jsem dosel k teto hodnote
#define FILE_SIZE_FIX 10000

#define IDT_UPDATEUI 10
#define IDT_UPDATESUI_PERIOD 100 // [ms]

#define IDT_RESENDCLOSE 11

// ****************************************************************************************************
//
//  CSFVMD5Dialog
//

CSFVMD5Dialog::CSFVMD5Dialog(int id, HWND parent, BOOL alwaysOnTop) : CDialog(HLanguage, id, NULL), hParent(parent), FileList(1000, 1000)
{
    HANDLES(InitializeCriticalSection(&DataCS));
    hThread = NULL;
    iThreadID = 0;
    bThreadRunning = FALSE;
    bTerminateThread = FALSE;
    bAlwaysOnTop = alwaysOnTop;
    ScheduledScrollIndex = 0;
    ScrollIndex = 0;
    CurrentSize.Value = 0;
    ScheduledCurrentSize.Value = 0;
    DirtyRowMin = -1;
    DirtyRowMax = -1;
    ReadingDirectories = FALSE;
    StopReadingDirectories = FALSE;
}

CSFVMD5Dialog::~CSFVMD5Dialog()
{
    HANDLES(DeleteCriticalSection(&DataCS));
}

void CSFVMD5Dialog::InitList(int columns[], int widths[], int numcols, SHashInfo* pHashInfo)
{
    CALL_STACK_MESSAGE2("CSFVMD5Dialog::InitList( , , %d)", numcols);

    hImg = ImageList_Create(16, 16, SalamanderGeneral->GetImageListColorFlags() | ILC_MASK, 0, 1);
    static int id[] = {IDI_FILE1, IDI_FILE2, IDI_FILE3, IDI_FILE4};
    int i;
    for (i = 0; i < SizeOf(id); i++)
    {
        HICON hIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(id[i]), IMAGE_ICON,
                                       0, 0, SalamanderGeneral->GetIconLRFlags());
        ImageList_AddIcon(hImg, hIcon);
        DestroyIcon(hIcon);
    }

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif
    hList = GetDlgItem(HWindow, IDC_LIST_FILES);
    ListView_SetExtendedListViewStyleEx(hList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    if (SalIsWindowsVersionOrGreater(6, 0, 0)) // WindowsVistaAndLater: Vista and later (CommonControls 6.0+)
        ListView_SetExtendedListViewStyleEx(hList, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
    ListView_SetImageList(hList, hImg, LVSIL_SMALL); // o destrukci se postara imagelist

    int j;
    for (j = 0; j < numcols; j++)
    {
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.pszText = LoadStr(columns[j]);
        lvc.cchTextMax = (int)_tcslen(lvc.pszText);
        lvc.cx = widths[j];
        lvc.fmt = (j == 1) ? LVCFMT_RIGHT : 0;
        ListView_InsertColumn(hList, j, &lvc);
    }
    if (pHashInfo)
    {
        int k, l;
        for (k = l = 0; k < HT_COUNT; k++)
            if (pHashInfo[k].bCalculate)
            {
                LVCOLUMN lvc;
                lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                lvc.pszText = LoadStr(pHashInfo[k].idColumnHeader);
                lvc.cchTextMax = (int)_tcslen(lvc.pszText);
                lvc.cx = widths[numcols + k];
                lvc.fmt = 0;
                ListView_InsertColumn(hList, numcols + l, &lvc);
                l++;
            }
    }
}

void CSFVMD5Dialog::InitLayout(int id[], int n, int m)
{
    CALL_STACK_MESSAGE3("CSFVMD5Dialog::InitLayout( , %d, %d)", n, m);

    RECT client, rc;
    GetClientRect(HWindow, &client);
    GetWindowRect(HWindow, &rc);
    minX = rc.right - rc.left;
    minY = rc.bottom - rc.top;
    GetWindowRect(hList, &rc);
    listX = client.right - (rc.right - rc.left);
    listY = client.bottom - (rc.bottom - rc.top);
    GetClientRect(HWindow, &client);
    int i;
    for (i = 0; i < n; i++)
    {
        GetWindowRect(GetDlgItem(HWindow, id[i]), &rc);
        ScreenToClient(HWindow, (LPPOINT)&rc);
        ctrlX[i] = (i < m) ? client.right - rc.left : rc.left;
        ctrlY[i] = client.bottom - rc.top;
    }
}

void CSFVMD5Dialog::RecalcLayout(int cx, int cy, int id[], int n, int m)
{
    CALL_STACK_MESSAGE5("CSFVMD5Dialog::RecalcLayout(%d, %d, , %d, %d)", cx, cy, n, m);

    SetWindowPos(hList, NULL, 0, 0, cx - listX, cy - listY, SWP_NOZORDER | SWP_NOMOVE);
    int i;
    for (i = 0; i < n; i++)
        SetWindowPos(GetDlgItem(HWindow, id[i]), 0, (i < m) ? cx - ctrlX[i] : ctrlX[i], cy - ctrlY[i], 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void CSFVMD5Dialog::SaveSizes(int sizes[], int numcols, SHashInfo* pHashInfo)
{
    CALL_STACK_MESSAGE2("CSFVMD5Dialog::SaveSizes( , %d)", numcols);

    RECT rc;
    GetWindowRect(HWindow, &rc);
    sizes[0] = rc.right - rc.left;
    sizes[1] = rc.bottom - rc.top;
    int i;
    for (i = 0; i < numcols; i++)
        sizes[i + 2] = ListView_GetColumnWidth(hList, i);

    if (pHashInfo)
    {
        int k = 0, l = numcols;
        for (; k < HT_COUNT; k++)
            if (pHashInfo[k].bCalculate)
                sizes[2 + numcols + k] = ListView_GetColumnWidth(hList, l++);
    }
}

void CSFVMD5Dialog::ConvertPath(char* str, char from, char to)
{
    while (NULL != (str = _tcschr(str, from)))
    {
        *str = to;
    }
}

void CSFVMD5Dialog::OnThreadEnd()
{
    CALL_STACK_MESSAGE1("CSFVMD5Dialog::OnThreadEnd()");
    SetDlgItemText(HWindow, IDC_BUTTON_CLOSE, LoadStr(IDS_CLOSE));
    ShowWindow(GetDlgItem(HWindow, IDC_LABEL), SW_HIDE);
    ShowWindow(GetDlgItem(HWindow, IDC_PROGRESS), SW_HIDE);
    bThreadRunning = FALSE;
    if (ScrollIndex < FileList.Count) // thread uz nepocita (bezet jeste muze), netreba synchronizace
    {                                 // zajistime update posledni "pocitane" polozky, aby nezustala calculating / verifying ...
        SetRowsDirty(ScrollIndex, ScrollIndex);
    }
}

void CSFVMD5Dialog::SetRowsDirty(int firstRow, int lastRow)
{
    if (lastRow < firstRow)
        TRACE_E("CSFVMD5Dialog::SetRowsDirty(): lastRow < firstRow");
    if (DirtyRowMin == -1 || firstRow < DirtyRowMin)
        DirtyRowMin = firstRow;
    if (lastRow > DirtyRowMax)
        DirtyRowMax = lastRow;
}

void CSFVMD5Dialog::SetItemTextAndIcon(int row, int col, const char* text, int icon)
{
    // CALL_STACK_MESSAGE4("CSFVMD5Dialog::SetItemTextAndIcon(%d, %d, , %d)", row, col, icon);
    FILELISTITEM* item = FileList[row]; // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (index je OK)

    // hashe a ikona synchronizaci nevyzaduji, kdyz bezi pracovni thread, cte thread dialogu jen z indexu
    // pred ScrollIndex (ten je maximalne ScheduledScrollIndex), a pracovni thread zapisuje jen do indexu
    // rovneho ScheduledScrollIndex, tedy nemuze se to potkat
    if (text != NULL)
    {
        if (col < 2 || col - 2 >= HT_COUNT)
            TRACE_E("Wrong col: " << col);
        else
        {
            if (item->Hashes[col - 2] != NULL)
                free(item->Hashes[col - 2]);
            item->Hashes[col - 2] = _strdup(text);
        }
    }
    if (icon != -1)
        item->IconIndex = icon;
}

void CSFVMD5Dialog::GetItemText(int row, int col, char* text, int textMax)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
        // CALL_STACK_MESSAGE4("CSFVMD5Dialog::GetItemText(%d, %d, , %d)", row, col, textMax);
        LVITEM lvi;
    lvi.mask = LVIF_TEXT;
    lvi.iItem = row;
    lvi.iSubItem = col;
    lvi.pszText = text;
    lvi.cchTextMax = textMax;
    ListView_GetItem(hList, &lvi);
}

void CSFVMD5Dialog::IncreaseProgress(const CQuadWord& delta)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
    // CALL_STACK_MESSAGE1("CSFVMD5Dialog::IncreaseProgress()");
    EnterDataCS();
    ScheduledCurrentSize += delta; // je 64-bit hodnota, proto nutna synchronizace v 32-bit kodu
    LeaveDataCS();
}

void CSFVMD5Dialog::DeleteItem(int index)
{
    CALL_STACK_MESSAGE2("CSFVMD5Dialog::DeleteItem(%d)", index);
    // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (nesmi se volat ani tato funkce)
    if (bThreadRunning)
        TRACE_E("CSFVMD5Dialog::DeleteItem(): unexpected situation: worker thread should not be running!");
    if (FileList.Count > 0 && index < FileList.Count)
    {
        FileList.Delete(index);
        if (index >= FileList.Count)
            index = FileList.Count - 1;
        ListView_SetItemCountEx(hList, FileList.Count, 0);
        if (index >= 0)
            ListView_SetItemState(hList, index, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    }
}

void CSFVMD5Dialog::ScrollToItem(int i)
{
    CALL_STACK_MESSAGE2("CSFVMD5Dialog::ScrollToItem(%d)", i);
    EnterDataCS();
    ScheduledScrollIndex = i;
    LeaveDataCS();
}

void CSFVMD5Dialog::AddFileListItem(const char* name, CQuadWord size, BOOL fileExist)
{
    // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (nesmi se volat ani tato funkce)
    if (bThreadRunning)
        TRACE_E("CSFVMD5Dialog::AddFileListItem(): unexpected situation: worker thread should not be running!");
    FILELISTITEM* item = new FILELISTITEM();
    item->Name = _strdup(name);
    item->Size = size;
    item->FileExist = fileExist;
    FileList.Add(item);
}

INT_PTR CSFVMD5Dialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
        //CALL_STACK_MESSAGE4("CSFVMD5Dialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

        switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        bScrollToItem = TRUE;
        bDisableNotification = FALSE;

        if (bAlwaysOnTop) // always-on-top osetrime aspon "staticky" (neni v system menu)
            SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        CSFVMD5ListView* lv = new CSFVMD5ListView(this);
        if (lv != NULL)
        {
            lv->AttachToWindow(GetDlgItem(HWindow, IDC_LIST_FILES));
            if (lv->HWindow == NULL)
                delete lv; // nepripojeno = neprobehne automaticka dealokace
        }

        SetForegroundWindow(HWindow);

        SetTimer(HWindow, IDT_UPDATEUI, IDT_UPDATESUI_PERIOD, NULL);
        break;
    }

    case WM_USER_ENDWORK:
        OnThreadEnd();
        return TRUE;

    case WM_TIMER:
    {
        if (wParam == IDT_RESENDCLOSE) // mame si poslat WM_CLOSE, blokujici dialog uz je snad sestreleny
        {
            KillTimer(HWindow, IDT_RESENDCLOSE);
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            return TRUE;
        }
        if (wParam == IDT_UPDATEUI)
        {
            EnterDataCS();
            int progress = -1; // -1 = nenastavovat progress
            if (totalSize.Value != 0 && ScheduledCurrentSize != CurrentSize)
            {
                CurrentSize = ScheduledCurrentSize;
                progress = CurrentSize < totalSize ? (int)((CurrentSize * CQuadWord(1024, 0)) / totalSize).Value : 1024;
            }
            int i = -1; // -1 = nevolat ensure visible
            if (ScheduledScrollIndex != ScrollIndex)
            {
                // v tento okamzik se priznaji napocitana data az do indexu ScrollIndex,
                // primo ScrollIndex je calculating / verifying, za nim je nevypocteno
                // (i kdyz realne uz to muze byt napocitane, ukaze se to proste az
                // v dalsim kole)
                // ukazeme nove priznana napocitana data + novy ScrollIndex se musi prekreslit
                // (vypsat calculating / verifying)
                SetRowsDirty(ScrollIndex, ScheduledScrollIndex);
                ScrollIndex = ScheduledScrollIndex;
                if (bScrollToItem)
                    i = ScrollIndex;
            }
            int dirtyRowMin = DirtyRowMin;
            int dirtyRowMax = DirtyRowMax;
            if (DirtyRowMin != -1)
            {
                DirtyRowMin = -1;
                DirtyRowMax = -1;
            }
            LeaveDataCS();

            if (progress != -1)
            {
                HWND pr = GetDlgItem(HWindow, IDC_PROGRESS);
                SendMessage(pr, PBM_SETPOS, progress + 1, 0);
                // hack for progress bar lag in aero: http://stackoverflow.com/questions/22469876/progressbar-lag-when-setting-position-with-pbm-setpos
                SendMessage(pr, PBM_SETPOS, progress, 0);
            }
            if (dirtyRowMin != -1)
                ListView_RedrawItems(hList, dirtyRowMin, dirtyRowMax);
            if (i != -1)
                ListView_EnsureVisible(hList, i, FALSE);

            return TRUE;
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (bDisableNotification)
            break;

        if (wParam == IDC_LIST_FILES)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if (!(nmhi->uOldState & LVIS_SELECTED) && nmhi->uNewState & LVIS_SELECTED)
                    bScrollToItem = FALSE; // uzivatel zmenil selection -> zakazeme autoscroll
            }
            }
        }
        break;
    }

    case WM_SIZING:
    {
        RECT* lprc = (RECT*)lParam;
        SIZE size = {lprc->right - lprc->left, lprc->bottom - lprc->top};
        if (size.cx < minX)
        {
            if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
                lprc->left = lprc->right - minX;
            else
                lprc->right = lprc->left + minX;
        }
        if (size.cy < minY)
        {
            if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
                lprc->top = lprc->bottom - minY;
            else
                lprc->bottom = lprc->top + minY;
        }
        break;
    }

    case WM_CLOSE:
    {
        if (!IsWindowEnabled(HWindow))
        { // zavreme postupne vsechny dialogy nad timto dialogem (posleme jim WM_CLOSE a pak ho posleme znovu sem)
            SalamanderGeneral->CloseAllOwnedEnabledDialogs(HWindow);
            if (iThreadID != 0) // pokud bezi thread, zavreme i jeho okna + nechame ho ukoncit,
            {                   // aby se hned neotevrelo dalsi okno s dalsi chybou
                bTerminateThread = TRUE;
                SalamanderGeneral->CloseAllOwnedEnabledDialogs(HWindow, iThreadID);
            }
            SetTimer(HWindow, IDT_RESENDCLOSE, 100, NULL);
            return TRUE;
        }

        EndDialog(HWindow, IDCANCEL);
        return TRUE;
    }

    case WM_DESTROY:
    {
        if (hThread != NULL) // pokud jsme thread nastartovali, zajistime jeho zavreni a pockame na nej
        {
            bTerminateThread = TRUE;
            ThreadQueue.WaitForExit(hThread, INFINITE);
            hThread = NULL;
            iThreadID = 0;
        }
        ModelessQueue.Remove(HWindow);
        KillTimer(HWindow, IDT_UPDATEUI);
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
} /* CSFVMD5Dialog::DialogProc */

// ****************************************************************************************************
//
//  CCalculateDialog
//

CCalculateDialog::CCalculateDialog(HWND parent, BOOL alwaysOnTop, TSeedFileList* pFileList, const char* sourcePath)
    : CSFVMD5Dialog(IDD_CALCULATE, parent, alwaysOnTop)
{
    pSeedFileList = pFileList;
    SourcePath = sourcePath;
    memcpy(HashInfo, Config.HashInfo, sizeof(Config.HashInfo));
}

#define REFRESH_LIMIT 1000

void CCalculateDialog::RefreshUI()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // we want responsive GUI
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

BOOL CCalculateDialog::AddDir(char (&path)[MAX_PATH + 50], size_t root, BOOL* ignoreAll)
{
    if (StopReadingDirectories)
        return FALSE;

    WIN32_FIND_DATA fd;
    HANDLE hFind;
    size_t plen = strlen(path);
    strcat(path, "\\*");
    BOOL ret = TRUE, again;

    // tohle se dela pred spustenim pracovniho threadu (jen overime), synchronizace neni potreba
    if (bThreadRunning)
        TRACE_E("CCalculateDialog::AddDir(): unexpected situation: worker thread should not be running!");

    do
    {
        again = FALSE;
        if ((hFind = HANDLES_Q(FindFirstFile(path, &fd))) != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.cFileName[0] != 0 && strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
                {
                    if (plen + 1 + strlen(fd.cFileName) < MAX_PATH)
                    {
                        strcpy(path + plen + 1, fd.cFileName);
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                            ret = AddDir(path, root, ignoreAll);
                        else
                        {
                            // linky: size == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
                            BOOL cancel = FALSE;
                            CQuadWord size(fd.nFileSizeLow, fd.nFileSizeHigh);
                            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                            { // jde o link na soubor
                                CQuadWord linkSize;
                                if (SalamanderGeneral->GetLinkTgtFileSize(HWindow, path, &linkSize, &cancel, ignoreAll))
                                {
                                    size = linkSize;
                                }
                            }
                            if (cancel)
                            {
                                ret = FALSE;
                            }
                            else
                            {
                                AddFileListItem(path + root + 1, size, TRUE);
                                totalSize += size + CQuadWord(FILE_SIZE_FIX, 0);
                            }
                        }
                        if (RefreshCounter++ > REFRESH_LIMIT)
                        { // synchronizace neni potreba viz vyse
                            ListView_SetItemCountEx(hList, FileList.Count, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
                            RefreshUI();
                            RefreshCounter = 0;
                        }
                    }
                    else
                    {
                        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_PLUGINNAME),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        ret = FALSE;
                    }
                }
            } while (!StopReadingDirectories && ret && FindNextFile(hFind, &fd));
            HANDLES(FindClose(hFind));
            if (StopReadingDirectories)
                return FALSE;
        }
        else
        {
            if (SalamanderGeneral->DialogError(HWindow, BUTTONS_RETRYCANCEL, path, LoadStr(IDS_ERRORREADINGDIR),
                                               NULL) == DIALOG_RETRY)
                again = TRUE;
            else
                ret = FALSE;
        }
    } while (again);

    path[plen] = 0;
    return ret;
}

BOOL CCalculateDialog::GetFileList()
{
    CALL_STACK_MESSAGE1("CCalculateDialog::GetFileList()");

    // tohle se dela pred spustenim pracovniho threadu (jen overime), synchronizace neni potreba
    if (bThreadRunning)
        TRACE_E("CCalculateDialog::GetFileList(): unexpected situation: worker thread should not be running!");

    totalSize = CQuadWord(0, 0);

    BOOL ret = TRUE;
    char path[MAX_PATH + 50];
    size_t root = strlen(SourcePath);
    if (root > 0 && SourcePath[root - 1] == '\\')
        root--;

    RefreshCounter = 0;

    strcpy(path, SourcePath);
    SalamanderGeneral->SalPathAddBackslash(path, MAX_PATH); // pokud selze, selze urcite i prilepeni cehokoliv pozdeji (neni nutne osetrovat zde)
    char* pathEnd = path + strlen(path);

    BOOL ignoreAll = FALSE;
    for (int i = 0; ret && i < pSeedFileList->Count; i++)
    {
        SEEDFILEINFO* cfi = (*pSeedFileList)[i];

        if ((pathEnd - path) + strlen(cfi->Name) < MAX_PATH)
        {
            strcpy(pathEnd, cfi->Name);
            if (!cfi->bDir)
            {
                // linky: cfi->Size == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
                BOOL cancel = FALSE;
                if ((cfi->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                { // jde o link na soubor
                    CQuadWord linkSize;
                    if (SalamanderGeneral->GetLinkTgtFileSize(HWindow, path, &linkSize, &cancel, &ignoreAll))
                    {
                        cfi->Size = linkSize;
                    }
                }
                if (cancel)
                    ret = FALSE;
                else
                {
                    AddFileListItem(cfi->Name, cfi->Size, TRUE);
                    totalSize += cfi->Size + CQuadWord(FILE_SIZE_FIX, 0);
                    if (RefreshCounter++ > REFRESH_LIMIT)
                    {
                        RefreshUI();
                        RefreshCounter = 0;
                    }
                }
            }
            else
            {
                ret = AddDir(path, root, &ignoreAll);
            }
        }
        else
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_PLUGINNAME),
                                             MB_OK | MB_ICONEXCLAMATION);
            ret = FALSE;
        }
    }

    if (ret)
    { // synchronizace neni potreba viz vyse
        ListView_SetItemCountEx(hList, FileList.Count, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        if (FileList.Count > 0)
        {
            DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
            bDisableNotification = TRUE;
            ListView_SetItemState(hList, 0, state, state);
            bDisableNotification = FALSE;
        }
    }
    return ret;
}

class CCalculateThread : public CCRCMD5Thread
{
public:
    CCalculateThread(CCalculateDialog* dlg, BOOL* terminate) : CCRCMD5Thread(terminate) { dialog = dlg; };
    virtual unsigned Body();

protected:
    CCalculateDialog* dialog;
};

unsigned CCalculateThread::Body()
{
    CALL_STACK_MESSAGE1("CCalculateThread::Body()");
    TRACE_I("Begin");

    BOOL skippedReadError = FALSE;
    BOOL skipAllReadErrors = FALSE;
    BOOL skip;
    CHashAlgo* pCalculators[HT_COUNT];
    int nCalculators = 0;

    int ii;
    for (ii = 0; ii < HT_COUNT; ii++)
    {
        if (dialog->HashInfo[ii].bCalculate)
        {
            pCalculators[nCalculators] = dialog->HashInfo[ii].Factory();
            if (!pCalculators[nCalculators])
            {
                while (nCalculators-- > 0)
                    delete pCalculators[nCalculators];
                TRACE_E("Could not initialize " << dialog->HashInfo[ii].sRegID);
                if (dialog->FileList.Count > 0)
                    dialog->SetItemTextAndIcon(0, 2, LoadStr(IDS_CANCELED));
                TRACE_I("End");
                PostMessage(dialog->HWindow, WM_USER_ENDWORK, 0, 0);
                return 0;
            }
            nCalculators++;
        }
    }

    // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (pocet prvku + indexy se
    // nemeni = neni potreba pristup k nim synchronizovat)
    int silent = 0;
    for (int i = 0; i < dialog->FileList.Count && !*Terminate; i++)
    {
        // nascrollovani na aktualni polozku (pouze pokud je predchozi viditelna, tj. uzivatel treba neodjel na zacatek)
        dialog->ScrollToItem(i);

        // otevreni souboru
        HANDLE hFile;
        char path[MAX_PATH];
        strcpy(path, dialog->SourcePath);
        // nemelo by se stat - uz je overena delka jmena z CCalculateDialog::GetFileList()
        // FILELISTITEM::Name se po pridani do pole uz nemeni = netreba synchronizovat pristup k ni
        if (!SalamanderGeneral->SalPathAppend(path, dialog->FileList[i]->Name, MAX_PATH))
        {
            TRACE_E("CCalculateThread::Body(): unexpected situation: SalPathAppend() has failed");
            break;
        }
        if (!SafeOpenCreateFile(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                &hFile, &skip, &silent, dialog->HWindow))
            break;
        if (skip)
        {
            dialog->SetItemTextAndIcon(i, 2, LoadStr(IDS_SKIPPED));
            // posunuti progressu o velikost preskoceneho souboru
            WIN32_FIND_DATA fd;
            memset(&fd, 0, sizeof(fd));
            HANDLE find = HANDLES_Q(FindFirstFile(path, &fd));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                CQuadWord size(fd.nFileSizeLow, fd.nFileSizeHigh);
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                    (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                {
                    if (!SalamanderGeneral->SalGetFileSize2(path, size, NULL))
                        size.Set(fd.nFileSizeLow, fd.nFileSizeHigh);
                }
                dialog->IncreaseProgress(size + CQuadWord(FILE_SIZE_FIX, 0));
            }
            continue;
        }

        // Now calculates the hashes
        int j;
        for (j = 0; j < nCalculators; j++)
            pCalculators[j]->Init();

        DWORD nr;
        CQuadWord done(0, 0);
        do
        {
            char buffer[BUFSIZE];
            if (!SafeReadFile(hFile, buffer, BUFSIZE, &nr, path, dialog->HWindow, &skippedReadError, &skipAllReadErrors))
            {
                nr = 0; // chyba cteni
                if (skippedReadError)
                {
                    dialog->SetItemTextAndIcon(i, 2, LoadStr(IDS_SKIPPED));
                    // posunuti progressu o velikost preskoceneho souboru
                    WIN32_FIND_DATA fd;
                    memset(&fd, 0, sizeof(fd));
                    HANDLE find = HANDLES_Q(FindFirstFile(path, &fd));
                    if (find != INVALID_HANDLE_VALUE)
                    {
                        HANDLES(FindClose(find));
                        CQuadWord size(fd.nFileSizeLow, fd.nFileSizeHigh);
                        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                            (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                        {
                            if (!SalamanderGeneral->SalGetFileSize2(path, size, NULL))
                                size.Set(fd.nFileSizeLow, fd.nFileSizeHigh);
                        }
                        if (size >= done)
                        {
                            size -= done;
                            dialog->IncreaseProgress(size);
                        }
                    }
                }
                else
                    *Terminate = TRUE;
            }
            if (nr > 0)
            {
                for (int j2 = 0; j2 < nCalculators; j2++)
                    pCalculators[j2]->Update(buffer, nr);
                dialog->IncreaseProgress(CQuadWord(nr, 0));
                done += CQuadWord(nr, 0);
            }
        } while (nr == BUFSIZE && !*Terminate && !skippedReadError);
        if (!*Terminate)
            dialog->IncreaseProgress(CQuadWord(FILE_SIZE_FIX, 0));
        CloseHandle(hFile);

        // dosazeni vysledku do listu
        if (!*Terminate && !skippedReadError)
        {
            for (int k = 0; k < nCalculators; k++)
                pCalculators[k]->Finalize();

            char digest[DIGEST_MAX_SIZE];
            char text[2 * DIGEST_MAX_SIZE + 1];

            int j2;
            for (j2 = 0; j2 < nCalculators; j2++)
            {
                int len = pCalculators[j2]->GetDigest(digest, SizeOf(digest));
                text[0] = 0;
                int k2;
                for (k2 = 0; k2 < len; k2++)
                    sprintf(text + k2 * 2, "%02X", digest[k2]);
                dialog->SetItemTextAndIcon(i, 2 + j2, text);
            }
        }
        else
        {
            if (*Terminate)
                dialog->SetItemTextAndIcon(i, 2, LoadStr(IDS_CANCELED));
        }
    }

    while (nCalculators > 0)
        delete pCalculators[--nCalculators];
    TRACE_I("End");
    PostMessage(dialog->HWindow, WM_USER_ENDWORK, 0, 0);
    return 0;
}

void CCalculateDialog::OnThreadEnd()
{
    CALL_STACK_MESSAGE1("CCalculateDialog::OnThreadEnd()");
    ShowWindow(GetDlgItem(HWindow, IDC_LABEL_HINT), SW_SHOW);
    if (ListView_GetItemCount(hList))
    {
        // Enable Save button only when calculating some checksums (their columns are visible)
        int i;
        for (i = 0; i < HT_COUNT; i++)
        {
            if (HashInfo[i].bCalculate)
            {
                EnableButtons(TRUE);
                break;
            }
        }
    }
    CSFVMD5Dialog::OnThreadEnd();
}

void CCalculateDialog::EnableButtons(BOOL bEnable)
{
    CALL_STACK_MESSAGE2("CCalculateDialog::EnableButtons(%d)", bEnable);
    EnableWindow(GetDlgItem(HWindow, IDC_BUTTON_SAVE), bEnable);
}

void CCalculateDialog::DeleteItem(int index)
{
    CALL_STACK_MESSAGE2("CCalculateDialog::DeleteItem(%d)", index);
    CSFVMD5Dialog::DeleteItem(index);
    if (!ListView_GetItemCount(hList))
        EnableButtons(FALSE);
}

BOOL CCalculateDialog::GetSaveFileName(LPTSTR buffer, LPCTSTR title)
{
    CALL_STACK_MESSAGE2("CCalculateDialog::GetSaveFileName(, %s)", title);

    // ziskani defaultniho jmena; jsou vsechna jmena stejna?
    char file1[MAX_PATH], file2[MAX_PATH], filter[MAX_PATH], *s;
    GetItemText(0, 0, file1, MAX_PATH);
    SalamanderGeneral->SalPathRemoveExtension(file1);
    BOOL allSame = TRUE;
    int i;
    for (i = 1; i < ListView_GetItemCount(hList); i++)
    {
        GetItemText(i, 0, file2, MAX_PATH);
        SalamanderGeneral->SalPathRemoveExtension(file2);
        if (_stricmp(file1, file2))
        {
            allSame = FALSE;
            break;
        }
    }

    // kdyz ne, zkusime posledni cast cesty
    if (!allSame)
    {
        const char* slash = _tcsrchr(SourcePath, '\\');
        if (slash != NULL && slash[1])
        {
            lstrcpyn(buffer, slash + 1, MAX_PATH);
        }
        else
            buffer[0] = 0; // zadne defaultni jmeno
    }
    else
    {
        lstrcpyn(buffer, file1, MAX_PATH);
    }

    // save dialog
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = HWindow;
    ofn.hInstance = HLanguage;
    ofn.nFilterIndex = 1; // 1-based index
    filter[0] = 0;
    int j, ind;
    for (j = 0, ind = 0; j < HT_COUNT; j++)
        if (HashInfo[j].bCalculate)
        {
            ind++;
            if (HashInfo[j].Type == Config.HashType)
                ofn.nFilterIndex = ind; // 1-based index
            _tcscat(filter, LoadStr(HashInfo[j].idSaveAsFilter));
        }
    ofn.lpstrFilter = s = filter;
    while (NULL != (s = _tcschr(s, '|')))
        *s++ = 0;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = SourcePath;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST;
    for (;;)
    {
        if (!SalamanderGeneral->SafeGetSaveFileName(&ofn))
            return FALSE; // Canceled
        // Translate filter index into eHASH_TYPE
        int HashType = ofn.nFilterIndex - 1; // keep ofn.nFilterIndex unmodified
        int ind2 = ofn.nFilterIndex - 1;
        int k;
        for (k = 0; k < HT_COUNT; k++)
        {
            if (!HashInfo[k].bCalculate)
            {
                if (ind2 >= 0)
                    HashType++;
            }
            else
            {
                ind2--;
            }
        }
        Config.HashType = (eHASH_TYPE)HashType;
        if (!buffer[ofn.nFileExtension] && ofn.nFileExtension)
        { // The filename ends with '.'
            buffer[--ofn.nFileExtension] = 0;
        }
        else if (_tcscmp(buffer + ofn.nFileExtension, HashInfo[Config.HashType].sSaveAsExt + 1))
        { // The user did not enter any extension -> use the default one
            _tcscat(buffer, HashInfo[Config.HashType].sSaveAsExt);
        }
        FILE* f = _tfopen(buffer, "r");
        if (!f)
            break;
        fclose(f);
        sprintf(file1, LoadStr(IDS_SAVE_OVERWRITE), buffer);
        switch (SalamanderGeneral->SalMessageBox(HWindow, file1, LoadStr(IDS_SAVE_TITLE),
                                                 MB_YESNOCANCEL | MB_ICONQUESTION))
        {
        case IDYES:
            return TRUE;
        case IDCANCEL:
            return FALSE;
        }
    }
    return TRUE;
}

void CCalculateDialog::SaveHashes()
{
    CALL_STACK_MESSAGE1("CCalculateDialog::SaveHashes()");

    char filename[MAX_PATH];
    if (GetSaveFileName(filename, LoadStr(IDS_SAVE_TITLE)))
    {
        FILE* f;
        if ((f = _tfopen(filename, _T("w"))) == NULL)
        {
            Error(HWindow, GetLastError(), IDS_SAVE_TITLE, IDS_ERRORCREATINGFILE);
            return;
        }

        /*if (sfv)*/ fprintf(f, "; Generated by Open Salamander, https://www.altap.cz\n;\n"); // proc si neudelat reklamu...
        BOOL warn = FALSE;
        int colInd = 2;
        // Determine column index
        int j;
        for (j = 0; j < HT_COUNT; j++)
        {
            if (Config.HashType == HashInfo[j].Type)
                break;
            if (HashInfo[j].bCalculate)
                colInd++;
        }
        int i;
        for (i = 0; i < ListView_GetItemCount(hList); i++)
        {
            char name[MAX_PATH], hash[HASH_MAX_SIZE];
            GetItemText(i, 0, name, SizeOf(name));
            GetItemText(i, colInd, hash, SizeOf(hash));
            if (!hash[0] || !strcmp(hash, LoadStr(IDS_CANCELED)) || !strcmp(hash, LoadStr(IDS_SKIPPED)))
            { // Skip canceled / skipped files with empty hash/CRC
                warn = TRUE;
                continue;
            }
            _strlwr(hash);
            if (Config.HashType != HT_CRC)
                ConvertPath(name, '\\', '/');
            // CRC precedes filename, but hashes follow filename
            // Unix-based md5sum has this format:
            //  <128bit checksum><space><binary-flag><filename>
            //  where <binary-flag> is either <space> (text mode default) or * (binary mode -b option).
            //  -> We write 2 spaces
            fprintf(f, "%s  %s\n", (Config.HashType == HT_CRC) ? name : hash, (Config.HashType == HT_CRC) ? hash : name);
        }

        fclose(f);

        // ohlasime zmenu na ceste (pribyl nas soubor)
        SalamanderGeneral->CutDirectory(filename);
        SalamanderGeneral->PostChangeOnPathNotification(filename, FALSE);

        if (warn)
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_SKIPPEDFILES),
                                             LoadStr(IDS_SAVE_TITLE), MB_ICONINFORMATION);
    }
}

void CCalculateDialog::OnContextMenu(int x, int y, eHASH_TYPE forceCopyHash)
{
    int focIndex = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (focIndex == -1)
        return;

    CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
    if (popup == NULL)
        return;

    char checksum[HASH_MAX_SIZE];
    LVITEM lvi;

    lvi.mask = LVIF_TEXT;
    lvi.iItem = focIndex;
    lvi.pszText = checksum;
    lvi.cchTextMax = SizeOf(checksum);

    // A better approach would loading a menu template from the language file
    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
    mii.Type = MENU_TYPE_STRING;

    bool bSomeValue = false;

    int forcedHashIndex = -1;

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertItem() dole...
MENU_TEMPLATE_ITEM CalculateDialogMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_COPYTOCBOARD_CRC
  {MNTT_IT, IDS_COPYTOCBOARD_MD5
  {MNTT_IT, IDS_COPYTOCBOARD_SHA1
  {MNTT_IT, IDS_COPYTOCBOARD_SHA256
  {MNTT_IT, IDS_COPYTOCBOARD_SHA512
  {MNTT_IT, IDS_REMOVEITEM
  {MNTT_PE, 0
};
*/

    int i, j;
    for (i = 0, j = 0; i < HT_COUNT; i++)
    {
        if (HashInfo[i].bCalculate)
        {
            lvi.iSubItem = 2 + j;
            ListView_GetItem(hList, &lvi);
            if (checksum[0] && strcmp(checksum, LoadStr(IDS_CANCELED)) && strcmp(checksum, LoadStr(IDS_SKIPPED)))
            { // Check if the values have been calculated
                mii.ID = ++j;
                mii.String = LoadStr(HashInfo[i].idContextMenu);
                popup->InsertItem(-1, TRUE, &mii);
                if (forceCopyHash != HT_COUNT && forceCopyHash == i)
                    forcedHashIndex = mii.ID;
                bSomeValue = true;
            }
        }
    }
    if (bSomeValue)
    {
        mii.Type = MENU_TYPE_SEPARATOR;
        popup->InsertItem(-1, TRUE, &mii);
    }

    mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID | MENU_MASK_STATE;
    mii.Type = MENU_TYPE_STRING;
    mii.String = LoadStr(IDS_REMOVEITEM);
    mii.ID = CM_REMOVEITEM;
    mii.State = bThreadRunning ? MENU_STATE_GRAYED : 0;
    popup->InsertItem(-1, TRUE, &mii);

    int id = 0;
    if (forceCopyHash == HT_COUNT)
        id = popup->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON, x, y, HWindow, NULL);
    else
        id = forcedHashIndex;
    if (id == CM_REMOVEITEM)
    {
        DeleteItem(focIndex);
    }
    else if ((id >= 1) && (id <= HT_COUNT))
    {
        lvi.iSubItem = 2 + id - 1;
        ListView_GetItem(hList, &lvi);
        SalamanderGeneral->CopyTextToClipboard(checksum, -1, FALSE, NULL);
    }

    SalamanderGUI->DestroyMenuPopup(popup);
}

void GetListViewContextMenuPos(HWND hListView, POINT* p)
{
    if (ListView_GetItemCount(hListView) == 0)
    {
        p->x = 0;
        p->y = 0;
        ClientToScreen(hListView, p);
        return;
    }
    int focIndex = ListView_GetNextItem(hListView, -1, LVNI_FOCUSED);
    if (focIndex != -1)
    {
        if ((ListView_GetItemState(hListView, focIndex, LVNI_SELECTED) & LVNI_SELECTED) == 0)
            focIndex = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    }
    RECT cr;
    GetClientRect(hListView, &cr);
    RECT r;
    ListView_GetItemRect(hListView, 0, &r, LVIR_LABEL);
    p->x = r.left;
    if (p->x < 0)
        p->x = 0;
    if (focIndex != -1)
        ListView_GetItemRect(hListView, focIndex, &r, LVIR_BOUNDS);
    if (focIndex == -1 || r.bottom < 0 || r.bottom > cr.bottom)
        r.bottom = 0;
    p->y = r.bottom;
    ClientToScreen(hListView, p);
}

INT_PTR CCalculateDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
        //CALL_STACK_MESSAGE4("CCalculateDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
        static int id[] = {IDC_BUTTON_CONFIGURE, IDC_BUTTON_SAVE, IDC_BUTTON_CLOSE, IDC_LABEL, IDC_PROGRESS, IDC_LABEL_HINT};
    static int columns[] = {IDS_COLUMN_FILE, IDS_COLUMN_SIZE};

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FILE4)));
        InitList(columns, Config.CalcDlgWidths + 2, SizeOf(columns), HashInfo);
        InitLayout(id, SizeOf(id), 3);
        SetWindowPos(HWindow, NULL, 0, 0, Config.CalcDlgWidths[0], Config.CalcDlgWidths[1], SWP_NOZORDER | SWP_NOMOVE);
        SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);

        EnableButtons(FALSE);

        ModelessQueue.Add(new CWindowQueueItem(HWindow));

        HWND hProgress = GetDlgItem(HWindow, IDC_PROGRESS);
        SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1024));
        if (SalIsWindowsVersionOrGreater(6, 0, 0)) // WindowsVistaAndLater: Vista and later (CommonControls 6.0+)
        {
            LONG_PTR style = GetWindowLongPtr(hProgress, GWL_STYLE);
#ifndef PBS_MARQUEE
#define PBS_MARQUEE 0x08
#endif
#ifndef PBM_SETMARQUEE
#define PBM_SETMARQUEE (WM_USER + 10)
#endif
            SetWindowLongPtr(hProgress, GWL_STYLE, style | PBS_MARQUEE);
            SendMessage(hProgress, PBM_SETMARQUEE, TRUE, 50);
        }

        CGUIHyperLinkAbstract* hl;
        hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_LABEL_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_HINT));
        ShowWindow(GetDlgItem(HWindow, IDC_LABEL_HINT), SW_HIDE);

        PostMessage(HWindow, WM_USER_STARTWORK, 0, 0);
        break;
    }

    case WM_USER_STARTWORK:
    {
        ReadingDirectories = TRUE;
        if (!GetFileList())
        {
            EndDialog(HWindow, 0);
            ReadingDirectories = FALSE;
            break;
        }
        ReadingDirectories = FALSE;

        HWND hProgress = GetDlgItem(HWindow, IDC_PROGRESS);
        if (SalIsWindowsVersionOrGreater(6, 0, 0)) // WindowsVistaAndLater: Vista and later
        {
            SendMessage(hProgress, PBM_SETMARQUEE, FALSE, 0);
            LONG_PTR style = GetWindowLongPtr(hProgress, GWL_STYLE);
            SetWindowLongPtr(hProgress, GWL_STYLE, style & ~PBS_MARQUEE);
        }
        SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1024));

        BOOL startThread = FALSE;
        for (int i = 0; i < HT_COUNT; i++)
        {
            if (HashInfo[i].bCalculate)
            {
                startThread = TRUE;
            }
        }

        if (startThread) // pokud neni co pocitat, thread ani nebudeme spoustet
        {
            bTerminateThread = FALSE;
            hThread = NULL;
            iThreadID = 0;
            bThreadRunning = TRUE;
            CCalculateThread* pThread = new CCalculateThread(this, &bTerminateThread);
            if (pThread == NULL || (hThread = pThread->Create(ThreadQueue, 0, &iThreadID)) == NULL)
            {
                TRACE_E("CCalculateDialog::DialogProc(): Nelze vytvorit worker thread.");
                if (pThread != NULL)
                    delete pThread; // pri chybe je potreba dealokovat objekt threadu
                bThreadRunning = FALSE;
            }
        }
        else
        {
            PostMessage(HWindow, WM_USER_ENDWORK, 0, 0);
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_CLOSE:
        case IDCANCEL:
        {
            if (bThreadRunning)
            {
                bTerminateThread = TRUE;
            }
            else
            {
                if (ReadingDirectories)
                    StopReadingDirectories = TRUE;
                else
                    EndDialog(HWindow, 0);
            }
            return TRUE; // dlgproc predku nechceme
        }

        case IDC_BUTTON_SAVE:
            SaveHashes();
            break;

        case IDC_BUTTON_CONFIGURE:
            if (IDOK == OnConfiguration(HWindow))
                SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CONFIG_CHANGES_EFFECT),
                                                 LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
            break;
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_LIST_FILES)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case LVN_GETDISPINFO:
            {
                NMLVDISPINFO* plvdi = (NMLVDISPINFO*)nmh;
                int index = plvdi->item.iItem;
                if (index < 0 || index >= FileList.Count) // kdyz bezi pracovni thread, neprovadi se zadne upravy
                    break;                                // pole (pocet prvku se nemeni = nesynchronizujeme)
                if (plvdi->item.mask & LVIF_IMAGE)
                {
                    // ScrollIndex ani ikony pred ScrollIndex se z threadu nemeni, nemusime synchronizovat
                    if (bThreadRunning && index >= ScrollIndex)
                        plvdi->item.iImage = 0;
                    else
                        plvdi->item.iImage = FileList[index]->IconIndex;
                }
                if (plvdi->item.mask & LVIF_TEXT)
                {
                    int col = plvdi->item.iSubItem;
                    switch (col)
                    {
                    case 0:
                    { // Name: po pridani do pole uz se nemeni = pristup nesynchronizujeme
                        strcpy(plvdi->item.pszText, FileList[index]->Name);
                        break;
                    }

                    case 1:
                    { // Size: po pridani do pole uz se nemeni = pristup nesynchronizujeme
                        SalamanderGeneral->NumberToStr(plvdi->item.pszText, FileList[index]->Size);
                        break;
                    }

                    default:
                    {
                        // ScrollIndex ani hashe pred ScrollIndex se z threadu nemeni, nemusime synchronizovat
                        if (bThreadRunning && index >= ScrollIndex)
                        {
                            // dokud bezi vypocetni thread: napocitana data se priznaji jen do indexu ScrollIndex,
                            // primo ScrollIndex je calculating / verifying, za nim je nevypocteno (byt realne uz muze byt)
                            if (index == ScrollIndex && col == 2)
                                strcpy(plvdi->item.pszText, LoadStr(IDS_CALCULATING));
                            else
                                plvdi->item.pszText[0] = 0;
                        }
                        else
                        {
                            if (col >= 2 && col - 2 < HT_COUNT && FileList[index]->Hashes[col - 2] != NULL)
                                strcpy(plvdi->item.pszText, FileList[index]->Hashes[col - 2]);
                        }
                        break;
                    }
                    }
                }
                break;
            }

            case NM_RCLICK:
            {
                DWORD pos = GetMessagePos();
                OnContextMenu(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
                break;
            }

            case LVN_KEYDOWN:
            {
                LPNMLVKEYDOWN nmhk = (LPNMLVKEYDOWN)nmh;
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shiftPressed && nmhk->wVKey == VK_F10 || nmhk->wVKey == VK_APPS)
                {
                    POINT p;
                    GetListViewContextMenuPos(hList, &p);
                    OnContextMenu(p.x, p.y);
                }
                WORD wVKey = ((LPNMLVKEYDOWN)lParam)->wVKey;
                if (!bThreadRunning && wVKey == VK_DELETE)
                {
                    DeleteItem(ListView_GetSelectionMark(hList));
                }
                if (controlPressed)
                {
                    eHASH_TYPE hashType = HT_COUNT;
                    switch (wVKey)
                    {
                    case 'C':
                        hashType = HT_CRC;
                        break;
                    case 'M':
                        hashType = HT_MD5;
                        break;
                    case '1':
                        hashType = HT_SHA1;
                        break;
                    case '2':
                        hashType = HT_SHA256;
                        break;
                    case '5':
                        hashType = HT_SHA512;
                        break;
                    }
                    if (hashType != HT_COUNT)
                        OnContextMenu(0, 0, hashType);
                }
            }
            }
        }
        break;
    }

    case WM_SIZE:
    {
        RecalcLayout(LOWORD(lParam), HIWORD(lParam), id, SizeOf(id), 3);
        break;
    }

    case WM_DESTROY:
    {
        SaveSizes(Config.CalcDlgWidths, SizeOf(columns), HashInfo);
        break;
    }
    }
    return CSFVMD5Dialog::DialogProc(uMsg, wParam, lParam);
} /* CCalculateDialog::DialogProc */

// ****************************************************************************************************
//
//  CVerifyDialog
//

CVerifyDialog::CVerifyDialog(HWND parent, BOOL alwaysOnTop, char* path, char* file)
    : CSFVMD5Dialog(IDD_VERIFY, parent, alwaysOnTop), fileList(100, 100, dtDelete)
{
    CALL_STACK_MESSAGE1("CVerifyDialog::CVerifyDialog(, , )");
    sourcePath = path;
    sourceFile = file;
}

void CVerifyDialog::LTrimStr(char* str)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
        // CALL_STACK_MESSAGE1("CVerifyDialog::LTrimStr()");
        int i = 0;
    while (str[i] && ((BYTE)str[i] <= ' '))
        i++;
    if (i)
        memmove(str, str + i, (int)strlen(str + i) + 1);
}

/*char* CVerifyDialog::GetLine(FILE* f, char* buffer, int max)
{
  CALL_STACK_MESSAGE2("CVerifyDialog::GetLine(, , %d)", max);
  char* ret = fgets(buffer, max-1, f);
  int len = strlen(buffer);
  if (buffer[len-1] == '\n') buffer[len-1] = 0;
  return ret;
}*/

char* CVerifyDialog::LoadFile(char* name)
{
    CALL_STACK_MESSAGE2("CVerifyDialog::LoadFile(%s)", name);
    HANDLE file = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        Error(HWindow, GetLastError(), IDS_VERIFYTITLE, IDS_ERROROPENING2, name);
        return NULL;
    }
    CQuadWord fsize;
    DWORD err;
    if (SalamanderGeneral->SalGetFileSize(file, fsize, err) && fsize.Value < 500 * 1024 * 1024) // 500 MB je snad uz fakt dost
    {
        char* text = new char[fsize.LoDWord + 1];
        if (text == NULL)
        {
            CloseHandle(file);
            Error(HWindow, 0, IDS_VERIFYTITLE, IDS_OUTOFMEM);
            return NULL;
        }
        DWORD numr;
        if (!SafeReadFile(file, text, fsize.LoDWord, &numr, name, HWindow))
        {
            delete[] text;
            CloseHandle(file);
            return NULL;
        }
        CloseHandle(file);
        text[fsize.LoDWord] = 0;
        return text;
    }
    else
    {
        CloseHandle(file);
        Error(HWindow, err, IDS_VERIFYTITLE, IDS_ERROROPENING2, name);
        return NULL;
    }
}

BOOL CVerifyDialog::AnalyzeSourceFile()
{
    CALL_STACK_MESSAGE1("CVerifyDialog::AnalyzeSourceFile()");

    char* text = LoadFile(sourceFile);
    if (text == NULL)
        return FALSE;
    char* line = strtok(text, "\r\n");

    BOOL isSFV = TRUE, isMD5 = TRUE, isSHA1 = TRUE, isSHA256 = TRUE, isSHA512 = TRUE;
    while (line != NULL)
    {
        LTrimStr(line);
        // Patera 2006.08.17: # used by MD5summer (http://www.md5summer.org) for comment lines
        if (line[0] && (line[0] != ';') && (line[0] != '#'))
        {
            // POZOR: nasledujici kod musi byt v souladu s CCRCAlgo::ParseDigest() !!!
            // checksum na konci (za ' ')

            int posLast, lenLast;
            GetLastWord(line, posLast, lenLast);
            BOOL lastIsHex = IsHex(line + posLast, lenLast);

            if (isSFV &&
                (lenLast != 8 || !lastIsHex))
            {                  // nekonci checksumem
                isSFV = FALSE; // nejde o SFV
            }

            // POZOR: nasledujici kod musi byt v souladu s CGenericHashAlgo::ParseDigest() !!!
            // checksum na zacatku (pred ' ') nebo checksum na konci (za ' ' nebo '=') a zaroven
            // jmeno hashe na zacatku (pred '(' nebo ' ')

            int posFirst, lenFirst;
            GetFirstWord(line, posFirst, lenFirst);
            BOOL firstIsHex = IsHex(line + posFirst, lenFirst);
            int posFirstHashName, lenFirstHashName;
            GetFirstWord(line, posFirstHashName, lenFirstHashName, '(');
            GetLastWord(line, posLast, lenLast, '='); // pro zbytek hashu je '=' oddelovac
            lastIsHex = IsHex(line + posLast, lenLast);

            if (isMD5 &&
                (lenFirst != 32 || !firstIsHex) &&
                (lenLast != 32 || !lastIsHex || lenFirstHashName != 3 || memcmp(line + posFirstHashName, "MD5", 3) != 0))
            {                  // nezacina checksumem, ani neni: MD5 (apache_2.0.46-win32-x86-symbols.zip) = eb5ba72b4164d765a79a7e06cee4eead
                isMD5 = FALSE; // nejde o MD5
            }
            if (isSHA1 &&
                (lenFirst != 40 || !firstIsHex) &&
                (lenLast != 40 || !lastIsHex || lenFirstHashName != 4 || memcmp(line + posFirstHashName, "SHA1", 4) != 0))
            {                   // nezacina checksumem, ani neni: SHA1 (openldap-2.2.25.tgz) = a983e039486b8495819e69260a8fad1fb9c77520
                isSHA1 = FALSE; // nejde o SHA1
            }
            if (isSHA256 &&
                (lenFirst != 64 || !firstIsHex) &&
                (lenLast != 64 || !lastIsHex || lenFirstHashName != 6 || memcmp(line + posFirstHashName, "SHA256", 6) != 0))
            {                     // nezacina checksumem, ani neni: SHA256 (README) = baaa5da257f848a4eece4fcf7653a7a58930124ef244bda374a6e906207d8a73
                isSHA256 = FALSE; // nejde o SHA256
            }
            if (isSHA512 &&
                (lenFirst != 128 || !firstIsHex) &&
                (lenLast != 128 || !lastIsHex || lenFirstHashName != 6 || memcmp(line + posFirstHashName, "SHA512", 6) != 0))
            {                     // nezacina checksumem, ani neni: SHA512 (README) = baaa5da257f848a4eece4fcf7653a7a58930124ef244bda374a6e906207d8a73baaa5da257f848a4eece4fcf7653a7a58930124ef244bda374a6e906207d8a73
                isSHA512 = FALSE; // nejde o SHA512
            }
        }
        line = strtok(NULL, "\r\n");
    }

    delete[] text;
    if (!isMD5 && !isSFV && !isSHA1 && !isSHA256 && !isSHA512)
        return Error(HWindow, 0, IDS_VERIFYTITLE, IDS_BADFILE);

    eHASH_TYPE HashType = isSFV ? HT_CRC : (isMD5 ? HT_MD5 : (isSHA1 ? HT_SHA1 : (isSHA256 ? HT_SHA256 : HT_SHA512)));
    for (int i = 0; i < HT_COUNT; i++)
        if (HashType == Config.HashInfo[i].Type)
        {
            pHashInfo = &Config.HashInfo[i];
            break;
        }

    return TRUE;
}

BOOL CVerifyDialog::LoadSourceFile()
{
    CALL_STACK_MESSAGE1("CVerifyDialog::LoadSourceFile()");

    // tohle se dela pred spustenim pracovniho threadu (jen overime), synchronizace neni potreba
    if (bThreadRunning)
        TRACE_E("CVerifyDialog::LoadSourceFile(): unexpected situation: worker thread should not be running!");

    char* text = LoadFile(sourceFile);
    if (text == NULL)
        return FALSE;
    char* line = strtok(text, "\r\n");
    FILEINFO* info;
    CHashAlgo* pCalculator = pHashInfo->Factory();

    if (!pCalculator)
    {
        delete[] text;
        return FALSE;
    }

    totalSize.Value = 0;

    // zpracovani radek
    BOOL ret = TRUE;
    BOOL ignoreAll = FALSE;
    while (ret && line != NULL)
    {
        LTrimStr(line);
        // Patera 2006.08.17: # used by MD5summer (http://www.md5summer.org) for comment lines
        if (line[0] && /*bSFV &&*/ (line[0] != ';') && (line[0] != '#'))
        {
            info = new FILEINFO;
            if (info == NULL)
            {
                Error(HWindow, 0, IDS_VERIFYTITLE, IDS_OUTOFMEM);
                ret = FALSE;
                break;
            }
            memset(info, 0, sizeof(FILEINFO));

            if (!pCalculator->ParseDigest(line, info->fileName, _countof(info->fileName), info->digest))
            {
                Error(HWindow, 0, IDS_VERIFYTITLE, IDS_TOOLONGNAME);
                ret = FALSE;
                delete info;
                break;
            }
            ConvertPath(info->fileName, '/', '\\');
            if (!info->fileName[0] && line == text)
            {
                // first (and hopefully the only) line contains no file name
                // -> take the hash file name and trim the suffix
                char* s = _tcsrchr(sourceFile, '\\');
                if (!s)
                    s = sourceFile;
                else
                    s++;
                strcpy(info->fileName, s);
                s = _tcsrchr(info->fileName, '.'); // ".cvspass" is extension in Windows
                if (s && !_stricmp(s, pHashInfo->sSaveAsExt))
                    *s = 0; // trim the default suffix
                else
                    info->fileName[0] = 0; // keep the empty name and skip this line
            }

            if (info->fileName[0] != 0)
            {
                // zjisteni informaci o souboru a vlozeni do listu
                char path[MAX_PATH];
                strcpy(path, sourcePath);
                if (SalamanderGeneral->SalPathAppend(path, info->fileName, MAX_PATH))
                {
                    WIN32_FIND_DATA fd;
                    HANDLE hFind = HANDLES_Q(FindFirstFile(path, &fd));
                    info->bFileExist = (hFind != INVALID_HANDLE_VALUE);
                    if (info->bFileExist)
                    {
                        // linky: info->size == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
                        BOOL cancel = FALSE;
                        info->size = CQuadWord(fd.nFileSizeLow, fd.nFileSizeHigh);
                        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                            (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                        { // jde o link na soubor
                            CQuadWord linkSize;
                            if (SalamanderGeneral->GetLinkTgtFileSize(HWindow, path, &linkSize, &cancel, &ignoreAll))
                                info->size = linkSize;
                        }
                        if (cancel)
                            ret = FALSE;
                    }
                    AddFileListItem(info->fileName, info->size, info->bFileExist);
                    if (info->bFileExist)
                    {
                        HANDLES(FindClose(hFind));
                        totalSize += info->size + CQuadWord(FILE_SIZE_FIX, 0);
                    }

                    strcpy(info->fileName, path);
                    fileList.Add(info); // synchronizace neni potreba, pracovni thread nebezi, viz vyse
                }
                else
                {
                    Error(HWindow, 0, IDS_VERIFYTITLE, IDS_TOOLONGNAME);
                    ret = FALSE;
                    delete info;
                    break;
                }
            }
            else // prazdne jmeno souboru = necekany format
            {
                Error(HWindow, 0, IDS_VERIFYTITLE, IDS_BADFILE);
                ret = FALSE;
                delete info;
                break;
            }
        }

        line = strtok(NULL, "\r\n");
    }

    // nacteni souboru je rychle, takze celkovy pocet polozek nastavime az po nem
    // synchronizace neni potreba, pracovni thread nebezi, viz vyse
    ListView_SetItemCountEx(hList, FileList.Count, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

    delete[] text;
    delete pCalculator;
    if (ret && FileList.Count > 0)
    {
        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        bDisableNotification = TRUE;
        ListView_SetItemState(hList, 0, state, state);
        bDisableNotification = FALSE;
    }

    return ret;
}

class CVerifyThread : public CCRCMD5Thread
{
public:
    CVerifyThread(CVerifyDialog* dlg, BOOL* terminate) : CCRCMD5Thread(terminate) { dialog = dlg; }
    virtual unsigned Body();

protected:
    CVerifyDialog* dialog;
};

unsigned CVerifyThread::Body()
{
    CALL_STACK_MESSAGE1("CVerifyThread::Body()");
    TRACE_I("Begin");

    CHashAlgo* pCalculator = dialog->pHashInfo->Factory();

    if (NULL == pCalculator)
    {
        TRACE_E("CVerifyThread::Body(): Could not instantiate calculator");
        TRACE_I("End");
        if (dialog->fileList.Count > 0)
            dialog->SetItemTextAndIcon(0, 2, LoadStr(IDS_CANCELED));
        dialog->bCanceled = TRUE;
        PostMessage(dialog->HWindow, WM_USER_ENDWORK, 0, 0);
        return 0;
    }

    BOOL skip;

    // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (pocet prvku + indexy se
    // nemeni = neni potreba pristup k nim synchronizovat)
    for (int i = 0, silent = 0; i < dialog->fileList.Count && !*Terminate; i++)
    {
        FILEINFO* info = dialog->fileList[i];

        // nascrollovani na aktualni polozku
        dialog->ScrollToItem(i);

        if (!info->bFileExist)
        {
            dialog->SetItemTextAndIcon(i, 0, NULL, 1);
            dialog->nMissing++; // pouziva se jen z threadu + z main-threadu jen kdyz thread nebezi, nesynchronizujeme
            continue;
        }

        // otevreni souboru
        HANDLE hFile;
        if (!SafeOpenCreateFile(info->fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                &hFile, &skip, &silent, dialog->HWindow))
        {
            dialog->SetItemTextAndIcon(i, 2, LoadStr(IDS_CANCELED));
            dialog->bCanceled = TRUE;
            break;
        }
        if (skip)
        {
            dialog->SetItemTextAndIcon(i, 2, LoadStr(IDS_SKIPPED));
            // posunuti progressu o velikost preskoceneho souboru
            dialog->IncreaseProgress(info->size + CQuadWord(FILE_SIZE_FIX, 0));
            dialog->nSkipped++; // pouziva se jen z threadu + z main-threadu jen kdyz thread nebezi, nesynchronizujeme
            continue;
        }

        // napocitani CRC nebo MD5
        pCalculator->Init();
        DWORD nr;
        do
        {
            char buffer[BUFSIZE];
            if (!SafeReadFile(hFile, buffer, BUFSIZE, &nr, info->fileName, dialog->HWindow))
            {
                dialog->bCanceled = TRUE;
                *Terminate = TRUE;
            }
            if (!*Terminate)
            {
                dialog->IncreaseProgress(CQuadWord(nr, 0));
                pCalculator->Update(buffer, nr);
            }
        } while (nr == BUFSIZE && !*Terminate);
        if (!*Terminate)
        {
            pCalculator->Finalize();
            dialog->IncreaseProgress(CQuadWord(FILE_SIZE_FIX, 0));
        }
        CloseHandle(hFile);

        // dosazeni vysledku do listu
        if (!*Terminate)
        {
            char digest[DIGEST_MAX_SIZE];
            int len = pCalculator->GetDigest(digest, SizeOf(digest));
            BOOL ok = (len > 0) && !memcmp(info->digest, digest, len);

            dialog->SetItemTextAndIcon(i, 2, LoadStr(ok ? IDS_OK : IDS_CORRUPT), ok ? 3 : 2);
            if (!ok)
                dialog->nCorrupt++; // pouziva se jen z threadu + z main-threadu jen kdyz thread nebezi, nesynchronizujeme
        }
        else
            dialog->SetItemTextAndIcon(i, 2, LoadStr(IDS_CANCELED));
    }

    delete pCalculator;
    TRACE_I("End");
    PostMessage(dialog->HWindow, WM_USER_ENDWORK, 0, 0);
    return 0;
}

void CVerifyDialog::OnThreadEnd()
{
    CALL_STACK_MESSAGE1("CVerifyDialog::OnThreadEnd()");
    ShowWindow(GetDlgItem(HWindow, IDC_LABEL_RESULT), SW_SHOW);
    char text[200];
    if (bCanceled)
        strcpy(text, LoadStr(IDS_CANCELED));
    else
    {
        if (!nMissing && !nSkipped && !nCorrupt)
        {
            if (fileList.Count)                   // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (pocet
                strcpy(text, LoadStr(IDS_ALLOK)); // prvku se nemeni = neni potreba pristup k nim synchronizovat)
            else
                strcpy(text, LoadStr(IDS_NOFILES));
        }
        else
        {
            sprintf(text, LoadStr(IDS_RESULT), nCorrupt, nMissing, nSkipped);
        }
    }
    SetDlgItemText(HWindow, IDC_LABEL_RESULT, text);
    CSFVMD5Dialog::OnThreadEnd();
}

INT_PTR CVerifyDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CVerifyDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static int id[] = {IDC_BUTTON_FOCUS, IDC_BUTTON_CLOSE, IDC_LABEL, IDC_PROGRESS, IDC_LABEL_RESULT};
    static int columns[] = {IDS_COLUMN_FILE, IDS_COLUMN_SIZE, IDS_COLUMN_STATUS};

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_CHECKMARK)));
        InitList(columns, Config.VerDlgWidths + 2, SizeOf(columns));
        InitLayout(id, SizeOf(id), 2);
        SetWindowPos(HWindow, NULL, 0, 0, Config.VerDlgWidths[0], Config.VerDlgWidths[1], SWP_NOZORDER | SWP_NOMOVE);
        SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);
        SendMessage(GetDlgItem(HWindow, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 1024));
        ShowWindow(GetDlgItem(HWindow, IDC_LABEL_RESULT), SW_HIDE);
        nMissing = nCorrupt = nSkipped = 0;
        bCanceled = FALSE;
        ModelessQueue.Add(new CWindowQueueItem(HWindow));
        SetWindowText(HWindow, LoadStr(IDS_VERIFYTITLE)); // provizorni titulek (pokud se ukaze chyba, at neni prazdny)

        PostMessage(HWindow, WM_USER_STARTWORK, 0, 0);
        break;
    }

    case WM_USER_STARTWORK:
    {
        if (!AnalyzeSourceFile() || !LoadSourceFile())
        {
            EndDialog(HWindow, 0);
        }
        else
        {
            SetWindowText(HWindow, LoadStr(pHashInfo->idVerifyTitle));
            bTerminateThread = FALSE;
            hThread = NULL;
            iThreadID = 0;
            bThreadRunning = TRUE;
            CVerifyThread* pThread = new CVerifyThread(this, &bTerminateThread);
            if (pThread == NULL || (hThread = pThread->Create(ThreadQueue, 0, &iThreadID)) == NULL)
            {
                TRACE_E("CVerifyDialog::DialogProc(): Nelze vytvorit worker thread.");
                if (pThread)
                    delete pThread; // pri chybe je potreba dealokovat objekt threadu
                pThread = NULL;
                bThreadRunning = FALSE;
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        case IDC_BUTTON_CLOSE:
        {
            bCanceled = TRUE;
            if (bThreadRunning)
                bTerminateThread = TRUE;
            else
                EndDialog(HWindow, 0);
            return TRUE; // dlgproc predku nechceme
        }

        case IDC_BUTTON_FOCUS:
        {
            int i = -1;
            i = ListView_GetNextItem(hList, i, LVNI_SELECTED);
            // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (pocet prvku + indexy se
            // nemeni = neni potreba pristup k nim synchronizovat)
            if (i < 0 || i >= fileList.Count || !fileList[i]->bFileExist)
                break;

            if (SalamanderGeneral->SalamanderIsNotBusy(NULL))
            {
                lstrcpyn(Focus_Path, fileList[i]->fileName, MAX_PATH);
                SalamanderGeneral->PostMenuExtCommand(CMD_FOCUSFILE, TRUE);
                Sleep(500);        // dojde k prepnuti do jineho okna, takze teoreticky tenhle Sleep nicemu nebude vadit
                Focus_Path[0] = 0; // po 0.5 sekunde uz o fokus nestojime (resi pripad, kdy jsme trefili zacatek BUSY rezimu Salamandera)
            }
            else
                SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_BUSY), LoadStr(IDS_VERIFYTITLE), MB_ICONINFORMATION);

            break;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_LIST_FILES)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case LVN_GETDISPINFO:
            {
                NMLVDISPINFO* plvdi = (NMLVDISPINFO*)nmh;
                int index = plvdi->item.iItem;
                // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (pocet prvku + indexy se
                // nemeni = neni potreba pristup k nim synchronizovat)
                if (index < 0 || index >= FileList.Count)
                    break;
                if (plvdi->item.mask & LVIF_IMAGE)
                {
                    // ScrollIndex ani ikony pred ScrollIndex se z threadu nemeni, nemusime synchronizovat
                    if (bThreadRunning && index >= ScrollIndex)
                        plvdi->item.iImage = 0;
                    else
                        plvdi->item.iImage = FileList[index]->IconIndex;
                }
                if (plvdi->item.mask & LVIF_TEXT)
                {
                    int col = plvdi->item.iSubItem;
                    switch (col)
                    {
                    case 0: // Name: po pridani do pole uz se nemeni = pristup nesynchronizujeme
                    {
                        strcpy(plvdi->item.pszText, FileList[index]->Name);
                        break;
                    }

                    case 1: // FileExist+Size: po pridani do pole uz se nemeni = pristup nesynchronizujeme
                    {
                        if (FileList[index]->FileExist)
                            SalamanderGeneral->NumberToStr(plvdi->item.pszText, FileList[index]->Size);
                        else
                            plvdi->item.pszText[0] = 0;
                        break;
                    }

                    case 2:
                    {
                        if (FileList[index]->FileExist)
                        {
                            // ScrollIndex ani hashe pred ScrollIndex se z threadu nemeni, nemusime synchronizovat
                            if (bThreadRunning && index >= ScrollIndex)
                            {
                                // dokud bezi vypocetni thread: napocitana data se priznaji jen do indexu ScrollIndex,
                                // primo ScrollIndex je calculating / verifying, za nim je nevypocteno (byt realne uz muze byt)
                                if (index == ScrollIndex)
                                    strcpy(plvdi->item.pszText, LoadStr(IDS_VERIFYING));
                                else
                                    plvdi->item.pszText[0] = 0;
                            }
                            else
                            {
                                if (FileList[index]->Hashes[0] != NULL)
                                    strcpy(plvdi->item.pszText, FileList[index]->Hashes[0]);
                                else
                                    plvdi->item.pszText[0] = 0;
                            }
                        }
                        else
                        {
                            strcpy(plvdi->item.pszText, LoadStr(IDS_MISSING));
                        }
                        break;
                    }
                    }
                }
                break;
            }

            case LVN_ITEMCHANGED:
            {
                int i = -1;
                i = ListView_GetNextItem(hList, i, LVNI_SELECTED);
                // kdyz bezi pracovni thread, neprovadi se zadne upravy pole (pocet prvku + indexy se
                // nemeni = neni potreba pristup k nim synchronizovat)
                if (i >= 0 && i < fileList.Count)
                {
                    EnableWindow(GetDlgItem(HWindow, IDC_BUTTON_FOCUS), fileList[i]->bFileExist);
                }
                break;
            }

            case NM_DBLCLK:
            {
                SendMessage(HWindow, WM_COMMAND, IDC_BUTTON_FOCUS, 0);
                break;
            }
            }
        }
        break;
    }

    case WM_SIZE:
    {
        RecalcLayout(LOWORD(lParam), HIWORD(lParam), id, SizeOf(id), 2);
        break;
    }

    case WM_DESTROY:
    {
        SaveSizes(Config.VerDlgWidths, SizeOf(columns));
        break;
    }
    }
    return CSFVMD5Dialog::DialogProc(uMsg, wParam, lParam);
} /* CVerifyDialog::DialogProc */

// ****************************************************************************************************
//
//  CCalculateDialogThread
//

class CCalculateDialogThread : public CThread
{
public:
    CCalculateDialogThread(HWND parent, BOOL alwaysOnTop, TSeedFileList* pFileList, char* sourcePath) : CThread("Calculate SFV/MD5 Dialog")
    {
        hParent = parent;
        pSeedFileList = pFileList;
        SourcePath = sourcePath;
        bAlwaysOnTop = alwaysOnTop;
    }

    virtual unsigned Body();

private:
    HWND hParent;
    BOOL bAlwaysOnTop;
    TSeedFileList* pSeedFileList;
    char* SourcePath;
};

unsigned CCalculateDialogThread::Body()
{

    CALL_STACK_MESSAGE1("CCalculateDialogThread::Body()");
    TRACE_I("Begin");

    CCalculateDialog dlg(hParent, bAlwaysOnTop, pSeedFileList, SourcePath);
    dlg.Execute();

    delete pSeedFileList;
    free(SourcePath);

    TRACE_I("End");
    return 0;
}

BOOL OpenCalculateDialog(HWND parent)
{
    CALL_STACK_MESSAGE1("CCalculateDialogThread()");

    TSeedFileList* pFileList = new TSeedFileList(100, 100, dtDelete);

    if (!pFileList)
    {
        TRACE_E("Allocating pFileList failed");
        return FALSE;
    }

    int nFiles, nDirs;
    char sourcePath[MAX_PATH];

    // Check if neni nic vybrano ani fokus
    if (SalamanderGeneral->GetPanelSelection(PANEL_SOURCE, &nFiles, &nDirs))
    {
        int index = 0;
        const CFileData* fd;
        BOOL isDir;

        SalamanderGeneral->GetPanelPath(PANEL_SOURCE, sourcePath, SizeOf(sourcePath), NULL, NULL);

        while (((nFiles || nDirs) ? (fd = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir)) != NULL : (fd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir)) != NULL))
        {
            int fdNameLen = (int)strlen(fd->Name);
            SEEDFILEINFO* cfi = (SEEDFILEINFO*)malloc(sizeof(SEEDFILEINFO) + fdNameLen);
            if (!cfi)
                continue;
            cfi->bDir = isDir ? true : false;
            cfi->Attr = fd->Attr;
            strcpy_s(cfi->Name, fdNameLen + _countof(cfi->Name), fd->Name);
            if (!isDir)
                cfi->Size = fd->Size;
            pFileList->Add(cfi);
            if (!nFiles && !nDirs)
                break;
        }
    }

    BOOL bAlwaysOnTop = FALSE;
    // NOTE: GetConfigParameter can only be called from the main thread
    SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &bAlwaysOnTop, sizeof(bAlwaysOnTop), NULL);

    CCalculateDialogThread* t = new CCalculateDialogThread(parent, bAlwaysOnTop, pFileList, _strdup(sourcePath));
    if (t != NULL)
    {
        // spusteni threadu
        if (t->Create(ThreadQueue) != NULL)
            return TRUE;

        delete t; // pri chybe je potreba dealokovat objekt threadu
    }
    return FALSE;
}

// ****************************************************************************************************
//
//  CVerifyDialogThread
//

class CVerifyDialogThread : public CThread
{
public:
    CVerifyDialogThread(HWND parent, BOOL alwaysOnTop) : CThread("Verify SFV/MD5 Dialog")
    {
        hParent = parent;
        bAlwaysOnTop = alwaysOnTop;
    }

    virtual unsigned Body();

    char sourcePath[MAX_PATH];
    char sourceFile[MAX_PATH];

private:
    HWND hParent;
    BOOL bAlwaysOnTop;
};

unsigned CVerifyDialogThread::Body()
{
    CALL_STACK_MESSAGE1("CVerifyDialogThread::Body()");
    TRACE_I("Begin");

    CVerifyDialog dlg(hParent, bAlwaysOnTop, sourcePath, sourceFile);
    dlg.Execute();

    TRACE_I("End");
    return 0;
}

BOOL OpenVerifyDialog(HWND parent)
{
    CALL_STACK_MESSAGE1("OpenVerifyDialog()");

    const CFileData* fd;
    BOOL isDir;
    fd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
    if (isDir)
        return FALSE;

    // Sanity: check if known extension
    bool bFound = false;
    int i;
    for (i = 0; i < HT_COUNT; i++)
        if (!_tcsicmp(fd->Ext, Config.HashInfo[i].sSaveAsExt + 1))
            bFound = true;
    if (!bFound)
        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_BADEXT), LoadStr(IDS_VERIFYTITLE),
                                             MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_DEFBUTTON2 |
                                                 MSGBOXEX_ESCAPEENABLED) == IDNO)
            return FALSE;

    BOOL bAlwaysOnTop = FALSE;
    // NOTE: GetConfigParameter can only be called from the main thread
    SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &bAlwaysOnTop, sizeof(bAlwaysOnTop), NULL);

    CVerifyDialogThread* t = new CVerifyDialogThread(parent, bAlwaysOnTop);
    if (t != NULL)
    {
        // predani dat do threadu
        SalamanderGeneral->GetPanelPath(PANEL_SOURCE, t->sourcePath, MAX_PATH, NULL, NULL);
        strcpy(t->sourceFile, t->sourcePath);
        if (SalamanderGeneral->SalPathAppend(t->sourceFile, fd->Name, MAX_PATH))
        {
            // spusteni threadu
            if (t->Create(ThreadQueue) != NULL)
                return TRUE;
        }
        else
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_VERIFYTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }

        delete t; // pri chybe je potreba dealokovat objekt threadu
    }
    return FALSE;
}

//****************************************************************************
//
// CCommonDialog - Centered dialog
//

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, hParent, origin)
{
}

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, helpID, hParent, origin)
{
}

INT_PTR
CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // Center horizontally & vertically to parent
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // Need focus from DefDlgProc
    }
    } // switch
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CConfigurationDialog
//

CConfigurationDialog::CConfigurationDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_CONFIGURATION, IDD_CONFIGURATION, hParent)
{
}

void CConfigurationDialog::Transfer(CTransferInfo& ti)
{
    int i;
    for (i = 0; i < HT_COUNT; i++)
        ti.CheckBox(IDC_CFG_SUM_1 + i, Config.HashInfo[i].bCalculate);
}

/*BOOL
CConfigurationDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_COMMAND:
    {
      break;
    }

  } // switch
  return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}*/
