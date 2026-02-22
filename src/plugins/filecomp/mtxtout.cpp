// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

class CMappedFont
{
public:
    CMappedFont(LOGFONT* plf, char* pMap, bool bWCHAR, int fontWidth, int fontHeight, bool substCharIsSpace);
    ~CMappedFont();

    CMappedFont* pNext;
    int nCounter;
    union
    {
        char* chr;
        wchar_t* wchr;
    } Map;
    bool bwchr;

    LOGFONT lf;
    int FontWidth, FontHeight;
    bool SubstCharIsSpace; // TRUE means a space is used as the substitution character instead of 0xB7 (middle dot)
};

// Returns true if some mapping ever can take place
template <class CChar>
bool FontHasChangedX(CChar* ViewerFontMapping, HDC memDC, int fontWidth, int fontHeight, int start, int end,
                     bool* substCharIsSpace)
{
    CChar ch[2];
    RECT rect;
    CChar substChar = *substCharIsSpace ? (CChar)' ' : (CChar)0xB7 /* middle dot */;
    bool ret = false;

    ch[1] = 0;
    int x;
    for (x = start; x < end; x++)
    {
        CChar result = x;
        // DrawTextEx would likely fail on a lone surrogate (without its pair)
        // or on an invalid UTF-16 code unit
        if (TCharSpecific<CChar>::IsValidChar(x))
        {
            ch[0] = x;
            rect.left = 0;
            rect.right = fontWidth;
            rect.top = 0;
            rect.bottom = fontHeight;
            // NOTE: GetCharWidth32 still returns the same width for all characters,
            // although DrawTextEx does not!
            if (DrawTextExX(memDC, ch, 1, &rect, DT_LEFT | DT_TOP | DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE | DT_NOCLIP, NULL)) // with DT_NOCLIP can be 2 x faster
            {
                if (rect.right - rect.left != fontWidth)
                {
                    if (x == 0xB7 /* middle dot */) // if the middle dot is also too narrow/wide, fall back to a space which should be safe
                    {
                        substChar = (CChar)' ';
                        int z;
                        for (z = 0; z < x; z++)
                            if (ViewerFontMapping[z] == (CChar)0xB7 /* middle dot */)
                                ViewerFontMapping[z] = substChar;
                    }
                    result = substChar;
                    ret = true;
                }
            }
            else
                TRACE_I("CFileViewWindow::ReloadConfiguration(): DrawTextEx: error for: " << x);
        }
        ViewerFontMapping[x] = result; // store the result at the end to avoid multi-thread races (no critical section here)
    }
    *substCharIsSpace = substChar == (CChar)' ';
    return ret;
}

template <class CChar>
void TMappedTextOut<CChar>::FontHasChanged(LOGFONT* plf, HFONT font, int fontWidth, int fontHeight)
{
    //  if (!ViewerFontMapping) return;
    // XP64/Vista: the Fixedsys font contains characters that do not have the expected width (despite being fixed width), so
    // measure individual characters and map those with a wrong width to a replacement glyph of the proper width
    ViewerFontNeedsMapping = false;
    if (WindowsXP64AndLater) // outside XP64 and Vista we never hit this quirk, so skip the expensive check
    {
        LPVOID pNewFont;

        pNewFont = MappedFontFactory.FindFont(plf, font, (LPVOID*)&ViewerFontMapping, fontWidth, fontHeight, sizeof(CChar));
        MappedFontFactory.ReleaseMappedFont(pMappedFont);
        pMappedFont = pNewFont;
        ViewerFontNeedsMapping = pMappedFont != NULL;
    }
}

template <class CChar>
void TMappedTextOut<CChar>::CalcMappingIfNeeded(HDC hDC, const CChar* buf, int len)
{
    while (len-- >= 0)
    {
        if (ViewerFontMapping[*buf] == 0xFEFE)
        {
            // Font not yet mapped/checked -> read/check/map entire 16-char block
            FontHasChangedX(ViewerFontMapping, hDC, ((CMappedFont*)pMappedFont)->FontWidth,
                            ((CMappedFont*)pMappedFont)->FontHeight, *buf & 0xFFF0, (int)(*buf & 0xFFF0) + 16,
                            &((CMappedFont*)pMappedFont)->SubstCharIsSpace);
        }
        buf++;
    }
}

CMappedFont::CMappedFont(LOGFONT* plf, char* pMap, bool bWCHAR, int fontWidth, int fontHeight, bool substCharIsSpace)
{
    pNext = NULL;
    // Initial nCounter is 2. It forces the map stay alive even when all FC windows
    // get closed and thus it can be quickly reused when a new FC window gets opened.
    // NOTE: the counter stays at 1 when no FC is open (and does not grow)
    // NOTE: the font map always gets freed on FC plugin unload
    nCounter = 1 + 1;
    Map.chr = pMap;
    lf = *plf;
    bwchr = bWCHAR;
    FontWidth = fontWidth;
    FontHeight = fontHeight;
    SubstCharIsSpace = substCharIsSpace;
}

