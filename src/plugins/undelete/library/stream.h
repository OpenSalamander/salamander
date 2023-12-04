// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
NTFS Sparse Files
-----------------

The Sparse Files features of NTFS allow applications to create very large files 
consisting mostly of zeroed ranges without actually allocating LCNs (logical 
clusters) for the zeroed ranges.

Any attempt to access a "sparsed" range would result in NTFS returning a buffer
full of zeroes. Accessing an allocated range would result in a normal read of 
the allocated range. When data is written to a sparse file, an allocated range
is created which is exactly aligned with the compression unit boundaries 
containing the byte(s) written. Refer to the example below. If a single byte 
write occurs for virtual cluster number VCN 0x3a, then all of Compression Unit
3 (VCN 0x030-0x03f) would become an allocated LCN (logical cluster number) 
range. The allocated LCN range would be filled with zeroes and the single byte 
would be written to the appropriate byte offset within the target LCN.

[...] - ALLOCATED
(,,,) - Compressed
{   } - Sparse (or free) range
00000000000000000000000000000000000000000000000000000000000000000000000000000000 V
00000000000000001111111111111111222222222222222233333333333333334444444444444444 C
0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef N
      CU0             CU1             CU2             CU3             CU4
|++++++++++++++||++++++++++++++||++++++++++++++||++++++++++++++||++++++++++++++|
{                                              }[..............]{              }

VCN = 0x000 LEN = 0x30                 CU0 - CU2
VCN = 0x030 LEN = 0x10 : LCN = 0x2a21f CU3
VCN = 0x010 LEN = 0x10                 CU4


NTFS Compression
----------------

NTFS compresses files by dividing the data stream into CU's (this is similar
to how sparse files work). When the stream contents are created or changed,
each CU in the data stream is compressed individually.  If the compression
results in a reduction by one or more clusters, the compressed unit will be
written to disk in its compressed format. Then a sparse VCN range is tacked
to the end of the compressed VCN range for alignment purposes (as shown in
the example below). If the data does not compress enough to reduce the size
by one cluster, then the entire CU is written to disk in its uncompressed form.

In the example below, the compressed file consists of six sets of mapping pairs
(encoded file extents). Three allocated ranges co-exist with three sparse ranges.
The purpose of the sparse ranges is to maintain VCN alignment on compression unit
boundaries. This prevents NTFS from having to decompress the entire file if a
user wants to read a small byte range within the file. The first compression unit
(CU0) is compressed by 12.5% (which makes the allocated range smaller by 2 VCNs).
An additional free VCN range is added to the file extents to act as a placeholder
for the freed LCNs at the tail of the CU. The second allocated compression unit
(CU1) is similar to the first except that the CU compressed by roughly 50%.

NTFS was unable to compress CU2 and CU3, but part of CU4 was compressible by 69%.
For this reason, CU2 & CU3 are left uncompressed while CU4 is compressed from VCNs
0x40 to 0x44. Thus, CU2, CU3, and CU4 are a single run, but the run contains
a mixture of compressed & uncompressed VCNs.

NOTE: Each set of brackets represents a contiguous run of allocated or free space. 
One set of NTFS mapping pairs describes each set of brackets.

[...] - ALLOCATED
(,,,) - Compressed
{   } - Sparse (or free) range
00000000000000000000000000000000000000000000000000000000000000000000000000000000 V
00000000000000001111111111111111222222222222222233333333333333334444444444444444 C
0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef N
      CU0             CU1             CU2             CU3             CU4
|++++++++++++++||++++++++++++++||++++++++++++++||++++++++++++++||++++++++++++++|
(,,,,,,,,,,,,){}(,,,,,,){      }[...............................,,,,){         }

VCN = 0x000 LEN = 0x0e : LCN = 0x29e32d CU0
VCN = 0x00e LEN = 0x02                  CU0
VCN = 0x010 LEN = 0x08 : LCN = 0x2a291f CU1
VCN = 0x018 LEN = 0x08                  CU1
VCN = 0x020 LEN = 0x25 : LCN = 0x39dd49 CU2 - CU4
VCN = 0x045 LEN = 0x0b                  CU4

