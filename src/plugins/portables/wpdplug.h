// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdplug.h
	Salamander plugin interface.
*/

#pragma once

#include "fx.h"

class CWpdPluginInterface : public CFxPluginInterface
{
private:
    static const TCHAR c_szEnglishName[];

public:
    CWpdPluginInterface()
    {
    }

    virtual ~CWpdPluginInterface()
    {
    }

protected:
    /* CFxPluginInterface Overrides */

    virtual CFxPluginInterfaceForFS* WINAPI CreateInterfaceForFS() override;

public:
    /* CFxPluginInterface Overrides */

    virtual DWORD WINAPI GetSupportedFunctions() const override;

    virtual void WINAPI GetPluginEnglishName(CFxString& name) const override;

    virtual bool WINAPI NeedsWinLib() const override;

    /* CPluginInterfaceAbstract Overrides */

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) override;

    virtual BOOL WINAPI Release(HWND parent, BOOL force) override;
};
