// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* SFX_TDTEMP;
extern const char* SFX_TDPROGFILES;
extern const char* SFX_TDWINDIR;
extern const char* SFX_TDSYSDIR;
extern const char* SFX_TDENVVAR;
extern const char* SFX_TDREGVAL;

int ImportSFXSettings(const char* textData, CSfxSettings* settings, const char* zip2sfxDir);

// return valueas
//
// lower word is one of the following values:
//
// 0 OK
// 1 bad temp
// 2 missing right parenthesis
// 3 unknown keyword in $()
// 4 bad key
//
// higher word contains index of where the syntax error occured in the target dir string
DWORD ParseTargetDir(const char* path, unsigned* targetDir, const char** subDir,
                     const char** dirSpecLeft, const char** dirSpecRight, HKEY* Key);
