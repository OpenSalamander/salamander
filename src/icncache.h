// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************
// CIconData
//

class CIconData
{
public:
    char* NameAndData;           // allocated on DWORD boundaries, null-terminated at the end (for comparison);
                                 // for Flag==3 (and for ==1 if it follows ==3), a string with icon-location is also appended;
                                 // for Flag==4,5,6, the file stamp (CQuadWord Size + FILETIME LastWrite) is also appended,
                                 // along with the list of CPluginInterfaceForThumbLoaderEncapsulation interfaces for all
                                 // plugins that can create a thumbnail for file 'NameAndData'; the list is terminated
                                 // with NULL)
    const CFileData* FSFileData; // pointer to CFileData for the file (only for FS with icon type pitFromPlugin), otherwise NULL

private:
    DWORD Index : 28;      // >= 0 index into the icon or thumbnail cache (must be < 134217728); -1 -> not loaded;
                           // for Flag==0,1,2,3 this is an index into the icon cache;
                           // for Flag==4,5,6 this is an index into the thumbnail cache
    DWORD ReadingDone : 1; // 1 = we already tried to load it (even unsuccessfully), 0 = we have not tried yet
    DWORD Flag : 3;        // flag for the given type, in CIconCache:
                           // icons: 0 - not loaded, 1 - OK, 2 - old version, 3 - icon specified via icon-location
                           // thumbnails: 4 - not loaded, 5 - OK, 6 - old version (or poor-quality/smaller)

public:
    int GetIndex()
    {
        int index = Index;
        if (index & 0x08000000)
            index |= 0xF0000000; // sign-extend 28-bit int to 32-bit int ...
        return index;
    }

    int SetIndex(int index)
    {
        return Index = index;
    }

    DWORD GetFlag() { return Flag; }
    DWORD SetFlag(DWORD f) { return Flag = f; }

    DWORD GetReadingDone() { return ReadingDone; }
    DWORD SetReadingDone(DWORD r) { return ReadingDone = r; }

    const CFileData* GetFSFileData() { return FSFileData; }
};

//
// ****************************************************************************
// CThumbnailData
//

//
// Represents one thumbnail in CIconCache::ThumbnailsCache.
// Because a larger number of bitmap handles can make the process stall,
// it is better to keep the bitmaps as RAW data.
//
struct CThumbnailData
{
    WORD Width; // thumbnail dimensions
    WORD Height;
    WORD Planes;       // define the data "geometry" (we could omit these two parameters,
    WORD BitsPerPixel; // but that would introduce a risk when switching color depth)
    DWORD* Bits;       // raw data of a device-dependent bitmap; format unknown
};

//
// ****************************************************************************
// CIconCache
//

class CIconCache : public TDirectArray<CIconData>
{
protected:
    //
    // Icons
    //
    TIndirectArray<CIconList> IconsCache; // array of bitmaps used as the icon cache
    int IconsCount;                       // number of occupied slots in the icon bitmaps
    CIconSizeEnum IconSize;               // which icon size do we keep?

    //
    // Thumbnails
    //
    TDirectArray<CThumbnailData> ThumbnailsCache; // array of bitmaps used as the thumbnail cache

    CPluginDataInterfaceEncapsulation* DataIfaceForFS; // for internal use in SortArray() only

public:
    // 'forAssociations' is used to size the array (base/delta); associations are expected to be larger
    CIconCache();
    ~CIconCache();

    void Release(); // release the entire array + invalidate cache
    void Destroy(); // release the entire array + cache

    // Sorts the array for fast lookup; 'dataIface' is NULL except when this is
    // ptPluginFS with icons of type pitFromPlugin.
    void SortArray(int left, int right, CPluginDataInterfaceEncapsulation* dataIface);

    // Returns "found?" and the item index, or the insertion position (sorted array);
    // 'name' must be DWORD-aligned (used only if 'dataIface' is NULL);
    // 'file' is the file-data for file/directory 'name' (used only if 'dataIface' is
    // not NULL); 'dataIface' is NULL except when this is ptPluginFS with icons of type
    // pitFromPlugin
    BOOL GetIndex(const char* name, int& index, CPluginDataInterfaceEncapsulation* dataIface,
                  const CFileData* file);

    // Copies known icons and thumbnails (the old and new caches must be sorted!)
    // In the thumbnail case, passes the image geometry and raw image data
    // (CThumbnailData::Bits) to the new cache; sets Bits=NULL in the old cache to avoid
    // deallocation during destruction; 'dataIface' is NULL except when both old and new
    // caches are ptPluginFS with icons of type pitFromPlugin
    void GetIconsAndThumbsFrom(CIconCache* icons, CPluginDataInterfaceEncapsulation* dataIface,
                               BOOL transferIconsAndThumbnailsAsNew = FALSE,
                               BOOL forceReloadThumbnails = FALSE);

    // Must redraw the base icon set with the new background.
    void ColorsChanged();

    ////////////////
    //
    // Icons methods
    //

    // Allocates space for an icon; returns its index or -1 on error.
    // Variables 'iconList' and 'iconListIndex' may be NULL (then they are not set).
    // Otherwise, 'iconList' returns a pointer to the CIconList carrying the icon and
    // 'iconListIndex' is the index within that image list.
    int AllocIcon(CIconList** iconList, int* imageIconIndex);

