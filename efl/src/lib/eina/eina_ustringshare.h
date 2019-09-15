/* EINA - EFL data type library
 * Copyright (C) 2002-2008 Carsten Haitzler, Jorge Luis Zapata Muga, Cedric Bail
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (C) 2008 Peter Wehrfritz
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies of the Software and its Copyright notices. In addition publicly
 *  documented acknowledgment must be given that this software has been used if no
 *  source code of this software is made available publicly. This includes
 *  acknowledgments in either Copyright notices, Manuals, Publicity and Marketing
 *  documents or any documentation provided with any product containing this
 *  software. This License does not apply to any software that links to the
 *  libraries provided by this software (statically or dynamically), but only to
 *  the software provided.
 *
 *  Please see the OLD-COPYING.PLAIN for a plain-english explanation of this notice
 *  and it's intent.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *  THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 *  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef EINA_USTRINGSHARE_H_
#define EINA_USTRINGSHARE_H_

#include "eina_types.h"
#include "eina_unicode.h"

/**
 * @addtogroup Eina_UStringshare_Group Unicode Stringshare
 *
 * These functions allow you to store one copy of a string, and use it
 * throughout your program.
 *
 * This is a method to reduce the number of duplicated strings kept in
 * memory. It's pretty common for the same strings to be dynamically
 * allocated repeatedly between applications and libraries, especially in
 * circumstances where you could have multiple copies of a structure that
 * allocates the string. So rather than duplicating and freeing these
 * strings, you request a read-only pointer to an existing string and
 * only incur the overhead of a hash lookup.
 *
 * It sounds like micro-optimizing, but profiling has shown this can have
 * a significant impact as you scale the number of copies up. It improves
 * string creation/destruction speed, reduces memory use and decreases
 * memory fragmentation, so a win all-around.
 *
 * For more information, you can look at the @ref tutorial_ustringshare_page.
 */

/**
 * @addtogroup Eina_Data_Types_Group Data Types
 *
 * @{
 */

/**
 * @defgroup Eina_UStringshare_Group Unicode Stringshare
 *
 * @{
 */


/**
 * @brief Retrieve an instance of a string for use in a program.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in]   str The string to retrieve an instance of.
 * @param[in]   slen The string size (<= strlen(str)).
 * @return  A pointer to an instance of the string on success.
 *          @c NULL on failure.
 *
 * @remark This function retrieves an instance of @p str. If @p str is
 * @c NULL, then @c NULL is returned. If @p str is already stored, it
 * is just returned and its reference counter is increased. Otherwise
 * it is added to the strings to be searched and a duplicated string
 * of @p str is returned.
 *
 * @remark This function does not check string size, but uses the
 * exact given size. This can be used to share_common part of a larger
 * buffer or substring.
 *
 * @see eina_ustringshare_add()
 */
EAPI const Eina_Unicode *eina_ustringshare_add_length(const Eina_Unicode *str, unsigned int slen) EINA_WARN_UNUSED_RESULT;

/**
 * @brief Retrieve an instance of a string for use in a program.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in]   str The NULL-terminated string to retrieve an instance of.
 * @return  A pointer to an instance of the string on success.
 *          @c NULL on failure.
 *
 * @remark This function retrieves an instance of @p str. If @p str is
 * @c NULL, then @c NULL is returned. If @p str is already stored, it
 * is just returned and its reference counter is increased. Otherwise
 * it is added to the strings to be searched and a duplicated string
 * of @p str is returned.
 *
 * @remark The string @p str must be NULL-terminated ('@\0') and its full
 * length will be used. To use part of the string or non-null
 * terminated, use eina_stringshare_add_length() instead.
 *
 * @see eina_ustringshare_add_length()
 */
EAPI const Eina_Unicode *eina_ustringshare_add(const Eina_Unicode *str) EINA_WARN_UNUSED_RESULT;

/**
 * @brief Increment references of the given shared string.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] str The shared string.
 * @return    A pointer to an instance of the string on success.
 *            @c NULL on failure.
 *
 * @remark This is similar to eina_share_common_add(), but it's faster since it will
 * avoid lookups if possible, but on the down side it requires the parameter
 * to be shared before, in other words, it must be the return of a previous
 * eina_ustringshare_add().
 *
 * @remark There is no unref since this is the work of eina_ustringshare_del().
 */
EAPI const Eina_Unicode *eina_ustringshare_ref(const Eina_Unicode *str);

/**
 * @brief Note that the given string has lost an instance.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] str string The given string.
 *
 * @remark This function decreases the reference counter associated to @p str
 * if it exists. If that counter reaches 0, the memory associated to
 * @p str is freed. If @p str is @c NULL, the function returns
 * immediately.
 *
 * @remark If the given pointer is not shared, bad things will happen, likely a
 * segmentation fault.
 */
EAPI void                eina_ustringshare_del(const Eina_Unicode *str);

/**
 * @brief Note that the given string @b must be shared.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] str the shared string to know the length. It is safe to
 *        give @c NULL, in that case @c -1 is returned.
 *
 * @remark This function is a cheap way to known the length of a shared
 * string.
 *
 * @remark If the given pointer is not shared, bad things will happen, likely a
 * segmentation fault. If in doubt, try strlen().
 */
EAPI int                 eina_ustringshare_strlen(const Eina_Unicode *str) EINA_PURE EINA_WARN_UNUSED_RESULT;

/**
 * @brief Dump the contents of the share_common.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remark This function dumps all strings in the share_common to stdout with a
 * DDD: prefix per line and a memory usage summary.
 */
EAPI void                eina_ustringshare_dump(void);

/**
 * @brief Replace the previously stringshared pointer with new content.
 *
 * @details The string pointed by @a p_str should be previously stringshared or
 *          @c NULL and it will be eina_ustringshare_del(). The new string will
 *          be passed to eina_ustringshare_add() and then assigned to @c *p_str.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] p_str pointer to the stringhare to be replaced. Must not be
 *        @c NULL, but @c *p_str may be @c NULL as it is a valid
 *        stringshare handle.
 * @param[in] news new string to be stringshared, may be @c NULL.
 *
 * @return @c EINA_TRUE if the strings were different and thus replaced, 
 *         @c EINA_FALSE if the strings were the same after shared.
 */
static inline Eina_Bool  eina_ustringshare_replace(const Eina_Unicode **p_str, const Eina_Unicode *news) EINA_ARG_NONNULL(1);

/**
 * @brief Replace the previously stringshared pointer with a new content.
 *
 * @details The string pointed by @a p_str should be previously stringshared or
 *          @c NULL and it will be eina_ustringshare_del(). The new string will
 *          be passed to eina_ustringshare_add_length() and then assigned to @c *p_str.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] p_str pointer to the stringhare to be replaced. Must not be
 *        @c NULL, but @c *p_str may be @c NULL as it is a valid
 *        stringshare handle.
 * @param[in] news new string to be stringshared, may be @c NULL.
 * @param[in] slen The string size (<= strlen(str)).
 *
 * @return @c EINA_TRUE if the strings were different and thus replaced,
 *         @c EINA_FALSE if the strings were the same after shared.
 */
static inline Eina_Bool  eina_ustringshare_replace_length(const Eina_Unicode **p_str, const Eina_Unicode *news, unsigned int slen) EINA_ARG_NONNULL(1);

#include "eina_inline_ustringshare.x"

/**
 * @}
 */

/**
 * @}
 */

#endif /* EINA_STRINGSHARE_H_ */
