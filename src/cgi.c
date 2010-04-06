/* CGI script stuff for serve

	 By James Stanley

	 Public domain */

#include "serve.h"

/* adds the given name and value to the given environment array
   num must be 1 if there are no items in the array, as the final item must be
   NULL. */
void add_env(char ***env, int num, const char *name, const char *value) {
  int lenname, lenvalue;

  lenname = strlen(name);
  lenvalue = strlen(value);

  /* only resize env if necessary */
  if(num >= INIT_ENV_LENGTH)
    *env = realloc(*env, (num + 1) * sizeof(char*));

	(*env)[num-1] = malloc(lenname + 1 + lenvalue + 1);

  /* name=value */
  strcpy((*env)[num-1], name);
  (*env)[num-1][lenname] = '=';
  strcpy((*env)[num-1] + lenname + 1, value);
  (*env)[num-1][lenname + 1 + lenvalue] = '\0';

	(*env)[num] = NULL;
}

/* free's everything in the given environment array */
void free_env(char **env) {
	int i;

	for(i = 0; env[i]; i++) free(env[i]);
	free(env);
}

/* sets up the environment for the given request */
void setup_env(char ***env, request *r) {
	int num = 1;
	char *scriptname;
	char *pathname;
	char *ptr;
	int i, len = 0, len2 = 0;
	char *header = NULL;
  
  /* make add_env() slightly faster by not needing to realloc every time;
     I profiled this with callgrind and add_env is the second worst performance
     bottleneck when running AjaxTerm */
  *env = malloc(INIT_ENV_LENGTH * sizeof(char*));

	add_env(env, num++, "SERVER_SOFTWARE", SERVER);
	add_env(env, num++, "GATEWAY_INTERFACE", "CGI/1.1");
	add_env(env, num++, "DOCUMENT_ROOT", r->doc_root);
	add_env(env, num++, "SERVER_PORT", port);
	add_env(env, num++, "SERVER_PROTOCOL", r->http);
	add_env(env, num++, "REMOTE_ADDR", r->client);
	add_env(env, num++, "REQUEST_URI", r->reqfile);
	add_env(env, num++, "REQUEST_METHOD", method[r->meth]);

	if(r->auth_realm) {
		add_env(env, num++, "AUTH_TYPE", "Basic");
		add_env(env, num++, "REMOTE_USER", r->auth_user);
	}

	/* get the pathname for SCRIPT_FILENAME */
	if(r->file[0] != '/') {/* make a path name for relative directories */
		len = strlen(r->doc_root);
		len2 = strlen(r->file);
		pathname = malloc(len + 1 + len2 + 1);
		strcpy(pathname, r->doc_root);
		if(pathname[len - 1] != '/') {/* add a slash */
			pathname[len] = '/';
			strcpy(pathname + len + 1, r->file);
		} else {/* no slash necessary */
			strcpy(pathname + len, r->file);
		}
		add_env(env, num++, "SCRIPT_FILENAME", pathname);
		free(pathname);
		len = 0;
		len2 = 0;
	} else {
		add_env(env, num++, "SCRIPT_FILENAME", r->file);
	}

	ptr = strchr(r->reqfile, '?');
	add_env(env, num++, "QUERY_STRING", ptr ? ptr+1 : "");
	if(ptr)	scriptname = strdup2(r->reqfile, ptr - r->reqfile);
	else scriptname = strdup(r->reqfile);
	add_env(env, num++, "SCRIPT_NAME", scriptname);
	free(scriptname);

	/* now the HTTP headers */
	for(i = 0; i < r->num_headers; i++) {
		/* make it long enough */
		len2 = 6 + strlen(r->header_list[i].name);
		if(len2 > len) {
			len = len2;
			header = realloc(header, len);
			strcpy(header, "HTTP_");
		}
		strcpy(header + 5, r->header_list[i].name);
		/* fix case and hyphenation */
		for(ptr = header + 5; *ptr; ptr++) {
			*ptr = toupper(*ptr);
			if(*ptr == '-') *ptr = '_';
		}
		add_env(env, num++, header, r->header_list[i].value);
		/* and check some special ones */
		if(strcmp(header, "HTTP_CONTENT_LENGTH") == 0) {
			add_env(env, num++, "CONTENT_LENGTH", r->header_list[i].value);
		} else if(strcmp(header, "HTTP_CONTENT_TYPE") == 0) {
			add_env(env, num++, "CONTENT_TYPE", r->header_list[i].value);
		} else if(strcmp(header, "HTTP_HOST") == 0) {
			add_env(env, num++, "SERVER_NAME", r->header_list[i].value);
		}
	}
	free(header);
}

