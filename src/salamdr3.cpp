// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "usermenu.h"
#include "plugins.h"
#include "fileswnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "pack.h"
#include "execute.h"
#include "shellib.h"
#include "menu.h"

CUserMenuIconBkgndReader UserMenuIconBkgndReader;

// ****************************************************************************

BOOL SalPathAppend(char* path, const char* name, int pathSize)
{
    if (path == NULL || name == NULL)
    {
        TRACE_E("Unexpected situation in SalPathAppend()");
        return FALSE;
    }
    if (*name == '\\')
        name++;
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] == '\\')
        l--;
    if (*name != 0)
    {
        int n = (int)strlen(name);
        if (l + 1 + n < pathSize) // vejdeme se i s nulou na konci?
        {
            if (l != 0)
                path[l] = '\\';
            else
                l = -1;
            memcpy(path + l + 1, name, n + 1);
        }
        else
            return FALSE;
    }
    else
        path[l] = 0;
    return TRUE;
}

// ****************************************************************************

BOOL SalPathAddBackslash(char* path, int pathSize)
{
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in SalPathAddBackslash()");
        return FALSE;
    }
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] != '\\')
    {
        if (l + 1 < pathSize)
        {
            path[l] = '\\';
            path[l + 1] = 0;
        }
        else
            return FALSE;
    }
    return TRUE;
}

// ****************************************************************************

void SalPathRemoveBackslash(char* path)
{
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in SalPathRemoveBackslash()");
        return;
    }
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] == '\\')
        path[l - 1] = 0;
}

void SalPathStripPath(char* path)
{
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in SalPathStripPath()");
        return;
    }
    char* name = strrchr(path, '\\');
    if (name != NULL)
        memmove(path, name + 1, strlen(name + 1) + 1);
}

void SalPathRemoveExtension(char* path)
{
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in SalPathRemoveExtension()");
        return;
    }

    int len = (int)strlen(path);
    char* iterator = path + len - 1;
    while (iterator >= path)
    {
        if (*iterator == '.')
        {
            //      if (iterator != path && *(iterator - 1) != '\\')  // ".cvspass" ve Windows je pripona ...
            *iterator = 0;
            break;
        }
        if (*iterator == '\\')
            break;
        iterator--;
    }
}

BOOL SalPathAddExtension(char* path, const char* extension, int pathSize)
{
    if (path == NULL || extension == NULL)
    {
        TRACE_E("Unexpected situation in SalPathAddExtension()");
        return FALSE;
    }

    int len = (int)strlen(path);
    char* iterator = path + len - 1;
    while (iterator >= path)
    {
        if (*iterator == '.')
        {
            //      if (iterator != path && *(iterator - 1) != '\\')  // ".cvspass" ve Windows je pripona ...
            return TRUE; // pripona jiz existuje
                         //      break;  // dal hledat nema smysl
        }
        if (*iterator == '\\')
            break;
        iterator--;
    }

    int extLen = (int)strlen(extension);
    if (len + extLen < pathSize)
    {
        memcpy(path + len, extension, extLen + 1);
        return TRUE;
    }
    else
        return FALSE;
}

BOOL SalPathRenameExtension(char* path, const char* extension, int pathSize)
{
    if (path == NULL || extension == NULL)
    {
        TRACE_E("Unexpected situation in SalPathRenameExtension()");
        return FALSE;
    }

    int len = (int)strlen(path);
    char* iterator = path + len - 1;
    while (iterator >= path)
    {
        if (*iterator == '.')
        {
            //      if (iterator != path && *(iterator - 1) != '\\')  // ".cvspass" ve Windows je pripona ...
            //      {
            len = (int)(iterator - path);
            break; // pripona jiz existuje -> prepiseme ji
                   //      }
                   //      break;
        }
        if (*iterator == '\\')
            break;
        iterator--;
    }

    int extLen = (int)strlen(extension);
    if (len + extLen < pathSize)
    {
        memcpy(path + len, extension, extLen + 1);
        return TRUE;
    }
    else
        return FALSE;
}

const char* SalPathFindFileName(const char* path)
{
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in SalPathFindFileName()");
        return NULL;
    }

    int len = (int)strlen(path);
    const char* iterator = path + len - 2;
    while (iterator >= path)
    {
        if (*iterator == '\\')
            return iterator + 1;
        iterator--;
    }
    return path;
}

// ****************************************************************************

BOOL SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file)
{
    char tmpDir[MAX_PATH + 10];
    char* end = tmpDir + MAX_PATH + 10;
    if (path == NULL)
    {
        if (!GetTempPath(MAX_PATH, tmpDir))
        {
            DWORD err = GetLastError();
            TRACE_E("Unable to get TEMP directory.");
            SetLastError(err);
            return FALSE;
        }
        if (SalGetFileAttributes(tmpDir) == 0xFFFFFFFF)
        {
            SalMessageBox(NULL, LoadStr(IDS_TMPDIRERROR), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            if (GetSystemDirectory(tmpDir, MAX_PATH) == 0)
            {
                DWORD err = GetLastError();
                TRACE_E("Unable to get system directory.");
                SetLastError(err);
                return FALSE;
            }
        }
    }
    else
        strcpy(tmpDir, path);

    char* s = tmpDir + strlen(tmpDir);
    if (s > tmpDir && *(s - 1) != '\\')
        *s++ = '\\';
    while (s < end && *prefix != 0)
        *s++ = *prefix++;

    if ((s - tmpDir) + 8 < MAX_PATH) // dost mista pro pripojeni "XXXX.tmp"
    {
        DWORD randNum = (GetTickCount() & 0xFFF);
        while (1)
        {
            sprintf(s, "%X.tmp", randNum++);
            if (file) // soubor
            {
                HANDLE h = HANDLES_Q(CreateFile(tmpDir, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                                FILE_ATTRIBUTE_NORMAL, NULL));
                if (h != INVALID_HANDLE_VALUE)
                {
                    HANDLES(CloseHandle(h));
                    strcpy(tmpName, tmpDir); // nakopirujeme vysledek
                    return TRUE;
                }
            }
            else // adresar
            {
                if (CreateDirectory(tmpDir, NULL))
                {
                    strcpy(tmpName, tmpDir); // nakopirujeme vysledek
                    return TRUE;
                }
            }
            DWORD err = GetLastError();
            if (err != ERROR_FILE_EXISTS && err != ERROR_ALREADY_EXISTS)
            {
                TRACE_E("Unable to create temporary " << (file ? "file" : "directory") << ": " << GetErrorText(err));
                SetLastError(err);
                return FALSE;
            }
        }
    }
    else
    {
        TRACE_E("Too long file name in SalGetTempFileName().");
        SetLastError(ERROR_BUFFER_OVERFLOW);
        return FALSE;
    }
}

// ****************************************************************************

int HandleFileException(EXCEPTION_POINTERS* e, char* fileMem, DWORD fileMemSize)
{
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) // in-page-error znamena urcite chybu souboru
    {
        return EXCEPTION_EXECUTE_HANDLER; // spustime __except blok
    }
    else
    {
        if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&    // access violation znamena chybu souboru jen pokud adresa chyby odpovida souboru
            (e->ExceptionRecord->NumberParameters >= 2 &&                         // mame co testovat
             e->ExceptionRecord->ExceptionInformation[1] >= (ULONG_PTR)fileMem && // ptr chyby ve view souboru
             e->ExceptionRecord->ExceptionInformation[1] < ((ULONG_PTR)fileMem) + fileMemSize))
        {
            return EXCEPTION_EXECUTE_HANDLER; // spustime __except blok
        }
        else
        {
            return EXCEPTION_CONTINUE_SEARCH; // hodime vyjimku dale ... call-stacku
        }
    }
}

// ****************************************************************************

BOOL SalRemovePointsFromPath(char* afterRoot)
{
    char* d = afterRoot; // ukazatel za root cestu
    while (*d != 0)
    {
        while (*d != 0 && *d != '.')
            d++;
        if (*d == '.')
        {
            if (d == afterRoot || d > afterRoot && *(d - 1) == '\\') // '.' za root cestou nebo "\."
            {
                if (*(d + 1) == '.' && (*(d + 2) == '\\' || *(d + 2) == 0)) // ".."
                {
                    char* l = d - 1;
                    while (l > afterRoot && *(l - 1) != '\\')
                        l--;
                    if (l >= afterRoot) // vypusteni adresare + ".."
                    {
                        if (*(d + 2) == 0)
                            *l = 0;
                        else
                            memmove(l, d + 3, strlen(d + 3) + 1);
                        d = l;
                    }
                    else
                        return FALSE; // ".." nelze vypustit
                }
                else
                {
                    if (*(d + 1) == '\\' || *(d + 1) == 0) // "."
                    {
                        if (*(d + 1) == 0)
                            *d = 0;
                        else
                            memmove(d, d + 2, strlen(d + 2) + 1);
                    }
                    else
                        d++;
                }
            }
            else
                d++;
        }
    }
    return TRUE;
}

BOOL SalRemovePointsFromPath(WCHAR* afterRoot)
{
    WCHAR* d = afterRoot; // ukazatel za root cestu
    while (*d != 0)
    {
        while (*d != 0 && *d != L'.')
            d++;
        if (*d == L'.')
        {
            if (d == afterRoot || d > afterRoot && *(d - 1) == L'\\') // '.' za root cestou nebo "\."
            {
                if (*(d + 1) == L'.' && (*(d + 2) == L'\\' || *(d + 2) == 0)) // ".."
                {
                    WCHAR* l = d - 1;
                    while (l > afterRoot && *(l - 1) != L'\\')
                        l--;
                    if (l >= afterRoot) // vypusteni adresare + ".."
                    {
                        if (*(d + 2) == 0)
                            *l = 0;
                        else
                            memmove(l, d + 3, sizeof(WCHAR) * (lstrlenW(d + 3) + 1));
                        d = l;
                    }
                    else
                        return FALSE; // ".." nelze vypustit
                }
                else
                {
                    if (*(d + 1) == L'\\' || *(d + 1) == 0) // "."
                    {
                        if (*(d + 1) == 0)
                            *d = 0;
                        else
                            memmove(d, d + 2, sizeof(WCHAR) * (lstrlenW(d + 2) + 1));
                    }
                    else
                        d++;
                }
            }
            else
                d++;
        }
    }
    return TRUE;
}

BOOL SalGetFullName(char* name, int* errTextID, const char* curDir, char* nextFocus,
                    BOOL* callNethood, int nameBufSize, BOOL allowRelPathWithSpaces)
{
    CALL_STACK_MESSAGE5("SalGetFullName(%s, , %s, , , %d, %d)", name, curDir, nameBufSize, allowRelPathWithSpaces);
    int err = 0;

    int rootOffset = 3; // offset zacatku adresarove casti cesty (3 pro "c:\path")
    char* s = name;
    while (*s >= 1 && *s <= ' ')
        s++;
    if (*s == '\\' && *(s + 1) == '\\') // UNC (\\server\share\...)
    {                                   // eliminace mezer na zacatku cesty
        if (s != name)
            memmove(name, s, strlen(s) + 1);
        s = name + 2;
        if (*s == '.' || *s == '?')
            err = IDS_PATHISINVALID; // cesty jako \\?\Volume{6e76293d-1828-11df-8f3c-806e6f6e6963}\ a \\.\PhysicalDisk5\ tady proste nepodporujeme...
        else
        {
            if (*s == 0 || *s == '\\')
            {
                if (callNethood != NULL)
                    *callNethood = *s == 0;
                err = IDS_SERVERNAMEMISSING;
            }
            else
            {
                while (*s != 0 && *s != '\\')
                    s++; // prejeti servername
                if (*s == '\\')
                    s++;
                if (s - name > MAX_PATH - 1)
                    err = IDS_SERVERNAMEMISSING; // nalezeny text je moc dlouhy na to, aby to byl server
                else
                {
                    if (*s == 0 || *s == '\\')
                    {
                        if (callNethood != NULL)
                            *callNethood = *s == 0 && (*(s - 1) != '.' || *(s - 2) != '\\') && (*(s - 1) != '\\' || *(s - 2) != '.' || *(s - 3) != '\\'); // nejde o "\\." ani "\\.\" (zacatek cesty typu "\\.\C:\")
                        err = IDS_SHARENAMEMISSING;
                    }
                    else
                    {
                        while (*s != 0 && *s != '\\')
                            s++; // prejeti sharename
                        if ((s - name) + 1 > MAX_PATH - 1)
                            err = IDS_SHARENAMEMISSING; // nalezeny text je moc dlouhy na to, aby to byl share (+1 za koncovy backslash)
                        if (*s == '\\')
                            s++;
                    }
                }
            }
        }
    }
    else // cesta zadana pomoci disku (c:\...)
    {
        if (*s != 0)
        {
            if (*(s + 1) == ':') // "c:..."
            {
                if (*(s + 2) == '\\') // "c:\..."
                {                     // eliminace mezer na zacatku cesty
                    if (s != name)
                        memmove(name, s, strlen(s) + 1);
                }
                else // "c:path..."
                {
                    int l1 = (int)strlen(s + 2); // delka zbytku ("path...")
                    if (LowerCase[*s] >= 'a' && LowerCase[*s] <= 'z')
                    {
                        const char* head;
                        if (curDir != NULL && LowerCase[curDir[0]] == LowerCase[*s])
                            head = curDir;
                        else
                            head = DefaultDir[LowerCase[*s] - 'a'];
                        int l2 = (int)strlen(head);
                        if (head[l2 - 1] != '\\')
                            l2++; // misto pro '\\'
                        if (l1 + l2 >= nameBufSize)
                            err = IDS_TOOLONGPATH;
                        else // sestaveni full path
                        {
                            memmove(name + l2, s + 2, l1 + 1);
                            *(name + l2 - 1) = '\\';
                            memmove(name, head, l2 - 1);
                        }
                    }
                    else
                        err = IDS_INVALIDDRIVE;
                }
            }
            else
            {
                if (curDir != NULL)
                {
                    // u relativnich cest bez '\\' na zacatku nebudeme pri zaplem 'allowRelPathWithSpaces' povazovat
                    // mezery za omyl (jmeno adresare i souboru muze zacinat na mezeru, i kdyz wokna i dalsi softy
                    // vcetne Salama se tomu snazi predejit)
                    if (allowRelPathWithSpaces && *s != '\\')
                        s = name;
                    int l1 = (int)strlen(s);
                    if (*s == '\\') // "\path...."
                    {
                        if (curDir[0] == '\\' && curDir[1] == '\\') // UNC
                        {
                            const char* root = curDir + 2;
                            while (*root != 0 && *root != '\\')
                                root++;
                            root++; // '\\'
                            while (*root != 0 && *root != '\\')
                                root++;
                            if (l1 + (root - curDir) >= nameBufSize)
                                err = IDS_TOOLONGPATH;
                            else // sestaveni cesty z rootu akt. disku
                            {
                                memmove(name + (root - curDir), s, l1 + 1);
                                memmove(name, curDir, root - curDir);
                            }
                            rootOffset = (int)(root - curDir) + 1;
                        }
                        else
                        {
                            if (l1 + 2 >= nameBufSize)
                                err = IDS_TOOLONGPATH;
                            else
                            {
                                memmove(name + 2, s, l1 + 1);
                                name[0] = curDir[0];
                                name[1] = ':';
                            }
                        }
                    }
                    else // "path..."
                    {
                        if (nextFocus != NULL)
                        {
                            char* test = name;
                            while (*test != 0 && *test != '\\')
                                test++;
                            if (*test == 0 && (int)strlen(name) < MAX_PATH)
                                strcpy(nextFocus, name);
                        }

                        int l2 = (int)strlen(curDir);
                        if (curDir[l2 - 1] != '\\')
                            l2++;
                        if (l1 + l2 >= nameBufSize)
                            err = IDS_TOOLONGPATH;
                        else
                        {
                            memmove(name + l2, s, l1 + 1);
                            name[l2 - 1] = '\\';
                            memmove(name, curDir, l2 - 1);
                        }
                    }
                }
                else
                    err = IDS_INCOMLETEFILENAME;
            }
            s = name + rootOffset;
        }
        else
        {
            name[0] = 0;
            err = IDS_EMPTYNAMENOTALLOWED;
        }
    }

    if (err == 0) // eliminace '.' a '..' v ceste
    {
        if (!SalRemovePointsFromPath(s))
            err = IDS_PATHISINVALID;
    }

    if (err == 0) // vyhozeni pripadneho nezadouciho backslashe z konce retezce
    {
        int l = (int)strlen(name);
        if (l > 1 && name[1] == ':') // typ cesty "c:\path"
        {
            if (l > 3) // neni root cesta
            {
                if (name[l - 1] == '\\')
                    name[l - 1] = 0; // orez backslashe
            }
            else
            {
                name[2] = '\\'; // root cesta, backslash nutny ("c:\")
                name[3] = 0;
            }
        }
        else
        {
            if (name[0] == '\\' && name[1] == '\\' && name[2] == '.' && name[3] == '\\' && name[4] != 0 && name[5] == ':') // cesta typu "\\.\C:\"
            {
                if (l > 7) // neni root cesta
                {
                    if (name[l - 1] == '\\')
                        name[l - 1] = 0; // orez backslashe
                }
                else
                {
                    name[6] = '\\'; // root cesta, backslash nutny ("\\.\C:\")
                    name[7] = 0;
                }
            }
            else // UNC cesta
            {
                if (l > 0 && name[l - 1] == '\\')
                    name[l - 1] = 0; // orez backslashe
            }
        }
    }

    if (errTextID != NULL)
        *errTextID = err;

    return err == 0;
}

