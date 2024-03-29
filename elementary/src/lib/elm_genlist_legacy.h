/**
 * @defgroup Genlist Genlist
 * @ingroup elm_widget_group
 *
 * @image html genlist_inheritance_tree.png
 * @image latex genlist_inheritance_tree.eps
 *
 * @image html img/genlist.png
 * @image latex img/genlist.eps
 *
 * @brief This widget aims to have a more expansive list than the simple list
 *        in Elementary that could have more flexible items and allow many more
 *        entries while still being fast and low on memory usage.
 *
 * At the same time it was also made to be able to do tree structures. But the
 * price to pay is based on complexity when it comes to usage. If all you want
 * is a simple list with icons and a single text, use the normal @ref List
 * object.
 *
 * Genlist has a fairly large API, mostly because it's relatively complex,
 * trying to be expansive, powerful, and efficient. First we begin with
 * an overview on the theory behind genlist.
 *
 * This widget inherits from the @ref Layout one, so that all the
 * functions acting on it also work for genlist objects.
 *
 * This widget implements the elm-scrollable-interface
 * interface, so that all (non-deprecated) functions for the base @ref
 * Scroller widget also work for genlists.
 *
 * Some calls on the genlist's API are marked as @b deprecated, as
 * they just wrap the scrollable widgets counterpart functions. Use
 * the ones mentioned for each case of deprecation here.
 * Eventually the deprecated ones are discarded (next
 * major release).
 *
 * @section Genlist_Item_Class Genlist item classes - creating items
 *
 * In order to have the ability to add and delete items on the fly, genlist
 * implements a class (callback) system where the application provides a
 * structure with information about that type of item (genlist may contain
 * multiple items of different types with different classes, states, and styles).
 * Genlist calls the functions in this struct (methods) when an item is
 * "realized" (i.e., created dynamically, while the user is scrolling the
 * list). All objects are simply deleted when they are no longer needed by
 * evas_object_del(). The #Elm_Genlist_Item_Class structure contains the
 * following members:
 * - @c item_style - This is a constant string and simply defines the name
 *   of the item style. It @b must be specified and the default should be @c
 *   "default".
 * @internal
 * - @c decorate_item_style - This is a constant string and simply defines the name
 *   of the decorate mode item style. It is used to specify the decorate mode item style. It can be
 *   used when elm_genlist_item_decorate_mode_set() is called.
 * - @c decorate_all_item_style - This is a constant string and simply defines the name
 *   of the decorate all item style. It is used to specify the decorate all item style. It can be
 *   used to set the selection, checking, and deletion mode. This is used when
 *   elm_genlist_decorate_mode_set() is called.
 * @endinternal
 * - @c func - This is a struct with pointers to functions that are called when
 *   an item is going to actually be created. All of them receive a @c data
 *   parameter that points to the same data that is passed to
 *   elm_genlist_item_append() and other related item creation functions, and an @c
 *   obj parameter that points to the genlist object itself.
 *
 * The function pointers inside @c func are @c text_get, @c content_get, @c
 * state_get, and @c del. The first three functions also receive a @c part
 * parameter described below. A brief description of these functions is as follows:
 *
 * - @c text_get - The @c part parameter is the name string of one of the
 *   existing text parts in the Edje group implementing the item's theme.
 *   This function @b must return a strdup'()ed string, as the caller is going to
 *   free() it when done. See #Elm_Genlist_Item_Text_Get_Cb.
 * - @c content_get - The @c part parameter is the name string of one of the
 *   existing (content) swallow parts in the Edje group implementing the item's
 *   theme. It must return @c NULL, when no content is desired, or a valid
 *   object handle, otherwise.  The object is deleted by the genlist on
 *   its deletion or when the item is "unrealized".
 *   See #Elm_Genlist_Item_Content_Get_Cb.
 * - @c func.state_get - The @c part parameter is the name string of one of
 *   the state parts in the Edje group implementing the item's theme. It must return
 *   @c EINA_FALSE for false/off or @c EINA_TRUE for true/on. Genlists
 *   emit a signal to its theming Edje object with @c "elm,state,xxx,active"
 *   and @c "elm" as "emission" and "source" arguments, respectively, when
 *   the state is @c true (the default is false), where @c xxx is the name of
 *   the (state) part.  See #Elm_Genlist_Item_State_Get_Cb.
 * - @c func.del - This is intended for use when genlist items are deleted,
 *   so any data attached to the item (e.g. its data parameter on creation)
 *   can be deleted. See #Elm_Genlist_Item_Del_Cb.
 *
 * The available item styles are as follows:
 * - default
 * - default_style - The text part is a textblock
 * - double_label
 * - icon_top_text_bottom
 * - group_index
 * - one_icon - Only 1 icon (left) @since 1.7
 * - end_icon - Only 1 icon (at end/right) @since 1.7
 * - no_icon - No icon (at end/right) @since 1.7
 *
 * @section Genlist_Items Structure of items
 *
 * An item in a genlist can have @c 0 or more texts (they can be regular
 * text or textblock Evas objects - that's up to the style to determine), @c 0
 * or more blocks of content (which are simply objects swallowed into the genlist item's
 * theming Edje object) and @c 0 or more <b>boolean states</b>, which have the
 * behavior left to the user to define. The Edje part names for each of
 * these properties are looked up, in the theme file for the genlist,
 * under the Edje (string) data items named @c "labels", @c "contents", and @c
 * "states", respectively. For each of these properties, if more than one
 * part is provided, they must have names listed and separated by spaces in the
 * data fields. For the default genlist item theme, we have @b one text
 * part (@c "elm.text"), @b two content parts (@c "elm.swalllow.icon" and @c
 * "elm.swallow.end") and @b no state parts.
 *
 * A genlist item may be having one of the several styles. Elementary provides one
 * by default - "default", but this can be extended by system or application
 * custom themes/overlays/extensions (see @ref Theme "themes" for more
 * details).
 *
 * @section Genlist_Manipulation Editing and Navigating
 *
 * Items can be added by several calls. All of them return a @ref
 * Elm_Object_Item handle that is an internal member inside the genlist.
 * They all take a data parameter that is meant to be used as a handle for
 * the application's internal data (eg. the struct with the original item
 * data). The parent parameter is the parent genlist item this belongs to if
 * it is a tree or an indexed group, and this value is @c NULL if there is no parent. The
 * flags can be a bitmask of #ELM_GENLIST_ITEM_NONE, #ELM_GENLIST_ITEM_TREE,
 * and #ELM_GENLIST_ITEM_GROUP. If #ELM_GENLIST_ITEM_TREE is set then this
 * item is displayed as an item that is able to expand and have child items.
 * If #ELM_GENLIST_ITEM_GROUP is set then this item is a group index item that
 * is displayed at the top until the next group comes. The @a func parameter is
 * a convenience callback that is called when the item is selected and the
 * @a data parameter is the @a func_data parameter, @a obj is the genlist
 * object, and @a event_info is the genlist item.
 *
 * elm_genlist_item_append() adds an item to the end of the list, or if
 * there is a parent, it adds the item to the end of all the child items of the parent.
 * elm_genlist_item_prepend() is the same but adds an item to the beginning of
 * the list or children list. elm_genlist_item_insert_before() inserts at
 * item before another item and elm_genlist_item_insert_after() inserts an item after
 * the indicated item.
 *
 * The application can clear the list with elm_genlist_clear() which deletes
 * all the items in the list. elm_object_item_del() deletes a specific
 * item. elm_genlist_item_subitems_clear() clears all items that are
 * children of the indicated parent item.
 *
 * To help inspect list items you can jump to the item at the top of the list
 * with elm_genlist_first_item_get() which returns the item pointer. Similarly,
 * elm_genlist_last_item_get() gets the item at the end of the list.
 * elm_genlist_item_next_get() and elm_genlist_item_prev_get() get the next
 * and previous items respectively relative to the indicated item. Using
 * these calls you can walk through the entire item list/tree. Note that as a tree
 * the items are flattened in the list, so elm_genlist_item_parent_get()
 * lets you know which item is the parent (and thus helps you skip them if
 * needed).
 *
 * @section Genlist_Multi_Selection Multi-selection
 *
 * If the application wants to allow multiple items to be selected,
 * elm_genlist_multi_select_set() can enable this. If the list is
 * single-selection only (the default), then elm_genlist_selected_item_get()
 * returns the selected item, if any, or @c NULL if none is selected. If the
 * list is multi-select then elm_genlist_selected_items_get() returns a
 * list (that is only valid as long as no items are modified (added, deleted,
 * selected, or unselected)).
 *
 * @section Genlist_Usage_Hints Usage hints
 *
 * There are also convenience functions. elm_object_item_widget_get()
 * returns the genlist object the item belongs to. elm_genlist_item_show()
 * makes the scroller scroll to show that specific item so that it is visible.
 * elm_object_item_data_get() returns the data pointer set by the item
 * creation functions.
 *
 * If an item changes (state of boolean changes, text or content changes),
 * then use elm_genlist_item_update() to have genlist update the item with
 * the new state. Genlist re-realizes the item and thus calls the functions
 * in the _Elm_Genlist_Item_Class for that item.
 *
 * To programmatically (un)select an item use elm_genlist_item_selected_set().
 * To get its selected state use elm_genlist_item_selected_get(). Similarly,
 * to expand/contract an item and get its expanded state, use
 * elm_genlist_item_expanded_set() and elm_genlist_item_expanded_get(). And
 * again to disable an item (unable to be selected and appear
 * differently) use elm_object_item_disabled_set() to set this and
 * elm_object_item_disabled_get() to get the disabled state.
 *
 * In general, to indicate how the genlist should expand items horizontally to
 * fill the list area, use elm_genlist_mode_set(). Valid modes are
 * ELM_LIST_LIMIT, ELM_LIST_COMPRESS, and ELM_LIST_SCROLL. The default is
 * ELM_LIST_SCROLL. This mode means that if items are too wide to fit, the
 * scroller scrolls horizontally. Otherwise items are expanded to
 * fill the width of the viewport of the scroller. If it is
 * ELM_LIST_LIMIT, items are expanded to the viewport width
 * if the viewport width is larger than the item, but the genlist widget width is
 * limited to the largest item. Do not use the ELM_LIST_LIMIT mode with the homogenous
 * mode turned on. ELM_LIST_COMPRESS can be combined with a different style
 * that uses the edjes' ellipsis feature (cutting text off like this: "tex...").
 *
 * Items call their selection func and callback only once when selected for the
 * first time. Any further clicks do nothing, unless you enable always
 * select with elm_genlist_select_mode_set() as ELM_OBJECT_SELECT_MODE_ALWAYS.
 * This means even if selected, every click make the selected callbacks
 * to be called. elm_genlist_select_mode_set() as ELM_OBJECT_SELECT_MODE_NONE
 * turns off the ability to select items entirely and they neither
 * appear selected nor call selected callback functions.
 *
 * Remember that you can create new styles and add your own theme augmentation
 * for each application with elm_theme_extension_add(). If you absolutely must
 * have a specific style that overrides any theme that the user or system sets up,
 * you can use elm_theme_overlay_add() to add such a file.
 *
 * @section Genlist_Implementation Implementation
 *
 * Evas tracks every object you create. Every time it processes an event
 * (mouse move, down, up etc.) it needs to walk through objects and find out
 * what event they affect. Further, every time it renders display updates,
 * in order to just calculate what to re-draw, it needs to walk through a large
 * number of objects. Thus, the more objects you keep active, the more
 * overhead Evas has in just doing its work. It is advisable to keep your
 * active objects to the minimum working set you need. Also remember that
 * object creation and deletion carries an overhead, so there is a
 * middle-ground, which is not easily determined. But don't keep massive lists
 * of objects you can't see or use. Genlist does this with list objects. It
 * creates and destroys them dynamically as you scroll around. It groups them
 * into blocks so that it can determine the visibility of a whole block at
 * once as opposed to having to walk through the whole list. This 2-level list allows
 * for very large numbers of items to be in the list (tests have used upto
 * 2,000,000 items). Also genlist employs a queue for adding items. As items
 * maybe of different sizes, every added item needs to be calculated as per its
 * size and thus this presents a lot of overhead on populating the list, this
 * genlist employs a queue. Every added item is queued and spooled off over
 * time, though it appears some time later. So if your list has many members,
 * you may find that it takes a while for them to appear and this process
 * consumes a lot of CPU time while it is busy spooling.
 *
 * Genlist also implements a tree structure for items, but it does so with
 * callbacks to the application, with the application filling in tree
 * structures when requested (allowing for efficient building of a very
 * deep tree that could even be used for file-management).
 * See the above smart signal callbacks for details.
 *
 * @section Genlist_Smart_Events Genlist smart events
 *
 * This widget emits the following signals, besides the ones sent from
 * @ref Layout :
 * - @c "activated" - The user has double-clicked or pressed
 *   (enter|return|spacebar) on an item. The @a event_info parameter is the
 *   item that is activated.
 * - @c "pressed" - The user pressed an item. The @a event_info
 *   parameter is the item that is pressed.
 * - @c "released" - The user released an item. The @a event_info
 *   parameter is the item that is released.
 * - @c "clicked,double" - The user has double-clicked an item.  The @a
 *   event_info parameter is the item that is double-clicked.
 * - @c "selected" - This is called when a user has selected an item.
 *   The @a event_info parameter is the genlist item that is selected.
 * - @c "unselected" - This is called when a user has unselected an item.
 *	 The @a event_info parameter is the genlist item that is
 *   unselected.
 * - @c "expanded" - This is called when elm_genlist_item_expanded_set() is
 *   called and the item is now meant to be expanded. The @a event_info
 *   parameter is the genlist item that is indicated to expand. It is the
 *   job of this callback to then fill in the child items.
 * - @c "contracted" - This is called when elm_genlist_item_expanded_set() is
 *   called and the item is now meant to contract. The @a event_info
 *   parameter is the genlist item that is indicated to contract. It is the
 *   job of this callback to then delete the child items.
 * - @c "expand,request" - This is called when a user has indicated that they want
 *   to expand a tree branch item. The callback should decide if the item can
 *   expand (has any children) and then call elm_genlist_item_expanded_set()
 *   appropriately to set the state. The @a event_info parameter is the genlist
 *   item that is indicated to expand.
 * - @c "contract,request" - This is called when a user has indicated that they
 *   want to contract a tree branch item. The callback should decide if the
 *   item can contract (has any children) and then call
 *   elm_genlist_item_expanded_set() appropriately to set the state. The
 *   event_info parameter is the genlist item that is indicated to contract.
 * - @c "realized" - This is called when the item in the list is created as a
 *   real evas object. @a event_info parameter is the genlist item that is
 *   created.
 * - @c "unrealized" - This is called just before an item is unrealized.
 *   After this call, the provided content objects are deleted and the item
 *   object itself is deleted or is put into a floating cache.
 * - @c "drag,start,up" - This is called when the item in the list has been
 *   dragged (not scrolled) up.
 * - @c "drag,start,down" - This is called when the item in the list has been
 *   dragged (not scrolled) down.
 * - @c "drag,start,left" - This is called when the item in the list has been
 *   dragged (not scrolled) left.
 * - @c "drag,start,right" - This is called when the item in the list has
 *   been dragged (not scrolled) right.
 * - @c "drag,stop" - This is called when the item in the list is stopped
 *   being dragged.
 * - @c "drag" - This is called when the item in the list is being dragged.
 * - @c "longpressed" - This is called when the item is pressed for a certain
 *   amount of time. By default it's @c 1 second. The @a event_info parameter is the
 *   longpressed genlist item.
 * - @c "scroll,anim,start" - This is called when scrolling animation has
 *   started.
 * - @c "scroll,anim,stop" - This is called when scrolling animation has
 *   stopped.
 * - @c "scroll,drag,start" - This is called when dragging the content has
 *   started.
 * - @c "scroll,drag,stop" - This is called when dragging the content has
 *   stopped.
 * - @c "edge,top" - This is called when the genlist is scrolled until
 *   the top edge.
 * - @c "edge,bottom" - This is called when the genlist is scrolled
 *   until the bottom edge.
 * - @c "edge,left" - This is called when the genlist is scrolled
 *   until the left edge.
 * - @c "edge,right" - This is called when the genlist is scrolled
 *   until the right edge.
 * - @c "multi,swipe,left" - This is called when the genlist is multi-touch
 *   swiped left.
 * - @c "multi,swipe,right" - This is called when the genlist is multi-touch
 *   swiped right.
 * - @c "multi,swipe,up" - This is called when the genlist is multi-touch
 *   swiped up.
 * - @c "multi,swipe,down" - This is called when the genlist is multi-touch
 *   swiped down.
 * - @c "multi,pinch,out" - This is called when the genlist is multi-touch
 *   pinched out.
 * - @c "multi,pinch,in" - This is called when the genlist is multi-touch
 *   pinched in.
 * - @c "swipe" - This is called when the genlist is swiped.
 * - @c "moved" - This is called when a genlist item is moved in the reorder mode.
 * - @c "moved,after" - This is called when a genlist item is moved after
 *   another item in the reorder mode. The @a event_info parameter is the reordered
 *   item. To get the relative previous item, use elm_genlist_item_prev_get().
 *   This signal is called along with the "moved" signal.
 * - @c "moved,before" - This is called when a genlist item is moved before
 *   another item in the reorder mode. The @a event_info parameter is the reordered
 *   item. To get the relative previous item, use elm_genlist_item_next_get().
 *   This signal is called along with the "moved" signal.
 * - @c "language,changed" - This is called when the program's language is
 *   changed. Call elm_genlist_realized_items_update() if the item's text should
 *   be translated.
 * - @c "tree,effect,finished" - This is called when the genlist tree effect is finished.
 * - @c "highlighted" - This is called when an item in the list is pressed and highlighted.
 *   The @a event_info parameter is the item that is highlighted.
 * - @c "unhighlighted" - This is called when an item in the list is unpressed and unhighlighted.
 *   The @a event_info parameter is the item that is unhighlighted.
 *
 *
 * Supported common elm_object_item APIs.
 * @li @ref elm_object_item_part_content_get()
 * @li @ref elm_object_item_part_text_get()
 * @li @ref elm_object_item_disabled_set()
 * @li @ref elm_object_item_disabled_get()
 * @li @ref elm_object_item_signal_emit()
 *
 * Unsupported common elm_object_item APIs as per the genlist concept.
 * Genlist fills content/text according to the appropriate callback functions.
 * Use elm_genlist_item_update() or elm_genlist_item_fields_update()
 * instead.
 * @li @ref elm_object_item_part_content_set()
 * @li @ref elm_object_item_part_content_unset()
 * @li @ref elm_object_item_part_text_set()
 *
 * @{
 */

