// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "find.h"
#include "md5.h"

char* FindNamedHistory[FIND_NAMED_HISTORY_SIZE];
char* FindLookInHistory[FIND_LOOKIN_HISTORY_SIZE];
char* FindGrepHistory[FIND_GREP_HISTORY_SIZE];

CFindOptions FindOptions;
CFindIgnore FindIgnore;
CFindDialogQueue FindDialogQueue("Find Dialogs");

HANDLE FindDialogContinue = NULL;

HACCEL FindDialogAccelTable = NULL;

const char* FINDOPTIONSITEM_ITEMNAME_REG = "ItemName";
const char* FINDOPTIONSITEM_SUBDIRS_REG = "SubDirectories";
const char* FINDOPTIONSITEM_WHOLEWORDS_REG = "WholeWords";
const char* FINDOPTIONSITEM_CASESENSITIVE_REG = "CaseSensitive";
const char* FINDOPTIONSITEM_HEXMODE_REG = "HexMode";
const char* FINDOPTIONSITEM_REGULAR_REG = "RegularExpresions";
const char* FINDOPTIONSITEM_AUTOLOAD_REG = "AutoLoad";
const char* FINDOPTIONSITEM_NAMED_REG = "Named";
const char* FINDOPTIONSITEM_LOOKIN_REG = "LookIn";
const char* FINDOPTIONSITEM_GREP_REG = "Grep";

const char* FINDIGNOREITEM_PATH_REG = "Path";
const char* FINDIGNOREITEM_ENABLED_REG = "Enabled";

// nasledujici promennou jsme pouzivali do Altap Salamander 2.5,
// kde jsme presli na CFilterCriteria a jeho Save/Load
const char* OLD_FINDOPTIONSITEM_EXCLUDEMASK_REG = "ExcludeMask";

//*********************************************************************************
//
// InitializeFind, ReleaseFind
//

BOOL InitializeFind()
{
    int i;
    for (i = 0; i < FIND_NAMED_HISTORY_SIZE; i++)
        FindNamedHistory[i] = NULL;
    for (i = 0; i < FIND_LOOKIN_HISTORY_SIZE; i++)
        FindLookInHistory[i] = NULL;
    for (i = 0; i < FIND_GREP_HISTORY_SIZE; i++)
        FindGrepHistory[i] = NULL;

    FindDialogContinue = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (FindDialogContinue == NULL)
    {
        TRACE_E("Unable to create FindDialogContinue event.");
        return FALSE;
    }

    FindDialogAccelTable = HANDLES(LoadAccelerators(HInstance, MAKEINTRESOURCE(IDA_FINDDIALOGACCELS)));
    if (FindDialogAccelTable == NULL)
    {
        TRACE_E("Unable to load accelerators for Find dialog.");
        return FALSE;
    }
    return TRUE;
}

void ClearFindHistory(BOOL dataOnly)
{
    int i;
    for (i = 0; i < FIND_NAMED_HISTORY_SIZE; i++)
    {
        if (FindNamedHistory[i] != NULL)
        {
            free(FindNamedHistory[i]);
            FindNamedHistory[i] = NULL;
        }
    }

    for (i = 0; i < FIND_LOOKIN_HISTORY_SIZE; i++)
    {
        if (FindLookInHistory[i] != NULL)
        {
            free(FindLookInHistory[i]);
            FindLookInHistory[i] = NULL;
        }
    }

    for (i = 0; i < FIND_GREP_HISTORY_SIZE; i++)
    {
        if (FindGrepHistory[i] != NULL)
        {
            free(FindGrepHistory[i]);
            FindGrepHistory[i] = NULL;
        }
    }

    // mame promazat i combboxy otevrenych oken
    if (!dataOnly)
    {
        FindDialogQueue.BroadcastMessage(WM_USER_CLEARHISTORY, 0, 0);
    }
}

void ReleaseFind()
{
    ClearFindHistory(TRUE); // pouze uvolnime data
    if (FindDialogContinue != NULL)
        HANDLES(CloseHandle(FindDialogContinue));
}

//*********************************************************************************
//
// CFindOptionsItem
//

CFindOptionsItem::CFindOptionsItem()
{
    // Internal
    ItemName[0] = 0;

    // Find dialog
    SubDirectories = TRUE;
    WholeWords = FALSE;
    CaseSensitive = FALSE;
    HexMode = FALSE;
    RegularExpresions = FALSE;

    AutoLoad = FALSE;

    NamedText[0] = 0;
    LookInText[0] = 0;
    GrepText[0] = 0;
}

CFindOptionsItem&
CFindOptionsItem::operator=(const CFindOptionsItem& s)
{
    // Internal
    lstrcpy(ItemName, s.ItemName);

    memmove(&Criteria, &s.Criteria, sizeof(Criteria));

    // Find dialog
    SubDirectories = s.SubDirectories;
    WholeWords = s.WholeWords;
    CaseSensitive = s.CaseSensitive;
    HexMode = s.HexMode;
    RegularExpresions = s.RegularExpresions;

    AutoLoad = s.AutoLoad;

    lstrcpy(NamedText, s.NamedText);
    lstrcpy(LookInText, s.LookInText);
    lstrcpy(GrepText, s.GrepText);

    return *this;
}

void CFindOptionsItem::BuildItemName()
{
    sprintf(ItemName, "\"%s\" %s \"%s\"",
            NamedText, LoadStr(IDS_FF_IN), LookInText);
}

BOOL CFindOptionsItem::Save(HKEY hKey)
{
    // optimalizace na velikost v Registry: ukladame pouze "non-default hodnoty";
    // pred ukladanim je proto treba promazat klic, do ktereho se budeme ukladat
    CFindOptionsItem def;

    if (strcmp(ItemName, def.ItemName) != 0)
        SetValue(hKey, FINDOPTIONSITEM_ITEMNAME_REG, REG_SZ, ItemName, -1);
    if (SubDirectories != def.SubDirectories)
        SetValue(hKey, FINDOPTIONSITEM_SUBDIRS_REG, REG_DWORD, &SubDirectories, sizeof(DWORD));
    if (WholeWords != def.WholeWords)
        SetValue(hKey, FINDOPTIONSITEM_WHOLEWORDS_REG, REG_DWORD, &WholeWords, sizeof(DWORD));
    if (CaseSensitive != def.CaseSensitive)
        SetValue(hKey, FINDOPTIONSITEM_CASESENSITIVE_REG, REG_DWORD, &CaseSensitive, sizeof(DWORD));
    if (HexMode != def.HexMode)
        SetValue(hKey, FINDOPTIONSITEM_HEXMODE_REG, REG_DWORD, &HexMode, sizeof(DWORD));
    if (RegularExpresions != def.RegularExpresions)
        SetValue(hKey, FINDOPTIONSITEM_REGULAR_REG, REG_DWORD, &RegularExpresions, sizeof(DWORD));
    if (AutoLoad != def.AutoLoad)
        SetValue(hKey, FINDOPTIONSITEM_AUTOLOAD_REG, REG_DWORD, &AutoLoad, sizeof(DWORD));
    if (strcmp(NamedText, def.NamedText) != 0)
        SetValue(hKey, FINDOPTIONSITEM_NAMED_REG, REG_SZ, NamedText, -1);
    if (strcmp(LookInText, def.LookInText) != 0)
        SetValue(hKey, FINDOPTIONSITEM_LOOKIN_REG, REG_SZ, LookInText, -1);
    if (strcmp(GrepText, def.GrepText) != 0)
        SetValue(hKey, FINDOPTIONSITEM_GREP_REG, REG_SZ, GrepText, -1);

    // advanced options
    Criteria.Save(hKey);
    return TRUE;
}

BOOL CFindOptionsItem::Load(HKEY hKey, DWORD cfgVersion)
{
    GetValue(hKey, FINDOPTIONSITEM_ITEMNAME_REG, REG_SZ, ItemName, ITEMNAME_TEXT_LEN);
    GetValue(hKey, FINDOPTIONSITEM_SUBDIRS_REG, REG_DWORD, &SubDirectories, sizeof(DWORD));
    GetValue(hKey, FINDOPTIONSITEM_WHOLEWORDS_REG, REG_DWORD, &WholeWords, sizeof(DWORD));
    GetValue(hKey, FINDOPTIONSITEM_CASESENSITIVE_REG, REG_DWORD, &CaseSensitive, sizeof(DWORD));
    GetValue(hKey, FINDOPTIONSITEM_HEXMODE_REG, REG_DWORD, &HexMode, sizeof(DWORD));
    GetValue(hKey, FINDOPTIONSITEM_REGULAR_REG, REG_DWORD, &RegularExpresions, sizeof(DWORD));
    GetValue(hKey, FINDOPTIONSITEM_AUTOLOAD_REG, REG_DWORD, &AutoLoad, sizeof(DWORD));
    GetValue(hKey, FINDOPTIONSITEM_NAMED_REG, REG_SZ, NamedText, NAMED_TEXT_LEN);
    GetValue(hKey, FINDOPTIONSITEM_LOOKIN_REG, REG_SZ, LookInText, LOOKIN_TEXT_LEN);
    GetValue(hKey, FINDOPTIONSITEM_GREP_REG, REG_SZ, GrepText, GREP_TEXT_LEN);

    if (cfgVersion <= 13)
    {
        // konverze starych hodnot

        // exclude mask
        BOOL excludeMask = FALSE;
        GetValue(hKey, OLD_FINDOPTIONSITEM_EXCLUDEMASK_REG, REG_DWORD, &excludeMask, sizeof(DWORD));
        if (excludeMask)
        {
            memmove(NamedText + 1, NamedText, NAMED_TEXT_LEN - 1);
            NamedText[0] = '|';
        }

        Criteria.LoadOld(hKey);
    }
    else
        Criteria.Load(hKey);

    return TRUE;
}

