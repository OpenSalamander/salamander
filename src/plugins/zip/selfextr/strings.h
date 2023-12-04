// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#ifndef STRING
#ifndef __STRIGS_H
//First time through, define the enum list
#define MAKE_ENUM_LIST
#else
//Repeated inclusions of this file are no-ops unless STRING is defined
#define STRING(code, string)
#endif //__STRIGS_H
#endif //STRING

#ifdef MAKE_ENUM_LIST

typedef enum
{

#define STRING(code, string) code,

#endif //MAKE_ENUM_LIST

#ifdef ENGLISH_VERSION
#include "language\\english\\texts.h"
#define LANG_DEFINED
#endif // ENGLISH_VERSION

#ifdef CZECH_VERSION
#include "language\\czech\\texts.h"
#define LANG_DEFINED
#endif // CZECH_VERSION

#ifdef SLOVAK_VERSION
#include "language\\slovak\\texts.h"
#define LANG_DEFINED
#endif // SLOVAK_VERSION

#ifdef GERMAN_VERSION
#include "language\\german\\texts.h"
#define LANG_DEFINED
#endif // GERMAN_VERSION

#ifdef DUTCH_VERSION
#include "language\\dutch\\texts.h"
#define LANG_DEFINED
#endif // DUTCH_VERSION

#ifdef FRENCH_VERSION
#include "language\\french\\texts.h"
#define LANG_DEFINED
#endif // FRENCH_VERSION

#ifdef HUNGARIAN_VERSION
#include "language\\hungarian\\texts.h"
#define LANG_DEFINED
#endif // HUNGARIAN_VERSION

#ifdef SPANISH_VERSION
#include "language\\spanish\\texts.h"
#define LANG_DEFINED
#endif // SPANISH_VERSION

#ifdef ROMANIAN_VERSION
#include "language\\romanian\\texts.h"
#define LANG_DEFINED
#endif // ROMANIAN_VERSION

#ifdef RUSSIAN_VERSION
#include "language\\russian\\texts.h"
#define LANG_DEFINED
#endif // RUSSIAN_VERSION

#ifdef CHINESESIMPL_VERSION
#include "language\\chinesesimplified\\texts.h"
#define LANG_DEFINED
#endif // CHINESESIMPL_VERSION

#ifndef LANG_DEFINED
#error Invalid or no language defined.
#endif // LANG_DEFINED

#ifdef MAKE_ENUM_LIST

    LAST_STRING
} CStringIndex;

#undef MAKE_ENUM_LIST
#endif /* MAKE_ENUM_LIST */

#undef STRING

#ifndef __STRIGS_H
#define __STRIGS_H

extern const char* const StringTable[];

#endif //__STRIGS_H
