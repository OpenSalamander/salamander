// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	fileinfo.h
	Information about the file object.
*/

#pragma once

/// Holds information about the file object.
class CFileInfo
{
private:
    PTSTR m_pszPath;
    PTSTR m_pszInfo;

    /// Clears the structure and releases resources.
    void Clear();

    HRESULT FromString(PCWSTR s);
    HRESULT FromDispatch(IDispatch* pdisp);

    void SetPath(PCWSTR pszPath, int len = -1);
    void SetInfo(PCWSTR pszInfo, int len = -1);
    void SetInfo(const CQuadWord& size, DATE date, UINT uValidFields);

    static PTSTR DupWideStr(PCWSTR s, int len = -1);

    static inline PTSTR EmptyStr()
    {
        TCHAR* empty = new TCHAR[1];
        empty[0] = _T('\0');
        return empty;
    }

public:
    /// Constructor.
    CFileInfo();

    /// Destructor.
    ~CFileInfo();

    /// Retrieves information about a file from the variant.
    HRESULT FromVariant(VARIANT* var);

    PCTSTR Path() const
    {
        _ASSERTE(m_pszPath);
        return m_pszPath;
    }

    PCTSTR Info() const
    {
        _ASSERTE(m_pszInfo);
        return m_pszInfo;
    }
};