/**
 * @brief Enumeration that defines whether the item is of a special type (has subitems or it's the
 * index of a group), or it is just a simple item.
 */
/**
 * @brief Enumeration that defines the type of the item field.
 * @remarks It is used while updating the item field.
 * @remarks It can be used for updating multi fields.
 */

/**
 * @brief Adds a new genlist widget to the given parent Elementary
 *        (container) object.
 *
 * @details This function inserts a new genlist widget on the canvas.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] parent The parent object
 * @return A new genlist widget handle, otherwise @c NULL in case of an error
 *
 * @see elm_genlist_item_append()
 * @see elm_object_item_del()
 * @see elm_genlist_clear()
 */
EAPI Evas_Object                  *elm_genlist_add(Evas_Object *parent);

/**
 * @brief Removes all items from a given genlist widget.
 *
 * @details This removes (and deletes) all items in @a obj, making it empty.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 *
 * @see elm_object_item_del() to remove just one item.
 */
EAPI void                          elm_genlist_clear(Evas_Object *obj);

/**
 * @brief Enables or disables multi-selection in the genlist.
 *
 * @details This enables (@c EINA_TRUE) or disables (@c EINA_FALSE) multi-selection in
 *          the list. This allows more than @c 1 item to be selected. To retrieve the list
 *          of selected items, use elm_genlist_selected_items_get().
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] multi The boolean value that enables or disables multi-selection \n
 *              Default is disabled.
 *
 * @see elm_genlist_selected_items_get()
 * @see elm_genlist_multi_select_get()
 */
