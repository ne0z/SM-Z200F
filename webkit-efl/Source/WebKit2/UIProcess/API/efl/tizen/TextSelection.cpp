/*
 * Copyright (C) 2012 Samsung Electronics
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

#if ENABLE(TIZEN_WEBKIT2_TEXT_SELECTION)
#include "TextSelection.h"

#include "EditorState.h"
#include "EwkViewImpl.h"
#include "NativeWebMouseEvent.h"
#include "ewk_view.h"
#include <Elementary.h>
#include "InputMethodContextEfl.h"
#include "WebContextMenuProxyTizen.h"
#include <utility>  // for std::swap

using namespace WebCore;

namespace WebKit {

static int s_magnifierMaxYOffsetDelta = 50;// Magnifier should not show the image below the Viewport
static int s_textSelectionMargin = 5;

TextSelection::TextSelection(EwkViewImpl* viewImpl)
      : m_viewImpl(viewImpl)
      , m_leftHandle(0)
      , m_rightHandle(0)
      , m_largeHandle(0)
      , m_isTextSelectionDowned(false)
      , m_isTextSelectionMode(ModeNone)
      , m_moveAnimator(0)
      , m_showTimer(0)
      , m_handleMovingDirection(HandleMovingDirectionNormal)
      , m_isScrollingByHandle(false)
      , m_isAutoWordSelectionScheduled(false)
      , m_showContextMenuTimer(0)
      , m_isLargeHandleScheduled(false)
      , m_inputMethodContextID(0)
      , m_relayoutRequired(false)
      , m_requestUpdatingEditorState(false)
      , m_doShowHandlesAndContextMenu(ShowNone)
      , m_lastAnimatorPoint({0, 0})
{
    ASSERT(viewWidget);

    m_magnifier = new TextSelectionMagnifier(m_viewImpl);
    evas_object_event_callback_add(m_viewImpl->view(), EVAS_CALLBACK_MOUSE_UP, onMouseUp, this);
#if ENABLE(TIZEN_WEBKIT2_BROWSER_TOOLBAR_OVERLAP_EXCEPTION)
    evas_object_smart_callback_add(m_viewImpl->view(), "browser,toolbar,changed", browserToolbarChanged, this);
#endif
}

TextSelection::~TextSelection()
{
    if (m_leftHandle)
        delete m_leftHandle;

    if (m_rightHandle)
        delete m_rightHandle;

    if (m_largeHandle)
        delete m_largeHandle;

    delete m_magnifier;

    if (m_moveAnimator) {
        ecore_animator_del(m_moveAnimator);
        m_moveAnimator = 0;
    }

    if (m_showTimer) {
        ecore_timer_del(m_showTimer);
        m_showTimer = 0;
    }

    if (m_showContextMenuTimer) {
        ecore_timer_del(m_showContextMenuTimer);
        m_showContextMenuTimer = 0;
    }

    evas_object_event_callback_del(m_viewImpl->view(), EVAS_CALLBACK_MOUSE_UP, onMouseUp);
#if ENABLE(TIZEN_WEBKIT2_BROWSER_TOOLBAR_OVERLAP_EXCEPTION)
    evas_object_smart_callback_del(m_viewImpl->view(), "browser,toolbar,changed", browserToolbarChanged);
#endif
}

void TextSelection::updateTextInputState()
{
    m_requestUpdatingEditorState = false;

    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (editorState.shouldIgnoreCompositionSelectionChange)
        return;

    if (editorState.updateEditorRectOnly) {
        if (isTextSelectionMode() && !m_viewImpl->gestureClient->isGestureWorking() && isUpdateRequired())
            requestToShow();
        return;
    }

#if ENABLE(TIZEN_WEBKIT2_GET_TEXT_STYLE_FOR_SELECTION)
    if (ewk_settings_text_style_state_enabled_get(ewk_view_settings_get(m_viewImpl->view())))
        informTextStyleState();
#endif

    if (isLargeHandleMode() && editorState.isContentEditable && !editorState.selectionIsRange
        && !m_isLargeHandleScheduled && !isTextSelectionDowned()) {
        updateLargeHandler();
        requestToShow();
        return;
    }

    if (!editorState.hasComposition && m_isAutoWordSelectionScheduled) {
        setAutoWordSelection(m_scheduledAutoWordSelectionPosition);
        return;
    }

    if (!editorState.selectionIsNone && editorState.isContentEditable) {
        if (!m_isLargeHandleScheduled && !isTextSelectionDowned() && !isTextSelectionHandleDowned())
            hideLargeHandler();
    }

    if (editorState.selectionIsNone && !isTextSelectionDowned())
        setIsTextSelectionMode(ModeNone);

    if ((m_lastEditorState.isContentEditable && !m_lastEditorState.selectionIsRange)
        && (editorState.isContentEditable && !editorState.selectionIsRange)) {
        if (!m_isLargeHandleScheduled && !isTextSelectionDowned() && !isTextSelectionHandleDowned())
            setIsTextSelectionMode(ModeNone);
    }

    if (m_lastEditorState.selectionIsRange && !editorState.selectionIsRange)
        setIsTextSelectionMode(ModeNone);

    // Auto selection happened from the JS through Editor command and update called from
    // WebPageProxy::editorStateChanged(), so need to update the selection handler and context
    // menu because focused node is currently active.
    if (editorState.selectionIsRange && !editorState.isInPasswordField) {
        if (!isTextSelectionDowned() && !isTextSelectionHandleDowned()) {
            if (isTextSelectionMode() || editorState.selectionByUserAction) {
                setIsTextSelectionMode(ModeSelection | ModeHandle | ModeContextMenu);
                if (editorState.selectionInMarque)
                    updateHandlers();
                else
                    requestToShow();
            }
        } else {
            if (!isTextSelectionMode() && editorState.selectionByUserAction)
                setIsTextSelectionMode(ModeSelection | ModeHandle | ModeContextMenu);
        }
    }

    m_lastEditorState = editorState;
}

void TextSelection::setIsTextSelectionMode(int isTextSelectionMode)
{
    if (!isTextSelectionMode) {
        if (m_showTimer) {
            ecore_timer_del(m_showTimer);
            m_showTimer = 0;
        }
        hide();
        clear();
        initHandlesMouseDownedStatus();
        setIsTextSelectionDowned(false);
        //initializing empty editor state.
        m_lastEditorState = EditorState();
    }

    int isPreviousTextSelectionMode = m_isTextSelectionMode;
    m_isTextSelectionMode = isTextSelectionMode;

    bool selectionState;
    if ((isPreviousTextSelectionMode == ModeNone) && (m_isTextSelectionMode != ModeNone)) {
        selectionState = true;
        evas_object_smart_callback_call(m_viewImpl->view(), "textselection,mode", &selectionState);
        return;
    }

    if ((isPreviousTextSelectionMode != ModeNone) && (m_isTextSelectionMode == ModeNone)) {
        selectionState = false;
        evas_object_smart_callback_call(m_viewImpl->view(), "textselection,mode", &selectionState);
    }
}

void TextSelection::clear()
{
    m_viewImpl->page()->selectionRangeClear();
}

void TextSelection::hide()
{
    hideHandlers();
    hideLargeHandler();
    hideMagnifier();
    hideContextMenu();
}

void TextSelection::initTextSelectionHandle()
{
    if (m_leftHandle && m_rightHandle && m_largeHandle)
        return;

    const Eina_List* defaultThemeList = elm_theme_list_get(0);

    const Eina_List* l;
    void* theme;
    EINA_LIST_FOREACH(defaultThemeList, l, theme) {
        char* themePath = elm_theme_list_item_path_get((const char*)theme, 0);

        if (themePath) {
#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
            m_leftHandle = new TextSelectionHandle(m_viewImpl->view(), themePath, "elm/entry/selection/block_handle_left", "elm/entry/handler/edge/start/default", TextSelectionHandle::LeftHandle, this);
            m_rightHandle = new TextSelectionHandle(m_viewImpl->view(), themePath, "elm/entry/selection/block_handle_right", "elm/entry/handler/edge/end/default", TextSelectionHandle::RightHandle, this);
            m_largeHandle = new TextSelectionHandle(m_viewImpl->view(), themePath, "elm/entry/cursor_handle/default", 0 /* no edge for caret */, TextSelectionHandle::LargeHandle, this);