One compression unit could contain multiple sparsed runs, for example when data
runs are stored in multiple $DATA blocks.

00000000000000000000000000000000 V
00000000000000001111111111111111 C
0123456789abcdef0123456789abcdef N
      CU0             CU1
|++++++++++++++||++++++++++++++|
(,,,,){        }(,,,,,){}{     }

VCN = 0x000 LEN = 0x06 : LCN = 0x70AEFB CU0
VCN = 0x006 LEN = 0x0A                  CU0
VCN = 0x010 LEN = 0x07 : LCN = 0x7C84A2 CU1
VCN = 0x017 LEN = 0x02                  CU1
VCN = 0x019 LEN = 0x07                  CU1
*/

// flags for STANDARD_ATTRIBUTE_HEADER and for DATA_POINTERS
#define FLAG_COMPRESSED 0x0001
#define FLAG_ENCRYPTED 0x4000
#define FLAG_SPARSE 0x8000

template <typename CHAR>
class CStreamReader
{
public:
    CStreamReader()
    {
        ReadBuffer = NULL;
        UnpackBuffer = NULL;
    };
    ~CStreamReader() { Release(); }

    BOOL Init(CVolume<CHAR>* volume, DATA_STREAM_I<CHAR>* str);
    // when 'numRead' is not NULL, it will return number of read clusters or -1 when special error occurred
    BOOL GetClusters(BYTE* buffer, QWORD num, QWORD* numRead = NULL);
    BOOL IsSafeChunk(QWORD n) { return RunLength >= n; }
    void Release();

private:
    BOOL GetUncompressedClusters(BYTE* buffer, QWORD num, QWORD* numRead);
    BOOL GetCompressedClusters(BYTE* buffer, QWORD num, QWORD* numRead);
    int DecompressBlock(const BYTE* src, DWORD len, BYTE* dest, DWORD destmax);

    CVolume<CHAR>* Volume;
    const DATA_STREAM_I<CHAR>* Stream;
    CRunsWalker<CHAR> RunsWalker;
    BOOL SparseRun;  // is current run sparse?
    QWORD RunLength; // current run length in bytes
    QWORD CurLCN;    // logical cluster number inside current run
    QWORD Read;      // total read bytes from beginning of the stream

    // NTFS-compressed stream related values
    BOOL CompressedStream; // is any of data runs set compressed?
    BYTE* ReadBuffer;      // read buffer
    BYTE* UnpackBuffer;    // unpacking buffer
    QWORD BufferSize;      // size of read and unpack buffers in bytes
    QWORD CULeft;          // compression unit left
    QWORD CUClusters;      // compression unit in clusters
};

template <typename CHAR>
struct EFIC_CONTEXT
{
public:
    typedef BOOL(WINAPI* PROGRESS_PROC)(int, void*);

    EFIC_CONTEXT(CVolume<CHAR>* volume, DATA_STREAM_I<CHAR>* stream, BYTE* buffer,
                 int bufsize, PROGRESS_PROC proc, void* procctx)
        : Volume(volume), State(0), FirstStream(stream), Buffer(buffer),
          BufSize(bufsize), ProgressProc(proc), ProcCtx(procctx) {}

    static DWORD WINAPI EncryptedFileImportCallback(PBYTE data, PVOID context, PULONG length);

private:
    CVolume<CHAR>* Volume;
    int State;
    int SubState;
    DATA_STREAM_I<CHAR>* FirstStream;
    DATA_STREAM_I<CHAR>* CurrentStream;
    CStreamReader<CHAR> Reader;
    int BufSize;
    int BufPos;
    int BufData;
    int RawSize;
    int BlockSize;
    int BlockNum;
    BYTE* Buffer;
    PROGRESS_PROC ProgressProc;
    void* ProcCtx;

    static const CHAR* const STRING_EFS;
};

// **************************************************************************************
//
//   CStreamReader
//

