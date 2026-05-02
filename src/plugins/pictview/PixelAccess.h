// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

bool GetRGBAtCursor(LPPVHandle PVHandle, DWORD Colors, int x, int y, RGBQUAD* pRGB, int* pIndex);
PVCODE CalculateHistogram(LPPVHandle PVHandle, const LPPVImageInfo pvii, LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb);
