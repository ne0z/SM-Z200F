/**
 * @brief Add a new conformant widget to the given parent Elementary
 * (container) object.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] parent The parent object.
 * @return A new conformant widget handle or @c NULL, on errors.
 *
 * @remark This function inserts a new conformant widget on the canvas.
 *
 * @ingroup Conformant
 */
EAPI Evas_Object                 *elm_conformant_add(Evas_Object *parent);

