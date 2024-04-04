// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "usermenu.h"
#include "toolbar.h"
#include "shellib.h"
#include "cfgdlg.h"
#include "execute.h"
#include "plugins.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"

//****************************************************************************
//
// CUMDropTarget
//

class CUMDropTarget : public IDropTarget
{
private:
    long RefCount;             // object lifespan
    IDataObject* DataObject;   // IDataObject that entered the drag
    CUserMenuBar* UserMenuBar; // bar we are associated with
    IDropTarget* DropTarget;
    char DropTargetFileName[MAX_PATH];

public:
    CUMDropTarget(CUserMenuBar* userMenuBar)
    {
        RefCount = 1;
        DataObject = NULL;
        UserMenuBar = userMenuBar;
        DropTarget = NULL;
        DropTargetFileName[0] = 0;
    }

    virtual ~CUMDropTarget()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
    }

    STDMETHOD(QueryInterface)
    (REFIID refiid, void FAR* FAR* ppv)
    {
        if (refiid == IID_IUnknown || refiid == IID_IDropTarget)
        {
            *ppv = this;
            AddRef();
            return NOERROR;
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
    }

    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // We must not access the object, it no longer exists
        }
        return RefCount;
    }

    void HitTest(POINTL pt, int& insertIndex, BOOL& after, int& pasteIndex, char* fileName, BOOL insert) // fileName buffer has a length of MAX_PATH
    {
        POINT p;
        p.x = pt.x;
        p.y = pt.y;
        ScreenToClient(UserMenuBar->HWindow, &p);

        int total = UserMenuBar->GetItemCount();
        int hitIndex = UserMenuBar->HitTest(p.x, p.y);

        int imIndex;
        int imAfter;
        pasteIndex = -1;
        if (!UserMenuBar->InsertMarkHitTest(p.x, p.y, imIndex, imAfter))
        {
            imIndex = -1;
            if (hitIndex >= 0)
                pasteIndex = hitIndex;
        }
        else
        {
            if (imIndex == -1)
                imIndex = 0;
        }

        if (!insert)
        {
            imIndex = -1;
            imAfter = FALSE;
            if (hitIndex >= 0)
                pasteIndex = hitIndex;
        }

        insertIndex = imIndex;
        after = imAfter;
        if (after)
            insertIndex++;

        if (pasteIndex != -1)
        {
            insertIndex = -1;
            // verify that it is the target
            TLBI_ITEM_INFO2 tii;
            tii.Mask = TLBI_MASK_ID;
            if (UserMenuBar->GetItemInfo2(pasteIndex, TRUE, &tii))
            {
                CUserMenuItem* item = MainWindow->UserMenuItems->At(tii.ID - CM_USERMENU_MIN);
                if (ExpandCommand(MainWindow->HWindow, item->UMCommand, fileName, MAX_PATH, TRUE))
                {
                    if (!HasDropTarget(fileName))
                        pasteIndex = -1;
                }
            }
        }

        UserMenuBar->SetInsertMark(imIndex, imAfter);
        UserMenuBar->SetHotItem(pasteIndex);
    }

    BOOL GetPathFromDataObject(IDataObject* pDataObject, char* path)
    {
        if (IsFakeDataObject(pDataObject, NULL, NULL, 0))
            return FALSE;

        FORMATETC formatEtc;
        formatEtc.cfFormat = CF_HDROP;
        formatEtc.ptd = NULL;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        STGMEDIUM stgMedium;
        stgMedium.tymed = TYMED_HGLOBAL;
        stgMedium.hGlobal = NULL;
        stgMedium.pUnkForRelease = NULL;

        BOOL ret = FALSE;
        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
            {
                DROPFILES* data = (DROPFILES*)HANDLES(GlobalLock(stgMedium.hGlobal));
                if (data != NULL)
                {
                    if (data->fWide)
                    {
                        const wchar_t* fileW = (wchar_t*)(((char*)data) + data->pFiles);
                        int l = lstrlenW(fileW);
                        if (*(fileW + l + 1) == 0)
                        {
                            WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, l + 1, NULL, NULL);
                            path[l] = 0;
                            ret = TRUE;
                        }
                    }
                    else
                    {
                        const char* fileA = ((char*)data) + data->pFiles;
                        int l = (int)strlen(fileA);
                        if (*(fileA + l + 1) == 0)
                        {
                            strcpy(path, fileA);
                            ret = TRUE;
                        }
                    }

                    HANDLES(GlobalUnlock(stgMedium.hGlobal));
                }
            }
            ReleaseStgMedium(&stgMedium);
        }
        if (ret && !FileExists(path))
            ret = FALSE;
        return ret;
    }

    STDMETHOD(DragEnter)
    (IDataObject* pDataObject, DWORD grfKeyState,
     POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragEnter(pt.x, pt.y);
        if (DataObject != NULL)
            DataObject->Release();
        DataObject = pDataObject;
        DataObject->AddRef();

        if (DataObject != NULL)
        {
            // perform a hit test
            int insertIndex = -1;
            BOOL after;
            int pasteIndex = -1;
            char fileName[MAX_PATH];

            char buff[MAX_PATH];
            BOOL insert = GetPathFromDataObject(pDataObject, buff);

            HitTest(pt, insertIndex, after, pasteIndex, fileName, insert);

            if (insertIndex != -1)
            {
                if (insert)
                    *pdwEffect = DROPEFFECT_LINK;
                else
                    *pdwEffect = DROPEFFECT_NONE;
            }
            else
            {
                if (pasteIndex != -1)
                {
                    if (strcmp(fileName, DropTargetFileName) != 0 &&
                        !IsFakeDataObject(pDataObject, NULL, NULL, 0))
                    {
                        if (DropTarget != NULL)
                        {
                            DropTarget->DragLeave();
                            DropTarget->Release();
                            DropTargetFileName[0] = 0;
                        }
                        DropTarget = CreateIDropTarget(UserMenuBar->HWindow, fileName);
                        if (DropTarget != NULL)
                            strcpy(DropTargetFileName, fileName);
                    }
                    if (DropTarget != NULL)
                    {
                        return DropTarget->DragEnter(pDataObject, grfKeyState, pt, pdwEffect);
                    }
                }
                *pdwEffect = DROPEFFECT_NONE;
            }
        }
        return S_OK;
    }

    STDMETHOD(DragOver)
    (DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragMove(pt.x, pt.y);
        if (DataObject != NULL)
        {
            // perform a hit test
            int insertIndex = -1;
            BOOL after;
            int pasteIndex = -1;
            char fileName[MAX_PATH];

            char buff[MAX_PATH];
            BOOL insert = GetPathFromDataObject(DataObject, buff);

            HitTest(pt, insertIndex, after, pasteIndex, fileName, insert);

            if (insertIndex != -1)
            {
                if (insert)
                    *pdwEffect = DROPEFFECT_LINK;
                else
                    *pdwEffect = DROPEFFECT_NONE;
            }
            else
            {
                if (pasteIndex != -1)
                {
                    if (strcmp(fileName, DropTargetFileName) != 0 &&
                        !IsFakeDataObject(DataObject, NULL, NULL, 0))
                    {
                        if (DropTarget != NULL)
                        {
                            DropTarget->DragLeave();
                            DropTarget->Release();
                            DropTargetFileName[0] = 0;
                        }
                        DropTarget = CreateIDropTarget(UserMenuBar->HWindow, fileName);
                        if (DropTarget != NULL)
                        {
                            strcpy(DropTargetFileName, fileName);
                            DropTarget->DragEnter(DataObject, grfKeyState, pt, pdwEffect);
                        }
                    }
                    if (DropTarget != NULL)
                    {
                        return DropTarget->DragOver(grfKeyState, pt, pdwEffect);
                    }
                }
                *pdwEffect = DROPEFFECT_NONE;
            }
        }
        return S_OK;
    }

    STDMETHOD(DragLeave)
    ()
    {
        if (ImageDragging)
            ImageDragLeave();
        if (DropTarget != NULL)
        {
            DropTarget->DragLeave();
            DropTarget->Release();
            DropTarget = NULL;
            DropTargetFileName[0] = 0;
        }
        if (DataObject != NULL)
        {
            DataObject->Release();
            DataObject = NULL;
        }

        // shoot down insert mark
        UserMenuBar->SetInsertMark(-1, FALSE);
        // shoot down hot item
        UserMenuBar->SetHotItem(-1);

        return E_UNEXPECTED;
    }

    STDMETHOD(Drop)
    (IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
     DWORD* pdwEffect)
    {
        HRESULT ret = E_UNEXPECTED;

        if (ImageDragging)
            ImageDragLeave();
        if (DataObject != NULL)
        {
            // perform a hit test
            int insertIndex = -1;
            BOOL after;
            int pasteIndex = -1;
            char fileName[MAX_PATH];

            char buff[MAX_PATH];
            BOOL insert = GetPathFromDataObject(pDataObject, buff);

            HitTest(pt, insertIndex, after, pasteIndex, fileName, insert);

            if (insertIndex != -1)
            {
                // shoot down insert mark
                UserMenuBar->SetInsertMark(-1, FALSE);

                if (insert)
                {
                    *pdwEffect = DROPEFFECT_LINK;
                    char name[MAX_PATH];
                    char* p = buff + strlen(buff);
                    while (p > buff && *p != '\\')
                        p--;
                    if (*p == '\\')
                        p++;
                    strcpy(name, p);

                    char tmp[MAX_PATH];
                    BOOL shell = FALSE;

                    // zmenil jsem metodu CMainWindow::UserMenu tak, ze pokud nespousti pres Shell,
                    // calls ShellExecuteEx instead of CreateProcess; therefore quotes are no longer needed
                    /*              // if it is not an executable file (.exe, .com, .bat, .pif),
            // enclose the name in quotes and run it through the shell
            char *dot = strrchr(buff, '.');
            if (dot != NULL && *(dot + 1) != 0)
            {
              dot++;
              BOOL executable = FALSE;
              if (stricmp(dot, "exe") == 0 ||
                  stricmp(dot, "com") == 0 ||
                  stricmp(dot, "bat") == 0 ||
                  stricmp(dot, "pif") == 0)
                executable = TRUE;

              if (!executable)
              {
                strcpy(tmp, buff);
                sprintf(buff, "\"%s\"", tmp);
                shell = TRUE;
              }
            }*/

                    // if there is a $ character in the path, I need to replace it with $$
                    strcpy(tmp, buff);
                    char* iterS = tmp;
                    char* iterT = buff;
                    while (*iterS != 0)
                    {
                        *iterT = *iterS;
                        if (*iterS == '$')
                        {
                            iterT++;
                            *iterT = '$';
                        }
                        iterT++;
                        iterS++;
                    }
                    *iterT = 0;

                    static char emptyBuffer[] = "";
                    static char fullPathBuffer[] = "$(FullPath)";
                    CUserMenuItem* item = new CUserMenuItem(name, buff, emptyBuffer, fullPathBuffer, emptyBuffer,
                                                            shell, FALSE, FALSE, TRUE, umitItem, NULL);

                    // Find the place where the item needs to be inserted
                    int count = 0;
                    int i;
                    for (i = 0; i < MainWindow->UserMenuItems->Count; i++)
                    {
                        CUserMenuItem* item2 = MainWindow->UserMenuItems->At(i);
                        if (count >= insertIndex)
                            break;

                        if (item2->Type == umitSubmenuBegin)
                        {
                            int endIndex = MainWindow->UserMenuItems->GetSubmenuEndIndex(i);
                            if (endIndex != -1)
                                i = endIndex;
                        }

                        if (item2->ShowInToolbar)
                            count++;
                    }
                    MainWindow->UserMenuItems->Insert(i, item);

                    if (UserMenuIconBkgndReader.IsReadingIcons()) // loading icons in progress = we need to restart it, the number of items in the user menu has changed (as a side effect, it messes up the just loaded icon of the droplet file, but I just ignore that)
                    {
                        CUserMenuIconDataArr* bkgndReaderData = new CUserMenuIconDataArr();
                        for (int i2 = 0; i2 < MainWindow->UserMenuItems->Count; i2++)
                            MainWindow->UserMenuItems->At(i2)->GetIconHandle(bkgndReaderData, FALSE);
                        UserMenuIconBkgndReader.StartBkgndReadingIcons(bkgndReaderData); // WARNING: release 'bkgndReaderData'
                    }

                    MainWindow->UMToolBar->CreateButtons();
                }
                else
                    *pdwEffect = DROPEFFECT_NONE;
            }
            else
            {
                if (pasteIndex != -1)
                {
                    if (strcmp(fileName, DropTargetFileName) != 0 &&
                        !IsFakeDataObject(pDataObject, NULL, NULL, 0))
                    {
                        if (DropTarget != NULL)
                        {
                            DropTarget->DragLeave();
                            DropTarget->Release();
                            DropTargetFileName[0] = 0;
                        }
                        DropTarget = CreateIDropTarget(UserMenuBar->HWindow, fileName);
                        if (DropTarget != NULL)
                            strcpy(DropTargetFileName, fileName);
                    }
                    if (DropTarget != NULL)
                    {
                        ret = DropTarget->Drop(pDataObject, grfKeyState, pt, pdwEffect);
                    }
                }
            }
        }

        if (DataObject != NULL)
        {
            DataObject->Release();
            DataObject = NULL;
        }

        if (DropTarget != NULL)
        {
            DropTarget->Release();
            DropTarget = NULL;
            DropTargetFileName[0] = 0;
        }

        return ret;
    }
};

