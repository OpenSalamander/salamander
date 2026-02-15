// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
//*****************************************************************************
// Functions for wildcard operations
//

void PrepareMask(char* mask, const char* src)
{
    SLOW_CALL_STACK_MESSAGE2("PrepareMask(, %s)", src);
    char* begMask = mask;
    char lastChar = 0;
    // remove spaces at the beginning of the mask
    while (*src == ' ')
        src++;
    while (*src != 0)
    {
        if (*src == '*' && lastChar == '*')
            src++;                               // "**" -> "*"
        else if (*src == '?' && lastChar == '*') // "*?" -> "?*"
        {
            *(mask - 1) = '?';
            *mask++ = '*';
            src++;
        }
        else
            *mask++ = (lastChar = *src++);
    }
    // trim spaces at the end of the mask
    while (mask > begMask && *(mask - 1) == ' ')
        mask--;
    *mask = 0;
}

BOOL AgreeMask(const char* filename, const char* mask, BOOL hasExtension, BOOL extendedMode)
{
    CALL_STACK_MESSAGE_NONE;
    //  CALL_STACK_MESSAGE4("AgreeMask(%s, %s, %d)", filename, mask, hasExtension);  // slows things down massively (called recursively)
    while (*filename != 0)
    {
        if (*mask == 0)
            return FALSE; // mask is too short
        BOOL agree;
        if (extendedMode)
            agree = (LowerCase[*filename] == LowerCase[*mask] || *mask == '?' || // match or '?' represents any character or '#' represents any digit
                     (*mask == '#' && *filename >= '0' && *filename <= '9'));
        else
            agree = (LowerCase[*filename] == LowerCase[*mask] || *mask == '?'); // match or '?' represents any character
        if (agree)
        {
            filename++;
            mask++;
        }
        else if (*mask == '*') // '*' represents a sequence of characters (possibly empty)
        {
            mask++;
            while (*filename != 0)
            {
                if (AgreeMask(filename, mask, hasExtension, extendedMode))
                    return TRUE; // the rest of the mask matches
                filename++;
            }
            break; // end of filename...
        }
        else
            return FALSE;
    }
    if (*mask == '*')
        mask++;                        // asterisk '*' afterwards -> represents "" -> everything is ok
    if (!hasExtension && *mask == '.') // without extension mask "*.*" must still match...
        return *(mask + 1) == 0 || (*(mask + 1) == '*' && *(mask + 2) == 0);
    else
        return *mask == 0;
}

