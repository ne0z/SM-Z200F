/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "WebPage.h"

#include "Arguments.h"
#include "DataReference.h"
#include "DecoderAdapter.h"
#include "DrawingArea.h"
#include "InjectedBundle.h"
#include "InjectedBundleBackForwardList.h"
#include "LayerTreeHost.h"
#include "MessageID.h"
#include "NetscapePlugin.h"
#include "NotificationPermissionRequestManager.h"
#include "PageOverlay.h"
#include "PluginProxy.h"
#include "PluginView.h"
#include "PrintInfo.h"
#include "SessionState.h"
#include "ShareableBitmap.h"
#include "WebAlternativeTextClient.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListProxy.h"
#include "WebChromeClient.h"
#include "WebColorChooser.h"
#include "WebContextMenu.h"
#include "WebContextMenuClient.h"
#include "WebContextMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebFrame.h"
#include "WebFullScreenManager.h"
#include "WebGeolocationClient.h"
#include "WebGeometry.h"
#include "WebImage.h"
#include "WebInspector.h"
#include "WebInspectorClient.h"
#include "WebNotificationClient.h"
#include "WebOpenPanelResultListener.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroupProxy.h"
#include "WebPageProxyMessages.h"
#include "WebPopupMenu.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/AbstractDatabase.h>
#include <WebCore/ArchiveResource.h>
#include <WebCore/Chrome.h>
#include <WebCore/ContextMenuController.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/DragSession.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PrintContext.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RunLoop.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/ScriptValue.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/Settings.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/TextIterator.h>
#include <WebCore/markup.h>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

#include <WebCore/Range.h>
#include <WebCore/VisiblePosition.h>

#if ENABLE(MHTML)
#include <WebCore/MHTMLArchive.h>
#endif

#if ENABLE(PLUGIN_PROCESS)
#if PLATFORM(MAC)
#include "MachPort.h"
#endif
#endif

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryClient.h"
#endif

#if ENABLE(NETWORK_INFO)
#include "WebNetworkInfoClient.h"
#endif

#if ENABLE(WEB_INTENTS)
#include "IntentData.h"
#endif

#if ENABLE(VIBRATION)
#include "WebVibrationClient.h"
#endif

#if ENABLE(SCRIPTED_SPEECH)
#include "WebSpeechRecognitionClient.h"
#endif

#if ENABLE(TIZEN_REGISTER_PROTOCOL_HANDLER) || ENABLE(TIZEN_CUSTOM_SCHEME_HANDLER)
#include "WebRegisterProtocolHandlerClient.h"
#endif

#if ENABLE(TIZEN_REGISTER_CONTENT_HANDLER)
#include "WebRegisterContentHandlerClient.h"
#endif

#if PLATFORM(MAC)
#include "BuiltInPDFView.h"
#endif

#if PLATFORM(QT)
#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionClientQt.h"
#include "DeviceOrientationClientQt.h"
#endif
#include "HitTestResult.h"
#include <QMimeData>
#endif

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#include "DataObjectGtk.h"
#include "WebPrintOperationGtk.h"
#endif

#if ENABLE(TIZEN_MEDIA_STREAM)
#include "WebUserMediaClient.h"
#include "UserMediaPermissionRequestManager.h"
#endif

#if ENABLE(TIZEN_SUPPORT_WEBAPP_META_TAG)
#include <WebCore/NodeList.h>
#include "HTMLLinkElement.h"
#include "HTMLMetaElement.h"
#endif

#if ENABLE(TIZEN_GEOLOCATION)
#include <WebCore/GeolocationClientMock.h>
#include <WebCore/GeolocationController.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

#if ENABLE(TIZEN_INDEXED_DATABASE)
#include <WebCore/PageGroup.h>
#include <WebCore/GroupSettings.h>
#endif

#if ENABLE(TIZEN_ISF_PORT)
#include "NativeWebKeyboardEvent.h"
#endif

#if ENABLE(TIZEN_NATIVE_MEMORY_SNAPSHOT)
#include "AboutDataTizen.h"
#endif

#if ENABLE(TIZEN_DRAG_SUPPORT)
#include "DataObjectTizen.h"
#endif

#if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_TEXT_SELECTION_MODE)
#include "HTMLImageElement.h"
#endif

#if ENABLE(TIZEN_GSTREAMER_VIDEO)
#include "MediaResourceControllerGStreamerTizen.h"
#endif

#if ENABLE(TIZEN_WEBKIT2_TEXT_SELECTION)
#include "visible_units.h"
#include "htmlediting.h"
#endif

#if ENABLE(TIZEN_COMPRESSION_PROXY)
#include "ResourceHandle.h"
#include <UCProxySDK/ProxyRequestContext.h>
#endif

using namespace JSC;
using namespace WebCore;
using namespace std;

namespace WebKit {

class SendStopResponsivenessTimer {
public:
    SendStopResponsivenessTimer(WebPage* page)
        : m_page(page)
    {
    }
    
    ~SendStopResponsivenessTimer()
    {
        m_page->send(Messages::WebPageProxy::StopResponsivenessTimer());
    }

private:
    WebPage* m_page;
};

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, webPageCounter, ("WebPage"));

PassRefPtr<WebPage> WebPage::create(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    RefPtr<WebPage> page = adoptRef(new WebPage(pageID, parameters));

    if (page->pageGroup()->isVisibleToInjectedBundle() && WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->didCreatePage(page.get());

    return page.release();
}

WebPage::WebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
    : m_viewSize(parameters.viewSize)
    , m_useFixedLayout(false)
    , m_drawsBackground(true)
    , m_drawsTransparentBackground(false)
    , m_isInRedo(false)
    , m_isClosed(false)
    , m_tabToLinks(false)
#if PLATFORM(MAC)
    , m_windowIsVisible(false)
    , m_isSmartInsertDeleteEnabled(parameters.isSmartInsertDeleteEnabled)
    , m_layerHostingMode(parameters.layerHostingMode)
    , m_keyboardEventBeingInterpreted(0)
#elif PLATFORM(WIN)
    , m_nativeWindow(parameters.nativeWindow)
#elif PLATFORM(GTK)
    , m_accessibilityObject(0)
#endif
    , m_setCanStartMediaTimer(WebProcess::shared().runLoop(), this, &WebPage::setCanStartMediaTimerFired)
    , m_findController(this)
#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(QT)
    , m_tapHighlightController(this)
#endif
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    , m_activeColorChooser(0)
#endif
#if ENABLE(GEOLOCATION)
    , m_geolocationPermissionRequestManager(this)
#endif
    , m_pageID(pageID)
    , m_canRunBeforeUnloadConfirmPanel(parameters.canRunBeforeUnloadConfirmPanel)
    , m_canRunModal(parameters.canRunModal)
    , m_isRunningModal(false)
    , m_cachedMainFrameIsPinnedToLeftSide(false)
    , m_cachedMainFrameIsPinnedToRightSide(false)
    , m_canShortCircuitHorizontalWheelEvents(false)
    , m_numWheelEventHandlers(0)
    , m_cachedPageCount(0)
#if ENABLE(CONTEXT_MENUS)
    , m_isShowingContextMenu(false)
#endif
    , m_willGoToBackForwardItemCallbackEnabled(true)
#if PLATFORM(WIN)
    , m_gestureReachedScrollingLimit(false)
#endif
#if ENABLE(PAGE_VISIBILITY_API)
    , m_visibilityState(WebCore::PageVisibilityStateVisible)
#endif
#if ENABLE(TIZEN_RECORDING_SURFACE_SET)
    , m_recordingSurfaceSetEnable(false)
    , m_recordingSurfaceSetLoadStart(false)
    , m_recordingSurfaceSetLoadFinished(false)
    , m_recordingSurfaceSetSettings(false)
#endif
#if ENABLE(TIZEN_SYNC_REQUEST_ANIMATION_FRAME)
    , m_suspendedAnimationController(false)
#endif
    , m_inspectorClient(0)
#if ENABLE(TIZEN_ISF_PORT) || ENABLE(TIZEN_WEBKIT2_TEXT_SELECTION)
    , m_updateEditorStateTimer(WebProcess::shared().runLoop(), this, &WebPage::updateEditorStateTimerFired)
#endif
#if PLATFORM(TIZEN)
    , m_updateRectsTimer(WebProcess::shared().runLoop(), this, &WebPage::updateRectsTimerFired)
#endif
#if ENABLE(TIZEN_ISF_PORT)
    , m_prepareKeyDownEvent(false)
    , m_recalcFilterEvent(false)

#if ENABLE(TIZEN_WEBKIT2_INPUT_FORM_NAVIGATION)
    , m_currInputMethodContextID(0)
#endif

#endif
#if ENABLE(TIZEN_INTERCEPT_REQUEST)
    , m_isInterceptRequestEnabled(false)
#endif
#if ENABLE(TIZEN_ORIENTATION_EVENTS)
    , m_newOrientation(-1)
#endif
    , m_backgroundColor(Color::white)
#if ENABLE(TIZEN_USER_AGENT_WHITELIST)
    , m_customUserAgentSet(false)
#endif
#if ENABLE(TIZEN_WEBKIT2_SCROLL_POSITION_BUG_FIX)
    , m_shouldSyncVisibleContentRect(false)
#endif
{
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI("Creating WebProcess->webpage");
#endif
    ASSERT(m_pageID);
    // FIXME: This is a non-ideal location for this Setting and
    // 4ms should be adopted project-wide now, https://bugs.webkit.org/show_bug.cgi?id=61214
#if ENABLE(TIZEN_DOM_TIMER_MIN_INTERVAL_SET)
    Settings::setDefaultMinDOMTimerInterval(0.001);
#else
    Settings::setDefaultMinDOMTimerInterval(0.004);
#endif

    Page::PageClients pageClients;
    pageClients.chromeClient = new WebChromeClient(this);
#if ENABLE(CONTEXT_MENUS)
    pageClients.contextMenuClient = new WebContextMenuClient(this);
#endif
    pageClients.editorClient = new WebEditorClient(this);
#if ENABLE(DRAG_SUPPORT)
    pageClients.dragClient = new WebDragClient(this);
#endif
    pageClients.backForwardClient = WebBackForwardListProxy::create(this);
#if ENABLE(INSPECTOR)
    m_inspectorClient = new WebInspectorClient(this);
    pageClients.inspectorClient = m_inspectorClient;
#endif
#if USE(AUTOCORRECTION_PANEL)
    pageClients.alternativeTextClient = new WebAlternativeTextClient(this);
#endif
    
    m_page = adoptPtr(new Page(pageClients));

#if ENABLE(BATTERY_STATUS)
    WebCore::provideBatteryTo(m_page.get(), new WebBatteryClient(this));
#endif
#if ENABLE(GEOLOCATION)
#if ENABLE(TIZEN_GEOLOCATION) && ENABLE(TIZEN_WEBKIT2_EFL_WTR)
    if (WebProcess::shared().injectedBundle() && WebProcess::shared().injectedBundle()->testRunnerModeEnabled()) {
        GeolocationClientMock* mock = new GeolocationClientMock();
        WebCore::provideGeolocationTo(m_page.get(), mock);
        mock->setController(WebCore::GeolocationController::from(m_page.get()));
    } else
#endif // ENABLE(TIZEN_GEOLOCATION) && ENABLE(TIZEN_WEBKIT2_EFL_WTR)
    WebCore::provideGeolocationTo(m_page.get(), new WebGeolocationClient(this));
#endif
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(QT)
    WebCore::provideDeviceMotionTo(m_page.get(), new DeviceMotionClientQt);
    WebCore::provideDeviceOrientationTo(m_page.get(), new DeviceOrientationClientQt);
#endif
#if ENABLE(NETWORK_INFO)
    WebCore::provideNetworkInfoTo(m_page.get(), new WebNetworkInfoClient(this));
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::provideNotification(m_page.get(), new WebNotificationClient(this));
#endif

#if ENABLE(VIBRATION)
    WebCore::provideVibrationTo(m_page.get(), new WebVibrationClient(this));
#endif

#if ENABLE(SCRIPTED_SPEECH)
    WebCore::provideSpeechRecognitionTo(m_page.get(), new WebSpeechRecognitionClient(this));
#endif

#if ENABLE(TIZEN_MEDIA_STREAM)
    WebCore::provideUserMediaTo(m_page.get(), new WebUserMediaClient(this));
#endif

#if ENABLE(TIZEN_REGISTER_PROTOCOL_HANDLER) || ENABLE(TIZEN_CUSTOM_SCHEME_HANDLER)
    WebCore::provideRegisterProtocolHandlerTo(m_page.get(), new WebRegisterProtocolHandlerClient(this));
#endif

#if ENABLE(TIZEN_REGISTER_CONTENT_HANDLER)
    WebCore::provideRegisterContentHandlerTo(m_page.get(), new WebRegisterContentHandlerClient(this));
#endif

    // Qt does not yet call setIsInWindow. Until it does, just leave
    // this line out so plug-ins and video will work. Eventually all platforms
    // should call setIsInWindow and this comment and #if should be removed,
    // leaving behind the setCanStartMedia call.
#if !PLATFORM(QT)
    m_page->setCanStartMedia(false);
#endif

    updatePreferences(parameters.store);

    m_pageGroup = WebProcess::shared().webPageGroup(parameters.pageGroupData);
    m_page->setGroupName(m_pageGroup->identifier());
    m_page->setDeviceScaleFactor(parameters.deviceScaleFactor);

    platformInitialize();

    m_drawingArea = DrawingArea::create(this, parameters);
    m_drawingArea->setPaintingEnabled(false);

    m_mainFrame = WebFrame::createMainFrame(this);

    setUseFixedLayout(parameters.useFixedLayout);

    setDrawsBackground(parameters.drawsBackground);
    setDrawsTransparentBackground(parameters.drawsTransparentBackground);

    setPaginationMode(parameters.paginationMode);
    setPaginationBehavesLikeColumns(parameters.paginationBehavesLikeColumns);
    setPageLength(parameters.pageLength);
    setGapBetweenPages(parameters.gapBetweenPages);

    setMemoryCacheMessagesEnabled(parameters.areMemoryCacheClientCallsEnabled);

    setActive(parameters.isActive);
    setFocused(parameters.isFocused);
    setIsInWindow(parameters.isInWindow);

    m_userAgent = parameters.userAgent;

    WebBackForwardListProxy::setHighestItemIDFromUIProcess(parameters.highestUsedBackForwardItemID);
    
    if (!parameters.sessionState.isEmpty())
        restoreSession(parameters.sessionState);

    m_drawingArea->setPaintingEnabled(true);
    
    setMediaVolume(parameters.mediaVolume);

#ifndef NDEBUG
    webPageCounter.increment();
#endif
}

WebPage::~WebPage()
{
    if (m_backForwardList)
        m_backForwardList->detach();

    ASSERT(!m_page);

    m_sandboxExtensionTracker.invalidate();

    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->webPageDestroyed();

#ifndef NDEBUG
    webPageCounter.decrement();
#endif
}

void WebPage::dummy(bool&)
{
}

CoreIPC::Connection* WebPage::connection() const
{
    return WebProcess::shared().connection();
}

#if ENABLE(CONTEXT_MENUS)
void WebPage::initializeInjectedBundleContextMenuClient(WKBundlePageContextMenuClient* client)
{
    m_contextMenuClient.initialize(client);
}
#endif

void WebPage::initializeInjectedBundleEditorClient(WKBundlePageEditorClient* client)
{
    m_editorClient.initialize(client);
}

void WebPage::initializeInjectedBundleFormClient(WKBundlePageFormClient* client)
{
    m_formClient.initialize(client);
}

void WebPage::initializeInjectedBundleLoaderClient(WKBundlePageLoaderClient* client)
{
    m_loaderClient.initialize(client);
}

void WebPage::initializeInjectedBundlePolicyClient(WKBundlePagePolicyClient* client)
{
    m_policyClient.initialize(client);
}

void WebPage::initializeInjectedBundleResourceLoadClient(WKBundlePageResourceLoadClient* client)
{
    m_resourceLoadClient.initialize(client);
}

void WebPage::initializeInjectedBundleUIClient(WKBundlePageUIClient* client)
{
    m_uiClient.initialize(client);
}

#if ENABLE(FULLSCREEN_API)
void WebPage::initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClient* client)
{
    m_fullScreenClient.initialize(client);
}
#endif

void WebPage::initializeInjectedBundleDiagnosticLoggingClient(WKBundlePageDiagnosticLoggingClient* client)
{
    m_logDiagnosticMessageClient.initialize(client);
}

PassRefPtr<Plugin> WebPage::createPlugin(WebFrame* frame, HTMLPlugInElement* pluginElement, const Plugin::Parameters& parameters)
{
    String pluginPath;
    bool blocked = false;

    if (!WebProcess::shared().connection()->sendSync(
            Messages::WebContext::GetPluginPath(parameters.mimeType, parameters.url.string()), 
            Messages::WebContext::GetPluginPath::Reply(pluginPath, blocked), 0)) {
        return 0;
    }

    if (blocked) {
        if (pluginElement->renderer()->isEmbeddedObject())
            toRenderEmbeddedObject(pluginElement->renderer())->setPluginUnavailabilityReason(RenderEmbeddedObject::InsecurePluginVersion);

        send(Messages::WebPageProxy::DidBlockInsecurePluginVersion(parameters.mimeType, parameters.url.string()));
        return 0;
    }

    if (pluginPath.isNull()) {
#if PLATFORM(MAC)
        if (parameters.mimeType == "application/pdf"
            || (parameters.mimeType.isEmpty() && parameters.url.path().lower().endsWith(".pdf")))
            return BuiltInPDFView::create(frame);
#else
        UNUSED_PARAM(frame);
#endif
        return 0;
    }

#if ENABLE(PLUGIN_PROCESS)
    return PluginProxy::create(pluginPath);
#elif ENABLE(NETSCAPE_PLUGIN_API)
    NetscapePlugin::setSetExceptionFunction(NPRuntimeObjectMap::setGlobalException);
    return NetscapePlugin::create(NetscapePluginModule::getOrCreate(pluginPath));
#else
    return 0;
#endif
}

