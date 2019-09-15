/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *               2000,2005 Wim Taymans <wim@fluendo.com>
 *
 * gstfilesrc.c:
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
 * SECTION:element-filesrc
 * @see_also: #GstFileSrc
 *
 * Read data from a file in the local file system.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=song.ogg ! decodebin ! autoaudiosink
 * ]| Play a song.ogg from local dir.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include "gstfilesrc.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef G_OS_WIN32
#include <io.h>                 /* lseek, open, close, read */
/* On win32, stat* default to 32 bit; we need the 64-bit
 * variants, so explicitly define it that way. */
#undef stat
#define stat __stat64
#undef fstat
#define fstat _fstat64
#undef lseek
#define lseek _lseeki64
#undef off_t
#define off_t guint64
/* Prevent stat.h from defining the stat* functions as
 * _stat*, since we're explicitly overriding that */
#undef _INC_STAT_INL
#endif
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <errno.h>
#include <string.h>

#include "../../gst/gst-i18n-lib.h"

#ifdef FILESRC_DECRYPTION
#define ENCRYPTED_DATA_READ_SIZE 25*4096
#define DECRYPTED_AES_CACHE_MAX (25*1024*1024)

static unsigned char* decipher_key(gchar *data){
  unsigned char* converted = 0;
  uint8_t* ch = (uint8_t*)data;
  int conv_idx = 0;
  converted = (unsigned char*)g_malloc0(sizeof(unsigned char)*strlen(data));

  while (*ch) {
    char byte = 0;
    if ('0' <= *ch && *ch <= '9') {
      byte = *ch - '0';
    } else if ('a' <= *ch && *ch <= 'f') {
      byte = *ch - 'a' + 10;
    }
    ch++;

    byte = byte << 4;
    if (*ch) {
      if ('0' <= *ch && *ch <= '9') {
        byte += *ch - '0';
      } else if ('a' <= *ch && *ch <= 'f') {
        byte += *ch - 'a' + 10;
      }
      ch++;
    }
    converted[conv_idx++] = byte;
  }
  return converted;
}
#endif

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* FIXME we should be using glib for this */
#ifndef S_ISREG
#define S_ISREG(mode) ((mode)&_S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(x) (0)
#endif
#ifndef O_BINARY
#define O_BINARY (0)
#endif

/* Copy of glib's g_open due to win32 libc/cross-DLL brokenness: we can't
 * use the 'file descriptor' opened in glib (and returned from this function)
 * in this library, as they may have unrelated C runtimes. */
static int
gst_open (const gchar * filename, int flags, int mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL) {
    errno = EINVAL;
    return -1;
  }

  retval = _wopen (wfilename, flags, mode);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#else
  return open (filename, flags, mode);
#endif
}

GST_DEBUG_CATEGORY_STATIC (gst_file_src_debug);
#define GST_CAT_DEFAULT gst_file_src_debug