#else
            m_leftHandle = new TextSelectionHandle(m_viewImpl->view(), themePath, "elm/entry/handler/start/default", TextSelectionHandle::LeftHandle, this);
            m_rightHandle = new TextSelectionHandle(m_viewImpl->view(), themePath, "elm/entry/handler/end/default", TextSelectionHandle::RightHandle, this);
            m_largeHandle = new TextSelectionHandle(m_viewImpl->view(), themePath, "elm/entry/cursor_handle/default", TextSelectionHandle::LargeHandle, this);
#endif
            edje_object_part_geometry_get(m_leftHandle->evas_object(), "handle", 0, 0, 0, &m_largeHandleSize);
            m_handleSize = 48;

            free(themePath);
            break;
        }
    }
}

void TextSelection::moveLeftHandle()
{
    if (m_lastLeftHandleRect.isEmpty()) {
        m_leftHandle->hide();
        return;
    }

    IntPoint handlePosition;
    const int reverseMargin = 32;
    if (m_viewImpl->page()->editorState().isLeftToRightDirection)
        handlePosition.setX(m_lastLeftHandleRect.x());
    else
        handlePosition.setX(m_lastLeftHandleRect.maxX());

    int webviewVerticalPosition;
    evas_object_geometry_get(m_viewImpl->view(), nullptr, &webviewVerticalPosition, nullptr, nullptr);

    // If there is a sufficient space above the left selection
    if (m_lastLeftHandleRect.y() > webviewVerticalPosition + m_handleSize) {
        handlePosition.setY(m_lastLeftHandleRect.y());
        if (handlePosition.x() <= reverseMargin)
            m_leftHandle->move(handlePosition, m_lastLeftHandleRect, TextSelectionHandle::DirectionTopReverse);
        else
            m_leftHandle->move(handlePosition, m_lastLeftHandleRect, TextSelectionHandle::DirectionTopNormal);
    } else {
        handlePosition.setY(m_lastLeftHandleRect.maxY());
        if (handlePosition.x() <= reverseMargin)
            m_leftHandle->move(handlePosition, m_lastLeftHandleRect, TextSelectionHandle::DirectionBottomReverse);
        else
            m_leftHandle->move(handlePosition, m_lastLeftHandleRect, TextSelectionHandle::DirectionBottomNormal);
    }
    m_leftHandle->show();
}
void TextSelection::moveRightHandle()
{
    if (m_lastRightHandleRect.isEmpty()) {
        m_rightHandle->hide();
        return;
    }

    IntPoint handlePosition;
    const int reverseMargin = 32;

    if (m_viewImpl->page()->editorState().isLeftToRightDirection)
        handlePosition.setX(m_lastRightHandleRect.maxX());
    else
        handlePosition.setX(m_lastRightHandleRect.x());

    int webviewHeight, webviewWidth;
    int webviewVerticalPosition, webviewHorizontalPosition;
    evas_object_geometry_get(m_viewImpl->view(), &webviewHorizontalPosition, &webviewVerticalPosition, &webviewWidth, &webviewHeight);
    // If there is a sufficient space below the right selection
    if (m_lastRightHandleRect.maxY() < webviewHeight + webviewVerticalPosition - m_handleSize) {
        handlePosition.setY(m_lastRightHandleRect.maxY());
        if (handlePosition.x() >= webviewWidth - reverseMargin)
            m_rightHandle->move(handlePosition, m_lastRightHandleRect, TextSelectionHandle::DirectionBottomReverse);
        else
            m_rightHandle->move(handlePosition, m_lastRightHandleRect, TextSelectionHandle::DirectionBottomNormal);
    } else {
        handlePosition.setY(m_lastRightHandleRect.y());
        if (handlePosition.x() >= webviewWidth - reverseMargin)
            m_rightHandle->move(handlePosition, m_lastRightHandleRect, TextSelectionHandle::DirectionTopReverse);
        else
            m_rightHandle->move(handlePosition, m_lastRightHandleRect, TextSelectionHandle::DirectionTopNormal);
    }
    m_rightHandle->show();
}


void TextSelection::updateSelectionFromEditor()
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    AffineTransform toEvasTransform = m_viewImpl->transformToScene();
    IntRect leftRect = toEvasTransform.mapRect(editorState.leftSelectionRect);
    IntRect rightRect = toEvasTransform.mapRect(editorState.rightSelectionRect);

    /* In case of editable content, we want the selection handlers
       to stay inside the editable area */
    IntRect selectionRect;
    if (editorState.isContentEditable)
        selectionRect = toEvasTransform.mapRect(editorState.editorRect);
    else
        selectionRect = toEvasTransform.mapRect(editorState.selectionRect);

    int beforeX = leftRect.x();
    leftRect.intersect(selectionRect);
    if (beforeX != leftRect.x())
        leftRect = IntRect();

    beforeX = rightRect.maxX();
    rightRect.intersect(selectionRect);
    if (beforeX != rightRect.maxX())
        rightRect = IntRect();

    m_lastLeftHandleRect = leftRect;
    m_lastRightHandleRect = rightRect;
}

