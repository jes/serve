/* Handling of serve's built-in images

   By James Stanley */

#include "images.h"
#include "serve.h"

image builtin[7];

/* initializes the builtin files */
void init_builtin_files(void) {
  builtin[0].name = IMAGE_PATH "folder.png";
  builtin[0].ctype = "image/png";
  builtin[0].data = folder_png;
  builtin[0].length = folder_png_length;

  builtin[1].name = IMAGE_PATH "image.png";
  builtin[1].ctype = "image/png";
  builtin[1].data = image_png;
  builtin[1].length = image_png_length;

  builtin[2].name = IMAGE_PATH "text.png";
  builtin[2].ctype = "image/png";
  builtin[2].data = text_png;
  builtin[2].length = text_png_length;

  builtin[3].name = IMAGE_PATH "binary.png";
  builtin[3].ctype = "image/png";
  builtin[3].data = binary_png;
  builtin[3].length = binary_png_length;

  builtin[4].name = IMAGE_PATH "video.png";
  builtin[4].ctype = "image/png";
  builtin[4].data = video_png;
  builtin[4].length = video_png_length;

  builtin[5].name = IMAGE_PATH "audio.png";
  builtin[5].ctype = "image/png";
  builtin[5].data = audio_png;
  builtin[5].length = audio_png_length;

  builtin[6].name = NULL;
  builtin[6].ctype = NULL;
  builtin[6].data = NULL;
  builtin[6].length = 0;
}

/* fills in file-based stuff in the request structure for built-in images */
void builtin_file_stuff(request *r) {
  image *i;

  /* linear search of files for this one */
  for(i = builtin; i->data; i++) {
    if(strcmp(i->name, r->file) == 0) {
      r->img_data = i->data;
      r->content_length = i->length;
      break;
    }
  }

  if(!i->data) {/* no filename matched */
    r->status = 404;
  }
}
