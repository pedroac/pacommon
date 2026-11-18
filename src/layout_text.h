#ifndef LAYOUT_TEXT_H
#define LAYOUT_TEXT_H

#include <stddef.h>

/* The words about choosing a keyboard layout that a tool shows and says aloud, so that they are the same
 * in every tool, on the screen and spoken (everything spoken is also shown), and translated once. They are
 * in a file of their own, apart from layout.c, so that a tool built without the choice of layout can still
 * draw and say them. The tool names its i18n.h as config_file.c does (-DPACOMMON_I18N='"i18n.h"'). */

/* Typing in the picker searches, and changes nothing until it is settled: said where the result will show. */
const char *layout_text_hint(void);

/* The search found no layout. */
const char *layout_text_nothing(void);

/* The name of a layout as a line of its own ("Portuguese keyboard"); writes it to out (of size) and returns out. */
const char *layout_text_name(char *out, size_t size, const char *name);

#endif