void TextSelection::updateHandlers()
{
    if (!evas_object_focus_get(m_viewImpl->view()))
        return;

    initTextSelectionHandle();
    updateSelectionFromEditor();

    if (m_handleMovingDirection == HandleMovingDirectionReverse)
        std::swap(m_rightHandle, m_leftHandle);

    moveLeftHandle();
    moveRightHandle();

    if (m_handleMovingDirection == HandleMovingDirectionReverse)
        std::swap(m_rightHandle, m_leftHandle);

    setIsTextSelectionMode(isTextSelectionMode() | ModeHandle);
}

void TextSelection::hideHandlers()
{
    if (!m_leftHandle || !m_rightHandle)
        return;

    m_leftHandle->hide();
    m_rightHandle->hide();
    m_lastLeftHandleRect = IntRect();
    m_lastRightHandleRect = IntRect();
}

void TextSelection::updateLargeHandler()
{
    if (!evas_object_focus_get(m_viewImpl->view()))
        return;

    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (!editorState.isContentEditable || editorState.selectionIsNone || editorState.selectionIsRange)
        return;

    initTextSelectionHandle();

    IntRect caretRect = m_viewImpl->transformToScene().mapRect(editorState.selectionRect);

    m_isLargeHandleScheduled = true;
    m_largeHandle->hide();
    m_lastLargeHandleRect = caretRect;

    if (!caretRect.isEmpty()) {
        int webviewHeight;
        evas_object_geometry_get(m_viewImpl->view(), nullptr, nullptr, nullptr, &webviewHeight);
        IntPoint point;
        // If there is a sufficent space below the selection
        if (m_lastLargeHandleRect.maxY() < webviewHeight - m_largeHandleSize) {
            IntPoint largeHandlePoint(caretRect.x() + (caretRect.width() / 2), caretRect.maxY());
            point = largeHandlePoint;
            m_largeHandle->move(largeHandlePoint, caretRect, TextSelectionHandle::DirectionBottomNormal);
        } else {
            IntPoint largeHandlePoint(caretRect.x() + (caretRect.width() / 2), caretRect.y());
            point = largeHandlePoint;
            m_largeHandle->move(largeHandlePoint, caretRect, TextSelectionHandle::DirectionTopNormal);
        }

        if (editorState.focusedNodeRect.intersects(editorState.selectionRect) && !m_browserToolbarRect.contains(point))
            m_largeHandle->show();
    }

    setIsTextSelectionMode(isTextSelectionMode() | ModeHandle);
    m_isLargeHandleScheduled = false;
}

void TextSelection::hideLargeHandler()
{
    if (!m_largeHandle)
        return;

    m_largeHandle->hide();
}

void TextSelection::showMagnifier()
{
#if ENABLE(TIZEN_WEBKIT2_PREVENT_TEXT_SELECTION_MAGNIFIER)
    Ewk_Settings* settings = ewk_view_settings_get(m_viewImpl->view());
    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (!settings->selectionMagnifierEnabled() || (editorState.isContentEditable && !editorState.surroundingText.length()))
        return;
#endif
    m_magnifier->show();
}

void TextSelection::hideMagnifier()
{
    m_magnifier->hide();
}

void TextSelection::updateMagnifier(const IntPoint& position)
{
#if ENABLE(TIZEN_WEBKIT2_PREVENT_TEXT_SELECTION_MAGNIFIER)
    Ewk_Settings* settings = ewk_view_settings_get(m_viewImpl->view());
    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (!settings->selectionMagnifierEnabled() || (editorState.isContentEditable && !editorState.surroundingText.length()))
        return;
#endif
    m_magnifier->update(position);
    m_magnifier->move(position);
}

void TextSelection::showContextMenu()
{
    if (m_showContextMenuTimer) {
        ecore_timer_del(m_showContextMenuTimer);
        m_showContextMenuTimer = 0;
    }

    if (!isEnabled())
        return;

    if (!evas_object_focus_get(m_viewImpl->view()))
        return;

    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (!editorState.selectionIsRange && !editorState.isContentEditable)
        return;

    AffineTransform toEvasTransform = m_viewImpl->transformToScene();
    IntPoint point;
    int viewX, viewY, viewWidth, viewHeight;
    evas_object_geometry_get(m_viewImpl->view(), &viewX, &viewY, &viewWidth, &viewHeight);

    if (editorState.selectionIsRange) {
        IntRect viewRect(viewX, viewY, viewWidth, viewHeight);

        if (editorState.isContentEditable) {
            viewRect.intersect(toEvasTransform.mapRect(editorState.editorRect));
            if (viewRect.isEmpty())
                return;
        }

        IntRect rect;
        if ((rect = toEvasTransform.mapRect(editorState.leftSelectionRect)).intersects(viewRect)) {
            rect.intersect(viewRect);
            point = rect.center();
        } else if ((rect = toEvasTransform.mapRect(editorState.rightSelectionRect)).intersects(viewRect)) {
            rect.intersect(viewRect);
            point = rect.center();
        } else if ((rect = toEvasTransform.mapRect(editorState.selectionRect)).intersects(viewRect)) {
            rect.intersect(viewRect);
            point = rect.center();
        } else
            return;
    } else if (editorState.isContentEditable) {
        IntRect caretRect = editorState.selectionRect;
        if (caretRect.x() < editorState.editorRect.x())
            caretRect.shiftXEdgeTo(editorState.editorRect.x());

        if (caretRect.isEmpty())
            return;

        point = toEvasTransform.mapPoint(caretRect.center());
    }

    int deviceWidth = WebCore::getDefaultScreenResolution().width();
    int deviceHeight = WebCore::getDefaultScreenResolution().height();
#if ENABLE(TIZEN_PRERENDERING_FOR_ROTATION)
    if (m_viewImpl->currentAngle() == 90 || m_viewImpl->currentAngle() == 270) {
        int tempWidth = deviceWidth;
        deviceWidth = deviceHeight;
        deviceHeight = tempWidth;
    }
#endif
    if ((point.x() >= viewX + viewWidth) || (point.y() >= viewY + viewHeight) || (point.x() >= deviceWidth) || (point.y() >= deviceHeight)
#if ENABLE(TIZEN_WEBKIT2_BROWSER_TOOLBAR_OVERLAP_EXCEPTION)
        || m_browserToolbarRect.contains(point)
#endif
        )
        return;

    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(m_viewImpl->view()));
    if (!smartData || !smartData->api || !smartData->api->mouse_down || !smartData->api->mouse_up)
        return;

    Evas* evas = evas_object_evas_get(m_viewImpl->view());

    // send mouse down.
    Evas_Event_Mouse_Down mouseDown;
    mouseDown.button = 3;
    mouseDown.output.x = mouseDown.canvas.x = point.x();
    mouseDown.output.y = mouseDown.canvas.y = point.y();
    mouseDown.data = 0;
    mouseDown.modifiers = const_cast<Evas_Modifier*>(evas_key_modifier_get(evas));
    mouseDown.locks = const_cast<Evas_Lock*>(evas_key_lock_get(evas));
    mouseDown.flags = EVAS_BUTTON_NONE;
    mouseDown.timestamp = ecore_time_get() * 1000;
    mouseDown.event_flags = EVAS_EVENT_FLAG_NONE;
    mouseDown.dev = 0;
    smartData->api->mouse_down(smartData, &mouseDown);

    // send mouse up.
    Evas_Event_Mouse_Up mouseUp;
    mouseUp.button = 3;
    mouseUp.output.x = mouseUp.canvas.x = point.x();
    mouseUp.output.y = mouseUp.canvas.y = point.y();
    mouseUp.data = 0;
    mouseUp.modifiers = const_cast<Evas_Modifier*>(evas_key_modifier_get(evas));
    mouseUp.locks = const_cast<Evas_Lock*>(evas_key_lock_get(evas));
    mouseUp.flags = EVAS_BUTTON_NONE;
    mouseUp.timestamp = ecore_time_get() * 1000;
    mouseUp.event_flags = EVAS_EVENT_FLAG_NONE;
    mouseUp.dev = 0;
    smartData->api->mouse_up(smartData, &mouseUp);

    setIsTextSelectionMode(isTextSelectionMode() | ModeContextMenu);
}

