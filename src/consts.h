// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// current configuration version (see mainwnd2.cpp for details)
extern const DWORD THIS_CONFIG_VERSION;

// Version expiration: uncomment for beta and PB builds, comment out for release builds:
//#define USE_BETA_EXPIRATION_DATE

// For PB (EAP) builds uncomment, for other builds comment out:
//#define THIS_IS_EAP_VERSION

#ifdef USE_BETA_EXPIRATION_DATE

// specifies the first day when this beta build will no longer run
extern SYSTEMTIME BETA_EXPIRATION_DATE;

#endif // USE_BETA_EXPIRATION_DATE

// DEBUG build only: allows debugging bug report creation (normally no report is built,
// the exception is simply passed to the MSVC debugger)
//#define ENABLE_BUGREPORT_DEBUGGING   1

// used to detect whether a wheel message came through the hook or directly
extern BOOL MouseWheelMSGThroughHook; // TRUE: the message went through the hook at the time stored in MouseWheelMSGTime; FALSE: the message went through the panel at the time stored in MouseWheelMSGTime
extern DWORD MouseWheelMSGTime;       // timestamp of the last wheel message
#define MOUSEWHEELMSG_VALID 100       // [ms] number of milliseconds for which one channel (hook vs. window) remains valid

enum
{
    otViewerWindow = 10,
};

// horizontal scroll support (works on W2K/XP with Intellipoint drivers, officially supported since Vista)
#define WM_MOUSEHWHEEL 0x020E
BOOL PostMouseWheelMessage(MSG* pMSG);

// checks if it is very likely (though not guaranteed) that Salamander will not be "busy" in the next
// few moments (no modal dialog open and no message being processed). Returns TRUE in that case, otherwise FALSE.
// If 'lastIdleTime' is not NULL, it receives GetTickCount() from the last idle->busy transition.
// Can be called from any thread.
BOOL SalamanderIsNotBusy(DWORD* lastIdleTime);

// Opens Salamander or plugin HTML Help. The help language (the directory with .chm files) is selected as follows:
// - directory from the current Salamander .slg file (see SLGHelpDir in shared\versinfo.rc)
// - HELP\ENGLISH\*.chm
// - first subdirectory found in HELP
// 'helpFileName' is the .chm file name to use (without a path); if NULL, "salamand.chm" is used.
// 'parent' is the parent window for the error message box; 'command' is the HTML Help command,
// see HHCDisplayXXX; 'dwData' is the HTML Help command parameter, see HHCDisplayXXX
// Can be called from any thread.
// If 'quiet' is TRUE, no error message is shown.
// Returns TRUE if the help was opened successfully; otherwise returns FALSE.
BOOL OpenHtmlHelp(char* helpFileName, HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet);

extern CRITICAL_SECTION OpenHtmlHelpCS; // critical section used by OpenHtmlHelp()

/* simple way to run inside a critical section, example usage:
  static CCriticalSection cs;
  CEnterCriticalSection enterCS(cs);
*/

class CCriticalSection
{
public:
    CRITICAL_SECTION cs;

    CCriticalSection() { InitializeCriticalSection(&cs); }
    ~CCriticalSection() { DeleteCriticalSection(&cs); }

    void Enter() { EnterCriticalSection(&cs); }
    void Leave() { LeaveCriticalSection(&cs); }
};

class CEnterCriticalSection
{
protected:
    CCriticalSection* CS;

public:
    CEnterCriticalSection(CCriticalSection& cs)
    {
        CS = &cs;
        CS->Enter();
    }

    ~CEnterCriticalSection()
    {
        CS->Leave();
    }
};

// Because Windows GetTempFileName does not work correctly, this function is our own clone:
// creates a file/directory (depending on 'file') at 'path' (NULL -> Windows TEMP dir),
// with prefix 'prefix', returns the created name in 'tmpName' (minimum buffer size MAX_PATH),
// and returns success status; on failure, SetLastError contains the Windows error code for compatibility.
BOOL SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file);

// Windows MoveFile cannot rename a file with the read-only attribute on Novell volumes,
// so we have our own version (on failure we drop the read-only flag, perform the operation,
// then restore it)
BOOL SalMoveFile(const char* srcName, const char* destName);

// replacement for Windows GetFileSize with simpler error handling; 'file' is an open
// handle for GetFileSize(); the result is stored in 'size'. Returns success, otherwise
// 'err' receives the Windows error code and 'size' is zero
BOOL SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err);
BOOL SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err); // 'err' may be NULL if the error code is not needed

struct COperation;

// Determines the size of the file pointed to by the symlink 'fileName'. If 'op' is not
// NULL, the contents of 'op' are released on Cancel. If 'fileName' is NULL,
// 'op->SourceName' is used. The size is returned in 'size'. 'ignoreAll' is both input
// and output; if it is TRUE, all errors are ignored. Set it to FALSE before the
// operation, otherwise the error dialog is not shown at all, and do not change it
// afterwards. On error, the standard Retry / Ignore / Ignore All / Cancel dialog with
// parent 'parent' is shown. Returns TRUE if the size is determined successfully. On
// error, if Ignore or Ignore All is chosen, returns FALSE and stores FALSE in 'cancel'.
// If 'ignoreAll' is TRUE, the dialog is not shown and the function behaves as if the
// user had pressed Ignore. On error, if Cancel is chosen, returns FALSE and stores TRUE
// in 'cancel'.
BOOL GetLinkTgtFileSize(HWND parent, const char* fileName, COperation* op, CQuadWord* size,
                        BOOL* cancel, BOOL* ignoreAll);

// Windows GetFileAttributes cannot handle names ending with a space or dot, so we created
// our own version. For such names it appends a backslash which fixes GetFileAttributes for directories;
// files with trailing spaces/dots remain problematic, but at least we avoid reading attributes of another file
// because the Windows version trims spaces/dots and operates on a different name.
DWORD SalGetFileAttributes(const char* fileName);

// If the file or directory 'name' has the read-only attribute we try to clear it
// (so DeleteFile can succeed). If the attributes have already been read we pass them in 'attr';
// when 'attr' is -1 the attributes are read from disk. Returns TRUE if the change was attempted
// (the success is not checked). NOTE: only the read-only attribute is cleared to avoid unnecessary
// attribute changes on other hard links which all share the same attributes.
BOOL ClearReadOnlyAttr(const char* name, DWORD attr = -1);

// Deletes a directory link (junction point, symbolic link, mount point). Returns TRUE on success;
// on failure returns FALSE and if 'err' is not NULL, the error code is stored there.
BOOL DeleteDirLink(const char* name, DWORD* err);

// Returns TRUE if 'path' resides on a NOVELL volume (used to detect whether fast-directory-move can be used)
BOOL IsNOVELLDrive(const char* path);

// Returns TRUE if 'path' is located on a LANTASTIC volume (used to decide
// whether the file size must be verified after copying). For performance the
// parameters 'lastLantasticCheckRoot' (empty for the first call, then unchanged)
// and 'lastIsLantasticPath' (result for 'lastLantasticCheckRoot') are used
BOOL IsLantasticDrive(const char* path, char* lastLantasticCheckRoot, BOOL& lastIsLantasticPath);

// returns TRUE for network paths
BOOL IsNetworkPath(const char* path);

// returns TRUE if 'path' lies on a volume supporting ADS (or an error occurred while
// detecting the file system) and we are on NT/W2K/XP; if 'isFAT32' is not NULL,
// it receives TRUE if 'path' points to a FAT32 volume; returns FALSE only when it is
// certain the FS does not support ADS
BOOL IsPathOnVolumeSupADS(const char* path, BOOL* isFAT32);

// test whether this is a Samba share (Linux sharing with Windows)
BOOL IsSambaDrivePath(const char* path);

// test whether the path is UNC (detects both \\server\share and \\?\UNC\server\share formats)
BOOL IsUNCPath(const char* path);

// test whether the path is UNC root (only detects the \\server\share format)
BOOL IsUNCRootPath(const char* path);

// creates a file named 'fileName' using the standard Win32 API CreateFile
// (lpSecurityAttributes==NULL, dwCreationDisposition==CREATE_NEW, hTemplateFile==NULL).
// This method handles collisions between 'fileName' and an existing DOS name
// (only when it is not also a long-name collision) by temporarily renaming the
// conflicting file/directory and renaming it back after creating 'fileName'.
// Returns a file handle or INVALID_HANDLE_VALUE on error (GetLastError()).
// If 'encryptionNotSupported' is not NULL and the file cannot be opened with the
// Encrypted attribute, it tries again without encryption support,
// the file is deleted and 'encryptionNotSupported' is set to TRUE - the function's
// return value and GetLastError() contain the original error from opening with the Encrypted attribute
HANDLE SalCreateFileEx(const char* fileName, DWORD desiredAccess,
                       DWORD shareMode, DWORD flagsAndAttributes,
                       BOOL* encryptionNotSupported);

// Checks the last component of the name in 'path'. If it starts or ends with a
// space or ends with a dot the function returns TRUE; otherwise FALSE
BOOL FileNameInvalidForManualCreate(const char* path);

// Trims spaces from the beginning and end of the path (CutWS or StripWS or CutWhiteSpace or StripWhiteSpace)
// Returns TRUE if any trimming occurred
BOOL CutSpacesFromBothSides(char* path);

// Trims leading spaces and trailing spaces or dots in the same way Explorer does
// because users requested this behavior, see https://forum.altap.cz/viewtopic.php?f=16&t=5891
// and https://forum.altap.cz/viewtopic.php?f=2&t=4210. Returns TRUE if 'path' was modified
BOOL MakeValidFileName(char* path);

// If 'name' ends with a space or dot a copy of 'name' is made to 'nameCopy' and
// a '\\' is appended. 'name' is then redirected to 'nameCopy'. Standard API
// functions silently trim trailing spaces or dots and operate on other
// files/directories than intended; adding '\\' at the end fixes that
void MakeCopyWithBackslashIfNeeded(const char*& name, char (&nameCopy)[3 * MAX_PATH]);

// Returns TRUE if the name ends with a backslash (the added '\\' fixes invalid names)
BOOL NameEndsWithBackslash(const char* name);

// Returns TRUE if 'name' ends with a space/dot or contains ':' (ADS conflict), otherwise FALSE.
// When 'ignInvalidName' is TRUE, returns TRUE only if 'name' contains ':' (ADS conflict)
BOOL FileNameIsInvalid(const char* name, BOOL isFullName, BOOL ignInvalidName = FALSE);

// Returns FALSE if any component of the path ends with a space or dot. When
// 'cutPath' is TRUE the path is shortened to the first invalid component (for an
// error message). Otherwise returns TRUE
BOOL PathContainsValidComponents(char* path, BOOL cutPath);

// creates a directory called 'name' using the standard CreateDirectory API
// (lpSecurityAttributes==NULL). The function resolves collisions with an
// existing DOS name (provided the long name itself does not collide) by
// temporarily renaming the conflicting item so the directory can be created and
// then restoring the original name. It also supports names ending with spaces,
// which CreateDirectory would otherwise trim. Returns TRUE on success or FALSE
// on failure. If 'err' is not NULL it receives the Windows error code.
BOOL SalCreateDirectoryEx(const char* name, DWORD* err);

void InitLocales();                                       // must be called before NumberToStr and PrintDiskSize
char* NumberToStr(char* buffer, const CQuadWord& number); // converts integer to a more readable string, !char buffer[50]!
int NumberToStr2(char* buffer, const CQuadWord& number);  // converts an integer to a more readable string, !char buffer[50]!, returns the number of characters written to the buffer
char* GetErrorText(DWORD error);                          // converts error code to a string
WCHAR* GetErrorTextW(DWORD error);                        // converts error code to a wide string
BOOL IsDirError(DWORD err);                               // does the error relate to directories?

// regular and UNC paths: do they share the same root?
BOOL HasTheSameRootPath(const char* path1, const char* path2);

// checks whether both paths have the same root and lie on the same volume
// (handles paths containing reparse points and SUBST drives)
// WARNING: this function can be quite slow (up to 200 ms)
BOOL HasTheSameRootPathAndVolume(const char* p1, const char* p2);

// returns TRUE if 'path1' and 'path2' are on the same volume; if 'resIsOnlyEstimation'
// is not NULL it is set to TRUE when the result is only an estimation (certain only
// when the "volume name" GUID could be obtained for both paths, which is possible
// only for local paths under Windows 2000 or newer)
// WARNING: this function can be quite slow (up to 200 ms)
BOOL PathsAreOnTheSameVolume(const char* path1, const char* path2, BOOL* resIsOnlyEstimation);

// compares two paths: case-insensitive and ignoring a single backslash at the start and end
BOOL IsTheSamePath(const char* path1, const char* path2);

// Determines whether 'path' is a plugin FS path; 'path' is the path to check,
// 'fsName' is a MAX_PATH buffer for the FS name (or NULL); returns in 'userPart'
// (if not NULL) a pointer into 'path' to the first character of the plugin-defined path (after the first ':')
BOOL IsPluginFSPath(const char* path, char* fsName = NULL, const char** userPart = NULL);
BOOL IsPluginFSPath(char* path, char* fsName = NULL, char** userPart = NULL);

// test whether the path is a URL, e.g. "file:///c|/WINDOWS/clock.avi" becomes "c:\\WINDOWS\\clock.avi"
BOOL IsFileURLPath(const char* path);

// determines from a file extension whether it is a shortcut (.lnk, .pif or .url);
// returns 1 for shortcuts or 0 otherwise
int IsFileLink(const char* fileExtension);

// obtains the UNC or drive root from 'path'; 'root' receives it in the form
// "C:\\" or "\\SERVER\\SHARE\\". The function returns the number of characters
// in the root (without the null terminator). If the UNC root is longer it is
// truncated to MAX_PATH-2 characters and a trailing backslash is added because
// such a string cannot be a real root
int GetRootPath(char* root, const char* path);

// returns a pointer just after the root (specifically to the backslash) of a
// UNC or standard path
const char* SkipRoot(const char* path);

// returns TRUE if `path` (UNC or regular path) can be shortened by removing the last
// directory (cut at the last backslash; the shortened path retains a trailing backslash
// only for paths like "c:\"), `cutDir` returns a pointer to the last directory
// (the removed part)
// replacement for PathRemoveFileSpec
BOOL CutDirectory(char* path, char** cutDir = NULL);

// concatenates 'name' to 'path' ensuring a single backslash separator. 'path'
// must hold at least 'pathSize' characters. Returns TRUE if 'name' fits; if
// either parameter is empty no separator is added (e.g. "c:\" + "" -> "c:")
BOOL SalPathAppend(char* path, const char* name, int pathSize);

// if 'path' does not already end with a backslash, appends one to the end of 'path'; 'path' is a buffer of at least 'pathSize'
// characters; returns TRUE if the backslash fits after 'path'; if 'path' is empty, no backslash is added
BOOL SalPathAddBackslash(char* path, int pathSize);

// removes a trailing backslash from 'path' if present
void SalPathRemoveBackslash(char* path);

// converts all '/' characters to '\\' and collapses repeated '\\' to a single one
// except for the two leading characters that denote a UNC path
void SlashesToBackslashesAndRemoveDups(char* path);

// extracts the file name from a full path ("c:\\path\\file" -> "file")
void SalPathStripPath(char* path);

// removes the file extension if one is present
void SalPathRemoveExtension(char* path);

// if 'path' has no extension yet, append 'extension' (for example ".txt").
// 'path' must hold at least 'pathSize' characters; returns FALSE when the buffer
// is not large enough for the resulting string
BOOL SalPathAddExtension(char* path, const char* extension, int pathSize);

// changes or adds the extension 'extension' (for example ".txt") in 'path'.
// 'path' must hold at least 'pathSize' characters; returns FALSE if the buffer
// is not large enough for the resulting string
BOOL SalPathRenameExtension(char* path, const char* extension, int pathSize);

// returns a pointer into 'path' pointing to the file or directory name. A
// trailing backslash is ignored. If there are no other backslashes the function
// simply returns 'path'
const char* SalPathFindFileName(const char* path);

// Works for both regular and UNC paths.
// Returns the number of characters common to both paths. For a regular path the
// root must end with a backslash, otherwise the function returns 0. Examples:
// "C:\"+"C:"->0, "C:\A\B"+"C:\"->3, "C:\A\B\"+"C:\A"->4,
// "C:\AA\BB"+"C:\AA\CC"->5
int CommonPrefixLength(const char* path1, const char* path2);

// Returns TRUE if the path 'prefix' is a prefix of the path 'path'. Otherwise returns FALSE.
// "C:\aa","C:\Aa\BB"->TRUE
// "C:\aa","C:\aaa"->FALSE
// "C:\aa\","C:\Aa"->TRUE
// "\\server\share","\\server\share\aaa"->TRUE
// Works with both regular and UNC paths.
BOOL SalPathIsPrefix(const char* prefix, const char* path);

// Removes ".." (together with one directory to the left) and "." (only the "." itself)
// from a path; backslashes must be used as directory separators; 'afterRoot' points past the
// root of the processed path (the path is modified only after 'afterRoot'); returns TRUE if the
// adjustments succeed, or FALSE if ".." cannot be removed because the root has already been reached
BOOL SalRemovePointsFromPath(char* afterRoot);
BOOL SalRemovePointsFromPath(WCHAR* afterRoot);

