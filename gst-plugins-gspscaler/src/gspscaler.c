/*
 * GSPScaler
 *
 * Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Abhinay Ganapavarapu <abhinay.g@samsung.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gspscaler.h"
#include "drminterface.h"

/* debug variable definition */
GST_DEBUG_CATEGORY(gspscaler_debug);
#define GST_CAT_DEFAULT gspscaler_debug

enum
{
  PROP_0,
  PROP_DST_WIDTH,
  PROP_DST_HEIGHT,
  PROP_DST_ROTATION,
  PROP_DST_FLIP,
};

//#define OUTPUT_DUMP_ENABLED //enable to dump GSP scaled output
//#define INPUT_DUMP_ENABLED //enable to dump GSP scaler input

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE ("SN12") ";"
                                  GST_VIDEO_CAPS_MAKE ("SR32") ";"
                                  GST_VIDEO_CAPS_MAKE ("NV12") ";"
                                  GST_VIDEO_CAPS_MAKE ("BGRA") ";"
                                  GST_VIDEO_CAPS_MAKE ("ARGB") ";"
                                  GST_VIDEO_CAPS_MAKE ("I420"))
  );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE ("SN12") ";"
                                  GST_VIDEO_CAPS_MAKE ("SR32") ";"
                                  GST_VIDEO_CAPS_MAKE ("NV12") ";"
                                  GST_VIDEO_CAPS_MAKE ("BGRA") ";"
                                  GST_VIDEO_CAPS_MAKE ("ARGB") ";"
                                  GST_VIDEO_CAPS_MAKE ("I420"))
  );

static void gst_gspscaler_base_init(gpointer g_class);
static void gst_gspscaler_class_init(GstGSPScalerClass * klass);
static void gst_gspscaler_init(GstGSPScaler * gspscaler);
static void gst_gspscaler_finalize(GstGSPScaler * gspscaler);
static GstStateChangeReturn gst_gspscaler_change_state (GstElement *element, GstStateChange transition);
static GstCaps * gst_gspscaler_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static GstCaps * gst_gspscaler_fixate_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *othercaps);
static gboolean gst_gspscaler_set_caps(GstBaseTransform *trans, GstCaps *in, GstCaps *out);
static gboolean gst_gspscaler_start(GstBaseTransform *trans);
static gboolean gst_gspscaler_stop(GstBaseTransform *trans);
static gboolean gst_gspscaler_get_unit_size(GstBaseTransform *trans, GstCaps *caps, guint *size);
static GstFlowReturn gst_gspscaler_transform(GstBaseTransform *trans, GstBuffer *in, GstBuffer *out);
static void gst_gspscaler_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gspscaler_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static GstFlowReturn gst_gspscaler_prepare_output_buffer(GstBaseTransform *trans, GstBuffer *input, GstBuffer **buf);
static gboolean gst_gspscaler_priv_init (GstGSPScaler *gspscaler);
static void gst_gspscaler_reset (GstGSPScaler *gspscaler);
static void gst_gspscaler_buffer_finalize(GstGSPScalerBuffer *buffer);


static GstElementClass *parent_class = NULL;

#if defined(OUTPUT_DUMP_ENABLED) ||defined(INPUT_DUMP_ENABLED)
static void
dump_data(const char *file, unsigned char*data, int size)
{
   FILE * fp = fopen (file, "ab+");
    if (fp == NULL)
    {
        return;
    }
    else {
        size =fwrite (data,1, size, fp);
        if (size == 0) {
          GST_ERROR("fwrite failed:");
        }
        GST_INFO ("%d bytes in %s saved", size , file);
        fclose (fp);
    }
    return;
}
#endif

GType
gst_gspscaler_get_type(void)
{
  static GType gspscaler_type = 0;

  if (!gspscaler_type) {
    static const GTypeInfo gspscaler_info = {
      sizeof(GstGSPScalerClass),
      gst_gspscaler_base_init,
      NULL,
      (GClassInitFunc) gst_gspscaler_class_init,
      NULL,
      NULL,
      sizeof(GstGSPScaler),
      0,
      (GInstanceInitFunc) gst_gspscaler_init,
    };

    gspscaler_type = g_type_register_static(GST_TYPE_BASE_TRANSFORM, "GstGSPScaler", &gspscaler_info, 0);
  }
  return gspscaler_type;
}

static void
gst_gspscaler_base_init(gpointer g_class)
{
  return;
}

