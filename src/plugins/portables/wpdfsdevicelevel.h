// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdfsdevicelevel.h
	Device level enumerator.
*/

#pragma once

#include "device.h"

class CWpdDeviceItem final : public CWpdItem
{
private:
    CWpdDevice* m_pDevice;

    // -1...slow icon not probed, >=0...cached slow icon in device icon list
    int m_iSimpleIcon;

    static int WINAPI GetSimpleDeviceIconByDeviceType(ULONG deviceType);

    int WINAPI GetSimpleDeviceIconByDeviceType() const
    {
        return GetSimpleDeviceIconByDeviceType(m_pDevice->GetDeviceType());
    }

    int WINAPI GetAndUpdateCachedSimpleIcon();

protected:
    virtual void WINAPI FinalRelease() override;

public:
    CWpdDeviceItem(CWpdDevice* device);

    CWpdDevice* WINAPI GetDeviceNoAddRef()
    {
        return m_pDevice;
    }

    /* CFxItem Overrides */

    virtual void WINAPI GetName(CFxString& name) override;

    virtual DWORD WINAPI GetAttributes() override;

    virtual int WINAPI GetSimpleIconIndex() override;

    virtual bool WINAPI HasSlowIcon() override;

    virtual HRESULT WINAPI GetSlowIcon(int size, _Out_ HICON& hico) override;
};

class CWpdDeviceEnumerator final : public CWpdEnumerator
{
private:
    int m_index;

    bool WINAPI ShouldSkip(CWpdDevice* device);

public:
    CWpdDeviceEnumerator();

    virtual ~CWpdDeviceEnumerator();

    HRESULT WINAPI Initialize();

    /* CFxEnumerator Overrides */

    virtual HRESULT WINAPI MoveNext() override;

    virtual void WINAPI Reset() override;

    virtual CFxItem* WINAPI GetCurrent() const override;

    /* CFxItemEnumerator Overrides */

    virtual DWORD WINAPI GetValidData() const override;

    /* CWpdEnumerator Overrides */

    virtual CWpdPluginDataInterface* WINAPI CreatePluginData(CFxPluginFSInterface& owner) override;
};

class CWpdDevicePluginDataInterface final : public CWpdPluginDataInterface
{
public:
    CWpdDevicePluginDataInterface(CFxPluginFSInterface& owner);

    /* CPluginDataInterfaceAbstract */

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) override;

    virtual BOOL WINAPI GetInfoLineContent(
        int panel,
        const CFileData* file,
        BOOL isDir,
        int selectedFiles,
        int selectedDirs,
        BOOL displaySize,
        const CQuadWord& selectedSize,
        char* buffer,
        DWORD* hotTexts,
        int& hotTextsCount) override;
};