// Converts a relative or absolute path to an absolute path without '.', '..', or a trailing
// backslash (except for "X:\"); if 'curDir' is NULL, relative paths such as "\\path" and "path"
// return an error (they cannot be resolved); otherwise, 'curDir' must be a valid normalized
// current path (UNC or regular); current paths of other drives (except 'curDir'; regular only,
// not UNC) are in DefaultDir (it is advisable to call CMainWindow::UpdateDefaultDir before use);
// 'name' is an in/out buffer of at least MAX_PATH characters (its size is in 'nameBufSize');
// if 'nextFocus' is not NULL and the specified relative path contains no backslash,
// strcpy(nextFocus, name) is called
// Returns TRUE: the name in 'name' is ready for use; otherwise, if 'errTextID' is not NULL, it
// contains the error (constants for LoadStr: IDS_SERVERNAMEMISSING, IDS_SHARENAMEMISSING,
// IDS_TOOLONGPATH, IDS_INVALIDDRIVE, IDS_INCOMLETEFILENAME, IDS_EMPTYNAMENOTALLOWED, and
// IDS_PATHISINVALID); TRUE is returned in 'callNethood' (if not NULL) if the Nethood plugin should
// be called for IDS_SERVERNAMEMISSING and IDS_SHARENAMEMISSING; if 'allowRelPathWithSpaces' is
// TRUE, leading spaces are not trimmed from a relative path (normally they are, to prevent users
// from accidentally creating names with leading spaces; Windows trims trailing spaces and dots)
// Returns TRUE if the path contains no error, otherwise FALSE (e.g. "\\\" or "\\server\\")
BOOL SalGetFullName(char* name, int* errTextID = NULL, const char* curDir = NULL,
                    char* nextFocus = NULL, BOOL* callNethood = NULL, int nameBufSize = MAX_PATH,
                    BOOL allowRelPathWithSpaces = FALSE);

// tries to access the 'path' (regular or UNC) in a worker thread so
// the attempt can be interrupted with the ESC key (after a while a
// message about the pressed ESC is shown)
// When 'echo' is TRUE an error message is displayed if the path is not
// accessible. If 'err' differs from ERROR_SUCCESS together with 'echo' TRUE, the
// error is shown and no additional check is performed. 'postRefresh' is passed
// to EndStopRefresh (normally TRUE). 'parent' is the parent message box. The
// function returns ERROR_SUCCESS when the path is valid or a standard Windows
// error code (or ERROR_USER_TERMINATED when ESC was pressed) otherwise.
DWORD SalCheckPath(BOOL echo, const char* path, DWORD err, BOOL postRefresh, HWND parent);

// Tries to access 'path' and optionally restores network connections via
// CheckAndRestoreNetworkConnection and CheckAndConnectUNCNetworkPath.
// Returns TRUE when the path is accessible. 'parent' is the parent message box;
// 'tryNet' specifies whether attempting to restore the connection makes sense.
BOOL SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet);

// Checks if 'path' is accessible and optionally shortens it. When 'tryNet' is
// TRUE, network connections can be restored using
// CheckAndRestoreNetworkConnection and CheckAndConnectUNCNetworkPath (unless
// 'donotReconnect' is TRUE, in which case the error is only reported). The
// function updates 'tryNet' to FALSE and returns 'err' (current path error),
// 'lastErr' (error that caused path shortening), 'pathInvalid' (TRUE when a
// network reconnection failed), and 'cut' (TRUE when the final path is shorter).
// 'parent' is the parent window. Returns TRUE if the resulting path is
// accessible.
BOOL SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err, DWORD& lastErr,
                                   BOOL& pathInvalid, BOOL& cut, BOOL donotReconnect);

// Detects the path type (FS/Windows/archive) and splits it into parts.
// For FS paths, this means the file-system name and user part; for archive
// paths, the path to the archive file and the path inside the archive; for
// Windows paths, the existing part and the remainder of the path. FS paths are
// not checked; Windows paths (normal and UNC) are checked for how far the path
// exists, and the network connection is restored if needed. For archive paths,
// the existence of the archive file is checked (archives are distinguished by
// extension). This function uses SalGetFullName, so it is advisable to call
// CMainWindow::UpdateDefaultDir first.
// 'path' is a full or relative path (buffer of at least 'pathBufSize' chars; for
// relative paths, the current path 'curPath' is used as the base for resolving
// the full path, if it is not NULL; 'curPathIsDiskOrArchive' is TRUE if
// 'curPath' is a Windows or archive path; if the current path is an archive
// path, 'curArchivePath' contains the archive file name, otherwise it is NULL).
// The resulting full path is stored in 'path' (which must be at least
// 'pathBufSize' chars long). Returns TRUE on successful recognition; then
// 'type' is the path type (see PATH_TYPE_XXX) and 'secondPart' is set to:
// - in 'path', at the position after the existing path (after '\\' or at the
//   end of the string; if the path contains a file, it points past the path to
//   that file) (Windows path type); WARNING: the length of the returned part of
//   the path is not handled (the whole path may be longer than MAX_PATH)
// - after the archive file (archive path type); WARNING: the length of the path
//   inside the archive is not handled (it may be longer than MAX_PATH)
// - after ':' following the file-system name, i.e. to the user part of the
//   file-system path (FS path type); WARNING: the length of the user part is
//   not handled (it may be longer than MAX_PATH);
// if TRUE is returned, 'isDir' is also set to:
// - TRUE if the existing part of the path is a directory, FALSE if it is a file
//   (Windows path type)
// - FALSE for archive and FS paths;
// if FALSE is returned, an error that occurred during recognition has already
// been shown to the user (with one exception; see the description of
// SPP_INCOMLETEPATH); if 'error' is not NULL, one of the SPP_XXX constants is
// returned in it. 'errorTitle' is the title of the error message box; if
// 'nextFocus' != NULL and the Windows/archive path does not contain '\\' or
// only ends with '\\', the path is copied to 'nextFocus' (see
// SalGetFullName).
BOOL SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                  const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                  const char* curPath, const char* curArchivePath, int* error,
                  int pathBufSize);

// obtains the existing part and operation mask from a Windows target path; allows any non-existent part
// to be created; on success returns TRUE, the existing Windows target path (in 'path')
// and the found operation mask (in 'mask' - it points into the 'path' buffer, but the path and mask are separated
// by a null character; if the path contains no mask, it automatically creates the mask "*.*"); 'parent' is the parent of any
// message boxes; 'title' + 'errorTitle' are the captions of the information + error message boxes; 'selCount' is
// the number of selected files and directories; 'path' is the input target path to process, on output
// (at least 2 * MAX_PATH characters) the existing target path; 'secondPart' points into 'path' to the position
// after the existing path (after '\\' or at the end of the string; if the path contains a file, it points after the path
// to that file); 'pathIsDir' is TRUE/FALSE if the existing part of the path is a directory/file;
// 'backslashAtEnd' is TRUE if there was a backslash at the end of 'path' before "parse" was performed (for example,
// SalParsePath removes such a backslash); 'dirName' + 'curDiskPath' are not NULL if at most one file/directory is selected
// (its name without the path is in 'dirName'; if nothing is selected, the focused item is used)
// and the current path is a Windows path (the path is in 'curDiskPath'); 'mask' is on output
// a pointer to the operation mask in the 'path' buffer; if the path contains an error, the method returns FALSE,
// the problem has already been reported to the user
BOOL SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                         char* path, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                         const char* dirName, const char* curDiskPath, char*& mask);

// Retrieves the existing part of the target path and the operation mask; also recognizes any non-existent part. On
// success, returns TRUE, the relative path to create (in 'newDirs'), the existing target path (in 'path'; valid only
// if the relative path 'newDirs' is created), and the found operation mask (in 'mask', which points into the 'path'
// buffer, with the path and mask separated by a zero; if the path contains no mask, the mask "*.*" is created
// automatically). 'parent' is the parent of any message boxes; 'title' and 'errorTitle' are the captions of the
// information and error message boxes; 'selCount' is the number of selected files and directories; 'path' is the
// target path to process on input and, on output (at least 2 * MAX_PATH characters), the existing target path (always
// ending with a backslash); 'afterRoot' points into 'path' past the path root (past '\\' or to the end of the
// string); 'secondPart' points into 'path' to the position after the existing path (past '\\' or to the end of the
// string; if the path contains a file, it points past the path to that file); 'pathIsDir' is TRUE/FALSE if the
// existing part of the path is a directory/file; 'backslashAtEnd' is TRUE if 'path' ended with a backslash before
// parsing (for example, SalParsePath removes such a backslash); 'dirName' and 'curPath' are non-NULL if at most one
// file/directory is selected (its name without the path is in 'dirName'; its path is in 'curPath'; if nothing is
// selected, the focused item is used); 'mask' receives a pointer to the operation mask in the 'path' buffer; if
// 'newDirs' is not NULL, it is a buffer (of size at least MAX_PATH) for the relative path (with respect to the
// existing path in 'path') that must be created (the user agrees to create it; the same prompt is used as when copying
// from disk to disk; an empty string means create nothing); if 'newDirs' is NULL and some relative path needs to be
// created, only an error is reported; 'isTheSamePathF' is a function for comparing two paths (needed only if 'curPath'
// is not NULL); if it is NULL, IsTheSamePath is used; if the path contains an error, the method returns FALSE and the
// problem has already been reported to the user
BOOL SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                         char* path, char* afterRoot, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                         const char* dirName, const char* curPath, char*& mask, char* newDirs,
                         SGP_IsTheSamePathF isTheSamePathF);

// tests whether the string 'fileNameComponent' can be used as a name component
// on a Windows filesystem (handles strings longer than MAX_PATH-4 (4 = "C:\\"
// + null terminator), an empty string, strings of '.' characters, strings of white-space,
// the characters "*?\\/<>|\":" and simple names such as "prn" and "prn  .txt")
BOOL SalIsValidFileNameComponent(const char* fileNameComponent);

// transforms 'fileNameComponent' so it can be used as a Windows file name
// component (handles strings longer than MAX_PATH-4 (4 = "C:\\" + null terminator),
// empty strings, strings of '.', whitespace-only strings; characters "*?\\/<>|\":"
// are replaced with '_'; simple names like "prn" and "prn  .txt" get an '_'
// appended at the end of the name); 'fileNameComponent' must be extendable by at
// least one character (but at most MAX_PATH bytes of 'fileNameComponent' are used)
void SalMakeValidFileNameComponent(char* fileNameComponent);

// prints disk space size; mode==0 "1.23 MB", mode==1 "1 230 000 bytes, 1.23 MB",
// mode==2 "1 230 000 bytes", mode==3 (always whole KBs), mode==4 (like mode==0 but
// always at least 3 significant digits, e.g. "2.00 MB")
char* PrintDiskSize(char* buf, const CQuadWord& size, int mode);

// converts the number of seconds to a string ("5 sec", "1 hr 34 min", etc.);
// 'buf' is the output buffer (at least 100 chars); 'secs' is the number of seconds.
// returns 'buf'
char* PrintTimeLeft(char* buf, CQuadWord const& secs);

// duplicates '&' - useful for paths shown in menus ('&&' displays as '&');
// 'buffer' is an in/out string, 'bufferSize' its size in bytes.
// returns TRUE if doubling did not truncate the string (buffer was large enough)
BOOL DuplicateAmpersands(char* buffer, int bufferSize, BOOL skipFirstAmpersand = FALSE);

// Removes '&' characters. Useful for menu commands that should be displayed
// without hot-key markers; if a double "&&" is found it is replaced with a
// single '&'. 'text' is an input/output string.
void RemoveAmpersands(char* text);

// Duplicates '\\'. Useful for strings passed to LookForSubTexts, which reduces
// "\\\\" back to "\\". 'buffer' is both input and output; 'bufferSize' is its
// size in bytes. Returns TRUE if doubling did not truncate the string (buffer
// was large enough).
BOOL DuplicateBackslashes(char* buffer, int bufferSize);

// Duplicates '$'. Used when importing old paths (HotPaths), which may
// contain $(SalDir) and now support Sal/Env variables such as $(SalDir) or
// $(WinDir). In 2.5RC1 this expansion was not done for editors, viewers, or
// archivers; this conversion is added only for HotPaths.
// 'buffer' is the input/output string, 'bufferSize' is the size of 'buffer' in
// bytes. Returns TRUE if doubling did not truncate the string (the buffer was
// large enough).
BOOL DuplicateDollars(char* buffer, int bufferSize);

// Searches 'buf' for a name (trimming spaces at both ends). If the name exists,
// is not quoted and contains at least one space, it is wrapped in quotes. The
// function returns FALSE when there is not enough room to add the quotes
// ('bufSize' specifies the buffer size).
BOOL AddDoubleQuotesIfNeeded(char* buf, int bufSize);

// trims '"' at the beginning and end of 'path' (CutDoubleQuotes or StripDoubleQuotes or
// CutQuotes or StripQuotes). Returns TRUE if trimming occurred
BOOL CutDoubleQuotesFromBothSides(char* path);

// wait up to 1/5 second for ESC to be released so that the dialog does not
// immediately abort operations such as reading a panel listing
void WaitForESCRelease();

// checks whether the root parent of 'parent' is the foreground window; if not
// FlashWindow(root-parent, TRUE) is called and root-parent is returned,
// otherwise NULL is returned
HWND GetWndToFlash(HWND parent);

// walks through all windows of thread 'tid' (0 means current) with
// EnumThreadWindows and posts WM_CLOSE to every enabled visible dialog
// (class name "#32770") owned by 'parent'. Used during critical shutdown to
// unblock windows when multiple layers of modal dialogs are open; call
// repeatedly if more layers appear
void CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid = 0);

// returns a displayable form of file or directory attributes; 'text' must be a
// buffer of at least 10 characters; 'attrs' are the attributes to format
void GetAttrsString(char* text, DWORD attrs);

// Copies 'srcStr' after the terminating zero of 'dstStr'.
// 'dstStr' is a buffer of size 'dstBufSize' (must be at least 2).
// If both strings do not fit, they are truncated so that as many characters as
// possible from both strings are kept.
void AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr);

// Creates and returns an allocated full file name. If 'dosName' is not NULL and
// 'path'+'name' is too long, it tries 'path'+'dosName'. If 'skip', 'skipAll',
// and 'sourcePath' are not NULL and a "name too long" error occurs, the user
// can skip this name (the function then returns NULL and sets 'skip' to TRUE).
// If the user selects "Skip All", 'skipAll' is set to TRUE. 'sourcePath' is
// used for the Focus button (the panel shows the too-long component in the
// source path that would cause the problem in the target path).
char* BuildName(char* path, char* name, char* dosName = NULL, BOOL* skip = NULL, BOOL* skipAll = NULL,
                const char* sourcePath = NULL);

// retrieves the date and time for file/directory 'f' from the panel (also handles
// values supplied by plugins-they may be invalid)
void GetFileDateAndTimeFromPanel(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData,
                                 const CFileData* f, BOOL isDir, SYSTEMTIME* st, BOOL* validDate,
                                 BOOL* validTime);

// retrieves the size for file/directory 'f' from the panel (also handles sizes
// supplied by plugins-they may be invalid)
void GetFileSizeFromPanel(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData,
                          const CFileData* f, BOOL isDir, CQuadWord* size, BOOL* validSize);

void DrawSplitLine(HWND HWindow, int newDragSplitX, int oldDragSplitX, RECT client);
BOOL InitializeCheckThread(); // initializes the thread used by CFilesWindow::CheckPath()
void ReleaseCheckThreads();   // releases the thread used by CFilesWindow::CheckPath()
void InitDefaultDir();        // initializes the DefaultDir array (last visited paths for all drives)

// shows or hides a message in its own thread without draining the message
// queue; only one message can be displayed at a time. Repeated calls report
// an error to TRACE (non-fatal). 'delay' is the delay before the window is
// opened (counted from the call to CreateSafeWaitWindow).
// 'message' may span multiple lines; individual lines are separated by '\n'.
// 'caption' may be NULL; in that case, the caption "Open Salamander" is used.
// 'showCloseButton' specifies whether the window contains a Close button.
// 'hForegroundWnd' specifies the window that must be active for the wait window
// to be shown and the window that will be activated when the wait window is clicked.
void CreateSafeWaitWindow(const char* message, const char* caption, int delay,
                          BOOL showCloseButton, HWND hForegroundWnd);
void DestroySafeWaitWindow(BOOL killThread = FALSE);
// hides the created window when 'show'==FALSE and shows it when 'show'==TRUE.
// Call in response to WM_ACTIVATE from the hForegroundWnd window:
//    case WM_ACTIVATE:
//    {
//      ShowSafeWaitWindow(LOWORD(wParam) != WA_INACTIVE);
//      break;
//    }
// If the thread that created the window is busy, messages are not dispatched,
// so WM_ACTIVATE is not delivered when the user clicks another application. The
// messages are delivered only when the message box is shown, which is what we
// need: hide temporarily and show again later (after the message box is closed
// and the hForegroundWnd window is activated).
void ShowSafeWaitWindow(BOOL show);
// after calling CreateSafeWaitWindow or ShowSafeWaitWindow the function returns
// FALSE until the user clicks the Close button (if shown); then it returns TRUE
BOOL GetSafeWaitWindowClosePressed();
// returns TRUE if the user is pressing ESC or clicked the Close button
BOOL UserWantsToCancelSafeWaitWindow();
// Used to change the message text later. NOTE: the window layout is not
// recomputed; if the text grows it will be truncated. Useful for countdowns
// like 60s, 55s, 50s, ...
void SetSafeWaitWindowText(const char* message);

// returns TRUE when Salamander is active (foreground window PID == current PID)
BOOL SalamanderActive();

