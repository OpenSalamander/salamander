// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

extern const char* FTP_ANONYMOUS; // standard name for an anonymous user
// standard FTP port (21) is defined in the constant: IPPORT_FTP

// path type constants on an FTP server for function GetFTPServerPathType
enum CFTPServerPathType
{
    ftpsptEmpty,   // empty value (has not been evaluated yet at all)
    ftpsptUnknown, // does not match any of the following path types
    ftpsptUnix,    // e.g. /pub/altap/salamand (but also /\dir-with-backslash)
    ftpsptNetware, // e.g. /pub/altap/salamand or \pub\altap\salamand
    ftpsptOpenVMS, // e.g. PUB$DEVICE:[PUB.VMS] or [PUB.VMS] (named "OpenVMS" so it is not confused with "MVS")
    ftpsptMVS,     // e.g. 'VEA0016.MAIN.CLIST'
    ftpsptWindows, // e.g. /pub/altap/salamand or \pub\altap\salamand
    ftpsptIBMz_VM, // e.g. ACADEM:ANONYMOU.PICS or ACADEM:ANONYMOU. (root)
    ftpsptOS2,     // e.g. C:/DIR1/DIR2 or C:\DIR1\DIR2
    ftpsptTandem,  // e.g. \SYSTEM.$VVVVV.SUBVOLUM.FILENAME
    ftpsptAS400,   // e.g. /QSYS.LIB/GARY.LIB (or /QDLS/oetst)
};

// determines the path type on an FTP server; if 'serverFirstReply' is not NULL, it is the first
// reply from the server (often containing the server version); if 'serverSystem' is not NULL, it
// is the FTP server response to the SYST command (our ftpcmdSystem); 'path' is the path on the FTP
CFTPServerPathType GetFTPServerPathType(const char* serverFirstReply, const char* serverSystem,
                                        const char* path);

// parses the system name from the server response to the SYST command stored in 'serverSystem'
// and stores it into 'sysName' (a buffer of at least 201 characters); if it is not possible to
// obtain the system name from this response, returns an empty string
void FTPGetServerSystem(const char* serverSystem, char* sysName);

// shortens the FTP server path by the last directory/file (directory separators depend on the
// path type - 'type'), 'path' is an in/out buffer (min. size 'pathBufSize' bytes; note that with
// VMS paths it may be necessary to write more characters into 'path' than its length - if there is
// not enough space in 'path', the string is truncated); 'cutDir'
// (a buffer of at least 'cutDirBufSize') returns the last directory (the removed part;
// if the string does not fit into the buffer, it is truncated); if 'fileNameCouldBeCut'
// is not NULL, it returns TRUE when the removed part can be a file name; returns TRUE
// if shortening was performed (it was not a root path and 'type' is a known path type)
BOOL FTPCutDirectory(CFTPServerPathType type, char* path, int pathBufSize,
                     char* cutDir, int cutDirBufSize, BOOL* fileNameCouldBeCut);

// concatenates the path 'path' and 'name' (file/directory name - 'isDir' is FALSE/TRUE) into
// 'path', performing the concatenation according to the path type - 'type'; 'path' is a buffer of
// at least 'pathSize' characters; returns TRUE if 'name' fits into 'path'; if 'path' or 'name' is
// empty, no concatenation occurs (the 'path' path may be adjusted - truncated to the minimal length - e.g.
// "/pub/" + "" = "/pub")
BOOL FTPPathAppend(CFTPServerPathType type, char* path, int pathSize, const char* name, BOOL isDir);

// determines whether the FTP server path (not a user-part path) 'path' is valid and whether it is
// not a root path (detection is based on the path type - 'type'); returns TRUE if 'path' is valid
// and is not a root path
BOOL FTPIsValidAndNotRootPath(CFTPServerPathType type, const char* path);

// converts escape sequences (e.g. "%20" = " ") in the string 'txt' to ASCII characters
void FTPConvertHexEscapeSequences(char* txt);
// prepares the text 'txt' so that it survives the subsequent conversion of escape sequences to
// ASCII characters (e.g. "%20" = "%2520"); 'txtSize' is the size of the 'txt' buffer; returns
// FALSE if the buffer is too small for a successful conversion
BOOL FTPAddHexEscapeSequences(char* txt, int txtSize);

