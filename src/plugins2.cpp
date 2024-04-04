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

// Header for saving DIB to the registry

#define DIB_METHOD_STORE 1   // prime storage 1:1
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
// Creates a new bitmap with a depth of 24 bits, copies the source into it
// bitmap and converts it to grayscale (leaving the transparent color untouched).
//

BOOL CreateGrayscaleDIB(HBITMAP hSource, COLORREF transparent, HBITMAP& hGrayscale)
{
    CALL_STACK_MESSAGE1("CreateGrayscaleDIB(, , ,)");
    BOOL ret = FALSE;
    VOID* lpvBits = NULL;
    hGrayscale = NULL;
    HDC hDC = HANDLES(GetDC(NULL));

    // retrieve bitmap dimensions
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // we do not want a palette

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

    // desired color depth is 24 bits
    bi.bmiHeader.biSizeImage = ((((bi.bmiHeader.biWidth * 24) + 31) & ~31) >> 3) * bi.bmiHeader.biHeight;
    // allocate the necessary space
    lpvBits = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBits == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }

    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    // extract my own data
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

    // convert to grayscale
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

    // create a new bitmap over grayscale data
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

/*  BOOL
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

  // try to compress the data using codecs
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

  BYTE *buff = (BYTE*)malloc(bufferSize + 4);  // +4 because CStaticHuffmanCodec::DecodeBuffer() reads 4 bytes beyond the buffer (values are not used, but reading outside the allocated buffer sometimes caused exceptions)
  if (buff == NULL)
  {
    TRACE_E(LOW_MEMORY);
    return FALSE;
  }
  *(DWORD *)(buff + bufferSize) = 0;  // zeroing buffer overflow, just to avoid random values
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
}*/
/*  #define ROP_DSna 0x00220326
HICON GetIconFromDIB(HBITMAP hBitmap, int index)
{
  // create a B&W mask + color bitmap based on the screen
  HDC hDC = HANDLES(GetDC(NULL));
  HBITMAP hMask = HANDLES(CreateBitmap(16, 16, 1, 1, NULL));
  HBITMAP hColor = HANDLES(CreateCompatibleBitmap(hDC, 16, 16));
  HANDLES(ReleaseDC(NULL, hDC));

  hDC = HANDLES(CreateCompatibleDC(NULL));
  SelectObject(hDC, hColor);

  // borrow hDCMask to copy the source bitmap to hDC
  HDC hDCMask = HANDLES(CreateCompatibleDC(NULL));
  SelectObject(hDCMask, hBitmap);
  BitBlt(hDC, 0, 0, 16, 16, hDCMask, 0, 0, SRCCOPY);

  // select the mask in hDCMask
  SelectObject(hDCMask, hMask);

  // purple is the transparent color
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
}*/
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
                if (StrICmp(p->FSNames[j], fsName1) == 0) // found fsName1
                {
                    int k;
                    for (k = 0; k < p->FSNames.Count; k++)
                    {
                        if (StrICmp(p->FSNames[k], fsName2) == 0)
                        {
                            fsName2Index = k;
                            return TRUE; // fsName2 was also found in the same plugin
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
    // we need to know the path to the plugin that the last command belonged to
    if (LastPlgCmdPlugin != NULL)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CPluginData* p = Data[i];
            // find the plugin
            if (stricmp(p->DLLName, LastPlgCmdPlugin) == 0)
            {
                if (p->GetLoaded()) // if the plugin is not loaded, we will pretend that we found nothing
                {
                    // last-cmd from dynamic menu: rebuild necessary to show the updated item
                    // version of the menu (otherwise, after opening a submenu and rebuilding the menu, the last-cmd item could disappear)
                    if (!rebuildDynMenu || !p->SupportDynMenuExt || p->BuildMenu(parent, FALSE))
                    {
                        // find LastPlgCmdID in the plugin menu
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
                break; // another match of DLLName is no longer imminent
            }
        }
        // Command not found, we will clear the Last Command item to prevent "reviving" old commands after reloading the plugin or adding an item back to the dynamic menu.
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
    if (FindLastCommand(&pluginIndex, &menuItemIndex, FALSE, parent)) // if the plugin is not loaded, return FALSE
    {
        // OnLastCommand can be invoked via Ctrl+I, so the menu didn't have to be displayed at all
        // and therefore I don't have to count cache states
        CalculateStateCache();

        CPluginData* pluginData = Data[pluginIndex];

        MENU_ITEM_INFO dummy;
        if (pluginData->GetMenuItemStateType(pluginIndex, menuItemIndex, &dummy))
        {
            BOOL unselect;
            if (pluginData->ExecuteMenuItem2(panel, parent, pluginIndex, LastPlgCmdID, unselect))
            {
                return unselect; // processed, finishing
            }
        }
    }
    return FALSE;
}

BOOL CPlugins::ExecuteCommand(int pluginIndex, int menuItemIndex, CFilesWindow* panel, HWND parent)
{
    CalculateStateCache();

    CPluginData* pluginData = Data[pluginIndex];

    if (pluginData->InitDLL(parent)) // For calling GetMenuItemStateType(), we need PluginIfaceForMenuExt to be initialized
    {
        MENU_ITEM_INFO dummy;
        if (pluginData->GetMenuItemStateType(pluginIndex, menuItemIndex, &dummy))
        {
            int id = pluginData->MenuItems[menuItemIndex]->ID;
            BOOL unselect;
            if (pluginData->ExecuteMenuItem2(panel, parent, pluginIndex, id, unselect))
            {
                return unselect; // processed, finishing
            }
        }
    }
    return FALSE;
}

void CPlugins::InitMenuItems(HWND parent, CMenuPopup* root)
{
    // build individual submenus for plugins

    // Clear SUID and allow possible calls to BuildMenu() in all menus
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        Data[i]->ClearSUID();
        Data[i]->DynMenuWasAlreadyBuild = FALSE;
    }

    LastSUID = CM_PLUGINCMD_MIN; // initialize the counter for generating SUID

    // we will clean up items from yesterday or find out the original number of items in the root menu
    int count = root->GetItemCount();
    if (RootMenuItemsCount == -1)
        RootMenuItemsCount = count;
    if (count > RootMenuItemsCount)
        if (!root->RemoveItemsRange(RootMenuItemsCount, count - 1))
            return; // error

    // Adding a submenu for plugins that have some items, cleaning up SUID items
    // (new numbers will be assigned)
    count = RootMenuItemsCount;
    UpdatePluginsOrder(Configuration.KeepPluginsSorted);
    for (i = 0; i < Order.Count; i++)
    {
        int orderIndex = Order[i].Index;
        CPluginData* p = Data[orderIndex];
        p->SubMenu = NULL;                                  // Currently, this plug-in does not have any submenus
        if (p->SupportDynMenuExt || p->MenuItems.Count > 0) // dynamic menu or has some items
        {
            // Thanks to the SkillLevel reduction, nothing has to remain in the submenu and we don't want empty submenus
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
                        { // When it comes to a submenu, we need to skip all of its items and submenus
                            int level = 1;
                            for (j++; j < p->MenuItems.Count; j++)
                            {
                                CPluginMenuItemType type = p->MenuItems[j]->Type;
                                if (type == pmitStartSubmenu)
                                    level++;
                                else
                                {
                                    if (type == pmitEndSubmenu && --level == 0)
                                        break; // end of submenu found
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
                { // first submenu -> separator needs to be added
                    mi.Mask = MENU_MASK_TYPE;
                    mi.Type = MENU_TYPE_SEPARATOR;
                    root->InsertItem(count++, TRUE, &mi);
                }

                // insert an empty subMenu of the plug-in
                mi.Mask = MENU_MASK_TYPE | MENU_MASK_SUBMENU | MENU_MASK_STRING |
                          MENU_MASK_IMAGEINDEX | MENU_MASK_ID;
                mi.Type = MENU_TYPE_STRING;

#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
                if (IsPluginUnsupportedOnX64(p->DLLName))
                {
                    mi.Mask |= MENU_MASK_STATE;
                    mi.State = MENU_STATE_GRAYED;
                }
#endif // _WIN64

                char pluginName[300];
                lstrcpyn(pluginName, p->Name, 299);
                DuplicateAmpersands(pluginName, 299); // Plugin name can contain the character '&'

                mi.String = pluginName;
                mi.ImageIndex = (p->PluginSubmenuIconIndex != -1) ? orderIndex : -1;
                mi.SubMenu = new CMenuPopup();
                p->SubMenu = (CMenuPopup*)mi.SubMenu; // assign this submenu to this plug-in
                mi.ID = CML_PLUGINS_SUBMENU;
                root->InsertItem(count++, TRUE, &mi);
            }
        }
    }

    // Calculate the new CPluginsStateCache::ActualStateMask and other variables (if it makes sense)
    // later CPluginData::GetMaskForMenuItems will determine the mask from them
    CalculateStateCache();

    // set the Last Command item
    char lastCmdStr[800];
    MENU_ITEM_INFO lcmii;
    lcmii.Type = MENU_TYPE_STRING;
    lcmii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_STATE | MENU_MASK_FLAGS;
    lcmii.Flags = MENU_FLAG_NOHOTKEY; // we do not want AssignHotKeys to assign a hot key to this item
    lcmii.String = lastCmdStr;
    BOOL setToDefaultItem = TRUE;
    int pluginIndex;
    int menuItemIndex;
    if (FindLastCommand(&pluginIndex, &menuItemIndex, TRUE, parent))
    {
        CPluginData* pluginData = Data[pluginIndex];
        CPluginMenuItem* menuItem = pluginData->MenuItems[menuItemIndex];
        if (menuItem->Name != NULL) // Display the item even if it does not match the current skill level
        {
            lcmii.State = 0;
            pluginData->GetMenuItemStateType(pluginIndex, menuItemIndex, &lcmii);

            lstrcpyn(lastCmdStr, pluginData->Name, 299);
            char* s = strchr(lastCmdStr, '('); // we will discard the text in parentheses from the plugin name ("WinSCP (SFTP/SCP Client)" -> "WinSCP")
            if (s != NULL)
            {
                char* e = strchr(s + 1, ')');
                if (s > lastCmdStr && *(s - 1) == ' ')
                    s--;
                if (e != NULL)
                    memmove(s, e + 1, strlen(e + 1) + 1);
            }
            DuplicateAmpersands(lastCmdStr, 299); // Plugin name can contain the character '&'
            strcat(lastCmdStr, ": ");
            int cmdNameOffset = (int)strlen(lastCmdStr);
            strcpy(lastCmdStr + cmdNameOffset, menuItem->Name);

            // If we have a hint in the text, we remove it
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

            // attach a hot-key from the original string
            const char* hotKey = LoadStr(IDS_MENU_PLG_LASTCMD);
            while (*hotKey != 0 && *hotKey != '\t')
                hotKey++;
            strcat(lastCmdStr, hotKey);
            // remove the ampersand so it doesn't mess up the hot keys for plugins
            RemoveAmpersands(lastCmdStr + cmdNameOffset);
            DuplicateAmpersands(lastCmdStr + cmdNameOffset, 500); // if the command contained &&, we need to return it
            setToDefaultItem = FALSE;
        }
    }
    if (setToDefaultItem)
    {
        // old command is not available, we will push (disabled) default item there
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
        tii.ID = CM_PLUGINCMD_MIN + orderIndex; // comes to mainwnd3 as WM_USER_TBDROPDOWN
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
    // Clear SUID and allow possible calls to BuildMenu() in all menus
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        Data[i]->ClearSUID();
        Data[i]->DynMenuWasAlreadyBuild = FALSE;
    }

    LastSUID = CM_PLUGINCMD_MIN; // initialize the counter for generating SUID
    CalculateStateCache();
    if (menu->GetPopupID() != CML_PLUGINS_SUBMENU)
        TRACE_E("CPlugins::InitPluginMenuItemsForBar() internal warning: wrong menu ID in, imagelists will not be destroyed!");
    plugin->InitMenuItems(parent, index, menu);
    menu->SetImageList(plugin->CreateImageList(TRUE), TRUE); // Destruction of the imagelist is done in CMainWindow::WindowProc / WM_USER_UNINITMENUPOPUP (the menu must have the ID CML_PLUGINS_SUBMENU)
    menu->SetHotImageList(plugin->CreateImageList(FALSE), TRUE);
    plugin->ReleasePluginDynMenuIcons(); // Icons are in the image lists of the menu, and this object is no longer needed (for the next display of the menu, everything is retrieved again)
    return TRUE;
}

void CPlugins::CalculateStateCache()
{
    StateCache.Clean();
    StateCache.ActualStateMask = MENU_EVENT_TRUE;
    //  if (count > RootMenuItemsCount)
    //  {
    // MENU_EVENT_THIS_PLUGIN_XXX and MENU_EVENT_TARGET_THIS_PLUGIN_XXX are being set for
    // each plug-in separately in CPluginData::InitMenuItems

    // MENU_EVENT_DISK, ActiveUnpackerIndex, ActivePackerIndex, ActiveFSIndex
    if (MainWindow->GetActivePanel()->Is(ptDisk))
        StateCache.ActualStateMask |= MENU_EVENT_DISK;
    else
    {
        if (MainWindow->GetActivePanel()->Is(ptZIPArchive))
        {
            int format = PackerFormatConfig.PackIsArchive(MainWindow->GetActivePanel()->GetZIPArchive());
            if (format != 0) // no error
            {
                format--;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0)
                    StateCache.ActiveUnpackerIndex = -index - 1; // it's a plug-in
                if (PackerFormatConfig.GetUsePacker(format))
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0)
                        StateCache.ActivePackerIndex = -index - 1; // it's a plug-in
                }
            }
        }
        else
        {
            if (MainWindow->GetActivePanel()->Is(ptPluginFS))
            {
                CPluginFSInterfaceEncapsulation* fs = MainWindow->GetActivePanel()->GetPluginFS();
                if (fs->NotEmpty()) // should be "always true"
                {                   // according to the iface we find the index of the plug-in in the current panel
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
            if (format != 0) // no error
            {
                format--;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0)
                    StateCache.NonactiveUnpackerIndex = -index - 1; // it's a plug-in
                if (PackerFormatConfig.GetUsePacker(format))
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0)
                        StateCache.NonactivePackerIndex = -index - 1; // it's a plug-in
                }
            }
        }
        else
        {
            if (MainWindow->GetNonActivePanel()->Is(ptPluginFS))
            {
                CPluginFSInterfaceEncapsulation* fs = MainWindow->GetNonActivePanel()->GetPluginFS();
                if (fs->NotEmpty()) // should be "always true"
                {                   // according to the iface we find the index of the plug-in in the inactive panel
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

    // MENU_EVENT_FILE_FOCUSED, MENU_EVENT_DIR_FOCUSED and MENU_EVENT_UPDIR_FOCUSED
    // + calculation of FileUnpackerIndex and FilePackerIndex
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
                    if (format != 0) // no error
                    {
                        format--;
                        int index = PackerFormatConfig.GetUnpackerIndex(format);
                        if (index < 0)
                            StateCache.FileUnpackerIndex = -index - 1; // it's a plug-in
                        if (PackerFormatConfig.GetUsePacker(format))
                        {
                            index = PackerFormatConfig.GetPackerIndex(format);
                            if (index < 0)
                                StateCache.FilePackerIndex = -index - 1; // it's a plug-in
                        }
                    }
                }
            }
        }
    }

    // MENU_EVENT_FILES_SELECTED and MENU_EVENT_DIRS_SELECTED
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
    // we will check if they are not crawling into some submenu of the plug-in
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->SubMenu == submenu) // Initialization of the plugin submenu
        {
            if (submenu->GetPopupID() != CML_PLUGINS_SUBMENU)
                TRACE_E("CPlugins::InitSubMenuItems() internal warning: wrong menu ID in, imagelists will not be destroyed!");
            p->InitMenuItems(parent, i, p->SubMenu);
            submenu->SetImageList(p->CreateImageList(TRUE), TRUE); // Destruction of the imagelist is done in CMainWindow::WindowProc / WM_USER_UNINITMENUPOPUP (the menu must have the ID CML_PLUGINS_SUBMENU)
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
            return unselect; // processed, finishing
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
            return helpDisplayed; // processed, finishing
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

    // preparing the Order array
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
            if (data->ThumbnailMasks.GetMasksString()[0] != 0 && // provides thumbnails
                !data->ThumbnailMasksDisabled)                   // his unload/remove is not happening
            {
                thumbLoaderPlugins.Add(data);
                if (!thumbLoaderPlugins.IsGood())
                {
                    // thumbLoaderPlugins.ResetState();  // the array state is reset outside the method (used as error detection)
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
            DuplicateAmpersands(pluginName, 299); // Plugin name can contain the character '&'

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
        return FALSE; // suppressing the assignment of hotkeys
    }
    return TRUE; // for added items we want hot keys
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
    drv.PluginFS = NULL; // just for order

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
    int remainingSize = sizeof(buf); // we will store in 'buf' a list of fs-names, the names will be separated by ':'
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
            if (Configuration.ConfigVersion < 7) // old version (saving functions after individual BOOLs)
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
            else // new version (storing functions in one DWORD bit by bit)
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

                // these values do not have to be loaded (they may be missing in the configuration)
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
                    err = FALSE; // there is an error, but let's try to recover from it
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
                            delete p->PluginIcons; // just to be sure (currently the bitmap is empty from the constructor)
                        p->PluginIcons = pluginIcons;
                        if (p->PluginIconsGray != NULL)
                        {
                            delete p->PluginIconsGray; // just to be sure (currently the bitmap is empty from the constructor)
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

                        pluginIcons = NULL; // Bitmaps no longer need to be deallocated

                        // Assigning icons here because it doesn't make sense in AddPlugin() (values need to be changed)
                        // when loading the plugin for the first time, until then we don't have a bitmap with icons)
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
                                // ChDrvMenuFSItemIconIndex is not saved if it is -1 (BTW, this also solves the conversion of old configuration)
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

                                // SkillLevel is stored only if it is different from MENU_SKILLLEVEL_ALL
                                // We saved the registry time and ensured the conversion of old configurations
                                if (!GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL, REG_DWORD, &skillLevel, sizeof(DWORD)))
                                    skillLevel = MENU_SKILLLEVEL_ALL;

                                // IconIndex is stored only if it is different from -1 (no icon)
                                // We saved registry time and ensured the conversion of old configurations;
                                // we do not save the icon in dynamic menu, so we do not save its index either
                                if (dynMenuExt ||
                                    !GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMICONINDEX, REG_DWORD, &iconIndex, sizeof(DWORD)))
                                    iconIndex = -1;

                                // The type is stored only if it is different from pmitItemOrSeparator
                                // We saved the registry time and ensured the conversion of old configurations
                                if (!GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMTYPE, REG_DWORD, &type, sizeof(DWORD)))
                                    type = pmitItemOrSeparator;

                                // HotKey is saved only if it is different from 0
                                // We saved the registry time and ensured the conversion of old configurations
                                if (!GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMHOTKEY, REG_DWORD, &hotKey, sizeof(DWORD)))
                                    hotKey = 0;

                                if (GetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMNAME, REG_SZ, name, MAX_PATH) &&
                                    stateLoaded && idLoaded &&
                                    (type == pmitItemOrSeparator || type == pmitStartSubmenu))
                                { // normal or start-submenu item of the menu
                                    p->AddMenuItem(iconIndex, name, hotKey, id, state == -1, HIWORD(state), LOWORD(state),
                                                   skillLevel, (CPluginMenuItemType)type);
                                }
                                else // separator or end-submenu
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

            // if we haven't assigned a bitmap, we need to release it
            if (pluginIcons != NULL)
                delete pluginIcons;

            if (err) // error, we can't afford that ...
            {
                HANDLES(EnterCriticalSection(&DataCS));
                Data.DestroyMembers();
                HANDLES(LeaveCriticalSection(&DataCS));
                break;
            }

            itoa(++i, buf, 10);
        }
    }
    else // default values
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

                if (p->LoadOnStart) // we will only save TRUE -> saving space in the registry
                {
                    DWORD loadOnStartDWORD = TRUE;
                    SetValue(itemKey, SALAMANDER_PLUGINS_LOADONSTART, REG_DWORD, &loadOnStartDWORD, sizeof(DWORD));
                }

                if (p->ChDrvMenuFSItemName != NULL) // we have the FS command to change-drive menu
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_FSCMDNAME, REG_SZ, p->ChDrvMenuFSItemName, -1);

                    // ChDrvMenuFSItemIconIndex is not saved if it is -1 (BTW, this also solves the conversion of old configuration)
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

                if (p->LastSLGName != NULL && p->LastSLGName[0] != 0) // we will save it if it is not an empty string
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_LASTSLGNAME, REG_SZ, p->LastSLGName, -1);
                }
                if (p->PluginHomePageURL != NULL && p->PluginHomePageURL[0] != 0) // we will save it if it is not an empty string
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_HOMEPAGE, REG_SZ, p->PluginHomePageURL, -1);
                }
                if (p->ThumbnailMasks.GetMasksString()[0] != 0) // we will save it if it is not an empty string
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_THUMBMASKS, REG_SZ, p->ThumbnailMasks.GetMasksString(), -1);
                }
                if (p->PluginIcons != NULL) // Save only if it exists
                {
                    SaveIconList(itemKey, SALAMANDER_PLUGINS_PLGICONLIST, p->PluginIcons);
                }
                if (p->PluginIconIndex != -1) // Save only if it is not -1
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_PLGICONINDEX, REG_DWORD, &(p->PluginIconIndex), sizeof(DWORD));
                }
                if (p->PluginSubmenuIconIndex != -1) // Save only if it is not -1
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_PLGSUBMENUICONINDEX, REG_DWORD, &(p->PluginSubmenuIconIndex), sizeof(DWORD));
                }
                if (!p->ShowSubmenuInPluginsBar) // Save only if not TRUE
                {
                    SetValue(itemKey, SALAMANDER_PLUGINS_SUBMENUINPLUGINSBAR, REG_DWORD, &(p->ShowSubmenuInPluginsBar), sizeof(DWORD));
                }

                HKEY menuKey;
                if (p->MenuItems.Count > 0 && CreateKey(itemKey, SALAMANDER_PLUGINS_MENU, menuKey))
                { // write new values
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
                                DWORD state = p->SupportDynMenuExt ? -1 : item->StateMask; // dynamic menu: this hack solves the situation when a plugin with a dynamic menu switches to a static menu during load, and then fails at the entry point (the content of the dynamic menu remains, and if there are no items with call-get-state in it, the menu can be displayed even without loading the plugin)
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSTATE, REG_DWORD,
                                         &state, sizeof(DWORD));
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMID, REG_DWORD,
                                         &(item->ID), sizeof(DWORD));
                            }
                            if (item->Name != NULL) // normal item - save the name
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMNAME, REG_SZ, item->Name, -1);

                            // SkillLevel is stored only if it is different from MENU_SKILLLEVEL_ALL
                            // We saved the registry time and ensured the conversion of old configurations
                            if (item->SkillLevel != MENU_SKILLLEVEL_ALL)
                            {
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL, REG_DWORD,
                                         &(item->SkillLevel), sizeof(DWORD));
                            }

                            // IconIndex is stored only if it is different from -1 (no icon)
                            // We saved registry time and ensured the conversion of old configurations;
                            // we do not save the icon in dynamic menu, so we do not save its index either
                            if (!p->SupportDynMenuExt && item->IconIndex != -1)
                            {
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMICONINDEX, REG_DWORD,
                                         &(item->IconIndex), sizeof(DWORD));
                            }

                            // The type is stored only if it is different from pmitItemOrSeparator
                            // We saved the registry time and ensured the conversion of old configurations
                            if (item->Type != pmitItemOrSeparator)
                            {
                                DWORD type = item->Type;
                                SetValue(menuItemKey, SALAMANDER_PLUGINS_MENUITEMTYPE, REG_DWORD, &type, sizeof(DWORD));
                            }

                            // HotKey is saved only if it is different from 0
                            // We saved the registry time and ensured the conversion of old configurations
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
            // notify the main window about the removal of the plugin
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
        CPluginData* p = Plugins.Get(-i - 1); // plugin index
        if (p != NULL)                        // the plug-in exists, we will now check if it has the required function
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
        return i < ArchiverConfig.GetArchiversCount(); // index of external archiver
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
        { // Here we are comparing pointers DLLName (allocated only once -> uniquely identifies the plug-in)
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
        // get the full name of the plugins directory
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
            if ((*s != '\\' || *(s + 1) != '\\') && // not UNC
                (*s == 0 || *(s + 1) != ':'))       // not "c:" -> relative path to plugins
            {
                strcpy(name, data->DLLName);
                s = fullDLLName;
            }
            int len = (int)strlen(s);
            if (len >= sufLen && StrNICmp(s + len - sufLen, dllSuffix, sufLen) == 0)
            {
                return data; // found
            }
        }
    }
    return NULL; // not found
}

