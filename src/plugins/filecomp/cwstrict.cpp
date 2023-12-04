// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CFilecompCoWorkerStrict
//

template <class CChar>
CFilecompCoWorkerStrict<CChar>::CFilecompCoWorkerStrict(HWND mainWindow,
                                                        CCompareOptions& options, const int& cancelFlag) : CFilecompCoWorkerBase(mainWindow, options, cancelFlag)
{
}

template <class CChar>
void CFilecompCoWorkerStrict<CChar>::Compare(CTextFileReader (&reader)[2])
{
    this->ReadFilesAndFindLines(reader);

    CLineScript script[2]; // line script
    size_t firstLines[2];
    size_t lenghts[2];

    int i;
    for (i = 0; i < 2; ++i)
    {
        script[i].reserve(max(this->Files[0].Lines.size(), this->Files[1].Lines.size()));
        firstLines[i] = 0;
        lenghts[i] = this->Files[i].Lines.size() - 1;
    }

    this->ScrictCompare(firstLines, lenghts, script, CLineSpec::lcCommon);

    // send results
    this->SendResults(script);
}
