// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

BOOL SalGetFullName(LPTSTR name, int* errTextID, LPCTSTR curDir)
{
    CALL_STACK_MESSAGE3(_T("SalGetFullName(%s, , %s)"), name, curDir);
    int err = 0;

    int rootOffset = 3; // offset zacatku adresarove casti cesty (3 pro "c:\path")
    LPTSTR s = name;
    while (*s == ' ')
        s++;
    if (*s == '\\' && *(s + 1) == '\\') // UNC (\\server\share\...)
    {                                   // eliminace mezer na zacatku cesty
        if (s != name)
            memmove(name, s, (_tcslen(s) + 1) * sizeof(TCHAR));
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
                        memmove(name, s, (_tcslen(s) + 1) * sizeof(TCHAR));
                }
                else // "c:path..."
                {
                    /*
          size_t l1 = _tcslen(s + 2);  // delka zbytku ("path...")
          if (SalamanderGeneral->CharToLowerCase(*s) >= 'a' && SalamanderGeneral->CharToLowerCase(*s) <= 'z')
          {
            const char *head;
            if (curDir != NULL && SalamanderGeneral->CharToLowerCase(curDir[0]) == SalamanderGeneral->CharToLowerCase(*s)) head = curDir;
            else head = DefaultDir[LowerCase[*s] - 'a'];
            int l2 = _tcslen(head);
            if (head[l2 - 1] != '\\') l2++;  // misto pro '\\'
            if (l1 + l2 >= MAX_PATH) err = GFN_TOOLONGPATH;
            else  // sestaveni full path
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
                            else // sestaveni cesty z rootu akt. disku
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

    if (err == 0) // eliminace '.' a '..' v ceste
    {
        if (!SalamanderGeneral->SalRemovePointsFromPath(s))
            err = GFN_PATHISINVALID;
    }

    if (err == 0) // vyhozeni pripadneho nezadouciho backslashe z konce retezce
    {
        size_t l = _tcslen(name);
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
