/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "MainResourceLoader.h"

#include "ApplicationCacheHost.h"
#include "BackForwardController.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoadTiming.h"
#include "DocumentLoader.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/CurrentTime.h>

#if ENABLE(TIZEN_INTERCEPT_REQUEST)
#include "MIMETypeRegistry.h"
#endif

#if PLATFORM(QT)
#include "PluginDatabase.h"
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
#include "WebCoreSystemInterface.h"
#endif

// FIXME: More that is in common with SubresourceLoader should move up into ResourceLoader.

namespace WebCore {

static bool shouldLoadAsEmptyDocument(const KURL& url)
{
    return url.isEmpty() || SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(url.protocol());
}

MainResourceLoader::MainResourceLoader(Frame* frame)
    : ResourceLoader(frame, ResourceLoaderOptions(SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForCrossOriginCredentials, SkipSecurityCheck))
    , m_dataLoadTimer(this, &MainResourceLoader::handleSubstituteDataLoadNow)
    , m_loadingMultipartContent(false)
    , m_waitingForContentPolicy(false)
    , m_timeOfLastDataReceived(0.0)
#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    , m_filter(0)
#endif
{
}

MainResourceLoader::~MainResourceLoader()
{
#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    ASSERT(!m_filter);
#endif
}

PassRefPtr<MainResourceLoader> MainResourceLoader::create(Frame* frame)
{
    return adoptRef(new MainResourceLoader(frame));
}

void MainResourceLoader::receivedError(const ResourceError& error)
{
    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);
    RefPtr<Frame> protectFrame(m_frame);

    // It is important that we call DocumentLoader::mainReceivedError before calling 
    // ResourceLoadNotifier::didFailToLoad because mainReceivedError clears out the relevant
    // document loaders. Also, mainReceivedError ends up calling a FrameLoadDelegate method
    // and didFailToLoad calls a ResourceLoadDelegate method and they need to be in the correct order.
    documentLoader()->mainReceivedError(error);

    if (!cancelled()) {
        ASSERT(!reachedTerminalState());
        frameLoader()->notifier()->didFailToLoad(this, error);
        
        releaseResources();
    }

    ASSERT(reachedTerminalState());
}

void MainResourceLoader::willCancel(const ResourceError&)
{
    m_dataLoadTimer.stop();

    if (m_waitingForContentPolicy) {
        frameLoader()->policyChecker()->cancelCheck();
        ASSERT(m_waitingForContentPolicy);
        m_waitingForContentPolicy = false;
        deref(); // balances ref in didReceiveResponse
    }
}

void MainResourceLoader::didCancel(const ResourceError& error)
{
    // We should notify the frame loader after fully canceling the load, because it can do complicated work
    // like calling DOMWindow::print(), during which a half-canceled load could try to finish.
    documentLoader()->mainReceivedError(error);

#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    if (m_filter) {
        wkFilterRelease(m_filter);
        m_filter = 0;
    }
#endif
}

ResourceError MainResourceLoader::interruptedForPolicyChangeError() const
{
    return frameLoader()->client()->interruptedForPolicyChangeError(request());
}

void MainResourceLoader::stopLoadingForPolicyChange()
{
    ResourceError error = interruptedForPolicyChangeError();
    error.setIsCancellation(true);
    cancel(error);
}

void MainResourceLoader::callContinueAfterNavigationPolicy(void* argument, const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterNavigationPolicy(request, shouldContinue);
}

