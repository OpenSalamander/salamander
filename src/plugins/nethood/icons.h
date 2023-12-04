// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

/// Icon cache.
class CNethoodIcons
{
public:
    /// Identifier of the supported icon.
    enum Icon
    {
        IconSimpleFile,
        IconSimpleDirectory,
        IconDomainOrGroup,
        IconServer,
        IconShare,
        IconNetwork,
        IconEntireNetwork,
        IconMain,
        IconNetDrive,

        /// Number of supported icons.
        _IconLast,

    };

private:
#if (_WIN32_WINNT < 0x0600)

#if !defined(_WIN64)
#include <pshpack1.h>
#endif

    enum SHSTOCKICONID
    {
        SIID_INVALID = -1,
        SIID_DOCNOASSOC = 0,
        SIID_FOLDER = 3,
        SIID_DRIVENET = 9,
        SIID_WORLD = 13,
        SIID_SERVER = 15,
        SIID_PRINTER = 16,
        SIID_MYNETWORK = 17,
        SIID_SERVERSHARE = 51
    };

    struct SHSTOCKICONINFO
    {
        DWORD cbSize;
        HICON hIcon;
        int iSysImageIndex;
        int iIcon;
        WCHAR szPath[MAX_PATH];
    };

    enum _SHSTOCKICONFLAG
    {
        SHGSI_ICONLOCATION = 0
    };

#if !defined(_WIN64)
#include <poppack.h>
#endif

#endif

    /// Context value used internally by the IconIndexToIconId method.
    struct ENUMICONRESINFO
    {
        /// Zero-based index of the icon to look-up.
        int iIcon;

        /// Identifier of the icon. If IS_INTRESOURCE(pszId) is true
        /// pszId is allocated dynamically and should be deleted when
        /// no longer needed.
        PTSTR pszId;
    };

    /// This structure contains info for the icon loading.
    struct LOADICONINFO
    {
        /// Path to the file containing the icon.
        PCTSTR pszFileName;

        /// Index of the icon. Negative indices indicate icon
        /// identifiers.
        int iIcon;

        /// Identifier of the icon for SHGetStockIconInfo API.
        SHSTOCKICONID nStockIconId;
    };

    typedef _OSSPECIFICDATA<LOADICONINFO> OSLOADICONINFO;

    // Following structures for accessing icon resource data are taken
    // from the IconPro application and the 'Icons in Win32' article
    // (http://msdn.microsoft.com/en-us/library/ms997538.aspx).
    // These first two structs represent how the icon information is stored
    // when it is bound into a EXE or DLL file. Structure members are WORD
    // aligned and the last member of the structure is the ID instead of
    // the imageoffset.
#pragma pack(push)
#pragma pack(2)
    struct MEMICONDIRENTRY
    {
        BYTE bWidth;        // Width of the image
        BYTE bHeight;       // Height of the image
        BYTE bColorCount;   // Number of colors in image (0 if >=8bpp)
        BYTE bReserved;     // Reserved
        WORD wPlanes;       // Color Planes
        WORD wBitCount;     // Bits per pixel
        DWORD dwBytesInRes; // How many bytes in this resource?
        WORD nID;           // The ID
    };

    struct MEMICONDIR
    {
        WORD idReserved;              // Reserved
        WORD idType;                  // Resource type (1 for icons)
        WORD idCount;                 // How many images?
        MEMICONDIRENTRY idEntries[1]; // The entries for each image
    };
#pragma pack(pop)

    /// Handle to the image list containing small (16x16) icons.
    HIMAGELIST m_himlSmall;

    /// Handle to the image list containing large (32x32) icons.
    HIMAGELIST m_himlLarge;

    /// Handle to the image list containing tile (48x48) icons.
    HIMAGELIST m_himlTile;

    static const OSLOADICONINFO s_asIconSimpleDirectoryInfo[];
    static const OSLOADICONINFO s_asIconSimpleFileInfo[];
    static const OSLOADICONINFO s_asIconDomainOrGroupInfo[];
    static const OSLOADICONINFO s_asIconServerInfo[];
    static const OSLOADICONINFO s_asIconShareInfo[];
    static const OSLOADICONINFO s_asIconNetworkInfo[];
    static const OSLOADICONINFO s_asIconEntireNetworkInfo[];
    static const OSLOADICONINFO s_asIconMainInfo[];
    static const OSLOADICONINFO s_asIconNetDriveInfo[];
    static const OSLOADICONINFO* s_apLoadIconInfo[_IconLast];