char* MaskName(char* buffer, int bufSize, const char* name, const char* mask)
{
    SLOW_CALL_STACK_MESSAGE1("MaskName()");
    if (buffer == NULL || bufSize <= 0 || name == NULL)
        return NULL;
    if (mask == NULL)
    {
        lstrcpyn(buffer, name, bufSize);
        return buffer;
    }

    // the first dot in the mask separates two operational parts: the first for the name,
    // the second for the extension (example: "a.b.c.d" + "*.*.old": "a.b.c" + "*" = "a.b.c";
    // "d" + "*.old" = "d.old" -> the result is the combination "a.b.c.d.old")

    int ignPoints = 0; // how many dots the name contains (this section corresponds to the text of the mask from the start to the first dot); the rest matches the extension (after the fisrt dot)
    const char* n = name;
    while (*n != 0)
        if (*n++ == '.')
            ignPoints++;
    const char* s = mask;
    while (*s != 0)
        if (*s++ == '.')
        {
            ignPoints--;
            break;
        } // in the mask, only one dot is meaningful (name.ext); the part after the second dot will be expanded at the end of the name
    //  while (*s != 0) if (*s++ == '.') {ignPoints--;}  // in this variant, sections in the name and mask between dots are matched from the end ("a.b.c.d" + "*.*.old": "a.b" + "*"; "c" + "*"; "d" + "old" -> "a.b.c.old")
    if (ignPoints < 0)
        ignPoints = 0;
    //  if (ignPoints == 0 && *name == '.') ignPoints++;   // dot at the start of the name should be ignored (not an extension); fix: ".cvspass" is an extension in Windows...

    n = name;
    char* d = buffer;
    char* endBuf = buffer + bufSize - 1;
    s = mask;
    while (*s != 0 && d < endBuf)
    {
        switch (*s)
        {
        case '*': // copy the rest of the name section (up to the "ignPoints+1"-th dot)
        {
            while (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                        ignPoints--; // this dot is part of the section, continue
                    else
                        break;
                }
                *d++ = *n++;
                if (d >= endBuf)
                    break;
            }
            break;
        }

        case '?': // copy one character unless it is the end of the name section (up to the "ignPoints+1"-th dot)
        {
            if (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                    {
                        ignPoints--; // this dot is part of the section, continue
                        *d++ = *n++;
                    }
                }
                else
                    *d++ = *n++;
            }
            break;
        }

        case '.': // end of the current name section (jump to the next section in the name)
        {
            *d++ = '.';
            while (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                        ignPoints--; // this dot is part of the section, continue
                    else
                        break;
                }
                n++;
            }
            if (*n == '.')
                n++;
            break;
        }

        default:
        {
            *d++ = *s; // regular character - just copy it
            if (*n != 0)
            {
                if (*n != '.')
                    n++; // if it isn't a '.', skip one character from the name
                else
                {
                    if (ignPoints > 0)
                    {
                        ignPoints--;
                        n++; // if the dot is part of the section, skip it as well
                    }
                }
            }
            break;
        }
        }
        s++;
    }
    while (--d >= buffer && *d == '.')
        ; // the result must be trimmed of trailing '.'
    *++d = 0;
    return buffer;
}

//
//*****************************************************************************
// Functions for quick-search
// the '/' character represents any number of characters (like '*' in a standard mask)

// returns TRUE if it is a wildcard character replacing '*'
// it must be a character not allowed in file names
// and at the same time, it should be easy to type (see the '<' on the German keyboard;
// for the backslash on the German keyboard you must press AltGr+\)
BOOL IsQSWildChar(char ch)
{
    return (ch == '/' || ch == '\\' || ch == '<');
}

void PrepareQSMask(char* mask, const char* src)
{
    CALL_STACK_MESSAGE2("PrepareQSMask(, %s)", src);
    char* begMask = mask;
    char lastChar = 0;
    while (*src != 0)
    {
        if (IsQSWildChar(*src))
        {
            if (lastChar == '/')
                src++;
            else
            {
                // convert other wild characters to '/'
                src++;
                *mask++ = (lastChar = '/');
            }
        }
        else
            *mask++ = (lastChar = *src++);
    }
    // trim '/' at the end of the mask (it has no meaning here)
    if (mask > begMask && *(mask - 1) == '/')
        mask--;
    *mask = 0;
}

BOOL AgreeQSMaskAux(const char* filename, BOOL hasExtension, const char* filenameBase, const char* mask, BOOL wholeString, int& offset)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE6("AgreeQSMaskAux(%s, %d, %s, %s, %d,)", filename, hasExtension, filenameBase, mask, wholeString);
    while (*filename != 0)
    {
        if (!wholeString && *mask == 0)
        {
            offset = (int)(filename - filenameBase);
            return TRUE; // end of mask, 'offset' = how far it reaches into the file name
        }
        if (LowerCase[*filename] == LowerCase[*mask])
        {
            filename++;
            mask++;
        }
        else if (*mask == '/') // '/' stands for a sequence of characters (can be empty)
        {
            mask++;
            while (*filename != 0)
            {
                if (AgreeQSMaskAux(filename, hasExtension, filenameBase, mask, wholeString, offset))
                    return TRUE; // the rest of the mask matches
                filename++;
            }
            break; // end of filename...
        }
        else
            return FALSE;
    }
    if (*mask == 0 ||
        !hasExtension && *mask == '.' && *(mask + 1) == 0) // a dot at the end of the mask is tolerated for names without an extension ('/' at the end is trimmed, not handled)
    {
        offset = (int)(filename - filenameBase);
        return TRUE; // mask matched the entire name -> 'offset' = length of the file name
    }
    else
        return FALSE;
}

