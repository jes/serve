bin_PROGRAMS = serve

noinst_PROGRAMS = bin2c

serve_SOURCES = auth.c \
	cgi.c \
	compression.c \
	genpage.c \
	handler.c \
	headers.c \
	images.c \
	init.c \
	log.c \
	md5.c \
	mimetypes.c \
	nextline.c \
	request.c \
	send.c \
	serve.c \
	md5.h \
	serve.h

nodist_serve_SOURCES = images.h

BUILT_SOURCES = images.h

IMAGES = ../misc/folder.png \
	../misc/image.png \
	../misc/text.png \
	../misc/binary.png \
	../misc/video.png \
	../misc/audio.png

images.h: bin2c $(IMAGES)
	../misc/make_images | cat images.h.head - > $@

bin2c_SOURCES =	bin2c.c

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
serve_LDFLAGS = $(all_libraries) 

EXTRA_DIST = zlib.supp
