#ifndef PACOMMON_GUI_IMG_H
#define PACOMMON_GUI_IMG_H

#include <cairo.h>
#include <stdbool.h>

/* Pictures to look at: PNG (cairo), JPEG (libjpeg-turbo) and WebP (libwebp), told apart by what the file starts with and not by its name. */

/* A file name that is a picture to look at: ends in .png, .jpg, .jpeg or .webp (in any case), not hidden. */
bool img_picture_name(const char *name);
/* The file as a cairo surface (ARGB32), or NULL if it is not a picture of those kinds, is not a regular file, is over USR_PIC_MAX_BYTES or has
 * more than IMG_PIXELS_MAX pixels. `want` > 0 says that nothing bigger than that across is needed: a JPEG or a WebP is then decoded smaller
 * (much faster), though never less than `want` on its short side; 0 is whole. */
cairo_surface_t *img_load(const char *path, int want);

#endif
