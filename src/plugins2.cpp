// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "drivelst.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "toolbar.h"
#include "zip.h"
#include "pack.h"
#include "dialogs.h"

// hlavicka pro ukladadni DIBu do registry

#define DIB_METHOD_STORE 1   // prime ulozeni 1:1
#define DIB_METHOD_HUFFMAN 2 // static huffman codec

struct CDIBHeader
{
    BYTE Method; // DIB_METHOD_xxx
    WORD bmWidth;
    WORD bmHeight;
    WORD bmWidthBytes;
    BYTE bmPlanes;
    BYTE bmBitsPixel;
};

//****************************************************************************
//
// CreateGrayscaleAndMaskBitmaps
//
// Vytvori novou bitmapu o hloubce 24 bitu, nakopiruje do ni zdrojovou
// bitmapu a prevede ji na stupne sedi (transparentni barvu necha nedotcenou).
//

BOOL CreateGrayscaleDIB(HBITMAP hSource, COLORREF transparent, HBITMAP& hGrayscale)
{
    CALL_STACK_MESSAGE1("CreateGrayscaleDIB(, , ,)");
    BOOL ret = FALSE;
    VOID* lpvBits = NULL;
    hGrayscale = NULL;
    HDC hDC = HANDLES(GetDC(NULL));

    // vytahnu rozmery bitmapy
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // nechceme paletu

    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    if (bi.bmiHeader.biSizeImage == 0)
    {
        TRACE_E("bi.bmiHeader.biSizeImage = 0");
        goto exitus;
    }

    // pozadovana barevna hloubka je 24 bitu
    bi.bmiHeader.biSizeImage = ((((bi.bmiHeader.biWidth * 24) + 31) & ~31) >> 3) * bi.bmiHeader.biHeight;
    // naalokuju potrebny prostor
    lpvBits = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBits == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }

    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    // vytahnu vlastni data
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBits,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // prevedu na grayscale
    BYTE* rgb;
    rgb = (BYTE*)lpvBits;
    int i;
    for (i = 0; i < bi.bmiHeader.biWidth * bi.bmiHeader.biHeight; i++)
    {
        BYTE r = rgb[2];
        BYTE g = rgb[1];
        BYTE b = rgb[0];
        if (transparent != RGB(r, g, b))
        {
            BYTE brightness = GetGrayscaleFromRGB(r, g, b);
            rgb[0] = rgb[1] = rgb[2] = brightness;
        }
        rgb += 3;
    }

    // vytvorim novou bitmapu nad grayscale datama
    hGrayscale = HANDLES(CreateDIBitmap(hDC,
                                        &bi.bmiHeader,
                                        (LONG)CBM_INIT,
                                        lpvBits,
                                        &bi,
                                        DIB_RGB_COLORS));
    if (hGrayscale == NULL)
    {
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    ret = TRUE;
exitus:
    if (lpvBits != NULL)
        free(lpvBits);
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));
    return ret;
}

BOOL SaveIconList(HKEY hKey, const char* valueName, CIconList* iconList)
{
    BYTE* rawPNG;
    DWORD rawPNGSize;
    if (iconList->SaveToPNG(&rawPNG, &rawPNGSize))
    {
        LONG res = RegSetValueEx(hKey, valueName, 0, REG_BINARY, rawPNG, rawPNGSize);
        free(rawPNG);
        return TRUE;
    }
    else
        return FALSE;
}

BOOL LoadIconList(HKEY hKey, const char* valueName, CIconList** iconList)
{
    DWORD gettedType;
    DWORD bufferSize;
    LONG res = SalRegQueryValueEx(hKey, valueName, 0, &gettedType, NULL, &bufferSize);
    if (res != ERROR_SUCCESS || gettedType != REG_BINARY)
        return FALSE;

    BYTE* buff = (BYTE*)malloc(bufferSize);

    DWORD bufferSize2 = bufferSize;
    res = SalRegQueryValueEx(hKey, valueName, 0, &gettedType, buff, &bufferSize2);
    if (res != ERROR_SUCCESS || bufferSize2 != bufferSize)
    {
        free(buff);
        return FALSE;
    }

    *iconList = new CIconList();
    //(*iconList)->Dump = TRUE;
    BOOL ret = (*iconList)->CreateFromRawPNG(buff, bufferSize2, 16);
    free(buff);
    return ret;
}

/*
BOOL
SaveDIB(HKEY hKey, const char *valueName, HBITMAP hBitmap)
{
  BITMAP bmp;
  if (GetObject(hBitmap, sizeof(bmp), &bmp) == 0 || bmp.bmBits == NULL)
  {
    TRACE_E("GetObject failed");
    return FALSE;
  }

  DWORD buffSize = sizeof(CDIBHeader) + bmp.bmWidthBytes * bmp.bmHeight;
  BYTE *buff = (BYTE*)malloc(buffSize);
  if (buff == NULL)
  {
    TRACE_E(LOW_MEMORY);
    return FALSE;
  }

  CDIBHeader *hdr = (CDIBHeader*)buff;
  hdr->Method = DIB_METHOD_HUFFMAN;
  hdr->bmWidth = (BYTE)bmp.bmWidth;
  hdr->bmHeight = (WORD)bmp.bmHeight;
  hdr->bmWidthBytes = (WORD)bmp.bmWidthBytes;
  hdr->bmPlanes = (BYTE)bmp.bmPlanes;
  hdr->bmBitsPixel = (BYTE)bmp.bmBitsPixel;

  // pokusime se data komprimovat pomoci codecu
  CStaticHuffmanCodec codec;
  DWORD compressed;
  compressed = codec.EncodeBuffer((const BYTE *)bmp.bmBits,
                                  buff + sizeof(CDIBHeader),
                                  bmp.bmWidthBytes * bmp.bmHeight);
  if (compressed == 0)
  {
    hdr->Method = DIB_METHOD_STORE;
    compressed = bmp.bmWidthBytes * bmp.bmHeight;
    memcpy(buff + sizeof(CDIBHeader), bmp.bmBits, compressed);
  }

  LONG res = RegSetValueEx(hKey, valueName, 0, REG_BINARY, buff, sizeof(CDIBHeader) + compressed);
  delete(buff);

  return TRUE;
}

BOOL
LoadDIB(HKEY hKey, const char *valueName, HBITMAP *hBitmap)
{
  DWORD gettedType;
  DWORD bufferSize;
  LONG res = SalRegQueryValueEx(hKey, valueName, 0, &gettedType, NULL, &bufferSize);
  if (res != ERROR_SUCCESS || gettedType != REG_BINARY)
    return FALSE;

  BYTE *buff = (BYTE*)malloc(bufferSize + 4);  // +4 kvuli tomu, ze CStaticHuffmanCodec::DecodeBuffer() cte 4 byty za buffer (hodnoty sice nevyuziva, ale cteni mimo alokovany buffer nekdy zpusobovalo exceptionu)
  if (buff == NULL)
  {
    TRACE_E(LOW_MEMORY);
    return FALSE;
  }
  *(DWORD *)(buff + bufferSize) = 0;  // nulovani presahu bufferu, jen aby tam nebyly nahodne hodnoty
  DWORD bufferSize2 = bufferSize;
  res = SalRegQueryValueEx(hKey, valueName, 0, &gettedType, buff, &bufferSize2);
  if (res != ERROR_SUCCESS || bufferSize2 != bufferSize)
  {
    free(buff);
    return FALSE;
  }
  CDIBHeader *hdr = (CDIBHeader*)buff;
  if (hdr->Method != DIB_METHOD_STORE && hdr->Method != DIB_METHOD_HUFFMAN)
  {
    TRACE_E("LoadDIB: unknown CDIBHeader Method");
    free(buff);
    return FALSE;
  }

  BITMAPINFO *bi = (BITMAPINFO*) calloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD), 1);
  if (bi == NULL)
  {
    TRACE_E(LOW_MEMORY);
    free(buff);
    return FALSE;
  }

  bi->bmiHeader.biSize = sizeof(bi->bmiHeader);
  bi->bmiHeader.biWidth = hdr->bmWidth;
  bi->bmiHeader.biHeight = -hdr->bmHeight;  // top-down
  bi->bmiHeader.biPlanes = hdr->bmPlanes;
  bi->bmiHeader.biBitCount = hdr->bmBitsPixel;
  bi->bmiHeader.biCompression = BI_RGB;
  bi->bmiHeader.biClrUsed = 256;
  memcpy(bi->bmiColors, ColorTable, 256 * sizeof(RGBQUAD));

  void *lpBits = NULL;
  HBITMAP hBmp = HANDLES(CreateDIBSection(NULL, bi, DIB_RGB_COLORS, &lpBits, NULL, 0));
  free(bi);
  if (hBmp == NULL)
  {
    TRACE_E("CreateDIBSection failed");
    free(buff);
    return FALSE;
  }

  if (hdr->Method == DIB_METHOD_STORE)
    memcpy(lpBits, buff + sizeof(CDIBHeader), hdr->bmWidthBytes * hdr->bmHeight);
  else
  {
    // DIB_METHOD_HUFFMAN
    CStaticHuffmanCodec codec;
    DWORD uncompressed;
    uncompressed = codec.DecodeBuffer((const BYTE *)buff + sizeof(CDIBHeader),
                                      bufferSize2 - sizeof(CDIBHeader),
                                      (BYTE*)lpBits,
                                      hdr->bmWidthBytes * hdr->bmHeight);
    if (uncompressed == 0 ||
        uncompressed != (DWORD)(hdr->bmWidthBytes * hdr->bmHeight))
    {
      TRACE_E("Error in decompression of DIB (Huffman)");
      free(buff);
      DeleteObject(hBmp);
      return FALSE;
    }
  }
  free(buff);
  *hBitmap = hBmp;

  return TRUE;
}
*/
/*
#define ROP_DSna 0x00220326
HICON GetIconFromDIB(HBITMAP hBitmap, int index)
{
  // vytvorime B&W masku + barevnou bitmapu dle obrazovky
  HDC hDC = HANDLES(GetDC(NULL));
  HBITMAP hMask = HANDLES(CreateBitmap(16, 16, 1, 1, NULL));
  HBITMAP hColor = HANDLES(CreateCompatibleBitmap(hDC, 16, 16));
  HANDLES(ReleaseDC(NULL, hDC));

  hDC = HANDLES(CreateCompatibleDC(NULL));
  SelectObject(hDC, hColor);

  // pujcime si hDCMask pro nakopirovani zdrojove bitmapy do hDC
  HDC hDCMask = HANDLES(CreateCompatibleDC(NULL));
  SelectObject(hDCMask, hBitmap);
  BitBlt(hDC, 0, 0, 16, 16, hDCMask, 0, 0, SRCCOPY);

  // do hDCMask vybereme masku
  SelectObject(hDCMask, hMask);

  // fialova je transparentni barva
  SetBkColor(hDC, RGB(255, 0, 255));

  BitBlt(hDCMask, 0, 0, 16, 16, hDC, 0, 0, SRCCOPY);
  BitBlt(hDC, 0, 0, 16, 16, hDCMask, 0, 0, ROP_DSna);

  HANDLES(DeleteDC(hDC));
  HANDLES(DeleteDC(hDCMask));

  ICONINFO ii;
  ii.fIcon    = TRUE;
  ii.xHotspot = 0;
  ii.yHotspot = 0;
  ii.hbmColor = hColor;
  ii.hbmMask  = hMask;
  HICON hIcon = HANDLES(CreateIconIndirect(&ii));

  HANDLES(DeleteObject(hColor));
  HANDLES(DeleteObject(hMask));

  return hIcon;
}
*/
/* tato verze neslapala pod starsimi Windows (pred W2K), ikonky mely pruhledne oblasti
HICON GetIconFromDIB(HBITMAP hBitmap, int index)
{
  // barevna bitmapa musi byt kompatibilni s obrazovkou
  HDC hDC = HANDLES(GetDC(NULL));
  HBITMAP hMask = HANDLES(CreateBitmap(16, 16, 1, 1, NULL)); // maska musi byt b&w
  HBITMAP hColor = HANDLES(CreateCompatibleBitmap(hDC, 16, 16));
  HANDLES(ReleaseDC(NULL, hDC));

  HDC hSrcDC = HANDLES(CreateCompatibleDC(NULL));
  HBITMAP hOldSrcBmp = (HBITMAP)SelectObject(hSrcDC, hBitmap);

  HDC hDstDC = HANDLES(CreateCompatibleDC(NULL));
  HBITMAP hOldDstBmp = (HBITMAP)SelectObject(hDstDC, hMask);

  // maska: fialova bude bila, ostatni cerne
  SetBkColor(hSrcDC, RGB(255, 0, 255));
  BitBlt(hDstDC, 0, 0, 16, 16, hSrcDC, 0, 0, SRCCOPY);

  // barevna cast: preneseme barvy
  SelectObject(hDstDC, hColor);
  BitBlt(hDstDC, 0, 0, 16, 16, hSrcDC, 0, 0, SRCCOPY);
  // maskovanou cast musime vycernit
  SelectObject(hSrcDC, hMask);
  BitBlt(hDstDC, 0, 0, 16, 16, hSrcDC, 0, 0, 0x00220326); // "DSna" Ternary Raster Operations

  SelectObject(hDstDC, hOldDstBmp);
  HANDLES(DeleteDC(hDstDC));

  SelectObject(hSrcDC, hOldSrcBmp);
  HANDLES(DeleteDC(hSrcDC));

  ICONINFO ii;
  ii.fIcon = TRUE;
  ii.xHotspot = 0;
  ii.yHotspot = 0;
  ii.hbmMask = hMask;
  ii.hbmColor = hColor;
  HICON hIcon = HANDLES(CreateIconIndirect(&ii));
  HANDLES(DeleteObject(hMask));
  HANDLES(DeleteObject(hColor));
  return hIcon;
}

*/

//
// ****************************************************************************
// CPlugins
//

CPlugins::~CPlugins()
{
    int i;
    for (i = 0; i < Order.Count; i++)
        free(Order[i].DLLName);
    Order.DetachMembers();

    if (LastPlgCmdPlugin != NULL)
    {
        free(LastPlgCmdPlugin);
        LastPlgCmdPlugin = NULL;
    }
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->GetLoaded())
        {
            TRACE_E("Plugin " << Data[i]->Name << " is still loaded!");
        }
    }
    HANDLES(DeleteCriticalSection(&DataCS));
}

BOOL CPlugins::IsPluginFS(const char* fsName, int& index, int& fsNameIndex)
{
    index = -1;
    fsNameIndex = -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->SupportFS)
        {
            int j;
            for (j = 0; j < p->FSNames.Count; j++)
            {
                if (StrICmp(p->FSNames[j], fsName) == 0)
                {
                    index = i;
                    fsNameIndex = j;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

BOOL CPlugins::AreFSNamesFromSamePlugin(const char* fsName1, const char* fsName2, int& fsName2Index)
{
    fsName2Index = -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->SupportFS)
        {
            int j;
            for (j = 0; j < p->FSNames.Count; j++)
            {
                if (StrICmp(p->FSNames[j], fsName1) == 0) // nalezeno fsName1
                {
                    int k;
                    for (k = 0; k < p->FSNames.Count; k++)
                    {
                        if (StrICmp(p->FSNames[k], fsName2) == 0)
                        {
                            fsName2Index = k;
                            return TRUE; // ve stejnem pluginu bylo nalezeno i fsName2
                        }
                    }
                    return FALSE;
                }
            }
        }
    }
    return FALSE;
}

BOOL CPlugins::FindLastCommand(int* pluginIndex, int* menuItemIndex, BOOL rebuildDynMenu, HWND parent)
{
    // musime znat cestu k pluginu, kteremu patril posledni command
    if (LastPlgCmdPlugin != NULL)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CPluginData* p = Data[i];
            // najdeme plugin
            if (stricmp(p->DLLName, LastPlgCmdPlugin) == 0)
            {
                if (p->GetLoaded()) // pokud plugin neni loaded, budeme se tvarit ze jsme nic nenasli
                {
                    // last-cmd z dynamickeho menu: nutny rebuild, abysme ukazovali polozku z aktualizovane
                    // verze menu (jinak po otevreni submenu a rebuildu menu by mohla last-cmd polozka zaniknout)
                    if (!rebuildDynMenu || !p->SupportDynMenuExt || p->BuildMenu(parent, FALSE))
                    {
                        // najdeme LastPlgCmdID v menu pluginu
                        int j;
                        for (j = 0; j < p->MenuItems.Count; j++)
                        {
                            CPluginMenuItem* menuItem = p->MenuItems[j];
                            if (menuItem->ID == LastPlgCmdID)
                            {
                                *pluginIndex = i;
                                *menuItemIndex = j;
                                return TRUE;
                            }
                        }
                    }
                }
                break; // dalsi shoda DLLName uz nehrozi
            }
        }
        // prikaz nebyl nalezen, vycistime Last Command polozku, aby nedochazelo k "ozivani" starych prikazu po znovunaloadeni pluginu nebo po znovupridani polozky do dynamickeho menu
        if (LastPlgCmdPlugin != NULL)
            free(LastPlgCmdPlugin);
        LastPlgCmdPlugin = NULL;
    }
    return FALSE;
}

