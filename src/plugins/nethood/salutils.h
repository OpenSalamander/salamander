// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

#define OSSPECIFIC_PLATFORM_SHIFT 30
#define OSSPECIFIC_PLATFORM_INDEPENDENT 0u
#define OSSPECIFIC_PLATFORM_NT (VER_PLATFORM_WIN32_NT << OSSPECIFIC_PLATFORM_SHIFT)
#define OSSPECIFIC_PLATFORM_MASK (OSSPECIFIC_PLATFORM_NT)

// Petr: pokud se sem bude pridavat novejsi verze Windows (posledni detekovana je Windows 8.1),
//       je nutne pridat tez jeji detekci do DllMain

#define OSSPECIFIC(major, minor, platform) ((DWORD)(((minor)&0xFF) | (((major)&0xFF) << 8) | platform))
#define OSSPECIFIC_7 OSSPECIFIC(6, 1, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_SERVER2008 OSSPECIFIC(6, 0, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_VISTA OSSPECIFIC(6, 0, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_SERVER2003 OSSPECIFIC(5, 2, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_XP OSSPECIFIC(5, 1, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_2K OSSPECIFIC(5, 0, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_ANY_NT OSSPECIFIC(0, 0, OSSPECIFIC_PLATFORM_NT)
#define OSSPECIFIC_ANY OSSPECIFIC(0, 0, OSSPECIFIC_PLATFORM_INDEPENDENT)
#define OSSPECIFIC_LAST 0xFFFFFFFFu

/// This structure drives the matching algorithm of the GetOsSpecificData
/// function.
template <typename T>
struct _OSSPECIFICDATA
{
    /// The version of the target Windows operating system.
    /// This can be one of the predefined OSSPECIFIC_xxx constant.
    /// The last structure in the array must have this value se to
    /// OSSPECIFIC_LAST.
    DWORD dwOsVersion;

    /// User defined data structure.
    T data;
};

/// Helper function for the GetOsSpecificData template function.
/// \param pData Pointer to array of the _OSSPECIFICDATA structures.
/// \param uSizeOfItem Size of the _OSSPECIFICDATA structure, in bytes.
/// \param uDataOffset Byte offset of the user data within the _OSSPECIFICDATA
///        structure. This may differ because of platforms (x86/x64) and
///        various data type alignment requirements.
/// \return If the function finds the OS specific data, the return value
///         is pointer to the user data.
///         If the function fails to to find the OS specific data, the
///         return value is NULL.
const void* GetOsSpecificDataHelper(
    __in const void* pData,
    __in size_t uSizeOfItem,
    __in size_t uDataOffset);

/// The GetOsSpecificData function returns the best matching user defined data
/// based on the current Windows version.
/// \param pData Pointer to array of the _OSSPECIFICDATA structures.
///        The dwOsVersion member of the last element of the array have to have
///        the OSSPECIFIC_LAST value.
///        The elements in the array must be sorted descending by the OS
///        version.
/// \return The function returns pointer to the first matching user data.
///         if no matching data is found, the return value is NULL.
///
/// Example:
///
///	typedef struct MYDATA {
///		int intData;
///	};
///
///	static const _OSSPECIFICDATA<MYDATA> myOsData[] = {
///		OSSPECIFIC_7,				7,
///		OSSPECIFIC_VISTA,		6,
///		OSSPECIFIC_ANY_NT,	5,
///
///		// Terminator.
///		OSSPECIFIC_LAST,	0
///	};
///
///	MYDATA *data = GetOsSpecificData(myOsData);
///
///	The above call will return pointer to the MYDATA structure containing
///	value of 7 on Windows 7 and later systems. The returned structure
///	will contain value of 6 on Windows Vista and Windows Server 2008.
///	On Windows 2000, Windows XP, and Windows Server 2003 it will contain 5.
template <typename T>
const T* GetOsSpecificData(__in const _OSSPECIFICDATA<T>* pData)
{
    return reinterpret_cast<const T*>(
        GetOsSpecificDataHelper(pData, sizeof(_OSSPECIFICDATA<T>), offsetof(_OSSPECIFICDATA<T>, data)));
}