    void Destroy();

    bool CreateImageLists();

    bool LoadSystemIcons();

    static HICON ExtractLowColorSmallIcon(
        __in HMODULE hModule,
        __in PCTSTR pszIconId);

    static BOOL CALLBACK EnumIconResourceProc(
        __in HMODULE hModule,
        __in PCTSTR pszType,
        __in PTSTR pszName,
        __in LONG_PTR lParam);

    static PCTSTR IconIndexToIconId(__in HMODULE hModule, __in int iIcon);

    static HICON FindBestLowColorSmallIcon(
        __in HMODULE hModule,
        __in void* pData,
        __in DWORD cbData);

    /// Returns score for matching the icon properties to the ideal
    /// icon properties.
    /// \param nWidth Width of the icon, in pixels.
    /// \param nHeight Height of the icon, in pixels.
    /// \param nBpp Color depth of the icon.
    /// \param nIdealWidth Ideal width of the icon, in pixels.
    /// \param nIdealHeight Ideal height of the icon, in pixels.
    /// \param nIdealBpp Ideal color depth.
    /// \return Returns the matching score for the icon. The smaller
    ///         the return value is, the better the icon properties
    ///         match the ideal icon propertis. The return value of zero
    ///         is for exact match.
    static int GetScoreForIcon(
        __in int nWidth,
        __in int nHeight,
        __in int nBpp,
        __in int nIdealWidth,
        __in int nIdealHeight,
        __in int nIdealBpp);

    bool MyExtractIcon(
        __in Icon icon,
        __in PCTSTR pszFileName,
        __in int iIconIndex);

    static HICON LoadIconFromModule(
        __in HMODULE hModule,
        __in PCTSTR pszIconId,
        __in int nWidth,
        __in int nHeight,
        __in UINT uFlags);

    static HICON LoadIconFromFile(
        __in PCTSTR pszFileName,
        __in int nWidth,
        __in int nHeight,
        __in UINT uFlags);

    /// Retrieves information about system-defined Shell icons.
    /// Binds to SHGetStockIconInfo function on Vista and later.
    /// \param siid One of the values from the SHSTOCKICONID enumeration
    ///        that specifies which icon should be retrieved.
    /// \param uFlags A combination of zero or more of values that specify
    ///        which information is requested. For list of possible values
    ///        see documentation of the SHGetStockIconInfo function.
    /// \param psii A pointer to a SHSTOCKICONINFO structure. When this
    ///        method is called, the cbSize member of this structure needs
    ///        to be set to the size of the SHSTOCKICONINFO structure.
    ///        When this function returns, the SHSTOCKICONINFO structure
    ///        contains the requested information.
    /// \return Returns S_OK if successful, or an error value otherwise.
    static HRESULT GetStockIconInfo(
        __in SHSTOCKICONID siid,
        __in UINT uFlags,
        __inout SHSTOCKICONINFO* psii);

public:
    /// Constructor.
    CNethoodIcons();

    /// Destructor.
    ~CNethoodIcons();

    /// Loads the icons from the system into the icon cache.
    /// \return If all the icons are successfully loaded the return
    ///         value is true. If the method fails the return value
    ///         is false.
    bool Load();

    /// Retrieves appropriate image list.
    /// \param nSize Size of the icons required. One of the
    ///        SALICONSIZE_xxx values.
    /// \return Returns handle to the image list. The return value
    ///         is NULL if the nSize parameter is invalid or the
    ///         required image list is not loaded.
    HIMAGELIST GetImageList(int nSize) const;

    /// Returns appropriate network icon.
    /// \param nSize Size of the requested icon. This is one
    ///        of the SALICONSIZE_xxx constans.
    /// \param icon Identifier of the icon to retrieve. This is
    ///        value from the Icon enumeration.
    /// \return If the method succeeds the return value is handle
    ///         to the requested icon. The caller is responsible
    ///         for destroying the icon using the DestroyIcon function
    ///         when it no longer needs it. If the method fails the
    ///         return value is NULL.
    HICON GetIcon(int nSize, Icon icon) const;
};
