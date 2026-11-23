#include "fcfont.h"

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

bool font_installed(const char *family) {
    if (!family[0] || !FcInit()) return false;
    FcPattern *want = FcPatternCreate();
    if (!want) return false;
    FcPatternAddString(want, FC_FAMILY, (const FcChar8 *)family);
    FcConfigSubstitute(NULL, want, FcMatchPattern);
    FcDefaultSubstitute(want);
    FcResult r;
    FcPattern *got = FcFontMatch(NULL, want, &r);
    bool found = false;
    FcChar8 *name;
    for (int i = 0; got && !found && FcPatternGetString(got, FC_FAMILY, i, &name) == FcResultMatch; i++)
        found = strcasecmp((const char *)name, family) == 0;
    if (got) FcPatternDestroy(got);
    FcPatternDestroy(want);
    return found;
}

bool font_file(const char *family, bool bold, char *path, size_t size, int *index) {
    if (!family[0] || !FcInit()) return false;
    FcPattern *want = FcPatternCreate();
    if (!want) return false;
    FcPatternAddString(want, FC_FAMILY, (const FcChar8 *)family);
    FcPatternAddInteger(want, FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
    FcConfigSubstitute(NULL, want, FcMatchPattern);
    FcDefaultSubstitute(want);
    FcResult r;
    FcPattern *got = FcFontMatch(NULL, want, &r);
    FcChar8 *file;
    int number = 0;
    bool ok = got && FcPatternGetString(got, FC_FILE, 0, &file) == FcResultMatch &&
              strlen((const char *)file) < size;
    if (ok) {
        snprintf(path, size, "%s", (const char *)file);
        if (FcPatternGetInteger(got, FC_INDEX, 0, &number) != FcResultMatch) number = 0;
        *index = number;               /* FreeType's own: the upper bits pick the named instance of a variable font (the bold) */
    }
    if (got) FcPatternDestroy(got);
    FcPatternDestroy(want);
    return ok;
}

static const char *const text_nerd[] = { "NotoSans Nerd Font", "Ubuntu Nerd Font", "Arimo Nerd Font", NULL };
static const char *const symbol_nerd[] = {
    "Symbols Nerd Font", "Symbols Nerd Font Mono", "NotoSans Nerd Font", "Ubuntu Nerd Font", "JetBrainsMono Nerd Font", NULL,
};

static bool first_installed(const char *const *list, char *out, size_t size) {
    for (; *list; list++)
        if (font_installed(*list)) {
            snprintf(out, size, "%s", *list);
            return true;
        }
    return false;
}

void fonts_pick_text(const char *wanted, char *out, size_t size) {
    if (strcmp(wanted, "auto") != 0) {
        snprintf(out, size, "%s", wanted);          /* the person's choice, as it is: fontconfig finds a stand-in */
        return;
    }
    if (!first_installed(text_nerd, out, size)) snprintf(out, size, "Sans");
}

void fonts_pick_symbols(const char *text, char *out, size_t size) {
    if (strstr(text, "Nerd Font") && font_installed(text)) snprintf(out, size, "%s", text);
    else if (!first_installed(symbol_nerd, out, size)) out[0] = '\0';
}
