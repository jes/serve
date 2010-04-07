/* Connection handling functions for serve

   By James Stanley

   Public domain */

#include "serve.h"

char *method[METHODS] = {
  "GET", "HEAD", "POST", "OPTIONS", "PUT", "DELETE", "TRACE", "CONNECT"
};

/* Handles the connection from the given file descriptor */
void handle(int fd, const char *addr) {
  int n;
  request *r;
  char *req;

  /* in while loop because of persistent connections */
  while(1) {
    /* read the first line from the client */
    req = stripendl(nextline(fd));
    if(!req) {/* client disappeared */
      close(fd);
      exit(0);
    }

    /* get request info */
    r = request_info(fd, addr, req);
    fix_request(r);

    /* now record the headers */
    while((n = next_header(fd, NULL, &(r->header_list), r->num_headers++,
                           &(r->post_length))) > 0);
    if(n == -1) {
      r->num_headers--;
      r->status = 400;
    }	else if(n == -2) {
      log_text(err, "Premature disconnection by %s during header collection.",
               addr);
      free_request(r);
      exit(0);
    } else if(n == -3) r->num_headers = 0;

    /* Sort out headers */
    /* TODO: "Accept:" header */
    /* TODO: "Range:" header
       http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.35 */
    /* TODO: http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html */
    /* TODO: Put this stuff in the loop that the headers are recorded in so
       that we don't have to go over the whole array twice */
    for(n = 0; n < r->num_headers; n++) {
      if(strcasecmp(r->header_list[n].name, "Host") == 0) {
        r->host = strdup(r->header_list[n].value);
      } else if(strcasecmp(r->header_list[n].name, "Connection") == 0) {
        if(strcasecmp(r->header_list[n].value, "close") == 0) {
          r->close_conn = 1;
        }
      } else if(strcasecmp(r->header_list[n].name, "Keep-alive") == 0) {
        r->keep_alive = MIN(atoi(r->header_list[n].value), MAXKEEPALIVE);
      } else if(strcasecmp(r->header_list[n].name, "If-modified-since")
                == 0) {
        r->if_modified_since = get_date(r->header_list[n].value);
      } else if(strcasecmp(r->header_list[n].name, "Accept-encoding") == 0) {
        r->encoding = get_encoding(r->header_list[n].value);
      } else if(strcasecmp(r->header_list[n].name, "Content-encoding")
                == 0) {
        if(strcasecmp(r->header_list[n].value, "identity") != 0)
          r->status = 415;
      } else if(strcasecmp(r->header_list[n].name, "User-agent") == 0) {
        r->user_agent = strdup(r->header_list[n].value);
      }
    }

    if(!r->host) {/* HTTP/1.1 requires a host header */
      if(strcmp(r->http, "HTTP/1.0") == 0) r->host = strdup(server_name);
      else r->status = 400;
    }
    
    /* see about authenticating the client */
    if(!authenticated(r)) r->status = 401;

    /* record post data */
    if(r->meth == POST) {
      if(r->post_length > 0)
        record_post_data(r);
      else
        r->status = 400;
    }

    /* and now handle the request */
    if(r->status == 200) {
      send_file(r);
      log_request(r);
    } else {
      send_errorpage(r);
      log_request(r);
    }

    if(r->close_conn) break;

    /* kill the process if there isn't another request soon */
    alarm(r->keep_alive);

    free_request(r);
  }

  /* to prevent valgrind from complaining, we could free the mime types here and
     also close libmagic's cookie;
     we won't do this though, as that would cause copy-on-write of the mime
     types, which is a waste of time seeing as we're leaving now anyway */

  free_request(r);
  exit(0);
}

/* records post data for the given request, terminates the process if there is
   an error collecting the data */
void record_post_data(request *r) {
  int bytes;
  int bytesdone = 0;

  if(r->meth != POST) return;

  r->post_data = malloc(r->post_length);

  while(bytesdone < r->post_length) {
    bytes =	recv(r->fd, r->post_data + bytesdone, r->post_length - bytesdone,
                     0);
    if(bytes <= 0) {/* client has left */
      log_text(err, "Premature disconnection by %s during POST data "
               "collection.", r->client);
      free_request(r);
      exit(1);
    }
    bytesdone += bytes;
  }
}

/* returns a time_t that is the date described by str */
time_t get_date(const char *str) {
  struct tm tm_time;

  /* appease valgrind */
  memset(&tm_time, '\0', sizeof(struct tm));

  if(strptime(str, "%a, %d %b %Y %T %Z", &tm_time) == NULL)
    if(strptime(str, "%A, %d-%b-%y %T %Z", &tm_time) == NULL)
      if(strptime(str, "%a %b %T %Y", &tm_time) == NULL)
        return 0;

  return mktime(&tm_time);
}
