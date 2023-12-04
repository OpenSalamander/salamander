// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "..\undelete.rh2"
//#include "texts.rh2"

#include "undelete.h"
#include "miscstr.h"
#include "os.h"
#include "volume.h"
#include "snapshot.h"
#include "volenum.h"
#include "bitmap.h"

#include "../undelete.rh2"
#include "../dialogs.h"
#include "../undelete.h"

#include "dataruns.h"
#include "stream.h"
#include "ntfs.h"

// **************************************************************************************
//
//   CMFTSnapshot
//

const char* const CMFTSnapshot<char>::STRING_EMPTY = "";
const wchar_t* const CMFTSnapshot<wchar_t>::STRING_EMPTY = L"";

const char* const CMFTSnapshot<char>::STRING_DOT = ".";
const wchar_t* const CMFTSnapshot<wchar_t>::STRING_DOT = L".";

const char* const CMFTSnapshot<char>::STRING_MFT = "$MFT";
const wchar_t* const CMFTSnapshot<wchar_t>::STRING_MFT = L"$MFT";

const char* const CMFTSnapshot<char>::STRING_EFS = "$EFS";
const wchar_t* const CMFTSnapshot<wchar_t>::STRING_EFS = L"$EFS";

const char* const CMFTSnapshot<char>::STRING_BITMAP = "$Bitmap";
const wchar_t* const CMFTSnapshot<wchar_t>::STRING_BITMAP = L"$Bitmap";
