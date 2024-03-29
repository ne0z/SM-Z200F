/*
 * gstwfdtsdescriptor.c -
 * Copyright (C) 2013 Edward Hervey
 *
 * Authors:
 *   Edward Hervey <edward@collabora.com>
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
#include <stdlib.h>
#include <string.h>

#include "gstwfdtsdescriptor.h"
#include "gstwfdtssection.h"
#include "gstwfdts-private.h"

/**
 * SECTION:gstwfdtsdescriptor
 * @title: Base MPEG-TS descriptors
 * @short_description: Descriptors for ITU H.222.0 | ISO/IEC 13818-1
 * @include: gst/mpegts/mpegts.h
 *
 * These are the base descriptor types and methods.
 *
 * For more details, refer to the ITU H.222.0 or ISO/IEC 13818-1 specifications
 * and other specifications mentionned in the documentation.
 */

/* FIXME : Move this to proper file once we have a C file for ATSC/ISDB descriptors */
/**
 * SECTION:gst-atsc-descriptor
 * @title: ATSC variants of MPEG-TS descriptors
 * @short_description: Descriptors for the various ATSC specifications
 * @include: gst/mpegts/mpegts.h
 *
 */

/**
 * SECTION:gst-isdb-descriptor
 * @title: ISDB variants of MPEG-TS descriptors
 * @short_description: Descriptors for the various ISDB specifications
 * @include: gst/mpegts/mpegts.h
 *
 */


/*
 * TODO
 *
 * * Add common validation code for data presence and minimum/maximum expected
 *   size.
 * * Add parsing methods for the following descriptors that were previously 
 *   handled in mpegtsbase:
 *   * GST_MTS_DESC_DVB_DATA_BROADCAST_ID
 *   * GST_MTS_DESC_DVB_CAROUSEL_IDENTIFIER
 *   * GST_MTS_DESC_DVB_FREQUENCY_LIST
 */

#define MAX_KNOWN_ICONV 25

/* First column is the original encoding,
 * second column is the target encoding */

static GIConv __iconvs[MAX_KNOWN_ICONV][MAX_KNOWN_ICONV];

/* All these conversions will be to UTF8 */
typedef enum
{
  _ICONV_UNKNOWN = -1,
  _ICONV_ISO8859_1,
  _ICONV_ISO8859_2,
  _ICONV_ISO8859_3,
  _ICONV_ISO8859_4,
  _ICONV_ISO8859_5,
  _ICONV_ISO8859_6,
  _ICONV_ISO8859_7,
  _ICONV_ISO8859_8,
  _ICONV_ISO8859_9,
  _ICONV_ISO8859_10,
  _ICONV_ISO8859_11,
  _ICONV_ISO8859_12,
  _ICONV_ISO8859_13,
  _ICONV_ISO8859_14,
  _ICONV_ISO8859_15,
  _ICONV_UCS_2BE,
  _ICONV_EUC_KR,
  _ICONV_GB2312,
  _ICONV_UTF_16BE,
  _ICONV_ISO10646_UTF8,
  _ICONV_ISO6937,
  _ICONV_UTF8,
  /* Insert more here if needed */
  _ICONV_MAX
} LocalIconvCode;

static const gchar *iconvtablename[] = {
  "iso-8859-1",
  "iso-8859-2",
  "iso-8859-3",
  "iso-8859-4",
  "iso-8859-5",
  "iso-8859-6",
  "iso-8859-7",
  "iso-8859-8",
  "iso-8859-9",
  "iso-8859-10",
  "iso-8859-11",
  "iso-8859-12",
  "iso-8859-13",
  "iso-8859-14",
  "iso-8859-15",
  "UCS-2BE",
  "EUC-KR",
  "GB2312",
  "UTF-16BE",
  "ISO-10646/UTF8",
  "iso6937",
  "utf-8"
      /* Insert more here if needed */
};

void
__initialize_descriptors (void)
{
  guint i, j;

  /* Initialize converters */
  /* FIXME : How/when should we close them ??? */
  for (i = 0; i < MAX_KNOWN_ICONV; i++) {
    for (j = 0; j < MAX_KNOWN_ICONV; j++)
      __iconvs[i][j] = ((GIConv) - 1);
  }
}

