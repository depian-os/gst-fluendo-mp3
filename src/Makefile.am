plugin_LTLIBRARIES = libgstflump3dec.la

DECODER_SOURCE_FILES = \
    mp3-c.c \
    mp3-c-synth.c \
    bitstream.c \
    common.c \
    decode.c \
    mp3tl.c

libgstflump3dec_la_SOURCES = \
    flump3dec.c \
    $(DECODER_SOURCE_FILES)

noinst_HEADERS = \
    flump3dec-0_10.c \
    flump3dec-1_0.c \
    bitstream.h \
    common.h \
    decode.h \
    flump3dec.h \
    mp3tl.h \
    mp3tl-priv.h \
    table-bitalloc.h \
    table-dewindow.h \
    table-huffdec.h \
    mp3-cos-tables.h \
    mp3-c.h \
    table-powtable.h \
    table-powtable-2.h

libgstflump3dec_la_CFLAGS = \
   -I$(top_srcdir)/common \
    $(GST_CFLAGS) \
    $(GST_PLUGINS_BASE_CFLAGS) \
    $(GST_AUDIO_CFLAGS) \
    $(ERROR_CFLAGS) \
    $(CPU_TUNE_CFLAGS) \
    $(DECODER_INCLUDES)

libgstflump3dec_la_LIBADD = \
    $(GST_LIBS) \
    $(GST_AUDIO_LIBS) \
    $(DECODER_LIBS) \
    -lm

libgstflump3dec_la_LDFLAGS = \
    $(GST_PLUGIN_LDFLAGS) \
    $(CPU_TUNE_LDFLAGS)

libgstflump3dec_la_CCASFLAGS = \
    $(CPU_TUNE_CCASFLAGS)