void CPlugins::CheckData()
{
    CALL_STACK_MESSAGE1("CPlugins::CheckData()");
    if (!DefaultConfiguration) // no default configuration of the plug-in -> delete all old ZIP+TAR+PAK
    {                          // and we will perform the conversion of the old type "external" in "custom pack/unpack"
                               // and declare all data as new
                               // We will now clean up internal and convert external viewers
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
                    PackerConfig.DeletePacker(i--); // not external
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
                    UnpackerConfig.DeleteUnpacker(i--); // not external
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
                    if (viewerMasks->At(i)->ViewerType != 2) // not external
                    {
                        if (viewerMasks->At(i)->ViewerType != 0) // there is no internal hex/text viewer
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
    else // is the default configuration, we will change the old types external+ZIP+TAR+PAK in "custom pack/unpack"
    {    // and declare all data as new, then change the old types in "file viewer" and declare
        // data for new

        // PackerFormatConfig.GetPackerIndex(i) remains -1 for ZIP, -2 for TAR, -3 for PAK
        // PackerFormatConfig.GetUnpackerIndex(i) zustava -1 ZIP, -2 TAR, -3 PAK
        int i;
        for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
        {
            if (PackerFormatConfig.GetOldType(i))
                PackerFormatConfig.SetOldType(i, FALSE);
        }

        // Conversions for "custom pack"
        for (i = 0; i < PackerConfig.GetPackersCount(); i++)
        {
            if (PackerConfig.GetPackerOldType(i)) // we will recode old values
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
                    break; // error
                }
            }
        }

        // Conversions for "custom unpack"
        for (i = 0; i < UnpackerConfig.GetUnpackersCount(); i++)
        {
            if (UnpackerConfig.GetUnpackerOldType(i)) // we will recode old values
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
                    break; // error
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
                if (viewerMasks->At(i)->OldType) // we will recode old values
                {
                    viewerMasks->At(i)->OldType = FALSE;
                    switch (viewerMasks->At(i)->ViewerType)
                    {
                    case 0:
                        viewerMasks->At(i)->ViewerType = VIEWER_INTERNAL;
                        break; // internal viewer
                    case 1:
                        viewerMasks->At(i)->ViewerType = -4;
                        break; // IE viewer
                    case 2:
                        viewerMasks->At(i)->ViewerType = VIEWER_EXTERNAL;
                        break; // external
                    default:
                        viewerMasks->Delete(i--);
                        break; // error
                    }
                }
            }
        }
        MainWindow->LeaveViewerMasksCS();
    }

    // Check index/type of archiver/plugin, possibly delete faulty ones
    int i;
    for (i = 0; i < PackerFormatConfig.GetFormatsCount(); i++)
    {
        if (PackerFormatConfig.GetUsePacker(i))
        {
            if (!IsArchiveIndexOK(PackerFormatConfig.GetPackerIndex(i), pftPanelEdit))
            {
                TRACE_E("Invalid packer index in PackerFormatConfig, ext = " << PackerFormatConfig.GetExt(i)); // When importing configuration from version 2.0, this error is reported because UnCAB released with version 2.0 incorrectly reported that it can also compress (which is nonsense and the error is corrected here) - no further action is needed...
                PackerFormatConfig.SetUsePacker(i, FALSE);                                                     // bad index of the packer -> "packing is not supported"
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
            if (t != VIEWER_EXTERNAL &&                 // not external
                t != VIEWER_INTERNAL &&                 // not internal
                (t > 0 || Plugins.Get(-t - 1) == NULL)) // It's not even plug-in related
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
            // inform the main window about adding a plugin
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
            if (StrICmp(uniqueKeyName, Data[i]->RegKeyName) == 0) // not unique
            {
                sprintf(uniqueKeyName + strlen(regKeyName), " (%d)", number++); // change of key name
                i = -1;                                                         // we will perform the comparison again
            }
        }
    }
}