static void
gst_gspscaler_class_init(GstGSPScalerClass *klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *trans_class;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

  gobject_class =(GObjectClass *) klass;
  trans_class =(GstBaseTransformClass *) klass;

  gobject_class->finalize =(GObjectFinalizeFunc) gst_gspscaler_finalize;
  gobject_class->set_property = gst_gspscaler_set_property;
  gobject_class->get_property = gst_gspscaler_get_property;

  gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_gspscaler_change_state);

  trans_class->transform_caps = GST_DEBUG_FUNCPTR(gst_gspscaler_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR(gst_gspscaler_fixate_caps);
  trans_class->set_caps = GST_DEBUG_FUNCPTR(gst_gspscaler_set_caps);
  trans_class->start = GST_DEBUG_FUNCPTR(gst_gspscaler_start);
  trans_class->stop = GST_DEBUG_FUNCPTR(gst_gspscaler_stop);
  trans_class->get_unit_size = GST_DEBUG_FUNCPTR(gst_gspscaler_get_unit_size);

  trans_class->transform = GST_DEBUG_FUNCPTR(gst_gspscaler_transform);
  trans_class->prepare_output_buffer = GST_DEBUG_FUNCPTR(gst_gspscaler_prepare_output_buffer);

  trans_class->passthrough_on_same_caps = FALSE;

  gst_element_class_set_static_metadata(gstelement_class,
                                        "HW Video Scale Plug-in based on GSP in SPRD chipsets",
                                        "Filter/Effect/Video",
                                        "HW Video scale using GSP",
                                        "Abhinay Ganapavarapu<abhinay.g@samsung.com>");
  gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&src_template));
  gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&sink_template));

  parent_class = g_type_class_peek_parent(klass);

  GST_DEBUG_CATEGORY_INIT(gspscaler_debug, "gspscaler", 0, "gspscaler element");

  g_object_class_install_property (gobject_class, PROP_DST_WIDTH,
    g_param_spec_int ("dst-width", "Output video width", "Output video width. If setting 0, output video width will be media src's width",
    GSPSCALER_DST_WIDTH_MIN, GSPSCALER_DST_WIDTH_MAX, GSPSCALER_DST_WIDTH_DEFAULT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DST_HEIGHT,
    g_param_spec_int ("dst-height", "Output video height", "Output video height. If setting 0, output video height will be media src's height",
    GSPSCALER_DST_HEIGHT_MIN, GSPSCALER_DST_HEIGHT_MAX, GSPSCALER_DST_HEIGHT_DEFAULT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DST_ROTATION,
    g_param_spec_int ("rotate", "Rotate", "Rotation angle 0-0 degree ,1-90 degree ,2-180 degree,3-270 degree",
    GSPSCALER_DST_ROTATION_MIN, GSPSCALER_DST_ROTATION_MAX, GSPSCALER_DST_ROTATION_DEFAULT, G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class, PROP_DST_FLIP,
    g_param_spec_int ("flip", "Flip", "Flip can be 0-none , 1-vertical, 2-horizontal,3-both",
    GSPSCALER_DST_FLIP_MIN, GSPSCALER_DST_FLIP_MAX, GSPSCALER_DST_FLIP_DEFAULT, G_PARAM_READWRITE));
}

static void
gst_gspscaler_init(GstGSPScaler * gspscaler)
{
  GST_INFO_OBJECT(gspscaler, "Enter");
  gst_base_transform_set_qos_enabled(GST_BASE_TRANSFORM(gspscaler), TRUE);

  /* pad setting */
  gspscaler->src_pad = GST_BASE_TRANSFORM_SRC_PAD(gspscaler);

  /* private values init */
  gst_gspscaler_priv_init (gspscaler);

  g_mutex_init(&gspscaler->buf_idx_lock);
  g_cond_init(&gspscaler->buf_idx_cond);

  GST_INFO_OBJECT(gspscaler, "Exit");
}

static gboolean
gst_gspscaler_priv_init (GstGSPScaler *gspscaler)
{
  int i = 0;
  g_return_val_if_fail (GST_IS_GSPSCALER (gspscaler), FALSE);
  GST_INFO_OBJECT(gspscaler, "Enter");

  gspscaler->is_src_format_std = FALSE;
  gspscaler->is_dst_format_std = FALSE;

  gspscaler->is_same_caps = FALSE;
  gspscaler->video_out_buf = NULL;
  gspscaler->dst_buf_idx = 0;
  gspscaler->is_drm_inited = FALSE;
  gspscaler->play_success = TRUE;
  gspscaler->is_playing = FALSE;
  gspscaler->is_property_set= FALSE;
  gspscaler->prop_id = 0;
  gspscaler->op_buf_share_method = MM_VIDEO_BUFFER_TYPE_TBM_BO;
  gspscaler->num_of_src_buffers = MAX_SRC_BUF_NUM;
  gspscaler->num_of_dst_buffers = MAX_DST_BUF_NUM;

  for (i = 0; i < MAX_DST_BUF_NUM; i++)
  {
    gspscaler->dst_buffer_status[i] = GSPSCALER_DST_BUFFER_FREE;
  }

  GST_INFO_OBJECT(gspscaler, "Exit");
  return TRUE;
}

static void
gst_gspscaler_finalize(GstGSPScaler *gspscaler)
{
  g_return_if_fail (GST_IS_GSPSCALER(gspscaler));
  GST_INFO_OBJECT(gspscaler, "Enter");

  if (gspscaler->is_drm_inited) {
    gst_gspscaler_reset(gspscaler);
  }

  /*Wait till all buffers are finalized*/
  if (!gspscaler->is_dst_format_std){
    GST_WARNING_OBJECT(gspscaler, "Check for buffer finalize");
    int buffer_index = 0;
    for(buffer_index = 0;buffer_index<MAX_DST_BUF_NUM;buffer_index++){
      if( GSPSCALER_DST_BUFFER_INUSE==gspscaler->dst_buffer_status[buffer_index]){
        GST_INFO_OBJECT(gspscaler, "Wait for buffer finalize");
        g_mutex_lock (&gspscaler->buf_idx_lock);
        gint64 wait_until = g_get_monotonic_time () + BUFFER_AVAILABLE_MAX_WAIT_TIME;
        if(!g_cond_wait_until(&gspscaler->buf_idx_cond, &gspscaler->buf_idx_lock, wait_until))
          GST_ERROR_OBJECT (gspscaler, "buffer %d state %d is not freed", buffer_index, gspscaler->dst_buffer_status[buffer_index]);
        g_mutex_unlock (&gspscaler->buf_idx_lock);
      }
    }
    GST_WARNING_OBJECT(gspscaler, "Check for buffer finalize done");
  }

  g_mutex_clear(&gspscaler->buf_idx_lock);
  g_cond_clear(&gspscaler->buf_idx_cond);

  GST_INFO_OBJECT(gspscaler, "Exit");

  G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(gspscaler));
}

static GstGSPScalerBuffer *
gst_gspscaler_buffer_new(GstGSPScaler *gspscaler, guint buf_size)
{
  GstGSPScalerBuffer *buffer = NULL;

  buffer = (GstGSPScalerBuffer *)malloc(sizeof(*buffer));

  buffer->actual_size = buf_size;
  buffer->buffer = gst_buffer_new();
  buffer->gspscaler = gst_object_ref(GST_OBJECT(gspscaler));

  return buffer;
}

static void
gst_gspscaler_buffer_finalize(GstGSPScalerBuffer *buffer)
{
  GstGSPScaler *gspscaler = NULL;

  gspscaler = buffer->gspscaler;
  g_mutex_lock (&gspscaler->buf_idx_lock);
  gspscaler->dst_buffer_status[buffer->dst_buffer_index] = GSPSCALER_DST_BUFFER_FREE;
  g_cond_signal(&gspscaler->buf_idx_cond);
  gst_object_unref(gspscaler);
  free(buffer);
  g_mutex_unlock (&gspscaler->buf_idx_lock);
  return;
}

static void
gst_gspscaler_reset (GstGSPScaler*gspscaler)
{
  g_return_if_fail (GST_IS_GSPSCALER (gspscaler));
  GST_INFO_OBJECT(gspscaler, "Enter");

  drm_release (gspscaler);

  GST_INFO_OBJECT(gspscaler, "Exit");
  return;
}

static GstStateChangeReturn
gst_gspscaler_change_state (GstElement *element, GstStateChange transition)
{
  GstGSPScaler *gspscaler = NULL;
  GstStateChangeReturn stateret = GST_STATE_CHANGE_SUCCESS;

  gspscaler = GST_GSPSCALER(element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_INFO_OBJECT( gspscaler, "GST_STATE_CHANGE_NULL_TO_READY");
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_INFO_OBJECT( gspscaler, "GST_STATE_CHANGE_READY_TO_PAUSED");
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_INFO_OBJECT( gspscaler, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
      break;
    default:
      break;
  }
  stateret = parent_class->change_state (element, transition);
  if ( stateret != GST_STATE_CHANGE_SUCCESS ) {
    GST_ERROR_OBJECT( gspscaler, "chane state error in parent class");
    return GST_STATE_CHANGE_FAILURE;
  }

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_INFO_OBJECT( gspscaler, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_INFO_OBJECT( gspscaler, "GST_STATE_CHANGE_PAUSED_TO_READY");
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_INFO_OBJECT( gspscaler, "GST_STATE_CHANGE_READY_TO_NULL");
      gst_gspscaler_reset(gspscaler);
      break;
    default:
      break;
  }

  return stateret;
}

static void
gst_gspscaler_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstGSPScaler *gspscaler = GST_GSPSCALER (object);
  gint width;
  gint height;
  gint rotation;
  gint flip;

  switch (prop_id) {
    case PROP_DST_WIDTH:
      width = g_value_get_int(value);
      if (width != gspscaler->dst_width) {
        if (!width) {
          gspscaler->dst_width = gspscaler->src_width;
        } else {
          gspscaler->dst_width = width;
        }
        GST_INFO_OBJECT (gspscaler, "Going to set output width = %d", gspscaler->dst_width);
        gst_gspscaler_reset(gspscaler);
      } else {
        GST_WARNING_OBJECT (gspscaler, "skip set width.. width(%d),gspscaler->dst_width(%d)",width,gspscaler->dst_width);
      }
      break;
    case PROP_DST_HEIGHT:
      height = g_value_get_int(value);
      if (height != gspscaler->dst_height) {
        if (!height) {
          gspscaler->dst_height = gspscaler->src_height;
        } else {
          gspscaler->dst_height = height;
        }
        GST_INFO_OBJECT (gspscaler, "Going to set output height = %d", gspscaler->dst_height);
        gst_gspscaler_reset(gspscaler);
      } else {
        GST_WARNING_OBJECT (gspscaler, "skip set height.. height(%d),gspscaler->dst_height(%d)",height,gspscaler->dst_height);
      }
      break;
    case PROP_DST_ROTATION:
        rotation = g_value_get_int(value);
        GST_INFO_OBJECT (gspscaler, "setting rotation:%d",rotation);
        if (rotation < GSPSCALER_DST_ROTATION_MIN || rotation > GSPSCALER_DST_ROTATION_MAX) {
          GST_ERROR_OBJECT (gspscaler, "Invalid rotation property = %d", rotation);
        } else {
          gspscaler->rotation = rotation;
          gst_gspscaler_reset(gspscaler);
        }
        break;
    case PROP_DST_FLIP:
        flip = g_value_get_int(value);
        GST_INFO_OBJECT (gspscaler, "setting flip type:%d",flip);
        if (flip < GSPSCALER_DST_FLIP_MIN || flip > GSPSCALER_DST_FLIP_MAX) {
          GST_ERROR_OBJECT (gspscaler, "Invalid flip property = %d", flip);
        } else {
          gspscaler->flip = flip;
         gst_gspscaler_reset(gspscaler);
        }
        break;
  }
}

static void
gst_gspscaler_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstGSPScaler *gspscaler = GST_GSPSCALER (object);

  switch (prop_id) {
  case PROP_DST_WIDTH:
    g_value_set_int(value, gspscaler->dst_width);
    break;
  case PROP_DST_HEIGHT:
    g_value_set_int(value, gspscaler->dst_height);
    break;
  case PROP_DST_ROTATION:
    g_value_set_int(value, gspscaler->rotation);
    break;
  case PROP_DST_FLIP:
    g_value_set_int(value, gspscaler->flip);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static gboolean
gst_gspscaler_start(GstBaseTransform *trans)
{
  GST_DEBUG("gst_gspscaler_start");

  return TRUE;
}

static gboolean
gst_gspscaler_stop(GstBaseTransform *trans)
{
  GST_DEBUG("gst_gspscaler_stop");

  return TRUE;
}

static int get_drm_format(const gchar *format_gst){
  if ((strcmp(format_gst, "SN12") == 0)||(strcmp(format_gst, "NV12") == 0)){
    return DRM_FORMAT_NV12;
  }else if((strcmp(format_gst, "BGRA") == 0)||(strcmp(format_gst, "SR32") == 0)){
    return DRM_FORMAT_XRGB8888;
  }else if((strcmp(format_gst, "RGBA") == 0)){
    return DRM_FORMAT_RGBA8888;
  }else if((strcmp(format_gst, "ARGB") == 0)){
    return DRM_FORMAT_ARGB8888;
  }else if((strcmp(format_gst, "I420") == 0)){
    return DRM_FORMAT_YUV420;
  }else{
    return 0;
  }
}

static int get_frame_size(int format,int width,int height){
  if (format == DRM_FORMAT_NV12){
    return (width*height*3)/2;
  }else if(format == DRM_FORMAT_XRGB8888||format == DRM_FORMAT_RGBA8888||format == DRM_FORMAT_ARGB8888){
    return (width*height*4);
  }else if(format == DRM_FORMAT_YUV420){
    return (width*height*3)/2;
  }else{
    return 0;
  }
}

static void
gst_gspscaler_set_configuration(GstGSPScaler *gspscaler) {
  GST_INFO_OBJECT(gspscaler, "Enter");
  gspscaler->src_config.fmt = gspscaler->src_format_drm;
  gspscaler->src_config.degree = SPRD_DRM_DEGREE_0;
  gspscaler->src_config.sz.hsize = (__u32) ALIGN(gspscaler->src_width,16);
  gspscaler->src_config.sz.vsize = (__u32) ALIGN(gspscaler->src_height,16);
  gspscaler->src_config.pos.x = 0;
  gspscaler->src_config.pos.y = 0;
  gspscaler->src_config.pos.w = (__u32) ALIGN(gspscaler->src_width,16);
  gspscaler->src_config.pos.h = (__u32) ALIGN(gspscaler->src_height,16);
  gspscaler->src_config.flip = SPRD_DRM_FLIP_NONE;
  gspscaler->src_config.ops_id = SPRD_DRM_OPS_SRC;

  gspscaler->dst_config.fmt = gspscaler->dst_format_drm;

  if(gspscaler->rotation == GSPSCALER_DST_ROTATION_90)
    gspscaler->dst_config.degree = SPRD_DRM_DEGREE_90;
  else if(gspscaler->rotation == GSPSCALER_DST_ROTATION_180)
    gspscaler->dst_config.degree = SPRD_DRM_DEGREE_180;
  else if(gspscaler->rotation == GSPSCALER_DST_ROTATION_270)
    gspscaler->dst_config.degree = SPRD_DRM_DEGREE_270;
  else
    gspscaler->dst_config.degree = SPRD_DRM_DEGREE_0;

  gspscaler->dst_config.sz.hsize = (__u32) ALIGN(gspscaler->dst_width,16);
  gspscaler->dst_config.sz.vsize = (__u32) ALIGN(gspscaler->dst_height,16);
  gspscaler->dst_config.pos.x = 0;
  gspscaler->dst_config.pos.y = 0;
  gspscaler->dst_config.pos.w = (__u32)ALIGN(gspscaler->dst_width,16);
  gspscaler->dst_config.pos.h = (__u32)ALIGN(gspscaler->dst_height,16);

  if (gspscaler->flip == GSPSCALER_DST_FLIP_VERT)
    gspscaler->dst_config.flip = SPRD_DRM_FLIP_VERTICAL;
  else if (gspscaler->flip == GSPSCALER_DST_FLIP_HORZ)
    gspscaler->dst_config.flip = SPRD_DRM_FLIP_HORIZONTAL;
  else if (gspscaler->flip == GSPSCALER_DST_FLIP_BOTH)
    gspscaler->dst_config.flip = SPRD_DRM_FLIP_BOTH;
  else
    gspscaler->dst_config.flip = SPRD_DRM_FLIP_NONE;

  gspscaler->dst_config.ops_id = SPRD_DRM_OPS_DST;
  GST_INFO_OBJECT(gspscaler, "Exit");
}

static void
CheckForCompletedCallBack(GstGSPScaler *gspscaler)
{
    char buffer[1024];
    struct drm_event *e;
    int i = 0;
    int ret = 0, len = 0;
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

  GST_INFO_OBJECT(gspscaler, "Enter");

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_CLR(gspscaler->drm_fd, &fds);
    FD_SET(gspscaler->drm_fd, &fds);
    bool found = true;

    while(found)
    {
        ret = select(gspscaler->drm_fd+1, &fds, NULL, NULL, &timeout);
        if (ret <= 0) {
            GST_ERROR_OBJECT(gspscaler, "select timed out or error");
            //continue;
            gspscaler->play_success = FALSE;
            break;
        } else if (FD_ISSET(gspscaler->drm_fd, &fds)) {
            GST_INFO_OBJECT(gspscaler, "FD SET FOUND");

            len = read(gspscaler->drm_fd, buffer, sizeof buffer);
            if (len == 0)
                return ;
            if (len < sizeof *e)
                return ;

            i = 0;
            while (i < len)
            {
                e = (struct drm_event *) &buffer[i];
                if (e->type == DRM_SPRD_IPP_EVENT)
                {
                    GST_INFO_OBJECT(gspscaler, "FD EVENT FOUND");
                    found = false;
                }
                break;
            }
            break;
        }
    }

  GST_INFO_OBJECT(gspscaler, "Exit");
}

static GstCaps *
gst_gspscaler_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
  GstCaps *template;
  GstCaps *ret_caps;
  GstPad *otherpad;

  if (direction == GST_PAD_SRC) {
    otherpad = trans->sinkpad;
  } else if (direction == GST_PAD_SINK) {
    otherpad = trans->srcpad;
  } else {
    return NULL;
  }

  template = gst_caps_copy(gst_pad_get_pad_template_caps(otherpad));
  ret_caps = gst_caps_intersect(caps, template);
  gst_caps_append(ret_caps, template);

  return ret_caps;
}

/*
 * This is an incomplete matrix of in formats and a score for the prefered output
 * format.
 *
 *         out: RGB24   RGB16  ARGB  AYUV  YUV444  YUV422 YUV420 YUV411 YUV410  PAL  GRAY
 *  in
 * RGB24          0      2       1     2     2       3      4      5      6      7    8
 * RGB16          1      0       1     2     2       3      4      5      6      7    8
 * ARGB           2      3       0     1     4       5      6      7      8      9    10
 * AYUV           3      4       1     0     2       5      6      7      8      9    10
 * YUV444         2      4       3     1     0       5      6      7      8      9    10
 * YUV422         3      5       4     2     1       0      6      7      8      9    10
 * YUV420         4      6       5     3     2       1      0      7      8      9    10
 * YUV411         4      6       5     3     2       1      7      0      8      9    10
 * YUV410         6      8       7     5     4       3      2      1      0      9    10
 * PAL            1      3       2     6     4       6      7      8      9      0    10
 * GRAY           1      4       3     2     1       5      6      7      8      9    0
 *
 * PAL or GRAY are never prefered, if we can we would convert to PAL instead
 * of GRAY, though
 * less subsampling is prefered and if any, preferably horizontal
 * We would like to keep the alpha, even if we would need to to colorspace conversion
 * or lose depth.
 */
#define SCORE_PALETTE_LOSS        1
#define SCORE_COLOR_LOSS          2
#define SCORE_ALPHA_LOSS          4
#define SCORE_CHROMA_W_LOSS       8
#define SCORE_CHROMA_H_LOSS      16
#define SCORE_DEPTH_LOSS         32

#define COLOR_MASK   (GST_VIDEO_FORMAT_FLAG_YUV | \
                      GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_GRAY)