// removes a directory along with its contents (SHFileOperation is terribly slow)
void RemoveTemporaryDir(const char* dir);

// helper for adding a name to a space-separated list; returns success
BOOL AddToListOfNames(char** list, char* listEnd, const char* name, int nameLen);

// if the directory does not exist, the user can create it;
// returns TRUE if the directory exists or is created successfully
// 'parent' is the parent window for error message boxes; NULL = Salamander's main window
// quiet = TRUE - do not ask whether to create it, but note that errors are shown if errBuf == NULL
// if errBuf != NULL, errBufSize is the size of the error-description buffer
// if newDir != NULL, the first created subdirectory (full path) is returned in 'newDir'
// if the full path already exists, newDir==""; newDir points to a buffer of size MAX_PATH
// noRetryButton = TRUE - error dialogs contain only an OK button, not Retry/Cancel
// manualCrDir = TRUE - do not allow creating a directory with a leading space (during manual
// directory creation; otherwise Windows allows leading spaces)
BOOL CheckAndCreateDirectory(const char* dir, HWND parent = NULL, BOOL quiet = FALSE, char* errBuf = NULL,
                             int errBufSize = 0, char* newDir = NULL, BOOL noRetryButton = FALSE,
                             BOOL manualCrDir = FALSE);

// removes empty subdirectories under 'dir' and deletes 'dir' itself when it becomes empty
void RemoveEmptyDirs(const char* dir);

// routine used to open the viewer-used in CFilesWindow::ViewFile and
// CSalamanderForViewFileOnFS::OpenViewer; no other use is expected, therefore
// parameters and return values are not documented
BOOL ViewFileInt(HWND parent, const char* name, BOOL altView, DWORD handlerID, BOOL returnLock,
                 HANDLE& lock, BOOL& lockOwner, BOOL addToHistory, int enumFileNamesSourceUID,
                 int enumFileNamesLastFileIndex);

// converts the string ('str' of length 'len') to unsigned __int64 (a leading
// '+' sign is allowed; leading and trailing white-space is ignored);
// if 'isNum' is not NULL it receives TRUE when the entire string represents
// a number
unsigned __int64 StrToUInt64(const char* str, int len, BOOL* isNum = NULL);

// handles the "in-page-error" and "access violation - read/write on XXX" exceptions
// (checks whether the exception is related to the file: 'fileMem' is the base address,
// 'fileMemSize' is the size of the current mapped file view); used when mapping files
// into memory (a read/write error raises one of these exceptions)
int HandleFileException(EXCEPTION_POINTERS* e, char* fileMem, DWORD fileMemSize);

struct CSalamanderVarStrEntry;

// ValidateVarString and ExpandVarString:
// methods for validating and expanding strings with variables like "$(var_name)",
// "$(var_name:num)" (num is the field width, numeric 1..9999), "$(var_name:max)"
// ("max" means the width follows the value in 'maxVarWidths'; details with
// ExpandVarString) and "$[env_var]" for environment variables.
// Used where the user can define a format string (e.g. in the info line). Example
// string with variables: "$(files) files and $(dirs) directories" where
// the variables are 'files' and 'dirs'.

// checks the syntax of 'varText' (string with variables); returns FALSE on error and
// provides the error position in 'errorPos1' (start offset) and 'errorPos2' (end offset).
// 'variables' is an array of CSalamanderVarStrEntry terminated with Name==NULL.
// 'msgParent' is the parent of error message boxes; if NULL, errors are not displayed
BOOL ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                       const CSalamanderVarStrEntry* variables);

// fills 'buffer' with the result of expanding 'varText'; returns FALSE if the
// buffer is too small (the variable string should be validated first via
// ValidateVarString, otherwise FALSE is also returned for syntax errors) or when
// the user clicked Cancel for an environment-variable error (not found or too
// long). 'bufferLen' is the size of 'buffer'. 'variables' is an array of
// CSalamanderVarStrEntry structures terminated by Name==NULL. 'param' is passed
// to CSalamanderVarStrEntry::Execute when expanding a variable. 'msgParent' is
// the parent for error message boxes; if NULL, errors are not shown. When
// 'ignoreEnvVarNotFoundOrTooLong' is TRUE, environment-variable errors are
// ignored; otherwise a message box is shown. If 'varPlacements' is not NULL it
// points to an array of DWORDs of '*varPlacementsCount' items that receives the
// variable positions in the output buffer (low WORD) and their lengths (high
// WORD). If 'varPlacementsCount' is not NULL it receives the number of filled
// entries.
// When this method is used just once for a single 'param', set
// 'detectMaxVarWidths' to FALSE, 'maxVarWidths' to NULL and 'maxVarWidthsCount'
// to 0. When expanding repeatedly for a set of values (e.g. Make File List), it
// makes sense to use variables like "$(varname:max)"; their width is measured as
// the maximum width of the expanded variable across the whole set. Measuring is
// done during the first cycle (for all values) with 'detectMaxVarWidths' TRUE
// and 'maxVarWidths' zeroed. The actual expansion then happens in the second
// cycle with 'detectMaxVarWidths' FALSE and 'maxVarWidths' containing the
// precomputed widths from the first cycle.
BOOL ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                     const CSalamanderVarStrEntry* variables, void* param,
                     BOOL ignoreEnvVarNotFoundOrTooLong = FALSE,
                     DWORD* varPlacements = NULL, int* varPlacementsCount = NULL,
                     BOOL detectMaxVarWidths = FALSE, int* maxVarWidths = NULL,
                     int maxVarWidthsCount = 0);

// stores the Unicode version of 'str' of length 'len' on the clipboard
// returns ERROR_SUCCESS or GetLastError
DWORD AddUnicodeToClipboard(const char* str, int len);

// puts text on the clipboard; if showEcho is TRUE a message box confirming success is shown
// when textLen==-1 the length is calculated automatically
BOOL CopyTextToClipboard(const char* text, int textLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL);
BOOL CopyTextToClipboardW(const wchar_t* text, int textLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL);
BOOL CopyHTextToClipboard(HGLOBAL hGlobalText, int textLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL);

// examines buffer 'pattern' of length 'patternLen' to determine whether it is text (that is,
// whether there is a code page in which it contains only allowed characters - printable and control).
// If it is text, it also determines its most probable code page. 'parent' is the parent of the
// message box. When 'forceText' is TRUE, forbidden characters are not checked (used when 'pattern'
// contains text). If 'isText' is not NULL it receives TRUE if the buffer is text. If 'codePage'
// is not NULL, it is a buffer (at least 101 chars) for the code page name
void RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                       BOOL* isText, char* codePage);

// sets the calling thread name in the VC debugger
void SetThreadNameInVC(LPCSTR szThreadName);

// sets the calling thread name in both the VC debugger and the Trace Server
void SetThreadNameInVCAndTrace(const char* name);

// configuration loading
class CEditorMasks;
class CViewerMasks;

// functions related to drag&drop and other shell tasks

class CFilesWindow;

// shell operations
enum CShellAction
{
    saLeftDragFiles,
    saRightDragFiles,
    saContextMenu,
    saCopyToClipboard,
    saCutToClipboard,
    saProperties,
    saPermissions, // same as saProperties but tries to activate the "security" tab
};

class CCopyMoveData;
struct CDragDropOperData;

const char* GetCurrentDir(POINTL& pt, void* param, DWORD* effect, BOOL rButton, BOOL& tgtFile,
                          DWORD keyState, int& tgtType, int srcType);
const char* GetCurrentDirClipboard(POINTL& pt, void* param, DWORD* effect, BOOL rButton,
                                   BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType);
BOOL DoCopyMove(BOOL copy, char* targetDir, CCopyMoveData* data, void* param);
void DoDragDropOper(BOOL copy, BOOL toArchive, const char* archiveOrFSName, const char* archivePathOrUserPart,
                    CDragDropOperData* data, void* param);
void DoGetFSToFSDropEffect(const char* srcFSPath, const char* tgtFSPath,
                           DWORD allowedEffects, DWORD keyState,
                           DWORD* dropEffect, void* param);
BOOL UseOwnRutine(IDataObject* pDataObject);
BOOL MouseConfirmDrop(DWORD& effect, DWORD& defEffect, DWORD& grfKeyState);
void DropEnd(BOOL drop, BOOL shortcuts, void* param, BOOL ownRutine, BOOL isFakeDataObject, int tgtType);
void EnterLeaveDrop(BOOL enter, void* param);

// stores the preferred drop effect on the clipboard and marks that it originated from Salamander
void SetClipCutCopyInfo(HWND hwnd, BOOL copy, BOOL salObject);

void ShellAction(CFilesWindow* panel, CShellAction action, BOOL useSelection = TRUE,
                 BOOL posByMouse = TRUE, BOOL onlyPanelMenu = FALSE);
void ExecuteAssociation(HWND hWindow, const char* path, const char* name);

BOOL CanUseShellExecuteWndAsParent(const char* cmdName);

// checks whether the file is a placeholder (an online file in a OneDrive folder),
// see http://msdn.microsoft.com/en-us/library/windows/desktop/dn323738%28v=vs.85%29.aspx
BOOL IsFilePlaceholder(WIN32_FIND_DATA const* findData);

// before opening the editor or viewer, the placeholder is converted to an offline file
// so that the viewer/editor can work with it
//BOOL MakeFileAvailOfflineIfOneDriveOnWin81(HWND parent, const char *name);

// sets the thread priority to normal and calls menu->InvokeCommand() in a try-except block;
// before exiting it restores the thread priority to its original value
BOOL SafeInvokeCommand(IContextMenu2* menu, CMINVOKECOMMANDINFO& ici);

// if 'hInstance' is NULL strings are loaded from HLanguage; otherwise from 'hInstance'
char* LoadStr(int resID, HINSTANCE hInstance = NULL);   // loads string from resources
WCHAR* LoadStrW(int resID, HINSTANCE hInstance = NULL); // loads wide-string from resources

// support for creating parameterized texts (handling singular and plural forms
// in text); 'lpFmt' is the format string for the resulting text - its format
// is described below; the resulting text is returned in the 'lpOut' buffer of
// size 'nOutMax' bytes; 'lpParArray' is the array of text parameters and
// 'nParCount' is the number of these parameters; returns the length of the
// resulting text
//
// description of the format string:
//   - every format string starts with the signature "{!}"
//   - the following escape sequences are recognized to suppress the special
//     meaning of characters in the format string (the backslash character in
//     this description is not doubled): "\\" = "\", "\{" = "{", "\}" = "}", "\:" = ":" and "\|" = "|"
//   - text outside curly braces is copied to the resulting string unchanged
//     (except for escape sequences)
//   - parameterized text is enclosed in curly braces
//   - each parameterized text uses one parameter from 'lpParArray' - it is a
//     64-bit unsigned int
//   - parameterized text contains different resulting texts for different
//     parameter value ranges
//   - individual resulting texts and range boundaries are separated by "|"
//   - parameterized text "{}" is used to skip one parameter from 'lpParArray'
//     (it produces no output text)
//   - if a parameterized text starts with a number followed by a colon, it is
//     the index of the parameter to use (from one up to the number of
//     parameters); if no index is specified, it is assigned automatically
//     (sequentially from one up to the number of parameters)
//   - specifying a parameter index does not change the sequentially assigned
//     index; for example, in "{!}%d file{2:s|0||1|s} and %d director{y|1|ies}"
//     the first parameterized text uses parameter 2 and the second uses
//     parameter 1
//   - any number of parameterized texts with an explicitly specified index may
//     be used
//
// examples of format strings:
//   - "{!}director{y|1|ies}" for parameter values from 0 to 1 (inclusive) will
//     be "directory", and for values from 2 to "infinity" (2^64-1) will be
//     "directories"
//   - "{!}soubo{rů|0|r|1|ry|4|rů}" for parameter value 0 will be "souborů", for
//     1 will be "soubor", for 2 to 4 (inclusive) will be "soubory", and from 5
//     to "infinity" will be "souborů"
int ExpandPluralString(char* lpOut, int nOutMax, const char* lpFmt, int nParCount,
                       const CQuadWord* lpParArray);

//
// Writes the text to 'lpOut' depending on the 'files' and 'dirs' variables:
// files > 0 && dirs == 0  ->  XXX (selected/hidden) files
// files == 0 && dirs > 0  ->  YYY (selected/hidden) directories
// files > 0 && dirs > 0   ->  XXX (selected/hidden) files and YYY directories
//
// where XXX and YYY correspond to the values of 'files' and 'dirs'.
// The variable 'selectedForm' controls insertion of the word "selected".
//
// 'forDlgCaption' is TRUE/FALSE if the text is meant for a dialog caption
// (capitalization is required in English).
//
// Returns the number of copied characters excluding the terminator.
//
// description of epfdmXXX constants is in spl_gen.h
int ExpandPluralFilesDirs(char* lpOut, int nOutMax, int files, int dirs,
                          int mode, BOOL forDlgCaption);
int ExpandPluralBytesFilesDirs(char* lpOut, int nOutMax, const CQuadWord& selectedBytes,
                               int files, int dirs, BOOL useSubTexts);

// Searches the text for '<' '>' pairs, removes them from the buffer, and stores
// references to their contents in 'varPlacements'. 'varPlacements' is an array of
// DWORDs with '*varPlacementsCount' items; each DWORD is composed of the position
// of the reference in the output buffer (low WORD) and the number of characters
// in the reference (high WORD). The strings "\<", "\>", "\\" are treated as
// escape sequences and are replaced with '<', '>' and '\\'.
// Returns TRUE on success, otherwise FALSE; always sets 'varPlacementsCount' to
// the number of processed variables.
BOOL LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount);

void MinimizeApp(HWND mainWnd);             // minimize the application
void RestoreApp(HWND mainWnd, HWND dlgWnd); // restore from minimized state
                                            // adjusts the name format (letter case), filename must be null-terminated
void AlterFileName(char* tgtName, char* filename, int filenameLen, int format, int change, BOOL dir);

// returns a string with the file size and times; 'fileTime' receives the time (may be NULL);
// if 'getTimeFailed' is not NULL, it is set to TRUE on failure to obtain the file time
void GetFileOverwriteInfo(char* buff, int buffLen, HANDLE file, const char* fileName, FILETIME* fileTime = NULL, BOOL* getTimeFailed = NULL);

void ColorsChanged(BOOL refresh, BOOL colorsOnly, BOOL reloadUMIcons);                // call after color change
HICON GetDriveIcon(const char* root, UINT type, BOOL accessible, BOOL large = FALSE); // drive icon
HICON SalLoadIcon(HINSTANCE hDLL, int id, int iconSize);

// SetCurrentDirectory(system directory) - detach from panel directory
void SetCurrentDirectoryToSystem();

// replaces SUBST drives in 'resPath' with their targets (converts to a path without SUBST drive letters);
// returns FALSE on error
BOOL ResolveSubsts(char* resPath);

// Resolves SUBSTs and reparse points for 'path' and then tries to obtain the GUID path for
// the mount point (or for the root if missing). On failure returns FALSE. On success returns
// TRUE and sets 'mountPoint' and 'guidPath' (if not NULL, they must be buffers at least
// MAX_PATH in size; strings will end with a backslash).
BOOL GetResolvedPathMountPointAndGUID(const char* path, char* mountPoint, char* guidPath);

// attempts to return correct values (handles reparse points - specify the full path instead of the root)
CQuadWord MyGetDiskFreeSpace(const char* path, CQuadWord* total = NULL);
// NOTE: do not use the return values 'lpNumberOfFreeClusters' and 'lpTotalNumberOfClusters'
// for large disks they may overflow. Use MyGetDiskFreeSpace(path, total) which returns 64-bit numbers.
BOOL MyGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters);

// enhanced GetVolumeInformation: works with the path (walks reparse points and SUBST drives)
// 'rootOrCurReparsePoint' (if not NULL and at least MAX_PATH chars) receives the
// root or the path to the current (last) local reparse point on 'path'
// (WARNING: this does not work if no medium is present in the drive; GetCurrentLocalReparsePoint() is not affected by this)
// 'junctionOrSymlinkTgt' (if not NULL and at least MAX_PATH chars) receives the
// target of the current reparse point or an empty string when none exists or it
// is of unknown type or a volume mount point. 'linkType' (if not NULL) receives
// the type of the current reparse point: 0 (unknown or none), 1 (MOUNT POINT),
// 2 (JUNCTION POINT), 3 (SYMBOLIC LINK)
BOOL MyGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, char* junctionOrSymlinkTgt, int* linkType,
                            LPTSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                            LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                            LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

// returns the target path of the reparse point 'repPointDir' in 'repPointDstBuf'
// (if not NULL) of size 'repPointDstBufSize'. 'repPointDir' and 'repPointDstBuf'
// may point into the same buffer (IN/OUT). If 'makeRelPathAbs' is TRUE and it is a
// relative symbolic link, the target path is converted to an absolute path.
// Returns TRUE on success and when 'repPointType' is not NULL it receives the
// reparse point type: 1 (MOUNT POINT), 2 (JUNCTION POINT), 3 (SYMBOLIC LINK)
BOOL GetReparsePointDestination(const char* repPointDir, char* repPointDstBuf, DWORD repPointDstBufSize,
                                int* repPointType, BOOL makeRelPathAbs);

// 'currentReparsePoint' (at least MAX_PATH chars) receives the current (last)
// local reparse point; on failure the standard root is returned and the result
// is FALSE. If 'error' is not NULL it is set to TRUE when an error occurs.
BOOL GetCurrentLocalReparsePoint(const char* path, char* currentReparsePoint, BOOL* error = NULL);