    // Returns in 'iconList' a pointer to IconList and in 'iconListIndex' the icon position
    // for 'iconIndex' (returned by AllocIcon);
    BOOL GetIcon(int iconIndex, CIconList** iconList, int* iconListIndex);

    ////////////////
    //
    // Thumbnails methods
    //

    // Allocates space for a thumbnail at the end of the ThumbnailsCache array.
    // If everything is OK, returns the index corresponding to that thumbnail.
    // On error, returns -1.
    int AllocThumbnail();

    // Returns in 'thumbnailData' a pointer to the item
    // 'index' (returned by AllocThumbnail);
    BOOL GetThumbnail(int index, CThumbnailData** thumbnailData);

    void SetIconSize(CIconSizeEnum iconSize);
    CIconSizeEnum GetIconSize() { return IconSize; }

protected:
    // For internal use only.
    void SortArrayInt(int left, int right);
    // For internal use only.
    void SortArrayForFSInt(int left, int right);
};

//
// ****************************************************************************
// CAssociationData
//

struct CAssociationIndexAndFlag
{
    DWORD Index : 31; // >= 0 index; -1 not loaded; -2 dynamic (icon in file); -3 loading (-1 -> -3)
    DWORD Flag : 1;   // can *.ExtensionAndData be opened?
};

class CAssociationData
{
public:
    char* ExtensionAndData; // allocated on DWORD boundaries, null-terminated at the end (for comparison);
                            // extension + additionally appended string with icon-location;
    char* Type;             // file-type string; NULL is used instead of "" (to save memory)

private:
    // We need an Index+Flag pair for each icon size.
    CAssociationIndexAndFlag IndexAndFlag[ICONSIZE_COUNT];

public:
    int GetIndex(CIconSizeEnum iconSize)
    {
        if (iconSize >= ICONSIZE_COUNT)
        {
            TRACE_E("CAssociationData::GetIndex() unexpected iconSize=" << iconSize);
            iconSize = ICONSIZE_16;
        }
        DWORD index = IndexAndFlag[iconSize].Index;
        if (index & 0x40000000)
            index |= 0x80000000; // sign-extend 31-bit int to 32-bit int ...
        return index;
    }

    int SetIndex(int index, CIconSizeEnum iconSize)
    {
        if (iconSize >= ICONSIZE_COUNT)
        {
            TRACE_E("CAssociationData::SetIndex() unexpected iconSize=" << iconSize);
            iconSize = ICONSIZE_16;
        }
        return IndexAndFlag[iconSize].Index = index;
    }

    int SetIndexAll(int index)
    {
        int i;
        for (i = 0; i < ICONSIZE_COUNT; i++)
            IndexAndFlag[i].Index = index;
        return index;
    }

    DWORD GetFlag() { return IndexAndFlag[0].Flag; }
    DWORD SetFlag(DWORD f) { return IndexAndFlag[0].Flag = f; }
};

//
// ****************************************************************************
// CAssociations
//

#define ASSOC_ICON_NO_ASSOC 0 // fixed icons in the CAssociations cache bitmap
#define ASSOC_ICON_SOME_FILE 1
#define ASSOC_ICON_SOME_EXE 2
#define ASSOC_ICON_SOME_DIR 3
#define ASSOC_ICON_COUNT 4

struct CAssociationsIcons
{
public:
    TIndirectArray<CIconList> IconsCache; // array of bitmaps used as the icon cache
    int IconsCount;                       // number of occupied slots in the icon bitmaps

public:
    CAssociationsIcons() : IconsCache(10, 5)
    {
        IconsCount = 0;
    }
};

class CAssociations : public TDirectArray<CAssociationData>
{
protected:
    CAssociationsIcons Icons[ICONSIZE_COUNT];

public:
    CAssociations();
    ~CAssociations();

    void Release(); // release the entire array + invalidate cache
    void Destroy(); // release the entire array + cache

    // all -3 -> -1
    //    void SetAllReadingToUnread();

    // Sorts the array for fast lookup.
    void SortArray(int left, int right);

    // Returns "found?" and the item index, or the insertion position (sorted array);
    // 'name' must be DWORD-aligned;
    BOOL GetIndex(const char* name, int& index);

    // Allocates space for an icon; returns its index or -1 on error.
    // Variables 'iconList' and 'iconListIndex' may be NULL (then they are not set).
    // Otherwise, 'iconList' returns a pointer to the CIconList carrying the icon and
    // 'iconListIndex' is the index within that image list.
    int AllocIcon(CIconList** iconList, int* imageIconIndex, CIconSizeEnum iconSize);

    // Returns in 'iconList' a pointer to IconList and in 'iconListIndex' the icon position
    // for 'iconIndex' (returned by AllocIcon);
    BOOL GetIcon(int iconIndex, CIconList** iconList, int* iconListIndex, CIconSizeEnum iconSize);

    // Must redraw the base icon set with the new background.
    void ColorsChanged();

    void ReadAssociations(BOOL showWaitWnd);

    // ext must be DWORD-aligned
    BOOL IsAssociated(char* ext, BOOL& addtoIconCache, CIconSizeEnum iconSize);
    BOOL IsAssociatedStatic(char* ext, const char*& iconLocation, CIconSizeEnum iconSize);
    BOOL IsAssociated(char* ext);

protected:
    // Helper method.
    void InsertData(const char* origin, int index, BOOL overwriteItem, char* e, char* s,
                    CAssociationData& data, LONG& size, const char* iconLocation, const char* type);
};

extern CAssociations Associations; // loaded associations are stored here
