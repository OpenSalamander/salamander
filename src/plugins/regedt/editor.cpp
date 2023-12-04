// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

char Command[MAX_PATH];
char Arguments[MAX_PATH];
char InitDir[MAX_PATH];

const char* EXP_FULLNAME = "FullName";
const char* EXP_DRIVE = "Drive";
const char* EXP_PATH = "Path";
const char* EXP_NAME = "Name";
const char* EXP_NAMEPART = "NamePart";
const char* EXP_EXTPART = "ExtPart";
const char* EXP_FULLPATH = "FullPath";
const char* EXP_WINDIR = "WinDir";
const char* EXP_SYSDIR = "SysDir";
const char* EXP_SALDIR = "SalDir";
const char* EXP_DOSFULLNAME = "DOSFullName";
const char* EXP_DOSDRIVE = "DOSDrive";
const char* EXP_DOSPATH = "DOSPath";
const char* EXP_DOSNAME = "DOSName";
const char* EXP_DOSNAMEPART = "DOSNamePart";
const char* EXP_DOSEXTPART = "DOSExtPart";
const char* EXP_DOSFULLPATH = "DOSFullPath";
const char* EXP_DOSWINDIR = "DOSWinDir";
const char* EXP_DOSSYSDIR = "DOSSysDir";

struct CExpData
{
    char Buffer[MAX_PATH];
    const char* LongName;
    const char* DosName;
};

const char* WINAPI
ExecuteFullName(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteFullName(, )");
    CExpData* data = (CExpData*)param;
    return strcpy(data->Buffer, data->LongName);
}

const char* WINAPI
ExecuteDOSFullName(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSFullName(, )");
    CExpData* data = (CExpData*)param;
    return strcpy(data->Buffer, data->DosName);
}