// call only for paths whose root (after removing SUBSTs) is DRIVE_FIXED (there is
// no point searching for reparse points elsewhere); we look for a path without
// reparse points that leads to the same volume as 'path'; for a path containing
// a symlink that leads to a network path (UNC or mapped), we return only the
// root of that network path (even Vista cannot work with reparse points on
// network paths, so this is probably unnecessary); if no such path exists
// because the current (last) local reparse point is a volume mount point (or an
// unknown type of reparse point), we return the path to this volume mount point
// (or reparse point of an unknown type); if the path contains more than 50
// reparse points (most likely an infinite loop), we return the original path;
//
// 'resPath' is a MAX_PATH-sized output buffer; 'path' is the original path; in
// 'cutResPathIsPossible' (must not be NULL) we return FALSE if the resulting
// path in 'resPath' ends with a reparse point (volume mount point or an unknown
// type of reparse point) and therefore must not be shortened (that would most
// likely take us to a different volume); if 'rootOrCurReparsePointSet' is not
// NULL and contains FALSE, and the original path contains at least one local
// reparse point (reparse points on the network part of the path are ignored),
// we return TRUE in this variable and in 'rootOrCurReparsePoint' (if not NULL)
// we return the full path to the current (last local) reparse point (note: not
// where it leads); the target path of the current reparse point (only if it is
// a junction or symlink) is returned in 'junctionOrSymlinkTgt' (if not NULL)
// and its type is returned in 'linkType': 2 (JUNCTION POINT), 3 (SYMBOLIC
// LINK); in 'netPath' (if not NULL) we return the network path to which the
// current (last) local symlink in the path leads - in this situation, the root
// of the network path is returned in 'resPath'
void ResolveLocalPathWithReparsePoints(char* resPath, const char* path, BOOL* cutResPathIsPossible,
                                       BOOL* rootOrCurReparsePointSet, char* rootOrCurReparsePoint,
                                       char* junctionOrSymlinkTgt, int* linkType, char* netPath);

// improved GetDriveType: works with a path and resolves reparse points and SUBSTs
UINT MyGetDriveType(const char* path);

// our own QueryDosDevice
// 'driveNum' is 0-based (0=A: 2=C: ...)
BOOL MyQueryDosDevice(BYTE driveNum, char* target, int maxTarget);

// detects whether 'driveNum' (0=A: 2=C: ...) is SUBSTed and if so where it is mounted
// returns FALSE when the drive is not SUBSTed
// when SUBSTed, returns TRUE and stores the target path into 'path'
// (up to 'pathMax' characters). If 'path' is NULL the path is not returned.
// The returned path may be in UNC form.
BOOL GetSubstInformation(BYTE driveNum, char* path, int pathMax);

// replaces the last '.' in the string with the decimal separator from LOCALE_SDECIMAL
// the string may grow because the separator can have up to 4 characters according to MSDN
// returns TRUE if the buffer was large enough and the operation succeeded, otherwise FALSE
BOOL PointToLocalDecimalSeparator(char* buffer, int bufferSize);

typedef WINBASEAPI LONG(WINAPI* MY_FMExtensionProc)(HWND hwnd,
                                                    WORD wMsg,
                                                    LONG lParam);
void GetMessagePos(POINT& p);

// Returns an icon handle obtained via SHGetFileInfo or NULL on failure.
// The caller is responsible for destroying the icon. The icon is assigned to HANDLES.
HICON GetFileOrPathIconAux(const char* path, BOOL large, BOOL isDir);

// If the UNC root of `UNCPath` is inaccessible (for listing), tries to establish a
// network connection and prompts the user for credentials if needed. Returns TRUE
// if the connection is established. Returns FALSE if `UNCPath` is not a UNC path,
// if the UNC root is accessible, or if the connection could not be established. In
// `pathInvalid`, returns TRUE if the user cancelled the credentials dialog or if
// establishing the connection failed (e.g. "credentials conflict"). If
// `donotReconnect` is TRUE, no network connection is attempted and FALSE is
// returned immediately.
BOOL CheckAndConnectUNCNetworkPath(HWND parent, const char* UNCPath, BOOL& pathInvalid,
                                   BOOL donotReconnect);

// Attempts to restore a network connection (if it previously existed) on
// 'drive:'. 'parent' is the parent dialog. Returns TRUE if the connection was
// restored successfully (the network drive is mapped again).
// 'pathInvalid' returns TRUE if the user cancelled the username/password dialog
// or an attempt to establish the connection failed (e.g. "credentials conflict")
BOOL CheckAndRestoreNetworkConnection(HWND parent, const char drive, BOOL& pathInvalid);

// thread management helpers-these threads should exit when the process ends; if
// they do not, they must be terminated
void AddAuxThread(HANDLE view, BOOL testIfFinished = FALSE);
void TerminateAuxThreads();

// returns TRUE if the specified file exists; otherwise FALSE
extern "C" BOOL FileExists(const char* fileName);

// returns TRUE if the specified directory exists; otherwise FALSE
BOOL DirExists(const char* dirName);

// tool tip
void SetCurrentToolTip(HWND hNotifyWindow, DWORD id, int showDelay = 0); // see tooltip.h
void SuppressToolTipOnCurrentMousePos();                                 // see tooltip.h

// Makes Ctrl+Left/Right skip over backslashes and spaces. Assigns
// EditWordBreakProc to an edit line or combo box. Can be called from
// WM_INITDIALOG. Uninstalling is not required.
BOOL InstallWordBreakProc(HWND hWindow);

// removes all items from a combo box drop-down list
// used when clearing history
void ClearComboboxListbox(HWND hCombo);

// structure for WM_USER_VIEWFILE and WM_USER_VIEWFILEWITH
struct COpenViewerData
{
    char* FileName;
    int EnumFileNamesSourceUID;
    int EnumFileNamesLastFileIndex;
};

//
// ****************************************************************************

#define WM_USER_REFRESH_DIR WM_APP + 100   // [BOOL setRefreshEvent, time]
#define WM_USER_S_REFRESH_DIR WM_APP + 101 // [BOOL setRefreshEvent, time]

#define WM_USER_SETDIALOG WM_APP + 103 // [CProgressData *data, 0] \
                                       // or [0, 0] - setprogress
#define WM_USER_DIALOG WM_APP + 104    // [int dlgID, void *data]

// the icon reader has loaded an icon and stored it in the icon cache; it notifies
// the panel running in the main thread so it can repaint and back up the icon in the associations
// 'index' is the located position of the item in CFilesWindow::Files/Dirs
#define WM_USER_REFRESHINDEX WM_APP + 105 // [int index, 0]

#define WM_USER_END_SUSPMODE WM_APP + 106  // [0, 0] - faster window activation
#define WM_USER_DRIVES_CHANGE WM_APP + 107 // [0, 0]
#define WM_USER_ICON_NOTIFY WM_APP + 108   // [0, 0] - the mouse pointer is over the icon in the taskbar
#define WM_USER_EDIT WM_APP + 110          // [begin, end] select this interval
#define WM_USER_SM_END_NOTIFY WM_APP + 111 // [0, 0] schedules WM_USER_SM_END_NOTIFY_DELAYED after 200ms
#define WM_USER_DISPLAYPOPUP WM_APP + 112  // [0, commandID] a popup menu should be displayed
//#define WM_USER_SETPATHS        WM_APP + 113    // do not use, legacy message that old applications may theoretically send us

#define WM_USER_CHAR WM_APP + 114 // notification from list view
// [command, index]
// command = 0 for normal configuration
// command = 1 to open the Hot Paths page; 'index' specifies the item
// command = 2 to open the User Menu page
// command = 3 to open the Internal Viewer page
// command = 4 to open the Views page; 'index' specifies the view
// command = 5 to open the Panels page
#define WM_USER_CONFIGURATION WM_APP + 115
#define WM_USER_MOUSEWHEEL WM_APP + 116     // [wParam, lParam] z WM_MOUSEWHEEL
#define WM_USER_SKIPONEREFRESH WM_APP + 117 // [0, 0] za 500 ms SkipOneActivateRefresh = FALSE
#define WM_USER_FLASHWINDOW WM_APP + 118    // [0, 0] flashes the window
#define WM_USER_SHOWWINDOW WM_APP + 119     // [0, 0] brings the window to the foreground (restores if needed)
#define WM_USER_DROPCOPYMOVE WM_APP + 120   // [CTmpDropData *, 0]
#define WM_USER_CHANGEDIR WM_APP + 121      // [convertFSPathToInternal, newDir] - the panel changes its path (calls ChangeDir)
#define WM_USER_FOCUSFILE WM_APP + 122      // [fileName, newPath] - the panel changes its path and selects the given file
#define WM_USER_CLOSEFIND WM_APP + 123      // [0, 0] - calls DestroyWindow from the find window thread
#define WM_USER_SELCHANGED WM_APP + 124     // [0, 0] - notification about selection change
#define WM_USER_MOUSEHWHEEL WM_APP + 126    // [wParam, lParam] z WM_MOUSEHWHEEL

#define WM_USER_CLOSEMENU WM_APP + 130 // [0, 0] - internal for menus - they must close

#define WM_USER_REFRESH_PLUGINFS WM_APP + 133 // [0, 0] - call for FS Event(FSE_ACTIVATEREFRESH)
#define WM_USER_REFRESH_SHARES WM_APP + 134   // [0, 0] - snooper.cpp reports a change in shares in the registry
#define WM_USER_PROCESSDELETEMAN WM_APP + 135 // [0, 0] - cache.cpp: DeleteManager - start processing new data in the main thread

#define WM_USER_CANCELPROGRDLG WM_APP + 136  // [0, 0] - CProgressDlgArray: when this message arrives the operation is canceled (no prompt; the dialog closes)
#define WM_USER_FOCUSPROGRDLG WM_APP + 137   // [0, 0] - CProgressDlgArray: activates the dialog (or its popup) when received
#define WM_USER_ICONREADING_END WM_APP + 138 // [0, 0] - notification that icon reading in the panel has finished

// moved to shexreg.h (constant must not change): #define WM_USER_SALSHEXT_PASTE  WM_APP + 139 // [postMsgIndex, 0] - SalamExt requests execution of the Paste command

#define WM_USER_DROPUNPACK WM_APP + 140    // [allocatedTgtPath, operation] - drag&drop from archive: target path and operation determined, perform unpack
#define WM_USER_PROGRDLGEND WM_APP + 141   // [cmd, 0] - CProgressDialog: workaround for bugs on W2K+ (closed dialogs remained in the taskbar) - delayed close of the dialog
#define WM_USER_PROGRDLGSTART WM_APP + 142 // [0, 0] - CProgressDialog: workaround for bugs on W2K+ (garbage left on screen) - delayed start of the worker thread

// moved to shexreg.h (constant must not change): #define WM_USER_SALSHEXT_TRYRELDATA WM_APP + 143 // [0, 0] - SalamExt reports release of the paste data lock (see CSalShExtSharedMem::BlockPasteDataRelease); if the data is no longer protected, let it be released

#define WM_USER_DROPFROMFS WM_APP + 144    // [allocatedTgtPath, operation] - drag&drop from FS: target path and operation determined, perform copy/move from FS
#define WM_USER_DROPTOARCORFS WM_APP + 145 // [CTmpDragDropOperData *, 0]

#define WM_USER_SHCHANGENOTIFY WM_APP + 146 // message for SHChangeNotifyRegister [pidlList, SHCNE_xxx (event that occured)]

#define WM_USER_REFRESH_DIR_EX WM_APP + 147 // [long_wait, time] - after (long_wait ? 5000 : 200) ms sends WM_USER_REFRESH_DIR_EX_DELAYED

#define WM_USER_SETPROGRESS WM_APP + 148 // [progress, text] used to cross thread boundaries

// icon reader has just loaded an icon overlay and informs the panel running in the main thread so it can repaint
// index is the position of the item in CFilesWindow::Files/Dirs
#define WM_USER_REFRESHINDEX2 WM_APP + 149 // [int index, 0]

#define WM_USER_DONEXTFOCUS WM_APP + 150       // [0, 0] - notification that NextFocusName changed
#define WM_USER_CREATEWAITWND WM_APP + 151     // [parent or NULL, delay] - message for the safe wait message thread: "create && show"
#define WM_USER_DESTROYWAITWND WM_APP + 152    // [killThread, 0] - message for the safe wait message thread: "hide && destroy"
#define WM_USER_SHOWWAITWND WM_APP + 153       // [show, 0] - message for the safe wait message thread: "show || hide"
#define WM_USER_SETWAITMSG WM_APP + 154        // [0, 0] - message for the safe wait message thread: text changed-repaint
#define WM_USER_REPAINTALLICONS WM_APP + 155   // [0, 0] - refresh icons in both panels
#define WM_USER_REPAINTSTATUSBARS WM_APP + 156 // [0, 0] - refresh the throbber (dirline) in both panels

#define WM_USER_VIEWERCONFIG WM_APP + 158 // [hWnd, 0] - hWnd indicates the viewer to bring to front after configuration

#define WM_USER_UPDATEPANEL WM_APP + 159 // [0, 0] - when delivered \
                                         // (the message loop sends it after opening a message box), \
                                         // the panel will be invalidated and the scrollbar updated \
                                         // (for internal use only)

#define WM_USER_AUTOCONFIG WM_APP + 160     // KICKER - autoconfig
#define WM_USER_ACFINDFINISHED WM_APP + 161 // KICKER - autoconfig
#define WM_USER_ACSEARCHING WM_APP + 162    // KICKER - autoconfig
#define WM_USER_ACADDFILE WM_APP + 163      // KICKER - autoconfig
#define WM_USER_ACERROR WM_APP + 164        // KICKER - autoconfig

#define WM_USER_QUERYCLOSEFIND WM_APP + 170  // [0, quiet] - ask the Find window thread whether it can be closed and stop any ongoing search if requested
#define WM_USER_COLORCHANGEFIND WM_APP + 171 // [0, 0] - notifies Find windows about color changes

#define WM_USER_HELPHITTEST WM_APP + 172  // lResult = dwContext, lParam = MAKELONG(x,y)
#define WM_USER_EXITHELPMODE WM_APP + 173 // [0, 0]

#define WM_USER_POSTCMDORUNLOADPLUGIN WM_APP + 180 // [plugin iface, 0, 1 or salCmd+2 or menuCmd+502] - sets ShouldUnload or ShouldRebuildMenu or adds salCmd/menuCmd to plugin data
#define WM_USER_POSTMENUEXTCMD WM_APP + 181        // [plugin iface, cmdID] - post a menu-ext command from a plugin

#define WM_USER_SHOWPLUGINMSGBOX WM_APP + 185 // [0, 0] - open the plugin message box above the Bug Report dialog

// commands for the main thread (cannot be run from another thread) - uses the
// Find dialog (running in its own thread)
#define WM_USER_VIEWFILE WM_APP + 190     // [COpenViewerData *, altView] - open a file in the (alternate) viewer
#define WM_USER_EDITFILE WM_APP + 191     // [name, 0] - open a file in the editor
#define WM_USER_VIEWFILEWITH WM_APP + 192 // [COpenViewerData *, handlerID] - open a file in the selected viewer
#define WM_USER_EDITFILEWITH WM_APP + 193 // [name, handlerID] - open a file in the selected editor

#define WM_USER_DISPACHCHANGENOTIF WM_APP + 194 // [0, time] - request to distribute notifications about path changes

#define WM_USER_DISPACHCFGCHANGE WM_APP + 195 // [0, 0] - request to distribute configuration change notifications among plugins

#define WM_USER_CFGCHANGED WM_APP + 196 // [0, 0] - sent to internal viewers and Find windows after configuration changes

#define WM_USER_CLEARHISTORY WM_APP + 197 // [0, 0] - instructs the window to clear all combo boxes containing histories

#define WM_USER_REFRESHTOOLTIP WM_APP + 198 // sent to the tooltip window: fetch text again, resize the window, and repaint
#define WM_USER_HIDETOOLTIP WM_APP + 199    // cross-thread message; hides the tooltip

////////////////////////////////////////////////////////
//                                                    //
// The range WM_APP + 200 to WM_APP + 399 is reserved  //
// for messages also sent to plugin windows.           //
// Definitions are in plugins\shared\spl_*.h           //
//                                                    //
////////////////////////////////////////////////////////

#define WM_USER_ENUMFILENAMES WM_APP + 400 // [requestUID, 0] - tells the source (panels and Find windows) to \
                                           // handle a request for file enumeration for the viewer

#define WM_USER_SM_END_NOTIFY_DELAYED WM_APP + 401  // [0, 0] notification that suspend mode ended \
                                                    // delayed by 200 ms to avoid conflicts with \
                                                    // WM_QUERYENDSESSION during Shutdown/Log Off
#define WM_USER_REFRESH_DIR_EX_DELAYED WM_APP + 402 // [FALSE, time] - unlike WM_USER_REFRESH_DIR this is \
                                                    // probably an unnecessary refresh (window activation, \
                                                    // request for a lock volume similar to "hands-off", etc.) \
                                                    // delayed by 200 ms or 5 s so it does not collide with \
                                                    // WM_QUERYENDSESSION during Shutdown/Log Off or so the \
                                                    // process requesting the lock can lock the volume

#define WM_USER_CLOSE_MAINWND WM_APP + 403 // [0, 0] - used instead of WM_CLOSE for closing the main \
                                           // Salamander window (advantage: we can detect whether it \
                                           // is dispatched from a non-main message loop)