EAPI void                          elm_genlist_multi_select_set(Evas_Object *obj, Eina_Bool multi);

/**
 * @brief Gets whether multi-selection in genlist is enabled or disabled.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The boolean value that indicates whether multi-selection is enabled or disabled
 *         (@c EINA_TRUE = enabled/@c EINA_FALSE = disabled). Default is @c EINA_FALSE.
 *
 * @see elm_genlist_multi_select_set()
 */
EAPI Eina_Bool                     elm_genlist_multi_select_get(const Evas_Object *obj);

/**
 * @brief Sets the horizontal stretching mode.
 *
 * @details This sets the mode used for sizing items horizontally. Valid modes
 *          are #ELM_LIST_LIMIT, #ELM_LIST_SCROLL, and #ELM_LIST_COMPRESS. The default is
 *          ELM_LIST_SCROLL. This mode means that if items are too wide to fit,
 *          the scroller scrolls horizontally. Otherwise items are expanded
 *          to fill the width of the viewport of the scroller. If it is
 *          ELM_LIST_LIMIT, items are expanded to the viewport width and
 *          limited to that size. If it is ELM_LIST_COMPRESS, the item width is
 *          fixed (restricted to a minimum of) to the list width when calculating its
 *          size in order to allow the height to be calculated based on it. This allows,
 *          for instance, a text block to wrap lines if the Edje part is configured with
 *          "text.min: 0 1".
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks ELM_LIST_COMPRESS makes list resize slower as it
 *          recalculates every item height again whenever the list width
 *          changes
 * @remarks The homogeneous mode is so that all items in the genlist are of the same
 *          width/height. With ELM_LIST_COMPRESS, genlist items are initialized fast.
 *          However, there are no sub-objects in the genlist which can be
 *          on the flying resizable (such as TEXTBLOCK). If so, then some dynamic
 *          resizable objects in the genlist would not be diplayed properly.
 *
 * @param[in] obj The genlist object
 * @param[in] mode The mode to use (either #ELM_LIST_SCROLL or #ELM_LIST_LIMIT)
 *
 * @see elm_genlist_mode_get()
 */
