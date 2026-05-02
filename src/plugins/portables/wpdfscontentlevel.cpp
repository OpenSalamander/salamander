// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfscontentlevel.cpp
	Content level enumerator.
*/

#include "precomp.h"
#include "fxfs.h"
#include "wpdfs.h"
#include "wpdfscontentlevel.h"
#include "wpdhelpers.h"
#include "wpddeviceicons.h"

extern CWpdDeviceIcons g_oDeviceIcons;

using namespace ATL;

////////////////////////////////////////////////////////////////////////////////
// CWpdBaseContentItem

CWpdBaseContentItem::CWpdBaseContentItem(
    WPDFS_LEVEL level,
    CWpdDevice* device,
    PCWSTR objectId,
    IPortableDeviceValues* values) : CWpdItem(WPDFS_LEVEL_CONTENT),
                                     m_device(device),
                                     m_objectId(objectId),
                                     m_flags(0U),
                                     m_attributes(0U)
{
    HRESULT hr;
    GUID guid;

    hr = WpdGetStringValue(values, WPD_OBJECT_ORIGINAL_FILE_NAME, m_strName);
    if (FAILED(hr) || m_strName.IsEmpty())
    {
        hr = WpdGetStringValue(values, WPD_OBJECT_NAME, m_strName);
        _ASSERTE(SUCCEEDED(hr));
        if (FAILED(hr))
        {
            TRACE_E("Failed to get WPD object name");
        }
    }

    hr = values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE, &guid);
    _ASSERTE(SUCCEEDED(hr));
    if (SUCCEEDED(hr) && IsFolder(guid))
    {
        m_attributes |= FILE_ATTRIBUTE_DIRECTORY;
    }
}

CWpdBaseContentItem::~CWpdBaseContentItem()
{
    ::CoTaskMemFree(const_cast<PWSTR>(m_objectId));
}

void WINAPI CWpdBaseContentItem::GetName(CFxString& name)
{
    name = m_strName;
}

DWORD WINAPI CWpdBaseContentItem::GetAttributes()
{
    return m_attributes;
}

bool WINAPI CWpdBaseContentItem::IsFolder(const GUID& contentType)
{
    return IsEqualGUID(contentType, WPD_CONTENT_TYPE_FOLDER) ||
           IsEqualGUID(contentType, WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT);
}

////////////////////////////////////////////////////////////////////////////////
// CWpdStorageItem

const PROPERTYKEY* const CWpdStorageItem::s_requiredKeys[] =
    {
        &WPD_OBJECT_ORIGINAL_FILE_NAME,
        &WPD_OBJECT_NAME,
        &WPD_OBJECT_CONTENT_TYPE,
        &WPD_FUNCTIONAL_OBJECT_CATEGORY,
};

CWpdStorageItem::CWpdStorageItem(
    CWpdDevice* device,
    PCWSTR objectId,
    IPortableDeviceValues* values) : CWpdBaseContentItem(WPDFS_LEVEL_STORAGE, device, objectId, values)
{
}

IPortableDeviceKeyCollection* WINAPI CWpdStorageItem::GetRequiredKeys()
{
    return WpdInitKeys(s_requiredKeys, _countof(s_requiredKeys));
}

int WINAPI CWpdStorageItem::GetSimpleIconIndex()
{
    return CWpdDeviceIcons::Storage;
}

////////////////////////////////////////////////////////////////////////////////
// CWpdContentItem

const PROPERTYKEY* const CWpdContentItem::s_requiredKeys[] =
    {
        &WPD_OBJECT_ORIGINAL_FILE_NAME,
        &WPD_OBJECT_NAME,
        &WPD_OBJECT_CONTENT_TYPE,
        &WPD_OBJECT_SIZE,
        &WPD_OBJECT_DATE_MODIFIED,
        &WPD_OBJECT_DATE_CREATED,
        &WPD_OBJECT_ISHIDDEN,
        &WPD_OBJECT_ISSYSTEM,
};

