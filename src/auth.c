/* HTTP Authentication stuff for serve

   By James Stanley

   Public domain */

#include "serve.h"

static char *b64alphabet =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* converts a 32-character hex string to 16-byte plain data 
   returns a pointer to a static buffer 
   Returns NULL if there is an invalid hex character, or the string is shorter
   than 32 bytes */
char *str2bin(const char *s) {
  static char data[16];
  int c, i;
  char *p;

  for(p = data, i = 0; s[i] && s[i+1] && (i < 32); i += 2, p++) {
    if((c = hex_to_digit(s[i])) == -1) return NULL;
    *p = c << 4;
    if((c = hex_to_digit(s[i+1])) == -1) return NULL;
    *p |= c;
  }

  if(i != 32) return NULL;

  return data;
}

/* returns 1 if the given request is authenticated correctly or authentication
   is not required, and 0 if the client is required to authenticate */
int authenticated(request *r) {
  int fd;
  char *fname;
  char *ptr;
  char *line;
  char *end;
  char *user = NULL, *secret_password = NULL;
  int lenpassword;
  char md5data[16];
  char *realmd5;
  int i;
  MD5_CTX ctx;

  /* no auth required if they're not getting a page */
  if(r->status != 200) return 1;

  /* check file in directory */
  if(r->is_dir) {
    fname = malloc(strlen(r->file) + 7);
    sprintf(fname, "%s/.auth", r->file);
  } else {/* get the directory and then check there */
    ptr = strrchr(r->file, '/');
    if(!ptr) fname = strdup(".auth");
    else {
      fname = malloc(ptr - r->file + 7);
      strncpy(fname, r->file, ptr - r->file);
      strcpy(fname + (ptr - r->file), "/.auth");
    }
  }

  /* now read the .auth file */
  if((fd = open(fname, O_RDONLY)) == -1) {/* can't read it, no auth required */
    free(fname);
    return 1;
  }

  /* now see what auth data the client sent */
  for(i = 0; i < r->num_headers; i++) {
    if(strcasecmp(r->header_list[i].name, "Authorization") == 0) {
      get_authdata(r->header_list[i].value, &user, &secret_password);
      /* we don't want passwords being swapped to disk! */
      lenpassword = strlen(secret_password);
#ifdef USE_MLOCK
      mlock(secret_password, lenpassword);
#endif
      break;
    }
  }

  /* get authentication realm */
  if(r->auth_realm) free(r->auth_realm);
  r->auth_realm = stripendl(nextline(fd));

  /* now get md5 hash */
  if(user && secret_password) {
    MD5_Init(&ctx);
    MD5_Update(&ctx, secret_password, lenpassword);
    MD5_Final((unsigned char*)md5data, &ctx);
	  
    /* and now read users and hashes */
    while((ptr = line = stripendl(nextline(fd)))) {
      while(iswhite(*ptr)) ptr++;/* strip leading blanks */
      if(*ptr && *ptr != '#') {/* skip comments and blanks */
        for(end = ptr; *end && !iswhite(*end); end++);/* find end of username */
        if(strncmp(ptr, user, end - ptr) == 0) {/* correct user */
          ptr = end;
          while(iswhite(*ptr)) ptr++;/* skip blanks */
          realmd5 = str2bin(ptr);
          if(realmd5) {/* valid hex string */
            if(memcmp(realmd5, md5data, 16) == 0) {/* correct password */
              close(fd);
              free(fname);
              r->auth_user = user;
              /* don't store passwords... */
              memset(secret_password, '\0', lenpassword);
              free(secret_password);
#ifdef USE_MLOCK
              munlock(secret_password, lenpassword);
#endif
              return 1;
            }
          } else {
            log_text(err, "Invalid md5 hash for %s in %s", user, fname);
          }
        }
      }
      free(line);
    }
  }
	
  /* we still here? thou shalt not pass! */
	
  close(fd);
  free(fname);
  free(user);
  if(secret_password) {
    /* don't store passwords in memory (even incorrect ones!) */
    memset(secret_password, '\0', lenpassword);
    free(secret_password);
#ifdef USE_MLOCK
    munlock(secret_password, lenpassword);
#endif
  }

  /* no access for you, mon amigo */
  return 0;
}

/* returns -1 if authentication data can't be got, and leaves user and pass
   untouched */
int get_authdata(const char *authdata, char **user, char **pass) {
  char *ptr = (char*)authdata;
  char *data;

  while(iswhite(*ptr)) ptr++;
  if(strncasecmp(ptr, "Basic", 5) != 0) return -1;/* incompatible auth */

  ptr += 5;/* jump past Basic */
  while(iswhite(*ptr)) ptr++;

  /* base64 decode the auth data */
  data = malloc((strlen(ptr) * 3) / 4 + 1);/* base64 takes up four thirds as
                                              much as plain */

  if(b64_decode(data, ptr) == -1) return -1;

  /* now split in to username and password */
  ptr = strchr(data, ':');
  if(!ptr) return -1;

  *user = strdup2(data, ptr - data);
  *pass = strdup(ptr + 1);

  free(data);
  return 0;
}

/* base 64 decodes src (must be NUL-terminated) in to dest, dest must be big
   enough. base 64 encoded data takes up four thirds as much memory as plain
   data.
   dest is NUL-terminated after b64_decode is done, so make room for that.
   If -1 is returned then the content of dest is undefined, and it means a
   character in src wasn't a valid base64 character */
int b64_decode(char *dest, const char *src) {
  char *seg;
  char *p;
  char *out = dest;
  char v[4];
  int i;

  /* go through 4 at a time */
  for(seg = (char*)src; *seg; seg += 4) {
    /* get the numeric values */
    for(i = 0; i < 4; i++) {
      p = strchr(b64alphabet, seg[i]);
      if(!p) {/* not in alphabet */
        if(seg[i] == '=') v[i] = 0;/* padding */
        else return -1;/* invalid */
      } else v[i] = p - b64alphabet;/* normal character */
    }

    /* and now decode it and stick it on the output */
    *(out++) = (v[0] << 2) | (v[1] >> 4);
    if(seg[2] != '=') *(out++) = ((v[1] & 0x0f) << 4) | (v[2] >> 2);
    if(seg[3] != '=') *(out++) = ((v[2] & 0x0f) << 6) | v[3];
  }

  *out = '\0';

  return 0;
}