#define ALPHA_MASK   (GST_VIDEO_FORMAT_FLAG_ALPHA)
#define PALETTE_MASK (GST_VIDEO_FORMAT_FLAG_PALETTE)

static void
score_value (GstBaseTransform * base, const GstVideoFormatInfo * in_info,
    const GValue * val, gint * min_loss, const GstVideoFormatInfo ** out_info)
{
  const gchar *fname;
  const GstVideoFormatInfo *t_info;
  GstVideoFormatFlags in_flags, t_flags;
  gint loss;

  fname = g_value_get_string (val);
  t_info = gst_video_format_get_info (gst_video_format_from_string (fname));
  if (!t_info)
    return;

  /* accept input format immediately without loss */
  if (in_info == t_info) {
    *min_loss = 0;
    *out_info = t_info;
    return;
  }

  loss = 1;

  in_flags = GST_VIDEO_FORMAT_INFO_FLAGS (in_info);
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  t_flags = GST_VIDEO_FORMAT_INFO_FLAGS (t_info);
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  t_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  if ((t_flags & PALETTE_MASK) != (in_flags & PALETTE_MASK))
    loss += SCORE_PALETTE_LOSS;

  if ((t_flags & COLOR_MASK) != (in_flags & COLOR_MASK))
    loss += SCORE_COLOR_LOSS;

  if ((t_flags & ALPHA_MASK) != (in_flags & ALPHA_MASK))
    loss += SCORE_ALPHA_LOSS;

  if ((in_info->h_sub[1]) < (t_info->h_sub[1]))
    loss += SCORE_CHROMA_H_LOSS;
  if ((in_info->w_sub[1]) < (t_info->w_sub[1]))
    loss += SCORE_CHROMA_W_LOSS;

  if ((in_info->bits) > (t_info->bits))
    loss += SCORE_DEPTH_LOSS;

  GST_DEBUG_OBJECT (base, "score %s -> %s = %d",
      GST_VIDEO_FORMAT_INFO_NAME (in_info),
      GST_VIDEO_FORMAT_INFO_NAME (t_info), loss);

  if (loss < *min_loss) {
    GST_DEBUG_OBJECT (base, "found new best %d", loss);
    *out_info = t_info;
    *min_loss = loss;
  }
}