EditorState WebPage::editorState() const
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    ASSERT(frame);

    EditorState result;
    result.selectionIsNone = frame->selection()->isNone();
    result.selectionIsRange = frame->selection()->isRange();
    result.isContentEditable = frame->selection()->isContentEditable();
    result.isContentRichlyEditable = frame->selection()->isContentRichlyEditable();
    result.isInPasswordField = frame->selection()->isInPasswordField();
    result.hasComposition = frame->editor()->hasComposition();
    result.shouldIgnoreCompositionSelectionChange = frame->editor()->ignoreCompositionSelectionChange();

#if ENABLE(TIZEN_ISF_PORT)
    result.inputMethodContextID = reinterpret_cast<uintptr_t>(frame->selection()->rootEditableElement());
#endif

#if PLATFORM(QT)
    size_t location = 0;
    size_t length = 0;

    Element* selectionRoot = frame->selection()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : frame->document()->documentElement();

    if (!scope)
        return result;

    if (scope->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(scope);
        if (input->isTelephoneField())
            result.inputMethodHints |= Qt::ImhDialableCharactersOnly;
        else if (input->isNumberField())
            result.inputMethodHints |= Qt::ImhDigitsOnly;
        else if (input->isEmailField()) {
            result.inputMethodHints |= Qt::ImhEmailCharactersOnly;
            result.inputMethodHints |= Qt::ImhNoAutoUppercase;
        } else if (input->isURLField()) {
            result.inputMethodHints |= Qt::ImhUrlCharactersOnly;
            result.inputMethodHints |= Qt::ImhNoAutoUppercase;
        } else if (input->isPasswordField()) {
            // Set ImhHiddenText flag for password fields. The Qt platform
            // is responsible for determining which widget will receive input
            // method events for password fields.
            result.inputMethodHints |= Qt::ImhHiddenText;
            result.inputMethodHints |= Qt::ImhNoAutoUppercase;
            result.inputMethodHints |= Qt::ImhNoPredictiveText;
            result.inputMethodHints |= Qt::ImhSensitiveData;
        }
    }

    if (selectionRoot)
        result.editorRect = frame->view()->contentsToWindow(selectionRoot->getPixelSnappedRect());

    RefPtr<Range> range;
    if (result.hasComposition && (range = frame->editor()->compositionRange())) {
        frame->editor()->getCompositionSelection(result.anchorPosition, result.cursorPosition);

        result.compositionRect = frame->view()->contentsToWindow(range->boundingBox());
    }

    if (!result.hasComposition && !result.selectionIsNone && (range = frame->selection()->selection().firstRange())) {
        TextIterator::getLocationAndLengthFromRange(scope, range.get(), location, length);
        bool baseIsFirst = frame->selection()->selection().isBaseFirst();

        result.cursorPosition = (baseIsFirst) ? location + length : location;
        result.anchorPosition = (baseIsFirst) ? location : location + length;
        result.selectedText = range->text();
    }

    if (range)
        result.cursorRect = frame->view()->contentsToWindow(frame->editor()->firstRectForRange(range.get()));

    // FIXME: We should only transfer innerText when it changes and do this on the UI side.
    if (result.isContentEditable && !result.isInPasswordField) {
        result.surroundingText = scope->innerText();
        if (result.hasComposition) {
            // The anchor is always the left position when they represent a composition.
            result.surroundingText.remove(result.anchorPosition, result.cursorPosition - result.anchorPosition);
        }
    }
#endif

    return result;
}

String WebPage::renderTreeExternalRepresentation() const
{
    return externalRepresentation(m_mainFrame->coreFrame(), RenderAsTextBehaviorNormal);
}

uint64_t WebPage::renderTreeSize() const
{
    if (!m_page)
        return 0;
    return m_page->renderTreeSize().treeSize;
}

void WebPage::setPaintedObjectsCounterThreshold(uint64_t threshold)
{
    if (!m_page)
        return;
    m_page->setRelevantRepaintedObjectsCounterThreshold(threshold);
}

void WebPage::setTracksRepaints(bool trackRepaints)
{
    if (FrameView* view = mainFrameView())
        view->setTracksRepaints(trackRepaints);
}

bool WebPage::isTrackingRepaints() const
{
    if (FrameView* view = mainFrameView())
        return view->isTrackingRepaints();

    return false;
}

void WebPage::resetTrackedRepaints()
{
    if (FrameView* view = mainFrameView())
        view->resetTrackedRepaints();
}

PassRefPtr<ImmutableArray> WebPage::trackedRepaintRects()
{
    FrameView* view = mainFrameView();
    if (!view)
        return ImmutableArray::create();

    const Vector<IntRect>& rects = view->trackedRepaintRects();
    size_t size = rects.size();
    if (!size)
        return ImmutableArray::create();

    Vector<RefPtr<APIObject> > vector;
    vector.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; ++i)
        vector.uncheckedAppend(WebRect::create(toAPI(rects[i])));

    return ImmutableArray::adopt(vector);
}

void WebPage::executeEditingCommand(const String& commandName, const String& argument)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    frame->editor()->command(commandName).execute(argument);
}

bool WebPage::isEditingCommandEnabled(const String& commandName)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return false;
    
    Editor::Command command = frame->editor()->command(commandName);
    return command.isSupported() && command.isEnabled();
}
    
void WebPage::clearMainFrameName()
{
    if (Frame* frame = mainFrame())
        frame->tree()->clearName();
}

#if USE(ACCELERATED_COMPOSITING)
void WebPage::enterAcceleratedCompositingMode(GraphicsLayer* layer)
{
    m_drawingArea->setRootCompositingLayer(layer);
}

void WebPage::exitAcceleratedCompositingMode()
{
    m_drawingArea->setRootCompositingLayer(0);
}
#endif

void WebPage::close()
{
    if (m_isClosed)
        return;
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" close() Invoked ,this[%p]",this);
#endif
    m_isClosed = true;
#if ENABLE(TIZEN_COMPRESSION_PROXY)
    proxy::ProxyRequestContext *context = WebCore::ResourceHandle::getProxyRequestContext();
    if (context) {
        TIZEN_LOGI("[UCPROXY] context->destoryProxyLoaderForFrame( FrameId[%llu] )", m_mainFrame->frameID());
        context->destoryProxyLoaderForFrame(m_mainFrame->coreFrame()->getUCFrameID());
    }
#endif
    if (pageGroup()->isVisibleToInjectedBundle() && WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->willDestroyPage(this);

#if ENABLE(INSPECTOR)
    m_inspector = 0;
#endif
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManager = 0;
#endif
#if ENABLE(TIZEN_GSTREAMER_VIDEO)
    MediaResourceControllerGStreamerTizen::mediaResourceController().clearAllLists();
#endif
    if (m_activePopupMenu) {
        m_activePopupMenu->disconnectFromPage();
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" m_activePopupMenu[%p] will be cleared now",m_activePopupMenu.get());
#endif
        m_activePopupMenu = 0;
    }

    if (m_activeOpenPanelResultListener) {
        m_activeOpenPanelResultListener->disconnectFromPage();
        m_activeOpenPanelResultListener = 0;
    }

#if ENABLE(INPUT_TYPE_COLOR)
    if (m_activeColorChooser) {
        m_activeColorChooser->disconnectFromPage();
        m_activeColorChooser = 0;
    }
#endif

    m_sandboxExtensionTracker.invalidate();

    m_underlayPage = nullptr;
    m_printContext = nullptr;
    m_mainFrame->coreFrame()->loader()->detachFromParent();
    m_page = nullptr;
    m_drawingArea = nullptr;

    bool isRunningModal = m_isRunningModal;
    m_isRunningModal = false;

    // The WebPage can be destroyed by this call.
    WebProcess::shared().removeWebPage(m_pageID);

    if (isRunningModal)
        WebProcess::shared().runLoop()->stop();
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" close() done ,this[%p]",this);
#endif

}

void WebPage::tryClose()
{
    SendStopResponsivenessTimer stopper(this);

    if (!m_mainFrame->coreFrame()->loader()->shouldClose()) {
        send(Messages::WebPageProxy::StopResponsivenessTimer());
        return;
    }

    send(Messages::WebPageProxy::ClosePage(true));
}

void WebPage::sendClose()
{
    send(Messages::WebPageProxy::ClosePage(false));
}

void WebPage::loadURL(const String& url, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    loadURLRequest(ResourceRequest(KURL(KURL(), url)), sandboxExtensionHandle);
}

void WebPage::loadURLRequest(const ResourceRequest& request, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    m_mainFrame->coreFrame()->loader()->load(request, false);
}

void WebPage::loadData(PassRefPtr<SharedBuffer> sharedBuffer, const String& MIMEType, const String& encodingName, const KURL& baseURL, const KURL& unreachableURL)
{
    SendStopResponsivenessTimer stopper(this);

    ResourceRequest request(baseURL);
    SubstituteData substituteData(sharedBuffer, MIMEType, encodingName, unreachableURL);
    m_mainFrame->coreFrame()->loader()->load(request, substituteData, false);
}

void WebPage::loadHTMLString(const String& htmlString, const String& baseURLString)
{
#if ENABLE(TIZEN_LOAD_HTML_STRING_AS_UTF8)
    CString html = htmlString.utf8();
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(html.data()), html.length());
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_SECURE_LOGI("[Loader] baseURLString[%.256s]", baseURLString.utf8().data());
#endif
    loadData(sharedBuffer, "text/html", "utf-8", baseURL, KURL());
#else
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters()), htmlString.length() * sizeof(UChar));
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
    loadData(sharedBuffer, "text/html", "utf-16", baseURL, KURL());
#endif
}

void WebPage::loadAlternateHTMLString(const String& htmlString, const String& baseURLString, const String& unreachableURLString)
{
#if ENABLE(TIZEN_LOAD_HTML_STRING_AS_UTF8)
    CString html = htmlString.utf8();
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(html.data()), html.length());
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
    KURL unreachableURL = unreachableURLString.isEmpty() ? KURL() : KURL(KURL(), unreachableURLString);
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_SECURE_LOGI("[Loader] baseURLString[%.256s]", baseURLString.utf8().data());
#endif
    loadData(sharedBuffer, "text/html", "utf-8", baseURL, unreachableURL);
#else
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(htmlString.characters()), htmlString.length() * sizeof(UChar));
    KURL baseURL = baseURLString.isEmpty() ? blankURL() : KURL(KURL(), baseURLString);
    KURL unreachableURL = unreachableURLString.isEmpty() ? KURL() : KURL(KURL(), unreachableURLString);
    loadData(sharedBuffer, "text/html", "utf-16", baseURL, unreachableURL);
#endif
}

void WebPage::loadPlainTextString(const String& string)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(string.characters()), string.length() * sizeof(UChar));
    loadData(sharedBuffer, "text/plain", "utf-16", blankURL(), KURL());
}

void WebPage::loadWebArchiveData(const CoreIPC::DataReference& webArchiveData)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(webArchiveData.data()), webArchiveData.size() * sizeof(uint8_t));
    loadData(sharedBuffer, "application/x-webarchive", "utf-16", blankURL(), KURL());
}

#if PLATFORM(TIZEN)
void WebPage::loadContentsbyMimeType(const CoreIPC::DataReference& contents, const String& mimeType, const String& encoding, const String& baseURL)
{
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(contents.data()), contents.size() * sizeof(uint8_t));
    KURL baseKURL(WebCore::KURL(), baseURL);
    loadData(sharedBuffer, mimeType, encoding, baseKURL, KURL());
}
#endif

void WebPage::linkClicked(const String& url, const WebMouseEvent& event)
{
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return;

    RefPtr<Event> coreEvent;
    if (event.type() != WebEvent::NoType)
        coreEvent = MouseEvent::create(eventNames().clickEvent, frame->document()->defaultView(), platform(event), 0, 0);

    frame->loader()->loadFrameRequest(FrameLoadRequest(frame->document()->securityOrigin(), ResourceRequest(url)), 
        false, false, coreEvent.get(), 0, MaybeSendReferrer);
}

void WebPage::stopLoadingFrame(uint64_t frameID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;

    frame->coreFrame()->loader()->stopForUserCancel();
}

void WebPage::stopLoading()
{
    SendStopResponsivenessTimer stopper(this);

    m_mainFrame->coreFrame()->loader()->stopForUserCancel();
}

void WebPage::setDefersLoading(bool defersLoading)
{
    m_page->setDefersLoading(defersLoading);
}

void WebPage::reload(bool reloadFromOrigin, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    SendStopResponsivenessTimer stopper(this);

    m_sandboxExtensionTracker.beginLoad(m_mainFrame.get(), sandboxExtensionHandle);
    m_mainFrame->coreFrame()->loader()->reload(reloadFromOrigin);
}

void WebPage::goForward(uint64_t backForwardItemID)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    m_page->goToItem(item, FrameLoadTypeForward);
}

void WebPage::goBack(uint64_t backForwardItemID)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    m_page->goToItem(item, FrameLoadTypeBack);
}

void WebPage::goToBackForwardItem(uint64_t backForwardItemID)
{
    SendStopResponsivenessTimer stopper(this);

    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    ASSERT(item);
    if (!item)
        return;

    m_page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void WebPage::tryRestoreScrollPosition()
{
    m_page->mainFrame()->loader()->history()->restoreScrollPositionAndViewState();
}

void WebPage::layoutIfNeeded()
{
    if (m_mainFrame->coreFrame()->view())
        m_mainFrame->coreFrame()->view()->updateLayoutAndStyleIfNeededRecursive();

    if (m_underlayPage) {
        if (FrameView *frameView = m_underlayPage->mainFrameView())
            frameView->updateLayoutAndStyleIfNeededRecursive();
    }
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    FrameView* view = m_page->mainFrame()->view();

#if USE(TILED_BACKING_STORE)
    // If we are resizing to content ignore external attempts.
    if (view->useFixedLayout())
        return;
#endif

    if (m_viewSize == viewSize)
        return;

    view->resize(viewSize);
    view->setNeedsLayout();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), viewSize));
    
    m_viewSize = viewSize;
}

#if USE(TILED_BACKING_STORE)
#if ENABLE(TIZEN_DEFER_FIXED_LAYER_UPDATE)
void WebPage::setFixedVisibleContentRectWithoutFixedElementUpdate(const IntRect& rect)
{
    m_page->mainFrame()->view()->setFixedVisibleContentRect(rect, false);
}
#endif

void WebPage::setFixedVisibleContentRect(const IntRect& rect)
{
    ASSERT(m_useFixedLayout);

    m_page->mainFrame()->view()->setFixedVisibleContentRect(rect);
}

void WebPage::setResizesToContentsUsingLayoutSize(const IntSize& targetLayoutSize)
{
    ASSERT(m_useFixedLayout);
    ASSERT(!targetLayoutSize.isEmpty());

    FrameView* view = m_page->mainFrame()->view();

    view->setDelegatesScrolling(true);
    view->setUseFixedLayout(true);
    view->setPaintsEntireContents(true);

    if (view->fixedLayoutSize() == targetLayoutSize)
        return;

    m_page->settings()->setAcceleratedCompositingForFixedPositionEnabled(true);
    m_page->settings()->setFixedElementsLayoutRelativeToFrame(true);
    m_page->settings()->setFixedPositionCreatesStackingContext(true);

    // Always reset even when empty. This also takes care of the relayout.
    setFixedLayoutSize(targetLayoutSize);
}

void WebPage::resizeToContentsIfNeeded()
{
    ASSERT(m_useFixedLayout);

    FrameView* view = m_page->mainFrame()->view();

    if (!view->useFixedLayout())
        return;

    IntSize newSize = view->contentsSize().expandedTo(view->fixedLayoutSize());
#if PLATFORM(TIZEN)
    if ((newSize == m_viewSize) || newSize.isZero()){
#if ENABLE(TIZEN_DLOG_SUPPORT)
        TIZEN_LOGI("Do not update the UIProcess as newSize == m_viewSize : %d OR newSize.isZero() : %d", (newSize == m_viewSize), newSize.isZero());
#endif
        return;
    }
#else
    if (newSize == m_viewSize)
        return;
#endif

    m_viewSize = newSize;
    view->resize(newSize);
    view->setNeedsLayout();
}

void WebPage::sendViewportAttributesChanged()
{
    ASSERT(m_useFixedLayout);

    // Viewport properties have no impact on zero sized fixed viewports.
    if (m_viewportSize.isEmpty())
        return;

    // Recalculate the recommended layout size, when the available size (device pixel) changes.
    Settings* settings = m_page->settings();

#if PLATFORM(TIZEN)
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" args type: [%d], scale: [%.1f, %.1f, %.1f], layout: [%.1f, %.1f], userScalable: [%.1f]"
        , static_cast<int>(m_page->viewportArguments().type), m_page->viewportArguments().initialScale
        , m_page->viewportArguments().minimumScale, m_page->viewportArguments().maximumScale
        , m_page->viewportArguments().width, m_page->viewportArguments().height, m_page->viewportArguments().userScalable);
#endif

    // As android and iphone does, we set layout width to 980 for desktop content
    int minimumLayoutFallbackWidth = settings->layoutFallbackWidth();
#else
    int minimumLayoutFallbackWidth = std::max(settings->layoutFallbackWidth(), m_viewportSize.width());
#endif

    // If unset  we use the viewport dimensions. This fits with the behavior of desktop browsers.
    int deviceWidth = (settings->deviceWidth() > 0) ? settings->deviceWidth() : m_viewportSize.width();
    int deviceHeight = (settings->deviceHeight() > 0) ? settings->deviceHeight() : m_viewportSize.height();

    ViewportAttributes attr = computeViewportAttributes(m_page->viewportArguments(), minimumLayoutFallbackWidth, deviceWidth, deviceHeight, m_page->deviceScaleFactor(), m_viewportSize);
    WebCore::restrictMinimumScaleFactorToViewportSize(attr, m_viewportSize);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attr);

