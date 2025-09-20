// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "menu.h"
#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "viewer.h"
#include "shellib.h"
#include "find.h"
#include "gui.h"
#include "usermenu.h"
#include "execute.h"
#include "tasklist.h"

#include <Shlwapi.h>

const char* MINIMIZED_FINDING_CAPTION = "(%d) %s [%s %s]";
const char* NORMAL_FINDING_CAPTION = "%s [%s %s]";

BOOL FindManageInUse = FALSE;
BOOL FindIgnoreInUse = FALSE;

//****************************************************************************
//
// CFindTBHeader
//

void CFindOptions::InitMenu(CMenuPopup* popup, BOOL enabled, int originalCount)
{
    int count = popup->GetItemCount();
    if (count > originalCount)
    {
        // remove any previously inserted items
        popup->RemoveItemsRange(originalCount, count - 1);
    }

    if (Items.Count > 0)
    {
        MENU_ITEM_INFO mii;

        // if there are items to append, insert a separator first
        mii.Mask = MENU_MASK_TYPE;
        mii.Type = MENU_TYPE_SEPARATOR;
        popup->InsertItem(-1, TRUE, &mii);

        // append the displayed portion of the items
        int maxCount = CM_FIND_OPTIONS_LAST - CM_FIND_OPTIONS_FIRST;
        int i;
        for (i = 0; i < min(Items.Count, maxCount); i++)
        {
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_STRING | MENU_MASK_ID;
            mii.Type = MENU_TYPE_STRING;
            mii.State = enabled ? 0 : MENU_STATE_GRAYED;
            if (Items[i]->AutoLoad)
                mii.State |= MENU_STATE_DEFAULT;
            mii.ID = CM_FIND_OPTIONS_FIRST + i;
            mii.String = Items[i]->ItemName;
            popup->InsertItem(-1, TRUE, &mii);
        }
    }
}

//****************************************************************************
//
// CFoundFilesData
//

BOOL CFoundFilesData::Set(const char* path, const char* name, const CQuadWord& size, DWORD attr,
                          const FILETIME* lastWrite, BOOL isDir)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE5("CFoundFilesData::Set(%s, %s, %g, 0x%X, )", path, name, size.GetDouble(), attr);
    int l1 = (int)strlen(path), l2 = (int)strlen(name);
    Path = (char*)malloc(l1 + 1);
    Name = (char*)malloc(l2 + 1);
    if (Path == NULL || Name == NULL)
        return FALSE;
    memmove(Path, path, l1 + 1);
    memmove(Name, name, l2 + 1);
    Size = size;
    Attr = attr;
    LastWrite = *lastWrite;
    IsDir = isDir ? 1 : 0;
    return TRUE;
}

