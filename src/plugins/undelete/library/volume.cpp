// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "..\undelete.rh2"
#include "snapshot.h"

#include "miscstr.h"
#include "os.h"
#include "volume.h"

const char* CVolume<char>::STRING_VOLUME_NT = "\\\\.\\";
const wchar_t* CVolume<wchar_t>::STRING_VOLUME_NT = L"\\\\.\\";

const char* CVolume<char>::STRING_VOLUME_95 = "\\\\.\\vwin32";
const wchar_t* CVolume<wchar_t>::STRING_VOLUME_95 = L"\\\\.\\vwin32";

const char* CVolume<char>::STRING_ROOT = "?:\\";
const wchar_t* CVolume<wchar_t>::STRING_ROOT = L"?:\\";

const char* CVolume<char>::STRING_FAT32 = "FAT32";
const wchar_t* CVolume<wchar_t>::STRING_FAT32 = L"FAT32";

const char CVolume<char>::CHAR_A = 'A';
const wchar_t CVolume<wchar_t>::CHAR_A = L'A';

const char CVolume<char>::CHAR_BSLASH = '\\';
const wchar_t CVolume<wchar_t>::CHAR_BSLASH = L'\\';

const char CVolume<char>::CHAR_COLON = ':';
const wchar_t CVolume<wchar_t>::CHAR_COLON = L':';