//*********************************************************************************
//
// CFindOptions
//

CFindOptions::CFindOptions()
    : Items(20, 10)
{
}

BOOL CFindOptions::Save(HKEY hKey)
{
    ClearKey(hKey);

    HKEY subKey;
    char buf[30];
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        itoa(i + 1, buf, 10);
        if (CreateKey(hKey, buf, subKey))
        {
            Items[i]->Save(subKey);
            CloseKey(subKey);
        }
        else
            break;
    }
    return TRUE;
}

BOOL CFindOptions::Load(HKEY hKey, DWORD cfgVersion)
{
    HKEY subKey;
    char buf[30];
    int i = 1;
    strcpy(buf, "1");
    Items.DestroyMembers();
    while (OpenKey(hKey, buf, subKey))
    {
        CFindOptionsItem* item = new CFindOptionsItem();
        if (item == NULL)
        {
            TRACE_E(LOW_MEMORY);
            break;
        }
        item->Load(subKey, cfgVersion);
        Items.Add(item);
        if (!Items.IsGood())
        {
            Items.ResetState();
            delete item;
            break;
        }
        itoa(++i, buf, 10);
        CloseKey(subKey);
    }

    return TRUE;
}

BOOL CFindOptions::Load(CFindOptions& source)
{
    CFindOptionsItem* item;
    Items.DestroyMembers();
    int i;
    for (i = 0; i < source.Items.Count; i++)
    {
        item = new CFindOptionsItem();
        if (item == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        *item = *source.Items[i];
        Items.Add(item);
        if (!Items.IsGood())
        {
            delete item;
            Items.ResetState();
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CFindOptions::Add(CFindOptionsItem* item)
{
    Items.Add(item);
    if (!Items.IsGood())
    {
        Items.ResetState();
        return FALSE;
    }
    return TRUE;
}

//*********************************************************************************
//
// CFindIgnoreItem
//

CFindIgnoreItem::CFindIgnoreItem()
{
    Enabled = TRUE;
    Path = NULL;
    Type = fiitUnknow;
}

CFindIgnoreItem::~CFindIgnoreItem()
{
    if (Path != NULL)
        free(Path);
}

//*********************************************************************************
//
// CFindIgnore
//

CFindIgnore::CFindIgnore()
    : Items(5, 5)
{
    Reset();
}

void CFindIgnore::Reset()
{
    Items.DestroyMembers();

    Add(TRUE, "\\System Volume Information");
    Add(FALSE, "Local Settings\\Temporary Internet Files");
}

BOOL CFindIgnore::Save(HKEY hKey)
{
    ClearKey(hKey);

    HKEY subKey;
    char buf[30];
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        itoa(i + 1, buf, 10);
        if (CreateKey(hKey, buf, subKey))
        {
            SetValue(subKey, FINDIGNOREITEM_PATH_REG, REG_SZ, Items[i]->Path, -1);
            if (!Items[i]->Enabled) // ukladame pouze je-li FALSE
                SetValue(subKey, FINDIGNOREITEM_ENABLED_REG, REG_DWORD, &Items[i]->Enabled, sizeof(DWORD));
            CloseKey(subKey);
        }
        else
            break;
    }
    return TRUE;
}

BOOL CFindIgnore::Load(HKEY hKey, DWORD cfgVersion)
{
    HKEY subKey;
    char buf[30];
    int i = 1;
    strcpy(buf, "1");
    Items.DestroyMembers();
    while (OpenKey(hKey, buf, subKey))
    {
        CFindIgnoreItem* item = new CFindIgnoreItem;
        if (item == NULL)
        {
            TRACE_E(LOW_MEMORY);
            break;
        }
        char path[MAX_PATH];
        if (!GetValue(subKey, FINDIGNOREITEM_PATH_REG, REG_SZ, path, MAX_PATH))
            path[0] = 0;
        item->Path = DupStr(path);
        if (!GetValue(subKey, FINDIGNOREITEM_ENABLED_REG, REG_DWORD, &item->Enabled, sizeof(DWORD)))
            item->Enabled = TRUE; // ulozeno pouze je-li FALSE
        if (Configuration.ConfigVersion < 32)
        {
            // uzivatele byli zmateni, ze jim neprohledavame tento adresar
            // takze ho sice nechame v seznamu, ale vypneme checkbox
            // kdo chce, muze si ho zapnout
            if (strcmp(item->Path, "Local Settings\\Temporary Internet Files") == 0)
                item->Enabled = FALSE;
        }
        Items.Add(item);
        if (!Items.IsGood())
        {
            Items.ResetState();
            delete item;
            break;
        }
        itoa(++i, buf, 10);
        CloseKey(subKey);
    }

    return TRUE;
}

BOOL CFindIgnore::Load(CFindIgnore* source)
{
    Items.DestroyMembers();
    int i;
    for (i = 0; i < source->Items.Count; i++)
    {
        CFindIgnoreItem* item = source->At(i);
        if (!Add(item->Enabled, item->Path))
            return FALSE;
    }
    return TRUE;
}

BOOL CFindIgnore::Prepare(CFindIgnore* source)
{
    Items.DestroyMembers();
    int i;
    for (i = 0; i < source->Items.Count; i++)
    {
        CFindIgnoreItem* item = source->At(i);
        if (item->Enabled) // zajimaji nas pouze enabled polozky
        {
            const char* path = item->Path;
            while (*path == ' ')
                path++;
            CFindIgnoreItemType type = fiitRelative;
            if (path[0] == '\\' && path[1] != '\\')
                type = fiitRooted;
            else if ((path[0] == '\\' && path[1] == '\\') ||
                     LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' && path[1] == ':')
                type = fiitFull;

            char buff[3 * MAX_PATH];
            if (strlen(path) >= 2 * MAX_PATH)
            {
                TRACE_E("CFindIgnore::Prepare() Path too long!");
                return FALSE;
            }
            if (type == fiitFull)
            {
                strcpy(buff, path);
            }
            else
            {
                if (path[0] == '\\')
                    strcpy(buff, path);
                else
                {
                    buff[0] = '\\';
                    strcpy(buff + 1, path);
                }
            }
            if (buff[strlen(buff) - 1] != '\\')
                strcat(buff, "\\");
            if (!Add(TRUE, buff))
                return FALSE;
            item = Items[Items.Count - 1];
            item->Type = type;
            item->Len = (int)strlen(buff);
        }
    }
    return TRUE;
}

const char* SkipRoot(const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        return s;
    }
    else
    {
        return path + 2;
    }
}

BOOL CFindIgnore::Contains(const char* path, int startPathLen)
{
    // plna cesta
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        // startPathLen - delka cesty zadane ve Find okne (root hledani), ignoruji se jen jeho
        //                podcesty, viz https://forum.altap.cz/viewtopic.php?f=7&t=7434
        CFindIgnoreItem* item = Items[i];
        switch (item->Type)
        {
        case fiitFull:
        {
            if (item->Len > startPathLen && StrNICmp(path, item->Path, item->Len) == 0)
                return TRUE;
            break;
        }

        case fiitRooted:
        {
            const char* noRoot = SkipRoot(path);
            if ((noRoot - path) + item->Len > startPathLen && StrNICmp(noRoot, item->Path, item->Len) == 0)
                return TRUE;
            break;
        }

        case fiitRelative:
        {
            const char* m = path;
            while (m != NULL)
            {
                m = StrIStr(m, item->Path);
                if (m != NULL) // nalezeno
                {
                    if ((m - path) + item->Len > startPathLen) // je to podcesta = ignorovat
                        return TRUE;
                    m++; // jdeme hledat dalsi vyskyt, treba uz bude v podceste
                }
            }
            break;
        }
        }
    }
    return FALSE;
}

BOOL CFindIgnore::Move(int srcIndex, int dstIndex)
{
    CFindIgnoreItem* tmp = Items[srcIndex];
    if (srcIndex < dstIndex)
    {
        int i;
        for (i = srcIndex; i < dstIndex; i++)
            Items[i] = Items[i + 1];
    }
    else
    {
        int i;
        for (i = srcIndex; i > dstIndex; i--)
            Items[i] = Items[i - 1];
    }
    Items[dstIndex] = tmp;
    return TRUE;
}

BOOL CFindIgnore::Add(BOOL enabled, const char* path)
{
    CFindIgnoreItem* item = new CFindIgnoreItem;
    if (item == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    item->Enabled = enabled;
    item->Path = DupStr(path);
    if (item->Path == NULL)
    {
        TRACE_E(LOW_MEMORY);
        delete item;
        return FALSE;
    }
    Items.Add(item);
    if (!Items.IsGood())
    {
        Items.ResetState();
        delete item;
        return FALSE;
    }
    return TRUE;
}

BOOL CFindIgnore::AddUnique(BOOL enabled, const char* path)
{
    int len = (int)strlen(path);
    if (len < 1)
        return FALSE;
    if (path[len - 1] == '\\') // budeme porovnavat bez koncovych lomitek
        len--;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CFindIgnoreItem* item = Items[i];
        int itemLen = (int)strlen(item->Path);
        if (itemLen < 1)
            continue;
        if (item->Path[itemLen - 1] == '\\') // budeme porovnavat bez koncovych lomitek
            itemLen--;
        if (len != itemLen)
            continue;
        if (StrNICmp(path, item->Path, len) == 0)
        {
            item->Enabled = TRUE; // v kazdem pripade polozku povolime
            return TRUE;
        }
    }
    // nenasli jsme -- pridame
    return Add(enabled, path);
}

BOOL CFindIgnore::Set(int index, BOOL enabled, const char* path)
{
    if (index < 0 || index >= Items.Count)
    {
        TRACE_E("Index is out of range");
        return FALSE;
    }
    char* p = DupStr(path);
    if (p == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    CFindIgnoreItem* item = Items[index];
    if (item->Path != NULL)
        free(item->Path);
    item->Path = p;
    item->Enabled = enabled;
    return TRUE;
}

//*********************************************************************************
//
// CDuplicateCandidates
//
// Drzak CFoundFilesData pri hledani duplicitnich souboru.
// 1) V prvni fazi se do objektu CDuplicateCandidates pridaji metodou Add
//    vsechny soubory odpovidajici kriteriim Findu.
// 2) Zavola se metoda Examine(), ktera pole seradi podle kriterii
//    data->FindDupFlags. Pokud se porovnava take obsah souboru, napocitaji
//    se MD5 digesty pro potencialne shodne soubory.
//    Pole se pak znovu seradi a odstrani se single soubory.
//    V poli zustanou pouze soubory vyskytujici se minimalne dvakrat.
//    Tem je prirazena promenna Group, aby bylo mozne skupiny od sebe
//    ve vysledkovem okne odlisit.
//

class CDuplicateCandidates : public TIndirectArray<CFoundFilesData>
{
public:
    CDuplicateCandidates() : TIndirectArray<CFoundFilesData>(2000, 4000) {}

    // - [nacteni/vypocet MD5 digestu]
    // - vyrazeni single souboru
    // - nastaveni promenne Group
    // - nastaveni promenne Different
    void Examine(CGrepData* data);

protected:
    // porovna dva zaznamy podle kriterii byName, bySize a byMD5
    // byPath je kriterium s nejnnizsi prioritou, slouzi pouze pro prehledny vystup
    int CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, BOOL byName, BOOL bySize, BOOL byMD5, BOOL byPath);

    // seradi drzene soubory podle kriterii byName, bySize a byMD5
    void QuickSort(int left, int right, BOOL byName, BOOL bySize, BOOL byMD5);

    // projde vsechny drzene polozky a na zaklade volani metody CompareFunc urci
    // ty, ktere se vyskytuji pouze jednou; ty pak z pole odstrani
    // pred volanim teto metody musi byt pole serazeno metodou QuickSort
    void RemoveSingleFiles(BOOL byName, BOOL bySize, BOOL byMD5);

    // projde vsechny drzene polozky a na zaklade volani metody CompareFunc urci
    // prislusnot do skupin; skupinam priradi na stridacku bit Different (0, 1, 0, 1, 0, 1, ...)
    // pred volanim teto metody musi byt pole serazeno metodou QuickSort
    void SetDifferentFlag(BOOL byName, BOOL bySize, BOOL byMD5);

    // projde vsechny drzene polozky a na zaklade promenne Different priradi
    // hodnotu Group; skupinam priradi na vzestupna cisla (0, 1, 2, 3, 4, 5, ...)
    void SetGroupByDifferentFlag();

    // napocita MD5 ze souboru 'file'
    // 'progress' je ciselna hodnota zobrazena v status bar (optimalizace, abychom nenastavovali progress, ktery uz tam je)
    // 'readSize' je pocet doposud nactenych bajtu ve vsech souborech
    // 'totalSize' je celkovy pocet bajtu vsech souboru, pro ktere se bude urcovat MD5 digest
    // metoda vraci TRUE, pokud se podarilo nacist MD5; na ukazatel (BYTE*)data->Group se zapise MD5 digest
    // metoda vraci FALSE pri chybe cteni nebo pri preruseni operace uzivatelem (pak je nastavena
    // promenna data->StopSearch na TRUE
    BOOL GetMD5Digest(CGrepData* data, CFoundFilesData* file,
                      int* progress, CQuadWord* readSize, const CQuadWord* totalSize);
};

int CDuplicateCandidates::CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2,
                                      BOOL byName, BOOL bySize, BOOL byMD5, BOOL byPath)
{
    int res;
    if (bySize)
    {
        if (byName)
            res = RegSetStrICmp(f1->Name, f2->Name);
        else
            res = 0;
        if (res == 0)
        {
            if (f1->Size < f2->Size)
                res = -1;
            else
            {
                if (f1->Size == f2->Size)
                {
                    if (!byMD5 || f1->Size == CQuadWord(0, 0))
                        res = 0;
                    else
                        res = memcmp((void*)f1->Group, (void*)f2->Group, MD5_DIGEST_SIZE);
                }
                else
                    res = 1;
            }
        }
    }
    else
    {
        // byName && !bySize
        res = RegSetStrICmp(f1->Name, f2->Name);
    }
    if (byPath && res == 0)
        res = RegSetStrICmp(f1->Path, f2->Path);
    return res;
}

void CDuplicateCandidates::QuickSort(int left, int right, BOOL byName, BOOL bySize, BOOL byMD5)
{

LABEL_QuickSort:

    int i = left, j = right;
    CFoundFilesData* pivot = At((i + j) / 2);

    do
    {
        while (CompareFunc(At(i), pivot, byName, bySize, byMD5, TRUE) < 0 && i < right)
            i++;
        while (CompareFunc(pivot, At(j), byName, bySize, byMD5, TRUE) < 0 && j > left)
            j--;

        if (i <= j)
        {
            CFoundFilesData* swap = At(i);
            At(i) = At(j);
            At(j) = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) QuickSort(left, j, byName, bySize, byMD5);
    //  if (i < right) QuickSort(i, right, byName, bySize, byMD5);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                QuickSort(left, j, byName, bySize, byMD5);
                left = i;
                goto LABEL_QuickSort;
            }
            else
            {
                QuickSort(i, right, byName, bySize, byMD5);
                right = j;
                goto LABEL_QuickSort;
            }
        }
        else
        {
            right = j;
            goto LABEL_QuickSort;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_QuickSort;
        }
    }
}