char* CFoundFilesData::GetText(int i, char* text, int fileNameFormat)
{
    // several FIND windows may run in parallel, which could overwrite this static buffer
    //  static char text[50];
    switch (i)
    {
    case 0:
    {
        AlterFileName(text, Name, -1, fileNameFormat, 0, IsDir);
        return text;
    }

    case 1:
        return Path;

    case 2:
    {
        if (IsDir)
            CopyMemory(text, DirColumnStr, DirColumnStrLen + 1);
        else
            NumberToStr(text, Size);
        break;
    }

    case 3:
    {
        SYSTEMTIME st;
        FILETIME ft;
        if (FileTimeToLocalFileTime(&LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
        {
            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, text, 50) == 0)
                sprintf(text, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        }
        else
            strcpy(text, LoadStr(IDS_INVALID_DATEORTIME));
        break;
    }

    case 4:
    {
        SYSTEMTIME st;
        FILETIME ft;
        if (FileTimeToLocalFileTime(&LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
        {
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 50) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        }
        else
            strcpy(text, LoadStr(IDS_INVALID_DATEORTIME));
        break;
    }

    default:
    {
        GetAttrsString(text, Attr);
        break;
    }
    }
    return text;
}

//****************************************************************************
//
// CFoundFilesListView
//

CFoundFilesListView::CFoundFilesListView(HWND dlg, int ctrlID, CFindDialog* findDialog)
    : Data(1000, 500), DataForRefine(1, 1000), CWindow(dlg, ctrlID)
{
    FindDialog = findDialog;
    HANDLES(InitializeCriticalSection(&DataCriticalSection));

    // add this panel to the array of sources for enumerating files in viewers
    EnumFileNamesAddSourceUID(HWindow, &EnumFileNamesSourceUID);
}

CFoundFilesListView::~CFoundFilesListView()
{
    // remove this panel from the array of sources for enumerating files in viewers
    EnumFileNamesRemoveSourceUID(HWindow);

    HANDLES(DeleteCriticalSection(&DataCriticalSection));
}

CFoundFilesData*
CFoundFilesListView::At(int index)
{
    CFoundFilesData* ptr;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    ptr = Data[index];
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
    return ptr;
}

void CFoundFilesListView::DestroyMembers()
{
    //  HANDLES(EnterCriticalSection(&DataCriticalSection));
    Data.DestroyMembers();
    //  HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

void CFoundFilesListView::Delete(int index)
{
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    Data.Delete(index);
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

int CFoundFilesListView::GetCount()
{
    int count;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    count = Data.Count;
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
    return count;
}

int CFoundFilesListView::Add(CFoundFilesData* item)
{
    int index;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    index = Data.Add(item);
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
    return index;
}

BOOL CFoundFilesListView::TakeDataForRefine()
{
    DataForRefine.DestroyMembers();
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CFoundFilesData* refineData = Data[i];
        DataForRefine.Add(refineData);
        if (!DataForRefine.IsGood())
        {
            DataForRefine.ResetState();
            DataForRefine.DetachMembers();
            return FALSE;
        }
    }
    Data.DetachMembers();
    return TRUE;
}

void CFoundFilesListView::DestroyDataForRefine()
{
    DataForRefine.DestroyMembers();
}

int CFoundFilesListView::GetDataForRefineCount()
{
    return DataForRefine.Count;
}

CFoundFilesData*
CFoundFilesListView::GetDataForRefine(int index)
{
    CFoundFilesData* ptr;
    ptr = DataForRefine[index];
    return ptr;
}

DWORD
CFoundFilesListView::GetSelectedListSize()
{
    // this method is invoked only from the main thread
    DWORD size = 0;
    int index = -1;
    do
    {
        index = ListView_GetNextItem(HWindow, index, LVIS_SELECTED);
        if (index != -1)
        {
            CFoundFilesData* ptr = Data[index];
            int pathLen = lstrlen(ptr->Path);
            if (ptr->Path[pathLen - 1] != '\\')
                pathLen++; // if the path does not contain a backslash, reserve space for it
            int nameLen = lstrlen(ptr->Name);
            size += pathLen + nameLen + 1; // reserve space for the terminator
        }
    } while (index != -1);
    if (size == 0)
        size = 2;
    else
        size++;

    return size;
}

BOOL CFoundFilesListView::GetSelectedList(char* list, DWORD maxSize)
{
    DWORD size = 0;
    int index = -1;
    do
    {
        index = ListView_GetNextItem(HWindow, index, LVIS_SELECTED);
        if (index != -1)
        {
            CFoundFilesData* ptr = Data[index];
            int pathLen = lstrlen(ptr->Path);
            if (ptr->Path[pathLen - 1] != '\\')
                size++; // if the path does not contain a backslash, reserve space for it
            size += pathLen;
            if (size > maxSize)
            {
                TRACE_E("Buffer is too short");
                return FALSE;
            }
            memmove(list, ptr->Path, pathLen);
            list += pathLen;
            if (ptr->Path[pathLen - 1] != '\\')
                *list++ = '\\';
            int nameLen = lstrlen(ptr->Name);
            size += nameLen + 1; // reserve space for the terminator
            if (size > maxSize)
            {
                TRACE_E("Buffer is too short");
                return FALSE;
            }
            memmove(list, ptr->Name, nameLen + 1);
            list += nameLen + 1;
        }
    } while (index != -1);
    if (size == 0)
    {
        if (size + 2 > maxSize)
        {
            TRACE_E("Buffer is too short");
            return FALSE;
        }
        *list++ = '\0';
        *list++ = '\0';
    }
    else
    {
        if (size + 1 > maxSize)
        {
            TRACE_E("Buffer is too short");
            return FALSE;
        }
        *list++ = '\0';
    }
    return TRUE;
}

void CFoundFilesListView::CheckAndRemoveSelectedItems(BOOL forceRemove, int lastFocusedIndex, const CFoundFilesData* lastFocusedItem)
{
    int removedItems = 0;

    int totalCount = ListView_GetItemCount(HWindow);
    int i;
    for (i = totalCount - 1; i >= 0; i--)
    {
        if (ListView_GetItemState(HWindow, i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            CFoundFilesData* ptr = Data[i];
            BOOL remove = forceRemove;
            if (!forceRemove)
            {
                char fullPath[MAX_PATH];
                int pathLen = lstrlen(ptr->Path);
                memmove(fullPath, ptr->Path, pathLen + 1);
                if (ptr->Path[pathLen - 1] != '\\')
                {
                    fullPath[pathLen] = '\\';
                    fullPath[pathLen + 1] = '\0';
                }
                lstrcat(fullPath, ptr->Name);
                remove = (SalGetFileAttributes(fullPath) == -1);
            }
            if (remove)
            {
                Delete(i);
                removedItems++;
            }
        }
    }
    if (removedItems > 0)
    {
        // inform the listview about the new item count
        totalCount = totalCount - removedItems;
        ListView_SetItemCount(HWindow, totalCount);
        if (totalCount > 0)
        {
            // clear selection of all items
            ListView_SetItemState(HWindow, -1, 0, LVIS_SELECTED);

            // try to locate the previously selected item and select it again if it still exists
            int selectIndex = -1;
            if (lastFocusedIndex != -1)
            {
                for (i = 0; i < totalCount; i++)
                {
                    CFoundFilesData* ptr = Data[i];
                    if (lastFocusedItem != NULL &&
                        lastFocusedItem->Name != NULL && strcmp(ptr->Name, lastFocusedItem->Name) == 0 &&
                        lastFocusedItem->Path != NULL && strcmp(ptr->Path, lastFocusedItem->Path) == 0)
                    {
                        selectIndex = i;
                        break;
                    }
                }
                if (selectIndex == -1)
                    selectIndex = min(lastFocusedIndex, totalCount - 1); // if we did not find it, keep the cursor in place but within item count
            }
            if (selectIndex == -1) // fallback -- first item
                selectIndex = 0;
            ListView_SetItemState(HWindow, selectIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(HWindow, selectIndex, FALSE);
        }
        else
            FindDialog->UpdateStatusBar = TRUE;
        FindDialog->UpdateListViewItems();
    }
}

BOOL CFoundFilesListView::IsGood()
{
    BOOL isGood;
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    isGood = Data.IsGood();
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
    return isGood;
}

void CFoundFilesListView::ResetState()
{
    HANDLES(EnterCriticalSection(&DataCriticalSection));
    Data.ResetState();
    HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

void CFoundFilesListView::StoreItemsState()
{
    int count = GetCount();
    int i;
    for (i = 0; i < count; i++)
    {
        DWORD state = ListView_GetItemState(HWindow, i, LVIS_FOCUSED | LVIS_SELECTED);
        Data[i]->Selected = (state & LVIS_SELECTED) != 0 ? 1 : 0;
        Data[i]->Focused = (state & LVIS_FOCUSED) != 0 ? 1 : 0;
    }
}

void CFoundFilesListView::RestoreItemsState()
{
    int count = GetCount();
    int i;
    for (i = 0; i < count; i++)
    {
        DWORD state = 0;
        if (Data[i]->Selected)
            state |= LVIS_SELECTED;
        if (Data[i]->Focused)
            state |= LVIS_FOCUSED;
        ListView_SetItemState(HWindow, i, state, LVIS_FOCUSED | LVIS_SELECTED);
    }
}

void CFoundFilesListView::SortItems(int sortBy)
{
    if (sortBy == 5)
        return; // sorting by attributes is unsupported

    BOOL enabledNameSize = TRUE;
    BOOL enabledPathTime = TRUE;
    if (FindDialog->GrepData.FindDuplicates)
    {
        enabledPathTime = FALSE; // path and time are irrelevant for duplicates
        // sorting by name and size works for duplicates only
        // when searching for identical name and size
        enabledNameSize = (FindDialog->GrepData.FindDupFlags & FIND_DUPLICATES_NAME) &&
                          (FindDialog->GrepData.FindDupFlags & FIND_DUPLICATES_SIZE);
    }

    if (!enabledNameSize && (sortBy == 0 || sortBy == 2))
        return;
    if (!enabledPathTime && (sortBy == 1 || sortBy == 3 || sortBy == 4))
        return;

    HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    HANDLES(EnterCriticalSection(&DataCriticalSection));

    //   EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);  // commented out, not sure why it is here: Petr

    // if some items are still in data but not in the listview, transfer them
    FindDialog->UpdateListViewItems();

    if (Data.Count > 0)
    {
        // save the selected and focused item state
        StoreItemsState();

        // sort the array by the requested criterion
        QuickSort(0, Data.Count - 1, sortBy);
        if (FindDialog->GrepData.FindDuplicates)
        {
            QuickSortDuplicates(0, Data.Count - 1, sortBy == 0);
            SetDifferentByGroup();
        }
        else
        {
            QuickSort(0, Data.Count - 1, sortBy);
        }

        // restore the item states
        RestoreItemsState();

        int focusIndex = ListView_GetNextItem(HWindow, -1, LVNI_FOCUSED);
        if (focusIndex != -1)
            ListView_EnsureVisible(HWindow, focusIndex, FALSE);
        ListView_RedrawItems(HWindow, 0, Data.Count - 1);
        UpdateWindow(HWindow);
    }

    HANDLES(LeaveCriticalSection(&DataCriticalSection));
    SetCursor(hCursor);
}

void CFoundFilesListView::SetDifferentByGroup()
{
    CFoundFilesData* lastData = NULL;
    int different = 0;
    if (Data.Count > 0)
    {
        lastData = Data.At(0);
        lastData->Different = different;
    }
    int i;
    for (i = 1; i < Data.Count; i++)
    {
        CFoundFilesData* data = Data.At(i);
        if (data->Group == lastData->Group)
        {
            data->Different = different;
        }
        else
        {
            different++;
            if (different > 1)
                different = 0;
            lastData = data;
            lastData->Different = different;
        }
    }
}

void CFoundFilesListView::QuickSort(int left, int right, int sortBy)
{

LABEL_QuickSort2:

    int i = left, j = right;
    CFoundFilesData* pivot = Data[(i + j) / 2];

    do
    {
        while (CompareFunc(Data[i], pivot, sortBy) < 0 && i < right)
            i++;
        while (CompareFunc(pivot, Data[j], sortBy) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CFoundFilesData* swap = Data[i];
            Data[i] = Data[j];
            Data[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // the following "nice" code was replaced with a code that is much more stack-efficient  (max. log(N) recursion depth)
    //  if (left < j) QuickSort(left, j, sortBy);
    //  if (i < right) QuickSort(i, right, sortBy);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // both halves need sorting: recurse on the smaller one and process the other via 'goto'
            {
                QuickSort(left, j, sortBy);
                left = i;
                goto LABEL_QuickSort2;
            }
            else
            {
                QuickSort(i, right, sortBy);
                right = j;
                goto LABEL_QuickSort2;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSort2;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSort2;
        }
    }
}

int CFoundFilesListView::CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, int sortBy)
{
    int res;
    int next = sortBy;
    do
    {
        if (f1->IsDir == f2->IsDir) // are the items from the same group (directories/files)?
        {
            switch (next)
            {
            case 0:
            {
                res = RegSetStrICmp(f1->Name, f2->Name);
                break;
            }

            case 1:
            {
                res = RegSetStrICmp(f1->Path, f2->Path);
                break;
                break;
            }

            case 2:
            {
                if (f1->Size < f2->Size)
                    res = -1;
                else
                {
                    if (f1->Size == f2->Size)
                        res = 0;
                    else
                        res = 1;
                }
                break;
            }

            default:
            {
                res = CompareFileTime(&f1->LastWrite, &f2->LastWrite);
                break;
            }
            }
        }
        else
            res = f1->IsDir ? -1 : 1;

        if (next == sortBy)
        {
            if (sortBy != 0)
                next = 0;
            else
                next = 1;
        }
        else if (next + 1 != sortBy)
            next++;
        else
            next += 2;
    } while (res == 0 && next <= 3);

    return res;
}

// quick sort routine for duplicate mode; it uses a special comparator
void CFoundFilesListView::QuickSortDuplicates(int left, int right, BOOL byName)
{

LABEL_QuickSortDuplicates:

    int i = left, j = right;
    CFoundFilesData* pivot = Data[(i + j) / 2];

    do
    {
        while (CompareDuplicatesFunc(Data[i], pivot, byName) < 0 && i < right)
            i++;
        while (CompareDuplicatesFunc(pivot, Data[j], byName) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CFoundFilesData* swap = Data[i];
            Data[i] = Data[j];
            Data[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // the following "nice" code was replaced with a code that is much more stack-efficient (max. log(N) recursion depth)
    //  if (left < j) QuickSortDuplicates(left, j, byName);
    //  if (i < right) QuickSortDuplicates(i, right, byName);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // both halves need sorting: recurse on the smaller one and use 'goto' for the other
            {
                QuickSortDuplicates(left, j, byName);
                left = i;
                goto LABEL_QuickSortDuplicates;
            }
            else
            {
                QuickSortDuplicates(i, right, byName);
                right = j;
                goto LABEL_QuickSortDuplicates;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSortDuplicates;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSortDuplicates;
        }
    }
}

// comparator for displayed duplicates; if 'byName', sorting is primarily by name, otherwise by size
int CFoundFilesListView::CompareDuplicatesFunc(CFoundFilesData* f1, CFoundFilesData* f2, BOOL byName)
{
    int res;
    if (byName)
    {
        // by name
        res = RegSetStrICmp(f1->Name, f2->Name);
        if (res == 0)
        {
            // by size
            if (f1->Size < f2->Size)
                res = -1;
            else
            {
                if (f1->Size == f2->Size)
                {
                    // by group
                    if (f1->Group < f2->Group)
                        res = -1;
                    else
                    {
                        if (f1->Group == f2->Group)
                            res = 0;
                        else
                            res = 1;
                    }
                }
                else
                    res = 1;
            }
        }
    }
    else
    {
        // by size
        if (f1->Size < f2->Size)
            res = -1;
        else
        {
            if (f1->Size == f2->Size)
            {
                // by name
                res = RegSetStrICmp(f1->Name, f2->Name);
                if (res == 0)
                {
                    // by group
                    if (f1->Group < f2->Group)
                        res = -1;
                    else
                    {
                        if (f1->Group == f2->Group)
                            res = 0;
                        else
                            res = 1;
                    }
                }
            }
            else
                res = 1;
        }
    }
    if (res == 0)
        res = RegSetStrICmp(f1->Path, f2->Path);
    return res;
}

struct CUMDataFromFind
{
    HWND HWindow;
    int* Index;
    int Count;

    CUMDataFromFind(HWND hWindow)
    {
        Count = -1;
        Index = NULL;
        HWindow = hWindow;
    }
    ~CUMDataFromFind()
    {
        if (Index != NULL)
            delete[] (Index);
    }
};

// description -- see mainwnd.h
BOOL GetNextItemFromFind(int index, char* path, char* name, void* param)
{
    CALL_STACK_MESSAGE2("GetNextItemFromFind(%d, , ,)", index);
    CUMDataFromFind* data = (CUMDataFromFind*)param;

    CFoundFilesListView* listView = (CFoundFilesListView*)WindowsManager.GetWindowPtr(data->HWindow);
    if (listView == NULL)
    {
        TRACE_E("Unable to find object for ListView");
        return FALSE;
    }

    LV_ITEM item;
    item.mask = LVIF_PARAM;
    item.iSubItem = 0;
    if (data->Count == -1)
    {
        data->Count = ListView_GetSelectedCount(data->HWindow);
        if (data->Count == 0)
            return FALSE;
        data->Index = new int[data->Count];
        if (data->Index == NULL)
            return FALSE; // error
        int i = 0;
        int findItem = -1;
        while (i < data->Count)
        {
            findItem = ListView_GetNextItem(data->HWindow, findItem, LVNI_SELECTED);
            data->Index[i++] = findItem;
        }
    }

    if (index >= 0 && index < data->Count)
    {
        CFoundFilesData* file = listView->At(data->Index[index]);
        strcpy(path, file->Path);
        strcpy(name, file->Name);
        return TRUE;
    }
    if (data->Index != NULL)
    {
        delete[] (data->Index);
        data->Index = NULL;
    }
    return FALSE;
}

LRESULT
CFoundFilesListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFoundFilesListView::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_GETDLGCODE:
    {
        if (lParam != NULL)
        {
            // if it is the Enter key, we want to process it, otherwise the Enter would not be delivered
            MSG* msg = (LPMSG)lParam;
            if (msg->message == WM_KEYDOWN && msg->wParam == VK_RETURN &&
                ListView_GetItemCount(HWindow) > 0)
                return DLGC_WANTMESSAGE;
        }
        return DLGC_WANTCHARS | DLGC_WANTARROWS;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        /* it seems this code is no longer needed; handled in the dialog wndproc
      if (wParam == VK_RETURN)
      {
        if (altPressed)
        {
          FindDialog->OnProperties();
          FindDialog->SkipCharacter = TRUE;
        }
        else
          FindDialog->OnOpen();
        return TRUE;
      }
*/
        if ((wParam == VK_F10 && shiftPressed || wParam == VK_APPS))
        {
            POINT p;
            GetListViewContextMenuPos(HWindow, &p);
            FindDialog->OnContextMenu(p.x, p.y);
            return TRUE;
        }
        break;
    }

    case WM_MOUSEACTIVATE:
    {
        // if Find is inactive and the user tries to drag & drop one of the items, the dialog must not pop up to the foreground
        return MA_NOACTIVATE;
    }

    case WM_SETFOCUS:
    {
        SendMessage(GetParent(HWindow), WM_USER_BUTTONS, 0, 0);
        break;
    }

    case WM_KILLFOCUS:
    {
        HWND next = (HWND)wParam;
        BOOL nextIsButton;
        if (next != NULL)
        {
            char className[30];
            WORD wl = LOWORD(GetWindowLongPtr(next, GWL_STYLE)); // only BS_ styles
            nextIsButton = (GetClassName(next, className, 30) != 0 &&
                            StrICmp(className, "BUTTON") == 0 &&
                            (wl == BS_PUSHBUTTON || wl == BS_DEFPUSHBUTTON));
        }
        else
            nextIsButton = FALSE;
        SendMessage(GetParent(HWindow), WM_USER_BUTTONS, nextIsButton ? wParam : 0, 0);
        break;
    }

    case WM_USER_ENUMFILENAMES: // searching for the next/previous name for the viewer
    {
        HANDLES(EnterCriticalSection(&FileNamesEnumDataSect));
        if ((int)wParam /* reqUID */ == FileNamesEnumData.RequestUID && // no further request was issued (this one would be pointless)
            EnumFileNamesSourceUID == FileNamesEnumData.SrcUID &&       // the source hasn't changed
            !FileNamesEnumData.TimedOut)                                // someone is still waiting for the result
        {
            HANDLES(EnterCriticalSection(&DataCriticalSection));

            BOOL selExists = FALSE;
            if (FileNamesEnumData.PreferSelected) // if needed, check whether there is a selection
            {
                int i = -1;
                int selCount = 0; // ignore the state where the only marked item is the focused one (this cannot logically be considered as selected items)
                while (1)
                {
                    i = ListView_GetNextItem(HWindow, i, LVNI_SELECTED);
                    if (i == -1)
                        break;
                    else
                    {
                        selCount++;
                        if (!Data[i]->IsDir)
                            selExists = TRUE;
                        if (selCount > 1 && selExists)
                            break;
                    }
                }
                if (selExists && selCount <= 1)
                    selExists = FALSE;
            }

            int index = FileNamesEnumData.LastFileIndex;
            int count = Data.Count;
            BOOL indexNotFound = TRUE;
            if (index == -1) // searching from the first or last item
            {
                if (FileNamesEnumData.RequestType == fnertFindPrevious)
                    index = count; // looking for the previous item, start at the end
                                   // else  // looking for the next item, start at the beginning
            }
            else
            {
                if (FileNamesEnumData.LastFileName[0] != 0) // the full name at 'index' is known; check for shifts and search for a new index if needed
                {
                    BOOL ok = FALSE;
                    CFoundFilesData* f = (index >= 0 && index < count) ? Data[index] : NULL;
                    char fileName[MAX_PATH];
                    if (f != NULL && f->Path != NULL && f->Name != NULL)
                    {
                        lstrcpyn(fileName, f->Path, MAX_PATH);
                        SalPathAppend(fileName, f->Name, MAX_PATH);
                        if (StrICmp(fileName, FileNamesEnumData.LastFileName) == 0)
                        {
                            ok = TRUE;
                            indexNotFound = FALSE;
                        }
                    }
                    if (!ok)
                    { // the name at index 'index' isn't FileNamesEnumData.LastFileName, try to find a new index for that name
                        int i;
                        for (i = 0; i < count; i++)
                        {
                            f = Data[i];
                            if (f->Path != NULL && f->Name != NULL)
                            {
                                lstrcpyn(fileName, f->Path, MAX_PATH);
                                SalPathAppend(fileName, f->Name, MAX_PATH);
                                if (StrICmp(fileName, FileNamesEnumData.LastFileName) == 0)
                                    break;
                            }
                        }
                        if (i != count) // new index found
                        {
                            index = i;
                            indexNotFound = FALSE;
                        }
                    }
                }
                if (index >= count)
                {
                    if (FileNamesEnumData.RequestType == fnertFindNext)
                        index = count - 1;
                    else
                        index = count;
                }
                if (index < 0)
                    index = 0;
            }

            int wantedViewerType = 0;
            BOOL onlyAssociatedExtensions = FALSE;
            if (FileNamesEnumData.OnlyAssociatedExtensions) // does the viewer request filtering by associated extensions?
            {
                if (FileNamesEnumData.Plugin != NULL) // viewer from a plugin
                {
                    int pluginIndex = Plugins.GetIndex(FileNamesEnumData.Plugin);
                    if (pluginIndex != -1) // "always true"
                    {
                        wantedViewerType = -1 - pluginIndex;
                        onlyAssociatedExtensions = TRUE;
                    }
                }
                else // internal viewer
                {
                    wantedViewerType = VIEWER_INTERNAL;
                    onlyAssociatedExtensions = TRUE;
                }
            }

            BOOL preferSelected = selExists && FileNamesEnumData.PreferSelected;
            switch (FileNamesEnumData.RequestType)
            {
            case fnertFindNext: // next
            {
                CDynString strViewerMasks;
                if (MainWindow->GetViewersAssoc(wantedViewerType, &strViewerMasks))
                {
                    CMaskGroup masks;
                    int errorPos;
                    if (masks.PrepareMasks(errorPos, strViewerMasks.GetString()))
                    {
                        while (index + 1 < count)
                        {
                            index++;
                            if (preferSelected)
                            {
                                int i = ListView_GetNextItem(HWindow, index - 1, LVNI_SELECTED);
                                if (i != -1)
                                {
                                    index = i;
                                    if (!Data[index]->IsDir) // we only search for files
                                    {
                                        if (!onlyAssociatedExtensions || masks.AgreeMasks(Data[index]->Name, NULL))
                                        {
                                            FileNamesEnumData.Found = TRUE;
                                            break;
                                        }
                                    }
                                }
                                else
                                    index = count - 1;
                            }
                            else
                            {
                                if (!Data[index]->IsDir)
                                {
                                    if (!onlyAssociatedExtensions || masks.AgreeMasks(Data[index]->Name, NULL))
                                    {
                                        FileNamesEnumData.Found = TRUE;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else
                        TRACE_E("Unexpected situation in Find::WM_USER_ENUMFILENAMES: grouped viewer's masks can't be prepared for use!");
                }
                break;
            }

            case fnertFindPrevious: // previous
            {
                CDynString strViewerMasks;
                if (MainWindow->GetViewersAssoc(wantedViewerType, &strViewerMasks))
                {
                    CMaskGroup masks;
                    int errorPos;
                    if (masks.PrepareMasks(errorPos, strViewerMasks.GetString()))
                    {
                        while (index - 1 >= 0)
                        {
                            index--;
                            if (!Data[index]->IsDir &&
                                (!preferSelected ||
                                 (ListView_GetItemState(HWindow, index, LVIS_SELECTED) & LVIS_SELECTED)))
                            {
                                if (!onlyAssociatedExtensions || masks.AgreeMasks(Data[index]->Name, NULL))
                                {
                                    FileNamesEnumData.Found = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    else
                        TRACE_E("Unexpected situation in Find::WM_USER_ENUMFILENAMES: grouped viewer's masks can't be prepared for use!");
                }
                break;
            }

            case fnertIsSelected: // check selection state
            {
                if (!indexNotFound && index >= 0 && index < Data.Count)
                {
                    FileNamesEnumData.IsFileSelected = (ListView_GetItemState(HWindow, index, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                    FileNamesEnumData.Found = TRUE;
                }
                break;
            }

            case fnertSetSelection: // set selection
            {
                if (!indexNotFound && index >= 0 && index < Data.Count)
                {
                    ListView_SetItemState(HWindow, index, FileNamesEnumData.Select ? LVIS_SELECTED : 0, LVIS_SELECTED);
                    FileNamesEnumData.Found = TRUE;
                }
                break;
            }
            }

            if (FileNamesEnumData.Found)
            {
                CFoundFilesData* f = Data[index];
                if (f->Path != NULL && f->Name != NULL)
                {
                    lstrcpyn(FileNamesEnumData.FileName, f->Path, MAX_PATH);
                    SalPathAppend(FileNamesEnumData.FileName, f->Name, MAX_PATH);
                    FileNamesEnumData.LastFileIndex = index;
                }
                else // should never happen
                {
                    TRACE_E("Unexpected situation in CFoundFilesListView::WindowProc(): handling of WM_USER_ENUMFILENAMES");
                    FileNamesEnumData.Found = FALSE;
                    FileNamesEnumData.NoMoreFiles = TRUE;
                }
            }
            else
                FileNamesEnumData.NoMoreFiles = TRUE;

            HANDLES(LeaveCriticalSection(&DataCriticalSection));
            SetEvent(FileNamesEnumDone);
        }
        HANDLES(LeaveCriticalSection(&FileNamesEnumDataSect));
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

BOOL CFoundFilesListView::InitColumns()
{
    CALL_STACK_MESSAGE1("CFoundFilesListView::InitColumns()");
    LV_COLUMN lvc;
    int header[] = {IDS_FOUNDFILESCOLUMN1, IDS_FOUNDFILESCOLUMN2,
                    IDS_FOUNDFILESCOLUMN3, IDS_FOUNDFILESCOLUMN4,
                    IDS_FOUNDFILESCOLUMN5, IDS_FOUNDFILESCOLUMN6,
                    -1};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; header[i] != -1; i++) // create columns
    {
        if (i == 2)
            lvc.fmt = LVCFMT_RIGHT;
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        if (ListView_InsertColumn(HWindow, i, &lvc) == -1)
            return FALSE;
    }

    RECT r;
    GetClientRect(HWindow, &r);
    DWORD cx = r.right - r.left - 1;
    ListView_SetColumnWidth(HWindow, 5, ListView_GetStringWidth(HWindow, "ARH") + 20);

    char format1[200];
    char format2[200];
    SYSTEMTIME st;
    ZeroMemory(&st, sizeof(st));
    st.wYear = 2000; // the longest possible value
    st.wMonth = 12;  // the longest possible value
    st.wDay = 30;    // the longest possible value
    st.wHour = 10;   // morning (not sure whether AM or PM will be shorter, so try both)
    st.wMinute = 59; // the longest possible value
    st.wSecond = 59; // the longest possible value
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, format1, 200) == 0)
        sprintf(format1, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    st.wHour = 20; // afternoon
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, format2, 200) == 0)
        sprintf(format2, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);

    int maxWidth = ListView_GetStringWidth(HWindow, format1);
    int w = ListView_GetStringWidth(HWindow, format2);
    if (w > maxWidth)
        maxWidth = w;
    ListView_SetColumnWidth(HWindow, 4, maxWidth + 20);

    maxWidth = 0;
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, format1, 200) == 0)
        sprintf(format1, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    else
    {
        // verify that the short date format does not contain alphabetic characters
        const char* p = format1;
        while (*p != 0 && !IsAlpha[*p])
            p++;
        if (IsAlpha[*p])
        {
            // contains alphabetic characters -- we must find the longest month and day text
            int maxMonth = 0;
            int sats[] = {1, 5, 4, 1, 6, 3, 1, 5, 2, 7, 4, 2};
            int mo;
            for (mo = 0; mo < 12; mo++) // iterate over all months starting from January; the weekday stays the same so its width doesn't influence the result, wDay is single digit for the same reason
            {
                st.wDay = sats[mo];
                st.wMonth = 1 + mo;
                if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, format1, 200) != 0)
                {
                    w = ListView_GetStringWidth(HWindow, format1);
                    if (w > maxWidth)
                    {
                        maxWidth = w;
                        maxMonth = st.wMonth;
                    }
                }
            }
            if (maxWidth > 0)
            {
                st.wMonth = maxMonth;
                for (st.wDay = 21; st.wDay < 28; st.wDay++) // all possible weekdays (doesn't have to start on Monday)
                {
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, format1, 200) != 0)
                    {
                        w = ListView_GetStringWidth(HWindow, format1);
                        if (w > maxWidth)
                        {
                            maxWidth = w;
                        }
                    }
                }
            }
        }
    }

    ListView_SetColumnWidth(HWindow, 3, (maxWidth > 0 ? maxWidth : ListView_GetStringWidth(HWindow, format1)) + 20);
    ListView_SetColumnWidth(HWindow, 2, ListView_GetStringWidth(HWindow, "000 000 000 000") + 20); // up to 1TB fits here
    int width;
    if (Configuration.FindColNameWidth != -1)
        width = Configuration.FindColNameWidth;
    else
        width = 20 + ListView_GetStringWidth(HWindow, "XXXXXXXX.XXX") + 20;
    ListView_SetColumnWidth(HWindow, 0, width);
    cx -= ListView_GetColumnWidth(HWindow, 0) + ListView_GetColumnWidth(HWindow, 2) +
          ListView_GetColumnWidth(HWindow, 3) + ListView_GetColumnWidth(HWindow, 4) +
          ListView_GetColumnWidth(HWindow, 5) + GetSystemMetrics(SM_CXHSCROLL) - 1;
    ListView_SetColumnWidth(HWindow, 1, cx);
    ListView_SetImageList(HWindow, HFindSymbolsImageList, LVSIL_SMALL);

    return TRUE;
}

//****************************************************************************
//
// CFindDialog
//

CFindDialog::CFindDialog(HWND hCenterAgainst, const char* initPath)
    : CCommonDialog(HLanguage, IDD_FIND, NULL, ooStandard, hCenterAgainst),
      SearchForData(50, 10)
{
    // data needed to lay out the dialog
    FirstWMSize = TRUE;
    VMargin = 0;
    HMargin = 0;
    ButtonW = 0;
    ButtonH = 0;
    RegExpButtonW = 0;
    RegExpButtonY = 0;
    MenuBarHeight = 0;
    StatusHeight = 0;
    ResultsY = 0;
    AdvancedY = 0;
    AdvancedTextY = 0;
    AdvancedTextX = 0;
    FindTextY = 0;
    FindTextH = 0;
    CombosX = 0;
    CombosH = 0;
    BrowseY = 0;
    Line2X = 0;
    FindNowY = 0;
    Expanded = TRUE; // persistent
    MinDlgW = 0;
    MinDlgH = 0;

    // additional data
    DlgFailed = FALSE;
    MainMenu = NULL;
    TBHeader = NULL;
    MenuBar = NULL;
    HStatusBar = NULL;
    HProgressBar = NULL;
    TwoParts = FALSE;
    FoundFilesListView = NULL;
    SearchInProgress = FALSE;
    StateOfFindCloseQuery = sofcqNotUsed;
    CanClose = TRUE;
    GrepThread = NULL;
    char buf[100];
    sprintf(buf, "%s ", LoadStr(IDS_FF_SEARCHING));
    SearchingText.SetBase(buf);
    UpdateStatusBar = FALSE;
    ContextMenu = NULL;
    ZeroOnDestroy = NULL;
    OleInitialized = FALSE;
    ProcessingEscape = FALSE;
    EditLine = new CComboboxEdit();
    OKButton = NULL;

    FileNameFormat = Configuration.FileNameFormat;
    SkipCharacter = FALSE;

    CacheBitmap = NULL;
    FlashIconsOnActivation = FALSE;

    FindNowText[0] = 0;

    // if any option has AutoLoad set, load it now
    int i;
    for (i = 0; i < FindOptions.GetCount(); i++)
        if (FindOptions.At(i)->AutoLoad)
        {
            Data = *FindOptions.At(i);
            Data.AutoLoad = FALSE;
            break;
        }

    // data for controls
    if (Data.NamedText[0] == 0)
        lstrcpy(Data.NamedText, "*.*");
    if (Data.LookInText[0] == 0)
    {
        const char* s = initPath;
        char* d = Data.LookInText;
        char* end = Data.LookInText + LOOKIN_TEXT_LEN - 1; // -1 leaves room for the null terminator at the end of the string
        while (*s != 0 && d < end)
        {
            if (*s == ';')
            {
                *d++ = ';';
                if (d >= end)
                    break;
            }
            *d++ = *s++;
        }
        *d++ = 0;
    }
}

CFindDialog::~CFindDialog()
{
    if (CacheBitmap != NULL)
        delete CacheBitmap;
}

void CFindDialog::GetLayoutParams()
{
    RECT wr;
    GetWindowRect(HWindow, &wr);
    MinDlgW = wr.right - wr.left;
    MinDlgH = wr.bottom - wr.top;

    RECT cr;
    GetClientRect(HWindow, &cr);

    RECT br;
    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_RESULTS), &br);
    int windowMargin = ((wr.right - wr.left) - (cr.right)) / 2;
    HMargin = br.left - wr.left - windowMargin;
    VMargin = HMargin;

    int captionH = wr.bottom - wr.top - cr.bottom - windowMargin;
    ResultsY = br.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDOK), &br); //IDC_FIND_FINDNOW
    ButtonW = br.right - br.left;
    ButtonH = br.bottom - br.top;
    FindNowY = br.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_REGEXP_BROWSE), &br);
    RegExpButtonW = br.right - br.left;
    RegExpButtonY = br.top - wr.top - captionH;

    MenuBarHeight = MenuBar->GetNeededHeight();

    RECT r;
    GetWindowRect(HStatusBar, &r);
    StatusHeight = r.bottom - r.top;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_NAMED), &r);
    CombosX = r.left - wr.left - windowMargin;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_LOOKIN_BROWSE), &r);
    BrowseY = r.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_LINE2), &r);
    Line2X = r.left - wr.left - windowMargin;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_SPACER), &r);
    SpacerH = r.bottom - r.top;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_ADVANCED), &r);
    AdvancedY = r.top - wr.top - captionH;

    GetWindowRect(GetDlgItem(HWindow, IDC_FIND_ADVANCED_TEXT), &r);
    AdvancedTextY = r.top - wr.top - captionH;
    AdvancedTextX = r.left - wr.left - windowMargin;

    //  GetWindowRect(GetDlgItem(HWindow, IDC_FIND_FOUND_FILES), &r);
    FindTextH = TBHeader->GetNeededHeight();
    FindTextY = ResultsY - FindTextH;
}

