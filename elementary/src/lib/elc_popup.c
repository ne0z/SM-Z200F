#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define ELM_INTERFACE_ATSPI_ACCESSIBLE_PROTECTED
#define ELM_INTERFACE_ATSPI_COMPONENT_PROTECTED
#define ELM_INTERFACE_ATSPI_WIDGET_ACTION_PROTECTED
#define ELM_WIDGET_ITEM_PROTECTED

#include <Elementary.h>
#include "elm_priv.h"
#include "elm_widget_popup.h"

#include "elm_popup_item.eo.h"
#include "ui_profiler.h"

#define MY_CLASS ELM_POPUP_CLASS

#define MY_CLASS_NAME "Elm_Popup"
#define MY_CLASS_NAME_LEGACY "elm_popup"

#define ELM_POPUP_ACTION_BUTTON_MAX 3

static void _button_remove(Evas_Object *, int, Eina_Bool);

static const char ACCESS_TITLE_PART[] = "access.title";
static const char ACCESS_BODY_PART[] = "access.body";
static const char CONTENT_PART[] = "elm.swallow.content";

static const char SIG_DISMISSED[] = "dismissed";
static const char SIG_SHOW_FINISHED[] = "show,finished";
static const char SIG_BLOCK_CLICKED[] = "block,clicked";
static const char SIG_TIMEOUT[] = "timeout";
static const char SIG_ITEM_FOCUSED[] = "item,focused";
static const char SIG_ITEM_UNFOCUSED[] = "item,unfocused";

static const Evas_Smart_Cb_Description _smart_callbacks[] = {
   {SIG_DISMISSED, ""},
   {SIG_SHOW_FINISHED, ""},
   {SIG_BLOCK_CLICKED, ""},
   {SIG_TIMEOUT, ""},
   {SIG_ITEM_FOCUSED, ""},
   {SIG_ITEM_UNFOCUSED, ""},
   {SIG_WIDGET_LANG_CHANGED, ""}, /**< handled by elm_widget */
   {SIG_WIDGET_ACCESS_CHANGED, ""}, /**< handled by elm_widget */
   {SIG_LAYOUT_FOCUSED, ""}, /**< handled by elm_layout */
   {SIG_LAYOUT_UNFOCUSED, ""}, /**< handled by elm_layout */
   {NULL, NULL}
};

static void _parent_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED);
static Eina_Bool _key_action_move(Evas_Object *obj, const char *params);
//TIZEN_ONLY(20150709) : add keybinding for elm_popup "block,clicked" action
static Eina_Bool _key_action_escape(Evas_Object *obj, const char *params);
///

static const Elm_Action key_actions[] = {
   {"move", _key_action_move},
//TIZEN_ONLY(20150709) : add keybinding for elm_popup "block,clicked" action
   {"escape", _key_action_escape},
///
   {NULL, NULL}
};

static void  _on_content_del(void *data, Evas *e, Evas_Object *obj, void *event_info);

EOLIAN static Eina_Bool
_elm_popup_elm_widget_translate(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   Elm_Popup_Item_Data *it;
   Eina_List *l;

   EINA_LIST_FOREACH(sd->items, l, it)
      eo_do(EO_OBJ(it), elm_wdg_item_translate());

   eo_do_super(obj, MY_CLASS, elm_obj_widget_translate());
   eo_do(sd->main_layout, elm_obj_widget_translate());

   return EINA_TRUE;
}

static void
_show_finished_cb(void *data,
                  Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   evas_object_smart_callback_call(data, SIG_SHOW_FINISHED, NULL);
}

static void
_hide_effect_finished_cb(void *data,
                         Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   evas_object_smart_callback_call(data, SIG_DISMISSED, NULL);
}

static void
_visuals_set(Evas_Object *obj)
{
   ELM_POPUP_DATA_GET(obj, sd);

   if (!sd->title_text && !sd->title_icon)
     elm_layout_signal_emit(sd->main_layout, "elm,state,title_area,hidden", "elm");
   else
     elm_layout_signal_emit(sd->main_layout, "elm,state,title_area,visible", "elm");

   if (sd->action_area)
     elm_layout_signal_emit(sd->main_layout, "elm,state,action_area,visible", "elm");
   else
     elm_layout_signal_emit(sd->main_layout, "elm,state,action_area,hidden", "elm");
}

static void
_block_clicked_cb(void *data,
                  Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   evas_object_smart_callback_call(data, SIG_BLOCK_CLICKED, NULL);
}

static void
_timeout_cb(void *data,
            Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   //TIZEN_ONLY(20150903): prevent popup hiding when the layout has "hide_finished_signal" data item
   //evas_object_hide(data);
   const char *hide_signal = NULL;

   ELM_POPUP_DATA_GET(data, sd);
   hide_signal = edje_object_data_get(elm_layout_edje_get(sd->main_layout), "hide_finished_signal");

   if ((hide_signal) && (!strcmp(hide_signal, "on")))
     elm_layout_signal_emit(sd->main_layout, "elm,state,hide", "elm");
   else
     evas_object_hide(data);
   //
   evas_object_smart_callback_call(data, SIG_TIMEOUT, NULL);
}

static Evas_Object *
_access_object_get(const Evas_Object *obj, const char* part)
{
   Evas_Object *po, *ao;
   ELM_POPUP_DATA_GET(obj, sd);

   po = (Evas_Object *)edje_object_part_object_get
      (elm_layout_edje_get(sd->main_layout), part);
   ao = evas_object_data_get(po, "_part_access_obj");

   return ao;
}

static void
_on_show(void *data EINA_UNUSED,
         Evas *e EINA_UNUSED,
         Evas_Object *obj,
         void *event_info EINA_UNUSED)
{
   elm_object_focus_set(obj, EINA_TRUE);
}

static void
_scroller_size_calc(Evas_Object *obj)
{
   Evas_Coord wy;
   Evas_Coord y, h;
   Evas_Coord h_title = 0;
   //TIZEN_ONLY(20150128): add subtitle text code of popup
   Evas_Coord h_subtitle = 0;
   //
   Evas_Coord h_action_area = 0;
   Evas_Coord h_pad = 0;
   //TIZEN_ONLY(20150305): fix popup align and size problem
   //const char *action_area_height;
   const char *str;
   //

   ELM_POPUP_DATA_GET(obj, sd);
   //TIZEN_ONLY(20150909): add widget data get to check orient mode
   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd);
   //

   //TIZEN_ONLY(20150204): add scroller of content text of popup
   //if (!sd->items) return;
   //

   sd->scr_size_recalc = EINA_FALSE;
   sd->max_sc_h = -1;
   sd->max_sc_w = -1;

   evas_object_geometry_get(sd->notify, NULL, &y, NULL, &h);

   /* TIZEN_ONLY(20151105): adjust notify's height
      in case height of popup's parent is smaller than window's
      and there is indicator below the popup */
   evas_object_geometry_get(elm_widget_top_get(sd->notify), NULL, &wy, NULL, NULL);
   h += (y - wy);

   //TIZEN_ONLY(20150305): fix popup align and size problem
   str = edje_object_data_get(elm_layout_edje_get(sd->main_layout),
                              "max_height");
   if (str) h = (int)(atoi(str)
                      * elm_config_scale_get()
                      * elm_object_scale_get(obj)
                      / edje_object_base_scale_get(elm_layout_edje_get(sd->main_layout)));
   //

   if (sd->title_text || sd->title_icon)
     edje_object_part_geometry_get(elm_layout_edje_get(sd->main_layout),
                                   "elm.bg.title", NULL, NULL, NULL, &h_title);
   //TIZEN_ONLY(20150128): add subtitle text code of popup
   if (sd->subtitle_text)
     edje_object_part_geometry_get(elm_layout_edje_get(sd->main_layout),
                                   "elm.text.subtitle", NULL, NULL, NULL, &h_subtitle);
   //
   str = edje_object_data_get(elm_layout_edje_get(sd->main_layout),
                              "popup_outside_pad");
   if (str) h_pad = (int)(atoi(str)
                      * elm_config_scale_get()
                      * elm_object_scale_get(obj)
                      / edje_object_base_scale_get(elm_layout_edje_get(sd->main_layout)));

   //TIZEN_ONLY(20150204): add scroller of content text of popup
   //if (sd->action_area)
   //  {
   //     action_area_height = edje_object_data_get(
   //     elm_layout_edje_get(sd->action_area), "action_area_height");
   //     if (action_area_height)
   //       h_action_area =
   //         (int)(atoi(action_area_height)
   //               * elm_config_scale_get() * elm_object_scale_get(obj));
   //  }
   if (sd->action_area)
     edje_object_part_geometry_get(elm_layout_edje_get(sd->main_layout),
                                   "elm.swallow.action_area.bg", NULL, NULL, NULL, &h_action_area);
   //

   //TIZEN_ONLY(20150827): add display mode configure to check compress mode
   //sd->max_sc_h = h - (h_title + h_action_area);
   if ((sd->dispmode == EVAS_DISPLAY_MODE_COMPRESS) && ((wd->orient_mode == 90) || (wd->orient_mode == 270)))
     sd->max_sc_h = h - h_action_area;
   else
     sd->max_sc_h = h - (h_title + h_subtitle + h_action_area + h_pad);
   //
}

static void
_size_hints_changed_cb(void *data,
                       Evas *e EINA_UNUSED,
                       Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   elm_layout_sizing_eval(data);
}

