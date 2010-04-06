/* Request structure stuff for serve

	 By James Stanley

	 Public domain */

#include "serve.h"

/* closes the connection and free's all the content */
void free_request(request *r) {
  free(r->user_agent);
	free(r->client);
	free(r->req);
	free(r->reqfile);
	free(r->file);
	free(r->http);
	free(r->content_type);
	free(r->last_modified);
	free(r->date);
	if(r->post_data) free(r->post_data);
	free(r->auth_realm);
	free(r->auth_user);
	free(r->doc_root);
	free(r->location);
	free(r->host);
	free_headers(r->header_list, r->num_headers);
	free(r);
}

/* fills in sensible defaults for required values */
void fix_request(request *r) {
	if(!r->http) r->http = strdup("HTTP/1.0");
	if(!r->file) r->file = strdup(".");
}

/* fills in file-based stuff in the request structure */
void file_stuff(request *r) {
	int fildes;
	int len;
	struct stat statbuf;
	struct tm *tm_time;

	/* no file */
	if(!r->file) {
		r->status = 400;
		return;
	}

	free(r->content_type);
	r->content_type = strdup(content_type(r->file));

	/* forbidden path */
	if(forbidden(r->file)) {
		r->status = 403;
		return;
	}

  /* built-in image? */
  /* skip one out because of the / at the start */
  if(strncmp(r->file, IMAGE_PATH, strlen(IMAGE_PATH)) == 0) {
    builtin_file_stuff(r);
    return;
  }

	/* check file exists */
	if(stat(r->file, &statbuf) != 0) {
		r->status = 404;
		return;
	}

	/* check if dir */
	if(statbuf.st_mode & S_IFDIR) {
		r->is_dir = 1;
		len = strlen(r->reqfile);
    /* redirect if a directory was requested without ending with a '/' */
		if(r->reqfile[len - 1] != '/') {
			r->status = 301;
			r->location = malloc(len + 2);
			strcpy(r->location, r->reqfile);
			strcpy(r->location + len, "/");
		}
	} else {
		r->is_dir = 0;
		/* not a directory, check if file can be read from */
		if((fildes = open(r->file, O_RDONLY)) < 0) {
			r->status = 403;
			return;
		}
		close(fildes);
	}

	/* WARNING: Only responses with status code 200 have content_length and
		 last_modified! */

	r->last_modified_t = statbuf.st_mtime;
	tm_time = gmtime(&statbuf.st_mtime);
	if(!r->last_modified) r->last_modified = malloc(64);
	strftime(r->last_modified, 64, "%a, %d %b %Y %T GMT", tm_time);

	r->content_length = statbuf.st_size;
}

/* creates a request structure with all the relevant information */
request *request_info(int fd, const char *addr, const char *req) {
	struct tm *tm_time;
	request *r;
	time_t tim;
	int len = PATH_MAX;

	r = calloc(1, sizeof(request));

	r->fd = fd;
	r->client = strdup((char*)addr);
	r->req = (char*)req;
	r->meth = method_type(req);
	r->reqfile = requ_file(req);
	r->file = filename(r->reqfile);
	r->http = http_version(req);
	r->status = 200;
	r->keep_alive = 300;

	/* get the document root */
	r->doc_root = malloc(len);
	while((getcwd(r->doc_root, len++)) == NULL)
		r->doc_root = realloc(r->doc_root, len);

	/* get date */
	time(&tim);
	tm_time = gmtime(&tim);
	r->date = malloc(64);
	strftime(r->date, 64, "%a, %d %b %Y %T GMT", tm_time);

	/* bad method */
	if(r->meth == INVALID) {
		r->status = 400;
		return r;
	}

	/* unsupported method? */
	if((r->meth != GET) &&
		 (r->meth != HEAD) &&
		 (r->meth != POST)) {
		r->status = 501;
		return r;
	}

	/* no http version */
	if(!r->http) {
    r->close_conn = 1;
		r->status = 400;
		return r;
	}

	/* unsupported major http version */
	if(strncmp(r->http, "HTTP/1.", 7) != 0)	{
		r->status = 505;
		return r;
	}

  /* check the file is proper */
  if(*r->reqfile != '/') {
    r->status = 400;
    return r;
  }

	/* close connection for HTTP 1.0 */
	if(strcmp(r->http, "HTTP/1.0") == 0) r->close_conn = 1;

	file_stuff(r);

	return r;
}

