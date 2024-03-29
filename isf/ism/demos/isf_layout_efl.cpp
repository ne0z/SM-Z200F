/*
 * ISF(Input Service Framework)
 *
 * ISF is based on SCIM 1.4.7 and extended for supporting more mobile fitable.
 * Copyright (c) 2012-2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Shuo Liu <shuo0805.liu@samsung.com>, Jihoon Kim <jihoon48.kim@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "isf_demo_efl.h"
#include "isf_layout_efl.h"
#include <Ecore_X.h>

struct _menu_item {
    const char *name;
    const char *guide_text;
    Elm_Input_Panel_Layout layout;
    int layout_variation;
};

static struct _menu_item _menu_its[] = {
    { N_("NORMAL LAYOUT"), N_("click to enter TEXT"), ELM_INPUT_PANEL_LAYOUT_NORMAL, 0 },
    { N_("NUMBER LAYOUT"), N_("click to enter NUMBER"), ELM_INPUT_PANEL_LAYOUT_NUMBER, 0 },
    { N_("EMAIL LAYOUT"), N_("click to enter EMAIL"), ELM_INPUT_PANEL_LAYOUT_EMAIL, 0 },
    { N_("URL LAYOUT"), N_("click to enter URL"), ELM_INPUT_PANEL_LAYOUT_URL, 0 },
    { N_("PHONENUMBER LAYOUT"), N_("click to enter PHONENUMBER"), ELM_INPUT_PANEL_LAYOUT_PHONENUMBER, 0 },
    { N_("IP LAYOUT"), N_("click to enter IP"), ELM_INPUT_PANEL_LAYOUT_IP, 0 },
    { N_("MONTH LAYOUT"), N_("click to enter MONTH"), ELM_INPUT_PANEL_LAYOUT_MONTH, 0 },
    { N_("NUMBERONLY LAYOUT"), N_("click to enter NUMBERONLY"), ELM_INPUT_PANEL_LAYOUT_NUMBERONLY, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY_VARIATION_NORMAL },
    { N_("NUMBERONLY - SIGNED"), N_("click to enter NUMBERONLY WITH SIGNED"), ELM_INPUT_PANEL_LAYOUT_NUMBERONLY, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY_VARIATION_SIGNED },
    { N_("NUMBERONLY - DECIMAL"), N_("click to enter NUMBERONLY WITH DECIMAL"), ELM_INPUT_PANEL_LAYOUT_NUMBERONLY, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY_VARIATION_DECIMAL },
    { N_("NUMBERONLY - SIGNED AND DECIMAL"), N_("click to enter NUMBERONLY WITH SIGNED AND DECIMAL"), ELM_INPUT_PANEL_LAYOUT_NUMBERONLY, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY_VARIATION_SIGNED_AND_DECIMAL },
    { N_("DATETIME LAYOUT"), N_("click to enter DATETIME"), ELM_INPUT_PANEL_LAYOUT_DATETIME, 0},
    { N_("PASSWORD LAYOUT"), N_("click to enter PASSWORD"), ELM_INPUT_PANEL_LAYOUT_PASSWORD, 0},
    { N_("PASSWORD NUMBERONLY LAYOUT"), N_("click to enter PASSWORD NUMBERONLY"), ELM_INPUT_PANEL_LAYOUT_PASSWORD, ELM_INPUT_PANEL_LAYOUT_PASSWORD_VARIATION_NUMBERONLY },
    { N_("Emoticon LAYOUT"), N_("click to enter Emoticon"), ELM_INPUT_PANEL_LAYOUT_EMOTICON, 0},
    { N_("TERMINAL LAYOUT"), N_("click to enter TERMINAL"), ELM_INPUT_PANEL_LAYOUT_TERMINAL, 0},

    /* do not delete below */
    { NULL, NULL, ELM_INPUT_PANEL_LAYOUT_NORMAL, 0 }
};

static Ecore_Event_Handler *prop_handler = NULL;

static void _back_key_cb (void *data, Evas_Object *obj, void *event_info)
{
    ecore_x_test_fake_key_press ("XF86Back");
}

static void _rotate_cb (void *data, Evas_Object *obj, void *event_info)
{
    struct appdata *ad = (struct appdata *)data;

    int angle = elm_win_rotation_get (ad->win_main);
    if (angle == 0) {
        elm_win_rotation_with_resize_set (ad->win_main, 270);
    } else if (angle == 270) {
        elm_win_rotation_with_resize_set (ad->win_main, 0);
    }
}