CWpdContentItem::CWpdContentItem(
    CWpdDevice* device,
    PCWSTR objectId,
    IPortableDeviceValues* values,
    FILETIME now) : CWpdBaseContentItem(WPDFS_LEVEL_CONTENT, device, objectId, values),
                    m_size(0ULL),
                    m_lastWrite(FILETIME_Nul)
{
    HRESULT hr;
    ULONGLONG ui8;
    BOOL b;

    hr = values->GetBoolValue(WPD_OBJECT_ISHIDDEN, &b);
    if (SUCCEEDED(hr) && b)
    {
        m_attributes |= FILE_ATTRIBUTE_HIDDEN;
    }

    hr = values->GetBoolValue(WPD_OBJECT_ISSYSTEM, &b);
    if (SUCCEEDED(hr) && b)
    {
        m_attributes |= FILE_ATTRIBUTE_SYSTEM;
    }

    hr = values->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE, &ui8);
    if (SUCCEEDED(hr))
    {
        m_flags |= FLAGS_HaveSize;
        m_size = ui8;
    }

    hr = WpdGetDateTimeValue(values, WPD_OBJECT_DATE_MODIFIED, m_lastWrite);
    if (FAILED(hr))
    {
        hr = WpdGetDateTimeValue(values, WPD_OBJECT_DATE_CREATED, m_lastWrite);
        if (FAILED(hr))
        {
            // If we could not obtain object's date, use the
            // current time instead.
            m_lastWrite = now;
        }
    }

    if (SUCCEEDED(hr))
    {
        m_flags |= FLAGS_HaveLastWrite;
    }
}

HRESULT WINAPI CWpdContentItem::GetSize(ULONGLONG& size)
{
    size = m_size;

    // Only consider FLAGS_HaveSize if CWpdContentEnumerator::GetValidData
    // includes VALID_DATA_PL_SIZE. Otherwise this method is not supposed
    // to fail.
    ////return (m_flags & FLAGS_HaveSize) ? S_OK : FX_E_NO_DATA;

    return S_OK;
}

HRESULT WINAPI CWpdContentItem::GetLastWriteTime(FILETIME& time)
{
    time = m_lastWrite;

    // Only consider FLAGS_HaveLastWrite if CWpdContentEnumerator::GetValidData
    // includes VALID_DATA_PL_DATE/TIME. Otherwise this method is not supposed
    // to fail.
    ////return (m_flags & FLAGS_HaveLastWrite) ? S_OK : FX_E_NO_DATA;

    return S_OK;
}

IPortableDeviceKeyCollection* WINAPI CWpdContentItem::GetRequiredKeys()
{
    return WpdInitKeys(s_requiredKeys, _countof(s_requiredKeys));
}

////////////////////////////////////////////////////////////////////////////////
// CWpdBaseContentEnumerator

CWpdBaseContentEnumerator::CWpdBaseContentEnumerator(WPDFS_LEVEL level)
    : CWpdEnumerator(level),
      m_device(nullptr),
      m_current(nullptr)
{
}

void CWpdBaseContentEnumerator::FinalRelease()
{
    ReleaseCurrent();

    if (m_device != nullptr)
    {
        m_device->Close();
        m_device->Release();
    }

    m_keys.Release();
    m_enum.Release();
}

HRESULT WINAPI CWpdBaseContentEnumerator::Initialize(CWpdDevice* device, PCWSTR parentObjectId)
{
    _ASSERTE(m_device == nullptr);
    _ASSERTE(device != nullptr);
    _ASSERTE(parentObjectId != nullptr && parentObjectId[0] != L'\0');

    HRESULT hr;

    hr = device->Open(GENERIC_READ);
    if (SUCCEEDED(hr))
    {
        auto* content = device->GetContentNoAddRef();
        hr = content->EnumObjects(0U, parentObjectId, nullptr, &m_enum);
        _ASSERTE(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            m_keys.Attach(GetRequiredKeys());

            m_device = device;
            device->AddRef();
        }

        if (FAILED(hr))
        {
            device->Close();
        }
    }

    return hr;
}

HRESULT WINAPI CWpdBaseContentEnumerator::MoveNext()
{
    HRESULT hr;
    ULONG fetched;
    PWSTR objectId;

    do
    {
        ReleaseCurrent();

        hr = m_enum->Next(1UL, &objectId, &fetched);
        _ASSERTE(fetched == 1UL || hr != S_OK);
        if (hr == S_OK)
        {
            CComPtr<IPortableDeviceValues> values;
            hr = m_device->GetPropertiesNoAddRef()->GetValues(objectId, m_keys, &values);
            if (SUCCEEDED(hr))
            {
                if (!ShouldSkip(values))
                {
                    // Reset hr to S_OK, since IPortableDeviceProperties::GetValues
                    // might have returned S_FALSE if one or more values could
                    // not be retrieved (this is ok). But S_FALSE ends enumeration.
                    hr = S_OK;

                    // ObjectId ownership is transferred to the CWpdContentItem,
                    // do not free.
                    m_current = CreateItem(m_device, objectId, values);
                    break;
                }
            }

            ::CoTaskMemFree(objectId);
        }
    } while (hr == S_OK);

    return hr;
}

