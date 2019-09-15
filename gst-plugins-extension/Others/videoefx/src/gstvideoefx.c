/*
 * Copyright (C) 2015 Rajendra Kolli <raj.kolli@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-videoefx
 *
 * FIXME:Adds effects to a given video frame.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! videoefx ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstvideoefx.h"

GST_DEBUG_CATEGORY_STATIC (gst_videoefx_debug);
#define GST_CAT_DEFAULT gst_videoefx_debug

#define VIDEOEFX_DEFAULT_EFFECT VIDEO_EFFECT_GS_BW

#define GST_TYPE_VIDEOEFX_EFFECT (gst_videoefx_get_effect_type())

#define GST_VIDEOEFX_SINKCAPS         \
  "video/x-raw, "                        \
  "format =(string) {I420,SN12}, "              \
  "width  =(int)  [16, 4096], "          \
  "height =(int)  [16, 4096], "          \
  "framerate = (fraction) [0/1,4096/1];"

#define GST_VIDEOEFX_SRCCAPS         \
  "video/x-raw, "                        \
  "format =(string) {ARGB,SR32}, "              \
  "width  =(int)  [16, 4096], "          \
  "height =(int)  [16, 4096], "          \
  "framerate = (fraction) [0/1,4096/1];"


enum
{
  PROP_0,
  PROP_SILENT,
  PROP_PASSTHROUGH,
  PROP_EFFECT,
  PROP_ENABLE_HARDWARE
};

//static GstElementClass parent_class = NULL;

/* the capabilities of the inputs and outputs.*/
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEOEFX_SINKCAPS)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEOEFX_SRCCAPS)
    );

#define gst_videoefx_parent_class parent_class
G_DEFINE_TYPE (GstVideoEfx, gst_videoefx, GST_TYPE_ELEMENT);

static void         gst_videoefx_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void             gst_videoefx_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static              gboolean gst_videoefx_set_caps (GstPad *pad, GstCaps *caps);
static void             gst_videoefx_finalize(GObject *object);
static GstStateChangeReturn     gst_videoefx_change_state (GstElement * element, GstStateChange transition);
static gboolean         gst_videoefx_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn        gst_videoefx_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

static GType gst_videoefx_get_effect_type (void)
{
    startfunc
    static GType effect_type = 0;
    static GEnumValue effect_types[] = {
    {VIDEO_EFFECT_GS_NO_EFFECT, "NO EFFECT", "NO EFFECT"},
    {VIDEO_EFFECT_GS_SEPIA, "SEPIA", "SEPIA"},
    {VIDEO_EFFECT_GS_BW, "BLACKWHITE", "BLACKWHITE"},
    {VIDEO_EFFECT_GS_VINTAGE, "VINTAGE", "VINTAGE"},
    {VIDEO_EFFECT_GS_FADEDCOLOR, "FADEDCOLOR", "FADEDCOLOR"},
    {VIDEO_EFFECT_GS_TINT, "TINT", "TINT"},
    {VIDEO_EFFECT_GS_TURQUISE, "TURQUISE", "TURQUISE"},
    {VIDEO_EFFECT_GS_VIGNETTE, "VIGNETTE", "VIGNETTE"},
    {VIDEO_EFFECT_GS_MOODY, "MOODY", "MOODY"},
    {VIDEO_EFFECT_GS_RUGGED, "RUGGED", "RUGGED"},
    {VIDEO_EFFECT_GS_COMIC, "COMIC", "COMIC"},
    {VIDEO_EFFECT_GS_FISHEYE, "FISHEYE", "FISHEYE"},
    {VIDEO_EFFECT_GS_MAX, NULL, NULL},
    };
    if (!effect_type) {
        effect_type = g_enum_register_static ("GstVideoEfxEffectType", effect_types);
    }
    endfunc
    return effect_type;
}

/* GObject vmethod implementations */

