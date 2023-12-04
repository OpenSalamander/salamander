// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "cache.h"
#include "plugins.h"
#include "dialogs.h"

CDiskCache DiskCache;
CDeleteManager DeleteManager;

// *****************************************************************************

BOOL InitializeDiskCache()
{
    return DiskCache.IsGood();
}

//
// *****************************************************************************
// CCacheData
//

CCacheData::CCacheData(const char* name, const char* tmpName, BOOL ownDelete,
                       CPluginInterfaceAbstract* ownDeletePlugin) : LockObject(1, 2), LockObjOwner(1, 2)
{
    Name = DupStr(name);
    TmpName = DupStr(tmpName);
    Preparing = HANDLES(CreateMutex(NULL, TRUE, NULL));
    if (Preparing == NULL || Name == NULL || TmpName == NULL)
    {
        if (Name != NULL)
            free(Name);
        if (TmpName != NULL)
            free(TmpName);
        if (Preparing != NULL)
            HANDLES(CloseHandle(Preparing));
        Preparing = NULL;
        TmpName = Name = NULL;
        NewCount = 0;
    }
    else
        NewCount = 1;
    Cached = FALSE;
    Prepared = FALSE;
    Size = CQuadWord(0, 0);
    LastAccess = 0;
    Detached = FALSE;
    OutOfDate = FALSE;
    OwnDelete = ownDelete;
    OwnDeletePlugin = ownDeletePlugin;
}

CCacheData::~CCacheData()
{
    if (TmpName != NULL && LockObject.Count == 0) // tmp-soubor jiz neni potreba
    {
        if (!CleanFromDisk())
        {
            if (Detached)
            {
                TRACE_I("Tmp-file stays on disk (was not deleted), it is a wish of client of disk-cache.");
            }
            else
            {
                TRACE_E("Unable to delete tmp-file (it is probably locked by other process).");
            }
        }
    }
    else
    {
        if (TmpName != NULL)
            TRACE_I("Tmp-file " << TmpName << " stays on disk. It is still opened (in use).");
    }
    if (NewCount != 0)
        TRACE_E("Preliminary destruction of tmp-file " << TmpName);
    if (Name != NULL)
        free(Name);
    if (TmpName != NULL)
        free(TmpName);
    if (Preparing != NULL)
    {
        ReleaseMutex(Preparing); // pro jistotu
        HANDLES(CloseHandle(Preparing));
    }
    int i;
    for (i = 0; i < LockObject.Count; i++)
    {
        if (LockObjOwner[i])
            HANDLES(CloseHandle(LockObject[i]));
    }
}

BOOL CCacheData::CleanFromDisk()
{
    CALL_STACK_MESSAGE1("CCacheData::CleanFromDisk");
    if (!Detached) // user si nepreje smazat tmp-soubor
    {
        if (!OwnDelete)
        {
            DWORD attrs = SalGetFileAttributes(TmpName);
            if (attrs == 0xFFFFFFFF)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
            }
            if (attrs & FILE_ATTRIBUTE_DIRECTORY) // adresar
            {
                RemoveTemporaryDir(TmpName);
            }
            else // soubor
            {
                if (attrs & FILE_ATTRIBUTE_READONLY)
                {
                    SetFileAttributes(TmpName, FILE_ATTRIBUTE_ARCHIVE);
                }
                DeleteFile(TmpName);
            }
            attrs = SalGetFileAttributes(TmpName); // overime jestli se povedl smazat
            if (attrs == 0xFFFFFFFF)
            {
                return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
            }
        }
        else // mazani ma zajistit plugin
        {
            if (OwnDeletePlugin != NULL) // muzeme zahajit mazani (plugin nemuze byt unloaded) jinak se soubor mazat
            {                            // nebude (bud uz je smazany nebo je jen odpojeny - vybira plugin pri svem unloadu)
                DeleteManager.AddFile(TmpName, OwnDeletePlugin);
                OwnDeletePlugin = NULL; // dalsi mazani uz neprobehne
            }
            return TRUE; // uspech (na skutecny vysledek operace nemuzeme cekat)
        }
    }
    return FALSE;
}

const char*
CCacheData::GetName(CDiskCache* monitor, BOOL* exists, BOOL canBlock, BOOL onlyAdd, int* errorCode)
{
    CALL_STACK_MESSAGE3("CCacheData::GetName(, , %d, %d,)", canBlock, onlyAdd);
    if (errorCode != NULL)
        *errorCode = DCGNE_SUCCESS;
    NewCount++;
    DWORD res;
    if (canBlock && !onlyAdd)
    {
        // uvolnime monitor dalsim threadum na dobu cekani na pripravenost tmp-souboru
        monitor->Leave();
        res = WaitForSingleObject(Preparing, INFINITE);
        monitor->Enter();
    }
    else
    {
        res = WaitForSingleObject(Preparing, 0); // zadne cekani
        if (res == WAIT_TIMEOUT)                 // soubor se pripravuje, nebudeme cekat
        {
            NewCount--;
            *exists = FALSE;
            if (errorCode != NULL)
                *errorCode = onlyAdd ? DCGNE_ALREADYEXISTS : DCGNE_NOTFOUND;
            return NULL; // chyba "nenalezeno" a je-li 'onlyAdd' TRUE, tak i "soubor jiz existuje"
        }
    }

    if (res == WAIT_OBJECT_0) // tmp-soubor je pripraven nebo ho smime pripravit my (ten kdo vola GetName)
    {
        if (Prepared) // tmp-soubor je pripraven
        {
            DWORD attrs = SalGetFileAttributes(TmpName);
            if (attrs == 0xFFFFFFFF || OutOfDate)
            {
                if (OutOfDate && attrs != 0xFFFFFFFF)
                    TRACE_I("Updating tmp-file " << TmpName << ", it was out-of-date.");
                else
                    TRACE_I("Somebody has removed our tmp-file " << TmpName << ", we must ask client to create it again.");
                /*
        char dir[MAX_PATH];
        char *s = strrchr(TmpName, '\\');
        memcpy(dir, TmpName, s - TmpName);
        dir[s - TmpName] = 0;
        DWORD attrs = SalGetFileAttributes(dir);
        if (attrs == 0xFFFFFFFF)
        {
          if (!CreateDirectory(dir, NULL))
          {
            TRACE_E("Unable to create tmp-directory " << dir);
          }
        }
*/
                Prepared = FALSE;
            }
            else
            {
                ReleaseMutex(Preparing); // predame zpravu o pripravenosti pro ostatnim threadum
                if (onlyAdd)
                {
                    NewCount--;
                    *exists = FALSE;
                    if (errorCode != NULL)
                        *errorCode = DCGNE_ALREADYEXISTS;
                    return NULL; // vratime chybu "soubor jiz existuje"
                }
                else
                {
                    *exists = TRUE;
                    return TmpName;
                }
            }
        }
        // tmp-soubor neni pripraven, vlastnici thread to vzdal
    }
    OutOfDate = FALSE; // pokud byl soubor out-of-date tak uz se pracuje na jeho obnove
    // tmp-soubor neni pripraven, vlastnici thread byl terminovan
    // mutex Preparing je nas (bud pres WAIT_OBJECT_0 nebo WAIT_ABANDONED)
    *exists = FALSE;
    if (!CleanFromDisk()) // je potreba zmenit jmeno, puvodni tmp-soubor asi zustal otevreny
    {
        TRACE_E("Unable to delete tmp-file.");
    }
    return TmpName;
}

