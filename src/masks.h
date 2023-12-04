// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// funkce pro quick-search vyhledavani
BOOL IsQSWildChar(char ch);
void PrepareQSMask(char* mask, const char* src);
BOOL AgreeQSMask(const char* filename, BOOL hasExtension, const char* mask, BOOL wholeString, int& offset);

// wildcards '*' (lib. retezec) + '?' (lib. znak) <+ '#' (cislice) je-li extendedMode==TRUE>
void PrepareMask(char* mask, const char* src);                                                // provede prevod do zvoleneho formatu
BOOL AgreeMask(const char* filename, const char* mask, BOOL hasExtension, BOOL extendedMode); // odpovida masce ?

// upravi jmeno dle masky; vysledek ulozi do bufferu 'buffer' o velikosti 'bufSize';
// 'name' je upravovane jmeno; 'mask' je maska (neupravena - nevolat pro ni PrepareMask);
// pri uspechu vraci 'buffer' (i pokud doslo k oriznuti jmena z duvodu maleho bufferu),
// jinak NULL; POZNAMKA: chova se podle prikazu "copy" ve Win2K
char* MaskName(char* buffer, int bufSize, const char* name, const char* mask);

//*****************************************************************************
//
// CMaskGroup
//
// Zivotni cyklus:
//   1) V konstruktoru nebo metode SetMasksString predame skupinu masek.
//   2) Zavolame PrepareMasks pro stavbu vnitrnich dat; v pripade neuspechu
//      zobrazime chybne misto a po oprave masky se vracime do bodu (2)
//   3) Libovolne volame AgreeMasks pro zjisteni, zda jmeno odpovida skupine masek.
//   4) Po pripadnem zavolani SetMasksString pokracujeme od (2)
//
// Maska:
//   '?' - libovolny znak
//   '*' - libovolny retezec (i prazdny)
//   '#' - libovolna cislice (pouze je-li 'extendedMode'==TRUE)
//
//   Priklady:
//     *     - vsechna jmena
//     *.*   - vsechna jmena
//     *.exe - jmena s priponou "exe"
//     *.t?? - jmena s priponou zacinajici znakem 't' a obsahujici jeste dva libovolne znaky
//     *.r## - jmena s priponou zacinajici znakem 'r' a obsahujici jeste dve libovolne cislice
//
// Skupina masek:
//   Pro oddeleni masek pouzivame znak ';'. Jako oddelovac lze pouzit znak '|', ktery
//   ma vsak specialni vyznam. Vsechny nasledujici masky budou zpracovavany inverzne,
//   tedy pokud jim bude odpovidat jmeno, metoda AgreeMasks vrati FALSE.
//   Oddelovac '|' muze byt ve skupine masek obsazen pouze jednou a musi za nim nasledovat
//   alespon jedna maska. Pokud pred '|' neni zadna maska, bude automaticky vlozena
//   maska "*".
//
//   Priklady:
//     *.txt;*.cpp - vsechna jmena s priponou txt nebo cpp
//     *.h*|*.html - vsechna jmena s priponou zacinajici znakem 'h', ale ne jmena s priponou "html"
//     |*.txt      - vsechna jmena s jinou priponou nez "txt"
//

#define MASK_OPTIMIZE_NONE 0      // zadna optimalizace
#define MASK_OPTIMIZE_ALL 1       // maska vyhovuje vsem pozadavkum (*.* nebo *)
#define MASK_OPTIMIZE_EXTENSION 2 // maska je ve tvaru (*.xxxx), kde xxxx je pripona

struct CMaskItemFlags
{
    unsigned Optimize : 7; // MASK_OPTIMIZE_xxx
    unsigned Exclude : 1;  // pokud je 1, jde o exlude masku, jinak o include masku
                           // exlude masky jsou v poli PreparedMasks zarazene pred
                           // include masky
};

struct CMasksHashEntry
{
    CMaskItemFlags* Mask;  // interni reprezentace masky, format viz CMaskItemFlags
    CMasksHashEntry* Next; // dalsi polozka se stejnym hashem
};

class CMaskGroup
{
protected:
    char MasksString[MAX_GROUPMASK];   // skupinka masek predana v konstruktoru nebo v PrepareMasks
    TDirectArray<char*> PreparedMasks; // interni reprezentace masek, format viz CMaskItemFlags - nemusi obsahovat vsechny masky, nektere muzou byt v MasksHashArray
    BOOL NeedPrepare;                  // je treba volat metodu PrepareMasks pred pouzitim 'PreparedMasks'?
    BOOL ExtendedMode;

    CMasksHashEntry* MasksHashArray; // neni-li NULL, jde o hashovaci pole obsahujici vsechny masky s formatem MASK_OPTIMIZE_EXTENSION (jen s CMaskItemFlags::Exclude==0)
    int MasksHashArraySize;          // velikost MasksHashArray (dvojnasobek poctu ulozenych masek)

public:
    CMaskGroup();
    CMaskGroup(const char* masks, BOOL extendedMode = FALSE);
    ~CMaskGroup();
    void Release();

    CMaskGroup& operator=(const CMaskGroup& s);

    // nastavi retezec masek 'masks'; (max. delka vcetne nuly na konci je MAX_GROUPMASK)
    void SetMasksString(const char* masks, BOOL extendedMode = FALSE);

    // vraci retezec masek; 'buffer' je buffer o delce alespon MAX_GROUPMASK
    const char* GetMasksString();

    // vraci retezec masek uvolneny pro zapis; 'buffer' je buffer o delce alespon MAX_GROUPMASK
    char* GetWritableMasksString();

    BOOL GetExtendedMode();

    // Prevede skupinu masek predanych konstruktoru nebo metodou SetMasksString
    // na vnitrni reprezentaci, ktera umozni volani AgreeMasks. Pokud je 'extendedMode'==TRUE,
    // bude syntaxe masky rozsirena o znak '#' reprezentujici libovolnou cislici.
    // Vraci TRUE v pripade uspechu nebo FALSE v pripade chyby. V pripade chyby
    // obsahuje 'errorPos' index znaku (z predane skupiny masek), ktery chybu zpusobil.
    // Pri nedostatku pameti se vraci 'errorPos' 0.
    // pokud je masksString == NULL, pouzije se CMaskGroup::MasksString, jinak se
    // pouzije 'masksString' (nasledne je mozne volat AgreeMasks - CMaskGroup::MasksString
    // se v tomto pripade ignoruje)
    BOOL PrepareMasks(int& errorPos, const char* masksString = NULL);

    // Zjisti, zda 'fileName' odpovida skupine masek.
    // POZOR: 'fileName' nesmi byt full path, ale pouze ve tvaru name.ext
    // fileExt musi ukazovat bud na terminator od fileName nebo na priponu (pokud existuje)
    // pokud je fileExt == NULL, bude pripona dohledana - pozor - pomalejsi
    BOOL AgreeMasks(const char* fileName, const char* fileExt);

protected:
    // uvolni hashovaci pole MasksHashArray
    void ReleaseMasksHashArray();
};
