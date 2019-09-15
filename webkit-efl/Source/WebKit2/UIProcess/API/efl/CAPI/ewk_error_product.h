/*
 * Copyright (C) 2013 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * @file    ewk_error_product.h
 * @brief   Describes the Web Error API.
 */

#ifndef ewk_error_product_h
#define ewk_error_product_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 * @addtogroup WEBVIEW
 * @{
 */

#ifndef ewk_error_type
#define ewk_error_type
/**
 * @brief The structure type that creates a type name for #Ewk_Error.
 * @since_tizen 2.3
 */
typedef struct Ewk_Error Ewk_Error;
#endif

EINA_DEPRECATED EXPORT_API int ewk_error_extra_code_get(const Ewk_Error* error);

EINA_DEPRECATED EXPORT_API const char* ewk_error_domain_get(const Ewk_Error* error);

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif // ewk_error_product_h