/* initialize the videoefx's class */
static void
gst_videoefx_class_init (GstVideoEfxClass * klass)
{
    startfunc

    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_videoefx_finalize);
    gobject_class->set_property = gst_videoefx_set_property;
    gobject_class->get_property = gst_videoefx_get_property;

    g_object_class_install_property (gobject_class, PROP_SILENT,
        g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_PASSTHROUGH,
        g_param_spec_boolean ("passthrough", "Passthrough", "Pass through without applying effect ",
          FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_ENABLE_HARDWARE,
        g_param_spec_boolean ("enable_hardware", "Enable Hardware", " Enable Hardware Acceleration",
          FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_EFFECT,
          g_param_spec_enum ("effect","Set effect type","effect type",
              GST_TYPE_VIDEOEFX_EFFECT,
              VIDEOEFX_DEFAULT_EFFECT,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_details_simple(gstelement_class,
      "VideoEfx",
      "Utiltiy/Video",
      "Utility to apply effects over decoded video frames",
      "Rajendra Kolli <raj.kolli@samsung.com>");

    gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_videoefx_change_state);

    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&sink_factory));

    endfunc
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_videoefx_init (GstVideoEfx * filter)
{
    startfunc

    filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_videoefx_sink_event));
    gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_videoefx_chain));
    gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

    filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    gst_pad_use_fixed_caps (filter->srcpad);
    gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

    filter->silent = FALSE;
    filter->passthrough = FALSE;
    filter->enable_hardware = FALSE;
    filter->effect_changed = FALSE;
    filter->app_data = NULL;
    filter->image_filter_handle = NULL;

    endfunc
}

static void
gst_videoefx_set_effect(GstVideoEfx *filter){
    startfunc
    switch (filter->effect){
      case VIDEO_EFFECT_GS_SEPIA:
        filter->image_filter_effect = IMAGE_FILTER_EFFECT_SEPIA;
    break;
      case VIDEO_EFFECT_GS_BW:
        filter->image_filter_effect = IMAGE_FILTER_EFFECT_GREY_SCALE;
    break;
      case VIDEO_EFFECT_GS_FADEDCOLOR:
        filter->image_filter_effect = IMAGE_FILTER_EFFECT_FADED_COLOR;
        break;
      case VIDEO_EFFECT_GS_VINTAGE:
        filter->image_filter_effect = IMAGE_FILTER_EFFECT_VINTAGE;
        break;
      case VIDEO_EFFECT_GS_TINT:
        filter->image_filter_effect = IMAGE_FILTER_EFFECT_TINT;
        break;
      case VIDEO_EFFECT_GS_MOODY:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_MOODY;
    break;
      case VIDEO_EFFECT_GS_RUGGED:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_RUGGED;
    break;
      case VIDEO_EFFECT_GS_TURQUISE:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_TURQUISE;
    break;
      case VIDEO_EFFECT_GS_COMIC:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_CARTOON;
    break;
      case VIDEO_EFFECT_GS_FISHEYE:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_FISHEYE;
    break;
      case VIDEO_EFFECT_GS_VIGNETTE:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_VIGNETTE;
    break;
      default:
    filter->image_filter_effect = IMAGE_FILTER_EFFECT_NO_EFFECT;
    break;
    }
    filter->effect_changed = TRUE;
    endfunc
}

