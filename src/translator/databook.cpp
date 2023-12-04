// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

int CData::FindBookmark(DWORD treeItem, WORD textItem)
{
    for (int i = 0; i < Bookmarks.Count; i++)
    {
        const CBookmark* bookmark = &Bookmarks[i];
        if (bookmark->TreeItem == treeItem && bookmark->TextItem == textItem)
            return i;
    }
    return -1;
}

void CData::ToggleBookmark(DWORD treeItem, WORD textItem)
{
    // pokud bookmark existuje, budeme prepinat jeho stav
    int index = FindBookmark(treeItem, textItem);
    if (index != -1)
        Bookmarks.Delete(index);
    else
    {
        CBookmark bookmark;
        bookmark.TreeItem = treeItem;
        bookmark.TextItem = textItem;
        Bookmarks.Add(bookmark);
    }
}

const CBookmark*
CData::GotoBookmark(BOOL next, DWORD treeItem, WORD textItem)
{
    int index = FindBookmark(treeItem, textItem);
    if (index == -1)
        index = next ? 0 : Bookmarks.Count - 1;
    else
    {
        index += next ? 1 : -1;
        if (index < 0)
        {
            index = Bookmarks.Count - 1;
        }
        else
        {
            if (index >= Bookmarks.Count)
                index = 0;
        }
    }
    if (index >= 0 && index < Bookmarks.Count)
    {
        const CBookmark* bookmark = &Bookmarks[index];
        return bookmark;
    }
    return NULL;
}

void CData::ClearBookmarks()
{
    Bookmarks.DestroyMembers();
}

int CData::GetBookmarksCount()
{
    return Bookmarks.Count;
}
