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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "SSLSecureTizen.h"
#include "WorkerContextSSLSecure.h"
#include <wtf/Uint8Array.h>

namespace WebCore {

WorkerContextSSLSecure::WorkerContextSSLSecure()
{
}

WorkerContextSSLSecure::~WorkerContextSSLSecure()
{
}

PassRefPtr<Uint8Array> WorkerContextSSLSecure::aesEncrypt(WorkerContext* worker, PassRefPtr<Uint8Array> text, PassRefPtr<Uint8Array> key, int mode, PassRefPtr<Uint8Array> iv, bool padding, int keyLength)
{
    if(!text || !key || !iv)
    {
        TIZEN_LOGI("SRIB Wrong Input");
        return Uint8Array::create(0);
    }

    return SSLSecureTizen::getInstance()->AES_Encrypt(text->data(), text->length(), key->data(), key->length(), mode, iv->data(), iv->length(), padding, keyLength);
}

PassRefPtr<Uint8Array> WorkerContextSSLSecure::aesDecrypt(WorkerContext* worker, PassRefPtr<Uint8Array> text, PassRefPtr<Uint8Array> key, int mode, PassRefPtr<Uint8Array> iv, bool padding, int keyLength)
{
    if(!text || !key || !iv)
    {
        TIZEN_LOGI("SRIB Wrong Input");
        return Uint8Array::create(0);
    }

    return SSLSecureTizen::getInstance()->AES_Decrypt(text->data(), text->length(), key->data(), key->length(), mode, iv->data(), iv->length(), padding, keyLength);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