void MainResourceLoader::continueAfterNavigationPolicy(const ResourceRequest& request, bool shouldContinue)
{
    if (!shouldContinue) {
#if ENABLE(TIZEN_DLOG_SUPPORT)
        TIZEN_LOGI("[Loader] !shouldContinue");
#endif
        stopLoadingForPolicyChange();
    }
    else if (m_substituteData.isValid()) {
        // A redirect resulted in loading substitute data.
        ASSERT(documentLoader()->timing()->redirectCount());
#if ENABLE(TIZEN_DLOG_SUPPORT)
        TIZEN_LOGI("[Loader] m_substituteData.isValid() call handle()->cancel()");
#endif
#if PLATFORM(TIZEN)
        if (handle())
            handle()->cancel();
#else
        handle()->cancel();
#endif
        handleSubstituteDataLoadSoon(request);
    }

    deref(); // balances ref in willSendRequest
}

bool MainResourceLoader::isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (newRequest.httpMethod() == "POST")
        return true;

    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && frameLoader()->initialRequest().httpMethod() == "POST")
        return true;
    
    return false;
}

void MainResourceLoader::addData(const char* data, int length, bool allAtOnce)
{
    ResourceLoader::addData(data, length, allAtOnce);
    documentLoader()->receivedData(data, length);
}

void MainResourceLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());

    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    ASSERT(documentLoader()->timing()->fetchStart());

#if ENABLE(TIZEN_DLOG_SUPPORT)
    if (!redirectResponse.isNull()) {
        TIZEN_SECURE_LOGI("[Loader] this[%p], RedirectedUrl[%.256s]", this, redirectResponse.url().string().utf8().data());
    }
    else {
        TIZEN_SECURE_LOGI("[Loader] this[%p], RequestUrl[%.256s]", this, newRequest.url().string().utf8().data());
    }