template <typename CHAR>
BOOL CStreamReader<CHAR>::Init(CVolume<CHAR>* volume, DATA_STREAM_I<CHAR>* str)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CStreamReader::Init(, )");

    Release();
    Volume = volume;
    Stream = str;
    Read = 0;
    if (str->Ptrs == NULL)
        return TRUE;

    RunsWalker.Init(str->Ptrs);

    RunLength = 0;
    CurLCN = 0;

    // first data runs set could be uncompressed and CompUnit will be 0, so we must search all fragments
    // we will also look out for compressed flag
    DATA_POINTERS* p = str->Ptrs;
    QWORD maxCompUnit = 0;
    CompressedStream = FALSE;
    while (p != NULL)
    {
        maxCompUnit = max(maxCompUnit, p->CompUnit);
        CompressedStream |= (p->DPFlags & FLAG_COMPRESSED) != 0;
        p = p->DPNext;
    }

    // "compressed" code path is slower compared to "uncompressed" path, use it only for streams with compressed units
    if (CompressedStream)
    {
        // the default size of the compression unit is 16 clusters, although the actual size depends on the cluster size
        CUClusters = (QWORD)1 << maxCompUnit;
        BufferSize = CUClusters * Volume->BytesPerCluster;
        ReadBuffer = new BYTE[(size_t)BufferSize];
        UnpackBuffer = new BYTE[(size_t)BufferSize];
        if (ReadBuffer == NULL || UnpackBuffer == NULL)
        {
            if (ReadBuffer != NULL)
                delete[] ReadBuffer;
            if (UnpackBuffer != NULL)
                delete[] UnpackBuffer;
            ReadBuffer = NULL;
            UnpackBuffer = NULL;
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        }
        SparseRun = FALSE;
        CULeft = 0;
    }
    return TRUE;
}

template <typename CHAR>
void CStreamReader<CHAR>::Release()
{
    if (ReadBuffer)
    {
        delete[] ReadBuffer;
        ReadBuffer = NULL;
    }
    if (UnpackBuffer)
    {
        delete[] UnpackBuffer;
        UnpackBuffer = NULL;
    }
}

template <typename CHAR>
BOOL CStreamReader<CHAR>::GetClusters(BYTE* buffer, QWORD num, QWORD* numRead)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CStreamReader::GetClusters(, %d)", num);

    // in case of resident data we will just return them
    if (Stream->ResidentData != NULL)
    {
        QWORD requiredNum = num;
        ZeroMemory(buffer, (size_t)(num * Volume->BytesPerCluster)); // don't leave end of cluster random
        if (Read < Stream->DSSize)
        {
            QWORD readBytes = min(num * Volume->BytesPerCluster, Stream->DSSize - Read);
            memcpy(buffer, Stream->ResidentData + Read, (size_t)readBytes);
            Read += readBytes;
            num = (readBytes + Volume->BytesPerCluster - 1) / Volume->BytesPerCluster;
        }
        else
        {
            TRACE_E("GetClusters() attempt to read beyond end of file");
            num = 0;
        }
        if (numRead != NULL)
            *numRead = num;
        return num == requiredNum;
    }

    if (CompressedStream)
    {
        if (!GetCompressedClusters(buffer, num, numRead))
            return FALSE;
    }
    else
    {
        if (!GetUncompressedClusters(buffer, num, numRead))
            return FALSE;
    }

    return TRUE;
}

template <typename CHAR>
BOOL CStreamReader<CHAR>::GetUncompressedClusters(BYTE* buffer, QWORD num, QWORD* numRead)
{
    CALL_STACK_MESSAGE_NONE

    QWORD requiredNum = num;
    do
    {
        // do we need to get next pair?
        if (RunLength == 0)
        {
            QWORD lcn;
            if (!RunsWalker.GetNextRun(&lcn, &RunLength, NULL))
            {
                if (numRead != NULL)
                    *numRead = requiredNum - num;
                return FALSE;
            }
            SparseRun = (lcn == -1);
            if (!SparseRun)
                CurLCN = lcn;
        }

        // read clusters data
        QWORD n = min(RunLength, num);
        if (!SparseRun)
        {
            // read clusters up to stream DSValidSize
            if (Read < Stream->DSValidSize)
            {
                if (!Volume->ReadClusters(buffer, CurLCN, n))
                {
                    if (numRead != NULL)
                        *numRead = requiredNum - num;
                    return FALSE;
                }
            }
            // for unencrypted streams zero range beyond DSValidSize
            if ((Stream->Ptrs->DPFlags & FLAG_ENCRYPTED) == 0 &&
                Read + n * Volume->BytesPerCluster > Stream->DSValidSize)
            {
                QWORD tailBytes = Read + n * Volume->BytesPerCluster - Stream->DSValidSize;
                tailBytes = min(tailBytes, n * Volume->BytesPerCluster);
                ZeroMemory(buffer + n * Volume->BytesPerCluster - tailBytes, (size_t)tailBytes);
            }
        }
        else
        {
            // sparse run
            memset(buffer, 0, (size_t)(n * Volume->BytesPerCluster));
        }

        num -= n;
        QWORD size = n * Volume->BytesPerCluster;
        buffer += size;
        Read += size;
        RunLength -= n;
        CurLCN += n;
    } while (num);

    if (numRead != NULL)
        *numRead = requiredNum;
    return TRUE;
}