static void _input_panel_state_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    if (value == ECORE_IMF_INPUT_PANEL_STATE_SHOW) {
        ecore_imf_context_input_panel_geometry_get (ctx, &x, &y, &w, &h);
        LOGD ("Input panel is shown. ctx : %p\n", ctx);
        LOGD ("x : %d, y : %d, w : %d, h : %d\n", x, y, w, h);
    } else if (value == ECORE_IMF_INPUT_PANEL_STATE_HIDE) {
        LOGD ("Input panel is hidden. ctx : %p\n", ctx);
    } else if (value == ECORE_IMF_INPUT_PANEL_STATE_WILL_SHOW) {
        LOGD ("Input panel will be shown. ctx : %p\n", ctx);
    }
}

static void _input_panel_resize_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    ecore_imf_context_input_panel_geometry_get (ctx, &x, &y, &w, &h);
    LOGD ("x : %d, y : %d, w : %d, h : %d\n", x, y, w, h);
}

static void _shift_mode_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    if (value == ECORE_IMF_INPUT_PANEL_SHIFT_MODE_OFF) {
        LOGD ("Shift Mode : OFF\n");
    } else if (value == ECORE_IMF_INPUT_PANEL_SHIFT_MODE_ON) {
        LOGD ("Shift Mode : ON\n");
    }
}

static void _language_changed_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    char *locale = NULL;

    ecore_imf_context_input_panel_language_locale_get (ctx, &locale);

    LOGD ("language : %s\n", locale);

    if (locale)
        free (locale);
}

static void _candidate_panel_state_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    if (value == ECORE_IMF_CANDIDATE_PANEL_SHOW) {
        ecore_imf_context_candidate_panel_geometry_get (ctx, &x, &y, &w, &h);
        LOGD ("Candidate window is shown\n");
        LOGD ("x : %d, y : %d, w : %d, h : %d\n", x, y, w, h);
    } else if (value == ECORE_IMF_CANDIDATE_PANEL_HIDE) {
        LOGD ("Candidate window is hidden\n");
    }
}

static void _candidate_panel_geometry_changed_cb (void *data, Ecore_IMF_Context *ctx, int value)
{
    int x, y, w, h;

    ecore_imf_context_candidate_panel_geometry_get (ctx, &x, &y, &w, &h);
    LOGD ("ctx : %p, x : %d, y : %d, w : %d, h : %d\n", ctx, x, y, w, h);
}

static void
_key_down_cb (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Down *ev = (Evas_Event_Key_Down *)event_info;
    LOGD ("[evas key down] keyname : '%s', key : '%s', string : '%s', compose : '%s'\n", ev->keyname, ev->key, ev->string, ev->compose);
}

static void
_key_up_cb (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Up *ev = (Evas_Event_Key_Up *)event_info;
    LOGD ("[evas key up] keyname : '%s', key : '%s', string : '%s', compose : '%s'\n", ev->keyname, ev->key, ev->string, ev->compose);
}

static void
_print_keyboard_geometry (Ecore_X_Window xwin)
{
    int sx, sy, sw, sh;
    Ecore_X_Window zone;

    zone = ecore_x_e_illume_zone_get (xwin);

    if (!ecore_x_e_illume_keyboard_geometry_get (zone, &sx, &sy, &sw, &sh))
        sx = sy = sw = sh = 0;

    LOGD ("Keyboard geometry x : %d, y : %d, w : %d, h : %d\n", sx, sy, sw, sh);
}

static Eina_Bool
_prop_change_cb (void *data, int type, void *event)
{
    Ecore_X_Event_Window_Property *ev;
    ev = (Ecore_X_Event_Window_Property *)event;
    if (!ev) return ECORE_CALLBACK_PASS_ON;

    if (ev->atom == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_STATE) {
        LOGD ("[ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_STATE] ");
        _print_keyboard_geometry (ev->win);
    } else if (ev->atom == ECORE_X_ATOM_E_ILLUME_KEYBOARD_GEOMETRY) {
        LOGD ("[ECORE_X_ATOM_E_ILLUME_KEYBOARD_GEOMETRY] ");
        _print_keyboard_geometry (ev->win);
    }

    return ECORE_CALLBACK_PASS_ON;
}

