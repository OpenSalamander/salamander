// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
//#include <windows.h>
#include <crtdbg.h>
#include <stdio.h>
#include <conio.h>

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "selfextr\\comdefs.h"
#include "typecons.h"
#include "sfxmake\\sfxmake.h"
#include "chicon.h"
#include "crc32.h"
#include "iosfxset.h"
#include "checkzip.h"
#include "zip2sfx.h"
#include "inflate.h"

#include "zip2sfx.rh"

#define STRING(code, string) string,
const char* const StringTable[] =
    {
#include "texts.h"
        NULL};

#undef STRING

const char* ZipName; // archive
HANDLE ZipFile = INVALID_HANDLE_VALUE;
DWORD ArcSize;
DWORD EOCentrDirOffs;
BOOL Encrypt = FALSE;

char ExeName[MAX_PATH]; // exe
HANDLE ExeFile = INVALID_HANDLE_VALUE;

char* Param = NULL; // pointer to parameter (either 'p' or 's' followed by a filename)

HANDLE SettingsFile = INVALID_HANDLE_VALUE; // optional settings file
char* SettingsTextData;

HANDLE SfxPackage = INVALID_HANDLE_VALUE; // sfx package

CSfxSettings Settings; // sfx options
char About[SE_MAX_ABOUT];
char DefVendor[SE_MAX_VENDOR];
char DefWWW[SE_MAX_WWW];
char DefAbout[SE_MAX_ABOUT];
CIcon* Icons = NULL;
int IconsCount;

char* IOBuffer;
__UINT32* CrcTab = NULL;
BOOL OvewriteExe = FALSE;

BOOL InflatingTexts;

BOOL Error(int error, ...)
{
    int lastErr = GetLastError();
    va_list arglist;
    va_start(arglist, error);
    vprintf(StringTable[error], arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        char buf[1024]; //temp variable
        *buf = 0;
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 1024, NULL);
        printf("%s", buf);
    }

    return FALSE;
}

BOOL Read(HANDLE file, void* buffer, DWORD size)
{
    DWORD read;
    if (!ReadFile(file, buffer, size, &read, NULL))
        return FALSE;
    if (read != size)
        return Error(STR_EOF);
    return TRUE;
}

BOOL Write(HANDLE file, const void* buffer, DWORD size)
{
    DWORD written;
    if (!WriteFile(file, buffer, size, &written, NULL))
        return FALSE;
    return TRUE;
}

BOOL PathRenameExtension(LPTSTR pszPath, LPCTSTR pszExt)
{
    char* ext = strrchr(pszPath, '.');
    if (ext != NULL)
        *ext = 0; // ".cvspass" ve Windows je pripona
    lstrcat(pszPath, pszExt);
    return TRUE;
}

/*
BOOL ProcessCommandline(int argc, char* argv[])
{
  int i = 1;
  if (argv[i][0] == '-')
  {
    switch (argv[i][1])
    { 
      case 'p': 
      case 's': Param = argv[i] + 1; break;
      //case 'S': Param = argv[i][1]; break;
      default: return FALSE;
    }
    i++;
  }
  else Param = 0;
  if (i >= argc) return 1;
  ZipName = argv[i];
  lstrcpy(ExeName, ZipName);
  PathRenameExtension(ExeName, ".exe");
  i++;
  if (i >= argc) return FALSE;
  lstrcpy(ExeName, argv[i]);
  if (i + 1 < argc) return FALSE;
  return TRUE;
}
*/

BOOL ProcessCommandline(int argc, char* argv[])
{
    BOOL b = FALSE;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
            case 'p':
            case 's':
                if (!b)
                {
                    b = TRUE;
                    Param = argv[i] + 1;
                }
                else
                    return FALSE;
                break;

            //case 'S': Param = argv[i][1]; break;
            case 'o':
                OvewriteExe = TRUE;
                break;
            default:
                return FALSE;
            }
        }
        else
            break;
    }
    if (i >= argc)
        return FALSE;
    ZipName = argv[i];
    const char* file = strrchr(ZipName, '\\');
    if (file)
        file++;
    else
        file = ZipName;
    lstrcpy(ExeName, file);
    PathRenameExtension(ExeName, ".exe");
    i++;
    if (i >= argc)
        return TRUE;
    lstrcpy(ExeName, argv[i]);
    if (i + 1 < argc)
        return FALSE;
    return TRUE;
}