/*
 * @text: The text you want to get the encoding from
 * @start_text: Location where the beginning of the actual text is stored
 * @is_multibyte: Location where information whether it's a multibyte encoding
 * or not is stored
 * @returns: GIconv for conversion or NULL
 */
static LocalIconvCode
get_encoding (const gchar * text, guint * start_text, gboolean * is_multibyte)
{
  LocalIconvCode encoding;
  guint8 firstbyte;

  *is_multibyte = FALSE;
  *start_text = 0;

  firstbyte = (guint8) text[0];

  /* A wrong value */
  g_return_val_if_fail (firstbyte != 0x00, _ICONV_UNKNOWN);

  if (firstbyte <= 0x0B) {
    /* 0x01 => iso 8859-5 */
    encoding = firstbyte + _ICONV_ISO8859_4;
    *start_text = 1;
    goto beach;
  }

  /* ETSI EN 300 468, "Selection of character table" */
  switch (firstbyte) {
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F:
      /* RESERVED */
      encoding = _ICONV_UNKNOWN;
      break;
    case 0x10:
    {
      guint16 table;

      table = GST_READ_UINT16_BE (text + 1);

      if (table < 17)
        encoding = _ICONV_UNKNOWN + table;
      else
        encoding = _ICONV_UNKNOWN;;
      *start_text = 3;
      break;
    }
    case 0x11:
      encoding = _ICONV_UCS_2BE;
      *start_text = 1;
      *is_multibyte = TRUE;
      break;
    case 0x12:
      /*  EUC-KR implements KSX1001 */
      encoding = _ICONV_EUC_KR;
      *start_text = 1;
      *is_multibyte = TRUE;
      break;
    case 0x13:
      encoding = _ICONV_GB2312;
      *start_text = 1;
      break;
    case 0x14:
      encoding = _ICONV_UTF_16BE;
      *start_text = 1;
      *is_multibyte = TRUE;
      break;
    case 0x15:
      /* TODO : Where does this come from ?? */
      encoding = _ICONV_ISO10646_UTF8;
      *start_text = 1;
      break;
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
      /* RESERVED */
      encoding = _ICONV_UNKNOWN;
      break;
    default:
      encoding = _ICONV_ISO6937;
      break;
  }

beach:
  GST_DEBUG
      ("Found encoding %d, first byte is 0x%02x, start_text: %u, is_multibyte: %d",
      encoding, firstbyte, *start_text, *is_multibyte);

  return encoding;
}

static GIConv
_get_iconv (LocalIconvCode from, LocalIconvCode to)
{
  if (__iconvs[from][to] == (GIConv) - 1)
    __iconvs[from][to] = g_iconv_open (iconvtablename[to],
        iconvtablename[from]);
  return __iconvs[from][to];
}

static void
_encode_control_codes (gchar * text, gsize length, gboolean is_multibyte)
{
  gsize pos = 0;

  while (pos < length) {
    if (is_multibyte) {
      guint16 code = GST_READ_UINT16_BE (text + pos);
      if (code == 0x000A) {
        text[pos] = 0xE0;
        text[pos + 1] = 0x8A;
      }
      pos += 2;
    } else {
      guint8 code = text[pos];
      if (code == 0x0A)
        text[pos] = 0x8A;
      pos++;
    }
  }
}

/**
 * dvb_text_from_utf8:
 * @text: The text to convert. This should be in UTF-8 format
 * @out_size: (out): the byte length of the new text
 *
 * Converts UTF-8 strings to text characters compliant with EN 300 468.
 * The converted text can be used directly in DVB #GstWFDTSDescriptor
 *
 * The function will try different character maps until the string is
 * completely converted.
 *
 * The function tries the default ISO 6937 character map first.
 *
 * If no character map that contains all characters could be found, the
 * string is converted to ISO 6937 with unknown characters set to `?`.
 *
 * Returns: (transfer full): byte array of size @out_size
 */
