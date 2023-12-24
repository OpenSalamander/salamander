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
#pragma pack(push, enter_include_spl_file) // so that structures are independent of set alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//****************************************************************************
//
// CSalamanderSafeFileAbstract
//
// The family of SafeFile methods is used for safe file operations. Methods check
// error states of API calls and display appropriate error messages. Error messages
// can contain various combinations of buttons. From OK, through Retry/Cancel, to
// Retry/Skip/Skip all/Cancel. The combination of buttons is determined by the
// calling function in one of the parameters.
//
// During the resolution of problem states, methods need to know the name of the
// file in order to display a solid error message. At the same time, they need to
// know the parameters of the opened file (such as dwDesiredAccess, dwShareMode, etc.)
// in case of an error so that they can close the handle and reopen it. For example,
// if there is an interruption at the network layer during ReadFile or WriteFile
// and the user removes the cause of the problem and presses Retry, the old file
// handle cannot be reused. It is necessary to close the old handle, reopen the file,
// set the pointer and repeat the operation.
// Therefore, BEWARE: the SafeFileRead and SafeFileWrite methods may change the
// value of SAFE_FILE::HFile when resolving error states.
//
// For the reasons described, the classic HANDLE was not enough to hold the context
// and it is replaced by the SAFE_FILE structure. In the case of the SafeFileOpen
// method, it is a necessary parameter, while for the SafeFileCreate methods this
// parameter is only [optional]. This is given by the need to preserve the compatible
// behavior of the SafeFileCreate method for older plugins.
//
// Methods supporting Skip all/Overwrite all buttons have the 'silentMask' parameter.
// It is a pointer to a bit array consisting of SILENT_SKIP_xxx and SILENT_OVERWRITE_xxx.
// If the pointer is different from NULL, the bit array has two functions:
// (1) input: if the corresponding bit is set, the method will not display
//           an error message in case of an error and will silently respond without
//          interacting with the user.
// (2) output: If the user responds to the error prompt with the Skip all or
//            Overwrite all button, the method sets the corresponding bit in the
//           bit array.
// This bit array serves as a context passed to the individual methods. For one
// logical group of operations (e.g. unpacking multiple files from an archive),
// the caller passes the same bit array, which it initializes to 0 at the beginning.
// Alternatively, it can explicitly set some bits in the bit array to suppress
// the corresponding queries.
// Salamander reserves part of the bit array for the internal states of the plugin.
// It is a bit array of ones in SILENT_RESERVED_FOR_PLUGINS.
//
// If the pointers passed to the method interface are not specified otherwise,
// they must not have a NULL value.
//

struct SAFE_FILE
{
    HANDLE HFile;                // handle of the opened file (beware, it is under HANDLES of Salamander core)
    char* FileName;              // name of the opened file with full path
    HWND HParentWnd;             // handle hParent of the window from the call of SafeFileOpen/SafeFileCreate; used
                                 // if hParent is set to HWND_STORED in the following calls
    DWORD dwDesiredAccess;       // > backup of parameters for API CreateFile
    DWORD dwShareMode;           // > for its possible repetition of calling
    DWORD dwCreationDisposition; // > in case of errors during reading or writing
    DWORD dwFlagsAndAttributes;  // >
    BOOL WholeFileAllocated;     // TRUE if the SafeFileCreate function preallocated the entire file
};

#define HWND_STORED ((HWND)-1)

#define SAFE_FILE_CHECK_SIZE 0x00010000 // FIXME: verify that there's no conflict with BUTTONS_xxx

// bits of silentMask mask
// skip section
#define SILENT_SKIP_FILE_NAMEUSED 0x00000001 // skips files that cannot be created because there is already \
                                             // a directory with the same name (old CNFRM_MASK_NAMEUSED)
#define SILENT_SKIP_DIR_NAMEUSED 0x00000002  // skips directories that cannot be created because there is already \
                                             // a file with the same name (old CNFRM_MASK_NAMEUSED)