static void
gst_gspscaler_fixate_format (GstBaseTransform * base, GstCaps * caps,
    GstCaps * result)
{
  GstStructure *ins, *outs;
  const gchar *in_format;
  const GstVideoFormatInfo *in_info, *out_info = NULL;
  gint min_loss = G_MAXINT;
  guint i, capslen;

  ins = gst_caps_get_structure (caps, 0);
  in_format = gst_structure_get_string (ins, "format");
  if (!in_format) {
    GST_WARNING_OBJECT (base, "Failed to get source format");
    return;
  }
  GST_DEBUG_OBJECT (base, "source format %s", in_format);

  in_info =
      gst_video_format_get_info (gst_video_format_from_string (in_format));
  if (!in_info){
    GST_WARNING_OBJECT (base, "Failed to get video format info");
    return;
  }
  outs = gst_caps_get_structure (result, 0);

  capslen = gst_caps_get_size (result);
  GST_DEBUG_OBJECT (base, "iterate %d structures", capslen);
  for (i = 0; i < capslen; i++) {
    GstStructure *tests;
    const GValue *format;

    tests = gst_caps_get_structure (result, i);
    format = gst_structure_get_value (tests, "format");
    /* should not happen */
    if (format == NULL)
      continue;

    if (GST_VALUE_HOLDS_LIST (format)) {
      gint j, len;

      len = gst_value_list_get_size (format);
      GST_DEBUG_OBJECT (base, "have %d formats", len);
      for (j = 0; j < len; j++) {
        const GValue *val;

        val = gst_value_list_get_value (format, j);
        if (G_VALUE_HOLDS_STRING (val)) {
          score_value (base, in_info, val, &min_loss, &out_info);
          if (min_loss == 0)
            break;
        }
      }
    } else if (G_VALUE_HOLDS_STRING (format)) {
      score_value (base, in_info, format, &min_loss, &out_info);
    }
  }
  if (out_info)
    gst_structure_set (outs, "format", G_TYPE_STRING,
        GST_VIDEO_FORMAT_INFO_NAME (out_info), NULL);
}