guint8 *
dvb_text_from_utf8 (const gchar * text, gsize * out_size)
{
  GError *error = NULL;
  gchar *out_text;
  guint8 *out_buffer;
  guint encoding;
  GIConv giconv = (GIConv) - 1;

  /* We test character maps one-by-one. Start with the default */
  encoding = _ICONV_ISO6937;
  giconv = _get_iconv (_ICONV_UTF8, encoding);
  out_text = g_convert_with_iconv (text, -1, giconv, NULL, out_size, &error);

  if (out_text) {
    GST_DEBUG ("Using default ISO6937 encoding");
    goto out;
  }

  g_clear_error (&error);

  for (encoding = _ICONV_ISO8859_1; encoding <= _ICONV_ISO10646_UTF8;
      encoding++) {
    giconv = _get_iconv (_ICONV_UTF8, encoding);
    if (giconv == (GIConv) - 1)
      continue;
    out_text = g_convert_with_iconv (text, -1, giconv, NULL, out_size, &error);

    if (out_text) {
      GST_DEBUG ("Found suitable character map - %s", iconvtablename[encoding]);
      goto out;
    }

    g_clear_error (&error);
  }

  out_text = g_convert_with_fallback (text, -1, iconvtablename[_ICONV_ISO6937],
      iconvtablename[_ICONV_UTF8], "?", NULL, out_size, &error);

out:

  if (error) {
    GST_WARNING ("Could not convert from utf-8: %s", error->message);
    g_error_free (error);
    g_free (out_text);
    return NULL;
  }

  switch (encoding) {
    case _ICONV_ISO6937:
      /* Default encoding contains no selection bytes. */
      _encode_control_codes (out_text, *out_size, FALSE);
      return (guint8 *) out_text;
    case _ICONV_ISO8859_1:
    case _ICONV_ISO8859_2:
    case _ICONV_ISO8859_3:
    case _ICONV_ISO8859_4:
      /* These character sets requires 3 selection bytes */
      _encode_control_codes (out_text, *out_size, FALSE);
      out_buffer = g_malloc (*out_size + 3);
      out_buffer[0] = 0x10;
      out_buffer[1] = 0x00;
      out_buffer[2] = encoding - _ICONV_ISO8859_1 + 1;
      memcpy (out_buffer + 3, out_text, *out_size);
      *out_size += 3;
      g_free (out_text);
      return out_buffer;
    case _ICONV_ISO8859_5:
    case _ICONV_ISO8859_6:
    case _ICONV_ISO8859_7:
    case _ICONV_ISO8859_8:
    case _ICONV_ISO8859_9:
    case _ICONV_ISO8859_10:
    case _ICONV_ISO8859_11:
    case _ICONV_ISO8859_12:
    case _ICONV_ISO8859_13:
    case _ICONV_ISO8859_14:
    case _ICONV_ISO8859_15:
      /* These character sets requires 1 selection byte */
      _encode_control_codes (out_text, *out_size, FALSE);
      out_buffer = g_malloc (*out_size + 1);
      out_buffer[0] = encoding - _ICONV_ISO8859_5 + 1;
      memcpy (out_buffer + 1, out_text, *out_size);
      *out_size += 1;
      g_free (out_text);
      return out_buffer;
    case _ICONV_UCS_2BE:
    case _ICONV_EUC_KR:
    case _ICONV_UTF_16BE:
      /* These character sets requires 1 selection byte */
      _encode_control_codes (out_text, *out_size, TRUE);
      out_buffer = g_malloc (*out_size + 1);
      out_buffer[0] = encoding - _ICONV_UCS_2BE + 0x11;
      memcpy (out_buffer + 1, out_text, *out_size);
      *out_size += 1;
      g_free (out_text);
      return out_buffer;
    case _ICONV_GB2312:
    case _ICONV_ISO10646_UTF8:
      /* These character sets requires 1 selection byte */
      _encode_control_codes (out_text, *out_size, FALSE);
      out_buffer = g_malloc (*out_size + 1);
      out_buffer[0] = encoding - _ICONV_UCS_2BE + 0x11;
      memcpy (out_buffer + 1, out_text, *out_size);
      *out_size += 1;
      g_free (out_text);
      return out_buffer;
    default:
      g_free (out_text);
      return NULL;
  }
}

/*
 * @text: The text to convert. It may include pango markup (<b> and </b>)
 * @length: The length of the string -1 if it's nul-terminated
 * @start: Where to start converting in the text
 * @encoding: The encoding of text
 * @is_multibyte: Whether the encoding is a multibyte encoding
 * @error: The location to store the error, or NULL to ignore errors
 * @returns: UTF-8 encoded string
 *
 * Convert text to UTF-8.
 */
