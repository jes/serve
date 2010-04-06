/* bin2c - Converts an arbitrary file to a C array and length

   Originally for conversion of images for serve

   By James Stanley */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* turns name in to a 'nice' c variable name, returns a pointer to name */
char *c_name(char *name) {
  char *n, *p;

  /* stdin becomes "s"; can't guarantee enough space for "stdin" */
  if(name[0] == '-' && name[1] == '\0') {
    name[0] = 's';
    return name;
  }

  for(p = name, n = name; *n; n++) {
    if(islower(*n) || (n > name && isdigit(*n)))
      *(p++) = *n;
    else if(p == name || *(p-1) != '_')
      *(p++) = '_';
  }

  *p = '\0';

  return name;
}

int main(int argc, char **argv) {
  int i;
  FILE *fp;
  int c;
  int width;
  int length;/* realistically, nobody is going to be making >2GB files in to
                C code! */
  int last_hex;

  if(argc == 1) {
    fprintf(stderr, "USAGE: %s {files}\nReads from all the file names and "
            "produces C code corresponding to the content and length of these "
            "files. If '-' is specified, stdin is used.\nWrites to stdout.\n"
            "If two input files would produce the same C variable name, this "
            "is ignored. You must rename the variables yourself.\n",
            argv[0]);
    return 1;
  }

  /* now convert each file */
  for(i = 1; i < argc; i++) {
    if(argv[i][0] == '-' && argv[i][1] == '\0') fp = stdin;
    else {/* read the file */
      fp = fopen(argv[i], "rb");
      if(!fp) {
        fprintf(stderr, "Unable to open %s for reading.\n", argv[i]);
        continue;
      }
    }

    length = 0;

    /* convert to C variable name */
    printf("unsigned char *%s = (unsigned char*)\n", c_name(argv[i]));
    printf("  \"");
    width = 3;
    last_hex = 0;

    /* print printable characters and convert others to hex */
    while((c = fgetc(fp)) != EOF) {
      length++;

      if(c == '\\' || c == '\'' || c == '\"') {/* escape the character */
        if(width > 77) {
          printf("\"\n  \"");
          width = 3;
        }
        printf("\\%c", c);
        width += 2;
        last_hex = 0;
      } else if(isprint(c) && !(last_hex && isxdigit(c))) {
        /* just text if it won't confuse the hex */
        if(width > 78) {/* need room for one character plus speech mark */
          printf("\"\n  \"");
          width = 3;
        }
        fputc(c, stdout);
        width++;
        last_hex = 0;
      } else {/* use hex */
        if(width > 75) {/* need room for 4 characters plus speech mark */
          printf("\"\n  \"");
          width = 3;
        }
        printf("\\x%02x", c);
        width += 4;
        last_hex = 1;
      }
    }

    printf("\";\n\n");

    printf("int %s_length = %d;\n\n", argv[i], length);
  }

  return 0;
}