#define DUPLICATES_BUFFER_SIZE 16384 // velikost bufferu pro vypocet MD5

BOOL CDuplicateCandidates::GetMD5Digest(CGrepData* data, CFoundFilesData* file,
                                        int* progress, CQuadWord* readSize, const CQuadWord* totalSize)
{
    // sestavime plnou cestu k souboru
    char fullPath[MAX_PATH];
    lstrcpyn(fullPath, file->Path, MAX_PATH);
    SalPathAppend(fullPath, file->Name, MAX_PATH);

    data->SearchingText->Set(fullPath); // nastavime aktualni soubor

    // otevreme soubor pro cteni a sekvencni pristup
    HANDLE hFile = HANDLES_Q(CreateFile(fullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile != INVALID_HANDLE_VALUE)
    {
        BYTE buffer[DUPLICATES_BUFFER_SIZE];
        MD5 context;
        DWORD read; // pocet skutecne nactenych bajtu
        while (TRUE)
        {
            // nacteme do 'buffer' segment ze souboru 'file'
            if (!ReadFile(hFile, buffer, DUPLICATES_BUFFER_SIZE, &read, NULL))
            {
                // chyba pri cteni souboru
                DWORD err = GetLastError();
                HANDLES(CloseHandle(hFile));

                char buf[MAX_PATH + 100];
                sprintf(buf, LoadStr(IDS_ERROR_READING_FILE2), GetErrorText(err));
                FIND_LOG_ITEM log;
                log.Flags = FLI_ERROR;
                log.Text = buf;
                log.Path = fullPath;
                SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);

                return FALSE;
            }

            // nechce user zastavit operaci?
            if (data->StopSearch)
            {
                HANDLES(CloseHandle(hFile));
                return FALSE;
            }

            // pokud jsme neco nacetli, provedeme update MD5
            if (read > 0)
            {
                context.update(buffer, read);

                // napocitame a zobrazime progress (pokud se zmenila hodnota 'progress')
                *readSize += CQuadWord(read, 0);
                char buff[100];
                int newProgress = *readSize >= *totalSize ? (totalSize->Value == 0 ? 0 : 100) : (int)((*readSize * CQuadWord(100, 0)) / *totalSize).Value;
                if (newProgress != *progress)
                {
                    *progress = newProgress;
                    buff[0] = (BYTE)newProgress; // misto retezce predame primo hodnotu
                    buff[1] = 0;
                    data->SearchingText2->Set(buff); // nastavime total
                }
            }

            // pokud jsme nacetli mene nez buffer, mame hotovo
            if (read != DUPLICATES_BUFFER_SIZE)
                break;
        }
        HANDLES(CloseHandle(hFile));

        context.finalize();
        memcpy((BYTE*)file->Group, context.digest, MD5_DIGEST_SIZE);

        return TRUE;
    }
    else
    {
        // chyba pri otevirani souboru
        DWORD err = GetLastError();

        char buf[MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_ERROR_OPENING_FILE2), GetErrorText(err));
        FIND_LOG_ITEM log;
        log.Flags = FLI_ERROR;
        log.Text = buf;
        log.Path = fullPath;
        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);

        return FALSE;
    }
}

