/* -*- Mode: C; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* GStreamer ID3 tag demuxer
 * Copyright (C) 2005 Jan Schmidt <thaytan@mad.scientist.com>
 * Copyright (C) 2003-2004 Benjamin Otte <otte@gnome.org>
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
 * SECTION:element-id3demux
 *
 * id3demux accepts data streams with either (or both) ID3v2 regions at the
 * start, or ID3v1 at the end. The mime type of the data between the tag blocks
 * is detected using typefind functions, and the appropriate output mime type
 * set on outgoing buffers.
 *
 * The element is only able to read ID3v1 tags from a seekable stream, because
 * they are at the end of the stream. That is, when get_range mode is supported
 * by the upstream elements. If get_range operation is available, id3demux makes
 * it available downstream. This means that elements which require get_range
 * mode, such as wavparse, can operate on files containing ID3 tag information.
 *
 * This id3demux element replaced an older element with the same name which
 * relied on libid3tag from the MAD project.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=file.mp3 ! id3demux ! fakesink -t
 * ]| This pipeline should read any available ID3 tag information and output it.
 * The contents of the file inside the ID3 tag regions should be detected, and
 * the appropriate mime type set on buffers produced from id3demux.
 * </refsect2>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>
#include <gst/gst-i18n-plugin.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/pbutils.h>
#include <string.h>

#include "gstid3demux.h"

#ifdef GST_EXT_ID3DEMUX_MODIFICATION
static const gchar * tag_check_list[] = {
  {GST_TAG_ALBUM},
  {GST_TAG_ARTIST},
  {GST_TAG_COMPOSER},
  {GST_TAG_GENRE},
  {GST_TAG_TITLE}
};
#endif

enum
{
  ARG_0,
  ARG_PREFER_V1
};

#ifndef GST_EXT_ID3DEMUX_MODIFICATION
#define DEFAULT_PREFER_V1  FALSE
#else
#define DEFAULT_PREFER_V1  TRUE
#endif

GST_DEBUG_CATEGORY (id3demux_debug);
#define GST_CAT_DEFAULT (id3demux_debug)

#define ID3V1_TAG_SIZE 128
#define ID3V2_HDR_SIZE GST_TAG_ID3V2_HEADER_SIZE

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-id3")
    );

static gboolean gst_id3demux_identify_tag (GstTagDemux * demux,
    GstBuffer * buffer, gboolean start_tag, guint * tag_size);
static GstTagDemuxResult gst_id3demux_parse_tag (GstTagDemux * demux,
    GstBuffer * buffer, gboolean start_tag, guint * tag_size,
    GstTagList ** tags);
static GstTagList *gst_id3demux_merge_tags (GstTagDemux * tagdemux,
    const GstTagList * start_tags, const GstTagList * end_tags);

static void gst_id3demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_id3demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define gst_id3demux_parent_class parent_class
G_DEFINE_TYPE (GstID3Demux, gst_id3demux, GST_TYPE_TAG_DEMUX);

static void
gst_id3demux_class_init (GstID3DemuxClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstTagDemuxClass *tagdemux_class = (GstTagDemuxClass *) klass;

  gobject_class->set_property = gst_id3demux_set_property;
  gobject_class->get_property = gst_id3demux_get_property;

  g_object_class_install_property (gobject_class, ARG_PREFER_V1,
      g_param_spec_boolean ("prefer-v1", "Prefer version 1 tag",
          "Prefer tags from ID3v1 tag at end of file when both ID3v1 "
          "and ID3v2 tags are present", DEFAULT_PREFER_V1,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_set_static_metadata (gstelement_class, "ID3 tag demuxer",
      "Codec/Demuxer/Metadata",
      "Read and output ID3v1 and ID3v2 tags while demuxing the contents",
      "Jan Schmidt <thaytan@mad.scientist.com>");

  tagdemux_class->identify_tag = GST_DEBUG_FUNCPTR (gst_id3demux_identify_tag);
  tagdemux_class->parse_tag = GST_DEBUG_FUNCPTR (gst_id3demux_parse_tag);
  tagdemux_class->merge_tags = GST_DEBUG_FUNCPTR (gst_id3demux_merge_tags);

  tagdemux_class->min_start_size = ID3V2_HDR_SIZE;
  tagdemux_class->min_end_size = ID3V1_TAG_SIZE;
}

static void
gst_id3demux_init (GstID3Demux * id3demux)
{
  id3demux->prefer_v1 = DEFAULT_PREFER_V1;
}

static gboolean
gst_id3demux_identify_tag (GstTagDemux * demux, GstBuffer * buf,
    gboolean start_tag, guint * tag_size)
{
  guint8 data[3];

  gst_buffer_extract (buf, 0, data, 3);

  if (start_tag) {
    if (data[0] != 'I' || data[1] != 'D' || data[2] != '3')
      goto no_marker;

    *tag_size = gst_tag_get_id3v2_tag_size (buf);
  } else {
    if (data[0] != 'T' || data[1] != 'A' || data[2] != 'G')
      goto no_marker;

    *tag_size = ID3V1_TAG_SIZE;
  }

  GST_INFO_OBJECT (demux, "Found ID3v%u marker, tag_size = %u",
      (start_tag) ? 2 : 1, *tag_size);

  return TRUE;

no_marker:
  {
    GST_DEBUG_OBJECT (demux, "No ID3v%u marker found", (start_tag) ? 2 : 1);
    return FALSE;
  }
}

static void
gst_id3demux_add_container_format (GstTagList * tags)
{
  GstCaps *sink_caps;

  sink_caps = gst_static_pad_template_get_caps (&sink_factory);
  gst_pb_utils_add_codec_description_to_tag_list (tags,
      GST_TAG_CONTAINER_FORMAT, sink_caps);
  gst_caps_unref (sink_caps);
}

static GstTagDemuxResult
gst_id3demux_parse_tag (GstTagDemux * demux, GstBuffer * buffer,
    gboolean start_tag, guint * tag_size, GstTagList ** tags)
{
  if (start_tag) {
    *tag_size = gst_tag_get_id3v2_tag_size (buffer);
    *tags = gst_tag_list_from_id3v2_tag (buffer);

    if (G_LIKELY (*tags != NULL)) {
      gst_id3demux_add_container_format (*tags);
      return GST_TAG_DEMUX_RESULT_OK;
    } else {
      return GST_TAG_DEMUX_RESULT_BROKEN_TAG;
    }
  } else {
    GstMapInfo map;

    gst_buffer_map (buffer, &map, GST_MAP_READ);
    *tags = gst_tag_list_new_from_id3v1 (map.data);
    gst_buffer_unmap (buffer, &map);

    if (G_UNLIKELY (*tags == NULL))
      return GST_TAG_DEMUX_RESULT_BROKEN_TAG;

    gst_id3demux_add_container_format (*tags);
    *tag_size = ID3V1_TAG_SIZE;
    return GST_TAG_DEMUX_RESULT_OK;
  }
}

#ifdef GST_EXT_ID3DEMUX_MODIFICATION
static gboolean
gst_id3demux_tag_is_valid (const GstTagList * list, const gchar * tag)
{
  gchar *string = NULL;
  gint len = 0;
  gint i = 0;

  if (gst_tag_list_get_string(list, tag, &string))
  {
    len = strlen(string);

    /* check whether tag is invalid */
    /* if converting string from any character set to utf-8 is failed on ID3v1 tag extracter, */
    /* the output string is consisted of white space and special characters ('?', etc ...) */
    for (i = 0; i < strlen(string); i++) {
      if ( !((gint)'!' <= ((gint)string[i]) && ((gint)string[i] <= (gint)'/')) &&
            !((gint)':' <= ((gint)string[i]) && ((gint)string[i] <= (gint)'@')) &&
            !((gint)'[' <= ((gint)string[i]) && ((gint)string[i] <= (gint)'.')) &&
            !((gint)'{' <= ((gint)string[i]) && ((gint)string[i] <= (gint)'~')) && (string[i] != ' ')) {
        g_free(string);
        return TRUE;
      }
    }
  }

  g_free(string);

  return FALSE;
}

