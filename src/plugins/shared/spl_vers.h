// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED
// 
//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

// WARNING: cannot be replaced by "#pragma once" because it is included from .rc file and it seems resource compiler does not support "#pragma once"
#ifndef __SPL_VERS_H
#define __SPL_VERS_H

#if defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)
#error this file is not editable by Microsoft Visual C++
#endif //defined(APSTUDIO_INVOKED) && !defined(APSTUDIO_READONLY_SYMBOLS)

// conversion macros num->str
#define VERSINFO_xstr(s) VERSINFO_str(s)
#define VERSINFO_str(s) #s

#define VERSINFO_SALAMANDER_MAJOR 5
#define VERSINFO_SALAMANDER_MINORA 0
#define VERSINFO_SALAMANDER_MINORB 0

#if (VERSINFO_SALAMANDER_MINORB == 0) // don't write zero on the hundredths 2.50 -> 2.5
#define VERSINFO_SALAMANDER_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) "." VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_BETAVERSION_TXT
#define VERSINFO_SAL_SHORT_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_BETAVERSIONSHORT_TXT
#else
#define VERSINFO_SALAMANDER_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) "." VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORB) VERSINFO_BETAVERSION_TXT
#define VERSINFO_SAL_SHORT_VERSION VERSINFO_xstr(VERSINFO_SALAMANDER_MAJOR) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORA) VERSINFO_xstr(VERSINFO_SALAMANDER_MINORB) VERSINFO_BETAVERSIONSHORT_TXT
#endif

#ifdef VERSINFO_MAJOR      // only defined if used from plugin
#if (VERSINFO_MINORB == 0) // don't write zero on the hundredths 2.50 -> 2.5
#define VERSINFO_VERSION VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_BETAVERSION_TXT
#define VERSINFO_VERSION_NO_PLATFORM VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_BETAVERSION_TXT_NO_PLATFORM
#else
#define VERSINFO_VERSION VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_xstr(VERSINFO_MINORB) VERSINFO_BETAVERSION_TXT
#define VERSINFO_VERSION_NO_PLATFORM VERSINFO_xstr(VERSINFO_MAJOR) "." VERSINFO_xstr(VERSINFO_MINORA) VERSINFO_xstr(VERSINFO_MINORB) VERSINFO_BETAVERSION_TXT_NO_PLATFORM
#endif
#endif

#ifdef _WIN64
#define SAL_VER_PLATFORM "x64"
#else // _WIN64
#define SAL_VER_PLATFORM "x86"
#endif // _WIN64

// VERSINFO_BUILDNUMBER:
//
// Used to easily distinguish versions of all modules between individual
// versions of Salamander (it is the last component of the version number of
// all plugins and Salamander). Increase with each version (IB, DB, PB, beta,
// release or even just a test version sent to one user). An overview of the
// various types of versions is in the file doc\versions.txt. Always introduce
// a comment with a description of which version of Salamander the new build
// number belongs to.
// 
// Overview of used values of VERSINFO_BUILDNUMBER:
// 9 - 2.5 beta 9
// 10 - 2.5 beta 10
// 11 - 2.5 beta 11
// 13 - 2.5 RC1
// 14 - 2.5 RC2
// 15 - 2.5 RC3
// 0 - 2.5
// 16 - 2.51
// 18 - 2.52 beta 1
// 29 - 2.52 beta 2
// 32 - 2.52
// 49 - 2.53 beta 1
// 57 - 2.53 beta 2
// 63 - 2.53
// 69 - 2.54
// 91 - 3.0 beta 1
// 97 - 3.0 beta 2
// 108 - 3.0 beta 3
// 114 - 3.0 beta 4
// 120 - 3.0
// 126 - 3.01
// 132 - 3.02
// 138 - 3.03
// 144 - 3.04
// 150 - 3.05
// 156 - 3.06
// 165 - 3.07
// 174 - 3.08
// 175 - 3.08 (SDK)
// 176 - 3.08 (CB176)
// 177 - 4.0 beta 1 (DB177)
// 178 - 4.0 beta 1 (CB178)
// 179 - 4.0 beta 1 (IB179)
// 180 - 4.0
// 181 - 4.0 (SDK)
// 182 - 4.0 (CB182)
// 183 - 5.0

// ! IMPORTANT: all build numbers must be written to the "default" branch, and then
//             to the secondary branch (the complete list is only in the "default" branch)
#define VERSINFO_BUILDNUMBER 183

// VERSINFO_BETAVERSION_TXT:
//
// Changed with each build, in the case of the release version VERSINFO_BETAVERSION_TXT=="".
// If we release special bug fix beta versions like 2.5 beta 9a, increase
// VERSINFO_BUILDNUMBER by one and set VERSINFO_BETAVERSION_TXT==" beta 9a".
//
// VERSINFO_BETAVERSIONSHORT_TXT is used to name bug reports, it is the shortest possible
// name.

