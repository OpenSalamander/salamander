// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "checksum.h"
#include "wrappers.h"
#include "misc.h"
#include "tomcrypt\tomcrypt.h"

class CCRCAlgo : public CHashAlgo
{
public:
    CCRCAlgo();
    ~CCRCAlgo();

    virtual bool IsOK(); // Was constructed successfully?
    virtual bool Init(); // Init for a new file. true on success
    virtual bool Update(const char* buf, DWORD size);
    virtual bool Finalize();
    virtual int GetDigest(char* buf, DWORD bufsize); // Returns # of copied binary bytes
    virtual bool ParseDigest(char* buf, char* fileName, int fileNameLen, char* digest);

private:
    DWORD crc;
};

class CGenericHashAlgo : public CHashAlgo
{
public:
    virtual bool ParseDigest(char* buf, char* fileName, int fileNameLen, char* digest);

protected:
    virtual const char* GetID() = 0;
    virtual int GetIDLen() = 0;
    virtual int GetDigestLen() = 0;
};

class CMD5Algo : public CGenericHashAlgo
{
public:
    CMD5Algo();
    ~CMD5Algo();

    virtual bool IsOK(); // Was constructed successfully?
    virtual bool Init(); // Init for a new file. true on success
    virtual bool Update(const char* buf, DWORD size);
    virtual bool Finalize();
    virtual int GetDigest(char* buf, DWORD bufsize); // Returns # of copied binary bytes

protected:
    virtual const char* GetID() { return "MD5"; };
    virtual int GetIDLen() { return 3; };      // strlen(GetID())
    virtual int GetDigestLen() { return 16; }; // ohlidat zda neni treba zvetsit DIGEST_MAX_SIZE!

private:
    CSalamanderMD5* md5;
};

class CSHA1Algo : public CGenericHashAlgo
{
public:
    CSHA1Algo();
    ~CSHA1Algo();

    virtual bool IsOK(); // Was constructed successfully?
    virtual bool Init(); // Init for a new file. true on success
    virtual bool Update(const char* buf, DWORD size);
    virtual bool Finalize();
    virtual int GetDigest(char* buf, DWORD bufsize); // Returns # of copied binary bytes

protected:
    virtual const char* GetID() { return "SHA1"; };
    virtual int GetIDLen() { return 4; };
    virtual int GetDigestLen() { return 20; }; // ohlidat zda neni treba zvetsit DIGEST_MAX_SIZE!

private:
    CSalSHA1 sha1;
};

class CSHA256Algo : public CGenericHashAlgo
{
public:
    CSHA256Algo();
    ~CSHA256Algo();

    virtual bool IsOK(); // Was constructed successfully?
    virtual bool Init(); // Init for a new file. true on success
    virtual bool Update(const char* buf, DWORD size);
    virtual bool Finalize();
    virtual int GetDigest(char* buf, DWORD bufsize); // Returns # of copied binary bytes

protected:
    virtual const char* GetID() { return "SHA256"; };
    virtual int GetIDLen() { return 6; };
    virtual int GetDigestLen() { return 32; }; // ohlidat zda neni treba zvetsit DIGEST_MAX_SIZE!

private:
    hash_state sha256;
};

class CSHA512Algo : public CGenericHashAlgo
{
public:
    CSHA512Algo();
    ~CSHA512Algo();

    virtual bool IsOK(); // Was constructed successfully?
    virtual bool Init(); // Init for a new file. true on success
    virtual bool Update(const char* buf, DWORD size);
    virtual bool Finalize();
    virtual int GetDigest(char* buf, DWORD bufsize); // Returns # of copied binary bytes

protected:
    virtual const char* GetID() { return "SHA512"; };
    virtual int GetIDLen() { return 6; };
    virtual int GetDigestLen() { return 64; }; // ohlidat zda neni treba zvetsit DIGEST_MAX_SIZE!

private:
    hash_state sha512;
};

CHashAlgo* CRCFactory()
{
    CCRCAlgo* pCalculator;

    pCalculator = new CCRCAlgo();
    if (pCalculator->IsOK())
    {
        return pCalculator;
    }
    delete pCalculator;
    return NULL;
}

CHashAlgo* MD5Factory()
{
    CMD5Algo* pCalculator;

    pCalculator = new CMD5Algo();
    if (pCalculator->IsOK())
    {
        return pCalculator;
    }
    delete pCalculator;
    return NULL;
}

CHashAlgo* SHA1Factory()
{
    CSHA1Algo* pCalculator;

    pCalculator = new CSHA1Algo();
    if (pCalculator->IsOK())
    {
        return pCalculator;
    }
    delete pCalculator;
    return NULL;
}

CHashAlgo* SHA256Factory()
{
    CSHA256Algo* pCalculator;

    pCalculator = new CSHA256Algo();
    if (pCalculator->IsOK())
    {
        return pCalculator;
    }
    delete pCalculator;
    return NULL;
}