template <typename CHAR>
BOOL CStreamReader<CHAR>::GetCompressedClusters(BYTE* buffer, QWORD num, QWORD* numRead)
{
    CALL_STACK_MESSAGE_NONE

    const BYTE* bufferPtr = NULL;
    QWORD requiredNum = num;
    while (num > 0)
    {
        // do we need to read next compression unit?
        if (CULeft == 0)
        {
            // we assume compressed unit
            BOOL compressed = TRUE;
            QWORD datasize = 0;
            BOOL sparseInsideCU = FALSE;
            while (CULeft < CUClusters)
            {
                // take next run if needed
                if (RunLength == 0)
                {
                    QWORD lcn;
                    if (!RunsWalker.GetNextRun(&lcn, &RunLength, NULL))
                    {
                        if (numRead != NULL)
                            *numRead = 0;
                        return FALSE;
                    }
                    SparseRun = (lcn == -1);
                    if (!SparseRun)
                        CurLCN = lcn;
                }

                // read clusters / fill with zeros for sparse
                QWORD n = min(RunLength, (QWORD)(CUClusters - CULeft));
                BYTE* bufpos = ReadBuffer + CULeft * Volume->BytesPerCluster;
                if (!SparseRun)
                {
                    if (Read < Stream->DSValidSize)
                    {
                        if (!Volume->ReadClusters(bufpos, CurLCN, n))
                        {
                            if (numRead != NULL)
                                *numRead = requiredNum - num;
                            return FALSE;
                        }
                    }
                    else
                    {
                        ZeroMemory(bufpos, (size_t)(n * Volume->BytesPerCluster));
                        compressed = FALSE;
                    }
                    CurLCN += n;
                    datasize += n * Volume->BytesPerCluster;
                }
                else
                {
                    memset(bufpos, 0, (size_t)(n * Volume->BytesPerCluster));
                    // it is not necessary to zero last spare but when error in data occurs
                    // we will return raw data and don't want to have mess there
                }
                CULeft += n;
                RunLength -= n;

                if (SparseRun)
                {
                    // when sparse run goes through whole unit, then unit is not compressed
                    if (n == CUClusters)
                        compressed = FALSE;
                    if (CULeft < CUClusters)
                        sparseInsideCU = TRUE;
                }
                else
                {
                    // when there is sparse run inside unit, followed by non-sparsed run, then unit is not compressed
                    if (CULeft < CUClusters && sparseInsideCU)
                        compressed = FALSE;
                }
            }

            // if unit doesn't end with sparse run, it is not compressed
            if (!SparseRun)
                compressed = FALSE;

            // decompress unit
            if (compressed)
            {
                BYTE* src = ReadBuffer;
                BYTE* dest = UnpackBuffer;
                int ret = -1;
                while (src < ReadBuffer + datasize)
                {
                    WORD blockHeader = *((WORD*)src);
                    DWORD len = blockHeader & 0x0FFF;
                    if (len == 0)
                    {
                        ZeroMemory(dest, (size_t)(BufferSize - (dest - UnpackBuffer)));
                        break;
                    }
                    len++;
                    src += 2;
                    if (len < 4096)
                    {
#ifdef _DEBUG
                        // compressed block must have 0x8000 bit set
                        if ((blockHeader & 0x8000) == 0)
                            TRACE_E("Block is not compressed!");
#endif
                        ret = DecompressBlock(src, len, dest, (DWORD)(UnpackBuffer + BufferSize - dest));
                        if (ret == -1)
                        {
                            TRACE_E("Corrupted compression unit...");
                            break;
                        }
                        dest += ret;
                    }
                    else
                    {
                        // block with size 4096 is not compressed
                        memcpy(dest, src, 4096);
                        dest += 4096;
                    }
                    src += len;
                }
                bufferPtr = (ret != -1) ? UnpackBuffer : ReadBuffer;
            }
            else
            {
                bufferPtr = ReadBuffer; // uncompressed unit: take raw data
            }
        }

        if (bufferPtr == NULL)
        {
            TRACE_E("Unexpected situation: bufferPtr == NULL");
            if (numRead != NULL)
                *numRead = requiredNum - num;
            return FALSE;
        }

        // copy decompressed data into target buffer
        QWORD n = min(num, CULeft);
        QWORD size = n * Volume->BytesPerCluster;
        memcpy(buffer, bufferPtr, (size_t)size);
        num -= n;
        buffer += size;
        Read += size;
        bufferPtr += size;
        CULeft -= n;
    }

    if (numRead != NULL)
        *numRead = requiredNum;
    return TRUE;
}