#if ENABLE(TIZEN_VIEWPORT_META_TAG)
    // On WebProcess, JS runs before visible content rect is updated by UIProcess
    // In this case, JS returns incorrect inner size value because scale factor is not updated yet
    // So, we update fixed visible content rect at here.
    // FIXME: scale factor needs to be calculated and set fixed visible content rect

    // if constrainsScrollingToContentEdge is true, scroll position will be adjusted with contents size.
    // disable this to avoid adjust scroll position on setFixedVisibleContentRect() because contents size is not fixed yet.
    bool constrainsScrollingToContentEdge = mainFrameView()->constrainsScrollingToContentEdge();
    mainFrameView()->setConstrainsScrollingToContentEdge(false);

    FloatSize contentFixedSize = m_viewportSize;
    contentFixedSize.scale(1 / (attr.initialScale * attr.devicePixelRatio));

#if PLATFORM(TIZEN) && USE(UI_SIDE_COMPOSITING)
    //When navigating to a new page by clicking a link or rotating the device
    //BackingStore visibleRect and ScaleFactor need to be reset, otherwise it will contain old values of previous page and leads to many display issues
    //Especially navigating from viewport page to non-viewport page and vice versa.
    if (mainFrameView() && (mainFrameView()->fixedLayoutSize().width() != attr.layoutSize.width() || m_newOrientation != -1))
        m_drawingArea->layerTreeHost()->setVisibleContentsRectAndScaleForBackingStore(IntRect(mainFrameView()->scrollPosition(), roundedIntSize(contentFixedSize)),(attr.initialScale * attr.devicePixelRatio));
#endif

    if (fullScreenManager()->isUsingHwVideoOverlay())
        mainFrameView()->setFixedVisibleContentRect(IntRect(mainFrameView()->scrollPosition(), expandedIntSize(contentFixedSize)));
    else
        mainFrameView()->setFixedVisibleContentRect(IntRect(mainFrameView()->scrollPosition(), roundedIntSize(contentFixedSize)));

    // If viewport meta tag is not defined or initial scale factor is not defined,
    // initial scale factor can be defined by "Default View" setting of UIProcess.
    // Therefore we set initialScale as ValueAuto as WebKit opensource does, to handle it with setting on UIProcess.
    // You can find this implementation on WebKit bug.
    // https://bugs.webkit.org/show_bug.cgi?id=102392
    if (m_page->viewportArguments().type == ViewportArguments::Implicit
        || m_page->viewportArguments().initialScale == ViewportArguments::ValueAuto)
        attr.initialScale = ViewportArguments::ValueAuto;
#endif

#if ENABLE(TIZEN_ORIENTATION_EVENTS)
    if (m_mainFrame->coreFrame() && m_newOrientation != -1) {
        m_mainFrame->coreFrame()->sendOrientationChangeEvent();
        for (WebCore::Frame* child = m_mainFrame->coreFrame()->tree()->firstChild(); child; child = child->tree()->nextSibling())
            child->sendOrientationChangeEvent();
        m_newOrientation = -1;

#if ENABLE(TIZEN_FULLSCREEN_API) && !ENABLE(TIZEN_PRERENDERING_FOR_ROTATION)
        if (fullScreenManager()->isFullscreen()) {
            fullScreenManager()->didRotateScreen();
        }
#endif
    }
#endif

    setResizesToContentsUsingLayoutSize(IntSize(static_cast<int>(attr.layoutSize.width()), static_cast<int>(attr.layoutSize.height())));

#if ENABLE(TIZEN_VIEWPORT_META_TAG)
    // recover constrainsScrollingToContentEdge
    mainFrameView()->setConstrainsScrollingToContentEdge(constrainsScrollingToContentEdge);
#endif

    send(Messages::WebPageProxy::DidChangeViewportProperties(attr));
}

void WebPage::setViewportSize(const IntSize& size)
{
    ASSERT(m_useFixedLayout);

    if (m_viewportSize == size)
        return;

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" viewportSize: [%d, %d]", size.width(), size.height());
#endif

     m_viewportSize = size;

     sendViewportAttributesChanged();
}

#endif

void WebPage::scrollMainFrameIfNotAtMaxScrollPosition(const IntSize& scrollOffset)
{
    Frame* frame = m_page->mainFrame();

    IntPoint scrollPosition = frame->view()->scrollPosition();
    IntPoint maximumScrollPosition = frame->view()->maximumScrollPosition();

    // If the current scroll position in a direction is the max scroll position 
    // we don't want to scroll at all.
    IntSize newScrollOffset;
    if (scrollPosition.x() < maximumScrollPosition.x())
        newScrollOffset.setWidth(scrollOffset.width());
    if (scrollPosition.y() < maximumScrollPosition.y())
        newScrollOffset.setHeight(scrollOffset.height());

    if (newScrollOffset.isZero())
        return;

    frame->view()->setScrollPosition(frame->view()->scrollPosition() + newScrollOffset);
}

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);

    if (m_underlayPage) {
        m_underlayPage->drawRect(graphicsContext, rect);

        graphicsContext.beginTransparencyLayer(1);
        m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
        graphicsContext.endTransparencyLayer();
        return;
    }

    m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
}

#if ENABLE(TIZEN_WEBKIT2_CONTEXT_MENU_TEXT_SELECTION_MODE)
void WebPage::selectLink(WebCore::IntPoint positionForSelection, bool& result)
{
    result = false;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    HitTestResult hitTestResult = m_page->contextMenuController()->hitTestResult();

    if (hitTestResult.innerNonSharedNode()) {
        Frame* hitTestFrame = hitTestResult.innerNonSharedNode()->document()->frame();
        if (frame != hitTestFrame) {
            frame->page()->focusController()->setFocusedFrame(hitTestFrame);
            frame = hitTestFrame;
        }
    }

    RefPtr<Range> range;
    Element* URLElement = hitTestResult.URLElement();
    if (URLElement) {
        // To check whether Select Start is allowed or not in case selection is disabled by Javascript.
        if (!URLElement->dispatchEvent(Event::create(eventNames().selectstartEvent, true, true)))
            return;

        for (Node* childNode = URLElement->firstChild(); childNode; childNode = childNode->traverseNextNode(URLElement)) {
            RenderObject* r = childNode->renderer();
            if (r && (r->isText() || r->isImage())) {
                // Whitespaces and escape sequence need to skip while creating the range,
                // because DOM visible position returns empty position ,for these elements ,
                // so need to handle this range before creating the range, above condition
                // handled the <br> condition.
                if (r->isText()) {
                    RenderText* text = toRenderText(r);
                    if (text->textLength() == 1
                        && (text->text()->characters()[0] == ' '
                        || text->text()->characters()[0] == '\n'
                        || text->text()->characters()[0] == '\t'))
                        continue;
                }
                if (!range)
                    range = VisibleSelection::selectionFromContentsOfNode(childNode).toNormalizedRange();
                else {
                    RefPtr<Range> extendedRange = VisibleSelection::selectionFromContentsOfNode(childNode).toNormalizedRange();
                    range->setEnd(childNode, extendedRange->endOffset());
                }
            }
        }
        if(!range)
            range = VisibleSelection::selectionFromContentsOfNode(URLElement).toNormalizedRange();
    } else {
#if ENABLE(TIZEN_PROD_DETECT_CONTENTS)
        bool isDetectedContents = false;
        String detectedContents;
        IntRect detectedContentsRect;
        if (m_page->settings()->detectContentsAutomatically()) {
            positionForSelection = hitTestResult.innerNonSharedNode()->renderer()->absoluteBoundingBoxRect().center();
            isDetectedContents = detectContents(positionForSelection, detectedContents, detectedContentsRect);
        }

        if (isDetectedContents) {
            IntPoint startPoint = IntPoint(detectedContentsRect.x(), detectedContentsRect.center().y());
            IntPoint endPoint = IntPoint(detectedContentsRect.maxX(), detectedContentsRect.center().y());

            VisiblePosition visibleStartPos = frame->visiblePositionForPoint(frameView->windowToContents(startPoint));
            VisiblePosition visibleEndPos = frame->visiblePositionForPoint(frameView->windowToContents(endPoint));

            range = makeRange(visibleStartPos, visibleEndPos);
        }
        else
#endif
            range = VisibleSelection::selectionFromContentsOfNode(hitTestResult.innerNonSharedNode()).toNormalizedRange();
    }

    WebCore::FrameSelection* frameSelection = frame->selection();
    if (!frameSelection)
        return;

    EAffinity affinity = frameSelection->affinity();
    frameSelection->setSelectedRange(range.get(), affinity, true);

    if (frameSelection->isRange())
        result = true;

    return;
}
#endif
#if ENABLE(TIZEN_ORIENTATION_EVENTS)
void WebPage::sendOrientationChangeEvent(int newOrientation)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame || frame->orientation() == newOrientation) {
#if ENABLE(TIZEN_FULLSCREEN_API)
        if (fullScreenManager()->isFullscreen())
            fullScreenManager()->willRotateScreen();
#endif
        return;
    }

    // Set orientation first, and fire orientation change event by calling sendOrientationChangeEvent()
    // sendOrientationChangeEvent() should be called after webview's resize if it's necessary
    int oldOrientation = frame->orientation();
    frame->setOrientation(newOrientation);

    // Now, Setting Orientation for children.
    for (WebCore::Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setOrientation(newOrientation);

    // If it's rotated 90 or -90 degrees,
    // orientation change event will be fired on WebPage::sendViewportAttributesChanged
    if (fabs(oldOrientation - newOrientation) != 180) {
        m_newOrientation = newOrientation;
#if ENABLE(TIZEN_FULLSCREEN_API)
        if (fullScreenManager()->isFullscreen())
            fullScreenManager()->willRotateScreen();
#endif
        return;
    }

    // If it's rotated 180 degrees, ewk_view will not be resized
    // So, fire orientation change event here
    frame->sendOrientationChangeEvent();
    for (WebCore::Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->sendOrientationChangeEvent();

#if ENABLE(TIZEN_FULLSCREEN_API)
    if (fullScreenManager()->isFullscreen()) {
        fullScreenManager()->willRotateScreen();
        fullScreenManager()->didRotateScreen();
    }
#endif
}
#endif

void WebPage::drawPageOverlay(GraphicsContext& graphicsContext, const IntRect& rect)
{
    ASSERT(m_pageOverlay);

    GraphicsContextStateSaver stateSaver(graphicsContext);
    graphicsContext.clip(rect);
    m_pageOverlay->drawRect(graphicsContext, rect);
}

double WebPage::textZoomFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->textZoomFactor();
}

void WebPage::setTextZoomFactor(double zoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setTextZoomFactor(static_cast<float>(zoomFactor));
}

double WebPage::pageZoomFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void WebPage::setPageZoomFactor(double zoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setPageZoomFactor(static_cast<float>(zoomFactor));
}

void WebPage::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    return frame->setPageAndTextZoomFactors(static_cast<float>(pageZoomFactor), static_cast<float>(textZoomFactor));
}

void WebPage::windowScreenDidChange(uint64_t displayID)
{
    m_page->windowScreenDidChange(static_cast<PlatformDisplayID>(displayID));
}

void WebPage::setViewMode(Page::ViewMode mode)
{
    m_page->setViewMode(mode);
}

void WebPage::scalePage(double scale, const IntPoint& origin)
{
    m_page->setPageScaleFactor(scale, origin);

    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->pageScaleFactorDidChange();

    send(Messages::WebPageProxy::PageScaleFactorDidChange(scale));
}

double WebPage::pageScaleFactor() const
{
    return m_page->pageScaleFactor();
}

void WebPage::setDeviceScaleFactor(float scaleFactor)
{
    if (scaleFactor == m_page->deviceScaleFactor())
        return;

    m_page->setDeviceScaleFactor(scaleFactor);

    // Tell all our plug-in views that the device scale factor changed.
#if PLATFORM(MAC)
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setDeviceScaleFactor(scaleFactor);
#endif

    if (m_findController.isShowingOverlay()) {
        // We must have updated layout to get the selection rects right.
        layoutIfNeeded();
        m_findController.deviceScaleFactorDidChange();
    }
}

float WebPage::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

void WebPage::setUseFixedLayout(bool fixed)
{
    m_useFixedLayout = fixed;

    FrameView* view = mainFrameView();
    if (!view)
        return;

    view->setUseFixedLayout(fixed);
    if (!fixed)
        setFixedLayoutSize(IntSize());
}

void WebPage::setFixedLayoutSize(const IntSize& size)
{
    FrameView* view = mainFrameView();
    if (!view || view->fixedLayoutSize() == size)
        return;

    view->setFixedLayoutSize(size);
    // Do not force it until the first layout, this would then become our first layout prematurely.
    if (view->didFirstLayout())
        view->forceLayout();
}

void WebPage::setPaginationMode(uint32_t mode)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.mode = static_cast<Page::Pagination::Mode>(mode);
    m_page->setPagination(pagination);
}

void WebPage::setPaginationBehavesLikeColumns(bool behavesLikeColumns)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.behavesLikeColumns = behavesLikeColumns;
    m_page->setPagination(pagination);
}

void WebPage::setPageLength(double pageLength)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.pageLength = pageLength;
    m_page->setPagination(pagination);
}

void WebPage::setGapBetweenPages(double gap)
{
    Page::Pagination pagination = m_page->pagination();
    pagination.gap = gap;
    m_page->setPagination(pagination);
}

void WebPage::installPageOverlay(PassRefPtr<PageOverlay> pageOverlay)
{
    bool shouldFadeIn = true;
    
    if (m_pageOverlay) {
        m_pageOverlay->setPage(0);

        if (pageOverlay) {
            // We're installing a page overlay when a page overlay is already active.
            // In this case we don't want to fade in the new overlay.
            shouldFadeIn = false;
        }
    }

    m_pageOverlay = pageOverlay;
    m_pageOverlay->setPage(this);

    if (shouldFadeIn)
        m_pageOverlay->startFadeInAnimation();

    m_drawingArea->didInstallPageOverlay();
#if PLATFORM(WIN)
    send(Messages::WebPageProxy::DidInstallOrUninstallPageOverlay(true));
#endif

    m_pageOverlay->setNeedsDisplay();
}

void WebPage::uninstallPageOverlay(PageOverlay* pageOverlay, bool fadeOut)
{
    if (pageOverlay != m_pageOverlay)
        return;

    if (fadeOut) {
        m_pageOverlay->startFadeOutAnimation();
        return;
    }

    m_pageOverlay->setPage(0);
    m_pageOverlay = nullptr;

    m_drawingArea->didUninstallPageOverlay();
#if PLATFORM(WIN)
    send(Messages::WebPageProxy::DidInstallOrUninstallPageOverlay(false));
#endif
}

#if PLATFORM(EFL) && PLATFORM(TIZEN)
PassRefPtr<WebImage> WebPage::scaledSnapshotInViewCoordinates(const IntRect& rect, double scaleFactor, ImageOptions options)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return 0;

#if PLATFORM(TIZEN)
    /* If frameRect is empty, frameview will not going to paint anything at all. So better return from here.
        With this , we can avoid black screenshot contents and unneccessary paint and copy operation.
    */
    if (frameView->frameRect().isEmpty())
        return 0;
#endif

    IntSize size(ceil(rect.width() * scaleFactor), ceil(rect.height() * scaleFactor));
    RefPtr<WebImage> snapshot = WebImage::create(size, options);
    if (!snapshot->bitmap())
        return 0;

    OwnPtr<WebCore::GraphicsContext> graphicsContext = snapshot->bitmap()->createGraphicsContext();
    graphicsContext->scale(FloatSize(scaleFactor, scaleFactor));
    graphicsContext->translate(-rect.x(), -rect.y());

    frameView->updateLayoutAndStyleIfNeededRecursive();

    PaintBehavior oldBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    frameView->paint(graphicsContext.get(), rect);
    frameView->setPaintBehavior(oldBehavior);

    return snapshot.release();
}

PassRefPtr<WebImage> WebPage::snapshotInViewCoordinates(const IntRect& rect, ImageOptions options)
{
    return scaledSnapshotInViewCoordinates(rect, 1, options);
}
#else
PassRefPtr<WebImage> WebPage::snapshotInViewCoordinates(const IntRect& rect, ImageOptions options)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return 0;

    IntSize bitmapSize = rect.size();
    float deviceScaleFactor = corePage()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);

    RefPtr<WebImage> snapshot = WebImage::create(bitmapSize, options);
    if (!snapshot->bitmap())
        return 0;
    
    OwnPtr<WebCore::GraphicsContext> graphicsContext = snapshot->bitmap()->createGraphicsContext();
    graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
    graphicsContext->translate(-rect.x(), -rect.y());

    frameView->updateLayoutAndStyleIfNeededRecursive();

    PaintBehavior oldBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    frameView->paint(graphicsContext.get(), rect);
    frameView->setPaintBehavior(oldBehavior);

    return snapshot.release();
}
#endif

PassRefPtr<WebImage> WebPage::scaledSnapshotInDocumentCoordinates(const IntRect& rect, double scaleFactor, ImageOptions options)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return 0;

    float combinedScaleFactor = scaleFactor * corePage()->deviceScaleFactor();
    IntSize size(ceil(rect.width() * combinedScaleFactor), ceil(rect.height() * combinedScaleFactor));
    RefPtr<WebImage> snapshot = WebImage::create(size, options);
    if (!snapshot->bitmap())
        return 0;

    OwnPtr<WebCore::GraphicsContext> graphicsContext = snapshot->bitmap()->createGraphicsContext();
    graphicsContext->applyDeviceScaleFactor(combinedScaleFactor);
    graphicsContext->translate(-rect.x(), -rect.y());

    frameView->updateLayoutAndStyleIfNeededRecursive();

    PaintBehavior oldBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    frameView->paintContents(graphicsContext.get(), rect);
    frameView->setPaintBehavior(oldBehavior);

    return snapshot.release();
}

