// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "usermenu.h"
#include "toolbar.h"
#include "shellib.h"
#include "cfgdlg.h"
#include "execute.h"
#include "drivelst.h"
#include "plugins.h"
#include "fileswnd.h"
#include "shiconov.h"

//*****************************************************************************
//
// CDriveBar
//

CDriveBar::CDriveBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin)
{
    CALL_STACK_MESSAGE_NONE
    List = NULL;
    // tato inicializace je take ve WM_DESTROY, protoze okno se pouze zhasina a rozsveci
    CheckedDrive[0] = 0;
    HDrivesIcons = NULL;
    HDrivesIconsGray = NULL;
}

CDriveBar::~CDriveBar()
{
    DestroyImageLists();
}

void CDriveBar::DestroyImageLists()
{
    if (HDrivesIcons != NULL)
    {
        ImageList_Destroy(HDrivesIcons);
        HDrivesIcons = NULL;
    }
    if (HDrivesIconsGray != NULL)
    {
        ImageList_Destroy(HDrivesIconsGray);
        HDrivesIconsGray = NULL;
    }
}

BOOL CDriveBar::CreateDriveButtons(CDriveBar* copyDrivesListFrom)
{
    CALL_STACK_MESSAGE1("CDriveBar::CreateButtons()");
    if (HWindow == NULL)
        return FALSE;

    // potlacime kresleni, abychom predesli mrkani checked polozky (staci otevrit a zavrit plugins manager a mrkalo by to)
    // take jsme mrkali pri spusteni salamandera
    SendMessage(HWindow, WM_SETREDRAW, FALSE, 0);
    RemoveAllItems();
    SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);

    TDirectArray<CDriveData>* copyDrives = NULL;
    BOOL destroyCopyDrives = FALSE;
    DWORD copyCachedDrivesMask = 0;

    DriveType = drvtUnknow;
    DriveTypeParam = 0;
    if (List == NULL)
    {
        List = new CDrivesList(MainWindow->GetActivePanel(), "", (CDriveTypeEnum*)&DriveType, &DriveTypeParam,
                               &PostCmd, &PostCmdParam, &FromContextMenu);
    }
    else
    {
        if (copyDrivesListFrom == this)
        {
            copyDrives = List->AllocNewDrivesAndGetCurrentDrives();
            destroyCopyDrives = TRUE;
            copyCachedDrivesMask = List->GetCachedDrivesMask();
        }
        List->DestroyData();
    }
    if (copyDrivesListFrom != NULL && copyDrivesListFrom != this &&
        copyDrivesListFrom->List != NULL)
    {
        copyDrives = copyDrivesListFrom->List->GetDrives();
        copyCachedDrivesMask = copyDrivesListFrom->List->GetCachedDrivesMask();
    }
    List->BuildData(FALSE, copyDrives, copyCachedDrivesMask, TRUE, TRUE);
    if (destroyCopyDrives)
    {
        List->DestroyDrives(copyDrives);
        delete copyDrives;
    }
    List->FillDriveBar(this, this == MainWindow->DriveBar2);
    if (MainWindow->DriveBar2->HWindow == NULL)
        SetCheckedDrive(MainWindow->GetActivePanel(), TRUE);
    else
    {
        CFilesWindow* panel = this == MainWindow->DriveBar2 ? MainWindow->RightPanel : MainWindow->LeftPanel;
        SetCheckedDrive(panel, TRUE);
    }
    SendMessage(HWindow, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(HWindow, NULL, FALSE);

    return TRUE;
}

int CDriveBar::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    // i v pripade, ze nedrzime zadnou ikonu budeem vracet spravnou vysku
    int height = CToolBar::GetNeededHeight();
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    int minH = 3 + iconSize + 3;
    if (height < minH)
        height = minH;
    return height;
}

