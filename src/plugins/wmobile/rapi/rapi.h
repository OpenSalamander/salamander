// --------------------------------------------------------------------------
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Module:
//
//     rapi.h
//
// Purpose:
//
//    Master include file for Windows CE Remote API
//
// --------------------------------------------------------------------------

#ifndef RAPI_H
#define RAPI_H

#include <windows.h>

//
// The Windows CE WIN32_FIND_DATA structure differs from the
// Windows WIN32_FIND_DATA stucture so we copy the Windows CE
// definition to here so that both sides match.
//
typedef struct _CE_FIND_DATA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwOID;
    WCHAR    cFileName[MAX_PATH];
} CE_FIND_DATA, *LPCE_FIND_DATA;

typedef CE_FIND_DATA** LPLPCE_FIND_DATA;

//
// These are flags for CeFindAllFiles
//
#define FAF_ATTRIBUTES      ((DWORD) 0x01)
#define FAF_CREATION_TIME   ((DWORD) 0x02)
#define FAF_LASTACCESS_TIME ((DWORD) 0x04)
#define FAF_LASTWRITE_TIME  ((DWORD) 0x08)
#define FAF_SIZE_HIGH       ((DWORD) 0x10)
#define FAF_SIZE_LOW        ((DWORD) 0x20)
#define FAF_OID             ((DWORD) 0x40)
#define FAF_NAME            ((DWORD) 0x80)
#define FAF_FLAG_COUNT      ((UINT)  8)
#define FAF_ATTRIB_CHILDREN ((DWORD)			0x01000)
#define FAF_ATTRIB_NO_HIDDEN ((DWORD)			0x02000)
#define FAF_FOLDERS_ONLY    ((DWORD)			0x04000)
#define FAF_NO_HIDDEN_SYS_ROMMODULES ((DWORD)	0x08000)
#define FAF_GETTARGET	    ((DWORD)			0x10000)

#define FAD_OID             ((WORD) 0x01)
#define FAD_FLAGS           ((WORD) 0x02)
#define FAD_NAME            ((WORD) 0x04)
#define FAD_TYPE            ((WORD) 0x08)
#define FAD_NUM_RECORDS     ((WORD) 0x10)
#define FAD_NUM_SORT_ORDER  ((WORD) 0x20)
#define FAD_SIZE            ((WORD) 0x40)
#define FAD_LAST_MODIFIED   ((WORD) 0x80)
#define FAD_SORT_SPECS      ((WORD) 0x100)
#define FAD_FLAG_COUNT      ((UINT) 9)

#ifndef FILE_ATTRIBUTE_INROM
#define FILE_ATTRIBUTE_INROM        0x00000040
#endif
#ifndef FILE_ATTRIBUTE_ROMSTATICREF
#define FILE_ATTRIBUTE_ROMSTATICREF 0x00001000
#endif
#ifndef FILE_ATTRIBUTE_ROMMODULE
#define FILE_ATTRIBUTE_ROMMODULE    0x00002000
#endif

//
// The following is not a standard Windows CE File Attribute.
//
#ifndef FILE_ATTRIBUTE_HAS_CHILDREN
#define FILE_ATTRIBUTE_HAS_CHILDREN 0x00010000
#endif
#ifndef FILE_ATTRIBUTE_SHORTCUT
#define FILE_ATTRIBUTE_SHORTCUT		0x00020000
#endif

#undef  INTERFACE
#define INTERFACE   IRAPIStream

typedef enum tagRAPISTREAMFLAG
{
	STREAM_TIMEOUT_READ
} RAPISTREAMFLAG;

DECLARE_INTERFACE_ (IRAPIStream,  IStream)
{
		STDMETHOD(SetRapiStat)( THIS_ RAPISTREAMFLAG Flag, DWORD dwValue) PURE;
		STDMETHOD(GetRapiStat)( THIS_ RAPISTREAMFLAG Flag, DWORD *pdwValue) PURE;
};

// RAPI extension on Windows CE (e.g., MyFunctionFOO) called via CeRapiInvoke should be declared as:
// EXTERN_C RAPIEXT MyFunctionFOO;
typedef  HRESULT (STDAPICALLTYPE RAPIEXT)(
		 DWORD			cbInput,			// [IN]
		 BYTE			*pInput,			// [IN]
		 DWORD			*pcbOutput,	 		// [OUT]
		 BYTE			**ppOutput,			// [OUT]
		 IRAPIStream	*pIRAPIStream		// [IN]
		 );

