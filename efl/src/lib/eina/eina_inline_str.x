/* EINA - EFL data type library
 * Copyright (C) 2002-2008 Gustavo Sverzut Barbieri
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
 */

#ifndef EINA_STR_INLINE_H_
#define EINA_STR_INLINE_H_

/**
 * @addtogroup Eina_String_Group String
 *
 * @{
 */

/**
 * @brief Count up to a given amount of bytes of the given string.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] str The string pointer.
 * @param[in] maxlen The maximum length to allow.
 * @return the string size or (size_t)-1 if greater than @a maxlen.
 *
 * @remark This function returns the size of @p str, up to @p maxlen
 * characters. It avoid needless iterations after that size. @p str
 * must be a valid pointer and MUST not be @c NULL, otherwise this
 * function will crash. This function returns the string size, or
 * (size_t)-1 if the size is greater than @a maxlen.
 */
static inline size_t
eina_strlen_bounded(const char *str, size_t maxlen)
{
   const char *itr, *str_maxend = str + maxlen;
   for (itr = str; *itr != '\0'; itr++)
     if (itr == str_maxend) return (size_t)-1;
   return itr - str;
}

/**
 * @brief Join two strings of known length.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] dst The buffer to store the result.
 * @param[in] size Size (in byte) of the buffer.
 * @param[in] sep The separator character to use.
 * @param[in] a First string to use, before @p sep.
 * @param[in] b Second string to use, after @p sep.
 * @return The number of characters printed.
 *
 * @remark This function is similar to eina_str_join_len(), but will compute
 * the length of @p a  and @p b using strlen().
 *
 * @see eina_str_join_len()
 * @see eina_str_join_static()
 */
static inline size_t
eina_str_join(char *dst, size_t size, char sep, const char *a, const char *b)
{
   return eina_str_join_len(dst, size, sep, a, strlen(a), b, strlen(b));
}

/**
 * @brief strdup function which takes @c NULL without crashing
 * @param str The string to copy
 * @return the copied string, must be freed
 * @since 1.12
 */
static inline char *
eina_strdup(const char *str)
{
   return str ? strdup(str) : NULL;
}

/**
 * @brief streq function which takes @c NULL without crashing
 * @param a string a
 * @param b string b
 * @return true if strings are equal
 * @since 1.12
 */
static inline Eina_Bool
eina_streq(const char *a, const char *b)
{
   if (a == b) return EINA_TRUE;
   if (!a) return EINA_FALSE;
   if (!b) return EINA_FALSE;
   return !strcmp(a, b);
}

/**
 * @}
 */

#endif /* EINA_STR_INLINE_H_ */
