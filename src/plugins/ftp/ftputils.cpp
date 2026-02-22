// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

const char* FTP_ANONYMOUS = "anonymous"; // standard name for an anonymous user

BOOL FTPCutDirectory(CFTPServerPathType type, char* path, int pathBufSize,
                     char* cutDir, int cutDirBufSize, BOOL* fileNameCouldBeCut)
{
    CALL_STACK_MESSAGE5("FTPCutDirectory(%d, %s, %d, , %d,)", (int)type, path, pathBufSize, cutDirBufSize);
    if (cutDirBufSize > 0)
        cutDir[0] = 0;
    if (fileNameCouldBeCut)
        *fileNameCouldBeCut = TRUE;
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in FTPCutDirectory()");
        return FALSE;
    }
    int l = (int)strlen(path);
    if (pathBufSize < l + 1)
        pathBufSize = l + 1;
    switch (type)
    {
    case ftpsptUnix:
    case ftpsptAS400: // although it is not perfect, I think it will be enough (improvement: if it is a /qsys.lib path and the name ends with .mbr, cut two components and return them as the file name: "/QSYS.LIB/GARY.LIB/UCLSRC.FILE/BKPLIB2.MBR" -> "/QSYS.LIB/GARY.LIB" + "UCLSRC.FILE/BKPLIB2.MBR")
    {
        char* lastSlash = path + l - 1;
        while (--lastSlash >= path && *lastSlash != '/')
            ;
        char* prevSlash = lastSlash;
        while (--prevSlash >= path && *prevSlash != '/')
            ;
        if (lastSlash < path)
            return FALSE; // "somedir" or "/"
        if (cutDirBufSize > 0)
        {
            if (*(path + l - 1) == '/')
                *(path + --l) = 0; // removal of trailing '/'
            lstrcpyn(cutDir, lastSlash + 1, cutDirBufSize);
        }
        if (prevSlash < path)
            *(lastSlash + 1) = 0; // "/somedir" or "/somedir/" -> "/"
        else
            *lastSlash = 0; // "/firstdir/seconddir" or "/firstdir/seconddir/" -> "/firstdir"
        return TRUE;
    }

    case ftpsptWindows:
    case ftpsptNetware: // matches UNIX, only separators are both '/' and '\\'
    {
        char* lastSlash = path + l - 1;
        while (--lastSlash >= path && *lastSlash != '/' && *lastSlash != '\\')
            ;
        char* prevSlash = lastSlash;
        while (--prevSlash >= path && *prevSlash != '/' && *prevSlash != '\\')
            ;
        if (lastSlash < path)
            return FALSE; // "somedir" or "/"
        if (cutDirBufSize > 0)
        {
            if (*(path + l - 1) == '/' || *(path + l - 1) == '\\')
                *(path + --l) = 0; // removal of trailing '/'
            lstrcpyn(cutDir, lastSlash + 1, cutDirBufSize);
        }
        if (prevSlash < path)
            *(lastSlash + 1) = 0; // "/somedir" or "/somedir/" -> "/"
        else
            *lastSlash = 0; // "/firstdir/seconddir" or "/firstdir/seconddir/" -> "/firstdir"
        return TRUE;
    }

    case ftpsptOS2:
    {
        char* lastSlash = path + l - 1;
        while (--lastSlash >= path && *lastSlash != '/' && *lastSlash != '\\')
            ;
        char* prevSlash = lastSlash;
        while (--prevSlash >= path && *prevSlash != '/' && *prevSlash != '\\')
            ;
        if (lastSlash < path)
            return FALSE; // "somedir" or "C:/"
        if (cutDirBufSize > 0)
        {
            if (*(path + l - 1) == '/' || *(path + l - 1) == '\\')
                *(path + --l) = 0; // removal of trailing '/'
            lstrcpyn(cutDir, lastSlash + 1, cutDirBufSize);
        }
        if (prevSlash < path)
            *(lastSlash + 1) = 0; // "C:/somedir" or "C:/somedir/" -> "C:/"
        else
            *lastSlash = 0; // "C:/firstdir/seconddir" or "C:/firstdir/seconddir/" -> "C:/firstdir"
        return TRUE;
    }

    case ftpsptOpenVMS: // "PUB$DEVICE:[PUB.VMS]" or "[PUB.VMS]" or "[PUB.VMS]filename.txt;1" + root is "[000000]" and '^' is the escape character
    {
        char* s = path + l - 1;
        char* name = s;
        while (name > path && (*name != ']' || FTPIsVMSEscapeSequence(path, name)))
            name--;
        if (name < s && name > path) // path with a file name: e.g. "[PUB.VMS]filename.txt;1"
        {
            name++;
            if (cutDirBufSize > 0)
                lstrcpyn(cutDir, name, cutDirBufSize);
            *name = 0;
            return TRUE;
        }
        else
        {
            if (l > 1 && *s == ']' && !FTPIsVMSEscapeSequence(path, s))
            {
                if (fileNameCouldBeCut)
                    *fileNameCouldBeCut = FALSE; // we are cutting a directory name; a file looks different
                char* end = s;
                if (*(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1))
                    end = --s; // skip/remove the trailing '.' as well (when not escaped)
                while (--s >= path && (*s != '.' && *s != '[' || FTPIsVMSEscapeSequence(path, s)))
                    ;
                if (s >= path)
                {
                    if (*s == '.') // "[pub.vms]"
                    {
                        *end = 0; // we remove the original closing ']'
                        if (cutDirBufSize > 0)
                            lstrcpyn(cutDir, s + 1, cutDirBufSize);
                        *s++ = ']';
                        *s = 0;
                        return TRUE;
                    }
                    else // "[pub]" or "[000000]" (root)
                    {
                        if (strncmp(s + 1, "000000", 6) != 0 || *(s + 7) != '.' && *(s + 7) != ']') // not the root
                        {
                            *end = 0; // we remove the original closing ']'
                            if (cutDirBufSize > 0)
                                lstrcpyn(cutDir, s + 1, cutDirBufSize);
                            lstrcpyn(s + 1, "000000]", pathBufSize - (int)((s - path) + 1));
                            return TRUE;
                        }
                    }
                }
            }
        }
        return FALSE;
    }

    case ftpsptMVS: // "'VEA0016.MAIN.CLIST.'", "''" is the root
    {
        char* s = path + l - 1;
        if (l > 1 && *s == '\'')
        {
            char* end = s;
            if (*(s - 1) == '.')
                end = --s; // skip/remove the trailing '.' as well
            while (--s >= path && *s != '.' && *s != '\'')
                ;
            if (s >= path)
            {
                if (*s == '.' || // "'pub.mvs'" or "'pub.mvs.'"
                    s + 1 < end) // not the root - "'pub'" or "'pub.'"
                {
                    *end = 0; // we remove the original closing '\''
                    if (cutDirBufSize > 0)
                        lstrcpyn(cutDir, s + 1, cutDirBufSize);
                    if (*s == '\'')
                        s++; // "'pub'" -> we must keep the first '\''
                    *s++ = '\'';
                    *s = 0;
                    return TRUE;
                }
            }
        }
        return FALSE;
    }

    case ftpsptTandem: // \\SYSTEM.$VVVVV.SUBVOLUM.FILENAME, \\SYSTEM is the root
    {
        char* lastDot = path + l - 1;
        while (--lastDot >= path && *lastDot != '.')
            ;
        if (lastDot < path)
            return FALSE; // "\SYSTEM" or "\SYSTEM."
        if (cutDirBufSize > 0)
        {
            if (*(path + l - 1) == '.')
                *(path + --l) = 0; // removal of trailing '.'
            lstrcpyn(cutDir, lastDot + 1, cutDirBufSize);
        }
        *lastDot = 0;
        return TRUE;
    }

    case ftpsptIBMz_VM:
    {
        char* lastPeriod = path + l;
        while (--lastPeriod >= path && *lastPeriod != '.')
            ;
        char* prevPeriod = lastPeriod;
        while (--prevPeriod >= path && *prevPeriod != '.')
            ;
        BOOL willBeRoot = FALSE;
        if (prevPeriod < path)
        {
            if (lastPeriod < path || lastPeriod + 1 == path + l)
                return FALSE; // invalid path or root (period only at the end)
            willBeRoot = TRUE;
        }
        if (*(path + l - 1) == '.')
        {
            *(path + --l) = 0; // removal of trailing '.'
            lastPeriod = prevPeriod;
            while (--lastPeriod >= path && *lastPeriod != '.')
                ;
            if (lastPeriod < path)
                willBeRoot = TRUE;
        }
        else
            prevPeriod = lastPeriod;
        if (cutDirBufSize > 0)
            lstrcpyn(cutDir, prevPeriod + 1, cutDirBufSize);
        *(prevPeriod + (willBeRoot ? 1 : 0)) = 0;
        return TRUE;
    }
    }
    TRACE_E("Unknown path type in FTPCutDirectory()");
    return FALSE;
}

BOOL FTPPathAppend(CFTPServerPathType type, char* path, int pathSize, const char* name, BOOL isDir)
{
    CALL_STACK_MESSAGE6("FTPPathAppend(%d, %s, %d, %s, %d)", (int)type, path, pathSize, name, isDir);
    if (path == NULL || name == NULL)
    {
        TRACE_E("Unexpected situation in FTPPathAppend()");
        return FALSE;
    }

    int l = (int)strlen(path);
    BOOL empty = l == 0;
    switch (type)
    {
    case ftpsptOpenVMS:
    {
        if (l > 1 && path[l - 1] == ']' && !FTPIsVMSEscapeSequence(path, path + (l - 1))) // must be a VMS path ("[dir1.dir2]"), otherwise there is nothing to do
        {
            char* s = path + l - 1;
            if (*(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1))
                s--; // unescaped '.'
            char* root = NULL;
            if (isDir && s - path >= 7 && strncmp(s - 7, "[000000", 7) == 0 &&
                !FTPIsVMSEscapeSequence(path, s - 7))
            {
                root = s - 6;
            }
            if (*name != 0)
            {
                int n = (int)strlen(name);
                if (root == NULL && (s - path) + (isDir ? 2 : 1) + n < pathSize ||
                    root != NULL && (root - path) + 1 + n < pathSize)
                { // do we fit with '.' (directory only + not in the root), ']' and the terminating zero?
                    if (isDir)
                    {
                        if (root == NULL)
                            *s++ = '.';
                        else
                            s = root;
                        memmove(s, name, n);
                        s += n;
                        *s++ = ']';
                        *s = 0;
                    }
                    else
                    {
                        *s++ = ']';
                        memmove(s, name, n);
                        s += n;
                        *s = 0;
                    }
                    return TRUE;
                }
            }
            else
            {
                *s++ = ']';
                *s = 0;
                return TRUE;
            }
        }
        return FALSE;
    }

    case ftpsptMVS:
    {
        if (l > 1 && path[l - 1] == '\'') // must be an MVS path (e.g. "'dir1.dir2.'"), otherwise there is nothing to do
        {
            char* s = path + l - 1;
            if (*(s - 1) == '.')
                s--;
            BOOL root = (s - 1 >= path && *(s - 1) == '\'');
            if (*name != 0)
            {
                int n = (int)strlen(name);
                if ((s - path) + (root ? 1 : 2) + n < pathSize)
                { // do we fit with '.' (except for the root), '\'' and the terminating zero?
                    if (!root)
                        *s++ = '.';
                    memmove(s, name, n);
                    s += n;
                    *s++ = '\'';
                    *s = 0;
                    return TRUE;
                }
            }
            else
            {
                *s++ = '\'';
                *s = 0;
                return TRUE;
            }
        }
        return FALSE;
    }

    case ftpsptIBMz_VM:
    case ftpsptTandem:
    {
        if (l > 0) // the path cannot be empty (at least the root must be present); otherwise there is nothing to do
        {
            if (*name != 0)
            {
                BOOL addPeriod = path[l - 1] != '.';
                int n = (int)strlen(name);
                if (l + (addPeriod ? 1 : 0) + n < pathSize)
                { // do we fit with '.' (if needed), the name, and the terminating zero?
                    if (addPeriod)
                        path[l++] = '.';
                    memmove(path + l, name, n + 1);
                    return TRUE;
                }
            }
            else
                return TRUE;
        }
        return FALSE;
    }

    default:
    {
        char slash = '/';
        if (type == ftpsptNetware || type == ftpsptWindows || type == ftpsptOS2) // novell + windows + OS/2
        {
            if (l > 0 && (path[l - 1] == '/' || path[l - 1] == '\\'))
            {
                slash = path[l - 1];
                l--;
            }
        }
        else // ftpsptUnix + others
        {
            if (l > 0 && path[l - 1] == '/')
                l--;
        }
        if (*name != 0)
        {
            int n = (int)strlen(name);
            if (l + 1 + n < pathSize) // do we fit even with the terminating zero?
            {
                if (!empty)
                    path[l] = slash;
                else
                    l = -1;
                memmove(path + l + 1, name, n + 1);
            }
            else
                return FALSE;
        }
        else
        {
            if (l > (type == ftpsptOS2 ? 2 : 0))
                path[l] = 0; // except for "/" + "", it must result in "/" (and likewise "C:/" + "" -> "C:/")
        }
        return TRUE;
    }
    }
}