//
// The following definitions are for the client side only,
// because they are already defined on Windows CE.
//
#ifndef UNDER_CE

#include <stddef.h>

typedef struct STORE_INFORMATION {
	DWORD dwStoreSize;
	DWORD dwFreeSize;
} STORE_INFORMATION, *LPSTORE_INFORMATION;

typedef DWORD CEPROPID;
typedef CEPROPID *PCEPROPID;
#define TypeFromPropID(propid) LOWORD(propid)

typedef DWORD CEOID;
typedef CEOID *PCEOID;

typedef struct  _CEGUID {
    DWORD Data1;
	DWORD Data2;
	DWORD Data3;
	DWORD Data4;
}	CEGUID;
typedef CEGUID *PCEGUID;

typedef struct _CENOTIFICATION {
	DWORD dwSize;
	DWORD dwParam;
	UINT  uType;
	CEGUID guid;
	CEOID  oid;
	CEOID  oidParent;
} CENOTIFICATION;

#define CEDB_EXNOTIFICATION	0x00000001
typedef struct _CENOTIFYREQUEST {
    DWORD dwSize;
    HWND  hwnd;
    DWORD dwFlags;
    HANDLE hHeap;
	DWORD  dwParam;
} CENOTIFYREQUEST;
typedef CENOTIFYREQUEST *PCENOTIFYREQUEST;

typedef struct _CEFILEINFO {
    DWORD   dwAttributes;
    CEOID   oidParent;
    WCHAR   szFileName[MAX_PATH];
    FILETIME ftLastChanged;
    DWORD   dwLength;
} CEFILEINFO;

typedef struct _CEDIRINFO {
    DWORD   dwAttributes;
    CEOID   oidParent;
    WCHAR   szDirName[MAX_PATH];
} CEDIRINFO;

typedef struct _CERECORDINFO {
    CEOID  oidParent;
} CERECORDINFO;

#define CEDB_SORT_DESCENDING        0x00000001
#define CEDB_SORT_CASEINSENSITIVE   0x00000002
#define CEDB_SORT_UNKNOWNFIRST      0x00000004
#define CEDB_SORT_GENERICORDER      0x00000008

typedef struct _SORTORDERSPEC {
    CEPROPID  propid;
    DWORD     dwFlags;
} SORTORDERSPEC;

#define CEDB_MAXDBASENAMELEN  32
#define CEDB_MAXSORTORDER     4

#define CEDB_VALIDNAME      0x0001
#define CEDB_VALIDTYPE      0x0002
#define CEDB_VALIDSORTSPEC  0x0004
#define CEDB_VALIDMODTIME   0x0008
#define CEDB_VALIDDBFLAGS   0x0010
#define CEDB_VALIDCREATE (CEDB_VALIDNAME|CEDB_VALIDTYPE|CEDB_VALIDSORTSPEC|CEDB_VALIDDBFLAGS)

#define CEDB_NOCOMPRESS     0x00010000

typedef struct _CEDBASEINFO {
    DWORD   dwFlags;
    WCHAR   szDbaseName[CEDB_MAXDBASENAMELEN];
    DWORD   dwDbaseType;
    WORD    wNumRecords;
    WORD    wNumSortOrder;
    DWORD   dwSize;
    FILETIME ftLastModified;
    SORTORDERSPEC rgSortSpecs[CEDB_MAXSORTORDER];
} CEDBASEINFO;

typedef struct _CEDB_FIND_DATA {
    CEOID       OidDb;
    CEDBASEINFO DbInfo;
} CEDB_FIND_DATA, *LPCEDB_FIND_DATA;

typedef CEDB_FIND_DATA ** LPLPCEDB_FIND_DATA;

#define OBJTYPE_INVALID     0
#define OBJTYPE_FILE        1
#define OBJTYPE_DIRECTORY   2
#define OBJTYPE_DATABASE    3
#define OBJTYPE_RECORD      4

typedef struct _CEOIDINFO {
    WORD  wObjType;
    WORD  wPad;
    union {
        CEFILEINFO   infFile;
        CEDIRINFO    infDirectory;
        CEDBASEINFO  infDatabase;
        CERECORDINFO infRecord;
    };
} CEOIDINFO;

#define CEDB_AUTOINCREMENT      0x00000001

