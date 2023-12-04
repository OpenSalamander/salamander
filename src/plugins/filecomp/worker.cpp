// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <functional>

using namespace std;

#undef min
#undef max

CCompareOptions DefaultCompareOptions =
    {
        0, // int ForceText;
        0, // int ForceBinary;
        0, // int IgnoreSpaceChange;
        0, // int IgnoreAllSpace;
        0, // int IgnoreLineBreakChanges;
        0, // int IgnoreSomeSpace;
        0, // int IgnoreCaseFlag;
        {  // int EolConversion[2];
         CTextFileReader::eolCR | CTextFileReader::eolCRLF | CTextFileReader::eolNULL,
         CTextFileReader::eolCR | CTextFileReader::eolCRLF | CTextFileReader::eolNULL},
        0, // int DetailedDifferences;
        {  // int Encoding[2];
         CTextFileReader::encUnknown,
         CTextFileReader::encUnknown},
        {// int Endians[2];
         CTextFileReader::endianLittle,
         CTextFileReader::endianLittle},
        {// int PerformASCII8InputEnc[2];
         0,
         0},
        {// char ASCII8InputEncTableName[2][101];
         "",
         ""},
        TRUE //  BOOL           NormalizationForm;
};

// ****************************************************************************
//
// CLineScript
//

CLineSpec::CLineSpec(int lineClass, int line, const vector<int>& changes) : Line(line << LineShift | lineClass & ClassMask)
{
    if (changes.size())
    {
        Changes = new int[changes.size() + 1];
        Changes[0] = int(changes.size());
        copy(changes.begin(), changes.end(), Changes + 1);
    }
    else
    {
        Changes = NULL;
    }
}

CLineSpec::CLineSpec(int lineClass, int line, int begin, int end)
    : Line(line << LineShift | lineClass & ClassMask)
{
    Changes = new int[3];
    Changes[0] = 2;
    Changes[1] = begin;
    Changes[2] = end;
}

void CLineScript::swap(CLineScript& v)
{
    std::swap(v.Owner, Owner);
    super::swap(v);
}

void CLineScript::clear()
{
    if (Owner)
        for_each(begin(), end(), std::mem_fn(&CLineSpec::DeleteData));
    super::clear();
}

// ****************************************************************************
//
// CEditScriptBuilder -- builds an edit script
//

void CEditScriptBuilder::operator()(char op, size_t off, size_t len)
{
    //CCounterDriver esb(EditScriptBuilder);

    if (len == 0)
        return;

    switch (op)
    {
    case diff_base::ed_match:
        DeletePos += len;
        InsertPos += len;
        break;

    case diff_base::ed_delete:
        if (EditScript.empty() || EditScript.back().DeletePos + EditScript.back().Deleted != DeletePos)
            EditScript.push_back(CChange(len, 0, DeletePos, InsertPos));
        else
            EditScript.back().Deleted += len;
        DeletePos += len;
        break;

    case diff_base::ed_insert:
        if (EditScript.empty() || EditScript.back().InsertPos + EditScript.back().Inserted != InsertPos)
            EditScript.push_back(CChange(0, len, DeletePos, InsertPos));
        else
            EditScript.back().Inserted += len;
        InsertPos += len;
        break;
    }
}

// ****************************************************************************
//
// CWorkerFileData
//

CWorkerFileData::CWorkerFileData()
{
    File = INVALID_HANDLE_VALUE;
}

CWorkerFileData::~CWorkerFileData()
{
    if (File != INVALID_HANDLE_VALUE)
        CloseHandle(File);
}

HANDLE
CWorkerFileData::DetachHFile()
{
    HANDLE ret = File;
    File = INVALID_HANDLE_VALUE;
    return ret;
}

// ****************************************************************************
//
// CFilecompWorker
//

CFilecompWorker::CFilecompWorker(HWND parent, HWND mainWindow, const char* name0, const char* name1, const CCompareOptions& options, const int& cancelFlag, HANDLE event) : CancelFlag(cancelFlag)
{
    Parent = Parent;
    MainWindow = mainWindow;
    strcpy(Files[0].Name, name0);
    strcpy(Files[1].Name, name1);
    Options = options;
    Event = event;
}

void CFilecompWorker::CException::Raise(int error, int lastError, ...)
{
    CALL_STACK_MESSAGE3("CFilecompWorker::CWorkerException::Raise(%d, %d, )", error, lastError);
    va_list arglist;
    va_start(arglist, lastError);
    char buf[1024]; //temp variable
    *buf = 0;
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastError != ERROR_SUCCESS)
    {
        int l = lstrlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    throw CException(buf);
}

