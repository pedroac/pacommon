#ifndef PACOMMON_GUI_KIT_H
#define PACOMMON_GUI_KIT_H

#include "length.h"
#include "palette.h"
#include "speech.h"
#include "tunables.h"
#include "gui_win.h"
#include <cairo/cairo.h>
#include <stdbool.h>

/* What the windows of paconf have in common: the colors, the text, the buttons, the places that can be
 * clicked or reached with the keyboard, and speech. A window keeps its own state and its own meaning for the
 * kinds of place (an int, 0 for none). */

typedef struct {
    bool high_contrast;
    bool speech;            /* speak what the screen shows */
    double text_scale;
    bool has_text_size_min, has_text_size_max;
    PacLength text_size_min, text_size_max;   /* hold the smallest text between them */
    const char *font;       /* a file or NULL */
    const char *language;
} UiOptions;

typedef struct {
    PacColor text, dim, bg, card, hover, rule, accent, ok, warn, error;
} Colors;

typedef struct {
    double x, y, w, h;
    int kind;
    int index;
    bool focusable;
} Hit;

#define MAX_HITS 96

/* A view that scrolls: how far, how tall it is and how tall what it shows is, and its scroll bar (where it was drawn, and if it is being dragged).
 * The kit has one for the window; a window with a part that scrolls by itself has another, and uses the kit_scroll_ functions on it. */
typedef struct {
    double scroll, content_h, view_h;
    bool sb_on, sb_drag;    /* a scroll bar is drawn, and is being dragged */
    double sb_x, sb_top, sb_ty, sb_th, sb_grab;   /* as drawn last; sb_x: where the part that takes the pointer starts; sb_grab: where on the thumb it was taken */
} KitScroll;

typedef struct {
    Win *win;
    cairo_font_face_t *regular, *bold;
    double ts;              /* everything drawn is this many times its design size */
    double text_scale;
    UiOptions opt;
    bool high;
    Colors c;

    int focus_kind;
    int focus_index;
    bool kbd;               /* the focus ring is shown: the last input was a key */
    int hover_kind;         /* what the pointer is over (kept by kind, since the hits are made again at every draw) */
    int hover_index;
    KitScroll sc;           /* the window's */
    Hit hits[MAX_HITS];
    int nhits;
    bool dirty;
    char *msg;              /* the message the window shows, and its level (0 is none): a good one goes after a few seconds */
    int *msg_level;
    long long ok_due;       /* when it goes, 0 when there is none to go */
    bool speaking;          /* speech is on and connected */
    double content_pad;     /* where the content of the window starts at the left, as kit_pad last said (the frame's title lines up with it) */
} Kit;

bool kit_init(Kit *k, const UiOptions *o);
/* Where the content of the window starts at the left (and ends at the same distance from the right): the window's width is not all used, a column of at
 * most UI_MAX_W in the middle is, so that a label is never far from its value. */
double kit_pad(Kit *k);
/* The same, without telling the frame (for measuring). */
double kit_pad_for(const Kit *k);
/* The same for a window that lays its cards out in two columns when it is wide enough (UI_TWO_COLUMNS_MIN): *columns is 1 or 2, and the content is then up
 * to UI_MAX_W_WIDE wide. */
double kit_pad_wide(Kit *k, int *columns);
void kit_destroy(Kit *k);

/* The smallest text is UI_TEXT_MM millimetres (times text_scale) where the screen says how large it is, held
 * between text_size_min and text_size_max; everything else follows in proportion. The window, design_h tall,
 * never grows past the screen. Sets k->ts and returns it (see WinHandlers.scale). */
double kit_scale_for(Kit *k, double design_h, double px_per_mm_known, double px_per_mm, double screen_short);

void kit_speak(const Kit *k, SpeechPriority priority, const char *text);
/* Tell the kit which message is shown now (ok: a good one, that is taken away after OK_MESSAGE_MS). */
void kit_message_shown(Kit *k, char *msg, int *level, bool ok);
int kit_speech_timeout(const Kit *k);
void kit_speech_tick(Kit *k);