int iswhite(char c) {
	return (c == ' ') || (c == '\t');
}

/* Find the method type in buf
	 Returns a method number (see serve.h) or -1 if no/invalid method */
int method_type(const char *buf) {
	char *ptr, *end;
	int i;

	/* e.g. "GET /filename HTTP/1.1" */

	/* skip whitespace at start */
	for(ptr = (char*)buf; *ptr && iswhite(*ptr); ptr++);

	/* now find the end of the non-whitespace */
	for(end = ptr; *end && !iswhite(*end); end++);

	/* and now find the method number */
	for(i = 0; i < METHODS; i++) {
		if(strncasecmp(method[i], ptr, end-ptr) == 0) return i;
	}

	/* invalid method */
	return -1;
}

/* Extracts the filename from the request.
   new array. You must free the new array when you are done with it. */
char *requ_file(const char *buf) {
	char *ptr, *end;
	char *reqfile;

	/* eg. "GET /filename HTTP/1.1" */

	/* skip whitespace at start */
	for(ptr = (char*)buf; *ptr && iswhite(*ptr); ptr++);

	/* now find the end of the method */
	for(; *ptr && !iswhite(*ptr); ptr++);

	/* now skip more whitespace */
	for(; *ptr && iswhite(*ptr); ptr++);

	/* and find the end of the filename */
	for(end = ptr; *end && !iswhite(*end); end++);/* find space */

	/* allocate the path */
	reqfile = strdup2(ptr, end - ptr);

	return reqfile;
}

/* convents the given character to a number, 0 to 15
   returns -1 if it's not a valid hex digit */
