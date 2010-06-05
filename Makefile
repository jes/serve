#Makefile for serve
#James Stanley 2010

################################################################################
#Enable this to use libmagic for automatic mimetype detection
LIBMAGIC=no

#Enable this to use zlib for gzip compression
ZLIB=no

#Enable this to use the sendfile() function instead of read()/write()
SENDFILE=no

#Set this to the bin directory you want serve installed in
BINDIR=/usr/bin

#Set this to the directory you want serve_mimetypes installed in
ETCDIR=/etc
################################################################################

CFLAGS=-g -Wall -DETCDIR=$(ETCDIR)
OBJS=src/auth.o src/cgi.o src/compression.o src/genpage.o src/handler.o \
	src/headers.o src/images.o src/init.o src/log.o src/md5.o \
	src/mimetypes.o src/nextline.o src/request.o src/send.o src/serve.o

ifeq ($(LIBMAGIC),yes)
LDFLAGS+=-lmagic
CFLAGS+=-DUSE_LIBMAGIC
endif

ifeq ($(ZLIB),yes)
LDFLAGS+=-lz
CFLAGS+=-DUSE_GZIP
endif

ifeq ($(SENDFILE),yes)
CFLAGS+=-DUSE_SENDFILE
endif

src/serve: $(OBJS)

src/bin2c: src/bin2c.o

clean:
	-rm -f $(OBJS) src/serve src/bin2c src/bin2c.o
.PHONY:	clean

install:
	install -m 0755 src/serve $(BINDIR)/serve
	install -m 0644 misc/serve_mimetypes $(ETCDIR)/serve_mimetypes
.PHONY: install