#endif

    if (!redirectResponse.isNull()) {
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        RefPtr<SecurityOrigin> redirectingOrigin = SecurityOrigin::create(redirectResponse.url());
        if (!redirectingOrigin->canDisplay(newRequest.url())) {
            FrameLoader::reportLocalLoadFailed(m_frame.get(), newRequest.url().string());
            cancel();
#if ENABLE(TIZEN_DLOG_SUPPORT)
            TIZEN_SECURE_LOGI("[Loader] !redirectingOrigin->canDisplay(newRequest.url() [%.256s])", newRequest.url().string().utf8().data());
#endif
            return;
        }
        documentLoader()->timing()->addRedirect(redirectResponse.url(), newRequest.url());
    }

    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frameLoader()->isLoadingMainFrame())
        newRequest.setFirstPartyForCookies(newRequest.url());

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ReloadIgnoringCacheData);

    Frame* top = m_frame->tree()->top();
    if (top != m_frame) {
        if (!frameLoader()->checkIfDisplayInsecureContent(top->document()->securityOrigin(), newRequest.url())) {
            cancel();
#if ENABLE(TIZEN_DLOG_SUPPORT)
            TIZEN_SECURE_LOGI("[Loader] !checkIfDisplayInsecureContent url [%.256s]", newRequest.url().string().utf8().data());
#endif
            return;
        }
    }

    ResourceLoader::willSendRequest(newRequest, redirectResponse);

    // Don't set this on the first request. It is set when the main load was started.
    m_documentLoader->setRequest(newRequest);

    if (!redirectResponse.isNull()) {
        // We checked application cache for initial URL, now we need to check it for redirected one.
        ASSERT(!m_substituteData.isValid());
        // We need to check content policy for redirected response.
#if ENABLE(TIZEN_ON_REDIRECTION_REQUESTED)
        int status = redirectResponse.httpStatusCode();
        if (status >= 300 && status < 400 && status != 304) {
            ASSERT(!m_waitingForContentPolicy);
            m_waitingForContentPolicy = true;
            m_response = redirectResponse;
            ref(); // balanced by deref in continueAfterContentPolicy and didCancel
            frameLoader()->policyChecker()->checkContentPolicy(redirectResponse, callContinueAfterContentPolicy, this);
            if (reachedTerminalState())
                return;
        }
#endif
        documentLoader()->applicationCacheHost()->maybeLoadMainResourceForRedirect(newRequest, m_substituteData);
    }

    // FIXME: Ideally we'd stop the I/O until we hear back from the navigation policy delegate
    // listener. But there's no way to do that in practice. So instead we cancel later if the
    // listener tells us to. In practice that means the navigation policy needs to be decided
    // synchronously for these redirect cases.
    if (!redirectResponse.isNull()) {
        ref(); // balanced by deref in continueAfterNavigationPolicy
        frameLoader()->policyChecker()->checkNavigationPolicy(newRequest, callContinueAfterNavigationPolicy, this);
    }
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction contentPolicy, const ResourceResponse& r)
{
    KURL url = request().url();
    const String& mimeType = r.mimeType();

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_SECURE_LOGI("[Loader] this[%p], mimeType[%s], contentPolicy[%d]", this, mimeType.utf8().data(), contentPolicy);
#endif

    switch (contentPolicy) {
    case PolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemoteWebArchive = (equalIgnoringCase("application/x-webarchive", mimeType)
#if PLATFORM(GTK) || PLATFORM(EFL)
                                   || equalIgnoringCase("message/rfc822", mimeType)
#endif
                                   || equalIgnoringCase("multipart/related", mimeType))
            && !m_substituteData.isValid() && !SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol());
        if (!frameLoader()->client()->canShowMIMEType(mimeType) || isRemoteWebArchive) {
            frameLoader()->policyChecker()->cannotShowMIMEType(r);
            // Check reachedTerminalState since the load may have already been canceled inside of _handleUnimplementablePolicyWithErrorCode::.
            if (!reachedTerminalState())
                stopLoadingForPolicyChange();
#if ENABLE(TIZEN_DLOG_SUPPORT)
            TIZEN_LOGI("[Loader] PolicyUse cannotShowMIMEType || isRemoteWebArchive return!!");
#endif
            return;
        }
        break;
    }

    case PolicyDownload: {
        // m_handle can be null, e.g. when loading a substitute resource from application cache.
        if (!m_handle) {
            receivedError(cannotShowURLError());
#if ENABLE(TIZEN_DLOG_SUPPORT)
            TIZEN_LOGI("[Loader] PolicyDownload !m_handle receivedError return!!");
#endif
            return;
        }
        InspectorInstrumentation::continueWithPolicyDownload(m_frame.get(), documentLoader(), identifier(), r);

        // When starting the request, we didn't know that it would result in download and not navigation. Now we know that main document URL didn't change.
        // Download may use this knowledge for purposes unrelated to cookies, notably for setting file quarantine data.
        ResourceRequest request = this->request();
        frameLoader()->setOriginalURLForDownloadRequest(request);

        frameLoader()->client()->download(m_handle.get(), request, r);

        // It might have gone missing
        if (frameLoader())
            receivedError(interruptedForPolicyChangeError());
        return;
    }
    case PolicyIgnore:
        InspectorInstrumentation::continueWithPolicyIgnore(m_frame.get(), documentLoader(), identifier(), r);
        stopLoadingForPolicyChange();
#if ENABLE(TIZEN_DLOG_SUPPORT)
        TIZEN_LOGI("[Loader] PolicyIgnore return!!");
#endif
        return;
    
    default:
        ASSERT_NOT_REACHED();
    }

    RefPtr<MainResourceLoader> protect(this);

    if (r.isHTTP()) {
        int status = r.httpStatusCode();
        if (status < 200 || status >= 300) {
            bool hostedByObject = frameLoader()->isHostedByObjectElement();

            frameLoader()->handleFallbackContent();
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject) {
#if ENABLE(TIZEN_DLOG_SUPPORT)
                TIZEN_LOGI("[Loader] status [%d] hostedByObject call cancel()", status);
#endif
                cancel();
            }
        }
    }

    // we may have cancelled this load as part of switching to fallback content
    if (!reachedTerminalState())
        ResourceLoader::didReceiveResponse(r);

    if (frameLoader() && !frameLoader()->activeDocumentLoader()->isStopping()) {
        if (m_substituteData.isValid()) {
            TIZEN_LOGI( "after content policy substituteData is valid");
            if (m_substituteData.content()->size())
                didReceiveData(m_substituteData.content()->data(), m_substituteData.content()->size(), m_substituteData.content()->size(), true);
            if (frameLoader() && !frameLoader()->activeDocumentLoader()->isStopping())
                didFinishLoading(0);
        } else if (shouldLoadAsEmptyDocument(url) || frameLoader()->client()->representationExistsForURLScheme(url.protocol()))
            didFinishLoading(0);
    }
}