static void
_list_del(Elm_Popup_Data *sd)
{
   if (!sd->scr) return;

   evas_object_event_callback_del
     (sd->scr, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _size_hints_changed_cb);

   ELM_SAFE_FREE(sd->tbl, evas_object_del);
   sd->scr = NULL;
   sd->box = NULL;
   sd->spacer = NULL;
}

static void
_items_remove(Elm_Popup_Data *sd)
{
   Elm_Popup_Item_Data *it;

   if (!sd->items) return;

   EINA_LIST_FREE(sd->items, it)
     eo_do(EO_OBJ(it), elm_wdg_item_del());

   sd->items = NULL;
}

EOLIAN static void
_elm_popup_evas_object_smart_del(Eo *obj, Elm_Popup_Data *sd)
{
   unsigned int i;

   evas_object_event_callback_del_full(sd->parent, EVAS_CALLBACK_RESIZE, _parent_resize_cb, obj);
   evas_object_smart_callback_del
     (sd->notify, "show,finished", _show_finished_cb);
   evas_object_smart_callback_del
     (sd->notify, "block,clicked", _block_clicked_cb);
   evas_object_smart_callback_del(sd->notify, "timeout", _timeout_cb);
   evas_object_event_callback_del
     (sd->content, EVAS_CALLBACK_DEL, _on_content_del);
   evas_object_event_callback_del(obj, EVAS_CALLBACK_SHOW, _on_show);

   sd->last_button_number = 0;

   for (i = 0; i < ELM_POPUP_ACTION_BUTTON_MAX; i++)
     {
        if (sd->buttons[i])
          {
             evas_object_del(sd->buttons[i]->btn);
             ELM_SAFE_FREE(sd->buttons[i], free);
          }
     }
   if (sd->items)
     {
        _items_remove(sd);
        _list_del(sd);
     }

   // XXX? delete other objects? just to be sure.
   ELM_SAFE_FREE(sd->main_layout, evas_object_del);
   ELM_SAFE_FREE(sd->notify, evas_object_del);
   ELM_SAFE_FREE(sd->title_icon, evas_object_del);
   ELM_SAFE_FREE(sd->content_area, evas_object_del);
   ELM_SAFE_FREE(sd->text_content_obj, evas_object_del);
   ELM_SAFE_FREE(sd->action_area, evas_object_del);
   ELM_SAFE_FREE(sd->box, evas_object_del);
   ELM_SAFE_FREE(sd->tbl, evas_object_del);
   ELM_SAFE_FREE(sd->spacer, evas_object_del);
   ELM_SAFE_FREE(sd->scr, evas_object_del);
   ELM_SAFE_FREE(sd->content, evas_object_del);
   ELM_SAFE_FREE(sd->title_text, eina_stringshare_del);
   //TIZEN_ONLY(20150128): add subtitle text code of popup
   ELM_SAFE_FREE(sd->subtitle_text, eina_stringshare_del);
   //

   eo_do_super(obj, MY_CLASS, evas_obj_smart_del());
}

static void
_mirrored_set(Evas_Object *obj,
              Eina_Bool rtl)
{
   ELM_POPUP_DATA_GET(obj, sd);

   elm_object_mirrored_set(sd->notify, rtl);
}

static void
_access_obj_process(Eo *obj, Eina_Bool is_access)
{
   Evas_Object *ao;

   ELM_POPUP_DATA_GET(obj, sd);

   if (is_access)
     {
        if (sd->title_text)
          {
             ao = _elm_access_edje_object_part_object_register
                    (obj, elm_layout_edje_get(sd->main_layout), ACCESS_TITLE_PART);
             _elm_access_text_set(_elm_access_info_get(ao),
                                  ELM_ACCESS_TYPE, E_("Popup Title"));
             _elm_access_text_set(_elm_access_info_get(ao),
                                  ELM_ACCESS_INFO, sd->title_text);
          }

        if (sd->text_content_obj)
          {
             ao = _elm_access_edje_object_part_object_register
                    (obj, elm_layout_edje_get(sd->main_layout), ACCESS_BODY_PART);
             _elm_access_text_set(_elm_access_info_get(ao),
                                  ELM_ACCESS_TYPE, E_("Popup Body Text"));
             _elm_access_text_set(_elm_access_info_get(ao),
               ELM_ACCESS_INFO, elm_object_text_get(sd->text_content_obj));
          }
     }
   else
     {
        if (sd->title_text)
          {
             _elm_access_edje_object_part_object_unregister
                    (obj, elm_layout_edje_get(sd->main_layout), ACCESS_TITLE_PART);
          }

        if (sd->text_content_obj)
          {
             _elm_access_edje_object_part_object_unregister
                    (obj, elm_layout_edje_get(sd->main_layout), ACCESS_BODY_PART);
          }
     }
}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_theme_apply(Eo *obj, Elm_Popup_Data *sd)
{
   Elm_Popup_Item_Data *it;
   unsigned int i = 0;
   Eina_List *elist;
   char buf[1024], style[1024];

   _mirrored_set(obj, elm_widget_mirrored_get(obj));

   // TIZEN_ONLY(20150121): Change style name of popup groups.
   //snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(obj));
   snprintf(style, sizeof(style), "%s", elm_widget_style_get(obj));

   // TIZEN_ONLY(20150722): Change style name of popup groups.
   if (!strcmp(style, "default"))
     elm_widget_style_set(sd->notify, "popup");
   //
   else
     elm_widget_style_set(sd->notify, style);

   if (!elm_layout_theme_set(sd->main_layout, "popup", "base",
                             elm_widget_style_get(obj)))
     ERR("Failed to set layout!");

   if (sd->action_area)
     {
        snprintf(buf, sizeof(buf), "buttons%i", sd->last_button_number);
        if (!elm_layout_theme_set(sd->action_area, "popup", buf, style))
          ERR("Failed to set layout!");
     }
   if (!elm_layout_theme_set(sd->content_area, "popup", "content", style))
     ERR("Failed to set layout!");
   if (sd->text_content_obj)
   // TIZEN_ONLY(20150722): Change style name of popup groups.
   // elm_object_style_set(sd->text_content_obj, style);
   //
     {
        snprintf(buf, sizeof(buf), "popup/%s", style);
        elm_object_style_set(sd->text_content_obj, buf);
     }
   else if (sd->items)
     {
        EINA_LIST_FOREACH(sd->items, elist, it)
          {
             if (!elm_layout_theme_set(VIEW(it), "popup", "item", style))
               ERR("Failed to set layout!");
             else
               {
                  if (it->label)
                    {
                       elm_layout_text_set(VIEW(it), "elm.text", it->label);
                       elm_layout_signal_emit(VIEW(it),
                                              "elm,state,item,text,visible",
                                              "elm");
                    }
                  if (it->icon)
                    elm_layout_signal_emit(VIEW(it),
                                           "elm,state,item,icon,visible",
                                           "elm");
                  if (it->disabled)
                    elm_layout_signal_emit(VIEW(it),
                                           "elm,state,item,disabled", "elm");
                  evas_object_show(VIEW(it));
                  edje_object_message_signal_process(
                     elm_layout_edje_get(VIEW(it)));
               }
          }
     }
   if (sd->title_text)
     {
        elm_layout_text_set(sd->main_layout, "elm.text.title", sd->title_text);
        elm_layout_signal_emit(sd->main_layout, "elm,state,title,text,visible", "elm");
     }
   if (sd->title_icon)
     elm_layout_signal_emit(sd->main_layout, "elm,state,title,icon,visible", "elm");
   //TIZEN_ONLY(20150128): add subtitle text code of popup
   if (sd->subtitle_text)
     {
        elm_layout_text_set(sd->main_layout, "elm.text.subtitle", sd->subtitle_text);
        elm_layout_signal_emit(sd->main_layout, "elm,state,subtitle,text,visible", "elm");
     }
   //

   _visuals_set(obj);
   //TIZEN_ONLY(20150820): change layout sizing eval to popup sizing eval
   edje_object_message_signal_process(elm_layout_edje_get(sd->main_layout));
   //elm_layout_sizing_eval(sd->main_layout);
   _scroller_size_calc(obj);
   elm_layout_sizing_eval(obj);
   //

   /* access */
   if (_elm_config->access_mode) _access_obj_process(obj, EINA_TRUE);

   return EINA_TRUE;
}

static void
_item_sizing_eval(Elm_Popup_Item_Data *it)
{
   Evas_Coord min_w = -1, min_h = -1, max_w = -1, max_h = -1;
   Evas_Object *edje = elm_layout_edje_get(VIEW(it));

   edje_object_size_min_restricted_calc
     (edje, &min_w, &min_h, min_w, min_h);
   evas_object_size_hint_min_set(edje, min_w, min_h);
   evas_object_size_hint_max_set(edje, max_w, max_h);
}

