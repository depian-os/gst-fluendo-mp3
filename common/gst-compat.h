/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef GST_COMPAT_H
#define GST_COMPAT_H

#include <string.h>

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#if !GLIB_CHECK_VERSION(2,14,0)
#define g_once_init_enter(_token) (!(*(_token)))
#define g_once_init_leave(_token,_value) (*(_token)=(_value))
#endif

#if !GLIB_CHECK_VERSION(2,18,0)
typedef unsigned long guintptr;
#endif

#if (!GLIB_CHECK_VERSION(2,28,0))
static inline void
g_list_free_full (GList * list, GDestroyNotify free_func)
{
  GList *next = list;
  while (next) {
    free_func (next->data);
    next = g_list_remove_link (next, next);
  }
}
#endif

#ifndef GST_CHECK_VERSION
#define GST_CHECK_VERSION(major,minor,micro)  \
    (GST_VERSION_MAJOR > (major) || \
     (GST_VERSION_MAJOR == (major) && GST_VERSION_MINOR > (minor)) || \
     (GST_VERSION_MAJOR == (major) && GST_VERSION_MINOR == (minor) && \
      GST_VERSION_MICRO >= (micro)))
#endif

#if !GST_CHECK_VERSION(0,10,4)
#define GST_QUERY_TYPE_NAME(query) (gst_query_type_get_name(GST_QUERY_TYPE(query)))
#endif

#if !GST_CHECK_VERSION(0,10,6)
static inline GstBuffer *
gst_adapter_take_buffer (GstAdapter * adapter, guint nbytes)
{
  GstBuffer *buf = NULL;

  if (G_UNLIKELY (nbytes > adapter->size))
    return NULL;

  buf = gst_buffer_new_and_alloc (nbytes);

  if (G_UNLIKELY (!buf))
    return NULL;

  /* Slow... */
  memcpy (GST_BUFFER_DATA (buf), gst_adapter_peek (adapter, nbytes), nbytes);

  return buf;
}
#endif

#if !GST_CHECK_VERSION(0,10,7)
#define GST_FLOW_CUSTOM_SUCCESS ((GstFlowReturn)100)
#define GST_FLOW_CUSTOM_ERROR ((GstFlowReturn)-100)
#define GST_FLOW_IS_SUCCESS(ret) ((ret) >= GST_FLOW_OK)
#endif

#if !GST_CHECK_VERSION(0,10,9)
#define GST_BUFFER_IS_DISCONT(buffer) \
    (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT))
#endif

#if !GST_CHECK_VERSION(0,10,10)
static inline GstPad *
gst_ghost_pad_new_from_template (const gchar * name, GstPad * target,
    GstPadTemplate * templ)
{
  GstPad *ret;

  g_return_val_if_fail (GST_IS_PAD (target), NULL);
  g_return_val_if_fail (!gst_pad_is_linked (target), NULL);
  g_return_val_if_fail (templ != NULL, NULL);
  g_return_val_if_fail (GST_PAD_TEMPLATE_DIRECTION (templ) ==
      GST_PAD_DIRECTION (target), NULL);

  if ((ret = gst_ghost_pad_new_no_target (name, GST_PAD_DIRECTION (target)))) {
    if (!gst_ghost_pad_set_target (GST_GHOST_PAD (ret), target))
      goto set_target_failed;
    g_object_set (ret, "template", templ, NULL);
  }

  return ret;

  /* ERRORS */
set_target_failed:
  {
    gst_object_unref (ret);
    return NULL;
  }
}
#endif

#if !GST_CHECK_VERSION(0,10,11)
#define gst_message_new_buffering(elem,perc) \
    gst_message_new_custom (GST_MESSAGE_BUFFERING,         \
        (elem),                                            \
        gst_structure_new ("GstMessageBuffering",          \
            "buffer-percent", G_TYPE_INT, (perc), NULL))

#endif

#if !GST_CHECK_VERSION(0,10,14)
#define gst_element_class_set_details_simple(klass,longname,classification,description,author) \
    G_STMT_START{                                                             \
      static GstElementDetails details =                                      \
          GST_ELEMENT_DETAILS (longname,classification,description,author);   \
      gst_element_class_set_details (klass, &details);                        \
    }G_STMT_END
#endif

#if !GST_CHECK_VERSION(0,10,15)
#define gst_structure_get_uint(stru,fn,fv) \
    gst_structure_get_int(stru,fn,(gint*)fv)
