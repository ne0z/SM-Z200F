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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#if ENABLE(TIZEN_ACCELERATED_COMPOSITING) && ENABLE(TIZEN_ACCELERATED_2D_CANVAS_EFL)
#include "Canvas2DLayerTizen.h"

#include "CanvasRenderingContext2D.h"
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING)
#include <cairo-gl.h>
#endif
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#if ENABLE(TIZEN_BROWSER_DASH_MODE)
#include <TizenSystemUtilities.h>
#endif

namespace WebCore {

PassRefPtr<Canvas2DLayerTizen> Canvas2DLayerTizen::create(Canvas2DLayerTizen* owner)
{
    return adoptRef(new Canvas2DLayerTizen(owner));
}

Canvas2DLayerTizen::Canvas2DLayerTizen(Canvas2DLayerTizen* owner)
    : m_renderingContext(0)
    , m_platformSurfaceID(0)
#if ENABLE(TIZEN_BROWSER_DASH_MODE)
    , m_countDidDraw(0)
    , m_canvasDashModeTimer(this, &Canvas2DLayerTizen::canvasDashModeFired)
#endif

{
#if ENABLE(TIZEN_DLOG_SUPPORT)
    TIZEN_LOGI("");
#endif
}

Canvas2DLayerTizen::~Canvas2DLayerTizen()
{
#if ENABLE(TIZEN_BROWSER_DASH_MODE)
    if (m_canvasDashModeTimer.isActive())
        m_canvasDashModeTimer.stop();
#endif
}

void Canvas2DLayerTizen::setContext(CanvasRenderingContext2D* context)
{
    if (!context)
        return;

    m_renderingContext = context;
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
    if (m_renderingContext->canvas() && m_renderingContext->canvas()->buffer())
        m_platformSurfaceID = m_renderingContext->canvas()->buffer()->getPlatformSurfaceID();
#endif
}

bool Canvas2DLayerTizen::swapPlatformSurfaces()
{
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
    if (m_renderingContext && m_renderingContext->canvas()) {
        if (m_renderingContext->canvas()->buffer() && m_renderingContext->canvas()->buffer()->getSurface()) {
            if (m_renderingContext->canvas()->buffer()->isLockable())
                m_renderingContext->canvas()->buffer()->swapPlatformSurfaces();
            else {
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_OFFSCREEN_BUFFER)
                if (m_renderingContext->canvas()->buffer()->isOffscreenEnabled()) {
                    m_renderingContext->canvas()->buffer()->context()->platformContext()->drawCanvasOffScreenBuffer(m_renderingContext->canvas()->buffer());
                    cairo_surface_flush(m_renderingContext->canvas()->buffer()->getPixmapSurface());
                    glFlush();
                } else
#endif
                {
                    cairo_surface_flush(m_renderingContext->canvas()->buffer()->getSurface());
                    glFlush();
                }
#if ENABLE(TIZEN_BROWSER_DASH_MODE)
                if (m_countDidDraw > 100 && !m_canvasDashModeTimer.isActive()) {
                    WTF::setWebkitDashMode(WTF::DashModeBrowserJavaScript, 3000);
                    m_canvasDashModeTimer.startOneShot(2.9);
                }
                m_countDidDraw = 0;
#endif

            }
        }
    }
#endif

    return true;
}

void Canvas2DLayerTizen::flushSurface()
{
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING)
    if (m_renderingContext && m_renderingContext->canvas())
        if (m_renderingContext->canvas()->buffer() && m_renderingContext->canvas()->buffer()->getSurface())
            if (!m_renderingContext->canvas()->buffer()->isLockable()) {
                cairo_surface_flush(m_renderingContext->canvas()->buffer()->getSurface());
            }
#endif
}

void Canvas2DLayerTizen::removePlatformSurface(int id)
{
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
    if (!m_platformSurfaceImage && m_renderingContext && m_renderingContext->canvas() && m_renderingContext->canvas()->buffer() && id == m_renderingContext->canvas()->buffer()->getPlatformSurfaceID()) {
        IntRect imageDataRect(0, 0, m_renderingContext->canvas()->width(), m_renderingContext->canvas()->height());
        m_platformSurfaceImage = m_renderingContext->canvas()->buffer()->getUnmultipliedImageData(imageDataRect);
        m_savedState = m_renderingContext->canvas()->drawingContext()->state();
        m_renderingContext->canvas()->clearBuffers();
    }
#endif
}

#if ENABLE(TIZEN_CANVAS_GRAPHICS_SURFACE)
uint32_t Canvas2DLayerTizen::copyToGraphicsSurface()
{
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
    if (m_renderingContext && m_renderingContext->canvas() && m_renderingContext->canvas()->buffer()) {
        if (m_platformSurfaceID != m_renderingContext->canvas()->buffer()->getPlatformSurfaceID()) {
            restorePlatformSurface();
            m_platformSurfaceID = m_renderingContext->canvas()->buffer()->getPlatformSurfaceID();
        }
        return m_renderingContext->canvas()->buffer()->getPlatformSurfaceID();
    }
#endif

    return m_platformSurfaceID;
}

uint64_t Canvas2DLayerTizen::graphicsSurfaceToken() const
{
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
    if (m_renderingContext && m_renderingContext->canvas() && m_renderingContext->canvas()->buffer())
        return m_renderingContext->canvas()->buffer()->getPlatformSurfaceID();
#endif

    return m_platformSurfaceID;
}
#endif

#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
void Canvas2DLayerTizen::restorePlatformSurface()
{
    if (m_renderingContext && m_renderingContext->canvas() && m_renderingContext->canvas()->buffer() && m_platformSurfaceImage) {
        IntPoint destOffset(0, 0);
        IntSize sourceSize = m_renderingContext->canvas()->size();
        IntRect sourceRect(0, 0, sourceSize.width(), sourceSize.height());
        m_renderingContext->canvas()->buffer()->putByteArray(Unmultiplied, m_platformSurfaceImage.get(), sourceSize, sourceRect, destOffset);
        m_platformSurfaceImage.clear();
        m_platformSurfaceImage = 0;
        m_renderingContext->canvas()->drawingContext()->setState(m_savedState);
    }
}
#endif

#if ENABLE(TIZEN_ACCELERATED_2D_CANVAS_EFL)
void Canvas2DLayerTizen::resetSurface()
{
#if ENABLE(TIZEN_CANVAS_CAIRO_GLES_RENDERING) || ENABLE(TIZEN_CANVAS_SURFACE_LOCKING)
    if (m_platformSurfaceImage) {
        m_platformSurfaceImage.clear();
        m_platformSurfaceImage = 0;
    }
#endif
}
#endif

} // namespace WebCore

#endif
