// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "miscstr.h"
#include "volume.h"
#include "snapshot.h"
#include "bitmap.h"
#include "exfat.h"

#ifdef MEASURE_READING_PERFORMANCE
LARGE_INTEGER ReadingDirEntriesTicks = {0};
LARGE_INTEGER ticks = {0};
#endif
