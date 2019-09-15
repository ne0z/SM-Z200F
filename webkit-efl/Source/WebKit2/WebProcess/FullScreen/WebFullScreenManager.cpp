/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebFullScreenManager.h"

#if ENABLE(FULLSCREEN_API)

#include "Connection.h"
#include "MessageID.h"
#include "WebCoreArgumentCoders.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Color.h>
#include <WebCore/Element.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>

#if ENABLE(TIZEN_FULLSCREEN_API)
#include <WebCore/HTMLMediaElement.h>
#endif

#if ENABLE(TIZEN_GSTREAMER_VIDEO)
#include "Chrome.h"
#include "ChromeClient.h"
#include "Page.h"
#include "TizenExtensibleAPI.h"
#include <runtime_info.h>
#if ENABLE(FORCE_LANDSCAPE_VIDEO_FOR_HOT_STAR_APP)
#include <app_common.h>
#endif
#endif

#if ENABLE(TIZEN_USE_HW_VIDEO_OVERLAY_IN_FULLSCREEN)
#include "RenderFullScreen.h"
#endif

using namespace WebCore;

namespace WebKit {

static IntRect screenRectOfContents(Element* element)
{
    ASSERT(element);
#if USE(ACCELERATED_COMPOSITING)
    if (element->renderer() && element->renderer()->hasLayer() && element->renderer()->enclosingLayer()->isComposited()) {
        FloatQuad contentsBox = static_cast<FloatRect>(element->renderer()->enclosingLayer()->backing()->contentsBox());
        contentsBox = element->renderer()->localToAbsoluteQuad(contentsBox, SnapOffsetForTransforms);
        return element->renderer()->view()->frameView()->contentsToScreen(contentsBox.enclosingBoundingBox());
    }
#endif
    return element->screenRect();
}

PassRefPtr<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return adoptRef(new WebFullScreenManager(page));
}

WebFullScreenManager::WebFullScreenManager(WebPage* page)
    : m_page(page)
#if ENABLE(TIZEN_USE_HW_VIDEO_OVERLAY_IN_FULLSCREEN)
    , m_isUsingHwVideoOverlay(false)
    , m_didExitFullScreenTimer(this, &WebFullScreenManager::didExitFullScreenTimerFired)
#endif
{
}
    
WebFullScreenManager::~WebFullScreenManager()
{
}

WebCore::Element* WebFullScreenManager::element() 
{ 
    return m_element.get(); 
}

void WebFullScreenManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebFullScreenManagerMessage(connection, messageID, arguments);
}

bool WebFullScreenManager::supportsFullScreen(bool withKeyboard)
{
    if (!m_page->corePage()->settings()->fullScreenEnabled())
        return false;

    return m_page->injectedBundleFullScreenClient().supportsFullScreen(m_page.get(), withKeyboard);
}

void WebFullScreenManager::enterFullScreenForElement(WebCore::Element* element)
{
    ASSERT(element);
    m_element = element;
#if ENABLE(TIZEN_FULLSCREEN_API)
    // Hide media controls until the fullscreen transition is complete
    if (m_element->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
        mediaElement->hideControls();
    }
#endif
    m_initialFrame = screenRectOfContents(m_element.get());
    m_page->injectedBundleFullScreenClient().enterFullScreenForElement(m_page.get(), element);

#if ENABLE(FORCE_LANDSCAPE_VIDEO_FOR_HOT_STAR_APP)
    // Rotate Video Fullscreen for HotStar App.
    if (m_element->isMediaElement()) {
        char *appId;
        if (!app_get_id(&appId)) {
            String appPackId(appId);
            if (appPackId.contains("FwDxV4XXVK")) {
                HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
                mediaElement->rotateScreenforHotStar();
            }
            free(appId);
        }
    }
#endif
}

void WebFullScreenManager::exitFullScreenForElement(WebCore::Element* element)
{
#if ENABLE(TIZEN_FULLSCREEN_API)
    hideFullScreenControls();
#endif
#if ENABLE(TIZEN_USE_HW_VIDEO_OVERLAY_IN_FULLSCREEN)
    if (isUsingHwVideoOverlay())
        removeBackgroundTransperent();
#endif
#if ENABLE(TIZEN_GSTREAMER_VIDEO)
    ASSERT(m_element);
    if (m_element->isMediaElement()) {
        bool auto_rotate = false;
        if (runtime_info_get_value_bool(RUNTIME_INFO_KEY_AUTO_ROTATION_ENABLED, &auto_rotate) == RUNTIME_INFO_ERROR_NONE) {
            if (!auto_rotate || TizenExtensibleAPI::extensibleAPI().rotationLock()) {
                Page* page = m_element->document()->page();
                if (page)
                    page->chrome()->client()->mediaControlsRequestRotate("exit");
           }
       } else
            TIZEN_LOGE("Failed to get RUNTIME_INFO_KEY_AUTO_ROTATION_ENABLED key");
    }
#endif
    m_page->injectedBundleFullScreenClient().exitFullScreenForElement(m_page.get(), element);
}

