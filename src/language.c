#include "language.h"
#include "tunables.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool valid(const char *s) {
    if (!*s || strlen(s) > PAC_LANGUAGE_MAX) return false;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
              strchr("_-.@:", *s)))
            return false;
    return true;
}

bool pac_set_language(const char *language) {
    if (!language[0]) return true;
    if (!valid(language)) return false;

    /* gettext reads LANGUAGE, but only when the message locale is not "C" (glibc counts C.UTF-8
     * as "C"), and a greeter or a daemon often runs with no locale at all. So the message locale
     * is the first of these that is generated: the one asked for, with and without ".UTF-8",
     * then en_US.UTF-8 only to switch LANGUAGE on, which then picks the catalog by itself. */
    setenv("LANGUAGE", language, 1);
    char first[PAC_LANGUAGE_MAX + 1], utf8[PAC_LANGUAGE_MAX + 8];
    snprintf(first, sizeof(first), "%.*s", (int)strcspn(language, ":"), language);
    snprintf(utf8, sizeof(utf8), "%s.UTF-8", first);
    const char *tries[] = { first, utf8, "en_US.UTF-8" };
    for (size_t i = 0; i < sizeof(tries) / sizeof(*tries); i++)
        if (setlocale(LC_MESSAGES, tries[i])) return true;
    return false;
}
