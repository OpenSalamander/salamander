// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

struct NSVGrasterizer;
struct NSVGimage;
void RenderSVGImage(NSVGrasterizer* rast, HDC hDC, int x, int y, const char* svgName, int iconSize, COLORREF bkColor, BOOL enabled);

// returns SysColor in the format for the SVG library (BGR instead of Win32 RGB)
DWORD GetSVGSysColor(int index);

//*****************************************************************************
//
// CSVGSprite
//

#define SVGSTATE_ORIGINAL 0x0001 // unchanged original form of the SVG
#define SVGSTATE_ENABLED 0x0002  // SVG recolored to the enabled text color
#define SVGSTATE_DISABLED 0x0004 // SVG recolored to the disabled text color
#define SVGSTATE_COUNT 3

// Object used to render SVGs via a cached bitmap.
// Primarily stores the colored variant of the image rendered with the colors defined in the source SVG.
// It can also hold other color variants of the bitmap (hence Sprite in the name — it internally uses a larger bitmap with multiple images),
// for example "disabled", "active", "selected".
class CSVGSprite
{
public:
    CSVGSprite();
    ~CSVGSprite();

    // discards the bitmap and initializes variables to their default values
    void Clean();

    // 'states' is a combination of bits from the SVGSTATE_* family
    BOOL Load(int resID, int width, int height, DWORD states);

    void GetSize(SIZE* s);
    int GetWidth();
    int GetHeight();

    // 'hDC' is the destination DC where the bitmap should be rendered
    // 'x' and 'y' are the destination coordinates in 'hDC'
    // 'width' and 'height' are the destination size; if they are -1, the stored 'Width'/'Height' size is used
    void AlphaBlend(HDC hDC, int x, int y, int width, int height, DWORD state);

protected:
    // loads the resource into memory, allocates a buffer one byte longer, and terminates the resource with zero
    // on success returns a pointer to the allocated memory (it has to be freed), on failure returns NULL
    char* LoadSVGResource(int resID);

    // Input 'sz' specifies the size in points that the SVG should fit into after conversion to a bitmap.
    // If one dimension is -1, it is unspecified and is computed to preserve the aspect ratio.
    // If neither dimension is specified, they are taken from the source data.
    // Returns the output bitmap dimensions in points.
    void GetScaleAndSize(const NSVGimage* image, const SIZE* sz, float* scale, int* width, int* height);

    // creates a DIB of size 'width' and 'height', returns its handle and a pointer to its data
    void CreateDIB(int width, int height, HBITMAP* hMemBmp, void** lpMemBits);

    // tints the SVG 'image' with the color determined by 'state' state
    void ColorizeSVG(NSVGimage* image, DWORD state);

protected:
    int Width; // dimension of a single image in points
    int Height;
    HBITMAP HBitmaps[SVGSTATE_COUNT];
};

//*****************************************************************************
//
// global variables
//

//extern HBITMAP HArrowRight;         // bitmap created from SVG, used for buttons as the right arrow
//extern SIZE ArrowRightSize;         // dimensions in points
//HBITMAP HArrowRight = NULL;
//SIZE ArrowRightSize = { 0 };

extern CSVGSprite SVGArrowRight;
extern CSVGSprite SVGArrowRightSmall;
extern CSVGSprite SVGArrowMore;
extern CSVGSprite SVGArrowLess;
extern CSVGSprite SVGArrowDropDown;
