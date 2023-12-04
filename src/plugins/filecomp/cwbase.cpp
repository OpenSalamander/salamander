// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <iterator>

using namespace std;

// ****************************************************************************
//
// CFilecompCoWorkerBase
//

template <class CChar>
CFilecompCoWorkerBase<CChar>::CFilecompCoWorkerBase(HWND mainWindow,
                                                    CCompareOptions& options, const int& cancelFlag) : MainWindow(mainWindow),
                                                                                                       Options(options), CancelFlag(cancelFlag)
{
}

template <class CChar>
void CFilecompCoWorkerBase<CChar>::ReadFilesAndFindLines(CTextFileReader (&reader)[2], bool& binaryIdentical)
{
    // nacteme soubory
    int i;
    for (i = 0; i < 2; i++)
    {
        Files[i].Name = reader[i].GetName();
        reader[i].Get(Files[i].Begin, Files[i].Length, CancelFlag);
        Files[i].End = Files[i].Begin + Files[i].Length;
    }

    // nejsou nahodou shodne?
    binaryIdentical = false;
    if (reader[0].HasMD5() && reader[1].HasMD5())
    {
        BYTE md5_1[16];
        BYTE md5_2[16];
        reader[0].GetMD5(md5_1);
        reader[1].GetMD5(md5_2);
        binaryIdentical = memcmp(md5_1, md5_2, 16) == 0;
    }

    // najdeme zacatky radku
    for (i = 0; i < 2; i++)
    {
        CChar* lineStart = Files[i].Begin;
        // skip BOM
        if (lineStart < Files[i].End && TCharSpecific<CChar>::IsBOM(*lineStart))
            lineStart++;
        for (CChar* iterator = lineStart; iterator < Files[i].End;)
        {
            if (*iterator == '\n')
            {
                Files[i].Lines.push_back(lineStart);
                lineStart = ++iterator;
                if (CancelFlag)
                    throw CFilecompWorker::CAbortByUserException();
            }
            else
                ++iterator;
        }
        // ukazatel za posledni radku
        Files[i].Lines.push_back(lineStart);
    }

    // jen pro klid duse
    if (Files[i].Lines.size() >= size_t(CLineSpec::MaxLines()))
        CFilecompWorker::CException::Raise(IDS_MAXLINES, 0);
}

template <class CChar>
void CFilecompCoWorkerBase<CChar>::SendResults(CLineScript (&lineScript)[2],
                                               CIntIndexes& changesToLines, CIntIndexes& changesLengths, CEditScript& changes, CTextFileReader (&reader)[2])
{
    CTextCompareResults<CChar> results(Options);
    int i;
    for (i = 0; i < 2; ++i)
    {
        results.Files[i].Name = Files[i].Name;
        results.Files[i].Text = Files[i].Begin;
        results.Files[i].Lines.swap(Files[i].Lines);
        results.Files[i].LineScript.swap(lineScript[i]);
        reader[i].GetEncodingName(results.Files[i].Encoding);
    }
    results.ChangesToLines.swap(changesToLines);
    results.ChangesLengths.swap(changesLengths);
    results.Changes.swap(changes);
    _CrtCheckMemory();
    if (SendMessage(MainWindow, WM_USER_WORKERNOTIFIES,
                    TCharSpecific<CChar>::IsUnicode() ? WN_UNICODE_FILES_DIFFER : WN_TEXT_FILES_DIFFER,
                    (LPARAM)&results))
    {
        // the main window is now responsible to free all allocated data
        Files[0].Begin = NULL;
        Files[1].Begin = NULL;
    }
}