PassRefPtr<WebImage> WebPage::snapshotInDocumentCoordinates(const IntRect& rect, ImageOptions options)
{
    return scaledSnapshotInDocumentCoordinates(rect, 1, options);
}

void WebPage::pageDidScroll()
{
    m_uiClient.pageDidScroll(this);

    send(Messages::WebPageProxy::PageDidScroll());
}

#if USE(TILED_BACKING_STORE)
void WebPage::pageDidRequestScroll(const IntPoint& point)
{
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" scroll position: [%d, %d]", point.x(), point.y());
#endif
#if ENABLE(TIZEN_WEBKIT2_SCROLL_POSITION_BUG_FIX)
    if (m_shouldSyncVisibleContentRect) {
        // Need to update the visible content rect immediately so that DOM Window properties have
        // correct values before the actual delegates scroll happens.
        FrameView* view = m_page->mainFrame()->view();

        // We need to floor the viewport here as to allow aligning the content in device units. If not,
        // it might not be possible to scroll the last pixel and that affects fixed position elements.
        float scaleFactor = corePage()->chrome()->client()->contentsScaleFactor();
        FloatRect bounds;
        bounds.setWidth(std::max(float(0), view->contentsSize().width() - floorf(m_viewportSize.width() / scaleFactor)));
        bounds.setHeight(std::max(float(0), view->contentsSize().height() - floorf(m_viewportSize.height() / scaleFactor)));
        FloatPoint position;

        position.setX(clampTo(point.x(), bounds.x(), bounds.width()));
        position.setY(clampTo(point.y(), bounds.y(), bounds.height()));
        setFixedVisibleContentRect(IntRect(roundedIntPoint(position), view->fixedVisibleContentRect().size()));
    }
#endif
    send(Messages::WebPageProxy::PageDidRequestScroll(point));
}
#endif

#if ENABLE(TIZEN_WEBKIT2_HISTORICAL_RESTORE_VISIBLE_CONTENT_RECT)
IntRect WebPage::adjustVisibleContentRect(IntRect visibleContentRect, float targetScale)
{
    IntSize contentsSize = mainFrameView()->contentsSize();
    contentsSize.scale(targetScale);
    if (contentsSize.width() < visibleContentRect.width())
        visibleContentRect.setX(0);
    else
        visibleContentRect.setX(clampTo(visibleContentRect.x(), 0, contentsSize.width() - visibleContentRect.width()));

    if (contentsSize.height() < visibleContentRect.height())
        visibleContentRect.setY(0);
    else
        visibleContentRect.setY(clampTo(visibleContentRect.y(), 0, contentsSize.height() - visibleContentRect.height()));

    return visibleContentRect;
}

void WebPage::pageDidRequestRestoreVisibleContentRect(const IntPoint& point, float scale)
{
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI(" scale factor: [%.2f], scroll position: [%d, %d]", scale, point.x(), point.y());
#endif
#if PLATFORM(TIZEN) && USE(UI_SIDE_COMPOSITING)
    if (scale) {
        IntPoint scrollPosition = IntPoint();
        FloatSize contentFixedSize = m_viewportSize;
        contentFixedSize.scale(1 / scale);

        IntRect adjustedVisibleContentRect = adjustVisibleContentRect(IntRect(point, m_viewportSize), scale);

        // Restore only if it's possible
        if (adjustedVisibleContentRect.location() == point)
          scrollPosition = point;

        //When navigating to cached page by clicking back/forward button
        //BackingStore visibleRect and ScaleFactor need to be restore/reset back, otherwise it will contain old values of previous page and leads to many display issues
        m_drawingArea->layerTreeHost()->setVisibleContentsRectAndScaleForBackingStore(IntRect(scrollPosition, roundedIntSize(contentFixedSize)), scale);
    }
#endif

    send(Messages::WebPageProxy::pageDidRequestRestoreVisibleContentRect(point, scale));
}
#endif

#if ENABLE(CONTEXT_MENUS)
WebContextMenu* WebPage::contextMenu()
{
    if (!m_contextMenu)
        m_contextMenu = WebContextMenu::create(this);
    return m_contextMenu.get();
}
#endif

// Events 

static const WebEvent* g_currentEvent = 0;

// FIXME: WebPage::currentEvent is used by the plug-in code to avoid having to convert from DOM events back to
// WebEvents. When we get the event handling sorted out, this should go away and the Widgets should get the correct
// platform events passed to the event handler code.
const WebEvent* WebPage::currentEvent()
{
    return g_currentEvent;
}

class CurrentEvent {
public:
    explicit CurrentEvent(const WebEvent& event)
        : m_previousCurrentEvent(g_currentEvent)
    {
        g_currentEvent = &event;
    }

    ~CurrentEvent()
    {
        g_currentEvent = m_previousCurrentEvent;
    }

private:
    const WebEvent* m_previousCurrentEvent;
};

#if ENABLE(CONTEXT_MENUS)
static bool isContextClick(const PlatformMouseEvent& event)
{
    if (event.button() == WebCore::RightButton)
        return true;

#if PLATFORM(MAC)
    // FIXME: this really should be about OSX-style UI, not about the Mac port
    if (event.button() == WebCore::LeftButton && event.ctrlKey())
        return true;
#endif

    return false;
}

static bool handleContextMenuEvent(const PlatformMouseEvent& platformMouseEvent, WebPage* page)
{
    IntPoint point = page->corePage()->mainFrame()->view()->windowToContents(platformMouseEvent.position());
    HitTestResult result = page->corePage()->mainFrame()->eventHandler()->hitTestResultAtPoint(point, false);

    Frame* frame = page->corePage()->mainFrame();
#if !ENABLE(TIZEN_CONTEXT_MENU_WEBKIT_2)
    if (result.innerNonSharedNode())
        frame = result.innerNonSharedNode()->document()->frame();
#endif

    bool handled = frame->eventHandler()->sendContextMenuEvent(platformMouseEvent);
    if (handled)
        page->contextMenu()->show();

#if ENABLE(TIZEN_DRAG_SUPPORT)
    if(result.isDragSupport()) {
        if (result.innerNonSharedNode())
            frame = result.innerNonSharedNode()->document()->frame();

        PlatformMouseEvent event(frame->view()->windowToContents(platformMouseEvent.position()), platformMouseEvent.globalPosition(), LeftButton, PlatformEvent::MousePressed, 1, false, false, false, false, currentTime());
        frame->page()->dragController()->setPosition(event);
    }
#endif

    return handled;
}
#endif

static bool handleMouseEvent(const WebMouseEvent& mouseEvent, WebPage* page, bool onlyUpdateScrollbars)
{
    Frame* frame = page->corePage()->mainFrame();
    if (!frame->view())
        return false;

    PlatformMouseEvent platformMouseEvent = platform(mouseEvent);

    switch (platformMouseEvent.type()) {
        case PlatformEvent::MousePressed: {
#if ENABLE(CONTEXT_MENUS)
            if (isContextClick(platformMouseEvent))
                page->corePage()->contextMenuController()->clearContextMenu();
#endif

#if ENABLE(TIZEN_CONTEXT_MENU_WEBKIT_2)
            bool handled;
            if (!isContextClick(platformMouseEvent))
                handled = frame->eventHandler()->handleMousePressEvent(platformMouseEvent);
#else
            bool handled = frame->eventHandler()->handleMousePressEvent(platformMouseEvent);
#endif
#if ENABLE(CONTEXT_MENUS)
            if (isContextClick(platformMouseEvent))
                handled = handleContextMenuEvent(platformMouseEvent, page);
#endif
#if PLATFORM(GTK)
            bool gtkMouseButtonPressHandled = page->handleMousePressedEvent(platformMouseEvent);
            handled = handled || gtkMouseButtonPressHandled;
#endif

            return handled;
        }
        case PlatformEvent::MouseReleased:
            return frame->eventHandler()->handleMouseReleaseEvent(platformMouseEvent);
        case PlatformEvent::MouseMoved:
            if (onlyUpdateScrollbars)
                return frame->eventHandler()->passMouseMovedEventToScrollbars(platformMouseEvent);
            return frame->eventHandler()->mouseMoved(platformMouseEvent);
        default:
            ASSERT_NOT_REACHED();
            return false;
    }
}

void WebPage::mouseEvent(const WebMouseEvent& mouseEvent)
{
#if ENABLE(CONTEXT_MENUS)
    // Don't try to handle any pending mouse events if a context menu is showing.
    if (m_isShowingContextMenu) {
        send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), false));
        return;
    }
#endif

    bool handled = false;
    
    if (m_pageOverlay) {
        // Let the page overlay handle the event.
        handled = m_pageOverlay->mouseEvent(mouseEvent);
    }

    if (!handled) {
        CurrentEvent currentEvent(mouseEvent);

        // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
        // button is currently pressed. It is possible that neither of those things will be true since on 
        // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one 
        // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
        // efficient scrollbars-only version of the event.
        bool onlyUpdateScrollbars = !(m_page->focusController()->isActive() || (mouseEvent.button() != WebMouseEvent::NoButton));
        handled = handleMouseEvent(mouseEvent, this, onlyUpdateScrollbars);
    }

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(mouseEvent.type()), handled));
}

void WebPage::mouseEventSyncForTesting(const WebMouseEvent& mouseEvent, bool& handled)
{
    handled = m_pageOverlay && m_pageOverlay->mouseEvent(mouseEvent);

    if (!handled) {
        CurrentEvent currentEvent(mouseEvent);

        // We need to do a full, normal hit test during this mouse event if the page is active or if a mouse
        // button is currently pressed. It is possible that neither of those things will be true since on 
        // Lion when legacy scrollbars are enabled, WebKit receives mouse events all the time. If it is one 
        // of those cases where the page is not active and the mouse is not pressed, then we can fire a more
        // efficient scrollbars-only version of the event.
        bool onlyUpdateScrollbars = !(m_page->focusController()->isActive() || (mouseEvent.button() != WebMouseEvent::NoButton));
        handled = handleMouseEvent(mouseEvent, this, onlyUpdateScrollbars);
    }
}

static bool handleWheelEvent(const WebWheelEvent& wheelEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    PlatformWheelEvent platformWheelEvent = platform(wheelEvent);
    return frame->eventHandler()->handleWheelEvent(platformWheelEvent);
}

void WebPage::wheelEvent(const WebWheelEvent& wheelEvent)
{
    CurrentEvent currentEvent(wheelEvent);

    bool handled = handleWheelEvent(wheelEvent, m_page.get());
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(wheelEvent.type()), handled));
}

void WebPage::wheelEventSyncForTesting(const WebWheelEvent& wheelEvent, bool& handled)
{
    CurrentEvent currentEvent(wheelEvent);

    handled = handleWheelEvent(wheelEvent, m_page.get());
}

static bool handleKeyEvent(const WebKeyboardEvent& keyboardEvent, Page* page)
{
    if (!page->mainFrame()->view())
        return false;

    if (keyboardEvent.type() == WebEvent::Char && keyboardEvent.isSystemKey())
        return page->focusController()->focusedOrMainFrame()->eventHandler()->handleAccessKey(platform(keyboardEvent));
    return page->focusController()->focusedOrMainFrame()->eventHandler()->keyEvent(platform(keyboardEvent));
}

#if ENABLE(TIZEN_ISF_PORT)
void WebPage::keyEvent(const NativeWebKeyboardEvent& keyboardEvent)
#else
void WebPage::keyEvent(const WebKeyboardEvent& keyboardEvent)
#endif
{
    CurrentEvent currentEvent(keyboardEvent);

#if ENABLE(TIZEN_ISF_PORT)
    m_prepareKeyDownEvent = false;
#endif

    bool handled = handleKeyEvent(keyboardEvent, m_page.get());
    // FIXME: Platform default behaviors should be performed during normal DOM event dispatch (in most cases, in default keydown event handler).
    if (!handled)
        handled = performDefaultBehaviorForKeyEvent(keyboardEvent);

#if ENABLE(TIZEN_ISF_PORT)
    if (keyboardEvent.type() == WebEvent::KeyUp)
        m_keyPressCommands.clear();
#endif

    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(keyboardEvent.type()), handled));
}

void WebPage::keyEventSyncForTesting(const WebKeyboardEvent& keyboardEvent, bool& handled)
{
    CurrentEvent currentEvent(keyboardEvent);

    handled = handleKeyEvent(keyboardEvent, m_page.get());
    if (!handled)
        handled = performDefaultBehaviorForKeyEvent(keyboardEvent);
}

#if ENABLE(GESTURE_EVENTS)
static bool handleGestureEvent(const WebGestureEvent& gestureEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    PlatformGestureEvent platformGestureEvent = platform(gestureEvent);
    return frame->eventHandler()->handleGestureEvent(platformGestureEvent);
}

void WebPage::gestureEvent(const WebGestureEvent& gestureEvent)
{
    CurrentEvent currentEvent(gestureEvent);

#if ENABLE(TIZEN_PROD_DETECT_CONTENTS)
    bool isDetectedContents = false;
    String detectedContents;
    IntRect detectedContentsRect;
    if (m_page->settings()->detectContentsAutomatically()) {
        if (gestureEvent.type() == WebEvent::GestureSingleTap)
            isDetectedContents = detectContents(gestureEvent.position(), detectedContents, detectedContentsRect);
    }
#endif
    bool handled = handleGestureEvent(gestureEvent, m_page.get());
#if ENABLE(TIZEN_ISF_PORT) || ENABLE(TIZEN_WEBKIT2_TEXT_SELECTION)
    if (m_updateEditorStateTimer.isActive()) {
        m_updateEditorStateTimer.stop();
        updateEditorStateTimerFired();
    }
#endif
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(gestureEvent.type()), handled));
#if ENABLE(TIZEN_PROD_DETECT_CONTENTS)
    if (isDetectedContents)
        send(Messages::WebPageProxy::DidDetectContents(gestureEvent.position(), detectedContents, detectedContentsRect, RenderStyle::initialTapHighlightColor()));
#endif
}
#endif

void WebPage::validateCommand(const String& commandName, uint64_t callbackID)
{
    bool isEnabled = false;
    int32_t state = 0;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (frame) {
        Editor::Command command = frame->editor()->command(commandName);
        state = command.state();
        isEnabled = command.isSupported() && command.isEnabled();
    }

    send(Messages::WebPageProxy::ValidateCommandCallback(commandName, isEnabled, state, callbackID));
}

void WebPage::executeEditCommand(const String& commandName)
{
    executeEditingCommand(commandName, String());
}

uint64_t WebPage::restoreSession(const SessionState& sessionState)
{
    const BackForwardListItemVector& list = sessionState.list();
    size_t size = list.size();
    uint64_t currentItemID = 0;
    for (size_t i = 0; i < size; ++i) {
        WebBackForwardListItem* webItem = list[i].get();
        DecoderAdapter decoder(webItem->backForwardData().data(), webItem->backForwardData().size());
        
        RefPtr<HistoryItem> item = HistoryItem::decodeBackForwardTree(webItem->url(), webItem->title(), webItem->originalURL(), decoder);
        if (!item) {
            TIZEN_LOGI("Failed to decode a HistoryItem from session state data.");
            return 0;
        }
        
        if (i == sessionState.currentIndex())
            currentItemID = webItem->itemID();
        
        WebBackForwardListProxy::addItemFromUIProcess(list[i]->itemID(), item.release());
    }    
    ASSERT(currentItemID);
    return currentItemID;
}

void WebPage::restoreSessionAndNavigateToCurrentItem(const SessionState& sessionState)
{
    if (uint64_t currentItemID = restoreSession(sessionState))
        goToBackForwardItem(currentItemID);
}

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(QT)
void WebPage::highlightPotentialActivation(const IntPoint& point, const IntSize& area)
{
    if (point == IntPoint::zero()) {
        // An empty point deactivates the highlighting.
        tapHighlightController().hideHighlight();
    } else {
        Frame* mainframe = m_page->mainFrame();
        Node* activationNode = 0;
        Node* adjustedNode = 0;
        IntPoint adjustedPoint;

#if ENABLE(TOUCH_ADJUSTMENT)
        if (!mainframe->eventHandler()->bestClickableNodeForTouchPoint(point, IntSize(area.width() / 2, area.height() / 2), adjustedPoint, adjustedNode))
            return;

#else
        HitTestResult result = mainframe->eventHandler()->hitTestResultAtPoint(mainframe->view()->windowToContents(point), /*allowShadowContent*/ false, /*ignoreClipping*/ true);
        adjustedNode = result.innerNode();
#endif
        // Find the node to highlight. This is not the same as the node responding the tap gesture, because many
        // pages has a global click handler and we do not want to highlight the body.
        // Instead find the enclosing link or focusable element, or the last enclosing inline element.
        for (Node* node = adjustedNode; node; node = node->parentOrHostNode()) {
            if (node->isMouseFocusable() || node->isLink()) {
                activationNode = node;
                break;
            }
            if (node->renderer() && node->renderer()->isInline())
                activationNode = node;
            else if (activationNode)
                break;
        }

        if (activationNode)
            tapHighlightController().highlight(activationNode);
    }
}
#endif

static bool handleTouchEvent(const WebTouchEvent& touchEvent, Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame->view())
        return false;

    return frame->eventHandler()->handleTouchEvent(platform(touchEvent));
}

