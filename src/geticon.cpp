// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "commoncontrols.h"

UINT WINAPI ExtractIcons(LPCTSTR szFileName, int nIconIndex, int cxIcon, int cyIcon, HICON* phicon, UINT* piconid, UINT nIcons, UINT flags)
{
    UINT nIconSize = cxIcon;
    HICON hLarge{};
    HICON hSmall{};
    auto shRet = SHDefExtractIcon(szFileName, nIconIndex, 0, &hLarge, &hSmall, nIconSize);
    if (shRet == S_OK)
    {
        if (phicon)
        {
            phicon[0] = hLarge;
            if (nIcons == 2)
            {
                phicon[1] = hSmall;
            }
            else
            {
                if (hSmall != 0)
                {
                    DestroyIcon(hSmall);
                }
            }
        }
    }
    return shRet == S_OK ? 1 : 0;
}

STDAPI SHBindToIDListParent(LPCITEMIDLIST pidl, REFIID riid, void** ppv, LPCITEMIDLIST* ppidlLast)
{
    return SHBindToFolderIDListParent(NULL, pidl, riid, ppv, ppidlLast);
}

BOOL OnExtList(LPCTSTR pszExtList, LPCTSTR pszExt)
{
    for (; *pszExtList; pszExtList += lstrlen(pszExtList) + 1)
    {
        if (!lstrcmpi(pszExt, pszExtList))
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ExtIsExe(LPCTSTR szExt)
{
    return OnExtList("cmd\0bat\0pif\0scf\0exe\0com\0scr\0", szExt);
}

#define SHIL_LARGE 0      // The image size is normally 32x32 pixels. However, if the Use large icons option is selected from the Effects section of the Appearance tab in Display Properties, the image is 48x48 pixels.
#define SHIL_SMALL 1      // These images are the Shell standard small icon size of 16x16, but the size can be customized by the user.
#define SHIL_EXTRALARGE 2 // These images are the Shell standard extra-large icon size. This is typically 48x48, but the size can be customized by the user.
#define SHIL_SYSSMALL 3   // These images are the size specified by GetSystemMetrics called with SM_CXSMICON and GetSystemMetrics called with SM_CYSMICON.
#define SHIL_JUMBO 4      // Windows Vista and later. The image is normally 256x256 pixels.
// ohledne rozmeru ikon pod Windows Vista: Creating a DPI-Aware Application ( http://msdn.microsoft.com/en-us/library/ms701681(VS.85).aspx )

BOOL SalGetIconFromPIDL(IShellFolder* psf, const char* path, LPCITEMIDLIST pidl, HICON* hIcon,
                        CIconSizeEnum iconSize, BOOL fallbackToDefIcon, BOOL defIconIsDir)
{
    BOOL ret = FALSE;

    IExtractIconA* pxi = NULL; // je-li 'isIExtractIconW' TRUE, je ukazatel typu IExtractIconW
    BOOL isIExtractIconW = FALSE;
    HICON hIconSmall = NULL;
    HICON hIconLarge = NULL;

    char iconFile[MAX_PATH];
    WCHAR iconFileW[MAX_PATH];
    int iconIndex;
    UINT wFlags = 0; // nulujeme kvuli shell extension DWGIcon.dll (viz forum), ktera bity pouze oruje

    CIconSizeEnum largeIconSize = ICONSIZE_32;
    if (iconSize == ICONSIZE_48)
        largeIconSize = ICONSIZE_48;

    HRESULT hres = psf->GetUIObjectOf(NULL, 1, &pidl, IID_IExtractIconA, NULL, (void**)&pxi);
    if (SUCCEEDED(hres))
    {
        hres = pxi->GetIconLocation(GIL_FORSHELL, iconFile, MAX_PATH, &iconIndex, &wFlags);
        //TRACE_I("  SalGetIconFromPIDL() IID_IExtractIconA iconFile="<<iconFile<<" iconIndex="<<iconIndex<<" wFlags="<<wFlags);
    }
    else
    {
        // ANSI verze selhala, zkusime jeste UNICODE verzi IID_IExtractIcon
        hres = psf->GetUIObjectOf(NULL, 1, &pidl, IID_IExtractIconW, NULL, (void**)&pxi);
        if (SUCCEEDED(hres))
        {
            isIExtractIconW = TRUE;
            hres = ((IExtractIconW*)pxi)->GetIconLocation(GIL_FORSHELL, iconFileW, MAX_PATH, &iconIndex, &wFlags);
            if (SUCCEEDED(hres))
            {
                // Convert the UNICODE string to ANSI
                WideCharToMultiByte(CP_ACP, 0, iconFileW, -1, iconFile, MAX_PATH, NULL, NULL);
                iconFile[MAX_PATH - 1] = 0;
                //TRACE_I("  SalGetIconFromPIDL() IID_IExtractIconW iconFile="<<iconFile<<" iconIndex="<<iconIndex<<" wFlags="<<wFlags);
            }
        }
    }
    //  TRACE_I("iconFile="<<iconFile<<" iconIndex="<<iconIndex);
    if (SUCCEEDED(hres))
    {
        // pod XP lze vytahnout system image list 48x48 (Extract() nam blbe vraci 32x32)
        // druhou moznosti jak ziskat ikonky 48x48 je LoadImage, ale to bychom museli znat cestu k souboru a cislo ikony...
        // "*" v nazvu souboru znamena, ze iconIndex uz je system icon index
        //TRACE_I("  SalGetIconFromPIDL() wFlags="<<wFlags<<" iconFile='"<<iconFile<<"' TryObtainGetImageList="<<TryObtainGetImageList);
        if ((wFlags & GIL_NOTFILENAME) && iconFile[0] == '*' && iconFile[1] == 0)
        {
            // vicenasobne pokusy sice u JISe zabraly, ale je tu riziko, ze pokud bude ziskavani ikony selhavat
            // z nejakeho opodstatneho duvodu, budeme se tu mrcasit s 50ms delayem zbytecne pri kazdem ziskani ikony
            // Petr zavedl Retry nahoru do IconReadera, takze radeji prizname porazku a vratime FALSE, aby se
            // po 500ms ikony zkusily nacist jeste jednou
            //      int attempt = 1;
            //AGAIN:
            // ***** hIconSmall ******
            //TRACE_I("  SalGetIconFromPIDL() Asking system image list '*' for iconIndex="<<iconIndex);
            IImageList* imageListSmall = NULL;
            hres = SHGetImageList(SHIL_SMALL, IID_IImageList, (void**)&imageListSmall);
            if (SUCCEEDED(hres) && (imageListSmall != NULL))
            {
                if (imageListSmall->GetIcon(iconIndex, ILD_NORMAL, &hIconSmall) != S_OK)
                    hIconSmall = NULL;
                //TRACE_I("  SalGetIconFromPIDL() SHIL_SMALL IID_IImageList, hIconSmall="<<hIconSmall);
                imageListSmall->Release();
            }
            if (hIconSmall == NULL)
            {
                // U duchodce Billa nam GetImageList selhalo a nasledne i pxi->Extract() vratilo NULL handly ikon
                //TRACE_I("  SalGetIconFromPIDL() SHIL_SMALL IID_IImageList was not obtained!");
                if (path != NULL)
                {
                    // zkusime pozadat system, aby nam vratil index do system image listu a jeho handle a ikonu z nej vytahneme
                    SHFILEINFO sfi;
                    ZeroMemory(&sfi, sizeof(sfi));
                    HIMAGELIST hSysImageList = (HIMAGELIST)SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON); // vraci stale stejny handle, neni treba uvolnovat
                    if (hSysImageList != NULL)
                    {
                        hIconSmall = ImageList_GetIcon(hSysImageList, sfi.iIcon, ILD_NORMAL);
                        //TRACE_I("  SalGetIconFromPIDL() ImageList_GetIcon for SHGFI_SMALLICON hIconSmall="<<hIconSmall<<" sfi.iIcon="<<sfi.iIcon<<" hSysImageList="<<hSysImageList);
                    }
                    if (hIconSmall == NULL)
                    {
                        // zkusime pozadat primo o ikonu
                        ZeroMemory(&sfi, sizeof(sfi));
                        if (SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) != 0)
                            hIconSmall = sfi.hIcon;
                        //TRACE_I("  SalGetIconFromPIDL() SHGetFileInfo for SHGFI_ICON | SHGFI_SMALLICON hIconSmall="<<hIconSmall);
                    }
                }
                //else TRACE_I("  SalGetIconFromPIDL() path == NULL");
            }
            // ***** hIconLarge ******
            if (iconSize == ICONSIZE_48)
            {
                IImageList* imageListExtraLarge = NULL;
                if (SUCCEEDED(SHGetImageList(SHIL_EXTRALARGE, IID_IImageList, (void**)&imageListExtraLarge)) && (imageListExtraLarge != NULL))
                {
                    if (imageListExtraLarge->GetIcon(iconIndex, ILD_NORMAL, &hIconLarge) != S_OK)
                        hIconLarge = NULL;
                    //TRACE_I("  SalGetIconFromPIDL() SHIL_EXTRALARGE IID_IImageList, hIconLarge="<<hIconLarge);
                    imageListExtraLarge->Release();
                }
            }
            else // ICONSIZE_16 || ICONSIZE_32
            {
                IImageList* imageListLarge = NULL;
                hres = SHGetImageList(SHIL_LARGE, IID_IImageList, (void**)&imageListLarge);
                if (SUCCEEDED(hres) && (imageListLarge != NULL))
                {
                    if (imageListLarge->GetIcon(iconIndex, ILD_NORMAL, &hIconLarge) != S_OK)
                        hIconLarge = NULL;
                    //TRACE_I("  SalGetIconFromPIDL() SHIL_LARGE IID_IImageList, hIconLarge="<<hIconLarge);
                    imageListLarge->Release();
                }
                if (hIconLarge == NULL)
                {
                    // U duchodce Billa nam GetImageList selhalo a nasledne i pxi->Extract() vratilo NULL handly ikon
                    //TRACE_I("  SalGetIconFromPIDL() SHIL_LARGE IID_IImageList was not obtained!");
                    if (path != NULL)
                    {
                        // zkusime pozadat system, aby nam vratil index do system image listu a jeho handle a ikonu z nej vytahneme
                        SHFILEINFO sfi;
                        ZeroMemory(&sfi, sizeof(sfi));
                        HIMAGELIST hSysImageList = (HIMAGELIST)SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_ICON);
                        if (hSysImageList != NULL)
                        {
                            hIconLarge = ImageList_GetIcon(hSysImageList, sfi.iIcon, ILD_NORMAL);
                            //TRACE_I("  SalGetIconFromPIDL() ImageList_GetIcon for SHGFI_ICON hIconLarge="<<hIconLarge<<" sfi.iIcon="<<sfi.iIcon<<" hSysImageList="<<hSysImageList);
                        }
                        if (hIconLarge == NULL)
                        {
                            // zkusime pozadat primo o ikonu
                            ZeroMemory(&sfi, sizeof(sfi));
                            if (SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) != 0)
                                hIconLarge = sfi.hIcon;
                            //TRACE_I("  SalGetIconFromPIDL() SHGetFileInfo for SHGFI_ICON | SHGFI_LARGEICON hIconLarge="<<hIconLarge);
                        }
                    }
                    //else TRACE_I("  SalGetIconFromPIDL() path == NULL");
                }
            }
            //      // pokud se nepodarilo vytahnout ani jednu ikonu
            //      if (hIconSmall == NULL && hIconLarge == NULL && attempt <= 3)
            //      {
            //        // zkusime to znovu, az 3x za sebou s cekanim 50ms
            //        //TRACE_I("  SalGetIconFromPIDL() Sleeping and trying again. Attempt="<<attempt);
            //        Sleep(50);
            //        attempt++;
            //        goto AGAIN;
            //      }
        }

        // pokud jsme ikonu nevytahli ze system image listu, pozadame o ni interface pxi
        if (hIconSmall == NULL && hIconLarge == NULL)
        {
            // zkusime IExtractIcon::Extract()
            // pozor, pokud je iconFile == '*', nekdy Exract vrati validni ikony, ale mame pripady, kdy je nevrati
            // (zalezi na implementaci shellextension) a uzivatelum se potom zobrazovaly default ikony, viz dole
            if (isIExtractIconW)
                hres = ((IExtractIconW*)pxi)->Extract(iconFileW, iconIndex, &hIconLarge, &hIconSmall, MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]));
            else
                hres = pxi->Extract(iconFile, iconIndex, &hIconLarge, &hIconSmall, MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]));
            //TRACE_I("  SalGetIconFromPIDL() pxi->Extract() hIconLarge="<<hIconLarge<<" hIconSmall="<<hIconSmall<<" isIExtractIconW="<<isIExtractIconW);
            // POZOR pro *.ai prichazi iconFile==0 a iconIndex==0 a Extract() presto vrati ikonu (asi zalezitost Adobe Illustrator shell extension)
            // POZOR D:\Store\Salamand\ICO_SONY\SonyF707_Day_Flash.icc vraci hIconLarge==hIconSmall, obe 32x32
        }

        // pokud lezi ikona v souboru, muzeme se ji pokusit ziskat sami pomoci ExtractIcons()
        if (hIconSmall == NULL && hIconLarge == NULL && !(wFlags & GIL_NOTFILENAME))
        {
            HICON hIcons[2] = {0, 0};
            UINT u = ExtractIcons(iconFile, iconIndex, MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]), MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]), hIcons, NULL, 2, IconLRFlags);
            if (u != -1)
            {
                hIconLarge = hIcons[0];
                hIconSmall = hIcons[1];
            }
            //TRACE_I("  SalGetIconFromPIDL() ExtractIcons hIconLarge="<<hIconLarge<<" hIconSmall="<<hIconSmall);
        }
    }
    if (pxi != NULL)
    {
        if (isIExtractIconW)
            ((IExtractIconW*)pxi)->Release();
        else
            pxi->Release();
    }

    // nezabrala ani jedna metoda; vratime default ikonu
    if (fallbackToDefIcon && hIconSmall == NULL && hIconLarge == NULL)
    {
        BOOL fileIsExecutable = FALSE;
        if (!defIconIsDir && path != NULL)
        {
            const char* name = strrchr(path, '\\');
            const char* ext = name != NULL ? strrchr(name + 1, '.') : NULL;
            //      if (ext > path && *(ext - 1) != '\\')    // ".cvspass" ve Windows je pripona ...
            if (ext != NULL)
                fileIsExecutable = ExtIsExe(ext + 1);
        }

        int resID;
        if (WindowsVistaAndLater)
            resID = defIconIsDir ? 4 : (fileIsExecutable ? 15 : 2); // symbolsDirectory : symbolsExecutable : symbolsNonAssociated
        else
            resID = defIconIsDir ? 4 : (fileIsExecutable ? 3 : 1); // symbolsDirectory : symbolsExecutable : symbolsNonAssociated
        HICON hIcons[2] = {0, 0};
        UINT u = ExtractIcons(WindowsVistaAndLater ? "imageres.dll" : "shell32.dll", -resID,
                              MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]), MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]),
                              hIcons, NULL, 2, IconLRFlags);
        if (u != -1)
        {
            hIconLarge = hIcons[0];
            hIconSmall = hIcons[1];
        }
        //TRACE_I("  SalGetIconFromPIDL() DEFAULT ICON ExtractIcons hIconLarge="<<hIconLarge<<" hIconSmall="<<hIconSmall);
    }

    if (hIconLarge != NULL || hIconSmall != NULL)
    {
        ret = TRUE;
        // malou ikonu vezmeme z hIconSmall, protoze IExtractIcon::Extract() ignoruje rozmery v pixlech a vraci vzdy 16 a 32
        if (iconSize == ICONSIZE_16)
        {
            // pokud neexistuje mala verze ikony nebo nam podstrcili handle velke ikonky, vytvorime ji
            if (hIconSmall == NULL || hIconSmall == hIconLarge)
            {
                hIconSmall = (HICON)CopyImage(hIconLarge, IMAGE_ICON, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], LR_COPYFROMRESOURCE);
                //TRACE_I("  SalGetIconFromPIDL() CopyImage 1 hIconSmall="<<hIconSmall<<" hIconLarge="<<hIconLarge);
            }
            *hIcon = hIconSmall;
            if (hIconLarge != NULL)
            {
                DestroyIcon(hIconLarge);
                hIconLarge = NULL;
            }
        }
        else // ICONSIZE_32 || ICONSIZE_48
        {
            // pokud neexistuje velka verze ikony nebo nam podstrcili handle male ikonky, vytvorime ji
            if (hIconLarge == NULL || hIconSmall == hIconLarge)
            {
                hIconLarge = (HICON)CopyImage(hIconSmall, IMAGE_ICON, IconSizes[largeIconSize], IconSizes[largeIconSize], LR_COPYFROMRESOURCE);
                //TRACE_I("  SalGetIconFromPIDL() CopyImage 2 hIconSmall="<<hIconSmall<<" hIconLarge="<<hIconLarge);
            }
            *hIcon = hIconLarge;
            if (hIconSmall != NULL)
            {
                DestroyIcon(hIconSmall);
                hIconSmall = NULL;
            }
        }
    }

    return ret;
}

