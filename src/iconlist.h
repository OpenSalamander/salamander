// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

/************************************************************************************

What can we extract from the HICON handles provided by the OS?

  Using GetIconInfo(), the OS returns copies of the MASK and COLOR bitmaps. We can
  inspect them further with GetObject(), which lets us obtain their geometry and
  color layout. These are bitmap copies, not the original bitmaps held inside the OS.
  MASK is always a 1-bit bitmap. COLOR is a bitmap compatible with the screen DC.
  There is therefore no way to determine the real color depth of the COLOR bitmap
  from this data.

  A special case is fully monochrome icons. They are passed entirely in MASK, which
  is then twice as tall. COLOR is NULL in that case. The upper half of the MASK
  bitmap is the AND part and the lower half is the XOR part. This case can be
  detected easily by testing COLOR == NULL.

  Since Windows XP there has been another special case: icons containing an alpha
  channel. These are DIBs with 32-bit color depth, where each pixel consists of
  ARGB components.

  




************************************************************************************/

//
// There is potential room to optimize our ImageList implementation.
// We could keep the DIB in the same format as the display. According to MSDN,
// BitBlt is then reportedly faster, although I did not verify that:
//   http://support.microsoft.com/default.aspx?scid=kb;EN-US;230492
//   (HOWTO: Retrieving an Optimal DIB Format for a Device)
//
// Several factors argue against that optimization:
//   - we would have to support different data formats in the code (15, 16, 24, 32 bits)
//   - because we draw at most a few dozen icons at once, drawing speed is not critical
//     for us; I measured the following drawing speeds:
//     (a 16x16, 32bpp DIB was drawn to the screen via BitBlt 100,000 times)
//     screen resolution        total time      (W2K, Matrox G450)
//     32 bpp                   0.40 s
//     24 bpp                   0.80 s
//     16 bpp                   0.65 s
//      8 bpp                   1.16 s
//   - we would still somehow need to keep icons with an ALPHA channel, which are 32 bpp
//

//
// Why do we need our own ImageList equivalent:
//
// The CommonControls ImageList has one major problem: if we ask it to keep
// DeviceDependentBitmaps, it cannot display a blended item. Instead, it fills
// it with a pattern.
//
// If a DIB bitmap is stored, blending works great, but drawing a normal item
// is orders of magnitude slower (DIB -> screen conversion).
//
// There is also a risk that in some implementations, calling ImageList_SetBkColor
// does not physically modify the stored bitmap using the mask, but only updates an
// internal variable. Drawing is then naturally slower because masking has to be
// performed. I tested this under W2K and the function behaves correctly there.
//
// The only option would be to keep ImageList for data storage and reimplement only
// blending. The problem appears in ImageList_GetImageInfo, which provides access
// to the internal Image/Mask bitmaps. ImageList keeps them permanently selected in
// MemDC, so according to MSDN (Q131279: SelectObject() Fails After
// ImageList_GetImageInfo()) the only option is to call CopyImage first and only
// then work with the bitmap. That would lead to unbearably slow drawing of blended
// items.
//
// Another difficulty for ImageList is icons with inverted pixels. An icon consists
// of two bitmaps: MASK and COLORS. The mask is AND-ed into the target and then the
// colors are XOR-ed through it. Thanks to XOR, icons can invert some of their
// parts. This is used mainly by cursors, see WINDOWS\\Cursors.
//

//******************************************************************************
//
// CIconList
//
//
// Following the W2K model, we keep items in a bitmap four items wide. Operations
// on a bitmap arranged like this will probably be faster.

#define IL_DRAW_BLEND 0x00000001       // blendClr will be used at 50%
#define IL_DRAW_TRANSPARENT 0x00000002 // drawing preserves the original background (unless specified otherwise, the background is filled with the defined color)
#define IL_DRAW_ASALPHA 0x00000004     // uses the (inverted) color in the BLUE channel as alpha, with which it mixes the specified foreground color into the background; currently used for the throbber
#define IL_DRAW_MASK 0x00000010        // draw the mask

class CIconList : public CGUIIconListAbstract
{
private:
    int ImageWidth; // dimensions of one image
    int ImageHeight;
    int ImageCount;  // number of images in the bitmap
    int BitmapWidth; // dimensions of the stored bitmaps
    int BitmapHeight;

    // images are laid out from left to right and top to bottom
    HBITMAP HImage;   // DIB; its raw data is in the ImageRaw variable
    DWORD* ImageRaw;  // ARGB values; Alpha: 0x00 = transparent, 0xFF = opaque, others = partial transparency (only for IL_TYPE_ALPHA)
    BYTE* ImageFlags; // array with 'imageCount' elements; (IL_TYPE_xxx)

    COLORREF BkColor; // current background color (pixels where Alpha == 0x00)

    // shared variables across all image lists -- saves memory
    static HDC HMemDC;                       // shared memory DC
    static HBITMAP HOldBitmap;               // original bitmap
    static HBITMAP HTmpImage;                // paint cache + temporary mask storage
    static DWORD* TmpImageRaw;               // raw data from HTmpImage
    static int TmpImageWidth;                // dimensions of HTmpImage in pixels
    static int TmpImageHeight;               // dimensions of HTmpImage in pixels
    static int MemDCLocks;                   // for destroying the memory DC
    static CRITICAL_SECTION CriticalSection; // access synchronization
    static int CriticalSectionLocks;         // for constructing/destroying CriticalSection

public:
    //    BOOL     Dump; // if TRUE, raw data is dumped to TRACE

public:
    CIconList();
    ~CIconList();

