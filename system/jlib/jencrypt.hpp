/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */


#ifndef JENCRYPT_HPP
#define JENCRYPT_HPP

#include "platform.h"
#include "jiface.hpp"
#include "jbuff.hpp"
#include "jexcept.hpp"

//for AES, keylen must be 16, 24, or 32 Bytes

namespace jlib
{
    extern jlib_decl MemoryBuffer &aesEncrypt(const void *key, size_t keylen, const void *input, size_t inlen, MemoryBuffer &output);
    extern jlib_decl MemoryBuffer &aesDecrypt(const void *key, size_t keylen, const void *input, size_t inlen, MemoryBuffer &output);
    extern jlib_decl size_t aesEncryptInPlace(const void *key, size_t keylen, void *data, size_t inlen, size_t buflen);
    extern jlib_decl size_t aesDecryptInPlace(const void *key, size_t keylen, void *data, size_t inlen);
} // end of namespace jlib;

#ifdef _USE_OPENSSL
namespace openssl
{
    extern jlib_decl MemoryBuffer &aesEncrypt(const void *key, size_t keylen, const void *input, size_t inlen, MemoryBuffer &output);
    extern jlib_decl MemoryBuffer &aesDecrypt(const void *key, size_t keylen, const void *input, size_t inlen, MemoryBuffer &output);
    extern jlib_decl size_t aesEncryptInPlace(const void *key, size_t keylen, void *data, size_t inlen, size_t buflen);
    extern jlib_decl size_t aesDecryptInPlace(const void *key, size_t keylen, void *data, size_t inlen);
} // end of namespace openssl;
#endif

// NB: these are wrappers to either the openssl versions (if USE_OPENSSL) or the jlib version.

extern jlib_decl MemoryBuffer &aesEncrypt(const void *key, size_t keylen, const void *input, size_t inlen, MemoryBuffer &output);
extern jlib_decl MemoryBuffer &aesDecrypt(const void *key, size_t keylen, const void *input, size_t inlen, MemoryBuffer &output);
extern jlib_decl size_t aesEncryptInPlace(const void *key, size_t keylen, void *data, size_t inlen, size_t buflen);
extern jlib_decl size_t aesDecryptInPlace(const void *key, size_t keylen, void *data, size_t inlen);

//Control whether to use the legacy jlib implementation of the AES functions, rather than openSSL when available.
extern jlib_decl void setLegacyAES(bool value);

#define encrypt _LogProcessError12
#define decrypt _LogProcessError15

extern jlib_decl void encrypt(StringBuffer &ret, const char *in);
extern jlib_decl void decrypt(StringBuffer &ret, const char *in);

// simple inline block scrambler (shouldn't need jlib_decl)
class Csimplecrypt
{
    unsigned *r;
    unsigned n;
    size32_t blksize;
public:
    Csimplecrypt(const byte *key, size32_t keysize, size32_t _blksize)
    {
        n = (unsigned)(_blksize/sizeof(unsigned));
        blksize = (size32_t)(n*sizeof(unsigned));
        r = (unsigned *)malloc(blksize);
        byte * z = (byte *)r;
        size32_t ks = keysize;
        const byte *k = key;
        for (size32_t i=0;i<blksize;i++) {
            z[i] = (byte)(*k+i);
            if (--ks==0) {
                k = key;
                ks = keysize;
            }
            else
                k++;
        }
        encrypt(z);
        encrypt(z);
    }
    ~Csimplecrypt()
    {
        free(r);
    }
    
    void encrypt( void * buffer )
    {
        unsigned * w = (unsigned *)buffer;
        unsigned i = 0;
        while (i<n) {
            unsigned mask = r[i];
            unsigned j = ((unsigned)mask) % n;
            unsigned wi = w[i];
            unsigned wj = w[j];
            w[j] = ( ( wj & mask ) | ( wi & ~mask ) ) + mask;
            w[i] = ( ( wi & mask ) | ( wj & ~mask ) ) + mask;
            i++;
        }
    }
    
    void decrypt( void * buffer )
    {
        unsigned * w = (unsigned*)buffer;
        unsigned i = n;
        while (i--) {
            unsigned mask = r[i];
            unsigned j = ((unsigned)mask) % n;
            unsigned wi = w[i] - mask;
            unsigned wj = w[j] - mask;
            w[i] = ( wi & mask ) | ( wj & ~mask );
            w[j] = ( wj & mask ) | ( wi & ~mask );
        }
    }
    
};


#endif //JENCRYPT_HPP