#define WM_USER_HELP_MOUSEMOVE WM_APP + 405  // [0, mousePos] - sent during Shift+F1 (context help) mode \
                                             // to all child windows; after one or more WM_USER_MOUSEMOVE \
                                             // messages WM_USER_MOUSELEAVE follows; used to track the \
                                             // mouse cursor without capture
#define WM_USER_HELP_MOUSELEAVE WM_APP + 406 // [0, 0] - sent after WM_USER_MOUSEMOVE when the cursor leaves a child window

//#define WM_USER_RENAME_NEXT_ITEM       WM_APP + 407 // [next, 0] - posted after pressing (Shift)Tab in inplace QuickRename to move to the (previous) next item; inspired by Vista; 'next' is TRUE for next and FALSE for previous

#define WM_USER_PROGRDLG_UPDATEICON WM_APP + 408 // [0, 0] - CProgressDlgArray: when this message arrives the \
                                                 // icon of the CProgressDialog is updated

#define WM_USER_FORCECLOSE_MAINWND WM_APP + 409 // [0, 0] - forced closing of the main Salamander window

#define WM_USER_INACTREFRESH_DIR WM_APP + 410 // [0, time] - delayed refresh when the main Salamander window is inactive

#define WM_USER_WAKEUP_FROM_IDLE WM_APP + 411 // [0, 0] - wakes up the main thread when it is IDLE

#define WM_USER_FINDFULLROWSEL WM_APP + 412 // [0, 0] - Find windows must set their list view to match Configuration.FindFullRowSelect

#define WM_USER_SLGINCOMPLETE WM_APP + 414 // [0, 0] - notification that SLG is not fully translated; encourages contributions

#define WM_USER_USERMENUICONS_READY WM_APP + 415 // [bkgndReaderData, threadID] - notification for the main window \
                                                 // that reading icons for the User Menu has finished in the \
                                                 // thread with ID 'threadID'

// states for Shift+F1 help mode
#define HELP_INACTIVE 0 // not in Shift+F1 help mode (must be 0)
#define HELP_ACTIVE 1   // in Shift+F1 help mode (non-zero)
#define HELP_ENTERING 2 // entering Shift+F1 help mode (non-zero)

#define STACK_CALLS_BUF_SIZE 5000       // each thread will have 5KB space for the text call stack
#define STACK_CALLS_MAX_MESSAGE_LEN 500 // the longest message is assumed to be 500 characters

#define MENU_MARK_CX 9 // check mark dimensions for menus
#define MENU_MARK_CY 9

#define BOTTOMBAR_CX 17 // button dimension in bottomtb.bmp (points)
#define BOTTOMBAR_CY 13

// colors in CurrentColors[x]
#define FOCUS_ACTIVE_NORMAL 0 // pen colors for the item frame
#define FOCUS_ACTIVE_SELECTED 1
#define FOCUS_FG_INACTIVE_NORMAL 2
#define FOCUS_FG_INACTIVE_SELECTED 3
#define FOCUS_BK_INACTIVE_NORMAL 4
#define FOCUS_BK_INACTIVE_SELECTED 5

#define ITEM_FG_NORMAL 6 // text colors of items in the panel
#define ITEM_FG_SELECTED 7
#define ITEM_FG_FOCUSED 8
#define ITEM_FG_FOCSEL 9
#define ITEM_FG_HIGHLIGHT 10

#define ITEM_BK_NORMAL 11 // background color of panel items
#define ITEM_BK_SELECTED 12
#define ITEM_BK_FOCUSED 13
#define ITEM_BK_FOCSEL 14
#define ITEM_BK_HIGHLIGHT 15

#define ICON_BLEND_SELECTED 16 // colors for icon blending
#define ICON_BLEND_FOCUSED 17
#define ICON_BLEND_FOCSEL 18

#define PROGRESS_FG_NORMAL 19 // progress bar colors
#define PROGRESS_FG_SELECTED 20
#define PROGRESS_BK_NORMAL 21
#define PROGRESS_BK_SELECTED 22

#define HOT_PANEL 23    // color of the hot item in the panel
#define HOT_ACTIVE 24   // in the active panel title
#define HOT_INACTIVE 25 //                   in the inactive panel caption, status bar, ...

#define ACTIVE_CAPTION_FG 26   // text color in the active panel title
#define ACTIVE_CAPTION_BK 27   // background color in the active panel title
#define INACTIVE_CAPTION_FG 28 // text color in the inactive panel title
#define INACTIVE_CAPTION_BK 29 // background color in the inactive panel caption

#define THUMBNAIL_FRAME_NORMAL 30 // pen colors for the frame around thumbnails
#define THUMBNAIL_FRAME_FOCUSED 31
#define THUMBNAIL_FRAME_SELECTED 32
#define THUMBNAIL_FRAME_FOCSEL 33

#define VIEWER_FG_NORMAL 0 // normal viewer colors
#define VIEWER_BK_NORMAL 1
#define VIEWER_FG_SELECTED 2 // selected text
#define VIEWER_BK_SELECTED 3

#define NUMBER_OF_COLORS 34       // number of colors in the scheme
#define NUMBER_OF_VIEWERCOLORS 4  // number of colors for the viewer
#define NUMBER_OF_CUSTOMCOLORS 16 // user-defined colors in the color dialog

// internal color holder with an extra flag
typedef DWORD SALCOLOR;

// SALCOLOR flags
#define SCF_DEFAULT 0x01 // the color component is ignored and the default value is used

#define GetCOLORREF(rgbf) ((COLORREF)rgbf & 0x00ffffff)
#define RGBF(r, g, b, f) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16) | (((DWORD)(BYTE)(f)) << 24)))
#define GetFValue(rgbf) ((BYTE)((rgbf) >> 24))

inline void SetRGBPart(SALCOLOR* salColor, COLORREF rgb)
{
    *salColor = rgb & 0x00ffffff | (((DWORD)(BYTE)((BYTE)((*salColor) >> 24))) << 24);
}

extern SALCOLOR* CurrentColors;               // current colors
extern SALCOLOR UserColors[NUMBER_OF_COLORS]; // modified colors

extern SALCOLOR SalamanderColors[NUMBER_OF_COLORS]; // standard colors
extern SALCOLOR ExplorerColors[NUMBER_OF_COLORS];   // standard colors
extern SALCOLOR NortonColors[NUMBER_OF_COLORS];     // standard colors
extern SALCOLOR NavigatorColors[NUMBER_OF_COLORS];  // standard colors

extern SALCOLOR ViewerColors[NUMBER_OF_VIEWERCOLORS]; // viewer colors

extern COLORREF CustomColors[NUMBER_OF_CUSTOMCOLORS]; // for the standard color dialog

#define CARET_WIDTH 2
#define MIN_PANELWIDTH 5 // a narrower panel does not receive focus

#define REFRESH_PAUSE 200 // pause between the two nearest refreshes

extern int SPACE_WIDTH; // spacing between columns in detailed view

#define MENU_CHECK_WIDTH 8 // dimensions of the check bitmap for menus
#define MENU_CHECK_HEIGHT 8

// numbers of remembered strings
#define SELECT_HISTORY_SIZE 20    // select / unselect
#define COPY_HISTORY_SIZE 20      // copy / move
#define EDIT_HISTORY_SIZE 30      // command line
#define CHANGEDIR_HISTORY_SIZE 20 // Shift+F7
#define PATH_HISTORY_SIZE 30      // forward/backward path history + visited paths history (Alt+F12)
#define FILTER_HISTORY_SIZE 15    // filter
#define FILELIST_HISTORY_SIZE 15
#define CREATEDIR_HISTORY_SIZE 20   // create directory
#define QUICKRENAME_HISTORY_SIZE 20 // quick rename
#define EDITNEW_HISTORY_SIZE 20     // edit new
#define CONVERT_HISTORY_SIZE 15     // convert

#define VK_LBRACKET 219
#define VK_BACKSLASH 220
#define VK_RBRACKET 221

// when to test for an attempt to interrupt script building
#define BS_TIMEOUT 200 // milliseconds since the last test

// band identifiers in the rebar
#define BANDID_MENU 1
#define BANDID_TOPTOOLBAR 2
#define BANDID_UMTOOLBAR 3
#define BANDID_DRIVEBAR 4
#define BANDID_DRIVEBAR2 5
#define BANDID_WORKER 6
#define BANDID_HPTOOLBAR 7
#define BANDID_PLUGINSBAR 8

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// reasons why some files/directories are not shown in the panel
#define HIDDEN_REASON_ATTRIBUTE 0x00000001 // have the hidden or system attribute and configuration suppresses such files/directories
#define HIDDEN_REASON_FILTER 0x00000002    // file is filtered out based on the panel filter
#define HIDDEN_REASON_HIDECMD 0x00000004   // name was hidden using Hide Selected/Unselected Names

// bit field of drive letters 'a' .. 'z'
#define DRIVES_MASK 0x03FFFFFF

//
// ****************************************************************************

// Windows XP, Windows 2003.NET, Vista, Windows 7, Windows 8, Windows 8.1, Windows 10
extern BOOL WindowsXP64AndLater;  // Windows XP64 or later
extern BOOL WindowsVistaAndLater; // Windows Vista or later
extern BOOL Windows7AndLater;     // Windows 7 or later
extern BOOL Windows8AndLater;     // Windows 8 or later
extern BOOL Windows8_1AndLater;   // Windows 8.1 or later
extern BOOL Windows10AndLater;    // Windows 10 or later

extern BOOL Windows64Bit; // x64 version of Windows

extern BOOL RunningAsAdmin; // TRUE when Salamander runs "As Administrator"

extern DWORD CCVerMajor; // version of the common controls DLL
extern DWORD CCVerMinor;

extern const char* SALAMANDER_TEXT_VERSION; // textual application label including the version

extern const char *LOW_MEMORY,
    *MAINWINDOW_NAME,
    *CMAINWINDOW_CLASSNAME,
    *CFILESBOX_CLASSNAME,
    *SAVEBITS_CLASSNAME,
    *SHELLEXECUTE_CLASSNAME;

extern const char* STR_NONE; // "(none)" - plugins: used for DLLName and Version when they cannot be determined

extern char DefaultDir['z' - 'a' + 1][MAX_PATH]; // target path when changing drives

extern int MyTimeCounter;                   // increment after each use!
extern CRITICAL_SECTION TimeCounterSection; // used to synchronize access to the above

extern HINSTANCE NtDLL;               // handle to ntdll.dll
extern HINSTANCE Shell32DLL;          // handle to shell32.dll (icons)
extern HINSTANCE ImageResDLL;         // handle to imageres.dll (icons - Vista+)
extern HINSTANCE User32DLL;           // handle to user32.dll (DisableProcessWindowsGhosting)
extern HINSTANCE HLanguage;           // handle to language-specific resources (path: Configuration.LoadedSLGName)
extern char CurrentHelpDir[MAX_PATH]; // after the first use of help this holds the path to the help directory (location of all .chm files)
extern WORD LanguageID;               // language ID of the language-specific resources (.SLG file)

extern BOOL UseCustomPanelFont; // if TRUE, Font and FontUL are based on LogFont; otherwise on the system font (default)
extern HFONT Font;              // panel font
extern HFONT FontUL;            // underlined version
extern int FontCharHeight;      // font height
extern LOGFONT LogFont;         // structure describing the panel font

BOOL CreatePanelFont(); // fills Font, FontUL and FontCharHeight based on LogFont

extern HFONT EnvFont;         // environment font (edit, toolbar, header, status)
extern HFONT EnvFontUL;       // underlined list box font
extern int EnvFontCharHeight; // font height
extern HFONT TooltipFont;     // font for tooltips (and status bars, although we don't use it there)

BOOL GetSystemGUIFont(LOGFONT* lf); // retrieves the font used for the main Salamander window
BOOL CreateEnvFonts();              // fills EnvFont, EnvFontUL, EnvFontCharHeight and TooltipFont based on system metrics

extern DWORD MouseHoverTime; // how long before highlighting occurs

extern HBRUSH HNormalBkBrush;        // background of a regular panel item
extern HBRUSH HFocusedBkBrush;       // background of the focused panel item
extern HBRUSH HSelectedBkBrush;      // background of the selected panel item
extern HBRUSH HFocSelBkBrush;        // background of a focused and selected item
extern HBRUSH HDialogBrush;          // dialog background fill
extern HBRUSH HButtonTextBrush;      // button text
extern HBRUSH HDitherBrush;          // 1-bit checkerboard pattern; the color can be set via SetTextColor/SetBkColor
extern HBRUSH HActiveCaptionBrush;   // background of the active panel title
extern HBRUSH HInactiveCaptionBrush; // background of the inactive panel title

extern HBRUSH HMenuSelectedBkBrush;
extern HBRUSH HMenuSelectedTextBrush;
extern HBRUSH HMenuHilightBrush;
extern HBRUSH HMenuGrayTextBrush;

extern HACCEL AccelTable1; // accelerators in panels and the command line
extern HACCEL AccelTable2; // accelerators in the command line

extern int SystemDPI;

enum CIconSizeEnum
{
    ICONSIZE_16,   // 16x16 @ 100%DPI, 20x20 @ 125%DPI, 24x24 @ 150%DPI, ...
    ICONSIZE_32,   // 32x32 @ 100%DPI, ...
    ICONSIZE_48,   // 48x48 @ 100%DPI, ...
    ICONSIZE_COUNT // item count
};

extern int IconSizes[ICONSIZE_COUNT]; // icon sizes: 16, 32, 48
extern int IconLRFlags;               // controls the color depth of loaded icons

// NOTE: on Vista a 48x48 icon uses overlay ICONSIZE_32 and thumbnails use overlay ICONSIZE_48
extern HICON HSharedOverlays[ICONSIZE_COUNT];   // shared (hand) overlay in all sizes
extern HICON HShortcutOverlays[ICONSIZE_COUNT]; // shortcut (lower left corner) overlay in all sizes
extern HICON HSlowFileOverlays[ICONSIZE_COUNT]; // slow file overlays

extern CIconList* SimpleIconLists[ICONSIZE_COUNT]; // simple icons in all sizes

#define THROBBER_WIDTH 12 // dimensions of one frame
#define THROBBER_HEIGHT 12
#define THROBBER_COUNT 36     // total number of frames
#define IDT_THROBBER_DELAY 30 // delay [ms] for one frame of the animation
extern CIconList* ThrobberFrames;

#define LOCK_WIDTH 8 // size of one frame
#define LOCK_HEIGHT 13
extern CIconList* LockFrames;

extern HICON HGroupIcon;   // group icon for UserMenu pop-ups
extern HICON HFavoritIcon; // hot path icon

#define TILE_LEFT_MARGIN 4 // number of pixels to the left of the icon

extern RGBQUAD ColorTable[256]; // palette used for all toolbars (including plugins)

// individual indices in the SymbolsImageList and LargeSymbolsImageList image lists
enum CSymbolsImageListIndexes
{
    symbolsExecutable,    // 0: exe/bat/pif/com
    symbolsDirectory,     // 1: dir
    symbolsNonAssociated, // 2: non-associated file
    symbolsAssociated,    // 3: associated file
    symbolsUpDir,         // 4: parent directory ".."
    symbolsArchive,       // 5: archive
    symbolsCount          // TERMINATOR
};

extern HIMAGELIST HFindSymbolsImageList; // symbols for Find
extern HIMAGELIST HMenuMarkImageList;    // check marks for menus
extern HIMAGELIST HGrayToolBarImageList; // toolbar and menu in a gray variant (generated from the colored one)
extern HIMAGELIST HHotToolBarImageList;  // toolbar and menu in color
extern HIMAGELIST HBottomTBImageList;    // bottom toolbar (F1 - F12)
extern HIMAGELIST HHotBottomTBImageList; // bottom toolbar (F1 - F12)

extern HPEN HActiveNormalPen; // pens for the rectangle around an item
extern HPEN HActiveSelectedPen;
extern HPEN HInactiveNormalPen;
extern HPEN HInactiveSelectedPen;

extern HPEN HThumbnailNormalPen; // pens for the rectangle around a thumbnail
extern HPEN HThumbnailFucsedPen;
extern HPEN HThumbnailSelectedPen;
extern HPEN HThumbnailFocSelPen;

extern HPEN BtnShadowPen;
extern HPEN BtnHilightPen;
extern HPEN Btn3DLightPen;
extern HPEN BtnFacePen;
extern HPEN WndFramePen;
extern HPEN WndPen;

extern HBITMAP HFilter; // bitmap - the panel hides some files or directories

extern HBITMAP HHeaderSort; // arrows for HeaderLine

extern CBitmap ItemBitmap; // helper for various things: drawing items in the panel, header line, etc.

extern HBITMAP HUpDownBitmap; // arrows for scrolling inside short popup menus
extern HBITMAP HZoomBitmap;   // panel zoom bitmap

//extern HBITMAP HWorkerBitmap;

extern HCURSOR HHelpCursor; // context help cursor - loaded only when needed

#define THUMBNAIL_SIZE_DEFAULT 94 // according to Windows XP
#define THUMBNAIL_SIZE_MIN 48     // to support less than 48 we would need to show smaller icons
#define THUMBNAIL_SIZE_MAX 1000

extern BOOL DragFullWindows; // if TRUE the panel is resized in real time, otherwise only after release (optimization for remote desktop)