const char* WINAPI
ExecuteDrive(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDrive(, )");
    CExpData* data = (CExpData*)param;
    SG->GetRootPath(data->Buffer, data->LongName);
    SG->SalPathRemoveBackslash(data->Buffer);
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSDrive(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSDrive(, )");
    CExpData* data = (CExpData*)param;
    SG->GetRootPath(data->Buffer, data->DosName);
    SG->SalPathRemoveBackslash(data->Buffer);
    return data->Buffer;
}

const char* WINAPI
ExecutePath(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecutePath(, )");
    CExpData* data = (CExpData*)param;
    SG->GetRootPath(data->Buffer, data->LongName);
    SG->SalPathRemoveBackslash(data->Buffer);
    strcpy(data->Buffer, data->LongName + strlen(data->Buffer));
    char* str = strrchr(data->Buffer, '\\');
    if (str)
        str[1] = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSPath(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSPath(, )");
    CExpData* data = (CExpData*)param;
    SG->GetRootPath(data->Buffer, data->DosName);
    SG->SalPathRemoveBackslash(data->Buffer);
    strcpy(data->Buffer, data->DosName + strlen(data->Buffer));
    char* str = strrchr(data->Buffer, '\\');
    if (str)
        str[1] = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteName(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteName(, )");
    CExpData* data = (CExpData*)param;
    return strcpy(data->Buffer, SG->SalPathFindFileName(data->LongName));
}

const char* WINAPI
ExecuteDOSName(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSName(, )");
    CExpData* data = (CExpData*)param;
    return strcpy(data->Buffer, SG->SalPathFindFileName(data->DosName));
}

const char* WINAPI
ExecuteNamePart(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteNamePart(, )");
    CExpData* data = (CExpData*)param;
    strcpy(data->Buffer, SG->SalPathFindFileName(data->LongName));
    SG->SalPathRemoveExtension(data->Buffer);
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSNamePart(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSNamePart(, )");
    CExpData* data = (CExpData*)param;
    strcpy(data->Buffer, SG->SalPathFindFileName(data->DosName));
    SG->SalPathRemoveExtension(data->Buffer);
    return data->Buffer;
}

const char* WINAPI
ExecuteExtPart(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteExtPart(, )");
    CExpData* data = (CExpData*)param;
    const char* name = SG->SalPathFindFileName(data->LongName);
    const char* ext = strrchr(name, '.');
    if (ext != NULL)
        strcpy(data->Buffer, ++ext); // ".cvspass" ve Windows je pripona
    else
        *data->Buffer = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSExtPart(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSExtPart(, )");
    CExpData* data = (CExpData*)param;
    const char* name = SG->SalPathFindFileName(data->DosName);
    const char* ext = strrchr(name, '.');
    if (ext != NULL)
        strcpy(data->Buffer, ++ext); // ".cvspass" ve Windows je pripona
    else
        *data->Buffer = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteFullPath(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteFullPath(, )");
    CExpData* data = (CExpData*)param;
    strcpy(data->Buffer, data->LongName);
    char* str = strrchr(data->Buffer, '\\');
    if (str)
        str[1] = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSFullPath(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSFullPath(, )");
    CExpData* data = (CExpData*)param;
    strcpy(data->Buffer, data->DosName);
    char* str = strrchr(data->Buffer, '\\');
    if (str)
        str[1] = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteWinDir(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteWinDir(, )");
    CExpData* data = (CExpData*)param;
    UINT l = GetWindowsDirectory(data->Buffer, MAX_PATH);
    if (l < 0 || l >= MAX_PATH)
        *data->Buffer = 0;
    else
        SG->SalPathAddBackslash(data->Buffer, MAX_PATH);
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSWinDir(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSWinDir(, )");
    CExpData* data = (CExpData*)param;
    UINT l = GetWindowsDirectory(data->Buffer, MAX_PATH);
    if (l < 0 || l >= MAX_PATH)
        *data->Buffer = 0;
    else
    {
        if (GetShortPathName(data->Buffer, data->Buffer, MAX_PATH))
            SG->SalPathAddBackslash(data->Buffer, MAX_PATH);
        else
            *data->Buffer = 0;
    }
    return data->Buffer;
}

const char* WINAPI
ExecuteSysDir(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteSysDir(, )");
    CExpData* data = (CExpData*)param;
    UINT l = GetSystemDirectory(data->Buffer, MAX_PATH);
    if (l < 0 || l >= MAX_PATH)
        *data->Buffer = 0;
    else
        SG->SalPathAddBackslash(data->Buffer, MAX_PATH);
    return data->Buffer;
}

const char* WINAPI
ExecuteDOSSysDir(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteDOSSysDir(, )");
    CExpData* data = (CExpData*)param;
    UINT l = GetSystemDirectory(data->Buffer, MAX_PATH);
    if (l < 0 || l >= MAX_PATH)
        *data->Buffer = 0;
    else
    {
        if (GetShortPathName(data->Buffer, data->Buffer, MAX_PATH))
            SG->SalPathAddBackslash(data->Buffer, MAX_PATH);
        else
            *data->Buffer = 0;
    }
    return data->Buffer;
}

const char* WINAPI
ExecutePath2(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecutePath2(, )");
    CExpData* data = (CExpData*)param;
    SG->GetRootPath(data->Buffer, data->LongName);
    SG->SalPathRemoveBackslash(data->Buffer);
    strcpy(data->Buffer, data->LongName + strlen(data->Buffer));
    int l = (int)strlen(data->Buffer);
    if (data->Buffer[l - 1] == '\\' && l > 1)
        data->Buffer[l - 1] = 0;
    return data->Buffer;
}

const char* WINAPI
ExecuteFullPath2(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteFullPath2(, )");
    CExpData* data = (CExpData*)param;
    return strcpy(data->Buffer, data->LongName);
}

const char* WINAPI
ExecuteWinDir2(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteWinDir2(, )");
    CExpData* data = (CExpData*)param;
    UINT l = GetWindowsDirectory(data->Buffer, MAX_PATH);
    if (l < 0 || l >= MAX_PATH)
        *data->Buffer = 0;
    else
        SG->SalPathRemoveBackslash(data->Buffer);
    return data->Buffer;
}

const char* WINAPI
ExecuteSysDir2(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteSysDir2(, )");
    CExpData* data = (CExpData*)param;
    UINT l = GetSystemDirectory(data->Buffer, MAX_PATH);
    if (l < 0 || l >= MAX_PATH)
        *data->Buffer = 0;
    else
        SG->SalPathRemoveBackslash(data->Buffer);
    return data->Buffer;
}

const char* WINAPI
ExecuteSalDir(HWND msgParent, void* param)
{
    CALL_STACK_MESSAGE1("ExecuteSalDir(, )");
    CExpData* data = (CExpData*)param;
    GetModuleFileName(NULL, data->Buffer, MAX_PATH); // hInstance==NULL: chceme cestu k EXE, ne k DLL
    *(strrchr(data->Buffer, '\\') + 1) = 0;
    return data->Buffer;
}

CSalamanderVarStrEntry ExpCommandVariables[] =
    {
        {EXP_WINDIR, ExecuteWinDir},
        {EXP_SYSDIR, ExecuteSysDir},
        {EXP_SALDIR, ExecuteSalDir},
        {NULL, NULL}};

CSalamanderVarStrEntry ExpArgumentsVariables[] =
    {
        {EXP_FULLNAME, ExecuteFullName},
        {EXP_DRIVE, ExecuteDrive},
        {EXP_PATH, ExecutePath},
        {EXP_NAME, ExecuteName},
        {EXP_NAMEPART, ExecuteNamePart},
        {EXP_EXTPART, ExecuteExtPart},
        {EXP_FULLPATH, ExecuteFullPath},
        {EXP_WINDIR, ExecuteWinDir},
        {EXP_SYSDIR, ExecuteSysDir},
        {EXP_DOSFULLNAME, ExecuteDOSFullName},
        {EXP_DOSDRIVE, ExecuteDOSDrive},
        {EXP_DOSPATH, ExecuteDOSPath},
        {EXP_DOSNAME, ExecuteDOSName},
        {EXP_DOSNAMEPART, ExecuteDOSNamePart},
        {EXP_DOSEXTPART, ExecuteDOSExtPart},
        {EXP_DOSFULLPATH, ExecuteDOSFullPath},
        {EXP_DOSWINDIR, ExecuteDOSWinDir},
        {EXP_DOSSYSDIR, ExecuteDOSSysDir},
        {NULL, NULL}};

CSalamanderVarStrEntry ExpInitDirVariables[] =
    {
        {EXP_DRIVE, ExecuteDrive},
        {EXP_PATH, ExecutePath2},
        {EXP_FULLPATH, ExecuteFullPath2},
        {EXP_WINDIR, ExecuteWinDir2},
        {EXP_SYSDIR, ExecuteSysDir2},
        {NULL, NULL}};

BOOL RemoveDoubleBackslahesFromPath(char* text)
{
    if (text == NULL)
    {
        TRACE_E("Unexpected situation in RemoveDoubleBackslahesFromPath().");
        return FALSE;
    }
    int len = (int)strlen(text);
    if (len < 3)
        return TRUE;
    char* s = text + 2; // UNC cesty maji na zacatku "\\"
    char* d = s;
    while (*s != 0)
    {
        if (*s == '\\' && *(s + 1) == '\\')
            s++;
        *d = *s;
        s++;
        d++;
    }
    *d = 0;
    return TRUE;
}

BOOL ExpandCommand(const char* varText, char* buffer, int bufferLen, BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE2("ExpandCommand(, %s, , ,)", varText);
    CExpData data;
    data.LongName = NULL;
    data.DosName = NULL;
    if (SG->ExpandVarString(GetParent(), varText, buffer, bufferLen, ExpCommandVariables, &data,
                            ignoreEnvVarNotFoundOrTooLong))
    {
        // promenne EXECUTE_WINDIR, EXECUTE_SYSDIR a EXECUTE_SALDIR jsou zakonceny zpetnym lomitkem
        // uzivatel doplni vlastni zpetne lomitko, takze jsou ve vysledku v ceste dve
        RemoveDoubleBackslahesFromPath(buffer); // setreseme dvojita zpetna lomitka na jedno
        return TRUE;
    }
    else
        return FALSE;
}

BOOL ExpandInitDir(const char* varText, char* directory,
                   const char* longName, const char* dosName)
{
    CALL_STACK_MESSAGE4("ExpandInitDir(%s, , %s, %s)", varText, longName, dosName);
    CExpData data;
    data.LongName = longName;
    data.DosName = dosName;
    return SG->ExpandVarString(GetParent(), varText, directory, MAX_PATH, ExpInitDirVariables, &data);
}

BOOL ExpandArguments(const char* varText, char* arguments,
                     const char* longName, const char* dosName)
{
    CALL_STACK_MESSAGE4("ExpandArguments(%s, , %s, %s)", varText, longName,
                        dosName);
    CExpData data;
    data.LongName = longName;
    data.DosName = dosName;
    return SG->ExpandVarString(GetParent(), varText, arguments, MAX_PATH, ExpArgumentsVariables, &data);
}

BOOL ExecuteEditor(const char* tempFile)
{
    CALL_STACK_MESSAGE2("ExecuteEditor(%s)", tempFile);
    char command[MAX_PATH];
    char directory[MAX_PATH];
    char arguments[MAX_PATH];

    char longName[MAX_PATH];
    char dosName[MAX_PATH];

    // expandujeme initdir
    SG->CutDirectory(strcpy(longName, tempFile));
    if (!GetShortPathName(longName, dosName, MAX_PATH))
        dosName[0] = 0;

    int e1, e2;
    if (!SG->ValidateVarString(GetParent(), Command, e1, e2, ExpCommandVariables) ||
        !ExpandCommand(Command, command, MAX_PATH, FALSE))
        return FALSE;

    if (!SG->ValidateVarString(GetParent(), InitDir, e1, e2, ExpInitDirVariables) ||
        !ExpandInitDir(InitDir, directory, longName, dosName))
        return FALSE;

    // expandujeme arguments
    if (!GetShortPathName(tempFile, dosName, MAX_PATH))
        dosName[0] = 0;

    if (!SG->ValidateVarString(GetParent(), Arguments, e1, e2, ExpArgumentsVariables) ||
        !ExpandArguments(Arguments, arguments, tempFile, dosName))
        return FALSE;

    // spustime command
    if (!*command)
        return Error(IDS_PROCESS);
    TBuffer<char> cmdLine;
    if (!cmdLine.Reserve((int)strlen(command) + 3 + (int)strlen(arguments) + 1))
        return Error(IDS_LOWMEM);
    SalPrintf(cmdLine.Get(), cmdLine.GetSize(), "\"%s\" %s", command, arguments);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.lpTitle = NULL;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    if (!CreateProcess(NULL, cmdLine.Get(), NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                       NULL, directory[0] ? directory : NULL, &si, &pi))
        return Error(IDS_PROCESS);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return TRUE;
}