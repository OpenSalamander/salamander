// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	saltypelib.h
	Type library encapsulation.
*/

#pragma once

class CSalamanderTypeLib
{
private:
    ITypeLib* m_pTypeLib;
    HRESULT m_hrLoad;

    void Load();

public:
    CSalamanderTypeLib();
    ~CSalamanderTypeLib();

    HRESULT Get(ITypeLib** ppTypeLib);
};

extern CSalamanderTypeLib SalamanderTypeLib;
