// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "dialogs.h"
#include "mainwnd.h"
#include "plugins.h"
#include "geticon.h"
#include "logo.h"

// melo by byt nasobkem hodnoty IL_ITEMS_IN_ROW
// aby se plne vyuzil prostor v bitmape
#define ICONS_IN_LIST 100

//
// ****************************************************************************
// CIconCache
//

CIconCache::CIconCache()
    : TDirectArray<CIconData>(50, 30),
      IconsCache(10, 5),     // jedna polozka je CIconList drzici ICONS_IN_LIST ikonek
      ThumbnailsCache(1, 20) // ziskavani thumbnailu je pomale, relokace je v tom hracka
{
    IconsCount = 0;
    IconSize = ICONSIZE_COUNT; // zatim nenastaveno; pokus o pridani ikonky bez predchoziho volani SetIconSize() zpusobi TRACE_E
    DataIfaceForFS = NULL;
}

CIconCache::~CIconCache()
{
    Destroy();
}

inline int CompareDWORDS(const char* s1, const char* s2, int length)
{ // porovna nejvyse 'length' DWORDu
    //  int res;
    const char* end = s1 + length;
    while (s1 <= end)
    {
        //    if ((res = *(DWORD *)s1 - *(DWORD *)s2) != 0) return res;  // takhle to nejde (zkus si 0x8 a 0x0 ve 4-bitovych cislech)
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
    if (dataIface != NULL) // jde o pitFromPlugin: nechame plugin, aby polozky porovnal sam (musi jit o porovnani
    {                      // beze shod zadnych dvou polozek listingu)
        DataIfaceForFS = dataIface;
        BOOL ok = TRUE;
        int i;
        for (i = left; i <= right; i++) // jeden paranoicky testik
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
    else // klasicke razeni podle jmen
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

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortArrayInt(left, j);
    //  if (i < right) SortArrayInt(i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
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

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortArrayForFSInt(left, j);
    //  if (i < right) SortArrayForFSInt(i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
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
    if (Count == 0 || dataIface != NULL && file == NULL) // overime platnost 'file'
    {
        if (dataIface != NULL && file == NULL)
            TRACE_E("CIconCache::GetIndex(): 'file' may not be NULL when 'dataIface' is not NULL! item=" << name);
        index = 0;
        return FALSE;
    }

    if (dataIface != NULL) // jde o pitFromPlugin: nechame plugin, aby polozky porovnal sam (musi jit o porovnani
    {                      // beze shod zadnych dvou polozek listingu)
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
                return FALSE; // chyba -> vratime treba: nenalezeno, vlozit na zacatek pole
            }
            if (res == 0) // nalezeno
            {
                index = m;
                return TRUE;
            }
            else if (res > 0)
            {
                if (l == r || l > m - 1) // nenalezeno
                {
                    index = m; // mel by byt na teto pozici
                    return FALSE;
                }
                r = m - 1;
            }
            else
            {
                if (l == r) // nenalezeno
                {
                    index = m + 1; // mel by byt az za touto pozici
                    return FALSE;
                }
                l = m + 1;
            }
        }
    }
    else // klasicke hledani podle jmena
    {
        int length = (int)strlen(name);
        int l = 0, r = Count - 1, m;
        int res;
        while (1)
        {
            m = (l + r) / 2;
            res = CompareDWORDS(At(m).NameAndData, name, length);
            if (res == 0) // nalezeno
            {
                index = m;
                return TRUE;
            }
            else if (res > 0)
            {
                if (l == r || l > m - 1) // nenalezeno
                {
                    index = m; // mel by byt na teto pozici
                    return FALSE;
                }
                r = m - 1;
            }
            else
            {
                if (l == r) // nenalezeno
                {
                    index = m + 1; // mel by byt az za touto pozici
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

    // destrukce raw dat z ThumbnailsCache
    for (i = 0; i < ThumbnailsCache.Count; i++)
    {
        CThumbnailData* data = &ThumbnailsCache[i];
        if (data->Bits != NULL)
            free(data->Bits); // alokovano v CSalamanderThumbnailMaker::RenderToThumbnailData()
    }
    ThumbnailsCache.DestroyMembers();
}

void CIconCache::Destroy()
{
    // uvonime alokovana data a pole samotne
    Release();

    // destrukce imagelistu z IconsCache
    IconsCache.DestroyMembers();
}

void CIconCache::ColorsChanged()
{
    CALL_STACK_MESSAGE1("CIconCache::ColorsChanged()");
    // tato funkce je volana pri zmene barev nebo barevne hloubky obrazovky
    // druhy pripad neni reseny -- bylo by treba znovu konstruovat bitmapy
    // v imagelistech pro aktualni barevnou hloubku
    COLORREF bkColor = GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]);
    int i;
    for (i = 0; i < IconsCache.Count; i++)
    {
        CIconList* il = IconsCache[i];
        if (il != NULL)
            il->SetBkColor(bkColor);
    }

    // thumbnaily je potreba prekreslit, pokud se zmenila barva pozadi, budou se ikonky s pruhlednymi
    // castmi kreslit do nove barvy pozadi
    for (i = 0; i < Count; i++)
    {
        CIconData* icon = &At(i);
        if (icon->GetFlag() == 5 /* o.k. thumbnail */)
            icon->SetFlag(6 /* stara verze thumbnailu */);
    }
}

int CIconCache::AllocIcon(CIconList** iconList, int* iconListIndex)
{
    SLOW_CALL_STACK_MESSAGE1("CIconCache::AllocIcon()");
    int cache = IconsCount / ICONS_IN_LIST; // cache
    int index = IconsCount % ICONS_IN_LIST; // index v ramci teto cache
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

    // stuktura pro drzeni thumbnailu
    CThumbnailData data;
    memset(&data, 0, sizeof(CThumbnailData));

    // zaradime jej do seznamu
    int index = ThumbnailsCache.Add(data);
    if (!ThumbnailsCache.IsGood())
    {
        ThumbnailsCache.ResetState();
        return -1;
    }

    // vratime index pridaneho prvku
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
        int index = iconIndex % ICONS_IN_LIST; // index v ramci cache
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

    if (dataIface != NULL) // jde o pitFromPlugin: nechame plugin, aby polozky porovnal sam (musi jit o porovnani
    {                      // beze shod zadnych dvou polozek listingu)
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
            return; // neni co spojovat

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
            return; // neni co spojovat
        int res;
        while (1)
        {
            res = dataIface->CompareFilesFromFS(file1, file2);
            if (res == 0) // shodne -> provedeme kopii (ikony a masky)
            {
                CIconList* srcIconList;
                int srcIconListIndex;

                CIconList* dstIconList;
                int dstIconListIndex;

                DWORD flag = icons->At(index2).GetFlag();

                if ((flag == 1 || flag == 2) &&  // platna nebo stara ikona
                    At(index1).GetFlag() == 0 && // zajima nas ikona (pokud doslo k prepnuti na thumbnail, starou ikonu nepotrebujeme)
                    GetIcon(At(index1).GetIndex(), &dstIconList, &dstIconListIndex) &&
                    icons->GetIcon(icons->At(index2).GetIndex(), &srcIconList, &srcIconListIndex))
                {
                    dstIconList->Copy(dstIconListIndex, srcIconList, srcIconListIndex);
                    At(index1).SetFlag((flag == 1 && transferIconsAndThumbnailsAsNew) ? 1 : 2); // ted uz je to stara (nebo platna/nova) verze ikony
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
                    break; // uz neni co spojovat
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
                    break; // uz neni co spojovat
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
            return; // neni co spojovat

        if (index2 < icons->Count)
            name2 = icons->At(index2).NameAndData;
        else
            return; // neni co spojovat
        int res;
        while (1)
        {
            res = CompareDWORDS(name1, name2, length);
            if (res == 0) // shodne -> provedeme kopii (ikony a masky) || thumbnailu
            {
                CIconList* srcIconList;
                int srcIconListIndex;

                CIconList* dstIconList;
                int dstIconListIndex;

                DWORD flag = icons->At(index2).GetFlag();

                if ((flag == 1 || flag == 2) &&  // platna nebo stara ikona
                    At(index1).GetFlag() == 0 && // zajima nas ikona (pokud doslo k prepnuti na thumbnail, starou ikonu nepotrebujeme)
                    GetIcon(At(index1).GetIndex(), &dstIconList, &dstIconListIndex) &&
                    icons->GetIcon(icons->At(index2).GetIndex(), &srcIconList, &srcIconListIndex))
                {
                    dstIconList->Copy(dstIconListIndex, srcIconList, srcIconListIndex);
                    At(index1).SetFlag((flag == 1 && transferIconsAndThumbnailsAsNew) ? 1 : 2); // ted uz je to stara (nebo platna/nova) verze ikony
                }
                else
                {
                    CThumbnailData* srcThumbnailData;
                    CThumbnailData* tgtThumbnailData;

                    if ((flag == 5 || flag == 6) &&  // platny nebo stary thumbnail
                        At(index1).GetFlag() == 4 && // zajima nas thumbnail (pokud doslo k prepnuti na ikonu, stary thumbnail nepotrebujeme)
                        GetThumbnail(At(index1).GetIndex(), &tgtThumbnailData) &&
                        icons->GetThumbnail(icons->At(index2).GetIndex(), &srcThumbnailData))
                    {
                        // stary thumbnail neni treba kopirovat -- staci predat jeho
                        // geometrii a raw data do ciloveho thumbnailu
                        *tgtThumbnailData = *srcThumbnailData; // predame stara data do nove cache
                        srcThumbnailData->Bits = NULL;         // stara cache zanika a nesmi se dealokovat data

                        int newFlag = 6;
                        // pokud kopirujeme platny thumbnail, zkontrolujeme znamku souboru (size+date), pripadne
                        // zkopirovany thumbnail oznacime rovnou jako platny (hrozba zmeny souboru beze zmeny
                        // size+date je miziva a rychlostni zisk je obrovsky)
                        if (flag == 5 && !forceReloadThumbnails)
                        {
                            if (transferIconsAndThumbnailsAsNew)
                                newFlag = 5;
                            else
                            {
                                int offset = length + 4;
                                offset -= (offset & 0x3); // offset % 4  (zarovnani po ctyrech bytech)
                                if (*(CQuadWord*)(name1 + offset) == *(CQuadWord*)(name2 + offset) &&
                                    CompareFileTime((FILETIME*)(name1 + offset + sizeof(CQuadWord)),
                                                    (FILETIME*)(name2 + offset + sizeof(CQuadWord))) == 0)
                                {
                                    newFlag = 5;
                                }
                            }
                        }
                        At(index1).SetFlag(newFlag); // ted uz je to stara (nebo platna/nova) verze thumbnailu
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
                    break; // uz neni co spojovat
            }

            if (res == 0 || res > 0) // index2++
            {
                if (++index2 < icons->Count)
                    name2 = icons->At(index2).NameAndData;
                else
                    break; // uz neni co spojovat
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
    if (iconSize == IconSize) // pokud se nemeni velikost, neni co resit
        return;

    // zahodime soucasne ikonky
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

        // file-type retezec ziskame jako value "" podklice keyName
        if (HANDLES_Q(RegOpenKey(root, keyNameBuf, &openKey)) == ERROR_SUCCESS)
        {
            LONG typeSize = MAX_PATH;
            if (SalRegQueryValue(openKey, "", type, &typeSize) != ERROR_SUCCESS)
                type[0] = 0;
            HANDLES(RegCloseKey(openKey));
        }
    }

    if (size - 1 + 7 <= MAX_PATH) // test na moznost otevirani pres asociace
    {
        memmove(keyNameBuf + size - 1, "\\Shell", 7);
        if (HANDLES_Q(RegOpenKey(root, keyNameBuf, &openKey)) == ERROR_SUCCESS)
        { // obsahuje-li "\\shell" nejaky podklic, da se otvirat (asociace na Enter)
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
        // obsahuje-li "\\ShellEx\\IconHandler", musi se tahat ze souboru
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
            size = MAX_PATH; // ziskani cesty k ikone
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

                // odstranime uvozovky v pripade "\"jmeno_souboru\",cislo_ikony" (napr. "\"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe\",0")
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
                    if (numBeg < numEnd && *numEnd == 0 &&                                   // cislo ikony je za posledni carkou
                        *iconLocation == '"' && num - 1 > iconLocation && *(num - 1) == '"') // na zacatku a pred carkou jsou uvozovky
                    {                                                                        // odstranime uvozovky
                        memmove(iconLocation, iconLocation + 1, (num - 1) - (iconLocation + 1));
                        memmove(num - 2, num, numEnd - num + 1);
                    }
                }

                char* s = buf; // rozliseni typu "%1" od "...%promenna%..."
                while (*s != 0)
                {
                    if (*s == '%')
                    {
                        s++;
                        if (*s != '%')
                        {
                            while (*s != 0 && *s != ' ' && *s != '%')
                                s++;
                            if (*s != '%') // neni to env. promenna -> dynamicky typ
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
    // uvonime alokovana data a pole samotne
    Release();

    // destrukce imagelistu z IconsCache
    int i;
    for (i = 0; i < ICONSIZE_COUNT; i++)
        Icons[i].IconsCache.DestroyMembers();
}

void CAssociations::ColorsChanged()
{
    CALL_STACK_MESSAGE1("CAssociations::ColorsChanged()");
    // tato funkce je volana pri zmene barev nebo barevne hloubky obrazovky
    // druhy pripad neni reseny -- bylo by treba znovu konstruovat bitmapy
    // v imagelistech pro aktualni barevnou hloubku
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
    // FIXME: stacilo by nastavovat pozadi pouze pro patricny iconlist
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
        if (res == 0) // nalezeno
        {
            index = m;
            return TRUE;
        }
        else if (res > 0)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                index = m; // mel by byt na teto pozici
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                index = m + 1; // mel by byt az za touto pozici
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
    int index = Icons[iconSize].IconsCount % ICONS_IN_LIST; // index v ramci teto cache
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
        int index = iconIndex % ICONS_IN_LIST; // index v ramci cache
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

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortArray(left, j);
    //  if (i < right) SortArray(i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
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
    size -= (size & 0x3); // size % 4  (zarovnani po ctyrech bytech)
    int iLen = (int)strlen(iconLocation) + 1;
    data.ExtensionAndData = (char*)malloc(size + iLen);
    memcpy(data.ExtensionAndData, e, size);                   // pripona + zarovnani nul +
    memcpy(data.ExtensionAndData + size, iconLocation, iLen); // icon-location
    if (type[0] != 0)
        data.Type = DupStr(type); // chyba -> jen se neukaze file-type
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
    //---  nahozeni dialogu cekani + hodin
    HCURSOR oldCur;
    HWND parent = (MainWindow != NULL) ? MainWindow->HWindow : NULL;
    // wait okenko zlobilo:
    // pokud dam nad souborem Open With a zvolim napriklad NOTEPAD, ten se otevre
    // potom se rozesle notifikace o zmene asociaci SHCNE_ASSOCCHANGED
    // v dusledku se zavola tato fce, ktera zobrazi okenko a vytahne ho nahoru
    // s nim vytahne celeho Salamandera, proto ho docasne zakazuji
    CWaitWindow waitWnd(parent, IDS_READINGASSOCIATIONS, FALSE, ooStatic);
    BOOL closeDialog = FALSE;
    if (!ExistSplashScreen())
    {
        if (showWaitWnd)
            waitWnd.Create(); //j.r. pro ladeni shortcuty z desktopu
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        closeDialog = TRUE;
    }
    else
        IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_ASSOCIATIONS));
    //---  vycisteni pole + cache
    Release();
    //---  projiti registry zaznamu o classech (extenzich)
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

    // Windows 2000 a novejsi maji jeste "Open With..." asociace ulozeny pro kazdeho usera zvlast
    // v klici HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts
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
    { // postupne enumnuti vsech pripon
        DWORD extS = MAX_PATH;
        if ((enumRet = RegEnumKeyEx(HKEY_CLASSES_ROOT, i, ext, &extS, NULL, NULL, NULL, &ft)) == ERROR_SUCCESS)
        { // otevreni klice pripony
            if (ext[0] == '.' && HANDLES_Q(RegOpenKey(HKEY_CLASSES_ROOT, ext, &extKey)) == ERROR_SUCCESS)
            {
                size = MAX_PATH; // ziskani typu asociace
                iconLocation[0] = 0;
                data.SetFlag(0);
                data.SetIndexAll(-1);
                type[0] = 0;
                BOOL tryPerceivedType = FALSE;
                BOOL addExt = (SalRegQueryValue(extKey, "", extType, &size) == ERROR_SUCCESS && size > 1);
                if (addExt)
                {
                    // test na typ ikony (staticka/dynamicka viz .h)
                    tryPerceivedType = !GetIconFromAssocAux(FALSE, HKEY_CLASSES_ROOT, extType, size, data, iconLocation, type);
                }
                else
                    tryPerceivedType = TRUE;
                if (tryPerceivedType && systemFileAssoc != NULL)
                {
                    // nejdrive zkusime najit 'ext' pod klicem SystemFileAssociations
                    if (GetIconFromAssocAux(FALSE, systemFileAssoc, ext, (LONG)strlen(ext) + 1, data, iconLocation, NULL))
                        addExt = TRUE;
                    else
                    { // zkusime jeste klic z hodnoty PerceivedType (je-li definovana)
                        size = MAX_PATH;
                        if (SalRegQueryValueEx(extKey, "PerceivedType", NULL, NULL, (BYTE*)extType, (DWORD*)&size) == ERROR_SUCCESS && size > 1)
                        {
                            extType[MAX_PATH - 1] = 0; // pro jistotu (hodnota nemusi byt typu string, pak muze chybet null-terminator)
                            if (GetIconFromAssocAux(FALSE, systemFileAssoc, extType, (LONG)strlen(extType) + 1, data, iconLocation, NULL))
                                addExt = TRUE;
                        }
                    }
                }
                if (addExt)
                {                // prevod ext. na mala pismena + pridani do pole
                    e = ext + 1; // preskok '.'
                    s = e;
                    while (*s != 0)
                    {
                        *s = LowerCase[*s];
                        s++;
                    }
                    *(DWORD*)s = 0; // nulovani konce stringu

                    InsertData("", Count, FALSE, e, s, data, size, iconLocation, type);
                }
                HANDLES(RegCloseKey(extKey));
            }
        }
        else
        {
            if (enumRet != ERROR_NO_MORE_ITEMS)
            {
                // u jednoho usera (Bernard Vander Beken <Bernard.VanderBeken@deceuninck.com>) sem prijde
                // jedna chyba ERROR_MORE_DATA, a pak spousta ERROR_OUTOFMEMORY, ma Microsoft Windows Server 2003,
                // Standard Edition Service Pack 2 (Build 3790), zvetseni bufferu na 10000 nepomaha, nutne
                // ukoncit enumeraci uz pri prvni chybe, jinak zacne cyklit a zarat pamet (zrejme jde
                // o interni chybu Windowsu), nikdo dalsi to nehlasil, takze to dal neresime
                _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_UNABLETOGETASSOC), GetErrorText(enumRet));
                SalMessageBox(parent, errBuf, LoadStr(IDS_UNABLETOGETASSOCTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            break; // konec enumerace
        }
        i++;
    }
    if (Count > 1)
        SortArray(0, Count - 1);

    // Windows XP maji asociace (viz PerceivedType) ulozeny jeste v klici HKEY_CLASSES_ROOT\SystemFileAssociations,
    // nacteme dosud nezname pripony jeste z tohoto klice
    if (systemFileAssoc != NULL)
    {
        i = 0;
        while (1)
        { // postupne enumnuti vsech pripon
            DWORD extS = MAX_PATH;
            if ((enumRet = RegEnumKeyEx(systemFileAssoc, i, ext, &extS, NULL, NULL, NULL, &ft)) == ERROR_SUCCESS)
            { // otevreni klice pripony
                if (ext[0] == '.')
                {
                    e = ext + 1; // preskok '.'
                    s = ext;
                    while (*++s != 0)
                        *s = LowerCase[*s];
                    *(DWORD*)s = 0; // nulovani konce stringu

                    int index;
                    if (!GetIndex(e, index)) // nenalezeno, ma smysl zkoumat + pripadne pridat
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
                    // u jednoho usera (Bernard Vander Beken <Bernard.VanderBeken@deceuninck.com>) sem prijde
                    // jedna chyba ERROR_MORE_DATA, a pak spousta ERROR_OUTOFMEMORY, ma Microsoft Windows Server 2003,
                    // Standard Edition Service Pack 2 (Build 3790), zvetseni bufferu na 10000 nepomaha, nutne
                    // ukoncit enumeraci uz pri prvni chybe, jinak zacne cyklit a zrat pamet (zrejme jde
                    // o interni chybu Windowsu), nikdo dalsi to nehlasil, takze to dal neresime
                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_UNABLETOGETASSOC), GetErrorText(enumRet));
                    SalMessageBox(parent, errBuf, LoadStr(IDS_UNABLETOGETASSOCTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                break; // konec enumerace
            }
            i++;
        }
    }

    if (explorerFileExts != NULL)
    {
        i = 0;
        while (1)
        { // postupne enumnuti vsech pripon
            DWORD extS = MAX_PATH;
            if (RegEnumKeyEx(explorerFileExts, i, ext, &extS, NULL, NULL, NULL, &ft) == ERROR_SUCCESS)
            { // otevreni klice pripony
                if (ext[0] == '.' && HANDLES_Q(RegOpenKey(explorerFileExts, ext, &extKey)) == ERROR_SUCCESS)
                {
                    e = ext + 1; // preskok '.'
                    s = ext;
                    while (*++s != 0)
                        *s = LowerCase[*s];
                    *(DWORD*)s = 0; // nulovani konce stringu

                    int index;
                    BOOL found = GetIndex(e, index);
                    if (WindowsVistaAndLater && HANDLES_Q(RegOpenKey(extKey, "UserChoice", &openKey)) == ERROR_SUCCESS)
                    {                    // zkusime, jestli neni asociovana pres klic UserChoice, pokud ano, je to neprioritnejsi zaznam, pripadne tedy prepiseme jiz existujici asociaci
                        size = MAX_PATH; // ziskani typu asociace
                        if (SalRegQueryValueEx(openKey, "Progid", NULL, NULL, (BYTE*)extType, (DWORD*)&size) == ERROR_SUCCESS && size > 1)
                        {
                            extType[MAX_PATH - 1] = 0; // pro jistotu (hodnota nemusi byt typu string, pak muze chybet null-terminator)

                            if (GetIconFromAssocAux(TRUE, HKEY_CLASSES_ROOT, extType, (LONG)strlen(extType) + 1, data, iconLocation, type))
                            {
                                InsertData("UserChoice: ", index, found, e, s, data, size, iconLocation, type); // found==TRUE znamena prepis jiz nalezene asociace tou z UserChoice
                                found = TRUE;
                            }
                        }
                        HANDLES(RegCloseKey(openKey));
                    }
                    if (!found) // zkusime jeste, jestli neni asociovana pres klic OpenWithProgids
                    {
                        if (WindowsVistaAndLater && HANDLES_Q(RegOpenKey(extKey, "OpenWithProgids", &openKey)) == ERROR_SUCCESS)
                        {
                            DWORD j = 0;
                            size = MAX_PATH; // ziskani typu asociace
                            while (RegEnumValue(openKey, j++, extType, (DWORD*)&size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                            { // postupne enumnuti vsech typu asociaci
                                if (extType[0] != 0)
                                {
                                    extType[MAX_PATH - 1] = 0; // pro jistotu (hodnota nemusi byt typu string, pak muze chybet null-terminator)

                                    if (GetIconFromAssocAux(TRUE, HKEY_CLASSES_ROOT, extType, (LONG)strlen(extType) + 1, data, iconLocation, type))
                                    {
                                        InsertData("OpenWithProgids: ", index, FALSE, e, s, data, size, iconLocation, type);
                                        found = TRUE;
                                        break;
                                    }
                                }
                                size = MAX_PATH; // ziskani typu asociace
                            }
                            HANDLES(RegCloseKey(openKey));
                        }
                    }

                    if (SalRegQueryValueEx(extKey, "Application", NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                    {
                        if (found) // nalezeno, nastavime, ze ma asociaci
                        {
                            CAssociationData* iconData = &(At(index));
                            iconData->SetFlag(1); // soubory s touto priponou jdou otevirat
                                                  // iconData->SetIndexAll(-1);  // prepnuti na statickou ikonu zlobi s CDR a CPT Corelackymi soubory s nahledy v ikonach, radsi nechame ikonu v nastaveni z HKEY_CLASSES_ROOT
                        }
                        else // nenalezeno, vlozime jako statickou ikonu
                        {
                            data.SetFlag(1); // soubory s touto priponou jdou otevirat
                            data.SetIndexAll(-1);

                            InsertData("FileExts: Application: ", index, FALSE, e, s, data, size, "", "");
                        }
                    }
                    HANDLES(RegCloseKey(extKey));
                }
            }
            else
                break; // konec enumerace
            i++;
        }
        HANDLES(RegCloseKey(explorerFileExts));
    }

    // pridani pevnych ikonek vsech velikosti do cache-bitmap CAssociations
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
            At(index).SetIndex(-3, iconSize);  // nenactena -> nacitana
        addtoIconCache = (i == -1 || i == -2); // dynamicka nebo nenactena/nenacitana staticka
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
            iconLocation = At(index).ExtensionAndData; // nenactena/nenacitana staticka
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
