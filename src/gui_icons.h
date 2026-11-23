#ifndef PACOMMON_GUI_ICONS_H
#define PACOMMON_GUI_ICONS_H

#include "palette.h"
#include <cairo/cairo.h>
#include <stdbool.h>

/* The pictures of the windows of paconf: size is the width of the box, r half of it, around (cx, cy). */
void icon_speaker(cairo_t *cr, double cx, double cy, double size, bool off, PacColor c);
void icon_mic(cairo_t *cr, double cx, double cy, double size, bool off, PacColor c);
void icon_bluetooth(cairo_t *cr, double cx, double cy, double r, double width, PacColor c);
void icon_keyboard(cairo_t *cr, double cx, double cy, double r, PacColor c);
void icon_mouse(cairo_t *cr, double cx, double cy, double h, PacColor c);
void icon_pen(cairo_t *cr, double cx, double cy, double size, PacColor c);
void icon_printer(cairo_t *cr, double cx, double cy, double size, PacColor c);
void icon_display(cairo_t *cr, double cx, double cy, double size, PacColor c);
void icon_person(cairo_t *cr, double cx, double cy, double size, PacColor c);
void icon_wifi(cairo_t *cr, double cx, double cy, double r, PacColor c);

#endif
