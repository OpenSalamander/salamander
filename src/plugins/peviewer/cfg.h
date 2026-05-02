// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: noexpandtab:sw=8:ts=8

/*
	PE File Viewer Plugin for Open Salamander

	Copyright (c) 2015-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2015-2023 Open Salamander Authors

	cfg.h
	Plugin configuration.
*/

#ifdef _MSC_VER
#pragma once
#endif

class CPEDumper;
class CPEFile;

typedef struct _CFG_DUMPER
{
    LPCTSTR pszKey;
    int nTitleId;
    CPEDumper*(WINAPI* pfnFactory)(CPEFile*);
} CFG_DUMPER;

typedef struct _CFG_CHAIN_ENTRY
{
    const CFG_DUMPER* pDumperCfg;
} CFG_CHAIN_ENTRY;

extern const CFG_DUMPER g_cfgDumpers[];
extern CFG_CHAIN_ENTRY g_cfgChain[];
extern CFG_CHAIN_ENTRY g_cfgChainDefault[]; // for "Reset" in config dialog box
extern unsigned g_cfgChainLength;

int FindDumperInChain(const CFG_DUMPER* pDumperCfg);
void BuildDefaultDumperChain();
PCTSTR GetDumperTitleStr(int nTitleId);
