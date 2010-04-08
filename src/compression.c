/* Compression stuff for serve

   By James Stanley */

#include "serve.h"

char *encoding_name[] = { "identity", "gzip" };

/* decides what encoding to use (gzip or plain) based on the content of the
   accept-encoding header. Will either return IDENTITY or GZIP. Gzip will
   always be favoured over identity if it is acceptable */
int get_encoding(const char *accept_encoding) {
#ifdef USE_GZIP
  const char *p = accept_encoding;
  const char *s;
  char *coding = NULL;
  char *qvalue;
  float q;
  
  while(*p) {
    /* skip comma */
    if(*p == ',') p++;

    /* skip whitespace */
    for(; *p && iswhite(*p); p++);
    if(!*p) goto identity;

    /* if it's not a real name, we have been supplied garbage or the parser
       has failed; just send it un-compressed */
    if(!isalnum(*p) && *p != '*' && *p != '-') goto identity;

    /* find end of coding name */
    for(s = p; *s && (isalnum(*s) || *s == '*' || *s == '-'); s++);

    /* get coding name */
    coding = strdup2(p, s - p);

    /* skip whitespace */
    for(p = s; *p && iswhite(*p); p++);

    if(*p == ';') {/* qvalue here */
      /* skip whitespace */
      for(p++; *p && iswhite(*p); p++);
      if(!*p) goto identity;

      /* pretend this is a 'q' */
      p++;

      /* skip whitespace */
      for(; *p && iswhite(*p); p++);
      if(!*p) goto identity;
      
      /* pretend this is an '=' */
      p++;

      /* skip whitespace */
      for(; *p && iswhite(*p); p++);
      if(!*p) goto identity;

      /* now read the q-value */
      for(s = p; *s && (isdigit(*s) || *s == '.' || *s == '-'); s++);

      /* get q value */
      qvalue = strdup2(p, s - p);
      q = atof(qvalue);
      free(qvalue);

      /* don't check for encoding type if this encoding is unacceptable */
      if(q <= 0.0) {
        free(coding);
        continue;
      }
    }

    /* if we're here, then this coding is acceptable */
    if(strcasecmp(coding, "gzip") == 0) {
      free(coding);
      return GZIP;
    }

    free(coding);
    coding = NULL;
  }

  /* this is goto'd whenever we reach the end of the string */
 identity:
  free(coding);
#endif

  /* we haven't found gzip acceptable, just go with identity */
  return IDENTITY;
}

/* sends a deflated copy of the given data if the encoding is GZIP, otherwise
   it sends it plain; you can give a list of headers, the number of them,
   and an array saying which have been sent if you want extra headers to be
   sent; See cgi.c. Set mmapable to non-zero if sendfile() can be used to send
   data from the given file descriptor to a socket.
   Set len as -1 if you don't know it*/