BOOL AgreeQSMask(const char* filename, BOOL hasExtension, const char* mask, BOOL wholeString, int& offset)
{
    SLOW_CALL_STACK_MESSAGE5("AgreeQSMask(%s, %d, %s, %d,)", filename, hasExtension, mask, wholeString);
    offset = 0;
    return AgreeQSMaskAux(filename, hasExtension, filename, mask, wholeString, offset);
}

//*****************************************************************************
//
// CMaskGroup
//

CMaskGroup::CMaskGroup()
    : PreparedMasks(10, 10)
{
    MasksString[0] = 0;
    NeedPrepare = FALSE;
    ExtendedMode = FALSE;
    MasksHashArray = NULL;
    MasksHashArraySize = 0;
}

CMaskGroup::CMaskGroup(const char* masks, BOOL extendedMode)
    : PreparedMasks(10, 10)
{
    MasksHashArray = NULL;
    MasksHashArraySize = 0;
    SetMasksString(masks, extendedMode);
}

CMaskGroup::~CMaskGroup()
{
    Release();
}

void CMaskGroup::Release()
{
    int i;
    for (i = 0; i < PreparedMasks.Count; i++)
    {
        if (PreparedMasks[i] != NULL)
        {
            free(PreparedMasks[i]);
            PreparedMasks[i] = NULL;
        }
    }
    PreparedMasks.DestroyMembers();
    ReleaseMasksHashArray();
}

CMaskGroup&
CMaskGroup::operator=(const CMaskGroup& s)
{
    Release();

    lstrcpy(MasksString, s.MasksString);
    ExtendedMode = s.ExtendedMode;

    NeedPrepare = TRUE;
    if (!s.NeedPrepare)
    {
        int errpos = 0;
        if (!PrepareMasks(errpos)) // an error shouldn't occur, the source mask is valid
            TRACE_E("CMaskGroup::operator= Internal error, PrepareMasks() failed.");
    }
    return *this;
}

void CMaskGroup::ReleaseMasksHashArray()
{
    if (MasksHashArray != NULL)
    {
        int i;
        for (i = 0; i < MasksHashArraySize; i++)
        {
            if (MasksHashArray[i].Mask != NULL) // if this array element is not empty
            {
                free(MasksHashArray[i].Mask);
                CMasksHashEntry* next = MasksHashArray[i].Next;
                while (next != NULL) // if there are multiple masks at the same entry (hash)
                {
                    CMasksHashEntry* nextNext = next->Next;
                    if (next->Mask != NULL)
                        free(next->Mask);
                    free(next);
                    next = nextNext;
                }
            }
        }
        free(MasksHashArray); // so destructors of objects in the array aren't called
        MasksHashArray = NULL;
        MasksHashArraySize = 0;
    }
}

void CMaskGroup::SetMasksString(const char* masks, BOOL extendedMode)
{
    int l = (int)strlen(masks);
    if (l > MAX_GROUPMASK - 1)
    {
        l = MAX_GROUPMASK - 1;
        TRACE_E("Group mask string is longer than MAX_GROUPMASK, using only first MAX_GROUPMASK-1 characters...");
    }
    // originally memcpy was used, but in some calls, the masks pointer referenced
    // MasksString itself, causing overlap
    memmove(MasksString, masks, l);

    MasksString[l] = 0;

    NeedPrepare = TRUE;
    ExtendedMode = extendedMode;
}

const char*
CMaskGroup::GetMasksString()
{
    return MasksString;
}

char* CMaskGroup::GetWritableMasksString()
{
    NeedPrepare = TRUE; // unfortunately also used as a write buffer, so we have to allow another PrepareMasks
    return MasksString;
}

BOOL CMaskGroup::GetExtendedMode()
{
    return ExtendedMode;
}