BOOL CPlugins::OnLastCommand(CFilesWindow* panel, HWND parent)
{
    int pluginIndex;
    int menuItemIndex;
    if (FindLastCommand(&pluginIndex, &menuItemIndex, FALSE, parent)) // pokud plugin neni loaded, vrati FALSE
    {
        // OnLastCommand muze byt zavolan pres Ctrl+I, takze vubec nemuselo byt zobrazeno menu
        // a nemuseji tedy byt napocitany stavy cache
        CalculateStateCache();

        CPluginData* pluginData = Data[pluginIndex];

        MENU_ITEM_INFO dummy;
        if (pluginData->GetMenuItemStateType(pluginIndex, menuItemIndex, &dummy))
        {
            BOOL unselect;
            if (pluginData->ExecuteMenuItem2(panel, parent, pluginIndex, LastPlgCmdID, unselect))
            {
                return unselect; // zpracovano, koncime
            }
        }
    }
    return FALSE;
}

BOOL CPlugins::ExecuteCommand(int pluginIndex, int menuItemIndex, CFilesWindow* panel, HWND parent)
{
    CalculateStateCache();

    CPluginData* pluginData = Data[pluginIndex];

    if (pluginData->InitDLL(parent)) // pro volani GetMenuItemStateType() potrebujeme mit inicializovano PluginIfaceForMenuExt
    {
        MENU_ITEM_INFO dummy;
        if (pluginData->GetMenuItemStateType(pluginIndex, menuItemIndex, &dummy))
        {
            int id = pluginData->MenuItems[menuItemIndex]->ID;
            BOOL unselect;
            if (pluginData->ExecuteMenuItem2(panel, parent, pluginIndex, id, unselect))
            {
                return unselect; // zpracovano, koncime
            }
        }
    }
    return FALSE;
}

void CPlugins::InitMenuItems(HWND parent, CMenuPopup* root)
{
    // build jednotlivych submenu pro plug-iny

    // promazneme SUID a povolime pripadne volani BuildMenu() ve vsech menu
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        Data[i]->ClearSUID();
        Data[i]->DynMenuWasAlreadyBuild = FALSE;
    }

    LastSUID = CM_PLUGINCMD_MIN; // inicializujeme pocitadlo pro generovani SUID

    // vycistime polozky od minula nebo zjistime puvodni pocet polozek v root menu
    int count = root->GetItemCount();
    if (RootMenuItemsCount == -1)
        RootMenuItemsCount = count;
    if (count > RootMenuItemsCount)
        if (!root->RemoveItemsRange(RootMenuItemsCount, count - 1))
            return; // chyba

    // napridavame submenu pro plug-iny, ktere maji nejake polozky, vycistime SUID polozek
    // (budou se pridelovat nova cisla)
    count = RootMenuItemsCount;
    UpdatePluginsOrder(Configuration.KeepPluginsSorted);
    for (i = 0; i < Order.Count; i++)
    {
        int orderIndex = Order[i].Index;
        CPluginData* p = Data[orderIndex];
        p->SubMenu = NULL;                                  // prozatim tento plug-in zadne submenu nema
        if (p->SupportDynMenuExt || p->MenuItems.Count > 0) // dynamicke menu nebo ma nejake polozky
        {
            // diky SkillLevel redukci nemusi v submenu nic zbyt a prazdne submenu nechceme
            BOOL containVisibleItem = p->SupportDynMenuExt;
            if (!containVisibleItem)
            {
                int j;
                for (j = 0; j < p->MenuItems.Count; j++)
                {
                    if (p->MenuItems[j]->SkillLevel & CfgSkillLevelToMenu(Configuration.SkillLevel))
                    {
                        containVisibleItem = TRUE;
                        break;
                    }
                    else
                    {
                        if (p->MenuItems[j]->Type == pmitStartSubmenu)
                        { // pokud jde o submenu, musime preskocit vsechny jeho polozky a submenu
                            int level = 1;
                            for (j++; j < p->MenuItems.Count; j++)
                            {
                                CPluginMenuItemType type = p->MenuItems[j]->Type;
                                if (type == pmitStartSubmenu)
                                    level++;
                                else
                                {
                                    if (type == pmitEndSubmenu && --level == 0)
                                        break; // konec submenu nalezen
                                }
                            }
                        }
                    }
                }
            }
            if (containVisibleItem)
            {
                MENU_ITEM_INFO mi;
                if (count == RootMenuItemsCount && RootMenuItemsCount != 0)
                { // prvni submenu -> je treba pridat separator
                    mi.Mask = MENU_MASK_TYPE;
                    mi.Type = MENU_TYPE_SEPARATOR;
                    root->InsertItem(count++, TRUE, &mi);
                }

                // vlozime prazdne subMenu plug-inu
                mi.Mask = MENU_MASK_TYPE | MENU_MASK_SUBMENU | MENU_MASK_STRING |
                          MENU_MASK_IMAGEINDEX | MENU_MASK_ID;
                mi.Type = MENU_TYPE_STRING;

#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
                if (IsPluginUnsupportedOnX64(p->DLLName))
                {
                    mi.Mask |= MENU_MASK_STATE;
                    mi.State = MENU_STATE_GRAYED;
                }
#endif // _WIN64

                char pluginName[300];
                lstrcpyn(pluginName, p->Name, 299);
                DuplicateAmpersands(pluginName, 299); // nazev pluginu muze obsahovat znak '&'

                mi.String = pluginName;
                mi.ImageIndex = (p->PluginSubmenuIconIndex != -1) ? orderIndex : -1;
                mi.SubMenu = new CMenuPopup();
                p->SubMenu = (CMenuPopup*)mi.SubMenu; // toto submenu pridelime tomuto plug-inu
                mi.ID = CML_PLUGINS_SUBMENU;
                root->InsertItem(count++, TRUE, &mi);
            }
        }
    }

    // napocteme novy CPluginsStateCache::ActualStateMask a ostatni promenne (pokud to ma cenu)
    // pozdeji z nich bude CPluginData::GetMaskForMenuItems urcovat masku
    CalculateStateCache();

    // nastavime polozku Last Command
    char lastCmdStr[800];
    MENU_ITEM_INFO lcmii;
    lcmii.Type = MENU_TYPE_STRING;
    lcmii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_STATE | MENU_MASK_FLAGS;
    lcmii.Flags = MENU_FLAG_NOHOTKEY; // nechceme, aby teto polozce AssignHotKeys priradilo hot key
    lcmii.String = lastCmdStr;
    BOOL setToDefaultItem = TRUE;
    int pluginIndex;
    int menuItemIndex;
    if (FindLastCommand(&pluginIndex, &menuItemIndex, TRUE, parent))
    {
        CPluginData* pluginData = Data[pluginIndex];
        CPluginMenuItem* menuItem = pluginData->MenuItems[menuItemIndex];
        if (menuItem->Name != NULL) // polozku zobrazim i kdyz neodpovida soucasnemu skill levelu
        {
            lcmii.State = 0;
            pluginData->GetMenuItemStateType(pluginIndex, menuItemIndex, &lcmii);

            lstrcpyn(lastCmdStr, pluginData->Name, 299);
            char* s = strchr(lastCmdStr, '('); // zahodime ze jmena pluginu text v zavorce ("WinSCP (SFTP/SCP Client)" -> "WinSCP")
            if (s != NULL)
            {
                char* e = strchr(s + 1, ')');
                if (s > lastCmdStr && *(s - 1) == ' ')
                    s--;
                if (e != NULL)
                    memmove(s, e + 1, strlen(e + 1) + 1);
            }
            DuplicateAmpersands(lastCmdStr, 299); // nazev pluginu muze obsahovat znak '&'
            strcat(lastCmdStr, ": ");
            int cmdNameOffset = (int)strlen(lastCmdStr);
            strcpy(lastCmdStr + cmdNameOffset, menuItem->Name);

            // pokud mame hint v textu, odstranime ho
            if ((menuItem->HotKey & HOTKEY_HINT) != 0)
            {
                char* p = lastCmdStr + cmdNameOffset;
                while (*p != 0)
                {
                    if (*p == '\t')
                    {
                        *p = 0;
                        break;
                    }
                    p++;
                }
            }

            // pripojime hot-key z originalniho stringu
            const char* hotKey = LoadStr(IDS_MENU_PLG_LASTCMD);
            while (*hotKey != 0 && *hotKey != '\t')
                hotKey++;
            strcat(lastCmdStr, hotKey);
            // odstranime ampersand, aby nam nerozhodil hot keys pro pluginy
            RemoveAmpersands(lastCmdStr + cmdNameOffset);
            DuplicateAmpersands(lastCmdStr + cmdNameOffset, 500); // pokud command obsahoval &&, musime ho vratit
            setToDefaultItem = FALSE;
        }
    }
    if (setToDefaultItem)
    {
        // stary command neni k dispozici, soupneme tam (disabled) default polozku
        lcmii.State = MENU_STATE_GRAYED;
        strcpy(lastCmdStr, LoadStr(IDS_MENU_PLG_LASTCMD));
    }
    root->SetItemInfo(CM_LAST_PLUGIN_CMD, FALSE, &lcmii);
}

BOOL CPlugins::InitPluginsBar(CToolBar* bar)
{
    UpdatePluginsOrder(Configuration.KeepPluginsSorted);
    TLBI_ITEM_INFO2 tii;
    int i;
    for (i = 0; i < Order.Count; i++)
    {
        int orderIndex = Order[i].Index;
        CPluginData* plugin = Plugins.Get(orderIndex);
        if (plugin == NULL || plugin->MenuItems.Count == 0 && !plugin->SupportDynMenuExt || !GetShowInBar(orderIndex))
            continue;

        tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID;
        tii.Style = TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN;
        tii.ImageIndex = orderIndex;
        tii.ID = CM_PLUGINCMD_MIN + orderIndex; // do mainwnd3 prijde jako WM_USER_TBDROPDOWN
        bar->InsertItem2(0xFFFFFFFF, TRUE, &tii);
    }
    return TRUE;
}

BOOL CPlugins::InitPluginMenuItemsForBar(HWND parent, int index, CMenuPopup* menu)
{
    CPluginData* plugin = Plugins.Get(index);
    if (plugin == NULL)
    {
        TRACE_E("Plugin does not exist, index=" << index);
        return FALSE;
    }
    // promazneme SUID a povolime pripadne volani BuildMenu() ve vsech menu
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        Data[i]->ClearSUID();
        Data[i]->DynMenuWasAlreadyBuild = FALSE;
    }

    LastSUID = CM_PLUGINCMD_MIN; // inicializujeme pocitadlo pro generovani SUID
    CalculateStateCache();
    if (menu->GetPopupID() != CML_PLUGINS_SUBMENU)
        TRACE_E("CPlugins::InitPluginMenuItemsForBar() internal warning: wrong menu ID in, imagelists will not be destroyed!");
    plugin->InitMenuItems(parent, index, menu);
    menu->SetImageList(plugin->CreateImageList(TRUE), TRUE); // destrukce imagelistu se provede v CMainWindow::WindowProc / WM_USER_UNINITMENUPOPUP (menu musi mit ID CML_PLUGINS_SUBMENU)
    menu->SetHotImageList(plugin->CreateImageList(FALSE), TRUE);
    plugin->ReleasePluginDynMenuIcons(); // ikony jsou v imagelistech menu a tenhle objekt uz nikdo nepotrebuje (pro dalsi zobrazeni menu se vse ziska znovu)
    return TRUE;
}

void CPlugins::CalculateStateCache()
{
    StateCache.Clean();
    StateCache.ActualStateMask = MENU_EVENT_TRUE;
    //  if (count > RootMenuItemsCount)
    //  {
    // MENU_EVENT_THIS_PLUGIN_XXX a MENU_EVENT_TARGET_THIS_PLUGIN_XXX se nastavuji pro
    // kazdy plug-in zvlast v CPluginData::InitMenuItems

    // MENU_EVENT_DISK, ActiveUnpackerIndex, ActivePackerIndex, ActiveFSIndex
    if (MainWindow->GetActivePanel()->Is(ptDisk))
        StateCache.ActualStateMask |= MENU_EVENT_DISK;
    else
    {
        if (MainWindow->GetActivePanel()->Is(ptZIPArchive))
        {
            int format = PackerFormatConfig.PackIsArchive(MainWindow->GetActivePanel()->GetZIPArchive());
            if (format != 0) // ne chyba
            {
                format--;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0)
                    StateCache.ActiveUnpackerIndex = -index - 1; // je to plug-in
                if (PackerFormatConfig.GetUsePacker(format))
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0)
                        StateCache.ActivePackerIndex = -index - 1; // je to plug-in
                }
            }
        }
        else
        {
            if (MainWindow->GetActivePanel()->Is(ptPluginFS))
            {
                CPluginFSInterfaceEncapsulation* fs = MainWindow->GetActivePanel()->GetPluginFS();
                if (fs->NotEmpty()) // melo by byt "always true"
                {                   // podle ifacu najdeme index plug-inu v akt. panelu
                    StateCache.ActiveFSIndex = Plugins.GetIndex(fs->GetPluginInterface());
                }
            }
        }
    }

    // MENU_EVENT_TARGET_DISK, NonactiveUnpackerIndex, NonactivePackerIndex, NonactiveFSIndex
    if (MainWindow->GetNonActivePanel()->Is(ptDisk))
        StateCache.ActualStateMask |= MENU_EVENT_TARGET_DISK;
    else
    {
        if (MainWindow->GetNonActivePanel()->Is(ptZIPArchive))
        {
            int format = PackerFormatConfig.PackIsArchive(MainWindow->GetNonActivePanel()->GetZIPArchive());
            if (format != 0) // ne chyba
            {
                format--;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0)
                    StateCache.NonactiveUnpackerIndex = -index - 1; // je to plug-in
                if (PackerFormatConfig.GetUsePacker(format))
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0)
                        StateCache.NonactivePackerIndex = -index - 1; // je to plug-in
                }
            }
        }
        else
        {
            if (MainWindow->GetNonActivePanel()->Is(ptPluginFS))
            {
                CPluginFSInterfaceEncapsulation* fs = MainWindow->GetNonActivePanel()->GetPluginFS();
                if (fs->NotEmpty()) // melo by byt "always true"
                {                   // podle ifacu najdeme index plug-inu v neakt. panelu
                    StateCache.NonactiveFSIndex = Plugins.GetIndex(fs->GetPluginInterface());
                }
            }
        }
    }

    // MENU_EVENT_SUBDIR
    BOOL upDir = (MainWindow->GetActivePanel()->Dirs->Count != 0 &&
                  strcmp(MainWindow->GetActivePanel()->Dirs->At(0).Name, "..") == 0);
    if (upDir)
        StateCache.ActualStateMask |= MENU_EVENT_SUBDIR;

    // MENU_EVENT_FILE_FOCUSED, MENU_EVENT_DIR_FOCUSED a MENU_EVENT_UPDIR_FOCUSED
    // + vypocet FileUnpackerIndex a FilePackerIndex
    int caret = MainWindow->GetActivePanel()->GetCaretIndex();
    if (caret >= 0)
    {
        if (caret == 0 && upDir)
            StateCache.ActualStateMask |= MENU_EVENT_UPDIR_FOCUSED;
        else
        {
            if (caret < MainWindow->GetActivePanel()->Dirs->Count)
                StateCache.ActualStateMask |= MENU_EVENT_DIR_FOCUSED;
            else
            {
                if (caret < MainWindow->GetActivePanel()->Dirs->Count +
                                MainWindow->GetActivePanel()->Files->Count)
                {
                    StateCache.ActualStateMask |= MENU_EVENT_FILE_FOCUSED;

                    CFileData* file = &MainWindow->GetActivePanel()->Files->At(caret -
                                                                               MainWindow->GetActivePanel()->Dirs->Count);

                    int format = PackerFormatConfig.PackIsArchive(file->Name);
                    if (format != 0) // ne chyba
                    {
                        format--;
                        int index = PackerFormatConfig.GetUnpackerIndex(format);
                        if (index < 0)
                            StateCache.FileUnpackerIndex = -index - 1; // je to plug-in
                        if (PackerFormatConfig.GetUsePacker(format))
                        {
                            index = PackerFormatConfig.GetPackerIndex(format);
                            if (index < 0)
                                StateCache.FilePackerIndex = -index - 1; // je to plug-in
                        }
                    }
                }
            }
        }
    }

    // MENU_EVENT_FILES_SELECTED a MENU_EVENT_DIRS_SELECTED
    int count = MainWindow->GetActivePanel()->GetSelCount();
    if (count > 0)
    {
        int* buf = (int*)malloc(sizeof(int) * count);
        if (buf != NULL)
        {
            if (MainWindow->GetActivePanel()->GetSelItems(count, buf) == count)
            {
                while (count-- > 0)
                {
                    if (buf[count] > 0 || buf[count] == 0 && !upDir)
                    {
                        if (buf[count] < MainWindow->GetActivePanel()->Dirs->Count)
                            StateCache.ActualStateMask |= MENU_EVENT_DIRS_SELECTED;
                        else
                            StateCache.ActualStateMask |= MENU_EVENT_FILES_SELECTED;
                    }
                }
            }
            free(buf);
        }
        else
            TRACE_E(LOW_MEMORY);
    }
    //  }
}

