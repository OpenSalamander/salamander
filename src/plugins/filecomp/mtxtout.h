// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CMappedFont;

class CMappedFontFactory
{
public:
    CMappedFontFactory()
    {
        pFonts = NULL;
        bInited = false;
    };
    ~CMappedFontFactory() { ReleaseFonts(); };

    bool Init();
    void Free(); // Can only be called at plugin shutdown

    void ReleaseMappedFont(LPVOID hMappedFont);
    LPVOID FindFont(LOGFONT* plf, HFONT font, LPVOID* ViewerFontMapping, int fontWidth, int fontHeight, int charsize);

private:
    bool bInited; // Is hCritSect valid?
    CRITICAL_SECTION hCritSect;
    CMappedFont* pFonts;

    void ReleaseFonts();
};

extern CMappedFontFactory MappedFontFactory;

template <class CChar>
class TMappedTextOut
{
public:
    TMappedTextOut()
    {
        ViewerFontNeedsMapping = false;
        ViewerFontMapping = NULL; //(CChar *)malloc(sizeof(CChar) * TCharSpecific<CChar>::CharCount() );
        Buffer = NULL;
        BufferSize = 0;
        pMappedFont = NULL;
    }

    ~TMappedTextOut()
    {
        MappedFontFactory.ReleaseMappedFont(pMappedFont);
        //      if (ViewerFontMapping) free(ViewerFontMapping);
        if (Buffer)
            free(Buffer);
    }

    void FontHasChanged(LOGFONT* plf, HFONT font, int fontWidth, int fontHeight);

    // replacement for ExtTextOut: on Vista performs remapping to characters with the correct width (see ViewerFontNeedsMapping)
    BOOL DoTextOut(HDC hdc, int X, int Y, UINT fuOptions, CONST RECT* lprc,
                   const CChar* lpString, UINT cbCount, CONST INT* lpDx)
    {
        if (ViewerFontNeedsMapping && lpString != NULL)
        {
            if (sizeof(CChar) > 1)
                CalcMappingIfNeeded(hdc, lpString, cbCount);
            const CChar* s = lpString;
            if (cbCount >= BufferSize) // realloc buffer if needed
            {
                size_t newSize = __max(cbCount + 1, BufferSize * 2);
                CChar* buf = (CChar*)realloc(Buffer, newSize * sizeof(CChar));
                if (!buf)
                {
                    TRACE_E("Low memory");
                    return FALSE;
                }
                Buffer = buf;
                BufferSize = newSize;
            }
            const CChar* end = s + cbCount;
            CChar* d = Buffer;
            while (s < end)
                *d++ = ViewerFontMapping[TCharSpecific<CChar>::Unsigned(*s++)];
            *d = 0;
            return ExtTextOutX(hdc, X, Y, fuOptions, lprc, Buffer, cbCount, lpDx);
        }
        else
            return ExtTextOutX(hdc, X, Y, fuOptions, lprc, lpString, cbCount, lpDx);
    }

    // true = only on XP64/Vista it is necessary to map characters before drawing (some glyphs have an incorrect width)
    bool NeedMapping() { return ViewerFontNeedsMapping; }

    // remap the character; may be called only when NeedMapping returns true!!!
    CChar MapChar(CChar c) { return ViewerFontMapping[TCharSpecific<CChar>::Unsigned(c)]; }

    // may be called only when NeedMapping returns true!!!
    void CalcMappingIfNeeded(HDC hDC, const CChar* buf, int len);

private:
    bool ViewerFontNeedsMapping; // TRUE = only on XP64/Vista do characters need to be remapped before rendering (some glyphs are wrongly sized)
    CChar* ViewerFontMapping;    // character map used when ViewerFontNeedsMapping is true
    CChar* Buffer;
    size_t BufferSize;
    LPVOID pMappedFont;
};
