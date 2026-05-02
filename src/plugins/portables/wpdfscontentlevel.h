// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfscontentlevel.h
	Content level enumerator.
*/

#pragma once

#include "device.h"

class CWpdBaseContentItem : public CWpdItem
{
private:
    PCWSTR m_objectId;

    CWpdDevice* m_device;

    CFxString m_strName;

protected:
    DWORD m_attributes;

    UINT m_flags;

    static bool WINAPI IsFolder(const GUID& contentType);

    CWpdBaseContentItem(WPDFS_LEVEL level, CWpdDevice* device, PCWSTR objectId, IPortableDeviceValues* values);

public:
    virtual ~CWpdBaseContentItem();

    CWpdDevice* WINAPI GetDeviceNoAddRef()
    {
        return m_device;
    }

    PCWSTR GetObjectId() const
    {
        return m_objectId;
    }

    /* CFxItem Overrides */

    virtual void WINAPI GetName(CFxString& name) override;

    virtual DWORD WINAPI GetAttributes() override;
};

class CWpdStorageItem final : public CWpdBaseContentItem
{
private:
    static const PROPERTYKEY* const s_requiredKeys[];

public:
    CWpdStorageItem(CWpdDevice* device, PCWSTR objectId, IPortableDeviceValues* values);

    static IPortableDeviceKeyCollection* WINAPI GetRequiredKeys();

    /* CFxItem Overrides */

    virtual int WINAPI GetSimpleIconIndex() override;
};

class CWpdContentItem final : public CWpdBaseContentItem
{
private:
    ULONGLONG m_size;

    FILETIME m_lastWrite;

    enum FLAGS
    {
        FLAGS_HaveSize = 1,
        FLAGS_HaveLastWrite = 2,
    };

    static const PROPERTYKEY* const s_requiredKeys[];

public:
    CWpdContentItem(CWpdDevice* device, PCWSTR objectId, IPortableDeviceValues* values, FILETIME now);

    static IPortableDeviceKeyCollection* WINAPI GetRequiredKeys();

    /* CFxItem Overrides */

    virtual HRESULT WINAPI GetSize(ULONGLONG& size) override;

    virtual HRESULT WINAPI GetLastWriteTime(FILETIME& time) override;
};

class CWpdBaseContentEnumerator : public CWpdEnumerator
{
private:
    CWpdDevice* m_device;

    ATL::CComPtr<IEnumPortableDeviceObjectIDs> m_enum;

    CWpdBaseContentItem* m_current;

    ATL::CComPtr<IPortableDeviceKeyCollection> m_keys;

    void WINAPI ReleaseCurrent(void)
    {
        if (m_current != nullptr)
        {
            m_current->Release();
            m_current = nullptr;
        }
    }

protected:
    CWpdBaseContentEnumerator(WPDFS_LEVEL level);

    virtual bool WINAPI ShouldSkip(IPortableDeviceValues* values) = 0;

    virtual CWpdBaseContentItem* WINAPI CreateItem(
        CWpdDevice* device,
        PCWSTR objectId,
        IPortableDeviceValues* values) = 0;

    virtual IPortableDeviceKeyCollection* WINAPI GetRequiredKeys() = 0;

    virtual void WINAPI FinalRelease() override;

public:
    virtual HRESULT WINAPI Initialize(CWpdDevice* device, PCWSTR parentObjectId);

    /* CFxEnumerator Overrides */

    virtual HRESULT WINAPI MoveNext() override;

    virtual void WINAPI Reset() override;

    virtual CFxItem* WINAPI GetCurrent() const override;
};

class CWpdStorageEnumerator final : public CWpdBaseContentEnumerator
{
protected:
    virtual bool WINAPI ShouldSkip(IPortableDeviceValues* values) override;

    virtual CWpdBaseContentItem* WINAPI CreateItem(
        CWpdDevice* device,
        PCWSTR objectId,
        IPortableDeviceValues* values) override;

    virtual IPortableDeviceKeyCollection* WINAPI GetRequiredKeys() override;

public:
    CWpdStorageEnumerator();

    /* CFxEnumerator Overrides */

    virtual DWORD WINAPI GetValidData() const override;

    /* CWpdEnumerator Overrides */

    virtual CWpdPluginDataInterface* WINAPI CreatePluginData(CFxPluginFSInterface& owner) override;
};

class CWpdContentEnumerator final : public CWpdBaseContentEnumerator
{
private:
    FILETIME m_now;

protected:
    virtual bool WINAPI ShouldSkip(IPortableDeviceValues* values) override;

    virtual CWpdBaseContentItem* WINAPI CreateItem(
        CWpdDevice* device,
        PCWSTR objectId,
        IPortableDeviceValues* values) override;

    virtual IPortableDeviceKeyCollection* WINAPI GetRequiredKeys() override;

public:
    CWpdContentEnumerator();

    /* CFxEnumerator Overrides */

    virtual DWORD WINAPI GetValidData() const override;

    /* CWpdEnumerator Overrides */

    virtual CWpdPluginDataInterface* WINAPI CreatePluginData(CFxPluginFSInterface& owner) override;
};

class CWpdBaseContentPluginDataInterface : public CWpdPluginDataInterface
{
protected:
    CWpdBaseContentPluginDataInterface(CFxPluginFSInterface& owner, WPDFS_LEVEL level);
};

class CWpdStoragePluginDataInterface final : public CWpdBaseContentPluginDataInterface
{
public:
    CWpdStoragePluginDataInterface(CFxPluginFSInterface& owner);

    /* CPluginDataInterfaceAbstract Overrides */

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) override;
};

class CWpdContentPluginDataInterface final : public CWpdBaseContentPluginDataInterface
{
public:
    CWpdContentPluginDataInterface(CFxPluginFSInterface& owner);

    /* CFxPluginDataInterface Overrides */

    virtual int WINAPI GetIconsType() const override;

    /* CPluginDataInterfaceAbstract Overrides */
};
