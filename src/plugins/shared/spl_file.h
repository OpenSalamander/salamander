// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_file) // so the structures are independent of the current packing alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// ****************************************************************************
//
// CSalamanderSafeFileAbstract
//
// The SafeFile family provides file operations with built-in error handling. The methods check API call
// failures and display the corresponding error messages. Error messages may
// contain various button combinations, from OK through Retry/Cancel to
// Retry/Skip/Skip all/Cancel. The calling function determines the button
// combination with one of the parameters.
//
// While handling error states, the methods need to know the file name so they can
// display a proper error message. They also need to know the parameters of the
// file being opened (such as dwDesiredAccess, dwShareMode, etc.) so that on error
// they can close the handle and reopen it. For example, if a network-layer
// interruption occurs during ReadFile or WriteFile and the user removes the cause
// of the problem and presses Retry, the old file handle cannot be reused. The old
// handle must be closed, the file reopened, the file pointer set, and the
// operation retried. Therefore, note that SafeFileRead and SafeFileWrite may
// change SAFE_FILE::HFile while handling errors.
//
// For these reasons, a plain HANDLE was not sufficient to hold the context and is
// replaced by the SAFE_FILE structure. For SafeFileOpen, this parameter is
// mandatory, while for SafeFileCreate it is only [optional]. This is necessary to
// preserve the compatible behavior of SafeFileCreate for older plugins.
//
// Methods that support the Skip all/Overwrite all buttons have the 'silentMask'
// parameter. It is a pointer to a bitmask composed of SILENT_SKIP_xxx and
// SILENT_OVERWRITE_xxx. If the pointer is not NULL, the bitmask has two functions:
// (1) input: if the corresponding bit is set, the method does not display an
//            error message when an error occurs and answers silently without user interaction.
// (2) output: if the user answers an error prompt with Skip all or Overwrite all,
//             the method sets the corresponding bit in the bitmask.
// This bitmask serves as context passed to the individual methods. For one logical
// group of operations (for example, unpacking multiple files from an archive), the
// caller passes the same bitmask, initialized to 0 at the beginning.
// The caller may also set some bits in the bitmask explicitly to suppress the
// corresponding prompts.
// Salamander reserves part of the bitmask for internal plugin state.
// These are the 1-bits in SILENT_RESERVED_FOR_PLUGINS.
//
// Unless otherwise specified, pointers passed to interface methods must not be NULL.

struct SAFE_FILE
{
    HANDLE HFile;                // handle of the open file (note: it is managed under Salamander core HANDLES)
    char* FileName;              // full path of the open file
    HWND HParentWnd;             // hParent window handle from the SafeFileOpen/SafeFileCreate call; it is used
                                 // if hParent is set to HWND_STORED in subsequent calls
    DWORD dwDesiredAccess;       // > backup of the CreateFile API parameters
    DWORD dwShareMode;           // > for possible retries
    DWORD dwCreationDisposition; // > in case of read or write errors
    DWORD dwFlagsAndAttributes;  // saved CreateFile flags and attributes for retrying the call if a read or write error occurs
    BOOL WholeFileAllocated;     // TRUE if SafeFileCreate preallocated the whole file
};

#define HWND_STORED ((HWND) - 1)

#define SAFE_FILE_CHECK_SIZE 0x00010000 // FIXME: verify that it does not conflict with BUTTONS_xxx

// silentMask bit flags
// skip section
#define SILENT_SKIP_FILE_NAMEUSED 0x00000001 // skips files that cannot be created because a \\
                                             // a directory with the same name already exists (old CNFRM_MASK_NAMEUSED)
#define SILENT_SKIP_DIR_NAMEUSED 0x00000002  // skips directories that cannot be created because a \\
                                             // a file with the same name already exists (old CNFRM_MASK_NAMEUSED)