unsigned
CFilecompWorker::Body()
{
    CALL_STACK_MESSAGE1("CFilecompWorker::CThreadBody()");
    try
    {
        GuardedBody();
    }
    catch (CException e)
    {
        TRACE_I("Error in worker. " << e.what());
        SendMessage(MainWindow, WM_USER_WORKERNOTIFIES, WN_ERROR, (LPARAM)e.what());
    }
    catch (diff_exception e)
    {
        TRACE_I("Operation canceled. CancelFlag = " << CancelFlag);
        PostMessage(MainWindow, WM_USER_WORKERNOTIFIES, WN_WORKER_CANCELED, 0);
    }
    catch (CAbortByUserException e)
    {
        TRACE_I("Operation canceled. CancelFlag = " << CancelFlag);
        PostMessage(MainWindow, WM_USER_WORKERNOTIFIES, WN_WORKER_CANCELED, 0);
    }
    catch (CFilesDontDifferException e)
    {
        TRACE_I("Files don't differ. " << e.what());
        PostMessage(MainWindow, WM_USER_WORKERNOTIFIES, WN_NO_DIFFERENCE, 0);
    }
    catch (CAllDiffsIgnoredException e)
    {
        TRACE_I("All differences were ignored. " << e.what());
        PostMessage(MainWindow, WM_USER_WORKERNOTIFIES, WN_NO_ALL_DIFFS_IGNORED, 0);
    }
    SetEvent(Event);
    _CrtCheckMemory();
    return 0;
}

void CFilecompWorker::GuardedBody()
{
    // otevreme soubory
    int i;
    for (i = 0; i <= 1; i++)
    {
        // Patera 2008.12.28: FILE_SHARE_WRITE added to support files locked by others
        // NOTE: IntViewer can open such files, users wants FC to support them as well
        // See https://forum.altap.cz/viewtopic.php?t=2675
        // See also CHexFileViewWindow::SetData()
        Files[i].File = CreateFile(Files[i].Name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (Files[i].File == INVALID_HANDLE_VALUE)
            CException::Raise(IDS_OPEN, GetLastError(), Files[i].Name);

        CQuadWord size;
        DWORD err;
        if (!SG->SalGetFileSize(Files[i].File, size, err))
            CException::Raise(IDS_ACCESFILE, err, Files[i].Name);
        Files[i].Size = size.Value;

        if (Files[i].Size > QWORD(numeric_limits<int>::max() - 1))
        {
            if (Options.ForceText)
                CException::Raise(IDS_LARGEFILE, 0, Files[i].Name);
            Options.ForceBinary;
        }
    }

    if (Options.ForceBinary)
    {
        // srovname jako binarni soubory
        CompareBinaryFiles();
    }
    else
    {
        CTextFileReader files[2];

        int j;
        for (j = 0; j <= 1; j++)
        {
            files[j].Set(Files[j].Name, Files[j].File, size_t(Files[j].Size),
                         Options.EolConversion[j], (CTextFileReader::eEncoding)Options.Encoding[j],
                         (CTextFileReader::eEndian)Options.Endians[j], Options.PerformASCII8InputEnc[j],
                         Options.ASCII8InputEncTableName[j], Options.NormalizationForm,
                         Files[0].Size == Files[1].Size);
            if (Options.ForceText)
                files[j].ForceType(CTextFileReader::ftText);
        }

        if (files[0].GetType() == CTextFileReader::ftText &&
            files[1].GetType() == CTextFileReader::ftText)
        {
            // srovnam jako text

            if (Options.IgnoreLineBreakChanges)
            {
                Options.IgnoreSpaceChange = 1; // pojiska
                Options.DetailedDifferences = 1;
            }
            Options.IgnoreSomeSpace = Options.IgnoreSpaceChange || Options.IgnoreAllSpace;

            if (files[0].GetEncoding() == CTextFileReader::encASCII8 &&
                files[1].GetEncoding() == CTextFileReader::encASCII8)
            {
                // srovname jako ASCII8 text
                CompareTextFiles<char>(files);
                //TRACE_I("Total time spent by comparing files " << TotalRuntime.Read());
                //TRACE_I("Total time spent by shifting boundaries " << ShiftBoundaries.Read());
                //TRACE_I("Total time spent by diff algorithm " << Diff.Read());
                //TRACE_I("Total time spent by edit script builder " << EditScriptBuilder.Read());
                //TotalRuntime.Reset();
                //ShiftBoundaries.Reset();
                //Diff.Reset();
            }
            else
            {
                // srovname jako UNICODE text
                if (files[0].HasSurrogates() || files[1].HasSurrogates())
                {
                    // srovname jako UCS-4/UTF-32 text
                    // TODO nekdy to treba budem podporovat, zatim umime jen BMP
                    // tento kod se nikdy nespusti, HasSurrogates vraci vzdy false
                    throw CFilecompWorker::CException("Only BMP character set is supported.");
                    //CompareTextFiles<DWORD>(files);
                }
                else
                {
                    // srovname jako UCS-2 text (UTF-16 without surrogates)
                    // TODO pritomne surrogates ingorujeme, uzivatel dostane warning
                    CompareTextFiles<wchar_t>(files);
                }
            }
        }
        else
        {
            // uvolnime pamet
            files[0].Reset();
            files[1].Reset();
            // srovname jako binarni soubory
            CompareBinaryFiles();
        }
    }
}