BOOL CCacheData::NamePrepared(const CQuadWord& size)
{
    CALL_STACK_MESSAGE2("CCacheData::NamePrepared(%g)", size.GetDouble());
    Size = size;
    Prepared = TRUE;
    ReleaseMutex(Preparing);
    return TRUE;
}

BOOL CCacheData::AssignName(CCacheHandles* handles, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove)
{
    CALL_STACK_MESSAGE3("CCacheData::AssignName(, , %d, %d)", lockOwner, remove);
    if (NewCount-- == 0)
        TRACE_E("Incorrect call to CCacheData::AssignName()");
    if (!Prepared)
        TRACE_E("Incorrect call to CCacheData::AssignName() - file is not ready (you should call NamePrepared or ReleaseName)");
    LockObject.Add(lock);
    if (!LockObject.IsGood())
    {
        LockObject.ResetState();
        return FALSE;
    }
    LockObjOwner.Add(lockOwner);
    if (!LockObjOwner.IsGood())
    {
        LockObjOwner.ResetState();
        LockObject.Delete(LockObject.Count - 1);
        return FALSE;
    }
    if (remove == crtCache && !OutOfDate)
        Cached = TRUE;

    handles->SetBox(lock, this);

    return TRUE;
}

int LastAccessCounter = 1; // globalni pocitadlo "casu" pro CCacheData::LastAccess

BOOL CCacheData::ReleaseName(BOOL* lastLock, BOOL storeInCache)
{
    CALL_STACK_MESSAGE2("CCacheData::ReleaseName(, %d)", storeInCache);
    if (NewCount-- == 0)
        TRACE_E("Incorrect call to CCacheData::ReleaseName()");
    if (!Prepared)
        ReleaseMutex(Preparing); // pokud tmp-soubor pripravujeme, timto to vzdavame
    *lastLock = IsLocked();
    if (*lastLock && Prepared && storeInCache && !OutOfDate) // v cache muze zustat jen pripraveny soubor
    {
        Cached = TRUE;                    // oznacime soubor jako cachovany, jinak by ho disk-cache okamzite zrusila
        LastAccess = LastAccessCounter++; // posledni 'lock', jde mu o krk (nastavime cas posl. pouziti)
    }
    return TRUE;
}

BOOL CCacheData::WaitSatisfied(HANDLE lock, BOOL* lastLock)
{
    CALL_STACK_MESSAGE1("CCacheData::WaitSatisfied(, )");

    int i;
    for (i = 0; i < LockObject.Count; i++)
    {
        if (LockObject[i] == lock)
        {
            if (LockObjOwner[i])
                HANDLES(CloseHandle(lock));
            LockObject.Delete(i);
            LockObjOwner.Delete(i);
            *lastLock = IsLocked();
            if (*lastLock)
                LastAccess = LastAccessCounter++; // posledni 'lock', jde mu o krk
            return TRUE;
        }
    }
    TRACE_E("Incorrect call to CCacheData::WaitSatisfied().");
    CALL_STACK_MESSAGE1("CCacheData::WaitSatisfied::bad_call");
    while (1)
        Sleep(1000);
    return FALSE;
}

void CCacheData::PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach)
{
    CALL_STACK_MESSAGE2("CCacheData::PrematureDeleteByPlugin(, %d)", onlyDetach);
    if (Detached)
        onlyDetach = TRUE;
    if (OwnDelete &&                        // mazani ma zajistit plugin
        OwnDeletePlugin == ownDeletePlugin) // tento tmp-soubor hledame (maze jej plugin 'ownDeletePlugin')
    {
        if (!onlyDetach)
            DeleteManager.AddFile(TmpName, OwnDeletePlugin);
        OwnDeletePlugin = NULL;
    }
}

//
// *****************************************************************************
// CCacheDirData
//

CCacheDirData::CCacheDirData(const char* path) : Names(100, 50)
{
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] == '\\')
        l--;
    memcpy(Path, path, l);
    if (l > 0)
        Path[l++] = '\\';
    Path[l] = 0;
    PathLength = l;
}

CCacheDirData::~CCacheDirData()
{
    int i;
    for (i = 0; i < Names.Count; i++)
    {
        CCacheData* name = Names[i];
        delete name;
    }
    if (PathLength > 0)
        Path[PathLength - 1] = 0; // oriznuti backslashe
    SetFileAttributes(Path, FILE_ATTRIBUTE_ARCHIVE);
    RemoveDirectory(Path);
}

BOOL CCacheDirData::ContainTmpName(const char* tmpName, const char* rootTmpPath,
                                   int rootTmpPathLen, BOOL* canContainThisName)
{
    CALL_STACK_MESSAGE2("CCacheDirData::ContainTmpName(%s, , ,)", tmpName);

    *canContainThisName = FALSE;
    if (rootTmpPathLen < PathLength &&
        StrNICmp(Path, rootTmpPath, rootTmpPathLen) == 0)
    {
        const char* s = Path + rootTmpPathLen;
        if (*s == '\\')
            s++;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s == 0 || *(s + 1) == 0) // za root-tmp-path je uz jen jeden nazev podadresare (prip. se '\\' na konci)
        {
            *canContainThisName = TRUE; // tmp-root odpovida, tmp-soubor sem lze umistit

            char tmpFullName[MAX_PATH];
            memcpy(tmpFullName, Path, PathLength);
            if (PathLength + strlen(tmpName) + 1 <= MAX_PATH)
            {
                strcpy(tmpFullName + PathLength, tmpName);
                int i;
                for (i = 0; i < Names.Count; i++)
                {
                    if (Names[i]->TmpNameEqual(tmpFullName))
                        return TRUE;
                }

                WIN32_FIND_DATA data;
                HANDLE find = HANDLES_Q(FindFirstFile(tmpFullName, &data));
                if (find != INVALID_HANDLE_VALUE)
                {
                    HANDLES(FindClose(find));
                    if (StrICmp(tmpName, data.cFileName) == 0)
                    {
                        TRACE_E("CCacheDirData::ContainTmpName(): unexpected situation: tmp-directory contains unknown file!");
                        *canContainThisName = FALSE; // soubor sem nelze umistit, otevrel by se nejaky cizi soubor
                    }
                    else
                    {
                        if (data.cAlternateFileName[0] != 0 && StrICmp(tmpName, data.cAlternateFileName) == 0)
                        {
                            TRACE_I("CCacheDirData::ContainTmpName(): tmp-directory contains file whose dos-name conflicts with new tmp-file - different tmp-directory has to be choosen!");
                            *canContainThisName = FALSE; // soubor sem nelze umistit, otevrel by se existujici soubor se shodnym dos-name
                        }
                    }
                }
            }
        }
    }
    return FALSE;
}

BOOL CCacheDirData::GetNameIndex(const char* name, int& index)
{
    if (Names.Count == 0)
    {
        index = 0;
        return FALSE;
    }

    int l = 0, r = Names.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = strcmp(name, Names[m]->GetName());
        if (res == 0) // nalezeno
        {
            index = m;
            return TRUE;
        }
        else if (res < 0)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                index = m; // mel by byt na teto pozici
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                index = m + 1; // mel by byt az za touto pozici
                return FALSE;
            }
            l = m + 1;
        }
    }
}