EAPI void                          elm_genlist_mode_set(Evas_Object *obj, Elm_List_Mode mode);

/**
 * @brief Gets the horizontal stretching mode.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The mode to use
 * (#ELM_LIST_LIMIT, #ELM_LIST_SCROLL)
 *
 * @see elm_genlist_mode_set()
 */
EAPI Elm_List_Mode                 elm_genlist_mode_get(const Evas_Object *obj);

/**
 * @internal
 *
 * @brief Enables or disables horizontal and vertical bouncing effect.
 *
 * @details This enables or disables the scroller bouncing effect for the
 *          genlist. See elm_scroller_bounce_set() for details.
 *
 * @param obj The genlist object
 * @param h_bounce The boolean value that allows horizontal bouncing (@c EINA_TRUE = on, @c
 *                 EINA_FALSE = off) \n
 *                 Default is @c EINA_FALSE.
 * @param v_bounce The boolean value that allows vertical bouncing (@c EINA_TRUE = on, @c
 *                 EINA_FALSE = off) \n
 *                 Default is @c EINA_TRUE.
 *
 * @deprecated Use elm_scroller_bounce_set() instead.
 *
 * @see elm_scroller_bounce_set()
 * @see elm_genlist_bounce_get()
 */
EINA_DEPRECATED EAPI void          elm_genlist_bounce_set(Evas_Object *obj, Eina_Bool h_bounce, Eina_Bool v_bounce);