// examples ("x86" is for 32-bit version, "x64" for 64-bit version, in the following
// examples x86/x64 are interchangeable): " (x86)" (for release versions), " beta 2 (x64)",
// " beta 2 (SDK x86)", " RC1 (x64)", " beta 2 (IB21 x86)", " beta 2 (DB21 x64)",
// " beta 2 (PB21 x86)"
#define VERSINFO_BETAVERSION_TXT " (" SAL_VER_PLATFORM ")"
#define VERSINFO_BETAVERSION_TXT_NO_PLATFORM "" // copy of the line above + delete SAL_VER_PLATFORM + if the bracket is empty, delete it + delete unnecessary spaces

// examples (x86/x64 see previous paragraph): "x86" (for release versions), "B2x64",
// "B2SDKx86", "RC1x64", "B2IB21x86", "B2DB21x64", "B2PB21x86"
#define VERSINFO_BETAVERSIONSHORT_TXT SAL_VER_PLATFORM

// LAST_VERSION_OF_SALAMANDER:
//
// Support for checking the current version of Salamander, which internal plugins
// (distributed in one package with Salamander) perform during the entry-point
// (SalamanderPluginEntry) see the method CSalamanderPluginEntryAbstract::GetVersion()
// (in spl_base.h). It is used mainly for simplicity: an internal plugin can call
// any method from the Salamander interface, because after checking the last version
// of Salamander it has the certainty that Salamander contains it (it only threatens
// to load into a newer version of Salamander, which must also contain these methods).
// 
// It is also used in reverse: so that the internal plugin has the certainty that
// Salamander will call all methods (including the latest ones), it returns this
// version as the version for which the plugin was built (see the export of the
// SalamanderPluginGetReqVer plugin).
// 
// If any plugin returns a lower version from SalamanderPluginGetReqVer than
// LAST_VERSION_OF_SALAMANDER (for backward compatibility with older versions
// of Salamander), it should add the export SalamanderPluginGetSDKVer and return
// LAST_VERSION_OF_SALAMANDER from it (the SDK version used to build the plugin),
// so that Salamander (e.g. current or newer) can also use methods of the plugin
// that were not yet in the version returned from SalamanderPluginGetReqVer.
// 
// When changing the interface, it is necessary to follow the procedure given in
// doc\how_to_change.txt.
// 
// Overview of used values of LAST_VERSION_OF_SALAMANDER:
//   1  - 1.6 beta 4 + 5
//   2  - 1.6 beta 6
//   3  - 1.6 beta 7
//   4  - 2.0
//   5  - 2.5 beta 1
//   6  - 2.5 beta 2
//   7  - 2.5 beta 3
//   8  - 2.5 beta 4
//   9  - 2.5 beta 5
//   10 - 2.5 beta 6
//   11 - 2.5 beta 7
//   12 - 2.5 beta 8
//   13 - 2.5 beta 9
//   14 - 2.5 beta 10
//   15 - 2.5 beta 10a
//   16 - 2.5 beta 11
//   17 - 2.5 beta 12 (internal only, we release RC1 instead of it)
//   18 - 2.5 RC1
//   19 - 2.5 RC2
//   20 - 2.5 RC3
//   21 - 2.5
//   22 - 2.51
//   23 - 2.52 beta 1 (CAUTION: incompatible SDK with previous and next versions)
//   29 - 2.52 beta 2
//   31 - 2.52
//   39 - 2.53 beta 1 + 2.53 beta 1a
//   41 - 2.53 beta 2
//   43 - 2.53
//   45 - 2.54
//   54 - 3.0 beta 1
//   56 - 3.0 beta 2
//   60 - 3.0 beta 3
//   62 - 3.0 beta 4
//   64 - 3.0
//   66 - 3.01
//   68 - 3.02
//   70 - 3.03
//   72 - 3.04
//   74 - 3.05
//   76 - 3.06
//   79 - 3.07
//   81 - 3.08
// IMPORTANT: all versions from VC2008 must be < 100, all versions from VC2019 must be >= 100,
//            new version numbers must be written to the "default" branch, and then
//            to the secondary branch (the complete list is only in the "default" branch)
//   101 - 4.0 beta 1 (DB177)
//   102 - 4.0
//   103 - 5.0

#define LAST_VERSION_OF_SALAMANDER 103
#define REQUIRE_LAST_VERSION_OF_SALAMANDER "This plugin requires Open Salamander 5.0 (" SAL_VER_PLATFORM ") or later."

#endif // __SPL_VERS_H
