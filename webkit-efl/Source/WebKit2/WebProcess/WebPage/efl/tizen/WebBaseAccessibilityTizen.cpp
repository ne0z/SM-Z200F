/*
 * Copyright (C) 2015 Samsung Electronics All rights reserved.
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
#include "WebBaseAccessibilityTizen.h"

#if HAVE(ACCESSIBILITY)

#include <atk-bridge.h>
#if ENABLE(TIZEN_ACCESSIBILITY_CHECK_SCREEN_READER_PROPERTY)
#include <eldbus_connection.h>
#include <eldbus_message.h>
#include <eldbus_object.h>
#include <eldbus_pending.h>
#include <Eldbus.h>
#include "WebPage.h"
#include "WebProcess.h"

namespace {
const char Bus[] = "org.a11y.Bus";
const char ObjectPath[] = "/org/a11y/bus";
const char PropertiesInterface[] = "org.freedesktop.DBus.Properties";
const char Method[] = "Get";
const char StatusInterface[] = "org.a11y.Status";
const char Property[] = "ScreenReaderEnabled";
}

#endif

G_DEFINE_TYPE(WebBaseAccessibility, web_base_accessibility, ATK_TYPE_OBJECT)

static void web_base_accessibility_init(WebBaseAccessibility*)
{
}

static void web_base_accessibility_class_init(WebBaseAccessibilityClass* klass)
{
}

static const char* toolkitName()
{
    return "WebKit";
}

static const char* toolkitVersion()
{
    return "1.0";
}

static AtkObject* root()
{
    static AtkObject* root = NULL;
    if (!root)
        root = ATK_OBJECT(g_object_new(WEB_BASE_ACCESSIBILITY_TYPE, NULL));

    return root;
}

#if ENABLE(TIZEN_ACCESSIBILITY_CHECK_SCREEN_READER_PROPERTY)

void WebAccessibility::instance(WebProcess* webProcess)
{
    static WebAccessibility webAccessibility(webProcess);
}

WebAccessibility::WebAccessibility(WebProcess* webProcess)
    : m_webProcess(webProcess)
    , m_connection(0)
    , m_proxy(0)
    , m_object(0)
{
    init();
}

#else

void WebAccessibility::instance()
{
    static WebAccessibility instance;
}

WebAccessibility::WebAccessibility()
{
    init();
}

#endif

WebAccessibility::~WebAccessibility()
{
#if ENABLE(TIZEN_ACCESSIBILITY_CHECK_SCREEN_READER_PROPERTY)
    if (m_proxy)
        eldbus_proxy_unref(m_proxy);

    if (m_object)
        eldbus_object_unref(m_object);

    if (m_connection)
        eldbus_connection_unref(m_connection);
#endif

    cleanup();
}

void WebAccessibility::cleanup()
{
    atk_bridge_adaptor_cleanup();
}

void WebAccessibility::bridgeInit()
{
    AtkUtilClass* atkUtilClass = ATK_UTIL_CLASS(g_type_class_ref(ATK_TYPE_UTIL));
    atkUtilClass->get_toolkit_name  = toolkitName;
    atkUtilClass->get_toolkit_version = toolkitVersion;
    atkUtilClass->get_root = root;

    // Init atk bridge
    atk_bridge_adaptor_init(NULL, NULL);
}

#if ENABLE(TIZEN_ACCESSIBILITY_CHECK_SCREEN_READER_PROPERTY)
void WebAccessibility::iterateOverPropertiesCb(void* data, const void* key, Eldbus_Message_Iter* var)
{
    WebAccessibility* instance = static_cast<WebAccessibility*>(data);
    if (!data)
        return;

    bool isAccessibilityEnabled  = false;
    eldbus_message_iter_arguments_get(var, "b", &isAccessibilityEnabled);

    if (isAccessibilityEnabled)
        instance->bridgeInit();
    else
        instance->cleanup();

    instance->webProcess()->notifyAccessibilityStatus(isAccessibilityEnabled);
}

void WebAccessibility::accessibilityPropertiesChangedCb(void* data, const Eldbus_Message* message)
{
    if (eldbus_message_error_get(message, 0, 0))
        return;

    const char* iface;
    Eldbus_Message_Iter* array, *invalidate;

    const char* signature = eldbus_message_signature_get(message);
    if (!eldbus_message_arguments_get(message, signature, &iface, &array, &invalidate))
        return;

    eldbus_message_iter_dict_iterate(array, "sv", iterateOverPropertiesCb, data);
}

void WebAccessibility::checkPlatformAccessibilitySupport(void* data, const Eldbus_Message* message, Eldbus_Pending*)
{
    if (eldbus_message_error_get(message, 0, 0))
        return;

    WebAccessibility* instance = static_cast<WebAccessibility*>(data);
    if (!data)
        return;

    Eldbus_Message_Iter* variant;
    if (!eldbus_message_arguments_get(message, eldbus_message_signature_get(message), &variant))
        return;

    bool isAccessibilityEnabled = false;
    eldbus_message_iter_arguments_get(variant, "b", &isAccessibilityEnabled);
    if (isAccessibilityEnabled)
        instance->bridgeInit();

    instance->webProcess()->notifyAccessibilityStatus(isAccessibilityEnabled);
}

void WebAccessibility::init()
{
    // Stop doing this on root account. Accessibility stack is supported only on app user account.
    if (getuid() == 0)
        return;

    m_connection = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
    if (!m_connection)
        return;

    m_object = eldbus_object_get(m_connection, Bus, ObjectPath);
    if (!m_object) {
        eldbus_connection_unref(m_connection);
        return;
    }

    m_proxy = eldbus_proxy_get(m_object, PropertiesInterface);
    if (!m_proxy) {
        eldbus_connection_unref(m_connection);
        eldbus_object_unref(m_object);
        return;
    }

    // First check ScreenReaderProperty, when on setup atk-bridge
    Eldbus_Message* message = eldbus_proxy_method_call_new(m_proxy, Method);
    Eldbus_Message_Iter* iter = eldbus_message_iter_get(message);
    eldbus_message_iter_arguments_append(iter, "s", StatusInterface);
    eldbus_message_iter_arguments_append(iter, "s", Property);
    eldbus_proxy_send(m_proxy, message, checkPlatformAccessibilitySupport, this, -1);

    // Listen to changes of ScreenReaderEnabled property
    eldbus_proxy_signal_handler_add(m_proxy, "PropertiesChanged", accessibilityPropertiesChangedCb, this);
}

#else

void WebAccessibility::init()
{
    // Stop doing this on root account. Accessibility stack is supported only on app user account.
    if (getuid() == 0)
        return;

    bridgeInit();
}

#endif // TIZEN_ACCESSIBILITY_CHECK_SCREEN_READER_PROPERTY

#endif // HAVE(ACCESSIBILITY)
