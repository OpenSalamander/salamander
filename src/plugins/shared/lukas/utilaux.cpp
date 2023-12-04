// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

char* Replace(char* string, char s, char d)
{
    CALL_STACK_MESSAGE3("Replace(, %u, %u)", s, d);
    char* iterator = string;
    while (*iterator)
    {
        if (*iterator == s)
            *iterator = d;
        iterator++;
    }
    return string;
}

BOOL GetOpenFileName(HWND parent, const char* title, const char* filter,
                     char* buffer, BOOL save)
{
    CALL_STACK_MESSAGE4("GetOpenFileName(, %s, %s, , %d)", title, filter, save);
    OPENFILENAME ofn;
    char buf[200];
    char fileName[MAX_PATH];
    lstrcpyn(buf, filter, 200);
    Replace(buf, '\t', '\0');

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = buf;
    DWORD attr = SG->SalGetFileAttributes(buffer);
    if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        fileName[0] = 0;
        ofn.lpstrInitialDir = buffer;
    }
    else
        strcpy(fileName, buffer);
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    //ofn.lpfnHook = OFNHookProc;
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR /*| OFN_ENABLEHOOK*/;

    BOOL ret;
    if (save)
        ret = SG->SafeGetSaveFileName(&ofn);
    else
    {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        ret = SG->SafeGetOpenFileName(&ofn);
    }

    if (ret)
        strcpy(buffer, fileName);

    return ret;
}

BOOL FileErrorL(int lastError, HWND parent, const char* fileName, int error,
                BOOL retry, BOOL* skip, BOOL* skipAll, int title)
{
    CALL_STACK_MESSAGE1("FileError()");

    char buffer[1024];
    strcpy(buffer, LoadStr(error));
    int len = int(strlen(buffer));
    if (lastError != NO_ERROR)
        SG->GetErrorText(lastError, buffer + len, 1024 - len);

    if (title == -1)
        title = IDS_SPLERROR;

    if (skipAll && *skipAll)
    {
        if (skip)
            *skip = TRUE;
        return FALSE;
    }

    int ret;
    if (retry)
    {
        if (skip)
            ret = SG->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName, buffer, LoadStr(title));
        else
            ret = SG->DialogError(parent, BUTTONS_RETRYCANCEL, fileName, buffer, LoadStr(title));
    }
    else
    {
        if (skip)
            ret = SG->DialogError(parent, BUTTONS_SKIPCANCEL, fileName, buffer, LoadStr(title));
        else
            ret = SG->DialogError(parent, BUTTONS_OK, fileName, buffer, LoadStr(title));
    }

    switch (ret)
    {
    case DIALOG_RETRY:
        return TRUE;

    case DIALOG_SKIPALL:
        if (skipAll)
            *skipAll = TRUE;

    case DIALOG_SKIP:
        if (skip)
            *skip = TRUE;
        return FALSE;

    //case DIALOG_OK:
    //case DIALOG_CANCEL:
    default:
        if (skip)
            *skip = FALSE;
        return FALSE;
    }
}

// ****************************************************************************
//
// CSynchronizedCounter
//