template <typename CHAR>
int CStreamReader<CHAR>::DecompressBlock(const BYTE* src, DWORD len, BYTE* dest, DWORD destmax)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE3("CStreamReader::DecompressBlock( , %d, , %d)", len, destmax);

    DWORD i = 0;
    DWORD j = 0;
    DWORD mask;
    const BYTE* max = src + len;
    DWORD ref;
    DWORD lmask = 0xFFF;
    DWORD dshift = 12;
    DWORD disp;
    DWORD l;
    DWORD amask = ~0xF;

    while (src < max)
    {
        while (((j - 1) & amask) && j)
        {
            lmask >>= 1;
            dshift--;
            amask <<= 1;
        }

        if (!(i++ & 7))
            mask = *src++;

        if (mask & 1)
        {
            ref = *((unsigned short*)src);
            src += 2;
            l = (ref & lmask) + 3;
            disp = (ref >> dshift) + 1;
            do
            {
                if (j >= destmax || j < disp)
                    return -1;
                dest[j++] = dest[j - disp];
            } while (--l);
        }
        else
        {
            if (j >= destmax)
                return -1;
            dest[j++] = *src++;
        }

        mask >>= 1;
    }

    return j;
}

// **************************************************************************************
//
//   EncryptedFileImportCallback
//

/* 
NTFS Encryption
---------------

How does it work: encrypted file is normal files (with ADS etc), but has attribute
ENCRYPTED and special stream with name $EFS. This special stream is not visible because
it has different number than normal data stream. Inside $EFS are stored information about
users whose have access to file and universal password encrypted with users keys.

How does backup ReadEncryptedFileRaw) work: goal is store all streams of encrypted
file into one. It is done by storing them one after one, divided by headers etc.
First is stored $EFS, then default stream, then first ADS, second ADS, etc.

Restore from backup: WriteEncryptedFileRaw takes this package and step by step creates
all parts of encrypted file exactly as they were. For backup/restore we don't need
to know keys, we are working with raw data only. If user stores keys through system
re installation, it is nice, but for backup software it doesn't matter. It restores
files 1:1.

In theory it would be possible to do undelete this way: restore all streams of file,
set attribute FILE_ATTRIBUTE_ENCRYPTED and recover special $EFS. Unfortunately it is
not possible with standard API, so we need to simulate ReadEncryptedFileRaw function
and restore file through WriteEncryptedFileRaw.

Here is output of ReadEncryptedFileRaw (file exported_ntfs.dat,
created by reading encrypted ntfs.cpp):

- [0000-0014) : file header

- [0014-0032) : block containing stream name
  - [0014-0018) - length of whole block
  - [0018-0020) - "NTFS" signature
  - [0020-002C) - unknown
  - [002C-0030) - length of next stream name
  - [0030-0032) - bytes 10 19, probably short name for $EFS

- [0032-025A) : block containing data of $EFS stream
  - [0032-0036) - length of whole block
  - [0036-003E) - "GURE" signature
  - [003E-0042) - unknown
  - [0042-025A) - $EFS stream, we will put concrete data here

- [025A-284) : "NTFS" block containing name of next stream (::$DATA)

- [0284-8284) : "GURE" block with raw streams data ::$DATA
  - [0284-0288) - length of block
  - [0288-0290) - signature
  - [029C-02A0) - before raw data there is header, this is header length
  - [02A0-02A8) - 2x repeated real length of decrypted stream (why 2x?)
  - [02A8-02AC) - unknown, differs for each file
  - [02AC-02B0) - unknown, constant (id of encrypted method?)
  - [02B0-02B4) - length of raw data - seems 512-bytes aligned compared to length of plain data
  - [02B4-0484) - padding to 512 (from block beginning)
  - [0484-8284) - raw data, we will put concrete data here

- [8284-82B8) : "NTFS" block with name of first ADS

- [82B8-94B8) : "GURE" block with constant of first ADS

Addition to format of backuped data: GURE vlock can contains max 65536 bytes.
If stream is larger, more GURE blocks must be created (will be placed consecutively).
We still don't know "unknown" fields meaning, it looks like they are changing with
Windows versions. Need to be tested, some errors in data passed to function
WriteEncryptedFileRaw could cause exception is lsass.exe and system restart
after 60 seconds countdown
*/

