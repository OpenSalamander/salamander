// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <lm.h>

//****************************************************************************
//
// CSharesItem
//

CSharesItem::CSharesItem(const char* localPath, const char* remoteName, const char* comment)
{
    Cleanup();

    if (localPath != NULL && localPath[0] != 0 && localPath[1] == ':')
    {
        char buff[MAX_PATH];
        lstrcpyn(buff, localPath, MAX_PATH);
        SalPathAddBackslash(buff, MAX_PATH); // kdyby nahodou bylo jen "c:", tak aby vznikl root
        if (SalGetFullName(buff))            // root "c:\\", ostatni bez '\\' na konci
        {
            LocalPath = DupStr(buff);
            RemoteName = DupStr(remoteName);
            Comment = DupStr(comment);
            if (LocalPath != NULL && RemoteName != NULL && Comment != NULL)
            {
                char* s = strrchr(LocalPath, '\\');
                if (s == NULL || *(s + 1) == 0) // root cesta; s==NULL je pouze pro jistotu, ale nikdy nemuze vyjit
                    LocalName = LocalPath;
                else
                    LocalName = s + 1;
            }
            else
            {
                TRACE_E(LOW_MEMORY);
                Destroy();
                Cleanup();
            }
        }
        else
            TRACE_E("Unexpected path (1) in CSharesItem::CSharesItem()");
    }
    else
        TRACE_E("Unexpected path (2) in CSharesItem::CSharesItem()");
}

CSharesItem::~CSharesItem()
{
    Destroy();
}

void CSharesItem::Cleanup()
{
    LocalPath = NULL;
    LocalName = NULL;
    RemoteName = NULL;
    Comment = NULL;
}

void CSharesItem::Destroy()
{
    if (LocalPath != NULL)
        free(LocalPath); // LocalName ukazuje do LocalPath, proto jej neuvolnujeme
    if (RemoteName != NULL)
        free(RemoteName);
    if (Comment != NULL)
        free(Comment);
}

//****************************************************************************
//
// CShares
//

/* Sekce nacteni sharu */