// ****************************************************************************

TDirectArray<HANDLE> AuxThreads(10, 5);

void AuxThreadBody(BOOL add, HANDLE thread, BOOL testIfFinished)
{
    // Prevent re-entrance
    static CCriticalSection cs;
    CEnterCriticalSection enterCS(cs);

    static BOOL finished = FALSE;
    if (!finished) // po volani TerminateAuxThreads() uz nic neprijimame
    {
        if (add)
        {
            // vycistime pole od threadu, ktere jiz dobehly
            for (int i = 0; i < AuxThreads.Count; i++)
            {
                DWORD code;
                if (!GetExitCodeThread(AuxThreads[i], &code) || code != STILL_ACTIVE)
                { // thread uz dobehl
                    HANDLES(CloseHandle(AuxThreads[i]));
                    AuxThreads.Delete(i);
                    i--;
                }
            }
            BOOL skipAdd = FALSE;
            if (testIfFinished)
            {
                DWORD code;
                if (!GetExitCodeThread(thread, &code) || code != STILL_ACTIVE)
                { // thread uz dobehl
                    HANDLES(CloseHandle(thread));
                    skipAdd = TRUE;
                }
            }
            // pridame novy thread
            if (!skipAdd)
                AuxThreads.Add(thread);
        }
        else
        {
            finished = TRUE;
            for (int i = 0; i < AuxThreads.Count; i++)
            {
                HANDLE t = AuxThreads[i];
                DWORD code;
                if (GetExitCodeThread(t, &code) && code == STILL_ACTIVE)
                { // thread stale bezi, terminujeme ho
                    TerminateThread(t, 666);
                    WaitForSingleObject(t, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
                }
                HANDLES(CloseHandle(t));
            }
            AuxThreads.DestroyMembers();
        }
    }
    else
        TRACE_E("AuxThreadBody(): calling after TerminateAuxThreads() is not supported! add=" << add);
}

void AddAuxThread(HANDLE thread, BOOL testIfFinished)
{
    AuxThreadBody(TRUE, thread, testIfFinished);
}

void TerminateAuxThreads()
{
    AuxThreadBody(FALSE, NULL, FALSE);
}

// ****************************************************************************

/*
#define STOPREFRESHSTACKSIZE 50

class CStopRefreshStack
{
  protected:
    DWORD CallerCalledFromArr[STOPREFRESHSTACKSIZE];  // pole navratovych adres funkci, odkud se volal BeginStopRefresh()
    DWORD CalledFromArr[STOPREFRESHSTACKSIZE];        // pole adres, odkud se volal BeginStopRefresh()
    int Count;                                        // pocet prvku v predchozich dvou polich
    int Ignored;                                      // pocet volani BeginStopRefresh(), ktere jsme museli ignorovat (prilis male STOPREFRESHSTACKSIZE -> pripadne zvetsit)

  public:
    CStopRefreshStack() {Count = 0; Ignored = 0;}
    ~CStopRefreshStack() {CheckIfEmpty(3);} // tri BeginStopRefresh() jsou OK: pro oba panely se vola BeginStopRefresh() a treti se vola z WM_USER_CLOSE_MAINWND (ten se vola jako prvni)

    void Push(DWORD caller_called_from, DWORD called_from);
    void Pop(DWORD caller_called_from, DWORD called_from);
    void CheckIfEmpty(int checkLevel);
};

void
CStopRefreshStack::Push(DWORD caller_called_from, DWORD called_from)
{
  if (Count < STOPREFRESHSTACKSIZE)
  {
    CallerCalledFromArr[Count] = caller_called_from;
    CalledFromArr[Count] = called_from;
    Count++;
  }
  else
  {
    Ignored++;
    TRACE_E("CStopRefreshStack::Push(): you should increase STOPREFRESHSTACKSIZE! ignored=" << Ignored);
  }
}

void
CStopRefreshStack::Pop(DWORD caller_called_from, DWORD called_from)
{
  if (Ignored == 0)
  {
    if (Count > 0)
    {
      Count--;
      if (CallerCalledFromArr[Count] != caller_called_from)
      {
        TRACE_E("CStopRefreshStack::Pop(): strange situation: BeginCallerCalledFrom!=StopCallerCalledFrom - BeginCalledFrom,StopCalledFrom");
        TRACE_E("CStopRefreshStack::Pop(): strange situation: 0x" << std::hex <<
                CallerCalledFromArr[Count] << "!=0x" << caller_called_from << " - 0x" <<
                CalledFromArr[Count] << ",0x" << called_from << std::dec);
      }
    }
    else TRACE_E("CStopRefreshStack::Pop(): unexpected call!");
  }
  else Ignored--;
}

void
CStopRefreshStack::CheckIfEmpty(int checkLevel)
{
  if (Count > checkLevel)
  {
    TRACE_E("CStopRefreshStack::CheckIfEmpty(" << checkLevel << "): listing remaining BeginStopRefresh calls: CallerCalledFrom,CalledFrom");
    int i;
    for (i = 0; i < Count; i++)
    {
      TRACE_E("CStopRefreshStack::CheckIfEmpty():: 0x" << std::hex <<
              CallerCalledFromArr[i] << ",0x" << CalledFromArr[i] << std::dec);
    }
  }
}

CStopRefreshStack StopRefreshStack;
*/

void BeginStopRefresh(BOOL debugSkipOneCaller, BOOL debugDoNotTestCaller)
{
    /*
#ifdef _DEBUG     // testujeme, jestli se BeginStopRefresh() a EndStopRefresh() volaji ze stejne funkce (podle navratove adresy volajici funkce -> takze nepozna "chybu" pri volani z ruznych funkci, ktere se obe volaji ze stejne funkce)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

pokud bude jeste nekdy potreba ozivit tenhle kod, vyuzit toho, ze lze nahradit (x86 i x64):
    called_from = *(DWORD_PTR *)_AddressOfReturnAddress();

    if (debugSkipOneCaller) caller_called_from = *(DWORD*)((char*)(*(DWORD *)(*register_ebp)) + 4);
    else caller_called_from = *(DWORD*)((char*)(*register_ebp) + 4);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    called_from = -1;
    caller_called_from = -1;
  }
  StopRefreshStack.Push(debugDoNotTestCaller ? 0 : caller_called_from, called_from);
#endif // _DEBUG
*/

    //  if (StopRefresh == 0) TRACE_I("Begin stop refresh mode");
    StopRefresh++;
}

void EndStopRefresh(BOOL postRefresh, BOOL debugSkipOneCaller, BOOL debugDoNotTestCaller)
{
    /*
#ifdef _DEBUG     // testujeme, jestli se BeginStopRefresh() a EndStopRefresh() volaji ze stejne funkce (podle navratove adresy volajici funkce -> takze nepozna "chybu" pri volani z ruznych funkci, ktere se obe volaji ze stejne funkce)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

pokud bude jeste nekdy potreba ozivit tenhle kod, vyuzit toho, ze lze nahradit (x86 i x64):
    called_from = *(DWORD_PTR *)_AddressOfReturnAddress();

    if (debugSkipOneCaller) caller_called_from = *(DWORD*)((char*)(*(DWORD *)(*register_ebp)) + 4);
    else caller_called_from = *(DWORD*)((char*)(*register_ebp) + 4);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    called_from = -1;
    caller_called_from = -1;
  }
  StopRefreshStack.Pop(debugDoNotTestCaller ? 0 : caller_called_from, called_from);
#endif // _DEBUG
*/

    if (StopRefresh < 1)
    {
        TRACE_E("Incorrect call to EndStopRefresh().");
        StopRefresh = 0;
    }
    else
    {
        if (--StopRefresh == 0)
        {
            //      TRACE_I("End stop refresh mode");
            // pokud jsme vyblokovali nejaky refresh, dame mu prilezitost probehnout
            if (postRefresh && MainWindow != NULL)
            {
                if (MainWindow->LeftPanel != NULL)
                {
                    PostMessage(MainWindow->LeftPanel->HWindow, WM_USER_SM_END_NOTIFY, 0, 0);
                }
                if (MainWindow->RightPanel != NULL)
                {
                    PostMessage(MainWindow->RightPanel->HWindow, WM_USER_SM_END_NOTIFY, 0, 0);
                }
            }

            if (MainWindow != NULL && MainWindow->NeedToResentDispachChangeNotif &&
                !AlreadyInPlugin) // pokud je jeste v plug-inu, nema posilani zpravy smysl
            {
                MainWindow->NeedToResentDispachChangeNotif = FALSE;

                // postneme zadost o rozeslani zprav o zmenach na cestach
                HANDLES(EnterCriticalSection(&TimeCounterSection));
                int t1 = MyTimeCounter++;
                HANDLES(LeaveCriticalSection(&TimeCounterSection));
                PostMessage(MainWindow->HWindow, WM_USER_DISPACHCHANGENOTIF, 0, t1);
            }
        }
    }
}

// ****************************************************************************

void BeginStopIconRepaint()
{
    StopIconRepaint++;
}

void EndStopIconRepaint(BOOL postRepaint)
{
    if (StopIconRepaint > 0)
    {
        if (--StopIconRepaint == 0 && PostAllIconsRepaint)
        {
            if (postRepaint && MainWindow != NULL)
            {
                PostMessage(MainWindow->HWindow, WM_USER_REPAINTALLICONS, 0, 0);
            }
            PostAllIconsRepaint = FALSE;
        }
    }
    else
    {
        TRACE_E("Incorrect call to EndStopIconRepaint().");
        StopIconRepaint = 0;
    }
}

// ****************************************************************************

void BeginStopStatusbarRepaint()
{
    StopStatusbarRepaint++;
}

void EndStopStatusbarRepaint()
{
    if (StopStatusbarRepaint > 0)
    {
        if (--StopStatusbarRepaint == 0 && PostStatusbarRepaint)
        {
            PostStatusbarRepaint = FALSE;
            PostMessage(MainWindow->HWindow, WM_USER_REPAINTSTATUSBARS, 0, 0);
        }
    }
    else
    {
        TRACE_E("Incorrect call to EndStopStatusbarRepaint().");
        StopStatusbarRepaint = 0;
    }
}

// ****************************************************************************

BOOL CanChangeDirectory()
{
    if (ChangeDirectoryAllowed == 0)
        return TRUE;
    else
    {
        ChangeDirectoryRequest = TRUE;
        return FALSE;
    }
}

// ****************************************************************************

void AllowChangeDirectory(BOOL allow)
{
    if (allow)
    {
        if (ChangeDirectoryAllowed == 0)
        {
            TRACE_E("Incorrect call to AllowChangeDirectory().");
            return;
        }
        if (--ChangeDirectoryAllowed == 0)
        {
            if (ChangeDirectoryRequest)
                SetCurrentDirectoryToSystem();
            ChangeDirectoryRequest = FALSE;
        }
    }
    else
        ChangeDirectoryAllowed++;
}

// ****************************************************************************

void SetCurrentDirectoryToSystem()
{
    char buf[MAX_PATH];
    GetSystemDirectory(buf, MAX_PATH);
    SetCurrentDirectory(buf);
}

// ****************************************************************************

void _RemoveTemporaryDir(const char* dir)
{
    char path[MAX_PATH + 2];
    WIN32_FIND_DATA file;
    strcpy(path, dir);
    char* end = path + strlen(path);
    if (*(end - 1) != '\\')
        *end++ = '\\';
    strcpy(end, "*");
    HANDLE find = HANDLES_Q(FindFirstFile(path, &file));
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (file.cFileName[0] != 0 && strcmp(file.cFileName, "..") && strcmp(file.cFileName, ".") &&
                (end - path) + strlen(file.cFileName) < MAX_PATH)
            {
                strcpy(end, file.cFileName);
                ClearReadOnlyAttr(path, file.dwFileAttributes);
                if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    _RemoveTemporaryDir(path);
                else
                    DeleteFile(path);
            }
        } while (FindNextFile(find, &file));
        HANDLES(FindClose(find));
    }
    *(end - 1) = 0;
    RemoveDirectory(path);
}

void RemoveTemporaryDir(const char* dir)
{
    CALL_STACK_MESSAGE2("RemoveTemporaryDir(%s)", dir);
    SetCurrentDirectory(dir); // aby to lepe odsejpalo (system ma rad cur-dir)
    if (strlen(dir) < MAX_PATH)
        _RemoveTemporaryDir(dir);
    SetCurrentDirectoryToSystem(); // musime z nej odejit, jinak nepujde smazat

    ClearReadOnlyAttr(dir);
    RemoveDirectory(dir);
}

// ****************************************************************************

void _RemoveEmptyDirs(const char* dir)
{
    char path[MAX_PATH + 2];
    WIN32_FIND_DATA file;
    strcpy(path, dir);
    char* end = path + strlen(path);
    if (*(end - 1) != '\\')
        *end++ = '\\';
    strcpy(end, "*");
    HANDLE find = HANDLES_Q(FindFirstFile(path, &file));
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (file.cFileName[0] != 0 && strcmp(file.cFileName, "..") && strcmp(file.cFileName, "."))
            {
                if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    (end - path) + strlen(file.cFileName) < MAX_PATH)
                {
                    strcpy(end, file.cFileName);
                    ClearReadOnlyAttr(path, file.dwFileAttributes);
                    _RemoveEmptyDirs(path);
                }
            }
        } while (FindNextFile(find, &file));
        HANDLES(FindClose(find));
    }
    *(end - 1) = 0;
    RemoveDirectory(path);
}

void RemoveEmptyDirs(const char* dir)
{
    CALL_STACK_MESSAGE2("RemoveEmptyDirs(%s)", dir);
    SetCurrentDirectory(dir); // aby to lepe odsejpalo (system ma rad cur-dir)
    if (strlen(dir) < MAX_PATH)
        _RemoveEmptyDirs(dir);
    SetCurrentDirectoryToSystem(); // musime z nej odejit, jinak nepujde smazat

    ClearReadOnlyAttr(dir);
    RemoveDirectory(dir);
}

// ****************************************************************************

