#ifndef __BZIP_H__
#define __BZIP_H__

class CBZip: public CZippedFile
{
  public:
    CBZip(const char *filename, HANDLE file, unsigned char *buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    virtual ~CBZip();

    virtual BOOL BuggySize() { return TRUE; }

    static BOOL DetectArchive(const unsigned char *inBuffer, unsigned int inBufSize);

  protected:
    BOOL EndReached;          // set, when all data was extracted

    bz_stream *BZStream;
    virtual BOOL DecompressBlock(unsigned short needed);
};

#endif // __BZIP_H__