void WebPage::touchEvent(const WebTouchEvent& touchEvent)
{
    CurrentEvent currentEvent(touchEvent);

    bool handled = handleTouchEvent(touchEvent, m_page.get());

#if ENABLE(TIZEN_CSS_OVERFLOW_SCROLL_ACCELERATION)
    static bool checkOverflow = false;
    if (touchEvent.type() == WebEvent::TouchStart && touchEvent.touchPoints().size() == 1) {
        checkOverflow = true;
    } else if (checkOverflow && touchEvent.type() == WebEvent::TouchMove && touchEvent.touchPoints().size() == 1) {
        bool pressed = false;
        uint32_t id = 0;
#if ENABLE(TIZEN_CSS_OVERFLOW_SCROLL_ACCELERATION_ON_UI_SIDE)
        setPressedNodeAtPoint(touchEvent.touchPoints()[0].position(), true, pressed, id);
#else
        setPressedNodeAtPoint(touchEvent.touchPoints()[0].position(), false, pressed, id);
#endif
        send(Messages::WebPageProxy::SetOverflowResult(pressed, id));
        checkOverflow = false;
    }
#endif
    send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(touchEvent.type()), handled));
}

void WebPage::touchEventSyncForTesting(const WebTouchEvent& touchEvent, bool& handled)
{
    CurrentEvent currentEvent(touchEvent);
    handled = handleTouchEvent(touchEvent, m_page.get());
}
#endif

void WebPage::scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->scrollRecursively(direction, granularity);
}

void WebPage::logicalScroll(Page* page, ScrollLogicalDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->logicalScrollRecursively(direction, granularity);
}

void WebPage::scrollBy(uint32_t scrollDirection, uint32_t scrollGranularity)
{
    scroll(m_page.get(), static_cast<ScrollDirection>(scrollDirection), static_cast<ScrollGranularity>(scrollGranularity));
}

void WebPage::centerSelectionInVisibleArea()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    
    frame->selection()->revealSelection(ScrollAlignment::alignCenterAlways);
    m_findController.showFindIndicatorInSelection();
}

void WebPage::setActive(bool isActive)
{
    m_page->focusController()->setActive(isActive);

#if PLATFORM(MAC)    
    // Tell all our plug-in views that the window focus changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setWindowIsFocused(isActive);
#endif
}

void WebPage::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree()->traverseNext()) {
        if (FrameView* view = coreFrame->view())
            view->setTransparent(!drawsBackground);
    }

    m_drawingArea->pageBackgroundTransparencyChanged();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
}

void WebPage::setDrawsTransparentBackground(bool drawsTransparentBackground)
{
    if (m_drawsTransparentBackground == drawsTransparentBackground)
        return;

    m_drawsTransparentBackground = drawsTransparentBackground;

    Color backgroundColor = drawsTransparentBackground ? Color::transparent : Color::white;
    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree()->traverseNext()) {
        if (FrameView* view = coreFrame->view())
            view->setBaseBackgroundColor(backgroundColor);
    }

    m_drawingArea->pageBackgroundTransparencyChanged();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
}

void WebPage::viewWillStartLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    if (Frame* frame = m_page->focusController()->focusedOrMainFrame()) {
        if (FrameView* view = frame->view())
            view->willStartLiveResize();
    }
}

void WebPage::viewWillEndLiveResize()
{
    if (!m_page)
        return;

    // FIXME: This should propagate to all ScrollableAreas.
    if (Frame* frame = m_page->focusController()->focusedOrMainFrame()) {
        if (FrameView* view = frame->view())
            view->willEndLiveResize();
    }
}

void WebPage::setFocused(bool isFocused)
{
    m_page->focusController()->setFocused(isFocused);
}

void WebPage::setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent& event)
{
    if (!m_page || !m_page->focusController())
        return;

    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    frame->document()->setFocusedNode(0);

    if (isKeyboardEventValid && event.type() == WebEvent::KeyDown) {
        PlatformKeyboardEvent platformEvent(platform(event));
        platformEvent.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown);
        m_page->focusController()->setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, KeyboardEvent::create(platformEvent, frame->document()->defaultView()).get());
        return;
    }

    m_page->focusController()->setInitialFocus(forward ? FocusDirectionForward : FocusDirectionBackward, 0);
}

void WebPage::setWindowResizerSize(const IntSize& windowResizerSize)
{
    if (m_windowResizerSize == windowResizerSize)
        return;

    m_windowResizerSize = windowResizerSize;

    for (Frame* coreFrame = m_mainFrame->coreFrame(); coreFrame; coreFrame = coreFrame->tree()->traverseNext()) {
        FrameView* view = coreFrame->view();
        if (view)
            view->windowResizerRectChanged();
    }
}

void WebPage::setCanStartMediaTimerFired()
{
    if (m_page)
        m_page->setCanStartMedia(true);
}

void WebPage::setIsInWindow(bool isInWindow)
{
    if (!isInWindow) {
        m_setCanStartMediaTimer.stop();
        m_page->setCanStartMedia(false);
        m_page->willMoveOffscreen();
    } else {
        // Defer the call to Page::setCanStartMedia() since it ends up sending a syncrhonous messages to the UI process
        // in order to get plug-in connections, and the UI process will be waiting for the Web process to update the backing
        // store after moving the view into a window, until it times out and paints white. See <rdar://problem/9242771>.
        m_setCanStartMediaTimer.startOneShot(0);
        m_page->didMoveOnscreen();
    }
}

void WebPage::didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction, uint64_t downloadID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame) {
#if ENABLE(TIZEN_DLOG_SUPPORT)
        TIZEN_LOGI("[Loader] !frame return!!");
#endif
        return;
    }
    frame->didReceivePolicyDecision(listenerID, static_cast<PolicyAction>(policyAction), downloadID);
}

#if ENABLE(TIZEN_INTERCEPT_REQUEST)
void WebPage::setInterceptRequestEnabled(bool interceptRequestEnabled)
{
    TIZEN_SECURE_LOGI("[INTERCEPT] interceptRequestEnabled [%d]", interceptRequestEnabled);
    m_isInterceptRequestEnabled = interceptRequestEnabled;
}

void WebPage::setInterceptResponse(uint64_t frameID, uint64_t listenerID, String headers, const CoreIPC::DataReference& body)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;
    ResourceHandle *handle = frame->getHandleForInterceptRequest(listenerID);
    if (!handle)
        return;
    frame->removeInterceptRequest(listenerID);
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(reinterpret_cast<const char*>(body.data()), body.size() * sizeof(uint8_t));
    handle->receivedInterceptResponse(headers, sharedBuffer->data(), sharedBuffer->size());
}

void WebPage::ignoreInterceptResponse(uint64_t frameID, uint64_t listenerID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;
    ResourceHandle *handle = frame->getHandleForInterceptRequest(listenerID);
    if (!handle)
        return;
    frame->removeInterceptRequest(listenerID);
    handle->ignoreInterceptResponse();
}
#endif

void WebPage::show()
{
    send(Messages::WebPageProxy::ShowPage());
}

#if ENABLE(TIZEN_USER_AGENT_WHITELIST)
void WebPage::setUserAgent(const String& userAgent, bool customUserAgentSet)
{
    m_userAgent = userAgent;
    m_customUserAgentSet = customUserAgentSet;
}
#else
void WebPage::setUserAgent(const String& userAgent)
{
    m_userAgent = userAgent;
}
#endif

#if ENABLE(TIZEN_CUSTOM_HEADERS)
void WebPage::addCustomHeader(const String& name, const String& value)
{
    m_customHeaders.add(name, value);
}

void WebPage::removeCustomHeader(const String& name)
{
    m_customHeaders.remove(name);
}

void WebPage::clearCustomHeaders()
{
    m_customHeaders.clear();
}

WebCore::HTTPHeaderMap WebPage::customHeaders()
{
    return m_customHeaders;
}
#endif

void WebPage::suspendActiveDOMObjectsAndAnimations()
{
    m_page->suspendActiveDOMObjectsAndAnimations();
}

void WebPage::resumeActiveDOMObjectsAndAnimations()
{
    m_page->resumeActiveDOMObjectsAndAnimations();

    // We need to repaint on resume to kickstart animated painting again.
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
}

IntPoint WebPage::screenToWindow(const IntPoint& point)
{
    IntPoint windowPoint;
    sendSync(Messages::WebPageProxy::ScreenToWindow(point), Messages::WebPageProxy::ScreenToWindow::Reply(windowPoint));
    return windowPoint;
}
    
IntRect WebPage::windowToScreen(const IntRect& rect)
{
    IntRect screenRect;
    sendSync(Messages::WebPageProxy::WindowToScreen(rect), Messages::WebPageProxy::WindowToScreen::Reply(screenRect));
    return screenRect;
}

IntRect WebPage::windowResizerRect() const
{
    if (m_windowResizerSize.isEmpty())
        return IntRect();

    IntSize frameViewSize;
    if (Frame* coreFrame = m_mainFrame->coreFrame()) {
        if (FrameView* view = coreFrame->view())
            frameViewSize = view->size();
    }

    return IntRect(frameViewSize.width() - m_windowResizerSize.width(), frameViewSize.height() - m_windowResizerSize.height(), 
                   m_windowResizerSize.width(), m_windowResizerSize.height());
}

KeyboardUIMode WebPage::keyboardUIMode()
{
    bool fullKeyboardAccessEnabled = WebProcess::shared().fullKeyboardAccessEnabled();
    return static_cast<KeyboardUIMode>((fullKeyboardAccessEnabled ? KeyboardAccessFull : KeyboardAccessDefault) | (m_tabToLinks ? KeyboardAccessTabsToLinks : 0));
}

void WebPage::runJavaScriptInMainFrame(const String& script, uint64_t callbackID)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    // Retain the SerializedScriptValue at this level so it (and the internal data) lives
    // long enough for the DataReference to be encoded by the sent message.
    RefPtr<SerializedScriptValue> serializedResultValue;
    CoreIPC::DataReference dataReference;

    JSLockHolder lock(JSDOMWindow::commonJSGlobalData());
    if (JSValue resultValue = m_mainFrame->coreFrame()->script()->executeScript(script, true).jsValue()) {
        if ((serializedResultValue = SerializedScriptValue::create(m_mainFrame->jsContext(), toRef(m_mainFrame->coreFrame()->script()->globalObject(mainThreadNormalWorld())->globalExec(), resultValue), 0)))
            dataReference = serializedResultValue->data();
    }

    send(Messages::WebPageProxy::ScriptValueCallback(dataReference, callbackID));
}

#if ENABLE(TIZEN_WEBKIT2_FORM_DATABASE)
void WebPage::runJavaScriptInFocusedFrame(const String& script, uint64_t callbackID)
{
    RefPtr<SerializedScriptValue> serializedResultValue;
    CoreIPC::DataReference dataReference;

    JSLockHolder lock(JSDOMWindow::commonJSGlobalData());

    Frame* focusedFrame = m_page->focusController()->focusedOrMainFrame();

    if (JSValue resultValue = focusedFrame->script()->executeScript(script, true).jsValue()) {
        if ((serializedResultValue = SerializedScriptValue::create(m_mainFrame->jsContext(), toRef(focusedFrame->script()->globalObject(mainThreadNormalWorld())->globalExec(), resultValue), 0)))
            dataReference = serializedResultValue->data();
    }

    send(Messages::WebPageProxy::ScriptValueCallback(dataReference, callbackID));
}
#endif

void WebPage::getContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

#if ENABLE(MHTML)
void WebPage::getContentsAsMHTMLData(uint64_t callbackID, bool useBinaryEncoding)
{
    CoreIPC::DataReference dataReference;

    RefPtr<SharedBuffer> buffer = useBinaryEncoding
        ? MHTMLArchive::generateMHTMLDataUsingBinaryEncoding(m_page.get())
        : MHTMLArchive::generateMHTMLData(m_page.get());

    if (buffer)
        dataReference = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}
#endif

void WebPage::getRenderTreeExternalRepresentation(uint64_t callbackID)
{
    String resultString = renderTreeExternalRepresentation();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getSelectionOrContentsAsString(uint64_t callbackID)
{
    String resultString = m_mainFrame->selectionAsString();
    if (resultString.isEmpty())
        resultString = m_mainFrame->contentsAsString();
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getSourceForFrame(uint64_t frameID, uint64_t callbackID)
{
    String resultString;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID))
       resultString = frame->source();

    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

void WebPage::getMainResourceDataOfFrame(uint64_t frameID, uint64_t callbackID)
{
    CoreIPC::DataReference dataReference;

    RefPtr<SharedBuffer> buffer;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID)) {
        if (DocumentLoader* loader = frame->coreFrame()->loader()->documentLoader()) {
            if ((buffer = loader->mainResourceData()))
                dataReference = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
        }
    }

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

static PassRefPtr<SharedBuffer> resourceDataForFrame(Frame* frame, const KURL& resourceURL)
{
    DocumentLoader* loader = frame->loader()->documentLoader();
    if (!loader)
        return 0;

    RefPtr<ArchiveResource> subresource = loader->subresource(resourceURL);
    if (!subresource)
        return 0;

    return subresource->data();
}

void WebPage::getResourceDataFromFrame(uint64_t frameID, const String& resourceURLString, uint64_t callbackID)
{
    CoreIPC::DataReference dataReference;
    KURL resourceURL(KURL(), resourceURLString);

    RefPtr<SharedBuffer> buffer;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID)) {
        buffer = resourceDataForFrame(frame->coreFrame(), resourceURL);
        if (!buffer) {
            // Try to get the resource data from the cache.
            buffer = cachedResponseDataForURL(resourceURL);
        }

        if (buffer)
            dataReference = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
    }

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::getWebArchiveOfFrame(uint64_t frameID, uint64_t callbackID)
{
    CoreIPC::DataReference dataReference;

#if PLATFORM(MAC) || PLATFORM(WIN)
    RetainPtr<CFDataRef> data;
    if (WebFrame* frame = WebProcess::shared().webFrame(frameID)) {
        if ((data = frame->webArchiveData(0, 0)))
            dataReference = CoreIPC::DataReference(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
    }
#endif

    send(Messages::WebPageProxy::DataCallback(dataReference, callbackID));
}

void WebPage::forceRepaintWithoutCallback()
{
    m_drawingArea->forceRepaint();
}

void WebPage::forceRepaint(uint64_t callbackID)
{
    if (m_drawingArea->forceRepaintAsync(callbackID))
        return;

    forceRepaintWithoutCallback();
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

#if ENABLE(WEB_INTENTS)
void WebPage::deliverIntentToFrame(uint64_t frameID, const IntentData& intentData)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;

    frame->deliverIntent(intentData);
}
#endif

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    WebPreferencesStore::removeTestRunnerOverrides();
    updatePreferences(store);
}

void WebPage::updatePreferences(const WebPreferencesStore& store)
{
    Settings* settings = m_page->settings();

    m_tabToLinks = store.getBoolValueForKey(WebPreferencesKey::tabsToLinksKey());

    // FIXME: This should be generated from macro expansion for all preferences,
    // but we currently don't match the naming of WebCore exactly so we are
    // handrolling the boolean and integer preferences until that is fixed.

#define INITIALIZE_SETTINGS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) settings->set##KeyUpper(store.get##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key()));

    FOR_EACH_WEBKIT_STRING_PREFERENCE(INITIALIZE_SETTINGS)

#undef INITIALIZE_SETTINGS

    settings->setScriptEnabled(store.getBoolValueForKey(WebPreferencesKey::javaScriptEnabledKey()));
    settings->setLoadsImagesAutomatically(store.getBoolValueForKey(WebPreferencesKey::loadsImagesAutomaticallyKey()));
    settings->setLoadsSiteIconsIgnoringImageLoadingSetting(store.getBoolValueForKey(WebPreferencesKey::loadsSiteIconsIgnoringImageLoadingPreferenceKey()));
    settings->setPluginsEnabled(store.getBoolValueForKey(WebPreferencesKey::pluginsEnabledKey()));
    settings->setJavaEnabled(store.getBoolValueForKey(WebPreferencesKey::javaEnabledKey()));
    settings->setJavaEnabledForLocalFiles(store.getBoolValueForKey(WebPreferencesKey::javaEnabledForLocalFilesKey()));    
    settings->setOfflineWebApplicationCacheEnabled(store.getBoolValueForKey(WebPreferencesKey::offlineWebApplicationCacheEnabledKey()));
    settings->setLocalStorageEnabled(store.getBoolValueForKey(WebPreferencesKey::localStorageEnabledKey()));
    settings->setXSSAuditorEnabled(store.getBoolValueForKey(WebPreferencesKey::xssAuditorEnabledKey()));
    settings->setFrameFlatteningEnabled(store.getBoolValueForKey(WebPreferencesKey::frameFlatteningEnabledKey()));
    settings->setPrivateBrowsingEnabled(store.getBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey()));
    settings->setDeveloperExtrasEnabled(store.getBoolValueForKey(WebPreferencesKey::developerExtrasEnabledKey()));
    settings->setJavaScriptExperimentsEnabled(store.getBoolValueForKey(WebPreferencesKey::javaScriptExperimentsEnabledKey()));
    settings->setTextAreasAreResizable(store.getBoolValueForKey(WebPreferencesKey::textAreasAreResizableKey()));
    settings->setNeedsSiteSpecificQuirks(store.getBoolValueForKey(WebPreferencesKey::needsSiteSpecificQuirksKey()));
    settings->setJavaScriptCanOpenWindowsAutomatically(store.getBoolValueForKey(WebPreferencesKey::javaScriptCanOpenWindowsAutomaticallyKey()));
    settings->setForceFTPDirectoryListings(store.getBoolValueForKey(WebPreferencesKey::forceFTPDirectoryListingsKey()));
    settings->setDNSPrefetchingEnabled(store.getBoolValueForKey(WebPreferencesKey::dnsPrefetchingEnabledKey()));
#if ENABLE(WEB_ARCHIVE)
    settings->setWebArchiveDebugModeEnabled(store.getBoolValueForKey(WebPreferencesKey::webArchiveDebugModeEnabledKey()));
