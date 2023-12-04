// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfsdevicelevel.cpp
	Device level enumerator.
*/

#include "precomp.h"
#include "fxfs.h"
#include "wpdfs.h"
#include "wpdfsdevicelevel.h"
#include "wpddeviceicons.h"
#include "device.h"
#include "lang/lang.rh"

extern CWpdDeviceIcons g_oDeviceIcons;
extern CWpdDeviceList g_oDeviceList;

////////////////////////////////////////////////////////////////////////////////
// CWpdDeviceItem

CWpdDeviceItem::CWpdDeviceItem(CWpdDevice* device)
    : CWpdItem(WPDFS_LEVEL_DEVICE)
{
    _ASSERTE(device != nullptr);
    m_pDevice = device;

    m_iSimpleIcon = -1;
}

void WINAPI CWpdDeviceItem::FinalRelease()
{
    m_pDevice->Release();
}

void WINAPI CWpdDeviceItem::GetName(CFxString& name)
{
    m_pDevice->GetFriendlyName(name);
    if (name.IsEmpty())
    {
        name = m_pDevice->GetPnpId();
    }
}

int WINAPI CWpdDeviceItem::GetSimpleIconIndex()
{
    int iconIndex = GetAndUpdateCachedSimpleIcon();
    if (iconIndex < 0)
    {
        // We don't have specific device icon, use generic one.
        iconIndex = GetSimpleDeviceIconByDeviceType();
    }

    return iconIndex;
}

bool WINAPI CWpdDeviceItem::HasSlowIcon()
{
    // If we have simple icon (either cached device specific icon or
    // a generic one) then there is no need for slow icon.
    return GetAndUpdateCachedSimpleIcon() < 0;
}

int WINAPI CWpdDeviceItem::GetAndUpdateCachedSimpleIcon()
{
    if (m_iSimpleIcon >= 0)
    {
        // We have cached simple icon index.
        return m_iSimpleIcon;
    }

    int iconIndex;

    iconIndex = g_oDeviceIcons.GetDeviceIconIndex(m_pDevice->GetPnpId());
    if (iconIndex > 0)
    {
        // We have device icon cached in the global device icon list.
        m_iSimpleIcon = iconIndex;
        return iconIndex;
    }
    else if (iconIndex == -2)
    {
        // The device is known to have no specific icon.
        iconIndex = GetSimpleDeviceIconByDeviceType();
        m_iSimpleIcon = iconIndex;
        return iconIndex;
    }

    return -1;
}

HRESULT WINAPI CWpdDeviceItem::GetSlowIcon(int size, _Out_ HICON& hico)
{
    _ASSERTE(m_iSimpleIcon < 0);

    HRESULT hr;
    hr = m_pDevice->LoadIconResource();
    if (FAILED(hr))
    {
        // Device has not provided its own icon or the download
        // failed, use a generic icon from now on.
        m_iSimpleIcon = GetSimpleDeviceIconByDeviceType();
        g_oDeviceIcons.AddDeviceNoIcon(m_pDevice->GetPnpId());
        return hr;
    }

    // Cache the device icon in the image list and store the index in item.
    m_iSimpleIcon = g_oDeviceIcons.AddDeviceIcon(
        m_pDevice->GetPnpId(),
        m_pDevice->GetIcon(16),
        m_pDevice->GetIcon(32),
        m_pDevice->GetIcon(48));
    _ASSERTE(m_iSimpleIcon >= 0);

    hico = m_pDevice->GetIcon(size);

    return S_OK;
}

int WINAPI CWpdDeviceItem::GetSimpleDeviceIconByDeviceType(ULONG deviceType)
{
    switch (deviceType)
    {
    case WPD_DEVICE_TYPE_CAMERA:
        return CWpdDeviceIcons::Camera;
    case WPD_DEVICE_TYPE_MEDIA_PLAYER:
        return CWpdDeviceIcons::MediaPlayer;
    case WPD_DEVICE_TYPE_PHONE:
        return CWpdDeviceIcons::Phone;
    case WPD_DEVICE_TYPE_VIDEO:
        return CWpdDeviceIcons::Video;
    case WPD_DEVICE_TYPE_PERSONAL_INFORMATION_MANAGER:
        return CWpdDeviceIcons::Pim;
    case WPD_DEVICE_TYPE_AUDIO_RECORDER:
        return CWpdDeviceIcons::AudioRecorder;
    default:
        // Unknown device type, assert and fall through
        // to the generic.
        _ASSERTE(0);
    case WPD_DEVICE_TYPE_GENERIC:
        return CWpdDeviceIcons::Generic;
    }
}

