/*
 * Copyright (C) 2016 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include "SSLSecureTizen.h"
#include <wtf/Uint8Array.h>

namespace WebCore {

static SSLSecureTizen* s_SSLSecureTizen = 0;
static Mutex s_SSLSecureTizenMutex;

const String defaultIv = "8947az34awl34kjq";

SSLSecureTizen::~SSLSecureTizen()
{
}

SSLSecureTizen* SSLSecureTizen::getInstance()
{
    if (!s_SSLSecureTizen) {
        MutexLocker locker(s_SSLSecureTizenMutex);
        s_SSLSecureTizen = new SSLSecureTizen();
    }

    return s_SSLSecureTizen;
}

PassRefPtr<Uint8Array> SSLSecureTizen::AES_Encrypt(unsigned char* text, int textLen, unsigned char* key, int keyLen, int mode, unsigned char* iv, int ivLen, bool padding, int keyLength)
{
    TIZEN_LOGI("SRIB SSLSecureTizen::AES_Encrypt");

    int outlen, tmplen;
    EVP_CIPHER_CTX *ctx;
    SSLAesMode aesMode = (SSLAesMode)(mode);

    unsigned char* outbuf = static_cast<unsigned char*>(malloc(textLen + 1 + EVP_MAX_BLOCK_LENGTH));

    ctx = EVP_CIPHER_CTX_new();

    switch(aesMode)
    {
    case AES_128_CBC:
        if(ivLen == 0)
            EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, (unsigned char*)(defaultIv.latin1().data()));
        else
            EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
        break;
    case AES_256_CBC:
        if(ivLen == 0)
            EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, (unsigned char*)(defaultIv.latin1().data()));
        else
            EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
        break;
    case AES_128_ECB:
            EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
        break;
    case AES_256_ECB:
            EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);
        break;
    default:
        if(ivLen == 0)
            EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, (unsigned char*)(defaultIv.latin1().data()));
        else
            EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
        break;
    }

    EVP_CIPHER_CTX_set_padding(ctx, padding);

    if(!EVP_EncryptUpdate(ctx, outbuf, &outlen, text, textLen))
    {
        /* Error */
        TIZEN_LOGI("SRIB encrypt update error");
        EVP_CIPHER_CTX_free(ctx);

        if(outbuf)
            free(outbuf);

        return Uint8Array::create(0);
    }

    /* Buffer passed to EVP_EncryptFinal() must be after data just
    * encrypted to avoid overwriting it.
    */
    if(!EVP_EncryptFinal_ex(ctx, outbuf + outlen, &tmplen))
    {
        /* Error */
        TIZEN_LOGI("SRIB encrypt update tmplen = %d", tmplen);
        TIZEN_LOGI("SRIB encrypt Final error");
        EVP_CIPHER_CTX_free(ctx);

        if(outbuf)
            free(outbuf);

        return Uint8Array::create(0);
    }

    outlen += tmplen;
    EVP_CIPHER_CTX_free(ctx);

    PassRefPtr<Uint8Array> encrypted = Uint8Array::create(outbuf, outlen);

    if(outbuf)
        free(outbuf);

    return encrypted;
}

PassRefPtr<Uint8Array> SSLSecureTizen::AES_Decrypt(unsigned char* encrypted, int encryptLen, unsigned char* key, int keyLen, int mode, unsigned char* iv, int ivLen, bool padding, int keyLength)
{
    TIZEN_LOGI("SRIB SSLSecureTizen::AES_Decrypt");

    int outlen, tmplen;
    EVP_CIPHER_CTX *ctx;
    SSLAesMode aesMode = (SSLAesMode)(mode);

    unsigned char* outbuf = static_cast<unsigned char*>(malloc(encryptLen + 1 + EVP_MAX_BLOCK_LENGTH));

    ctx = EVP_CIPHER_CTX_new();

    switch(aesMode) {
    case AES_128_CBC:
        if(ivLen == 0)
            EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, (unsigned char*)(defaultIv.latin1().data()));
        else
            EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
        break;
    case AES_256_CBC:
        if(ivLen == 0)
            EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, (unsigned char*)(defaultIv.latin1().data()));
        else
            EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
        break;
    case AES_128_ECB:
            EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
        break;
    case AES_256_ECB:
            EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);
        break;
    default:
        if(ivLen == 0)
            EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, (unsigned char*)(defaultIv.latin1().data()));
        else
            EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
        break;
    }

    EVP_CIPHER_CTX_set_padding(ctx, padding);

    if(!EVP_DecryptUpdate(ctx, outbuf, &outlen, encrypted, encryptLen))
    {
        /* Error */
        TIZEN_LOGI("SRIB decrypt update error");
        EVP_CIPHER_CTX_free(ctx);

        if(outbuf)
            free(outbuf);

        return Uint8Array::create(0);
    }

    /* Buffer passed to EVP_EncryptFinal() must be after data just
    * encrypted to avoid overwriting it.
    */
    if(!EVP_DecryptFinal_ex(ctx, outbuf + outlen, &tmplen))
    {
        /* Error */
        TIZEN_LOGI("SRIB decrypt Final error");

        char buf[1024];
        TIZEN_LOGI("SRIB : error = %s", ERR_error_string(ERR_get_error(), buf));
        TIZEN_LOGI("SRIB : error buf = %s", buf);

        EVP_CIPHER_CTX_free(ctx);

        if(outbuf)
            free(outbuf);

        return Uint8Array::create(0);
    }

    outlen += tmplen;
    EVP_CIPHER_CTX_free(ctx);

    PassRefPtr<Uint8Array> plainText = Uint8Array::create(outbuf, outlen);

    if(outbuf)
        free(outbuf);

    return plainText;
}
}
