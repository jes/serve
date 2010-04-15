/* Generates pages for serve

   By James Stanley */

#include "serve.h"

char *size[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
int num_sizes = 9;

/* adds a formatted string to the given string, and reallocates the given string
   such that it is big enough */
void add_text(char **page, size_t *pagelen, char *fmt, ...) {
  va_list args;
  char *line;
  int len;
  size_t oldlen = *pagelen;

  line = malloc(1024);

  va_start(args, fmt);

  /* let vsnprintf convert to a string */
  if((len = vsnprintf(line, 1024, fmt, args)) > 1024) {
    line = realloc(line, len + 1);
    vsnprintf(line, len + 1, fmt, args);
  }
  *pagelen += len;
	
  /* and now re-allocate the main string and add the new string */
  *page = realloc(*page, *pagelen + 1);
  strcpy(*page + oldlen, line);

  free(line);
		
  va_end(args);
}

/* Decides whether to send a dir listing or the index page and acts
   accordingly */
void send_dir(request *r) {
  char *file;
  int fildes;
  char *ptr;
  int i;

  /* try to get the index page */
  file = malloc(strlen(r->file) + /* strlen("/index.") */ 7 + longest_ext + 1);
  sprintf(file, "%s/index.%n", r->file, &i);
  ptr = file + i;/* ptr is the place where file extension should go */
  i = 0;

  /* go through each mimetype and try to open index */
  while(i < mimetypes) {
    strcpy(ptr, EXT(i++));
    fildes = open(file, O_RDONLY);
    if(fildes != -1) {/* index page can be opened for reading, send it */
      close(fildes);
      /* sort out our request structure */
      free(r->file);
      r->file = file;
      file_stuff(r);
      send_file(r);
      return;
    }
  }

  /* index page doesn't exist or can't be read, send dir list */
  free(file);
  send_dirlist(r);
}

/* Selects all non-dotfile entries */
int nonhidden(const struct dirent *d) {
  if(*(d->d_name) == '.' && strcmp(d->d_name, "..") != 0) return 0;

  return 1;
}

/* Sort entries in a directory;
   directories first, and then by name */
int dirsort(const struct dirent **a, const struct dirent **b) {
  /* both the same type, go alphabetically */
  if((*a)->d_type == (*b)->d_type) return alphasort((void*)a, (void*)b);

  /* a is a dir, b is not */
  if((*a)->d_type == DT_DIR) return -1;

  /* b is a dir, a is not */
  if((*b)->d_type == DT_DIR) return 1;

  /* different types but neither is directory, alphabetical */
  return alphasort((void*)a, (void*)b);
}

/* Returns the textual file size of the given dirent structure.
   Return value is not to be modified and may be a static buffer. */
char *file_size(const char *file) {
  static char val[32];
  struct stat stat_buf;
  float fsize;
  int mul = 0;

  /* some files can't be done */
  if(stat(file, &stat_buf) == -1) return "-";
  if(stat_buf.st_mode & S_IFDIR) return "-";

  /* now fix it to be in bytes, kilobytes, etc. */
  fsize = stat_buf.st_size;

  while((mul < num_sizes) && (fsize > 1023)) {
    fsize /= 1024;
    mul++;
  }

  snprintf(val, 32, "%.1f%s", fsize, size[mul]);

  return val;
}

/* Generates and sends a directory listing */
void send_dirlist(request *r) {
  int i;
  char *page = NULL;
  size_t len = 0;
  char *reqfile;
  int numfiles = 0;
  struct dirent **file = NULL;
  char *fname = NULL;
  char *p;

  /* redirect if the URL contains GET parameters */
  if((p = strchr(r->reqfile, '?'))) {
    reqfile = strdup(r->reqfile);
    p = strchr(reqfile, '?');
    *p = '\0';
    free(r->location);
    r->location = strdup(reqfile);
    r->status = 301;
    send_errorpage(r);
    free(reqfile);
    return;
  }
 
  /* obtain and sort the dirents */
  if((numfiles = scandir(r->file, &file, nonhidden, dirsort)) == -1) {
    r->status = 403;
    send_errorpage(r);
    return;
  }

  /* and generate the page */
  add_text(&page, &len,
           "<html><head><title>Index of %s</title></head></html>\n",
           r->reqfile);
  add_text(&page, &len, "<body><h1>Index of %s</h1></body>\n", r->reqfile);
  add_text(&page, &len, "<table>\n");

  /* generate the page */
  for(i = 0; i < numfiles; i++) {
    /* skip ".." if we are in the root */
    if((strcmp(r->reqfile, "/") == 0) &&
       (strcmp(file[i]->d_name, "..") == 0)) continue;

    /* path to this file */
    fname = realloc(fname, strlen(r->file) + 1 + strlen(file[i]->d_name) + 1);
    sprintf(fname, "%s/%s", r->file, file[i]->d_name);

    /* add the image */
    add_text(&page, &len,
             "<tr><td><img src=\"/" IMAGE_PATH "%s\" alt=\"%s\"></td> ",
             (file[i]->d_type == DT_DIR) ? "folder.png" : type_image(fname),
             (file[i]->d_type == DT_DIR) ? "DIR" : general_type(fname));

    /* the file name; append a / if it's a dir so we don't cause a redirect */
    add_text(&page, &len, "<td><a href=\"%s%s%s\">%s</a></td> ",
             r->reqfile, file[i]->d_name,
             (file[i]->d_type == DT_DIR) ? "/" : "", file[i]->d_name);

    /* and the size */
    add_text(&page, &len, "<td align=\"right\">%s</td></tr>\n",
             file_size(fname));
    free(file[i]);
  }

  /* terminate the page */
  add_text(&page, &len, "</table></body></html>\n");

  free(fname);

  /* send the page */
  free(r->content_type);
  r->content_type = strdup("text/html");
  
  r->encoding = IDENTITY;
  r->content_length = len;

  send_headers(r);
  send_str(r->fd, "\r\n");
  if(r->meth != HEAD) send(r->fd, page, len, 0);

  free(file);
  free(page);
}

/* Generates and sends an error document */
void send_errorpage(request *r) {
  char *page = NULL;
  size_t len = 0;
  int fildes;
  char file[decimal_length(int) + 5 + 1];/* an int, .html, \0 */

  /* Look for a ready-made error document */
  snprintf(file, 9, "%d.html", r->status);
  if((fildes = open(file, O_RDONLY)) != -1) {/* file exists and is readable */
    close(fildes);
    /* set up the request structure */
    free(r->file);
    r->file = strdup(file);
    file_stuff(r);
    send_file(r);
    return;
  }

  /* Generate page */
  add_text(&page, &len, "<html><head><title>%d %s</title></head>\n", r->status,
           status_reason[r->status]);
  add_text(&page, &len, "<body><h1>%d %s</h1>\n", r->status,
           status_reason[r->status]);

  if(page_text[r->status]) 
    add_text(&page, &len, page_text[r->status]);
  else
    add_text(&page, &len, "See "
             "<a href=\"http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html\">"
             "this</a> for more information.");

  add_text(&page, &len, "\n</body></html>\n");

  /* And now send the page */
  free(r->content_type);
  r->content_type = strdup("text/html");

  r->encoding = IDENTITY;
  r->content_length = len;

  send_headers(r);
  send_str(r->fd, "\r\n");
  if(r->meth != HEAD) send(r->fd, page, len, 0);

  free(page);
}

/* sends a HTTP/1.0 500 page to fd, without requiring a request structure */
void send_emergency_500(int fd, const char *file, int line,
                        const char *reason) {
  request *r;

  r = calloc(1, sizeof(request));
  
  /* fill in an absolutely minimal request structure */
  r->fd = fd;
  r->http = strdup("HTTP/1.0");
  r->status = 500;
  r->meth = GET;
  r->close_conn = 1;

  /* and send the error */
  send_errorpage(r);

  log_text(err, "Sending 500 %s at %s:%d (%s)", status_reason[500],
           file, line, reason);

  free_request(r);
}