//*****************************************************************************
//
// CUserMenuBar
//

CUserMenuBar::CUserMenuBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin){
          CALL_STACK_MESSAGE_NONE}

      BOOL CUserMenuBar::CreateButtons()
{
    CALL_STACK_MESSAGE1("CUserMenuBar::CreateButtons()");
    if (HWindow == NULL)
        return FALSE;

    RemoveAllItems();
    SetStyle(TLB_STYLE_IMAGE | (Configuration.UserMenuToolbarLabels ? TLB_STYLE_TEXT : 0));

    // add icons to a custom toolbar
    // Only insert items and submenus from the top level; the rest will expand as submenus
    int level = 0;
    TLBI_ITEM_INFO2 tii;
    int i;
    for (i = 0; i < MainWindow->UserMenuItems->Count; i++)
    {
        CUserMenuItem* item = MainWindow->UserMenuItems->At(i);
        switch (item->Type)
        {
        case umitSubmenuEnd:
        {
            level--;
            break;
        }

        case umitSeparator:
        {
            if (level == 0 && item->ShowInToolbar)
            {
                tii.Mask = TLBI_MASK_STYLE;
                tii.Style = TLBI_STYLE_SEPARATOR;
                InsertItem2(0xFFFFFFFF, TRUE, &tii);
            }
            break;
        }

        case umitItem:
        case umitSubmenuBegin:
        {
            if (level == 0 && item->ShowInToolbar)
            {
                tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_TEXT | TLBI_MASK_ICON |
                           TLBI_MASK_ID | TLBI_MASK_ENABLER;
                tii.Style = TLBI_STYLE_SHOWTEXT | TLBI_STYLE_NOPREFIX;
                if (item->Type == umitSubmenuBegin)
                    tii.Style |= TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN;
                char buff[80];
                lstrcpyn(buff, item->ItemName, 80);
                RemoveAmpersands(buff);
                tii.Text = buff;
                tii.HIcon = item->UMIcon;
                tii.ID = CM_USERMENU_MIN + i;
                tii.Enabler = &EnablerOnDisk;
                InsertItem2(0xFFFFFFFF, TRUE, &tii);
            }
            if (item->Type == umitSubmenuBegin)
                level++;
            break;
        }
        }
    }

    UpdateItemsState();

    return TRUE;
}

