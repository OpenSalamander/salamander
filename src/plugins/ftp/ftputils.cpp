// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

const char* FTP_ANONYMOUS = "anonymous"; // standardni jmeno pro anonymniho uzivatele

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
    case ftpsptAS400: // sice neni dokonale, ale myslim ze bude stacit (vylepseni: jde-li o cestu /qsys.lib a jmeno konci na .mbr, oriznout dve komponenty a vratit je jako jmeno souboru: "/QSYS.LIB/GARY.LIB/UCLSRC.FILE/BKPLIB2.MBR" -> "/QSYS.LIB/GARY.LIB" + "UCLSRC.FILE/BKPLIB2.MBR")
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
                *(path + --l) = 0; // zruseni '/' na konci
            lstrcpyn(cutDir, lastSlash + 1, cutDirBufSize);
        }
        if (prevSlash < path)
            *(lastSlash + 1) = 0; // "/somedir" or "/somedir/" -> "/"
        else
            *lastSlash = 0; // "/firstdir/seconddir" or "/firstdir/seconddir/" -> "/firstdir"
        return TRUE;
    }

    case ftpsptWindows:
    case ftpsptNetware: // shodne s UNIXem, jen jsou oddelovace jak '/', tak '\\'
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
                *(path + --l) = 0; // zruseni '/' na konci
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
                *(path + --l) = 0; // zruseni '/' na konci
            lstrcpyn(cutDir, lastSlash + 1, cutDirBufSize);
        }
        if (prevSlash < path)
            *(lastSlash + 1) = 0; // "C:/somedir" or "C:/somedir/" -> "C:/"
        else
            *lastSlash = 0; // "C:/firstdir/seconddir" or "C:/firstdir/seconddir/" -> "C:/firstdir"
        return TRUE;
    }

    case ftpsptOpenVMS: // "PUB$DEVICE:[PUB.VMS]" nebo "[PUB.VMS]" nebo "[PUB.VMS]filename.txt;1" + root je "[000000]" + '^' je escape-char
    {
        char* s = path + l - 1;
        char* name = s;
        while (name > path && (*name != ']' || FTPIsVMSEscapeSequence(path, name)))
            name--;
        if (name < s && name > path) // cesta se jmenem souboru: napr. "[PUB.VMS]filename.txt;1"
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
                    *fileNameCouldBeCut = FALSE; // rezeme jmeno adresare, soubor vypada jinak
                char* end = s;
                if (*(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1))
                    end = --s; // preskocime/zrusime i koncovou '.' (neescapovana '.')
                while (--s >= path && (*s != '.' && *s != '[' || FTPIsVMSEscapeSequence(path, s)))
                    ;
                if (s >= path)
                {
                    if (*s == '.') // "[pub.vms]"
                    {
                        *end = 0; // zrusime puvodni koncovou ']'
                        if (cutDirBufSize > 0)
                            lstrcpyn(cutDir, s + 1, cutDirBufSize);
                        *s++ = ']';
                        *s = 0;
                        return TRUE;
                    }
                    else // "[pub]" nebo "[000000]" (root)
                    {
                        if (strncmp(s + 1, "000000", 6) != 0 || *(s + 7) != '.' && *(s + 7) != ']') // neni root
                        {
                            *end = 0; // zrusime puvodni koncovou ']'
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

    case ftpsptMVS: // "'VEA0016.MAIN.CLIST.'", "''" je root
    {
        char* s = path + l - 1;
        if (l > 1 && *s == '\'')
        {
            char* end = s;
            if (*(s - 1) == '.')
                end = --s; // preskocime/zrusime i koncovou '.'
            while (--s >= path && *s != '.' && *s != '\'')
                ;
            if (s >= path)
            {
                if (*s == '.' || // "'pub.mvs'" nebo "'pub.mvs.'"
                    s + 1 < end) // neni root - "'pub'" nebo "'pub.'"
                {
                    *end = 0; // zrusime puvodni koncovou '\''
                    if (cutDirBufSize > 0)
                        lstrcpyn(cutDir, s + 1, cutDirBufSize);
                    if (*s == '\'')
                        s++; // "'pub'" -> prvni '\'' musime nechat
                    *s++ = '\'';
                    *s = 0;
                    return TRUE;
                }
            }
        }
        return FALSE;
    }

    case ftpsptTandem: // \SYSTEM.$VVVVV.SUBVOLUM.FILENAME, \SYSTEM je root
    {
        char* lastDot = path + l - 1;
        while (--lastDot >= path && *lastDot != '.')
            ;
        if (lastDot < path)
            return FALSE; // "\SYSTEM" or "\SYSTEM."
        if (cutDirBufSize > 0)
        {
            if (*(path + l - 1) == '.')
                *(path + --l) = 0; // zruseni '.' na konci
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
                return FALSE; // invalid path nebo root (tecka jen na konci)
            willBeRoot = TRUE;
        }
        if (*(path + l - 1) == '.')
        {
            *(path + --l) = 0; // zruseni '.' na konci
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
        if (l > 1 && path[l - 1] == ']' && !FTPIsVMSEscapeSequence(path, path + (l - 1))) // musi jit o VMS cestu ("[dir1.dir2]"), jinak neni co resit
        {
            char* s = path + l - 1;
            if (*(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1))
                s--; // neescapovana '.'
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
                { // vejdeme se s '.' (jen adresar + neni v rootu), ']' a s nulou na konci?
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
        if (l > 1 && path[l - 1] == '\'') // musi jit o MVS cestu (napr. "'dir1.dir2.'"), jinak neni co resit
        {
            char* s = path + l - 1;
            if (*(s - 1) == '.')
                s--;
            BOOL root = (s - 1 >= path && *(s - 1) == '\'');
            if (*name != 0)
            {
                int n = (int)strlen(name);
                if ((s - path) + (root ? 1 : 2) + n < pathSize)
                { // vejdeme se s '.' (krome rootu), '\'' a s nulou na konci?
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
        if (l > 0) // cesta nemuze byt prazdna (aspon root zde byt musi), jinak neni co resit
        {
            if (*name != 0)
            {
                BOOL addPeriod = path[l - 1] != '.';
                int n = (int)strlen(name);
                if (l + (addPeriod ? 1 : 0) + n < pathSize)
                { // vejdeme se s '.' (je-li treba), jmenem a s nulou na konci?
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
        else // ftpsptUnix + ostatni
        {
            if (l > 0 && path[l - 1] == '/')
                l--;
        }
        if (*name != 0)
        {
            int n = (int)strlen(name);
            if (l + 1 + n < pathSize) // vejdeme se i s nulou na konci?
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
                path[l] = 0; // krome pripadu "/" + "", to musi vyjit "/" (zaroven krome "C:/" + "" -> "C:/")
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
    case ftpsptOpenVMS: // platna cesta = aspon dva znaky + konci na ']'
    {
        if (l > 1 && path[l - 1] == ']' && !FTPIsVMSEscapeSequence(path, path + (l - 1))) // musi jit o VMS cestu ("[dir1.dir2]"), jinak neni co resit
        {
            const char* s = path + l - 1;
            if (*(s - 1) == '.' && !FTPIsVMSEscapeSequence(path, s - 1))
                s--;
            return s - path < 7 || strncmp(s - 7, "[000000", 7) != 0 || FTPIsVMSEscapeSequence(path, s - 7);
        }
        return FALSE;
    }

    case ftpsptMVS: // platna cesta = aspon dva znaky + konci na '\''
    {
        if (l > 1 && path[l - 1] == '\'') // musi jit o MVS cestu (napr. "'dir1.dir2.'"), jinak neni co resit
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
        if (l > 0) // platna cesta = neprazdna
        {
            const char* s = strchr(path, '.');
            return s == NULL || s != path + l - 1; // root konci na '.' a obsahuje jen jednu tecku
        }
        return FALSE;
    }

    case ftpsptTandem:
    {
        if (l > 0) // platna cesta = neprazdna
        {
            const char* s = strchr(path, '.');
            return s != NULL && s != path + l - 1; // root neobsahuje tecku nebo obsahuje jednu tecku na konci
        }
        return FALSE;
    }

    case ftpsptOS2:
    {
        return l > 0 &&                                                                  // platna cesta = neprazdna
               (l > 3 || path[1] != ':' || l == 3 && path[2] != '\\' && path[2] != '/'); // "C:" i "C:/" povazujeme za root
    }

    default:
    {
        return l > 0 && // platna cesta = neprazdna
               (l != 1 ||
                (path[0] != '/' && (type != ftpsptNetware && type != ftpsptWindows || path[0] != '\\')));
    }
    }
}

const char* FTPFindEndOfUserNameOrHostInURL(const char* url)
{
    // format cesty: "user:password@host:port/path"
    // pri parsovani zprava umi resit:
    //  ftp://ms-domain\name@localhost:22/pub/a
    //  ftp://test.name@nas.server.cz@localhost:22/pub/test@bla

    // zkusime URL rozparsovat zprava od oddelovace cesty ('/') nebo od konce URL
    const char* p = strchr(url, '/');
    if (p == NULL)
        p = url + strlen(url);
    const char* hostEnd = p;
    while (--p >= url && *p != '@' && *p != ':' && *p != '\\')
        ;
    if (p < url)
        return hostEnd; // jde pouze o adresu serveru
    if (*p != '\\')     // tady nema '\\' co delat, koncime s parsovanim zprava
    {
        BOOL skip = FALSE;
        if (*p == ':') // v URL je oddelovac cisla portu
        {
            hostEnd = p;
            while (--p >= url && *p != '@' && *p != ':' && *p != '\\')
                ;
            if (p < url)
                return hostEnd; // jde pouze o adresu serveru a port
            if (*p == '\\' || *p == ':')
                skip = TRUE; // tady nema '\\' ani ':' co delat, koncime s parsovanim zprava
        }
        if (!skip) // jen pokud nekoncime s parsovanim zprava
        {          // *p je '@', zjistime jestli je to konec hesla nebo jmena uzivatele
            const char* userEnd = p;
            BOOL invalidCharInPasswd = FALSE;
            while (1)
            {
                while (--p >= url && *p != '@' && *p != ':' && *p != '\\')
                    ;
                if (p < url)
                    return userEnd; // jmeno uzivatele bez hesla
                if (*p == '@' || *p == '\\')
                    invalidCharInPasswd = TRUE;
                else
                {
                    if (invalidCharInPasswd)
                        break; // heslo obsahuje '@' nebo '\\', koncime s parsovanim zprava

                    // *p je ':', zjistime jestli jmeno uzivatele neobsahuje nepovoleny znak ':'
                    userEnd = p;
                    while (--p >= url && *p != ':')
                        ;
                    if (p < url)
                        return userEnd; // jmeno uzivatele bez hesla
                    break;              // uzivatelske jmeno obsahuje ':', koncime s parsovanim zprava
                }
            }
        }
    }

    // parsovani URL zleva (bere '\\' i jako oddelovac cesty)
    p = url;
    while (*p != 0 && *p != '@' && *p != ':' && *p != '/' && *p != '\\')
        p++;
    return p;
}

void FTPSplitPath(char* p, char** user, char** password, char** host, char** port, char** path,
                  char* firstCharOfPath, int userLength)
{
    // format cesty: "//user:password@host:port/path" nebo jen "user:password@host:port/path"
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
        p += 2; // preskocime pripadne "//"
    char* beg = p;
    if (userLength > 0 && (int)strlen(p) > userLength &&
        (p[userLength] == '@' || p[userLength] == ':' && strchr(p + userLength + 1, '@') != NULL))
    { // parsujeme username podle jeho predpokladane delky (zavedeno kvuli tomu, ze username muze obsahovat '@', '/' a '\\')
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
        if (passwd) // jen ':' - musime zkusit najit '@' (pokud neni, jde o ':' z "host:port")
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
            *firstCharOfPath = *p; // vracime jestli slo o '/' nebo '\\'
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
        return 0; // bezproblemove jmeno (i anonymni user)
    while (*s != 0)
        s++;
    return (int)(s - user);
}

const char* FTPFindPath(const char* path, int userLength)
{
    // format cesty: "//user:password@host:port/path" nebo jen "user:password@host:port/path"
    const char* p = path;
    if (*p == '/' && *(p + 1) == '/')
        p += 2; // preskocime pripadne "//"
    if (userLength > 0 && (int)strlen(p) > userLength &&
        (p[userLength] == '@' || p[userLength] == ':' && strchr(p + userLength + 1, '@') != NULL))
    { // parsujeme username podle jeho predpokladane delky (zavedeno kvuli tomu, ze username muze obsahovat '@', '/' a '\\')
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
    case ftpsptOpenVMS: // case-insensitive + VMS cesta
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

    case ftpsptMVS: // case-insensitive + MVS cesta
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
    case ftpsptOS2: // case-insensitive + '/' je ekvivalentni s '\\'
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

    case ftpsptIBMz_VM: // case-insensitive + IBM_z/VM cesta
    case ftpsptTandem:  // case-insensitive + Tandem cesta
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

    default: // unix + ostatni
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
    // format cesty: "//user:password@host:port/path"
    if (*p1 == '/' && *(p1 + 1) == '/' && *p2 == '/' && *(p2 + 1) == '/') // musi to byt user-part cesty
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
                memset(passwd1, 0, strlen(passwd1)); // nulovani pameti s heslem
            if (passwd2 != NULL)
                memset(passwd2, 0, strlen(passwd2)); // nulovani pameti s heslem
            if (user1 == NULL && user2 == NULL ||
                user1 != NULL && user2 != NULL && strcmp(user1, user2) == 0 ||
                user1 == NULL && user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0 ||
                user2 == NULL && user1 != NULL && strcmp(user1, FTP_ANONYMOUS) == 0)
            { // shoda uzivatelskych jmen (je case-sensitive - Unixovy konta)
                if (host1 != NULL && host2 != NULL &&
                    SalamanderGeneral->StrICmp(host1, host2) == 0)
                {                                // shoda hostitele (je case-insensitive - Internetove konvence - casem mozna lepsi test IP adres)
                    const char* ftp_port = "21"; // standardni FTP port
                    if (port1 == NULL && port2 == NULL ||
                        port1 != NULL && port2 != NULL && strcmp(port1, port2) == 0 ||
                        port1 == NULL && port2 != NULL && strcmp(port2, ftp_port) == 0 ||
                        port2 == NULL && port1 != NULL && strcmp(port1, ftp_port) == 0)
                    {                                       // shodny port (case-sensitive, melo by byt jen cislo, takze celkem fuk)
                        if (path1 != NULL && path2 != NULL) // cesty bez uvodniho slashe
                        {
                            return FTPIsTheSameServerPath(type, path1, path2);
                        }
                        else // aspon jedna z cest chybi
                        {
                            if (path1 == NULL && path2 == NULL ||
                                path1 == NULL && path2 != NULL && *path2 == 0 ||
                                path2 == NULL && path1 != NULL && *path1 == 0)
                                return TRUE; // dve root cesty (s/bez slashe)
                            else
                            {
                                if (sameIfPath2IsRelative && path2 == NULL)
                                { // 'p2' je relativni + jinak se s 'p1' shoduji (resi
                                    // "ftp://petr@localhost/path" == "ftp://petr@localhost" - potreba pro
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
    // format rootu cesty: "//user:password@host:port"
    if (*p1 == '/' && *(p1 + 1) == '/' && *p2 == '/' && *(p2 + 1) == '/') // musi to byt user-part cesty
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
                memset(passwd1, 0, strlen(passwd1)); // nulovani pameti s heslem
            if (passwd2 != NULL)
                memset(passwd2, 0, strlen(passwd2)); // nulovani pameti s heslem
            if (user1 == NULL && user2 == NULL ||
                user1 != NULL && user2 != NULL && strcmp(user1, user2) == 0 ||
                user1 == NULL && user2 != NULL && strcmp(user2, FTP_ANONYMOUS) == 0 ||
                user2 == NULL && user1 != NULL && strcmp(user1, FTP_ANONYMOUS) == 0)
            { // shoda uzivatelskych jmen (je case-sensitive - Unixovy konta)
                if (host1 != NULL && host2 != NULL &&
                    SalamanderGeneral->StrICmp(host1, host2) == 0)
                {                                // shoda hostitele (je case-insensitive - Internetove konvence - casem mozna lepsi test IP adres)
                    const char* ftp_port = "21"; // standardni FTP port
                    if (port1 == NULL && port2 == NULL ||
                        port1 != NULL && port2 != NULL && strcmp(port1, port2) == 0 ||
                        port1 == NULL && port2 != NULL && strcmp(port2, ftp_port) == 0 ||
                        port2 == NULL && port1 != NULL && strcmp(port1, ftp_port) == 0)
                    {                // shodny port (case-sensitive, melo by byt jen cislo, takze celkem fuk)
                        return TRUE; // rooty jsou shodne
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

// zjisti jestli text 'text' obsahuje retezec 'sub' (velikost pismen nehraje roli)
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
        if (*serverSystem == '2' && replyLen > 4) // FTP_D1_SUCCESS + je sance na string jmena systemu
        {
            const char* sys;
            if (serverSystem[3] == ' ')
                sys = serverSystem + 4; // jednoradkova odpoved
            else                        // viceradkova odpoved, musime najit posledni radek
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
                    sys = serverSystem + 4; // neocekavana
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
                    if (IsKnownOSName(nextSysBeg, nextSys)) // nasli jsme jmeno OS az v nekterem z dalsich slov (chyba prekladu, napr. "215 Betriebssystem OS/2")
                    {
                        sysBeg = nextSysBeg;
                        sys = nextSys;
                        break;
                    }
                }
            }

            int len = (int)(sys - sysBeg);
            if (len > 200)
                len = 200; // orez na 200 znaku
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
                    openBracket++; // bereme jen na zacatku cesty nebo pred ':'
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
                const char* name = s + 1; // zkusime preskocit jmeno souboru (napr. "DKA0:[MYDIR.SUBDIR]MYFILE.TXT;1")
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
                    closeBracket++; // bereme jen ']' na konci cesty
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
        if (HaveSubstring(sysName, "Windows")) // zname jmeno systemu + jde o Windows (zatim vime jen o NT)
            return ftpsptWindows;
        else
        {
            if (HaveSubstring(sysName, "NETWARE") || // zname jmeno systemu + jde o netware
                serverFirstReply != NULL && HaveSubstring(serverFirstReply, " NW 3") &&
                    HaveSubstring(serverFirstReply, " HellSoft")) // zname prvni odpoved serveru a jde o Hellsoft server na Netwaru
            {
                return ftpsptNetware;
            }
            else
            {
                if (HaveSubstring(sysName, "OS/400"))
                    return ftpsptAS400; // kdyby nahodou vratili jako prvni cestu uz cestu se slashem na zacatku (podle logu zatim vraci napr. "QGPL" ())
                else
                    return ftpsptUnix; // unix je pravdepodobnejsi nez netware
            }
        }
    }
    if (backslashAtBeg)
    {
        if (HaveSubstring(sysName, "NETWARE") || // zname jmeno systemu + jde o netware
            serverFirstReply != NULL && HaveSubstring(serverFirstReply, " NW 3") &&
                HaveSubstring(serverFirstReply, " HellSoft")) // zname prvni odpoved serveru a jde o Hellsoft server na Netwaru
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
                return ftpsptWindows; // windows jsou pravdepodobnejsi nez netware
        }
    }
    if (charOnFirstPos && colonOnSecondPos && colon == 1 && // cesty typu "C:"
        (pathLen == 2 || slash > 0 || backslash > 0))       // cesty typu "C:/..." nebo "C:\\..."
    {
        return ftpsptOS2;
    }
    if (openBracket - vmsEscapedOpenBracket == 1 &&
        closeBracket - vmsEscapedCloseBracket == 1 &&
        bracket - vmsEscapedBracket == 0 &&                   // jen oteviraci a zaviraci zavorka
        apostrophAtBeg + apostrophAtEnd == 0 &&               // zadny apostrofy na zacatku a konci
        slashAtBeg + slash + backslashAtBeg + backslash == 0) // zadny slashe ani backslashe
    {
        return ftpsptOpenVMS;
    }
    if (apostrophAtBeg && apostrophAtEnd && apostroph == 0) // cesta uzavrena v apostrofech
    {
        return ftpsptMVS;
    }
    if (slash == 0 && backslash == 0 && colon == 1 && periodsAfterColon > 0 &&
        periodsBeforeColon == 0 && spaces == 0)
    {
        return ftpsptIBMz_VM;
    }

    if (path[0] == 0) // u prazdnych cest zkusime detekci podle jmena systemu,
    {                 // resi detekci "salamanderovskeho" obecneho rootu ("ftp://server/" = root
                      // at je "server" jakykoliv system)
        if (HaveSubstring(sysName, "UNIX"))
            return ftpsptUnix;
        if (HaveSubstring(sysName, "Windows"))
            return ftpsptWindows;
        if (HaveSubstring(sysName, "NETWARE") ||
            serverFirstReply != NULL && HaveSubstring(serverFirstReply, " NW 3") &&
                HaveSubstring(serverFirstReply, " HellSoft")) // zname prvni odpoved serveru a jde o Hellsoft server na Netwaru
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
        return ftpsptAS400; // prvni cesta, kterou AS/400 vraci je napr. "QGPL" (odpoved na prvni PWD: 257 "QGPL" is current library.)
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
        { // nahradime znak "%" jeho hex-esc-sekvenci "%25"
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

    // POZOR: pokud dojde ke zmenam formatu UNIXovych prav, je nutne predelat i IsUNIXLink()

    if (rights != NULL && strlen(rights) == 10) // musi to byt deset pismen, jinak to nemuzou byt UNIXova prava (prava s ACL maji jedenact znaku (napr. "drwxrwxr-x+"), ty ale neumime menit, takze radsi delame, ze je nezname)
    {
        BOOL ok = TRUE;
        *actAttr = 0;
        // -rwxrwxrwx
        // 0.znak ignorujeme (zname pismenka: d,l,c,b,p)
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
        // 3./6./9.: rozlisujeme jen x,- a nastavujeme attrDiff je-li jine (zname pismenka: s,S,X,t,T)
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

    // POZOR: pokud dojde ke zmenam formatu UNIXovych prav, je nutne predelat i GetAttrsFromUNIXRights()

    int len = (rights != NULL ? (int)strlen(rights) : 0);
    return ((len == 10 || len == 11 && rights[10] == '+') && // musi to byt deset pismen, jinak to nemuzou byt UNIXova prava; vyjimka: s ACL je to jedenact znaku: napr. "drwxrwxr-x+"
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
    FTPGetErrorText(err, errBuf, bufSize - 2); // (bufSize-2) aby zbylo na nase CRLF
    char* s = errBuf + strlen(errBuf);
    while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
        s--;
    strcpy(s, "\r\n"); // dosazeni naseho CRLF na konec radky z textem chyby
}

BOOL FTPReadFTPReply(char* readBytes, int readBytesCount, int readBytesOffset,
                     char** reply, int* replySize, int* replyCode)
{
    BOOL ret = FALSE;
    if (readBytesOffset < readBytesCount) // je-li vubec neco nactene
    {
        char* s = readBytes + readBytesOffset;
        char* end = readBytes + readBytesCount;
        int ftpRplCode = 0; // kod FTP odpovedi, -1 = chyba (neni FTP odpoved)
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
                if (*s == '-') // multi-line, preskocime vse az do mezery za kodem odpovedi na posl. radku
                {
                    s++;
                    while (s < end)
                    {
                        do // najdeme konec radky (CRLF + proti specifikaci i LF)
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

                        if (s < end) // byl nalezen konec radky
                        {
                            s++; // preskok LF
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
                            // test na konec multi-line textu - musi byt stejny reply-code + nasledovany space
                            if (j == 3 && code == ftpRplCode && s < end && *s == ' ')
                                break;
                        }
                    }
                }

                if (s < end && *s == ' ') // za kodem odpovedi musi byt podle specifikace mezera
                {
                    while (++s < end) // najdeme konec radky (CRLF + proti specifikaci i LF)
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

                    if (s < end) // nasli jsme konec radky - mame celou FTP odpoved
                    {
                        s++; // preskok LF
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

        if (s < end && !syntaxOK) // neocekavana syntaxe, vratime radek ukonceny CRLF nebo jen LF
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
                    break; // nejspis vubec neni FTP server, balime to
                s++;
            }
            if (s < end) // nasli jsme cely radek (nebo velikost radku presahla mez)
            {
                ret = TRUE;
                *reply = readBytes + readBytesOffset;
                *replySize = (int)(s - (readBytes + readBytesOffset) + 1); // +1 kvuli zahrnuti znaku *s
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
    // VxWorks FTP server vraci: 257 Current directory is "mars:"
    // nemecky AIX vraci pro a'g'f: 257 '/projects/acaix3/iplus/lnttmp/a'g'f' ist das aktuelle Verzeichnis.
    // nemecky AIX vraci pro a"d: 257 '/projects/acaix3/iplus/lnttmp/a"d' ist das aktuelle Verzeichnis.

    // reseni VxWorks: hledame prvni '"'
    // reseni nemeckyho AIX: detekujeme '\'' drive nez '"', cesta saha az po posledni '\'', za posledni '\'' uz neni '"'

    while (s < end && *s != '"' && *s != '\'')
        s++;
    if (*s == '\'') // nemecky AIX
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
    if (s < end && *s == '"') // musi to zacinat na '"'
    {
        char* d = dirBuf;
        char* endDir = dirBuf + dirBufSize - 1; // musime si nechat misto pro nulu na konci retezce
        while (++s < end && d < endDir)
        {
            if (*s == '"')
            {
                if (s + 1 < end && *(s + 1) == '"')
                    s++; // '""' = '"' (escape sequence)
                else
                {
                    ok = TRUE; // konec jmena adresare
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
    while (s < end) // hledame sekvenci sesti cisel oddelenych ',' a white-spaces
    {
        if (*s >= '0' && *s <= '9') // nadeje na zacatek sekvence
        {
            int i;
            for (i = 0; i < 6; i++)
            {
                // cteni jednoho cisla
                int n = 0;
                while (s < end && *s >= '0' && *s <= '9')
                {
                    n = n * 10 + (*s - '0');
                    s++;
                }

                if (n < 256) // jde-li o bytovou hodnotu, priradime ji do prislusne promenne
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
                    break; // prilis velke cislo, nemuze jit o hledanou sestici

                if (i == 5) // konec hledani, uspech
                {
                    *ip = (h4 << 24) + (h3 << 16) + (h2 << 8) + h1;
                    *port = (p1 << 8) + p2;
                    return TRUE;
                }

                BOOL delim = FALSE; // povolime jen jednu carku
                while (s < end)
                {
                    if (*s > ' ' && (delim || *s != ','))
                        break;
                    if (*s == ',')
                        delim = TRUE;
                    s++;
                }
                if (s >= end || *s != ',' && (*s < '0' || *s > '9'))
                    break; // neocekavany format
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

        // mame celkovou velikost listingu - 'num'
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
        dirNameLen--; // tecku na konci jmena ignorujeme
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
    return (((checkedChar - t) - 1) & 1) != 0; // lichy pocet '^' pred znakem = escapovany znak
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
            return FALSE; // invalid path nebo mene nez dve komponenty (jen dve tecky, jedna z nich na konci)
        willBeRoot = TRUE;
    }
    if (*(path + l - 1) == '.')
    {
        *(path + --l) = 0; // zruseni '.' na konci
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
        ;                      // preskocime cislo verze
    if (s > name && *s == ';') // pokud se podarilo oriznout cislo verze (pred cislem je ';') a zbyl aspon jeden znak jmena
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
        return *path != '\\' && *path != '/' && *path != 0 && *(path + 1) != ':'; // absolutni cesty: C:/dir1, /dir1

    case ftpsptOpenVMS: // relativni cesty: aaa, [.aaa], aaa.bbb, [.aaa.bbb]
    {                   // absolutni cesty: PUB$DEVICE:[PUB], [PUB.VMS]
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
        if (*s != 0) // "aaa/bbb" nebo "aaa\\bbb"
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

    case ftpsptOpenVMS: // relativni cesty: aaa, [.aaa], aaa.bbb, [.aaa.bbb] + u souboru (sem by nemela prijit): [.pub]file.txt;1
    {                   // absolutni cesty: PUB$DEVICE:[PUB], [PUB.VMS]
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
                    memmove(path, s + 1, strlen(s + 1) + 1); // chybne rozdeli "[.a]b.c;1" na "a", "b" a "c;1", ale maji sem lezt jen adresare, takze to neresime
                else
                    path[0] = 0; // to byl konec cesty
            }
            else
                return FALSE; // neni relativni cesta
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
                return FALSE; // workPath neni plna absolutni cesta
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
                return FALSE; // workPath neni plna absolutni cesta
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
        // case ftpsptIBMz_VM, ftpsptMVS, ftpsptOpenVMS: nic nedelame

    case ftpsptUnix:
    case ftpsptAS400:
        backslash = FALSE; // tady break nechybi!
    case ftpsptNetware:
    case ftpsptWindows:
    case ftpsptOS2:
    {
        char backslashSep = backslash ? '\\' : '/'; // backslash==FALSE -> vsude se bude dvakrat testovat slash (misto slashe i backslashe)
        char* afterRoot = path + (*path == '/' || *path == backslashSep ? 1 : 0);
        if (pathType == ftpsptOS2 && afterRoot == path && *path != 0 && *(path + 1) == ':')
            afterRoot = path + 2 + (*(path + 2) == '/' || *(path + 2) == backslashSep ? 1 : 0);

        char* d = afterRoot; // ukazatel za root cestu
        while (*d != 0)
        {
            while (*d != 0 && *d != '.')
                d++;
            if (*d == '.')
            {
                if (d == afterRoot || d > afterRoot && (*(d - 1) == '/' || *(d - 1) == backslashSep)) // '.' za root cestou nebo "/." ("\\.")
                {
                    if (*(d + 1) == '.' && (*(d + 2) == '/' || *(d + 2) == backslashSep || *(d + 2) == 0)) // ".."
                    {
                        char* l = d - 1;
                        while (l > afterRoot && *(l - 1) != '/' && *(l - 1) != backslashSep)
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
           (_strnicmp(listErrReply + 4, "file not found", 14) == 0 ||                  // VMS (cs.felk.cvut.cz) hlasi u prazdneho adresare (nelze povazovat za chybu)
            _strnicmp(listErrReply + 4, "The specified directory is empty", 32) == 0); // Z/VM (vm.marist.edu) hlasi u prazdneho adresare (nelze povazovat za chybu)
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
                            return FALSE; // dve tecky ve jmene souboru nejsou povolene
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
                        return FALSE; // dve tecky ve jmene souboru nejsou povolene
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

    case ftpsptAS400: // povoleny jeden slash ("a.file/a.mbr")
    {
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        {
            const char* slash = strchr(name, '/');
            if (slash != NULL &&
                (isDir || !FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", path) || strchr(slash + 1, '/') != NULL))
            {
                return FALSE; // jeden slash pro jmeno adresare nebo mimo cestu /qsys.lib nebo vic nez jeden slash na ceste /qsys.lib -> chyba
            }
            return TRUE;
        }
        else
            return FALSE;
    }

    default:
    {
        TRACE_E("FTPIsValidName(): unexpected path type!");
        return TRUE; // nevime co je to za cestu, takze OK (jestli neni OK, ohlasi to pozdeji server, maximalne vytvori vic podadresaru, atp., smula)
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

    // overime jestli jmeno neni prilis dlouhe
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
                        s = num; // verze souboru je OK
                    else
                        return FALSE; // invalidni verze souboru
                }
                else
                    return FALSE; // invalidni znak ve jmene
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
        // zde break nechybi!
    case ftpsptUnix:
    {
        if (*phase == 0) // v teto fazi predpokladame klasicky Unix (jmena max. 255 znaku, neobsahuji '/' a '\0')
        {
            const char* s = originalName;
            char* n = newName;
            char* dot = NULL;    // jen soubory: prvni tecka zprava, ktera ovsem neni na zacatku jmena
            char* end = n + 255; // MAX_PATH==260, takze buffer 'newName' je velky dost
            BOOL containsSlash = FALSE;
            while (*s != 0 && n < end)
            {
                if (*s != '/')
                {
                    if (*s == '.' && !isDir && s != originalName)
                        dot = n; // POZOR: vyjimka, nejde o Windows: ".cvspass" na UNIXu neni pripona
                    *n++ = *s++;
                }
                else
                {
                    *n++ = '_'; // znak '/' nahradime znakem '_'
                    s++;
                    containsSlash = TRUE;
                }
            }
            *n = 0;
            if (*index == 0 && *s == 0 && !containsSlash) // newName je shodne s originalName, musime newName upravit
                *index = 1;                               // resi i rezervovana jmena "." a ".."
            if (*index != 0)                              // pridame za jmeno " (cislo)"
            {
                if (alreadyRenamedFile) // zajistime "name (2)"->"name (3)" misto ->"name (2) (2)"
                {
                    char* s2 = dot == NULL ? n : dot;
                    if (s2 > newName)
                        s2--;
                    if (*s2 == ')') // hledame pozadu " (cislo)"
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
                    if (dot != NULL && dot - newName > cut) // zkraceni ve jmene
                    {
                        memmove(dot - cut, dot, (n - dot) + 1);
                        dot -= cut;
                        n -= cut;
                    }
                    else // proste orizneme konec
                    {
                        n = newName + 255 - suffixLen;
                        if (dot == n - 1)
                            n--; // pokud by mela na konci nazvu zustat '.', orizneme ji take
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
        else // v teto fazi budou jmena validni na Windows (pro FTP servery na Windows, tvarici se jako Unix) + ftpsptNetware + ftpsptWindows + ftpsptOS2
        {
            lstrcpyn(newName, originalName, MAX_PATH);
            SalamanderGeneral->SalMakeValidFileNameComponent(newName);
            if (*index == 0 && strcmp(newName, originalName) == 0)
                *index = 1;  // newName je shodne s originalName, musime newName upravit
            if (*index != 0) // pridame za jmeno " (cislo)"
            {
                char* n = newName + strlen(newName);
                char* dot = isDir ? NULL : strrchr(newName + 1, '.'); // jen soubory: prvni tecka zprava, ktera ovsem neni na zacatku jmena; POZOR: vyjimka, kvuli phase==0, kdy nejde o Windows: ".cvspass" na UNIXu neni pripona
                if (alreadyRenamedFile)                               // zajistime "name (2)"->"name (3)" misto ->"name (2) (2)"
                {
                    char* s = dot == NULL ? n : dot;
                    if (s > newName)
                        s--;
                    if (*s == ')') // hledame pozadu " (cislo)"
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
                    if (dot != NULL && dot - newName > cut) // zkraceni ve jmene
                    {
                        memmove(dot - cut, dot, (n - dot) + 1);
                        dot -= cut;
                        n -= cut;
                    }
                    else // proste orizneme konec
                    {
                        n = newName + MAX_PATH - 4 - suffixLen;
                        if (dot == n - 1)
                            n--; // pokud by mela na konci nazvu zustat '.', orizneme ji take
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
                SalamanderGeneral->SalMakeValidFileNameComponent(newName); // mohlo vzniknout invalidni jmeno, pripadne ho nechame opravit
            }
            (*index)++;
            *phase = -1; // zadna dalsi faze generovani jmen neni
        }
        break;
    }

    case ftpsptOpenVMS:
    {
        if (*phase == 0) // vyhodime tutove zakazane znaky + pripadne pridame "_cislo" ke jmenu
        {
            const char* s = originalName;
            const char* ext = !isDir ? strrchr(originalName, '.') : NULL;
            const char* ver = !isDir ? strrchr(originalName, ';') : NULL;
            if (ver != NULL && ver < ext)
                ver = NULL;
            char* n = newName;
            char* dot = NULL;     // prvni tecka (pripona)
            char* semicol = NULL; // ';' oddelujici verzi souboru
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
                                *n++ = *s++; // cislo je OK (velikost nekontrolujeme, to nechame na VMS)
                            }
                            else
                            {
                                *n++ = '_'; // zakazany znak nahradime znakem '_'
                                s++;
                                changed = TRUE;
                            }
                        }
                        else
                        {
                            *n++ = '_'; // zakazany znak nahradime znakem '_'
                            s++;
                            changed = TRUE;
                        }
                    }
                }
            }
            *n = 0;
            if (*index == 0 && *s == 0 && !changed) // newName je shodne s originalName, musime newName upravit
                *index = 1;                         // resi i rezervovane jmeno "." (mimochodem: ".." -> "__" nebo "_.")
            if (*index != 0)                        // pridame za jmeno "_cislo"
            {
                if (alreadyRenamedFile) // zajistime "name_2"->"name_3" misto ->"name_2_2"
                {
                    char* s2 = dot == NULL ? (semicol == NULL ? n : semicol) : dot;
                    if (s2 > newName)
                    {
                        s2--;
                        if (*s2 >= '0' && *s2 <= '9') // hledame pozadu "_cislo"
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
                    if (dot != NULL && dot - newName > cut) // zkraceni ve jmene
                    {
                        memmove(dot - cut, dot, (n - dot) + 1);
                        dot -= cut;
                        if (semicol != NULL)
                            semicol -= cut;
                        n -= cut;
                    }
                    else
                    {
                        if (semicol != NULL && semicol - newName > cut) // zkraceni v pripone
                        {
                            memmove(semicol - cut, semicol, (n - semicol) + 1);
                            if (dot != NULL && dot >= semicol - cut)
                                dot = NULL;
                            semicol -= cut;
                            n -= cut;
                        }
                        else // proste orizneme konec
                        {
                            n = newName + MAX_PATH - 4 - suffixLen;
                            if (semicol == n - 1)
                                n--; // pokud by mel na konci nazvu zustat ';', orizneme ho take
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
        else // vyhodime vsechny mozne zakazane znaky + orizneme delku na MAX_VMS_COMP_LEN + pripadne pridame "_cislo" ke jmenu
        {
            const char* s = originalName;
            const char* ext = !isDir ? strrchr(originalName, '.') : NULL;
            const char* ver = !isDir ? strrchr(originalName, ';') : NULL;
            if (ver != NULL && ver < ext)
                ver = NULL;
            // provedeme kopii jmena do 'newName' s tim, ze vyhodime vsechny mozne zakazane znaky
            char* n = newName;
            char* dot = NULL;     // prvni tecka (pripona)
            char* semicol = NULL; // prvni strednik (verze souboru)
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
                                *n++ = *s++; // cislo je OK
                            }
                            else // invalidni verze souboru - ';' je zde jen zakazany znak
                            {
                                *n++ = '_';
                                s++;
                                changed = TRUE;
                            }
                        }
                        else
                        {
                            *n++ = '_'; // zakazany znak nahradime znakem '_'
                            s++;
                            changed = TRUE;
                        }
                    }
                }
            }
            *n = 0;
            // zkratime jmeno tak, aby komponenty mely max. MAX_VMS_COMP_LEN znaku
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
            if (*index == 0 && *s == 0 && !changed) // newName je shodne s originalName, musime newName upravit
                *index = 1;                         // resi i rezervovane jmeno "." (mimochodem: ".." -> "__" nebo "_.")
            if (*index != 0)                        // pridame za jmeno "_cislo"
            {
                if (alreadyRenamedFile) // zajistime "name_2"->"name_3" misto ->"name_2_2"
                {
                    char* s2 = dot == NULL ? (semicol == NULL ? n : semicol) : dot;
                    if (s2 > newName)
                    {
                        s2--;
                        if (*s2 >= '0' && *s2 <= '9') // hledame pozadu "_cislo"
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

    case ftpsptAS400: // AS/400: zatim nevime nic o formatu jmen na AS/400, prozatim se svezeme s nasledujicim kodem pro Tandem, vypada dost restriktivne na to, aby se s nim AS/400 take spokojilo
    case ftpsptTandem:
    {
        // plna cesta k souboru: \SYSTEM.$VVVVV.SUBVOLUM.FILENAME
        // Tandem ma ruzna pravidla pro jmeno systemu/masiny, volumu, subvolumu a jmena:
        // System: zacina backslashem, prvni znak je alfa, dalsi alfanumericke, max. delka 7 (vcetne backslashe)
        // Volume: zacina $, prvni znak je alfa, dalsi alfanumericke, max. delka 6 (vcetne $)
        // Subvolume + Jmeno: prvni znak je alfa, dalsi alfanumericke, max. delka 8 znaku
        //
        // Tady ma smysl resit asi jen jmena souboru, protoze volumy ani subvolumy se
        // nejspis pres MKD nevytvareji (jsou jen soucasti plneho jmena souboru).

        // prozatim vse prevedu na UPPER-CASE, predsadim aplha znak ('A') pokud na nej jmeno
        // nezacina, ze zbytku jmena odstranim nealphanumericke znaky a pripadne pridam "cislo"
        // na konec - max. delka jmena je 8 znaku
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
        if (*index == 0 && *s == 0 && !change) // newName je shodne s originalName, musime newName upravit
            *index = 1;
        if (*index != 0) // pridame za jmeno "cislo"
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
        // Vse co se mi podarilo zjistit o MVS naming conventions:
        // -kazda cast jmena (mezi teckama):
        //   -musi zacinat pismenem nebo '#'
        //   -obsahuje A-Z, 0-9, '#', '%', '§'
        // Implementaci odkladam, jestli to vubec nekdo nekdy upotrebi, mam podezreni ze ani nahodou. ;-)

        // prozatim odstranim '\'' a pripadne pridam "#cislo" na konec
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
                *n++ = '#'; // znak '\'' nahradime znakem '#' ('_' zrejme MVS nepodporuje)
                s++;
                change = TRUE;
            }
        }
        *n = 0;
        if (*index == 0 && *s == 0 && !change) // newName je shodne s originalName, musime newName upravit
            *index = 1;                        // resi i rezervovana jmena "." a ".."
        if (*index != 0)                       // pridame za jmeno "#cislo"
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
        // Vse co se mi podarilo zjistit o z/VM naming conventions:
        // -soubor muze mit maximalne 8.8 znaku, vic tecek neni moznych,
        //  jedna tecka je povina (jinak doplni ".$DEFAULT")
        // -adresar muze mit maximalne 16 znaku (tecky jsou oddelovace adresaru, takze ve jmene adresare nejsou pripustne)
        // Implementaci odkladam, jestli to vubec nekdo nekdy upotrebi, mam podezreni ze ani nahodou. ;-)

        // prozatim odstranime '.' (jedna '.' u souboru povolena - ta prvni zprava) a ':' a pripadne
        // pridame "_cislo" na konec
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
                    *n++ = '_'; // nepripustne znaky nahradime znakem '_'
                    s++;
                    change = TRUE;
                }
            }
        }
        *n = 0;
        if (*index == 0 && *s == 0 && !change) // newName je shodne s originalName, musime newName upravit
            *index = 1;                        // resi i rezervovane jmeno "." (mimochodem: ".." -> "_.")
        if (*index != 0)                       // pridame za jmeno "_cislo"
        {
            if (alreadyRenamedFile) // zajistime "name_2"->"name_3" misto ->"name_2_2"
            {
                char* s2 = dot == NULL ? n : dot;
                if (s2 > newName)
                {
                    s2--;
                    if (*s2 >= '0' && *s2 <= '9') // hledame pozadu "_cislo"
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
                if (dot != NULL && dot - newName > cut) // zkraceni ve jmene
                {
                    memmove(dot - cut, dot, (n - dot) + 1);
                    dot -= cut;
                    n -= cut;
                }
                else // proste orizneme konec
                {
                    n = newName + MAX_PATH - 4 - suffixLen;
                    if (dot == n - 1)
                        n--; // pokud by mela na konci nazvu zustat '.', orizneme ji take
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
                _strnicmp(fileBeg, mbrBeg, fileEnd - fileBeg) == 0) // pokud se shoduji jmena pred ".file" a pred ".mbr", je to hledany pripad, bude se zkracovat
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