static void
gst_id3demux_taglist_extension (GstTagDemux * tagdemux, const GstTagList * start_tags,
    const GstTagList * end_tags, GstTagList * merged_tags, gint tag_no)
{
  GstID3Demux *id3demux;
  gchar *string_start = NULL;
  gchar *string_end = NULL;
  gint string_len = 0;

  id3demux = GST_ID3DEMUX (tagdemux);

  if ((gst_tag_list_get_string(start_tags, tag_check_list[tag_no], &string_start)) &&
      (gst_tag_list_get_string(end_tags, tag_check_list[tag_no], &string_end))) {
    string_len = strlen(string_end);
    if (string_len > 0) {
      /* if the tag of ID3v1 and ID3v2 have same text in 30byte, set the merged tag to the same of ID3v2 taglist */
      /* actually, the merged tag is shorter than 30byte when the last character is white space(' ') */
      /* so, compare the string size in the length of string not 30byte */
      if (!strncmp(string_start, string_end, string_len)) {
        gst_tag_list_add(merged_tags, GST_TAG_MERGE_REPLACE, tag_check_list[tag_no], string_start, NULL);
        GST_DEBUG_OBJECT (tagdemux, "modified %s tag from [%s] to [%s]", tag_check_list[tag_no], string_end, string_start);
      }
    }
  }

  g_free (string_start);
  g_free (string_end);
}