#define CEDB_SEEK_CEOID         0x00000001
#define CEDB_SEEK_BEGINNING     0x00000002
#define CEDB_SEEK_END           0x00000004
#define CEDB_SEEK_CURRENT       0x00000008
#define CEDB_SEEK_VALUESMALLER     0x00000010
#define CEDB_SEEK_VALUEFIRSTEQUAL  0x00000020
#define CEDB_SEEK_VALUEGREATER     0x00000040
#define CEDB_SEEK_VALUENEXTEQUAL   0x00000080

typedef struct _CEBLOB {
    DWORD       dwCount;
    LPBYTE      lpb;
} CEBLOB;

#define CEVT_I2       2
#define CEVT_UI2      18
#define CEVT_I4       3
#define CEVT_UI4      19
#define CEVT_FILETIME 64
#define CEVT_LPWSTR   31
#define CEVT_BLOB     65
#define CEVT_BOOL     11
#define	CEVT_R8       5

typedef union _CEVALUNION {
    short           iVal;
    USHORT          uiVal;
    long            lVal;
    ULONG           ulVal;
    FILETIME        filetime;
    LPWSTR          lpwstr;
    CEBLOB          blob;
    BOOL            boolVal;
    double          dblVal;
} CEVALUNION;
 
#define CEDB_PROPNOTFOUND 0x0100
#define CEDB_PROPDELETE   0x0200
typedef struct _CEPROPVAL {
    CEPROPID    propid;
    WORD        wLenData;
    WORD        wFlags;
    CEVALUNION  val;
} CEPROPVAL, *PCEPROPVAL;

#define CEDB_MAXDATABLOCKSIZE  4092
#define CEDB_MAXPROPDATASIZE   (CEDB_MAXDATABLOCKSIZE*16)
#define CEDB_MAXRECORDSIZE     (128*1024)

#define CEDB_ALLOWREALLOC  0x00000001

#define CREATE_SYSTEMGUID(pguid) (memset((pguid), 0, sizeof(CEGUID)))
#define CREATE_INVALIDGUID(pguid) (memset((pguid), -1, sizeof(CEGUID)))

#define CHECK_SYSTEMGUID(pguid) !((pguid)->Data1|(pguid)->Data2|(pguid)->Data3|(pguid)->Data4)
#define CHECK_INVALIDGUID(pguid) !~((pguid)->Data1&(pguid)->Data2&(pguid)->Data3&(pguid)->Data4)

#define SYSMEM_CHANGED	0
#define SYSMEM_MUSTREBOOT 1
#define SYSMEM_REBOOTPENDING 2
#define SYSMEM_FAILED 3

typedef struct _CEOSVERSIONINFO{
    DWORD dwOSVersionInfoSize; 
    DWORD dwMajorVersion; 
    DWORD dwMinorVersion; 
    DWORD dwBuildNumber; 
    DWORD dwPlatformId; 
    WCHAR szCSDVersion[ 128 ]; 
} CEOSVERSIONINFO, *LPCEOSVERSIONINFO;

#define AC_LINE_OFFLINE                 0x00
#define AC_LINE_ONLINE                  0x01
#define AC_LINE_BACKUP_POWER            0x02
#define AC_LINE_UNKNOWN                 0xFF

#define BATTERY_FLAG_HIGH               0x01
#define BATTERY_FLAG_LOW                0x02
#define BATTERY_FLAG_CRITICAL           0x04
#define BATTERY_FLAG_CHARGING           0x08
#define BATTERY_FLAG_NO_BATTERY         0x80
#define BATTERY_FLAG_UNKNOWN            0xFF

#define BATTERY_PERCENTAGE_UNKNOWN      0xFF

#define BATTERY_LIFE_UNKNOWN        0xFFFFFFFF

typedef struct _SYSTEM_POWER_STATUS_EX {
    BYTE ACLineStatus;
    BYTE BatteryFlag;
    BYTE BatteryLifePercent;
    BYTE Reserved1;
    DWORD BatteryLifeTime;
    DWORD BatteryFullLifeTime;
    BYTE Reserved2;
    BYTE BackupBatteryFlag;
    BYTE BackupBatteryLifePercent;
    BYTE Reserved3;
    DWORD BackupBatteryLifeTime;
    DWORD BackupBatteryFullLifeTime;
}   SYSTEM_POWER_STATUS_EX, *PSYSTEM_POWER_STATUS_EX, *LPSYSTEM_POWER_STATUS_EX;