#define SILENT_SKIP_FILE_CREATE 0x00000004   // skips files that cannot be created for another reason (old CNFRM_MASK_ERRCREATEFILE)
#define SILENT_SKIP_DIR_CREATE 0x00000008    // skips directories that cannot be created for another reason (old CNFRM_MASK_ERRCREATEDIR)
#define SILENT_SKIP_FILE_EXIST 0x00000010    // skips files that already exist (old CNFRM_MASK_FILEOVERSKIP) \
                                             // excluded with SILENT_OVERWRITE_FILE_EXIST
#define SILENT_SKIP_FILE_SYSHID 0x00000020   // skips System/Hidden files that already exist (old CNFRM_MASK_SHFILEOVERSKIP) \
                                             // excluded with SILENT_OVERWRITE_FILE_SYSHID
#define SILENT_SKIP_FILE_READ 0x00000040     // skips files that cannot be read
#define SILENT_SKIP_FILE_WRITE 0x00000080    // skips files that cannot be written
#define SILENT_SKIP_FILE_OPEN 0x00000100     // skips files that cannot be opened

// overwrite section
#define SILENT_OVERWRITE_FILE_EXIST 0x00001000  // overwrites files that already exist (old CNFRM_MASK_FILEOVERYES) \
                                                // excluded with SILENT_SKIP_FILE_EXIST
#define SILENT_OVERWRITE_FILE_SYSHID 0x00002000 // overwrites System/Hidden files that already exist (old CNFRM_MASK_SHFILEOVERYES) \
                                                // excluded with SILENT_SKIP_FILE_SYSHID
#define SILENT_RESERVED_FOR_PLUGINS 0xFFFF0000  // this part of the bit array is reserved for plugins