void send_gzipped(request *r, int fd, int mmapable, long long len,
                  header *header_list, int num_headers, char *sent) {
  char buf[GZIP_BUF_SIZE];
  int read_data = 0;
  int fildes = -1;
  char file[] = "/tmp/gzip.XXXXXX";
  int made_tmp = 0;
  int n;
  int len_data = 0;
#ifdef USE_GZIP
  gzFile gzstrm = NULL;
#endif

#ifndef USE_GZIP
  r->encoding = IDENTITY;
#endif

  /* if we know the length and it's too small (or we can't compress), send it
     un-compressed straight from the file descriptor we've been given */
  if(len >= 0 && (len < GZIP_BUF_SIZE || r->encoding == IDENTITY)) {
    r->encoding = IDENTITY;
    r->content_length = len;
    fildes = fd;

    goto send_data;
  }

  /* we don't know the length and we're not allowed to gzip */
  if(len < 0 && r->encoding == IDENTITY) {
    fildes = mkstemp(file);
    made_tmp = 1;

    /* couldn't make temporary file, send error page */
    if(fildes == -1) {
      r->status = 500;
      
      log_text(err, "Couldn't create temporary file.");
      send_errorpage(r);

      return;
    }

    /* now buffer it */
    do {
      while((len = read(fd, buf, GZIP_BUF_SIZE)) > 0)
        write(fildes, buf, len);
    } while(len == -1 && errno == EINTR);
    
    goto send_fildes;
  }

  /* don't know the length yet, read the first chunk */
  while(len_data < GZIP_BUF_SIZE) {
    /* read some */
    do {
      n = read(fd, buf + len_data, GZIP_BUF_SIZE - len_data);
    } while(n == -1 && errno == EINTR);

    /* eof or error */
    if(n <= 0) break;

    /* add this many to the length */
    len_data += n;
  }
  read_data = 1;

  log_text(out, "n = %d\n", n);

  /* if all data fits in the first chunk, send it un-compressed;
     also handles the case where n is 0 (i.e. no data) */
  if(n < GZIP_BUF_SIZE) {
    r->encoding = IDENTITY;
    r->content_length = n;
    fildes = fd;

    goto send_data;
  }

  /* error reading data, send error page */
  if(n == -1) {
    r->status = 500;

    log_text(err, "Error reading first chunk (%s).", strerror(errno));
    send_errorpage(r);

    return;
  }

#ifdef USE_GZIP
  /* still here? we don't know the length and can gzip; get gzipping */
  fildes = mkstemp(file);
  made_tmp = 1;

  /* couldn't make temporary file, send error page */
  if(fildes == -1) {
    r->status = 500;
    
    log_text(err, "Couldn't create temporary file for gzip output.");
    send_errorpage(r);

    return;
  }

  /* open the gz stream from the file descriptor of the temporary file */
  gzstrm = gzdopen(fildes, "wb");

  /* write the chunk we've already read to this file */
  gzwrite(gzstrm, buf, n);

  /* now send the entire input stream through the gz stream */
  do {
    while((len = read(fd, buf, GZIP_BUF_SIZE)) > 0)
      gzwrite(gzstrm, buf, len);
  } while(len == -1 && errno == EINTR);

  /* flush the gz data */
  gzflush(gzstrm, Z_FINISH);
#endif

 send_fildes:

  /* ensure that fildes is set to something plausible  */
  serve_assert(fildes != -1);

  /* get the size (or send error page) */
  if((len = lseek(fildes, 0, SEEK_END)) == -1) {
    r->status = 500;

    if(fildes != fd)
      log_text(err, "Couldn't seek to end of temporary file %s (len = %lld).",
               file);
    else
      log_text(err, "Couldn't seek to end of file descriptor %d (len = %lld).",
               fd, len);

    send_errorpage(r);
    
#ifdef USE_GZIP
    if(gzstrm) gzclose(gzstrm);
    else
#endif
      close(fildes);

    if(made_tmp) unlink(file);
    return;
  }
  r->content_length = len;

  /* and go back to the start of the data */
  lseek(fildes, 0, SEEK_SET);

 send_data:

  /* now start sending stuff */
  send_headers(r);

  /* HACK: Let CGI scripts send extra headers */
  if(header_list) {
    send_new_headers(header_list, num_headers, sent, r->fd);
  }

  send_str(r->fd, "\r\n");

  /* if we've already read some data, write that first */
  if(read_data && fildes == fd) {
    send(r->fd, buf, n, 0);
  }

  /* only send data if it wasn't a HEAD request */
  if(r->meth != HEAD) {
#ifdef USE_SENDFILE
    /* If we're allowed sendfile and it's possible, try */
    if(!mmapable ||
       sendfile(fildes, r->fd, NULL, r->content_length) == -1) {
#endif
      /* Either not allowed sendfile or it's not possible */
      do {
        while((len = read(fildes, buf, GZIP_BUF_SIZE)) > 0) {
          log_text(out, "Read %d bytes.", len);
          if(send(r->fd, buf, len, 0) == -1) {
            /* some error sending */
            log_text(err, "send: %s", strerror(errno));
            r->close_conn = 1;
            goto cleanup;
          }
        }
      } while(len == -1 && errno == EINTR);

      /* some other error */
      if(len == -1) {
        log_text(err, "Failed to read data from temporary file: %s",
                 strerror(errno));
        r->close_conn = 1;
      }
#ifdef USE_SENDFILE
    }
#endif
  }

 cleanup:

#ifdef USE_GZIP
  if(gzstrm) gzclose(gzstrm);
  else
#endif
    close(fildes);

  if(made_tmp) unlink(file);
}
