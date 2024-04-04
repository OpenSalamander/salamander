// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
//*****************************************************************************
// Function for wild-card operations
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
    //  CALL_STACK_MESSAGE4("AgreeMask(%s, %s, %d)", filename, mask, hasExtension);  // brutally slows down (called recursively)
    while (*filename != 0)
    {
        if (*mask == 0)
            return FALSE; // too short mask
        BOOL agree;
        if (extendedMode)
            agree = (LowerCase[*filename] == LowerCase[*mask] || *mask == '?' || // 'trefa' or '?' represents any character or '#' represents any digit
                     (*mask == '#' && *filename >= '0' && *filename <= '9'));
        else
            agree = (LowerCase[*filename] == LowerCase[*mask] || *mask == '?'); // hit or '?' represents any character
        if (agree)
        {
            filename++;
            mask++;
        }
        else if (*mask == '*') // '*' represents a string of characters (which can also be empty)
        {
            mask++;
            while (*filename != 0)
            {
                if (AgreeMask(filename, mask, hasExtension, extendedMode))
                    return TRUE; // rest of the mask agrees
                filename++;
            }
            break; // end of filename ...
        }
        else
            return FALSE;
    }
    if (*mask == '*')
        mask++;                        // asterisk behind -> represents "" -> all o.k.
    if (!hasExtension && *mask == '.') // the mask "*" must be taken without an extension ...
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

    // the first dot in the mask is a separator of two parts of the operation mask: the first for the name, the second for
    // extension (example: "a.b.c.d" + "*.*.old": "a.b.c" + "*" = "a.b.c"; "d" + "*.old" = "d.old" -> result
    // is the connection: "a.b.c.d.old")

    int ignPoints = 0; // how many dots does the name contain (this section corresponds to the text mask from the beginning to the first dot); the suffix corresponds to the rest of the mask (after the first dot)
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
        } // in the mask, only one dot is meaningful (name.ext), we will expand the part of the mask after the second dot at the end of the name
          //  while (*s != 0) if (*s++ == '.') {ignPoints--;}  // In this variant, the segments in the name and mask between dots will match from the back ("a.b.c.d" + "*.*.old": "a.b" + "*"; "c" + "*"; "d" + "old" -> 'a.b.c.old')
    if (ignPoints < 0)
        ignPoints = 0;
    //  if (ignPoints == 0 && *name == '.') ignPoints++;   // a dot at the beginning of the name should be ignored (it is not an extension); correction: ".cvspass" in Windows is an extension ...

    n = name;
    char* d = buffer;
    char* endBuf = buffer + bufSize - 1;
    s = mask;
    while (*s != 0 && d < endBuf)
    {
        switch (*s)
        {
        case '*': // copy the rest of the section names (up to the 'ignPoints+1'-th dot)
        {
            while (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                        ignPoints--; // This dot is part of the section, we continue
                    else
                        break;
                }
                *d++ = *n++;
                if (d >= endBuf)
                    break;
            }
            break;
        }

        case '?': // copy one character if it is not the end of the name section (up to the 'ignPoints+1'-th dot)
        {
            if (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                    {
                        ignPoints--; // This dot is part of the section, we continue
                        *d++ = *n++;
                    }
                }
                else
                    *d++ = *n++;
            }
            break;
        }

        case '.': // end of the current section name (skip to the next section in the name)
        {
            *d++ = '.';
            while (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                        ignPoints--; // This dot is part of the section, we continue
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
            *d++ = *s; // normal character - just copying
            if (*n != 0)
            {
                if (*n != '.')
                    n++; // if it's not '.', skip one character of the name
                else
                {
                    if (ignPoints > 0)
                    {
                        ignPoints--;
                        n++; // if a dot is part of a section, let's skip it too
                    }
                }
            }
            break;
        }
        }
        s++;
    }
    while (--d >= buffer && *d == '.')
        ; // we need to trim the result of the trailing '.'
    *++d = 0;
    return buffer;
}

//
//*****************************************************************************
// Function for quick-search searching
// the character '/' represents any number of characters (like '*' in a normal mask)