CSynchronizedCounter::CSynchronizedCounter()
{
    CALL_STACK_MESSAGE_NONE
    Counter = 0;
    ChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CSynchronizedCounter::~CSynchronizedCounter()
{
    CALL_STACK_MESSAGE_NONE
    CloseHandle(ChangeEvent);
}

int CSynchronizedCounter::Up()
{
    CALL_STACK_MESSAGE_NONE
    CS.Enter();
    int ret = ++Counter;
    SetEvent(ChangeEvent);
    CS.Leave();
    return ret;
}

int CSynchronizedCounter::Down()
{
    CALL_STACK_MESSAGE_NONE
    CS.Enter();
    int ret = --Counter;
    SetEvent(ChangeEvent);
    CS.Leave();
    return ret;
}

int CSynchronizedCounter::Value()
{
    CALL_STACK_MESSAGE_NONE
    CS.Enter();
    int ret = Counter;
    ResetEvent(ChangeEvent);
    CS.Leave();
    return ret;
}

DWORD
CSynchronizedCounter::WaitForChange()
{
    CALL_STACK_MESSAGE_NONE
    return WaitForSingleObject(ChangeEvent, INFINITE);
}

// ****************************************************************************
//
// CArgv
//

CArgv::CArgv(const char* commandLine) : TIndirectArray<char>(8, 8)
{
    CALL_STACK_MESSAGE2("CArgv::CArgv(%s)", commandLine);
    const char* start = commandLine;
    while (*start)
    {
        // orizneme white-space na zacatku
        while (*start && IsSpace(*start))
            start++;
        if (!*start)
            break;
        const char* end = start;
        // najdeme konec tokenu
        while (*end && !IsSpace(*end))
        {
            if (*end++ == '"')
            {
                end = strchr(end, '"');
                if (end)
                    end++;
                else
                    end = start + strlen(start);
            }
        }
        // pridame token do pole
        char* str = DupStr(start, end);
        RemoveCharacters(str, str, "\"");
        Add(str);
        start = end;
    }
}

// ****************************************************************************

char* DupStr(const char* begin, const char* end)
{
    CALL_STACK_MESSAGE_NONE
    size_t len = end - begin;
    char* ret = (char*)malloc(len + 1);
    memcpy(ret, begin, len);
    ret[len] = 0;
    return ret;
}

int RemoveCharacters(char* dest, const char* source, const char* charSet)
{
    CALL_STACK_MESSAGE_NONE
    int d = 0, s = 0;
    while (source[s])
    {
        if (strchr(charSet, source[s]))
            s++;
        else
            dest[d++] = source[s++];
    }
    dest[d] = 0;
    return d;
}

BOOL SalGetFullName(char* name, int* errTextID, const char* curDir)
{
    CALL_STACK_MESSAGE3("SalGetFullName(%s, , %s)", name, curDir);
    int err = 0;

    size_t rootOffset = 3; // offset zacatku adresarove casti cesty (3 pro "c:\path")
    char* s = name;
    while (*s == ' ')
        s++;
    if (*s == '\\' && *(s + 1) == '\\') // UNC (\\server\share\...)
    {                                   // eliminace mezer na zacatku cesty
        if (s != name)
            memmove(name, s, strlen(s) + 1);
        s = name + 2;
        if (*s == 0 || *s == '\\')
            err = GFN_SERVERNAMEMISSING;
        else
        {
            while (*s != 0 && *s != '\\')
                s++; // prejeti servername
            if (*s == '\\')
                s++;
            if (*s == 0 || *s == '\\')
                err = GFN_SHARENAMEMISSING;
            else
            {
                while (*s != 0 && *s != '\\')
                    s++; // prejeti sharename
                if (*s == '\\')
                    s++;
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
                    /*
          int l1 = strlen(s + 2);  // delka zbytku ("path...")
          if (SalamanderGeneral->CharToLowerCase(*s) >= 'a' && SalamanderGeneral->CharToLowerCase(*s) <= 'z')
          {
            const char *head;
            if (curDir != NULL && SalamanderGeneral->CharToLowerCase(curDir[0]) == SalamanderGeneral->CharToLowerCase(*s)) head = curDir;
            else head = DefaultDir[LowerCase[*s] - 'a'];
            int l2 = strlen(head);
            if (head[l2 - 1] != '\\') l2++;  // misto pro '\\'
            if (l1 + l2 >= MAX_PATH) err = GFN_TOOLONGPATH;
            else  // sestaveni full path
            {
              memmove(name + l2, s + 2, l1 + 1);
              *(name + l2 - 1) = '\\';
              memmove(name, head, l2 - 1);
            }
          }
          else err = GFN_INVALIDDRIVE;
          */
                    err = GFN_INVALIDDRIVE;
                }
            }
            else
            {
                size_t l1 = strlen(s);
                if (curDir != NULL)
                {
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
                            if (l1 + (root - curDir) >= MAX_PATH)
                                err = GFN_TOOLONGPATH;
                            else // sestaveni cesty z rootu akt. disku
                            {
                                memmove(name + (root - curDir), s, l1 + 1);
                                memmove(name, curDir, root - curDir);
                            }
                            rootOffset = (root - curDir) + 1;
                        }
                        else
                        {
                            if (l1 + 2 >= MAX_PATH)
                                err = GFN_TOOLONGPATH;
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
                        /*
            if (nextFocus != NULL)
            {
              char *test = name;
              while (*test != 0 && *test != '\\') test++;
              if (*test == 0) strcpy(nextFocus, name);
            }
            */

                        size_t l2 = strlen(curDir);
                        if (curDir[l2 - 1] != '\\')
                            l2++;
                        if (l1 + l2 >= MAX_PATH)
                            err = GFN_TOOLONGPATH;
                        else
                        {
                            memmove(name + l2, s, l1 + 1);
                            name[l2 - 1] = '\\';
                            memmove(name, curDir, l2 - 1);
                        }
                    }
                }
                else
                    err = GFN_INCOMLETEFILENAME;
            }
            s = name + rootOffset;
        }
        else
        {
            name[0] = 0;
            err = GFN_EMPTYNAMENOTALLOWED;
        }
    }

    if (err == 0) // eliminace '.' a '..' v ceste
    {
        if (!SG->SalRemovePointsFromPath(s))
            err = GFN_PATHISINVALID;
    }

    if (err == 0) // vyhozeni pripadneho nezadouciho backslashe z konce retezce
    {
        size_t l = strlen(name);
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
        else // UNC cesta
        {
            if (l > 0 && name[l - 1] == '\\')
                name[l - 1] = 0; // orez backslashe
        }
    }

    if (errTextID != NULL)
        *errTextID = err;

    return err == 0;
}