BOOL FTPIsValidAndNotRootPath(CFTPServerPathType type, const char* path)
{
    CALL_STACK_MESSAGE3("FTPIsValidAndNotRootPath(%d, %s)", (int)type, path);
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in FTPIsValidAndNotRootPath()");
        return FALSE;
    }

    int l = (int)strlen(path);
    switch (type)
    {
    case ftpsptOpenVMS: // valid path = at least two characters + ends with ']'
    {
        if (l > 1 && path[l - 1] == ']' && !FTPIsVMSEscapeSequence(path, path + (l - 1))) // must be a VMS path ("[dir1.dir2]"), otherwise there is nothing to do
        {
            const char* s = path + l - 1;
            if (*(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1))
                s--;
            return s - path < 7 || strncmp(s - 7, "[000000", 7) != 0 || FTPIsVMSEscapeSequence(path, s - 7);
        }
        return FALSE;
    }

    case ftpsptMVS: // valid path = at least two characters + ends with '\''
    {
        if (l > 1 && path[l - 1] == '\'') // must be an MVS path (e.g. "'dir1.dir2.'"), otherwise there is nothing to do
        {
            const char* s = path + l - 1;
            if (*(s - 1) == '.')
                s--;
            return s - 1 < path || *(s - 1) != '\'';
        }
        return FALSE;
    }

    case ftpsptIBMz_VM:
    {
        if (l > 0) // valid path = non-empty
        {
            const char* s = strchr(path, '.');
            return s == NULL || s != path + l - 1; // root ends with '.' and contains only one period
        }
        return FALSE;
    }

    case ftpsptTandem:
    {
        if (l > 0) // valid path = non-empty
        {
            const char* s = strchr(path, '.');
            return s != NULL && s != path + l - 1; // root contains no period or has a single period at the end
        }
        return FALSE;
    }

    case ftpsptOS2:
    {
        return l > 0 &&                                                                  // valid path = non-empty
               (l > 3 || path[1] != ':' || l == 3 && path[2] != '\\' && path[2] != '/'); // "C:" and "C:/" are considered the root
    }

    default:
    {
        return l > 0 && // valid path = non-empty
               (l != 1 ||
                (path[0] != '/' && (type != ftpsptNetware && type != ftpsptWindows || path[0] != '\\')));
    }
    }
}

const char* FTPFindEndOfUserNameOrHostInURL(const char* url)
{
    // path format: "user:password@host:port/path"
    // while parsing from the right it can handle:
    //  ftp://ms-domain\name@localhost:22/pub/a
    //  ftp://test.name@nas.server.cz@localhost:22/pub/test@bla

    // try to parse the URL from the right starting at the path separator ('/') or from the end of the URL
    const char* p = strchr(url, '/');
    if (p == NULL)
        p = url + strlen(url);
    const char* hostEnd = p;
    while (--p >= url && *p != '@' && *p != ':' && *p != '\\')
        ;
    if (p < url)
        return hostEnd; // it is only the server address
    if (*p != '\\')     // '\\' does not belong here, stop parsing from the right
    {
        BOOL skip = FALSE;
        if (*p == ':') // the URL contains the port number separator
        {
            hostEnd = p;
            while (--p >= url && *p != '@' && *p != ':' && *p != '\\')
                ;
            if (p < url)
                return hostEnd; // it is only the server address and port
            if (*p == '\\' || *p == ':')
                skip = TRUE; // neither '\\' nor ':' belongs here, stop parsing from the right
        }
        if (!skip) // only if we are not finishing the right-to-left parsing
        {          // *p is '@'; determine whether it ends the password or the user name
            const char* userEnd = p;
            BOOL invalidCharInPasswd = FALSE;
            while (1)
            {
                while (--p >= url && *p != '@' && *p != ':' && *p != '\\')
                    ;
                if (p < url)
                    return userEnd; // user name without a password
                if (*p == '@' || *p == '\\')
                    invalidCharInPasswd = TRUE;
                else
                {
                    if (invalidCharInPasswd)
                        break; // the password contains '@' or '\\'; stop parsing from the right

                    // *p is ':'; determine whether the user name contains the forbidden ':' character
                    userEnd = p;
                    while (--p >= url && *p != ':')
                        ;
                    if (p < url)
                        return userEnd; // user name without a password
                    break;              // the user name contains ':'; stop parsing from the right
                }
            }
        }
    }

    // parsing the URL from the left (treat '\\' as a path separator as well)
    p = url;
    while (*p != 0 && *p != '@' && *p != ':' && *p != '/' && *p != '\\')
        p++;
    return p;
}

void FTPSplitPath(char* p, char** user, char** password, char** host, char** port, char** path,
                  char* firstCharOfPath, int userLength)
{
    // path format: "//user:password@host:port/path" or just "user:password@host:port/path"
    if (user != NULL)
        *user = NULL;
    if (password != NULL)
        *password = NULL;
    if (host != NULL)
        *host = NULL;
    if (port != NULL)
        *port = NULL;
    if (path != NULL)
        *path = NULL;

    if (*p == '/' && *(p + 1) == '/')
        p += 2; // skip an optional "//"
    char* beg = p;
    if (userLength > 0 && (int)strlen(p) > userLength &&
        (p[userLength] == '@' || p[userLength] == ':' && strchr(p + userLength + 1, '@') != NULL))
    { // parse the username according to its expected length (introduced because the username may contain '@', '/' and '\\')
        p += userLength;
    }
    else
    {
        p = (char*)FTPFindEndOfUserNameOrHostInURL(p);
    }
    if (*p == '@' || *p == ':') // user
    {
        BOOL passwd = *p == ':';
        char* passEnd = p + 1;
        if (passwd) // only ':' - we must try to find '@' (if it is absent, the ':' comes from "host:port")
        {
            while (*passEnd != 0 && *passEnd != '@' && *passEnd != ':' &&
                   *passEnd != '/' && *passEnd != '\\')
                passEnd++;
        }
        if (!passwd || *passEnd == '@')
        {
            if (user != NULL)
            {
                while (*beg <= ' ')
                    beg++; // skip spaces at beginning; there must be at least one '@' or ':'
                *user = beg;
                char* e = p - 1;
                while (e >= beg && *e <= ' ')
                    *e-- = 0; // clip spaces at end
            }
            *p++ = 0;
            if (passwd) // next is password
            {
                beg = p;
                p = passEnd;
                if (password != NULL)
                    *password = beg; // let password as is (do not skip spaces)
                *p++ = 0;
            }
            // next is host
            beg = p;
            while (*p != 0 && *p != ':' && *p != '/' && *p != '\\')
                p++; // find end of host
        }
    }
    if (host != NULL) // host
    {
        while (*beg != 0 && *beg <= ' ')
            beg++; // skip spaces at beginning
        *host = beg;
        char* e = p - 1;
        while (e >= beg && *e <= ' ')
            *e-- = 0; // clip spaces at end
    }
    if (*p == ':')
    {
        *p++ = 0;
        beg = p;
        while (*p != 0 && *p != '/' && *p != '\\')
            p++;          // find end of port
        if (port != NULL) // port
        {
            while (*beg != 0 && *beg <= ' ')
                beg++; // skip spaces at beginning
            *port = beg;
            char* e = p - 1;
            while (e >= beg && *e <= ' ')
                *e-- = 0; // clip spaces at end
        }
    }
    if (*p == '/' || *p == '\\') // does it have path?
    {
        if (firstCharOfPath != NULL)
            *firstCharOfPath = *p; // return whether it was '/' or '\\'
        *p++ = 0;
        if (path != NULL)
            *path = p; // path
    }
}

int FTPGetUserLength(const char* user)
{
    if (user == NULL)
        return 0;
    const char* s = user;
    while (*s != 0 && *s != '/' && *s != '\\' && *s != ':' && *s != '@')
        s++;
    if (*s == 0)
        return 0; // problem-free name (including anonymous user)
    while (*s != 0)
        s++;
    return (int)(s - user);
}

const char* FTPFindPath(const char* path, int userLength)
{
    // path format: "//user:password@host:port/path" or just "user:password@host:port/path"
    const char* p = path;
    if (*p == '/' && *(p + 1) == '/')
        p += 2; // skip an optional "//"
    if (userLength > 0 && (int)strlen(p) > userLength &&
        (p[userLength] == '@' || p[userLength] == ':' && strchr(p + userLength + 1, '@') != NULL))
    { // parse the username according to its expected length (introduced because the username may contain '@', '/' and '\\')
        p += userLength;
    }
    else
    {
        p = FTPFindEndOfUserNameOrHostInURL(p);
    }
    if (*p == '@' || *p == ':') // user + password
    {
        BOOL passwd = *p == ':';
        p++;
        if (passwd) // next is password
        {
            while (*p != 0 && *p != '@' && *p != ':' && *p != '/' && *p != '\\')
                p++; // find end of password
            p++;
        }
        // next is host
        while (*p != 0 && *p != ':' && *p != '/' && *p != '\\')
            p++; // find end of host
    }
    if (*p == ':')
    {
        p++;
        while (*p != 0 && *p != '/' && *p != '\\')
            p++; // find end of port
    }
    return p;
}

const char* FTPGetLocalPath(const char* path, CFTPServerPathType type)
{
    switch (type)
    {
    case ftpsptIBMz_VM:
    case ftpsptOpenVMS:
    case ftpsptOS2:
    case ftpsptMVS:
        return path + ((*path == '\\' || *path == '/') ? 1 : 0);

    //case ftpsptTandem:
    //case ftpsptNetware:
    //case ftpsptWindows:
    //case ftpsptEmpty:
    //case ftpsptUnknown:
    //case ftpsptUnix:
    //case ftpsptAS400:
    default:
        return path;
    }
}

BOOL FTPIsTheSameServerPath(CFTPServerPathType type, const char* p1, const char* p2)
{
    return FTPIsPrefixOfServerPath(type, p1, p2, TRUE);
}

BOOL FTPIsPrefixOfServerPath(CFTPServerPathType type, const char* prefix, const char* path,
                             BOOL mustBeSame)
{
    switch (type)
    {
    case ftpsptOpenVMS: // case-insensitive + VMS path
    {
        int l1 = (int)strlen(prefix);
        int l2 = (int)strlen(path);
        if (l1 > 1 && prefix[l1 - 1] == ']' && !FTPIsVMSEscapeSequence(prefix, prefix + (l1 - 1)))
        {
            l1--;
            if (prefix[l1 - 1] == '.' && !FTPIsVMSEscapeSequence(prefix, prefix + (l1 - 1)))
                l1--;
            if (l1 >= 7 && strncmp(prefix + l1 - 7, "[000000", 7) == 0 &&
                !FTPIsVMSEscapeSequence(prefix, prefix + (l1 - 7)))
            {
                l1 -= 6;
            }
        }
        if (l2 > 1 && path[l2 - 1] == ']' && !FTPIsVMSEscapeSequence(path, path + (l2 - 1)))
        {
            l2--;
            if (path[l2 - 1] == '.' && !FTPIsVMSEscapeSequence(path, path + (l2 - 1)))
                l2--;
            if (l2 >= 7 && strncmp(path + l2 - 7, "[000000", 7) == 0 &&
                !FTPIsVMSEscapeSequence(path, path + (l2 - 7)))
            {
                l2 -= 6;
            }
        }
        return (l1 == l2 || !mustBeSame && l1 < l2) &&
               SalamanderGeneral->StrNICmp(prefix, path, l1) == 0 &&
               (l1 == l2 ||
                prefix[l1 - 1] == '[' && !FTPIsVMSEscapeSequence(prefix, prefix + (l1 - 1)) ||
                (path[l1] == '.' || path[l1] == ']') && !FTPIsVMSEscapeSequence(path, path + l1));
    }

    case ftpsptMVS: // case-insensitive + MVS path
    {
        int l1 = (int)strlen(prefix);
        int l2 = (int)strlen(path);
        if (l1 > 1 && prefix[l1 - 1] == '\'')
        {
            l1--;
            if (prefix[l1 - 1] == '.')
                l1--;
        }
        if (l2 > 1 && path[l2 - 1] == '\'')
        {
            l2--;
            if (path[l2 - 1] == '.')
                l2--;
        }
        return (l1 == l2 || !mustBeSame && l1 < l2) && SalamanderGeneral->StrNICmp(prefix, path, l1) == 0 &&
               (l1 == l2 || path[l1] == '.' || path[l1] == '\'');
    }

    case ftpsptNetware:
    case ftpsptWindows:
    case ftpsptOS2: // case-insensitive + '/' is equivalent to '\\'
    {
        const char* s1 = prefix;
        const char* s2 = path;
        while (*s1 != 0 &&
               ((*s1 == '/' || *s1 == '\\') && (*s2 == '/' || *s2 == '\\') ||
                LowerCase[*s1] == LowerCase[*s2]))
        {
            s1++;
            s2++;
        }
        if (*s1 == '/' || *s1 == '\\')
            s1++;
        if (*s2 == '/' || *s2 == '\\')
            s2++;
        return *s1 == 0 && *s2 == 0 ||
               !mustBeSame && *s1 == 0 && s2 > path && (*(s2 - 1) == '/' || *(s2 - 1) == '\\');
    }

    case ftpsptIBMz_VM: // case-insensitive + IBM_z/VM path
    case ftpsptTandem:  // case-insensitive + Tandem path
    {
        int l1 = (int)strlen(prefix);
        int l2 = (int)strlen(path);
        if (l1 > 1 && prefix[l1 - 1] == '.')
            l1--;
        if (l2 > 1 && path[l2 - 1] == '.')
            l2--;
        return (l1 == l2 || !mustBeSame && l1 < l2) && SalamanderGeneral->StrNICmp(prefix, path, l1) == 0 &&
               (l1 == l2 || path[l1] == '.');
    }

    case ftpsptAS400: // case-insensitive
    {
        const char* s1 = prefix;
        const char* s2 = path;
        while (*s1 != 0 && LowerCase[*s1] == LowerCase[*s2])
        {
            s1++;
            s2++;
        }
        if (*s1 == '/')
            s1++;
        if (*s2 == '/')
            s2++;
        return *s1 == 0 && *s2 == 0 ||
               !mustBeSame && *s1 == 0 && s2 > path && *(s2 - 1) == '/';
    }

    default: // unix + others
    {
        int l1 = (int)strlen(prefix);
        int l2 = (int)strlen(path);
        if (l1 > 0 && prefix[l1 - 1] == '/')
            l1--;
        if (l2 > 0 && path[l2 - 1] == '/')
            l2--;
        return (l1 == l2 || !mustBeSame && l1 < l2) && strncmp(prefix, path, l1) == 0 &&
               (l1 == l2 || path[l1] == '/');
    }
    }
}