int CCacheDirData::CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin)
{
    CALL_STACK_MESSAGE1("CCacheDirData::CountNamesDeletedByPlugin()");
    int count = 0;
    int i;
    for (i = 0; i < Names.Count; i++)
        if (Names[i]->IsDeletedByPlugin(ownDeletePlugin))
            count++;
    return count;
}

void CCacheDirData::PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach)
{
    CALL_STACK_MESSAGE2("CCacheDirData::PrematureDeleteByPlugin(, %d)", onlyDetach);
    int i;
    for (i = 0; i < Names.Count; i++)
        Names[i]->PrematureDeleteByPlugin(ownDeletePlugin, onlyDetach);
}

void CCacheDirData::RemoveEmptyTmpDirsOnlyFromDisk()
{
    CALL_STACK_MESSAGE1("CCacheDirData::RemoveEmptyTmpDirsOnlyFromDisk()");

    if (PathLength > 0)
        Path[PathLength - 1] = 0; // oriznuti backslashe
    SetFileAttributes(Path, FILE_ATTRIBUTE_ARCHIVE);
    // system smaze nas tmp-adresar jen je-li prazdny (v pripade dalsi potreby tmp-adresare si ho vytvorime v CCacheDirData::GetName())
    RemoveDirectory(Path);
    if (PathLength > 0)
        Path[PathLength - 1] = '\\'; // vraceni backslashe
}

BOOL CCacheDirData::GetName(CDiskCache* monitor, const char* name, BOOL* exists, const char** tmpPath,
                            BOOL canBlock, BOOL onlyAdd, int* errorCode)
{
    CALL_STACK_MESSAGE4("CCacheDirData::GetName(, %s, , , %d, %d,)", name, canBlock, onlyAdd);
    int i;
    if (errorCode != NULL)
        *errorCode = DCGNE_SUCCESS;
    if (GetNameIndex(name, i)) // 'name' nalezeno na indexu 'i'
    {
        *tmpPath = Names[i]->GetName(monitor, exists, canBlock, onlyAdd, errorCode);
        if (*tmpPath != NULL) // nejde o fatal-error ani o nepripraveny tmp-soubor
        {                     // ani o chybu "soubor jiz existuje" (jen je-li 'onlyAdd' TRUE)
            CheckAndCreateDirectory(Path, NULL, TRUE);
        }
        return TRUE;
    }
    return FALSE;
}

const char*
CCacheDirData::GetName(const char* name, const char* tmpName, BOOL* exists, BOOL ownDelete,
                       CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode)
{
    CALL_STACK_MESSAGE4("CCacheDirData::GetName(%s, %s, , %d, ,)", name, tmpName, ownDelete);
    if (errorCode != NULL)
        *errorCode = DCGNE_SUCCESS;
    char tmpFullName[MAX_PATH];
    memcpy(tmpFullName, Path, PathLength);
    if (PathLength + strlen(tmpName) + 1 <= MAX_PATH)
    {
        strcpy(tmpFullName + PathLength, tmpName);
        CCacheData* newName = new CCacheData(name, tmpFullName, ownDelete, ownDeletePlugin);
        if (newName == NULL || !newName->IsGood())
        {
            if (newName == NULL)
                TRACE_E(LOW_MEMORY);
            else
                delete newName;
            *exists = TRUE; // fatal error
            if (errorCode != NULL)
                *errorCode = DCGNE_LOWMEMORY;
            return NULL;
        }
        int i;
        if (GetNameIndex(name, i)) // chyba: vkladane jmeno je unikatni - jeste v poli nemuze byt
        {
            TRACE_E("This should never happen!");
        }
        Names.Insert(i, newName);
        if (!Names.IsGood())
        {
            Names.ResetState();
            delete newName;
            *exists = TRUE; // fatal error
            if (errorCode != NULL)
                *errorCode = DCGNE_LOWMEMORY;
            return NULL;
        }
        *exists = FALSE;
        CheckAndCreateDirectory(Path, NULL, TRUE);
        return newName->GetTmpName();
    }
    else
    {
        TRACE_I("CCacheDirData::GetName(): Too long name for tmp-file in Disk Cache.");
        *exists = TRUE; // fatal error
        if (errorCode != NULL)
            *errorCode = DCGNE_TOOLONGNAME;
        return NULL;
    }
}

BOOL CCacheDirData::NamePrepared(const char* name, const CQuadWord& size, BOOL* ret)
{
    CALL_STACK_MESSAGE3("CCacheDirData::NamePrepared(%s, %g, )", name, size.GetDouble());
    int i;
    if (GetNameIndex(name, i)) // 'name' nalezeno na indexu 'i'
    {
        *ret = Names[i]->NamePrepared(size);
        return TRUE;
    }
    return FALSE;
}

BOOL CCacheDirData::AssignName(CCacheHandles* handles, const char* name, HANDLE lock, BOOL lockOwner,
                               CCacheRemoveType remove, BOOL* ret)
{
    CALL_STACK_MESSAGE4("CCacheDirData::AssignName(, %s, , %d, %d, )", name, lockOwner, remove);
    int i;
    if (GetNameIndex(name, i)) // 'name' nalezeno na indexu 'i'
    {
        *ret = Names[i]->AssignName(handles, lock, lockOwner, remove);
        return TRUE;
    }
    return FALSE;
}

