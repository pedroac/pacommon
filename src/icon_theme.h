#ifndef PACOMMON_ICON_THEME_H
#define PACOMMON_ICON_THEME_H

#include <cairo.h>
#include <stdbool.h>

/* Freedesktop icons by name (or by path), looked up in the installed themes: the one the person's
 * GTK settings name first, then the common ones, then hicolor. A PNG is read with Cairo. An SVG is
 * drawn by rsvg-convert (principle 3: the library would stay in memory to draw a few pictures
 * once); at most ICON_SVG_PER_FRAME of them per frame, and the others are ICON_PENDING until the
 * next frame, so that a window opens at once.
 *
 * The data: ICON_CACHE records of a name, a size and a surface, read in order. Misses are kept
 * too, so that the disk is not searched again at every frame. */

typedef enum { ICON_MISSING, ICON_PENDING, ICON_READY } IconState;

/* theme_icons false (icons = 0): every icon is missing. `prog` is the tool's name, for its one message. */
void icons_init(bool theme_icons, const char *prog);
/* For a right-to-left language the themes' right-to-left variants are looked for first: "go-next-rtl" for "go-next"
 * (the freedesktop icon naming; Adwaita has many, as format-indent-more-rtl and go-previous-rtl), and the plain
 * name where there is none. The pictures made so far are forgotten when the direction changes. */
void icons_set_direction(bool rtl);
/* Call before drawing a frame: gives the SVG budget back. */
void icons_new_frame(void);
/* True if a frame left icons ICON_PENDING: draw again. */
bool icons_pending(void);
void icons_free(void);

/* `spec` is a name, a path, or alternatives "folder-download|folder": the first that exists wins.
 * `px` is the size in device pixels. On ICON_READY, *surface is px by px, owned by this module, and
 * *symbolic says it is a mask to be painted in the text's color. */
IconState icon_get(const char *spec, int px, cairo_surface_t **surface, bool *symbolic);

#endif