void CShares::Refresh()
{
    HANDLES(EnterCriticalSection(&CS));
    Wanted.DestroyMembers();
    Data.DestroyMembers();

    PSHARE_INFO_502 BufPtr, p;
    NET_API_STATUS res;
    DWORD er = 0, tr = 0, resume = 0, i;

    do
    {
        res = NetShareEnum(NULL, 502, (LPBYTE*)&BufPtr, -1, &er, &tr, &resume);
        if (res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
        {
            p = BufPtr;
            for (i = 1; i <= er; i++)
            {
                char netname[MAX_PATH];
                char path[MAX_PATH];
                char remark[MAX_PATH];
                // special nechceme, protoze je Explorer neukazuje
                BOOL include = p->shi502_type == 0;
                if (!SubsetOnly && p->shi502_type == 0x80000000) // special
                    include = TRUE;
                if (include &&
                    WideCharToMultiByte(CP_ACP, 0, p->shi502_netname, -1, netname, MAX_PATH, NULL, NULL) &&
                    WideCharToMultiByte(CP_ACP, 0, p->shi502_path, -1, path, MAX_PATH, NULL, NULL) &&
                    WideCharToMultiByte(CP_ACP, 0, p->shi502_remark, -1, remark, MAX_PATH, NULL, NULL))
                {
                    //              TRACE_I("Share: " << netname << " = " << path);
                    // pridani sharovane cesty do pole Data
                    CSharesItem* item = new CSharesItem(path, netname, remark);
                    if (item != NULL && item->IsGood())
                    {
                        Data.Add(item);
                        if (Data.IsGood())
                            item = NULL; // uspesne pridani
                        else
                        {
                            delete item;
                            Data.ResetState();
                            Data.DestroyMembers();
                            break; // chyba, nema smysl pokracovat v enumu
                        }
                    }
                    if (item != NULL)
                        delete item;
                }
                p++;
            }
            NetApiBufferFree(BufPtr);
        }
        else
            TRACE_I("Error getting shares: (" << res << ") " << GetErrorText(res));
    } while (res == ERROR_MORE_DATA);
    HANDLES(LeaveCriticalSection(&CS));
}

CShares::CShares(BOOL subsetOnly)
    : Data(10, 10), Wanted(10, 10, dtNoDelete)
{
    HANDLES(InitializeCriticalSection(&CS));
    SubsetOnly = subsetOnly;
}

CShares::~CShares()
{
    HANDLES(DeleteCriticalSection(&CS));
}

BOOL CShares::GetWantedIndex(const char* name, int& index)
{
    if (Wanted.Count == 0)
    {
        index = 0;
        return FALSE;
    }

    int l = 0, r = Wanted.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        char* hw = Wanted[m]->LocalName;
        int res = StrICmp(hw, name);
        if (res == 0) // nalezeno
        {
            index = m;
            return TRUE;
        }
        else
        {
            if (res > 0)
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
}

void CShares::PrepareSearch(const char* path)
{
    HANDLES(EnterCriticalSection(&CS));
    // vyprazdnime pole Wanted
    Wanted.DestroyMembers();

    // zaradime do nej pouze ty shary, ktere lezi na pozadovane ceste
    char buff[MAX_PATH];
    lstrcpyn(buff, path, MAX_PATH);
    if (buff[0] != 0)                        // pokud hledame shary z this_computer nesmime pripojit backslash
        SalPathAddBackslash(buff, MAX_PATH); // na konci chceme backslash
    int pathLen = (int)strlen(buff);

    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CSharesItem* item = Data[i];
        int itemNameLen = (int)(item->LocalName - item->LocalPath);
        if (pathLen == itemNameLen && StrNICmp(item->LocalPath, buff, itemNameLen) == 0)
        {
            int index;
            if (!GetWantedIndex(item->LocalName, index)) // odpovidajici share zaradime do pole Wanted jen pokud jiz tam neni
            {
                Wanted.Insert(index, item);
            }
        }
    }
    HANDLES(LeaveCriticalSection(&CS));
}

BOOL CShares::Search(const char* name)
{
    HANDLES(EnterCriticalSection(&CS));
    int index;
    BOOL ret = GetWantedIndex(name, index);
    HANDLES(LeaveCriticalSection(&CS));
    return ret;
}

BOOL CShares::GetUNCPath(const char* path, char* uncPath, int uncPathMax)
{
    HANDLES(EnterCriticalSection(&CS));
    char buff[MAX_PATH];
    lstrcpyn(buff, path, MAX_PATH);
    SalPathAddBackslash(buff, MAX_PATH); // na konci chceme backslash

    int longestIndex = -1; // index do pole Data, kde lezi nejdelsi vyhovujici share

    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CSharesItem* item = Data[i];
        int itemNameLen = (int)strlen(item->LocalPath);
        if (StrNICmp(buff, item->LocalPath, itemNameLen) == 0)
        {
            // hledame nejdelsi mozny share, ktery jeste odpovida pozadovane 'path'
            if (longestIndex == -1 || (int)strlen(Data[longestIndex]->LocalPath) < itemNameLen)
                longestIndex = i;
        }
    }
    if (longestIndex != -1)
    {
        CSharesItem* item = Data[longestIndex];
        // vlozime nazev naseho pocitace
        char unc[2 * MAX_PATH];
        strcpy(unc, "\\\\");
        DWORD len = MAX_PATH;
        GetComputerName(unc + 2, &len);
        strcat(unc, "\\");
        // pripojime nazev sharu
        strcat(unc, item->RemoteName);
        SalPathAddBackslash(unc, 2 * MAX_PATH); // na konci chceme backslash
        // z puvodni cesty pripojime adresare od sdileni dale
        if (strlen(item->LocalPath) < strlen(path))
        {
            const char* s = path + strlen(item->LocalPath);
            if (*s == '\\')
                s++; // preskocime pripadny backslash
            strcat(unc, s);
        }
        if (!SalGetFullName(unc)) // root "c:\\", ostatni bez '\\' na konci
        {
            TRACE_E("Unexpected path in CSharesItem::GetUNCPath()");
            HANDLES(LeaveCriticalSection(&CS));
            return FALSE;
        }

        lstrcpyn(uncPath, unc, uncPathMax);
        HANDLES(LeaveCriticalSection(&CS));
        return TRUE;
    }
    else
    {
        HANDLES(LeaveCriticalSection(&CS));
        return FALSE;
    }
}

BOOL CShares::GetItem(int index, const char** localPath, const char** remoteName, const char** comment)
{
    HANDLES(EnterCriticalSection(&CS));
    if (index < 0 || index >= Data.Count)
    {
        TRACE_E("CShares::GetItem index=" << index);
        HANDLES(LeaveCriticalSection(&CS));
        return FALSE;
    }
    CSharesItem* item = Data[index];
    if (localPath != NULL)
        *localPath = item->LocalPath;
    if (remoteName != NULL)
        *remoteName = item->RemoteName;
    if (comment != NULL)
        *comment = item->Comment;
    HANDLES(LeaveCriticalSection(&CS));
    return TRUE;
}
