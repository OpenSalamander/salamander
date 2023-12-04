// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CFilterCriteria
//

enum CFilterCriteriaTimeModeEnum
{
    fctmIgnore, // ignorovat datum a cas
    fctmDuring, // modifikovano behem xxx vterin/minut/hodin/dnu/tydnu/mesicu/roku (a od soucasnoti dal)
    fctmFromTo  // modifikovano od - do
};

// poradi musi korespondovat s polem 'sizeUnits'
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

// poradi musi korespondovat s polem 'timeUnits'
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

    // jednotlive bity poli odpobidaji konstantam FILE_ATTRIBUTE_xxx
    DWORD AttributesMask;  // pokud je bit roven nule, checkbox bude sedivy; na atributu nezalezi
    DWORD AttributesValue; // pokud odpovidajici bit v 'AttributesMask' roven 1, tento bit vyjadruje stav

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

    // promenne predpocitane v PrepareForTest
    BOOL UseMinTime;          // kontrolovat MinTime
    unsigned __int64 MinTime; // local time
    BOOL UseMaxTime;          // kontrolovat MaxTime
    unsigned __int64 MaxTime; // local time
    CQuadWord MinSizeBytes;   // 'MinSize' prevedena na bajty
    CQuadWord MaxSizeBytes;   // 'MaxSize' prevedena na bajty
    BOOL NeedPrepare;         // pouze hlidaci pes, je-li TRUE, je treba zavolat PrepareForTest pred volanim Test

public:
    CFilterCriteria();

    // nastavi hodnoty na uvodni stav, nesaha na 'Name' a 'Masks'
    // slouzi pro tlacitko Reset v dialogu
    void Reset();

    // PrepareForTest je treba zavolat pred volanim metody Test
    void PrepareForTest();
    // vraci TRUE, pokud predane parametry souboru/adresare vyhovuji kriteriim
    // 'modified' je UTC, prevedeme jej na local time
    BOOL Test(DWORD attributes, const CQuadWord* size, const FILETIME* modified);

    // do retezce 'buffer' naplni popis nastavenych hodnot, jsou-li
    // jine nez implicitni (po zavolani Reset metody)
    // 'maxLen' udava maximalni delku retezce 'buffer'
    // diry vraci TRUE, pokud je nektera z hodnot jina nez implicitni
    BOOL GetAdvancedDescription(char* buffer, int maxLen, BOOL& dirty);

    // save/load do/z Windows Registry
    // !!! POZOR: ukladani optimalizovano, ukladaji se pouze zmenene hodnoty; pred ulozenim do klice musi by tento klic napred promazan
    BOOL Save(HKEY hKey);
    BOOL Load(HKEY hKey);
    BOOL LoadOld(HKEY hKey); // nacte data z configu verze 13 a mene

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