static void entry_changed_cb (void *data, Evas_Object *obj, void *event_info)
{
    LOGD ("The text within the entry was changed.\n");
}

static void entry_preedit_changed_cb (void *data, Evas_Object *obj, void *event_info)
{
    LOGD ("The preedit string has changed.\n");
}

static void entry_cursor_changed_cb (void *data, Evas_Object *obj, void *event_info)
{
    LOGD ("cursor pos : %d\n", elm_entry_cursor_pos_get (obj));
}

static Evas_Object *_create_ef_layout (Evas_Object *parent, const char *label, const char *guide_text, Elm_Input_Panel_Layout layout, int layout_variation = 0)
{
    Evas_Object *en;
    Evas_Object *ef = create_ef (parent, label, guide_text, &en);
    if (!ef || !en) return NULL;

    Ecore_IMF_Context *ic = NULL;
    elm_entry_input_panel_layout_set (en, layout);
    elm_entry_input_panel_layout_variation_set (en, layout_variation);
    evas_object_event_callback_add (en, EVAS_CALLBACK_KEY_DOWN, _key_down_cb, NULL);
    evas_object_event_callback_add (en, EVAS_CALLBACK_KEY_UP, _key_up_cb, NULL);
    evas_object_smart_callback_add (en, "changed", entry_changed_cb, NULL);
    evas_object_smart_callback_add (en, "preedit,changed", entry_preedit_changed_cb, NULL);
    evas_object_smart_callback_add (en, "cursor,changed", entry_cursor_changed_cb, NULL);

    ic = (Ecore_IMF_Context *)elm_entry_imf_context_get (en);

    if (ic != NULL) {
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_STATE_EVENT, _input_panel_state_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_GEOMETRY_EVENT, _input_panel_resize_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_SHIFT_MODE_EVENT, _shift_mode_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_INPUT_PANEL_LANGUAGE_EVENT, _language_changed_cb, NULL);

        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_CANDIDATE_PANEL_STATE_EVENT, _candidate_panel_state_cb, NULL);
        ecore_imf_context_input_panel_event_callback_add (ic, ECORE_IMF_CANDIDATE_PANEL_GEOMETRY_EVENT, _candidate_panel_geometry_changed_cb, NULL);
    }

    return ef;
}

static void
_layout_del_cb (void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
    if (prop_handler) {
        ecore_event_handler_del (prop_handler);
        prop_handler = NULL;
    }
}

static Evas_Object * create_inner_layout (void *data)
{
    struct appdata *ad = (struct appdata *)data;
    Evas_Object *bx = NULL;
    Evas_Object *ef = NULL;
    Evas_Object *parent = ad->naviframe;
    int idx = 0;

    bx = elm_box_add (parent);
    evas_object_size_hint_weight_set (bx, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set (bx, EVAS_HINT_FILL, 0.0);
    evas_object_show (bx);

    while (_menu_its[idx].name != NULL) {
        ef = _create_ef_layout (parent, _menu_its[idx].name, _menu_its[idx].guide_text, _menu_its[idx].layout, _menu_its[idx].layout_variation);
        elm_box_pack_end (bx, ef);
        ++idx;
    }

    /* create back key event generation button */
    Evas_Object *back_key_btn = create_button (parent, "Generate BACK key");
    evas_object_smart_callback_add (back_key_btn, "clicked", _back_key_cb, (void *)ad);
    elm_box_pack_end (bx, back_key_btn);
    elm_object_focus_allow_set (back_key_btn, EINA_FALSE);

    /* Click to rotate button */
    Evas_Object *rotate_btn = create_button (parent, "rotate");
    evas_object_smart_callback_add (rotate_btn, "clicked", _rotate_cb, (void *)ad);
    elm_box_pack_end (bx, rotate_btn);

    prop_handler = ecore_event_handler_add (ECORE_X_EVENT_WINDOW_PROPERTY, _prop_change_cb, NULL);

    evas_object_event_callback_add (bx, EVAS_CALLBACK_DEL, _layout_del_cb, NULL);

    return bx;
}

void ise_layout_bt (void *data, Evas_Object *obj, void *event_info)
{
    Evas_Object *lay_inner = create_inner_layout (data);
    add_layout_to_naviframe (data, lay_inner, _("Layout"));
}

/*
vi:ts=4:ai:nowrap:expandtab
*/
