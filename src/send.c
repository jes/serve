/* Sends stuff to the client for serve

   By James Stanley

   Public domain */

#include "serve.h"

/* This function closes the process if the client disconnects.
   Yeah, I know. */
ssize_t send_str(int fd, const char *str) {
  int n;

  if((n = send(fd, str, strlen(str), 0)) == -1) {
    if(errno == EPIPE) exit(1);/* client disconnected */
  }

  return n;
}

/* Sends a response code to the client */
void send_response(request *r) {
  char *buf;
  int len;

  /* enough space for the http stuff and a space, plus 3 digits, plus a space,
     plus the text, plus the endline, plus the NUL */
  buf = malloc(strlen(r->http) + 1 + 3 + 1 +
               strlen(status_reason[r->status]) + 2 + 1);
  len = sprintf(buf, "%s %d %s\r\n", r->http, r->status,
                status_reason[r->status]);

  send(r->fd, buf, len, 0);

  free(buf);
}

/* Sends the headers for the given request to the client. */
void send_headers(request *r) {
  char clength[decimal_length(unsigned long long) + 3];

  send_response(r);

  send_str(r->fd, "Server: " SERVER "\r\n");

  send_str(r->fd, "Connection: ");
  send_str(r->fd, r->close_conn ? "close" : "keep-alive");
  send_str(r->fd, "\r\n");

  if(r->date) {
    send_str(r->fd, "Date: ");
    send_str(r->fd, r->date);
    send_str(r->fd, "\r\n");
  }

  if(r->location) {/* it's a redirect */
    send_str(r->fd, "Location: ");
    if(r->location[0] == '/') {/* absolute URI required */
      send_str(r->fd, "http://");
      send_str(r->fd, r->host);
    }
    send_str(r->fd, r->location);
    send_str(r->fd, "\r\n");
  } else {
    if(r->status == 401) {
      send_str(r->fd, "WWW-Authenticate: Basic realm=\"");
      send_str(r->fd, r->auth_realm ? r->auth_realm : "default");
      send_str(r->fd, "\"\r\n");
    }

    if(r->status == 200 && r->last_modified) {
      send_str(r->fd, "Last-Modified: ");
      send_str(r->fd, r->last_modified);
      send_str(r->fd, "\r\n");
    }
  }
	
  send_str(r->fd, "Content-Type: ");
  send_str(r->fd, r->content_type);
  send_str(r->fd, "\r\n");

  if(r->meth != HEAD || r->content_length != 0) {
    send_str(r->fd, "Content-Length: ");
    sprintf(clength, "%llu\r\n", r->content_length);
    send_str(r->fd, clength);
  }

  if(r->encoding != IDENTITY) {
    send_str(r->fd, "Content-Encoding: ");
    send_str(r->fd, encoding_name[r->encoding]);
    send_str(r->fd, "\r\n");
  }
}

/* sends the file with the given name to the socket; returns 0 on success and
   -1 on error; if USE_SENDFILE is defined, len must be the length to send,
   otherwise it is unused. Sending always starts at the start of the file */
int send_file_to_socket(const char *filename, int fd, size_t len) {
  char buf[1024];
  int fildes;

  if((fildes = open(filename, O_RDONLY)) == -1) return -1;

  /* Note the unterminated if statement in the preprocessor macro */
#ifdef USE_SENDFILE
  /* we are using sendfile */
  if(sendfile(fd, fildes, NULL, len) == -1)
#endif
    /* sendfile failed or isn't used, fall back to manual sending */
    do {
      while((len = read(fildes, buf, 1024)) > 0)
        send(fd, buf, len, 0);
    } while(len == -1 && errno == EINTR);

  close(fildes);

  return 0;
}

/* Sends the builtin file to the client */
void send_builtin(request *r) {
  r->encoding = IDENTITY;
  send_headers(r);
  send_str(r->fd, "\r\n");
  send(r->fd, r->img_data, r->content_length, 0);
}

/* Sends the file to the client */
void send_file(request *r) {
  int fd;

  /* find out if we must make a dir listing */
  if(r->is_dir) {
    send_dir(r);
    return;
  }

  /* send builtin file */
  if(r->img_data) {
    send_builtin(r);
    return;
  }

  if(r->content_type[0] == '/') {
    /* a path to a handler has been given, run the script */
    run_cgi(r);
    return;
  }

  /* page not modified */
  if(r->status == 200) {
    if(r->last_modified_t <= r->if_modified_since) {
      r->status = 304;
      r->meth = HEAD;
      send_errorpage(r);
      return;
    }
  }

  /* now send the file un-compressed if we're not using gzip, or if it's
     filename contains ".gz" */
  if(r->encoding != GZIP || strstr(r->file, ".gz")) {
    r->encoding = IDENTITY;

    send_headers(r);
    send_str(r->fd, "\r\n");
    
    send_file_to_socket(r->file, r->fd, r->content_length);

    return;
  }

  /* gzip and send it */
  fd = open(r->file, O_RDONLY);
  if(fd == -1) {
    r->status = 500;
    
    log_text(err, "Unable to open %s for reading.", r->file);
    send_errorpage(r);

    return;
  }
 
  send_gzipped(r, fd, MMAPABLE, r->content_length, NULL, 0, NULL);
}