BOOL CCacheDirData::ReleaseName(const char* name, BOOL* ret, BOOL* lastCached, BOOL storeInCache)
{
    CALL_STACK_MESSAGE3("CCacheDirData::ReleaseName(%s, , , %d)", name, storeInCache);
    int i;
    if (GetNameIndex(name, i)) // 'name' nalezeno na indexu 'i'
    {
        BOOL last;
        *ret = Names[i]->ReleaseName(&last, storeInCache);
        *lastCached = FALSE;
        if (last) // byl to zaroven posledni link na tento tmp-soubor
        {
            if (Names[i]->IsCached())
                *lastCached = TRUE;
            else // neni cachovany, rovnou ho zrusime
            {
                delete (Names[i]);
                Names.Delete(i);
            }
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CCacheDirData::Release(CCacheData* data)
{
    CALL_STACK_MESSAGE1("CCacheDirData::Release()");
    int i;
    if (GetNameIndex(data->GetName(), i)) // 'data' nalezeno na indexu 'i'
    {                                     // vyuzili jsme toho, ze jmeno je unikatni, takze nutne Names[i] == data
#ifdef _DEBUG
        if (Names[i] != data)
        {
            TRACE_E("This should never happen!");
            return FALSE;
        }
#endif // _DEBUG
        Names.Delete(i);
        TRACE_I("Tmp-file " << data->GetTmpName() << " was deleted.");
        delete data;
        return TRUE;
    }
    return FALSE;
}

CQuadWord
CCacheDirData::GetSizeOfFiles()
{
    CALL_STACK_MESSAGE1("CCacheDirData::GetSizeOfFiles()");
    CQuadWord size(0, 0);
    int i;
    for (i = 0; i < Names.Count; i++)
        size += Names[i]->GetSize();
    return size;
}

void CCacheDirData::AddVictimsToArray(TDirectArray<CCacheData*>& victArr)
{
    CALL_STACK_MESSAGE1("CCacheDirData::AddVictimsToArray()");
    int i;
    for (i = 0; i < Names.Count; i++)
    {
        if (Names[i]->IsLocked())
        {
            if (Names[i]->IsCached())
                victArr.Add(Names[i]);
            else
                TRACE_E("Unexpected situation in CCacheDirData::AddVictimsToArray().");
        }
    }
}

BOOL CCacheDirData::DetachTmpFile(const char* tmpName)
{
    if (StrNICmp(tmpName, Path, PathLength) == 0) // je-li sance, ze jde o nas tmp-soubor
    {
        int i;
        for (i = 0; i < Names.Count; i++)
        {
            if (StrICmp(Names[i]->GetTmpName() + PathLength, tmpName + PathLength) == 0) // mame ho
            {
                Names[i]->DetachTmpFile();
                return TRUE;
            }
        }
    }
    return FALSE;
}

void CCacheDirData::FlushCache(const char* name)
{
    int nameLen = (int)strlen(name);
    int i;
    GetNameIndex(name, i);       // najdeme index (shoda/vlozeni), od ktereho ma smysl hledat stejne predpony
    for (; i < Names.Count; i++) // jmena jsou razena -> prvky se stejnou predponou jsou souvisle za sebou
    {
        if (strncmp(Names[i]->GetName(), name, nameLen) == 0) // mame ho
        {
            CCacheData* data = Names[i];
            if (data->IsLocked())
            {
                // smazeme nalezeny tmp-soubor
                Names.Delete(i);
                TRACE_I("Tmp-file " << data->GetTmpName() << " was deleted.");
                delete data;
                i--;
            }
            else
            {
                // nelze jej smazat ted, smaze se tedy ihned, jak jen to pujde
                data->SetOutOfDate();
            }
        }
        else
            break; // dal uz byt nemohou, koncime
    }
}

BOOL CCacheDirData::FlushOneFile(const char* name)
{
    int i;
    if (GetNameIndex(name, i)) // 'name' nalezeno na indexu 'i'
    {
        CCacheData* data = Names[i];
        if (data->IsLocked())
        {
            // smazeme nalezeny tmp-soubor
            Names.Delete(i);
            TRACE_I("Tmp-file " << data->GetTmpName() << " was deleted.");
            delete data;
            i--;
        }
        else
        {
            // nelze jej smazat ted, smaze se tedy ihned, jak jen to pujde
            data->SetOutOfDate();
        }
        return TRUE; // smazano
    }
    return FALSE; // nenalezen
}

//
// *****************************************************************************
// CCacheHandles
//

unsigned ThreadCacheHandlesBody(void* param)
{
    CALL_STACK_MESSAGE1("ThreadCacheHandlesBody()");
    SetThreadNameInVCAndTrace("DiskCache");
    TRACE_I("Begin");

    Sleep(300); // aby stihl nastartovat Salamander

    CCacheHandles* handles = (CCacheHandles*)param;
    while (1)
    {
        HANDLE handle;
        CCacheData* owner;
        int index;
        handles->WaitForObjects(&handle, &owner, &index);
        CALL_STACK_MESSAGE1("ThreadCacheHandlesBody::WaitForObjects_end");
        if (handle == handles->Terminate) // meli bychom koncit
        {
            break;
        }
        else
        {
            if (handle == handles->BoxFull) // meli bychom zpracovat data v boxu
            {
                if (!handles->ReceiveBox())
                {
                    TRACE_E("ThreadCacheHandles(): Unable to receive data from the box.");
                    CALL_STACK_MESSAGE1("ThreadCacheHandlesBody::Unable_to_receive_box");
                    while (1)
                        Sleep(1000); // kousneme se kontrolovane ;-)
                }
            }
            else
            {
                if (handle == handles->TestIdle) // WaitForIdle odblokoval WaitForObjects (staci ignorovat)
                {
                }
                else // nekdo zvenci zmenil stav 'handle' na "signaled" (napr. skoncila aplikace)
                {
                    if (handle != NULL && owner != NULL)
                        handles->WaitSatisfied(handle, owner, index);
                    else
                    {
                        TRACE_E("Unexpected situation in ThreadCacheHandles().");
                        CALL_STACK_MESSAGE1("ThreadCacheHandlesBody::NULL");
                        while (1)
                            Sleep(1000); // kousneme se kontrolovane ;-)
                    }
                }
            }
        }
    }

    TRACE_I("End");
    return 0;
}

unsigned ThreadCacheHandlesEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadCacheHandlesBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread CacheHandles: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadCacheHandles(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return ThreadCacheHandlesEH(param);
}

CCacheHandles::CCacheHandles() : Handles(100, 50), Owners(100, 50)
{
    Idle = 0;
    HandleInBox = NULL;
    OwnerInBox = NULL;
    DiskCache = NULL;
    Box = HANDLES(CreateEvent(NULL, FALSE, TRUE, NULL));                                           // "signaled" state, auto
    BoxFull = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));                                      // "nonsignaled" state, auto
    Terminate = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));                                     // "nonsignaled" state, manual
    TestIdle = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));                                     // "nonsignaled" state, auto
    IsIdle = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));                                       // "nonsignaled" state, auto
    if (Box == NULL || BoxFull == NULL || Terminate == NULL || TestIdle == NULL || IsIdle == NULL) // error
    {
        if (Box != NULL)
            HANDLES(CloseHandle(Box));
        if (BoxFull != NULL)
            HANDLES(CloseHandle(BoxFull));
        if (Terminate != NULL)
            HANDLES(CloseHandle(Terminate));
        if (TestIdle != NULL)
            HANDLES(CloseHandle(TestIdle));
        if (IsIdle != NULL)
            HANDLES(CloseHandle(IsIdle));
        IsIdle = TestIdle = Terminate = BoxFull = Box = NULL;
        Thread = NULL;
    }
    else
    {
        Handles.Add(Terminate); // pridame ridici objekty na zacatek pole
        Handles.Add(BoxFull);
        Handles.Add(TestIdle);
        Owners.Add(0);
        Owners.Add(0);
        Owners.Add(0);
        if (Handles.IsGood() && Owners.IsGood())
        {
            DWORD threadID;
            Thread = HANDLES(CreateThread(NULL, 0, ThreadCacheHandles, this, 0, &threadID));
        }
        else
            Thread = NULL;
        if (Thread == NULL)
        {
            TRACE_E("Unable to start cache-handles thread.");
            if (Box != NULL)
                HANDLES(CloseHandle(Box));
            if (BoxFull != NULL)
                HANDLES(CloseHandle(BoxFull));
            if (Terminate != NULL)
                HANDLES(CloseHandle(Terminate));
            if (TestIdle != NULL)
                HANDLES(CloseHandle(TestIdle));
            if (IsIdle != NULL)
                HANDLES(CloseHandle(IsIdle));
            IsIdle = TestIdle = Terminate = BoxFull = Box = NULL;
        }
    }
}

