// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
        if (l + 1 + n < pathSize) // do we still fit even with the terminating null?
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
            //      if (iterator != path && *(iterator - 1) != '\\')  // ".cvspass" in Windows is treated as an extension ...
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
            //      if (iterator != path && *(iterator - 1) != '\\')  // ".cvspass" is considered an extension in Windows ...
            return TRUE; // the extension already exists
                         //      break;  // no point searching any further
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
            //      if (iterator != path && *(iterator - 1) != '\\')  // ".cvspass" in Windows is treated as an extension ...
            //      {
            len = (int)(iterator - path);
            break; // the extension already exists -> overwrite it
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

    if ((s - tmpDir) + 8 < MAX_PATH) // enough room to append "XXXX.tmp"
    {
        DWORD randNum = (GetTickCount() & 0xFFF);
        while (1)
        {
            sprintf(s, "%X.tmp", randNum++);
            if (file) // file
            {
                HANDLE h = HANDLES_Q(CreateFile(tmpDir, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                                FILE_ATTRIBUTE_NORMAL, NULL));
                if (h != INVALID_HANDLE_VALUE)
                {
                    HANDLES(CloseHandle(h));
                    strcpy(tmpName, tmpDir); // we copy the result
                    return TRUE;
                }
            }
            else // directory
            {
                if (CreateDirectory(tmpDir, NULL))
                {
                    strcpy(tmpName, tmpDir); // we copy the result
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
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) // an in-page error always means a file error
    {
        return EXCEPTION_EXECUTE_HANDLER; // run the __except block
    }
    else
    {
        if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&    // access violation signals a file error only when the failing address falls within the file
            (e->ExceptionRecord->NumberParameters >= 2 &&                         // we have something to test
             e->ExceptionRecord->ExceptionInformation[1] >= (ULONG_PTR)fileMem && // pointer to the fault is within the mapped file
             e->ExceptionRecord->ExceptionInformation[1] < ((ULONG_PTR)fileMem) + fileMemSize))
        {
            return EXCEPTION_EXECUTE_HANDLER; // we run the __except block
        }
        else
        {
            return EXCEPTION_CONTINUE_SEARCH; // we pass the exception further... up the call stack
        }
    }
}

// ****************************************************************************

BOOL SalRemovePointsFromPath(char* afterRoot)
{
    char* d = afterRoot; // pointer just past the root path
    while (*d != 0)
    {
        while (*d != 0 && *d != '.')
            d++;
        if (*d == '.')
        {
            if (d == afterRoot || d > afterRoot && *(d - 1) == '\\') // '.' right after the root path or "\."
            {
                if (*(d + 1) == '.' && (*(d + 2) == '\\' || *(d + 2) == 0)) // ".."
                {
                    char* l = d - 1;
                    while (l > afterRoot && *(l - 1) != '\\')
                        l--;
                    if (l >= afterRoot) // removing the directory and ".."
                    {
                        if (*(d + 2) == 0)
                            *l = 0;
                        else
                            memmove(l, d + 3, strlen(d + 3) + 1);
                        d = l;
                    }
                    else
                        return FALSE; // cannot remove ".."
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
    WCHAR* d = afterRoot; // pointer just past the root path
    while (*d != 0)
    {
        while (*d != 0 && *d != L'.')
            d++;
        if (*d == L'.')
        {
            if (d == afterRoot || d > afterRoot && *(d - 1) == L'\\') // '.' right after the root path or "\."
            {
                if (*(d + 1) == L'.' && (*(d + 2) == L'\\' || *(d + 2) == 0)) // ".."
                {
                    WCHAR* l = d - 1;
                    while (l > afterRoot && *(l - 1) != L'\\')
                        l--;
                    if (l >= afterRoot) // removing the directory and ".."
                    {
                        if (*(d + 2) == 0)
                            *l = 0;
                        else
                            memmove(l, d + 3, sizeof(WCHAR) * (lstrlenW(d + 3) + 1));
                        d = l;
                    }
                    else
                        return FALSE; // cannot remove ".."
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

    int rootOffset = 3; // offset to the beginning of the directory part of the path (3 for "c:\path")
    char* s = name;
    while (*s >= 1 && *s <= ' ')
        s++;
    if (*s == '\\' && *(s + 1) == '\\') // UNC (\\server\share\...)
    {                                   // trim spaces at the beginning of the path
        if (s != name)
            memmove(name, s, strlen(s) + 1);
        s = name + 2;
        if (*s == '.' || *s == '?')
            err = IDS_PATHISINVALID; // paths like \\?\Volume{6e76293d-1828-11df-8f3c-806e6f6e6963}\ and \\.\PhysicalDisk5\ are simply not supported here
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
                    s++; // skipping the server name
                if (*s == '\\')
                    s++;
                if (s - name > MAX_PATH - 1)
                    err = IDS_SERVERNAMEMISSING; // found text is too long to be a server name
                else
                {
                    if (*s == 0 || *s == '\\')
                    {
                        if (callNethood != NULL)
                            *callNethood = *s == 0 && (*(s - 1) != '.' || *(s - 2) != '\\') && (*(s - 1) != '\\' || *(s - 2) != '.' || *(s - 3) != '\\'); // not "\\." or "\\.\" (the beginning of a path like "\\.\C:\")
                        err = IDS_SHARENAMEMISSING;
                    }
                    else
                    {
                        while (*s != 0 && *s != '\\')
                            s++; // skipping the share name
                        if ((s - name) + 1 > MAX_PATH - 1)
                            err = IDS_SHARENAMEMISSING; // found text is too long to be a share name (+1 for the trailing backslash)
                        if (*s == '\\')
                            s++;
                    }
                }
            }
        }
    }
    else // path specified by drive letter (c:\...)
    {
        if (*s != 0)
        {
            if (*(s + 1) == ':') // "c:..."
            {
                if (*(s + 2) == '\\') // "c:\..."
                {                     // trim spaces at the beginning of the path
                    if (s != name)
                        memmove(name, s, strlen(s) + 1);
                }
                else // "c:path..."
                {
                    int l1 = (int)strlen(s + 2); // length of the remainder ("path...")
                    if (LowerCase[*s] >= 'a' && LowerCase[*s] <= 'z')
                    {
                        const char* head;
                        if (curDir != NULL && LowerCase[curDir[0]] == LowerCase[*s])
                            head = curDir;
                        else
                            head = DefaultDir[LowerCase[*s] - 'a'];
                        int l2 = (int)strlen(head);
                        if (head[l2 - 1] != '\\')
                            l2++; // leave room for '\\'
                        if (l1 + l2 >= nameBufSize)
                            err = IDS_TOOLONGPATH;
                        else // build the full path
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
                    // for relative paths without a leading '\\', do not treat spaces as mistakes when 'allowRelPathWithSpaces' is enabled
                    // (a directory or file name can start with a space even though Windows and other softwares,
                    // Salamander included, try to prevent it)
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
                            else // build the path starting from the root of the current drive
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

    if (err == 0) // removal of '.' and '..' from the path
    {
        if (!SalRemovePointsFromPath(s))
            err = IDS_PATHISINVALID;
    }

    if (err == 0) // removal of any unwanted trailing backslash from the string
    {
        int l = (int)strlen(name);
        if (l > 1 && name[1] == ':') // path type "c:\path"
        {
            if (l > 3) // not a root path
            {
                if (name[l - 1] == '\\')
                    name[l - 1] = 0; // trim the backslash
            }
            else
            {
                name[2] = '\\'; // root path, backslash required ("c:\")
                name[3] = 0;
            }
        }
        else
        {
            if (name[0] == '\\' && name[1] == '\\' && name[2] == '.' && name[3] == '\\' && name[4] != 0 && name[5] == ':') // path like "\\.\C:\"
            {
                if (l > 7) // not a root path
                {
                    if (name[l - 1] == '\\')
                        name[l - 1] = 0; // trim the backslash
                }
                else
                {
                    name[6] = '\\'; // root path, backslash required ("\\.\C:\")
                    name[7] = 0;
                }
            }
            else // UNC path
            {
                if (l > 0 && name[l - 1] == '\\')
                    name[l - 1] = 0; // trim the backslash
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
    if (!finished) // after calling TerminateAuxThreads(), we no longer accept anything
    {
        if (add)
        {
            // remove threads that have already finished from the array
            for (int i = 0; i < AuxThreads.Count; i++)
            {
                DWORD code;
                if (!GetExitCodeThread(AuxThreads[i], &code) || code != STILL_ACTIVE)
                { // the thread has already finished
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
                { // the thread has already finished
                    HANDLES(CloseHandle(thread));
                    skipAdd = TRUE;
                }
            }
            // add the new thread
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
                { // the thread is still running, we terminate it
                    TerminateThread(t, 666);
                    WaitForSingleObject(t, INFINITE); // wait until the thread actually terminates, it sometimes takes a while
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
    DWORD CallerCalledFromArr[STOPREFRESHSTACKSIZE];  // array of return addresses of functions from which BeginStopRefresh() was called
    DWORD CalledFromArr[STOPREFRESHSTACKSIZE];        // array of addresses from which BeginStopRefresh() was called
    int Count;                                        // number of elements in the previous two arrays
    int Ignored;                                      // number of BeginStopRefresh() calls we had to ignore (STOPREFRESHSTACKSIZE too small -> consider increasing it)

  public:
    CStopRefreshStack() {Count = 0; Ignored = 0;}
    ~CStopRefreshStack() {CheckIfEmpty(3);} // three BeginStopRefresh() calls are OK: for both panels BeginStopRefresh() is called, and the third comes from WM_USER_CLOSE_MAINWND (invoked first)

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
#ifdef _DEBUG     // we test whether BeginStopRefresh() and EndStopRefresh() are called from the same function (based on the return address of the calling function -> it will not detect a "mistake" when two different functions are both called from the same function)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

if this code ever needs to be revived, keep in mind it can be replaced (x86 and x64):
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
#ifdef _DEBUG     // we test whether BeginStopRefresh() and EndStopRefresh() are called from the same function (based on the return address of the calling function -> it will not detect a "mistake" when two different functions are both called from the same function)
  DWORD *register_ebp;
  __asm mov register_ebp, ebp
  DWORD called_from, caller_called_from;
  __try
  {
    called_from = *(DWORD*)((char*)register_ebp + 4);

if this code ever needs to be revived, keep in mind it can be replaced (x86 and x64):
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
            // if we blocked any refresh, give it a chance to run now
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
                !AlreadyInPlugin) // if it is still inside a plug-in, posting the notification is pointless
            {
                MainWindow->NeedToResentDispachChangeNotif = FALSE;

                // post a request to broadcast path change notifications
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
    SetCurrentDirectory(dir); // so it runs faster (the system prefers the current directory)
    if (strlen(dir) < MAX_PATH)
        _RemoveTemporaryDir(dir);
    SetCurrentDirectoryToSystem(); // we must leave it, otherwise it cannot be deleted

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
    SetCurrentDirectory(dir); // so it runs faster (the system prefers the current directory)
    if (strlen(dir) < MAX_PATH)
        _RemoveEmptyDirs(dir);
    SetCurrentDirectoryToSystem(); // we must leave it, otherwise it cannot be deleted

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
    if (attrs == 0xFFFFFFFF) // probably does not exist, we allow it to be created
    {
        char root[MAX_PATH];
        GetRootPath(root, dir);
        if (dirLen <= (int)strlen(root)) // the directory is a root directory
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
            // if the user did not suppress it, we show information about the missing directory
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
            while (1) // find the first existing directory
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
                    break; // we have reached the root directory
                }
                attrs = SalGetFileAttributes(name);
                if (attrs != 0xFFFFFFFF) // the name exists
                {
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        break; // we will build from this directory
                    else       // it is a file, that would not work ...
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
                BOOL invalidName = manualCrDir && *st <= ' '; // leading spaces are undesirable only when creating directories manually (Windows allows it, but it is potentially dangerous)
                const char* slash = strchr(st, '\\');
                if (slash == NULL)
                    slash = st + strlen(st);
                memcpy(name + len, st, slash - st);
                name[len += (int)(slash - st)] = 0;
                if (name[len - 1] <= ' ' || name[len - 1] == '.')
                    invalidName = TRUE; // trailing spaces and dots are undesirable in the created directory name
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
    else // it is a file, that would not work ...
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

    if (Type == 0) // drive
    {
        char root[MAX_PATH];
        GetRootPath(root, pathOrArchiveOrFSName);
        const char* e = pathOrArchiveOrFSName + strlen(pathOrArchiveOrFSName);
        if ((int)strlen(root) < e - pathOrArchiveOrFSName || // not a root path
            pathOrArchiveOrFSName[0] == '\\')                // it is a UNC path
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
        else // it is a standard root path (c:\)
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
        if (Type == 1 || Type == 2) // archive or FS (just copies of both strings)
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
            return; // no change -> done
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
    if (Type == 1 || Type == 2) // archive or FS
    {
        buffer += l - 1;
        bufferSize -= l - 1;
        char* s = ArchivePathOrFSUserPart;
        if (*s != 0 || Type == 2)
        {
            if (bufferSize >= 2) // append '\\' or ':'
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

    // we must duplicate every '&', otherwise they will become underlines
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
                    memmove(s + 1, s, l - (s - buffer) + 1); // duplicate '&'
                    l++;
                    s++;
                }
                else // no room, truncate the buffer
                {
                    ret = FALSE;
                    memmove(s + 1, s, l - (s - buffer)); // duplicate '&' and cut off one character
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
                    *d++ = *s++; // replace "&&" with '&'
                s++;
            }
        }
        *d = 0;
    }
}

BOOL CPathHistoryItem::Execute(CFilesWindow* panel)
{
    BOOL ret = TRUE; // by default we report success
    char errBuf[MAX_PATH + 200];
    if (PathOrArchiveOrFSName != NULL) // data are valid
    {
        int failReason;
        BOOL clear = TRUE;
        if (Type == 0) // drive
        {
            if (!panel->ChangePathToDisk(panel->HWindow, PathOrArchiveOrFSName, TopIndex, FocusedName, NULL,
                                         TRUE, FALSE, FALSE, &failReason))
            {
                if (failReason == CHPPFR_CANNOTCLOSEPATH)
                {
                    ret = FALSE;   // we remain in place
                    clear = FALSE; // no jump, no need to clear stored top indices
                }
            }
        }
        else
        {
            if (Type == 1) // archive
            {
                if (!panel->ChangePathToArchive(PathOrArchiveOrFSName, ArchivePathOrFSUserPart, TopIndex,
                                                FocusedName, FALSE, NULL, TRUE, &failReason, FALSE, FALSE, TRUE))
                {
                    if (failReason == CHPPFR_CANNOTCLOSEPATH)
                    {
                        ret = FALSE;   // we remain in place
                        clear = FALSE; // no jump, no need to clear stored top indices
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
                    // if the FS interface where the path was last opened is known, try to
                    // find it among the detached ones and reuse it
                    if (MainWindow != NULL && PluginFS != NULL && // when the FS interface is known
                        (!panel->Is(ptPluginFS) ||                // and if it is not currently in the panel
                         !panel->GetPluginFS()->Contains(PluginFS)))
                    {
                        CDetachedFSList* list = MainWindow->DetachedFSList;
                        int i;
                        for (i = 0; i < list->Count; i++)
                        {
                            if (list->At(i)->Contains(PluginFS))
                            {
                                done = TRUE;
                                // we try to switch to the requested path (it was there last time, no need to test IsOurPath),
                                // and attach the detached FS at the same time
                                if (!panel->ChangePathToDetachedFS(i, TopIndex, FocusedName, TRUE, &failReason,
                                                                   PathOrArchiveOrFSName, ArchivePathOrFSUserPart))
                                {
                                    if (failReason == CHPPFR_CANNOTCLOSEPATH)
                                    {
                                        ret = FALSE;   // we remain in place
                                        clear = FALSE; // no jump, no need to clear stored top indices
                                    }
                                }

                                break; // done, no further match with PluginFS is possible
                            }
                        }
                    }

                    // if the previous part failed and the FS interface in the panel cannot list the path,
                    // we try to find a detached FS interface that can list it (to avoid opening a new FS unnecessarily)
                    int fsNameIndex;
                    BOOL convertPathToInternalDummy = FALSE;
                    if (!done && MainWindow != NULL &&
                        (!panel->Is(ptPluginFS) || // the FS interface in the panel cannot list the path
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
                                // we try to switch to the requested path while attaching the detached FS
                                if (!panel->ChangePathToDetachedFS(i, TopIndex, FocusedName, TRUE, &failReason,
                                                                   PathOrArchiveOrFSName, ArchivePathOrFSUserPart))
                                {
                                    if (failReason == CHPPFR_SHORTERPATH) // almost success (the path was only shortened) (CHPPFR_FILENAMEFOCUSED cannot occur here)
                                    {                                     // refresh the record about the FS interface
                                        if (panel->Is(ptPluginFS))
                                            PluginFS = panel->GetPluginFS()->GetInterface();
                                    }
                                    if (failReason == CHPPFR_CANNOTCLOSEPATH)
                                    {
                                        ret = FALSE;   // remain in place
                                        clear = FALSE; // no jump, no need to clear stored top indices
                                    }
                                }
                                else // full success
                                {    // refresh the record about the FS interface
                                    if (panel->Is(ptPluginFS))
                                        PluginFS = panel->GetPluginFS()->GetInterface();
                                }

                                break;
                            }
                        }
                    }

                    // if nothing else works, we open a new FS interface or just change the path on the active FS interface
                    if (!done)
                    {
                        if (!panel->ChangePathToPluginFS(PathOrArchiveOrFSName, ArchivePathOrFSUserPart, TopIndex,
                                                         FocusedName, FALSE, 2, NULL, TRUE, &failReason))
                        {
                            if (failReason == CHPPFR_SHORTERPATH ||   // almost success (the path was only shortened)
                                failReason == CHPPFR_FILENAMEFOCUSED) // almost success (the path only changed to a file and that file was focused)
                            {                                         // refresh the record about the FS interface
                                if (panel->Is(ptPluginFS))
                                    PluginFS = panel->GetPluginFS()->GetInterface();
                            }
                            if (failReason == CHPPFR_CANNOTCLOSEPATH)
                            {
                                ret = FALSE;   // we remain in place
                                clear = FALSE; // no jump, no need to clear stored top indices
                            }
                        }
                        else // full success
                        {    // refresh the record about the FS interface
                            if (panel->Is(ptPluginFS))
                                PluginFS = panel->GetPluginFS()->GetInterface();
                        }
                    }
                }
            }
        }
        if (clear)
            panel->TopIndexMem.Clear(); // long jump
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
        if (Type == 0) // drive
        {
            GetPath(buf1, 2 * MAX_PATH);
            item.GetPath(buf2, 2 * MAX_PATH);
            if (StrICmp(buf1, buf2) == 0)
                return TRUE;
        }
        else
        {
            if (Type == 1) // archive
            {
                if (StrICmp(PathOrArchiveOrFSName, item.PathOrArchiveOrFSName) == 0 &&  // the archive file is case-insensitive
                    strcmp(ArchivePathOrFSUserPart, item.ArchivePathOrFSUserPart) == 0) // the path inside the archive is case-sensitive
                {
                    return TRUE;
                }
            }
            else
            {
                if (Type == 2) // FS
                {
                    if (StrICmp(PathOrArchiveOrFSName, item.PathOrArchiveOrFSName) == 0) // the FS name is case-insensitive
                    {
                        if (strcmp(ArchivePathOrFSUserPart, item.ArchivePathOrFSUserPart) == 0) // the FS user part is case-sensitive
                            return TRUE;
                        if (curPluginFS != NULL && // we handle the scenario where both FS user parts match because the FS returns TRUE from IsCurrentPath for them (we would generally need to implement a method to compare two FS user parts, but that feels excessive just for history purposes; maybe later...)
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
        NewItem->PluginFS = NULL; // the FS was just closed -> reset to NULL
    }
    int i;
    for (i = 0; i < Paths.Count; i++)
    {
        CPathHistoryItem* item = Paths[i];
        if (item->Type == 2 && item->PluginFS == fs)
            item->PluginFS = NULL; // the FS was just closed -> reset to NULL
    }
}

void CPathHistory::FillBackForwardPopupMenu(CMenuPopup* popup, BOOL forward)
{
    // Item IDs must be in the <1..?> range
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

    int added = 0; // number of added items

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
        // insert the separator
        mii.Mask = MENU_MASK_TYPE;
        mii.Type = MENU_TYPE_SEPARATOR;
        popup->InsertItem(firstIndex, TRUE, &mii);
    }
}

void CPathHistory::Execute(int index, BOOL forward, CFilesWindow* panel, BOOL allItems, BOOL removeItem)
{
    if (Lock)
        return;

    CPathHistoryItem* item = NULL; // if the path should be removed, keep a pointer for later lookup

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
                    item = NULL; // failed to change the path => keep it in the history
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
        index--; // because numbering starts at 2 in FillPopupMenu
        if (HasBackward() || allItems && HasPaths())
        {
            int count = ((ForwardIndex == -1) ? Paths.Count : ForwardIndex) - 1;
            if (count - index >= 0) // there is a destination (not the last item)
            {
                if (count - index < Paths.Count)
                {
                    Lock = TRUE;
                    item = Paths[count - index];
                    change = item->Execute(panel);
                    if (!change)
                        item = NULL; // failed to change the path => keep it in the history
                    Lock = FALSE;
                }
                if (change && !DontChangeForwardIndex)
                    ForwardIndex = count - index + 1;
            }
        }
    }
    IdleRefreshStates = TRUE; // force a status-variable check on the next Idle

    if (NewItem != NULL)
    {
        AddPathUnique(NewItem->Type, NewItem->PathOrArchiveOrFSName, NewItem->ArchivePathOrFSUserPart,
                      NewItem->HIcon, NewItem->PluginFS, NULL);
        NewItem->HIcon = NULL; // AddPathUnique now owns the responsibility for destroying the icon
        delete NewItem;
        NewItem = NULL;
    }
    if (removeItem && item != NULL)
    {
        if (DontChangeForwardIndex)
        {
            // remove the executed item from the list
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

        if (n2 != NULL && n.IsTheSamePath(*n2, curPluginFS)) // same paths -> update the data
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
            if (n.IsTheSamePath(*n2, curPluginFS)) // same paths -> remove the record
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
            return; // same paths -> nothing to do
        }
    }

    // the path really needs to be added ...
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
            HANDLES(DestroyIcon(hIcon)); // must destroy the icon
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
                { // FS case: replace pluginFS (so that the path is opened in the last filesystem of this path)
                    item->PluginFS = pluginFS;
                }
                delete n;
                if (i < Paths.Count - 1)
                {
                    // move the path to the top of the list
                    Paths.Add(item);
                    if (Paths.IsGood())
                        Paths.Detach(i); // if the addition succeeded, remove the original entry
                    if (!Paths.IsGood())
                        Paths.ResetState();
                }
                return; // same paths -> nothing to do
            }
        }
    }

    // the path really needs to be added ...
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

        if (!onlyClear) // if we are not just clearing the key, store the history values
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
                case 0: // drive
                {
                    strcpy(path, item->PathOrArchiveOrFSName);
                    break;
                }

                // archive & FS: use ':' character to separate the two parts of the path
                // during loading, determine the path type based on this character
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
                    // the path can be of type
                    // 0 (drive): "C:\???" or "\\server\???"
                    // 1 (archive): "C:\???:" or "\\server\???:"
                    // 2 (FS): "XY:???"
                    type = -1; // do not add
                    if ((path[0] == '\\' && path[1] == '\\') || path[1] == ':')
                    {
                        // this is type==0 (drive) or type==1 (archive)
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
                        // candidate for an FS path
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
            HICON icon = item->LoadedIcon; // set LoadedIcon to NULL; otherwise DestroyIcon() would deallocate it
            item->Clear();                 // avoid compressing the array when deleting (slow and unnecessary); just clear the item so it can be skipped faster during searching
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
    if (UserMenuIIU_BkgndReaderData != NULL) // they truly are no longer needed, release them now
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
    // required so GetFileOrPathIconAux works (it relies on COM/OLE stuff)
    if (OleInitialize(NULL) != S_OK)
        TRACE_E("Error in OleInitialize.");

    CUserMenuIconDataArr* bkgndReaderData = (CUserMenuIconDataArr*)param;
    DWORD threadID = bkgndReaderData->GetIRThreadID();

    for (int i = 0; UserMenuIconBkgndReader.IsCurrentIRThreadID(threadID) && i < bkgndReaderData->Count; i++)
    {
        CUserMenuIconData* item = bkgndReaderData->At(i);
        HICON umIcon;
        if (item->FileName[0] != 0 &&
            SalGetFileAttributes(item->FileName) != INVALID_FILE_ATTRIBUTES && // accessibility check (instead of CheckPath)
            ExtractIconEx(item->FileName, item->IconIndex, NULL, &umIcon, 1) == 1)
        {
            HANDLES_ADD(__htIcon, __hoLoadImage, umIcon); // add the 'umIcon' handle to HANDLES
        }
        else
        {
            umIcon = NULL;
            if (item->UMCommand[0] != 0)
            { // if the previous attempt failed, try to obtain the icon from the system
                DWORD attrs = SalGetFileAttributes(item->UMCommand);
                if (attrs != INVALID_FILE_ATTRIBUTES) // accessibility check (instead of CheckPath)
                {
                    umIcon = GetFileOrPathIconAux(item->UMCommand, FALSE,
                                                  (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)));
                }
            }
        }
        item->LoadedIcon = umIcon; // store the result: the loaded icon or NULL if it failed
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
        TerminateProcess(GetCurrentProcess(), 1); // a harder exit (this one still performs some calls)
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
            // the main thread runs at a higher priority; to keep icons loading as fast as it was before moving work to a background thread,
            // boost the priority here as well
            SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);

            bkgndReaderData = NULL; // they are handed over to the thread, do not release them here
            CurIRThreadIDIsValid = TRUE;
            CurIRThreadID = newThreadID;
            AddAuxThread(thread); // if the thread does not finish in time, kill it before shutting down the app
        }
        else
            TRACE_E("CUserMenuIconBkgndReader::StartBkgndReadingIcons(): unable to start thread for reading user menu icons.");
    }
    if (bkgndReaderData != NULL)
        delete bkgndReaderData;
    HANDLES(LeaveCriticalSection(&CS));

    // we pause briefly; if the icons load quickly, the "simple" variants will not be shown at all (less flickering)
    // additionally, some users also reported that due to the current simultaneous loading of icons into the panel,
    // loading icons into the user menu is significantly slowed down, causing the icons
    // on the user menu toolbar to appear with a large delay, which is ugly.
    // This delay should prevent that (it will simply handle only slow loading of user menu icons, which is the goal of this whole tasker).
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

    if (ok) // the User Menu is still waiting for these icons
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
        { // last lock released: if there is pending data to process, send it
            if (CurIRThreadIDIsValid && CurIRThreadID == UserMenuIIU_ThreadID)
            {
                PostMessage(MainWindow->HWindow, WM_USER_USERMENUICONS_READY,
                            (WPARAM)UserMenuIIU_BkgndReaderData, (LPARAM)UserMenuIIU_ThreadID);
            }
            else // nobody needs the data anymore, just release it
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
            if (UserMenuIIU_BkgndReaderData != NULL) // release any previously stored data (e.g. entering the config dialog mid-load, then changing colors and coming here again)
                delete UserMenuIIU_BkgndReaderData;
            UserMenuIIU_BkgndReaderData = *bkgndReaderData;
            UserMenuIIU_ThreadID = threadID;
            *bkgndReaderData = NULL; // the caller handed over the data; we'll release them later ourselves
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
    CurIRThreadIDIsValid = FALSE; // the icons are now handed over to the user menu (IsReadingIcons() must return FALSE)
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
        if (bkgndReaderData == NULL) // this is a copy for the config dialog; do not propagate newly loaded icons (wait until the dialog ends)
        {
            UMIcon = DuplicateIcon(NULL, item.UMIcon); // GetIconHandle() unnecessarily slowed things down. 
            if (UMIcon != NULL)                        // add the 'UMIcon' handle to HANDLES
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
    // umitSubmenuBegin shares a single icon
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
            // switching to the shared icon, remove the allocated one
            if (UMIcon != NULL)
            {
                HANDLES(DestroyIcon(UMIcon));
                UMIcon = NULL;
            }
        }
        if (Type == umitSubmenuBegin)
            UMIcon = NULL; // leaving the shared icon
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

    if (Type == umitSeparator) // separator has no icon
        return TRUE;

    // try to extract the icon from the specified file
    char fileName[MAX_PATH];
    fileName[0] = 0;
    DWORD iconIndex = -1;
    if (MainWindow != NULL && Icon != NULL && Icon[0] != 0)
    {
        // Icon has the format "file name,resID"
        // break it down
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

    if (bkgndReaderData == NULL && fileName[0] != 0 && // we have to load icons right here
        MainWindow->GetActivePanel() != NULL &&
        MainWindow->GetActivePanel()->CheckPath(FALSE, fileName) == ERROR_SUCCESS &&
        ExtractIconEx(fileName, iconIndex, NULL, &UMIcon, 1) == 1)
    {
        HANDLES_ADD(__htIcon, __hoLoadImage, UMIcon); // add the 'UMIcon' handle to HANDLES
        return TRUE;
    }

    // if the previous method failed, try to obtain the icon from the system
    char umCommand[MAX_PATH];
    if (MainWindow != NULL && UMCommand != NULL && UMCommand[0] != 0 &&
        ExpandCommand(MainWindow->HWindow, UMCommand, umCommand, MAX_PATH, TRUE))
    {
        while (strlen(umCommand) > 2 && CutDoubleQuotesFromBothSides(umCommand))
            ;
    }
    else
        umCommand[0] = 0;

    if (bkgndReaderData == NULL && umCommand[0] != 0 && // we have to load icons right here
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
        if (getIconsFromReader) // icons are already loaded, just take the right one
        {
            UMIcon = bkgndReaderData->GiveIconForUMI(fileName, iconIndex, umCommand);
            if (UMIcon != NULL)
                return TRUE;
        }
        else // request loading the required icon
            bkgndReaderData->Add(new CUserMenuIconData(fileName, iconIndex, umCommand));
    }

    // retrieve the default icon from shell32.dll
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
        UserMenuIconBkgndReader.StartBkgndReadingIcons(bkgndReaderData); // NOTE: this call releases 'bkgndReaderData'
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
    // find the window under the mouse cursor
    HWND hWindow = WindowFromPoint(pMSG->pt);
    if (hWindow != NULL)
    {
        char className[101];
        className[0] = 0;
        if (GetClassName(hWindow, className, 100) != 0)
        {
            // some Synaptics touchpad versions (for example on HP notebooks) display their own window with a scroll icon
            // under the cursor; in that case, we do not route the message to the "proper" window below the cursor
            // because the touchpad handles it on its own
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
                if (winProcessId != GetCurrentProcessId()) // sending WM_USER_* outside our process makes no sense
                    hWindow = pMSG->hwnd;
            }
        }
        else
        {
            TRACE_E("GetClassName() failed!");
            hWindow = pMSG->hwnd;
        }
        // if this is a scrollbar with a parent window, post the message to the parent.
        // Scrollbars in the panels are not subclassed, so this is currently the only way
        // for the panel to learn about the wheel when the cursor is over the scroll bar.
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
    MessagesKeeper.Add(pMSG); // keep a message history in case Salamander crashes

    // we only care about WM_MOUSEWHEEL and WM_MOUSEHWHEEL
    //
    // 7 Oct 2009 - AS253_B1_IB34: Manison reported that horizontal scrolling did not work for him on Windows Vista.
    // It worked for me (through this hook). After installing Intellipoint drivers v7 (previously I had no special drivers installed on Vista x64)
    // WM_MOUSEHWHEEL messages stopped flowing through here and went directly into the Salamander panels.
    // Therefore, I disabled this path and we will handle the messages only in the panel.
    // Note: we could probably cut off WM_MOUSEWHEEL handling in the same way, but I do not want to risk breaking something on older OSes
    // (we can revisit this when moving to Windows 2000 and later).
    // Note 2: if it turns out we must capture WM_MOUSEHWHEEL via this hook as well, we should implemenent runtime detection that the messages
    // pass through here and then disable their processing in the panels and the command line.

    // 30 Nov 2012 - a user appeared on the forum for whom WM_MOUSEHWHEEL does not get through the message hook (the same as previously with Manison in the case of WM_MOUSEHWHEEL):
    // https://forum.altap.cz/viewtopic.php?f=24&t=6039
    // so from now on, we will also capture the message in each window that might receive it (depending on focus)
    // and then route it so it is delivered to the window under the cursor, just as we always did.

    // currently we let both WM_MOUSEWHEEL and WM_MOUSEHWHEEL pass through and wait for feedback from beta testers

    if ((pMSG->message != WM_MOUSEWHEEL && pMSG->message != WM_MOUSEHWHEEL) || (wParam == PM_NOREMOVE))
        return retValue;

    // if the message arrived "recently" through the other channel, ignore this one
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
    HOldMouseWheelHookProc = SetWindowsHookEx(WH_GETMESSAGE, // HANDLES cannot manage this!
                                              MenuWheelHookProc,
                                              NULL, threadID);
    return (HOldMouseWheelHookProc != NULL);
}

BOOL ReleaseMenuWheelHook()
{
    // unhook mouse messages
    if (HOldMouseWheelHookProc != NULL)
    {
        UnhookWindowsHookEx(HOldMouseWheelHookProc); // HANDLES cannot manage this!
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
    if (ZIPRoot != NULL) // zip-root has no '\\' at the beginning or at the end.
    {
        int l = (int)strlen(ZIPRoot);
        if (l > 0 && ZIPRoot[l - 1] == '\\')
            ZIPRoot[l - 1] = 0;
    }
    SourcePath = DupStr(sourcePath);
    if (SourcePath != NULL) // ensure the source path has no trailing '\\'
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

    // test whether it is already present (performed after constructing the item because the strings were adjusted - '\\')
    int i;
    for (i = 0; i < List.Count; i++)
    {
        CFileTimeStampsItem* item2 = List[i];
        if (StrICmp(item->FileName, item2->FileName) == 0 &&
            StrICmp(item->SourcePath, item2->SourcePath) == 0)
        {
            delete item;
            return FALSE; // already present, do not add another one
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
{ // we enumerate only files, so enumFiles can be ignored entirely
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
        int index = indexes[count - i - 1];   // remove from the end—less shifting and no index changes
        if (index < List.Count && index >= 0) // just to be safe
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
            if (index < List.Count && index >= 0) // just to be safe
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
        fromStr.Add("\0", 2); // append two extra nulls just in case (no Add call needed, this works too)
        toStr.Add("\0", 2);   // append two extra nulls just in case (no Add call needed, this works too)

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
            lstrcpyn(title, LoadStr(IDS_BROWSEARCUPDATE), 100); // make a copy; LoadStr is used by other threads as well
            fo.lpszProgressTitle = title;
            // perform the actual copying — it is wonderfully easy, though it does crash for some users now and then ;-)
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
    //--- remove files that have not changed from the list
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
            if (CompareFileTime(&data.ftLastWriteTime, &item->LastWrite) != 0 ||    // timestamps differ
                CQuadWord(data.nFileSizeLow, data.nFileSizeHigh) != item->FileSize) // sizes differ
            {
                item->FileSize = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh); // use the new size
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
        // during a critical shutdown we pretend the updated files do not exist; we cannot re-pack them into the archive,
        // but we must not delete them either — after startup, the user must still have a chance to manually pack the updated files into the archive
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
                    //--- pack the modified files, grouped by identical zip root and source path
                    TIndirectArray<CFileTimeStampsItem> packList(10, 5); // list of all entries with the same zip root and source path
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
                        for (i = List.Count - 1; i >= 0; i--) // quadratic complexity should not be an issue here
                        {                                     // iterate backwards because Detach is easier that way
                            CFileTimeStampsItem* item2 = List[i];
                            char* r2 = item2->ZIPRoot;
                            char* s2 = item2->SourcePath;
                            if (strcmp(r1, r2) == 0 && // identical zip root (case-sensitive comparison required - update test\A.txt and Test\b.txt must not run simultaneously)
                                StrICmp(s1, s2) == 0)  // identical source path
                            {
                                packList.Add(item2);
                                List.Detach(i);
                            }
                        }

                        // invoke pack for packList
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
                                if (!loop) // "Cancel": detach files from the disk cache, otherwise they would be deleted
                                {
                                    List.Add(packList.GetData(), packList.Count);
                                    packList.DetachMembers();
                                    showDlg = TRUE; // show the Archive Update dialog again (with the remaining files)
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
    // check whether 'path' follows 'Path' (path == Path + "\\name")
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

    if (ok) // it follows -> remember the next top index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // it is necessary to drop the first stored top index
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // not sequential -> first top index in the series
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(const char* path, int& topIndex)
{
    // determine whether 'path' matches Path (path == Path)
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
        else // we no longer have this value (it was never stored or was dropped due to low memory)
        {
            Clear();
            return FALSE;
        }
    }
    else // querying a different path -> clear the memory because a long jump occurred
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

    // search existing items to see if the item being added is already present
    int i;
    for (i = 0; i < Files.Count; i++)
    {
        CFileHistoryItem* item = Files[i];
        if (item->Equal(type, handlerID, fileName))
        {
            // if it is, just bring it to the top
            if (i > 0)
            {
                Files.Detach(i);
                    if (!Files.IsGood())
                    Files.ResetState(); // cannot fail; it only reports an out-of-memory condition when shrinking the array
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

    // item does not exist — insert it at the top
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
    // trim the list to 30 entries
    if (Files.Count > 30)
        Files.Delete(30);

    return TRUE;
}

BOOL CFileHistory::FillPopupMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFileHistory::FillPopupMenu()");

    // add the menu items
    char name[2 * MAX_PATH];
    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_ICON | MENU_MASK_STRING;
    mii.Type = MENU_TYPE_STRING;
    mii.String = name;
    int i;
    for (i = 0; i < Files.Count; i++)
    {
        CFileHistoryItem* item = Files[i];

        // separate the name from the path with '\t' character so it appears in a separate column
        lstrcpy(name, item->FileName);
        char* ptr = strrchr(name, '\\');
        if (ptr == NULL)
            return FALSE;
        memmove(ptr + 1, ptr, lstrlen(ptr) + 1);
        *(ptr + 1) = '\t';
        const char* text = "";
        // duplicate '&' so it is not rendered as an underline
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
        sprintf(name + lstrlen(name), "\t(%s)", text); // append the way the file is opened
        mii.ID = i + 1;
        popup->InsertItem(-1, TRUE, &mii);
    }
    if (i > 0)
    {
        popup->SetStyle(MENU_POPUP_THREECOLUMNS); // the first two columns are left-aligned
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
#define DIRECTORY_COMMAND_LEFT 3      // path from the left panel
#define DIRECTORY_COMMAND_RIGHT 4     // path from the right panel
#define DIRECTORY_COMMAND_HOTPATHF 5  // first hot path
#define DIRECTORY_COMMAND_HOTPATHL 35 // last hot path

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

    /* Used by the export_mnu.py script, which generates salmenu.mnu for Translator.
       Keep it synchronized with the InsertItem() calls below...
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

    // append hot paths if any exist
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
    // since Windows Vista, SelectAll works properly by default, so we leave Select All enabled there.
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
        GetWindowText(hDialog, caption, 100); // use the same caption as the dialog
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
    BOOL SkipCharacter; // prevents a beep for keys we process
    HWND HDialog;       // dialog that will receive WM_USER_KEYDOWN
    int CtrlID;         // identifier forwarded via WM_USER_KEYDOWN

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
        SkipCharacter = TRUE; // suppress the beep
        BOOL ret = (BOOL)SendMessage(HDialog, WM_USER_KEYDOWN, MAKELPARAM(CtrlID, 0), wParam);
        if (ret)
            return 0;
        SkipCharacter = FALSE;
        break;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        SkipCharacter = FALSE; // reset just in case
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
        // it might be a combo box; try to reach its internal edit control
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