void TextSelection::hideContextMenu()
{
    if (!isEnabled())
        return;

    m_viewImpl->page()->hideContextMenu();
}

void TextSelection::scrollContentWithSelectionIfRequired(const IntPoint& evasPoint)
{
    if (!m_leftHandle || !m_rightHandle)
        return;

    static const int scrollClipValue = 70;

    // Content should get scroll irrespective whether content is editable or not.
    m_isScrollingByHandle = true;

    int viewX, viewY, viewWidth, viewHeight;
    evas_object_geometry_get(m_viewImpl->view(), &viewX, &viewY, &viewWidth, &viewHeight);
    WebCore::IntRect visibleViewRect = IntRect(viewX, viewY, viewWidth, viewHeight);

    Evas_Object* topWidget = elm_object_top_widget_get(elm_object_parent_widget_get(m_viewImpl->view()));
    evas_object_geometry_get(topWidget, &viewX, &viewY, &viewWidth, &viewHeight);

    visibleViewRect.intersect(IntRect(viewX, viewY, viewWidth, viewHeight));

#if ENABLE(TIZEN_WEBKIT2_BROWSER_TOOLBAR_OVERLAP_EXCEPTION)
    if (!m_browserToolbarRect.isEmpty() && visibleViewRect.contains(m_browserToolbarRect.location())) {
        visibleViewRect.setHeight(visibleViewRect.height() - WebCore::intersection(visibleViewRect, m_browserToolbarRect).height());
    }
#endif

    // Y position of evasPoint from EVAS_CALLBACK_MOUSE_MOVE does not include indicator area.
    // It means that Y postion on the indicator area is negative number.
    // So, the height of indicator should be added to compare the position of view
    static const int indicatorHeight = 40;
    int evasPointY = evasPoint.y() + indicatorHeight;

    int scrollClipValueY = 70;
    // If the height of view is very small, scrollClipValue should be changed.
    // scrollClipValueY * 3 means the below:
    // =========================================
    //  The gap for the top : working scroll
    // -----------------------------------------
    //  The normal area : not working scroll
    // -----------------------------------------
    //  The gap for the bottom : working scroll
    // =========================================
    if (visibleViewRect.height() < scrollClipValueY * 3)
        scrollClipValueY = 10;

    // Scroll step when selection handler is moving out sviewport.
    static const int textSelectionScrollSize = 20;

#if ENABLE(TIZEN_WEBKIT2_TEXT_SELECTION_TEXT_CURSOR)
    static const int handleCursorHeight = 45;
#else
    static const int handleCursorHeight = 0;
#endif

    if (evasPointY >= visibleViewRect.maxY() - scrollClipValueY)
        m_viewImpl->pageClient->scrollBy(IntSize(0, textSelectionScrollSize));
    else if (evasPointY <= visibleViewRect.y() + scrollClipValueY + handleCursorHeight) {
        IntSize scrollSize(0, textSelectionScrollSize);
        scrollSize.scale(-1);
        m_viewImpl->pageClient->scrollBy(scrollSize);
    }

    if (evasPoint.x() >= (visibleViewRect.maxX() - scrollClipValue))
        m_viewImpl->pageClient->scrollBy(IntSize(textSelectionScrollSize,0));
    else if (evasPoint.x() <= visibleViewRect.x() + scrollClipValue) {
        IntSize scrollSize(textSelectionScrollSize, 0);
        scrollSize.scale(-1);
        m_viewImpl->pageClient->scrollBy(scrollSize);
    }

    m_isScrollingByHandle = false;
}

void TextSelection::setLeftSelectionToEvasPoint(const IntPoint& evasPoint)
{
    scrollContentWithSelectionIfRequired(evasPoint);
    int result = m_viewImpl->page()->setLeftSelection(m_viewImpl->transformFromScene().mapPoint(evasPoint), m_handleMovingDirection);
    if (result)
        m_handleMovingDirection = result;
    updateHandlers();
}

void TextSelection::setRightSelectionToEvasPoint(const IntPoint& evasPoint)
{
    scrollContentWithSelectionIfRequired(evasPoint);
    int result = m_viewImpl->page()->setRightSelection(m_viewImpl->transformFromScene().mapPoint(evasPoint), m_handleMovingDirection);
    if (result)
        m_handleMovingDirection = result;
    updateHandlers();
}

