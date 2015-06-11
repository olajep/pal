if ENABLE_DEV_EPIPHANY
    MAYBE_EPIPHANY = epiphany
    MAYBE_LIB_EPIPHANY = epiphany/libdevice.la
endif

if ENABLE_DEV_PTHREADS
    MAYBE_EPIPHANY = pthreads
    MAYBE_LIB_EPIPHANY = pthreads/libdevice.la
endif

SUBDIRS = \
    $(MAYBE_EPIPHANY) \
    $(MAYBE_PTHREADS)

noinst_LTLIBRARIES = libdevices.la
libdevices_la_SOURCES = dummy.c
libdevices_la_LIBADD = \
    $(MAYBE_LIB_EPIPHANY) \
    $(MAYBE_LIB_PTHREADS)
