// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// loads the bitmap resource hRsrc (obtained with FindResource(...)),
// remaps mapCount colors: mapColor[i] -> toColor[i],
// and creates a bitmap compatible with the desktop

HBITMAP LoadBitmapAndMapColors(HINSTANCE hInst, HRSRC hRsrc, int mapCount,
                               COLORREF* mapColor, COLORREF* toColor);

DWORD DIBHeight(LPSTR lpDIB);
DWORD DIBWidth(LPSTR lpDIB);
HBITMAP DIBToBitmap(HANDLE hDIB, HPALETTE hPal);