template <class CChar>
void CFilecompCoWorkerBase<CChar>::ScrictCompare(
    size_t (&firstLines)[2],
    size_t (&lenghts)[2],
    CLineScript (&script)[2],
    int lcCommon)
{
    // prepare compare data
    int i;
    for (i = 0; i < 2; ++i)
    {
        CChar* begin = Files[i].Lines[firstLines[i]];
        CChar* end = Files[i].Lines[firstLines[i] + lenghts[i]];
        Strict.CompareData[i].clear();
        Strict.CompareData[i].reserve(end - begin);
        size_t line = firstLines[i];
        for (CChar* iterator = begin; iterator < end;)
        {
            if (Options.IgnoreSomeSpace && IsSpaceX(*iterator))
            {
                if (Options.IgnoreAllSpace)
                {
                    if (*iterator == '\n')
                    {
                        line++;
                        if (CancelFlag)
                            throw CFilecompWorker::CAbortByUserException();
                    }
                    ++iterator;
                }
                else // Options.IgnoreSpaceChange
                {
                    CChar* pos = iterator;
                    size_t l = line;
                    if (*iterator == '\n')
                    {
                        line++;
                        if (CancelFlag)
                            throw CFilecompWorker::CAbortByUserException();
                    }
                    if (Options.IgnoreLineBreakChanges)
                    {
                        int linebreaks = 0;
                        if (*iterator == '\n')
                            linebreaks++;
                        while (++iterator < end && IsSpaceX(*iterator))
                            if (*iterator == '\n')
                                line++, linebreaks++;
                        // Whitespace containing one linebreak matches whitespace without
                        // linebreaks. It is represented by ' ' in compare data.
                        // Blank line matches any sequence of one or more blanklines. Blank
                        // line can contain any whitespace. It is represented by '\n' in
                        // compare data.
                        Strict.CompareData[i].push_back(
                            CCharProxy<CChar>(linebreaks > 1 ? '\n' : ' ', pos, l));
                    }
                    else
                    {
                        if (*iterator == '\n')
                        {
                            while (++iterator < end && *iterator == '\n')
                                line++;
                            Strict.CompareData[i].push_back(CCharProxy<CChar>('\n', pos, l));
                        }
                        else
                        {
                            while (++iterator < end &&
                                   *iterator != '\n' && IsSpaceX(*iterator))
                                ;
                            Strict.CompareData[i].push_back(CCharProxy<CChar>(' ', pos, l));
                        }
                    }
                }
            }
            else
            {
                if (Options.IgnoreCase)
                    Strict.CompareData[i].push_back(
                        CCharProxy<CChar>(ToLowerX(*iterator), iterator, line));
                else
                    Strict.CompareData[i].push_back(
                        CCharProxy<CChar>(*iterator, iterator, line));
                if (*iterator == '\n')
                {
                    line++;
                    if (CancelFlag)
                        throw CFilecompWorker::CAbortByUserException();
                }
                ++iterator;
            }
        }
    }

    // compare data
    Strict.EditScript.clear();
    ptrdiff_t d = diff(
        0, Strict.CompareData[0].size(),
        0, Strict.CompareData[1].size(),
        sequence_comparator(
            Strict.CompareData[0].begin(),
            Strict.CompareData[1].begin()),
        CEditScriptBuilder(Strict.EditScript),
        CancelFlag, INT_MAX, Strict.DiffBuf);
    if (d == -1)
        CFilecompWorker::CException::Raise(IDS_INTERNALERROR, 0);

    // TODO az budu implementovat strict, tak tohle by se melo osetrit
    // if (d == 0)

    // shift-boundaries se vola pred RemoveSingleCharMatches
    ShiftBoundaries(Strict.EditScript, Strict.CompareData);

    // remove sigle char matches
    RemoveSingleCharMatches();

    // build line scripts
    typename CStrictCompareData::iterator cdi[2]; // compare data iterator
    typename CStrictCompareData::iterator mstart; // match start (in sequence 0)
    typename CStrictCompareData::iterator mend;   // match end
    size_t line[2];                               // current line number
    //vector<size_t> Strict.Changes[2];	// changes in current line

    for (i = 0; i < 2; ++i)
    {
        cdi[i] = Strict.CompareData[i].begin();
        line[i] = firstLines[i];
        Strict.Changes[i].clear();
    }

    // NOTE prefixy i suffixy mohou byt ruzne dlouhe -- jinak zalamany stejny
    // text
    for (CEditScript::iterator esi = Strict.EditScript.begin();; ++esi)
    {
        // zpracujeme spolecnou cast
        mstart = cdi[0];
        mend = esi >= Strict.EditScript.end() ? Strict.CompareData[0].end() : Strict.CompareData[0].begin() + esi->DeletePos;
        // break condition is at the loop end
        while (1)
        {
            bool lineChanged[2] = {false, false};
            for (i = 0; i < 2; ++i)
            {
                if (cdi[i] >= Strict.CompareData[i].end())
                    continue;
                if (cdi[i]->CharLine > line[i])
                {
                    lineChanged[i] = true;
                    if (Strict.Changes[i].size())
                    {
                        script[i].push_back(CLineSpec(CLineSpec::lcChange, int(line[i]), Strict.Changes[i]));
                        Strict.Changes[i].clear();
                        line[i]++;
                    }
                    while (cdi[i]->CharLine > line[i])
                    {
                        script[i].push_back(CLineSpec(
                            cdi[0] > mstart ? lcCommon : CLineSpec::lcChange, int(line[i])));
                        line[i]++;
                    }
                }
            }
            for (i = 0; i < 2; ++i)
            {
                if (lineChanged[i] && script[i].size() < script[1 - i].size())
                {
                    // dorovname
                    fill_n(back_inserter(script[i]),
                           script[1 - i].size() - script[i].size(),
                           CLineSpec(CLineSpec::lcBlank));
                }
            }
            // do one extra loop for the first matched data, to ensure that
            // cdi[i]->CharLine = line[i]
            if (cdi[0] >= mend)
                break;
            ++cdi[0], ++cdi[1];
        }
        // konec?
        if (esi >= Strict.EditScript.end())
            break;
        if (CancelFlag)
            throw CFilecompWorker::CAbortByUserException();
        // zpracujeme rozdilnou cast
        for (i = 0; i < 2; ++i)
        {
            if (cdi[i] >= Strict.CompareData[i].end())
                continue;
            size_t endline =
                cdi[i] + esi->Length[i] < Strict.CompareData[i].end() ? (cdi[i] + esi->Length[i])->CharLine : firstLines[i] + lenghts[i];
            if (endline > line[i])
            {
                _ASSERT(cdi[i]->CharLine == line[i]);
                // skript pro aktualni radku
                Strict.Changes[i].push_back((int)(cdi[i]->Position - Files[i].Lines[line[i]]));
                Strict.Changes[i].push_back(
                    (int)(Files[i].Lines[line[i] + 1] - Files[i].Lines[line[i]] - 1));
                script[i].push_back(CLineSpec(CLineSpec::lcChange, int(line[i]), Strict.Changes[i]));
                Strict.Changes[i].clear();
                // doplnime radkama ktere jsou ciste zmeny
                while (endline > ++line[i])
                    script[i].push_back(CLineSpec(CLineSpec::lcChange, int(line[i]), 0,
                                                  int(Files[i].Lines[line[i] + 1] - Files[i].Lines[line[i]]) - 1));
                if (cdi[i] + esi->Length[i] < Strict.CompareData[i].end())
                {
                    // zahajime skript pro posledni radku zmeny
                    Strict.Changes[i].push_back(0);
                    Strict.Changes[i].push_back(
                        (int)((cdi[i] + esi->Length[i])->Position - Files[i].Lines[line[i]]));
                }
            }
            else
            {
                _ASSERT(cdi[i] + esi->Length[i] < Strict.CompareData[i].end());
                // zmena zustava v aktualni radce
                Strict.Changes[i].push_back((int)(cdi[i]->Position - Files[i].Lines[line[i]]));
                Strict.Changes[i].push_back(
                    (int)((cdi[i] + esi->Length[i])->Position - Files[i].Lines[line[i]]));
            }
            cdi[i] += esi->Length[i];
        }
    }
    for (i = 0; i < 2; ++i)
    {
        // flush changes buffer for last changed line
        if (Strict.Changes[i].size())
        {
            script[i].push_back(CLineSpec(CLineSpec::lcChange, int(line[i]), Strict.Changes[i]));
            Strict.Changes[i].clear();
            ++line[i];
        }
        // send remaining lines as common
        for (; line[i] < firstLines[i] + lenghts[i]; ++line[i])
            script[i].push_back(CLineSpec(lcCommon, int(line[i])));
    }

    // // doplnime do plneho poctu radku
    // int i;
    // for (i = 0; i < 2; ++i)
    // {
    //   while (line[i] < File(i).size()-1)
    //   {
    //     script[i].push_back(CLineSpec(script[i].back().GetClass(), line[i]));
    //     line[i]++;
    //   }
    // }

    // dorovname kratsi script
    int s = script[0].size() < script[1].size() ? 0 : 1;
    fill_n(back_inserter(script[s]),
           script[1 - s].size() - script[s].size(),
           CLineSpec(CLineSpec::lcBlank));
}