// CConfiguration::SizeFormat (the Size column format in panels)
// WARNING! Do not change the constants; they are exported to plugins via SALCFG_SIZEFORMAT
#define SIZE_FORMAT_BYTES 0 // in bytes (Open Salamander)
#define SIZE_FORMAT_KB 1    // in KB (Windows Explorer)
#define SIZE_FORMAT_MIXED 2 // bytes, KB, MB, GB, ...

// names of registry keys
extern const char* SALAMANDER_ROOT_REG;
extern const char* SALAMANDER_SAVE_IN_PROGRESS;
extern const char* SALAMANDER_COPY_IS_OK;
extern const char* SALAMANDER_AUTO_IMPORT_CONFIG;
extern const char* SALAMANDER_CONFIG_REG;
extern const char* SALAMANDER_VERSION_REG;
extern const char* SALAMANDER_VERSIONREG_REG;
extern const char* CONFIG_ONLYONEINSTANCE_REG;
extern const char* CONFIG_LANGUAGE_REG;
extern const char* CONFIG_ALTLANGFORPLUGINS_REG;
extern const char* CONFIG_LANGUAGECHANGED_REG;
extern const char* CONFIG_USEALTLANGFORPLUGINS_REG;
extern const char* CONFIG_STATUSAREA_REG;
extern const char* CONFIG_SHOWSPLASHSCREEN_REG;
extern const char* CONFIG_ENABLECUSTICOVRLS_REG;
extern const char* CONFIG_DISABLEDCUSTICOVRLS_REG;
extern const char* VIEWERS_MASKS_REG;
extern const char* VIEWERS_COMMAND_REG;
extern const char* VIEWERS_ARGUMENTS_REG;
extern const char* VIEWERS_INITDIR_REG;
extern const char* VIEWERS_TYPE_REG;
extern const char* EDITORS_MASKS_REG;
extern const char* EDITORS_COMMAND_REG;
extern const char* EDITORS_ARGUMENTS_REG;
extern const char* EDITORS_INITDIR_REG;
extern const char* SALAMANDER_PLUGINSCONFIG;
extern const char* SALAMANDER_PLUGINS_NAME;
extern const char* SALAMANDER_PLUGINS_DLLNAME;
extern const char* SALAMANDER_PLUGINS_VERSION;
extern const char* SALAMANDER_PLUGINS_COPYRIGHT;
extern const char* SALAMANDER_PLUGINS_EXTENSIONS;
extern const char* SALAMANDER_PLUGINS_DESCRIPTION;
extern const char* SALAMANDER_PLUGINS_LASTSLGNAME;
extern const char* SALAMANDER_PLUGINS_HOMEPAGE;
//extern const char *SALAMANDER_PLUGINS_PLGICONS;
extern const char* SALAMANDER_PLUGINS_PLGICONLIST;
extern const char* SALAMANDER_PLUGINS_PLGICONINDEX;
extern const char* SALAMANDER_PLUGINS_PLGSUBMENUICONINDEX;
extern const char* SALAMANDER_PLUGINS_SUBMENUINPLUGINSBAR;
extern const char* SALAMANDER_PLUGINS_THUMBMASKS;
extern const char* SALAMANDER_PLUGINS_REGKEYNAME;
extern const char* SALAMANDER_PLUGINS_FSNAME;
extern const char* SALAMANDER_PLUGINS_FUNCTIONS;
extern const char* SALAMANDER_PLUGINS_LOADONSTART;
extern const char* SALAMANDER_PLUGINS_MENU;
extern const char* SALAMANDER_PLUGINS_MENUITEMNAME;
extern const char* SALAMANDER_PLUGINS_MENUITEMHOTKEY;
extern const char* SALAMANDER_PLUGINS_MENUITEMSTATE;
extern const char* SALAMANDER_PLUGINS_MENUITEMID;
extern const char* SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL;
extern const char* SALAMANDER_PLUGINS_MENUITEMICONINDEX;
extern const char* SALAMANDER_PLUGINS_MENUITEMTYPE;
extern const char* SALAMANDER_PLUGINS_FSCMDNAME;
extern const char* SALAMANDER_PLUGINS_FSCMDICON;
extern const char* SALAMANDER_PLUGINS_FSCMDVISIBLE;
extern const char* SALAMANDER_PLUGINSORDER_SHOW;
extern const char* SALAMANDER_PLUGINS_ISNETHOOD;
extern const char* SALAMANDER_PLUGINS_USESPASSWDMAN;

// the following eight strings are only for loading configuration version 6 and
// older; newer versions already use SALAMANDER_PLUGINS_FUNCTIONS (stored in bits
// of a DWORD mask of functions)
extern const char* SALAMANDER_PLUGINS_PANELVIEW;
extern const char* SALAMANDER_PLUGINS_PANELEDIT;
extern const char* SALAMANDER_PLUGINS_CUSTPACK;
extern const char* SALAMANDER_PLUGINS_CUSTUNPACK;
extern const char* SALAMANDER_PLUGINS_CONFIG;
extern const char* SALAMANDER_PLUGINS_LOADSAVE;
extern const char* SALAMANDER_PLUGINS_VIEWER;
extern const char* SALAMANDER_PLUGINS_FS;

// clipboard format for SalIDataObject (marks our IDataObject on the clipboard)
extern const char* SALCF_IDATAOBJECT;
// clipboard format for CFakeDragDropDataObject specifying the path that should
// appear after drop in the directory line or command line and blocking drop to
// the user menu toolbar; if multiple folders/files are dragged the path is an
// empty string (drop to directory/command line is not possible)
extern const char* SALCF_FAKE_REALPATH;
// clipboard format for CFakeDragDropDataObject specifying the source type
// (1=archive, 2=FS)
extern const char* SALCF_FAKE_SRCTYPE;
// clipboard format for CFakeDragDropDataObject (only when the source is FS:
// source FS path)
extern const char* SALCF_FAKE_SRCFSPATH;

// variables for CanChangeDirectory() and AllowChangeDirectory()
extern int ChangeDirectoryAllowed; // 0 means the directory can be changed
extern BOOL ChangeDirectoryRequest;
// function handling the automatic switch of the current directory to the system
// one-used when other software unmaps drives or deletes directories shown by
// Salamander
BOOL CanChangeDirectory();
void AllowChangeDirectory(BOOL allow);

// variable for BeginStopRefresh() and EndStopRefresh()
extern int StopRefresh;
// after calling no directory refresh will occur
void BeginStopRefresh(BOOL debugSkipOneCaller = FALSE, BOOL debugDoNotTestCaller = FALSE);
// releases refreshing -> possibly posts WM_USER_SM_END_NOTIFY to the main window
// so missed refreshes are processed
void EndStopRefresh(BOOL postRefresh = TRUE, BOOL debugSkipOneCaller = FALSE, BOOL debugDoNotTestCaller = FALSE);

// variable checked in the main message loop during the "idle" part; if TRUE, it unloads
// plugins with ShouldUnload==TRUE, rebuilds menus for plugins with ShouldRebuildMenu==TRUE,
// and runs commands posted from plugins plus requested Salamander commands
extern BOOL ExecCmdsOrUnloadMarkedPlugins;

// variable checked in the main message loop during the "idle" part; if TRUE, it opens the Pack/Unpack
// dialog for plugins with OpenPackDlg==TRUE or OpenUnpackDlg==TRUE
extern BOOL OpenPackOrUnpackDlgForMarkedPlugins;

// variable for BeginStopIconRepaint() and EndStopIconRepaint()
extern int StopIconRepaint;
extern BOOL PostAllIconsRepaint;
// after calling no icon refresh in the panels takes place
void BeginStopIconRepaint();
// releases repaint -> optionally posts WM_USER_REPAINTALLICONS to the main
// window (refresh of all icons)
void EndStopIconRepaint(BOOL postRepaint = TRUE);

// variable for BeginStopStatusbarRepaint() and EndStopStatusbarRepaint()
extern int StopStatusbarRepaint;
extern BOOL PostStatusbarRepaint;
// after calling the throbber stops repainting
void BeginStopStatusbarRepaint();
// resumes repainting
void EndStopStatusbarRepaint();

// in module msgbox.cpp - center the message box according to the parent specified by hParent
int SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
int SalMessageBoxEx(const MSGBOXEX_PARAMS* params);

// draws icons from the image list with the specified styles
#define IMAGE_STATE_FOCUSED 0x00000001
#define IMAGE_STATE_SELECTED 0x00000002
#define IMAGE_STATE_HIDDEN 0x00000004
#define IMAGE_STATE_SHARED 0x00000100
#define IMAGE_STATE_SHORTCUT 0x00000200
#define IMAGE_STATE_MASK 0x00000400
#define IMAGE_STATE_OFFLINE 0x00000800
BOOL StateImageList_Draw(CIconList* iconList, int imageIndex, HDC hDC, int xDst, int yDst,
                         DWORD state, CIconSizeEnum iconSize, DWORD iconOverlayIndex,
                         const RECT* overlayRect, BOOL overlayOnly, BOOL iconOverlayFromPlugin,
                         int pluginIconOverlaysCount, HICON* pluginIconOverlays);
DWORD GetImageListColorFlags(); // returns the ILC_COLOR* flag for the current Windows version; tuned for using image lists in list views

// The GetOpenFileName/GetSaveFileName APIs return FALSE and set
// CommDlgExtendedError() to FNERR_INVALIDFILENAME when the file path in
// OPENFILENAME::lpstrFile does not exist (or contains e.g. C:\). To handle this
// case we introduce "safe" versions that detect the problem and try to open the
// dialog for Documents or Desktop instead.
BOOL SafeGetOpenFileName(LPOPENFILENAME lpofn);
BOOL SafeGetSaveFileName(LPOPENFILENAME lpofn);

extern char DecimalSeparator[5]; // characters (max. 4) obtained from the system
extern int DecimalSeparatorLen;  // length in characters without the terminating null
extern char ThousandsSeparator[5];
extern int ThousandsSeparatorLen;

extern DWORD SalamanderStartTime;     // Salamander start time (GetTickCount)
extern DWORD SalamanderExceptionTime; // time of the exception in Salamander (GetTickCount) or the last Bug Report dialog time

extern BOOL SkipOneActivateRefresh; // should refresh be skipped when the main window is activated? (for internal viewers)

extern int MenuNewExceptionHasOccured; // has the New menu crashed already? (maybe overwrote memory somewhere)
extern int FGIExceptionHasOccured;     // has SHGetFileInfo crashed?
extern int ICExceptionHasOccured;      // has InvokeCommand crashed?
extern int QCMExceptionHasOccured;     // has QueryContextMenu crashed?
extern int OCUExceptionHasOccured;     // has OleUninitialize or CoUninitialize failed?
extern int GTDExceptionHasOccured;     // has GetTargetDirectory crashed?
extern int SHLExceptionHasOccured;     // has something from ShellLib crashed?
extern int RelExceptionHasOccured;     // has any IUnknown::Release() call crashed?

extern BOOL SalamanderBusy;          // is Salamander busy?
extern DWORD LastSalamanderIdleTime; // GetTickCount() from the moment SalamanderBusy last changed to TRUE

extern int PasteLinkIsRunning; // when greater than zero a Paste Shortcuts command is in progress in one of the panels

extern BOOL CannotCloseSalMainWnd; // TRUE = the main window must not be closed

extern const char* DirColumnStr;      // LoadStr(IDS_DIRCOLUMN) - used very often, cached
extern int DirColumnStrLen;           // string length
extern const char* ColExtStr;         // LoadStr(IDS_COLUMN_NAME_EXT) - used very often, cached
extern int ColExtStrLen;              // string length
extern int TextEllipsisWidth;         // width of "..." drawn with font 'Font'
extern int TextEllipsisWidthEnv;      // width of "..." drawn with font 'FontEnv'
extern const char* ProgDlgHoursStr;   // LoadStr(IDS_PROGDLGHOURS) - used very often, cached
extern const char* ProgDlgMinutesStr; // LoadStr(IDS_PROGDLGMINUTES) - used very often, cached
extern const char* ProgDlgSecsStr;    // LoadStr(IDS_PROGDLGSECS) - used very often, cached

extern char FolderTypeName[80];         // file type for all directories (obtained from the system directory)
extern int FolderTypeNameLen;           // length of FolderTypeName
extern const char* UpDirTypeName;       // LoadStr(IDS_UPDIRTYPENAME) - used very often, cached
extern int UpDirTypeNameLen;            // string length
extern const char* CommonFileTypeName;  // LoadStr(IDS_COMMONFILETYPE) - used very often, cached
extern int CommonFileTypeNameLen;       // length of CommonFileTypeName
extern const char* CommonFileTypeName2; // LoadStr(IDS_COMMONFILETYPE2) - used very often, cached

extern char WindowsDirectory[MAX_PATH]; // cached result of GetWindowsDirectory

//#ifdef MSVC_RUNTIME_CHECKS
#define RTC_ERROR_DESCRIPTION_SIZE 2000 // buffer for the run-time check error description
extern char RTCErrorDescription[RTC_ERROR_DESCRIPTION_SIZE];
//#endif // MSVC_RUNTIME_CHECKS

// path where the bug report and minidump are created: before Vista, next to
// salamand.exe; in Vista and later, in CSIDL_APPDATA + "\\Open Salamander"
extern char BugReportPath[MAX_PATH];

// name of the file that will be imported into the registry if it exists
extern char ConfigurationName[MAX_PATH];
extern BOOL ConfigurationNameIgnoreIfNotExists;

extern HWND PluginProgressDialog; // if a plugin opens a progress dialog this is its HWND, otherwise NULL
extern HWND PluginMsgBoxParent;   // parent for plugin message boxes (main window, Plugins dialog, etc.)

extern BOOL CriticalShutdown; // TRUE = "critical shutdown" in progress; no time to ask, exiting quickly, 5s until kill

// "translation" of POSIX names to MS equivalents
#define itoa _itoa
#define stricmp _stricmp
#define strnicmp _strnicmp

// skill levels
#define SKILL_LEVEL_BEGINNER 0
#define SKILL_LEVEL_INTERMEDIATE 1
#define SKILL_LEVEL_ADVANCED 2

// converts the CConfiguration::SkillLevel variable to the menu SkillLevel
DWORD CfgSkillLevelToMenu(BYTE cfgSkillLevel);

// Attributes shown in the panel which must also be masked when comparing directories.
// NOTE: FILE_ATTRIBUTE_DIRECTORY is not displayed as an attribute so it has no place in the mask.
#define DISPLAYED_ATTRIBUTES (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | \
                              FILE_ATTRIBUTE_SYSTEM | \
                              FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_ENCRYPTED | \
                              FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_COMPRESSED | \
                              FILE_ATTRIBUTE_OFFLINE)

// timers
#define IDT_SCROLL 930
#define IDT_REPAINT 931
#define IDT_DRAGDROPTESTAGAIN 932
#define IDT_PANELSCROLL 933
#define IDT_SINGLECLICKSELECT 934
#define IDT_FLASHICON 935
#define IDT_QUICKRENAMEBEGIN 936
#define IDT_PLUGINFSTIMERS 937
#define IDT_EDITLB 938
#define IDT_PROGRESSSELFMOVE 939
#define IDT_DELETEMNGR_PROCESS 940
#define IDT_ADDNEWMODULES 941
#define IDT_POSTENDSUSPMODE 942
#define IDT_ASSOCIATIONSCHNG 943
#define IDT_SM_END_NOTIFY 944
#define IDT_REFRESH_DIR_EX 945
#define IDT_UPDATESTATUS 946
#define IDT_ICONOVRREFRESH 947
#define IDT_INACTIVEREFRESH 948
#define IDT_THROBBER 949
#define IDT_DELAYEDTHROBBER 950
#define IDT_UPDATETASKLIST 951

// NOTE: nearly all functions in this section display LOAD/SAVE configuration
//       error messages on failure, which makes them unsuitable for general
//       registry access. See the functions at the beginning of regwork.h:
//       OpenKeyAux, CreateKeyAux, etc.
BOOL ClearKey(HKEY key);
BOOL CreateKey(HKEY hKey, const char* name, HKEY& createdKey);
BOOL OpenKey(HKEY hKey, const char* name, HKEY& openedKey);
void CloseKey(HKEY key);
BOOL DeleteKey(HKEY hKey, const char* name);
BOOL DeleteValue(HKEY hKey, const char* name);
// when dataSize is -1 the function calculates the string length using strlen
BOOL SetValue(HKEY hKey, const char* name, DWORD type,
              const void* data, DWORD dataSize);
BOOL GetValue(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize);
BOOL GetSize(HKEY hKey, const char* name, DWORD type, DWORD& bufferSize);
BOOL LoadRGB(HKEY hKey, const char* name, COLORREF& color);
BOOL SaveRGB(HKEY hKey, const char* name, COLORREF color);
BOOL LoadRGBF(HKEY hKey, const char* name, SALCOLOR& color);
BOOL SaveRGBF(HKEY hKey, const char* name, SALCOLOR color);
BOOL LoadLogFont(HKEY hKey, const char* name, LOGFONT* logFont);
BOOL SaveLogFont(HKEY hKey, const char* name, LOGFONT* logFont);
BOOL LoadHistory(HKEY hKey, const char* name, char* history[], int maxCount);
BOOL SaveHistory(HKEY hKey, const char* name, char* history[], int maxCount, BOOL onlyClear = FALSE);
BOOL LoadViewers(HKEY hKey, const char* name, CViewerMasks* viewerMasks);
BOOL SaveViewers(HKEY hKey, const char* name, CViewerMasks* viewerMasks);
BOOL LoadEditors(HKEY hKey, const char* name, CEditorMasks* editorMasks);
BOOL SaveEditors(HKEY hKey, const char* name, CEditorMasks* editorMasks);

