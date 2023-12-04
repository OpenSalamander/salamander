// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//******************************************************************************
//
// CShrinkImage
//

class CShrinkImage
{
protected:
    DWORD NormCoeffX, NormCoeffY;
    DWORD* RowCoeff;
    DWORD* ColCoeff;
    DWORD* YCoeff;
    DWORD NormCoeff;
    DWORD Y, YBndr;
    DWORD* OutLine;
    DWORD* Buff;
    DWORD OrigHeight;
    WORD NewWidth;
    BOOL ProcessTopDown;

public:
    CShrinkImage();
    ~CShrinkImage();

    // alokuje interni data pro zmensovani a vraci TRUE v pripade upsechu
    // pokud se alokace nepovedou, vrati FALSE
    BOOL Alloc(DWORD origWidth, DWORD origHeight,
               WORD newWidth, WORD newHeight,
               DWORD* outBuff, BOOL processTopDown);

    // destukce alokovanych bufferu a inicializace promennych
    void Destroy();

    void ProcessRows(DWORD* inBuff, DWORD rowCount);

protected:
    DWORD* CreateCoeff(DWORD origLen, WORD newLen, DWORD& norm);
    void Cleanup();
};

//******************************************************************************
//
// CSalamanderThumbnailMaker
//
// Slouzi pro zmensovani puvodniho obrazku do thumbnailu.
//

class CSalamanderThumbnailMaker //: public CSalamanderThumbnailMakerAbstract
{
protected:
    //    CFilesWindow *Window;  // okno panelu, v jehoz icon-readeru fungujeme

    DWORD* Buffer;  // vlastni buffer pro data radek od pluginu
    int BufferSize; // velikost bufferu 'Buffer'
    BOOL Error;     // je-li TRUE, nastala pri zpracovani thumbnailu chyba (vysledek neni pouzitelny)
    int NextLine;   // cislo pristi zpracovavane radky

    DWORD* ThumbnailBuffer;    // zmenseny obrazek
    DWORD* AuxTransformBuffer; // pomocny buffer o stejne velikosti jako ThumbnailBuffer (slouzi pro prenos dat pri transformaci + po transformaci se buffery prohodi)
    int ThumbnailMaxWidth;     // maximalni teoreticke rozmery thumbnailu (v bodech)
    int ThumbnailMaxHeight;
    int ThumbnailRealWidth;  // realne rozmery zmenseneho obrazku (v bodech)
    int ThumbnailRealHeight; //

    // parametry zpracovavaneho obrazku
    int OriginalWidth;
    int OriginalHeight;
    DWORD PictureFlags;
    BOOL ProcessTopDown;

    CShrinkImage Shrinker; // zajistuje zmensovani obrazku
    BOOL ShrinkImage;

public:
    CSalamanderThumbnailMaker(/*CFilesWindow *window*/);
    ~CSalamanderThumbnailMaker();

    // Calculates the thumbnail size, given image and max thumbnail sizes
    static BOOL CalculateThumbnailSize(int originalWidth, int originalHeight,
                                       int maxWidth, int maxHeight,
                                       int& thumbWidth, int& thumbHeight);

    // vycisteni objektu - vola se pred zpracovanim dalsiho thumbnailu nebo kdyz uz
    // neni potreba thumbnail (at uz hotovy nebo ne) z tohoto objektu
    // parametr 'thumbnailMaxSize' udava maximalni moznou sirku a vysku
    // thumbnailu v bodech; pokud je roven -1, ignoruje se
    void Clear(int thumbnailMaxSize = -1);
    /*
    // vraci TRUE pokud je v tomto objektu pripraveny cely thumbnail (povedlo se
    // jeho ziskani od pluginu)
    BOOL ThumbnailReady();

    // provede transformaci thumbnailu podle PictureFlags (SSTHUMB_MIRROR_VERT uz je hotova,
    // zbyva provest SSTHUMB_MIRROR_HOR a SSTHUMB_ROTATE_90CW)
    void TransformThumbnail();

    // konvertuje hotovy thumbnail na DDB a jeji rozmery a raw data ulozi do 'data'
    BOOL RenderToThumbnailData(CThumbnailData *data);*/

    // pokud se nevytvoril cely thumbnail a nenastala chyba (viz 'Error'), doplni
    // zbytek thumbnailu bilou barvou (aby se v nedefinovane casti thumbnailu
    // nezobrazovaly zbytky predchoziho thumbnailu); pokud se nevytvorily ani
    // tri radky thumbnailu, nic se nedoplnuje (thumbnail by byl stejne k nicemu)
    void HandleIncompleteImages();

    //    BOOL IsOnlyPreview() {return (PictureFlags & SSTHUMB_ONLY_PREVIEW) != 0;}

    void* WINAPI GetThumbnailBuffer() { return ThumbnailBuffer; }

    // *********************************************************************************
    // metody rozhrani CSalamanderThumbnailMakerAbstract
    // *********************************************************************************

    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags);
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount);
    virtual void* WINAPI GetBuffer(int rowsCount);
    virtual void WINAPI SetError() { Error = TRUE; }
    virtual BOOL WINAPI GetCancelProcessing();
};