void MainResourceLoader::callContinueAfterContentPolicy(void* argument, PolicyAction policy)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterContentPolicy(policy);
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction policy)
{
    ASSERT(m_waitingForContentPolicy);
    m_waitingForContentPolicy = false;
    if (frameLoader() && !frameLoader()->activeDocumentLoader()->isStopping())
        continueAfterContentPolicy(policy, m_response);
    deref(); // balances ref in didReceiveResponse
}

void MainResourceLoader::didReceiveResponse(const ResourceResponse& r)
{
    // HTTP redirection
    if (r.httpStatusCode() == 302) {
        String location = r.httpHeaderField("location");
        if (!location.isEmpty()) {
            KURL newURL = KURL(r.url(), location);

            if(newURL.protocol() == "mailto") {
                ResourceRequest clientRequest(newURL);
                this->willSendRequest(clientRequest, r);

                return;
            }
        }
    }

    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForMainResponse(request(), r))
        return;

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI("[Loader] this[%p]", this);
#endif

    HTTPHeaderMap::const_iterator it = r.httpHeaderFields().find(AtomicString("x-frame-options"));
    if (it != r.httpHeaderFields().end()) {
        String content = it->second;
        if (m_frame->loader()->shouldInterruptLoadForXFrameOptions(content, r.url())) {
            InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_frame.get(), documentLoader(), identifier(), r);
            DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to display document because display forbidden by X-Frame-Options.\n"));
            m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage);

            cancel();
            return;
        }
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(shouldLoadAsEmptyDocument(r.url()) || !defersLoading());
#endif

    if (m_loadingMultipartContent) {
        frameLoader()->activeDocumentLoader()->setupForReplaceByMIMEType(r.mimeType());
        clearResourceData();
    }
    
    if (r.isMultipart())
        m_loadingMultipartContent = true;
        
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    m_documentLoader->setResponse(r);

    m_response = r;

#if ENABLE (TIZEN_INTERCEPT_REQUEST)
    if (frameLoader()->networkingContext()->interceptRequestEnabled() && ResourceLoader::isReceivedInterceptResponse()) {
        TIZEN_SECURE_LOGI( "[INTERCEPT] content policy check ignored for this resource: [%s]", r.url().string().utf8().data());
        if (!MIMETypeRegistry::canShowMIMEType(r.mimeType())) {
            TIZEN_SECURE_LOGI( "[INTERCEPT] !canShowMIMEType");
            stopLoadingForPolicyChange();
        }
        return;
    }
#endif

    ASSERT(!m_waitingForContentPolicy);
    m_waitingForContentPolicy = true;
    ref(); // balanced by deref in continueAfterContentPolicy and didCancel

    ASSERT(frameLoader()->activeDocumentLoader());

    // Always show content with valid substitute data.
    if (frameLoader()->activeDocumentLoader()->substituteData().isValid()) {
        callContinueAfterContentPolicy(this, PolicyUse);
        return;
    }

#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    Settings* settings = m_frame->settings();
    if (settings && settings->forceFTPDirectoryListings() && m_response.mimeType() == "application/x-ftp-directory") {
        callContinueAfterContentPolicy(this, PolicyUse);
        return;
    }
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    if (r.url().protocolIs("https") && wkFilterIsManagedSession())
        m_filter = wkFilterCreateInstance(r.nsURLResponse());
