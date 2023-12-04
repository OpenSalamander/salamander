// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/************************************************************************************

Co lze vytahnout z HICON, ktere nam dodava OS?

  Pomoci GetIconInfo() nam OS vrati kopie bitmap MASK a COLOR. Ty lze dale
  zkoumat volanim GetObject(), pomoci ktere vytahneme geometrii a bervne usporadani.
  Jedna se o kopie bitmap, nikoliv o bitmapy originalni, drzene uvnitr OS. MASK je
  vzdy 1bitova bitmapa. COLOR je bitmapa kompatibilni s DC obrazovky. Neexistuje tedy
  zpusob, jak z techto dat ziskat informace o realne barevne hloubce bitmapy COLOR.

  Specialni pripad jsou ikony ciste cernobile. Ty jsou predany cele v MASK, ktera
  je pak 2x vyssi. COLOR je pak NULL. Horni polovina MASK bitmapy AND cast a spodni
  polovina je XOR cast. Tento pripad lze snadno detekovat pomoci testu COLOR == NULL.

  Od Windows XP existuje dalsi specialni pripad: ikony obsahujici ALFA kanal. Jedna
  s o DIBy s barevnou hloubkou 32bitu, kde je kazdy bod slozen z ARGB slozek.

  




************************************************************************************/

//
// Existuje potencialni prostor pro optimalizaci nasi implementace ImageListu.
// DIB bychom mohli drzet ve stejnem formatu, ve kterem jede obrazovka. BitBlt
// je potom udajne rychlejsi (neoveroval jsem) podle MSDN:
//   http://support.microsoft.com/default.aspx?scid=kb;EN-US;230492
//   (HOWTO: Retrieving an Optimal DIB Format for a Device)
//
// Proti teto optimalizaci mluvi nekolik faktoru:
//   - museli bychom v kodu podporit ruzne formaty dat (15, 16, 24, 32 bitu)
//   - protoze vykreslujeme soucasne maximalne desitky ikonek, neni pro
//     nas rychlost kresleni kriticka; nameril jsem tyto rychlosti kresleni:
//     (100 000 krat se do obrazovky kreslil pres BitBlt DIB 16x16, 32bpp)
//     rozliseni obrazovky     celkova doba    (W2K, Matrox G450)
//     32 bpp                  0.40 s
//     24 bpp                  0.80 s
//     16 bpp                  0.65 s
//      8 bpp                  1.16 s
//   - Nejak bychom stejne museli drzet ikonky s ALFA kanalem, ktere jsou 32 bpp
//

//
// Proc potrebujeme vlastni obdobu ImageList:
//
// ImageList z CommonControls ma jeden zasadni problem: pokud jej pozadame,
// aby drzel DeviceDependentBitmapy, neumi zobrazit blendenou polozku. Misto
// toho ji sjede paternem.
//
// Pokud je drzena bitmapa DIB, blendeni slape skvele, ale vykresleni
// klasicke polozky je radove pomalejsi (konverze DIB->obrazovka).
//
// Dale existuje riziko, ze v nekterych implementacich volani ImageList_SetBkColor
// fyzicky nezmeni drzenou bitmapu na zaklade masky, ale pouze nastavi vnitrni
// promennou. Samozrejme je pak kresleni pomalejsi, protoze je treba provadet
// maskovani. Testoval jsem pod W2K a funkce chodi spravne.
//
// Jedina moznost by byla v zachovani ImageListu pro drzeni dat a pouze blend
// preprogramovat. Problem ale nastava ve funkci ImageList_GetImageInfo, ktera
// umoznuje pristup k vnitrnim bitmapam Image/Mask. ImageList je ma neustale
// vybrane v MemDC, takze podle MSDN (Q131279: SelectObject() Fails After
// ImageList_GetImageInfo()) je jedinou moznosti napred volat CopyImage a teprve
// potom nad bitmapou pracovat. To by vedlo ne neskutecne pomale kresleni
// blendenych polozek.
//
// Dalsim oriskem jsou pro ImageList ikony invert body. Ikona se sklada ze
// dvou bitmap: MASKA a COLORS. Maska se ANDuje do cile a pres ni se XORuji barvy.
// Diky XORovani tak muzou ikonky invertovat nektere sve casti. Vyuzivaji toho
// predevsim kurzory, viz WINDOWS\Cursors.
//