#define COMPUTEMASKGROUPHASH(hash, exten) \
    { \
        const unsigned char* __CMGH_ext = (exten); \
        if (*__CMGH_ext != 0) \
        { \
            hash = (LowerCase[*__CMGH_ext] - 'a'); \
            if (*++__CMGH_ext != 0) \
            { \
                hash += 3 * (LowerCase[*__CMGH_ext] - 'a'); \
                if (*++__CMGH_ext != 0) \
                    hash += 7 * (LowerCase[*__CMGH_ext] - 'a'); \
            } \
        } \
        hash = hash % MasksHashArraySize; \
    }

BOOL CMaskGroup::PrepareMasks(int& errorPos, const char* masksString)
{
    CALL_STACK_MESSAGE1("CMaskGroup::PrepareMasks(,)");
    if (masksString == NULL && !NeedPrepare)
        return TRUE;

    int i;
    for (i = 0; i < PreparedMasks.Count; i++)
        if (PreparedMasks[i] != NULL)
            free(PreparedMasks[i]);
    PreparedMasks.DestroyMembers();
    ReleaseMasksHashArray();

    const char* useMasksString = masksString == NULL ? MasksString : masksString;
    const char* s = useMasksString;
    char buf[MAX_PATH];
    char maskBuf[MAX_PATH];
    int excludePos = -1;   // if not -1, all following masks are exclude type
                           // and will be inserted at the beginning of the array
    int hashableMasks = 0; // number of masks that can be hashed (MASK_OPTIMIZE_EXTENSION + CMaskItemFlags::Exclude==0)

    // to avoid unnecessary reallocations for longer arrays, set a reasonable delta
    int masksLen = (int)strlen(s);
    PreparedMasks.SetDelta(max(10, (masksLen / 6) / 2)); // "*.xxx;" is 6 characters, use half the extensions

    while (1)
    {
        char* mask = maskBuf;
        while (*s != 0 && *s > 31 && *s != '\\' && *s != '/' &&
               *s != '<' && *s != '>' && *s != ':' && *s != '"')
        {
            if (*s == '|')
                break;
            if (*s == ';')
            {
                if (*(s + 1) == ';')
                    s++;
                else
                    break;
            }
            *mask++ = *s++;
        }
        *mask = 0;
        if (*s != 0 && *s != ';' && (*s != '|' || excludePos != -1)) // the exclude character '|' may appear only once in the mask
        {
            errorPos = (int)(s - useMasksString);
            return FALSE;
        }

        while (--mask >= maskBuf && *mask <= ' ')
            ;
        *(mask + 1) = 0;
        mask = maskBuf;
        while (*mask != 0 && *mask <= ' ')
            mask++;

        if (*mask != 0)
        {
            PrepareMask(buf, mask); // call PrepareMask for extendedMode as well
            if (buf[0] != 0)
            {
                int l = (int)strlen(buf) + 1;
                char* newMask = (char*)malloc(1 + l);
                if (newMask != NULL)
                {
                    CMaskItemFlags* flags = (CMaskItemFlags*)newMask;
                    flags->Optimize = MASK_OPTIMIZE_NONE;
                    // determine whether one of the optimizations can be used
                    if (lstrcmp(buf, "*") == 0 || lstrcmp(buf, "*.*") == 0)
                        flags->Optimize = MASK_OPTIMIZE_ALL; // *.* nebo *
                    else
                    {
                        if (l > 3 && buf[0] == '*' && buf[1] == '.') // *.xxxx
                        {
                            const char* iter = buf + 2;
                            if (ExtendedMode)
                            {
                                while (*iter != 0 && *iter != '*' && *iter != '?' && *iter != '#' && *iter != '.')
                                    iter++;
                            }
                            else
                            {
                                while (*iter != 0 && *iter != '*' && *iter != '?' && *iter != '.')
                                    iter++;
                            }
                            if (*iter == 0)
                            {
                                flags->Optimize = MASK_OPTIMIZE_EXTENSION;
                                if (excludePos == -1)
                                    hashableMasks++;
                            }
                        }
                    }
                    flags->Exclude = excludePos != -1 ? 1 : 0;

                    memmove(newMask + 1, buf, l);
                    if (excludePos != -1)
                        PreparedMasks.Insert(0, newMask); // insert exclude masks at the beginning
                    else
                        PreparedMasks.Add(newMask); // append include masks at the end
                    if (!PreparedMasks.IsGood())
                    {
                        free(newMask);
                        PreparedMasks.ResetState();
                        errorPos = 0;
                        return FALSE;
                    }
                }
                else
                {
                    TRACE_E(LOW_MEMORY);
                    errorPos = 0;
                    return FALSE;
                }
            }
        }

        if (*s == 0)
        {
            if (excludePos != -1 && (PreparedMasks.Count == 0 ||
                                     ((CMaskItemFlags*)PreparedMasks[0])->Exclude == 0))
            {
                // if the '|' character is not followed by another mask, the syntax is invalid
                errorPos = excludePos;
                return FALSE;
            }
            break;
        }
        if (*s == '|')
        {
            if (PreparedMasks.Count == 0)
            {
                // the user specified a sequence starting with '|', we must append an implicit * at the end
                char* newMask = (char*)malloc(1 + 2);
                if (newMask != NULL)
                {
                    CMaskItemFlags* flags = (CMaskItemFlags*)newMask;
                    flags->Optimize = MASK_OPTIMIZE_ALL;
                    flags->Exclude = 0;
                    newMask[1] = '*';
                    newMask[2] = 0;
                    PreparedMasks.Add(newMask);
                    if (!PreparedMasks.IsGood())
                    {
                        free(newMask);
                        PreparedMasks.ResetState();
                        errorPos = 0;
                        return FALSE;
                    }
                }
                else
                {
                    TRACE_E(LOW_MEMORY);
                    errorPos = 0;
                    return FALSE;
                }
            }
            excludePos = (int)(s - useMasksString); // the next mask will be of the exclude type
        }
        s++;
    }

    if (hashableMasks >= 10) // to be worthwhile there should be at least 10
    {
        MasksHashArraySize = 2 * hashableMasks;
        MasksHashArray = (CMasksHashEntry*)malloc(MasksHashArraySize * sizeof(CMasksHashEntry));
        if (MasksHashArray != NULL)
        {
            memset(MasksHashArray, 0, MasksHashArraySize * sizeof(CMasksHashEntry));
            int i2;
            for (i2 = PreparedMasks.Count - 1; i2 >= 0; i2--)
            {
                CMaskItemFlags* mask = (CMaskItemFlags*)PreparedMasks[i2];
                if (mask->Optimize == MASK_OPTIMIZE_EXTENSION &&
                    mask->Exclude == 0)
                { // this mask can be hashed; add it to the hash array
                    DWORD hash = 0;
                    COMPUTEMASKGROUPHASH(hash, (unsigned char*)mask + 3);
                    if (MasksHashArray[hash].Mask == NULL)
                    {
                        MasksHashArray[hash].Mask = mask;
                        PreparedMasks.Detach(i2);
                        if (!PreparedMasks.IsGood())
                            PreparedMasks.ResetState(); // Detach always succeeds (at most the array won't shift, which is fine)
                    }
                    else
                    {
                        CMasksHashEntry* next = &MasksHashArray[hash];
                        while (next->Next != NULL)
                        {
                            next = next->Next;
                        }
                        next->Next = (CMasksHashEntry*)malloc(sizeof(CMasksHashEntry));
                        if (next->Next != NULL)
                        {
                            next->Next->Mask = mask;
                            next->Next->Next = NULL;
                            PreparedMasks.Detach(i2);
                            if (!PreparedMasks.IsGood())
                                PreparedMasks.ResetState(); // Detach always succeeds (at most the array won't shift, which is fine)
                        }
                        else // out of memory -> nothing happens, we skip this one mask
                            TRACE_E(LOW_MEMORY);
                    }
                }
            }
            /*
#ifdef _DEBUG
      int maxDepth = 0;
      int squareOfDepths = 0;
      int usedIndexes = 0;
      for (i = 0; i < MasksHashArraySize; i++)
      {
        CMasksHashEntry *next = &MasksHashArray[i];
        if (next->Mask != NULL)
        {
          int depth = 1;
          while (next->Next != NULL)
          {
            next = next->Next;
            depth++;
          }
          if (depth > maxDepth) maxDepth = depth;
          if (depth > 1) squareOfDepths += depth * depth;
          usedIndexes++;
        }
      }
      TRACE_I("CMaskGroup::PrepareMasks(): maxHashDepth=" << maxDepth << ", count=" <<
              hashableMasks << ", squareOfDepths=" << squareOfDepths << ", usedIndexes=" << usedIndexes);
#endif // _DEBUG
*/
        }
        else // out of memory -> we simply won't accelerate searching in masks
        {
            TRACE_E(LOW_MEMORY);
            MasksHashArraySize = 0;
        }
    }
    NeedPrepare = FALSE;
    return TRUE;
}