LPITEMIDLIST SHILCreateFromPath(LPCSTR pszPath)
{
    LPITEMIDLIST pidl = NULL;
    IShellFolder* psfDesktop;
    if (SUCCEEDED(SHGetDesktopFolder(&psfDesktop)))
    {
        ULONG cchEaten;
        WCHAR wszPath[MAX_PATH];

        MultiByteToWideChar(CP_ACP, 0, pszPath, -1, wszPath, MAX_PATH);
        wszPath[MAX_PATH - 1] = 0;

        psfDesktop->ParseDisplayName(NULL, NULL, wszPath, &cchEaten, &pidl, NULL);

        psfDesktop->Release();
    }
    return pidl;
}

// komentar viz spl_gen.h/GetFileIcon
BOOL GetFileIcon(const char* path, BOOL pathIsPIDL, HICON* hIcon, CIconSizeEnum iconSize,
                 BOOL fallbackToDefIcon, BOOL defIconIsDir)
{
    BOOL ret = FALSE;
    LPITEMIDLIST pidlFull;

    if (hIcon == NULL)
    {
        TRACE_E("hIcon == NULL");
        return FALSE;
    }
    /*
  if (!pathIsPIDL)
    TRACE_I("GetFileIcon() path="<<path<<" iconSize="<<iconSize);
  else
    TRACE_I("GetFileIcon() pathIsPIDL"); // toto v Salamanderu nepouzivame, pouze pro plugin Folders
*/
    if (!pathIsPIDL)
        pidlFull = SHILCreateFromPath(path);
    else
        pidlFull = (LPITEMIDLIST)path;

    if (pidlFull != NULL)
    {
        IShellFolder* psf;
        LPITEMIDLIST pidlLast;
        HRESULT hres = SHBindToIDListParent(pidlFull, IID_IShellFolder, (void**)&psf, (LPCITEMIDLIST*)&pidlLast);
        if (SUCCEEDED(hres))
        {
            // pokud zname cestu, posleme ji do SalGetIconFromPIDL
            ret = SalGetIconFromPIDL(psf, pathIsPIDL ? NULL : path, pidlLast, hIcon, iconSize,
                                     fallbackToDefIcon, defIconIsDir);

            psf->Release();
        }

        if (!pathIsPIDL)
            ILFree(pidlFull);
    }

    return ret;
}