void CPlugins::GetUniqueFSName(char* uniqueFSName, const char* fsName, TIndirectArray<char>* uniqueFSNames,
                               TIndirectArray<char>* oldFSNames)
{
    CALL_STACK_MESSAGE2("CPlugins::GetUniqueFSName(, %s, ,)", fsName);
    int number = 2;
    lstrcpyn(uniqueFSName, fsName, MAX_PATH - 9); // Leave 9 characters at the end of fs-name reserved for digits to search for a unique name
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
            { // except for a possible suffix, we will check if the suffix is numeric (numbers are added when searching for a unique name)
                char* num = s + offset;
                while (*num != 0 && *num >= '0' && *num <= '9')
                    num++;
                if (*num == 0) // suffix is numeric -> this old fs-name can be used for the searched fs-name
                {
                    lstrcpyn(uniqueFSName + offset, s + offset, MAX_PATH - offset);
                    oldFSNameUsed = TRUE;
                    oldFSNames->Delete(i); // Remove the fs-name used from the array (repeated use is nonsense - even in the case of a non-unique name)
                    if (!oldFSNames->IsGood())
                        oldFSNames->ResetState(); // deletion was successful, we are not interested in anything else
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
                if (StrICmp(uniqueFSName, uniqueFSNames->At(j)) == 0) // not unique
                {
                    if (!oldFSNameUsed)
                        sprintf(uniqueFSName + offset, "%d", number++); // change of key name
                    else                                                // the old name is no longer unique, let's look for another unique name
                    {
                        oldFSNameUsed = FALSE;
                        uniqueFSName[offset] = 0;
                    }
                    i = -1; // we will perform the comparison again
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
            if (StrICmp(uniqueFSName, plugin->FSNames[j]) == 0) // not unique
            {
                if (!oldFSNameUsed)
                    sprintf(uniqueFSName + offset, "%d", number++); // change of key name
                else                                                // the old name is no longer unique, let's look for another unique name
                {
                    oldFSNameUsed = FALSE;
                    uniqueFSName[offset] = 0;
                }
                i = -1; // we will perform the comparison again
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
    // get an array of extensions from the string extensions
    char ext[300];
    int len = (int)strlen(extensions);
    if (len > 299)
        len = 299;
    memcpy(ext, extensions, len);
    ext[len] = 0;
    TDirectArray<char*> extArray(10, 5); // array suffix
    char* s = ext + len;
    while (s > ext)
    {
        while (--s >= ext && *s != ';')
            ;
        if (s >= ext)
            *s = 0;
        extArray.Add(s + 1); // be the first or some of the series suffix
    }

    char ext2[300]; // Copy Extensions sequentially from all plug-ins
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (i == exclude)
            continue; // this index cannot be a result

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
                if (StrICmp(s + 1, extArray[j]) == 0) // sets of suffixes have a non-empty intersection
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
                break; // We have found it, it doesn't make sense to continue with 'i'
        }
        if (viewFound && editFound)
            break;
    }

    if (!viewFound || !editFound)
    {
        // !!! WARNING: when changing the order of external archivers, the order in the array must also be changed
        // externalArchivers in the method CPlugins::FindViewEdit
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
                if (StrICmp(externalArchivers[i].ext, extArray[j]) == 0) // External archiver found
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

// Helper function for CPlugins::AutoInstallStdPluginsDir
void SearchForSPLs(char* buf, char* s, TIndirectArray<char>& foundFiles, WIN32_FIND_DATA& data)
{
    strcpy(s++, "\\*");
    HANDLE find = HANDLES_Q(FindFirstFile(buf, &data));
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) // it's about the file
            {
                char* str = strrchr(data.cFileName, '.');
                //        if (str != NULL && str > data.cFileName && StrICmp(str, ".spl") == 0) // ".cvspass" ve Windows je pripona ...
                if (str != NULL && StrICmp(str, ".spl") == 0)
                { // Adding the SPL extension to the list of found...
                    strcpy(s, data.cFileName);
                    str = DupStr(buf);
                    if (str != NULL)
                    {
                        foundFiles.Add(str);
                        if (!foundFiles.IsGood())
                        {
                            // foundFiles.ResetState();   // called outside of the method, used for error detection
                            free(str); // bad luck...
                        }
                    }
                    else
                        TRACE_E(LOW_MEMORY);
                }
            }
            else // it's about the directory
            {
                if (data.cFileName[0] != 0 && strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0)
                { // It's not about "." and "..", we are going to search the subdirectory...
                    strcpy(s, data.cFileName);
                    SearchForSPLs(buf, s + strlen(s), foundFiles, data);
                }
            }
        } while (FindNextFile(find, &data));
        HANDLES(FindClose(find));
    }
}

BOOL SearchForAddedSPLs(char* buf, char* s, TIndirectArray<char>& foundFiles)
{ // returns TRUE if the plug-ins from 'foundFiles' should be installed and all plug-ins loaded
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
                isEOF = TRUE;              // EOF, it no longer makes sense to read the file, we will just process the remaining buffer
            char* end = line + off + read; // end of valid characters in buffer
            char* eol = line;              // first byte of EOL (end of line)
            while (eol < end && (*eol == '\r' || *eol == '\n'))
                eol++;         // skip EOL (or more)
            char* start = eol; // first byte of the line
            while (eol < end && *eol != '\r' && *eol != '\n')
                eol++; // searching for EOL

            // (start, eol) - next line, let's process it
            if (start < eol) // not an empty line
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

                // (ver + name) - content of the currently read line
                if (firstRow) // first line of the file
                {
                    firstRow = FALSE;
                    if (ver <= Configuration.LastPluginVer)
                        break; // We have already read this plugins.ver
                    else       // newer version, we need to process the entire file
                    {
                        lastPluginVer = Configuration.LastPluginVer;
                        Configuration.LastPluginVer = ver; // we will save the number of this file as plugins.ver
                        isPluginVerNew = TRUE;             // we will have to load all the plug-ins at least
                    }
                }
                else // next lines of the file
                {
                    if (ver > lastPluginVer && // It's about a newly added plug-in
                        name[0] != 0)          // only if the line is okay
                    {
                        if ((name[0] != '\\' || name[1] != '\\') && // it is not a UNC path
                            name[1] != ':')                         // It's not a normal full path
                        {                                           // path relative to the plugins directory
                            int l = (int)(s - buf + 1);
                            memmove(name + l, name, min((int)strlen(name) + 1, MAX_PATH - l));
                            memcpy(name, buf, l); // expects that the path in the buffer includes a backslash
                        }
                        DWORD attrs = SalGetFileAttributes(name);
                        if (attrs != 0xFFFFFFFF && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
                        { // name exists and it is a file
                            char* str = DupStr(name);
                            if (str != NULL)
                            {
                                foundFiles.Add(str);
                                if (!foundFiles.IsGood())
                                {
                                    // foundFiles.ResetState();   // called outside of the method, used for error detection
                                    free(str); // bad luck...
                                }
                            }
                            else
                                TRACE_E(LOW_MEMORY);
                        }
                    }
                }
            }

            // Discard the read part of the buffer
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
        return FALSE; // nothing new
    }
}

