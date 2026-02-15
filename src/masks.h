// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// functions used by the quick-search feature
BOOL IsQSWildChar(char ch);
void PrepareQSMask(char* mask, const char* src);
BOOL AgreeQSMask(const char* filename, BOOL hasExtension, const char* mask, BOOL wholeString, int& offset);

// wildcards '*' (any string) + '?' (any character) <+ '#' (a digit) if extendedMode==TRUE>
void PrepareMask(char* mask, const char* src);                                                // converts the mask into the chosen format
BOOL AgreeMask(const char* filename, const char* mask, BOOL hasExtension, BOOL extendedMode); // does it match the mask ??

// adjusts the name according to the mask and stores the result in buffer 'buffer' of size 'bufSize'
// 'name' is the name to adjust; 'mask' is the mask (unmodified - do not call PrepareMask on it)
// returns 'buffer' on success (even if the name was truncated because the buffer was too small),
// otherwise NULL; NOTE: behaves like the "copy" command in Win2K
char* MaskName(char* buffer, int bufSize, const char* name, const char* mask);

//*****************************************************************************
//
// CMaskGroup
//
// Life cycle:
//   1) In the constructor or SetMasksString method, provide a group of masks.
//   2) Call PrepareMasks to build internal data; if it fails, display the error
//      location and after fixing the mask, return to step (2).
//   3) Call AgreeMasks at any time to check whether a name matches the mask group.
//   4) If SetMasksString is called again, continue from step (2).
//
// Mask:
//   '?' - any character
//   '*' - any string (including empty)
//   '#' - any digit (only if 'extendedMode'==TRUE)
//
//   Examples:
//     *     - all names
//     *.*   - all names
//     *.exe - names with the "exe" extension
//     *.t?? - names with an extension starting with 't' and having two additional characters
//     *.r## - names with an extension starting with 'r' and two additional digits
//
// Mask group:
//   Masks are separated by ';' character. The '|' character can also be used as a separator
//   that has a special meaning. All masks following '|' are treated inversely,
//   meaning AgreeMasks returns FALSE if a name matches them.
//   The '|' separator may appear only once in the mask group and must be followed by at least one mask.
//   If nothing precedes '|', a "*" mask is automatically inserted.
//
//   Examples:
//     *.txt;*.cpp - all names with the txt or cpp extension
//     *.h*|*.html - all names whose extension starts with 'h' but not names with the extension "html"
//     |*.txt      - all names with an extension other than "txt"
//

#define MASK_OPTIMIZE_NONE 0      // no optimization
#define MASK_OPTIMIZE_ALL 1       // mask satisfies all requests (*.* or *)
#define MASK_OPTIMIZE_EXTENSION 2 // mask is in form (*.xxxx) where xxxx is the extension

struct CMaskItemFlags
{
    unsigned Optimize : 7; // MASK_OPTIMIZE_xxx
    unsigned Exclude : 1;  // if 1, this is an exclude mask; otherwise, it is an include mask
                           // exclude masks are stored before include masks in PreparedMasks array
};

struct CMasksHashEntry
{
    CMaskItemFlags* Mask;  // internal mask representation, see CMaskItemFlags for the format
    CMasksHashEntry* Next; // next entry with the same hash
};

class CMaskGroup
{
protected:
    char MasksString[MAX_GROUPMASK];   // mask group passed in the constructor or in PrepareMasks
    TDirectArray<char*> PreparedMasks; // internal mask representation; for the format see CMaskItemFlags - may not contain all masks, some may be in MasksHashArray
    BOOL NeedPrepare;                  // is it necessary to call the PrepareMasks method before using 'PreparedMasks'?
    BOOL ExtendedMode;

    CMasksHashEntry* MasksHashArray; // if not NULL, it is a hash array containing all masks with MASK_OPTIMIZE_EXTENSION format (only those with CMaskItemFlags::Exclude==0)
    int MasksHashArraySize;          // size of MasksHashArray (twice the number of stored masks)

public:
    CMaskGroup();
    CMaskGroup(const char* masks, BOOL extendedMode = FALSE);
    ~CMaskGroup();
    void Release();

    CMaskGroup& operator=(const CMaskGroup& s);

    // sets the mask string 'masks'; (maximum length including the terminating null is MAX_GROUPMASK)
    void SetMasksString(const char* masks, BOOL extendedMode = FALSE);

    // returns the string of masks; 'buffer' is a buffer that is at least MAX_GROUPMASK long
    const char* GetMasksString();

    // returns the mask string released for writing; 'buffer' is a buffer that is at least MAX_GROUPMASK long
    char* GetWritableMasksString();

    BOOL GetExtendedMode();

    // Converts the mask group passed to the constructor or by the SetMasksString method
    // into an internal representation that allows calling AgreeMasks. If 'extendedMode' is TRUE, the
    // mask syntax is extended with '#' representing any digit.
    // Returns TRUE on success or FALSE on error. In case of an error,
    // 'errorPos' contains the index of the character (in the provided mask group) that caused the error.
    // If memory is low, 'errorPos' is set to 0.
    // If masksString == NULL, CMaskGroup::MasksString is used; otherwise the
    // provided 'masksString' is used (in that case, AgreeMasks can be called —
    // CMaskGroup::MasksString is ignored).
    BOOL PrepareMasks(int& errorPos, const char* masksString = NULL);

    // Determines whether 'fileName' matches the mask group.
    // NOTE: 'fileName' must not be a full path, only in the form name.ext
    // fileExt must point either to the terminator of fileName or to the extension (if it exists)
    // if fileExt == NULL, the extension will be searched for - this is slower
    BOOL AgreeMasks(const char* fileName, const char* fileExt);

protected:
    // releases the hash array MasksHashArray
    void ReleaseMasksHashArray();
};