#endif
    settings->setLocalFileContentSniffingEnabled(store.getBoolValueForKey(WebPreferencesKey::localFileContentSniffingEnabledKey()));
    settings->setUsesPageCache(store.getBoolValueForKey(WebPreferencesKey::usesPageCacheKey()));
    settings->setPageCacheSupportsPlugins(store.getBoolValueForKey(WebPreferencesKey::pageCacheSupportsPluginsKey()));
    settings->setAuthorAndUserStylesEnabled(store.getBoolValueForKey(WebPreferencesKey::authorAndUserStylesEnabledKey()));
    settings->setPaginateDuringLayoutEnabled(store.getBoolValueForKey(WebPreferencesKey::paginateDuringLayoutEnabledKey()));
    settings->setDOMPasteAllowed(store.getBoolValueForKey(WebPreferencesKey::domPasteAllowedKey()));
    settings->setJavaScriptCanAccessClipboard(store.getBoolValueForKey(WebPreferencesKey::javaScriptCanAccessClipboardKey()));
    settings->setShouldPrintBackgrounds(store.getBoolValueForKey(WebPreferencesKey::shouldPrintBackgroundsKey()));
    settings->setWebSecurityEnabled(store.getBoolValueForKey(WebPreferencesKey::webSecurityEnabledKey()));
    settings->setAllowUniversalAccessFromFileURLs(store.getBoolValueForKey(WebPreferencesKey::allowUniversalAccessFromFileURLsKey()));
    settings->setAllowFileAccessFromFileURLs(store.getBoolValueForKey(WebPreferencesKey::allowFileAccessFromFileURLsKey()));

    settings->setMinimumFontSize(store.getUInt32ValueForKey(WebPreferencesKey::minimumFontSizeKey()));
    settings->setMinimumLogicalFontSize(store.getUInt32ValueForKey(WebPreferencesKey::minimumLogicalFontSizeKey()));
    settings->setDefaultFontSize(store.getUInt32ValueForKey(WebPreferencesKey::defaultFontSizeKey()));
    settings->setDefaultFixedFontSize(store.getUInt32ValueForKey(WebPreferencesKey::defaultFixedFontSizeKey()));
    settings->setLayoutFallbackWidth(store.getUInt32ValueForKey(WebPreferencesKey::layoutFallbackWidthKey()));
    settings->setDeviceWidth(store.getUInt32ValueForKey(WebPreferencesKey::deviceWidthKey()));
    settings->setDeviceHeight(store.getUInt32ValueForKey(WebPreferencesKey::deviceHeightKey()));
    settings->setEditableLinkBehavior(static_cast<WebCore::EditableLinkBehavior>(store.getUInt32ValueForKey(WebPreferencesKey::editableLinkBehaviorKey())));
    settings->setShowsToolTipOverTruncatedText(store.getBoolValueForKey(WebPreferencesKey::showsToolTipOverTruncatedTextKey()));

    settings->setAcceleratedCompositingEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedCompositingEnabledKey()) && LayerTreeHost::supportsAcceleratedCompositing());
    settings->setAcceleratedDrawingEnabled(store.getBoolValueForKey(WebPreferencesKey::acceleratedDrawingEnabledKey()) && LayerTreeHost::supportsAcceleratedCompositing());
    settings->setCanvasUsesAcceleratedDrawing(store.getBoolValueForKey(WebPreferencesKey::canvasUsesAcceleratedDrawingKey()) && LayerTreeHost::supportsAcceleratedCompositing());
    settings->setShowDebugBorders(store.getBoolValueForKey(WebPreferencesKey::compositingBordersVisibleKey()));
    settings->setShowRepaintCounter(store.getBoolValueForKey(WebPreferencesKey::compositingRepaintCountersVisibleKey()));
    settings->setCSSCustomFilterEnabled(store.getBoolValueForKey(WebPreferencesKey::cssCustomFilterEnabledKey()));
    settings->setCSSRegionsEnabled(store.getBoolValueForKey(WebPreferencesKey::cssRegionsEnabledKey()));
    settings->setCSSGridLayoutEnabled(store.getBoolValueForKey(WebPreferencesKey::cssGridLayoutEnabledKey()));
    settings->setRegionBasedColumnsEnabled(store.getBoolValueForKey(WebPreferencesKey::regionBasedColumnsEnabledKey()));
    settings->setWebGLEnabled(store.getBoolValueForKey(WebPreferencesKey::webGLEnabledKey()));
    settings->setMediaPlaybackRequiresUserGesture(store.getBoolValueForKey(WebPreferencesKey::mediaPlaybackRequiresUserGestureKey()));
    settings->setMediaPlaybackAllowsInline(store.getBoolValueForKey(WebPreferencesKey::mediaPlaybackAllowsInlineKey()));
    settings->setMockScrollbarsEnabled(store.getBoolValueForKey(WebPreferencesKey::mockScrollbarsEnabledKey()));
    settings->setHyperlinkAuditingEnabled(store.getBoolValueForKey(WebPreferencesKey::hyperlinkAuditingEnabledKey()));
    settings->setRequestAnimationFrameEnabled(store.getBoolValueForKey(WebPreferencesKey::requestAnimationFrameEnabledKey()));

    // <rdar://problem/10697417>: It is necessary to force compositing when accelerate drawing
    // is enabled on Mac so that scrollbars are always in their own layers.
#if PLATFORM(MAC)
    if (settings->acceleratedDrawingEnabled())
        settings->setForceCompositingMode(LayerTreeHost::supportsAcceleratedCompositing());
    else
#endif
        settings->setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()) && LayerTreeHost::supportsAcceleratedCompositing());

#if ENABLE(TIZEN_RUNTIME_BACKEND_SELECTION)
    if (!settings->acceleratedCompositingEnabled()) {
        settings->setAcceleratedCompositingFor3DTransformsEnabled(false);
        settings->setAcceleratedCompositingForVideoEnabled(false);
        settings->setAcceleratedCompositingForPluginsEnabled(false);
        settings->setAcceleratedCompositingForCanvasEnabled(false);
        settings->setAcceleratedCompositingForAnimationEnabled(false);
    }
#endif
#if ENABLE(TIZEN_WEBKIT2_MEMORY_SAVING_MODE)
    settings->setMemorySavingMode(WebProcess::shared().memorySavingModeEnabled());
#endif

#if ENABLE(SQL_DATABASE)
    AbstractDatabase::setIsAvailable(store.getBoolValueForKey(WebPreferencesKey::databasesEnabledKey()));
#endif

#if ENABLE(FULLSCREEN_API)
    settings->setFullScreenEnabled(store.getBoolValueForKey(WebPreferencesKey::fullScreenEnabledKey()));
#endif

#if ENABLE(TIZEN_WEBKIT2_ASYNCHRONOUS_SPELLCHECKING)
    settings->setAsynchronousSpellCheckingEnabled(store.getBoolValueForKey(WebPreferencesKey::asynchronousSpellCheckingEnabledKey()));
#endif
    settings->setLocalStorageDatabasePath(WebProcess::shared().localStorageDirectory());

#if USE(AVFOUNDATION)
    settings->setAVFoundationEnabled(store.getBoolValueForKey(WebPreferencesKey::isAVFoundationEnabledKey()));
#endif

#if ENABLE(WEB_AUDIO)
    settings->setWebAudioEnabled(store.getBoolValueForKey(WebPreferencesKey::webAudioEnabledKey()));
#endif

    settings->setApplicationChromeMode(store.getBoolValueForKey(WebPreferencesKey::applicationChromeModeKey()));
    settings->setSuppressesIncrementalRendering(store.getBoolValueForKey(WebPreferencesKey::suppressesIncrementalRenderingKey()));
    settings->setBackspaceKeyNavigationEnabled(store.getBoolValueForKey(WebPreferencesKey::backspaceKeyNavigationEnabledKey()));
    settings->setWantsBalancedSetDefersLoadingBehavior(store.getBoolValueForKey(WebPreferencesKey::wantsBalancedSetDefersLoadingBehaviorKey()));
    settings->setCaretBrowsingEnabled(store.getBoolValueForKey(WebPreferencesKey::caretBrowsingEnabledKey()));

#if ENABLE(VIDEO_TRACK)
    settings->setShouldDisplaySubtitles(store.getBoolValueForKey(WebPreferencesKey::shouldDisplaySubtitlesKey()));
    settings->setShouldDisplayCaptions(store.getBoolValueForKey(WebPreferencesKey::shouldDisplayCaptionsKey()));
    settings->setShouldDisplayTextDescriptions(store.getBoolValueForKey(WebPreferencesKey::shouldDisplayTextDescriptionsKey()));
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    settings->setNotificationsEnabled(store.getBoolValueForKey(WebPreferencesKey::notificationsEnabledKey()));
#endif

#if ENABLE(TIZEN_HONOUR_IMAGE_ORIENTATION)
    settings->setShouldRespectImageOrientation(true);
#else
    settings->setShouldRespectImageOrientation(store.getBoolValueForKey(WebPreferencesKey::shouldRespectImageOrientationKey()));
#endif
    settings->setDiagnosticLoggingEnabled(store.getBoolValueForKey(WebPreferencesKey::diagnosticLoggingEnabledKey()));

#if ENABLE(TEXT_AUTOSIZING)
    settings->setTextAutosizingEnabled(store.getBoolValueForKey(WebPreferencesKey::textAutosizingEnabledKey()));
#if ENABLE(TIZEN_TEXT_AUTOSIZING)
    settings->setTextAutosizingFontScaleFactor(store.getDoubleValueForKey(WebPreferencesKey::textAutosizingFontScaleFactorKey()));
#endif
#endif

#if ENABLE(TIZEN_WEBKIT2_GET_TEXT_STYLE_FOR_SELECTION)
    settings->setTextStyleStateEnabled(store.getBoolValueForKey(WebPreferencesKey::textStyleStateEnabledKey()));
#endif

#if ENABLE(TIZEN_PROD_DETECT_CONTENTS)
    settings->setDetectContentsAutomatically(store.getBoolValueForKey(WebPreferencesKey::detectContentsAutomaticallyKey()));
#endif
#if ENABLE(TIZEN_FONT_SIZE_CSS_STYLING)
    settings->setLegacyFontSizeEnabled(store.getBoolValueForKey(WebPreferencesKey::legacyFontSizeEnabledKey()));
#endif
#if ENABLE(TIZEN_PASTE_IMAGE_URI)
    settings->setPasteImageUriEnabled(store.getBoolValueForKey(WebPreferencesKey::pasteImageUriEnabledKey()));;
#endif
#if ENABLE(TIZEN_DISABLE_WEBKIT_TEXT_SIZE_ADJUST)
    settings->setWebkitTextSizeAdjustEnabled(store.getBoolValueForKey(WebPreferencesKey::webkitTextSizeAdjustEnabledKey()));
#endif

    platformPreferencesDidChange(store);

    if (m_drawingArea)
        m_drawingArea->updatePreferences();
}

#if ENABLE(INSPECTOR)
WebInspector* WebPage::inspector()
{
    if (m_isClosed)
        return 0;
    if (!m_inspector)
        m_inspector = WebInspector::create(this, m_inspectorClient);
    return m_inspector.get();
}
#endif

#if ENABLE(FULLSCREEN_API)
WebFullScreenManager* WebPage::fullScreenManager()
{
    if (!m_fullScreenManager)
        m_fullScreenManager = WebFullScreenManager::create(this);
    return m_fullScreenManager.get();
}
#endif

NotificationPermissionRequestManager* WebPage::notificationPermissionRequestManager()
{
    if (m_notificationPermissionRequestManager)
        return m_notificationPermissionRequestManager.get();

    m_notificationPermissionRequestManager = NotificationPermissionRequestManager::create(this);
    return m_notificationPermissionRequestManager.get();
}

#if ENABLE(TIZEN_MEDIA_STREAM)
UserMediaPermissionRequestManager* WebPage::userMediaPermissionRequestManager()
{
    if (m_userMediaPermissionRequestManager)
        return m_userMediaPermissionRequestManager.get();

    m_userMediaPermissionRequestManager = UserMediaPermissionRequestManager::create(this);
    return m_userMediaPermissionRequestManager.get();
}
#endif

#if !PLATFORM(GTK) && !PLATFORM(MAC)
bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return false;

    Editor::Command command = frame->editor()->command(interpretKeyEvent(evt));

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(evt);
    }

    if (command.execute(evt))
        return true;

    // Don't allow text insertion for nodes that cannot edit.
    if (!frame->editor()->canEdit())
        return false;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (evt->charCode() < ' ')
        return false;

    return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}
#endif

#if ENABLE(DRAG_SUPPORT)

#if PLATFORM(WIN)
void WebPage::performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const WebCore::DragDataMap& dataMap, uint32_t flags)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(WebCore::DragSession()));
        return;
    }

    DragData dragData(dataMap, clientPosition, globalPosition, static_cast<DragOperation>(draggingSourceOperationMask), static_cast<DragApplicationFlags>(flags));
    switch (action) {
    case DragControllerActionEntered:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragEntered(&dragData)));
        break;

    case DragControllerActionUpdated:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragUpdated(&dragData)));
        break;
        
    case DragControllerActionExited:
        m_page->dragController()->dragExited(&dragData);
        break;
        
    case DragControllerActionPerformDrag:
        m_page->dragController()->performDrag(&dragData);
        break;
        
    default:
        ASSERT_NOT_REACHED();
    }
}

#elif PLATFORM(QT) || PLATFORM(GTK) || ENABLE(TIZEN_DRAG_SUPPORT)
void WebPage::performDragControllerAction(uint64_t action, WebCore::DragData dragData)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(WebCore::DragSession()));
#if PLATFORM(QT)
        QMimeData* data = const_cast<QMimeData*>(dragData.platformData());
        delete data;
#elif PLATFORM(GTK)
        DataObjectGtk* data = const_cast<DataObjectGtk*>(dragData.platformData());
        data->deref();
#endif
#if ENABLE(TIZEN_DRAG_SUPPORT)
        DataObjectTizen* data = const_cast<DataObjectTizen*>(dragData.platformData());
        data->deref();
#endif
        return;
    }

    switch (action) {
    case DragControllerActionEntered:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragEntered(&dragData)));
        break;

    case DragControllerActionUpdated:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragUpdated(&dragData)));
        break;

    case DragControllerActionExited:
        m_page->dragController()->dragExited(&dragData);
        break;

    case DragControllerActionPerformDrag: {
        m_page->dragController()->performDrag(&dragData);
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
    // DragData does not delete its platformData so we need to do that here.
#if PLATFORM(QT)
    QMimeData* data = const_cast<QMimeData*>(dragData.platformData());
    delete data;
#elif PLATFORM(GTK)
    DataObjectGtk* data = const_cast<DataObjectGtk*>(dragData.platformData());
    data->deref();
#endif
#if ENABLE(TIZEN_DRAG_SUPPORT)
    DataObjectTizen* data = const_cast<DataObjectTizen*>(dragData.platformData());
    data->deref();
#endif
}

#else
void WebPage::performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const String& dragStorageName, uint32_t flags, const SandboxExtension::Handle& sandboxExtensionHandle, const SandboxExtension::HandleArray& sandboxExtensionsHandleArray)
{
    if (!m_page) {
        send(Messages::WebPageProxy::DidPerformDragControllerAction(WebCore::DragSession()));
        return;
    }

    DragData dragData(dragStorageName, clientPosition, globalPosition, static_cast<DragOperation>(draggingSourceOperationMask), static_cast<DragApplicationFlags>(flags));
    switch (action) {
    case DragControllerActionEntered:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragEntered(&dragData)));
        break;

    case DragControllerActionUpdated:
        send(Messages::WebPageProxy::DidPerformDragControllerAction(m_page->dragController()->dragUpdated(&dragData)));
        break;
        
    case DragControllerActionExited:
        m_page->dragController()->dragExited(&dragData);
        break;
        
    case DragControllerActionPerformDrag: {
        ASSERT(!m_pendingDropSandboxExtension);

        m_pendingDropSandboxExtension = SandboxExtension::create(sandboxExtensionHandle);
        for (size_t i = 0; i < sandboxExtensionsHandleArray.size(); i++) {
            if (RefPtr<SandboxExtension> extension = SandboxExtension::create(sandboxExtensionsHandleArray[i]))
                m_pendingDropExtensionsForFileUpload.append(extension);
        }

        m_page->dragController()->performDrag(&dragData);

        // If we started loading a local file, the sandbox extension tracker would have adopted this
        // pending drop sandbox extension. If not, we'll play it safe and invalidate it.
        if (m_pendingDropSandboxExtension) {
            m_pendingDropSandboxExtension->invalidate();
            m_pendingDropSandboxExtension = nullptr;
        }
        for (size_t i = 0; i < m_pendingDropExtensionsForFileUpload.size(); i++)
            m_pendingDropExtensionsForFileUpload[i]->invalidate();

        m_pendingDropExtensionsForFileUpload.clear();
        break;
    }

    default:
        ASSERT_NOT_REACHED();
    }
}
#endif

void WebPage::dragEnded(WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t operation)
{
    IntPoint adjustedClientPosition(clientPosition.x() + m_page->dragController()->dragOffset().x(), clientPosition.y() + m_page->dragController()->dragOffset().y());
    IntPoint adjustedGlobalPosition(globalPosition.x() + m_page->dragController()->dragOffset().x(), globalPosition.y() + m_page->dragController()->dragOffset().y());

    m_page->dragController()->dragEnded();
    FrameView* view = m_page->mainFrame()->view();
    if (!view)
        return;
    // FIXME: These are fake modifier keys here, but they should be real ones instead.
    PlatformMouseEvent event(adjustedClientPosition, adjustedGlobalPosition, LeftButton, PlatformEvent::MouseMoved, 0, false, false, false, false, currentTime());
    m_page->mainFrame()->eventHandler()->dragSourceEndedAt(event, (DragOperation)operation);
}

void WebPage::willPerformLoadDragDestinationAction()
{
    m_sandboxExtensionTracker.willPerformLoadDragDestinationAction(m_pendingDropSandboxExtension.release());
}

