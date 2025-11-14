#ifndef _GST_FLUENDO_H_
#define _GST_FLUENDO_H_

#if (GST_VERSION_MAJOR == 1)
#define GST_PLUGIN_DEFINE_WRAP(major, minor, name, desc, init, version, license, pkg ,url) \
  GST_PLUGIN_DEFINE(major, minor, name, desc, init, version, license, pkg, url)
#else
#define GST_PLUGIN_DEFINE_WRAP(major, minor, name, desc, init, version, license, pkg, url) \
  GST_PLUGIN_DEFINE(major, minor, #name, desc, init, version, license, pkg, url)
#endif

#ifndef GST_PLUGIN_DEFINE2
#if defined(GST_PLUGIN_BUILD_STATIC)
#define GST_PLUGIN_DEFINE2(major, minor, name, desc, init, version, license, pkg, url) \
    gboolean gst_##name##_init_static() { \
      return gst_plugin_register_static(major, minor, #name, desc, init, \
          version, license, "Fluendo", pkg, url); \
    }
#else
#define GST_PLUGIN_DEFINE2 GST_PLUGIN_DEFINE_WRAP
#endif
#endif

#if ENABLE_DEMO_PLUGIN
#define FLUENDO_PLUGIN_DEFINE(major, minor, name, fun, desc, init, version, \
  license,pkg,url) \
  GST_PLUGIN_DEFINE2(major, minor, fun, desc " [Demo Version]", init, version, license, pkg, url)
#else
#define FLUENDO_PLUGIN_DEFINE(major, minor, name, fun, desc, init, version, \
  license,pkg,url) \
  GST_PLUGIN_DEFINE2(major, minor, fun, desc, init, version, license, pkg, url)
#endif

#if ENABLE_DEMO_PLUGIN && (!defined (_GST_DEMO_H_))
#warning "To support Demo plugins include gst-demo.h"
#endif

#define GSTFLU_SETUP_STATISTICS(sink, stats)
#define GSTFLU_PAD_PUSH(src, buf, stats) gst_pad_push (src, buf)
#define GSTFLU_STATISTICS


#ifndef GST_COMPAT_H
#ifndef POST_10_14
#ifndef POST_1_0
#define gst_element_class_set_details_simple( \
    klass, longname, classification, description, author) \
    { \
      GstElementDetails details = { \
        longname, classification, description, author}; \
      gst_element_class_set_details (klass, &details); \
    }
#endif
#endif
#endif

#endif /* _GST_FLUENDO_H_ */