//
// MessageId: CERAPI_E_ALREADYINITIALIZED
//
//  CeRapiInit(Ex) has already been successfully called
//
#define CERAPI_E_ALREADYINITIALIZED     0x80041001

typedef struct _RAPIINIT
{
    DWORD cbSize;
    HANDLE heRapiInit;
    HRESULT hrRapiInit;
} RAPIINIT;

STDAPI CeRapiInitEx(RAPIINIT*);
STDAPI CeRapiInit();
STDAPI CeRapiUninit();
STDAPI CeRapiGetError(void);
STDAPI CeRapiFreeBuffer(LPVOID);
STDAPI_( HRESULT ) CeRapiInvoke(LPCWSTR, LPCWSTR,DWORD,BYTE *, DWORD *,BYTE **, IRAPIStream **,DWORD);

STDAPI_(CEOID)  CeCreateDatabase     (LPWSTR, DWORD, WORD, SORTORDERSPEC*);
STDAPI_(BOOL  ) CeDeleteDatabase     (CEOID);
STDAPI_(BOOL  ) CeDeleteRecord       (HANDLE, CEOID);
STDAPI_(HANDLE) CeFindFirstDatabase  (DWORD);
STDAPI_(CEOID)  CeFindNextDatabase    (HANDLE);
STDAPI_(BOOL  ) CeOidGetInfo         (CEOID, CEOIDINFO*);
STDAPI_(HANDLE) CeOpenDatabase       (PCEOID, LPWSTR, CEPROPID, DWORD, HWND);
STDAPI_(CEOID)  CeReadRecordProps    (HANDLE, DWORD, LPWORD, CEPROPID*, LPBYTE*, LPDWORD);
STDAPI_(CEOID)  CeSeekDatabase       (HANDLE, DWORD, DWORD, LPDWORD);
STDAPI_(BOOL  ) CeSetDatabaseInfo    (CEOID, CEDBASEINFO*);
STDAPI_(CEOID)  CeWriteRecordProps   (HANDLE, CEOID, WORD, CEPROPVAL*);
STDAPI_(HANDLE) CeFindFirstFile      (LPCWSTR, LPCE_FIND_DATA);
STDAPI_(BOOL  ) CeFindNextFile       (HANDLE, LPCE_FIND_DATA);
STDAPI_(BOOL  ) CeFindClose          (HANDLE);
STDAPI_(DWORD ) CeGetFileAttributes  (LPCWSTR);
STDAPI_(BOOL  ) CeSetFileAttributes  (LPCWSTR, DWORD);
STDAPI_(HANDLE) CeCreateFile         (LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
STDAPI_(BOOL  ) CeReadFile           (HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
STDAPI_(BOOL  ) CeWriteFile          (HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
STDAPI_(BOOL  ) CeCloseHandle        (HANDLE);
STDAPI_(BOOL  ) CeFindAllFiles       (LPCWSTR, DWORD, LPDWORD, LPLPCE_FIND_DATA);
STDAPI_(BOOL  ) CeFindAllDatabases   (DWORD, WORD, LPWORD, LPLPCEDB_FIND_DATA);
STDAPI_(DWORD ) CeGetLastError       (void);
STDAPI_(DWORD ) CeSetFilePointer     (HANDLE, LONG, PLONG, DWORD);
STDAPI_(BOOL  ) CeSetEndOfFile       (HANDLE);
STDAPI_(BOOL  ) CeCreateDirectory    (LPCWSTR, LPSECURITY_ATTRIBUTES);
STDAPI_(BOOL  ) CeRemoveDirectory    (LPCWSTR);
STDAPI_(BOOL  ) CeCreateProcess      (LPCWSTR, LPCWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPWSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);
STDAPI_(BOOL  ) CeMoveFile           (LPCWSTR, LPCWSTR);
STDAPI_(BOOL  ) CeCopyFile           (LPCWSTR, LPCWSTR, BOOL);
STDAPI_(BOOL  ) CeDeleteFile         (LPCWSTR);
STDAPI_(DWORD ) CeGetFileSize        (HANDLE, LPDWORD);
STDAPI_(LONG  ) CeRegOpenKeyEx       (HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
STDAPI_(LONG  ) CeRegEnumKeyEx       (HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME);
STDAPI_(LONG  ) CeRegCreateKeyEx     (HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
STDAPI_(LONG  ) CeRegCloseKey        (HKEY);
STDAPI_(LONG  ) CeRegDeleteKey       (HKEY, LPCWSTR);
STDAPI_(LONG  ) CeRegEnumValue       (HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
STDAPI_(LONG  ) CeRegDeleteValue     (HKEY, LPCWSTR);
STDAPI_(LONG  ) CeRegQueryInfoKey    (HKEY, LPWSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPDWORD, LPDWORD, PFILETIME);
STDAPI_(LONG  ) CeRegQueryValueEx    (HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
STDAPI_(LONG  ) CeRegSetValueEx      (HKEY, LPCWSTR, DWORD, DWORD, LPBYTE, DWORD);
STDAPI_(BOOL  ) CeGetStoreInformation(LPSTORE_INFORMATION);
STDAPI_(INT   ) CeGetSystemMetrics   (INT);
STDAPI_(INT   ) CeGetDesktopDeviceCaps(INT);
STDAPI_(VOID  ) CeGetSystemInfo      (LPSYSTEM_INFO);
STDAPI_(DWORD ) CeSHCreateShortcut   (LPWSTR, LPWSTR);
STDAPI_(BOOL  ) CeSHGetShortcutTarget(LPWSTR, LPWSTR, INT);
STDAPI_(BOOL  ) CeCheckPassword      (LPWSTR);
STDAPI_(BOOL  ) CeGetFileTime        (HANDLE, LPFILETIME, LPFILETIME, LPFILETIME);
STDAPI_(BOOL  ) CeSetFileTime        (HANDLE, LPFILETIME, LPFILETIME, LPFILETIME);
STDAPI_(BOOL  ) CeGetVersionEx       (LPCEOSVERSIONINFO);
STDAPI_(HWND  ) CeGetWindow          (HWND, UINT);
STDAPI_(LONG  ) CeGetWindowLong      (HWND, int);
STDAPI_(int   ) CeGetWindowText      (HWND, LPWSTR, int);
STDAPI_(int   ) CeGetClassName       (HWND, LPWSTR, int);
STDAPI_(VOID  ) CeGlobalMemoryStatus (LPMEMORYSTATUS);
STDAPI_(BOOL  ) CeGetSystemPowerStatusEx(PSYSTEM_POWER_STATUS_EX, BOOL);
STDAPI_(DWORD ) CeGetTempPath        (DWORD, LPWSTR);
STDAPI_(DWORD ) CeGetSpecialFolderPath(int, DWORD, LPWSTR);
STDAPI_(HANDLE) CeFindFirstDatabaseEx (PCEGUID, DWORD);
STDAPI_(CEOID ) CeFindNextDatabaseEx (HANDLE, PCEGUID);
STDAPI_(CEOID ) CeCreateDatabaseEx   (PCEGUID, CEDBASEINFO*);
STDAPI_(BOOL  ) CeSetDatabaseInfoEx  (PCEGUID, CEOID, CEDBASEINFO*);
STDAPI_(HANDLE) CeOpenDatabaseEx     (PCEGUID, PCEOID, LPWSTR, CEPROPID, DWORD, CENOTIFYREQUEST *);
STDAPI_(BOOL  ) CeDeleteDatabaseEx   (PCEGUID, CEOID);
STDAPI_(CEOID ) CeReadRecordPropsEx  (HANDLE, DWORD, LPWORD, CEPROPID*, LPBYTE*, LPDWORD, HANDLE);
STDAPI_(CEOID ) CeWriteRecordProps   (HANDLE, CEOID, WORD, CEPROPVAL*);
STDAPI_(BOOL  ) CeMountDBVol         (PCEGUID, LPWSTR, DWORD);
STDAPI_(BOOL  ) CeUnmountDBVol       (PCEGUID);
STDAPI_(BOOL  ) CeFlushDBVol         (PCEGUID);
STDAPI_(BOOL  ) CeEnumDBVolumes      (PCEGUID, LPWSTR, DWORD);
STDAPI_(BOOL  ) CeOidGetInfoEx       (PCEGUID, CEOID, CEOIDINFO*);


#endif // #ifndef UNDER_CE

#include <ceapimap.h>

#ifdef CONN_INTERNAL
#include <prapi.h>  // internal defines
#endif

#endif // #ifndef RAPI_H
