// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#include "precomp.h"
#include "salutils.h"

const void* GetOsSpecificDataHelper(
    __in const void* pData,
    __in size_t uSizeOfItem,
    __in size_t uDataOffset)
{
    const DWORD* pdwOsVersion;
    const DWORD* pBestMatch = NULL;
    DWORD dwPlatform;
    DWORD dwDataPlatform;
    DWORD dwMajor, dwMinor;

    dwPlatform = _osplatform << OSSPECIFIC_PLATFORM_SHIFT;
    pdwOsVersion = reinterpret_cast<const DWORD*>(pData);

    while (*pdwOsVersion != OSSPECIFIC_LAST)
    {
        dwDataPlatform = (*pdwOsVersion) & OSSPECIFIC_PLATFORM_MASK;
        if (dwDataPlatform == dwPlatform || dwDataPlatform == OSSPECIFIC_PLATFORM_INDEPENDENT)
        {
            dwMajor = HIBYTE(LOWORD(*pdwOsVersion));
            dwMinor = LOBYTE(LOWORD(*pdwOsVersion));
            if (dwMajor < _winmajor || (dwMajor == _winmajor && dwMinor <= _winminor))
            {
                pBestMatch = pdwOsVersion;
                break;
            }
        }
        pdwOsVersion = reinterpret_cast<const DWORD*>(
            reinterpret_cast<const char*>(pdwOsVersion) + uSizeOfItem);
    }

    if (pBestMatch == NULL)
    {
        return NULL;
    }

    return reinterpret_cast<const char*>(pBestMatch) + uDataOffset;
}
