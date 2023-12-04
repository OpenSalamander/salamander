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
    char* NameAndData;           // alokovano po DWORDech, konce nulovane (kvuli porovnavani);
                                 // pro Flag==3 (i pro ==1, nasleduje-li po ==3) navic pripojen string s icon-location;
                                 // pro Flag==4,5,6 navic pripojena znamka souboru (CQuadWord Size + FILETIME LastWrite)
                                 //   a seznam rozhrani CPluginInterfaceForThumbLoaderEncapsulation
                                 //   vsech pluginu, ktere umi vytvorit thumbnail pro soubor 'NameAndData', seznam je ukoncen
                                 //   NULLem)
    const CFileData* FSFileData; // ukazatel na CFileData souboru (jen u FS s typem ikon pitFromPlugin), jinak NULL

private:
    DWORD Index : 28;      // >= 0 index do cache ikon nebo thumbnailu (index musi byt < 134217728); -1 -> nenactene;
                           //   pri Flag==0,1,2,3 jde o index do cache ikon;
                           //   pri Flag==4,5,6 jde o index do cache thumbnailu
    DWORD ReadingDone : 1; // 1 = uz jsme se pokouseli nacist (i neuspesne), 0 = jeste jsme nenacitali
    DWORD Flag : 3;        // flag k danemu typu, v CIconCache:
                           //   ikony: 0 - nenactene, 1 - o.k., 2 - stara verze, 3 - ikona zadana pomoci icon-location
                           //   thumbnaily: 4 - nenactene, 5 - o.k., 6 - stara verze (nebo nekvalitni/mensi)

