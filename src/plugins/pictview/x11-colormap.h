// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <string_view>

/// converts X11 color name to COLORREF
bool LookupX11Color(std::string_view name, COLORREF& outColor);