DWORD WINAPI CWpdDeviceItem::GetAttributes()
{
    // Our device items behave like directories.
    return FILE_ATTRIBUTE_DIRECTORY;
}

////////////////////////////////////////////////////////////////////////////////
// CWpdDeviceEnumerator

CWpdDeviceEnumerator::CWpdDeviceEnumerator()
    : CWpdEnumerator(WPDFS_LEVEL_DEVICE)
{
    Reset();
}

CWpdDeviceEnumerator::~CWpdDeviceEnumerator()
{
}

HRESULT WINAPI CWpdDeviceEnumerator::Initialize()
{
    HRESULT hr = g_oDeviceList.Update();
    _ASSERTE(SUCCEEDED(hr));
    return hr;
}

HRESULT WINAPI CWpdDeviceEnumerator::MoveNext()
{
    unsigned count = g_oDeviceList.GetCount();

    ++m_index;
    _ASSERTE(m_index >= 0);

    while (static_cast<unsigned>(m_index) < count &&
           ShouldSkip(g_oDeviceList.GetDeviceNoAddRef(m_index)))
    {
        ++m_index;
    }

    return static_cast<unsigned>(m_index) < count ? S_OK : S_FALSE;
}

CFxItem* WINAPI CWpdDeviceEnumerator::GetCurrent() const
{
    _ASSERTE(m_index >= 0);
    return new CWpdDeviceItem(g_oDeviceList.GetDevice(m_index));
}

void WINAPI CWpdDeviceEnumerator::Reset()
{
    m_index = -1;
}

DWORD WINAPI CWpdDeviceEnumerator::GetValidData() const
{
    // We do not have any extra data but the name for the device items.
    return VALID_DATA_NONE;
}

CWpdPluginDataInterface* WINAPI CWpdDeviceEnumerator::CreatePluginData(CFxPluginFSInterface& owner)
{
    return new CWpdDevicePluginDataInterface(owner);
}

bool CWpdDeviceEnumerator::ShouldSkip(CWpdDevice* device)
{
    return device->IsHidden();
}

////////////////////////////////////////////////////////////////////////////////
// CWpdDevicePluginDataInterface

CWpdDevicePluginDataInterface::CWpdDevicePluginDataInterface(CFxPluginFSInterface& owner)
    : CWpdPluginDataInterface(owner, WPDFS_LEVEL_DEVICE)
{
}

HIMAGELIST WINAPI CWpdDevicePluginDataInterface::GetSimplePluginIcons(int iconSize)
{
    return g_oDeviceIcons.GetSimpleDeviceIcons(iconSize);
}

BOOL WINAPI CWpdDevicePluginDataInterface::GetInfoLineContent(
    int panel,
    const CFileData* file,
    BOOL isDir,
    int selectedFiles,
    int selectedDirs,
    BOOL displaySize,
    const CQuadWord& selectedSize,
    char* buffer,
    DWORD* hotTexts,
    int& hotTextsCount)
{
    // On the device level we have no 'files'.
    _ASSERTE(selectedFiles == 0);

    hotTextsCount = 0;

    if (selectedDirs > 0)
    {
        CQuadWord selectedDirsQWord;
        selectedDirsQWord.Set(selectedDirs, 0);
        TCHAR buf[200];
        SalamanderGeneral->ExpandPluralString(
            buf,
            _countof(buf),
            SalamanderGeneral->LoadStr(FxGetLangInstance(), IDS_SELECTEDDEVICEFMT),
            1,
            &selectedDirsQWord);
        StringCchPrintf(buffer, 1000, buf, selectedDirs);
        return TRUE;
    }
    else if (file != nullptr)
    {
        *buffer = TEXT('\0');
        CFxString s;
        auto* item = reinterpret_cast<CWpdDeviceItem*>(file->PluginData);
        auto* device = item->GetDeviceNoAddRef();

        device->GetManufacturer(s);
        AppendInfoLineContentPart(s, buffer, hotTexts, hotTextsCount);

        device->GetModel(s);
        if (s.IsEmpty())
        {
            device->GetName(s);
        }

        AppendInfoLineContentPart(s, buffer, hotTexts, hotTextsCount);
        return TRUE;
    }
    else
    {
        ::LoadString(FxGetLangInstance(), IDS_NODEVICEINFOLINE, buffer, 1000);
        return TRUE;
    }
}