class CSalamanderSafeFileAbstract
{
public:
    //
    // SafeFileOpen
    //   Opens an existing file.
    //
    // Parameters
    //   'file'
    //      [out] Pointer to 'SAFE_FILE' structure that receives information about the opened
    //      file. This structure serves as a context for other methods from the SafeFile family.
    //      The values of the structure have meaning only if SafeFileOpen returns TRUE.
    //      To close the file, call the SafeFileClose method.
    //
    //   'fileName'
    //      [in] Pointer to a null-terminated string that contains the name of the file to be opened.
    //
    //   'dwDesiredAccess'
    //   'dwShareMode'
    //   'dwCreationDisposition'
    //   'dwFlagsAndAttributes'
    //      [in] see API CreateFile.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages will be displayed.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values, specifies the buttons displayed in error messages.
    //
    //   'pressedButton'
    //      [out] Pointer to a variable that receives the pressed button during the error message.
    //      The variable has meaning only if the SafeFileOpen method returns FALSE, otherwise
    //      its value is undefined. Returns one of the DIALOG_xxx values. In case of errors,
    //      it returns the value DIALOG_CANCEL.
    //      If any error message is ignored due to 'silentMask', it returns the value of the
    //      corresponding button (e.g. DIALOG_SKIP or DIALOG_YES).
    //
    //      'pressedButton' can be NULL (e.g. for BUTTONS_OK or BUTTONS_RETRYCANCEL it does not
    //      make sense to test the pressed button).
    //
    //   'silentMask'
    //      [in/out] Pointer to a variable containing a bit array of SILENT_xxx values.
    //      For the SafeFileOpen method, only the value SILENT_SKIP_FILE_OPEN has meaning.
    //
    //      If the SILENT_SKIP_FILE_OPEN bit is set in the bit array and the error message
    //      should also have the Skip button (controlled by the 'flags' parameter) and
    //      an error occurs while opening the file, the error message will be suppressed.
    //      SafeFileOpen then returns FALSE and if 'pressedButton' is different from NULL,
    //      it sets the value DIALOG_SKIP to it.
    //
    // Return Values
    //   Returns TRUE if the file was successfully opened. The 'file' structure is initialized
    //   and to close the file, call SafeFileClose.
    //
    //   Returns FALSE if an error occurs and sets the values of the 'pressedButton' and
    //   'silentMask' variables, if they are different from NULL.
    //
    // Remarks
    //   The method can be called from any thread.
    //
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
    //   Creates a new file including the path if it does not already exist. If the file
    //   already exists, it offers to overwrite it. The method is primarily intended for
    //   creating files and directories unpacked from an archive.
    //
    // Parameters
    //   'fileName'
    //      [in] Pointer to a null-terminated string that contains the name of the file to be created.
    //
    //   'dwDesiredAccess'
    //   'dwShareMode'
    //   'dwFlagsAndAttributes'
    //      [in] see API CreateFile.
    //
    //   'isDir'
    //      [in] Specifies whether the last folder of the 'fileName' path should be a directory (TRUE)
    //      or a file (FALSE). If 'isDir' is TRUE, the variables 'dwDesiredAccess', 'dwShareMode',
    //      'dwFlagsAndAttributes', 'srcFileName', 'srcFileInfo' and 'file' are ignored.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages will be displayed.
    //
    //   'srcFileName'
    //      [in] Pointer to a null-terminated string that contains the name of the source file.
    //      This name will be displayed together with the size and time ('srcFileInfo') in the
    //      query to overwrite an existing file if the file 'fileName' already exists.
    //      'srcFileName' can be NULL, then 'srcFileInfo' is ignored.
    //      In this case, the query to overwrite will contain the text "a newly created file"
    //      instead of the source file name.
    //
    //   'srcFileInfo'
    //      [in] Pointer to a null-terminated string that contains the size, date and time of
    //      the source file. This information will be displayed together with the name of the
    //      source file 'srcFileName' in the query to overwrite an existing file.
    //      Format: "size, date, time".
    //      The size is obtained using CSalamanderGeneralAbstract::NumberToStr,
    //      the date using GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, ...
    //      and the time using GetTimeFormat(LOCALE_USER_DEFAULT, 0, ...
    //      See the implementation of the GetFileInfo method in the UnFAT plugin.
    //      'srcFileInfo' can be NULL if 'srcFileName' is also NULL.
    //
    //   'silentMask'
    //      [in/out] Pointer to a bit array consisting of SILENT_SKIP_xxx and SILENT_OVERWRITE_xxx,
    //      see the introduction at the beginning of this file. If 'silentMask' is NULL, it will be ignored.
    //      The SafeFileCreate method tests and sets these constants:
    //        SILENT_SKIP_FILE_NAMEUSED
    //        SILENT_SKIP_DIR_NAMEUSED
    //        SILENT_OVERWRITE_FILE_EXIST
    //        SILENT_SKIP_FILE_EXIST
    //        SILENT_OVERWRITE_FILE_SYSHID
    //        SILENT_SKIP_FILE_SYSHID
    //        SILENT_SKIP_DIR_CREATE
    //        SILENT_SKIP_FILE_CREATE
    //
    //      If 'srcFileName' is different from NULL, i.e. it is a COPY/MOVE operation, the following applies:
    //        If the "Confirm on file overwrite" option is disabled in the Salamander configuration
    //        (Confirmations page), the method behaves as if 'silentMask' contained SILENT_OVERWRITE_FILE_EXIST.
    //        If "Confirm on system or hidden file overwrite" is disabled, the method behaves as if
    //        'silentMask' contained SILENT_OVERWRITE_FILE_SYSHID.
    //
    //    'allowSkip'
    //      [in] Specifies whether the queries and error messages will also contain the Skip
    //      and Skip all buttons.
    //
    //    'skipped'
    //      [out] Returns TRUE if the user clicked the "Skip" or "Skip all" button in the query.
    //      Otherwise, it returns FALSE. The 'skipped' variable can be NULL.
    //      The variable has meaning only if SafeFileCreate returns INVALID_HANDLE_VALUE.
    //
    //    'skipPath'
    //      [out] Pointer to a buffer that receives the path that the user wanted to skip
    //      by clicking the "Skip" or "Skip all" button in one of the queries. The size of
    //      the buffer is given by the 'skipPathMax' variable, which will not be exceeded.
    //      The path will be terminated by a zero. At the beginning of the SafeFileCreate
    //      method, the buffer is set to an empty string.
    //      'skipPath' can be NULL, 'skipPathMax' is then ignored.
    //
    //    'skipPathMax'
    //      [in] Size of the 'skipPath' buffer in characters. Must be set if 'skipPath'
    //      is different from NULL.
    //
    //    'allocateWholeFile'
    //      [in/out] Pointer to CQuadWord specifying the size to which the file should be
    //      preallocated using the SetEndOfFile function. If the pointer is NULL, it will
    //      be ignored and SafeFileCreate will not attempt to preallocate. The required
    //      size must be greater than CQuadWord(2, 0) and less than CQuadWord(0, 0x80000000) (8EB).
    //
    //      If SafeFileCreate is also to perform a test (the preallocation mechanism may not
    //      always work), the highest bit of the size must be set, i.e. the value must be
    //      added to CQuadWord(0, 0x80000000).
    //
    //      If the file is successfully created (the SafeFileCreate function returns a handle
    //      other than INVALID_HANDLE_VALUE), the 'allocateWholeFile' variable will be set
    //      to one of the following values:
    //        CQuadWord(0, 0x80000000): the file could not be preallocated and during the next
    //                                  call to SafeFileCreate for files to the same destination,
    //                                  'allocateWholeFile' should be NULL
    //        CQuadWord(0, 0):          the file could not be preallocated, but it is not fatal
    //                                  and when calling SafeFileCreate for files to this destination
    //                                  you can specify their preallocation
    //        other:                    preallocation was successful
    //                                  In this case, SAFE_FILE::WholeFileAllocated is set to TRUE
    //                                  and during SafeFileClose, SetEndOfFile is called to
    //                                  shorten the file and prevent unnecessary data storage.
    //
    //    'file'
    //      [out] Pointer to 'SAFE_FILE' structure that receives information about the opened
    //      file. This structure serves as a context for other methods from the SafeFile family.
    //      The values of the structure have meaning only if SafeFileCreate returns a value
    //      other than INVALID_HANDLE_VALUE. To close the file, call the SafeFileClose method.
    //      If 'file' is different from NULL, the SafeFileCreate method adds the handle of
    //      the created file to the HANDLES of Salamander. If 'file' is NULL, the handle
    //      will not be added to HANDLES. If 'isDir' is TRUE, the 'file' variable is ignored.
    //
    // Return Values
    //   If 'isDir' is TRUE, returns the value other than INVALID_HANDLE_VALUE in case of success.
    //   Beware, it is not a valid handle of the created directory. In case of failure, returns
    //   INVALID_HANDLE_VALUE and sets the 'silentMask', 'skipped' and 'skipPath' variables.
    //
    //   If 'isDir' is FALSE, returns the handle of the created file in case of success and if
    //   'file' is different from NULL, it fills in the SAFE_FILE structure.
    //   In case of failure, returns INVALID_HANDLE_VALUE and sets the 'silentMask', 'skipped'
    //   and 'skipPath' variables.
    //
    // Remarks
    //   The method can be called only from the main thread. (it can call the API FlashWindow
    //   (MainWindow), which must be called from the window thread, otherwise it will cause
    //   a deadlock)
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