static gchar *
convert_to_utf8 (const gchar * text, gint length, guint start,
    GIConv giconv, gboolean is_multibyte, GError ** error)
{
  gchar *new_text;
  gchar *tmp, *pos;
  gint i;

  text += start;

  pos = tmp = g_malloc (length * 2);
  if(!pos)
    return NULL;

  if (is_multibyte) {
    if (length == -1) {
      while (*text != '\0') {
        guint16 code = GST_READ_UINT16_BE (text);

        switch (code) {
          case 0xE086:         /* emphasis on */
          case 0xE087:         /* emphasis off */
            /* skip it */
            break;
          case 0xE08A:{
            pos[0] = 0x00;      /* 0x00 0x0A is a new line */
            pos[1] = 0x0A;
            pos += 2;
            break;
          }
          default:
            pos[0] = text[0];
            pos[1] = text[1];
            pos += 2;
            break;
        }

        text += 2;
      }
    } else {
      for (i = 0; i < length; i += 2) {
        guint16 code = GST_READ_UINT16_BE (text);

        switch (code) {
          case 0xE086:         /* emphasis on */
          case 0xE087:         /* emphasis off */
            /* skip it */
            break;
          case 0xE08A:{
            pos[0] = 0x00;      /* 0x00 0x0A is a new line */
            pos[1] = 0x0A;
            pos += 2;
            break;
          }
          default:
            pos[0] = text[0];
            pos[1] = text[1];
            pos += 2;
            break;
        }

        text += 2;
      }
    }
  } else {
    if (length == -1) {
      while (*text != '\0') {
        guint8 code = (guint8) (*text);

        switch (code) {
          case 0x86:           /* emphasis on */
          case 0x87:           /* emphasis off */
            /* skip it */
            break;
          case 0x8A:
            *pos = '\n';
            pos += 1;
            break;
          default:
            *pos = *text;
            pos += 1;
            break;
        }

        text++;
      }
    } else {
      for (i = 0; i < length; i++) {
        guint8 code = (guint8) (*text);

        switch (code) {
          case 0x86:           /* emphasis on */
          case 0x87:           /* emphasis off */
            /* skip it */
            break;
          case 0x8A:
            *pos = '\n';
            pos += 1;
            break;
          default:
            *pos = *text;
            pos += 1;
            break;
        }

        text++;
      }
    }
  }

  if (pos > tmp) {
    gsize bread = 0;
    new_text =
        g_convert_with_iconv (tmp, pos - tmp, giconv, &bread, NULL, error);
    GST_DEBUG ("Converted to : %s", new_text);
  } else {
    new_text = g_strdup ("");
  }

  g_free (tmp);

  return new_text;
}

gchar *
get_encoding_and_convert (const gchar * text, guint length)
{
  GError *error = NULL;
  gchar *converted_str;
  guint start_text = 0;
  gboolean is_multibyte;
  LocalIconvCode encoding;
  GIConv giconv = (GIConv) - 1;

  g_return_val_if_fail (text != NULL, NULL);

  if (text == NULL || length == 0)
    return g_strdup ("");

  encoding = get_encoding (text, &start_text, &is_multibyte);

  if (encoding > _ICONV_UNKNOWN && encoding < _ICONV_MAX) {
    GST_DEBUG ("Encoding %s", iconvtablename[encoding]);
    giconv = _get_iconv (encoding, _ICONV_UTF8);
  } else {
    GST_FIXME ("Could not detect encoding. Returning NULL string");
    converted_str = NULL;
    goto beach;
  }

  converted_str = convert_to_utf8 (text, length - start_text, start_text,
      giconv, is_multibyte, &error);
  if (error != NULL) {
    GST_WARNING ("Could not convert string: %s", error->message);
    g_free (converted_str);
    g_error_free (error);
    error = NULL;

    if (encoding >= _ICONV_ISO8859_2 && encoding <= _ICONV_ISO8859_15) {
      /* Sometimes using the standard 8859-1 set fixes issues */
      GST_DEBUG ("Encoding %s", iconvtablename[_ICONV_ISO8859_1]);
      giconv = _get_iconv (_ICONV_ISO8859_1, _ICONV_UTF8);

      GST_INFO ("Trying encoding ISO 8859-1");
      converted_str = convert_to_utf8 (text, length, 1, giconv, FALSE, &error);
      if (error != NULL) {
        GST_WARNING
            ("Could not convert string while assuming encoding ISO 8859-1: %s",
            error->message);
        g_error_free (error);
        goto failed;
      }
    } else if (encoding == _ICONV_ISO6937) {

      /* The first part of ISO 6937 is identical to ISO 8859-9, but
       * they differ in the second part. Some channels don't
       * provide the first byte that indicates ISO 8859-9 encoding.
       * If decoding from ISO 6937 failed, we try ISO 8859-9 here.
       */
      giconv = _get_iconv (_ICONV_ISO8859_9, _ICONV_UTF8);

      GST_INFO ("Trying encoding ISO 8859-9");
      converted_str = convert_to_utf8 (text, length, 0, giconv, FALSE, &error);
      if (error != NULL) {
        GST_WARNING
            ("Could not convert string while assuming encoding ISO 8859-9: %s",
            error->message);
        g_error_free (error);
        goto failed;
      }
    } else
      goto failed;
  }

beach:
  return converted_str;

failed:
  {
    text += start_text;
    return g_strndup (text, length - start_text);
  }
}