CHashAlgo* SHA512Factory()
{
    CSHA512Algo* pCalculator;

    pCalculator = new CSHA512Algo();
    if (pCalculator->IsOK())
    {
        return pCalculator;
    }
    delete pCalculator;
    return NULL;
}

////////////////////////////// CRC algorithm ///////////////////////////

CCRCAlgo::CCRCAlgo()
{
}

CCRCAlgo::~CCRCAlgo()
{
}

bool CCRCAlgo::IsOK()
{
    return true;
}

bool CCRCAlgo::Init()
{
    crc = 0;
    return true;
}

bool CCRCAlgo::Update(const char* buf, DWORD size)
{
    crc = SalamanderGeneral->UpdateCrc32((void*)buf, size, crc);
    return true;
}

bool CCRCAlgo::Finalize()
{
    return true;
}

int CCRCAlgo::GetDigest(char* buf, DWORD bufsize)
{
    if (bufsize >= 4)
    {
        char* val = (char*)&crc;
        buf[0] = val[3];
        buf[1] = val[2];
        buf[2] = val[1];
        buf[3] = val[0];
        return 4;
    }
    else
    {
        TRACE_E("Small buffer size!");
    }
    return 0;
}

bool CCRCAlgo::ParseDigest(char* buf, char* fileName, int fileNameLen, char* digest)
{
    int pos, len;

    // POZOR: nasledujici kod musi byt v souladu s CVerifyDialog::AnalyzeSourceFile() !!!
    // checksum na konci (za ' ')

    GetLastWord(buf, pos, len);
    for (int i = 0; i < 4; i++) // delka digestu a hex znaky se kontroluji ve CVerifyDialog::AnalyzeSourceFile()
        digest[i] = (hex(buf[pos + 2 * i]) << 4) + hex(buf[pos + 2 * i + 1]);
    while (pos > 0 && (buf[pos - 1] == ' ' || buf[pos - 1] == '\t'))
        pos--;
    buf[pos] = 0;
    if ((int)strlen(buf) >= fileNameLen)
        return false; // too long name
    strcpy_s(fileName, fileNameLen, buf);
    return true;
}

//////////////////////////// Generic HASH algorithm /////////////////////////

bool CGenericHashAlgo::ParseDigest(char* buf, char* fileName, int fileNameLen, char* digest)
{
    int pos, len;

    // POZOR: nasledujici kod musi byt v souladu s CVerifyDialog::AnalyzeSourceFile() !!!
    // checksum na zacatku (pred ' ') nebo checksum na konci (za ' ' nebo '=') a zaroven
    // jmeno hashe na zacatku (pred '(' nebo ' ')

    GetFirstWord(buf, pos, len, '(');
    if (len == GetIDLen())
    {
        // MD5 (apache_2.0.46-win32-x86-symbols.zip) = eb5ba72b4164d765a79a7e06cee4eead
        GetLastWord(buf, pos, len, '=');
        for (int i = 0; i < GetDigestLen(); i++) // delka digestu a hex znaky se kontroluji ve CVerifyDialog::AnalyzeSourceFile()
            digest[i] = (hex(buf[pos + 2 * i]) << 4) + hex(buf[pos + 2 * i + 1]);
        // oriznu rovnitko a checksum pro snazsi vytazeni jmena souboru
        while (pos > 0 && (buf[pos - 1] == ' ' || buf[pos - 1] == '\t'))
            pos--;
        if (pos > 0 && buf[pos - 1] == '=')
            pos--;
        while (pos > 0 && (buf[pos - 1] == ' ' || buf[pos - 1] == '\t'))
            pos--;
        buf[pos] = 0;
        // vytazeni jmena souboru
        pos = GetIDLen();
        while ((buf[pos] == ' ') || (buf[pos] == '\t'))
            pos++;
        if (buf[pos] == '(')
        {
            pos++;
            // What if filename contains ')'???
            // "openssl md5 file().txt" produces: "MD5(file().txt)= 636e5618c32f05ac9f2623169527cd3c"
            len = (int)strlen(buf);
            if (len > 0 && buf[len - 1] == ')')
                buf[len - 1] = 0; // orizneme parujici ')'
            else
                pos--; // zavorka neparuje, takze uvodni zavorka je jen nahoda, vratime ji zpet do jmena souboru
        }
    }
    else
    {
        // aa8b248510531ff91a48e04c5d7ca939 *apache_2.0.44-win9x-x86-apr-patch.zip
        for (int i = 0; i < GetDigestLen(); i++) // delka digestu a hex znaky se kontroluji ve CVerifyDialog::AnalyzeSourceFile()
            digest[i] = (hex(buf[pos + 2 * i]) << 4) + hex(buf[pos + 2 * i + 1]);
        pos += len;
        while (buf[pos] && (buf[pos] == ' ' || buf[pos] == '\t'))
            pos++;
        if (buf[pos] == '*')
            pos++; // preskoceni hvezdicky oznacujici binarni soubor
    }
    if ((int)strlen(buf + pos) >= fileNameLen)
        return false; // too long name
    strcpy_s(fileName, fileNameLen, buf + pos);
    return true;
}

