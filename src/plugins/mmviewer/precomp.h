// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <tchar.h>
#include <windows.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <limits.h>
#include <stdio.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_arc.h"
#include "spl_gen.h"
#include "spl_menu.h"
#include "spl_view.h"
#include "spl_gui.h"
#include "spl_vers.h"
#include "dbg.h"
#include "winliblt.h"
#include "mhandles.h"
#include "arraylt.h"

#include <assert.h>

// zde si zvolte formaty jake chcete aby plugin podporoval
// retezce ve stringlistu ale zustanou vzdy vsechny (nedostatecna podpora preprocesoru od MS)

//#define _MP4_SUPPORT_ - zatim neimplementovano
#define _MPG_SUPPORT_
#define _MOD_SUPPORT_
#define _VQF_SUPPORT_
#define _WAV_SUPPORT_
#define _WMA_SUPPORT_
// FIXME - zatloukl jsem OGG support, protoze ogglib je priserny kolos (lib ma nove pres 2MB)
// a stara verze co jsme pouzivali nema x64 podporu; reseni je prejit na TagLib projekt
// http://taglib.github.com/ ktery je malicky a ktery nam umozni podporovat i dalsi formaty
// chce to prozkoumat jeho MPL licenci, ale zrejme je volnejsi nez LGPL a knihovna by sla zakompilovat
#if ((_MSC_VER < 1600) && !defined(_WIN64)) // pouze VC2008 a x87
//#define _OGG_SUPPORT_
#endif
