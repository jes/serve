/* nextline function for serve

   By James Stanley

   Public domain */

#include "serve.h"

/* Returns a pointer to a new array containing the next line of input read from
   the given file descriptor, or NULL on error or if at EOF upon function entry
   You should free the returned pointer yourself when you are done with it */
char *nextline(int fd) {
  char *line;
  size_t pos = 0;
  char input = 0;
  int n;
  size_t len = 128;/* reduce likelihood of expensive realloc */

  line = malloc(len);

  /* loop until we read an endline */
  while(input != '\n') {
    /* read one character */
    if((n = read(fd, &input, 1)) < 0) {
      if(errno == EINTR) continue;
      free(line);
      return NULL;
    } else if(n == 0) {/* EOF */
      if(pos == 0) {
        free(line);
        return NULL;
      }
      break;
    }

    /* extend the line and store the character */
    if(pos >= (len - 2)) {
      len *= 2;
      line = realloc(line, len);
    }
    if(!line) {
      log_text(err, "realloc returned NULL");
      return NULL;
    }
    line[pos++] = input;
  }

  line[pos] = '\0';

  return line;
}