public:
    int GetIndex()
    {
        int index = Index;
        if (index & 0x08000000)
            index |= 0xF0000000; // neumi 28-bit int na 32-bit int ...
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
// reprezentuje jeden thumbnail v CIconCache::ThumbnailsCache
// protoze pri vetsim mnozstvi handlu bitmap dochazi k tuhnuti procesu,
// je lepsi drzet bitmapy jako RAW data
//
struct CThumbnailData
{
    WORD Width; // rozmery thumbnailu
    WORD Height;
    WORD Planes;       // urcuji "geometrii" dat (tyto dva parametry bychom mohli vypustit,
    WORD BitsPerPixel; // ale vzniklo by riziko pri prepnuti barevne hloubky)
    DWORD* Bits;       // raw data device dependent bitmapy; format neznamy
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
    TIndirectArray<CIconList> IconsCache; // pole bitmap slouzici jako cache na ikonky
    int IconsCount;                       // pocet zaplnenych mist v bitmapach (ikon)
    CIconSizeEnum IconSize;               // jakou velikost ikonek drzime?

    //
    // Thumbnails
    //
    TDirectArray<CThumbnailData> ThumbnailsCache; // pole bitmap slouzici jako cache na thumbnaily

    CPluginDataInterfaceEncapsulation* DataIfaceForFS; // jen pro interni pouziti v SortArray()

public:
    // 'forAssociations' slouzi k dimenzovani velikosti (base/delta) pole; u asociaci se predpoklada vetsi
    CIconCache();
    ~CIconCache();

    void Release(); // uvolneni celeho pole + invalidate cache
    void Destroy(); // uvolneni celeho pole + cache

    // seradi pole pro rychle vyhledavani; 'dataIface' je NULL krome pripadu, kdy jde
    // o ptPluginFS s ikonami typu pitFromPlugin
    void SortArray(int left, int right, CPluginDataInterfaceEncapsulation* dataIface);

    // vraci "nalezeno ?" a index polozky nebo kam se ma vlozit (razene pole);
    // 'name' musi byt zarovnane po DWORDech (pouziva se jen pokud je 'dataIface' NULL);
    // 'file' jsou file-data souboru/adresare 'name' (pouziva se jen pokud neni 'dataIface'
    // NULL); 'dataIface' je NULL krome pripadu, kdy jde o ptPluginFS s ikonami typu
    // pitFromPlugin
    BOOL GetIndex(const char* name, int& index, CPluginDataInterfaceEncapsulation* dataIface,
                  const CFileData* file);

    // nakopiruje si zname ikonky a thumbnaily (stara a nova cache musi byt serazene !)
    // v pripade thumbnailu preda geometrii a raw data obrazku (CThumbnailData::Bits)
    // do nove cache; ve stare nastavi Bits=NULL, aby pri destrukci nedoslo k dealokaci;
    // 'dataIface' je NULL krome pripadu, kdy jde u stare i nove cache o ptPluginFS
    // s ikonami typu pitFromPlugin
    void GetIconsAndThumbsFrom(CIconCache* icons, CPluginDataInterfaceEncapsulation* dataIface,
                               BOOL transferIconsAndThumbnailsAsNew = FALSE,
                               BOOL forceReloadThumbnails = FALSE);

    // musi prekreslit zakladni sadu ikon s novym pozadim
    void ColorsChanged();

    ////////////////
    //
    // Icons methods
    //

    // allokuje misto pro ikonku; vraci jeji index nebo -1 pri chybe
    // promenne 'iconList' a 'iconListIndex' mohou byt NULL (pak nejsou nastavovany)
    // jinak 'iconList' vraci ukazatel na CIconList, ktery nese ikonu a 'iconListIndex'
    // je index v ramci tohoto imagelistu.
    int AllocIcon(CIconList** iconList, int* imageIconIndex);

    // vrati v 'iconList' ukazatel na IconList a v 'iconListIndex' pozici ikonky
    // 'iconIndex' (vracene z AllocIcon);
    BOOL GetIcon(int iconIndex, CIconList** iconList, int* iconListIndex);

    ////////////////
    //
    // Thumbnails methods
    //

    // alokuje misto pro thumbnail na konci pole ThumbnailsCache
    // pokud je vse OK, vraci index, ktery thumbnailu odpovida
    // pri chybe vrati -1
    int AllocThumbnail();

    // vrati v 'thumbnailData' ukazatel na polozku
    // 'index' (vracena z AllocThumbnail);
    BOOL GetThumbnail(int index, CThumbnailData** thumbnailData);

    void SetIconSize(CIconSizeEnum iconSize);
    CIconSizeEnum GetIconSize() { return IconSize; }

protected:
    // jen pro interni pouziti
    void SortArrayInt(int left, int right);
    // jen pro interni pouziti
    void SortArrayForFSInt(int left, int right);
};

//
// ****************************************************************************
// CAssociationData
//

struct CAssociationIndexAndFlag
{
    DWORD Index : 31; // >= 0 index; -1 nenactene; -2 dynamicke (ikona v souboru); -3 nacitane (-1 -> -3)
    DWORD Flag : 1;   // jde *.ExtensionAndData otevrit ?
};

class CAssociationData
{
public:
    char* ExtensionAndData; // alokovano po DWORDech, konce nulovane (kvuli porovnavani);
                            // extension + navic pripojen string s icon-location;
    char* Type;             // retezec file-type; misto "" je NULL (setrime pamet)

private:
    // pro kazdy rozmer ikon potrebujeme par Index+Flag
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
            index |= 0x80000000; // neumi 31-bit int na 32-bit int ...
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

#define ASSOC_ICON_NO_ASSOC 0 // pevne ikonky v cache-bitmape CAssociations
#define ASSOC_ICON_SOME_FILE 1
#define ASSOC_ICON_SOME_EXE 2
#define ASSOC_ICON_SOME_DIR 3
#define ASSOC_ICON_COUNT 4

struct CAssociationsIcons
{
public:
    TIndirectArray<CIconList> IconsCache; // pole bitmap slouzici jako cache na ikonky
    int IconsCount;                       // pocet zaplnenych mist v bitmapach (ikon)

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

    void Release(); // uvolneni celeho pole + invalidate cache
    void Destroy(); // uvolneni celeho pole + cache

    // vsechny -3 -> -1
    //    void SetAllReadingToUnread();

    // seradi pole pro rychle vyhledavani
    void SortArray(int left, int right);

    // vraci "nalezeno ?" a index polozky nebo kam se ma vlozit (razene pole);
    // 'name' musi byt zarovnane po DWORDech ;
    BOOL GetIndex(const char* name, int& index);

    // allokuje misto pro ikonku; vraci jeji index nebo -1 pri chybe
    // promenne 'iconList' a 'iconListIndex' mohou byt NULL (pak nejsou nastavovany)
    // jinak 'iconList' vraci ukazatel na CIconList, ktery nese ikonu a 'iconListIndex'
    // je index v ramci tohoto imagelistu.
    int AllocIcon(CIconList** iconList, int* imageIconIndex, CIconSizeEnum iconSize);

    // vrati v 'iconList' ukazatel na IconList a v 'iconListIndex' pozici ikonky
    // 'iconIndex' (vracene z AllocIcon);
    BOOL GetIcon(int iconIndex, CIconList** iconList, int* iconListIndex, CIconSizeEnum iconSize);

    // musi prekreslit zakladni sadu ikon s novym pozadim
    void ColorsChanged();

    void ReadAssociations(BOOL showWaitWnd);

    // ext musi byt zarovnan po DWORDech
    BOOL IsAssociated(char* ext, BOOL& addtoIconCache, CIconSizeEnum iconSize);
    BOOL IsAssociatedStatic(char* ext, const char*& iconLocation, CIconSizeEnum iconSize);
    BOOL IsAssociated(char* ext);

protected:
    // pomocna metoda
    void InsertData(const char* origin, int index, BOOL overwriteItem, char* e, char* s,
                    CAssociationData& data, LONG& size, const char* iconLocation, const char* type);
};

extern CAssociations Associations; // zde jsou zalozeny nactene asociace
