// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdplugfs.h
	Salamander plugin interface for file system.
*/

#pragma once

#include "fx.h"
#include "fxfs.h"
#include "wpdfs.h"

class CWpdPluginInterfaceForFS : public TFxPluginInterfaceForFS<CWpdFS>
{
public:
    CWpdPluginInterfaceForFS(CFxPluginInterface& owner)
        : TFxPluginInterfaceForFS(owner)
    {
    }

    virtual ~CWpdPluginInterfaceForFS()
    {
    }
};