/**
 * @internal
 *
 * @brief Gets whether the horizontal and vertical bouncing effect is enabled.
 *
 * @param obj The genlist object
 * @param h_bounce The pointer to a bool that indicates if horizontal bouncing is set
 * @param v_bounce The pointer to a bool that indicates if vertical bouncing is set
 *
 * @deprecated Use elm_scroller_bounce_get() instead.
 *
 * @see elm_scroller_bounce_get()
 * @see elm_genlist_bounce_set()
 */
EINA_DEPRECATED EAPI void          elm_genlist_bounce_get(const Evas_Object *obj, Eina_Bool *h_bounce, Eina_Bool *v_bounce);

/**
 * @brief Appends a new item to a given genlist widget.
 *
 * @details This adds the given item to the end of the list or the end of
 *          the children list if the @a parent is given.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] itc The item class for the item
 * @param[in] data The item data
 * @param[in] parent The parent item, otherwise @c NULL if there is no parent item
 * @param[in] type The item type
 * @param[in] func The convenience function that is called when the item is selected
 * @param[in] func_data The data passed to @a func mentioned above
 * @return A handle to the added item, otherwise @c NULL if it is not possible
 *
 * @see elm_genlist_item_prepend()
 * @see elm_genlist_item_insert_before()
 * @see elm_genlist_item_insert_after()
 * @see elm_object_item_del()
 */
EAPI Elm_Object_Item             *elm_genlist_item_append(Evas_Object *obj, const Elm_Genlist_Item_Class *itc, const void *data, Elm_Object_Item *parent, Elm_Genlist_Item_Type type, Evas_Smart_Cb func, const void *func_data);

/**
 * @brief Prepends a new item to a given genlist widget.
 *
 * @details This adds an item to the beginning of the list or beginning of the
 *          children of the parent if given.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] itc The item class for the item
 * @param[in] data The item data
 * @param[in] parent The parent item, otherwise @c NULL if there is no parent item
 * @param[in] type The item type
 * @param[in] func The convenience function that is called when the item is selected
 * @param[in] func_data The data passed to @a func mentioned above
 * @return A handle to the added item, otherwise @c NULL if it is not possible
 *
 * @see elm_genlist_item_append()
 * @see elm_genlist_item_insert_before()
 * @see elm_genlist_item_insert_after()
 * @see elm_object_item_del()
 */
EAPI Elm_Object_Item             *elm_genlist_item_prepend(Evas_Object *obj, const Elm_Genlist_Item_Class *itc, const void *data, Elm_Object_Item *parent, Elm_Genlist_Item_Type type, Evas_Smart_Cb func, const void *func_data);

/**
 * @brief Inserts an item before another in a genlist widget.
 *
 * @details This inserts an item before another in the list. It is the
 *          same tree level or group as the item before which it is inserted.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] itc The item class for the item
 * @param[in] data The item data
 * @param[in] parent The parent item, otherwise @c NULL if there is no parent item
 * @param[in] before The item before which to place this new one
 * @param[in] type The item type
 * @param[in] func The convenience function that is called when the item is selected
 * @param[in] func_data The data passed to @a func mentioned above
 * @return A handle to the item added, otherwise @c NULL if it is not possible
 *
 * @see elm_genlist_item_append()
 * @see elm_genlist_item_prepend()
 * @see elm_genlist_item_insert_after()
 * @see elm_object_item_del()
 */
EAPI Elm_Object_Item             *elm_genlist_item_insert_before(Evas_Object *obj, const Elm_Genlist_Item_Class *itc, const void *data, Elm_Object_Item *parent, Elm_Object_Item *before, Elm_Genlist_Item_Type type, Evas_Smart_Cb func, const void *func_data);

/**
 * @brief Inserts an item after another in a genlist widget.
 *
 * @details This inserts an item after another in the list. It is in the
 *          same tree level or group as the item after which it is inserted.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] itc The item class for the item
 * @param[in] data The item data
 * @param[in] parent The parent item, otherwise @c NULL if there is no parent item
 * @param[in] after The item after which to place this new one
 * @param[in] type The item type
 * @param[in] func The convenience function that is called when the item is selected
 * @param[in] func_data The data passed to @a func mentioned above
 * @return A handle to the item added, otherwise @c NULL if it is not possible
 *
 * @see elm_genlist_item_append()
 * @see elm_genlist_item_prepend()
 * @see elm_genlist_item_insert_before()
 * @see elm_object_item_del()
 */
EAPI Elm_Object_Item             *elm_genlist_item_insert_after(Evas_Object *obj, const Elm_Genlist_Item_Class *itc, const void *data, Elm_Object_Item *parent, Elm_Object_Item *after, Elm_Genlist_Item_Type type, Evas_Smart_Cb func, const void *func_data);

/**
 * @brief Inserts a new item into the sorted genlist object.
 *
 * @details This inserts an item in the genlist based on a user defined comparison
 *          function. The two arguments passed to the function @a func are genlist item
 *          handles to compare.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] itc The item class for the item
 * @param[in] data The item data
 * @param[in] parent The parent item, otherwise @c NULL if there is no parent item
 * @param[in] type The item type
 * @param[in] comp The function called for sorting
 * @param[in] func The convenience function that is called when the item is selected
 * @param[in] func_data The data passed to @a func mentioned above
 * @return A handle to the item added, otherwise @c NULL if it is not possible
 *
 * @see elm_genlist_item_append()
 * @see elm_genlist_item_prepend()
 * @see elm_genlist_item_insert_after()
 * @see elm_object_item_del()
 */