CFxItem* WINAPI CWpdBaseContentEnumerator::GetCurrent() const
{
    _ASSERTE(m_current != nullptr);
    m_current->AddRef();
    return m_current;
}

void WINAPI CWpdBaseContentEnumerator::Reset()
{
    ReleaseCurrent();
    m_enum->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// CWpdStorageEnumerator

CWpdStorageEnumerator::CWpdStorageEnumerator()
    : CWpdBaseContentEnumerator(WPDFS_LEVEL_STORAGE)
{
}

CWpdPluginDataInterface* WINAPI CWpdStorageEnumerator::CreatePluginData(CFxPluginFSInterface& owner)
{
    return new CWpdStoragePluginDataInterface(owner);
}

IPortableDeviceKeyCollection* WINAPI CWpdStorageEnumerator::GetRequiredKeys()
{
    return CWpdStorageItem::GetRequiredKeys();
}

bool WINAPI CWpdStorageEnumerator::ShouldSkip(IPortableDeviceValues* values)
{
    HRESULT hr;
    GUID guid;

    hr = values->GetGuidValue(WPD_FUNCTIONAL_OBJECT_CATEGORY, &guid);
    bool isStorage = SUCCEEDED(hr) && IsEqualGUID(guid, WPD_FUNCTIONAL_CATEGORY_STORAGE);

    return !isStorage;
}

CWpdBaseContentItem* WINAPI CWpdStorageEnumerator::CreateItem(CWpdDevice* device, PCWSTR objectId, IPortableDeviceValues* values)
{
    return new CWpdStorageItem(device, objectId, values);
}

DWORD WINAPI CWpdStorageEnumerator::GetValidData() const
{
    // We do not have any extra data but the name for the storage items.
    return VALID_DATA_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// CWpdContentEnumerator

CWpdContentEnumerator::CWpdContentEnumerator()
    : CWpdBaseContentEnumerator(WPDFS_LEVEL_CONTENT)
{
    ::GetSystemTimeAsFileTime(&m_now);
}

CWpdPluginDataInterface* WINAPI CWpdContentEnumerator::CreatePluginData(CFxPluginFSInterface& owner)
{
    return new CWpdContentPluginDataInterface(owner);
}

IPortableDeviceKeyCollection* WINAPI CWpdContentEnumerator::GetRequiredKeys()
{
    return CWpdContentItem::GetRequiredKeys();
}

bool WINAPI CWpdContentEnumerator::ShouldSkip(IPortableDeviceValues* values)
{
    return false;
}

CWpdBaseContentItem* WINAPI CWpdContentEnumerator::CreateItem(CWpdDevice* device, PCWSTR objectId, IPortableDeviceValues* values)
{
    return new CWpdContentItem(device, objectId, values, m_now);
}

DWORD WINAPI CWpdContentEnumerator::GetValidData() const
{
    DWORD mask = __super::GetValidData();
    ////mask |= VALID_DATA_PL_SIZE;
    mask |= VALID_DATA_SIZE | VALID_DATA_DATE | VALID_DATA_TIME;
    return mask;
}

////////////////////////////////////////////////////////////////////////////////
// CWpdBaseContentPluginDataInterface

CWpdBaseContentPluginDataInterface::CWpdBaseContentPluginDataInterface(
    CFxPluginFSInterface& owner,
    WPDFS_LEVEL level) : CWpdPluginDataInterface(owner, level)
{
}

////////////////////////////////////////////////////////////////////////////////
// CWpdStoragePluginDataInterface

CWpdStoragePluginDataInterface::CWpdStoragePluginDataInterface(CFxPluginFSInterface& owner)
    : CWpdBaseContentPluginDataInterface(owner, WPDFS_LEVEL_STORAGE)
{
}

HIMAGELIST WINAPI CWpdStoragePluginDataInterface::GetSimplePluginIcons(int iconSize)
{
    return g_oDeviceIcons.GetSimpleDeviceIcons(iconSize);
}

////////////////////////////////////////////////////////////////////////////////
// CWpdContentPluginDataInterface

CWpdContentPluginDataInterface::CWpdContentPluginDataInterface(CFxPluginFSInterface& owner)
    : CWpdBaseContentPluginDataInterface(owner, WPDFS_LEVEL_CONTENT)
{
}

int WINAPI CWpdContentPluginDataInterface::GetIconsType() const
{
    return pitFromRegistry;
}
