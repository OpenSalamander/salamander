// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
//*****************************************************************************
// Funkce pro wild-card operace
//

void PrepareMask(char* mask, const char* src)
{
    SLOW_CALL_STACK_MESSAGE2("PrepareMask(, %s)", src);
    char* begMask = mask;
    char lastChar = 0;
    // odstranime mezery na zacatku masky
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
    // orizneme mezery na konci masky
    while (mask > begMask && *(mask - 1) == ' ')
        mask--;
    *mask = 0;
}

BOOL AgreeMask(const char* filename, const char* mask, BOOL hasExtension, BOOL extendedMode)
{
    CALL_STACK_MESSAGE_NONE;
    //  CALL_STACK_MESSAGE4("AgreeMask(%s, %s, %d)", filename, mask, hasExtension);  // brutalne zpomaluje (vola se rekurzivne)
    while (*filename != 0)
    {
        if (*mask == 0)
            return FALSE; // prilis kratka maska
        BOOL agree;
        if (extendedMode)
            agree = (LowerCase[*filename] == LowerCase[*mask] || *mask == '?' || // trefa nebo '?' zastupuje libovolny znak nebo '#' zastupuje libovolnou cislici
                     (*mask == '#' && *filename >= '0' && *filename <= '9'));
        else
            agree = (LowerCase[*filename] == LowerCase[*mask] || *mask == '?'); // trefa nebo '?' zastupuje libovolny znak
        if (agree)
        {
            filename++;
            mask++;
        }
        else if (*mask == '*') // '*' zastupuje retezec znaku (muze byt i prazdny)
        {
            mask++;
            while (*filename != 0)
            {
                if (AgreeMask(filename, mask, hasExtension, extendedMode))
                    return TRUE; // zbytek masky sohlasi
                filename++;
            }
            break; // konec filename ...
        }
        else
            return FALSE;
    }
    if (*mask == '*')
        mask++;                        // hvezdicka za -> zastupuje "" -> vse o.k.
    if (!hasExtension && *mask == '.') // bez pripony musi maska "*.*" vzit ...
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

    // prvni tecka v masce je oddelovac dvou casti operacni masky: prvni pro jmeno, druha pro
    // priponu (priklad: "a.b.c.d" + "*.*.old": "a.b.c" + "*" = "a.b.c"; "d" + "*.old" = "d.old" -> vysledek
    // je spojeni: "a.b.c.d.old")

    int ignPoints = 0; // kolik tecek obsahuje jmeno (teto sekci odpovida text masky od zacatku do prvni tecky); pripone odpovida zbytek masky (za prvni teckou)
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
        } // v masce je smysluplna jen jedna tecka (name.ext), cast masky za druhou teckou budeme expandovat na konci jmena
          //  while (*s != 0) if (*s++ == '.') {ignPoints--;}  // v teto variante si budou odpovidat useky ve jmene a masce mezi teckami odzadu ("a.b.c.d" + "*.*.old": "a.b" + "*"; "c" + "*"; "d" + "old" -> 'a.b.c.old')
    if (ignPoints < 0)
        ignPoints = 0;
    //  if (ignPoints == 0 && *name == '.') ignPoints++;   // tecka na zacatku jmena se ma ignorovat (neni pripona); oprava: ".cvspass" ve Windows je pripona ...

    n = name;
    char* d = buffer;
    char* endBuf = buffer + bufSize - 1;
    s = mask;
    while (*s != 0 && d < endBuf)
    {
        switch (*s)
        {
        case '*': // nakopirujeme zbytek sekce jmena (do 'ignPoints+1'-te tecky)
        {
            while (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                        ignPoints--; // tato tecka je soucasti sekce, pokracujeme
                    else
                        break;
                }
                *d++ = *n++;
                if (d >= endBuf)
                    break;
            }
            break;
        }

        case '?': // nakopirujeme jeden znak pokud nejde o konec sekce jmena (do 'ignPoints+1'-te tecky)
        {
            if (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                    {
                        ignPoints--; // tato tecka je soucasti sekce, pokracujeme
                        *d++ = *n++;
                    }
                }
                else
                    *d++ = *n++;
            }
            break;
        }

        case '.': // konec aktualni sekce jmena (preskocime na dalsi sekci ve jmene)
        {
            *d++ = '.';
            while (*n != 0)
            {
                if (*n == '.')
                {
                    if (ignPoints > 0)
                        ignPoints--; // tato tecka je soucasti sekce, pokracujeme
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
            *d++ = *s; // normalni znak - pouze kopirujeme
            if (*n != 0)
            {
                if (*n != '.')
                    n++; // pokud nejde o '.', preskocime jeden znak ze jmena
                else
                {
                    if (ignPoints > 0)
                    {
                        ignPoints--;
                        n++; // je-li tecka soucasti sekce, preskocime ji tez
                    }
                }
            }
            break;
        }
        }
        s++;
    }
    while (--d >= buffer && *d == '.')
        ; // vysledek musime orezat o koncove '.'
    *++d = 0;
    return buffer;
}

//
//*****************************************************************************
// Funkce pro quick-search vyhledavani
// znak '/' zastupuje lib. pocet znaku (ala '*' v normalni masce)