void CFindDialog::SetTwoStatusParts(BOOL two, BOOL force)
{
    int margin = HMargin - 4;
    int parts[3] = {margin, -1, -1};
    RECT r;
    GetClientRect(HStatusBar, &r);

    int gripWidth = HMargin;
    if (!IsZoomed(HWindow))
        gripWidth = GetSystemMetrics(SM_CXVSCROLL);

    int progressWidth = 0;
    int progressHeight = 0;
    if (two)
    {
        progressWidth = 104; // 100 plus the frame
        if (HProgressBar == NULL)
        {
            HProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL,
                                          WS_CHILD | PBS_SMOOTH,
                                          0, 0,
                                          progressWidth, r.bottom - 2,
                                          HStatusBar, (HMENU)0, HInstance, NULL);
            SendMessage(HProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        }
    }
    else
    {
        if (HProgressBar != NULL)
        {
            DestroyWindow(HProgressBar);
            HProgressBar = NULL;
        }
    }

    parts[1] = r.right - progressWidth - gripWidth;

    if (HProgressBar != NULL)
    {
        parts[1] -= 10; // increase spacing from the progress bar
        SetWindowPos(HProgressBar, NULL,
                     r.right - progressWidth - gripWidth, 2, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    if (TwoParts != two || force)
    {
        TwoParts = two;
        SendMessage(HStatusBar, WM_SETREDRAW, FALSE, 0); // when redraw is enabled, the status bar ends up with a stray frame
        SendMessage(HStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
        SendMessage(HStatusBar, SB_SETTEXT, 0 | SBT_NOBORDERS, (LPARAM) "");
        SendMessage(HStatusBar, SB_SETTEXT, 2 | SBT_NOBORDERS, (LPARAM) "");
        SendMessage(HStatusBar, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(HStatusBar, NULL, TRUE);
    }
    else
        SendMessage(HStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
}

void CFindDialog::LayoutControls()
{
    RECT clientRect;

    if (CombosH == 0)
    {
        RECT r;
        GetWindowRect(GetDlgItem(HWindow, IDC_FIND_NAMED), &r);
        if (r.bottom - r.top != 0)
            CombosH = r.bottom - r.top;
    }

    GetClientRect(HWindow, &clientRect);
    clientRect.bottom -= StatusHeight;

    HDWP hdwp = HANDLES(BeginDeferWindowPos(14));
    if (hdwp != NULL)
    {
        // spacing between buttons
        int buttonMargin = ButtonH / 3;

        // position the MenuBar
        hdwp = HANDLES(DeferWindowPos(hdwp, MenuBar->HWindow, NULL,
                                      0, -1, clientRect.right, MenuBarHeight,
                                      SWP_NOZORDER));

        // position the Status Bar
        hdwp = HANDLES(DeferWindowPos(hdwp, HStatusBar, NULL,
                                      0, clientRect.bottom, clientRect.right, StatusHeight,
                                      SWP_NOZORDER));

        // position the Advanced button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_ADVANCED), NULL,
                                      HMargin, AdvancedY, 0, 0, SWP_NOSIZE | SWP_NOZORDER));

        // place and stretch the Advanced edit line
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_ADVANCED_TEXT), NULL,
                                      AdvancedTextX, AdvancedTextY, clientRect.right - AdvancedTextX - HMargin, CombosH,
                                      SWP_NOZORDER));

        // position the "Found files" label
        hdwp = HANDLES(DeferWindowPos(hdwp, TBHeader->HWindow /*GetDlgItem(HWindow, IDC_FIND_FOUND_FILES)*/, NULL,
                                      HMargin, FindTextY, clientRect.right - 2 * HMargin, FindTextH, SWP_NOZORDER));

        // place and stretch the list view
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_RESULTS), NULL,
                                      HMargin, ResultsY, clientRect.right - 2 * HMargin,
                                      clientRect.bottom - ResultsY /*- VMargin*/, SWP_NOZORDER));

        // place and stretch the separator line under the menu
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_LINE1), NULL,
                                      0, MenuBarHeight - 1, clientRect.right, 2, SWP_NOZORDER));

        // stretch the "Named" combo box
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_NAMED), NULL,
                                      0, 0, clientRect.right - CombosX - HMargin - ButtonW - buttonMargin, CombosH,
                                      SWP_NOMOVE | SWP_NOZORDER));

        // position the "Find Now" button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDOK), NULL, //IDC_FIND_FINDNOW
                                      clientRect.right - HMargin - ButtonW, FindNowY, 0, 0,
                                      SWP_NOSIZE | SWP_NOZORDER));

        // stretch the "Look in" combo box
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_LOOKIN), NULL,
                                      0, 0, clientRect.right - CombosX - HMargin - ButtonW - buttonMargin, CombosH,
                                      SWP_NOMOVE | SWP_NOZORDER));

        // position the Browse button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_LOOKIN_BROWSE), NULL,
                                      clientRect.right - HMargin - ButtonW, BrowseY, 0, 0,
                                      SWP_NOSIZE | SWP_NOZORDER));

        // stretch the "Containing" combo box
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_CONTAINING), NULL,
                                      0, 0, clientRect.right - CombosX - HMargin - RegExpButtonW - buttonMargin, CombosH,
                                      SWP_NOMOVE | SWP_NOZORDER));

        // position the Regular Expression Browse button
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_REGEXP_BROWSE), NULL,
                                      clientRect.right - HMargin - RegExpButtonW, RegExpButtonY, 0, 0,
                                      SWP_NOSIZE | SWP_NOZORDER));

        // stretch the separator line next to "Search file content"
        hdwp = HANDLES(DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_FIND_LINE2), NULL,
                                      0, 0, clientRect.right - Line2X - HMargin, 2,
                                      SWP_NOMOVE | SWP_NOZORDER));

        HANDLES(EndDeferWindowPos(hdwp));
    }
    SetTwoStatusParts(TwoParts);
}

void CFindDialog::SetContentVisible(BOOL visible)
{
    if (Expanded != visible)
    {
        if (visible)
        {
            ResultsY += SpacerH;
            AdvancedY += SpacerH;
            AdvancedTextY += SpacerH;
            FindTextY += SpacerH;

            MinDlgH += SpacerH;
            LayoutControls();
            if (!IsIconic(HWindow) && !IsZoomed(HWindow))
            {
                RECT wr;
                GetWindowRect(HWindow, &wr);
                if (wr.bottom - wr.top < MinDlgH)
                {
                    SetWindowPos(HWindow, NULL, 0, 0, wr.right - wr.left, MinDlgH,
                                 SWP_NOMOVE | SWP_NOZORDER);
                }
            }
        }

        // when items appear, we must enable them
        Expanded = visible;
        if (visible)
        {
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING_TEXT), TRUE);
            EnableControls();
        }

        ShowWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING_TEXT), visible);
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING), visible);
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_REGEXP_BROWSE), visible);
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_HEX), visible);
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_CASE), visible);
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_WHOLE), visible);
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_REGULAR), visible);

        if (!visible)
        {
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING_TEXT), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_REGEXP_BROWSE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_HEX), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CASE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_WHOLE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_REGULAR), FALSE);
        }

        if (!visible)
        {
            ResultsY -= SpacerH;
            AdvancedY -= SpacerH;
            AdvancedTextY -= SpacerH;
            FindTextY -= SpacerH;
            MinDlgH -= SpacerH;
            LayoutControls();
        }
    }
}

void CFindDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFindDialog::Validate()");

    HWND hNamesWnd;
    HWND hLookInWnd;

    if (ti.GetControl(hNamesWnd, IDC_FIND_NAMED) &&
        ti.GetControl(hLookInWnd, IDC_FIND_LOOKIN))
    {
        // back up the data
        char bufNamed[NAMED_TEXT_LEN];
        char bufLookIn[LOOKIN_TEXT_LEN];
        strcpy(bufNamed, Data.NamedText);
        strcpy(bufLookIn, Data.LookInText);

        SendMessage(hNamesWnd, WM_GETTEXT, NAMED_TEXT_LEN, (LPARAM)Data.NamedText);
        CMaskGroup mask(Data.NamedText);
        int errorPos;
        if (!mask.PrepareMasks(errorPos))
        {
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(hNamesWnd); // ensure the CB_SETEDITSEL message works correctly
            SendMessage(hNamesWnd, CB_SETEDITSEL, 0, MAKELPARAM(errorPos, errorPos + 1));
            ti.ErrorOn(IDC_FIND_NAMED);
        }

        if (ti.IsGood())
        {
            SendMessage(hLookInWnd, WM_GETTEXT, LOOKIN_TEXT_LEN, (LPARAM)Data.LookInText);

            BuildSerchForData();
            if (SearchForData.Count == 0)
            {
                SalMessageBox(HWindow, LoadStr(IDS_FF_EMPTYSTRING), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDC_FIND_LOOKIN);
            }
        }

        // restore data from the backup
        strcpy(Data.LookInText, bufLookIn);
        strcpy(Data.NamedText, bufNamed);
    }
}

void CFindDialog::Transfer(CTransferInfo& ti)
{
    HistoryComboBox(HWindow, ti, IDC_FIND_NAMED, Data.NamedText, NAMED_TEXT_LEN,
                    FALSE, FIND_NAMED_HISTORY_SIZE, FindNamedHistory);
    HistoryComboBox(HWindow, ti, IDC_FIND_LOOKIN, Data.LookInText, LOOKIN_TEXT_LEN,
                    FALSE, FIND_LOOKIN_HISTORY_SIZE, FindLookInHistory);

    ti.CheckBox(IDC_FIND_INCLUDE_SUBDIR, Data.SubDirectories);
    HistoryComboBox(HWindow, ti, IDC_FIND_CONTAINING, Data.GrepText, GREP_TEXT_LEN,
                    !Data.RegularExpresions && Data.HexMode, FIND_GREP_HISTORY_SIZE,
                    FindGrepHistory);
    ti.CheckBox(IDC_FIND_HEX, Data.HexMode);
    ti.CheckBox(IDC_FIND_CASE, Data.CaseSensitive);
    ti.CheckBox(IDC_FIND_WHOLE, Data.WholeWords);
    ti.CheckBox(IDC_FIND_REGULAR, Data.RegularExpresions);
}

void CFindDialog::UpdateAdvancedText()
{
    char buff[200];
    BOOL dirty;
    Data.Criteria.GetAdvancedDescription(buff, 200, dirty);
    SetDlgItemText(HWindow, IDC_FIND_ADVANCED_TEXT, buff);
    EnableWindow(GetDlgItem(HWindow, IDC_FIND_ADVANCED_TEXT), dirty);
}

void CFindDialog::LoadControls(int index)
{
    CALL_STACK_MESSAGE2("CFindDialog::LoadControls(0x%X)", index);
    Data = *FindOptions.At(index);

    // if any edit line is empty, keep its previous value
    if (Data.NamedText[0] == 0)
        GetDlgItemText(HWindow, IDC_FIND_NAMED, Data.NamedText, NAMED_TEXT_LEN);
    if (Data.LookInText[0] == 0)
        GetDlgItemText(HWindow, IDC_FIND_LOOKIN, Data.LookInText, LOOKIN_TEXT_LEN);
    if (Data.GrepText[0] == 0)
        GetDlgItemText(HWindow, IDC_FIND_CONTAINING, Data.GrepText, GREP_TEXT_LEN);

    TransferData(ttDataToWindow);

    // if Grep contains text and the dialog isn't expanded, expand it
    if (Data.GrepText[0] != 0 && !Expanded)
    {
        CheckDlgButton(HWindow, IDC_FIND_GREP, TRUE);
        SetContentVisible(TRUE);
    }

    UpdateAdvancedText();
    EnableControls();
}