static void
gst_videoefx_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    startfunc
    GstVideoEfx *filter = GST_VIDEOEFX (object);

    switch (prop_id) {
      case PROP_SILENT:
        filter->silent = g_value_get_boolean (value);
        break;
      case PROP_EFFECT:
        filter->effect = g_value_get_enum(value);
        gst_videoefx_set_effect(filter);
        break;
      case PROP_PASSTHROUGH:
        filter->passthrough = g_value_get_boolean(value);
        break;
      case PROP_ENABLE_HARDWARE:
        filter->enable_hardware = g_value_get_boolean(value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

    endfunc
}

static void
gst_videoefx_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
    startfunc
    GstVideoEfx *filter = GST_VIDEOEFX (object);

    switch (prop_id) {
      case PROP_SILENT:
        g_value_set_boolean (value, filter->silent);
        break;
      case PROP_EFFECT:
        g_value_set_enum(value, filter->effect);
        break;
      case PROP_PASSTHROUGH:
        g_value_set_boolean(value, filter->passthrough);
        break;
      case PROP_ENABLE_HARDWARE:
        g_value_set_boolean(value, filter->enable_hardware);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
    endfunc
}


static gboolean
gst_videoefx_set_caps (GstPad *pad, GstCaps *caps)
{
    GstVideoEfx *filter = GST_VIDEOEFX(GST_PAD_PARENT(pad));
    GstStructure *s_in = NULL;
    int in_width=0, in_height=0, in_fps_num=0, in_fps_den=0;
    const GValue *framerate;
    gchar *format = NULL;
    GstCaps *newcaps = NULL;
    int result;
    GST_DEBUG("videoefx set caps");
    s_in = gst_caps_get_structure(caps, 0);
    if(s_in) {
        gst_structure_get_int(s_in, "width", &in_width);
    gst_structure_get_int(s_in, "height", &in_height);
    format = gst_structure_get_string(s_in, "format");
    framerate = gst_structure_get_value (s_in, "framerate");
    in_fps_num = gst_value_get_fraction_numerator (framerate);
    in_fps_den = gst_value_get_fraction_denominator (framerate);
    filter->width = in_width;
    filter->height = in_height;
    GST_INFO("width and Height from parsed info is %d %d", filter->width, filter->height);
    }
    if(format == NULL || in_fps_num <=0 || in_fps_den <=0 || in_width <=0 || in_height <=0 )
    {
        GST_ERROR("Input caps are erroneous");
        return FALSE;
    }
    if(!strcmp(format,"SN12")){
        GST_LOG("Got SN12 as caps, enabling hardware");
    filter->enable_hardware = TRUE;
    format = "SR32";
    }
    gst_videoefx_set_effect(filter);
    if(!filter->enable_hardware){
        GST_DEBUG ("creating image filter");
    result = image_filter_create(&(filter->image_filter_handle),IMAGE_FILTER_CONTEXT_CPU);
    if(result != IMAGE_FILTER_ERROR_NONE || filter->image_filter_handle == NULL){
            GST_ERROR("Could not create image filter, returning failure");
        return FALSE;
    }
        image_filter_set_filter(filter->image_filter_handle, filter->image_filter_effect);
    }else{
        result = image_filter_create(&(filter->image_filter_handle),IMAGE_FILTER_CONTEXT_GPU);
    if(result != IMAGE_FILTER_ERROR_NONE || filter->image_filter_handle == NULL){
            GST_ERROR("Could not create image filter, returning failure");
        return FALSE;
    }
    image_filter_set_filter(filter->image_filter_handle, filter->image_filter_effect);
    if(filter->app_data == NULL){
            filter->app_data = init_app_data();
        filter->app_data->width = filter->width;
        filter->app_data->height = filter->height;
        filter->app_data->ready_for_glm = FALSE;
        filter->app_data->has_created_glm = FALSE;
    }

    filter->app_data->image_filter_handle = filter->image_filter_handle;
    }
    newcaps = gst_caps_new_simple(
            "video/x-raw",
            "format", G_TYPE_STRING, format,
            "width", G_TYPE_INT, filter->width,
            "height", G_TYPE_INT, filter->height,
            "framerate", GST_TYPE_FRACTION, in_fps_num, in_fps_den,
            NULL);
    if(!newcaps)
    {
        GST_ERROR("fail to create new-caps");
        endfunc
        return FALSE;
    }
    filter->frame_rate_numerator = in_fps_num;
    filter->frame_rate_denominator = in_fps_den;

    gst_pad_set_caps(filter->srcpad, newcaps);
    endfunc
    return TRUE;
}
static void
gst_videoefx_finalize(GObject *object){
    GstVideoEfx *filter = GST_VIDEOEFX(object);

    GST_DEBUG("Finalize start");
    //Do any finalizations here
    if(filter->app_data != NULL)
    {
        destroy_app_data(filter->app_data);
    filter->app_data = NULL;
    }

    if(filter->image_filter_handle != NULL)
    {
        image_filter_destroy(filter->image_filter_handle);
    filter->image_filter_handle = NULL;
    }
    filter->passthrough = FALSE;
    G_OBJECT_CLASS(parent_class)->finalize(object);
    GST_DEBUG("Finalize End");
}


static GstStateChangeReturn gst_videoefx_change_state (GstElement * element, GstStateChange transition)
{
    startfunc
    GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
    GstVideoEfx *filter  = GST_VIDEOEFX (element);
    GST_INFO ("Change state  = %d ", transition);
    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:{
            GST_DEBUG ("change state NULL TO READY");
    }
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        GST_DEBUG ("change state READY to PAUSED");
        GST_DEBUG ("Nothing to be done");
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
            GST_DEBUG ("change state PAUSED_TO_PLAYING");
    }
    break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
    goto EXIT;

    switch(transition){
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            GST_DEBUG ("change state READY to PAUSED");
        GST_DEBUG ("Nothing to be done");
        break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        GST_DEBUG ("change state PLAYING to PAUSED");
        GST_DEBUG ("Nothing to be done");
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG ("change state PAUSED to READY");
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG ("change state READY to NULL");
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

EXIT:   endfunc
    return ret;
}


/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_videoefx_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
    startfunc
    gboolean ret;
    GstVideoEfx *filter;

    filter = GST_VIDEOEFX (parent);
    switch (GST_EVENT_TYPE (event)) {
        case GST_EVENT_CAPS:
    {
            GstCaps * caps;
            gst_event_parse_caps (event, &caps);
            /* do something with the caps */
            ret = gst_videoefx_set_caps(pad, caps);
            /* will send our own caps downstream */
            gst_event_unref (event);
            event = NULL;
            break;
        }
        case GST_EVENT_EOS:
        {
            GST_DEBUG("Received an EOS");
            gst_pad_push_event(filter->srcpad,event);
            break;
        }
        case GST_EVENT_FLUSH_START:
        {
            GST_WARNING("Received a flush start event");
            gst_pad_push_event(filter->srcpad,event);
            break;
        }
        case GST_EVENT_FLUSH_STOP:
        {
            GST_WARNING("Received a flush stop event");
            gst_pad_push_event(filter->srcpad,event);
            break;
        }
        default:
        {
            if(event){
                GST_DEBUG("Forwarding events \n");
                ret = gst_pad_event_default (pad, parent, event);
                break;
            }
        }
   }
    endfunc
    return ret;
}
static MMVideoBuffer *make_tbm_buffer(OutputData *output){
    if (!output) {
        GST_LOG("output is null");
    return NULL;
    }
    MMVideoBuffer *psimgb = NULL;
    psimgb = (MMVideoBuffer *)malloc(sizeof(MMVideoBuffer));

    if (!psimgb) {
        GST_LOG("Failed to alloc SCMN_IMGB");
    return NULL;
    }
    memset(psimgb, 0x00, sizeof(MMVideoBuffer));
    tbm_surface_h surface = output->surface;
    if (!surface) {
        GST_LOG("surface or imgb is null");
    free(psimgb);
    psimgb = NULL;
    return NULL;
    }
    tbm_surface_info_s info;
    tbm_surface_get_info(surface,&info);
    int num_bos = tbm_surface_internal_get_num_bos (surface);
    /*output buffer is SR32 format, which has only one plane*/
    psimgb->type = output->buf_share_method;
    if (psimgb->type == MM_VIDEO_BUFFER_TYPE_DMABUF_FD) {
        psimgb->handle.bo[0] = tbm_surface_internal_get_bo (surface, 0);
    psimgb->handle.dmabuf_fd[0] = tbm_bo_export_fd(psimgb->handle.bo[0]);
    } else {
        psimgb->handle.bo[0] = tbm_surface_internal_get_bo (surface, 0);
    }
    /*psimgb->a[0] point to vitual address of output buffer*/
    psimgb->data[0] = info.planes[0].ptr;
    psimgb->width[0] = tbm_surface_get_width(surface);
    psimgb->height[0] = tbm_surface_get_height(surface);
    psimgb->stride_width[0] = psimgb->width[0] * 4;
    psimgb->stride_height[0] = psimgb->height[0];
    psimgb->size[0] = psimgb->width[0] * psimgb->height[0] * 4;

    return psimgb;
}