    //
    // SafeFileClose
    //   Closes the file and releases the allocated data in the 'file' structure.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to the 'SAFE_FILE' structure that was initialized by a successful
    //      call to the SafeFileCreate or SafeFileOpen method.
    //
    // Remarks
    //   The method can be called from any thread.
    //
    virtual void WINAPI SafeFileClose(SAFE_FILE* file) = 0;

    //
    // SafeFileSeek
    //   Sets the pointer in the open file.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to the 'SAFE_FILE' structure that was initialized by a successful
    //      call to the SafeFileCreate or SafeFileOpen method.
    //
    //   'distance'
    //      [in/out] Number of bytes by which the pointer in the file should be moved.
    //      In case of success, it receives the value of the new pointer position.
    //
    //      The value of CQuadWord::Value is interpreted as signed for all three values
    //      of 'moveMethod' (beware of the error in MSDN in SetFilePointerEx, which states
    //      that the value is unsigned for FILE_BEGIN). If we want to move back from the
    //      current position (FILE_CURRENT) or from the end (FILE_END) of the file,
    //      we set CQuadWord::Value to a negative number. For example, __int64 can be
    //      assigned directly to the CQuadWord::Value variable.
    //
    //      The returned value is the absolute position from the beginning of the file
    //      and its values will be from 0 to 2^63. Files over 2^63 are not supported
    //      by any of the current Windows.
    //
    //   'moveMethod'
    //      [in] Initial position for the pointer. Can be one of the values:
    //           FILE_BEGIN, FILE_CURRENT or FILE_END.
    //
    //   'error'
    //      [out] Pointer to a DWORD variable that will contain the value returned from
    //      GetLastError in case of an error. 'error' can be NULL.
    //
    // Return Values
    //   Returns TRUE if successful and the value of the 'distance' variable is set
    //   to the new pointer position in the file.
    //
    //   Returns FALSE in case of an error and sets the value 'error' to GetLastError,
    //   if it is different from NULL. It does not display an error, this is what
    //   SafeFileSeekMsg is for.
    //
    // Remarks
    //   The method calls the API SetFilePointer, so the limitations of this function
    //   apply to it.
    //
    //   It is not an error to set the pointer beyond the end of the file. The file size
    //   does not increase until you call SetEndOfFile or SafeFileWrite. See API SetFilePointer.
    //
    //   The method can be used to get the size of the file if we set the 'distance'
    //   value to 0 and 'moveMethod' to FILE_END. The returned value of 'distance'
    //   will be the size of the file.
    //
    //   The method can be called from any thread.
    //
    virtual BOOL WINAPI SafeFileSeek(SAFE_FILE* file,
                                     CQuadWord* distance,
                                     DWORD moveMethod,
                                     DWORD* error) = 0;