gchar *
convert_lang_code (guint8 * data)
{
  gchar *code;
  /* the iso language code and country code is always 3 byte long */
  code = g_malloc0 (4);
  memcpy (code, data, 3);

  return code;
}

void
_packetize_descriptor_array (GPtrArray * array, guint8 ** out_data)
{
  guint i;
  guint8 header_size;
  GstWFDTSDescriptor *descriptor;

  g_return_if_fail (out_data != NULL);
  g_return_if_fail (*out_data != NULL);

  if (array == NULL)
    return;

  for (i = 0; i < array->len; i++) {
    descriptor = g_ptr_array_index (array, i);

#ifdef ENABLE_ALL
    if (descriptor->tag == GST_MTS_DESC_DVB_EXTENSION)
      header_size = 3;
    else
      header_size = 2;
#else
    header_size = 2;
#endif

    memcpy (*out_data, descriptor->data, descriptor->length + header_size);
    *out_data += descriptor->length + header_size;
  }
}

GstWFDTSDescriptor *
_new_wfddescriptor (guint8 tag, guint8 length)
{
  GstWFDTSDescriptor *descriptor;
  guint8 *data;

  descriptor = g_slice_new (GstWFDTSDescriptor);

  descriptor->tag = tag;
  descriptor->tag_extension = 0;
  descriptor->length = length;

  descriptor->data = g_malloc (length + 2);

  data = descriptor->data;

  *data++ = descriptor->tag;
  *data = descriptor->length;

  return descriptor;
}

GstWFDTSDescriptor *
_new_wfddescriptor_with_extension (guint8 tag, guint8 tag_extension, guint8 length)
{
  GstWFDTSDescriptor *descriptor;
  guint8 *data;

  descriptor = g_slice_new (GstWFDTSDescriptor);

  descriptor->tag = tag;
  descriptor->tag_extension = tag_extension;
  descriptor->length = length;

  descriptor->data = g_malloc (length + 3);

  data = descriptor->data;

  *data++ = descriptor->tag;
  *data++ = descriptor->tag_extension;
  *data = descriptor->length;

  return descriptor;
}

static GstWFDTSDescriptor *
_copy_descriptor (GstWFDTSDescriptor * desc)
{
  GstWFDTSDescriptor *copy;

  copy = g_slice_dup (GstWFDTSDescriptor, desc);
  copy->data = g_memdup (desc->data, desc->length + 2);

  return copy;
}

/**
 * gst_wfd_ts_descriptor_free:
 * @desc: The descriptor to free
 *
 * Frees @desc
 */
void
gst_wfd_ts_descriptor_free (GstWFDTSDescriptor * desc)
{
  g_free ((gpointer) desc->data);
  g_slice_free (GstWFDTSDescriptor, desc);
}

G_DEFINE_BOXED_TYPE (GstWFDTSDescriptor, gst_wfd_ts_descriptor,
    (GBoxedCopyFunc) _copy_descriptor,
    (GBoxedFreeFunc) gst_wfd_ts_descriptor_free);