EOLIAN static void
_elm_popup_elm_layout_sizing_eval(Eo *obj, Elm_Popup_Data *sd)
{
   Eina_List *elist;
   Elm_Popup_Item_Data *it;
   Evas_Coord h_box = 0, minh_box = 0;
   Evas_Coord minw = -1, minh = -1;

   //TIZEN_ONLY(20150305): add a allow eval flag not to call another sizing eval during sizing eval
   if (!sd->allow_eval) return;

   sd->allow_eval = EINA_FALSE;
   //
   if (sd->items)
     {
        EINA_LIST_FOREACH(sd->items, elist, it)
          {
             _item_sizing_eval(it);
             evas_object_size_hint_min_get(elm_layout_edje_get(VIEW(it)),
                                           NULL, &minh_box);
             if (minh_box != -1) h_box += minh_box;
          }
        evas_object_size_hint_min_set(sd->spacer, 0, MIN(h_box, sd->max_sc_h));
        evas_object_size_hint_max_set(sd->spacer, -1, sd->max_sc_h);

        evas_object_size_hint_min_get(sd->scr, &minw, &minh);
        evas_object_size_hint_max_get(sd->scr, &minw, &minh);
     }
   //TIZEN_ONLY(20150305): fix popup align and size problem
   else if(sd->content || sd->text_content_obj)
     {
        double horizontal, vertical;
        Evas_Coord w, h;
        int w_content_area = 9999;
        int rotation = -1;
        const char *str;

        edje_object_message_signal_process(elm_layout_edje_get(sd->content_area));

        elm_popup_align_get(obj, &horizontal, &vertical);
        evas_object_geometry_get(elm_widget_parent_get(obj), NULL, NULL, &w, &h);

        str = edje_object_data_get(elm_layout_edje_get(sd->content_area), "content_area_width");

        if (str) w_content_area = (int)(atoi(str)
                                      * elm_config_scale_get()
                                      * elm_object_scale_get(obj)
                                      / edje_object_base_scale_get(elm_layout_edje_get(sd->content_area)));

        if (horizontal == ELM_NOTIFY_ALIGN_FILL)
          minw = w;

        if (vertical == ELM_NOTIFY_ALIGN_FILL)
          minh = h;

        rotation = elm_win_rotation_get(elm_widget_top_get(elm_widget_parent_get(sd->notify)));
        if ((rotation == 90 || rotation == 270) && (horizontal == ELM_NOTIFY_ALIGN_FILL))
          minw = w_content_area;

        edje_object_size_min_restricted_calc(elm_layout_edje_get(sd->content_area),
                                             &minw, &minh, minw, minh);
        evas_object_size_hint_min_set(sd->content_area, minw, minh);

        //TIZEN_ONLY(20150806): support scrollable content
        //FIXME: genlist only now, it should be changed to support other scrollable.
        if (sd->scrollable_content)
          minh = sd->min_scrollable_content_h;
        //

        if (minh > sd->max_sc_h)
          evas_object_size_hint_min_set(sd->spacer, minw, sd->max_sc_h);
        else if(minh >= 0)
          evas_object_size_hint_min_set(sd->spacer, minw, minh);
     }
     //

   //TIZEN_ONLY(20150305): fix popup align and size problem
   //edje_object_size_min_calc(elm_layout_edje_get(sd->main_layout), &minw, &minh);

   //evas_object_size_hint_min_set(obj, minw, minh);
   //evas_object_size_hint_max_set(obj, -1, -1);
   //

   //TIZEN_ONLY(20150305): add a allow eval flag not to call another sizing eval during sizing eval
   sd->allow_eval = EINA_TRUE;
   //
}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_sub_object_del(Eo *obj, Elm_Popup_Data *sd, Evas_Object *sobj)
{
   Elm_Popup_Item_Data *it;
   Eina_Bool int_ret = EINA_FALSE;

   eo_do_super(obj, MY_CLASS, int_ret = elm_obj_widget_sub_object_del(sobj));
   if (!int_ret) return EINA_FALSE;

   if (sobj == sd->title_icon)
     {
        elm_layout_signal_emit(sd->main_layout, "elm,state,title,icon,hidden", "elm");
        edje_object_message_signal_process(elm_layout_edje_get(sd->main_layout));
     }
   else if ((it =
               evas_object_data_get(sobj, "_popup_icon_parent_item")) != NULL)
     {
        if (sobj == it->icon)
          {
             eo_do(VIEW(it), elm_obj_container_content_unset(CONTENT_PART));
             elm_layout_signal_emit(VIEW(it),
                                    "elm,state,item,icon,hidden", "elm");
             it->icon = NULL;
          }
     }

   return EINA_TRUE;
}

static void
_on_content_del(void *data,
                Evas *e EINA_UNUSED,
                Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   ELM_POPUP_DATA_GET(data, sd);

   sd->content = NULL;
   elm_layout_sizing_eval(sd->main_layout);
}

static void
_on_text_content_del(void *data,
                     Evas *e EINA_UNUSED,
                     Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   ELM_POPUP_DATA_GET(data, sd);

   sd->text_content_obj = NULL;
   elm_layout_sizing_eval(sd->main_layout);
}

static void
_on_table_del(void *data,
              Evas *e EINA_UNUSED,
              Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   ELM_POPUP_DATA_GET(data, sd);

   sd->tbl = NULL;
   sd->spacer = NULL;
   sd->scr = NULL;
   sd->box = NULL;
   elm_layout_sizing_eval(sd->main_layout);
}

static void
_on_button_del(void *data,
               Evas *e EINA_UNUSED,
               Evas_Object *obj,
               void *event_info EINA_UNUSED)
{
   int i;

   ELM_POPUP_DATA_GET(data, sd);

   for (i = 0; i < ELM_POPUP_ACTION_BUTTON_MAX; i++)
     {
        if (sd->buttons[i] && obj == sd->buttons[i]->btn &&
            sd->buttons[i]->delete_me == EINA_TRUE)
          {
             _button_remove(data, i, EINA_FALSE);
             break;
          }
     }
}

static void
_button_remove(Evas_Object *obj,
               int pos,
               Eina_Bool delete)
{
   int i = 0;
   char buf[128];

   ELM_POPUP_DATA_GET(obj, sd);

   if (!sd->last_button_number) return;

   if (!sd->buttons[pos]) return;

   if (delete)
     {
        evas_object_del(sd->buttons[pos]->btn);
     }
   else
     {
        evas_object_event_callback_del
          (sd->buttons[pos]->btn, EVAS_CALLBACK_DEL, _on_button_del);
        // TIZEN_ONLY(20150121): Change style name of popup groups.
        //snprintf(buf, sizeof(buf), "elm.swallow.content.button%i", pos + 1);
        snprintf(buf, sizeof(buf), "actionbtn%i", pos + 1);
        //
        elm_object_part_content_unset(sd->action_area, buf);
     }

   ELM_SAFE_FREE(sd->buttons[pos], free);

   for (i = ELM_POPUP_ACTION_BUTTON_MAX - 1; i >= 0; i--)
     {
        if (sd->buttons[i])
          {
             sd->last_button_number = i + 1;
             break;
          }
     }

   if (!sd->last_button_number)
     {
        _visuals_set(obj);
        ELM_SAFE_FREE(sd->action_area, evas_object_del);
        edje_object_message_signal_process(elm_layout_edje_get(sd->main_layout));
     }
   else
     {
        char style[1024];

        // TIZEN_ONLY(20150121): Change style name of popup groups.
        //snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(obj));
        snprintf(style, sizeof(style), "%s", elm_widget_style_get(obj));
        //
        snprintf(buf, sizeof(buf), "buttons%i", sd->last_button_number);
        if (!elm_layout_theme_set(sd->action_area, "popup", buf, style))
          ERR("Failed to set layout!");
     }
}

static void
_layout_change_cb(void *data EINA_UNUSED,
                  Evas_Object *obj,
                  const char *emission EINA_UNUSED,
                  const char *source EINA_UNUSED)
{
   elm_layout_sizing_eval(obj);
}

static void
_restack_cb(void *data EINA_UNUSED,
            Evas *e EINA_UNUSED,
            Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   ELM_POPUP_DATA_GET(obj, sd);

   evas_object_raise(sd->notify);
}

//TIZEN_ONLY(20150204): add scroller of content text of popup
//static void
//_list_add(Evas_Object *obj)
//{
//   char style[1024];
//
//   ELM_POPUP_DATA_GET(obj, sd);
//
//   //Table
//   sd->tbl = elm_table_add(sd->main_layout);
//   evas_object_event_callback_add(sd->tbl, EVAS_CALLBACK_DEL,
//                                  _on_table_del, obj);
//   eo_do(sd->main_layout, elm_obj_container_content_set(CONTENT_PART, sd->tbl));
//   evas_object_show(sd->tbl);
//
//   //Spacer
//   sd->spacer = evas_object_rectangle_add(evas_object_evas_get(obj));
//   evas_object_color_set(sd->spacer, 0, 0, 0, 0);
//   elm_table_pack(sd->tbl, sd->spacer, 0, 0, 1, 1);
//
//   //Scroller
//   snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(obj));
//   sd->scr = elm_scroller_add(sd->tbl);
//   elm_widget_style_set(sd->scr, style);
//   elm_scroller_content_min_limit(sd->scr, EINA_TRUE, EINA_FALSE);
//   elm_scroller_bounce_set(sd->scr, EINA_FALSE, EINA_TRUE);
//   evas_object_size_hint_weight_set(sd->scr,
//                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
//   evas_object_size_hint_align_set(sd->scr, EVAS_HINT_FILL, EVAS_HINT_FILL);
//   evas_object_event_callback_add(sd->scr, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
//                                  _size_hints_changed_cb, sd->main_layout);
//   elm_table_pack(sd->tbl, sd->scr, 0, 0, 1, 1);
//   evas_object_show(sd->scr);
//
//   //Box
//   sd->box = elm_box_add(sd->scr);
//   evas_object_size_hint_weight_set(sd->box, EVAS_HINT_EXPAND, 0.0);
//   evas_object_size_hint_align_set(sd->box, EVAS_HINT_FILL, 0.0);
//   elm_object_content_set(sd->scr, sd->box);
//   evas_object_show(sd->box);
//}
//

