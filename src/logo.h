// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// Splash Screen
//

BOOL SplashScreenOpen();                           // otevre splash screen okenko
void SplashScreenCloseIfExist();                   // pokud okenko existuje, zavre ho
BOOL ExistSplashScreen();                          // detekuje existenci splash screen okna
HWND GetSplashScreenHandle();                      // vrati handle splash screenu, pripadne NULL pokud neexistuje
void IfExistSetSplashScreenText(const char* text); // pokud existuje, nastavi text, ktery se hned zobrazi
