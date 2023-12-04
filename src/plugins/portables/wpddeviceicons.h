// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfsdeviceicons.h
	Device icons.
*/

#pragma once

class CWpdDeviceIcons
{
public:
    enum IconIndex
    {
        // Those device types have their own generic icon.
        Camera = 0,
        MediaPlayer = 1,
        Phone = 2,
        Video = 3,

        // We use Phone icon as the main plugin/FS icon. Use it for
        // unknown devices.
        Generic = Phone,

        // Use generic icon for those rare device types.
        Pim = Generic,
        AudioRecorder = Generic,

        Storage = 4,
    };

private:
    enum
    {
        IMAGELIST_COUNT = 3
    };

    HIMAGELIST m_himl[IMAGELIST_COUNT];

    struct DeviceIconInfo
    {
        ATL::CStringW m_pnpId;
        int m_iconIndex;

        DeviceIconInfo(PCWSTR pnpId, int iconIndex)
            : m_pnpId(pnpId), m_iconIndex(iconIndex)
        {
        }
    };

    ATL::CSimpleArray<DeviceIconInfo> m_deviceIcons;
    ATL::CCriticalSection m_deviceIconsLock;

    HIMAGELIST WINAPI CreateImageList(int size);

    int WINAPI FindDeviceIconInfo(PCWSTR devicePnpId);

    void WINAPI SetDeviceIcon(PCWSTR devicePnpId, int iconIndex);

    /// \param size SALICONSIZE_nn
    HIMAGELIST WINAPI EnsureImageList(int size);

    void WINAPI EnsureImageLists();

public:
    CWpdDeviceIcons();

    void WINAPI Initialize();

    void WINAPI Destroy();

    /// \param size SALICONSIZE_nn
    HIMAGELIST WINAPI GetSimpleDeviceIcons(int size)
    {
        int index = size - 1;
        _ASSERTE(index >= 0 && index < _countof(m_himl));
        HIMAGELIST himl = m_himl[index];
        _ASSERTE(himl != nullptr);
        return himl;
    }

    /// Adds device icon into the image list.
    /// \return Returns index of the device icon.
    int WINAPI AddDeviceIcon(PCWSTR devicePnpId, HICON hico16, HICON hico32, HICON hico48);

    void WINAPI AddDeviceNoIcon(PCWSTR devicePnpId)
    {
        SetDeviceIcon(devicePnpId, -2);
    }

    void WINAPI AddDeviceGenericIcon(PCWSTR devicePnpId, IconIndex icon)
    {
        SetDeviceIcon(devicePnpId, icon);
    }

    /// \return If the return value is >= 0 then the value is icon index of
    ///         the specified device in the image list.
    ///         If the return value is -1 then the icon for the specified
    ///         device is unknown.
    ///         If the return value is -2 then it is known that the device
    ///         does not have its own icon and the generic icon should be used
    ///         instead.
    int WINAPI GetDeviceIconIndex(PCWSTR devicePnpId);
};