/**
 * gst_wfd_ts_parse_descriptors:
 * @buffer: (transfer none): descriptors to parse
 * @buf_len: Size of @buffer
 *
 * Parses the descriptors present in @buffer and returns them as an
 * array.
 *
 * Note: The data provided in @buffer will not be copied.
 *
 * Returns: (transfer full) (element-type GstWFDTSDescriptor): an
 * array of the parsed descriptors or %NULL if there was an error.
 * Release with #g_array_unref when done with it.
 */
GPtrArray *
gst_wfd_ts_parse_descriptors (guint8 * buffer, gsize buf_len)
{
  GPtrArray *res;
  guint8 length;
  guint8 *data;
  guint i, nb_desc = 0;

  /* fast-path */
  if (buf_len == 0)
    return g_ptr_array_new ();

  data = buffer;

  GST_MEMDUMP ("Full descriptor array", buffer, buf_len);

  while (data - buffer < buf_len) {
    data++;                     /* skip tag */
    length = *data++;

    if (data - buffer > buf_len) {
      GST_WARNING ("invalid descriptor length %d now at %d max %"
          G_GSIZE_FORMAT, length, (gint) (data - buffer), buf_len);
      return NULL;
    }

    data += length;
    nb_desc++;
  }

  GST_DEBUG ("Saw %d descriptors, read %" G_GSIZE_FORMAT " bytes",
      nb_desc, data - buffer);

  if (data - buffer != buf_len) {
    GST_WARNING ("descriptors size %d expected %" G_GSIZE_FORMAT,
        (gint) (data - buffer), buf_len);
    return NULL;
  }

  res =
      g_ptr_array_new_full (nb_desc + 1,
      (GDestroyNotify) gst_wfd_ts_descriptor_free);

  data = buffer;

  for (i = 0; i < nb_desc; i++) {
    GstWFDTSDescriptor *desc = g_slice_new0 (GstWFDTSDescriptor);

    desc->data = data;
    desc->tag = *data++;
    desc->length = *data++;
    /* Copy the data now that we known the size */
    desc->data = g_memdup (desc->data, desc->length + 2);
    GST_LOG ("descriptor 0x%02x length:%d", desc->tag, desc->length);
    GST_MEMDUMP ("descriptor", desc->data + 2, desc->length);
    /* extended descriptors */
    if (G_UNLIKELY (desc->tag == 0x7f))
      desc->tag_extension = *data;

    data += desc->length;

    /* Set the descriptor in the array */
    g_ptr_array_index (res, i) = desc;
  }

  res->len = nb_desc;

  return res;
}

/**
 * gst_wfd_ts_find_descriptor:
 * @descriptors: (element-type GstWFDTSDescriptor) (transfer none): an array
 * of #GstWFDTSDescriptor
 * @tag: the tag to look for
 *
 * Finds the first descriptor of type @tag in the array.
 *
 * Note: To look for descriptors that can be present more than once in an
 * array of descriptors, iterate the #GArray manually.
 *
 * Returns: (transfer none): the first descriptor matchin @tag, else %NULL.
 */
const GstWFDTSDescriptor *
gst_wfd_ts_find_descriptor (GPtrArray * descriptors, guint8 tag)
{
  guint i, nb_desc;

  g_return_val_if_fail (descriptors != NULL, NULL);

  nb_desc = descriptors->len;
  for (i = 0; i < nb_desc; i++) {
    GstWFDTSDescriptor *desc = g_ptr_array_index (descriptors, i);
    if (desc->tag == tag)
      return (const GstWFDTSDescriptor *) desc;
  }
  return NULL;
}

/* GST_MTS_DESC_REGISTRATION (0x05) */
/**
 * gst_wfd_ts_descriptor_from_registration:
 * @format_identifier: (transfer none): a 4 character format identifier string
 * @additional_info: (transfer none) (allow-none): pointer to optional additional info
 * @additional_info_length: length of the optional @additional_info
 *
 * Creates a %GST_MTS_DESC_REGISTRATION #GstWFDTSDescriptor
 *
 * Return: #GstWFDTSDescriptor, %NULL on failure
 */