/* FileSrc signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_BLOCKSIZE       4*1024

enum
{
  PROP_0,
  PROP_LOCATION
#ifdef FILESRC_DECRYPTION
  ,PROP_ENCRYPTION
  ,PROP_ENCRYPTION_KEY
  ,PROP_ENCRYPTION_IV
#endif
};

static void gst_file_src_finalize (GObject * object);

static void gst_file_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_file_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_file_src_start (GstBaseSrc * basesrc);
static gboolean gst_file_src_stop (GstBaseSrc * basesrc);

static gboolean gst_file_src_is_seekable (GstBaseSrc * src);
static gboolean gst_file_src_get_size (GstBaseSrc * src, guint64 * size);
static GstFlowReturn gst_file_src_fill (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer * buf);
#ifdef FILESRC_MODIFICATION
static gboolean gst_file_src_query (GstBaseSrc * src, GstQuery * query);
#endif
#ifdef FILESRC_DECRYPTION
static GstFlowReturn gst_file_src_fill_ex (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer * buf);
static void gst_file_src_decryptor_init (GstBaseSrc * basesrc);
static void gst_file_src_decryptor_deinit (GObject * object);
static GstFlowReturn gst_aes_file_src_fill (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer * buf);
#endif
static void gst_file_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define _do_init \
  G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_file_src_uri_handler_init); \
  GST_DEBUG_CATEGORY_INIT (gst_file_src_debug, "filesrc", 0, "filesrc element");
#define gst_file_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstFileSrc, gst_file_src, GST_TYPE_BASE_SRC, _do_init);

static void
gst_file_src_class_init (GstFileSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_file_src_set_property;
  gobject_class->get_property = gst_file_src_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Location of the file to read", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
#ifdef FILESRC_DECRYPTION
  g_object_class_install_property (gobject_class, PROP_ENCRYPTION,
      g_param_spec_int ("content_encryption", "Content encryption type",
          "0-None 1-AES", CONTENT_ENCRYPTION_NONE, CONTENT_ENCRYPTION_AES, CONTENT_ENCRYPTION_NONE,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_ENCRYPTION_KEY,
      g_param_spec_string ("content_key", "Content encryption key",
          "Content encryption key", NULL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_ENCRYPTION_IV,
      g_param_spec_string ("content_iv", "Content encryption IV",
          "Content encryption initialization vector", NULL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));
#endif
  gobject_class->finalize = gst_file_src_finalize;

  gst_element_class_set_static_metadata (gstelement_class,
      "File Source",
      "Source/File",
      "Read from arbitrary point in a file",
      "Erik Walthinsen <omega@cse.ogi.edu>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_file_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_file_src_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_file_src_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_file_src_get_size);
#ifdef FILESRC_DECRYPTION
  gstbasesrc_class->fill = GST_DEBUG_FUNCPTR (gst_file_src_fill_ex);
#else
  gstbasesrc_class->fill = GST_DEBUG_FUNCPTR (gst_file_src_fill);
#endif
#ifdef FILESRC_MODIFICATION
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_file_src_query);
#endif
  if (sizeof (off_t) < 8) {
    GST_LOG ("No large file support, sizeof (off_t) = %" G_GSIZE_FORMAT "!",
        sizeof (off_t));
  }
}

static void
gst_file_src_init (GstFileSrc * src)
{
  src->filename = NULL;
  src->fd = 0;
  src->uri = NULL;

  src->is_regular = FALSE;
#ifdef FILESRC_DECRYPTION
  src->data_encryption_type = CONTENT_ENCRYPTION_NONE;
  src->data_encryption_key = NULL;
  src->data_encryption_iv = NULL;
  src->decryptor_init_done = FALSE;
#endif
  gst_base_src_set_blocksize (GST_BASE_SRC (src), DEFAULT_BLOCKSIZE);
}

static void
gst_file_src_finalize (GObject * object)
{
  GstFileSrc *src;

  src = GST_FILE_SRC (object);

  g_free (src->filename);
  g_free (src->uri);
#ifdef FILESRC_DECRYPTION
  gst_file_src_decryptor_deinit(object);
#endif
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_file_src_set_location (GstFileSrc * src, const gchar * location)
{
  GstState state;

  /* the element must be stopped in order to do this */
  GST_OBJECT_LOCK (src);
  state = GST_STATE (src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrong_state;
  GST_OBJECT_UNLOCK (src);

  g_free (src->filename);
  g_free (src->uri);

  /* clear the filename if we get a NULL */
  if (location == NULL) {
    src->filename = NULL;
    src->uri = NULL;
  } else {
    /* we store the filename as received by the application. On Windows this
     * should be UTF8 */
    src->filename = g_strdup (location);
    src->uri = gst_filename_to_uri (location, NULL);
    GST_INFO ("filename : %s", src->filename);
    GST_INFO ("uri      : %s", src->uri);
  }
  g_object_notify (G_OBJECT (src), "location");
  /* FIXME 0.11: notify "uri" property once there is one */

  return TRUE;

  /* ERROR */
wrong_state:
  {
    g_warning ("Changing the `location' property on filesrc when a file is "
        "open is not supported.");
    GST_OBJECT_UNLOCK (src);
    return FALSE;
  }
}

static void
gst_file_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFileSrc *src;

  g_return_if_fail (GST_IS_FILE_SRC (object));

  src = GST_FILE_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_file_src_set_location (src, g_value_get_string (value));
      break;
