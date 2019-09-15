/*
 * Copyright (C) 2015 Samsung Electronics
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
 * @file    ewk_context_menu.h
 * @brief   This file describes the Ewk Context Menu API.
 */

#ifndef ewk_context_menu_product_h
#define ewk_context_menu_product_h

#include <tizen.h>
#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup WEBVIEW
 * @{
 */

/**
 * @brief The enum of Ewk_Context_Menu_Item_Tag for additional item tags
 * @since_tizen 2.4.0.2
 */

enum _Ewk_Context_Menu_New_Item_Tag{
    EWK_CONTEXT_MENU_ITEM_NEW_TAGS = 9000,
    EWK_CONTEXT_MENU_ITEM_TAG_QUICKMEMO,
    EWK_CONTEXT_MENU_ITEM_TAG_OPEN_LINK_IN_BACKGROUND
};

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif // ewk_context_menu_product_h
