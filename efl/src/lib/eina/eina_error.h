/* EINA - EFL data type library
 * Copyright (C) 2007-2008 Jorge Luis Zapata Muga, Cedric Bail
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

#ifndef EINA_ERROR_H_
#define EINA_ERROR_H_

#include <stdarg.h>

#include "eina_types.h"


/**
 * @page tutorial_error_page Error Tutorial
 *
 * This feature is not supported on Tizen.
 *
 * @section tutorial_error_registering_msg Registering messages
 *
 * The error module can provide a system that mimics the errno system
 * of the C standard library. It consists in 2 parts:
 *
 *
 * So one has to first register all the error messages that a program
 * or a library should manage. Then, when an error occurs, use
 * Eina Error Set function, and when errors are managed, use
 * Eina Error Get function. If Eina Error Set function is used to set an error, do
 * not forget to remove previous set errors before calling Eina Error Set function.
 *
 * Here is an example for use:
 *
 * @dontinclude eina_error_01.c
 *
 * Of course, instead of printf(), Eina Log Print function can be used to
 * have beautiful error messages.
 */

/**
 * @defgroup Eina_Error_Group Error
 * @ingroup Eina_Tools_Group
 *
 * @brief This group discusses the functions that provide error management for projects.
 *
 * The Eina error module provides a way to manage errors in a simple but
 * powerful way in libraries and modules. It is also used in Eina itself.
 * Similar to libC's @c errno and strerror() facilities, this is extensible and
 * recommended for other libraries and applications as well.
 *
 * A simple example of how to use this can be seen @ref tutorial_error_page
 * "here".
 *
 * @{
 */

/**
 * @typedef Eina_Error
 * @brief The integer type containing the error type.
 */
typedef int Eina_Error;

/**
 * @internal
 * @var EINA_ERROR_OUT_OF_MEMORY
 * @brief The error identifier corresponding to lack of memory.
 */
EAPI extern Eina_Error EINA_ERROR_OUT_OF_MEMORY;

/**
 * @internal
 * @brief Registers a new error type.
 *
 * @param[in] msg The description of the error \n
 *                It is duplicated using eina_stringshare_add().
 * @return The unique number identifier for this error
 *
 * @details This function stores the error message described by
 *          @p msg in a list. The returned value is a unique identifier greater than or equal
 *          to @c 1. The description can be retrieved later by passing
 *          the returned value to eina_error_msg_get().
 *
 * @see eina_error_msg_static_register()
 */
EAPI Eina_Error  eina_error_msg_register(const char *msg) EINA_ARG_NONNULL(1) EINA_WARN_UNUSED_RESULT;

/**
 * @internal
 * @brief Registers a new error type, statically allocated message.
 *
 * @param[in] msg The description of the error \n
 *            This string is not duplicated and thus
 *            the given pointer should live during the usage of eina_error.
 * @return The unique number identifier for this error
 *
 * @details This function stores the error message described by
 *          @p msg in a list. The returned value is a unique identifier greater than or equal
 *          to @c 1. The description can be retrieved later by passing
 *          the returned value to eina_error_msg_get().
 *
 * @see eina_error_msg_register()
 */
EAPI Eina_Error  eina_error_msg_static_register(const char *msg) EINA_ARG_NONNULL(1) EINA_WARN_UNUSED_RESULT;

/**
 * @internal
 * @brief Changes the message of an already registered message.
 *
 * @param[in] error The Eina_Error to change the message of
 * @param[in] msg The description of the error \n
 *            This string is duplicated only if the error is registered with @ref eina_error_msg_register,
 *            otherwise it must remain intact for the duration.
 * @return #EINA_TRUE if successful, otherwise #EINA_FALSE on error
 *
 * @details This function modifies the message associated with @p error and changes
 *          it to @p msg. If the error is previously registered by @ref eina_error_msg_static_register
 *          then the string is not duplicated, otherwise the previous message
 *          is unrefed and @p msg is copied.
 *
 * @see eina_error_msg_register()
 */
EAPI Eina_Bool   eina_error_msg_modify(Eina_Error  error,
                                       const char *msg) EINA_ARG_NONNULL(2);

/**
 * @internal
 * @brief Returns the last set error.
 *
 * @return The last error
 *
 * @details This function returns the last error set by eina_error_set(). The
 *          description of the message is returned by eina_error_msg_get().
 *
 * @note This function is thread safe @since 1.10, but slower to use.
 */
EAPI Eina_Error  eina_error_get(void);

/**
 * @internal
 * @brief Sets the last error.
 *
 * @param[in] err The error identifier
 *
 * @details This function sets the last error identifier. The last error can be
 *          retrieved by eina_error_get().
 *
 * @note This is also used to clear previous errors, in which case @p err should
 *        be @c 0.
 *
 * @note This function is thread safe @since 1.10, but slower to use.
 */
EAPI void        eina_error_set(Eina_Error err);

/**
 * @internal
 * @brief Returns the description of the given error number.
 *
 * @param[in] error The error number
 * @return The description of the error
 *
 * @details This function returns the description of an error that has been
 *          registered by eina_error_msg_register(). If an incorrect error is
 *          given, then @c NULL is returned.
 */
EAPI const char *eina_error_msg_get(Eina_Error error) EINA_PURE;

/**
 * @internal
 * @brief Finds the #Eina_Error corresponding to a message string.
 *
 * @param[in] msg The error message string to match (NOT @c NULL)
 * @return The #Eina_Error matching @p msg, otherwise @c 0 on failure
 *
 * @details This function attempts to match @p msg with its corresponding #Eina_Error value.
 *          If no such value is found, @c 0 is returned.
 */
EAPI Eina_Error  eina_error_find(const char *msg) EINA_ARG_NONNULL(1) EINA_PURE;

/**
 * @}
 */

#endif /* EINA_ERROR_H_ */
