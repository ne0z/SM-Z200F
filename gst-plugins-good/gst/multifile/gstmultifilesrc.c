/* GStreamer
 * Copyright (C) 2006 David A. Schleef <ds@schleef.org>
 *
 * gstmultifilesrc.c:
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:element-multifilesrc
 * @see_also: #GstFileSrc
 *
 * Reads buffers from sequentially named files. If used together with an image
 * decoder, one needs to use the #GstMultiFileSrc:caps property or a capsfilter
 * to force to caps containing a framerate. Otherwise image decoders send EOS
 * after the first picture. We also need a videorate element to set timestamps
 * on all buffers after the first one in accordance with the framerate.
 *
 * File names are created by replacing "\%d" with the index using printf().
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 multifilesrc location="img.%04d.png" index=0 caps="image/png,framerate=\(fraction\)12/1" ! \
 *     pngdec ! videoconvert ! videorate ! theoraenc ! oggmux ! \
 *     filesink location="images.ogg"
 * ]| This pipeline creates a video file "images.ogg" by joining multiple PNG
 * files named img.0000.png, img.0001.png, etc.
 * </refsect2>
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstmultifilesrc.h"


static GstFlowReturn gst_multi_file_src_create (GstPushSrc * src,
    GstBuffer ** buffer);

static GstFlowReturn gst_multi_file_src_fill (GstPushSrc * src,
    GstBuffer * buffer);

static void gst_multi_file_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);

static void gst_multi_file_src_dispose (GObject * object);

static void gst_multi_file_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_multi_file_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_multi_file_src_getcaps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_multi_file_src_query (GstBaseSrc * src, GstQuery * query);


static GstStaticPadTemplate gst_multi_file_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_multi_file_src_debug);
#define GST_CAT_DEFAULT gst_multi_file_src_debug

enum
{
  ARG_0,
  ARG_LOCATION,
  ARG_INDEX,
  ARG_START_INDEX,
  ARG_STOP_INDEX,
  ARG_CAPS,
  ARG_LOOP,
  ARG_IS_LIVE
};

#define DEFAULT_LOCATION "%05d"
#define DEFAULT_INDEX 0

#define gst_multi_file_src_parent_class parent_class
G_DEFINE_TYPE (GstMultiFileSrc, gst_multi_file_src, GST_TYPE_PUSH_SRC);


static gboolean
is_seekable (GstBaseSrc * src)
{
  GstMultiFileSrc *mfs = GST_MULTI_FILE_SRC (src);

  if (mfs->fps_n != -1)
    return TRUE;

  return FALSE;
}

static gboolean
do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  gboolean reverse;
  GstClockTime position;
  GstMultiFileSrc *src;

  src = GST_MULTI_FILE_SRC (bsrc);

  segment->time = segment->start;
  position = segment->position;
  reverse = segment->rate < 0;

  if (reverse) {
    GST_FIXME_OBJECT (src, "Handle reverse playback");

    return FALSE;
  }

  /* now move to the position indicated */
  if (src->fps_n) {
    src->index = gst_util_uint64_scale (position,
        src->fps_n, src->fps_d * GST_SECOND);
  } else {
    src->index = 0;
    GST_WARNING_OBJECT (src, "No FPS set, can not seek");

    return FALSE;
  }

  return TRUE;
}

