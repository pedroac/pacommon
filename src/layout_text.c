#include "layout_text.h"
#include <stdio.h>

/* A tool that translates its messages names its i18n.h here (-DPACOMMON_I18N='"i18n.h"'). */
#ifdef PACOMMON_I18N
#include PACOMMON_I18N
#else
#define _(s) (s)
#endif

const char *layout_text_hint(void) {
    return _("Type to search");
}

const char *layout_text_nothing(void) {
    return _("No keyboard matches");
}

const char *layout_text_name(char *out, size_t size, const char *name) {
    snprintf(out, size, _("%s keyboard"), name);
    return out;
}
