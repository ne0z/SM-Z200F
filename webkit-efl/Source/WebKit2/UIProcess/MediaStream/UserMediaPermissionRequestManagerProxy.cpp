/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UserMediaPermissionRequestManagerProxy.h"

#if ENABLE(TIZEN_MEDIA_STREAM)

#include "UserMediaPermissionRequest.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

UserMediaPermissionRequestManagerProxy::UserMediaPermissionRequestManagerProxy(WebPageProxy* page)
    : m_page(page)
{
}

void UserMediaPermissionRequestManagerProxy::invalidateRequests()
{
    PendingRequestMap::const_iterator it = m_pendingRequests.begin();
    PendingRequestMap::const_iterator end = m_pendingRequests.end();
    for (; it != end; ++it)
        it->second->invalidate();

    m_pendingRequests.clear();
}

PassRefPtr<UserMediaPermissionRequest> UserMediaPermissionRequestManagerProxy::createRequest(uint64_t userMediaID)
{
    RefPtr<UserMediaPermissionRequest> request = UserMediaPermissionRequest::create(this, userMediaID);
    m_pendingRequests.add(userMediaID, request.get());
    return request.release();
}

void UserMediaPermissionRequestManagerProxy::didReceiveUserMediaPermissionDecision(uint64_t userMediaID, bool allow)
{
    if (!m_page->isValid())
        return;

    RefPtr<UserMediaPermissionRequest> request = m_pendingRequests.take(userMediaID);
    if (!request)
        return;

    m_page->process()->send(Messages::WebPage::DidReceiveUserMediaPermissionDecision(userMediaID, allow), m_page->pageID());
}

} // namespace WebKit

#endif // ENABLE(TIZEN_MEDIA_STREAM)