static GstVideoEfxBuffer* gst_videoefx_buffer_new(GstVideoEfx *filter){
    GstVideoEfxBuffer *ret = NULL;
    ret = (GstVideoEfxBuffer *)malloc(sizeof(GstVideoEfxBuffer));
    ret->buffer = gst_buffer_new();
    ret->filter = gst_object_ref(GST_OBJECT(filter));
    ret->pixmap_id = -1;
    ret->surface = -1;
    return ret;
}

static void gst_internal_buffer_finalize(GstVideoEfxBuffer *buffer){
    GST_INFO("gst_internal_buffer_finalize");
    if(!buffer) {
        GST_ERROR("GstVideoEfxBuffer is null");
    return;
    }

    if (buffer->filter->app_data->has_created_glm) {
        if (buffer->pixmap_id >= 0 && buffer->pixmap_id < SURFACE_COUNT) {
            GST_LOG("pixmap id[%d] is finalized, send signal", buffer->pixmap_id);
        buffer->filter->app_data->surface_using[buffer->pixmap_id] = FALSE;
    } else {
            GST_ERROR("invalid pixmap_id %d", buffer->pixmap_id);
    }
    } else {
        GST_WARNING("GLManager was destroyed");
    }

    gst_object_unref(buffer->filter);
    free(buffer);
    GST_INFO("gst_internal_buffer_finalize end ");
    return;
}

