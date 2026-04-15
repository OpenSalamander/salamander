// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_thum) // so the structures are independent of the current packing alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//
// ****************************************************************************
// CSalamanderThumbnailMakerAbstract
//

// information about the image from which the thumbnail is generated; these flags are used
// in CSalamanderThumbnailMakerAbstract::SetParameters():
#define SSTHUMB_MIRROR_HOR 1                                            // image must be mirrored horizontally
#define SSTHUMB_MIRROR_VERT 2                                           // image must be mirrored vertically
#define SSTHUMB_ROTATE_90CW 4                                           // image must be rotated 90 degrees clockwise
#define SSTHUMB_ROTATE_180 (SSTHUMB_MIRROR_VERT | SSTHUMB_MIRROR_HOR)   // image must be rotated 180 degrees
#define SSTHUMB_ROTATE_90CCW (SSTHUMB_ROTATE_90CW | SSTHUMB_ROTATE_180) // image must be rotated 90 degrees counterclockwise
// the image is lower quality or smaller than required; after the first pass of obtaining
// fast thumbnails, Salamander will try to obtain a quality thumbnail for this image
#define SSTHUMB_ONLY_PREVIEW 8

class CSalamanderThumbnailMakerAbstract
{
public:
    // sets the image-processing parameters for thumbnail creation; this method must
    // be called first on this interface; 'picWidth' and 'picHeight' are the dimensions
    // of the processed image (in pixels); 'flags' is a combination of SSTHUMB_XXX flags,
    // which describe the image passed in the 'buffer' parameter to
    // ProcessBuffer; vraci TRUE, pokud se podarilo alokovat buffery pro zmensovani
    // a je mozne nasledne volat ProcessBuffer; pokud vrati FALSE, doslo k chybe
    // and thumbnail loading must be aborted
    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags) = 0;

    // processes part of the image in 'buffer' (the processed part of the image is stored
    // row by row from top to bottom, pixels in each row are stored left to right, and each pixel
    // is represented by a 32-bit value composed of three bytes with the R+G+B colors and a
    // fourth byte, which is ignored); there are two processing modes: copying the image
    // to the resulting thumbnail (if the processed image does not exceed the thumbnail size)
    // and scaling the image down to the thumbnail (image larger than the
    // thumbnail); 'buffer' is used for reading only; 'rowsCount' specifies how many rows of the
    // image are in the buffer;
    // je-li'buffer' NULL, berou se data z vlastniho bufferu (plugin ziska pres GetBuffer);
    // vraci TRUE pokud ma plugin pokracovat s nacitanim obrazku, vraci-li FALSE,
    // thumbnail generation is finished (the whole image has been processed) or it should be
    // aborted as soon as possible (for example, the user changed the panel path, so the thumbnail is no
    // longer needed)
    //
    // POZOR: pokud je spustena metoda CPluginInterfaceForThumbLoader::LoadThumbnail,
    // changing the panel path is blocked. Therefore larger images should be passed and
    // loaded in chunks, and the return value of
    // ProcessBuffer should be checked to see whether loading should be aborted.
    // If time-consuming operations must be performed before calling SetParameters
    // or before calling ProcessBuffer, GetCancelProcessing must be called occasionally during that time.
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount) = 0;

    // returns an internal buffer large enough to store 'rowsCount' rows of the image
    // (4 * 'rowsCount' * 'picWidth' bytes); if the object is in the error state (after calling
    // SetError), returns NULL;
    // the plugin must not deallocate the returned buffer (Salamander deallocates it automatically)
    virtual void* WINAPI GetBuffer(int rowsCount) = 0;

    // reports an error while obtaining the image (the thumbnail is considered invalid
    // and will not be used); after SetError is called, the other methods of this interface
    // only return errors (GetBuffer and SetParameters) or indicate that processing must stop
    // (ProcessBuffer)
    virtual void WINAPI SetError() = 0;

    // vraci TRUE, pokud ma plugin preprusit nacitani thumbnailu
    // vraci FALSE, pokud ma plugin pokracovat s nacitanim obrazku
    //
    // this method can be called before and after SetParameters
    //
    // it is used to detect abort requests in cases where the plugin
    // needs to perform time-consuming operations before calling SetParameters
    // or when the plugin needs to prerender the image, that is, after calling
    // SetParameters but before calling ProcessBuffer
    virtual BOOL WINAPI GetCancelProcessing() = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForThumbLoaderAbstract
//

class CPluginInterfaceForThumbLoaderAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginInterfaceForThumbLoaderEncapsulation)
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
