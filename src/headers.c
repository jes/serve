/* Stuff for maintaining lists of HTTP headers for serve

   By James Stanley

   Public domain */

#include "serve.h"

/* adds the given header and value to the list;
   These will be free'd when free_headers() is called, so make sure that's
   safe */
void add_header(header **list, int num, char *name, char *value) {
  *list = realloc(*list, (num + 1) * 2 * sizeof(char*));

  (*list)[num].name = name;
  (*list)[num].value = value;
}

/* frees the given header list, including all the headers and values */
void free_headers(header *list, int num) {
  int i;

  if(!list) return;

  for(i = 0; i < num; i++) {
    free(list[i].name);
    free(list[i].value);
  }

  free(list);
}

/* gets the next header from the given file descriptor and adds it to the given
   list; returns > 0 if there are more headers, = 0 if there are no more
   headers, -1 if there was a bad request, -2 if the connection was dropped
   prematurely, and -3 if this is the first line to be read and it is blank
   If the return value is -1 and the parameter lin is non-NULL , then a pointer
   to the last line to be read (with endline stripped) is put in lin, which you
   must free yourself */
int next_header(int fd, char **lin, header **list, int num, size_t *length) {
  static char *line;
  char *header = NULL;
  char *value = NULL;
  char *ptr;
  size_t size = 0;
  size_t len = 0;

  /* sort out the last line that was read */
  if(line) {
    if(*line == '\0') {
      free(line);
      line = NULL;
      return 0;
    }
  } else {
    if((line = stripendl(nextline(fd))) == NULL) return -2;
  }

  if(*line == '\0') {
    free(line);
    line = NULL;
    return -3;
  }

  /* find the end of the header name */
  ptr = strchr(line, ':');
  if(!ptr) {
    if(lin) *lin = line;
    else free(line);
    line = NULL;
    return -1;
  }

  /* copy the header name */
  header = strdup2(line, ptr - line);

  ptr++;/* skip the colon */
  while(iswhite(*ptr)) ptr++;/* skip whitespace */

  /* go through each line that contains the contents for this header */
  do {
    /* add on the value */
    len = strlen(ptr);
    value = realloc(value, size + len + 1);
    strcpy(value + size, ptr);
    size += len;
    /* get the next line */
    free(line);
    if((line = stripendl(nextline(fd))) == NULL) return -2;
    for(ptr = line; iswhite(*(ptr+1)); ptr++);/* skip whitespace, except one */
  } while(!strchr(line, ':') && *line);

  /* now add the header to the list */
  add_header(list, num, header, value);

  /* set *length */
  if(length && (strcasecmp(header, "Content-Length") == 0))
    *length = strtoull(value, NULL, 10);

  if(*line) return 1;/* more headers */
  else {/* end of headers */
    free(line);
    line = NULL;
    return 0;
  }
}
