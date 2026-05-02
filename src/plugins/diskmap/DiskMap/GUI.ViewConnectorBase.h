// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TreeMap.FileData.CZDirectory.h"

#ifdef SALAMANDER
class CSalamanderCallbackAbstract
{
public:
    virtual BOOL FocusFile(TCHAR const* filename) = 0;
    virtual BOOL CanFocusFile() = 0;
    virtual BOOL OpenFolder(TCHAR const* path) = 0;
    virtual BOOL CanOpenFolder() = 0;
    virtual void SetCloseConfirm(BOOL value) = 0;
    virtual BOOL GetCloseConfirm() = 0;
    virtual void SetShowFolders(BOOL value) = 0;
    virtual BOOL GetShowFolders() = 0;
    virtual void SetShowTooltip(BOOL value) = 0;
    virtual BOOL GetShowTooltip() = 0;
    virtual void SetPathFormat(int value) = 0;
    virtual int GetPathFormat() = 0;
    virtual BOOL ConfirmClose(HWND parent) = 0;
    virtual void About(HWND hWndParent) = 0;
    virtual int GetClusterSize(TCHAR const* path) = 0;
};
#endif

class CViewConnectorBase
{
public:
    virtual BOOL DM_ZoomOut(int level) = 0;
    virtual BOOL DL_SetSubPath(CZString const* path) = 0;
    virtual BOOL DL_SetDiskSize(INT64 size) = 0;
    virtual CZDirectory* DM_GetViewDirectory(int parentlevel) = 0;
#ifdef SALAMANDER
    virtual CSalamanderCallbackAbstract* GetSalamander() = 0;
#endif
};
