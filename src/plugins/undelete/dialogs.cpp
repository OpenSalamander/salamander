// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "undelete.rh"
#include "undelete.rh2"
#include "lang\lang.rh"

#include "miscstr.h"
#include "os.h"
#include "volume.h"
#include "snapshot.h"
#include "dialogs.h"
#include "undelete.h"

#include "library\volenum.h"

#pragma comment(lib, "UxTheme.lib")

// ****************************************************************************
//
//  CSnapshotProgressDlg
//

CSnapshotProgressDlg::CSnapshotProgressDlg(HWND parent, CObjectOrigin origin)
    : CDialog(HLanguage, IDD_SNAPSHOTPROGRESSDLG, parent, origin)
{
    ProgressBar = NULL;
    WantCancel = FALSE;
}

void CSnapshotProgressDlg::SetProgressText(int resID)
{
    CALL_STACK_MESSAGE2("CSnapshotProgressDlg::SetProgressText(%d)", resID);
    SetDlgItemText(HWindow, IDC_LABEL_FILENAME, String<char>::LoadStr(resID));
}

void CSnapshotProgressDlg::SetProgressText(int resID, int number)
{
    CALL_STACK_MESSAGE3("CSnapshotProgressDlg::SetProgressText(%d, %d)", resID, number);
    char plural[200];
    CQuadWord qwnumber = CQuadWord(number, 0);
    SalamanderGeneral->ExpandPluralString(plural, _countof(plural), String<char>::LoadStr(resID), 1, &qwnumber);
    char text[200];
    _snprintf_s(text, _TRUNCATE, plural, number);
    SetDlgItemText(HWindow, IDC_LABEL_FILENAME, text);
}

void CSnapshotProgressDlg::SetProgress(DWORD progress)
{
    CALL_STACK_MESSAGE2("CSnapshotProgressDlg::SetProgress(0x%X)", progress);
    ProgressBar->SetProgress(progress, NULL);
}

BOOL CSnapshotProgressDlg::GetWantCancel()
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CSnapshotProgressDlg::GetWantCancel()");
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // we want responsive GUI
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return WantCancel;
}

INT_PTR CSnapshotProgressDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSnapshotProgressDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        ProgressBar = SalamanderGUI->AttachProgressBar(HWindow, IDC_PROGRESSBAR);
        if (ProgressBar == NULL)
        {
            DestroyWindow(HWindow);
            return FALSE;
        }
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // we want focus from DefDlgProc
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
        {
            WantCancel = TRUE;
            return TRUE;
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
//  CCopyProgressDlg
//

CCopyProgressDlg::CCopyProgressDlg(HWND parent, CObjectOrigin origin)
    : CDialog(HLanguage, IDD_COPYPROGRESSDLG, parent, origin)
{
    CALL_STACK_MESSAGE1("CCopyProgressDlg::CCopyProgressDlg(, )");
    ProgressBar1 = ProgressBar2 = NULL;
    WantCancel = FALSE;
    FileProgress = TotalProgress = 0;
    SrcName[0] = DestName[0] = 0;
    Changed[0] = Changed[1] = Changed[2] = Changed[3] = 0;
    LastTick = 0;
}

void CCopyProgressDlg::SetSourceFileName(const char* fileName)
{
    CALL_STACK_MESSAGE2("CCopyProgressDlg::SetSourceFileName(%s)", fileName);
    strcpy(SrcName, fileName);
    Changed[0] = TRUE;
    UpdateControls();
}

void CCopyProgressDlg::SetDestFileName(const char* fileName)
{
    CALL_STACK_MESSAGE2("CCopyProgressDlg::SetDestFileName(%s)", fileName);
    strcpy(DestName, fileName);
    Changed[1] = TRUE;
    UpdateControls();
}

void CCopyProgressDlg::SetFileProgress(DWORD progress)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CCopyProgressDlg::SetFileProgress(0x%X)", progress);
    FileProgress = progress;
    Changed[2] = TRUE;
    UpdateControls();
}

void CCopyProgressDlg::SetTotalProgress(DWORD progress)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CCopyProgressDlg::SetTotalProgress(0x%X)", progress);
    TotalProgress = progress;
    Changed[3] = TRUE;
    UpdateControls();
}