void CUserMenuBar::ToggleLabels()
{
    CALL_STACK_MESSAGE1("CUserMenuBar::ToggleLabels()");
    // set style
    DWORD style = GetStyle();
    if (Configuration.UserMenuToolbarLabels)
        style &= ~TLB_STYLE_TEXT;
    else
        style |= TLB_STYLE_TEXT;
    Configuration.UserMenuToolbarLabels = !Configuration.UserMenuToolbarLabels;
    SetStyle(style);
}

int CUserMenuBar::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    // and in case we don't hold any icon, we will return the correct height
    int height = CToolBar::GetNeededHeight();
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    int minH = 3 + iconSize + 3;
    if (height < minH)
        height = minH;
    return height;
}

void CUserMenuBar::Customize()
{
    CALL_STACK_MESSAGE_NONE
    // Let's unpack the UserMenu page and edit the index item
    PostMessage(MainWindow->HWindow, WM_USER_CONFIGURATION, 2, 0);
}

void CUserMenuBar::SetInsertMark(int index, BOOL after)
{
    CALL_STACK_MESSAGE3("CUserMenuBar::SetInsertMark(%d, %d)", index, after);
    if (InserMarkIndex == index && InserMarkAfter == after)
        return;
    if (ImageDragging)
        ImageDragShow(FALSE);
    CToolBar::SetInsertMark(index, after);
    if (ImageDragging)
        ImageDragShow(TRUE);
}