GstWFDTSDescriptor *
gst_wfd_ts_descriptor_from_registration (const gchar * format_identifier,
    guint8 * additional_info, gsize additional_info_length)
{
  GstWFDTSDescriptor *descriptor;

  g_return_val_if_fail (format_identifier != NULL, NULL);

  descriptor = _new_wfddescriptor (GST_MTS_DESC_REGISTRATION,
      4 + additional_info_length);

  memcpy (descriptor->data + 2, format_identifier, 4);
  if (additional_info && (additional_info_length > 0))
    memcpy (descriptor->data + 6, additional_info, additional_info_length);

  return descriptor;
}

/* GST_MTS_DESC_CA (0x09) */

/**
 * gst_wfd_ts_descriptor_parse_ca:
 * @descriptor: a %GST_MTS_DESC_CA #GstWFDTSDescriptor
 * @ca_system_id: (out): the type of CA system used
 * @ca_pid: (out): The PID containing ECM or EMM data
 * @private_data: (out) (allow-none): The private data
 * @private_data_size: (out) (allow-none): The size of @private_data in bytes
 *
 * Extracts the Conditional Access information from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */

gboolean
gst_wfd_ts_descriptor_parse_ca (GstWFDTSDescriptor * descriptor,
    guint16 * ca_system_id, guint16 * ca_pid,
    const guint8 ** private_data, gsize * private_data_size)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && ca_system_id != NULL
      && ca_pid != NULL, FALSE);
  /* The smallest CA is 4 bytes (though not having any private data
   * sounds a bit ... weird) */
  __common_desc_checks (descriptor, GST_MTS_DESC_CA, 4, FALSE);

  data = (guint8 *) descriptor->data + 2;
  *ca_system_id = GST_READ_UINT16_BE (data);
  data += 2;
  *ca_pid = GST_READ_UINT16_BE (data) & 0x1fff;
  data += 2;
  if (private_data && private_data_size) {
    *private_data = data;
    *private_data_size = descriptor->length - 4;
  }

  return TRUE;
}

/* GST_MTS_DESC_ISO_639_LANGUAGE (0x0A) */
static GstWFDTSISO639LanguageDescriptor *
_gst_wfd_ts_iso_639_language_descriptor_copy (GstWFDTSISO639LanguageDescriptor
    * source)
{
  GstWFDTSISO639LanguageDescriptor *copy;
  guint i;

  copy = g_slice_dup (GstWFDTSISO639LanguageDescriptor, source);

  for (i = 0; i < source->nb_language; i++) {
    copy->language[i] = g_strdup (source->language[i]);
  }

  return copy;
}

void
gst_wfd_ts_iso_639_language_descriptor_free (GstWFDTSISO639LanguageDescriptor
    * desc)
{
  guint i;

  for (i = 0; i < desc->nb_language; i++) {
    g_free (desc->language[i]);
  }
  g_slice_free (GstWFDTSISO639LanguageDescriptor, desc);
}

G_DEFINE_BOXED_TYPE (GstWFDTSISO639LanguageDescriptor,
    gst_wfd_ts_iso_639_language,
    (GBoxedCopyFunc) _gst_wfd_ts_iso_639_language_descriptor_copy,
    (GFreeFunc) gst_wfd_ts_iso_639_language_descriptor_free);

/**
 * gst_wfd_ts_descriptor_parse_iso_639_language:
 * @descriptor: a %GST_MTS_DESC_ISO_639_LANGUAGE #GstWFDTSDescriptor
 * @res: (out) (transfer full): the #GstWFDTSISO639LanguageDescriptor to fill
 *
 * Extracts the iso 639-2 language information from @descriptor.
 *
 * Note: Use #gst_tag_get_language_code if you want to get the the
 * ISO 639-1 language code from the returned ISO 639-2 one.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_wfd_ts_descriptor_parse_iso_639_language (const GstWFDTSDescriptor *
    descriptor, GstWFDTSISO639LanguageDescriptor ** desc)
{
  guint i;
  guint8 *data;
  GstWFDTSISO639LanguageDescriptor *res;

  g_return_val_if_fail (descriptor != NULL && desc != NULL, FALSE);
  /* This descriptor can be empty, no size check needed */
  __common_desc_checks (descriptor, GST_MTS_DESC_ISO_639_LANGUAGE, 0, FALSE);

  data = (guint8 *) descriptor->data + 2;

  res = g_slice_new0 (GstWFDTSISO639LanguageDescriptor);

  /* Each language is 3 + 1 bytes */
  res->nb_language = descriptor->length / 4;
  for (i = 0; i < res->nb_language; i++) {
    res->language[i] = convert_lang_code (data);
    res->audio_type[i] = data[3];
    data += 4;
  }

  *desc = res;

  return TRUE;

}