void WebPage::mayPerformUploadDragDestinationAction()
{
    for (size_t i = 0; i < m_pendingDropExtensionsForFileUpload.size(); i++)
        m_pendingDropExtensionsForFileUpload[i]->consumePermanently();
    m_pendingDropExtensionsForFileUpload.clear();
}
    
#endif // ENABLE(DRAG_SUPPORT)

WebUndoStep* WebPage::webUndoStep(uint64_t stepID)
{
    return m_undoStepMap.get(stepID).get();
}

void WebPage::addWebUndoStep(uint64_t stepID, WebUndoStep* entry)
{
    m_undoStepMap.set(stepID, entry);
}

void WebPage::removeWebEditCommand(uint64_t stepID)
{
    m_undoStepMap.remove(stepID);
}

void WebPage::unapplyEditCommand(uint64_t stepID)
{
    WebUndoStep* step = webUndoStep(stepID);
    if (!step)
        return;

    step->step()->unapply();
}

void WebPage::reapplyEditCommand(uint64_t stepID)
{
    WebUndoStep* step = webUndoStep(stepID);
    if (!step)
        return;

    m_isInRedo = true;
    step->step()->reapply();
    m_isInRedo = false;
}

void WebPage::didRemoveEditCommand(uint64_t commandID)
{
    removeWebEditCommand(commandID);
}

void WebPage::setActivePopupMenu(WebPopupMenu* menu)
{
#if ENABLE(TIZEN_WEBKIT2_POPUP_INTERNAL)
    if (!(m_activePopupMenu.get()) && !menu)
        return;
#endif
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI("m_activePopupMenu:[%p] = menu:[%p]", m_activePopupMenu.get(), menu);
#endif

    m_activePopupMenu = menu;
}

#if ENABLE(INPUT_TYPE_COLOR)
void WebPage::setActiveColorChooser(WebColorChooser* colorChooser)
{
    m_activeColorChooser = colorChooser;
}

void WebPage::didEndColorChooser()
{
    m_activeColorChooser->didEndChooser();
}

void WebPage::didChooseColor(const WebCore::Color& color)
{
    m_activeColorChooser->didChooseColor(color);
}
#endif

void WebPage::setActiveOpenPanelResultListener(PassRefPtr<WebOpenPanelResultListener> openPanelResultListener)
{
    m_activeOpenPanelResultListener = openPanelResultListener;
}

bool WebPage::findStringFromInjectedBundle(const String& target, FindOptions options)
{
    return m_page->findString(target, options);
}

void WebPage::findString(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.findString(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::findStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.findStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::getImageForFindMatch(uint32_t matchIndex)
{
    m_findController.getImageForFindMatch(matchIndex);
}

void WebPage::selectFindMatch(uint32_t matchIndex)
{
    m_findController.selectFindMatch(matchIndex);
}

void WebPage::hideFindUI()
{
    m_findController.hideFindUI();
}

void WebPage::countStringMatches(const String& string, uint32_t options, uint32_t maxMatchCount)
{
    m_findController.countStringMatches(string, static_cast<FindOptions>(options), maxMatchCount);
}

void WebPage::didChangeSelectedIndexForActivePopupMenu(int32_t newIndex)
{
    changeSelectedIndex(newIndex);

    // Since applictions on tizen may provide a way to select item more than one time,
    // we need to keep the active popup menu to allow repetitive selection.
#if !PLATFORM(TIZEN)
    m_activePopupMenu = 0;
#endif
}

void WebPage::changeSelectedIndex(int32_t index)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->didChangeSelectedIndex(index);
}

void WebPage::didChooseFilesForOpenPanel(const Vector<String>& files)
{
    if (!m_activeOpenPanelResultListener)
        return;

    m_activeOpenPanelResultListener->didChooseFiles(files);
    m_activeOpenPanelResultListener = 0;
}

void WebPage::didCancelForOpenPanel()
{
    m_activeOpenPanelResultListener = 0;
}

#if PLATFORM(TIZEN)
void WebPage::cancelForOpenPanel()
{
    if (m_activeOpenPanelResultListener) {
        m_activeOpenPanelResultListener->disconnectFromPage();
        m_activeOpenPanelResultListener = 0;
    }
}
#endif

#if ENABLE(WEB_PROCESS_SANDBOX)
void WebPage::extendSandboxForFileFromOpenPanel(const SandboxExtension::Handle& handle)
{
    SandboxExtension::create(handle)->consumePermanently();
}
#endif

#if ENABLE(GEOLOCATION)
void WebPage::didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed)
{
    m_geolocationPermissionRequestManager.didReceiveGeolocationPermissionDecision(geolocationID, allowed);
}
#endif

void WebPage::didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed)
{
    notificationPermissionRequestManager()->didReceiveNotificationPermissionDecision(notificationID, allowed);
}

#if ENABLE(TIZEN_MEDIA_STREAM)
void WebPage::didReceiveUserMediaPermissionDecision(uint64_t userMediaID, bool allowed)
{
    userMediaPermissionRequestManager()->didReceiveUserMediaPermissionDecision(userMediaID, allowed);
}
#endif

void WebPage::advanceToNextMisspelling(bool startBeforeSelection)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    frame->editor()->advanceToNextMisspelling(startBeforeSelection);
}

void WebPage::changeSpellingToWord(const String& word)
{
    replaceSelectionWithText(m_page->focusController()->focusedOrMainFrame(), word);
}

void WebPage::unmarkAllMisspellings()
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* document = frame->document())
            document->markers()->removeMarkers(DocumentMarker::Spelling);
    }
}

void WebPage::unmarkAllBadGrammar()
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* document = frame->document())
            document->markers()->removeMarkers(DocumentMarker::Grammar);
    }
}

#if USE(APPKIT)
void WebPage::uppercaseWord()
{
    m_page->focusController()->focusedOrMainFrame()->editor()->uppercaseWord();
}

void WebPage::lowercaseWord()
{
    m_page->focusController()->focusedOrMainFrame()->editor()->lowercaseWord();
}

void WebPage::capitalizeWord()
{
    m_page->focusController()->focusedOrMainFrame()->editor()->capitalizeWord();
}
#endif
    
void WebPage::setTextForActivePopupMenu(int32_t index)
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->setTextForIndex(index);
}

#if PLATFORM(GTK)
void WebPage::failedToShowPopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->client()->popupDidHide();
}
#endif

#if ENABLE(CONTEXT_MENUS)
void WebPage::didSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    if (!m_contextMenu)
        return;

    m_contextMenu->itemSelected(item);
    m_contextMenu = 0;
}
#endif

void WebPage::replaceSelectionWithText(Frame* frame, const String& text)
{
    bool selectReplacement = true;
    bool smartReplace = false;
    return frame->editor()->replaceSelectionWithText(text, selectReplacement, smartReplace);
}

void WebPage::clearSelection()
{
    m_page->focusController()->focusedOrMainFrame()->selection()->clear();
}

bool WebPage::mainFrameHasCustomRepresentation() const
{
    if (Frame* frame = mainFrame())
        return static_cast<WebFrameLoaderClient*>(frame->loader()->client())->frameHasCustomRepresentation();

    return false;
}

void WebPage::didChangeScrollOffsetForMainFrame()
{
    Frame* frame = m_page->mainFrame();
    IntPoint scrollPosition = frame->view()->scrollPosition();
    IntPoint maximumScrollPosition = frame->view()->maximumScrollPosition();
    IntPoint minimumScrollPosition = frame->view()->minimumScrollPosition();

#if PLATFORM(EFL) && PLATFORM(TIZEN)
    send(Messages::WebPageProxy::DidChangeScrollPositionForMainFrame(scrollPosition));
#endif

    bool isPinnedToLeftSide = (scrollPosition.x() <= minimumScrollPosition.x());
    bool isPinnedToRightSide = (scrollPosition.x() >= maximumScrollPosition.x());

    if (isPinnedToLeftSide != m_cachedMainFrameIsPinnedToLeftSide || isPinnedToRightSide != m_cachedMainFrameIsPinnedToRightSide) {
        send(Messages::WebPageProxy::DidChangeScrollOffsetPinningForMainFrame(isPinnedToLeftSide, isPinnedToRightSide));
        
        m_cachedMainFrameIsPinnedToLeftSide = isPinnedToLeftSide;
        m_cachedMainFrameIsPinnedToRightSide = isPinnedToRightSide;
    }
}

void WebPage::mainFrameDidLayout()
{
    unsigned pageCount = m_page->pageCount();
    if (pageCount != m_cachedPageCount) {
        send(Messages::WebPageProxy::DidChangePageCount(pageCount));
        m_cachedPageCount = pageCount;
    }

#if USE(TILED_BACKING_STORE) && USE(ACCELERATED_COMPOSITING)
    if (m_drawingArea && m_drawingArea->layerTreeHost()) {
        double red, green, blue, alpha;
        m_mainFrame->getDocumentBackgroundColor(&red, &green, &blue, &alpha);
        RGBA32 rgba = makeRGBA32FromFloats(red, green, blue, alpha);
        if (m_backgroundColor.rgb() != rgba) {
            m_backgroundColor.setRGB(rgba);
            m_drawingArea->layerTreeHost()->setBackgroundColor(m_backgroundColor);
        }
    }
#endif
}

void WebPage::addPluginView(PluginView* pluginView)
{
    ASSERT(!m_pluginViews.contains(pluginView));

    m_pluginViews.add(pluginView);
}

void WebPage::removePluginView(PluginView* pluginView)
{
    ASSERT(m_pluginViews.contains(pluginView));

    m_pluginViews.remove(pluginView);
}

#if PLATFORM(MAC)
void WebPage::setWindowIsVisible(bool windowIsVisible)
{
    m_windowIsVisible = windowIsVisible;

    corePage()->focusController()->setContainingWindowIsVisible(windowIsVisible);

    // Tell all our plug-in views that the window visibility changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->setWindowIsVisible(windowIsVisible);
}

void WebPage::windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates, const WebCore::IntPoint& accessibilityViewCoordinates)
{
    m_windowFrameInScreenCoordinates = windowFrameInScreenCoordinates;
    m_viewFrameInWindowCoordinates = viewFrameInWindowCoordinates;
    m_accessibilityPosition = accessibilityViewCoordinates;
    
    // Tell all our plug-in views that the window and view frames have changed.
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it)
        (*it)->windowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates);
}
#endif

bool WebPage::windowIsFocused() const
{
    return m_page->focusController()->isActive();
}

bool WebPage::windowAndWebPageAreFocused() const
{
#if PLATFORM(MAC)
    if (!m_windowIsVisible)
        return false;
#endif
    return m_page->focusController()->isFocused() && m_page->focusController()->isActive();
}

void WebPage::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingArea>()) {
        if (m_drawingArea)
            m_drawingArea->didReceiveDrawingAreaMessage(connection, messageID, arguments);
        return;
    }

#if USE(TILED_BACKING_STORE) && USE(ACCELERATED_COMPOSITING)
    if (messageID.is<CoreIPC::MessageClassLayerTreeCoordinator>()) {
        if (m_drawingArea)
            m_drawingArea->didReceiveLayerTreeCoordinatorMessage(connection, messageID, arguments);
        return;
    }
#endif
    
#if ENABLE(INSPECTOR)
    if (messageID.is<CoreIPC::MessageClassWebInspector>()) {
        if (WebInspector* inspector = this->inspector())
            inspector->didReceiveWebInspectorMessage(connection, messageID, arguments);
        return;
    }
#endif

#if ENABLE(FULLSCREEN_API)
    if (messageID.is<CoreIPC::MessageClassWebFullScreenManager>()) {
        fullScreenManager()->didReceiveMessage(connection, messageID, arguments);
        return;
    }
#endif

    didReceiveWebPageMessage(connection, messageID, arguments);
}

void WebPage::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{   
    didReceiveSyncWebPageMessage(connection, messageID, arguments, reply);
}
    
InjectedBundleBackForwardList* WebPage::backForwardList()
{
    if (!m_backForwardList)
        m_backForwardList = InjectedBundleBackForwardList::create(this);
    return m_backForwardList.get();
}

#if PLATFORM(QT) || PLATFORM(TIZEN)
#if ENABLE(TOUCH_ADJUSTMENT)
void WebPage::findZoomableAreaForPoint(const WebCore::IntPoint& point, const WebCore::IntSize& area)
{
    Node* node = 0;
    IntRect zoomableArea;
    bool foundAreaForTouchPoint = m_mainFrame->coreFrame()->eventHandler()->bestZoomableAreaForTouchPoint(point, IntSize(area.width() / 2, area.height() / 2), zoomableArea, node);
    ASSERT(node);

    if (!foundAreaForTouchPoint)
        return;

    send(Messages::WebPageProxy::DidFindZoomableArea(point, zoomableArea));
}

#else
void WebPage::findZoomableAreaForPoint(const WebCore::IntPoint& point, const WebCore::IntSize& area)
{
    UNUSED_PARAM(area);
    Frame* mainframe = m_mainFrame->coreFrame();
    HitTestResult result = mainframe->eventHandler()->hitTestResultAtPoint(mainframe->view()->windowToContents(point), /*allowShadowContent*/ false, /*ignoreClipping*/ true);

    Node* node = result.innerNode();

    if (!node)
        return;

#if PLATFORM(TIZEN) // FIXME: This is to fix wrong codes of open source.
    IntRect zoomableArea = enclosingIntRect(node->getRect());
#else
    IntRect zoomableArea = node->getRect();
#endif

    while (true) {
        bool found = !node->isTextNode() && !node->isShadowRoot();

        // No candidate found, bail out.
        if (!found && !node->parentNode())
            return;

        // Candidate found, and it is a better candidate than its parent.
        // NB: A parent is considered a better candidate iff the node is
        // contained by it and it is the only child.
        if (found && (!node->parentNode() || node->parentNode()->childNodeCount() != 1))
            break;

        node = node->parentNode();
#if PLATFORM(TIZEN) // FIXME: This is to fix wrong codes of open source.
        zoomableArea.unite(enclosingIntRect(node->getRect()));
#else
        zoomableArea.unite(node->getRect());
#endif
    }

    if (node->document() && node->document()->frame() && node->document()->frame()->view()) {
        const ScrollView* view = node->document()->frame()->view();
        zoomableArea = view->contentsToWindow(zoomableArea);
    }

    send(Messages::WebPageProxy::DidFindZoomableArea(point, zoomableArea));
}
#endif
#endif

WebPage::SandboxExtensionTracker::~SandboxExtensionTracker()
{
    invalidate();
}

void WebPage::SandboxExtensionTracker::invalidate()
{
    if (m_pendingProvisionalSandboxExtension) {
        m_pendingProvisionalSandboxExtension->invalidate();
        m_pendingProvisionalSandboxExtension = 0;
    }

    if (m_provisionalSandboxExtension) {
        m_provisionalSandboxExtension->invalidate();
        m_provisionalSandboxExtension = 0;
    }

    if (m_committedSandboxExtension) {
        m_committedSandboxExtension->invalidate();
        m_committedSandboxExtension = 0;
    }
}

void WebPage::SandboxExtensionTracker::willPerformLoadDragDestinationAction(PassRefPtr<SandboxExtension> pendingDropSandboxExtension)
{
    setPendingProvisionalSandboxExtension(pendingDropSandboxExtension);
}

void WebPage::SandboxExtensionTracker::beginLoad(WebFrame* frame, const SandboxExtension::Handle& handle)
{
    ASSERT(frame->isMainFrame());

    setPendingProvisionalSandboxExtension(SandboxExtension::create(handle));
}

void WebPage::SandboxExtensionTracker::setPendingProvisionalSandboxExtension(PassRefPtr<SandboxExtension> pendingProvisionalSandboxExtension)
{
    // If we get two beginLoad calls in succession, without a provisional load starting, then
    // m_pendingProvisionalSandboxExtension will be non-null. Invalidate and null out the extension if that is the case.
    if (m_pendingProvisionalSandboxExtension) {
        m_pendingProvisionalSandboxExtension->invalidate();
        m_pendingProvisionalSandboxExtension = nullptr;
    }
    
    m_pendingProvisionalSandboxExtension = pendingProvisionalSandboxExtension;    
}

static bool shouldReuseCommittedSandboxExtension(WebFrame* frame)
{
    ASSERT(frame->isMainFrame());

    FrameLoader* frameLoader = frame->coreFrame()->loader();
    FrameLoadType frameLoadType = frameLoader->loadType();

    // If the page is being reloaded, it should reuse whatever extension is committed.
    if (frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeReloadFromOrigin)
        return true;

    DocumentLoader* documentLoader = frameLoader->documentLoader();
    DocumentLoader* provisionalDocumentLoader = frameLoader->provisionalDocumentLoader();
    if (!documentLoader || !provisionalDocumentLoader)
        return false;

    if (documentLoader->url().isLocalFile() && provisionalDocumentLoader->url().isLocalFile())
        return true;

    return false;
}

void WebPage::SandboxExtensionTracker::didStartProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // We should only reuse the commited sandbox extension if it is not null. It can be
    // null if the last load was for an error page.
    if (m_committedSandboxExtension && shouldReuseCommittedSandboxExtension(frame)) {
        m_pendingProvisionalSandboxExtension = m_committedSandboxExtension.release();
        ASSERT(!m_committedSandboxExtension);
    }

    ASSERT(!m_provisionalSandboxExtension);

    m_provisionalSandboxExtension = m_pendingProvisionalSandboxExtension.release();
    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->consume();
}

void WebPage::SandboxExtensionTracker::didCommitProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // Generally, there should be no pending extension at this stage, but we can have one if UI process
    // has an out of date idea of WebProcess state, and initiates a load or reload without stopping an existing one.
    if (m_pendingProvisionalSandboxExtension) {
        m_pendingProvisionalSandboxExtension->invalidate();
        m_pendingProvisionalSandboxExtension = nullptr;
    }

    // The provisional load has been committed. Invalidate the currently committed sandbox
    // extension and make the provisional sandbox extension the committed sandbox extension.
    if (m_committedSandboxExtension)
        m_committedSandboxExtension->invalidate();

    m_committedSandboxExtension = m_provisionalSandboxExtension.release();
}