void CPlugins::InitSubMenuItems(HWND parent, CMenuPopup* submenu)
{
    // zkontrolujeme, jestli nelezou do nektereho submenu plug-inu
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->SubMenu == submenu) // inicializace submenu plug-inu
        {
            if (submenu->GetPopupID() != CML_PLUGINS_SUBMENU)
                TRACE_E("CPlugins::InitSubMenuItems() internal warning: wrong menu ID in, imagelists will not be destroyed!");
            p->InitMenuItems(parent, i, p->SubMenu);
            submenu->SetImageList(p->CreateImageList(TRUE), TRUE); // destrukce imagelistu se provede v CMainWindow::WindowProc / WM_USER_UNINITMENUPOPUP (menu musi mit ID CML_PLUGINS_SUBMENU)
            submenu->SetHotImageList(p->CreateImageList(FALSE), TRUE);
            break;
        }
    }
}

BOOL CPlugins::ExecuteMenuItem(CFilesWindow* panel, HWND parent, int suid)
{
    BOOL unselect;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* pluginData = Data[i];
        if (pluginData->ExecuteMenuItem(panel, parent, i, suid, unselect))
            return unselect; // zpracovano, koncime
    }
    return FALSE;
}

BOOL CPlugins::HelpForMenuItem(HWND parent, int suid)
{
    BOOL helpDisplayed;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* pluginData = Data[i];
        if (pluginData->HelpForMenuItem(parent, i, suid, helpDisplayed))
            return helpDisplayed; // zpracovano, koncime
    }
    return FALSE;
}

HIMAGELIST
CPlugins::CreateIconsList(BOOL gray)
{
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    HIMAGELIST hIL = ImageList_Create(iconSize, iconSize, GetImageListColorFlags() | ILC_MASK, 0, 1);
    if (hIL != NULL)
    {
        HICON hDefIcon = SalLoadIcon(HInstance, IDI_PLUGIN, iconSize);
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CPluginData* p = Data[i];
            if (p->PluginIcons != NULL && p->PluginIconIndex != -1)
            {
                CIconList* iconList = gray ? p->PluginIconsGray : p->PluginIcons;
                if (p->PluginIconsGray == NULL)
                    iconList = p->PluginIcons;
                HICON hIcon = iconList->GetIcon(p->PluginIconIndex, TRUE);
                ImageList_AddIcon(hIL, hIcon);
                HANDLES(DestroyIcon(hIcon));
            }
            else
                ImageList_AddIcon(hIL, hDefIcon);
        }
        HANDLES(DestroyIcon(hDefIcon));
    }
    return hIL;
}

void CPlugins::AddNamesToListView(HWND hListView, BOOL setOnly, int* numOfLoaded)
{
    CALL_STACK_MESSAGE3("CPlugins::AddNamesToListView(0x%p, %d)", hListView, setOnly);

    // priprava pole Order
    UpdatePluginsOrder(Configuration.KeepPluginsSorted);

    if (!setOnly)
        ListView_DeleteAllItems(hListView);

    int loaded = 0;
    int i;
    for (i = 0; i < Order.Count; i++)
    {
        int orderIndex = Order[i].Index;
        CPluginData* plugin = Data[orderIndex];
        if (plugin->GetLoaded())
            loaded++;
        if (!setOnly)
        {
            LVITEM lvi;
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            char buffEmpty[] = "";
            lvi.pszText = buffEmpty;
            ListView_InsertItem(hListView, &lvi);
        }
        // icon
        LVITEM lvi;
        lvi.mask = LVIF_IMAGE;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.iImage = orderIndex;
        ListView_SetItem(hListView, &lvi);
        // plugin name
        ListView_SetItemText(hListView, i, 0, plugin->Name);
        // loaded
        ListView_SetItemText(hListView, i, 1,
                             LoadStr(plugin->GetLoaded() ? IDS_PLUGINS_LOADED_YES : IDS_PLUGINS_LOADED_NO));
        // version
        ListView_SetItemText(hListView, i, 2, plugin->Version);
        // location
        ListView_SetItemText(hListView, i, 3, plugin->DLLName);
    }
    *numOfLoaded = loaded;
}

void CPlugins::AddThumbLoaderPlugins(TIndirectArray<CPluginData>& thumbLoaderPlugins)
{
    CALL_STACK_MESSAGE1("CPlugins::AddThumbLoaderPlugins()");
    if (MainWindow != NULL && !MainWindow->DoNotLoadAnyPlugins)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CPluginData* data = Data[i];
            if (data->ThumbnailMasks.GetMasksString()[0] != 0 && // poskytuje thumbnaily
                !data->ThumbnailMasksDisabled)                   // neprobiha jeho unload/remove
            {
                thumbLoaderPlugins.Add(data);
                if (!thumbLoaderPlugins.IsGood())
                {
                    // thumbLoaderPlugins.ResetState();  // stav pole se nuluje mimo metodu (pouziva se jako detekce chyby)
                    break;
                }
            }
        }
    }
}

BOOL CPlugins::AddNamesToMenu(CMenuPopup* menu, DWORD firstID, int maxCount, BOOL configurableOnly)
{
    CALL_STACK_MESSAGE5("CPlugins::AddNamesToMenu(0x%p, %u, %d, %d)", menu, firstID, maxCount, configurableOnly);
    UpdatePluginsOrder(Configuration.KeepPluginsSorted);
    int count = min(Order.Count, maxCount);
    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_IMAGEINDEX;
    mii.Type = MENU_TYPE_STRING;
    int i;
    for (i = 0; i < count; i++)
    {
        int orderIndex = Order[i].Index;
        CPluginData* data = Data[orderIndex];
        if (!configurableOnly || data->SupportConfiguration)
        {
            mii.ID = firstID + orderIndex;

            char pluginName[300];
            lstrcpyn(pluginName, data->Name, 299);
            DuplicateAmpersands(pluginName, 299); // nazev pluginu muze obsahovat znak '&'

            mii.String = (LPTSTR)pluginName;
            mii.ImageIndex = (data->PluginIconIndex != -1) ? orderIndex : -1;
            menu->InsertItem(0xFFFFFFFF, TRUE, &mii);
        }
    }
    if (menu->GetItemCount() == 0)
    {
        mii.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_STRING;
        mii.Type = MENU_TYPE_STRING;
        mii.State = MENU_STATE_GRAYED;
        mii.String = LoadStr(configurableOnly ? IDS_EMPTYPLUGINSMENU2 : IDS_EMPTYPLUGINSMENU1);
        menu->InsertItem(0xFFFFFFFF, TRUE, &mii);
        return FALSE; // potlacime prirazeni horkych klaves
    }
    return TRUE; // pro pridane polozky chceme hot keys
}

BOOL CPlugins::AddItemsToChangeDrvMenu(CDrivesList* drvList, int& currentFSIndex,
                                       CPluginInterfaceForFSAbstract* ifaceForFS,
                                       BOOL getGrayIcons)
{
    CDriveData drv;
    drv.DriveType = drvtPluginCmd;
    drv.Param = 0;
    drv.Accessible = FALSE;
    drv.Shared = FALSE;
    drv.PluginFS = NULL; // jen pro poradek

    UpdatePluginsOrder(Configuration.KeepPluginsSorted);
    int i;
    for (i = 0; i < Order.Count; i++)
    {
        int orderIndex = Order[i].Index;
        CPluginData* p = Data[orderIndex];
        if (p->SupportFS && p->ChDrvMenuFSItemName != NULL && p->ChDrvMenuFSItemVisible)
        {
            drv.DriveText = DupStr(p->ChDrvMenuFSItemName);
            if (drv.DriveText == NULL)
                return FALSE;
            if (p->PluginIcons != NULL && p->ChDrvMenuFSItemIconIndex != -1)
            {
                drv.HIcon = p->PluginIcons->GetIcon(p->ChDrvMenuFSItemIconIndex, TRUE);
                if (getGrayIcons && p->PluginIconsGray != NULL)
                    drv.HGrayIcon = p->PluginIconsGray->GetIcon(p->ChDrvMenuFSItemIconIndex, TRUE);
                else
                    drv.HGrayIcon = NULL;
                drv.DestroyIcon = TRUE;
            }
            else
            {
                drv.HIcon = SalLoadIcon(HInstance, IDI_PLUGINFS, IconSizes[ICONSIZE_16]);
                drv.HGrayIcon = NULL;
                drv.DestroyIcon = TRUE;
            }
            drv.DLLName = p->DLLName;
            int index;
            drvList->AddDrive(drv, index);
            if (currentFSIndex == -1 && ifaceForFS != NULL &&
                p->GetPluginInterfaceForFS()->GetInterface() == ifaceForFS)
            {
                currentFSIndex = index;
            }
        }
    }
    return TRUE;
}

void CPlugins::OnPluginAbout(HWND hParent, int index)
{
    CPluginData* p = Get(index);
    if (p != NULL)
        p->About(hParent);
    else
        TRACE_E("Unexpected situation in CPlugins::OnPluginAbout.");
}

void CPlugins::OnPluginConfiguration(HWND hParent, int index)
{
    CPluginData* p = Get(index);
    if (p != NULL)
    {
        if (p->SupportConfiguration)
            p->Configuration(hParent);
    }
    else
        TRACE_E("Unexpected situation in CPlugins::OnPluginConfiguration.");
}

BOOL LoadFSNames(HKEY itemKey, TIndirectArray<char>* fsNames)
{
    char buf[1000];
    if (GetValue(itemKey, SALAMANDER_PLUGINS_FSNAME, REG_SZ, buf, 1000))
    {
        fsNames->DestroyMembers();
        char* s = buf;
        char* end = s;
        while (*end != 0)
        {
            while (*end != 0 && *end != ':')
                end++;
            if (end > s)
            {
                char* name = (char*)malloc((end - s) + 1);
                if (name != NULL)
                {
                    memcpy(name, s, end - s);
                    name[end - s] = 0;
                    fsNames->Add(name);
                    if (!fsNames->IsGood())
                    {
                        free(name);
                        fsNames->ResetState();
                        fsNames->DestroyMembers();
                        return FALSE;
                    }
                }
                else
                {
                    TRACE_E(LOW_MEMORY);
                    fsNames->DestroyMembers();
                    return FALSE;
                }
            }
            if (*end != 0)
                end++;
            s = end;
        }
        return TRUE;
    }
    else
        return FALSE;
}

void SaveFSNames(HKEY itemKey, TIndirectArray<char>* fsNames)
{
    char buf[1000];
    buf[0] = 0;
    int remainingSize = sizeof(buf); // ulozime do 'buf' seznam fs-names, jmena budou oddelena ':'
    int i;
    for (i = 0; remainingSize > 1 && i < fsNames->Count; i++)
    {
        int len = _snprintf_s(buf + (sizeof(buf) - remainingSize), remainingSize, _TRUNCATE,
                              (i + 1 != fsNames->Count) ? "%s:" : "%s", fsNames->At(i));
        if (len < 0)
        { // small buffer
            TRACE_E("Fatal error: small buffer for storing fs-names to registry!");
            buf[(sizeof(buf) - remainingSize)] = 0;
            if ((sizeof(buf) - remainingSize) > 0 && buf[(sizeof(buf) - remainingSize) - 1] == ':')
                buf[(sizeof(buf) - remainingSize) - 1] = 0;
            break;
        }
        remainingSize -= len;
    }
    SetValue(itemKey, SALAMANDER_PLUGINS_FSNAME, REG_SZ, buf, -1);
}

