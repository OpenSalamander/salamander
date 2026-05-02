// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CFilecompCoWorkerOptimized
//

template <class CChar>
class CFilecompCoWorkerOptimized : public CFilecompCoWorkerBase<CChar>
{
    SUPER(CFilecompCoWorkerBase<CChar>);

public:
    CFilecompCoWorkerOptimized(HWND mainWindow, CCompareOptions& options, const int& cancelFlag);
    template <class CCaseConverter, class CLineIterator>
    void IdentifyLines(CFilecompCoWorkerBase<CChar>::CFCFileData (&files)[2], CIndexes (&compareData)[2], const int& cancel);
    bool IsChangeIgnorable(const CChange& change);
    void Compare(CTextFileReader (&reader)[2]);

protected:
};