// handle callbacks
void TextSelection::handleMouseDown(TextSelectionHandle* handle, const IntPoint& /*position*/)
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    setIsTextSelectionMode(m_isTextSelectionMode & (ModeHandle | ModeSelection));

    if ((handle->isLeft() || handle->isRight()) && editorState.selectionIsRange) {
        int adjustValue = editorState.isOnlyImageSelection ? 1 : 2;
        IntPoint basePosition;

        if (handle->isLeft()) {
            const IntRect& leftRect = editorState.leftSelectionRect;
            if (editorState.isLeftToRightDirection) {
                basePosition.setX(leftRect.x());
                basePosition.setY(leftRect.y() + (leftRect.height() / adjustValue));
            } else {
                basePosition.setX(leftRect.x() + leftRect.width());
                basePosition.setY(leftRect.y() + (leftRect.height() / adjustValue));
            }
        } else {
            const IntRect& rightRect = editorState.rightSelectionRect;
            if (editorState.isLeftToRightDirection) {
                basePosition.setX(rightRect.x() + rightRect.width());
                basePosition.setY(rightRect.y() + (rightRect.height() / adjustValue));
            } else {
                basePosition.setX(rightRect.x());
                basePosition.setY(rightRect.y() + (rightRect.height() / adjustValue));
            }
        }
        handle->setBasePositionForMove(m_viewImpl->transformToScene().mapPoint(basePosition));
    } else if (handle->isLarge()) {
        if (!editorState.isContentEditable)
            return;

        IntRect caretRect;
        if (!editorState.selectionIsNone && !editorState.selectionIsRange)
            caretRect = editorState.selectionRect;

        if (caretRect.isEmpty())
            return;

        handle->setBasePositionForMove(m_viewImpl->transformToScene().mapPoint(caretRect.center()));
    } else
        return;

    hideContextMenu();
}

void TextSelection::handleMouseMove(TextSelectionHandle* handle, const IntPoint& position)
{
    if (!handle->isMouseDowned())
        return;

    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (handle->isLeft()) {
        if (editorState.isContentEditable)
            moveSelection(position, true);
        setLeftSelectionToEvasPoint(position);

    } else if (handle->isRight()) {
        if (editorState.isContentEditable)
            moveSelection(position, true);
        setRightSelectionToEvasPoint(position);

    } else {
        moveSelection(position, false);
        updateLargeHandler();
    }
}

void TextSelection::handleMouseUp(TextSelectionHandle* handle, const IntPoint& /* position */)
{
    m_handleMovingDirection = HandleMovingDirectionNormal;

    hideMagnifier();
    if (handle->isLeft() || handle->isRight())
        updateHandlers();
    else
        updateLargeHandler();
    showContextMenu();
}

void TextSelection::handleDown(TextSelectionHandle::HandleType type, const IntPoint& position)
{
    if (!m_leftHandle || !m_rightHandle || !m_largeHandle)
        return;

    switch (type) {
    case TextSelectionHandle::LeftHandle :
        m_leftHandle->mouseDown(position);
        break;
    case TextSelectionHandle::RightHandle :
        m_rightHandle->mouseDown(position);
        break;
    case TextSelectionHandle::LargeHandle :
        m_largeHandle->mouseDown(position);
        break;
    }
}

void TextSelection::handleMove(TextSelectionHandle::HandleType type, const IntPoint& position)
{
    if (!m_leftHandle || !m_rightHandle || !m_largeHandle)
        return;

    switch (type) {
    case TextSelectionHandle::LeftHandle :
        m_leftHandle->mouseMove(position);
        break;
    case TextSelectionHandle::RightHandle :
        m_rightHandle->mouseMove(position);
        break;
    case TextSelectionHandle::LargeHandle :
        m_largeHandle->mouseMove(position);
        break;
    }
}

void TextSelection::handleUp(TextSelectionHandle::HandleType type, const IntPoint& position)
{
    if (!m_leftHandle || !m_rightHandle || !m_largeHandle)
        return;

    switch (type) {
    case TextSelectionHandle::LeftHandle :
        m_leftHandle->mouseUp();
        break;
    case TextSelectionHandle::RightHandle :
        m_rightHandle->mouseUp();
        break;
    case TextSelectionHandle::LargeHandle :
        m_largeHandle->mouseUp();
        break;
    }
}

bool TextSelection::isMagnifierVisible()
{
    return m_magnifier->isVisible();
}

void TextSelection::updateVisible(bool isVisible, bool isHandleIncluded)
{
    if (m_isScrollingByHandle)
        return;

    if (m_showContextMenuTimer)
        return;

    if (isVisible) {
        if (m_viewImpl->gestureClient->isGestureWorking())
            return;

        if (m_showTimer)
            return;

        InputMethodContextEfl* inputMethodContext = m_viewImpl->inputMethodContext();
        if (inputMethodContext) {
            bool isIMEShow = InputMethodContextEfl::isSystemKeypadShow();
            bool isHostKeyboardOn = InputMethodContextEfl::shouldUseExternalKeyboard();
            int isIMEState = inputMethodContext->state();
            if (!isHostKeyboardOn && !isIMEShow && (isIMEState == ECORE_IMF_INPUT_PANEL_STATE_WILL_SHOW)) {
                requestToShow();
                return;
            }
        }

        if (m_doShowHandlesAndContextMenu & ShowContextMenu)
            m_doShowHandlesAndContextMenu &= (~ShowContextMenu);
        if (isTextSelectionMode() & ModeHandle) {
            if (isLargeHandleMode())
                updateLargeHandler();
            else {
                const EditorState& editorState = m_viewImpl->page()->editorState();
                if (editorState.selectionIsRange)
                    updateHandlers();
                else
                    updateLargeHandler();
            }
        }

        if ((isTextSelectionMode() & ModeContextMenu)
            && (!m_viewImpl->pageClient->isContextMenuVisible() || m_doShowHandlesAndContextMenu & ShowContextMenu)) {
            m_doShowHandlesAndContextMenu = ShowNone;
            showContextMenu();
        }
    } else {
        if (isHandleIncluded) {
            hideHandlers();
            hideLargeHandler();
        }
        hideContextMenu();
    }
}

void TextSelection::startMoveAnimator()
{
    if (!isEnabled() || !isTextSelectionDowned())
        return;
    m_lastAnimatorPoint.x = 0;
    m_lastAnimatorPoint.y = 0;

    if (!m_moveAnimator)
        m_moveAnimator = ecore_animator_add(moveAnimatorCallback, this);
}

void TextSelection::stopMoveAnimator()
{
    if (m_moveAnimator) {
        ecore_animator_del(m_moveAnimator);
        m_moveAnimator = 0;
    }
}

void TextSelection::onMouseUp(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    static_cast<TextSelection*>(data)->textSelectionUp(IntPoint());
}

