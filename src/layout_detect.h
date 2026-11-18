#ifndef LAYOUT_DETECT_H
#define LAYOUT_DETECT_H

#include "layout.h"

/* ---- autodetecting the system's keyboard layout ----
 * The compositor does not know it: wlroots compositors ignore /etc/default/keyboard and the
 * like, and use "us" unless told. Where the system says what it is, it is read from there. */

/* From an X11 keyboard section: Option "XkbLayout" "pt" and "XkbVariant" (xorg.conf.d). The
 * first of a comma-separated list. */
bool layout_from_xorg(const char *text, Layout *out);
/* From shell-style variables: XKBLAYOUT=pt and XKBVARIANT=intl (/etc/default/keyboard, and
 * /etc/vconsole.conf when it has them). */
bool layout_from_shell_vars(const char *text, Layout *out);
/* From a console keymap name such as "pt-latin9" or "uk", as /etc/vconsole.conf has: only
 * where it names one of the known layouts. */
bool layout_from_console_keymap(const char *keymap, const Layout *known, int known_count, Layout *out);
/* Tries, in this order, the .conf files in etc/X11/xorg.conf.d, etc/default/keyboard and
 * etc/vconsole.conf. known is what to check a console keymap against. */
bool layout_detect(const char *etc, const Layout *known, int known_count, Layout *out);

#endif
