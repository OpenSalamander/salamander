// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <unordered_map>
#include <iterator>

using namespace std;

#if (_MSC_VER < 1910) // MSVC 2017 rve kvuli nestandardnimu namespace std::tr1 (deprecated), jde zrejme jen o unordered_map, ktere drive bylo jen v std::tr1 a ted je primo v std
using namespace std::tr1;
#endif

// ****************************************************************************
//
// CFilecompCoWorkerOptimized
//

template <class CChar>
CFilecompCoWorkerOptimized<CChar>::CFilecompCoWorkerOptimized(HWND mainWindow,
                                                              CCompareOptions& options, const int& cancelFlag) : CFilecompCoWorkerBase<CChar>(mainWindow, options, cancelFlag)
{
}

template <class CChar>
struct CToLowerCase
{
    CChar operator()(const CChar& c) const { return ToLowerX(c); }
};

template <class CChar>
struct identity
{
    CChar operator()(const CChar& c) const { return c; }
};

template <class CChar>
class CIgnoringSpace
{
public:
    CIgnoringSpace(const CChar* position) : Position(position)
    {
        // eat a white space at the beginning of a line
        if (*Position != '\n' && IsSpaceX(*Position))
            operator++();
    }
    CIgnoringSpace<CChar>& operator++()
    {
        while (*++Position != '\n' && IsSpaceX(*Position))
            ;
        return *this;
    }
    const CChar& operator*() const { return *Position; }

private:
    const CChar* Position;
};

template <class CChar>
class CIgnoringSpaceChange
{
public:
    CIgnoringSpaceChange(const CChar* position) : Position(position)
    {
        operator++();
        // ignore addition of a white space at the beginning of a line
        if (Value == ' ')
            operator++();
    }
    CIgnoringSpaceChange<CChar>& operator++()
    {
        if (IsSpaceX(*Position))
        {
            do
            {
                if (*Position == '\n')
                {
                    // ignore addition of a white space at the end of a line
                    Value = '\n';
                    return *this;
                }
            } while (IsSpaceX(*++Position));
            Value = ' ';
        }
        else
        {
            Value = *Position;
            ++Position;
        }
        return *this;
    }
    const CChar& operator*() const { return Value; }

private:
    const CChar* Position;
    CChar Value;
};

template <class CChar, class CLineIterator, class CCaseConverter>
struct CHash
{
    CCaseConverter ConvertCase;
    size_t operator()(const CChar* line) const
    {
        CLineIterator i(line);
        unsigned long h = 0; // hashing from STLport
        while (*i != '\n')
        {
            h = (h << 2) + h + ConvertCase(*i);
            ++i;
        }
        return size_t(h);
    }
};

template <class CChar, class CLineIterator, class CCaseConverter>
struct CLineEqual
{
    CCaseConverter ConvertCase;
    bool operator()(const CChar* first, const CChar* second) const
    {
        CLineIterator i(first), j(second);
        while (ConvertCase(*i) == ConvertCase(*j))
        {
            if (*i == '\n')
                return true;
            ++i, ++j;
        }
        return false;
    }
};

template <class CChar>
template <class CCaseConverter, class CLineIterator>
void CFilecompCoWorkerOptimized<CChar>::IdentifyLines(CFilecompCoWorkerBase<CChar>::CFCFileData (&files)[2], CIndexes (&compareData)[2], const int& cancel)
{
    typedef vector<CChar*> CLineBuffer;
    typedef unordered_map<
        const CChar*, size_t,
        CHash<CChar, CLineIterator, CCaseConverter>,
        CLineEqual<CChar, CLineIterator, CCaseConverter>>
        CLineClasses;

    CLineClasses classes;
    size_t next = 0; // next class ID

    int i;
    for (i = 0; i < 2; ++i)
    {
        // rezerve space
        compareData[i].resize(files[i].Lines.size() - 1);
        // indentify each line
        typename CLineBuffer::iterator line = files[i].Lines.begin();
        CIndexes::iterator eqvclass = compareData[i].begin();
        for (; line < files[i].Lines.end() - 1; ++line, ++eqvclass)
        {

            std::pair<CLineClasses::iterator, bool> ir =
                classes.insert(CLineClasses::value_type(*line, next));
            if (ir.second)
                next++; // new class created
            *eqvclass = ir.first->second;
            if (cancel)
                throw CFilecompWorker::CAbortByUserException();
        }
    }
}

template <class CChar>
bool CFilecompCoWorkerOptimized<CChar>::IsChangeIgnorable(const CChange& change)
{
    int i;
    for (i = 0; i < 2; ++i)
    {
        size_t line;
        for (line = change.Position[i];
             line < change.Position[i] + change.Length[i]; ++line)
        {
            for (const CChar* ci = this->Files[i].Lines[line]; *ci != '\n'; ++ci)
            {
                if (!IsSpaceX(*ci))
                    return false;
            }
        }
    }
    return true;
}