Eina_Bool TextSelection::moveAnimatorCallback(void* data)
{
    TextSelection* textSelection = static_cast<TextSelection*>(data);

    Evas_Coord_Point point;
    evas_pointer_canvas_xy_get(evas_object_evas_get(textSelection->m_viewImpl->view()), &point.x, &point.y);
    if (point.x == textSelection->m_lastAnimatorPoint.x
        && point.y == textSelection->m_lastAnimatorPoint.y)
        return ECORE_CALLBACK_RENEW;
    textSelection->m_lastAnimatorPoint = point;
    textSelection->textSelectionMove(IntPoint(point.x, point.y));

    return ECORE_CALLBACK_RENEW;
}

// 'return false' means text selection is not possible for point.
// 'return true' means text selection is possible.
bool TextSelection::textSelectionDown(const IntPoint& point)
{
    // text selection should be ignored when longtap on handle from osp
    if (!isEnabled() && isTextSelectionHandleDowned())
        return false;

#if ENABLE(TIZEN_ISF_PORT)
    m_viewImpl->inputMethodContext()->resetIMFContext();
#endif
    setIsTextSelectionMode(ModeNone);
    setIsTextSelectionDowned(true);

    IntPoint contentsPoint = m_viewImpl->transformFromScene().mapPoint(point);
    bool isCaretSelection,result;

    if (!m_viewImpl->page()->editorState().isContentEmpty
        && m_viewImpl->page()->editorState().focusedNodeRect.contains(contentsPoint))
        result = m_viewImpl->page()->selectClosestWord(contentsPoint, true, isCaretSelection);
    else
        result = m_viewImpl->page()->selectClosestWord(contentsPoint, false, isCaretSelection);

    if (!result)
        return false;

    if (isTextSelectionMode())
        hide();
    else {
        if (!isCaretSelection)
            setIsTextSelectionMode(ModeSelection);
    }

    if (!isCaretSelection && !m_viewImpl->page()->editorState().isContentEditable) {
        updateMagnifier(point);
        showMagnifier();
    }
    if (isCaretSelection)
        updateLargeHandler();

    startMoveAnimator();

    return true;
}

void TextSelection::moveSelection(const IntPoint& point, bool extend)
{
    IntPoint viewPoint;
    const EditorState& editorState = m_viewImpl->page()->editorState();
    bool isInEditablePicker = false;

#if ENABLE(TIZEN_INPUT_TAG_EXTENSION)
    if (editorState.isContentEditable) {
        if (editorState.inputMethodHints == "date"
            || editorState.inputMethodHints == "datetime"
            || editorState.inputMethodHints == "datetime-local"
            || editorState.inputMethodHints == "month"
            || editorState.inputMethodHints == "time"
            || editorState.inputMethodHints == "week")
            isInEditablePicker = true;
    }
#endif

    if (editorState.isContentEditable && !isInEditablePicker) {
        IntRect mapRect = m_viewImpl->transformToScene().mapRect(editorState.editorRect);
        IntPoint updatedPoint = point;
        bool scrolledY = false;
        if (point.y() < mapRect.y()) {
            updatedPoint.setY(mapRect.y() + s_textSelectionMargin);
            if (m_viewImpl->page()->scrollContentByLine(point, DirectionBackward)) {
                scrolledY = true;
            }
        } else if (point.y() > mapRect.maxY()) {
            updatedPoint.setY(mapRect.maxY() - s_textSelectionMargin);
            if (m_viewImpl->page()->scrollContentByLine(point, DirectionForward)) {
                scrolledY = true;
            }
        }

        bool scrolledX = false;
        if (point.x() < mapRect.x()) {
            updatedPoint.setX(mapRect.x() + s_textSelectionMargin);
            if (m_viewImpl->page()->scrollContentByCharacter(point, DirectionBackward, extend)) {
                scrolledX = true;
            }
        } else if (point.x() > mapRect.maxX()) {
            updatedPoint.setX(mapRect.maxX() - s_textSelectionMargin);
            if (m_viewImpl->page()->scrollContentByCharacter(point, DirectionForward, extend)) {
                scrolledX = true;
            }
        }

        if (!scrolledX && !scrolledY) {
            viewPoint = m_viewImpl->transformFromScene().mapPoint(updatedPoint);
            bool isCaretSelection;
            m_viewImpl->page()->selectClosestWord(viewPoint, false, isCaretSelection);
        }
    } else {
        viewPoint = m_viewImpl->transformFromScene().mapPoint(point);
        bool isCaretSelection;
        m_viewImpl->page()->selectClosestWord(viewPoint, false, isCaretSelection);
        int viewX, viewY, viewWidth, viewHeight;
        evas_object_geometry_get(m_viewImpl->view(), &viewX, &viewY, &viewWidth, &viewHeight);

        if (!m_browserToolbarRect.contains(point)) {
            if (point.y() > (viewHeight + viewY - s_magnifierMaxYOffsetDelta)) {
                updateMagnifier(IntPoint(point.x(), viewHeight + viewY - s_magnifierMaxYOffsetDelta));
            } else {
                updateMagnifier(point);
            }
            showMagnifier();
        } else {
            hideMagnifier();
        }
    }

}

void TextSelection::textSelectionMove(const IntPoint& point)
{
    // text selection should be ignored when longtap on handle from osp
    if (!isEnabled() && isTextSelectionHandleDowned())
        return;

    moveSelection(point, false);
}

void TextSelection::textSelectionUp(const IntPoint& point, bool isStartedTextSelectionFromOutside)
{
    // text selection should be ignored when longtap on handle from osp
    if (!isEnabled() && isTextSelectionHandleDowned())
        return;

    stopMoveAnimator();
    hideMagnifier();

    if (!isTextSelectionMode() || m_viewImpl->gestureClient->isGestureWorking() || !isTextSelectionDowned()) {
        setIsTextSelectionDowned(false);
        return;
    }

    setIsTextSelectionDowned(false);

    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (editorState.selectionIsRange || editorState.isContentEditable) {
        if (editorState.selectionIsRange)
            updateHandlers();
        else
            updateLargeHandler();

        showContextMenu();
    } else if (!isStartedTextSelectionFromOutside)
        setIsTextSelectionMode(ModeNone);
}

bool TextSelection::isEnabled()
{
    return ewk_settings_text_selection_enabled_get(ewk_view_settings_get(m_viewImpl->view()));
}

bool TextSelection::isAutomaticClearEnabled()
{
    return ewk_settings_clear_text_selection_automatically_get(ewk_view_settings_get(m_viewImpl->view()));
}