void CCacheHandles::Destroy()
{
    CALL_STACK_MESSAGE1("CCacheHandles::Destroy()");
    if (Thread != NULL) // je potreba thread terminovat
    {
        SetEvent(Terminate);                                   // "mel bys koncit"
        if (WaitForSingleObject(Thread, 1000) == WAIT_TIMEOUT) // dame mu na to 1 sekundu
        {
            TerminateThread(Thread, 666);          // nechce se mu, zabijeme ho
            WaitForSingleObject(Thread, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
        }
        HANDLES(CloseHandle(Thread));
        Thread = NULL;
    }
    if (Terminate != NULL)
        HANDLES(CloseHandle(Terminate));
    if (TestIdle != NULL)
        HANDLES(CloseHandle(TestIdle));
    if (Box != NULL)
        HANDLES(CloseHandle(Box));
    if (BoxFull != NULL)
        HANDLES(CloseHandle(BoxFull));
    if (IsIdle != NULL)
        HANDLES(CloseHandle(IsIdle));
    IsIdle = TestIdle = Terminate = Box = BoxFull = NULL;
}

void CCacheHandles::WaitForBox()
{
    CALL_STACK_MESSAGE1("CCacheHandles::WaitForBox()");
    WaitForSingleObject(Box, INFINITE);
}

void CCacheHandles::SetBox(HANDLE handle, CCacheData* owner)
{
    HandleInBox = handle;
    OwnerInBox = owner;
    SetEvent(BoxFull);
}

void CCacheHandles::ReleaseBox()
{
    HandleInBox = NULL;
    OwnerInBox = NULL;
    SetEvent(Box);
}

void CCacheHandles::WaitForObjects(HANDLE* handle, CCacheData** owner, int* index)
{
    CALL_STACK_MESSAGE1("CCacheHandles::WaitForObjects(,)");
    *handle = NULL;
    *owner = NULL;
    *index = -1;
    if (Handles.Count == 0)
    {
        TRACE_E("CCacheHandles::WaitForObjects(): Deadlock!");
        CALL_STACK_MESSAGE1("CCacheHandles::WaitForObjects::Deadlock");
        while (1)
            Sleep(1000); // kousneme se kontrolovane ;-)
        return;
    }
    BOOL showError = TRUE;
    DWORD res;
    int timeout = 0; // time-out pro wait-for-multiple-objects (viz nize)
    while (1)
    {
        if (Idle == 1)
            Idle = 2; // pta se, budeme zjistovat
        int offset;
        for (offset = 0; offset < Handles.Count;)
        {
            int count = Handles.Count - offset;
            if (count > MAXIMUM_WAIT_OBJECTS)
                count = MAXIMUM_WAIT_OBJECTS;

            res = WaitForMultipleObjects(count, ((HANDLE*)Handles.GetData()) + offset, FALSE, timeout);
            timeout = 0;
            if (res == WAIT_FAILED)
            {
                DWORD err = GetLastError();
                TRACE_E("WaitForMultipleObjects() returned error: " << GetErrorText(err));
                CALL_STACK_MESSAGE5("CCacheHandles::WaitForObjects::WaitForMultipleObjects_error: %s, %d, %d, %d",
                                    GetErrorText(err), offset, count, Handles.Count);
#ifndef CALLSTK_DISABLE
                int i;
                for (i = 0; i < count; i++) // trochu prasarna ...
                {
#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
                    new CCallStackMessage(TRUE, 0, "handle %d: %p", i, (((HANDLE*)Handles.GetData()) + offset)[i]);
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
                    new CCallStackMessage("handle %d: %p", i, (((HANDLE*)Handles.GetData()) + offset)[i]);
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
                }
#endif // CALLSTK_DISABLE
                while (1)
                    Sleep(1000); // kousneme se kontrolovane ;-)
                return;
            }
            else
            {
                if (res != WAIT_TIMEOUT)
                {
                    if (res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + count)
                        *index = offset + res - WAIT_OBJECT_0;
                    else
                    {
                        if (res >= WAIT_ABANDONED_0 && res < WAIT_ABANDONED_0 + count)
                        {
                            *index = offset + res - WAIT_ABANDONED_0;
                        }
                        else
                        {
                            CALL_STACK_MESSAGE2("CCacheHandles::WaitForObjects::WaitForMultipleObjects_res=%X", res);
                            while (1)
                                Sleep(1000); // kousneme se kontrolovane ;-)
                        }
                    }
                    *handle = (HANDLE)Handles[*index];
                    *owner = Owners[*index];
                    if (Idle == 2)
                        Idle = 1; // formalita: odpoved ne, ale ptal se, takze se pta znovu
                    return;
                }
            }
            offset += count;
        }
        if (Idle == 2)
        {
            Idle = 3;         // pokud se pta a doslo to az sem, odpoved je ano
            SetEvent(IsIdle); // pustime WaitForIdle - uz jsme v idle-stavu
        }

        // dame si na zacatku cyklu kontroly handlu chvilku pohov (pri te prilezitosti cekame na
        // prvni dil handlu - zahrnuje 'Terminate', 'BoxFull', 'TestIdle' a prvnich par beznych handlu)
        timeout = CACHE_HANDLES_WAIT;
    }
}

BOOL CCacheHandles::ReceiveBox()
{
    CALL_STACK_MESSAGE1("CCacheHandles::ReceiveBox()");
    Handles.Add(HandleInBox); // pridame novou dvojici z boxu do pole
    if (!Handles.IsGood())
    {
        Handles.ResetState();
        ReleaseBox();
        return FALSE;
    }
    Owners.Add(OwnerInBox);
    if (!Owners.IsGood())
    {
        Handles.Delete(Handles.Count - 1);
        Owners.ResetState();
        ReleaseBox();
        return FALSE;
    }
    ReleaseBox();
    return TRUE;
}

void CCacheHandles::WaitSatisfied(HANDLE handle, CCacheData* owner, int index)
{
    CALL_STACK_MESSAGE2("CCacheHandles::WaitSatisfied(, , %d)", index);
    // vyhodime dvojici z pole
    if (Handles[index] != handle && Owners[index] != owner)
    {
        TRACE_E("Inconsistency of internal data was detected.");
        CALL_STACK_MESSAGE1("CCacheHandles::WaitSatisfied::inconsistency");
        while (1)
            Sleep(1000);
    }
    Handles.Delete(index);
    Owners.Delete(index);
    DiskCache->WaitSatisfied(handle, owner); // vstup do monitoru disk-cache
}

void CCacheHandles::WaitForIdle()
{
    CALL_STACK_MESSAGE1("CCacheHandles::WaitForIdle()");
    TRACE_I("CCacheHandles::WaitForIdle begin");
    Idle = 1;                              // ptame se
    SetEvent(TestIdle);                    // pokud sledovaci thread ceka, prerusime cekani (spustime test idle-stavu)
    WaitForSingleObject(IsIdle, INFINITE); // cekame na idle
    TRACE_I("CCacheHandles::WaitForIdle end");
}

//
// *****************************************************************************
// CDiskCache
//

CDiskCache::CDiskCache() : Dirs(10, 5)
{
    CALL_STACK_MESSAGE_NONE;
    Handles.SetDiskCache(this);
    HANDLES(InitializeCriticalSection(&Monitor));
    HANDLES(InitializeCriticalSection(&WaitForIdleCS));
}

CDiskCache::~CDiskCache()
{
    CALL_STACK_MESSAGE_NONE;
    Handles.Destroy();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        if (Dirs[i] != NULL)
        {
            CCacheDirData* data = Dirs[i];
            delete data;
        }
    }
    HANDLES(DeleteCriticalSection(&WaitForIdleCS));
    HANDLES(DeleteCriticalSection(&Monitor));
}

