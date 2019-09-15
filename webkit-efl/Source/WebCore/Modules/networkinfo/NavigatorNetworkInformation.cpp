/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "NavigatorNetworkInformation.h"

#if ENABLE(NETWORK_TYPE)

#include "Frame.h"
#include "Navigator.h"
#include "NetworkInfoController.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

NavigatorNetworkInformation::NavigatorNetworkInformation()
{
}

NavigatorNetworkInformation::~NavigatorNetworkInformation()
{
}

NavigatorNetworkInformation* NavigatorNetworkInformation::from(Navigator* navigator)
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("NavigatorNetworkInformation"));
    NavigatorNetworkInformation* supplement = static_cast<NavigatorNetworkInformation*>(Supplement<Navigator>::from(navigator, name));
    if (!supplement) {
        supplement = new NavigatorNetworkInformation();
        provideTo(navigator, name, adoptPtr(supplement));
    }
    return supplement;
}

NetworkInformation* NavigatorNetworkInformation::connection(Navigator* navigator)
{
    if (!navigator->frame() || !NetworkInformation::UDSEnabled())
        return 0;

    NavigatorNetworkInformation* navigatorConnection = NavigatorNetworkInformation::from(navigator);
    if (!navigatorConnection->m_connection)
        navigatorConnection->m_connection = NetworkInformation::create(navigator);
    return navigatorConnection->m_connection.get();
}

} // namespace WebCore

#endif // ENABLE(NETWORK_TYPE)