// fixate_caps function is from videoscale plugin
static GstCaps *
gst_gspscaler_fixate_caps(GstBaseTransform *base, GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
  GstCaps *result;

  GST_DEBUG_OBJECT (base, "trying to fixate othercaps %" GST_PTR_FORMAT
    " based on caps %" GST_PTR_FORMAT, othercaps, caps);

  result = gst_caps_intersect (othercaps, caps);
  if (gst_caps_is_empty (result)) {
    gst_caps_unref (result);
    result = othercaps;
  } else {
    gst_caps_unref (othercaps);
  }

  GST_DEBUG_OBJECT (base, "now fixating %" GST_PTR_FORMAT, result);

  result = gst_caps_make_writable (result);
  gst_gspscaler_fixate_format (base, caps, result);

  /* fixate remaining fields */
  result = gst_caps_fixate (result);

  return result;
}

static gboolean
gst_gspscaler_set_caps(GstBaseTransform *trans, GstCaps *in, GstCaps *out)
{
  GstGSPScaler *gspscaler = NULL;
  GstStructure *structure = NULL;
  gchar *str_in = NULL;
  gchar *str_out = NULL;
  const gchar *src_format_gst = 0;
  const gchar *dst_format_gst = 0;
  gint src_caps_width = 0;
  gint src_caps_height = 0;
  GstVideoInfo src_video_info;
  GstVideoInfo dst_video_info;


  gspscaler = GST_GSPSCALER(trans);
  str_in = gst_caps_to_string(in);
  if(str_in == NULL) {
    GST_ERROR_OBJECT(gspscaler, "gst_caps_to_string() returns NULL...");
    return FALSE;
  }
  GST_WARNING_OBJECT(gspscaler, "[incaps] %s", str_in);

  str_out = gst_caps_to_string(out);
  if(str_out == NULL) {
    GST_ERROR_OBJECT(gspscaler, "gst_caps_to_string() returns NULL...");
    g_free (str_in);
    return FALSE;
  }
  GST_WARNING_OBJECT(gspscaler, "[outcaps] %s", str_out);

  /* get src width/height/format of video frame */
  structure = gst_caps_get_structure(in, 0);
  if (!gst_structure_get_int (structure, "width", &src_caps_width) || !gst_structure_get_int (structure, "height", &src_caps_height)) {
    GST_ERROR_OBJECT (gspscaler, "input frame width or height is not set...");
    return FALSE;
  }

  gst_video_info_from_caps(&src_video_info, in);
  src_format_gst = GST_VIDEO_INFO_NAME(&src_video_info);
  if (!src_format_gst) {
    GST_ERROR ("can not get source format in gst structure");
    return FALSE;
  }
  GST_INFO_OBJECT (gspscaler,"source format - %s", src_format_gst);
  if ((strcmp(src_format_gst, "SN12") != 0)){
    GST_INFO_OBJECT (gspscaler, "received standard source format...");
    gspscaler->is_src_format_std = TRUE;
  }

  /* set src width/height of video frame */
  gspscaler->src_width = src_caps_width;
  gspscaler->src_height = src_caps_height;
  gspscaler->src_format_gst = src_format_gst;
  gspscaler->src_format_drm = get_drm_format(src_format_gst);
  GST_INFO_OBJECT(gspscaler, "[set src_caps] width(%d) height(%d)", gspscaler->src_width, gspscaler->src_height);

  structure = gst_caps_get_structure(out, 0);

  /* set dst width/height of video frame */
  if (!gspscaler->dst_width || !gspscaler->dst_height) {
    if (!gst_structure_get_int (structure, "width", &gspscaler->dst_width) || !gst_structure_get_int (structure, "height", &gspscaler->dst_height) ) {
      GST_ERROR_OBJECT (gspscaler, "output frame width or height is not set...");
      return FALSE;
    }
  }

  gst_video_info_from_caps(&dst_video_info, out);
  dst_format_gst = GST_VIDEO_INFO_NAME(&dst_video_info);
  if (!dst_format_gst) {
    GST_ERROR ("can not get destination format in gst structure");
    return FALSE;
  }
  GST_INFO_OBJECT (gspscaler,"destination format - %s", dst_format_gst);
  if ((strcmp(dst_format_gst, "SN12") != 0) && (strcmp(dst_format_gst, "SR32") != 0)){
    GST_INFO_OBJECT (gspscaler, "received standard destination format...");
    gspscaler->is_dst_format_std = TRUE;
  }

  gspscaler->dst_format_gst = dst_format_gst;
  gspscaler->dst_format_drm = get_drm_format(dst_format_gst);

  GST_INFO_OBJECT(gspscaler, "[set dst_caps] width(%d) height(%d)", gspscaler->dst_width, gspscaler->dst_height);

  gst_gspscaler_reset(gspscaler);

  /* determine whether if incaps is same as outcaps */
  if (gst_caps_is_equal (in, out) &&
      ((!gspscaler->dst_width && !gspscaler->dst_height) ||
          ((gspscaler->dst_width == src_caps_width)
              && (gspscaler->dst_height == src_caps_height)))
      && !gspscaler->flip && !gspscaler->rotation) {
    GST_LOG_OBJECT(gspscaler, "incaps is same as outcaps");
    gspscaler->is_same_caps = TRUE;
    g_free (str_in);
    g_free (str_out);
  } else {
    gspscaler->is_same_caps = FALSE;
    g_free (str_in);
    g_free (str_out);
  }
  return TRUE;
}