int CUserMenuBar::SetHotItem(int index)
{
    CALL_STACK_MESSAGE2("CUserMenuBar::SetHotItem(%d)", index);
    if (HotIndex == index)
        return index;

    if (ImageDragging)
        ImageDragShow(FALSE);
    int ret = CToolBar::SetHotItem(index);
    if (ImageDragging)
        ImageDragShow(TRUE);
    return ret;
}

void CUserMenuBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CUserMenuBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    if (tt->ID - CM_USERMENU_MIN < (DWORD)MainWindow->UserMenuItems->Count)
    {
        CUserMenuItem* item = MainWindow->UserMenuItems->At(tt->ID - CM_USERMENU_MIN);
        if (Configuration.UserMenuToolbarLabels)
        {
            char umCommand[MAX_PATH];
            if (ExpandCommand(MainWindow->HWindow, item->UMCommand, umCommand, MAX_PATH, TRUE))
                lstrcpy(tt->Buffer, umCommand);
        }
        else
            lstrcpy(tt->Buffer, item->ItemName);
    }
}

LRESULT
CUserMenuBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CUserMenuBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CUMDropTarget* dropTarget = new CUMDropTarget(this);
        if (dropTarget != NULL)
        {
            if (HANDLES(RegisterDragDrop(HWindow, dropTarget)) != S_OK)
            {
                TRACE_E("RegisterDragDrop error.");
            }
            dropTarget->Release(); // RegisterDragDrop called AddRef()
        }
        break;
    }

    case WM_DESTROY:
    {
        HANDLES(RevokeDragDrop(HWindow));
        break;
    }
    }
    return CToolBar::WindowProc(uMsg, wParam, lParam);
}