// returns TRUE if it is a character representing '*'
// must be characters that are not allowed in the file name
// should also be easily insertable (see the '<' character on a German keyboard;
// to type a backslash on a German keyboard, you need to press AltGr+\)
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
                // Convert 'other' wild character to '/'
                src++;
                *mask++ = (lastChar = '/');
            }
        }
        else
            *mask++ = (lastChar = *src++);
    }
    // Remove '/' at the end of the mask (it doesn't make sense here)
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
        else if (*mask == '/') // '/' represents a string of characters (can also be empty)
        {
            mask++;
            while (*filename != 0)
            {
                if (AgreeQSMaskAux(filename, hasExtension, filenameBase, mask, wholeString, offset))
                    return TRUE; // rest of the mask agrees
                filename++;
            }
            break; // end of filename ...
        }
        else
            return FALSE;
    }
    if (*mask == 0 ||
        !hasExtension && *mask == '.' && *(mask + 1) == 0) // A dot at the end of the mask is tolerated for names without suffix ('/' at the end of the string is trimmed, we do not solve)
    {
        offset = (int)(filename - filenameBase);
        return TRUE; // mask went to the full name -> 'offset' = length of the file name
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
        if (!PrepareMasks(errpos)) // There should be no error because the source mask is okay
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
            if (MasksHashArray[i].Mask != NULL) // if the element of the array is not empty
            {
                free(MasksHashArray[i].Mask);
                CMasksHashEntry* next = MasksHashArray[i].Next;
                while (next != NULL) // if there are multiple masks on the same element (hash)
                {
                    CMasksHashEntry* nextNext = next->Next;
                    if (next->Mask != NULL)
                        free(next->Mask);
                    free(next);
                    next = nextNext;
                }
            }
        }
        free(MasksHashArray); // to prevent the destructors of individual objects in the array from being called
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
    // originally memcpy, but in some calls it entered as
    // masks is a pointer to MasksString and there was an overlap
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
    NeedPrepare = TRUE; // Unfortunately, it is also used as a write buffer, so we need to enable additional PrepareMasks
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
    int excludePos = -1;   // if it is different from -1, all subsequent masks are of type exclude
                           // and we will insert them at the beginning of the array
    int hashableMasks = 0; // number of masks that can be hashed (MASK_OPTIMIZE_EXTENSION + CMaskItemFlags::Exclude==0)

    // To prevent unnecessary relocations in longer arrays, we will set a reasonable delta
    int masksLen = (int)strlen(s);
    PreparedMasks.SetDelta(max(10, (masksLen / 6) / 2)); // "*.xxx;" is 6 characters, we will take half of the suffixes

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
        if (*s != 0 && *s != ';' && (*s != '|' || excludePos != -1)) // exclude character '|' can only appear once in the mask
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
            PrepareMask(buf, mask); // for extendedMode we also call PrepareMask
            if (buf[0] != 0)
            {
                int l = (int)strlen(buf) + 1;
                char* newMask = (char*)malloc(1 + l);
                if (newMask != NULL)
                {
                    CMaskItemFlags* flags = (CMaskItemFlags*)newMask;
                    flags->Optimize = MASK_OPTIMIZE_NONE;
                    // Check if it will be possible to use one of the optimizations
                    if (lstrcmp(buf, "*") == 0 || lstrcmp(buf, "*.*") == 0)
                        flags->Optimize = MASK_OPTIMIZE_ALL; // *.* or *
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
                        PreparedMasks.Insert(0, newMask); // exclude insert at the beginning
                    else
                        PreparedMasks.Add(newMask); // include attach to the end
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
                // if the character '|' is not followed by another mask, it is a syntax error
                errorPos = excludePos;
                return FALSE;
            }
            break;
        }
        if (*s == '|')
        {
            if (PreparedMasks.Count == 0)
            {
                // The user entered a sequence starting with the '|' character, we must append an implicit * at the end
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
            excludePos = (int)(s - useMasksString); // the following mask will be of type exclude
        }
        s++;
    }

    if (hashableMasks >= 10) // for it to make any sense, let there be at least 10 of them
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
                { // this is a hashable mask, let's add it to the hash table
                    DWORD hash = 0;
                    COMPUTEMASKGROUPHASH(hash, (unsigned char*)mask + 3);
                    if (MasksHashArray[hash].Mask == NULL)
                    {
                        MasksHashArray[hash].Mask = mask;
                        PreparedMasks.Detach(i2);
                        if (!PreparedMasks.IsGood())
                            PreparedMasks.ResetState(); // Detaching always succeeds (even if the array does not collapse, we don't care)
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
                                PreparedMasks.ResetState(); // Detaching always succeeds (even if the array does not collapse, we don't care)
                        }
                        else // low memory -> nothing happens, let's ignore this one mask
                            TRACE_E(LOW_MEMORY);
                    }
                }
            }
            /*  #ifdef _DEBUG
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
#endif // _DEBUG*/
        }
        else // low memory -> nothing is happening, we just won't speed up searching in masks
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
    if (*ext == 0 && *fileName == '.' && *(ext - 1) != '.') // It could be a case of ".cvspass"; ".." does not have an extension
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
    if (MasksHashArray != NULL) // we still have some masks in the hashing array
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