#ifdef FILESRC_DECRYPTION
    case PROP_ENCRYPTION:
      src->data_encryption_type = g_value_get_int (value);
      break;
    case PROP_ENCRYPTION_KEY:
      src->data_encryption_key = g_strdup (g_value_get_string (value));
      break;
    case PROP_ENCRYPTION_IV:
      src->data_encryption_iv = g_strdup (g_value_get_string (value));
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_file_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFileSrc *src;

  g_return_if_fail (GST_IS_FILE_SRC (object));

  src = GST_FILE_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->filename);
      break;
#ifdef FILESRC_DECRYPTION
    case PROP_ENCRYPTION:
      g_value_set_int (value, src->data_encryption_type);
      break;
    case PROP_ENCRYPTION_KEY:
      g_value_set_string (value, src->data_encryption_key);
      break;
    case PROP_ENCRYPTION_IV:
      g_value_set_string (value, src->data_encryption_iv);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/***
 * read code below
 * that is to say, you shouldn't read the code below, but the code that reads
 * stuff is below.  Well, you shouldn't not read the code below, feel free
 * to read it of course.  It's just that "read code below" is a pretty crappy
 * documentation string because it sounds like we're expecting you to read
 * the code to understand what it does, which, while true, is really not
 * the sort of attitude we want to be advertising.  No sir.
 *
 */
static GstFlowReturn
gst_file_src_fill (GstBaseSrc * basesrc, guint64 offset, guint length,
    GstBuffer * buf)
{
  GstFileSrc *src;
  guint to_read, bytes_read;
  int ret;
  GstMapInfo info;
  guint8 *data;

  src = GST_FILE_SRC_CAST (basesrc);

  if (G_UNLIKELY (offset != -1 && src->read_position != offset)) {
    off_t res;

    res = lseek (src->fd, offset, SEEK_SET);
    if (G_UNLIKELY (res < 0 || res != offset))
      goto seek_failed;

    src->read_position = offset;
  }

  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  data = info.data;

  bytes_read = 0;
  to_read = length;
  while (to_read > 0) {
    GST_LOG_OBJECT (src, "Reading %d bytes at offset 0x%" G_GINT64_MODIFIER "x",
        to_read, offset + bytes_read);
    errno = 0;
    ret = read (src->fd, data + bytes_read, to_read);
    if (G_UNLIKELY (ret < 0)) {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      goto could_not_read;
    }

    /* files should eos if they read 0 and more was requested */
    if (G_UNLIKELY (ret == 0)) {
      /* .. but first we should return any remaining data */
      if (bytes_read > 0)
        break;
      goto eos;
    }

    to_read -= ret;
    bytes_read += ret;

    src->read_position += ret;
  }

  gst_buffer_unmap (buf, &info);
  if (bytes_read != length)
    gst_buffer_resize (buf, 0, bytes_read);

  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset + bytes_read;

  return GST_FLOW_OK;

  /* ERROR */
seek_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    return GST_FLOW_ERROR;
  }
could_not_read:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unmap (buf, &info);
    gst_buffer_resize (buf, 0, 0);
    return GST_FLOW_ERROR;
  }
eos:
  {
    GST_DEBUG ("EOS");
    gst_buffer_unmap (buf, &info);
    gst_buffer_resize (buf, 0, 0);
    return GST_FLOW_EOS;
  }
}

#ifdef FILESRC_MODIFICATION
static gboolean
gst_file_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean ret = FALSE;
  GstFileSrc *src = GST_FILE_SRC (basesrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      gst_query_set_uri (query, src->uri);
      ret = TRUE;
      break;
    case GST_QUERY_CUSTOM: {
      GstStructure *s;
      gchar * file_path =0;
      s = gst_query_get_structure (query);
      if (gst_structure_has_name (s, "FileSrcURI")) {
        GValue value = { 0 };

        g_value_init (&value, G_TYPE_STRING);
        file_path = g_strdup_printf ("%s://%s", "file", src->filename);
        g_value_set_string (&value, file_path);

        GST_INFO_OBJECT (src, "Received supported custom query");
        gst_structure_set_value (s, "file-uri", &value);
        g_free(file_path);
        g_value_unset (&value);
        ret = TRUE;
      } else {
        GST_WARNING_OBJECT (src,"Unsupported query");
        ret = FALSE;
      }
      break;
    }
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);

  return ret;
}
#endif

