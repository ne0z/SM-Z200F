/*
    Copyright (C) 2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"
#include "EWK2UnitTestBase.h"

#include "EWK2UnitTestEnvironment.h"
#include <Ecore.h>
#include <glib-object.h>
#include <wtf/UnusedParam.h>

#if PLATFORM(TIZEN) && defined(HAVE_ECORE_X)
#include <Ecore_X.h>
#endif

#if ENABLE(TIZEN_WEBKIT2_UNIT_TESTS)
#include <Elementary.h>
#endif

extern EWK2UnitTest::EWK2UnitTestEnvironment* environment;

namespace EWK2UnitTest {

EWK2UnitTestBase::EWK2UnitTestBase()
    : m_webView(0)
    , m_ecoreEvas(0)
    , m_ewkViewClass(EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("Browser_View"))
{
    ewk_view_smart_class_set(&m_ewkViewClass);
}

void EWK2UnitTestBase::SetUp()
{
    ewk_init();
#if PLATFORM(TIZEN) && defined(HAVE_ECORE_X)
    // Source connected with TIZEN_VIEWPORT_META_TAG macro (even if it's disabled)
    // uses ecore_x library which should be initialized by the application.
    if (!ecore_x_init(0))
        return;
#endif

#if ENABLE(TIZEN_WEBKIT2_UNIT_TESTS)
    elm_init(0, 0);
#endif

    unsigned int width = environment->defaultWidth();
    unsigned int height = environment->defaultHeight();

    if (environment->useX11Window())
        m_ecoreEvas = ecore_evas_new(0, 0, 0, width, height, 0);
    else
        m_ecoreEvas = ecore_evas_buffer_new(width, height);

    ecore_evas_show(m_ecoreEvas);
    Evas* evas = ecore_evas_get(m_ecoreEvas);

    Evas_Smart* smart = evas_smart_class_new(&m_ewkViewClass.sc);
    m_webView = ewk_view_smart_add(evas, smart, ewk_context_default_get());
    ewk_view_theme_set(m_webView, environment->defaultTheme());

    evas_object_resize(m_webView, width, height);
    evas_object_show(m_webView);
    evas_object_focus_set(m_webView, true);
}

void EWK2UnitTestBase::TearDown()
{
    evas_object_del(m_webView);
    ecore_evas_free(m_ecoreEvas);
    ewk_shutdown();
#if PLATFORM(TIZEN) && defined(HAVE_ECORE_X)
    ecore_x_shutdown();
#endif
}

bool EWK2UnitTestBase::loadUrlSync(const char* url, double timeoutSeconds)
{
    ewk_view_url_set(m_webView, url);
    return waitUntilLoadFinished(timeoutSeconds);
}

class CallbackDataTimer {
public:
    CallbackDataTimer(double timeoutSeconds, Ecore_Task_Cb callback)
        : m_done(false)
        , m_timer(timeoutSeconds >= 0 ? ecore_timer_add(timeoutSeconds, callback, this) : 0)
        , m_didTimeOut(false)
    {
    }

    virtual ~CallbackDataTimer()
    {
        if (m_timer)
            ecore_timer_del(m_timer);
    }

    bool isDone() const { return m_done; }

    void setDone()
    {
        if (m_timer) {
            ecore_timer_del(m_timer);
            m_timer = 0;
        }
        m_done = true;
    }

    bool didTimeOut() const { return m_didTimeOut; }

    void setTimedOut()
    {
        m_done = true;
        m_timer = 0;
        m_didTimeOut = true;
    }

protected:
    bool m_done;
    Ecore_Timer* m_timer;
    bool m_didTimeOut;
};

template <class T>
class CallbackDataExpectedValue : public CallbackDataTimer {
public:
    CallbackDataExpectedValue(const T& expectedValue, double timeoutSeconds, Ecore_Task_Cb callback)
        : CallbackDataTimer(timeoutSeconds, callback)
        , m_expectedValue(expectedValue)
    {
    }

    const T& expectedValue() const { return m_expectedValue; }

private:
    T m_expectedValue;
};

static void onLoadFinished(void* userData, Evas_Object* webView, void* eventInfo)
{
    UNUSED_PARAM(webView);
    UNUSED_PARAM(eventInfo);

    CallbackDataTimer* data = static_cast<CallbackDataTimer*>(userData);
    data->setDone();
}

static bool timeOutWhileWaitingUntilLoadFinished(void* userData)
{
    CallbackDataTimer* data = static_cast<CallbackDataTimer*>(userData);
    data->setTimedOut();

    return ECORE_CALLBACK_CANCEL;
}

bool EWK2UnitTestBase::waitUntilLoadFinished(double timeoutSeconds)
{
    CallbackDataTimer data(timeoutSeconds, reinterpret_cast<Ecore_Task_Cb>(timeOutWhileWaitingUntilLoadFinished));

    evas_object_smart_callback_add(m_webView, "load,finished", onLoadFinished, &data);

    while (!data.isDone())
        ecore_main_loop_iterate();

    evas_object_smart_callback_del(m_webView, "load,finished", onLoadFinished);

    return !data.didTimeOut();
}

static void onTitleChanged(void* userData, Evas_Object* webView, void*)
{
    CallbackDataExpectedValue<CString>* data = static_cast<CallbackDataExpectedValue<CString>*>(userData);

    if (strcmp(ewk_view_title_get(webView), data->expectedValue().data()))
        return;

    data->setDone();
}

static bool timeOutWhileWaitingUntilTitleChangedTo(void* userData)
{
    CallbackDataExpectedValue<CString>* data = static_cast<CallbackDataExpectedValue<CString>*>(userData);
    data->setTimedOut();

    return ECORE_CALLBACK_CANCEL;
}

bool EWK2UnitTestBase::waitUntilTitleChangedTo(const char* expectedTitle, double timeoutSeconds)
{
    CallbackDataExpectedValue<CString> data(expectedTitle, timeoutSeconds, reinterpret_cast<Ecore_Task_Cb>(timeOutWhileWaitingUntilTitleChangedTo));

    evas_object_smart_callback_add(m_webView, "title,changed", onTitleChanged, &data);

    while (!data.isDone())
        ecore_main_loop_iterate();

    evas_object_smart_callback_del(m_webView, "title,changed", onTitleChanged);

    return !data.didTimeOut();
}

static void onURLChanged(void* userData, Evas_Object* webView, void*)
{
    CallbackDataExpectedValue<CString>* data = static_cast<CallbackDataExpectedValue<CString>*>(userData);

    if (strcmp(ewk_view_url_get(webView), data->expectedValue().data()))
        return;

    data->setDone();
}

static bool timeOutWhileWaitingUntilURLChangedTo(void* userData)
{
    CallbackDataExpectedValue<CString>* data = static_cast<CallbackDataExpectedValue<CString>*>(userData);
    data->setTimedOut();

    return ECORE_CALLBACK_CANCEL;
}

bool EWK2UnitTestBase::waitUntilURLChangedTo(const char* expectedURL, double timeoutSeconds)
{
    CallbackDataExpectedValue<CString> data(expectedURL, timeoutSeconds, reinterpret_cast<Ecore_Task_Cb>(timeOutWhileWaitingUntilURLChangedTo));

    evas_object_smart_callback_add(m_webView, "url,changed", onURLChanged, &data);

    while (!data.isDone())
        ecore_main_loop_iterate();

    evas_object_smart_callback_del(m_webView, "url,changed", onURLChanged);

    return !data.didTimeOut();
}

void EWK2UnitTestBase::mouseClick(int x, int y)
{
    Evas* evas = evas_object_evas_get(m_webView);
    evas_event_feed_mouse_move(evas, x, y, 0, 0);
    evas_event_feed_mouse_down(evas, /* Left */ 1, EVAS_BUTTON_NONE, 0, 0);
    evas_event_feed_mouse_up(evas, /* Left */ 1, EVAS_BUTTON_NONE, 0, 0);
}