EAPI Elm_Object_Item             *elm_genlist_item_sorted_insert(Evas_Object *obj, const Elm_Genlist_Item_Class *itc, const void *data, Elm_Object_Item *parent, Elm_Genlist_Item_Type type, Eina_Compare_Cb comp, Evas_Smart_Cb func, const void *func_data);

/* Operations to retrieve existing items */
/**
 * @brief Gets the selected item in the genlist.
 *
 * @details This gets the selected item in the list (if multi-selection is enabled, only
 *          the item that is first selected in the list is returned, which is not very
 *          useful, so see elm_genlist_selected_items_get() to know when multi-selection is
 *          used).
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks If no item is selected, @c NULL is returned.
 *
 * @param[in] obj The genlist object
 * @return The selected item, otherwise @c NULL if none are selected
 *
 * @see elm_genlist_selected_items_get()
 */
EAPI Elm_Object_Item             *elm_genlist_selected_item_get(const Evas_Object *obj);

/**
 * @brief Gets a list of selected items in the genlist.
 *
 * @details It returns a list of selected items. This list pointer is only valid as
 *          long as the selection doesn't change (no items are selected or unselected, or
 *          unselected implicitly by deletion). The list contains genlist item
 *          pointers. The order of the items in this list is the order in which they were
 *          selected, i.e. the first item in this list is the first item that is
 *          selected, and so on.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks If not in the multi-select mode, use
 *          elm_genlist_selected_item_get() instead.
 *
 * @param[in] obj The genlist object
 * @return The list of selected items, otherwise @c NULL if none are selected
 *
 * @see elm_genlist_multi_select_set()
 * @see elm_genlist_selected_item_get()
 */
EAPI Eina_List              *elm_genlist_selected_items_get(const Evas_Object *obj);

/**
 * @brief Gets a list of realized items in the genlist.
 *
 * @details This returns a list of realized items in the genlist. The list
 *          contains genlist item pointers. The list must be freed by the
 *          caller when done with eina_list_free(). The item pointers in the
 *          list are only valid as long as those items are not deleted or the
 *          genlist is not deleted.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The list of realized items, otherwise @c NULL if none are realized
 *
 * @see elm_genlist_realized_items_update()
 */
EAPI Eina_List                    *elm_genlist_realized_items_get(const Evas_Object *obj);

/**
 * @brief Gets the first item in the genlist.
 *
 * @details This returns the first item in the list.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The first item, otherwise @c NULL if there are no items
 */
EAPI Elm_Object_Item             *elm_genlist_first_item_get(const Evas_Object *obj);

/**
 * @brief Gets the last item in the genlist.
 *
 * @details This returns the last item in the list.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The last item, otherwise @c NULL if there are no items
 */
EAPI Elm_Object_Item             *elm_genlist_last_item_get(const Evas_Object *obj);

/**
 * @internal
 *
 * @brief Sets the scrollbar policy.
 *
 * @details This sets the scrollbar visibility policy for the given genlist
 *          scroller. #ELM_SCROLLER_POLICY_AUTO means the scrollbar is
 *          made visible if it is needed, and otherwise kept hidden. #ELM_SCROLLER_POLICY_ON
 *          turns it on at all times, and #ELM_SCROLLER_POLICY_OFF always keeps it off.
 *          This applies for the horizontal and vertical scrollbars respectively.
 *          The default is #ELM_SCROLLER_POLICY_AUTO.
 *
 * @param obj The genlist object
 * @param policy_h The horizontal scrollbar policy
 * @param policy_v The vertical scrollbar policy
 *
 * @deprecated Use elm_scroller_policy_set() instead.
 *
 * @see elm_scroller_policy_set()
 */
EINA_DEPRECATED EAPI void          elm_genlist_scroller_policy_set(Evas_Object *obj, Elm_Scroller_Policy policy_h, Elm_Scroller_Policy policy_v);

/**
 * @internal
 *
 * @brief Gets the scrollbar policy.
 *
 * @param obj The genlist object
 * @param policy_h The pointer to store the horizontal scrollbar policy
 * @param policy_v The pointer to store the vertical scrollbar policy
 *
 * @deprecated Use elm_scroller_policy_get() instead.
 *
 * @see elm_scroller_policy_get()
 */
EINA_DEPRECATED EAPI void          elm_genlist_scroller_policy_get(const Evas_Object *obj, Elm_Scroller_Policy *policy_h, Elm_Scroller_Policy *policy_v);

/**
 * @brief Updates the content of all the realized items.
 *
 * @details This updates all the realized items by calling all the item class functions again
 *          to get the content, text and states. Use this when the original
 *          item data has changed and the changes are desired to reflect.
 *
 *          To update just one item, use elm_genlist_item_update().
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 *
 * @see elm_genlist_realized_items_get()
 * @see elm_genlist_item_update()
 */
EAPI void                          elm_genlist_realized_items_update(Evas_Object *obj);

/**
 * @brief Returns the number of items that are currently in a list.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks This behavior is O(1) and includes items which may or may not be realized.
 *
 * @param[in] obj The list
 * @return The total number of items in the list
 */
EAPI unsigned int elm_genlist_items_count(const Evas_Object *obj);