/* Runs the CGI script with the given handler */
void run_cgi(request *r) {
	char **env = NULL;
	char buf[1024];
	char *line = NULL;
	char *ptr;
	unsigned long len = 0;
	int n;
	char *handler = r->content_type;
	header *header_list = NULL;
	int num_headers = 0;
	char *sent = NULL;
	int fildes[2];/* 0 is for parent to read from, 1 is for child to write to */
  long long clength = -1;

	/* make a socket to send the post data and cgi output down */
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, fildes) < 0) {
		log_text(err, "Unable to create a socket pair!");
    r->status = 500;
    send_errorpage(r);
		return;
	}

	/* set up the environment */
	setup_env(&env, r);

	/* now fork a handler and give it the file */
	if((n = fork()) < 0) {
		log_text(err, "Unable to fork!");
    r->status = 500;
    send_errorpage(r);
		free_env(env);
		return;
	}

	/* replace child process with script */
	if(!n) exec_script(fildes, env, r);

	/* don't want child's end or the environment */
  free_env(env);
	close(fildes[1]);

	/* write POST data down socket */
	if(r->post_data) write_post_data(fildes[0], r);

	/* sort out headers that don't apply to CGI scripts */
	r->content_length = 0;
	free(r->last_modified);
	r->last_modified = NULL;

	/* read headers until there's a blank line */
	while((n = next_header(fildes[0], &line, &header_list, num_headers++,
												 (size_t *)&clength)) > 0);

	/* sort out header errors */
	if(n == -1) {/* bad headers; non-parsed header script? */
		if(strncmp(line, "HTTP/", 4) != 0) {
			log_text(err, "%s printed a bad header.", r->file);
		}
		send_str(r->fd, line);
		send_str(r->fd, "\r\n");
		free(line);
	} else if(n == -2) {/* premature close of file descriptor */
		log_text(err, "Premature exit by %s while collecting header %d.",
						 handler[1] ? handler : r->file, num_headers);
    r->status = 500;
    send_errorpage(r);
    free_headers(header_list, num_headers-1);
    close(fildes[0]);
    return;
	}

	/* next_header didn't add a header if it returned a value < 0 */
	if(n < 0) num_headers--;

	if(n == -1) {/* this is a non-parsed-header script */
    do {
      while((len = read(fildes[0], buf, 1024)) > 0)
        send(r->fd, buf, len, 0);
    } while(len == -1 && errno == EINTR);
    r->close_conn = 1;/* NPH scripts are more reliable if the connection ends */
	} else {
		/* fill in the headers the script gave us */
		sent = malloc(num_headers);

    /* fill_headers returns the content of the Location header */
		if((ptr = fill_headers(r, header_list, num_headers, sent))) {
			/* Location header was sent, send a redirect to the client */
      r->status = 302;
      r->location = ptr;
      send_errorpage(r);
		} else {/* no location header, send script output */
      r->content_length = 0;

      send_gzipped(r, fildes[0], NOT_MMAPABLE, clength, header_list,
                   num_headers, sent);
		}
	}

	free_headers(header_list, num_headers);
	free(sent);
}

/* writes the post data for the given request to the given file descriptor */
void write_post_data(int fd, request *r) {
	if(!r->post_data) return;

  if(write(fd, r->post_data, r->post_length) < 0) {
    log_text(err, "Unable to write post data to CGI script: %s",
             strerror(errno));
  }
}

/* replaces the current process image with that of the script for the given
	 request. */
void exec_script(int *fildes, char * const *env, request *r) {
	char *path;
	char *ptr;
	char *tmp;
	char *handler = r->content_type;

	close(fildes[0]);/* don't want parents end */
	close(r->fd);/* don't give the cgi script our network connection */

	dup2(fildes[1], STDIN_FILENO);
	dup2(fildes[1], STDOUT_FILENO);

	/* now go in to the script's directory */
  tmp = strdup(r->file);
  path = tmp;

  /* strip everything after the right-most slash */
  if((ptr = strrchr(path, '/'))) {
    *ptr = '\0';
    chdir(path);

    /* now get the filename */
    path = ptr + 1;
  }
  
	if(handler[1] == '\0') {/* run script */
		execle(path, path, NULL, env);
		log_text(err, "running script %s with no handler, execle: %s", r->file,
						 strerror(errno));
	} else {/* run handler on script */
		execle(handler, handler, path, NULL, env);
		log_text(err, "running handler %s, execle: %s", handler,
						 strerror(errno));
	}

  free(tmp);
	close(fildes[1]);
	exit(1);
}

/* sends headers down fd where the corresponding byte in sent is 0.
	 There are never enough headers to warrant a bit array for sent */
void send_new_headers(header *header_list, int num_headers, char *sent,
                      int fd) {
	int i;

	for(i = 0; i < num_headers; i++) {
		/* go through headers not sent already */
		if(!sent[i]) {
			send_str(fd, header_list[i].name);
			send_str(fd, ": ");
			send_str(fd, header_list[i].value);
			send_str(fd, "\r\n");
      sent[i] = 1;
		}
	}
}

/* Fills in request structure headers from the header list
	 A byte in done is set to 0 if that header does not go in a request structure
	 field.
   Returns the content of the Location header, or NULL if there was none */
char *fill_headers(request *r, header *header_list, int num_headers,
									char *done) {
	int i;
	char *s = NULL;

	memset(done, 1, num_headers);

	for(i = 0; i < num_headers; i++) {
		if(strcasecmp(header_list[i].name, "Content-Type") == 0) {
			free(r->content_type);
			r->content_type = strdup(header_list[i].value);
		} else if(strcasecmp(header_list[i].name, "Content-Length") == 0) {
			r->content_length = strtoull(header_list[i].value, NULL, 10);
		} else if(strcasecmp(header_list[i].name, "Date") == 0) {
			free(r->date);
			r->date = strdup(header_list[i].value);
		} else if(strcasecmp(header_list[i].name, "Last-Modified") == 0) {
			free(r->last_modified);
			r->last_modified = strdup(header_list[i].value);
		} else if(strcasecmp(header_list[i].name, "Status") == 0) {
			r->status = atoi(header_list[i].value);
		} else if(strcasecmp(header_list[i].name, "Location") == 0) {
      free(s);/* get rid of the old one */
			s = strdup(header_list[i].value);
		} else if((strcasecmp(header_list[i].name, "Server") != 0) &&
							(strcasecmp(header_list[i].name, "Connection") != 0)) {
			/* send what the script says instead of letting send_headers do it */
			done[i] = 0;
		}
	}

	return s;
}