    virtual BOOL WINAPI Create(int imageWidth, int imageHeight, int imageCount);
    virtual BOOL WINAPI CreateFromImageList(HIMAGELIST hIL, int requiredImageSize = -1);          // if 'requiredImageSize' is -1, the geometry from hIL is used
    virtual BOOL WINAPI CreateFromPNG(HINSTANCE hInstance, LPCTSTR lpBitmapName, int imageWidth); // loads PNG from resources; it must be a long strip one row high
    virtual BOOL WINAPI CreateFromRawPNG(const void* rawPNG, DWORD rawPNGSize, int imageWidth);
    virtual BOOL WINAPI CreateFromBitmap(HBITMAP hBitmap, int imageCount, COLORREF transparentClr); // accepts a bitmap (up to 256 colors); it must be a long strip one row high
    virtual BOOL WINAPI CreateAsCopy(const CIconList* iconList, BOOL grayscale);
    virtual BOOL WINAPI CreateAsCopy(const CGUIIconListAbstract* iconList, BOOL grayscale);

    // converts the icon list to a grayscale version
    virtual BOOL WINAPI ConvertToGrayscale(BOOL forceAlphaForBW);

    // compresses the bitmap into a 32-bit PNG with an alpha channel (one long row)
    // on success, returns TRUE and a pointer to allocated memory that must later be freed
    // on error, returns FALSE
    virtual BOOL WINAPI SaveToPNG(BYTE** rawPNG, DWORD* rawPNGSize);

    virtual BOOL WINAPI ReplaceIcon(int index, HICON hIcon);

    // creates an icon from position 'index'; returns its handle or NULL on failure
    // the returned icon must be destroyed after use with the DestroyIcon API
    virtual HICON WINAPI GetIcon(int index);
    HICON GetIcon(int index, BOOL useHandles);

    // creates an image list (one row, with the number of columns based on the number of items); returns its handle or NULL on failure
    // the returned image list must be destroyed with the ImageList_Destroy() API after use
    virtual HIMAGELIST WINAPI GetImageList();

    // copies one item from 'srcIL' at position 'srcIndex' to position 'dstIndex'
    virtual BOOL WINAPI Copy(int dstIndex, CIconList* srcIL, int srcIndex);

    // copies one item from position 'srcIndex' into 'hDstImageList' at position 'dstIndex'
    //    BOOL CopyToImageList(HIMAGELIST hDstImageList, int dstIndex, int srcIndex);

    virtual BOOL WINAPI Draw(int index, HDC hDC, int x, int y, COLORREF blendClr, DWORD flags);

    virtual BOOL WINAPI SetBkColor(COLORREF bkColor);
    virtual COLORREF WINAPI GetBkColor();

private:
    // creates HTmpImage if it does not exist
    // if HTmpImage exists and is smaller than 'width' x 'height', creates a new one
    // returns TRUE on success; otherwise returns FALSE and keeps the previous HTmpImage
    BOOL CreateOrEnlargeTmpImage(int width, int height);

    // returns the handle of the bitmap currently selected in HMemDC
    // if HMemDC does not exist, returns NULL
    HBITMAP GetCurrentBitmap();

    // 'index' specifies the icon position in HImage
    // returns TRUE if image 'index' in HImage contained an alpha channel
    BYTE ApplyMaskToImage(int index, BYTE forceXOR);

    // for debugging purposes -- shows a dump of the ARGB values of both the color bitmap and the mask
    //    void DumpToTrace(int index, BOOL dumpMask);

    // point-by-point rendering followed by BitBlt is in the RELEASE build
    // only about 30% slower than a plain BitBlt

    BOOL DrawALPHA(HDC hDC, int x, int y, int index, COLORREF bkColor);
    BOOL DrawXOR(HDC hDC, int x, int y, int index, COLORREF bkColor);
    BOOL AlphaBlend(HDC hDC, int x, int y, int index, COLORREF bkColor, COLORREF fgColor);
    BOOL DrawMask(HDC hDC, int x, int y, int index, COLORREF fgColor, COLORREF bkColor);
    BOOL DrawALPHALeaveBackground(HDC hDC, int x, int y, int index);
    BOOL DrawAsAlphaLeaveBackground(HDC hDC, int x, int y, int index, COLORREF fgColor);

    void StoreMonoIcon(int index, WORD* mask);

    // special helper for CreateFromBitmap(); copies the selected number of items
    // from 'hSrcBitmap' into 'dstIndex'; assumes 'hSrcBitmap' is a long strip of
    // icons one row high
    // transparentClr specifies the color to be considered transparent
    // assumes the source bitmap has the same icon dimensions as the target one (ImageWidth, ImageHeight)
    // a single copy operation can work with at most one row of the target bitmap;
    // for example, it cannot copy data into two rows in the target bitmap
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