DWORD SalGetFileAttributes(const char* fileName)
{
    int fileNameLen = (int)strlen(fileName);
    char fileNameCopy[3 * MAX_PATH];
    // pokud cesta konci mezerou/teckou, musime pripojit '\\', jinak GetFileAttributes
    // mezery/tecky orizne a pracuje tak s jinou cestou + u souboru to sice nefunguje,
    // ale porad lepsi nez ziskat atributy jineho souboru/adresare (pro "c:\\file.txt   "
    // pracuje se jmenem "c:\\file.txt")
    if (fileNameLen > 0 && (fileName[fileNameLen - 1] <= ' ' || fileName[fileNameLen - 1] == '.') &&
        fileNameLen + 1 < _countof(fileNameCopy))
    {
        memcpy(fileNameCopy, fileName, fileNameLen);
        fileNameCopy[fileNameLen] = '\\';
        fileNameCopy[fileNameLen + 1] = 0;
        return GetFileAttributes(fileNameCopy);
    }
    else // obycejna cesta, neni co resit, jen zavolame windowsovou GetFileAttributes
    {
        return GetFileAttributes(fileName);
    }
}

void GetZip2SfxDir(char* zip2sfxDir)
{
    if (GetModuleFileName(NULL, zip2sfxDir, MAX_PATH))
    {
        char* name = strrchr(zip2sfxDir, '\\');
        if (name != NULL)
            *++name = 0;
        else
            zip2sfxDir[0] = 0;
    }
    else
        zip2sfxDir[0] = 0;
}