template <class CChar>
void CFilecompCoWorkerBase<CChar>::RemoveSingleCharMatches()
{
    CEditScript::iterator curr = Strict.EditScript.begin();
    CEditScript::iterator out = curr;
    CEditScript::iterator next;
    size_t compareData_0_Size = Strict.CompareData[0].size();
    size_t compareData_1_Size = Strict.CompareData[1].size();
    for (; curr < Strict.EditScript.end();)
    {
        next = curr + 1;
        while (next < Strict.EditScript.end() &&
               //curr->Length[0] > 0 && curr->Length[1] > 0 &&
               //next->Length[0] > 0 && next->Length[1] > 0 &&
               next->Position[0] < compareData_0_Size &&
               next->Position[1] < compareData_1_Size &&
               Strict.CompareData[0][next->Position[0]].Position -
                       Strict.CompareData[0][curr->Position[0] + curr->Length[0]].Position ==
                   1 &&
               Strict.CompareData[1][next->Position[1]].Position -
                       Strict.CompareData[1][curr->Position[1] + curr->Length[1]].Position ==
                   1 &&
               Strict.CompareData[0][curr->Position[0] + curr->Length[0]].Char != '\n' &&
               Strict.CompareData[1][curr->Position[1] + curr->Length[1]].Char != '\n')
        {
            // this is a single char match, remove it
            curr->Length[0] = next->Position[0] - curr->Position[0] + next->Length[0];
            curr->Length[1] = next->Position[1] - curr->Position[1] + next->Length[1];
            next++;
        }
        *out = *curr;
        ++out;
        curr = next;
    }
    Strict.EditScript.erase(out, Strict.EditScript.end());
}