void CPlugins::Load(HWND parent, HKEY regKey)
{
    CALL_STACK_MESSAGE1("CPlugins::Load(,)");
    HANDLES(EnterCriticalSection(&DataCS));
    Data.DestroyMembers();
    HANDLES(LeaveCriticalSection(&DataCS));
    DefaultConfiguration = FALSE;
    if (regKey != NULL)
    {
        char pluginsDir[MAX_PATH];
        GetModuleFileName(HInstance, pluginsDir, MAX_PATH);
        char* s = strrchr(pluginsDir, '\\');
        if (s != NULL)
            strcpy(s + 1, "plugins");

        HKEY itemKey;
        char buf[30];
        int i = 1;
        strcpy(buf, "1");
        BOOL view, edit, pack, unpack, config, loadsave, viewer, fs, loadOnStart, dynMenuExt;
        char name[MAX_PATH];
        char dllName[MAX_PATH];
        char version[MAX_PATH];
        char copyright[MAX_PATH];
        char extensions[MAX_PATH];
        char description[MAX_PATH];
        char regKeyName[MAX_PATH];
        TIndirectArray<char> fsNames(1, 10);
        char fsCmdName[MAX_PATH];
        char lastSLGName[MAX_PATH];
        char pluginHomePageURL[MAX_PATH];
        char thumbnailMasks[MAX_GROUPMASK];
        CIconList* pluginIcons;
        int pluginIconIndex;
        int pluginSubmenuIconIndex;
        BOOL showSubmenuPluginsBar;
        while (OpenKey(regKey, buf, itemKey))
        {
            BOOL err = TRUE;
            BOOL ok = FALSE;
            loadOnStart = FALSE;
            thumbnailMasks[0] = 0;
            lastSLGName[0] = 0;
            pluginHomePageURL[0] = 0;
            pluginIcons = NULL;
            pluginIconIndex = -1;
            pluginSubmenuIconIndex = -1;
            showSubmenuPluginsBar = TRUE;
            if (Configuration.ConfigVersion < 7) // stara verze (ulozeni funkci po jednotlivych BOOLech)
            {
                ok = GetValue(itemKey, SALAMANDER_PLUGINS_NAME, REG_SZ, name, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_DLLNAME, REG_SZ, dllName, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_VERSION, REG_SZ, version, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_COPYRIGHT, REG_SZ, copyright, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_EXTENSIONS, REG_SZ, extensions, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_DESCRIPTION, REG_SZ, description, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_REGKEYNAME, REG_SZ, regKeyName, MAX_PATH) &&
                     LoadFSNames(itemKey, &fsNames) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_PANELVIEW, REG_DWORD, &view, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_PANELEDIT, REG_DWORD, &edit, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_CUSTPACK, REG_DWORD, &pack, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_CUSTUNPACK, REG_DWORD, &unpack, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_CONFIG, REG_DWORD, &config, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_LOADSAVE, REG_DWORD, &loadsave, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_VIEWER, REG_DWORD, &viewer, sizeof(DWORD)) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_FS, REG_DWORD, &fs, sizeof(DWORD));
                dynMenuExt = FALSE;
            }
            else // nova verze (ulozeni funkci do jednoho DWORDu po bitech)
            {
                DWORD functions = 0;
                ok = GetValue(itemKey, SALAMANDER_PLUGINS_NAME, REG_SZ, name, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_DLLNAME, REG_SZ, dllName, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_VERSION, REG_SZ, version, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_COPYRIGHT, REG_SZ, copyright, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_EXTENSIONS, REG_SZ, extensions, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_DESCRIPTION, REG_SZ, description, MAX_PATH) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_REGKEYNAME, REG_SZ, regKeyName, MAX_PATH) &&
                     LoadFSNames(itemKey, &fsNames) &&
                     GetValue(itemKey, SALAMANDER_PLUGINS_FUNCTIONS, REG_DWORD, &functions, sizeof(DWORD));

                view = (functions & FUNCTION_PANELARCHIVERVIEW) != 0;
                edit = (functions & FUNCTION_PANELARCHIVEREDIT) != 0;
                pack = (functions & FUNCTION_CUSTOMARCHIVERPACK) != 0;
                unpack = (functions & FUNCTION_CUSTOMARCHIVERUNPACK) != 0;
                config = (functions & FUNCTION_CONFIGURATION) != 0;
                loadsave = (functions & FUNCTION_LOADSAVECONFIGURATION) != 0;
                viewer = (functions & FUNCTION_VIEWER) != 0;
                fs = (functions & FUNCTION_FILESYSTEM) != 0;
                dynMenuExt = (functions & FUNCTION_DYNAMICMENUEXT) != 0;

                DWORD loadOnStartDWORD;
                if (GetValue(itemKey, SALAMANDER_PLUGINS_LOADONSTART, REG_DWORD, &loadOnStartDWORD, sizeof(DWORD)))
                {
                    loadOnStart = loadOnStartDWORD != 0;
                }

                // tyto hodnoty se nemusi nacist (muzou v konfiguraci chybet)
                GetValue(itemKey, SALAMANDER_PLUGINS_LASTSLGNAME, REG_SZ, lastSLGName, MAX_PATH);
                GetValue(itemKey, SALAMANDER_PLUGINS_HOMEPAGE, REG_SZ, pluginHomePageURL, MAX_PATH);
                GetValue(itemKey, SALAMANDER_PLUGINS_THUMBMASKS, REG_SZ, thumbnailMasks, MAX_GROUPMASK);
                GetValue(itemKey, SALAMANDER_PLUGINS_PLGICONINDEX, REG_DWORD, &pluginIconIndex, sizeof(DWORD));
                GetValue(itemKey, SALAMANDER_PLUGINS_PLGSUBMENUICONINDEX, REG_DWORD, &pluginSubmenuIconIndex, sizeof(DWORD));
                if (!GetValue(itemKey, SALAMANDER_PLUGINS_SUBMENUINPLUGINSBAR, REG_DWORD, &showSubmenuPluginsBar, sizeof(DWORD)))
                {
                    if (Configuration.ConfigVersion < 25)
                        showSubmenuPluginsBar = PluginVisibleInBar(dllName);
                }

                LoadIconList(itemKey, SALAMANDER_PLUGINS_PLGICONLIST, &pluginIcons);
            }
            if (ok)
            {
                char normalizedDLLName[MAX_PATH];
                if (StrNICmp(dllName, pluginsDir, (int)strlen(pluginsDir)) == 0 && dllName[(int)strlen(pluginsDir)] == '\\')
                {
                    memmove(normalizedDLLName, dllName + strlen(pluginsDir) + 1, strlen(dllName) - strlen(pluginsDir) + 1 - 1);
                }
                else
                    strcpy(normalizedDLLName, dllName);
                int dummyIndex;
                if (Plugins.FindDLL(normalizedDLLName, dummyIndex))
                {
                    err = FALSE; // sice je chyba, ale zkusime se z ni vzpamatovat
                }
                else
                {
                    if (AddPlugin(name, normalizedDLLName, view, edit, pack, unpack, config, loadsave, viewer, fs,
                                  dynMenuExt, version, copyright, description, regKeyName, extensions, &fsNames,
                                  loadOnStart, lastSLGName, pluginHomePageURL[0] != 0 ? pluginHomePageURL : NULL))
                    {
                        err = FALSE;
                        CPluginData* p = Get(Data.Count - 1);

                        if (p->PluginIcons != NULL)
                            delete p->PluginIcons; // jen pro jistotu (zatim je z konstruktoru bitmapa prazdna)
                        p->PluginIcons = pluginIcons;
                        if (p->PluginIconsGray != NULL)
                        {
                            delete p->PluginIconsGray; // jen pro jistotu (zatim je z konstruktoru bitmapa prazdna)
                            p->PluginIconsGray = NULL;
                        }
                        if (pluginIcons != NULL)
                        {
                            p->PluginIconsGray = new CIconList();
                            if (!p->PluginIconsGray->CreateAsCopy(pluginIcons, TRUE))
                            {
                                delete p->PluginIconsGray;
                                p->PluginIconsGray = NULL;
                            }
                        }

                        pluginIcons = NULL; // uz neni potreba bitmapu dealokovat

                        // prirazeni ikon dame sem, protoze v AddPlugin() nema smysl (hodnoty se musi zmenit
                        // pri prvnim loadu pluginu, do te doby stejne nemame bitmapu s ikonami)
                        p->PluginIconIndex = pluginIconIndex;
                        p->PluginSubmenuIconIndex = pluginSubmenuIconIndex;
                        p->ShowSubmenuInPluginsBar = showSubmenuPluginsBar;

                        if (thumbnailMasks[0] != 0)
                        {
                            p->ThumbnailMasks.SetMasksString(thumbnailMasks);
                            int err2;
                            if (!p->ThumbnailMasks.PrepareMasks(err2)) // error
                            {
                                p->ThumbnailMasks.SetMasksString("");
                            }
                        }

                        if (GetValue(itemKey, SALAMANDER_PLUGINS_FSCMDNAME, REG_SZ, fsCmdName, MAX_PATH))
                        {
                            p->ChDrvMenuFSItemName = DupStr(fsCmdName);
                            if (p->ChDrvMenuFSItemName != NULL)
                            {
                                // ChDrvMenuFSItemIconIndex se neuklada pokud je -1 (BTW, resi to i konverzi stare konfigurace)
                                if (!GetValue(itemKey, SALAMANDER_PLUGINS_FSCMDICON, REG_DWORD,
                                              &(p->ChDrvMenuFSItemIconIndex), sizeof(DWORD)))
                                {
                                    p->ChDrvMenuFSItemIconIndex = -1;
                                }
                            }
                        }

                        if (!GetValue(itemKey, SALAMANDER_PLUGINS_FSCMDVISIBLE, REG_DWORD, &p->ChDrvMenuFSItemVisible, sizeof(DWORD)))
                            p->ChDrvMenuFSItemVisible = TRUE;

                        if (!GetValue(itemKey, SALAMANDER_PLUGINS_ISNETHOOD, REG_DWORD, &p->PluginIsNethood, sizeof(DWORD)))
                            p->PluginIsNethood = FALSE;

                        if (!GetValue(itemKey, SALAMANDER_PLUGINS_USESPASSWDMAN, REG_DWORD, &p->PluginUsesPasswordManager, sizeof(DWORD)))
                            p->PluginUsesPasswordManager = FALSE;

                        HKEY menuKey;
                        if (p != NULL && OpenKey(itemKey, SALAMANDER_PLUGINS_MENU, menuKey))
                        {
                            HKEY menuItemKey;
                            char buf2[30];
                            int i2 = 1;
                            strcpy(buf2, "1");
                            while (OpenKey(menuKey, buf2, menuItemKey))
                            {
                                DWORD state, id, skillLevel, iconIndex, type, hotKey;
                                BOOL stateLoaded, idLoaded;
                                idLoaded = GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMID, REG_DWORD, &id, sizeof(DWORD));
                                stateLoaded = GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSTATE, REG_DWORD, &state, sizeof(DWORD));

                                // SkillLevel je ulozen pouze v pripade, ze je ruzny od MENU_SKILLLEVEL_ALL
                                // setrime tim registry a zajistili jsme konverzi starych konfiguraci
                                if (!GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL, REG_DWORD, &skillLevel, sizeof(DWORD)))
                                    skillLevel = MENU_SKILLLEVEL_ALL;

                                // IconIndex je ulozen pouze v pripade, ze je ruzny od -1 (zadna ikona)
                                // setrime tim registry a zajistili jsme konverzi starych konfiguraci;
                                // u dynamickych menu ikonu neukladame, tedy neukladame ani jeji index
                                if (dynMenuExt ||
                                    !GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMICONINDEX, REG_DWORD, &iconIndex, sizeof(DWORD)))
                                    iconIndex = -1;

                                // Type je ulozen pouze v pripade, ze je ruzny od pmitItemOrSeparator
                                // setrime tim registry a zajistili jsme konverzi starych konfiguraci
                                if (!GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMTYPE, REG_DWORD, &type, sizeof(DWORD)))
                                    type = pmitItemOrSeparator;

                                // HotKey je ulozen pouze v pripade, ze je ruzny od 0
                                // setrime tim registry a zajistili jsme konverzi starych konfiguraci
                                if (!GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMHOTKEY, REG_DWORD, &hotKey, sizeof(DWORD)))
                                    hotKey = 0;

                                if (GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMNAME, REG_SZ, name, MAX_PATH) &&
                                    stateLoaded && idLoaded &&
                                    (type == pmitItemOrSeparator || type == pmitStartSubmenu))
                                { // normalni nebo start-submenu polozka menu
                                    p->AddMenuItem(iconIndex, name, hotKey, id, state == -1, HIWORD(state), LOWORD(state),
                                                   skillLevel, (CPluginMenuItemType)type);
                                }
                                else // separator nebo end-submenu
                                {
                                    if (type == pmitItemOrSeparator)
                                    {
                                        p->AddMenuItem(-1, NULL, 0, idLoaded ? id : 0, stateLoaded && state == -1, 0, 0,
                                                       skillLevel, (CPluginMenuItemType)type);
                                    }
                                    else
                                    {
                                        p->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL, pmitEndSubmenu);
                                    }
                                }

                                CloseKey(menuItemKey);
                                itoa(++i2, buf2, 10);
                            }
                            CloseKey(menuKey);
                        }
                        p->HotKeysEnsureIntegrity();
                    }
                }
            }
            CloseKey(itemKey);

            // pokud jsme bitmapu nepriradili, musime ji uvolnit
            if (pluginIcons != NULL)
                delete pluginIcons;

            if (err) // chyba, to si nemuzeme dovolit ...
            {
                HANDLES(EnterCriticalSection(&DataCS));
                Data.DestroyMembers();
                HANDLES(LeaveCriticalSection(&DataCS));
                break;
            }

            itoa(++i, buf, 10);
        }
    }
    else // default hodnoty
    {
        if (!AddPlugin("ZIP", "zip\\zip.spl",
                       TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, "1.32",
                       "Copyright © 2000-2023 Open Salamander Authors",
                       "ZIP archives support for Open Salamander.",
                       "ZIP", "zip;pk3;jar", NULL, FALSE, NULL, NULL) ||
            !AddPlugin("TAR", "tar\\tar.spl",
                       TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE, "3.3",
                       "Copyright © 1999-2023 Open Salamander Authors",
                       "Unix archives readonly support for Open Salamander.",
                       "TAR", "tar;tgz;taz;tbz;gz;bz;bz2;z;rpm;cpio", NULL, FALSE, NULL, NULL) ||
            !AddPlugin("PAK", "pak\\pak.spl",
                       TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, "1.68",
                       "Copyright © 1999-2023 Open Salamander Authors",
                       "This plug-ing adds support for Quake PAK archives.",
                       "PAK", "pak", NULL, FALSE, NULL, NULL) ||
            !AddPlugin("Internet Explorer Viewer", "ieviewer\\ieviewer.spl",
                       FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE,
                       "1.1", "Copyright © 1999-2023 Open Salamander Authors",
                       "Internet Explorer Viewer for Open Salamander.",
                       "IEVIEWER", "", NULL, FALSE, NULL, NULL))
        {
            HANDLES(EnterCriticalSection(&DataCS));
            Data.DestroyMembers();
            HANDLES(LeaveCriticalSection(&DataCS));
        }
        else
            DefaultConfiguration = TRUE;
    }
}

void CPlugins::LoadOrder(HWND parent, HKEY regKey)
{
    if (regKey != NULL)
    {
        char dllName[MAX_PATH];
        DWORD showInBar;
        HKEY itemKey;
        char buf[30];
        int i = 1;
        strcpy(buf, "1");
        while (OpenKey(regKey, buf, itemKey))
        {
            if (!GetValue(itemKey, SALAMANDER_PLUGINSORDER_SHOW, REG_DWORD, &showInBar, sizeof(DWORD)))
                showInBar = TRUE;
            if (GetValue(itemKey, SALAMANDER_PLUGINS_DLLNAME, REG_SZ, dllName, MAX_PATH))
            {
                AddPluginToOrder(dllName, showInBar);
            }
            itoa(++i, buf, 10);
            CloseKey(itemKey);
        }
    }
}

void CPlugins::Save(HWND parent, HKEY regKey, HKEY regKeyConfig, HKEY regKeyOrder)
{
    CALL_STACK_MESSAGE1("CPlugins::Save(, ,)");
    if (regKey != NULL)
    {
        ClearKey(regKey);
        HKEY itemKey;
        char buf[30];
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            itoa(i + 1, buf, 10);
            if (CreateKey(regKey, buf, itemKey))
            {
                CPluginData* p = Data[i];

                SetValue(itemKey, SALAMANDER_PLUGINS_NAME, REG_SZ, p->Name, -1);
                SetValue(itemKey, SALAMANDER_PLUGINS_DLLNAME, REG_SZ, p->DLLName, -1);
                SetValue(itemKey, SALAMANDER_PLUGINS_VERSION, REG_SZ, p->Version, -1);
                SetValue(itemKey, SALAMANDER_PLUGINS_COPYRIGHT, REG_SZ, p->Copyright, -1);
                SetValue(itemKey, SALAMANDER_PLUGINS_EXTENSIONS, REG_SZ, p->Extensions, -1);
                SetValue(itemKey, SALAMANDER_PLUGINS_DESCRIPTION, REG_SZ, p->Description, -1);
                SetValue(itemKey, SALAMANDER_PLUGINS_REGKEYNAME, REG_SZ, p->RegKeyName, -1);
                SaveFSNames(itemKey, &p->FSNames);

                DWORD functions = 0;
                functions |= p->SupportPanelView ? FUNCTION_PANELARCHIVERVIEW : 0;
                functions |= p->SupportPanelEdit ? FUNCTION_PANELARCHIVEREDIT : 0;
                functions |= p->SupportCustomPack ? FUNCTION_CUSTOMARCHIVERPACK : 0;
                functions |= p->SupportCustomUnpack ? FUNCTION_CUSTOMARCHIVERUNPACK : 0;
                functions |= p->SupportConfiguration ? FUNCTION_CONFIGURATION : 0;
                functions |= p->SupportLoadSave ? FUNCTION_LOADSAVECONFIGURATION : 0;
                functions |= p->SupportViewer ? FUNCTION_VIEWER : 0;
                functions |= p->SupportFS ? FUNCTION_FILESYSTEM : 0;
                functions |= p->SupportDynMenuExt ? FUNCTION_DYNAMICMENUEXT : 0;

                SetValue(itemKey, SALAMANDER_PLUGINS_FUNCTIONS, REG_DWORD, &functions, sizeof(DWORD));

                if (p->LoadOnStart) // budeme ukladat jen TRUE -> setrime mistem v registry
                {
                    DWORD loadOnStartDWORD = TRUE;
                    SetValue(itemKey, SALAMANDER_PLUGINS_LOADONSTART, REG_DWORD, &loadOnStartDWORD, sizeof(DWORD));
                }

                if (p->ChDrvMenuFSItemName != NULL) // mame prikaz FS do change-drive menu
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_FSCMDNAME, REG_SZ, p->ChDrvMenuFSItemName, -1);

                    // ChDrvMenuFSItemIconIndex se neuklada pokud je -1 (BTW, resi to i konverzi stare konfigurace)
                    if (p->ChDrvMenuFSItemIconIndex != -1)
                    {
                        SetValue(itemKey, SALAMANDER_PLUGINS_FSCMDICON, REG_DWORD,
                                 &(p->ChDrvMenuFSItemIconIndex), sizeof(DWORD));
                    }
                }

                if (!p->ChDrvMenuFSItemVisible)
                    SetValue(itemKey, SALAMANDER_PLUGINS_FSCMDVISIBLE, REG_DWORD, &p->ChDrvMenuFSItemVisible, sizeof(DWORD));

                if (p->PluginIsNethood)
                    SetValue(itemKey, SALAMANDER_PLUGINS_ISNETHOOD, REG_DWORD, &p->PluginIsNethood, sizeof(DWORD));

                if (p->PluginUsesPasswordManager)
                    SetValue(itemKey, SALAMANDER_PLUGINS_USESPASSWDMAN, REG_DWORD, &p->PluginUsesPasswordManager, sizeof(DWORD));

                if (p->LastSLGName != NULL && p->LastSLGName[0] != 0) // ulozime je pokud neni prazdny string
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_LASTSLGNAME, REG_SZ, p->LastSLGName, -1);
                }
                if (p->PluginHomePageURL != NULL && p->PluginHomePageURL[0] != 0) // ulozime je pokud neni prazdny string
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_HOMEPAGE, REG_SZ, p->PluginHomePageURL, -1);
                }
                if (p->ThumbnailMasks.GetMasksString()[0] != 0) // ulozime je pokud neni prazdny string
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_THUMBMASKS, REG_SZ, p->ThumbnailMasks.GetMasksString(), -1);
                }
                if (p->PluginIcons != NULL) // ulozime jen pokud existuje
                {
                    SaveIconList(itemKey, SALAMANDER_PLUGINS_PLGICONLIST, p->PluginIcons);
                }
                if (p->PluginIconIndex != -1) // ulozime jen pokud neni -1
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_PLGICONINDEX, REG_DWORD, &(p->PluginIconIndex), sizeof(DWORD));
                }
                if (p->PluginSubmenuIconIndex != -1) // ulozime jen pokud neni -1
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_PLGSUBMENUICONINDEX, REG_DWORD, &(p->PluginSubmenuIconIndex), sizeof(DWORD));
                }
                if (!p->ShowSubmenuInPluginsBar) // ulozime jen pokud neni TRUE
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_SUBMENUINPLUGINSBAR, REG_DWORD, &(p->ShowSubmenuInPluginsBar), sizeof(DWORD));
                }

                HKEY menuKey;
                if (p->MenuItems.Count > 0 && CreateKey(itemKey, SALAMANDER_PLUGINS_MENU, menuKey))
                { // zapiseme nove hodnoty
                    HKEY menuItemKey;
                    char buf2[30];
                    int i2;
                    for (i2 = 0; i2 < p->MenuItems.Count; i2++)
                    {
                        itoa(i2 + 1, buf2, 10);
                        if (CreateKey(menuKey, buf2, menuItemKey))
                        {
                            CPluginMenuItem* item = p->MenuItems[i2];
                            if (item->Name != NULL || item->StateMask == -1)
                            {                                                              // 'state' ukladame jen pokud jde o polozku nebo separator s "call-get-state"
                                DWORD state = p->SupportDynMenuExt ? -1 : item->StateMask; // dynamicke menu: tento hack resi stav, kdy plugin s dynamickym menu pri loadu prejde na staticke menu, a pak selze v entry pointu (obsah dynamickeho menu zustane a pokud by v nem nebyly polozky s call-get-state, muze se menu zobrazit i bez loadu pluginu)
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSTATE, REG_DWORD,
                                         &state, sizeof(DWORD));
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMID, REG_DWORD,
                                         &(item->ID), sizeof(DWORD));
                            }
                            if (item->Name != NULL) // normalni polozka - ulozime jmeno
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMNAME, REG_SZ, item->Name, -1);

                            // SkillLevel je ulozen pouze v pripade, ze je ruzny od MENU_SKILLLEVEL_ALL
                            // setrime tim registry a zajistili jsme konverzi starych konfiguraci
                            if (item->SkillLevel != MENU_SKILLLEVEL_ALL)
                            {
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL, REG_DWORD,
                                         &(item->SkillLevel), sizeof(DWORD));
                            }

                            // IconIndex je ulozen pouze v pripade, ze je ruzny od -1 (zadna ikona)
                            // setrime tim registry a zajistili jsme konverzi starych konfiguraci;
                            // u dynamickych menu ikonu neukladame, tedy neukladame ani jeji index
                            if (!p->SupportDynMenuExt && item->IconIndex != -1)
                            {
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMICONINDEX, REG_DWORD,
                                         &(item->IconIndex), sizeof(DWORD));
                            }

                            // Type je ulozen pouze v pripade, ze je ruzny od pmitItemOrSeparator
                            // setrime tim registry a zajistili jsme konverzi starych konfiguraci
                            if (item->Type != pmitItemOrSeparator)
                            {
                                DWORD type = item->Type;
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMTYPE, REG_DWORD, &type, sizeof(DWORD));
                            }

                            // HotKey je ulozen pouze v pripade, ze je ruzny od 0
                            // setrime tim registry a zajistili jsme konverzi starych konfiguraci
                            if (item->HotKey != 0)
                            {
                                DWORD hotKey = item->HotKey;
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMHOTKEY, REG_DWORD, &hotKey, sizeof(DWORD));
                            }

                            CloseKey(menuItemKey);
                        }
                    }
                    CloseKey(menuKey);
                }

                if (p->GetLoaded() && regKeyConfig != NULL)
                    p->Save(parent, regKeyConfig);

                CloseKey(itemKey);
            }
            else
                break;
        }
    }
    if (regKeyOrder != NULL)
    {
        ClearKey(regKeyOrder);
        HKEY itemKey;
        char buf[30];
        int i;
        for (i = 0; i < Order.Count; i++)
        {
            itoa(i + 1, buf, 10);
            if (CreateKey(regKeyOrder, buf, itemKey))
            {
                CPluginOrder* order = &Order[i];
                SetValue(itemKey, SALAMANDER_PLUGINS_DLLNAME, REG_SZ, order->DLLName, -1);
                CloseKey(itemKey);
            }
        }
    }
}