// splits the user-part path into individual components (user name, host, port, and path (without
// '/' or '\\' at the beginning)); inserts zero terminators into the path string 'p' so each
// component becomes a null-terminated string; if 'firstCharOfPath' is not NULL and the FTP path
// contains a path within the server ('path' string), 'firstCharOfPath' receives the separator of this
// path ('/' or '\\'); it does not need to return all components ('user', 'host', 'port', and 'path'
// can be NULL); if a particular component cannot be obtained (the path might not contain it), the
// corresponding variable receives NULL; 'user', 'host', and 'port' are returned trimmed of
// whitespace on both sides; 'userLength' is zero if we do not know how long the user name is or if it
// does not contain "forbidden" characters, otherwise it is the expected length of the user name;
// path format: "//user:password@host:port/path" or just "user:password@host:port/path" (and
// "/path" can also be "\path") + "user:password@", ":password", and ":port" can be omitted
void FTPSplitPath(char* p, char** user, char** password, char** host, char** port,
                  char** path, char* firstCharOfPath, int userLength);

// returns the length of the username for use in the "userLength" parameters (FTPSplitPath,
// FTPFindPath, etc.); for an anonymous user and other usernames without special characters
// ('@', '/', '\', ':') returns zero; 'user' can also be NULL
int FTPGetUserLength(const char* user);

// returns a pointer to the remote path in an FTP path (a pointer into the 'path' buffer); 'path'
// is in the format "//user:password@host:port/remotepath" or just
// "user:password@host:port/remotepath" ("user:password@", ":password", and ":port" can be
// omitted); 'userLength' is zero if we do not know how long the user name is or if it does not
// contain "forbidden" characters, otherwise it is the expected length of the user name; if the
// remote path is not found, returns a pointer to the end of the 'path' string
const char* FTPFindPath(const char* path, int userLength);

// based on the path type returns a pointer to the remote path in the path (a pointer into the
// 'path' buffer); used to skip/keep the leading slash/backslash in the path; 'path' is of the form
// "/remotepath" or "\remotepath" (the part of the path after the host in the user-part path); 'type' is
// the path type
const char* FTPGetLocalPath(const char* path, CFTPServerPathType type);

// compares two paths on the FTP server (not user-part paths), returns TRUE if they are the same;
// 'type' is the type of at least one of the paths
BOOL FTPIsTheSameServerPath(CFTPServerPathType type, const char* p1, const char* p2);

// determines whether 'prefix' is a prefix of 'path' - both paths are on the FTP server (not
// user-part paths), returns TRUE if it is a prefix; 'type' is the type of at least one of the
// paths; if 'mustBeSame' is TRUE, 'prefix' and 'path' must match (same function as
BOOL FTPIsPrefixOfServerPath(CFTPServerPathType type, const char* prefix, const char* path,
                             BOOL mustBeSame = FALSE);

// compares two user-part paths on FTP, returns TRUE if they are the same; if
// 'sameIfPath2IsRelative' is TRUE, returns TRUE even if 'p1' and 'p2' match only in user+host+port
// and 'p2' does not contain a path within the FTP server (e.g. "ftp://petr@localhost"); 'type' is
// 'userLength' is zero if we do not know how long the user name is or if it does not contain "forbidden"
// characters, otherwise it is the expected length of the user name
BOOL FTPIsTheSamePath(CFTPServerPathType type, const char* p1, const char* p2,
                      BOOL sameIfPath2IsRelative, int userLength);

// compares the roots of two user-part FTP paths, returns TRUE if they are the same;
// 'userLength' is zero if we do not know how long the user name is or if it does not contain "forbidden"
// characters, otherwise it is the expected length of the user name
BOOL FTPHasTheSameRootPath(const char* p1, const char* p2, int userLength);

// returns an error description
char* FTPGetErrorText(int err, char* buf, int bufSize);

// returns, based on the path type, the character used to separate path components (subdirectories)
char FTPGetPathDelimiter(CFTPServerPathType pathType);

// only for the ftpsptIBMz_VM path type: obtains the root path from the path 'path';
// 'root'+'rootSize' is the buffer for the result; returns success
BOOL FTPGetIBMz_VMRootPath(char* root, int rootSize, const char* path);

// only for the ftpsptOS2 path type: obtains the root path from the path 'path';
// 'root'+'rootSize' is the buffer for the result; returns success
BOOL FTPGetOS2RootPath(char* root, int rootSize, const char* path);

// obtains the numeric value from the UNIX rights string 'rights'; returns the value in actAttr
// (must not be NULL); if it finds permissions that cannot be set via "site chmod" ('s', 't', etc.),
// it ORs the respective bits into 'attrDiff' (must not be NULL); if the rights are UNIX rights, it
// returns TRUE, otherwise it returns FALSE (unknown rights string or e.g. ACL rights on UNIX (such
BOOL GetAttrsFromUNIXRights(DWORD* actAttr, DWORD* attrDiff, const char* rights);