void CDuplicateCandidates::RemoveSingleFiles(BOOL byName, BOOL bySize, BOOL byMD5)
{
    if (Count == 0)
        return;

    CFoundFilesData* lastData = Count > 0 ? At(Count - 1) : NULL;
    int lastDataIndex = Count - 1;
    BOOL lastIsSingle = TRUE;
    int i;
    for (i = Count - 2; i >= 0; i--)
    {
        if (CompareFunc(At(i), lastData, byName, bySize, byMD5, FALSE) == 0)
        {
            lastIsSingle = FALSE;
        }
        else
        {
            if (lastIsSingle)
            {
                // lastData ukazuje na polozku, ktera se vyskytuje pouze jednou; vyradime ji
                Delete(lastDataIndex);
            }
            lastDataIndex = i;
            lastData = At(i);
            lastIsSingle = TRUE;
        }
    }
    if (lastIsSingle)
    {
        // lastData ukazuje na polozku, ktera se vyskytuje pouze jednou; vyradime ji
        Delete(lastDataIndex);
    }
}

void CDuplicateCandidates::SetDifferentFlag(BOOL byName, BOOL bySize, BOOL byMD5)
{
    if (Count == 0)
        return;

    CFoundFilesData* lastData = NULL;
    int different = 0;
    if (Count > 0)
    {
        lastData = At(0);
        lastData->Different = different;
    }
    int i;
    for (i = 1; i < Count; i++)
    {
        CFoundFilesData* data = At(i);
        if (CompareFunc(data, lastData, byName, bySize, byMD5, FALSE) == 0)
        {
            data->Different = different;
        }
        else
        {
            different++;
            if (different > 1)
                different = 0;
            lastData = data;
            lastData->Different = different;
        }
    }
}

void CDuplicateCandidates::SetGroupByDifferentFlag()
{
    if (Count == 0)
        return;

    CFoundFilesData* lastData = NULL;
    DWORD custom = 0;
    if (Count > 0)
    {
        lastData = At(0);
        lastData->Group = custom;
    }
    int i;
    for (i = 1; i < Count; i++)
    {
        CFoundFilesData* data = At(i);
        if (data->Different == lastData->Different)
        {
            data->Group = custom;
        }
        else
        {
            custom++;
            lastData = data;
            lastData->Group = custom;
        }
    }
}

void CDuplicateCandidates::Examine(CGrepData* data)
{
    if (Count == 0)
        return;

    // dostali jsme seznam nalezenych souboru, odpovidajicich kriteriim

    // vytahneme kriteria pro hledani duplicit
    BOOL byName = (data->FindDupFlags & FIND_DUPLICATES_NAME) != 0;
    BOOL bySize = (data->FindDupFlags & FIND_DUPLICATES_SIZE) != 0;
    BOOL byContent = bySize && (data->FindDupFlags & FIND_DUPLICATES_CONTENT) != 0;

    // dohledali jsme, pripravujeme vysledky (muze jeste prijit hledani MD5)
    data->SearchingText->Set(LoadStr(IDS_FIND_DUPS_RESULTS));

    // seradime je podle zvolenych kriterii
    QuickSort(0, Count - 1, byName, bySize, FALSE);

    // vyradime polozky, ktere se vyskytuji pouze jednou
    RemoveSingleFiles(byName, bySize, FALSE);

    CMD5Digest* digest = NULL;
    if (byContent)
    {
        // pro soubory s velikosti nad 0 bajtu budeme pocitat MD5
        // prostor pro MD5 digesty alokujeme najednou

        // urcime pocet souboru s velikosti vetsi nez 0 bajtu
        DWORD count = 0;
        int i;
        for (i = 0; i < Count; i++)
        {
            CFoundFilesData* file = At(i);
            if (file->Size > CQuadWord(0, 0))
                count++;
        }

        if (count > 0)
        {
            // alokujeme prostor pro MD5 digesty v jednom poli
            digest = (CMD5Digest*)malloc(count * sizeof(CMD5Digest));
            if (digest == NULL)
            {
                TRACE_E(LOW_MEMORY);
                return;
            }

            // nasmerujeme ukazatele
            CMD5Digest* iterator = digest;
            for (i = 0; i < Count; i++)
            {
                CFoundFilesData* file = At(i);
                if (file->Size > CQuadWord(0, 0))
                {
                    file->Group = (DWORD_PTR)iterator;
                    iterator++;
                }
                else
                    file->Group = 0;
            }

            // urcime celkovou velikost souboru pro progress
            CQuadWord totalSize(0, 0);
            for (i = 0; i < Count; i++)
                totalSize += At(i)->Size;

            // ziskame MD5 digest souboru
            CQuadWord readSize(0, 0);
            int progress = -1;
            for (i = Count - 1; i >= 0; i--)
            {
                CFoundFilesData* file = At(i);
                if (file->Size > CQuadWord(0, 0))
                {
                    if (!GetMD5Digest(data, file, &progress, &readSize, &totalSize))
                    {
                        if (data->StopSearch)
                        {
                            // uzivatel chce zastavit hledani
                            // podrizneme neprozkoumane polozky
                            int j;
                            for (j = 0; j <= i; j++)
                                Delete(0);
                            break; // ukazeme alespon nalezene duplicity
                        }
                        // doslo k chybe pri cteni souboru, ale uzivatel chce hledat dal
                        // vyradime soubor z kandidatu
                        Delete(i);
                    }
                }
            }

            // dohledali jsme, pripravujeme vysledky
            data->SearchingText->Set(LoadStr(IDS_FIND_DUPS_RESULTS));

            // znovu seradime soubory
            if (Count > 0)
                QuickSort(0, Count - 1, byName, bySize, TRUE);

            // vyradime polozky, ktere se vyskytuji pouze jednou
            RemoveSingleFiles(byName, bySize, TRUE);
        }
    }

    // nastavime bit Different
    SetDifferentFlag(byName, bySize, byContent);

    if (digest != NULL)
        free(digest);

    // do promenne Group priradime cisla zacinajici 0
    // soubory se stejnym bitem Different budou mit stejnou hodnotu
    SetGroupByDifferentFlag();

    // pridame je do listview
    int i;
    for (i = 0; i < Count; i++)
    {
        data->FoundFilesListView->Add(At(i));
        if (!data->FoundFilesListView->IsGood())
        {
            TRACE_E(LOW_MEMORY);
            data->FoundFilesListView->ResetState();
            // podrizneme nepridane polozky
            int j;
            for (j = Count - 1; j >= i; j--)
                Delete(j);
            // odpojime uz pridane
            DetachMembers();
            return;
        }
    }
    DetachMembers();
    return;
}

//*********************************************************************************
//
// CSearchForData
//

void CSearchForData::Set(const char* dir, const char* masksGroup, BOOL includeSubDirs)
{
    strcpy(Dir, dir);
    MasksGroup.SetMasksString(masksGroup);
    IncludeSubDirs = includeSubDirs;
}

//*********************************************************************************
//
// Search engine
//

#define SEARCH_SIZE 10000 // musi byt > nez max. delka retezce

