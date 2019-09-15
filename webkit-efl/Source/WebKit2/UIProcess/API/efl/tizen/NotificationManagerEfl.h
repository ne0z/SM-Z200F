/*
   Copyright (C) 2014 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef NOTIFICATION_MANAGER_EFL_H
#define NOTIFICATION_MANAGER_EFL_H

#include <vector>
#include <Ecore.h>
#include <notification.h>

#include "ewk_notification.h"

namespace WebKit {

class NotificationItem;
class NotificationManagerEfl {
public:
    NotificationManagerEfl(Evas_Object* webView, EwkViewImpl* ewkView);

    Evas_Object* webView() { return m_webView; }
    EwkViewImpl* ewkview() { return m_ewkView; }
    Eina_Bool showNotification(Ewk_Notification* ewkNoti);
    Eina_Bool cancelNotification(int);
    Eina_Bool deleteAllNotifications();

private:
    Evas_Object* m_webView;
    EwkViewImpl* m_ewkView;

    std::vector<NotificationItem *> m_notiItems;
};


class NotificationItem {
public:
    NotificationItem(NotificationManagerEfl* notificationManagerEfl, Ewk_Notification* ewkNoti);
    ~NotificationItem();

    friend bool operator==(NotificationItem item1, NotificationItem item2)
    {
        return ((item1.m_origin && item1.m_title && item1.m_body)
                && (item2.m_origin && item2.m_title && item2.m_body)
                && (!strcmp(item1.m_origin, item2.m_origin))
                && (!strcmp(item1.m_title, item2.m_title))
                && (!strcmp(item1.m_body, item2.m_body)));
    }

    Eina_Bool makeNotification();
    Eina_Bool deactivateItem();
    void showNotiPopup(const char*);
    const char* origin() { return m_origin; }
    const char* title() { return m_title; }
    const char* body() { return m_body; }
    int ewkNotiId() { return m_ewkNotiId; }
    int notiId() { return m_notiId; }

private:
    void init();
    Eina_Bool createNotification(Eina_Bool);
    Eina_Bool downloadNotificationIconStart();
    Eina_Bool saveNotificationIcon(SoupBuffer*);
    void removeIconDownloadTimer();

    static void downloadNotificationIconFinishedCb(SoupSession*, SoupMessage*, gpointer);
    static Eina_Bool iconDownloadTimerExpiredCb(void*);

    Ewk_Notification* m_ewkNoti;
    NotificationManagerEfl* m_parent;
    const char* m_origin;
    const char* m_title;
    const char* m_body;
    const char* m_iconUri;
    const char* m_iconPath;
    int m_ewkNotiId;
    int m_notiId;
    double m_timeStamp;
    notification_h m_notiHandle;
    Ecore_Timer* m_iconDownloadTimer;
};

} // namespace WebKit

#endif /* NOTIFICATION_MANAGER_EFL_H */
