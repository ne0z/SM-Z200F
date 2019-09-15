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

#ifndef TextSelectionHandle_h
#define TextSelectionHandle_h

#if ENABLE(TIZEN_WEBKIT2_TEXT_SELECTION)

#include "TextSelection.h"
#include <Ecore.h>
#include <Evas.h>
#include <WebCore/IntPoint.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class TextSelection;

/**
 * Contains selection handler (m_icon).
 * Selection edge (m_edge) is drawn for selection range only (does not appear
 * for HandleType::LargeHandle).
 */
class TextSelectionHandle {
public:
    enum HandleType {
        LeftHandle,
        RightHandle,
        LargeHandle,
    };

    enum HandleDirection {
        DirectionBottomNormal,
        DirectionBottomReverse,
        DirectionTopNormal,
        DirectionTopReverse,
    };

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    TextSelectionHandle(Evas_Object* object, const char* path, const char* handlerGroup, const char* edgeGroup, HandleType type, TextSelection* textSelection);
#else
    TextSelectionHandle(Evas_Object* object, const char* path, const char* handlerGroup, HandleType type, TextSelection* textSelection);
#endif
    ~TextSelectionHandle();

    Evas_Object* evas_object() { return m_icon; };
    void move(const WebCore::IntPoint& point, const WebCore::IntRect& selectionRect, HandleDirection direction);
    void show();
    void hide();
    bool isLeft() const { return (m_type ==  LeftHandle) ? true : false; }
    bool isRight() const { return (m_type ==  RightHandle) ? true : false; }
    bool isLarge() const { return (m_type ==  LargeHandle) ? true : false; }
    const WebCore::IntPoint position() const { return m_iconPosition; }
    void setBasePositionForMove(const WebCore::IntPoint& position) { m_basePositionForMove = position; }
    bool isMouseDowned() { return m_isMouseDowned; }
    bool setIsMouseDowned(bool isMouseDowned) { return m_isMouseDowned = isMouseDowned; }
    const WebCore::IntRect getHandleRect();
    bool isTop() const;
    bool isVisible() const { return evas_object_visible_get(m_icon); }

    void mouseDown(const WebCore::IntPoint& point);
    void mouseMove(const WebCore::IntPoint& point);
    void mouseUp();

private:
    // callbacks
    static void onMouseDown(void*, Evas*, Evas_Object*, void*);
    static void onMouseMove(void*, Evas*, Evas_Object*, void*);
    static void onMouseUp(void*, Evas*, Evas_Object*, void*);
    static void update(void*);

    void setFirstDownMousePosition(const WebCore::IntPoint& position) { m_firstDownMousePosition = position; }
    void setMousePosition(const WebCore::IntPoint& position) { m_mousePosition = position; }

    void showObjects();
    void hideObjects();

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    void moveObjects(const WebCore::IntPoint& handlerPosition, const WebCore::IntPoint &edgePosition);
#else
    void moveObjects(const WebCore::IntPoint& handlerPosition);
#endif
    void changeIconDirection(HandleDirection direction);

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    void updateSelectionEdgeHeight(int height);
    WebCore::IntPoint edgePosition(const WebCore::IntRect& selectionRect);
#endif

    Evas_Object* m_icon;
    Evas_Object* m_widget;
    WebCore::IntPoint m_mousePosition;
    static Ecore_Job* s_job;
    HandleType m_type;
    HandleDirection m_direction;
    TextSelection* m_textSelection;
    WebCore::IntPoint m_iconPosition;
    WebCore::IntPoint m_firstDownMousePosition;
    WebCore::IntPoint m_basePositionForMove;
    bool m_isMouseDowned;

#if ENABLE(TIZEN_TEXT_SELECTION_EDGE_SUPPORT)
    Evas_Object* m_edge;
    int m_defaultEdgeWidth;
#endif
};

} // namespace WebKit

#endif // TIZEN_WEBKIT2_TEXT_SELECTION
#endif // TextSelectionHandle_h