BOOL FTPIsTheSamePath(CFTPServerPathType type, const char* p1, const char* p2,
                      BOOL sameIfPath2IsRelative, int userLength)
{
    // path format: "//user:password@host:port/path"
    if (*p1 == '/' && *(p1 + 1) == '/' && *p2 == '/' && *(p2 + 1) == '/') // this must be the user part of the path
    {
        p1 += 2;
        p2 += 2;
        int l1 = (int)strlen(p1);
        int l2 = (int)strlen(p2);
        if (l1 < FTP_USERPART_SIZE && l2 < FTP_USERPART_SIZE)
        {
            char buf1[FTP_USERPART_SIZE];
            char buf2[FTP_USERPART_SIZE];
            strcpy(buf1, p1);
            strcpy(buf2, p2);
            char *user1, *host1, *port1, *path1, *passwd1;
            char *user2, *host2, *port2, *path2, *passwd2;
            FTPSplitPath(buf1, &user1, &passwd1, &host1, &port1, &path1, NULL, userLength);
            FTPSplitPath(buf2, &user2, &passwd2, &host2, &port2, &path2, NULL, userLength);
            if (passwd1 != NULL)
                memset(passwd1, 0, strlen(passwd1)); // zero out the memory with the password
            if (passwd2 != NULL)
                memset(passwd2, 0, strlen(passwd2)); // zero out the memory with the password
            if (user1 == NULL && user2 == NULL ||
                user1 != NULL && user2 != NULL && strcmp(user1, user2) == 0 ||
                user1 == NULL && user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0 ||
                user2 == NULL && user1 != NULL && strcmp(user1, FTP_ANONYMOUS) == 0)
            { // match user names (case-sensitive - UNIX accounts)
                if (host1 != NULL && host2 != NULL &&
                    SalamanderGeneral->StrICmp(host1, host2) == 0)
                {                                // match host names (case-insensitive - Internet conventions - perhaps better to test IP addresses later)
                    const char* ftp_port = "21"; // standard FTP port
                    if (port1 == NULL && port2 == NULL ||
                        port1 != NULL && port2 != NULL && strcmp(port1, port2) == 0 ||
                        port1 == NULL && port2 != NULL && strcmp(port2, ftp_port) == 0 ||
                        port2 == NULL && port1 != NULL && strcmp(port1, ftp_port) == 0)
                    {                                       // matching port (case-sensitive; it should be just a number, so it hardly matters)
                        if (path1 != NULL && path2 != NULL) // paths without a leading slash
                        {
                            return FTPIsTheSameServerPath(type, path1, path2);
                        }
                        else // at least one of the paths is missing
                        {
                            if (path1 == NULL && path2 == NULL ||
                                path1 == NULL && path2 != NULL && *path2 == 0 ||
                                path2 == NULL && path1 != NULL && *path1 == 0)
                                return TRUE; // two root paths (with/without a slash)
                            else
                            {
                                if (sameIfPath2IsRelative && path2 == NULL)
                                { // 'p2' is relative + otherwise matches 'p1' (handles
                                    // "ftp://petr@localhost/path" == "ftp://petr@localhost" - needed for
                                    // Change Directory command)
                                    return TRUE;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
            TRACE_E("Too large paths in FTPIsTheSamePath!");
    }
    return FALSE;
}

BOOL FTPHasTheSameRootPath(const char* p1, const char* p2, int userLength)
{
    // root path format: "//user:password@host:port"
    if (*p1 == '/' && *(p1 + 1) == '/' && *p2 == '/' && *(p2 + 1) == '/') // this must be the user part of the path
    {
        p1 += 2;
        p2 += 2;
        int l1 = (int)strlen(p1);
        int l2 = (int)strlen(p2);
        if (l1 < FTP_USERPART_SIZE && l2 < FTP_USERPART_SIZE)
        {
            char buf1[FTP_USERPART_SIZE];
            char buf2[FTP_USERPART_SIZE];
            strcpy(buf1, p1);
            strcpy(buf2, p2);
            char *user1, *host1, *port1, *passwd1;
            char *user2, *host2, *port2, *passwd2;
            FTPSplitPath(buf1, &user1, &passwd1, &host1, &port1, NULL, NULL, userLength);
            FTPSplitPath(buf2, &user2, &passwd2, &host2, &port2, NULL, NULL, userLength);
            if (passwd1 != NULL)
                memset(passwd1, 0, strlen(passwd1)); // zero out the memory with the password
            if (passwd2 != NULL)
                memset(passwd2, 0, strlen(passwd2)); // zero out the memory with the password
            if (user1 == NULL && user2 == NULL ||
                user1 != NULL && user2 != NULL && strcmp(user1, user2) == 0 ||
                user1 == NULL && user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0 ||
                user2 == NULL && user1 != NULL && strcmp(user1, FTP_ANONYMOUS) == 0)
            { // match user names (case-sensitive - UNIX accounts)
                if (host1 != NULL && host2 != NULL &&
                    SalamanderGeneral->StrICmp(host1, host2) == 0)
                {                                // match host names (case-insensitive - Internet conventions - perhaps better to test IP addresses later)
                    const char* ftp_port = "21"; // standard FTP port
                    if (port1 == NULL && port2 == NULL ||
                        port1 != NULL && port2 != NULL && strcmp(port1, port2) == 0 ||
                        port1 == NULL && port2 != NULL && strcmp(port2, ftp_port) == 0 ||
                        port2 == NULL && port1 != NULL && strcmp(port1, ftp_port) == 0)
                    {                // matching port (case-sensitive; it should be just a number, so it hardly matters)
                        return TRUE; // roots match
                    }
                }
            }
        }
        else
            TRACE_E("Too large paths in FTPHasTheSameRootPath!");
    }
    return FALSE;
}

char* FTPGetErrorText(int err, char* buf, int bufSize)
{
    int l = 0;
    if (bufSize > 20)
        l = sprintf(buf, "(%d) ", err);
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      buf + l,
                      bufSize - l,
                      NULL) == 0 ||
        bufSize > l && *(buf + l) == 0)
    {
        char txt[100];
        sprintf(txt, "System error %d, text description is not available.", err);
        lstrcpyn(buf, txt, bufSize);
    }
    return buf;
}

// determine whether the text 'text' contains the string 'sub' (letter case does not matter)
BOOL HaveSubstring(const char* text, const char* sub)
{
    const char* t = text;
    while (*t != 0)
    {
        if (LowerCase[*t] == LowerCase[*sub])
        {
            const char* s = sub + 1;
            const char* tt = t + 1;
            while (*s != 0 && LowerCase[*tt] == LowerCase[*s])
            {
                tt++;
                s++;
            }
            if (*s == 0)
                return TRUE; // found
        }
        t++;
    }
    return *sub == 0; // not found (exception: empty text and empty substring)
}

const char* KnownOSNames[] = {"UNIX", "Windows", "NETWARE", "TANDEM", "OS/2", "VMS", "MVS", "VM", "OS/400", NULL};

BOOL IsKnownOSName(const char* sysBeg, const char* sysEnd)
{
    const char** os = KnownOSNames;
    while (*os != NULL)
    {
        if (strlen(*os) == (DWORD)(sysEnd - sysBeg) &&
            SalamanderGeneral->StrNICmp(*os, sysBeg, (int)(sysEnd - sysBeg)) == 0)
            return TRUE;
        os++;
    }
    return FALSE;
}

void FTPGetServerSystem(const char* serverSystem, char* sysName)
{
    sysName[0] = 0;
    if (serverSystem != NULL)
    {
        int replyLen = (int)strlen(serverSystem);
        if (*serverSystem == '2' && replyLen > 4) // FTP_D1_SUCCESS + there is a chance of the system name string
        {
            const char* sys;
            if (serverSystem[3] == ' ')
                sys = serverSystem + 4; // single-line response
            else                        // multi-line reply, we must find the last line
            {
                sys = serverSystem + replyLen;
                if (sys > serverSystem && *(sys - 1) == '\n')
                    sys--;
                if (sys > serverSystem && *(sys - 1) == '\r')
                    sys--;
                while (sys > serverSystem && *(sys - 1) != '\r' && *(sys - 1) != '\n')
                    sys--;
                sys += 4;
                if (sys >= serverSystem + replyLen)
                {
                    TRACE_E("Unexpected format of SYST reply: " << serverSystem);
                    sys = serverSystem + 4; // unexpected
                }
            }
            while (*sys != 0 && *sys <= ' ')
                sys++;
            const char* sysBeg = sys;
            while (*sys != 0 && *sys > ' ')
                sys++;
            if (!IsKnownOSName(sysBeg, sys))
            {
                const char* nextSys = sys;
                while (*nextSys != 0)
                {
                    while (*nextSys != 0 && *nextSys <= ' ')
                        nextSys++; // skip white-spaces
                    const char* nextSysBeg = nextSys;
                    while (*nextSys != 0 && *nextSys > ' ')
                        nextSys++;
                    if (IsKnownOSName(nextSysBeg, nextSys)) // we found the OS name only in one of the following words (translation error, e.g. "215 Betriebssystem OS/2")
                    {
                        sysBeg = nextSysBeg;
                        sys = nextSys;
                        break;
                    }
                }
            }

            int len = (int)(sys - sysBeg);
            if (len > 200)
                len = 200; // truncate to 200 characters
            memcpy(sysName, sysBeg, len);
            sysName[len] = 0;
        }
    }
}

CFTPServerPathType GetFTPServerPathType(const char* serverFirstReply, const char* serverSystem, const char* path)
{
    const char* s = path;
    int slash = 0;
    int slashAtBeg = 0;
    int backslash = 0;
    int backslashAtBeg = 0;
    int apostroph = 0;
    int apostrophAtBeg = 0;
    int apostrophAtEnd = 0;
    int bracket = 0;
    int vmsEscapedBracket = 0;
    int openBracket = 0;
    int vmsEscapedOpenBracket = 0;
    int closeBracket = 0;
    int vmsEscapedCloseBracket = 0;
    int colon = 0;
    int periodsAfterColon = 0;
    int periodsBeforeColon = 0;
    int spaces = 0;
    BOOL colonOnSecondPos = FALSE;
    BOOL charOnFirstPos = *s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z';
    BOOL vmsEscape = FALSE;

    while (*s != 0)
    {
        if (*s == '^')
            vmsEscape = !vmsEscape;
        else
        {
            switch (*s)
            {
            case '/':
            {
                if (s == path)
                    slashAtBeg++;
                else
                    slash++;
                break;
            }

            case '\\':
            {
                if (s == path)
                    backslashAtBeg++;
                else
                    backslash++;
                break;
            }

            case '\'':
            {
                if (s == path)
                    apostrophAtBeg++;
                else
                {
                    if (*(s + 1) == 0)
                        apostrophAtEnd++;
                    else
                        apostroph++;
                }
                break;
            }

            case '[':
            {
                if (s == path || *(s - 1) == ':')
                {
                    openBracket++; // consider only at the beginning of the path or before ':'
                    if (vmsEscape)
                        vmsEscapedOpenBracket++;
                }
                else
                {
                    bracket++;
                    if (vmsEscape)
                        vmsEscapedBracket++;
                }
                break;
            }

            case ']':
            {
                const char* name = s + 1; // try to skip the file name (e.g. "DKA0:[MYDIR.SUBDIR]MYFILE.TXT;1")
                BOOL vmsEsc = FALSE;
                while (*name != 0 && *name != '/' && *name != '\\')
                {
                    if (*name == '^')
                        vmsEsc = !vmsEsc;
                    else
                    {
                        if (vmsEsc)
                            vmsEsc = FALSE;
                        else
                        {
                            if (*name == '[' || *name == ']')
                                break;
                        }
                    }
                    name++;
                }
                if (*name == 0)
                {
                    closeBracket++; // we only take ']' at the end of the path
                    if (vmsEscape)
                        vmsEscapedCloseBracket++;
                }
                else
                {
                    bracket++;
                    if (vmsEscape)
                        vmsEscapedBracket++;
                }
                break;
            }

            case ':':
            {
                colon++;
                if (s - path == 1)
                    colonOnSecondPos = TRUE;
                break;
            }

            case '.':
            {
                if (colon > 0)
                    periodsAfterColon++;
                else
                    periodsBeforeColon++;
                break;
            }

            case ' ':
            {
                spaces++;
                break;
            }
            }
            vmsEscape = FALSE;
        }
        s++;
    }
    int pathLen = (int)(s - path);

    char sysName[201];
    FTPGetServerSystem(serverSystem, sysName);

    if (slashAtBeg)
    {
        if (HaveSubstring(sysName, "Windows")) // known system name + it is Windows (so far we know only about NT)
            return ftpsptWindows;
        else
        {
            if (HaveSubstring(sysName, "NETWARE") || // known system name + it is NetWare
                serverFirstReply != NULL && HaveSubstring(serverFirstReply, " NW 3") &&
                    HaveSubstring(serverFirstReply, " HellSoft")) // known first server response and it is a Hellsoft server on NetWare
            {
                return ftpsptNetware;
            }
            else
            {
                if (HaveSubstring(sysName, "OS/400"))
                    return ftpsptAS400; // if they happened to return a path that already starts with a slash as the first path (so far the logs show e.g. "QGPL" ())
                else
                    return ftpsptUnix; // UNIX is more likely than NetWare
            }
        }
    }
    if (backslashAtBeg)
    {
        if (HaveSubstring(sysName, "NETWARE") || // known system name + it is NetWare
            serverFirstReply != NULL && HaveSubstring(serverFirstReply, " NW 3") &&
                HaveSubstring(serverFirstReply, " HellSoft")) // known first server response and it is a Hellsoft server on NetWare
        {
            return ftpsptNetware;
        }
        else
        {
            if (slash == 0 && backslash == 0 &&
                serverFirstReply != NULL && HaveSubstring(serverFirstReply, " TANDEM ") &&
                (sysName[0] == 0 || HaveSubstring(sysName, "TANDEM")))
            {
                return ftpsptTandem;
            }
            else
                return ftpsptWindows; // Windows are more likely than NetWare
        }
    }
    if (charOnFirstPos && colonOnSecondPos && colon == 1 && // paths of the form "C:"
        (pathLen == 2 || slash > 0 || backslash > 0))       // paths of the form "C:/..." or "C\\..."
    {
        return ftpsptOS2;
    }
    if (openBracket - vmsEscapedOpenBracket == 1 &&
        closeBracket - vmsEscapedCloseBracket == 1 &&
        bracket - vmsEscapedBracket == 0 &&                   // only opening and closing brackets
        apostrophAtBeg + apostrophAtEnd == 0 &&               // no apostrophes at the beginning or end
        slashAtBeg + slash + backslashAtBeg + backslash == 0) // no slashes or backslashes
    {
        return ftpsptOpenVMS;
    }
    if (apostrophAtBeg && apostrophAtEnd && apostroph == 0) // path enclosed in apostrophes
    {
        return ftpsptMVS;
    }
    if (slash == 0 && backslash == 0 && colon == 1 && periodsAfterColon > 0 &&
        periodsBeforeColon == 0 && spaces == 0)
    {
        return ftpsptIBMz_VM;
    }

    if (path[0] == 0) // for empty paths we try detection by the system name,
    {                 // handles detection of the "Salamander" generic root ("ftp://server/" = root)
                      // whatever system "server" may be)
        if (HaveSubstring(sysName, "UNIX"))
            return ftpsptUnix;
        if (HaveSubstring(sysName, "Windows"))
            return ftpsptWindows;
        if (HaveSubstring(sysName, "NETWARE") ||
            serverFirstReply != NULL && HaveSubstring(serverFirstReply, " NW 3") &&
                HaveSubstring(serverFirstReply, " HellSoft")) // known first server response and it is a Hellsoft server on NetWare
        {
            return ftpsptNetware;
        }
        if (HaveSubstring(sysName, "OS/2"))
            return ftpsptOS2;
        if (HaveSubstring(sysName, "VMS"))
            return ftpsptOpenVMS;
        if (HaveSubstring(sysName, "MVS"))
            return ftpsptMVS;
        if (HaveSubstring(sysName, "VM"))
            return ftpsptIBMz_VM;
        if (serverFirstReply != NULL && HaveSubstring(serverFirstReply, " TANDEM ") &&
            (sysName[0] == 0 || HaveSubstring(sysName, "TANDEM")))
        {
            return ftpsptTandem;
        }
    }
    if (HaveSubstring(sysName, "OS/400"))
        return ftpsptAS400; // the first path that AS/400 returns is e.g. "QGPL" (reply to the first PWD: 257 "QGPL" is current library.)
    return ftpsptUnknown;
}

char FTPGetPathDelimiter(CFTPServerPathType pathType)
{
    switch (pathType)
    {
    case ftpsptTandem:
    case ftpsptIBMz_VM:
    case ftpsptOpenVMS:
    case ftpsptMVS:
        return '.';

    // ftpsptEmpty
    // ftpsptUnknown
    // ftpsptUnix
    // ftpsptNetware
    // ftpsptWindows
    // ftpsptOS2
    // ftpsptAS400
    default:
        return '/';
    }
}

BOOL IsHexChar(char c, int* val)
{
    if (c >= '0' && c <= '9')
        *val = c - '0';
    else if (c >= 'a' && c <= 'f')
        *val = 10 + (c - 'a');
    else if (c >= 'A' && c <= 'F')
        *val = 10 + (c - 'A');
    else
        return FALSE;
    return TRUE;
}

void FTPConvertHexEscapeSequences(char* txt)
{
    char* d = NULL;
    char* s = txt;
    int val1, val2;
    while (*s != 0)
    {
        if (*s == '%' && IsHexChar(*(s + 1), &val1) && IsHexChar(*(s + 2), &val2))
        {
            if (d == NULL)
                d = s;
            *d++ = (val1 << 4) + val2;
            s += 3;
        }
        else
        {
            if (d != NULL)
                *d++ = *s++;
            else
                s++;
        }
    }
    if (d != NULL)
        *d = 0;
}

BOOL FTPAddHexEscapeSequences(char* txt, int txtSize)
{
    char* s = txt;
    int txtLen = (int)strlen(txt);
    int val1, val2;
    while (*s != 0)
    {
        if (*s == '%' && IsHexChar(*(s + 1), &val1) && IsHexChar(*(s + 2), &val2))
        { // replace the "%" character with its hex escape sequence "%25"
            if (txtLen + 2 < txtSize)
            {
                memmove(s + 3, s + 1, txtLen - (s - txt));
                *(s + 1) = '2';
                *(s + 2) = '5';
                txtLen += 2;
                s += 3;
            }
            else
                return FALSE; // maly buffer
        }
        else
            s++;
    }
    return TRUE;
}

BOOL FTPGetIBMz_VMRootPath(char* root, int rootSize, const char* path)
{
    const char* s = path;
    while (*s != 0 && *s != ':')
        s++;
    if (*s != 0)
    {
        while (*s != 0 && *s != '.')
            s++;
        if (*s != 0)
        {
            s++;
            lstrcpyn(root, path, (int)min(s - path + 1, rootSize));
            return TRUE;
        }
    }
    if (rootSize > 0)
        root[0] = 0;
    return FALSE;
}

BOOL FTPGetOS2RootPath(char* root, int rootSize, const char* path)
{
    if (path[0] != 0 && path[1] == ':' && rootSize > 3)
    {
        root[0] = path[0];
        root[1] = ':';
        if (path[2] == '/' || path[2] == '\\')
            root[2] = path[2];
        else
            root[2] = '/';
        root[3] = 0;
        return TRUE;
    }
    if (rootSize > 0)
        root[0] = 0;
    return FALSE;
}

BOOL GetAttrsFromUNIXRights(DWORD* actAttr, DWORD* attrDiff, const char* rights)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("GetAttrsFromUNIXRights(, , %s)", rights);

    // CAUTION: if the format of UNIX permissions changes, IsUNIXLink() must be updated as well

    if (rights != NULL && strlen(rights) == 10) // must be ten characters, otherwise it cannot be UNIX permissions (permissions with ACL have eleven characters, e.g. "drwxrwxr-x+", and we cannot modify them, so we pretend we do not know them)
    {
        BOOL ok = TRUE;
        *actAttr = 0;
        // -rwxrwxrwx
        // 0th character is ignored (known letters: d,l,c,b,p)
        // 1./4./7.: r,-
        if (rights[1] == 'r')
            *actAttr |= 0400;
        else
            ok &= rights[1] == '-';
        if (rights[4] == 'r')
            *actAttr |= 0040;
        else
            ok &= rights[4] == '-';
        if (rights[7] == 'r')
            *actAttr |= 0004;
        else
            ok &= rights[7] == '-';
        // 2./5./8.: w,-
        if (rights[2] == 'w')
            *actAttr |= 0200;
        else
            ok &= rights[2] == '-';
        if (rights[5] == 'w')
            *actAttr |= 0020;
        else
            ok &= rights[5] == '-';
        if (rights[8] == 'w')
            *actAttr |= 0002;
        else
            ok &= rights[8] == '-';
        // 3rd/6th/9th: we distinguish only x and - and set attrDiff if it differs (known letters: s,S,X,t,T)
        if (rights[3] == 'x')
            *actAttr |= 0100;
        else if (rights[3] != '-')
            *attrDiff |= 0100;
        if (rights[6] == 'x')
            *actAttr |= 0010;
        else if (rights[6] != '-')
            *attrDiff |= 0010;
        if (rights[9] == 'x')
            *actAttr |= 0001;
        else if (rights[9] != '-')
            *attrDiff |= 0001;
        return ok;
    }
    return FALSE;
}

BOOL IsUNIXLink(const char* rights)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("IsUNIXLink(%s)", rights);

    // CAUTION: if the format of UNIX permissions changes, GetAttrsFromUNIXRights() must be updated as well

    int len = (rights != NULL ? (int)strlen(rights) : 0);
    return ((len == 10 || len == 11 && rights[10] == '+') && // must be ten characters, otherwise it cannot be UNIX permissions; exception: with ACL it is eleven characters, e.g. "drwxrwxr-x+"
            rights[0] == 'l' &&
            (rights[1] == 'r' || rights[1] == '-') &&
            (rights[2] == 'w' || rights[2] == '-') &&
            (rights[4] == 'r' || rights[4] == '-') &&
            (rights[5] == 'w' || rights[5] == '-') &&
            (rights[7] == 'r' || rights[7] == '-') &&
            (rights[8] == 'w' || rights[8] == '-'));
}

void GetUNIXRightsStr(char* buf, int bufSize, DWORD attrs)
{
    if (bufSize <= 0)
        return;
    char* end = buf + bufSize - 1;
    char* s = buf;
    if (s < end)
        *s++ = (attrs & 0400) ? 'r' : '-';
    if (s < end)
        *s++ = (attrs & 0200) ? 'w' : '-';
    if (s < end)
        *s++ = (attrs & 0100) ? 'x' : '-';
    if (s < end)
        *s++ = (attrs & 0040) ? 'r' : '-';
    if (s < end)
        *s++ = (attrs & 0020) ? 'w' : '-';
    if (s < end)
        *s++ = (attrs & 0010) ? 'x' : '-';
    if (s < end)
        *s++ = (attrs & 0004) ? 'r' : '-';
    if (s < end)
        *s++ = (attrs & 0002) ? 'w' : '-';
    if (s < end)
        *s++ = (attrs & 0001) ? 'x' : '-';
    *s = 0;
}

void FTPGetErrorTextForLog(DWORD err, char* errBuf, int bufSize)
{
    FTPGetErrorText(err, errBuf, bufSize - 2); // (bufSize-2) so there is room left for our CRLF
    char* s = errBuf + strlen(errBuf);
    while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
        s--;
    strcpy(s, "\r\n"); // append our CRLF to the end of the line with the error text
}

BOOL FTPReadFTPReply(char* readBytes, int readBytesCount, int readBytesOffset,
                     char** reply, int* replySize, int* replyCode)
{
    BOOL ret = FALSE;
    if (readBytesOffset < readBytesCount) // if anything is loaded at all
    {
        char* s = readBytes + readBytesOffset;
        char* end = readBytes + readBytesCount;
        int ftpRplCode = 0; // FTP reply code, -1 = error (not an FTP reply)
        int i;
        for (i = 0; s < end && i < 3; i++)
        {
            if (*s >= '0' && *s <= '9')
            {
                ftpRplCode = 10 * ftpRplCode + (*s - '0');
                s++;
            }
            else
            {
                ftpRplCode = -1;
                break;
            }
        }
        BOOL syntaxOK = FALSE;
        if (ftpRplCode == -1)
            TRACE_E("Unexpected syntax of FTP reply (not a three digit number).");
        else
        {
            if (s < end)
            {
                if (*s == '-') // multi-line, skip everything up to the space after the reply code on the last line
                {
                    s++;
                    while (s < end)
                    {
                        do // find the end of the line (CRLF and, contrary to the specification, also LF)
                        {
                            if (*s == '\r' && s + 1 < end && *(s + 1) == '\n')
                            {
                                s++;
                                break; // CRLF
                            }
                            else
                            {
                                if (*s == '\n')
                                    break; // LF
                            }
                        } while (++s < end);

                        if (s < end) // the end of the line was found
                        {
                            s++; // skip the LF
                            int j, code = 0;
                            for (j = 0; s < end && j < 3; j++)
                            {
                                if (*s >= '0' && *s <= '9')
                                {
                                    code = 10 * code + (*s - '0');
                                    s++;
                                }
                                else
                                    break;
                            }
                            // test for the end of the multi-line text - must be the same reply code followed by a space
                            if (j == 3 && code == ftpRplCode && s < end && *s == ' ')
                                break;
                        }
                    }
                }

                if (s < end && *s == ' ') // according to the specification there must be a space after the reply code
                {
                    while (++s < end) // find the end of the line (CRLF and, contrary to the specification, also LF)
                    {
                        if (*s == '\r' && s + 1 < end && *(s + 1) == '\n')
                        {
                            s++;
                            break; // CRLF
                        }
                        else
                        {
                            if (*s == '\n')
                                break; // LF
                        }
                    }

                    if (s < end) // we found the end of the line - we have the entire FTP reply
                    {
                        s++; // skip the LF
                        ret = TRUE;
                        *reply = readBytes + readBytesOffset;
                        *replySize = (int)(s - (readBytes + readBytesOffset));
                        if (replyCode != NULL)
                            *replyCode = ftpRplCode;
                        syntaxOK = TRUE;
                    }
                }
                else
                {
                    if (s < end)
                        TRACE_E("Unexpected syntax of FTP reply (space doesn't follow three digit number).");
                }
            }
        }

        if (s < end && !syntaxOK) // unexpected syntax, return a line terminated by CRLF or just LF
        {
            int len = 0;
            while (s < end)
            {
                if (*s == '\r' && s + 1 < end && *(s + 1) == '\n')
                {
                    s++;
                    break; // CRLF
                }
                else
                {
                    if (*s == '\n')
                        break; // LF
                }
                if (++len >= 1000)
                    break; // most likely not an FTP server at all, aborting
                s++;
            }
            if (s < end) // we found the entire line (or the line length exceeded the limit)
            {
                ret = TRUE;
                *reply = readBytes + readBytesOffset;
                *replySize = (int)(s - (readBytes + readBytesOffset) + 1); // +1 to include the character *s
                if (replyCode != NULL)
                    *replyCode = -1;
            }
        }
    }
    return ret;
}

BOOL FTPGetDirectoryFromReply(const char* reply, int replySize, char* dirBuf, int dirBufSize)
{
    CALL_STACK_MESSAGE3("FTPGetDirectoryFromReply(, %d, , %d)", replySize, dirBufSize);

    if (dirBufSize > 0)
        dirBuf[0] = 0;
    else
    {
        TRACE_E("Insufficient buffer space in FTPGetDirectoryFromReply().");
        return FALSE;
    }
    BOOL ok = FALSE;
    const char* end = reply + replySize;
    const char* s = reply + 4;

    // rfc 959 format: 257<space>"<directory-name>"<space><commentary>
    // VxWorks FTP server returns: 257 Current directory is "mars:"
    // German AIX returns for a'g'f: 257 '/projects/acaix3/iplus/lnttmp/a'g'f' ist das aktuelle Verzeichnis.
    // German AIX returns for a"d: 257 '/projects/acaix3/iplus/lnttmp/a"d' ist das aktuelle Verzeichnis.

    // solution for VxWorks: find the first '"'
    // solution for German AIX: detect '\'' before '"'; the path reaches up to the last '\'', and there is no '"' after the last '\''

    while (s < end && *s != '"' && *s != '\'')
        s++;
    if (*s == '\'') // German AIX
    {
        const char* lastQuote = strrchr(s + 1, '\'');
        if (lastQuote != NULL && strchr(lastQuote, '"') == NULL)
        {
            int len = (int)(lastQuote - (s + 1));
            if (len >= dirBufSize)
                len = dirBufSize - 1;
            memcpy(dirBuf, s + 1, len);
            dirBuf[len] = 0;
            return TRUE;
        }
        while (s < end && *s != '"')
            s++;
    }
    if (s < end && *s == '"') // must start with '"'
    {
        char* d = dirBuf;
        char* endDir = dirBuf + dirBufSize - 1; // we must leave space for the terminating null character
        while (++s < end && d < endDir)
        {
            if (*s == '"')
            {
                if (s + 1 < end && *(s + 1) == '"')
                    s++; // '""' = '"' (escape sequence)
                else
                {
                    ok = TRUE; // end of the directory name
                    break;
                }
            }
            *d++ = *s;
        }
        *d = 0;
    }
    if (!ok)
        TRACE_E("Syntax error in get-directory reply (reply code 257) in FTPGetDirectoryFromReply().");
    return ok;
}

BOOL FTPGetIPAndPortFromReply(const char* reply, int replySize, DWORD* ip, unsigned short* port)
{
    CALL_STACK_MESSAGE3("FTPGetIPAndPortFromReply(%s, %d, ,)", reply, replySize);

    const char* end = reply + (replySize == -1 ? strlen(reply) : replySize);
    const char* s = reply + 4;
    int h1, h2, h3, h4;
    int p1, p2;
    while (s < end) // looking for a sequence of six numbers separated by ',' and whitespace
    {
        if (*s >= '0' && *s <= '9') // hope for the start of the sequence
        {
            int i;
            for (i = 0; i < 6; i++)
            {
                // reading one number
                int n = 0;
                while (s < end && *s >= '0' && *s <= '9')
                {
                    n = n * 10 + (*s - '0');
                    s++;
                }

                if (n < 256) // if it is a byte value, assign it to the appropriate variable
                {
                    switch (i)
                    {
                    case 0:
                        h1 = n;
                        break;
                    case 1:
                        h2 = n;
                        break;
                    case 2:
                        h3 = n;
                        break;
                    case 3:
                        h4 = n;
                        break;
                    case 4:
                        p1 = n;
                        break;
                    case 5:
                        p2 = n;
                        break;
                    }
                }
                else
                    break; // number too large, cannot be the requested sextet

                if (i == 5) // end of search, success
                {
                    *ip = (h4 << 24) + (h3 << 16) + (h2 << 8) + h1;
                    *port = (p1 << 8) + p2;
                    return TRUE;
                }

                BOOL delim = FALSE; // allow only one comma
                while (s < end)
                {
                    if (*s > ' ' && (delim || *s != ','))
                        break;
                    if (*s == ',')
                        delim = TRUE;
                    s++;
                }
                if (s >= end || *s != ',' && (*s < '0' || *s > '9'))
                    break; // unexpected format
            }
        }
        s++;
    }
    TRACE_E("FTPGetIPAndPortFromReply(): unexpected format of 'passive' reply!");
    return FALSE;
}

BOOL FTPGetDataSizeInfoFromSrvReply(CQuadWord& size, const char* reply, int replySize)
{
    CALL_STACK_MESSAGE1("FTPGetDataSizeInfoFromSrvReply(, ,)");

    const char* s = reply;
    const char* end = reply + replySize;
    while (s < end)
    {
        while (s < end && *s != '(')
            s++;
        if (s < end)
            s++;
        else
            continue;
        size.Set(0, 0);
        if (s >= end || *s < '0' || *s > '9')
            continue;
        while (s < end && *s >= '0' && *s <= '9')
            size = size * CQuadWord(10, 0) + CQuadWord((*s++ - '0'), 0);
        if (s >= end || *s > ' ')
            continue;
        while (s < end && *s <= ' ')
            s++;
        if (s >= end || !IsCharAlpha(*s))
            continue;
        while (s < end && IsCharAlpha(*s))
            s++;
        if (s >= end || *s != ')')
            continue;

        // we have the total size of the listing - "num"
        return TRUE;
    }
    return FALSE;
}

void FTPMakeVMSDirName(char* vmsDirNameBuf, int vmsDirNameBufSize, const char* dirName)
{
    if (vmsDirNameBufSize <= 0)
        return;
    int dirNameLen = (int)strlen(dirName);
    if (dirNameLen > 0 && dirName[dirNameLen - 1] == '.' &&
        !FTPIsVMSEscapeSequence(dirName, dirName + (dirNameLen - 1)))
    {
        dirNameLen--; // ignore the period at the end of the name
    }
    if (dirNameLen >= vmsDirNameBufSize)
        dirNameLen = vmsDirNameBufSize - 1;
    memcpy(vmsDirNameBuf, dirName, dirNameLen);
    lstrcpyn(vmsDirNameBuf + dirNameLen, ".DIR;1", vmsDirNameBufSize - dirNameLen);
}

BOOL FTPIsVMSEscapeSequence(const char* pathBeginning, const char* checkedChar)
{
    const char* t = checkedChar;
    while (--t >= pathBeginning && *t == '^')
        ;
    return (((checkedChar - t) - 1) & 1) != 0; // an odd number of '^' before a character = escaped character
}

BOOL FTPPathEndsWithDelimiter(CFTPServerPathType type, const char* path)
{
    CALL_STACK_MESSAGE3("FTPPathEndsWithDelimiter(%d, %s)", (int)type, path);
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in FTPPathEndsWithDelimiter()");
        return FALSE;
    }

    int l = (int)strlen(path);
    const char* s = path + l - 1;
    switch (type)
    {
    case ftpsptOpenVMS:
        return l > 1 && *s == ']' && !FTPIsVMSEscapeSequence(path, s) &&
               *(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1);

    case ftpsptMVS:
        return l > 1 && *s == '\'' && *(s - 1) == '.';

    case ftpsptTandem:
    case ftpsptIBMz_VM:
        return l > 0 && *s == '.';

    default:
        return l > 0 && (*s == '/' || (type == ftpsptNetware || type == ftpsptWindows ||
                                       type == ftpsptOS2) &&
                                          *s == '\\');
    }
}

BOOL FTPIBMz_VmCutTwoDirectories(char* path, int pathBufSize, char* cutDir, int cutDirBufSize)
{
    CALL_STACK_MESSAGE4("FTPIBMz_VmCutTwoDirectories(%s, %d, , %d,)", path, pathBufSize, cutDirBufSize);
    if (cutDirBufSize > 0)
        cutDir[0] = 0;
    if (path == NULL)
    {
        TRACE_E("Unexpected situation in FTPIBMz_VmCutTwoDirectories()");
        return FALSE;
    }

    int l = (int)strlen(path);
    if (pathBufSize < l + 1)
        pathBufSize = l + 1;

    char* lastPeriod = path + l;
    while (--lastPeriod >= path && *lastPeriod != '.')
        ;
    char* prevPeriod = lastPeriod;
    while (--prevPeriod >= path && *prevPeriod != '.')
        ;
    char* prevPrevPeriod = prevPeriod;
    while (--prevPrevPeriod >= path && *prevPrevPeriod != '.')
        ;
    BOOL willBeRoot = FALSE;
    if (prevPrevPeriod < path)
    {
        if (prevPeriod < path || lastPeriod + 1 == path + l)
            return FALSE; // invalid path or fewer than two components (only two periods, one of them at the end)
        willBeRoot = TRUE;
    }
    if (*(path + l - 1) == '.')
    {
        *(path + --l) = 0; // removal of trailing '.'
        lastPeriod = prevPrevPeriod;
        while (--lastPeriod >= path && *lastPeriod != '.')
            ;
        if (lastPeriod < path)
            willBeRoot = TRUE;
    }
    else
        prevPrevPeriod = prevPeriod;
    if (cutDirBufSize > 0)
        lstrcpyn(cutDir, prevPrevPeriod + 1, cutDirBufSize);
    *(prevPrevPeriod + (willBeRoot ? 1 : 0)) = 0;
    return TRUE;
}

BOOL FTPVMSCutFileVersion(char* name, int nameLen)
{
    if (nameLen == -1)
        nameLen = (int)strlen(name);
    char* s = name + nameLen;
    while (--s > name && *s >= '0' && *s <= '9')
        ;                      // skip the version number
    if (s > name && *s == ';') // if trimming the version number succeeded (there is a ';' before the number) and at least one character of the name remains
    {
        *s = 0;
        return TRUE;
    }
    return FALSE;
}

BOOL FTPIsPathRelative(CFTPServerPathType pathType, const char* path)
{
    if (path == NULL)
        return FALSE;
    switch (pathType)
    {
    case ftpsptUnix:
    case ftpsptAS400:
        return *path != '/';

    case ftpsptWindows:
    case ftpsptNetware:
        return *path != '/' && *path != '\\';

    case ftpsptOS2:
        return *path != '\\' && *path != '/' && *path != 0 && *(path + 1) != ':'; // absolute paths: C:/dir1, /dir1

    case ftpsptOpenVMS: // relative paths: aaa, [.aaa], aaa.bbb, [.aaa.bbb]
    {                   // absolute paths: PUB$DEVICE:[PUB], [PUB.VMS]
        return (*path != '[' || *(path + 1) == '.') && strchr(path, ':') == NULL;
    }

    case ftpsptMVS:
        return *path != '\'';

    case ftpsptIBMz_VM:
        return strchr(path, ':') == NULL;

    case ftpsptTandem:
        return *path != '\\';
    }
    TRACE_E("Unknown path type in FTPIsPathRelative()");
    return FALSE;
}

BOOL FTPCutFirstDirFromRelativePath(CFTPServerPathType pathType, char* path, char* cut,
                                    int cutBufSize)
{
    if (path == NULL)
        return FALSE;
    if (cutBufSize > 0)
        cut[0] = 0;
    char* s = path;
    switch (pathType)
    {
    case ftpsptUnix:
    case ftpsptAS400:
    {
        s = strchr(path, '/');
        if (s != NULL) // "aaa/bbb"
        {
            *s = 0;
            lstrcpyn(cut, path, cutBufSize);
            memmove(path, s + 1, strlen(s + 1) + 1);
        }
        else // "aaa"
        {
            if (*path == 0)
                return FALSE;
            lstrcpyn(cut, path, cutBufSize);
            *path = 0;
        }
        return TRUE;
    }

    case ftpsptWindows:
    case ftpsptNetware:
    case ftpsptOS2:
    {
        while (*s != 0 && *s != '/' && *s != '\\')
            s++;
        if (*s != 0) // "aaa/bbb" or "aaa\\bbb"
        {
            *s = 0;
            lstrcpyn(cut, path, cutBufSize);
            memmove(path, s + 1, strlen(s + 1) + 1);
        }
        else // "aaa"
        {
            if (*path == 0)
                return FALSE;
            lstrcpyn(cut, path, cutBufSize);
            *path = 0;
        }
        return TRUE;
    }

    case ftpsptOpenVMS: // relative paths: aaa, [.aaa], aaa.bbb, [.aaa.bbb] + for files (should not appear here): [.pub]file.txt;1
    {                   // absolute paths: PUB$DEVICE:[PUB], [PUB.VMS]
        if (*path == '[')
        {
            if (*(path + 1) == '.')
            {
                s = path + 2;
                while (*s != 0 && (*s != '.' && *s != ']' || FTPIsVMSEscapeSequence(path, s)))
                    s++;
                int l = (int)(s - (path + 2));
                if (l >= cutBufSize)
                    l = cutBufSize - 1;
                lstrcpyn(cut, path + 2, l + 1);
                if (*s == '.')
                {
                    s++;
                    if (*s != ']' && *s != 0)
                    {
                        memmove(path + 2, s, strlen(s) + 1);
                        return TRUE;
                    }
                }
                if (*s == ']')
                    memmove(path, s + 1, strlen(s + 1) + 1); // incorrectly splits "[.a]b.c;1" into "a", "b" and "c;1", but only directories should reach here, so we ignore it
                else
                    path[0] = 0; // that was the end of the path
            }
            else
                return FALSE; // not a relative path
        }
        else
        {
            s = strchr(path, '.');
            if (s != NULL) // "aaa.bbb"
            {
                *s = 0;
                lstrcpyn(cut, path, cutBufSize);
                memmove(path, s + 1, strlen(s + 1) + 1);
            }
            else // "aaa"
            {
                if (*path == 0)
                    return FALSE;
                lstrcpyn(cut, path, cutBufSize);
                *path = 0;
            }
        }
        return TRUE;
    }

    case ftpsptTandem:
    case ftpsptMVS:
    case ftpsptIBMz_VM:
    {
        s = strchr(path, '.');
        if (s != NULL) // "aaa.bbb"
        {
            *s = 0;
            lstrcpyn(cut, path, cutBufSize);
            memmove(path, s + 1, strlen(s + 1) + 1);
        }
        else // "aaa"
        {
            if (*path == 0)
                return FALSE;
            lstrcpyn(cut, path, cutBufSize);
            *path = 0;
        }
        return TRUE;
    }
    }
    return FALSE;
}

BOOL FTPCompleteAbsolutePath(CFTPServerPathType pathType, char* path, int pathBufSize,
                             const char* workPath)
{
    if (path == NULL || workPath == NULL)
        return FALSE;
    switch (pathType)
    {
    case ftpsptOS2:
    {
        if (path[0] == '/' || path[0] == '\\')
        {
            if (workPath[0] != 0 && workPath[1] == ':')
            {
                int l = (int)strlen(path);
                if (l + 3 > pathBufSize)
                    l = pathBufSize - 3;
                if (l >= 0)
                {
                    memmove(path + 2, path, l + 1);
                    path[l + 2] = 0;
                    path[0] = workPath[0];
                    path[1] = ':';
                }
            }
            else
                return FALSE; // workPath is not a full absolute path
        }
        break;
    }

    case ftpsptOpenVMS:
    {
        if (path[0] == '[' && path[1] != '.')
        {
            const char* s = strchr(workPath, ':');
            if (s != NULL && *(s + 1) == '[')
            {
                s++;
                int l = (int)strlen(path);
                if (l + 1 + (s - workPath) > pathBufSize)
                    l = pathBufSize - (int)(1 + (s - workPath));
                if (l >= 0)
                {
                    memmove(path + (s - workPath), path, l + 1);
                    path[l + (s - workPath)] = 0;
                    memmove(path, workPath, (s - workPath));
                }
            }
            else
                return FALSE; // workPath is not a full absolute path
        }
        break;
    }
    }
    return TRUE;
}

BOOL FTPRemovePointsFromPath(char* path, CFTPServerPathType pathType)
{
    BOOL backslash = TRUE;
    switch (pathType)
    {
        // case ftpsptIBMz_VM, ftpsptMVS, ftpsptOpenVMS: do nothing

    case ftpsptUnix:
    case ftpsptAS400:
        backslash = FALSE; // the break is intentionally missing here!
    case ftpsptNetware:
    case ftpsptWindows:
    case ftpsptOS2:
    {
        char backslashSep = backslash ? '\\' : '/'; // backslash==FALSE -> every check will test '/' twice (instead of both slash and backslash)
        char* afterRoot = path + (*path == '/' || *path == backslashSep ? 1 : 0);
        if (pathType == ftpsptOS2 && afterRoot == path && *path != 0 && *(path + 1) == ':')
            afterRoot = path + 2 + (*(path + 2) == '/' || *(path + 2) == backslashSep ? 1 : 0);

        char* d = afterRoot; // pointer beyond the root path
        while (*d != 0)
        {
            while (*d != 0 && *d != '.')
                d++;
            if (*d == '.')
            {
                if (d == afterRoot || d > afterRoot && (*(d - 1) == '/' || *(d - 1) == backslashSep)) // '.' after the root path or "/." ("\\.")
                {
                    if (*(d + 1) == '.' && (*(d + 2) == '/' || *(d + 2) == backslashSep || *(d + 2) == 0)) // ".."
                    {
                        char* l = d - 1;
                        while (l > afterRoot && *(l - 1) != '/' && *(l - 1) != backslashSep)
                            l--;
                        if (l >= afterRoot) // omitting the directory plus ".."
                        {
                            if (*(d + 2) == 0)
                                *l = 0;
                            else
                                memmove(l, d + 3, strlen(d + 3) + 1);
                            d = l;
                        }
                        else
                            return FALSE; // ".." cannot be omitted
                    }
                    else
                    {
                        if (*(d + 1) == '/' || *(d + 1) == backslashSep || *(d + 1) == 0) // "."
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
        break;
    }
    }
    return TRUE;
}

BOOL FTPIsCaseSensitive(CFTPServerPathType pathType)
{
    switch (pathType)
    {
    case ftpsptUnix:
        return TRUE;

    // case ftpsptOS2:
    // case ftpsptNetware:
    // case ftpsptOpenVMS:
    // case ftpsptMVS:
    // case ftpsptWindows:
    // case ftpsptIBMz_VM:
    // case ftpsptTandem:
    // case ftpsptAS400:
    default:
        return FALSE;
    }
}

BOOL FTPIsEmptyDirListErrReply(const char* listErrReply)
{
    return strlen(listErrReply) > 4 &&
           (_strnicmp(listErrReply + 4, "file not found", 14) == 0 ||                  // VMS (cs.felk.cvut.cz) reports an empty directory (cannot be considered an error)
            _strnicmp(listErrReply + 4, "The specified directory is empty", 32) == 0); // Z/VM (vm.marist.edu) reports an empty directory (cannot be considered an error)
}

BOOL FTPMayBeValidNameComponent(const char* name, const char* path, BOOL isDir, CFTPServerPathType pathType)
{
    switch (pathType)
    {
    case ftpsptUnix:
        return strcmp(name, ".") != 0 && strcmp(name, "..") != 0 && strchr(name, '/') == NULL;

    case ftpsptWindows:
    case ftpsptNetware:
    {
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        {
            const char* s = name;
            while (*s != 0 && *s != '/' && *s != '\\')
                s++;
            return *s == 0;
        }
        else
            return FALSE;
    }

    case ftpsptOS2:
    {
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        {
            const char* s = name;
            while (*s != 0 && *s != '/' && *s != '\\' && *s != ':')
                s++;
            return *s == 0;
        }
        else
            return FALSE;
    }

    case ftpsptOpenVMS:
    {
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        {
            const char* s = name;
            if (isDir)
            {
                while (*s != 0 && (*s != ' ' && *s != '/' && *s != '\\' && *s != ':' && *s != '.' && *s != '[' &&
                                       *s != ']' ||
                                   FTPIsVMSEscapeSequence(name, s)))
                    s++;
                return *s == 0;
            }
            else
            {
                while (*s != 0 && (*s != ' ' && *s != '/' && *s != '\\' && *s != ':' && *s != '[' &&
                                       *s != ']' ||
                                   FTPIsVMSEscapeSequence(name, s)))
                    s++;
                if (*s == 0)
                {
                    s = name;
                    while (*s != 0 && (*s != '.' || FTPIsVMSEscapeSequence(name, s)))
                        s++;
                    if (*s != 0)
                    {
                        s++;
                        while (*s != 0 && (*s != '.' || FTPIsVMSEscapeSequence(name, s)))
                            s++;
                        if (*s != 0)
                            return FALSE; // two periods in the file name are not allowed
                    }
                    return TRUE;
                }
                return FALSE;
            }
        }
        else
            return FALSE;
    }

    case ftpsptMVS:
        return strcmp(name, ".") != 0 && strcmp(name, "..") != 0 && strchr(name, '\'') == NULL;

    case ftpsptIBMz_VM:
    {
        if (isDir)
        {
            const char* s = name;
            while (*s != 0 && *s != '.' && *s != ':')
                s++;
            return *s == 0;
        }
        else
        {
            if (strcmp(name, ".") != 0 && strchr(name, ':') == NULL)
            {
                const char* s = name;
                while (*s != 0 && *s != '.')
                    s++;
                if (*s != 0)
                {
                    s++;
                    while (*s != 0 && *s != '.')
                        s++;
                    if (*s != 0)
                        return FALSE; // two periods in the file name are not allowed
                }
                return TRUE;
            }
            else
                return FALSE;
        }
    }

    case ftpsptTandem:
    {
        const char* s = name;
        while (*s != 0 && *s != '.' && *s != '/' && *s != '\\')
            s++;
        return *s == 0;
    }

    case ftpsptAS400: // one slash allowed ("a.file/a.mbr")
    {
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        {
            const char* slash = strchr(name, '/');
            if (slash != NULL &&
                (isDir || !FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", path) || strchr(slash + 1, '/') != NULL))
            {
                return FALSE; // one slash for a directory name or outside the /qsys.lib path, or more than one slash on the /qsys.lib path -> error
            }
            return TRUE;
        }
        else
            return FALSE;
    }

    default:
    {
        TRACE_E("FTPIsValidName(): unexpected path type!");
        return TRUE; // we do not know what path that is, so assume OK (if it is not, the server will report it later; at worst it creates more subdirectories, tough luck)
    }
    }
}

void FTPAddOperationMask(CFTPServerPathType pathType, char* targetPath, int targetPathBufSize,
                         BOOL noFilesSelected)
{
    switch (pathType)
    {
    case ftpsptUnix:
    case ftpsptNetware:
    case ftpsptWindows:
    case ftpsptOS2:
    case ftpsptAS400:
    {
        FTPPathAppend(pathType, targetPath, targetPathBufSize, "*.*", FALSE);
        break;
    }

    case ftpsptOpenVMS:
    {
        if (noFilesSelected)
            FTPPathAppend(pathType, targetPath, targetPathBufSize, "*", TRUE);
        else
            FTPPathAppend(pathType, targetPath, targetPathBufSize, "*.*", FALSE);
        break;
    }

    case ftpsptTandem:
    case ftpsptMVS:
    {
        FTPPathAppend(pathType, targetPath, targetPathBufSize, "*", FALSE);
        break;
    }

    case ftpsptIBMz_VM:
    {
        if (noFilesSelected)
            FTPPathAppend(pathType, targetPath, targetPathBufSize, "*", TRUE);
        else
            FTPPathAppend(pathType, targetPath, targetPathBufSize, "*.*", FALSE);
        break;
    }
    }
}

#define MAX_VMS_COMP_LEN 39

BOOL FTPVMSIsSimpleName(const char* name, BOOL isDir)
{
    const char* ext = !isDir ? strrchr(name, '.') : NULL;
    const char* ver = !isDir ? strrchr(name, ';') : NULL;
    if (ver != NULL && ver < ext)
        ver = NULL;

    // check whether the name is not too long
    if (isDir)
    {
        if (strlen(name) > MAX_VMS_COMP_LEN)
            return FALSE;
    }
    else
    {
        if (ext == NULL)
        {
            if (ver == NULL)
            {
                if (strlen(name) > MAX_VMS_COMP_LEN)
                    return FALSE;
            }
            else
            {
                if (ver - name > MAX_VMS_COMP_LEN)
                    return FALSE;
            }
        }
        else
        {
            if (ver == NULL)
            {
                if (ext - name > MAX_VMS_COMP_LEN || strlen(ext) - 1 > MAX_VMS_COMP_LEN)
                    return FALSE;
            }
            else
            {
                if (ext - name > MAX_VMS_COMP_LEN || (ver - ext) - 1 > MAX_VMS_COMP_LEN)
                    return FALSE;
            }
        }
    }

    const char* s = name;
    while (*s != 0)
    {
        if (/**s >= 'a' && *s <= 'z' ||*/ *s >= 'A' && *s <= 'Z' || *s >= '0' && *s <= '9' || *s == '_')
            s++;
        else
        {
            if (*s == '.' && s == ext)
                s++;
            else
            {
                if (*s == ';' && s == ver)
                {
                    const char* num = s + 1;
                    int verNum = 0;
                    while (*num != 0 && *num >= '0' && *num <= '9')
                    {
                        verNum = 10 * verNum + (*num - '0');
                        num++;
                    }
                    if (*num == 0 && verNum >= 1 && verNum <= 32767)
                        s = num; // file version is OK
                    else
                        return FALSE; // invalid file version
                }
                else
                    return FALSE; // invalid character in the name
            }
        }
    }
    return TRUE;
}

void FTPGenerateNewName(int* phase, char* newName, int* index, const char* originalName,
                        CFTPServerPathType pathType, BOOL isDir, BOOL alreadyRenamedFile)
{
    char suffix[20];
    int suffixLen;
    BOOL firstCallInPhase = *index == 0;
    switch (pathType)
    {
    case ftpsptNetware:
    case ftpsptWindows:
    case ftpsptOS2:
        *phase = 1;
        // the break is not missing here!
    case ftpsptUnix:
    {
        if (*phase == 0) // at this stage we assume a classic UNIX (names max 255 characters, do not contain '/' or '\0')
        {
            const char* s = originalName;
            char* n = newName;
            char* dot = NULL;    // files only: the first period from the right that is not at the beginning of the name
            char* end = n + 255; // MAX_PATH==260, so the 'newName' buffer is large enough
            BOOL containsSlash = FALSE;
            while (*s != 0 && n < end)
            {
                if (*s != '/')
                {
                    if (*s == '.' && !isDir && s != originalName)
                        dot = n; // NOTE: exception, this is not Windows; ".cvspass" on UNIX is not an extension
                    *n++ = *s++;
                }
                else
                {
                    *n++ = '_'; // replace '/' with '_'
                    s++;
                    containsSlash = TRUE;
                }
            }
            *n = 0;
            if (*index == 0 && *s == 0 && !containsSlash) // newName is identical to originalName, we must adjust newName
                *index = 1;                               // also handles the reserved names "." and ".."
            if (*index != 0)                              // append " (number)" after the name
            {
                if (alreadyRenamedFile) // ensure "name (2)"->"name (3)" instead of ->"name (2) (2)"
                {
                    char* s2 = dot == NULL ? n : dot;
                    if (s2 > newName)
                        s2--;
                    if (*s2 == ')') // searching backwards for " (number)"
                    {
                        char* end2 = s2 + 1;
                        int num = 0;
                        int digit = 1;
                        while (--s2 >= newName && *s2 >= '0' && *s2 <= '9')
                        {
                            num += digit * (*s2 - '0');
                            digit *= 10;
                        }
                        if (s2 > newName && *s2 == '(' && *(s2 - 1) == ' ')
                        {
                            memmove(s2 - 1, end2, (n - end2) + 1);
                            if (num < 1)
                                num = 1;
                            if (firstCallInPhase)
                                *index = num;
                            n -= end2 - (s2 - 1);
                            if (dot != NULL)
                                dot -= end2 - (s2 - 1);
                        }
                    }
                }
                sprintf(suffix, n > newName ? " (%d)" : "(%d)", (*index + 1));
                suffixLen = (int)strlen(suffix);
                if (255 - (n - newName) < suffixLen)
                {
                    int cut = (int)(suffixLen - (255 - (n - newName)));
                    if (dot != NULL && dot - newName > cut) // shortening in the name
                    {
                        memmove(dot - cut, dot, (n - dot) + 1);
                        dot -= cut;
                        n -= cut;
                    }
                    else // just trim the end
                    {
                        n = newName + 255 - suffixLen;
                        if (dot == n - 1)
                            n--; // if a '.' would remain at the end of the name, trim it as well
                        *n = 0;
                        if (dot >= n)
                            dot = NULL;
                    }
                }
                if (dot == NULL)
                    memcpy(n, suffix, suffixLen + 1);
                else
                {
                    memmove(dot + suffixLen, dot, (n - dot) + 1);
                    memcpy(dot, suffix, suffixLen);
                }
            }
            (*index)++;
            if (!SalamanderGeneral->SalIsValidFileNameComponent(newName))
                *phase = 1;
            else
                *phase = -1;
        }
        else // at this stage the names will be valid on Windows (for FTP servers on Windows pretending to be UNIX) + ftpsptNetware + ftpsptWindows + ftpsptOS2
        {
            lstrcpyn(newName, originalName, MAX_PATH);
            SalamanderGeneral->SalMakeValidFileNameComponent(newName);
            if (*index == 0 && strcmp(newName, originalName) == 0)
                *index = 1;  // newName is identical to originalName, we must adjust newName
            if (*index != 0) // append " (number)" after the name
            {
                char* n = newName + strlen(newName);
                char* dot = isDir ? NULL : strrchr(newName + 1, '.'); // files only: the first period from the right that is not at the beginning of the name; NOTE: exception, because phase==0 means that we are not dealing with Windows: ".cvspass" on UNIX is not an extension
                if (alreadyRenamedFile)                               // ensure "name (2)"->"name (3)" instead of ->"name (2) (2)"
                {
                    char* s = dot == NULL ? n : dot;
                    if (s > newName)
                        s--;
                    if (*s == ')') // searching backwards for " (number)"
                    {
                        char* end = s + 1;
                        int num = 0;
                        int digit = 1;
                        while (--s >= newName && *s >= '0' && *s <= '9')
                        {
                            num += digit * (*s - '0');
                            digit *= 10;
                        }
                        if (s > newName && *s == '(' && *(s - 1) == ' ')
                        {
                            memmove(s - 1, end, (n - end) + 1);
                            if (num < 1)
                                num = 1;
                            if (firstCallInPhase)
                                *index = num;
                            n -= end - (s - 1);
                            if (dot != NULL)
                                dot -= end - (s - 1);
                        }
                    }
                }
                sprintf(suffix, " (%d)", (*index + 1));
                suffixLen = (int)strlen(suffix);
                if (MAX_PATH - 4 - (n - newName) < suffixLen)
                {
                    int cut = (int)(suffixLen - (MAX_PATH - 4 - (n - newName)));
                    if (dot != NULL && dot - newName > cut) // shortening in the name
                    {
                        memmove(dot - cut, dot, (n - dot) + 1);
                        dot -= cut;
                        n -= cut;
                    }
                    else // just trim the end
                    {
                        n = newName + MAX_PATH - 4 - suffixLen;
                        if (dot == n - 1)
                            n--; // if a '.' would remain at the end of the name, trim it as well
                        *n = 0;
                        if (dot >= n)
                            dot = NULL;
                    }
                }
                if (dot == NULL)
                    memcpy(n, suffix, suffixLen + 1);
                else
                {
                    memmove(dot + suffixLen, dot, (n - dot) + 1);
                    memcpy(dot, suffix, suffixLen);
                }
                SalamanderGeneral->SalMakeValidFileNameComponent(newName); // an invalid name might have been produced, so let it be corrected if needed
            }
            (*index)++;
            *phase = -1; // there is no further phase of name generation
        }
        break;
    }

    case ftpsptOpenVMS:
    {
        if (*phase == 0) // drop the obviously forbidden characters and optionally append "_number" to the name
        {
            const char* s = originalName;
            const char* ext = !isDir ? strrchr(originalName, '.') : NULL;
            const char* ver = !isDir ? strrchr(originalName, ';') : NULL;
            if (ver != NULL && ver < ext)
                ver = NULL;
            char* n = newName;
            char* dot = NULL;     // first period (extension)
            char* semicol = NULL; // ';' separating the file version
            char* end = n + MAX_PATH - 4;
            BOOL changed = FALSE;
            while (*s != 0 && n < end)
            {
                if (*s != ';' && *s != ' ' && *s != '/' && *s != '\\' && *s != ':' && *s != '.' && *s != '[' && *s != ']' ||
                    FTPIsVMSEscapeSequence(originalName, s))
                {
                    *n++ = *s++;
                }
                else
                {
                    if (*s == '.' && s == ext)
                    {
                        dot = n;
                        *n++ = *s++;
                    }
                    else
                    {
                        if (*s == ';' && s == ver)
                        {
                            const char* num = s + 1;
                            while (*num != 0 && *num >= '0' && *num <= '9')
                                num++;
                            if (*num == 0)
                            {
                                semicol = n;
                                *n++ = *s++; // the number is OK (we do not check its size; we leave that to VMS)
                            }
                            else
                            {
                                *n++ = '_'; // replace the forbidden character with '_'
                                s++;
                                changed = TRUE;
                            }
                        }
                        else
                        {
                            *n++ = '_'; // replace the forbidden character with '_'
                            s++;
                            changed = TRUE;
                        }
                    }
                }
            }
            *n = 0;
            if (*index == 0 && *s == 0 && !changed) // newName is identical to originalName, we must adjust newName
                *index = 1;                         // also handles the reserved name "." (by the way: ".." -> "__" or "_.")
            if (*index != 0)                        // append "_number" after the name
            {
                if (alreadyRenamedFile) // ensure "name_2"->"name_3" instead of ->"name_2_2"
                {
                    char* s2 = dot == NULL ? (semicol == NULL ? n : semicol) : dot;
                    if (s2 > newName)
                    {
                        s2--;
                        if (*s2 >= '0' && *s2 <= '9') // searching backwards for "_number"
                        {
                            char* end2 = s2 + 1;
                            int num = 0;
                            int digit = 1;
                            do
                            {
                                num += digit * (*s2 - '0');
                                digit *= 10;
                                s2--;
                            } while (s2 >= newName && *s2 >= '0' && *s2 <= '9');
                            if (s2 >= newName && *s2 == '_')
                            {
                                memmove(s2, end2, (n - end2) + 1);
                                if (num < 1)
                                    num = 1;
                                if (firstCallInPhase)
                                    *index = num;
                                n -= end2 - s2;
                                if (dot != NULL)
                                    dot -= end2 - s2;
                            }
                        }
                    }
                }
                sprintf(suffix, "_%d", (*index + 1));
                suffixLen = (int)strlen(suffix);
                if (MAX_PATH - 4 - (n - newName) < suffixLen)
                {
                    int cut = (int)(suffixLen - (MAX_PATH - 4 - (n - newName)));
                    if (dot != NULL && dot - newName > cut) // shortening in the name
                    {
                        memmove(dot - cut, dot, (n - dot) + 1);
                        dot -= cut;
                        if (semicol != NULL)
                            semicol -= cut;
                        n -= cut;
                    }
                    else
                    {
                        if (semicol != NULL && semicol - newName > cut) // shortening in the extension
                        {
                            memmove(semicol - cut, semicol, (n - semicol) + 1);
                            if (dot != NULL && dot >= semicol - cut)
                                dot = NULL;
                            semicol -= cut;
                            n -= cut;
                        }
                        else // just trim the end
                        {
                            n = newName + MAX_PATH - 4 - suffixLen;
                            if (semicol == n - 1)
                                n--; // if a ';' would remain at the end of the name, trim it as well
                            *n = 0;
                            if (dot >= n)
                                dot = NULL;
                            if (semicol >= n)
                                semicol = NULL;
                        }
                    }
                }
                if (dot == NULL && semicol != NULL)
                    dot = semicol;
                if (isDir || dot == NULL)
                    memcpy(n, suffix, suffixLen + 1);
                else
                {
                    memmove(dot + suffixLen, dot, (n - dot) + 1);
                    memcpy(dot, suffix, suffixLen);
                }
            }
            (*index)++;
            if (!FTPVMSIsSimpleName(newName, isDir))
                *phase = 1;
            else
                *phase = -1;
        }
        else // remove every possible forbidden character, trim the length to MAX_VMS_COMP_LEN, and optionally append "_number" to the name
        {
            const char* s = originalName;
            const char* ext = !isDir ? strrchr(originalName, '.') : NULL;
            const char* ver = !isDir ? strrchr(originalName, ';') : NULL;
            if (ver != NULL && ver < ext)
                ver = NULL;
            // copy the name into 'newName' while removing every possible forbidden character
            char* n = newName;
            char* dot = NULL;     // first period (extension)
            char* semicol = NULL; // first semicolon (file version)
            char* end = n + MAX_PATH - 4;
            BOOL changed = FALSE;
            while (*s != 0 && n < end)
            {
                if (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' || *s >= '0' && *s <= '9' || *s == '_')
                    *n++ = UpperCase[*s++];
                else
                {
                    if (*s == '.' && s == ext)
                    {
                        dot = n;
                        *n++ = *s++;
                    }
                    else
                    {
                        if (*s == ';' && s == ver)
                        {
                            const char* num = s + 1;
                            int verNum = 0;
                            while (*num != 0 && *num >= '0' && *num <= '9')
                            {
                                verNum = 10 * verNum + (*num - '0');
                                num++;
                            }
                            if (*num == 0 && verNum >= 1 && verNum <= 32767)
                            {
                                semicol = n;
                                *n++ = *s++; // the number is OK
                            }
                            else // invalid file version - ';' is only a forbidden character here
                            {
                                *n++ = '_';
                                s++;
                                changed = TRUE;
                            }
                        }
                        else
                        {
                            *n++ = '_'; // replace the forbidden character with '_'
                            s++;
                            changed = TRUE;
                        }
                    }
                }
            }
            *n = 0;
            // shorten the name so that the components have at most MAX_VMS_COMP_LEN characters
            if (dot == NULL)
            {
                if (semicol == NULL)
                {
                    if (n - newName > MAX_VMS_COMP_LEN)
                    {
                        n = newName + MAX_VMS_COMP_LEN;
                        *n = 0;
                        changed = TRUE;
                    }
                }
                else
                {
                    if (semicol - newName > MAX_VMS_COMP_LEN)
                    {
                        memmove(newName + MAX_VMS_COMP_LEN, semicol, (n - semicol) + 1);
                        n -= semicol - (newName + MAX_VMS_COMP_LEN);
                        semicol = newName + MAX_VMS_COMP_LEN;
                        changed = TRUE;
                    }
                }
            }
            else
            {
                if (semicol == NULL)
                {
                    if (dot - newName > MAX_VMS_COMP_LEN)
                    {
                        memmove(newName + MAX_VMS_COMP_LEN, dot, (n - dot) + 1);
                        n -= dot - (newName + MAX_VMS_COMP_LEN);
                        dot = newName + MAX_VMS_COMP_LEN;
                        changed = TRUE;
                    }
                    if ((n - dot) - 1 > MAX_VMS_COMP_LEN)
                    {
                        n = dot + 1 + MAX_VMS_COMP_LEN;
                        *n = 0;
                        changed = TRUE;
                    }
                }
                else
                {
                    if (dot - newName > MAX_VMS_COMP_LEN)
                    {
                        memmove(newName + MAX_VMS_COMP_LEN, dot, (n - dot) + 1);
                        n -= dot - (newName + MAX_VMS_COMP_LEN);
                        semicol -= dot - (newName + MAX_VMS_COMP_LEN);
                        dot = newName + MAX_VMS_COMP_LEN;
                        changed = TRUE;
                    }
                    if ((semicol - dot) - 1 > MAX_VMS_COMP_LEN)
                    {
                        memmove(dot + 1 + MAX_VMS_COMP_LEN, semicol, (n - semicol) + 1);
                        n -= semicol - (dot + 1 + MAX_VMS_COMP_LEN);
                        semicol = dot + 1 + MAX_VMS_COMP_LEN;
                        changed = TRUE;
                    }
                }
            }
            if (*index == 0 && *s == 0 && !changed) // newName is identical to originalName, we must adjust newName
                *index = 1;                         // also handles the reserved name "." (by the way: ".." -> "__" or "_.")
            if (*index != 0)                        // append "_number" after the name
            {
                if (alreadyRenamedFile) // ensure "name_2"->"name_3" instead of ->"name_2_2"
                {
                    char* s2 = dot == NULL ? (semicol == NULL ? n : semicol) : dot;
                    if (s2 > newName)
                    {
                        s2--;
                        if (*s2 >= '0' && *s2 <= '9') // searching backwards for "_number"
                        {
                            char* end2 = s2 + 1;
                            int num = 0;
                            int digit = 1;
                            do
                            {
                                num += digit * (*s2 - '0');
                                digit *= 10;
                                s2--;
                            } while (s2 >= newName && *s2 >= '0' && *s2 <= '9');
                            if (s2 >= newName && *s2 == '_')
                            {
                                memmove(s2, end2, (n - end2) + 1);
                                if (num < 1)
                                    num = 1;
                                if (firstCallInPhase)
                                    *index = num;
                                n -= end2 - s2;
                                if (dot != NULL)
                                    dot -= end2 - s2;
                                if (semicol != NULL)
                                    semicol -= end2 - s2;
                            }
                        }
                    }
                }
                sprintf(suffix, "_%d", (*index + 1));
                suffixLen = (int)strlen(suffix);
                if (dot == NULL)
                {
                    if (semicol == NULL)
                    {
                        if ((n - newName) + suffixLen > MAX_VMS_COMP_LEN)
                            n = newName + MAX_VMS_COMP_LEN - suffixLen;
                        memcpy(n, suffix, suffixLen + 1);
                    }
                    else
                        dot = semicol;
                }
                if (dot != NULL)
                {
                    if ((dot - newName) + suffixLen > MAX_VMS_COMP_LEN)
                    {
                        memmove(newName + MAX_VMS_COMP_LEN - suffixLen, dot, (n - dot) + 1);
                        n -= dot - (newName + MAX_VMS_COMP_LEN - suffixLen);
                        dot = newName + MAX_VMS_COMP_LEN - suffixLen;
                    }
                    memmove(dot + suffixLen, dot, (n - dot) + 1);
                    memcpy(dot, suffix, suffixLen);
                }
            }
            (*index)++;
            *phase = -1;
        }
        break;
    }

    case ftpsptAS400: // AS/400: we do not know anything about the naming format on AS/400 yet; for now we reuse the following Tandem code, it seems restrictive enough that AS/400 should be satisfied with it as well
    case ftpsptTandem:
    {
        // full path to a file: \\SYSTEM.$VVVVV.SUBVOLUM.FILENAME
        // Tandem has different rules for the system/machine name, volume, subvolume, and file name:
        // System: begins with a backslash, first character is alphabetic, the rest alphanumeric, maximum length 7 (including the backslash)
        // Volume: begins with $, first character is alphabetic, the rest alphanumeric, maximum length 6 (including the $)
        // Subvolume + Name: first character is alphabetic, the rest alphanumeric, maximum length 8 characters
        //
        // Here it probably only makes sense to handle file names, because volumes and subvolumes are
        // they probably are not created through MKD (they are just parts of the full file name).

        // for now convert everything to UPPER-CASE, prepend an alpha character ('A') if the name does not start with one
        // if it does not start with a letter, prepend an alphabetic character ('A'); remove non-alphanumeric characters from the rest of the name and append a "number" if needed
        // finally - maximum name length is 8 characters
        const char* s = originalName;
        char* n = newName;
        char* end = n + 8;
        BOOL change = FALSE;
        if (*s < 'A' || *s > 'Z')
        {
            if (*s >= 'a' && *s <= 'z')
                *n++ = UpperCase[*s++];
            else
                *n++ = 'A';
            change = TRUE;
        }
        while (*s != 0 && n < end)
        {
            if (*s >= 'A' && *s <= 'Z' || *s >= '0' && *s <= '9')
                *n++ = *s++;
            else
            {
                if (*s >= 'a' && *s <= 'z')
                    *n++ = UpperCase[*s++];
                else
                    s++;
                change = TRUE;
            }
        }
        *n = 0;
        if (*index == 0 && *s == 0 && !change) // newName is identical to originalName, we must adjust newName
            *index = 1;
        if (*index != 0) // append "number" after the name
        {
            sprintf(suffix, "%d", (*index + 1));
            suffixLen = (int)strlen(suffix);
            if (suffixLen > 7)
            {
                TRACE_E("FTPGenerateNewName(): file number is too high!");
                suffixLen = 7;
            }
            if (8 - (n - newName) < suffixLen)
                n = newName + 8 - suffixLen;
            memcpy(n, suffix, suffixLen + 1);
        }
        (*index)++;
        *phase = -1;
        break;
    }

    case ftpsptMVS:
    {
        // Everything I have managed to learn about the MVS naming conventions:
        // -each part of the name (between periods):
        //   -must start with a letter or '#'
        //   -contains A-Z, 0-9, '#', '%', '§'
        // I am postponing the implementation; if anyone ever uses it, which I doubt. ;-)

        // for now remove '\'' and optionally append "#number" to the end
        const char* s = originalName;
        char* n = newName;
        char* end = n + MAX_PATH - 4;
        BOOL change = FALSE;
        while (*s != 0 && n < end)
        {
            if (*s != '\'')
                *n++ = *s++;
            else
            {
                *n++ = '#'; // replace '\'' with '#'' ('_' is apparently not supported by MVS)
                s++;
                change = TRUE;
            }
        }
        *n = 0;
        if (*index == 0 && *s == 0 && !change) // newName is identical to originalName, we must adjust newName
            *index = 1;                        // also handles the reserved names "." and ".."
        if (*index != 0)                       // append "#number" after the name
        {
            sprintf(suffix, "#%d", (*index + 1));
            suffixLen = (int)strlen(suffix);
            if (MAX_PATH - 4 - (n - newName) < suffixLen)
                n = newName + MAX_PATH - 4 - suffixLen;
            memcpy(n, suffix, suffixLen + 1);
        }
        (*index)++;
        *phase = -1;
        break;
    }

    case ftpsptIBMz_VM:
    {
        // Everything I have managed to learn about the z/VM naming conventions:
        // -a file name may have at most 8.8 characters; more periods are not possible,
        //  one period is mandatory (otherwise it adds ".$DEFAULT")
        // -a directory part may have at most 16 characters (periods are directory separators, so they are not allowed inside a directory name)
        // I am postponing the implementation; if anyone ever uses it, which I doubt. ;-)

        // for now remove '.' (one '.' is allowed in a file name - the first one from the right) and ':' and optionally
        // append "_number" to the end
        const char* s = originalName;
        const char* ext = isDir ? NULL : strrchr(originalName, '.');
        char* n = newName;
        char* dot = NULL;
        char* end = n + MAX_PATH - 4;
        BOOL change = FALSE;
        while (*s != 0 && n < end)
        {
            if (*s != '.' && *s != ':')
                *n++ = *s++;
            else
            {
                if (*s == '.' && s == ext)
                {
                    dot = n;
                    *n++ = *s++;
                }
                else
                {
                    *n++ = '_'; // replace forbidden characters with '_'
                    s++;
                    change = TRUE;
                }
            }
        }
        *n = 0;
        if (*index == 0 && *s == 0 && !change) // newName is identical to originalName, we must adjust newName
            *index = 1;                        // also handles the reserved name "." (by the way: ".." -> "_.")
        if (*index != 0)                       // append "_number" after the name
        {
            if (alreadyRenamedFile) // ensure "name_2"->"name_3" instead of ->"name_2_2"
            {
                char* s2 = dot == NULL ? n : dot;
                if (s2 > newName)
                {
                    s2--;
                    if (*s2 >= '0' && *s2 <= '9') // searching backwards for "_number"
                    {
                        char* end2 = s2 + 1;
                        int num = 0;
                        int digit = 1;
                        do
                        {
                            num += digit * (*s2 - '0');
                            digit *= 10;
                            s2--;
                        } while (s2 >= newName && *s2 >= '0' && *s2 <= '9');
                        if (s2 >= newName && *s2 == '_')
                        {
                            memmove(s2, end2, (n - end2) + 1);
                            if (num < 1)
                                num = 1;
                            if (firstCallInPhase)
                                *index = num;
                            n -= end2 - s2;
                            if (dot != NULL)
                                dot -= end2 - s2;
                        }
                    }
                }
            }
            sprintf(suffix, "_%d", (*index + 1));
            suffixLen = (int)strlen(suffix);
            if (MAX_PATH - 4 - (n - newName) < suffixLen)
            {
                int cut = (int)(suffixLen - (MAX_PATH - 4 - (n - newName)));
                if (dot != NULL && dot - newName > cut) // shortening in the name
                {
                    memmove(dot - cut, dot, (n - dot) + 1);
                    dot -= cut;
                    n -= cut;
                }
                else // just trim the end
                {
                    n = newName + MAX_PATH - 4 - suffixLen;
                    if (dot == n - 1)
                        n--; // if a '.' would remain at the end of the name, trim it as well
                    *n = 0;
                    if (dot >= n)
                        dot = NULL;
                }
            }
            if (dot == NULL)
                memcpy(n, suffix, suffixLen + 1);
            else
            {
                memmove(dot + suffixLen, dot, (n - dot) + 1);
                memcpy(dot, suffix, suffixLen);
            }
        }
        (*index)++;
        *phase = -1;
        break;
    }
    }
}

void FTPAS400CutFileNamePart(char* mbrName, const char* name)
{
    mbrName[0] = 0;
    const char* fileBeg = name;
    const char* fileEnd = fileBeg;
    while (*fileEnd != 0 && *fileEnd != '.')
        fileEnd++;
    if (_strnicmp(fileEnd, ".file/", 6) == 0)
    {
        const char* mbrBeg = fileEnd + 6;
        const char* mbrEnd = mbrBeg;
        while (*mbrEnd != 0 && *mbrEnd != '.')
            mbrEnd++;
        if (_stricmp(mbrEnd, ".mbr") == 0)
        {
            if (mbrEnd - mbrBeg == fileEnd - fileBeg &&
                _strnicmp(fileBeg, mbrBeg, fileEnd - fileBeg) == 0) // if the names before ".file" and before ".mbr" match, this is the case we are looking for and it will be shortened
            {
                lstrcpyn(mbrName, mbrBeg, MAX_PATH);
            }
            else // "a.file/b.mbr" -> "a.b.mbr"
            {
                if ((fileEnd - fileBeg) + 1 < MAX_PATH)
                {
                    memcpy(mbrName, fileBeg, (fileEnd - fileBeg) + 1);
                    lstrcpyn(mbrName + (fileEnd - fileBeg) + 1, mbrBeg, (int)(MAX_PATH - ((fileEnd - fileBeg) + 1)));
                }
            }
        }
    }
    if (mbrName[0] == 0)
        lstrcpyn(mbrName, name, MAX_PATH);
}

void FTPAS400AddFileNamePart(char* name)
{
    char* mbrEnd = name;
    while (*mbrEnd != 0 && *mbrEnd != '.')
        mbrEnd++;
    if (*mbrEnd != 0 && _stricmp(mbrEnd, ".mbr") != 0) // "a.b.mbr" -> "a.file/b.mbr"
    {
        char* fileEnd = ++mbrEnd;
        while (*mbrEnd != 0 && *mbrEnd != '.')
            mbrEnd++;
        if (_stricmp(mbrEnd, ".mbr") == 0 &&
            (mbrEnd - name) + 5 /* "file/" */ + 4 /* ".mbr" */ + 1 <= 2 * MAX_PATH)
        {
            memmove(fileEnd + 5, fileEnd, (mbrEnd - fileEnd) + 5);
            memcpy(fileEnd, "FILE/", 5);
        }
        return;
    }
    if (_stricmp(mbrEnd, ".mbr") == 0 && // "a.mbr" -> "a.file/a.mbr"
        2 * (mbrEnd - name) + 6 /* ".file/" */ + 4 /* ".mbr" */ + 1 <= 2 * MAX_PATH)
    {
        memmove(mbrEnd + 6 + (mbrEnd - name), mbrEnd, 5);
        memmove(mbrEnd + 6, name, mbrEnd - name);
        memcpy(mbrEnd + 1, "FILE/", 5);
    }
}