static gboolean
gst_file_src_is_seekable (GstBaseSrc * basesrc)
{
  GstFileSrc *src = GST_FILE_SRC (basesrc);

  return src->seekable;
}

static gboolean
gst_file_src_get_size (GstBaseSrc * basesrc, guint64 * size)
{
  struct stat stat_results;
  GstFileSrc *src;

  src = GST_FILE_SRC (basesrc);

  if (!src->seekable) {
    /* If it isn't seekable, we won't know the length (but fstat will still
     * succeed, and wrongly say our length is zero. */
    return FALSE;
  }

  if (fstat (src->fd, &stat_results) < 0)
    goto could_not_stat;

  *size = stat_results.st_size;

  return TRUE;

  /* ERROR */
could_not_stat:
  {
    return FALSE;
  }
}

/* open the file, necessary to go to READY state */
static gboolean
gst_file_src_start (GstBaseSrc * basesrc)
{
  GstFileSrc *src = GST_FILE_SRC (basesrc);
  struct stat stat_results;

  if (src->filename == NULL || src->filename[0] == '\0')
    goto no_filename;

  GST_INFO_OBJECT (src, "opening file %s", src->filename);

  /* open the file */
  src->fd = gst_open (src->filename, O_RDONLY | O_BINARY, 0);

  if (src->fd < 0)
    goto open_failed;

  /* check if it is a regular file, otherwise bail out */
  if (fstat (src->fd, &stat_results) < 0)
    goto no_stat;

  if (S_ISDIR (stat_results.st_mode))
    goto was_directory;

  if (S_ISSOCK (stat_results.st_mode))
    goto was_socket;

  src->read_position = 0;

  /* record if it's a regular (hence seekable and lengthable) file */
  if (S_ISREG (stat_results.st_mode))
    src->is_regular = TRUE;

  /* We need to check if the underlying file is seekable. */
  {
    off_t res = lseek (src->fd, 0, SEEK_END);

    if (res < 0) {
      GST_LOG_OBJECT (src, "disabling seeking, lseek failed: %s",
          g_strerror (errno));
      src->seekable = FALSE;
    } else {
      res = lseek (src->fd, 0, SEEK_SET);

      if (res < 0) {
        /* We really don't like not being able to go back to 0 */
        src->seekable = FALSE;
        goto lseek_wonky;
      }

      src->seekable = TRUE;
    }
  }

  /* We can only really do seeking on regular files - for other file types, we
   * don't know their length, so seeking isn't useful/meaningful */
  src->seekable = src->seekable && src->is_regular;

  gst_base_src_set_dynamic_size (basesrc, src->seekable);

#ifdef FILESRC_DECRYPTION
  gst_file_src_decryptor_init(basesrc);
#endif

  return TRUE;

  /* ERROR */
no_filename:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        (_("No file name specified for reading.")), (NULL));
    goto error_exit;
  }
open_failed:
  {
    switch (errno) {
      case ENOENT:
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
            ("No such file \"%s\"", src->filename));
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
            (_("Could not open file \"%s\" for reading."), src->filename),
            GST_ERROR_SYSTEM);
        break;
    }
    goto error_exit;
  }
no_stat:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        (_("Could not get info on \"%s\"."), src->filename), (NULL));
    goto error_close;
  }
was_directory:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        (_("\"%s\" is a directory."), src->filename), (NULL));
    goto error_close;
  }
was_socket:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        (_("File \"%s\" is a socket."), src->filename), (NULL));
    goto error_close;
  }
lseek_wonky:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Could not seek back to zero after seek test in file \"%s\"",
            src->filename));
    goto error_close;
  }
error_close:
  close (src->fd);
error_exit:
  return FALSE;
}

