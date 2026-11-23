#ifndef PACOMMON_GUI_WIN_H
#define PACOMMON_GUI_WIN_H

#include "field.h"
#include <cairo/cairo.h>
#include <stdbool.h>
#include <stdint.h>

/* The name of the program, set by the program itself: the title and id of its windows, its memory file and its speech. */
extern const char *const app_name;

/* The window: for paconf one wlr-layer-shell surface in the TOP layer (above the other windows, but it does not take
 * the keyboard away from them: the focus is only given when it is clicked, so that a person can copy a
 * password from another window and paste it in). Drawn with Cairo into shared memory. */

/* A pen over the window, as the tablet protocol tells it. */
typedef struct {
    double x, y;           /* in the window, logical pixels */
    double pressure;       /* 0 to 1 */
    double tilt_x, tilt_y; /* degrees */
    bool near;             /* over the window (false: it has left) */
    bool down;             /* touching the tablet */
    bool eraser;           /* the other end of the pen */
} WinTablet;

typedef enum { ANCHOR_TOP_RIGHT, ANCHOR_TOP_LEFT, ANCHOR_BOTTOM_RIGHT, ANCHOR_BOTTOM_LEFT, ANCHOR_CENTER } Anchor;

typedef struct {
    /* keysym is an X11 keysym; text what it types (UTF-8, "" if nothing); held is FIELD_CTRL, FIELD_ALT, ... */
    void (*key)(void *data, uint32_t keysym, const char *text, unsigned held);
    /* The pointer moved (button 0), or a button went down (button 1: the left one; 2: another) or the left one came up (3). x and y in logical pixels. */
    void (*pointer)(void *data, double x, double y, int button);
    /* The wheel: pixels, positive down. */
    void (*scroll)(void *data, double dy);
    /* The size or the scale changed, or the window has just appeared: draw again. */
    void (*redraw)(void *data);
    /* Asked once, before the window is made: how much the design size (win_create's width and height) is multiplied
     * by. The three lengths are of the screen the window will most likely be on: logical pixels per millimetre,
     * 0 where the screen does not say how large it is; the same with 96 dpi taken where it does not say; and the
     * shorter side of the screen in logical pixels (0 if unknown). */
    double (*scale)(void *data, double px_per_mm_known, double px_per_mm, double screen_short);
    /* A pen moved, touched or left (NULL: not wanted). */
    void (*tablet)(void *data, const WinTablet *pen);
} WinHandlers;

typedef struct Win Win;

/* The shape of the pointer over the window: the compositor draws it (cursor-shape-v1; unchanged without it). */
typedef enum { WIN_CURSOR_DEFAULT, WIN_CURSOR_POINTER, WIN_CURSOR_TEXT, WIN_CURSOR_GRAB, WIN_CURSOR_GRABBING } WinCursor;
void win_set_cursor(Win *w, WinCursor cursor);

/* width and height are the design size, which handlers->scale multiplies. A height of 0 is as tall as the screen. */
/* True for a normal window (the compositor draws its frame; it needs no close button or round corners of its own). */
bool win_toplevel(const Win *w);
Win *win_create(int width, int height, Anchor anchor, int margin, bool toplevel, const WinHandlers *handlers, void *data);
void win_destroy(Win *w);

/* The loop: win_prepare before waiting on win_fd (false: the connection is gone), win_read after
 * (readable: the descriptor said so). */
int win_fd(const Win *w);
bool win_prepare(Win *w);
bool win_read(Win *w, bool readable);
/* How long to wait for the next key repeat, in ms, or -1. win_repeat sends the repeats that are due. */
int win_repeat_timeout(const Win *w);
void win_repeat(Win *w);

/* The text that is in the clipboard (what was copied in another window), as one line with the ends trimmed,
 * written to out. False if there is none, it is not text, it does not fit, or its owner does not answer
 * (which is waited for half a second at most). Wipe out when done: it may be a password. */
bool win_paste(Win *w, char *out, size_t size);

/* The evdev code of the key sent last and the xkb modifiers then, and what a key types with them in the layout in use
 * now (for text that is typed again after the layout changed; false if it types nothing). */
void win_last_key(const Win *w, uint32_t *key, uint32_t *mods);
bool win_key_text(Win *w, uint32_t key, uint32_t mods, char *out, size_t size);
/* The name of the first tablet there is ("" if none), and the screens: how many and the name of each ("DP-1"). */
const char *win_tablet_name(const Win *w);
/* How many buttons the first tablet pad has (the keys on the side of the tablet), 0 if none. */
int win_pad_buttons(const Win *w);
int win_output_count(const Win *w);
const char *win_output_name(const Win *w, int index);
/* What the keymap the compositor sent names the layout in use ("Portuguese"), "" if it is not known yet. */
const char *win_layout_name(const Win *w);

bool win_running(const Win *w);
void win_close(Win *w);
bool win_ready(const Win *w);      /* the compositor has given the window its size */
int win_width(const Win *w);       /* logical pixels */
int win_height(const Win *w);

/* A context for drawing the whole window, in logical pixels (the scale is applied), or NULL. */
cairo_t *win_begin(Win *w);
void win_end(Win *w, cairo_t *cr);

#endif