static void
_item_select_cb(void *data,
                Evas_Object *obj EINA_UNUSED,
                const char *emission EINA_UNUSED,
                const char *source EINA_UNUSED)
{
   Elm_Popup_Item_Data *it = data;

   if (!it || it->disabled) return;
   if (it->func)
     it->func((void *)WIDGET_ITEM_DATA_GET(EO_OBJ(it)), WIDGET(it), EO_OBJ(it));
}

static void
_item_text_set(Elm_Popup_Item_Data *it,
               const char *label)
{
   if (!eina_stringshare_replace(&it->label, label)) return;

   elm_layout_text_set(VIEW(it), "elm.text", label);

   if (it->label)
     elm_layout_signal_emit(VIEW(it),
                            "elm,state,item,text,visible", "elm");
   else
     elm_layout_signal_emit(VIEW(it),
                            "elm,state,item,text,hidden", "elm");

   edje_object_message_signal_process(elm_layout_edje_get(VIEW(it)));
}

EOLIAN static void
_elm_popup_item_elm_widget_item_part_text_set(Eo *eo_it EINA_UNUSED, Elm_Popup_Item_Data *it,
                    const char *part,
                    const char *label)
{
   ELM_POPUP_ITEM_CHECK_OR_RETURN(it);

   if ((!part) || (!strcmp(part, "default")))
     {
        _item_text_set(it, label);
        return;
     }

   WRN("The part name is invalid! : popup=%p", WIDGET(it));
}

EOLIAN static const char *
_elm_popup_item_elm_widget_item_part_text_get(Eo *eo_it EINA_UNUSED, Elm_Popup_Item_Data *it,
                    const char *part)
{
   ELM_POPUP_ITEM_CHECK_OR_RETURN(it, NULL);

   if ((!part) || (!strcmp(part, "default")))
     return it->label;

   WRN("The part name is invalid! : popup=%p", WIDGET(it));

   return NULL;
}

static void
_item_icon_set(Elm_Popup_Item_Data *it,
               Evas_Object *icon)
{
   if (it->icon == icon) return;

   evas_object_del(it->icon);
   it->icon = icon;
   if (it->icon)
     {
        elm_widget_sub_object_add(WIDGET(it), it->icon);
        evas_object_data_set(it->icon, "_popup_icon_parent_item", it);
        eo_do(VIEW(it), elm_obj_container_content_set(CONTENT_PART, it->icon));
        elm_layout_signal_emit(VIEW(it), "elm,state,item,icon,visible", "elm");
     }
   else
     elm_layout_signal_emit(VIEW(it), "elm,state,item,icon,hidden", "elm");

   edje_object_message_signal_process(elm_layout_edje_get(VIEW(it)));
}

EOLIAN static void
_elm_popup_item_elm_widget_item_part_content_set(Eo *eo_it EINA_UNUSED, Elm_Popup_Item_Data *it,
                       const char *part,
                       Evas_Object *content)
{
   ELM_POPUP_ITEM_CHECK_OR_RETURN(it);

   if ((!(part)) || (!strcmp(part, "default")))
     _item_icon_set(it, content);
   else
     WRN("The part name is invalid! : popup=%p", WIDGET(it));
}

EOLIAN static Evas_Object *
_elm_popup_item_elm_widget_item_part_content_get(Eo *eo_it EINA_UNUSED, Elm_Popup_Item_Data *it,
                       const char *part)
{
   ELM_POPUP_ITEM_CHECK_OR_RETURN(it, NULL);

   if ((!(part)) || (!strcmp(part, "default")))
     return it->icon;

   WRN("The part name is invalid! : popup=%p", WIDGET(it));

   return NULL;
}

static Evas_Object *
_item_icon_unset(Elm_Popup_Item_Data *it)
{
   Evas_Object *icon = it->icon;

   if (!it->icon) return NULL;
   elm_widget_sub_object_del(WIDGET(it), icon);
   evas_object_data_del(icon, "_popup_icon_parent_item");
   eo_do(VIEW(it), elm_obj_container_content_unset(CONTENT_PART));
   elm_layout_signal_emit(VIEW(it), "elm,state,item,icon,hidden", "elm");
   it->icon = NULL;

   return icon;
}

EOLIAN static Evas_Object *
_elm_popup_item_elm_widget_item_part_content_unset(Eo *eo_it EINA_UNUSED, Elm_Popup_Item_Data *it,
                         const char *part)
{
   Evas_Object *content = NULL;

   ELM_POPUP_ITEM_CHECK_OR_RETURN(it, NULL);

   if ((!(part)) || (!strcmp(part, "default")))
     content = _item_icon_unset(it);
   else
     WRN("The part name is invalid! : popup=%p", WIDGET(it));

   return content;
}

EOLIAN static void
_elm_popup_item_elm_widget_item_disable(Eo *eo_it, Elm_Popup_Item_Data *it)
{
   Eina_Bool tmp;
   ELM_POPUP_ITEM_CHECK_OR_RETURN(it);

   if (eo_do_ret(eo_it, tmp, elm_wdg_item_disabled_get()))
     elm_layout_signal_emit(VIEW(it), "elm,state,item,disabled", "elm");
   else
     elm_layout_signal_emit(VIEW(it), "elm,state,item,enabled", "elm");
}

EOLIAN static void
_elm_popup_item_eo_base_destructor(Eo *eo_it, Elm_Popup_Item_Data *it)
{
   ELM_POPUP_ITEM_CHECK_OR_RETURN(it);
   ELM_POPUP_DATA_GET(WIDGET(it), sd);

   evas_object_del(it->icon);
   eina_stringshare_del(it->label);
   sd->items = eina_list_remove(sd->items, it);
   if (!eina_list_count(sd->items))
     {
        sd->items = NULL;
        _list_del(sd);
     }
   eo_do_super(eo_it, ELM_POPUP_ITEM_CLASS, eo_destructor());
}

EOLIAN static void
_elm_popup_item_elm_widget_item_signal_emit(Eo *eo_it EINA_UNUSED, Elm_Popup_Item_Data *it,
                       const char *emission,
                       const char *source)
{
   elm_layout_signal_emit(VIEW(it), emission, source);
}

static void
_item_focused_cb(void *data,
                 Evas_Object *obj EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   Elm_Popup_Item_Data *it = data;

   evas_object_smart_callback_call(WIDGET(it), SIG_ITEM_FOCUSED, EO_OBJ(it));
}

static void
_item_unfocused_cb(void *data,
                   Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Elm_Popup_Item_Data *it = data;

   evas_object_smart_callback_call(WIDGET(it), SIG_ITEM_UNFOCUSED, EO_OBJ(it));
}

EOLIAN static Eo *
_elm_popup_item_eo_base_constructor(Eo *eo_it, Elm_Popup_Item_Data *it)
{
   eo_it = eo_do_super_ret(eo_it, ELM_POPUP_ITEM_CLASS, eo_it, eo_constructor());
   it->base = eo_data_scope_get(eo_it, ELM_WIDGET_ITEM_CLASS);

   return eo_it;
}

static void
_item_new(Elm_Popup_Item_Data *it)
{
   char style[1024];

   VIEW(it) = elm_layout_add(WIDGET(it));
   elm_object_focus_allow_set(VIEW(it), EINA_TRUE);

   snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(WIDGET(it)));
   if (!elm_layout_theme_set(VIEW(it), "popup", "item", style))
     ERR("Failed to set layout!");
   else
     {
        elm_layout_signal_callback_add(VIEW(it), "elm,action,click", "*",
                                       _item_select_cb, it);
        evas_object_size_hint_align_set(VIEW(it), EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_smart_callback_add(VIEW(it), SIG_LAYOUT_FOCUSED, _item_focused_cb, it);
        evas_object_smart_callback_add(VIEW(it), SIG_LAYOUT_UNFOCUSED, _item_unfocused_cb, it);
        evas_object_show(VIEW(it));
     }
}

//TIZEN_ONLY(20150128): add subtitle text code of popup
static Eina_Bool
_subtitle_text_set(Evas_Object *obj,
                const char *text)
{
   ELM_POPUP_DATA_GET(obj, sd);

   if (sd->subtitle_text == text) return EINA_TRUE;

   eina_stringshare_replace(&sd->subtitle_text, text);

   //bare edje here because we're inside the hook, already
   edje_object_part_text_escaped_set
     (elm_layout_edje_get(sd->main_layout), "elm.text.subtitle", text);

   if (sd->subtitle_text)
     elm_object_signal_emit(sd->main_layout, "elm,state,subtitle,text,visible", "elm");
   else
     elm_object_signal_emit(sd->main_layout, "elm,state,subtitle,text,hidden", "elm");

   edje_object_message_signal_process(elm_layout_edje_get(sd->main_layout));

   return EINA_TRUE;
}
//

//TIZEN ONLY(20150717): expose title as at-spi object
static char *
_access_info_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   Elm_Popup_Data *priv = data;
   return priv->title_text ? strdup(priv->title_text) : NULL;
}
//