static void
gst_id3demux_taglist_is_valid_and_modification (GstTagDemux * tagdemux, const GstTagList * start_tags,
    const GstTagList * end_tags, GstTagList * merged_tags)
{
  GstID3Demux *id3demux;
  gchar *merge_str = NULL, *start_str = NULL;
  gint num = 0;
  gint i = 0;

  g_return_val_if_fail (start_tags == NULL || GST_IS_TAG_LIST (start_tags), NULL);
  g_return_val_if_fail (end_tags == NULL || GST_IS_TAG_LIST (end_tags), NULL);
  g_return_val_if_fail (merged_tags == NULL || GST_IS_TAG_LIST (merged_tags), NULL);

  id3demux = GST_ID3DEMUX (tagdemux);

  /* get the number of tag list which need validation check */
  num = sizeof(tag_check_list) / sizeof(tag_check_list[0]);

  if (merged_tags) {
    for(i = 0; i < num; i++) {
      start_str = NULL;
      merge_str = NULL;

      if (gst_tag_list_get_string(merged_tags, tag_check_list[i], &merge_str)) {
        g_free (merge_str);

        if (!gst_id3demux_tag_is_valid(merged_tags, tag_check_list[i])) {
          /* if merged tag is invalid, set the tag to the same of ID3v2 taglist */
          if (gst_tag_list_get_string(start_tags, tag_check_list[i], &start_str)) {
            gst_tag_list_add(merged_tags, GST_TAG_MERGE_REPLACE, tag_check_list[i], start_str, NULL);
            GST_DEBUG_OBJECT (tagdemux, "modified %s tag: %s", tag_check_list[i], start_str);
            g_free (start_str);
          }
        } else if (!(strcmp(tag_check_list[i], GST_TAG_ALBUM)) || !(strcmp(tag_check_list[i], GST_TAG_ARTIST)) ||
                    !(strcmp(tag_check_list[i], GST_TAG_TITLE))) {
          /* 1) if merged tag is valid and tag frame is album or artist or title, */
          /* 2) the merged tag has 30byte(maximum size of ID3v1) and the tag of ID3v1 and ID3v2 have same text in 30byte */
          /* when 1) and 2) are satisfied, set the tag to the same of ID3v2 taglist */
          gst_id3demux_taglist_extension(tagdemux, start_tags, end_tags, merged_tags, i);
        }
      }
    }
  }

}
#endif

static GstTagList *
gst_id3demux_merge_tags (GstTagDemux * tagdemux, const GstTagList * start_tags,
    const GstTagList * end_tags)
{
  GstID3Demux *id3demux;
  GstTagList *merged;
  gboolean prefer_v1;

  id3demux = GST_ID3DEMUX (tagdemux);

  GST_OBJECT_LOCK (id3demux);
  prefer_v1 = id3demux->prefer_v1;
  GST_OBJECT_UNLOCK (id3demux);

  /* we merge in REPLACE mode, so put the less important tags first */
  if (prefer_v1)
    merged = gst_tag_list_merge (start_tags, end_tags, GST_TAG_MERGE_REPLACE);
  else
    merged = gst_tag_list_merge (end_tags, start_tags, GST_TAG_MERGE_REPLACE);

#ifdef GST_EXT_ID3DEMUX_MODIFICATION
  /* if merged taglist has invalid tags, modify it to the same of ID3v2 taglist */
  gst_id3demux_taglist_is_valid_and_modification (tagdemux, start_tags, end_tags, merged);
#endif

  GST_LOG_OBJECT (id3demux, "start  tags: %" GST_PTR_FORMAT, start_tags);
  GST_LOG_OBJECT (id3demux, "end    tags: %" GST_PTR_FORMAT, end_tags);
  GST_LOG_OBJECT (id3demux, "merged tags: %" GST_PTR_FORMAT, merged);

  return merged;
}

static void
gst_id3demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstID3Demux *id3demux;

  id3demux = GST_ID3DEMUX (object);

  switch (prop_id) {
    case ARG_PREFER_V1:{
      GST_OBJECT_LOCK (id3demux);
      id3demux->prefer_v1 = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (id3demux);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_id3demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstID3Demux *id3demux;

  id3demux = GST_ID3DEMUX (object);

  switch (prop_id) {
    case ARG_PREFER_V1:
      GST_OBJECT_LOCK (id3demux);
      g_value_set_boolean (value, id3demux->prefer_v1);
      GST_OBJECT_UNLOCK (id3demux);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (id3demux_debug, "id3demux", 0,
      "GStreamer ID3 tag demuxer");

  gst_tag_register_musicbrainz_tags ();

  return gst_element_register (plugin, "id3demux",
      GST_RANK_PRIMARY, GST_TYPE_ID3DEMUX);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    id3demux,
    "Demux ID3v1 and ID3v2 tags from a file",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