static MMVideoBuffer *
gst_gspscaler_get_videobuffer_for_dst(GstGSPScaler *gspscaler)
{
  MMVideoBuffer* mm_video_buf = NULL;

  GST_INFO_OBJECT(gspscaler, "Enter");

  if ((strcmp(gspscaler->dst_format_gst, "SN12") == 0)||(strcmp(gspscaler->dst_format_gst, "SR32") == 0)){
    mm_video_buf = g_malloc0 (sizeof (MMVideoBuffer));
    if (!mm_video_buf) {
      GST_ERROR_OBJECT (gspscaler, "failed to allocate memory...");
      return NULL;
    }

    mm_video_buf->width[0] = gspscaler->dst_width;
    mm_video_buf->height[0] = gspscaler->dst_height;
    mm_video_buf->width[1] = gspscaler->dst_width;
    mm_video_buf->height[1] = gspscaler->dst_height / 2;

    mm_video_buf->stride_width[0] = ALIGN (gspscaler->dst_width, 16);
    mm_video_buf->stride_height[0] = ALIGN (gspscaler->dst_height, 16);
    mm_video_buf->stride_width[1] = ALIGN (gspscaler->dst_width, 16);
    mm_video_buf->stride_height[1] = ALIGN (gspscaler->dst_height / 2, 16);

    mm_video_buf->type = gspscaler->op_buf_share_method;

    switch (mm_video_buf->type) {
      case MM_VIDEO_BUFFER_TYPE_DMABUF_FD:
        mm_video_buf->handle.dmabuf_fd[0] = gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].ion_fd;
        mm_video_buf->data[0] = gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].usr_addr;
        mm_video_buf->data[1] = mm_video_buf->data[0] + mm_video_buf->stride_width[0] * mm_video_buf->stride_height[0];
        GST_DEBUG_OBJECT (gspscaler, "vir_addr : %p and fd = %u", mm_video_buf->data[0], mm_video_buf->handle.dmabuf_fd[0]);
        break;
      case MM_VIDEO_BUFFER_TYPE_TBM_BO:
        mm_video_buf->handle.bo[0] = gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].bo;
        mm_video_buf->data[0] = gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].usr_addr;
        mm_video_buf->data[1] = mm_video_buf->data[0] + mm_video_buf->stride_width[0] * mm_video_buf->stride_height[0];
        mm_video_buf->size[0] = mm_video_buf->stride_width[0]*mm_video_buf->stride_height[0];
        mm_video_buf->size[1] = (mm_video_buf->stride_width[1]*mm_video_buf->stride_height[1]);

        mm_video_buf->handle.paddr[0] = (void*)gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].phy_addr;
        GST_DEBUG_OBJECT (gspscaler, "vir_addr : %p, bo = %p, phy_addr = %p", mm_video_buf->data[0], mm_video_buf->handle.bo[0], mm_video_buf->handle.paddr[0]);
        break;
      case MM_VIDEO_BUFFER_TYPE_PHYSICAL_ADDRESS:
      default:
        GST_ERROR_OBJECT (gspscaler, "support not yet added, buf_share_method = %d", mm_video_buf->type);

        if(mm_video_buf)
          g_free(mm_video_buf);

        return NULL;
    }
  }else{
    GST_ERROR_OBJECT (gspscaler, "unsupported format");
  }

  GST_INFO_OBJECT(gspscaler, "Exit");
  return mm_video_buf;
}

static gboolean
gst_gspscaler_get_unit_size(GstBaseTransform *trans, GstCaps *caps, guint *size)
{
  GstStructure *structure;
  gint width, height;
  GstGSPScaler *gspscaler;
  GstVideoInfo video_info;
  const gchar *format_gst = 0;

  gspscaler = GST_GSPSCALER(trans);

  structure = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (structure, "width", &width) ||
      !gst_structure_get_int (structure, "height", &height)) {
    return FALSE;
  }

  gst_video_info_from_caps(&video_info, caps);
  if (GST_VIDEO_INFO_IS_YUV(&video_info)){
    format_gst = GST_VIDEO_INFO_NAME(&video_info);
    if (!format_gst) {
      GST_ERROR ("can not get format in gst structure");
      return FALSE;
    }
    GST_INFO_OBJECT (gspscaler,"format - %s", format_gst);
    if ((strcmp(format_gst, "SN12") == 0)||(strcmp(format_gst, "NV12") == 0)){
      *size = (width * height * 3) >> 1;
    }else if (strcmp(format_gst, "I420") == 0){
      *size = (width * height * 3) >> 1;
    }else{
        GST_DEBUG_OBJECT (gspscaler, "Not handling selected YUV format...");
        return FALSE;
    }
  }else if(GST_VIDEO_INFO_IS_RGB(&video_info)){
    format_gst = GST_VIDEO_INFO_NAME(&video_info);
    if (!format_gst) {
      GST_ERROR ("can not get format in gst structure");
      return FALSE;
    }
    GST_INFO_OBJECT (gspscaler,"format - %s", format_gst);
    if ((strcmp(format_gst, "RGBA") == 0)||(strcmp(format_gst, "BGRA") == 0)||(strcmp(format_gst, "SR32") == 0)||(strcmp(format_gst, "ARGB") == 0)){
      *size = (width * height * 4);
    }else{
        GST_DEBUG_OBJECT (gspscaler, "Not handling selected RGB format...");
        return FALSE;
    }
  }
  else{
      GST_ERROR_OBJECT(gspscaler, "Not handling formats other than YUV and RGB");
  }

  GST_DEBUG_OBJECT (gspscaler, "Buffer size is %d", *size);
  return TRUE;
}

static GstFlowReturn
gst_gspscaler_prepare_output_buffer(GstBaseTransform *trans, GstBuffer *in_buf, GstBuffer **out_buf)
{
  gboolean ret = TRUE;
  guint out_size = 0;
  guint in_size = 0;
  GstGSPScaler *gspscaler = GST_GSPSCALER(trans);
  GstCaps *out_caps;
  GstCaps *in_caps;
  GstGSPScalerBuffer *gspscaler_buffer = NULL;

  if (gspscaler->is_same_caps && !gspscaler->flip && !gspscaler->rotation) {
    GST_INFO_OBJECT (gspscaler, "in/out caps are same. skipping it");
    *out_buf = in_buf;
    return GST_FLOW_OK;
  }

  in_caps = gst_pad_get_current_caps (trans->sinkpad);
  out_caps = gst_pad_get_current_caps (trans->srcpad);

  ret = gst_gspscaler_get_unit_size(trans, in_caps, &in_size);
  if (ret == FALSE) {
    GST_ERROR_OBJECT (gspscaler, "ERROR in _get_unit_size from in_caps...");
    return GST_FLOW_ERROR;
  }

  ret = gst_gspscaler_get_unit_size(trans, out_caps, &out_size);
  if (ret == FALSE) {
    GST_ERROR_OBJECT (gspscaler, "ERROR in _get_unit_size from out_caps...");
    return GST_FLOW_ERROR;
  }

  //GST_INFO_OBJECT (gspscaler, "input caps :: %s", gst_caps_to_string(in_caps));
  //GST_INFO_OBJECT (gspscaler, "output caps :: %s", gst_caps_to_string(out_caps));
  //GST_DEBUG_OBJECT (gspscaler, "In buffer size = %d", in_size);
  //GST_DEBUG_OBJECT (gspscaler, "Out buffer size = %d", out_size);

  gspscaler->dst_buf_size = out_size;

  if (!gspscaler->is_dst_format_std) {
    //GST_DEBUG_OBJECT (gspscaler, "prepare customized buffer...");
    gspscaler_buffer = gst_gspscaler_buffer_new(gspscaler,out_size);
    *out_buf = (GstBuffer*)gspscaler_buffer->buffer;
    if (*out_buf == NULL) {
      GST_ERROR_OBJECT (gspscaler, "ERROR in creating gspscalerbuf...");
      return GST_FLOW_ERROR;
    }
  } else {
    *out_buf = gst_buffer_new_and_alloc (out_size);
    if (*out_buf == NULL) {
      GST_ERROR_OBJECT (gspscaler, "ERROR in creating outbuf...");
      return GST_FLOW_ERROR;
    }
    //GST_DEBUG_OBJECT (gspscaler, "allocated software buffer = %p...", *out_buf);
  }

  gst_buffer_copy_into(*out_buf, in_buf, GST_BUFFER_COPY_METADATA, 0, -1);

  GST_LOG_OBJECT (gspscaler, "out ts : %"GST_TIME_FORMAT" and out dur : %"GST_TIME_FORMAT,
  GST_TIME_ARGS(GST_BUFFER_PTS(*out_buf)), GST_TIME_ARGS(GST_BUFFER_DURATION(*out_buf)));

  if (!gspscaler->is_dst_format_std)
  {
    g_mutex_lock (&gspscaler->buf_idx_lock);
    if( GSPSCALER_DST_BUFFER_INUSE==gspscaler->dst_buffer_status[gspscaler->dst_buf_idx])
    {
      gint64 wait_until = g_get_monotonic_time () + BUFFER_AVAILABLE_MAX_WAIT_TIME;
      GST_WARNING_OBJECT (gspscaler, "waiting for buffer %d state %d to free", gspscaler->dst_buf_idx, gspscaler->dst_buffer_status[gspscaler->dst_buf_idx]);
      if(g_cond_wait_until(&gspscaler->buf_idx_cond, &gspscaler->buf_idx_lock, wait_until))
        GST_WARNING_OBJECT (gspscaler, "buffer %d state %d is free", gspscaler->dst_buf_idx, gspscaler->dst_buffer_status[gspscaler->dst_buf_idx]);
      else
        GST_ERROR_OBJECT (gspscaler, "buffer %d state %d is not free possible frame corruption", gspscaler->dst_buf_idx, gspscaler->dst_buffer_status[gspscaler->dst_buf_idx]);
    }
    gspscaler_buffer->dst_buffer_index = gspscaler->dst_buf_idx;
    g_mutex_unlock (&gspscaler->buf_idx_lock);
  }

  if(gspscaler_buffer){
    gst_buffer_append_memory(*out_buf, gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                                                  gspscaler_buffer,
                                                  sizeof(*gspscaler_buffer),
                                                  0,
                                                  sizeof(*gspscaler_buffer),
                                                  gspscaler_buffer,
                                                  (GDestroyNotify)gst_gspscaler_buffer_finalize));
  }

  gst_caps_unref(out_caps);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_gspscaler_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
  GstGSPScaler *gspscaler = GST_GSPSCALER(trans);
  GstMemory *mem;
  GstMapInfo mem_info = GST_MAP_INFO_INIT;
  MMVideoBuffer *mm_video_buf = NULL;
  gboolean error = FALSE;

  gspscaler->play_success = TRUE;

