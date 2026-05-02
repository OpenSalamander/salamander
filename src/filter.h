// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//*****************************************************************************
//
// CFilterCriteria
//

enum CFilterCriteriaTimeModeEnum
{
    fctmIgnore, // ignore date and time
    fctmDuring, // modified within xxx seconds/minutes/hours/days/weeks/months/years (from now on)
    fctmFromTo  // modified from - to
};

// the order must match the 'sizeUnits' array
enum CFilterCriteriaSizeUnitsEnum
{
    fcsuBytes,
    fcsuKB,
    fcsuMB,
    fcsuGB,
    fcsuTB,
    fcsuPB,
    fcsuEB,
};

// the order must match the 'timeUnits' array
enum CFilterCriteriaTimeUnitsEnum
{
    fctuSeconds,
    fctuMinutes,
    fctuHours,
    fctuDays,
    fctuWeeks,
    fctuMonths,
    fctuYears
};

class CFilterCriteria
{
protected:
    // Attributes

    // individual bits of the arrays correspond to the FILE_ATTRIBUTE_xxx constants
    DWORD AttributesMask;  // if the bit is zero, the checkbox is grayed out and the attribute does not matter
    DWORD AttributesValue; // if the corresponding bit in 'AttributesMask' equals 1, this bit indicates the attribute state

    // Size Min/Max
    int UseMinSize;
    CQuadWord MinSize;
    CFilterCriteriaSizeUnitsEnum MinSizeUnits;

    int UseMaxSize;
    CQuadWord MaxSize;
    CFilterCriteriaSizeUnitsEnum MaxSizeUnits;

    // Date & Time
    CFilterCriteriaTimeModeEnum TimeMode;

    //   during
    CQuadWord DuringTime;
    CFilterCriteriaTimeUnitsEnum DuringTimeUnits;

    //   from - to
    int UseFromDate;
    int UseFromTime;
    unsigned __int64 From; // local time

    int UseToDate;
    int UseToTime;
    unsigned __int64 To; // local time

    // variables precomputed in PrepareForTest
    BOOL UseMinTime;          // check MinTime
    unsigned __int64 MinTime; // local time
    BOOL UseMaxTime;          // check MaxTime
    unsigned __int64 MaxTime; // local time
    CQuadWord MinSizeBytes;   // 'MinSize' converted to bytes
    CQuadWord MaxSizeBytes;   // 'MaxSize' converted to bytes
    BOOL NeedPrepare;         // just a guard flag: if TRUE, PrepareForTest must be called before calling Test

public:
    CFilterCriteria();

    // resets values to the initial state without touching 'Name' and 'Masks'
    // used for the Reset button in the dialog
    void Reset();

    // PrepareForTest must be called before calling Test method
    void PrepareForTest();
    // returns TRUE if the file/directory parameters satisfy the criteria
    // 'modified' is UTC and will be converted to local time
    BOOL Test(DWORD attributes, const CQuadWord* size, const FILETIME* modified);

    // fills 'buffer' with a description of the set values if they differ from the default ones
    // (after calling the Reset method). 'maxLen' specifies the maximum length of the 'buffer' string.
    // the function returns TRUE if any value differs from the default
    BOOL GetAdvancedDescription(char* buffer, int maxLen, BOOL& dirty);

    // save/load to/from the Windows Registry
    // !!! WARNING: saving is optimized-only changed values are stored; before saving into a key, this key must be cleared first
    BOOL Save(HKEY hKey);
    BOOL Load(HKEY hKey);
    BOOL LoadOld(HKEY hKey); // loads data from configuration version 13 and older

    friend class CFilterCriteriaDialog;
};

//*********************************************************************************
//
// CFilterCriteriaDialog
//

class CFilterCriteriaDialog : public CCommonDialog
{
protected:
    BOOL EnableDirectory;
    CFilterCriteria* Data;

public:
    CFilterCriteriaDialog(HWND hParent, CFilterCriteria* data, BOOL enableDirectory);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void FillUnits(int editID, int comboID, int* units, BOOL appendSizes);
    void EnableControls();
};