void CCopyProgressDlg::UpdateControls(BOOL now)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CCopyProgressDlg::UpdateControls(%d)", now);
    DWORD tick = GetTickCount();
    if (now || tick - LastTick >= 50)
    {
        if (Changed[0])
        {
            Label1->SetText(SrcName);
            Changed[0] = FALSE;
        }
        if (Changed[1])
        {
            Label2->SetText(DestName);
            Changed[1] = FALSE;
        }
        if (Changed[2])
        {
            ProgressBar1->SetProgress(FileProgress, NULL);
            Changed[2] = FALSE;
        }
        if (Changed[3])
        {
            ProgressBar2->SetProgress(TotalProgress, NULL);
            Changed[3] = FALSE;
        }
        LastTick = tick;
    }
}

BOOL CCopyProgressDlg::GetWantCancel()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CCopyProgressDlg::GetWantCancel()");

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // we want responsive GUI
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UpdateControls();
    return WantCancel;
}

INT_PTR CCopyProgressDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCopyProgressDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        ProgressBar1 = SalamanderGUI->AttachProgressBar(HWindow, IDC_PROGRESS_FILE);
        ProgressBar2 = SalamanderGUI->AttachProgressBar(HWindow, IDC_PROGRESS_TOTAL);
        Label1 = SalamanderGUI->AttachStaticText(HWindow, IDC_LABEL_SOURCE, STF_PATH_ELLIPSIS);
        Label2 = SalamanderGUI->AttachStaticText(HWindow, IDC_LABEL_DEST, STF_PATH_ELLIPSIS);
        if (ProgressBar1 == NULL || ProgressBar2 == NULL || Label1 == NULL || Label2 == NULL)
        {
            DestroyWindow(HWindow);
            return FALSE;
        }
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
        {
            if (!WantCancel)
            {
                UpdateControls(TRUE);
                if (SalamanderGeneral->SalMessageBox(HWindow, String<char>::LoadStr(IDS_WANTCANCEL),
                                                     String<char>::LoadStr(IDS_QUESTION),
                                                     MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    WantCancel = TRUE;
                }
            }
            return TRUE;
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
//  CConnectDialog
//

CConnectDialog::CConnectDialog(HWND parent, int panel)
    : CDialog(HLanguage, IDD_CONNECT, IDD_CONNECT, parent)
{
    Volume[0] = 0;
    hDrivesImg = NULL;
    Panel = panel;
}

void CConnectDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CConnectDialog::Transfer()");
    ti.CheckBox(IDC_CHECK_SCANVACANT, ConfigScanVacantClusters);
    ti.CheckBox(IDC_CHECK_SHOWEXISTING, ConfigShowExistingFiles);
    ti.CheckBox(IDC_CHECK_SHOWZEROFILES, ConfigShowZeroFiles);
    ti.CheckBox(IDC_CHECK_SHOWEMPTYDIRS, ConfigShowEmptyDirs);
    ti.CheckBox(IDC_CHECK_SHOWMETAFILES, ConfigShowMetafiles);
    ti.CheckBox(IDC_CHECK_ESTIMATEDAMAGE, ConfigEstimateDamage);
}

void CConnectDialog::AddVolumeDetails(const char* root, const char* volumeName, const char* volumeFS,
                                      const CQuadWord& bytesTotal, const CQuadWord& bytesFree,
                                      const char* volumeGUIDPath, int serial, BOOL selected)
{
    LVITEM itemInfo;
    itemInfo.iItem = serial;

    itemInfo.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    itemInfo.iSubItem = 0;
    itemInfo.pszText = const_cast<char*>(root);
    itemInfo.iImage = serial;
    itemInfo.stateMask = LVIS_FOCUSED; // using LVIS_FOCUSED instead of LVIS_SELECTED (we can switch to multi-select ListView)
    if (selected)
        itemInfo.state = LVIS_FOCUSED;
    else
        itemInfo.state = 0;
    itemInfo.iItem = (int)SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&itemInfo);

    itemInfo.mask = LVIF_TEXT;

    itemInfo.iSubItem = 1;
    itemInfo.pszText = const_cast<char*>(volumeName);
    SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&itemInfo);

    itemInfo.iSubItem = 2;
    itemInfo.pszText = const_cast<char*>(volumeFS);
    SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&itemInfo);

    itemInfo.iSubItem = 3;
    char buf[150];
    static char emptyBuff[] = "";
    itemInfo.pszText = (bytesTotal != CQuadWord(-1, -1) ? SalamanderGeneral->PrintDiskSize(buf, bytesTotal, 0) : emptyBuff);
    SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&itemInfo);

    itemInfo.iSubItem = 4;
    itemInfo.pszText = (bytesFree != CQuadWord(-1, -1) ? SalamanderGeneral->PrintDiskSize(buf, bytesFree, 0) : emptyBuff);
    SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&itemInfo);

    itemInfo.iSubItem = 5;
    if (bytesTotal != CQuadWord(-1, -1))
    {
        double pct = (CQuadWord(1000, 0) * bytesFree / bytesTotal).GetDouble() / 10;
        char buf2[15];
        sprintf(buf2, "%5.1f %%", pct);
        SalamanderGeneral->PointToLocalDecimalSeparator(buf2, _countof(buf2));
        itemInfo.pszText = buf2;
    }
    else
        itemInfo.pszText = emptyBuff;
    SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&itemInfo);

    itemInfo.iSubItem = 6;
    itemInfo.pszText = const_cast<char*>(volumeGUIDPath);
    SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&itemInfo);
}

