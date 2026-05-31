// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "dialogs.h"
#include "mainwnd.h"
#include "plugins.h"
#include "geticon.h"
#include "logo.h"

// should be a multiple of IL_ITEMS_IN_ROW
// to fully use the space in the bitmap
#define ICONS_IN_LIST 100

//
// ****************************************************************************
// CIconCache
//

CIconCache::CIconCache()
    : TDirectArray<CIconData>(50, 30),
      IconsCache(10, 5),     // one item is a CIconList holding ICONS_IN_LIST icons
      ThumbnailsCache(1, 20) // obtaining thumbnails is slow, relocation is trivial by comparison
{
    IconsCount = 0;
    IconSize = ICONSIZE_COUNT; // not set yet; trying to add an icon without calling SetIconSize() first triggers TRACE_E
    DataIfaceForFS = NULL;
}

CIconCache::~CIconCache()
{
    Destroy();
}

inline int CompareDWORDS(const char* s1, const char* s2, int length)
{ // compares at most 'length' DWORDs
    //  int res;
    const char* end = s1 + length;
    while (s1 <= end)
    {
        //    if ((res = *(DWORD *)s1 - *(DWORD *)s2) != 0) return res;  // this does not work (try 0x8 and 0x0 as 4-bit numbers)
        if (*(DWORD*)s1 > *(DWORD*)s2)
            return 1;
        else
        {
            if (*(DWORD*)s1 < *(DWORD*)s2)
                return -1;
        }
        s1 += sizeof(DWORD);
        s2 += sizeof(DWORD);
    }
    return 0;
}

void CIconCache::SortArray(int left, int right, CPluginDataInterfaceEncapsulation* dataIface)
{
    if (dataIface != NULL) // this is pitFromPlugin: let the plugin compare the items itself (the comparison must not treat any two listing items as equal)
    {                      // with no two identical listing items)
        DataIfaceForFS = dataIface;
        BOOL ok = TRUE;
        int i;
        for (i = left; i <= right; i++) // one paranoid check
        {
            if (Data[i].GetFSFileData() == NULL)
            {
                TRACE_E("CIconCache::SortArray(): unexpected error: Icon Cache doesn't contain FSFileData for item: " << Data[i].NameAndData);
                ok = FALSE;
                break;
            }
        }
        if (ok)
            SortArrayForFSInt(left, right);
        DataIfaceForFS = NULL;
    }
    else // standard sorting by name
    {
        SortArrayInt(left, right);
    }
}