void CDiskCache::PrepareForShutdown()
{
    CALL_STACK_MESSAGE1("CDiskCache::PrepareForShutdown()");
    WaitForIdle();
    Enter();
    int i;
    for (i = Dirs.Count - 1; i >= 0; i--)
    {
        CCacheDirData* data = Dirs[i];
        if (data != NULL && data->GetNamesCount() == 0)
        {
            delete data;
            Dirs.Delete(i);
            if (!Dirs.IsGood())
                Dirs.ResetState();
        }
    }
    Leave();
}

void CDiskCache::RemoveEmptyTmpDirsOnlyFromDisk()
{
    CALL_STACK_MESSAGE1("CDiskCache::RemoveEmptyTmpDirsOnlyFromDisk()");
    WaitForIdle();
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
        Dirs[i]->RemoveEmptyTmpDirsOnlyFromDisk();
    Leave();
}

const char*
CDiskCache::GetName(const char* name, const char* tmpName, BOOL* exists, BOOL onlyAdd,
                    const char* rootTmpPath, BOOL ownDelete,
                    CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode)
{
    CALL_STACK_MESSAGE6("CDiskCache::GetName(%s, %s, , %d, %s, %d, ,)", name, tmpName, onlyAdd,
                        rootTmpPath, ownDelete);
    Enter();
    if (errorCode != NULL)
        *errorCode = DCGNE_SUCCESS;
    // overime, jestli 'name' zname
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        const char* tmpPath;
        if (Dirs[i]->GetName(this, name, exists, &tmpPath,
                             tmpName != NULL && !onlyAdd, onlyAdd, errorCode))
        { // 'name' nalezeno; je-li tmpName NULL, muze jit i o nepripraveny soubor (vraci chybu "nenalezeno")
            // je-li 'onlyAdd' TRUE, muze jit o chybu "soubor jiz existuje"
            Leave();
            return tmpPath;
        }
    }

    // pokud jen hledame existujici tmp-soubor, vratime ze nebyl nalezen
    if (tmpName == NULL)
    {
        *exists = FALSE; // chyba "nenalezeno" (a ne fatalni chyba)
        Leave();
        if (errorCode != NULL)
            *errorCode = DCGNE_NOTFOUND;
        return NULL;
    }

    char sysTmpDir[MAX_PATH];
    const char* rootTmpPathExp;
    if (rootTmpPath == NULL) // jde-li o TEMP, zjistime jeho polohu
    {
        if (!GetTempPath(MAX_PATH, sysTmpDir))
            sysTmpDir[0] = 0;
        rootTmpPathExp = sysTmpDir;
    }
    else
        rootTmpPathExp = rootTmpPath;
    int rootTmpPathExpLen = (int)strlen(rootTmpPathExp);
    BOOL canContainThisName;

    // najdeme vhodny tmp-adresar pro pridavany tmp-soubor
    for (i = 0; i < Dirs.Count; i++)
    {
        if (!Dirs[i]->ContainTmpName(tmpName, rootTmpPathExp, rootTmpPathExpLen, &canContainThisName) &&
            canContainThisName) // pridani noveho 'name'
        {
            const char* ret = Dirs[i]->GetName(name, tmpName, exists, ownDelete, ownDeletePlugin, errorCode);
            Leave();
            return ret;
        }
    }
    // musime zalozit novy tmp-adresar
    if (rootTmpPath != NULL)
        GetRootPath(sysTmpDir, rootTmpPath);
    char newDirPath[MAX_PATH];
    if (rootTmpPath != NULL &&
            (SalCheckPath(TRUE, sysTmpDir, ERROR_SUCCESS, TRUE, MainWindow->HWindow) != ERROR_SUCCESS ||
             !CheckAndCreateDirectory(rootTmpPath, NULL, TRUE)) || // pokud to neni TEMP, musime tmp-root nejprve overit a pripadne vytvorit
        !SalGetTempFileName(rootTmpPath, "SAL", newDirPath, FALSE))
    {
        *exists = TRUE; // fatal error
        Leave();
        if (errorCode != NULL)
            *errorCode = DCGNE_ERRCREATINGTMPDIR;
        return NULL;
    }

    CCacheDirData* newDir = new CCacheDirData(newDirPath);
    if (newDir == NULL)
    {
        TRACE_E(LOW_MEMORY);
        RemoveDirectory(newDirPath);
        *exists = TRUE; // fatal error
        Leave();
        if (errorCode != NULL)
            *errorCode = DCGNE_LOWMEMORY;
        return NULL;
    }
    Dirs.Add(newDir);
    if (!Dirs.IsGood())
    {
        delete newDir;
        Dirs.ResetState();
        RemoveDirectory(newDirPath);
        *exists = TRUE; // fatal error
        Leave();
        if (errorCode != NULL)
            *errorCode = DCGNE_LOWMEMORY;
        return NULL;
    }

    // pridame 'name' do noveho tmp-adresare (index==Dirs.Count - 1)
    const char* ret = newDir->GetName(name, tmpName, exists, ownDelete, ownDeletePlugin, errorCode);
    Leave();
    return ret;
}

BOOL CDiskCache::NamePrepared(const char* name, const CQuadWord& size)
{
    CALL_STACK_MESSAGE3("CDiskCache::NamePrepared(%s, %g)", name, size.GetDouble());
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        BOOL ret;
        if (Dirs[i]->NamePrepared(name, size, &ret)) // 'name' nalezeno
        {
            Leave();
            return ret;
        }
    }
    Leave();
    TRACE_E("Incorrect call to CDiskCache::NamePrepared().");
    return FALSE;
}

BOOL CDiskCache::AssignName(const char* name, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove)
{
    CALL_STACK_MESSAGE4("CDiskCache::AssignName(%s, , %d, %d)", name, lockOwner, remove);
    Handles.WaitForBox(); // pockame az budeme mit kam zapisovat

    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        BOOL ret;
        if (Dirs[i]->AssignName(&Handles, name, lock, lockOwner, remove, &ret))
        { // 'name' nalezeno
            if (!ret)
                Handles.ReleaseBox(); // nastala chyba, uvolnime box
            Leave();
            return ret;
        }
    }
    Handles.ReleaseBox(); // nastala chyba, uvolnime box
    Leave();
    TRACE_E("Incorrect call to CDiskCache::AssignName().");
    return FALSE;
}

BOOL CDiskCache::ReleaseName(const char* name, BOOL storeInCache)
{
    CALL_STACK_MESSAGE3("CDiskCache::ReleaseName(%s, %d)", name, storeInCache);
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        BOOL ret;
        BOOL lastCached;
        if (Dirs[i]->ReleaseName(name, &ret, &lastCached, storeInCache)) // 'name' nalezeno
        {
            if (lastCached) // tmp-soubor je bezprizorny a cachovany, podivame se, jestli uz je
            {               // potreba uvolnovat, pripadne uvolnime misto na disku
                CheckCachedFiles();
            }
            Leave();
            return ret;
        }
    }
    Leave();
    TRACE_E("Incorrect call to CDiskCache::ReleaseName().");
    return FALSE;
}