BOOL ExportConfiguration(HWND hParent, const char* fileName, BOOL clearKeyBeforeImport);
BOOL ImportConfiguration(HWND hParent, const char* fileName, BOOL ignoreIfNotExists,
                         BOOL autoImportConfig, BOOL* importCfgFromFileWasSkipped);

class CHighlightMasks;
void UpdateDefaultColors(SALCOLOR* colors, CHighlightMasks* highlightMasks, BOOL processColors, BOOL processMasks);

extern BOOL ImageDragging;                                                // an image is being dragged
extern BOOL ImageDraggingVisible;                                         // is the image currently visible?
void ImageDragBegin(int width, int height, int dxHotspot, int dyHotspot); // size of the dragged image
void ImageDragEnd();                                                      // end dragging
BOOL ImageDragInterfereRect(const RECT* rect);                            // rect is in screen coordinates, check whether the dragged item intersects it
void ImageDragEnter(int x, int y);                                        // x and y are screen coordinates
void ImageDragMove(int x, int y);                                         // x and y are screen coordinates
void ImageDragLeave();
void ImageDragShow(BOOL show); // hides/shows; does not affect ImageDragging, only ImageDraggingVisible

// sets the cursor to a hand shape
HCURSOR SetHandCursor();

//******************************************************************************
//
// CreateToolbarBitmaps
//
// IN:   hInstance       - instance containing the bitmap with resID
//       resID           - identifier of the input bitmap
//       transparent     - this color will be transparent
//       bkColorForAlpha - color showing through the alpha parts of icons (WinXP)
// OUT:  hMaskBitmap  - mask (b&w)
//       hGrayBitmap  - grayscale variant
//       hColorBitmap - color variant
//

struct CSVGIcon
{
    int ImageIndex;
    const char* SVGName;
};

BOOL CreateToolbarBitmaps(HINSTANCE hInstance, int resID, COLORREF transparent, COLORREF bkColorForAlpha,
                          HBITMAP& hMaskBitmap, HBITMAP& hGrayBitmap, HBITMAP& hColorBitmap, BOOL appendIcons,
                          const CSVGIcon* svgIcons, int svgIconsCount);

//****************************************************************************
//
// CreateGrayscaleAndMaskBitmaps
//
// Creates a new 24-bit bitmap, copies the source bitmap into it and converts it to grayscale.
// At the same time prepares another bitmap with a mask based on the transparent color.
//

BOOL CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                   HBITMAP& hGrayscale, HBITMAP& hMask);

//******************************************************************************
//
// UpdateCrc32
//   Updates CRC-32 (32-bit Cyclic Redundancy Check) with specified array of bytes.
//
// Parameters
//   'buffer'
//      [in] Pointer to the starting address of the block of memory to update 'crcVal' with.
//
//   'count'
//      [in] Size, in bytes, of the block of memory to update 'crcVal' with.
//
//   'crcVal'
//      [in] Initial crc value. Set this value to zero to calculate CRC-32 of the 'buffer'.
//
// Return Values
//   Returns updated CRC-32 value.
//

DWORD UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal);

//******************************************************************************
//
// Idle processing control (CMainWindow::OnEnterIdle)
//
// variables are global for easy access and not attributes of CMainWindow
//

extern BOOL IdleRefreshStates;  // when set, the next CMainWindow::OnEnterIdle will update command states (toolbar, menu)
extern BOOL IdleForceRefresh;   // if IdleRefreshStates is set, setting IdleForceRefresh bypasses Salamander's cache
extern BOOL IdleCheckClipboard; // when IdleRefreshStates is TRUE and this flag is set, the clipboard is checked as well (time consuming)

// ".." is not counted among files/directories
extern DWORD EnablerUpDir;                // is a parent directory available?
extern DWORD EnablerRootDir;              // are we already at the root? (note: UNC roots have an updir but are still roots)
extern DWORD EnablerForward;              // is forward available in history?
extern DWORD EnablerBackward;             // is backward available in history?
extern DWORD EnablerFileOnDisk;           // focus is on a file and the panel is disk-based
extern DWORD EnablerFileOnDiskOrArchive;  // focus is on a file and the panel is disk or archive
extern DWORD EnablerFileOrDirLinkOnDisk;  // focus is on a file or directory link and the panel is disk
extern DWORD EnablerFiles;                // focus/selection is on files/directories
extern DWORD EnablerFilesOnDisk;          // focus/selection is on files/directories and the panel is a disk
extern DWORD EnablerFilesOnDiskCompress;  // focus/selection is on files/directories and the panel is a disk that supports compression
extern DWORD EnablerFilesOnDiskEncrypt;   // focus/selection is on files/directories and the panel is a disk that supports encryption
extern DWORD EnablerFilesOnDiskOrArchive; // focus/selection is on files/directories and the panel is disk or archive
extern DWORD EnablerOccupiedSpace;        // panel is disk or archive with VALID_DATA_SIZE and EnablerFilesOnDiskOrArchive holds
extern DWORD EnablerFilesCopy;            // focus/selection is on files/directories and the panel is disk, archive or FS supporting "copy from fs"
extern DWORD EnablerFilesMove;            // focus/selection is on files/directories and the panel is disk or FS supporting "move from fs"
extern DWORD EnablerFilesDelete;          // focus/selection is on files/directories and the panel is a disk, an editable archive, or an FS supporting "delete"
extern DWORD EnablerFileDir;              // focus is on a file/directory
extern DWORD EnablerFileDirANDSelected;   // focus is on a file/directory and some files/directories are selected
extern DWORD EnablerQuickRename;          // focus is on a file/directory and the panel is disk or FS (with quick-rename support)
extern DWORD EnablerOnDisk;               // panel is a disk
extern DWORD EnablerCalcDirSizes;         // panel is a disk or archive with VALID_DATA_SIZE
extern DWORD EnablerPasteFiles;           // can Paste be performed? (files on the clipboard) used as memory of the last clipboard state for 'pasteFiles' in CMainWindow::RefreshCommandStates()
extern DWORD EnablerPastePath;            // can Paste be performed? (path text on the clipboard) used as memory of the last clipboard state for 'pastePath' in CMainWindow::RefreshCommandStates()
extern DWORD EnablerPasteLinks;           // can Paste Links be performed? (files copied to the clipboard) used as memory of the last clipboard state for 'pasteLinks' in CMainWindow::RefreshCommandStates()
extern DWORD EnablerPasteSimpleFiles;     // are there files/directories from a single path on the clipboard? (chance to Paste into an archive or FS)
extern DWORD EnablerPasteDefEffect;       // what is the default paste effect; may be a combination of DROPEFFECT_COPY+DROPEFFECT_MOVE (Copy or Cut?)
extern DWORD EnablerPasteFilesToArcOrFS;  // can files be pasted into the archive/FS in the current panel? (the panel is archive/FS && EnablerPasteSimpleFiles && the operation according to EnablerPasteDefEffect is allowed)
extern DWORD EnablerPaste;                // can Paste be performed? (files on clipboard && panel is disk || paste into archive/FS is possible || path text on clipboard)
extern DWORD EnablerPasteLinksOnDisk;     // can Paste Links be performed and the panel is a disk?
extern DWORD EnablerSelected;             // are any files/directories selected
extern DWORD EnablerUnselected;           // is there at least one unselected file/directory (UpDir ".." not considered)
extern DWORD EnablerHiddenNames;          // the HiddenNames array contains some names
extern DWORD EnablerSelectionStored;      // is a selection stored in OldSelection of the active panel?
extern DWORD EnablerGlobalSelStored;      // is a selection stored in GlobalSelection?
extern DWORD EnablerSelGotoPrev;          // is there a selected item before the focus?
extern DWORD EnablerSelGotoNext;          // is there a selected item after the focus?
extern DWORD EnablerLeftUpDir;            // does the left panel have a parent directory?
extern DWORD EnablerRightUpDir;           // does the right panel have a parent directory?
extern DWORD EnablerLeftRootDir;          // are we not yet at the root in the left panel? (UNC roots have an updir but are still roots)
extern DWORD EnablerRightRootDir;         // are we not yet at the root in the right panel? (UNC roots have an updir but are still roots)
extern DWORD EnablerLeftForward;          // is forward available in the left panel history?
extern DWORD EnablerRightForward;         // is forward available in the right panel history?
extern DWORD EnablerLeftBackward;         // is backward available in the left panel history?
extern DWORD EnablerRightBackward;        // is backward available in the right panel history?
extern DWORD EnablerFileHistory;          // is a file available in the view/edit history?
extern DWORD EnablerDirHistory;           // is a directory available in the directory history?
extern DWORD EnablerCustomizeLeftView;    // can columns be configured for the left view?
extern DWORD EnablerCustomizeRightView;   // can columns be configured for the right view?
extern DWORD EnablerDriveInfo;            // can Drive Info be displayed?
extern DWORD EnablerCreateDir;            // panel is disk or FS (supports create-dir)
extern DWORD EnablerViewFile;             // focus is on a file and the panel is disk, archive or FS (supports view-file)
extern DWORD EnablerChangeAttrs;          // focus/selection is on files/directories and the panel is disk or FS (supports change-attributes)
extern DWORD EnablerShowProperties;       // focus/selection is on files/directories and the panel is disk or FS (supports show-properties)
extern DWORD EnablerItemsContextMenu;     // focus/selection is on files/directories and the panel is disk or FS (supports context menu)
extern DWORD EnablerOpenActiveFolder;     // panel is a disk or an FS (with open-active-folder support)
extern DWORD EnablerPermissions;          // focus/selection is on files/directories and the panel is a disk; running at least on W2K with NTFS supporting ACLs

//******************************************************************************
//
// ToolBar Bitmap indexes
//
// Items can be added to the array.
// The array is divided into two parts. The first contains indexes with real
// images in the bitmap. After that come indexes with icons pulled from
// shell32.dll. These two groups must always be contiguous and the indexes cannot
// be mixed.
//

#define IDX_TB_CONNECTNET 0    // Connect Network Drive
#define IDX_TB_DISCONNECTNET 1 // Disconnect Network Drive
#define IDX_TB_SHARED_DIRS 2   // Shared Directories
#define IDX_TB_CHANGE_DIR 3    // Change Directory
#define IDX_TB_CREATEDIR 4     // Create Directory
#define IDX_TB_NEW 5           // New
#define IDX_TB_FINDFILE 6      // Find Files
#define IDX_TB_PREV_SELECTED 7 // Previous Selected Item
#define IDX_TB_NEXT_SELECTED 8 // Next Selected Item
#define IDX_TB_SORTBYNAME 9    // Sort by Name
#define IDX_TB_SORTBYTYPE 10   // Sort by Type
#define IDX_TB_SORTBYSIZE 11   // Sort by Size
#define IDX_TB_SORTBYDATE 12   // Sort by Date
#define IDX_TB_PARENTDIR 13    // Parent Directory
#define IDX_TB_ROOTDIR 14      // Root Directory
#define IDX_TB_FILTER 15       // Filter
#define IDX_TB_BACK 16         // Back
#define IDX_TB_FORWARD 17      // Forward
#define IDX_TB_REFRESH 18      // Refresh
#define IDX_TB_SWAPPANELS 19   // Swap Panels
#define IDX_TB_CHANGEATTR 20   // Change Attributes
#define IDX_TB_USERMENU 21     // User Menu
#define IDX_TB_COMMANDSHELL 22 // Command Shell
#define IDX_TB_COPY 23         // Copy
#define IDX_TB_MOVE 24         // Move
#define IDX_TB_DELETE 25       // Delete
// 1x not used
#define IDX_TB_COMPRESS 27       // Compress
#define IDX_TB_UNCOMPRESS 28     // UnCompress
#define IDX_TB_QUICKRENAME 29    // Quick Rename
#define IDX_TB_CHANGECASE 30     // Change Case
#define IDX_TB_VIEW 31           // View
#define IDX_TB_CLIPBOARDCUT 32   // Clipboard Cut
#define IDX_TB_CLIPBOARDCOPY 33  // Clipboard Copy
#define IDX_TB_CLIPBOARDPASTE 34 // Clipboard Paste
#define IDX_TB_PERMISSIONS 35    // Permissions
#define IDX_TB_PROPERTIES 36     // Properties
#define IDX_TB_COMPAREDIR 37     // Comapare Directories
#define IDX_TB_DRIVEINFO 38      // Drive Information
#define IDX_TB_RESELECT 39       // Reselect
#define IDX_TB_HELP 40           // Help
#define IDX_TB_CONTEXTHELP 41    // Context Help
// 1x not used
#define IDX_TB_EDIT 43              // Edit
#define IDX_TB_SORTBYEXT 44         // Sort by Extension
#define IDX_TB_SELECT 45            // Select
#define IDX_TB_UNSELECT 46          // Unselect
#define IDX_TB_INVERTSEL 47         // Invert selection
#define IDX_TB_SELECTALL 48         // Select all
#define IDX_TB_PACK 49              // Pack
#define IDX_TB_UNPACK 50            // UnPack
#define IDX_TB_CONVERT 51           // Convert
#define IDX_TB_UNSELECTALL 52       // Unselect all
#define IDX_TB_VIEW_MODE 53         // View Mode
#define IDX_TB_HOTPATHS 54          // Hot Paths
#define IDX_TB_FOCUS 55             // Focus (green arrow)
#define IDX_TB_STOP 56              // Stop (red circle with a cross)
#define IDX_TB_EMAIL 57             // Email Files
#define IDX_TB_EDITNEW 58           // Edit New
#define IDX_TB_PASTESHORTCUT 59     // Paste Shortcut
#define IDX_TB_FOCUSSHORTCUT 60     // Focus Shortcut or Link Target
#define IDX_TB_CALCDIRSIZES 61      // Calculate Directory Sizes
#define IDX_TB_OCCUPIEDSPACE 62     // Calculate Occupied Space
#define IDX_TB_SAVESELECTION 63     // Save Selection
#define IDX_TB_LOADSELECTION 64     // Load Selection
#define IDX_TB_SEL_BY_EXT 65        // Select Files With Same Extension
#define IDX_TB_UNSEL_BY_EXT 66      // Unselect Files With Same Extension
#define IDX_TB_SEL_BY_NAME 67       // Select Files With Same Name
#define IDX_TB_UNSEL_BY_NAME 68     // Unselect Files With Same Name
#define IDX_TB_OPEN_FOLDER 69       // Open Folder
#define IDX_TB_CONFIGURARTION 70    // Configuration
#define IDX_TB_OPEN_IN_OTHER_ACT 71 // Focus Name in Other Panel
#define IDX_TB_OPEN_IN_OTHER 72     // Open Name in Other Panel
#define IDX_TB_AS_OTHER_PANEL 73    // Go To Path From Other Panel
#define IDX_TB_HIDE_UNSELECTED 74   // Hide Unselected Names
#define IDX_TB_HIDE_SELECTED 75     // Hide Selected Names
#define IDX_TB_SHOW_ALL 76          // Show All Names
#define IDX_TB_SMART_COLUMN_MODE 77 // Smart Column Mode

#define IDX_TB_FD 78 // first "dynamically added" index
// the following icons will be added to the bitmap dynamically
// and some will be loaded from shell32.dll

#define IDX_TB_CHANGEDRIVEL IDX_TB_FD + 0 // Change Drive Left
#define IDX_TB_CHANGEDRIVER IDX_TB_FD + 1 // Change Drive Right
#define IDX_TB_OPENACTIVE IDX_TB_FD + 2   // Open Active Folder
#define IDX_TB_OPENDESKTOP IDX_TB_FD + 3  // Open Desktop
#define IDX_TB_OPENMYCOMP IDX_TB_FD + 4   // Open Computer
#define IDX_TB_OPENCONTROL IDX_TB_FD + 5  // Open Control Panel
#define IDX_TB_OPENPRINTERS IDX_TB_FD + 6 // Open Printers
#define IDX_TB_OPENNETWORK IDX_TB_FD + 7  // Open Network
#define IDX_TB_OPENRECYCLE IDX_TB_FD + 8  // Open Recycle Bin
#define IDX_TB_OPENFONTS IDX_TB_FD + 9    // Open Fonts
#define IDX_TB_OPENMYDOC IDX_TB_FD + 10   // Open Documents

#define IDX_TB_COUNT IDX_TB_FD + 11 // number of bitmaps including those pulled from shell32.dll

//******************************************************************************
//
// Custom Exceptions
//

#define OPENSAL_EXCEPTION_RTC 0xE0EA4321   // raised in the RTC callback
#define OPENSAL_EXCEPTION_BREAK 0xE0EA4322 // raised when breaking from another Salamander or via salbreak

//******************************************************************************
//
// Set of variables and functions for opening associations via SalOpen.exe
//

// shared memory
extern HANDLE SalOpenFileMapping;
extern void* SalOpenSharedMem;

// release the service
void ReleaseSalOpen();

// launch salopen.exe and pass 'fileName' via shared memory
// returns TRUE on success, otherwise FALSE (the association should be launched another way)
BOOL SalOpenExecute(HWND hWindow, const char* fileName);

//******************************************************************************

// mapping salCmd (the Salamander command number from a plugin, see SALCMD_XXX)
// to the command number for WM_COMMAND
int GetWMCommandFromSalCmd(int salCmd);

//******************************************************************************

// number of items in the SalamanderConfigurationRoots array
#define SALCFG_ROOTS_COUNT 83