static void
gst_multi_file_src_class_init (GstMultiFileSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_multi_file_src_set_property;
  gobject_class->get_property = gst_multi_file_src_get_property;

  g_object_class_install_property (gobject_class, ARG_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Pattern to create file names of input files.  File names are "
          "created by calling sprintf() with the pattern and the current "
          "index.", DEFAULT_LOCATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_INDEX,
      g_param_spec_int ("index", "File Index",
          "Index to use with location property to create file names.  The "
          "index is incremented by one for each buffer read.",
          0, INT_MAX, DEFAULT_INDEX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_START_INDEX,
      g_param_spec_int ("start-index", "Start Index",
          "Start value of index.  The initial value of index can be set "
          "either by setting index or start-index.  When the end of the loop "
          "is reached, the index will be set to the value start-index.",
          0, INT_MAX, DEFAULT_INDEX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_STOP_INDEX,
      g_param_spec_int ("stop-index", "Stop Index",
          "Stop value of index.  The special value -1 means no stop.",
          -1, INT_MAX, DEFAULT_INDEX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "Caps describing the format of the data.",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_LOOP,
      g_param_spec_boolean ("loop", "Loop",
          "Whether to repeat from the beginning when all files have been read.",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_IS_LIVE,
      g_param_spec_boolean ("is-live", "is-live",
          "Whether to act as a live source",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->dispose = gst_multi_file_src_dispose;

  gstbasesrc_class->get_caps = gst_multi_file_src_getcaps;
  gstbasesrc_class->query = gst_multi_file_src_query;
  gstbasesrc_class->is_seekable = is_seekable;
  gstbasesrc_class->do_seek = do_seek;
  gstbasesrc_class->get_times = gst_multi_file_src_get_times;

  gstpushsrc_class->create = gst_multi_file_src_create;

  GST_DEBUG_CATEGORY_INIT (gst_multi_file_src_debug, "multifilesrc", 0,
      "multifilesrc element");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_multi_file_src_pad_template));
  gst_element_class_set_static_metadata (gstelement_class, "Multi-File Source",
      "Source/File",
      "Read a sequentially named set of files into buffers",
      "David Schleef <ds@schleef.org>");
}

static void
gst_multi_file_src_init (GstMultiFileSrc * multifilesrc)
{
  multifilesrc->start_index = DEFAULT_INDEX;
  multifilesrc->index = DEFAULT_INDEX;
  multifilesrc->stop_index = -1;
  multifilesrc->filename = g_strdup (DEFAULT_LOCATION);
  multifilesrc->successful_read = FALSE;
  multifilesrc->fps_n = multifilesrc->fps_d = -1;
  multifilesrc->timestamp_offset = 0;
  multifilesrc->running_time = 0;
  multifilesrc->n_frames = 0;
}

static void
gst_multi_file_src_dispose (GObject * object)
{
  GstMultiFileSrc *src = GST_MULTI_FILE_SRC (object);

  g_free (src->filename);
  src->filename = NULL;
  if (src->caps)
    gst_caps_unref (src->caps);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GstCaps *
gst_multi_file_src_getcaps (GstBaseSrc * src, GstCaps * filter)
{
  GstMultiFileSrc *multi_file_src = GST_MULTI_FILE_SRC (src);

  GST_DEBUG_OBJECT (src, "returning %" GST_PTR_FORMAT, multi_file_src->caps);

  if (multi_file_src->caps) {
    if (filter)
      return gst_caps_intersect_full (filter, multi_file_src->caps,
          GST_CAPS_INTERSECT_FIRST);
    else
      return gst_caps_ref (multi_file_src->caps);
  } else {
    if (filter)
      return gst_caps_ref (filter);
    else
      return gst_caps_new_any ();
  }
}

static gboolean
gst_multi_file_src_query (GstBaseSrc * src, GstQuery * query)
{
  gboolean res;
  GstMultiFileSrc *mfsrc;

  mfsrc = GST_MULTI_FILE_SRC (src);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);
      switch (format) {
        case GST_FORMAT_BUFFERS:
        case GST_FORMAT_DEFAULT:
          gst_query_set_position (query, GST_FORMAT_BUFFERS, mfsrc->index);
          res = TRUE;
          break;
        default:
          res = GST_BASE_SRC_CLASS (parent_class)->query (src, query);
          break;
      }
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (src, query);
      break;
  }
  return res;
}

static gboolean
gst_multi_file_src_set_location (GstMultiFileSrc * src, const gchar * location)
{
  g_free (src->filename);
  if (location != NULL) {
    src->filename = g_strdup (location);
  } else {
    src->filename = NULL;
  }

  return TRUE;
}

static void
gst_multi_file_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMultiFileSrc *src = GST_MULTI_FILE_SRC (object);

  switch (prop_id) {
    case ARG_LOCATION:
      gst_multi_file_src_set_location (src, g_value_get_string (value));
      break;
    case ARG_INDEX:
      src->index = g_value_get_int (value);
      break;
    case ARG_START_INDEX:
      src->start_index = g_value_get_int (value);
      break;
    case ARG_STOP_INDEX:
      src->stop_index = g_value_get_int (value);
      break;
    case ARG_CAPS:
    {
      GstStructure *st = NULL;
      const GstCaps *caps = gst_value_get_caps (value);
      GstCaps *new_caps;

      if (caps == NULL) {
        new_caps = gst_caps_new_any ();
      } else {
        new_caps = gst_caps_copy (caps);
      }
      gst_caps_replace (&src->caps, new_caps);
      gst_pad_set_caps (GST_BASE_SRC_PAD (src), new_caps);

      if (new_caps && gst_caps_get_size (new_caps) == 1 &&
          (st = gst_caps_get_structure (new_caps, 0))
          && gst_structure_get_fraction (st, "framerate", &src->fps_n,
              &src->fps_d)) {
        GST_INFO_OBJECT (src, "Seting framerate to %d/%d", src->fps_n,
            src->fps_d);
      } else {
        src->fps_n = -1;
        src->fps_d = -1;
      }
    }
      break;
    case ARG_LOOP:
      src->loop = g_value_get_boolean (value);
      break;
    case ARG_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_multi_file_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMultiFileSrc *src = GST_MULTI_FILE_SRC (object);

  switch (prop_id) {
    case ARG_LOCATION:
      g_value_set_string (value, src->filename);
      break;
    case ARG_INDEX:
      g_value_set_int (value, src->index);
      break;
    case ARG_START_INDEX:
      g_value_set_int (value, src->start_index);
      break;
    case ARG_STOP_INDEX:
      g_value_set_int (value, src->stop_index);
      break;
    case ARG_CAPS:
      gst_value_set_caps (value, src->caps);
      break;
    case ARG_LOOP:
      g_value_set_boolean (value, src->loop);
      break;
    case ARG_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_multi_file_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_DTS (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gchar *
gst_multi_file_src_get_filename (GstMultiFileSrc * multifilesrc)
{
  gchar *filename;

  GST_DEBUG ("%d", multifilesrc->index);
  filename = g_strdup_printf (multifilesrc->filename, multifilesrc->index);

  return filename;
}

static GstFlowReturn
gst_multi_file_src_create (GstPushSrc * src, GstBuffer ** buffer)
{
  GstMultiFileSrc *multifilesrc;
  GstClockTime next_time;
  gsize size;
  gchar *data;
  gchar *filename;
  GstBuffer *buf;
  gboolean ret;
  GError *error = NULL;

  multifilesrc = GST_MULTI_FILE_SRC (src);
  GST_LOG_OBJECT (multifilesrc,
      "creating buffer from pool for frame %d", (gint) multifilesrc->n_frames);

  if (multifilesrc->index < multifilesrc->start_index) {
    multifilesrc->index = multifilesrc->start_index;
  }

  if (multifilesrc->stop_index != -1 &&
      multifilesrc->index > multifilesrc->stop_index) {
    if (multifilesrc->loop)
      multifilesrc->index = multifilesrc->start_index;
    else
      return GST_FLOW_EOS;
  }

  filename = gst_multi_file_src_get_filename (multifilesrc);

  GST_DEBUG_OBJECT (multifilesrc, "reading from file \"%s\".", filename);

  ret = g_file_get_contents (filename, &data, &size, &error);
  if (!ret) {
    if (multifilesrc->successful_read) {
      /* If we've read at least one buffer successfully, not finding the
       * next file is EOS. */
      g_free (filename);
      if (error != NULL)
        g_error_free (error);

      if (multifilesrc->loop) {
        error = NULL;
        multifilesrc->index = multifilesrc->start_index;

        filename = gst_multi_file_src_get_filename (multifilesrc);
        ret = g_file_get_contents (filename, &data, &size, &error);
        if (!ret) {
          g_free (filename);
          if (error != NULL)
            g_error_free (error);

          return GST_FLOW_EOS;
        }
      } else {
        return GST_FLOW_EOS;
      }
    } else {
      goto handle_error;
    }
  }

  multifilesrc->successful_read = TRUE;
  multifilesrc->index++;

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (0, data, size, 0, size, data, g_free));
  GST_BUFFER_OFFSET (buf) = multifilesrc->offset;
  GST_BUFFER_OFFSET_END (buf) = multifilesrc->offset + size;
  multifilesrc->offset += size;
  GST_BUFFER_DTS (buf) =
      multifilesrc->timestamp_offset + multifilesrc->running_time;
  GST_BUFFER_PTS (buf) = GST_BUFFER_DTS (buf);

  gst_object_sync_values (GST_OBJECT (src), GST_BUFFER_DTS (buf));

  GST_DEBUG_OBJECT (multifilesrc, "Timestamp: %"
      GST_TIME_FORMAT " + offset: %"
      GST_TIME_FORMAT " + running time: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
      GST_TIME_ARGS (multifilesrc->timestamp_offset), GST_TIME_ARGS (multifilesrc->running_time));

  GST_BUFFER_OFFSET (buf) = multifilesrc->n_frames;
  multifilesrc->n_frames++;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET (buf) + 1;
  if (multifilesrc->fps_n) {
    next_time = gst_util_uint64_scale_int (multifilesrc->n_frames * GST_SECOND,
        multifilesrc->fps_d, multifilesrc->fps_n);
    GST_BUFFER_DURATION (buf) = next_time - multifilesrc->running_time;
  } else {
    next_time = multifilesrc->timestamp_offset;
    /* NONE means forever */
    GST_BUFFER_DURATION (buf) = GST_CLOCK_TIME_NONE;
  }

  multifilesrc->running_time = next_time;
  GST_DEBUG_OBJECT (multifilesrc, "read file \"%s\".", filename);

  g_free (filename);
  *buffer = buf;
  return GST_FLOW_OK;

handle_error:
  {
    if (error != NULL) {
      GST_ELEMENT_ERROR (multifilesrc, RESOURCE, READ,
          ("Error while reading from file \"%s\".", filename),
          ("%s", error->message));
      g_error_free (error);
    } else {
      GST_ELEMENT_ERROR (multifilesrc, RESOURCE, READ,
          ("Error while reading from file \"%s\".", filename),
          ("%s", g_strerror (errno)));
    }
    g_free (filename);
    return GST_FLOW_ERROR;
  }
}