BOOL CheckAndCreateDirectory(const char* dir, HWND parent, BOOL quiet, char* errBuf,
                             int errBufSize, char* newDir, BOOL noRetryButton,
                             BOOL manualCrDir)
{
    CALL_STACK_MESSAGE2("CheckAndCreateDirectory(%s)", dir);
AGAIN:
    if (parent == NULL)
        parent = MainWindow->HWindow;
    if (newDir != NULL)
        newDir[0] = 0;
    int dirLen = (int)strlen(dir);
    if (dirLen >= MAX_PATH) // too long name
    {
        if (errBuf != NULL)
            strncpy_s(errBuf, errBufSize, LoadStr(IDS_TOOLONGNAME), _TRUNCATE);
        else
            SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    DWORD attrs = SalGetFileAttributes(dir);
    char buf[MAX_PATH + 200];
    char name[MAX_PATH];
    if (attrs == 0xFFFFFFFF) // asi neexistuje, umoznime jej vytvorit
    {
        char root[MAX_PATH];
        GetRootPath(root, dir);
        if (dirLen <= (int)strlen(root)) // dir je root adresar
        {
            sprintf(buf, LoadStr(IDS_CREATEDIRFAILED), dir);
            if (errBuf != NULL)
                strncpy_s(errBuf, errBufSize, buf, _TRUNCATE);
            else
                SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        int msgBoxRet = IDCANCEL;
        if (!quiet)
        {
            // pokud to user nepotlacil, zobrazime info o neexistenci adresare
            if (Configuration.CnfrmCreateDir)
            {
                char title[100];
                char text[MAX_PATH + 500];
                char checkText[200];
                sprintf(title, LoadStr(IDS_QUESTION));
                sprintf(text, LoadStr(IDS_CREATEDIRECTORY), dir);
                sprintf(checkText, LoadStr(IDS_DONTSHOWAGAINCD));
                BOOL dontShow = !Configuration.CnfrmCreateDir;

                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = parent;
                params.Flags = MSGBOXEX_OKCANCEL | MSGBOXEX_ICONQUESTION | MSGBOXEX_HINT;
                params.Caption = title;
                params.Text = text;
                params.CheckBoxText = checkText;
                params.CheckBoxValue = &dontShow;
                msgBoxRet = SalMessageBoxEx(&params);

                Configuration.CnfrmCreateDir = !dontShow;
            }
            else
                msgBoxRet = IDOK;
        }
        if (quiet || msgBoxRet == IDOK)
        {
            strcpy(name, dir);
            char* s;
            while (1) // najdeme prvni existujici adresar
            {
                s = strrchr(name, '\\');
                if (s == NULL)
                {
                    sprintf(buf, LoadStr(IDS_CREATEDIRFAILED), dir);
                    if (errBuf != NULL)
                        strncpy_s(errBuf, errBufSize, buf, _TRUNCATE);
                    else
                        SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    return FALSE;
                }
                if (s - name > (int)strlen(root))
                    *s = 0;
                else
                {
                    strcpy(name, root);
                    break; // uz jsme na root-adresari
                }
                attrs = SalGetFileAttributes(name);
                if (attrs != 0xFFFFFFFF) // jmeno existuje
                {
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        break; // budeme stavet od tohoto adresare
                    else       // je to soubor, to by neslo ...
                    {
                        sprintf(buf, LoadStr(IDS_NAMEUSEDFORFILE), name);
                        if (errBuf != NULL)
                            strncpy_s(errBuf, errBufSize, buf, _TRUNCATE);
                        else
                        {
                            if (noRetryButton)
                            {
                                CFileErrorDlg dlg(parent, LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(ERROR_ALREADY_EXISTS), FALSE, IDD_ERROR3);
                                dlg.Execute();
                            }
                            else
                            {
                                CFileErrorDlg dlg(parent, LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(ERROR_ALREADY_EXISTS), TRUE);
                                if (dlg.Execute() == IDRETRY)
                                    goto AGAIN;
                                // SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                        }
                        return FALSE;
                    }
                }
            }
            s = name + strlen(name) - 1;
            if (*s != '\\')
            {
                *++s = '\\';
                *++s = 0;
            }
            const char* st = dir + strlen(name);
            if (*st == '\\')
                st++;
            int len = (int)strlen(name);
            BOOL first = TRUE;
            while (*st != 0)
            {
                BOOL invalidName = manualCrDir && *st <= ' '; // mezery na zacatku jmena vytvareneho adresare jsou nezadouci jen pri rucnim vytvareni (Windows to umi, ale je to potencialne nebezpecne)
                const char* slash = strchr(st, '\\');
                if (slash == NULL)
                    slash = st + strlen(st);
                memcpy(name + len, st, slash - st);
                name[len += (int)(slash - st)] = 0;
                if (name[len - 1] <= ' ' || name[len - 1] == '.')
                    invalidName = TRUE; // mezery a tecky na konci jmena vytvareneho adresare jsou nezadouci
            AGAIN2:
                if (invalidName || !CreateDirectory(name, NULL))
                {
                    DWORD lastErr = invalidName ? ERROR_INVALID_NAME : GetLastError();
                    sprintf(buf, LoadStr(IDS_CREATEDIRFAILED), name);
                    if (errBuf != NULL)
                        strncpy_s(errBuf, errBufSize, buf, _TRUNCATE);
                    else
                    {
                        if (noRetryButton)
                        {
                            CFileErrorDlg dlg(parent, LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(lastErr), FALSE, IDD_ERROR3);
                            dlg.Execute();
                        }
                        else
                        {
                            CFileErrorDlg dlg(parent, LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(lastErr), TRUE);
                            if (dlg.Execute() == IDRETRY)
                                goto AGAIN2;
                            //              SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                    }
                    return FALSE;
                }
                else
                {
                    if (first && newDir != NULL)
                        strcpy(newDir, name);
                    first = FALSE;
                }
                name[len++] = '\\';
                if (*slash == '\\')
                    slash++;
                st = slash;
            }
            return TRUE;
        }
        return FALSE;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return TRUE;
    else // soubor, to by neslo ...
    {
        sprintf(buf, LoadStr(IDS_NAMEUSEDFORFILE), dir);
        if (errBuf != NULL)
            strncpy_s(errBuf, errBufSize, buf, _TRUNCATE);
        else
        {
            if (noRetryButton)
            {
                CFileErrorDlg dlg(parent, LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(ERROR_ALREADY_EXISTS), FALSE, IDD_ERROR3);
                dlg.Execute();
            }
            else
            {
                CFileErrorDlg dlg(parent, LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(ERROR_ALREADY_EXISTS), TRUE);
                if (dlg.Execute() == IDRETRY)
                    goto AGAIN;
                //        SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        return FALSE;
    }
}

//
// ****************************************************************************
// CToolTipWindow
//

LRESULT
CToolTipWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == TTM_WINDOWFROMPOINT)
        return (LRESULT)ToolWindow;
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CPathHistoryItem
//

CPathHistoryItem::CPathHistoryItem(int type, const char* pathOrArchiveOrFSName,
                                   const char* archivePathOrFSUserPart, HICON hIcon,
                                   CPluginFSInterfaceAbstract* pluginFS)
{
    Type = type;
    HIcon = hIcon;
    PluginFS = NULL;

    TopIndex = -1;
    FocusedName = NULL;

    if (Type == 0) // disk
    {
        char root[MAX_PATH];
        GetRootPath(root, pathOrArchiveOrFSName);
        const char* e = pathOrArchiveOrFSName + strlen(pathOrArchiveOrFSName);
        if ((int)strlen(root) < e - pathOrArchiveOrFSName || // neni to root cesta
            pathOrArchiveOrFSName[0] == '\\')                // je to UNC cesta
        {
            if (*(e - 1) == '\\')
                e--;
            PathOrArchiveOrFSName = (char*)malloc((e - pathOrArchiveOrFSName) + 1);
            if (PathOrArchiveOrFSName != NULL)
            {
                memcpy(PathOrArchiveOrFSName, pathOrArchiveOrFSName, e - pathOrArchiveOrFSName);
                PathOrArchiveOrFSName[e - pathOrArchiveOrFSName] = 0;
            }
        }
        else // je to normal root cesta (c:\)
        {
            PathOrArchiveOrFSName = DupStr(root);
        }
        if (PathOrArchiveOrFSName == NULL)
        {
            TRACE_E(LOW_MEMORY);
            if (PathOrArchiveOrFSName != NULL)
                free(PathOrArchiveOrFSName);
            PathOrArchiveOrFSName = NULL;
            HIcon = NULL;
        }
        ArchivePathOrFSUserPart = NULL;
    }
    else
    {
        if (Type == 1 || Type == 2) // archiv nebo FS (jen kopie obou stringu)
        {
            if (Type == 2)
                PluginFS = pluginFS;
            PathOrArchiveOrFSName = DupStr(pathOrArchiveOrFSName);
            ArchivePathOrFSUserPart = DupStr(archivePathOrFSUserPart);
            if (PathOrArchiveOrFSName == NULL || ArchivePathOrFSUserPart == NULL)
            {
                TRACE_E(LOW_MEMORY);
                if (PathOrArchiveOrFSName != NULL)
                    free(PathOrArchiveOrFSName);
                if (ArchivePathOrFSUserPart != NULL)
                    free(ArchivePathOrFSUserPart);
                PathOrArchiveOrFSName = NULL;
                ArchivePathOrFSUserPart = NULL;
                HIcon = NULL;
            }
        }
        else
            TRACE_E("CPathHistoryItem::CPathHistoryItem(): unknown 'type'");
    }
}

CPathHistoryItem::~CPathHistoryItem()
{
    if (FocusedName != NULL)
        free(FocusedName);
    if (PathOrArchiveOrFSName != NULL)
        free(PathOrArchiveOrFSName);
    if (ArchivePathOrFSUserPart != NULL)
        free(ArchivePathOrFSUserPart);
    if (HIcon != NULL)
        HANDLES(DestroyIcon(HIcon));
}

void CPathHistoryItem::ChangeData(int topIndex, const char* focusedName)
{
    TopIndex = topIndex;
    if (FocusedName != NULL)
    {
        if (focusedName != NULL && strcmp(FocusedName, focusedName) == 0)
            return; // neni zmena -> konec
        free(FocusedName);
    }
    if (focusedName != NULL)
        FocusedName = DupStr(focusedName);
    else
        FocusedName = NULL;
}

void CPathHistoryItem::GetPath(char* buffer, int bufferSize)
{
    char* origBuffer = buffer;
    if (bufferSize == 0)
        return;
    if (PathOrArchiveOrFSName == NULL)
    {
        buffer[0] = 0;
        return;
    }
    int l = (int)strlen(PathOrArchiveOrFSName) + 1;
    if (l > bufferSize)
        l = bufferSize;
    memcpy(buffer, PathOrArchiveOrFSName, l - 1);
    buffer[l - 1] = 0;
    if (Type == 1 || Type == 2) // archiv nebo FS
    {
        buffer += l - 1;
        bufferSize -= l - 1;
        char* s = ArchivePathOrFSUserPart;
        if (*s != 0 || Type == 2)
        {
            if (bufferSize >= 2) // doplnime '\\' nebo ':'
            {
                *buffer++ = Type == 1 ? '\\' : ':';
                *buffer = 0;
                bufferSize--;
            }
            l = (int)strlen(s) + 1;
            if (l > bufferSize)
                l = bufferSize;
            memcpy(buffer, s, l - 1);
            buffer[l - 1] = 0;
        }
    }

    // musime zdvojit vsechny '&' jinak z nich budou podtrzeni
    DuplicateAmpersands(origBuffer, bufferSize);
}

HICON
CPathHistoryItem::GetIcon()
{
    return HIcon;
}

BOOL DuplicateAmpersands(char* buffer, int bufferSize, BOOL skipFirstAmpersand)
{
    if (buffer == NULL)
    {
        TRACE_E("Unexpected situation (1) in DuplicateAmpersands()");
        return FALSE;
    }
    char* s = buffer;
    int l = (int)strlen(buffer);
    if (l >= bufferSize)
    {
        TRACE_E("Unexpected situation (2) in DuplicateAmpersands()");
        return FALSE;
    }
    BOOL ret = TRUE;
    BOOL first = TRUE;
    while (*s != 0)
    {
        if (*s == '&')
        {
            if (!(skipFirstAmpersand && first))
            {
                if (l + 1 < bufferSize)
                {
                    memmove(s + 1, s, l - (s - buffer) + 1); // zdvojime '&'
                    l++;
                    s++;
                }
                else // nevejde se, orizneme buffer
                {
                    ret = FALSE;
                    memmove(s + 1, s, l - (s - buffer)); // zdvojime '&', orizneme o znak
                    buffer[l] = 0;
                    s++;
                }
            }
            first = FALSE;
        }
        s++;
    }
    return ret;
}

void RemoveAmpersands(char* text)
{
    if (text == NULL)
    {
        TRACE_E("Unexpected situation in RemoveAmpersands().");
        return;
    }
    char* s = text;
    while (*s != 0 && *s != '&')
        s++;
    if (*s != 0)
    {
        char* d = s;
        while (*s != 0)
        {
            if (*s != '&')
                *d++ = *s++;
            else
            {
                if (*(s + 1) == '&')
                    *d++ = *s++; // dvojice "&&" -> nahradime '&'
                s++;
            }
        }
        *d = 0;
    }
}

BOOL CPathHistoryItem::Execute(CFilesWindow* panel)
{
    BOOL ret = TRUE; // standardne vracime uspech
    char errBuf[MAX_PATH + 200];
    if (PathOrArchiveOrFSName != NULL) // jsou platna data
    {
        int failReason;
        BOOL clear = TRUE;
        if (Type == 0) // disk
        {
            if (!panel->ChangePathToDisk(panel->HWindow, PathOrArchiveOrFSName, TopIndex, FocusedName, NULL,
                                         TRUE, FALSE, FALSE, &failReason))
            {
                if (failReason == CHPPFR_CANNOTCLOSEPATH)
                {
                    ret = FALSE;   // zustavame na miste
                    clear = FALSE; // zadny skok, neni treba mazat top-indexy
                }
            }
        }
        else
        {
            if (Type == 1) // archiv
            {
                if (!panel->ChangePathToArchive(PathOrArchiveOrFSName, ArchivePathOrFSUserPart, TopIndex,
                                                FocusedName, FALSE, NULL, TRUE, &failReason, FALSE, FALSE, TRUE))
                {
                    if (failReason == CHPPFR_CANNOTCLOSEPATH)
                    {
                        ret = FALSE;   // zustavame na miste
                        clear = FALSE; // zadny skok, neni treba mazat top-indexy
                    }
                    else
                    {
                        if (failReason == CHPPFR_SHORTERPATH || failReason == CHPPFR_FILENAMEFOCUSED)
                        {
                            sprintf(errBuf, LoadStr(IDS_PATHINARCHIVENOTFOUND), ArchivePathOrFSUserPart);
                            SalMessageBox(panel->HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                          MB_OK | MB_ICONEXCLAMATION);
                        }
                    }
                }
            }
            else
            {
                if (Type == 2) // FS
                {
                    BOOL done = FALSE;
                    // pokud je znamy FS interface, ve kterem byla cesta naposledy otevrena, zkusime
                    // ho najit mezi odpojenymi a pouzit
                    if (MainWindow != NULL && PluginFS != NULL && // pokud je znamy FS interface
                        (!panel->Is(ptPluginFS) ||                // a pokud neni prave v panelu
                         !panel->GetPluginFS()->Contains(PluginFS)))
                    {
                        CDetachedFSList* list = MainWindow->DetachedFSList;
                        int i;
                        for (i = 0; i < list->Count; i++)
                        {
                            if (list->At(i)->Contains(PluginFS))
                            {
                                done = TRUE;
                                // zkusime zmenu na pozadovanou cestu (posledne tam byla, nemusime testovat IsOurPath),
                                // zaroven pripojime odpojene FS
                                if (!panel->ChangePathToDetachedFS(i, TopIndex, FocusedName, TRUE, &failReason,
                                                                   PathOrArchiveOrFSName, ArchivePathOrFSUserPart))
                                {
                                    if (failReason == CHPPFR_CANNOTCLOSEPATH)
                                    {
                                        ret = FALSE;   // zustavame na miste
                                        clear = FALSE; // zadny skok, neni treba mazat top-indexy
                                    }
                                }

                                break; // konec, dalsi shoda s PluginFS uz nepripada v uvahu
                            }
                        }
                    }

                    // pokud predchozi cast neuspela a cestu nelze vylistovat ve FS interfacu v panelu,
                    // zkusime najit odpojeny FS interface, ktery by cestu umel vylistovat (aby se
                    // zbytecne neoteviral novy FS)
                    int fsNameIndex;
                    BOOL convertPathToInternalDummy = FALSE;
                    if (!done && MainWindow != NULL &&
                        (!panel->Is(ptPluginFS) || // FS interface v panelu cestu neumi vylistovat
                         !panel->GetPluginFS()->Contains(PluginFS) &&
                             !panel->IsPathFromActiveFS(PathOrArchiveOrFSName, ArchivePathOrFSUserPart,
                                                        fsNameIndex, convertPathToInternalDummy)))
                    {
                        CDetachedFSList* list = MainWindow->DetachedFSList;
                        int i;
                        for (i = 0; i < list->Count; i++)
                        {
                            if (list->At(i)->IsPathFromThisFS(PathOrArchiveOrFSName, ArchivePathOrFSUserPart))
                            {
                                done = TRUE;
                                // zkusime zmenu na pozadovanou cestu, zaroven pripojime odpojene FS
                                if (!panel->ChangePathToDetachedFS(i, TopIndex, FocusedName, TRUE, &failReason,
                                                                   PathOrArchiveOrFSName, ArchivePathOrFSUserPart))
                                {
                                    if (failReason == CHPPFR_SHORTERPATH) // temer uspech (cesta je jen zkracena) (CHPPFR_FILENAMEFOCUSED tu nehrozi)
                                    {                                     // obnovime zaznam o FS interfacu
                                        if (panel->Is(ptPluginFS))
                                            PluginFS = panel->GetPluginFS()->GetInterface();
                                    }
                                    if (failReason == CHPPFR_CANNOTCLOSEPATH)
                                    {
                                        ret = FALSE;   // zustavame na miste
                                        clear = FALSE; // zadny skok, neni treba mazat top-indexy
                                    }
                                }
                                else // uplny uspech
                                {    // obnovime zaznam o FS interfacu
                                    if (panel->Is(ptPluginFS))
                                        PluginFS = panel->GetPluginFS()->GetInterface();
                                }

                                break;
                            }
                        }
                    }

                    // kdyz to nejde jinak, otevreme novy FS interface nebo jen zmenime cestu na aktivnim FS interfacu
                    if (!done)
                    {
                        if (!panel->ChangePathToPluginFS(PathOrArchiveOrFSName, ArchivePathOrFSUserPart, TopIndex,
                                                         FocusedName, FALSE, 2, NULL, TRUE, &failReason))
                        {
                            if (failReason == CHPPFR_SHORTERPATH ||   // temer uspech (cesta je jen zkracena)
                                failReason == CHPPFR_FILENAMEFOCUSED) // temer uspech (cesta se jen zmenila na soubor a ten byl vyfokusen)
                            {                                         // obnovime zaznam o FS interfacu
                                if (panel->Is(ptPluginFS))
                                    PluginFS = panel->GetPluginFS()->GetInterface();
                            }
                            if (failReason == CHPPFR_CANNOTCLOSEPATH)
                            {
                                ret = FALSE;   // zustavame na miste
                                clear = FALSE; // zadny skok, neni treba mazat top-indexy
                            }
                        }
                        else // uplny uspech
                        {    // obnovime zaznam o FS interfacu
                            if (panel->Is(ptPluginFS))
                                PluginFS = panel->GetPluginFS()->GetInterface();
                        }
                    }
                }
            }
        }
        if (clear)
            panel->TopIndexMem.Clear(); // dlouhy skok
    }
    UpdateWindow(MainWindow->HWindow);
    return ret;
}

BOOL CPathHistoryItem::IsTheSamePath(CPathHistoryItem& item, CPluginFSInterfaceEncapsulation* curPluginFS)
{
    char buf1[2 * MAX_PATH];
    char buf2[2 * MAX_PATH];
    if (Type == item.Type)
    {
        if (Type == 0) // disk
        {
            GetPath(buf1, 2 * MAX_PATH);
            item.GetPath(buf2, 2 * MAX_PATH);
            if (StrICmp(buf1, buf2) == 0)
                return TRUE;
        }
        else
        {
            if (Type == 1) // archiv
            {
                if (StrICmp(PathOrArchiveOrFSName, item.PathOrArchiveOrFSName) == 0 &&  // soubor archivu je "case-insensitive"
                    strcmp(ArchivePathOrFSUserPart, item.ArchivePathOrFSUserPart) == 0) // cesta v archivu je "case-sensitive"
                {
                    return TRUE;
                }
            }
            else
            {
                if (Type == 2) // FS
                {
                    if (StrICmp(PathOrArchiveOrFSName, item.PathOrArchiveOrFSName) == 0) // fs-name je "case-insensitive"
                    {
                        if (strcmp(ArchivePathOrFSUserPart, item.ArchivePathOrFSUserPart) == 0) // fs-user-part je "case-sensitive"
                            return TRUE;
                        if (curPluginFS != NULL && // resime jeste pripad, kdy jsou obe fs-user-part shodne z duvodu, ze pro ne FS vrati TRUE z IsCurrentPath (obecne bysme museli zavest metodu pro porovnani dvou fs-user-part, coz se mi ale jen kvuli historiim nechce, treba casem...)
                            StrICmp(PathOrArchiveOrFSName, curPluginFS->GetPluginFSName()) == 0)
                        {
                            int fsNameInd = curPluginFS->GetPluginFSNameIndex();
                            if (curPluginFS->IsCurrentPath(fsNameInd, fsNameInd, ArchivePathOrFSUserPart) &&
                                curPluginFS->IsCurrentPath(fsNameInd, fsNameInd, item.ArchivePathOrFSUserPart))
                            {
                                return TRUE;
                            }
                        }
                    }
                }
            }
        }
    }
    return FALSE;
}

//
// ****************************************************************************
// CPathHistory
//

CPathHistory::CPathHistory(BOOL dontChangeForwardIndex) : Paths(10, 5)
{
    ForwardIndex = -1;
    Lock = FALSE;
    DontChangeForwardIndex = dontChangeForwardIndex;
    NewItem = NULL;
}

CPathHistory::~CPathHistory()
{
    if (NewItem != NULL)
        delete NewItem;
}

void CPathHistory::ClearHistory()
{
    Paths.DestroyMembers();

    if (NewItem != NULL)
    {
        delete NewItem;
        NewItem = NULL;
    }
}

void CPathHistory::ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs)
{
    if (NewItem != NULL && NewItem->PluginFS == fs)
    {
        NewItem->PluginFS = NULL; // fs byl prave zavren -> NULLovani
    }
    int i;
    for (i = 0; i < Paths.Count; i++)
    {
        CPathHistoryItem* item = Paths[i];
        if (item->Type == 2 && item->PluginFS == fs)
            item->PluginFS = NULL; // fs byl prave zavren -> NULLovani
    }
}

void CPathHistory::FillBackForwardPopupMenu(CMenuPopup* popup, BOOL forward)
{
    // IDcka item musi byt v intervalu <1..?>
    char buffer[2 * MAX_PATH];

    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING;
    mii.Type = MENU_TYPE_STRING;

    if (forward)
    {
        if (ForwardIndex != -1)
        {
            int id = 1;
            int i;
            for (i = ForwardIndex; i < Paths.Count; i++)
            {
                Paths[i]->GetPath(buffer, 2 * MAX_PATH);
                mii.String = buffer;
                mii.ID = id++;
                popup->InsertItem(-1, TRUE, &mii);
            }
        }
    }
    else
    {
        int id = 2;
        int count = (ForwardIndex == -1) ? Paths.Count : ForwardIndex;
        int i;
        for (i = count - 2; i >= 0; i--)
        {
            Paths[i]->GetPath(buffer, 2 * MAX_PATH);
            mii.String = buffer;
            mii.ID = id++;
            popup->InsertItem(-1, TRUE, &mii);
        }
    }
}

void CPathHistory::FillHistoryPopupMenu(CMenuPopup* popup, DWORD firstID, int maxCount,
                                        BOOL separator)
{
    char buffer[2 * MAX_PATH];

    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_ICON;
    mii.Type = MENU_TYPE_STRING;

    int firstIndex = popup->GetItemCount();

    int added = 0; // pocet pridanych polozek

    int id = firstID;
    int count = (ForwardIndex == -1) ? Paths.Count : ForwardIndex;
    int i;
    for (i = count - 1; i >= 0; i--)
    {
        if (maxCount != -1 && added >= maxCount)
            break;
        Paths[i]->GetPath(buffer, 2 * MAX_PATH);
        mii.String = buffer;
        mii.HIcon = Paths[i]->GetIcon();
        mii.ID = id++;
        popup->InsertItem(-1, TRUE, &mii);
        added++;
    }

    if (added > 0)
        popup->AssignHotKeys();

    if (separator && added > 0)
    {
        // vlozime separator
        mii.Mask = MENU_MASK_TYPE;
        mii.Type = MENU_TYPE_SEPARATOR;
        popup->InsertItem(firstIndex, TRUE, &mii);
    }
}

void CPathHistory::Execute(int index, BOOL forward, CFilesWindow* panel, BOOL allItems, BOOL removeItem)
{
    if (Lock)
        return;

    CPathHistoryItem* item = NULL; // pokud mame cestu vyradit, ulozime si na ni ukazatel pro dohledani

    BOOL change = TRUE;
    if (forward)
    {
        if (HasForward())
        {
            if (ForwardIndex + index - 1 < Paths.Count)
            {
                Lock = TRUE;
                item = Paths[ForwardIndex + index - 1];
                change = item->Execute(panel);
                if (!change)
                    item = NULL; // nepodarilo se zmenit cestu => nechame ji v historii
                Lock = FALSE;
            }
            if (change && !DontChangeForwardIndex)
                ForwardIndex = ForwardIndex + index;
            if (ForwardIndex >= Paths.Count)
                ForwardIndex = -1;
        }
    }
    else
    {
        index--; // z duvodu pocatku cislovani od 2 v FillPopupMenu
        if (HasBackward() || allItems && HasPaths())
        {
            int count = ((ForwardIndex == -1) ? Paths.Count : ForwardIndex) - 1;
            if (count - index >= 0) // mame kam jit (neni to posledni polozka)
            {
                if (count - index < Paths.Count)
                {
                    Lock = TRUE;
                    item = Paths[count - index];
                    change = item->Execute(panel);
                    if (!change)
                        item = NULL; // nepodarilo se zmenit cestu => nechame ji v historii
                    Lock = FALSE;
                }
                if (change && !DontChangeForwardIndex)
                    ForwardIndex = count - index + 1;
            }
        }
    }
    IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych

    if (NewItem != NULL)
    {
        AddPathUnique(NewItem->Type, NewItem->PathOrArchiveOrFSName, NewItem->ArchivePathOrFSUserPart,
                      NewItem->HIcon, NewItem->PluginFS, NULL);
        NewItem->HIcon = NULL; // zodpovednost za destrukci ikony si prebrala metoda AddPathUnique
        delete NewItem;
        NewItem = NULL;
    }
    if (removeItem && item != NULL)
    {
        if (DontChangeForwardIndex)
        {
            // vyradime execlou polozku ze seznamu
            Lock = TRUE;
            int i;
            for (i = 0; i < Paths.Count; i++)
            {
                if (Paths[i] == item)
                {
                    Paths.Delete(i);
                    break;
                }
            }
            Lock = FALSE;
        }
        else
        {
            TRACE_E("Path removing is not supported for this setting.");
        }
    }
}

void CPathHistory::ChangeActualPathData(int type, const char* pathOrArchiveOrFSName,
                                        const char* archivePathOrFSUserPart,
                                        CPluginFSInterfaceAbstract* pluginFS,
                                        CPluginFSInterfaceEncapsulation* curPluginFS,
                                        int topIndex, const char* focusedName)
{
    if (Paths.Count > 0)
    {
        CPathHistoryItem n(type, pathOrArchiveOrFSName, archivePathOrFSUserPart, NULL, pluginFS);
        CPathHistoryItem* n2 = NULL;
        if (ForwardIndex != -1)
        {
            if (ForwardIndex > 0)
                n2 = Paths[ForwardIndex - 1];
            else
                TRACE_E("Unexpected situation in CPathHistory::ChangeActualPathData");
        }
        else
            n2 = Paths[Paths.Count - 1];

        if (n2 != NULL && n.IsTheSamePath(*n2, curPluginFS)) // stejne cesty -> zmenime data
            n2->ChangeData(topIndex, focusedName);
    }
}

void CPathHistory::RemoveActualPath(int type, const char* pathOrArchiveOrFSName,
                                    const char* archivePathOrFSUserPart,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    CPluginFSInterfaceEncapsulation* curPluginFS)
{
    if (Lock)
        return;
    if (Paths.Count > 0)
    {
        if (ForwardIndex == -1)
        {
            CPathHistoryItem n(type, pathOrArchiveOrFSName, archivePathOrFSUserPart, NULL, pluginFS);
            CPathHistoryItem* n2 = Paths[Paths.Count - 1];
            if (n.IsTheSamePath(*n2, curPluginFS)) // stejne cesty -> smazeme zaznam
                Paths.Delete(Paths.Count - 1);
        }
        else
            TRACE_E("Unexpected situation in CPathHistory::RemoveActualPath(): ForwardIndex != -1");
    }
}

void CPathHistory::AddPath(int type, const char* pathOrArchiveOrFSName, const char* archivePathOrFSUserPart,
                           CPluginFSInterfaceAbstract* pluginFS, CPluginFSInterfaceEncapsulation* curPluginFS)
{
    if (Lock)
        return;

    CPathHistoryItem* n = new CPathHistoryItem(type, pathOrArchiveOrFSName, archivePathOrFSUserPart,
                                               NULL, pluginFS);
    if (n == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }
    if (Paths.Count > 0)
    {
        CPathHistoryItem* n2 = NULL;
        if (ForwardIndex != -1)
        {
            if (ForwardIndex > 0)
                n2 = Paths[ForwardIndex - 1];
            else
                TRACE_E("Unexpected situation in CPathHistory::AddPath");
        }
        else
            n2 = Paths[Paths.Count - 1];

        if (n2 != NULL && n->IsTheSamePath(*n2, curPluginFS))
        {
            delete n;
            return; // stejne cesty -> neni co delat
        }
    }

    // cestu je opravdu potreba pridat ...
    if (ForwardIndex != -1)
    {
        while (Paths.IsGood() && ForwardIndex < Paths.Count)
        {
            Paths.Delete(ForwardIndex);
        }
        ForwardIndex = -1;
    }
    while (Paths.IsGood() && Paths.Count > PATH_HISTORY_SIZE)
    {
        Paths.Delete(0);
    }
    Paths.Add(n);
    if (!Paths.IsGood())
    {
        delete n;
        Paths.ResetState();
    }
}

void CPathHistory::AddPathUnique(int type, const char* pathOrArchiveOrFSName, const char* archivePathOrFSUserPart,
                                 HICON hIcon, CPluginFSInterfaceAbstract* pluginFS,
                                 CPluginFSInterfaceEncapsulation* curPluginFS)
{
    CPathHistoryItem* n = new CPathHistoryItem(type, pathOrArchiveOrFSName, archivePathOrFSUserPart,
                                               hIcon, pluginFS);
    if (Lock)
    {
        if (NewItem != NULL)
        {
            TRACE_E("Unexpected situation in CPathHistory::AddPathUnique()");
            delete NewItem;
        }
        NewItem = n;
        return;
    }

    if (n == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (hIcon != NULL)
            HANDLES(DestroyIcon(hIcon)); // musime sestrelit ikonu
        return;
    }
    if (Paths.Count > 0)
    {
        int i;
        for (i = 0; i < Paths.Count; i++)
        {
            CPathHistoryItem* item = Paths[i];

            if (n->IsTheSamePath(*item, curPluginFS))
            {
                if (type == 2 && pluginFS != NULL)
                { // jde o FS, nahradime pluginFS (aby se cesta otevrela na poslednim FS teto cesty)
                    item->PluginFS = pluginFS;
                }
                delete n;
                if (i < Paths.Count - 1)
                {
                    // vytahneme cestu v seznamu nahoru
                    Paths.Add(item);
                    if (Paths.IsGood())
                        Paths.Detach(i); // pokud se pridani povedlo, vyhodime zdroj
                    if (!Paths.IsGood())
                        Paths.ResetState();
                }
                return; // stejne cesty -> neni co delat
            }
        }
    }

    // cestu je opravdu potreba pridat ...
    if (ForwardIndex != -1)
    {
        while (Paths.IsGood() && ForwardIndex < Paths.Count)
        {
            Paths.Delete(ForwardIndex);
        }
        ForwardIndex = -1;
    }
    while (Paths.IsGood() && Paths.Count > PATH_HISTORY_SIZE)
    {
        Paths.Delete(0);
    }
    Paths.Add(n);
    if (!Paths.IsGood())
    {
        delete n;
        Paths.ResetState();
    }
}

void CPathHistory::SaveToRegistry(HKEY hKey, const char* name, BOOL onlyClear)
{
    HKEY historyKey;
    if (CreateKey(hKey, name, historyKey))
    {
        ClearKey(historyKey);

        if (!onlyClear) // pokud se nema jen vycistit klic, ulozime hodnoty z historie
        {
            int index = 0;
            char buf[10];
            char path[2 * MAX_PATH];
            int i;
            for (i = 0; i < Paths.Count; i++)
            {
                CPathHistoryItem* item = Paths[i];
                switch (item->Type)
                {
                case 0: // disk
                {
                    strcpy(path, item->PathOrArchiveOrFSName);
                    break;
                }

                // archive & FS: pouzijeme znak ':' jako oddelovac dvou casti cesty
                // behem loadu podle tohoto znaku urcime, o jaky typ cesty se jedna
                case 1: // archive
                case 2: // FS
                {
                    strcpy(path, item->PathOrArchiveOrFSName);
                    StrNCat(path, ":", 2 * MAX_PATH);
                    if (item->ArchivePathOrFSUserPart != NULL)
                        StrNCat(path, item->ArchivePathOrFSUserPart, 2 * MAX_PATH);
                    break;
                }
                default:
                {
                    TRACE_E("CPathHistory::SaveToRegistry() uknown path type");
                    continue;
                }
                }
                itoa(index + 1, buf, 10);
                SetValue(historyKey, buf, REG_SZ, path, (DWORD)strlen(path) + 1);
                index++;
            }
        }
        CloseKey(historyKey);
    }
}

void CPathHistory::LoadFromRegistry(HKEY hKey, const char* name)
{
    ClearHistory();
    HKEY historyKey;
    if (OpenKey(hKey, name, historyKey))
    {
        char path[2 * MAX_PATH];
        char fsName[MAX_PATH];
        const char* pathOrArchiveOrFSName = path;
        const char* archivePathOrFSUserPart = NULL;
        char buf[10];
        int type;
        int i;
        for (i = 0;; i++)
        {
            itoa(i + 1, buf, 10);
            if (GetValue(historyKey, buf, REG_SZ, path, 2 * MAX_PATH))
            {
                if (strlen(path) >= 2)
                {
                    // cesta muze byt typu
                    // 0 (disk): "C:\???" nebo "\\server\???"
                    // 1 (archive): "C:\???:" nebo "\\server\???:"
                    // 2 (FS): "XY:???"
                    type = -1; // nepridavat
                    if ((path[0] == '\\' && path[1] == '\\') || path[1] == ':')
                    {
                        // jde o type==0 (disk) nebo type==1 (archive)
                        pathOrArchiveOrFSName = path;
                        char* separator = strchr(path + 2, ':');
                        if (separator == NULL)
                        {
                            type = 0;
                            archivePathOrFSUserPart = NULL;
                        }
                        else
                        {
                            *separator = 0;
                            type = 1;
                            archivePathOrFSUserPart = separator + 1;
                        }
                    }
                    else
                    {
                        // kandidat na FS path
                        if (IsPluginFSPath(path, fsName, &archivePathOrFSUserPart))
                        {
                            pathOrArchiveOrFSName = fsName;
                            type = 2;
                        }
                    }
                    if (type != -1)
                        AddPath(type, pathOrArchiveOrFSName, archivePathOrFSUserPart, NULL, NULL);
                    else
                        TRACE_E("CPathHistory::LoadFromRegistry() invalid path: " << path);
                }
            }
            else
                break;
        }
        CloseKey(historyKey);
    }
}

//
// ****************************************************************************
// CUserMenuIconData
//

CUserMenuIconData::CUserMenuIconData(const char* fileName, DWORD iconIndex, const char* umCommand)
{
    strcpy_s(FileName, fileName);
    IconIndex = iconIndex;
    strcpy_s(UMCommand, umCommand);
    LoadedIcon = NULL;
}

CUserMenuIconData::~CUserMenuIconData()
{
    if (LoadedIcon != NULL)
    {
        HANDLES(DestroyIcon(LoadedIcon));
        LoadedIcon = NULL;
    }
}

void CUserMenuIconData::Clear()
{
    FileName[0] = 0;
    IconIndex = -1;
    UMCommand[0] = 0;
    LoadedIcon = NULL;
}

//
// ****************************************************************************
// CUserMenuIconDataArr
//

HICON
CUserMenuIconDataArr::GiveIconForUMI(const char* fileName, DWORD iconIndex, const char* umCommand)
{
    CALL_STACK_MESSAGE1("CUserMenuIconDataArr::GiveIconForUMI(, ,)");
    for (int i = 0; i < Count; i++)
    {
        CUserMenuIconData* item = At(i);
        if (item->IconIndex == iconIndex &&
            strcmp(item->FileName, fileName) == 0 &&
            strcmp(item->UMCommand, umCommand) == 0)
        {
            HICON icon = item->LoadedIcon; // LoadedIcon vyNULLujem, jinak by se dealokovalo (pres DestroyIcon())
            item->Clear();                 // nechceme sesouvat pole (pri mazani) - pomale+zbytecne, tak aspon vycistime polozku, aby se rychleji preskocila pri hledani
            return icon;
        }
    }
    TRACE_E("CUserMenuIconDataArr::GiveIconForUMI(): unexpected situation: item not found!");
    return NULL;
}

//
// ****************************************************************************
// CUserMenuIconBkgndReader
//

CUserMenuIconBkgndReader::CUserMenuIconBkgndReader()
{
    SysColorsChanged = FALSE;
    HANDLES(InitializeCriticalSection(&CS));
    IconReaderThreadUID = 1;
    CurIRThreadIDIsValid = FALSE;
    CurIRThreadID = -1;
    AlreadyStopped = FALSE;
    UserMenuIconsInUse = 0;
    UserMenuIIU_BkgndReaderData = NULL;
    UserMenuIIU_ThreadID = 0;
}

CUserMenuIconBkgndReader::~CUserMenuIconBkgndReader()
{
    if (UserMenuIIU_BkgndReaderData != NULL) // tak ted uz vazne nebudou potreba, uvolnime je
    {
        delete UserMenuIIU_BkgndReaderData;
        UserMenuIIU_BkgndReaderData = NULL;
    }
    HANDLES(DeleteCriticalSection(&CS));
}

unsigned BkgndReadingIconsThreadBody(void* param)
{
    CALL_STACK_MESSAGE1("BkgndReadingIconsThreadBody()");
    SetThreadNameInVCAndTrace("UMIconReader");
    TRACE_I("Begin");
    // aby chodilo GetFileOrPathIconAux (obsahuje COM/OLE sracky)
    if (OleInitialize(NULL) != S_OK)
        TRACE_E("Error in OleInitialize.");

    CUserMenuIconDataArr* bkgndReaderData = (CUserMenuIconDataArr*)param;
    DWORD threadID = bkgndReaderData->GetIRThreadID();

    for (int i = 0; UserMenuIconBkgndReader.IsCurrentIRThreadID(threadID) && i < bkgndReaderData->Count; i++)
    {
        CUserMenuIconData* item = bkgndReaderData->At(i);
        HICON umIcon;
        if (item->FileName[0] != 0 &&
            SalGetFileAttributes(item->FileName) != INVALID_FILE_ATTRIBUTES && // test pristupnosti (misto CheckPath)
            ExtractIconEx(item->FileName, item->IconIndex, NULL, &umIcon, 1) == 1)
        {
            HANDLES_ADD(__htIcon, __hoLoadImage, umIcon); // pridame handle na 'umIcon' do HANDLES
        }
        else
        {
            umIcon = NULL;
            if (item->UMCommand[0] != 0)
            { // pro pripad, ze by minula metoda selhala - zkusim ziskat ikonku od systemu
                DWORD attrs = SalGetFileAttributes(item->UMCommand);
                if (attrs != INVALID_FILE_ATTRIBUTES) // test pristupnosti (misto CheckPath)
                {
                    umIcon = GetFileOrPathIconAux(item->UMCommand, FALSE,
                                                  (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)));
                }
            }
        }
        item->LoadedIcon = umIcon; // ulozime vysledek: nactenou ikonu nebo NULL pri chybe
    }

    UserMenuIconBkgndReader.ReadingFinished(threadID, bkgndReaderData);
    OleUninitialize();
    TRACE_I("End");
    return 0;
}

unsigned BkgndReadingIconsThreadEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return BkgndReadingIconsThreadBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread BkgndReadingIconsThread: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI BkgndReadingIconsThread(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return BkgndReadingIconsThreadEH(param);
}

void CUserMenuIconBkgndReader::StartBkgndReadingIcons(CUserMenuIconDataArr* bkgndReaderData)
{
    CALL_STACK_MESSAGE1("CUserMenuIconBkgndReader::StartBkgndReadingIcons()");
    HANDLE thread = NULL;
    HANDLES(EnterCriticalSection(&CS));
    CurIRThreadIDIsValid = FALSE;
    if (!AlreadyStopped && bkgndReaderData != NULL && bkgndReaderData->Count > 0)
    {
        DWORD newThreadID = IconReaderThreadUID++;
        bkgndReaderData->SetIRThreadID(newThreadID);
        thread = HANDLES(CreateThread(NULL, 0, BkgndReadingIconsThread, bkgndReaderData, 0, NULL));
        if (thread != NULL)
        {
            // hlavni thread jede na vyssi priorite, jestli se maji ikony cist stejne rychle
            // jako pred zavedenim cteni ve vedlejsim threadu, musime mu tez zvysit prioritu
            SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);

            bkgndReaderData = NULL; // jsou predana v threadu, nebudeme je uvolnovat zde
            CurIRThreadIDIsValid = TRUE;
            CurIRThreadID = newThreadID;
            AddAuxThread(thread); // pokud by thread nestihl dobehnout, vykillujeme ho pred uzavrenim softu
        }
        else
            TRACE_E("CUserMenuIconBkgndReader::StartBkgndReadingIcons(): unable to start thread for reading user menu icons.");
    }
    if (bkgndReaderData != NULL)
        delete bkgndReaderData;
    HANDLES(LeaveCriticalSection(&CS));

    // na kratkou chvilku se pozastavime, pokud se ikony prectou rychle, tak se vubec neukazou "simple"
    // varianty (mene blikani) + nekteri uzivatele hlasili, ze se diky soucasnemu nacitani ikon do panelu
    // dost hrube zpomalilo cteni ikon do usermenu a diky tomu se jim ikony na usermenu toolbare ukazuji
    // s velkym zpozdenim, coz je osklive, toto by tomu melo predejit (bude to proste resit jen pomale
    // nacitani usermenu ikon, coz je cilem cele tehle taskarice)
    if (thread != NULL)
    {
        //    TRACE_I("Waiting for finishing of thread for reading user menu icons...");
        BOOL finished = WaitForSingleObject(thread, 500) == WAIT_OBJECT_0;
        //    TRACE_I("Thread for reading user menu icons is " << (finished ? "FINISHED." : "still running..."));
    }
}

void CUserMenuIconBkgndReader::EndProcessing()
{
    CALL_STACK_MESSAGE1("CUserMenuIconBkgndReader::EndProcessing()");
    HANDLES(EnterCriticalSection(&CS));
    CurIRThreadIDIsValid = FALSE;
    AlreadyStopped = TRUE;
    HANDLES(LeaveCriticalSection(&CS));
}

BOOL CUserMenuIconBkgndReader::IsCurrentIRThreadID(DWORD threadID)
{
    CALL_STACK_MESSAGE2("CUserMenuIconBkgndReader::IsCurrentIRThreadID(%d)", threadID);
    HANDLES(EnterCriticalSection(&CS));
    BOOL ret = CurIRThreadIDIsValid && CurIRThreadID == threadID;
    HANDLES(LeaveCriticalSection(&CS));
    return ret;
}

BOOL CUserMenuIconBkgndReader::IsReadingIcons()
{
    CALL_STACK_MESSAGE1("CUserMenuIconBkgndReader::IsReadingIcons()");
    HANDLES(EnterCriticalSection(&CS));
    BOOL ret = CurIRThreadIDIsValid;
    HANDLES(LeaveCriticalSection(&CS));
    return ret;
}

void CUserMenuIconBkgndReader::ReadingFinished(DWORD threadID, CUserMenuIconDataArr* bkgndReaderData)
{
    CALL_STACK_MESSAGE2("CUserMenuIconBkgndReader::ReadingFinished(%d,)", threadID);
    HANDLES(EnterCriticalSection(&CS));
    BOOL ok = CurIRThreadIDIsValid && CurIRThreadID == threadID;
    HWND mainWnd = ok ? MainWindow->HWindow : NULL;
    HANDLES(LeaveCriticalSection(&CS));

    if (ok) // na tyto ikony stale jeste ceka User Menu
        PostMessage(mainWnd, WM_USER_USERMENUICONS_READY, (WPARAM)bkgndReaderData, (LPARAM)threadID);
    else
        delete bkgndReaderData;
}

void CUserMenuIconBkgndReader::BeginUserMenuIconsInUse()
{
    CALL_STACK_MESSAGE1("CUserMenuIconBkgndReader::BeginUserMenuIconsInUse()");
    HANDLES(EnterCriticalSection(&CS));
    UserMenuIconsInUse++;
    if (UserMenuIconsInUse > 2)
        TRACE_E("CUserMenuIconBkgndReader::BeginUserMenuIconsInUse(): unexpected situation, report to Petr!");
    HANDLES(LeaveCriticalSection(&CS));
}

void CUserMenuIconBkgndReader::EndUserMenuIconsInUse()
{
    CALL_STACK_MESSAGE1("CUserMenuIconBkgndReader::EndUserMenuIconsInUse()");
    HANDLES(EnterCriticalSection(&CS));
    if (UserMenuIconsInUse == 0)
        TRACE_E("CUserMenuIconBkgndReader::EndUserMenuIconsInUse(): unexpected situation, report to Petr!");
    else
    {
        UserMenuIconsInUse--;
        if (UserMenuIconsInUse == 0 && UserMenuIIU_BkgndReaderData != NULL)
        { // posledni zamek, pokud mame ulozena data ke zpracovani, posleme je
            if (CurIRThreadIDIsValid && CurIRThreadID == UserMenuIIU_ThreadID)
            {
                PostMessage(MainWindow->HWindow, WM_USER_USERMENUICONS_READY,
                            (WPARAM)UserMenuIIU_BkgndReaderData, (LPARAM)UserMenuIIU_ThreadID);
            }
            else // o data uz nikdo nestoji, jen je uvolnime
                delete UserMenuIIU_BkgndReaderData;
            UserMenuIIU_BkgndReaderData = NULL;
            UserMenuIIU_ThreadID = 0;
        }
    }
    HANDLES(LeaveCriticalSection(&CS));
}

BOOL CUserMenuIconBkgndReader::EnterCSIfCanUpdateUMIcons(CUserMenuIconDataArr** bkgndReaderData, DWORD threadID)
{
    CALL_STACK_MESSAGE2("CUserMenuIconBkgndReader::EnterCSIfCanUpdateUMIcons(, %d)", threadID);
    HANDLES(EnterCriticalSection(&CS));
    BOOL ret = FALSE;
    if (CurIRThreadIDIsValid && CurIRThreadID == threadID)
    {
        if (UserMenuIconsInUse > 0)
        {
            if (UserMenuIIU_BkgndReaderData != NULL) // pokud jsou uz nejake ulozene, uvolnime je (vlezu do cfg behem nacitani, pak zmena barev a prijde to sem podruhe)
                delete UserMenuIIU_BkgndReaderData;
            UserMenuIIU_BkgndReaderData = *bkgndReaderData;
            UserMenuIIU_ThreadID = threadID;
            *bkgndReaderData = NULL; // volajici nam timto data predal, uvolnime je casem sami
        }
        else
        {
            ret = TRUE;
            TRACE_I("Updating user menu icons to results from reading thread no. " << threadID);
        }
    }
    if (!ret)
        HANDLES(LeaveCriticalSection(&CS));
    return ret;
}

void CUserMenuIconBkgndReader::LeaveCSAfterUMIconsUpdate()
{
    CurIRThreadIDIsValid = FALSE; // timto jsou ikony predane do usermenu (IsReadingIcons() musi vracet FALSE)
    HANDLES(LeaveCriticalSection(&CS));
}

//
// ****************************************************************************
// CUserMenuItem
//

CUserMenuItem::CUserMenuItem(char* name, char* umCommand, char* arguments, char* initDir, char* icon,
                             int throughShell, int closeShell, int useWindow, int showInToolbar, CUserMenuItemType type,
                             CUserMenuIconDataArr* bkgndReaderData)
{
    UMIcon = NULL;
    ItemName = UMCommand = Arguments = InitDir = Icon = NULL;
    ThroughShell = throughShell;
    CloseShell = closeShell;
    UseWindow = useWindow;
    ShowInToolbar = showInToolbar;
    Type = type;
    Set(name, umCommand, arguments, initDir, icon);
    if (Type == umitItem || Type == umitSubmenuBegin)
        GetIconHandle(bkgndReaderData, FALSE);
}

CUserMenuItem::CUserMenuItem()
{
    UMIcon = NULL;
    ItemName = UMCommand = Arguments = InitDir = Icon = NULL;
    ThroughShell = TRUE;
    CloseShell = TRUE;
    UseWindow = TRUE;
    ShowInToolbar = TRUE;
    Type = umitItem;
    static char emptyBuffer[] = "";
    static char nameBuffer[] = "\"$(Name)\"";
    static char fullPathBuffer[] = "$(FullPath)";
    Set(emptyBuffer, emptyBuffer, nameBuffer, fullPathBuffer, emptyBuffer);
}

CUserMenuItem::CUserMenuItem(CUserMenuItem& item, CUserMenuIconDataArr* bkgndReaderData)
{
    UMIcon = NULL;
    ItemName = UMCommand = Arguments = InitDir = Icon = NULL;
    ThroughShell = item.ThroughShell;
    CloseShell = item.CloseShell;
    UseWindow = item.UseWindow;
    ShowInToolbar = item.ShowInToolbar;
    Type = item.Type;
    Set(item.ItemName, item.UMCommand, item.Arguments, item.InitDir, item.Icon);
    if (Type == umitItem)
    {
        if (bkgndReaderData == NULL) // tady jde o kopii do cfg dialogu, tam nove nactene ikony nepropagujeme (pockame az na konec dialogu)
        {
            UMIcon = DuplicateIcon(NULL, item.UMIcon); // GetIconHandle(); zbytecne brzdilo
            if (UMIcon != NULL)                        // pridame handle na 'UMIcon' do HANDLES
                HANDLES_ADD(__htIcon, __hoLoadImage, UMIcon);
        }
        else
            GetIconHandle(bkgndReaderData, FALSE);
    }
    if (Type == umitSubmenuBegin)
    {
        if (item.UMIcon != HGroupIcon)
            TRACE_E("CUserMenuItem::CUserMenuItem(): unexpected submenu item icon.");
        UMIcon = HGroupIcon;
    }
}

CUserMenuItem::~CUserMenuItem()
{
    // umitSubmenuBegin sdili jednu ikonu
    if (UMIcon != NULL && Type != umitSubmenuBegin)
        HANDLES(DestroyIcon(UMIcon));
    if (ItemName != NULL)
        free(ItemName);
    if (UMCommand != NULL)
        free(UMCommand);
    if (Arguments != NULL)
        free(Arguments);
    if (InitDir != NULL)
        free(InitDir);
    if (Icon != NULL)
        free(Icon);
}

BOOL CUserMenuItem::Set(char* name, char* umCommand, char* arguments, char* initDir, char* icon)
{
    char* itemName = (char*)malloc(strlen(name) + 1);
    char* commandName = (char*)malloc(strlen(umCommand) + 1);
    char* argumentsName = (char*)malloc(strlen(arguments) + 1);
    char* initDirName = (char*)malloc(strlen(initDir) + 1);
    char* iconName = (char*)malloc(strlen(icon) + 1);
    if (itemName == NULL || commandName == NULL ||
        argumentsName == NULL || initDirName == NULL || iconName == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    strcpy(itemName, name);
    strcpy(commandName, umCommand);
    strcpy(argumentsName, arguments);
    strcpy(initDirName, initDir);
    strcpy(iconName, icon);

    if (ItemName != NULL)
        free(ItemName);
    if (UMCommand != NULL)
        free(UMCommand);
    if (Arguments != NULL)
        free(Arguments);
    if (InitDir != NULL)
        free(InitDir);
    if (Icon != NULL)
        free(Icon);

    ItemName = itemName;
    UMCommand = commandName;
    Arguments = argumentsName;
    InitDir = initDirName;
    Icon = iconName;
    return TRUE;
}

void CUserMenuItem::SetType(CUserMenuItemType type)
{
    if (Type != type)
    {
        if (type == umitSubmenuBegin)
        {
            // prechazime na sdilenou ikonu, smazneme alokovanou
            if (UMIcon != NULL)
            {
                HANDLES(DestroyIcon(UMIcon));
                UMIcon = NULL;
            }
        }
        if (Type == umitSubmenuBegin)
            UMIcon = NULL; // odchazime ze sdilene ikony
    }
    Type = type;
}

BOOL CUserMenuItem::GetIconHandle(CUserMenuIconDataArr* bkgndReaderData, BOOL getIconsFromReader)
{
    if (Type == umitSubmenuBegin)
    {
        UMIcon = HGroupIcon;
        return TRUE;
    }

    if (UMIcon != NULL)
    {
        HANDLES(DestroyIcon(UMIcon));
        UMIcon = NULL;
    }

    if (Type == umitSeparator) // separator nema ikonku
        return TRUE;

    // pokusim se vytahnout ikonu ze specifikovaneho souboru
    char fileName[MAX_PATH];
    fileName[0] = 0;
    DWORD iconIndex = -1;
    if (MainWindow != NULL && Icon != NULL && Icon[0] != 0)
    {
        // Icon je ve formatu "nazev souboru,resID"
        // provedu rozklad
        char* iterator = Icon + strlen(Icon) - 1;
        while (iterator > Icon && *iterator != ',')
            iterator--;
        if (iterator > Icon && *iterator == ',')
        {
            strncpy(fileName, Icon, iterator - Icon);
            fileName[iterator - Icon] = 0;
            iterator++;
            iconIndex = atoi(iterator);
        }
    }

    if (bkgndReaderData == NULL && fileName[0] != 0 && // mame cist ikony hned tady
        MainWindow->GetActivePanel() != NULL &&
        MainWindow->GetActivePanel()->CheckPath(FALSE, fileName) == ERROR_SUCCESS &&
        ExtractIconEx(fileName, iconIndex, NULL, &UMIcon, 1) == 1)
    {
        HANDLES_ADD(__htIcon, __hoLoadImage, UMIcon); // pridame handle na 'UMIcon' do HANDLES
        return TRUE;
    }

    // pro pripad, ze by minula metoda selhala - zkusim ziskat ikonku od systemu
    char umCommand[MAX_PATH];
    if (MainWindow != NULL && UMCommand != NULL && UMCommand[0] != 0 &&
        ExpandCommand(MainWindow->HWindow, UMCommand, umCommand, MAX_PATH, TRUE))
    {
        while (strlen(umCommand) > 2 && CutDoubleQuotesFromBothSides(umCommand))
            ;
    }
    else
        umCommand[0] = 0;

    if (bkgndReaderData == NULL && umCommand[0] != 0 && // mame cist ikony hned tady
        MainWindow->GetActivePanel() != NULL &&
        MainWindow->GetActivePanel()->CheckPath(FALSE, umCommand) == ERROR_SUCCESS)
    {
        DWORD attrs = SalGetFileAttributes(umCommand);
        UMIcon = GetFileOrPathIconAux(umCommand, FALSE,
                                      (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)));
        if (UMIcon != NULL)
            return TRUE;
    }

    if (bkgndReaderData != NULL)
    {
        if (getIconsFromReader) // ikoy uz jsou nactene, jen prevezmeme tu spravnou
        {
            UMIcon = bkgndReaderData->GiveIconForUMI(fileName, iconIndex, umCommand);
            if (UMIcon != NULL)
                return TRUE;
        }
        else // zadame nacteni potrebne ikony
            bkgndReaderData->Add(new CUserMenuIconData(fileName, iconIndex, umCommand));
    }

    // vytahnu implicitni ikonu z shell32.dll
    UMIcon = SalLoadImage(2, 1, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags);
    return TRUE;
}

BOOL CUserMenuItem::GetHotKey(char* key)
{
    if (ItemName == NULL || Type == umitSeparator)
        return FALSE;
    char* iterator = ItemName;
    while (*iterator != 0)
    {
        if (*iterator == '&' && *(iterator + 1) != 0 && *(iterator + 1) != '&')
        {
            *key = *(iterator + 1);
            return TRUE;
        }
        iterator++;
    }
    return FALSE;
}

//
// ****************************************************************************
// CUserMenuItems
//

BOOL CUserMenuItems::LoadUMI(CUserMenuItems& source, BOOL readNewIconsOnBkgnd)
{
    CUserMenuItem* item;
    DestroyMembers();
    CUserMenuIconDataArr* bkgndReaderData = readNewIconsOnBkgnd ? new CUserMenuIconDataArr() : NULL;
    int i;
    for (i = 0; i < source.Count; i++)
    {
        item = new CUserMenuItem(*source[i], bkgndReaderData);
        Add(item);
    }
    if (readNewIconsOnBkgnd)
        UserMenuIconBkgndReader.StartBkgndReadingIcons(bkgndReaderData); // POZOR: uvolni 'bkgndReaderData'
    return TRUE;
}

int CUserMenuItems::GetSubmenuEndIndex(int index)
{
    int level = 1;
    int i;
    for (i = index + 1; i < Count; i++)
    {
        CUserMenuItem* item = At(i);
        if (item->Type == umitSubmenuBegin)
            level++;
        else
        {
            if (item->Type == umitSubmenuEnd)
            {
                level--;
                if (level == 0)
                    return i;
            }
        }
    }
    return -1;
}

//****************************************************************************
//
// Mouse Wheel support
//

// Default values for SPI_GETWHEELSCROLLLINES and
// SPI_GETWHEELSCROLLCHARS
#define DEFAULT_LINES_TO_SCROLL 3
#define DEFAULT_CHARS_TO_SCROLL 3

// handle of the old mouse hook procedure
HHOOK HOldMouseWheelHookProc = NULL;
BOOL MouseWheelMSGThroughHook = FALSE;
DWORD MouseWheelMSGTime = 0;
BOOL GotMouseWheelScrollLines = FALSE;
BOOL GotMouseWheelScrollChars = FALSE;

UINT GetMouseWheelScrollLines()
{
    static UINT uCachedScrollLines;

    // if we've already got it and we're not refreshing,
    // return what we've already got

    if (GotMouseWheelScrollLines)
        return uCachedScrollLines;

    // see if we can find the mouse window

    GotMouseWheelScrollLines = TRUE;

    static UINT msgGetScrollLines;
    static WORD nRegisteredMessage = 0;

    if (nRegisteredMessage == 0)
    {
        msgGetScrollLines = ::RegisterWindowMessage(MSH_SCROLL_LINES);
        if (msgGetScrollLines == 0)
            nRegisteredMessage = 1; // couldn't register!  never try again
        else
            nRegisteredMessage = 2; // it worked: use it
    }

    if (nRegisteredMessage == 2)
    {
        HWND hwMouseWheel = NULL;
        hwMouseWheel = FindWindow(MSH_WHEELMODULE_CLASS, MSH_WHEELMODULE_TITLE);
        if (hwMouseWheel && msgGetScrollLines)
        {
            uCachedScrollLines = (UINT)::SendMessage(hwMouseWheel, msgGetScrollLines, 0, 0);
            return uCachedScrollLines;
        }
    }

    // couldn't use the window -- try system settings
    uCachedScrollLines = DEFAULT_LINES_TO_SCROLL;
    ::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &uCachedScrollLines, 0);

    return uCachedScrollLines;
}

#define SPI_GETWHEELSCROLLCHARS 0x006C

UINT GetMouseWheelScrollChars()
{
    static UINT uCachedScrollChars;
    if (GotMouseWheelScrollChars)
        return uCachedScrollChars;

    if (WindowsVistaAndLater)
    {
        if (!SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &uCachedScrollChars, 0))
            uCachedScrollChars = DEFAULT_CHARS_TO_SCROLL;
    }
    else
        uCachedScrollChars = DEFAULT_CHARS_TO_SCROLL;
    GotMouseWheelScrollChars = TRUE;
    return uCachedScrollChars;
}

BOOL PostMouseWheelMessage(MSG* pMSG)
{
    // nechame najit okno pod kurzorem mysi
    HWND hWindow = WindowFromPoint(pMSG->pt);
    if (hWindow != NULL)
    {
        char className[101];
        className[0] = 0;
        if (GetClassName(hWindow, className, 100) != 0)
        {
            // nektere verze synaptics touchpad (napriklad na HP noteboocich) zobrazi pod kurzorem sve okno se symbolem
            // scrolovani; v takovem pripade se nebudeme snazit o routeni do "spravneho" okna pod kurzorem, protoze
            // se o to postara sam touchpad
            // https://forum.altap.cz/viewtopic.php?f=24&t=6039
            if (strcmp(className, "SynTrackCursorWindowClass") == 0 || strcmp(className, "Syn Visual Class") == 0)
            {
                //TRACE_I("Synaptics touchpad detected className="<<className);
                hWindow = pMSG->hwnd;
            }
            else
            {
                DWORD winProcessId = 0;
                GetWindowThreadProcessId(hWindow, &winProcessId);
                if (winProcessId != GetCurrentProcessId()) // WM_USER_* nema smysl posilat mimo nas proces
                    hWindow = pMSG->hwnd;
            }
        }
        else
        {
            TRACE_E("GetClassName() failed!");
            hWindow = pMSG->hwnd;
        }
        // pokud jde o ScrollBar, ktery ma parenta, postneme zpravu do parenta.
        // Scrollbars v panelech nejsou subclassnute, takze je to momentalne jediny zpusob,
        // jak se muze panel dozvedet o toceni koleckem pokud kurzor stoji nad rolovatkem.
        className[0] = 0;
        if (GetClassName(hWindow, className, 100) == 0 || StrICmp(className, "scrollbar") == 0)
        {
            HWND hParent = GetParent(hWindow);
            if (hParent != NULL)
                hWindow = hParent;
        }
        PostMessage(hWindow, pMSG->message == WM_MOUSEWHEEL ? WM_USER_MOUSEWHEEL : WM_USER_MOUSEHWHEEL, pMSG->wParam, pMSG->lParam);
    }
    return TRUE;
}

// hook procedure for mouse messages
LRESULT CALLBACK MenuWheelHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    //  CALL_STACK_MESSAGE4("MenuWheelHookProc(%d, 0x%IX, 0x%IX)", nCode, wParam, lParam);
    LRESULT retValue = 0;

    retValue = CallNextHookEx(HOldMouseWheelHookProc, nCode, wParam, lParam);

    if (nCode < 0)
        return retValue;

    MSG* pMSG = (MSG*)lParam;
    MessagesKeeper.Add(pMSG); // pokud se slozi Salam, budeme mit historii zprav

    // zajima nas pouze WM_MOUSEWHEEL a WM_MOUSEHWHEEL
    //
    // 7.10.2009 - AS253_B1_IB34: Manison nam hlasil, ze mu pod Windows Vista nefunguje horizontalni scroll.
    // Me fungoval (touto cestou). Po nainstalovani Intellipoint ovladacu v7 (predtime jsem na Vista x64
    // nemel zadne spesl ovladace) prestaly WM_MOUSEHWHEEL zpravy prochazet tudy a natejkaly primo do
    // panelu Salamandera. Takze tuto cestu zakazuji a zpravy budeme chytat pouze v panelu.
    // poznamka: asi bychom stejnym zpusobem mohli odriznou i handling WM_MOUSEWHEEL, ale nebudu riskovat,
    // ze neco podelam na starsich OS (muzeme to zkusit s prechodem na W2K a dal)
    // poznamka2: pokud by se ukazalo, ze musime chytat WM_MOUSEHWHEEL i skrz tento hook, chtelo by to
    // provest runtime detekci, ze tudy zpravy WM_MOUSEHWHEEL natekaji a nasledne zakazat jejich zpracovani
    // v panelech a commandline.

    // 30.11.2012 - na foru se objevil clovek, kteremu WM_MOUSEHWEEL nechodi skrz message hook (stejna jako drive
    // u Manisona v pripade WM_MOUSEHWHEEL): https://forum.altap.cz/viewtopic.php?f=24&t=6039
    // takze nove budeme zpravu chytat take v jednotlivych oknech, kam muze potencialne chodit (dle focusu)
    // a nasledne ji routit tak, aby se dorucila do okna pod kurzorem, jak jsme to vzdy delali

    // aktualne tedy budeme jak WM_MOUSEWHEEL tak WM_MOUSEHWHEEL poustet a uvidime, co na to beta testeri

    if ((pMSG->message != WM_MOUSEWHEEL && pMSG->message != WM_MOUSEHWHEEL) || (wParam == PM_NOREMOVE))
        return retValue;

    // pokud zprava prisla "nedavno" druhym kanalem, budeme tento kanal ignorovat
    if (!MouseWheelMSGThroughHook && MouseWheelMSGTime != 0 && (GetTickCount() - MouseWheelMSGTime < MOUSEWHEELMSG_VALID))
        return retValue;
    MouseWheelMSGThroughHook = TRUE;
    MouseWheelMSGTime = GetTickCount();

    PostMouseWheelMessage(pMSG);

    return retValue;
}

BOOL InitializeMenuWheelHook()
{
    // setup hook for mouse messages
    DWORD threadID = GetCurrentThreadId();
    HOldMouseWheelHookProc = SetWindowsHookEx(WH_GETMESSAGE, // HANDLES neumi!
                                              MenuWheelHookProc,
                                              NULL, threadID);
    return (HOldMouseWheelHookProc != NULL);
}

BOOL ReleaseMenuWheelHook()
{
    // unhook mouse messages
    if (HOldMouseWheelHookProc != NULL)
    {
        UnhookWindowsHookEx(HOldMouseWheelHookProc); // HANDLES neumi!
        HOldMouseWheelHookProc = NULL;
    }
    return TRUE;
}

//
// *****************************************************************************
// CFileTimeStampsItem
//

CFileTimeStampsItem::CFileTimeStampsItem()
{
    DosFileName = FileName = SourcePath = ZIPRoot = NULL;
    memset(&LastWrite, 0, sizeof(LastWrite));
    FileSize = CQuadWord(0, 0);
    Attr = 0;
}

CFileTimeStampsItem::~CFileTimeStampsItem()
{
    if (ZIPRoot != NULL)
        free(ZIPRoot);
    if (SourcePath != NULL)
        free(SourcePath);
    if (FileName != NULL)
        free(FileName);
    if (DosFileName != NULL)
        free(DosFileName);
    DosFileName = FileName = SourcePath = ZIPRoot = NULL;
}

BOOL CFileTimeStampsItem::Set(const char* zipRoot, const char* sourcePath, const char* fileName,
                              const char* dosFileName, const FILETIME& lastWrite, const CQuadWord& fileSize,
                              DWORD attr)
{
    if (*zipRoot == '\\')
        zipRoot++;
    ZIPRoot = DupStr(zipRoot);
    if (ZIPRoot != NULL) // zip-root nema '\\' ani na zacatku, ani na konci
    {
        int l = (int)strlen(ZIPRoot);
        if (l > 0 && ZIPRoot[l - 1] == '\\')
            ZIPRoot[l - 1] = 0;
    }
    SourcePath = DupStr(sourcePath);
    if (SourcePath != NULL) // source-path nema '\\' na konci
    {
        int l = (int)strlen(SourcePath);
        if (l > 0 && SourcePath[l - 1] == '\\')
            SourcePath[l - 1] = 0;
    }
    FileName = DupStr(fileName);
    if (dosFileName[0] != 0)
        DosFileName = DupStr(dosFileName);
    LastWrite = lastWrite;
    FileSize = fileSize;
    Attr = attr;
    return ZIPRoot != NULL && SourcePath != NULL && FileName != NULL &&
           (DosFileName != NULL || dosFileName[0] == 0);
}

//
// *****************************************************************************
// CFileTimeStamps
//

BOOL CFileTimeStamps::AddFile(const char* zipFile, const char* zipRoot, const char* sourcePath,
                              const char* fileName, const char* dosFileName,
                              const FILETIME& lastWrite, const CQuadWord& fileSize, DWORD attr)
{
    if (ZIPFile[0] == 0)
        strcpy(ZIPFile, zipFile);
    else
    {
        if (strcmp(zipFile, ZIPFile) != 0)
        {
            TRACE_E("Unexpected situation in CFileTimeStamps::AddFile().");
            return FALSE;
        }
    }

    CFileTimeStampsItem* item = new CFileTimeStampsItem;
    if (item == NULL ||
        !item->Set(zipRoot, sourcePath, fileName, dosFileName, lastWrite, fileSize, attr))
    {
        if (item != NULL)
            delete item;
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    // test jestli uz zde neni (neni pred konstrukci item kvuli uprave stringu - '\\')
    int i;
    for (i = 0; i < List.Count; i++)
    {
        CFileTimeStampsItem* item2 = List[i];
        if (StrICmp(item->FileName, item2->FileName) == 0 &&
            StrICmp(item->SourcePath, item2->SourcePath) == 0)
        {
            delete item;
            return FALSE; // je uz zde, nepridavat dalsi ...
        }
    }

    List.Add(item);
    if (!List.IsGood())
    {
        delete item;
        List.ResetState();
        return FALSE;
    }
    return TRUE;
}

struct CFileTimeStampsEnum2Info
{
    TIndirectArray<CFileTimeStampsItem>* PackList;
    int Index;
};

const char* WINAPI FileTimeStampsEnum2(HWND parent, int enumFiles, const char** dosName, BOOL* isDir,
                                       CQuadWord* size, DWORD* attr, FILETIME* lastWrite, void* param,
                                       int* errorOccured)
{ // enumerujeme jen soubory, enumFiles lze tedy uplne vynechat
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    CFileTimeStampsEnum2Info* data = (CFileTimeStampsEnum2Info*)param;

    if (enumFiles == -1)
    {
        if (dosName != NULL)
            *dosName = NULL;
        if (isDir != NULL)
            *isDir = FALSE;
        if (size != NULL)
            *size = CQuadWord(0, 0);
        if (attr != NULL)
            *attr = 0;
        if (lastWrite != NULL)
            memset(lastWrite, 0, sizeof(FILETIME));
        data->Index = 0;
        return NULL;
    }

    if (data->Index < data->PackList->Count)
    {
        CFileTimeStampsItem* item = data->PackList->At(data->Index++);
        if (dosName != NULL)
            *dosName = (item->DosFileName == NULL) ? item->FileName : item->DosFileName;
        if (isDir != NULL)
            *isDir = FALSE;
        if (size != NULL)
            *size = item->FileSize;
        if (attr != NULL)
            *attr = item->Attr;
        if (lastWrite != NULL)
            *lastWrite = item->LastWrite;
        return item->FileName;
    }
    else
        return NULL;
}

void CFileTimeStamps::AddFilesToListBox(HWND list)
{
    int i;
    for (i = 0; i < List.Count; i++)
    {
        char buf[MAX_PATH];
        strcpy(buf, List[i]->ZIPRoot);
        SalPathAppend(buf, List[i]->FileName, MAX_PATH);
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

void CFileTimeStamps::Remove(int* indexes, int count)
{
    int i;
    for (i = 0; i < count; i++)
    {
        int index = indexes[count - i - 1];   // mazeme zezadu - mene sesouvani + neposouvaji se indexy
        if (index < List.Count && index >= 0) // jen tak pro sychr
        {
            List.Delete(index);
        }
    }
}

BOOL CDynamicStringImp::Add(const char* str, int len)
{
    if (len == -1)
        len = (int)strlen(str);
    else
    {
        if (len == -2)
            len = (int)strlen(str) + 1;
    }
    if (Length + len >= Allocated)
    {
        char* text = (char*)realloc(Text, Length + len + 100);
        if (text == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        Allocated = Length + len + 100;
        Text = text;
    }
    memcpy(Text + Length, str, len);
    Length += len;
    Text[Length] = 0;
    return TRUE;
}

void CDynamicStringImp::DetachData()
{
    Text = NULL;
    Allocated = 0;
    Length = 0;
}

void CFileTimeStamps::CopyFilesTo(HWND parent, int* indexes, int count, const char* initPath)
{
    CALL_STACK_MESSAGE3("CFileTimeStamps::CopyFilesTo(, , %d, %s)", count, initPath);
    char path[MAX_PATH];
    if (count > 0 &&
        GetTargetDirectory(parent, parent, LoadStr(IDS_BROWSEARCUPDATE),
                           LoadStr(IDS_BROWSEARCUPDATETEXT), path, FALSE, initPath))
    {
        CDynamicStringImp fromStr, toStr;
        BOOL ok = TRUE;
        BOOL tooLongName = FALSE;
        int i;
        for (i = 0; i < count; i++)
        {
            int index = indexes[i];
            if (index < List.Count && index >= 0) // jen tak pro sychr
            {
                CFileTimeStampsItem* item = List[index];
                char name[MAX_PATH];
                strcpy(name, item->SourcePath);
                tooLongName |= !SalPathAppend(name, item->FileName, MAX_PATH);
                ok &= fromStr.Add(name, (int)strlen(name) + 1);

                strcpy(name, path);
                tooLongName |= !SalPathAppend(name, item->ZIPRoot, MAX_PATH);
                tooLongName |= !SalPathAppend(name, item->FileName, MAX_PATH);
                ok &= toStr.Add(name, (int)strlen(name) + 1);
            }
        }
        fromStr.Add("\0", 2); // pro jistotu dame na konec dalsi dve nuly (zadne Add, taky o.k.)
        toStr.Add("\0", 2);   // pro jistotu dame na konec dalsi dve nuly (zadne Add, taky o.k.)

        if (ok && !tooLongName)
        {
            CShellExecuteWnd shellExecuteWnd;
            SHFILEOPSTRUCT fo;
            fo.hwnd = shellExecuteWnd.Create(parent, "SEW: CFileTimeStamps::CopyFilesTo");
            fo.wFunc = FO_COPY;
            fo.pFrom = fromStr.Text;
            fo.pTo = toStr.Text;
            fo.fFlags = FOF_SIMPLEPROGRESS | FOF_NOCONFIRMMKDIR | FOF_MULTIDESTFILES;
            fo.fAnyOperationsAborted = FALSE;
            fo.hNameMappings = NULL;
            char title[100];
            lstrcpyn(title, LoadStr(IDS_BROWSEARCUPDATE), 100); // radsi backupneme, LoadStr pouzivaji i ostatni thready
            fo.lpszProgressTitle = title;
            // provedeme samotne nakopirovani - uzasne snadne, bohuzel jim sem tam pada ;-)
            CALL_STACK_MESSAGE1("CFileTimeStamps::CopyFilesTo::SHFileOperation");
            SHFileOperation(&fo);
        }
        else
        {
            if (tooLongName)
            {
                SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
}

void CFileTimeStamps::CheckAndPackAndClear(HWND parent, BOOL* someFilesChanged, BOOL* archMaybeUpdated)
{
    CALL_STACK_MESSAGE1("CFileTimeStamps::CheckAndPackAndClear()");
    //---  vyhodime ze seznamu soubory, ktere nebyly zmeneny
    BeginStopRefresh();
    if (someFilesChanged != NULL)
        *someFilesChanged = FALSE;
    if (archMaybeUpdated != NULL)
        *archMaybeUpdated = FALSE;
    char buf[MAX_PATH + 100];
    WIN32_FIND_DATA data;
    int i;
    for (i = List.Count - 1; i >= 0; i--)
    {
        CFileTimeStampsItem* item = List[i];
        sprintf(buf, "%s\\%s", item->SourcePath, item->FileName);
        BOOL kill = TRUE;
        HANDLE find = HANDLES_Q(FindFirstFile(buf, &data));
        if (find != INVALID_HANDLE_VALUE)
        {
            HANDLES(FindClose(find));
            if (CompareFileTime(&data.ftLastWriteTime, &item->LastWrite) != 0 ||    // lisi se casy
                CQuadWord(data.nFileSizeLow, data.nFileSizeHigh) != item->FileSize) // lisi se velikosti
            {
                item->FileSize = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh); // bereme novou velikost
                item->LastWrite = data.ftLastWriteTime;
                item->Attr = data.dwFileAttributes;
                kill = FALSE;
            }
        }
        if (kill)
        {
            List.Delete(i);
        }
    }

    if (List.Count > 0)
    {
        if (someFilesChanged != NULL)
            *someFilesChanged = TRUE;
        // pri critical shutdown se tvarime, ze updatle soubory neexistuji, zabalit zpet do archivu je
        // nestihneme, ale smazat je nesmime, po startu musi zustat uzivateli sance updatle soubory
        // rucne do archivu zabalit
        if (!CriticalShutdown)
        {
            CArchiveUpdateDlg dlg(parent, this, Panel);
            BOOL showDlg = TRUE;
            while (showDlg)
            {
                showDlg = FALSE;
                if (dlg.Execute() == IDOK)
                {
                    if (archMaybeUpdated != NULL)
                        *archMaybeUpdated = TRUE;
                    //--- zapakujeme zmenene soubory, po skupinach se stejnym zip-root a source-path
                    TIndirectArray<CFileTimeStampsItem> packList(10, 5); // seznam vsech se stejnym zip-root a source-path
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    while (!showDlg && List.Count > 0)
                    {
                        CFileTimeStampsItem* item1 = List[0];
                        char *r1, *s1;
                        if (item1 != NULL)
                        {
                            r1 = item1->ZIPRoot;
                            s1 = item1->SourcePath;
                            packList.Add(item1);
                            List.Detach(0);
                        }
                        for (i = List.Count - 1; i >= 0; i--) // kvadraticka slozitost zde snad nebude na obtiz
                        {                                     // jedeme odzadu, protoze Detach je tak "jednodusi"
                            CFileTimeStampsItem* item2 = List[i];
                            char* r2 = item2->ZIPRoot;
                            char* s2 = item2->SourcePath;
                            if (strcmp(r1, r2) == 0 && // shodny zip-root (case-sensitive porovnani nutne - update test\A.txt a Test\b.txt nesmi probehnout zaroven)
                                StrICmp(s1, s2) == 0)  // shodny source-path
                            {
                                packList.Add(item2);
                                List.Detach(i);
                            }
                        }

                        // volani pack pro packList
                        BOOL loop = TRUE;
                        while (loop)
                        {
                            CFileTimeStampsEnum2Info data2;
                            data2.PackList = &packList;
                            data2.Index = 0;
                            SetCurrentDirectory(s1);
                            if (Panel->CheckPath(TRUE, NULL, ERROR_SUCCESS, TRUE, parent) == ERROR_SUCCESS &&
                                PackCompress(parent, Panel, ZIPFile, r1, FALSE, s1, FileTimeStampsEnum2, &data2))
                                loop = FALSE;
                            else
                            {
                                loop = SalMessageBox(parent, LoadStr(IDS_UPDATEFAILED),
                                                     LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONQUESTION) == IDYES;
                                if (!loop) // "cancel", odpojime soubory z disk-cache, jinak je odmaze
                                {
                                    List.Add(packList.GetData(), packList.Count);
                                    packList.DetachMembers();
                                    showDlg = TRUE; // zobrazime znovu Archive Update dialog (se zbylymi soubory)
                                }
                            }
                            SetCurrentDirectoryToSystem();
                        }

                        packList.DestroyMembers();
                    }
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                }
            }
        }
    }

    List.DestroyMembers();
    ZIPFile[0] = 0;
    EndStopRefresh();
}

//****************************************************************************
//
// CTopIndexMem
//

void CTopIndexMem::Push(const char* path, int topIndex)
{
    // zjistime, jestli path navazuje na Path (path==Path+"\\jmeno")
    const char* s = path + strlen(path);
    if (s > path && *(s - 1) == '\\')
        s--;
    BOOL ok;
    if (s == path)
        ok = FALSE;
    else
    {
        if (s > path && *s == '\\')
            s--;
        while (s > path && *s != '\\')
            s--;

        int l = (int)strlen(Path);
        if (l > 0 && Path[l - 1] == '\\')
            l--;
        ok = s - path == l && StrNICmp(path, Path, l) == 0;
    }

    if (ok) // navazuje -> zapamatujeme si dalsi top-index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // je potreba vyhodit z pameti prvni top-index
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // nenavazuje -> prvni top-index v rade
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(const char* path, int& topIndex)
{
    // zjistime, jestli path odpovida Path (path==Path)
    int l1 = (int)strlen(path);
    if (l1 > 0 && path[l1 - 1] == '\\')
        l1--;
    int l2 = (int)strlen(Path);
    if (l2 > 0 && Path[l2 - 1] == '\\')
        l2--;
    if (l1 == l2 && StrNICmp(path, Path, l1) == 0)
    {
        if (TopIndexesCount > 0)
        {
            char* s = Path + strlen(Path);
            if (s > Path && *(s - 1) == '\\')
                s--;
            if (s > Path && *s == '\\')
                s--;
            while (s > Path && *s != '\\')
                s--;
            *s = 0;
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // tuto hodnotu jiz nemame (nebyla ulozena nebo mala pamet->byla vyhozena)
        {
            Clear();
            return FALSE;
        }
    }
    else // dotaz na jinou cestu -> vycistime pamet, doslo k dlouhemu skoku
    {
        Clear();
        return FALSE;
    }
}

//*****************************************************************************

CFileHistory::CFileHistory()
    : Files(10, 10)
{
}

void CFileHistory::ClearHistory()
{
    Files.DestroyMembers();
}

BOOL CFileHistory::AddFile(CFileHistoryItemTypeEnum type, DWORD handlerID, const char* fileName)
{
    CALL_STACK_MESSAGE4("CFileHistory::AddFile(%d, %u, %s)", type, handlerID, fileName);

    // prohledame stavajici polozky, jestli uz se pridavana polozka nevyskytuje
    int i;
    for (i = 0; i < Files.Count; i++)
    {
        CFileHistoryItem* item = Files[i];
        if (item->Equal(type, handlerID, fileName))
        {
            // pokud ano, pouze ji vytahneme na vrchni pozici
            if (i > 0)
            {
                Files.Detach(i);
                if (!Files.IsGood())
                    Files.ResetState(); // nemuze se nepovest, hlasi jedine nedostatek pameti pro zmenseni pole
                Files.Insert(0, item);
                if (!Files.IsGood())
                {
                    Files.ResetState();
                    delete item;
                    return FALSE;
                }
            }
            return TRUE;
        }
    }

    // polozka neexistuje - vlozime ji na horni pozici
    CFileHistoryItem* item = new CFileHistoryItem(type, handlerID, fileName);
    if (item == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    if (!item->IsGood())
    {
        delete item;
        return FALSE;
    }
    Files.Insert(0, item);
    if (!Files.IsGood())
    {
        Files.ResetState();
        delete item;
        return FALSE;
    }
    // zarizneme na 30 polozek
    if (Files.Count > 30)
        Files.Delete(30);

    return TRUE;
}

BOOL CFileHistory::FillPopupMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFileHistory::FillPopupMenu()");

    // nalejeme polozky
    char name[2 * MAX_PATH];
    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_ICON | MENU_MASK_STRING;
    mii.Type = MENU_TYPE_STRING;
    mii.String = name;
    int i;
    for (i = 0; i < Files.Count; i++)
    {
        CFileHistoryItem* item = Files[i];

        // jmeno oddelime od cesty znakem '\t' - tim bude ve zvlastnim sloupci
        lstrcpy(name, item->FileName);
        char* ptr = strrchr(name, '\\');
        if (ptr == NULL)
            return FALSE;
        memmove(ptr + 1, ptr, lstrlen(ptr) + 1);
        *(ptr + 1) = '\t';
        const char* text = "";
        // zdvojime '&', aby se nezobrazovalo jako podtrzeni
        DuplicateAmpersands(name, 2 * MAX_PATH);

        mii.HIcon = item->HIcon;
        switch (item->Type)
        {
        case fhitView:
            text = LoadStr(IDS_FILEHISTORY_VIEW);
            break;
        case fhitEdit:
            text = LoadStr(IDS_FILEHISTORY_EDIT);
            break;
        case fhitOpen:
            text = LoadStr(IDS_FILEHISTORY_OPEN);
            break;
        default:
            TRACE_E("Unknown Type=" << item->Type);
        }
        sprintf(name + lstrlen(name), "\t(%s)", text); // pripojime zpusob, jakym je soubor oteviran
        mii.ID = i + 1;
        popup->InsertItem(-1, TRUE, &mii);
    }
    if (i > 0)
    {
        popup->SetStyle(MENU_POPUP_THREECOLUMNS); // prvni dva sloupce jsou zarovnane doleva
        popup->AssignHotKeys();
    }
    return TRUE;
}

BOOL CFileHistory::Execute(int index)
{
    CALL_STACK_MESSAGE2("CFileHistory::Execute(%d)", index);
    if (index < 1 || index > Files.Count)
    {
        TRACE_E("Index is out of range");
        return FALSE;
    }
    return Files[index - 1]->Execute();
    return TRUE;
}

BOOL CFileHistory::HasItem()
{
    return Files.Count > 0;
}

//****************************************************************************
//
// Directory editline/combobox support
//

#define DIRECTORY_COMMAND_BROWSE 1    // browse directory
#define DIRECTORY_COMMAND_LEFT 3      // cesta z leveho panelu
#define DIRECTORY_COMMAND_RIGHT 4     // cesta z praveho panelu
#define DIRECTORY_COMMAND_HOTPATHF 5  // prvni hot path
#define DIRECTORY_COMMAND_HOTPATHL 35 // posledni hot path

BOOL SetEditOrComboText(HWND hWnd, const char* text)
{
    char className[31];
    className[0] = 0;
    if (GetClassName(hWnd, className, 30) == 0)
    {
        TRACE_E("GetClassName failed on hWnd=0x" << hWnd);
        return FALSE;
    }

    HWND hEdit;
    if (StrICmp(className, "edit") != 0)
    {
        hEdit = GetWindow(hWnd, GW_CHILD);
        if (hEdit == NULL ||
            GetClassName(hEdit, className, 30) == 0 ||
            StrICmp(className, "edit") != 0)
        {
            TRACE_E("Edit window was not found hWnd=0x" << hWnd);
            return FALSE;
        }
    }
    else
        hEdit = hWnd;

    SendMessage(hEdit, WM_SETTEXT, 0, (LPARAM)text);
    SendMessage(hEdit, EM_SETSEL, 0, lstrlen(text));
    return TRUE;
}

DWORD TrackDirectoryMenu(HWND hDialog, int buttonID, BOOL selectMenuItem)
{
    RECT r;
    GetWindowRect(GetDlgItem(hDialog, buttonID), &r);

    CMenuPopup popup;
    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_STATE;
    mii.Type = MENU_TYPE_STRING;
    mii.State = 0;

    MENU_ITEM_INFO miiSep;
    miiSep.Mask = MENU_MASK_TYPE;
    miiSep.Type = MENU_TYPE_SEPARATOR;

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertItem() dole...
MENU_TEMPLATE_ITEM CopyMoveBrowseMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_PATHMENU_BROWSE
  {MNTT_IT, IDS_PATHMENU_LEFT
  {MNTT_IT, IDS_PATHMENU_RIGHT
  {MNTT_PE, 0
};
*/

    mii.ID = DIRECTORY_COMMAND_BROWSE;
    mii.String = LoadStr(IDS_PATHMENU_BROWSE);
    popup.InsertItem(0xFFFFFFFF, TRUE, &mii);

    //  mii.ID = 2;
    //  mii.String = "Tree...\tCtrl+T";
    //  popup.InsertItem(0xFFFFFFFF, TRUE, &mii);

    popup.InsertItem(0xFFFFFFFF, TRUE, &miiSep);

    mii.ID = DIRECTORY_COMMAND_LEFT;
    mii.String = LoadStr(IDS_PATHMENU_LEFT);
    popup.InsertItem(0xFFFFFFFF, TRUE, &mii);

    mii.ID = DIRECTORY_COMMAND_RIGHT;
    mii.String = LoadStr(IDS_PATHMENU_RIGHT);
    popup.InsertItem(0xFFFFFFFF, TRUE, &mii);

    // pripojime hotpaths, existuji-li
    DWORD firstID = DIRECTORY_COMMAND_HOTPATHF;
    MainWindow->HotPaths.FillHotPathsMenu(&popup, firstID, FALSE, FALSE, FALSE, TRUE);

    DWORD flags = MENU_TRACK_RETURNCMD;
    if (selectMenuItem)
    {
        popup.SetSelectedItemIndex(0);
        flags |= MENU_TRACK_SELECT;
    }
    return popup.Track(flags, r.right, r.top, hDialog, &r);
}

DWORD OnKeyDownHandleSelectAll(DWORD keyCode, HWND hDialog, int editID)
{
    // od Windows Vista uz SelectAll standardne funguje, takze tam nechame select all na nich
    if (WindowsVistaAndLater)
        return FALSE;

    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (controlPressed && !shiftPressed && !altPressed)
    {
        if (keyCode == 'A')
        {
            // select all
            HWND hChild = GetDlgItem(hDialog, editID);
            if (hChild != NULL)
            {
                char className[30];
                GetClassName(hChild, className, 29);
                className[29] = 0;
                BOOL combo = (stricmp(className, "combobox") == 0);
                if (combo)
                    SendMessage(hChild, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
                else
                    SendMessage(hChild, EM_SETSEL, 0, -1);
                return TRUE;
            }
        }
    }
    return FALSE;
}

void InvokeDirectoryMenuCommand(DWORD cmd, HWND hDialog, int editID, int editBufSize);

void OnDirectoryButton(HWND hDialog, int editID, int editBufSize, int buttonID, WPARAM wParam, LPARAM lParam)
{
    BOOL selectMenuItem = LOWORD(lParam);
    DWORD cmd = TrackDirectoryMenu(hDialog, buttonID, selectMenuItem);
    InvokeDirectoryMenuCommand(cmd, hDialog, editID, editBufSize);
}

DWORD OnDirectoryKeyDown(DWORD keyCode, HWND hDialog, int editID, int editBufSize, int buttonID)
{
    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (!controlPressed && !shiftPressed && altPressed && keyCode == VK_RIGHT)
    {
        OnDirectoryButton(hDialog, editID, editBufSize, buttonID, MAKELPARAM(buttonID, 0), MAKELPARAM(TRUE, 0));
        return TRUE;
    }
    if (controlPressed && !shiftPressed && !altPressed)
    {
        switch (keyCode)
        {
        case 'B':
        {
            InvokeDirectoryMenuCommand(DIRECTORY_COMMAND_BROWSE, hDialog, editID, editBufSize);
            return TRUE;
        }

        case 219: // '['
        case 221: // ']'
        {
            InvokeDirectoryMenuCommand((keyCode == 219) ? DIRECTORY_COMMAND_LEFT : DIRECTORY_COMMAND_RIGHT, hDialog, editID, editBufSize);
            return TRUE;
        }

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
        {
            int index = keyCode == '0' ? 9 : keyCode - '1';
            InvokeDirectoryMenuCommand(DIRECTORY_COMMAND_HOTPATHF + index, hDialog, editID, editBufSize);
            return TRUE;
        }
        }
    }
    return FALSE;
}

void InvokeDirectoryMenuCommand(DWORD cmd, HWND hDialog, int editID, int editBufSize)
{
    char path[2 * MAX_PATH];
    BOOL setPathToEdit = FALSE;
    switch (cmd)
    {
    case 0:
    {
        return;
    }

    case DIRECTORY_COMMAND_BROWSE:
    {
        // browse
        GetDlgItemText(hDialog, editID, path, MAX_PATH);
        char caption[100];
        GetWindowText(hDialog, caption, 100); // bude mit stejny caption jako dialog
        if (GetTargetDirectory(hDialog, hDialog, caption, LoadStr(IDS_BROWSETARGETDIRECTORY), path, FALSE, path))
            setPathToEdit = TRUE;
        break;
    }

        //    case 2:
        //    {
        //      // tree
        //      break;
        //    }

    case DIRECTORY_COMMAND_LEFT:
    case DIRECTORY_COMMAND_RIGHT:
    {
        // left/right panel directory
        CFilesWindow* panel = (cmd == DIRECTORY_COMMAND_LEFT) ? MainWindow->LeftPanel : MainWindow->RightPanel;
        if (panel != NULL)
        {
            panel->GetGeneralPath(path, 2 * MAX_PATH, TRUE);
            setPathToEdit = TRUE;
        }
        break;
    }

    default:
    {
        // hot path
        if (cmd >= DIRECTORY_COMMAND_HOTPATHF && cmd <= DIRECTORY_COMMAND_HOTPATHL)
        {
            if (MainWindow->GetExpandedHotPath(hDialog, cmd - DIRECTORY_COMMAND_HOTPATHF, path, 2 * MAX_PATH))
                setPathToEdit = TRUE;
        }
        else
            TRACE_E("Unknown cmd=" << cmd);
    }
    }
    if (setPathToEdit)
    {
        if ((int)strlen(path) >= editBufSize)
        {
            TRACE_E("InvokeDirectoryMenuCommand(): too long path! len=" << (int)strlen(path));
            path[editBufSize - 1] = 0;
        }
        SetEditOrComboText(GetDlgItem(hDialog, editID), path);
    }
}

//****************************************************************************
//
// CKeyForwarder
//

class CKeyForwarder : public CWindow
{
protected:
    BOOL SkipCharacter; // zamezuje pipnuti pro zpracovane klavesy
    HWND HDialog;       // dialog, kam budeme zasilat WM_USER_KEYDOWN
    int CtrlID;         // pro WM_USER_KEYDOWN

public:
    CKeyForwarder(HWND hDialog, int ctrlID, CObjectOrigin origin = ooAllocated);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

CKeyForwarder::CKeyForwarder(HWND hDialog, int ctrlID, CObjectOrigin origin)
    : CWindow(origin)
{
    SkipCharacter = FALSE;
    HDialog = hDialog;
    CtrlID = ctrlID;
}

LRESULT
CKeyForwarder::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CKeyForwarder::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CHAR:
    {
        if (SkipCharacter)
        {
            SkipCharacter = FALSE;
            return 0;
        }
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        SkipCharacter = TRUE; // zamezime pipani
        BOOL ret = (BOOL)SendMessage(HDialog, WM_USER_KEYDOWN, MAKELPARAM(CtrlID, 0), wParam);
        if (ret)
            return 0;
        SkipCharacter = FALSE;
        break;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        SkipCharacter = FALSE; // pro jistotu
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

BOOL CreateKeyForwarder(HWND hDialog, int ctrlID)
{
    HWND hWindow = GetDlgItem(hDialog, ctrlID);
    char className[31];
    className[0] = 0;
    if (GetClassName(hWindow, className, 30) == 0 || StrICmp(className, "edit") != 0)
    {
        // mohlo by jit o combobox, zkusime sahnout pro vnitrni edit
        hWindow = GetWindow(hWindow, GW_CHILD);
        if (hWindow == NULL || GetClassName(hWindow, className, 30) == 0 || StrICmp(className, "edit") != 0)
        {
            TRACE_E("CreateKeyForwarder: edit window was not found ClassName is " << className);
            return FALSE;
        }
    }

    CKeyForwarder* edit = new CKeyForwarder(hDialog, ctrlID);
    if (edit != NULL)
    {
        edit->AttachToWindow(hWindow);
        return TRUE;
    }
    return FALSE;
}
