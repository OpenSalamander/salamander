// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "udfiso.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

// ****************************************************************************
//
// CUDFISO
//

CUDFISO::CUDFISO(CISOImage* image, DWORD extent) : CUnISOFSAbstract(image)
{
    ExtentOffset = extent;
    ISO = NULL;
    UDF = NULL;
    HFS = NULL;
}

CUDFISO::~CUDFISO()
{
    delete ISO;
    delete UDF;
    delete HFS;
}

BOOL CUDFISO::Open(BOOL quiet)
{
    CALL_STACK_MESSAGE2("CUDFISO::Open(%d)", quiet);

    // scan ISO part
    CISO9660* iso = new CISO9660(Image, ExtentOffset);
    if (!iso)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    }

    if (iso->Open(quiet))
        ISO = iso;
    else
        delete iso;

    // scan udf part
    CUDF* udf = new CUDF(Image, ExtentOffset, 0 /****/);
    if (!udf)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    }

    if (udf->Open(quiet))
        UDF = udf;
    else
        delete udf;

    // scan udf part
    CHFS* hfs = new CHFS(Image);
    if (!hfs)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    }

    // Scan HFS part but silently ignore any problem
    if (hfs->Open((udf || iso) ? TRUE : quiet))
        HFS = hfs;
    else
        delete hfs;

    return TRUE;
}

BOOL CUDFISO::ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir,
                            CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE3("CUDFISO::ListDirectory(%s, %d, , )", path, session);

    char partPath[2 * MAX_PATH + 1];
    ZeroMemory(&partPath, sizeof(partPath));
    int pathLen = (int)strlen(path);

    if (ISO != NULL)
    {
        char volId[33]; // volume identifier for ISO is 32 chars long
        strncpy_s(volId, (char*)ISO->PVD.VolumeIdentifier, _TRUNCATE);
        // trim trailing spaces
        int i;
        for (i = 32; i > 0 && (volId[i] == ' ' || volId[i] == '\0'); i--)
            volId[i] = '\0';
        if (strlen(volId) != 0)
            sprintf(partPath, "\\ISO (%s)", volId);
        else
            strcpy(partPath, "\\ISO partition");

        strcat(path, partPath);

        // avoid creating virtual session folder
        // (we have created ISO virtual folder (for partition) and therefore virtual session folder is not needed)
        if (ISO->BootRecordInfo != NULL && session == -1)
            session = 1;
        ISO->ListDirectory(path, session, dir, pluginData);

        path[pathLen] = '\0';
    }

    if (UDF != NULL)
    {
        char volId[130] = {0}; // logical volume identifier for UDF is 128 chars long

        BYTE len;
        memcpy(&len, UDF->LVD.LogicalVolumeIdentifier, 1);

        // very stupid detection if the name is encoded with 16bit chars or 8bit chars
        // note: only very stupid sw use 8bit chars and violates the standard
        if (UDF->LVD.LogicalVolumeIdentifier[3] == 0 &&
            UDF->LVD.LogicalVolumeIdentifier[5] == 0)
        {
            // seems to be 16bit char
            WideCharToMultiByte(CP_ACP, 0, (WCHAR*)(UDF->LVD.LogicalVolumeIdentifier + 2), 64, volId, sizeof(volId) - 1, 0, 0);
            volId[sizeof(volId) - 1] = 0;
        }
        else
        {
            memcpy(volId, UDF->LVD.LogicalVolumeIdentifier + 1, len * 2);
            volId[2 * len - 1] = 0;
        }

        if (volId[0] != 0)
            sprintf(partPath, "\\UDF (%s)", volId);
        else
            strcpy(partPath, "\\UDF partition");

        strcat(path, partPath);

        UDF->ListDirectory(path, session, dir, pluginData);

        path[pathLen] = '\0';
    }

    if (HFS != NULL)
    {
        char rootName[128];

        if (HFS->GetRootName(rootName, SizeOf(rootName)))
        {
            sprintf(path, "\\HFS (%s)", rootName);
        }
        else
        {
            strcat(path, "\\HFS");
        }

        HFS->ListDirectory(path, session, dir, pluginData);

        path[pathLen] = '\0';
    }

    return TRUE;
}

int CUDFISO::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                        const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE7("CUDFISO::UnpackFile( , %s, %s, %s, %p, %u, %d)", srcPath, path, nameInArc, fileData, silent, toSkip);

    if (fileData == NULL)
        return UNPACK_ERROR;

    CISOImage::CFilePos* fp = (CISOImage::CFilePos*)fileData->PluginData;
    switch (fp->Type)
    {
    case FS_TYPE_ISO9660:
        return ISO->UnpackFile(salamander, srcPath, path, nameInArc, fileData, silent, toSkip);

    case FS_TYPE_UDF:
        return UDF->UnpackFile(salamander, srcPath, path, nameInArc, fileData, silent, toSkip);

    case FS_TYPE_HFS:
        return HFS->UnpackFile(salamander, srcPath, path, nameInArc, fileData, silent, toSkip);

    default:
        return UNPACK_ERROR;
    }
}

BOOL CUDFISO::DumpInfo(FILE* outStream)
{
    CALL_STACK_MESSAGE1("CUDFISO::DumpInfo( )");

    return ISO->DumpInfo(outStream);
}