void CDriveBar::Execute(DWORD id)
{
    if (List != NULL)
    {
        BOOL bar2 = this == MainWindow->DriveBar2;
        int index = id - (bar2 ? CM_DRIVEBAR2_MIN : CM_DRIVEBAR_MIN);
        RECT r;
        GetItemRect(index, r);

        BOOL fromDropDown;
        if (List->ExecuteItem(index, MainWindow->HWindow, &r, &fromDropDown))
        {
            CFilesWindow* panel;
            if (MainWindow->DriveBar2->HWindow != NULL)
                panel = bar2 ? MainWindow->RightPanel : MainWindow->LeftPanel;
            else
                panel = MainWindow->GetActivePanel();

            if (DriveType != drvtPluginCmd)
                panel->TopIndexMem.Clear(); // dlouhy skok

            char path[MAX_PATH];
            switch (DriveType)
            {
            case drvtMyDocuments:
            case drvtGoogleDrive:
            case drvtDropbox:
            case drvtOneDrive:    // bud primo tlacitko nebo vybrane z drop down menu
            case drvtOneDriveBus: // bud primo tlacitko nebo vybrane z drop down menu
            {
                panel->ChangePathToDrvType(HWindow, DriveType, DriveType == drvtOneDriveBus ? (const char*)DriveTypeParam : NULL);
                if (DriveType == drvtOneDriveBus)
                    free((char*)DriveTypeParam);
                if ((DriveType == drvtOneDrive || DriveType == drvtOneDriveBus) &&
                    !fromDropDown && GetOneDriveStorages() > 1)
                { // OneDrive by zase mel byt drop down, provedeme refresh obou Drive bar, at se updatne tlacitko
                    if (MainWindow != NULL && MainWindow->HWindow != NULL)
                        PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
                }
                return;
            };

            case drvtOneDriveMenu:
                TRACE_E("CDriveBar::Execute(): unexpected drive type: drvtOneDriveMenu");
                return;

            // jde o Network?
            case drvtNeighborhood:
            {
                if (GetTargetDirectory(panel->HWindow, panel->HWindow, LoadStr(IDS_CHANGEDRIVE),
                                       LoadStr(IDS_CHANGEDRIVETEXT), path, TRUE))
                {
                    UpdateWindow(MainWindow->HWindow);
                    panel->ChangePathToDisk(panel->HWindow, path);
                }
                return;
            }

            case drvtRemovable:
            case drvtFixed:
            case drvtRemote:
            case drvtCDROM:
            case drvtRAMDisk:
            {
                char drive = (char)DriveTypeParam;
                panel->ChangePathToDisk(HWindow, DefaultDir[LowerCase[drive] - 'a'], -1, NULL,
                                        NULL, TRUE, FALSE, FALSE, NULL, FALSE);
                return;
            }

            case drvtPluginCmd:
            {
                // kod prevzaty z fileswn3.cpp, CFilesWindow::ChangeDrive()
                const char* dllName = (const char*)DriveTypeParam;
                CPluginData* data = Plugins.GetPluginData(dllName);
                if (data != NULL) // plug-in existuje, jdeme spustit prikaz
                    data->ExecuteChangeDriveMenuItem(panel == MainWindow->LeftPanel ? PANEL_LEFT : PANEL_RIGHT);
                return;
            }
            }
        }
    }
}

void CDriveBar::SetCheckedDrive(CFilesWindow* panel, BOOL force)
{
    BOOL isDiskOrArchive = panel->Is(ptDisk) || panel->Is(ptZIPArchive);
    if (!force && isDiskOrArchive && strnicmp(CheckedDrive, panel->GetPath(), 2) == 0) // cache
        return;
    if (isDiskOrArchive)
        lstrcpyn(CheckedDrive, panel->GetPath(), 3);
    else
        CheckedDrive[0] = 0; // pro FS tato cache nefunguje
    DWORD index;
    if (List == NULL || !List->FindPanelPathIndex(panel, &index))
        index = -1;
    int i;
    for (i = 0; i < GetItemCount(); i++)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_ID | TLBI_MASK_STYLE;
        if (GetItemInfo2(i, TRUE, &tii) && !(tii.Style & TLBI_STYLE_SEPARATOR))
        {
            DWORD indexInList = tii.ID;
            if (indexInList >= CM_DRIVEBAR_MIN && indexInList <= CM_DRIVEBAR_MAX)
                indexInList -= CM_DRIVEBAR_MIN;
            else
            {
                if (indexInList >= CM_DRIVEBAR2_MIN && indexInList <= CM_DRIVEBAR2_MAX)
                    indexInList -= CM_DRIVEBAR2_MIN;
                else
                    indexInList = -2; // nemelo by se stat
            }
            CheckItem(i, TRUE, index == indexInList);
        }
    }
}

void CDriveBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CDriveBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    tt->Buffer[0] = 0;
    if (List != NULL)
    {
        DWORD id = tt->ID;
        if (id >= CM_DRIVEBAR_MIN && id <= CM_DRIVEBAR_MAX)
            List->GetDriveBarToolTip(id - CM_DRIVEBAR_MIN, tt->Buffer);
        else if (id >= CM_DRIVEBAR2_MIN && id <= CM_DRIVEBAR2_MAX)
            List->GetDriveBarToolTip(id - CM_DRIVEBAR2_MIN, tt->Buffer);
    }
}

void CDriveBar::RebuildDrives(CDriveBar* copyDrivesListFrom)
{
    CreateDriveButtons(copyDrivesListFrom);
}

BOOL CDriveBar::OnContextMenu()
{
    DWORD pos = GetMessagePos();
    POINT p;
    p.x = GET_X_LPARAM(pos);
    p.y = GET_Y_LPARAM(pos);
    ScreenToClient(HWindow, &p);
    int index = HitTest(p.x, p.y);
    if (List != NULL && index >= 0)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_ID;
        if (GetItemInfo2(index, TRUE, &tii))
        {
            DWORD indexInList = tii.ID;
            if (indexInList >= CM_DRIVEBAR_MIN && indexInList <= CM_DRIVEBAR_MAX)
                indexInList -= CM_DRIVEBAR_MIN;
            else
            {
                if (indexInList >= CM_DRIVEBAR2_MIN && indexInList <= CM_DRIVEBAR2_MAX)
                    indexInList -= CM_DRIVEBAR2_MIN;
                else
                    return FALSE; // nemelo by se stat
            }
            CFilesWindow* panel;
            BOOL bar2 = this == MainWindow->DriveBar2;
            if (MainWindow->DriveBar2->HWindow != NULL)
                panel = bar2 ? MainWindow->RightPanel : MainWindow->LeftPanel;
            else
                panel = MainWindow->GetActivePanel();

            PostCmd = 0;
            PostCmdParam = NULL;
            FromContextMenu = FALSE;
            const char* dllName = NULL;
            List->OnContextMenu(TRUE, indexInList, panel == MainWindow->LeftPanel ? PANEL_LEFT : PANEL_RIGHT, &dllName);
            if (PostCmd != 0) // nastavuje se jen na drvtPluginFS a drvtPluginCmd, pricemz drvtPluginFS nemuze byt na Drive bare
            {
                UpdateWindow(MainWindow->HWindow);

                CPluginData* data = Plugins.GetPluginData(dllName);
                if (data != NULL) // plug-in existuje, jdeme spustit prikaz
                {                 // post-cmd z kontextoveho menu polozky FS
                    data->GetPluginInterfaceForFS()->ExecuteChangeDrivePostCommand(panel == MainWindow->LeftPanel ? PANEL_LEFT : PANEL_RIGHT,
                                                                                   PostCmd, PostCmdParam);
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

LRESULT
CDriveBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CDriveBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        if (List != NULL)
        {
            List->DestroyData();
            delete List;
            List = NULL;
        }
        CheckedDrive[0] = 0;
        break;
    }
    }
    return CToolBar::WindowProc(uMsg, wParam, lParam);
}

DWORD
CDriveBar::GetCachedDrivesMask()
{
    if (List == NULL)
        return 0;
    return List->GetCachedDrivesMask();
}

DWORD
CDriveBar::GetCachedCloudStoragesMask()
{
    if (List == NULL)
        return 0;
    return List->GetCachedCloudStoragesMask();
}
