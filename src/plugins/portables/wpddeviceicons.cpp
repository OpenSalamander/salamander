// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfsdeviceicons.cpp
	Device icons.
*/

#include "precomp.h"
#include "wpddeviceicons.h"
#include "globals.h"
#include "fx.h"
#include "wpd.rh"

CWpdDeviceIcons::CWpdDeviceIcons()
{
    memset(m_himl, 0, sizeof(m_himl));
}

void WINAPI CWpdDeviceIcons::Initialize()
{
    EnsureImageLists();
}

void WINAPI CWpdDeviceIcons::Destroy()
{
    for (int index = 0; index < IMAGELIST_COUNT; index++)
    {
        HIMAGELIST himl = m_himl[index];
        if (himl != nullptr)
        {
            BOOL ok = ImageList_Destroy(himl);
            _ASSERTE(ok);
            m_himl[index] = nullptr;
        }
    }
}

HIMAGELIST WINAPI CWpdDeviceIcons::CreateImageList(int size)
{
    BOOL ok;
    HIMAGELIST himl;
    int pixelSize = FxSalIconSizeToPixelSize(size);
    _ASSERTE(pixelSize > 0);

    auto iconList = SalamanderGUI->CreateIconList();
    _ASSERTE(iconList != nullptr);
    _ASSERTE(size > 0 && size <= IMAGELIST_COUNT);
    ok = iconList->CreateFromPNG(FxGetModuleInstance(), MAKEINTRESOURCE(IDR_DEVICEICONS16 + size - 1), pixelSize);
    _ASSERTE(ok);
    himl = iconList->GetImageList();
    _ASSERTE(himl != nullptr);
    ok = SalamanderGUI->DestroyIconList(iconList);
    _ASSERTE(ok);

    return himl;
}

int WINAPI CWpdDeviceIcons::AddDeviceIcon(PCWSTR devicePnpId, HICON hico16, HICON hico32, HICON hico48)
{
    _ASSERTE(devicePnpId != nullptr && *devicePnpId != L'\0');
    _ASSERTE(hico16 != nullptr);
    _ASSERTE(hico32 != nullptr);
    _ASSERTE(hico48 != nullptr);

    m_deviceIconsLock.Enter();

    int deviceIconInfoIndex = FindDeviceIconInfo(devicePnpId);
    int indexInImageList;
    if (deviceIconInfoIndex < 0)
    {
        // Icons for specified PnP ID don't exist.
        EnsureImageLists();
        indexInImageList = -1;
    }
    else
    {
        indexInImageList = m_deviceIcons[deviceIconInfoIndex].m_iconIndex;
    }

    int index16 = ImageList_ReplaceIcon(m_himl[0], indexInImageList, hico16);
    _ASSERTE(index16 >= 0);
    int index32 = ImageList_ReplaceIcon(m_himl[1], indexInImageList, hico32);
    _ASSERTE(index32 >= 0);
    int index48 = ImageList_ReplaceIcon(m_himl[2], indexInImageList, hico48);
    _ASSERTE(index48 >= 0);

    // Indices in all image lists must be equal.
    _ASSERTE(index16 == index32 && index32 == index48 && index16 == index48);

    // If replacing existing icon, the index must be the same.
    _ASSERTE(index16 == indexInImageList || indexInImageList == -1);

    if (deviceIconInfoIndex < 0)
    {
        DeviceIconInfo deviceIconInfo(devicePnpId, index16);
        m_deviceIcons.Add(deviceIconInfo);
    }

    m_deviceIconsLock.Leave();

    return index16;
}

int WINAPI CWpdDeviceIcons::GetDeviceIconIndex(PCWSTR devicePnpId)
{
    _ASSERTE(devicePnpId != nullptr && *devicePnpId != L'\0');

    int iconIndex;

    m_deviceIconsLock.Enter();

    int deviceIconInfoIndex = FindDeviceIconInfo(devicePnpId);
    if (deviceIconInfoIndex < 0)
    {
        iconIndex = -1;
    }
    else
    {
        iconIndex = m_deviceIcons[deviceIconInfoIndex].m_iconIndex;
    }

    m_deviceIconsLock.Leave();

    return iconIndex;
}

int WINAPI CWpdDeviceIcons::FindDeviceIconInfo(PCWSTR devicePnpId)
{
    for (int i = 0; i < m_deviceIcons.GetSize(); i++)
    {
        if (m_deviceIcons[i].m_pnpId.CompareNoCase(devicePnpId) == 0)
        {
            return i;
        }
    }

    return -1;
}

void WINAPI CWpdDeviceIcons::SetDeviceIcon(PCWSTR devicePnpId, int iconIndex)
{
    _ASSERTE(devicePnpId != nullptr && *devicePnpId != L'\0');

    m_deviceIconsLock.Enter();

    int deviceIconInfoIndex = FindDeviceIconInfo(devicePnpId);
    if (deviceIconInfoIndex >= 0)
    {
        m_deviceIcons[deviceIconInfoIndex].m_iconIndex = iconIndex;
    }
    else
    {
        DeviceIconInfo deviceIconInfo(devicePnpId, iconIndex);
        m_deviceIcons.Add(deviceIconInfo);
    }

    m_deviceIconsLock.Leave();
}

HIMAGELIST WINAPI CWpdDeviceIcons::EnsureImageList(int size)
{
    int index = size - 1;
    _ASSERTE(index >= 0 && index < IMAGELIST_COUNT);

    HIMAGELIST himl = m_himl[index];
    if (himl == nullptr)
    {
        himl = CreateImageList(size);
        _ASSERTE(himl != nullptr);
        m_himl[index] = himl;
    }

    return himl;
}

void WINAPI CWpdDeviceIcons::EnsureImageLists()
{
    EnsureImageList(SALICONSIZE_16);
    EnsureImageList(SALICONSIZE_32);
    EnsureImageList(SALICONSIZE_48);
}