void kit_color(cairo_t *cr, PacColor c, double alpha);
void kit_round_rect(cairo_t *cr, double x, double y, double w, double h, double r);
void kit_set_font(Kit *k, cairo_t *cr, bool bold, double px);
/* Text with its middle at cy, fitted to max_w; returns its width. */
double kit_label(Kit *k, cairo_t *cr, double x, double cy, double max_w, const char *s, bool bold, double px, PacColor c);
double kit_label_width(Kit *k, cairo_t *cr, const char *s, bool bold, double px);
/* A place that can be clicked (and reached with the keyboard if focusable), cut to the visible part clip0..clip1. */
void kit_add_hit(Kit *k, double x, double y, double w, double h, int kind, int index, bool focusable, double clip0, double clip1);
bool kit_is_focus(const Kit *k, int kind, int index);
bool kit_is_hover(const Kit *k, int kind, int index);
int kit_focus_position(const Kit *k);
void kit_move_focus(Kit *k, int step);
/* The same for any view that scrolls (`right` is the right edge that its bar is drawn against: the window's width for the window's own). */
void kit_scroll_clamp(KitScroll *s);
void kit_scroll_end(Kit *k, KitScroll *s, cairo_t *cr, double right, double top, double content_h);
bool kit_scroll_pointer(Kit *k, KitScroll *s, double x, double y, int button);

/* Keeps the scroll within the last known content, before it is used to draw (else a frame is drawn out of range and then fixed). */
void kit_clamp_scroll(Kit *k);
/* The thin scroll bar at the right edge of a view of height k->view_h starting at top (nothing if all fits); remembers where it is. */
void kit_draw_scrollbar(Kit *k, cairo_t *cr, double W, double top);
/* The end of drawing a scrolling view that starts at top: takes content_h (the height of all of it), brings the scroll back into
 * range (drawn again if it had to move) and draws the scroll bar. */
void kit_end_scroll(Kit *k, cairo_t *cr, double W, double top, double content_h);
/* Lets the pointer drag it (or click on its track); true if the event was for the scroll bar and is used up (the caller redraws on k->dirty). */
bool kit_scrollbar_pointer(Kit *k, double x, double y, int button);
int kit_hit_at(const Kit *k, double x, double y);
void kit_focus_ring(Kit *k, cairo_t *cr, double x, double y, double w, double h, double r);
void kit_button(Kit *k, cairo_t *cr, double x, double y, double w, double h, const char *text, bool primary,
                bool enabled, int kind, int index, bool focusable, double clip0, double clip1);
/* The window's body and border, its title and the button that closes it (a hit of kind close_kind). Returns where
 * the content starts. */
/* A thin line across the content (a separator between rows that are far apart): x and width as the content, at y. */
void kit_rule(Kit *k, cairo_t *cr, double x, double y, double w);
double kit_frame(Kit *k, cairo_t *cr, const char *title, int close_kind);
/* A PNG made at most side across (square, from the middle) so that drawing it is cheap; NULL if it is not one that can be read. */
cairo_surface_t *kit_load_square(const char *path, int side);
/* A picture in a circle: the surface (as made by kit_load_square), or a person's shape where there is none. */
void kit_round_picture(Kit *k, cairo_t *cr, cairo_surface_t *img, double cx, double cy, double r);
void kit_toggle_switch(Kit *k, cairo_t *cr, double x, double cy, bool on);
/* A gauge: its track, the fill up to fraction (0 to 1), and a tick where something begins (tick 0: none). */
void kit_gauge(Kit *k, cairo_t *cr, double x, double y, double w, double h, double fraction, double tick, PacColor fill);
/* A line of how a number has been: the n readings v (the oldest first) of a window of `capacity` of them, the newest at the right edge of the
 * rectangle, with a dot at its end. Its range is never less than min_span or min_rel of the highest (so that a quiet number does not look wild);
 * zero_based starts it at 0 (a speed or a share), else the range is centered on the readings. */
void kit_sparkline(Kit *k, cairo_t *cr, double x, double y, double w, double h, const double *v, int n, int capacity,
                   double min_span, double min_rel, bool zero_based, PacColor c);

#endif