    //
    // SafeFileSeekMsg
    //   Sets the pointer in the open file. If an error occurs, it displays an error.
    //
    // Parameters
    //   'file'
    //   'distance'
    //   'moveMethod'
    //      See the comment for SafeFileSeek.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages will be displayed.
    //      If it is set to HWND_STORED, the 'hParent' from the call of SafeFileOpen/SafeFileCreate
    //      will be used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values, specifies the buttons displayed in error messages.
    //
    //   'pressedButton'
    //      [out] Pointer to a variable that receives the pressed button during the error message.
    //      The variable has meaning only if the SafeFileSeekMsg method returns FALSE.
    //      'pressedButton' can be NULL (e.g. for BUTTONS_OK it does not make sense to test
    //      the pressed button).
    //
    //   'silentMask'
    //      [in/out] Pointer to a variable containing a bit array of SILENT_SKIP_xxx values.
    //      For more information see a comment for SafeFileOpen.
    //      The SafeFileSeekMsg tests and sets the SILENT_SKIP_FILE_READ bit if 'seekForRead'
    //      is TRUE or SILENT_SKIP_FILE_WRITE if 'seekForRead' is FALSE.
    //
    //    'seekForRead'
    //      [in] Specifies to the method for what purpose we performed the seek in the file.
    //      The method uses this variable only in case of an error. It determines which bit
    //      will be used for 'silentMask' and what will be the title of the error message:
    //      "Error Reading File" or "Error Writing File".
    //
    // Return Values
    //   If successful, returns TRUE and the value of the 'distance' variable is set
    //   to the new pointer position in the file.
    //
    //   In case of an error, returns FALSE and sets the values of the 'pressedButton'
    //   and 'silentMask' variables, if they are different from NULL.
    //
    // Remarks
    //   See method SafeFileSeek.
    //
    //   The method can be called from any thread.
    //
    virtual BOOL WINAPI SafeFileSeekMsg(SAFE_FILE* file,
                                        CQuadWord* distance,
                                        DWORD moveMethod,
                                        HWND hParent,
                                        DWORD flags,
                                        DWORD* pressedButton,
                                        DWORD* silentMask,
                                        BOOL seekForRead) = 0;