BOOL LoadSettings()
{
    HANDLE file = CreateFile(Param + 1, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return Error(STR_ERROPEN, Param + 1);

    BOOL ret = TRUE;
    DWORD fsiz = GetFileSize(file, NULL);
    if (fsiz != 0xFFFFFFFF)
    {
        SettingsTextData = (char*)malloc(fsiz + 1);
        if (SettingsTextData)
        {
            if (Read(file, SettingsTextData, fsiz))
            {
                char zip2sfxDir[MAX_PATH];
                GetZip2SfxDir(zip2sfxDir);

                SettingsTextData[fsiz] = 0;
                int err;
                switch (ImportSFXSettings(SettingsTextData, &Settings, zip2sfxDir))
                {
                case 0:
                    err = 0;
                    break; //OK
                case 1:
                    err = STR_BADTEMP;
                    break;
                case 2:
                    err = STR_MISBAR;
                    break;
                case 3:
                    err = STR_BADVAR;
                    break;
                case 4:
                    err = STR_BADKEY;
                    break;
                case 5:
                    err = STR_MISSINGVERSION;
                    break;
                case 6:
                    err = STR_BADVERSION;
                    break;
                case 8:
                    err = STR_BADMSGBOXTYPE;
                    break;
                case 7:
                default:
                    err = STR_BADSETFORMAT;
                    break;
                }
                if (err)
                    ret = Error(err);
            }
            else
                ret = Error(STR_ERRREAD, Param + 1);
        }
        else
            ret = Error(STR_LOWMEM);
    }
    else
        ret = Error(STR_ERRACCESS, Param + 1);

    CloseHandle(file);

    return ret;
}

BOOL LoadDefaults()
{
    CSfxFileHeader sfxHead;
    Settings.Flags = SE_SHOWSUMARY;
    *Settings.Command = 0;
    *Settings.TargetDir = 0;
    Settings.MBoxStyle = MB_OK;
    Settings.SetMBoxText("");
    *Settings.MBoxTitle = 0;
    *Settings.WaitFor = 0;
    SfxPackage = CreateFile(Settings.SfxFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (SfxPackage == INVALID_HANDLE_VALUE)
        return Error(STR_ERROPEN, Settings.SfxFile);

    if (!Read(SfxPackage, &sfxHead, sizeof(CSfxFileHeader)))
        return Error(STR_ERRREAD, Settings.SfxFile);
    if (sfxHead.Signature != SFX_SIGNATURE)
        return Error(STR_CORRUPTSFX, Settings.SfxFile);
    if (sfxHead.CompatibleVersion != SFX_SUPPORTEDVERSION)
        return Error(STR_BADSFXVER, Settings.SfxFile);
    if (sfxHead.HeaderCRC != UpdateCrc((__UINT8*)&sfxHead, sizeof(CSfxFileHeader) - sizeof(DWORD), INIT_CRC, CrcTab))
        return Error(STR_CORRUPTSFX, Settings.SfxFile);

    if (sfxHead.TotalTextSize > 0xFFFF)
        return Error(STR_LARGETEXT, Settings.SfxFile);
    if (!Read(SfxPackage, IOBuffer, sfxHead.TotalTextSize))
        return Error(STR_ERRACCESS, Settings.SfxFile);
    CloseHandle(SfxPackage);
    SfxPackage = INVALID_HANDLE_VALUE;

    SlideWin = (unsigned char*)malloc(WSIZE);
    if (!SlideWin)
        return Error(STR_LOWMEM);
    InPtr = (unsigned char*)IOBuffer;
    InEnd = InPtr + sfxHead.TotalTextSize;
    InflatingTexts = TRUE;

    switch (Inflate())
    {
    case 4:
    case 1:
    case 2:
        return Error(STR_CORRUPTSFX, Settings.SfxFile);
    case 3:
        return Error(STR_LOWMEM);
    case 5:
        return Error(STR_LARGETEXT, Settings.SfxFile);
    }
    if (Crc != sfxHead.TextsCRC)
        return Error(STR_CORRUPTSFX, Settings.SfxFile);

    char* ptr = (char*)SlideWin;
    int size = min(sfxHead.TextLen[TITLELEN], SE_MAX_TITLE - 1);
    memcpy(Settings.Title, ptr, size);
    Settings.Title[size] = 0;
    ptr += sfxHead.TextLen[TITLELEN];

    size = min(sfxHead.TextLen[TEXTLEN], SE_MAX_TEXT - 1);
    memcpy(Settings.Text, ptr, size);
    Settings.Text[size] = 0;
    ptr += sfxHead.TextLen[TEXTLEN];

    size = min(sfxHead.TextLen[ABOUTLICENCEDLEN], SE_MAX_ABOUT - 1);
    memcpy(About, ptr, size);
    About[size] = 0;
    lstrcpy(DefAbout, About); // pro pozdejsi pouziti
    ptr += sfxHead.TextLen[ABOUTLICENCEDLEN];

    size = min(sfxHead.TextLen[BUTTONTEXTLEN], SE_MAX_EXTRBTN - 1);
    memcpy(Settings.ExtractBtnText, ptr, size);
    Settings.ExtractBtnText[size] = 0;
    ptr += sfxHead.TextLen[BUTTONTEXTLEN];

    size = min(sfxHead.TextLen[VENDORLEN], SE_MAX_VENDOR - 1);
    memcpy(Settings.Vendor, ptr, size);
    Settings.Vendor[size] = 0;
    lstrcpy(DefVendor, Settings.Vendor); // pro pozdejsi pouziti
    ptr += sfxHead.TextLen[VENDORLEN];

    size = min(sfxHead.TextLen[WWWLEN], SE_MAX_WWW - 1);
    memcpy(Settings.WWW, ptr, size);
    Settings.WWW[size] = 0;
    lstrcpy(DefWWW, Settings.WWW); // pro pozdejsi pouziti
    ptr += sfxHead.TextLen[WWWLEN];

    GetModuleFileName(NULL, Settings.IconFile, MAX_PATH);
    Settings.IconIndex = -IDI_SFXICON;

    return TRUE;
}

BOOL main2()
{
    CrcTab = (__UINT32*)malloc(CRC_TAB_SIZE);
    if (!CrcTab)
        return Error(STR_LOWMEM);
    MakeCrcTable(CrcTab);

    GetEnvironmentVariable("SFX_DEFPACKAGE", Settings.SfxFile, MAX_PATH);
    SetLastError(ERROR_SUCCESS);

    if (Param)
    {
        switch (*Param)
        {
        case 'p':
            lstrcpy(Settings.SfxFile, Param + 1);
            break;
        case 's':
            if (!LoadSettings())
                return FALSE;
        }
    }

    if (!*Settings.SfxFile)
        return Error(STR_NODEFPACKAGE);
    if (!LoadDefaults())
        return FALSE;
    if (Param && *Param == 's')
    {
        char zip2sfxDir[MAX_PATH];
        GetZip2SfxDir(zip2sfxDir);

        ImportSFXSettings(SettingsTextData, &Settings, zip2sfxDir);
        if (lstrcmpi(Settings.Vendor, DefVendor) != 0 || lstrcmpi(Settings.WWW, DefWWW) != 0)
        {
            char buffer[SE_MAX_VENDOR + SE_MAX_WWW + 10 + SE_MAX_ABOUT];
            sprintf(buffer, "%s\r\n%s\r\n\r\n%s", DefVendor, DefWWW, DefAbout);
            lstrcpyn(About, buffer, SE_MAX_ABOUT);
        }
    }
    switch (LoadIcons(Settings.IconFile, Settings.IconIndex, &Icons, &IconsCount))
    {
    case 1:
        return Error(STR_ERROPENICO, Settings.IconFile);
    case 2:
        return Error(STR_ERRLOADLIB, Settings.IconFile);
    case 3:
        return Error(STR_ERRLOADLIB2, Settings.IconFile);
    case 4:
        return Error(STR_ERRLOADICON, Settings.IconFile);
    }

    ZipFile = CreateFile(ZipName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (ZipFile == INVALID_HANDLE_VALUE)
        return Error(STR_ERROPEN, ZipName);

    if (!CheckZip())
        return FALSE;

    if (!OvewriteExe && SalGetFileAttributes(ExeName) != 0xFFFFFFFF)
    {
        printf(StringTable[STR_OVERWRITE], ExeName);
        char c;
        while ((c = _getch()) != 'n' && c != 'y')
            ;
        printf("%c\n", c);
        if (c != 'y')
            return FALSE;
    }
    ExeFile = CreateFile(ExeName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (ExeFile == INVALID_HANDLE_VALUE)
        return Error(STR_ERRCREATE, ExeName);

    if (!WriteSfxExecutable())
        return FALSE;
    if (!AppendArchive())
        return FALSE;
    return TRUE;
}

// Chceme se dozvedet o SEH Exceptions i na x64 Windows 7 SP1 a dal
// http://blog.paulbetts.org/index.php/2010/07/20/the-case-of-the-disappearing-onload-exception-user-mode-callback-exceptions-in-x64/
// http://connect.microsoft.com/VisualStudio/feedback/details/550944/hardware-exceptions-on-x64-machines-are-silently-caught-in-wndproc-messages
// http://support.microsoft.com/kb/976038
void EnableExceptionsOn64()
{
    typedef BOOL(WINAPI * FSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
    typedef BOOL(WINAPI * FGetProcessUserModeExceptionPolicy)(LPDWORD dwFlags);
    typedef BOOL(WINAPI * FIsWow64Process)(HANDLE, PBOOL);
#define PROCESS_CALLBACK_FILTER_ENABLED 0x1

    HINSTANCE hDLL = LoadLibrary("KERNEL32.DLL");
    if (hDLL != NULL)
    {
        FIsWow64Process isWow64 = (FIsWow64Process)GetProcAddress(hDLL, "IsWow64Process");                                                      // Min: XP SP2
        FSetProcessUserModeExceptionPolicy set = (FSetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "SetProcessUserModeExceptionPolicy"); // Min: Vista with hotfix
        FGetProcessUserModeExceptionPolicy get = (FGetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "GetProcessUserModeExceptionPolicy"); // Min: Vista with hotfix
        if (isWow64 != NULL && set != NULL && get != NULL)
        {
            BOOL bIsWow64;
            if (isWow64(GetCurrentProcess(), &bIsWow64) && bIsWow64)
            {
                DWORD dwFlags;
                if (get(&dwFlags))
                    set(dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            }
        }
        FreeLibrary(hDLL);
    }
}

int main(int argc, char* argv[])
{
    EnableExceptionsOn64();

    printf(StringTable[STR_WELCOME_MESSAGE]);
    if (argc < 2 || argc > 5 || !ProcessCommandline(argc, argv))
    {
        printf(StringTable[STR_HELP]);
        return 1;
    }

    IOBuffer = (char*)malloc(0xFFFF);
    if (!IOBuffer)
        return Error(STR_LOWMEM);

    BOOL ret = main2();

    if (SettingsTextData)
        free(SettingsTextData);
    if (ZipFile != INVALID_HANDLE_VALUE)
        CloseHandle(ZipFile);
    if (SettingsFile != INVALID_HANDLE_VALUE)
        CloseHandle(SettingsFile);
    if (SfxPackage != INVALID_HANDLE_VALUE)
        CloseHandle(SfxPackage);
    if (ExeFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ExeFile);
        if (!ret)
            DeleteFile(ExeName);
    }
    if (CrcTab)
        free(CrcTab);
    if (SlideWin)
        free(SlideWin);
    if (Icons)
        DestroyIcons(Icons, IconsCount);

    if (ret)
    {
        printf(StringTable[STR_SUCCESS]);
        return 0;
    }

    return 1;
}