/**
 * @brief Enables or disables the homogeneous mode.
 *
 * @details This enables the homogeneous mode where items are of the same
 *          height and width so that genlist may perform lazy-loading at its
 *          maximum (which increases the performance for scrolling the list).
 *          In the normal mode, genlist pre-calculates all the items' sizes even
 *          though they are not in use. So items' callbacks are called for more times than
 *          expected. But the homogeneous mode skips the item size pre-calculation
 *          process so items' callbacks are called only when the item is needed.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks This also works well with group index.
 *
 * @param[in] obj The genlist object
 * @param[in] homogeneous The boolean value assuming that the items within the genlist are of the
 *                    same height and width (@c EINA_TRUE = on, @c EINA_FALSE = off) \n
 *                    Default is @c EINA_FALSE.
 *
 * @see elm_genlist_mode_set()
 * @see elm_genlist_homogeneous_get()
 */
EAPI void                          elm_genlist_homogeneous_set(Evas_Object *obj, Eina_Bool homogeneous);

/**
 * @brief Gets whether the homogeneous mode is enabled.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The boolean value assuming that the items within the genlist are of the same height
 *         and width (@c EINA_TRUE = on, @c EINA_FALSE = off)
 *
 * @see elm_genlist_homogeneous_set()
 */
EAPI Eina_Bool                     elm_genlist_homogeneous_get(const Evas_Object *obj);

/**
 * @brief Sets the maximum number of items within an item block.
 *
 * @details This configures the block count to tune the target with, for a particular
 *          performance matrix.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks A block of objects are used to reduce the number of operations occurring due to
 *          large number of objects on the screen. It can determine the visibility, or if the
 *          object has changed, its theme needs to be updated by doing this kind of
 *          calculation to the entire block, instead of every object.
 *
 * @remarks The default value for the block count is enough for most lists, so unless
 *          your sure that you have a lot of objects visible on the screen at the same
 *          time, don't try to change this.
 *
 * @param[in] obj The genlist object
 * @param[in] count The maximum number of items within an item block \n
 *              Default is @c 32.
 *
 * @see elm_genlist_block_count_get()
 * @see @ref Genlist_Implementation
 */
EAPI void                          elm_genlist_block_count_set(Evas_Object *obj, int count);

/**
 * @brief Gets the maximum number of items within an item block.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The maximum number of items within an item block
 *
 * @see elm_genlist_block_count_set()
 */
EAPI int                           elm_genlist_block_count_get(const Evas_Object *obj);

/**
 * @brief Sets the timeout in seconds for the longpress event.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks This option changes the time it takes to send an event @c "longpressed"
 *          after the mouse down signal is sent to the list. If this event occurs, no
 *          @c "clicked" event is sent.
 *
 * @remarks If you set the longpress timeout value with this API, your genlist
 *          is not affected by the longpress value of the elementary config value
 *          later.
 *
 * @param[in] obj The genlist object
 * @param[in] timeout The timeout in seconds \n
 *                The default value is elm config value(1.0).
 *
 * @see elm_genlist_longpress_timeout_set()
 */
EAPI void                          elm_genlist_longpress_timeout_set(Evas_Object *obj, double timeout);

/**
 * @brief Gets the timeout in seconds for the longpress event.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The timeout in seconds
 *
 * @see elm_genlist_longpress_timeout_get()
 */
EAPI double                        elm_genlist_longpress_timeout_get(const Evas_Object *obj);

/**
 * @brief Gets the item that is at the x, y canvas coordinates.
 *
 * @details This returns the item at the given coordinates (which are canvas
 *          relative, not object-relative). If an item is at that coordinate,
 *          that item handle is returned, and if @a posret is not @c NULL, the
 *          integer it is pointing to is set to either @c -1, @c 0, or @c 1, depending on whether
 *          the coordinate is on the upper portion of that item (-1), in the
 *          middle section (0), or on the lower part (1). If @c NULL is returned as
 *          an item (no item found there), then posret may indicate @c -1 or @c 1
 *          depending on whether the coordinate is above or below the items in
 *          the genlist respectively.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] x The input x coordinate
 * @param[in] y The input y coordinate
 * @param[out] posret The position relative to the returned item
 * @return The item at the coordinates, otherwise @c NULL if there are none
 */
EAPI Elm_Object_Item             *elm_genlist_at_xy_item_get(const Evas_Object *obj, Evas_Coord x, Evas_Coord y, int *posret);

/**
 * @internal
 * @remarks Tizen no feature
 *
 * @brief Gets the active genlist mode item.
 *
 * @details This function returns the item that is activated with a mode, by the
 *          function elm_genlist_item_decorate_mode_set().
 *
 * @param[in] obj The genlist object
 * @return The active item for that current mode, otherwise @c NULL if no item is
 *         activated with a mode
 *
 * @see elm_genlist_item_decorate_mode_set()
 * @see elm_genlist_mode_get()
 */
EAPI Elm_Object_Item              *elm_genlist_decorated_item_get(const Evas_Object *obj);

/**
 * @brief Sets the reorder mode.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks After turning on the reorder mode, longpress on a normal item triggers
 *          reordering of the item. You can move the item up and down. However, reordering
 *          does not work with group items.
 *
 * @param[in] obj The genlist object
 * @param[in] reorder_mode The reorder mode
 * (@c EINA_TRUE = on, @c EINA_FALSE = off)
 */
EAPI void                          elm_genlist_reorder_mode_set(Evas_Object *obj, Eina_Bool reorder_mode);

/**
 * @brief Gets the reorder mode.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The reorder mode
 *         (@c EINA_TRUE = on, @c EINA_FALSE = off)
 */
EAPI Eina_Bool                     elm_genlist_reorder_mode_get(const Evas_Object *obj);

/**
 * @internal
 * @remarks Tizen no feature
 *
 * @brief Sets the genlist decorate mode.
 *
 * @details This sets the genlist decorate mode for all items.
 *
 * @param obj The genlist object
 * @param decorated The decorate mode status
 *                  (@c EINA_TRUE = decorate mode, @c EINA_FALSE = normal mode
 */
EAPI void               elm_genlist_decorate_mode_set(Evas_Object *obj, Eina_Bool decorated);

