// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

// following variable was used up to Altap Salamander 2.5,
// where we switched to CFilterCriteria with its Save/Load
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

    // we should also clear the comboboxes of open windows
    if (!dataOnly)
    {
        FindDialogQueue.BroadcastMessage(WM_USER_CLEARHISTORY, 0, 0);
    }
}

void ReleaseFind()
{
    ClearFindHistory(TRUE); // we only release data
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
    // optimize registry size by storing only non-default values;
    // before saving, we need to clear the key we’re going to save into
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
        // conversion of old values

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
            if (!Items[i]->Enabled) // save only if it is FALSE
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
            item->Enabled = TRUE; // saved only if it is FALSE
        if (Configuration.ConfigVersion < 32)
        {
            // users were confused that this folder was not searched
            // so we keep it listed but uncheck the checkbox
            // anyone interested can manually enable it
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
        if (item->Enabled) // we are only interested in enabled items
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
    // full path
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        // startPathLen is the path length entered in the Find dialog (search root);
        // only its subpaths are ignored, see https://forum.altap.cz/viewtopic.php?f=7&t=7434
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
                if (m != NULL) // found
                {
                    if ((m - path) + item->Len > startPathLen) // is it a subpath? then ignore it
                        return TRUE;
                    m++; // look for another occurrence, maybe it will be in a subpath
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
    if (path[len - 1] == '\\') // compare without trailing backslashes
        len--;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CFindIgnoreItem* item = Items[i];
        int itemLen = (int)strlen(item->Path);
        if (itemLen < 1)
            continue;
        if (item->Path[itemLen - 1] == '\\') // compare without a trailing backslash
            itemLen--;
        if (len != itemLen)
            continue;
        if (StrNICmp(path, item->Path, len) == 0)
        {
            item->Enabled = TRUE; // always enable this item
            return TRUE;
        }
    }
    // not found -- add it
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
// Container for CFoundFilesData when searching for duplicate files.
// 1) In the first phase, all files matching the Find criteria are added
//    to the CDuplicateCandidates object using the Add method.
// 2) Then the Examine() method is called which sorts the array using data->FindDupFlags criteria. If file contents
//    are compared, MD5 digests are calculated for potentially identical files.
//    Then, the array is sorted again and single files are removed so
//    Only files that appear at least twice remain in the array.
//    These get a Group variable so that sets can be distinguished in the result window.
//

class CDuplicateCandidates : public TIndirectArray<CFoundFilesData>
{
public:
    CDuplicateCandidates() : TIndirectArray<CFoundFilesData>(2000, 4000) {}

    // - loading/calculating MD5 digests
    // - removing single files
    // - setting the Group variable
    // - setting the Different flag
    void Examine(CGrepData* data);

protected:
    // compares two records using criteria byName, bySize and byMD5
    // byPath is a criterium with the lowest priority and is used only for clearer output
    int CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, BOOL byName, BOOL bySize, BOOL byMD5, BOOL byPath);

    // sort stored files by byName, bySize and byMD5 criteria
    void QuickSort(int left, int right, BOOL byName, BOOL bySize, BOOL byMD5);

    // goes through all stored items and uses CompareFunc to identify those that
    // appear only once; those are then removed from the array
    // before calling this method, the array must be sorted with QuickSort
    void RemoveSingleFiles(BOOL byName, BOOL bySize, BOOL byMD5);

    // goes through all stored items and uses CompareFunc assign them
    // to groups; Alternates the Different bit for the groups  (0, 1, 0, 1, 0, 1, ...)
    // before calling this method, the array must be sorted with QuickSort
    void SetDifferentFlag(BOOL byName, BOOL bySize, BOOL byMD5);

    // goes through all stored items and uses the Different flag to assign
    // Group values; groups are numbered increasingly (0, 1, 2, 3, 4, 5, ...)
    void SetGroupByDifferentFlag();

    // compute MD5 from the file 'file'
    // 'progress' is the numeric value shown in the status bar (optimized so we do not
    // update the same porgress repeatedly)
    // 'readSize' holds the number of bytes read so far across all files
    // 'totalSize' is the total number of bytes of all files for which the MD5 digest
    // will be determined
    // the method returns TRUE, if the MD5 value was successfully read; the digest is stored at
    // (BYTE*)data->Group
    // the method returns FALSE on read errors or when the user aborts the operation
    // (then, the variable data->StopSearch is set to TRUE)
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

    // the following "nice" code was replaced by a version that saves stack space (max. log(N) recursion depth)
    //  if (left < j) QuickSort(left, j, byName, bySize, byMD5);
    //  if (i < right) QuickSort(i, right, byName, bySize, byMD5);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // both halves must be sorted: send the smaller half to recursion and handle the other via "goto"
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

#define DUPLICATES_BUFFER_SIZE 16384 // buffer size for MD5 calculation

BOOL CDuplicateCandidates::GetMD5Digest(CGrepData* data, CFoundFilesData* file,
                                        int* progress, CQuadWord* readSize, const CQuadWord* totalSize)
{
    // build full path to the file
    char fullPath[MAX_PATH];
    lstrcpyn(fullPath, file->Path, MAX_PATH);
    SalPathAppend(fullPath, file->Name, MAX_PATH);

    data->SearchingText->Set(fullPath); // set the current file

    // open the file for reading with sequential access
    HANDLE hFile = HANDLES_Q(CreateFile(fullPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile != INVALID_HANDLE_VALUE)
    {
        BYTE buffer[DUPLICATES_BUFFER_SIZE];
        MD5 context;
        DWORD read; // number of bytes that were actually read
        while (TRUE)
        {
            // read a segment from a file 'file' into 'buffer'
            if (!ReadFile(hFile, buffer, DUPLICATES_BUFFER_SIZE, &read, NULL))
            {
                // error reading the file
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

            // does the user want to stop the operation?
            if (data->StopSearch)
            {
                HANDLES(CloseHandle(hFile));
                return FALSE;
            }

            // if anything was read, update the MD5
            if (read > 0)
            {
                context.update(buffer, read);

                // compute and display progress (if the 'progress' value changed)
                *readSize += CQuadWord(read, 0);
                char buff[100];
                int newProgress = *readSize >= *totalSize ? (totalSize->Value == 0 ? 0 : 100) : (int)((*readSize * CQuadWord(100, 0)) / *totalSize).Value;
                if (newProgress != *progress)
                {
                    *progress = newProgress;
                    buff[0] = (BYTE)newProgress; // pass the numeric value directly instead of a string
                    buff[1] = 0;
                    data->SearchingText2->Set(buff); // update the total progress
                }
            }

            // if fewer bytes were read than the buffer size, we are done
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
        // error occured while opening the file
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
                // lastData points to an item that occurs only once; remove it
                Delete(lastDataIndex);
            }
            lastDataIndex = i;
            lastData = At(i);
            lastIsSingle = TRUE;
        }
    }
    if (lastIsSingle)
    {
        // lastData points to an item that occurs only once; remove it
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

    // we received a list of found files matching the criteria

    // extract criteria for duplicate search
    BOOL byName = (data->FindDupFlags & FIND_DUPLICATES_NAME) != 0;
    BOOL bySize = (data->FindDupFlags & FIND_DUPLICATES_SIZE) != 0;
    BOOL byContent = bySize && (data->FindDupFlags & FIND_DUPLICATES_CONTENT) != 0;

    // search completed, preparing results (MD5 computation may still follow)
    data->SearchingText->Set(LoadStr(IDS_FIND_DUPS_RESULTS));

    // sort them according to selected criteria
    QuickSort(0, Count - 1, byName, bySize, FALSE);

    // remove items that occur only once
    RemoveSingleFiles(byName, bySize, FALSE);

    CMD5Digest* digest = NULL;
    if (byContent)
    {
        // for files larger than 0 bytes we'll compute MD5
        // allocate memory for MD5 digests at once

        // determine the number of files with size greater than 0 bytes
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
            // allocate memory for MD5 digests in one array
            digest = (CMD5Digest*)malloc(count * sizeof(CMD5Digest));
            if (digest == NULL)
            {
                TRACE_E(LOW_MEMORY);
                return;
            }

            // set up the pointers
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

            // determine total file size for progress
            CQuadWord totalSize(0, 0);
            for (i = 0; i < Count; i++)
                totalSize += At(i)->Size;

            // retrieve the MD5 digest of files
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
                            // the user wants to stop searching
                            // trim the unprocessed items
                            int j;
                            for (j = 0; j <= i; j++)
                                Delete(0);
                            break; // show at least the duplicates that have been already found
                        }
                        // an error occurred during reading the file but the user wants to continue
                        // exclude the file from candidates
                        Delete(i);
                    }
                }
            }

            // search finished, preparing results
            data->SearchingText->Set(LoadStr(IDS_FIND_DUPS_RESULTS));

            // sort the files again
            if (Count > 0)
                QuickSort(0, Count - 1, byName, bySize, TRUE);

            // remove items that occur only once
            RemoveSingleFiles(byName, bySize, TRUE);
        }
    }

    // set the Different bit
    SetDifferentFlag(byName, bySize, byContent);

    if (digest != NULL)
        free(digest);

    // assign numbers starting from 0 to the Group variable
    // files with the same Different bit will share the same value
    SetGroupByDifferentFlag();

    // add them to the listview
    int i;
    for (i = 0; i < Count; i++)
    {
        data->FoundFilesListView->Add(At(i));
        if (!data->FoundFilesListView->IsGood())
        {
            TRACE_E(LOW_MEMORY);
            data->FoundFilesListView->ResetState();
            // cut off items that were not added
            int j;
            for (j = Count - 1; j >= i; j--)
                Delete(j);
            // detach the already added ones
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

#define SEARCH_SIZE 10000 // must be greater than the maximum string length

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
                                    (end + 1 < totalEnd ||                              // it was able to test that there is no LF there
                                     !EOL_CRLF ||                                       // LF should not be considered an EOL
                                     fileOffset + CQuadWord(viewSize, 0) >= totalSize)) // it is the end of the file
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

                if (end == endLimit &&                               // if no line ending character was found
                    fileOffset + CQuadWord(viewSize, 0) < totalSize) // the end of the file is not in the file view
                {                                                    // the line can continue beyond the boundary of the current view of the file
                    fileOffset += CQuadWord(DWORD(beg - txt), 0);
                    return TRUE; // continue with the next view segment
                }

                // line beg->end
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
                    return FALSE; // do not search this file further
                }

                beg = nextBeg;
            }
            // line ends exactly at the end of the view segment (may also be the end of the file)
            if (beg >= totalEnd)
                fileOffset += CQuadWord(viewSize, 0); // advance the offset to continue searching
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
                        if ((fileOffset + CQuadWord(off, 0) == CQuadWord(0, 0) ||                                        // beginning of the file
                             off > 0 && txt[off - 1] != '_' && IsNotAlphaNorNum[txt[off - 1]]) &&                        // not at the start of the buffer and no letter or digit before the pattern
                            (fileOffset + CQuadWord(off, 0) + CQuadWord(data->SearchData.GetLength(), 0) >= totalSize || // end of the file
                             (DWORD)(off + data->SearchData.GetLength()) < viewSize &&                                   // not at the end of the buffer
                                 txt[off + data->SearchData.GetLength()] != '_' &&
                                 IsNotAlphaNorNum[txt[off + data->SearchData.GetLength()]])) // no letter or digit after the pattern
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
            if (!ok && !data->StopSearch) // not found and not interrupted
            {
                if (fileOffset + CQuadWord(viewSize, 0) < totalSize &&
                    CQuadWord(data->SearchData.GetLength() + 1, 0) < CQuadWord(viewSize, 0))
                {
                    fileOffset = fileOffset + CQuadWord(viewSize, 0) - CQuadWord(data->SearchData.GetLength() + 1, 0);
                }
                else
                    fileOffset = totalSize; // the pattern cannot be in the file anymore
            }
        }
        return TRUE; // continue searching (unless we are at the end of the file)
    }
    __except (HandleFileException(GetExceptionInformation(), txt, viewSize))
    {
        // file error
        FIND_LOG_ITEM log;
        log.Flags = FLI_ERROR;
        log.Text = LoadStr(IDS_FILEREADERROR2);
        log.Path = path;
        SendMessage(data->HWindow, WM_USER_ADDLOG, (WPARAM)&log, 0);
        ok = FALSE;   // not found
        return FALSE; // do not continue searching
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
        data->SearchingText->Set(path); // set the current file
        HANDLE hFile = HANDLES_Q(CreateFile(path, GENERIC_READ,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                            OPEN_EXISTING,
                                            FILE_FLAG_SEQUENTIAL_SCAN,
                                            NULL));
        BOOL getLinkFileSizeErr = FALSE;
        if (hFile != INVALID_HANDLE_VALUE)
        {
            // links have zero file size; size of the target file must be obtained separately
            if (!isLink || SalGetFileSize(hFile, totalSize, err))
            {
                HANDLE mFile = HANDLES(CreateFileMapping(hFile, NULL, PAGE_READONLY,
                                                         0, 0, NULL));
                if (mFile != NULL)
                {
                    CQuadWord allocGran(AllocationGranularity, 0);
                    while (!data->StopSearch && fileOffset < totalSize)
                    {
                        // ensure the offset matches the granularity
                        CQuadWord mapFileOffset(fileOffset);
                        mapFileOffset = (mapFileOffset / allocGran) * allocGran;

                        // calculate the size of the view segment
                        if (CQuadWord(VOF_VIEW_SIZE, 0) <= totalSize - mapFileOffset)
                            viewSize = VOF_VIEW_SIZE;
                        else
                            viewSize = (DWORD)(totalSize - mapFileOffset).Value;

                        // map the file view
                        char* txt = (char*)HANDLES(MapViewOfFile(mFile, FILE_MAP_READ,
                                                                 mapFileOffset.HiDWord, mapFileOffset.LoDWord,
                                                                 viewSize));
                        if (txt != NULL)
                        {
                            // let the file view be examined
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
    if (duplicateCandidates != NULL && isDir) // directories are irrelevant to us when searching for duplicates
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
                // duplicateCandidates == NULL, adding the item to data->FoundFilesListView
                data->FoundFilesListView->Add(foundData);
                if (!data->FoundFilesListView->IsGood())
                {
                    data->FoundFilesListView->ResetState();
                    delete foundData;
                    foundData = NULL;
                }
                else
                {
                    // request a listview redraw after every 100 added items
                    // also after 0.5 seconds has passed since the last redraw
                    // we also call update for each item when grepping
                    if (data->FoundFilesListView->GetCount() >= data->FoundVisibleCount + 100 ||
                        GetTickCount() - data->FoundVisibleTick >= 500
                        /* || data->Grep*/) // a half-second update interval is enough even for grepping
                    {
                        SendMessage(data->HWindow, WM_USER_ADDFILE, 0, 0);
                    }
                    else
                        data->NeedRefresh = TRUE; // we will redraw at latest after 0.5 second
                }
            }
            else
            {
                // duplicateCandidates != NULL, adding the item to duplicateCandidates
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

// 'dirStack' stores directories for late grepping. Otherwise,
// during searching in the current directory, recursive searching in subdirectories would occur. With this
// trick all files and directories matching the criteria are found first and
// then this function is called for all discovered directories.
// 'dirStack' only grows. When items are removed from it, they are just destroyed but
// not removed from the array, therefore the variable 'dirStackCount' holds the
// actual number of items in the array (always less than or equal to dirStack->Count).
// If memory is low or subdirectories are not searched,
// 'dirStack' is NULL.
// If 'duplicateCandidates' != NULL, found items will be added to this array
// instead of data->FoundFilesListView
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
        data->SearchingText->Set(path); // set the current path
        if (end - path > 3)
            *(end - 1) = '\\';
        else
            *end = 0;

        int dirStackEnterCount = 0; // number of items before starting the search at this level
        if (dirStack != NULL)
            dirStackEnterCount = dirStackCount;
        BOOL testFindNextErr = TRUE;

        do
        {
            BOOL isDir = (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            BOOL ignoreDir = isDir && (lstrcmp(file.cFileName, ".") == 0 || lstrcmp(file.cFileName, "..") == 0);
            if (ignoreDir || (end - path) + lstrlen(file.cFileName) < _countof(path))
            {
                // after finding an item without displaying it and once 0.5 s have passed since the last redraw,
                // we request the listview to redraw
                if (data->NeedRefresh && GetTickCount() - data->FoundVisibleTick >= 500)
                {
                    SendMessage(data->HWindow, WM_USER_ADDFILE, 0, 0);
                    data->NeedRefresh = FALSE;
                }

                if (file.cFileName[0] != 0 && !ignoreDir)
                {
                    // add all files and directories except "." and ".."

                    // test the criteria attributes, size, date and time
                    CQuadWord size(file.nFileSizeLow, file.nFileSizeHigh);
                    if (data->Criteria.Test(file.dwFileAttributes, &size, &file.ftLastWriteTime))
                    {
                        // file name
                        // let the extension be resolved if ext==NULL
                        if (masksGroup->AgreeMasks(file.cFileName, NULL)) // mask is OK
                        {
                            BOOL ok;
                            if (data->Grep)
                            {
                                // content
                                if (isDir)
                                    ok = FALSE; // a directory cannot be grepped
                                else
                                {
                                    strcpy_s(end, _countof(path) - (end - path), file.cFileName);
                                    // links: file.nFileSizeLow == 0 && file.nFileSizeHigh == 0, the file size
                                    // must be additionally obtained via SalGetFileSize()
                                    BOOL isLink = (file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                                    ok = TestFileContent(file.nFileSizeLow, file.nFileSizeHigh, path, data, isLink);
                                }
                            }
                            else
                                ok = TRUE;

                            // if the item matches all criteria,
                            // add it to the list of found items
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
                if (isDir && includeSubDirs && !ignoreDir) // directory + not "." or ".."
                {
                    int l = (int)strlen(file.cFileName);

                    if ((end - path) + l + 1 /* 1 za backslash */ < _countof(path))
                    {
                        BOOL searchNow = TRUE;

                        if (dirStack != NULL)
                        {
                            // just store for later search
                            char* newFileName = new char[l + 1];
                            if (newFileName != NULL)
                            {
                                memmove(newFileName, file.cFileName, l + 1);
                                if (dirStackCount < dirStack->Count)
                                {
                                    // no need to assign an item - we have space
                                    dirStack->At(dirStackCount) = newFileName;
                                    dirStackCount++;
                                    searchNow = FALSE;
                                }
                                else
                                {
                                    // we must allocate a new item in the array
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
                            // out of memory - we will not use dirStack
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

        // search through directories
        if (dirStack != NULL)
        {
            int i;
            for (i = dirStackEnterCount; i < dirStackCount; i++)
            {
                char* newFileName = (char*)dirStack->At(i);
                if (!data->StopSearch) // may be set during SearchDirectory
                {
                    strcpy_s(end, _countof(path) - (end - path), newFileName);
                    strcat_s(end, _countof(path) - (end - path), "\\");
                    SearchDirectory(path, end + strlen(end), startPathLen, masksGroup, includeSubDirs, data,
                                    dirStack, dirStackCount, duplicateCandidates, ignoreList, message);
                }
            }
            // and release data from this level
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
            // if grepping is disabled, show progress in percent
            int progress = (int)((double)i / (double)refineCount * 100.0);
            if (progress != oldProgress)
            {
                char buf[20];
                sprintf(buf, "%d%%", progress);
                data->SearchingText->Set(buf); // set the current path
                oldProgress = progress;
            }
        }

        CFoundFilesData* refineData = data->FoundFilesListView->GetDataForRefine(i);

        // test the criteria
        BOOL ok = TRUE;

        // attributes, size, date, time
        if (ok && !data->Criteria.Test(refineData->Attr, &refineData->Size, &refineData->LastWrite))
            ok = FALSE;

        // file name (let the extension be resolved if ext==NULL)
        if (ok && !masksGroup->AgreeMasks(refineData->Name, NULL))
            ok = FALSE;

        // content
        if (ok && data->Grep)
        {
            if (refineData->IsDir)
                ok = FALSE; // a directory cannot be grepped
            else
            {
                char fullPath[MAX_PATH];
                strcpy(fullPath, refineData->Path);
                if (fullPath[strlen(fullPath) - 1] != '\\')
                    strcat(fullPath, "\\");
                strcat(fullPath, refineData->Name);
                // links: refineData->Size == 0, the file size must be additionally obtained via SalGetFileSize()
                BOOL isLink = (refineData->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0; // size == 0, the file size must be obtained via SalGetFileSize()
                ok = TestFileContent(refineData->Size.LoDWord, refineData->Size.HiDWord,
                                     fullPath, data, isLink);
            }
        }

        // if refine==1 (intersect) and the item matches, add it
        // if refine==2 (subtract) and the item does not match, add it
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
    //  Sleep(200);  // give the dialog a moment to redraw...
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
        // if we search for duplicates, data are primarily placed into this array
        // after scanning all directories, the array is sorted (by name or by size)
        // if content is checked, MD5 is calculated for ambiguous cases
        // afterwards the data are passed to FoundFilesListView
        CDuplicateCandidates* duplicateCandidates = NULL;
        if (data->FindDuplicates)
        {
            duplicateCandidates = new CDuplicateCandidates;
            if (duplicateCandidates == NULL)
            {
                TRACE_E(LOW_MEMORY); // the algorithm will run even without the stack
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
                TDirectArray<char*>* dirStack = NULL; // see description at SearchDirectory
                if (includeSubDirs)
                {
                    dirStack = new TDirectArray<char*>(1000, 1000);
                    if (dirStack == NULL)
                        TRACE_E(LOW_MEMORY); // the algorithm will run even without the stack
                }

                // create a local copy of the ignore list since it has to be processed anyway
                // and as a bonus the user can edit the ignore list while searching
                CFindIgnore* ignoreList = new CFindIgnore;
                if (ignoreList == NULL)
                    TRACE_E(LOW_MEMORY); // the algorithm will run even without the ignore list
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
    SendMessage(data->HWindow, WM_USER_ADDFILE, 0, 0); // update the listview
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
        TerminateProcess(GetCurrentProcess(), 1); // harder exit (this call still performs some operations)
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

    { // this block ensures destructors are called properly before calling _end_thread() (see below)
        SetThreadNameInVCAndTrace("FindDialog");
        TRACE_I("Begin");
        CTFDData* data = (CTFDData*)parameter;
        CFindDialog* findDialog = data->FindDialog;
        findDialog->SetZeroOnDestroy(&findDialog); // on WM_DESTROY the pointer is zeroed
                                                   // protection against accessing an invalid pointer
                                                   // from the message loop after the window is destroyed

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
        data = NULL;                  // no longer valid afterwards
        SetEvent(FindDialogContinue); // let the main thread continue

        if (ok) // if the window was created, run the message loop
        {
            CALL_STACK_MESSAGE1("ThreadFindDialogMessageLoopBody::message_loop");

            MSG msg;
            HWND findDialogHWindow = findDialog->HWindow; // because of WM_QUIT, when the window will no longer be allocated
            BOOL haveMSG = FALSE;                         // FALSE means GetMessage() should be called in the loop condition
            while (haveMSG || GetMessage(&msg, NULL, 0, 0))
            {
                haveMSG = FALSE;
                if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN) &&
                    msg.wParam != VK_MENU && msg.wParam != VK_CONTROL && msg.wParam != VK_SHIFT)
                    SetCurrentToolTip(NULL, 0); // turn off the tooltip
                // ensure messages reach our menu (avoids the need for a keyboard hook)
                if (findDialog == NULL || !findDialog->IsMenuBarMessage(&msg))
                {
                    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && findDialog != NULL)
                        findDialog->SetProcessingEscape(TRUE);
                    if (findDialog == NULL ||
                        (!TranslateAccelerator(findDialogHWindow, FindDialogAccelTable, &msg)) &&
                            (!findDialog->ManageHiddenShortcuts(&msg)) &&
                            (!IsDialogMessage(findDialogHWindow, &msg)))
                    {
                        TranslateMessage(&msg); // prevent generating WM_CHAR -> would cause a beep on Cancel
                        DispatchMessage(&msg);
                    }
                    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && findDialog != NULL)
                        findDialog->SetProcessingEscape(FALSE);
                }

                if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                        break;      // equivalent to the situation when GetMessage() is returning FALSE
                    haveMSG = TRUE; // a message is pending; process it without calling GetMessage()
                }
                else // if there is no message in the queue, perform Idle processing
                {
                    if (findDialog != NULL)
                        findDialog->OnEnterIdle();
                }
            }
        }

        TRACE_I("End");
    }

#ifndef CALLSTK_DISABLE
    CCallStack::ReleaseBeforeExitThread(); // before exiting the thread, we must release call-stack data (still in protected section - generating our bug report)
#endif                                     // CALLSTK_DISABLE
    _endthreadex(ok ? 0 : 1);
    return ok ? 0 : 1; // dead code to keep the compiler happy
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
        TerminateProcess(GetCurrentProcess(), 1); // harder exit (this call still performs some operations)
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

        WaitForSingleObject(FindDialogContinue, INFINITE); // wait until the thread starts
        if (!data.Success)
        {
            HANDLES(CloseHandle(loop));
            goto ERROR_TFD_CREATE;
        }
        AddAuxThread(loop); // add the thread among existing viewers (killed on exit)
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