void WebFullScreenManager::willEnterFullScreen()
{
    ASSERT(m_element);
    m_element->document()->webkitWillEnterFullScreenForElement(m_element.get());
    m_element->document()->updateLayout();
#if !ENABLE(TIZEN_FULLSCREEN_API)
    m_page->forceRepaintWithoutCallback();
#endif
    m_finalFrame = screenRectOfContents(m_element.get());
    m_page->injectedBundleFullScreenClient().beganEnterFullScreen(m_page.get(), m_initialFrame, m_finalFrame);
}

void WebFullScreenManager::didEnterFullScreen()
{
    ASSERT(m_element);
#if ENABLE(TIZEN_FULLSCREEN_API)
    m_page->forceRepaintWithoutCallback();
#endif
    m_element->document()->webkitDidEnterFullScreenForElement(m_element.get());
}

void WebFullScreenManager::willExitFullScreen()
{
    ASSERT(m_element);
    m_finalFrame = screenRectOfContents(m_element.get());
    m_element->document()->webkitWillExitFullScreenForElement(m_element.get());
    m_page->injectedBundleFullScreenClient().beganExitFullScreen(m_page.get(), m_finalFrame, m_initialFrame);
}

void WebFullScreenManager::didExitFullScreen()
{
    ASSERT(m_element);

// Force Repaint to avoid displaying Video frame and Controls after Fullscreen Exit.
#if ENABLE(TIZEN_USE_HW_VIDEO_OVERLAY_IN_FULLSCREEN)
    if (isUsingHwVideoOverlay())
        m_page->forceRepaintWithoutCallback();
#endif

    m_element->document()->webkitDidExitFullScreenForElement(m_element.get());

#if ENABLE(TIZEN_USE_HW_VIDEO_OVERLAY_IN_FULLSCREEN)
    if (isUsingHwVideoOverlay()) {
        setUsingHwVideoOverlay(false);
        // To enable compositing after tiles for normal screen is prepared.
        m_didExitFullScreenTimer.startOneShot(0);
    }
#endif
}

#if ENABLE(TIZEN_FULLSCREEN_API)
void WebFullScreenManager::hideFullScreenControls()
{
    ASSERT(m_element);
    if (!m_element->isMediaElement())
        return;

    HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
    mediaElement->hideControls();
}

void WebFullScreenManager::updateMediaControlsStyle()
{
    ASSERT(m_element);

#if ENABLE(TIZEN_GSTREAMER_VIDEO)
    if (isMiniWindowMode() || isMultiWindowMode())
        hideFullScreenControls();
#endif

    if (m_element->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
        mediaElement->updateMediaControlsStyle();
    }
}
#endif

void WebFullScreenManager::setAnimatingFullScreen(bool animating)
{
    ASSERT(m_element);
    m_element->document()->setAnimatingFullScreen(animating);
}

void WebFullScreenManager::requestExitFullScreen()
{
    ASSERT(m_element);
#if ENABLE(TIZEN_GSTREAMER_VIDEO)
    if (m_element->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
        mediaElement->exitFullscreen();
    } else
#endif
    m_element->document()->webkitCancelFullScreen();
}

void WebFullScreenManager::close()
{
    m_page->injectedBundleFullScreenClient().closeFullScreen(m_page.get());
}

#if ENABLE(TIZEN_FULLSCREEN_API)
void WebFullScreenManager::willRotateScreen()
{
    ASSERT(m_element);
    if (m_element.get()->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
        mediaElement->willRotateScreen();
    }
}

void WebFullScreenManager::didRotateScreen()
{
    TIZEN_LOGI("");

    ASSERT(m_element);
    if (m_element.get()->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
        mediaElement->didRotateScreen();
    }
}
#endif

#if ENABLE(TIZEN_USE_HW_VIDEO_OVERLAY_IN_FULLSCREEN)
void WebFullScreenManager::removeBackgroundTransperent()
{
    RenderFullScreen* fullScreenRenderer = m_element->document()->fullScreenRenderer();
    if (!fullScreenRenderer)
        return;

    RefPtr<RenderStyle> newStyle = RenderStyle::clone(fullScreenRenderer->style());
    newStyle->setBackgroundColor(Color::black);
    fullScreenRenderer->setStyle(newStyle);

    if (m_element->document()->renderer())
       m_element->document()->renderer()->repaint(true);
}

void WebFullScreenManager::didExitFullScreenTimerFired(Timer < WebFullScreenManager > *)
{
    m_page->send(Messages::WebFullScreenManagerProxy::SetUsingHwVideoOverlay(false));
}
#endif

#if ENABLE(VIDEO_RESOLUTION_DISPLAY_POP_UP) && ENABLE(TIZEN_GSTREAMER_VIDEO)
const String WebFullScreenManager::showVideoSizeInToastedPopUp()
{
    if (m_element.get()->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(m_element.get());
        return mediaElement->showVideoSizeInToastedPopUp();
    }
    return emptyString();
}
#endif

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