/**
 * @internal
 * @remarks Tizen no feature
 *
 * @brief Gets the genlist decorate mode.
 *
 * @param obj The genlist object
 * @return The decorate mode status
 *         (@c EINA_TRUE = decorate mode, @c EINA_FALSE = normal mode
 */
EAPI Eina_Bool          elm_genlist_decorate_mode_get(const Evas_Object *obj);

/**
 * @internal
 * @remarks Tizen no feature
 *
 * @brief Sets the genlist tree effect.
 *
 * @param obj The genlist object
 * @param enabled The tree effect status
 *                (@c EINA_TRUE = enabled, @c EINA_FALSE = disabled
 */
EAPI void               elm_genlist_tree_effect_enabled_set(Evas_Object *obj, Eina_Bool enabled);

/**
 * @internal
 * @remarks Tizen no feature
 *
 * @brief Gets the genlist tree effect.
 *
 * @param obj The genlist object
 * @return The tree effect status
 *         (@c EINA_TRUE = enabled, @c EINA_FALSE = disabled
 */
EAPI Eina_Bool          elm_genlist_tree_effect_enabled_get(const Evas_Object *obj);

/**
 * @brief Sets the genlist select mode.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks elm_genlist_select_mode_set() changes the item select mode in the genlist widget.
 *          - ELM_OBJECT_SELECT_MODE_DEFAULT : Items call their selection @a func and
 *            callback on first getting selected. Any further clicks
 *            do nothing, unless you set the always select mode.
 *          - ELM_OBJECT_SELECT_MODE_ALWAYS :  This means that, even if selected,
 *            every click calls the selected callbacks.
 *          - ELM_OBJECT_SELECT_MODE_NONE : This turns off the ability to select items
 *            entirely and they neither appear selected nor call selected
 *            callback functions.
 *
 * @param[in] obj The genlist object
 * @param[in] mode The select mode
 *
 * @see elm_genlist_select_mode_get()
 */
EAPI void elm_genlist_select_mode_set(Evas_Object *obj, Elm_Object_Select_Mode mode);

/**
 * @brief Gets the genlist select mode.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return The select mode
 *         (If getting the mode fails, it returns @c ELM_OBJECT_SELECT_MODE_MAX)
 *
 * @see elm_genlist_select_mode_set()
 */
EAPI Elm_Object_Select_Mode elm_genlist_select_mode_get(const Evas_Object *obj);

/**
 * @brief Sets whether the genlist items should be highlighted when an item is selected.
 *
 * @details This turns on/off the highlight effect when an item is selected and
 *          it gets or does not get highlighted. The selected and clicked
 *          callback functions are still called.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @remarks Highlight is enabled by default.
 *
 * @param[in] obj The genlist object
 * @param[in] highlight If @c EINA_TRUE highlighting is enabled,
 *                  otherwise @c EINA_FALSE to disable it
 *
 * @see elm_genlist_highlight_mode_get().
 */
EAPI void               elm_genlist_highlight_mode_set(Evas_Object *obj, Eina_Bool highlight);

/**
 * @brief Gets whether the genlist items should be highlighted when an item is selected.
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @return @c EINA_TRUE indicates that items can be highlighted,
 *         otherwise @c EINA_FALSE indicates that they can't \n
 *         If @a obj is @c NULL, @c EINA_FALSE is returned.
 *
 * @see elm_genlist_highlight_mode_set()
 */
EAPI Eina_Bool          elm_genlist_highlight_mode_get(const Evas_Object *obj);

/**
 * @brief Gets the nth item in a given genlist widget, placed at position @a nth, in
 *        its internal items list.
 *
 * @since 1.8
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 *
 * @param[in] obj The genlist object
 * @param[in] nth The number of the item to grab (@c 0 being the first)
 *
 * @return The item stored in @a obj at position @a nth, otherwise @c NULL if there is
 *         no item with that index (and on errors)
 */
EAPI Elm_Object_Item *
elm_genlist_nth_item_get(const Evas_Object *obj, unsigned int nth);

/**
 * @internal
 * @remarks Tizen only feature
 *
 * @brief Sets the genlist realization mode.
 *
 * @remarks By default, genlist disables the realization mode and genlists realize and
 *          unrealize some items when needed. If the realization mode is on,
 *          all items are realized when genlist is created and no items are unrealized.
 *          If this mode is on and the content size is changed, the item size
 *          changes accordingly. By default, genlist does not change item sizes
 *          eventhough they can be changed for performance reasons.
 *          This consumes more memory and decrease performance. So if the application
 *          appends many items, do not use the realization mode.
 *
 * @param obj The genlist object
 * @param mode The realization mode
 *             (@c EINA_TRUE = on, @c EINA_FALSE = off)
 */
EAPI void
elm_genlist_realization_mode_set(Evas_Object *obj, Eina_Bool mode);

/**
 * @internal
 * @remarks Tizen only feature
 *
 * @brief Gets the genlist realization mode.
 *
 * @param obj The genlist object
 * @return The realization mode
 *         (@c EINA_TRUE = on, @c EINA_FALSE = off)
 */
EAPI Eina_Bool
elm_genlist_realization_mode_get(Evas_Object *obj);

/**
 * @internal
 * @remarks Tizen only feature
 *
 * @brief Starts reordering for a specific item. it moves by move event.
 *
 * @param item The genlist item object
 */
EAPI void
elm_genlist_item_reorder_start(Elm_Object_Item *item);

/**
 * @internal
 * @remarks Tizen only feature
 *
 * @brief Stop reordering and relocate the item at touch released position.
 * @param item The genlist item object
 */
EAPI void
elm_genlist_item_reorder_stop(Elm_Object_Item *item);
/**
 * @}
 */

#include "elm_genlist_item.eo.legacy.h"
#include "elm_genlist.eo.legacy.h"
