// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CCompareOptions
//

struct CCompareOptions
{
    // Consider all files as text files.
    int ForceText;

    // Consider all files as binaty files.
    int ForceBinary;

    // Ignore changes in horizontal white space.
    int IgnoreSpaceChange;

    // Ignore all horizontal white space.
    int IgnoreAllSpace;

    // zahrnuto do ingnore space change
    // Ignore changes that affect only blank lines.
    //int IgnoreBlankLines;

    // Treat linebreaks as regular white space. Implies IgnoreSpaceChange.
    int IgnoreLineBreakChanges;

    // Options.IgnoreSpaceChange || Options.IgnoreAllSpace;
    int IgnoreSomeSpace;

    // Ignore differences in case of letters
    int IgnoreCase;

    int EolConversion[2];

    // Calculate detailed differences, makes sense only when
    // IgnoreLineBreakChanges = 0
    int DetailedDifferences;

    // Input text file encoding, used only for text file comparison
    int Encoding[2];

    // Endians for UTF-16, UTF-32 encodings, used only for text file comparison
    int Endians[2];

    // Recode text file using Salamander conversion table, used only for text
    // file comparison, with ASCII8 encoding
    int PerformASCII8InputEnc[2];

    // Name of conversion table, "" means autodetection, used only for text
    // file comparison, with ASCII8 encoding and PerformASCII8InputEnc != 0
    char ASCII8InputEncTableName[2][101];

    // Normalize Unicode texts to Normalization Form C using normaliz.dll
    BOOL NormalizationForm;
};

extern CCompareOptions DefaultCompareOptions;

// ****************************************************************************
//
// CEditScript
//

struct CChange
{
    union
    {
        struct
        {
            size_t Deleted;   // Length of subsequence deleted from the first sequence.
            size_t Inserted;  // Length of subsequence inserted to the seccond sequence.
            size_t DeletePos; // Position in the first sequence.
            size_t InsertPos; // Position in the seccond sequence.
        };
        struct
        {
            size_t Length[2];
            size_t Position[2];
        };
    };

    CChange(size_t deleted, size_t inserted, size_t deletePos, size_t insertPos) : Deleted(deleted), Inserted(inserted), DeletePos(deletePos),
                                                                                   InsertPos(insertPos) {}
};

typedef std::vector<CChange> CEditScript;

// ****************************************************************************
//
// CBinaryChanges
//

enum
{
    MaxBinChanges = 32 * 1024
};

struct CBinaryChange
{
    QWORD Offset;
    QWORD Length;

    CBinaryChange(QWORD offset, QWORD length)
    {
        Offset = offset;
        Length = length;
    }
};

typedef std::vector<CBinaryChange> CBinaryChanges;

// ****************************************************************************
//
// CLineScript
//

class CLineSpec
{
public:
    // line class
    enum
    {
        lcCommon = 0,
        lcChange = 1,
        lcSeparator = 2,
        lcBlank = 3,
    };

    CLineSpec(int lineClass, int line = 0) : Line(line << LineShift | lineClass & ClassMask), Changes(NULL) {}
    CLineSpec(int lineClass, int line, const std::vector<int>& changes);
    CLineSpec(int lineClass, int line, int begin, int end);
    void DeleteData()
    {
        if (Changes)
            delete Changes;
    }

    int GetLine() const { return Line >> LineShift; }
    int GetClass() const { return int(Line & ClassMask); }
    int* GetChanges() const { return Changes; }

    static int MaxLines() { return ~int(0) >> LineShift; }
    bool IsBlank() const { return GetClass() >= lcSeparator; }

protected:
    enum
    {
        LineShift = 2,
        ClassMask = 3
    };
    int Line;
    int* Changes;
};

