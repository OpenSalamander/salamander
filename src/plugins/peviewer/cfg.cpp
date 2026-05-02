// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: noexpandtab:sw=8:ts=8

/*
	PE File Viewer Plugin for Open Salamander

	Copyright (c) 2015-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2015-2023 Open Salamander Authors

	cfg.cpp
	Plugin configuration.
*/

#include "precomp.h"
#include "cfg.h"
#include "peviewer.rh2"
#include "pefile.h"

const CFG_DUMPER g_cfgDumpers[AVAILABLE_PE_DUMPERS] =
    {
        {_T("FileType"),
         IDS_DUMPER_FILETYPE,
         &CFileTypeDumper::Create},
        {_T("Version"),
         IDS_DUMPER_FILEVERSIONRESOURCE,
         &CFileVersionResourceDumper::Create},
        {_T("FileHeader"),
         IDS_DUMPER_FILEHEADER,
         &CFileHeaderDumper::Create},
        {_T("OptionalFileHeader"),
         IDS_DUMPER_OPTIONALFILEHEADER,
         &COptionalFileHeaderDumper::Create},
        {_T("ExportTable"),
         IDS_DUMPER_EXPORTTABLE,
         &CExportTableDumper::Create},
        {_T("ImportTable"),
         IDS_DUMPER_IMPORTTABLE,
         &CImportTableDumper::Create},
        {_T("SectionTable"),
         IDS_DUMPER_SECTIONTABLE,
         &CSectionTableDumper::Create},
        {_T("LoadConfig"),
         IDS_DUMPER_LOADCONFIG,
         &CLoadConfigDumper::Create},
        {_T("DebugDirectory"),
         IDS_DUMPER_DEBUGDIRECTORY,
         &CDebugDirectoryDumper::Create},
        {_T("Manifest"),
         IDS_DUMPER_MANIFEST,
         &CManifestResourceDumper::Create},
        {_T("CorHeader"),
         IDS_DUMPER_CORHEADER,
         &CCorHeaderDumper::Create},
#if WITH_COR_METADATA_DUMPER
        {_T("CorMetadata"),
         IDS_DUMPER_CORMETADATA,
         &CCorMetadataDumper::Create},
#endif
        {_T("ResourceDirectory"),
         IDS_DUMPER_RESOURCEDIRECTORY,
         &CResourceDirectoryDumper::Create},
};

CFG_CHAIN_ENTRY g_cfgChain[AVAILABLE_PE_DUMPERS];
CFG_CHAIN_ENTRY g_cfgChainDefault[AVAILABLE_PE_DUMPERS];
unsigned g_cfgChainLength;

PCTSTR GetDumperTitleStr(int nTitleId)
{
    switch (nTitleId)
    {
    case IDS_DUMPER_FILETYPE:
        return _T("File type");
    case IDS_DUMPER_FILEVERSIONRESOURCE:
        return _T("Version");
    case IDS_DUMPER_FILEHEADER:
        return _T("File header");
    case IDS_DUMPER_OPTIONALFILEHEADER:
        return _T("Optional file header");
    case IDS_DUMPER_EXPORTTABLE:
        return _T("Export table");
    case IDS_DUMPER_IMPORTTABLE:
        return _T("Import table");
    case IDS_DUMPER_SECTIONTABLE:
        return _T("Section table");
    case IDS_DUMPER_DEBUGDIRECTORY:
        return _T("Debug directory");
    case IDS_DUMPER_RESOURCEDIRECTORY:
        return _T("Resource directory");
    case IDS_DUMPER_LOADCONFIG:
        return _T("Load configuration");
    case IDS_DUMPER_MANIFEST:
        return _T("Manifest");
    case IDS_DUMPER_CORHEADER:
        return _T("CLR header");
    case IDS_DUMPER_CORMETADATA:
        return _T("CLR metadata");
    }
    TRACE_C("GetDumperTitleStr(): unknown nTitleId (" << nTitleId << ")");
    return _T("");
}

int FindDumperInChain(const CFG_DUMPER* pDumperCfg)
{
    int found = -1;

    for (unsigned i = 0; i < g_cfgChainLength; i++)
    {
        if (g_cfgChain[i].pDumperCfg == pDumperCfg)
        {
            found = (int)i;
            break;
        }
    }

    return found;
}

void BuildDefaultDumperChain()
{
    g_cfgChainLength = AVAILABLE_PE_DUMPERS;
    for (unsigned i = 0; i < g_cfgChainLength; i++)
    {
        g_cfgChain[i].pDumperCfg = &g_cfgDumpers[i];
    }
    memcpy(g_cfgChainDefault, g_cfgChain, sizeof(g_cfgChainDefault));
}
