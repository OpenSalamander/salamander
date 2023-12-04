#ifndef _TYPES_H_
#define _TYPES_H_

/* i386, 32-bit, Windows */
#ifdef WIN32
typedef unsigned char UChar;
typedef __int16 Int16;
typedef unsigned __int16 UInt16;
typedef __int32 Int32;
typedef unsigned __int32 UInt32;
typedef __int64 Int64;
typedef unsigned __int64 UInt64;

/* I386, 32-bit, non-Windows */
/* Sparc        */
/* MIPS         */
/* PPC          */
#elif __i386__ || __sun || __sgi || __ppc__
typedef unsigned char UChar;
typedef short Int16;
typedef unsigned short UInt16;
typedef long Int32;
typedef unsigned long UInt32;
typedef long long Int64;
typedef unsigned long long UInt64;

/* x86-64 */
/* Note that these may be appropriate for other 64-bit machines. */
#elif __x86_64__ || __ia64__
typedef unsigned char UChar;
typedef short Int16;
typedef unsigned short UInt16;
typedef int Int32;
typedef unsigned int UInt32;
typedef long Int64;
typedef unsigned long UInt64;

#else

/* yielding an error is preferable to yielding incorrect behavior */
#error "Please define the sized types for your platform in chm_lib.c"
#endif

#ifdef WIN32
#ifdef __MINGW32__
#define __int64 long long
#endif
typedef unsigned __int64 LONGUINT64;
typedef __int64 LONGINT64;
#else
typedef unsigned long long LONGUINT64;
typedef long long LONGINT64;
#endif

/* structure representing an element from an ITS file stream   */
#define CHM_MAX_PATHLEN (512)
struct chmUnitInfo
{
    LONGUINT64 start;
    LONGUINT64 length;
    int space;
    int flags;
    char path[CHM_MAX_PATHLEN + 1];
};

struct chmLzxcResetTable
{
    UInt32 version;
    UInt32 block_count;
    UInt32 unknown;
    UInt32 table_offset;
    UInt64 uncompressed_len;
    UInt64 compressed_len;
    UInt64 block_len;
}; /* __attribute__ ((aligned (1))); */

/* the structure used for chm file handles */
struct chmFile
{
#ifdef WIN32
    HANDLE fd;
#else
    int fd;
#endif

#ifdef CHM_MT
#ifdef WIN32
    CRITICAL_SECTION mutex;
    CRITICAL_SECTION lzx_mutex;
    CRITICAL_SECTION cache_mutex;
#else
    pthread_mutex_t mutex;
    pthread_mutex_t lzx_mutex;
    pthread_mutex_t cache_mutex;
#endif
#endif

    UInt64 dir_offset;
    UInt64 dir_len;
    UInt64 data_offset;
    Int32 index_root;
    Int32 index_head;
    UInt32 block_len;

    UInt64 span;
    struct chmUnitInfo rt_unit;
    struct chmUnitInfo cn_unit;
    struct chmLzxcResetTable reset_table;

    /* LZX control data */
    int compression_enabled;
    UInt32 window_size;
    UInt32 reset_interval;
    UInt32 reset_blkcount;

    /* decompressor state */
    struct LZXstate* lzx_state;
    int lzx_last_block;

    /* cache for decompressed blocks */
    UChar** cache_blocks;
    UInt64* cache_block_indices;
    Int32 cache_num_blocks;
};

/* enumerate the objects in the .chm archive */
typedef int (*CHM_ENUMERATOR)(struct chmFile* h,
                              struct chmUnitInfo* ui,
                              void* context);
#define CHM_ENUMERATE_NORMAL (1)
#define CHM_ENUMERATE_META (2)
#define CHM_ENUMERATE_SPECIAL (4)
#define CHM_ENUMERATE_FILES (8)
#define CHM_ENUMERATE_DIRS (16)
#define CHM_ENUMERATE_ALL (31)
#define CHM_ENUMERATOR_FAILURE (0)
#define CHM_ENUMERATOR_CONTINUE (1)
#define CHM_ENUMERATOR_SUCCESS (2)

#endif //_TYPES_H_