template <class CChar>
void CFilecompCoWorkerOptimized<CChar>::Compare(CTextFileReader (&reader)[2])
{
    //  CCounterDriver tr(TotalRuntime);

    bool binaryIdentical; // Are files binary identical?

    this->ReadFilesAndFindLines(reader, binaryIdentical);

    CEditScript editScript;
    ptrdiff_t d = 0;
    if (!binaryIdentical)
    {
        // do the comparison

        // prepare compare data
        CIndexes compareData[2];
        if (this->Options.IgnoreCase)
        {
            if (this->Options.IgnoreAllSpace)
                IdentifyLines<CToLowerCase<CChar>, CIgnoringSpace<CChar>>(this->Files, compareData, this->CancelFlag);
            elif (this->Options.IgnoreSpaceChange)
                IdentifyLines<CToLowerCase<CChar>, CIgnoringSpaceChange<CChar>>(this->Files, compareData, this->CancelFlag);
            else IdentifyLines<CToLowerCase<CChar>, const CChar*>(this->Files, compareData, this->CancelFlag);
        }
        else
        {
            if (this->Options.IgnoreAllSpace)
                IdentifyLines<::identity<CChar>, CIgnoringSpace<CChar>>(this->Files, compareData, this->CancelFlag);
            elif (this->Options.IgnoreSpaceChange)
                IdentifyLines<::identity<CChar>, CIgnoringSpaceChange<CChar>>(this->Files, compareData, this->CancelFlag);
            else IdentifyLines<::identity<CChar>, const CChar*>(this->Files, compareData, this->CancelFlag);
        }

        // compare the two sequences sequence
        d = diff(
            0, compareData[0].size(),
            0, compareData[1].size(),
            sequence_comparator(
                compareData[0].begin(),
                compareData[1].begin()),
            CEditScriptBuilder(editScript),
            this->CancelFlag);
        if (d == -1)
            CFilecompWorker::CException::Raise(IDS_INTERNALERROR, 0);
        /*  if (d == 0) We now let the file display
    {
      throw CFilecompWorker::CFilesDontDifferException();
      }
    }*/

        // shift-boundaries, volat pred RemoveSingleCharMatches
        this->ShiftBoundaries(editScript, compareData);
    }

    // let main window know that we finished basic comparison
    if (this->Options.DetailedDifferences)
        SendMessage(this->MainWindow, WM_USER_WORKERNOTIFIES, WN_CALCULATING_DETAILS, 0);

    // build line scripts
    CLineScript script[2]; // line script
    size_t li[2];          // line index
    size_t mend;           // match end (in sequence 0)
    size_t dend;           // diff end
    CIntIndexes changesToLines;
    CIntIndexes changesLengths;
    CEditScript changes; // data to fill combo-box

    int j;
    for (j = 0; j < 2; ++j)
    {
        script[j].reserve(max(this->Files[0].Lines.size(), this->Files[1].Lines.size()));
        li[j] = 0;
    }

    changesToLines.reserve(editScript.size());
    changesLengths.reserve(editScript.size());
    changes.reserve(editScript.size());

    for (CEditScript::iterator esi = editScript.begin();; ++esi)
    {
        // zpracujeme spolecnou cast
        mend = esi >= editScript.end() ? this->Files[0].Lines.size() - 1 : esi->DeletePos;
        for (; li[0] < mend; li[0]++, li[1]++)
        {
            script[0].push_back(CLineSpec(CLineSpec::lcCommon, int(li[0])));
            script[1].push_back(CLineSpec(CLineSpec::lcCommon, int(li[1])));
        }
        // konec?
        if (esi >= editScript.end())
            break;
        if (this->CancelFlag)
            throw CFilecompWorker::CAbortByUserException();
        // zpracujeme rozdilnou cast
        if ((this->Options.IgnoreSpaceChange || this->Options.IgnoreAllSpace) &&
            IsChangeIgnorable(*esi))
        {
            // encode this change like CLineSpec::lcCommon
            int i;
            for (i = 0; i < 2; ++i)
            {
                dend = li[i] + esi->Length[i];
                for (; li[i] < dend; ++li[i])
                    script[i].push_back(CLineSpec(CLineSpec::lcCommon, int(li[i])));
            }
        }
        else
        {
            changes.push_back(*esi);
            changesToLines.push_back((int)script[0].size()); // oba by tu meli byt stejne dlouhe
            if (esi->Length[0] && esi->Length[1] && this->Options.DetailedDifferences)
            {
                this->ScrictCompare(esi->Position, esi->Length, script, CLineSpec::lcChange);
                li[0] += esi->Length[0];
                li[1] += esi->Length[1];
            }
            else
            {
                int i;
                for (i = 0; i < 2; ++i)
                {
                    dend = li[i] + esi->Length[i];
                    for (; li[i] < dend; ++li[i])
                        script[i].push_back(CLineSpec(CLineSpec::lcChange, int(li[i])));
                }
            }
            changesLengths.push_back(
                (int)max(script[0].size(), script[1].size()) - changesToLines.back());
        }
        // dorovname kratsi script
        int s = script[0].size() < script[1].size() ? 0 : 1;
        fill_n(back_inserter(script[s]),
               script[1 - s].size() - script[s].size(),
               CLineSpec(CLineSpec::lcBlank));
    }

    /*  if (changesToLines.size() == 0) // all changes were ignorable
  {
    throw CFilecompWorker::CFilesDontDifferException();
  }*/

    if (changesToLines.size() == 0) // all changes were ignorable
    {                               // Check & remmber the # of changes. SendResults resets it to 0.
        // d == 0 from above means no changes
        d = 0;
    }
    // send results
    this->SendResults(script, changesToLines, changesLengths, changes, reader);
    if (d == 0)
    {
        if (binaryIdentical)
            throw CFilecompWorker::CFilesDontDifferException();
        else
            throw CFilecompWorker::CAllDiffsIgnoredException();
    }
    SendMessage(this->MainWindow, WM_USER_WORKERNOTIFIES, WN_COMPARE_FINISHED, 0);
}

template class CFilecompCoWorkerOptimized<char>;
template class CFilecompCoWorkerOptimized<wchar_t>;
