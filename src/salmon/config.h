// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CConfiguration
//

#define DESCRIPTION_SIZE 5000
#define EMAIL_SIZE 100

class CConfiguration
{
public:
    char Description[DESCRIPTION_SIZE];
    char Email[EMAIL_SIZE];
    BOOL Restart; // neukladame

public:
    CConfiguration();

    BOOL Save();
    BOOL Load();
};

extern CConfiguration Config;