BOOL CPlugins::Remove(HWND parent, int index, BOOL canDelPluginRegKey)
{
    CALL_STACK_MESSAGE3("CPlugins::Remove(, %d, %d)", index, canDelPluginRegKey);
    if (index >= 0 && index < Data.Count)
    {
        if (Data[index]->Remove(parent, index, canDelPluginRegKey))
        {
            HANDLES(EnterCriticalSection(&DataCS));
            Data.Delete(index);
            HANDLES(LeaveCriticalSection(&DataCS));
            // dame hlavnimu oknu vedet o odstraneni pluginu
            MainWindow->OnPluginsStateChanged();
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CPlugins::UnloadAll(HWND parent)
{
    CALL_STACK_MESSAGE1("CPlugins::UnloadAll()");
    BOOL ret = TRUE;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->GetLoaded())
        {
            if (!Data[i]->Unload(parent, FALSE))
                ret = FALSE;
        }
    }
    return ret;
}

enum CPluginFunctionType
{
    pftPanelView,
    pftPanelEdit,
    pftCustomPack,
    pftCustomUnpack
};

BOOL IsArchiveIndexOK(int i, CPluginFunctionType type)
{
    if (i < 0)
    {
        CPluginData* p = Plugins.Get(-i - 1); // plug-inovy index
        if (p != NULL)                        // plug-in existuje, jeste zkontrolujeme jestli ma pozadovanou funkci
        {
            return type == pftPanelView && p->SupportPanelView ||
                   type == pftPanelEdit && p->SupportPanelEdit ||
                   type == pftCustomPack && p->SupportCustomPack ||
                   type == pftCustomUnpack && p->SupportCustomUnpack;
        }
        else
            return FALSE;
    }
    else
        return i < ArchiverConfig.GetArchiversCount(); // index externiho archivatoru
}

int CPlugins::GetIndexJustForConnect(const CPluginData* plugin)
{
    if (plugin != NULL)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            if (Data[i] == plugin)
                return i;
        }
    }
    return -1;
}

int CPlugins::GetIndex(const CPluginInterfaceAbstract* plugin)
{
    HANDLES(EnterCriticalSection(&DataCS));
    if (plugin != NULL)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            if (Data[i]->GetPluginInterface()->Contains(plugin))
            {
                HANDLES(LeaveCriticalSection(&DataCS));
                return i;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&DataCS));
    return -1;
}

CPluginData*
CPlugins::GetPluginData(const CPluginInterfaceForFSAbstract* plugin)
{
    if (plugin != NULL)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            if (Data[i]->GetPluginInterfaceForFS()->Contains(plugin))
                return Data[i];
        }
    }
    return NULL;
}

CPluginData*
CPlugins::GetPluginData(const CPluginInterfaceAbstract* plugin, int* lastIndex)
{
    if (plugin != NULL)
    {
        if (lastIndex != NULL && *lastIndex >= 0 && *lastIndex < Data.Count &&
            Data[*lastIndex]->GetPluginInterface()->Contains(plugin))
        {
            return Data[*lastIndex];
        }
        for (int i = 0; i < Data.Count; i++)
        {
            if (Data[i]->GetPluginInterface()->Contains(plugin))
            {
                if (lastIndex != NULL)
                    *lastIndex = i;
                return Data[i];
            }
        }
    }
    if (lastIndex != NULL)
        *lastIndex = -1;
    return NULL;
}

CPluginData*
CPlugins::GetPluginData(const char* dllName)
{
    if (dllName != NULL)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        { // zde porovnavame ukazatele DLLName (alokuje se jen jednou -> identifikuje jednoznacne plug-in)
            if (Data[i]->DLLName == dllName)
                return Data[i];
        }
    }
    return NULL;
}

CPluginData*
CPlugins::GetPluginDataFromSuffix(const char* dllSuffix)
{
    CALL_STACK_MESSAGE2("CPlugins::GetPluginDataFromSuffix(%s)", dllSuffix);
    if (dllSuffix != NULL)
    {
        // ziskame plne jmeno adresare plugins
        char fullDLLName[MAX_PATH];
        GetModuleFileName(HInstance, fullDLLName, MAX_PATH);
        char* name = strrchr(fullDLLName, '\\') + 1;
        strcpy(name, "plugins\\");
        name += strlen(name);

        int sufLen = (int)strlen(dllSuffix);
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CPluginData* data = Data[i];
            char* s = data->DLLName;
            if ((*s != '\\' || *(s + 1) != '\\') && // ne UNC
                (*s == 0 || *(s + 1) != ':'))       // ne "c:" -> relativni cesta k plugins
            {
                strcpy(name, data->DLLName);
                s = fullDLLName;
            }
            int len = (int)strlen(s);
            if (len >= sufLen && StrNICmp(s + len - sufLen, dllSuffix, sufLen) == 0)
            {
                return data; // nalezeno
            }
        }
    }
    return NULL; // nenalezeno
}

void CPlugins::CheckData()
{
    CALL_STACK_MESSAGE1("CPlugins::CheckData()");
    if (!DefaultConfiguration) // neni def. konfigurace plug-inu -> smazeme vsechny stare ZIP+TAR+PAK
    {                          // a provedeme konverzi stareho typu "external" u "custom pack/unpack"
                               // a prohlasime vsechny data za nova
                               // dale provedeme vycisteni od internich a konverzi externich viewru
        int i;
        for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
        {
            if (PackerFormatConfig.GetOldType(i))
            {
                if (PackerFormatConfig.GetUsePacker(i) && PackerFormatConfig.GetPackerIndex(i) < 0 ||
                    PackerFormatConfig.GetUnpackerIndex(i) < 0)
                {
                    PackerFormatConfig.DeleteFormat(i--);
                }
                else
                    PackerFormatConfig.SetOldType(i, FALSE);
            }
        }

        for (i = 0; i < PackerConfig.GetPackersCount(); i++)
        {
            if (PackerConfig.GetPackerOldType(i))
            {
                if (PackerConfig.GetPackerType(i) != 1)
                    PackerConfig.DeletePacker(i--); // neni externi
                else
                {
                    PackerConfig.SetPackerType(i, CUSTOMPACKER_EXTERNAL);
                    PackerConfig.SetPackerOldType(i, FALSE);
                }
            }
        }

        for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
        {
            if (UnpackerConfig.GetUnpackerOldType(i))
            {
                if (UnpackerConfig.GetUnpackerType(i) != 1)
                    UnpackerConfig.DeleteUnpacker(i--); // neni externi
                else
                {
                    UnpackerConfig.SetUnpackerType(i, CUSTOMUNPACKER_EXTERNAL);
                    UnpackerConfig.SetUnpackerOldType(i, FALSE);
                }
            }
        }

        CViewerMasks* viewerMasks;
        MainWindow->EnterViewerMasksCS();
        int k;
        for (k = 0; k < 2; k++)
        {
            if (k == 0)
                viewerMasks = MainWindow->ViewerMasks;
            else
                viewerMasks = MainWindow->AltViewerMasks;
            for (i = 0; i < viewerMasks->Count; i++)
            {
                if (viewerMasks->At(i)->OldType)
                {
                    if (viewerMasks->At(i)->ViewerType != 2) // neni externi
                    {
                        if (viewerMasks->At(i)->ViewerType != 0) // neni interni hex/text viewer
                        {
                            viewerMasks->Delete(i--);
                        }
                        else
                        {
                            viewerMasks->At(i)->OldType = FALSE;
                            viewerMasks->At(i)->ViewerType = VIEWER_INTERNAL;
                        }
                    }
                    else
                    {
                        viewerMasks->At(i)->OldType = FALSE;
                        viewerMasks->At(i)->ViewerType = VIEWER_EXTERNAL;
                    }
                }
            }
        }
        MainWindow->LeaveViewerMasksCS();
    }
    else // je defaulni konfigurace, zmenime stare typy external+ZIP+TAR+PAK u "custom pack/unpack"
    {    // a prohlasime vsechna data za nova, dale zmenime stare typy u "file viewer" a prohlasime
        // data za nova

        // PackerFormatConfig.GetPackerIndex(i) zustava -1 ZIP, -2 TAR, -3 PAK
        // PackerFormatConfig.GetUnpackerIndex(i) zustava -1 ZIP, -2 TAR, -3 PAK
        int i;
        for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
        {
            if (PackerFormatConfig.GetOldType(i))
                PackerFormatConfig.SetOldType(i, FALSE);
        }

        // prevody pro "custom pack"
        for (i = 0; i < PackerConfig.GetPackersCount(); i++)
        {
            if (PackerConfig.GetPackerOldType(i)) // stare hodnoty prekodujeme
            {
                PackerConfig.SetPackerOldType(i, FALSE);
                switch (PackerConfig.GetPackerType(i))
                {
                case 0:
                    PackerConfig.SetPackerType(i, -1);
                    break; // ZIP
                case 1:
                    PackerConfig.SetPackerType(i, CUSTOMPACKER_EXTERNAL);
                    break; // external
                case 2:
                    PackerConfig.SetPackerType(i, -2);
                    break; // TAR
                case 3:
                    PackerConfig.SetPackerType(i, -3);
                    break; // PAK
                default:
                    PackerConfig.DeletePacker(i--);
                    break; // chyba
                }
            }
        }

        // prevody pro "custom unpack"
        for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
        {
            if (UnpackerConfig.GetUnpackerOldType(i)) // stare hodnoty prekodujeme
            {
                UnpackerConfig.SetUnpackerOldType(i, FALSE);
                switch (UnpackerConfig.GetUnpackerType(i))
                {
                case 0:
                    UnpackerConfig.SetUnpackerType(i, -1);
                    break; // ZIP
                case 1:
                    UnpackerConfig.SetUnpackerType(i, CUSTOMUNPACKER_EXTERNAL);
                    break; // external
                case 2:
                    UnpackerConfig.SetUnpackerType(i, -2);
                    break; // TAR
                case 3:
                    UnpackerConfig.SetUnpackerType(i, -3);
                    break; // PAK
                default:
                    UnpackerConfig.DeleteUnpacker(i--);
                    break; // chyba
                }
            }
        }

        CViewerMasks* viewerMasks;
        MainWindow->EnterViewerMasksCS();
        int k;
        for (k = 0; k < 2; k++)
        {
            if (k == 0)
                viewerMasks = MainWindow->ViewerMasks;
            else
                viewerMasks = MainWindow->AltViewerMasks;
            for (i = 0; i < viewerMasks->Count; i++)
            {
                if (viewerMasks->At(i)->OldType) // stare hodnoty prekodujeme
                {
                    viewerMasks->At(i)->OldType = FALSE;
                    switch (viewerMasks->At(i)->ViewerType)
                    {
                    case 0:
                        viewerMasks->At(i)->ViewerType = VIEWER_INTERNAL;
                        break; // interni viewer
                    case 1:
                        viewerMasks->At(i)->ViewerType = -4;
                        break; // IE viewer
                    case 2:
                        viewerMasks->At(i)->ViewerType = VIEWER_EXTERNAL;
                        break; // external
                    default:
                        viewerMasks->Delete(i--);
                        break; // chyba
                    }
                }
            }
        }
        MainWindow->LeaveViewerMasksCS();
    }

    // kontrola indexu/typu archivatoru/plug-inu, pripadne mazani vadnych
    int i;
    for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
    {
        if (PackerFormatConfig.GetUsePacker(i))
        {
            if (!IsArchiveIndexOK(PackerFormatConfig.GetPackerIndex(i), pftPanelEdit))
            {
                TRACE_E("Invalid packer index in PackerFormatConfig, ext = " << PackerFormatConfig.GetExt(i)); // pri importu konfigurace z verze 2.0 hlasi tento error, protoze UnCAB vydany s verzi 2.0 hlasil chybne, ze umi i pakovat (coz je nesmysl a tady se chyba koriguje) - neni treba dale resit...
                PackerFormatConfig.SetUsePacker(i, FALSE);                                                     // spatny index packeru -> "packing is not supported"
            }
        }
        if (!IsArchiveIndexOK(PackerFormatConfig.GetUnpackerIndex(i), pftPanelView))
        {
            TRACE_E("Invalid unpacker index in PackerFormatConfig, ext = " << PackerFormatConfig.GetExt(i));
            PackerFormatConfig.DeleteFormat(i--);
        }
    }
    PackerFormatConfig.BuildArray();

    for (i = 0; i < PackerConfig.GetPackersCount(); i++)
    {
        int t = PackerConfig.GetPackerType(i);
        if (t > 0 || t < 0 && !IsArchiveIndexOK(t, pftCustomPack))
        {
            TRACE_E("Invalid packer type in PackerConfig, title = " << PackerConfig.GetPackerTitle(i));
            PackerConfig.DeletePacker(i--);
        }
    }

    for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
    {
        int t = UnpackerConfig.GetUnpackerType(i);
        if (t > 0 || t < 0 && !IsArchiveIndexOK(t, pftCustomUnpack))
        {
            TRACE_E("Invalid unpacker type in UnpackerConfig, title = " << UnpackerConfig.GetUnpackerTitle(i));
            UnpackerConfig.DeleteUnpacker(i--);
        }
    }

    CViewerMasks* viewerMasks;
    MainWindow->EnterViewerMasksCS();
    int k;
    for (k = 0; k < 2; k++)
    {
        if (k == 0)
            viewerMasks = MainWindow->ViewerMasks;
        else
            viewerMasks = MainWindow->AltViewerMasks;
        for (i = 0; i < viewerMasks->Count; i++)
        {
            int t = viewerMasks->At(i)->ViewerType;
            if (t != VIEWER_EXTERNAL &&                 // neni externi
                t != VIEWER_INTERNAL &&                 // neni interni
                (t > 0 || Plugins.Get(-t - 1) == NULL)) // neni ani plug-inovy
            {
                TRACE_E("Invalid viewer-type in (Alt)ViewerMasks, masks = " << (viewerMasks->At(i)->Masks != NULL ? viewerMasks->At(i)->Masks->GetMasksString() : "NULL"));
                viewerMasks->Delete(i--);
            }
        }
    }
    MainWindow->LeaveViewerMasksCS();
}