#endif

#if ENABLE(TIZEN_SET_AUTHORIZATION_TO_RESOURCE_REQUEST_FROM_SOUP_MESSAGE)
    if (handle()) {
        String auth = handle()->firstRequest().httpHeaderField("Authorization");
        if (!auth.isEmpty()) {
            if (!frameLoader()->activeDocumentLoader()->request().httpHeaderField("Authorization").isEmpty())
                frameLoader()->activeDocumentLoader()->request().clearHTTPAuthorization();
            frameLoader()->activeDocumentLoader()->request().addHTTPHeaderField("Authorization", auth);
        }
    }
#endif

    frameLoader()->policyChecker()->checkContentPolicy(m_response, callContinueAfterContentPolicy, this);
}

void MainResourceLoader::didReceiveData(const char* data, int length, long long encodedDataLength, bool allAtOnce)
{
    ASSERT(data);
    ASSERT(length != 0);

    ASSERT(!m_response.isNull());

#if USE(CFNETWORK) || PLATFORM(MAC)
    // Workaround for <rdar://problem/6060782>
    if (m_response.isNull()) {
        m_response = ResourceResponse(KURL(), "text/html", 0, String(), String());
        if (DocumentLoader* documentLoader = frameLoader()->activeDocumentLoader())
            documentLoader->setResponse(m_response);
    }
#endif

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!defersLoading());
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    if (m_filter) {
        ASSERT(!wkFilterWasBlocked(m_filter));
        const char* blockedData = wkFilterAddData(m_filter, data, &length);
        // If we don't have blockedData, that means we're still accumulating data
        if (!blockedData) {
            // Transition to committed state.
            ResourceLoader::didReceiveData("", 0, 0, false);
            return;
        }

        data = blockedData;
        encodedDataLength = -1;
    }
#endif

    documentLoader()->applicationCacheHost()->mainResourceDataReceived(data, length, encodedDataLength, allAtOnce);

    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    m_timeOfLastDataReceived = monotonicallyIncreasingTime();

    ResourceLoader::didReceiveData(data, length, encodedDataLength, allAtOnce);

#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    if (WebFilterEvaluator *filter = m_filter) {
        // If we got here, it means we know if we were blocked or not. If we were blocked, we're
        // done loading the page altogether. Either way, we don't need the filter anymore.

        // Remove this->m_filter early so didFinishLoading doesn't see it.
        m_filter = 0;
        if (wkFilterWasBlocked(filter))
            cancel();
        wkFilterRelease(filter);
    }
#endif
}

void MainResourceLoader::didFinishLoading(double finishTime)
{
    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(shouldLoadAsEmptyDocument(frameLoader()->activeDocumentLoader()->url()) || !defersLoading() || InspectorInstrumentation::isDebuggerPaused(m_frame.get()));
#endif

    // The additional processing can do anything including possibly removing the last
    // reference to this object.
    RefPtr<MainResourceLoader> protect(this);
    RefPtr<DocumentLoader> dl = documentLoader();

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI("[Loader] this[%p]",  this);
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    if (m_filter) {
        int length;
        const char* data = wkFilterDataComplete(m_filter, &length);
        WebFilterEvaluator *filter = m_filter;
        // Remove this->m_filter early so didReceiveData doesn't see it.
        m_filter = 0;
        if (data)
            didReceiveData(data, length, -1, false);
        wkFilterRelease(filter);
    }
#endif

    if (m_loadingMultipartContent)
        dl->maybeFinishLoadingMultipartContent();

    documentLoader()->timing()->setResponseEnd(finishTime ? finishTime : (m_timeOfLastDataReceived ? m_timeOfLastDataReceived : monotonicallyIncreasingTime()));
    documentLoader()->finishedLoading();
    ResourceLoader::didFinishLoading(finishTime);

    dl->applicationCacheHost()->finishedLoadingMainResource();
}

