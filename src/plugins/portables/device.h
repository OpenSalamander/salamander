// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	device.h
	Represents WPD device.
*/

#pragma once

#include "fx.h"

class CWpdDevice : public CFxRefCounted
{
private:
    static bool s_bFTMFailed;

    static ATL::CCriticalSection s_iconReaderLock;

    PCWSTR m_pwzPnpId;

    CFxString m_strName;
    CFxString m_strFriendlyName;
    CFxString m_strManufacturer;
    CFxString m_strModel;

    /// WPD_DEVICE_TYPE_xxx.
    ULONG m_uDeviceType;

    ATL::CComPtr<IPicture> m_icons[3];

    enum FLAGS
    {
        FLAGS_OpenFailed = 1,
    };

    UINT m_uFlags;

    volatile LONG m_bHasIconResource;

    bool m_bIsHidden;

    unsigned m_cOpen;

    IPortableDevice* m_pDevice;

    IPortableDeviceContent* m_pContent;

    IPortableDeviceProperties* m_pProperties;

private:
    HRESULT WINAPI Open(_Out_ IPortableDevice** device, _In_opt_ DWORD access = GENERIC_READ | GENERIC_WRITE);

    HRESULT WINAPI LoadIconResourceLocked();

protected:
    virtual void WINAPI FinalRelease() override;

    friend class CWpdDeviceList;

    void WINAPI Update(IPortableDeviceManager* devMgr);

public:
    /// \param pnpId PnP identifier of the device. The string is allocated
    ///        on the COM heap and must be released when no longer needed.
    CWpdDevice(PCWSTR pnpId);

    virtual ~CWpdDevice();

    PCWSTR WINAPI GetPnpId() const
    {
        return m_pwzPnpId;
    }

    void WINAPI GetName(CFxString& name) const
    {
        name = m_strName;
    }

    void WINAPI GetFriendlyName(CFxString& name) const
    {
        name = m_strFriendlyName;
    }

    void WINAPI GetManufacturer(CFxString& manufacturer) const
    {
        manufacturer = m_strManufacturer;
    }

    void WINAPI GetModel(CFxString& model) const
    {
        model = m_strModel;
    }

    ULONG WINAPI GetDeviceType() const
    {
        return m_uDeviceType;
    }

    HRESULT WINAPI HasIconResource() const
    {
        LONG hasIconResource = m_bHasIconResource;
        if (hasIconResource == -1)
        {
            return E_PENDING;
        }

        return (m_icons[0] != nullptr) ? S_OK : S_FALSE;
    }

    HRESULT WINAPI LoadIconResource();

    HICON WINAPI GetIcon(int size);

    bool WINAPI IsHidden() const
    {
        return m_bIsHidden;
    }

    HRESULT WINAPI Open(_In_opt_ DWORD access = GENERIC_READ | GENERIC_WRITE);

    void WINAPI Close();

    IPortableDevice* WINAPI GetDeviceNoAddRef();

    IPortableDeviceContent* WINAPI GetContentNoAddRef();

    IPortableDeviceProperties* WINAPI GetPropertiesNoAddRef();
};

class CWpdDeviceList
{
private:
    bool m_bForceUpdate;

    CWpdDevice** m_apDevices;

    unsigned m_cDevices;

    void WINAPI UpdateList(PWSTR* pnpIds, unsigned count, IPortableDeviceManager* devMgr);

    void WINAPI DeleteList();

public:
    CWpdDeviceList();

    ~CWpdDeviceList();

    HRESULT WINAPI Update();

    void WINAPI SetForceUpdate()
    {
        m_bForceUpdate = true;
    }

    unsigned WINAPI GetCount() const
    {
        // Update must be called first.
        _ASSERTE(m_apDevices != nullptr);
        return m_cDevices;
    }

    CWpdDevice* WINAPI GetDeviceNoAddRef(unsigned index)
    {
        // Update must be called first.
        _ASSERTE(m_apDevices != nullptr);
        _ASSERTE(index < m_cDevices);
        auto device = m_apDevices[index];
        return device;
    }

    CWpdDevice* WINAPI GetDevice(unsigned index)
    {
        auto device = GetDeviceNoAddRef(index);
        device->AddRef();
        return device;
    }
};
