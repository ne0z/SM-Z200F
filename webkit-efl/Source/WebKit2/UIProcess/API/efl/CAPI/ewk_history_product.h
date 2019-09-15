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
 * @file    ewk_history_product.h
 * @brief   Describes the history(back & forward list) API of visited page.
 */

#ifndef ewk_history_product_h
#define ewk_history_product_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 * @addtogroup WEBVIEW
 * @{
 */

/**
 * @brief Creates a type name for _Ewk_History
 * @since_tizen 2.3
 */
typedef struct _Ewk_History Ewk_History;

/**
 * @brief Creates a type name for _Ewk_History_Item
 * @since_tizen 2.3
 */
typedef struct _Ewk_History_Item Ewk_History_Item;

/**
 * @brief Get the whole length of back list.
 *
 * @since_tizen 2.3
 *
 * @param[in] history to get the whole back list
 *
 * @return number of elements in whole list
 */
EXPORT_API int ewk_history_back_list_length_get(Ewk_History* history);

/**
 * @brief Get the whole length of forward list.
 *
 * @since_tizen 2.3
 *
 * @param[in] history to get the whole forward list
 *
 * @return number of elements in whole list
 */
EXPORT_API int ewk_history_forward_list_length_get(Ewk_History* history);

/**
 * @brief Get the item at a given index relative to the current item.
 *
 * @since_tizen 2.3
 *
 * @remarks The back list item in case of index < 0, the current item in case of index == 0, the forward list item in case of index > 0.
 *
 * @param[in] history which history instance to query
 * @param[in] index position of item to get
 *
 * @return the item at a given index relative to the current item
 */
EXPORT_API Ewk_History_Item* ewk_history_nth_item_get(Ewk_History* history, int index);

/**
 * @brief Query URI for given list item.
 *
 * @since_tizen 2.3
 *
 * @param[in] item history item to query.
 *
 * @return the URI pointer, that may be @c NULL.
 */
EXPORT_API const char* ewk_history_item_uri_get(Ewk_History_Item* item);

/**
 * @brief Free given history instance.
 *
 * @since_tizen 2.3
 *
 * @param[in] history what to free
 */
EXPORT_API void ewk_history_free(Ewk_History* history);

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif // ewk_history_product_h