void MainResourceLoader::didFail(const ResourceError& error)
{
#if PLATFORM(MAC) && !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    if (m_filter) {
        wkFilterRelease(m_filter);
        m_filter = 0;
    }
#endif

    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForMainError(request(), error))
        return;

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!defersLoading());
#endif

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_SECURE_LOGI("[Loader] errorDomain[%s] errorCode[%d], this[%p]", error.domain().utf8().data(), error.errorCode(), this);
#endif
    receivedError(error);
}

void MainResourceLoader::handleEmptyLoad(const KURL& url, bool forURLScheme)
{
    String mimeType;
    if (forURLScheme)
        mimeType = frameLoader()->client()->generatedMIMETypeForURLScheme(url.protocol());
    else
        mimeType = "text/html";
    
    ResourceResponse response(url, mimeType, 0, String(), String());
    didReceiveResponse(response);
}

void MainResourceLoader::handleSubstituteDataLoadNow(MainResourceLoaderTimer*)
{
    RefPtr<MainResourceLoader> protect(this);

    KURL url = m_substituteData.responseURL();
    if (url.isEmpty())
        url = m_initialRequest.url();

    // Clear the initial request here so that subsequent entries into the
    // loader will not think there's still a deferred load left to do.
    m_initialRequest = ResourceRequest();
        
    ResourceResponse response(url, m_substituteData.mimeType(), m_substituteData.content()->size(), m_substituteData.textEncoding(), "");
    didReceiveResponse(response);
}

void MainResourceLoader::startDataLoadTimer()
{
    m_dataLoadTimer.startOneShot(0);

#if HAVE(RUNLOOP_TIMER)
    if (SchedulePairHashSet* scheduledPairs = m_frame->page()->scheduledRunLoopPairs())
        m_dataLoadTimer.schedule(*scheduledPairs);
#endif
}

void MainResourceLoader::handleSubstituteDataLoadSoon(const ResourceRequest& r)
{
    m_initialRequest = r;
    
    if (m_documentLoader->deferMainResourceDataLoad())
        startDataLoadTimer();
    else
        handleSubstituteDataLoadNow(0);
}

