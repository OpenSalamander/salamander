// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Salamander Plugin Development Framework
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	fx_errcodes.h
	Defines framework error codes.
*/

// WARNING: cannot be replaced by "#pragma once" because it is included from .rc file and it seems resource compiler does not support "#pragma once"
#ifndef __FX_ERRCODES_H
#define __FX_ERRCODES_H

#define __FX_HRCODE_BASE 0x2000
#define _FX_HRCODE_DESTROY_ICON (__FX_HRCODE_BASE + 0)
#define _FX_HRCODE_PATH_IS_FILE (__FX_HRCODE_BASE + 1)
#define _FX_HRCODE_PATH_WAS_CUT (__FX_HRCODE_BASE + 2)

#define __FX_HRFACILITY 0x0100

#define __FX_MAKE_HRESULT(severity, code) MAKE_HRESULT(severity, __FX_HRFACILITY, code)

#ifndef RC_INVOKED

#ifdef __cplusplus
namespace Fx
{
#endif

    enum
    {
        FX_S_DESTROY_ICON = __FX_MAKE_HRESULT(SEVERITY_SUCCESS, _FX_HRCODE_DESTROY_ICON),
        FX_E_PATH_IS_FILE = __FX_MAKE_HRESULT(SEVERITY_ERROR, _FX_HRCODE_PATH_IS_FILE),
        FX_E_PATH_WAS_CUT = __FX_MAKE_HRESULT(SEVERITY_ERROR, _FX_HRCODE_PATH_WAS_CUT),
        FX_E_BAD_PATHNAME = __HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME),
        FX_E_PATH_NOT_FOUND = __HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND),
        FX_E_NO_DATA = __HRESULT_FROM_WIN32(ERROR_NO_DATA),
    };

#ifdef __cplusplus
}; // namespace Fx
#endif

#endif // RC_INVOKED

#endif // __FX_ERRCODES_H
