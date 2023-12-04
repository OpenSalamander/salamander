// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfs.h
	Salamander file system.
*/

#pragma once

#include "fx.h"
#include "fxfs.h"

class CWpdFS final : public TFxPluginFSInterface<CWpdFS>
{
protected:
    /* CFxPluginFSInterface Overrides */

    virtual CFxPluginDataInterface* WINAPI CreatePluginData(CFxItemEnumerator* enumerator) override;

    virtual HRESULT WINAPI GetChildEnumerator(
        _Out_ CFxItemEnumerator*& enumerator,
        CFxItem* parentItem,
        int level,
        bool forceRefresh) override;

public:
    CWpdFS(CFxPluginInterfaceForFS& owner);

    enum _SupportedServices
    {
        SUPPORTED_SERVICES = 0U
    };

    static PCTSTR SUGGESTED_NAME;
};

typedef enum _WPDFS_LEVEL
{
    WPDFS_LEVEL_DEVICE = 1,
    WPDFS_LEVEL_STORAGE = 2,
    WPDFS_LEVEL_CONTENT = 3,
} WPDFS_LEVEL;

class CWpdType
{
protected:
    WPDFS_LEVEL m_level;

    CWpdType(WPDFS_LEVEL level)
        : m_level(level)
    {
    }

public:
    bool IsDevice() const
    {
        return m_level == WPDFS_LEVEL_DEVICE;
    }

    bool IsStorage() const
    {
        return m_level == WPDFS_LEVEL_STORAGE;
    }

    bool IsContent() const
    {
        return m_level == WPDFS_LEVEL_CONTENT;
    }
};

class CWpdItem : public CFxItem, public CWpdType
{
protected:
    CWpdItem(WPDFS_LEVEL level)
        : CWpdType(level)
    {
    }
};

class CWpdEnumerator : public CFxItemEnumerator, public CWpdType
{
protected:
    CWpdEnumerator(WPDFS_LEVEL level)
        : CWpdType(level)
    {
    }

public:
    virtual class CWpdPluginDataInterface* WINAPI CreatePluginData(CFxPluginFSInterface& owner) = 0;
};

class CWpdPluginDataInterface : public CFxPluginFSDataInterface, public CWpdType
{
protected:
    CWpdPluginDataInterface(CFxPluginFSInterface& owner, WPDFS_LEVEL level)
        : CFxPluginFSDataInterface(owner),
          CWpdType(level)
    {
    }
};
