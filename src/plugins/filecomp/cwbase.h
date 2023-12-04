// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CFilecompCoWorkerBase
//

template <class CChar>
class CFilecompCoWorkerBase
{
public:
    CFilecompCoWorkerBase(HWND mainWindow, CCompareOptions& options, const int& cancelFlag);

protected:
    typedef std::vector<CChar*> CLineBuffer;
    struct CFCFileData
    {
        const char* Name;
        CChar* Begin;
        CChar* End;
        size_t Length;
        CLineBuffer Lines;

        CFCFileData() : Begin(0) {}
        ~CFCFileData()
        {
            if (Begin)
                free(Begin);
        }
    };

    template <class CChar>
    struct CCharProxy
    {
        CChar Char;
        const CChar* Position;
        size_t CharLine;
        CCharProxy(CChar c, const CChar* position, size_t line)
            : Char(c), Position(position), CharLine(line) {}
        bool operator==(const CCharProxy& proxy) const { return Char == proxy.Char; }
    };
    typedef std::vector<CCharProxy<CChar>> CStrictCompareData;

    HWND MainWindow; // okno, ktere dostane WM_USER_WORKERNOTIFIES
    CCompareOptions& Options;
    const int& CancelFlag; // 0 ... jedeme dal, jinak konec
    CFCFileData Files[2];

    // work buffers for StrictCompare are to prevent offten re-allocation when
    // called multiple times
    struct CStrictData
    {
        CStrictCompareData CompareData[2];
        CEditScript EditScript;
        std::vector<int> Changes[2];    // changes in current line
        std::vector<ptrdiff_t> DiffBuf; // prevent diff reallocate buffer each call
    } Strict;

    void ReadFilesAndFindLines(CTextFileReader (&reader)[2], bool& binaryIdentical);
    void SendResults(CLineScript (&lineScript)[2], CIntIndexes& changesToLines,
                     CIntIndexes& changesLengths, CEditScript& changes, CTextFileReader (&reader)[2]);
    void ScrictCompare(size_t (&firstLines)[2], size_t (&lenghts)[2],
                       CLineScript (&script)[2], int lcCommon);
    void RemoveSingleCharMatches();

    template <class CCompareData>
    void ShiftBoundaries(CEditScript& editScript, CCompareData (&compareData)[2]);
};