extern char Junk_ROBS[20];
extern char Junk_NTFS[20];
extern char Junk_GURE[12];

#define OUTPUT_INT(x) \
    { \
        *((int*)(data + pos)) = (x); \
        pos += sizeof(int); \
    }
#define OUTPUT_JUNK(x) \
    { \
        memcpy(data + pos, (x), sizeof(x)); \
        pos += sizeof(x); \
    }
#define OUTPUT_WCHAR(x) \
    { \
        *((WCHAR*)(data + pos)) = (x); \
        pos += sizeof(WCHAR); \
    }
#define OUTPUT_WSTR(x, l) \
    { \
        memcpy(data + pos, (x), (l) * sizeof(WCHAR)); \
        pos += (l) * sizeof(WCHAR); \
    }
#define OUTPUT_ZEROS(l) \
    { \
        memset(data + pos, 0, (l)); \
        pos += (l); \
    }

template <typename CHAR>
DWORD WINAPI EFIC_CONTEXT<CHAR>::EncryptedFileImportCallback(PBYTE data, PVOID context, PULONG length)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("EncryptedFileImportCallback( , , %u)", *length);

    EFIC_CONTEXT<CHAR>* c = (EFIC_CONTEXT<CHAR>*)context;
    int pos = 0;

    if (c->State == 0)
    {
        // beginning: header of whole backup
        OUTPUT_JUNK(Junk_ROBS);
        *length = pos;
        c->State = 1;
        c->SubState = 0;
    }
    else
    {
        BOOL efs = (c->State == 1);

        // not beginning: each stream has 3 "sub-states" - NTFS block, GURE block, and raw data
        if (c->SubState == 0)
        {
            DATA_STREAM_I<CHAR>* s = c->FirstStream;
            WCHAR sname[MAX_PATH];
            int nlen;
            int blen;

            // find proper stream, 1=$EFS, 2=data1, 3=data2...
            if (efs)
            {
                while (s != NULL && (s->DSName == NULL || String<CHAR>::StrICmp(s->DSName, STRING_EFS)))
                    s = s->DSNext;
                if (s == NULL)
                    return ERROR_FILE_NOT_FOUND; // fixme
                sname[0] = 0x1910;
                nlen = 1;
            }
            else
            {
                for (int i = 0; s != NULL; s = s->DSNext)
                {
                    if (s->DSName == NULL || String<CHAR>::StrICmp(s->DSName, STRING_EFS))
                    {
                        if (i++ >= c->State - 2)
                            break;
                    }
                }
                if (s == NULL)
                {
                    *length = 0; // out of streams, end
                    return ERROR_SUCCESS;
                }

                if (s->DSName != NULL)
                {
                    if (!String<CHAR>::CopyToUnicode(sname, s->DSName, (unsigned int)String<CHAR>::StrLen(s->DSName), MAX_PATH))
                        return ERROR_INVALID_DATA;
                    nlen = (int)wcslen(sname);
                }
                else
                {
                    nlen = 0;
                }
            }
            c->CurrentStream = s;

            // output "NTFS" block
            blen = (nlen + (efs ? 0 : 7)) * sizeof(WCHAR);
            OUTPUT_INT(sizeof(Junk_NTFS) + 2 * sizeof(int) + blen);
            OUTPUT_JUNK(Junk_NTFS);
            OUTPUT_INT(blen);
            if (!efs)
                OUTPUT_WCHAR(L':');
            OUTPUT_WSTR(sname, nlen);
            if (!efs)
                OUTPUT_WSTR(L":$DATA", 6);
            *length = pos;

            if (c->CurrentStream->DSSize)
            {
                c->Reader.Init(c->Volume, c->CurrentStream); // fixme: ret
                c->RawSize = (int)c->CurrentStream->DSSize;
                c->BufPos = 0;
                c->BufData = 0;
                c->BlockNum = 0;
                c->SubState = 1;
            }
            else
            {
                c->State++;
                c->SubState = 0;
            }
        }
        else if (c->SubState == 1) // block "GURE"
        {
            if (efs)
            {
                c->BlockSize = c->RawSize;
                OUTPUT_INT(sizeof(int) + sizeof(Junk_GURE) + c->RawSize);
                OUTPUT_JUNK(Junk_GURE);
            }
            else
            {
                int realsize;
                if (c->RawSize <= 0x10000)
                {
                    c->BlockSize = (c->RawSize + 0x1FF) & ~0x1FF; // 512-byte alignment
                    realsize = c->RawSize;
                }
                else
                {
                    c->BlockSize = 0x10000;
                    realsize = c->BlockSize;
                }
                OUTPUT_INT(0x200 + c->BlockSize);
                OUTPUT_JUNK(Junk_GURE);
                OUTPUT_INT(c->BlockNum << 16);
                OUTPUT_INT(0);
                OUTPUT_INT(0x1F0);
                OUTPUT_INT(realsize);
                OUTPUT_INT(realsize);
                OUTPUT_INT(0);
                OUTPUT_INT(0x01010C);
                OUTPUT_INT(c->BlockSize);
                OUTPUT_ZEROS(0x1D0);
            }
            *length = pos;

            c->SubState = 2;
        }
        else // raw stream
        {
            int nc;
            int copied = 0;

            while ((ULONG)copied < *length && !(c->BufPos >= c->BufData && c->BlockSize <= 0))
            {
                if (c->BufPos >= c->BufData)
                {
                    nc = (c->BlockSize - 1) / c->Volume->BytesPerCluster + 1;
                    int bc = c->BufSize / c->Volume->BytesPerCluster;
                    if (bc <= 0)
                        TRACE_E("EncryptedFileImportCallback: buffer size is smaller than one cluster!!!");
                    if (nc > bc)
                        nc = bc;
                    c->Reader.GetClusters(c->Buffer, nc); // fixme: ret
                    c->BufData = min((ULONG)c->BlockSize, nc * c->Volume->BytesPerCluster);
                    c->BlockSize -= c->BufData;
                    c->BufPos = 0;
                }

                nc = min((ULONG)(c->BufData - c->BufPos), *length - copied);
                memcpy(data + copied, c->Buffer + c->BufPos, nc);
                c->BufPos += nc;
                copied += nc;
            }
            *length = copied;

            if (c->ProgressProc && c->ProgressProc(copied, c->ProcCtx))
            {
                c->Reader.Release();
                *length = 0;
                return ERROR_CANCELLED;
            }

            if (c->BufPos >= c->BufData && c->BlockSize <= 0) // all block data processed?
            {
                if (c->RawSize > 0x10000) // another block?
                {
                    c->RawSize -= 0x10000;
                    c->BlockNum++;
                    c->SubState = 1; // again
                }
                else
                {
                    c->Reader.Release(); // end, next stream
                    c->State++;
                    c->SubState = 0;
                }
            }
        }
    }

    return ERROR_SUCCESS;
}