void CFindDialog::BuildSerchForData()
{
    char named[NAMED_TEXT_LEN + NAMED_TEXT_LEN];
    char* begin;
    char* end;

    // Users often want to enter just "i_am_dummy" to find files "*i_am_dummy*".
    // Therefore, we must inspect each item from the mask group and, if it lacks
    // any wildcard or '.', surround it with asterisks.
    char* iterator = named;
    begin = Data.NamedText;
    while (1)
    {
        end = begin;
        while (*end != 0)
        {
            if (*end == '|')
                break;
            if (*end == ';')
            {
                if (*(end + 1) != ';')
                    break;
                else
                    end++;
            }
            end++;
        }
        while (*begin != 0 && *begin <= ' ')
            begin++; // skip spaces at the beginning
        char* tmpEnd = end;
        while (tmpEnd > begin && *(tmpEnd - 1) <= ' ')
            tmpEnd--; // skip spaces at the end
        if (tmpEnd > begin)
        {
            // check whether the substring contains a wildcard '*', '?', or '.'
            BOOL wildcard = FALSE;
            char* tmp = begin;
            while (tmp < tmpEnd)
            {
                if (*tmp == '*' || *tmp == '?' || *tmp == '.')
                {
                    wildcard = TRUE;
                    break;
                }
                tmp++;
            }

            if (!wildcard)
                *iterator++ = '*'; // no wildcard - prepend an asterisk

            memcpy(iterator, begin, tmpEnd - begin);
            iterator += tmpEnd - begin;

            if (!wildcard)
                *iterator++ = '*'; // no wildcard - append an asterisk
        }
        *iterator++ = *end;
        if (*end != 0)
            begin = end + 1;
        else
            break;
    }

    if (named[0] == 0)
        strcpy(named, "*"); // replace empty string with '*'

    SearchForData.DestroyMembers();

    char path[MAX_PATH];
    lstrcpy(path, Data.LookInText);
    begin = path;
    do
    {
        end = begin;
        while (*end != 0)
        {
            if (*end == ';')
            {
                if (*(end + 1) != ';')
                    break;
                else
                    memmove(end, end + 1, strlen(end + 1) + 1); // shift left (";;" -> ";")
            }
            end++;
        }
        char* tmp = end - 1;
        if (*end == ';')
        {
            *end = 0;
            end++;
        }
        // while (*end == ';') end++;   // always false because ";;" -> ";" and it's a regular character, not a separator

        // remove spaces before the path
        while (*begin == ' ')
            begin++;
        // remove spaces after the path
        if (tmp > begin)
        {
            while (tmp > begin && *tmp <= ' ')
                tmp--;
            *(tmp + 1) = 0; // there might already be '\0'; otherwise add it
        }
        // remove redundant slashes/backslashes at the end of the path (keep at most one)
        if (tmp > begin)
        {
            while (tmp > begin && (*tmp == '/' || *tmp == '\\'))
                tmp--;
            if (*(tmp + 1) == '/' || *(tmp + 1) == '\\')
                tmp++;      // leave one
            *(tmp + 1) = 0; // there might already be '\0'; otherwise add it
        }

        if (*begin != 0)
        {
            CSearchForData* item = new CSearchForData(begin, named, Data.SubDirectories);
            if (item != NULL)
            {
                SearchForData.Add(item);
                if (!SearchForData.IsGood())
                {
                    SearchForData.ResetState();
                    delete item;
                    return;
                }
            }
        }
        if (*end != 0)
            begin = end;
    } while (*end != 0);
}

void CFindDialog::StartSearch(WORD command)
{
    CALL_STACK_MESSAGE1("CFindDialog::StartSearch()");
    if (FoundFilesListView == NULL || GrepThread != NULL)
        return;

    // if we are searching for duplicates, ask for additional options
    CFindDuplicatesDialog findDupDlg(HWindow);
    if (command == CM_FIND_DUPLICATES)
    {
        if (findDupDlg.Execute() != IDOK)
            return;

        // better verify the output variables
        if (!findDupDlg.SameName && !findDupDlg.SameSize)
        {
            TRACE_E("Invalid output from CFindDuplicatesDialog dialog.");
            return;
        }
    }

    TBHeader->SetFoundCount(0);
    TBHeader->SetErrorsInfosCount(0, 0);

    EnumFileNamesChangeSourceUID(FoundFilesListView->HWindow, &(FoundFilesListView->EnumFileNamesSourceUID));

    ListView_SetItemCount(FoundFilesListView->HWindow, 0);
    UpdateWindow(FoundFilesListView->HWindow);

    // release any errors held from the previous search
    Log.Clean();

    GrepData.FindDuplicates = FALSE;
    GrepData.FindDupFlags = 0;

    GrepData.Refine = 0; // no refine

    switch (command)
    {
    case IDOK:
    case CM_FIND_NOW:
    {
        FoundFilesListView->DestroyMembers();
        break;
    }

    case CM_FIND_INTERSECT:
    {
        // if this is a refine operation, copy data into the DataForRefine array
        FoundFilesListView->TakeDataForRefine();
        GrepData.Refine = 1;
        break;
    }

    case CM_FIND_SUBTRACT:
    {
        // if this is a refine operation, copy data into the DataForRefine array
        FoundFilesListView->TakeDataForRefine();
        GrepData.Refine = 2;
        break;
    }

    case CM_FIND_APPEND:
    {
        break;
    }

    case CM_FIND_DUPLICATES:
    {
        GrepData.FindDuplicates = TRUE;
        GrepData.FindDupFlags = 0;
        if (findDupDlg.SameName)
            GrepData.FindDupFlags |= FIND_DUPLICATES_NAME;
        if (findDupDlg.SameSize)
            GrepData.FindDupFlags |= FIND_DUPLICATES_SIZE;
        if (findDupDlg.SameContent)
            GrepData.FindDupFlags |= FIND_DUPLICATES_SIZE | FIND_DUPLICATES_CONTENT;

        FoundFilesListView->DestroyMembers();
        break;
    }
    }
    UpdateListViewItems();

    if (Data.GrepText[0] == 0)
        GrepData.Grep = FALSE;
    else
    {
        GrepData.EOL_CRLF = Configuration.EOL_CRLF;
        GrepData.EOL_CR = Configuration.EOL_CR;
        GrepData.EOL_LF = Configuration.EOL_LF;
        //    GrepData.EOL_NULL = Configuration.EOL_NULL;   // can't handle this with regexp :(
        GrepData.Regular = Data.RegularExpresions;
        GrepData.WholeWords = Data.WholeWords;
        if (Data.RegularExpresions)
        {
            if (!GrepData.RegExp.Set(Data.GrepText, (WORD)(sfForward |
                                                           (Data.CaseSensitive ? sfCaseSensitive : 0))))
            {
                char buf[500];
                if (GrepData.RegExp.GetPattern() != NULL)
                    sprintf(buf, LoadStr(IDS_INVALIDREGEXP), GrepData.RegExp.GetPattern(), GrepData.RegExp.GetLastErrorText());
                else
                    strcpy(buf, GrepData.RegExp.GetLastErrorText());
                SalMessageBox(HWindow, buf, LoadStr(IDS_ERRORFINDINGFILE), MB_OK | MB_ICONEXCLAMATION);
                if (GrepData.Refine != 0)
                    FoundFilesListView->DestroyDataForRefine();
                return; // error
            }
            GrepData.Grep = TRUE;
        }
        else
        {
            if (Data.HexMode)
            {
                char hex[GREP_TEXT_LEN];
                int len;
                ConvertHexToString(Data.GrepText, hex, len);
                GrepData.SearchData.Set(hex, len, (WORD)(sfForward | (Data.CaseSensitive ? sfCaseSensitive : 0)));
            }
            else
                GrepData.SearchData.Set(Data.GrepText, (WORD)(sfForward |
                                                              (Data.CaseSensitive ? sfCaseSensitive : 0)));
            GrepData.Grep = GrepData.SearchData.IsGood();
        }
    }
    SetFocus(FoundFilesListView->HWindow);

    BuildSerchForData();
    GrepData.Data = &SearchForData;
    GrepData.StopSearch = FALSE;
    GrepData.HWindow = HWindow;

    // advanced search
    memmove(&GrepData.Criteria, &Data.Criteria, sizeof(Data.Criteria));

    GrepData.FoundFilesListView = FoundFilesListView;
    GrepData.FoundVisibleCount = 0;
    GrepData.FoundVisibleTick = GetTickCount();

    GrepData.SearchingText = &SearchingText;
    GrepData.SearchingText2 = &SearchingText2;

    DWORD threadId;
    GrepThread = HANDLES(CreateThread(NULL, 0, GrepThreadF, &GrepData, 0, &threadId));
    if (GrepThread == NULL)
    {
        TRACE_E("Unable to start GrepThread thread.");
        if (GrepData.Refine != 0)
            FoundFilesListView->DestroyDataForRefine();
        return;
    }

    if (OKButton != NULL) // hide the drop-down arrow
    {
        DWORD flags = OKButton->GetFlags();
        flags &= ~BTF_DROPDOWN;
        OKButton->SetFlags(flags, FALSE);
    }
    SetDlgItemText(HWindow, IDOK, LoadStr(IDS_FF_STOP));

    SearchInProgress = TRUE;

    // start a timer to update the dirty text
    SetTimer(HWindow, IDT_REPAINT, 100, NULL);

    // force the first refresh of the status bar
    SearchingText.SetDirty(TRUE);
    PostMessage(HWindow, WM_TIMER, IDT_REPAINT, 0);

    char buff[MAX_PATH + 100];
    _snprintf_s(buff, _TRUNCATE, NORMAL_FINDING_CAPTION, LoadStr(IDS_FF_NAME), LoadStr(IDS_FF_NAMED), SearchForData[0]->MasksGroup.GetMasksString());
    SetWindowText(HWindow, buff);

    EnableControls();
}

void CFindDialog::StopSearch()
{
    CALL_STACK_MESSAGE1("CFindDialog::StopSearch()");
    GrepData.StopSearch = TRUE;
    MSG msg;
    while (1)
    {
        BOOL oldCanClose = CanClose;
        CanClose = FALSE; // don't allow closing while we are inside this method

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        { // message loop for messages from the grep thread
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        CanClose = oldCanClose;
        if (GrepThread == NULL)
            return; // DispatchMessage may call us again and we've already handled closing
        if (WaitForSingleObject(GrepThread, 100) != WAIT_TIMEOUT)
            break;
    }
    if (GrepThread != NULL)
        HANDLES(CloseHandle(GrepThread));
    GrepThread = NULL;

    SearchInProgress = FALSE;
    if (OKButton != NULL) // trigger the drop-down arrow
    {
        DWORD flags = OKButton->GetFlags();
        flags |= BTF_DROPDOWN;
        OKButton->SetFlags(flags, FALSE);
    }
    SetDlgItemText(HWindow, IDOK, FindNowText);

    // stop the timer used for updating the text
    KillTimer(HWindow, IDT_REPAINT);

    // if the second text appeared during search, it's time to hide it
    if (TwoParts)
    {
        SearchingText2.Set("");
        SetTwoStatusParts(FALSE);
    }

    SearchingText.Set("");
    UpdateStatusBar = FALSE;
    if (!GrepData.SearchStopped)
    {
        DWORD items = FoundFilesListView->GetCount();
        if (items == 0)
        {
            int msgID = GrepData.FindDuplicates ? IDS_FIND_NO_DUPS_FOUND : IDS_FIND_NO_FILES_FOUND;
            SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS, (LPARAM)LoadStr(msgID));
        }
        else
            UpdateStatusBar = TRUE;

        if (Log.GetErrorCount() > 0 && Configuration.ShowGrepErrors)
            OnShowLog();
    }
    else
    {
        SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS, (LPARAM)LoadStr(IDS_STOPPED));
    }

    SetWindowText(HWindow, LoadStr(IDS_FF_NAME));
    if (GrepData.Refine != 0)
        FoundFilesListView->DestroyDataForRefine();
    UpdateListViewItems();
    EnableControls();
}

void CFindDialog::EnableToolBar()
{
    if (FoundFilesListView == NULL)
        return;
    BOOL lvFocused = GetFocus() == FoundFilesListView->HWindow;
    BOOL selectedCount = ListView_GetSelectedCount(FoundFilesListView->HWindow);
    int focusedIndex = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_FOCUSED);
    BOOL focusedIsFile = focusedIndex != -1 && !FoundFilesListView->At(focusedIndex)->IsDir;

    TBHeader->EnableItem(CM_FIND_FOCUS, FALSE, lvFocused && focusedIndex != -1);
    TBHeader->EnableItem(CM_FIND_VIEW, FALSE, lvFocused && focusedIsFile);
    TBHeader->EnableItem(CM_FIND_EDIT, FALSE, lvFocused && focusedIsFile);
    TBHeader->EnableItem(CM_FIND_DELETE, FALSE, lvFocused && selectedCount > 0);
    TBHeader->EnableItem(CM_FIND_USERMENU, FALSE, lvFocused && selectedCount > 0);
    TBHeader->EnableItem(CM_FIND_PROPERTIES, FALSE, lvFocused && selectedCount > 0);
    TBHeader->EnableItem(CM_FIND_CLIPCUT, FALSE, lvFocused && selectedCount > 0);
    TBHeader->EnableItem(CM_FIND_CLIPCOPY, FALSE, lvFocused && selectedCount > 0);
    TBHeader->EnableItem(IDC_FIND_STOP, FALSE, SearchInProgress);
}

void CFindDialog::EnableControls(BOOL nextIsButton)
{
    CALL_STACK_MESSAGE2("CFindDialog::EnableButtons(%d)", nextIsButton);
    if (FoundFilesListView == NULL)
        return;

    EnableToolBar();

    HWND focus = GetFocus();
    if (SearchInProgress)
    {
        HWND hFocus = GetFocus();

        EnableWindow(GetDlgItem(HWindow, IDC_FIND_NAMED), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_LOOKIN), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_LOOKIN_BROWSE), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_INCLUDE_SUBDIR), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_INCLUDE_ARCHIVES), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_GREP), FALSE);
        if (Expanded)
        {
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_REGEXP_BROWSE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_HEX), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_WHOLE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CASE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_REGULAR), FALSE);
        }
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_ADVANCED), FALSE);

        if (hFocus != NULL && !IsWindowEnabled(hFocus))
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)FoundFilesListView->HWindow, TRUE);
    }
    else
    {
        HWND setFocus = NULL;

        BOOL enableHexMode = !Data.RegularExpresions;
        if (!enableHexMode && GetDlgItem(HWindow, IDC_FIND_HEX) == focus)
            setFocus = GetDlgItem(HWindow, IDC_FIND_CONTAINING);

        TBHeader->EnableItem(IDC_FIND_STOP, FALSE, FALSE);

        /*
    int foundItems = FoundFilesListView->GetCount();
    int refineItems = FoundFilesListView->GetDataForRefineCount();
    BOOL refine = FALSE;
    EnableWindow(GetDlgItem(HWindow, IDC_FIND_INCLUDE_ARCHIVES), foundItems > 0);
    if (foundItems == 0 && refineItems == 0)
    {
      if (IsDlgButtonChecked(HWindow, IDC_FIND_INCLUDE_ARCHIVES) == BST_CHECKED)
        CheckDlgButton(HWindow, IDC_FIND_INCLUDE_ARCHIVES, BST_UNCHECKED);
    }
    else
      refine = IsDlgButtonChecked(HWindow, IDC_FIND_INCLUDE_ARCHIVES) == BST_CHECKED;
//    EnableWindow(GetDlgItem(HWindow, IDC_FIND_LOOKIN), !refine);
//    EnableWindow(GetDlgItem(HWindow, IDC_FIND_LOOKIN_BROWSE), !refine);
//    EnableWindow(GetDlgItem(HWindow, IDC_FIND_INCLUDE_SUBDIR), !refine);
    */

        EnableWindow(GetDlgItem(HWindow, IDC_FIND_NAMED), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_LOOKIN), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_LOOKIN_BROWSE), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_INCLUDE_SUBDIR), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_GREP), TRUE);
        if (Expanded)
        {
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CONTAINING), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_REGEXP_BROWSE), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_HEX), enableHexMode);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_WHOLE), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_CASE), TRUE);
            EnableWindow(GetDlgItem(HWindow, IDC_FIND_REGULAR), TRUE);
        }
        EnableWindow(GetDlgItem(HWindow, IDC_FIND_ADVANCED), TRUE);

        if (setFocus != NULL)
            SetFocus(setFocus);
    }

    int defID;
    if (focus == FoundFilesListView->HWindow &&
        ListView_GetItemCount(FoundFilesListView->HWindow) > 0)
    { // without the default push button
        defID = (int)SendMessage(HWindow, DM_GETDEFID, 0, 0);
        if (HIWORD(defID) == DC_HASDEFID)
            defID = LOWORD(defID);
        else
            defID = -1;
        SendMessage(HWindow, DM_SETDEFID, -1, 0);
        if (defID != -1)
            SendMessage(GetDlgItem(HWindow, defID), BM_SETSTYLE,
                        BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
    }
    else // select the default push button
    {
        if (nextIsButton)
        {
            defID = (int)SendMessage(HWindow, DM_GETDEFID, 0, 0);
            if (HIWORD(defID) == DC_HASDEFID)
            {
                defID = LOWORD(defID);
                PostMessage(GetDlgItem(HWindow, defID), BM_SETSTYLE, BS_PUSHBUTTON,
                            MAKELPARAM(TRUE, 0));
            }
        }
        defID = IDOK;
        // the following code caused the Find Now button to flicker during search
        // when the mouse focus rested on it; removing it doesn't seem to break anything, we'll see...
        /*
    char className[30];
    WORD wl = LOWORD(GetWindowLongPtr(focus, GWL_STYLE));  // only BS_ styles
    if (GetClassName(focus, className, 30) != 0 &&
        StrICmp(className, "BUTTON") == 0 &&
        (wl == BS_PUSHBUTTON || wl == BS_DEFPUSHBUTTON))
    {
      nextIsButton = TRUE;
      PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
    }
*/
        SendMessage(HWindow, DM_SETDEFID, defID, 0);
        if (nextIsButton)
            PostMessage(GetDlgItem(HWindow, defID), BM_SETSTYLE, BS_PUSHBUTTON,
                        MAKELPARAM(TRUE, 0));
    }
}

