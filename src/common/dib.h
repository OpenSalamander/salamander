// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// nacte z resourcu bitmapu hRsrc (ziskan z FindResource(...)),
// premapuje mapCount barev: mapColor[i] -> toColor[i]
// a vytvori bitmapu kompatibilni s desktopem

HBITMAP LoadBitmapAndMapColors(HINSTANCE hInst, HRSRC hRsrc, int mapCount,
                               COLORREF* mapColor, COLORREF* toColor);

DWORD DIBHeight(LPSTR lpDIB);
DWORD DIBWidth(LPSTR lpDIB);
HBITMAP DIBToBitmap(HANDLE hDIB, HPALETTE hPal);