void CConnectDialog::InitDrives()
{
    CALL_STACK_MESSAGE1("CConnectDialog::InitDrives()");

    // set controls status
    EnableWindow(GetDlgItem(HWindow, IDC_EDIT_IMAGE), FALSE);
    EnableWindow(GetDlgItem(HWindow, IDC_BUTTON_BROWSE), FALSE);
    hList = GetDlgItem(HWindow, IDC_LIST_VOLUMES);
    SendMessage(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    // initialize the list view

    // insert columns (column widths will be set at the end using autosize)
    const int columnsCount = 7;
    int headerResId[columnsCount] = {IDS_VOLUME_MOUNT, IDS_VOLUME_NAME, IDS_VOLUME_FORMAT, IDS_VOLUME_SIZE, IDS_VOLUME_FREE, IDS_VOLUME_FREEPROC, IDS_VOLUME_VOLID};
    LVCOLUMN columnInfo;
    columnInfo.mask = LVCF_TEXT | LVCF_FMT;
    int i;
    for (i = 0; i < columnsCount; i++)
    {
        columnInfo.pszText = String<char>::LoadStr(headerResId[i]);
        if (i >= 3 && i <= 5)
            columnInfo.fmt = LVCFMT_RIGHT;
        else
            columnInfo.fmt = LVCFMT_LEFT;
        SendMessage(hList, LVM_INSERTCOLUMN, i, (LPARAM)&columnInfo);
    }

    // prepare the image list and fill in the list view
    hDrivesImg = ImageList_Create(16, 16, SalamanderGeneral->GetImageListColorFlags() | ILC_MASK, 0, 1);

    // get info about current panel and focused item
    int sourcePanelType;
    char sourcePanelPath[MAX_PATH];
    char* archiveOrFS = NULL;
    sourcePanelPath[0] = 0;
    sourcePanelPath[1] = 0;
    BOOL ret = SalamanderGeneral->GetPanelPath(Panel, sourcePanelPath, MAX_PATH, &sourcePanelType, &archiveOrFS);
    if (ret)
    {
        switch (sourcePanelType)
        {
        // DOS/WIN or UNC path
        case PATH_TYPE_WINDOWS:
        {
            // pre-fill the disk image path with focused file
            BOOL isDir;
            const CFileData* data = SalamanderGeneral->GetPanelFocusedItem(Panel, &isDir);
            if (data && !isDir)
            {
                size_t len = MAX_PATH - strlen(sourcePanelPath);
                if (len - 1 > data->NameLen)
                {
                    strcat(sourcePanelPath, "\\");
                    strncat(sourcePanelPath, data->Name, len - 1);
                    sourcePanelPath[MAX_PATH - 1] = 0;
                    SetDlgItemText(HWindow, IDC_EDIT_IMAGE, sourcePanelPath);
                }
            }
            break;
        }

        case PATH_TYPE_FS:
        {
            // remove the "del:" prefix so correct path will be selected when opening this dialog box on Undelete path
            if (archiveOrFS != NULL)
                memmove(sourcePanelPath, archiveOrFS + 1, strlen(archiveOrFS + 1) + 1);
            break;
        }

        // can't do much about this
        default:
            break;
        }
    }

    char sourcePanelGUIDPath[MAX_PATH];
    sourcePanelGUIDPath[0] = 0;
    if (!SalamanderGeneral->GetResolvedPathMountPointAndGUID(sourcePanelPath, NULL, sourcePanelGUIDPath))
        sourcePanelGUIDPath[0] = 0;

    SendMessage(hList, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)hDrivesImg);

    VolumeListing<char> volumeListing;
    DWORD err = GetVolumeListing(volumeListing);
    if (err != 0)
    {
        char buf[1024];
        buf[0] = 0;
        strcpy(buf, String<char>::LoadStr(IDS_VOLENUMERATE));
        if (err != ERROR_SUCCESS)
        {
            strcat(buf, " ");
            int l = (int)strlen(buf);
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
        }
        SalamanderGeneral->SalMessageBox(HWindow, buf, String<char>::LoadStr(IDS_UNDELETE), MSGBOXEX_OK);
    }
    else
    {
        int serial = 0;
        for (i = 0; i < volumeListing.Count; ++i)
        {
            // for simple disks get the icon from system, for mount points or unmounted disks get just plain HDD icon
            // (passing MountPoints instead of volumeName is much faster for some disks)
            HICON icn;
            if (volumeListing[i].MountPoint && (3 == strlen(volumeListing[i].MountPoint)))
                icn = OS<char>::OS_GetDriveIcon(volumeListing[i].MountPoint, volumeListing[i].Type, TRUE, FALSE);
            else
                icn = OS<char>::OS_GetDriveIcon("", DRIVE_FIXED, TRUE, FALSE);

            ImageList_AddIcon(hDrivesImg, icn);
            DestroyIcon(icn);

            // add volume details to the list view
            BOOL selected = FALSE;
            if (sourcePanelGUIDPath[0] != 0)
            {
                selected = (strcmp(volumeListing[i].GUIDPath, sourcePanelGUIDPath) == 0);
            }
            else
            {
                if (volumeListing[i].MountPoint && volumeListing[i].MountPoint[0] != 0 &&
                    SalamanderGeneral->PathIsPrefix(volumeListing[i].MountPoint, sourcePanelPath))
                {
                    selected = true;
                }
            }

            // strip trailing slash from mount points
            if (volumeListing[i].MountPoint && strlen(volumeListing[i].MountPoint) > 3)
                SalamanderGeneral->SalPathRemoveBackslash(volumeListing[i].MountPoint);

            AddVolumeDetails(volumeListing[i].MountPoint, volumeListing[i].VolumeName, volumeListing[i].FSName,
                             volumeListing[i].BytesTotal, volumeListing[i].BytesFree, volumeListing[i].GUIDPath,
                             serial, selected);

            serial++;
        }
    }

    // we have columns header and content, we can (auto)adjust columns width
    for (i = 0; i < columnsCount; i++)
        ListView_SetColumnWidth(hList, i, LVSCW_AUTOSIZE_USEHEADER);

    // select item and ensure it is visible
    int selectIndex = ListView_GetNextItem(hList, -1, LVIS_FOCUSED);
    if (selectIndex == -1)
        selectIndex = 0;
    ListView_SetItemState(hList, selectIndex, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    ListView_EnsureVisible(hList, selectIndex, FALSE);
}

BOOL CConnectDialog::OnDialogOK()
{
    CALL_STACK_MESSAGE1("CConnectDialog::OnDialogOK()");

    // check if dealing with volume or image
    if (BST_CHECKED == SendMessage(GetDlgItem(HWindow, IDC_CHECK_IMAGE), BM_GETCHECK, 0, 0))
    {
        // disk image
        GetDlgItemText(HWindow, IDC_EDIT_IMAGE, Volume, MAX_PATH);

        // reset the volume if the image file does not exist
        DWORD attr = SalamanderGeneral->SalGetFileAttributes(Volume);
        if (attr == INVALID_FILE_ATTRIBUTES ||
            attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            SalamanderGeneral->SalMessageBox(HWindow, String<char>::LoadStr(IDS_IMAGENOTFOUND),
                                             String<char>::LoadStr(IDS_UNDELETE), MSGBOXEX_ICONEXCLAMATION | MSGBOXEX_OK);
            return FALSE;
        }
    }
    else
    {
        // physical disk
        HWND hList2 = GetDlgItem(HWindow, IDC_LIST_VOLUMES);
        LVITEM item;
        item.iItem = (int)SendMessage(hList2, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED);
        // get the unique volume name
        item.iSubItem = 0;
        item.mask = LVIF_TEXT;
        item.pszText = Volume;
        item.cchTextMax = MAX_PATH;
        SendMessage(hList2, LVM_GETITEM, 0, (LPARAM)&item);
        if (Volume[0] == 0)
        {
            // get GUID Path
            item.iSubItem = 6;
            SendMessage(hList2, LVM_GETITEM, 0, (LPARAM)&item);
        }

        // append current path, if current drive is selected
        int sourcePanelType;
        char sourcePanelPath[MAX_PATH];
        BOOL ret = SalamanderGeneral->GetPanelPath(Panel, sourcePanelPath, MAX_PATH, &sourcePanelType, NULL);
        if (ret && sourcePanelType == PATH_TYPE_WINDOWS)
        {
            // if mount points are supported, check if we are on the correct volume
            char vol1[MAX_PATH], vol2[MAX_PATH];
            if (GetVolumePathName(sourcePanelPath, vol1, MAX_PATH) &&
                GetVolumePathName(Volume, vol2, MAX_PATH) &&
                !strcmp(vol1, vol2))
                strcpy(Volume, sourcePanelPath);
        }
    }
    return TRUE;
}

void CConnectDialog::OnImageBrowse()
{
    GetDlgItemText(HWindow, IDC_EDIT_IMAGE, Volume, MAX_PATH);

    OPENFILENAME openInfo;
    memset(&openInfo, 0, sizeof(OPENFILENAME));
    openInfo.lStructSize = sizeof(OPENFILENAME);
    openInfo.hwndOwner = HWindow;
    openInfo.lpstrFilter = "Image Files (*.img;*.ima)\0*.IMG;*.IMA\0AllFiles (*.*)\0*.*\0\0\0";
    openInfo.lpstrFile = Volume;
    // TODO: porad to tady smrdi, kdyz je volume treba C:\Work\Altap\, tak init dir je svinstvo kvuli lpstrFile
    openInfo.lpstrInitialDir = Volume;
    openInfo.nMaxFile = MAX_PATH;
    openInfo.Flags = OFN_FILEMUSTEXIST | OFN_READONLY;
    BOOL ret = GetOpenFileName(&openInfo);
    if (!ret && FNERR_INVALIDFILENAME == CommDlgExtendedError())
    {
        // Windows refuse to open dialog with initial path e.g. C:\. Oh well...
        strcpy(Volume, "");
        ret = GetOpenFileName(&openInfo);
    }
    if (ret)
        SetDlgItemText(HWindow, IDC_EDIT_IMAGE, Volume);
}

INT_PTR CConnectDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConnectDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        if (IsAppThemed())
            SetWindowTheme(GetDlgItem(HWindow, IDC_LIST_VOLUMES), L"explorer", NULL);
        InitDrives();
        break;
    }

    case WM_DESTROY:
    {
        // if (hDrivesImg) ImageList_Destroy(hDrivesImg);  // image-list will be released by listview
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(HWindow, FALSE);
            return TRUE;

        case IDOK:
            if (!OnDialogOK())
                return TRUE;
            break;

        case IDC_CHECK_IMAGE:
        {
            BOOL checked = (SendDlgItemMessage(HWindow, IDC_CHECK_IMAGE, BM_GETCHECK, 0, 0) == BST_CHECKED);
            EnableWindow(GetDlgItem(HWindow, IDC_LIST_VOLUMES), !checked);
            EnableWindow(GetDlgItem(HWindow, IDC_EDIT_IMAGE), checked);
            EnableWindow(GetDlgItem(HWindow, IDC_BUTTON_BROWSE), checked);
            return TRUE;
        }

        case IDC_BUTTON_BROWSE:
            OnImageBrowse();
            return TRUE;
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
//  CFileNameDialog
//

CFileNameDialog::CFileNameDialog(HWND parent, char* filename)
    : CDialog(HLanguage, IDD_FILENAME, parent)
{
    FileName = filename;
    AllPressed = FALSE;
}

void CFileNameDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFileNameDialog::Transfer()");
    ti.EditLine(IDC_EDIT_FILENAME, FileName, MAX_PATH, FALSE);
}