BOOL CMaskGroup::AgreeMasks(const char* fileName, const char* fileExt)
{
    if (NeedPrepare)
        TRACE_E("CMaskGroup::AgreeMasks: PrepareMasks must be called before AgreeMasks!");

    SLOW_CALL_STACK_MESSAGE3("CMaskGroup::AgreeMasks(%s, %s)", fileName, fileExt);
    if (fileExt == NULL)
    {
        int tmpLen = lstrlen(fileName);
        fileExt = fileName + tmpLen;
        while (--fileExt >= fileName && *fileExt != '.')
            ;
        if (fileExt < fileName)
            fileExt = fileName + tmpLen; // ".cvspass" in Windows is an extension ...
        else
            fileExt++;
    }
    const char* ext = fileExt;
    if (*ext == 0 && *fileName == '.' && *(ext - 1) != '.') // may be the ".cvspass" case; ".." has no extension
    {
        TRACE_E("CMaskGroup::AgreeMasks: Unexpected situation: fileName starts with '.' but fileExt points to end of name: " << fileName);
        ext = fileName + 1;
    }
    int i;
    for (i = 0; i < PreparedMasks.Count; i++)
    {
        char* mask = PreparedMasks[i];
        if (mask != NULL)
        {
            CMaskItemFlags* flags = (CMaskItemFlags*)mask;
            if (flags->Exclude == 1)
            {
                if (flags->Optimize == MASK_OPTIMIZE_ALL) // *.*; *
                    return FALSE;
                if (flags->Optimize == MASK_OPTIMIZE_EXTENSION) // *.xxxx
                {
                    if (StrICmp(ext, mask + 3) == 0)
                        return FALSE;
                    else
                        continue;
                }
                mask++;
                if (AgreeMask(fileName, mask, *fileExt != 0, ExtendedMode))
                    return FALSE;
            }
            else
            {
                if (flags->Optimize == MASK_OPTIMIZE_ALL) // *.*; *
                    return TRUE;
                if (flags->Optimize == MASK_OPTIMIZE_EXTENSION) // *.xxxx
                {
                    if (StrICmp(ext, mask + 3) == 0)
                        return TRUE;
                    else
                        continue;
                }
                mask++;
                if (AgreeMask(fileName, mask, *fileExt != 0, ExtendedMode))
                    return TRUE;
            }
        }
    }
    if (MasksHashArray != NULL) // there are still some masks in the hash array
    {
        DWORD hash = 0;
        COMPUTEMASKGROUPHASH(hash, (unsigned char*)ext);
        CMasksHashEntry* item = &MasksHashArray[hash];
        if (item->Mask != NULL)
        {
            do
            {
                if (StrICmp(ext, ((char*)item->Mask) + 3) == 0)
                    return TRUE;
                item = item->Next;
            } while (item != NULL);
        }
    }
    return FALSE;
}