void CIconCache::SortArrayInt(int left, int right)
{

LABEL_SortArrayInt:

    int i = left, j = right;
    char* pivot = Data[(i + j) / 2].NameAndData;
    int length = (int)strlen(pivot);

    do
    {
        while (CompareDWORDS(Data[i].NameAndData, pivot, length) < 0 && i < right)
            i++;
        while (CompareDWORDS(pivot, Data[j].NameAndData, length) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CIconData swap = Data[i];
            Data[i] = Data[j];
            Data[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // The following "nice" code was replaced with code that saves substantially more stack
    // space (maximum recursion depth log(N)).
    //  if (left < j) SortArrayInt(left, j);
    //  if (i < right) SortArrayInt(i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // Both halves need to be sorted: recurse into the smaller one and process the other via goto.
            {
                SortArrayInt(left, j);
                left = i;
                goto LABEL_SortArrayInt;
            }
            else
            {
                SortArrayInt(i, right);
                right = j;
                goto LABEL_SortArrayInt;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortArrayInt;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortArrayInt;
        }
    }
}

void CIconCache::SortArrayForFSInt(int left, int right)
{

LABEL_SortArrayForFSInt:

    int i = left, j = right;
    const CFileData* pivot = Data[(i + j) / 2].FSFileData;

    do
    {
        while (DataIfaceForFS->CompareFilesFromFS(Data[i].FSFileData, pivot) < 0 && i < right)
            i++;
        while (DataIfaceForFS->CompareFilesFromFS(pivot, Data[j].FSFileData) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CIconData swap = Data[i];
            Data[i] = Data[j];
            Data[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // The following "nice" code was replaced with code that uses significantly less stack (maximum recursion depth log(N)).
    //  if (left < j) SortArrayForFSInt(left, j);
    //  if (i < right) SortArrayForFSInt(i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // Both halves need to be sorted, so recurse into the smaller one and process the other using goto.
            {
                SortArrayForFSInt(left, j);
                left = i;
                goto LABEL_SortArrayForFSInt;
            }
            else
            {
                SortArrayForFSInt(i, right);
                right = j;
                goto LABEL_SortArrayForFSInt;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortArrayForFSInt;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortArrayForFSInt;
        }
    }
}

BOOL CIconCache::GetIndex(const char* name, int& index, CPluginDataInterfaceEncapsulation* dataIface,
                          const CFileData* file)
{
    if (Count == 0 || dataIface != NULL && file == NULL) // verify that 'file' is valid
    {
        if (dataIface != NULL && file == NULL)
            TRACE_E("CIconCache::GetIndex(): 'file' may not be NULL when 'dataIface' is not NULL! item=" << name);
        index = 0;
        return FALSE;
    }

    if (dataIface != NULL) // this is pitFromPlugin: let the plugin compare the items itself (the comparison must not treat any two listing items as equal)
    {                      // with no two identical listing items)
        int l = 0, r = Count - 1, m;
        int res;
        while (1)
        {
            m = (l + r) / 2;
            const CFileData* fileM = At(m).GetFSFileData();
            if (fileM != NULL)
                res = dataIface->CompareFilesFromFS(fileM, file);
            else
            {
                TRACE_E("CIconCache::GetIndex(): unexpected error: Icon Cache doesn't contain FSFileData "
                        "for item: "
                        << At(m).NameAndData);
                index = 0;
                return FALSE; // error -> return as if not found: insert at the beginning of the array
            }
            if (res == 0) // found
            {
                index = m;
                return TRUE;
            }
            else if (res > 0)
            {
                if (l == r || l > m - 1) // not found
                {
                    index = m; // it should be at this position
                    return FALSE;
                }
                r = m - 1;
            }
            else
            {
                if (l == r) // not found
                {
                    index = m + 1; // it should be just after this position
                    return FALSE;
                }
                l = m + 1;
            }
        }
    }
    else // classic search by name
    {
        int length = (int)strlen(name);
        int l = 0, r = Count - 1, m;
        int res;
        while (1)
        {
            m = (l + r) / 2;
            res = CompareDWORDS(At(m).NameAndData, name, length);
            if (res == 0) // found
            {
                index = m;
                return TRUE;
            }
            else if (res > 0)
            {
                if (l == r || l > m - 1) // not found
                {
                    index = m; // it should be at this position
                    return FALSE;
                }
                r = m - 1;
            }
            else
            {
                if (l == r) // not found
                {
                    index = m + 1; // it should be just after this position
                    return FALSE;
                }
                l = m + 1;
            }
        }
    }
}

void CIconCache::Release()
{
    int i;
    for (i = 0; i < Count; i++)
    {
        CIconData* data = &At(i);
        if (data->NameAndData != NULL)
            free(data->NameAndData);
    }
    DestroyMembers();
    IconsCount = 0;

    // destroy the raw data from ThumbnailsCache
    for (i = 0; i < ThumbnailsCache.Count; i++)
    {
        CThumbnailData* data = &ThumbnailsCache[i];
        if (data->Bits != NULL)
            free(data->Bits); // allocated in CSalamanderThumbnailMaker::RenderToThumbnailData()
    }
    ThumbnailsCache.DestroyMembers();
}

void CIconCache::Destroy()
{
    // free the allocated data and the array itself
    Release();

    // destroy the image lists from IconsCache
    IconsCache.DestroyMembers();
}

void CIconCache::ColorsChanged()
{
    CALL_STACK_MESSAGE1("CIconCache::ColorsChanged()");
    // this function is called when the screen colors or color depth change
    // the second case is not handled; it would be necessary to reconstruct the bitmaps
    // in the image lists for the current color depth
    COLORREF bkColor = GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]);
    int i;
    for (i = 0; i < IconsCache.Count; i++)
    {
        CIconList* il = IconsCache[i];
        if (il != NULL)
            il->SetBkColor(bkColor);
    }

    // thumbnails need to be redrawn if the background color changed; icons with transparent
    // parts will be drawn onto the new background color
    for (i = 0; i < Count; i++)
    {
        CIconData* icon = &At(i);
        if (icon->GetFlag() == 5 /* valid thumbnail */)
            icon->SetFlag(6 /* old thumbnail version */);
    }
}

int CIconCache::AllocIcon(CIconList** iconList, int* iconListIndex)
{
    SLOW_CALL_STACK_MESSAGE1("CIconCache::AllocIcon()");
    int cache = IconsCount / ICONS_IN_LIST; // cache
    int index = IconsCount % ICONS_IN_LIST; // index within this cache
    if (cache >= IconsCache.Count)
    {
        if (cache > IconsCache.Count)
        {
            TRACE_E("Unexpected situation in CIconCache::AllocIcon.");
            return -1;
        }

        int iconWidth = 16;
        if (IconSize == ICONSIZE_COUNT)
            TRACE_E("CIconCache::AllocIcon() IconSize == ICONSIZE_COUNT, you must call SetIconSize() first!");
        else
            iconWidth = IconSizes[IconSize];

        CIconList* il = new CIconList();
        if (il == NULL)
        {
            TRACE_E("Unable to create icon-list cache of icons.");
            return -1;
        }
        if (!il->Create(iconWidth, iconWidth, ICONS_IN_LIST))
        {
            TRACE_E("Unable to create icon-list cache of icons.");
            delete il;
            return -1;
        }
        il->SetBkColor(GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));

        IconsCache.Add(il);
        if (!IconsCache.IsGood())
        {
            delete il;
            IconsCache.ResetState();
            return -1;
        }
        if (iconList != NULL)
            *iconList = il;
    }
    else
    {
        if (iconList != NULL)
            *iconList = IconsCache[cache];
    }
    if (iconListIndex != NULL)
        *iconListIndex = index;
    return IconsCount++;
}

int CIconCache::AllocThumbnail()
{
    CALL_STACK_MESSAGE1("CIconCache::AllocThumbnail()");

    // structure for holding thumbnails
    CThumbnailData data;
    memset(&data, 0, sizeof(CThumbnailData));

    // add it to the list
    int index = ThumbnailsCache.Add(data);
    if (!ThumbnailsCache.IsGood())
    {
        ThumbnailsCache.ResetState();
        return -1;
    }

    // returns the index of the added item
    return index;
}

BOOL CIconCache::GetThumbnail(int index, CThumbnailData** thumbnailData)
{
    CALL_STACK_MESSAGE2("CIconCache::GetThumbnail(%d, , )", index);
    if (index >= 0 && index < ThumbnailsCache.Count)
    {
        *thumbnailData = &ThumbnailsCache[index];
        return TRUE;
    }
    else
    {
        /*if (echo) */ TRACE_E("Incorrect call to CIconCache::GetThumbnail.");
        return FALSE;
    }
}

BOOL CIconCache::GetIcon(int iconIndex, CIconList** iconList, int* iconListIndex)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CIconCache::GetIcon(%d, , )", iconIndex);
    if (iconIndex >= 0 && iconIndex < IconsCount)
    {
        int cache = iconIndex / ICONS_IN_LIST; // cache
        int index = iconIndex % ICONS_IN_LIST; // index within the cache
        if (cache < IconsCache.Count)
        {
            *iconList = IconsCache[cache];
            *iconListIndex = index;
            return TRUE;
        }
        else
        {
            /*if (echo) */ TRACE_E("Unexpected situation in CIconCache::GetIcon.");
            return FALSE;
        }
    }
    else
    {
        /*if (echo) */ TRACE_E("Incorrect call to CIconCache::GetIcon.");
        return FALSE;
    }
}