void CFindDialog::UpdateListViewItems()
{
    if (FoundFilesListView != NULL)
    {
        int count = FoundFilesListView->GetCount();

        // inform the list view about the new item count
        ListView_SetItemCountEx(FoundFilesListView->HWindow,
                                count,
                                LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        // for the first added data, select the first item
        if (GrepData.FoundVisibleCount == 0 && count > 0)
            ListView_SetItemState(FoundFilesListView->HWindow, 0,
                                  LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED)

                // write the number of items above the list view
                TBHeader->SetFoundCount(count);
        TBHeader->SetErrorsInfosCount(Log.GetErrorCount(), Log.GetInfoCount());

        // when minimized, display the item count in the title
        if (IsIconic(HWindow))
        {
            char buf[MAX_PATH + 100];
            if (SearchInProgress)
            {
                _snprintf_s(buf, _TRUNCATE, MINIMIZED_FINDING_CAPTION, FoundFilesListView->GetCount(),
                            LoadStr(IDS_FF_NAME), LoadStr(IDS_FF_NAMED), SearchForData[0]->MasksGroup.GetMasksString());
            }
            else
                lstrcpy(buf, LoadStr(IDS_FF_NAME));
            SetWindowText(HWindow, buf);
        }

        // used by the search thread to know when to notify us next
        GrepData.FoundVisibleCount = count;
        GrepData.FoundVisibleTick = GetTickCount();

        EnableToolBar();
    }
}

void CFindDialog::OnFocusFile()
{
    CALL_STACK_MESSAGE1("CFindDialog::FocusButton()");

    int index = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_FOCUSED);
    if (index < 0)
        return;

    if (SalamanderBusy)
    {
        Sleep(200); // give Salamander time-if we switched from the main window the menu's message queue might still be running
        if (SalamanderBusy)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                          LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    CFoundFilesData* data = FoundFilesListView->At(index);
    SendMessage(MainWindow->GetActivePanel()->HWindow, WM_USER_FOCUSFILE, (WPARAM)data->Name, (LPARAM)data->Path);
}

BOOL CFindDialog::GetFocusedFile(char* buffer, int bufferLen, int* viewedIndex)
{
    int index = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_FOCUSED);
    if (index < 0)
        return FALSE;

    if (viewedIndex != NULL)
        *viewedIndex = index;

    CFoundFilesData* data = FoundFilesListView->At(index);
    if (data->IsDir)
        return FALSE;
    char longName[MAX_PATH];
    int len = (int)strlen(data->Path);
    memmove(longName, data->Path, len);
    if (data->Path[len - 1] != '\\')
        longName[len++] = '\\';
    strcpy(longName + len, data->Name);

    lstrcpyn(buffer, longName, bufferLen);
    return TRUE;
}

void CFindDialog::UpdateInternalViewerData()
{
    // copy the find text to the internal viewer
    if (Configuration.CopyFindText)
    { // Alt+F3 never reaches here, so no alternate viewer...
        CFindSetDialog oldGlobalFindDialog = GlobalFindDialog;

        GlobalFindDialog.Forward = TRUE;
        CTransferInfo dummyTI(HWindow, ttDataFromWindow);
        dummyTI.CheckBox(IDC_FIND_WHOLE, GlobalFindDialog.WholeWords);
        dummyTI.CheckBox(IDC_FIND_CASE, GlobalFindDialog.CaseSensitive);
        dummyTI.CheckBox(IDC_FIND_HEX, GlobalFindDialog.HexMode);
        dummyTI.CheckBox(IDC_FIND_REGULAR, GlobalFindDialog.Regular);
        dummyTI.EditLine(IDC_FIND_CONTAINING, GlobalFindDialog.Text, FIND_TEXT_LEN);

        HistoryComboBox(NULL, dummyTI, 0, GlobalFindDialog.Text,
                        (int)strlen(GlobalFindDialog.Text),
                        !GlobalFindDialog.Regular && GlobalFindDialog.HexMode,
                        VIEWER_HISTORY_SIZE,
                        ViewerHistory, TRUE);
        if (!dummyTI.IsGood()) // something went wrong (hex mode)
            GlobalFindDialog = oldGlobalFindDialog;
    }
}

void CFindDialog::OnViewFile(BOOL alternate)
{
    CALL_STACK_MESSAGE2("CFindDialog::OnViewFile(%d)", alternate);
    char longName[MAX_PATH];
    int viewedIndex = 0;
    if (!GetFocusedFile(longName, MAX_PATH, &viewedIndex))
        return;

    if (SalamanderBusy)
    {
        Sleep(200); // give Salamander time-if we switched from the main window
                    // the menu's message queue might still be running
        if (SalamanderBusy)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                          LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    UpdateInternalViewerData();
    COpenViewerData openData;
    openData.FileName = longName;
    openData.EnumFileNamesSourceUID = FoundFilesListView->EnumFileNamesSourceUID;
    openData.EnumFileNamesLastFileIndex = viewedIndex;
    SendMessage(MainWindow->GetActivePanel()->HWindow, WM_USER_VIEWFILE, (WPARAM)(&openData), (LPARAM)alternate);
}

void CFindDialog::OnEditFile()
{
    CALL_STACK_MESSAGE1("CFindDialog::OnEditFile()");
    char longName[MAX_PATH];
    if (!GetFocusedFile(longName, MAX_PATH, NULL))
        return;

    if (SalamanderBusy)
    {
        Sleep(200); // give Salamander time-if we switched from the main window
                    // the menu's message queue might still be running
        if (SalamanderBusy)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                          LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    SendMessage(MainWindow->GetActivePanel()->HWindow, WM_USER_EDITFILE, (WPARAM)longName, 0);
}

void CFindDialog::OnViewFileWith()
{
    CALL_STACK_MESSAGE1("CFindDialog::OnViewFileWith()");
    char longName[MAX_PATH];
    int viewedIndex = 0;
    if (!GetFocusedFile(longName, MAX_PATH, &viewedIndex))
        return;

    if (SalamanderBusy)
    {
        Sleep(200); // give Salamander time-if we switched from the main window
                    // the menu's message queue might still be running
        if (SalamanderBusy)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                          LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    UpdateInternalViewerData();
    POINT menuPoint;
    GetListViewContextMenuPos(FoundFilesListView->HWindow, &menuPoint);
    DWORD handlerID;
    // this call isn't entirely correct because ViewFileWith lacks critical sections
    // for working with the configuration. the assumption for proper functioning is that the user does only one thing
    // (doesn't edit configuration while working in the Find window) -- hopefully almost always true
    MainWindow->GetActivePanel()->ViewFileWith(longName, FoundFilesListView->HWindow, &menuPoint, &handlerID, -1, -1);
    if (handlerID != 0xFFFFFFFF)
    {
        if (SalamanderBusy) // almost impossible, but Salamander could be busy
        {
            Sleep(200); // give Salamander time-if we switched from the main window
                        // the menu's message queue might still be running
            if (SalamanderBusy)
            {
                SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                              LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                return;
            }
        }
        COpenViewerData openData;
        openData.FileName = longName;
        openData.EnumFileNamesSourceUID = FoundFilesListView->EnumFileNamesSourceUID;
        openData.EnumFileNamesLastFileIndex = viewedIndex;
        SendMessage(MainWindow->GetActivePanel()->HWindow, WM_USER_VIEWFILEWITH,
                    (WPARAM)(&openData), (LPARAM)handlerID);
    }
}

void CFindDialog::OnEditFileWith()
{
    CALL_STACK_MESSAGE1("CFindDialog::OnEditFileWith()");
    char longName[MAX_PATH];
    if (!GetFocusedFile(longName, MAX_PATH, NULL))
        return;

    if (SalamanderBusy)
    {
        Sleep(200); // give Salamander time-if we switched from the main window
                    // the menu's message queue might still be running
        if (SalamanderBusy)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                          LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    POINT menuPoint;
    GetListViewContextMenuPos(FoundFilesListView->HWindow, &menuPoint);
    DWORD handlerID;
    // this call isn't entirely correct because EditFileWith lacks critical sections
    // for working with the configuration. the assumption for proper functioning is that the user does only one thing
    // (doesn't change configuration while using the Find window) -- hopefully almost always true
    MainWindow->GetActivePanel()->EditFileWith(longName, FoundFilesListView->HWindow, &menuPoint, &handlerID);
    if (handlerID != 0xFFFFFFFF)
    {
        if (SalamanderBusy) // almost impossible, but Salamander could be busy
        {
            Sleep(200); // give Salamander time-if we switched from the main window
                        // the menu's message queue might still be running
            if (SalamanderBusy)
            {
                SalMessageBox(HWindow, LoadStr(IDS_SALAMANDBUSY2),
                              LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                return;
            }
        }
        SendMessage(MainWindow->GetActivePanel()->HWindow, WM_USER_EDITFILEWITH,
                    (WPARAM)longName, (WPARAM)handlerID);
    }
}

void CFindDialog::OnUserMenu()
{
    CALL_STACK_MESSAGE1("CFindDialog::OnUserMenu()");
    DWORD selectedCount = ListView_GetSelectedCount(FoundFilesListView->HWindow);
    if (selectedCount < 1)
        return;

    UserMenuIconBkgndReader.BeginUserMenuIconsInUse();
    CMenuPopup menu;
    MainWindow->FillUserMenu(&menu, FALSE); // keep customization disabled
    POINT p;
    GetListViewContextMenuPos(FoundFilesListView->HWindow, &p);
    // another locking round (BeginUserMenuIconsInUse+EndUserMenuIconsInUse) will happen
    // inside WM_USER_ENTERMENULOOP and WM_USER_LEAVEMENULOOP; it's nested and has no overhead,
    // so we ignore it and don't try to fight it
    DWORD cmd = menu.Track(MENU_TRACK_RETURNCMD, p.x, p.y, HWindow, NULL);
    UserMenuIconBkgndReader.EndUserMenuIconsInUse();

    if (cmd != 0)
    {
        CUserMenuAdvancedData userMenuAdvancedData;

        char* list = userMenuAdvancedData.ListOfSelNames;
        char* listEnd = list + USRMNUARGS_MAXLEN - 1;
        int findItem = -1;
        DWORD i;
        for (i = 0; i < selectedCount; i++) // fill the list of selected names
        {
            findItem = ListView_GetNextItem(FoundFilesListView->HWindow, findItem, LVNI_SELECTED);
            if (findItem != -1)
            {
                if (list > userMenuAdvancedData.ListOfSelNames)
                {
                    if (list < listEnd)
                        *list++ = ' ';
                    else
                        break;
                }
                CFoundFilesData* file = FoundFilesListView->At(findItem);
                if (!AddToListOfNames(&list, listEnd, file->Name, (int)strlen(file->Name)))
                    break;
            }
        }
        if (i < selectedCount)
            userMenuAdvancedData.ListOfSelNames[0] = 0; // small buffer for the list of selected names
        else
            *list = 0;
        userMenuAdvancedData.ListOfSelNamesIsEmpty = FALSE; // not a concern for Find (otherwise the User Menu would not open)

        char* listFull = userMenuAdvancedData.ListOfSelFullNames;
        char* listFullEnd = listFull + USRMNUARGS_MAXLEN - 1;
        findItem = -1;
        for (i = 0; i < selectedCount; i++) // fill the list of selected names
        {
            findItem = ListView_GetNextItem(FoundFilesListView->HWindow, findItem, LVNI_SELECTED);
            if (findItem != -1)
            {
                if (listFull > userMenuAdvancedData.ListOfSelFullNames)
                {
                    if (listFull < listFullEnd)
                        *listFull++ = ' ';
                    else
                        break;
                }
                CFoundFilesData* file = FoundFilesListView->At(findItem);
                char fullName[MAX_PATH];
                lstrcpyn(fullName, file->Path, MAX_PATH);
                if (!SalPathAppend(fullName, file->Name, MAX_PATH) ||
                    !AddToListOfNames(&listFull, listFullEnd, fullName, (int)strlen(fullName)))
                    break;
            }
        }
        if (i < selectedCount)
            userMenuAdvancedData.ListOfSelFullNames[0] = 0; // small buffer for the list of selected full names
        else
            *listFull = 0;
        userMenuAdvancedData.ListOfSelFullNamesIsEmpty = FALSE; // not a concern for Find (otherwise the User Menu would not open)

        userMenuAdvancedData.FullPathLeft[0] = 0;
        userMenuAdvancedData.FullPathRight[0] = 0;
        userMenuAdvancedData.FullPathInactive = userMenuAdvancedData.FullPathLeft;

        int comp1 = -1;
        int comp2 = -1;
        if (selectedCount == 1)
            comp1 = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_SELECTED);
        else
        {
            if (selectedCount == 2)
            {
                comp1 = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_SELECTED);
                comp2 = ListView_GetNextItem(FoundFilesListView->HWindow, comp1, LVNI_SELECTED);
            }
        }
        userMenuAdvancedData.CompareNamesAreDirs = FALSE;
        userMenuAdvancedData.CompareNamesReversed = FALSE;
        if (comp1 != -1 && comp2 != -1 &&
            FoundFilesListView->At(comp1)->IsDir != FoundFilesListView->At(comp2)->IsDir)
        {
            comp1 = -1;
            comp2 = -1;
        }
        if (comp1 == -1)
            userMenuAdvancedData.CompareName1[0] = 0;
        else
        {
            CFoundFilesData* file = FoundFilesListView->At(comp1);
            userMenuAdvancedData.CompareNamesAreDirs = file->IsDir;
            lstrcpyn(userMenuAdvancedData.CompareName1, file->Path, MAX_PATH);
            if (!SalPathAppend(userMenuAdvancedData.CompareName1, file->Name, MAX_PATH))
                userMenuAdvancedData.CompareName1[0] = 0;
        }
        if (comp2 == -1)
            userMenuAdvancedData.CompareName2[0] = 0;
        else
        {
            CFoundFilesData* file = FoundFilesListView->At(comp2);
            userMenuAdvancedData.CompareNamesAreDirs = file->IsDir;
            lstrcpyn(userMenuAdvancedData.CompareName2, file->Path, MAX_PATH);
            if (!SalPathAppend(userMenuAdvancedData.CompareName2, file->Name, MAX_PATH))
                userMenuAdvancedData.CompareName2[0] = 0;
        }

        CUMDataFromFind data(FoundFilesListView->HWindow);
        MainWindow->UserMenu(HWindow, cmd - CM_USERMENU_MIN,
                             GetNextItemFromFind, &data, &userMenuAdvancedData);
        SetFocus(FoundFilesListView->HWindow);
    }
}

void CFindDialog::OnCopyNameToClipboard(CCopyNameToClipboardModeEnum mode)
{
    CALL_STACK_MESSAGE1("CFindDialog::FocusButton()");
    DWORD selectedCount = ListView_GetSelectedCount(FoundFilesListView->HWindow);
    if (selectedCount != 1)
        return;
    int index = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_SELECTED);
    if (index < 0)
        return;
    CFoundFilesData* data = FoundFilesListView->At(index);
    char buff[2 * MAX_PATH];
    buff[0] = 0;
    switch (mode)
    {
    case cntcmFullName:
    {
        strcpy(buff, data->Path);
        int len = (int)strlen(buff);
        if (len > 0 && buff[len - 1] != '\\')
            strcat(buff, "\\");
        AlterFileName(buff + strlen(buff), data->Name, -1, FileNameFormat, 0, data->IsDir);
        break;
    }

    case cntcmName:
    {
        AlterFileName(buff, data->Name, -1, FileNameFormat, 0, data->IsDir);
        break;
    }

    case cntcmFullPath:
    {
        strcpy(buff, data->Path);
        break;
    }

    case cntcmUNCName:
    {
        AlterFileName(buff, data->Name, -1, FileNameFormat, 0, data->IsDir);
        CopyUNCPathToClipboard(data->Path, buff, data->IsDir, HWindow);
        break;
    }
    }
    if (mode != cntcmUNCName)
        CopyTextToClipboard(buff);
}

BOOL CFindDialog::IsMenuBarMessage(CONST MSG* lpMsg)
{
    CALL_STACK_MESSAGE_NONE
    if (MenuBar == NULL)
        return FALSE;
    return MenuBar->IsMenuBarMessage(lpMsg);
}

void CFindDialog::InsertDrives(HWND hEdit, BOOL network)
{
    CALL_STACK_MESSAGE_NONE
    char drives[200];
    char* iterator = drives;
    char root[4] = " :\\";
    char drive = 'A';
    DWORD mask = GetLogicalDrives();
    int i = 1;
    while (i != 0)
    {
        if (mask & i) // the drive is accessible
        {
            root[0] = drive;
            DWORD driveType = GetDriveType(root);
            if (driveType == DRIVE_FIXED || network && driveType == DRIVE_REMOTE)
            {
                if (iterator > drives)
                {
                    *iterator++ = ';';
                }
                memmove(iterator, root, 3);
                iterator += 3;
            }
        }
        i <<= 1;
        drive++;
    }
    *iterator = '\0';

    SetWindowText(hEdit, drives);
    SendMessage(hEdit, EM_SETSEL, lstrlen(drives), lstrlen(drives));
}

BOOL CFindDialog::CanCloseWindow()
{
    // we used to get bug reports with crashes in CFindDialog::StopSearch()
    // likely because the window was being destroyed while still inside the method
    // this check on CanClose disappeared after version 1.52
    if (!CanClose)
        return FALSE;

    // if CShellExecuteWnd windows exist, we offer to cancel closing or send a bug report and terminate
    char reason[BUG_REPORT_REASON_MAX]; // cause of the problem + list of windows (multiline)
    strcpy(reason, "Some faulty shell extension has locked our find window.");
    if (EnumCShellExecuteWnd(HWindow, reason + (int)strlen(reason), BUG_REPORT_REASON_MAX - ((int)strlen(reason) + 1)) > 0)
    {
        // ask whether Salamander should continue or generate a bug report
        if (SalMessageBox(HWindow, LoadStr(IDS_SHELLEXTBREAK3), SALAMANDER_TEXT_VERSION,
                          MSGBOXEX_CONTINUEABORT | MB_ICONINFORMATION | MSGBOXEX_SETFOREGROUND) != IDABORT)
        {
            return FALSE; // continue
        }

        // break into the debugger
        strcpy(BugReportReasonBreak, reason);
        TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
        // freeze this thread
        while (1)
            Sleep(1000);
    }
    return TRUE;
}

BOOL CFindDialog::DoYouWantToStopSearching()
{
    int ret = IDYES;
    if (Configuration.CnfrmStopFind)
    {
        BOOL dontShow = !Configuration.CnfrmStopFind;

        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SETFOREGROUND | MSGBOXEX_HINT;
        params.Caption = LoadStr(IDS_WANTTOSTOPTITLE);
        params.Text = LoadStr(IDS_WANTTOSTOP);
        params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAINSS);
        params.CheckBoxValue = &dontShow;
        ret = SalMessageBoxEx(&params);
        Configuration.CnfrmStopFind = !dontShow;
    }
    return (ret == IDYES);
}

// pull the text from the control and search for a hot key;
// if found, return its character (uppercase), otherwise return 0
char GetControlHotKey(HWND hWnd, int resID)
{
    char buff[500];
    if (!GetDlgItemText(hWnd, resID, buff, 500))
        return 0;
    const char* p = buff;
    while (*p != 0)
    {
        if (*p == '&' && *(p + 1) != '&')
            return UpperCase[*(p + 1)];
        p++;
    }
    return 0;
}

BOOL CFindDialog::ManageHiddenShortcuts(const MSG* msg)
{
    if (msg->message == WM_SYSKEYDOWN)
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!controlPressed && altPressed && !shiftPressed)
        {
            // if Alt+? is pressed while the Options section is collapsed, it makes sense to investigate further
            if (!IsDlgButtonChecked(HWindow, IDC_FIND_GREP))
            {
                // check the hotkeys of monitored controls
                int resID[] = {IDC_FIND_CONTAINING_TEXT, IDC_FIND_HEX, IDC_FIND_CASE,
                               IDC_FIND_WHOLE, IDC_FIND_REGULAR, -1}; // (terminate with -1)
                int i;
                for (i = 0; resID[i] != -1; i++)
                {
                    char key = GetControlHotKey(HWindow, resID[i]);
                    if (key != 0 && (WPARAM)key == msg->wParam)
                    {
                        // expand the Options section
                        CheckDlgButton(HWindow, IDC_FIND_GREP, BST_CHECKED);
                        SendMessage(HWindow, WM_COMMAND, MAKEWPARAM(IDC_FIND_GREP, BN_CLICKED), 0);
                        return FALSE; // expanded; IsDialogMessage will handle the rest after we return
                    }
                }
            }
        }
    }
    return FALSE; // not our message
}

