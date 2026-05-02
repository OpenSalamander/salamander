// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// Following line suppresses the MS Visual C++ Linker warning 4221
// warning LNK4221: no public symbols found; archive member will be inaccessible
// It shows in VC2008, building undellib.lib, Debug, defined: TRACE_ENABLE.
namespace
{
    char dummy;
};
