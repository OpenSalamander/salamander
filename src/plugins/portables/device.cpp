// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	device.cpp
	Represents WPD device.
*/

#include "precomp.h"
#include "device.h"
#include "versinfo.rh2"
#include "wpdhelpers.h"

using namespace ATL;

#define _WIDETEXT(s) L##s
#define WIDETEXT(s) _WIDETEXT(s)

////////////////////////////////////////////////////////////////////////////////
// CWpdDevice

bool CWpdDevice::s_bFTMFailed;
CCriticalSection CWpdDevice::s_iconReaderLock;

CWpdDevice::CWpdDevice(PCWSTR pnpId)
{
    _ASSERTE(pnpId != nullptr);
    m_pwzPnpId = pnpId;

    m_uFlags = 0U;
    m_uDeviceType = static_cast<ULONG>(-1);
    m_bHasIconResource = -1;
    m_bIsHidden = false;

    m_cOpen = 0;
    m_pDevice = nullptr;
    m_pContent = nullptr;
    m_pProperties = nullptr;
}

CWpdDevice::~CWpdDevice()
{
    ::CoTaskMemFree(const_cast<PWSTR>(m_pwzPnpId));
}

void WINAPI CWpdDevice::FinalRelease()
{
    // Device must be closed whend releasing the object.
    _ASSERTE(m_cOpen == 0);
}

void WINAPI CWpdDevice::Update(IPortableDeviceManager* devMgr)
{
    HRESULT hr;
    DWORD len;
    WCHAR wz[500];

    len = _countof(wz) - 1;
    hr = devMgr->GetDeviceFriendlyName(m_pwzPnpId, wz, &len);
    if (SUCCEEDED(hr))
    {
        m_strFriendlyName = wz;
    }
    else
    {
        m_strFriendlyName.Empty();
    }

    hr = Open(GENERIC_READ);
    if (!SUCCEEDED(hr))
    {
        return;
    }

    CComPtr<IPortableDeviceKeyCollection> keys;
    hr = keys.CoCreateInstance(CLSID_PortableDeviceKeyCollection);
    _ASSERTE(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        hr = keys->Add(WPD_OBJECT_NAME);
        _ASSERTE(SUCCEEDED(hr));

        hr = keys->Add(WPD_DEVICE_TYPE);
        _ASSERTE(SUCCEEDED(hr));

        hr = keys->Add(WPD_DEVICE_FRIENDLY_NAME);
        _ASSERTE(SUCCEEDED(hr));

        hr = keys->Add(WPD_DEVICE_MANUFACTURER);
        _ASSERTE(SUCCEEDED(hr));

        hr = keys->Add(WPD_DEVICE_MODEL);
        _ASSERTE(SUCCEEDED(hr));

        hr = keys->Add(WPD_OBJECT_ISHIDDEN);
        _ASSERTE(SUCCEEDED(hr));

        CComPtr<IPortableDeviceValues> values;
        hr = GetPropertiesNoAddRef()->GetValues(WPD_DEVICE_OBJECT_ID, keys, &values);
        _ASSERTE(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            // Only use WPD_DEVICE_FRIENDLY_NAME if IPortableDeviceManager::GetDeviceFriendlyName
            // failed.
            if (m_strFriendlyName.IsEmpty())
            {
                WpdGetStringValue(values, WPD_DEVICE_FRIENDLY_NAME, m_strFriendlyName);
            }
        }

        WpdGetStringValue(values, WPD_DEVICE_MANUFACTURER, m_strManufacturer);
        WpdGetStringValue(values, WPD_DEVICE_MODEL, m_strModel);
        WpdGetStringValue(values, WPD_OBJECT_NAME, m_strName);

        ULONG ui4;
        hr = values->GetUnsignedIntegerValue(WPD_DEVICE_TYPE, &ui4);
        if (SUCCEEDED(hr))
        {
            m_uDeviceType = ui4;
        }

        BOOL b;
        hr = values->GetBoolValue(WPD_OBJECT_ISHIDDEN, &b);
        if (SUCCEEDED(hr))
        {
            m_bIsHidden = !!b;
        }
    }

    Close();
}