void CFindDialog::SetFullRowSelect(BOOL fullRow)
{
    Configuration.FindFullRowSelect = fullRow;

    // notify all find dialogs about the change
    FindDialogQueue.BroadcastMessage(WM_USER_FINDFULLROWSEL, 0, 0);
}

INT_PTR
CFindDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFindDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        FindDialogQueue.Add(new CWindowQueueItem(HWindow));

        GetDlgItemText(HWindow, IDOK, FindNowText, 100);

        UpdateAdvancedText();

        InstallWordBreakProc(GetDlgItem(HWindow, IDC_FIND_NAMED));      // install WordBreakProc into the combo box
        InstallWordBreakProc(GetDlgItem(HWindow, IDC_FIND_LOOKIN));     // install WordBreakProc into the combo box
        InstallWordBreakProc(GetDlgItem(HWindow, IDC_FIND_CONTAINING)); // install WordBreakProc into the combo box

        CComboboxEdit* edit = new CComboboxEdit();
        if (edit != NULL)
        {
            HWND hCombo = GetDlgItem(HWindow, IDC_FIND_CONTAINING);
            edit->AttachToWindow(GetWindow(hCombo, GW_CHILD));
        }
        ChangeToArrowButton(HWindow, IDC_FIND_REGEXP_BROWSE);

        OKButton = new CButton(HWindow, IDOK, BTF_DROPDOWN);
        new CButton(HWindow, IDC_FIND_LOOKIN_BROWSE, BTF_RIGHTARROW);

        // set the checkbox controlling the visibility of the Content section of the dialog
        CheckDlgButton(HWindow, IDC_FIND_GREP, Configuration.SearchFileContent);

        // assign an icon to the window
        HICON findIcon = HANDLES(LoadIcon(ImageResDLL, MAKEINTRESOURCE(8)));
        if (findIcon == NULL)
            findIcon = HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(IDI_FIND)));
        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)findIcon);

        // construct the list view
        FoundFilesListView = new CFoundFilesListView(HWindow, IDC_FIND_RESULTS, this);

        SetFullRowSelect(Configuration.FindFullRowSelect);

        TBHeader = new CFindTBHeader(HWindow, IDC_FIND_FOUND_FILES);

        // create the status bar
        HStatusBar = CreateWindowEx(0,
                                    STATUSCLASSNAME,
                                    (LPCTSTR)NULL,
                                    SBARS_SIZEGRIP | WS_CHILD | CCS_BOTTOM | WS_VISIBLE,
                                    0, 0, 0, 0,
                                    HWindow,
                                    (HMENU)IDC_FIND_STATUS,
                                    HInstance,
                                    NULL);
        if (HStatusBar == NULL)
        {
            TRACE_E("Error creating StatusBar");
            DlgFailed = TRUE;
            PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            break;
        }

        SetTwoStatusParts(FALSE, TRUE);
        SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS, (LPARAM)LoadStr(IDS_FIND_INIT_HINT));

        // assign a menu to the window
        MainMenu = new CMenuPopup;

        BuildFindMenu(MainMenu);
        MenuBar = new CMenuBar(MainMenu, HWindow);
        if (!MenuBar->CreateWnd(HWindow))
        {
            TRACE_E("Error creating Menu");
            DlgFailed = TRUE;
            PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            break;
        }
        ShowWindow(MenuBar->HWindow, SW_SHOW);

        // load parameters for laying out the window
        GetLayoutParams();

        WINDOWPLACEMENT* wp = &Configuration.FindDialogWindowPlacement;
        if (wp->length != 0)
        {
            RECT r = wp->rcNormalPosition;
            SetWindowPos(HWindow, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
            if (wp->showCmd == SW_MAXIMIZE || wp->showCmd == SW_SHOWMAXIMIZED)
                ShowWindow(HWindow, SW_MAXIMIZE);
        }

        SetWindowPos(HWindow, Configuration.AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        LayoutControls();
        FoundFilesListView->InitColumns();
        SetContentVisible(Configuration.SearchFileContent);

        // remove WS_TABSTOP from IDC_FIND_ADVANCED_TEXT
        DWORD style = (DWORD)GetWindowLongPtr(GetDlgItem(HWindow, IDC_FIND_ADVANCED_TEXT), GWL_STYLE);
        style &= ~WS_TABSTOP;
        SetWindowLongPtr(GetDlgItem(HWindow, IDC_FIND_ADVANCED_TEXT), GWL_STYLE, style);

        ListView_SetItemCount(FoundFilesListView->HWindow, 0);

        SetDlgItemText(HWindow, IDOK, FindNowText);
        TBHeader->SetFoundCount(0);

        SetWindowText(HWindow, LoadStr(IDS_FF_NAME));

        int i;
        for (i = 0; i < FindOptions.GetCount(); i++)
            if (FindOptions.At(i)->AutoLoad)
            {
                char buff[1024];
                sprintf(buff, LoadStr(IDS_FF_AUTOLOAD), FindOptions.At(i)->ItemName);
                SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS, (LPARAM)buff);
                break;
            }

        HWND hCombo = GetDlgItem(HWindow, IDC_FIND_LOOKIN);
        EditLine->AttachToWindow(GetWindow(hCombo, GW_CHILD));

        // not supported yet, hide the option
        ShowWindow(GetDlgItem(HWindow, IDC_FIND_INCLUDE_ARCHIVES), FALSE);

        EnableControls();
        break;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_REPAINT)
        {
            char buf[MAX_PATH + 50];
            if (SearchingText.GetDirty())
            {
                SearchingText.SetDirty(FALSE); // already being redrawn - Get will be called; better to refresh twice than not at all
                                               //          SearchingText.Get(buf, MAX_PATH + 50);
                SendMessage(HStatusBar, SB_SETTEXT, 1 | SBT_NOBORDERS | SBT_OWNERDRAW, 0);
            }
            if (SearchingText2.GetDirty())
            {
                if (!TwoParts)
                    SetTwoStatusParts(TRUE);
                SearchingText2.SetDirty(FALSE); // already being redrawn - Get will be called; better to refresh twice than not at all
                SearchingText2.Get(buf, MAX_PATH + 50);
                int pos = buf[0]; // extract the value directly instead of the string
                SendMessage(HProgressBar, PBM_SETPOS, pos, 0);
            }
            return 0;
        }
        break;
    }

    case WM_USER_FLASHICON:
    {
        if (GetForegroundWindow() == HWindow && TBHeader != NULL)
            TBHeader->StartFlashIcon();
        else
            FlashIconsOnActivation = TRUE;
        return 0;
    }

    case WM_USER_COLORCHANGEFIND:
    {
        OnColorsChange();
        return TRUE;
    }

    case WM_USER_FINDFULLROWSEL:
    {
        DWORD flags = ListView_GetExtendedListViewStyle(FoundFilesListView->HWindow);
        BOOL hasFullRow = (flags & LVS_EX_FULLROWSELECT) != 0;
        if (hasFullRow != Configuration.FindFullRowSelect)
        {
            if (Configuration.FindFullRowSelect)
                flags |= LVS_EX_FULLROWSELECT;
            else
                flags &= ~LVS_EX_FULLROWSELECT;
            ListView_SetExtendedListViewStyle(FoundFilesListView->HWindow, flags); // 4.71
        }
        return TRUE;
    }

    case WM_USER_CLEARHISTORY:
    {
        ClearComboboxListbox(GetDlgItem(HWindow, IDC_FIND_NAMED));
        ClearComboboxListbox(GetDlgItem(HWindow, IDC_FIND_LOOKIN));
        ClearComboboxListbox(GetDlgItem(HWindow, IDC_FIND_CONTAINING));
        return TRUE;
    }

    // used to test closing the window because Salamander is shutting down
    case WM_USER_QUERYCLOSEFIND:
    {
        BOOL query = TRUE;
        if (SearchInProgress)
        {
            if (lParam /* quiet */)
                StopSearch(); // no need to ask anything, stop the ongoing search anyway
            else
            {
                if (!DoYouWantToStopSearching())
                    query = FALSE;
                else
                {
                    if (SearchInProgress) // stop searching immediately if the user wants
                        StopSearch();
                }
            }
        }
        if (query)
            query = CanCloseWindow();
        if (StateOfFindCloseQuery == sofcqSentToFind)
            StateOfFindCloseQuery = query ? sofcqCanClose : sofcqCannotClose;
        return TRUE;
    }

    // used for remote closing of the window because Salamander is shutting down
    case WM_USER_CLOSEFIND:
    {
        if (SearchInProgress)
            StopSearch();
        DestroyWindow(HWindow);
        return 0;
    }

    case WM_USER_INITMENUPOPUP:
    {
        // menu enablers
        if (FoundFilesListView != NULL && FoundFilesListView->HWindow != NULL)
        {
            CMenuPopup* popup = (CMenuPopup*)(CGUIMenuPopupAbstract*)wParam;
            WORD popupID = HIWORD(lParam);

            BOOL lvFocused = GetFocus() == FoundFilesListView->HWindow;
            DWORD totalCount = ListView_GetItemCount(FoundFilesListView->HWindow);
            BOOL selectedCount = ListView_GetSelectedCount(FoundFilesListView->HWindow);
            int focusedIndex = ListView_GetNextItem(FoundFilesListView->HWindow, -1, LVNI_FOCUSED);
            BOOL focusedIsFile = focusedIndex != -1 && !FoundFilesListView->At(focusedIndex)->IsDir;

            switch (popupID)
            {
            case CML_FIND_FILES:
            {
                popup->EnableItem(CM_FIND_OPEN, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_OPENSEL, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_FOCUS, FALSE, lvFocused && focusedIndex != -1);
                popup->EnableItem(CM_FIND_HIDESEL, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_HIDE_DUP, FALSE, totalCount > 0);
                popup->EnableItem(CM_FIND_VIEW, FALSE, lvFocused && focusedIsFile);
                popup->EnableItem(CM_FIND_VIEW_WITH, FALSE, lvFocused && focusedIsFile);
                popup->EnableItem(CM_FIND_ALTVIEW, FALSE, lvFocused && focusedIsFile);
                popup->EnableItem(CM_FIND_EDIT, FALSE, lvFocused && focusedIsFile);
                popup->EnableItem(CM_FIND_EDIT_WITH, FALSE, lvFocused && focusedIsFile);
                popup->EnableItem(CM_FIND_DELETE, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_USERMENU, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_PROPERTIES, FALSE, lvFocused && selectedCount > 0);
                break;
            }

            case CML_FIND_FIND:
            {
                popup->EnableItem(CM_FIND_NOW, FALSE, !SearchInProgress);
                popup->EnableItem(CM_FIND_INTERSECT, FALSE, !SearchInProgress && totalCount > 0);
                popup->EnableItem(CM_FIND_SUBTRACT, FALSE, !SearchInProgress && totalCount > 0);
                popup->EnableItem(CM_FIND_APPEND, FALSE, !SearchInProgress && totalCount > 0);
                popup->EnableItem(CM_FIND_DUPLICATES, FALSE, !SearchInProgress);
                popup->EnableItem(CM_FIND_MESSAGES, FALSE, Log.GetCount() > 0);
                break;
            }

            case CML_FIND_EDIT:
            {
                popup->EnableItem(CM_FIND_CLIPCUT, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_CLIPCOPY, FALSE, lvFocused && selectedCount > 0);
                popup->EnableItem(CM_FIND_CLIPCOPYFULLNAME, FALSE, lvFocused && selectedCount == 1);
                popup->EnableItem(CM_FIND_CLIPCOPYNAME, FALSE, lvFocused && selectedCount == 1);
                popup->EnableItem(CM_FIND_CLIPCOPYFULLPATH, FALSE, lvFocused && selectedCount == 1);
                popup->EnableItem(CM_FIND_CLIPCOPYUNCNAME, FALSE, lvFocused && selectedCount == 1);
                popup->EnableItem(CM_FIND_SELECTALL, FALSE, lvFocused && totalCount > 0);
                popup->EnableItem(CM_FIND_INVERTSEL, FALSE, lvFocused && totalCount > 0);
                break;
            }

            case CML_FIND_VIEW:
            {
                BOOL enabledNameSize = TRUE;
                BOOL enabledPathTime = TRUE;
                if (GrepData.FindDuplicates)
                {
                    enabledPathTime = FALSE; // path and time are irrelevant for duplicates
                    // sorting by name and size works for duplicates only
                    // if the search was by the same name and size
                    enabledNameSize = (GrepData.FindDupFlags & FIND_DUPLICATES_NAME) &&
                                      (GrepData.FindDupFlags & FIND_DUPLICATES_SIZE);
                }
                popup->EnableItem(CM_FIND_NAME, FALSE, enabledNameSize && totalCount > 0);
                popup->EnableItem(CM_FIND_PATH, FALSE, enabledPathTime && totalCount > 0);
                popup->EnableItem(CM_FIND_TIME, FALSE, enabledPathTime && totalCount > 0);
                popup->EnableItem(CM_FIND_SIZE, FALSE, enabledNameSize && totalCount > 0);
                break;
            }

            case CML_FIND_OPTIONS:
            {
                static int count = -1;
                if (count == -1)
                    count = popup->GetItemCount();

                popup->CheckItem(CM_FIND_SHOWERRORS, FALSE, Configuration.ShowGrepErrors);
                popup->CheckItem(CM_FIND_FULLROWSEL, FALSE, Configuration.FindFullRowSelect);
                // if the manage dialog is open, disable it in another window and also disable adding to the list
                popup->EnableItem(CM_FIND_ADD_CURRENT, FALSE, !FindManageInUse);
                popup->EnableItem(CM_FIND_MANAGE, FALSE, !FindManageInUse);
                popup->EnableItem(CM_FIND_IGNORE, FALSE, !FindIgnoreInUse);
                FindOptions.InitMenu(popup, !SearchInProgress, count);
                break;
            }
            }
        }
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (SearchInProgress)
            return 0;

        HWND hCtrl = GetDlgItem(HWindow, (int)wParam);
        RECT r;
        GetWindowRect(hCtrl, &r);

        CGUIMenuPopupAbstract* popup = MainMenu->GetSubMenu(CML_FIND_FIND, FALSE);
        if (popup != NULL)
        {
            BOOL selectMenuItem = LOWORD(lParam);
            DWORD flags = 0;
            if (selectMenuItem)
            {
                popup->SetSelectedItemIndex(0);
                flags |= MENU_TRACK_SELECT;
            }
            popup->Track(flags, r.left, r.bottom, HWindow, &r);
        }
        break;
    }

    case WM_SIZE:
    {
        // when restoring, refresh the window title
        if (SearchInProgress && (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)) // restore
        {
            char buff[MAX_PATH + 100];
            _snprintf_s(buff, _TRUNCATE, NORMAL_FINDING_CAPTION, LoadStr(IDS_FF_NAME), LoadStr(IDS_FF_NAMED),
                        SearchForData[0]->MasksGroup.GetMasksString());
            SetWindowText(HWindow, buff);
        }

        //      if (FirstWMSize)
        //        FirstWMSize = FALSE;
        //      else
        LayoutControls();
        break;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgW;
        lpmmi->ptMinTrackSize.y = MinDlgH;
        break;
    }

    case WM_HELP:
    {
        PostMessage(HWindow, WM_COMMAND, CM_HELP_CONTENTS, 0);
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (FoundFilesListView != NULL && ListView_GetEditControl(FoundFilesListView->HWindow) != NULL)
            return 0; // the list view sends some commands while editing
        if (LOWORD(wParam) >= CM_FIND_OPTIONS_FIRST && LOWORD(wParam) <= CM_FIND_OPTIONS_LAST)
        {
            LoadControls(LOWORD(wParam) - CM_FIND_OPTIONS_FIRST);
            return TRUE;
        }

        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_FIND_GREP)
        {
            Configuration.SearchFileContent = IsDlgButtonChecked(HWindow, IDC_FIND_GREP);
            SetContentVisible(Configuration.SearchFileContent);
            if (!Configuration.SearchFileContent)
            {
                // grab the actual content of hidden elements
                SetDlgItemText(HWindow, IDC_FIND_CONTAINING, "");
                CheckDlgButton(HWindow, IDC_FIND_HEX, FALSE);
                CheckDlgButton(HWindow, IDC_FIND_CASE, FALSE);
                CheckDlgButton(HWindow, IDC_FIND_WHOLE, FALSE);
                CheckDlgButton(HWindow, IDC_FIND_REGULAR, FALSE);
                Data.HexMode = FALSE;
                Data.RegularExpresions = FALSE;
            }
            return TRUE;
        }

        switch (LOWORD(wParam))
        {
        case CM_FIND_INTERSECT:
        case CM_FIND_SUBTRACT:
        case CM_FIND_APPEND:
        {
            DWORD totalCount = ListView_GetItemCount(FoundFilesListView->HWindow);
            if (!SearchInProgress && totalCount > 0)
            {
                if (ValidateData() && TransferData(ttDataFromWindow))
                    StartSearch(LOWORD(wParam));
            }
            return 0;
        }

        case CM_FIND_NOW:
        case CM_FIND_DUPLICATES:
        {
            if (!SearchInProgress)
            {
                if (ValidateData() && TransferData(ttDataFromWindow))
                    StartSearch(LOWORD(wParam));
            }
            return 0;
        }

        case IDOK:
        {
            if (SearchInProgress) // is this a stop request?
            {
                if (Configuration.MinBeepWhenDone && GetForegroundWindow() != HWindow)
                    MessageBeep(0);
                StopSearch();
                return TRUE;
            }
            else // no, it is the start
            {
                if (!ValidateData() || !TransferData(ttDataFromWindow))
                    return TRUE;
                StartSearch(LOWORD(wParam));
                return TRUE;
            }
        }

        case IDCANCEL:
        {
            if (!CanCloseWindow())
                return TRUE;
            if (SearchInProgress)
            {
                if (!DoYouWantToStopSearching())
                    return TRUE;

                if (SearchInProgress)
                    StopSearch();

                if (ProcessingEscape)
                    return TRUE;
                else
                    break;
            }
            else
            {
                if (ProcessingEscape && Configuration.CnfrmCloseFind)
                {
                    BOOL dontShow = !Configuration.CnfrmCloseFind;

                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = HWindow;
                    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                    params.Caption = LoadStr(IDS_WANTTOSTOPTITLE);
                    params.Text = LoadStr(IDS_WANTTOCLOSEFIND);
                    params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAINCF);
                    params.CheckBoxValue = &dontShow;
                    int ret = SalMessageBoxEx(&params);
                    Configuration.CnfrmCloseFind = !dontShow;
                    if (ret != IDYES)
                        return 0;
                }
            }
            break;
        }

        case IDC_FIND_STOP:
        {
            if (SearchInProgress)
            {
                if (Configuration.MinBeepWhenDone && GetForegroundWindow() != HWindow)
                    MessageBeep(0);
                StopSearch();
                return TRUE;
            }
            break;
        }

        case IDC_FIND_HEX:
        {
            if (HIWORD(wParam) == BN_CLICKED)
            {
                Data.HexMode = (IsDlgButtonChecked(HWindow, IDC_FIND_HEX) != BST_UNCHECKED);
                if (Data.HexMode)
                    CheckDlgButton(HWindow, IDC_FIND_CASE, BST_CHECKED);
                return TRUE;
            }
            break;
        }

        case IDC_FIND_REGEXP_BROWSE:
        {
            const CExecuteItem* item = TrackExecuteMenu(HWindow, IDC_FIND_REGEXP_BROWSE,
                                                        IDC_FIND_CONTAINING, TRUE,
                                                        RegularExpressionItems);
            if (item != NULL)
            {
                BOOL regular = (IsDlgButtonChecked(HWindow, IDC_FIND_REGULAR) == BST_CHECKED);
                if (item->Keyword == EXECUTE_HELP)
                {
                    // open the help page dedicated to regular expressions
                    OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, IDH_REGEXP, FALSE);
                }
                if (item->Keyword != EXECUTE_HELP && !regular)
                {
                    // the user chose a pattern -> check the checkbox for regular search
                    CheckDlgButton(HWindow, IDC_FIND_REGULAR, BST_CHECKED);
                    PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDC_FIND_REGULAR, BN_CLICKED), 0);
                }
            }
            return 0;
        }

        case IDC_FIND_REGULAR:
        {
            if (HIWORD(wParam) == BN_CLICKED)
            {
                Data.RegularExpresions = (IsDlgButtonChecked(HWindow, IDC_FIND_REGULAR) != BST_UNCHECKED);
                if (Data.RegularExpresions)
                {
                    Data.HexMode = FALSE;
                    CheckDlgButton(HWindow, IDC_FIND_HEX, FALSE);
                }
                EnableControls();
                return TRUE;
            }
            break;
        }

            /*
        case IDC_FIND_INCLUDE_ARCHIVES:
        {
          if (HIWORD(wParam) == BN_CLICKED)
          {
            EnableControls();
            return TRUE;
          }
          break;
        }
*/

        case IDC_FIND_CONTAINING:
        {
            if (!Data.RegularExpresions && Data.HexMode && HIWORD(wParam) == CBN_EDITUPDATE)
            {
                DoHexValidation((HWND)lParam, GREP_TEXT_LEN);
                return TRUE;
            }
            break;
        }

        case IDC_FIND_ADVANCED:
        {
            CFilterCriteriaDialog dlg(HWindow, &Data.Criteria, TRUE);
            if (dlg.Execute() == IDOK)
                UpdateAdvancedText();
            return TRUE;
        }

        case CM_FIND_ADD_CURRENT:
        {
            CFindOptionsItem* item = new CFindOptionsItem();
            if (item != NULL)
            {
                TransferData(ttDataFromWindow);
                *item = Data;
                item->BuildItemName();
                if (!FindOptions.Add(item))
                    delete item;
            }
            else
                TRACE_E(LOW_MEMORY);

            return TRUE;
        }

        case CM_FIND_MANAGE:
        {
            if (FindManageInUse)
                return 0;
            FindManageInUse = TRUE;
            TransferData(ttDataFromWindow);
            CFindManageDialog dlg(HWindow, &Data);
            if (dlg.IsGood())
                dlg.Execute();
            FindManageInUse = FALSE;
            return 0;
        }

        case CM_FIND_IGNORE:
        {
            if (FindIgnoreInUse)
                return 0;
            FindIgnoreInUse = TRUE;
            TransferData(ttDataFromWindow);
            CFindIgnoreDialog dlg(HWindow, &FindIgnore);
            if (dlg.IsGood())
                dlg.Execute();
            FindIgnoreInUse = FALSE;
            return 0;
        }

        case IDC_FIND_LOOKIN_BROWSE:
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, IDC_FIND_LOOKIN_BROWSE), &r);
            POINT p;
            p.x = r.right;
            p.y = r.top;

            CMenuPopup menu;
            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
            mii.Type = MENU_TYPE_STRING;

            /* used by the export_mnu.py script which generates salmenu.mnu for the Translator
   keep synchronized with the InsertItem() call below...
MENU_TEMPLATE_ITEM FindLookInBrowseMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_FF_BROWSE
  {MNTT_IT, IDS_FF_LOCALDRIVES
  {MNTT_IT, IDS_FF_ALLDRIVES
  {MNTT_PE, 0
};
*/
            int ids[] = {IDS_FF_BROWSE, -1, IDS_FF_LOCALDRIVES, IDS_FF_ALLDRIVES, 0};
            int i;
            for (i = 0; ids[i] != 0; i++)
            {
                if (ids[i] == -1)
                    mii.Type = MENU_TYPE_SEPARATOR;
                else
                {
                    mii.Type = MENU_TYPE_STRING;
                    mii.String = LoadStr(ids[i]);
                    mii.ID = i + 1;
                }
                menu.InsertItem(-1, TRUE, &mii);
            }

            DWORD cmd = menu.Track(MENU_TRACK_VERTICAL | MENU_TRACK_RETURNCMD, p.x, p.y, HWindow, &r);

            if (cmd != 0)
            {
                if (cmd == 1)
                {
                    // Browse...
                    char path[MAX_PATH + 200];
                    char buff[1024];
                    DWORD start, end;
                    EditLine->GetSel(&start, &end);
                    SendMessage(EditLine->HWindow, WM_GETTEXT, (WPARAM)1024, (LPARAM)buff);
                    path[0] = 0;
                    if (start < end)
                        lstrcpyn(path, buff + start, end - start + 1);
                    if (GetTargetDirectory(HWindow, HWindow, LoadStr(IDS_CHANGE_DIRECTORY),
                                           LoadStr(IDS_BROWSECHANGEDIRTEXT), path, FALSE, path))
                    {
                        char* s = path;
                        while (*s != 0) // duplicate ';' characters (escape sequence for ';' is ';;')
                        {
                            if (*s == ';')
                            {
                                memmove(s + 1, s, strlen(s) + 1);
                                s++;
                            }
                            s++;
                        }

                        int leftIndex = -1;  // last character after which the text will be inserted
                        int rightIndex = -1; // first character after the inserted text
                        if (start > 0)
                            leftIndex = start - 1;
                        if (end < (DWORD)lstrlen(buff))
                            rightIndex = end;
                        if (leftIndex != -1)
                        {
                            s = buff + leftIndex;
                            while (s >= buff && *s == ';')
                                s--;
                            if ((((buff + leftIndex) - s) & 1) == 0)
                            {
                                memmove(path + 2, path, lstrlen(path) + 1);
                                path[0] = ';';
                                path[1] = ' ';
                            }
                        }
                        if (rightIndex != -1 && (buff[rightIndex] != ';' || buff[rightIndex + 1] == ';'))
                            lstrcat(path, "; ");

                        EditLine->ReplaceText(path);
                    }
                    return TRUE;
                }
                if (cmd == 3 || cmd == 4)
                    InsertDrives(EditLine->HWindow, cmd == 4); // local drives (3) || all drives (4)
            }
            return 0;
        }

        case CM_FIND_NAME:
        {
            FoundFilesListView->SortItems(0);
            return TRUE;
        }

        case CM_FIND_PATH:
        {
            FoundFilesListView->SortItems(1);
            return TRUE;
        }

        case CM_FIND_TIME:
        {
            FoundFilesListView->SortItems(3);
            return TRUE;
        }

        case CM_FIND_SIZE:
        {
            FoundFilesListView->SortItems(2);
            return TRUE;
        }

        case CM_FIND_OPEN:
        {
            OnOpen(TRUE);
            return TRUE;
        }

        case CM_FIND_OPENSEL:
        {
            OnOpen(FALSE);
            return TRUE;
        }

        case CM_FIND_FOCUS:
        {
            OnFocusFile();
            return TRUE;
        }

        case CM_FIND_VIEW:
        {
            OnViewFile(FALSE);
            return TRUE;
        }

        case CM_FIND_VIEW_WITH:
        {
            OnViewFileWith();
            return TRUE;
        }

        case CM_FIND_ALTVIEW:
        {
            OnViewFile(TRUE);
            return TRUE;
        }

        case CM_FIND_EDIT:
        {
            OnEditFile();
            return TRUE;
        }

        case CM_FIND_EDIT_WITH:
        {
            OnEditFileWith();
            return TRUE;
        }

        case CM_FIND_USERMENU:
        {
            OnUserMenu();
            return TRUE;
        }

        case CM_FIND_PROPERTIES:
        {
            OnProperties();
            return TRUE;
        }

        case CM_FIND_HIDESEL:
        {
            OnHideSelection();
            return TRUE;
        }

        case CM_FIND_HIDE_DUP:
        {
            OnHideDuplicateNames();
            return TRUE;
        }

        case CM_FIND_DELETE:
        {
            OnDelete((GetKeyState(VK_SHIFT) & 0x8000) == 0);
            return TRUE;
        }

        case CM_FIND_CLIPCUT:
        {
            OnCutOrCopy(TRUE);
            return TRUE;
        }

        case CM_FIND_CLIPCOPY:
        {
            OnCutOrCopy(FALSE);
            return TRUE;
        }

        case CM_FIND_CLIPCOPYFULLNAME:
        {
            OnCopyNameToClipboard(cntcmFullName);
            return TRUE;
        }

        case CM_FIND_CLIPCOPYNAME:
        {
            OnCopyNameToClipboard(cntcmName);
            return TRUE;
        }

        case CM_FIND_CLIPCOPYFULLPATH:
        {
            OnCopyNameToClipboard(cntcmFullPath);
            return TRUE;
        }

        case CM_FIND_CLIPCOPYUNCNAME:
        {
            OnCopyNameToClipboard(cntcmUNCName);
            return TRUE;
        }

        case CM_FIND_SELECTALL:
        {
            OnSelectAll();
            return TRUE;
        }

        case CM_FIND_INVERTSEL:
        {
            OnInvertSelection();
            return TRUE;
        }

        case CM_FIND_SHOWERRORS:
        {
            Configuration.ShowGrepErrors = !Configuration.ShowGrepErrors;
            return TRUE;
        }

        case CM_FIND_FULLROWSEL:
        {
            SetFullRowSelect(!Configuration.FindFullRowSelect);
            return TRUE;
        }

        case CM_FIND_MESSAGES:
        {
            if (TBHeader != NULL)
                TBHeader->StopFlashIcon();
            OnShowLog();
            return TRUE;
        }

        case CM_HELP_CONTENTS:
        case CM_HELP_INDEX:
        case CM_HELP_SEARCH:
        {
            CHtmlHelpCommand command;
            DWORD_PTR dwData = 0;
            switch (LOWORD(wParam))
            {
            case CM_HELP_INDEX:
            {
                command = HHCDisplayIndex;
                break;
            }

            case CM_HELP_SEARCH:
            {
                command = HHCDisplaySearch;
                break;
            }

            case CM_HELP_CONTENTS:
            {
                OpenHtmlHelp(NULL, HWindow, HHCDisplayTOC, 0, TRUE); // avoid two message boxes in a row
                command = HHCDisplayContext;
                dwData = IDD_FIND;
                break;
            }
            }

            OpenHtmlHelp(NULL, HWindow, command, dwData, FALSE);

            return 0;
        }
        }
        break;
    }

    case WM_INITMENUPOPUP:
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
    {
        if (wParam == IDC_FIND_STATUS)
        {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
            int prevBkMode = SetBkMode(di->hDC, TRANSPARENT);
            char buff[MAX_PATH + 50];
            SearchingText.Get(buff, MAX_PATH + 50);
            DrawText(di->hDC, buff, (int)strlen(buff), &di->rcItem, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS);
            SetBkMode(di->hDC, prevBkMode);
            return TRUE;
        }

        if (ContextMenu != NULL)
        {
            IContextMenu3* contextMenu3 = NULL;
            LRESULT lResult = 0;
            if (uMsg == WM_MENUCHAR)
            {
                if (SUCCEEDED(ContextMenu->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
                {
                    contextMenu3->HandleMenuMsg2(uMsg, wParam, lParam, &lResult);
                    contextMenu3->Release();
                    return (BOOL)lResult;
                }
            }
            if (ContextMenu->HandleMenuMsg(uMsg, wParam, lParam) == NOERROR)
            {
                if (uMsg == WM_INITMENUPOPUP) // ensure the return value is correct
                    return 0;
                else
                    return TRUE;
            }
        }
        break;
    }

    case WM_SYSCOMMAND:
    {
        if (SkipCharacter) // suppress the beep on Alt+Enter
        {
            SkipCharacter = FALSE;
            return TRUE; // MSDN says we should return 0, but that beeps, so I am not sure
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_FIND_RESULTS)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_DBLCLK:
            {
                if (((LPNMITEMACTIVATE)lParam)->iItem >= 0) // double-click outside items does nothing
                    OnOpen(TRUE);
                break;
            }

            case NM_RCLICK:
            {
                int clickedIndex = ((LPNMITEMACTIVATE)lParam)->iItem;
                if (clickedIndex >= 0) // right-click outside the item won't show the menu
                {
                    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                    // when clicking outside the selection while holding Shift (Alt+Ctrl doesn't matter) or
                    // holding only Alt, the selection changes to the clicked item before the menu is opened
                    HWND hListView = FoundFilesListView->HWindow;
                    if ((shiftPressed || altPressed && !controlPressed) &&
                        (ListView_GetItemState(hListView, clickedIndex, LVIS_SELECTED) & LVIS_SELECTED) == 0)
                    {
                        ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); // -1: all items
                        ListView_SetItemState(hListView, clickedIndex, LVIS_SELECTED | LVIS_FOCUSED, 0x000F);
                    }

                    DWORD pos = GetMessagePos();
                    OnContextMenu(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
                }
                break;
            }

            case NM_CUSTOMDRAW:
            {
                LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;

                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                {
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                    return TRUE;
                }

                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                {
                    // request notification of CDDS_ITEMPREPAINT | CDDS_SUBITEM
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
                    return TRUE;
                }

                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
                {
                    CFoundFilesData* item = FoundFilesListView->At((int)cd->nmcd.dwItemSpec);

                    // we'd like to draw the Path column ourselves (with ellipsis for paths)
                    if (cd->iSubItem == 1)
                    {
                        HDC hDC = cd->nmcd.hdc;

                        // if the cache DC does not exist yet, try to create it
                        if (CacheBitmap == NULL)
                        {
                            CacheBitmap = new CBitmap();
                            if (CacheBitmap != NULL)
                                CacheBitmap->CreateBmp(hDC, 1, 1);
                        }
                        if (CacheBitmap == NULL)
                            break; // out of memory; let the list view draw it; we're done

                        RECT r; // rectangle around the sub item
                        ListView_GetSubItemRect(FoundFilesListView->HWindow, cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, &r);
                        RECT r2; // rectangle same size as r but shifted to origin
                        r2.left = 0;
                        r2.top = 0;
                        r2.right = r.right - r.left;
                        r2.bottom = r.bottom - r.top;

                        // enlarge the cache bitmap if necessary
                        if (CacheBitmap->NeedEnlarge(r2.right, r2.bottom))
                            CacheBitmap->Enlarge(r2.right, r2.bottom);

                        // fill the background with the default color
                        int bkColor = (GrepData.FindDuplicates && item->Different == 1) ? COLOR_3DFACE : COLOR_WINDOW;
                        int textColor = COLOR_WINDOWTEXT;

                        if (Configuration.FindFullRowSelect)
                        {
                            if (ListView_GetItemState(FoundFilesListView->HWindow, cd->nmcd.dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED)
                            {
                                if (GetFocus() == FoundFilesListView->HWindow)
                                {
                                    bkColor = COLOR_HIGHLIGHT;
                                    textColor = COLOR_HIGHLIGHTTEXT;
                                }
                                else
                                {
                                    if (GetSysColor(COLOR_3DFACE) != GetSysColor(COLOR_WINDOW))
                                        bkColor = COLOR_3DFACE;
                                    else
                                    {
                                        // for high contrast color schemes
                                        bkColor = COLOR_HIGHLIGHT;
                                        textColor = COLOR_HIGHLIGHTTEXT;
                                    }
                                }
                            }
                        }

                        SetBkColor(CacheBitmap->HMemDC, GetSysColor(bkColor));
                        ExtTextOut(CacheBitmap->HMemDC, 0, 0, ETO_OPAQUE, &r2, "", 0, NULL);
                        SetBkMode(CacheBitmap->HMemDC, TRANSPARENT);

                        // draw the text with path shortening
                        r2.left += 5;
                        r2.right -= 5;
                        CFoundFilesData* item2 = FoundFilesListView->At((int)cd->nmcd.dwItemSpec);
                        SelectObject(CacheBitmap->HMemDC, (HFONT)SendMessage(FoundFilesListView->HWindow, WM_GETFONT, 0, 0));
                        int oldTextColor = SetTextColor(CacheBitmap->HMemDC, GetSysColor(textColor));

                        // DT_PATH_ELLIPSIS doesn't work on some strings and causing clipped text to be printed
                        // PathCompactPath() requires a copy in a local buffer but doesn't clip text
                        char buff[2 * MAX_PATH];
                        strncpy_s(buff, _countof(buff), item2->Path, _TRUNCATE);
                        PathCompactPath(CacheBitmap->HMemDC, buff, r2.right - r2.left);
                        DrawText(CacheBitmap->HMemDC, buff, -1, &r2,
                                 DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        //                DrawText(CacheBitmap->HMemDC, item2->Path, -1, &r2,
                        //                         DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_PATH_ELLIPSIS);
                        SetTextColor(CacheBitmap->HMemDC, oldTextColor);

                        // copy the cache to the list view
                        BitBlt(hDC, r.left, r.top, r.right - r.left, r.bottom - r.top,
                               CacheBitmap->HMemDC, 0, 0, SRCCOPY);

                        // disable default drawing
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
                        return TRUE;
                    }

                    if (GrepData.FindDuplicates && item->Different == 1)
                    {
                        cd->clrTextBk = GetSysColor(COLOR_3DFACE);
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_NEWFONT);
                        return TRUE;
                    }
                    break;
                }

                break;
            }

            case LVN_ODFINDITEM:
            {
                // assist the list view with quick search
                NMLVFINDITEM* pFindInfo = (NMLVFINDITEM*)lParam;
                int iStart = pFindInfo->iStart;
                LVFINDINFO* fi = &pFindInfo->lvfi;
                int ret = -1; // not found

                if (fi->flags & LVFI_STRING || fi->flags & LVFI_PARTIAL)
                {
                    //              BOOL partial = fi->flags & LVFI_PARTIAL != 0;
                    // the documentation says LVFI_PARTIAL and LVFI_STRING should arrive,
                    // but only LVFI_STRING comes through. Some guy complained about it
                    // on the newsgroups, but no reply. So we'll force it here.
                    BOOL partial = TRUE;
                    int i;
                    for (i = iStart; i < FoundFilesListView->GetCount(); i++)
                    {
                        const CFoundFilesData* item = FoundFilesListView->At(i);
                        if (partial)
                        {
                            if (StrNICmp(item->Name, fi->psz, (int)strlen(fi->psz)) == 0)
                            {
                                ret = i;
                                break;
                            }
                        }
                        else
                        {
                            if (StrICmp(item->Name, fi->psz) == 0)
                            {
                                ret = i;
                                break;
                            }
                        }
                    }
                    if (ret == -1 && fi->flags & LVFI_WRAP)
                    {
                        for (i = 0; i < iStart; i++)
                        {
                            const CFoundFilesData* item = FoundFilesListView->At(i);
                            if (partial)
                            {
                                if (StrNICmp(item->Name, fi->psz, (int)strlen(fi->psz)) == 0)
                                {
                                    ret = i;
                                    break;
                                }
                            }
                            else
                            {
                                if (StrICmp(item->Name, fi->psz) == 0)
                                {
                                    ret = i;
                                    break;
                                }
                            }
                        }
                    }
                }

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, ret);
                return TRUE;
            }

            case LVN_COLUMNCLICK:
            {
                int subItem = ((NM_LISTVIEW*)lParam)->iSubItem;
                if (subItem >= 0 && subItem < 5)
                    FoundFilesListView->SortItems(subItem);
                break;
            }

            case LVN_GETDISPINFO:
            {
                LV_DISPINFO* info = (LV_DISPINFO*)lParam;
                CFoundFilesData* item = FoundFilesListView->At(info->item.iItem);
                if (info->item.mask & LVIF_IMAGE)
                    info->item.iImage = item->IsDir ? 0 : 1;
                if (info->item.mask & LVIF_TEXT)
                    info->item.pszText = item->GetText(info->item.iSubItem, FoundFilesDataTextBuffer, FileNameFormat);
                break;
            }

            case LVN_ITEMCHANGED:
            {
                EnableToolBar();
                if (!IsSearchInProgress())
                    UpdateStatusBar = TRUE; // the text will be set during Idle time
                break;
            }

            case LVN_BEGINDRAG:
            case LVN_BEGINRDRAG:
            {
                OnDrag(((LPNMHDR)lParam)->code == LVN_BEGINRDRAG);
                return 0;
            }

            case LVN_KEYDOWN:
            {
                NMLVKEYDOWN* kd = (NMLVKEYDOWN*)lParam;
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                DWORD cmd = 0;
                switch (kd->wVKey)
                {
                case VK_F3:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_VIEW;
                    if (!controlPressed && altPressed && !shiftPressed)
                        cmd = CM_FIND_ALTVIEW;
                    if (controlPressed && !altPressed && shiftPressed)
                        cmd = CM_FIND_VIEW_WITH;
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_NAME;
                    break;
                }

                case VK_F4:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_EDIT;
                    if (controlPressed && !altPressed && shiftPressed)
                        cmd = CM_FIND_EDIT_WITH;
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_PATH;
                    break;
                }

                case VK_F5:
                {
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_TIME;
                    break;
                }

                case VK_F6:
                {
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_SIZE;
                    break;
                }

                case VK_DELETE:
                {
                    if (!controlPressed && !altPressed && !shiftPressed ||
                        !controlPressed && !altPressed && shiftPressed)
                        cmd = CM_FIND_DELETE;
                    break;
                }

                case VK_F8:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_DELETE;
                    break;
                }

                case VK_F9:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_USERMENU;
                    break;
                }

                case VK_RETURN:
                {
                    if (!controlPressed && !altPressed)
                        OnOpen(!shiftPressed);
                    if (!controlPressed && altPressed && !shiftPressed)
                    {
                        cmd = CM_FIND_PROPERTIES;
                        SkipCharacter = TRUE;
                    }
                    break;
                }

                case VK_SPACE:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_FOCUS;
                    break;
                }

                case VK_INSERT:
                {
                    if (!controlPressed && altPressed && !shiftPressed)
                        cmd = CM_FIND_CLIPCOPYFULLNAME;
                    if (!controlPressed && altPressed && shiftPressed)
                        cmd = CM_FIND_CLIPCOPYNAME;
                    if (controlPressed && altPressed && !shiftPressed)
                        cmd = CM_FIND_CLIPCOPYFULLPATH;
                    if (controlPressed && !altPressed && shiftPressed)
                        cmd = CM_FIND_CLIPCOPYUNCNAME;
                    break;
                }

                case 'A':
                {
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_SELECTALL;
                    break;
                }

                case 'C':
                {
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_CLIPCOPY;
                    break;
                }

                case 'X':
                {
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_CLIPCUT;
                    break;
                }

                case 'H':
                {
                    if (controlPressed && !altPressed && !shiftPressed)
                        cmd = CM_FIND_HIDESEL;
                    if (controlPressed && !altPressed && shiftPressed)
                        cmd = CM_FIND_HIDE_DUP;
                    break;
                }
                }
                if (cmd != 0)
                    PostMessage(HWindow, WM_COMMAND, cmd, 0);
                return 0;
            }
            }
        }
        break;
    }

    case WM_USER_ADDFILE:
    {
        UpdateListViewItems();
        return 0;
    }

    case WM_USER_ADDLOG:
    {
        // running in the find thread
        FIND_LOG_ITEM* item = (FIND_LOG_ITEM*)wParam;
        Log.Add(item->Flags, item->Text, item->Path);
        return 0;
    }

    case WM_USER_BUTTONS:
    {
        EnableControls(wParam != NULL);
        if (wParam != NULL)
            PostMessage((HWND)wParam, BM_SETSTYLE, BS_DEFPUSHBUTTON, MAKELPARAM(TRUE, 0));
        return 0;
    }

    case WM_USER_CFGCHANGED:
    {
        TBHeader->SetFont();
        return 0;
    }

    case WM_ACTIVATEAPP:
    {
        if (wParam == FALSE) // when deactivated we leave directories shown in panels
        {                    // so they can be deleted, dissconected, etc. by other software
            if (CanChangeDirectory())
                SetCurrentDirectoryToSystem();
        }
        else
        {
            SuppressToolTipOnCurrentMousePos(); // suppress unwanted tooltip when switching to the window
        }
        break;
    }

    case WM_ACTIVATE:
    {
        if (wParam != WA_INACTIVE)
        {
            if (FlashIconsOnActivation)
            {
                if (TBHeader != NULL)
                    TBHeader->StartFlashIcon();
                FlashIconsOnActivation = FALSE;
            }
        }
        break;
    }

    case WM_DESTROY:
    {
        if (SearchInProgress)
            StopSearch();

        if (!DlgFailed)
        {
            // store the width of the Name column
            Configuration.FindColNameWidth = ListView_GetColumnWidth(FoundFilesListView->HWindow, 0);
            // store the window placement
            Configuration.FindDialogWindowPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(HWindow, &Configuration.FindDialogWindowPlacement);
        }
        if (FoundFilesListView != NULL)
        {
            // release the handle, otherwise ListView would drag it to hell with itself
            ListView_SetImageList(FoundFilesListView->HWindow, NULL, LVSIL_SMALL);
        }
        if (MenuBar != NULL)
        {
            DestroyWindow(MenuBar->HWindow);
            delete MenuBar;
            MenuBar = NULL;
        }
        if (MainMenu != NULL)
        {
            delete MainMenu;
            MainMenu = NULL;
        }
        if (TBHeader != NULL)
        {
            DestroyWindow(TBHeader->HWindow);
            TBHeader = NULL;
        }
        if (EditLine->HWindow == NULL)
        {
            delete EditLine;
            EditLine = NULL;
        }

        FindDialogQueue.Remove(HWindow);

        // if the user copies the search results to the clipboard (Ctrl+C), switches to the main window
        // and invokes Paste Shortcut (Ctrl+S) while the shortcuts are being created, the Find window may close.
        // We must wait for Paste to finish in the main window; otherwise it could crash.
        //
        // If Paste Shortcut is used in Explorer (or elsewhere), we have no notification and a crash is still possible.
        //
        // If Ctrl+C is followed by closing the Find window before Paste,
        // we call OleFlushClipboard() within UninitializeOle(), which detaches the data from this thread and no problem occurs.
        // theoretically, we could call OleFlushClipboard() after every Ctrl+C directly here,
        // but we're not sure if something would stop working (not sure how robust the data rendering is),
        // plus OleFlushClipboard() can take a second with 2000 files.
        // Therefore we use this hack:
        while (PasteLinkIsRunning > 0)
        {
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (PasteLinkIsRunning > 0)
                Sleep(50); // active waiting; slow the thread down a bit
        }

        UninitializeOle();

        if (ZeroOnDestroy != NULL)
            *ZeroOnDestroy = NULL;
        PostQuitMessage(0);
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(GetDlgItem(HWindow, IDC_FIND_RESULTS), GetSysColor(COLOR_WINDOW));
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CFindDialogQueue
//

void CFindDialogQueue::AddToArray(TDirectArray<HWND>& arr)
{
    CS.Enter();
    CWindowQueueItem* item = Head;
    while (item != NULL)
    {
        arr.Add(item->HWindow);
        item = item->Next;
    }
    CS.Leave();
}