bool MainResourceLoader::loadNow(ResourceRequest& r)
{
    bool shouldLoadEmptyBeforeRedirect = shouldLoadAsEmptyDocument(r.url());

    ASSERT(!m_handle);
    ASSERT(shouldLoadEmptyBeforeRedirect || !defersLoading());

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_SECURE_LOGI("[Loader] this[%p], url[%.256s]", this, r.url().string().utf8().data());
#endif

    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    willSendRequest(r, ResourceResponse());
    ASSERT(!deletionHasBegun());

    // <rdar://problem/4801066>
    // willSendRequest() is liable to make the call to frameLoader() return NULL, so we need to check that here
    if (!frameLoader()) {
#if ENABLE(TIZEN_DLOG_SUPPORT)
        TIZEN_LOGI("[Loader] !frameLoader() return false!!");
#endif
        return false;
    }
    
    const KURL& url = r.url();
    bool shouldLoadEmpty = shouldLoadAsEmptyDocument(url) && !m_substituteData.isValid();

    if (shouldLoadEmptyBeforeRedirect && !shouldLoadEmpty && defersLoading())
        return true;

    resourceLoadScheduler()->addMainResourceLoad(this);
    if (m_substituteData.isValid()) 
        handleSubstituteDataLoadSoon(r);
    else if (shouldLoadEmpty || frameLoader()->client()->representationExistsForURLScheme(url.protocol()))
        handleEmptyLoad(url, !shouldLoadEmpty);
    else {
#if ENABLE (TIZEN_ALLOW_STALE_CACHE_IN_BACKFORWARD)
        if (isBackForwardLoadType(frameLoader()->loadType()))
            r.setHTTPHeaderField("Cache-Control", "max-stale=86400");
#endif

#if ENABLE(TIZEN_CACHING_NO_CACHE_MAIN_RESOURCES)
    r.setHTTPHeaderField("x-mainrequest", "1");
#endif

#if ENABLE(TIZEN_COMPRESSION_PROXY)
        int frameId;
        r.setMainFrameRequest(true);
        frameId = m_frame->page()->mainFrame()->getUCFrameID();
        if (m_frame == m_frame->page()->mainFrame()) {
           m_frame->ucBypassSet(false);
           TIZEN_LOGI("[UCPROXY] BYPASS mainFrame bypass set to FALSE");
           proxy::ProxyRequestContext *context = WebCore::ResourceHandle::getProxyRequestContext();
           if (context && ResourceHandle::compressionProxyEnabled()
#if ENABLE(TIZEN_PRIVATE_BROWSING)
                   && !ResourceHandle::privateBrowsingEnabledGet()
#endif
                   && WebCore::protocolIs(r.url().string(), "http")) {
               TIZEN_LOGI ("[UCPROXY] context->clearProxyLoaderContextForFrame( FrameId[ %d] ), context [%p]", m_frame->page()->mainFrame()->getUCFrameID(), context);
               context->clearProxyLoaderContextForFrame(m_frame->page()->mainFrame()->getUCFrameID());
           }
        } else {
           if (m_frame->page()->mainFrame()->ucBypassGet()) {
               m_frame->ucBypassSet(true);
               TIZEN_LOGI("[UCPROXY] BYPASS subframe bypass set to TRUE");
           }
        }
        m_frame->setUCFrameID(frameId);
        TIZEN_SECURE_LOGI("[UCPROXY] MainResource url [%s], frameId [%d], m_frame [%p]", r.url().string().utf8().data(), frameId, m_frame.get());
#endif

        m_handle = ResourceHandle::create(m_frame->loader()->networkingContext(), r, this, false, true);
    }

    return false;
}

void MainResourceLoader::load(const ResourceRequest& r, const SubstituteData& substituteData)
{
    ASSERT(!m_handle);

    // It appears that it is possible for this load to be cancelled and derefenced by the DocumentLoader
    // in willSendRequest() if loadNow() is called.
    RefPtr<MainResourceLoader> protect(this);

    m_substituteData = substituteData;

    ASSERT(documentLoader()->timing()->navigationStart());
    ASSERT(!documentLoader()->timing()->fetchStart());
    documentLoader()->timing()->markFetchStart();
    ResourceRequest request(r);

    documentLoader()->applicationCacheHost()->maybeLoadMainResource(request, m_substituteData);

    bool defer = defersLoading();
    if (defer) {
        bool shouldLoadEmpty = shouldLoadAsEmptyDocument(request.url());
        if (shouldLoadEmpty)
            defer = false;
    }
    if (!defer) {
        if (loadNow(request)) {
            // Started as an empty document, but was redirected to something non-empty.
            ASSERT(defersLoading());
            defer = true;
        }
    }
    if (defer)
        m_initialRequest = request;
}

void MainResourceLoader::setDefersLoading(bool defers)
{
    ResourceLoader::setDefersLoading(defers);

    if (defers) {
        if (m_dataLoadTimer.isActive())
            m_dataLoadTimer.stop();
    } else {
        if (m_initialRequest.isNull())
            return;

        ResourceRequest initialRequest(m_initialRequest);
        m_initialRequest = ResourceRequest();
        loadNow(initialRequest);
    }
}

#if ENABLE(TIZEN_CERTIFICATE_HANDLING)
 void MainResourceLoader::setCertificatePemFile(const char* certificate)
{
    if (frameLoader()->isLoadingMainFrame()) {
        frameLoader()->client()->dispatchSetCertificateFile(String::fromUTF8(certificate));
    }
}
#endif

}