////////////////////////////// MD5 algorithm ///////////////////////////

CMD5Algo::CMD5Algo()
{
    md5 = SalamanderGeneral->AllocSalamanderMD5();
}

CMD5Algo::~CMD5Algo()
{
    if (md5)
        SalamanderGeneral->FreeSalamanderMD5(md5);
}

bool CMD5Algo::IsOK()
{
    if (md5 == NULL)
    {
        TRACE_E("CMD5Algo::IsOK(): Could not obtain inteface for MD5");
    }
    return md5 ? true : false;
}

bool CMD5Algo::Init()
{
    md5->Init();
    return true;
}

bool CMD5Algo::Update(const char* buf, DWORD size)
{
    md5->Update((void*)buf, size);
    return true;
}

bool CMD5Algo::Finalize()
{
    md5->Finalize();
    return true;
}

int CMD5Algo::GetDigest(char* buf, DWORD bufsize)
{
    if (bufsize >= 16)
    {
        md5->GetDigest(buf);
        return 16;
    }
    else
    {
        TRACE_E("Small buffer size!");
    }
    return 0;
}

////////////////////////////// SHA1 algorithm ///////////////////////////

CSHA1Algo::CSHA1Algo()
{
}

CSHA1Algo::~CSHA1Algo()
{
}

bool CSHA1Algo::IsOK()
{
    return true;
}

bool CSHA1Algo::Init()
{
    SalamanderCrypt->SHA1Init(&sha1);
    return true;
}

bool CSHA1Algo::Update(const char* buf, DWORD size)
{
    SalamanderCrypt->SHA1Update(&sha1, (BYTE*)buf, size);
    return true;
}

bool CSHA1Algo::Finalize()
{
    return true;
}

int CSHA1Algo::GetDigest(char* buf, DWORD bufsize)
{
    if (bufsize >= 20)
    {
        SalamanderCrypt->SHA1Final(&sha1, (LPBYTE)buf);
        return 20;
    }
    else
    {
        TRACE_E("Small buffer size!");
    }
    return 0;
}

////////////////////////////// SHA256 algorithm ///////////////////////////

CSHA256Algo::CSHA256Algo()
{
}

CSHA256Algo::~CSHA256Algo()
{
}

bool CSHA256Algo::IsOK()
{
    return true;
}

bool CSHA256Algo::Init()
{
    if (sha256_init(&sha256) != CRYPT_OK)
    {
        TRACE_E("sha256_process internal error!");
        return false;
    }
    return true;
}

bool CSHA256Algo::Update(const char* buf, DWORD size)
{
    if (sha256_process(&sha256, (const unsigned char*)buf, size) != CRYPT_OK)
    {
        TRACE_E("sha256_process internal error!");
        return false;
    }
    return true;
}

bool CSHA256Algo::Finalize()
{
    return true;
}

int CSHA256Algo::GetDigest(char* buf, DWORD bufsize)
{
#define SHA256_DIGEST_SIZE (256 / 8)
    if (bufsize >= SHA256_DIGEST_SIZE)
    {
        if (sha256_done(&sha256, (unsigned char*)buf) == CRYPT_OK)
            return SHA256_DIGEST_SIZE;
        else
            TRACE_E("sha256_done internal error!");
    }
    else
    {
        TRACE_E("Small buffer size!");
    }
    return 0;
}

////////////////////////////// SHA512 algorithm ///////////////////////////

CSHA512Algo::CSHA512Algo()
{
}

CSHA512Algo::~CSHA512Algo()
{
}

bool CSHA512Algo::IsOK()
{
    return true;
}

bool CSHA512Algo::Init()
{
    if (sha512_init(&sha512) != CRYPT_OK)
    {
        TRACE_E("sha512_init internal error!");
        return false;
    }
    return true;
}

bool CSHA512Algo::Update(const char* buf, DWORD size)
{
    if (sha512_process(&sha512, (const unsigned char*)buf, size) != CRYPT_OK)
    {
        TRACE_E("sha512_process internal error!");
        return false;
    }
    return true;
}

bool CSHA512Algo::Finalize()
{
    return true;
}

int CSHA512Algo::GetDigest(char* buf, DWORD bufsize)
{
#define SHA512_DIGEST_SIZE (512 / 8)
    if (bufsize >= SHA512_DIGEST_SIZE)
    {
        if (sha512_done(&sha512, (unsigned char*)buf) == CRYPT_OK)
            return SHA512_DIGEST_SIZE;
        else
            TRACE_E("sha512_done internal error!");
    }
    else
    {
        TRACE_E("Small buffer size!");
    }
    return 0;
}