// converts 'attrs' (numeric rights on UNIX) to a UNIX rights string (without the first letter)
// into the 'buf' buffer of size 'bufSize'
void GetUNIXRightsStr(char* buf, int bufSize, DWORD attrs);

// returns TRUE if 'rights' represents UNIX link rights; UNIX rights = must have 10 characters or 11
// if it ends with '+' (ACL rights); link rights format: 'lrw?rw?rw?' + instead of 'r' and 'w'
// there may be '-'
BOOL IsUNIXLink(const char* rights);

// same function as FTPGetErrorText, only ensures CRLF at the end of the string;
// NOTE: 'bufSize' must be greater than 2
void FTPGetErrorTextForLog(DWORD err, char* errBuf, int bufSize);

// method for detecting whether the buffer 'readBytes' (with 'readBytesCount' valid bytes, reading
// from position 'readBytesOffset') already contains the entire response from the FTP server;
// returns TRUE on success - 'reply' (must not be NULL) receives a pointer to the beginning of the
// response, 'replySize' (must not be NULL) receives the length of the response, 'replyCode'
// (if not NULL) receives the FTP response code or -1 if the response has no code (does not start
// with a three-digit number); if the response is not complete yet, returns FALSE
BOOL FTPReadFTPReply(char* readBytes, int readBytesCount, int readBytesOffset,
                     char** reply, int* replySize, int* replyCode);

// parses the directory from the string 'reply' (rules see RFC 959 - FTP response number "257");
// does not strictly require "257" at the beginning of the string (handle if necessary before
// calling); returns TRUE if the directory was obtained successfully
// can be called from any thread
BOOL FTPGetDirectoryFromReply(const char* reply, int replySize, char* dirBuf, int dirBufSize);

// parses IP+port from the string 'reply' of length 'replySize' (-1 == use 'strlen(reply)') (returns
// it in 'ip'+'port' (must not be NULL); rules see RFC 959 - FTP response number 227);
// does not strictly require "227" at the beginning of the string (handle if necessary before
// calling); returns TRUE if obtaining IP+port succeeded
// can be called from any thread
BOOL FTPGetIPAndPortFromReply(const char* reply, int replySize, DWORD* ip, unsigned short* port);

// parses the data size from the string 'reply' of length 'replySize' and returns it in 'size';
// on success returns TRUE, otherwise FALSE and 'size' is set arbitrarily
BOOL FTPGetDataSizeInfoFromSrvReply(CQuadWord& size, const char* reply, int replySize);

// creates a VMS directory name (adds ".DIR;1")
void FTPMakeVMSDirName(char* vmsDirNameBuf, int vmsDirNameBufSize, const char* dirName);

// checks whether there is an escape sequence before the character 'checkedChar' in the path
// 'pathBeginning' (e.g. "^." is '.', which VMS does not consider a path or extension separator,
// etc.); returns TRUE if there is an escape sequence before the character (meaning the character
BOOL FTPIsVMSEscapeSequence(const char* pathBeginning, const char* checkedChar);

// returns TRUE if the path 'path' of type 'type' ends with a path component delimiter
// (e.g. "/pub/dir/" or "PUB$DEVICE:[PUB.VMS.]")
BOOL FTPPathEndsWithDelimiter(CFTPServerPathType type, const char* path);

// shortens an IBM z/VM path on the FTP server by the last two components; 'path' is an in/out
// buffer (min. size 'pathBufSize' bytes), 'cutDir' (a buffer of at least
// 'cutDirBufSize') returns the last two components (the removed part; if the string
// does not fit into the buffer, it is truncated); returns TRUE if shortening occurred
BOOL FTPIBMz_VmCutTwoDirectories(char* path, int pathBufSize, char* cutDir, int cutDirBufSize);

// trims the file version number from an OpenVMS file name (e.g. "a.txt;1" -> "a.txt");
// 'name' is the name; 'nameLen' is the length of 'name' (-1 = length unknown, use strlen(name));
// returns TRUE if trimming occurred
BOOL FTPVMSCutFileVersion(char* name, int nameLen);

// returns TRUE if 'path' is a relative path on a path of type 'pathType';
// NOTE: e.g. "[pub]" on VMS or "/dir" on OS/2 are considered absolute paths for this function,
// even though they need to be completed to a full absolute path
// by calling FTPCompleteAbsolutePath
BOOL FTPIsPathRelative(CFTPServerPathType pathType, const char* path);

