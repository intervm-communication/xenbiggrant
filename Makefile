MAJOR    = 0
MINOR    = 1
SHLIB_LDFLAGS = -shared

CFLAGS   += -Werror -Wmissing-prototypes
CFLAGS   += -I./include -I./include/userspace

VPATH     = src

SRCS-y   := xenbiggrant.c

LIB_OBJS := $(patsubst %.c,%.o,$(SRCS-y))
PIC_OBJS := $(patsubst %.c,%.opic,$(SRCS-y))

# Temporary; for out-of-tree builds.
SYMLINK_SHLIB := ln -sf
SONAME_LDFLAG := -soname

LIB := libxenbiggrant.a
ifneq ($(nosharedlibs),y)
LIB += libxenbiggrant.so
endif

.PHONY: all
all: build

.PHONY: build
build:
	$(MAKE) libs
	$(MAKE) user_biggrantout

.PHONY: libs
libs: headers.chk $(LIB)

# XXX temporary XXX
user_biggrantout: tests/user_biggrantout.c libxenbiggrant.so
	LIBRARY_PATH=$(shell pwd) $(CC) $< $(CFLAGS) -Wl,-rpath=$(shell pwd) -lxengnttab -lxenbiggrant -lxentoollog -o user_biggrantout -o $@

headers.chk: $(wildcard include/*.h)

libxenbiggrant.a: $(LIB_OBJS)
	$(AR) rc $@ $^

libxenbiggrant.so: libxenbiggrant.so.$(MAJOR)
	$(SYMLINK_SHLIB) $< $@
libxenbiggrant.so.$(MAJOR): libxenbiggrant.so.$(MAJOR).$(MINOR)
	$(SYMLINK_SHLIB) $< $@

libxenbiggrant.so.$(MAJOR).$(MINOR): $(PIC_OBJS)
	$(CC) $(LDFLAGS) -Wl,$(SONAME_LDFLAG) -Wl,libxenbiggrant.so.$(MAJOR) $(SHLIB_LDFLAGS) -o $@ $(PIC_OBJS) $(LDLIBS_libxentoollog) $(APPEND_LDFLAGS)

%.opic: %.c
	$(CC) $(CPPFLAGS) -DPIC $(CFLAGS) $(CFLAGS_$*.opic) -fPIC -c -o $@ $< $(APPEND_CFLAGS)

.PHONY: install
install: build
	$(INSTALL_DIR) $(DESTDIR)$(libdir)
	$(INSTALL_DIR) $(DESTDIR)$(includedir)
	$(INSTALL_SHLIB) libxenbiggrant.so.$(MAJOR).$(MINOR) $(DESTDIR)$(libdir)
	$(INSTALL_DATA) libxenbiggrant.a $(DESTDIR)$(libdir)
	$(SYMLINK_SHLIB) libxenbiggrant.so.$(MAJOR).$(MINOR) $(DESTDIR)$(libdir)/libxenbiggrant.so.$(MAJOR)
	$(SYMLINK_SHLIB) libxenbiggrant.so.$(MAJOR) $(DESTDIR)$(libdir)/libxenbiggrant.so
	$(INSTALL_DATA) include/xenbiggrant.h $(DESTDIR)$(includedir)

.PHONY: TAGS
TAGS:
	etags -t *.c *.h

.PHONY: clean
clean:
	rm -rf *.rpm $(LIB) *~ $(DEPS) $(LIB_OBJS) $(PIC_OBJS)
	rm -f libxenbiggrant.so.$(MAJOR).$(MINOR) libxenbiggrant.so.$(MAJOR)
	rm -f headers.chk

.PHONY: distclean
distclean: clean