#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
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

    // obtaining the directory "plugins"
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
        return ret; // will not occur
    }

    LoadInfoBase |= LOADINFO_NEWPLUGINSVER;

    // we will search for added plugins listed in the plugins.ver file
    TIndirectArray<char> foundFiles(10, 10);
    if (SearchForAddedSPLs(buf, s, foundFiles))
    {
        ret = TRUE;
        // First, we will uninstall plugins that no longer have a .spl file (are no longer supported)
        RemoveNoLongerExistingPlugins(!importFromOldConfig); // We must not delete the plugin key in the registry if it is an import from the configuration of the previous version of Salamander

        CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
        char textProgress[1000];
        _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), LoadStr(IDS_AUTOINSTALLPLUGINS_INIT));
        analysing.SetText(textProgress);
        analysing.Create();

        // for progress we will count the number of existing plugins that we will later load
        int toLoadCount = GetNumOfPluginsToLoad();

        // set overall progress
        analysing.SetProgressMax((foundFiles.IsGood() ? foundFiles.Count : 0) + toLoadCount);
        int progress = 0;

        if (foundFiles.IsGood())
        {
            *s = 0; // fix the path in the buffer
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

                    Plugins.AddPlugin(parent, pluginName); // whatever we add will already be loaded (loading is used to test if it's even a plugin)
                }
                analysing.SetProgressPos(++progress);
            }
        }
        else
            foundFiles.ResetState();

        // we will load all plug-ins to refresh the data in Salamander...
        for (int i = 0; i < Data.Count; i++)
        {
            if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
                && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), Data[i]->DLLName);
                analysing.SetText(textProgress);

                Data[i]->InitDLL(parent, TRUE, FALSE); // suppress the desperate X-fold cursor blinking

                UpdateWindow(MainWindow->HWindow); // we will redraw the windows after the message boxes
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

    // we will load all plugins and call their method ClearHistory;
    // we will leave them loaded so that they are always executed (even when "save on exit" is turned off)
    // Save configuration -> cleaning in Registry; when loading, we will report errors so that
    // user was aware of potential issues with cleaning history in the plugin
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

    // for progress we will count the number of plugins that we will load
    int toLoadCount = GetNumOfPluginsToLoad();
    analysing.SetProgressMax(toLoadCount);

    int progress = 0;
    for (int i = 0; i < Data.Count; i++)
    {
        BOOL wasLoaded = Data[i]->GetLoaded();
#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
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
                UpdateWindow(MainWindow->HWindow); // we will redraw the windows after the message boxes
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

    // for progress we will count the number of plugins that we will load
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
#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
                && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_LOADINGPLUGINS), Data[i]->DLLName);
                analysing.SetText(textProgress);

                Data[i]->InitDLL(parent, TRUE);

                UpdateWindow(MainWindow->HWindow); // we will redraw the windows after the message boxes
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

    // we will load all plugins with the load-on-start flag...
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

    // we will rebuild the menu and unload all plugins that requested it...
    BeginStopRefresh();
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->ShouldRebuildMenu)
        {
            p->ShouldRebuildMenu = FALSE;
            if (p->GetLoaded()) // if the plugin was manually unloaded by the user in the meantime, we discard the rebuild request
            {
                if (p->SupportDynMenuExt)
                {
                    p->BuildMenu(parent, TRUE);
                    p->ReleasePluginDynMenuIcons(); // no one needs this object (for the next menu display, everything is retrieved again)
                }
                else
                    TRACE_I("CSalamanderGeneral::GetCmdAndUnloadMarkedPlugins(): call ignored because plugin has not dynamic menu (see FUNCTION_DYNAMICMENUEXT)");
            }
        }
        if (p->ShouldUnload && (p->Commands.Count == 0 || p->Commands[0] == -1)) // no commands are pending or an error has occurred
        {
            p->ShouldUnload = FALSE;
            if (p->GetLoaded())
                p->Unload(parent, FALSE); // save-config without asking + unload
        }
    }
    EndStopRefresh();

    // find the first Salamander/menu command that plugins requested
    *cmd = -1;    // -1 == "not found"
    *data = NULL; // NULL == "not found"
    for (i = 0; i < Data.Count; i++)
    {
        CPluginData* p = Data[i];
        if (p->Commands.Count > 0)
        {
            if (!p->Commands.IsGood())
                p->Commands.ResetState(); // to make sure it is possible to select (Add() is not checked)
            *cmd = p->Commands[0];        // first command that is in order
            p->Commands.Delete(0);        // we remove this command from the queue
            if (!p->Commands.IsGood())
            {
                p->Commands.ResetState();
                p->Commands[0] = -1; // Prevention against infinite loop in case of command throwing an error
            }
            *data = p;
            break; // let the command be executed
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
    *data = NULL; // not found
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
        return; // will not occur
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

    // Uninstalling plugins that no longer have a .spl file (are no longer supported)
    int numOfNotLoaded = 0;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (!Data[i]->GetLoaded() // applies only to non-loaded plugins
#ifdef _WIN64                     // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
            && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
        )
        {
            char* fullName = Data[i]->DLLName;
            if ((*fullName != '\\' || *(fullName + 1) != '\\') && // not UNC
                (*fullName == 0 || *(fullName + 1) != ':'))       // not "c:" -> relative path to the subdirectory plugins
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
                    _stricmp(Data[i]->DLLName, "fsearch\\fsearch.spl") != 0 && // fsearch we want to silence (not unnecessarily alert that it is gone)
                    _stricmp(Data[i]->DLLName, "eroiica\\eroiica.spl") != 0 && // We want to silence the Eroiica (unnecessarily not to draw attention that it's gone)
                    _stricmp(Data[i]->DLLName, "unace\\unace.spl") != 0 &&     // UnACE we want to silence (not unnecessarily alert that it is gone)
                    _stricmp(Data[i]->DLLName, "diskcopy\\diskcopy.spl") != 0) // We want to silence DiskCopy (unnecessary to notify that it's gone)
                {
                    lstrcpyn(pluginName, Data[i]->Name, MAX_PATH); // if the key is in the registry, I will save its name
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

    if (loadAllPlugins) // we will load all plugins, we are interested in those that fail to load (their configuration will not be transferred from the old version to the new one)
    {
        LoadInfoBase |= LOADINFO_NEWSALAMANDERVER;

        // for progress we will count the number of plugins that we will load
        int toLoadCount = GetNumOfPluginsToLoad();
        analysing.SetProgressMax(toLoadCount);

        int progress = 0;
        for (i = 0; i < Data.Count; i++)
        {
            if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
                && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), Data[i]->DLLName);
                analysing.SetText(textProgress);

                if (!Data[i]->InitDLL(parent, TRUE, FALSE)) // suppress the desperate X-fold cursor blinking
                {
                    if (notLoadedPluginNames != NULL && Data[i]->RegKeyName != NULL && Data[i]->RegKeyName[0] != 0)
                    { // if the key is in the registry, I will save its name
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

                UpdateWindow(MainWindow->HWindow); // we will redraw the windows after the message boxes
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

    // obtaining the directory "plugins"
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
        return; // will not occur
    }

    CWaitWindow analysing(parent, 0, FALSE, ooStatic, TRUE);
    char textProgress[1000];
    _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), LoadStr(IDS_AUTOINSTALLPLUGINS_INIT));
    analysing.SetText(textProgress);
    analysing.Create();

    // First, we will uninstall plugins that no longer have a .spl file (are no longer supported)
    RemoveNoLongerExistingPlugins(FALSE); // We must not delete the plugin key in the registry, it is an import from the configuration of the previous version of Salamander

    LoadInfoBase |= LOADINFO_NEWSALAMANDERVER;

    // we will search for *.spl in the "plugins" directory and its subdirectories
    TIndirectArray<char> foundFiles(10, 10);
    WIN32_FIND_DATA data;
    SearchForSPLs(buf, s, foundFiles, data);

    // for progress we will count the number of existing plugins that we will later load
    int toLoadCount = GetNumOfPluginsToLoad();

    // set overall progress
    analysing.SetProgressMax((foundFiles.IsGood() ? foundFiles.Count : 0) + toLoadCount);
    int progress = 0;

    if (foundFiles.IsGood())
    {
        *s = 0; // fix the path in the buffer
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
#ifdef _WIN64                                            // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
                && !IsPluginUnsupportedOnX64(pluginName) // Only threatens in the internal debug version (incomplete plugin is compiled, even though it is not deployed to users)
#endif                                                   // _WIN64
            )
            {
                _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), pluginName);
                analysing.SetText(textProgress);

                if (Plugins.AddPlugin(parent, pluginName)) // whatever we add will already be loaded (loading is used to test if it's even a plugin)
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

    // we will load all plug-ins to refresh the data in Salamander...
    for (int i = 0; i < Data.Count; i++)
    {
        if (!Data[i]->GetLoaded()
#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
            && !IsPluginUnsupportedOnX64(Data[i]->DLLName)
#endif // _WIN64
        )
        {
            _snprintf_s(textProgress, _TRUNCATE, "%s\n%s", LoadStr(IDS_AUTOINSTALLPLUGINS), Data[i]->DLLName);
            analysing.SetText(textProgress);

            Data[i]->InitDLL(parent, TRUE, FALSE); // suppress the desperate X-fold cursor blinking

            UpdateWindow(MainWindow->HWindow); // we will redraw the windows after the message boxes
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
        // get the full name of salamand.exe
        GetModuleFileName(HInstance, module, MAX_PATH);
        // get version
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
            // get the full name of the DLL
            char* s = data->DLLName;
            if ((*s != '\\' || *(s + 1) != '\\') && // not UNC
                (*s == 0 || *(s + 1) != ':'))       // not "c:" -> relative path to packers
            {
                GetModuleFileName(HInstance, module, MAX_PATH);
                s = strrchr(module, '\\') + 1;
                strcpy(s, "plugins\\");
                strcat(s, data->DLLName);
            }
            else
                strcpy(module, s);
            // get version
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
        if (p != ignorePlugin && p->MenuItems.Count > 0) // has some items
        {
            int j;
            for (j = 0; j < p->MenuItems.Count; j++)
            {
                // Reduce the SkillLevel ctime?
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
        if (p != ignorePlugin && p->MenuItems.Count > 0) // has some items
        {
            int j;
            for (j = 0; j < p->MenuItems.Count; j++)
            {
                if (HOTKEY_GET(p->MenuItems[j]->HotKey) == hotKey)
                {
                    p->MenuItems[j]->HotKey = (p->MenuItems[j]->HotKey & ~HOTKEY_MASK) | HOTKEY_DIRTY; // we do not want this change to be rolled back by the Connect() plugin
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
    BYTE vk = UpperCase[(BYTE)wParam]; // We should be case insensitive, the user may accidentally press the caps lock and the hotkey editline always pretends that the key is uppercase
    if (vk == 0)
        return FALSE; // I disabled hot keys in the plugin for vk==0
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
        // we found the command with our hotkey

        // we do not want to let WM_SYSCHAR fall through to the menu that opens for example Automation on Alt+Shift+A
        // because it would trigger the execution of command from 'A' -- I will empty the message queue
        // Note: originally I thought it would be better to call HandleKeyDown() from WM_CHAR and WM_SYSCHAR,
        // but for example, the combination of Ctrl+Shift+Alt+letter does not trigger WM_CHAR or WM_SYSCHAR,
        // takze je potreba pouzivat WM_KEYDOWN a WM_SYSKEYDOWN, ktere ovsem nasledne posilaji WM_CHAR a WM_SYSCHAR
        MSG msg;
        PeekMessage(&msg, hParent, WM_SYSCHAR, WM_SYSCHAR, PM_REMOVE);
        PeekMessage(&msg, hParent, WM_CHAR, WM_CHAR, PM_REMOVE);

        // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        if (ExecuteCommand(pluginIndex, menuItemIndex, activePanel, hParent))
        {
            activePanel->SetSel(FALSE, -1, TRUE);                        // explicit override
            PostMessage(activePanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
        }

        // increase the thread priority again, operation completed
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        // Restoring the content of non-automatic panels is in the plugins

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
    LastPlgCmdPlugin = DupStr(dllName); // if allocation fails, LastPlgCmdPlugin will be NULL -- default item will be in the menu
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
#ifdef _WIN64 // FIXME_X64_WINSCP - this will probably need to be solved differently... (ignoring missing WinSCP in x64 version of Salama)
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

// WARNING: _sal_safe_memcpy must be at the end of the module!!!