static Eina_Bool
_title_text_set(Evas_Object *obj,
                const char *text)
{
   Evas_Object *ao;
   Eina_Bool title_visibility_old, title_visibility_current;

   ELM_POPUP_DATA_GET(obj, sd);

   if (sd->title_text == text) return EINA_TRUE;

   title_visibility_old = (sd->title_text) || (sd->title_icon);
   eina_stringshare_replace(&sd->title_text, text);

   elm_layout_text_set(sd->main_layout, "elm.text.title", text);

   /* access */
   if (_elm_config->access_mode)
     {
        ao = _access_object_get(obj, ACCESS_TITLE_PART);
        if (!ao)
          {
             ao = _elm_access_edje_object_part_object_register
                    (obj, elm_layout_edje_get(sd->main_layout), ACCESS_TITLE_PART);
             _elm_access_text_set(_elm_access_info_get(ao),
                                  ELM_ACCESS_TYPE, E_("Popup Title"));
          }
        _elm_access_text_set(_elm_access_info_get(ao), ELM_ACCESS_INFO, text);
     }

   //TIZEN ONLY(20150717): expose title as at-spi object
   if (_elm_config->atspi_mode)
     {
        if (sd->title_text)
          {
             ao = _access_object_get(obj, ACCESS_TITLE_PART);
             if (!ao)
               {
                  ao = _elm_access_edje_object_part_object_register
                        (obj, elm_layout_edje_get(sd->main_layout), ACCESS_TITLE_PART);
                  elm_atspi_accessible_role_set(ao, ELM_ATSPI_ROLE_HEADING);
                  _elm_access_callback_set(_elm_access_info_get(ao),
                                            ELM_ACCESS_INFO, _access_info_cb, sd);
               }
          }
        else
          {
                ao = _access_object_get(obj, ACCESS_TITLE_PART);
                if (ao)
                    _elm_access_object_unregister(ao, NULL);
          }
     }
   //
   if (sd->title_text)
     elm_layout_signal_emit(sd->main_layout, "elm,state,title,text,visible", "elm");
   else
     elm_layout_signal_emit(sd->main_layout, "elm,state,title,text,hidden", "elm");

   title_visibility_current = (sd->title_text) || (sd->title_icon);

   if (title_visibility_old != title_visibility_current)
     _visuals_set(obj);

   //TIZEN_ONLY(20150316): scroller size calculation for title text popup
   edje_object_message_signal_process(elm_layout_edje_get(sd->main_layout));
   //

   return EINA_TRUE;
}