// ID of the main thread (valid only after entering WinMain())
extern DWORD MainThreadID;

extern BOOL IsNotAlphaNorNum[256]; // TRUE/FALSE array for characters (TRUE = not a letter or digit)
extern BOOL IsAlpha[256];          // TRUE/FALSE array for characters (TRUE = letter)

extern int UserCharset; // user's default charset for fonts

// allocation granularity (needed when using memory-mapped files)
extern DWORD AllocationGranularity;

// should we wait for ESC to be released before starting to list the path in the panel?
extern BOOL WaitForESCReleaseBeforeTestingESC;

// returns the screen coordinates where the context menu should appear
// used when invoking the context menu via keyboard (Shift+F10 or VK_APP)
void GetListViewContextMenuPos(HWND hListView, POINT* p);

// based on the display color depth decides whether to use 256-color
// or 16-color bitmaps
BOOL Use256ColorsBitmap();

// restores focus in the source panel (used when focus disappears after disabling the main window, etc.)
void RestoreFocusInSourcePanel();

#define ISSLGINCOMPLETE_SIZE 200
extern char IsSLGIncomplete[ISSLGINCOMPLETE_SIZE];

//******************************************************************************
// enumeration of file names from panels/Find for viewers

// initialization and release of data associated with enumeration
void InitFileNamesEnumForViewers();
void ReleaseFileNamesEnumForViewers();

enum CFileNamesEnumRequestType
{
    fnertFindNext,     // look for the next file in the source
    fnertFindPrevious, // look for the previous file in the source
    fnertIsSelected,   // query whether the file is selected in the source
    fnertSetSelection, // set the selection state of the file in the source
};

struct CFileNamesEnumData
{
    // request:
    int RequestUID;                        // request ID
    CFileNamesEnumRequestType RequestType; // type of request
    int SrcUID;
    int LastFileIndex;
    char LastFileName[MAX_PATH];
    BOOL PreferSelected;
    BOOL OnlyAssociatedExtensions;
    CPluginInterfaceAbstract* Plugin; // used when 'OnlyAssociatedExtensions'==TRUE; designates which plugin to filter file names for ('Plugin'==NULL = internal viewer)
    char FileName[MAX_PATH];
    BOOL Select;
    BOOL TimedOut; // TRUE when nobody is waiting for the result anymore (searching the name would be pointless)

    // result:
    BOOL Found; // TRUE when the desired file name was found
    BOOL NoMoreFiles;
    BOOL SrcBusy;
    BOOL IsFileSelected;
};

// section for handling enumeration data (FileNamesEnumSources, FileNamesEnumData,
// FileNamesEnumDone, NextRequestUID and NextSourceUID)
extern CRITICAL_SECTION FileNamesEnumDataSect;
// structure containing the enumeration request and results
extern CFileNamesEnumData FileNamesEnumData;
// the event is signaled once the source fills FileNamesEnumData with the result
extern HANDLE FileNamesEnumDone;

#define FILENAMESENUM_TIMEOUT 1000 // timeout for delivering WM_USER_ENUMFILENAMES to the source window

// returns TRUE if the enumeration source is a panel; 'panel' then receives
// PANEL_LEFT or PANEL_RIGHT. Returns FALSE if the enumeration source was not
// found or if it is a Find window
BOOL IsFileEnumSourcePanel(int srcUID, int* panel);

// Returns the next file name for the viewer from the source (left/right panel or Find window).
// 'srcUID' is the unique identifier of the source (it is passed as a parameter when opening
// the viewer). 'lastFileIndex' (must not be NULL) is an IN/OUT parameter that the plugin should
// change only if it wants to return the first file name; in that case, set 'lastFileIndex'
// to -1. The initial value of 'lastFileIndex' is passed as a parameter when opening the
// viewer. 'lastFileName' is the full name of the current file (an empty string if it is not
// known, for example if 'lastFileIndex' is -1). If 'preferSelected' is TRUE and at least one
// name is selected, selected names are returned. If 'onlyAssociatedExtensions' is TRUE, only
// files with an extension associated with this plugin's viewer are returned (pressing F3 on that
// file would attempt to open this plugin's viewer, and possible shadowing by another plugin's
// viewer is ignored). 'fileName' is the buffer for the retrieved name (size at least
// MAX_PATH). Returns TRUE if the name is retrieved successfully. Returns FALSE on error: there
// is no next file name in the source (if 'noMoreFiles' is not NULL, TRUE is returned in it),
// the source is busy (it is not processing messages; if 'srcBusy' is not NULL, TRUE is returned
// in it), or the source no longer exists (panel path changed, sorting changed, etc.).
BOOL GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                              BOOL preferSelected, BOOL onlyAssociatedExtensions,
                              char* fileName, BOOL* noMoreFiles, BOOL* srcBusy,
                              CPluginInterfaceAbstract* plugin);

// Retrieves the previous file name for the viewer from the given source.
// Usage and return values are equivalent to GetNextFileNameForViewer but walk
// backwards through the list of files.
BOOL GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                  BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                  char* fileName, BOOL* noMoreFiles, BOOL* srcBusy,
                                  CPluginInterfaceAbstract* plugin);

// Checks whether the current viewer file is selected in the source (left/right
// panel or Find window). 'srcUID' uniquely identifies the source. 'lastFileIndex'
// should not be modified by the plugin except when requesting the last file
// (set it to -1). 'lastFileName' is the full name of the current file. Returns
// TRUE on success and stores the result in 'isFileSelected' (must not be NULL).
// Returns FALSE if the source no longer exists or the file is missing (and sets
// 'srcBusy' to FALSE if provided) or if the source is busy processing messages
// (sets 'srcBusy' to TRUE).
BOOL IsFileNameForViewerSelected(int srcUID, int lastFileIndex, const char* lastFileName,
                                 BOOL* isFileSelected, BOOL* srcBusy);

// Sets or clears the selection of the current file from the viewer in the source
// (left/right panel or Find window); 'srcUID' is the unique identifier of the source
// (passed as a parameter when opening the viewer); 'lastFileIndex' is a parameter that
// the plugin should not modify, its initial value is passed as a parameter when opening
// the viewer; 'lastFileName' is the full name of the current file; 'select' is TRUE/FALSE
// depending on whether the current file should be selected/deselected; returns TRUE on
// success; returns FALSE on error: the source no longer exists (panel path changed, etc.)
// or the file 'lastFileName' is no longer in the source (for these two errors, if
// 'srcBusy' is not NULL, FALSE is returned in it), or the source is busy (not processing
// messages; for this error, if 'srcBusy' is not NULL, TRUE is returned in it).
BOOL SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex, const char* lastFileName,
                                     BOOL select, BOOL* srcBusy);

// Changes the UID assigned to a source panel or Find window without generating
// a new one. The FileNamesEnumSources array is updated and the new UID is
// returned in 'srcUID'.
void EnumFileNamesChangeSourceUID(HWND hWnd, int* srcUID);

// Adds a source to FileNamesEnumSources without creating a new UID. The pair
// hWnd+UID is stored and the new UID is returned in 'srcUID'.
void EnumFileNamesAddSourceUID(HWND hWnd, int* srcUID);

// Removes the specified source from FileNamesEnumSources
void EnumFileNamesRemoveSourceUID(HWND hWnd);

//******************************************************************************
// non-blocking reading of the volume name of a CD drive

extern CRITICAL_SECTION ReadCDVolNameCS;   // critical section for access to data
extern UINT_PTR ReadCDVolNameReqUID;       // request UID (to detect if someone still waits for the result)
extern char ReadCDVolNameBuffer[MAX_PATH]; // IN/OUT buffer (root/volume_name)

//******************************************************************************
// functions for handling histories of recently used values in combo boxes

// adds an allocated copy of the new value 'value' to the shared history ('historyArr'+'historyItemsCount');
// if 'caseSensitiveValue' is TRUE the value is looked up in the history using a case-sensitive comparison
// (FALSE = case-insensitive); when found, the existing value is just moved to the first position
void AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                const char* value, BOOL caseSensitiveValue);

// adds the texts from the shared history ('historyArr'+'historyItemsCount') to the combo box 'combo';
// before adding it resets the combo box content (see CB_RESETCONTENT)
void LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount);

//******************************************************************************

// function for adding all yet unknown loaded modules of the process
void AddNewlyLoadedModulesToGlobalModulesStore();

//******************************************************************************

// quicksort using StrICmp for comparisons
void SortNames(char* files[], int left, int right);

// searches for the string 'name' in the 'usedNames' array (sorted via StrICmp);
// returns TRUE when found and the found index in 'index' (if not NULL); returns
// FALSE when the item is missing and provides the insertion index in 'index' (if not NULL)
BOOL ContainsString(TIndirectArray<char>* usedNames, const char* name, int* index = NULL);

//******************************************************************************

// returns TRUE on success and stores the path to "Documents" or "Desktop"
// returns FALSE on failure
// 'pathLen' specifies the size of the 'path' buffer; the function ensures the
// string is null-terminated even if truncated
BOOL GetMyDocumentsOrDesktopPath(char* path, int pathLen);

// To optimize performance, it is good practice for applications to detect whether they
// are running in a Terminal Services client session. For example, when an application
// is running on a remote session, it should eliminate unnecessary graphic effects, as
// described in Graphic Effects. If the user is running the application in a console
// session (directly on the terminal), it is not necessary for the application to
// optimize its behavior.
//
// Returns TRUE if the application is running in a remote session and FALSE if the
// application is running on the console.
BOOL IsRemoteSession(void);

// returns TRUE if the user is a member of the Administrators group
// returns FALSE on error
BOOL IsUserAdmin();

//******************************************************************************

// ensures we escape from removed drives to a fixed drive (after ejecting a device such as a USB flash disk)
extern BOOL ChangeLeftPanelToFixedWhenIdleInProgress; // TRUE when the path is currently being changed; setting ChangeLeftPanelToFixedWhenIdle to TRUE is unnecessary
extern BOOL ChangeLeftPanelToFixedWhenIdle;
extern BOOL ChangeRightPanelToFixedWhenIdleInProgress; // TRUE when the path is currently being changed; setting ChangeRightPanelToFixedWhenIdle to TRUE is unnecessary
extern BOOL ChangeRightPanelToFixedWhenIdle;
extern BOOL OpenCfgToChangeIfPathIsInaccessibleGoTo; // TRUE = open configuration on Drives during idle and focus "If path in panel is inaccessible, go to:"

// drive root (including UNC) for which the "drive not ready" message box with Retry+Cancel buttons is shown
// used for automatic Retry after inserting media into the drive
extern char CheckPathRootWithRetryMsgBox[MAX_PATH];
// dialog "drive not ready" with Retry+Cancel buttons (used for automatic Retry after
// inserting media into the drive)
extern HWND LastDriveSelectErrDlgHWnd;

// GetDriveFormFactor returns the drive form factor.
//  It returns 350 if the drive is a 3.5" floppy drive.
//  It returns 525 if the drive is a 5.25" floppy drive.
//  It returns 800 if the drive is an 8" floppy drive.
//  It returns   1 if the drive supports removable media other than 3.5", 5.25", and 8" floppies.
//  It returns   0 on error.
//  iDrive is 1 for drive A:, 2 for drive B:, etc.
DWORD GetDriveFormFactor(int iDrive);

//******************************************************************************

// sorts the array of plugins by PluginFSCreateTime (for display in Alt+F1/F2 and the Disconnect dialog)
void SortPluginFSTimes(CPluginFSInterfaceEncapsulation** list, int left, int right);

// returns the index for the item text in the Change Drive menu and Disconnect dialog;
// see CPluginFSInterfaceEncapsulation::ChngDrvDuplicateItemIndex
int GetIndexForDrvText(CPluginFSInterfaceEncapsulation** fsList, int count,
                       CPluginFSInterfaceAbstract* fsIface, int currentIndex);

//******************************************************************************

// using TweakUI users can change the shortcut icon (default, custom or none)
// this function obtains HShortcutOverlayXX and discards any existing one
BOOL GetShortcutOverlay();

// returns a textual representation of 'hotKey' (LOBYTE=vk, HIBYTE=mods); 'buff' must have at least 50 characters
void GetHotKeyText(WORD hotKey, char* buff);

// returns the display bits per pixel
int GetCurrentBPP(HDC hDC = NULL);

// iterates through parents up to the topmost one
HWND GetTopLevelParent(HWND hWindow);

//******************************************************************************

// variables used while saving the configuration during shutdown, logoff, or restart
// we must pump messages so the system does not terminate us as a "not responding" application
class CWaitWindow;
extern CWaitWindow* GlobalSaveWaitWindow; // if a global wait window for Save exists, it is stored here (otherwise NULL)
extern int GlobalSaveWaitWindowProgress;  // current progress value of the global wait window for Save

extern BOOL IsSetSALAMANDER_SAVE_IN_PROGRESS; // TRUE if the SALAMANDER_SAVE_IN_PROGRESS value exists in the registry (used to detect interrupted configuration saving)

//******************************************************************************

// helper structure and functions for opening a context menu and executing its items
// in CSalamanderGeneral::OpenNetworkContextMenu()

struct CTmpEnumData
{
    int* Indexes;
    CFilesWindow* Panel;
};

const char* EnumFileNames(int index, void* param);

void ShellActionAux5(UINT flags, CFilesWindow* panel, HMENU h);
void AuxInvokeCommand(CFilesWindow* panel, CMINVOKECOMMANDINFO* ici);
void ShellActionAux6(CFilesWindow* panel);

//******************************************************************************

// returns in 'path' (buffer at least MAX_PATH characters) the path Configuration.IfPathIsInaccessibleGoTo;
// respects the setting Configuration.IfPathIsInaccessibleGoToIsMyDocs
void GetIfPathIsInaccessibleGoTo(char* path, BOOL forceIsMyDocs = FALSE);

// loads icon overlay handler configuration from the registry
void LoadIconOvrlsInfo(const char* root);

// returns TRUE if the icon overlay handler is disabled, or if custom icon overlays are disabled globally
BOOL IsDisabledCustomIconOverlays(const char* name);

// returns TRUE if the icon overlay handler is in the list of disabled icon overlay handlers
BOOL IsNameInListOfDisabledCustomIconOverlays(const char* name);

// clears the list of disabled icon overlay handlers
void ClearListOfDisabledCustomIconOverlays();

// adds 'name' to the list of disabled icon overlay handlers
BOOL AddToListOfDisabledCustomIconOverlays(const char* name);

// loads an icon from ImageResDLL
HICON SalLoadImage(int vistaResID, int otherResID, int cx, int cy, UINT flags);

// loads an icon for archives
HICON LoadArchiveIcon(int cx, int cy, UINT flags);

// obtains login credentials for the given network path and optionally restores its mapping
BOOL RestoreNetworkConnection(HWND parent, const char* name, const char* remoteName, DWORD* retErr = NULL,
                              LPNETRESOURCE lpNetResource = NULL);

// builds the text for the Type column in the panel for an unassociated file (e.g. "AAA File" or "File")
void GetCommonFileTypeStr(char* buf, int* resLen, const char* ext);

// finds duplicate separators and removes the redundant ones (on Vista duplicate
// separators appeared in the context menu on .bar files)
void RemoveUselessSeparatorsFromMenu(HMENU h);

// returns the "Open Salamander" directory in the CSIDL_APPDATA path in 'buf' (buffer of size MAX_PATH)
BOOL GetOurPathInRoamingAPPDATA(char* buf);

// creates the "Open Salamander" directory in the CSIDL_APPDATA path; returns TRUE if the path
// fits into MAX_PATH (its existence is not guaranteed and the result of CreateDirectory is not checked);
// if 'buf' is not NULL it is a buffer of size MAX_PATH that receives this path
// NOTE: Vista+ only
BOOL CreateOurPathInRoamingAPPDATA(char* buf);

#ifndef _WIN64

// 32-bit version on Win64 only: checks whether the path is redirected by the
// file system redirector to SysWOW64 or back to System32
BOOL IsWin64RedirectedDir(const char* path, char** lastSubDir, BOOL failIfDirWithSameNameExists);

// 32-bit build on Win64 only: tests whether the selection contains a pseudo
// directory that the redirector sends to SysWOW64 or back to System32 while no
// real directory with the same name exists on disk (pseudo-directories are
// added only via AddWin64RedirectedDir)
BOOL ContainsWin64RedirectedDir(CFilesWindow* panel, int* indexes, int count, char* redirectedDir,
                                BOOL onlyAdded);

#endif // _WIN64

// our variants of RegQueryValue and RegQueryValueEx add a null terminator for
// REG_SZ, REG_MULTI_SZ and REG_EXPAND_SZ values unlike their API counterparts.
// WARNING: when calculating the required buffer size they return one extra
// character (two for REG_MULTI_SZ) in case the string needs terminating nulls
extern "C"
{
    LONG SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData);
    LONG SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                            LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
}

// Notification from the taskbar on Win7 and newer that a button was created for
// our window. Set at startup, check that it is non-zero
extern UINT TaskbarBtnCreatedMsg;

// returns the icon size with respect to SystemDPI
// if 'large' is TRUE returns the size for a large icon, otherwise for a small one
int GetIconSizeForSystemDPI(CIconSizeEnum iconSize);

// returns the current system DPI (96, 120, 144, ...)
int GetSystemDPI();

// returns the scale corresponding to the current DPI; instead of 1.0 returns
// 100, for 1.25 returns 125, etc.
int GetScaleForSystemDPI();