BOOL CPlugins::AddPlugin(HWND parent, const char* fileName)
{
    CALL_STACK_MESSAGE2("CPlugins::AddPlugin(, %s)", fileName);
    static char emptyBuffer[] = "";
    if (AddPlugin(emptyBuffer, fileName, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
                  emptyBuffer, emptyBuffer, emptyBuffer, emptyBuffer, emptyBuffer, NULL, FALSE, emptyBuffer, NULL))
    {
        LoadInfoBase |= LOADINFO_INSTALL;
        BOOL ret = Data[Data.Count - 1]->InitDLL(parent);
        LoadInfoBase &= ~LOADINFO_INSTALL;

        if (!ret)
        {
            HANDLES(EnterCriticalSection(&DataCS));
            Data.Delete(Data.Count - 1);
            HANDLES(LeaveCriticalSection(&DataCS));
        }
        else
        {
            // dame hlavnimu oknu vedet o pridani pluginu
            MainWindow->OnPluginsStateChanged();
            return TRUE;
        }
    }
    return FALSE;
}

void CPlugins::GetUniqueRegKeyName(char* uniqueKeyName, const char* regKeyName)
{
    CALL_STACK_MESSAGE2("CPlugins::GetUniqueRegKeyName(, %s)", regKeyName);
    int number = 2;
    strcpy(uniqueKeyName, regKeyName);
    if (regKeyName[0] != 0)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            if (StrICmp(uniqueKeyName, Data[i]->RegKeyName) == 0) // neni unikatni
            {
                sprintf(uniqueKeyName + strlen(regKeyName), " (%d)", number++); // zmena jmena klice
                i = -1;                                                         // provedeme porovnani znovu
            }
        }
    }
}

void CPlugins::GetUniqueFSName(char* uniqueFSName, const char* fsName, TIndirectArray<char>* uniqueFSNames,
                               TIndirectArray<char>* oldFSNames)
{
    CALL_STACK_MESSAGE2("CPlugins::GetUniqueFSName(, %s, ,)", fsName);
    int number = 2;
    lstrcpyn(uniqueFSName, fsName, MAX_PATH - 9); // nechame na konci fs-name 9 znaku rezervu na cislice pro hledani unikatniho jmena
    int offset = (int)strlen(uniqueFSName);
    if (offset < 2)
    {
        TRACE_E("File system name is too short (" << fsName << ")");
        strcpy(uniqueFSName, "fs");
        offset = 2;
    }
    int i;
    for (i = 0; i < offset; i++)
    {
        if ((uniqueFSName[i] < 'a' || uniqueFSName[i] > 'z') &&
            (uniqueFSName[i] < 'A' || uniqueFSName[i] > 'Z') &&
            (uniqueFSName[i] < '0' || uniqueFSName[i] > '9') &&
            uniqueFSName[i] != '_' &&
            uniqueFSName[i] != '+' &&
            uniqueFSName[i] != '-')
        {
            TRACE_E("File system name '" << fsName << "' contains illegal character: '" << uniqueFSName[i] << "'");
            uniqueFSName[i] = '_';
        }
    }

    BOOL oldFSNameUsed = FALSE;
    if (oldFSNames != NULL)
    {
        for (i = 0; i < oldFSNames->Count; i++)
        {
            char* s = oldFSNames->At(i);
            int len = (int)strlen(s);
            if (len >= offset && StrNICmp(s, uniqueFSName, offset) == 0)
            { // shoda az na pripadny suffix, zkontrolujeme jestli je suffix ciselny (cisla pridavaji pri hledani unikatniho jmena)
                char* num = s + offset;
                while (*num != 0 && *num >= '0' && *num <= '9')
                    num++;
                if (*num == 0) // suffix je ciselny -> toto stare fs-name lze pouzit pro hledane fs-name
                {
                    lstrcpyn(uniqueFSName + offset, s + offset, MAX_PATH - offset);
                    oldFSNameUsed = TRUE;
                    oldFSNames->Delete(i); // pouzite fs-name vyhodime z pole (opakovane pouziti je nesmysl - i v pripade neunikatniho jmena)
                    if (!oldFSNames->IsGood())
                        oldFSNames->ResetState(); // mazani se povedlo, nic dalsiho nas nezajima
                    break;
                }
            }
        }
    }

    for (i = 0; i < Data.Count; i++)
    {
        if (i == 0 && uniqueFSNames != NULL)
        {
            int j;
            for (j = 0; j < uniqueFSNames->Count; j++)
            {
                if (StrICmp(uniqueFSName, uniqueFSNames->At(j)) == 0) // neni unikatni
                {
                    if (!oldFSNameUsed)
                        sprintf(uniqueFSName + offset, "%d", number++); // zmena jmena klice
                    else                                                // stare jmeno uz neni unikatni, jdeme hledat jine unikatni jmeno
                    {
                        oldFSNameUsed = FALSE;
                        uniqueFSName[offset] = 0;
                    }
                    i = -1; // provedeme porovnani znovu
                    break;
                }
            }
            if (i == -1)
                continue;
        }

        CPluginData* plugin = Data[i];
        int j;
        for (j = 0; j < plugin->FSNames.Count; j++)
        {
            if (StrICmp(uniqueFSName, plugin->FSNames[j]) == 0) // neni unikatni
            {
                if (!oldFSNameUsed)
                    sprintf(uniqueFSName + offset, "%d", number++); // zmena jmena klice
                else                                                // stare jmeno uz neni unikatni, jdeme hledat jine unikatni jmeno
                {
                    oldFSNameUsed = FALSE;
                    uniqueFSName[offset] = 0;
                }
                i = -1; // provedeme porovnani znovu
                break;
            }
        }
    }
}

BOOL CPlugins::AddPlugin(const char* name, const char* dllName, BOOL supportPanelView,
                         BOOL supportPanelEdit, BOOL supportCustomPack, BOOL supportCustomUnpack,
                         BOOL supportConfiguration, BOOL supportLoadSave, BOOL supportViewer,
                         BOOL supportFS, BOOL supportDynMenuExt, const char* version,
                         const char* copyright, const char* description, const char* regKeyName,
                         const char* extensions, TIndirectArray<char>* fsNames, BOOL loadOnStart,
                         char* lastSLGName, const char* pluginHomePageURL)
{
    CALL_STACK_MESSAGE20("CPlugins::AddPlugin(%s, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %s, %s, %s, %s, , %d, %s, %s)",
                         name, dllName, supportPanelView, supportPanelEdit, supportCustomPack,
                         supportCustomUnpack, supportConfiguration, supportLoadSave, supportViewer,
                         supportFS, supportDynMenuExt, version, copyright, description, regKeyName, extensions,
                         loadOnStart, lastSLGName, pluginHomePageURL);
    BOOL ret = FALSE;

    char uniqueKeyName[MAX_PATH];
    if (supportLoadSave)
        GetUniqueRegKeyName(uniqueKeyName, regKeyName);
    else
        uniqueKeyName[0] = 0;
    TIndirectArray<char>* uniqueFSNames = NULL;
    if (supportFS && fsNames != NULL)
    {
        uniqueFSNames = new TIndirectArray<char>(1, 10);
        if (uniqueFSNames != NULL)
        {
            int i;
            for (i = 0; i < fsNames->Count; i++)
            {
                char uniqueFSName[MAX_PATH];
                GetUniqueFSName(uniqueFSName, fsNames->At(i), uniqueFSNames, NULL);

                char* s = DupStr(uniqueFSName);
                if (s != NULL)
                {
                    uniqueFSNames->Add(s);
                    if (!uniqueFSNames->IsGood())
                    {
                        uniqueFSNames->ResetState();
                        free(s);
                        delete uniqueFSNames;
                        uniqueFSNames = NULL;
                        break;
                    }
                }
                else
                {
                    delete uniqueFSNames;
                    uniqueFSNames = NULL;
                    break;
                }
            }
        }
        else
            TRACE_E(LOW_MEMORY);
    }

    if (!supportFS || fsNames == NULL || uniqueFSNames != NULL)
    {
        CPluginData* item = new CPluginData(name, dllName, supportPanelView,
                                            supportPanelEdit, supportCustomPack, supportCustomUnpack,
                                            supportConfiguration, supportLoadSave, supportViewer, supportFS,
                                            supportDynMenuExt, version, copyright, description,
                                            uniqueKeyName, extensions, uniqueFSNames, loadOnStart,
                                            lastSLGName, pluginHomePageURL);
        if (item != NULL && item->IsGood())
        {
            HANDLES(EnterCriticalSection(&DataCS));
            Data.Add(item);
            HANDLES(LeaveCriticalSection(&DataCS));
            if (!Data.IsGood())
                Data.ResetState();
            else
            {
                ret = TRUE;
                item = NULL;
            }
        }
        if (item != NULL)
            delete item;
    }
    if (uniqueFSNames != NULL)
        delete uniqueFSNames;
    return ret;
}

void CPlugins::FindViewEdit(const char* extensions, int exclude, BOOL& viewFound, int& view,
                            BOOL& editFound, int& edit)
{
    CALL_STACK_MESSAGE3("CPlugins::FindViewEdit(%s, %d, , , , )", extensions, exclude);
    viewFound = editFound = FALSE;
    // ziskame pole pripon z retezec pripon (extensions)
    char ext[300];
    int len = (int)strlen(extensions);
    if (len > 299)
        len = 299;
    memcpy(ext, extensions, len);
    ext[len] = 0;
    TDirectArray<char*> extArray(10, 5); // pole pripon
    char* s = ext + len;
    while (s > ext)
    {
        while (--s >= ext && *s != ';')
            ;
        if (s >= ext)
            *s = 0;
        extArray.Add(s + 1); // bud prvni nebo nektery z rady pripon
    }

    char ext2[300]; // kopie Extensions postupne ze vsech plug-inu
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (i == exclude)
            continue; // tento index nemuze byt vysledkem

        CPluginData* p = Data[i];
        len = (int)strlen(p->Extensions);
        if (len > 299)
            len = 299;
        memcpy(ext2, p->Extensions, len);
        ext2[len] = 0;
        s = ext2 + len;
        while (s > ext2)
        {
            while (--s >= ext2 && *s != ';')
                ;
            if (s >= ext2)
                *s = 0;
            int j;
            for (j = 0; j < extArray.Count; j++)
            {
                if (StrICmp(s + 1, extArray[j]) == 0) // mnoziny pripon maji neprazdny prunik
                {
                    if (p->SupportPanelView && !viewFound)
                    {
                        view = -i - 1;
                        viewFound = TRUE;
                    }
                    if (p->SupportPanelEdit && !editFound)
                    {
                        edit = -i - 1;
                        editFound = TRUE;
                    }
                    break;
                }
            }
            if (j < extArray.Count)
                break; // uz jsme nasli, nema smysl s 'i' pokracovat
        }
        if (viewFound && editFound)
            break;
    }

    if (!viewFound || !editFound)
    {
        // !!! POZOR: pri zmenach poradi externich archivatoru je treba zmenit i poradi v poli
        // externalArchivers v metode CPlugins::FindViewEdit
        struct
        {
            const char* ext;
            int index;
        } externalArchivers[] =
            {
                {"J", 0},
                {"RAR", 1},
                // {"ARJ", 2},
                {"LZH", 3},
                {"UC2", 4},
                // {"J", 5},
                // {"RAR", 6},
                {"ZIP", 7},
                {"PK3", 7},
                {"JAR", 7},
                // {"ZIP;PK3;JAR", 8},
                {"ARJ", 9},
                {"ACE", 10},
                // {"ACE", 11},
                {NULL, 0}};

        i = 0;
        while (externalArchivers[i].ext != NULL)
        {
            int j;
            for (j = 0; j < extArray.Count; j++)
            {
                if (StrICmp(externalArchivers[i].ext, extArray[j]) == 0) // ext. archivator nalezen
                {
                    if (!viewFound)
                    {
                        view = externalArchivers[i].index;
                        viewFound = TRUE;
                    }
                    if (!editFound)
                    {
                        edit = externalArchivers[i].index;
                        editFound = TRUE;
                    }
                    break;
                }
            }
            if (viewFound && editFound)
                break;
            i++;
        }
    }
}

BOOL CPlugins::FindDLL(const char* dllName, int& index)
{
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (StrICmp(Data[i]->DLLName, dllName) == 0)
        {
            index = i;
            return TRUE;
        }
    }
    return FALSE;
}

int CPlugins::GetCustomPackerIndex(int count)
{
    CALL_STACK_MESSAGE2("CPlugins::GetCustomPackerIndex(%d)", count);
    if (count < 0)
        return -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->SupportCustomPack && count-- == 0)
            return i;
    }
    return -1;
}

int CPlugins::GetCustomPackerCount(int index)
{
    CALL_STACK_MESSAGE2("CPlugins::GetCustomPackerCount(%d)", index);
    if (index < 0 || index >= Data.Count)
        return -1;
    int count = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        if (Data[i]->SupportCustomPack)
            count++;
    }
    return count;
}

int CPlugins::GetCustomUnpackerIndex(int count)
{
    CALL_STACK_MESSAGE2("CPlugins::GetCustomUnpackerIndex(%d)", count);
    if (count < 0)
        return -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->SupportCustomUnpack && count-- == 0)
            return i;
    }
    return -1;
}

int CPlugins::GetCustomUnpackerCount(int index)
{
    CALL_STACK_MESSAGE2("CPlugins::GetCustomUnpackerCount(%d)", index);
    if (index < 0 || index >= Data.Count)
        return -1;
    int count = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        if (Data[i]->SupportCustomUnpack)
            count++;
    }
    return count;
}

int CPlugins::GetPanelViewIndex(int count)
{
    CALL_STACK_MESSAGE2("CPlugins::GetPanelViewIndex(%d)", count);
    if (count < 0)
        return -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->SupportPanelView && count-- == 0)
            return i;
    }
    return -1;
}

int CPlugins::GetPanelViewCount(int index)
{
    CALL_STACK_MESSAGE2("CPlugins::GetPanelViewCount(%d)", index);
    if (index < 0 || index >= Data.Count)
        return -1;
    int count = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        if (Data[i]->SupportPanelView)
            count++;
    }
    return count;
}

int CPlugins::GetPanelEditIndex(int count)
{
    CALL_STACK_MESSAGE2("CPlugins::GetPanelEditIndex(%d)", count);
    if (count < 0)
        return -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->SupportPanelEdit && count-- == 0)
            return i;
    }
    return -1;
}

int CPlugins::GetPanelEditCount(int index)
{
    CALL_STACK_MESSAGE2("CPlugins::GetPanelEditCount(%d)", index);
    if (index < 0 || index >= Data.Count)
        return -1;
    int count = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        if (Data[i]->SupportPanelEdit)
            count++;
    }
    return count;
}

int CPlugins::GetViewerIndex(int count)
{
    CALL_STACK_MESSAGE2("CPlugins::GetViewerIndex(%d)", count);
    if (count < 0)
        return -1;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->SupportViewer && count-- == 0)
            return i;
    }
    return -1;
}

int CPlugins::GetViewerCount(int index)
{
    CALL_STACK_MESSAGE2("CPlugins::GetViewerCount(%d)", index);
    if (index < 0 || index >= Data.Count)
        return -1;
    int count = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        if (Data[i]->SupportViewer)
            count++;
    }
    return count;
}

// pomocna funkce pro CPlugins::AutoInstallStdPluginsDir
void SearchForSPLs(char* buf, char* s, TIndirectArray<char>& foundFiles, WIN32_FIND_DATA& data)
{
    strcpy(s++, "\\*");
    HANDLE find = HANDLES_Q(FindFirstFile(buf, &data));
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) // jde o soubor
            {
                char* str = strrchr(data.cFileName, '.');
                //        if (str != NULL && str > data.cFileName && StrICmp(str, ".spl") == 0) // ".cvspass" ve Windows je pripona ...
                if (str != NULL && StrICmp(str, ".spl") == 0)
                { // pripona SPL, pridame do seznamu nalezenych...
                    strcpy(s, data.cFileName);
                    str = DupStr(buf);
                    if (str != NULL)
                    {
                        foundFiles.Add(str);
                        if (!foundFiles.IsGood())
                        {
                            // foundFiles.ResetState();   // vola se mimo metodu, vyuziva se pro detekci chyby
                            free(str); // smula ...
                        }
                    }
                    else
                        TRACE_E(LOW_MEMORY);
                }
            }
            else // jde o adresar
            {
                if (data.cFileName[0] != 0 && strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0)
                { // nejde o "." a "..", jdeme prohledat podadresar...
                    strcpy(s, data.cFileName);
                    SearchForSPLs(buf, s + strlen(s), foundFiles, data);
                }
            }
        } while (FindNextFile(find, &data));
        HANDLES(FindClose(find));
    }
}