    //
    // SafeFileGetSize
    //   Returns the size of the file.
    //
    //   'file'
    //      [in] Pointer to the 'SAFE_FILE' structure that was initialized by a call
    //      of the SafeFileCreate or SafeFileOpen method.
    //
    //   'lpBuffer'
    //      [out] Pointer to a CQuadWord variable that receives the size of the file.
    //
    //   'error'
    //      [out] Pointer to a DWORD variable that will contain the value returned from
    //      GetLastError in case of an error. 'error' can be NULL.
    //
    // Return Values
    //   Returns TRUE if successful and sets the value 'fileSize'.
    //   In case of an error, returns FALSE and sets the value 'error', if it is different
    //   from NULL.
    //
    //
    // Remarks
    //   The method can be called from any thread.
    //
    virtual BOOL WINAPI SafeFileGetSize(SAFE_FILE* file,
                                        CQuadWord* fileSize,
                                        DWORD* error) = 0;

    //
    // SafeFileRead
    //   Reads data from the file starting at the pointer position. After the operation
    //   is completed, the pointer is moved by the number of bytes read. The method
    //   supports only synchronous reading, i.e. it does not return until the data is
    //   read or an error occurs.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to the 'SAFE_FILE' structure that was initialized by a call
    //      of the SafeFileCreate or SafeFileOpen method.
    //
    //    'lpBuffer'
    //      [out] Pointer to a buffer that receives the data read from the file.
    //
    //    'nNumberOfBytesToRead'
    //      [in] Specifies the number of bytes to read from the file.
    //
    //    'lpNumberOfBytesRead'
    //      [out] Pointer to a variable that receives the number of bytes read from the file.
    //
    //    'hParent'
    //      [in] Handle of the window to which error messages will be displayed.
    //      If it is set to HWND_STORED, the 'hParent' from the call of SafeFileOpen/SafeFileCreate
    //      will be used.
    //
    //    'flags'
    //      [in] One of the BUTTONS_xxx values, possibly combined with the SAFE_FILE_CHECK_SIZE
    //      flag, specifies the buttons displayed in error messages. If the SAFE_FILE_CHECK_SIZE
    //      flag is set, the SafeFileRead method considers it an error if it fails to read
    //      the requested number of bytes and displays an error message. Without this flag,
    //      it behaves the same as the ReadFile API.
    //
    //   'pressedButton'
    //   'silentMask'
    //      See SafeFileOpen.
    //
    // Return Values
    //   Returns TRUE if successful and sets the value 'lpNumberOfBytesRead' to the number
    //   of read bytes.
    //
    //   In case of an error, returns FALSE and sets the values of the 'pressedButton'
    //   and 'silentMask' variables, if they are different from NULL.
    //
    // Remarks
    //   The method can be called from any thread.
    //
    virtual BOOL WINAPI SafeFileRead(SAFE_FILE* file,
                                     LPVOID lpBuffer,
                                     DWORD nNumberOfBytesToRead,
                                     LPDWORD lpNumberOfBytesRead,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask) = 0;

    //
    // SafeFileWrite
    //   Writes data to the file starting at the pointer position. After the operation
    //   is completed, the pointer is moved by the number of bytes written. The method
    //   supports only synchronous writing, i.e. it does not return until the data is
    //   written or an error occurs.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to the 'SAFE_FILE' structure that was initialized by a call
    //      of the SafeFileCreate or SafeFileOpen method.
    //
    //    'lpBuffer'
    //      [in] Pointer to a buffer containing the data to be written to the file.
    //
    //    'nNumberOfBytesToWrite'
    //      [in] Specifies the number of bytes to write to the file.
    //
    //    'lpNumberOfBytesWritten'
    //      [out] Pointer to a variable that receives the number of bytes written to the file.
    //
    //    'hParent'
    //      [in] Handle of the window to which error messages will be displayed.
    //      If it is set to HWND_STORED, the 'hParent' from the call of SafeFileOpen/SafeFileCreate
    //      will be used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values, specifies the buttons displayed in error messages.
    //
    //   'pressedButton'
    //   'silentMask'
    //      See SafeFileOpen.
    //
    // Return Values
    //   If successful, returns TRUE and sets the value 'lpNumberOfBytesWritten' to the number
    //   of written bytes.
    //
    //   In case of an error, returns FALSE and sets the values of the 'pressedButton'
    //   and 'silentMask' variables, if they are different from NULL.
    //
    // Remarks
    //   The method can be called from any thread.
    //
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
