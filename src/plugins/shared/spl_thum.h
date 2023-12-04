// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_thum) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//
// ****************************************************************************
// CSalamanderThumbnailMakerAbstract
//

// informace o obrazku, ze ktereho generujeme thumbnail, tyto flagy se pouzivaji
// v CSalamanderThumbnailMakerAbstract::SetParameters():
#define SSTHUMB_MIRROR_HOR 1                                            // obrazek je potreba horizontalne zrcadlit
#define SSTHUMB_MIRROR_VERT 2                                           // obrazek je potreba vertikalne zrcadlit
#define SSTHUMB_ROTATE_90CW 4                                           // obrazek je potreba otocit o 90 stupnu ve smeru hodinovych rucicek
#define SSTHUMB_ROTATE_180 (SSTHUMB_MIRROR_VERT | SSTHUMB_MIRROR_HOR)   // obrazek je potreba otocit o 180 stupnu
#define SSTHUMB_ROTATE_90CCW (SSTHUMB_ROTATE_90CW | SSTHUMB_ROTATE_180) // obrazek je potreba otocit o 90 stupnu proti smeru hodinovych rucicek
// obrazek je v horsi kvalite nebo mensi nez je potreba, Salamander po dokonceni prvniho kola
// ziskavani "rychlych" thumbnailu zkusi pro tento obrazek ziskat "kvalitni" thumbnail
#define SSTHUMB_ONLY_PREVIEW 8

class CSalamanderThumbnailMakerAbstract
{
public:
    // nastaveni parametru zpracovani obrazku pri tvorbe thumbnailu; nutne volat
    // jako prvni metodu tohoto rozhrani; 'picWidth' a 'picHeight' jsou rozmery
    // zpracovavaneho obrazku (v bodech); 'flags' je kombinace flagu SSTHUMB_XXX,
    // ktera udava informace o obrazku predavanem v parametru 'buffer' v metode
    // ProcessBuffer; vraci TRUE, pokud se podarilo alokovat buffery pro zmensovani
    // a je mozne nasledne volat ProcessBuffer; pokud vrati FALSE, doslo k chybe
    // a je treba ukoncit nacitani thumbnailu
    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags) = 0;

    // zpracuje cast obrazku v bufferu 'buffer' (zpracovavana cast obrazku je ulozena
    // po radcich shora dolu, body na radcich jsou ulozeny zleva doprava, kazdy bod
    // reprezentuje 32-bitova hodnota, ktera je slozena z tri bytu s barvami R+G+B a
    // ctvrteho bytu, ktery se ignoruje); rozlisujeme dva typy zpracovani: kopie
    // obrazku do vysledneho thumbnailu (nepresahuje-li velikost zpracovavaneho obrazku
    // velikost thumbnailu) a zmenseni obrazku do thumbnailu (obrazek vetsi nez
    // thumbnail); 'buffer' je pouzit pouze pro cteni; 'rowsCount' urcuje kolik radku
    // obrazku je v bufferu;
    // je-li'buffer' NULL, berou se data z vlastniho bufferu (plugin ziska pres GetBuffer);
    // vraci TRUE pokud ma plugin pokracovat s nacitanim obrazku, vraci-li FALSE,
    // tvorba thumbnailu je hotova (byl zpracovan cely obrazek) nebo se ma co
    // nejdrive prerusit (napr. uzivatel zmenil cestu v panelu, thumbnail tedy jiz
    // neni potreba)
    //
    // POZOR: pokud je spustena metoda CPluginInterfaceForThumbLoader::LoadThumbnail,
    // je blokovana zmena cesty v panelu. Z toho duvodu je treba predavat a hlavne
    // nacitat vetsi obrazky po castech a testovatovanim navratove hodnoty
    // metody ProcessBuffer overovat, zda se nacitani nema prerusit.
    // Pokud je treba provest casove narocnejsi operace pred volanim metody SetParameters
    // nebo pred volanim ProcessBuffer, je behem teto doby nutne obcas volat GetCancelProcessing.
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount) = 0;

    // vraci vlastni buffer o velikosti potrebne k ulozeni 'rowsCount' radku obrazku
    // (4 * 'rowsCount' * 'picWidth' bytu); je-li objekt v chybovem stavu (po volani
    // SetError), vraci NULL;
    // plugin nesmi dealokovat ziskany buffer (dealokuje se v Salamanderovi automaticky)
    virtual void* WINAPI GetBuffer(int rowsCount) = 0;

    // oznameni chyby pri ziskavani obrazku (thumbnail je povazovan za vadny
    // a nepouzije se), ostatni metody tohoto rozhrani budou od chvile volani
    // SetError uz jen vracet chyby (GetBuffer a SetParameters) nebo preruseni
    // prace (ProcessBuffer)
    virtual void WINAPI SetError() = 0;

    // vraci TRUE, pokud ma plugin preprusit nacitani thumbnailu
    // vraci FALSE, pokud ma plugin pokracovat s nacitanim obrazku
    //
    // metodu lze volat pred i po volani metody SetParameters
    //
    // slouzi k detekci pozadavku na preruseni v pripadech, kdy plugin
    // potrebuje vykonat casove narocne operace jeste pred volanim SetParameters
    // nebo v pripade, kdy plugin potrebuje obrazek predrenderovat, tedy po volani
    // SetParameters, ale pred volanim ProcessBuffer
    virtual BOOL WINAPI GetCancelProcessing() = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForThumbLoaderAbstract
//

class CPluginInterfaceForThumbLoaderAbstract
{
#ifdef INSIDE_SALAMANDER
private: // ochrana proti nespravnemu primemu volani metod (viz CPluginInterfaceForThumbLoaderEncapsulation)
    friend class CPluginInterfaceForThumbLoaderEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // nacte thumbnail pro soubor 'filename'; 'thumbWidth' a 'thumbHeight' jsou
    // rozmery pozadovaneho thumbnailu; 'thumbMaker' je rozhrani algoritmu pro
    // tvorbu thumbnailu (umi prijmout hotovy thumbnail nebo ho vyrobit zmensenim
    // obrazku); vraci TRUE pokud je format souboru 'filename' znamy, pokud vrati
    // FALSE, Salamander zkusi nacist thumbnail pomoci jineho pluginu; chybu pri
    // ziskavani thumbnailu (napr. chybu cteni souboru) plugin hlasi pomoci
    // rozhrani 'thumbMaker' - viz metoda SetError; 'fastThumbnail' je TRUE v prvnim
    // kole cteni thumbnailu - cilem je vratit thumbnail co nejrychleji (klidne
    // v horsi kvalite nebo mensi nez je potreba), v druhem kole cteni thumbnailu
    // (jen pokud se v prvnim kole nastavi flag SSTHUMB_ONLY_PREVIEW) je
    // 'fastThumbnail' FALSE - cilem je vratit kvalitni thumbnail
    // omezeni: jelikoz se vola z threadu pro nacitani ikon (neni to hlavni thread), lze z
    // CSalamanderGeneralAbstract pouzivat jen metody, ktere lze volat z libovolneho threadu
    //
    // Doporucene schema implementace:
    //   - pokusit se otevrit obrazek
    //   - pokud se nepodari, vratit FALSE
    //   - extrahovat rozmery obrazku
    //   - predat je do Salamandera pres thumbMaker->SetParameters
    //   - pokud vrati FALSE, uklid a odchod (nepovedlo se alokovat buffery)
    //   - SMYCKA
    //     - nacist cast dat z obrazku
    //     - poslat je do Salamandera pres thumbMaker->ProcessBuffer
    //     - pokud vrati FALSE, uklid a odchod (preruseni z duvodu zmeny cesty)
    //     - pokracovat ve SMYCCE, dokud nebude cely obrazek predan
    //   - uklid a odchod
    virtual BOOL WINAPI LoadThumbnail(const char* filename, int thumbWidth, int thumbHeight,
                                      CSalamanderThumbnailMakerAbstract* thumbMaker,
                                      BOOL fastThumbnail) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_thum)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