#define SILENT_SKIP_FILE_CREATE 0x00000004   // skips files that cannot be created for another reason (old CNFRM_MASK_ERRCREATEFILE)
#define SILENT_SKIP_DIR_CREATE 0x00000008    // skips directories that cannot be created for another reason (old CNFRM_MASK_ERRCREATEDIR)
#define SILENT_SKIP_FILE_EXIST 0x00000010    // skips files that already exist (old CNFRM_MASK_FILEOVERSKIP) \\
                                             // mutually exclusive with SILENT_OVERWRITE_FILE_EXIST
#define SILENT_SKIP_FILE_SYSHID 0x00000020   // skips existing System/Hidden files (old CNFRM_MASK_SHFILEOVERSKIP) \\
                                             // mutually exclusive with SILENT_OVERWRITE_FILE_SYSHID
#define SILENT_SKIP_FILE_READ 0x00000040     // skips files for which a read error occurred
#define SILENT_SKIP_FILE_WRITE 0x00000080    // skips files for which a write error occurred
#define SILENT_SKIP_FILE_OPEN 0x00000100     // skips files that cannot be opened

// overwrite section
#define SILENT_OVERWRITE_FILE_EXIST 0x00001000  // overwrites files that already exist (old CNFRM_MASK_FILEOVERYES) \\
                                                // mutually exclusive with SILENT_SKIP_FILE_EXIST
#define SILENT_OVERWRITE_FILE_SYSHID 0x00002000 // overwrites existing System/Hidden files (old CNFRM_MASK_SHFILEOVERYES) \\
                                                // mutually exclusive with SILENT_SKIP_FILE_SYSHID
#define SILENT_RESERVED_FOR_PLUGINS 0xFFFF0000  // this range is reserved for plugin-specific flags

