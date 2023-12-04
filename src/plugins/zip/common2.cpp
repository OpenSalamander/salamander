// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr\\comdefs.h"
#include "config.h"
#include "typecons.h"
//#include "resource.h"
#include "zip.rh2"

#include "chicon.h"
#include "common.h"
#include "add_del.h"
#include "dialogs.h"
#include "sfxmake\\sfxmake.h"

int RenumberName(int number, const char* oldName, char* newName,
                 bool lastFile, BOOL winzip)
{
    CALL_STACK_MESSAGE4("RenumberName(%d, %s, , %d)", number, oldName, lastFile);

    const char* arcName = strrchr(oldName, '\\');
    if (arcName != NULL)
        arcName++;
    else
        arcName = oldName;
    const char* ext = strrchr(arcName, '.');
    const char* ptr = arcName + strlen(arcName);
    const char *numberEnd = NULL, *numberStart = arcName;
    bool haveNumber = false;

    while (--ptr > arcName)
    {
        if (!numberEnd && isdigit(*ptr))
            numberEnd = ptr + 1;
        if (numberEnd && !isdigit(*ptr))
        {
            numberStart = ptr + 1;
            break;
        }
    }

    char buf[MAX_PATH + 12];

    if (numberEnd && !winzip)
    {
        if (lastFile && ext && ext < numberStart) // ".cvspass" ve Windows je pripona
        {
            // zrejme posledni soubor podle winzip formatu
            int i = (int)(ext - oldName);
            memmove(buf, oldName, i);
            strcpy(buf + i, ".zip");
        }
        else
        {
            int i = (int)(numberStart - oldName);
            memcpy(buf, oldName, i);
            i += sprintf(buf + i, "%0*d", (int)(numberEnd - numberStart), number);
            strcpy(buf + i, numberEnd);
        }
    }
    else
    {
        if (ext) // ".cvspass" ve Windows je pripona
        {
            // zrejme winzip format
            int i = (int)(ext - oldName);
            memmove(buf, oldName, i);
            sprintf(buf + i, lastFile ? ".zip" : ".z%02d", number);
        }
        else
        {
            strcpy(buf, oldName);
            sprintf(buf + strlen(buf), "%02d", number);
        }
    }

    if (strlen(buf) > MAX_PATH)
    {
        TRACE_I("archive name is too long to add file numbers:" << buf);
        strcpy(newName, oldName);
    }
    else
        strcpy(newName, buf);
    return (int)strlen(newName);
}

void SplitPath2(const char* pathToSplit, char* path, char* name, char* ext)
{
    CALL_STACK_MESSAGE2("SplitPath2(%s, , , )", pathToSplit);
    const char* sour = pathToSplit;
    const char* lastSlash = NULL;
    const char* lastDot = NULL;

    while (*sour)
    {
        if (*sour == '\\')
            lastSlash = sour;
        sour++;
    }
    if (lastSlash)
    {
        lstrcpyn(path, pathToSplit, (int)(lastSlash - pathToSplit) + 2);
        sour = ++lastSlash;
    }
    else
    {
        *path = 0;
        lastSlash = sour = pathToSplit;
    }
    while (*sour)
    {
        if (*sour == '.')
            lastDot = sour;
        sour++;
    }
    if (lastDot)
    {
        lstrcpyn(name, lastSlash, (int)(lastDot - lastSlash) + 1);
        lstrcpy(ext, lastDot);
    }
    else
    {
        lstrcpy(name, lastSlash);
        *ext = 0;
    }
}