void TextSelection::requestToShow()
{
    if (m_showContextMenuTimer) {
        requestToShowContextMenu();
        return;
    }

    if (!isTextSelectionMode() || m_isScrollingByHandle)
        return;

    if (m_showTimer)
        ecore_timer_del(m_showTimer);

    evas_object_focus_set(m_viewImpl->view(), true);
    m_showTimer = ecore_timer_loop_add((double)200.0/1000.0, showTimerCallback, this);
}

Eina_Bool TextSelection::showTimerCallback(void* data)
{
    TextSelection* textSelection = static_cast<TextSelection*>(data);
    textSelection->showHandlesAndContextMenu();

    return ECORE_CALLBACK_RENEW;
}

void TextSelection::showHandlesAndContextMenu()
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (editorState.selectionIsRange) {
        IntRect leftRect = m_viewImpl->transformToScene().mapRect(editorState.leftSelectionRect);
        IntRect rightRect = m_viewImpl->transformToScene().mapRect(editorState.rightSelectionRect);
        if (leftRect == m_lastLeftHandleRect && rightRect == m_lastRightHandleRect) {
            if (m_showTimer) {
                ecore_timer_del(m_showTimer);
                m_showTimer = 0;
            }

            updateVisible(true);
            if (editorState.leftSelectionRect != m_lastEditorState.leftSelectionRect)
                m_lastEditorState.leftSelectionRect = editorState.leftSelectionRect;
            if (editorState.rightSelectionRect != m_lastEditorState.rightSelectionRect)
                m_lastEditorState.rightSelectionRect = editorState.rightSelectionRect;

            return;
        } else {
            int viewX, viewY, viewWidth, viewHeight;
            evas_object_geometry_get(m_viewImpl->view(), &viewX, &viewY, &viewWidth, &viewHeight);
            IntRect viewRect(viewX, viewY, viewWidth, viewHeight);
            if ((m_lastLeftHandleRect != leftRect && leftRect.intersects(viewRect))
                 || (m_lastRightHandleRect != rightRect && rightRect.intersects(viewRect)))
                hide();
        }

        if (m_lastLeftHandleRect != leftRect)
            m_doShowHandlesAndContextMenu |= ShowLeftHandle;
        if (m_lastRightHandleRect != rightRect)
            m_doShowHandlesAndContextMenu |= ShowRightHandle;

        m_lastLeftHandleRect = leftRect;
        m_lastRightHandleRect = rightRect;
    } else if (editorState.isContentEditable && !editorState.selectionIsNone) {
        IntRect caretRect = m_viewImpl->transformToScene().mapRect(editorState.selectionRect);

        if (editorState.focusedNodeRect.intersects(editorState.selectionRect))
        if (caretRect == m_lastLargeHandleRect) {
            if (m_showTimer) {
                ecore_timer_del(m_showTimer);
                m_showTimer = 0;
            }
            updateVisible(true);
        }

        m_lastLargeHandleRect = caretRect;
    }
}

void TextSelection::initHandlesMouseDownedStatus()
{
    if (!m_largeHandle || !m_rightHandle || !m_largeHandle)
        return;

    m_leftHandle->setIsMouseDowned(false);
    m_rightHandle->setIsMouseDowned(false);
    m_largeHandle->setIsMouseDowned(false);
}


IntRect TextSelection::getForbiddenRegionRect()
{
    const AffineTransform& toEvasTransform = m_viewImpl->transformToScene();
    const EditorState& editorState = m_viewImpl->page()->editorState();

    // Get visible selection rectangle
    IntRect forbiddenRegion = toEvasTransform.mapRect(editorState.selectionRect);
    forbiddenRegion.intersect(m_viewportRect);

    forbiddenRegion.inflateY(ctxMenuSpacePadding);

    if (m_largeHandle->isVisible()) {
        if (m_largeHandle->isTop())
            forbiddenRegion.setY(forbiddenRegion.y() - m_largeHandleSize);

        forbiddenRegion.setHeight(forbiddenRegion.height() + m_largeHandleSize);
    }

    if (m_leftHandle->isVisible() && m_leftHandle->isTop()) {
        forbiddenRegion.setY(forbiddenRegion.y() - m_handleSize);
        forbiddenRegion.setHeight(forbiddenRegion.height() + m_handleSize);
    }

    if ((m_rightHandle->isVisible() && !m_rightHandle->isTop()) ||(m_leftHandle->isVisible() && !m_leftHandle->isTop()) ) {
        forbiddenRegion.setHeight(forbiddenRegion.height() + m_handleSize);
    }

    return forbiddenRegion;
}
void TextSelection::changeContextMenuPosition(IntPoint& position, ContextMenuDirection& drawDirection)
{
    drawDirection = DirectionNone;
    if (!m_leftHandle || !m_rightHandle || !m_largeHandle)
         return;

    updateViewportRect();

    IntRect forbiddenRegionRect = getForbiddenRegionRect();

    // The position of X-asis of context menu should always be set
    // to center position of X-asis of visible selection
    position.setX((forbiddenRegionRect.x() + forbiddenRegionRect.maxX())/2);

    // If there is sufficient space above forbidden region
    // context menu should be shown above it.
    if (forbiddenRegionRect.y() > m_viewportRect.y() + ctxMenuHeight) {
        position.setY(forbiddenRegionRect.y() + ctxMenuSpacePadding);
         drawDirection = DirectionUp;
        return;
    }
    int toolbarHeight = 0;
#if ENABLE(TIZEN_WEBKIT2_BROWSER_TOOLBAR_OVERLAP_EXCEPTION)
    if (!m_browserToolbarRect.isEmpty() && m_viewportRect.contains(m_browserToolbarRect.location())) {
        toolbarHeight = m_browserToolbarRect.height();
    }
#endif
    // If there is sufficient space below forbidden region
    // context menu should be shown below it.
    if (forbiddenRegionRect.maxY() < m_viewportRect.maxY() - ctxMenuHeight - toolbarHeight - m_handleSize) {
        // TODO: Figure out why we need to add 6.
        position.setY(forbiddenRegionRect.maxY() + m_handleSize - ctxMenuHeight/2 - 6);
        drawDirection = DirectionDown;
        return;
    }

    // If there is no sufficient spaces below and above selecetion
    // position of Y-asis of context menu should be set to
    // center of Y-asis of forbidden selection
    position.setY((forbiddenRegionRect.y() + forbiddenRegionRect.maxY())/2);
}

