// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CZipList : public CZipCommon
{
public:
    CZipList(const char* zipName, CSalamanderForOperationsAbstract* salamander) : CZipCommon(zipName, "", salamander, NULL)
    {
        Extract = true;
    }
    int ListArchive(CSalamanderDirectoryAbstract* dir, BOOL& haveFiles);
    int List(CSalamanderDirectoryAbstract* dir, BOOL& haveFiles);
};
