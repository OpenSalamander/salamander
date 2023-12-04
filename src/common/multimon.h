// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

HWND GetTopVisibleParent(HWND hParent);
BOOL MultiMonGetDefaultWindowPos(HWND hByWnd, POINT* p);
void MultiMonGetClipRectByRect(const RECT* rect, RECT* workClipRect, RECT* monitorClipRect);
void MultiMonGetClipRectByWindow(HWND hByWnd, RECT* workClipRect, RECT* monitorClipRect);
void MultiMonCenterWindow(HWND hWindow, HWND hByWnd, BOOL findTopWindow);
void MultiMonCenterWindowByRect(HWND hWindow, const RECT& clipR, const RECT& byR);
BOOL MultiMonEnsureRectVisible(RECT* rect, BOOL partialOK);
