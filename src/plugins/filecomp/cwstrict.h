// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CFilecompCoWorkerStrict
//

template <class CChar>
class CFilecompCoWorkerStrict : public CFilecompCoWorkerBase<CChar>
{
public:
    CFilecompCoWorkerStrict(HWND mainWindow, CCompareOptions& options, const int& cancelFlag);
    void Compare(CTextFileReader (&reader)[2]);

protected:
};
