/*
   Copyright (C) 2012 Samsung Electronics

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

#ifndef ewk_user_media_h
#define ewk_user_media_h

#include <tizen.h>
#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Ewk_User_Media_Permission_Request Ewk_User_Media_Permission_Request;

/**
 * Requests for getting origin of local media permission request.
 *
 * @param request Ewk_User_Media_Permission_Request object to get origin for userMedia permission request
 *
 * @return security origin of userMedia permission request
 */
EXPORT_API const Ewk_Security_Origin *ewk_user_media_permission_request_origin_get(const Ewk_User_Media_Permission_Request *request);

/**
 * Sets the permission to access local media
 *
 * @param request Ewk_View_User_Media_Permission_Request object for userMedia permission
 * @param allowed decided permission value from user
 */
EINA_DEPRECATED EXPORT_API void ewk_user_media_permission_request_set(Ewk_User_Media_Permission_Request *request, Eina_Bool allowed);

/**
 * Suspend the operation for user media permission
 *
 * @param request user media permission request object
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 */
EINA_DEPRECATED EXPORT_API Eina_Bool ewk_user_media_permission_request_suspend(Ewk_User_Media_Permission_Request *request);

/**
 * Reply the result about user media permission.
 *
 * @param request Ewk_User_Media_Permission_Request object to get the information about user media permission request
 * @param allow result about user media permission
 */
EXPORT_API void ewk_user_media_permission_reply(Ewk_User_Media_Permission_Request* permissionRequest, Eina_Bool allow);

#ifdef __cplusplus
}
#endif
#endif // ewk_user_media_h