CMappedFont::~CMappedFont()
{
    if (Map.chr)
        free(Map.chr);
}

bool CMappedFontFactory::Init()
{
    InitializeCriticalSection(&hCritSect);
    bInited = true;
    return true;
}

void CMappedFontFactory::Free()
{ // Can only be called at plugin shutdown
    ReleaseFonts();
    DeleteCriticalSection(&hCritSect);
    bInited = false;
}

void CMappedFontFactory::ReleaseFonts()
{
    if (bInited)
        EnterCriticalSection(&hCritSect);

    while (pFonts)
    {
        CMappedFont* pNext = pFonts->pNext;
        delete pFonts;
        pFonts = pNext;
    }

    if (bInited)
        LeaveCriticalSection(&hCritSect);
}

void CMappedFontFactory::ReleaseMappedFont(LPVOID hMappedFont)
{
    if (!hMappedFont)
        return;
    EnterCriticalSection(&hCritSect);

    CMappedFont** ppFonts = &pFonts;
    while (*ppFonts)
    {
        if (*ppFonts == hMappedFont)
        {
            if (!--(*ppFonts)->nCounter)
            {
                CMappedFont* pNext = (*ppFonts)->pNext;
                delete *ppFonts;
                *ppFonts = pNext;
            }
            break;
        }
        ppFonts = &(*ppFonts)->pNext;
    }

    LeaveCriticalSection(&hCritSect);
}

LPVOID CMappedFontFactory::FindFont(LOGFONT* plf, HFONT font, LPVOID* ViewerFontMapping, int fontWidth, int fontHeight, int charsize)
{
    EnterCriticalSection(&hCritSect);

    CMappedFont* pFont = pFonts;
    while (pFont)
    {
        if ((((1 == charsize) && !pFont->bwchr) || ((2 == charsize) && pFont->bwchr)) && !memcmp(plf, &pFont->lf, sizeof(*plf)))
        {
            pFont->nCounter++; // Increase reference count
            *ViewerFontMapping = pFont->Map.chr;
            LeaveCriticalSection(&hCritSect);
            return pFont;
        }
        pFont = pFont->pNext;
    }
    // This font has not been used yet
    HDC hdc = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(NULL);
    if (memDC != NULL)
    {
        HBITMAP bmp = CreateCompatibleBitmap(hdc, fontWidth, fontHeight);
        if (bmp != NULL)
        {
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);
            HFONT oldFont = (HFONT)SelectObject(memDC, font);
            bool bNeedMapping = false;
            char* pMap;
            bool substCharIsSpace = false;

            if (charsize == 1)
            {
                pMap = (char*)malloc(sizeof(char) * TCharSpecific<char>::CharCount());
                if (pMap)
                    bNeedMapping = FontHasChangedX(pMap, memDC, fontWidth, fontHeight, 0, TCharSpecific<char>::CharCount(), &substCharIsSpace);
            }
            else
            {
                pMap = (char*)malloc(sizeof(wchar_t) * TCharSpecific<wchar_t>::CharCount());
                if (pMap)
                {
                    memset(pMap, 0xFE, sizeof(wchar_t) * TCharSpecific<wchar_t>::CharCount());
                    FontHasChangedX((wchar_t*)pMap, memDC, fontWidth, fontHeight, 0, 256, &substCharIsSpace);
                    bNeedMapping = true; // we do not verify every character, so we cannot claim mapping will never be needed
                }
            }
            if (bNeedMapping)
            {
                pFont = new CMappedFont(plf, pMap, charsize == 2, fontWidth, fontHeight, substCharIsSpace);
                if (pFont)
                {
                    pFont->pNext = pFonts;
                    pFonts = pFont;
                    *ViewerFontMapping = pMap;
                }
            }
            else if (pMap)
            {
                free(pMap); // No mapping needed
            }
            SelectObject(memDC, oldFont);
            SelectObject(memDC, oldBmp);
            DeleteObject(bmp);
        }
        else
            TRACE_E("CFileViewWindow::ReloadConfiguration(): CreateCompatibleBitmap failed!");
        DeleteDC(memDC);
    }
    else
        TRACE_E("CFileViewWindow::ReloadConfiguration(): CreateCompatibleDC failed!");
    ReleaseDC(NULL, hdc);

    LeaveCriticalSection(&hCritSect);
    return pFont;
}

template class TMappedTextOut<char>;
template class TMappedTextOut<wchar_t>;