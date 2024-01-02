#ifndef __BZIP_H__
#define __BZIP_H__

#include <bzlib.h>

class CBZip : public CZippedFile
{
public:
    CBZip(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    ~CBZip() override;

    BOOL BuggySize() const override { return TRUE; }

protected:
    BOOL EndReached{FALSE}; // set, when all data was extracted

    bz_stream* BZStream{};
    BOOL DecompressBlock(unsigned short needed) override;
};

#endif // __BZIP_H__