void CIconCache::GetIconsAndThumbsFrom(CIconCache* icons, CPluginDataInterfaceEncapsulation* dataIface,
                                       BOOL transferIconsAndThumbnailsAsNew, BOOL forceReloadThumbnails)
{
    CALL_STACK_MESSAGE1("CIconCache::GetIconsAndThumbsFrom()");
    int index1 = 0;
    int index2 = 0;

    if (dataIface != NULL) // This is pitFromPlugin: let the plugin compare the items itself.
    {                      // with no two identical listing items)
        const CFileData *file1, *file2;

        if (index1 < Count)
        {
            file1 = At(index1).GetFSFileData();
            if (file1 == NULL)
            {
                TRACE_E("CIconCache::GetIconsAndThumbsFrom(): unexpected error: Icon Cache doesn't contain FSFileData "
                        "for item: "
                        << At(index1).NameAndData);
                return;
            }
        }
        else
            return; // nothing to merge

        if (index2 < icons->Count)
        {
            file2 = icons->At(index2).GetFSFileData();
            if (file2 == NULL)
            {
                TRACE_E("CIconCache::GetIconsAndThumbsFrom(): unexpected error: Icon Cache doesn't contain FSFileData "
                        "for item: "
                        << At(index2).NameAndData);
                return;
            }
        }
        else
            return; // nothing to merge
        int res;
        while (1)
        {
            res = dataIface->CompareFilesFromFS(file1, file2);
            if (res == 0) // identical -> copy them (icons and masks)
            {
                CIconList* srcIconList;
                int srcIconListIndex;

                CIconList* dstIconList;
                int dstIconListIndex;

                DWORD flag = icons->At(index2).GetFlag();

                if ((flag == 1 || flag == 2) &&  // valid or old icon
                    At(index1).GetFlag() == 0 && // we care about the icon (if it switched to a thumbnail, the old icon is no longer needed)
                    GetIcon(At(index1).GetIndex(), &dstIconList, &dstIconListIndex) &&
                    icons->GetIcon(icons->At(index2).GetIndex(), &srcIconList, &srcIconListIndex))
                {
                    dstIconList->Copy(dstIconListIndex, srcIconList, srcIconListIndex);
                    At(index1).SetFlag((flag == 1 && transferIconsAndThumbnailsAsNew) ? 1 : 2); // now it is the old (or valid/new) icon version
                }
            }

            if (res == 0 || res < 0) // index1++
            {
                if (++index1 < Count)
                {
                    file1 = At(index1).GetFSFileData();
                    if (file1 == NULL)
                    {
                        TRACE_E("CIconCache::GetIconsAndThumbsFrom(): unexpected error: Icon Cache doesn't contain FSFileData "
                                "for item: "
                                << At(index1).NameAndData);
                        return;
                    }
                }
                else
                    break; // nothing left to merge
            }

            if (res == 0 || res > 0) // index2++
            {
                if (++index2 < icons->Count)
                {
                    file2 = icons->At(index2).GetFSFileData();
                    if (file2 == NULL)
                    {
                        TRACE_E("CIconCache::GetIconsAndThumbsFrom(): unexpected error: Icon Cache doesn't contain FSFileData "
                                "for item: "
                                << At(index2).NameAndData);
                        return;
                    }
                }
                else
                    break; // nothing left to merge
            }
        }
    }
    else
    {
        int length;
        char *name1, *name2;

        if (index1 < Count)
        {
            name1 = At(index1).NameAndData;
            length = (int)strlen(name1);
        }
        else
            return; // nothing to merge

        if (index2 < icons->Count)
            name2 = icons->At(index2).NameAndData;
        else
            return; // nothing to merge
        int res;
        while (1)
        {
            res = CompareDWORDS(name1, name2, length);
            if (res == 0) // match -> copy the icons and masks, or the thumbnail
            {
                CIconList* srcIconList;
                int srcIconListIndex;

                CIconList* dstIconList;
                int dstIconListIndex;

                DWORD flag = icons->At(index2).GetFlag();

                if ((flag == 1 || flag == 2) &&  // valid or old icon
                    At(index1).GetFlag() == 0 && // we care about the icon (if it switched to a thumbnail, the old icon is no longer needed)
                    GetIcon(At(index1).GetIndex(), &dstIconList, &dstIconListIndex) &&
                    icons->GetIcon(icons->At(index2).GetIndex(), &srcIconList, &srcIconListIndex))
                {
                    dstIconList->Copy(dstIconListIndex, srcIconList, srcIconListIndex);
                    At(index1).SetFlag((flag == 1 && transferIconsAndThumbnailsAsNew) ? 1 : 2); // now it is the old (or valid/new) icon version
                }
                else
                {
                    CThumbnailData* srcThumbnailData;
                    CThumbnailData* tgtThumbnailData;

                    if ((flag == 5 || flag == 6) &&  // valid or old thumbnail
                        At(index1).GetFlag() == 4 && // we care about the thumbnail (if it switched to an icon, the old thumbnail is no longer needed)
                        GetThumbnail(At(index1).GetIndex(), &tgtThumbnailData) &&
                        icons->GetThumbnail(icons->At(index2).GetIndex(), &srcThumbnailData))
                    {
                        // the old thumbnail does not need to be copied; it is enough to pass its
                        // geometry and raw data to the target thumbnail
                        *tgtThumbnailData = *srcThumbnailData; // pass the old data to the new cache
                        srcThumbnailData->Bits = NULL;         // the old cache is going away and must not deallocate the data

                        int newFlag = 6;
                        // if we are copying a valid thumbnail, check the file stamp (size+date); if appropriate,
                        // mark the copied thumbnail as valid immediately (the risk of the file changing without
                        // changing size+date is negligible, while the speed gain is huge)
                        if (flag == 5 && !forceReloadThumbnails)
                        {
                            if (transferIconsAndThumbnailsAsNew)
                                newFlag = 5;
                            else
                            {
                                int offset = length + 4;
                                offset -= (offset & 0x3); // offset % 4  (4-byte alignment)
                                if (*(CQuadWord*)(name1 + offset) == *(CQuadWord*)(name2 + offset) &&
                                    CompareFileTime((FILETIME*)(name1 + offset + sizeof(CQuadWord)),
                                                    (FILETIME*)(name2 + offset + sizeof(CQuadWord))) == 0)
                                {
                                    newFlag = 5;
                                }
                            }
                        }
                        At(index1).SetFlag(newFlag); // now it is the old (or valid/new) thumbnail version
                    }
                }
            }

            if (res == 0 || res < 0) // index1++
            {
                if (++index1 < Count)
                {
                    name1 = At(index1).NameAndData;
                    length = (int)strlen(name1);
                }
                else
                    break; // nothing left to merge
            }

            if (res == 0 || res > 0) // index2++
            {
                if (++index2 < icons->Count)
                    name2 = icons->At(index2).NameAndData;
                else
                    break; // nothing left to merge
            }
        }
    }
}