#endif

#if !GST_CHECK_VERSION(0,10,20)
static inline gboolean
gst_event_has_name (GstEvent * event, const gchar * name)
{
  g_return_val_if_fail (GST_IS_EVENT (event), FALSE);

  if (event->structure == NULL)
    return FALSE;

  return gst_structure_has_name (event->structure, name);
}
#endif

#if !GST_CHECK_VERSION(0,10,23)
#define GST_BUFFER_FLAG_MEDIA1 (GST_MINI_OBJECT_FLAG_LAST << 5)
#define GST_BUFFER_FLAG_MEDIA2 (GST_MINI_OBJECT_FLAG_LAST << 6)
#define GST_BUFFER_FLAG_MEDIA3 (GST_MINI_OBJECT_FLAG_LAST << 7)

#define GST_MEMDUMP(_title, _data, _length) while (0)
#define GST_MEMDUMP_OBJECT(_object, _title, _data, _length) while (0)
#endif

#if !GST_CHECK_VERSION(0,10,24)
static inline void
gst_object_ref_sink (gpointer object)
{
  g_return_if_fail (GST_IS_OBJECT (object));

  GST_OBJECT_LOCK (object);
  if (G_LIKELY (GST_OBJECT_IS_FLOATING (object))) {
    GST_OBJECT_FLAG_UNSET (object, GST_OBJECT_FLOATING);
    GST_OBJECT_UNLOCK (object);
  } else {
    GST_OBJECT_UNLOCK (object);
    gst_object_ref (object);
  }
}
#endif

#if !GST_CHECK_VERSION(0,10,26)
static inline void
gst_caps_set_value (GstCaps *caps, const char *field, const GValue *value)
{
  GstStructure *structure;
  structure = gst_caps_get_structure (caps, 0);
  gst_structure_set_value (structure, field, value);
}
#endif

#if !GST_CHECK_VERSION(0,10,30)
static inline GstStructure *
gst_caps_steal_structure (GstCaps * caps, guint index)
{
  GstStructure *s;
  g_return_val_if_fail (caps != NULL, NULL);
  g_return_val_if_fail ((g_atomic_int_get (&(caps)->refcount) == 1), NULL);

  if (G_UNLIKELY (index >= caps->structs->len))
    return NULL;

  s = (GstStructure *)g_ptr_array_remove_index (caps->structs, index);
  gst_structure_set_parent_refcount (s, NULL);
  return s;
}

#define GST_TRACE_OBJECT(...) G_STMT_START{ }G_STMT_END
#define GST_TRACE(...) G_STMT_START{ }G_STMT_END
#endif

#if !GST_CHECK_VERSION(0,10,33)
#define GST_MINI_OBJECT_FLAG_RESERVED1 (1<<1)
#define GST_BUFFER_FLAG_MEDIA4 GST_MINI_OBJECT_FLAG_RESERVED1
#endif

#if !GST_CHECK_VERSION (1,0,0)

typedef struct {
  guint8 * data;
  gsize size;
} GstMapInfo;

typedef enum {
  GST_MAP_READ = 1 << 0,
  GST_MAP_WRITE = 1 << 1
} GstMapFlags;

static inline gboolean
gst_buffer_map (GstBuffer * buffer, GstMapInfo * info, GstMapFlags flags)
{
  info->data = GST_BUFFER_DATA (buffer);
  info->size = GST_BUFFER_SIZE (buffer);
  return TRUE;
}

#define gst_buffer_unmap(buffer,info) while(0)
#define gst_buffer_get_size(buffer) ((gsize) GST_BUFFER_SIZE((buffer)))

#define GST_FLOW_EOS GST_FLOW_UNEXPECTED
#define GST_FLOW_FLUSHING GST_FLOW_WRONG_STATE

static inline GstBuffer *
gst_buffer_new_wrapped (gpointer data, gsize size)
{
  GstBuffer * buffer = gst_buffer_new ();

  GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) data;
  GST_BUFFER_SIZE (buffer) = size;

  return buffer;
}

#define gst_adapter_map gst_adapter_peek
#define gst_adapter_unmap(adapter) while(0)
#define gst_segment_do_seek gst_segment_set_seek
#define gst_tag_list_new_empty gst_tag_list_new

#endif
#endif /* GST_COMPAT_H */
