#ifndef LAYOUT_H
#define LAYOUT_H

#include "field.h"
#include "tunables.h"
#include <stdbool.h>

/* A keyboard layout, as xkb names it: "pt", or "us" with the variant "intl". */
typedef struct {
    char layout[24];
    char variant[24];
    char name[96];       /* "Portuguese"; the code when the description is unknown */
    bool unavailable;    /* xkb could not make it: stepping goes past it */
} Layout;

/* Reads the layouts out of the "! layout" section of an xkeyboard-config rules list
 * (evdev.lst). Returns how many there are. */
int layouts_parse_list(const char *text, Layout *out, int max);

/* Reads "us(intl)", "us (intl)" or "pt": a layout, optionally with a variant. */
bool layout_parse_spec(const char *spec, Layout *out);

/* The spec of a layout, the inverse of layout_parse_spec. */
void layout_spec(const Layout *layout, char *out, int size);

/* The layouts to offer: the comma-separated specs in configured, or else every layout
 * xkeyboard-config knows. Returns how many. */
int layouts_load(Layout *out, int max, const char *configured);

/* The layouts to choose from, and the search typed to find one. Number 0 is always the
 * compositor's own. Typing never changes the layout in use, or the next key would be read
 * with it: a search only proposes one (pick), which is applied when the user settles. */
typedef struct {
    Layout items[MAX_LAYOUTS + 1];
    int count, current;
    Field filter;
    int pick;               /* the layout the search found; 0: none */
} LayoutPicker;

/* Whether layout i is among those the search lets through (all, without one). */
bool picker_matches(const LayoutPicker *p, int i);
int picker_match_count(const LayoutPicker *p);
/* The next layout matching the search from from, in a direction; -1 if there is none. */
int picker_neighbour(const LayoutPicker *p, int from, int dir);
/* Left or right: moves the proposal while searching, else steps to the next layout. Returns
 * the layout to switch to, or -1. */
int picker_step(LayoutPicker *p, int dir);
/* Call after the search text changed. */
void picker_filter_changed(LayoutPicker *p);
/* Ends the search. Returns the layout to switch to, or -1. */
int picker_settle(LayoutPicker *p);
/* Ends the search, changing nothing. */
void picker_cancel(LayoutPicker *p);

/* The name of a keyboard layout ("Portuguese") in the language, from xkeyboard-config's own
 * catalog, which the system installs; the name itself where it has no translation, or when built
 * with NO_I18N. */
const char *pac_layout_name(const char *name);

#endif