//typedef std::vector<CLineSpec> CLineScript;
class CLineScript : public std::vector<CLineSpec>
{
    SUPER(std::vector<CLineSpec>);

public:
    typedef super::value_type value_type;
    CLineScript() : Owner(true) {}
    ~CLineScript() { clear(); }
    void swap(CLineScript& v);
    void clear();
    void release() { Owner = false; }

protected:
    bool Owner;
    // those should be never called, for paranoid people there should be also
    // non-cost variants of (r)begin,(r)end and operator[]
    void resize(size_type, value_type) {}
    void resize(size_type) {}
    void pop_back() {}
    iterator erase(iterator pos) { return pos; }
    iterator erase(iterator first, iterator last) { return first; }
};

// ****************************************************************************
//
// CBinaryCompareResults
//

struct CBinaryCompareResults
{
    struct
    {
        const char* Name;
        QWORD Size;
    } Files[2];
    CCompareOptions& Options;
    QWORD FirstChange;

    CBinaryCompareResults(CCompareOptions& options) : Options(options) {}
};

// ****************************************************************************
//
// Often used types
//

typedef std::vector<int> CIntIndexes; // integer indexes
typedef std::vector<size_t> CIndexes; // positive indexes

// ****************************************************************************
//
// CTextCompareResults
//

template <class CChar>
struct CTextCompareResults
{
    typedef std::vector<CChar*> CLineBuffer;
    struct CFiles
    {
        const char* Name;
        CChar* Text;
        CLineBuffer Lines;
        CLineScript LineScript;
        char Encoding[100];
    } Files[2];
    CCompareOptions& Options;
    CIntIndexes ChangesToLines;
    CIntIndexes ChangesLengths;
    CEditScript Changes; // data to fill combo-box

    CTextCompareResults(CCompareOptions& options) : Options(options) {}
};

// ****************************************************************************
//
// CEditScriptBuilder -- builds an edit script
//

class CEditScriptBuilder
{
public:
    CEditScriptBuilder(CEditScript& editScript)
        : EditScript(editScript), DeletePos(0), InsertPos(0) {}

    void operator()(char op, size_t off, size_t len);

protected:
    size_t DeletePos;
    size_t InsertPos;
    CEditScript& EditScript;
};

// ****************************************************************************
//
// CWorkerFileData
//

struct CWorkerFileData
{
    TCHAR Name[MAX_PATH];
    HANDLE File;
    QWORD Size;

    CWorkerFileData();
    ~CWorkerFileData();
    HANDLE DetachHFile();
};

// ****************************************************************************
//
// CFilecompWorker
//

class CFilecompWorker : public CThread
{
public:
    // obecna chyba
    class CException : public std::exception
    {
    public:
        CException(const char* s = "Unspecified Error.") : exception(s) {}
        static void Raise(int error, int lastError, ...);
    };
    // ukonceno uzivatelem
    class CAbortByUserException : public std::exception
    {
    public:
        CAbortByUserException() : exception("") {}
    };
    // soubory jsou stejny
    class CFilesDontDifferException : public std::exception
    {
    public:
        CFilesDontDifferException() : exception("") {}
    };
    // soubory jsou stejny
    class CAllDiffsIgnoredException : public std::exception
    {
    public:
        CAllDiffsIgnoredException() : exception("") {}
    };

    CFilecompWorker(HWND parent, HWND mainWindow, const char* name0, const char* name1,
                    const CCompareOptions& options, const int& cancelFlag, HANDLE event);

protected:
    HWND Parent;
    HWND MainWindow; // The window receiving WM_USER_WORKERNOTIFIES
    CCompareOptions Options;
    const int& CancelFlag; // 0 ... jedeme dal, jinak konec
    HANDLE Event;

    CWorkerFileData Files[2];

    virtual unsigned Body();
    void GuardedBody();
    void CompareBinaryFiles();
    int CompareBinaryFilesAux(CCachedFile (&cf)[2], QWORD& changeOffs);
    int FindDifferencesBody(CCachedFile (&cf)[2], QWORD changeOffs, CBinaryChanges& changes);

    template <class CChar>
    void CompareTextFiles(CTextFileReader (&reader)[2])
    {
        CFilecompCoWorkerOptimized<CChar>(MainWindow, Options, CancelFlag).Compare(reader);
    }
};