template <class CChar>
template <class CCompareData>
void CFilecompCoWorkerBase<CChar>::ShiftBoundaries(CEditScript& editScript, CCompareData (&compareData)[2])
{
    //CCounterDriver sb(::ShiftBoundaries);

    // TODO na vektoru tohle muze byt dost pomale, overit, zda se nevyplati
    // to docasne prekopirovat do listu
    int f;
    for (f = 0; f < 2; ++f)
    {
        int other = 1 - f;

        for (CEditScript::iterator change = editScript.begin();
             change < editScript.end(); ++change)
        {
            // ignore changes occured only in other file
            if (change->Length[f] == 0)
                continue;

            while (1)
            {
                size_t begin = change->Position[f];
                size_t end = begin + change->Length[f];

                size_t changeLength = end - begin;

                // move change up, possibly merge with other changes
                while (begin > 0 && compareData[f][begin - 1] == compareData[f][end - 1])
                {
                    --begin;
                    --end;
                    if (change > editScript.begin() &&
                        begin <= (change - 1)->Position[f] + (change - 1)->Length[f])
                    {
                        // merge changes
                        if (change->Length[other])
                        {
                            change->Position[f] += change->Length[f];
                            change->Length[f] = 0;
                        }
                        else
                            change = editScript.erase(change);
                        --change;
                        begin = change->Position[f];
                        change->Length[f] = end - begin;
                    }
                }

                if (begin < change->Position[f])
                {
                    // move change, possibly splitting change to one insert and one delete
                    if (change->Length[other])
                    {
                        change = editScript.insert(change, *change); // insert a copy
                        change->Position[other] -= change->Position[f] - begin;
                        change->Length[other] = 0;
                        change->Position[f] = begin;
                        (change + 1)->Position[f] += (change + 1)->Length[f];
                        (change + 1)->Length[f] = 0;
                    }
                    else
                    {
                        change->Position[other] -= change->Position[f] - begin;
                        change->Position[f] = begin;
                    }
                }

                // move change down, possibly merge with other changes
                while (end < compareData[f].size() && compareData[f][begin] == compareData[f][end])
                {
                    ++begin;
                    ++end;
                    if ((change + 1) < editScript.end() && end >= (change + 1)->Position[f])
                    {
                        // merge changes
                        if (change->Length[other])
                        {
                            change->Length[f] = 0;
                            ++change;
                        }
                        else
                            change = editScript.erase(change);
                        end = change->Position[f] + change->Length[f];
                        change->Position[f] = begin;
                        change->Length[f] = end - begin;
                    }
                }

                if (changeLength == end - begin)
                    break;

                if (begin > change->Position[f])
                {
                    // move change, possibly splitting change to one insert and one delete
                    if (change->Length[other])
                    {
                        change = editScript.insert(change, *change); // insert a copy
                        change->Length[f] = 0;
                        ++change;
                        change->Position[other] += change->Length[other] + (begin - change->Position[f]);
                        change->Length[other] = 0;
                        change->Position[f] = begin;
                    }
                    else
                    {
                        change->Position[other] += begin - change->Position[f];
                        change->Position[f] = begin;
                    }
                }
            }

            if (CancelFlag)
                throw CFilecompWorker::CAbortByUserException();
        }
    }
}

template class CFilecompCoWorkerBase<char>;
template class CFilecompCoWorkerBase<wchar_t>;

template void CFilecompCoWorkerBase<char>::ShiftBoundaries<CIndexes>(CEditScript& editScript, CIndexes (&compareData)[2]);
template void CFilecompCoWorkerBase<wchar_t>::ShiftBoundaries<CIndexes>(CEditScript& editScript, CIndexes (&compareData)[2]);