int SearchForward(CGrepData* data, char* txt, int size, int off)
{
    if (size < 0)
        return -1;
    int curOff = off, curSize = min(SEARCH_SIZE, size - curOff);
    int found;
    while (!data->StopSearch)
    {
        if (curSize >= data->SearchData.GetLength())
            found = data->SearchData.SearchForward(txt + curOff, curSize, 0); // find
        else
            break; // not found
        if (found == -1)
        {
            curOff += curSize - data->SearchData.GetLength() + 1;
            curSize = min(SEARCH_SIZE, size - curOff);
        }
        else
            return curOff + found;
    }
    return -1;
}

//
// ****************************************************************************

BOOL TestFileContentAux(BOOL& ok, CQuadWord& fileOffset, const CQuadWord& totalSize,
                        DWORD viewSize, const char* path, char* txt, CGrepData* data)
{
    __try
    {
        if (data->Regular)
        {
            char *beg, *end, *nextBeg, *totalEnd, *endLimit;
            //      BOOL EOL_NULL = TRUE;
            BOOL EOL_CR = data->EOL_CR;
            BOOL EOL_LF = data->EOL_LF;
            BOOL EOL_CRLF = data->EOL_CRLF;
            beg = txt;
            totalEnd = txt + viewSize;

            while (!data->StopSearch && beg < totalEnd)
            {
                end = beg;
                endLimit = beg + GREP_LINE_LEN;
                if (endLimit > totalEnd)
                    endLimit = totalEnd;
                nextBeg = NULL;
                do
                {
                    if (*end > '\r')
                        end++;
                    else
                    {
                        if (*end == '\r')
                        {
                            if (end + 1 < totalEnd && *(end + 1) == '\n' && EOL_CRLF)
                            {
                                nextBeg = end + 2;
                                break;
                            }
                            else
                            {
                                if (EOL_CR &&
                                    (end + 1 < totalEnd ||                              // slo otestovat, ze tam LF neni
                                     !EOL_CRLF ||                                       // LF se nema uvazovat jako EOL
                                     fileOffset + CQuadWord(viewSize, 0) >= totalSize)) // jde o konec souboru
                                {
                                    nextBeg = end + 1;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            if (*end == '\n' && EOL_LF || *end == 0 /*&& EOL_NULL*/)
                            {
                                nextBeg = end + 1;
                                break;
                            }
                        }
                        end++;
                    }
                } while (end < endLimit);
                if (nextBeg == NULL)
                    nextBeg = end;

                if (end == endLimit &&                               // pokud nebyl nalezen znak konce radky
                    fileOffset + CQuadWord(viewSize, 0) < totalSize) // konec souboru neni ve view souboru
                {                                                    // radka muze pokracovat pres okraj aktualniho view souboru
                    fileOffset += CQuadWord(DWORD(beg - txt), 0);
                    return TRUE; // pokracujeme s dalsim view souboru
                }

                // radka beg->end
                if (data->RegExp.SetLine(beg, end))
                {
                    int foundLen, start = 0;

                GREP_REGEXP_NEXT:

                    int found = data->RegExp.SearchForward(start, foundLen);
                    if (found != -1)
                    {
                        if (data->WholeWords)
                        {
                            if ((found == 0 || *(beg + found - 1) != '_' && IsNotAlphaNorNum[*(beg + found - 1)]) &&
                                (found + foundLen == (end - beg) ||
                                 *(beg + found + foundLen) != '_' && IsNotAlphaNorNum[*(beg + found + foundLen)]))
                            {
                                ok = TRUE; // found
                                break;
                            }
                            start = found + 1;
                            if (start < end - beg)
                                goto GREP_REGEXP_NEXT;
                        }
                        else
                        {
                            ok = TRUE; // found
                            break;
                        }
                    }
                }
                else
                {
                    FIND_LOG_ITEM log;
                    log.Flags = FLI_ERROR;
                    log.Text = data->RegExp.GetLastErrorText();
                    log.Path = NULL;
                    SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
                    return FALSE; // dal soubor neprohledavej
                }

                beg = nextBeg;
            }
            // radka konci presne na konci view souboru (muze jit i o konec souboru)
            if (beg >= totalEnd)
                fileOffset += CQuadWord(viewSize, 0); // posuneme offset, aby jsme hledali dale
        }
        else
        {
            int off = 0;
            while (1)
            {
                off = SearchForward(data, txt, viewSize, off);
                if (off != -1)
                {
                    if (data->WholeWords)
                    {
                        if ((fileOffset + CQuadWord(off, 0) == CQuadWord(0, 0) ||                                        // zacatek souboru
                             off > 0 && txt[off - 1] != '_' && IsNotAlphaNorNum[txt[off - 1]]) &&                        // neni na zac. bufferu + pred vzorkem ani znak ani cislo
                            (fileOffset + CQuadWord(off, 0) + CQuadWord(data->SearchData.GetLength(), 0) >= totalSize || // konec souboru
                             (DWORD)(off + data->SearchData.GetLength()) < viewSize &&                                   // neni na konci bufferu
                                 txt[off + data->SearchData.GetLength()] != '_' &&
                                 IsNotAlphaNorNum[txt[off + data->SearchData.GetLength()]])) // za vzorkem ani znak ani cislo
                        {
                            ok = TRUE; // found
                            break;
                        }
                        off++;
                    }
                    else
                    {
                        ok = TRUE; // found
                        break;
                    }
                }
                else
                    break; // not found or terminated
            }
            if (!ok && !data->StopSearch) // nenalezeno ani nepreruseno
            {
                if (fileOffset + CQuadWord(viewSize, 0) < totalSize &&
                    CQuadWord(data->SearchData.GetLength() + 1, 0) < CQuadWord(viewSize, 0))
                {
                    fileOffset = fileOffset + CQuadWord(viewSize, 0) - CQuadWord(data->SearchData.GetLength() + 1, 0);
                }
                else
                    fileOffset = totalSize; // vzorek jiz v souboru byt nemuze
            }
        }
        return TRUE; // pokracuj v hledani (pokud uz nejsme na konci souboru)
    }
    __except (HandleFileException(GetExceptionInformation(), txt, viewSize))
    {
        // chyba v souboru
        FIND_LOG_ITEM log;
        log.Flags = FLI_ERROR;
        log.Text = LoadStr(IDS_FILEREADERROR2);
        log.Path = path;
        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
        ok = FALSE;   // nenasel
        return FALSE; // nepokracovat v hledani
    }
}

BOOL TestFileContent(DWORD sizeLow, DWORD sizeHigh, const char* path, CGrepData* data, BOOL isLink)
{
    CQuadWord totalSize(sizeLow, sizeHigh);
    CQuadWord fileOffset(0, 0);
    DWORD viewSize = 0;

    BOOL ok = FALSE;
    if (totalSize > CQuadWord(0, 0) || isLink)
    {
        DWORD err = ERROR_SUCCESS;
        data->SearchingText->Set(path); // nastavime aktualni soubor
        HANDLE hFile = HANDLES_Q(CreateFile(path, GENERIC_READ,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                            OPEN_EXISTING,
                                            FILE_FLAG_SEQUENTIAL_SCAN,
                                            NULL));
        BOOL getLinkFileSizeErr = FALSE;
        if (hFile != INVALID_HANDLE_VALUE)
        {
            // linky maji nulovou velikost souboru, velikost ciloveho souboru se musi ziskat dodatecne
            if (!isLink || SalGetFileSize(hFile, totalSize, err))
            {
                HANDLE mFile = HANDLES(CreateFileMapping(hFile, NULL, PAGE_READONLY,
                                                         0, 0, NULL));
                if (mFile != NULL)
                {
                    CQuadWord allocGran(AllocationGranularity, 0);
                    while (!data->StopSearch && fileOffset < totalSize)
                    {
                        // zajistime, aby offset odpovidal granularite
                        CQuadWord mapFileOffset(fileOffset);
                        mapFileOffset = (mapFileOffset / allocGran) * allocGran;

                        // napocitame velikost view souboru
                        if (CQuadWord(VOF_VIEW_SIZE, 0) <= totalSize - mapFileOffset)
                            viewSize = VOF_VIEW_SIZE;
                        else
                            viewSize = (DWORD)(totalSize - mapFileOffset).Value;

                        // namapujeme view souboru
                        char* txt = (char*)HANDLES(MapViewOfFile(mFile, FILE_MAP_READ,
                                                                 mapFileOffset.HiDWord, mapFileOffset.LoDWord,
                                                                 viewSize));
                        if (txt != NULL)
                        {
                            // nechame prohlidnout view souboru
                            DWORD diff = (DWORD)(fileOffset - mapFileOffset).Value;
                            BOOL err2 = !TestFileContentAux(ok, fileOffset, totalSize, viewSize - diff,
                                                            path, txt + diff, data);
                            HANDLES(UnmapViewOfFile(txt));
                            if (err2 || ok)
                                break;
                        }
                        else
                            err = GetLastError();
                    }

                    HANDLES(CloseHandle(mFile));
                }
                else
                    err = GetLastError();
            }
            else
                getLinkFileSizeErr = TRUE;
            HANDLES(CloseHandle(hFile));
        }
        else
            err = GetLastError();

        if (err != ERROR_SUCCESS || getLinkFileSizeErr)
        {
            char buf[MAX_PATH + 100];
            sprintf(buf, LoadStr(getLinkFileSizeErr ? IDS_GETLINKTGTFILESIZEERROR : IDS_ERROR_OPENING_FILE2),
                    GetErrorText(err));
            FIND_LOG_ITEM log;
            log.Flags = FLI_ERROR;
            log.Text = buf;
            log.Path = path;
            SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
        }
    }

    return ok;
}

BOOL AddFoundItem(const char* path, const char* name, DWORD sizeLow, DWORD sizeHigh,
                  DWORD attr, const FILETIME* lastWrite, BOOL isDir, CGrepData* data,
                  CDuplicateCandidates* duplicateCandidates)
{
    if (duplicateCandidates != NULL && isDir) // adresare nas pri hledani duplicit nezajimaji
        return TRUE;

    CFoundFilesData* foundData = new CFoundFilesData;
    if (foundData != NULL)
    {
        BOOL good = foundData->Set(path, name,
                                   CQuadWord(sizeLow, sizeHigh),
                                   attr, lastWrite, isDir);
        if (good)
        {
            if (duplicateCandidates == NULL)
            {
                // duplicateCandidates == NULL, pridavame polozku do data->FoundFilesListView
                data->FoundFilesListView->Add(foundData);
                if (!data->FoundFilesListView->IsGood())
                {
                    data->FoundFilesListView->ResetState();
                    delete foundData;
                    foundData = NULL;
                }
                else
                {
                    // po kazdych 100 pridanych polozkach pozadam listview o prekresleni
                    // take po uplynuti 0.5 vteriny o posledniho prekresleni
                    // zaroven volame update v pripade grepovani pro kazdou polozku
                    if (data->FoundFilesListView->GetCount() >= data->FoundVisibleCount + 100 ||
                        GetTickCount() - data->FoundVisibleTick >= 500
                        /* || data->Grep*/) // po pul vterine update staci bohate i pro grepovani
                    {
                        SendMessage(data->HWindow, WM_USER_ADDFILE, 0, 0);
                    }
                    else
                        data->NeedRefresh = TRUE; // nejpozdeji za 0.5 vteriny prekreslime
                }
            }
            else
            {
                // duplicateCandidates != NULL, pridavame polozku do duplicateCandidates
                duplicateCandidates->Add(foundData);
                if (!duplicateCandidates->IsGood())
                {
                    duplicateCandidates->ResetState();
                    delete foundData;
                    foundData = NULL;
                }
            }
        }
        else
        {
            delete foundData;
            foundData = NULL;
        }
    }
    if (foundData == NULL)
    {
        FIND_LOG_ITEM log;
        log.Flags = FLI_ERROR;
        log.Text = LoadStr(IDS_CANTSHOWRESULTS);
        log.Path = NULL;
        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);

        data->StopSearch = TRUE;
        return FALSE;
    }
    return TRUE;
}

// 'dirStack' slouzi k ukladani adresaru pro pozdni grepnuti. Jinak
// by behem hledani v aktualnim adresari doslo k rekurzivnimu hledani
// v podadresarich. Touto obezlickou budou napred nalezeny vsechny
// soubory a adresare, ktere vyhovuji kriteriim a pak bude tato funkce
// zavolana pro vsechny nalezene adresare.
// 'dirStack' je pouze nafukovan. Pokud jsou z nej polozky odstranovany,
// pouze jsou destruovany, ale z pole se neodebereou. Proto je predavana
// promenna 'dirStackCount', ktra obsahuje skutecny pocet polozek
// v poli (vzdy je mensi nebo rovna dirStack->Count).
// Pokud je nedostatek pameti nebo se nehleda v podadresarich,
// je 'dirStack' roven NULL.
// Pokud je 'duplicateCandidates' != NULL, budou se nalezene polozky
// pridavat do tohoto pole misto do data->FoundFilesListView
void SearchDirectory(char (&path)[MAX_PATH], char* end, int startPathLen,
                     CMaskGroup* masksGroup, BOOL includeSubDirs, CGrepData* data,
                     TDirectArray<char*>* dirStack, int dirStackCount,
                     CDuplicateCandidates* duplicateCandidates,
                     CFindIgnore* ignoreList, char (&message)[2 * MAX_PATH])
{
    SLOW_CALL_STACK_MESSAGE6("SearchDirectory(%s, , %d, %s, %d, , , %d, , )", path, startPathLen,
                             masksGroup->GetMasksString(), includeSubDirs, dirStackCount);

    if (ignoreList != NULL && ignoreList->Contains(path, startPathLen))
    {
        FIND_LOG_ITEM log;
        log.Flags = FLI_INFO;
        log.Text = LoadStr(IDS_FINDLOG_SKIP);
        log.Path = path;
        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
        return;
    }

    if ((end - path) + 1 < _countof(path))
        strcpy_s(end, _countof(path) - (end - path), "*");
    else
    {
        FIND_LOG_ITEM log;
        log.Flags = FLI_ERROR;
        log.Text = LoadStr(IDS_TOOLONGNAME);
        log.Path = path;
        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
        return;
    }

    WIN32_FIND_DATA file;
    HANDLE find = HANDLES_Q(FindFirstFile(path, &file));
    if (find != INVALID_HANDLE_VALUE)
    {
        if (end - path > 3)
            *(end - 1) = 0;
        else
            *end = 0;
        data->SearchingText->Set(path); // nastavime aktualni cestu
        if (end - path > 3)
            *(end - 1) = '\\';
        else
            *end = 0;

        int dirStackEnterCount = 0; // pocet polozek pred zacatkem hledani v teto urovni
        if (dirStack != NULL)
            dirStackEnterCount = dirStackCount;
        BOOL testFindNextErr = TRUE;

        do
        {
            BOOL isDir = (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            BOOL ignoreDir = isDir && (lstrcmp(file.cFileName, ".") == 0 || lstrcmp(file.cFileName, "..") == 0);
            if (ignoreDir || (end - path) + lstrlen(file.cFileName) < _countof(path))
            {
                // po nalezeni polozky bez zobrazeni a po uplynuti 0.5 vteriny od posledniho
                // prekresleni pozadame listview o prekresleni
                if (data->NeedRefresh && GetTickCount() - data->FoundVisibleTick >= 500)
                {
                    SendMessage(data->HWindow, WM_USER_ADDFILE, 0, 0);
                    data->NeedRefresh = FALSE;
                }

                if (file.cFileName[0] != 0 && !ignoreDir)
                {
                    // pridavat budeme vsechny soubory a adresare mimo "." a ".."

                    // otestujeme kriteria attributes, size, date, time
                    CQuadWord size(file.nFileSizeLow, file.nFileSizeHigh);
                    if (data->Criteria.Test(file.dwFileAttributes, &size, &file.ftLastWriteTime))
                    {
                        // file name
                        // priponu nechame dohledat ext==NULL
                        if (masksGroup->AgreeMasks(file.cFileName, NULL)) // maska o.k.
                        {
                            BOOL ok;
                            if (data->Grep)
                            {
                                // content
                                if (isDir)
                                    ok = FALSE; // adresar nelze grepovat
                                else
                                {
                                    strcpy_s(end, _countof(path) - (end - path), file.cFileName);
                                    // linky: file.nFileSizeLow == 0 && file.nFileSizeHigh == 0, velikost souboru
                                    // se musi ziskat pres SalGetFileSize() dodatecne
                                    BOOL isLink = (file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                                    ok = TestFileContent(file.nFileSizeLow, file.nFileSizeHigh, path, data, isLink);
                                }
                            }
                            else
                                ok = TRUE;

                            // pokud polozka vyhovuje vsem kriteriim,
                            // pridam ji do seznamu nalezenych polozek
                            if (ok)
                            {
                                if (end - path > 3)
                                    *(end - 1) = 0;
                                else
                                    *end = 0;

                                AddFoundItem(path, file.cFileName, file.nFileSizeLow, file.nFileSizeHigh,
                                             file.dwFileAttributes, &file.ftLastWriteTime, isDir, data,
                                             duplicateCandidates);

                                if (end - path > 3)
                                    *(end - 1) = '\\';
                                else
                                    *end = 0;
                            }
                        }
                    }
                }
                if (isDir && includeSubDirs && !ignoreDir) // directory + neni to "." a ".."
                {
                    int l = (int)strlen(file.cFileName);

                    if ((end - path) + l + 1 /* 1 za backslash */ < _countof(path))
                    {
                        BOOL searchNow = TRUE;

                        if (dirStack != NULL)
                        {
                            // pouze ulozim pro pozdejsi prohledani
                            char* newFileName = new char[l + 1];
                            if (newFileName != NULL)
                            {
                                memmove(newFileName, file.cFileName, l + 1);
                                if (dirStackCount < dirStack->Count)
                                {
                                    // nemusime priradi polozku - mame misto
                                    dirStack->At(dirStackCount) = newFileName;
                                    dirStackCount++;
                                    searchNow = FALSE;
                                }
                                else
                                {
                                    // musime alokovat novou polozku v poli
                                    dirStack->Add(newFileName);
                                    if (dirStack->IsGood())
                                    {
                                        dirStackCount++;
                                        searchNow = FALSE;
                                    }
                                    else
                                    {
                                        dirStack->ResetState();
                                        delete[] newFileName;
                                    }
                                }
                            }
                        }

                        if (searchNow)
                        {
                            // neni pamet - nebudeme pouzivat dirStack
                            strcpy_s(end, _countof(path) - (end - path), file.cFileName);
                            strcat_s(end, _countof(path) - (end - path), "\\");
                            l++;
                            SearchDirectory(path, end + l, startPathLen, masksGroup, includeSubDirs, data, NULL,
                                            0, duplicateCandidates, ignoreList, message);
                        }
                    }
                    else
                    {
                        FIND_LOG_ITEM log;
                        log.Flags = FLI_ERROR;
                        log.Text = LoadStr(IDS_TOOLONGNAME);
                        strcpy_s(end, _countof(path) - (end - path), file.cFileName);
                        log.Path = path;
                        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
                    }
                }
            }
            else // too long file-name
            {
                FIND_LOG_ITEM log;
                log.Flags = FLI_ERROR;
                log.Text = LoadStr(IDS_TOOLONGNAME);
                *end = 0;
                strcpy_s(message, path);
                strcat_s(message, file.cFileName);
                log.Path = message;
                SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
            }
            if (data->StopSearch)
            {
                testFindNextErr = FALSE;
                break;
            }
        } while (FindNextFile(find, &file));
        DWORD err = GetLastError();
        HANDLES(FindClose(find));

        if (testFindNextErr && err != ERROR_NO_MORE_FILES)
        {
            if (end - path > 3)
                *(end - 1) = 0;
            else
                *end = 0;

            sprintf(message, LoadStr(IDS_DIRERRORFORMAT), GetErrorText(err));
            FIND_LOG_ITEM log;
            log.Flags = FLI_ERROR;
            log.Text = message;
            log.Path = path;
            SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);

            if (end - path > 3)
                *(end - 1) = '\\';
            else
                *end = 0;
        }

        // doprohledam adresare
        if (dirStack != NULL)
        {
            int i;
            for (i = dirStackEnterCount; i < dirStackCount; i++)
            {
                char* newFileName = (char*)dirStack->At(i);
                if (!data->StopSearch) // muze byt nastavena behem SearchDirectory
                {
                    strcpy_s(end, _countof(path) - (end - path), newFileName);
                    strcat_s(end, _countof(path) - (end - path), "\\");
                    SearchDirectory(path, end + strlen(end), startPathLen, masksGroup, includeSubDirs, data,
                                    dirStack, dirStackCount, duplicateCandidates, ignoreList, message);
                }
            }
            // a uvolnim data z teto urovne
            for (i = dirStackEnterCount; i < dirStackCount; i++)
                delete[] dirStack->At(i);
        }
    }
    else
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
        {
            if (end - path > 3)
                *(end - 1) = 0;
            else
                *end = 0;

            sprintf(message, LoadStr(IDS_DIRERRORFORMAT), GetErrorText(err));

            FIND_LOG_ITEM log;
            log.Flags = FLI_ERROR | FLI_IGNORE;
            log.Text = message;
            log.Path = path;
            SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);

            if (end - path > 3)
                *(end - 1) = '\\';
            else
                *end = 0;
        }
    }
    *end = 0;
}

