#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <stdbool.h>

/* Makes the texts of the tool come in language (e.g. "pt", "pt_BR", "pt_BR:pt") whatever the
 * environment says; an empty one leaves the environment's choice. Returns false, changing
 * nothing, if the name is not usable: it is names of letters, digits and "_-.@" separated by
 * ':', up to 63 bytes. Sets LANGUAGE and LC_MESSAGES, so call it before the first translated
 * text, and not from two threads. A tool built with I18N=0 does not call it. */
bool pac_set_language(const char *language);

#endif