void SortVictims(TDirectArray<CCacheData*>& victArr, int left, int right)
{
    int i = left, j = right;
    int pivot = victArr[(i + j) / 2]->GetLastAccess();

    do
    {
        while (victArr[i]->GetLastAccess() < pivot && i < right)
            i++;
        while (pivot < victArr[j]->GetLastAccess() && j > left)
            j--;

        if (i <= j)
        {
            CCacheData* swap = victArr[i];
            victArr[i] = victArr[j];
            victArr[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        SortVictims(victArr, left, j);
    if (i < right)
        SortVictims(victArr, i, right);
}

void CDiskCache::CheckCachedFiles()
{
    CALL_STACK_MESSAGE1("CDiskCache::CheckCachedFiles()");
    CQuadWord size(0, 0);
    int i;
    for (i = 0; i < Dirs.Count; i++)
        size += Dirs[i]->GetSizeOfFiles();
    if (size > MAX_CACHE_SIZE) // je potreba par souboru zrusit
    {
        TDirectArray<CCacheData*> victArr(100, 50);
        for (i = 0; i < Dirs.Count; i++)
        {
            Dirs[i]->AddVictimsToArray(victArr);
        }
        if (!victArr.IsGood())
            return; // malo pameti, neprovedeme optimalizaci
        if (victArr.Count > 1)
            SortVictims(victArr, 0, victArr.Count - 1);
        int actVict = 0;
        while (size > MAX_CACHE_SIZE)
        {
            // vybereme nejstarsi cachovany bezprizorny tmp-soubor
            if (actVict + 1 < victArr.Count)
            {
                CCacheData* data = victArr[actVict++];
                size -= data->GetSize();         // odecteme jeho velikost z celkove velikosti cache
                for (i = 0; i < Dirs.Count; i++) // uvolnime ho z cache a disku
                {
                    if (Dirs[i]->Release(data))
                        break; // nalezen a zrusen
                }
            }
            else
                break; // alespon jeden cachovany soubor musime v cache nechat,
                       // bude to prave ten, ktery jsme naposledy uvolnili,
                       // zamezi to zahozeni souboru, na ktery user zrovna koukal
        }
    }
}

void CDiskCache::WaitSatisfied(HANDLE lock, CCacheData* owner)
{
    CALL_STACK_MESSAGE1("CDiskCache::WaitSatisfied(,)");
    Enter();
    BOOL last;
    if (owner->WaitSatisfied(lock, &last))
    {
        if (last) // tmp-soubor je bezprizorny, muzeme ho zrusit
        {
            if (owner->IsCached()) // podivame se, jestli uz je potreba uvolnovat,
            {                      // pripadne uvolnime misto na disku
                CheckCachedFiles();
            }
            else // mame soubor primo smazat
            {
                int i;
                for (i = 0; i < Dirs.Count; i++)
                {
                    if (Dirs[i]->Release(owner)) // nalezen
                    {
                        Leave();
                        return;
                    }
                }
                Leave();
                TRACE_E("Incorrect call to CDiskCache::WaitSatisfied().");
                return;
            }
        }
        Leave();
        return;
    }
    Leave();
    return; // chyba v owner->WaitSatisfied()
}

BOOL CDiskCache::DetachTmpFile(const char* tmpName)
{
    CALL_STACK_MESSAGE2("CDiskCache::DetachTmpFile(%s)", tmpName);
    BOOL ret = FALSE;
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        if (Dirs[i]->DetachTmpFile(tmpName))
        {
            ret = TRUE;
            break;
        }
    }
    Leave();
    return ret;
}

void CDiskCache::FlushCache(const char* name)
{
    CALL_STACK_MESSAGE2("CDiskCache::FlushCache(%s)", name);
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        Dirs[i]->FlushCache(name);
    }
    Leave();
}

BOOL CDiskCache::FlushOneFile(const char* name)
{
    CALL_STACK_MESSAGE2("CDiskCache::FlushOneFile(%s)", name);
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
    {
        if (Dirs[i]->FlushOneFile(name))
        {
            Leave();
            return TRUE; // smazano
        }
    }
    Leave();
    return FALSE;
}

int CDiskCache::CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin)
{
    CALL_STACK_MESSAGE1("CDiskCache::CountNamesDeletedByPlugin()");
    int count = 0;
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
        count += Dirs[i]->CountNamesDeletedByPlugin(ownDeletePlugin);
    Leave();
    return count;
}

void CDiskCache::PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach)
{
    CALL_STACK_MESSAGE2("CDiskCache::PrematureDeleteByPlugin(, %d)", onlyDetach);
    Enter();
    int i;
    for (i = 0; i < Dirs.Count; i++)
        Dirs[i]->PrematureDeleteByPlugin(ownDeletePlugin, onlyDetach);
    Leave();
}

void CDiskCache::ClearTEMPIfNeeded(HWND parent, HWND hActivePanel)
{
    char tmpDir[2 * MAX_PATH];
    if (GetTempPath(2 * MAX_PATH, tmpDir))
    {
        SalPathAddBackslash(tmpDir, 2 * MAX_PATH);
        char* tmpDirEnd = tmpDir + strlen(tmpDir);
        if (SalPathAppend(tmpDir, "SAL*.tmp", 2 * MAX_PATH)) // pridame masku (nevejde se = nema smysl nic hledat)
        {
            TIndirectArray<char> tmpDirs(10, 50);

            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(tmpDir, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                { // zpracujeme vsechny nalezene adresare (chyby hledani ignorujeme)
                    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        char* s = data.cFileName + 3;
                        while (*s != 0 && *s != '.' &&
                               (*s >= '0' && *s <= '9' || *s >= 'a' && *s <= 'f' || *s >= 'A' && *s <= 'F'))
                            s++;
                        if (StrICmp(s, ".tmp") == 0) // odpovida "SAL" + hex-cislo + ".tmp" = je temer jiste nas adresar
                        {
                            char* tmp = DupStr(data.cFileName);
                            if (tmp != NULL)
                            {
                                tmpDirs.Add(tmp);
                                if (!tmpDirs.IsGood())
                                {
                                    free(tmp);
                                    tmpDirs.ResetState();
                                }
                            }
                        }
                    }
                } while (FindNextFile(find, &data));
                HANDLES(FindClose(find));
            }

            if (tmpDirs.IsGood() && tmpDirs.Count > 0)
            {
                char buf[300];
                char buf2[300];
                CQuadWord qwSize(tmpDirs.Count, 0);
                ExpandPluralString(buf2, 300, LoadStr(IDS_DELETETMPSALDIRS),
                                   1, &qwSize);
                _snprintf_s(buf, _TRUNCATE, buf2, tmpDirs.Count);
                char alias[200];
                sprintf(alias, "%d\t%s\t%d\t%s\t%d\t%s",
                        DIALOG_ABORT, LoadStr(IDS_DELETETMPSALDIRS_YES),
                        DIALOG_RETRY, LoadStr(IDS_DELETETMPSALDIRS_NO),
                        DIALOG_IGNORE, LoadStr(IDS_DELETETMPSALDIRS_FOCUS));
                int ret = CMessageBox(parent,
                                      MSGBOXEX_ABORTRETRYIGNORE | MSGBOXEX_ICONQUESTION | MSGBOXEX_DEFBUTTON3,
                                      LoadStr(IDS_QUESTION), buf, NULL, NULL, NULL,
                                      0, NULL, alias, NULL, NULL)
                              .Execute();
                if (ret == IDABORT)
                {
                    for (int i = 0; i < tmpDirs.Count; i++)
                    {
                        lstrcpyn(tmpDirEnd, tmpDirs[i], (int)(2 * MAX_PATH - (tmpDirEnd - tmpDir)));
                        RemoveTemporaryDir(tmpDir);
                    }
                }
                if (ret == IDIGNORE) // focus
                {
                    lstrcpyn(tmpDirEnd, tmpDirs[0], (int)(2 * MAX_PATH - (tmpDirEnd - tmpDir)));
                    SendMessage(hActivePanel, WM_USER_FOCUSFILE, (WPARAM) "", (LPARAM)tmpDir);
                }
            }
        }
    }
    else
        TRACE_E("Unable to clear TEMP directory: TEMP directory not defined!");
}