// vraci TRUE, pokud se jedna o znak zastupujici '*'
// musi se jednat o znaky, ktere nejsou povolene v nazvu souboru
// zaroven by mely byt dobre vlozitelne (viz znak '<' na nemecke klavesnici;
// pro zpetne lomitko je na nemecke klavesnici treba stisknout AltGr+\)
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
                // konverze 'ostatni' wild znaku na '/'
                src++;
                *mask++ = (lastChar = '/');
            }
        }
        else
            *mask++ = (lastChar = *src++);
    }
    // orizneme '/' na konci masky (nema zde smysl)
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
            return TRUE; // konec masky, 'offset' = kam az to saha do jmena souboru
        }
        if (LowerCase[*filename] == LowerCase[*mask])
        {
            filename++;
            mask++;
        }
        else if (*mask == '/') // '/' zastupuje retezec znaku (muze byt i prazdny)
        {
            mask++;
            while (*filename != 0)
            {
                if (AgreeQSMaskAux(filename, hasExtension, filenameBase, mask, wholeString, offset))
                    return TRUE; // zbytek masky souhlasi
                filename++;
            }
            break; // konec filename ...
        }
        else
            return FALSE;
    }
    if (*mask == 0 ||
        !hasExtension && *mask == '.' && *(mask + 1) == 0) // tecka na konci masky se u jmen bez pripony toleruje ('/' na konci retezce se orezava, neresime)
    {
        offset = (int)(filename - filenameBase);
        return TRUE; // maska vysla na cele jmeno -> 'offset' = delka jmena souboru
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
        if (!PrepareMasks(errpos)) // k chybe by nemelo dojit, protoze zdrojova maska je ok
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
            if (MasksHashArray[i].Mask != NULL) // neni-li prvek pole prazdny
            {
                free(MasksHashArray[i].Mask);
                CMasksHashEntry* next = MasksHashArray[i].Next;
                while (next != NULL) // pokud je vic masek na stejnem prvku (hashi)
                {
                    CMasksHashEntry* nextNext = next->Next;
                    if (next->Mask != NULL)
                        free(next->Mask);
                    free(next);
                    next = nextNext;
                }
            }
        }
        free(MasksHashArray); // aby se nevolali destruktory jednotlivych objektu v poli
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
    // puvodne memcpy, ale pri nekterych volani vstupoval jako
    // masks ukazatel na MasksString a dochazelo k prekryvu
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
    NeedPrepare = TRUE; // pouziva se bohuzel i jako buffer pro zapis, takze musime povolit dalsi PrepareMasks
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
    int excludePos = -1;   // pokud je ruzny od -1, vsechny nasledujici masky jsou typu exclude
                           // a budeme je zarazovat na zacatek pole
    int hashableMasks = 0; // pocet masek, ktere je mozne hashovat (MASK_OPTIMIZE_EXTENSION + CMaskItemFlags::Exclude==0)

    // abychom predesli zbytecnym relokacim u delsich poli, nastavime rozumne deltu
    int masksLen = (int)strlen(s);
    PreparedMasks.SetDelta(max(10, (masksLen / 6) / 2)); // "*.xxx;" to je 6 znaku, dame polovinu pripon

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
        if (*s != 0 && *s != ';' && (*s != '|' || excludePos != -1)) // exclude znak '|' muze byt v masce pouze jednou
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
            PrepareMask(buf, mask); // pro extendedMode volame take PrepareMask
            if (buf[0] != 0)
            {
                int l = (int)strlen(buf) + 1;
                char* newMask = (char*)malloc(1 + l);
                if (newMask != NULL)
                {
                    CMaskItemFlags* flags = (CMaskItemFlags*)newMask;
                    flags->Optimize = MASK_OPTIMIZE_NONE;
                    // zjistim, jestli bude mozne pouzit jednu z optimalizaci
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
                        PreparedMasks.Insert(0, newMask); // exlude vkladam na zacatek
                    else
                        PreparedMasks.Add(newMask); // include pripojim na konec
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
                // pokud znak '|' neni nasledovan dalsi maskou, je to syntakticka chyba
                errorPos = excludePos;
                return FALSE;
            }
            break;
        }
        if (*s == '|')
        {
            if (PreparedMasks.Count == 0)
            {
                // uzivatel zadal sekvenci zacinajici znakem '|', musime na konec pripojit implicitni *
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
            excludePos = (int)(s - useMasksString); // nasledujici masku budou typu exclude
        }
        s++;
    }

    if (hashableMasks >= 10) // aby to vubec melo smysl, at je jich aspon 10
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
                { // to je hashovatelna maska, jdeme ji pridat do hashovaciho pole
                    DWORD hash = 0;
                    COMPUTEMASKGROUPHASH(hash, (unsigned char*)mask + 3);
                    if (MasksHashArray[hash].Mask == NULL)
                    {
                        MasksHashArray[hash].Mask = mask;
                        PreparedMasks.Detach(i2);
                        if (!PreparedMasks.IsGood())
                            PreparedMasks.ResetState(); // Detach se vzdy povede (max se nesesune pole a to je nam fuk)
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
                                PreparedMasks.ResetState(); // Detach se vzdy povede (max se nesesune pole a to je nam fuk)
                        }
                        else // malo pameti -> nic se nedeje, vykasleme se na tuhle jednu masku
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
        else // malo pameti -> nic se nedeje, jen nebudeme zrychlovat hledani v maskach
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
            fileExt = fileName + tmpLen; // ".cvspass" ve Windows je pripona ...
        else
            fileExt++;
    }
    const char* ext = fileExt;
    if (*ext == 0 && *fileName == '.' && *(ext - 1) != '.') // muze jit o pripad ".cvspass"; ".." nema priponu
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
    if (MasksHashArray != NULL) // jeste mame nejake masky v hashovacim poli
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