#if ENABLE(TIZEN_WEBKIT2_GET_TEXT_STYLE_FOR_SELECTION)
void TextSelection::informTextStyleState()
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    IntPoint startPoint, endPoint;

    if (editorState.selectionIsRange) {
        startPoint = editorState.leftSelectionRect.minXMaxYCorner();
        endPoint = editorState.rightSelectionRect.maxXMaxYCorner();
    } else if (!editorState.selectionIsNone) {
        startPoint = editorState.selectionRect.minXMaxYCorner();
        endPoint = editorState.selectionRect.maxXMaxYCorner();
    }

    AffineTransform toEvasTransform = m_viewImpl->transformToScene();
    ewkViewTextStyleState(m_viewImpl->view(), toEvasTransform.mapPoint(startPoint), toEvasTransform.mapPoint(endPoint));
}
#endif

void TextSelection::selectWordAutomatically(const IntPoint& point)
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (editorState.hasComposition) {
        setAutoWordSelectionScheduled(true, point);
        m_viewImpl->inputMethodContext()->resetIMFContext();
        return;
    }

    setAutoWordSelection(point);
}

void TextSelection::setAutoWordSelectionScheduled(bool scheduled, const IntPoint& position)
{
    m_isAutoWordSelectionScheduled = scheduled;
    m_scheduledAutoWordSelectionPosition = position;
}

void TextSelection::setAutoWordSelection(const IntPoint& point)
{
    setIsTextSelectionMode(ModeNone);
    setAutoWordSelectionScheduled(false, IntPoint(0, 0));

    IntPoint contentsPoint = m_viewImpl->transformFromScene().mapPoint(point);
    bool isCaretSelection;
    bool result = m_viewImpl->page()->selectClosestWord(contentsPoint, true, isCaretSelection);
    if (!result) {
        setIsTextSelectionMode(ModeNone);
        return;
    }

    if (isCaretSelection) {
        setIsTextSelectionMode(isTextSelectionMode() | ModeHandle | ModeContextMenu);
        m_isLargeHandleScheduled = true;
        requestToShow();
    } else
        setIsTextSelectionMode(ModeSelection);
}

void TextSelection::requestToShowContextMenu()
{
    if (m_showContextMenuTimer)
        ecore_timer_del(m_showContextMenuTimer);
    m_showContextMenuTimer = ecore_timer_loop_add((double)250.0/1000.0, showContextMenuTimerCallback, this);
}

Eina_Bool TextSelection::showContextMenuTimerCallback(void* data)
{
    TextSelection* textSelection = static_cast<TextSelection*>(data);
    textSelection->showContextMenu();

    return ECORE_CALLBACK_RENEW;
}

void TextSelection::scheduleLargeHandle()
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (editorState.isContentEditable && !editorState.selectionIsNone
        && (editorState.selectionIsRange || !editorState.selectionRect.isEmpty()))
        setShowLargeHandleScheduled(true, editorState.inputMethodContextID);
}

void TextSelection::setShowLargeHandleScheduled(bool scheduled, uintptr_t inputMethodContextID)
{
    m_isLargeHandleScheduled = scheduled;
    m_inputMethodContextID = inputMethodContextID;
}

void TextSelection::showScheduledLargeHandle()
{
    const EditorState& editorState = m_viewImpl->page()->editorState();
    if (m_isLargeHandleScheduled && (m_inputMethodContextID == editorState.inputMethodContextID)
        && !editorState.selectionIsNone && !editorState.selectionIsRange && editorState.isContentEditable && !editorState.isContentEmpty)
        updateLargeHandler();
    else
        hideLargeHandler();

    setShowLargeHandleScheduled(false, editorState.inputMethodContextID);
}

// When content size is changed don't call Update if there is no left Rect and right rects are not changed.
bool TextSelection::isUpdateRequired()
{
    const EditorState& editorState = m_viewImpl->page()->editorState();

    if (editorState.selectionIsRange) {
        IntRect leftRect = m_viewImpl->transformToScene().mapRect(editorState.leftSelectionRect);
        IntRect rightRect = m_viewImpl->transformToScene().mapRect(editorState.rightSelectionRect);
        if (leftRect == m_lastLeftHandleRect && rightRect == m_lastRightHandleRect) {
            if (m_leftHandle && m_leftHandle->isVisible() && m_rightHandle && m_rightHandle->isVisible())
                return false;
        }
        if (editorState.leftSelectionRect == m_lastEditorState.leftSelectionRect && editorState.rightSelectionRect == m_lastEditorState.rightSelectionRect)
            return false;
    } else if (editorState.isContentEditable && !editorState.selectionIsNone) {
        IntRect caretRect = m_viewImpl->transformToScene().mapRect(editorState.selectionRect);
        if (caretRect == m_lastLargeHandleRect)
            return false;
    }

    return true;
}

void TextSelection::setRelayoutRequired(bool required)
{
    m_relayoutRequired = required;

    if (!m_relayoutRequired)
        return;

    if (m_showTimer) {
        ecore_timer_del(m_showTimer);
        m_showTimer = 0;
    }
}

bool TextSelection::isTextSelectionHandleDowned()
{
    if (!m_leftHandle || !m_rightHandle || !m_largeHandle)
        return false;

    return m_leftHandle->isMouseDowned() || m_rightHandle->isMouseDowned() || m_largeHandle->isMouseDowned();
}

void TextSelection::hideMagnifierOnRotation()
{
    if (isMagnifierVisible())
        hideMagnifier();
}

void TextSelection::updateViewportRect()
{
    int viewX, viewY, viewWidth, viewHeight;
    evas_object_geometry_get(m_viewImpl->view(), &viewX, &viewY, &viewWidth, &viewHeight);

    m_viewportRect = IntRect(viewX, viewY, viewWidth, viewHeight);
}

void TextSelection::requestUpdatingEditorState()
{
    m_requestUpdatingEditorState = true;
    m_viewImpl->page()->process()->send(Messages::WebPage::RequestUpdatingEditorState(), m_viewImpl->page()->pageID());
}

#if ENABLE(TIZEN_WEBKIT2_BROWSER_TOOLBAR_OVERLAP_EXCEPTION)
void TextSelection::browserToolbarChanged(void *data, Evas_Object *obj, void *eventInfo)
{
    TextSelection* textSelection = static_cast<TextSelection*>(data);

    Eina_Rectangle* rect = static_cast<Eina_Rectangle*>(eventInfo);
    textSelection->setBrowserToolbarRect(IntRect(rect->x, rect->y, rect->w, rect->h));
}

void TextSelection::setBrowserToolbarRect(const IntRect& rect)
{
    m_browserToolbarRect = rect;
}
#endif

} // namespace WebKit

#endif // TIZEN_WEBKIT2_TEXT_SELECTION