INT_PTR CFileNameDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFileNameDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        TransferData(ttDataToWindow);
        SetFocus(GetDlgItem(HWindow, IDC_EDIT_FILENAME));
        SendDlgItemMessage(HWindow, IDC_EDIT_FILENAME, EM_SETSEL, 0, 1);
        AllPressed = FALSE;
        return FALSE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(HWindow, FALSE);
            return TRUE;

        case IDC_BUTTON_ALL:
            AllPressed = TRUE;
            return CDialog::DialogProc(uMsg, IDOK, 0);
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
//  CConfigDialog
//

CConfigDialog::CConfigDialog(HWND parent)
    : CDialog(HLanguage, IDD_CONFIG, IDD_CONFIG, parent)
{
}

void CConfigDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CConfigDialog::Transfer()");
    ti.EditLine(IDC_EDIT_TEMPPATH, ConfigTempPath, MAX_PATH, FALSE);
    ti.CheckBox(IDC_CHECK_ALWAYSREUSE, ConfigAlwaysReuseScanInfo);
    ti.CheckBox(IDC_CHECK_SAMEPARTITION, ConfigDontShowSamePartitionWarning);
    ti.CheckBox(IDC_CHECK_EFS, ConfigDontShowEncryptedWarning);
}

INT_PTR CConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_BROWSE:
        {
            char path[MAX_PATH];
            GetDlgItemText(HWindow, IDC_EDIT_TEMPPATH, path, MAX_PATH);
            SalamanderGeneral->GetTargetDirectory(HWindow, HWindow, String<char>::LoadStr(IDS_UNDELETE),
                                                  String<char>::LoadStr(IDS_CHOOSETEMPDIR),
                                                  path, FALSE, path);
            SetDlgItemText(HWindow, IDC_EDIT_TEMPPATH, path);
            return TRUE;
        }
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
//  CRestoreDialog
//

