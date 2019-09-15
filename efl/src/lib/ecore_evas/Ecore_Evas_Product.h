#ifndef __ECORE_EVAS_PRODUCT_H__
#define __ECORE_EVAS_PRODUCT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Creates a new @c Ecore_Evas canvas bound to the Evas
 *          @b buffer engine, giving custom allocation and freeing functions for
 *          the canvas memory region.
 *
 * @details Like ecore_evas_buffer_allocfunc_new(), but it also specifies stride of canvas.
 *
 * @remarks This function is used when the stride of buffer is different from @a w times @a sizeof(int).
 *
 *
 * @param   w           The width of the canvas, in canvas units
 * @param   h           The height of the canvas, in canvas units
 * @param   alloc_func_with_stride  The function to be called to allocate the memory
 *                      needed for the new buffer canvas \n
 *                      @a data is passed the same value as the @a data of this function,
 *                      while @a size is passed @a w times @a h times @c sizeof(int).
 *                      @a stride and @bpp would be assigned within @a allocfunc.
 * @param   free_func   The function to be called to free the memory used by
 *                      the new buffer canvas \n
 *                      @a data will be passed the same value as the @a data of this function,
 *                      while @a pix will be passed the canvas memory pointer.
 * @param   data        The custom data to be passed to the allocation and freeing functions
 * @return  A new @c Ecore_Evas instance, \n
 *          otherwise @c NULL on failure
 *
 * @see ecore_evas_buffer_new()
 */
EAPI Ecore_Evas     *ecore_evas_buffer_allocfunc_with_stride_new(int w, int h, void *(*alloc_func_with_stride) (void *data, int size, int *stride, int *bpp), void (*free_func) (void *data, void *pix), const void *data);

/**
 * @brief   Set a custom allocation function for the memory region of @c Ecore_Evas canvas
 *          bound to the Evas @b buffer engine.
 *
 * @remarks This function is useful when one wants an @c Ecore_Evas buffer
 *          canvas with a custom allocation function, like one getting memory
 *          chunks from a memory pool, for example.
 *
 * @remarks This enables to set the allocfunc anytime. Not only creation time of @c Ecore_Evas @b buffer backend.
 *
 * @param   ee  An @c Ecore_Evas handle what you want to set the @c allocfunc at.
 * @param   alloc_func_with_stride  The function to be called to allocate the memory
 *                      needed for the new buffer canvas \n
 *                      @a data will be passed the same value as the @a data of this function,
 *                      while @a size is passed @a w times @a h times @c sizeof(int).
 *                      @a stride and @bpp would be assigned within @a allocfunc.
 *
 * @see ecore_evas_buffer_allocfunc_with_stride_new()
 * @see ecore_evas_buffer_freefunc_set()
 */
EAPI void    ecore_evas_buffer_allocfunc_set(Ecore_Evas *ee, void *(*alloc_func_with_stride) (void *data, int size, int *stride, int *bpp));

/**
 * @brief   Set a custom freeing function for the memory region of @c Ecore_Evas canvas
 *          bound to the Evas @b buffer engine.
 *
 * @remarks This function is useful when one wants to free an @c Ecore_Evas @b buffer
 *          canvas with a custom freeing function.
 *
 * @remarks This enables to set the user specific method for freeing @c Ecore_Evas @b buffer canvas.
 *
 * @param   ee  An @c Ecore_Evas handle what you want to set the @c allocfunc at.
 * @param   free_func  The function to be called to free the memory of buffer canvas.\n
 *                      @a data will be passed the same value as the @a data of this function,
 *                      while @a pix will be passed the canvas memory pointer.
 *
 * @see ecore_evas_buffer_allocfunc_with_stride_new()
 * @see ecore_evas_buffer_allocfunc_set()
 */

EAPI void    ecore_evas_buffer_freefunc_set(Ecore_Evas *ee, void (*free_func) (void *data, void *pix));

/**
 * @brief   Store user data to given @c Ecore_Evas @b buffer canvas/window.
 *
 * @remarks This function store the user specific data in a given Ecore_Evas buffer canvas.
 *
 * @param   ee  An @c Ecore_Evas handle to store the user data in.
 * @param   data A pointer to the user data to store.
 *
 * @see ecore_evas_buffer_data_get()
 */
EAPI void    ecore_evas_buffer_data_set(Ecore_Evas *ee, void *data);

/**
 * @brief   Retrieve user data associated with a given Ecore_Evas @b buffer canvas/window.
 *
 * @remarks This function retrieve the user specific data that has been stored
 *                   within an @aEcore_Evas buffer canvas with ecore_evas_buffer_data_set().
 *
 * @param   ee  An @c Ecore_Evas handle to retrieve the user data from.
 * @return  NULL on error or no data found. A pointer to the user data of @a ee
 *
 * @see ecore_evas_buffer_data_set()
 */
EAPI void*   ecore_evas_buffer_data_get(Ecore_Evas *ee);


#ifdef __cplusplus
}
#endif

#endif

