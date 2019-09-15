/*
 * cairotext
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Rajendra Kolli <raj.kolli@samsung.com>
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

/**
 * SECTION:element-cairotext
 *
 * FIXME:Describe cairotext here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m videotestsrc ! cairotext ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */
#include <stdio.h>
#include <stdlib.h>
#include <cairo.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "gstcairotext.h"

#define DEFAULT_PROP_XPOS         110
#define DEFAULT_PROP_YPOS         110
#define DEFAULT_PROP_FONT         "DejaVu"
#define DEFAULT_RENDER_WIDTH      800
#define DEFAULT_RENDER_HEIGHT     480
#define DEFAULT_PROP_COLOR        0xffffffff
#define DEFAULT_PROP_FONT_SIZE    30

#define DEFAULT_PROP_TEXT   ""
#define GST_CAIROTEXT_CAPS         \
  "video/x-raw, "                        \
  "format =(string) ARGB, "              \
  "width  =(int)  [16, 4096], "          \
  "height =(int)  [16, 4096], "          \
  "framerate = (fraction) [0/1,4096/1];"

/* Filter signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_TEXT,
  PROP_SCILENTPROP_0,
  PROP_HALIGNMENT,
  PROP_VALIGNMENT,
  PROP_LINE_ALIGNMENT,
  PROP_XPOS,
  PROP_YPOS,
  PROP_FONT,
  PROP_FONT_SIZE,
  PROP_FONT_STYLE,
  PROP_BUFFER_DEPTH,
  PROP_COLOR,
  PROP_ENABLE_OVERLAY
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_CAIROTEXT_CAPS)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_CAIROTEXT_CAPS)
    );

GST_DEBUG_CATEGORY_STATIC(gst_cairotext_debug);
#define GST_CAT_DEFAULT gst_cairotext_debug

#define gst_cairotext_parent_class parent_class
G_DEFINE_TYPE (Gstcairotext, gst_cairotext, GST_TYPE_BASE_TRANSFORM);

static void                 gst_cairotext_set_property (GObject * object, guint prop_id,const GValue * value, GParamSpec * pspec);
static void                 gst_cairotext_get_property (GObject * object, guint prop_id,GValue * value, GParamSpec * pspec);
static void                 gst_cairotext_finalize (GObject * object);
static GstFlowReturn        gst_cairotext_transform_ip (GstBaseTransform * base, GstBuffer * outbuf);

/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
gst_cairotext_class_init (GstcairotextClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_cairotext_set_property;
  gobject_class->get_property = gst_cairotext_get_property;

  gst_element_class_set_details_simple (gstelement_class,
  "Cairo text plugin",
  "Utility/Video",
  "Utility for overlaying text on video frames",
  "Samsung Electronics <www.samsung.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

 gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_cairotext_finalize);
 g_object_class_install_property (gobject_class, PROP_FONT,
      g_param_spec_string ("font", "font name","font name to be used for rendering. eg.DejaVu, courier, utopia  for syntax.",
      DEFAULT_PROP_FONT, G_PARAM_WRITABLE ));

  g_object_class_install_property (gobject_class, PROP_XPOS,
      g_param_spec_int ("xpos", "horizontal position","Horizontal position when using position alignment", 0, 4096, DEFAULT_PROP_XPOS,
      G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_YPOS,
      g_param_spec_int ("ypos", "vertical position","Vertical position when using position alignment", 0, 4096, DEFAULT_PROP_YPOS,
      G_PARAM_READWRITE ));
  g_object_class_install_property (gobject_class, PROP_TEXT,
      g_param_spec_string ("text", "text","Text to be display.", DEFAULT_PROP_TEXT,
      G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_FONT_SIZE,
      g_param_spec_int("size", "font size","Font size of the text to be display.", 0, 200, DEFAULT_PROP_FONT_SIZE,
      G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_FONT_STYLE,
      g_param_spec_int ("style", "font style","Font style of the text to be display.eg, 0 -> EVAS_TEXT_STYLE_PLAIN refer enum _Evas_Text_Style_Type", 0, 112, 0,
      G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_COLOR,
      g_param_spec_uint("color", "font color","Font color of the text to be display eg: ARGB 65280(ffff0000).", 0, DEFAULT_PROP_COLOR, DEFAULT_PROP_COLOR,
      G_PARAM_READWRITE ));

  g_object_class_install_property (gobject_class, PROP_ENABLE_OVERLAY,
      g_param_spec_int("enableoverlay", "enable overlay", "enable or disable text overlay", 0, 1, 1,
      G_PARAM_READWRITE ));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_cairotext_transform_ip);

  GST_DEBUG_CATEGORY_INIT(gst_cairotext_debug, "cairotext", 0, "Cairo text plugin");
  GST_DEBUG("Cairotext class init");
}
static void
gst_cairotext_init (Gstcairotext *overlay)
{
  overlay->enable_overlay = TRUE;
  overlay->color = DEFAULT_PROP_COLOR;
  overlay->font = strdup(DEFAULT_PROP_FONT);
  overlay->size = DEFAULT_PROP_FONT_SIZE;
  overlay->text = NULL;
  overlay->width = -1;
  overlay->height = -1;

  gst_base_transform_set_passthrough(GST_BASE_TRANSFORM_CAST(overlay),FALSE);

  GST_DEBUG("Cairo text init");
}

static void
gst_cairotext_set_property (GObject * object, guint prop_id,  const GValue * value, GParamSpec * pspec)
{
  Gstcairotext *overlay = GST_CAIROTEXT (object);
  GST_DEBUG("set property called for ID: %d", prop_id);
   switch (prop_id) {
    case PROP_XPOS:
      overlay->xpos = g_value_get_int (value);
      break;
    case PROP_YPOS:
      overlay->ypos = g_value_get_int (value);
      break;
    case PROP_TEXT:
      //if(overlay->text) g_free (overlay->text);
      overlay->text = g_value_dup_string (value);
      break;
    case PROP_FONT_SIZE:
      overlay->size = g_value_get_int (value);
      break;
    case PROP_FONT_STYLE:
      overlay->style = g_value_get_int (value);
      break;
    case PROP_FONT:
      //if(overlay->font) g_free (overlay->font);
      overlay->font = g_value_dup_string (value);
      break;
    case PROP_COLOR:
      overlay->color = g_value_get_uint (value);
      break;
    case PROP_ENABLE_OVERLAY:
      overlay->enable_overlay = g_value_get_int (value);
      if(!overlay->enable_overlay)
      {
         gst_base_transform_set_passthrough(GST_BASE_TRANSFORM_CAST(overlay),TRUE);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cairotext_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  Gstcairotext *overlay = GST_CAIROTEXT (object);
  switch (prop_id) {
    case PROP_XPOS:
      g_value_set_int (value, overlay->xpos);
      break;
    case PROP_YPOS:
      g_value_set_int (value, overlay->ypos);
      break;
    case PROP_TEXT:
      g_value_set_string (value, overlay->text);
      break;
    case PROP_FONT_SIZE:
      g_value_set_int (value, overlay->size);
      break;
    case PROP_FONT_STYLE:
      g_value_set_int (value, overlay->style);
      break;
    case PROP_FONT:
      g_value_set_string (value, overlay->font);
      break;
    case PROP_COLOR:
      g_value_set_uint (value, overlay->color );
      break;
    case PROP_ENABLE_OVERLAY:
      g_value_set_int (value, overlay->enable_overlay);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_cairotext_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  Gstcairotext *overlay;
  overlay = GST_CAIROTEXT (base);

  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_font_extents_t fe;
  cairo_text_extents_t te;
  GstMapInfo buf_info = GST_MAP_INFO_INIT;
  GstMemory *mem;
  int stride;
  double a,r,g,b;

  GstFlowReturn ret = GST_FLOW_OK;

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (overlay), GST_BUFFER_TIMESTAMP (outbuf));

  if(overlay->enable_overlay == 0)
	  return ret;
  if(overlay->width == -1 || overlay->height ==-1)
  {
    GstCaps *configured_caps = NULL;
    configured_caps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(base));
    GstStructure *s_in = NULL;
    s_in = gst_caps_get_structure(configured_caps, 0);
    if(s_in) {
        gst_structure_get_int(s_in, "width", &overlay->width);
        gst_structure_get_int(s_in, "height", &overlay->height);
        GST_INFO("width and Height from parsed info is %d %d", overlay->width, overlay->height);
    }
    gst_caps_unref(configured_caps);
  }
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, overlay->width);

  GST_INFO("stride obtained for %d is %d",overlay->width,stride);

  mem = gst_buffer_peek_memory(outbuf, 0);
  gst_memory_map (mem, &buf_info, GST_MAP_READ);

  if(buf_info.data == NULL)
  {
	  GST_ERROR("Buffer information is null");
	  ret = GST_FLOW_ERROR;
	  goto EXIT;
  }
  surface = cairo_image_surface_create_for_data (buf_info.data,CAIRO_FORMAT_ARGB32, overlay->width, overlay->height,stride);
  cr = cairo_create (surface);
  a = 1.0;
  r = (overlay->color >>16) & 0xff;
  g = (overlay->color >>8)  & 0xff;
  b = (overlay->color)      & 0xff;

  r = r/255.0;
  g = g/255.0;
  b = b/255.0;


  GST_INFO("R is %lf G is %lf B is %lf",r,g,b);

  cairo_set_font_size(cr,overlay->size);

  cairo_select_font_face(cr,overlay->font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

  cairo_set_source_rgba(cr,r,g,b,a);

  cairo_move_to(cr,overlay->xpos,overlay->ypos);

  cairo_show_text(cr,overlay->text);

  cairo_surface_flush(surface);

  cairo_destroy(cr);
  cairo_surface_destroy(surface);

EXIT:
  gst_memory_unmap(mem, &buf_info);
  return ret;
}


static void gst_cairotext_finalize (GObject * object)
{
  GST_DEBUG("finalize start");
  Gstcairotext *overlay = GST_CAIROTEXT (object);
  if(overlay->text) g_free(overlay->text);
  if(overlay->font) g_free(overlay->font);
  G_OBJECT_CLASS(parent_class)->finalize (object);
  GST_DEBUG(" finalize end");
}
#ifndef PACKAGE
#define PACKAGE "cairotext"
#endif

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean cairotext_init (GstPlugin * cairotext)
{
  GST_DEBUG_CATEGORY_INIT (gst_cairotext_debug, "cairotext",0, "cairotext");
  return gst_element_register (cairotext, "cairotext", GST_RANK_PRIMARY, GST_TYPE_CAIROTEXT);
}

/* gstreamer looks for this structure to register cairotexts
 *
 * exchange the string 'Template cairotext' with your cairotext description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    cairotext,
    "cairotext",
    cairotext_init,
    VERSION,
    "Proprietary",
    "Samsung Electronics Co",
    "http://www.samsung.com"
)
