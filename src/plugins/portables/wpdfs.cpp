// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfs.cpp
	Salamander file system.
*/

#include "precomp.h"
#include "fxfs.h"
#include "wpdfs.h"
#include "wpdfsdevicelevel.h"
#include "wpdfscontentlevel.h"
#include "device.h"
#include "config.h"

////////////////////////////////////////////////////////////////////////////////
// CWpdFS

extern CWpdDeviceList g_oDeviceList;

PCSTR CWpdFS::SUGGESTED_NAME = SuggestedFSName;

CWpdFS::CWpdFS(CFxPluginInterfaceForFS& owner)
    : TFxPluginFSInterface(owner)
{
}

HRESULT WINAPI CWpdFS::GetChildEnumerator(
    _Out_ CFxItemEnumerator*& enumerator,
    CFxItem* parentItem,
    int level,
    bool forceRefresh)
{
    HRESULT hr;

    if (forceRefresh)
    {
        g_oDeviceList.SetForceUpdate();
    }

    if (level == 0)
    {
        auto* deviceEnumerator = new CWpdDeviceEnumerator();
        hr = deviceEnumerator->Initialize();
        if (SUCCEEDED(hr))
        {
            enumerator = deviceEnumerator;
        }
        else
        {
            deviceEnumerator->Release();
        }
    }
    else
    {
        CWpdDevice* device;
        PCWSTR parentObjectId;

        if (static_cast<CWpdItem*>(parentItem)->IsDevice())
        {
            auto* deviceItem = static_cast<CWpdDeviceItem*>(parentItem);
            device = deviceItem->GetDeviceNoAddRef();
            parentObjectId = WPD_DEVICE_OBJECT_ID;
        }
        else
        {
            auto* contentItem = static_cast<CWpdBaseContentItem*>(parentItem);
            device = contentItem->GetDeviceNoAddRef();
            parentObjectId = contentItem->GetObjectId();
        }

        CWpdBaseContentEnumerator* contentEnumerator;
        if (level == 1)
        {
            contentEnumerator = new CWpdStorageEnumerator();
        }
        else
        {
            contentEnumerator = new CWpdContentEnumerator();
        }

        hr = contentEnumerator->Initialize(device, parentObjectId);
        if (SUCCEEDED(hr))
        {
            enumerator = contentEnumerator;
        }
        else
        {
            contentEnumerator->Release();
        }
    }

    return hr;
}

CFxPluginDataInterface* WINAPI CWpdFS::CreatePluginData(CFxItemEnumerator* enumerator)
{
    // Redirect the call to the enumerator, since the enumerator knows what
    // data it needs.
    auto wpdEnum = static_cast<CWpdEnumerator*>(enumerator);
    return wpdEnum->CreatePluginData(*this);
}