// returns the first subdirectory on a relative path and shortens that relative path by it;
// 'path' is a relative path on a path of type 'pathType'; 'cut' is a buffer for
// the removed first subdirectory of size 'cutBufSize'; returns TRUE if the
// trimming succeeded (after trimming the last subdirectory from the path 'path'
// contains an empty string); NOTE: does not handle file names (e.g. VMS: incorrectly
// splits "[.a]b.c;1" into "a", "b", and "c;1")
BOOL FTPCutFirstDirFromRelativePath(CFTPServerPathType pathType, char* path,
                                    char* cut, int cutBufSize);

// completes an absolute path to a full absolute path (for VMS the volume is added - e.g.
// "PUB$DEVICE:", for OS/2 the drive - e.g. "C:"); 'pathType' is the path type; 'path' is an
// absolute path; 'path' (a buffer of at least 'pathBufSize' characters) returns the full
// absolute path (lack of space = truncated result); 'workPath' is the working
// full absolute path (from which the volume/drive is taken); returns success (even if
// 'path' is already a full absolute path on input - meaning there is nothing to do)
BOOL FTPCompleteAbsolutePath(CFTPServerPathType pathType, char* path, int pathBufSize,
                             const char* workPath);

// removes "." and ".." from the path 'path' of type 'pathType' (applies only to selected path
// types; for example on VMS this function does nothing); returns FALSE only if ".." cannot be
// removed (the path is invalid: e.g. "/..")
BOOL FTPRemovePointsFromPath(char* path, CFTPServerPathType pathType);

// returns TRUE if the file system with path type 'pathType' is case-sensitive (unix),
// otherwise it is case-insensitive (windows)
BOOL FTPIsCaseSensitive(CFTPServerPathType pathType);

// returns TRUE if this is an error response to the LIST command that only says the directory is
// empty (so it is not actually an error - unfortunately VMS and Z/VM still report such errors)
BOOL FTPIsEmptyDirListErrReply(const char* listErrReply);

// returns TRUE if the name 'name' can be the name of a single file/directory ('isDir' is
// FALSE/TRUE) on the path 'path' of type 'pathType'; used only to prevent creating multiple
// subdirectories instead of just one (e.g. "a.b.c" on VMS creates three subdirectories)
// the server reports the syntax error in the name
BOOL FTPMayBeValidNameComponent(const char* name, const char* path, BOOL isDir,
                                CFTPServerPathType pathType);

// for uploading to the server: adds the *.* or * mask to the target path (for later processing of
// the operation mask); 'pathType' is the path type; 'targetPath' is a buffer (of size
// 'targetPathBufSize') containing the target path on input (full, including fs-name)
// and on output enriched with the mask; 'noFilesSelected' is TRUE if no files should be uploaded
// (only directories are selected in the panel)
void FTPAddOperationMask(CFTPServerPathType pathType, char* targetPath, int targetPathBufSize,
                         BOOL noFilesSelected);

// generates a new name for the name 'originalName' (it is a file/directory if 'isDir' is
// FALSE/TRUE) on a path of type 'pathType'; 'phase' is the IN/OUT generation phase (0 = initial
// phase, returns -1 if there is no next phase, otherwise returns the number of the next phase -
// used for possible subsequent calls of this function); 'newName' is the buffer for the generated
// name (buffer size is MAX_PATH); 'index' is the IN/OUT name index within phase 'phase'
// (used for generating additional names in a single phase), zero on the first call within one
// phase; 'alreadyRenamedFile' is TRUE only if it is a file that has most likely
// already been renamed (we ensure "name (2)"->"name (3)" instead of ->"name (2) (2)")
void FTPGenerateNewName(int* phase, char* newName, int* index, const char* originalName,
                        CFTPServerPathType pathType, BOOL isDir, BOOL alreadyRenamedFile);

// for AS/400: if the name is in the form "???1.file/???2.mbr" (both names ???1 and ???2 are the
// same), copies "???2.mbr" into 'mbrName' (a buffer of size MAX_PATH), otherwise copies
// "???1.???2.mbr"
void FTPAS400CutFileNamePart(char* mbrName, const char* name);

// for AS/400: if the name is in the form "???.mbr", rewrites 'name' (a buffer of size at least
// 2*MAX_PATH) to "???.file/???.mbr" (both names ??? are the same); if the name is in the form
// "???1.???2.mbr", writes "???1.file.???2.mbr" into 'name'
void FTPAS400AddFileNamePart(char* name);