void RefineData(CMaskGroup* masksGroup, CGrepData* data)
{
    int refineCount = data->FoundFilesListView->GetDataForRefineCount();
    int oldProgress = -1;

    int i;
    for (i = 0; i < refineCount && !data->StopSearch; i++)
    {
        if (!data->Grep)
        {
            // pokud se negrepuje, budeme zobrazovat procenta
            int progress = (int)((double)i / (double)refineCount * 100.0);
            if (progress != oldProgress)
            {
                char buf[20];
                sprintf(buf, "%d%%", progress);
                data->SearchingText->Set(buf); // nastavime aktualni cestu
                oldProgress = progress;
            }
        }

        CFoundFilesData* refineData = data->FoundFilesListView->GetDataForRefine(i);

        // otestujeme kriteria
        BOOL ok = TRUE;

        // attributes, size, date, time
        if (ok && !data->Criteria.Test(refineData->Attr, &refineData->Size, &refineData->LastWrite))
            ok = FALSE;

        // file name (priponu nechame dohledat ext==NULL)
        if (ok && !masksGroup->AgreeMasks(refineData->Name, NULL))
            ok = FALSE;

        // content
        if (ok && data->Grep)
        {
            if (refineData->IsDir)
                ok = FALSE; // adresar nelze grepovat
            else
            {
                char fullPath[MAX_PATH];
                strcpy(fullPath, refineData->Path);
                if (fullPath[strlen(fullPath) - 1] != '\\')
                    strcat(fullPath, "\\");
                strcat(fullPath, refineData->Name);
                // linky: refineData->Size == 0, velikost souboru se musi ziskat pres SalGetFileSize() dodatecne
                BOOL isLink = (refineData->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0; // velikost == 0, velikost souboru se musi ziskat pres SalGetFileSize()
                ok = TestFileContent(refineData->Size.LoDWord, refineData->Size.HiDWord,
                                     fullPath, data, isLink);
            }
        }

        // pokud je refine==1 (intersect) a polozka vyhovuje, pridame ji
        // pokud je refine==2 (subtract) a polozka nevyhovuje, pridame ji
        if (data->Refine == 1 && ok ||
            data->Refine == 2 && !ok)
        {
            AddFoundItem(refineData->Path, refineData->Name,
                         refineData->Size.LoDWord, refineData->Size.HiDWord,
                         refineData->Attr, &refineData->LastWrite,
                         refineData->IsDir, data, NULL);
        }
    }
}

unsigned GrepThreadFBody(void* ptr)
{
    CALL_STACK_MESSAGE1("GrepThreadFBody()");

    SetThreadNameInVCAndTrace("Grep");
    TRACE_I("Begin");
    //  Sleep(200);  // trochu casu pro prekresleni dialogu...
    CGrepData* data = (CGrepData*)ptr;
    data->NeedRefresh = FALSE;
    data->Criteria.PrepareForTest();
    char path[MAX_PATH];
    char* end;
    if (data->Refine != 0)
    {
        if (data->Data->Count > 0)
        {
            strcpy_s(path, data->Data->At(0)->Dir);
            int len = (int)strlen(path);
            if (path[len - 1] != '\\')
            {
                strcat_s(path, "\\");
                len++;
            }
            end = path + len;
            CMaskGroup* mg = &data->Data->At(0)->MasksGroup;
            int errorPos;
            if (mg->PrepareMasks(errorPos))
            {
                RefineData(mg, data);
            }
            else
            {
                TRACE_E("PrepareMasks failed errorPos=" << errorPos);
                data->StopSearch = TRUE;
            }
        }
    }
    else
    {
        // pokud hledame duplikaty, data se primarne umistuji do tohoto pole
        // po prohledani vsech adresaru se pole seradi (podle jmena nebo podle velikosti)
        // pokud se kontroluje obsah, napocitaji se pro sporne pripady MD5
        // potom se data predaji do FoundFilesListView
        CDuplicateCandidates* duplicateCandidates = NULL;
        if (data->FindDuplicates)
        {
            duplicateCandidates = new CDuplicateCandidates;
            if (duplicateCandidates == NULL)
            {
                TRACE_E(LOW_MEMORY); // algoritmus pobezi i bez stacku
                data->StopSearch = TRUE;
            }
        }

        if (!data->StopSearch)
        {
            int i;
            for (i = 0; i < data->Data->Count; i++)
            {
                strcpy_s(path, data->Data->At(i)->Dir);
                int len = (int)strlen(path);
                if (path[len - 1] != '\\')
                {
                    strcat_s(path, "\\");
                    len++;
                }
                end = path + len;
                CMaskGroup* mg = &data->Data->At(i)->MasksGroup;
                int errorPos;
                if (!mg->PrepareMasks(errorPos))
                {
                    TRACE_E("PrepareMasks failed errorPos=" << errorPos);
                    data->StopSearch = TRUE;
                    break;
                }

                BOOL includeSubDirs = data->Data->At(i)->IncludeSubDirs;
                TDirectArray<char*>* dirStack = NULL; // popis u funkce SearchDirectory
                if (includeSubDirs)
                {
                    dirStack = new TDirectArray<char*>(1000, 1000);
                    if (dirStack == NULL)
                        TRACE_E(LOW_MEMORY); // algoritmus pobezi i bez stacku
                }

                // udelame si lokalni kopii ignore listu, stejne je treba ho predkousat
                // a jako bonus muzeme uzivatele ignore list nechat editovat v prubehu hledani
                CFindIgnore* ignoreList = new CFindIgnore;
                if (ignoreList == NULL)
                    TRACE_E(LOW_MEMORY); // algoritmus pobezi i bez ignore listu
                else
                {
                    if (!ignoreList->Prepare(&FindIgnore))
                    {
                        delete ignoreList;
                        ignoreList = NULL;
                    }
                }

                char message[2 * MAX_PATH];
                SearchDirectory(path, end, (int)(end - path), mg, includeSubDirs, data, dirStack, 0,
                                duplicateCandidates, ignoreList, message);

                if (ignoreList != NULL)
                    delete ignoreList;

                if (dirStack != NULL)
                    delete dirStack;
                if (data->StopSearch)
                    break;
            }
        }
        if (duplicateCandidates != NULL)
        {
            if (!data->StopSearch)
                duplicateCandidates->Examine(data);
            delete duplicateCandidates;
        }
    }

    data->SearchStopped = data->StopSearch;
    SendMessage(data->HWindow, WM_USER_ADDFILE, 0, 0); // aktualizace listview
    PostMessage(data->HWindow, WM_COMMAND, IDC_FIND_STOP, 0);
    TRACE_I("End");
    return 0;
}

unsigned GrepThreadFEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return GrepThreadFBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread Grep: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI GrepThreadF(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return GrepThreadFEH(param);
}

//*********************************************************************************
//
// CSearchingString
//

CSearchingString::CSearchingString()
{
    HANDLES(InitializeCriticalSection(&Section));
    Buffer[0] = 0;
    BaseLen = 0;
    Dirty = FALSE;
}

CSearchingString::~CSearchingString()
{
    HANDLES(DeleteCriticalSection(&Section));
}

void CSearchingString::SetBase(const char* buf)
{
    HANDLES(EnterCriticalSection(&Section));
    lstrcpyn(Buffer, buf, MAX_PATH + 50);
    BaseLen = (int)strlen(Buffer);
    HANDLES(LeaveCriticalSection(&Section));
}

void CSearchingString::Set(const char* buf)
{
    HANDLES(EnterCriticalSection(&Section));
    lstrcpyn(Buffer + BaseLen, buf, MAX_PATH + 50 - BaseLen);
    Dirty = TRUE;
    HANDLES(LeaveCriticalSection(&Section));
}

void CSearchingString::Get(char* buf, int bufSize)
{
    HANDLES(EnterCriticalSection(&Section));
    lstrcpyn(buf, Buffer, bufSize);
    HANDLES(LeaveCriticalSection(&Section));
}

void CSearchingString::SetDirty(BOOL dirty)
{
    HANDLES(EnterCriticalSection(&Section));
    Dirty = dirty;
    HANDLES(LeaveCriticalSection(&Section));
}

BOOL CSearchingString::GetDirty()
{
    BOOL r;
    HANDLES(EnterCriticalSection(&Section));
    r = Dirty;
    HANDLES(LeaveCriticalSection(&Section));
    return r;
}

//*********************************************************************************
//
// Find Dialog Thread functions
//

struct CTFDData
{
    CFindDialog* FindDialog;
    BOOL Success;
};

unsigned ThreadFindDialogMessageLoopBody(void* parameter)
{
    CALL_STACK_MESSAGE1("ThreadFindDialogMessageLoopBody()");
    BOOL ok;

    { // tento blok je zde kvuli korektnimu zavolani destruktoru pred volanim _end_thread() (viz nize)
        SetThreadNameInVCAndTrace("FindDialog");
        TRACE_I("Begin");
        CTFDData* data = (CTFDData*)parameter;
        CFindDialog* findDialog = data->FindDialog;
        findDialog->SetZeroOnDestroy(&findDialog); // pri WM_DESTROY bude ukazatel vynulovan
                                                   // obrana pred pristupem na neplatny ukazatel
                                                   // z message loopy po destrukci okna

        data->Success = TRUE;

        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        if (findDialog->Create() != NULL)
        {
            SetForegroundWindow(findDialog->HWindow);
            UpdateWindow(findDialog->HWindow);
        }
        else
            data->Success = FALSE;

        ok = data->Success;
        data = NULL;                  // dale jiz neni platne
        SetEvent(FindDialogContinue); // pustime dal hlavni thread

        if (ok) // pokud se okno vytvorilo, spustime aplikacni smycku
        {
            CALL_STACK_MESSAGE1("ThreadFindDialogMessageLoopBody::message_loop");

            MSG msg;
            HWND findDialogHWindow = findDialog->HWindow; // kvuli wm_quit pri ktere uz okno nebude alokovane
            BOOL haveMSG = FALSE;                         // FALSE pokud se ma volat GetMessage() v podmince cyklu
            while (haveMSG || GetMessage(&msg, NULL, 0, 0))
            {
                haveMSG = FALSE;
                if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN) &&
                    msg.wParam != VK_MENU && msg.wParam != VK_CONTROL && msg.wParam != VK_SHIFT)
                    SetCurrentToolTip(NULL, 0); // zhasneme tooltip
                // zajistime zaslani zprav do naseho menu (obchazime tim potrebu hooku pro klavesnici)
                if (findDialog == NULL || !findDialog->IsMenuBarMessage(&msg))
                {
                    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && findDialog != NULL)
                        findDialog->SetProcessingEscape(TRUE);
                    if (findDialog == NULL ||
                        (!TranslateAccelerator(findDialogHWindow, FindDialogAccelTable, &msg)) &&
                            (!findDialog->ManageHiddenShortcuts(&msg)) &&
                            (!IsDialogMessage(findDialogHWindow, &msg)))
                    {
                        TranslateMessage(&msg); // aby se nenageneroval WM_CHAR -> zpusobilo pipnuti pri Cancel
                        DispatchMessage(&msg);
                    }
                    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && findDialog != NULL)
                        findDialog->SetProcessingEscape(FALSE);
                }

                if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                        break;      // ekvivalent situace, kdy GetMessage() vraci FALSE
                    haveMSG = TRUE; // mame zpravu, jdeme ji zpracovat (bez volani GetMessage())
                }
                else // pokud ve fronte neni zadna message, provedeme Idle processing
                {
                    if (findDialog != NULL)
                        findDialog->OnEnterIdle();
                }
            }
        }

        TRACE_I("End");
    }