static void gst_mmvidbuf_finalize(MMVideoBuffer* buffer){
    if(buffer){
        GST_WARNING("Freeing MMVideoBuffer ");
    free(buffer);
    }
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_videoefx_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
    startfunc
    GstVideoEfx *filter;

    GstMapInfo info;
    GstBuffer *outbuf;

    unsigned char *outbufdata = NULL;

    filter = GST_VIDEOEFX (parent);

    if(filter->effect_changed == TRUE)
    {
        image_filter_set_filter(filter->image_filter_handle,filter->image_filter_effect);
        filter->effect_changed = FALSE;
    }
    if(filter->enable_hardware)
    {
        if(filter->app_data->buf_share_method == -1){
            GST_DEBUG("Getting buf_share_method only once");
            MMVideoBuffer *imgb = NULL;
            GstMapInfo mapInfo ;
            GstMemory *img_buf_memory = NULL;
        if (buf && gst_buffer_n_memory(buf) > 1){
                GST_WARNING_OBJECT(filter, "Its zero copy");
        img_buf_memory = gst_buffer_peek_memory(buf,1);
        gst_memory_map(img_buf_memory,&mapInfo,GST_MAP_READ);
        imgb = (MMVideoBuffer *)mapInfo.data;
        }
        if(imgb){
                filter->app_data->buf_share_method = imgb->type;
        }
        gst_memory_unmap(img_buf_memory,&mapInfo);
    }
    //Do processing of buffer using hardware mode
    OutputData* curOutput = NULL;
    tbm_surface_h surface = NULL;
    MMVideoBuffer *psimgb = NULL;
    GstVideoEfxBuffer *internalBuf = NULL;
    if(filter->app_data==NULL)
    {
        GST_ERROR("App data is null, plugin improperly initialized");
        return GST_FLOW_ERROR;
    }

    curOutput = process_input_buffer(filter->app_data, buf);
    if(!curOutput){
        GST_ERROR("output buffer is NULL");
        return GST_FLOW_ERROR;
    }

    surface = curOutput->surface;
    if(!surface){
        IF_FREE(curOutput);
        GST_WARNING("Get native buffer failed");
        return GST_FLOW_ERROR;
    }

    psimgb = make_tbm_buffer(curOutput);
    if(!psimgb){
        IF_FREE(curOutput);
        GST_WARNING("gst_make_tbm_buffer failed");
        return GST_FLOW_ERROR;
    }

    internalBuf = gst_videoefx_buffer_new(filter);
    if (!internalBuf) {
        IF_FREE(curOutput);
        free(psimgb);
        GST_WARNING("gst_buffer_new failed");
        return GST_FLOW_ERROR;
    }

    outbuf = (GstBuffer *)internalBuf->buffer;
    internalBuf->pixmap_id = curOutput->pixmap_id;
    internalBuf->surface = surface;
    gst_buffer_append_memory(outbuf,
            gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                psimgb->data[0],
                tbm_surface_internal_get_size (surface),
                0,
                tbm_surface_internal_get_size (surface),
                internalBuf,
                gst_internal_buffer_finalize)
            );
    gst_buffer_append_memory(outbuf,
            gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                psimgb,
                sizeof(*psimgb),
                0,
                sizeof(*psimgb),
                psimgb,
                gst_mmvidbuf_finalize)
            );
    GST_BUFFER_PTS(outbuf) = GST_BUFFER_PTS(buf);
    GST_BUFFER_DTS(outbuf) = GST_BUFFER_DTS(buf);
    GST_BUFFER_DURATION(outbuf) = GST_BUFFER_DURATION(buf);
    GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET (buf);
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_END (buf);
    GST_MINI_OBJECT_FLAGS (outbuf) = GST_MINI_OBJECT_FLAGS (buf);

    gst_buffer_unref(buf);
    IF_FREE(curOutput);
    }
    else{
        gst_buffer_map(buf,&info,GST_MAP_READ);
        image_filter_set_input_by_buffer(filter->image_filter_handle,(unsigned char*)info.data,filter->width, filter->height, IMAGE_FILTER_CORLOR_FORMAT_YUVP);
        outbufdata = (unsigned char*)malloc(sizeof(unsigned char)*info.size);
        image_filter_set_output_by_buffer(filter->image_filter_handle,outbufdata);
        image_filter_apply(filter->image_filter_handle);

        outbuf = gst_buffer_new_wrapped((gpointer)outbufdata,info.size);
        GST_BUFFER_PTS(outbuf) = GST_BUFFER_PTS(buf);
        GST_BUFFER_DTS(outbuf) = GST_BUFFER_DTS(buf);
        GST_BUFFER_DURATION(outbuf) = GST_BUFFER_DURATION(buf);
        //g_print( "Video GST_BUFFER_DTS = %d  --->  "GST_TIME_FORMAT" \n",  GST_TIME_ARGS(GST_BUFFER_DTS(outbuf)));
        gst_buffer_unmap(buf,&info);

        gst_buffer_unref(buf);
    }
    endfunc
    return gst_pad_push (filter->srcpad, outbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
videoefx_init (GstPlugin * videoefx)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template videoefx' with your description
   */
  startfunc
  GST_DEBUG_CATEGORY_INIT (gst_videoefx_debug, "videoefx",
      0, "videoefx");

  endfunc
  return gst_element_register (videoefx, "videoefx", GST_RANK_NONE,
      GST_TYPE_VIDEOEFX);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "videoefx"
#endif

/* gstreamer looks for this structure to register videoefxs
 *
 * exchange the string 'Template videoefx' with your videoefx description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    videoefx,
    "videoefx",
    videoefx_init,
    VERSION,
    "Proprietary",
    "Samsung Electronics Co",
    "http://www.samsung.com"
)