int CZipCommon::ChangeDisk()
{
    CALL_STACK_MESSAGE1("CZipCommon::ChangeDisk()");
    int ret;
    bool retry = false;

    // maly test na detekci win-zip jmen
    if (CHDiskFlags & (CHD_FIRST | CHD_SEQNAMES))
    {
        char buf[MAX_PATH];
        RenumberName(DiskNum + 1, ZipName, buf,
                     DiskNum == EOCentrDir.DiskNum, CHDiskFlags & CHD_WINZIP);
        if (SalamanderGeneral->SalGetFileAttributes(buf) == 0xFFFFFFFF)
        {
            // soubor s nasledujicim cislem neni na disku, zkusime, jestli tam nebude
            // s invertovanym winzip flagem
            RenumberName(DiskNum + 1, ZipName, buf,
                         DiskNum == EOCentrDir.DiskNum, !(CHDiskFlags & CHD_WINZIP));
            if (SalamanderGeneral->SalGetFileAttributes(buf) != 0xFFFFFFFF && lstrcmpi(ZipName, buf))
                CHDiskFlags ^= CHD_WINZIP;
        }
    }

    bool useReadCache = false;
    bool bigFile = false;
    if (ZipFile)
    {
        useReadCache = ZipFile->InputBuffer != NULL;
        bigFile = ZipFile->BigFile != 0;
    }
    do
    {
        if (!retry && (CHDiskFlags & CHD_ALL ||
                       !Removable && Config.AutoExpandMV))
        {
            if (CHDiskFlags & CHD_SEQNAMES)
            {
                RenumberName(DiskNum + 1, ZipName, ZipName,
                             DiskNum == EOCentrDir.DiskNum, CHDiskFlags & CHD_WINZIP);
            }
        }
        else
        {
            if (ChangeDiskDialog3(SalamanderGeneral->GetMsgBoxParent(),
                                  DiskNum + 1, DiskNum == EOCentrDir.DiskNum,
                                  ZipName, &CHDiskFlags) != IDOK)
            {
                return IDS_NODISPLAY;
            }
        }
        retry = false;
        if (ZipFile)
            CloseCFile(ZipFile);
        ZipFile = NULL;
        ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                          bigFile, useReadCache);
        if (ret)
        {
            if (ret == ERR_LOWMEM)
                return IDS_LOWMEM;
            else
                retry = true;
        }
    } while (retry);
    CHDiskFlags &= ~CHD_FIRST;
    //if (*OriginalCurrentDir)  SetCurrentDirToZipPath();
    if (ArchiveVolumes != NULL)
        ArchiveVolumes->Add(_strdup(ZipName));
    return 0;
}

void CZipCommon::FindLastFile(char* lastFile)
{
    CALL_STACK_MESSAGE1("CZipCommon::FindLastFile()");
    char path[MAX_PATH];
    char name[MAX_PATH];
    char ext[MAX_PATH];
    char* sour;
    int i, j;
    char mask[MAX_PATH];
    WIN32_FIND_DATA data;
    HANDLE search;
    int biggest = 0;
    char buf[MAX_PATH];
    int pathLen;

    *lastFile = NULL;
    SplitPath2(ZipName, path, name, ext);
    if (!*name)
        return;
    /*{
    lstrcpy(name, ext);
    *ext = 0;
  }*/
    i = lstrlen(name) - 1;
    sour = name + i;
    while (sour >= name)
    {
        if (!isdigit(*sour))
            break;
        sour--;
    }
    if (sour < name || sour == name + i)
        return;
    *(++sour) = 0;
    sprintf(mask, "%s%s*%s", path, name, ext);
    search = FindFirstFile(mask, &data);
    if (search == INVALID_HANDLE_VALUE)
        return;
    lstrcpy(buf, path);
    pathLen = lstrlen(buf);
    do
    {
        SplitPath2(data.cFileName, path, name, ext);
        /*if (!*name)
    {
      lstrcpy(name, ext);
      *ext = 0;
    }*/
        i = lstrlen(name) - 1;
        sour = name + i;
        while (sour >= name)
        {
            if (!isdigit(*sour))
                break;
            sour--;
        }
        if (sour >= name && sour != name + i)
        {
            j = atoi(++sour);
            if (j > biggest && !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                biggest = j;
                lstrcpy(buf + pathLen, data.cFileName);
            }
        }
    } while (FindNextFile(search, &data));
    if (GetLastError() == ERROR_NO_MORE_FILES && biggest)
        lstrcpy(lastFile, buf);
    FindClose(search);
    return;
}
