// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
// regarding icon sizes on Windows Vista: see "Creating a DPI-Aware Application" (http://msdn.microsoft.com/en-us/library/ms701681(VS.85).aspx)

BOOL SalGetIconFromPIDL(IShellFolder* psf, const char* path, LPCITEMIDLIST pidl, HICON* hIcon,
                        CIconSizeEnum iconSize, BOOL fallbackToDefIcon, BOOL defIconIsDir)
{
    BOOL ret = FALSE;

    IExtractIconA* pxi = NULL; // if 'isIExtractIconW' is TRUE, this pointer is actually IExtractIconW
    BOOL isIExtractIconW = FALSE;
    HICON hIconSmall = NULL;
    HICON hIconLarge = NULL;

    char iconFile[MAX_PATH];
    WCHAR iconFileW[MAX_PATH];
    int iconIndex;
    UINT wFlags = 0; // clear because the DWGIcon.dll shell extension just ORs these bits

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
        // The ANSI version failed, so we try the UNICODE IID_IExtractIcon variant
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
        // on XP we can obtain the 48x48 system image list (Extract() incorrectly returns 32x32)
        // another way to get 48x48 icons is LoadImage, but we would need the file path and icon number
        // a "*" in the file name means iconIndex already refers to a system icon index
        //TRACE_I("  SalGetIconFromPIDL() wFlags="<<wFlags<<" iconFile='"<<iconFile<<"' TryObtainGetImageList="<<TryObtainGetImageList);
        if ((wFlags & GIL_NOTFILENAME) && iconFile[0] == '*' && iconFile[1] == 0)
        {
            // multiple attempts helped JIS, but if icon extraction keeps failing
            // we would waste 50 ms on each icon retrieval for no reason
            // Petr moved the retry logic to IconReader, so we fail here and return FALSE
            // icons will be retried after 500 ms
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
                // With old man Bill, GetImageList fails and pxi->Extract() then returns NULL icon handles
                //TRACE_I("  SalGetIconFromPIDL() SHIL_SMALL IID_IImageList was not obtained!");
                if (path != NULL)
                {
                    // Try asking the system for the system image list index and use that handle to extract the icon
                    SHFILEINFO sfi;
                    ZeroMemory(&sfi, sizeof(sfi));
                    HIMAGELIST hSysImageList = (HIMAGELIST)SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON); // returns a persistent handle, no need to release
                    if (hSysImageList != NULL)
                    {
                        hIconSmall = ImageList_GetIcon(hSysImageList, sfi.iIcon, ILD_NORMAL);
                        //TRACE_I("  SalGetIconFromPIDL() ImageList_GetIcon for SHGFI_SMALLICON hIconSmall="<<hIconSmall<<" sfi.iIcon="<<sfi.iIcon<<" hSysImageList="<<hSysImageList);
                    }
                    if (hIconSmall == NULL)
                    {
                        // Try asking directly for the icon
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
                    // With old man Bill, GetImageList fails and pxi->Extract() then returns NULL icon handles
                    //TRACE_I("  SalGetIconFromPIDL() SHIL_LARGE IID_IImageList was not obtained!");
                    if (path != NULL)
                    {
                        // Try asking the system for the system image list index and use that handle to extract the icon
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
                            // Try asking directly for the icon
                            ZeroMemory(&sfi, sizeof(sfi));
                            if (SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) != 0)
                                hIconLarge = sfi.hIcon;
                            //TRACE_I("  SalGetIconFromPIDL() SHGetFileInfo for SHGFI_ICON | SHGFI_LARGEICON hIconLarge="<<hIconLarge);
                        }
                    }
                    //else TRACE_I("  SalGetIconFromPIDL() path == NULL");
                }
            }
            //      // if we failed to extract any icon
            //      if (hIconSmall == NULL && hIconLarge == NULL && attempt <= 3)
            //      {
            //        // try again up to three times with a 50 ms delay
            //        //TRACE_I("  SalGetIconFromPIDL() Sleeping and trying again. Attempt="<<attempt);
            //        Sleep(50);
            //        attempt++;
            //        goto AGAIN;
            //      }
        }

        // if the icon was not taken from the system image list, ask the pxi interface for it
        if (hIconSmall == NULL && hIconLarge == NULL)
        {
            // try IExtractIcon::Extract()
            // Note: if iconFile == '*', Extract sometimes returns valid icons but in some implementations it doesn't,
            // leaving users with default icons; see below
            if (isIExtractIconW)
                hres = ((IExtractIconW*)pxi)->Extract(iconFileW, iconIndex, &hIconLarge, &hIconSmall, MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]));
            else
                hres = pxi->Extract(iconFile, iconIndex, &hIconLarge, &hIconSmall, MAKELONG(IconSizes[largeIconSize], IconSizes[ICONSIZE_16]));
            //TRACE_I("  SalGetIconFromPIDL() pxi->Extract() hIconLarge="<<hIconLarge<<" hIconSmall="<<hIconSmall<<" isIExtractIconW="<<isIExtractIconW);
            // WARNING: for *.ai files iconFile==0 and iconIndex==0 yet Extract() still returns an icon (Adobe Illustrator shell extension)
            // WARNING: D:\Store\Salamand\ICO_SONY\SonyF707_Day_Flash.icc returns hIconLarge==hIconSmall, both 32x32
        }

        // if the icon is stored in a file, we can attempt to retrieve it ourselves via ExtractIcons()
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

    // none of the methods worked, so return the default icon
    if (fallbackToDefIcon && hIconSmall == NULL && hIconLarge == NULL)
    {
        BOOL fileIsExecutable = FALSE;
        if (!defIconIsDir && path != NULL)
        {
            const char* name = strrchr(path, '\\');
            const char* ext = name != NULL ? strrchr(name + 1, '.') : NULL;
            //      if (ext > path && *(ext - 1) != '\\')    // ".cvspass" is an extension in Windows ...
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
        // use hIconSmall for the small icon because IExtractIcon::Extract() ignores the pixel size and always returns 16 and 32
        if (iconSize == ICONSIZE_16)
        {
            // if the small icon is missing or we were given the handle of the large one, create it
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
            // if the large icon is missing or we were given the handle of the small one, create it
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

// comment see spl_gen.h/GetFileIcon
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
    TRACE_I("GetFileIcon() pathIsPIDL"); // not used by Salamander itself, only by the Folders plugin
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
            // if we know the path, pass it to SalGetIconFromPIDL
            ret = SalGetIconFromPIDL(psf, pathIsPIDL ? NULL : path, pidlLast, hIcon, iconSize,
                                     fallbackToDefIcon, defIconIsDir);

            psf->Release();
        }

        if (!pathIsPIDL)
            ILFree(pidlFull);
    }

    return ret;
}