#ifdef INPUT_DUMP_ENABLED
  gchar *inputdump_filename= NULL;
  mem = gst_buffer_peek_memory (inbuf, 0);
  gst_memory_map (mem, &mem_info, GST_MAP_READ);
  if(mem_info.data){
    GST_ERROR_OBJECT(gspscaler,"Vaddr %p size %d", mem_info.data, mem_info.size);
    inputdump_filename = g_strdup_printf ("/tmp/%s_input_dump_%dX%d.%s", gst_element_get_name(gspscaler), gspscaler->src_width,gspscaler->src_height,gspscaler->src_format_gst);
    dump_data(inputdump_filename, mem_info.data,mem_info.size);
    g_free(inputdump_filename);
  }
  gst_memory_unmap (mem, &mem_info);
#endif

  if(gspscaler->is_same_caps && !gspscaler->flip && !gspscaler->rotation){
    GST_INFO_OBJECT (gspscaler, "in/out caps are same. skipping it");
    return GST_FLOW_OK;
  }

  /* extract gem name of the source buffer in case of zero copy format*/
  if(!gspscaler->is_src_format_std) {
    guint width = 0, height = 0;

    mem = gst_buffer_peek_memory (inbuf, 1);
    gst_memory_map (mem, &mem_info, GST_MAP_READ);
    mm_video_buf = (MMVideoBuffer *) mem_info.data;
    gst_memory_unmap (mem, &mem_info);
    if (mm_video_buf == NULL) {
      GST_ERROR_OBJECT (gspscaler, "received input mm_video_buf as NULL...");
      error = TRUE;
      goto cleanup;
    }

    width = mm_video_buf->width[0];
    height = mm_video_buf->height[0];

    if ((width != gspscaler->src_width) || (height != gspscaler->src_height)) {
      GST_ERROR_OBJECT (gspscaler, 
      "Something wrong with width & height params. Dimensions in caps do not match video frame dimensions");
      return GST_FLOW_ERROR;
    }
    GST_INFO_OBJECT(gspscaler, "width = %d, height = %d, bo = %p, virtual address = %p, stride = %d, elevation = %d",
      mm_video_buf->width[0], mm_video_buf->height[0], mm_video_buf->handle.bo[0], mm_video_buf->data[0], mm_video_buf->stride_width[0], mm_video_buf->stride_height[0]);
  }


  if(!gspscaler->is_drm_inited) {
    /* Open the DRM device. */
    if (!drm_initialise(gspscaler)) {
      GST_ERROR_OBJECT (gspscaler, "failed to do DRM init");
      error = TRUE;
      goto cleanup;
    }
    gspscaler->is_drm_inited = TRUE;

    /* Allocate the source buffer if required*/
    if(gspscaler->is_src_format_std) {
      if(!drm_allocate_buffer(gspscaler,gspscaler->src_buffer_info,
          get_frame_size(gspscaler->src_format_drm, ALIGN(gspscaler->src_width, 16), ALIGN(gspscaler->src_height, 16)),
          gspscaler->num_of_src_buffers)) {
        GST_ERROR_OBJECT(gspscaler, "unable to allocate input buffer, terminating.");
        error = TRUE;
        goto cleanup;
      }
    }

    /* Allocate the destination buffer */
    if(!drm_allocate_buffer(gspscaler,gspscaler->dst_buffer_info,
          get_frame_size(gspscaler->dst_format_drm, ALIGN(gspscaler->dst_width, 16), ALIGN(gspscaler->dst_height, 16)),
          gspscaler->num_of_dst_buffers)) {
      GST_ERROR_OBJECT(gspscaler, "unable to allocate output buffer, terminating.");
      error = TRUE;
      goto cleanup;
    }
  }

  if(!gspscaler->is_dst_format_std){
    if(mm_video_buf){
      gspscaler->op_buf_share_method = mm_video_buf->type;
    }

    gspscaler->video_out_buf = gst_gspscaler_get_videobuffer_for_dst(gspscaler);
    if(!gspscaler->video_out_buf) {
      GST_ERROR_OBJECT(gspscaler, "unable to get imgb for output buffer, terminating.");
      error = TRUE;
      goto cleanup;
    }
  }

  if(gspscaler->is_src_format_std) {
    /* Copy input buffer to allocated src buffer */
    GstMemory *buf_memory = NULL;
    GstMapInfo buf_info = GST_MAP_INFO_INIT;
    buf_memory = gst_buffer_peek_memory(inbuf, 0);
    gst_memory_map(buf_memory, &buf_info, GST_MAP_READ);
    drm_tbm_buffer_map(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx], TBM_DEVICE_CPU);
    if(buf_info.data){
      memcpy(gspscaler->src_buffer_info[gspscaler->src_buf_idx].usr_addr,
                    buf_info.data,
                    buf_info.size);
      drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
      gst_memory_unmap(buf_memory, &buf_info);
    }else{
      GST_ERROR_OBJECT(gspscaler, "unable to get buffer data, skipping.");
      drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
      gst_memory_unmap(buf_memory, &buf_info);
      error = FALSE;
      goto cleanup;
    }
  }else{
    tbm_bo_handle handle_fd;
    /* Convert tbm fd to gem name */
    handle_fd = tbm_bo_get_handle (mm_video_buf->handle.bo[0], TBM_DEVICE_MM);
    if (!handle_fd.u32) {
      GST_ERROR_OBJECT (gspscaler, "failed to get fd from TBM Object");
      error = TRUE;
      goto cleanup;
    }
    mm_video_buf->handle.dmabuf_fd[0] = handle_fd.u32;

    GST_LOG_OBJECT (gspscaler, "fd = %d from buffer object", mm_video_buf->handle.dmabuf_fd[0]);

    if(drm_convert_dmabuf_gemname(gspscaler, handle_fd.u32, &(gspscaler->src_buffer_info[gspscaler->src_buf_idx].gem_handle[0]))) {
      GST_LOG_OBJECT (gspscaler, "gem name = %d from buffer object", gspscaler->src_buffer_info[gspscaler->src_buf_idx].gem_handle[0]);
    } else  {
      GST_ERROR_OBJECT(gspscaler, "unable to convert FD to gem name, terminating.");
      error = TRUE;
      goto cleanup;
    }
  }

  drm_tbm_buffer_map(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx], TBM_DEVICE_MM);
  drm_tbm_buffer_map(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx], TBM_DEVICE_MM);

  if(!gspscaler->is_property_set) {
    /* Set the source and destination configuration for GSP */
    gst_gspscaler_set_configuration(gspscaler);

    /* Set the property and execute the ioctl */
    gspscaler->prop_id = drm_ipp_set_property(gspscaler, IPP_CMD_M2M, gspscaler->prop_id, 0);
    if(gspscaler->prop_id < 0) {
      GST_ERROR_OBJECT(gspscaler, "setting property failed, terminating.");
      error = FALSE;
      gspscaler->play_success = FALSE;
      drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
      drm_tbm_buffer_unmap(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx]);
      goto cleanup;
    }
    gspscaler->is_property_set = TRUE;
  }

  /* Set the source and destination buffer structures */
  drm_ipp_set_buffers(gspscaler, gspscaler->prop_id);

  gspscaler->ctrl.prop_id = gspscaler->prop_id;
  gspscaler->ctrl.ctrl = IPP_CTRL_PLAY;

  /* Queue src and dest buffers into the GSP */
  if(!drm_ipp_queue_buffers(gspscaler, gspscaler->prop_id)) {
    GST_ERROR_OBJECT(gspscaler, "queuing buffers failed, terminating.");
    error = FALSE;
    gspscaler->play_success = FALSE;
    drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
    drm_tbm_buffer_unmap(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx]);
    goto cleanup;
  }

  if(!gspscaler->is_playing) {
    gspscaler->ctrl.prop_id = gspscaler->prop_id;
    gspscaler->ctrl.ctrl = IPP_CTRL_PLAY;

    if(!drm_ipp_cmd_ctrl_play(gspscaler)) {
      GST_ERROR_OBJECT(gspscaler, "Failed to set PLAY mode, terminating.");
      error = FALSE;
      gspscaler->play_success = FALSE;
      drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
      drm_tbm_buffer_unmap(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx]);
      goto cleanup;
    }
    gspscaler->is_playing = TRUE;
  }

  CheckForCompletedCallBack(gspscaler);
  if(!gspscaler->play_success) {
    GST_ERROR_OBJECT(gspscaler, "Failed to go into PLAY mode, terminating.");
    error = FALSE;
    drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
    drm_tbm_buffer_unmap(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx]);
    goto cleanup;
  }

  drm_tbm_buffer_unmap(gspscaler,&gspscaler->src_buffer_info[gspscaler->src_buf_idx]);
  drm_tbm_buffer_unmap(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx]);