/* unmap and close the file */
static gboolean
gst_file_src_stop (GstBaseSrc * basesrc)
{
  GstFileSrc *src = GST_FILE_SRC (basesrc);

  /* close the file */
  close (src->fd);

  /* zero out a lot of our state */
  src->fd = 0;
  src->is_regular = FALSE;

  return TRUE;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_file_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_file_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "file", NULL };

  return protocols;
}

static gchar *
gst_file_src_uri_get_uri (GstURIHandler * handler)
{
  GstFileSrc *src = GST_FILE_SRC (handler);

  /* FIXME: make thread-safe */
  return g_strdup (src->uri);
}

static gboolean
gst_file_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** err)
{
  gchar *location, *hostname = NULL;
  gboolean ret = FALSE;
  GstFileSrc *src = GST_FILE_SRC (handler);

  if (strcmp (uri, "file://") == 0) {
    /* Special case for "file://" as this is used by some applications
     *  to test with gst_element_make_from_uri if there's an element
     *  that supports the URI protocol. */
    gst_file_src_set_location (src, NULL);
    return TRUE;
  }

  location = g_filename_from_uri (uri, &hostname, err);

  if (!location || (err != NULL && *err != NULL)) {
    GST_WARNING_OBJECT (src, "Invalid URI '%s' for filesrc: %s", uri,
        (err != NULL && *err != NULL) ? (*err)->message : "unknown error");
    goto beach;
  }

  if ((hostname) && (strcmp (hostname, "localhost"))) {
    /* Only 'localhost' is permitted */
    GST_WARNING_OBJECT (src, "Invalid hostname '%s' for filesrc", hostname);
    g_set_error (err, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "File URI with invalid hostname '%s'", hostname);
    goto beach;
  }
#ifdef G_OS_WIN32
  /* Unfortunately, g_filename_from_uri() doesn't handle some UNC paths
   * correctly on windows, it leaves them with an extra backslash
   * at the start if they're of the mozilla-style file://///host/path/file 
   * form. Correct this.
   */
  if (location[0] == '\\' && location[1] == '\\' && location[2] == '\\')
    memmove (location, location + 1, strlen (location + 1) + 1);
#endif

  ret = gst_file_src_set_location (src, location);

beach:
  if (location)
    g_free (location);
  if (hostname)
    g_free (hostname);

  return ret;
}

static void
gst_file_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_file_src_uri_get_type;
  iface->get_protocols = gst_file_src_uri_get_protocols;
  iface->get_uri = gst_file_src_uri_get_uri;
  iface->set_uri = gst_file_src_uri_set_uri;
}

#ifdef FILESRC_DECRYPTION
static void
gst_file_src_decryptor_init (GstBaseSrc * basesrc)
{
  struct stat stat_results;
  guint64 file_size=0;
  int64_t ret=-1;
  GstFileSrc *src = GST_FILE_SRC (basesrc);
  switch(src->data_encryption_type){
    case CONTENT_ENCRYPTION_AES:
      if(!src->decryptor_init_done){
        unsigned char* deciphered_key = decipher_key(src->data_encryption_key);
        unsigned char* deciphered_iv = decipher_key(src->data_encryption_iv);
        if(deciphered_key && deciphered_iv)
        {
          aes_set_decrypt_key (&src->aes_ctx.ctx, 32, deciphered_key);
          CBC_SET_IV (&src->aes_ctx, deciphered_iv);

          src->decryptor_init_done = TRUE;

          ret = fstat (src->fd, &stat_results);
          if (ret < 0)
          {
            file_size = DECRYPTED_AES_CACHE_MAX;
            GST_WARNING_OBJECT (src, "Failed to get file using default  size %" G_GUINT64_FORMAT,file_size);
          }
          else
          {
            file_size = stat_results.st_size+1;
          }
          src->decrypted_data_cache = (data_cache*)g_malloc (sizeof(data_cache));
          src->decrypted_data_cache->cache_size = 0;
          src->decrypted_data_cache->cache_data = (guint8 *) g_malloc (file_size);
          GST_INFO_OBJECT (src, "AES context init done with cache size %" G_GUINT64_FORMAT,file_size);
          g_free(deciphered_key);
          g_free(deciphered_iv);
        }
        else
        {
          GST_INFO_OBJECT (src, "AES context init key error");
        }
      }
    break;
    case CONTENT_ENCRYPTION_NONE:
    break;
    default:
      GST_INFO_OBJECT (src, "Content encryption type not specified");
    break;
  }
  return;
}

