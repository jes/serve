/* Mime type stuff for serve

   By James Stanley
	 
   Public domain */

#include "serve.h"

char **mimetype;
int mimetypes;
int longest_ext;

/* Loads mime types from all of the files
   Later-loaded mimetypes overwrite earlier-loaded ones */
void load_mimetypes(void) {
  load_mimetypes_from("/etc/serve_mimetypes");
  load_mimetypes_from(".mimetypes");
}

/* Returns the filename for the image for files of the given type. Make a copy
   of the returned string if you wish to modify it, as this function returns
   pointers to read-only strings */
char *type_image(const char *filename) {
  char *ctype;

  ctype = content_type(filename);

  if(strncmp(ctype, "image/", 6) == 0) return "image.png";
  if(strncmp(ctype, "text/", 5) == 0) return "text.png";
  if(strncmp(ctype, "video/", 6) == 0) return "video.png";
  if(strncmp(ctype, "audio/", 6) == 0) return "audio.png";
	
  return "binary.png";
}

/* Returns the general type of the given file. Make a copy if you wish to modify
   it, because this function returns pointers to read-only data */
char *general_type(const char *filename) {
  char *ctype;

  ctype = content_type(filename);

  if(strncmp(ctype, "image/", 6) == 0) return "IMG";
  if(strncmp(ctype, "text/", 5) == 0) return "TXT";
  if(strncmp(ctype, "video/", 6) == 0) return "VID";
  if(strncmp(ctype, "audio/", 6) == 0) return "SND";
	
  return "BIN";
}

/* Loads the mime types (and CGI handlers) from a given file
   Expects lines like:
   #this is a comment line
   text/html   html
   /bin/sh     sh */
void load_mimetypes_from(const char *filename) {
  int fd;
  char *line;
  char *ptr, *end;
  char *ext, *type;
  int linenum = 0;

  fd = open(filename, O_RDONLY);

  /* don't bother if we can't load it */
  if(fd == -1) {
    log_text(err, "Can't load MIME types from '%s' because it can't be opened "
             "for reading.", filename);
    return;
  }

  log_text(out, "Loading MIME types from file '%s'.", filename);

  /* go through each line of the file and add the mimetype pair */
  while((line = stripendl(nextline(fd)))) {
    linenum++;
    for(ptr = line; *ptr && iswhite(*ptr); ptr++);/* skip whitespace */

    if(*ptr && *ptr != '#') {/* skip blank lines and comment lines */
      /* firstly, get mime type */
      for(end = ptr; *end && !iswhite(*end); end++);/* find end of text */
      if(!*end) {/* if we reach the end, complain */
        printf("warning: %s:%d: ignoring invalid line\n", filename, linenum);
      } else {/* all good */
        *end = '\0';/* overwrite first blank with NUL */
        type = ptr;

        /* and now get the file extension */
        for(ptr = end+1; *ptr && iswhite(*ptr); ptr++);/* skip whitespace */
        for(end = ptr; *end && !iswhite(*end); end++);/* find end of text */
        *end = '\0';/* overwrite first blank with a NUL */
        ext = ptr;

        /* and add the mimetype */
        add_mimetype(type, ext);
      }
    }
    free(line);
  }

  /* our work here is done */
  close(fd);
}

/* adds the given mimetype to the mimetype list */
void add_mimetype(const char *type, const char *ext) {
  int i;

  if(strlen(ext) > longest_ext) longest_ext = strlen(ext);

  /* check if this file extension is already taken */
  for(i = 0; i < mimetypes; i++) {
    if(strcasecmp(EXT(i), ext) == 0) {/* replace the mimetype */
      TYPE(i) = strdup(type);
      return;
    }
  }
 
  i = mimetypes;/* copy the mimetype count */

  /* reallocate the array */
  mimetype = realloc(mimetype, sizeof(char*) * 2 * ++mimetypes);

  /* and copy the stuff */
  TYPE(i) = strdup(type);
  EXT(i)  = strdup(ext);
}

/* returns a pointer to the content type; do NOT modify the value of the data at
   the returned pointer because it WILL cause one of a number of possible
   problems.  */
char *content_type(const char *file) {
  char *ext;
  int i;

  if(!file) return NULL;

  /* find the final dot */
  ext = strrchr(file, '.');
  if(!ext) return magic_content_type(file);/* no file extension */

  /* skip the dot */
  ext++;

  /* and find the mime type */
  for(i = 0; i < mimetypes; i++) {
    if(strcasecmp(EXT(i), ext) == 0) return TYPE(i);
  }

  /* unrecognised file extension */
  return magic_content_type(file);
}

/* Uses libmagic to get the content type of the file */
char *magic_content_type(const char *file) {
#ifndef USE_LIBMAGIC
  /* we're not using libmagic */
  return "application/octet-stream";
#else
  static magic_t cookie;
  char *type;

  if(!cookie) {
    /* Valgrind complains about bytes not freed to do with libmagic; this
       doesn't matter because they are re-used instead of being re-allocated */
    cookie = magic_open(MAGIC_MIME_TYPE | MAGIC_SYMLINK |
                        MAGIC_NO_CHECK_COMPRESS	| MAGIC_NO_CHECK_TAR |
                        MAGIC_NO_CHECK_TROFF);
    magic_load(cookie, NULL);
  }

  type = (char*)magic_file(cookie, file);

  if(!type) type = "application/octet-strem";

  return type;
#endif
}