//******************************************************************************
//
// CIconList
//
//
// Po vzoru W2K drzime polozky v bitmape siroke 4 polozky. Pravdepodobne
// budou operace nad takto orientovanou bitmapu rychlejsi.

#define IL_DRAW_BLEND 0x00000001       // z 50% bude pouzita barva blendClr
#define IL_DRAW_TRANSPARENT 0x00000002 // pri kresleni se zachova puvodni pozadi (pokud neni specifikovano, pouzije vyplni se pozadi definovanou barvou)
#define IL_DRAW_ASALPHA 0x00000004     // pouzije (invertovanou) barvu v BLUE kanalu jako alpha, pomoci ktere k pozadi primicha specifikovanou barvu popredi; zatim slouzi pro throbber
#define IL_DRAW_MASK 0x00000010        // vykreslit masku

class CIconList : public CGUIIconListAbstract
{
private:
    int ImageWidth; // rozmery jednoho obrazku
    int ImageHeight;
    int ImageCount;  // pocet obrazku v bitmape
    int BitmapWidth; // rozmery drzenych bitmap
    int BitmapHeight;

    // obrazky jsou usporadany zleva doprava a shora dolu
    HBITMAP HImage;   // DIB, jeho raw data jsou v promenne ImageRaw
    DWORD* ImageRaw;  // ARGB hodnoty; Alfa: 0x00=pruhledna, 0xFF=nepruhledna, ostatni=castecna_pruhlednost(puze u IL_TYPE_ALPHA)
    BYTE* ImageFlags; // pole o poctu prvku 'imageCount'; (IL_TYPE_xxx)

    COLORREF BkColor; // aktualni barva pozadi (body kde je Alfa==0x00)

    // sdilene promenne pres vsechny imagelisty -- setrime pameti
    static HDC HMemDC;                       // sdilene mem dc
    static HBITMAP HOldBitmap;               // puvodni bitmapa
    static HBITMAP HTmpImage;                // cache pro paint + docasne uloziste masky
    static DWORD* TmpImageRaw;               // raw data od HTmpImage
    static int TmpImageWidth;                // rozmery HTmpImage v bodech
    static int TmpImageHeight;               // rozmery HTmpImage v bodech
    static int MemDCLocks;                   // pro destrukci mem dc
    static CRITICAL_SECTION CriticalSection; // synchronizace pristupu
    static int CriticalSectionLocks;         // pro konstrukci/destrukci CriticalSection

public:
    //    BOOL     Dump; // pokud je TRUE, dumpuji se raw data do TRACE

public:
    CIconList();
    ~CIconList();

    virtual BOOL WINAPI Create(int imageWidth, int imageHeight, int imageCount);
    virtual BOOL WINAPI CreateFromImageList(HIMAGELIST hIL, int requiredImageSize = -1);          // pokud je 'requiredImageSize' -1, pouzije se geometrie z hIL
    virtual BOOL WINAPI CreateFromPNG(HINSTANCE hInstance, LPCTSTR lpBitmapName, int imageWidth); // nacte z resourcu PNG, musi jit o dlouhy prouzek jeden radek vysoky
    virtual BOOL WINAPI CreateFromRawPNG(const void* rawPNG, DWORD rawPNGSize, int imageWidth);
    virtual BOOL WINAPI CreateFromBitmap(HBITMAP hBitmap, int imageCount, COLORREF transparentClr); // nasosne bitmapu (maximalne 256 barev), musi jit o dlouhy prouzek jeden radek vysoky
    virtual BOOL WINAPI CreateAsCopy(const CIconList* iconList, BOOL grayscale);
    virtual BOOL WINAPI CreateAsCopy(const CGUIIconListAbstract* iconList, BOOL grayscale);

    // prevede icon list na cernobilou verzi
    virtual BOOL WINAPI ConvertToGrayscale(BOOL forceAlphaForBW);

    // zakomprimuje bitmapu do 32bitoveho PNG a alfa kanalem (jeden dlouhy radek)
    // pokud se vse podari, vraci TRUE a ukazatel na alokovanou pamet, kterou je nutne potom dealokovat
    // pri chybe vraci FALSE
    virtual BOOL WINAPI SaveToPNG(BYTE** rawPNG, DWORD* rawPNGSize);

    virtual BOOL WINAPI ReplaceIcon(int index, HICON hIcon);

    // vytvori ikonku z pozice 'index'; vraci jeji handle nebo NULL v pripade neuspechu
    // vracenou ikonu je po pouziti treba destruovat pomoci API DestroyIcon
    virtual HICON WINAPI GetIcon(int index);
    HICON GetIcon(int index, BOOL useHandles);