#ifdef OUTPUT_DUMP_ENABLED
  gchar *outputdump_filename= NULL;
  outputdump_filename = g_strdup_printf ("/tmp/%s_output_dump_%dX%d.%s", gst_element_get_name(gspscaler), gspscaler->dst_width,gspscaler->dst_height,gspscaler->dst_format_gst);
  dump_data(outputdump_filename, gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].usr_addr, get_frame_size(gspscaler->dst_format_drm,gspscaler->dst_width,gspscaler->dst_height));
  g_free(outputdump_filename);
#endif

  if(!gspscaler->is_dst_format_std) {
    gst_buffer_append_memory(outbuf, gst_memory_new_wrapped(0,
                                                  gspscaler->video_out_buf,
                                                  sizeof(MMVideoBuffer),
                                                  0,
                                                  sizeof(MMVideoBuffer),
                                                  gspscaler->video_out_buf,
                                                  free));
  }else{
    GstMemory *buf_memory = NULL;
    GstMapInfo buf_info = GST_MAP_INFO_INIT;
    buf_memory = gst_buffer_peek_memory(outbuf, 0);
    gst_memory_map(buf_memory, &buf_info, GST_MAP_WRITE);
    drm_tbm_buffer_map(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx], TBM_DEVICE_CPU);
    memcpy(buf_info.data,
                  gspscaler->dst_buffer_info[gspscaler->dst_buf_idx].usr_addr,
                  get_frame_size(gspscaler->dst_format_drm,gspscaler->dst_width,gspscaler->dst_height));
    drm_tbm_buffer_unmap(gspscaler,&gspscaler->dst_buffer_info[gspscaler->dst_buf_idx]);
    gst_memory_unmap(buf_memory, &buf_info);
  }
  GST_BUFFER_PTS (outbuf) = GST_BUFFER_PTS (inbuf);
  GST_BUFFER_DURATION (outbuf) = GST_BUFFER_DURATION (inbuf);
  GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET (inbuf);
  GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_END (inbuf);
  GST_MINI_OBJECT_FLAGS (outbuf) = GST_MINI_OBJECT_FLAGS (inbuf);

  g_mutex_lock (&gspscaler->buf_idx_lock);
  gspscaler->dst_buffer_status[gspscaler->dst_buf_idx] = GSPSCALER_DST_BUFFER_INUSE;
  g_mutex_unlock (&gspscaler->buf_idx_lock);

cleanup:

  if (!gspscaler->is_src_format_std) {
    drm_close_gem(gspscaler, &(gspscaler->src_buffer_info[gspscaler->src_buf_idx].gem_handle[0]));
  }

  ++gspscaler->src_buf_idx;
  gspscaler->src_buf_idx = gspscaler->src_buf_idx % gspscaler->num_of_src_buffers;

  ++gspscaler->dst_buf_idx;
  gspscaler->dst_buf_idx = gspscaler->dst_buf_idx % gspscaler->num_of_dst_buffers;

  if(!gspscaler->play_success)
    drm_release(gspscaler);

  if(error)
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT(gspscaler_debug, "gspscaler", 0, "GSP Scaler");
  return gst_element_register(plugin, "gspscaler", GST_RANK_NONE, GST_TYPE_GSPSCALER);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gspscaler,
    "GSP Scaler",
    plugin_init, VERSION, "LGPL", "Samsung Electronics Co", "http://www.samsung.com")

