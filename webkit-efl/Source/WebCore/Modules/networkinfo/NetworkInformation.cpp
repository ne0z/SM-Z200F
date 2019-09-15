/*
 * Copyright (C) 2016 Samsung Electronics. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkInformation.h"

#if ENABLE(NETWORK_TYPE)

#include "Frame.h"
#include "NetworkInfoClient.h"
#include <net_connection.h>
#include <telephony.h>

namespace WebCore {

bool NetworkInformation::m_UDSEnabled = false;

PassRefPtr<NetworkInformation> NetworkInformation::create(Navigator* navigator)
{
    RefPtr<NetworkInformation> networkInformation(adoptRef(new NetworkInformation(navigator)));
    networkInformation->suspendIfNeeded();
    return networkInformation.release();
}

NetworkInformation::NetworkInformation(Navigator* navigator)
    : ActiveDOMObject(navigator->frame()->document(), this)
{
}

NetworkInformation::~NetworkInformation()
{
}

void NetworkInformation::setUDSEnabled (bool enabled)
{
    if (m_UDSEnabled == enabled)
        return;
    TIZEN_LOGI ("UDS enabled [%d]", enabled);
    m_UDSEnabled = enabled;
}

int getNetworkType(telephony_network_type_e* network_type) {
    int status;
    telephony_handle_list_s handle_list;

    status = telephony_init(&handle_list);
    if (status == TELEPHONY_ERROR_NONE) {
        status = telephony_network_get_type(handle_list.handle[0], network_type);
        if (status != TELEPHONY_ERROR_NONE) {
            TIZEN_LOGI("telephony_network_get_type failed: %d", status);
        }
        telephony_deinit(&handle_list);
    } else {
        TIZEN_LOGI("telephony_init failed: %d", status);
    }

    return status;
}

String NetworkInformation::type() const
{
    String type = "unknown";
    connection_h connectionHandle = NULL;
    if (connection_create(&connectionHandle) != CONNECTION_ERROR_NONE)
        return type;

    connection_wifi_state_e wifiState;
    connection_get_wifi_state(connectionHandle, &wifiState);

    if (wifiState == CONNECTION_WIFI_STATE_CONNECTED) {
        TIZEN_LOGI("Connection Type: wifi");
        type = "swifi";
    } else {

        connection_cellular_state_e cellularState;
        connection_get_cellular_state(connectionHandle, &cellularState);

        if (cellularState == CONNECTION_CELLULAR_STATE_CONNECTED) {
            TIZEN_LOGI("Connection Type: cellular");
            /*telephony_network_type_e network_type;
            if (getNetworkType(&network_type) == 0) { // 0 for success case
                if (network_type == TELEPHONY_NETWORK_TYPE_LTE) {
                    TIZEN_LOGI("Connection Type: LTE- wifi");
                    type = "swifi";
                } else {
                    type = "cellular";
                }
            }*/
            type = "cellular";
        }
    }

    if (connectionHandle)
        connection_destroy(connectionHandle);

    return type;
}

double NetworkInformation::downlinkMax() const
{
    return 0.386;

/*    double downlinkMaxMbps = 0;
    telephony_network_type_e network_type;
    if (getNetworkType(&network_type) != 0) // 0 for success case
        return downlinkMaxMbps;

    switch(network_type) {
    case TELEPHONY_NETWORK_TYPE_GSM:
        downlinkMaxMbps = 0.01;
        break;

    case TELEPHONY_NETWORK_TYPE_GPRS:
        downlinkMaxMbps = 0.237;
        break;

    case TELEPHONY_NETWORK_TYPE_EDGE:
        downlinkMaxMbps = 0.384;
        break;

    case TELEPHONY_NETWORK_TYPE_UMTS:
        downlinkMaxMbps = 2;
        break;

    case TELEPHONY_NETWORK_TYPE_HSDPA:
        downlinkMaxMbps = 14.3;
        break;

    case TELEPHONY_NETWORK_TYPE_LTE:
        downlinkMaxMbps = 100;
        break;

    case TELEPHONY_NETWORK_TYPE_UNKNOWN:
        downlinkMaxMbps = 0;
        break;
    }

    TIZEN_LOGI("downlinkMaxMbps: %f", downlinkMaxMbps);
    return downlinkMaxMbps;
    */
}

void NetworkInformation::suspend(ReasonForSuspension)
{
}

void NetworkInformation::resume()
{
}

void NetworkInformation::stop()
{
}

} // namespace WebCore

#endif // ENABLE(NETWORK_TYPE)