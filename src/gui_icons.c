#define _GNU_SOURCE
#include "gui_icons.h"
#include <math.h>

/* Drawn with strokes and fills of one color, so that they are the same in every contrast. */

static void icon_color(cairo_t *cr, PacColor c, double alpha) { cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha); }

static void icon_round_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void icon_speaker(cairo_t *cr, double cx, double cy, double size, bool off, PacColor c) {
    double s = size / 24;
    icon_color(cr, c, 1);
    cairo_move_to(cr, cx - 10 * s, cy - 4 * s);
    cairo_line_to(cr, cx - 5 * s, cy - 4 * s);
    cairo_line_to(cr, cx + 1 * s, cy - 10 * s);
    cairo_line_to(cr, cx + 1 * s, cy + 10 * s);
    cairo_line_to(cr, cx - 5 * s, cy + 4 * s);
    cairo_line_to(cr, cx - 10 * s, cy + 4 * s);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (off) {
        cairo_move_to(cr, cx + 5 * s, cy - 5 * s); cairo_line_to(cr, cx + 13 * s, cy + 5 * s);
        cairo_move_to(cr, cx + 13 * s, cy - 5 * s); cairo_line_to(cr, cx + 5 * s, cy + 5 * s);
        cairo_stroke(cr);
    } else {
        for (int i = 1; i <= 2; i++) {
            cairo_arc(cr, cx + 1 * s, cy, 5 * s * i, -M_PI * 0.28, M_PI * 0.28);
            cairo_stroke(cr);
        }
    }
}

void icon_mic(cairo_t *cr, double cx, double cy, double size, bool off, PacColor c) {
    double s = size / 24;
    icon_color(cr, c, 1);
    icon_round_rect(cr, cx - 4 * s, cy - 11 * s, 8 * s, 15 * s, 4 * s);
    cairo_fill(cr);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, cx, cy - 1 * s, 8 * s, 0.1 * M_PI, 0.9 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, cx, cy + 7 * s); cairo_line_to(cr, cx, cy + 11 * s);
    cairo_move_to(cr, cx - 4 * s, cy + 11 * s); cairo_line_to(cr, cx + 4 * s, cy + 11 * s);
    if (off) { cairo_move_to(cr, cx - 11 * s, cy - 11 * s); cairo_line_to(cr, cx + 11 * s, cy + 11 * s); }
    cairo_stroke(cr);
}

void icon_bluetooth(cairo_t *cr, double cx, double cy, double r, double width, PacColor c) {
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx - r * 0.5, cy - r * 0.5);
    cairo_line_to(cr, cx + r * 0.5, cy + r * 0.5);
    cairo_line_to(cr, cx, cy + r);
    cairo_line_to(cr, cx, cy - r);
    cairo_line_to(cr, cx + r * 0.5, cy - r * 0.5);
    cairo_line_to(cr, cx - r * 0.5, cy + r * 0.5);
    cairo_stroke(cr);
}

void icon_keyboard(cairo_t *cr, double cx, double cy, double r, PacColor c) {
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, r * 0.16);
    icon_round_rect(cr, cx - r, cy - r * 0.62, 2 * r, r * 1.24, r * 0.2);
    cairo_stroke(cr);
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 4; col++) {
            double x = cx - r * 0.66 + col * r * 0.44, y = cy - r * 0.28 + row * r * 0.4;
            cairo_arc(cr, x, y, r * 0.09, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    cairo_rectangle(cr, cx - r * 0.48, cy + r * 0.2, r * 0.96, r * 0.13);
    cairo_fill(cr);
}

void icon_wifi(cairo_t *cr, double cx, double cy, double r, PacColor c) {
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, r * 0.16);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 1; i <= 3; i++) {
        cairo_arc(cr, cx, cy + r * 0.55, r * 0.36 * i, -M_PI * 0.75, -M_PI * 0.25);
        cairo_stroke(cr);
    }
    cairo_arc(cr, cx, cy + r * 0.55, r * 0.1, 0, 2 * M_PI);
    cairo_fill(cr);
}

void icon_mouse(cairo_t *cr, double cx, double cy, double h, PacColor c) {
    double w = h * 0.62, x = cx - w / 2, y = cy - h / 2;
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, h * 0.075);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    icon_round_rect(cr, x, y, w, h, w / 2.2);
    cairo_stroke(cr);
    cairo_move_to(cr, x, y + h * 0.42); cairo_line_to(cr, x + w, y + h * 0.42);
    cairo_move_to(cr, cx, y); cairo_line_to(cr, cx, y + h * 0.42);
    cairo_stroke(cr);
}

void icon_pen(cairo_t *cr, double cx, double cy, double size, PacColor c) {
    double s = size / 24;
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, 2.4 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx - 9 * s, cy + 9 * s);   /* the tip */
    cairo_line_to(cr, cx - 7 * s, cy + 3 * s);
    cairo_line_to(cr, cx + 5 * s, cy - 9 * s);
    cairo_line_to(cr, cx + 9 * s, cy - 5 * s);
    cairo_line_to(cr, cx - 3 * s, cy + 7 * s);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void icon_printer(cairo_t *cr, double cx, double cy, double size, PacColor c) {
    double s = size / 24;
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_rectangle(cr, cx - 6 * s, cy - 10 * s, 12 * s, 6 * s);
    cairo_stroke(cr);
    cairo_rectangle(cr, cx - 10 * s, cy - 4 * s, 20 * s, 10 * s);
    cairo_stroke(cr);
    cairo_rectangle(cr, cx - 6 * s, cy + 2 * s, 12 * s, 8 * s);
    cairo_stroke(cr);
}

void icon_display(cairo_t *cr, double cx, double cy, double size, PacColor c) {
    double s = size / 24;
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    icon_round_rect(cr, cx - 11 * s, cy - 8 * s, 22 * s, 14 * s, 2 * s);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - 5 * s, cy + 10 * s);
    cairo_line_to(cr, cx + 5 * s, cy + 10 * s);
    cairo_stroke(cr);
}

void icon_person(cairo_t *cr, double cx, double cy, double size, PacColor c) {
    double s = size / 24;
    icon_color(cr, c, 1);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_arc(cr, cx, cy - 5 * s, 5 * s, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy + 12 * s, 9 * s, M_PI * 1.15, M_PI * 1.85);
    cairo_stroke(cr);
}