//****************************************************************************
//
// CDeleteManager
//

CDeleteManagerItem::CDeleteManagerItem(const char* fileName, CPluginInterfaceAbstract* plugin)
{
    FileName = DupStr(fileName);
    Plugin = plugin;
}

CDeleteManagerItem::~CDeleteManagerItem()
{
    if (FileName != NULL)
        free(FileName);
}

CDeleteManager::CDeleteManager() : Data(10, 50)
{
    HANDLES(InitializeCriticalSection(&CS));
    WaitingForProcessing = FALSE;
    BlockDataProcessing = FALSE;
}

CDeleteManager::~CDeleteManager()
{
    HANDLES(DeleteCriticalSection(&CS));
}

void CDeleteManager::AddFile(const char* fileName, CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE2("CDeleteManager::AddFile(%s)", fileName);

    CDeleteManagerItem* item = new CDeleteManagerItem(fileName, plugin);
    if (item != NULL && item->IsGood())
    {
        HANDLES(EnterCriticalSection(&CS));
        Data.Add(item);
        if (Data.IsGood())
        {
            item = NULL;               // je pridana, bude se dealokovat mimo tuto metodu
            if (!WaitingForProcessing) // je-li treba, zajistime zpracovani novych dat
            {
                WaitingForProcessing = TRUE;
                if (MainWindow != NULL && MainWindow->HWindow != NULL)
                    PostMessage(MainWindow->HWindow, WM_USER_PROCESSDELETEMAN, 0, 0);
                else
                    TRACE_E("Unexpected situation in CDeleteManager::AddFile().");
            }
        }
        else
        {
            Data.ResetState();
            TRACE_I("Unable to delete file " << fileName << ". Low memory!");
        }
        HANDLES(LeaveCriticalSection(&CS));
    }
    if (item != NULL)
        delete item; // pokud se ji nepodarilo pridat, dealokujeme
}

void CDeleteManager::ProcessData()
{
    CALL_STACK_MESSAGE1("CDeleteManager::ProcessData()");

    if (BlockDataProcessing)
        return;

    // snizime prioritu threadu - jdeme mazat soubor do pluginu
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    if (MainWindow != NULL)
        UpdateWindow(MainWindow->HWindow);

    CPluginData* plugin = NULL;
    CPluginInterfaceAbstract* lastFoundPlugin = NULL;
    BOOL firstFile = TRUE;
    while (1)
    {
        // vyber novych dat
        CDeleteManagerItem* item = NULL;
        HANDLES(EnterCriticalSection(&CS));
        if (Data.Count > 0)
        {
            item = Data[0]; // vybereme nejstarsi prvek (nemuze byt NULL)
            Data.Detach(0);
            if (!Data.IsGood())
                Data.ResetState(); // k odpojeni prvku doslo, a ze je vetsi pole nicemu nevadi
        }
        else
            WaitingForProcessing = FALSE; // koncime zpracovani
        HANDLES(LeaveCriticalSection(&CS));
        if (item == NULL)
            break; // konec zpracovani dat

        // zpracovani novych dat
        if (lastFoundPlugin == NULL || lastFoundPlugin != item->Plugin)
        {
            plugin = Plugins.GetPluginData(item->Plugin);
            lastFoundPlugin = item->Plugin;
        }
        if (plugin != NULL)
        {
            plugin->DeleteTmpCopy(item->FileName, firstFile);
            firstFile = FALSE;
        }
        else
            TRACE_E("Unexpected situation in CDeleteManager::ProcessData(): plugin not found!");
        delete item;
    }

    // zvysime zpet prioritu threadu
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

void CDeleteManager::PluginMayBeUnloaded(HWND parent, CPluginData* plugin)
{
    CALL_STACK_MESSAGE1("CDeleteManager::PluginMayBeUnloaded(,)");

    if (plugin->IsArchiverAndHaveOwnDelete())
    {
        // snizime prioritu threadu - jdeme mazat soubor do pluginu
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        HANDLES(EnterCriticalSection(&CS));
        if (WaitingForProcessing) // message je na ceste, zrusime ji, je zbytecna, na konci metody probehne zpracovani dat
        {
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
            {
                // vycistime message-queue od nabufferovane WM_USER_PROCESSDELETEMAN
                MSG msg;
                PeekMessage(&msg, MainWindow->HWindow, WM_USER_PROCESSDELETEMAN, WM_USER_PROCESSDELETEMAN, PM_REMOVE);
                KillTimer(MainWindow->HWindow, IDT_DELETEMNGR_PROCESS); // timer pro zpozdene zpracovani dat (nahazuje se v WM_USER_PROCESSDELETEMAN)
            }
        }
        else
            WaitingForProcessing = TRUE; // zablokujeme posilani WM_USER_PROCESSDELETEMAN - nezadouci a zbytecne, na konci metody probehne zpracovani dat
        HANDLES(LeaveCriticalSection(&CS));

        BlockDataProcessing = TRUE; // pro pripad, ze by nam unikl WM_TIMER postnuty hl. oknu (blokujeme ho, protoze muze byt dorucen prvnim vybalenym messageboxem a jeho messageloopou)

        int copiesCount = DiskCache.CountNamesDeletedByPlugin(plugin->GetPluginInterface()->GetInterface());
        if (copiesCount > 0) // plugin ma otevrene jeste nejake tmp-soubory (a ma zajistit jejich vymaz)
        {
            if (plugin->PrematureDeleteTmpCopy(parent, copiesCount))
            { // user si preje, aby byly tmp-soubory zruseny, i kdyz se jeste pouzivaji (jsou ve viewerech, atp.)
                if (parent != NULL)
                    UpdateWindow(parent);
                DiskCache.PrematureDeleteByPlugin(plugin->GetPluginInterface()->GetInterface(), FALSE);
            }
        }

        BlockDataProcessing = FALSE;

        // zpracujeme nova data + tim prepneme WaitingForProcessing na FALSE
        ProcessData();

        // vyhodime z disku prazdne tmp-adresare (po smazanych tmp-souborech)
        DiskCache.RemoveEmptyTmpDirsOnlyFromDisk();

        // zvysime zpet prioritu threadu
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    }
}

void CDeleteManager::PluginWasUnloaded(CPluginData* plugin, CPluginInterfaceAbstract* unloadedPlugin)
{
    CALL_STACK_MESSAGE1("CDeleteManager::PluginWasUnloaded(,)");

    if (plugin->IsArchiverAndHaveOwnDelete())
    {
        // tmp-soubory musi byt odpojeny (plugin jiz neni naloaden)
        DiskCache.PrematureDeleteByPlugin(unloadedPlugin, TRUE);
    }
}
