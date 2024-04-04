// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mapi.h"

CSimpleMAPI::CSimpleMAPI()
    : FileNames(10, 50), TotalSize(0, 0)
{
    CALL_STACK_MESSAGE_NONE
    HLibrary = NULL;
    MAPISendMail = NULL;
};

CSimpleMAPI::~CSimpleMAPI()
{
    CALL_STACK_MESSAGE_NONE
    Release();
}

BOOL CSimpleMAPI::Init(HWND hParent)
{
    CALL_STACK_MESSAGE_NONE
    if (HLibrary == NULL)
    {
        HLibrary = HANDLES_Q(LoadLibrary("mapi32.dll"));
        if (HLibrary == NULL)
        {
            // under NT4.0 US + IE 4.01 US, mapi32.dll is not installed, but
            // I found the msoemapi.dll there, which has the necessary export and mainly
            // it works, so why not try it...
            HLibrary = HANDLES_Q(LoadLibrary("msoemapi.dll"));
            if (HLibrary == NULL)
            {
                SalMessageBox(hParent, LoadStr(IDS_EMAILFILES_MAPIERROR), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
        }

        MAPISendMail = (PFNMAPISENDMAIL)GetProcAddress(HLibrary, "MAPISendMail"); // no header

        if (MAPISendMail == NULL)
        {
            Release();
            SalMessageBox(hParent, LoadStr(IDS_EMAILFILES_MAPIERROR), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    }
    return TRUE;
}

void CSimpleMAPI::Release()
{
    CALL_STACK_MESSAGE1("CSimpleMAPI::Release()");
    // free allocated strings
    int i;
    for (i = 0; i < FileNames.Count; i++)
        free(FileNames[i]);
    FileNames.DestroyMembers();
    // Unbind functions
    MAPISendMail = NULL;
    // release the library
    if (HLibrary != NULL)
    {
        HANDLES(FreeLibrary(HLibrary));
        HLibrary = NULL;
    }
    TotalSize.Set(0, 0);
}

BOOL CSimpleMAPI::AddFile(const char* fileName, const CQuadWord* size)
{
    CALL_STACK_MESSAGE_NONE
    int len = (int)strlen(fileName);

    char* text = (char*)malloc(len + 1);
    if (text == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    FileNames.Add(text);
    if (!FileNames.IsGood())
    {
        delete text;
        FileNames.ResetState();
        return FALSE;
    }

    memcpy(text, fileName, len + 1);
    TotalSize += *size;

    return TRUE;
}

BOOL CSimpleMAPI::SendMail()
{
    CALL_STACK_MESSAGE1("CSimpleMAPI::SendMail()");
    if (HLibrary == NULL)
    {
        TRACE_E("HLibrary == NULL");
        return FALSE;
    }
    if (FileNames.Count == 0)
    {
        TRACE_E("FileNames.Count == 0");
        return FALSE;
    }

    MapiFileDesc* fileDesc = (MapiFileDesc*)malloc(FileNames.Count * sizeof(MapiFileDesc));
    if (fileDesc == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    ZeroMemory(fileDesc, FileNames.Count * sizeof(MapiFileDesc));

    MapiFileDesc* iterator = fileDesc;
    int subjectSize = 0;
    int i;
    for (i = 0; i < FileNames.Count; i++)
    {
        char* fileName = FileNames[i];
        iterator->nPosition = (ULONG)-1;   // position not specified
        iterator->lpszPathName = fileName; // pathname
        char* p = strrchr(fileName, '\\');
        if (p == NULL)
            p = fileName;
        else
            p++;
        iterator->lpszFileName = p;        // filename visible for user
        subjectSize += (int)strlen(p) + 2; // file_name1, file_name2, ....
        iterator++;
    }

    MapiMessage message;
    ZeroMemory(&message, sizeof(message));
    message.nFileCount = FileNames.Count;
    message.lpFiles = fileDesc;

    char* subject = (char*)malloc(subjectSize + strlen(LoadStr(IDS_EMAILFILES_SUBJECT)) + 2); // space for emailing and terminating NULL
    if (subject == NULL)
    {
        TRACE_E(LOW_MEMORY);
        free(fileDesc);
        return FALSE;
    }

    char* p = subject;
    p += sprintf(p, "%s ", LoadStr(IDS_EMAILFILES_SUBJECT));
    iterator = fileDesc;
    for (i = 0; i < FileNames.Count; i++)
    {
        int len = (int)strlen(iterator->lpszFileName);
        memcpy(p, iterator->lpszFileName, len);
        p += len;
        if (i < FileNames.Count - 1)
        {
            memcpy(p, ", ", len);
            p += 2;
        }
        else
            *p = 0;
        iterator++;
    }
    message.lpszSubject = subject;

    char body[1000];
    strcpy(body, LoadStr(IDS_EMAILFILES_BODY));
    message.lpszNoteText = body;

    ULONG ret = MAPISendMail(0,
                             /*(ULONG)hParent*/ 0, // we will be unable
                             &message,
                             MAPI_LOGON_UI | MAPI_DIALOG,
                             0L);

    free(subject);
    free(fileDesc);
    // We do not report anything because different clients return different return values
    // for example Outlook 6 under XP reported 1 when canceling a letter or 3 when canceling
    // connection guide (when not configured)
    /*    if (ret != 0)
  {
    char buff[50];
    sprintf(buff, "error 3: %d", err);
    SalMessageBox(hParent, buff, LoadStr(IDS_ERRORTITLE),
                  MB_OK | MB_ICONEXCLAMATION);
    return FALSE;
  }*/
    return TRUE;
}

unsigned SimpleMAPISendMailThreadBody(void* param)
{
    CALL_STACK_MESSAGE1("SimpleMAPISendMailThreadBody()");
    SetThreadNameInVCAndTrace("MapiSendMail");
    TRACE_I("Begin");
    // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    CSimpleMAPI* mapi = (CSimpleMAPI*)param;
    BOOL ret = mapi->SendMail();
    delete mapi;

    TRACE_I("End");
    return ret ? 1 : 0;
}

unsigned SimpleMAPISendMailThreadEH(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return SimpleMAPISendMailThreadBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread SimpleMAPISendMailThreadBody: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI SimpleMAPISendMailThread(void* param)
{
    CALL_STACK_MESSAGE_NONE
    CCallStack stack;
    return SimpleMAPISendMailThreadEH(param);
}

BOOL SimpleMAPISendMail(CSimpleMAPI* mapi)
{
    CALL_STACK_MESSAGE_NONE
    HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    DWORD threadID;
    HANDLE thread = HANDLES(CreateThread(NULL, 0, SimpleMAPISendMailThread, mapi, 0, &threadID));
    if (thread == NULL)
    {
        TRACE_E("Unable to start SimpleMAPISendMailThread thread.");
        delete mapi;
        SetCursor(hOldCur);
        return FALSE;
    }

    AddAuxThread(thread); // add a thread between existing viewers (kill on exit)
    SetCursor(hOldCur);
    return TRUE;
}
