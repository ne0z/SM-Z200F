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
#include "TextSelectionHandle.h"

#include <Edje.h>

using namespace WebCore;

namespace WebKit {

Ecore_Job* TextSelectionHandle::s_job = 0;

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
TextSelectionHandle::TextSelectionHandle(Evas_Object* object, const char* themePath, const char* handlerGroup, const char* edgeGroup, HandleType type, TextSelection* textSelection)
#else
TextSelectionHandle::TextSelectionHandle(Evas_Object* object, const char* themePath, const char* handlerGroup, HandleType type, TextSelection* textSelection)
#endif
    : m_widget(object)
    , m_type(type)
    , m_textSelection(textSelection)
    , m_isMouseDowned(false)
#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    , m_edge(0) // for LargeHandle
#endif
{
    Evas* evas = evas_object_evas_get(object);
    m_icon = edje_object_add(evas);

    if (!m_icon)
        return;

    if (!edje_object_file_set(m_icon, themePath, handlerGroup))
        return;

    edje_object_signal_emit(m_icon, "edje,focus,in", "edje");
    edje_object_signal_emit(m_icon, "elm,state,bottom", "elm");
    evas_object_smart_member_add(m_icon, object);

    evas_object_propagate_events_set(m_icon, false);
    evas_object_event_callback_add(m_icon, EVAS_CALLBACK_MOUSE_DOWN, onMouseDown, this);
    evas_object_event_callback_add(m_icon, EVAS_CALLBACK_MOUSE_MOVE, onMouseMove, this);
    evas_object_event_callback_add(m_icon, EVAS_CALLBACK_MOUSE_UP, onMouseUp, this);
#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    // We intentionally do not draw selection edge for caret selection.
    if (m_type == LargeHandle)
        return;

    m_edge = edje_object_add(evas);
    if (!m_edge)
        return;

    if (!edje_object_file_set(m_edge, themePath, edgeGroup))
        return;

    edje_object_signal_emit(m_edge, "edje,focus,in", "edje");
    evas_object_smart_member_add(m_edge, object);

    // Edje sets width of selection edge depending on platform.
    edje_object_part_geometry_get(m_edge, "bg", 0, 0, &m_defaultEdgeWidth, 0);
#endif
}

TextSelectionHandle::~TextSelectionHandle()
{
    evas_object_event_callback_del(m_icon, EVAS_CALLBACK_MOUSE_DOWN, onMouseDown);
    evas_object_event_callback_del(m_icon, EVAS_CALLBACK_MOUSE_MOVE, onMouseMove);
    evas_object_event_callback_del(m_icon, EVAS_CALLBACK_MOUSE_UP, onMouseUp);
    evas_object_del(m_icon);

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    if (m_type != LargeHandle)
        evas_object_del(m_edge);
#endif

    if (s_job) {
        ecore_job_del(s_job);
        s_job = 0;
    }
}

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
void TextSelectionHandle::updateSelectionEdgeHeight(int height)
{
    if (m_type == LargeHandle)
        return;

    evas_object_resize(m_edge, 0 /* width is determined by edje) */, height);
}

IntPoint TextSelectionHandle::edgePosition(const IntRect& selectionRect)
{
    if (m_type == LargeHandle)
        return IntPoint();

    // Edge is supposed to overlap the selection.
    return IntPoint(
        isLeft() ? selectionRect.x() : selectionRect.maxX(), selectionRect.y());
}
#endif

void TextSelectionHandle::move(const WebCore::IntPoint& point, const IntRect& selectionRect, HandleDirection direction)
{
    m_iconPosition = point;
    changeIconDirection(direction);
#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    updateSelectionEdgeHeight(selectionRect.height());
    moveObjects(m_iconPosition, edgePosition(selectionRect));
#else
    moveObjects(m_iconPosition);
#endif

}

void TextSelectionHandle::show()
{
    int x, y, deviceWidth, deviceHeight;
    evas_object_geometry_get(m_widget, &x, &y, &deviceWidth, &deviceHeight);
    IntRect viewPort(x, y, deviceWidth, deviceHeight);
    // Checking Boundary conditions also if m_iconPosition is on boundary.
    if (m_iconPosition.x() >= viewPort.x() && m_iconPosition.x() <= viewPort.maxX()
       && m_iconPosition.y() >= viewPort.y() && m_iconPosition.y() <= viewPort.maxY())
        showObjects();
    else
        hide();   // Hide the handle if we are not showing it.
}

void TextSelectionHandle::hide()
{
    hideObjects();
}

const IntRect TextSelectionHandle::getHandleRect()
{
    if (!evas_object_visible_get(m_icon))
        return IntRect();

    int x, y;
    evas_object_geometry_get(m_icon, &x, &y, 0, 0);

    int partX, partY, partWidth, partHeight;
    edje_object_part_geometry_get(m_icon, "handle", &partX, &partY, &partWidth, &partHeight);

    return IntRect(x + partX, y + partY, partWidth, partHeight);
}

// callbacks
void TextSelectionHandle::mouseDown(const IntPoint& point)
{
    setIsMouseDowned(true);
    setFirstDownMousePosition(point);
    setMousePosition(point);
    m_textSelection->handleMouseDown(this, m_mousePosition);
}

void TextSelectionHandle::mouseMove(const IntPoint& point)
{
    setMousePosition(point);

    if (!s_job)
        s_job = ecore_job_add(update, this);
}

void TextSelectionHandle::mouseUp()
{
    setIsMouseDowned(false);
    m_textSelection->handleMouseUp(this, m_mousePosition);
}

void TextSelectionHandle::onMouseDown(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Down* event = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    TextSelectionHandle* handle = static_cast<TextSelectionHandle*>(data);

    handle->mouseDown(IntPoint(event->canvas.x, event->canvas.y));
}

void TextSelectionHandle::onMouseMove(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Move* event = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    TextSelectionHandle* handle = static_cast<TextSelectionHandle*>(data);

    handle->mouseMove(IntPoint(event->cur.canvas.x, event->cur.canvas.y));
}

void TextSelectionHandle::onMouseUp(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    TextSelectionHandle* handle = static_cast<TextSelectionHandle*>(data);

    handle->mouseUp();
}

// job
void TextSelectionHandle::update(void* data)
{
    TextSelectionHandle* handle = static_cast<TextSelectionHandle*>(data);

    int deltaX = handle->m_mousePosition.x() - handle->m_firstDownMousePosition.x();
    int deltaY = handle->m_mousePosition.y() - handle->m_firstDownMousePosition.y();
    if (deltaX || deltaY) {
        IntPoint movePosition;

        movePosition.setX(handle->m_basePositionForMove.x() + deltaX);
        movePosition.setY(handle->m_basePositionForMove.y() + deltaY);

        handle->m_textSelection->handleMouseMove(handle, movePosition);
    }

    s_job = 0;
}

void TextSelectionHandle::showObjects()
{
    if (ewk_settings_selection_handle_enabled_get(ewk_view_settings_get(m_widget))) {
        edje_object_message_signal_process(m_icon);
        evas_object_show(m_icon);

        // In case of autofill popup is shown, we want push it under handle icon
        auto autoFillPopup = EwkViewImpl::fromEvasObject(m_widget)->pageClient->m_autoFillPopup.get();
        if (autoFillPopup && evas_object_visible_get(autoFillPopup->m_icon)) {
            evas_object_smart_member_add(m_icon, autoFillPopup->m_icon);
            evas_object_stack_below(m_icon, autoFillPopup->m_icon);
            evas_object_smart_member_del(m_icon);
        }

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
        if (m_type != LargeHandle) {
            edje_object_message_signal_process(m_edge);
            evas_object_show(m_edge);
        }
#endif
    } else
        evas_object_smart_callback_call(m_widget, "selection,handle,show", &m_type);
}

void TextSelectionHandle::hideObjects()
{
    if (ewk_settings_selection_handle_enabled_get(ewk_view_settings_get(m_widget))) {
        evas_object_hide(m_icon);
#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
        if (m_type != LargeHandle)
            evas_object_hide(m_edge);
#endif
    } else
        evas_object_smart_callback_call(m_widget, "selection,handle,hide", &m_type);
}


#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
void TextSelectionHandle::moveObjects(const IntPoint& handlerPosition, const IntPoint& edgePosition)
#else
void TextSelectionHandle::moveObjects(const IntPoint& handlerPosition)
#endif
{
    if (ewk_settings_selection_handle_enabled_get(ewk_view_settings_get(m_widget))) {
        evas_object_move(m_icon, handlerPosition.x(), handlerPosition.y());
#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
        if (m_type != LargeHandle)
            evas_object_move(m_edge, edgePosition.x(), edgePosition.y());
#endif
    } else {
        Evas_Point point;
        point.x = handlerPosition.x();
        point.y = handlerPosition.y();

        switch (m_type) {
        case LeftHandle :
            evas_object_smart_callback_call(m_widget, "selection,handle,left,move", &point);
            break;
        case RightHandle :
            evas_object_smart_callback_call(m_widget, "selection,handle,right,move", &point);
            break;
        case LargeHandle :
            evas_object_smart_callback_call(m_widget, "selection,handle,large,move", &point);
            break;
       }
    }
}

bool TextSelectionHandle::isTop() const
{
    return (m_direction == DirectionTopNormal
            || m_direction == DirectionTopReverse) ? true : false;
}


void TextSelectionHandle::changeIconDirection(HandleDirection direction)
{
    m_direction = direction;
    if (ewk_settings_selection_handle_enabled_get(ewk_view_settings_get(m_widget))) {
        switch (direction) {
        case DirectionBottomNormal :
            if (isLarge())
                edje_object_signal_emit(m_icon, "edje,cursor,handle,show", "edje");
            else
                edje_object_signal_emit(m_icon, "elm,state,bottom", "elm");
            break;
        case DirectionBottomReverse :
            if (isLarge())
                edje_object_signal_emit(m_icon, "edje,cursor,handle,show", "edje");
            else
                edje_object_signal_emit(m_icon, "elm,state,bottom,reversed", "elm");
            break;
        case DirectionTopNormal :
            if (isLarge())
                edje_object_signal_emit(m_icon, "edje,cursor,handle,top", "edje");
            else
                edje_object_signal_emit(m_icon, "elm,state,top", "elm");
            break;
        case DirectionTopReverse :
            if (isLarge())
                edje_object_signal_emit(m_icon, "edje,cursor,handle,top", "edje");
            else
                edje_object_signal_emit(m_icon, "elm,state,top,reversed", "elm");
            break;
        }

        return;
    }

    switch (m_type) {
    case LeftHandle :
        evas_object_smart_callback_call(m_widget, "selection,handle,left,direction", &direction);
        break;
    case RightHandle :
        evas_object_smart_callback_call(m_widget, "selection,handle,right,direction", &direction);
        break;
    case LargeHandle :
        evas_object_smart_callback_call(m_widget, "selection,handle,large,direction", &direction);
        break;
    }
}

} // namespace WebKit

#endif // TIZEN_WEBKIT2_TEXT_SELECTION