#ifndef CALLSTK_DISABLE
    CCallStack::ReleaseBeforeExitThread(); // pred exitem treadu musime uvolnit call-stack data (ale nadale jsme v chranene sekci - generovani naseho bug reportu)
#endif                                     // CALLSTK_DISABLE
    _endthreadex(ok ? 0 : 1);
    return ok ? 0 : 1; // dead code (pro spokojenost prekladace)
}

unsigned ThreadFindDialogMessageLoopEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadFindDialogMessageLoopBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread FindDialogMessageLoop: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadFindDialogMessageLoop(void* param)
{
    CCallStack stack;
    return ThreadFindDialogMessageLoopEH(param);
}

BOOL OpenFindDialog(HWND hCenterAgainst, const char* initPath)
{
    CALL_STACK_MESSAGE3("OpenFindDialog(0x%p, %s)", hCenterAgainst, initPath);

    HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    CFindDialog* findDlg = new CFindDialog(hCenterAgainst, initPath);
    if (findDlg != NULL && findDlg->IsGood())
    {
        CTFDData data;
        data.FindDialog = findDlg;

        DWORD ThreadID;
        HANDLE loop = HANDLES(CreateThread(NULL, 0, ThreadFindDialogMessageLoop, &data, 0, &ThreadID));
        if (loop == NULL)
        {
            TRACE_E("Unable to start ThreadFindDialogMessageLoop thread.");
            goto ERROR_TFD_CREATE;
        }

        WaitForSingleObject(FindDialogContinue, INFINITE); // pockame az se thread nahodi
        if (!data.Success)
        {
            HANDLES(CloseHandle(loop));
            goto ERROR_TFD_CREATE;
        }
        AddAuxThread(loop); // pridame thread mezi existujici viewry (kill pri exitu)
        SetCursor(hOldCur);
        return TRUE;
    }
    else
    {
        TRACE_E(LOW_MEMORY);

    ERROR_TFD_CREATE:

        if (findDlg != NULL)
            delete findDlg;

        SetCursor(hOldCur);
        return FALSE;
    }
    SetCursor(hOldCur);
    return TRUE;
}