int hex_to_digit(char c) {
	if((c >= '0') && (c <= '9')) return c - '0';
	else if((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
	else if((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
	else {/* not a valid hex digit */
		return -1;
	}
}

/* url decodes the contents of buf, and places the decoded version back in buf
	 Returns buf, or NULL on error */
char *url_decode(char *buf) {
	char *ptr = buf;
	int i;
	int c = 0, c2;
	int state = TEXT;

	for(i = 0; *ptr; ptr++) {
		switch(state) {
		case TEXT:
			if(*ptr == '%')	state = HEX1;
			else buf[i++] = *ptr;
			break;
		case HEX1:
			/* convert to a value and put in c */
			if((c = hex_to_digit(*ptr)) < 0) return NULL;
			state = HEX2;
			break;
		case HEX2:
			/* convert to a value, put in c2, and add the character */
			if((c2 = hex_to_digit(*ptr)) < 0) return NULL;
			buf[i++] = (c << 4) | c2;
			state = TEXT;
			break;
		}
	}

	return buf;
}

/* Takes the given requ_file()-returned name, sorts out user directories, url
	 decodes it, and places it in a new array. You must free the new array when
	 you are done with it.
   Returns NULL if the filename given can not be url decoded. */
char *filename(const char *buf) {
	char *ptr, *end;
	char *p, *s;
	char *path, *newpath;
	struct passwd *pw;
	char *user = NULL;
	int len, userdir = 0;

  /* no path given, get root */
  if(*buf == '\0') return strdup(".");

	/* get just the file part */
	ptr = (char*)buf;
	while(iswhite(*ptr)) ptr++;
	
	/* get a url decoded copy */
	if((end = strchr(ptr, '?'))) path = strdup2(ptr, end - ptr);
	else path = strdup(ptr);
	if(url_decode(path) == NULL) {
		free(path);
		return NULL;
	}
	ptr = path;

	/* sort out leading slashes */
	while(*ptr == '/') ptr++;

	/* sort out double slashes */
	for(p = ptr, s = ptr; *s; s++) {
		if(*s != '/') *p++ = *s;
		else if(*(s+1) != '/') *p++ = *s;
	}
	*p = '\0';
	len = strlen(ptr);

	/* sort out trailing /. */
	if(len >= 2)
		if((ptr[len - 1] == '.') && (ptr[len - 2] == '/')) len--;

	/* sort out trailing slashes */
	while((ptr[len - 1] == '/') && (len > 0)) len--;

	/* NUL-terminate */
	ptr[len] = '\0';

	/* sort out empty path */
	if(len == 0) {
		free(path);
		ptr = path = strdup(".");
	}

	/* and now get user directories */
	/* Valgrind complains about using getpwnam() because it allocates a buffer
		 that never gets freed. This isn't a problem because it is reused on
		 subsequent calls to getpwnam(). */
	if(*ptr == '~') {
		if((end = strchr(ptr, '/'))) {/* user directory at start of path */
			user = strdup2(ptr + 1, end - (ptr + 1));
			pw = getpwnam(user);
		} else {/* index */
			end = ptr + len;
			pw = getpwnam(ptr + 1);
		}
		if(pw) {/* user exists, construct path */
			userdir = 1;
			len = strlen(pw->pw_dir);
			newpath = malloc(len + /* strlen("/public_html") */ 12 +
											 strlen(end) + 1);
			strcpy(newpath, pw->pw_dir);
			if(newpath[len - 1] != '/') newpath[len++] = '/';
			strcpy(newpath + len, "public_html");
			strcpy(newpath + len + /* strlen("public_html") */ 11, end);
			free(path);
			free(user);
		}
	}

	/* sort out the path if it's not a userdir */
	if(!userdir) {
		newpath = strdup(ptr);
		free(path);
	}

	return newpath;
}

/* Removes \r and \n characters from the end of buf
	 More accurately, it replaces the first \r or \n with a \0
   Returns buf */
char *stripendl(char *buf) {
	char *ptr;

	if(!buf) return NULL;

	ptr = strchr(buf, '\r');
	if(ptr) *ptr = '\0';

	ptr = strchr(buf, '\n');
	if(ptr) *ptr = '\0';

	return buf;
}

/* Returns a pointer to the place in buf where the HTTP version starts or NULL
	 if none is found */
char *http_version(const char *buf) {
	char *ptr;
  char *p;

	/* eg. "GET /filename HTTP/1.1" */

	/* skip whitespace at start */
	for(ptr = (char*)buf; *ptr && iswhite(*ptr); ptr++);

	/* now find the end of the method */
	for(; *ptr && !iswhite(*ptr); ptr++);

	/* now skip more whitespace */
	for(; *ptr && iswhite(*ptr); ptr++);

	/* now find the end of the filename */
	for(; *ptr && !iswhite(*ptr); ptr++);

	/* and now skip over the last of the whitespace */
	for(; *ptr && iswhite(*ptr); ptr++);

	if(!*ptr) return NULL;

	/* copy the string */
  p = strdup(ptr);
  
  /* strip trailing whitespace */
  for(ptr = p; *ptr && !iswhite(*ptr); ptr++);
  *ptr = '\0';

  return p;
}

/* Returns 1 if the given file is forbidden and 0 otherwise */
int forbidden(const char *file) {
	char *ptr;
	int level = 0;

	if(!file) return 0;

	/* Deny files beginning with a ., except . the directory */
	if((*file == '.') && file[1] && file[1] != '/') return 1;
	ptr = strrchr(file, '/');
	if(ptr) {
		if(*(ptr+1) == '.') return 1;
	}

	/* And deny anything that tries to go above the current working directory */

	/* special case that isn't a /../ */
	if(strncmp(file, "../", 3) == 0) return 1;

	/* Look for the next /. If it is a /../, decrement the level, if it is not a
		 /./, increment the level. If we ever end up lower than where we started, it
		 is forbidden */
	ptr = (char*)file;
	while((ptr = strchr(ptr+1, '/'))) {
		if(strncmp(ptr, "/../", 4) == 0) level--;
		else if(strncmp(ptr, "/./", 3) != 0) level++;

		if(level < 0) return 1;
	}

	return 0;
}