class CSalamanderSafeFileAbstract
{
public:
    // SafeFileOpen
    //   Opens an existing file.
    //
    // Parameters
    //   'file'
    //      [out] Pointer to a 'SAFE_FILE' structure that receives information about the
    //      opened file. This structure serves as context for the other methods in the
    //      SafeFile family. The structure values are meaningful only if SafeFileOpen
    //      returned TRUE. To close the file, call SafeFileClose.
    //
    //   'fileName'
    //      [in] Pointer to a null-terminated string that specifies the name of the
    //      file to open.
    //
    //   'dwDesiredAccess'
    //   'dwShareMode'
    //   'dwCreationDisposition'
    //   'dwFlagsAndAttributes'
    //      [in] See the CreateFile API.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values; specifies the buttons shown in error messages.
    //
    //   'pressedButton'
    //      [out] Pointer to a variable that receives the button pressed in the error
    //      message. The variable is meaningful only if SafeFileOpen returns FALSE;
    //      otherwise its value is undefined. It returns one of the DIALOG_xxx values.
    //      On error it returns DIALOG_CANCEL.
    //      If an error message is suppressed because of 'silentMask', it returns the
    //      value of the corresponding button instead (for example DIALOG_SKIP or DIALOG_YES).
    //
    //      'pressedButton' may be NULL (for example, for BUTTONS_OK or BUTTONS_RETRYCANCEL
    //      there is no point in testing which button was pressed).
    //
    //   'silentMask'
    //      [in/out] Pointer to a variable containing a bitmask of SILENT_xxx values.
    //      For SafeFileOpen, only SILENT_SKIP_FILE_OPEN is meaningful.
    //
    //      If the SILENT_SKIP_FILE_OPEN bit is set in the bitmask, the message would
    //      contain a Skip button (controlled by the 'flags' parameter), and an error
    //      occurs while opening the file, the error message is suppressed.
    //      SafeFileOpen then returns FALSE and, if 'pressedButton' is not NULL, sets
    //      it to DIALOG_SKIP.
    //
    // Return Values
    //   Returns TRUE if the file is opened successfully. The 'file' structure is initialized
    //   and SafeFileClose must be called to close the file.
    //
    //   On failure returns FALSE and sets 'pressedButton'
    //   and 'silentMask' if they are not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileOpen(SAFE_FILE* file,
                                     const char* fileName,
                                     DWORD dwDesiredAccess,
                                     DWORD dwShareMode,
                                     DWORD dwCreationDisposition,
                                     DWORD dwFlagsAndAttributes,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask) = 0;

    //
    // SafeFileCreate
    //   Creates a new file, including its path, if it does not already exist. If the file already exists,
    //   it offers to overwrite it. The method is primarily intended for creating files and directories
    //   extracted from an archive.
    //
    // Parameters
    //   'fileName'
    //      [in] Pointer to a null-terminated string that specifies the name of the
    //      file to create.
    //
    //   'dwDesiredAccess'
    //   'dwShareMode'
    //   'dwFlagsAndAttributes'
    //      [in] See the CreateFile API.
    //
    //   'isDir'
    //      [in] Specifies whether the last path component of 'fileName' is to be a directory (TRUE)
    //      or a file (FALSE). If 'isDir' is TRUE, the variables
    //      'dwDesiredAccess', 'dwShareMode', 'dwFlagsAndAttributes', 'srcFileName',
    //      'srcFileInfo' and 'file' are ignored.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //
    //   'srcFileName'
    //      [in] Pointer to a null-terminated string that specifies the name of the
    //      source file. This name is displayed together with the size
    //      and time ('srcFileInfo') in the prompt to overwrite an existing file,
    //      if the 'fileName' file already exists.
    //      'srcFileName' may be NULL; 'srcFileInfo' is then ignored.
    //      In that case, the overwrite prompt will display the text
    //      "a newly created file" instead of the source file.
    //
    //   'srcFileInfo'
    //      [in] Pointer to a null-terminated string containing the size, date
    //      and time of the source file. This information is displayed together with the name
    //      of the source file 'srcFileName' in the prompt to overwrite an existing file.
    //      Format: "size, date, time".
    //      Size is obtained using CSalamanderGeneralAbstract::NumberToStr,
    //      date by calling GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, ...
    //      and time by calling GetTimeFormat(LOCALE_USER_DEFAULT, 0, ...
    //      See the implementation of the GetFileInfo method in the UnFAT plugin.
    //      'srcFileInfo' may be NULL if 'srcFileName' is also NULL.
    //
    //    'silentMask'
    //      [in/out] Pointer to a bitmask composed of SILENT_SKIP_xxx and SILENT_OVERWRITE_xxx,
    //      see the introduction at the beginning of this file. If 'silentMask' is NULL, it is ignored.
    //      SafeFileCreate checks and sets these constants:
    //        SILENT_SKIP_FILE_NAMEUSED
    //        SILENT_SKIP_DIR_NAMEUSED
    //        SILENT_OVERWRITE_FILE_EXIST
    //        SILENT_SKIP_FILE_EXIST
    //        SILENT_OVERWRITE_FILE_SYSHID
    //        SILENT_SKIP_FILE_SYSHID
    //        SILENT_SKIP_DIR_CREATE
    //        SILENT_SKIP_FILE_CREATE
    //
    //      If 'srcFileName' is not NULL, meaning this is a COPY/MOVE operation, the following applies:
    //        If the "Confirm on file overwrite" option is disabled in Salamander's
    //        Confirmations configuration page, the method behaves as if 'silentMask' contained
    //        SILENT_OVERWRITE_FILE_EXIST.
    //        If "Confirm on system or hidden file overwrite" is disabled, the method behaves
    //        as if 'silentMask' contained SILENT_OVERWRITE_FILE_SYSHID.
    //
    //    'allowSkip'
    //      [in] Specifies whether prompts and error messages will also contain the "Skip"
    //      and "Skip all" buttons.
    //
    //    'skipped'
    //      [out] Returns TRUE if the user clicked the "Skip" or "Skip all" button in a prompt
    //      or error message. Otherwise it returns FALSE. The 'skipped' variable may be NULL.
    //      The variable is meaningful only if SafeFileCreate returns INVALID_HANDLE_VALUE.
    //
    //    'skipPath'
    //      [out] Pointer to a buffer that receives the path the user chose to skip in one of the
    //      prompts by clicking the "Skip" or "Skip all" button. The buffer size is
    //      given by the skipPathMax variable and will not be exceeded. The path is null-terminated.
    //      At the beginning of SafeFileCreate, the buffer is set to an empty string.
    //      'skipPath' may be NULL; 'skipPathMax' is then ignored.
    //
    //    'skipPathMax'
    //      [in] Size of the 'skipPath' buffer in characters. It must be set if 'skipPath'
    //      is not NULL.
    //
    //    'allocateWholeFile'
    //      [in/out] Pointer to a CQuadWord specifying the size to which the file should be
    //      preallocated using SetEndOfFile. If the pointer is NULL, it is ignored
    //      and SafeFileCreate does not attempt preallocation. If the pointer is not
    //      NULL, the function attempts preallocation. The requested size must be greater than
    //      CQuadWord(2, 0) and less than CQuadWord(0, 0x80000000) (8EB).
    //
    //      If SafeFileCreate is also to perform a test (the preallocation mechanism may not always be
    //      functional), the highest bit of the size must be set, that is, add
    //      CQuadWord(0, 0x80000000).
    //
    //      If the file is created successfully (SafeFileCreate returns a handle other than
    //      INVALID_HANDLE_VALUE), the 'allocateWholeFile' variable is set to one of
    //      the following values:
    //       CQuadWord(0, 0x80000000): the file could not be preallocated, and on the next
    //                                 call to SafeFileCreate for files in the same destination
    //                                 'allocateWholeFile' should be NULL
    //       CQuadWord(0, 0):          the file could not be preallocated, but this is not
    //                                 fatal, and on the next call to SafeFileCreate for
    //                                 files with this destination you may request preallocation
    //       other:                    preallocation completed successfully
    //                                 In this case SAFE_FILE::WholeFileAllocated is set
    //                                 to TRUE and SetEndOfFile is called during SafeFileClose to
    //                                 truncate the file and avoid storing unnecessary data.
    //
    //    'file'
    //      [out] Pointer to a 'SAFE_FILE' structure that receives information about the opened
    //      file. This structure serves as context for the other methods in the
    //      SafeFile family. The structure values are meaningful only if SafeFileCreate
    //      returned a value other than INVALID_HANDLE_VALUE. To close the file,
    //      call the SafeFileClose method. If 'file' is not NULL,
    //      SafeFileCreate places the created handle into Salamander HANDLES. If 'file' is NULL,
    //      the handle is not placed into HANDLES. If 'isDir' is TRUE, the 'file' variable is
    //      ignored.
    //
    // Return Values
    //   If 'isDir' is TRUE, returns a value other than INVALID_HANDLE_VALUE on success.
    //   Note that this is not a valid handle of the created directory. On failure it returns
    //   INVALID_HANDLE_VALUE and sets the 'silentMask', 'skipped', and 'skipPath' variables.
    //
    //   If 'isDir' is FALSE, returns the handle of the created file on success and, if
    //   if 'file' is not NULL, the SAFE_FILE structure is filled.
    //   On failure it returns INVALID_HANDLE_VALUE and sets the 'silentMask',
    //   'skipped' and 'skipPath' variables.
    //
    // Remarks
    //   This method may be called only from the main thread. (It may call API FlashWindow(MainWindow),
    //   which must be called from the window thread or it will cause a deadlock.)
    //
    virtual HANDLE WINAPI SafeFileCreate(const char* fileName,
                                         DWORD dwDesiredAccess,
                                         DWORD dwShareMode,
                                         DWORD dwFlagsAndAttributes,
                                         BOOL isDir,
                                         HWND hParent,
                                         const char* srcFileName,
                                         const char* srcFileInfo,
                                         DWORD* silentMask,
                                         BOOL allowSkip,
                                         BOOL* skipped,
                                         char* skipPath,
                                         int skipPathMax,
                                         CQuadWord* allocateWholeFile,
                                         SAFE_FILE* file) = 0;

    // SafeFileClose
    //   Closes the file and frees the data allocated in the 'file' structure.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a successful
    //      call to SafeFileCreate or SafeFileOpen.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual void WINAPI SafeFileClose(SAFE_FILE* file) = 0;

    //SafeFileSeek
    //   Sets the file pointer in an open file.
    //
    //Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //  'distance'
    //      [in/out] Number of bytes by which to move the file pointer.
    //      On success, it receives the new file pointer position.
    //
    //     CQuadWord::Value is interpreted as signed for all three 'moveMethod'
    //      values (note the MSDN bug in SetFilePointerEx, which claims the value
    //      is unsigned for FILE_BEGIN). Therefore, if you want to move backward
    //      from the current position (FILE_CURRENT) or from the end of the file
    //      (FILE_END), set CQuadWord::Value to a negative number. You can assign
    //      __int64 directly to CQuadWord::Value.
    //
    //     The returned value is the absolute position from the beginning of the file
    //      and ranges from 0 to 2^63. No current Windows version supports files
    //      larger than 2^63.
    //
    //   'moveMethod'
    //      [in] Starting position for the file pointer. It can be one of:
    //           FILE_BEGIN, FILE_CURRENT or FILE_END.
    //
    //   'error'
    //      [out] Pointer to a DWORD variable that receives the value returned
    //      by GetLastError() on failure. 'error' may be NULL.
    //
    // Return Values
    //   On success returns TRUE and sets 'distance' to the new file
    //   pointer position.
    //
    //   On failure returns FALSE and sets 'error' to GetLastError(),
    //   if 'error' is not NULL. It does not display the error; use SafeFileSeekMsg for that.
    //
    // Remarks
    //   This method calls the SetFilePointer API, so its limitations apply.
    //
    //   Setting the file pointer beyond the end of the file is not an error. The file
    //   size does not increase until you call SetEndOfFile or SafeFileWrite. See the
    //  SetFilePointer API.
    //
    //   This method can be used to get the file size by setting 'distance'
    //   to 0 and 'moveMethod' to FILE_END. The returned 'distance' value will be
    //  the file size.
    //
    //  This method can be called from any thread.
    virtual BOOL WINAPI SafeFileSeek(SAFE_FILE* file,
                                     CQuadWord* distance,
                                     DWORD moveMethod,
                                     DWORD* error) = 0;

    // SafeFileSeekMsg
    //   Sets the file pointer in an open file. Displays an error if one occurs.
    //
    // Parameters
    //   'file'
    //   'distance'
    //   'moveMethod'
    //      See the SafeFileSeek comment.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //      If it is HWND_STORED, the 'hParent' from the SafeFileOpen/SafeFileCreate call is used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values; determines the buttons shown in the error message.
    //
    //   'pressedButton'
    //      [out] Pointer to a variable that receives the button pressed in the error
    //      message. The variable is meaningful only if SafeFileSeekMsg returns FALSE.
    //      'pressedButton' may be NULL (for example, for BUTTONS_OK there is no point in testing
    //      which button was pressed)
    //
    //   'silentMask'
    //      [in/out] Pointer to a variable containing a bitmask of SILENT_SKIP_xxx values.
    //      See the SafeFileOpen comment for details.
    //      SafeFileSeekMsg checks and sets the SILENT_SKIP_FILE_READ bit if
    //      'seekForRead' is TRUE, or SILENT_SKIP_FILE_WRITE if 'seekForRead' is FALSE;
    //
    //   'seekForRead'
    //      [in] Tells the method whether the seek was performed for reading or writing. The method uses
    //      this variable only on error. It determines which bit is used for
    //      'silentMask' and what the error message title will be: "Error Reading File" or
    //      "Error Writing File".
    //
    // Return Values
    //   On success returns TRUE and sets 'distance' to the new file
    //   pointer position.
    //
    //   On failure returns FALSE and sets 'pressedButton'
    //   and 'silentMask', if they are not NULL.
    //
    // Remarks
    //   See SafeFileSeek.
    //
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileSeekMsg(SAFE_FILE* file,
                                        CQuadWord* distance,
                                        DWORD moveMethod,
                                        HWND hParent,
                                        DWORD flags,
                                        DWORD* pressedButton,
                                        DWORD* silentMask,
                                        BOOL seekForRead) = 0;

    // SafeFileGetSize
    //   Returns the file size.
    //
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //   'fileSize'
    //      [out] Pointer to a CQuadWord structure that receives the file size.
    //
    //   'error'
    //      [out] Pointer to a DWORD variable that receives the value returned
    //      by GetLastError() on failure. 'error' may be NULL.
    //
    // Return Values
    //   Returns TRUE on success and sets 'fileSize'.
    //   Returns FALSE on failure and sets 'error' if it is not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileGetSize(SAFE_FILE* file,
                                        CQuadWord* fileSize,
                                        DWORD* error) = 0;

    // SafeFileRead
    //   Reads data from the file starting at the current file pointer. After the
    //   operation completes, the file pointer is advanced by the number of bytes
    //   read. This method supports only synchronous reads, so it does not return
    //   until the data is read or an error occurs.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //   'lpBuffer'
    //      [out] Pointer to the buffer that receives the data read from the file.
    //
    //   'nNumberOfBytesToRead'
    //      [in] Specifies how many bytes to read from the file.
    //
    //   'lpNumberOfBytesRead'
    //      [out] Points to a variable that receives the actual number of bytes read
    //      into the buffer.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //      If set to HWND_STORED, the 'hParent' from SafeFileOpen/SafeFileCreate is used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values, optionally ORed with SAFE_FILE_CHECK_SIZE;
    //      determines the buttons shown in error messages. If the SAFE_FILE_CHECK_SIZE bit is set,
    //      SafeFileRead treats failure to read the requested number of bytes as an error and
    //      shows an error message. Without this bit, it behaves like the ReadFile API.
    //
    //   'pressedButton'
    //   'silentMask'
    //      See SafeFileOpen.
    //
    // Return Values
    //   On success, returns TRUE and sets 'lpNumberOfBytesRead' to the number of bytes read.
    //
    //   On failure, returns FALSE and sets 'pressedButton' and 'silentMask' if they are not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileRead(SAFE_FILE* file,
                                     LPVOID lpBuffer,
                                     DWORD nNumberOfBytesToRead,
                                     LPDWORD lpNumberOfBytesRead,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask) = 0;

    // SafeFileWrite
    //   Writes data to the file at the current file pointer. After the operation
    //   completes, the file pointer is advanced by the number of bytes written.
    //   This method supports only synchronous writes, so it does not return
    //   until the data is written or an error occurs.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //   'lpBuffer'
    //      [in] Pointer to the buffer containing the data to write to the file.
    //
    //   'nNumberOfBytesToWrite'
    //      [in] Specifies how many bytes to write from the buffer to the file.
    //
    //   'lpNumberOfBytesWritten'
    //      [out] Points to a variable that receives the actual number of bytes written.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //      If set to HWND_STORED, the 'hParent' from SafeFileOpen/SafeFileCreate is used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values; determines the buttons shown in error messages.
    //
    //   'pressedButton'
    //   'silentMask'
    //      See SafeFileOpen.
    //
    // Return Values
    //   On success, returns TRUE and sets 'lpNumberOfBytesWritten' to the number of bytes written.
    //
    //   On failure, returns FALSE and sets 'pressedButton' and 'silentMask' if they are not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileWrite(SAFE_FILE* file,
                                      LPVOID lpBuffer,
                                      DWORD nNumberOfBytesToWrite,
                                      LPDWORD lpNumberOfBytesWritten,
                                      HWND hParent,
                                      DWORD flags,
                                      DWORD* pressedButton,
                                      DWORD* silentMask) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_file)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