void CIconCache::SetIconSize(CIconSizeEnum iconSize)
{
    if (iconSize == ICONSIZE_COUNT)
    {
        TRACE_E("CIconCache::SetIconSize() unexpected iconSize==ICONSIZE_COUNT");
        return;
    }
    if (iconSize == IconSize) // if the size is not changing, there is nothing to do
        return;

    // discard the current icons
    int i;
    for (i = 0; i < Count; i++)
    {
        CIconData* data = &At(i);
        data->SetFlag(0);
        data->SetIndex(-1);
    }
    IconsCache.DestroyMembers();
    IconsCount = 0;

    IconSize = iconSize;
}

//
// ****************************************************************************
// CAssociations
//

BOOL ReadDirectoryIconAndTypeAux(CIconList* iconList, int index, CIconSizeEnum iconSize)
{
    char systemDir[MAX_PATH];
    GetSystemDirectory(systemDir, MAX_PATH);
    SHFILEINFO shi;
    HICON hIcon;
    __try
    {
        if (GetFileIcon(systemDir, FALSE, &hIcon, iconSize, TRUE, TRUE))
        {
            iconList->ReplaceIcon(index, hIcon);
            NOHANDLES(DestroyIcon(hIcon));
        }
        if (SHGetFileInfo(systemDir, 0, &shi, sizeof(shi), SHGFI_TYPENAME) != 0)
        {
            lstrcpyn(FolderTypeName, shi.szTypeName, sizeof(FolderTypeName));
            FolderTypeNameLen = (int)strlen(FolderTypeName);
        }
        return TRUE;
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 10))
    {
        FGIExceptionHasOccured++;
    }
    return FALSE;
}

