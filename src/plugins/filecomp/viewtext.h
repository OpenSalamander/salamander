// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// TTextFileViewWindow<CChar>
//

template <class CChar>
class TTextFileViewWindow : public CTextFileViewWindowBase
{
    SUPER(CTextFileViewWindowBase);

protected:
    typedef typename TCharSpecific<CChar>::POLYTEXT POLYTEXT;
    typedef typename std::vector<CChar*> CLineBuffer;
    // CChar * Buffer;
    // CChar const ** Linbuf;
    CChar* LineBuffer; // bufer for one formated line
    CChar* Text;
    CLineBuffer Lines;
    CIntIndexes LineChanges;
    std::vector<POLYTEXT> PTCommon;
    std::vector<POLYTEXT> PTChange;
    std::vector<POLYTEXT> PTZeroChange;
    CChar WhiteSpaceChar;
    TMappedTextOut<CChar> MappedTextOut;

public:
    TTextFileViewWindow(CFileViewID id, BOOL showWhiteSpace, CMainWindow* mainWindow);
    virtual ~TTextFileViewWindow() { DestroyData(); }
    virtual void DestroyData();
    virtual int GetLines();
    virtual void ReloadConfiguration(DWORD flags, BOOL updateWindow);
    virtual int MeasureLine(int line, int length);
    int FormatLine(HDC hDC, CChar* buffer, const CChar* start, const CChar* end);
    void SetData(CChar* text, CLineBuffer& lines, CLineScript& script);
    virtual BOOL ReallocLineBuffer();
    virtual int TransformXOrg(int x, int line);
    virtual BOOL CopySelection();
    virtual void MoveCaret(int yPos);
    virtual int GetLeftmostVisibleChar(int line);
    virtual int GetRightmostVisibleChar(int line);
    virtual int LineLength(int line);
    virtual void SelectWord(int line, int& start, int& end);
    static int GetCharCat(CChar c);
    virtual void NextCharBlock(int line, int& xpos);
    virtual void PrevCharBlock(int line, int& xpos);
    void TransformOffsets(const int* changes, int size, const CChar* line);
    void PolytextLengthFixup(typename std::vector<POLYTEXT>::iterator begin,
                             typename std::vector<POLYTEXT>::iterator end);
    virtual void Paint();
};

template class TTextFileViewWindow<char>;
template class TTextFileViewWindow<wchar_t>;