    // vytvori imagelist (jeden radek, pocet sloupcu dle poctu polozek); vraci jeho handle nebo NULL v pripade neuspechu
    // vraceny imagelist je po pouziti treba destruovat pomoci API ImageList_Destroy()
    virtual HIMAGELIST WINAPI GetImageList();

    // kopiruje jednu polozku ze 'srcIL' a pozice 'srcIndex' na pozici 'dstIndex'
    virtual BOOL WINAPI Copy(int dstIndex, CIconList* srcIL, int srcIndex);

    // kopiruje jednu polozku z pozice 'srcIndex' do 'hDstImageList' na pozici 'dstIndex'
    //    BOOL CopyToImageList(HIMAGELIST hDstImageList, int dstIndex, int srcIndex);

    virtual BOOL WINAPI Draw(int index, HDC hDC, int x, int y, COLORREF blendClr, DWORD flags);

    virtual BOOL WINAPI SetBkColor(COLORREF bkColor);
    virtual COLORREF WINAPI GetBkColor();

private:
    // pokud neexistuje, vytvori HTmpImage
    // pokud HTmpImage existuje a je mensi nez 'width' x 'height', vytvori novy
    // vraci TRUE v pripade uspechu, jinak vraci FALSE a zachovava minuly HTmpImage
    BOOL CreateOrEnlargeTmpImage(int width, int height);

    // vraci handle bitmapy prave vybrane v HMemDC
    // pokud HMemDC neexistuje, vraci NULL
    HBITMAP GetCurrentBitmap();

    // 'index' urcuje pozici ikonky v HImage
    // vraci TRUE, pokud v obrazek 'index' v HImage obsahoval alfa kanal
    BYTE ApplyMaskToImage(int index, BYTE forceXOR);

    // pro ladici ucely -- zobrazi dump ARGB hodnot barevne bitmapy i masky
    //    void DumpToTrace(int index, BOOL dumpMask);

    // renderovani bod po bodu a nasledny BitBlt je v RELEASE verzi
    // pouze o 30% pomalejsi proti cistemu BitBlt

    BOOL DrawALPHA(HDC hDC, int x, int y, int index, COLORREF bkColor);
    BOOL DrawXOR(HDC hDC, int x, int y, int index, COLORREF bkColor);
    BOOL AlphaBlend(HDC hDC, int x, int y, int index, COLORREF bkColor, COLORREF fgColor);
    BOOL DrawMask(HDC hDC, int x, int y, int index, COLORREF fgColor, COLORREF bkColor);
    BOOL DrawALPHALeaveBackground(HDC hDC, int x, int y, int index);
    BOOL DrawAsAlphaLeaveBackground(HDC hDC, int x, int y, int index, COLORREF fgColor);

    void StoreMonoIcon(int index, WORD* mask);

    // specialni podpurna funkce pro CreateFromBitmap(); nakopiruj z 'hSrcBitmap'
    // zvoleny pocet polozek do 'dstIndex'; predpoklada, ze 'hSrcBitmap' bude dlouhy
    // prouzek ikon, jeden radek vysoky
    // transparentClr udava barvu, ktera se ma povazovat za pruhlednou
    // predpoklada se, ze zdrojova bitmapa ma stejny rozmer ikonek jako ma cilova (ImageWidth, ImageHeight)
    // jednim kopirovanim lze pracovat maximalne s jednim radkem cilove bitmapy,
    // nelze napriklad nakopirovat data do dvou radku v cilove bitmape
    BOOL CopyFromBitmapIternal(int dstIndex, HBITMAP hSrcBitmap, int srcIndex, int imageCount, COLORREF transparentClr);
};

HBITMAP LoadPNGBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, DWORD flags);
HBITMAP LoadRawPNGBitmap(const void* rawPNG, DWORD rawPNGSize, DWORD flags);

inline BYTE GetGrayscaleFromRGB(int red, int green, int blue)
{
    //  int brightness = (76*(int)red + 150*(int)green + 29*(int)blue) / 255;
    int brightness = (55 * (int)red + 183 * (int)green + 19 * (int)blue) / 255;
    //  int brightness = (40*(int)red + 175*(int)green + 60*(int)blue) / 255;
    if (brightness > 255)
        brightness = 255;
    return (BYTE)brightness;
}