static void
gst_file_src_decryptor_deinit (GObject * object)
{
  GstFileSrc *src = GST_FILE_SRC (object);
  switch(src->data_encryption_type){
    case CONTENT_ENCRYPTION_AES:
      if(src->decryptor_init_done){
        src->decryptor_init_done = FALSE;
        g_free(src->decrypted_data_cache->cache_data);
        src->decrypted_data_cache->cache_data =0;
        g_free(src->decrypted_data_cache);
        src->decrypted_data_cache = 0;
        if(src->data_encryption_key)
          g_free(src->data_encryption_key);
        src->data_encryption_key = 0;
        if(src->data_encryption_iv)
          g_free(src->data_encryption_iv);
        src->data_encryption_iv = 0;
        GST_INFO_OBJECT (src, "AES context de-init done");
      }
    break;
    case CONTENT_ENCRYPTION_NONE:
    break;
    default:
      GST_INFO_OBJECT (src, "Content encryption type not specified");
    break;
  }
  return;
}

static GstFlowReturn
gst_aes_file_src_fill (GstBaseSrc * basesrc, guint64 offset, guint length,
    GstBuffer * buf)
{
  GstFileSrc *src;
  guint to_read, bytes_read;
  int ret;
  GstMapInfo info;
  guint8 *data;
  guint8* file_data = 0;

  src = GST_FILE_SRC_CAST (basesrc);

  GST_LOG_OBJECT (src, "Reading %d bytes at offset %" G_GUINT64_FORMAT, length, offset);

  while(offset+length > src->decrypted_data_cache->cache_size){
    GST_LOG_OBJECT (src, "cache size %d is smaller so reading from file",src->decrypted_data_cache->cache_size);
    bytes_read = 0;
    to_read = ENCRYPTED_DATA_READ_SIZE;
    file_data = (guint8 *) g_malloc (ENCRYPTED_DATA_READ_SIZE+1);
    errno = 0;
    ret = read (src->fd, file_data, to_read);

    if (G_UNLIKELY (ret < 0)) {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      goto could_not_read;
    }

    if (G_UNLIKELY (ret == 0)) {
      goto eos;
    }

    if(ret){
      CBC_DECRYPT (&src->aes_ctx, aes_decrypt, ret,
                          src->decrypted_data_cache->cache_data+src->decrypted_data_cache->cache_size,
                          file_data);
      src->decrypted_data_cache->cache_size+=ret;
      bytes_read+=ret;
    }else{
      GST_INFO_OBJECT (src, "reached end of file");
      break;
    }
  }

  gst_buffer_map (buf, &info, GST_MAP_WRITE);
  data = info.data;

  memcpy(data,src->decrypted_data_cache->cache_data+offset,length);
  bytes_read = length;

  gst_buffer_unmap (buf, &info);
  if (bytes_read != length)
    gst_buffer_resize (buf, 0, bytes_read);

  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset + bytes_read;

  if(file_data)
    g_free(file_data);

  return GST_FLOW_OK;

  /* ERROR */
could_not_read:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unmap (buf, &info);
    gst_buffer_resize (buf, 0, 0);
    return GST_FLOW_ERROR;
  }
eos:
  {
    GST_DEBUG ("EOS");
    gst_buffer_unmap (buf, &info);
    gst_buffer_resize (buf, 0, 0);
    return GST_FLOW_EOS;
  }
}

static GstFlowReturn
gst_file_src_fill_ex (GstBaseSrc * basesrc, guint64 offset, guint length,
    GstBuffer * buf)
{
  GstFileSrc *src = GST_FILE_SRC_CAST (basesrc);
  if(CONTENT_ENCRYPTION_AES == src->data_encryption_type)
    return gst_aes_file_src_fill(basesrc,offset,length,buf);
  return gst_file_src_fill(basesrc,offset,length,buf);
}
#endif