/**
 * gst_wfd_ts_descriptor_parse_iso_639_language_idx:
 * @descriptor: a %GST_MTS_DESC_ISO_639_LANGUAGE #GstWFDTSDescriptor
 * @idx: Table id of the language to parse
 * @lang: (out) (transfer full): 4-byte gchar array to hold the language code
 * @audio_type: (out) (transfer none) (allow-none): the #GstWFDTSIso639AudioType to set
 *
 * Extracts the iso 639-2 language information from specific table id in @descriptor.
 *
 * Note: Use #gst_tag_get_language_code if you want to get the the
 * ISO 639-1 language code from the returned ISO 639-2 one.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_wfd_ts_descriptor_parse_iso_639_language_idx (const GstWFDTSDescriptor *
    descriptor, guint idx, gchar ** lang, GstWFDTSIso639AudioType * audio_type)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && lang != NULL, FALSE);
  /* This descriptor can be empty, no size check needed */
  __common_desc_checks (descriptor, GST_MTS_DESC_ISO_639_LANGUAGE, 0, FALSE);

  if (descriptor->length / 4 <= idx)
    return FALSE;

  data = (guint8 *) descriptor->data + 2 + idx * 4;

  *lang = convert_lang_code (data);

  data += 3;

  if (audio_type)
    *audio_type = *data;

  return TRUE;
}

/**
 * gst_wfd_ts_descriptor_parse_iso_639_language_nb:
 * @descriptor: a %GST_MTS_DESC_ISO_639_LANGUAGE #GstWFDTSDescriptor
 *
 * Returns: The number of languages in @descriptor
 */
guint
gst_wfd_ts_descriptor_parse_iso_639_language_nb (const GstWFDTSDescriptor *
    descriptor)
{
  g_return_val_if_fail (descriptor != NULL, 0);
  /* This descriptor can be empty, no size check needed */
  __common_desc_checks (descriptor, GST_MTS_DESC_ISO_639_LANGUAGE, 0, FALSE);

  return descriptor->length / 4;
}

/**
 * gst_wfd_ts_descriptor_parse_logical_channel:
 * @descriptor: a %GST_MTS_DESC_DTG_LOGICAL_CHANNEL #GstWFDTSDescriptor
 * @res: (out) (transfer none): the #GstWFDTSLogicalChannelDescriptor to fill
 *
 * Extracts the logical channels from @descriptor.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 */
gboolean
gst_wfd_ts_descriptor_parse_logical_channel (const GstWFDTSDescriptor *
    descriptor, GstWFDTSLogicalChannelDescriptor * res)
{
  guint i;
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && res != NULL, FALSE);
  /* This descriptor loop can be empty, no size check required */
  __common_desc_checks (descriptor, GST_MTS_DESC_DTG_LOGICAL_CHANNEL, 0, FALSE);

  data = (guint8 *) descriptor->data + 2;

  res->nb_channels = descriptor->length / 4;

  for (i = 0; i < res->nb_channels; i++) {
    res->channels[i].service_id = GST_READ_UINT16_BE (data);
    data += 2;
    res->channels[i].visible_service = *data >> 7;
    res->channels[i].logical_channel_number =
        GST_READ_UINT16_BE (data) & 0x03ff;
    data += 2;
  }

  return TRUE;
}

/**
 * gst_wfd_ts_descriptor_from_custom:
 * @tag: descriptor tag
 * @data: (transfer none): descriptor data (after tag and length field)
 * @length: length of @data
 *
 * Creates a #GstWFDTSDescriptor with custom @tag and @data
 *
 * Returns: #GstWFDTSDescriptor
 */
GstWFDTSDescriptor *
gst_wfd_ts_descriptor_from_custom (guint8 tag, const guint8 * data,
    gsize length)
{
  GstWFDTSDescriptor *descriptor;

  descriptor = _new_wfddescriptor (tag, length);

  if (data && (length > 0))
    memcpy (descriptor->data + 2, data, length);

  return descriptor;
}