HRESULT WINAPI CWpdDevice::Open(_Out_ IPortableDevice** device_, _In_opt_ DWORD access)
{
    HRESULT hr, hr2;
    CComPtr<IPortableDeviceValues> clientInfo;
    CComPtr<IPortableDevice> device;

    if (!s_bFTMFailed)
    {
        hr = device.CoCreateInstance(CLSID_PortableDeviceFTM);
        if (FAILED(hr))
        {
            s_bFTMFailed = true;
        }
    }

    if (device == nullptr)
    {
        hr = device.CoCreateInstance(CLSID_PortableDevice);
        if (FAILED(hr))
        {
            return hr;
        }
    }

    hr = clientInfo.CoCreateInstance(CLSID_PortableDeviceValues);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    // If we fail to set any of the properties below it is OK. Failing to
    // set a property in the client information isn't a fatal error.
    hr2 = clientInfo->SetStringValue(WPD_CLIENT_NAME, WIDETEXT(VERSINFO_DESCRIPTION));
    _ASSERTE(SUCCEEDED(hr2));
    hr2 = clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, VERSINFO_MAJOR);
    _ASSERTE(SUCCEEDED(hr2));
    hr2 = clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, VERSINFO_MINORA * 10 + VERSINFO_MINORB);
    _ASSERTE(SUCCEEDED(hr2));
    hr2 = clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, 0U);
    _ASSERTE(SUCCEEDED(hr2));

    hr = clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS, access);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    // Some device drivers need to impersonate the caller in order to function
    // correctly. Since our application does not need to restrict its identity,
    // specify SECURITY_IMPERSONATION so that we work with all devices.
    hr2 = clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
    _ASSERTE(SUCCEEDED(hr2));

    hr = device->Open(m_pwzPnpId, clientInfo);
    if (SUCCEEDED(hr))
    {
        *device_ = device.Detach();
    }

    return hr;
}

HRESULT WINAPI CWpdDevice::LoadIconResource()
{
    HRESULT hr;

    s_iconReaderLock.Enter();

    hr = LoadIconResourceLocked();
    ::InterlockedExchange(&m_bHasIconResource, SUCCEEDED(hr) ? 1L : 0L);

    s_iconReaderLock.Leave();

    return hr;
}

HRESULT WINAPI CWpdDevice::LoadIconResourceLocked()
{
    if (m_icons[0] != nullptr)
    {
        return S_OK;
    }

    HRESULT hr;
    CComPtr<IPortableDevice> device;

    hr = Open(&device, GENERIC_READ);
    if (FAILED(hr))
    {
        return hr;
    }

    CComPtr<IPortableDeviceContent> content;
    hr = device->Content(&content);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    CComPtr<IPortableDeviceResources> resources;
    hr = content->Transfer(&resources);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    CComPtr<IStream> stream;
    DWORD optimalBufferSize = 4000;
    hr = resources->GetStream(WPD_DEVICE_OBJECT_ID, WPD_RESOURCE_ICON, STGM_READ, &optimalBufferSize, &stream);
    if (FAILED(hr))
    {
        return hr;
    }

    for (int i = 0; i < _countof(m_icons); i++)
    {
        CComPtr<IPicture> picture;
        static const int sizes[3] = {16, 32, 48};

        hr = OleLoadPictureEx(stream, 0L, FALSE, __uuidof(IPicture),
                              sizes[i], sizes[i], LP_COLOR, reinterpret_cast<void**>(&picture));
        if (FAILED(hr))
        {
            return hr;
        }

        m_icons[i] = picture;

        LARGE_INTEGER moveTo = {
            0,
        };
        ULARGE_INTEGER newPos;
        hr = stream->Seek(moveTo, STREAM_SEEK_SET, &newPos);
        _ASSERTE(SUCCEEDED(hr));
        if (FAILED(hr))
        {
            break;
        }
    }

    return hr;
}

HICON WINAPI CWpdDevice::GetIcon(int size)
{
    HRESULT hr;
    int index;

    switch (size)
    {
    case 16:
        index = 0;
        break;
    case 32:
        index = 1;
        break;
    case 48:
        index = 2;
        break;
    default:
        _ASSERTE(0);
        return nullptr;
    }

    OLE_HANDLE hico;
    hr = m_icons[index]->get_Handle(&hico);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return nullptr;
    }

    return reinterpret_cast<HICON>(static_cast<UINT_PTR>(hico));
}