void WebPage::SandboxExtensionTracker::didFailProvisionalLoad(WebFrame* frame)
{
    if (!frame->isMainFrame())
        return;

    // Generally, there should be no pending extension at this stage, but we can have one if UI process
    // has an out of date idea of WebProcess state, and initiates a load or reload without stopping an existing one.
    if (m_pendingProvisionalSandboxExtension) {
        m_pendingProvisionalSandboxExtension->invalidate();
        m_pendingProvisionalSandboxExtension = nullptr;
    }

    if (!m_provisionalSandboxExtension)
        return;

    m_provisionalSandboxExtension->invalidate();
    m_provisionalSandboxExtension = nullptr;
}

bool WebPage::hasLocalDataForURL(const KURL& url)
{
    if (url.isLocalFile())
        return true;

    FrameLoader* frameLoader = m_page->mainFrame()->loader();
    DocumentLoader* documentLoader = frameLoader ? frameLoader->documentLoader() : 0;
    if (documentLoader && documentLoader->subresource(url))
        return true;

    return platformHasLocalDataForURL(url);
}

void WebPage::setCustomTextEncodingName(const String& encoding)
{
    m_page->mainFrame()->loader()->reloadWithOverrideEncoding(encoding);
}

void WebPage::didRemoveBackForwardItem(uint64_t itemID)
{
    WebBackForwardListProxy::removeItem(itemID);
}

#if PLATFORM(MAC)

bool WebPage::isSpeaking()
{
    bool result;
    return sendSync(Messages::WebPageProxy::GetIsSpeaking(), Messages::WebPageProxy::GetIsSpeaking::Reply(result)) && result;
}

void WebPage::speak(const String& string)
{
    send(Messages::WebPageProxy::Speak(string));
}

void WebPage::stopSpeaking()
{
    send(Messages::WebPageProxy::StopSpeaking());
}

#endif

#if PLATFORM(MAC)
RetainPtr<PDFDocument> WebPage::pdfDocumentForPrintingFrame(Frame* coreFrame)
{
    Document* document = coreFrame->document();
    if (!document)
        return 0;

    if (!document->isPluginDocument())
        return 0;

    PluginView* pluginView = static_cast<PluginView*>(toPluginDocument(document)->pluginWidget());
    if (!pluginView)
        return 0;

    return pluginView->pdfDocumentForPrinting();
}
#endif // PLATFORM(MAC)

void WebPage::beginPrinting(uint64_t frameID, const PrintInfo& printInfo)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    if (!frame)
        return;

    Frame* coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;

#if PLATFORM(MAC)
    if (pdfDocumentForPrintingFrame(coreFrame))
        return;
#endif // PLATFORM(MAC)

    if (!m_printContext)
        m_printContext = adoptPtr(new PrintContext(coreFrame));

    drawingArea()->setLayerTreeStateIsFrozen(true);
    m_printContext->begin(printInfo.availablePaperWidth, printInfo.availablePaperHeight);

    float fullPageHeight;
    m_printContext->computePageRects(FloatRect(0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight), 0, 0, printInfo.pageSetupScaleFactor, fullPageHeight, true);

#if PLATFORM(GTK)
    if (!m_printOperation)
        m_printOperation = WebPrintOperationGtk::create(this, printInfo);
#endif
}

void WebPage::endPrinting()
{
    drawingArea()->setLayerTreeStateIsFrozen(false);
#if PLATFORM(GTK)
    m_printOperation = 0;
#endif
    m_printContext = nullptr;
}

void WebPage::computePagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, uint64_t callbackID)
{
    Vector<IntRect> resultPageRects;
    double resultTotalScaleFactorForPrinting = 1;

    beginPrinting(frameID, printInfo);

    if (m_printContext) {
        resultPageRects = m_printContext->pageRects();
        resultTotalScaleFactorForPrinting = m_printContext->computeAutomaticScaleFactor(FloatSize(printInfo.availablePaperWidth, printInfo.availablePaperHeight)) * printInfo.pageSetupScaleFactor;
    }
#if PLATFORM(MAC)
    else
        computePagesForPrintingPDFDocument(frameID, printInfo, resultPageRects);
#endif // PLATFORM(MAC)

    // If we're asked to print, we should actually print at least a blank page.
    if (resultPageRects.isEmpty())
        resultPageRects.append(IntRect(0, 0, 1, 1));

    send(Messages::WebPageProxy::ComputedPagesCallback(resultPageRects, resultTotalScaleFactorForPrinting, callbackID));
}

#if PLATFORM(MAC) || PLATFORM(WIN)
void WebPage::drawRectToPDF(uint64_t frameID, const PrintInfo& printInfo, const WebCore::IntRect& rect, uint64_t callbackID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;

    RetainPtr<CFMutableDataRef> pdfPageData(AdoptCF, CFDataCreateMutable(0, 0));

#if USE(CG)
    if (coreFrame) {
#if PLATFORM(MAC)
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame));
#else
        ASSERT(coreFrame->document()->printing());
#endif

        // FIXME: Use CGDataConsumerCreate with callbacks to avoid copying the data.
        RetainPtr<CGDataConsumerRef> pdfDataConsumer(AdoptCF, CGDataConsumerCreateWithCFData(pdfPageData.get()));

        CGRect mediaBox = CGRectMake(0, 0, rect.width(), rect.height());
        RetainPtr<CGContextRef> context(AdoptCF, CGPDFContextCreate(pdfDataConsumer.get(), &mediaBox, 0));
        RetainPtr<CFDictionaryRef> pageInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CGPDFContextBeginPage(context.get(), pageInfo.get());

#if PLATFORM(MAC)
        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame)) {
            ASSERT(!m_printContext);
            drawRectToPDFFromPDFDocument(context.get(), pdfDocument.get(), printInfo, rect);
        } else
#endif
        {
            GraphicsContext ctx(context.get());
            ctx.scale(FloatSize(1, -1));
            ctx.translate(0, -rect.height());
            m_printContext->spoolRect(ctx, rect);
        }

        CGPDFContextEndPage(context.get());
        CGPDFContextClose(context.get());
    }
#endif

    send(Messages::WebPageProxy::DataCallback(CoreIPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get())), callbackID));
}

void WebPage::drawPagesToPDF(uint64_t frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, uint64_t callbackID)
{
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;

    RetainPtr<CFMutableDataRef> pdfPageData(AdoptCF, CFDataCreateMutable(0, 0));

#if USE(CG)
    if (coreFrame) {

#if PLATFORM(MAC)
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame));
#else
        ASSERT(coreFrame->document()->printing());
#endif

        // FIXME: Use CGDataConsumerCreate with callbacks to avoid copying the data.
        RetainPtr<CGDataConsumerRef> pdfDataConsumer(AdoptCF, CGDataConsumerCreateWithCFData(pdfPageData.get()));

        CGRect mediaBox = (m_printContext && m_printContext->pageCount()) ? m_printContext->pageRect(0) : CGRectMake(0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight);
        RetainPtr<CGContextRef> context(AdoptCF, CGPDFContextCreate(pdfDataConsumer.get(), &mediaBox, 0));

#if PLATFORM(MAC)
        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame)) {
            ASSERT(!m_printContext);
            drawPagesToPDFFromPDFDocument(context.get(), pdfDocument.get(), printInfo, first, count);
        } else
#endif
        {
            size_t pageCount = m_printContext->pageCount();
            for (uint32_t page = first; page < first + count; ++page) {
                if (page >= pageCount)
                    break;

                RetainPtr<CFDictionaryRef> pageInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
                CGPDFContextBeginPage(context.get(), pageInfo.get());

                GraphicsContext ctx(context.get());
                ctx.scale(FloatSize(1, -1));
                ctx.translate(0, -m_printContext->pageRect(page).height());
                m_printContext->spoolPage(ctx, page, m_printContext->pageRect(page).width());

                CGPDFContextEndPage(context.get());
            }
        }
        CGPDFContextClose(context.get());
    }
#endif

    send(Messages::WebPageProxy::DataCallback(CoreIPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get())), callbackID));
}

#elif PLATFORM(GTK)

void WebPage::drawPagesForPrinting(uint64_t frameID, const PrintInfo& printInfo, uint64_t callbackID)
{
    beginPrinting(frameID, printInfo);
    if (m_printContext && m_printOperation) {
        m_printOperation->startPrint(m_printContext.get(), callbackID);
        return;
    }

    send(Messages::WebPageProxy::VoidCallback(callbackID));
}
#endif

#if ENABLE(TIZEN_SUPPORT_WEBAPP_META_TAG)
void WebPage::getWebAppCapable(uint64_t callbackID)
{
    RefPtr<WebCore::NodeList> nodeList = mainFrame()->document()->getElementsByTagName("meta");
    bool capable = false;

    for (unsigned i = 0; i < nodeList->length(); i++) {
        WebCore::HTMLMetaElement* metaElement = static_cast<WebCore::HTMLMetaElement*>(nodeList->item(i));
        if (metaElement->name() == "apple-mobile-web-app-capable") {
            capable = metaElement->content() == "yes" ? true : false;
            break;
        }
    }

    send(Messages::WebPageProxy::DidGetWebAppCapable(capable, callbackID));
}

void WebPage::getWebAppIconURL(uint64_t callbackID)
{
    RefPtr<WebCore::NodeList> nodeList = mainFrame()->document()->getElementsByTagName("link");
    String iconURL;

    for (unsigned i = 0; i < nodeList->length(); i++) {
        WebCore::HTMLLinkElement* linkElement = static_cast<WebCore::HTMLLinkElement*>(nodeList->item(i));
        if (linkElement->rel() == "apple-touch-icon" || linkElement->rel() == "apple-touch-icon-precomposed")
            iconURL = linkElement->href().string();
    }

    send(Messages::WebPageProxy::DidGetWebAppIconURL(iconURL, callbackID));
}

void WebPage::getWebAppIconURLs(uint64_t callbackID)
{
    RefPtr<WebCore::NodeList> nodeList = mainFrame()->document()->getElementsByTagName("link");
    Vector<std::pair<String, String> > iconURLs;

    for (unsigned i = 0; i < nodeList->length(); i++) {
        WebCore::HTMLLinkElement* linkElement = static_cast<WebCore::HTMLLinkElement*>(nodeList->item(i));
        if (linkElement->rel() == "apple-touch-icon" || linkElement->rel() == "apple-touch-icon-precomposed") {
            if (linkElement->href().string().isEmpty() || linkElement->href().string().isNull())
                continue;
            iconURLs.append(pair<String, String>(linkElement->href().string(), linkElement->iconSizes()));
        }
    }

    send(Messages::WebPageProxy::DidGetWebAppIconURLs(iconURLs, callbackID));
}
#endif

void WebPage::setMediaVolume(float volume)
{
    m_page->setMediaVolume(volume);
}

#if ENABLE(VIDEO_RESOLUTION_DISPLAY_POP_UP) && ENABLE(TIZEN_GSTREAMER_VIDEO)
void WebPage::showVideoSizeInToastedPopUp(String& videoSize)
{
    videoSize = m_fullScreenManager->showVideoSizeInToastedPopUp();
}
#endif

void WebPage::runModal()
{
    if (m_isClosed)
        return;
    if (m_isRunningModal)
        return;

    m_isRunningModal = true;
    send(Messages::WebPageProxy::RunModal());
    RunLoop::run();
    ASSERT(!m_isRunningModal);
}

void WebPage::setMemoryCacheMessagesEnabled(bool memoryCacheMessagesEnabled)
{
    m_page->setMemoryCacheClientCallsEnabled(memoryCacheMessagesEnabled);
}

bool WebPage::canHandleRequest(const WebCore::ResourceRequest& request)
{
    if (SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(request.url().protocol()))
        return true;
    return platformCanHandleRequest(request);
}

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
void WebPage::handleAlternativeTextUIResult(const String& result)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    frame->editor()->handleAlternativeTextUIResult(result);
}
#endif

void WebPage::simulateMouseDown(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseDown, static_cast<WebMouseEvent::Button>(button), position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time));
}

void WebPage::simulateMouseUp(int button, WebCore::IntPoint position, int clickCount, WKEventModifiers modifiers, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseUp, static_cast<WebMouseEvent::Button>(button), position, position, 0, 0, 0, clickCount, static_cast<WebMouseEvent::Modifiers>(modifiers), time));
}

void WebPage::simulateMouseMotion(WebCore::IntPoint position, double time)
{
    mouseEvent(WebMouseEvent(WebMouseEvent::MouseMove, WebMouseEvent::NoButton, position, position, 0, 0, 0, 0, WebMouseEvent::Modifiers(), time));
}

void WebPage::setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, compositionString.length(), Color(Color::black), false));
    frame->editor()->setComposition(compositionString, underlines, from, from + length);
}

bool WebPage::hasCompositionForTesting()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    return frame && frame->editor()->hasComposition();
}

void WebPage::confirmCompositionForTesting(const String& compositionString)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;

    if (compositionString.isNull())
        frame->editor()->confirmComposition();
    frame->editor()->confirmComposition(compositionString);
}

void WebPage::numWheelEventHandlersChanged(unsigned numWheelEventHandlers)
{
    if (m_numWheelEventHandlers == numWheelEventHandlers)
        return;

    m_numWheelEventHandlers = numWheelEventHandlers;
    recomputeShortCircuitHorizontalWheelEventsState();
}

static bool hasEnabledHorizontalScrollbar(ScrollableArea* scrollableArea)
{
    if (Scrollbar* scrollbar = scrollableArea->horizontalScrollbar())
        return scrollbar->enabled();

    return false;
}

static bool pageContainsAnyHorizontalScrollbars(Frame* mainFrame)
{
    if (FrameView* frameView = mainFrame->view()) {
        if (hasEnabledHorizontalScrollbar(frameView))
            return true;
    }

    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView)
            continue;

        const HashSet<ScrollableArea*>* scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (HashSet<ScrollableArea*>::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            if (!scrollableArea->isOnActivePage())
                continue;

            if (hasEnabledHorizontalScrollbar(scrollableArea))
                return true;
        }
    }

    return false;
}

void WebPage::recomputeShortCircuitHorizontalWheelEventsState()
{
    bool canShortCircuitHorizontalWheelEvents = !m_numWheelEventHandlers;

    if (canShortCircuitHorizontalWheelEvents) {
        // Check if we have any horizontal scroll bars on the page.
        if (pageContainsAnyHorizontalScrollbars(mainFrame()))
            canShortCircuitHorizontalWheelEvents = false;
    }

    if (m_canShortCircuitHorizontalWheelEvents == canShortCircuitHorizontalWheelEvents)
        return;

    m_canShortCircuitHorizontalWheelEvents = canShortCircuitHorizontalWheelEvents;
    send(Messages::WebPageProxy::SetCanShortCircuitHorizontalWheelEvents(m_canShortCircuitHorizontalWheelEvents));
}

Frame* WebPage::mainFrame() const
{
    return m_page ? m_page->mainFrame() : 0;
}

FrameView* WebPage::mainFrameView() const
{
    if (Frame* frame = mainFrame())
        return frame->view();
    
    return 0;
}

#if ENABLE(PAGE_VISIBILITY_API)
void WebPage::setVisibilityState(int visibilityState, bool isInitialState)
{
    if (!m_page)
        return;

    WebCore::PageVisibilityState state = static_cast<WebCore::PageVisibilityState>(visibilityState);

    if (m_visibilityState == state)
        return;

    FrameView* view = m_page->mainFrame() ? m_page->mainFrame()->view() : 0;

    if (state == WebCore::PageVisibilityStateVisible) {
        m_page->didMoveOnscreen();
        if (view)
            view->show();
    }

    m_page->setVisibilityState(state, isInitialState);
    m_visibilityState = state;

    if (state == WebCore::PageVisibilityStateHidden) {
        m_page->willMoveOffscreen();
        if (view)
            view->hide();
    }
}
#endif

#if ENABLE(TIZEN_NATIVE_MEMORY_SNAPSHOT)
const char* dumpFileName = "/tmp/webkit_memory";
char* lastUrl = 0;

void dumpMemorySnapshotForUrl(char* url)
{
    PrintFormat format = PRINT_FORMAT_JSON;
    FILE* fp = 0;
    fp = fopen(dumpFileName, "a");
    if (fp) {
        if (format == PRINT_FORMAT_JSON)
            fprintf(fp, "{");
        else
            fprintf(fp, "URL:%s\n", url);
        fprintf(fp, "%s", memoryPage(format).utf8().data());
        if (format == PRINT_FORMAT_JSON)
            fprintf(fp, "\"URL\":\"%s\"}\n", url);
        fclose(fp);
    }
    lastUrl = url;
}

void dumpMemorySnapshot()
{
    dumpMemorySnapshotForUrl(lastUrl);
}

void WebPage::dumpMemorySnapshot()
{
    char* url = strdup(mainWebFrame()->url().utf8().data());
    if (lastUrl)
        free(lastUrl);
    lastUrl = url;
}
#endif

#if ENABLE(TIZEN_BACK_FORWARD_LIST_STORE_RESTORE)
void WebPage::saveHistoryItem()
{
    HistoryController* historyController = m_page->mainFrame()->loader()->history();
    ASSERT(historyController);

#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI("[BackForward] currentItem = %p", historyController->currentItem());
#endif

    historyController->saveScrollPositionAndViewStateToItem(historyController->currentItem());
    send(Messages::WebPageProxy::BackForwardItemChanged());
}

#endif


bool WebPage::synchronousMessagesShouldSpinRunLoop()
{
#if PLATFORM(MAC)
    return WebCore::AXObjectCache::accessibilityEnabled();
#endif
    return false;
}

} // namespace WebKit