BOOL SearchForAddedSPLs(char* buf, char* s, TIndirectArray<char>& foundFiles)
{ // vraci TRUE pokud se maji doinstalovat plug-iny z 'foundFiles' a vsechny plug-iny loadnout
    strcpy(s, "\\plugins.ver");
    HANDLE file = HANDLES_Q(CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                       FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (file != INVALID_HANDLE_VALUE)
    {
        BOOL isPluginVerNew = FALSE;
        int lastPluginVer = 0;
        char line[MAX_PATH + 20];
        BOOL isEOF = FALSE;
        BOOL firstRow = TRUE;
        DWORD read, off;
        off = 0;
        while (isEOF || ReadFile(file, line + off, firstRow ? 20 : MAX_PATH + 20 - off, &read, NULL))
        {
            if (read == 0)
                isEOF = TRUE;              // EOF, uz nema smysl cist soubor, jen zpracujeme zbytek bufferu
            char* end = line + off + read; // konec platnych bytu v bufferu
            char* eol = line;              // prvni byte EOLu (ukazuje konec radky)
            while (eol < end && (*eol == '\r' || *eol == '\n'))
                eol++;         // preskok EOLu (i vice)
            char* start = eol; // prvni byte radky
            while (eol < end && *eol != '\r' && *eol != '\n')
                eol++; // hledani EOLu

            // (start, eol) - dalsi radka, jdeme ji zpracovat
            if (start < eol) // neni prazdna radka
            {
                char* sep = start;
                while (sep < eol && *sep != ':')
                    sep++;
                char num[20];
                lstrcpyn(num, start, (int)min(20, sep - start + 1));
                int ver = atoi(num);
                char name[MAX_PATH];
                if (sep + 1 < eol)
                {
                    lstrcpyn(name, sep + 1, (int)min(MAX_PATH, (eol - sep) - 1 + 1));
                }
                else
                    name[0] = 0;

                // (ver + name) - obsah prave nacteneho radku
                if (firstRow) // prvni radek souboru
                {
                    firstRow = FALSE;
                    if (ver <= Configuration.LastPluginVer)
                        break; // tenhle plugins.ver jsme uz cetli
                    else       // novejsi verze, musime zpracovat cely soubor
                    {
                        lastPluginVer = Configuration.LastPluginVer;
                        Configuration.LastPluginVer = ver; // ulozime si cislo tohoto souboru plugins.ver
                        isPluginVerNew = TRUE;             // minimalne budeme muset provest load vsech plug-inu
                    }
                }
                else // dalsi radky souboru
                {
                    if (ver > lastPluginVer && // jde o nove pridany plug-in
                        name[0] != 0)          // jen pokud je radek o.k.
                    {
                        if ((name[0] != '\\' || name[1] != '\\') && // neni to UNC cesta
                            name[1] != ':')                         // neni to normal plna cesta
                        {                                           // cesta reativni k adresari plugins
                            int l = (int)(s - buf + 1);
                            memmove(name + l, name, min((int)strlen(name) + 1, MAX_PATH - l));
                            memcpy(name, buf, l); // pocita s tim, ze je v buf cesta i s backslashem
                        }
                        DWORD attrs = SalGetFileAttributes(name);
                        if (attrs != 0xFFFFFFFF && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
                        { // jmeno existuje a jde o soubor
                            char* str = DupStr(name);
                            if (str != NULL)
                            {
                                foundFiles.Add(str);
                                if (!foundFiles.IsGood())
                                {
                                    // foundFiles.ResetState();   // vola se mimo metodu, vyuziva se pro detekci chyby
                                    free(str); // smula ...
                                }
                            }
                            else
                                TRACE_E(LOW_MEMORY);
                        }
                    }
                }
            }

            // odrizneme prectenou cast bufferu
            off = (DWORD)(end - eol);
            if (off == 0 && isEOF)
                break;
            memmove(line, eol, off);
        }
        HANDLES(CloseHandle(file));
        return isPluginVerNew;
    }
    else
    {
        TRACE_I("Unable to open file plugins.ver");
        return FALSE; // nic noveho
    }
}

#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
BOOL IsPluginUnsupportedOnX64(const char* dllName, const char** pluginNameEN)
{
    const char* nameEN = "";
    if (_stricmp(dllName, "winscp\\winscp.spl") == 0)
        nameEN = "WinSCP (SFTP/SCP Client)";
    if (pluginNameEN != NULL)
        *pluginNameEN = nameEN;
    return nameEN[0] != 0;
}
#endif // _WIN64

BOOL CPlugins::ReadPluginsVer(HWND parent, BOOL importFromOldConfig)
{
    CALL_STACK_MESSAGE2("CPlugins::ReadPluginsVer(, %d)", importFromOldConfig);

    BOOL ret = FALSE;

    // ziskani adresare "plugins"
    char buf[MAX_PATH + 20];
    GetModuleFileName(HInstance, buf, MAX_PATH);
    char* s = strrchr(buf, '\\');
    if (s != NULL)
    {
        strcpy(s + 1, "plugins");
        s = s + strlen(s);
    }
    else
    {
        TRACE_E("Unexpected situation in CPlugins::ReadPluginsVer().");
        return ret; // nenastane
    }

    LoadInfoBase |= LOADINFO_NEWPLUGINSVER;

    // budeme hledat pridane pluginy uvedene v plugins.ver souboru
    TIndirectArray<char> foundFiles(10, 10);
    if (SearchForAddedSPLs(buf, s, foundFiles))
    {
        ret = TRUE;
        // nejdrive odinstalujeme pluginy, ktere jiz nemaji .spl soubor (nejsou dale podporovany)
        RemoveNoLongerExistingPlugins(!importFromOldConfig); // nesmime mazat klic pluginu v registry, pokud jde o import z konfigurace predchozi verze Salamandera

        CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
        char textProgress[1000];
        _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), LoadStr(IDS_AUTOINSTALLPLUGINS_INIT));
        analysing.SetText(textProgress);
        analysing.Create();

        // pro progress napocitame pocet stavajicich pluginu, ktere budeme pozdeji loadit
        int toLoadCount = GetNumOfPluginsToLoad();

        // nastavime celkovy progress
        analysing.SetProgressMax((foundFiles.IsGood() ? foundFiles.Count : 0) + toLoadCount);
        int progress = 0;

        if (foundFiles.IsGood())
        {
            *s = 0; // opravime cestu v buf
            char pluginName[MAX_PATH];
            for (int i = 0; i < foundFiles.Count; i++)
            {
                char* file = foundFiles[i];
                if (StrNICmp(file, buf, (int)strlen(buf)) == 0 && file[strlen(buf)] == '\\')
                {
                    memmove(pluginName, file + strlen(buf) + 1, strlen(file) - strlen(buf) + 1 - 1);
                }
                else
                    strcpy(pluginName, file);
                int index;
                if (!Plugins.FindDLL(pluginName, index))
                {
                    _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), pluginName);
                    analysing.SetText(textProgress);

                    Plugins.AddPlugin(parent, pluginName); // co pridame, to bude uz nacteny (loadem se testuje, jestli je to vubec plugin)
                }
                analysing.SetProgressPos(++progress);
            }
        }
        else
            foundFiles.ResetState();

        // provedeme load vsech plug-inu, aby si obnovily data v Salamanderu...
        for (int i = 0; i < Data.Count; i++)
        {
            if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
                && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), Data[i]->DLLName);
                analysing.SetText(textProgress);

                Data[i]->InitDLL(parent, TRUE, FALSE); // potlacime zoufale Xnasobne blikani kurzoru

                UpdateWindow(MainWindow->HWindow); // nechame prekreslit okna po messageboxech
                analysing.SetProgressPos(++progress);
            }
        }

        DestroyWindow(analysing.HWindow);
    }

    LoadInfoBase &= ~LOADINFO_NEWPLUGINSVER;

    return ret;
}

void CPlugins::ClearHistory(HWND parent)
{
    CALL_STACK_MESSAGE1("CPlugins::ClearHistory()");

    // provedeme load vsech plug-inu a zavolani jejich metody ClearHistory;
    // nechame je naloadene, aby vzdy (i pri vypnutem "save on exit") sel provest
    // Save konfigurace -> cistka v Registry; pri loadu budeme hlasit chyby, aby
    // uzivatel vedel o pripadnych problemech v cistenim historie v pluginu
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        Data[i]->ClearHistory(parent);
    }
}

BOOL CPlugins::TestAll(HWND parent)
{
    CALL_STACK_MESSAGE1("CPlugins::TestAll()");

    BOOL ret = FALSE;
    BOOL err = FALSE;

    CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
    char textProgress[1000];
    _snprintf_s(textProgress, _TRUNCATE, "%s\n", LoadStr(IDS_LOADINGPLUGINS));
    analysing.SetText(textProgress);
    analysing.Create();

    // pro progress napocitame pocet pluginu, ktere budeme loadit
    int toLoadCount = GetNumOfPluginsToLoad();
    analysing.SetProgressMax(toLoadCount);

    int progress = 0;
    for (int i = 0; i < Data.Count; i++)
    {
        BOOL wasLoaded = Data[i]->GetLoaded();
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
        if (IsPluginUnsupportedOnX64(Data[i]->DLLName))
            continue;
#endif // _WIN64
        if (!wasLoaded)
        {
            _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_LOADINGPLUGINS), Data[i]->DLLName);
            analysing.SetText(textProgress);
        }
        if (!Data[i]->InitDLL(parent, FALSE, FALSE))
            err = TRUE;
        else
        {
            if (!wasLoaded)
            {
                ret = TRUE;
                UpdateWindow(MainWindow->HWindow); // nechame prekreslit okna po messageboxech
                analysing.SetProgressPos(++progress);
            }
        }
    }

    DestroyWindow(analysing.HWindow);

    if (!err)
    {
        SalMessageBox(parent, LoadStr(IDS_PLUGINTESTALLOK), LoadStr(IDS_INFOTITLE),
                      MB_OK | MB_ICONINFORMATION);
    }
    return ret;
}

void CPlugins::LoadAll(HWND parent)
{
    CALL_STACK_MESSAGE1("CPlugins::LoadAll()");

    // pro progress napocitame pocet pluginu, ktere budeme loadit
    int toLoadCount = GetNumOfPluginsToLoad();
    if (toLoadCount > 0)
    {
        CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
        char textProgress[1000];
        _snprintf_s(textProgress, _TRUNCATE, "%s\n", LoadStr(IDS_LOADINGPLUGINS));
        analysing.SetText(textProgress);
        analysing.Create();

        analysing.SetProgressMax(toLoadCount);

        int progress = 0;
        for (int i = 0; i < Data.Count; i++)
        {
            if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
                && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_LOADINGPLUGINS), Data[i]->DLLName);
                analysing.SetText(textProgress);

                Data[i]->InitDLL(parent, TRUE);

                UpdateWindow(MainWindow->HWindow); // nechame prekreslit okna po messageboxech
                analysing.SetProgressPos(++progress);
            }
        }

        DestroyWindow(analysing.HWindow);
    }
}

void CPlugins::HandleLoadOnStartFlag(HWND parent)
{
    CALL_STACK_MESSAGE1("CPlugins::HandleLoadOnStartFlag()");

    LoadInfoBase |= LOADINFO_LOADONSTART;

    // provedeme load vsech plug-inu s flagem load-on-start...
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (!Data[i]->GetLoaded() && Data[i]->LoadOnStart)
        {
            Data[i]->InitDLL(parent, TRUE);
        }
    }

    LoadInfoBase &= ~LOADINFO_LOADONSTART;
}

void CPlugins::GetCmdAndUnloadMarkedPlugins(HWND parent, int* cmd, CPluginData** data)
{
    CALL_STACK_MESSAGE1("CPlugins::GetCmdAndUnloadMarkedPlugins(, ,)");

    // provedeme rebuild menu a unload vsech plug-inu, ktere si pozadaly...
    BeginStopRefresh();
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->ShouldRebuildMenu)
        {
            p->ShouldRebuildMenu = FALSE;
            if (p->GetLoaded()) // pokud plugin mezitim user rucne unloadnul, zadost o rebuild zahodime
            {
                if (p->SupportDynMenuExt)
                {
                    p->BuildMenu(parent, TRUE);
                    p->ReleasePluginDynMenuIcons(); // tenhle objekt nikdo nepotrebuje (pro dalsi zobrazeni menu se vse ziska znovu)
                }
                else
                    TRACE_I("CSalamanderGeneral::GetCmdAndUnloadMarkedPlugins(): call ignored because plugin has not dynamic menu (see FUNCTION_DYNAMICMENUEXT)");
            }
        }
        if (p->ShouldUnload && (p->Commands.Count == 0 || p->Commands[0] == -1)) // nestoji o zadne prikazy nebo nastala chyba
        {
            p->ShouldUnload = FALSE;
            if (p->GetLoaded())
                p->Unload(parent, FALSE); // save-config bez ptani + unload
        }
    }
    EndStopRefresh();

    // najdeme prvni prikaz Salamandera/menu, o ktery si plug-iny pozadaly
    *cmd = -1;    // -1 == "nenalezen"
    *data = NULL; // NULL == "nenalezen"
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->Commands.Count > 0)
        {
            if (!p->Commands.IsGood())
                p->Commands.ResetState(); // aby slo jiste vybrat (Add() se nekontroluje)
            *cmd = p->Commands[0];        // prvni prikaz, ktery je na rade
            p->Commands.Delete(0);        // vyhodime tento prikaz z fronty
            if (!p->Commands.IsGood())
            {
                p->Commands.ResetState();
                p->Commands[0] = -1; // prevence proti nekonecnemu cyklu v pripade chyby vyhozeni prikazu
            }
            *data = p;
            break; // nechame prikaz provest
        }
    }
}

void CPlugins::OpenPackOrUnpackDlgForMarkedPlugins(CPluginData** data, int* pluginIndex)
{
    CALL_STACK_MESSAGE1("CPlugins::OpenPackOrUnpackDlgForMarkedPlugins()");

    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->OpenPackDlg || p->OpenUnpackDlg)
        {
            *pluginIndex = i;
            *data = p;
            return;
        }
    }
    *pluginIndex = -1;
    *data = NULL; // nenalezen
}