HRESULT WINAPI CWpdDevice::Open(DWORD access)
{
    UNREFERENCED_PARAMETER(access); // Unused, for now.

    HRESULT hr = S_OK;

    if (m_cOpen == 0)
    {
        hr = Open(&m_pDevice, GENERIC_READ | GENERIC_WRITE);
    }

    if (SUCCEEDED(hr))
    {
        ++m_cOpen;
    }

    return hr;
}

void WINAPI CWpdDevice::Close()
{
    _ASSERTE(m_cOpen > 0);

    --m_cOpen;
    if (m_cOpen == 0)
    {
        if (m_pContent != nullptr)
        {
            m_pContent->Release();
            m_pContent = nullptr;
        }

        if (m_pProperties != nullptr)
        {
            m_pProperties->Release();
            m_pProperties = nullptr;
        }

        m_pDevice->Release();
        m_pDevice = nullptr;
    }
}

IPortableDevice* WINAPI CWpdDevice::GetDeviceNoAddRef()
{
    _ASSERTE(m_cOpen > 0);
    _ASSERTE(m_pDevice != nullptr);
    return m_pDevice;
}

IPortableDeviceContent* WINAPI CWpdDevice::GetContentNoAddRef()
{
    _ASSERTE(m_cOpen > 0);
    _ASSERTE(m_pDevice != nullptr);

    if (m_pContent == nullptr)
    {
        HRESULT hr = m_pDevice->Content(&m_pContent);
        _ASSERTE(SUCCEEDED(hr) && m_pContent != nullptr);
    }

    return m_pContent;
}

IPortableDeviceProperties* WINAPI CWpdDevice::GetPropertiesNoAddRef()
{
    _ASSERTE(m_cOpen > 0);
    _ASSERTE(m_pDevice != nullptr);

    if (m_pProperties == nullptr)
    {
        auto content = GetContentNoAddRef();
        HRESULT hr = content->Properties(&m_pProperties);
        _ASSERTE(SUCCEEDED(hr) && m_pProperties != nullptr);
    }

    return m_pProperties;
}

////////////////////////////////////////////////////////////////////////////////
// CWpdDeviceList

CWpdDeviceList::CWpdDeviceList()
    : m_bForceUpdate(true),
      m_apDevices(nullptr),
      m_cDevices(0U)
{
}

CWpdDeviceList::~CWpdDeviceList()
{
    DeleteList();
}

HRESULT WINAPI CWpdDeviceList::Update()
{
    HRESULT hr;
    DWORD count;

    if (!m_bForceUpdate)
    {
        _ASSERTE(m_apDevices != nullptr);
        return S_FALSE;
    }

    CComPtr<IPortableDeviceManager> devMgr;

    hr = devMgr.CoCreateInstance(CLSID_PortableDeviceManager);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    hr = devMgr->GetDevices(nullptr, &count);
    _ASSERTE(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    PWSTR* devicePnpIds = new PWSTR[count];
    hr = devMgr->GetDevices(devicePnpIds, &count);
    _ASSERTE(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        UpdateList(devicePnpIds, count, devMgr);
        m_bForceUpdate = false;
    }

    delete[] devicePnpIds;

    return hr;
}

void WINAPI CWpdDeviceList::UpdateList(PWSTR* pnpIds, unsigned count, IPortableDeviceManager* devMgr)
{
    DeleteList();

    CWpdDevice** devices = new CWpdDevice*[count];

    for (unsigned i = 0; i < count; i++)
    {
        devices[i] = new CWpdDevice(pnpIds[i]);
        devices[i]->Update(devMgr);
    }

    m_apDevices = devices;
    m_cDevices = count;
}

void WINAPI CWpdDeviceList::DeleteList()
{
    for (unsigned i = 0; i < m_cDevices; i++)
    {
        m_apDevices[i]->Release();
    }

    delete[] m_apDevices;
    m_apDevices = nullptr;
}
