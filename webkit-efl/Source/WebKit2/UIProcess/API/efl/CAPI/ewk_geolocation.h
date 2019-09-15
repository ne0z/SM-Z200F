/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * @file    ewk_geolocation.h
 * @brief   This file describes the Ewk Geolacation API.
 */

#ifndef ewk_geolocation_h
#define ewk_geolocation_h

#include "ewk_security_origin.h"
#include <tizen.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Ewk_Geolocation_Permission_Request Ewk_Geolocation_Permission_Request;

/**
 * @brief Requests for getting origin of geolocation permission request.
 *
 * @since_tizen 2.4
 *
 * @param[in] request Ewk_Geolocation_Permission_Request object to get origin
 *
 * @return security origin of geolocation permission data
 */
EXPORT_API const Ewk_Security_Origin *ewk_geolocation_permission_request_origin_get(const Ewk_Geolocation_Permission_Request *request);

#ifdef __cplusplus
}
#endif

#endif // ewk_geolocation_h