CRestoreDialog::CRestoreDialog(HWND parent)
    : CDialog(HLanguage, IDD_RESTORE, IDD_RESTORE, parent)
{
}

INT_PTR CRestoreDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRestoreDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    char path[MAX_PATH + 300];
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);

        SalamanderGeneral->GetPanelPath(PANEL_TARGET, path, MAX_PATH, NULL, NULL);
        SetDlgItemText(HWindow, IDC_EDIT_TARGET, path);

        int files, dirs;
        char text1[200], text2[MAX_PATH + 100];
        SalamanderGeneral->GetPanelSelection(PANEL_SOURCE, &files, &dirs);
        SalamanderGeneral->GetCommonFSOperSourceDescr(text2, MAX_PATH + 100, PANEL_SOURCE, files, dirs, NULL, FALSE, FALSE);
        GetDlgItemText(HWindow, IDC_LABEL_SOURCE, text1, 200);
        BOOL labelSet = FALSE;
        // pokud jde o "file \"name.txt\"" nebo "directory \"name\"" najdeme jmeno a zbytek
        // retezce pridame do text1, aby sla pouzit CSalamanderGUI::SetSubjectTruncatedText
        if (files + dirs <= 1)
        {
            char* beg = strchr(text2, '"');
            char* end = strrchr(text2, '"');
            if (beg != NULL && end != NULL && beg < end && end - (beg + 1) < MAX_PATH)
            {
                char fileName[MAX_PATH];
                lstrcpyn(fileName, beg + 1, (int)(end - (beg + 1) + 1));
                memmove(beg + 1 + 2, end, strlen(end) + 1);
                memcpy(beg + 1, "%s", 2);
                _snprintf_s(path, _TRUNCATE, text1, text2);

                BOOL isDir = dirs == 1;
                if (files + dirs == 0)
                    SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
                SalamanderGUI->SetSubjectTruncatedText(GetDlgItem(HWindow, IDC_LABEL_SOURCE), path,
                                                       fileName, isDir, TRUE);
                labelSet = TRUE;
            }
        }
        if (!labelSet)
        {
            _snprintf_s(path, _TRUNCATE, text1, text2);
            SetDlgItemText(HWindow, IDC_LABEL_SOURCE, path);
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_BROWSE:
        {
            char title[100];
            GetDlgItemText(HWindow, IDC_EDIT_TARGET, path, MAX_PATH);
            GetWindowText(HWindow, title, 100);
            SalamanderGeneral->GetTargetDirectory(HWindow, HWindow, title, String<char>::LoadStr(IDS_CHOOSETARGET),
                                                  path, FALSE, path);
            SetDlgItemText(HWindow, IDC_EDIT_TARGET, path);
            return TRUE;
        }

        case IDOK:
        {
            GetDlgItemText(HWindow, IDC_EDIT_TARGET, TargetPath, MAX_PATH);
            break;
        }
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
//  CRestoreProgressDlg
//

INT_PTR CRestoreProgressDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRestoreProgressDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG)
    {
        SetDlgItemText(HWindow, IDC_LABEL_UNDELETING, String<char>::LoadStr(IDS_RESTORING));
        SetWindowText(HWindow, String<char>::LoadStr(IDS_RESTORE));
        /*HWND hLabel = GetDlgItem(HWindow, IDC_LABEL_UNDELETING);
    SetWindowLong(hLabel, GWL_STYLE, GetWindowLong(hLabel, GWL_STYLE) | SS_RIGHT);*/
    }
    return CCopyProgressDlg::DialogProc(uMsg, wParam, lParam);
}