void CPlugins::RemoveNoLongerExistingPlugins(BOOL canDelPluginRegKey, BOOL loadAllPlugins,
                                             char* notLoadedPluginNames, int notLoadedPluginNamesSize,
                                             int maxNotLoadedPluginNames, int* numOfSkippedNotLoadedPluginNames,
                                             HWND parent)
{
    char buf[MAX_PATH + 20];
    GetModuleFileName(HInstance, buf, MAX_PATH);
    char* s = strrchr(buf, '\\');
    if (s != NULL)
    {
        strcpy(s + 1, "plugins\\");
        s = s + strlen(s);
    }
    else
    {
        TRACE_E("Unexpected situation in CPlugins::RemoveNoLongerExistingPlugins().");
        return; // nenastane
    }

    if (notLoadedPluginNames != NULL && notLoadedPluginNamesSize > 0)
        *notLoadedPluginNames = 0;
    if (numOfSkippedNotLoadedPluginNames != NULL)
        *numOfSkippedNotLoadedPluginNames = 0;

    CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
    char textProgress[1000];
    if (loadAllPlugins)
    {
        _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), LoadStr(IDS_AUTOINSTALLPLUGINS_INIT));
        analysing.SetText(textProgress);
        analysing.Create();
    }

    // odinstalujeme pluginy, ktere jiz nemaji .spl soubor (nejsou dale podporovany)
    int numOfNotLoaded = 0;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (!Data[i]->GetLoaded() // tyka se jen neloadlych pluginu
#ifdef _WIN64                     // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
            && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
        )
        {
            char* fullName = Data[i]->DLLName;
            if ((*fullName != '\\' || *(fullName + 1) != '\\') && // ne UNC
                (*fullName == 0 || *(fullName + 1) != ':'))       // ne "c:" -> relativni cesta k podadresari plugins
            {
                strcpy(s, fullName);
                fullName = buf;
            }

            DWORD attr = SalGetFileAttributes(fullName);
            DWORD err = GetLastError();
            if (attr == INVALID_FILE_ATTRIBUTES &&
                (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND || err == ERROR_BAD_PATHNAME))
            {
                char pluginName[MAX_PATH];
                pluginName[0] = 0;
                if (notLoadedPluginNames != NULL && Data[i]->RegKeyName != NULL && Data[i]->RegKeyName[0] != 0 &&
                    _stricmp(Data[i]->DLLName, "fsearch\\fsearch.spl") != 0 && // fsearch chceme zamlcet (zbytecne neupozornovat, ze je pryc)
                    _stricmp(Data[i]->DLLName, "eroiica\\eroiica.spl") != 0 && // Eroiica chceme zamlcet (zbytecne neupozornovat, ze je pryc)
                    _stricmp(Data[i]->DLLName, "unace\\unace.spl") != 0 &&     // UnACE chceme zamlcet (zbytecne neupozornovat, ze je pryc)
                    _stricmp(Data[i]->DLLName, "diskcopy\\diskcopy.spl") != 0) // DiskCopy chceme zamlcet (zbytecne neupozornovat, ze je pryc)
                {
                    lstrcpyn(pluginName, Data[i]->Name, MAX_PATH); // ma-li klic v registry, ulozim si jeho jmeno
                }
                if (Remove(parent, i, canDelPluginRegKey))
                {
                    i--;
                    if (pluginName[0] != 0)
                    {
                        numOfNotLoaded++;
                        if (numOfNotLoaded <= maxNotLoadedPluginNames &&
                            (int)strlen(notLoadedPluginNames) + 2 /*", "*/ + (int)strlen(pluginName) + 1 /*null*/ <= notLoadedPluginNamesSize)
                        {
                            if (*notLoadedPluginNames != 0)
                                strcat(notLoadedPluginNames, ", ");
                            strcat(notLoadedPluginNames, pluginName);
                        }
                        else
                        {
                            if (numOfSkippedNotLoadedPluginNames != NULL)
                                (*numOfSkippedNotLoadedPluginNames)++;
                        }
                    }
                }
            }
        }
    }

    if (loadAllPlugins) // provedeme load vsech plug-inu, zajima nas ktere se nepovede naloadit (jejich konfigurace se neprenese ze stare verze do nove)
    {
        LoadInfoBase |= LOADINFO_NEWSALAMANDERVER;

        // pro progress napocitame pocet pluginu, ktere budeme loadit
        int toLoadCount = GetNumOfPluginsToLoad();
        analysing.SetProgressMax(toLoadCount);

        int progress = 0;
        for (i = 0; i < Data.Count; i++)
        {
            if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
                && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), Data[i]->DLLName);
                analysing.SetText(textProgress);

                if (!Data[i]->InitDLL(parent, TRUE, FALSE)) // potlacime zoufale Xnasobne blikani kurzoru
                {
                    if (notLoadedPluginNames != NULL && Data[i]->RegKeyName != NULL && Data[i]->RegKeyName[0] != 0)
                    { // ma-li klic v registry, ulozim si jeho jmeno
                        numOfNotLoaded++;
                        if (numOfNotLoaded <= maxNotLoadedPluginNames &&
                            (int)strlen(notLoadedPluginNames) + 2 /*", "*/ + (int)strlen(Data[i]->Name) + 1 /*null*/ <= notLoadedPluginNamesSize)
                        {
                            if (*notLoadedPluginNames != 0)
                                strcat(notLoadedPluginNames, ", ");
                            strcat(notLoadedPluginNames, Data[i]->Name);
                        }
                        else
                        {
                            if (numOfSkippedNotLoadedPluginNames != NULL)
                                (*numOfSkippedNotLoadedPluginNames)++;
                        }
                    }
                }

                UpdateWindow(MainWindow->HWindow); // nechame prekreslit okna po messageboxech
                analysing.SetProgressPos(++progress);
            }
        }

        LoadInfoBase &= ~LOADINFO_NEWSALAMANDERVER;

        DestroyWindow(analysing.HWindow);
    }
}

void CPlugins::AutoInstallStdPluginsDir(HWND parent)
{
    CALL_STACK_MESSAGE1("CPlugins::AutoInstallStdPluginsDir()");

    // ziskani adresare "plugins"
    char buf[MAX_PATH + 20];
    GetModuleFileName(HInstance, buf, MAX_PATH);
    char* s = strrchr(buf, '\\');
    if (s != NULL)
    {
        strcpy(s + 1, "plugins");
        s = s + strlen(s);
    }
    else
    {
        TRACE_E("Unexpected situation in CPlugins::AutoInstallStdPluginsDir().");
        return; // nenastane
    }

    CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
    char textProgress[1000];
    _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), LoadStr(IDS_AUTOINSTALLPLUGINS_INIT));
    analysing.SetText(textProgress);
    analysing.Create();

    // nejdrive odinstalujeme pluginy, ktere jiz nemaji .spl soubor (nejsou dale podporovany)
    RemoveNoLongerExistingPlugins(FALSE); // nesmime mazat klic pluginu v registry, jde o import z konfigurace predchozi verze Salamandera

    LoadInfoBase |= LOADINFO_NEWSALAMANDERVER;

    // budeme hledat *.spl v adresari "plugins" a jeho podadresarich
    TIndirectArray<char> foundFiles(10, 10);
    WIN32_FIND_DATA data;
    SearchForSPLs(buf, s, foundFiles, data);

    // pro progress napocitame pocet stavajicich pluginu, ktere budeme pozdeji loadit
    int toLoadCount = GetNumOfPluginsToLoad();

    // nastavime celkovy progress
    analysing.SetProgressMax((foundFiles.IsGood() ? foundFiles.Count : 0) + toLoadCount);
    int progress = 0;

    if (foundFiles.IsGood())
    {
        *s = 0; // opravime cestu v buf
        char pluginName[MAX_PATH];
        for (int i = 0; i < foundFiles.Count; i++)
        {
            char* file = foundFiles[i];
            if (StrNICmp(file, buf, (int)strlen(buf)) == 0 && file[strlen(buf)] == '\\')
            {
                memmove(pluginName, file + strlen(buf) + 1, strlen(file) - strlen(buf) + 1 - 1);
            }
            else
                strcpy(pluginName, file);
            int index;
            if (!Plugins.FindDLL(pluginName, index)
#ifdef _WIN64                                            // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
                && !IsPluginUnsupportedOnX64(pluginName) // hrozi jen v interni debug verzi (nekompletni plugin se kompiluje, i kdyz se pak nepousti mezi lidi)
#endif                                                   // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), pluginName);
                analysing.SetText(textProgress);

                if (Plugins.AddPlugin(parent, pluginName)) // co pridame, to bude uz nacteny (loadem se testuje, jestli je to vubec plugin)
                {
                    CPluginData* p = Plugins.Get(Plugins.GetCount() - 1);
                    if (StrICmp(p->DLLName, "nethood\\nethood.spl") == 0)
                    {
                        int index2 = AddPluginToOrder(p->DLLName, TRUE);
                        Plugins.ChangePluginsOrder(index2, 0);
                    }
                }
            }
            analysing.SetProgressPos(++progress);
        }
    }
    else
        foundFiles.ResetState();

    // provedeme load vsech plug-inu, aby si obnovily data v Salamanderu...
    for (int i = 0; i < Data.Count; i++)
    {
        if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
            && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
        )
        {
            _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), Data[i]->DLLName);
            analysing.SetText(textProgress);

            Data[i]->InitDLL(parent, TRUE, FALSE); // potlacime zoufale Xnasobne blikani kurzoru

            UpdateWindow(MainWindow->HWindow); // nechame prekreslit okna po messageboxech
            analysing.SetProgressPos(++progress);
        }
    }

    LoadInfoBase &= ~LOADINFO_NEWSALAMANDERVER;

    DestroyWindow(analysing.HWindow);
}

BOOL CPlugins::EnumInstalledModules(int* index, char* module, char* version)
{
    CALL_STACK_MESSAGE1("CPlugins::EnumInstalledModules()");
    if (*index == 0)
    {
        // ziskame plne jmeno salamand.exe
        GetModuleFileName(HInstance, module, MAX_PATH);
        // ziskame verzi
        const char* s = SALAMANDER_TEXT_VERSION;
        while (*s != 0 && (*s < '0' || *s > '9'))
            s++;
        strcpy(version, s);
        (*index)++;
        return TRUE;
    }
    else
    {
        if (*index > 0 && *index - 1 < Data.Count)
        {
            CPluginData* data = Data[*index - 1];
            // ziskame plne jmeno DLL
            char* s = data->DLLName;
            if ((*s != '\\' || *(s + 1) != '\\') && // ne UNC
                (*s == 0 || *(s + 1) != ':'))       // ne "c:" -> relativni cesta k packers
            {
                GetModuleFileName(HInstance, module, MAX_PATH);
                s = strrchr(module, '\\') + 1;
                strcpy(s, "plugins\\");
                strcat(s, data->DLLName);
            }
            else
                strcpy(module, s);
            // ziskame verzi
            strcpy(version, data->Version);
            (*index)++;
            return TRUE;
        }
    }
    return FALSE;
}

void CPlugins::Event(int event, DWORD param)
{
    CALL_STACK_MESSAGE2("CPlugins::Event(%d,)", event);
    int i;
    for (i = 0; i < Data.Count; i++)
        Data[i]->Event(event, param);
}

void CPlugins::AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CPlugins::AcceptChangeOnPathNotification(%s, %d)", path, includingSubdirs);
    int i;
    for (i = 0; i < Data.Count; i++)
        Data[i]->AcceptChangeOnPathNotification(path, includingSubdirs);
}

BOOL CPlugins::FindHotKey(WORD hotKey, BOOL ignoreSkillLevels, const CPluginData* ignorePlugin, int* pluginIndex, int* menuItemIndex)
{
    CALL_STACK_MESSAGE3("CPlugins::FindHotKey(%u, %d, , )", hotKey, ignoreSkillLevels);
    if (hotKey == 0)
        return FALSE;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p != ignorePlugin && p->MenuItems.Count > 0) // ma nejake polozky
        {
            int j;
            for (j = 0; j < p->MenuItems.Count; j++)
            {
                // ctime SkillLevel redukci?
                if (ignoreSkillLevels || (p->MenuItems[j]->SkillLevel & CfgSkillLevelToMenu(Configuration.SkillLevel)))
                {
                    if (HOTKEY_GET(p->MenuItems[j]->HotKey) == hotKey)
                    {
                        *pluginIndex = i;
                        *menuItemIndex = j;
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

void CPlugins::RemoveHotKey(WORD hotKey, const CPluginData* ignorePlugin)
{
    CALL_STACK_MESSAGE2("CPlugins::RemoveHotKey(%u, )", hotKey);
    if (hotKey == 0)
        return;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p != ignorePlugin && p->MenuItems.Count > 0) // ma nejake polozky
        {
            int j;
            for (j = 0; j < p->MenuItems.Count; j++)
            {
                if (HOTKEY_GET(p->MenuItems[j]->HotKey) == hotKey)
                {
                    p->MenuItems[j]->HotKey = (p->MenuItems[j]->HotKey & ~HOTKEY_MASK) | HOTKEY_DIRTY; // nechceme, aby tuto zmenu prevalcoval Connect() pluginu
                }
            }
        }
    }
}

// bit 24: Specifies whether the key is an extended key, such as the right-hand ALT and CTRL keys that
// appear on an enhanced 101- or 102-key keyboard. The value is 1 if it is an extended key; otherwise, it is 0.
#define F_EXT 0x01000000

BOOL CPlugins::QueryHotKey(WPARAM wParam, LPARAM lParam, int* pluginIndex, int* menuItemIndex)
{
    CALL_STACK_MESSAGE3("CPlugins::QueryHotKey(0x%IX, 0x%IX, , )", wParam, lParam);
    BYTE vk = UpperCase[(BYTE)wParam]; // meli bychom byt case insensitive, uzivatel muze mit omylem zmackly capslock a hotkey editline se vzdy tvari, ze klavesa je velka
    if (vk == 0)
        return FALSE; // u pluginu jsem zakazal hot keys z vk==0
    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
    BYTE mods = 0;
    if (controlPressed)
        mods |= HOTKEYF_CONTROL;
    if (shiftPressed)
        mods |= HOTKEYF_SHIFT;
    if (altPressed)
        mods |= HOTKEYF_ALT;
    if (lParam & F_EXT)
        mods |= HOTKEYF_EXT;
    WORD hotKey = (WORD)((WORD)vk | (WORD)mods << 8);

    return FindHotKey(hotKey, FALSE, NULL, pluginIndex, menuItemIndex);
}

BOOL CPlugins::HandleKeyDown(WPARAM wParam, LPARAM lParam, CFilesWindow* activePanel, HWND hParent)
{
    CALL_STACK_MESSAGE3("CPlugins::HandleKeyDown(0x%IX, 0x%IX, , )", wParam, lParam);

    int pluginIndex;
    int menuItemIndex;
    if (QueryHotKey(wParam, lParam, &pluginIndex, &menuItemIndex))
    {
        // nasli jsme command s nasi horkou klavesou

        // nechceme nechat WM_SYSCHAR propadnout do menu, ktere otevira naprilad Automation na Alt+Shift+A
        // protoze by doslo ke spusteni prikazu od 'A' -- vypumpuji message queue
        // poznamka: puvodne jsem si myslel, ze by bylo resnei volat HandleKeyDown() az z WM_CHAR a WM_SYSCHAR,
        // ale napriklad na kombinaci Ctrl+Shift+Alt+pismeno ani WM_CHAR ani WM_SYSCHAR neprijdou,
        // takze je potreba pouzivat WM_KEYDOWN a WM_SYSKEYDOWN, ktere ovsem nasledne posilaji WM_CHAR a WM_SYSCHAR
        MSG msg;
        PeekMessage(&msg, hParent, WM_SYSCHAR, WM_SYSCHAR, PM_REMOVE);
        PeekMessage(&msg, hParent, WM_CHAR, WM_CHAR, PM_REMOVE);

        // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        if (ExecuteCommand(pluginIndex, menuItemIndex, activePanel, hParent))
        {
            activePanel->SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
            PostMessage(activePanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
        }

        // opet zvysime prioritu threadu, operace dobehla
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        // obnova obsahu neautomatickych panelu je na plug-inech

        UpdateWindow(hParent);

        return TRUE;
    }
    return FALSE;
}

void CPlugins::SetLastPlgCmd(const char* dllName, int id)
{
    CALL_STACK_MESSAGE3("CPlugins::SetLastPlgCmd(%s, %d)", dllName, id);
    if (LastPlgCmdPlugin != NULL)
        free(LastPlgCmdPlugin);
    LastPlgCmdPlugin = DupStr(dllName); // pokud se alokace nepovede, LastPlgCmdPlugin bude NULL -- v menu bude default polozka
    LastPlgCmdID = id;
}

int CPlugins::GetPluginSaveCount()
{
    CALL_STACK_MESSAGE1("CPlugins::GetPluginSaveCount()");
    int loadedCount = 0;
    if (::Configuration.AutoSave)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
            if (Data[i]->GetLoaded() && Data[i]->SupportLoadSave)
                loadedCount++;
    }
    return loadedCount;
}

void CPlugins::ClearLastSLGNames()
{
    int i;
    for (i = 0; i < Data.Count; i++)
        if (Data[i]->LastSLGName != NULL)
        {
            free(Data[i]->LastSLGName);
            Data[i]->LastSLGName = NULL;
        }
}

BOOL CPlugins::GetFirstNethoodPluginFSName(char* fsName, CPluginData** nethoodPlugin)
{
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->PluginIsNethood && p->SupportFS && p->FSNames.Count > 0)
        {
            if (fsName != NULL)
                lstrcpyn(fsName, p->FSNames[0], MAX_PATH);
            if (nethoodPlugin != NULL)
                *nethoodPlugin = p;
            return TRUE;
        }
    }
    return FALSE;
}

// *********************************************************************************************

void CPlugins::PasswordManagerEvent(HWND parent, int event)
{
    CALL_STACK_MESSAGE2("CPlugins::PasswordManagerEvent(, %d)", event);

    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->PluginUsesPasswordManager &&
            p->InitDLL(parent, FALSE, TRUE, FALSE))
        {
            p->PasswordManagerEvent(parent, event);
        }
    }
}

int CPlugins::GetNumOfPluginsToLoad()
{
    int toLoadCount = 0;
    for (int i = 0; i < Data.Count; i++)
    {
        if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
            && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
        )
        {
            toLoadCount++;
        }
    }
    return toLoadCount;
}

void CPlugins::ReleasePluginDynMenuIcons()
{
    int i;
    for (i = 0; i < Data.Count; i++)
        Data[i]->ReleasePluginDynMenuIcons();
}

#ifdef _DEBUG
#undef memcpy
void* _sal_safe_memcpy(void* dest, const void* src, size_t count)
{
    if ((char*)dest + count > src && (char*)src + count > dest)
    {
        TRACE_E("_sal_safe_memcpy: source and destination of memcpy overlap!");
    }
    return memcpy(dest, src, count);
}
#endif // _DEBUG

// POZOR: _sal_safe_memcpy musi lezet na konci modulu!!!
