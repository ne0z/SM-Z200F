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

#ifndef SSLSecureTizen_h
#define SSLSecureTizen_h

#include "KURL.h"
#include "PlatformString.h"
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

enum SSLAesMode { AES_128_CBC, AES_256_CBC, AES_128_ECB, AES_256_ECB };

class SSLSecureTizen : public RefCounted<SSLSecureTizen> {
private:
    SSLSecureTizen() {
        m_aesMode = AES_128_CBC;
        m_KeyLength = 16;
    };

public:
    ~SSLSecureTizen();

    static SSLSecureTizen* getInstance();

    PassRefPtr<Uint8Array> AES_Encrypt(unsigned char* text, int textLen, unsigned char* key, int keyLen, int mode, unsigned char* iv, int ivLen, bool padding, int keyLength);

    PassRefPtr<Uint8Array> AES_Decrypt(unsigned char* text, int textLen, unsigned char* key, int keyLen, int mode, unsigned char* iv, int ivLen, bool padding, int keyLength);

    void setAESMode(SSLAesMode aesMode) { m_aesMode = aesMode; }

    SSLAesMode getAESMode() { return m_aesMode; }

    void setAESKeyLength(int keyLen) { m_KeyLength = keyLen; }

    int getAESKeyLength() { return m_KeyLength; }

private:
    SSLAesMode m_aesMode;
    int m_KeyLength;
};
} // namespace WebCore

#endif // SSLSecureTizen_h
