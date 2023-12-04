// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "..\undelete.rh2"
#include "texts.rh2"

#include "snapshot.h"
#include "miscstr.h"
#include "dataruns.h"
#include "stream.h"

char Junk_ROBS[20] = {0, 1, 0, 0, 'R', 0, 'O', 0, 'B', 0, 'S', 0, 0, 0, 0, 0, 0, 0, 0, 0};
char Junk_NTFS[20] = {'N', 0, 'T', 0, 'F', 0, 'S', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char Junk_GURE[12] = {'G', 0, 'U', 0, 'R', 0, 'E', 0, 0, 0, 0, 0};

template <>
const char* const EFIC_CONTEXT<char>::STRING_EFS = "$EFS";
template <>
const wchar_t* const EFIC_CONTEXT<wchar_t>::STRING_EFS = L"$EFS";
