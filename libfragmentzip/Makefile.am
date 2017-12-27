AM_CFLAGS = $(libzip_CFLAGS) $(curl_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS = $(libzip_LIBS) $(curl_LIBS)

lib_LTLIBRARIES = libfragmentzip.la
libfragmentzip_la_CFLAGS = $(AM_CFLAGS)
libfragmentzip_la_LDFLAGS = $(AM_LDFLAGS)
libfragmentzip_la_SOURCES = libfragmentzip.c

if WIN32
libfragmentzip_la_CFLAGS += -mno-ms-bitfields # mingw64 __attribute__((packed)) bug
libfragmentzip_la_LDFLAGS += -no-undefined
endif