BOOL GetIconFromAssocAux(BOOL initFlagAndIndexes, HKEY root, const char* keyName, LONG size,
                         CAssociationData& data, char* iconLocation, char* type)
{
    BOOL found = FALSE;
    if (initFlagAndIndexes)
    {
        data.SetFlag(0);
        data.SetIndexAll(-1);
    }
    iconLocation[0] = 0;
    char keyNameBuf[MAX_PATH];
    lstrcpyn(keyNameBuf, keyName, min(size, MAX_PATH));
    HKEY openKey;

    if (type != NULL)
    {
        type[0] = 0;

        // get the file-type string as the value "" of the keyName subkey
        if (HANDLES_Q(RegOpenKey(root, keyNameBuf, &openKey)) == ERROR_SUCCESS)
        {
            LONG typeSize = MAX_PATH;
            if (SalRegQueryValue(openKey, "", type, &typeSize) != ERROR_SUCCESS)
                type[0] = 0;
            HANDLES(RegCloseKey(openKey));
        }
    }

    if (size - 1 + 7 <= MAX_PATH) // test whether opening via associations is possible
    {
        memmove(keyNameBuf + size - 1, "\\Shell", 7);
        if (HANDLES_Q(RegOpenKey(root, keyNameBuf, &openKey)) == ERROR_SUCCESS)
        { // if "\\shell" contains any subkey, it can be opened (association for Enter)
            DWORD keys;
            if (RegQueryInfoKey(openKey, NULL, NULL, NULL, &keys, NULL,
                                NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            {
                if (keys > 0)
                    data.SetFlag(1);
            }
            HANDLES(RegCloseKey(openKey));
        }
    }

    if (size - 1 + 21 <= MAX_PATH)
    {
        memmove(keyNameBuf + size - 1, "\\ShellEx\\IconHandler", 21);
        // if "\\ShellEx\\IconHandler" is present, the icon must be loaded from the file
        if (HANDLES_Q(RegOpenKey(root, keyNameBuf, &openKey)) == ERROR_SUCCESS)
        {
            found = TRUE;

            HANDLES(RegCloseKey(openKey));
            data.SetIndexAll(-2);
        }
    }

    if (!found && size - 1 + 13 <= MAX_PATH)
    {
        memmove(keyNameBuf + size - 1, "\\DefaultIcon", 13);
        if (HANDLES_Q(RegOpenKey(root, keyNameBuf, &openKey)) == ERROR_SUCCESS)
        {
            char buf[MAX_PATH];
            size = MAX_PATH; // get the path to the icon
            DWORD type2 = REG_SZ;
            DWORD err = SalRegQueryValueEx(openKey, "", 0, &type2,
                                           (LPBYTE)buf, (LPDWORD)&size);
            if (err == ERROR_SUCCESS && size > 1)
            {
                found = TRUE;

                if (type2 == REG_EXPAND_SZ)
                {
                    DWORD auxRes = ExpandEnvironmentStrings(buf, iconLocation, MAX_PATH + 10);
                    if (auxRes == 0 || auxRes > MAX_PATH + 10)
                    {
                        TRACE_E("ExpandEnvironmentStrings failed.");
                        strcpy(iconLocation, buf);
                    }
                }
                else
                    strcpy(iconLocation, buf);

                // remove quotes in the case "\"file_name\",icon_number" (for example "\"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe\",0")
                char* num = strrchr(iconLocation, ',');
                if (num != NULL)
                {
                    char* numEnd = num;
                    while (*(numEnd + 1) == ' ')
                        numEnd++;
                    if (*(numEnd + 1) == '-')
                        numEnd++;
                    if (*(numEnd + 1) == '+')
                        numEnd++;
                    char* numBeg = numEnd + 1;
                    while (*++numEnd >= '0' && *numEnd <= '9')
                        ;
                    if (numBeg < numEnd && *numEnd == 0 &&                                   // the icon number is after the last comma
                        *iconLocation == '"' && num - 1 > iconLocation && *(num - 1) == '"') // there are quotes at the beginning and before the comma
                    {                                                                        // remove the quotes
                        memmove(iconLocation, iconLocation + 1, (num - 1) - (iconLocation + 1));
                        memmove(num - 2, num, numEnd - num + 1);
                    }
                }

                char* s = buf; // distinguish "%1" from "...%variable%..."
                while (*s != 0)
                {
                    if (*s == '%')
                    {
                        s++;
                        if (*s != '%')
                        {
                            while (*s != 0 && *s != ' ' && *s != '%')
                                s++;
                            if (*s != '%') // not an environment variable -> dynamic type
                            {
                                data.SetIndexAll(-2);
                                break;
                            }
                        }
                    }
                    s++;
                }
            }
            HANDLES(RegCloseKey(openKey));
        }
    }
    return found;
}

CAssociations::CAssociations()
    : TDirectArray<CAssociationData>(500, 300)
{
}

CAssociations::~CAssociations()
{
    Destroy();
}

void CAssociations::Release()
{
    int i;
    for (i = 0; i < Count; i++)
    {
        CAssociationData* data = &At(i);
        if (data->ExtensionAndData != NULL)
            free(data->ExtensionAndData);
        if (data->Type != NULL)
            free(data->Type);
    }
    DestroyMembers();
    for (i = 0; i < ICONSIZE_COUNT; i++)
        Icons[i].IconsCount = 0;
}

void CAssociations::Destroy()
{
    // free the allocated data and the array itself
    Release();

    // destroy the image lists from IconsCache
    int i;
    for (i = 0; i < ICONSIZE_COUNT; i++)
        Icons[i].IconsCache.DestroyMembers();
}

void CAssociations::ColorsChanged()
{
    CALL_STACK_MESSAGE1("CAssociations::ColorsChanged()");
    // this function is called when the screen colors or color depth change
    // the second case is not handled; it would be necessary to reconstruct the bitmaps
    // in the image lists for the current color depth
    COLORREF bkColor = GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]);
    int j;
    for (j = 0; j < ICONSIZE_COUNT; j++)
    {
        int i;
        for (i = 0; i < Icons[j].IconsCache.Count; i++)
        {
            CIconList* il = Icons[j].IconsCache[i];
            if (il != NULL)
                il->SetBkColor(bkColor);
        }
    }
    // FIXME: it would be enough to set the background only for the relevant icon list
    int i;
    for (i = 0; i < ICONSIZE_COUNT; i++)
        SimpleIconLists[i]->SetBkColor(bkColor);
}

BOOL CAssociations::GetIndex(const char* name, int& index)
{
    if (Count == 0)
    {
        index = 0;
        return FALSE;
    }

    int length = (int)strlen(name);
    int l = 0, r = Count - 1, m;
    int res;
    while (1)
    {
        m = (l + r) / 2;
        res = CompareDWORDS(At(m).ExtensionAndData, name, length);
        if (res == 0) // found
        {
            index = m;
            return TRUE;
        }
        else if (res > 0)
        {
            if (l == r || l > m - 1) // not found
            {
                index = m; // it should be at this position
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // not found
            {
                index = m + 1; // it should be just after this position
                return FALSE;
            }
            l = m + 1;
        }
    }
}

int CAssociations::AllocIcon(CIconList** iconList, int* iconListIndex, CIconSizeEnum iconSize)
{
    CALL_STACK_MESSAGE1("CAssociations::AllocIcon()");
    int cache = Icons[iconSize].IconsCount / ICONS_IN_LIST; // cache
    int index = Icons[iconSize].IconsCount % ICONS_IN_LIST; // index within this cache
    if (cache >= Icons[iconSize].IconsCache.Count)
    {
        if (cache > Icons[iconSize].IconsCache.Count)
        {
            TRACE_E("Unexpected situation in CAssociations::AllocIcon.");
            return -1;
        }

        int iconWidth = IconSizes[iconSize];

        CIconList* il = new CIconList();
        if (il == NULL)
        {
            TRACE_E("Unable to create icon-list cache of icons.");
            return -1;
        }
        if (!il->Create(iconWidth, iconWidth, ICONS_IN_LIST))
        {
            TRACE_E("Unable to create icon-list cache of icons.");
            delete il;
            return -1;
        }
        il->SetBkColor(GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));

        Icons[iconSize].IconsCache.Add(il);
        if (!Icons[iconSize].IconsCache.IsGood())
        {
            delete il;
            Icons[iconSize].IconsCache.ResetState();
            return -1;
        }
        if (iconList != NULL)
            *iconList = il;
    }
    else
    {
        if (iconList != NULL)
            *iconList = Icons[iconSize].IconsCache[cache];
    }
    if (iconListIndex != NULL)
        *iconListIndex = index;
    return Icons[iconSize].IconsCount++;
}

