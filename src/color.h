// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Converts colors from RGB to hue-luminance-saturation (HLS) format.
void ColorRGBToHLS(COLORREF clrRGB, WORD* pwHue, WORD* pwLuminance, WORD* pwSaturation);

// Converts colors from hue-luminance-saturation (HLS) to RGB format.
COLORREF ColorHLSToRGB(WORD wHue, WORD wLuminance, WORD wSaturation);