static Eina_Bool
_content_text_set(Evas_Object *obj,
                  const char *text)
{
   Evas_Object *ao;
   char style[1024];

   ELM_POPUP_DATA_GET(obj, sd);

   if (sd->items)
     {
        _items_remove(sd);
        _list_del(sd);
     }
   //TIZEN_ONLY(20150204): add scroller of content text of popup
   //else eo_do(sd->main_layout, elm_obj_container_content_set(CONTENT_PART, sd->content_area));
   else elm_object_content_set(sd->scr, sd->content_area);
   //

   if (!text) goto end;

   if (sd->text_content_obj)
     {
        eo_do(sd->content_area, sd->text_content_obj = elm_obj_container_content_unset(CONTENT_PART));
        evas_object_del(sd->text_content_obj);
        sd->text_content_obj = NULL;
     }

   sd->text_content_obj = elm_label_add(sd->content_area);
   snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(obj));
   elm_object_style_set(sd->text_content_obj, style);

   evas_object_event_callback_add
     (sd->text_content_obj, EVAS_CALLBACK_DEL, _on_text_content_del, obj);

   elm_label_line_wrap_set(sd->text_content_obj, sd->content_text_wrap_type);
   elm_object_text_set(sd->text_content_obj, text);
   evas_object_size_hint_weight_set
     (sd->text_content_obj, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set
     (sd->text_content_obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
   eo_do(sd->content_area, elm_obj_container_content_set
     (CONTENT_PART, sd->text_content_obj));

   /* access */
   if (_elm_config->access_mode)
     {
        /* unregister label, ACCESS_BODY_PART will register */
        elm_access_object_unregister(sd->text_content_obj);

        ao = _access_object_get(obj, ACCESS_BODY_PART);
        if (!ao)
          {
             ao = _elm_access_edje_object_part_object_register
                    (obj, elm_layout_edje_get(sd->main_layout), ACCESS_BODY_PART);
             _elm_access_text_set(_elm_access_info_get(ao),
                                  ELM_ACCESS_TYPE, E_("Popup Body Text"));
          }
        _elm_access_text_set(_elm_access_info_get(ao), ELM_ACCESS_INFO, text);
     }

end:
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_elm_popup_elm_layout_text_set(Eo *obj, Elm_Popup_Data *_pd, const char *part, const char *label)
{
   Eina_Bool int_ret = EINA_FALSE;

   if (!part || !strcmp(part, "default"))
     int_ret = _content_text_set(obj, label);
   else if (!strcmp(part, "title,text"))
     int_ret = _title_text_set(obj, label);
   //TIZEN_ONLY(20150128): add subtitle text code of popup
   else if (!strcmp(part, "subtitle,text"))
     int_ret = _subtitle_text_set(obj, label);
   //
   else
     int_ret = elm_layout_text_set(_pd->main_layout, part, label);

   _scroller_size_calc(obj);
   elm_layout_sizing_eval(obj);

   return int_ret;
}

//TIZEN_ONLY(20150128): add subtitle text code of popup
static const char *
_subtitle_text_get(const Elm_Popup_Data *sd)
{
   return sd->subtitle_text;
}
//

static const char *
_title_text_get(const Elm_Popup_Data *sd)
{
   return sd->title_text;
}

static const char *
_content_text_get(const Elm_Popup_Data *sd)
{
   const char *str = NULL;

   if (sd->text_content_obj)
     str = elm_object_text_get(sd->text_content_obj);

   return str;
}

EOLIAN static const char*
_elm_popup_elm_layout_text_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *_pd, const char *part)
{
   const char *text = NULL;

   if (!part || !strcmp(part, "default"))
     text = _content_text_get(_pd);
   else if (!strcmp(part, "title,text"))
     text = _title_text_get(_pd);
   //TIZEN_ONLY(20150128): add subtitle text code of popup
   else if (!strcmp(part, "subtitle,text"))
     text = _subtitle_text_get(_pd);
   //
   else
     text = elm_layout_text_get(_pd->main_layout, part);

   return text;
}

static Eina_Bool
_title_icon_set(Evas_Object *obj,
                Evas_Object *icon)
{
   Eina_Bool title_visibility_old, title_visibility_current;

   ELM_POPUP_DATA_GET(obj, sd);

   if (sd->title_icon == icon) return EINA_TRUE;
   title_visibility_old = (sd->title_text) || (sd->title_icon);
   evas_object_del(sd->title_icon);

   sd->title_icon = icon;
   title_visibility_current = (sd->title_text) || (sd->title_icon);

   eo_do(sd->main_layout, elm_obj_container_content_set
      ("elm.swallow.title.icon", sd->title_icon));

   if (sd->title_icon)
     elm_layout_signal_emit(sd->main_layout, "elm,state,title,icon,visible", "elm");
   if (title_visibility_old != title_visibility_current) _visuals_set(obj);

   return EINA_TRUE;
}

//TIZEN_ONLY(20150806): support scrollable content
//FIXME: genlist only now, it should be changed to support other scrollable.
static void
_scrollable_content_loaded_cb(void *data, Evas_Object *obj, void *event_info)
{
   ELM_POPUP_DATA_GET(data, sd);

   Evas_Coord w, h;
   eo_do(obj, elm_interface_scrollable_content_size_get(&w, &h));

   sd->scrollable_content = EINA_TRUE;
   sd->min_scrollable_content_h = h;

   elm_layout_sizing_eval(data);
}
//

static Eina_Bool
_content_set(Evas_Object *obj,
             Evas_Object *content)
{
   ELM_POPUP_DATA_GET(obj, sd);

   if (sd->content && sd->content == content) return EINA_TRUE;
   if (sd->items)
     {
        _items_remove(sd);
        _list_del(sd);
     }

   sd->content = content;
   if (content)
     {
        //TIZEN_ONLY(20150305): set scroller to content area
        //eo_do(sd->main_layout, elm_obj_container_content_set
        //   (CONTENT_PART, sd->content_area));
        evas_object_show(sd->content_area);
        elm_object_content_set(sd->scr, sd->content_area);
        //

        eo_do(sd->content_area, elm_obj_container_content_set
          (CONTENT_PART, content));
        evas_object_show(content);

        //TIZEN_ONLY(20150806): support scrollable content
        //FIXME: genlist only now, it should be changed to support other scrollable.
        if (eo_isa(content, ELM_INTERFACE_SCROLLABLE_MIXIN))
          evas_object_smart_callback_add(content, "loaded", _scrollable_content_loaded_cb, obj);
        //

        evas_object_event_callback_add
          (content, EVAS_CALLBACK_DEL, _on_content_del, obj);
     }

   return EINA_TRUE;
}

static void
_action_button_set(Evas_Object *obj,
                   Evas_Object *btn,
                   unsigned int idx)
{
   char buf[128], style[1024];

   ELM_POPUP_DATA_GET(obj, sd);

   if (idx >= ELM_POPUP_ACTION_BUTTON_MAX) return;

   if (!btn)
     {
        _button_remove(obj, idx, EINA_TRUE);
        return;
     }

   if (sd->buttons[idx])
     {
        evas_object_del(sd->buttons[idx]->btn);
        free(sd->buttons[idx]);
     }

   sd->buttons[idx] = ELM_NEW(Action_Area_Data);
   sd->buttons[idx]->obj = obj;
   sd->buttons[idx]->btn = btn;

   evas_object_event_callback_add
     (btn, EVAS_CALLBACK_DEL, _on_button_del, obj);

   for (unsigned int i = ELM_POPUP_ACTION_BUTTON_MAX - 1; i >= idx; i--)
     {
        if (sd->buttons[i])
          {
             sd->last_button_number = i + 1;
             break;
          }
     }

   snprintf(buf, sizeof(buf), "buttons%i", sd->last_button_number);
   if (!sd->action_area)
     {
        sd->action_area = elm_layout_add(sd->main_layout);
        evas_object_event_callback_add
          (sd->action_area, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
           _size_hints_changed_cb, sd->main_layout);
        eo_do(sd->main_layout, elm_obj_container_content_set
          ("elm.swallow.action_area", sd->action_area));

        _visuals_set(obj);
     }

   // TIZEN_ONLY(20150121): Change style name of popup groups.
   //snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(obj));
   snprintf(style, sizeof(style), "%s", elm_widget_style_get(obj));
   //
   if (!elm_layout_theme_set(sd->action_area, "popup", buf, style))
     ERR("Failed to set layout!");

   // TIZEN_ONLY(20150121): Change style name of popup groups.
   //snprintf(buf, sizeof(buf), "elm.swallow.content.button%i", idx + 1);
   snprintf(buf, sizeof(buf), "actionbtn%i", idx + 1);
   //
   elm_object_part_content_set
     (sd->action_area, buf, sd->buttons[idx]->btn);

   edje_object_message_signal_process(elm_layout_edje_get(sd->main_layout));
}

EOLIAN static Eina_Bool
_elm_popup_elm_container_content_set(Eo *obj, Elm_Popup_Data *_pd EINA_UNUSED, const char *part, Evas_Object *content)
{
   Eina_Bool ret = EINA_TRUE;
   unsigned int i;

   if (!part || !strcmp(part, "default"))
     ret = _content_set(obj, content);
   else if (!strcmp(part, "title,icon"))
     ret = _title_icon_set(obj, content);
   else if (!strncmp(part, "button", 6))
     {
        i = atoi(part + 6) - 1;

        if (i >= ELM_POPUP_ACTION_BUTTON_MAX)
          {
             ERR("The part name is invalid! : popup=%p", obj);
             return EINA_FALSE;
          }

        _action_button_set(obj, content, i);
     }
   else
     ret = eo_do_ret(_pd->main_layout, ret, elm_obj_container_content_set(part, content));

   _scroller_size_calc(obj);
   elm_layout_sizing_eval(obj);

   return ret;
}

static Evas_Object *
_title_icon_get(const Elm_Popup_Data *sd)
{
   return sd->title_icon;
}

static Evas_Object *
_content_get(const Elm_Popup_Data *sd)
{
   return sd->content;
}

static Evas_Object *
_action_button_get(const Evas_Object *obj,
                   unsigned int idx)
{
   Evas_Object *button = NULL;

   ELM_POPUP_DATA_GET(obj, sd);
   if (!sd->action_area) return NULL;

   if (sd->buttons[idx])
     button = sd->buttons[idx]->btn;

   return button;
}

EOLIAN static Evas_Object*
_elm_popup_elm_container_content_get(Eo *obj, Elm_Popup_Data *_pd, const char *part)
{
   Evas_Object *content = NULL;
   unsigned int i;

   if (!part || !strcmp(part, "default"))
     content = _content_get(_pd);
   else if (!strcmp(part, "title,text"))
     content = _title_icon_get(_pd);
   else if (!strncmp(part, "button", 6))
     {
        i = atoi(part + 6) - 1;

        if (i >= ELM_POPUP_ACTION_BUTTON_MAX)
          goto err;

        content = _action_button_get(obj, i);
     }
   else
      eo_do(_pd->main_layout, content = elm_obj_container_content_get(part));

   if (!content)
     goto err;

   return content;

err:
   WRN("The part name is invalid! : popup=%p", obj);
   return NULL;
}

static Evas_Object *
_content_unset(Evas_Object *obj)
{
   Evas_Object *content;

   ELM_POPUP_DATA_GET(obj, sd);

   if (!sd->content) return NULL;

   evas_object_event_callback_del
     (sd->content, EVAS_CALLBACK_DEL, _on_content_del);

   eo_do(sd->content_area, content = elm_obj_container_content_unset(CONTENT_PART));
   sd->content = NULL;

   //TIZEN_ONLY(20150206): change layout sizing eval to popup sizing eval
   //elm_layout_sizing_eval(sd->main_layout);
   elm_layout_sizing_eval(obj);
   //

   return content;
}

static Evas_Object *
_title_icon_unset(Evas_Object *obj)
{
   Evas_Object *icon;

   ELM_POPUP_DATA_GET(obj, sd);

   if (!sd->title_icon) return NULL;

   icon = sd->title_icon;
   eo_do(sd->main_layout, elm_obj_container_content_unset("elm.swallow.title.icon"));
   sd->title_icon = NULL;

   return icon;
}

EOLIAN static Evas_Object*
_elm_popup_elm_container_content_unset(Eo *obj, Elm_Popup_Data *_pd EINA_UNUSED, const char *part)
{
   Evas_Object *content = NULL;
   unsigned int i;

   if (!part || !strcmp(part, "default"))
     content = _content_unset(obj);
   else if (!strcmp(part, "title,icon"))
     content = _title_icon_unset(obj);
   else if (!strncmp(part, "button", 6))
     {
        i = atoi(part + 6) - 1;

        if (i >= ELM_POPUP_ACTION_BUTTON_MAX)
          goto err;

        _button_remove(obj, i, EINA_FALSE);
     }
   else
     goto err;

   return content;

err:
   ERR("The part name is invalid! : popup=%p", obj);

   return NULL;
}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_focus_next_manager_is(Eo *obj EINA_UNUSED, Elm_Popup_Data *_pd EINA_UNUSED)
{
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_focus_next(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, Elm_Focus_Direction dir, Evas_Object **next)
{
   Evas_Object *ao;
   Eina_List *items = NULL;
   Eina_List *base_items = NULL;

   /* access */
   if (_elm_config->access_mode)
     {
        if (sd->title_text)
          {
             ao = _access_object_get(obj, ACCESS_TITLE_PART);
             items = eina_list_append(items, ao);
          }

        ao = _access_object_get(obj, ACCESS_BODY_PART);
        if (ao) items = eina_list_append(items, ao);
     }

   eo_do(sd->main_layout, base_items = elm_obj_container_content_swallow_list_get());

   items = eina_list_merge(items, base_items);

   if (!elm_widget_focus_list_next_get(sd->main_layout, items, eina_list_data_get, dir, next))
     *next = sd->main_layout;
   eina_list_free(items);

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_focus_direction_manager_is(Eo *obj EINA_UNUSED, Elm_Popup_Data *_pd EINA_UNUSED)
{
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_focus_direction(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, const Evas_Object *base, double degree, Evas_Object **direction, double *weight)
{
   Evas_Object *ao;
   Eina_List *items = NULL;
   Eina_List *base_items = NULL;

   /* access */
   if (_elm_config->access_mode)
     {
        if (sd->title_text)
          {
             ao = _access_object_get(obj, ACCESS_TITLE_PART);
             items = eina_list_append(items, ao);
          }

        ao = _access_object_get(obj, ACCESS_BODY_PART);
        if (ao) items = eina_list_append(items, ao);
     }

   eo_do(sd->main_layout, base_items = elm_obj_container_content_swallow_list_get());

   items = eina_list_merge(items, base_items);

   elm_widget_focus_list_direction_get
     (sd->main_layout, base, items, eina_list_data_get, degree, direction, weight);
   eina_list_free(items);

   return EINA_TRUE;
}

//TIZEN_ONLY(20150709) : add keybinding for elm_popup "block,clicked" action
static Eina_Bool
_key_action_escape(Evas_Object *obj, const char *params EINA_UNUSED)
{
   evas_object_smart_callback_call(obj, SIG_BLOCK_CLICKED, NULL);
   return EINA_TRUE;
}
//

static Eina_Bool
_key_action_move(Evas_Object *obj, const char *params)
{
   const char *dir = params;

   if (!strcmp(dir, "previous"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_PREVIOUS);
   else if (!strcmp(dir, "next"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_NEXT);
   else if (!strcmp(dir, "left"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_LEFT);
   else if (!strcmp(dir, "right"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_RIGHT);
   else if (!strcmp(dir, "up"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_UP);
   else if (!strcmp(dir, "down"))
     elm_widget_focus_cycle(obj, ELM_FOCUS_DOWN);
   else return EINA_FALSE;

   return EINA_TRUE;

}

EOLIAN static Eina_Bool
_elm_popup_elm_widget_event(Eo *obj, Elm_Popup_Data *_pd EINA_UNUSED, Evas_Object *src, Evas_Callback_Type type, void *event_info)
{
   (void)src;
   Evas_Event_Key_Down *ev = event_info;

   if (type != EVAS_CALLBACK_KEY_DOWN) return EINA_FALSE;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return EINA_FALSE;

   if (!_elm_config_key_binding_call(obj, ev, key_actions))
     return EINA_FALSE;

   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   return EINA_TRUE;
}

//TIZEN_ONLY(20150204): add scroller of content text of popup
static void
_create_scroller(Evas_Object *obj)
{
   ELM_POPUP_DATA_GET(obj, sd);

   //table
   sd->tbl = elm_table_add(sd->main_layout);
   evas_object_event_callback_add(sd->tbl, EVAS_CALLBACK_DEL,
                                  _on_table_del, obj);

   //spacer
   sd->spacer = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_color_set(sd->spacer, 0, 0, 0, 0);
   elm_table_pack(sd->tbl, sd->spacer, 0, 0, 1, 1);

   //Scroller
   sd->scr = elm_scroller_add(sd->tbl);
   elm_object_style_set(sd->scr, "effect");
   evas_object_size_hint_weight_set(sd->scr, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sd->scr, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_content_min_limit(sd->scr, EINA_TRUE, EINA_FALSE);
   elm_scroller_bounce_set(sd->scr, EINA_FALSE, EINA_TRUE);
   evas_object_event_callback_add(sd->scr, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _size_hints_changed_cb, obj);
   elm_table_pack(sd->tbl, sd->scr, 0, 0, 1, 1);
   evas_object_show(sd->scr);
}
//

//TIZEN_ONLY(20150305): fix popup align and size problem
static void
_notify_resize_cb(void *data,
                  Evas *e EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;

   ELM_POPUP_CHECK(popup);

   _scroller_size_calc(popup);
   elm_layout_sizing_eval(popup);
}
//

//TIZEN_ONLY(20150305): add resize callback for rotation
static void _rotation_changed_cb(void *data,
                               Evas_Object *o EINA_UNUSED,
                               const char *emission EINA_UNUSED,
                               const char *source EINA_UNUSED)
{
   Evas_Object *popup = data;

   ELM_POPUP_CHECK(popup);

   _scroller_size_calc(popup);
   elm_layout_sizing_eval(popup);
}
//

//TIZEN_ONLY(20150827): add display mode configure to check compress mode
static void
_on_obj_size_hints_changed(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                           Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evas_Display_Mode dispmode;

   ELM_POPUP_DATA_GET(obj, sd);

   dispmode = evas_object_size_hint_display_mode_get(obj);
   if (sd->dispmode == dispmode) return;

   sd->dispmode = dispmode;

   _scroller_size_calc(obj);
   elm_layout_sizing_eval(obj);
}
//

EOLIAN static void
_elm_popup_evas_object_smart_add(Eo *obj, Elm_Popup_Data *priv)
{
   char style[1024];

   eo_do_super(obj, MY_CLASS, evas_obj_smart_add());
   elm_widget_sub_object_parent_add(obj);

   // TIZEN_ONLY(20150121): Change style name of popup groups.
   //snprintf(style, sizeof(style), "popup/%s", elm_widget_style_get(obj));
   snprintf(style, sizeof(style), "%s", elm_widget_style_get(obj));
   //

   //TIZEN_ONLY(20150305): add a allow eval flag not to call another sizing eval during sizing eval
   priv->allow_eval = EINA_TRUE;
   //

   //TIZEN_ONLY(20150806): support scrollable content
   //FIXME: genlist only now, it should be changed to support other scrollable.
   priv->scrollable_content = EINA_FALSE;
   //

   //TIZEN_ONLY(20150827): add display mode configure to check compress mode
   priv->dispmode = evas_object_size_hint_display_mode_get(obj);
   //

   priv->notify = elm_notify_add(obj);

   //TIZEN_ONLY(20150305): fix popup align and size problem
   //elm_object_style_set(priv->notify, style);
   elm_object_style_set(priv->notify, "popup");
   //

   //TIZEN_ONLY(20150723): add popup default align
   //elm_notify_align_set(priv->notify, 0.5, 0.5);
   elm_notify_align_set(priv->notify,
                        _elm_config->popup_horizontal_align,
                        _elm_config->popup_vertical_align);
   //
   elm_notify_allow_events_set(priv->notify, EINA_FALSE);
   evas_object_size_hint_weight_set
     (priv->notify, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set
     (priv->notify, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_member_add(priv->notify, obj);

   evas_object_smart_callback_add(priv->notify, "block,clicked", _block_clicked_cb, obj);
   evas_object_smart_callback_add(priv->notify, "timeout", _timeout_cb, obj);
   evas_object_smart_callback_add(priv->notify, "show,finished", _show_finished_cb, obj);
   evas_object_smart_callback_add(priv->notify, "dismissed", _hide_effect_finished_cb, obj);
   //TIZEN_ONLY(20150305): fix popup align and size problem
   evas_object_event_callback_add
     (priv->notify, EVAS_CALLBACK_RESIZE, _notify_resize_cb, obj);
   //

   priv->main_layout = elm_layout_add(obj);
   if (!elm_layout_theme_set(priv->main_layout, "popup", "base",
                             elm_widget_style_get(obj)))
     ERR("Failed to set layout!");

   //TIZEN_ONLY(20150204): add scroller of content text of popup
   _create_scroller(obj);
   elm_layout_content_set(priv->main_layout, "elm.swallow.content", priv->tbl);
   _scroller_size_calc(obj);
   //

   elm_object_content_set(priv->notify, priv->main_layout);

   evas_object_event_callback_add(obj, EVAS_CALLBACK_SHOW, _on_show, NULL);

   evas_object_event_callback_add
     (obj, EVAS_CALLBACK_RESTACK, _restack_cb, NULL);

   //TIZEN_ONLY(20150827): add display mode configure to check compress mode
   evas_object_event_callback_add(obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _on_obj_size_hints_changed, NULL);
   //

   elm_layout_signal_callback_add
     (priv->main_layout, "elm,state,title_area,visible", "elm", _layout_change_cb, NULL);
   elm_layout_signal_callback_add
     (priv->main_layout, "elm,state,title_area,hidden", "elm", _layout_change_cb, NULL);
   elm_layout_signal_callback_add
     (priv->main_layout, "elm,state,action_area,visible", "elm", _layout_change_cb, NULL);
   elm_layout_signal_callback_add
     (priv->main_layout, "elm,state,action_area,hidden", "elm", _layout_change_cb, NULL);
   //TIZEN_ONLY(20150305): add resize callback for rotation
   elm_layout_signal_callback_add
     (priv->main_layout, "elm,state,orientation,changed", "", _rotation_changed_cb, obj);
   //

   priv->content_area = elm_layout_add(priv->main_layout);
   if (!elm_layout_theme_set(priv->content_area, "popup", "content", style))
     ERR("Failed to set layout!");
   else
   //TIZEN_ONLY(20150305): fix popup align and size problem
   //  evas_object_event_callback_add
   //     (priv->content_area, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
   //      _size_hints_changed_cb, priv->main_layout);
   evas_object_event_callback_add
        (priv->content_area, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
         _size_hints_changed_cb, obj);
   //

   priv->content_text_wrap_type = ELM_WRAP_MIXED;

   elm_widget_can_focus_set(obj, EINA_TRUE);
   elm_widget_can_focus_set(priv->main_layout, EINA_TRUE);

   _visuals_set(obj);
   edje_object_message_signal_process(elm_layout_edje_get(priv->main_layout));
}

static void
_parent_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evas_Coord x, y, w, h;
   Evas_Object *popup = data;

   evas_object_geometry_get(obj, &x, &y, &w, &h);

   evas_object_move(popup, x, y);
   evas_object_resize(popup, w, h);
}

EOLIAN static void
_elm_popup_elm_widget_parent_set(Eo *obj, Elm_Popup_Data *sd EINA_UNUSED, Evas_Object *parent)
{
   Evas_Coord x, y, w, h;
   evas_object_geometry_get(parent, &x, &y, &w, &h);

   if (eo_isa(parent, ELM_WIN_CLASS))
     {
        x = 0;
        y = 0;
     }
   evas_object_move(obj, x, y);
   evas_object_resize(obj, w, h);

   sd->parent = parent;
   evas_object_event_callback_add(parent, EVAS_CALLBACK_RESIZE, _parent_resize_cb, obj);
}

EOLIAN static void
_elm_popup_elm_widget_access(Eo *obj, Elm_Popup_Data *_pd EINA_UNUSED, Eina_Bool is_access)
{
   _access_obj_process(obj, is_access);
}

EAPI Evas_Object *
elm_popup_add(Evas_Object *parent)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, NULL);

   UPF_TRACE_BEGIN_F(UPF_TRACE_TAG_ELEMENTARY, "elm_popup_add: parent(%p)", parent);

   Evas_Object *obj = eo_add(MY_CLASS, parent);

   UPF_TRACE_END(UPF_TRACE_TAG_ELEMENTARY);

   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd, NULL);
   wd->highlight_root = EINA_TRUE;

   return obj;
}

EOLIAN static Eo *
_elm_popup_eo_base_constructor(Eo *obj, Elm_Popup_Data *_pd EINA_UNUSED)
{
   obj = eo_do_super_ret(obj, MY_CLASS, obj, eo_constructor());
   eo_do(obj,
         evas_obj_type_set(MY_CLASS_NAME_LEGACY),
         evas_obj_smart_callbacks_descriptions_set(_smart_callbacks),
         elm_interface_atspi_accessible_role_set(ELM_ATSPI_ROLE_DIALOG));

   return obj;
}

EOLIAN static void
_elm_popup_content_text_wrap_type_set(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, Elm_Wrap_Type wrap)
{
   //Need to wrap the content text, so not allowing ELM_WRAP_NONE
   if (sd->content_text_wrap_type == ELM_WRAP_NONE) return;

   sd->content_text_wrap_type = wrap;
   if (sd->text_content_obj)
     elm_label_line_wrap_set(sd->text_content_obj, wrap);
}

EOLIAN static Elm_Wrap_Type
_elm_popup_content_text_wrap_type_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   return sd->content_text_wrap_type;
}

/* keeping old externals orient api for notify, but taking away the
 * introduced deprecation warning by copying the deprecated code
 * here */
static Elm_Notify_Orient
_elm_notify_orient_get(const Evas_Object *obj)
{
   Elm_Notify_Orient orient;
   double horizontal, vertical;

   elm_notify_align_get(obj, &horizontal, &vertical);

   if ((horizontal == 0.5) && (vertical == 0.0))
     orient = ELM_NOTIFY_ORIENT_TOP;
   else if ((horizontal == 0.5) && (vertical == 0.5))
     orient = ELM_NOTIFY_ORIENT_CENTER;
   else if ((horizontal == 0.5) && (vertical == 1.0))
     orient = ELM_NOTIFY_ORIENT_BOTTOM;
   else if ((horizontal == 0.0) && (vertical == 0.5))
     orient = ELM_NOTIFY_ORIENT_LEFT;
   else if ((horizontal == 1.0) && (vertical == 0.5))
     orient = ELM_NOTIFY_ORIENT_RIGHT;
   else if ((horizontal == 0.0) && (vertical == 0.0))
     orient = ELM_NOTIFY_ORIENT_TOP_LEFT;
   else if ((horizontal == 1.0) && (vertical == 0.0))
     orient = ELM_NOTIFY_ORIENT_TOP_RIGHT;
   else if ((horizontal == 0.0) && (vertical == 1.0))
     orient = ELM_NOTIFY_ORIENT_BOTTOM_LEFT;
   else if ((horizontal == 1.0) && (vertical == 1.0))
     orient = ELM_NOTIFY_ORIENT_BOTTOM_RIGHT;
   else
     orient = ELM_NOTIFY_ORIENT_TOP;
   return orient;
}

static void
_elm_notify_orient_set(Evas_Object *obj,
                       Elm_Notify_Orient orient)
{
   double horizontal = 0, vertical = 0;

   switch (orient)
     {
      case ELM_NOTIFY_ORIENT_TOP:
         horizontal = 0.5; vertical = 0.0;
        break;

      case ELM_NOTIFY_ORIENT_CENTER:
         horizontal = 0.5; vertical = 0.5;
        break;

      case ELM_NOTIFY_ORIENT_BOTTOM:
         horizontal = 0.5; vertical = 1.0;
        break;

      case ELM_NOTIFY_ORIENT_LEFT:
         horizontal = 0.0; vertical = 0.5;
        break;

      case ELM_NOTIFY_ORIENT_RIGHT:
         horizontal = 1.0; vertical = 0.5;
        break;

      case ELM_NOTIFY_ORIENT_TOP_LEFT:
         horizontal = 0.0; vertical = 0.0;
        break;

      case ELM_NOTIFY_ORIENT_TOP_RIGHT:
         horizontal = 1.0; vertical = 0.0;
        break;

      case ELM_NOTIFY_ORIENT_BOTTOM_LEFT:
         horizontal = 0.0; vertical = 1.0;
        break;

      case ELM_NOTIFY_ORIENT_BOTTOM_RIGHT:
         horizontal = 1.0; vertical = 1.0;
        break;

      case ELM_NOTIFY_ORIENT_LAST:
        break;
     }

   elm_notify_align_set(obj, horizontal, vertical);
}

EOLIAN static void
_elm_popup_orient_set(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, Elm_Popup_Orient orient)
{
   if (orient >= ELM_POPUP_ORIENT_LAST) return;
   _elm_notify_orient_set(sd->notify, (Elm_Notify_Orient)orient);
}

EOLIAN static Elm_Popup_Orient
_elm_popup_orient_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   return (Elm_Popup_Orient)_elm_notify_orient_get(sd->notify);
}

EOLIAN static void
_elm_popup_align_set(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, double horizontal, double vertical)
{
   elm_notify_align_set(sd->notify, horizontal, vertical);
}

EOLIAN static void
_elm_popup_align_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, double *horizontal, double *vertical)
{
   elm_notify_align_get(sd->notify, horizontal, vertical);
}

EOLIAN static void
_elm_popup_timeout_set(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, double timeout)
{
   elm_notify_timeout_set(sd->notify, timeout);
}

EOLIAN static double
_elm_popup_timeout_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   return elm_notify_timeout_get(sd->notify);
}

EOLIAN static void
_elm_popup_allow_events_set(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd, Eina_Bool allow)
{
   Eina_Bool allow_events = !!allow;

   elm_notify_allow_events_set(sd->notify, allow_events);
}

EOLIAN static Eina_Bool
_elm_popup_allow_events_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   return elm_notify_allow_events_get(sd->notify);
}

EOLIAN static Elm_Object_Item*
_elm_popup_item_append(Eo *obj, Elm_Popup_Data *sd, const char *label, Evas_Object *icon, Evas_Smart_Cb func, const void *data)
{
   Evas_Object *prev_content;
   Eo *eo_it;

   eo_it = eo_add(ELM_POPUP_ITEM_CLASS, obj);
   if (!eo_it) return NULL;
   ELM_POPUP_ITEM_DATA_GET(eo_it, it);
   if (sd->content || sd->text_content_obj)
     {
        eo_do(sd->content_area, prev_content =
                             elm_obj_container_content_get(CONTENT_PART));
        evas_object_del(prev_content);
     }

   //The first item is appended.
   //TIZEN_ONLY(20150204): add scroller of content text of popup
   //if (!sd->items)
   //  _list_add(obj);
   //

   it->func = func;
   WIDGET_ITEM_DATA_SET(eo_it, data);

   _item_new(it);
   _item_icon_set(it, icon);
   _item_text_set(it, label);

   elm_box_pack_end(sd->box, VIEW(it));
   sd->items = eina_list_append(sd->items, it);

   _scroller_size_calc(obj);
   elm_layout_sizing_eval(obj);

   return eo_it;
}

EOLIAN static void
_elm_popup_dismiss(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   elm_layout_signal_emit(sd->main_layout, "elm,state,hide", "elm");
   elm_notify_dismiss(sd->notify);
}

static void
_elm_popup_class_constructor(Eo_Class *klass)
{
   evas_smart_legacy_type_register(MY_CLASS_NAME_LEGACY, klass);
}

EOLIAN const Elm_Atspi_Action *
_elm_popup_elm_interface_atspi_widget_action_elm_actions_get(Eo *obj EINA_UNUSED, Elm_Popup_Data *pd EINA_UNUSED)
{
   static Elm_Atspi_Action atspi_actions[] = {
//TIZEN_ONLY(20150709) : add keybinding for elm_popup "block,clicked" action
          { "escape", "escape", NULL, _key_action_escape},
          { NULL, NULL, NULL, NULL }
   };
   if (pd->action_area)
     return NULL;
///

   return &atspi_actions[0];
}

EOLIAN static Elm_Atspi_State_Set
_elm_popup_elm_interface_atspi_accessible_state_set_get(Eo *obj, Elm_Popup_Data *sd EINA_UNUSED)
{
   Elm_Atspi_State_Set ret;
   eo_do_super(obj, MY_CLASS, ret = elm_interface_atspi_accessible_state_set_get());

   STATE_TYPE_SET(ret, ELM_ATSPI_STATE_MODAL);

   return ret;
}

//TIZEN ONLY(20150708): popup and ctxpopup accessibility highlight impementation
EOLIAN static Eina_Bool
_elm_popup_elm_interface_atspi_component_highlight_grab(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   if (sd->scr)
     {
        elm_object_accessibility_highlight_set(sd->main_layout, EINA_TRUE);
        ///TIZEN_ONLY(20170717) : expose highlight information on atspi
        elm_interface_atspi_accessible_state_changed_signal_emit(obj, ELM_ATSPI_STATE_HIGHLIGHTED, EINA_TRUE);
        ///
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

EOLIAN static Eina_Bool
_elm_popup_elm_interface_atspi_component_highlight_clear(Eo *obj EINA_UNUSED, Elm_Popup_Data *sd)
{
   if (sd->scr)
     {
        elm_object_accessibility_highlight_set(sd->main_layout, EINA_FALSE);
        ///TIZEN_ONLY(20170717) : expose highlight information on atspi
        elm_interface_atspi_accessible_state_changed_signal_emit(obj, ELM_ATSPI_STATE_HIGHLIGHTED, EINA_FALSE);
        ///
        return EINA_TRUE;
     }
   return EINA_FALSE;
}
//
#include "elm_popup.eo.c"
#include "elm_popup_item.eo.c"
