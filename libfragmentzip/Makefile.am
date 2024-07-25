AM_CFLAGS  = -I$(top_srcdir)/include $(curl_CFLAGS) $(libgeneral_CFLAGS) $(zlib_CFLAGS)
AM_LDFLAGS = $(curl_LIBS) $(zlib_LIBS)

lib_LTLIBRARIES = libfragmentzip.la
libfragmentzip_la_CFLAGS = $(AM_CFLAGS)
libfragmentzip_la_LDFLAGS = $(AM_LDFLAGS)
libfragmentzip_la_SOURCES = libfragmentzip.c

if WIN32
libfragmentzip_la_CFLAGS += $(AM_CFLAGS) -DCURL_STATICLIB -mno-ms-bitfields #mingw64 __attribute__((packed)) bug
libfragmentzip_la_LDFLAGS += $(AM_LDFLAGS) -no-undefined
endif
