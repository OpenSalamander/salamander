// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

BOOL SalGetFullName(LPTSTR name, int* errTextID, LPCTSTR curDir)
{
    CALL_STACK_MESSAGE3(_T("SalGetFullName(%s, , %s)"), name, curDir);
    int err = 0;

    int rootOffset = 3; // offset of the start of the directory part of the path (3 for "c:\path")
    LPTSTR s = name;
    while (*s == ' ')
        s++;
    if (*s == '\\' && *(s + 1) == '\\') // UNC (\\server\share\...)
    {                                   // remove leading spaces from the path
        if (s != name)
            memmove(name, s, (_tcslen(s) + 1) * sizeof(TCHAR));
        s = name + 2;
        if (*s == 0 || *s == '\\')
            err = GFN_SERVERNAMEMISSING;
        else
        {
            while (*s != 0 && *s != '\\')
                s++; // skip the server name
            if (*s == '\\')
                s++;
            if (*s == 0 || *s == '\\')
                err = GFN_SHARENAMEMISSING;
            else
            {
                while (*s != 0 && *s != '\\')
                    s++; // skip the share name
                if (*s == '\\')
                    s++;
            }
        }
    }
    else // path specified using a drive (c:\...)
    {
        if (*s != 0)
        {
            if (*(s + 1) == ':') // "c:..."
            {
                if (*(s + 2) == '\\') // "c:\..."
                {                     // remove leading spaces from the path
                    if (s != name)
                        memmove(name, s, (_tcslen(s) + 1) * sizeof(TCHAR));
                }
                else // "c:path..."
                {
                    /*
          size_t l1 = _tcslen(s + 2);  // length of the remainder ("path...")
          if (SalamanderGeneral->CharToLowerCase(*s) >= 'a' && SalamanderGeneral->CharToLowerCase(*s) <= 'z')
          {
            const char *head;
            if (curDir != NULL && SalamanderGeneral->CharToLowerCase(curDir[0]) == SalamanderGeneral->CharToLowerCase(*s)) head = curDir;
            else head = DefaultDir[LowerCase[*s] - 'a'];
            int l2 = _tcslen(head);
            if (head[l2 - 1] != '\\') l2++;  // space for '\\'
            if (l1 + l2 >= MAX_PATH) err = GFN_TOOLONGPATH;
            else  // build the full path
            {
              memmove(name + l2, s + 2, (l1 + 1)*sizeof(TCHAR));
              *(name + l2 - 1) = '\\';
              memmove(name, head, (l2 - 1)*sizeof(TCHAR));
            }
          }
          else err = GFN_INVALIDDRIVE;
          */
                    err = GFN_INVALIDDRIVE;
                }
            }
            else
            {
                size_t l1 = _tcslen(s);
                if (curDir != NULL)
                {
                    if (*s == '\\') // "\path...."
                    {
                        if (curDir[0] == '\\' && curDir[1] == '\\') // UNC
                        {
                            LPCTSTR root = curDir + 2;
                            while (*root != 0 && *root != '\\')
                                root++;
                            root++; // '\\'
                            while (*root != 0 && *root != '\\')
                                root++;
                            if (l1 + (root - curDir) >= MAX_PATH)
                                err = GFN_TOOLONGPATH;
                            else // build the path from the root of the current drive
                            {
                                memmove(name + (root - curDir), s, (l1 + 1) * sizeof(TCHAR));
                                memmove(name, curDir, (root - curDir) * sizeof(TCHAR));
                            }
                            rootOffset = (int)(root - curDir) + 1;
                        }
                        else
                        {
                            if (l1 + 2 >= MAX_PATH)
                                err = GFN_TOOLONGPATH;
                            else
                            {
                                memmove(name + 2, s, (l1 + 1) * sizeof(TCHAR));
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

                        size_t l2 = _tcslen(curDir);
                        if (curDir[l2 - 1] != '\\')
                            l2++;
                        if (l1 + l2 >= MAX_PATH)
                            err = GFN_TOOLONGPATH;
                        else
                        {
                            memmove(name + l2, s, (l1 + 1) * sizeof(TCHAR));
                            name[l2 - 1] = '\\';
                            memmove(name, curDir, (l2 - 1) * sizeof(TCHAR));
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

    if (err == 0) // remove '.' and '..' from the path
    {
        if (!SalamanderGeneral->SalRemovePointsFromPath(s))
            err = GFN_PATHISINVALID;
    }

    if (err == 0) // remove any undesired trailing backslash from the end of the string
    {
        size_t l = _tcslen(name);
        if (l > 1 && name[1] == ':') // path type "c:\path"
        {
            if (l > 3) // not a root path
            {
                if (name[l - 1] == '\\')
                    name[l - 1] = 0; // trim the trailing backslash
            }
            else
            {
                name[2] = '\\'; // root path, backslash is required ("c:\")
                name[3] = 0;
            }
        }
        else // UNC path
        {
            if (l > 0 && name[l - 1] == '\\')
                name[l - 1] = 0; // trim the trailing backslash
        }
    }

    if (errTextID != NULL)
        *errTextID = err;

    return err == 0;
}
