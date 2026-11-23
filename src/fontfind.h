#ifndef FONTFIND_H
#define FONTFIND_H

#include <stdbool.h>
#include <stddef.h>
#include "tunables.h"

/* The directories fonts are installed in: $XDG_DATA_HOME/fonts, ~/.fonts and the fonts
 * directory of every entry of $XDG_DATA_DIRS. This covers distributions that keep them
 * elsewhere (Fedora, NixOS) without listing each location. */
typedef struct {
    char storage[FONT_MAX_DIRS][FONT_DIR_MAX];
    const char *list[FONT_MAX_DIRS + 1];   /* NULL-terminated */
} FontDirs;

void font_dirs_init(FontDirs *dirs);

/* Searches dirs (recursively) for a file named like one of names (NULL-terminated). The
 * earliest name wins, wherever it is found. */
bool font_find(const char *const *dirs, const char *const *names, char *path, size_t size);

/* The Latin fonts every tool prefers, the best for reading first, as the `names` of font_find and pac_font_load: Atkinson
 * Hyperlegible (made for readability: its letters cannot be mistaken for one another), then Noto Sans, DejaVu Sans and
 * Liberation Sans. Package: ttf-atkinson-hyperlegible (Arch). The light one is for a clock or another big, thin figure
 * (Atkinson has no light weight: its regular stands in). A tool does not keep a list of its own (docs/style.md). */
extern const char *const PAC_FONTS_REGULAR[];
extern const char *const PAC_FONTS_BOLD[];
extern const char *const PAC_FONTS_LIGHT[];

#endif