void EWK2UnitTestBase::mouseDown(int x, int y)
{
    Evas* evas = evas_object_evas_get(m_webView);
    evas_event_feed_mouse_move(evas, x, y, 0, 0);
    evas_event_feed_mouse_down(evas, /* Left */ 1, EVAS_BUTTON_NONE, 0, 0);
}

void EWK2UnitTestBase::mouseUp(int x, int y)
{
    Evas* evas = evas_object_evas_get(m_webView);
    evas_event_feed_mouse_move(evas, x, y, 0, 0);
    evas_event_feed_mouse_up(evas, /* Left */ 1, EVAS_BUTTON_NONE, 0, 0);
}

void EWK2UnitTestBase::mouseMove(int x, int y)
{
    evas_event_feed_mouse_move(evas_object_evas_get(m_webView), x, y, 0, 0);
}

void EWK2UnitTestBase::multiDown(int id, int x, int y)
{
    evas_event_feed_multi_down(evas_object_evas_get(m_webView), id, x, y, 0, 0, 0, 0, 0, 0, 0, EVAS_BUTTON_NONE, 0, 0);
}

void EWK2UnitTestBase::multiUp(int id, int x, int y)
{
    evas_event_feed_multi_up(evas_object_evas_get(m_webView), id, x, y, 0, 0, 0, 0, 0, 0, 0, EVAS_BUTTON_NONE, 0, 0);
}

void EWK2UnitTestBase::multiMove(int id, int x, int y)
{
    evas_event_feed_multi_move(evas_object_evas_get(m_webView), id, x, y, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

} // namespace EWK2UnitTest
