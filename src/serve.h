/* Header for serve */

#ifndef SERVE_H_INC
#define SERVE_H_INC

/* for strptime */
#define _XOPEN_SOURCE 600

/* for scandir */
#define _BSD_SOURCE
#define _NETBSD_SOURCE

#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdarg.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "md5.h"

#ifdef USE_LIBMAGIC
#include <magic.h>
#endif

#ifdef USE_SENDFILE
#include <sys/sendfile.h>
#endif

#ifdef USE_GZIP
#include <zlib.h>
#endif

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

/* HTTP Server header */
#ifdef VERSION
#define SERVER "serve/" VERSION
#else
#define SERVER "serve"
#endif

/* the built-in path images are in; feel free to change this */
#define IMAGE_PATH "serve_images/"

/* Maximum size of POST data before it is stored in a file instead of memory */
#define MAXPOSTSIZE 65536

/* Maximum amount of time in seconds to keep persitent connections alive for */
#define MAXKEEPALIVE 600

/* Minimum size file to send gzip'd, also the size allocated for gzip buffer */
#define GZIP_BUF_SIZE 16384

/* Initial length for environment arrays for CGI scripts */
#define INIT_ENV_LENGTH 64

/* maximum number of characters required to represent the largest value of
   the given _integer_ type in decimal. 3 * number of bytes, plus 1 for a minus
   sign */
#define decimal_length(type) (3 * sizeof(type) + 1)

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

extern char *port;
extern char *pidfile;
extern char *server_name;
extern char *listen_addr;

/* so that we can state Main process or Handler process when we are killed */
unsigned char is_handler;

#define DISK   0
#define MEMORY 1

typedef struct header_s header;

typedef struct request_s {
  int fd;
  char *user_agent;
  char *client;
  char *req;
  int meth;
  char *reqfile;
  char *file;
  char *http;
  char *content_type;
  unsigned long long content_length;
  char *last_modified;
  time_t last_modified_t;
  char *date;
  int status;
  int is_dir;
  header *header_list;
  int num_headers;
  char *post_file;
  char *post_data;
  size_t post_length;
  char *auth_realm;
  char *auth_user;
  char *doc_root;
  int keep_alive;
  int close_conn;
  char *location;
  time_t if_modified_since;
  char *host;
  unsigned char *img_data;
  int encoding;
} request;

char *strdup2(const char *s, size_t n);

/* init.c */
extern char *status_reason[600];
extern char *page_text[600];

int init_net(const char *service);
void init_sighandlers(void);
void init_status_reason(void);

/* handler.c */
#define INVALID -1
#define GET     0
#define HEAD    1
#define POST    2
#define OPTIONS 3
#define PUT     4
#define DELETE  5
#define TRACE   6
#define CONNECT 7
#define METHODS 8

#define TEXT 0
#define HEX1 1
#define HEX2 2

#define ENC_NORMAL  0
#define ENC_CHUNKED 1

extern char *method[METHODS];

void handle(int fd, const char *addr);
void record_post_data(request *r);
time_t get_date(const char *str);

/* nextline.c */
char *nextline(int fd);

/* send.c */
ssize_t send_str(int fd, const char *str);
void send_response(request *r);
void send_headers(request *r);
int send_file_to_socket(const char *filename, int fd, size_t len);
void send_file(request *r);

/* genpage.c */
/* void emergency_500(int fd, const char *reason); */

/* TODO: Support file, line, and reason in proper 500 pages */
#define emergency_500(f, r) send_emergency_500(f, __FILE__, __LINE__, r)

void add_text(char **page, size_t *pagelen, char *fmt, ...);
void send_dir(request *r);
int nonhidden(const struct dirent *d);
int dirsort(const struct dirent **a, const struct dirent **b);
char *file_size(const char *file);
void send_dirlist(request *r);
void send_errorpage(request *r);
void send_emergency_500(int fd, const char *file, int line, const char *reason);

/* mimetypes.c */
#define TYPE(i) mimetype[i*2]
#define EXT(i)  mimetype[i*2+1]

extern char **mimetype;
extern int mimetypes;
extern int longest_ext;

void load_mimetypes(void);
char *type_image(const char *filename);
char *general_type(const char *filename);
void load_mimetypes_from(const char *filename);
void add_mimetype(const char *type, const char *ext);
char *content_type(const char *file);
char *magic_content_type(const char *file);

/* log.c */
#define serve_assert(x)                                                 \
  do {                                                                  \
    if(!(x)) {                                                          \
      log_text(err, "Assertion '%s' failed, aborting %s process!",      \
               #x, is_handler ? "handler" : "main");                    \
      raise(SIGABRT);                                                   \
    }                                                                   \
  } while(0)                                                            \

extern FILE *out;
extern FILE *err;

void save_pid(int pid);
void log_text(FILE *file, const char *fmt, ...);
void log_request(request *r);

/* cgi.c */
void add_env(char ***env, int num, const char *name, const char *value);
void free_env(char **env);
void setup_env(char ***env, request *r);
void run_cgi(request *r);
void write_post_data(int fd, request *r);
void exec_script(int *fildes, char * const *env, request *r);
void send_new_headers(header *header_list, int num_headers, char *sent,
                      int fd);
char *fill_headers(request *r, header *header_list, int num_headers,
                   char *done);

/* request.c */
void free_request(request *r);
void fix_request(request *r);
void file_stuff(request *r);
request *request_info(int fd, const char *addr, const char *req);
int iswhite(char c);
int method_type(const char *buf);
char *requ_file(const char *buf);
int hex_to_digit(char c);
char *filename(const char *buf);
char *stripendl(char *buf);
char *http_version(const char *buf);
int forbidden(const char *file);

/* headers.c */
struct header_s {
  char *name;
  char *value;
};

void add_header(header **list, int num, char *header, char *value);
void free_headers(header *list, int num);
int next_header(int fd, char **lin, header **list, int num, size_t *length);

/* auth.c */
char *str2bin(const char *s);
int authenticated(request *r);
int get_authdata(const char *authdata, char **user, char **pass);
int b64_decode(char *dest, const char *src);

/* images.c */
/* NOTE: The images themselves are in images.h */

typedef struct image_s {
  char *name;
  char *ctype;
  unsigned char *data;
  int length;
} image;

void init_builtin_files(void);
void builtin_file_stuff(request *r);

/* compression.c */
#define IDENTITY 0
#define GZIP     1

#define NOT_MMAPABLE 0
#define MMAPABLE     1

extern char *encoding_name[];

int get_encoding(const char *accept_encoding);
void send_gzipped(request *r, int fd, int mmapable, long long len,
                  header *header_list, int num_headers, char *sent);

#endif
