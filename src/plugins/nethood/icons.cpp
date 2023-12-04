// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#include "precomp.h"
#include "icons.h"
#include "nethood.h"
#include "nethoodfs.h"
#include "globals.h"

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconSimpleFileInfo[] =
    {
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            1,
            SIID_DOCNOASSOC,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            0,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconSimpleDirectoryInfo[] =
    {
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            3,
            SIID_FOLDER,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            3,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconDomainOrGroupInfo[] =
    {
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            18,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconServerInfo[] =
    {
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            103,
            SIID_SERVER,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            15,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconShareInfo[] =
    {
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            136,
            SIID_SERVERSHARE,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            85,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconNetworkInfo[] =
    {
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            14,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconEntireNetworkInfo[] =
    {
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            145,
            SIID_WORLD,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            13,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconMainInfo[] =
    {
        {
            OSSPECIFIC_7,
            TEXT("shell32.dll"),
            17,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            145,
            SIID_WORLD,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            17,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO CNethoodIcons::s_asIconNetDriveInfo[] =
    {
        {
            OSSPECIFIC_VISTA,
            TEXT("imageres.dll"),
            27,
            SIID_DRIVENET,
        },
        {
            OSSPECIFIC_ANY,
            TEXT("shell32.dll"),
            9,
            SIID_INVALID,
        },
        {
            OSSPECIFIC_LAST,
            NULL,
            0,
            SIID_INVALID,
        }};

const CNethoodIcons::OSLOADICONINFO* CNethoodIcons::s_apLoadIconInfo[_IconLast] =
    {
        CNethoodIcons::s_asIconSimpleFileInfo,      /* IconSimpleFile	*/
        CNethoodIcons::s_asIconSimpleDirectoryInfo, /* IconSimpleDirectory	*/
        CNethoodIcons::s_asIconDomainOrGroupInfo,   /* IconDomainOrGroup	*/
        CNethoodIcons::s_asIconServerInfo,          /* IconServer		*/
        CNethoodIcons::s_asIconShareInfo,           /* IconShare		*/
        CNethoodIcons::s_asIconNetworkInfo,         /* IconNetwork		*/
        CNethoodIcons::s_asIconEntireNetworkInfo,   /* IconEntireNetwork	*/
        CNethoodIcons::s_asIconMainInfo,            /* IconMain		*/
        CNethoodIcons::s_asIconNetDriveInfo,        /* IconNetDrive		*/
};

CNethoodIcons::CNethoodIcons()
{
    m_himlSmall = NULL;
    m_himlLarge = NULL;
    m_himlTile = NULL;
}

CNethoodIcons::~CNethoodIcons()
{
    Destroy();
}

bool CNethoodIcons::Load()
{
    Destroy();

    if (!CreateImageLists())
    {
        Destroy();
        return false;
    }

    if (!LoadSystemIcons())
    {
        Destroy();
        return false;
    }

    return true;
}

HICON CNethoodIcons::GetIcon(int nSize, Icon icon) const
{
    HIMAGELIST himl;
    HICON hIcon = NULL;

    himl = GetImageList(nSize);
    if (himl != NULL)
    {
        hIcon = ImageList_GetIcon(himl, icon, ILD_NORMAL);
    }

    return hIcon;
}

void CNethoodIcons::Destroy()
{
    if (m_himlSmall != NULL)
    {
        ImageList_Destroy(m_himlSmall);
        m_himlSmall = NULL;
    }

    if (m_himlLarge != NULL)
    {
        ImageList_Destroy(m_himlLarge);
        m_himlLarge = NULL;
    }

    if (m_himlTile != NULL)
    {
        ImageList_Destroy(m_himlTile);
        m_himlTile = NULL;
    }
}

bool CNethoodIcons::CreateImageLists()
{
    UINT uFlags;
    BITMAPINFO bmi = {
        0,
    };

    uFlags = ILC_MASK | SalamanderGeneral->GetImageListColorFlags();

    m_himlSmall = ImageList_Create(16, 16, uFlags, _IconLast, 0);
    if (m_himlSmall != NULL)
    {
        ImageList_SetImageCount(m_himlSmall, _IconLast);
    }

    m_himlLarge = ImageList_Create(32, 32, uFlags, _IconLast, 0);
    if (m_himlLarge != NULL)
    {
        ImageList_SetImageCount(m_himlLarge, _IconLast);
    }

    m_himlTile = ImageList_Create(48, 48, uFlags, _IconLast, 0);
    if (m_himlTile != NULL)
    {
        ImageList_SetImageCount(m_himlTile, _IconLast);
    }

    return (m_himlSmall != NULL) && (m_himlLarge != NULL) &&
           (m_himlTile != NULL);
}

bool CNethoodIcons::LoadSystemIcons()
{
    int iIcon;
    TCHAR szIconLocation[MAX_PATH];
    PCTSTR pszFileName;
    HKEY hkeyShellIcons;
    LONG err;
    const LOADICONINFO* pLoadInfo;
    int nRetries;

    err = HANDLES_Q(RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Icons"),
        0,
        KEY_READ,
        &hkeyShellIcons));
    if (err != NO_ERROR)
    {
        hkeyShellIcons = NULL;
    }

    int i;
    for (i = 0; i < _IconLast; i++)
    {
        pLoadInfo = GetOsSpecificData(s_apLoadIconInfo[i]);
        assert(pLoadInfo != NULL);

        nRetries = 2;
        while (nRetries--)
        {
            pszFileName = pLoadInfo->pszFileName;
            iIcon = pLoadInfo->iIcon;

            if (nRetries == 1)
            {
                if (pLoadInfo->nStockIconId != SIID_INVALID)
                {
                    SHSTOCKICONINFO sStockIcon = {
                        0,
                    };

                    sStockIcon.cbSize = sizeof(SHSTOCKICONINFO);
                    if (SUCCEEDED(GetStockIconInfo(
                            pLoadInfo->nStockIconId,
                            SHGSI_ICONLOCATION,
                            &sStockIcon)))
                    {
#ifdef _UNICODE
                                                if (SUCCEEDED(StringCchCopy(
							szIconLocation,
							COUNTOF(szIconLocation),
							sStockIcon.szPath))
#else
                        if (WideCharToMultiByte(CP_ACP,
                                                0, sStockIcon.szPath,
                                                -1, szIconLocation,
                                                sizeof(szIconLocation),
                                                NULL, NULL) > 0)
#endif
						{
                            iIcon = sStockIcon.iIcon;
                            pszFileName = szIconLocation;
						}
						else
						{
                            --nRetries;
						}
                    }
                    else
                    {
                                                --nRetries;
                    }
                }
                else if (hkeyShellIcons != NULL)
                {
                    DWORD cbData;
                    DWORD dwType;
                    TCHAR szValueName[16];

                    StringCchPrintf(szValueName, COUNTOF(szValueName),
                                    TEXT("%d"), iIcon);

                    memset(szIconLocation, 0, sizeof(szIconLocation));
                    cbData = sizeof(szIconLocation) - sizeof(TCHAR);
                    err = SalamanderGeneral->SalRegQueryValueEx(hkeyShellIcons, szValueName,
                                                                NULL, &dwType, reinterpret_cast<LPBYTE>(szIconLocation),
                                                                &cbData);

                    if (err == NO_ERROR)
                    {
                                                iIcon = PathParseIconLocation(szIconLocation);
                                                pszFileName = szIconLocation;
                    }
                    else
                    {
                                                --nRetries;
                    }
                }
            }

            if (MyExtractIcon(static_cast<Icon>(i), pszFileName, iIcon))
            {
                break;
            }
        }
    }

    if (hkeyShellIcons != NULL)
    {
        HANDLES(RegCloseKey(hkeyShellIcons));
    }

    return true;
}

HIMAGELIST CNethoodIcons::GetImageList(int nSize) const
{
    switch (nSize)
    {
    case SALICONSIZE_16:
        return m_himlSmall;

    case SALICONSIZE_32:
        return m_himlLarge;

    case SALICONSIZE_48:
        return m_himlTile;
    }

    assert(0);
    return NULL;
}

HICON CNethoodIcons::ExtractLowColorSmallIcon(
    __in HMODULE hModule,
    __in PCTSTR pszIconId)
{
    HICON hIcon = NULL;
    HRSRC hRsrcInfo;

    hRsrcInfo = FindResource(hModule, pszIconId, RT_GROUP_ICON);
    if (hRsrcInfo != NULL)
    {
        HGLOBAL hRsrc;

        hRsrc = LoadResource(hModule, hRsrcInfo);
        if (hRsrc != NULL)
        {
            void* pRsrcData;
            DWORD cbRsrcData;

            cbRsrcData = SizeofResource(hModule, hRsrcInfo);
            pRsrcData = LockResource(hRsrc);

            hIcon = FindBestLowColorSmallIcon(
                hModule, pRsrcData, cbRsrcData);
        }
    }

    return hIcon;
}

PCTSTR CNethoodIcons::IconIndexToIconId(__in HMODULE hModule, __in int iIcon)
{
    ENUMICONRESINFO eiri = {
        0,
    };

    assert(iIcon >= 0);
    eiri.iIcon = iIcon;

    EnumResourceNames(hModule, RT_GROUP_ICON,
                      EnumIconResourceProc, reinterpret_cast<LONG_PTR>(&eiri));

    return eiri.pszId;
}

HICON CNethoodIcons::FindBestLowColorSmallIcon(
    __in HMODULE hModule,
    __in void* pData,
    __in DWORD cbData)
{
    MEMICONDIR* pDir;
    MEMICONDIRENTRY* pEntry;
    int nScore = INT_MAX;
    int iIcon = 0;
    int nIdealBpp;

    nIdealBpp = SalamanderGeneral->CanUse256ColorsBitmap() ? 8 : 4;

    pDir = static_cast<MEMICONDIR*>(pData);
    int i;
    for (i = 0; i < pDir->idCount && nScore > 0; i++)
    {
        int nScore2;

        pEntry = &pDir->idEntries[i];
        nScore2 = GetScoreForIcon(pEntry->bWidth, pEntry->bHeight,
                                  pEntry->wBitCount, 16, 16, nIdealBpp);
        if (nScore2 < nScore)
        {
            nScore = nScore2;
            iIcon = pEntry->nID;
        }
    }

    if (iIcon >= 0)
    {
        HRSRC hRsrcInfo;

        hRsrcInfo = FindResource(hModule, MAKEINTRESOURCE(iIcon), RT_ICON);
        if (hRsrcInfo != NULL)
        {
            HGLOBAL hRsrc;

            hRsrc = LoadResource(hModule, hRsrcInfo);
            if (hRsrc != NULL)
            {
                void* pData2;
                DWORD cbData2;

                cbData2 = SizeofResource(hModule, hRsrcInfo);
                pData2 = LockResource(hRsrc);
                if (pData2 != NULL)
                {
                    HICON hIcon;

                    hIcon = CreateIconFromResourceEx(
                        static_cast<BYTE*>(pData2),
                        cbData2, TRUE, 0x00030000,
                        16, 16, 0);

                    return hIcon;
                }
            }
        }
    }

    return NULL;
}

BOOL CALLBACK CNethoodIcons::EnumIconResourceProc(
    __in HMODULE hModule,
    __in PCTSTR pszType,
    __in PTSTR pszName,
    __in LONG_PTR lParam)
{
    ENUMICONRESINFO* peiri;

    peiri = reinterpret_cast<ENUMICONRESINFO*>(lParam);

    if (peiri->iIcon == 0)
    {
        if (IS_INTRESOURCE(pszName))
        {
            peiri->pszId = pszName;
        }
        else
        {
            size_t len = _tcslen(pszName) + 1;
            peiri->pszId = new TCHAR[len];
            StringCchCopy(peiri->pszId, len, pszName);
        }
        return FALSE;
    }

    --peiri->iIcon;
    return TRUE;
}

int CNethoodIcons::GetScoreForIcon(
    __in int nWidth,
    __in int nHeight,
    __in int nBpp,
    __in int nIdealWidth,
    __in int nIdealHeight,
    __in int nIdealBpp)
{
    int nScore = 0;

    if (nWidth > nIdealWidth)
    {
        // Wider is better...
        nScore += 50;
    }
    else if (nWidth < nIdealWidth)
    {
        // ...than narrow.
        nScore += 100;
    }

    if (nHeight > nIdealHeight)
    {
        // Taller is better...
        nScore += 50;
    }
    else if (nHeight < nIdealHeight)
    {
        // ...than short.
        nScore += 100;
    }

    if (nBpp > nIdealBpp)
    {
        // Images with more colors draws badly
        // on low color displays.
        nScore += 100;
    }
    else if (nBpp < nIdealBpp)
    {
        nScore += 50;
    }

    return nScore;
}

bool CNethoodIcons::MyExtractIcon(
    __in Icon icon,
    __in PCTSTR pszFileName,
    __in int iIconIndex)
{
    HMODULE hModule;
    PCTSTR pszIconId;
    int cIconsLoaded = 0;
    UINT uLoadFlags;
    HICON hicoSmall = NULL;
    HICON hicoLarge = NULL;
    HICON hicoTile = NULL;

    uLoadFlags = SalamanderGeneral->GetIconLRFlags();

    hModule = HANDLES_Q(LoadLibraryEx(pszFileName, NULL, LOAD_LIBRARY_AS_DATAFILE));
    if (hModule != NULL)
    {
        if (iIconIndex < 0)
        {
            pszIconId = MAKEINTRESOURCE(-iIconIndex);
        }
        else
        {
            pszIconId = IconIndexToIconId(hModule, iIconIndex);
        }

        hicoSmall = LoadIconFromModule(hModule, pszIconId, 16, 16, uLoadFlags);
        hicoLarge = LoadIconFromModule(hModule, pszIconId, 32, 32, uLoadFlags);
        hicoTile = LoadIconFromModule(hModule, pszIconId, 48, 48, uLoadFlags);

        if (!IS_INTRESOURCE(pszIconId))
        {
            delete[] const_cast<PTSTR>(pszIconId);
        }

        HANDLES(FreeLibrary(hModule));
    }
    else if (GetLastError() == ERROR_BAD_EXE_FORMAT)
    {
        assert(iIconIndex == 0);

        hicoSmall = LoadIconFromFile(pszFileName, 16, 16, uLoadFlags);
        hicoLarge = LoadIconFromFile(pszFileName, 32, 32, uLoadFlags);
        hicoTile = LoadIconFromFile(pszFileName, 48, 48, uLoadFlags);
    }

    if (hicoSmall != NULL)
    {
        ++cIconsLoaded;
        ImageList_ReplaceIcon(m_himlSmall, icon, hicoSmall);
        HANDLES(DestroyIcon(hicoSmall));
    }

    if (hicoLarge != NULL)
    {
        ++cIconsLoaded;
        ImageList_ReplaceIcon(m_himlLarge, icon, hicoLarge);
        HANDLES(DestroyIcon(hicoLarge));
    }

    if (hicoTile != NULL)
    {
        ++cIconsLoaded;
        ImageList_ReplaceIcon(m_himlTile, icon, hicoTile);
        HANDLES(DestroyIcon(hicoTile));
    }

    return (cIconsLoaded >= 3);
}

HICON CNethoodIcons::LoadIconFromModule(
    __in HMODULE hModule,
    __in PCTSTR pszIconId,
    __in int nWidth,
    __in int nHeight,
    __in UINT uFlags)
{
    return static_cast<HICON>(HANDLES_Q(LoadImage(hModule, pszIconId,
                                                  IMAGE_ICON, nWidth, nHeight, uFlags)));
}

HICON CNethoodIcons::LoadIconFromFile(
    __in PCTSTR pszFileName,
    __in int nWidth,
    __in int nHeight,
    __in UINT uFlags)
{
    return static_cast<HICON>(HANDLES_Q(LoadImage(NULL, pszFileName,
                                                  IMAGE_ICON, nWidth, nHeight, uFlags | LR_LOADFROMFILE)));
}

HRESULT CNethoodIcons::GetStockIconInfo(
    __in SHSTOCKICONID siid,
    __in UINT uFlags,
    __inout SHSTOCKICONINFO* psii)
{
    typedef HRESULT(WINAPI * PFN_SHGetStockIconInfo)(SHSTOCKICONID, UINT, SHSTOCKICONINFO*);

    if (_winmajor >= 6)
    {
        // SHGetStockIconInfo is implemented on Vista and
        // later systems only.

        HMODULE hShell32;

        hShell32 = GetModuleHandle(TEXT("shell32.dll"));
        if (hShell32 != NULL)
        {
            PFN_SHGetStockIconInfo pfnSHGetStockIconInfo;

            pfnSHGetStockIconInfo = reinterpret_cast<PFN_SHGetStockIconInfo>(
                GetProcAddress(hShell32, "SHGetStockIconInfo")); // Min: Vista

            if (pfnSHGetStockIconInfo != NULL)
            {
                return (*pfnSHGetStockIconInfo)(siid, uFlags, psii);
            }
        }
    }

    return E_NOTIMPL;
}
