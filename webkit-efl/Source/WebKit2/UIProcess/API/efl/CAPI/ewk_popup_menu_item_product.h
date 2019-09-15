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
 * @file    ewk_popup_menu_item_product.h
 * @brief   Describes the Ewk Popup Menu Item API.
 */

#ifndef ewk_popup_menu_item_product_h
#define ewk_popup_menu_item_product_h

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
 * @brief Enum values containing type of popup menu item.
 * @since_tizen 2.3
 */
typedef enum {
    EWK_POPUP_MENU_SEPARATOR,   /**< Separator type */
    EWK_POPUP_MENU_ITEM,    /**< Item type */
    EWK_POPUP_MENU_UNKNOWN = -1 /**< Unknown type */
} Ewk_Popup_Menu_Item_Type;

/**
 * @brief Creates a type name for #Ewk_Popup_Menu_Item
 * @since_tizen 2.3
 */
typedef struct Ewk_Popup_Menu_Item Ewk_Popup_Menu_Item;

/**
 * @brief Returns type of the popup menu item.
 *
 * @since_tizen 2.3
 *
 * @param[in] item the popup menu item instance
 *
 * @return the type of the @a item or @c #EWK_POPUP_MENU_UNKNOWN in case of error
 */
EXPORT_API Ewk_Popup_Menu_Item_Type ewk_popup_menu_item_type_get(const Ewk_Popup_Menu_Item* item);

/**
 * @brief Returns text of the popup menu item.
 *
 * @since_tizen 2.3
 *
 * @param[in] item the popup menu item instance
 *
 * @return the text of the @a item or @c NULL in case of error. This pointer is\n
 *         guaranteed to be eina_stringshare, so whenever possible\n
 *         save yourself some cpu cycles and use\n
 *         eina_stringshare_ref() instead of eina_stringshare_add() or\n
 *         strdup()
 */
EXPORT_API const char* ewk_popup_menu_item_text_get(const Ewk_Popup_Menu_Item* item);

/**
 * @brief Returns whether the popup menu item is selected or not.
 *
 * @since_tizen 2.3
 *
 * @param[in] item the popup menu item instance
 *
 * @return @c EINA_TRUE if the popup menu item is selected, @c EINA_FALSE otherwise
 */
EXPORT_API Eina_Bool ewk_popup_menu_item_selected_get(const Ewk_Popup_Menu_Item* item);

/**
* @}
*/

#ifdef __cplusplus
}
#endif
#endif // ewk_popup_menu_item_product_h