BOOL CAssociations::GetIcon(int iconIndex, CIconList** iconList, int* iconListIndex, CIconSizeEnum iconSize)
{
    CALL_STACK_MESSAGE2("CAssociations::GetIcon(%d, , )", iconIndex);
    if (iconIndex >= 0 && iconIndex < Icons[iconSize].IconsCount)
    {
        int cache = iconIndex / ICONS_IN_LIST; // cache
        int index = iconIndex % ICONS_IN_LIST; // index within the cache
        if (cache < Icons[iconSize].IconsCache.Count)
        {
            *iconList = Icons[iconSize].IconsCache[cache];
            *iconListIndex = index;
            return TRUE;
        }
        else
        {
            TRACE_E("Unexpected situation in CAssociations::GetIcon.");
            return FALSE;
        }
    }
    else
    {
        TRACE_E("Incorrect call to CAssociations::GetIcon.");
        return FALSE;
    }
}

void CAssociations::SortArray(int left, int right)
{

LABEL_SortArray:

    int i = left, j = right;
    char* pivot = Data[(i + j) / 2].ExtensionAndData;
    int length = (int)strlen(pivot);

    do
    {
        while (CompareDWORDS(Data[i].ExtensionAndData, pivot, length) < 0 && i < right)
            i++;
        while (CompareDWORDS(pivot, Data[j].ExtensionAndData, length) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CAssociationData swap = Data[i];
            Data[i] = Data[j];
            Data[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // We replaced the following "nice" code with code that uses substantially less stack space
    //  // (maximum recursion depth of log(N)).
    //  if (left < j) SortArray(left, j);
    //  if (i < right) SortArray(i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // Both "halves" must be sorted, so recurse into the smaller one and process the other via "goto"
            {
                SortArray(left, j);
                left = i;
                goto LABEL_SortArray;
            }
            else
            {
                SortArray(i, right);
                right = j;
                goto LABEL_SortArray;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortArray;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortArray;
        }
    }
}

void CAssociations::InsertData(const char* /*origin*/, int index, BOOL overwriteItem, char* e, char* s, CAssociationData& data, LONG& size,
                               const char* iconLocation, const char* type)
{
    //  TRACE_I(origin << (overwriteItem ? "overwriting existing record by: " : "") << "file association: ext=" << e <<
    //          ": index=" << data.GetIndex(ICONSIZE_16) << ": flag=" << data.GetFlag() << ": icon-location=" << iconLocation <<
    //          ": type=" << (type == NULL ? "" : type));

    size = (LONG)(s - e) + 4;
    size -= (size & 0x3); // size % 4 (alignment to four bytes)
    int iLen = (int)strlen(iconLocation) + 1;
    data.ExtensionAndData = (char*)malloc(size + iLen);
    memcpy(data.ExtensionAndData, e, size);                   // extension + null padding +
    memcpy(data.ExtensionAndData + size, iconLocation, iLen); // icon-location
    if (type[0] != 0)
        data.Type = DupStr(type); // error -> only the file type stays hidden
    else
        data.Type = NULL;
    if (overwriteItem)
    {
        if (At(index).ExtensionAndData != NULL)
            free(At(index).ExtensionAndData);
        if (At(index).Type != NULL)
            free(At(index).Type);
        At(index) = data;
    }
    else
        Insert(index, data);
}

void CAssociations::ReadAssociations(BOOL showWaitWnd)
{
    //---  bring up the wait dialog and hourglass
    HCURSOR oldCur;
    HWND parent = (MainWindow != NULL) ? MainWindow->HWindow : NULL;
    // The wait window was problematic:
    // if I use Open With on a file and choose, for example, NOTEPAD, it opens,
    // then SHCNE_ASSOCCHANGED notifications are broadcast for the association change,
    // which calls this function, shows the wait window, and pulls it to the front,
    // taking the whole Salamander window with it, so I disable it temporarily
    CWaitWindow waitWnd(parent, IDS_READINGASSOCIATIONS, FALSE, ooStatic);
    BOOL closeDialog = FALSE;
    if (!ExistSplashScreen())
    {
        if (showWaitWnd)
            waitWnd.Create(); // j.r. for debugging desktop shortcuts
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        closeDialog = TRUE;
    }
    else
        IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_ASSOCIATIONS));
    //---  clear the array and cache
    Release();
    //---  iterate through the registry records for classes (extensions)
    char ext[MAX_PATH + 4];
    char extType[MAX_PATH];
    char *s, *e;

    char iconLocation[MAX_PATH + 10];
    char type[MAX_PATH];
    HKEY extKey, openKey;
    LONG size;
    CAssociationData data;

    char errBuf[200 + MAX_PATH];

    HKEY systemFileAssoc = NULL;
    if (HANDLES_Q(RegOpenKey(HKEY_CLASSES_ROOT, "SystemFileAssociations", &systemFileAssoc)) != ERROR_SUCCESS)
    {
        systemFileAssoc = NULL;
    }

    // Windows 2000 and later also store "Open With..." associations separately for each user
    // in HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts
    HKEY explorerFileExts;
    if (HANDLES_Q(RegOpenKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts",
                             &explorerFileExts)) != ERROR_SUCCESS)
    {
        explorerFileExts = NULL;
    }

    DWORD i = 0;
    LONG enumRet;
    FILETIME ft;
    while (1)
    { // enumerate all extensions one by one
        DWORD extS = MAX_PATH;
        if ((enumRet = RegEnumKeyEx(HKEY_CLASSES_ROOT, i, ext, &extS, NULL, NULL, NULL, &ft)) == ERROR_SUCCESS)
        { // open the extension key
            if (ext[0] == '.' && HANDLES_Q(RegOpenKey(HKEY_CLASSES_ROOT, ext, &extKey)) == ERROR_SUCCESS)
            {
                size = MAX_PATH; // get the association type
                iconLocation[0] = 0;
                data.SetFlag(0);
                data.SetIndexAll(-1);
                type[0] = 0;
                BOOL tryPerceivedType = FALSE;
                BOOL addExt = (SalRegQueryValue(extKey, "", extType, &size) == ERROR_SUCCESS && size > 1);
                if (addExt)
                {
                    // test the icon type (static/dynamic, see .h)
                    tryPerceivedType = !GetIconFromAssocAux(FALSE, HKEY_CLASSES_ROOT, extType, size, data, iconLocation, type);
                }
                else
                    tryPerceivedType = TRUE;
                if (tryPerceivedType && systemFileAssoc != NULL)
                {
                    // first try to find 'ext' under the SystemFileAssociations key
                    if (GetIconFromAssocAux(FALSE, systemFileAssoc, ext, (LONG)strlen(ext) + 1, data, iconLocation, NULL))
                        addExt = TRUE;
                    else
                    { // also try the key from the PerceivedType value, if it is defined
                        size = MAX_PATH;
                        if (SalRegQueryValueEx(extKey, "PerceivedType", NULL, NULL, (BYTE*)extType, (DWORD*)&size) == ERROR_SUCCESS && size > 1)
                        {
                            extType[MAX_PATH - 1] = 0; // just in case (the value may not be a string, so the null terminator may be missing)
                            if (GetIconFromAssocAux(FALSE, systemFileAssoc, extType, (LONG)strlen(extType) + 1, data, iconLocation, NULL))
                                addExt = TRUE;
                        }
                    }
                }
                if (addExt)
                {                // convert the extension to lowercase and add it to the array
                    e = ext + 1; // skip '.'
                    s = e;
                    while (*s != 0)
                    {
                        *s = LowerCase[*s];
                        s++;
                    }
                    *(DWORD*)s = 0; // zero the end of the string

                    InsertData("", Count, FALSE, e, s, data, size, iconLocation, type);
                }
                HANDLES(RegCloseKey(extKey));
            }
        }
        else
        {
            if (enumRet != ERROR_NO_MORE_ITEMS)
            {
                // For one reported user, this returns
                // one ERROR_MORE_DATA and then many ERROR_OUTOFMEMORY errors on Microsoft Windows Server 2003,
                // Standard Edition Service Pack 2 (Build 3790). Increasing the buffer to 10000 does not help;
                // enumeration must be stopped at the first error, otherwise it starts looping and eating memory
                // (apparently an internal Windows bug). Nobody else reported it, so we do not investigate further.
                _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_UNABLETOGETASSOC), GetErrorText(enumRet));
                SalMessageBox(parent, errBuf, LoadStr(IDS_UNABLETOGETASSOCTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            break; // end of enumeration
        }
        i++;
    }
    if (Count > 1)
        SortArray(0, Count - 1);

    // Windows XP also stores associations (see PerceivedType) under HKEY_CLASSES_ROOT\SystemFileAssociations,
    // so load any extensions that are still unknown from this key as well
    if (systemFileAssoc != NULL)
    {
        i = 0;
        while (1)
        { // enumerate all extensions one by one
            DWORD extS = MAX_PATH;
            if ((enumRet = RegEnumKeyEx(systemFileAssoc, i, ext, &extS, NULL, NULL, NULL, &ft)) == ERROR_SUCCESS)
            { // open the extension key
                if (ext[0] == '.')
                {
                    e = ext + 1; // skip '.'
                    s = ext;
                    while (*++s != 0)
                        *s = LowerCase[*s];
                    *(DWORD*)s = 0; // zero the end of the string

                    int index;
                    if (!GetIndex(e, index)) // not found; it makes sense to check and add it if needed
                    {
                        if (GetIconFromAssocAux(TRUE, systemFileAssoc, ext, (LONG)strlen(ext) + 1, data, iconLocation, NULL))
                        {
                            InsertData("SystemFileAssociations: ", index, FALSE, e, s, data, size, iconLocation, "");
                        }
                    }
                }
            }
            else
            {
                if (enumRet != ERROR_NO_MORE_ITEMS)
                {
                    // For one reported user, this returns
                    // one ERROR_MORE_DATA and then many ERROR_OUTOFMEMORY errors on Microsoft Windows Server 2003,
                    // Standard Edition Service Pack 2 (Build 3790). Increasing the buffer to 10000 does not help;
                    // enumeration must be stopped at the first error, otherwise it starts looping and eating memory
                    // (apparently an internal Windows bug). Nobody else reported it, so we do not investigate further.
                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_UNABLETOGETASSOC), GetErrorText(enumRet));
                    SalMessageBox(parent, errBuf, LoadStr(IDS_UNABLETOGETASSOCTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                break; // end of enumeration
            }
            i++;
        }
    }

    if (explorerFileExts != NULL)
    {
        i = 0;
        while (1)
        { // enumerate all extensions one by one
            DWORD extS = MAX_PATH;
            if (RegEnumKeyEx(explorerFileExts, i, ext, &extS, NULL, NULL, NULL, &ft) == ERROR_SUCCESS)
            { // open the extension key
                if (ext[0] == '.' && HANDLES_Q(RegOpenKey(explorerFileExts, ext, &extKey)) == ERROR_SUCCESS)
                {
                    e = ext + 1; // skip '.'
                    s = ext;
                    while (*++s != 0)
                        *s = LowerCase[*s];
                    *(DWORD*)s = 0; // zero the end of the string

                    int index;
                    BOOL found = GetIndex(e, index);
                    if (WindowsVistaAndLater && HANDLES_Q(RegOpenKey(extKey, "UserChoice", &openKey)) == ERROR_SUCCESS)
                    {                    // try whether it is associated through the UserChoice key; if so, it is the highest-priority record, so overwrite any association already found
                        size = MAX_PATH; // get the association type
                        if (SalRegQueryValueEx(openKey, "Progid", NULL, NULL, (BYTE*)extType, (DWORD*)&size) == ERROR_SUCCESS && size > 1)
                        {
                            extType[MAX_PATH - 1] = 0; // just in case (the value may not be a string, so the null terminator may be missing)

                            if (GetIconFromAssocAux(TRUE, HKEY_CLASSES_ROOT, extType, (LONG)strlen(extType) + 1, data, iconLocation, type))
                            {
                                InsertData("UserChoice: ", index, found, e, s, data, size, iconLocation, type); // found==TRUE means overwriting the association already found with the one from UserChoice
                                found = TRUE;
                            }
                        }
                        HANDLES(RegCloseKey(openKey));
                    }
                    if (!found) // Check whether it is associated through the OpenWithProgids key
                    {
                        if (WindowsVistaAndLater && HANDLES_Q(RegOpenKey(extKey, "OpenWithProgids", &openKey)) == ERROR_SUCCESS)
                        {
                            DWORD j = 0;
                            size = MAX_PATH; // get the association type
                            while (RegEnumValue(openKey, j++, extType, (DWORD*)&size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                            { // enumerate all association types one by one
                                if (extType[0] != 0)
                                {
                                    extType[MAX_PATH - 1] = 0; // just in case (the value may not be a string, so the null terminator may be missing)

                                    if (GetIconFromAssocAux(TRUE, HKEY_CLASSES_ROOT, extType, (LONG)strlen(extType) + 1, data, iconLocation, type))
                                    {
                                        InsertData("OpenWithProgids: ", index, FALSE, e, s, data, size, iconLocation, type);
                                        found = TRUE;
                                        break;
                                    }
                                }
                                size = MAX_PATH; // get the association type
                            }
                            HANDLES(RegCloseKey(openKey));
                        }
                    }

                    if (SalRegQueryValueEx(extKey, "Application", NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                    {
                        if (found) // found, mark that it has an association
                        {
                            CAssociationData* iconData = &(At(index));
                            iconData->SetFlag(1); // files with this extension can be opened
                                                  // iconData->SetIndexAll(-1);  // switching to a static icon causes problems for CDR and CPT Corel files with thumbnails in their icons, so leave the icon configured in HKEY_CLASSES_ROOT
                        }
                        else // not found, insert it as a static icon
                        {
                            data.SetFlag(1); // files with this extension can be opened
                            data.SetIndexAll(-1);

                            InsertData("FileExts: Application: ", index, FALSE, e, s, data, size, "", "");
                        }
                    }
                    HANDLES(RegCloseKey(extKey));
                }
            }
            else
                break; // end of enumeration
            i++;
        }
        HANDLES(RegCloseKey(explorerFileExts));
    }

    // add fixed icons of all sizes to the CAssociations cache bitmap
    int iconSize;
    for (iconSize = 0; iconSize < ICONSIZE_COUNT; iconSize++)
    {
        int resID, vistaResID;
        CIconList* iconList;
        int iconListIndex;
        int j;
        for (j = 0; j < 4; j++)
        {
            if (AllocIcon(&iconList, &iconListIndex, (CIconSizeEnum)iconSize) != -1)
            {
                switch (j)
                {
                case ASSOC_ICON_SOME_DIR:
                {
                    if (!ReadDirectoryIconAndTypeAux(iconList, iconListIndex, (CIconSizeEnum)iconSize))
                        TRACE_E("ReadDirectoryIconAndTypeAux() failed!");
                    continue;
                }

                case ASSOC_ICON_SOME_FILE:
                    resID = 2;
                    vistaResID = 90;
                    break;
                case ASSOC_ICON_SOME_EXE:
                    resID = 3;
                    vistaResID = 15;
                    break;
                default:
                    resID = 1;
                    vistaResID = 2;
                    break;
                }
                int iconWidth = IconSizes[iconSize];
                HICON smallIcon = SalLoadImage(vistaResID, resID, iconWidth, iconWidth, IconLRFlags);
                if (smallIcon != NULL)
                {
                    iconList->ReplaceIcon(iconListIndex, smallIcon);
                    HANDLES(DestroyIcon(smallIcon));
                }
            }
        }
        if (Icons[iconSize].IconsCount != ASSOC_ICON_COUNT)
            TRACE_E("ICON_COUNT and number of icons in cache are not the same!");
    }

    if (systemFileAssoc != NULL)
        HANDLES(RegCloseKey(systemFileAssoc));
    if (closeDialog)
    {
        SetCursor(oldCur);
        if (waitWnd.HWindow != NULL)
            DestroyWindow(waitWnd.HWindow);
    }
}

BOOL CAssociations::IsAssociated(char* ext, BOOL& addtoIconCache, CIconSizeEnum iconSize)
{
    int index;
    if (GetIndex(ext, index))
    {
        int i = At(index).GetIndex(iconSize);
        if (i == -1)
            At(index).SetIndex(-3, iconSize);  // not loaded -> loading
        addtoIconCache = (i == -1 || i == -2); // dynamic or static not loaded/not loading
        return At(index).GetFlag() != 0;
    }
    else
    {
        addtoIconCache = FALSE;
        return FALSE;
    }
}

BOOL CAssociations::IsAssociatedStatic(char* ext, const char*& iconLocation, CIconSizeEnum iconSize)
{
    int index;
    if (GetIndex(ext, index))
    {
        int i = At(index).GetIndex(iconSize);
        if (i == -1)
        {
            At(index).SetIndex(-3, iconSize);          // nenactena -> nacitana
            iconLocation = At(index).ExtensionAndData; // unloaded static item
        }
        else
            iconLocation = NULL;
        return At(index).GetFlag() != 0;
    }
    else
    {
        iconLocation = NULL;
        return FALSE;
    }
}

BOOL CAssociations::IsAssociated(char* ext)
{
    int index;
    if (GetIndex(ext, index))
        return At(index).GetFlag() != 0;
    else
        return FALSE;
}
